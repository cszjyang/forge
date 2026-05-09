#include "local_cache.h"

#include <immintrin.h>  // Add this include for _mm_pause()

LocalCache::LocalCache(const DMCConfig* conf) {
  assert(NUM_FIFO_NODE > 1 && (NUM_FIFO_NODE & (NUM_FIFO_NODE - 1)) == 0);
  assert(MAX_GROUP_NUM > 1 && (MAX_GROUP_NUM & (MAX_GROUP_NUM - 1)) == 0);

  if ((uint64_t)local_gnode_map % 8 != 0) {
    printd(L_ERROR, "local_gnode_map is not aligned to 8 bytes");
    abort();
  }

  memset(local_gnode_map, 0, MAX_GROUP_NUM * sizeof(LocalGroupNode));
  free_size_.store(conf->lc_size_per_client * conf->num_local_clients / NUM_MN, std::memory_order_relaxed);

  local_fifo[SFIFO].init(SFIFO, this, conf);
#if NUM_FIFO_TYPE > 1
  local_fifo[MFIFO].init(MFIFO, this, conf);
#endif

  printd(L_DEBUG, "Client %d init shared_lc, cache size: %d", conf->client_id, free_size_.load(std::memory_order_relaxed));
}

void LocalCache::remove_from_cache(uint32_t gid) {
  remove_from_cache(&local_gnode_map[get_gnode_id(gid)]);
}

void LocalCache::remove_from_cache(LocalGroupNode* gnode) {
  gnode->age = FNODE_DEAD;
  if (gnode->num_active > 0) {
    gnode->num_active = 0;
    free_size_.fetch_add(sizeof(ByteMap), std::memory_order_relaxed);
    memset(gnode->freq_map, 0, sizeof(ByteMap));
  }
}

bool LocalCache::need_evict_() { return !EN_LOCAL_OBJECT_CACHE && free_size_.load(std::memory_order_relaxed) < 0; }
bool LocalCache::is_cached_gnode(uint32_t gnode_id) { return local_gnode_map[gnode_id].gid != INVALID_GID; }
bool LocalCache::is_cached_gid(uint32_t gid) { return local_gnode_map[get_gnode_id(gid)].gid == gid; }

uint8_t LocalCache::get_gid_age(uint32_t gid) {
  LocalGroupNode* node = &local_gnode_map[get_gnode_id(gid)];

  if (node->gid != gid) {
    return gid > node->gid ? FNODE_ALIVE : FNODE_DEAD;
  }

  return node->age;
}

int LocalCache::get_obj_freq(uint32_t gid, uint16_t sn) {
  LocalGroupNode* node = &local_gnode_map[get_gnode_id(gid)];
  if (node->gid == gid) {
    return node->age == FNODE_ALIVE ? node->freq_map[sn] : 2;
  } else if (node->gid < gid) {
    return 16;
  } else {
    return -1;
  }
}

void LocalCache::update_metrics(uint32_t gid, uint16_t sn) {
  static constexpr uint8_t MAX_FREQ_PER_NODE = 255 / NUM_CN;
  LocalGroupNode* node = &local_gnode_map[get_gnode_id(gid)];

  if ((node->gid == gid && node->age == FNODE_ALIVE) ||
      (node->gid < gid && node->age == FNODE_DEAD)) {
    if (node->freq_map[sn] < MAX_FREQ_PER_NODE && node->freq_map[sn]++ == 0 &&
        __atomic_fetch_add(&node->num_active, 1, __ATOMIC_RELAXED) == 0) {
      free_size_.fetch_sub(sizeof(ByteMap), std::memory_order_relaxed);
    }
  }
}

bool LocalCache::local_get(const Slot* slot, KVOpsCtx* ctx) {
  if (!EN_LOCAL_OBJECT_CACHE || !slot->is_consistent())
    return false;

  AtomicEntry ext = slot->extra;
  LocalCacheBucket* bucket = &local_cache_table_[ctx->key_hash % LC_NUM_BUCKETS];

  for (uint8_t i = 0; i < HASH_BUCKET_ASSOC_NUM; i++) {
    AtomicEntry first_read{bucket->slots[i].load(std::memory_order_acquire)};

    if (first_read.ext_match_mask_efp(ext)) {
      // Spin until lc_kv_size is non-zero (data is ready)
      while (first_read.get_lc_kv_size() == 0) {
        // Add a small pause to reduce CPU usage during spinning
        _mm_pause();
        first_read.data = bucket->slots[i].load(std::memory_order_acquire);
      }

      memcpy((void*)ctx->read_buf_laddr, bucket->blocks[i], sizeof(ObjHeader) + ctx->key_size);
      std::atomic_thread_fence(std::memory_order_release);  // memory barrier

      // Validate the slot is not changed
      AtomicEntry second_read{bucket->slots[i].load(std::memory_order_relaxed)};
      if (first_read.data != second_read.data) {
        return false;  // Slot changed, return cache miss
      }

      printd(L_DEBUG, "local get hit, key: %s", (char*)ctx->read_buf_laddr + sizeof(ObjHeader));
      return true;
    }
  }

  return false;
}

void LocalCache::local_set(const KVOpsCtx* ctx) {
  if (!EN_LOCAL_OBJECT_CACHE || ctx->op_type == SET)
    return;

  // Extract entry and find target bucket
  AtomicEntry ext = ((Slot*)(ctx->target_slot_laddr))->extra;
  LocalCacheBucket* bucket = &local_cache_table_[ctx->key_hash % LC_NUM_BUCKETS];

  // Check for cache hit
  for (uint8_t i = 0; i < HASH_BUCKET_ASSOC_NUM; i++) {
    AtomicEntry slot{bucket->slots[i].load(std::memory_order_relaxed)};
    if (slot.ext_match_mask_efp(ext)) return;
  }

  uint16_t kv_size = sizeof(ObjHeader) + ctx->key_size + ctx->val_size;
  int64_t size_delta = 0;

  // Make space if needed by evicting entries
  bool enough_space = true;
  if (free_size_.load(std::memory_order_relaxed) < kv_size) {
    size_delta = evict_bucket(ctx->key_hash, 2, kv_size * 2);
    if (size_delta < kv_size) {
      enough_space = false;
    }
  }

  // Try to insert into an empty slot
  ext.set_lc_kv_size(0);  // lc_kv_size set to 0 to indicate that the slot is being locked and updated
  while (enough_space) {
    bool inserted = false;

    // Look for empty slot to insert into
    for (uint8_t i = 0; !inserted && i < HASH_BUCKET_ASSOC_NUM; i++) {
      uint64_t first_read = bucket->slots[i].load(std::memory_order_relaxed);
      if (first_read == 0 && bucket->slots[i].compare_exchange_strong(first_read, ext.data,
                                                                      std::memory_order_relaxed,
                                                                      std::memory_order_relaxed)) {
        memcpy(bucket->blocks[i], (void*)ctx->read_buf_laddr, sizeof(ObjHeader) + ctx->key_size);
        std::atomic_thread_fence(std::memory_order_release);  // memory barrier

        ext.set_lc_kv_size(kv_size);  // Now set lc_kv_size to signal that the data is ready
        bucket->slots[i].store(ext.data, std::memory_order_relaxed);
        size_delta -= kv_size;
        inserted = true;
      }
    }
    if (inserted) {
      printd(L_DEBUG, "local set hit, key: %s", (char*)ctx->read_buf_laddr + sizeof(ObjHeader));
      break;
    }

    size_delta += evict_bucket(ctx->key_hash, 1, 0);  // No empty slots found, evict one entry and try again
  }

  // Update free space tracking
  if (size_delta != 0) {
    free_size_.fetch_add(size_delta, std::memory_order_relaxed);
  }
}

uint32_t LocalCache::evict_bucket(uint64_t key_hash, uint32_t sample_bucket_num, uint32_t bytes) {
  static constexpr uint8_t MAX_SAMPLE_BUCKET_NUM = 2;
  assert(sample_bucket_num <= MAX_SAMPLE_BUCKET_NUM);

  LocalEvictSlot prio_slot_id[HASH_BUCKET_ASSOC_NUM * MAX_SAMPLE_BUCKET_NUM];
  uint32_t prio_slot_id_size = 0;
  for (uint8_t i = 0; i < sample_bucket_num; i++) {
    LocalCacheBucket* bucket = &local_cache_table_[(key_hash + i) % LC_NUM_BUCKETS];
    sample_bucket(bucket, i * HASH_BUCKET_ASSOC_NUM, prio_slot_id, &prio_slot_id_size);
  }

  std::sort(prio_slot_id, prio_slot_id + prio_slot_id_size,
            [](const LocalEvictSlot& a, const LocalEvictSlot& b) {
              return a.freq < b.freq;
            });

  uint32_t evict_bytes = 0;
  for (uint32_t i = 0; i < prio_slot_id_size; i++) {
    uint8_t slot_id = prio_slot_id[i].slot_id;
    auto* bucket = &local_cache_table_[(key_hash + slot_id / HASH_BUCKET_ASSOC_NUM) % LC_NUM_BUCKETS];
    auto* slot = &bucket->slots[slot_id % HASH_BUCKET_ASSOC_NUM];

    // Only count if we actually evicted something
    if (slot->compare_exchange_strong(prio_slot_id[i].old_data, 0, std::memory_order_relaxed)) {
      printd(L_DEBUG, "local evict, key: %s", (char*)bucket->blocks[slot_id % HASH_BUCKET_ASSOC_NUM] + sizeof(ObjHeader));
      AtomicEntry old{prio_slot_id[i].old_data};
      evict_bytes += old.get_lc_kv_size();
      if (evict_bytes >= bytes) break;
    }
  }

  return evict_bytes;
}

void LocalCache::sample_bucket(LocalCacheBucket* bucket, uint8_t base_slot_id,
                               __OUT LocalEvictSlot* prio_slot_id,
                               __OUT uint32_t* prio_slot_id_size) {
  for (uint8_t i = 0; i < HASH_BUCKET_ASSOC_NUM; i++) {
    AtomicEntry slot{bucket->slots[i].load(std::memory_order_relaxed)};
    if (slot.get_lc_kv_size() == 0)
      continue;

    uint32_t gid = slot.get_ext_gid();
    uint16_t sn = slot.get_ext_sn();
    LocalGroupNode* node = &local_gnode_map[get_gnode_id(gid)];
    uint8_t freq = 0;
    if ((node->gid == gid && node->age == FNODE_ALIVE) ||
        (node->gid < gid && node->age == FNODE_DEAD)) {
      freq = node->freq_map[sn] + 1;
    }

    prio_slot_id[(*prio_slot_id_size)++] = {freq, i + base_slot_id, slot.data};
  }
}

/************************ LocalFifo ************************/
void LocalFifo::init(uint8_t fifo_type, LocalCache* lc, const DMCConfig* conf) {
  p_lc = lc;
  lc_fifo_head = 0;
  lc_fifo_tail = 0;
  clk_fifo_head = 0;
  clk_fifo_tail = 0;
  clk_fifo_len = 0;
  clk_head_alarm = 0;
  min_alarm_len = conf->num_group_samples * conf->num_global_clients / conf->num_local_clients / NUM_MN;

  uint64_t server_fifo_addr = conf->server_base_addr + HASH_SPACE_BYTES + GNODE_SPACE_BYTES;
  if (fifo_type == SFIFO) {
    fifo_raddr = server_fifo_addr;
    fwindow_raddr = 0;  // RNIC device memory
    clock_raddr = FIFO_WINDOW_BYTES * NUM_FIFO_TYPE;
  } else {
    fifo_raddr = server_fifo_addr + FIFO_SPACE_BYTES;
    fwindow_raddr = FIFO_WINDOW_BYTES;
    clock_raddr = FIFO_WINDOW_BYTES * NUM_FIFO_TYPE + 8;
  }
  memset(lc_fifo_buffer, 0, NUM_FIFO_NODE * sizeof(uint32_t));
}

void LocalFifo::update_clock(const uint64_t clock) {
  clk_fifo_head = clock;
  clk_fifo_tail = clock >> 32;
  clk_fifo_len = clk_fifo_tail - clk_fifo_head;
  uint32_t alarm_len = clk_fifo_len / 16;
  clk_head_alarm = alarm_len > min_alarm_len ? clk_fifo_head + alarm_len
                                             : clk_fifo_head + min_alarm_len;

  // if (lc_fifo_tail < clk_fifo_head) {
  //   lc_fifo_head = clk_fifo_head;
  //   lc_fifo_tail = clk_fifo_head;
  //   printd(L_WARN, "lc fifo too far behind, clk_fifo_head: %d, clk_fifo_tail: %d", clk_fifo_head, clk_fifo_tail);
  // }

  printd(L_DEBUG, "update clock, head: %d, tail: %d, len: %d", clk_fifo_head, clk_fifo_tail, clk_fifo_len);
  printd(L_DEBUG, "local fifo head: %d, tail: %d, len: %d", lc_fifo_head, lc_fifo_tail, lc_fifo_tail - lc_fifo_head);
}

void LocalFifo::push_fifo_tail(const AtomicEntry* fnode) {
  uint32_t gid = fnode->get_fnode_gid();
  uint32_t gnode_id = get_gnode_id(gid);
  LocalGroupNode* node = &p_lc->local_gnode_map[gnode_id];

  node->gid = gid;
  node->age = FNODE_ALIVE;
  node->segment = fnode->get_fnode_segment();

  uint32_t idx = fid_wrap_index(lc_fifo_tail++);  // need to ensure that log_sync is exactly at lc_fifo_tail
  lc_fifo_buffer[idx] = gid;

  printd(L_DEBUG, "sync tail, gid: %d, fid:%d, segment: %d",
         gid, lc_fifo_tail - 1, node->segment);
}

void LocalFifo::pop_fifo_head(bool flush, __OUT AssistCtx* ctx) {
  uint32_t idx = fid_wrap_index(lc_fifo_head);
  uint32_t gid = lc_fifo_buffer[idx];
  uint32_t gnode_id = get_gnode_id(gid);
  LocalGroupNode* node = &p_lc->local_gnode_map[gnode_id];

  if (node->segment > 0) {
    node->segment--;
  } else if (flush) {
    ctx->gnodes_to_flush[ctx->num_flush++] = {lc_fifo_head, node};
    printd(L_DEBUG, "sync head, fid:%d, gid: %d", lc_fifo_head, gid);
  } else {
    p_lc->remove_from_cache(node->gid);
    printd(L_DEBUG, "dead gnode, gid: %d", gid);
  }
  lc_fifo_head++;
}

void LocalFifo::sync_fifo_tail(const AtomicEntry* fnode, const uint32_t num_sync_tail) {
  printd(L_DEBUG, "sync tail num: %d, head: %d, tail: %d", num_sync_tail, lc_fifo_head, lc_fifo_tail);
  for (uint32_t i = 0; i < num_sync_tail; i++) {
    if (fnode->get_fnode_epoch() != get_fnode_epoch(lc_fifo_tail)) {
      printd(L_DEBUG, "epoch mismatch, fnode_epoch: %d != %d, lc_fifo_tail: %d",
             fnode->get_fnode_epoch(), get_fnode_epoch(lc_fifo_tail), lc_fifo_tail);
      break;
    }

    push_fifo_tail(fnode++);
  }
}

void LocalFifo::sync_fifo_head(__OUT AssistCtx* ctx) {
  // flush gnodes that are dying
  while (lc_fifo_head <= clk_fifo_head + NUM_FWINDOW_NODE / 2 && lc_fifo_head < lc_fifo_tail &&
         ctx->num_flush < AssistCtx::MAX_ASSIST_GNODE_NUM) {
    uint8_t age = get_fid_age(lc_fifo_head);
    if (age == FNODE_ALIVE && !p_lc->need_evict_()) {
      break;
    }
    printd(L_DEBUG, "sync head, fid: %d, age: %d", lc_fifo_head, age);

    pop_fifo_head(SAMPLE_WAIT_DIE || (age != FNODE_DEAD), ctx);
  }
}

uint8_t LocalFifo::get_fid_age(const uint32_t fid) {
  if (fid < clk_fifo_head) {
    return FNODE_DEAD;
  } else if (fid < clk_head_alarm) {
    return FNODE_DYING;
  } else {
    return FNODE_ALIVE;
  }
}

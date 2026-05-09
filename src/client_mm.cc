#include "client_mm.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "debug.h"

ClientSharedMM::ClientSharedMM(const DMCConfig* conf) {
  segment_size_ = conf->segment_size;

  head_tail_.store(0);
  memset(galloc_pool_, 0, sizeof(uint64_t) * GROUP_POOL_SIZE);
  memset(group_pool_, 0, sizeof(Group) * GROUP_POOL_SIZE);
  twim_allocator_ = new Allocator<TWIM>(GROUP_POOL_SIZE * 4);
  loff_map_allocator_ = new Allocator<LoffMap>(GROUP_POOL_SIZE * 2);

  if ((uint64_t)twim_allocator_->get_buf_addr() % 8 != 0 ||
      (uint64_t)loff_map_allocator_->get_buf_addr() % 8 != 0) {
    printd(L_ERROR, "Allocator is not aligned to 8 bytes");
    abort();
  }

  init_free_gid_queue(conf);
#if PREFILL_MM
  init_free_group_queue(conf);
  fill_and_seal(NULL);
#endif

  printd(L_DEBUG, "Client %d init shared_mm", conf->client_id);
}

ClientSharedMM::~ClientSharedMM() {}

void ClientSharedMM::init_free_gid_queue(const DMCConfig* conf) {
  uint32_t st = conf->client_id - 1;
  uint32_t ed = st + NUM_THREAD_PER_SMM;
  uint32_t max_ed = conf->st_client_id - 1 + conf->num_local_clients;
  if (ed > max_ed)
    ed = max_ed;

  uint32_t st_gnode_id = st * MAX_GROUP_NUM / conf->num_global_clients;
  uint32_t ed_gnode_id = ed * MAX_GROUP_NUM / conf->num_global_clients;

  for (uint32_t gnode_id = st_gnode_id; gnode_id < ed_gnode_id; gnode_id++) {
    if (gnode_id == INVALID_GID)
      continue;
    free_gid_queue.push(gnode_id);
  }

  printd(L_INFO, "Init %u free gnodes", free_gid_queue.size());
}

void ClientSharedMM::init_free_group_queue(const DMCConfig* conf) {
  uint32_t st = conf->client_id - 1;
  uint32_t ed = st + NUM_THREAD_PER_SMM;
  uint32_t max_ed = conf->st_client_id - 1 + conf->num_local_clients;
  if (ed > max_ed)
    ed = max_ed;

  assert(conf->segment_size % GROUP_BYTES == 0);
  uint64_t st_segment_raddr = HASH_SPACE_BYTES + GNODE_SPACE_BYTES + FIFO_SPACE_BYTES * NUM_FIFO_TYPE +
                              conf->server_base_addr;
  uint32_t num_segments = ROUNDUP(conf->server_data_len / NUM_MN, conf->segment_size) / conf->segment_size;
  uint32_t st_segment_id = st * num_segments / conf->num_global_clients;
  uint32_t ed_segment_id = ed * num_segments / conf->num_global_clients;

  for (uint32_t segment_id = st_segment_id; segment_id < ed_segment_id; segment_id++) {
    uint64_t raddr = st_segment_raddr + segment_id * conf->segment_size;
    add_segment(raddr, 0);
  }

  printd(L_INFO, "Prefill %u free groups", free_group_queue.size());
}

void ClientSharedMM::add_segment(uint64_t raddr, uint16_t server) {
  uint32_t num_groups_per_segment = segment_size_ / GROUP_BYTES;

  for (uint32_t i = 0; i < num_groups_per_segment; i++) {
    Group new_group;
    memset(&new_group, 0, sizeof(Group));

    new_group.addr = raddr + i * GROUP_BYTES;
    new_group.server = server;

    free_group_queue.push(new_group);
  }
}

void ClientSharedMM::add_group(uint32_t tail, AssistCtx* assist_ctx) {
  uint32_t idx = wrap_index(tail);
  Group* group = &group_pool_[idx];

  if (group->twim != NULL) {
    twim_allocator_->free((TWIM*)group->twim);
  }
  if (group->evt_twim != NULL) {
    twim_allocator_->free((TWIM*)group->evt_twim);
    loff_map_allocator_->free((LoffMap*)group->evt_loff_map);
  }

  AtomicEntry block;
  if (assist_ctx == NULL) {
    free_group_queue.front().gid = free_gid_queue.front();
    free_group_queue.front().twim = (AtomicEntry*)twim_allocator_->alloc(true);
    *group = free_group_queue.front();
    block.init_galloc(0, 0, group->gid);

    free_group_queue.pop();
    free_gid_queue.pop();
  } else {
    *group = assist_ctx->merge_group;
    block.init_galloc(SIZE_TO_LEN(assist_ctx->merge_kv_bytes), assist_ctx->merge_kv_num, group->gid);
  }

  galloc_pool_[idx].store(block.data, std::memory_order_relaxed);
  printd(L_DEBUG_EG, "Fill group, gid: %u, addr: %lx, server: %u, segment: %d, sn: %d, offset: %d, rest: %d->%d",
         group->gid, group->addr, group->server, group->segment, block.get_galloc_sn(), LEN_TO_SIZE(block.get_galloc_loff()),
         free_group_queue.size() + 1, free_group_queue.size());
}

void ClientSharedMM::free_group(SampleGroupNode* gnode) {
  Group new_group{
      .addr = gnode->atomic.get_gnode_pointer(),
      .gid = 0,
      .old_size = gnode->atomic.get_gnode_size(),
      .segment = 0,
      .server = 0,
      .twim = NULL,
      .evt_twim = gnode->twim,
      .evt_loff_map = gnode->loff_map,
  };

  free_group_queue.push(new_group);
  free_gid_queue.push(get_next_gid(gnode->gid));

  printd(L_DEBUG, "Free group, gid: %u, addr: %lx, server: %u, evt_twim: %d, rest: %d->%d",
         gnode->gid, gnode->atomic.get_gnode_pointer(), 0, new_group.evt_twim != NULL,
         free_group_queue.size() - 1, free_group_queue.size());
}

int ClientSharedMM::alloc(uint32_t size, __OUT RemoteBlock* rblock) {
  size = ROUNDUP(size, BLOCK_SIZE);
  uint8_t len = SIZE_TO_LEN(size);
  AtomicEntry block;
  block.init_galloc(len, 1, 0);

  uint64_t head_tail = head_tail_.load(std::memory_order_acquire);
  uint32_t head = head_tail;
  uint32_t tail = head_tail >> 32;

  for (uint32_t i = head; i != tail; i++) {
    uint32_t idx = wrap_index(i);
    std::atomic<uint64_t>* alloc = &galloc_pool_[idx];

    AtomicEntry try_alloc{alloc->load(std::memory_order_relaxed) + block.data};
    if (try_alloc.is_galloc_full()) {
      continue;
    }

    AtomicEntry old_alloc{alloc->fetch_add(block.data, std::memory_order_relaxed)};
    AtomicEntry new_alloc{old_alloc.data + block.data};
    if (new_alloc.is_galloc_full()) {
      // withdraw using fetch_sub can cause ABA problem when EVA_REAL_SIZE
      // using compare_exchange_strong can avoid ABA problem by ensuring withdraw order
#if EVA_REAL_SIZE
      uint64_t expected = new_alloc.data;
      while (!alloc->compare_exchange_strong(expected, old_alloc.data,
                                             std::memory_order_relaxed,
                                             std::memory_order_relaxed)) {
        expected = new_alloc.data;
      }
#else
      alloc->fetch_sub(block.data, std::memory_order_relaxed);
#endif
      printd(L_DEBUG, "Group %u is full, sn %d/%d, offset: %d/%d",
             old_alloc.get_galloc_gid(), old_alloc.get_galloc_sn(), MAX_GROUP_SIZE,
             LEN_TO_SIZE(old_alloc.get_galloc_loff()), GROUP_BYTES);
      continue;
    }

    Group* group = &group_pool_[idx];
    uint16_t sn = old_alloc.get_galloc_sn();
    group->twim[sn].set_idx_kv_len(len);

    uint16_t st_loff = old_alloc.get_galloc_loff();
    uint32_t gid = old_alloc.get_galloc_gid();

    rblock->addr = group->addr + LEN_TO_SIZE(st_loff);
    rblock->size = size;
    rblock->gid = gid;
    rblock->sn = sn;
    rblock->group = group;

    if (group->old_size != 0) {
      printd(L_DEBUG, "Group %d has old size %u", gid, group->old_size);
#ifdef EVA_REAL_SIZE
      // binary search to find the first twim_entry that has a st_loff >= my st_loff
      uint16_t left = 0;
      uint16_t right = group->old_size;
      while (left < right) {
        uint16_t mid = (left + right) / 2;
        if (group->evt_loff_map[mid] < st_loff) {
          left = mid + 1;
        } else {
          right = mid;
        }
      }

      // linear search to find the last twim_entry that has a st_loff < my ed_loff
      for (; right < group->old_size && group->evt_loff_map[right] < st_loff + len; right++);

      rblock->st_evt_sn = left;
      rblock->num_evt_sn = right - left;
#else
      rblock->st_evt_sn = sn;
      rblock->num_evt_sn = 1;
#endif
    }

    return i != head && size < GROUP_BYTES * SEAL_WATERMARK ? ALLOC_SHARED_SEAL : ALLOC_SHARED;
  }

  printd(L_DEBUG, "No available group. Head: %u, Tail: %u", head, tail);
  return ALLOC_FAIL;
}

void ClientSharedMM::fill_and_seal(AssistCtx* assist_ctx) {
  uint64_t head_tail = head_tail_.load(std::memory_order_relaxed);
  uint32_t head = head_tail;
  uint32_t tail = head_tail >> 32;

  // seal full groups
  if (assist_ctx != NULL) {
    if (assist_ctx->need_evict) {
      add_group(tail++, assist_ctx);  // fill the merged group in the last round
    }

    // seal groups that are less than SEAL_WATERMARK full
    assist_ctx->old_gpool_head = head;
    while (head != tail) {
      uint32_t idx = wrap_index(head);
      AtomicEntry alloc{galloc_pool_[idx].load(std::memory_order_relaxed)};

      uint16_t sn = alloc.get_galloc_sn();
      uint16_t loff = alloc.get_galloc_loff();
      if (sn < MAX_GROUP_SIZE && sn * (SIZE_TO_LEN(GROUP_BYTES) - loff) >= loff) {
        break;
      }

      head++;
      printd(L_DEBUG_EG, "Going to seal group, gid: %d, sn: %d, offset: %d",
             group_pool_[idx].gid, alloc.get_galloc_sn(), LEN_TO_SIZE(alloc.get_galloc_loff()));
    }

    assist_ctx->num_seal = head - assist_ctx->old_gpool_head;
  }

  // fill empty group slots
  while (!is_pool_full(head, tail) && !free_group_queue.empty() && !free_gid_queue.empty()) {
    add_group(tail++);
  }

  if (assist_ctx != NULL) {
    assist_ctx->need_evict = is_pool_low(head, tail);
  }

  head_tail_.store((uint64_t)tail << 32 | head, std::memory_order_release);
  // note that other threads may get the group idx before the head_tail is updated
  // they can alloc memory after the group is sealed
  // however, these actions are sured to be completed quickly before the completion of  RDMA_FAA clock for seal
  // thus, when flush gnodes for seal, all allocations are completed, the seal is safe
}

// **************************************** ClientExclusiveMM ****************************************
ClientExclusiveMM::ClientExclusiveMM(LocalCache* lc) {
  free_blocks_by_size_ = std::set<RemoteBlock, CompareSizeIncr>();
  lc_ = lc;
}

ClientExclusiveMM::~ClientExclusiveMM() {
  return;
}

// Best-Fit Memory Allocation
int ClientExclusiveMM::alloc(uint32_t size, __OUT RemoteBlock* r_block) {
  // Early exit if no free blocks
  if (free_blocks_by_size_.empty()) {
    return ALLOC_FAIL;
  }

  // Round up size to block boundary
  size = ROUNDUP(size, BLOCK_SIZE);

  // Find best fit block
  RemoteBlock balloc;
  balloc.size = size;
  auto best_fit_block = free_blocks_by_size_.lower_bound(balloc);

  // Search for a suitable block that's alive and not too large
  const float MAX_SIZE_RATIO = 2.0f;
  while (best_fit_block != free_blocks_by_size_.end() &&
         best_fit_block->size < size * MAX_SIZE_RATIO) {
    uint8_t age = lc_->get_gid_age(best_fit_block->gid);

    if (age == FNODE_ALIVE) {
      // Found suitable block - allocate it
      *r_block = *best_fit_block;
      free_blocks_by_size_.erase(best_fit_block);
      return ALLOC_EXCLUSIVE;
    } else {
      // Block is dead - remove it and continue search
      best_fit_block = free_blocks_by_size_.erase(best_fit_block);
    }
  }

  // No suitable block found
  printd(L_INFO, "No enough memory. Alloc: %u. Max: %u. Free: %lu.",
         balloc.size, get_max_block_size(), get_free_size());
  return ALLOC_FAIL;
}

int ClientExclusiveMM::free(const RemoteBlock* r_block) {
  assert(r_block->num_evt_sn == 0);
  free_blocks_by_size_.insert(*r_block);
  printd(L_INFO, "Free rb:0x%lx, size: %u", r_block->addr, r_block->size);
  return 0;
}

uint32_t ClientExclusiveMM::get_max_block_size() {
  if (free_blocks_by_size_.size() == 0) {
    return 0;
  }
  return free_blocks_by_size_.rbegin()->size;
}

uint32_t ClientExclusiveMM::get_min_block_size() {
  if (free_blocks_by_size_.size() == 0) {
    return 0;
  }
  return free_blocks_by_size_.begin()->size;
}

uint64_t ClientExclusiveMM::get_free_size() {
  uint64_t sum = 0;
  for (auto it = free_blocks_by_size_.begin(); it != free_blocks_by_size_.end(); it++) {
    sum += it->size;
  }
  return sum;
}

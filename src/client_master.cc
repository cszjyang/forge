#include <assert.h>
#include <stddef.h>
#include <sys/time.h>
#include <unistd.h>

#include <algorithm>
#include <iostream>
#include <vector>

#include "client.h"
#include "debug.h"
#include "dmc_table.h"
#include "ib.h"

void DMCClient::switch_fifo_type(uint8_t fifo_type) {
#if NUM_FIFO_TYPE > 1
  local_fifo_ = &shared_lc_->local_fifo[fifo_type];
  printd(L_DEBUG, "switch fifo type %d", fifo_type);
#endif
}

uint8_t DMCClient::decide_fifo_type() {
#if NUM_FIFO_TYPE > 1
  int64_t sfifo_len = shared_lc_->local_fifo[SFIFO].get_fifo_rlen();
  int64_t mfifo_len = shared_lc_->local_fifo[MFIFO].get_fifo_rlen();
  printd(L_DEBUG, "sfifo_len: %d, mfifo_len: %d", sfifo_len, mfifo_len);

  return sfifo_len >= 0.2 * (sfifo_len + mfifo_len) ? SFIFO : MFIFO;
#endif
  return SFIFO;
}

void DMCClient::poll_and_fetch_flush_fifo(KVOpsCtx* ctx, uint64_t clk) {
  local_fifo_->update_clock(clk);
  fetch_fifo_tail(ctx);
  flush_fifo_head();
  nm_->rdma_poll_send_sid_async();  // poll the index
  add_fifo_tail(ctx);
}

void DMCClient::fetch_fifo_tail(KVOpsCtx* ctx) {
  static constexpr uint32_t BUFFER_SIZE = BLOCK_SIZE * MAX_KV_LEN / sizeof(AtomicEntry);
  assist_ctx_->num_sync_tail = std::min(local_fifo_->clk_fifo_tail - local_fifo_->lc_fifo_tail, BUFFER_SIZE);
  printd(L_DEBUG, "fetch fifo tail, num_sync_tail: %d", assist_ctx_->num_sync_tail);

  if (assist_ctx_->num_sync_tail == 0)
    return;

  db_helper_.init(10000);
  uint32_t num_rear_fnode = get_num_rear_fnode(local_fifo_->lc_fifo_tail);
  if (assist_ctx_->num_sync_tail <= num_rear_fnode) {
    db_helper_.push_read(get_fnode_raddr(local_fifo_->lc_fifo_tail), server_rkeys_[mn_id_],
                         ctx->write_buf_laddr, local_buf_lkey_,
                         sizeof(AtomicEntry) * assist_ctx_->num_sync_tail,
                         IBV_SEND_SIGNALED);
  } else {
    db_helper_.push_read(get_fnode_raddr(local_fifo_->lc_fifo_tail), server_rkeys_[mn_id_],
                         ctx->write_buf_laddr, local_buf_lkey_,
                         sizeof(AtomicEntry) * num_rear_fnode,
                         IBV_SEND_SIGNALED);
    db_helper_.push_read(get_fnode_raddr(0), server_rkeys_[mn_id_],
                         ctx->write_buf_laddr + sizeof(AtomicEntry) * num_rear_fnode, local_buf_lkey_,
                         sizeof(AtomicEntry) * (assist_ctx_->num_sync_tail - num_rear_fnode),
                         0);
  }
  nm_->rdma_post_send_sid_async(db_helper_.head_wr, mn_id_);
}

void DMCClient::add_fifo_tail(KVOpsCtx* ctx) {
  if (assist_ctx_->num_sync_tail == 0)
    return;

  nm_->rdma_poll_send_sid_async();  // poll the fifo
  local_fifo_->sync_fifo_tail((AtomicEntry*)ctx->write_buf_laddr, assist_ctx_->num_sync_tail);
}

void DMCClient::flush_fifo_head() {
  local_fifo_->sync_fifo_head(assist_ctx_);
  if (assist_ctx_->num_flush == 0)
    return;

  printd(L_DEBUG, "flush fifo head, head: %d, tail: %d, num_flush: %d",
         local_fifo_->clk_fifo_head, local_fifo_->lc_fifo_tail, assist_ctx_->num_flush);

  uint32_t num_flush = assist_ctx_->num_flush;
  assist_ctx_->num_flush = 0;

  db_helper_.init(11000);

  for (uint32_t i = 0; i < num_flush; i++) {
    AtomicEntry fnode;
    uint64_t fnode_raddr = get_fnode_raddr(assist_ctx_->gnodes_to_flush[i].first);
    LocalGroupNode* gnode = assist_ctx_->gnodes_to_flush[i].second;
    printd(L_DEBUG, "flush gnode, gid: %d", gnode->gid);

    // FAA freq_map
    if (gnode->num_active >= HOT_THRESH) {
      // Early Exemption: skip gnode whose num_active is very high
      fnode.init_fnode(0, MAX_GROUP_SIZE, 1, 0, 0);
    } else {
      fnode.init_fnode(0, gnode->num_active, 1, 0, 0);

      uint64_t freq_map_raddr = EN_RNIC_DM ? get_fwnode_raddr(assist_ctx_->gnodes_to_flush[i].first)
                                           : get_gnode_raddr(gnode->gid) + GNODE_FREQ_MAP_OFF;
      uint32_t freq_map_rkey = EN_RNIC_DM ? server_dm_rkeys_[mn_id_] : server_rkeys_[mn_id_];

      uint64_t* freq_8B = (uint64_t*)gnode->freq_map;
      for (uint16_t j = 0; j < (MAX_GROUP_SIZE / 8); j++, freq_8B++) {
        if (*freq_8B == 0) continue;

        db_helper_.push_faa(freq_map_raddr + j * 8, freq_map_rkey,
                            (uint64_t)local_buf_, local_buf_lkey_, *freq_8B, 0);
      }
    }

    db_helper_.push_faa(fnode_raddr, server_rkeys_[mn_id_],
                        (uint64_t)local_buf_, local_buf_lkey_,
                        fnode.data, 0);
    printd(L_DEBUG, "flush fnode, gid: %d, fid: %d, num_active: %d",
           gnode->gid, assist_ctx_->gnodes_to_flush[i].first, gnode->num_active);
  }

  nm_->rdma_post_send_sid_async(db_helper_.head_wr, mn_id_);

  // remove from cache
  for (uint32_t i = 0; i < num_flush; i++) {
    shared_lc_->remove_from_cache(assist_ctx_->gnodes_to_flush[i].second);
  }
}

void DMCClient::seal_reinsert(KVOpsCtx* ctx, uint8_t fifo_type) {
  // one of num_seal and num_reinsert is sure to be 0
  uint64_t clk_add = ((uint64_t)(assist_ctx_->num_seal + assist_ctx_->num_reinsert)) << 32;
  if (clk_add == 0) {
    return;
  }

  // RTT 1
  // 1.1: FAA FIFO clock
  // clk buffer: ctx->op_laddr
  switch_fifo_type(fifo_type);
  db_helper_.push_faa(local_fifo_->clock_raddr, server_dm_rkeys_[mn_id_],
                      ctx->op_laddr, local_buf_lkey_,
                      clk_add, IBV_SEND_SIGNALED);
#if NUM_FIFO_TYPE > 1
  uint8_t another_type = fifo_type == SFIFO ? MFIFO : SFIFO;
  db_helper_.push_read(shared_lc_->local_fifo[another_type].clock_raddr, server_dm_rkeys_[mn_id_],
                       ctx->op_laddr + sizeof(uint64_t), local_buf_lkey_,
                       sizeof(uint64_t), 0);
#endif

  nm_->rdma_post_send_sid_sync(db_helper_.head_wr, mn_id_);
  db_helper_.init();

  // get FIFO clock
  uint64_t clk = *(uint64_t*)ctx->op_laddr;
  uint32_t clk_tail_fid = clk >> 32;

  // RTT 2
  // 2.1: seal full group, and reinsert group from last iteration
  // write buffer: ctx->op_laddr + sizeof(AtomicEntry) * sample_window_
  uint64_t seal_buffer = ctx->op_laddr + sizeof(AtomicEntry) * sample_window_;
  prepare_seal_reinsert(seal_buffer, clk_tail_fid);
  nm_->rdma_post_send_sid_async(db_helper_.head_wr, mn_id_);

  if (lc_master_) {
    local_fifo_->update_clock(clk + clk_add);
    flush_fifo_head();

#if NUM_FIFO_TYPE > 1
    switch_fifo_type(another_type);
    local_fifo_->update_clock(*(uint64_t*)(ctx->op_laddr + sizeof(uint64_t)));
    flush_fifo_head();
#endif
  }

  db_helper_.init();
}

void DMCClient::sample(KVOpsCtx* ctx) {
  uint32_t num_active = 0;
  assist_ctx_->num_sample = 0;
  assist_ctx_->num_reinsert = 0;
  assist_ctx_->num_freq_map = 0;

  while (assist_ctx_->num_sample < min_sample_groups_ && num_active < HOT_THRESH) {
    // RTT 1
    // 1.1: FAA FIFO clock
    // clk buffer: ctx->op_laddr
    switch_fifo_type(decide_fifo_type());
    db_helper_.push_faa(local_fifo_->clock_raddr, server_dm_rkeys_[mn_id_],
                        ctx->op_laddr, local_buf_lkey_,
                        sample_window_, IBV_SEND_SIGNALED);

    nm_->rdma_post_send_sid_sync(db_helper_.head_wr, mn_id_);
    db_helper_.init();

    // get FIFO clock
    uint64_t clk = *(uint64_t*)ctx->op_laddr;
    uint32_t clk_head_fid = clk;

    // 2.2: read FIFO queue head for eviction
    // read buffer: ctx->op_laddr
    uint64_t fnode_buf_laddr = ctx->op_laddr;
    prepare_sample_head(fnode_buf_laddr, clk_head_fid);
    if (lc_master_) {
      nm_->rdma_post_send_sid_async(db_helper_.head_wr, mn_id_);
      poll_and_fetch_flush_fifo(ctx, clk + sample_window_);
    } else {
      nm_->rdma_post_send_sid_sync(db_helper_.head_wr, mn_id_);
    }
    db_helper_.init();

    // RTT 3
    // 3.1: filter out reinserted group
    // reinsert buffer: ctx->op_laddr + sizeof(AtomicEntry) * sample_window_
    // 3.2: read the evicted group
    // there have been some sampled groups in the buffer
    // read buffer: ctx->read_buf_laddr + sizeof(SampleGroupNode) * assist_ctx_->num_sample
    uint64_t gnode_buf_laddr = ctx->read_buf_laddr + assist_ctx_->num_sample * sizeof(SampleGroupNode);
    uint64_t reinsert_buffer = ctx->op_laddr + sizeof(AtomicEntry) * sample_window_;
    num_active += prepare_sample_group(clk_head_fid, fnode_buf_laddr, gnode_buf_laddr, reinsert_buffer);
    seal_reinsert(ctx, MFIFO);
  }

  if (db_helper_.num_wr > 0) {
    db_helper_.sr[0].send_flags |= IBV_SEND_SIGNALED;
  } else {
    db_helper_.push_read(local_fifo_->clock_raddr, server_dm_rkeys_[mn_id_],
                         ctx->op_laddr, local_buf_lkey_,
                         0, IBV_SEND_SIGNALED);
  }
  nm_->rdma_post_send_sid_async(db_helper_.head_wr, mn_id_);
  db_helper_.init();
}

void DMCClient::prepare_seal_reinsert(uint64_t seal_reinsert_buffer, uint32_t clk_tail_fid) {
  uint32_t num_group = assist_ctx_->num_seal + assist_ctx_->num_reinsert;
  if (num_group == 0)
    return;

  uint32_t fid = clk_tail_fid;
  AtomicEntry* fnode = (AtomicEntry*)seal_reinsert_buffer;
  InitGroupNode* gnode = (InitGroupNode*)(fnode + num_group);

  // handle reinserted group
  for (uint32_t i = 0; i < assist_ctx_->num_reinsert; i++, fnode++) {
    printd(L_DEBUG, "Reinsert group, gid: %d, segment: %d, active_num: %d",
           fnode->get_fnode_gid(), fnode->get_fnode_segment(), fnode->get_fnode_active_num());

    uint8_t segment = fnode->get_fnode_segment();
    if (segment > 0) {
      // virtual segmentation
      fnode->set_fnode_segment(segment - 1);
    } else {
      // second chance
      fnode->set_fnode_flush_num(0);
      fnode->set_fnode_active_num(0);
    }

    fnode->set_fnode_epoch(get_fnode_epoch(fid++));
  }

  // handle full group
  for (uint32_t i = 0; i < assist_ctx_->num_seal; i++, fnode++, gnode++) {
    uint32_t idx = shared_mm_->wrap_index(assist_ctx_->old_gpool_head + i);
    Group* group = &shared_mm_->group_pool_[idx];

    gnode->fid = fid;
    gnode->gid = group->gid;

    AtomicEntry alloc;
    do {
      alloc.data = shared_mm_->galloc_pool_[idx].load(std::memory_order_relaxed);
    } while (alloc.is_galloc_full());
    gnode->atomic.init_gnode(alloc.get_galloc_sn(), group->addr, GroupNode::GNODE_ALIVE, group->segment);

    uint64_t gnode_raddr = get_gnode_raddr(group->gid);

    db_helper_.push_write(gnode_raddr, server_rkeys_[mn_id_],
                          (uint64_t)gnode, local_buf_lkey_,
                          sizeof(InitGroupNode), 0);
    db_helper_.push_write(gnode_raddr + GNODE_TWIM_OFF, server_rkeys_[mn_id_],
                          (uint64_t)group->twim, twim_buf_lkeys_[mn_id_],
                          sizeof(AtomicEntry) * alloc.get_galloc_sn(), 0);

#if _DEBUG
    printd(L_DEBUG, "Seal group, gid: %d, size: %d, gnode raddr: %lx, segment: %d",
           gnode->gid, gnode->atomic.get_gnode_size(), group->addr, group->segment);

    if (gnode_raddr < server_gmeta_addr_ || gnode_raddr >= server_fifo_addr_) {
      printd(L_ERROR, "Invalid gnode raddr: %lx, gid: %d", gnode_raddr, group->gid);
      abort();
    }

    if (gnode->atomic.get_gnode_size() > MAX_GROUP_SIZE) {
      printd(L_ERROR, "Invalid group size: %d, gid: %d, gnode raddr: %lx, group raddr: %lx",
             gnode->atomic.get_gnode_size(), gnode->gid, gnode_raddr, gnode->atomic.get_gnode_pointer());
    }

    uint32_t bytes = 0;
    for (uint32_t j = 0; j < gnode->atomic.get_gnode_size(); j++) {
      // if (group->twim[j].get_idx_kv_len() == 0) {
      //   printd(L_ERROR, "Invalid kv len: %d, gid: %d, sn: %d, group size: %d",
      //          group->twim[j].get_idx_kv_len(), group->gid, j, gnode->atomic.get_gnode_size());
      // }
      bytes += LEN_TO_SIZE(group->twim[j].get_idx_kv_len());
    }
    if (bytes > GROUP_BYTES) {
      printd(L_ERROR, "Invalid group bytes: %d > %d", bytes, GROUP_BYTES);
    }

#endif

    fnode->init_fnode(group->gid, 0, 0, group->segment, get_fnode_epoch(fid++));
    group->gid = INVALID_GID;  // prevent future twim_entry-setting
  }

  uint32_t num_rear_fnode = get_num_rear_fnode(clk_tail_fid);
  if (num_group <= num_rear_fnode) {
    db_helper_.push_write(get_fnode_raddr(clk_tail_fid), server_rkeys_[mn_id_],
                          seal_reinsert_buffer, local_buf_lkey_,
                          sizeof(AtomicEntry) * num_group, 0);
  } else {
    db_helper_.push_write(get_fnode_raddr(clk_tail_fid), server_rkeys_[mn_id_],
                          seal_reinsert_buffer, local_buf_lkey_,
                          sizeof(AtomicEntry) * num_rear_fnode, 0);
    db_helper_.push_write(get_fnode_raddr(0), server_rkeys_[mn_id_],
                          seal_reinsert_buffer + sizeof(AtomicEntry) * num_rear_fnode, local_buf_lkey_,
                          sizeof(AtomicEntry) * (num_group - num_rear_fnode), 0);
  }

  assist_ctx_->num_reinsert = 0;
  assist_ctx_->num_seal = 0;
}

void DMCClient::prepare_sample_head(uint64_t fnode_buf_laddr, uint32_t clk_head_fid) {
  printd(L_DEBUG, "prepare sample head, clk_head_fid: %d", clk_head_fid);
  uint32_t num_rear_fnode = get_num_rear_fnode(clk_head_fid);
  if (sample_window_ <= num_rear_fnode) {
    db_helper_.push_read(get_fnode_raddr(clk_head_fid), server_rkeys_[mn_id_],
                         fnode_buf_laddr, local_buf_lkey_,
                         sizeof(AtomicEntry) * sample_window_,
                         IBV_SEND_SIGNALED);
  } else {
    db_helper_.push_read(get_fnode_raddr(clk_head_fid), server_rkeys_[mn_id_],
                         fnode_buf_laddr, local_buf_lkey_,
                         sizeof(AtomicEntry) * num_rear_fnode,
                         IBV_SEND_SIGNALED);
    db_helper_.push_read(get_fnode_raddr(0), server_rkeys_[mn_id_],
                         fnode_buf_laddr + sizeof(AtomicEntry) * num_rear_fnode, local_buf_lkey_,
                         sizeof(AtomicEntry) * (sample_window_ - num_rear_fnode),
                         0);
  }
}

uint32_t DMCClient::prepare_sample_group(uint32_t clk_head_fid, uint64_t fnode_buf_laddr,
                                         uint64_t gnode_buf_laddr, uint64_t reinsert_buf_laddr) {
  SampleGroupNode* gnode = (SampleGroupNode*)gnode_buf_laddr;
  AtomicEntry* sample_fnode = (AtomicEntry*)fnode_buf_laddr;
  AtomicEntry* reinsert_fnode = (AtomicEntry*)reinsert_buf_laddr;
  uint32_t num_active = 0;
  for (uint32_t i = 0; i < sample_window_; i++, sample_fnode++) {
    // 3.1
    uint32_t fid = clk_head_fid + i;
    uint32_t retry = 10;
    while (sample_fnode->get_fnode_epoch() != get_fnode_epoch(fid) ||
           (sample_fnode->get_fnode_segment() == 0 &&
            sample_fnode->get_fnode_flush_num() < NUM_CN)) {
      printd(L_DEBUG, "retry, gid: %d, fid: %d, epoch: %d != %d, segment: %d, flush_num: %d < %d",
             sample_fnode->get_fnode_gid(), fid, sample_fnode->get_fnode_epoch(), get_fnode_epoch(fid),
             sample_fnode->get_fnode_segment(), sample_fnode->get_fnode_flush_num(), NUM_CN);

      prepare_sample_head(fnode_buf_laddr, clk_head_fid);
      nm_->rdma_post_send_sid_async(db_helper_.head_wr, mn_id_);
      flush_fifo_head();
      db_helper_.init();
      nm_->rdma_poll_send_sid_async();  // poll the FIFO queue head

      if (!SAMPLE_WAIT_DIE && --retry == 0) {
        printd(L_DEBUG, "reach retry limit");
        if (sample_fnode->get_fnode_epoch() != get_fnode_epoch(fid) ||
            sample_fnode->get_fnode_flush_num() == 0) {
          printd(L_ERROR, "Invalid FIFO entry, gid: %d, fid: %d, epoch: %d != %d, flush_num: %d < %d",
                 sample_fnode->get_fnode_gid(), fid, sample_fnode->get_fnode_epoch(), get_fnode_epoch(fid),
                 sample_fnode->get_fnode_flush_num(), NUM_CN);
          abort();
        }
        break;
      }
    }

    // 3.2: read the evicted group
    uint64_t gnode_raddr = get_gnode_raddr(sample_fnode->get_fnode_gid());
    uint64_t freq_map_raddr = EN_RNIC_DM ? get_fwnode_raddr(fid) : gnode_raddr + GNODE_FREQ_MAP_OFF;
    uint32_t freq_map_rkey = EN_RNIC_DM ? server_dm_rkeys_[mn_id_] : server_rkeys_[mn_id_];

    // add freq_map
    if (sample_fnode->get_fnode_segment() == 0) {
      printd(L_DEBUG, "add freq map, raddr: %lx", freq_map_raddr);
      assist_ctx_->freq_map_raddr[assist_ctx_->num_freq_map++] = freq_map_raddr;
    }
    // reinsert
    if (en_reinsert && (sample_fnode->get_fnode_segment() > 0 ||
                        sample_fnode->get_fnode_active_num() > HOT_THRESH * sample_fnode->get_fnode_flush_num())) {
      printd(L_DEBUG, "reinsert group, gid: %d, segment: %d, active_num: %d, freq_map_raddr: %lx",
             sample_fnode->get_fnode_gid(), sample_fnode->get_fnode_segment(), sample_fnode->get_fnode_active_num(), freq_map_raddr);
      reinsert_fnode[assist_ctx_->num_reinsert++] = *sample_fnode;
      continue;
    }

    printd(L_DEBUG, "sample group, gid: %d, segment: %d, active_num: %d, freq_map_raddr: %lx",
           sample_fnode->get_fnode_gid(), sample_fnode->get_fnode_segment(), sample_fnode->get_fnode_active_num(), freq_map_raddr);

    num_active += sample_fnode->get_fnode_active_num() / sample_fnode->get_fnode_flush_num();
    gnode->twim = (AtomicEntry*)shared_mm_->twim_allocator_->alloc(false);
    gnode->loff_map = (uint16_t*)shared_mm_->loff_map_allocator_->alloc(false);

    db_helper_.push_read(freq_map_raddr, freq_map_rkey,
                         (uint64_t)gnode->freq_map, local_buf_lkey_,
                         GNODE_TWIM_OFF - GNODE_FREQ_MAP_OFF, 0);
    db_helper_.push_read(gnode_raddr, server_rkeys_[mn_id_],
                         (uint64_t)gnode, local_buf_lkey_,
                         GNODE_FREQ_MAP_OFF, 0);
    db_helper_.push_read(gnode_raddr + GNODE_TWIM_OFF, server_rkeys_[mn_id_],
                         (uint64_t)gnode->twim, twim_buf_lkeys_[mn_id_],
                         sizeof(TWIM), 0);

    assist_ctx_->num_sample++;
    gnode++;
  }

  return num_active;
}

void DMCClient::poll_and_clear_freq_map(uint64_t write_buf_laddr) {
  // prepare write to clear freq map
  memset((void*)write_buf_laddr, 0, GNODE_TWIM_OFF - GNODE_FREQ_MAP_OFF);
  uint32_t rkey = EN_RNIC_DM ? server_dm_rkeys_[mn_id_] : server_rkeys_[mn_id_];

  db_helper_.init(20000);
  for (uint32_t i = 0; i < assist_ctx_->num_freq_map; i++) {
    printd(L_DEBUG, "clear freq map, raddr: %lx", assist_ctx_->freq_map_raddr[i]);
    db_helper_.push_write(assist_ctx_->freq_map_raddr[i], rkey,
                          write_buf_laddr, local_buf_lkey_,
                          GNODE_TWIM_OFF - GNODE_FREQ_MAP_OFF, 0);
  }

  // poll the last send in seal_and_sample
  nm_->rdma_poll_send_sid_async();
  // post write to clear freq map
  nm_->rdma_post_send_sid_async(db_helper_.head_wr, mn_id_);
}

void DMCClient::evict_group(KVOpsCtx* ctx) {
  // init eviction-related context
  AtomicEntry* merge_twim = (AtomicEntry*)shared_mm_->twim_allocator_->alloc(true);
  poll_and_clear_freq_map(ctx->write_buf_laddr);

  SampleGroupNode* gnode = (SampleGroupNode*)(ctx->read_buf_laddr);
  assist_ctx_->merge_group = {
      // use the first sampled group as the merge group
      .addr = gnode->atomic.get_gnode_pointer(),
      .gid = get_next_gid(gnode->gid),
      .old_size = gnode->atomic.get_gnode_size(),
      .segment = 0,
      .server = 0,
      .twim = merge_twim,
      .evt_twim = gnode->twim,
      .evt_loff_map = gnode->loff_map,
  };

  assist_ctx_->merge_buf_laddr = (uint64_t)(gnode + assist_ctx_->num_sample);
  assist_ctx_->merge_buf_raddr = assist_ctx_->merge_group.addr;
  assist_ctx_->merge_kv_slot = (AtomicEntry*)ctx->op_laddr;
  assist_ctx_->merge_kv_num = 0;
  assist_ctx_->merge_kv_bytes = 0;
  assist_ctx_->merge_kv_freq = 0;

  if (assist_ctx_->merge_group.old_size > MAX_GROUP_SIZE) {
    printd(L_ERROR, "%lx, gnode atomic: %lx, Invalid group size: %d (%d), gid: %d, raddr: %lx (%lx), laddr: %lx",
           &gnode->atomic, gnode->atomic.data, assist_ctx_->merge_group.old_size, gnode->atomic.get_gnode_size(), gnode->gid,
           assist_ctx_->merge_group.addr, gnode->atomic.get_gnode_pointer(), gnode);
  }

  // 1: Sort obj_meta by freq in decreasing order
  eg_sort_kv(ctx);

  // 2: Merge hot objects
  eg_lock_read_kv(ctx);     // 2.1: lock slot and read block
  eg_free_group(ctx);       // 2.2: free the sampled groups
  eg_merge_update_kv(ctx);  // 2.3: check lock, write block, and update locked slot
}

void DMCClient::eg_sort_kv(KVOpsCtx* ctx) {
  KVMeta* head = assist_ctx_->sample_kv_meta;
  KVMeta* tail = head;
  SampleGroupNode* gnode = (SampleGroupNode*)(ctx->read_buf_laddr);

  // 1: Sort obj_meta by freq in decreasing order
  for (uint8_t i = 0; i < assist_ctx_->num_sample; i++, gnode++) {
    uint16_t size = gnode->atomic.get_gnode_size();

    if (size <= MAX_GROUP_SIZE) {
      printd(L_DEBUG, "sample group, gid: %d, size: %d, raddr: %lx, laddr: %lx",
             gnode->gid, size, gnode->atomic.get_gnode_pointer(), gnode);
    } else {
      printd(L_ERROR, "Invalid group size: %d, i: %d, gid: %d, raddr: %lx, laddr: %lx",
             size, i, gnode->gid, gnode->atomic.get_gnode_pointer(), gnode);
    }

    uint16_t st_loff = 0;
    uint16_t num_active = 0;
    for (uint16_t j = 0; j < size; j++) {
      if (gnode->twim[j].get_idx_kv_len() == 0) {
        gnode->atomic.set_gnode_size(j);
        break;
      }

      if (gnode->freq_map[j] != 0) {
        // #ifdef _DEBUG
        //         if (gnode->twim[j].get_idx_kv_len() == 0) {
        //           printd(L_ERROR, "Invalid kv len: %d, gid: %d, sn: %d", gnode->twim[j].get_idx_kv_len(), gnode->gid, j);
        //         }
        // #endif
        *(tail++) = {
            .freq = gnode->freq_map[j],
            .gidx = i,
            .sn = j,
            .st_loff = st_loff,
#ifdef EVA_REAL_SIZE
            .score = (uint16_t)(gnode->freq_map[j] * 256 / gnode->twim[j].get_idx_kv_len()),
#else
            .score = gnode->freq_map[j],
#endif
        };
        num_active++;
      }

      gnode->loff_map[j] = st_loff;
      st_loff += gnode->twim[j].get_idx_kv_len();
    }
    if (st_loff > SIZE_TO_LEN(GROUP_BYTES)) {
      printd(L_ERROR, "Invalid group bytes: %d > %d", LEN_TO_SIZE(st_loff), GROUP_BYTES);
      abort();
    }
    printd(L_DEBUG, "gid: %d, num_active: %d", gnode->gid, num_active);
  }

  std::sort(head, tail, [](const KVMeta& a, const KVMeta& b) {
    return a.score > b.score;
  });
  assist_ctx_->num_sample_kv = tail - head;
  printd(L_DEBUG_EG, "sorted %d kv", assist_ctx_->num_sample_kv);
  for (uint32_t i = 0; i < 11; i++) {
    printd(L_DEBUG_EG, "kv %d: %d", assist_ctx_->num_sample_kv / 10 * i, head[assist_ctx_->num_sample_kv / 10 * i].score);
  }
}

void DMCClient::eg_lock_read_kv(KVOpsCtx* ctx) {
  // 2.1: lock slot and read block
  auto post_lock_read = [&]() {
    db_helper_.sr[0].send_flags |= IBV_SEND_SIGNALED;
    nm_->rdma_post_send_sid_async(db_helper_.head_wr, mn_id_);
    db_helper_.init();
  };

  db_helper_.init(22000);
  SampleGroupNode* gnode = (SampleGroupNode*)(ctx->read_buf_laddr);
  AtomicEntry* slot_idx = assist_ctx_->merge_kv_slot;

  KVMeta* tail = assist_ctx_->sample_kv_meta + assist_ctx_->num_sample_kv;
  for (KVMeta* kv = assist_ctx_->sample_kv_meta; kv < tail; kv++) {
    AtomicEntry* evt_twim_entry = &(gnode[kv->gidx].twim[kv->sn]);
    uint64_t slot_raddr = evt_twim_entry->get_idx_pointer();
    uint16_t kv_size = LEN_TO_SIZE(evt_twim_entry->get_idx_kv_len());

    if (slot_raddr == 0) {
      continue;
    }
    if (assist_ctx_->merge_kv_num >= MAX_GROUP_SIZE ||
        assist_ctx_->merge_kv_bytes + kv_size > GROUP_BYTES) {
      break;
    }

    uint64_t block_raddr = gnode[kv->gidx].atomic.get_gnode_pointer() + LEN_TO_SIZE(kv->st_loff);
    slot_idx->data = evt_twim_entry->data;
    slot_idx->set_idx_pointer(block_raddr);
    (slot_idx + 1)->data = slot_idx->data;
    slot_idx->set_idx_lock(true);

    db_helper_.push_read(block_raddr, server_rkeys_[mn_id_],
                         assist_ctx_->merge_buf_laddr + assist_ctx_->merge_kv_bytes,
                         local_buf_lkey_, kv_size, 0);
    db_helper_.push_cas(slot_raddr, server_rkeys_[mn_id_],
                        (uint64_t)slot_idx, local_buf_lkey_,
                        (slot_idx + 1)->data, slot_idx->data, 0);  // cas ret in slot_idx

    printd(L_DEBUG_EG, "lock slot, gidx: %d, laddr: %lx, gid: %d, sn: %d, addr: %lx, size: %d",
           kv->gidx, gnode + kv->gidx, gnode[kv->gidx].gid, kv->sn, slot_raddr, kv_size);

    slot_idx += 2;
    assist_ctx_->merge_kv_bytes += kv_size;
    assist_ctx_->merge_kv_freq += kv->freq;
    assist_ctx_->merge_group.twim[assist_ctx_->merge_kv_num++].data = evt_twim_entry->data;
    evt_twim_entry->set_idx_pointer(0);

    if (db_helper_.num_wr == AssistCtx::EVT_BATCH_SIZE * 2) {
      post_lock_read();
    }
  }

  if (db_helper_.num_wr > 0) {
    post_lock_read();
  }
}

void DMCClient::eg_free_group(KVOpsCtx* ctx) {
  // 2.2: free the sampled groups, except the first sampled group (merge group)
  SampleGroupNode* gnode = (SampleGroupNode*)(ctx->read_buf_laddr) + 1;
  for (uint32_t i = 1; i < assist_ctx_->num_sample; i++, gnode++) {
    shared_mm_->free_group(gnode);
  }

  // fill group pool using freed groups
  shared_mm_->fill_and_seal(NULL);

  // clear the merge group
  ctx->kv_remote_block.group = &assist_ctx_->merge_group;
  ctx->kv_remote_block.num_evt_sn = assist_ctx_->merge_group.old_size;
  ctx->kv_remote_block.st_evt_sn = 0;

  db_helper_.init(23000);
  prepare_lazy_evict(ctx);
  if (db_helper_.num_wr > 0) {
    nm_->rdma_post_send_sid_async(db_helper_.head_wr, mn_id_);
  }

  assist_ctx_->merge_group.old_size = 0;
}

void DMCClient::eg_merge_update_kv(KVOpsCtx* ctx) {
  uint32_t local_offset = 0, remote_offset = 0, batch_bytes = 0, merge_sn = 0;
  auto push_post_merge_update = [&]() {
    if (batch_bytes > 0) {
      db_helper_.push_write(assist_ctx_->merge_buf_raddr + remote_offset - batch_bytes, server_rkeys_[mn_id_],
                            assist_ctx_->merge_buf_laddr + local_offset - batch_bytes, local_buf_lkey_,
                            batch_bytes, 0);
      nm_->rdma_post_send_sid_async(db_helper_.head_wr, mn_id_);
      db_helper_.init();
      batch_bytes = 0;
    }
  };

  // 2.3: check lock, write block, and update locked slot
  AtomicEntry* merge_twim = assist_ctx_->merge_group.twim;
  AtomicEntry* slot_idx = assist_ctx_->merge_kv_slot;

  db_helper_.init(24000);
  for (uint32_t i = 0; i < assist_ctx_->merge_kv_num; i++, slot_idx += 2) {
    if (i % AssistCtx::EVT_BATCH_SIZE == 0)
      nm_->rdma_poll_send_sid_async();

    uint16_t kv_size = LEN_TO_SIZE(merge_twim[i].get_idx_kv_len());
    uint64_t slot_raddr = merge_twim[i].get_idx_pointer();

    if (slot_idx->data == (slot_idx + 1)->data) {
      // cas lock success
      // outplace update does not need to update version
      // non-blocking for read

      printd(L_DEBUG_EG, "merge block, gid: %d, sn: %d, size: %d, slot raddr: %lx",
             assist_ctx_->merge_group.gid, merge_sn, kv_size, slot_raddr);

      AtomicEntry* ext_checker = (AtomicEntry*)(assist_ctx_->merge_buf_laddr + local_offset);
      ext_checker->set_ext_gid(assist_ctx_->merge_group.gid);
      ext_checker->set_ext_sn(merge_sn);
      ext_checker->set_ext_version(slot_idx->get_idx_version());
      (slot_idx + 1)->data = ext_checker->data;                                 // ext atomic
      slot_idx->set_idx_pointer(assist_ctx_->merge_buf_raddr + remote_offset);  // idx atomic

      db_helper_.push_write(slot_raddr, server_rkeys_[mn_id_],
                            (uint64_t)slot_idx, local_buf_lkey_,
                            sizeof(Slot), 0);

      merge_twim[merge_sn++] = merge_twim[i];
      local_offset += kv_size;
      remote_offset += kv_size;
      batch_bytes += kv_size;

      // #if EVA_REAL_SIZE  // compact the variable block
      //       // TODO: Debugging!!! It will decrease hit ratios
      //       ObjHeader* obj_header = (ObjHeader*)ext_checker;
      //       uint16_t real_size = ROUNDUP(sizeof(ObjHeader) + obj_header->key_size + obj_header->val_size, BLOCK_SIZE);
      //       if (real_size < kv_size) {
      //         slot_idx->set_idx_kv_len(SIZE_TO_LEN(real_size));
      //         merge_twim[merge_sn - 1].set_idx_kv_len(SIZE_TO_LEN(real_size));
      //         remote_offset -= kv_size - real_size;
      //         batch_bytes -= kv_size - real_size;
      //         push_post_merge_update();
      //       }
      // #endif

      if (db_helper_.num_wr >= AssistCtx::EVT_BATCH_SIZE)
        push_post_merge_update();
    } else {
      // the slot is locked by others
      printd(L_DEBUG_EG, "give up block, gid: %d, sn: %d, size: %d, expected: %lx, actual: %lx",
             assist_ctx_->merge_group.gid, i, kv_size, (slot_idx + 1)->data, slot_idx->data);

      merge_twim[i].set_idx_pointer(0);
      assist_ctx_->merge_kv_freq -= assist_ctx_->sample_kv_meta[i].freq;

      push_post_merge_update();
      local_offset += kv_size;
    }
  }

  push_post_merge_update();
  assist_ctx_->merge_kv_num = merge_sn;
  assist_ctx_->merge_kv_bytes = remote_offset;
  assist_ctx_->merge_group.segment = assist_ctx_->get_segment();
}

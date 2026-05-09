#include "client.h"

#include <assert.h>
#include <stddef.h>
#include <sys/time.h>
#include <unistd.h>

#include <algorithm>
#include <iostream>
#include <vector>

#include "debug.h"
#include "dmc_table.h"
#include "ib.h"

// GLOBAL VARIABLES SHARED BY ALL THREADS
// init in run_client.cc
LocalCache* shared_lcs[NUM_MN] = {nullptr};
ClientSharedMM* shared_mms[MAX_NUM_THREAD / NUM_THREAD_PER_SMM][NUM_MN] = {nullptr};

DMCClient::DMCClient(const DMCConfig* conf) {
  assert(conf->block_size == BLOCK_SIZE);
  assert(conf->memory_num == NUM_MN);
  assert((GNODE_TWIM_OFF - GNODE_FREQ_MAP_OFF) % sizeof(ByteMap) == 0);
  assert(ALLOC_RETRY >= 0);  // at least 1 try

  init_eviction(conf);
  init_local_buf(conf);
  init_counters();
}

DMCClient::~DMCClient() {
  delete nm_;
  delete mm_;
}

int DMCClient::init_eviction(const DMCConfig* conf) {
  printd(L_DEBUG, "Client %d init eviction", conf->client_id);

  my_cid_ = conf->client_id;
  my_cn_ = (conf->st_client_id - 1) / (conf->num_global_clients / NUM_CN);
  en_reinsert = true;
  for (uint32_t i = 0; i < NUM_MN; i++)
    server_oom_[i] = PREFILL_MM;
  num_servers_ = conf->memory_num;
  server_base_addr_ = conf->server_base_addr;
  eviction_type_ = conf->eviction_type;
  sample_window_ = MAX_SAMPLE_NUM / 2;
  min_sample_groups_ = MAX_SAMPLE_NUM / 4;

  server_htable_addr_ = server_base_addr_;
  server_gmeta_addr_ = server_htable_addr_ + HASH_SPACE_BYTES;
  server_fifo_addr_ = server_gmeta_addr_ + GNODE_SPACE_BYTES;

  lc_master_ = smm_master_ = false;
  if (my_cid_ == conf->st_client_id) {
    lc_master_ = true;
    for (uint32_t i = 0; i < NUM_MN; i++)
      shared_lcs[i] = new LocalCache(conf);
  }

  uint32_t mm_idx = (conf->client_id - conf->st_client_id) / NUM_THREAD_PER_SMM;
  uint32_t max_num_wr = MAX_SEND_WR;
  if ((my_cid_ - conf->st_client_id) % NUM_THREAD_PER_SMM == 0) {
    smm_master_ = true;
    for (uint32_t i = 0; i < NUM_MN; i++)
      shared_mms[mm_idx][i] = new ClientSharedMM(conf);
    max_num_wr = MAX_SEND_WR_SMM;
  }

  for (uint32_t i = 0; i < NUM_MN; i++) {
    shared_lcs_[i] = shared_lcs[i];
    shared_mms_[i] = shared_mms[mm_idx][i];
    mms_[i] = new ClientExclusiveMM(shared_lcs_[i]);
  }

  nm_ = new UDPNetworkManager(conf);
  int ret = connect_all_rc_qp();
  assert(ret == 0);

  sr_ = (struct ibv_send_wr*)malloc(max_num_wr * sizeof(struct ibv_send_wr));
  sge_ = (struct ibv_sge*)malloc(max_num_wr * sizeof(struct ibv_sge));
  db_helper_.init(sr_, sge_, max_num_wr);

  gen_.seed(my_cid_);
  hash_ = dmc_new_hash(conf->hash_type);
  assert(hash_ != NULL);

#ifdef _DEBUG
  dbg_server_st_raddr = server_base_addr_;
  dbg_server_ed_raddr = server_fifo_addr_ + FIFO_SPACE_BYTES * NUM_FIFO_TYPE + ROUNDUP(conf->server_data_len / NUM_MN, conf->segment_size);
  dbg_server_htable_raddr = server_htable_addr_;
  dbg_server_gmeta_raddr = server_gmeta_addr_;
  dbg_server_fifo_raddr = server_fifo_addr_;
  dbg_server_fifo_raddr = server_gmeta_addr_ + GNODE_SPACE_BYTES;
  dbg_server_devst_raddr = server_dm_raddr_[0];
  dbg_server_deved_raddr = server_dm_raddr_[0] + RNIC_DM_BYTES;
#endif

  return 0;
}

void DMCClient::init_local_buf(const DMCConfig* conf) {
  printd(L_DEBUG, "Client %d init local buf", conf->client_id);

  struct ibv_pd* pd = nm_->get_ib_pd();
  int PROT = IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ |
             IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_REMOTE_ATOMIC;

  // local kv operation buffer
  local_buf_size_ = conf->client_local_size;  // 64MB
  local_buf_ = malloc(local_buf_size_);
  assert(local_buf_ != NULL && (uint64_t)local_buf_ % 8 == 0);

  local_buf_mr_ = ibv_reg_mr(pd, local_buf_, local_buf_size_, PROT);
  assert(local_buf_mr_ != NULL);
  local_buf_lkey_ = local_buf_mr_->lkey;

  if (lc_master_) {
    for (uint32_t i = 0; i < NUM_MN; i++) {
      freq_buf_mrs_[i] = ibv_reg_mr(pd, shared_lcs_[i]->get_freq_buf_(),
                                    shared_lcs_[i]->get_freq_buf_size(), PROT);
      assert(freq_buf_mrs_[i] != NULL);
      freq_buf_lkeys_[i] = freq_buf_mrs_[i]->lkey;
    }
  }

  if (smm_master_) {
    for (uint32_t i = 0; i < NUM_MN; i++) {
      twim_buf_mrs_[i] = ibv_reg_mr(pd, shared_mms_[i]->twim_allocator_->get_buf_addr(),
                                   shared_mms_[i]->twim_allocator_->get_buf_size(), PROT);
      assert(twim_buf_mrs_[i] != NULL);
      twim_buf_lkeys_[i] = twim_buf_mrs_[i]->lkey;
    }

    assist_ctx_ = (AssistCtx*)malloc(sizeof(AssistCtx));
    memset(assist_ctx_, 0, sizeof(AssistCtx));
    assist_ctx_->sample_kv_meta = (KVMeta*)malloc(MAX_SAMPLE_NUM * sizeof(KVMeta) * MAX_GROUP_SIZE);
  }
}

void DMCClient::init_counters() {
  printd(L_DEBUG, "Client %d init counters", my_cid_);

  num_group_evict_ = 0;
  num_success_group_evict_ = 0;
  num_object_merge_ = 0;
  num_success_object_merge_ = 0;
  num_bucket_evict_ = 0;
  num_evict_ = 0;
  num_success_bucket_evict_ = 0;
  num_retrain_ = 0;
  num_retrain_success_ = 0;
  num_buffered_flush_ = 0;
  num_real_flush_ = 0;
  num_read_checksum_failed_ = 0;
  num_evict_read_bucket_ = 0;
  num_success_evict_ = 0;
  num_set_retry_ = 0;
  num_rdma_send_ = 0;
  num_rdma_recv_ = 0;
  num_rdma_read_ = 0;
  num_rdma_write_ = 0;
  num_rdma_cas_ = 0;
  num_rdma_faa_ = 0;
  num_udp_req_ = 0;
  num_merge_obj_ = 0;
  inference_time_ = 0;
}

// The size of a segment is known by the client and the server.
// For new FORGE to avoid local blocking,
// we can prefill the segment list, or alloc one segment at initialization
int DMCClient::alloc_segment(KVOpsCtx* ctx) {
  if (server_oom_[mn_id_] == true) {
    return -1;
  }
  num_udp_req_++;

  printd(L_DEBUG, "Client allocating new segment");

  // construct RPC request
  *(uint16_t*)ctx->op_laddr = IBMSG_REQ_ALLOC;
  *(uint16_t*)((uint64_t)ctx->op_laddr + sizeof(uint16_t)) = my_cid_;

  // post rr first
  struct ibv_recv_wr rr;
  struct ibv_sge recv_sge;
  memset(&rr, 0, sizeof(struct ibv_recv_wr));
  ib_create_sge(ctx->op_laddr + BLOCK_SIZE, local_buf_lkey_, BLOCK_SIZE,
                &recv_sge);
  rr.wr_id = 1;
  rr.next = NULL;
  rr.sg_list = &recv_sge;
  rr.num_sge = 1;
  int ret = nm_->rdma_post_recv_sid_async(&rr, mn_id_);
  assert(ret == 0);

  struct ibv_send_wr sr;
  struct ibv_sge send_sge;
  memset(&sr, 0, sizeof(struct ibv_send_wr));
  ib_create_sge(ctx->op_laddr, local_buf_lkey_, BLOCK_SIZE, &send_sge);
  sr.wr_id = 0;
  sr.next = NULL;
  sr.sg_list = &send_sge;
  sr.num_sge = 1;
  sr.opcode = IBV_WR_SEND;
  sr.send_flags = IBV_SEND_SIGNALED;
  ret = nm_->rdma_post_send_sid_sync(&sr, mn_id_);
  assert(ret == 0);

  struct ibv_wc recv_wc;
  ret = nm_->rdma_poll_one_recv_completion_sync(&recv_wc);
  assert(ret == 0);
  assert(recv_wc.status == IBV_WC_SUCCESS);
  assert(recv_wc.opcode == IBV_WC_RECV);
  assert(recv_wc.wr_id == 1);
  assert(*(uint16_t*)((uint64_t)ctx->op_laddr + BLOCK_SIZE) == IBMSG_REP_ALLOC);

  uint64_t alloc_addr =
      *(uint64_t*)(ctx->op_laddr + BLOCK_SIZE + sizeof(uint16_t));
  if (alloc_addr == 0) {
    printd(L_INFO, "Server out-of-memory");
    server_oom_[mn_id_] = true;
    return -1;
  }

  shared_mm_->add_segment(alloc_addr, mn_id_);
  shared_mm_->fill_and_seal(NULL);
  return 0;
}

int DMCClient::connect_all_rc_qp() {
  int ret;
  for (int i = 0; i < num_servers_; i++) {
    MrInfo mr;
    MrInfo dm_mr_info;
    ret = nm_->client_connect_one_rc_qp(i, &mr, &dm_mr_info);
    assert(ret == 0);
    server_rkeys_[i] = mr.rkey;
    server_dm_raddr_[i] = dm_mr_info.addr;
    server_dm_rkeys_[i] = dm_mr_info.rkey;

    printd(L_DEBUG, "client: %d, server: %d, rkey: 0x%x, dm_rkey: 0x%x",
           my_cid_, i, server_rkeys_[i], server_dm_rkeys_[i]);
    if (dm_mr_info.addr != 0) {
      abort();
    }
  }
  return 0;
}

void DMCClient::create_op_ctx(void* key, uint32_t key_size,
                              void* val, uint32_t val_size,
                              uint8_t op_type, __OUT KVOpsCtx* ctx) {
  memset(ctx, 0, sizeof(KVOpsCtx));
  ctx->op_type = op_type;
  ctx->key = key;
  ctx->val = val;
  ctx->key_size = key_size;
  ctx->val_size = val_size;
  ctx->kv_size = sizeof(ObjHeader) + key_size + val_size;
  ctx->ret = DMC_SUCCESS;
  ctx->key_found = false;

  // calculate hash
  ctx->key_hash = hash_->hash_func1(key, key_size);
  HashIndexComputeFp(ctx->key_hash, &ctx->fp, &ctx->ext_fp);
  ctx->bucket_raddr = get_bucket_raddr(ctx->key_hash);

  // for mn scalability
  mn_id_ = ctx->key_hash % NUM_MN;
  mm_ = mms_[mn_id_];
  shared_mm_ = shared_mms_[mn_id_];
  shared_lc_ = shared_lcs_[mn_id_];
  local_fifo_ = &shared_lc_->local_fifo[SFIFO];

  // local_buf_: transient_buf: 64B | write_buf_: header + key + val | op: 1 MB | bucket: sizeof(Bucket) | read_buf
  // Attention: ctx->write_buf_laddr cannot be used as async buffer, since it  can be overwrite by next SET req
  ctx->write_buf_laddr = ROUNDUP((uint64_t)local_buf_ + 64, BLOCK_SIZE);  // reserve 64B for transient_buf
  ctx->target_block_laddr = ctx->write_buf_laddr;
  ((ObjHeader*)(ctx->write_buf_laddr))->key_size = key_size;
  ((ObjHeader*)(ctx->write_buf_laddr))->val_size = val_size;
  uint64_t key_addr = ctx->write_buf_laddr + sizeof(ObjHeader);
  uint64_t val_addr = key_addr + key_size;
  memcpy((void*)key_addr, key, key_size);
  memcpy((void*)val_addr, val, val_size);

  ctx->op_laddr = ctx->write_buf_laddr + BLOCK_SIZE * MAX_KV_LEN;
  ctx->bucket_laddr = ctx->op_laddr + 2 * MAX_GROUP_SIZE * sizeof(AtomicEntry);
  ctx->read_buf_laddr = ROUNDUP(ctx->bucket_laddr + sizeof(Bucket), BLOCK_SIZE);
}

void DMCClient::prepare_lazy_evict(KVOpsCtx* ctx) {
  if (ctx->kv_remote_block.num_evt_sn == 0)
    return;

  Group* group = ctx->kv_remote_block.group;
  uint16_t st_evt_sn = ctx->kv_remote_block.st_evt_sn;
  uint16_t ed_evt_sn = st_evt_sn + ctx->kv_remote_block.num_evt_sn;
  ctx->kv_remote_block.num_evt_sn = 0;

  for (uint16_t sn = st_evt_sn; sn < ed_evt_sn; sn++) {
    uint64_t evt_slot_raddr = group->evt_twim[sn].get_idx_pointer();
    if (evt_slot_raddr == 0)
      continue;

#ifdef WRITE_EVICT_FLAG
    AtomicEntry* evt_buf = (AtomicEntry*)ctx->op_laddr - (ed_evt_sn - sn);
    evt_buf->data = group->evt_twim[sn].data;
    evt_buf->set_idx_pointer(group->addr + LEN_TO_SIZE(group->evt_loff_map[sn]));
    evt_buf->set_ext_sn(WRITE_EVICT_FLAG);
    db_helper_.push_write(evt_slot_raddr + SLOT_EXT_OFF, server_rkeys_[mn_id_],
                          (uint64_t)evt_buf, local_buf_lkey_,
                          sizeof(AtomicEntry), 0);
#else
    AtomicEntry empty_slot{0};
    empty_slot.set_idx_version(group->evt_twim[sn].get_idx_version());

    group->evt_twim[sn].set_idx_pointer(group->addr + LEN_TO_SIZE(group->evt_loff_map[sn]));
    db_helper_.push_cas(evt_slot_raddr, server_rkeys_[mn_id_],
                        (uint64_t)local_buf_, local_buf_lkey_,
                        group->evt_twim[sn].data, empty_slot.data, 0);
#endif

    printd(L_DEBUG, "lazy evict, gid: %d, sn: %d, slot raddr: %lx, pointer: %lx, group addr: %lx",
           group->gid, sn, evt_slot_raddr, group->evt_twim[sn].get_idx_pointer(), group->addr);
  }
}

void DMCClient::kv_set_delete_duplicate(KVOpsCtx* ctx) {
  if (*(uint64_t*)ctx->target_slot_laddr != *(uint64_t*)ctx->op_laddr) {
    // my own key is evicted before
    // the evictor should manage the rblock
    return;
  }

  Slot* slot = (Slot*)ctx->bucket_laddr;
  Slot* l_slot;
  uint8_t num_fp_match = 0;
  uint8_t fp_match_slot_id[HASH_BUCKET_ASSOC_NUM];

  for (uint32_t i = 0; i < HASH_BUCKET_ASSOC_NUM; i++) {
    l_slot = &slot[i];

    if (l_slot->is_empty() || ctx->fp != l_slot->index.get_idx_fp() ||
        (l_slot->is_consistent() && l_slot->extra.get_ext_fp() != ctx->ext_fp))
      continue;

    fp_match_slot_id[num_fp_match++] = i;
  }

  if (num_fp_match == 1) {
    // only one key matches, no need to read the kv
    return;
  }

  uint64_t key_match_slot_laddr = 0;
  uint32_t idx;

  for (uint32_t i = 0; i < num_fp_match; i++) {
    idx = fp_match_slot_id[i];
    l_slot = &slot[idx];

#ifdef _DEBUG
    if (l_slot->index.get_idx_pointer() > dbg_server_ed_raddr) {
      printd(L_ERROR, "raddr %lx out of range, slot raddr: %lx, idx: %lx, idx version: %d, ext version: %ld, gid: %d, sn: %d",
             l_slot->index.get_idx_pointer(), ctx->bucket_raddr + sizeof(Slot) * idx, l_slot->index.data,
             l_slot->index.get_idx_version(), l_slot->extra.get_ext_version(),
             l_slot->extra.get_ext_gid(), l_slot->extra.get_ext_sn());
    }
#endif

    db_helper_.init(2200);
    db_helper_.push_read(l_slot->index.get_idx_pointer(), server_rkeys_[mn_id_],
                         ctx->read_buf_laddr, local_buf_lkey_,
                         sizeof(ObjHeader) + ctx->key_size, IBV_SEND_SIGNALED);
    nm_->rdma_post_send_sid_sync(db_helper_.head_wr, mn_id_);

    if (compare_key(idx, ctx) == KEY_MATCH) {
      key_match_slot_laddr = (uint64_t)l_slot;
      break;
    }
  }

  if (key_match_slot_laddr == ctx->target_slot_laddr || key_match_slot_laddr == 0) {
    // my key is first or all keys are evicted before
    return;
  }

// CAS my slot to 0 and free the remote block
#ifdef WRITE_EVICT_FLAG
  l_slot = (Slot*)ctx->target_slot_laddr;
  l_slot->extra.data = l_slot->index.data;
  l_slot->extra.set_ext_sn(WRITE_EVICT_FLAG);
  db_helper_.push_write(ctx->target_slot_raddr + SLOT_EXT_OFF, server_rkeys_[mn_id_],
                        ctx->target_slot_laddr + SLOT_EXT_OFF, local_buf_lkey_,
                        sizeof(AtomicEntry), 0);
#else
  AtomicEntry empty_slot{0};
  empty_slot.set_idx_version(((AtomicEntry*)ctx->target_slot_laddr)->get_idx_version());
  db_helper_.push_cas(ctx->target_slot_raddr, server_rkeys_[mn_id_],
                      (uint64_t)local_buf_, local_buf_lkey_,
                      *(uint64_t*)ctx->target_slot_laddr, empty_slot.data, 0);
#endif
  nm_->rdma_post_send_sid_async(db_helper_.head_wr, mn_id_);
  ctx->commit_rblock = false;

  printd(L_DEBUG, "My fp: %hx, bucket id: %d, delete duplicate key, slot id: %d, slot raddr: %lx",
         ctx->fp, (ctx->bucket_raddr - server_htable_addr_) / sizeof(Bucket),
         (ctx->target_slot_laddr - ctx->bucket_laddr) / sizeof(Slot), ctx->target_slot_raddr);
}

inline int DMCClient::compare_key(uint8_t idx, KVOpsCtx* ctx) {
  // compare key
  Slot* slot = (Slot*)(ctx->bucket_laddr + sizeof(Slot) * idx);
  uint64_t slot_raddr = ctx->bucket_raddr + sizeof(Slot) * idx;
  AtomicEntry ext_checker = *(AtomicEntry*)(ctx->read_buf_laddr);

  void *key = NULL, *val = NULL;
  uint32_t key_size = 0, val_size = 0;
  parse_obj((void*)(ctx->read_buf_laddr), &key, &key_size, &val, &val_size);

  if (!is_key_match(key, key_size, ctx->key, ctx->key_size)) {
#ifdef _DEBUG
    char key_buf[256] = {0};
    if (key_size < 256) {
      memcpy(key_buf, key, key_size);
      printd(L_DEBUG, "Key not matched. key: %s(%s)", ctx->key, key_buf);
    } else {
      printd(L_DEBUG, "Key not matched. key: %s(invalid key size: %d)", ctx->key, key_size);
    }
#endif

    if (slot->is_consistent() && slot->index.get_idx_lock() == 0 &&
        !slot->extra.ext_match_mask_ver(ext_checker)) {
      printd(L_DEBUG, "gid: %d(%d), sn: %d(%d), fp: %hx(%hx), lock: %d",
             slot->extra.get_ext_gid(), ext_checker.get_ext_gid(),
             slot->extra.get_ext_sn(), ext_checker.get_ext_sn(),
             slot->extra.get_ext_fp(), ext_checker.get_ext_fp(),
             slot->index.get_idx_lock());
      // lazy eviction
      db_helper_.init(1200);

#ifdef WRITE_EVICT_FLAG
      slot->extra.data = slot->index.data;
      slot->extra.set_ext_sn(WRITE_EVICT_FLAG);
      db_helper_.push_write(slot_raddr + SLOT_EXT_OFF, server_rkeys_[mn_id_],
                            (uint64_t)slot + SLOT_EXT_OFF, local_buf_lkey_,
                            sizeof(AtomicEntry), 0);
#else
      AtomicEntry empty_slot{0};
      empty_slot.set_idx_version(slot->index.get_idx_version());
      db_helper_.push_cas(slot_raddr, server_rkeys_[mn_id_],
                          (uint64_t)local_buf_, local_buf_lkey_,
                          slot->index.data, empty_slot.data, 0);
#endif
      nm_->rdma_post_send_sid_async(db_helper_.head_wr, mn_id_);

      printd(L_DEBUG, "Key lazy evicted, slot raddr: %lx, key: %s(%s), gid: %d(%d), sn: %d(%d), fp: %hx(%hx)",
             slot_raddr, ctx->key, key_buf, slot->extra.get_ext_gid(), ext_checker.get_ext_gid(),
             slot->extra.get_ext_sn(), ext_checker.get_ext_sn(), slot->extra.get_ext_fp(), ext_checker.get_ext_fp());
    }
    return KEY_NOT_MATCH;
  }

  if (!slot->is_consistent()) {
    slot->extra.data = ext_checker.data;
    slot->extra.set_ext_version(slot->index.get_idx_version());
  }

  if (ctx->op_type == GET) {
    shared_lc_->update_metrics(ext_checker.get_ext_gid(), ext_checker.get_ext_sn());
  }

  ctx->key_found = true;
  ctx->target_slot_laddr = (uint64_t)slot;
  ctx->target_slot_raddr = slot_raddr;
  ctx->target_block_laddr = ctx->read_buf_laddr;
  ctx->target_block_raddr = slot->index.get_idx_pointer();

#ifdef _DEBUG
  if (slot->index.get_idx_pointer() > dbg_server_ed_raddr) {
    printd(L_ERROR, "raddr %lx out of range, slot raddr: %lx, idx: %lx, idx version: %d, ext version: %ld, gid: %d, sn: %d",
           slot->index.get_idx_pointer(), slot_raddr, slot->index.data,
           slot->index.get_idx_version(), slot->extra.get_ext_version(),
           slot->extra.get_ext_gid(), slot->extra.get_ext_sn());
  }
#endif

  printd(L_DEBUG, "Key matched");
  return KEY_MATCH;
}

void DMCClient::read_index(KVOpsCtx* ctx) {
  db_helper_.init(1000);
  db_helper_.push_read(ctx->bucket_raddr, server_rkeys_[mn_id_],
                       ctx->bucket_laddr, local_buf_lkey_,
                       sizeof(Bucket), IBV_SEND_SIGNALED);
#if GET_CHECK_MM
  nm_->rdma_post_send_sid_sync(db_helper_.head_wr, mn_id_);
#else
  if (!lc_master_) {
    nm_->rdma_post_send_sid_sync(db_helper_.head_wr, mn_id_);
  } else {
    static uint8_t fifo_type = SFIFO;
    switch_fifo_type((fifo_type++) % NUM_FIFO_TYPE);  // switch fifo type

    db_helper_.push_read(local_fifo_->clock_raddr, server_dm_rkeys_[mn_id_],
                         ctx->read_buf_laddr, local_buf_lkey_,
                         sizeof(uint64_t), IBV_SEND_SIGNALED);
    nm_->rdma_post_send_sid_sync(db_helper_.head_wr, mn_id_);  // poll the clock first
    poll_and_fetch_flush_fifo(ctx, *(uint64_t*)ctx->read_buf_laddr);
  }
#endif
}

void DMCClient::kv_get_copy_value(KVOpsCtx* ctx,
                                  __OUT void* val,
                                  __OUT uint32_t* val_len) {
  void* rval = NULL;
  uint32_t rval_len = 0;
  get_obj_value((void*)(ctx->target_block_laddr), &rval, &rval_len);
  memcpy(val, rval, rval_len);
  *val_len = rval_len;
}

uint8_t DMCClient::kv_set_alloc_rblock(KVOpsCtx* ctx) {
  // Return if already allocated
  if (ctx->alloc_type != ALLOC_FAIL) {
    return ctx->alloc_type;
  }

  // Try exclusive allocation first
  ctx->alloc_type = mm_->alloc(ctx->kv_size, &ctx->kv_remote_block);
  if (ctx->alloc_type == ALLOC_EXCLUSIVE) {
    printd(L_DEBUG, "alloc exclusive, gid: %d, sn: %d, addr: %lx, size: %u",
           ctx->kv_remote_block.gid, ctx->kv_remote_block.sn,
           ctx->kv_remote_block.addr, ctx->kv_remote_block.size);
    return ctx->alloc_type;
  }

  // Try shared allocation with retries
  for (int retry = ALLOC_RETRY; retry >= 0; retry--) {
    ctx->alloc_type = shared_mm_->alloc(ctx->kv_size, &ctx->kv_remote_block);
    if (ctx->alloc_type != ALLOC_FAIL || smm_master_) {
      printd(L_DEBUG, "alloc shared, gid: %d, sn: %d, addr: %lx, size: %u",
             ctx->kv_remote_block.gid, ctx->kv_remote_block.sn,
             ctx->kv_remote_block.addr, ctx->kv_remote_block.size);
      return ctx->alloc_type;
    }
  }

  // Failed to allocate after all retries
  printd(L_INFO, "alloc shared failed, gid: %d, sn: %d, size: %u",
         ctx->kv_remote_block.gid, ctx->kv_remote_block.sn, ctx->kv_size);
  return ctx->alloc_type;
}

void DMCClient::prepare_set_twim_entry(KVOpsCtx* ctx, AtomicEntry* twim_entry) {
  // set new twim_entry
  twim_entry->data = ((Slot*)ctx->op_laddr)->index.data;
  twim_entry->set_idx_pointer(ctx->target_slot_raddr);

  Group* group = ctx->kv_remote_block.group;
  if (group != NULL && group->twim[ctx->kv_remote_block.sn].get_idx_kv_len() == twim_entry->get_idx_kv_len())
    group->twim[ctx->kv_remote_block.sn].data = twim_entry->data;

  if (group == NULL || group->gid != ctx->kv_remote_block.gid) {
    uint64_t twim_entry_raddr = get_twim_entry_raddr(ctx->kv_remote_block.gid, ctx->kv_remote_block.sn);
    uint8_t shift = AtomicEntry::get_idx_fp_shift() / 8;
    db_helper_.push_write(twim_entry_raddr + shift, server_rkeys_[mn_id_],
                          (uint64_t)twim_entry + shift, local_buf_lkey_,
                          sizeof(AtomicEntry) - shift, 0);
  }

  printd(L_DEBUG, "set twim_entry, gid: %d, sn: %d, addr: %lx, block raddr: %lx, shared: %d",
         ctx->kv_remote_block.gid, ctx->kv_remote_block.sn,
         ctx->target_slot_raddr, ctx->kv_remote_block.addr, group != NULL);

  Slot* slot = (Slot*)ctx->target_slot_laddr;
  if (slot->is_empty() || !slot->is_consistent())  // insert_empty does not need to clear twim_entry and free hole
    return;

#ifdef _DEBUG
  if (slot->index.get_idx_pointer() > dbg_server_ed_raddr) {
    printd(L_ERROR, "raddr %lx out of range, slot raddr: %lx, idx: %lx, idx version: %d, ext version: %ld, gid: %d, sn: %d",
           slot->index.get_idx_pointer(), ctx->target_slot_raddr, slot->index.data,
           slot->index.get_idx_version(), slot->extra.get_ext_version(),
           slot->extra.get_ext_gid(), slot->extra.get_ext_sn());
  }
#endif

  RemoteBlock hole{slot->index.get_idx_pointer(),
                   LEN_TO_SIZE(slot->index.get_idx_kv_len()),
                   slot->extra.get_ext_gid(), slot->extra.get_ext_sn(),
                   0, 0, NULL};
  mm_->free(&hole);

  printd(L_DEBUG, "free hole, gid: %d, sn: %d, slot raddr: %lx, block raddr: %lx, slot: %lx",
         slot->extra.get_ext_gid(), slot->extra.get_ext_sn(), ctx->target_slot_raddr, slot->index.get_idx_pointer(), slot->index.data);
}

int DMCClient::kv_insert_evict_bucket(KVOpsCtx* ctx) {
  std::pair<int, int> prio_slot_id[HASH_BUCKET_ASSOC_NUM];
  uint32_t prio_slot_id_size = 0;
  Slot* slot = (Slot*)ctx->bucket_laddr;
  uint32_t r_start = gen_();

  for (int j = 0; j < HASH_BUCKET_ASSOC_NUM; j++) {
    uint32_t idx = (j + r_start) % HASH_BUCKET_ASSOC_NUM;
    Slot* l_slot = &slot[idx];

    int prio = l_slot->is_consistent() ? shared_lc_->get_obj_freq(l_slot->extra.get_ext_gid(),
                                                                  l_slot->extra.get_ext_sn())
                                       : 16;
    int pos = prio_slot_id_size;
    for (; pos > 0; pos--) {
      if (prio_slot_id[pos - 1].first > prio) {
        prio_slot_id[pos] = prio_slot_id[pos - 1];
      } else {
        break;
      }
    }
    prio_slot_id[pos] = std::make_pair(prio, idx);
    prio_slot_id_size++;
  }

  for (int i = 0; i < prio_slot_id_size; i++) {
    uint32_t idx = prio_slot_id[i].second;
    Slot* l_slot = &slot[idx];
    ctx->target_slot_laddr = (uint64_t)l_slot;
    ctx->target_slot_raddr = ctx->bucket_raddr + sizeof(Slot) * idx;

    if (kv_set_update_index(ctx) == 1)
      return DMC_SUCCESS;
  }

  // Come here when all CASes fail (i.e., slots were evicted by others)
  return DMC_SET_RETRY;
}

int DMCClient::kv_insert_empty_slot(KVOpsCtx* ctx) {
  Slot* slot = (Slot*)ctx->bucket_laddr;
  uint32_t r_start = gen_();
  for (uint32_t i = 0; i < ctx->num_empty_slot; i++) {
    uint32_t idx = ctx->empty_slot_id[(i + r_start) % ctx->num_empty_slot];
    Slot* l_slot = &slot[idx];
    ctx->target_slot_laddr = (uint64_t)l_slot;
    ctx->target_slot_raddr = ctx->bucket_raddr + sizeof(Slot) * idx;

    if (kv_set_update_index(ctx) == 1)
      return DMC_SUCCESS;
  }

  // Come here when all CASes fail (i.e., slots were evicted by others)
  return DMC_SET_RETRY;
}

// return: 0 for set hit, -1 for set miss
int DMCClient::kv_set_1s(void* key,
                         uint32_t key_size,
                         void* val,
                         uint32_t val_size) {
  printd(L_DEBUG, "set key %s", (char*)key);
  KVOpsCtx ctx;
  create_op_ctx(key, key_size, val, val_size, SET, &ctx);
  int ret = 0;

  if (kv_set_alloc_rblock(&ctx) == ALLOC_FAIL)
    goto kv_set_1s_done;
  kv_set_read_index_write_kv(&ctx);

kv_set_1s_retry:
  search_bucket_read_kv(&ctx);
  if (ctx.key_found) {
    kv_set_update_index(&ctx);
    goto kv_set_1s_done;
  }

  ret = ctx.num_empty_slot > 0 ? kv_insert_empty_slot(&ctx)
                               : kv_insert_evict_bucket(&ctx);

  read_index(&ctx);
  if (ret == DMC_SET_RETRY)
    goto kv_set_1s_retry;
  kv_set_delete_duplicate(&ctx);

kv_set_1s_done:
  check_mm(&ctx);
  return ctx.key_found ? 0 : -1;
}

void DMCClient::kv_set_read_index_write_kv(KVOpsCtx* ctx) {
  AtomicEntry* ext_checker = (AtomicEntry*)ctx->write_buf_laddr;
  ext_checker->init_ext(ctx->kv_remote_block.gid, ctx->ext_fp,
                        ctx->kv_remote_block.sn, 0);

  db_helper_.init(2000);
  db_helper_.push_write(ctx->kv_remote_block.addr, server_rkeys_[mn_id_],
                        ctx->write_buf_laddr, local_buf_lkey_,
                        ctx->kv_size, 0);
  prepare_lazy_evict(ctx);
  db_helper_.push_read(ctx->bucket_raddr, server_rkeys_[mn_id_],
                       ctx->bucket_laddr, local_buf_lkey_,
                       sizeof(Bucket), IBV_SEND_SIGNALED);

  nm_->rdma_post_send_sid_sync(db_helper_.head_wr, mn_id_);
}

// the caller should ensure that ctx->target_slot_laddr and ctx->target_slot_raddr are set
int DMCClient::kv_set_update_index(KVOpsCtx* ctx) {
  Slot* slot = (Slot*)(ctx->target_slot_laddr);
  if (slot->index.get_idx_lock()) {
    printd(L_DEBUG, "slot is locked, slot raddr: %lx", ctx->target_slot_raddr);
    return 0;
  }

  uint8_t version = slot->index.get_idx_version() + 1;
  Slot* new_slot = (Slot*)ctx->op_laddr;
  Slot* ret_slot = new_slot + 1;

  new_slot->index.init_idx(ctx->fp, SIZE_TO_LEN(ctx->kv_remote_block.size),
                           ctx->kv_remote_block.addr, 0, version);
  new_slot->extra.init_ext(ctx->kv_remote_block.gid, ctx->ext_fp,
                           ctx->kv_remote_block.sn, version);

  db_helper_.init(2100);
  db_helper_.push_cas(ctx->target_slot_raddr, server_rkeys_[mn_id_],
                      (uint64_t)&ret_slot->index, local_buf_lkey_,
                      slot->index.data, new_slot->index.data,
                      IBV_SEND_SIGNALED);
  nm_->rdma_post_send_sid_sync(db_helper_.head_wr, mn_id_);

  if (ret_slot->index.data == slot->index.data) {
    db_helper_.init();
    AtomicEntry* twim_entry = (AtomicEntry*)(ret_slot + 1);
    prepare_set_twim_entry(ctx, twim_entry);
    db_helper_.push_write(ctx->target_slot_raddr + SLOT_EXT_OFF, server_rkeys_[mn_id_],
                          (uint64_t)&new_slot->extra, local_buf_lkey_,
                          sizeof(AtomicEntry), 0);
    nm_->rdma_post_send_sid_async(db_helper_.head_wr, mn_id_);
    ctx->commit_rblock = true;

    printd(L_DEBUG, "update index, slot raddr: %lx, new version: %d",
           ctx->target_slot_raddr, version);
    return 1;
  }

  return 0;
}

void DMCClient::check_mm(KVOpsCtx* ctx) {
  // commit_rblock is set once only when sure to complete the SET operation
  if (ctx->alloc_type != ALLOC_FAIL && !ctx->commit_rblock) {
    ctx->kv_remote_block.group = NULL;
    mm_->free(&ctx->kv_remote_block);
  }

  if (smm_master_ && ctx->alloc_type <= ALLOC_SHARED_SEAL) {
    while (true) {
      shared_mm_->fill_and_seal(assist_ctx_);
      db_helper_.init(12000);
      seal_reinsert(ctx, SFIFO);

      if (!assist_ctx_->need_evict)
        break;

      if (!server_oom_[mn_id_] && alloc_segment(ctx) == 0) {
        assist_ctx_->need_evict = false;
        continue;
      }

      sample(ctx);
      evict_group(ctx);
    }
  }
}

int DMCClient::kv_get_1s(void* key,
                         uint32_t key_size,
                         __OUT void* val,
                         __OUT uint32_t* val_size) {
  printd(L_DEBUG, "get key %s", key);
  KVOpsCtx ctx;
  create_op_ctx(key, key_size, NULL, 0, GET, &ctx);  // ATTENTION: VALUE SIZE IS 0.

  read_index(&ctx);             // Reading index using 1 RDMA read in 1-RTT
  search_bucket_read_kv(&ctx);  // Search slots with matched fp, read kv and compare key

  if (ctx.key_found == false) {
    printd(L_DEBUG, "get miss %s", (char*)key);
    key_miss_ = true;
    return -1;
  }

  kv_get_copy_value(&ctx, val, val_size);
#if GET_CHECK_MM
  check_mm(&ctx);
#endif
  return 0;
}

void DMCClient::search_bucket_read_kv(KVOpsCtx* ctx) {
  Slot* slot = (Slot*)ctx->bucket_laddr;
  Slot* l_slot;
  uint64_t slot_raddr;
  uint64_t kv_raddr;
  uint32_t kv_size;

  ctx->num_empty_slot = 0;
  ctx->key_found = false;

  for (uint32_t i = 0; i < HASH_BUCKET_ASSOC_NUM; i++) {
    l_slot = &slot[i];
    slot_raddr = ctx->bucket_raddr + i * sizeof(Slot);

#ifdef _DEBUG
    if (l_slot->index.get_idx_pointer() > dbg_server_ed_raddr) {
      printd(L_ERROR, "raddr %lx out of range, slot raddr: %lx, idx: %lx, idx version: %d, ext version: %ld, gid: %d, sn: %d",
             l_slot->index.get_idx_pointer(), slot_raddr, l_slot->index.data,
             l_slot->index.get_idx_version(), l_slot->extra.get_ext_version(),
             l_slot->extra.get_ext_gid(), l_slot->extra.get_ext_sn());
    }
#endif
    if (l_slot->is_empty()) {
      if (ctx->op_type == SET)
        ctx->empty_slot_id[ctx->num_empty_slot++] = i;
      continue;
    }

    if (ctx->fp != l_slot->index.get_idx_fp() ||
        (l_slot->is_consistent() && ctx->ext_fp != l_slot->extra.get_ext_fp()))
      continue;

    if (shared_lc_->local_get(l_slot, ctx) && compare_key(i, ctx) == KEY_MATCH)
      break;

    kv_raddr = l_slot->index.get_idx_pointer();
    kv_size = ctx->op_type == GET ? LEN_TO_SIZE(l_slot->index.get_idx_kv_len())
                                  : sizeof(ObjHeader) + ctx->key_size;

    db_helper_.init(1100);
    db_helper_.push_read(kv_raddr, server_rkeys_[mn_id_],
                         ctx->read_buf_laddr, local_buf_lkey_,
                         kv_size, IBV_SEND_SIGNALED);
    nm_->rdma_post_send_sid_sync(db_helper_.head_wr, mn_id_);

    printd(L_DEBUG, "read kv of slot[%d], raddr: %lx, size: %d, version: %d. Group id: %d, sn: %d, version: %d",
           i, kv_raddr, kv_size, l_slot->index.get_idx_version(), l_slot->extra.get_ext_gid(),
           l_slot->extra.get_ext_sn(), l_slot->extra.get_ext_version());

    if (compare_key(i, ctx) == KEY_MATCH) {
      shared_lc_->local_set(ctx);
      break;
    }
  }
}

// One-sided get
int DMCClient::kv_get(void* key,
                      uint32_t key_size,
                      __OUT void* val,
                      __OUT uint32_t* val_size) {
  int ret = kv_get_1s(key, key_size, val, val_size);
  return ret;
}

// Two-sided get for EVICT_CLIQUEMAP. One-sided get otherwise.
int DMCClient::kv_set(void* key,
                      uint32_t key_size,
                      void* val,
                      uint32_t val_size) {
  int ret = 0;
  ret = kv_set_1s(key, key_size, val, val_size);
  n_set_miss_ += (ret < 0);
  return ret;
}

std::string DMCClient::get_str_key(KVOpsCtx* ctx) {
  char key_buf[256] = {0};
  memcpy(key_buf, ctx->key, ctx->key_size);
  return std::string(key_buf);
}

uint64_t DMCClient::get_mm_free_size() {
  return mm_->get_free_size();
}

#ifndef _DMC_CLIENT_H_
#define _DMC_CLIENT_H_

#define FORGE 1

#include <infiniband/verbs.h>

#include <atomic>
#include <list>
#include <map>
#include <queue>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

#include "client_ctx.h"
#include "client_mm.h"
#include "db_helper.h"
#include "dmc_table.h"
#include "dmc_utils.h"
#include "local_cache.h"
#include "nm.h"

class DMCClient {
  uint16_t my_cid_;
  uint8_t my_cn_;
  uint8_t num_servers_;
  uint64_t server_base_addr_;
  uint64_t server_htable_addr_;
  uint64_t server_gmeta_addr_;
  uint64_t server_fifo_addr_;

  uint32_t local_buf_size_;
  void* local_buf_;
  struct ibv_mr* local_buf_mr_;

  DMCHash* hash_;
  UDPNetworkManager* nm_;
  ClientExclusiveMM* mm_;
  ClientSharedMM* shared_mm_;
  LocalCache* shared_lc_;
  LocalFifo* local_fifo_;

  // for mn scalability
  uint8_t mn_id_;
  ClientExclusiveMM* mms_[NUM_MN];
  ClientSharedMM* shared_mms_[NUM_MN];
  LocalCache* shared_lcs_[NUM_MN];
  struct ibv_mr* twim_buf_mrs_[NUM_MN];
  struct ibv_mr* freq_buf_mrs_[NUM_MN];

  bool lc_master_;
  bool smm_master_;
  bool server_oom_[NUM_MN];
  bool key_miss_;

  // group-level eviction
  uint8_t eviction_type_;
  uint32_t sample_window_;
  uint32_t min_sample_groups_;

  uint64_t server_dm_raddr_[NUM_MN];
  uint32_t server_dm_rkeys_[NUM_MN];
  uint32_t server_rkeys_[NUM_MN];
  uint32_t twim_buf_lkeys_[NUM_MN];
  uint32_t freq_buf_lkeys_[NUM_MN];
  uint32_t local_buf_lkey_;

  // for generate random number
  std::mt19937 gen_;

  // db helper
  DBHelper db_helper_;
  struct ibv_send_wr* sr_;
  struct ibv_sge* sge_;

  AssistCtx* assist_ctx_;

 public:
  // signal
  bool en_reinsert;
  // counters
  uint64_t num_group_evict_;
  uint64_t num_success_group_evict_;
  uint64_t num_object_merge_;
  uint64_t num_success_object_merge_;
  uint64_t num_bucket_evict_;
  uint64_t num_success_bucket_evict_;
  uint64_t num_retrain_;
  uint64_t num_retrain_success_;
  uint64_t num_buffered_flush_;
  uint64_t num_real_flush_;
  uint64_t num_read_checksum_failed_;
  uint64_t num_evict_;
  uint64_t num_evict_read_bucket_;
  uint64_t num_success_evict_;
  uint64_t num_set_retry_;
  uint64_t num_rdma_send_;
  uint64_t num_rdma_recv_;
  uint64_t num_rdma_read_;
  uint64_t num_rdma_write_;
  uint64_t num_rdma_cas_;
  uint64_t num_rdma_faa_;
  uint64_t num_udp_req_;
  uint64_t num_merge_obj_;
  uint64_t inference_time_;

 private:
  void init_local_buf(const DMCConfig* conf);

  int alloc_segment(KVOpsCtx* ctx);
  int connect_all_rc_qp();
  int init_eviction(const DMCConfig* conf);

  // common op
  void search_bucket_read_kv(KVOpsCtx* ctx);

  // kv_set
  int kv_set_1s(void* key, uint32_t key_size, void* val, uint32_t val_size);
  void kv_set_search_bucket(KVOpsCtx* ctx);  // iteratively read kv using rdma read and compare key
  uint8_t kv_set_alloc_rblock(KVOpsCtx* ctx);
  void kv_set_read_index_write_kv(KVOpsCtx* ctx);
  int kv_set_update_index(KVOpsCtx* ctx);
  int kv_insert_empty_slot(KVOpsCtx* ctx);
  int kv_insert_evict_bucket(KVOpsCtx* ctx);
  void prepare_lazy_evict(KVOpsCtx* ctx);
  void kv_set_delete_duplicate(KVOpsCtx* ctx);
  void prepare_set_twim_entry(KVOpsCtx* ctx, AtomicEntry* twim_entry);
  void check_mm(KVOpsCtx* ctx);
  void read_index(KVOpsCtx* ctx);

  // kv_get
  int kv_get_1s(void* key, uint32_t key_size, __OUT void* val, __OUT uint32_t* val_size);
  int compare_key(uint8_t slot_idx, KVOpsCtx* ctx);
  void kv_get_copy_value(KVOpsCtx* ctx,
                         __OUT void* val,
                         __OUT uint32_t* val_len);

  // sync fifo
  void switch_fifo_type(uint8_t fifo_type);
  uint8_t decide_fifo_type();
  void fetch_fifo_tail(KVOpsCtx* ctx);
  void add_fifo_tail(KVOpsCtx* ctx);
  void flush_fifo_head();
  void poll_and_fetch_flush_fifo(KVOpsCtx* ctx, uint64_t clk);

  // evict
  void seal_reinsert(KVOpsCtx* ctx, uint8_t fifo_type);
  void sample(KVOpsCtx* ctx);
  void evict_group(KVOpsCtx* ctx);
  void eg_sort_kv(KVOpsCtx* ctx);
  void eg_lock_read_kv(KVOpsCtx* ctx);
  void eg_free_group(KVOpsCtx* ctx);
  void eg_merge_update_kv(KVOpsCtx* ctx);
  void prepare_seal_reinsert(uint64_t write_buf_laddr, uint32_t clk_tail_fid);
  void prepare_sample_head(uint64_t fnode_buf_laddr, uint32_t clk_head_fid);
  uint32_t prepare_sample_group(uint32_t clk_head_fid, uint64_t fnode_buf_laddr,
                                uint64_t gnode_buf_laddr, uint64_t reinsert_buf_laddr);
  void poll_and_clear_freq_map(uint64_t write_buf_laddr);

  // others
  void create_op_ctx(void* key, uint32_t key_size, void* val, uint32_t val_size,
                     uint8_t op_type, __OUT KVOpsCtx* ctx);
  void get_n_unused_entries(KVOpsCtx* ctx, uint32_t n, __OUT uint32_t* candidates);
  std::string get_str_key(KVOpsCtx* ctx);

  inline uint64_t get_bucket_raddr(uint64_t hash) { return server_htable_addr_ + (hash % HASH_NUM_BUCKETS) * sizeof(Bucket); }
  inline uint64_t get_slot_raddr(const KVOpsCtx* ctx, uint64_t slot_laddr) { return ctx->bucket_raddr + slot_laddr - ctx->bucket_laddr; }
  inline uint64_t get_gnode_raddr(uint32_t gid) { return server_gmeta_addr_ + get_gnode_id(gid) * sizeof(GroupNode); }
  inline uint64_t get_twim_entry_raddr(uint32_t gid, uint16_t sn) { return get_gnode_raddr(gid) + GNODE_TWIM_OFF + sn * sizeof(AtomicEntry); }
  inline uint32_t get_num_rear_fnode(uint32_t fid) { return NUM_FIFO_NODE - get_fnode_id(fid); }
  inline uint64_t get_fnode_raddr(uint32_t fid) { return local_fifo_->fifo_raddr + get_fnode_id(fid) * sizeof(AtomicEntry); }
  inline uint64_t get_fwnode_raddr(uint32_t fid) { return local_fifo_->fwindow_raddr + get_fwnode_id(fid) * MAX_GROUP_SIZE; }
  inline uint64_t get_clock_raddr() { return local_fifo_->clock_raddr; }

 public:
  uint32_t n_set_miss_;
  void init_counters();
  DMCClient(const DMCConfig* conf);
  ~DMCClient();
  int kv_get(void* key,
             uint32_t key_size,
             __OUT void* val,
             __OUT uint32_t* val_size);
  int kv_set(void* key, uint32_t key_size, void* val, uint32_t val_size);

  uint64_t get_mm_free_size();

  bool get_oom() {
    for (uint32_t i = 0; i < num_servers_; i++) {
      if (server_oom_[i])
        return true;
    }
    return false;
  }

};

#endif

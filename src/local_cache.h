#ifndef _DMC_LOCAL_CACHE_H_
#define _DMC_LOCAL_CACHE_H_

#include <stdint.h>

#include <algorithm>
#include <atomic>
#include <queue>
#include <set>
#include <unordered_map>

#include "client_ctx.h"
#include "dmc_table.h"
#include "dmc_utils.h"

#define LC_NUM_BUCKETS (HASH_NUM_BUCKETS / 8)

enum FNODE_AGE {
  FNODE_DEAD = 0,  // no need to maintain freq_map
  FNODE_DYING = 1,
  FNODE_ALIVE = 2,
};

typedef struct _LocalCacheBucket {
  std::atomic<uint64_t> slots[HASH_BUCKET_ASSOC_NUM];
  uint8_t blocks[HASH_BUCKET_ASSOC_NUM][128];
} LocalCacheBucket;

typedef struct _LocalEvictSlot {
  int freq;
  int slot_id;
  uint64_t old_data;
} LocalEvictSlot;

// forward declaration
class LocalCache;

struct LocalFifo {
 public:
  LocalCache* p_lc;
  uint32_t clk_fifo_head;
  uint32_t clk_fifo_tail;
  uint32_t clk_fifo_len;
  uint32_t clk_head_alarm;
  uint32_t min_alarm_len;
  uint32_t lc_fifo_head;
  uint32_t lc_fifo_tail;
  uint64_t fifo_raddr;
  uint64_t fwindow_raddr;
  uint64_t clock_raddr;
  uint32_t lc_fifo_buffer[NUM_FIFO_NODE];

  void init(uint8_t fifo_type, LocalCache* lc, const DMCConfig* conf);
  void update_clock(const uint64_t clock);
  void push_fifo_tail(const AtomicEntry* fnode);
  void pop_fifo_head(bool flush, __OUT AssistCtx* ctx);
  void sync_fifo_tail(const AtomicEntry* fnode, const uint32_t num_sync_tail);
  void sync_fifo_head(__OUT AssistCtx* ctx);
  uint8_t get_fid_age(const uint32_t fid);
  uint32_t fid_wrap_index(uint32_t fid) { return fid % NUM_FIFO_NODE; }
  int64_t get_fifo_rlen() { return clk_fifo_tail - clk_fifo_head; }
};

class LocalCache {
 private:
  std::atomic<int64_t> free_size_;
  LocalCacheBucket local_cache_table_[LC_NUM_BUCKETS];

  void sample_bucket(LocalCacheBucket* bucket, uint8_t base_slot_id,
                     __OUT LocalEvictSlot* prio_slot_id,
                     __OUT uint32_t* prio_slot_id_size);
  uint32_t evict_bucket(uint64_t key_hash, uint32_t sample_bucket_num, uint32_t bytes);

 public:
  LocalGroupNode local_gnode_map[MAX_GROUP_NUM];
  LocalFifo local_fifo[NUM_FIFO_TYPE];

  LocalCache(const DMCConfig* conf);
  bool is_cached_gid(uint32_t gid);
  bool is_cached_gnode(uint32_t gnode_id);
  void remove_from_cache(uint32_t gid);
  void remove_from_cache(LocalGroupNode* gnode);
  uint8_t get_gid_age(uint32_t gid);
  int get_obj_freq(uint32_t gid, uint16_t sn);
  void update_metrics(uint32_t gid, uint16_t sn);
  bool local_get(const Slot* slot, KVOpsCtx* ctx);
  void local_set(const KVOpsCtx* ctx);
  bool need_evict_();
  void* get_freq_buf_() { return (void*)local_cache_table_; }
  uint32_t get_freq_buf_size() { return sizeof(LocalCacheBucket) * LC_NUM_BUCKETS; }
};



#endif

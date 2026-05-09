#ifndef _DMC_CLIENT_CTX_H_
#define _DMC_CLIENT_CTX_H_

#include "dmc_table.h"

enum KVOP { SET,
            GET };

enum DMC_RET_CODE { DMC_SUCCESS = 0,
                    DMC_SET_RETRY };

enum COMPARE_KEY { KEY_SILENT_EVICTED = 0,
                   KEY_MATCH,
                   KEY_NOT_MATCH };

typedef struct _Group {
  uint64_t addr;
  uint32_t gid;
  uint16_t old_size;
  uint8_t segment;
  uint8_t server;
  AtomicEntry* twim;
  AtomicEntry* evt_twim;
  uint16_t* evt_loff_map;
} Group;

typedef struct _RemoteBlock {
  uint64_t addr;
  uint16_t size;
  uint32_t gid;
  uint16_t sn;
  uint16_t st_evt_sn;
  uint16_t num_evt_sn;
  Group* group;
} RemoteBlock;

typedef struct _KVOpsCtx {
  void* key;
  void* val;
  uint32_t key_size;
  uint32_t val_size;
  uint32_t kv_size;

  // for indicating return value
  uint8_t ret;
  uint8_t op_type;
  uint8_t alloc_type;

  // hash value
  uint8_t fp;
  uint16_t ext_fp;
  uint64_t key_hash;

  // for reading index
  uint64_t bucket_laddr;
  uint64_t bucket_raddr;

  // for writing data
  RemoteBlock kv_remote_block;
  uint64_t write_buf_laddr;

  // for arbitrary operations
  uint64_t op_laddr;

  // for reading data
  uint64_t read_buf_laddr;
  uint32_t num_empty_slot;
  uint8_t empty_slot_id[HASH_BUCKET_ASSOC_NUM];
  bool key_found;
  bool local_hit;  // for local cache

  // for committing object rblock
  bool commit_rblock;

  // for modify index
  uint64_t target_slot_laddr;
  uint64_t target_slot_raddr;
  uint64_t target_block_laddr;
  uint64_t target_block_raddr;

  // for check duplicated keys
  bool need_check_dup;
} KVOpsCtx;

// for group-level eviction
typedef struct _KVMeta {
  uint8_t freq;
  uint8_t gidx;
  uint16_t sn;
  uint16_t st_loff;
  uint16_t score;
} KVMeta;

typedef struct _LcOp {
  uint32_t gid;
  uint16_t sn;
  uint8_t len;
  uint8_t epoch;
} LcOp;

typedef struct _AssistCtx {
  static constexpr uint16_t EVT_BATCH_SIZE = MAX_GROUP_SIZE / 4;
  static constexpr uint16_t MAX_ASSIST_GNODE_NUM = 16 * DEFAULT_MAX_GROUP_SIZE / MAX_GROUP_SIZE;
  static constexpr uint16_t FREQ_SAMPLE_RBUF_SIZE = 1024;
  static constexpr uint16_t MAX_FREQ_SAMPLE_NUM = 512;

  // for group-level eviction
  Group merge_group;
  uint64_t merge_buf_laddr;
  uint64_t merge_buf_raddr;
  AtomicEntry* merge_kv_slot;
  uint32_t merge_kv_bytes;
  uint32_t merge_kv_freq;
  uint16_t merge_kv_num;
  uint32_t num_sample_kv;
  KVMeta* sample_kv_meta;

  // for shared memory management
  uint32_t old_gpool_head;  // for seal
  bool need_evict;
  uint8_t num_sample;  // sampled groups in ctx->read_buf_laddr
  uint8_t num_seal;
  uint8_t num_reinsert;
  uint8_t num_freq_map;
  uint64_t freq_map_raddr[MAX_SAMPLE_NUM];

  // for deciding segment
  uint32_t freq_sample_head;
  uint32_t freq_sample_tail;
  uint32_t freq_sample_sum;
  uint32_t freq_samples[FREQ_SAMPLE_RBUF_SIZE];
  uint16_t freq_wrap_index(uint32_t idx) { return idx % FREQ_SAMPLE_RBUF_SIZE; }

  // for local cache operations
  uint32_t num_sync_tail;
  uint8_t num_flush;
  bool need_sync_head;
  bool need_sync_tail;
  std::pair<uint32_t, LocalGroupNode*> gnodes_to_flush[MAX_ASSIST_GNODE_NUM];

  uint8_t get_segment() {
    static constexpr uint8_t BASE = 2;
    if (merge_kv_freq == 0) {
      return 0;
    }

    if (freq_sample_tail - freq_sample_head >= MAX_FREQ_SAMPLE_NUM)
      freq_sample_sum -= freq_samples[freq_wrap_index(freq_sample_head++)];

    freq_samples[freq_wrap_index(freq_sample_tail++)] = merge_kv_freq;
    freq_sample_sum += merge_kv_freq;

    uint32_t mean_freq = freq_sample_sum / (freq_sample_tail - freq_sample_head);
    if (merge_kv_freq > mean_freq * 2) {
      return BASE + 1;
    } else if (merge_kv_freq < mean_freq / 2) {
      return BASE - 1;
    }
    return BASE;
  }
} AssistCtx;
#endif  // _DMC_CLIENT_CTX_H_

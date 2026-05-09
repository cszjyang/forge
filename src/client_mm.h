#ifndef _DMC_CLIENT_MM_H_
#define _DMC_CLIENT_MM_H_

#include <dmc_table.h>
#include <dmc_utils.h>

#include <atomic>
#include <map>
#include <memory>
#include <queue>
#include <set>
#include <vector>

#include "client_ctx.h"
#include "local_cache.h"

enum AllocResult {
  ALLOC_FAIL = 0,
  ALLOC_SHARED_SEAL = 1,
  ALLOC_SHARED = 2,
  ALLOC_EXCLUSIVE = 3,
};

typedef struct _RemoteSegment {
  uint64_t addr;
  uint16_t server;
} RemoteSegment;

class ClientSharedMM {
  friend class DMCClient;

 private:
  static constexpr uint32_t LOW_WATERMARK = 4 * DEFAULT_MAX_GROUP_SIZE / MAX_GROUP_SIZE / NUM_MN;
  static constexpr uint32_t GROUP_POOL_SIZE = LOW_WATERMARK * 8;
  static constexpr float SEAL_WATERMARK = 0.1;

  std::queue<uint32_t> free_gid_queue;
  std::queue<Group> free_group_queue;
  std::atomic<uint64_t> head_tail_;
  uint32_t segment_size_;

  void init_free_gid_queue(const DMCConfig* conf);
  void init_free_group_queue(const DMCConfig* conf);

  bool is_pool_full(uint32_t head, uint32_t tail) { return tail - head >= GROUP_POOL_SIZE / 2; }  // reserve half as seal buffer
  bool is_pool_low(uint32_t head, uint32_t tail) { return tail - head <= LOW_WATERMARK; }

 public:
  ClientSharedMM(const DMCConfig* conf);
  ~ClientSharedMM();

  std::atomic<uint64_t> galloc_pool_[GROUP_POOL_SIZE];
  Group group_pool_[GROUP_POOL_SIZE];
  Allocator<TWIM>* twim_allocator_;
  Allocator<LoffMap>* loff_map_allocator_;

  // for group & twim pool
  uint32_t wrap_index(uint32_t index) { return index % GROUP_POOL_SIZE; }

  void add_segment(uint64_t raddr, uint16_t server);
  void free_group(SampleGroupNode* gnode);
  void add_group(uint32_t tail, AssistCtx* assist_ctx = NULL);
  int alloc(uint32_t size, __OUT RemoteBlock* rblock);

  void fill_and_seal(AssistCtx* assist_ctx_if_seal = NULL);
};

// **************************************** ClientExclusiveMM ****************************************
struct CompareSizeIncr {
  bool operator()(const RemoteBlock& lhs, const RemoteBlock& rhs) const { return lhs.size < rhs.size; }
};

class ClientExclusiveMM {
  LocalCache* lc_;
  std::set<RemoteBlock, CompareSizeIncr> free_blocks_by_size_;

 public:
  ClientExclusiveMM(LocalCache* lc);
  ~ClientExclusiveMM();

  int alloc(uint32_t size, __OUT RemoteBlock* r_block);
  int free(const RemoteBlock* r_block);
  uint32_t get_max_block_size();
  uint32_t get_min_block_size();
  uint64_t get_free_size();
};

#endif

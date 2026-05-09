#ifndef _DMC_SERVER_MM_H_
#define _DMC_SERVER_MM_H_

#include <assert.h>
#include <infiniband/verbs.h>
#include <stdint.h>

#include <map>
#include <queue>

#include "dmc_utils.h"

typedef struct _SegmentInfo {
  uint64_t addr;
  uint16_t allocated_to;
} SegmentInfo;

class ServerMM {
private:
  uint64_t base_addr_;
  uint64_t base_len_;
  uint64_t index_area_addr_; // object index, e.g., hash table
  uint64_t index_area_len_;
  uint64_t gnode_area_addr_; // group metadata array
  uint64_t gnode_area_len_;
  uint64_t fifo_buffer_addr_; // FIFO queue
  uint64_t fifo_buffer_len_;
  uint64_t free_space_addr_; // object data region
  uint64_t free_space_len_;
  uint64_t server_data_len_;

  uint32_t segment_size_;
  uint32_t num_segments_;
  std::queue<SegmentInfo> free_segment_list_;
  std::map<uint64_t, SegmentInfo> used_segment_map_;

  struct ibv_mr *mr_;
  void *data_;
  struct ibv_mr *dm_mr_;
  struct ibv_dm *dm_;

  uint32_t num_reserved_segments_;

public:
  ServerMM(const DMCConfig *conf, struct ibv_context *ctx, struct ibv_pd *pd);
  ~ServerMM();

  int get_mr_info(__OUT MrInfo *mr_info);
  int get_dm_mr_info(__OUT MrInfo *dm_mr_info);
  uint32_t get_mr_lkey();
  uint32_t get_mr_rkey();
  inline uint64_t get_base_addr() { return base_addr_; }
  inline uint64_t get_index_area_addr() { return index_area_addr_; }
  inline uint32_t get_index_area_len() { return index_area_len_; }
  inline uint64_t get_gnode_area_addr() { return gnode_area_addr_; }
  inline uint32_t get_gnode_area_len() { return gnode_area_len_; }
  inline uint64_t get_fifo_buffer_addr() { return fifo_buffer_addr_; }
  inline uint32_t get_fifo_buffer_len() { return fifo_buffer_len_; }
  inline float get_usage() {
    return 1 - ((float)free_segment_list_.size() / num_segments_);
  }
  inline uint64_t get_free_addr() { return free_space_addr_; }

  int alloc_segment(uint16_t sid, __OUT SegmentInfo *seg_info);
  int free_segment(uint64_t seg_addr);

  // for testing
  bool check_num_segments();

  inline uint32_t get_rkey() { return mr_->rkey; }
};

#endif
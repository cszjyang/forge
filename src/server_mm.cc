#include "server_mm.h"

#include <assert.h>
#include <infiniband/verbs.h>
#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>

#include "debug.h"
#include "dmc_table.h"
#include "dmc_utils.h"

#define MAP_HUGE_2MB (21 << MAP_HUGE_SHIFT)
#define MAP_HUGE_1GB (30 << MAP_HUGE_SHIFT)

// One-sided Memory Region in Server
ServerMM::ServerMM(const DMCConfig *conf, struct ibv_context *ctx,
                   struct ibv_pd *pd) {
  segment_size_ = conf->segment_size;
  base_addr_ = conf->server_base_addr;

  index_area_len_ = HASH_SPACE_BYTES;
  gnode_area_len_ = GNODE_SPACE_BYTES;
  fifo_buffer_len_ = FIFO_SPACE_BYTES * NUM_FIFO_TYPE;
  server_data_len_ = ROUNDUP(conf->server_data_len / NUM_MN, segment_size_);
  base_len_ =
      index_area_len_ + gnode_area_len_ + fifo_buffer_len_ + server_data_len_;
  assert(base_len_ > server_data_len_);

  // allocate data area
  int prot_flag = PROT_READ | PROT_WRITE;
  int mm_flag =
      MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED | MAP_HUGETLB | MAP_HUGE_2MB;
  data_ = mmap((void *)base_addr_, base_len_, prot_flag, mm_flag, -1, 0);
  if ((uint64_t)data_ != base_addr_) {
    printd(L_ERROR, "mapping %ld to 0x%lx failed", base_len_, base_addr_);
    exit(-1);
  }

  // | index area: HASH_SPACE_BYTES | global group metadata array area:
  // GNODE_SPACE_BYTES | FIFO buffer area: FIFO_SPACE_BYTES | free segment area:
  // free_space_len_ |
  index_area_addr_ = base_addr_;
  gnode_area_addr_ = index_area_addr_ + index_area_len_;
  fifo_buffer_addr_ = gnode_area_addr_ + gnode_area_len_;
  free_space_addr_ = fifo_buffer_addr_ + fifo_buffer_len_;
  free_space_len_ = server_data_len_;
  // printf("cache size: %ld GB\n", free_space_len_ / (1024 * 1024 * 1024));

  printd(L_INFO,
         "ServerMM initialized with parameters:\n"
         "\tsegment_size: %d, base_addr: 0x%llx, base_len: 0x%llx\n"
         "\tindex_area_addr: [0x%llx, 0x%llx), index_area_len: 0x%llx\n"
         "\tgnode_area_addr: [0x%llx, 0x%llx), gnode_area_len: 0x%llx\n"
         "\tfifo_buffer_addr: [0x%llx, 0x%llx), fifo_buffer_len: 0x%llx\n"
         "\tfree_space_addr: [0x%llx, 0x%llx), free_space_len (cache size): "
         "0x%llx",
         segment_size_, base_addr_, base_len_, index_area_addr_,
         index_area_addr_ + index_area_len_, index_area_len_, gnode_area_addr_,
         gnode_area_addr_ + gnode_area_len_, gnode_area_len_, fifo_buffer_addr_,
         fifo_buffer_addr_ + fifo_buffer_len_, fifo_buffer_len_,
         free_space_addr_, free_space_addr_ + free_space_len_, free_space_len_);

  // register memory region
  int access_flag = IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ |
                    IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_REMOTE_ATOMIC;
  mr_ = ibv_reg_mr(pd, data_, base_len_, access_flag);
  assert(mr_ != NULL);
  printd(L_INFO, "Registered MR: %p rkey %x\n", mr_->addr, mr_->rkey);

  // init free_segment_list
  free_segment_list_ = std::queue<SegmentInfo>();
  used_segment_map_.clear();
  num_segments_ = free_space_len_ / segment_size_;

#if PREFILL_MM
  printd(L_INFO, "%u segments to pre-fill", num_segments_);
#else
  // num_segments_ = 500;
  for (uint64_t i = 0; i < num_segments_; i++) {
    uint64_t segment_addr = free_space_addr_ + i * segment_size_;
    SegmentInfo info;
    info.addr = segment_addr;
    info.allocated_to = -1;
    free_segment_list_.push(info);
  }

  num_reserved_segments_ = 0;

  printd(L_INFO, "%ld segments in total", free_segment_list_.size());
#endif

  /* Device memory allocation request */
  struct ibv_alloc_dm_attr dm_attr = {
      .length = RNIC_DM_BYTES, .log_align_req = 3, .comp_mask = 0};

  dm_ = ibv_alloc_dm(ctx, &dm_attr);
  if (dm_ == NULL) {
    printd(L_ERROR, "Allocate on-chip memory failed");
    abort();
  }

  /* Device memory registration as memory region */
  dm_mr_ = ibv_reg_dm_mr(pd, dm_, 0, RNIC_DM_BYTES,
                         access_flag | IBV_ACCESS_ZERO_BASED);
  if (dm_mr_ == NULL) {
    printd(L_ERROR, "Register on-chip memory failed");
    abort();
  }

  // init zero
  char *buffer = (char *)malloc(RNIC_DM_BYTES);
  memset(buffer, 0, RNIC_DM_BYTES);
  if (ibv_memcpy_to_dm(dm_, 0, buffer, RNIC_DM_BYTES) != 0) {
    printd(L_ERROR, "Failed to initialize on-chip memory");
    abort();
  }
  free(buffer);

  printd(L_INFO, "Registered DM: %lx rkey %x\n", dm_mr_->addr, dm_mr_->rkey);
}

ServerMM::~ServerMM() {
  ibv_dereg_mr(mr_);
  munmap(data_, base_len_);

  ibv_dereg_mr(dm_mr_);
  ibv_free_dm(dm_);
}

int ServerMM::get_mr_info(__OUT MrInfo *mr_info) {
  mr_info->addr = (uint64_t)mr_->addr;
  mr_info->rkey = mr_->rkey;
  return 0;
}

int ServerMM::get_dm_mr_info(__OUT MrInfo *dm_mr_info) {
  dm_mr_info->addr = (uint64_t)dm_mr_->addr;
  dm_mr_info->rkey = dm_mr_->rkey;
  return 0;
}

uint32_t ServerMM::get_mr_lkey() { return mr_->lkey; }

uint32_t ServerMM::get_mr_rkey() { return mr_->rkey; }

int ServerMM::alloc_segment(uint16_t sid, __OUT SegmentInfo *_seg_info) {
  if (free_segment_list_.size() == num_reserved_segments_) {
    memset(_seg_info, 0, sizeof(SegmentInfo));
    return -1;
  }
  SegmentInfo seg_info = free_segment_list_.front();
  seg_info.allocated_to = sid;
  free_segment_list_.pop();
  used_segment_map_[seg_info.addr] = seg_info;
  memcpy(_seg_info, &seg_info, sizeof(SegmentInfo));
  printd(L_INFO, "allocated segment @0x%lx to client %d", seg_info.addr, sid);
  printd(L_INFO, "free segments: %ld", free_segment_list_.size());
  return 0;
}

int ServerMM::free_segment(uint64_t seg_addr) {
  std::map<uint64_t, SegmentInfo>::iterator it =
      used_segment_map_.find(seg_addr);
  if (it == used_segment_map_.end()) {
    printd(L_ERROR, "Freeing invalid segment %lu", seg_addr);
    return -1;
  }
  SegmentInfo seg_info = it->second;
  seg_info.allocated_to = -1;
  free_segment_list_.push(seg_info);
  used_segment_map_.erase(it);
  return 0;
}

bool ServerMM::check_num_segments() {
  uint64_t num_free = free_segment_list_.size();
  uint64_t num_used = used_segment_map_.size();
  return (num_free + num_used) == num_segments_;
}
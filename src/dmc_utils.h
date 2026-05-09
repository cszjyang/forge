#ifndef _DMC_UTILS_H_
#define _DMC_UTILS_H_

#include <stdint.h>
#include <stdlib.h>
#include <sys/time.h>

#include "debug.h"

#define __OUT
#define __IN

const int reserved_segment_list[] = {24512, 18336, 12224, 6112, 0};  // list[0] is total allocated memory

enum Role {
  SERVER,
  CLIENT,
};

enum ConnType {
  IB,
  ROCE,
};

enum UDPMsgType {
  UDPMSG_REQ_CONNECT,
  UDPMSG_REP_CONNECT,
  UDPMSG_REQ_ALLOC,
  UDPMSG_REP_ALLOC,
  UDPMSG_REQ_TS,
  UDPMSG_REP_TS,
};

enum IBMsgType {
  IBMSG_REQ_ALLOC,
  IBMSG_REP_ALLOC,
  IBMSG_REQ_RETRAIN,
  IBMSG_REP_RETRAIN
};

enum HashType {
  HASH_DEFAULT,
  HASH_DUMB,
};

enum EvictionType {
  GROUP_LEVEL_EVICTION
};

typedef struct _DMCConfig {
  // global server info
  uint8_t role;
  uint8_t conn_type;
  uint32_t server_id;
  uint16_t udp_port;
  uint32_t memory_num;
  char memory_ip_list[16][16];

  // ib device info
  uint32_t ib_dev_id;
  uint32_t ib_port_id;
  int32_t ib_gid_idx;

  // hash info
  uint8_t hash_type;

  // group-level eviction
  uint8_t eviction_type;
  uint32_t num_group_samples;

  // client node
  uint32_t st_client_id;
  uint32_t client_id;
  uint32_t client_local_size;
  uint32_t lc_size_per_client;
  uint32_t num_local_clients;
  uint32_t num_global_clients;

  // memory node
  uint64_t server_base_addr;
  uint64_t server_data_len;
  uint64_t segment_size;
  uint64_t block_size;

  uint32_t core_id;
  uint32_t num_cores;

  // testing
  bool testing;

  // more cliquemap threads
  uint32_t num_server_threads;

} DMCConfig;

typedef struct _GroupNodeEntry {
  uint32_t gnode_id;
  uint32_t gid;
} GroupNodeEntry;

typedef struct _MrInfo {
  uint64_t addr;
  uint32_t rkey;
} MrInfo;

typedef struct _QPInfo {
  uint32_t qp_num;
  uint16_t lid;
  uint8_t port_num;
  uint8_t gid[16];
  uint8_t gid_idx;
} QPInfo;

typedef struct _ConnInfo {
  // qp info
  QPInfo qp_info;
  // initial mr info
  MrInfo mr_info;
  MrInfo dm_mr_info;
} ConnInfo;

typedef struct _UDPMsg {
  uint16_t type;
  uint16_t id;
  union {
    ConnInfo conn_info;
    MrInfo mr_info;
    uint64_t sys_start_ts;
  } body;
} UDPMsg;

static inline uint64_t new_ts() {
  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  return ts.tv_sec * 1000000 + ts.tv_nsec / 1000;  // us
}

static inline uint64_t diff_ts_us(const struct timeval* et,
                                  const struct timeval* st) {
  return (et->tv_sec - st->tv_sec) * 1000000 + (et->tv_usec - st->tv_usec);
}

int load_config(const char* fname, __OUT DMCConfig* config);
// for udpmsg
void serialize_qp_info(__OUT QPInfo* qp_info);
void deserialize_qp_info(__OUT QPInfo* qp_info);
void serialize_mr_info(__OUT MrInfo* mr_info);
void deserialize_mr_info(__OUT MrInfo* mr_info);
void serialize_conn_info(__OUT ConnInfo* conn_info);
void deserialize_conn_info(__OUT ConnInfo* conn_info);
void serialize_udpmsg(__OUT UDPMsg* msg);
void deserialize_udpmsg(__OUT UDPMsg* msg);

int stick_this_thread_to_core(int core_id);

void set_sys_start_ts(uint64_t ts);

template <typename T>
class Allocator {
 private:
  T* pool_;
  uint32_t size_;
  T** free_list_;
  uint32_t free_list_size_;

 public:
  Allocator(uint32_t size) {
    size_ = size;
    pool_ = new T[size];
    free_list_ = new T*[size];
    free_list_size_ = size;

    memset(pool_, 0, sizeof(T) * size);
    for (uint32_t i = 0; i < size; i++) {
      free_list_[i] = &pool_[i];
    }
  }
  ~Allocator() {
    delete[] pool_;
    delete[] free_list_;
  }
  T* alloc(bool init) {
    if (free_list_size_ == 0) {
      printd(L_ERROR, "Allocator is full");
      abort();
    }
    T* ptr = free_list_[--free_list_size_];
    if (init) {
      memset(ptr, 0, sizeof(T));
    }
    return ptr;
  }
  void free(T* ptr) {
    free_list_[free_list_size_++] = ptr;
  }

  void* get_buf_addr() { return (void*)pool_; }
  uint32_t get_buf_size() { return size_ * sizeof(T); }
  uint32_t get_used_bytes() { return (size_ - free_list_size_) * sizeof(T); }
};

#endif
#ifndef _DMC_EXP_RUN_CLIENT_H_
#define _DMC_EXP_RUN_CLIENT_H_

#include <pthread.h>

#include "client.h"
#include "init.h"
#include "third_party/json.hpp"

using json = nlohmann::json;

#define TICK_US (500000)
#define TICK_PER_SEC (1000000 / 500000)

typedef struct _ClientArgs {
  DMCClient* client;
  uint32_t core;
  uint32_t cid;
  uint32_t num_global_clients;

  // workload args
  char workload_name[128];
  uint32_t num_load_ops;
  bool validate;

  // return values
  uint32_t num_success_ops;
  uint32_t num_failed_ops;

  int lat_list_len;

  // macro benchmark
  uint32_t num_ops;
  uint32_t run_tims_s;

  // miss rate test
  bool use_warmup;

  // controller ip
  char controller_ip[64];
} ClientArgs;

struct PerformanceTracker {
  // timer
  uint32_t tick = 0;
  uint32_t num_ticks;
  uint32_t max_req_num = 0;

  // Sequence and operation counters
  uint32_t seq = 0;
  uint32_t n_miss = 0;
  uint32_t n_op[2] = {0, 0};

  // Vectors for tracking metrics over time
  std::vector<uint32_t> ops_vec;
  std::vector<uint32_t> n_miss_vec;
  std::vector<uint64_t> num_evict_vec;
  std::vector<uint64_t> succ_evict_vec;
  std::vector<uint64_t> num_evict_bucket_vec;
  std::vector<uint64_t> succ_evict_bucket_vec;

  // Map for latency distribution
  std::map<uint32_t, uint32_t> lat_map;

  // Timing variables
  struct timeval start_time, op_start_time, op_end_time;

  // Initialize and start timing
  void init(uint32_t run_time_seconds, uint32_t max_req_num_) {
    clear_counters();
    gettimeofday(&start_time, NULL);
    num_ticks = run_time_seconds * TICK_PER_SEC;
    max_req_num = max_req_num_;
  }

  void clear_counters() {
    tick = 0;
    seq = 0;
    n_op[0] = 0;
    n_op[1] = 0;
    n_miss = 0;

    ops_vec.clear();
    n_miss_vec.clear();
    num_evict_vec.clear();
    succ_evict_vec.clear();
    num_evict_bucket_vec.clear();
    succ_evict_bucket_vec.clear();
    lat_map.clear();
  }

  // Start operation timing
  void start_op_timer() {
    gettimeofday(&op_start_time, NULL);
  }

  // End operation timing and record latency
  void end_op_timer(uint8_t op, bool is_miss, DMCClient* client) {
    gettimeofday(&op_end_time, NULL);
    uint64_t lat = diff_ts_us(&op_end_time, &op_start_time);
    lat_map[lat]++;
    seq++;
    n_op[op]++;
    n_miss += is_miss;

    uint64_t elapsed = diff_ts_us(&op_end_time, &start_time);
    if (elapsed > TICK_US * (tick + 1)) {
      ops_vec.push_back(seq);
      num_evict_bucket_vec.push_back(client->num_bucket_evict_);
      succ_evict_bucket_vec.push_back(client->num_success_bucket_evict_);
      num_evict_vec.push_back(client->num_evict_);
      succ_evict_vec.push_back(client->num_success_evict_);
      n_miss_vec.push_back(n_miss);
      tick++;
    }
  }

  bool need_stop() {
    return (max_req_num == 0 && tick >= num_ticks) || (max_req_num > 0 && seq >= max_req_num);
  }

  // Generate results as JSON
  json get_results(DMCClient* client) {
    json res;
    res["n_ops_cont"] = json(ops_vec);
    res["n_evict_cont"] = json(num_evict_vec);
    res["n_succ_evict_cont"] = json(succ_evict_vec);
    res["n_evict_bucket_cont"] = json(num_evict_bucket_vec);
    res["n_succ_evict_bucket_cont"] = json(succ_evict_bucket_vec);
    res["n_miss_cont"] = json(n_miss_vec);
    res["n_set"] = n_op[SET];
    res["n_get"] = n_op[GET];
    res["n_retry"] = client->num_set_retry_;
    res["n_rdma_send"] = client->num_rdma_send_;
    res["n_rdma_recv"] = client->num_rdma_recv_;
    res["n_rdma_read"] = client->num_rdma_read_;
    res["n_rdma_write"] = client->num_rdma_write_;
    res["n_rdma_cas"] = client->num_rdma_cas_;
    res["n_rdma_faa"] = client->num_rdma_faa_;
    res["lat_map"] = json(lat_map);

    return res;
  }

  // Print summary statistics
  void print_summary(int client_id, DMCClient* client) {
    printf("Client %d finish, n_get: %d, n_set: %d, flush: %lu, checksum failed: %lu, num_merge/num_evict: %lu/%lu = %f\n",
           client_id, n_op[GET], n_op[SET], client->num_real_flush_, client->num_read_checksum_failed_,
           client->num_merge_obj_, client->num_success_evict_,
           (float)client->num_merge_obj_ / client->num_success_evict_);
  }
};

void run_client(const InitArgs* args);

#endif

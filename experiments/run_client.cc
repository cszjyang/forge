#include "run_client.h"

#include <assert.h>
#include <sys/time.h>
#include <unistd.h>

#include "memcached.h"
#include "third_party/json.hpp"
#include "workload.h"

void setup_client_logger(const char* workload_name, int cid) {
#ifdef _DEBUG
  std::string output_file = std::string(get_current_dir_name()) + "/../outputs/" + workload_name + "_c" + std::to_string(cid) + ".log";
  printd(L_INFO, "Redirecting logs to %s", output_file.c_str());
  open_thread_log(output_file.c_str());
#endif
}

void shutdown_client_logger() {
#ifdef _DEBUG
  close_thread_log();
#endif
}

static void get_workload_kv(DMCWorkload* wl,
                            uint32_t idx,
                            __OUT uint64_t* key_addr,
                            __OUT uint64_t* val_addr,
                            __OUT uint32_t* key_size,
                            __OUT uint32_t* val_size,
                            __OUT uint8_t* op) {
  if (wl->max_key_size == 0) {
    printd(L_ERROR, "max_key_size is unset, workload not loaded properly");
    exit(-1);
  }
  idx = idx % wl->num_ops;
  *key_addr = ((uint64_t)wl->key_buf + idx * wl->max_key_size);
  *val_addr = (uint64_t)wl->val_buf;
  *key_size = wl->key_size_list[idx];
  *val_size = wl->val_size_list[idx];
  *op = wl->op_list[idx];

  printd(L_DEBUG, "idx: %d, num_ops: %d, key: %s, val: %s, key_size: %d, val_size: %d, op: %d",
         idx, wl->num_ops, (char*)*key_addr, (char*)*val_addr, *key_size,
         *val_size, *op);
}

void run_workload(DMCClient* client, DMCWorkload* wl, PerformanceTracker* tracker) {
  int ret;
  char tmp_buf[65536];
  uint32_t tmp_len;
  uint64_t key_addr, val_addr;
  uint32_t key_size, val_size;
  uint8_t op;
  while (!tracker->need_stop()) {
    get_workload_kv(wl, tracker->seq % wl->num_ops, &key_addr, &val_addr, &key_size, &val_size, &op);
    tracker->start_op_timer();
    if (op == GET) {
      ret = client->kv_get((void*)key_addr, key_size, tmp_buf, &tmp_len);
      if (ret < 0) {
        if (PENALTY_US > 0)
          usleep(PENALTY_US);
        client->kv_set((void*)key_addr, key_size, (void*)val_addr, val_size);
      }
    } else {
      ret = client->kv_set((void*)key_addr, key_size, (void*)val_addr, val_size);
    }
    tracker->end_op_timer(op, ret < 0, client);
  }
}

enum WL_PHASE {
  WARMUP,
  TRANS,
};

std::pair<uint32_t, float> get_time_or_ratio(const char* workload_name, WL_PHASE phase) {
  if (phase == WARMUP)
    if (strcmp(workload_name, "ycsb") == 0)
      return std::make_pair(0, 1);
    else
      return std::make_pair(0, 0.2);

  return std::make_pair(20, 0);
}

void* client_workload(void* _args) {
  ClientArgs* args = (ClientArgs*)_args;
  setup_signal_handler();
  setup_client_logger(args->workload_name, args->cid);

  int ret = stick_this_thread_to_core(args->core);
  if (ret) {
    printd(L_INFO, "Failed to bind client %d to core %d", args->cid, args->core);
  } else {
    printd(L_INFO, "Running client %d on core %d", args->cid, args->core);
  }

  DMCWorkload load_wl, trans_wl;
  ret = load_workload(args->workload_name, -1, args->cid, args->num_global_clients, &load_wl, &trans_wl);
  assert(ret == 0 && load_wl.max_key_size + load_wl.max_val_size < 256 * 255);

  printd(L_DEBUG, "client %d waiting sync", args->cid);
  DMCMemcachedClient con_client(args->controller_ip);
  con_client.memcached_sync_ready(args->cid);

  DMCClient* client = args->client;

  PerformanceTracker tracker;
  std::pair<uint32_t, float> time_or_ratio = get_time_or_ratio(args->workload_name, WARMUP);
  tracker.init(time_or_ratio.first, load_wl.num_ops * time_or_ratio.second);
  run_workload(client, &load_wl, &tracker);

  bool warmup_oom = client->get_oom();
  printf("client %d warmup %d ops, oom: %s",
         args->cid, tracker.seq, warmup_oom ? "true" : "false");
  printd(L_INFO, "client %d warmup %d ops, oom: %s",
         args->cid, tracker.seq, warmup_oom ? "true" : "false");

  // synchronize clients before the measured run
  printd(L_INFO, "client %d waiting sync", args->cid);
  client->init_counters();
  con_client.memcached_sync_ready(args->cid);

  time_or_ratio = get_time_or_ratio(args->workload_name, TRANS);
  tracker.init(time_or_ratio.first, load_wl.num_ops * time_or_ratio.second);
  run_workload(client, &trans_wl, &tracker);

  // Get results and report
  json res = tracker.get_results(client);
  std::string str = res.dump();
  con_client.memcached_put_result((void*)str.c_str(), strlen(str.c_str()), args->cid);

  tracker.print_summary(args->cid, client);
  shutdown_client_logger();
  return NULL;
}

void run_client(const InitArgs* args) {
  int ret = 0;
  int num_local_clients = args->num_local_clients;
  DMCConfig client_conf_list[num_local_clients];
  DMCClient* client_list[num_local_clients];
  pthread_t client_tid_list[num_local_clients];
  ClientArgs client_main_arg_list[num_local_clients];

  for (int i = 0; i < num_local_clients; i++) {
    ret = load_config(args->config_fname, &client_conf_list[i]);
    assert(ret == 0);
    int core = client_conf_list[i].core_id + i;

    client_conf_list[i].st_client_id = args->st_client_id;
    client_conf_list[i].client_id = args->st_client_id + i;
    client_conf_list[i].server_id = client_conf_list[i].client_id;  // avoid bugs due to server_id
    client_conf_list[i].num_local_clients = num_local_clients;
    client_conf_list[i].num_global_clients = args->num_global_clients;
    client_list[i] = new DMCClient(&client_conf_list[i]);

    memset(&client_main_arg_list[i], 0, sizeof(ClientArgs));
    client_main_arg_list[i].client = client_list[i];
    client_main_arg_list[i].core = core;
    client_main_arg_list[i].cid = client_conf_list[i].client_id;            // client_id
    client_main_arg_list[i].num_load_ops = args->num_load_ops;              // -n
    client_main_arg_list[i].validate = args->validate;                      // -v
    client_main_arg_list[i].num_global_clients = args->num_global_clients;  // -A
    client_main_arg_list[i].run_tims_s = args->run_time;                    // -T
    client_main_arg_list[i].use_warmup = args->use_warmup;                  // -W
    strcpy(client_main_arg_list[i].workload_name, args->workload_name);
    strcpy(client_main_arg_list[i].controller_ip, args->memcached_ip);

    // Run workload for each client
    pthread_create(&client_tid_list[i], NULL, client_workload, &client_main_arg_list[i]);
  }

  for (int i = 0; i < args->num_local_clients; i++) {
    pthread_join(client_tid_list[i], NULL);
  }
}

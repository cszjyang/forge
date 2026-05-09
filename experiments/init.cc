#include "init.h"

#include <assert.h>
#include <getopt.h>
#include <stdio.h>
#include <unistd.h>

#include "client.h"
#include "dmc_utils.h"
#include "memcached.h"
#include "run_client.h"
#include "server.h"
#include "third_party/json.hpp"

using json = nlohmann::json;

static struct option opts[] = {
    {"config-file", 1, NULL, 'c'},
    {"num-client-threads", 1, NULL, 't'},
    {"all-clients", 1, NULL, 'A'},
    {"server", 0, NULL, 'S'},
    {"client", 0, NULL, 'C'},
    {"workload", 1, NULL, 'w'},
    {"num-load-ops", 1, NULL, 'n'},
    {"run-time", 1, NULL, 'T'},
    {"id", 1, NULL, 'i'},
    {"validate", 0, NULL, 'v'},
    {"memcached-ip", 1, NULL, 'm'},
    {"use-warmup", 0, NULL, 'W'},
};

InitArgs g_init_args;

int init_parse_args(int argc, char **argv) {
  memset(&g_init_args, 0, sizeof(_InitArgs));
  char c;
  bool is_server = false;
  bool is_client = false;
  while (1) {
    c = getopt_long(argc, argv, "c:t:SCi:n:T:w:vm:A:W", opts, NULL);
    if (c == -1) {
      break;
    }
    switch (c) {
    case 'c': // config file name
      strcpy(g_init_args.config_fname, optarg);
      break;
    case 't': // number of local clients
      g_init_args.num_local_clients = atoi(optarg);
      break;
    case 'S': // run as server
      is_server = true;
      g_init_args.role = SERVER;
      break;
    case 'C': // run as client
      is_client = true;
      g_init_args.role = CLIENT;
      break;
    case 'i': // the start id of local clients
      g_init_args.st_client_id = atoi(optarg);
      break;
    case 'n': // number of load operations
      g_init_args.num_load_ops = atoi(optarg);
      break;
    case 'T': // run time
      g_init_args.run_time = atoi(optarg);
      break;
    case 'w': // workload name
      strcpy(g_init_args.workload_name, optarg);
      printd(L_INFO, "workload name: %s", g_init_args.workload_name);
      break;
    case 'v': // validate
      g_init_args.validate = true;
      printd(L_DEBUG, "validation enabled");
      break;
    case 'm': // memcached ip
      strcpy(g_init_args.memcached_ip, optarg);
      break;
    case 'A': // number of global clients
      g_init_args.num_global_clients = atoi(optarg);
      break;
    case 'W': // use warmup
      g_init_args.use_warmup = true;
      break;
    default:
      printf("Invalid argument %c\n", c);
      return -1;
    }
  }
  if (is_server == is_client) {
    printf("Error: is_server == is_client\n");
    return -1;
  }
  return 0;
}

void run_server(const InitArgs *args);

int main(int argc, char **argv) {
  printf("HASH_BUCKET_ASSOC_NUM %lld\n", HASH_BUCKET_ASSOC_NUM);
  printf("HASH_NUM_BUCKETS %lld\n", HASH_NUM_BUCKETS);
  printf("MAX_GROUP_SIZE %lld\n", MAX_GROUP_SIZE);
  printf("GROUP_BYTES %lld\n", GROUP_BYTES);

  int ret = 0;
  ret = init_parse_args(argc, argv);
  if (ret) {
    printf("Failed to parse args\n");
    return ret;
  }
  if (g_init_args.role == SERVER) {
    run_server(&g_init_args);
  } else {
    assert(g_init_args.role == CLIENT);
    run_client(&g_init_args);
  }

  return 0;
}

void run_server(const InitArgs *args) {
  int ret = 0;
  DMCConfig server_conf;
  ret = load_config(args->config_fname, &server_conf);
  DMCMemcachedClient con_client(
      args->memcached_ip); // Use memcached to sync and collect results

  Server server(&server_conf);
  ServerMainArgs server_thread_args;
  server_thread_args.core_id = server_conf.core_id;
  server_thread_args.server = &server;
  pthread_t server_tid;
  ret = pthread_create(&server_tid, NULL, server_main, &server_thread_args);
  assert(ret == 0);

  con_client.memcached_sync_server_stop();
  std::vector<float> expert_weights;
  json res;
  res["num_evict"] = server.num_evict_;
  std::string res_str = res.dump();
  con_client.memcached_put_server_result((void *)res_str.c_str(),
                                         strlen(res_str.c_str()), 0);
  server.stop();
  pthread_join(server_tid, NULL);
}

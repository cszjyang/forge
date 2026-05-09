#ifndef _DMC_EXP_INIT_H_
#define _DMC_EXP_INIT_H_

#include <stdint.h>

typedef struct _InitArgs {
  uint8_t role;
  char config_fname[128];

  // client config
  uint32_t st_client_id;
  int num_local_clients;    // number of clients to start
  uint32_t num_global_clients;
  int num_load_ops;         // number of operations to execute
  int run_time;             // time to iteratively execute
  char workload_name[128];  // workload filename
  bool validate;            // enable validation

  // warmup
  bool use_warmup;

  // memcached controller
  char memcached_ip[128];
} InitArgs;

#endif

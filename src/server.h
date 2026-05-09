#ifndef _DMC_SERVER_H_
#define _DMC_SERVER_H_

#include <pthread.h>
#include <stdint.h>
#include <sys/socket.h>

#include <list>
#include <map>
#include <random>
#include <set>
#include <vector>

#include "client_mm.h"
#include "db_helper.h"
#include "dmc_table.h"
#include "dmc_utils.h"
#include "nm.h"
#include "server_mm.h"
#include "third_party/spinlock.h"

class Server;

typedef struct _ServerMainArgs {
  Server* server;
  int core_id;
  int worker_id;
} ServerMainArgs;

class Server {
  uint32_t server_id_;

  uint8_t eviction_type_;

  // use lock to protect the prio_slot_addr_map_;
  spinlock prio_slot_addr_map_lock_;
  std::map<double, std::set<uint64_t>> prio_slot_addr_map_;

  uint8_t need_stop_;

  UDPNetworkManager* nm_;
  spinlock mm_lock_;
  ServerMM* mm_;

  // global ts
  uint64_t sys_start_ts_;

  // for active server
  void* send_msg_buffer_;
  void* recv_msg_buffer_;
  struct ibv_mr* send_msg_buffer_mr_;
  struct ibv_mr* recv_msg_buffer_mr_;
  struct ibv_recv_wr rr_list_[MSG_BUF_NUM];
  struct ibv_send_wr sr_list_[MSG_BUF_NUM];
  struct ibv_sge rr_sge_list_[MSG_BUF_NUM];
  struct ibv_sge sr_sge_list_[MSG_BUF_NUM];
  int send_counter_;  // for periodically generate a send wc

  uint32_t num_workers_;
  pthread_t* worker_tid_;
  ServerMainArgs* worker_args_;

  uint32_t num_clients_;

  bool testing_;

  // for generate random number
  std::mt19937 gen_;

 public:
  // counters
  uint64_t num_evict_;

 private:
  void init_counters();
  void init_eviction(const DMCConfig* conf);
  void init_rdma_rpc();
  int server_on_connect(const UDPMsg* request,
                        struct sockaddr_in* src_addr,
                        socklen_t src_addr_len);
  int server_on_alloc(const UDPMsg* request,
                      struct sockaddr_in* src_addr,
                      socklen_t src_addr_len);
  int server_on_get_ts(const UDPMsg* request,
                       struct sockaddr_in* src_addr,
                       socklen_t src_addr_len);
  int server_on_recv_msg_alloc(const struct ibv_wc* wc);

  int server_reply_to_sid(const struct ibv_wc* wc,
                          uint16_t sid,
                          void* ret_val,
                          uint32_t val_size,
                          uint16_t ibmsg_type);

 public:
  Server(const DMCConfig* conf);
  ~Server();

  void* thread_main();                    // listen for connection and allocation requests
  void* worker_main(uint32_t worker_id);  // polling for 2-sided rdma rpcs

  void stop();

  // for testing
  int get(void* key,
          uint32_t key_size,
          __OUT void* val,
          __OUT uint32_t* val_size);
  int set(uint32_t worker_id,
          void* key,
          uint32_t key_size,
          void* val,
          uint32_t val_size);
};

void* server_main(void* server_main_args);

#endif

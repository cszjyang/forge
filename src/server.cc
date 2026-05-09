#include "server.h"

#include <assert.h>
#include <stdio.h>
#include <sys/time.h>
#include <unistd.h>

#include <algorithm>
#include <vector>

#include "debug.h"
#include "dmc_table.h"
#include "dmc_utils.h"
#include "ib.h"

void* server_main(void* server_main_args) {
  ServerMainArgs* args = (ServerMainArgs*)server_main_args;
  Server* server_instance = args->server;

  int ret = stick_this_thread_to_core(args->core_id);
  assert(ret == 0);
  printd(L_INFO, "Server is running on core: %d\n", args->core_id);

  return server_instance->thread_main();
}

void* server_worker_thread(void* server_main_args) {
  ServerMainArgs* args = (ServerMainArgs*)server_main_args;
  Server* server_instance = args->server;

  int ret = stick_this_thread_to_core(args->core_id);
  assert(ret == 0);
  printf("Server worker %d thread is running on core: %d\n", args->worker_id,
         args->core_id);

  return server_instance->worker_main(args->worker_id);
}

Server::Server(const DMCConfig* conf) {
  assert(conf->block_size == BLOCK_SIZE);
  init_eviction(conf);
  init_counters();
  init_rdma_rpc();
}

Server::~Server() {
  delete mm_;
  delete nm_;
  ibv_dereg_mr(send_msg_buffer_mr_);
  ibv_dereg_mr(recv_msg_buffer_mr_);
  free(send_msg_buffer_);
  free(recv_msg_buffer_);
}

void Server::init_eviction(const DMCConfig* conf) {
  server_id_ = conf->server_id;
  gen_.seed(server_id_);

  eviction_type_ = conf->eviction_type;
  num_workers_ = conf->num_server_threads;
  testing_ = conf->testing;
  num_clients_ = 0;
  need_stop_ = 0;

  nm_ = new UDPNetworkManager(conf);
  mm_ = new ServerMM(conf, nm_->get_ib_ctx(), nm_->get_ib_pd());
  spin_unlock(&mm_lock_);

  uint64_t base_addr = mm_->get_base_addr();
  uint64_t index_area_addr = mm_->get_index_area_addr();
  uint64_t gnode_area_addr = mm_->get_gnode_area_addr();
  uint64_t fifo_buffer_addr = mm_->get_fifo_buffer_addr();

  memset((void*)index_area_addr, 0, HASH_SPACE_BYTES);      // initialize hash table
  memset((void*)gnode_area_addr, 0, GNODE_SPACE_BYTES);     // initialize group meta array
  memset((void*)fifo_buffer_addr, 0xFF, FIFO_SPACE_BYTES * NUM_FIFO_TYPE);  // initialize FIFO queue, with invalid epoch 0xFF

  sys_start_ts_ = new_ts();
  printd(L_INFO, "start ts: %lx", sys_start_ts_);
}

void Server::init_rdma_rpc() {
  // initialize data structures for active server
  // allocate message buffer
  send_msg_buffer_ = malloc(MSG_BUF_SIZE * MSG_BUF_NUM);
  recv_msg_buffer_ = malloc(MSG_BUF_SIZE * MSG_BUF_NUM);
  assert(send_msg_buffer_ != NULL);
  assert(recv_msg_buffer_ != NULL);
  struct ibv_pd* pd = nm_->get_ib_pd();
  send_msg_buffer_mr_ =
      ibv_reg_mr(pd, send_msg_buffer_, MSG_BUF_SIZE * MSG_BUF_NUM,
                 IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE);
  recv_msg_buffer_mr_ =
      ibv_reg_mr(pd, recv_msg_buffer_, MSG_BUF_SIZE * MSG_BUF_NUM,
                 IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE);
  assert(send_msg_buffer_mr_ != NULL && recv_msg_buffer_mr_ != NULL);
  memset(sr_list_, 0, sizeof(struct ibv_send_wr) * MSG_BUF_NUM);

  // prepare multiple work requests
  for (uint64_t i = 0; i < MSG_BUF_NUM; i++) {
    // initialize sr_list_
    sr_list_[i].wr_id = i + 1000;
    sr_list_[i].next = NULL;
    sr_list_[i].sg_list = &sr_sge_list_[i];
    sr_list_[i].num_sge = 1;
    sr_list_[i].opcode = IBV_WR_SEND;
    sr_list_[i].send_flags = IBV_SEND_SIGNALED;
    sr_sge_list_[i].addr = (uint64_t)send_msg_buffer_ + i * MSG_BUF_SIZE;
    sr_sge_list_[i].length = MSG_BUF_SIZE;
    sr_sge_list_[i].lkey = send_msg_buffer_mr_->lkey;

    // initialize rr_list_
    rr_list_[i].wr_id = i;
    rr_list_[i].next = NULL;
    rr_list_[i].sg_list = &rr_sge_list_[i];
    rr_list_[i].num_sge = 1;
    rr_sge_list_[i].addr = (uint64_t)recv_msg_buffer_ + i * MSG_BUF_SIZE;
    rr_sge_list_[i].length = MSG_BUF_SIZE;
    rr_sge_list_[i].lkey = recv_msg_buffer_mr_->lkey;
  }

  // start worker thread, poll for RPC requests
  worker_tid_ = (pthread_t*)malloc(sizeof(pthread_t) * num_workers_);
  worker_args_ =
      (ServerMainArgs*)malloc(sizeof(ServerMainArgs) * num_workers_);
  for (int w = 0; w < num_workers_; w++) {
    printf("start worker %d\n", w);
    worker_args_[w].worker_id = w;
    worker_args_[w].core_id =
        (w % NUM_CORES) * 2;  // bind to the same numa node
    worker_args_[w].server = this;
    int ret = pthread_create(&worker_tid_[w], NULL, server_worker_thread,
                             (void*)&worker_args_[w]);
    assert(ret == 0);
  }
}

int Server::server_on_connect(const UDPMsg* request,
                              struct sockaddr_in* src_addr,
                              socklen_t src_addr_len) {
  int rc = 0;
  UDPMsg reply;
  memset(&reply, 0, sizeof(UDPMsg));

  reply.id = server_id_;
  reply.type = UDPMSG_REP_CONNECT;
  rc = nm_->nm_on_connect_new_qp(request, &reply.body.conn_info.qp_info);
  assert(rc == 0);

  spin_lock(&mm_lock_);
  rc = mm_->get_mr_info(&reply.body.conn_info.mr_info);
  rc = mm_->get_dm_mr_info(&reply.body.conn_info.dm_mr_info);
  spin_unlock(&mm_lock_);
  assert(rc == 0);

  serialize_udpmsg(&reply);

  rc = nm_->send_udp_msg(&reply, src_addr, src_addr_len);
  assert(rc == 0);

  deserialize_udpmsg(&reply);
  rc = nm_->nm_on_connect_connect_qp(request->id, &reply.body.conn_info.qp_info,
                                     &request->body.conn_info.qp_info);
  assert(rc == 0);

  num_clients_++;  // count the number of clients
  // post recv if the server is active
  if (true) {
    assert(request->id < 512);
    rc = nm_->rdma_post_recv_sid_async(&rr_list_[request->id], request->id);  // post one recv for each client, the recv would be reused after the completion
  }
  return 0;
}

int Server::server_on_alloc(const UDPMsg* request,
                            struct sockaddr_in* src_addr,
                            socklen_t src_addr_len) {
  SegmentInfo info;
  int ret = 0;
  ret = mm_->alloc_segment(request->id, &info);
  if (ret == -1) {
    printd(L_INFO, "Server run out-of-memory");
  }
  printd(L_DEBUG, "Allocated addr: 0x%lx to %d", info.addr, info.allocated_to);

  UDPMsg reply;
  memset(&reply, 0, sizeof(UDPMsg));
  reply.type = UDPMSG_REP_ALLOC;
  reply.id = server_id_;
  reply.body.mr_info.addr = info.addr;
  reply.body.mr_info.rkey = mm_->get_rkey();
  serialize_udpmsg(&reply);

  ret = nm_->send_udp_msg(&reply, src_addr, src_addr_len);
  assert(ret == 0);
  return 0;
}

int Server::server_on_get_ts(const UDPMsg* request,
                             struct sockaddr_in* src_addr,
                             socklen_t src_addr_len) {
  UDPMsg reply;
  memset(&reply, 0, sizeof(UDPMsg));
  reply.type = UDPMSG_REP_TS;
  reply.id = server_id_;
  reply.body.sys_start_ts = sys_start_ts_;
  serialize_udpmsg(&reply);
  int ret = nm_->send_udp_msg(&reply, src_addr, src_addr_len);
  assert(ret == 0);
  return 0;
}

void Server::init_counters() {
  num_evict_ = 0;
}

void* Server::thread_main() {
#ifdef _DEBUG
#if REDIRECT_LOG == 1
  std::string output_file = std::string(get_current_dir_name()) + "/../outputs/" + "server_thread_main_" + std::to_string(server_id_) + ".log";
  printd(L_INFO, "Redirecting logs to %s", output_file.c_str());
  open_thread_log(output_file.c_str());
#endif
#endif

  struct sockaddr_in client_addr;
  socklen_t client_addr_len = sizeof(struct sockaddr_in);
  UDPMsg request;
  int rc = 0;
  while (!need_stop_) {
    rc = nm_->recv_udp_msg(&request, &client_addr, &client_addr_len);
    if (rc) {
      continue;
    }

    deserialize_udpmsg(&request);

    if (request.type == UDPMSG_REQ_CONNECT) {
      rc = server_on_connect(&request, &client_addr, client_addr_len);
      assert(rc == 0);
    } else if (request.type == UDPMSG_REQ_ALLOC) {
      rc = server_on_alloc(&request, &client_addr, client_addr_len);
      assert(rc == 0);
    } else if (request.type == UDPMSG_REQ_TS) {
      rc = server_on_get_ts(&request, &client_addr, client_addr_len);
    } else {
      printd(L_ERROR, "Unsupported message type: %d", request.type);
    }
  }

#ifdef _DEBUG
#if REDIRECT_LOG == 1
  close_thread_log();
#endif
#endif
  return NULL;
}

int Server::server_on_recv_msg_alloc(const struct ibv_wc* wc) {
  uint16_t req_sid = *(uint16_t*)((uint64_t)recv_msg_buffer_ +
                                  wc->wr_id * MSG_BUF_SIZE + sizeof(uint16_t));
  uint64_t msg_laddr = (uint64_t)recv_msg_buffer_ + wc->wr_id * MSG_BUF_SIZE +
                       sizeof(uint16_t) + sizeof(uint16_t);

  SegmentInfo info;
  int ret = mm_->alloc_segment(req_sid, &info);
  if (ret == -1) {
    printd(L_INFO, "Server run out-of-memory");
  }
  printd(L_DEBUG, "Allocated addr: 0x%lx to %d", info.addr, info.allocated_to);

  ret = server_reply_to_sid(wc, req_sid, &info.addr, sizeof(uint64_t), IBMSG_REP_ALLOC);
  assert(ret == 0);
  return 0;
}

// poll for RPC requests
void* Server::worker_main(uint32_t worker_id) {
#ifdef _DEBUG
#if REDIRECT_LOG == 1
  std::string output_file = std::string(get_current_dir_name()) + "/../outputs/" + "server_worker_main_" + std::to_string(worker_id) + ".log";
  printd(L_INFO, "Redirecting logs to %s", output_file.c_str());
  open_thread_log(output_file.c_str());
#endif
#endif

  printd(L_INFO, "Worker main started!");

  int ret = 0;
  int num_polled = 0;
  uint64_t ops_executed = 0;
  printd(L_INFO, "Worker %d start to poll RDMA RPC!", worker_id);
  while (!need_stop_) {
    struct ibv_wc wc[512 / num_workers_];
    num_polled = nm_->rdma_poll_recv_completion_async(wc, 512 / num_workers_);
    for (int i = 0; i < num_polled; i++) {
      if (wc[i].status != IBV_WC_SUCCESS) {
        // printd(L_ERROR, "%lx", rr_list_[wc[i].wr_id].sg_list->addr);
        printd(L_ERROR,
               "OP (%ld): Server %d polled one error wc(wr_id: %ld, status: "
               "%d, opcode: %d)",
               ops_executed, worker_id, wc[i].wr_id, wc[i].status,
               wc[i].opcode);
        assert(0);
      }
      if (wc[i].opcode != IBV_WC_RECV) {
        printd(L_ERROR, "Expected WR %d Polled %d, wr_id: %ld", IBV_WC_RECV,
               wc[i].opcode, wc[i].wr_id);
        assert(0);
      }
      // process request
      uint64_t msg_lbuf =
          (uint64_t)recv_msg_buffer_ + wc[i].wr_id * MSG_BUF_SIZE;
      uint16_t msg_type = *(uint16_t*)msg_lbuf;
      uint16_t req_sid = *(uint16_t*)(msg_lbuf + sizeof(uint16_t));
      printd(L_DEBUG, "msg: %d", msg_type);
      switch (msg_type) {
        case IBMSG_REQ_ALLOC:
          ret = server_on_recv_msg_alloc(&wc[i]);
          assert(ret == 0);
          break;
        default:
          printd(L_ERROR, "Message %d not supported!", msg_type);
          abort();
      }
      ops_executed++;
    }
  }

#ifdef _DEBUG
#if REDIRECT_LOG == 1
  close_thread_log();
#endif
#endif

  return NULL;
}

void Server::stop() {
  // set stop flag
  need_stop_ = 1;

  // wait worker to exit
  for (int i = 0; i < num_workers_; i++) {
    pthread_join(worker_tid_[i], NULL);
  }
}

int Server::server_reply_to_sid(const struct ibv_wc* wc,
                                uint16_t sid,
                                void* ret_val,
                                uint32_t val_size,
                                uint16_t ibmsg_type) {
  assert(rr_list_[wc->wr_id].wr_id == wc->wr_id);
  int ret = nm_->rdma_post_recv_sid_async(&rr_list_[wc->wr_id], sid);
  assert(ret == 0);

  if (ret_val == NULL) {
    // if the return value is NULL, then we don't need to send anything
    // but only post a recv
    return 0;
  }
  uint64_t msg_buffer = (uint64_t)send_msg_buffer_ + wc->wr_id * MSG_BUF_SIZE;
  *(uint16_t*)msg_buffer = ibmsg_type;
  memcpy((void*)((uint64_t)msg_buffer + sizeof(uint16_t)), ret_val, val_size);  // allocated remote address

  struct ibv_send_wr* sr = &sr_list_[wc->wr_id];
  struct ibv_sge* sge = &sr_sge_list_[wc->wr_id];
  sge->length = sizeof(uint16_t) + val_size;  // set the sge->length according to the size of the message
  ret = nm_->rdma_post_send_sid_sync(sr, sid);
  if (ret != 0) {
    printd(L_ERROR, "rdma_post_send_sid_async return %d(%s) %d", ret,
           strerror(ret), send_counter_);
  }
  assert(ret == 0);
  return 0;
}

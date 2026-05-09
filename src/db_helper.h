#ifndef _DMC_DB_HELPER_H_
#define _DMC_DB_HELPER_H_

#include <infiniband/verbs.h>

#include "debug.h"
#include "ib.h"

typedef struct _DBHelper {
  struct ibv_send_wr* head_wr;
  struct ibv_send_wr* sr;
  struct ibv_sge* sge;
  uint32_t max_num_wr;
  uint32_t num_wr;
  uint64_t start_wr_id;

  void check_num_wr() {
    if (num_wr >= max_num_wr) {
      printd(L_ERROR, "num_wr %d >= max_num_wr %d", num_wr, max_num_wr);
      abort();
    }
  }

  void check_raddr(uint64_t raddr, uint64_t size) {
    if ((raddr >= dbg_server_st_raddr && raddr + size <= dbg_server_ed_raddr) ||
        (raddr >= dbg_server_devst_raddr && raddr + size <= dbg_server_deved_raddr)) {
      return;
    }

    printd(L_ERROR, "raddr %lx out of range", raddr);
    abort();
  }

  void init(struct ibv_send_wr* sr_, struct ibv_sge* sge_, uint32_t max_num_wr_, uint64_t start_wr_id_ = 0) {
    sr = sr_;
    sge = sge_;
    max_num_wr = max_num_wr_;
    start_wr_id = start_wr_id_;
    head_wr = NULL;
    num_wr = 0;
  }

  void init(uint64_t start_wr_id_) {
    start_wr_id = start_wr_id_;
    head_wr = NULL;
    num_wr = 0;
  }

  void init() {
    head_wr = NULL;
    num_wr = 0;
  }

  int pop() {
    if (num_wr <= 0) {
      printd(L_ERROR, "pop: num_wr %d <= 0", num_wr);
      abort();
    }

    num_wr--;
    if (num_wr == 0) {
      head_wr = NULL;
    } else {
      head_wr = &sr[num_wr - 1];
    }
    return num_wr;
  }

  // Simplified single-SGE write for backward compatibility
  int push_write(uint64_t raddr, uint32_t rkey, uint64_t laddr, uint32_t lkey, uint32_t size, int send_flags) {
#ifdef _DEBUG
    check_num_wr();
    check_raddr(raddr, size);

    printd(L_DEBUG_VERBOSE, "%i push_write: wr_id %d, raddr %lx, rkey %x, laddr %lx, lkey %x, size %u, send_flags %x",
           num_wr, start_wr_id + num_wr, raddr, rkey, laddr, lkey, size, send_flags);
#endif

    ib_create_sge(laddr, lkey, size, &sge[num_wr]);
    memset(&sr[num_wr], 0, sizeof(struct ibv_send_wr));
    sr[num_wr].wr_id = start_wr_id + num_wr;
    sr[num_wr].next = head_wr;
    sr[num_wr].sg_list = &sge[num_wr];
    sr[num_wr].num_sge = 1;
    sr[num_wr].opcode = IBV_WR_RDMA_WRITE;
    sr[num_wr].send_flags = size <= MAX_INLINE_SIZE ? (send_flags | IBV_SEND_INLINE)
                                                    : send_flags;
    sr[num_wr].wr.rdma.remote_addr = raddr;
    sr[num_wr].wr.rdma.rkey = rkey;
    head_wr = &sr[num_wr];
    num_wr++;
    return num_wr;
  }

  int push_read(uint64_t raddr, uint32_t rkey, uint64_t laddr, uint32_t lkey, uint32_t size, int send_flags) {
#ifdef _DEBUG
    check_num_wr();
    check_raddr(raddr, size);

    printd(L_DEBUG_VERBOSE, "%i push_read: wr_id %d, raddr %lx, rkey %x, laddr %lx, lkey %x, size %u, send_flags %x",
           num_wr, start_wr_id + num_wr, raddr, rkey, laddr, lkey, size, send_flags);
#endif

    ib_create_sge(laddr, lkey, size, &sge[num_wr]);
    memset(&sr[num_wr], 0, sizeof(struct ibv_send_wr));
    sr[num_wr].wr_id = start_wr_id + num_wr;
    sr[num_wr].next = head_wr;
    sr[num_wr].sg_list = &sge[num_wr];
    sr[num_wr].num_sge = 1;
    sr[num_wr].opcode = IBV_WR_RDMA_READ;
    sr[num_wr].send_flags = send_flags;
    sr[num_wr].wr.rdma.remote_addr = raddr;
    sr[num_wr].wr.rdma.rkey = rkey;
    head_wr = &sr[num_wr];
    num_wr++;
    return num_wr;
  }

  int push_cas(uint64_t raddr, uint32_t rkey, uint64_t laddr, uint32_t lkey, uint64_t expected_val, uint64_t swap_val, int send_flags) {
#ifdef _DEBUG
    check_num_wr();
    check_raddr(raddr, 0);
    if (laddr % 8 != 0 || raddr % 8 != 0) {
      printd(L_ERROR, "address %lx or %lx is not 8-byte aligned", laddr, raddr);
      abort();
    }

    printd(L_DEBUG_VERBOSE, "%d push_cas: wr_id %d, raddr %lx, rkey %x, laddr %lx, lkey %x, expected_val %lx, swap_val %lx, send_flags %x",
           num_wr, start_wr_id + num_wr, raddr, rkey, laddr, lkey, expected_val, swap_val, send_flags);
#endif

    ib_create_sge(laddr, lkey, sizeof(uint64_t), &sge[num_wr]);
    memset(&sr[num_wr], 0, sizeof(struct ibv_send_wr));
    sr[num_wr].wr_id = start_wr_id + num_wr;
    sr[num_wr].next = head_wr;
    sr[num_wr].sg_list = &sge[num_wr];
    sr[num_wr].num_sge = 1;
    sr[num_wr].opcode = IBV_WR_ATOMIC_CMP_AND_SWP;
    sr[num_wr].send_flags = send_flags;
    sr[num_wr].wr.atomic.remote_addr = raddr;
    sr[num_wr].wr.atomic.rkey = rkey;
    sr[num_wr].wr.atomic.compare_add = expected_val;
    sr[num_wr].wr.atomic.swap = swap_val;
    head_wr = &sr[num_wr];
    num_wr++;
    return num_wr;
  }

  int push_faa(uint64_t raddr, uint32_t rkey, uint64_t laddr, uint32_t lkey, uint64_t add, int send_flags) {
#ifdef _DEBUG
    check_num_wr();
    check_raddr(raddr, 0);
    if (laddr % 8 != 0 || raddr % 8 != 0) {
      printd(L_ERROR, "address %lx or %lx is not 8-byte aligned", laddr, raddr);
      abort();
    }

    printd(L_DEBUG_VERBOSE, "%i push_faa: wr_id %d, raddr %lx, rkey %x, laddr %lx, lkey %x, add %lx, send_flags %x",
           num_wr, start_wr_id + num_wr, raddr, rkey, laddr, lkey, add, send_flags);
#endif

    ib_create_sge(laddr, lkey, sizeof(uint64_t), &sge[num_wr]);
    memset(&sr[num_wr], 0, sizeof(struct ibv_send_wr));
    sr[num_wr].wr_id = start_wr_id + num_wr;
    sr[num_wr].next = head_wr;
    sr[num_wr].sg_list = &sge[num_wr];
    sr[num_wr].num_sge = 1;
    sr[num_wr].opcode = IBV_WR_ATOMIC_FETCH_AND_ADD;
    sr[num_wr].send_flags = send_flags;
    sr[num_wr].wr.atomic.remote_addr = raddr;
    sr[num_wr].wr.atomic.rkey = rkey;
    sr[num_wr].wr.atomic.compare_add = add;
    head_wr = &sr[num_wr];
    num_wr++;
    return num_wr;
  }
} DBHelper;

#endif
#ifndef _DMC_WORKLOAD_H_
#define _DMC_WORKLOAD_H_

#include <dmc_utils.h>
#include <stdint.h>

#ifdef EVA_REAL_SIZE
#define META_MAX_KEY_SIZE (256)
#define META_MAX_VAL_SIZE (64768)
#define META_UNI_BLOCK_SIZE (256)

#define TWITTER_MAX_KEY_SIZE (256)
#define TWITTER_MAX_VAL_SIZE (64768)
#define TWITTER_UNI_BLOCK_SIZE (256)
#else
#define META_MAX_KEY_SIZE (128)
#define META_MAX_VAL_SIZE (120)
#define META_UNI_BLOCK_SIZE (256)

#define TWITTER_MAX_KEY_SIZE (128)
#define TWITTER_MAX_VAL_SIZE (120)
#define TWITTER_UNI_BLOCK_SIZE (256)
#endif

#define YCSB_MAX_KEY_SIZE (128)
#define YCSB_MAX_VAL_SIZE (120)
#define YCSB_UNI_BLOCK_SIZE (256)
#define YCSB_MAX_SCAN_LEN (100ULL)

#define WIKI_MAX_KEY_SIZE (128)
#define WIKI_MAX_VAL_SIZE (120)
#define WIKI_UNI_BLOCK_SIZE (256)

#define CPHY_MAX_KEY_SIZE (128)
#define CPHY_MAX_VAL_SIZE (120)
#define CPHY_UNI_BLOCK_SIZE (256)

#define MSR_MAX_KEY_SIZE (128)
#define MSR_MAX_VAL_SIZE (120)
#define MSR_UNI_BLOCK_SIZE (256)

#define IBM_MAX_KEY_SIZE (128)
#define IBM_MAX_VAL_SIZE (120)
#define IBM_UNI_BLOCK_SIZE (256)

typedef struct _DMCWorkload {
  uint32_t max_key_size;
  uint32_t max_val_size;
  void* key_buf;
  void* val_buf;
  uint32_t* key_size_list;
  uint32_t* val_size_list;
  uint8_t* op_list;
  uint64_t num_ops;
} DMCWorkload;

int load_workload(const char* workload_name,
                  int num_load_ops,
                  uint32_t client_id,
                  uint32_t num_global_clients,
                  __OUT DMCWorkload* load_wl,
                  __OUT DMCWorkload* trans_wl);

int load_workload(const char* workload_name,
                  int num_load_ops,
                  uint32_t client_id,
                  uint32_t num_global_clients,
                  __OUT DMCWorkload* wl);

int load_workload_ycsb(const char* workload_name,
                       int num_load_ops,
                       uint32_t client_id,
                       uint32_t num_global_clients,
                       __OUT DMCWorkload* load_wl,
                       __OUT DMCWorkload* trans_wl);

void free_workload(DMCWorkload* wl);

#endif

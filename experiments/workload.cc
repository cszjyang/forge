#include "workload.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "client.h"

std::mutex workload_mutex;
std::map<std::string, std::map<uint32_t, std::vector<std::string>>> fname_wl_lists;
static constexpr int st_client_id = 1;

std::vector<std::string>& read_workload_file(const char* wl_fname, uint32_t client_id, uint32_t num_global_clients) {
  std::lock_guard<std::mutex> lock(workload_mutex);
  auto& wl_lists = fname_wl_lists[wl_fname];
  uint32_t wl_idx = client_id - st_client_id;
  if (wl_lists[wl_idx].size() > 0) {
    printf("Client %d use cached workload %s\n", client_id, wl_fname);
    return wl_lists[wl_idx];
  }

  FILE* f = fopen(wl_fname, "r");
  assert(f != NULL);
  printf("Client %d loading %s\n", client_id, wl_fname);

  char buf[2048];
  uint32_t cnt = 0;
  while (fgets(buf, 2048, f) == buf) {
    if (buf[0] == '\n')
      continue;
    wl_lists[cnt++ % num_global_clients].emplace_back(buf);
  }

  return wl_lists[wl_idx];
}

static int load_ycsb_workload(const char* workload_name,
                              int num_load_ops,
                              uint32_t client_id,
                              uint32_t num_global_clients,
                              __OUT DMCWorkload* wl) {
  wl->max_key_size = YCSB_MAX_KEY_SIZE;
  wl->max_val_size = YCSB_MAX_VAL_SIZE;

  char wl_fname[512];
  if (snprintf(wl_fname, sizeof(wl_fname), "../workloads/ycsb/%s", workload_name) >= sizeof(wl_fname)) {
    fprintf(stderr, "Workload filename too long\n");
    return -1;
  }
  std::vector<std::string>& wl_list = read_workload_file(wl_fname, client_id, num_global_clients);

  if (num_load_ops == -1)  // If num_load_ops is -1, then load all the operations
    wl->num_ops = wl_list.size();
  else  // Else, load num_load_ops operations
    wl->num_ops = num_load_ops;

  if (strcmp("ycsbe.trans", workload_name) == 0) {
    wl->key_buf = malloc(wl->max_key_size * wl->num_ops * YCSB_MAX_SCAN_LEN);  // Max key size is 128 bytes
    wl->val_buf = malloc(wl->max_val_size * wl->num_ops * YCSB_MAX_SCAN_LEN);  // Max value size is 120 bytes
    wl->key_size_list = (uint32_t*)malloc(sizeof(uint32_t) * wl->num_ops * YCSB_MAX_SCAN_LEN);
    wl->val_size_list = (uint32_t*)malloc(sizeof(uint32_t) * wl->num_ops * YCSB_MAX_SCAN_LEN);
    wl->op_list = (uint8_t*)malloc(sizeof(uint8_t) * wl->num_ops * YCSB_MAX_SCAN_LEN);

    printf("Client %d loading %ld ops\n", client_id, wl_list.size());
    char ops_buf[64];
    char key_buf[YCSB_MAX_KEY_SIZE];
    uint64_t key;
    uint32_t scan_len;
    uint32_t num_req = 0;
    for (int i = 0; i < wl->num_ops; i++) {
      if (strncmp("SCAN", wl_list[i].c_str(), 4) == 0) {
        sscanf(wl_list[i].c_str(), "%s %lu %u", ops_buf, &key, &scan_len);
        for (uint32_t j = 0; j < scan_len; j++) {
          sprintf(key_buf, "%lu", key + j);
          memcpy((void*)((uint64_t)wl->key_buf + num_req * wl->max_key_size), key_buf, wl->max_key_size);
          memcpy((void*)((uint64_t)wl->val_buf + num_req * wl->max_val_size), &num_req, sizeof(int));
          wl->key_size_list[num_req] = strlen(key_buf);
          wl->val_size_list[num_req] = 120;
          wl->op_list[num_req] = GET;
          num_req++;
        }
      } else {
        sscanf(wl_list[i].c_str(), "%s %s", ops_buf, key_buf);
        memcpy((void*)((uint64_t)wl->key_buf + num_req * wl->max_key_size), key_buf, wl->max_key_size);
        memcpy((void*)((uint64_t)wl->val_buf + num_req * wl->max_val_size), &num_req, sizeof(int));
        wl->key_size_list[num_req] = strlen(key_buf);
        wl->val_size_list[num_req] = 120;
        wl->op_list[num_req] = SET;
        num_req++;
      }
    }
    wl->num_ops = num_req;
    printf("Client %d loading %d ycsbe reqs\n", client_id, wl->num_ops);
    return 0;
  }

  wl->key_buf = malloc(wl->max_key_size * wl->num_ops);  // Max key size is 128 bytes
  wl->val_buf = malloc(wl->max_val_size * wl->num_ops);  // Max value size is 120 bytes
  wl->key_size_list = (uint32_t*)malloc(sizeof(uint32_t) * wl->num_ops);
  wl->val_size_list = (uint32_t*)malloc(sizeof(uint32_t) * wl->num_ops);
  wl->op_list = (uint8_t*)malloc(sizeof(uint8_t) * wl->num_ops);

  printf("Client %d loading %ld ops\n", client_id, wl_list.size());
  char ops_buf[64];
  char key_buf[YCSB_MAX_KEY_SIZE];
  for (int i = 0; i < wl->num_ops; i++) {
    sscanf(wl_list[i].c_str(), "%s %s", ops_buf, key_buf);  // Get the operation and key form the line in the workload file
    memcpy((void*)((uint64_t)wl->key_buf + i * wl->max_key_size), key_buf, wl->max_key_size);
    memcpy((void*)((uint64_t)wl->val_buf + i * wl->max_val_size), &i, sizeof(int));
    wl->key_size_list[i] = strlen(key_buf);
    wl->val_size_list[i] = 120;

    // In YCSB loads, the operation is always INSERT
    // In YCSB transactions, the operation is either READ or UPDATE
    if (strcmp("READ", ops_buf) == 0) {
      wl->op_list[i] = GET;
    } else {
      wl->op_list[i] = SET;
    }
  }
  return 0;
}

static int load_wiki_workload(const char* workload_name,
                              int num_load_ops,
                              uint32_t client_id,
                              uint32_t num_global_clients,
                              __OUT DMCWorkload* wl) {
  wl->max_key_size = WIKI_MAX_KEY_SIZE;
  wl->max_val_size = WIKI_MAX_VAL_SIZE;
  char wl_fname[512];
  if (snprintf(wl_fname, sizeof(wl_fname), "../workloads/wiki/%s", workload_name) >= sizeof(wl_fname)) {
    fprintf(stderr, "Workload filename too long\n");
    return -1;
  }
  std::vector<std::string>& wl_list = read_workload_file(wl_fname, client_id, num_global_clients);

  if (num_load_ops == -1) {
    wl->num_ops = wl_list.size();
  } else {
    wl->num_ops = num_load_ops;
  }
  wl->key_buf = malloc(wl->max_key_size * wl->num_ops);
  wl->val_buf = malloc(wl->max_val_size * wl->num_ops);
  wl->key_size_list = (uint32_t*)malloc(sizeof(uint32_t) * wl->num_ops);
  wl->val_size_list = (uint32_t*)malloc(sizeof(uint32_t) * wl->num_ops);
  wl->op_list = (uint8_t*)malloc(sizeof(uint8_t) * wl->num_ops);

  printf("Client %d loading %ld ops\n", client_id, wl_list.size());
  int ts;
  uint32_t item_size;
  char keybuf[WIKI_MAX_KEY_SIZE];
  int feature;
  for (int i = 0; i < wl->num_ops; i++) {
    sscanf(wl_list[i].c_str(), "%d %s %d %d", &ts, keybuf, &item_size, &feature);
    memcpy((void*)((uint64_t)wl->key_buf + i * wl->max_key_size), keybuf, wl->max_key_size);
    memcpy((void*)((uint64_t)wl->val_buf + i * wl->max_val_size), &i, sizeof(int));
    wl->key_size_list[i] = strlen(keybuf);
    wl->val_size_list[i] = 120;
    wl->op_list[i] = GET;
  }
  return 0;
}

static int load_cphy_workload(const char* workload_name,
                              int num_load_ops,
                              uint32_t client_id,
                              uint32_t num_global_clients,
                              __OUT DMCWorkload* wl) {
  wl->max_key_size = CPHY_MAX_KEY_SIZE;  // 10 in fact
  wl->max_val_size = CPHY_MAX_VAL_SIZE;  // 1052672 in fact

  char wl_fname[512];
  if (snprintf(wl_fname, sizeof(wl_fname), "../workloads/cphy/%s", workload_name) >= sizeof(wl_fname)) {
    fprintf(stderr, "Workload filename too long\n");
    return -1;
  }
  std::vector<std::string>& wl_list = read_workload_file(wl_fname, client_id, num_global_clients);

  if (num_load_ops == -1) {
    wl->num_ops = wl_list.size();
  } else {
    wl->num_ops = num_load_ops;
  }
  wl->key_buf = malloc(wl->max_key_size * wl->num_ops);
  wl->val_buf = malloc(wl->max_val_size * wl->num_ops);
  wl->key_size_list = (uint32_t*)malloc(sizeof(uint32_t) * wl->num_ops);
  wl->val_size_list = (uint32_t*)malloc(sizeof(uint32_t) * wl->num_ops);
  wl->op_list = (uint8_t*)malloc(sizeof(uint8_t) * wl->num_ops);

  printf("Client %d loading %ld ops\n", client_id, wl_list.size());
  uint64_t ts;
  uint32_t obj_size;
  char keybuf[CPHY_MAX_KEY_SIZE];
  char opbuf[128];
  int feature;
  uint64_t next_acc_time;
  for (uint64_t i = 0; i < wl->num_ops; i++) {
    sscanf(wl_list[i].c_str(), "%ld %s %d %ld", &ts, keybuf, &obj_size,
           &next_acc_time);
    memcpy((void*)((uint64_t)wl->key_buf + i * wl->max_key_size), keybuf, wl->max_key_size);
    memcpy((void*)((uint64_t)wl->val_buf + i * wl->max_val_size), &i, sizeof(uint64_t));
    wl->key_size_list[i] = strlen(keybuf);
    wl->val_size_list[i] = 120;
    wl->op_list[i] = GET;
  }
  printd(L_INFO, "load done");
  return 0;
}

static int load_msr_workload(const char* workload_name,
                             int num_load_ops,
                             uint32_t client_id,
                             uint32_t num_global_clients,
                             __OUT DMCWorkload* wl) {
  wl->max_key_size = MSR_MAX_KEY_SIZE;
  wl->max_val_size = MSR_MAX_VAL_SIZE;

  char wl_fname[512];
  if (snprintf(wl_fname, sizeof(wl_fname), "../workloads/msr/%s", workload_name) >= sizeof(wl_fname)) {
    fprintf(stderr, "Workload filename too long\n");
    return -1;
  }
  std::vector<std::string>& wl_list = read_workload_file(wl_fname, client_id, num_global_clients);

  if (num_load_ops == -1) {
    wl->num_ops = wl_list.size();
  } else {
    wl->num_ops = num_load_ops;
  }
  wl->key_buf = malloc(wl->max_key_size * wl->num_ops);
  wl->val_buf = malloc(wl->max_val_size * wl->num_ops);
  wl->key_size_list = (uint32_t*)malloc(sizeof(uint32_t) * wl->num_ops);
  wl->val_size_list = (uint32_t*)malloc(sizeof(uint32_t) * wl->num_ops);
  wl->op_list = (uint8_t*)malloc(sizeof(uint8_t) * wl->num_ops);

  printf("Client %d loading %ld ops\n", client_id, wl_list.size());
  uint64_t ts;
  uint32_t obj_size;
  char keybuf[MSR_MAX_KEY_SIZE];
  char opbuf[128];
  int feature;
  uint64_t next_acc_time;
  for (uint64_t i = 0; i < wl->num_ops; i++) {
    sscanf(wl_list[i].c_str(), "%ld %s %d %ld", &ts, keybuf, &obj_size,
           &next_acc_time);
    memcpy((void*)((uint64_t)wl->key_buf + i * wl->max_key_size), keybuf, wl->max_key_size);
    memcpy((void*)((uint64_t)wl->val_buf + i * wl->max_val_size), &i, sizeof(uint64_t));
    wl->key_size_list[i] = strlen(keybuf);
    wl->val_size_list[i] = 120;
    wl->op_list[i] = GET;
  }
  printd(L_INFO, "load done");
  return 0;
}

static int load_meta_workload(const char* workload_name,
                              int num_load_ops,
                              uint32_t client_id,
                              uint32_t num_global_clients,
                              __OUT DMCWorkload* wl) {
  wl->max_key_size = META_MAX_KEY_SIZE;
  wl->max_val_size = META_MAX_VAL_SIZE;

  char wl_fname[512];
  if (snprintf(wl_fname, sizeof(wl_fname), "../workloads/meta/%s", workload_name) >= sizeof(wl_fname)) {
    fprintf(stderr, "Workload filename too long\n");
    return -1;
  }
  std::vector<std::string>& wl_list = read_workload_file(wl_fname, client_id, num_global_clients);

  if (num_load_ops == -1) {
    wl->num_ops = wl_list.size();
  } else {
    wl->num_ops = num_load_ops;
  }
  wl->key_buf = malloc(wl->max_key_size * wl->num_ops);
  wl->val_buf = malloc(wl->max_val_size);
  wl->key_size_list = (uint32_t*)malloc(sizeof(uint32_t) * wl->num_ops);
  wl->val_size_list = (uint32_t*)malloc(sizeof(uint32_t) * wl->num_ops);
  wl->op_list = (uint8_t*)malloc(sizeof(uint8_t) * wl->num_ops);

  printf("Client %d loading %ld ops\n", client_id, wl_list.size());
  uint64_t ts;
  uint32_t obj_size;
  char keybuf[META_MAX_KEY_SIZE];
  char opbuf[128];
  int feature;
  uint64_t next_acc_time;
  for (uint64_t i = 0; i < wl->num_ops; i++) {
    sscanf(wl_list[i].c_str(), "%ld %s %d %ld", &ts, keybuf, &obj_size,
           &next_acc_time);
    memcpy((void*)((uint64_t)wl->key_buf + i * wl->max_key_size), keybuf, wl->max_key_size);
#ifdef EVA_REAL_SIZE
    wl->key_size_list[i] = strlen(keybuf);
    wl->val_size_list[i] = obj_size - wl->key_size_list[i] > META_MAX_VAL_SIZE
                               ? META_MAX_VAL_SIZE
                               : obj_size - wl->key_size_list[i];
#else
    wl->key_size_list[i] = strlen(keybuf);
    wl->val_size_list[i] = 120;
#endif
    wl->op_list[i] = GET;
  }
  printd(L_INFO, "load done");
  return 0;
}

static int load_oG_twitter_workload(const char* workload_name,
                                    int num_load_ops,
                                    uint32_t client_id,
                                    uint32_t num_global_clients,
                                    __OUT DMCWorkload* wl) {
  wl->max_key_size = TWITTER_MAX_KEY_SIZE;
  wl->max_val_size = TWITTER_MAX_VAL_SIZE;

  char wl_fname[512];
  if (snprintf(wl_fname, sizeof(wl_fname), "../workloads/twitter/%s", workload_name) >= sizeof(wl_fname)) {
    fprintf(stderr, "Workload filename too long\n");
    return -1;
  }
  std::vector<std::string>& wl_list = read_workload_file(wl_fname, client_id, num_global_clients);

  if (num_load_ops == -1) {
    wl->num_ops = wl_list.size();
  } else {
    wl->num_ops = num_load_ops;
  }
  wl->key_buf = malloc(wl->max_key_size * wl->num_ops);
  wl->val_buf = malloc(wl->max_val_size);
  wl->key_size_list = (uint32_t*)malloc(sizeof(uint32_t) * wl->num_ops);
  wl->val_size_list = (uint32_t*)malloc(sizeof(uint32_t) * wl->num_ops);
  wl->op_list = (uint8_t*)malloc(sizeof(uint8_t) * wl->num_ops);

  printf("Client %d loading %ld ops\n", client_id, wl_list.size());
  uint64_t ts;
  uint32_t obj_size;
  char keybuf[wl->max_key_size];
  char opbuf[128];
  int feature;
  uint64_t next_acc_time;
  for (uint64_t i = 0; i < wl->num_ops; i++) {
    sscanf(wl_list[i].c_str(), "%ld %s %d %ld", &ts, keybuf, &obj_size,
           &next_acc_time);
    memcpy((void*)((uint64_t)wl->key_buf + i * wl->max_key_size), keybuf, wl->max_key_size);
#ifdef EVA_REAL_SIZE
    wl->key_size_list[i] = strlen(keybuf);
    wl->val_size_list[i] = obj_size - wl->key_size_list[i] > TWITTER_MAX_VAL_SIZE
                               ? TWITTER_MAX_VAL_SIZE
                               : obj_size - wl->key_size_list[i];
#else
    wl->key_size_list[i] = strlen(keybuf);
    wl->val_size_list[i] = 120;
#endif
    wl->op_list[i] = GET;
  }
  printd(L_INFO, "load done");
  return 0;
}

static int load_ibm_workload(const char* workload_name,
                             int num_load_ops,
                             uint32_t client_id,
                             uint32_t num_global_clients,
                             __OUT DMCWorkload* wl) {
  wl->max_key_size = IBM_MAX_KEY_SIZE;
  wl->max_val_size = IBM_MAX_VAL_SIZE;

  char wl_fname[512];
  if (snprintf(wl_fname, sizeof(wl_fname), "../workloads/ibm/%s", workload_name) >= sizeof(wl_fname)) {
    fprintf(stderr, "Workload filename too long\n");
    return -1;
  }
  std::vector<std::string>& wl_list = read_workload_file(wl_fname, client_id, num_global_clients);

  FILE* f = fopen(wl_fname, "r");
  assert(f != NULL);
  printd(L_INFO, "Client %d loading %s", client_id, workload_name);

  if (num_load_ops == -1) {
    wl->num_ops = wl_list.size();
  } else {
    wl->num_ops = num_load_ops;
  }
  wl->key_buf = malloc(wl->max_key_size * wl->num_ops);
  wl->val_buf = malloc(wl->max_val_size * wl->num_ops);
  wl->key_size_list = (uint32_t*)malloc(sizeof(uint32_t) * wl->num_ops);
  wl->val_size_list = (uint32_t*)malloc(sizeof(uint32_t) * wl->num_ops);
  wl->op_list = (uint8_t*)malloc(sizeof(uint8_t) * wl->num_ops);

  printf("Client %d loading %ld ops\n", client_id, wl_list.size());
  uint64_t ts;
  uint32_t obj_size;
  char keybuf[128];
  char opbuf[128];
  int feature;
  for (uint64_t i = 0; i < wl->num_ops; i++) {
    sscanf(wl_list[i].c_str(), "%lu %s %s %d", &ts, opbuf, keybuf, &obj_size);
    memcpy((void*)((uint64_t)wl->key_buf + i * wl->max_key_size), keybuf, wl->max_key_size);
    memcpy((void*)((uint64_t)wl->val_buf + i * wl->max_val_size), &i, sizeof(uint64_t));
    wl->key_size_list[i] = strlen(keybuf);
    wl->val_size_list[i] = 120;
    if (strcmp(opbuf, "REST.PUT.OBJECT") == 0) {
      wl->op_list[i] = SET;
    } else {
      wl->op_list[i] = GET;
    }
  }
  printd(L_INFO, "load done");
  return 0;
}

static int load_twitter_workload(const char* workload_name,
                                 int num_load_ops,
                                 uint32_t client_id,
                                 uint32_t num_global_clients,
                                 __OUT DMCWorkload* wl) {
  wl->max_key_size = TWITTER_MAX_KEY_SIZE;
  wl->max_val_size = TWITTER_MAX_VAL_SIZE;

  char wl_fname[512];
  if (snprintf(wl_fname, sizeof(wl_fname), "../workloads/twitter/%s", workload_name) >= sizeof(wl_fname)) {
    fprintf(stderr, "Workload filename too long\n");
    return -1;
  }
  std::vector<std::string>& wl_list = read_workload_file(wl_fname, client_id, num_global_clients);

  if (num_load_ops == -1) {
    wl->num_ops = wl_list.size();
  } else {
    wl->num_ops = num_load_ops;
  }
  wl->key_buf = malloc(wl->max_key_size * wl->num_ops);  // buffer keys from the trace in memory
  wl->val_buf = malloc(wl->max_val_size);                // the value buffer is shared by all keys to save memory
  wl->key_size_list = (uint32_t*)malloc(sizeof(uint32_t) * wl->num_ops);
  wl->val_size_list = (uint32_t*)malloc(sizeof(uint32_t) * wl->num_ops);
  wl->op_list = (uint8_t*)malloc(sizeof(uint8_t) * wl->num_ops);

  printf("Client %d loading %ld ops\n", client_id, wl_list.size());
  int ts;
  uint32_t key_size;
  uint32_t val_size;
  uint32_t cid;
  char keybuf[2048];
  char opbuf[64];
  int ttl;
  for (int i = 0; i < wl->num_ops; i++) {
    sscanf(wl_list[i].c_str(), "%d %s %d %d %d %s %d", &ts, keybuf, &key_size,
           &val_size, &cid, opbuf, &ttl);
    memcpy((void*)((uint64_t)wl->key_buf + i * wl->max_key_size), keybuf, wl->max_key_size);
#ifdef EVA_REAL_SIZE
    wl->key_size_list[i] = strlen(keybuf) > TWITTER_MAX_KEY_SIZE ? TWITTER_MAX_KEY_SIZE : strlen(keybuf);
    wl->val_size_list[i] = val_size > TWITTER_MAX_VAL_SIZE ? TWITTER_MAX_VAL_SIZE : val_size;
#else
    wl->key_size_list[i] = strlen(keybuf) > 128 ? 128 : strlen(keybuf);
    wl->val_size_list[i] = sizeof(int);
#endif
    if (strcmp(opbuf, "get") == 0 || strcmp(opbuf, "gets") == 0) {
      wl->op_list[i] = GET;
    } else {
      wl->op_list[i] = SET;
    }
  }
  return 0;
}

int load_workload(const char* workload_name,
                  int num_load_ops,
                  uint32_t client_id,
                  uint32_t num_global_clients,
                  __OUT DMCWorkload* load_wl,
                  __OUT DMCWorkload* trans_wl) {
  if (memcmp(workload_name, "ycsb", strlen("ycsb")) == 0) {
    load_workload_ycsb(workload_name, num_load_ops, client_id, num_global_clients, load_wl, trans_wl);
  } else {
    load_workload(workload_name, num_load_ops, client_id, num_global_clients, load_wl);
    *trans_wl = *load_wl;
  }
  return 0;
}

int load_workload(const char* workload_name,
                  int num_load_ops,
                  uint32_t client_id,
                  uint32_t num_global_clients,
                  __OUT DMCWorkload* wl) {
  if (memcmp(workload_name, "oG-twitter", strlen("oG-twitter")) == 0) {
    char fname_buf[256];
    snprintf(fname_buf, sizeof(fname_buf), "%s", workload_name);
    load_oG_twitter_workload(fname_buf, num_load_ops, client_id, num_global_clients, wl);
  } else if (memcmp(workload_name, "twitter", strlen("twitter")) == 0) {
    load_twitter_workload(workload_name, num_load_ops, client_id,
                          num_global_clients, wl);
  } else if (memcmp(workload_name, "wiki", strlen("wiki")) == 0) {
    load_wiki_workload(workload_name, num_load_ops, client_id, num_global_clients,
                       wl);
  } else if (memcmp(workload_name, "ibm", strlen("ibm")) == 0) {
    char fname_buf[256];
    snprintf(fname_buf, sizeof(fname_buf), "%s", workload_name);
    load_ibm_workload(fname_buf, num_load_ops, client_id, num_global_clients, wl);
  } else if (memcmp(workload_name, "cphy", strlen("cphy")) == 0) {
    char fname_buf[256];
    snprintf(fname_buf, sizeof(fname_buf), "%s", workload_name);
    load_cphy_workload(fname_buf, num_load_ops, client_id, num_global_clients, wl);
  } else if (memcmp(workload_name, "msr", strlen("msr")) == 0) {
    char fname_buf[256];
    snprintf(fname_buf, sizeof(fname_buf), "%s", workload_name);
    load_msr_workload(fname_buf, num_load_ops, client_id, num_global_clients, wl);
  } else if (memcmp(workload_name, "meta", strlen("meta")) == 0) {
    char fname_buf[256];
    snprintf(fname_buf, sizeof(fname_buf), "%s", workload_name);
    load_meta_workload(fname_buf, num_load_ops, client_id, num_global_clients, wl);
  }
  return 0;
}

// Load workload into workload list (wl), where operations and keys are stored in op_list and key_buf respectively
// load_wl consists of only Insert
// trans_wl consists of Read and Update, specifically depending on the workload
int load_workload_ycsb(const char* workload_name,
                       int num_load_ops,
                       uint32_t client_id,
                       uint32_t num_global_clients,
                       __OUT DMCWorkload* load_wl,
                       __OUT DMCWorkload* trans_wl) {
  char fname_buf[256];
  snprintf(fname_buf, sizeof(fname_buf), "%s.load", workload_name);
  load_ycsb_workload(fname_buf, num_load_ops, client_id, num_global_clients, load_wl);

  snprintf(fname_buf, sizeof(fname_buf), "%s.trans", workload_name);
  load_ycsb_workload(fname_buf, num_load_ops, client_id, num_global_clients, trans_wl);
  return 0;
}

void free_workload(DMCWorkload* wl) {
  free(wl->key_buf);
  free(wl->val_buf);
  free(wl->key_size_list);
  free(wl->val_size_list);
  free(wl->op_list);
}

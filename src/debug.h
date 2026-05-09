#ifndef _DEBUG_H_
#define _DEBUG_H_

#define L_DEBUG_VERBOSE 0
#define L_DEBUG 1
#define L_DEBUG_MM 2
#define L_DEBUG_EG 2
#define L_DEBUG_LINE 2
#define L_INFO 3
#define L_WARN 4
#define L_ERROR 5

#define STR_L_DEBUG_VERBOSE "[DEBUG_VERBOSE]"
#define STR_L_DEBUG "[DEBUG]"
#define STR_L_DEBUG_MM "[DEBUG_MM]"
#define STR_L_DEBUG_EG "[DEBUG_EG]"
#define STR_L_DEBUG_LINE "[DEBUG_LINE]"
#define STR_L_INFO "[INFO]"
#define STR_L_WARN "[WARN]"
#define STR_L_ERROR "[ERROR]"

#define VERBO L_DEBUG

#define COMPACT 0
#define ERROR_ABORT 1
#define REDIRECT_LOG 1
#define HANDLE_SIGSEGV 1

#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

extern uint64_t dbg_server_st_raddr;
extern uint64_t dbg_server_ed_raddr;
extern uint64_t dbg_server_htable_raddr;
extern uint64_t dbg_server_gmeta_raddr;
extern uint64_t dbg_server_fifo_raddr;
extern uint64_t dbg_server_free_raddr;
extern uint64_t dbg_server_devst_raddr;
extern uint64_t dbg_server_deved_raddr;

extern __thread FILE* thread_log_file;
extern void open_thread_log(const char* logFileName);
extern void close_thread_log();
void sigsegv_handler(int sig);
void setup_signal_handler();

#ifdef _DEBUG
#define printd(level, fmt, ...)                                                                                         \
  do {                                                                                                                  \
    if (level >= VERBO) {                                                                                               \
      if (COMPACT == 0) {                                                                                               \
        if (thread_log_file) {                                                                                          \
          fprintf(thread_log_file, STR_##level " %s:%d:%s():\t" fmt "\n", __FILE__, __LINE__, __func__, ##__VA_ARGS__); \
        } else {                                                                                                        \
          printf(STR_##level " %s:%d:%s():\t" fmt "\n", __FILE__, __LINE__, __func__, ##__VA_ARGS__);                   \
        }                                                                                                               \
      } else {                                                                                                          \
        if (thread_log_file) {                                                                                          \
          fprintf(thread_log_file, STR_##level " %s():\t" fmt "\n", __func__, ##__VA_ARGS__);                           \
        } else {                                                                                                        \
          printf(STR_##level " %s():\t" fmt "\n", __func__, ##__VA_ARGS__);                                             \
        }                                                                                                               \
      }                                                                                                                 \
      if (ERROR_ABORT == 1 && level == L_ERROR) {                                                                       \
        close_thread_log();                                                                                             \
        abort();                                                                                                        \
      }                                                                                                                 \
    }                                                                                                                   \
  } while (0)
#else
#define printd(level, fmt, ...)
#endif

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)
#define TODO(message)                                                     \
  do {                                                                    \
    printf("TODO: " message " at " __FILE__ ":" TOSTRING(__LINE__) "\n"); \
    abort();                                                              \
  } while (0)

#endif
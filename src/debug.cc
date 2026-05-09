#include "debug.h"

#include <execinfo.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

__thread FILE* thread_log_file = NULL;
uint64_t dbg_server_st_raddr = 0;
uint64_t dbg_server_ed_raddr = 0;
uint64_t dbg_server_htable_raddr = 0;
uint64_t dbg_server_gmeta_raddr = 0;
uint64_t dbg_server_fifo_raddr = 0;
uint64_t dbg_server_free_raddr = 0;
uint64_t dbg_server_devst_raddr = 0;
uint64_t dbg_server_deved_raddr = 0;

void open_thread_log(const char* logFileName) {
  thread_log_file = fopen(logFileName, "w");
  if (thread_log_file == NULL) {
    perror("Failed to open the log file");
    abort();
  }
}

void close_thread_log() {
  if (thread_log_file != NULL) {
    fclose(thread_log_file);
    thread_log_file = NULL;
  }
}

#define BACKTRACE_SIZE 128

void sigsegv_handler(int sig) {
#ifdef _DEBUG
#if HANDLE_SIGSEGV == 1
  void* array[BACKTRACE_SIZE];
  size_t size;
  char** strings;

  size = backtrace(array, BACKTRACE_SIZE);
  strings = backtrace_symbols(array, size);

  printd(L_DEBUG, "Backtrace:");
  if (thread_log_file) {
    for (size_t i = 0; i < size; i++) {
      fprintf(thread_log_file, "%s\n", strings[i]);
    }
  } else {
    for (size_t i = 0; i < size; i++) {
      printf("%s\n", strings[i]);
    }
  }
  free(strings);

  printd(L_ERROR, "Segmentation fault detected.");
#endif
#endif
}

void setup_signal_handler() {
#ifdef _DEBUG
#if HANDLE_SIGSEGV == 1
  struct sigaction sa;
  sa.sa_handler = sigsegv_handler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART;
  sigaction(SIGSEGV, &sa, NULL);
#endif
#endif
}

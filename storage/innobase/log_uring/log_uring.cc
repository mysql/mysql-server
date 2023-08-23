#include "log_uring/log_uring.h"
#include "log_uring/xlog.h"
#include "log_uring/duration.h"
#include <thread>
#include <iostream>
#include <atomic>
#ifdef __MYSQLD__
#include "mysql/components/services/log_builtins.h"
#include "mysqld_error.h"
#endif
bool enable_log_uring = false;
bool enable_io_stat = false;
bool disable_file_io = false;

bool is_enable_log_uring() {
  return enable_log_uring;
}

bool is_enable_io_stat() {
  return enable_io_stat;
}

bool is_disable_file_io() {
    return disable_file_io;
}

void log_stat_thread(std::atomic_bool *stop) {
  while (true) {
    if (stop->load()) {
        break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    if (stop->load()) {
        break;
    }
    if (is_enable_io_stat()) {
      auto s = log_stat_period();
      if (!s.empty()) {
  #ifdef __MYSQLD__
        LogErr(WARNING_LEVEL, ER_LOG_WAL_STAT, s.c_str());
  #else
        std::cout << s << std::endl;
  #endif
      }
    }
  }
}


void log_uring(void *ptr) {
  if (getenv("ENABLE_LOG_URING")) {
    enable_log_uring = true;
  }

  if (getenv("ENABLE_IO_STAT")) {
    enable_io_stat = true;
  }

  if (getenv("DISABLE_FILE_IO")) {
      disable_file_io = true;
  }
  log_uring_thread();
}

void log_stat(void* p) {
    log_stat_thread((std::atomic_bool *) p);
}

int log_uring_append(void *buf, size_t size) {
  return get_xlog()->append(buf, size);
}

int log_uring_sync(size_t lsn) {
  return get_xlog()->sync(lsn);
}

void log_uring_stop() {
    return get_xlog()->stop();
}

void log_uring_create(
  int num_log_file, 
  int num_uring_sqe,
  bool use_iouring
) {
  get_xlog()->init_log(
      num_log_file, 
      num_uring_sqe,
      use_iouring
  );
}

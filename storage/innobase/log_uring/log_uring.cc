#include "log_uring/log_uring.h"
#include "log_uring/xlog.h"
#include "log_uring/iouring.h"
#include "log_uring/duration.h"
#include "mysql/components/services/log_builtins.h"
#include "mysqld_error.h"
#include <thread>
#include <iostream>

bool enable_log_uring = false;
bool enable_io_stat = false;

bool is_enable_log_uring() {
  return enable_log_uring;
}

bool is_enable_io_stat() {
  return enable_io_stat;
}

void log_stat_thread() {
  while (true) {
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    if (is_enable_io_stat()) {
      auto s = log_stat_period();
  #ifdef __MYSQLD__
      LogErr(WARNING_LEVEL, ER_LOG_WAL_STAT, s.c_str());
  #else
      std::cout << s << std::endl;
  #endif
    }
  }
}


std::thread *_thread = NULL;

void create_log_stat_thread() {
  _thread = new std::thread(log_stat_thread); 
}

void log_uring(void *) {
  if (getenv("ENABLE_LOG_URING")) {
    enable_log_uring = true;
  }

  if (getenv("ENABLE_IO_STAT")) {
    enable_io_stat = true;
  }
  create_log_stat_thread();
  log_uring_thread();
}

int log_uring_append(void *buf, size_t size) {
  return get_xlog()->append(buf, size);
}

int log_uring_sync(size_t lsn) {
  return get_xlog()->sync(lsn);
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

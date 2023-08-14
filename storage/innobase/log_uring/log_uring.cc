#include "log_uring/log_uring.h"
#include "log_uring/xlog.h"
#include "log_uring/iouring.h"
#include "log_uring/duration.h"
#include "mysql/components/services/log_builtins.h"
#include "mysqld_error.h"
#include <thread>
#include <iostream>

void log_stat_thread() {
  while (true) {
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    auto s = log_stat_period();
    LogErr(WARNING_LEVEL, ER_LOG_WAL_STAT, s.c_str());
  }
}


std::thread *_thread = NULL;

void create_log_stat_thread() {
  _thread = new std::thread(log_stat_thread);
}

void log_uring(void *) {
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

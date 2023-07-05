#pragma once

#include "iouring_proc.h"
#include <thread>
#include <mutex>
#include <condition_variable>

#define MAX_FD_NUM 200
class xlog {
public:
  xlog();
  virtual ~xlog();
  int append_log(void *buf, size_t size, size_t lsn);

  int sync_lsn(size_t lsn);
private:
  int *fd_;
  int num_fd_;
  size_t max_lsn_;
  iouring_ctx_t iouring_context_;
  std::mutex mutex_;
  std::condition_variable condition_;
};
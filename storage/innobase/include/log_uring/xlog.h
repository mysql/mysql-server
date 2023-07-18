#pragma once



#include "log_uring/ptr.hpp"
#include "log_uring/event.h"
#include "log_uring/iouring.h"
#include <thread>
#include <mutex>
#include <vector>
#include <condition_variable>

#define MAX_FD_NUM 200


struct file_ctrl {
  int fd_;
  uint64_t sync_lsn_;
  uint64_t max_lsn_;
};

class xlog {
public:
  xlog();

  virtual ~xlog();
  
  void start();
  
  int append(void *buf, size_t size, size_t lsn);

  int sync(size_t lsn);


private:
  void main_loop();
  void add_event(event *e);
  int handle_event_list();
  int handle_completion(int submit);
  void handle_completion_event(event *e);
  void enqueue_sqe(event *e);
  void enqueue_sqe_write(event *e);
  void enqueue_sqe_fsync(event *e);
  void enqueue_sqe_fsync_combine();

  int num_fd_;
  std::mutex mutex_cond_;
  std::condition_variable condition_;
  size_t max_sync_lsn_;
  uint64_t max_to_sync_lsn_;


  iouring_ctx_t iouring_context_;
  std::mutex mutex_queue_;
  std::vector<event *> list_[2];
  std::vector<file_ctrl> file_;
  std::vector<int> fd_;
  uint32_t sequence_;
};


void log_iouring_thread();

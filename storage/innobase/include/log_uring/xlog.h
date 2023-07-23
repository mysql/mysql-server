#pragma once



#include "log_uring/ptr.hpp"
#include "log_uring/event.h"
#include "log_uring/iouring.h"
#include <thread>
#include <mutex>
#include <atomic>
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
  
  int append(void *buf, size_t size);

  int sync(size_t lsn);
  
  void wait_start();

private:
  void main_loop();
  void add_event(io_event *e);
  int handle_event_list();
  int handle_completion(int submit);
  void handle_completion_event(io_event *e);
  bool enqueue_sqe(io_event *e);
  bool enqueue_sqe_write(io_event *e);
  bool enqueue_sqe_fsync(io_event *e);
  bool enqueue_sqe_fsync_combine();
  
  void notify_start();


  static io_event* new_io_event(size_t size);
  static void delete_io_event(io_event*);

  std::atomic<uint64_t> next_lsn_;
  int num_fd_;
  size_t max_sync_lsn_;
  uint64_t max_to_sync_lsn_;
  std::mutex mutex_cond_;
  // condition variable to wait LSN
  std::condition_variable condition_;

  iouring_ctx_t iouring_context_;
  std::mutex mutex_queue_;
  // condition variable to wait queue size
  std::condition_variable condition_queue_;
  std::vector<io_event *> list_[2];
  std::vector<file_ctrl> file_;
  // only access by uring main loop, need not to lock
  std::vector<io_event *> prev_list;
  std::vector<int> fd_;
  uint32_t sequence_;

  std::mutex mutex_state_;
  std::condition_variable condition_state_;
  bool state_;


};


void log_iouring_thread();
xlog *get_xlog();
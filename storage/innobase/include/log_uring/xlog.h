#pragma once



#include "log_uring/ptr.hpp"
#include "log_uring/event.h"
#include "log_uring/iouring.h"
#include <thread>
#include <mutex>
#include <atomic>
#include <vector>
#include <condition_variable>


struct file_ctrl {
  int fd_;
  uint64_t sync_lsn_;
  uint64_t max_lsn_;
};


class op_duration {
  std::chrono::duration<double> duration_;
  int count;
public:
  void add(std::chrono::duration<double> duration) {
    duration_ += duration;
    count += 1;
  }
};


class xlog_op_duration {
  op_duration append_;
  op_duration sync_;

  void sync_add(std::chrono::duration<double> duration) {
    sync_.add(duration);
  }

  void append_add(std::chrono::duration<double> duration) {
    append_.add(duration);
  }
};

class xlog {
public:
  xlog();

  virtual ~xlog();
  

  void init(
    int num_log_file, 
    int num_uring_entries,
    bool use_iouring
  );

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


  size_t num_log_files_;
  size_t num_uring_entries_;
  bool use_uring_;
  bool init_;
  std::mutex mutex_init_;
  std::condition_variable cond_init_;

  std::atomic<uint64_t> next_lsn_;

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


  int sync_log_fd_;
  std::mutex sync_log_mutex_;
  std::vector<int8_t> sync_log_buf_;

  std::condition_variable sync_log_cond_;
  bool sync_log_write_;

};


void log_uring_thread();
void log_uring_create(  
  int num_log_file, 
  int num_uring_entries,
  bool use_iouring
);
xlog *get_xlog();
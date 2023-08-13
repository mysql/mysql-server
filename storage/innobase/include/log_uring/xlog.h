#pragma once



#include "log_uring/ptr.hpp"
#include "log_uring/event.h"
#include "log_uring/iouring.h"
#include "log_uring/duration.h"
#include <thread>
#include <mutex>
#include <atomic>
#include <vector>
#include <condition_variable>
#include <boost/thread/sync_bounded_queue.hpp>

struct file_ctrl {
  int fd_;
  uint64_t sync_lsn_;
  uint64_t max_lsn_;
};



class xlog {
public:
  xlog();

  virtual ~xlog();
  

  void init_log(
    int num_log_file, 
    int num_uring_sqe,
    bool use_iouring
  );

  void start();
  
  void stop();

  int append(void *buf, size_t size);

  int sync(size_t lsn);
  
  void wait_start();

  static xlog_op_duration op_duration() ;

  static void reset_duration();
private:
  // main loop run in io_uring handle thread
  void main_loop();

  // add io event to queue
  void add_event(io_event *e);

  // handle io event in queue
  int handle_event_list();
  
  // handle completion in main loop 
  int handle_completion(int submit);

  // handle an io completion event
  void handle_completion_event(io_event *e);
  
  bool enqueue_sqe(io_event *e);
  bool enqueue_sqe_write(io_event *e);
  bool enqueue_sqe_fsync(io_event *e);
  bool enqueue_sqe_fsync_combine();
  
  // uring thread notify the log service start
  void notify_start();

  // create a new io event
  static io_event* new_io_event();

  // delete the io event
  static void delete_io_event(io_event*);


  uint64_t last_lsn();

  size_t num_log_files_;
  size_t num_uring_sqe_;
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
  boost::sync_bounded_queue<io_event*> queue_;

  // only access by uring main loop, need not to lock
  std::vector<file_ctrl> file_;
  std::vector<io_event *> prev_list;
  std::vector<int> fd_;


  std::mutex mutex_state_;
  std::condition_variable condition_state_;
  bool state_;


  int sync_log_fd_;
  std::mutex sync_log_mutex_;
  std::vector<int8_t> sync_log_buf_;

  std::condition_variable sync_log_cond_;
  bool sync_log_write_;


  std::atomic_bool stopped_;
};


void log_uring_thread();



xlog *get_xlog();
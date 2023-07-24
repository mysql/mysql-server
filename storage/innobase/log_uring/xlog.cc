

#include "log_uring/xlog.h"
#include "log_uring/iouring.h"
#include "log_uring/utils.h"
#include "log_uring/define.h"
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sstream>
#include <iostream>



#define SQ_THD_IDLE 2000


xlog::xlog():
num_log_files_(NUM_LOG_FILES),
num_uring_entries_(NUM_URING_SQES),
use_uring_(USE_URING),
init_(false),
next_lsn_(0),
max_sync_lsn_(0),
max_to_sync_lsn_(0),
sequence_(0),
sync_log_fd_(0),
sync_log_write_(false) {

}

xlog::~xlog() {

}

void xlog::init(
  int num_log_file, 
  int num_uring_entries,
  bool use_iouring
) {
  num_log_files_ = num_log_file;
  num_uring_entries_ = num_uring_entries;
  use_uring_ = use_iouring;
  {
    std::scoped_lock l(mutex_init_);
    init_ = false;
    cond_init_.notify_all();
  }
}

void xlog::start() {
  if (use_uring_) {
#ifdef __URING__
  std::unique_lock l(mutex_init_);
  cond_init_.wait(l,
    [this] {
        return this->init_;
  });
  for (size_t i = 0; i < num_log_files_; i++) {
    file_ctrl ctrl;
    std::stringstream ssm;
    ssm << "wal." << i + 1 << ".redo";
    ctrl.fd_ = open(ssm.str().c_str(), O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    if (ctrl.fd_ < 0) {
      panic("open file error");
    }
    ctrl.max_lsn_ = 0;
    ctrl.sync_lsn_ = 0;
    file_.push_back(ctrl);
    fd_.push_back(ctrl.fd_);
  }
  
  memset(&iouring_context_.params, 0, sizeof(iouring_context_.params));
  iouring_context_.params.flags |= IORING_SETUP_SQPOLL;
  iouring_context_.params.sq_thread_idle = SQ_THD_IDLE;
  
  int ret = io_uring_queue_init_params(num_uring_entries_, &iouring_context_.ring, &iouring_context_.params);
  if (ret < 0) {
    panic("io_uring initialize parameter fail");
    return;
  }
  
  ret = io_uring_register_files(&iouring_context_.ring, fd_.data(), num_log_files_);
  if (ret < 0) {
    panic("io_uring register files fail");
    return;
  }
  notify_start();
  main_loop();
#endif
  } else {
    std::stringstream ssm;
    ssm << "wal.sync.redo";
    sync_log_fd_ = open(ssm.str().c_str(), O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    if (sync_log_fd_ < 0) {
      panic("open sync log file error");
    }
    notify_start();
  }
}
  
int xlog::append(void *buf, size_t size) {
  auto start = std::chrono::high_resolution_clock::now();
  uint64_t lsn = next_lsn_.fetch_add(1);

  if (use_uring_) {
    io_event* e = new_io_event(size);
    e->type_ = EVENT_TYPE_WRITE;
    e->event_.index_ = 0;
    e->event_.lsn_ = lsn;
    e->event_.buffer_.resize(size, 0);
    memcpy(e->event_.buffer_.data(), buf, size);
    
    std::unique_lock l(mutex_queue_);
    size_t n = sequence_ % 2;
    if (list_[n].size() >= num_uring_entries_) {
      condition_queue_.wait(l,
        [this, n] {
          return list_[n].size() < num_uring_entries_;
        });
    }
    add_event(e);
  } else {
    // append to memory
    std::scoped_lock l(sync_log_mutex_);
    size_t old_size = sync_log_buf_.size();
    sync_log_buf_.resize(size + old_size);
    memcpy(sync_log_buf_.data() + old_size, buf, size);
  }
  auto end = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double> diff = end - start;

  return 0;
}

int xlog::sync(size_t lsn) {
  if (use_uring_) {
    io_event* e = new_io_event(0);
    e->type_ = EVENT_TYPE_FSYNC;
    e->event_.index_ = 0;
    e->event_.lsn_ = lsn;
    
    std::unique_lock<std::mutex> l(mutex_cond_);
    add_event(e);
    if (max_to_sync_lsn_ < lsn) {
      condition_.wait(l,
        [this, lsn] {
          return this->max_sync_lsn_ >= lsn;
        });
    }
  } else {
    std::vector<int8_t> buffer;
    {
      std::unique_lock<std::mutex> l(sync_log_mutex_);
      if (sync_log_write_) {
        sync_log_cond_.wait(l, 
          [this] {
          return !this->sync_log_write_;
          });
      }


      buffer.swap(sync_log_buf_);
      if (!buffer.empty()) {
        sync_log_write_ = true;
      }
    }
    {
      if (!buffer.empty()) {
        ssize_t  size = write(sync_log_fd_, buffer.data(), buffer.size());
        if (size < 0) {
          panic("write error");
        }

        fsync(sync_log_fd_);

        std::unique_lock l(sync_log_mutex_);
        sync_log_write_ = false;
        sync_log_cond_.notify_all();
      }
    }
  }
  return 0;
}


int xlog::handle_event_list() {
  size_t list_index = sequence_ % 2;
  std::vector<io_event*> list;
  {
    std::scoped_lock l(mutex_queue_);
    max_to_sync_lsn_ = 0;
    list.reserve(num_uring_entries_ * 2);
    list.swap(list_[list_index]);
    assert(list_[list_index].empty());
    sequence_ ++ ;
    if (list_[sequence_ % 2].size() < num_uring_entries_) {
      condition_queue_.notify_all();
    }
  }
  if (!prev_list.empty()) {
    list.insert(list.begin(), prev_list.begin(), prev_list.end());
    prev_list.clear();
  }
  size_t size = list.size();
  size_t i = 0;
  for (; i < size; i++) {
    if (!enqueue_sqe(list[i])) {
      break;
    }
  }
  if (i != size) {
    list.erase(list.begin(), list.begin() + i);
    prev_list = list;
  } else {
    list.clear();
  }

  enqueue_sqe_fsync_combine();
#ifdef __URING__
  return io_uring_submit(&iouring_context_.ring);
#else
  return 0;
#endif
}

int xlog::handle_completion(int submit) {
#ifdef __URING__
  for (int i = 0; i < submit; i++) {
    iouring_cqe_t *cqe = NULL;
    int ret = io_uring_wait_cqe(&iouring_context_.ring, &cqe);
    if (ret < 0) {
      return ret;
    }

    io_event * e = (io_event*)io_uring_cqe_get_data(cqe);
    if (e) {
      handle_completion_event(e);
      delete_io_event(e);
    }
    io_uring_cqe_seen(&iouring_context_.ring, cqe);
  }

  bool notify = false;
  {
    std::unique_lock<std::mutex> l(mutex_cond_);
    if (max_sync_lsn_ < max_to_sync_lsn_) {
      max_sync_lsn_ = max_to_sync_lsn_;
      notify = true;
    }
  }
  if (notify) {
    condition_.notify_all();
  }
#endif
  return 0;
}

void xlog::handle_completion_event(io_event *e) {
  switch (e->type_)
  {
  case EVENT_TYPE_FSYNC/* constant-expression */:
    /* code */
    {
      io_write_event_t *fsync_event = &e->event_;
      file_[fsync_event->index_].sync_lsn_ = fsync_event->lsn_;
    }
    break;
  case EVENT_TYPE_WRITE:
    {
      io_write_event_t *write_event = &e->event_;
      file_[write_event->index_].max_lsn_ = write_event->lsn_;
    }
    break;
  default:
    break;
  }
}

void xlog::main_loop() {
  while (true) {
    int submit = handle_event_list();
    if (submit < 0) {
      break;
    }
    int ret = handle_completion(submit);
    if (ret < 0) {
      break;
    }
  }
}

void xlog::add_event(io_event *e) {
  list_[sequence_ % 2].push_back(e);
}

bool xlog::enqueue_sqe_write(io_event *e) {
#ifdef __URING__
  iouring_sqe_t *sqe =  io_uring_get_sqe(&iouring_context_.ring);
  if (sqe == NULL) {
    std::cerr << "write, io_uring_get_sqe failed..." << std::endl;
    return false;
  }
  io_write_event_t *io_event = &e->event_;
  size_t index = io_event->lsn_ % file_.size();
  int fd = file_[index].fd_;
  io_event->index_ = index;
  io_uring_prep_write(sqe, fd, io_event->buffer_.data(), io_event->buffer_.size(), -1);
  io_uring_sqe_set_data(sqe, e);
#endif
  return true;
}

bool xlog::enqueue_sqe_fsync(io_event *e) {
#ifdef __URING__
  io_write_event_t *io_event = &e->event_;
  uint64_t lsn = io_event->lsn_;
  if (max_to_sync_lsn_ < lsn) {
    max_to_sync_lsn_ = lsn;
  }
  delete_io_event(e);
#endif
  return true;
}

bool xlog::enqueue_sqe_fsync_combine() {
#ifdef __URING__
  uint64_t max_lsn = 0;
  size_t size = file_.size();
  for (size_t i = 0; i < size; i++) {
    if (file_[i].sync_lsn_ != file_[i].max_lsn_) {
      if (max_lsn < file_[i].max_lsn_) {
        max_lsn = file_[i].max_lsn_;
      }
      iouring_sqe_t *sqe =  io_uring_get_sqe(&iouring_context_.ring);
      if (sqe == NULL) {
        std::cerr << "fsync io_uring_get_sqe failed..." << std::endl;
        return false;
      }

      io_event *e = new_io_event(0);
      e->type_ = EVENT_TYPE_FSYNC;
      e->event_.lsn_ = file_[i].max_lsn_;
      e->event_.index_ = i;
      io_uring_prep_fsync(sqe, file_[i].fd_, 0);
      io_uring_sqe_set_data(sqe, e);
    }
  }
  if (max_lsn > max_to_sync_lsn_) {
    max_to_sync_lsn_ = max_lsn;
  }
#endif
  return true;
}

bool xlog::enqueue_sqe(io_event *e) {
  switch (e->type_) {
    case EVENT_TYPE_WRITE:
      return enqueue_sqe_write(e);
    case EVENT_TYPE_FSYNC:
      return enqueue_sqe_fsync(e);
    default:
      panic("unknown io event type");
      return false;
  }
}


void xlog::notify_start() {
  std::unique_lock l(mutex_state_);
  state_ = true;
  condition_state_.notify_all();
}

void xlog::wait_start() {
  std::unique_lock l(mutex_state_);
  if (!state_) {
    condition_state_.wait(l,
      [this] {
        return state_;
      });
  }
}


io_event* xlog::new_io_event(size_t size) {
  io_event* e = new io_event;
  return e;
}

void xlog::delete_io_event(io_event* event) {
  delete event;
}

xlog __global_xlog;

void log_uring_thread() {
  __global_xlog.start();
}

void log_uring_create(
  int num_log_file, 
  int num_uring_entries,
  bool use_iouring
) {
  __global_xlog.init(
      num_log_file, 
      num_uring_entries,
      use_iouring
  );
}

xlog *get_xlog() {
  return &__global_xlog;
}

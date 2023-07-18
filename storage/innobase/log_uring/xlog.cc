

#include "log_uring/xlog.h"
#include "log_uring/iouring.h"
#include "log_uring/utils.h"
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sstream>
#include <iostream>

#define EVENT_CAPACITY 40960
#define MAX_FILE_FD 32
#define SQ_THD_IDLE 2000
#define NUM_ENTRIES 32000



xlog::xlog():
max_sync_lsn_(0),
max_to_sync_lsn_(0),
sequence_(0)
{
  
}

xlog::~xlog() {

}

void xlog::start() {
#ifdef __URING__
  num_fd_ = MAX_FILE_FD;
  for (int i = 0; i < MAX_FILE_FD; i++) {
    file_ctrl ctrl;
    std::stringstream ssm;
    ssm << "log_" << i + 1 << ".redo";
    ctrl.fd_ = open(ssm.str().c_str(), O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    ctrl.max_lsn_ = 0;
    ctrl.sync_lsn_ = 0;
    file_.push_back(ctrl);
    fd_.push_back(ctrl.fd_);
  }
  
  memset(&iouring_context_.params, 0, sizeof(iouring_context_.params));
  iouring_context_.params.flags |= IORING_SETUP_SQPOLL;
  iouring_context_.params.sq_thread_idle = SQ_THD_IDLE;
  
  int ret = io_uring_queue_init_params(NUM_ENTRIES, &iouring_context_.ring, &iouring_context_.params);
  if (ret < 0) {
    panic("io_uring initialize parameter fail");
    return;
  }
  
  ret = io_uring_register_files(&iouring_context_.ring, fd_.data(), num_fd_);
  if (ret < 0) {
    panic("io_uring register files fail");
    return;
  }

  main_loop();
#endif
}
  
int xlog::append(void *buf, size_t size, size_t lsn) {
  event* e = (event*)malloc(sizeof(event) + size);
  e->type_ = EVENT_TYPE_WRITE;
  e->event_.write_event_.index_ = 0;
  e->event_.write_event_.lsn_ = lsn;
  e->event_.write_event_.size_ = size;
  memcpy(e->event_.write_event_.buffer_, buf, size);
  
  std::unique_lock l(mutex_queue_);
  add_event(e);
  return 0;
}

int xlog::sync(size_t lsn) {
  event* e = (event*)malloc(sizeof(event));
  e->type_ = EVENT_TYPE_FSYNC;
  e->event_.fsync_event_.index_ = 0;
  e->event_.fsync_event_.lsn_ = lsn;
  
  std::unique_lock l(mutex_cond_);
  add_event(e);
  condition_.wait(l,
    [this, lsn] {
      return this->max_sync_lsn_ >= lsn;
    });
  return 0;
}


int xlog::handle_event_list() {
#ifdef __URING__
  std::scoped_lock l(mutex_queue_);
  std::vector<event*> list;
  max_to_sync_lsn_ = 0;
  list.reserve(EVENT_CAPACITY);
  sequence_ ++ ;
  list.swap(list_[sequence_ % 2]);

  size_t size = list.size();
  for (size_t i = 0; i < size; i++) {
    enqueue_sqe(list[i]);
  }
  enqueue_sqe_fsync_combine();
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

    event * e = (event*)io_uring_cqe_get_data(cqe);
    if (e) {
      handle_completion_event(e);
      free(e);
    }
  }

  bool notify = false;
  {
    std::scoped_lock l(mutex_cond_);
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

void xlog::handle_completion_event(event *e) {
  switch (e->type_)
  {
  case EVENT_TYPE_FSYNC/* constant-expression */:
    /* code */
    {
      io_fsync_event_t *event = &e->event_.fsync_event_;
      file_[event->index_].sync_lsn_ = event->lsn_;
    }
    break;
  case EVENT_TYPE_WRITE:
    {
      io_write_event_t *event = &e->event_.write_event_;
      file_[event->index_].max_lsn_ = event->lsn_;
    }
    break;
  default:
    break;
  }
}

void xlog::main_loop() {
  std::cout << "begin xlog main loop ..." << std::endl;
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

void xlog::add_event(event *e) {
  list_[sequence_ % 2].push_back(e);
}

void xlog::enqueue_sqe_write(event *e) {
#ifdef __URING__
  iouring_sqe_t *sqe =  io_uring_get_sqe(&iouring_context_.ring);
  io_write_event_t *event = &e->event_.write_event_;
  size_t index = event->lsn_ % file_.size();
  int fd = file_[index].fd_;
  event->index_ = index;
  io_uring_prep_write(sqe, fd, event->buffer_, event->size_, -1);
  io_uring_sqe_set_data(sqe, e);
#endif
}

void xlog::enqueue_sqe_fsync(event *e) {
#ifdef __URING__
  io_fsync_event_t *event = &e->event_.fsync_event_;
  uint64_t lsn = event->lsn_;
  if (max_to_sync_lsn_ < lsn) {
    max_to_sync_lsn_ = lsn;
  }
  free(e);
#endif
}

void xlog::enqueue_sqe_fsync_combine() {
#ifdef __URING__
  uint64_t max_lsn = 0;
  size_t size = file_.size();
  for (size_t i = 0; i < size; i++) {
    if (file_[i].sync_lsn_ != file_[i].max_lsn_) {
      if (max_lsn < file_[i].max_lsn_) {
        max_lsn = file_[i].max_lsn_;
      }
      event *e = (event*)malloc(sizeof(event));
      e->type_ = EVENT_TYPE_FSYNC;
      e->event_.fsync_event_.lsn_ = file_[i].max_lsn_;
      iouring_sqe_t *sqe =  io_uring_get_sqe(&iouring_context_.ring);
      io_uring_prep_fsync(sqe, file_[i].fd_, 0);
      io_uring_sqe_set_data(sqe, e);
    }
  }
  if (max_lsn > max_to_sync_lsn_) {
    max_to_sync_lsn_ = max_lsn;
  }
#endif
}

void xlog::enqueue_sqe(event *e) {
  switch (e->type_) {
    case EVENT_TYPE_WRITE:
      enqueue_sqe_write(e);
      break;
    case EVENT_TYPE_FSYNC:
      enqueue_sqe_fsync(e);
      break;
    default:
      break;
  }
}

xlog __global_xlog;

void log_iouring_thread() {
  __global_xlog.start();
}

xlog *get_xlog() {
  return &__global_xlog;
}

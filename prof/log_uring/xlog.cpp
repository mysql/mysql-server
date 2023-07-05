#include "xlog.h"
#include "io_event.h"
#include <stdlib.h>
#include <string.h>

xlog::xlog() {
  
}

xlog::~xlog() {

}
  
int xlog::append_log(void *buf, size_t size, size_t lsn) {
  iouring_sqe_t *sqe1 =  io_uring_get_sqe(&iouring_context_.ring);
  int fd = fd_[lsn % num_fd_];
  io_event_t *event1 = (io_event_t*) malloc(sizeof(io_event_t) + size);
  event1->type = IO_EVENT_WRITE;
  event1->event.write.fd = fd;
  memcpy(event1 + sizeof(io_event_t), buf, size);
  io_uring_prep_write(sqe1, fd, buf, size, -1);
  io_uring_sqe_set_data(sqe1, event1);

  
  iouring_sqe_t *sqe2 =  io_uring_get_sqe(&iouring_context_.ring);
  io_event_t *event2 = (io_event_t*) malloc(sizeof(io_event_t));
  event2->type = IO_EVENT_FSYNC;
  event2->event.fsync.fd = fd;
  event2->event.fsync.flag = 0;
  event2->event.fsync.lsn = lsn;
  io_uring_prep_fsync(sqe2, fd, 0);
  io_uring_sqe_set_data(sqe2, event2);

  io_uring_submit(&iouring_context_.ring);
  return 0;
}

int xlog::sync_lsn(size_t lsn) {
  std::unique_lock l(mutex_);
  condition_.wait(l, 
    [this, lsn] {
      return this->max_lsn_ >= lsn;
    });
  return 0;
}
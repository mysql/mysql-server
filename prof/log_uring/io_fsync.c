#include "io_fsync.h"
#include <assert.h>

typedef struct {
  int32_t fd;
  uint32_t flag;
} ioe_fsync_t;


int io_event_fsync_enqueue(io_event_t *e, void *userdata, iouring_t *ring) {
  iouring_sqe_t *sqe = io_uring_get_sqe(ring);
  if (!sqe) {
    return -1;
  }
  assert(userdata);
  io_uring_sqe_set_data(sqe, userdata);
  io_uring_prep_fsync(sqe, e->event.fsync.fd, e->event.fsync.flag);
  return 1;
}
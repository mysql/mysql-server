#include "io_read.h"
#include <assert.h>

typedef struct {
  int fd;
  int64_t offset;
  int64_t count;
} ioe_read_t;


int io_event_read_enqueue(io_event_t *e, void *userdata, iouring_t *ring) {
  iouring_sqe_t *sqe = io_uring_get_sqe(ring);
  if (!sqe) {
    return -1;
  }
  assert(userdata);
  io_uring_sqe_set_data(sqe, userdata);
  io_uring_prep_read(sqe, e->event.read.fd, e->event.read.buf, e->event.read.count, e->event.read.offset);
  return 1;
}
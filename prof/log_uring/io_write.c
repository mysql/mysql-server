#include "io_write.h"
#include <assert.h>

typedef struct {
  int fd;
  int64_t offset;
  int64_t count;
} ioe_write_t;


int io_event_write_enqueue(io_event_t *e, void *userdata, iouring_t *ring) {
  iouring_sqe_t *sqe = io_uring_get_sqe(ring);
  if (!sqe) {
    return -1;
  }
  assert(userdata);
  io_uring_sqe_set_data(sqe, userdata);
  io_uring_prep_write(sqe, e->event.write.fd, e->event.write.buf, e->event.write.count, e->event.write.offset);
  return 1;
}
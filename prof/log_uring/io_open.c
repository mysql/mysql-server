#include "io_open.h"

typedef struct {
  int32_t fd;
} ioe_open_t;


int io_event_open_enqueue(io_event_t *e, void *userdata, iouring_t *ring) {
  io_uring_register_files(ring, &e->event.open.fd, 1);
  return 0;
}
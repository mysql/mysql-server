#include "io_close.h"

int io_event_close_enqueue(io_event_t *e, void *, iouring_t *ring) {
  close(e->event.close.fd);
  return 0;
}

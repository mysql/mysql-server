#include "io_event.h"
#include "iouring.h"

int io_event_close_enqueue(io_event_t *e, void *userdata, iouring_t *ring);
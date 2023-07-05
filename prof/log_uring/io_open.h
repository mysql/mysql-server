#include "io_event.h"
#include "iouring.h"


int io_event_open_enqueue(io_event_t *e, void *userdata, iouring_t *ring);
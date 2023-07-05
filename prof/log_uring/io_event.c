#include "io_event.h"
#include "io_read.h"
#include "io_write.h"
#include "io_open.h"
#include "io_close.h"
#include "io_fsync.h"
/*

#define IO_EVENT_CLOSE 0
#define IO_EVENT_FSYNC 1
#define IO_EVENT_OPEN 2
#define IO_EVENT_READ 3
#define IO_EVENT_WRITE 4

*/


const io_event_enqueue ENQUEUE[] = {
    io_event_close_enqueue,
    io_event_fsync_enqueue,
    io_event_open_enqueue,
    io_event_read_enqueue,
    io_event_write_enqueue
};


int io_event_handler_init(io_event_handler_t *h) {
  for (int i = 0; i < MAX_EVENT_TYPES; i++) {
    h[i].enqueue = ENQUEUE[i];
  }
  return 0;
}

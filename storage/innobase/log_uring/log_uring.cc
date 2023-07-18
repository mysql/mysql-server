#include "log_uring/log_uring.h"
#include "log_uring/xlog.h"
#include "log_uring/iouring.h"

void log_iouring(void *) {
  log_iouring_thread();
}
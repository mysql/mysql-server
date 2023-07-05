#include <string.h>
#include "utils.h"
#include "iouring_proc.h"

#define SQ_THD_IDLE 2000
#define NUM_ENTRIES 32000

int iouring_init(iouring_ctx_t *c, handle_completion_t handle,  uint32_t entries) {
  int ret = 0;
  memset(&c->params, 0, sizeof(c->params));
  c->params.flags |= IORING_SETUP_SQPOLL;
  c->params.sq_thread_idle = SQ_THD_IDLE;
  c->handle = handle;
  ret = io_uring_queue_init_params(entries, &c->ring, &c->params);
  if (ret < 0) {
    return ret;
  }

  return 0;
}


int iouring_loop(iouring_ctx_t *c) {
  int ret = 0;
  while (TRUE) {
    ret = iouring_wait_completion(c);
    if (ret < 0) {
      return ret;
    }
  }
}


int iouring_wait_completion(iouring_ctx_t *c) {
  iouring_cqe_t *cqe = NULL;
  int ret = io_uring_wait_cqe(&c->ring, &cqe);

  if (ret < 0) {
    return ret;
  }

  void *user_data = io_uring_cqe_get_data(cqe);
  if (user_data) {
    int ret = c->handle(user_data);
    if (ret < 0) {
      return ret;
    }
  } else {
    PANIC("error");
  }
  io_uring_cqe_seen(&c->ring, cqe);
  return 0;
}


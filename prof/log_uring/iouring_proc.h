#include "iouring.h"
#include "handle_completion.h"

typedef struct {
  iouring_t ring;
  iouring_params_t params;
  handle_completion_t handle;
} iouring_ctx_t;


int iouring_loop(iouring_ctx_t *);

int iouring_init(
  iouring_ctx_t *c,
  handle_completion_t handle,
  uint32_t entries
);


#pragma once

#define __URING__
#ifdef __URING__

#include <liburing.h>

typedef struct io_uring_sqe iouring_sqe_t;
typedef struct io_uring_cqe iouring_cqe_t;
typedef struct io_uring iouring_t;
typedef struct io_uring_params iouring_params_t;


typedef struct {
  iouring_t ring;
  iouring_params_t params;

} iouring_ctx_t;

#else

typedef struct {

} iouring_ctx_t;

#endif


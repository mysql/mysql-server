#pragma once

#include<stdlib.h>
#include <stdint.h>
#include "iouring.h"
#include "io_event.h"

#define IO_EVENT_CLOSE 0
#define IO_EVENT_FSYNC 1
#define IO_EVENT_OPEN 2
#define IO_EVENT_READ 3
#define IO_EVENT_WRITE 4

#define MAX_EVENT_TYPES 5


typedef
struct {
  int fd;
  int64_t offset;
  void *buf;
  int64_t count;
  int32_t ret;
} io_read_t;

typedef
struct {
  int fd;
  int64_t offset;
  void *buf;
  int64_t count;
  ssize_t ret;
} io_write_t;

typedef
struct {
  int fd;
  unsigned flag;
  int32_t ret;
  uint64_t lsn;
} io_fsync_t;

typedef
struct {
  int fd;
  int32_t ret;
} io_open_t;

typedef
struct {
  int fd;
  int32_t ret;
} io_close_t;

typedef
struct {
  int type;
  union {
    io_close_t close;
    io_open_t open;
    io_fsync_t fsync;
    io_read_t read;
    io_write_t write;
  } event;
} io_event_t;


typedef int (*io_event_enqueue)(io_event_t *, void *user_data, iouring_t *);

typedef struct {
  io_event_enqueue enqueue;
} io_event_handler_t;

int io_event_handler_init(io_event_handler_t *);

io_event_handler_t *io_event_handler_get(int h_type);



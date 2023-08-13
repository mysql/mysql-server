#pragma once

#include <stdlib.h>
#include "log_uring/define.h"

void log_uring(void *log_ptr);

int log_uring_append(void *buf, size_t size);

int log_uring_sync(size_t lsn);

void log_uring_create(  
  int num_log_file, 
  int num_uring_entries,
  bool use_iouring
);

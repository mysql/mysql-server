#pragma once

#include <stdlib.h>

void log_uring(void *log_ptr);

int log_uring_append(void *buf, size_t size);

int log_uring_sync(size_t lsn);

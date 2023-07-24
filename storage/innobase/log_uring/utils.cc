#include "log_uring/utils.h"
#include <time.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>

void panic(const char *message) {
  perror(message);
  exit(-1);
}

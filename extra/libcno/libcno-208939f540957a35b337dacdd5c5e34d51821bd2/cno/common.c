
#include <stdarg.h>
#include <stdio.h>

#include "common.h"

THREAD_LOCAL static struct cno_error_t E;

const struct cno_error_t *cno_error(void) { return &E; }

int cno_error_set(int code, const char *fmt, ...) {
  E.code = code;
  va_list vl;
  va_start(vl, fmt);
  vsnprintf(E.text, sizeof(E.text), fmt, vl);
  va_end(vl);
  return -1;
}

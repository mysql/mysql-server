
#include <stdarg.h>
#include <stdio.h>

#include "common.h"

THREAD_LOCAL static struct cno_error_t E;

const struct cno_error_t *cno_error(void) { return &E; }

static int cno_error_setv(int code, int detail, const char *fmt, va_list vl) {
  E.code = code;
  E.detail = detail;
  vsnprintf(E.text, sizeof(E.text), fmt, vl);
  return -1;
}

int cno_error_set(int code, const char *fmt, ...) {
  va_list vl;
  va_start(vl, fmt);
  cno_error_setv(code, CNO_ERROR_DETAIL_NONE, fmt, vl);
  va_end(vl);
  return -1;
}

int cno_error_set_detail(int code, int detail, const char *fmt, ...) {
  va_list vl;
  va_start(vl, fmt);
  cno_error_setv(code, detail, fmt, vl);
  va_end(vl);
  return -1;
}

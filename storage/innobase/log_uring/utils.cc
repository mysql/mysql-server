#include "log_uring/utils.h"
#include <time.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>

void panic(const char *message) {
  perror(message);
  exit(-1);
}

void log_format(const char *tag, const char *message, va_list args) {
  time_t now;
  time(&now);
  char *date = ctime(&now);
  date[strlen(date) - 1] = '\0';
  printf("%s [%s] ", date, tag);
  vprintf(message, args);
  printf("\n");
}

void log_error(const char *message, ...) {
#if LOG_LEVEL >= ERROR
  va_list args;
  va_start(args, message);
  log_format("error", message, args);
  va_end(args);
#endif
}

void log_info(const char *message, ...) {
#if LOG_LEVEL >= INFO
  va_list args;
  va_start(args, message);
  log_format("info", message, args);
  va_end(args);
#endif
}

void log_debug(const char *message, ...) {
#if LOG_LEVEL >= DEBUG
  va_list args;
  va_start(args, message);
  log_format("debug", message, args);
  va_end(args);
#endif
}

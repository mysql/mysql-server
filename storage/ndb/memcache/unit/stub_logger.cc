/*
 Copyright (c) 2011, 2017, Oracle and/or its affiliates. All rights
 reserved.
 
 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; version 2 of
 the License.
 
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 02110-1301  USA
 */
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include <memcached/extension_loggers.h>

#include "my_compiler.h"

static const char *stderror_get_name(void) {
  return "standard error";
}

static void stderror_logger_log(EXTENSION_LOG_LEVEL severity __attribute__((unused)),
                                const void* client_cookie,
                                const char *fmt, ...)
  MY_ATTRIBUTE((format(printf, 3, 4)));

static void stderror_logger_log(EXTENSION_LOG_LEVEL severity __attribute__((unused)),
                                const void* client_cookie,
                                const char *fmt, ...)
{
    int len = strlen(fmt);
    bool needlf = (len > 0 && fmt[len - 1] != '\n');
    va_list ap;
    (void)client_cookie;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    if (needlf) {
      fprintf(stderr, "\n");
    }
    fflush(stderr);
}

#ifdef __GNUC__
/*
  To fix this warning, EXTENSION_LOGGER_DESCRIPTOR would need to be fixed
  (by adding a format attribute on the "log" member) in memcached headers,
  not our code.
*/
#pragma GCC diagnostic ignored "-Wsuggest-attribute=format"
#endif

static EXTENSION_LOGGER_DESCRIPTOR stderror_logger_descriptor = {
  stderror_get_name,
  stderror_logger_log
};

EXTENSION_LOGGER_DESCRIPTOR* get_stderr_logger(void) {
  return &stderror_logger_descriptor;
}

/* Copyright (c) 2016, 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/task_debug.h"

#include <assert.h>
#include <stdarg.h>


/* purecov: begin deadcode */
static int mystrcat_core_sprintf(char *dest, int size, const char *format, va_list args)
                                 MY_ATTRIBUTE((format(printf, 3, 0)));

char *mystrcat(char *dest, int *size, const char *src) {
  int current_size = *size;
  char *ret = dest;

  int num = 0;
  while (*dest) {
    num++;
    dest++;
  }

  /* Avoids adding chars to full buffer */
  if (current_size < STR_SIZE) {
    while ((*dest++ = *src++)) {
      if (++current_size > (STR_SIZE - 1)) {
        break;
      }
    }
    *size = current_size;
    ret = --dest;
  }

  return ret;
}
/* purecov: end */


static int mystrcat_core_sprintf(char *dest, int size, const char *format,
                                 va_list args)
{

  int remaining_size = STR_SIZE - size;
  int ret = vsnprintf(dest, (size_t) remaining_size, format, args);
  if (ret > remaining_size) {
/* purecov: begin deadcode */
    fprintf(stderr,
            "ERROR: mystrcat_sprintf wasn't able to add \"%s\" to "
            "destination string! Full buffer!\n",
            format);
    ret = remaining_size;
/* purecov: end */
  }

  return ret;
}


char *mystrcat_sprintf(char *dest, int *size, const char *format, ...) {
  va_list args;
  int ret = 0;

  va_start(args, format);
  ret = mystrcat_core_sprintf(dest, *size, format, args);
  va_end(args);

  *size += ret;

  return dest + ret;
}


/* purecov: begin deadcode */
void xcom_default_log(const int64_t l, const char *msg)
{
  char buffer[STR_SIZE + 1];
  char *buf = buffer;
  int buffer_size = 0;
  buffer[0] = 0;

  buf = mystrcat(buf, &buffer_size, xcom_log_levels[l]);
  buf = mystrcat(buf, &buffer_size, msg);
  buf = mystrcat(buf, &buffer_size, NEWLINE);

  if (l < XCOM_LOG_INFO) {
    fputs(buffer, stderr);
  } else {
    fputs(buffer, stdout);
  }
}


/**
  Print the logging messages to the console. It is invoked when no debugger
  callback was set by an upper layer.
*/
void xcom_default_debug(const char *format, ...)
{
  va_list args;
  char buffer[STR_SIZE + 1];
  int buffer_size= 0;
  buffer[0]= 0;

  va_start(args, format);
  buffer_size= mystrcat_core_sprintf(buffer, buffer_size, format, args);
  va_end(args);

  mystrcat(buffer + buffer_size, &buffer_size, NEWLINE);

  fputs(buffer, stdout);
}


/**
  Check whether a debug option was enabled or not.
*/
int xcom_default_debug_check(const int64_t debug_options)
{
  return (xcom_debug_options & debug_options) != 0;
}
/* purecov: end */

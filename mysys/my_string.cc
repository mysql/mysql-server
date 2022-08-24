/* Copyright (c) 2000, 2022, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   Without limiting anything contained in the foregoing, this file,
   which is part of C Driver for MySQL (Connector/C), is also subject to the
   Universal FOSS Exception, version 1.0, a copy of which can be found at
   http://oss.oracle.com/licenses/universal-foss-exception.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/**
  @file mysys/my_string.cc
  Code for handling strings with can grow dynamically.
*/

#include <stdarg.h>
#include <string.h>
#include <sys/types.h>

#include "m_string.h"
#include "my_dbug.h"
#include "my_inttypes.h"
#include "my_sys.h"
#include "mysql/service_mysql_alloc.h"
#include "mysys/mysys_priv.h"

bool init_dynamic_string(DYNAMIC_STRING *str, const char *init_str,
                         size_t init_alloc) {
  DBUG_TRACE;

  size_t length = init_str == nullptr ? 0 : strlen(init_str);
  init_alloc = std::max(init_alloc, length + 1);

  if (!(str->str = (char *)my_malloc(key_memory_DYNAMIC_STRING, init_alloc,
                                     MYF(MY_WME))))
    return true;
  str->length = length;
  if (init_str) memcpy(str->str, init_str, length + 1);
  str->max_length = init_alloc;
  return false;
}

bool dynstr_set(DYNAMIC_STRING *str, const char *init_str) {
  uint length = 0;
  DBUG_TRACE;

  if (init_str && (length = (uint)strlen(init_str) + 1) > str->max_length) {
    str->max_length =
        std::max(str->max_length * 3 / 2, str->length + length + 1);
    if (!(str->str = (char *)my_realloc(key_memory_DYNAMIC_STRING, str->str,
                                        str->max_length, MYF(MY_WME))))
      return true;
  }
  if (init_str) {
    str->length = length - 1;
    memcpy(str->str, init_str, length);
  } else
    str->length = 0;
  return false;
}

bool dynstr_realloc(DYNAMIC_STRING *str, size_t additional_size) {
  DBUG_TRACE;

  if (!additional_size) return false;
  if (str->length + additional_size > str->max_length) {
    str->max_length =
        std::max(str->max_length * 3 / 2, str->length + additional_size + 1);
    if (!(str->str = (char *)my_realloc(key_memory_DYNAMIC_STRING, str->str,
                                        str->max_length, MYF(MY_WME))))
      return true;
  }
  return false;
}

bool dynstr_append(DYNAMIC_STRING *str, const char *append) {
  return dynstr_append_mem(str, append, (uint)strlen(append));
}

bool dynstr_append_mem(DYNAMIC_STRING *str, const char *append, size_t length) {
  char *new_ptr;
  if (str->length + length >= str->max_length) {
    size_t new_length =
        std::max(str->max_length * 3 / 2, str->length + length + 1);
    if (!(new_ptr = (char *)my_realloc(key_memory_DYNAMIC_STRING, str->str,
                                       new_length, MYF(MY_WME))))
      return true;
    str->str = new_ptr;
    str->max_length = new_length;
  }
  if (length > 0) memcpy(str->str + str->length, append, length);
  str->length += length;
  str->str[str->length] = 0; /* Safety for C programs */
  return false;
}

bool dynstr_trunc(DYNAMIC_STRING *str, size_t n) {
  str->length -= n;
  str->str[str->length] = '\0';
  return false;
}

/*
  Concatenates any number of strings, escapes any quote in the result, then
  surrounds the resulting string in another set of quotes which is finally
  appended to specified DYNAMIC_STRING.  This function is especially useful when
  building strings to be executed with the system() function.

  @param str Dynamic String which will have additional strings appended.
  @param append String to be appended.
  @param ... Optional. Additional string(s) to be appended.

  @note The final argument in the list must be NullS even if no additional
  options are passed.

  @return True = Success.
*/

static bool dynstr_append_quoted_inner(DYNAMIC_STRING *str,
                                       const char *quote_str,
                                       const uint quote_len, const char *append,
                                       va_list dirty_text) {
  bool ret = true;
  ret &= dynstr_append_mem(str, quote_str, quote_len); /* Leading quote */
  while (append != NullS) {
    const char *cur_pos = append;
    const char *next_pos = cur_pos;

    /* Search for quote in each string and replace with escaped quote */
    while (*(next_pos = strcend(cur_pos, quote_str[0])) != '\0') {
      ret &= dynstr_append_mem(str, cur_pos, (uint)(next_pos - cur_pos));
      ret &= dynstr_append_mem(str, "\\", 1);
      ret &= dynstr_append_mem(str, quote_str, quote_len);
      cur_pos = next_pos + 1;
    }
    ret &= dynstr_append_mem(str, cur_pos, (uint)(next_pos - cur_pos));
    append = va_arg(dirty_text, char *);
  }
  va_end(dirty_text);
  ret &= dynstr_append_mem(str, quote_str, quote_len); /* Trailing quote */

  return ret;
}

bool dynstr_append_os_quoted(DYNAMIC_STRING *str, const char *append, ...) {
#ifdef _WIN32
  const char *quote_str = "\"";
  const uint quote_len = 1;
#else
  const char *quote_str = "\'";
  const uint quote_len = 1;
#endif /* _WIN32 */
  va_list dirty_text;

  va_start(dirty_text, append);
  return dynstr_append_quoted_inner(str, quote_str, quote_len, append,
                                    dirty_text);
}

bool dynstr_append_quoted(DYNAMIC_STRING *str, const char *quote_str,
                          const uint quote_len, const char *append, ...) {
  va_list dirty_text;

  va_start(dirty_text, append);
  return dynstr_append_quoted_inner(str, quote_str, quote_len, append,
                                    dirty_text);
}

void dynstr_free(DYNAMIC_STRING *str) {
  my_free(str->str);
  str->str = nullptr;
}

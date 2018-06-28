/* Copyright (c) 2017, 2018, Oracle and/or its affiliates. All rights reserved.

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

#ifndef LOG_BUILTINS_IMP_H
#define LOG_BUILTINS_IMP_H

#include <stdarg.h>

#ifdef IN_DOXYGEN
#include <mysql/components/services/log_builtins.h>
#endif

/**
  This defines built-in functions for use by logging services.
  These helpers are organized into a number of APIs grouping
  related functionality.

  This file defines internals; to use the logger from a service,
  include log_builtins.h instead.

  For documentation of the individual functions, see log_builtins.cc
*/

#include <mysql/components/services/log_shared.h>

/**
  Maximum number of key/value pairs in a log event.
  May be changed or abolished later.
*/
#define LOG_ITEM_MAX 64

/**
  Iterator over the key/value pairs of a log_line.
  At present, only one iter may exist per log_line.
*/
typedef struct _log_item_iter {
  struct _log_line *ll;  ///< log_line this is the iter for
  int index;             ///< index of current key/value pair
} log_item_iter;

/**
  log_line ("log event")
*/
typedef struct _log_line {
  log_item_type_mask seen;      ///< bit field flagging item-types contained
  log_item_iter iter;           ///< iterator over key/value pairs
  int count;                    ///< number of key/value pairs ("log items")
  log_item item[LOG_ITEM_MAX];  ///< log items
} log_line;

// see include/mysql/components/services/log_builtins.h

/**
  Primitives for services to interact with the structured logger:
  functions pertaining to log_line and log_item data
*/
class log_builtins_imp {
 public: /* Service Implementations */
  static DEFINE_METHOD(int, wellknown_by_type, (log_item_type t));
  static DEFINE_METHOD(int, wellknown_by_name,
                       (const char *key, size_t length));
  static DEFINE_METHOD(log_item_type, wellknown_get_type, (uint i));
  static DEFINE_METHOD(const char *, wellknown_get_name, (uint i));

  static DEFINE_METHOD(int, item_inconsistent, (log_item * li));
  static DEFINE_METHOD(bool, item_generic_type, (log_item_type t));
  static DEFINE_METHOD(bool, item_string_class, (log_item_class c));
  static DEFINE_METHOD(bool, item_numeric_class, (log_item_class c));

  static DEFINE_METHOD(bool, item_set_int, (log_item_data * lid, longlong i));
  static DEFINE_METHOD(bool, item_set_float, (log_item_data * lid, double f));
  static DEFINE_METHOD(bool, item_set_lexstring,
                       (log_item_data * lid, const char *s, size_t s_len));
  static DEFINE_METHOD(bool, item_set_cstring,
                       (log_item_data * lid, const char *s));

  static DEFINE_METHOD(log_item_data *, item_set_with_key,
                       (log_item * li, log_item_type t, const char *key,
                        uint32 alloc));
  static DEFINE_METHOD(log_item_data *, item_set,
                       (log_item * li, log_item_type t));

  static DEFINE_METHOD(log_item_data *, line_item_set_with_key,
                       (log_line * ll, log_item_type t, const char *key,
                        uint32 alloc));
  static DEFINE_METHOD(log_item_data *, line_item_set,
                       (log_line * ll, log_item_type t));

  static DEFINE_METHOD(log_line *, line_init, ());
  static DEFINE_METHOD(void, line_exit, (log_line * ll));
  static DEFINE_METHOD(int, line_item_count, (log_line * ll));

  static DEFINE_METHOD(log_item_type_mask, line_item_types_seen,
                       (log_line * ll, log_item_type_mask m));

  static DEFINE_METHOD(log_item_iter *, line_item_iter_acquire,
                       (log_line * ll));
  static DEFINE_METHOD(void, line_item_iter_release, (log_item_iter * it));
  static DEFINE_METHOD(log_item *, line_item_iter_first, (log_item_iter * it));
  static DEFINE_METHOD(log_item *, line_item_iter_next, (log_item_iter * it));
  static DEFINE_METHOD(log_item *, line_item_iter_current,
                       (log_item_iter * it));

  static DEFINE_METHOD(int, line_submit, (log_line * ll));

  static DEFINE_METHOD(int, message, (int log_type, ...));

  static DEFINE_METHOD(int, sanitize, (log_item * li));

  static DEFINE_METHOD(const char *, errmsg_by_errcode, (int mysql_errcode));

  static DEFINE_METHOD(longlong, errcode_by_errsymbol, (const char *sym));

  static DEFINE_METHOD(const char *, label_from_prio, (int prio));

  static DEFINE_METHOD(int, open_errstream,
                       (const char *file, void **my_errstream));

  static DEFINE_METHOD(int, write_errstream,
                       (void *my_errstream, const char *buffer, size_t length));

  static DEFINE_METHOD(int, dedicated_errstream, (void *my_errstream));

  static DEFINE_METHOD(int, close_errstream, (void **my_errstream));
};

/**
  String primitives for logging services.
*/
class log_builtins_string_imp {
 public: /* Service Implementations */
  static DEFINE_METHOD(void *, malloc, (size_t len));
  static DEFINE_METHOD(char *, strndup, (const char *fm, size_t len));
  static DEFINE_METHOD(void, free, (void *ptr));
  static DEFINE_METHOD(size_t, length, (const char *s));
  static DEFINE_METHOD(char *, find_first, (const char *s, int c));
  static DEFINE_METHOD(char *, find_last, (const char *s, int c));

  static DEFINE_METHOD(int, compare,
                       (const char *a, const char *b, size_t len,
                        bool case_insensitive));

  static DEFINE_METHOD(size_t, substitutev,
                       (char *to, size_t n, const char *fmt, va_list ap))
      MY_ATTRIBUTE((format(printf, 3, 0)));

  static DEFINE_METHOD(size_t, substitute,
                       (char *to, size_t n, const char *fmt, ...))
      MY_ATTRIBUTE((format(printf, 3, 4)));
};

/**
  Temporary primitives for logging services.
*/
class log_builtins_tmp_imp {
 public: /* Service Implementations */
  static DEFINE_METHOD(size_t, notify_client,
                       (void *thd, uint severity, uint code, char *to, size_t n,
                        const char *format, ...))
      MY_ATTRIBUTE((format(printf, 6, 7)));
};

/**
  Syslog/Eventlog functions for logging services.
*/
class log_builtins_syseventlog_imp {
 public: /* Service Implementations */
  static DEFINE_METHOD(int, open, (const char *name, int option, int facility));
  static DEFINE_METHOD(int, write, (enum loglevel level, const char *msg));
  static DEFINE_METHOD(int, close, (void));
};

#endif /* LOG_BUILTINS_IMP_H */

/* Copyright (c) 2021, 2023, Oracle and/or its affiliates.

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

#ifndef KEYRING_LOG_BUILTINS_DEFINITION_INCLUDED
#define KEYRING_LOG_BUILTINS_DEFINITION_INCLUDED

#include <mysql/components/component_implementation.h>
#include <mysql/components/service_implementation.h>

#include <mysql/components/services/log_builtins.h>

namespace keyring_common {
namespace service_definition {

class Log_builtins_keyring {
 public:
  /* ================ DUMMY ================ */
  /* log_builtins */
  static DEFINE_METHOD(int, wellknown_by_type, (log_item_type)) {
    return LOG_ITEM_TYPE_NOT_FOUND;
  }
  static DEFINE_METHOD(int, wellknown_by_name, (const char *, size_t)) {
    return LOG_ITEM_TYPE_NOT_FOUND;
  }
  static DEFINE_METHOD(log_item_type, wellknown_get_type, (uint)) {
    return LOG_ITEM_END;
  }
  static DEFINE_METHOD(const char *, wellknown_get_name, (uint)) {
    return "--ERROR--";
  }
  static DEFINE_METHOD(int, item_inconsistent, (log_item *)) { return -2; }
  static DEFINE_METHOD(bool, item_generic_type, (log_item_type)) {
    return true;
  }
  static DEFINE_METHOD(bool, item_string_class, (log_item_class)) {
    return false;
  }
  static DEFINE_METHOD(bool, item_numeric_class, (log_item_class)) {
    return false;
  }

  static DEFINE_METHOD(log_item_data *, item_set_with_key,
                       (log_item *, log_item_type, const char *, uint32)) {
    return nullptr;
  }
  static DEFINE_METHOD(log_item_data *, item_set, (log_item *, log_item_type)) {
    return nullptr;
  }
  static DEFINE_METHOD(bool, item_set_float, (log_item_data *, double)) {
    return false;
  }

  static DEFINE_METHOD(int, line_item_count, (log_line *)) { return 0; }
  static DEFINE_METHOD(log_item *, line_get_output_buffer, (log_line *)) {
    return nullptr;
  }
  static DEFINE_METHOD(log_item_iter *, line_item_iter_acquire, (log_line *)) {
    return nullptr;
  }
  static DEFINE_METHOD(void, line_item_iter_release, (log_item_iter *)) {
    return;
  }
  static DEFINE_METHOD(log_item *, line_item_iter_first, (log_item_iter *)) {
    return nullptr;
  }
  static DEFINE_METHOD(log_item *, line_item_iter_next, (log_item_iter *)) {
    return nullptr;
  }
  static DEFINE_METHOD(log_item *, line_item_iter_current, (log_item_iter *)) {
    return nullptr;
  }
  static DEFINE_METHOD(int, sanitize, (log_item *)) { return -1; }
  static DEFINE_METHOD(ulonglong, parse_iso8601_timestamp,
                       (const char *, size_t)) {
    return 0;
  }
  static DEFINE_METHOD(const char *, label_from_prio, (int)) { return "Error"; }
  static DEFINE_METHOD(log_service_error, open_errstream,
                       (const char *, void **)) {
    return LOG_SERVICE_COULD_NOT_MAKE_LOG_NAME;
  }
  static DEFINE_METHOD(log_service_error, write_errstream,
                       (void *, const char *, size_t)) {
    return LOG_SERVICE_NOTHING_DONE;
  }
  static DEFINE_METHOD(int, dedicated_errstream, (void *)) { return -1; }
  static DEFINE_METHOD(log_service_error, close_errstream, (void **)) {
    return LOG_SERVICE_NOTHING_DONE;
  }
  static DEFINE_METHOD(log_service_error, reopen_errstream,
                       (const char *, void **)) {
    return LOG_SERVICE_NOTHING_DONE;
  }
  static DEFINE_METHOD(int, message, (int, ...)) { return 0; }

  /* log_builtins_string */
  static DEFINE_METHOD(char *, find_first, (const char *, int)) {
    return nullptr;
  }
  static DEFINE_METHOD(char *, find_last, (const char *, int)) {
    return nullptr;
  }
  static DEFINE_METHOD(int, compare,
                       (const char *, const char *, size_t, bool)) {
    return 0;
  }
  static DEFINE_METHOD(size_t, substitute,
                       (char *, size_t, const char *, ...)) {
    return -1;
  }
  static DEFINE_METHOD(longlong, errcode_by_errsymbol, (const char *)) {
    return 0;
  }

  /* ================ REQUIRED ================ */
  /* log_builtins */
  static DEFINE_METHOD(log_item_data *, line_item_set_with_key,
                       (log_line * ll, log_item_type t, const char *key,
                        uint32 alloc));
  static DEFINE_METHOD(log_item_data *, line_item_set,
                       (log_line * ll, log_item_type t));
  static DEFINE_METHOD(log_line *, line_init, ());
  static DEFINE_METHOD(void, line_exit, (log_line * ll));
  static DEFINE_METHOD(log_item_type_mask, line_item_types_seen,
                       (log_line * ll, log_item_type_mask m));
  static DEFINE_METHOD(bool, item_set_int, (log_item_data * lid, longlong i));
  static DEFINE_METHOD(bool, item_set_lexstring,
                       (log_item_data * lid, const char *s, size_t s_len));
  static DEFINE_METHOD(bool, item_set_cstring,
                       (log_item_data * lid, const char *s));
  static DEFINE_METHOD(int, line_submit, (log_line * ll));
  static DEFINE_METHOD(const char *, errmsg_by_errcode, (int mysql_errcode));

  /* log_builtins_string */
  static DEFINE_METHOD(void *, malloc, (size_t len));
  static DEFINE_METHOD(char *, strndup, (const char *fm, size_t len));
  static DEFINE_METHOD(void, free, (void *ptr));
  static DEFINE_METHOD(size_t, length, (const char *s));
  static DEFINE_METHOD(size_t, substitutev,
                       (char *to, size_t n, const char *fmt, va_list ap))
      MY_ATTRIBUTE((format(printf, 3, 0)));
};

}  // namespace service_definition
}  // namespace keyring_common

#define KEYRING_LOG_BUILTINS_IMPLEMENTOR(component_name)                       \
  BEGIN_SERVICE_IMPLEMENTATION(component_name, log_builtins)                   \
  keyring_common::service_definition::Log_builtins_keyring::wellknown_by_type, \
      keyring_common::service_definition::Log_builtins_keyring::               \
          wellknown_by_name,                                                   \
      keyring_common::service_definition::Log_builtins_keyring::               \
          wellknown_get_type,                                                  \
      keyring_common::service_definition::Log_builtins_keyring::               \
          wellknown_get_name,                                                  \
      keyring_common::service_definition::Log_builtins_keyring::               \
          item_inconsistent,                                                   \
      keyring_common::service_definition::Log_builtins_keyring::               \
          item_generic_type,                                                   \
      keyring_common::service_definition::Log_builtins_keyring::               \
          item_string_class,                                                   \
      keyring_common::service_definition::Log_builtins_keyring::               \
          item_numeric_class,                                                  \
      keyring_common::service_definition::Log_builtins_keyring::item_set_int,  \
      keyring_common::service_definition::Log_builtins_keyring::               \
          item_set_float,                                                      \
      keyring_common::service_definition::Log_builtins_keyring::               \
          item_set_lexstring,                                                  \
      keyring_common::service_definition::Log_builtins_keyring::               \
          item_set_cstring,                                                    \
      keyring_common::service_definition::Log_builtins_keyring::               \
          item_set_with_key,                                                   \
      keyring_common::service_definition::Log_builtins_keyring::item_set,      \
      keyring_common::service_definition::Log_builtins_keyring::               \
          line_item_set_with_key,                                              \
      keyring_common::service_definition::Log_builtins_keyring::line_item_set, \
      keyring_common::service_definition::Log_builtins_keyring::line_init,     \
      keyring_common::service_definition::Log_builtins_keyring::line_exit,     \
      keyring_common::service_definition::Log_builtins_keyring::               \
          line_item_count,                                                     \
      keyring_common::service_definition::Log_builtins_keyring::               \
          line_item_types_seen,                                                \
      keyring_common::service_definition::Log_builtins_keyring::               \
          line_get_output_buffer,                                              \
      keyring_common::service_definition::Log_builtins_keyring::               \
          line_item_iter_acquire,                                              \
      keyring_common::service_definition::Log_builtins_keyring::               \
          line_item_iter_release,                                              \
      keyring_common::service_definition::Log_builtins_keyring::               \
          line_item_iter_first,                                                \
      keyring_common::service_definition::Log_builtins_keyring::               \
          line_item_iter_next,                                                 \
      keyring_common::service_definition::Log_builtins_keyring::               \
          line_item_iter_current,                                              \
      keyring_common::service_definition::Log_builtins_keyring::line_submit,   \
      keyring_common::service_definition::Log_builtins_keyring::message,       \
      keyring_common::service_definition::Log_builtins_keyring::sanitize,      \
      keyring_common::service_definition::Log_builtins_keyring::               \
          errmsg_by_errcode,                                                   \
      keyring_common::service_definition::Log_builtins_keyring::               \
          errcode_by_errsymbol,                                                \
      keyring_common::service_definition::Log_builtins_keyring::               \
          label_from_prio,                                                     \
      keyring_common::service_definition::Log_builtins_keyring::               \
          parse_iso8601_timestamp,                                             \
      keyring_common::service_definition::Log_builtins_keyring::               \
          open_errstream,                                                      \
      keyring_common::service_definition::Log_builtins_keyring::               \
          write_errstream,                                                     \
      keyring_common::service_definition::Log_builtins_keyring::               \
          dedicated_errstream,                                                 \
      keyring_common::service_definition::Log_builtins_keyring::               \
          close_errstream,                                                     \
      keyring_common::service_definition::Log_builtins_keyring::               \
          reopen_errstream                                                     \
          END_SERVICE_IMPLEMENTATION()

#define KEYRING_LOG_BUILTINS_STRING_IMPLEMENTOR(component_name)              \
  BEGIN_SERVICE_IMPLEMENTATION(component_name, log_builtins_string)          \
  keyring_common::service_definition::Log_builtins_keyring::malloc,          \
      keyring_common::service_definition::Log_builtins_keyring::strndup,     \
      keyring_common::service_definition::Log_builtins_keyring::free,        \
      keyring_common::service_definition::Log_builtins_keyring::length,      \
      keyring_common::service_definition::Log_builtins_keyring::find_first,  \
      keyring_common::service_definition::Log_builtins_keyring::find_last,   \
      keyring_common::service_definition::Log_builtins_keyring::compare,     \
      keyring_common::service_definition::Log_builtins_keyring::substitutev, \
      keyring_common::service_definition::Log_builtins_keyring::substitute   \
      END_SERVICE_IMPLEMENTATION()

#endif /* KEYRING_LOG_BUILTINS_DEFINITION_INCLUDED */

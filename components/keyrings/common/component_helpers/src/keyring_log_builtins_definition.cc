/* Copyright (c) 2021, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include <time.h>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <locale>
#include <memory>
#include <sstream>

#include <string_with_len.h>

#include <components/keyrings/common/component_helpers/include/keyring_log_builtins_definition.h>
/** Error structure */
typedef struct {
  const char *name;
  int errnr;
  const char *text;

  /* SQLSTATE */
  const char *odbc_state;
  const char *jdbc_state;
  int error_index;
} st_error;

/** Error info - generated from error message file */
static st_error global_error_names[] = {
#ifndef IN_DOXYGEN
#include "mysqld_ername.h"
#endif /* IN_DOXYGEN */
    {nullptr, 0, nullptr, nullptr, nullptr, 0}};

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
  log_item output_buffer;       ///< buffer a service can return its output in
  int count;                    ///< number of key/value pairs ("log items")
  log_item item[LOG_ITEM_MAX];  ///< log items
} log_line;

/**
 Pre-defined "well-known" keys, as opposed to ad hoc ones,
 for key/value pairs in logging.
*/
typedef struct _log_item_wellknown_key {
  const char *name;           ///< key name
  size_t name_len;            ///< length of key's name
  log_item_class item_class;  ///< item class (float/int/string)
  log_item_type item_type;    ///< exact type, 1:1 relationship with name
} log_item_wellknown_key;

/** Required items and their type - See LogComponentErr */
static constexpr log_item_wellknown_key log_item_wellknown_keys[] = {
    {STRING_WITH_LEN("prio"), LOG_INTEGER, LOG_ITEM_LOG_PRIO},
    {STRING_WITH_LEN("err_code"), LOG_INTEGER, LOG_ITEM_SQL_ERRCODE},
    {STRING_WITH_LEN("subsystem"), LOG_CSTRING, LOG_ITEM_SRV_SUBSYS},
    {STRING_WITH_LEN("component"), LOG_CSTRING, LOG_ITEM_SRV_COMPONENT},
    {STRING_WITH_LEN("source_line"), LOG_INTEGER, LOG_ITEM_SRC_LINE},
    {STRING_WITH_LEN("source_file"), LOG_CSTRING, LOG_ITEM_SRC_FILE},
    {STRING_WITH_LEN("function"), LOG_CSTRING, LOG_ITEM_SRC_FUNC},
    {STRING_WITH_LEN("msg"), LOG_CSTRING, LOG_ITEM_LOG_MESSAGE}};

static uint log_item_wellknown_keys_count =
    (sizeof(log_item_wellknown_keys) / sizeof(log_item_wellknown_key));

/** Check if we know about the item type */
static int log_item_wellknown_by_type(log_item_type t) {
  uint c;
  // optimize and safeify lookup
  for (c = 0; (c < log_item_wellknown_keys_count); c++) {
    if (log_item_wellknown_keys[c].item_type == t) return c;
  }
  return LOG_ITEM_TYPE_NOT_FOUND;
}

/**
 Convenience function: Derive a log label ("error", "warning",
 "information") from a severity.

 @param   prio       the severity/prio in question

 @return             a label corresponding to that priority.
 @retval  "System"   for prio of SYSTEM_LEVEL
 @retval  "Error"    for prio of ERROR_LEVEL
 @retval  "Warning"  for prio of WARNING_LEVEL
 @retval  "Note"     for prio of INFORMATION_LEVEL
*/
static const char *log_label_from_prio(int prio) {
  switch (prio) {
    case SYSTEM_LEVEL:
      return "System";
    case ERROR_LEVEL:
      return "Error";
    case WARNING_LEVEL:
      return "Warning";
    case INFORMATION_LEVEL:
      return "Note";
    default:
      return "Error";
  }
}

/**
  Base for line_item_set[_with_key]. Stripped down version of that in
  log_builtins.cc.
*/
static log_item_data *kr_line_item_set_with_key(log_line *ll, log_item_type t,
                                                const char *k [[maybe_unused]],
                                                uint32 alloc) {
  if ((ll == nullptr) || (ll->count >= LOG_ITEM_MAX)) return nullptr;

  log_item *li = &(ll->item[ll->count++]);
  const int c = log_item_wellknown_by_type(t);

  li->alloc = alloc;
  /*
    keyring does not currently support generic types, so we always take
    the key from the well-known array to be sure to use the canonical form.
  */
  li->key = log_item_wellknown_keys[c].name;
  if ((li->item_class = log_item_wellknown_keys[c].item_class) == LOG_CSTRING)
    li->item_class = LOG_LEX_STRING;
  ll->seen |= (li->type = t);
  return &li->data;
}

/**
  Release any of key and value on a log-item that were dynamically allocated.

  @param  li  log-item to release the payload of
*/
static void kr_log_item_free(log_item *li) {
  const char *fa;
  if ((li->alloc & LOG_ITEM_FREE_VALUE) && (li->item_class == LOG_LEX_STRING) &&
      ((fa = const_cast<char *>(li->data.data_string.str)) != nullptr)) {
    delete[] fa;
    li->alloc &= ~LOG_ITEM_FREE_VALUE;
  }
}

/**
  Release all log line items (key/value pairs) in log line ll.
  This frees whichever keys and values were dynamically allocated.

  @param         ll    log_line
*/
static void kr_log_line_item_free_all(log_line *ll) {
  while (ll->count > 0) kr_log_item_free(&(ll->item[--ll->count]));
  ll->seen = LOG_ITEM_END;
}

namespace keyring_common::service_definition {

/* log_builtins */
DEFINE_METHOD(log_item_data *, Log_builtins_keyring::line_item_set_with_key,
              (log_line * ll, log_item_type t, const char *key, uint32 alloc)) {
  return kr_line_item_set_with_key(ll, t, key, alloc);
}

DEFINE_METHOD(log_item_data *, Log_builtins_keyring::line_item_set,
              (log_line * ll, log_item_type t)) {
  return kr_line_item_set_with_key(ll, t, nullptr, LOG_ITEM_FREE_NONE);
}

DEFINE_METHOD(log_line *, Log_builtins_keyring::line_init, ()) {
  auto *ll = new log_line();
  if (ll != nullptr) memset(ll, 0, sizeof(log_line));
  return ll;
}

DEFINE_METHOD(void, Log_builtins_keyring::line_exit, (log_line * ll)) {
  delete ll;
}

DEFINE_METHOD(log_item_type_mask, Log_builtins_keyring::line_item_types_seen,
              (log_line * ll, log_item_type_mask m)) {
  return (ll != nullptr) ? (ll->seen & m) : 0;
}

DEFINE_METHOD(bool, Log_builtins_keyring::item_set_int,
              (log_item_data * lid, longlong i)) {
  if (lid != nullptr) {
    lid->data_integer = i;
    return false;
  }
  return true;
}

DEFINE_METHOD(bool, Log_builtins_keyring::item_set_lexstring,
              (log_item_data * lid, const char *s, size_t s_len)) {
  if (lid != nullptr) {
    lid->data_string.str = (s == nullptr) ? "" : s;
    lid->data_string.length = s_len;
    return false;
  }
  return true;
}

DEFINE_METHOD(bool, Log_builtins_keyring::item_set_cstring,
              (log_item_data * lid, const char *s)) {
  if (lid != nullptr) {
    lid->data_string.str = (s == nullptr) ? "" : s;
    lid->data_string.length = strlen(lid->data_string.str);
    return false;
  }
  return true;
}

DEFINE_METHOD(int, Log_builtins_keyring::line_submit, (log_line * ll)) {
  if (ll->count > 0) {
    log_item_type item_type;
    int out_fields = 0;
    const char *label = "Error";
    size_t label_len = strlen(label);
    enum loglevel prio;
    unsigned int errcode = 0;
    const char *msg = "";
    size_t msg_len = 0;
    char *line_buffer = nullptr;
    bool have_message = false;
    for (int c = 0; c < ll->count; ++c) {
      item_type = ll->item[c].type;
      ++out_fields;
      switch (item_type) {
        case LOG_ITEM_LOG_PRIO:
          prio = (enum loglevel)ll->item[c].data.data_integer;
          label = log_label_from_prio(prio);
          label_len = strlen(label);
          break;
        case LOG_ITEM_SQL_ERRCODE:
          errcode = (unsigned int)ll->item[c].data.data_integer;
          break;
        case LOG_ITEM_LOG_MESSAGE: {
          have_message = true;
          msg = ll->item[c].data.data_string.str;
          msg_len = ll->item[c].data.data_string.length;
          if (memchr(msg, '\n', msg_len) != nullptr) {
            delete[] line_buffer;
            line_buffer = new char[msg_len + 1]();
            if (line_buffer == nullptr) {
              msg =
                  "The submitted error message contains a newline, "
                  "and a buffer to sanitize it for the traditional "
                  "log could not be allocated.";
              msg_len = strlen(msg);
            } else {
              memcpy(line_buffer, msg, msg_len);
              line_buffer[msg_len] = '\0';
              char *nl2 = line_buffer;
              while ((nl2 = strchr(nl2, '\n')) != nullptr) *(nl2++) = ' ';
              msg = line_buffer;
            }
          }
          break;
        }
        default:
          --out_fields;
      }
    }

    if (have_message) {
      char internal_buff[LOG_BUFF_MAX];
      constexpr size_t buff_size = sizeof(internal_buff);
      char *buff_line = internal_buff;

      constexpr char format[] = "%Y-%m-%d %X";
      const time_t t(time(nullptr));
      const tm tm(*localtime(&t));

      constexpr size_t date_length{50};
      const std::unique_ptr<char[]> date{new char[date_length]};
      strftime(date.get(), date_length, format, &tm);

      const std::string time_string = date.get();

      (void)snprintf(buff_line, buff_size, "%s [%.*s] [MY-%06u] [Keyring] %.*s",
                     time_string.c_str(), (int)label_len, label, errcode,
                     (int)msg_len, msg);
      std::cout << buff_line << std::endl;
      delete[] line_buffer;
      kr_log_line_item_free_all(ll);
      return out_fields;
    }
    kr_log_line_item_free_all(ll);
    return 0;
  }
  return 0;
}

DEFINE_METHOD(const char *, Log_builtins_keyring::errmsg_by_errcode,
              (int mysql_errcode)) {
  st_error *tmp_error;
  const char *errtxt{"Unknown error"};

  tmp_error = &global_error_names[0];

  while (tmp_error->name != nullptr) {
    if (tmp_error->errnr == mysql_errcode) {
      errtxt = tmp_error->text;
      break;
    }
    tmp_error++;
  }
  return errtxt;
}

/* log_builtins_string */
DEFINE_METHOD(void *, Log_builtins_keyring::malloc, (size_t len)) {
  return new char[len + 1]();
}

DEFINE_METHOD(char *, Log_builtins_keyring::strndup,
              (const char *fm, size_t len)) {
  char *ptr = new char[len + 1]();
  if (ptr != nullptr) {
    memcpy(ptr, fm, len);
    ptr[len] = 0;
  }
  return ptr;
}

DEFINE_METHOD(void, Log_builtins_keyring::free, (void *ptr)) {
  if (ptr != nullptr) {
    const char *mem = (const char *)ptr;
    delete[] mem;
  }
}

DEFINE_METHOD(size_t, Log_builtins_keyring::length, (const char *s)) {
  return strlen(s);
}

DEFINE_METHOD(size_t, Log_builtins_keyring::substitutev,
              (char *to, size_t n, const char *fmt, va_list ap)) {
  return vsnprintf(to, n, fmt, ap);
}

}  // namespace keyring_common::service_definition

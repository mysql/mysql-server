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

/*
  NB  This module has an unusual amount of failsafes, OOM checks, and
      so on as it implements a public API. This makes a fair number
      of minor code paths cases of "we should never get here (unless
      someone's going out of their way to break to API)". :)
*/

#define LOG_SUBSYSTEM_TAG "Server"

#include "log_builtins_filter_imp.h"
#include "log_builtins_imp.h"  // internal structs
                               // connection_events_loop_aborted()

#include "my_dir.h"

#include <mysql/components/services/log_service.h>
#include <mysql/components/services/log_shared.h>  // data types

#include "registry.h"  // mysql_registry_imp
#include "server_component.h"
#include "sql/current_thd.h"  // current_thd
#include "sql/log.h"          // make_iso8601_timestamp, log_write_errstream,
                              // log_get_thread_id, mysql_errno_to_symbol,
                              // mysql_symbol_to_errno, log_vmessage,
                              // error_message_for_error_log, LogVar
#include "sql/mysqld.h"       // opt_log_(timestamps|error_services),
#include "sql/sql_class.h"    // THD

// Must come after sql/log.h.
#include "mysql/components/services/log_builtins.h"

#ifndef _WIN32
#include <syslog.h>
#else
#include <stdio.h>

#include "my_sys.h"

extern CHARSET_INFO my_charset_utf16le_bin;  // used in Windows EventLog
static HANDLE hEventLog = NULL;              // global
#define MSG_DEFAULT 0xC0000064L
#endif

extern log_filter_ruleset *log_filter_builtin_rules;  // what it says on the tin

PSI_memory_key key_memory_log_error_loaded_services;
PSI_memory_key key_memory_log_error_stack;

using std::string;
using std::unique_ptr;

struct log_service_cache_entry;

struct log_service_cache_entry_free {
  /**
    Release an entry in the hash of log services.

    @param       sce     the entry to free
  */
  void operator()(log_service_cache_entry *sce) const;
};

/**
  We're caching handles to the services used in error logging
  as looking them up is costly.
*/
using cache_entry_with_deleter =
    unique_ptr<log_service_cache_entry, log_service_cache_entry_free>;
static collation_unordered_map<string, cache_entry_with_deleter>
    *log_service_cache;

/**
  Lock for the log "stack" (i.e. the list of active log-services).
  X-locked while stack is changed/configured.
  S-locked while stack is used.
*/
static mysql_rwlock_t THR_LOCK_log_stack;

/**
  Make sure only one instance of syslog/Eventlog code runs at a time.
*/
static mysql_mutex_t THR_LOCK_log_syseventlog;

/**
  Subsystem initialized and ready to use?
*/
static int log_builtins_inited = 0;

/**
  Variable listeners: check or update?
*/
static const char *LOG_VAR_KEY_CHECK = "check";

/**
  Name of the interface that log-services implements.
*/
#define LOG_SERVICES_PREFIX "log_service"

/**
  Name of internal filtering engine (so we may recognize it when the
  user refers to it by name in log_error_services).
*/
#define LOG_BUILTINS_FILTER "log_filter_internal"

/**
  Name of internal log writer (so we may recognize it when the user
  refers to it by name in log_error_services).
*/
#define LOG_BUILTINS_SINK "log_sink_internal"

/**
  We have a built-in default filtering engine, and a built-in
  log-writer (the "classic" MySQL server log). These are available
  even without the component framework (before it's initialized,
  or if it somehow fails). Since these two are special, we need
  to be able to identify them.
*/
typedef enum enum_log_service_builtin_type {
  LOG_SERVICE_BUILTIN_TYPE_NONE = 0,    ///< not a built-in service
  LOG_SERVICE_BUILTIN_TYPE_FILTER = 1,  ///< built-in filtering engine
  LOG_SERVICE_BUILTIN_TYPE_SINK = 2     ///< built-in default writer
} log_service_builtin_type;

/**
  Finding and acquiring a service in the component framework is
  expensive, and we may use services a log (depending on how many
  events are logged per second), so we cache the relevant data.
  This struct describes a given service.
*/
struct log_service_cache_entry {
  char *name;                     ///< name of this service
  size_t name_len;                ///< service-name's length
  my_h_service service;           ///< handle (service framework)
  int opened;                     ///< currently open instances
  int requested;                  ///< requested instances
  bool multi_open;                ///< multi-open supported?
  log_service_builtin_type type;  ///< regular, builtin filter/sink
};

/**
  State of a given instance of a service. A service may support being
  opened several times.
*/
typedef struct _log_service_instance {
  log_service_cache_entry *sce;        ///< the service in question
  void *instance;                      ///< instance handle (multi-open)
  struct _log_service_instance *next;  ///< next instance (any service)
} log_service_instance;

static log_service_instance *log_service_instances = nullptr;  ///< anchor

/**
  An error-stream.
  Rather than implement its own file operations, a log-service may use
  convenience functions defined in this file. These functions use the
  log_errstream struct to describe their log-files. These structs are
  opaque to the log-services.
*/
struct log_errstream {
  FILE *file{nullptr};           ///< file to log to
  mysql_mutex_t LOCK_errstream;  ///< lock for logging
};

/**
  Test whether a given log-service name refers to a built-in
  service (built-in filter or built-in sink at this point).

  @param  name   the name -- either just the component's, or
                 a fully qualified service.component
  @retval        LOG_SERVICE_BUILTIN_TYPE_(FILTER|SINK|NONE)
*/
log_service_builtin_type log_service_builtin(char *name) {
  if ((strlen(name) > sizeof(LOG_SERVICES_PREFIX)) && (name[11] == '.') &&
      (0 == strncmp(name, LOG_SERVICES_PREFIX, 11)))
    name += (sizeof(LOG_SERVICES_PREFIX));

  if (0 == strncmp(name, LOG_BUILTINS_FILTER, sizeof(LOG_BUILTINS_FILTER)))
    return LOG_SERVICE_BUILTIN_TYPE_FILTER;
  if (0 == strncmp(name, LOG_BUILTINS_SINK, sizeof(LOG_BUILTINS_SINK)))
    return LOG_SERVICE_BUILTIN_TYPE_SINK;
  return LOG_SERVICE_BUILTIN_TYPE_NONE;
}

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

/**
  We support a number of predefined keys, such as "error-code" or
  "message".  These are defined here.  We also support user-defined
  "ad hoc" (or "generic") keys that let users of the error stack
  add values with arbitrary keys (as long as those keys don't coincide
  with the wellknown ones, anyway).

  The idea here is that we want the flexibility of arbitrary keys,
  while being able to do certain optimizations for the common case.
  This also allows us to relatively cheaply add some convenience
  features, e.g. we know that error symbol ("ER_STARTUP") and
  error code (1451) are related, and can supply one as the other
  is submitted.  Likewise of course, we can use the error code to
  fetch the associated registered error message string for that
  error code.  Et cetera!
*/
static const log_item_wellknown_key log_item_wellknown_keys[] = {
    {C_STRING_WITH_LEN("--ERROR--"), LOG_UNTYPED, LOG_ITEM_END},
    {C_STRING_WITH_LEN("log_type"), LOG_INTEGER, LOG_ITEM_LOG_TYPE},
    {C_STRING_WITH_LEN("err_code"), LOG_INTEGER, LOG_ITEM_SQL_ERRCODE},
    {C_STRING_WITH_LEN("err_symbol"), LOG_CSTRING, LOG_ITEM_SQL_ERRSYMBOL},
    {C_STRING_WITH_LEN("SQL_state"), LOG_CSTRING, LOG_ITEM_SQL_STATE},
    {C_STRING_WITH_LEN("OS_errno"), LOG_INTEGER, LOG_ITEM_SYS_ERRNO},
    {C_STRING_WITH_LEN("OS_errmsg"), LOG_CSTRING, LOG_ITEM_SYS_STRERROR},
    {C_STRING_WITH_LEN("source_file"), LOG_CSTRING, LOG_ITEM_SRC_FILE},
    {C_STRING_WITH_LEN("source_line"), LOG_INTEGER, LOG_ITEM_SRC_LINE},
    {C_STRING_WITH_LEN("function"), LOG_CSTRING, LOG_ITEM_SRC_FUNC},
    {C_STRING_WITH_LEN("subsystem"), LOG_CSTRING, LOG_ITEM_SRV_SUBSYS},
    {C_STRING_WITH_LEN("component"), LOG_CSTRING, LOG_ITEM_SRV_COMPONENT},
    {C_STRING_WITH_LEN("user"), LOG_LEX_STRING, LOG_ITEM_MSC_USER},
    {C_STRING_WITH_LEN("host"), LOG_LEX_STRING, LOG_ITEM_MSC_HOST},
    {C_STRING_WITH_LEN("thread"), LOG_INTEGER, LOG_ITEM_SRV_THREAD},
    {C_STRING_WITH_LEN("query_id"), LOG_INTEGER, LOG_ITEM_SQL_QUERY_ID},
    {C_STRING_WITH_LEN("table"), LOG_CSTRING, LOG_ITEM_SQL_TABLE_NAME},
    {C_STRING_WITH_LEN("prio"), LOG_INTEGER, LOG_ITEM_LOG_PRIO},
    {C_STRING_WITH_LEN("label"), LOG_CSTRING, LOG_ITEM_LOG_LABEL},
    {C_STRING_WITH_LEN("verbatim"), LOG_CSTRING, LOG_ITEM_LOG_VERBATIM},
    {C_STRING_WITH_LEN("msg"), LOG_CSTRING, LOG_ITEM_LOG_MESSAGE},
    {C_STRING_WITH_LEN("msg_id"), LOG_INTEGER, LOG_ITEM_LOG_LOOKUP},
    {C_STRING_WITH_LEN("time"), LOG_CSTRING, LOG_ITEM_LOG_TIMESTAMP},
    {C_STRING_WITH_LEN("and_n_more"), LOG_INTEGER, LOG_ITEM_LOG_SUPPRESSED},
    /*
      We should never see the following key names in normal operations
      (but see the user-specified key instead).  These have entries all
      the same, covering the entirety of log_item_type, so we can use the
      usual mechanisms for type-to-class mapping etc.
      We could set the names to nullptr, but they're not much overhead, add
      readability, and allow for easily creating debug info of the form,
      "%s:%s=\"%s\"", wellknown_name, item->key, item->value
    */
    {C_STRING_WITH_LEN("misc_float"), LOG_FLOAT, LOG_ITEM_GEN_FLOAT},
    {C_STRING_WITH_LEN("misc_integer"), LOG_INTEGER, LOG_ITEM_GEN_INTEGER},
    {C_STRING_WITH_LEN("misc_string"), LOG_LEX_STRING, LOG_ITEM_GEN_LEX_STRING},
    {C_STRING_WITH_LEN("misc_cstring"), LOG_CSTRING, LOG_ITEM_GEN_CSTRING}};

static uint log_item_wellknown_keys_count =
    (sizeof(log_item_wellknown_keys) / sizeof(log_item_wellknown_key));

/*
  string helpers
*/

/**
  Compare two NUL-terminated byte strings

  Note that when comparing without length limit, the long string
  is greater if they're equal up to the length of the shorter
  string, but the shorter string will be considered greater if
  its "value" up to that point is greater:

  compare 'abc','abcd':      -100  (longer wins if otherwise same)
  compare 'abca','abcd':       -3  (higher value wins)
  compare 'abcaaaaa','abcd':   -3  (higher value wins)

  @param  a                 the first string
  @param  b                 the second string
  @param  len               compare at most this many characters --
                            0 for no limit
  @param  case_insensitive  ignore upper/lower case in comparison

  @retval -1                a < b
  @retval  0                a == b
  @retval  1                a > b
*/
int log_string_compare(const char *a, const char *b, size_t len,
                       bool case_insensitive) {
  if (a == nullptr) /* purecov: begin inspected */
    return (b == nullptr) ? 0 : -1;
  else if (b == nullptr)
    return 1;        /* purecov: end */
  else if (len < 1)  // no length limit for comparison
  {
    return case_insensitive ? native_strcasecmp(a, b) : strcmp(a, b);
  }

  return case_insensitive ? native_strncasecmp(a, b, len) : strncmp(a, b, len);
}

/*
  log item helpers
*/

/**
  Predicate used to determine whether a type is generic
  (generic string, generic float, generic integer) rather
  than a well-known type.

  @param t          log item type to examine

  @retval  true     if generic type
  @retval  false    if wellknown type
*/
bool log_item_generic_type(log_item_type t) {
  return (t & (LOG_ITEM_GEN_CSTRING | LOG_ITEM_GEN_LEX_STRING |
               LOG_ITEM_GEN_INTEGER | LOG_ITEM_GEN_FLOAT));
}

/**
  Predicate used to determine whether a class is a string
  class (C-string or Lex-string).

  @param c          log item class to examine

  @retval   true    if of a string class
  @retval   false   if not of a string class
*/
bool log_item_string_class(log_item_class c) {
  return ((c == LOG_CSTRING) || (c == LOG_LEX_STRING));
}

/**
  Predicate used to determine whether a class is a numeric
  class (integer or float).

  @param c         log item class to examine

  @retval   true   if of a numeric class
  @retval   false  if not of a numeric class
*/
bool log_item_numeric_class(log_item_class c) {
  return ((c == LOG_INTEGER) || (c == LOG_FLOAT));
}

/**
  Get an integer value from a log-item of float or integer type.

  @param      li   log item to get the value from
  @param[out] i    longlong to store  the value in
*/
void log_item_get_int(log_item *li, longlong *i) /* purecov: begin inspected */
{
  if (li->item_class == LOG_FLOAT)
    *i = (longlong)li->data.data_float;
  else
    *i = (longlong)li->data.data_integer;
} /* purecov: end */

/**
  Get a float value from a log-item of float or integer type.

  @param       li      log item to get the value from
  @param[out]  f       float to store  the value in
*/
void log_item_get_float(log_item *li, double *f) {
  if (li->item_class == LOG_FLOAT)
    *f = (float)li->data.data_float;
  else
    *f = (float)li->data.data_integer;
}

/**
  Get a string value from a log-item of C-string or Lex string type.

  @param li            log item to get the value from
  @param[out]  str     char-pointer   to store the pointer to the value in
  @param[out]  len     size_t pointer to store the length of  the value in
*/
void log_item_get_string(log_item *li, char **str, size_t *len) {
  if ((*str = (char *)li->data.data_string.str) == nullptr)
    *len = 0;
  else if (li->item_class & LOG_CSTRING)
    *len = strlen(li->data.data_string.str);
  else
    *len = li->data.data_string.length;
}

/**
  See whether a string is a wellknown field name.

  @param key     potential key starts here
  @param len     length of the string to examine

  @retval        LOG_ITEM_TYPE_RESERVED:  reserved, but not "wellknown" key
  @retval        LOG_ITEM_TYPE_NOT_FOUND: key not found
  @retval        >0:                      index in array of wellknowns
*/
int log_item_wellknown_by_name(const char *key, size_t len) {
  uint c;
  // optimize and safeify lookup
  for (c = 0; (c < log_item_wellknown_keys_count); c++) {
    if ((log_item_wellknown_keys[c].name_len == len) &&
        (0 == native_strncasecmp(log_item_wellknown_keys[c].name, key, len))) {
      if (log_item_generic_type(log_item_wellknown_keys[c].item_type) ||
          (log_item_wellknown_keys[c].item_type == LOG_ITEM_END))
        return LOG_ITEM_TYPE_RESERVED;
      return c;
    }
  }
  return LOG_ITEM_TYPE_NOT_FOUND;
}

/**
  See whether a type is wellknown.

  @param t       log item type to examine

  @retval        LOG_ITEM_TYPE_NOT_FOUND: key not found
  @retval        >0:                      index in array of wellknowns
*/
int log_item_wellknown_by_type(log_item_type t) {
  uint c;
  // optimize and safeify lookup
  for (c = 0; (c < log_item_wellknown_keys_count); c++) {
    if (log_item_wellknown_keys[c].item_type == t) return c;
  }
  DBUG_PRINT("warning", ("wellknown_by_type: type %d is not well-known."
                         " Or, you know, known.",
                         t));
  return LOG_ITEM_TYPE_NOT_FOUND;
}

/**
  Accessor: from a record describing a wellknown key, get its name

  @param   idx  index in array of wellknowns, see log_item_wellknown_by_...()

  @retval       name (NTBS)
*/
const char *log_item_wellknown_get_name(uint idx) {
  return log_item_wellknown_keys[idx].name;
}

/**
  Accessor: from a record describing a wellknown key, get its type

  @param   idx  index in array of wellknowns, see log_item_wellknown_by_...()

  @retval       the log item type for the wellknown key
*/
log_item_type log_item_wellknown_get_type(uint idx) {
  return log_item_wellknown_keys[idx].item_type;
}

/**
  Accessor: from a record describing a wellknown key, get its class

  @param   idx  index in array of wellknowns, see log_item_wellknown_by_...()

  @retval       the log item class for the wellknown key
*/
log_item_class log_item_wellknown_get_class(uint idx) {
  return log_item_wellknown_keys[idx].item_class;
}

/**
  Sanity check an item.
  Certain log sinks have very low requirements with regard to the data
  they receive; they write keys as strings, and then data according to
  the item's class (string, integer, or float), formatted to the sink's
  standards (e.g. JSON, XML, ...).
  Code that has higher requirements can use this check to see whether
  the given item is of a known type (whether generic or wellknown),
  whether the given type and class agree, and whether in case of a
  well-known type, the given key is correct for that type.
  If your code generates items that don't pass this check, you should
  probably go meditate on it.

  @param  li  the log_item to check

  @retval LOG_ITEM_OK              no problems
  @retval LOG_ITEM_TYPE_NOT_FOUND  unknown item type
  @retval LOG_ITEM_CLASS_MISMATCH  item_class derived from type isn't
                                   what's set on the item
  @retval LOG_ITEM_KEY_MISMATCH    class not generic, so key should
                                   match wellknown
  @retval LOG_ITEM_STRING_NULL     class is string, pointer is nullptr
  @retval LOG_ITEM_KEY_NULL        no key set (this is legal e.g. on aux
                                   items of filter rules, but should not
                                   occur in a log_line, i.e., log_sinks are
                                   within their rights to discard such items)
*/
int log_item_inconsistent(log_item *li) {
  int w, c;

  // invalid type
  if ((w = log_item_wellknown_by_type(li->type)) == LOG_ITEM_TYPE_NOT_FOUND)
    return LOG_ITEM_TYPE_NOT_FOUND;

  // fetch expected storage class for this type
  if ((c = log_item_wellknown_keys[w].item_class) == LOG_CSTRING)
    c = LOG_LEX_STRING;

  // class and type don't match
  if (c != li->item_class) return LOG_ITEM_CLASS_MISMATCH;

  // no key set
  if (li->key == nullptr) return LOG_ITEM_KEY_NULL;

  // it's not a generic, and key and type don't match
  if (!log_item_generic_type(li->type) &&
      (0 != strcmp(li->key, log_item_wellknown_keys[w].name)))
    return LOG_ITEM_KEY_MISMATCH;

  // strings should have non-nullptr
  if ((c == LOG_LEX_STRING) && (li->data.data_string.str == nullptr))
    return LOG_ITEM_STRING_NULL;

  return LOG_ITEM_OK;
}

/**
  Release any of key and value on a log-item that were dynamically allocated.

  @param  li  log-item to release the payload of
*/
void log_item_free(log_item *li) {
  if (li->alloc & LOG_ITEM_FREE_KEY) my_free((void *)li->key);

  if (li->alloc & LOG_ITEM_FREE_VALUE) {
    if (li->item_class == LOG_LEX_STRING)
      my_free((void *)li->data.data_string.str);
    else
      DBUG_ASSERT(false);
  }

  li->alloc = LOG_ITEM_FREE_NONE;
}

/**
  Dynamically allocate and initialize a log_line.

  @retval nullptr  could not set up buffer (too small?)
  @retval other    address of the newly initialized log_line
*/
log_line *log_line_init() {
  log_line *ll;
  if ((ll = (log_line *)my_malloc(key_memory_log_error_stack, sizeof(log_line),
                                  MYF(0))) != nullptr)
    memset(ll, 0, sizeof(log_line));
  return ll;
}

/**
  Release a log_line allocated with line_init()

  @param  ll       a log_line previously allocated with line_init()
*/
void log_line_exit(log_line *ll) {
  if (ll != nullptr) my_free(ll);
}

/**
  Predicate indicating whether a log line is "willing" to accept any more
  key/value pairs.

  @param   ll     the log-line to examine

  @retval  false  if not full / if able to accept another log_item
  @retval  true   if full
*/
bool log_line_full(log_line *ll) {
  return ((ll == nullptr) || (ll->count >= LOG_ITEM_MAX));
}

/**
  How many items are currently set on the given log_line?

  @param   ll     the log-line to examine

  @retval         the number of items set
*/
int log_line_item_count(log_line *ll) { return ll->count; }

/**
  Test whether a given type is presumed present on the log line.

  @param  ll  the log_line to examine
  @param  m   the log_type to test for

  @retval  0  not present
  @retval !=0 present
*/
log_item_type_mask log_line_item_types_seen(log_line *ll,
                                            log_item_type_mask m) {
  return (ll != nullptr) ? (ll->seen & m) : 0;
}

/**
  Release log line item (key/value pair) with the index elem in log line ll.
  This frees whichever of key and value were dynamically allocated.
  This leaves a "gap" in the bag that may immediately be overwritten
  with an updated element.  If the intention is to remove the item without
  replacing it, use log_line_item_remove() instead!

  @param         ll    log_line
  @param         elem  index of the key/value pair to release
*/
void log_line_item_free(log_line *ll, size_t elem) {
  DBUG_ASSERT(ll->count > 0);
  log_item_free(&(ll->item[elem]));
}

/**
  Release all log line items (key/value pairs) in log line ll.
  This frees whichever keys and values were dynamically allocated.

  @param         ll    log_line
*/
void log_line_item_free_all(log_line *ll) {
  while (ll->count > 0) log_item_free(&(ll->item[--ll->count]));
  ll->seen = LOG_ITEM_END;
}

/**
  Release log line item (key/value pair) with the index elem in log line ll.
  This frees whichever of key and value were dynamically allocated.
  This moves any trailing items to fill the "gap" and decreases the counter
  of elements in the log line.  If the intention is to leave a "gap" in the
  bag that may immediately be overwritten with an updated element, use
  log_line_item_free() instead!

  @param         ll    log_line
  @param         elem  index of the key/value pair to release
*/
void log_line_item_remove(log_line *ll, int elem) {
  DBUG_ASSERT(ll->count > 0);

  log_line_item_free(ll, elem);

  if ((ll->count > 1) && (elem < (ll->count - 1)))
    ll->item[elem] = ll->item[ll->count - 1];

  ll->count--;
}

/**
  Find the (index of the) last key/value pair of the given name
  in the log line.

  @param         ll   log line
  @param         key  the key to look for

  @retval        -1:  none found
  @retval        -2:  invalid search-key given
  @retval        -3:  no log_line given
  @retval        >=0: index of the key/value pair in the log line
*/
int log_line_index_by_name(log_line *ll, const char *key) {
  uint32 count = ll->count;

  if (ll == nullptr) /* purecov: begin inspected */
    return -3;
  else if ((key == nullptr) || (key[0] == '\0'))
    return -2; /* purecov: end */

  /*
    As later items overwrite earlier ones, return the rightmost match!
  */
  while (count > 0) {
    if (0 == strcmp(ll->item[--count].key, key)) return count;
  }

  return -1;
}

/**
  Find the last item matching the given key in the log line.

  @param         ll   log line
  @param         key  the key to look for

  @retval        nullptr    item not found
  @retval        otherwise  pointer to the item (not a copy thereof!)
*/
log_item *log_line_item_by_name(log_line *ll, const char *key) {
  int i = log_line_index_by_name(ll, key);
  return (i < 0) ? nullptr : &ll->item[i];
}

/**
  Find the (index of the) last key/value pair of the given type
  in the log line.

  @param         ll   log line
  @param         t    the log item type to look for

  @retval        <0:  none found
  @retval        >=0: index of the key/value pair in the log line
*/
int log_line_index_by_type(log_line *ll, log_item_type t) {
  uint32 count = ll->count;

  /*
    As later items overwrite earlier ones, return the rightmost match!
  */
  while (count > 0) {
    if (ll->item[--count].type == t) return count;
  }

  return -1;
}

/**
  Find the (index of the) last key/value pair of the given type
  in the log line. This variant accepts a reference item and looks
  for an item that is of the same type (for wellknown types), or
  one that is of a generic type, and with the same key name (for
  generic types).  For example, a reference item containing a
  generic string with key "foo" will a generic string, integer, or
  float with the key "foo".

  @param         ll   log line
  @param         ref  a reference item of the log item type to look for

  @retval        <0:  none found
  @retval        >=0: index of the key/value pair in the log line
*/
int log_line_index_by_item(log_line *ll, log_item *ref) {
  uint32 count = ll->count;

  if (log_item_generic_type(ref->type)) {
    while (count > 0) {
      count--;

      if (log_item_generic_type(ll->item[count].type) &&
          (native_strcasecmp(ref->key, ll->item[count].key) == 0))
        return count;
    }
  } else {
    while (count > 0) {
      if (ll->item[--count].type == ref->type) return count;
    }
  }

  return -1;
}

/**
  Initializes a log entry for use. This simply puts it in a defined
  state; if you wish to reset an existing item, see log_item_free().

  @param  li  the log-item to initialize
*/
void log_item_init(log_item *li) { memset(li, 0, sizeof(log_item)); }

/**
  Initializes an entry in a log line for use. This simply puts it in
  a defined state; if you wish to reset an existing item, see
  log_item_free().
  This resets the element beyond the last. The element count is not
  adjusted; this is for the caller to do once it sets up a valid
  element to suit its needs in the cleared slot. Finally, it is up
  to the caller to make sure that an element can be allocated.

  @param  ll  the log-line to initialize a log_item in

  @retval     the address of the cleared log_item
*/
log_item *log_line_item_init(log_line *ll) {
  log_item_init(&ll->item[ll->count]);
  return &ll->item[ll->count];
}

/**
  Create new log item with key name "key", and allocation flags of
  "alloc" (see enum_log_item_free).
  Will return a pointer to the item's log_item_data struct for
  convenience.
  This is mostly interesting for filters and other services that create
  items that are not part of a log_line; sources etc. that intend to
  create an item for a log_line (the more common case) should usually
  use the below line_item_set_with_key() which creates an item (like
  this function does), but also correctly inserts it into a log_line.

  @param  li     the log_item to work on
  @param  t      the item-type
  @param  key    the key to set on the item.
                 ignored for non-generic types (may pass nullptr for those)
                 see alloc
  @param  alloc  LOG_ITEM_FREE_KEY  if key was allocated by caller
                 LOG_ITEM_FREE_NONE if key was not allocated
                 Allocated keys will automatically free()d when the
                 log_item is.
                 The log_item's alloc flags will be set to the
                 submitted value; specifically, any pre-existing
                 value will be clobbered.  It is therefore WRONG
                 a) to use this on a log_item that already has a key;
                    it should only be used on freshly init'd log_items;
                 b) to use this on a log_item that already has a
                    value (specifically, an allocated one); the correct
                    order is to init a log_item, then set up type and
                    key, and finally to set the value. If said value is
                    an allocated string, the log_item's alloc should be
                    bitwise or'd with LOG_ITEM_FREE_VALUE.

  @retval        a pointer to the log_item's log_data, for easy chaining:
                 log_item_set_with_key(...)->data_integer= 1;
*/
log_item_data *log_item_set_with_key(log_item *li, log_item_type t,
                                     const char *key, uint32 alloc) {
  int c = log_item_wellknown_by_type(t);

  li->alloc = alloc;
  if (log_item_generic_type(t)) {
    li->key = key;
  } else {
    li->key = log_item_wellknown_keys[c].name;
    DBUG_ASSERT((alloc & LOG_ITEM_FREE_KEY) == 0);
  }

  // If we accept a C-string as input, it'll become a Lex string internally
  if ((li->item_class = log_item_wellknown_keys[c].item_class) == LOG_CSTRING)
    li->item_class = LOG_LEX_STRING;

  li->type = t;

  DBUG_ASSERT(((alloc & LOG_ITEM_FREE_VALUE) == 0) ||
              (li->item_class == LOG_CSTRING) ||
              (li->item_class == LOG_LEX_STRING));

  return &li->data;
}

/**
  Create new log item in log line "ll", with key name "key", and
  allocation flags of "alloc" (see enum_log_item_free).
  On success, the number of registered items on the log line is increased,
  the item's type is added to the log_line's "seen" property,
  and a pointer to the item's log_item_data struct is returned for
  convenience.

  @param  ll        the log_line to work on
  @param  t         the item-type
  @param  key       the key to set on the item.
                    ignored for non-generic types (may pass nullptr for those)
                    see alloc
  @param  alloc     LOG_ITEM_FREE_KEY  if key was allocated by caller
                    LOG_ITEM_FREE_NONE if key was not allocated
                    Allocated keys will automatically free()d when the
                    log_item is.
                    The log_item's alloc flags will be set to the
                    submitted value; specifically, any pre-existing
                    value will be clobbered.  It is therefore WRONG
                    a) to use this on a log_item that already has a key;
                       it should only be used on freshly init'd log_items;
                    b) to use this on a log_item that already has a
                       value (specifically, an allocated one); the correct
                       order is to init a log_item, then set up type and
                       key, and finally to set the value. If said value is
                       an allocated string, the log_item's alloc should be
                       bitwise or'd with LOG_ITEM_FREE_VALUE.

  @retval !nullptr  a pointer to the log_item's log_data, for easy chaining:
                    log_line_item_set_with_key(...)->data_integer= 1;
  @retval  nullptr  could not create a log_item in given log_line
*/
log_item_data *log_line_item_set_with_key(log_line *ll, log_item_type t,
                                          const char *key, uint32 alloc) {
  log_item *li;

  if (log_line_full(ll)) return nullptr;

  li = &(ll->item[ll->count]);

  log_item_set_with_key(li, t, key, alloc);
  ll->seen |= t;
  ll->count++;

  return &li->data;
}

/**
  As log_item_set_with_key(), except that the key is automatically
  derived from the wellknown log_item_type t.

  Create new log item with type "t".
  Will return a pointer to the item's log_item_data struct for
  convenience.
  This is mostly interesting for filters and other services that create
  items that are not part of a log_line; sources etc. that intend to
  create an item for a log_line (the more common case) should usually
  use the below line_item_set_with_key() which creates an item (like
  this function does), but also correctly inserts it into a log_line.

  The allocation of this item will be LOG_ITEM_FREE_NONE;
  specifically, any pre-existing value will be clobbered.
  It is therefore WRONG
  a) to use this on a log_item that already has a key;
     it should only be used on freshly init'd log_items;
  b) to use this on a log_item that already has a
     value (specifically, an allocated one); the correct
     order is to init a log_item, then set up type and
     key, and finally to set the value. If said value is
     an allocated string, the log_item's alloc should be
     bitwise or'd with LOG_ITEM_FREE_VALUE.

  @param  li     the log_item to work on
  @param  t      the item-type

  @retval        a pointer to the log_item's log_data, for easy chaining:
                 log_item_set_with_key(...)->data_integer= 1;
*/
log_item_data *log_item_set(log_item *li, log_item_type t) {
  return log_item_set_with_key(li, t, nullptr, LOG_ITEM_FREE_NONE);
}

/**
  Create a new log item of well-known type "t" in log line "ll".
  On success, the number of registered items on the log line is increased,
  the item's type is added to the log_line's "seen" property,
  and a pointer to the item's log_item_data struct is returned for
  convenience.

  The allocation of this item will be LOG_ITEM_FREE_NONE;
  specifically, any pre-existing value will be clobbered.
  It is therefore WRONG
  a) to use this on a log_item that already has a key;
     it should only be used on freshly init'd log_items;
  b) to use this on a log_item that already has a
     value (specifically, an allocated one); the correct
     order is to init a log_item, then set up type and
     key, and finally to set the value. If said value is
     an allocated string, the log_item's alloc should be
     bitwise or'd with LOG_ITEM_FREE_VALUE.

  @param  ll        the log_line to work on
  @param  t         the item-type

  @retval !nullptr  a pointer to the log_item's log_data, for easy chaining:
                    log_line_item_set(...)->data_integer= 1;
  @retval  nullptr  could not create a log_item in given log_line
*/
log_item_data *log_line_item_set(log_line *ll, log_item_type t) {
  return log_line_item_set_with_key(ll, t, nullptr, LOG_ITEM_FREE_NONE);
}

/**
  Set an integer value on a log_item.
  Fails gracefully if not log_item_data is supplied, so it can safely
  wrap log_line_item_set[_with_key]().

  @param  lid    log_item_data struct to set the value on
  @param  i      integer to set

  @retval true   lid was nullptr (possibly: OOM, could not set up log_item)
  @retval false  all's well
*/
bool log_item_set_int(log_item_data *lid, longlong i) {
  if (lid != nullptr) {
    lid->data_integer = i;
    return false;
  }
  return true;
}

/**
  Set a floating point value on a log_item.
  Fails gracefully if not log_item_data is supplied, so it can safely
  wrap log_line_item_set[_with_key]().

  @param  lid    log_item_data struct to set the value on
  @param  f      float to set

  @retval true   lid was nullptr (possibly: OOM, could not set up log_item)
  @retval false  all's well
*/
bool log_item_set_float(log_item_data *lid, double f) {
  if (lid != nullptr) {
    lid->data_float = f;
    return false;
  }
  return true;
}

/**
  Set a string value on a log_item.
  Fails gracefully if not log_item_data is supplied, so it can safely
  wrap log_line_item_set[_with_key]().

  @param  lid    log_item_data struct to set the value on
  @param  s      pointer to string
  @param  s_len  length of string

  @retval true   lid was nullptr (possibly: OOM, could not set up log_item)
  @retval false  all's well
*/
bool log_item_set_lexstring(log_item_data *lid, const char *s, size_t s_len) {
  if (lid != nullptr) {
    lid->data_string.str = (s == nullptr) ? "" : s;
    lid->data_string.length = s_len;
    return false;
  }
  return true;
}

/**
  Set a string value on a log_item.
  Fails gracefully if not log_item_data is supplied, so it can safely
  wrap log_line_item_set[_with_key]().

  @param  lid    log_item_data struct to set the value on
  @param  s      pointer to NTBS

  @retval true   lid was nullptr (possibly: OOM, could not set up log_item)
  @retval false  all's well
*/
bool log_item_set_cstring(log_item_data *lid, const char *s) {
  if (lid != nullptr) {
    lid->data_string.str = (s == nullptr) ? "" : s;
    lid->data_string.length = strlen(lid->data_string.str);
    return false;
  }
  return true;
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
const char *log_label_from_prio(int prio) {
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
      DBUG_ASSERT(false);
      return "";
  }
}

/**
  services: log sinks: basic logging ("classic error-log")
  Will write timestamp, label, thread-ID, and message to stderr/file.
  If you should not be able to specify a label, one will be generated
  for you from the line's priority field.

  @param           instance             instance handle
  @param           ll                   the log line to write

  @retval          int                  number of added fields, if any
*/
static int log_sink_trad(void *instance MY_ATTRIBUTE((unused)), log_line *ll) {
  const char *label = "", *msg = "";
  int c, out_fields = 0;
  size_t msg_len = 0, ts_len = 0, label_len = 0, subsys_len = 0;
  enum loglevel prio = ERROR_LEVEL;
  unsigned int errcode = 0;
  log_item_type item_type = LOG_ITEM_END;
  log_item_type_mask out_types = 0;
  const char *iso_timestamp = "", *subsys = "";
  ;
  my_thread_id thread_id = 0;

  if (ll->count > 0) {
    for (c = 0; c < ll->count; c++) {
      item_type = ll->item[c].type;

      if (log_item_inconsistent(&ll->item[c])) continue;

      out_fields++;

      switch (item_type) {
        case LOG_ITEM_SQL_ERRCODE:
          errcode = (unsigned int)ll->item[c].data.data_integer;
          break;
        case LOG_ITEM_LOG_PRIO:
          prio = (enum loglevel)ll->item[c].data.data_integer;
          break;
        case LOG_ITEM_LOG_MESSAGE:
          msg = ll->item[c].data.data_string.str;
          msg_len = ll->item[c].data.data_string.length;
          break;
        case LOG_ITEM_LOG_LABEL:
          label = ll->item[c].data.data_string.str;
          label_len = ll->item[c].data.data_string.length;
          break;
        case LOG_ITEM_SRV_SUBSYS:
          subsys = ll->item[c].data.data_string.str;
          if ((subsys_len = ll->item[c].data.data_string.length) > 12)
            subsys_len = 12;
          break;
        case LOG_ITEM_LOG_TIMESTAMP:
          iso_timestamp = ll->item[c].data.data_string.str;
          ts_len = ll->item[c].data.data_string.length;
          break;
        case LOG_ITEM_SRV_THREAD:
          thread_id = (my_thread_id)ll->item[c].data.data_integer;
          break;
        default:
          out_fields--;
      }
      out_types |= item_type;
    }

    if (!(out_types & LOG_ITEM_LOG_MESSAGE)) {
      msg =
          "No error message, or error message of non-string type. "
          "This is almost certainly a bug!";
      msg_len = strlen(msg);

      prio = ERROR_LEVEL;                  // force severity
      out_types &= ~(LOG_ITEM_LOG_LABEL);  // regenerate label
      out_types |= LOG_ITEM_LOG_MESSAGE;   // we added a message
    }

    {
      char buff_line[LOG_BUFF_MAX];
      size_t len;

      if (!(out_types & LOG_ITEM_LOG_LABEL)) {
        label = (prio == ERROR_LEVEL) ? "ERROR" : log_label_from_prio(prio);
        label_len = strlen(label);
      }

      if (!(out_types & LOG_ITEM_LOG_TIMESTAMP)) {
        char buff_local_time[iso8601_size];

        make_iso8601_timestamp(buff_local_time, my_micro_time(),
                               opt_log_timestamps);
        iso_timestamp = buff_local_time;
        ts_len = strlen(buff_local_time);
      }

      /*
        WL#11009 adds "error identifier" as a field in square brackets
        that directly precedes the error message. As a result, new
        tools can check for the presence of this field by testing
        whether the first character of the presumed message field is '['.
        Older tools will just consider this identifier part of the
        message; this should therefore not affect log aggregation.
        Tools reacting to the contents of the message may wish to
        use the new field instead as it's simpler to parse.
        The rules are like so:

          '[' [ <namespace> ':' ] <identifier> ']'

        That is, an error identifier may be namespaced by a
        subsystem/component name and a ':'; the identifier
        itself should be considered opaque; in particular, it
        may be non-numerical: [ <alpha> | <digit> | '_' | '.' | '-' ]
      */
      len = snprintf(buff_line, sizeof(buff_line),
                     "%.*s %u [%.*s] [MY-%06u] [%.*s] %.*s", (int)ts_len,
                     iso_timestamp, thread_id, (int)label_len, label, errcode,
                     (int)subsys_len, subsys, (int)msg_len, msg);

      log_write_errstream(buff_line, len);

      return out_fields;  // returning number of processed items
    }
  }
  return 0;
}

/**
  Broadcast: Call variable set or update function for all active log services.

  @param   ll       log-line data for the service to operate on:

                    If ll->item[1] has a key of LOG_VAR_KEY_CHECK and a
                    value of 1, we submit the data (in item[0]) to the
                    log-services for validation:

                      If a service either flags the value as invalid,
                      or indicates an internal error (out of memory etc.,
                      in which case it cannot ascertain whether the value
                      is valid), processing is aborted, and an error response
                      to the user's SET is sent.
                      If all services have no objections (because they
                      consider the value correct or another service's business),
                      the value is considered acceptable.

                    Otherwise, we submit the data in item[0] to the
                    log-services for them to update themselves accordingly:

                      The value will be offered to each service in turn,
                      even if one flags an internal error (out of the return
                      states of, "updated", "ignored", "update attempted and
                      failed").

  @retval           "check"  mode: number of services that failed or voted
  "deny"
  @retval           "update" mode: number of services that successfully updated
*/
static int log_broadcast_sys_var_set(log_line *ll) {
  int count = 0;
  log_item *li;
  bool validate_only;

  DBUG_ASSERT(ll != nullptr);
  DBUG_ASSERT(ll->count >= 2);

  /*
    Changing the variable e.g. for Windows eventlog tag calls some Win
    functions that are marked not thread-safe, so we serialize them here.
    It's a service we provide to all services (even the ones that consider
    themselves thread-safe), as this section isn't performance critical --
    we wouldn't normally expect even just 2 sessions concurrently setting
    log_stack or log-variables, much less dozens or hundreds of sessions.
    Therefore, our concern here is to be robust.
  */

  mysql_rwlock_wrlock(&THR_LOCK_log_stack);

  /*
    log_line:
    ll[0] contains the variable key and value
    ll[1] contains the action name (check or update)
  */
  li = &ll->item[1];
  validate_only =
      !strcmp(li->key, LOG_VAR_KEY_CHECK) && (li->data.data_integer != 0);

  for (auto &log_service_cache_element : *log_service_cache) {
    log_service_cache_entry *sce = log_service_cache_element.second.get();

    /*
      We only broadcast updates of system variables.
      As such, we only need to broadcast to external
      (loaded, not built-in) services, as we can handle
      the built-in ones directly.
      It is expected that eventually, services will be
      either internal, or use pluggable system-variables
      through the appropriate component framework functionality;
      once that is achieved, this broadcast function may
      be removed.
    */
    if (sce->type == LOG_SERVICE_BUILTIN_TYPE_NONE) {
      if (sce->service != nullptr) {
        /*
          result:
          -1 if any of the parameters were nonsense; otherwise
          the return value of the service we queried;
          by convention:

          variable check:  <0 failure; 0 proceed; >0 value invalid

            - failure: we tried to run our checks, but failed
            - proceed: we checked the value, and it is valid
            - invalid: we checked the value, and it is invalid

          variable update: <0 failure; 0 not processed; >0 processed

            - failure:       we tried to update the value, but failed
            - not processed: no service chose to update itself (as
                             opposed to "tried to do so, but failed", above)
            - processed:     one (or more) services updated themselves
        */
        int result;
        SERVICE_TYPE(log_service) * ls;

        ls = reinterpret_cast<SERVICE_TYPE(log_service) *>(sce->service);

        /*
          If we can't get the pointer, this service counts as failed.
        */
        if (ls == nullptr) result = -1;

        /*
          Call check function or update function in service depending
          on the parameters we received (in the log_line).
        */
        else {
          result =
              validate_only ? ls->variable_check(ll) : ls->variable_update(ll);
        }

        /*
          Variable checking fails as soon as a single service says it does,
          or as soon as one fails internally (as this is our opportunity to
          throw an error to the client for it)
        */
        if (validate_only) {
          if (result != 0) {
            count++;
            break;
          }
        }
        /*
          Variable updating tries all services, even if some fail
          (which they normally shouldn't, they had time to complain
          during the check phase).
        */
        else if (result > 0) {
          count++;
        }
      }
    }
  }

  mysql_rwlock_unlock(&THR_LOCK_log_stack);

  return count;
}

/**
  Complete, filter, and write submitted log items.

  This expects a log_line collection of log-related key/value pairs,
  e.g. from log_message().

  Where missing, timestamp, priority, thread-ID (if any) and so forth
  are added.

  Log item source services, log item filters, and log item sinks are
  then called.

  @param           ll                   key/value pairs describing info to log

  @retval          int                  number of fields in created log line
*/
int log_line_submit(log_line *ll) {
  log_item_iter iter_save;

  DBUG_ENTER("log_line_submit");

  /*
    The log-services we'll call below are likely to change the default
    iter. Since log-services are allowed to call the logger, we'll save
    the iter on entry and restore it on exit to be properly re-entrant in
    that regard.
  */
  iter_save = ll->iter;
  ll->iter.ll = nullptr;

  /*
    If anything of what was submitted survived, proceed ...
  */
  if (ll->count > 0) {
    THD *thd = nullptr;

    // avoid some allocs/frees.
    char local_time_buff[iso8601_size];
    char strerr_buf[MYSYS_STRERROR_SIZE];

    /* auto-add a prio item */
    if (!(ll->seen & LOG_ITEM_LOG_PRIO) && !log_line_full(ll)) {
      log_line_item_set(ll, LOG_ITEM_LOG_PRIO)->data_integer = ERROR_LEVEL;
    }

    /* auto-add a timestamp item if needed */
    if (!(ll->seen & LOG_ITEM_LOG_TIMESTAMP) && !log_line_full(ll)) {
      log_item_data *d;
      ulonglong now = my_micro_time();

      DBUG_EXECUTE_IF("log_error_normalize", { now = 0; });

      make_iso8601_timestamp(local_time_buff, now, opt_log_timestamps);

      d = log_line_item_set(ll, LOG_ITEM_LOG_TIMESTAMP);
      d->data_string.str = local_time_buff;
      d->data_string.length = strlen(d->data_string.str);
    }

    /* auto-add a strerror item if relevant and available */
    if (!(ll->seen & LOG_ITEM_SYS_STRERROR) && !log_line_full(ll) &&
        (ll->seen & LOG_ITEM_SYS_ERRNO)) {
      int en;  // operating system errno
      int n = log_line_index_by_type(ll, LOG_ITEM_SYS_ERRNO);
      log_item_data *d = log_line_item_set(ll, LOG_ITEM_SYS_STRERROR);

      DBUG_ASSERT(n >= 0);

      en = (int)ll->item[n].data.data_integer;
      my_strerror(strerr_buf, sizeof(strerr_buf), en);
      d->data_string.str = strerr_buf;
      d->data_string.length = strlen(d->data_string.str);
    }

    /* add thread-related info, if available */
    if ((thd = current_thd) != nullptr) {
      /* auto-add a thread item if needed */
      if (!(ll->seen & LOG_ITEM_SRV_THREAD) && !log_line_full(ll)) {
        my_thread_id tid = log_get_thread_id(thd);

        DBUG_EXECUTE_IF("log_error_normalize", { tid = 0; });

        log_line_item_set(ll, LOG_ITEM_SRV_THREAD)->data_integer = tid;
      }
    }

    /* auto-add a symbolic MySQL error code item item if needed */
    if (!(ll->seen & LOG_ITEM_SQL_ERRSYMBOL) && !log_line_full(ll) &&
        (ll->seen & LOG_ITEM_SQL_ERRCODE)) {
      int ec;  // MySQL error code
      int n = log_line_index_by_type(ll, LOG_ITEM_SQL_ERRCODE);
      const char *es;

      DBUG_ASSERT(n >= 0);

      ec = (int)ll->item[n].data.data_integer;
      if ((ec != 0) && ((es = mysql_errno_to_symbol(ec)) != nullptr)) {
        log_item_data *d = log_line_item_set(ll, LOG_ITEM_SQL_ERRSYMBOL);
        d->data_string.str = es;
        d->data_string.length = strlen(d->data_string.str);
      }
    }
    /* auto-add a numeric MySQL error code item item if needed */
    else if (!(ll->seen & LOG_ITEM_SQL_ERRCODE) && !log_line_full(ll) &&
             (ll->seen & LOG_ITEM_SQL_ERRSYMBOL)) {
      const char *es;  // MySQL error symbol
      int n = log_line_index_by_type(ll, LOG_ITEM_SQL_ERRSYMBOL);
      int ec;

      DBUG_ASSERT(n >= 0);

      es = ll->item[n].data.data_string.str;

      DBUG_ASSERT(es != nullptr);

      if ((ec = mysql_symbol_to_errno(es)) > 0) {
        log_item_data *d = log_line_item_set(ll, LOG_ITEM_SQL_ERRCODE);
        d->data_integer = ec;
      }
    }

    /* auto-add a SQL state item item if needed */
    if (!(ll->seen & LOG_ITEM_SQL_STATE) && !log_line_full(ll) &&
        (ll->seen & LOG_ITEM_SQL_ERRCODE)) {
      int ec;  // MySQL error code
      int n = log_line_index_by_type(ll, LOG_ITEM_SQL_ERRCODE);
      const char *es;

      if (n < 0) {
        n = log_line_index_by_type(ll, LOG_ITEM_SQL_ERRSYMBOL);
        DBUG_ASSERT(n >= 0);

        es = ll->item[n].data.data_string.str;
        DBUG_ASSERT(es != nullptr);

        ec = mysql_symbol_to_errno(es);
      } else
        ec = (int)ll->item[n].data.data_integer;

      if ((ec > 0) && ((es = mysql_errno_to_sqlstate((uint)ec)) != nullptr)) {
        log_item_data *d = log_line_item_set(ll, LOG_ITEM_SQL_STATE);
        d->data_string.str = es;
        d->data_string.length = strlen(d->data_string.str);
      }
    }

    /* add the default sub-system if none is set */
    if (!(ll->seen & LOG_ITEM_SRV_SUBSYS) && !log_line_full(ll)) {
      log_item_data *d = log_line_item_set(ll, LOG_ITEM_SRV_SUBSYS);
      d->data_string.str = LOG_SUBSYSTEM_TAG;
      d->data_string.length = strlen(d->data_string.str);
    }

    /* normalize source line if needed */
    DBUG_EXECUTE_IF("log_error_normalize", {
      if (ll->seen & LOG_ITEM_SRC_LINE) {
        int n = log_line_index_by_type(ll, LOG_ITEM_SRC_LINE);

        if (n >= 0) {
          ll->item[n] = ll->item[ll->count - 1];
          ll->count--;
          ll->seen &= ~LOG_ITEM_SRC_LINE;
        }
      }
    });

    mysql_rwlock_rdlock(&THR_LOCK_log_stack);

    /*
      sources:
      Add info from other log item sources,
      e.g. that supplied by the client on connect using mysql_options4();

      filters:
      Remove or modify entries

      sinks:
      Write logs
    */

    {
      log_service_cache_entry *sce;
      log_service_instance *lsi = log_service_instances;

      while ((lsi != nullptr) && ((sce = lsi->sce) != nullptr)) {
        if (sce->type == LOG_SERVICE_BUILTIN_TYPE_NONE) {
          SERVICE_TYPE(log_service) * ls;

          ls = reinterpret_cast<SERVICE_TYPE(log_service) *>(sce->service);

          if (ls != nullptr) ls->run(lsi->instance, ll);
        } else if (sce->type == LOG_SERVICE_BUILTIN_TYPE_FILTER)
          log_builtins_filter_run(log_filter_builtin_rules, ll);
        else if (sce->type == LOG_SERVICE_BUILTIN_TYPE_SINK)
          log_sink_trad(lsi->instance, ll);

        lsi = lsi->next;
      }
    }

    // release any memory that might need it

    mysql_rwlock_unlock(&THR_LOCK_log_stack);

#if !defined(DBUG_OFF)
    /*
      Assert that we're not given anything but server error-log codes.
      If your code bombs out here, check whether you're trying to log
      using an error-code in the range intended for messages that are
      sent to the client, not the error-log, (< ER_SERVER_RANGE_START).
    */
    if (ll->seen & LOG_ITEM_SQL_ERRCODE) {
      int n = log_line_index_by_type(ll, LOG_ITEM_SQL_ERRCODE);
      if (n >= 0) {
        int ec = (int)ll->item[n].data.data_integer;
        DBUG_ASSERT((ec < 1) || (ec >= ER_SERVER_RANGE_START));
      }
    }
#endif

    log_line_item_free_all(ll);
  }

  ll->iter = iter_save;

  DBUG_RETURN(ll->count);
}

/**
  Make and return an ISO 8601 / RFC 3339 compliant timestamp.
  Heeds log_timestamps.

  @param buf       A buffer of at least 26 bytes to store the timestamp in
                   (19 + tzinfo tail + \0)
  @param utime     Microseconds since the epoch
  @param mode      if 0, use UTC; if 1, use local time

  @retval          length of timestamp (excluding \0)
*/
int make_iso8601_timestamp(char *buf, ulonglong utime, int mode) {
  struct tm my_tm;
  char tzinfo[8] = "Z";  // max 6 chars plus \0
  size_t len;
  time_t seconds;

  seconds = utime / 1000000;
  utime = utime % 1000000;

  if (mode == 0)
    gmtime_r(&seconds, &my_tm);
  else if (mode == 1) {
    localtime_r(&seconds, &my_tm);

#ifdef __FreeBSD__
    /*
      The field tm_gmtoff is the offset (in seconds) of the time represented
      from UTC, with positive values indicating east of the Prime Meridian.
    */
    long tim = -my_tm.tm_gmtoff;
#elif defined(_WIN32)
    long tim = _timezone;
#else
    long tim = timezone;  // seconds West of UTC.
#endif
    char dir = '-';

    if (tim < 0) {
      dir = '+';
      tim = -tim;
    }
    snprintf(tzinfo, sizeof(tzinfo), "%c%02d:%02d", dir, (int)(tim / (60 * 60)),
             (int)((tim / 60) % 60));
  } else {
    DBUG_ASSERT(false);
  }

  len = snprintf(buf, iso8601_size, "%04d-%02d-%02dT%02d:%02d:%02d.%06lu%s",
                 my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday,
                 my_tm.tm_hour, my_tm.tm_min, my_tm.tm_sec,
                 (unsigned long)utime, tzinfo);

  return std::min<int>((int)len, iso8601_size - 1);
}

/**
  Helper: get token from error stack configuration string

  @param[in,out]  s  start of the token (may be positioned on whitespace
                     on call; this will be adjusted to the first non-white
                     character)
  @param[out]     e  end of the token

  @retval            the length in bytes of the token
*/
static size_t log_builtins_stack_get_service_from_var(const char **s,
                                                      const char **e) {
  DBUG_ASSERT(s != nullptr);
  DBUG_ASSERT(e != nullptr);

  while (isspace(**s) || (**s == ';')) (*s)++;

  *e = *s;

  while ((**e != '\0') && (**e != ';') && !isspace(**e)) (*e)++;

  return (size_t)(*e - *s);
}

/**
  Look up a log service by name (in the service registry).

  @param        name    name of the component (will be auto-qualified)
  @param        len     length of that name
  @param[out]   buf     buffer to qualify the name in
  @param        bufsiz  size of that buffer

  @retval               a handle to that service.
*/
static my_h_service log_service_get_by_name(const char *name, size_t len,
                                            char *buf, size_t bufsiz) {
  my_h_service service = nullptr;
  size_t needed;

  needed = snprintf(buf, bufsiz, LOG_SERVICES_PREFIX ".%.*s", (int)len, name);

  if (needed > bufsiz) return service;

  if ((!imp_mysql_server_registry.acquire(buf, &service)) &&
      (service != nullptr)) {
    SERVICE_TYPE(log_service) * ls;

    if ((ls = reinterpret_cast<SERVICE_TYPE(log_service) *>(service)) ==
        nullptr) {
      imp_mysql_server_registry.release(service);
      service = nullptr;
    }
  } else
    service = nullptr;

  return service;
}

void log_service_cache_entry_free::operator()(
    log_service_cache_entry *sce) const {
  if (sce == nullptr) return;

  if (sce->name != nullptr) my_free(sce->name);

  assert(sce->opened == 0);

  if (sce->service != nullptr) imp_mysql_server_registry.release(sce->service);

  memset(sce, 0, sizeof(log_service_cache_entry));

  my_free(sce);
}

/**
  Create a new entry in the cache of log services.

  @param  name      Name of component that provides the service
  @param  name_len  Length of that name
  @param  srv       The handle of the log_service

  @retval           A new log_service_cache_entry on success
  @retval           nullptr on failure
*/
static log_service_cache_entry *log_service_cache_entry_new(const char *name,
                                                            size_t name_len,
                                                            my_h_service srv) {
  char *n = my_strndup(key_memory_log_error_stack, name, name_len, MYF(0));
  log_service_cache_entry *sce = nullptr;

  if (n != nullptr) {
    // make new service cache entry
    if ((sce = (log_service_cache_entry *)my_malloc(
             key_memory_log_error_stack, sizeof(log_service_cache_entry),
             MYF(0))) == nullptr)
      my_free(n);
    else {
      memset(sce, 0, sizeof(log_service_cache_entry));
      sce->name = n;
      sce->name_len = name_len;
      sce->service = srv;
      sce->multi_open = false;
      sce->requested = 0;
      sce->opened = 0;
    }
  }

  return sce;
}

/**
  Find out whether a given service is a singleton

  @param  service  what service to examine

  @retval true     multiple instances of the given service may be opened
  @retval false    only one instance  of the given service may be opened
*/
static bool log_service_multi_open_capable(my_h_service service) {
  SERVICE_TYPE(log_service) * ls;

  DBUG_ASSERT(service != nullptr);

  ls = reinterpret_cast<SERVICE_TYPE(log_service) *>(service);
  return ((ls->open != nullptr) && (ls->close != nullptr));
}

/**
  Allocate and open a new instance of a given service.

  @param  sce  the cache-entry for the service
  @param  ll   a log_line containing optional parameters, or nullptr

  @return      a pointer to an instance record or success, nullptr otherwise
*/
log_service_instance *log_service_instance_new(log_service_cache_entry *sce,
                                               log_line *ll) {
  log_service_instance *lsi;

  // make new service instance entry
  if ((lsi = (log_service_instance *)my_malloc(
           key_memory_log_error_stack, sizeof(log_service_instance), MYF(0))) !=
      nullptr) {
    memset(lsi, 0, sizeof(log_service_instance));
    lsi->sce = sce;

    DBUG_ASSERT(sce != nullptr);

    if (lsi->sce->service != nullptr) {
      SERVICE_TYPE(log_service) *ls = nullptr;

      ls = reinterpret_cast<SERVICE_TYPE(log_service) *>(lsi->sce->service);

      if ((ls == nullptr) ||
          ((ls->open != nullptr) && (ls->open(ll, &lsi->instance) < 0)))
        goto fail;
    }

    lsi->sce->opened++;
  }

  return lsi;

fail:
  my_free(lsi);
  return nullptr;
}

/**
  Close and release all instances of all log services.
*/
static void log_service_instance_release_all() {
  log_service_instance *lsi, *lsi_next;

  lsi = log_service_instances;
  log_service_instances = nullptr;

  // release all instances!
  while (lsi != nullptr) {
    SERVICE_TYPE(log_service) * ls;

    ls = reinterpret_cast<SERVICE_TYPE(log_service) *>(lsi->sce->service);

    if (ls != nullptr) {
      if (ls->close != nullptr) ls->close(&lsi->instance);
    }

    lsi->sce->opened--;
    lsi_next = lsi->next;
    my_free(lsi);
    lsi = lsi_next;
  }
}

/**
  Call flush() on all log_services.
  flush() function must not try to log anything, as we hold an
  exclusive lock on the stack.

  @retval   0   no problems
  @retval  -1   error
*/
int log_builtins_error_stack_flush() {
  int rr = 0;
  log_service_cache_entry *sce;
  log_service_instance *lsi;

  mysql_rwlock_wrlock(&THR_LOCK_log_stack);

  lsi = log_service_instances;

  while ((lsi != nullptr) && ((sce = lsi->sce) != nullptr)) {
    if (sce->type == LOG_SERVICE_BUILTIN_TYPE_NONE) {
      SERVICE_TYPE(log_service) *ls = nullptr;
      ls = reinterpret_cast<SERVICE_TYPE(log_service) *>(sce->service);

      if (ls != nullptr) {
        if ((ls->flush != nullptr) && (ls->flush(&lsi->instance) < 0)) rr--;
      } else
        rr--;
    }
    lsi = lsi->next;
  }

  mysql_rwlock_unlock(&THR_LOCK_log_stack);

  return rr;
}

/**
  Set up custom error logging stack.

  @param   conf        The configuration string
  @param   check_only  if true, report on whether configuration is valid
                       (i.e. whether all requested services are available),
                       but do not apply the new configuration.
                       if false, set the configuration (acquire the necessary
                       services, update the hash by adding/deleting entries
                       as necessary)

  @retval              <0   failure
  @retval              >=0  success
*/
int log_builtins_error_stack(const char *conf, bool check_only) {
  char buf[128];
  const char *start = conf, *end;
  size_t len;
  my_h_service service;
  int rr = 0;
  log_service_cache_entry *sce;
  log_service_instance *lsi;

  mysql_rwlock_wrlock(&THR_LOCK_log_stack);

  // if we're actually setting this configuration, release the previous one!
  if (!check_only) log_service_instance_release_all();

  // clear keep flag on all service cache entries
  for (auto &key_and_value : *log_service_cache) {
    sce = key_and_value.second.get();
    sce->requested = 0;

    assert(check_only || (sce->opened == 0));
  }

  lsi = nullptr;
  while ((len = log_builtins_stack_get_service_from_var(&start, &end)) > 0) {
    log_service_builtin_type srvtype = LOG_SERVICE_BUILTIN_TYPE_NONE;

    // find current service name in service-cache
    auto it = log_service_cache->find(string(start, len));

    // not found in cache; ask component framework
    if (it == log_service_cache->end()) {
      service = log_service_get_by_name(start, len, buf, sizeof(buf));

      // not found in framework, is it a built-in default?
      if (service == nullptr) {
        srvtype = log_service_builtin(buf);

        if (srvtype == LOG_SERVICE_BUILTIN_TYPE_NONE) {
          rr = (int)-(start - conf +
                      1);  // at least one service not found => fail
          goto done;
        }
      }

      // make a cache-entry for this service
      if ((sce = log_service_cache_entry_new(start, len, service)) == nullptr) {
        // failed to make cache-entry. if we hold a service handle, release it!
        if (service != nullptr) imp_mysql_server_registry.release(service);
        rr = -2;
        goto done;
      }

      sce->type = srvtype;

      switch (srvtype) {
        case LOG_SERVICE_BUILTIN_TYPE_NONE:
          sce->multi_open = log_service_multi_open_capable(service);
          break;
        case LOG_SERVICE_BUILTIN_TYPE_FILTER:
          sce->multi_open = false;
          break;
        case LOG_SERVICE_BUILTIN_TYPE_SINK:
          sce->multi_open = false;
          break;
        default:
          DBUG_ASSERT(false);
      }

      log_service_cache->emplace(string(sce->name, sce->name_len),
                                 cache_entry_with_deleter(sce));
    } else
      sce = it->second.get();

    // at this point, it's in cache, one way or another
    sce->requested++;

    if (check_only) {
      // tried to multi-open a service that doesn't support it => fail
      if ((sce->requested > 1) && (!sce->multi_open)) {
        rr = (int)-(start - conf + 1);
        goto done;
      }
    } else if ((sce->requested == 1) || sce->multi_open) {
      log_service_instance *lsi_new = nullptr;

      // actually setting this config, so open this instance!
      lsi_new = log_service_instance_new(sce, nullptr);

      if (lsi_new != nullptr)  // add to chain of instances
      {
        if (log_service_instances == nullptr)
          log_service_instances = lsi_new;
        else {
          assert(lsi != nullptr);
          lsi->next = lsi_new;
        }

        lsi = lsi_new;
      } else  // could not make new instance entry; fail
      {
        rr = (int)-(start - conf + 1);
        goto done;
      }
    }

    /*
      If neither branch was true, we're in set mode, but the set-up
      is invalid (i.e. we're trying to multi-open a singleton). As
      this should have been caught in the check phase, we don't
      specfically handle it here; the invalid element is skipped and
      note added to the instance list; that way, we'll get as close
      to a working configuration as possible in our attempt to fail
      somewhat gracefully.
    */

    start = end;
  }

  rr = 0;

done:
  // remove stale entries from cache
  for (auto it = log_service_cache->begin(); it != log_service_cache->end();) {
    sce = it->second.get();

    if (sce->opened <= 0)
      it = log_service_cache->erase(it);
    else
      ++it;
  }

  mysql_rwlock_unlock(&THR_LOCK_log_stack);

  return rr;
}

/**
  De-initialize the structured logging subsystem.

  @retval  0  no errors
*/
int log_builtins_exit() {
  DBUG_ASSERT(log_builtins_inited > 0);

  mysql_mutex_lock(&THR_LOCK_log_syseventlog);
  mysql_rwlock_wrlock(&THR_LOCK_log_stack);

  log_builtins_filter_exit();
  log_service_instance_release_all();
  delete log_service_cache;

  log_builtins_inited = 0;

  mysql_rwlock_unlock(&THR_LOCK_log_stack);
  mysql_rwlock_destroy(&THR_LOCK_log_stack);

  mysql_mutex_unlock(&THR_LOCK_log_syseventlog);
  mysql_mutex_destroy(&THR_LOCK_log_syseventlog);

  return 0;
}

/**
  Initialize the structured logging subsystem.

  @retval  0  no errors
  @retval -1  couldn't initialize stack lock
  @retval -2  couldn't initialize built-in default filter
  @retval -3  couldn't set up service hash
  @retval -4  couldn't initialize syseventlog lock
*/
int log_builtins_init() {
  int rr = 0;

  DBUG_ASSERT(log_builtins_inited <= 0);

  if (mysql_rwlock_init(0, &THR_LOCK_log_stack)) return -1;

  if (mysql_mutex_init(0, &THR_LOCK_log_syseventlog, MY_MUTEX_INIT_FAST)) {
    mysql_rwlock_destroy(&THR_LOCK_log_stack);
    return -4;
  }

  mysql_rwlock_wrlock(&THR_LOCK_log_stack);

  if (log_builtins_filter_init())
    rr = -2;
  else {
    log_service_cache =
        new collation_unordered_map<string, cache_entry_with_deleter>(
            system_charset_info, 0);
  }

  log_service_instances = nullptr;

  mysql_rwlock_unlock(&THR_LOCK_log_stack);

  if (rr < 0)
    mysql_rwlock_destroy(&THR_LOCK_log_stack);
  else {
    log_builtins_inited++;

    if (log_builtins_error_stack(opt_log_error_services, false) >= 0)
      log_builtins_inited++;
  }

  return rr;
}

/*
  Service: helpers for logging. Mostly accessors for log events.
  See include/mysql/components/services/log_builtins.h for more information.
*/

/**
  See whether a type is wellknown.

  @param t       log item type to examine

  @retval        LOG_ITEM_TYPE_NOT_FOUND: key not found
  @retval        >0:                      index in array of wellknowns
*/
DEFINE_METHOD(int, log_builtins_imp::wellknown_by_type, (log_item_type t)) {
  return log_item_wellknown_by_type(t);
}

/**
  See whether a string is a wellknown field name.

  @param key     potential key starts here
  @param length  length of the string to examine

  @retval        LOG_ITEM_TYPE_RESERVED:  reserved, but not "wellknown" key
  @retval        LOG_ITEM_TYPE_NOT_FOUND: key not found
  @retval        >0:                      index in array of wellknowns
*/
DEFINE_METHOD(int, log_builtins_imp::wellknown_by_name,
              (const char *key, size_t length)) {
  return log_item_wellknown_by_name(key, length);
}

/**
  Accessor: from a record describing a wellknown key, get its type

  @param i       index in array of wellknowns, see log_item_wellknown_by_...()

  @retval        the log item type for the wellknown key
*/
DEFINE_METHOD(log_item_type, log_builtins_imp::wellknown_get_type, (uint i)) {
  return log_item_wellknown_get_type(i);
}

/**
  Accessor: from a record describing a wellknown key, get its name

  @param   i    index in array of wellknowns, see log_item_wellknown_by_...()

  @retval       name (NTBS)
*/
DEFINE_METHOD(const char *, log_builtins_imp::wellknown_get_name, (uint i)) {
  return log_item_wellknown_get_name(i);
}

/**
  Sanity check an item.
  Certain log sinks have very low requirements with regard to the data
  they receive; they write keys as strings, and then data according to
  the item's class (string, integer, or float), formatted to the sink's
  standards (e.g. JSON, XML, ...).
  Code that has higher requirements can use this check to see whether
  the given item is of a known type (whether generic or wellknown),
  whether the given type and class agree, and whether in case of a
  well-known type, the given key is correct for that type.
  If your code generates items that don't pass this check, you should
  probably go meditate on it.

  @param  li  the log_item to check

  @retval  0  no problems
  @retval -2  unknown item type
  @retval -3  item_class derived from type isn't what's set on the item
  @retval -4  class not generic, so key should match wellknown
*/
DEFINE_METHOD(int, log_builtins_imp::item_inconsistent, (log_item * li)) {
  return log_item_inconsistent(li);
}

/**
  Predicate used to determine whether a type is generic
  (generic string, generic float, generic integer) rather
  than a well-known type.

  @param t          log item type to examine

  @retval  true     if generic type
  @retval  false    if wellknown type
*/
DEFINE_METHOD(bool, log_builtins_imp::item_generic_type, (log_item_type t)) {
  return log_item_generic_type(t);
}

/**
  Predicate used to determine whether a class is a string
  class (C-string or Lex-string).

  @param c          log item class to examine

  @retval   true    if of a string class
  @retval   false   if not of a string class
*/
DEFINE_METHOD(bool, log_builtins_imp::item_string_class, (log_item_class c)) {
  return log_item_string_class(c);
}

/**
  Predicate used to determine whether a class is a numeric
  class (integer or float).

  @param c         log item class to examine

  @retval   true   if of a numeric class
  @retval   false  if not of a numeric class
*/
DEFINE_METHOD(bool, log_builtins_imp::item_numeric_class, (log_item_class c)) {
  return log_item_numeric_class(c);
}

/**
  Set an integer value on a log_item.
  Fails gracefully if not log_item_data is supplied, so it can safely
  wrap log_line_item_set[_with_key]().

  @param  lid    log_item_data struct to set the value on
  @param  i      integer to set

  @retval true   lid was nullptr (possibly: OOM, could not set up log_item)
  @retval false  all's well
*/
DEFINE_METHOD(bool, log_builtins_imp::item_set_int,
              (log_item_data * lid, longlong i)) {
  return log_item_set_int(lid, i);
}

/**
  Set a floating point value on a log_item.
  Fails gracefully if not log_item_data is supplied, so it can safely
  wrap log_line_item_set[_with_key]().

  @param  lid    log_item_data struct to set the value on
  @param  f      float to set

  @retval true   lid was nullptr (possibly: OOM, could not set up log_item)
  @retval false  all's well
*/
DEFINE_METHOD(bool, log_builtins_imp::item_set_float,
              (log_item_data * lid, double f)) {
  return log_item_set_float(lid, f);
}

/**
  Set a string value on a log_item.
  Fails gracefully if not log_item_data is supplied, so it can safely
  wrap log_line_item_set[_with_key]().

  @param  lid    log_item_data struct to set the value on
  @param  s      pointer to string
  @param  s_len  length of string

  @retval true   lid was nullptr (possibly: OOM, could not set up log_item)
  @retval false  all's well
*/
DEFINE_METHOD(bool, log_builtins_imp::item_set_lexstring,
              (log_item_data * lid, const char *s, size_t s_len)) {
  return log_item_set_lexstring(lid, s, s_len);
}

/**
  Set a string value on a log_item.
  Fails gracefully if not log_item_data is supplied, so it can safely
  wrap log_line_item_set[_with_key]().

  @param  lid    log_item_data struct to set the value on
  @param  s      pointer to NTBS

  @retval true   lid was nullptr (possibly: OOM, could not set up log_item)
  @retval false  all's well
*/
DEFINE_METHOD(bool, log_builtins_imp::item_set_cstring,
              (log_item_data * lid, const char *s)) {
  return log_item_set_cstring(lid, s);
}

/**
  Create new log item with key name "key", and allocation flags of
  "alloc" (see enum_log_item_free).
  Will return a pointer to the item's log_item_data struct for
  convenience.
  This is mostly interesting for filters and other services that create
  items that are not part of a log_line; sources etc. that intend to
  create an item for a log_line (the more common case) should usually
  use the below line_item_set_with_key() which creates an item (like
  this function does), but also correctly inserts it into a log_line.

  @param  li     the log_item to work on
  @param  t      the item-type
  @param  key    the key to set on the item.
                 ignored for non-generic types (may pass nullptr for those)
                 see alloc
  @param  alloc  LOG_ITEM_FREE_KEY  if key was allocated by caller
                 LOG_ITEM_FREE_NONE if key was not allocated
                 Allocated keys will automatically free()d when the
                 log_item is.
                 The log_item's alloc flags will be set to the
                 submitted value; specifically, any pre-existing
                 value will be clobbered.  It is therefore WRONG
                 a) to use this on a log_item that already has a key;
                    it should only be used on freshly init'd log_items;
                 b) to use this on a log_item that already has a
                    value (specifically, an allocated one); the correct
                    order is to init a log_item, then set up type and
                    key, and finally to set the value. If said value is
                    an allocated string, the log_item's alloc should be
                    bitwise or'd with LOG_ITEM_FREE_VALUE.

  @retval        a pointer to the log_item's log_data, for easy chaining:
                 log_item_set_with_key(...)->data_integer= 1;
*/
DEFINE_METHOD(log_item_data *, log_builtins_imp::item_set_with_key,
              (log_item * li, log_item_type t, const char *key, uint32 alloc)) {
  return log_item_set_with_key(li, t, key, alloc);
}

/**
  As log_item_set_with_key(), except that the key is automatically
  derived from the wellknown log_item_type t.

  Create new log item with type "t".
  Will return a pointer to the item's log_item_data struct for
  convenience.
  This is mostly interesting for filters and other services that create
  items that are not part of a log_line; sources etc. that intend to
  create an item for a log_line (the more common case) should usually
  use the below line_item_set_with_key() which creates an item (like
  this function does), but also correctly inserts it into a log_line.

  The allocation of this item will be LOG_ITEM_FREE_NONE;
  specifically, any pre-existing value will be clobbered.
  It is therefore WRONG
  a) to use this on a log_item that already has a key;
     it should only be used on freshly init'd log_items;
  b) to use this on a log_item that already has a
     value (specifically, an allocated one); the correct
     order is to init a log_item, then set up type and
     key, and finally to set the value. If said value is
     an allocated string, the log_item's alloc should be
     bitwise or'd with LOG_ITEM_FREE_VALUE.

  @param  li     the log_item to work on
  @param  t      the item-type

  @retval        a pointer to the log_item's log_data, for easy chaining:
                 log_item_set_with_key(...)->data_integer= 1;
*/
DEFINE_METHOD(log_item_data *, log_builtins_imp::item_set,
              (log_item * li, log_item_type t)) {
  return log_item_set(li, t);
}

/**
  Create new log item in log line "ll", with key name "key", and
  allocation flags of "alloc" (see enum_log_item_free).
  On success, the number of registered items on the log line is increased,
  the item's type is added to the log_line's "seen" property,
  and a pointer to the item's log_item_data struct is returned for
  convenience.

  @param  ll        the log_line to work on
  @param  t         the item-type
  @param  key       the key to set on the item.
                    ignored for non-generic types (may pass nullptr for those)
                    see alloc
  @param  alloc     LOG_ITEM_FREE_KEY  if key was allocated by caller
                    LOG_ITEM_FREE_NONE if key was not allocated
                    Allocated keys will automatically free()d when the
                    log_item is.
                    The log_item's alloc flags will be set to the
                    submitted value; specifically, any pre-existing
                    value will be clobbered.  It is therefore WRONG
                    a) to use this on a log_item that already has a key;
                       it should only be used on freshly init'd log_items;
                    b) to use this on a log_item that already has a
                       value (specifically, an allocated one); the correct
                       order is to init a log_item, then set up type and
                       key, and finally to set the value. If said value is
                       an allocated string, the log_item's alloc should be
                       bitwise or'd with LOG_ITEM_FREE_VALUE.


  @retval !nullptr  a pointer to the log_item's log_data, for easy chaining:
                    line_item_set_with_key(...)->data_integer= 1;
  @retval  nullptr  could not create a log_item in given log_line
*/
DEFINE_METHOD(log_item_data *, log_builtins_imp::line_item_set_with_key,
              (log_line * ll, log_item_type t, const char *key, uint32 alloc)) {
  return log_line_item_set_with_key(ll, t, key, alloc);
}

/**
  Create a new log item of well-known type "t" in log line "ll".
  On success, the number of registered items on the log line is increased,
  the item's type is added to the log_line's "seen" property,
  and a pointer to the item's log_item_data struct is returned for
  convenience.

  The allocation of this item will be LOG_ITEM_FREE_NONE;
  specifically, any pre-existing value will be clobbered.
  It is therefore WRONG
  a) to use this on a log_item that already has a key;
     it should only be used on freshly init'd log_items;
  b) to use this on a log_item that already has a
     value (specifically, an allocated one); the correct
     order is to init a log_item, then set up type and
     key, and finally to set the value. If said value is
     an allocated string, the log_item's alloc should be
     bitwise or'd with LOG_ITEM_FREE_VALUE.

  @param  ll        the log_line to work on
  @param  t         the item-type

  @retval !nullptr  a pointer to the log_item's log_data, for easy chaining:
                    line_item_set(...)->data_integer= 1;
  @retval  nullptr  could not create a log_item in given log_line
*/
DEFINE_METHOD(log_item_data *, log_builtins_imp::line_item_set,
              (log_line * ll, log_item_type t)) {
  return log_line_item_set_with_key(ll, t, nullptr, LOG_ITEM_FREE_NONE);
}

/**
  Dynamically allocate and initialize a log_line.

  @retval nullptr  could not set up buffer (too small?)
  @retval other    address of the newly initialized log_line
*/
DEFINE_METHOD(log_line *, log_builtins_imp::line_init, ()) {
  return log_line_init();
}

/**
  Release a log_line allocated with line_init()

  @param  ll       a log_line previously allocated with line_init()
*/
DEFINE_METHOD(void, log_builtins_imp::line_exit, (log_line * ll)) {
  log_line_exit(ll);
}

/**
  How many items are currently set on the given log_line?

  @param   ll     the log-line to examine

  @retval         the number of items set
*/
DEFINE_METHOD(int, log_builtins_imp::line_item_count, (log_line * ll)) {
  return log_line_item_count(ll);
}

/**
  Test whether a given type is presumed present on the log line.

  @param  ll  the log_line to examine
  @param  m   the log_type to test for

  @retval  0  not present
  @retval !=0 present
*/
DEFINE_METHOD(log_item_type_mask, log_builtins_imp::line_item_types_seen,
              (log_line * ll, log_item_type_mask m)) {
  return log_line_item_types_seen(ll, m);
}

/**
  Get an iterator for the items in a log_line.
  For now, only one iterator may exist per log_line.

  @param  ll  the log_line to examine

  @retval     a log_iterm_iter, or nullptr on failure
*/
DEFINE_METHOD(log_item_iter *, log_builtins_imp::line_item_iter_acquire,
              (log_line * ll)) {
  if (ll == nullptr) return nullptr;

  // If the default iter has already been claimed, refuse to overwrite it.
  if (ll->iter.ll != nullptr) return nullptr;

  ll->iter.ll = ll;
  ll->iter.index = -1;

  return &ll->iter;
}

/**
  Release an iterator for the items in a log_line.

  @param  it  the iterator to release
*/
DEFINE_METHOD(void, log_builtins_imp::line_item_iter_release,
              (log_item_iter * it)) {
  DBUG_ASSERT(it != nullptr);
  DBUG_ASSERT(it->ll != nullptr);

  it->ll = nullptr;
}

/**
  Use the log_line iterator to get the first item from the set.

  @param  it  the iterator to use

  @retval  pointer to the first log_item in the collection, or nullptr
*/
DEFINE_METHOD(log_item *, log_builtins_imp::line_item_iter_first,
              (log_item_iter * it)) {
  DBUG_ASSERT(it != nullptr);
  DBUG_ASSERT(it->ll != nullptr);

  if (it->ll->count < 1) return nullptr;

  it->index = 0;
  return &it->ll->item[it->index];
}

/**
  Use the log_line iterator to get the next item from the set.

  @param  it  the iterator to use

  @retval  pointer to the next log_item in the collection, or nullptr
*/
DEFINE_METHOD(log_item *, log_builtins_imp::line_item_iter_next,
              (log_item_iter * it)) {
  DBUG_ASSERT(it != nullptr);
  DBUG_ASSERT(it->ll != nullptr);
  DBUG_ASSERT(it->index >= 0);

  it->index++;

  if (it->index >= it->ll->count) return nullptr;

  return &it->ll->item[it->index];
}

/**
  Use the log_line iterator to get the current item from the set.

  @param  it  the iterator to use

  @retval  pointer to the current log_item in the collection, or nullptr
*/
DEFINE_METHOD(log_item *, log_builtins_imp::line_item_iter_current,
              (log_item_iter * it)) {
  DBUG_ASSERT(it != nullptr);
  DBUG_ASSERT(it->ll != nullptr);
  DBUG_ASSERT(it->index >= 0);

  if (it->index >= it->ll->count) return nullptr;

  return &it->ll->item[it->index];
}

/**
  Complete, filter, and write submitted log items.

  This expects a log_line collection of log-related key/value pairs,
  e.g. from log_message().

  Where missing, timestamp, priority, thread-ID (if any) and so forth
  are added.

  Log item source services, log item filters, and log item sinks are
  then called; then all applicable resources are freed.

  This interface is intended to facilitate the building of submission
  interfaces other than the variadic message() one below.  See the
  example fluent C++ LogEvent() wrapper for an example of how to leverage
  it.

  @param           ll                   key/value pairs describing info to log

  @retval          int                  number of fields in created log line
*/
DEFINE_METHOD(int, log_builtins_imp::line_submit, (log_line * ll)) {
  return log_line_submit(ll);
}

/**
  Submit a log-message for log "log_type".
  Variadic convenience function for logging.

  This fills in the array that is used by the filter and log-writer
  services. Where missing, timestamp, priority, and thread-ID (if any)
  are added. Log item source services, log item filters, and log item
  writers are called.

  The variadic list accepts a list of "assignments" of the form
  - log_item_type, value,         for well-known types, and
  - log_item_type, key, value,    for ad-hoc types (LOG_ITEM_GEN_*)

  As its last item, the list should have
  - an element of type LOG_ITEM_LOG_MESSAGE, containing a printf-style
    format string, followed by all variables necessary to satisfy the
    substitutions in that string

    OR

  - an element of type LOG_ITEM_LOG_LOOKUP, containing a MySQL error code,
    which will be looked up in the list or regular error messages, followed
    by all variables necessary to satisfy the substitutions in that string

    OR

  - an element of type LOG_ITEM_LOG_VERBATIM, containing a string that will
    be used directly, with no % substitutions

  see log_vmessage() for more information.

  @param           log_type             what log should this go to?
  @param           ...                  fields: LOG_ITEM_* tag, [[key], value]
  @retval          int                  return value of log_vmessage()
*/
DEFINE_METHOD(int, log_builtins_imp::message, (int log_type, ...)) {
  va_list lili;
  int ret;

  va_start(lili, log_type);
  ret = log_vmessage(log_type, lili);
  va_end(lili);

  return ret;
}

/*
  Escape \0 bytes, add \0 terminator. For log-sinks that terminate in
  an API using C-strings.

  @param  li  list_item to process

  @retval  -1 out of memory
  @retval  0  success
*/
DEFINE_METHOD(int, log_builtins_imp::sanitize, (log_item * li)) {
  size_t in_len = li->data.data_string.length, out_len, len;
  const char *in_start = li->data.data_string.str, *in_read;
  char *out_start = nullptr, *out_write;
  int nuls_found = 0;

  DBUG_ASSERT((li != nullptr) && (li->item_class == LOG_LEX_STRING));

  // find out how many \0 to escape
  for (in_read = in_start, len = in_len;
       ((in_read = (const char *)memchr(in_read, '\0', len)) != nullptr);) {
    nuls_found++;
    in_read++;  // skip over \0
    len = in_len - (in_read - in_start);
  }

  /*
    Current length + 3 extra for each NUL so we can escape it + terminating NUL
  */
  out_len = in_len + (nuls_found * 3) + 1;

  if ((out_start = (char *)my_malloc(key_memory_log_error_loaded_services,
                                     out_len, MYF(0))) == nullptr)
    return -1;

  /*
    copy over
  */
  in_read = in_start;
  out_write = out_start;

  while (nuls_found--) {
    // copy part before NUL
    len = strlen(in_read);
    strcpy(out_write, in_read);
    out_write += len;

    // add escaped NUL
    strcpy(out_write, "\\000");
    out_write += 4;
    in_read += (len + 1);
  }

  // calculate tail (with no further NUL bytes) length
  len = (in_read > in_start) ? (in_read - in_start) : in_len;

  // copy tail
  strncpy(out_write, in_read, len);

  /*
    NUL terminate. (the formula above always gives a minimum out-size of 1.)
  */
  out_start[out_len - 1] = '\0';

  if (li->alloc & LOG_ITEM_FREE_VALUE) {
    my_free((void *)in_start);
  }

  li->data.data_string.str = out_start;
  li->alloc |= LOG_ITEM_FREE_VALUE;

  return 0;
}

/**
  Return MySQL error message for a given error code.

  @param  mysql_errcode  the error code the message for which to look up

  @retval                the message (a printf-style format string)
*/
DEFINE_METHOD(const char *, log_builtins_imp::errmsg_by_errcode,
              (int mysql_errcode)) {
  return error_message_for_error_log(mysql_errcode);
}

/**
  Return MySQL error code for a given error symbol.

  @param  sym  the symbol to look up

  @retval  -1  failure
  @retval >=0  the MySQL error code
*/
DEFINE_METHOD(longlong, log_builtins_imp::errcode_by_errsymbol,
              (const char *sym)) {
  return mysql_symbol_to_errno(sym);
}

/**
  Convenience function: Derive a log label ("error", "warning",
  "information") from a severity.

  @param   prio       the severity/prio in question

  @return             a label corresponding to that priority.
  @retval  "Error"    for prio of ERROR_LEVEL or higher
  @retval  "Warning"  for prio of WARNING_LEVEL
  @retval  "Note"     otherwise
*/
DEFINE_METHOD(const char *, log_builtins_imp::label_from_prio, (int prio)) {
  return log_label_from_prio(prio);
}

/**
  open an error log file

  @param       file          if beginning with '.':
                               @@global.log_error, except with this extension
                             otherwise:
                               use this as file name in the same location as
                               @@global.log_error

                             Value not contain folder separators!

  @param[out]  my_errstream  an error log handle, or nullptr on failure

  @retval      0             success
  @retval     -1             EINVAL: my_errlog
  @retval     -2             EINVAL: invalid file name / extension
  @retval     -3             OOM: could not allocate file handle
  @retval     -4             couldn't lock lock
  @retval     -5             couldn't write to given location
*/
DEFINE_METHOD(int, log_builtins_imp::open_errstream,
              (const char *file, void **my_errstream)) {
  log_errstream *les;
  int rr;

  if (my_errstream == nullptr) return -1;

  *my_errstream = nullptr;

  les = (log_errstream *)my_malloc(key_memory_log_error_loaded_services,
                                   sizeof(log_errstream), MYF(0));

  if (les == nullptr) return -3;

  new (les) log_errstream();

  if (mysql_mutex_init(0, &les->LOCK_errstream, MY_MUTEX_INIT_FAST)) {
    my_free(les);
    return -4;
  }

  if ((file == nullptr) || (log_error_dest == nullptr) ||
      (!strcmp(log_error_dest, "stderr"))) {
    // using default stream, no file struct needed
    les->file = nullptr;
  } else if ((file[0] == '\0') || (strchr(file, FN_LIBCHAR) != nullptr)) {
    rr = -2;
    goto fail_with_free;
  } else {
    char errorlog_filename_buff[FN_REFLEN];
    char path[FN_REFLEN];
    size_t path_length;
    MY_STAT f_stat;

    dirname_part(path, log_error_dest, &path_length);

    rr = -5;

    if (file[0] == '.') {
      fn_format(errorlog_filename_buff, log_error_dest, path, file,
                MY_APPEND_EXT | MY_REPLACE_DIR);

    } else {
      fn_format(errorlog_filename_buff, file, path, ".les", MY_REPLACE_DIR);
    }

    if (my_stat(errorlog_filename_buff, &f_stat, MYF(0)) != nullptr) {
      if (!(f_stat.st_mode & MY_S_IWRITE)) goto fail_with_free;
    } else {
      if (path_length && my_access(path, (F_OK | W_OK))) goto fail_with_free;
    }

    les->file = my_fopen(errorlog_filename_buff,
                         O_APPEND | O_WRONLY | MY_FOPEN_BINARY, MYF(0));

    if (les->file == nullptr) goto fail_with_free;
  }

  *my_errstream = les;

  return 0;

fail_with_free:
  my_free(les);

  return rr;
}

/**
  write to an error log file previously opened with open_errstream()

  @param       my_errstream  a handle describing the log file
  @param       buffer        pointer to the string to write
  @param       length        length of the string to write

  @retval  0                 success
  @retval !0                 failure
*/
DEFINE_METHOD(int, log_builtins_imp::write_errstream,
              (void *my_errstream, const char *buffer, size_t length)) {
  log_errstream *les = (log_errstream *)my_errstream;

  if ((les == nullptr) || (les->file == nullptr))
    log_write_errstream(buffer, length);
  else {
    mysql_mutex_lock(&les->LOCK_errstream);
    fprintf(les->file, "%.*s\n", (int)length, buffer);
    fflush(les->file);
    mysql_mutex_unlock(&les->LOCK_errstream);
  }

  return 0;
}

/**
  are we writing to a dedicated errstream, or are we sharing it?

  @param       my_errstream  a handle describing the log file

  @retval <0                 error
  @retval  0                 not dedicated (multiplexed, stderr, ...)
  @retval  1                 dedicated
*/
DEFINE_METHOD(int, log_builtins_imp::dedicated_errstream,
              (void *my_errstream)) {
  log_errstream *les = (log_errstream *)my_errstream;

  if (les == nullptr) return -1;

  return (les->file != nullptr) ? 1 : 0;
}

/**
  close an error log file previously opened with open_errstream()

  @param       my_errstream  a handle describing the log file

  @retval      0          success
  @retval     !0          failure
*/
DEFINE_METHOD(int, log_builtins_imp::close_errstream, (void **my_errstream)) {
  int rr;

  if (my_errstream == nullptr) return -1;

  log_errstream *les = (log_errstream *)(*my_errstream);

  if (les == nullptr) return -2;

  *my_errstream = nullptr;

  if (les->file != nullptr) {
    my_fclose(les->file, MYF(0));
    // Continue to log after closing, you'll log to stderr. That'll learn ya.
    les->file = nullptr;
  }

  rr = mysql_mutex_destroy(&les->LOCK_errstream);

  my_free(les);

  return rr;
}

/*
  Service: some stand-ins for string functions we need until they are
  implemented in a more comprehensive service.
  3rd party services should not rely on these being here forever.
*/

/**
  Wrapper for my_malloc()

  Alloc (len+1) bytes.

  @param len  length of string to copy
*/
DEFINE_METHOD(void *, log_builtins_string_imp::malloc, (size_t len)) {
  return my_malloc(key_memory_log_error_loaded_services, len, MYF(0));
}

/**
  Wrapper for my_strndup()

  Alloc (len+1) bytes, then copy len bytes from fm, and \0 terminate.
  Like my_strndup(), and unlike strndup(), \0 in input won't end copying.

  @param fm   string to copy
  @param len  length of string to copy
*/
DEFINE_METHOD(char *, log_builtins_string_imp::strndup,
              (const char *fm, size_t len)) {
  return my_strndup(key_memory_log_error_loaded_services, fm, len, MYF(0));
}

/**
  Wrapper for my_free() - free allocated memory
*/
DEFINE_METHOD(void, log_builtins_string_imp::free, (void *ptr)) {
  return my_free(ptr);
}

/**
  Wrapper for strlen() - length of a nul-terminated byte string
*/
DEFINE_METHOD(size_t, log_builtins_string_imp::length, (const char *s)) {
  return strlen(s);
}

/**
  Wrapper for strchr() - find character in string, from the left
*/
DEFINE_METHOD(char *, log_builtins_string_imp::find_first,
              (const char *s, int c)) {
  return strchr((char *)s, c);
}

/**
  Wrapper for strrchr() - find character in string, from the right
*/
DEFINE_METHOD(char *, log_builtins_string_imp::find_last,
              (const char *s, int c)) {
  return strrchr((char *)s, c);
}

/**
  Compare two NUL-terminated byte strings

  Note that when comparing without length limit, the long string
  is greater if they're equal up to the length of the shorter
  string, but the shorter string will be considered greater if
  its "value" up to that point is greater:

  compare 'abc','abcd':      -100  (longer wins if otherwise same)
  compare 'abca','abcd':       -3  (higher value wins)
  compare 'abcaaaaa','abcd':   -3  (higher value wins)

  @param  a                 the first string
  @param  b                 the second string
  @param  len               compare at most this many characters --
                            0 for no limit
  @param  case_insensitive  ignore upper/lower case in comparison

  @retval -1                a < b
  @retval  0                a == b
  @retval  1                a > b
*/
DEFINE_METHOD(int, log_builtins_string_imp::compare,
              (const char *a, const char *b, size_t len,
               bool case_insensitive)) {
  return log_string_compare(a, b, len, case_insensitive);
}

/**
  Wrapper for vsnprintf()
  Replace all % in format string with variables from list

  @param  to    buffer to write the result to
  @param  n     size of that buffer
  @param  fmt   format string
  @param  ap    va_list with valuables for all substitutions in format string

  @retval       return value of vsnprintf
*/
DEFINE_METHOD(size_t, log_builtins_string_imp::substitutev,
              (char *to, size_t n, const char *fmt, va_list ap)) {
  return vsnprintf(to, n, fmt, ap);
}

/**
  Wrapper for vsnprintf()
  Replace all % in format string with variables from list
*/
DEFINE_METHOD(size_t, log_builtins_string_imp::substitute,
              (char *to, size_t n, const char *fmt, ...)) {
  size_t ret;
  va_list ap;

  va_start(ap, fmt);
  ret = vsnprintf(to, n, fmt, ap);
  va_end(ap);
  return ret;
}

/*
  Service: some stand-ins we need until certain other WLs are implemented.
  3rd party services should not rely on these being here for long.
*/

// Are we shutting down yet?  Windows EventLog needs to know.
DEFINE_METHOD(bool, log_builtins_tmp_imp::connection_loop_aborted, (void)) {
  return connection_events_loop_aborted();
}

DEFINE_METHOD(size_t, log_builtins_tmp_imp::notify_client,
              (void *thd, uint severity, uint code, char *to, size_t n,
               const char *format, ...)) {
  size_t ret = 0;

  if ((to != nullptr) && (n > 0)) {
    va_list ap;

    va_start(ap, format);
    ret = vsnprintf(to, n, format, ap);
    va_end(ap);

    push_warning((THD *)thd, (Sql_condition::enum_severity_level)severity, code,
                 to);
  }

  return ret;
}

  /*
    Service: expose syslog/eventlog to other components.
    3rd party services should not rely on these being here for long,
    as this may be merged into a possibly mysys API later.
  */

#ifdef _WIN32

/**
   Create a key in the Windows registry.
   We'll setup a "MySQL" key in the EventLog branch (RegCreateKey),
   set our executable name (GetModuleFileName) as file-name
   ("EventMessageFile"), then set the message types we expect to
   be logging ("TypesSupported").
   If the key does not exist, sufficient privileges will be required
   to create and configure it.  If the key does exist, opening it
   should be unprivileged; modifying will fail on insufficient
   privileges, but that is non-fatal.

  @param key          Name of the event generator.
                      (Only last part of the key, e.g. "MySQL")

  @return
     0 Success
    -1 Error
*/

static const char log_registry_prefix[] =
    "SYSTEM\\CurrentControlSet\\services\\eventlog\\Application\\";

static int log_eventlog_create_registry_entry(const char *key) {
  HKEY hRegKey = NULL;
  DWORD dwError = 0;
  TCHAR szPath[MAX_PATH];
  DWORD dwTypes;

  size_t l = sizeof(log_registry_prefix) + strlen(key) + 1;
  char *buff;

  int ret = 0;

  if ((buff = (char *)my_malloc(PSI_NOT_INSTRUMENTED, l, MYF(0))) == NULL)
    return -1;

  snprintf(buff, l, "%s%s", log_registry_prefix, key);

  // Opens the event source registry key; creates it first if required.
  dwError = RegCreateKey(HKEY_LOCAL_MACHINE, buff, &hRegKey);

  my_free(buff);

  if (dwError != ERROR_SUCCESS) {
    if (dwError == ERROR_ACCESS_DENIED) {
      my_message_stderr(0,
                        "Could not create or access the registry key needed "
                        "for the MySQL application\nto log to the Windows "
                        "EventLog. Run the application with sufficient\n"
                        "privileges once to create the key, add the key "
                        "manually, or turn off\nlogging for that application.",
                        MYF(0));
    }
    return -1;
  }

  /* Name of the PE module that contains the message resource */
  GetModuleFileName(NULL, szPath, MAX_PATH);

  /* Register EventMessageFile (DLL/exec containing event identifiers) */
  dwError = RegSetValueEx(hRegKey, "EventMessageFile", 0, REG_EXPAND_SZ,
                          (PBYTE)szPath, (DWORD)(strlen(szPath) + 1));
  if ((dwError != ERROR_SUCCESS) && (dwError != ERROR_ACCESS_DENIED)) ret = -1;

  /* Register supported event types */
  dwTypes =
      (EVENTLOG_ERROR_TYPE | EVENTLOG_WARNING_TYPE | EVENTLOG_INFORMATION_TYPE);
  dwError = RegSetValueEx(hRegKey, "TypesSupported", 0, REG_DWORD,
                          (LPBYTE)&dwTypes, sizeof dwTypes);
  if ((dwError != ERROR_SUCCESS) && (dwError != ERROR_ACCESS_DENIED)) ret = -1;

  RegCloseKey(hRegKey);

  return ret;
}
#endif

/**
  Wrapper for mysys' my_openlog.
  Opens/Registers a new handle for system logging.
  Note: It's a thread-unsafe function. It should either
  be invoked from the main thread or some extra thread
  safety measures need to be taken.

  @param name     Name of the event source / syslog ident.
  @param option   MY_SYSLOG_PIDS to log PID with each message.
  @param facility Type of program. Passed to openlog().

  @retval   0 Success
  @retval  -1 Error, log not opened
  @retval  -2 Error, not updated, using previous values
*/
DEFINE_METHOD(int, log_builtins_syseventlog_imp::open,
              (const char *name, int option, int facility)) {
  int ret = 0;
  mysql_mutex_lock(&THR_LOCK_log_syseventlog);

#ifndef _WIN32
  int opts = (option & MY_SYSLOG_PIDS) ? LOG_PID : 0;

  openlog(name, opts | LOG_NDELAY, facility);

#else

  HANDLE hEL_new;

  // OOM failsafe.  Not needed for syslog.
  if (name == NULL)
    ret = -1;
  else if ((log_eventlog_create_registry_entry(name) != 0) ||
           !(hEL_new = RegisterEventSource(NULL, name)))
    ret = (hEventLog == NULL) ? -1 : -2;
  else {
    // deregister previous
    if (hEventLog != NULL) DeregisterEventSource(hEventLog);
    hEventLog = hEL_new;
  }
#endif

  mysql_mutex_unlock(&THR_LOCK_log_syseventlog);
  return ret;
}

/**
  Wrapper for mysys' my_syslog.
  Sends message to the system logger. On Windows, the specified message is
  internally converted to UCS-2 encoding, while on other platforms, no
  conversion takes place and the string is passed to the syslog API as it is.

  @param level                Log level
  @param msg                  Message to be logged

  @retval   0  Success
  @retval <>0  Error
*/
DEFINE_METHOD(int, log_builtins_syseventlog_imp::write,
              (enum loglevel level, const char *msg)) {
  int ret = 0;
  mysql_mutex_lock(&THR_LOCK_log_syseventlog);

#ifdef _WIN32
  int _level = EVENTLOG_INFORMATION_TYPE;
  wchar_t buff[MAX_SYSLOG_MESSAGE_SIZE];
  wchar_t *u16buf = NULL;
  size_t nchars;
  uint dummy_errors;

  switch (level) {
    case INFORMATION_LEVEL:
    case SYSTEM_LEVEL:
      _level = EVENTLOG_INFORMATION_TYPE;
      break;
    case WARNING_LEVEL:
      _level = EVENTLOG_WARNING_TYPE;
      break;
    case ERROR_LEVEL:
      _level = EVENTLOG_ERROR_TYPE;
      break;
    default:
      DBUG_ASSERT(false);
  }

  if (hEventLog) {
    nchars = my_convert((char *)buff, sizeof(buff) - sizeof(buff[0]),
                        &my_charset_utf16le_bin, msg, MAX_SYSLOG_MESSAGE_SIZE,
                        &my_charset_utf8_bin, &dummy_errors);

    // terminate it with NULL
    buff[nchars / sizeof(wchar_t)] = L'\0';
    u16buf = buff;

    if (!ReportEventW(hEventLog, _level, 0, MSG_DEFAULT, NULL, 1, 0,
                      (LPCWSTR *)&u16buf, NULL))
      ret = -1;
  }

#else
  int _level = LOG_INFO;

  switch (level) {
    case INFORMATION_LEVEL:
    case SYSTEM_LEVEL:
      _level = LOG_INFO;
      break;
    case WARNING_LEVEL:
      _level = LOG_WARNING;
      break;
    case ERROR_LEVEL:
      _level = LOG_ERR;
      break;
    default:
      DBUG_ASSERT(false);
  }

  syslog(_level, "%s", msg);

#endif /* _WIN32 */

  mysql_mutex_unlock(&THR_LOCK_log_syseventlog);
  return ret;
}

/**
  Wrapper for mysys' my_closelog.
  Closes/de-registers the system logging handle.

  @retval   0 Success
  @retval <>0 Error
*/
DEFINE_METHOD(int, log_builtins_syseventlog_imp::close, (void)) {
  int ret = 0;
  mysql_mutex_lock(&THR_LOCK_log_syseventlog);

#ifndef _WIN32
  closelog();
#else
  if ((hEventLog != NULL) && (!DeregisterEventSource(hEventLog))) ret = -1;

  hEventLog = NULL;
#endif

  mysql_mutex_unlock(&THR_LOCK_log_syseventlog);
  return ret;
}

/**
  Temporary helper class to implement services' system variables
  handling against until the component framework supports
  per-component variables.
*/

/**
  constructor with variable name

  @param s  the variable's name as a constant string-with-length (LEX_CSTRING)
*/
LogVar::LogVar(LEX_CSTRING &s) {
  this->lv.type = LOG_ITEM_END;
  this->lv.item_class = LOG_UNTYPED;
  this->lv.key = s.str;
  this->lv.alloc = LOG_ITEM_FREE_NONE;
}

/**
  sanity check a new value for a system variable handled in a log_service

  @retval true   on failure  (e.g. a service called the value illegitimate)
  @retval false  on success  (no service had any complaints)
*/
int LogVar::check() {
  int rr;
  log_line ll;

  memset((void *)&ll, 0, sizeof(log_line));

  ll.count = 1;
  ll.item[0] = this->lv;

  log_line_item_set_with_key(&ll, LOG_ITEM_GEN_INTEGER, LOG_VAR_KEY_CHECK,
                             LOG_ITEM_FREE_NONE)
      ->data_integer = 1;

  rr = log_broadcast_sys_var_set(&ll);

  log_line_item_free_all(&ll);

  return rr != 0;  // if any services complained, signal true for "deny"
}

/**
  apply new value to a system variable handled in a log_service

  @retval true   on failure
  @retval false  on success
*/
int LogVar::update() {
  int rr;
  log_line ll;

  memset((void *)&ll, 0, sizeof(log_line));

  ll.count = 1;
  ll.item[0] = this->lv;

  log_line_item_set_with_key(&ll, LOG_ITEM_GEN_INTEGER, LOG_VAR_KEY_CHECK,
                             LOG_ITEM_FREE_NONE)
      ->data_integer = 0;

  rr = log_broadcast_sys_var_set(&ll);

  log_line_item_free_all(&ll);

  return rr < 1;  // if no services updated successfully, signal true for error
}

/**
  set value (of lex-string type)

  @param v  the value

  @retval   the LogVar, for easy fluent-style chaining.
*/
LogVar &LogVar::val(LEX_STRING &v) {
  this->lv.type = LOG_ITEM_GEN_LEX_STRING;
  this->lv.item_class = LOG_LEX_STRING;
  this->lv.data.data_string.str = v.str;
  this->lv.data.data_string.length = v.length;
  return *this;
}

/**
  set value (of NTBS type)

  @param v  the value

  @retval   the LogVar, for easy fluent-style chaining.
*/
LogVar &LogVar::val(const char *v) {
  this->lv.type = LOG_ITEM_GEN_LEX_STRING;
  this->lv.item_class = LOG_LEX_STRING;
  this->lv.data.data_string.str = v;
  this->lv.data.data_string.length = (v == nullptr) ? 0 : strlen(v);
  return *this;
}

/**
  set value (of long long integer type)

  @param v  the value

  @retval   the LogVar, for easy fluent-style chaining.
*/
LogVar &LogVar::val(longlong v) {
  this->lv.type = LOG_ITEM_GEN_INTEGER;
  this->lv.item_class = LOG_INTEGER;
  this->lv.data.data_integer = v;
  return *this;
}

/**
  set value (of double type)

  @param v  the value

  @retval   the LogVar, for easy fluent-style chaining.
*/
LogVar &LogVar::val(double v) {
  this->lv.type = LOG_ITEM_GEN_FLOAT;
  this->lv.item_class = LOG_FLOAT;
  this->lv.data.data_float = v;
  return *this;
}

/**
  set non-default service group

  @param g  the group

  @retval   the LogVar, for easy fluent-style chaining.
*/
LogVar &LogVar::group(const char *g) {
  this->service_group = g;
  return *this;
}

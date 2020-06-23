/* Copyright (c) 2017, 2020, Oracle and/or its affiliates.

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

#include "log_sink_buffer.h"
#include "log_sink_perfschema.h"
#include "log_sink_trad.h"

#include "mysys_err.h"

#include <mysql/components/services/log_shared.h>  // data types

#include "my_time.h"  // str_to_datetime()

#include "sql/current_thd.h"  // current_thd
#include "sql/log.h"          // make_iso8601_timestamp, log_write_errstream,
                              // log_get_thread_id, mysql_errno_to_symbol,
                              // mysql_symbol_to_errno, log_vmessage,
                              // error_message_for_error_log
#include "sql/mysqld.h"       // opt_log_(timestamps|error_services),
#include "sql/sql_class.h"    // THD
#include "sql/tztime.h"       // my_tz_OFFSET0

// Must come after sql/log.h.
#include "mysql/components/services/log_builtins.h"

#ifndef _WIN32
#include <syslog.h>
#else
#include <stdio.h>

#include "my_sys.h"

#include "my_time.h"  // str_to_datetime()

extern CHARSET_INFO my_charset_utf16le_bin;  // used in Windows EventLog
static HANDLE hEventLog = NULL;              // global
#define MSG_DEFAULT 0xC0000064L
#endif

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
  (The loadable log-service is a singleton, which enforces that at
  most one instance of it exists. The logger-core has its own lock
  that serializes access to it. That however does not prevent the
  logger core and system variable updates from using Eventlog functions
  concurrently. This lock guards against that. It also serializes
  any other (non-error logging) users of this service.
*/
static mysql_mutex_t THR_LOCK_log_syseventlog;

/**
  Subsystem initialized and ready to use?
*/
bool log_builtins_inited = false;

/**
  Name of the interface that log-services implements.
*/
#define LOG_SERVICES_PREFIX "log_service"

/**
  Chain of log-service instances.
 (Each service can have no/one/several instances.)
*/
log_service_instance *log_service_instances = nullptr;

/**
  The first configured writer that also has a log-reader
  is the source for the "data" field in performance_schema.error_log.
*/
log_service_instance *log_sink_pfs_source = nullptr;

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

/// What mode is error-logging in (e.g. are loadable services available yet)?
static enum log_error_stage log_error_stage_current =
    LOG_ERROR_STAGE_BUFFERING_EARLY;

/// Set error-logging stage hint (e.g. are loadable services available yet?).
void log_error_stage_set(enum log_error_stage les) {
  log_error_stage_current = les;
}

/// What mode is error-logging in (e.g. are loadable services available yet)?
enum log_error_stage log_error_stage_get() { return log_error_stage_current; }

/**
  Test whether a given log-service name refers to a built-in
  service (built-in filter or built-in sink at this point).

  @param  name   the name -- either just the component's, or
                 a fully qualified service.component
  @param  len    the length of the aforementioned name
  @return flags for built-in|singleton|filter (if built-in filter)
          or flags for built-in|singleton|sink (if built-in sink)
          otherwise LOG_SERVICE_UNSPECIFIED
*/
static int log_service_check_if_builtin(const char *name, size_t len) {
  const size_t builtin_len = sizeof(LOG_SERVICES_PREFIX) - 1;

  if ((len > (builtin_len + 1)) && (name[builtin_len] == '.') &&
      (0 == strncmp(name, LOG_SERVICES_PREFIX, builtin_len))) {
    name += builtin_len;
    len -= builtin_len;
  }

  if ((len == sizeof(LOG_BUILTINS_FILTER) - 1) &&
      (0 == strncmp(name, LOG_BUILTINS_FILTER, len)))
    return LOG_SERVICE_BUILTIN | LOG_SERVICE_FILTER | LOG_SERVICE_SINGLETON;

  if ((len == sizeof(LOG_BUILTINS_SINK) - 1) &&
      (0 == strncmp(name, LOG_BUILTINS_SINK, len)))
    return LOG_SERVICE_BUILTIN | LOG_SERVICE_SINK | LOG_SERVICE_SINGLETON |
           LOG_SERVICE_LOG_PARSER | LOG_SERVICE_PFS_SUPPORT;

  if ((len == sizeof(LOG_BUILTINS_BUFFER) - 1) &&
      (0 == strncmp(name, LOG_BUILTINS_BUFFER, len)))
    return LOG_SERVICE_BUILTIN | LOG_SERVICE_SINK | LOG_SERVICE_SINGLETON |
           LOG_SERVICE_BUFFER;

  return LOG_SERVICE_UNSPECIFIED;
}

/**
  Test whether given service has *all* of the given characteristics.
  (See log_service_chistics for a list!)

  @param  sce              service cache entry for the service in question
  @param  required_flags   flags we're interested in (bitwise or'd)

  @retval                  true if all given flags are present, false otherwise
*/
static inline bool log_service_has_characteristics(log_service_cache_entry *sce,
                                                   int required_flags) {
  return ((sce->chistics & required_flags) == required_flags);
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
    {STRING_WITH_LEN("--ERROR--"), LOG_UNTYPED, LOG_ITEM_END},
    {STRING_WITH_LEN("log_type"), LOG_INTEGER, LOG_ITEM_LOG_TYPE},
    {STRING_WITH_LEN("err_code"), LOG_INTEGER, LOG_ITEM_SQL_ERRCODE},
    {STRING_WITH_LEN("err_symbol"), LOG_CSTRING, LOG_ITEM_SQL_ERRSYMBOL},
    {STRING_WITH_LEN("SQL_state"), LOG_CSTRING, LOG_ITEM_SQL_STATE},
    {STRING_WITH_LEN("OS_errno"), LOG_INTEGER, LOG_ITEM_SYS_ERRNO},
    {STRING_WITH_LEN("OS_errmsg"), LOG_CSTRING, LOG_ITEM_SYS_STRERROR},
    {STRING_WITH_LEN("source_file"), LOG_CSTRING, LOG_ITEM_SRC_FILE},
    {STRING_WITH_LEN("source_line"), LOG_INTEGER, LOG_ITEM_SRC_LINE},
    {STRING_WITH_LEN("function"), LOG_CSTRING, LOG_ITEM_SRC_FUNC},
    {STRING_WITH_LEN("subsystem"), LOG_CSTRING, LOG_ITEM_SRV_SUBSYS},
    {STRING_WITH_LEN("component"), LOG_CSTRING, LOG_ITEM_SRV_COMPONENT},
    {STRING_WITH_LEN("user"), LOG_LEX_STRING, LOG_ITEM_MSC_USER},
    {STRING_WITH_LEN("host"), LOG_LEX_STRING, LOG_ITEM_MSC_HOST},
    {STRING_WITH_LEN("thread"), LOG_INTEGER, LOG_ITEM_SRV_THREAD},
    {STRING_WITH_LEN("query_id"), LOG_INTEGER, LOG_ITEM_SQL_QUERY_ID},
    {STRING_WITH_LEN("table"), LOG_CSTRING, LOG_ITEM_SQL_TABLE_NAME},
    {STRING_WITH_LEN("prio"), LOG_INTEGER, LOG_ITEM_LOG_PRIO},
    {STRING_WITH_LEN("label"), LOG_CSTRING, LOG_ITEM_LOG_LABEL},
    {STRING_WITH_LEN("verbatim"), LOG_CSTRING, LOG_ITEM_LOG_VERBATIM},
    {STRING_WITH_LEN("msg"), LOG_CSTRING, LOG_ITEM_LOG_MESSAGE},
    {STRING_WITH_LEN("msg_id"), LOG_INTEGER, LOG_ITEM_LOG_LOOKUP},
    {STRING_WITH_LEN("time"), LOG_CSTRING, LOG_ITEM_LOG_TIMESTAMP},
    {STRING_WITH_LEN("ts"), LOG_INTEGER, LOG_ITEM_LOG_TS},
    {STRING_WITH_LEN("buffered"), LOG_INTEGER, LOG_ITEM_LOG_BUFFERED},
    {STRING_WITH_LEN("and_n_more"), LOG_INTEGER, LOG_ITEM_LOG_SUPPRESSED},
    /*
      We should never see the following key names in normal operations
      (but see the user-specified key instead).  These have entries all
      the same, covering the entirety of log_item_type, so we can use the
      usual mechanisms for type-to-class mapping etc.
      We could set the names to nullptr, but they're not much overhead, add
      readability, and allow for easily creating debug info of the form,
      "%s:%s=\"%s\"", wellknown_name, item->key, item->value
    */
    {STRING_WITH_LEN("misc_float"), LOG_FLOAT, LOG_ITEM_GEN_FLOAT},
    {STRING_WITH_LEN("misc_integer"), LOG_INTEGER, LOG_ITEM_GEN_INTEGER},
    {STRING_WITH_LEN("misc_string"), LOG_LEX_STRING, LOG_ITEM_GEN_LEX_STRING},
    {STRING_WITH_LEN("misc_cstring"), LOG_CSTRING, LOG_ITEM_GEN_CSTRING},
    {STRING_WITH_LEN("misc_buffer"), LOG_BUFFER, LOG_ITEM_GEN_BUFFER}};

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
  return (t &
          (LOG_ITEM_GEN_CSTRING | LOG_ITEM_GEN_LEX_STRING |
           LOG_ITEM_GEN_INTEGER | LOG_ITEM_GEN_FLOAT | LOG_ITEM_GEN_BUFFER));
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
  if ((*str = const_cast<char *>(li->data.data_string.str)) == nullptr)
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
  if (li->alloc & LOG_ITEM_FREE_KEY) my_free(const_cast<char *>(li->key));

  if (li->alloc & LOG_ITEM_FREE_VALUE) {
    if (li->item_class == LOG_LEX_STRING)
      my_free(const_cast<char *>(li->data.data_string.str));
    else if (li->item_class == LOG_BUFFER) /* purecov: begin inspected */
      my_free(const_cast<char *>(li->data.data_buffer.str));
    else                  // free() is only defined on string and buffer
      DBUG_ASSERT(false); /* purecov: end */
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
  Get log-line's output buffer.
  If the logger core provides this buffer, the log-service may use it
  to assemble its output therein and implicitly return it to the core.
  Participation is required for services that support populating
  performance_schema.error_log, and optional for all others.

  @param  ll  the log_line to examine

  @retval  nullptr    success, an output buffer is available
  @retval  otherwise  failure, no output buffer is available
*/
log_item *log_line_get_output_buffer(log_line *ll) {
  if ((ll == nullptr) || (ll->output_buffer.item_class != LOG_BUFFER))
    return nullptr;
  return &ll->output_buffer;
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

  // Fill the gap if needed (if there are more elements and we're not the tail)
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

  DBUG_ASSERT(
      ((alloc & LOG_ITEM_FREE_VALUE) == 0) || (li->item_class == LOG_CSTRING) ||
      (li->item_class == LOG_LEX_STRING) || (li->item_class == LOG_BUFFER));

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
  Fails gracefully if no log_item_data is supplied, so it can safely
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
  Fails gracefully if no log_item_data is supplied, so it can safely
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
  Set a string buffer on a log_item.
  On success, the caller should change the item_class to LOG_BUFFER.

  @param  lid    log_item_data struct to set the value on
  @param  s      pointer to string-buffer (non-const)
  @param  s_len  buffer-size

  @retval true   could not set a valid buffer
  @retval false  item was assigned a buffer
*/
bool log_item_set_buffer(log_item_data *lid, char *s, size_t s_len) {
  if (lid != nullptr) {            // if we have an item ...
    lid->data_buffer.str = s;      // set the buffer on it
    if (s == nullptr) {            // if the buffer is NULL, zero the length
      lid->data_buffer.length = 0; /* purecov: begin inspected */
      return true;                 /* purecov: end */
    }
    lid->data_buffer.length = s_len;  // set the given buffer-size
    return false;                     // signal success
  }

  // no item => failure
  return true; /* purecov: inspected */
}

/**
  Set a string value on a log_item.
  Fails gracefully if no log_item_data is supplied, so it can safely
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
  Fails gracefully if no log_item_data is supplied, so it can safely
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
  Derive the event's priority (SYSTEM_LEVEL, ERROR_LEVEL, ...)
  from a textual label. If the label can not be identified,
  default to ERROR_LEVEL as it is better to keep something
  that needn't be kept than to discard something that shouldn't
  be.

  @param  label  The prio label as a \0 terminated C-string.

  @retval  the priority (as an enum loglevel)
*/
enum loglevel log_prio_from_label(const char *label) {
  if (0 == native_strcasecmp(label, "SYSTEM")) return SYSTEM_LEVEL;
  if (0 == native_strcasecmp(label, "WARNING")) return WARNING_LEVEL;
  if (0 == native_strcasecmp(label, "NOTE")) return INFORMATION_LEVEL;

  return ERROR_LEVEL; /* purecov: inspected */
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
  static ulonglong previous_microtime = 0;

  DBUG_TRACE;

  /*
    The log-services we'll call below are likely to change the default
    iter. Since log-services are allowed to call the logger, we'll save
    the iter on entry and restore it on exit to be properly re-entrant
    in that regard.
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

      DBUG_EXECUTE_IF("log_error_normalize", {
        /*
          If previous value is significantly larger than the epoch,
          normalization has just been turned on, and we've remembered
          a contemporary timestamp, rather than a normalized one, so
          we reset it here.
        */
        if (previous_microtime >= 1000000) previous_microtime = 0;
        /*
          Now, we reset the current timestamp. This will result in it
          being forced to the value of ( previous + 1), generating a
          sequence of 1, 2, 3, ... for normalized timestamps.
          This sequence restarts any time log_error_normalize is toggled
          on (i.e. changed to on from having been off).
        */
        now = 0;
      });

      // enforce uniqueness of timestamps
      if (now <= previous_microtime)
        now = ++previous_microtime;
      else
        previous_microtime = now;

      make_iso8601_timestamp(local_time_buff, now,
                             iso8601_sysvar_logtimestamps);

      d = log_line_item_set(ll, LOG_ITEM_LOG_TIMESTAMP);
      d->data_string.str = local_time_buff;
      d->data_string.length = strlen(d->data_string.str);
    }

    /* auto-add a ts item if needed */
    if (!(ll->seen & LOG_ITEM_LOG_TS) && !log_line_full(ll)) {
      log_item_data *d;
      ulonglong now = my_milli_time();

      DBUG_EXECUTE_IF("log_error_normalize", { now = 0; });

      d = log_line_item_set(ll, LOG_ITEM_LOG_TS);
      d->data_integer = now;
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

    /*
      We were called before even the buffered sink (and our locks)
      were set up. This usually means that something went
      catastrophically wrong, so we'll make sure the information
      (e.g. cause of failure) isn't lost.
    */
    if (!log_builtins_inited)
      log_sink_buffer(nullptr, ll);

    else {
      mysql_rwlock_rdlock(&THR_LOCK_log_stack);

      // set up output buffer
      char capture_buffer[LOG_BUFF_MAX];

      log_item_init(&ll->output_buffer);
      // Set up a valid item. It's not needed here, but it's a good habit.
      log_item_set_with_key(&ll->output_buffer, LOG_ITEM_GEN_BUFFER,
                            "output_buffer", LOG_ITEM_FREE_NONE);
      // Attach the output buffer to the item and set the item-class.
      log_item_set_buffer(&ll->output_buffer.data, capture_buffer,
                          sizeof(capture_buffer));

      /*
        Call all configured log-services (sources, filters, sinks)
        on this log-event.

        sources:
        Add info from other log item sources,
        e.g. that supplied by the client on connect using mysql_options4();

        filters:
        Remove or modify entries

        sinks:
        Write logs
      */

      log_service_cache_entry *sce;
      log_service_instance *lsi = log_service_instances;

      while ((lsi != nullptr) && ((sce = lsi->sce) != nullptr)) {
        // make capture buffer valid if primary log-writer
        ll->output_buffer.item_class =
            (lsi == log_sink_pfs_source) ? LOG_BUFFER : LOG_UNTYPED;

        // buffered logging trumps everything
        if (sce->chistics & LOG_SERVICE_BUFFER)
          log_sink_buffer(lsi->instance, ll);

        // loadable services
        else if (!(sce->chistics & LOG_SERVICE_BUILTIN)) {
          SERVICE_TYPE(log_service) * ls;

          ls = reinterpret_cast<SERVICE_TYPE(log_service) *>(sce->service);

          if (ls != nullptr) ls->run(lsi->instance, ll);

        }  // built-in filter
        else if (log_service_has_characteristics(
                     sce, (LOG_SERVICE_BUILTIN | LOG_SERVICE_FILTER)))
          log_builtins_filter_run(log_filter_builtin_rules, ll);

        // built-in sink
        else if (log_service_has_characteristics(
                     sce, (LOG_SERVICE_BUILTIN | LOG_SERVICE_SINK)))
          log_sink_trad(lsi->instance, ll);

        lsi = lsi->next;
      }

      /*
        If there is anything in the capture buffer, log it to
        performance_schema.error_log.
      */
      if ((log_error_stage_get() ==
           LOG_ERROR_STAGE_EXTERNAL_SERVICES_AVAILABLE) &&
          (ll->output_buffer.type == LOG_ITEM_RET_BUFFER) &&
          (ll->output_buffer.data.data_buffer.length > 0))
        log_sink_perfschema(nullptr, ll);

      // release output buffer if changed by the service
      log_item_free(&ll->output_buffer);

      mysql_rwlock_unlock(&THR_LOCK_log_stack);
    }

#if !defined(DBUG_OFF)
    /*
      Assert that we're not given anything but server error-log codes
      or global error codes (shared between MySQL server and clients).
      If your code bombs out here, check whether you're trying to log
      using an error-code in the range intended for messages that are
      sent to the client, not the error-log, (< ER_SERVER_RANGE_START).
    */
    if (ll->seen & LOG_ITEM_SQL_ERRCODE) {
      int n = log_line_index_by_type(ll, LOG_ITEM_SQL_ERRCODE);
      if (n >= 0) {
        int ec = (int)ll->item[n].data.data_integer;
        DBUG_ASSERT((ec < 1) || (ec >= EE_ERROR_FIRST && ec <= EE_ERROR_LAST) ||
                    (ec >= ER_SERVER_RANGE_START));
      }
    }
#endif

    // release any memory that might need it
    log_line_item_free_all(ll);
  }

  ll->iter = iter_save;

  return ll->count;
}

/**
  Make and return an ISO 8601 / RFC 3339 compliant timestamp.
  Accepts the log_timestamps global variable in its third parameter.

  @param buf       A buffer of at least 26 bytes to store the timestamp in
                   (19 + tzinfo tail + \0)
  @param utime     Microseconds since the epoch
  @param mode      if 0, use UTC; if 1, use local time

  @retval          length of timestamp (excluding \0)
*/
int make_iso8601_timestamp(char *buf, ulonglong utime,
                           enum enum_iso8601_tzmode mode) {
  struct tm my_tm;
  char tzinfo[8] = "Z";  // max 6 chars plus \0
  size_t len;
  time_t seconds;

  seconds = utime / 1000000;
  utime = utime % 1000000;

  if (mode == iso8601_sysvar_logtimestamps)
    mode = (opt_log_timestamps == 0) ? iso8601_utc : iso8601_system_time;

  if (mode == iso8601_utc)
    gmtime_r(&seconds, &my_tm);
  else if (mode == iso8601_system_time) {
    localtime_r(&seconds, &my_tm);

#ifdef HAVE_TM_GMTOFF
    /*
      The field tm_gmtoff is the offset (in seconds) of the time represented
      from UTC, with positive values indicating east of the Prime Meridian.
      Originally a BSDism, this is also supported in glibc, so this should
      cover the majority of our platforms.
    */
    long tim = -my_tm.tm_gmtoff;
#else
    /*
      Work this out "manually".
    */
    struct tm my_gm;
    long tim, gm;
    gmtime_r(&seconds, &my_gm);
    gm = (my_gm.tm_sec + 60 * (my_gm.tm_min + 60 * my_gm.tm_hour));
    tim = (my_tm.tm_sec + 60 * (my_tm.tm_min + 60 * my_tm.tm_hour));
    tim = gm - tim;
#endif
    char dir = '-';

    if (tim < 0) {
      dir = '+';
      tim = -tim;
    }
    snprintf(tzinfo, sizeof(tzinfo), "%c%02u:%02u", dir,
             (unsigned int)((tim / (60 * 60)) % 100),
             (unsigned int)((tim / 60) % 60));
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
  Parse a ISO8601 timestamp and return the number of microseconds
  since the epoch. Heeds +/- timezone info if present.

  @see make_iso8601_timestamp()

  @param timestamp  an ASCII string containing an ISO8601 timestamp
  @param len        Length in bytes of the aforementioned string

  @return microseconds since the epoch
*/
ulonglong iso8601_timestamp_to_microseconds(const char *timestamp, size_t len) {
  MYSQL_TIME mt;
  MYSQL_TIME_STATUS status;
  my_time_t t;
  bool in_dst_time_gap;

  if (str_to_datetime(timestamp, len, &mt, 0, &status) ||
      ((t = my_tz_OFFSET0->TIME_to_gmt_sec(&mt, &in_dst_time_gap)) <= 0))
    return 0;

  return ((ulonglong)t) * 1000000ULL + mt.second_part;
}

/**
  Helper: get token from error stack configuration string

  @param[in,out]  s   start of the token (may be positioned on whitespace
                      on call; this will be adjusted to the first non-white
                      character)
  @param[out]     e   end of the token
  @param[in,out]  d   delimiter (in: last used, \0 if none; out: detected here)

  @retval         <0  an error occured
  @retval        >=0  the length in bytes of the token
*/
static ssize_t log_builtins_stack_get_service_from_var(const char **s,
                                                       const char **e,
                                                       char *d) {
  DBUG_ASSERT(s != nullptr);
  DBUG_ASSERT(e != nullptr);

  // proceed to next service (skip whitespace, and the delimiter once defined)
  while (isspace(**s) || ((*d != '\0') && (**s == *d))) (*s)++;

  *e = *s;

  // find end of service
  while ((**e != '\0') && !isspace(**e)) {
    if ((**e == ';') || (**e == ',')) {
      if (*d == '\0')  // no delimiter determined yet
      {
        if (*e == *s)  // token may not start with a delimiter
          return LOG_ERROR_UNEXPECTED_DELIMITER_FOUND;
        *d = **e;            // save the delimiter we found
      } else if (**e != *d)  // different delimiter than last time: error
        return LOG_ERROR_MIXED_DELIMITERS;
    }
    if (**e == *d)  // found a valid delimiter; end scan
      goto done;
    (*e)++;  // valid part of token found, go on!
  }

done:
  return (ssize_t)(*e - *s);
}

/**
  Look up a log service by name (in the service registry).

  @param        name    name of the component
  @param        len     length of that name

  @retval               a handle to that service.
*/
static my_h_service log_service_get_by_name(const char *name, size_t len) {
  char buf[128];
  my_h_service service = nullptr;
  size_t needed;

  needed =
      snprintf(buf, sizeof(buf), LOG_SERVICES_PREFIX ".%.*s", (int)len, name);

  if (needed > sizeof(buf)) return service;

  if ((!srv_registry->acquire(buf, &service)) && (service != nullptr)) {
    SERVICE_TYPE(log_service) * ls;

    if ((ls = reinterpret_cast<SERVICE_TYPE(log_service) *>(service)) ==
        nullptr) {
      srv_registry->release(service);
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

  DBUG_ASSERT(sce->opened == 0);

  if (sce->service != nullptr) srv_registry->release(sce->service);

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
      sce->chistics = LOG_SERVICE_UNSPECIFIED;
      sce->requested = 0;
      sce->opened = 0;
    }
  }

  return sce;
}

/**
  Find out characteristics of a service (e.g. whether it is a singleton)
  by asking it.

  (See log_service_chistics for a list of possible characteristics!)

  @param  service  what service to examine

  @retval a set of log_service_chistics flags
*/
static int log_service_get_characteristics(my_h_service service) {
  SERVICE_TYPE(log_service) * ls;

  DBUG_ASSERT(service != nullptr);

  ls = reinterpret_cast<SERVICE_TYPE(log_service) *>(service);

  // no information available, default to restrictive
  if (ls->characteristics == nullptr)
    return LOG_SERVICE_UNSPECIFIED |
           LOG_SERVICE_SINGLETON; /* purecov: inspected */

  return ls->characteristics();
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

  if (!log_builtins_inited) return 0;

  mysql_rwlock_wrlock(&THR_LOCK_log_stack);

  lsi = log_service_instances;

  while ((lsi != nullptr) && ((sce = lsi->sce) != nullptr)) {
    if (!(sce->chistics & LOG_SERVICE_BUILTIN)) {
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

  @param        conf        The configuration string
  @param        check_only  If true, report on whether configuration is valid
                            (i.e. whether all requested services are available),
                            but do not apply the new configuration.
                            if false, set the configuration (acquire the
                            necessary services, update the hash by
                            adding/deleting entries as necessary)
  @param[out]   pos         If an error occurs and this pointer is non-null,
                            the position in the configuration string where
                            the error occurred will be written to the
                            pointed-to size_t.

  @retval  LOG_ERROR_STACK_SUCCESS               success

  @retval LOG_ERROR_STACK_DELIMITER_MISSING      expected delimiter not found

  @retval LOG_ERROR_STACK_SERVICE_MISSING        one or more services not found

  @retval LOG_ERROR_STACK_CACHE_ENTRY_OOM        couldn't create service cache
                                                 entry

  @retval LOG_ERROR_STACK_MULTITON_DENIED        tried to multi-open singleton

  @retval LOG_ERROR_STACK_SERVICE_INSTANCE_OOM   couldn't create service
                                                 instance entry

  @retval LOG_ERROR_STACK_ENDS_IN_NON_SINK       last element should be a sink

  @retval LOG_ERROR_STACK_SERVICE_UNAVAILABLE    service only available during
                                                 start-up (may not be set by the
                                                 user)


  @retval  LOG_ERROR_STACK_NO_PFS_SUPPORT        (check_only warning)
                                                 no sink with performance_schema
                                                 support selected

  @retval  LOG_ERROR_STACK_NO_LOG_PARSER         (check_only warning)
                                                 no sink providing a log-parser
                                                 selected

  @retval  LOG_ERROR_MULTIPLE_FILTERS            (check_only warning)
                                                 more than one filter service
                                                 selected

  @retval  LOG_ERROR_UNEXPECTED_DELIMITER_FOUND  service starts with a delimiter

  @retval  LOG_ERROR_MIXED_DELIMITERS            use ',' or ';', not both!
*/
log_error_stack_error log_builtins_error_stack(const char *conf,
                                               bool check_only, size_t *pos) {
  const char *start = conf, *end;
  char delim = '\0';
  ssize_t len;
  my_h_service service;
  log_error_stack_error rr = LOG_ERROR_STACK_SUCCESS;
  int count = 0;
  log_service_cache_entry *sce = nullptr;
  log_service_instance *lsi;
  log_service_instance *log_sink_pfs_parser = nullptr;  // sink with log parser
  log_service_instance *log_sink_pfs_buffer = nullptr;  // sink with pfs support
  int log_filter_count = 0;  // number of filters in pipeline
  int log_pfs_count = 0;     // number of pfs-supporting sinks in pipeline
  int log_parser_count = 0;  // number of log-parsers in pipeline
  int chistics = LOG_SERVICE_UNSPECIFIED;

  mysql_rwlock_wrlock(&THR_LOCK_log_stack);

  // if we're actually setting this configuration, release the previous one!
  if (!check_only) {
    log_sink_pfs_source = nullptr;
    log_service_instance_release_all();
  }

  // clear keep flag on all service cache entries
  for (auto &key_and_value : *log_service_cache) {
    sce = key_and_value.second.get();
    sce->requested = 0;

    DBUG_ASSERT(check_only || (sce->opened == 0));
  }

  sce = nullptr;

  lsi = nullptr;
  while ((len = log_builtins_stack_get_service_from_var(&start, &end, &delim)) >
         0) {
    chistics = LOG_SERVICE_UNSPECIFIED;

    // more than one service listed, but no delimiter used (only space)
    if ((++count > 1) && (delim == '\0')) {
      // at least one service not found => fail
      rr = LOG_ERROR_STACK_DELIMITER_MISSING;
      goto done;
    }

    // find current service name in service-cache
    auto it = log_service_cache->find(string(start, len));

    // not found in cache; is it a built-in default?
    if (it == log_service_cache->end()) {
      chistics = log_service_check_if_builtin(start, len);

      /*
        Buffered logging is only used during start-up.
        As we set it up internally from a constant, we don't
        check the value first, but go straight to the set phase.
        If we do see the special sink during the check phase,
        it was requested by the user. That is not supported
        (as it isn't useful), so we throw an error here.
      */
      if (check_only && (chistics & LOG_SERVICE_BUFFER)) {
        rr = LOG_ERROR_STACK_SERVICE_UNAVAILABLE;
        goto done;
      }

      // not a built-in; ask component framework
      if (!(chistics & LOG_SERVICE_BUILTIN)) {
        service = log_service_get_by_name(start, len);

        // not found in framework, signal failure
        if (service == nullptr) {
          // at least one service not found => fail
          rr = LOG_ERROR_STACK_SERVICE_MISSING;
          goto done;
        }
      } else
        service = nullptr;

      // make a cache-entry for this service
      if ((sce = log_service_cache_entry_new(start, len, service)) == nullptr) {
        // failed to make cache-entry. if we hold a service handle, release it!
        /* purecov: begin inspected */
        if (service != nullptr) srv_registry->release(service);
        rr = LOG_ERROR_STACK_CACHE_ENTRY_OOM;
        goto done; /* purecov: end */
      }

      // service is not built-in, so we know nothing about it. Ask it!
      if ((sce->chistics = chistics) == LOG_SERVICE_UNSPECIFIED)
        sce->chistics =
            log_service_get_characteristics(service) &
            ~LOG_SERVICE_BUILTIN;  // loaded service can not be built-in

      log_service_cache->emplace(string(sce->name, sce->name_len),
                                 cache_entry_with_deleter(sce));
    } else
      sce = it->second.get();

    // at this point, it's in cache, one way or another
    sce->requested++;

    if (check_only) {
      // tried to multi-open a service that doesn't support it => fail
      if ((sce->requested > 1) && (sce->chistics & LOG_SERVICE_SINGLETON)) {
        rr = LOG_ERROR_STACK_MULTITON_DENIED;
        goto done;
      }

      // count log-parsers
      if ((log_sink_pfs_parser == nullptr) &&
          log_service_has_characteristics(
              sce, (LOG_SERVICE_LOG_PARSER | LOG_SERVICE_PFS_SUPPORT)))
        log_parser_count++;

      // count pfs-supporting sinks
      if ((log_sink_pfs_buffer == nullptr) &&
          log_service_has_characteristics(sce, LOG_SERVICE_PFS_SUPPORT))
        log_pfs_count++;

      // count filters
      if (sce->chistics & LOG_SERVICE_FILTER) log_filter_count++;

    } else if ((sce->requested == 1) ||
               !(sce->chistics & LOG_SERVICE_SINGLETON)) {
      log_service_instance *lsi_new = nullptr;

      // actually setting this config, so open this instance!
      lsi_new = log_service_instance_new(sce, nullptr);

      if (lsi_new != nullptr)  // add to chain of instances
      {
        if (log_service_instances == nullptr)
          log_service_instances = lsi_new;
        else {
          DBUG_ASSERT(lsi != nullptr);
          lsi->next = lsi_new;
        }

        lsi = lsi_new;

        // remember first log-parser
        if ((log_sink_pfs_parser == nullptr) &&
            (sce->chistics &
             (LOG_SERVICE_LOG_PARSER | LOG_SERVICE_PFS_SUPPORT)))
          log_sink_pfs_parser = lsi;
        // remember first pfs-supporting sink
        if ((log_sink_pfs_buffer == nullptr) &&
            (sce->chistics & LOG_SERVICE_PFS_SUPPORT))
          log_sink_pfs_buffer = lsi;
        // count filters
        if (sce->chistics & LOG_SERVICE_FILTER) log_filter_count++;
      } else  // could not make new instance entry; fail
      {
        rr = LOG_ERROR_STACK_SERVICE_INSTANCE_OOM; /* purecov: inspected */
        goto done;                                 /* purecov: inspected */
      }
    }

    /*
      If neither branch was true, we're in set mode, but the set-up
      is invalid (i.e. we're trying to multi-open a singleton). As
      this should have been caught in the check phase, we don't
      specfically handle it here; the invalid element is skipped and
      not added to the instance list; that way, we'll get as close
      to a working configuration as possible in our attempt to fail
      somewhat gracefully.
    */

    start = end;
  }

  if (len < 0)  // log_builtins_stack_get_service_from_var() failed
    rr = (log_error_stack_error)len;  // flag delimiter issue in string
  else if ((sce != nullptr) && !(sce->chistics & LOG_SERVICE_SINK))
    rr = LOG_ERROR_STACK_ENDS_IN_NON_SINK;  // last past service was not a sink.
  else                                      // success
    rr = LOG_ERROR_STACK_SUCCESS;

done:
  // remove stale entries from cache
  for (auto it = log_service_cache->begin(); it != log_service_cache->end();) {
    sce = it->second.get();

    if (sce->opened <= 0)
      it = log_service_cache->erase(it);
    else
      ++it;
  }

  if (!check_only) {
    log_sink_pfs_source = (log_sink_pfs_parser != nullptr)
                              ? log_sink_pfs_parser
                              : log_sink_pfs_buffer;
  }
  /*
    We only process warnings if
    a) We're in check_only mode;
    b) there aren't errors already (which outrank warnings)
    c) pos != non-NULL
  */
  else if ((rr == LOG_ERROR_STACK_SUCCESS) && (pos != nullptr)) {
    if (log_pfs_count == 0)
      rr = LOG_ERROR_STACK_NO_PFS_SUPPORT;
    else if (log_parser_count == 0)
      rr = LOG_ERROR_STACK_NO_LOG_PARSER;
    else if (log_filter_count > 1)
      rr = LOG_ERROR_MULTIPLE_FILTERS;
  }

  mysql_rwlock_unlock(&THR_LOCK_log_stack);

  if (pos != nullptr) *pos = (size_t)(start - conf);

  return rr;
}

/**
  Acquire an exclusive lock on the error logger core.
*/
void log_builtins_error_stack_wrlock() {
  mysql_rwlock_wrlock(&THR_LOCK_log_stack);
}

/**
  Release a lock on the error logger core.
*/
void log_builtins_error_stack_unlock() {
  mysql_rwlock_unlock(&THR_LOCK_log_stack);
}

/**
  De-initialize the structured logging subsystem.

  @retval  0  no errors
  @retval -1  not stopping, never started
*/
int log_builtins_exit() {
  if (!log_builtins_inited) return -1;

  mysql_rwlock_wrlock(&THR_LOCK_log_stack);
  mysql_mutex_lock(&THR_LOCK_log_buffered);
  mysql_mutex_lock(&THR_LOCK_log_syseventlog);

  log_builtins_filter_exit();
  log_service_instance_release_all();
  delete log_service_cache;

  log_builtins_inited = false;
  log_error_stage_set(LOG_ERROR_STAGE_BUFFERING_EARLY);

  mysql_rwlock_unlock(&THR_LOCK_log_stack);
  mysql_rwlock_destroy(&THR_LOCK_log_stack);

  mysql_mutex_unlock(&THR_LOCK_log_syseventlog);
  mysql_mutex_destroy(&THR_LOCK_log_syseventlog);

  mysql_mutex_unlock(&THR_LOCK_log_buffered);
  mysql_mutex_destroy(&THR_LOCK_log_buffered);

  return 0;
}

/**
  Initialize the structured logging subsystem.

  Since we're initializing various locks here, we must call this late enough
  so this is clean, but early enough so it still happens while we're running
  single-threaded -- this specifically also means we must call it before we
  start plug-ins / storage engines / external components!

  @retval  0  no errors
  @retval -1  couldn't initialize stack lock
  @retval -2  couldn't initialize built-in default filter
  @retval -3  couldn't set up service hash
  @retval -4  couldn't initialize syseventlog lock
  @retval -5  couldn't set service pipeline
  @retval -6  couldn't initialize buffered logging lock
*/
int log_builtins_init() {
  int rr = 0;

  DBUG_ASSERT(!log_builtins_inited);

  // Reset flag. This is *also* set on definition, this is intentional.
  log_buffering_flushworthy = false;

  if (mysql_rwlock_init(0, &THR_LOCK_log_stack)) return -1;

  if (mysql_mutex_init(0, &THR_LOCK_log_syseventlog, MY_MUTEX_INIT_FAST)) {
    mysql_rwlock_destroy(&THR_LOCK_log_stack);
    return -4;
  }

  if (mysql_mutex_init(0, &THR_LOCK_log_buffered, MY_MUTEX_INIT_FAST)) {
    rr = -6;
    goto fail;
  }

  mysql_rwlock_wrlock(&THR_LOCK_log_stack);

  if (log_builtins_filter_init())
    rr = -2;
  else {
    log_service_cache =
        new collation_unordered_map<string, cache_entry_with_deleter>(
            system_charset_info, 0);
    if (log_service_cache == nullptr) rr = -3;
  }

  log_service_instances = nullptr;

  mysql_rwlock_unlock(&THR_LOCK_log_stack);

  if (rr >= 0) {
    if (log_builtins_error_stack(LOG_BUILTINS_BUFFER, false, nullptr) >= 0) {
      log_error_stage_set(LOG_ERROR_STAGE_BUFFERING_EARLY);
      log_builtins_inited = true;
      return 0;
    } else {
      rr = -5;            /* purecov: inspected */
      DBUG_ASSERT(false); /* purecov: inspected */
    }
  }

fail:
  mysql_rwlock_destroy(&THR_LOCK_log_stack); /* purecov: begin inspected */
  mysql_mutex_destroy(&THR_LOCK_log_syseventlog);

  return rr; /* purecov: end */
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
  Fails gracefully if no log_item_data is supplied, so it can safely
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
  Fails gracefully if no log_item_data is supplied, so it can safely
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
  Fails gracefully if no log_item_data is supplied, so it can safely
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
  Fails gracefully if no log_item_data is supplied, so it can safely
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
  Get log-line's output buffer.
  If the logger core provides this buffer, the log-service may use it
  to assemble its output therein and implicitly return it to the core.
  Participation is required for services that support populating
  performance_schema.error_log, and optional for all others.

  @param  ll  the log_line to examine

  @retval  nullptr    success, an output buffer is available
  @retval  otherwise  failure, no output buffer is available
*/
DEFINE_METHOD(log_item *, log_builtins_imp::line_get_output_buffer,
              (log_line * ll)) {
  return log_line_get_output_buffer(ll);
}

/**
  Get an iterator for the items in a log_line.
  For now, only one iterator may exist per log_line.

  @param  ll  the log_line to examine

  @retval     a log_item_iter, or nullptr on failure
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
// Call to function through pointer to incorrect function type
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
    my_free(const_cast<char *>(in_start));
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
  Parse a ISO8601 timestamp and return the number of microseconds
  since the epoch. Heeds +/- timezone info if present.

  @see make_iso8601_timestamp()

  @param timestamp  an ASCII string containing an ISO8601 timestamp
  @param len        Length in bytes of the aforementioned string

  @return microseconds since the epoch
*/
DEFINE_METHOD(ulonglong, log_builtins_imp::parse_iso8601_timestamp,
              (const char *timestamp, size_t len)) {
  return iso8601_timestamp_to_microseconds(timestamp, len);
}

/**
  Create a log-file name (path + name + extension).
  The path will be taken from @@log_error.
  If name + extension are given, they are used.
  If only an extension is given (argument starts with '.'),
  the name is taken from @@log_error, and the extension is used.
  If only a name is given (but no extension), the name and a
  default extension are used.

  @param  result        Buffer to return to created path+name+extension in.
                        Size must be FN_REFLEN.
  @param  name_or_ext   if beginning with '.':
                          @@global.log_error, except with this extension
                        otherwise:
                          use this as file name in the same location as
                          @@global.log_error

                        Value may not contain folder separators!

  @retval LOG_SERVICE_SUCCESS                   buffer contains a valid result
  @retval LOG_SERVICE_BUFFER_SIZE_INSUFFICIENT  an error occurred
*/
log_service_error make_log_path(char *result, const char *name_or_ext) {
  char path[FN_REFLEN];  // Just the path (without file-name / extension)
  size_t path_length;

  // Get just the directories from @@log_error.
  if (dirname_part(path, log_error_dest, &path_length) >= sizeof(path)) {
    return LOG_SERVICE_BUFFER_SIZE_INSUFFICIENT; /* purecov: inspected */
  }

  // If the provided argument starts with a '.', it's only the extension
  if (name_or_ext[0] == '.') {
    // Copy the file-name and (original) ext.
    char name_buff[FN_REFLEN];
    strcpy(name_buff, &log_error_dest[path_length]);

    /*
      The logs should arguably be e.g. log.abc.err and log.abc.json.

      MY_APPEND_EXT gives us log.abc.err and log.abc.err.json however.

      MY_REPLACE_EXT uses strchr() (instead of strrchr() as it arguably
      should), so it would give us log.json, deleting the abc part.

      To fix this, we should do eventually do the following here:

      char *period = strrchr(dest_buff, '.');
      if (period != nullptr) *period = '\0';
    */

    // use path + file-name from log-error, and use the provided extension
    if (fn_format(result, name_buff, path, name_or_ext,
                  MY_APPEND_EXT | MY_REPLACE_DIR | MY_SAFE_PATH) == nullptr)
      return LOG_SERVICE_BUFFER_SIZE_INSUFFICIENT; /* purecov: inspected */
  }
  // The provided argument is a file-name (possibly with extension).
  else {
    /*
      Use the path part of @@log_error, and append the provided file-name.
      If the argument contained an extension, use that; otherwise, we'll
      use a default ("log error stream").
    */
    if (fn_format(result, name_or_ext, path, ".les",
                  MY_REPLACE_DIR | MY_SAFE_PATH) == nullptr)
      return LOG_SERVICE_BUFFER_SIZE_INSUFFICIENT; /* purecov: inspected */
  }

  return LOG_SERVICE_SUCCESS;
}

/**
  open an error log file

  @param       name_or_ext   if beginning with '.':
                               @@global.log_error, except with this extension
                             otherwise:
                               use this as file name in the same location as
                               @@global.log_error

                             Value may not contain folder separators!

  @param[out]  my_errstream  an error log handle, or nullptr on failure

  @retval LOG_SERVICE_SUCCESS                  success
  @retval LOG_SERVICE_INVALID_ARGUMENT         no my_errstream, or bad log name
  @retval LOG_SERVICE_OUT_OF_MEMORY            could not allocate file handle
  @retval LOG_SERVICE_LOCK_ERROR               couldn't lock lock
  @retval LOG_SERVICE_UNABLE_TO_WRITE          couldn't write to given location
  @retval LOG_SERVICE_COULD_NOT_MAKE_LOG_NAME  could not make log name
*/
DEFINE_METHOD(log_service_error, log_builtins_imp::open_errstream,
              (const char *name_or_ext, void **my_errstream)) {
  log_errstream *les;
  log_service_error rr;

  if (my_errstream == nullptr)
    return LOG_SERVICE_INVALID_ARGUMENT; /* purecov: inspected */

  *my_errstream = nullptr;

  les = (log_errstream *)my_malloc(key_memory_log_error_loaded_services,
                                   sizeof(log_errstream), MYF(0));

  if (les == nullptr) return LOG_SERVICE_OUT_OF_MEMORY; /* purecov: inspected */

  new (les) log_errstream();

  if (mysql_mutex_init(0, &les->LOCK_errstream, MY_MUTEX_INIT_FAST)) {
    my_free(les);                  /* purecov: inspected */
    return LOG_SERVICE_LOCK_ERROR; /* purecov: inspected */
  }

  // We require an argument, but don't allow dir separators.
  if ((name_or_ext == nullptr) || (name_or_ext[0] == '\0') ||
      (strchr(name_or_ext, FN_LIBCHAR) != nullptr)) {
    rr = LOG_SERVICE_INVALID_ARGUMENT; /* purecov: inspected */
    goto fail_with_free;               /* purecov: inspected */
  }
  // --log-error=... was not set, we're logging to stderr
  else if ((log_error_dest == nullptr) || (!strcmp(log_error_dest, "stderr"))) {
    // When using default stream, no file struct is needed.
    les->file = nullptr;
  }
  // Logging to file. Create an accept path+name+extension, and open the file.
  else {
    MY_STAT f_stat;
    char errorlog_instance_full[FN_REFLEN];  // result: path + name + extension

    if (make_log_path(errorlog_instance_full, name_or_ext)) {
      rr = LOG_SERVICE_COULD_NOT_MAKE_LOG_NAME; /* purecov: inspected */
      goto fail_with_free;                      /* purecov: inspected */
    }

    rr = LOG_SERVICE_UNABLE_TO_WRITE;

    // If the log-file exists, make sure it's writeable.
    if (my_stat(errorlog_instance_full, &f_stat, MYF(0)) != nullptr) {
      if (!(f_stat.st_mode & MY_S_IWRITE)) {
        goto fail_with_free; /* purecov: inspected */
      }
    }
    /*
      If the log-file doesn't exist yet, check whether we can write to
      the directory.
    */
    else {
      char path[FN_REFLEN];
      size_t path_length;

      if ((dirname_part(path, log_error_dest, &path_length) >= sizeof(path)) ||
          my_access(path, (F_OK | W_OK))) {
        goto fail_with_free; /* purecov: inspected */
      }
    }

    // Now finally, we open the log.
    les->file = my_fopen(errorlog_instance_full,
                         O_APPEND | O_WRONLY | MY_FOPEN_BINARY, MYF(0));

    if (les->file == nullptr) goto fail_with_free; /* purecov: inspected */
  }

  *my_errstream = les;

  return LOG_SERVICE_SUCCESS;

fail_with_free:
  my_free(les); /* purecov: begin inspected */

  return rr; /* purecov: end */
}

/**
  write to an error log file previously opened with open_errstream()

  @param       my_errstream  a handle describing the log file
  @param       buffer        pointer to the string to write
  @param       length        length of the string to write

  @retval  LOG_SERVICE_SUCCESS                 success
  @retval  otherwise                           failure
*/
DEFINE_METHOD(log_service_error, log_builtins_imp::write_errstream,
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

  return LOG_SERVICE_SUCCESS;
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

  @retval     LOG_SERVICE_SUCCESS          success
  @retval     otherwise                    failure
*/
DEFINE_METHOD(log_service_error, log_builtins_imp::close_errstream,
              (void **my_errstream)) {
  int rr;

  if (my_errstream == nullptr) return LOG_SERVICE_INVALID_ARGUMENT;

  log_errstream *les = (log_errstream *)(*my_errstream);

  if (les == nullptr) return LOG_SERVICE_INVALID_ARGUMENT;

  *my_errstream = nullptr;

  if (les->file != nullptr) {
    my_fclose(les->file, MYF(0));
    // Continue to log after closing, you'll log to stderr. That'll learn ya.
    les->file = nullptr;
  }

  rr = mysql_mutex_destroy(&les->LOCK_errstream);

  my_free(les);

  return rr ? LOG_SERVICE_LOCK_ERROR : LOG_SERVICE_SUCCESS;
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
  return strchr(const_cast<char *>(s), c);
}

/**
  Wrapper for strrchr() - find character in string, from the right
*/
DEFINE_METHOD(char *, log_builtins_string_imp::find_last,
              (const char *s, int c)) {
  return strrchr(const_cast<char *>(s), c);
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

/**
  Wrapper for mysys' my_openlog.
  Opens/Registers a new handle for system logging.
  Note: It's a thread-unsafe function. It should either
  be invoked from the main thread or some extra thread
  safety measures need to be taken.

  @param name     Name of the event source / syslog ident.
  @param option   MY_SYSLOG_PIDS to log PID with each message.
  @param facility Type of program. Passed to openlog().

  @retval  LOG_SERVICE_SUCCESS        Success
  @retval  LOG_SERVICE_NOT_AVAILABLE  Error, log not opened
  @retval  LOG_ERROR_NOTHING_DONE     Error, not updated, using previous values
*/
DEFINE_METHOD(log_service_error, log_builtins_syseventlog_imp::open,
              (const char *name, int option, int facility)) {
  int ret;

  mysql_mutex_lock(&THR_LOCK_log_syseventlog);
  ret = my_openlog(name, option, facility);
  mysql_mutex_unlock(&THR_LOCK_log_syseventlog);

  switch (ret) {
    case 0:
      return LOG_SERVICE_SUCCESS;
    case -1: /* purecov: begin inspected */
      return LOG_SERVICE_NOT_AVAILABLE;
    case -2:
      return LOG_SERVICE_NOTHING_DONE;
    default:
      DBUG_ASSERT(false);
  }

  return LOG_SERVICE_MISC_ERROR; /* purecov: end */
}

/**
  Wrapper for mysys' my_syslog.
  Sends message to the system logger. On Windows, the specified message is
  internally converted to UCS-2 encoding, while on other platforms, no
  conversion takes place and the string is passed to the syslog API as it is.

  @param level                Log level
  @param msg                  Message to be logged

  @retval LOG_SERVICE_SUCCESS  Success
  @retval otherwise            Error, nothing logged
*/
DEFINE_METHOD(log_service_error, log_builtins_syseventlog_imp::write,
              (enum loglevel level, const char *msg)) {
  int ret;

  mysql_mutex_lock(&THR_LOCK_log_syseventlog);
  ret = my_syslog(&my_charset_utf8_bin, level, msg);
  mysql_mutex_unlock(&THR_LOCK_log_syseventlog);

  return (ret == 0) ? LOG_SERVICE_SUCCESS : LOG_SERVICE_NOT_AVAILABLE;
}

/**
  Wrapper for mysys' my_closelog.
  Closes/de-registers the system logging handle.

  @retval LOG_SERVICE_SUCCESS  Success
  @retval otherwise            Error
*/
DEFINE_METHOD(log_service_error, log_builtins_syseventlog_imp::close, (void)) {
  int ret = 0;

  mysql_mutex_lock(&THR_LOCK_log_syseventlog);
  ret = my_closelog();
  mysql_mutex_unlock(&THR_LOCK_log_syseventlog);

  return (ret == 0) ? LOG_SERVICE_SUCCESS : LOG_SERVICE_MISC_ERROR;
}

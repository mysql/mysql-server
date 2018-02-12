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

#include <mysql/components/services/log_builtins.h>

#include "log_service_imp.h"
#include "m_string.h"  // native_strncasecmp()/native_strcasecmp()
#include "my_compiler.h"
#include "my_io.h"
#include "my_sys.h"
#include "mysqld_error.h"  // so we can throw ER_LOG_SYSLOG_*
#ifndef _WIN32
#include <syslog.h>  // LOG_DAEMON etc. -- facility names

/*
  Some C libraries offer a variant of this, but we roll our own so we
  won't have to worry about portability.
*/
SYSLOG_FACILITY syslog_facility[] = {
    {LOG_DAEMON, "daemon"}, /* default for mysqld */
    {LOG_USER, "user"},     /* default for mysql command-line client */

    {LOG_LOCAL0, "local0"},
    {LOG_LOCAL1, "local1"},
    {LOG_LOCAL2, "local2"},
    {LOG_LOCAL3, "local3"},
    {LOG_LOCAL4, "local4"},
    {LOG_LOCAL5, "local5"},
    {LOG_LOCAL6, "local6"},
    {LOG_LOCAL7, "local7"},

    /* "just in case" */
    {LOG_AUTH, "auth"},
    {LOG_CRON, "cron"},
    {LOG_KERN, "kern"},
    {LOG_LPR, "lpr"},
    {LOG_MAIL, "mail"},
    {LOG_NEWS, "news"},
    {LOG_SYSLOG, "syslog"},
    {LOG_UUCP, "uucp"},

#if defined(LOG_FTP)
    {LOG_FTP, "ftp"},
#endif
#if defined(LOG_AUTHPRIV)
    {LOG_AUTHPRIV, "authpriv"},
#endif

    {-1, NULL}};
#endif

static const char *opt_fac = "log_syslog_facility";
static const char *opt_enable = "log_syslog";
static const char *opt_tag = "log_syslog_tag";
static const char *opt_pid = "log_syslog_include_pid";

static bool inited = false;

REQUIRES_SERVICE_PLACEHOLDER(log_builtins);
REQUIRES_SERVICE_PLACEHOLDER(log_builtins_string);
REQUIRES_SERVICE_PLACEHOLDER(log_builtins_syseventlog);
#ifdef _WIN32
REQUIRES_SERVICE_PLACEHOLDER(log_builtins_tmp);
#endif

SERVICE_TYPE(log_builtins) *log_bi = nullptr;
SERVICE_TYPE(log_builtins_string) *log_bs = nullptr;
SERVICE_TYPE(log_builtins_syseventlog)
*log_se = nullptr;
#ifdef _WIN32
SERVICE_TYPE(log_builtins_tmp) *log_bt = nullptr;
#else
static int log_syslog_facility = LOG_DAEMON;
#endif
static char *log_syslog_ident = nullptr;
static bool log_syslog_enabled = false;
static bool log_syslog_include_pid = true;

#ifndef _WIN32
/*
  Logs historically have subtly different names, to meet each platform's
  conventions -- "mysqld" on unix (via mysqld_safe), and "MySQL" for the
  Win NT EventLog.
*/
static const char *prefix = "mysqld";
#else
static const char *prefix = "MySQL";
#endif

#ifndef _WIN32

/**
  On being handed a syslog facility name tries to look it up.
  If successful, fills in a struct with the facility ID and
  the facility's canonical name.

  @param f           Name of the facility we're trying to look up.
                     Lookup is case-insensitive; leading "log_" is ignored.
  @param [out] rsf   A buffer in which to return the ID and canonical name.

  @retval false      No errors; buffer contains valid result
  @retval true       Something went wrong, no valid result set returned
*/
static bool log_syslog_find_facility(const char *f, SYSLOG_FACILITY *rsf) {
  if (!f || !*f || !rsf) return true;

  if (native_strncasecmp(f, "log_", 4) == 0) f += 4;

  for (int i = 0; syslog_facility[i].name != nullptr; i++)
    if (!native_strcasecmp(f, syslog_facility[i].name)) {
      rsf->id = syslog_facility[i].id;
      rsf->name = syslog_facility[i].name;
      return false;
    }

  return true;
}

#endif

/**
  Close POSIX syslog / Windows EventLog.

  @retval  0    On Success.
  @retval  !=0  On failure.
*/
static int log_syslog_close() {
  if (log_syslog_enabled) {
    log_syslog_enabled = false;
    log_se->close();
    return 1;
  }

  return 0;
}

/**
  Open POSIX syslog / Windows EventLog.

  @retval  -3  log was already open, close it before opening again! (log open)
  @retval  -2  cannot set up new registry entry, continuing with previous value
               (log open, but continues to log under previous key)
  @retval  -1  cannot set up new registry entry, no previous value
               (log not opened)
  @retval   0  success (log open)
*/
int log_syslog_open() {
  int ret;
  const char *ident = (log_syslog_ident != nullptr) ? log_syslog_ident : prefix;

  if (log_syslog_enabled) return -3;

  ret = log_se->open(ident,
#ifndef _WIN32
                     log_syslog_include_pid ? MY_SYSLOG_PIDS : 0,
                     log_syslog_facility
#else
                     0, 0
#endif
  );

  if (ret != -1) log_syslog_enabled = true;

  if (ret == -2) {
    log_bi->message(LOG_TYPE_ERROR, LOG_ITEM_LOG_PRIO, ERROR_LEVEL,
                    LOG_ITEM_LOG_MESSAGE,
                    "log_sink_syseventlog was unable to create a new "
                    "Windows registry key %s for logging; "
                    "continuing to log to previous ident",
                    ident);
  }

  return ret;
}

/**
  Syslog settings have changed; close and re-open.
*/
static void log_syslog_reopen() {
  if (log_syslog_enabled) {
    log_syslog_close();
    log_syslog_open();
  }
}

/**
  Stop using syslog / EventLog. Call as late as possible.
*/
void log_syslog_exit(void) {
  log_syslog_close();

  // free ident.
  if (log_syslog_ident != nullptr) {
    log_bs->free(log_syslog_ident);
    log_syslog_ident = nullptr;
  }
}

/**
  variable listener. This is a temporary solution until we have
  per-component system variables. "check" is where our component
  can veto.

  @param   ll  a log_line with a list-item describing the variable
               (name, new value)

  @retval   0  for allow (including when we don't feel the event is for us),
  @retval  <0  deny (nullptr, malformed structures, etc. -- caller broken?)
  @retval  >0  deny (user input rejected)
*/
DEFINE_METHOD(int, log_service_imp::variable_check, (log_line * ll)) {
  log_item_iter *it;
  log_item *li;
  int rr = -1;

  if ((it = log_bi->line_item_iter_acquire(ll)) == nullptr) return rr;

  if ((li = log_bi->line_item_iter_first(it)) == nullptr) goto done;

  rr = 1;

  if ((native_strncasecmp(li->key, opt_tag, log_bs->length(opt_tag)) == 0)) {
    const char *option;

    if ((li->item_class != LOG_LEX_STRING)) goto done;

    if ((option = li->data.data_string.str) != nullptr) {
      DBUG_ASSERT(option[li->data.data_string.length] == '\0');

      if (strchr(option, FN_LIBCHAR) != nullptr) goto done;
    } else
      goto done;
  }
#ifndef _WIN32
  else if ((native_strncasecmp(li->key, opt_fac, log_bs->length(opt_fac)) ==
            0)) {
    SYSLOG_FACILITY rsf;

    if ((li->item_class != LOG_LEX_STRING) ||
        log_syslog_find_facility(li->data.data_string.str, &rsf))
      goto done;
  }
#endif

  rr = 0;

done:
  log_bi->line_item_iter_release(it);

  return rr;
}

/**
  variable listener. This is a temporary solution until we have
  per-component system variables. "update" is where we're told
  to update our state (if the variable concerns us to begin with).

  @param  ll  a log_line with a list-item describing the variable
              (name, new value)

  @retval  0  the event is not for us
  @retval <0  for failure
  @retval >0  for success (at least one item was updated)
*/
DEFINE_METHOD(int, log_service_imp::variable_update, (log_line * ll)) {
  log_item_iter *it;
  log_item *li;
  int rr = -1;

  if ((it = log_bi->line_item_iter_acquire(ll)) == nullptr) return rr;

  if ((li = log_bi->line_item_iter_first(it)) == nullptr) goto done;

  /*
    on/off
  */

  if ((native_strcasecmp(li->key, opt_enable) == 0)) {
    if (li->item_class != LOG_INTEGER) goto done;

    if (li->data.data_integer == 0) {
      log_syslog_close();
      rr = 1;
      goto done;
    } else if (li->data.data_integer == 1) {
      int ret = log_syslog_open();

      rr = ((ret == 0) || (ret == -3)) ? 1 : ret;
      goto done;
    }
  }
    /*
      facility
    */
#ifndef _WIN32
  else if ((native_strcasecmp(li->key, opt_fac) == 0)) {
    SYSLOG_FACILITY rsf = {LOG_DAEMON, "daemon"};
    char *option;

    if (li->item_class != LOG_LEX_STRING) goto done;

    /*
      make facility
    */

    option = (char *)li->data.data_string.str;

    DBUG_ASSERT(option != nullptr);
    DBUG_ASSERT(option[li->data.data_string.length] == '\0');

    /*
      if we can't find the submitted facility, we default to "daemon"
    */
    if (log_syslog_find_facility(option, &rsf)) {
      log_syslog_find_facility((char *)"daemon", &rsf);
      log_bi->message(LOG_TYPE_ERROR, LOG_ITEM_LOG_PRIO, WARNING_LEVEL,
                      LOG_ITEM_LOG_LOOKUP, ER_LOG_SYSLOG_FACILITY_FAIL, option,
                      rsf.name, rsf.id);
      rsf.name = nullptr;
    }
    // If NaN, set to the canonical form (cut "log_", fix case)
    if ((rsf.name != nullptr) && (strcmp(option, rsf.name) != 0))
      strcpy(option, rsf.name);

    if (log_syslog_facility != rsf.id) {
      log_syslog_facility = rsf.id;
      log_syslog_reopen();
    }
    rr = 1;
    goto done;
  }
#endif

  /*
    ident/tag
  */
  else if ((native_strcasecmp(li->key, opt_tag) == 0)) {
    char *option;
    char *new_ident = nullptr, *old_ident = nullptr;
    bool ident_changed = false;

    if (li->item_class != LOG_LEX_STRING) goto done;

    option = (char *)li->data.data_string.str;

    // tag must not contain directory separators
    if ((option != nullptr) && (strchr(option, FN_LIBCHAR) != nullptr))
      goto done;

    /*
      make ident
    */

    if ((option == nullptr) || (*option == '\0'))
      new_ident = log_bs->strndup(prefix, log_bs->length(prefix));
    else {
      // prefix + '-' + '\0' + option
      size_t l = log_bs->length(prefix) + 1 + 1 + log_bs->length(option);

      new_ident = (char *)log_bs->malloc(l);
      if (new_ident != nullptr)
        log_bs->substitute(new_ident, l, "%s-%s", prefix, option);
    }

    // if we succeeded in making an ident, replace the old one
    if (new_ident != nullptr) {
      ident_changed = (log_syslog_ident == nullptr) ||
                      (strcmp(new_ident, log_syslog_ident) != 0);
    } else
      goto done;

    if (ident_changed) {
      old_ident = log_syslog_ident;
      log_syslog_ident = new_ident;

      log_syslog_reopen();

      if (old_ident != nullptr) log_bs->free(old_ident);
    } else
      log_bs->free(new_ident);

    rr = 1;
    goto done;
  }

  /*
    include PID?
  */
  else if ((native_strcasecmp(li->key, opt_pid) == 0)) {
    bool inc_pid;

    if (li->item_class != LOG_INTEGER) goto done;

    inc_pid = (li->data.data_integer != 0);

    if (inc_pid != log_syslog_include_pid) {
      log_syslog_include_pid = inc_pid;
      log_syslog_reopen();
    }
    rr = 1;
    goto done;
  }

  rr = 0;

done:
  log_bi->line_item_iter_release(it);

  return rr;
}

/**
  services: log sinks: classic syslog/EventLog writer (message only)
  label will be ignored (one will be generated from priority by the syslogger).
  If the message is not \0 terminated, it will be terminated.

  @param           instance             instance's state
  @param           ll                   the log line to write

  @retval          >=0                  number of accepted fields, if any
  @retval          -1                   log was not open
  @retval          -2                   could not sanitize log message
  @retval          -3                   failure not otherwise specified
*/
DEFINE_METHOD(int, log_service_imp::run,
              (void *instance MY_ATTRIBUTE((unused)), log_line *ll)) {
  const char *msg = nullptr;
  int out_fields = 0;
  enum loglevel level = ERROR_LEVEL;
  log_item_type item_type = LOG_ITEM_END;
  log_item_type_mask out_types = 0;

  log_item_iter *it;
  log_item *li;

  if (!log_syslog_enabled) return -1;

  if ((it = log_bi->line_item_iter_acquire(ll)) == nullptr) return -3;

  li = log_bi->line_item_iter_first(it);

  while (li != nullptr) {
    item_type = li->type;

    if (log_bi->item_inconsistent(li)) goto skip_item;

    if (item_type == LOG_ITEM_LOG_PRIO)
      level = static_cast<enum loglevel>(li->data.data_integer);
    else if (item_type == LOG_ITEM_LOG_MESSAGE) {
      if (log_bi->sanitize(li) < 0) {
        log_bi->line_item_iter_release(it);
        return -2;
      }

      msg = li->data.data_string.str;
    } else if (item_type != LOG_ITEM_LOG_LABEL)
      // This backend won't let us use custom labels, so we skip over them.
      goto skip_item;

    out_types |= item_type;
    out_fields++;

  skip_item:
    li = log_bi->line_item_iter_next(it);
  }

  if ((out_types & (LOG_ITEM_LOG_PRIO | LOG_ITEM_LOG_MESSAGE)) ==
      (LOG_ITEM_LOG_PRIO | LOG_ITEM_LOG_MESSAGE)) {
#ifdef _WIN32
    /*
      Don't write to the eventlog during shutdown.
    */
    if (!log_bt->connection_loop_aborted())
#endif
    {
      log_se->write(level, msg);
    }
  }

  log_bi->line_item_iter_release(it);

  return out_fields;
}

/**
  De-initialization method for Component used when unloading the Component.

  @return Status of performed operation
  @retval false  success
  @retval true   failure
*/
mysql_service_status_t log_service_exit() {
  if (inited) {
    log_syslog_exit();

    log_bi = nullptr;
    log_bs = nullptr;
    log_se = nullptr;
#ifdef _WIN32
    log_bt = nullptr;
#endif

    inited = false;

    return false;
  }
  return true;
}

/**
  Initialization entry method for Component used when loading the Component.

  @return Status of performed operation
  @retval false success
  @retval true failure
*/
mysql_service_status_t log_service_init() {
  if (inited) return true;

  inited = true;

  log_bi = mysql_service_log_builtins;
  log_bs = mysql_service_log_builtins_string;
  log_se = mysql_service_log_builtins_syseventlog;
#ifdef _WIN32
  log_bt = mysql_service_log_builtins_tmp;
#endif

  /*
    If this component is loaded, we enable it by default, as that's
    probably what the user expects.
  */
  log_syslog_open();

  if (!log_syslog_enabled) {
#ifdef _WIN32
    const char *l = "Windows EventLog";
#else
    const char *l = "syslog";
#endif
    log_bi->message(LOG_TYPE_ERROR, LOG_ITEM_LOG_PRIO, ERROR_LEVEL,
                    LOG_ITEM_LOG_LOOKUP, ER_LOG_SYSLOG_CANNOT_OPEN, l);
    return true;
  }

  return false;
}

/* flush logs */
DEFINE_METHOD(int, log_service_imp::flush,
              (void **instance MY_ATTRIBUTE((unused)))) {
  if (inited) log_service_exit();
  return log_service_init();
}

/**
  Open a new instance.

  @param   ll        optional arguments
  @param   instance  If state is needed, the service may allocate and
                     initialize it and return a pointer to it here.
                     (This of course is particularly pertinent to
                     components that may be opened multiple times,
                     such as the JSON log writer.)
                     This state is for use of the log-service component
                     in question only and can take any layout suitable
                     to that component's need. The state is opaque to
                     the server/logging framework. It must be released
                     on close.

  @retval  <0        a new instance could not be created
  @retval  =0        success, returned hande is valid
*/
DEFINE_METHOD(int, log_service_imp::open,
              (log_line * ll MY_ATTRIBUTE((unused)), void **instance)) {
  if (instance == nullptr) return -1;

  *instance = nullptr;

  return 0;
}

/**
  Close and release an instance. Flushes any buffers.

  @param   instance  State-pointer that was returned on open.
                     If memory was allocated for this state,
                     it should be released, and the pointer
                     set to nullptr.

  @retval  <0        an error occurred
  @retval  =0        success
*/
DEFINE_METHOD(int, log_service_imp::close,
              (void **instance MY_ATTRIBUTE((unused)))) {
  return 0;
}

/* implementing a service: log_service */
BEGIN_SERVICE_IMPLEMENTATION(log_sink_syseventlog, log_service)
log_service_imp::run, log_service_imp::flush, log_service_imp::open,
    log_service_imp::close, log_service_imp::variable_check,
    log_service_imp::variable_update END_SERVICE_IMPLEMENTATION();

/* component provides: just the log_service service, for now */
BEGIN_COMPONENT_PROVIDES(log_sink_syseventlog)
PROVIDES_SERVICE(log_sink_syseventlog, log_service), END_COMPONENT_PROVIDES();

/* component requires: log-builtins */
BEGIN_COMPONENT_REQUIRES(log_sink_syseventlog)
REQUIRES_SERVICE(log_builtins), REQUIRES_SERVICE(log_builtins_string),
    REQUIRES_SERVICE(log_builtins_syseventlog),
#ifdef _WIN32
    REQUIRES_SERVICE(log_builtins_tmp),
#endif
    END_COMPONENT_REQUIRES();

/* component description */
BEGIN_COMPONENT_METADATA(log_sink_syseventlog)
METADATA("mysql.author", "Oracle Corporation"),
    METADATA("mysql.license", "GPL"), METADATA("log_service_type", "sink"),
    END_COMPONENT_METADATA();

/* component declaration */
DECLARE_COMPONENT(log_sink_syseventlog, "mysql:log_sink_syseventlog")
log_service_init, log_service_exit END_DECLARE_COMPONENT();

/* components contained in this library.
   for now assume that each library will have exactly one component. */
DECLARE_LIBRARY_COMPONENTS &COMPONENT_REF(log_sink_syseventlog)
    END_DECLARE_LIBRARY_COMPONENTS

    /* EOT */

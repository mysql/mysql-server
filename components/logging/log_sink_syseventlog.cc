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

#include <mysql/components/component_implementation.h>
#include <mysql/components/service_implementation.h>

#include <mysql/components/services/component_sys_var_service.h>
#include <mysql/plugin.h>

#include "../sql/set_var.h"

#ifndef _WIN32
#include <syslog.h>  // LOG_DAEMON etc. -- facility names

/*
  Some C libraries offer a variant of this, but we roll our own so we
  won't have to worry about portability.
*/
#define LOG_DAEMON_NAME "daemon"

/* facilities on unixoid syslog. */
struct SYSLOG_FACILITY {
  int id;
  const char *name;
};

static SYSLOG_FACILITY syslog_facility[] = {
    {LOG_DAEMON, LOG_DAEMON_NAME}, /* default for mysqld */
    {LOG_USER, "user"},            /* default for mysql command-line client */

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

// variable names
#define OPT_FAC "facility"
#define OPT_PID "include_pid"
#endif
#define OPT_TAG "tag"

static bool inited = false; /**< component initialized */

// components we'll be using
REQUIRES_SERVICE_PLACEHOLDER(component_sys_variable_register);
REQUIRES_SERVICE_PLACEHOLDER(component_sys_variable_unregister);

REQUIRES_SERVICE_PLACEHOLDER(log_builtins);
REQUIRES_SERVICE_PLACEHOLDER(log_builtins_string);
REQUIRES_SERVICE_PLACEHOLDER(log_builtins_syseventlog);
#ifdef _WIN32
REQUIRES_SERVICE_PLACEHOLDER(log_builtins_tmp);
#endif

// shorter component pointers for legibility
SERVICE_TYPE(log_builtins) *log_bi = nullptr;
SERVICE_TYPE(log_builtins_string) *log_bs = nullptr;
SERVICE_TYPE(log_builtins_syseventlog)
*log_se = nullptr;
#ifdef _WIN32
SERVICE_TYPE(log_builtins_tmp) *log_bt = nullptr;
#define LOG_TYPE "Eventlog"
#else
#define LOG_TYPE "syslog"
#define MAX_FAC_LEN 32
#endif
#define MAX_TAG_LEN 32

// internal state
#ifndef _WIN32
static int log_syslog_facility = LOG_DAEMON;  ///< facility we're syslogging to
static bool log_syslog_include_pid = true;    ///< log process ID
#endif
static char *log_syslog_ident = nullptr;  ///< ident we're using (see "tag")
static bool log_syslog_enabled = false;   ///< logging engaged

// The following describe our system variables (their ranges, defaults, etc.).
STR_CHECK_ARG(tag) values_tag;  ///< syslog default tag
#ifndef _WIN32
STR_CHECK_ARG(fac) values_fac;   ///< syslog default facility
BOOL_CHECK_ARG(pid) values_pid;  ///< syslog default PID state
#endif

static char *buffer_tag = nullptr;  ///< sysvar containing tag, if any
static char *buffer_fac = nullptr;  ///< sysvar containing fac, if any

#ifndef _WIN32
/*
  Logs historically have subtly different names, to meet each platform's
  conventions -- "mysqld" on unix (via mysqld_safe), and "MySQL" for the
  Win NT EventLog.
*/
#define PREFIX "mysqld"
#else
#define PREFIX "MySQL"
#endif
#define MY_NAME "syseventlog"

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

  @retval  -3  log already open, close it before opening again! (log still open)
  @retval  -2  cannot set up new registry entry, continuing with previous value
               (log still open, but continues to log under previous key)
  @retval  -1  cannot set up new registry entry, no previous value
               (log not opened)
  @retval   0  success (log opened)
*/
int log_syslog_open() {
  int ret;
  const char *ident = (log_syslog_ident != nullptr) ? log_syslog_ident : PREFIX;

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
    log_bi->message(/* purecov: inspected */
                    LOG_TYPE_ERROR, LOG_ITEM_LOG_PRIO, ERROR_LEVEL,
                    LOG_ITEM_LOG_LOOKUP,
                    ER_COULD_NOT_CREATE_WINDOWS_REGISTRY_KEY, MY_NAME, ident,
                    "logging");
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

/*
  Functions to validate and set variables governing the sys/event-logging
  (tag/ident, facility, etc.). The following functions manipulate the
  internal representation of these values. They are used by a layer
  of callback functions run on changes of our component system variables.
*/

/**
  Internal state: Validate new tag to log under

  @param tag  a string containing the tag

  @retval 0    no complaints
  @retval -1   no argument
  @retval 1    invalid argument
*/
static int var_check_tag(const char *tag) {
  if (tag == nullptr) return -1;
  if (strchr(tag, '/') != nullptr) return 1;
  if (strchr(tag, '\\') != nullptr) return 1;
  if (log_bs->length(tag) >= MAX_TAG_LEN) return 1;
  return 0;
}

/**
  Internal state: Update tag to log under

  @param tag  new tag to log under (will be appended to PREFIX)

  @retval  0  success
  @retval -1  EINVAL
  @retval -2  out of memory
*/
static int var_update_tag(const char *tag) {
  char *new_ident = nullptr, *old_ident = nullptr;
  bool ident_changed = false;

  // tag must not contain directory separators
  if ((tag != nullptr) && (strchr(tag, FN_LIBCHAR) != nullptr)) return -1;

  /*
    make ident
  */

  if ((tag == nullptr) || (*tag == '\0'))
    new_ident = log_bs->strndup(PREFIX, log_bs->length(PREFIX));
  else {
    // prefix + '-' + '\0' + tag
    size_t l = log_bs->length(PREFIX) + 1 + 1 + log_bs->length(tag);

    new_ident = (char *)log_bs->malloc(l);
    if (new_ident != nullptr)
      log_bs->substitute(new_ident, l, "%s-%s", PREFIX, tag);
  }

  // if we succeeded in making an ident, replace the old one
  if (new_ident != nullptr) {
    ident_changed = (log_syslog_ident == nullptr) ||
                    (strcmp(new_ident, log_syslog_ident) != 0);
  } else
    return -2; /* purecov: inspected */

  if (ident_changed) {
    old_ident = log_syslog_ident;
    log_syslog_ident = new_ident;

    log_syslog_reopen();

    if (old_ident != nullptr) log_bs->free(old_ident);
  } else
    log_bs->free(new_ident);

  return 0;
}

#ifndef _WIN32
/**
  Internal state: Validate facility to log under

  @param fac  a string containing the facility

  @retval 0    no complaints
  @retval -1   unknown facility
  @retval -2   facility name exceeds buffer
*/
static int var_check_fac(const char *fac) {
  SYSLOG_FACILITY rsf;

  if (log_syslog_find_facility(fac, &rsf))
    return -1;
  else if (log_bs->length(fac) >= MAX_FAC_LEN)
    return -2; /* purecov: inspected */
  return 0;
}

/**
  Internal state: Update facility to log under
  May change facility to its canonical representation if needed.

  @param fac  new facility to log under

  @retval  0  success
*/
static int var_update_fac(char *fac) {
  SYSLOG_FACILITY rsf = {LOG_DAEMON, LOG_DAEMON_NAME};

  /*
    make facility
  */

  DBUG_ASSERT(fac != nullptr);

  log_syslog_find_facility(fac, &rsf);

  // If NaN, set to the canonical form (cut "log_", fix case)
  if ((rsf.name != nullptr) && (strcmp(fac, rsf.name) != 0))
    strcpy(fac, rsf.name);

  // if the value has actually changed, tell the subsystem about it
  if (log_syslog_facility != rsf.id) {
    log_syslog_facility = rsf.id;
    log_syslog_reopen();
  }

  // signal success (always, since we fail gracefully with a sensible default)
  return 0;
}

/**
  Internal state: Toggle inclusion of process ID (pid)

  @param inc_pid  include PID?

  @retval  0  success
*/
static int var_update_pid(bool inc_pid) {
  if (inc_pid != log_syslog_include_pid) {
    log_syslog_include_pid = inc_pid;
    log_syslog_reopen();
  }
  return 0;
}
#endif

/*
  Component system variable handling.
  Uses the above functions to manipulate internal state.
*/

/**
  System-variable:
  Check proposed value for component variable controlling tag to log under.
  Queries internal state as needed.

  @param  thd      session
  @param  self     the system variable we're checking
  @param  save     where to save the resulting intermediate (char *) value
  @param  value    the value we're validating

  @retval false    value OK, go ahead and update system variable (from "save")
  @retval true     value rejected, do not update variable
*/
static int sysvar_check_tag(MYSQL_THD thd MY_ATTRIBUTE((unused)),
                            SYS_VAR *self MY_ATTRIBUTE((unused)), void *save,
                            struct st_mysql_value *value) {
  int value_len = 0;
  const char *proposed_value;

  if (value == nullptr) return true;

  proposed_value = value->val_str(value, nullptr, &value_len);

  if (proposed_value == nullptr) return true;

  DBUG_ASSERT(proposed_value[value_len] == '\0');

  if (var_check_tag(proposed_value) != 0)  // no complaints?
    return true;

  *static_cast<const char **>(save) = proposed_value;
  return false;
}

/**
  System-variable:
  Update value of component variable controlling tag to log under
  Updates internal state as needed.

  @param  thd      session
  @param  self     the system variable we're changing
  @param  var_ptr  where to save the resulting (char *) value
  @param  save     pointer to the new value (from check function)
*/
static void sysvar_update_tag(MYSQL_THD thd MY_ATTRIBUTE((unused)),
                              SYS_VAR *self MY_ATTRIBUTE((unused)),
                              void *var_ptr, const void *save) {
  const char *new_val = *((const char **)save);

  var_update_tag(new_val);

  if (var_ptr != nullptr) {
    // the caller will free the old value, don't double free it here!
    *((const char **)var_ptr) = new_val;
  }
}

/**
  Set up system variable containing the tag to log under (for ident).

  @retval 0   success
  @retval -1  failure
*/
static int sysvar_install_tag(void) {
  char *var_value;
  char *new_value;
  size_t var_len = 0;
  int rr = -1;

  if ((var_value = new char[MAX_TAG_LEN + 1]) == nullptr) return -1;

  values_tag.def_val = (char *)"";

  DBUG_ASSERT(buffer_tag == nullptr);

  if (mysql_service_component_sys_variable_register->register_variable(
          MY_NAME, OPT_TAG, PLUGIN_VAR_STR | PLUGIN_VAR_MEMALLOC,
          "When logging issues using the host operating system's " LOG_TYPE ", "
          "tag the entries from this particular MySQL server with this ident. "
          "This will help distinguish entries from MySQL servers co-existing "
          "on the same host machine. A non-empty tag will be appended to the "
          "default ident of '" PREFIX "', connected by a hyphen.",
          sysvar_check_tag, sysvar_update_tag, (void *)&values_tag,
          (void *)&buffer_tag) ||
      mysql_service_component_sys_variable_register->get_variable(
          MY_NAME, OPT_TAG, (void **)&var_value, &var_len))
    goto done; /* purecov: inspected */

  /*
    We asked the server for a value for this variable as it may have
    been SET PERSISTed.
  */

  if ((rr = var_check_tag(var_value)))
    log_bi->message(LOG_TYPE_ERROR, LOG_ITEM_LOG_PRIO, WARNING_LEVEL,
                    LOG_ITEM_LOG_LOOKUP, ER_SERVER_WRONG_VALUE_FOR_VAR,
                    MY_NAME "." OPT_TAG, var_value);

  /*
    If the actual setup worked, but we were passed an invalid value
    for the variable, try to set default!
  */

  new_value = (rr == 0) ? var_value : values_tag.def_val;

  if (!var_update_tag(new_value)) {
    /*
      Update of internal state succeeded!
      Update system variable's value if adjusted.
    */
    char *old = buffer_tag;
    if ((buffer_tag = log_bs->strndup(
             new_value, log_bs->length(new_value) + 1)) != nullptr) {
      if (old != nullptr) log_bs->free((void *)old); /* purecov: inspected */
      rr = 0;
      goto done;
    }

    // if we failed to copy the default, restore the previous value
    buffer_tag = old; /* purecov: inspected */
  }

  rr = -1; /* purecov: inspected */

done:
  delete[] var_value;
  return rr;
}

#ifndef _WIN32

/**
  System-variable:
  Check proposed value for component variable controlling facility to log under.
  Queries internal state as needed.

  @param  thd      session
  @param  self     the system variable we're checking
  @param  save     where to save the resulting intermediate (char *) value
  @param  value    the value we're validating

  @retval false    value OK, go ahead and update system variable (from "save")
  @retval true     value rejected, do not update variable
*/
static int sysvar_check_fac(MYSQL_THD thd MY_ATTRIBUTE((unused)),
                            SYS_VAR *self MY_ATTRIBUTE((unused)), void *save,
                            struct st_mysql_value *value) {
  int value_len = 0;
  const char *proposed_value;

  if (value == nullptr) return true;

  proposed_value = value->val_str(value, nullptr, &value_len);

  if (proposed_value == nullptr) return true;

  DBUG_ASSERT(proposed_value[value_len] == '\0');

  if (var_check_fac(proposed_value) != 0)  // if value is invalid, bail
    return true;

  *static_cast<const char **>(save) = proposed_value;
  return false;
}

/**
  System-variable:
  Update value of component variable controlling facilty to log under
  Updates internal state as needed.

  @param  thd      session
  @param  self     the system variable we're changing
  @param  var_ptr  where to save the resulting (char *) value
  @param  save     pointer to the new value (from check function)
*/
static void sysvar_update_fac(MYSQL_THD thd MY_ATTRIBUTE((unused)),
                              SYS_VAR *self MY_ATTRIBUTE((unused)),
                              void *var_ptr, const void *save) {
  char *new_val = *((char **)save);

  var_update_fac(new_val);

  if (var_ptr != nullptr) {
    // the caller will free the old value, don't double free it here!
    *((char **)var_ptr) = new_val;
  }
}

/**
  Set up system variable containing the tag to log under (for ident).

  @retval 0   success
  @retval -1  failure
*/
static int sysvar_install_fac(void) {
  char *var_value;
  char *new_value;
  size_t var_len = 0;
  int rr = -1;

  if ((var_value = new char[MAX_FAC_LEN + 1]) == nullptr) return -1;

  values_fac.def_val = (char *)LOG_DAEMON_NAME;

  if (mysql_service_component_sys_variable_register->register_variable(
          MY_NAME, OPT_FAC, PLUGIN_VAR_STR | PLUGIN_VAR_MEMALLOC,
          "When logging issues using the host operating system's syslog, "
          "identify as a facility of the given type (to aid in log filtering).",
          sysvar_check_fac, sysvar_update_fac, (void *)&values_fac,
          (void *)&buffer_fac) ||
      mysql_service_component_sys_variable_register->get_variable(
          MY_NAME, OPT_FAC, (void **)&var_value, &var_len))
    goto done; /* purecov: inspected */

  /*
    We asked the server for a value for this variable as it may have
    been SET PERSISTed.
  */

  if ((rr = var_check_fac(var_value)))
    log_bi->message(LOG_TYPE_ERROR, LOG_ITEM_LOG_PRIO, WARNING_LEVEL,
                    LOG_ITEM_LOG_LOOKUP, ER_SERVER_WRONG_VALUE_FOR_VAR,
                    MY_NAME "." OPT_FAC, var_value);

  /*
    If the actual setup worked, but we were passed an invalid value
    for the variable, try to set default!
  */

  new_value = (rr == 0) ? var_value : values_fac.def_val;

  var_update_fac(new_value);

  /*
    Update of internal state succeeded!
    Update system variable's value if adjusted.
  */
  if (rr != 0) {
    char *old = buffer_fac;
    if ((buffer_fac = log_bs->strndup(
             new_value, log_bs->length(new_value) + 1)) != nullptr) {
      if (old != nullptr) log_bs->free((void *)old);
      rr = 0;
      goto done;
    }

    // if we failed to copy the default, restore the previous value
    buffer_fac = old; /* purecov: inspected */
    rr = -1;          /* purecov: inspected */
  }

done:
  delete[] var_value;
  return rr;
}

/**
  System-variable:
  Update value of component variable governing inclusion of process ID (pid)
  Updates internal state as needed.

  @param  thd      session
  @param  self     the system variable we're changing
  @param  var_ptr  where to save the resulting (char *) value
  @param  save     pointer to the new value (from check function)
*/
static void sysvar_update_pid(MYSQL_THD thd MY_ATTRIBUTE((unused)),
                              SYS_VAR *self MY_ATTRIBUTE((unused)),
                              void *var_ptr MY_ATTRIBUTE((unused)),
                              const void *save) {
  var_update_pid(*((bool *)save));
}

/*
  Set up system variable governing the inclusion of the process ID (pid).

  @retval 0   success
  @retval -1  failure
*/
static int sysvar_install_pid(void) {
  char *var_value = nullptr;
  size_t var_len;
  bool var_bool;
  int rr = -1;

  values_pid.def_val = log_syslog_include_pid;

  if ((var_value = new char[16]) == nullptr) return -1;

  // register variable
  if (mysql_service_component_sys_variable_register->register_variable(
          MY_NAME, OPT_PID, PLUGIN_VAR_BOOL,
          "When logging issues using the host operating system's log "
          "(\"" LOG_TYPE "\"), include this MySQL server's process ID (PID). "
          "This setting does not affect MySQL's own error log file.",
          nullptr, sysvar_update_pid, (void *)&values_pid,
          (void *)&log_syslog_include_pid) ||

      // get variable in case it was PERSISTed
      mysql_service_component_sys_variable_register->get_variable(
          MY_NAME, OPT_PID, (void **)&var_value, &var_len))
    goto done; /* purecov: inspected */

  // set the (possibly PERSISTed) value we received from the server
  var_bool = ((native_strcasecmp(var_value, "ON") == 0));
  rr = var_update_pid(var_bool);

done:
  delete[] var_value;
  return rr;
}

#endif

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
    log_se->write(level, msg);
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

    // release system variables if we hold any
#ifndef _WIN32
    mysql_service_component_sys_variable_unregister->unregister_variable(
        MY_NAME, OPT_PID);
    mysql_service_component_sys_variable_unregister->unregister_variable(
        MY_NAME, OPT_FAC);
#endif
    mysql_service_component_sys_variable_unregister->unregister_variable(
        MY_NAME, OPT_TAG);

    log_bi = nullptr;
    log_bs = nullptr;
    log_se = nullptr;
#ifdef _WIN32
    log_bt = nullptr;
#endif

    buffer_tag = nullptr;
    buffer_fac = nullptr;

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
    Register our system variables.
    Enable last, so all the values are correct by the time we open
    the log (which is especially useful when using a non-default tag
    on Windows as it prevents us from first creating a tag in the
    registry that we then won't use).
  */

  /* try to set up system-variables */
  if (sysvar_install_tag()
#ifndef _WIN32
      || sysvar_install_fac() || sysvar_install_pid()
#endif
  )
    goto fail; /* purecov: inspected */

  /*
    If this component is loaded, we enable it by default, as that's
    probably what the user expects.
  */
  log_syslog_open();
  if (!log_syslog_enabled) goto fail;

  return false;

  /*
    If we failed to open our log, try to log the failure to any open loggers.
  */

fail:
  /* purecov: begin inspected */
  log_bi->message(LOG_TYPE_ERROR, LOG_ITEM_LOG_PRIO, ERROR_LEVEL,
                  LOG_ITEM_LOG_LOOKUP, ER_LOG_SYSLOG_CANNOT_OPEN, LOG_TYPE);

  log_service_exit();
  return true; /* purecov: end */
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

/**
  Get characteristics of a log-service.

  @retval  <0        an error occurred
  @retval  >=0       characteristics (a set of log_service_chistics flags)
*/
DEFINE_METHOD(int, log_service_imp::characteristics, (void)) {
  return LOG_SERVICE_SINK | LOG_SERVICE_SINGLETON;
}

/* implementing a service: log_service */
BEGIN_SERVICE_IMPLEMENTATION(log_sink_syseventlog, log_service)
log_service_imp::run, log_service_imp::flush, log_service_imp::open,
    log_service_imp::close,
    log_service_imp::characteristics END_SERVICE_IMPLEMENTATION();

/* component provides: just the log_service service, for now */
BEGIN_COMPONENT_PROVIDES(log_sink_syseventlog)
PROVIDES_SERVICE(log_sink_syseventlog, log_service), END_COMPONENT_PROVIDES();

/* component requires: log-builtins */
BEGIN_COMPONENT_REQUIRES(log_sink_syseventlog)
REQUIRES_SERVICE(component_sys_variable_register),
    REQUIRES_SERVICE(component_sys_variable_unregister),
    REQUIRES_SERVICE(log_builtins), REQUIRES_SERVICE(log_builtins_string),
    REQUIRES_SERVICE(log_builtins_syseventlog),
#ifdef _WIN32
    REQUIRES_SERVICE(log_builtins_tmp),
#endif
    END_COMPONENT_REQUIRES();

/* component description */
BEGIN_COMPONENT_METADATA(log_sink_syseventlog)
METADATA("mysql.author", "T.A. Nuernberg, Oracle Corporation"),
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

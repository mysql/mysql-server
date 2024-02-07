/*
   Copyright (c) 2014, 2024, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "storage/ndb/plugin/ndb_log.h"

#include <mysql/components/services/log_builtins.h>

#include "my_dbug.h"
#include "mysqld_error.h"
/*
  Implements a logging interface for the ndbcluster
  plugin using the LogEvent class as defined in log_builtins.h
  Beware, the #include ordering here matters!
*/
#include "sql/log.h"

/*
  Submit message to logging interface

  @param loglevel    Selects the loglevel used when
                     printing the message to log.
  @param prefix      Prefix to be used in front of the message, this is
                     primarily used by the Ndb_component framework where each
                     component will have their messages prefixed.
  @param[in]  fmt    printf-like format string
  @param[in]  ap     Arguments

*/

void ndb_log_print(enum ndb_log_loglevel loglevel, const char *prefix,
                   const char *fmt, va_list args) {
  assert(fmt);

  // Translate loglevel to prio
  auto loglevel2prio = [](ndb_log_loglevel loglevel) {
    switch (loglevel) {
      case NDB_LOG_ERROR_LEVEL:
        return ERROR_LEVEL;
      case NDB_LOG_WARNING_LEVEL:
        return WARNING_LEVEL;
      case NDB_LOG_INFORMATION_LEVEL:
        // Informational log messages are used to notify about important state
        // changes in this server and its connection to the cluster ->  use
        // SYSTEM_LEVEL to avoid that they are filtered out by the
        // --log-error-verbosity setting.
        // This means that messages from `ndb_log_info()` will always be logged
        // while messages from `ndb_log_verbose()` will be controlled by the
        // --ndb-extra-logging=<number> variable.
        return SYSTEM_LEVEL;
      default:
        // Should never happen, crash in debug
        DBUG_ABORT();
        return ERROR_LEVEL;
    }
  };

  // Print message to MySQL error log
  LogEvent log_event;
  log_event.prio(loglevel2prio(loglevel));
  log_event.subsys("NDB");

  if (prefix != nullptr) {
    // Log with given prefix, i.e "[NDB] Binlog: logging..."
    // primarily used by Ndb_component instances
    constexpr int PREFIXED_FORMAT_BUF_MAX = 512;
    char prefixed_fmt[PREFIXED_FORMAT_BUF_MAX];
    snprintf(prefixed_fmt, sizeof(prefixed_fmt) - 1, "%s: %s", prefix, fmt);
    log_event.errcode(ER_NDB_LOG_ENTRY_WITH_PREFIX);
    log_event.messagev(prefixed_fmt, args);
  } else {
    // Non prefixed message i.e "[NDB] Creating table..."
    log_event.errcode(ER_NDB_LOG_ENTRY);
    log_event.messagev(fmt, args);
  }
}

/*
  Check log message and any prefix it may contain

  @param fmt             The log message format to be checked

  @note  In debug compile the function will perform checks to make sure that
         the format string follow the rules. The intention is that
         faulty prefix usage should be detected but allowed
         otherwise.
*/
static void ndb_log_check_prefix(const char *fmt [[maybe_unused]]) {
  // Check if string starts with prefix "NDB", this prefix is redundant
  // since all log messages will be prefixed with [NDB] anyway
  // Crash in debug compile, caller should fix by removing prefix "NDB"
  // from the printout
  assert(strncmp(fmt, "NDB", 3) != 0);
}

void ndb_log_info(const char *fmt, ...) {
  ndb_log_check_prefix(fmt);

  va_list args;
  va_start(args, fmt);
  ndb_log_print(NDB_LOG_INFORMATION_LEVEL, nullptr, fmt, args);
  va_end(args);
}

void ndb_log_warning(const char *fmt, ...) {
  ndb_log_check_prefix(fmt);

  va_list args;
  va_start(args, fmt);
  ndb_log_print(NDB_LOG_WARNING_LEVEL, nullptr, fmt, args);
  va_end(args);
}

void ndb_log_error(const char *fmt, ...) {
  ndb_log_check_prefix(fmt);

  va_list args;
  va_start(args, fmt);
  ndb_log_print(NDB_LOG_ERROR_LEVEL, nullptr, fmt, args);
  va_end(args);
}

// the verbose level is controlled by "--ndb_extra_logging"
extern ulong opt_ndb_extra_logging;

unsigned ndb_log_get_verbose_level(void) { return opt_ndb_extra_logging; }

void ndb_log_verbose(unsigned verbose_level, const char *fmt, ...) {
  // Print message only if verbose level is set high enough
  if (ndb_log_get_verbose_level() < verbose_level) return;

  ndb_log_check_prefix(fmt);

  va_list args;
  va_start(args, fmt);
  ndb_log_print(NDB_LOG_INFORMATION_LEVEL, nullptr, fmt, args);
  va_end(args);
}

void ndb_log_flush_buffered_messages() { flush_error_log_messages(); }

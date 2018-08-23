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

#ifndef DD_UPGRADE__GLOBAL_H_INCLUDED
#define DD_UPGRADE__GLOBAL_H_INCLUDED

#include <sys/types.h>

#include "my_inttypes.h"
#include "mysql/components/services/log_builtins.h"
#include "mysql/components/services/log_shared.h"
#include "sql/dd/string_type.h"
#include "sql/error_handler.h"  // Internal_error_handler
#include "sql/item_create.h"
#include "sql/sql_class.h"
#include "sql/sql_servers.h"
#include "sql/table.h"  // Table_check_intact

class THD;
class Time_zone;

using sql_mode_t = ulonglong;

namespace dd {
/**
  Class to keep track of upgrade errors during upgrade after 8.0 GA.
*/
class Upgrade_error_counter {
 private:
  int m_error_count = 0;
  const int ERROR_LIMIT = 50;

 public:
  bool has_errors() { return (m_error_count > 0); }
  bool has_too_many_errors() { return (m_error_count > ERROR_LIMIT); }
  Upgrade_error_counter operator++(int) {
    m_error_count++;
    return *this;
  }
};

namespace upgrade_57 {

const String_type ISL_EXT = ".isl";
const String_type PAR_EXT = ".par";
const String_type OPT_EXT = ".opt";
extern const char *TRN_EXT;
extern const char *TRG_EXT;

const String_type IBD_EXT = ".ibd";
const String_type index_stats = "innodb_index_stats";
const String_type index_stats_backup = "innodb_index_stats_backup57";
const String_type table_stats = "innodb_table_stats";
const String_type table_stats_backup = "innodb_table_stats_backup57";

/**
  THD::mem_root is only switched with the given mem_root and switched back
  on destruction. This does not free any mem_root.
 */
class Thd_mem_root_guard {
  THD *m_thd;
  MEM_ROOT *m_thd_prev_mem_root;

 public:
  Thd_mem_root_guard(THD *thd, MEM_ROOT *mem_root);
  ~Thd_mem_root_guard();
};

/**
   RAII for handling open and close of event and proc tables.
*/

class System_table_close_guard {
  THD *m_thd;
  TABLE *m_table;

 public:
  System_table_close_guard(THD *thd, TABLE *table);
  ~System_table_close_guard();
};

/**
  Class to check the system tables we are using from 5.7 are
  not corrupted before migrating the information to new DD.
*/
class Check_table_intact : public Table_check_intact {
 protected:
  void report_error(uint, const char *fmt, ...)
      MY_ATTRIBUTE((format(printf, 3, 4)));
};

/**
   RAII for handling creation context of Events and
   Stored routines.
*/

class Routine_event_context_guard {
  THD *m_thd;
  sql_mode_t m_sql_mode;
  ::Time_zone *m_saved_time_zone;
  const CHARSET_INFO *m_client_cs;
  const CHARSET_INFO *m_connection_cl;

 public:
  Routine_event_context_guard(THD *thd);
  ~Routine_event_context_guard();
};

/**
  Bootstrap thread executes SQL statements.
  Any error in the execution of SQL statements causes call to my_error().
  At this moment, error handler hook is set to my_message_stderr.
  my_message_stderr() prints the error messages to standard error stream but
  it does not follow the standard error format. Further, the error status is
  not set in Diagnostics Area.

  This class is to create RAII error handler hooks to be used when executing
  statements from bootstrap thread.

  It will print the error in the standard error format.
  Diagnostics Area error status will be set to avoid asserts.
  Error will be handler by caller function.
*/

class Bootstrap_error_handler {
 private:
  void (*m_old_error_handler_hook)(uint, const char *, myf);

  //  Set the error in DA. Optionally print error in log.
  static void my_message_bootstrap(uint error, const char *str, myf MyFlags) {
    set_abort_on_error(error);
    my_message_sql(error, str, MyFlags);
    if (m_log_error)
      LogEvent()
          .type(LOG_TYPE_ERROR)
          .subsys(LOG_SUBSYSTEM_TAG)
          .prio(ERROR_LEVEL)
          .errcode(ER_ERROR_INFO_FROM_DA)
          .verbatim(str);
  }

  // Set abort on error flag and enable error logging for certain fatal error.
  static void set_abort_on_error(uint error) {
    switch (error) {
      case ER_WRONG_COLUMN_NAME: {
        abort_on_error = true;
        m_log_error = true;
        break;
      }
      default:
        break;
    }
  }

 public:
  Bootstrap_error_handler() {
    m_old_error_handler_hook = error_handler_hook;
    error_handler_hook = my_message_bootstrap;
  }

  // Mark as error is set.
  void set_log_error(bool log_error) { m_log_error = log_error; }

  ~Bootstrap_error_handler() { error_handler_hook = m_old_error_handler_hook; }
  static bool m_log_error;
  static bool abort_on_error;
};

/**
  This class keeps a count of all the syntax errors that occured while parsing
  views, routines, events or triggers. This count is used along with
  MAX_SERVER_CHECK_FAILS to exit upgrade.
*/
class Syntax_error_handler : public Internal_error_handler {
 public:
  Syntax_error_handler() {}
  Syntax_error_handler(Upgrade_error_counter *counter)
      : m_global_counter(counter) {}
  virtual bool handle_condition(THD *, uint sql_errno, const char *,
                                Sql_condition::enum_severity_level *,
                                const char *msg) {
    if (sql_errno == ER_PARSE_ERROR) {
      parse_error_count++;
      if (m_global_counter) (*m_global_counter)++;
      is_parse_error = true;
      reason = msg;
    } else {
      is_parse_error = false;
      reason = "";
    }
    return false;
  }

  static bool has_too_many_errors() {
    return parse_error_count > MAX_SERVER_CHECK_FAILS;
  }

  static bool has_errors() { return parse_error_count > 0; }

  static const char *error_message() { return reason.c_str(); }

  static uint parse_error_count;
  static bool is_parse_error;
  static const uint MAX_SERVER_CHECK_FAILS = 50;
  static dd::String_type reason;
  Upgrade_error_counter *m_global_counter = nullptr;
};

/**
 * @brief Validate the SQL string provided.
 *
 * @param thd       Thread handle
 * @param dbname    The database used in the SQL string's context.
 * @param sql       The SQL string to be validated
 * @return true
 * @return false
 */
bool invalid_sql(THD *thd, const char *dbname, const dd::String_type &sql);

}  // namespace upgrade_57
}  // namespace dd
#endif  // DD_UPGRADE__GLOBAL_H_INCLUDED

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
#include "sql/item_create.h"
#include "sql/sql_class.h"
#include "sql/sql_servers.h"
#include "sql/table.h"  // Table_check_intact

class THD;
class Time_zone;

using sql_mode_t = ulonglong;

namespace dd {
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
   RAII for handling open and close of event and proc tables.
*/

class System_table_close_guard {
  THD *m_thd;
  TABLE *m_table;
  MEM_ROOT *m_mem_root;

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

}  // namespace upgrade_57
}  // namespace dd
#endif  // DD_UPGRADE__GLOBAL_H_INCLUDED

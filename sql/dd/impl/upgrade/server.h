/* Copyright (c) 2019, 2024, Oracle and/or its affiliates.

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

#ifndef DD_UPGRADE_IMPL__SERVER_H_INCLUDED
#define DD_UPGRADE_IMPL__SERVER_H_INCLUDED

#include <stdint.h>
#include <stdio.h>

#include <set>

#include "my_inttypes.h"
#include "my_sys.h"  // ErrorHandlerFunctionPointer
#include "sql/dd/string_type.h"
#include "sql/error_handler.h"  // Internal_error_handler

class THD;
class Time_zone;

struct CHARSET_INFO;

using sql_mode_t = uint64_t;

namespace dd {
class Routine;
class Schema;
class Table;

namespace upgrade {
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
  ErrorHandlerFunctionPointer m_old_error_handler_hook;

  //  Set the error in DA. Optionally print error in log.
  static void my_message_bootstrap(uint error, const char *str, myf MyFlags);

  // Set abort on error flag and enable error logging for certain fatal error.
  static void set_abort_on_error(uint error);

  // Check if error should be logged.
  static bool should_log_error(uint error);

 public:
  Bootstrap_error_handler();

  // Log all errors to the error log file too.
  void set_log_error(bool log_error);

  void set_allowlist_errors(std::set<uint> &error_codes);
  void clear_allowlist_errors();

  ~Bootstrap_error_handler();
  static bool m_log_error;
  static bool abort_on_error;
  // Set of errors which are logged to error log file always.
  static std::set<uint> m_allowlist_errors;
};

/**
  Class to keep track of upgrade errors during upgrade after 8.0 GA.
*/
class Upgrade_error_counter {
 private:
  int m_error_count;
  const int ERROR_LIMIT;

 public:
  Upgrade_error_counter() : m_error_count(0), ERROR_LIMIT(50) {}
  bool has_errors();
  bool has_too_many_errors();
  Upgrade_error_counter operator++(int);
  Upgrade_error_counter operator--(int);
};

/**
  This class keeps a count of all the syntax errors that occurred while parsing
  views, routines, events or triggers. This count is used along with
  MAX_SERVER_CHECK_FAILS to exit upgrade.
*/
class Syntax_error_handler : public Internal_error_handler {
 public:
  Syntax_error_handler() : m_global_counter(nullptr) {}
  Syntax_error_handler(Upgrade_error_counter *counter)
      : m_global_counter(counter) {}
  bool handle_condition(THD *, uint sql_errno, const char *,
                        Sql_condition::enum_severity_level *,
                        const char *msg) override;
  void reset_last_condition();

  static bool has_too_many_errors();
  static bool has_errors();
  static const char *error_message();

  static uint parse_error_count;
  static bool is_parse_error;
  static const uint MAX_SERVER_CHECK_FAILS;
  static dd::String_type reason;
  Upgrade_error_counter *m_global_counter;
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
  Maintain a file named "mysql_upgrade_history" in the data directory.

  The file will contain one entry for each upgrade. The format is structured
  text on JSON format.

  Errors will be written as warnings to the error log; if we e.g. fail to
  open the upgrade history file, we will not abort the server since this file
  is not considered a critical feature of the server.

  @param initialize   If this is the initialization of the data directory.
*/
void update_upgrade_history_file(bool initialize);

/**
  Performs validation on server metadata.

  @param thd    Thread context.

  @return       Upon failure, return true, otherwise false.
 */
bool do_server_upgrade_checks(THD *thd);

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

/**
  Validate all the triggers of the given table.

  @param[in]  thd                        Thread handle.
  @param[in]  schema_name                Pointer for database name.
  @param[in]  table                      Triggers of the table to be checked.

  @retval false  ON SUCCESS
  @retval true   ON FAILURE
*/
bool invalid_triggers(THD *thd, const char *schema_name,
                      const dd::Table &table);

/**
  Validate a dd::Routine object.

  @param[in]  thd        Thread handle.
  @param[in]  schema     Schema in which the routine belongs.
  @param[in]  routine    Routine to be validated.

  @retval false  ON SUCCESS
  @retval true   ON FAILURE
*/
bool invalid_routine(THD *thd, const dd::Schema &schema,
                     const dd::Routine &routine);

/**
  Helper function to create a stored procedure from an event body.

  @param[in]  thd             Thread handle.
  @param[in]  name            Name of the event.
  @param[in]  name_len        Length of the name of the event.
  @param[in]  body            Body of the event.
  @param[in]  body_len        Length of the body of the event.
  @param[out] sp_sql          Stored procedure SQL string.

  @retval false  ON SUCCESS
  @retval true   ON FAILURE
*/
bool build_event_sp(const THD *thd, const char *name, size_t name_len,
                    const char *body, size_t body_len, dd::String_type *sp_sql);
}  // namespace upgrade

}  // namespace dd
#endif  // DD_UPGRADE_IMPL__SERVER_H_INCLUDED

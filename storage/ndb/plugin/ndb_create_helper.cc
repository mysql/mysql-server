/*
   Copyright (c) 2018, 2024, Oracle and/or its affiliates.

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

// Implements the functions declared in ndb_create_helper.h
#include "storage/ndb/plugin/ndb_create_helper.h"

// Using
#include "my_base.h"  // HA_ERR_GENERIC
#include "my_dbug.h"
#include "mysqld_error.h"
#include "sql/sql_class.h"
#include "sql/sql_error.h"
#include "storage/ndb/plugin/ndb_thd.h"
#include "storage/ndb/plugin/ndb_thd_ndb.h"

Ndb_create_helper::Ndb_create_helper(THD *thd, const char *table_name)
    : m_thd(thd), m_thd_ndb(get_thd_ndb(thd)), m_table_name(table_name) {}

void Ndb_create_helper::check_warnings_and_error() const {
  bool have_error = false;
  bool have_warning = false;
  uint error_code = 0;
  const Sql_condition *cond;
  const Diagnostics_area *da = m_thd->get_stmt_da();
  Diagnostics_area::Sql_condition_iterator it(da->sql_conditions());
  while ((cond = it++)) {
    DBUG_PRINT("info", ("condition: (%u) %s", cond->mysql_errno(),
                        cond->message_text()));
    switch (cond->severity()) {
      case Sql_condition::SL_WARNING:
        DBUG_PRINT("info", ("Found warning"));
        // Warnings should come before errors
        assert(!have_error);
        have_warning = true;
        break;
      case Sql_condition::SL_ERROR:
        DBUG_PRINT("info", ("Found error"));
        // There should not be more than one error
        assert(!have_error);
        have_error = true;
        error_code = cond->mysql_errno();
        break;
      case Sql_condition::SL_NOTE:
        DBUG_PRINT("info", ("Found note"));
        // Ignore notes for now
        break;
      default:
        // There are no other severities
        assert(false);
        break;
    }
  }

  // Check that an error has been set
  if (!have_error) assert(have_error);

  // Check that a warning which describes the failure has been set
  // in addition to the error message
  if (!have_warning) {
    DBUG_PRINT("info", ("No warning have been pushed"));
    switch (error_code) {
      // Some error codes are already descriptive enough and
      // are thus allowed to be returned without a warning
      case ER_ILLEGAL_HA_CREATE_OPTION:
        DBUG_PRINT("info", ("Allowing error %u without warning", error_code));
        break;
      default:
        assert(false);
        break;
    }
  }
}

bool Ndb_create_helper::have_warning() const {
  const Diagnostics_area *da = m_thd->get_stmt_da();
  Diagnostics_area::Sql_condition_iterator it(da->sql_conditions());
  const Sql_condition *cond;
  while ((cond = it++)) {
    if (cond->severity() == Sql_condition::SL_WARNING) return true;
  }
  return false;
}

int Ndb_create_helper::set_create_table_error() const {
  const char *append_warning_text = "";
  if (have_warning()) {
    append_warning_text = " (use SHOW WARNINGS for more info).";
  }
  if (thd_sql_command(m_thd) == SQLCOM_ALTER_TABLE ||
      thd_sql_command(m_thd) == SQLCOM_CREATE_INDEX) {
    // Error occurred while creating the destination table for a copying
    // alter table, print error message describing the problem
    my_printf_error(ER_CANT_CREATE_TABLE,
                    "Can\'t create destination table for copying alter "
                    "table%s",
                    MYF(0), append_warning_text);
  } else {
    // Print error saying that table couldn't be created
    my_printf_error(ER_CANT_CREATE_TABLE, "Can\'t create table \'%-.200s\'%s",
                    MYF(0), m_table_name, append_warning_text);
  }
  check_warnings_and_error();

  // The error has now been reported, return an error code which
  // tells ha_ndbcluster::print_error() that error can be ignored.
  return HA_ERR_GENERIC;
}

int Ndb_create_helper::failed_warning_already_pushed() const {
  // Check that warning describing the problem has already been pushed
  if (!have_warning()) {
    // Crash in debug compile
    assert(false);
  }

  return set_create_table_error();
}

int Ndb_create_helper::failed(uint code, const char *message) const {
  m_thd_ndb->push_warning(code, "%s", message);
  return set_create_table_error();
}

int Ndb_create_helper::failed_in_NDB(const NdbError &ndb_err) const {
  m_thd_ndb->push_ndb_error_warning(ndb_err);
  return set_create_table_error();
}

int Ndb_create_helper::failed_oom(const char *message) const {
  return failed(ER_OUTOFMEMORY, message);
}

int Ndb_create_helper::failed_internal_error(const char *message) const {
  return failed(ER_INTERNAL_ERROR, message);
}

int Ndb_create_helper::failed_missing_create_option(
    const char *description) const {
  return failed(ER_MISSING_HA_CREATE_OPTION, description);
}

int Ndb_create_helper::failed_illegal_create_option(const char *reason) const {
  // The format string does not allow the reason to be longer than 64 bytes
  assert(strlen(reason) < 64);
  my_error(ER_ILLEGAL_HA_CREATE_OPTION, MYF(0), "ndbcluster", reason);
  check_warnings_and_error();
  // The error has now been reported, return an error code which
  // tells ha_ndbcluster::print_error() that error can be ignored.
  return HA_ERR_GENERIC;
}

int Ndb_create_helper::succeeded() { return 0; }

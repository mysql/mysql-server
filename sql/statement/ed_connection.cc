/* Copyright (c) 2023, 2024, Oracle and/or its affiliates.

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

#include "sql/statement/ed_connection.h"

#include "sql/sql_prepare.h"
#include "sql/statement/protocol_local.h"
#include "sql/statement/statement_runnable.h"

/**
  Create a new "execute direct" connection.
*/

Ed_connection::Ed_connection(THD *thd)
    : m_diagnostics_area(false),
      m_thd(thd),
      m_rsets(nullptr),
      m_current_rset(nullptr) {}

/**
  Free all result sets of the previous statement, if any,
  and reset warnings and errors.

  Called before execution of the next query.
*/

void Ed_connection::free_old_result() {
  while (m_rsets) {
    Ed_result_set *rset = m_rsets->m_next_rset;
    delete m_rsets;
    m_rsets = rset;
  }
  m_current_rset = m_rsets;
  m_diagnostics_area.reset_diagnostics_area();
  m_diagnostics_area.reset_condition_info(m_thd);
}

/**
  A simple wrapper that uses a helper class to execute SQL statements.
*/

bool Ed_connection::execute_direct(LEX_STRING sql_text) {
  Statement_runnable execute_sql_statement(sql_text);
  DBUG_PRINT("ed_query", ("%s", sql_text.str));

  return execute_direct(&execute_sql_statement);
}

/**
  Execute a fragment of server functionality without an effect on
  thd, and store results in memory.

  Conventions:
  - the code fragment must finish with OK, EOF or ERROR.
  - the code fragment doesn't have to close thread tables,
  free memory, commit statement transaction or do any other
  cleanup that is normally done in the end of dispatch_command().

  @param server_runnable A code fragment to execute.
*/

bool Ed_connection::execute_direct(Server_runnable *server_runnable) {
  DBUG_TRACE;

  free_old_result(); /* Delete all data from previous execution, if any */

  Protocol_local protocol_local(m_thd, this);
  m_thd->push_protocol(&protocol_local);
  m_thd->push_diagnostics_area(&m_diagnostics_area);

  Prepared_statement stmt(m_thd);
  bool rc = stmt.execute_server_runnable(m_thd, server_runnable);
  m_thd->send_statement_status();

  m_thd->pop_protocol();
  m_thd->pop_diagnostics_area();
  /*
    Protocol_local makes use of m_current_rset to keep
    track of the last result set, while adding result sets to the end.
    Reset it to point to the first result set instead.
  */
  m_current_rset = m_rsets;

  /*
    Reset rewritten (for password obfuscation etc.) query after
    internal call from NDB etc.  Without this, a rewritten query
    would get "stuck" in SHOW PROCESSLIST.
  */
  m_thd->reset_rewritten_query();
  m_thd->reset_query_for_display();

  return rc;
}

/**
  A helper method that is called only during execution.

  Although Ed_connection doesn't support multi-statements,
  a statement may generate many result sets. All subsequent
  result sets are appended to the end.

  @pre This is called only by Protocol_local.
*/

void Ed_connection::add_result_set(Ed_result_set *ed_result_set) {
  if (m_rsets) {
    m_current_rset->m_next_rset = ed_result_set;
    /* While appending, use m_current_rset as a pointer to the tail. */
    m_current_rset = ed_result_set;
  } else
    m_current_rset = m_rsets = ed_result_set;
}

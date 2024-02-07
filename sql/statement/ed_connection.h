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

#ifndef SQL_ED_CONNECTION_H
#define SQL_ED_CONNECTION_H

#include "sql/sql_class.h"
#include "sql/sql_error.h"
#include "sql/statement/protocol_local.h"

class Server_runnable;

class Ed_connection final {
 public:
  /**
    Construct a new "execute direct" connection.

    The connection can be used to execute SQL statements.
    If the connection failed to initialize, the error
    will be returned on the attempt to execute a statement.

    @pre thd  must have no open tables
              while the connection is used. However,
              Ed_connection works okay in LOCK TABLES mode.
              Other properties of THD, such as the current warning
              information, errors, etc. do not matter and are
              preserved by Ed_connection. One thread may have many
              Ed_connections created for it.
  */
  Ed_connection(THD *thd);

  /**
    Execute one SQL statement.

    Until this method is executed, no other methods of
    Ed_connection can be used. Life cycle of Ed_connection is:

    Initialized -> a statement has been executed ->
    look at result, move to next result ->
    look at result, move to next result ->
    ...
    moved beyond the last result == Initialized.

    This method can be called repeatedly. Once it's invoked,
    results of the previous execution are lost.

    A result of execute_direct() can be either:

    - success, no result set rows. In this case get_field_count()
    returns 0. This happens after execution of INSERT, UPDATE,
    DELETE, DROP and similar statements. Some other methods, such
    as get_affected_rows() can be used to retrieve additional
    result information.

    - success, there are some result set rows (maybe 0). E.g.
    happens after SELECT. In this case get_field_count() returns
    the number of columns in a result set and store_result()
    can be used to retrieve a result set..

    - an error, methods to retrieve error information can
    be used.

    @return execution status
    @retval false  success, use get_field_count()
                   to determine what to do next.
    @retval true   error, use get_last_error()
                   to see the error number.
  */
  bool execute_direct(LEX_STRING sql_text);

  /**
    Same as the previous, but takes an instance of Server_runnable
    instead of SQL statement text.

    @return execution status

    @retval  false  success, use get_field_count()
                    if your code fragment is supposed to
                    return a result set
    @retval  true   failure
  */
  bool execute_direct(Server_runnable *server_runnable);

  /**
    The following three members are only valid if execute_direct()
    or move_to_next_result() returned an error.
    They never fail, but if they are called when there is no
    result, or no error, the result is not defined.
  */
  const char *get_last_error() const {
    return m_diagnostics_area.message_text();
  }

  unsigned int get_last_errno() const {
    return m_diagnostics_area.mysql_errno();
  }

  Ed_result_set *get_result_sets() { return m_rsets; }

  ~Ed_connection() { free_old_result(); }

 private:
  Diagnostics_area m_diagnostics_area;
  /**
    Execute direct interface does not support multi-statements, only
    multi-results. So we never have a situation when we have
    a mix of result sets and OK or error packets. We either
    have a single result set, a single error, or a single OK,
    or we have a series of result sets, followed by an OK or error.
  */
  THD *m_thd;
  Ed_result_set *m_rsets;
  Ed_result_set *m_current_rset;
  friend class Protocol_local;

 private:
  void free_old_result();
  void add_result_set(Ed_result_set *ed_result_set);

 private:
  Ed_connection(const Ed_connection &);      /* not implemented */
  Ed_connection &operator=(Ed_connection &); /* not implemented */
};

#endif

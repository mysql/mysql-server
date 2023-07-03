/* Copyright (c) 2022, Oracle and/or its affiliates.

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

#ifndef XA_SQL_CMD_XA_START
#define XA_SQL_CMD_XA_START

#include "sql/sql_cmd.h"  // Sql_cmd
#include "sql/xa.h"       // xid_t

/**
  @class Sql_cmd_xa_start

  This class represents the `XA START ...` SQL statement which starts an XA
  transaction with the given xid value.

  @see Sql_cmd
*/
class Sql_cmd_xa_start : public Sql_cmd {
 public:
  /**
    Class constructor.

    @param xid_arg XID of the XA transacation about to start
    @param xa_option Additional options for the `XA START` command
   */
  Sql_cmd_xa_start(xid_t *xid_arg, enum xa_option_words xa_option);
  virtual ~Sql_cmd_xa_start() override = default;

  /**
    Retrieves the SQL command code for this class, `SQLCOM_XA_START`.

    @see Sql_cmd::sql_command_code

    @return The SQL command code for this class, `SQLCOM_XA_START`.
   */
  enum_sql_command sql_command_code() const override;
  /**
    Executes the SQL command.

    @see Sql_cmd::execute

    @param thd The `THD` session object within which the command is being
               executed.

    @return false if the execution is successful, true otherwise.
   */
  bool execute(THD *thd) override;

 private:
  /** The XID associated with the underlying XA transaction. */
  xid_t *m_xid;
  /** Options associated with the underlying `XA START`  */
  enum xa_option_words m_xa_opt;

  /**
    Start a XA transaction with the given xid value.

    @param thd    Current thread

    @retval false  Success
    @retval true   Failure
  */
  bool trans_xa_start(THD *thd);
};

#endif  // XA_SQL_CMD_XA_START

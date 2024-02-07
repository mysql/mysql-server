/* Copyright (c) 2022, 2024, Oracle and/or its affiliates.

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

#ifndef XA_SQL_CMD_XA_END
#define XA_SQL_CMD_XA_END

#include "sql/sql_cmd.h"  // Sql_cmd
#include "sql/xa.h"       // xid_t

/**
  @class Sql_cmd_xa_end

  This class represents the `XA END ...` SQL statement which puts in the IDLE
  state an XA transaction with the given xid value.

  @see Sql_cmd
*/
class Sql_cmd_xa_end : public Sql_cmd {
 public:
  /**
    Class constructor.

    @param xid_arg XID of the XA transacation about to end
    @param xa_option Additional options for the `XA END` command
   */
  Sql_cmd_xa_end(xid_t *xid_arg, enum xa_option_words xa_option);
  virtual ~Sql_cmd_xa_end() override = default;

  /**
    Retrieves the SQL command code for this class, `SQLCOM_XA_END`.

    @see Sql_cmd::sql_command_code

    @return The SQL command code for this class, `SQLCOM_XA_END`.
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
  /** Options associated with the underlying `XA END`  */
  enum xa_option_words m_xa_opt;

  /**
    Put a XA transaction in the IDLE state.

    @param thd    Current thread

    @retval false  Success
    @retval true   Failure
  */
  bool trans_xa_end(THD *thd);
};

#endif  // XA_SQL_CMD_XA_END

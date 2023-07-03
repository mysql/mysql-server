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

#ifndef XA_SQL_CMD_XA_ROLLBACK
#define XA_SQL_CMD_XA_ROLLBACK

#include "sql/sql_cmd.h"                 // Sql_cmd
#include "sql/xa.h"                      // xid_t
#include "sql/xa/sql_xa_second_phase.h"  // Sql_cmd_xa_second_phase

/**
  @class Sql_cmd_xa_rollback

  This class represents the `XA ROLLBACK ...` SQL statement which rollbacks
  and terminates an XA transaction with the given xid value.

  @see Sql_cmd
*/
class Sql_cmd_xa_rollback : public Sql_cmd_xa_second_phase {
 public:
  /**
    Class constructor.

    @param xid_arg XID of the XA transacation about to be rolled back.
   */
  explicit Sql_cmd_xa_rollback(xid_t *xid_arg);
  virtual ~Sql_cmd_xa_rollback() override = default;

  /**
    Retrieves the SQL command code for this class, `SQLCOM_XA_ROLLBACK`.

    @see Sql_cmd::sql_command_code

    @return The SQL command code for this class, `SQLCOM_XA_ROLLBACK`.
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
  /**
    Roll back and terminate a XA transaction.

    @param thd The `THD` session object within which the command is being
               executed.

    @retval false  Success
    @retval true   Failure
  */
  bool trans_xa_rollback(THD *thd);
  /**
    Handle the statement XA ROLLBACK for the case when xid corresponds to an
    XA transaction that is attached to the thread's THD object, that is a
    transaction that is part of the current thread's session context.

    @param thd The `THD` session object within which the command is being
               executed.

    @return  operation result
      @retval false  Success
      @retval true   Failure
  */
  bool process_attached_xa_rollback(THD *thd) const;
  /**
    Handle the statement XA ROLLBACK for the case when xid corresponds to an
    XA transaction that is detached from the thread's THD, that it a
    transaction that isn't part of the current's thread session context.

    @param thd The `THD` session object within which the command is being
               executed.

    @return  operation result
      @retval false  Success
      @retval true   Failure
  */
  bool process_detached_xa_rollback(THD *thd);
};

#endif  // XA_SQL_CMD_XA_ROLLBACK

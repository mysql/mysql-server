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

#ifndef XA_SQL_CMD_XA_RECOVER
#define XA_SQL_CMD_XA_RECOVER

#include "sql/sql_cmd.h"  // Sql_cmd
#include "sql/xa.h"       // xid_t

/**
  @class Sql_cmd_xa_recover

  This class represents the `XA RECOVER` SQL statement which returns to a
  client a list of XID's prepared to a XA commit/rollback.

  @see Sql_cmd
*/
class Sql_cmd_xa_recover : public Sql_cmd {
 public:
  /**
    Class constructor.

    @param print_xid_as_hex Whether or not to print the XID as hexadecimal.
   */
  explicit Sql_cmd_xa_recover(bool print_xid_as_hex);
  virtual ~Sql_cmd_xa_recover() override = default;

  /**
    Retrieves the SQL command code for this class, `SQLCOM_XA_RECOVER`.

    @see Sql_cmd::sql_command_code

    @return The SQL command code for this class, `SQLCOM_XA_RECOVER`.
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
  /** Whether or not the XID should be printed in hexadecimal form. */
  bool m_print_xid_as_hex;

  /**
    Return the list of XID's to a client, the same way SHOW commands do.

    @param thd The `THD` session object within which the command is being
               executed.

    @retval false  Success
    @retval true   Failure

    @note
      I didn't find in XA specs that an RM cannot return the same XID twice,
      so trans_xa_recover does not filter XID's to ensure uniqueness.
      It can be easily fixed later, if necessary.
  */
  bool trans_xa_recover(THD *thd);
  /**
    Check if the current user has a privilege to perform XA RECOVER.

    @param thd The `THD` session object within which the command is being
               executed.

    @retval false  A user has a privilege to perform XA RECOVER
    @retval true   A user doesn't have a privilege to perform XA RECOVER
  */
  bool check_xa_recover_privilege(THD *thd) const;
};

#endif  // XA_SQL_CMD_XA_RECOVER

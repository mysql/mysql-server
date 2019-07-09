/* Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved.

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

#ifndef SQL_CMD_DML_INCLUDED
#define SQL_CMD_DML_INCLUDED

#include "sql_cmd.h"


class Sql_cmd_dml : public Sql_cmd
{
public:
  /**
    Validate PS and do name resolution:

    * Check ACLs
    * Open tables
    * Prepare the query when needed (resolve names etc)

    @see check_prepared_statement()

    @param thd the current thread.

    @retval false on success.
    @retval true on error
  */
  virtual bool prepared_statement_test(THD *thd)= 0;

  /**
    Command-specific resolving (doesn't include LEX::prepare())

    @see select_like_stmt_test()

    @param thd  Current THD.

    @retval false on success.
    @retval true on error
  */
  virtual bool prepare(THD *thd)= 0;
};

#endif /* SQL_CMD_DML_INCLUDED */

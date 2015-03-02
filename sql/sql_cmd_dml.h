/* Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

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

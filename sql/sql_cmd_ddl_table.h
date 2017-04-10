/* Copyright (c) 2016, Oracle and/or its affiliates. All rights reserved.

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

#ifndef SQL_CMD_CREATE_TABLE_INCLUDED
#define SQL_CMD_CREATE_TABLE_INCLUDED

#include "my_sqlcommand.h"
#include "sql_cmd.h"

class THD;

class Sql_cmd_create_table : public Sql_cmd
{
public:
  virtual enum_sql_command sql_command_code() const
  {
    return SQLCOM_CREATE_TABLE;
  }

  virtual bool execute(THD *thd);
};

#endif /* SQL_CMD_CREATE_TABLE_INCLUDED */

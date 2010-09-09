#ifndef SQL_TRUNCATE_INCLUDED
#define SQL_TRUNCATE_INCLUDED
/* Copyright (c) 2010, Oracle and/or its affiliates. All rights reserved.

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

class THD;
struct TABLE_LIST;

bool mysql_truncate_table(THD *thd, TABLE_LIST *table_ref);

/**
  Sql_cmd_truncate_table represents the TRUNCATE statement.
*/
class Sql_cmd_truncate_table : public Sql_cmd
{
public:
  /**
    Constructor, used to represent a TRUNCATE statement.
  */
  Sql_cmd_truncate_table()
  {}

  ~Sql_cmd_truncate_table()
  {}

  bool execute(THD *thd);

  virtual enum_sql_command sql_command_code() const
  {
    return SQLCOM_TRUNCATE;
  }
};


#endif

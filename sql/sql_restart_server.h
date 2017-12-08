/* Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.

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

#ifndef SQL_RESTART_SERVER_H_
#define SQL_RESTART_SERVER_H_

#include "sql/sql_cmd.h"  // Sql_cmd


#ifndef _WIN32
/**
  Check if mysqld is managed by an external supervisor.

  @return true if it is under control of supervisor else false.
*/

bool is_mysqld_managed();
#endif  // _WIN32


/**
  Sql_cmd_restart_server represents the RESTART server statement.
*/

class Sql_cmd_restart_server : public Sql_cmd
{
public:
  bool execute(THD *thd) override;
  enum_sql_command sql_command_code() const override
  {
    return SQLCOM_RESTART_SERVER;
  }
};


#endif // SQL_RESTART_SERVER_H_

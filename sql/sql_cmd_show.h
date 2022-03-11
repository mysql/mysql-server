/* Copyright (c) 2005, 2022, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

#ifndef SQL_CMD_SHOW_H
#define SQL_CMD_SHOW_H

#include "my_global.h"

#include "sql_cmd.h" // Sql_cmd
#include "sql_class.h" // THD
#include "auth_common.h"            // check_global_access
#include "debug_sync.h"            // DEBUG_SYNC

/**
  Sql_cmd_show represents the SHOW statements that are implemented
  as SELECT statements internally.
  Normally, preparation and execution is the same as for regular SELECT
  statements.
*/
class Sql_cmd_show : public Sql_cmd {
 public:
  Sql_cmd_show(enum_sql_command sql_command)
      : Sql_cmd(), m_sql_command(sql_command) {}

  virtual enum_sql_command sql_command_code() const { return m_sql_command; }
  virtual bool execute(THD *thd);
  virtual bool check_privileges(THD *);
  virtual bool execute_inner(THD *thd);

 protected:
  enum_sql_command m_sql_command;
};

/// Represents SHOW PROCESSLIST statement.

class Sql_cmd_show_processlist : public Sql_cmd_show {
 public:
  Sql_cmd_show_processlist() : Sql_cmd_show(SQLCOM_SHOW_PROCESSLIST) {}
  explicit Sql_cmd_show_processlist(bool verbose)
      : Sql_cmd_show(SQLCOM_SHOW_PROCESSLIST),
        m_verbose(verbose),
        m_use_pfs(false) {}
  virtual bool check_privileges(THD *thd);
  virtual bool execute_inner(THD *thd);

  void set_use_pfs(bool use_pfs) { m_use_pfs = use_pfs; }
  bool verbose() const { return m_verbose; }

 private:
  bool use_pfs() { return m_use_pfs; }

  bool m_verbose;
  bool m_use_pfs;
};

#endif /* SQL_SHOW_H */

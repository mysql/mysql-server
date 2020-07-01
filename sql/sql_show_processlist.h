/* Copyright (c) 2015, 2020, Oracle and/or its affiliates.

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

#ifndef SQL_SHOW_PROCESSLIST_H
#define SQL_SHOW_PROCESSLIST_H

#include "sql/parse_tree_node_base.h"  // POS
#include "sql/sql_cmd.h"               // Sql_cmd

class Item;
class SELECT_LEX;
class String;
class THD;

SELECT_LEX *build_show_processlist(const POS &pos, THD *thd, bool verbose);

/**
  Implement SHOW PROCESSLIST using either the Performance Schema
  or the Information Schema.
*/
class Sql_cmd_show_processlist : public Sql_cmd {
 public:
  Sql_cmd_show_processlist(const POS &pos, THD *thd, enum_sql_command command,
                           bool verbose);
  virtual bool execute(THD *thd) override;
  virtual enum_sql_command sql_command_code() const override {
    return m_sql_command;
  }

 protected:
  bool use_pfs() { return m_use_pfs; }
  void set_use_pfs(bool use_pfs) { m_use_pfs = use_pfs; }
  bool execute_with_information_schema(THD *thd);
  bool execute_with_performance_schema(THD *thd);

 private:
  THD *m_thd;
  enum_sql_command m_sql_command;
  bool m_verbose;
  bool m_use_pfs;
};

#endif /* SQL_SHOW_PROCESSLIST_H */

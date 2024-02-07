/* Copyright (c) 2016, 2024, Oracle and/or its affiliates.

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

#ifndef SQL_CALL_INCLUDED
#define SQL_CALL_INCLUDED

#include "my_sqlcommand.h"
#include "sql/sql_cmd_dml.h"  // Sql_cmd_dml

class Item;
class THD;
class sp_name;
template <class T>
class mem_root_deque;

class Sql_cmd_call final : public Sql_cmd_dml {
 public:
  explicit Sql_cmd_call(sp_name *proc_name_arg,
                        mem_root_deque<Item *> *prog_args_arg)
      : Sql_cmd_dml(), proc_name(proc_name_arg), proc_args(prog_args_arg) {}

  enum_sql_command sql_command_code() const override { return SQLCOM_CALL; }

  bool is_data_change_stmt() const override { return false; }

 protected:
  bool precheck(THD *thd) override;
  bool check_privileges(THD *thd) override;

  bool prepare_inner(THD *thd) override;

  bool execute_inner(THD *thd) override;

 private:
  sp_name *proc_name;
  mem_root_deque<Item *> *proc_args;
};

#endif /* SQL_CALL_INCLUDED */

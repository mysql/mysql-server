/* Copyright (c) 2026, Oracle and/or its affiliates.

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

#ifndef SQL_CMD_DDL_TYPE_INCLUDED
#define SQL_CMD_DDL_TYPE_INCLUDED

#include "lex_string.h"
#include "my_sqlcommand.h"
#include "sql/sql_cmd_ddl.h"

class THD;
class Type_ident;
class PT_type;

class Sql_cmd_ddl_type : public Sql_cmd_ddl {
 public:
  Sql_cmd_ddl_type() = default;
  ~Sql_cmd_ddl_type() = default;
};

class Sql_cmd_create_type final : public Sql_cmd_ddl_type {
 public:
  Sql_cmd_create_type(Type_ident *type_ident, PT_type *type)
      : Sql_cmd_ddl_type(), m_type_ident(type_ident), m_type(type) {}

  enum_sql_command sql_command_code() const override {
    return SQLCOM_CREATE_TYPE;
  }

  bool execute(THD *thd) override;

 private:
  Type_ident *m_type_ident;
  PT_type *m_type;
};

#endif /* SQL_CMD_DDL_TYPE_INCLUDED */

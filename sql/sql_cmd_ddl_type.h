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

#include "sql/dd/dd_udt_type.h"

class THD;
class Type_ident;
class PT_type;

class Sql_cmd_ddl_type : public Sql_cmd_ddl {
 public:
  Sql_cmd_ddl_type(Type_ident *type_ident) : m_type_ident(type_ident) {}
  ~Sql_cmd_ddl_type() = default;

 protected:
  bool use_default_db(THD *thd);
  bool check_privileges(THD *thd);
  bool acquire_mdl_schema(THD *thd);
  bool acquire_mdl_type(THD *thd);
  const dd::Schema *acquire_dd_schema(THD *thd);
  const dd::UDT_Type *acquire_dd_type(THD *thd);

  Type_ident *m_type_ident;
};

class Sql_cmd_create_type final : public Sql_cmd_ddl_type {
 public:
  Sql_cmd_create_type(Type_ident *type_ident, PT_type *type)
      : Sql_cmd_ddl_type(type_ident), m_type(type) {}

  enum_sql_command sql_command_code() const override {
    return SQLCOM_CREATE_TYPE;
  }

  bool execute(THD *thd) override;

 private:
  PT_type *m_type;
};

class Sql_cmd_drop_type final : public Sql_cmd_ddl_type {
 public:
  Sql_cmd_drop_type(Type_ident *type_ident, bool if_exists)
      : Sql_cmd_ddl_type(type_ident), m_if_exists(if_exists) {}

  enum_sql_command sql_command_code() const override {
    return SQLCOM_DROP_TYPE;
  }

  bool execute(THD *thd) override;

 private:
  bool m_if_exists;
};

#endif /* SQL_CMD_DDL_TYPE_INCLUDED */

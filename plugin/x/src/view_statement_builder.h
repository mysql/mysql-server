/*
 * Copyright (c) 2016, 2023, Oracle and/or its affiliates.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is also distributed with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have included with MySQL.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */

#ifndef PLUGIN_X_SRC_VIEW_STATEMENT_BUILDER_H_
#define PLUGIN_X_SRC_VIEW_STATEMENT_BUILDER_H_

#include <string>

#include "plugin/x/src/find_statement_builder.h"
#include "plugin/x/src/statement_builder.h"

namespace xpl {

class View_statement_builder : public Statement_builder {
 public:
  typedef ::Mysqlx::Crud::DropView View_drop;
  typedef ::Mysqlx::Crud::CreateView View_create;
  typedef ::Mysqlx::Crud::ModifyView View_modify;

  explicit View_statement_builder(const Expression_generator &gen)
      : Statement_builder(gen) {}

  void build(const View_create &msg) const;
  void build(const View_modify &msg) const;
  void build(const View_drop &msg) const;

 protected:
  typedef ::Mysqlx::Crud::ViewAlgorithm Algorithm;
  typedef ::Mysqlx::Crud::ViewSqlSecurity Sql_security;
  typedef ::Mysqlx::Crud::ViewCheckOption Check_option;
  typedef ::google::protobuf::RepeatedPtrField<std::string> Column_list;
  typedef Find_statement_builder::Find Find;

  void add_definer(const std::string &definer) const;
  void add_algorithm(const Algorithm &algorithm) const;
  void add_sql_security(const Sql_security &security) const;
  void add_check_option(const Check_option &option) const;
  void add_columns(const Column_list &columns) const;
  void add_stmt(const Find &find) const;
  template <typename M>
  void build_common(const M &msg) const;
};

}  // namespace xpl

#endif  // PLUGIN_X_SRC_VIEW_STATEMENT_BUILDER_H_

/*
 * Copyright (c) 2018, 2023, Oracle and/or its affiliates.
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

#ifndef PLUGIN_X_SRC_PREPARED_STATEMENT_BUILDER_H_
#define PLUGIN_X_SRC_PREPARED_STATEMENT_BUILDER_H_

#include "plugin/x/src/expr_generator.h"
#include "plugin/x/src/ngs/error_code.h"
#include "plugin/x/src/ngs/protocol_fwd.h"
#include "plugin/x/src/query_string_builder.h"

namespace xpl {

class Prepared_statement_builder {
 public:
  using Placeholder_list = Expression_generator::Prep_stmt_placeholder_list;
  using Find = ::Mysqlx::Crud::Find;
  using Delete = ::Mysqlx::Crud::Delete;
  using Update = ::Mysqlx::Crud::Update;
  using Insert = ::Mysqlx::Crud::Insert;
  using Stmt = ::Mysqlx::Sql::StmtExecute;

  Prepared_statement_builder(Query_string_builder *qb, Placeholder_list *ph)
      : m_qb{qb}, m_placeholders{ph} {}

  ngs::Error_code build(const Find &msg) const;
  ngs::Error_code build(const Delete &msg) const;
  ngs::Error_code build(const Update &msg) const;
  ngs::Error_code build(const Insert &msg) const;
  ngs::Error_code build(const Stmt &msg) const;

 private:
  Query_string_builder *m_qb;
  Placeholder_list *m_placeholders;
};

}  // namespace xpl

#endif  // PLUGIN_X_SRC_PREPARED_STATEMENT_BUILDER_H_

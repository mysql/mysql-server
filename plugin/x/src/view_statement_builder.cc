/*
 * Copyright (c) 2015, 2024, Oracle and/or its affiliates.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is designed to work with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have either included with
 * the program or referenced in the documentation.
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

#include "plugin/x/src/view_statement_builder.h"

#include <assert.h>

#include "plugin/x/src/ngs/protocol/protocol_protobuf.h"
#include "plugin/x/src/xpl_error.h"

namespace xpl {

template <typename M>
void View_statement_builder::build_common(const M &msg) const {
  if (!msg.has_stmt())
    throw ngs::Error_code(
        ER_X_INVALID_ARGUMENT,
        "The field that defines the select statement is required");

  if (msg.has_algorithm()) add_algorithm(msg.algorithm());
  if (msg.has_definer()) add_definer(msg.definer());
  if (msg.has_security()) add_sql_security(msg.security());
  m_builder.put("VIEW ");
  add_collection(msg.collection());
  if (msg.column_size() > 0) add_columns(msg.column());
  m_builder.put(" AS ");
  add_stmt(msg.stmt());
  if (msg.has_check()) add_check_option(msg.check());
}

void View_statement_builder::build(const View_create &msg) const {
  m_builder.put("CREATE ");
  if (msg.has_replace_existing() && msg.replace_existing())
    m_builder.put("OR REPLACE ");
  build_common(msg);
}

void View_statement_builder::build(const View_modify &msg) const {
  m_builder.put("ALTER ");
  build_common(msg);
}

void View_statement_builder::build(const View_drop &msg) const {
  m_builder.put("DROP VIEW ");
  if (msg.has_if_exists() && msg.if_exists()) m_builder.put("IF EXISTS ");
  add_collection(msg.collection());
}

void View_statement_builder::add_definer(const std::string &definer) const {
  if (definer.empty()) return;

  m_builder.put("DEFINER=");
  std::string::size_type p = definer.find("@");
  if (p == std::string::npos)
    m_builder.put_quote(definer).put(" ");
  else
    m_builder.put_quote(definer.substr(0, p))
        .put("@")
        .put_quote(definer.substr(p + 1))
        .put(" ");
}

void View_statement_builder::add_algorithm(const Algorithm &algorithm) const {
  m_builder.put("ALGORITHM=");
  switch (algorithm) {
    case Mysqlx::Crud::UNDEFINED:
      m_builder.put("UNDEFINED ");
      break;

    case Mysqlx::Crud::MERGE:
      m_builder.put("MERGE ");
      break;

    case Mysqlx::Crud::TEMPTABLE:
      m_builder.put("TEMPTABLE ");
      break;

    default:
      assert("Unknown ALGORITHM type");
  }
}

void View_statement_builder::add_sql_security(
    const Sql_security &security) const {
  m_builder.put("SQL SECURITY ");
  switch (security) {
    case Mysqlx::Crud::DEFINER:
      m_builder.put("DEFINER ");
      break;

    case Mysqlx::Crud::INVOKER:
      m_builder.put("INVOKER ");
      break;

    default:
      assert("Unknown SECURITY type");
  }
}

void View_statement_builder::add_check_option(
    const Check_option &option) const {
  m_builder.put(" WITH ");

  switch (option) {
    case Mysqlx::Crud::CASCADED:
      m_builder.put("CASCADED");
      break;

    case Mysqlx::Crud::LOCAL:
      m_builder.put("LOCAL");
      break;

    default:
      assert("Unknown CHECK type");
  }
  m_builder.put(" CHECK OPTION");
}

void View_statement_builder::add_columns(const Column_list &columns) const {
  m_builder.put(" (").put_list(columns, &Generator::put_identifier).put(")");
}

void View_statement_builder::add_stmt(const Find &find) const {
  Expression_generator gen(&m_builder.m_qb, find.args(),
                           find.collection().schema(),
                           is_table_data_model(find));
  Find_statement_builder(gen).build(find);
}
}  // namespace xpl

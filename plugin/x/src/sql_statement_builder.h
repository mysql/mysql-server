/*
 * Copyright (c) 2018, 2022, Oracle and/or its affiliates.
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

#ifndef PLUGIN_X_SRC_SQL_STATEMENT_BUILDER_H_
#define PLUGIN_X_SRC_SQL_STATEMENT_BUILDER_H_

#include <string>

#include "plugin/x/src/ngs/protocol/protocol_protobuf.h"
#include "plugin/x/src/prepare_param_handler.h"

namespace xpl {
class Query_string_builder;

class Sql_statement_builder {
 public:
  static const char *const k_sql_namespace;

  using Arg_list = google::protobuf::RepeatedPtrField<::Mysqlx::Datatypes::Any>;
  using Placeholder_list = Prepare_param_handler::Placeholder_list;

  explicit Sql_statement_builder(Query_string_builder *qb) : m_qb(qb) {}

  void build(const std::string &query, const Arg_list &args) const;
  void build(const std::string &query, const Arg_list &args,
             Placeholder_list *phs) const;

 private:
  Query_string_builder *m_qb;
};

}  // namespace xpl

#endif  // PLUGIN_X_SRC_SQL_STATEMENT_BUILDER_H_

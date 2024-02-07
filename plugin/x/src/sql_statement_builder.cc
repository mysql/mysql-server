/*
 * Copyright (c) 2018, 2024, Oracle and/or its affiliates.
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

#include "plugin/x/src/sql_statement_builder.h"

#include "plugin/x/src/ngs/mysqlx/getter_any.h"
#include "plugin/x/src/query_string_builder.h"

namespace xpl {

const char *const Sql_statement_builder::k_sql_namespace = "sql";

namespace {

class Arg_inserter {
 public:
  explicit Arg_inserter(Query_string_builder *qb) : m_qb(qb) {}

  void operator()() {
    static const char *const k_value_null = "NULL";
    m_qb->format() % Query_formatter::No_escape<const char *>(k_value_null);
  }

  template <typename Value_type>
  void operator()(const Value_type &value) {
    m_qb->format() % value;
  }

  void operator()(const std::string &value, const uint32_t) {
    m_qb->format() % value;
  }

 private:
  Query_string_builder *m_qb;
};

}  // namespace

void Sql_statement_builder::build(const std::string &query,
                                  const Arg_list &args) const {
  m_qb->put(query);

  Arg_inserter inserter(m_qb);
  for (int i = 0; i < args.size(); ++i) {
    ngs::Getter_any::put_scalar_value_to_functor(args.Get(i), inserter);
  }
}

void Sql_statement_builder::build(const std::string &query,
                                  const Arg_list &args,
                                  Placeholder_list *phs) const {
  build(query, args);
  auto placeholders_count = m_qb->format().count_tags();
  for (Placeholder_info::Id i = 0; i < placeholders_count; ++i)
    phs->emplace_back(i, Placeholder_info::Type::k_raw);
}

}  // namespace xpl

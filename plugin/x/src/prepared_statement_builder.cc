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

#include "plugin/x/src/prepared_statement_builder.h"

#include "plugin/x/src/delete_statement_builder.h"
#include "plugin/x/src/find_statement_builder.h"
#include "plugin/x/src/insert_statement_builder.h"
#include "plugin/x/src/sql_statement_builder.h"
#include "plugin/x/src/update_statement_builder.h"
#include "plugin/x/src/xpl_error.h"

namespace xpl {

namespace {
template <typename B, typename M>
ngs::Error_code build_prepared_statement(
    const M &msg, Query_string_builder *qb,
    Prepared_statement_builder::Placeholder_list *phs) {
  qb->clear();
  phs->clear();
  Expression_generator gen(qb, msg.args(), msg.collection().schema(),
                           is_table_data_model(msg));
  gen.set_prep_stmt_placeholder_list(phs);
  B builder(gen);
  try {
    builder.build(msg);
  } catch (const Expression_generator::Error &exc) {
    return ngs::Error(exc.error(), "%s", exc.what());
  } catch (const ngs::Error_code &error) {
    return error;
  }
  return ngs::Success();
}

}  // namespace

ngs::Error_code Prepared_statement_builder::build(const Find &msg) const {
  return build_prepared_statement<Find_statement_builder>(msg, m_qb,
                                                          m_placeholders);
}

ngs::Error_code Prepared_statement_builder::build(const Delete &msg) const {
  return build_prepared_statement<Delete_statement_builder>(msg, m_qb,
                                                            m_placeholders);
}

ngs::Error_code Prepared_statement_builder::build(const Update &msg) const {
  return build_prepared_statement<Update_statement_builder>(msg, m_qb,
                                                            m_placeholders);
}

ngs::Error_code Prepared_statement_builder::build(const Insert &msg) const {
  return build_prepared_statement<Insert_statement_builder>(msg, m_qb,
                                                            m_placeholders);
}

ngs::Error_code Prepared_statement_builder::build(const Stmt &msg) const {
  if (msg.namespace_() != Sql_statement_builder::k_sql_namespace)
    return ngs::Error(ER_X_INVALID_NAMESPACE, "Expected namespace '%s'",
                      Sql_statement_builder::k_sql_namespace);
  m_qb->clear();
  m_placeholders->clear();
  Sql_statement_builder builder(m_qb);
  try {
    builder.build(msg.stmt(), msg.args(), m_placeholders);
  } catch (const ngs::Error_code &error) {
    return error;
  }
  return ngs::Success();
}

}  // namespace xpl

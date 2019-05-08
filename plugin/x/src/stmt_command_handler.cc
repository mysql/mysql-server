/*
 * Copyright (c) 2018, 2019, Oracle and/or its affiliates. All rights reserved.
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

#include "plugin/x/src/stmt_command_handler.h"

#include <string>

#include "plugin/x/src/admin_cmd_arguments.h"
#include "plugin/x/src/notices.h"
#include "plugin/x/src/sql_statement_builder.h"
#include "plugin/x/src/xpl_client.h"
#include "plugin/x/src/xpl_error.h"
#include "plugin/x/src/xpl_log.h"
#include "plugin/x/src/xpl_resultset.h"
#include "plugin/x/src/xpl_session.h"

namespace xpl {

ngs::Error_code Stmt_command_handler::execute(
    const Mysqlx::Sql::StmtExecute &msg) {
  log_debug("%s: %s", m_session->client().client_id(), msg.stmt().c_str());

  if (msg.namespace_() == Sql_statement_builder::k_sql_namespace ||
      !msg.has_namespace_())
    return sql_stmt_execute(msg);

  if (msg.namespace_() == "xplugin") return deprecated_admin_stmt_execute(msg);

  if (msg.namespace_() == Admin_command_handler::k_mysqlx_namespace)
    return admin_stmt_execute(msg);

  return ngs::Error(ER_X_INVALID_NAMESPACE, "Unknown namespace %s",
                    msg.namespace_().c_str());
}

ngs::Error_code Stmt_command_handler::sql_stmt_execute(
    const Mysqlx::Sql::StmtExecute &msg) {
  m_session->update_status(&ngs::Common_status_variables::m_stmt_execute_sql);

  m_qb.clear();
  Sql_statement_builder builder(&m_qb);
  try {
    builder.build(msg.stmt(), msg.args());
  } catch (const ngs::Error_code &error) {
    return error;
  }

  bool show_warnings = m_session->get_notice_configuration().is_notice_enabled(
      ngs::Notice_type::k_warning);
  auto &proto = m_session->proto();
  auto &da = m_session->data_context();
  Streaming_resultset<Stmt_command_delegate> resultset(m_session,
                                                       msg.compact_metadata());
  ngs::Error_code error =
      da.execute(m_qb.get().data(), m_qb.get().length(), &resultset);
  if (error) {
    if (show_warnings) notices::send_warnings(da, proto, true);
    return error;
  }

  return ngs::Success();
}

ngs::Error_code Stmt_command_handler::deprecated_admin_stmt_execute(
    const Mysqlx::Sql::StmtExecute &msg) {
  m_session->update_status(
      &ngs::Common_status_variables::m_stmt_execute_xplugin);
  auto &notice_config = m_session->get_notice_configuration();

  if (notice_config.is_notice_enabled(
          ngs::Notice_type::k_xplugin_deprecation)) {
    notices::send_message(
        m_session->proto(),
        "Namespace 'xplugin' is deprecated, please use 'mysqlx' instead");

    notice_config.set_notice(ngs::Notice_type::k_xplugin_deprecation, false);
  }
  Admin_command_arguments_list args(msg.args());
  return Admin_command_handler(m_session).execute(msg.namespace_(), msg.stmt(),
                                                  &args);
}

ngs::Error_code Stmt_command_handler::admin_stmt_execute(
    const Mysqlx::Sql::StmtExecute &msg) {
  m_session->update_status(
      &ngs::Common_status_variables::m_stmt_execute_mysqlx);
  Admin_command_arguments_object args(msg.args());
  return Admin_command_handler(m_session).execute(msg.namespace_(), msg.stmt(),
                                                  &args);
}

}  // namespace xpl

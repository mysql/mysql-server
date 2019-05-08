/*
 * Copyright (c) 2015, 2018, Oracle and/or its affiliates. All rights reserved.
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

#include "plugin/x/src/xpl_session.h"

#include "plugin/x/ngs/include/ngs/interface/client_interface.h"
#include "plugin/x/ngs/include/ngs/ngs_error.h"
#include "plugin/x/ngs/include/ngs/protocol/protocol_protobuf.h"
#include "plugin/x/ngs/include/ngs/scheduler.h"
#include "plugin/x/src/document_id_aggregator.h"
#include "plugin/x/src/notices.h"
#include "plugin/x/src/sql_data_context.h"
#include "plugin/x/src/xpl_dispatcher.h"
#include "plugin/x/src/xpl_log.h"
#include "plugin/x/src/xpl_server.h"

namespace xpl {

Session::Session(ngs::Client_interface *client,
                 ngs::Protocol_encoder_interface *proto,
                 const Session_id session_id)
    : ngs::Session(client, proto, session_id),
      m_sql(proto),
      m_notice_output_queue(proto, &m_notice_configuration),
      m_was_authenticated(false),
      m_document_id_aggregator(&client->server().get_document_id_generator()) {}

Session::~Session() {
  if (m_was_authenticated)
    --Global_status_variables::instance().m_sessions_count;

  if (m_failed_auth_count > 0 && !m_was_authenticated)
    ++Global_status_variables::instance().m_rejected_sessions_count;

  m_sql.deinit();
}

// handle a message while in Ready state
bool Session::handle_ready_message(ngs::Message_request &command) {
  // check if the session got killed
  if (m_sql.is_killed()) {
    m_encoder->send_result(ngs::Error_code(ER_QUERY_INTERRUPTED,
                                           "Query execution was interrupted",
                                           "70100", ngs::Error_code::FATAL));
    // close as fatal_error instead of killed. killed is for when the client is
    // idle
    on_close();
    return true;
  }

  if (ngs::Session::handle_ready_message(command)) return true;

  try {
    return m_dispatcher.execute(command);
  } catch (ngs::Error_code &err) {
    m_encoder->send_result(err);
    on_close();
    return true;
  } catch (std::exception &exc) {
    // not supposed to happen, but catch exceptions as a last defense..
    log_error(ER_XPLUGIN_UNEXPECTED_EXCEPTION_DISPATCHING_CMD,
              m_client->client_id(), exc.what());
    on_close();
    return true;
  }
  return false;
}

ngs::Error_code Session::init() {
  const unsigned short port = m_client->client_port();
  const Connection_type type = m_client->connection().get_type();

  return m_sql.init(port, type);
}

void Session::on_kill() {
  if (!m_sql.is_killed()) {
    if (!m_sql.kill())
      log_debug("%s: Could not interrupt client session",
                m_client->client_id());
  }

  on_close(true);
}

void Session::on_auth_success(
    const ngs::Authentication_interface::Response &response) {
  notices::send_client_id(proto(), m_client->client_id_num());
  ngs::Session::on_auth_success(response);

  ++Global_status_variables::instance().m_accepted_sessions_count;
  ++Global_status_variables::instance().m_sessions_count;

  m_was_authenticated = true;
}

void Session::on_auth_failure(
    const ngs::Authentication_interface::Response &response) {
  if (response.error_code == ER_MUST_CHANGE_PASSWORD &&
      !m_sql.password_expired()) {
    ngs::Authentication_interface::Response r{
        response.status, response.error_code,
        "Password for " MYSQLXSYS_ACCOUNT " account has been expired"};
    ngs::Session::on_auth_failure(r);
  } else {
    ngs::Session::on_auth_failure(response);
  }
}

void Session::on_reset() {
  ngs::Error_code error = m_sql.reset();
  if (error) {
    m_encoder->send_result(error);
    return;
  }
  m_dispatcher.reset();
  m_encoder->send_ok();
}

void Session::mark_as_tls_session() {
  data_context().set_connection_type(Connection_tls);
}

THD *Session::get_thd() const { return m_sql.get_thd(); }

/** Checks whether things owned by the given user are visible to this session.
 Returns true if we're SUPER or the same user as the given one.
 If user is NULL, then it's only visible for SUPER users.
 */
bool Session::can_see_user(const std::string &user) const {
  const std::string owner = m_sql.get_authenticated_user_name();

  if (state() == ngs::Session_interface::k_ready && !owner.empty()) {
    if (m_sql.has_authenticated_user_a_super_priv() || (owner == user))
      return true;
  }
  return false;
}

void Session::update_status(ngs::Common_status_variables::Variable
                                ngs::Common_status_variables::*variable) {
  ++(m_status_variables.*variable);
  ++(Global_status_variables::instance().*variable);
}

bool Session::get_prepared_statement_id(const uint32_t client_stmt_id,
                                        uint32_t *out_server_stmt_id) const {
  const auto &stmt_info = m_dispatcher.get_prepared_stmt_info();
  const auto i = stmt_info.find(client_stmt_id);
  if (i == stmt_info.end()) return false;
  *out_server_stmt_id = i->second.m_server_stmt_id;
  return true;
}

}  // namespace xpl

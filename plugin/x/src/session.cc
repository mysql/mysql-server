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

#include "plugin/x/src/session.h"

#include <stddef.h>
#include <sys/types.h>

#include <set>
#include <string>

#include "include/my_sys.h"
#include "plugin/x/src/document_id_aggregator.h"
#include "plugin/x/src/interface/authentication.h"
#include "plugin/x/src/interface/client.h"
#include "plugin/x/src/interface/protocol_encoder.h"
#include "plugin/x/src/interface/protocol_monitor.h"
#include "plugin/x/src/interface/server.h"
#include "plugin/x/src/ngs/log.h"
#include "plugin/x/src/ngs/protocol/protocol_protobuf.h"
#include "plugin/x/src/ngs/scheduler.h"
#include "plugin/x/src/notices.h"
#include "plugin/x/src/sql_data_context.h"
#include "plugin/x/src/xpl_dispatcher.h"
#include "plugin/x/src/xpl_error.h"
#include "plugin/x/src/xpl_log.h"

// #undef ERROR  // Needed to avoid conflict with ERROR in mysqlx.pb.h
// #include "plugin/x/src/ngs/protocol/protocol_protobuf.h"

namespace xpl {

// Code below this line is executed from the network thread
// ----------------------------------------------------------------------------

Session::Session(iface::Client *client, iface::Protocol_encoder *proto,
                 const Session_id session_id)
    : m_client(client),  // don't hold a real reference to the parent to avoid
                         // circular reference
      m_encoder(proto),
      m_auth_handler(),
      m_state(State::k_authenticating),
      m_state_before_close(State::k_authenticating),
      m_id(session_id),
      m_thread_pending(0),
      m_thread_active(0),
      m_notice_output_queue(proto, &m_notice_configuration),
      m_was_authenticated(false),
      m_document_id_aggregator(&client->server().get_document_id_generator()) {
  log_debug("%s.%i: New session allocated by client", client->client_id(),
            session_id);

#ifndef WIN32
  mdbg_my_thread = pthread_self();
#endif
}

Session::~Session() {
  log_debug("%s: Delete session", m_client->client_id());
#ifndef WIN32
  assert(mdbg_my_thread == pthread_self());
#endif
  m_sql.deinit();
  DBUG_LOG("debug", "~Session(m_was_authenticated:"
                        << m_was_authenticated << ", m_failed_auth_count: "
                        << static_cast<int>(m_failed_auth_count)
                        << ", m_state_before_close: "
                        << static_cast<int>(state_before_close()));

  if (m_was_authenticated)
    --Global_status_variables::instance().m_sessions_count;

  if (m_failed_auth_count > 0 && !m_was_authenticated)
    ++Global_status_variables::instance().m_rejected_sessions_count;

  if (state_before_close() != iface::Session::State::k_authenticating) {
    ++Global_status_variables::instance().m_closed_sessions_count;
  }
}

void Session::on_close(const Session::Close_flags flags) {
  if (m_state != State::k_closing) {
    if (flags & Close_flags::k_update_old_state) m_state_before_close = m_state;
    m_state = State::k_closing;
    if (flags & Close_flags::k_force_close_client)
      m_client->on_session_close(this);
  }
}

void Session::set_proto(iface::Protocol_encoder *encoder) {
  m_encoder = encoder;
  m_notice_output_queue.set_encoder(encoder);
}

// Code below this line is executed from the worker thread
// ----------------------------------------------------------------------------

// Return value means true if message was handled, false if not.
// If message is handled, ownership of the object is passed on (and should be
// deleted by the callee)
bool Session::handle_message(const ngs::Message_request &command) {
  if (m_state == State::k_authenticating) {
    return handle_auth_message(command);
  } else if (m_state == State::k_ready) {
    // handle session commands
    return handle_ready_message(command);
  }
  // msg not handled
  return false;
}

void Session::stop_auth() {
  m_auth_handler.reset();

  // request termination
  m_client->on_session_close(this);
}

bool Session::handle_auth_message(const ngs::Message_request &command) {
  iface::Authentication::Response r;
  int8_t type = command.get_message_type();

  if (type == Mysqlx::ClientMessages::SESS_AUTHENTICATE_START &&
      m_auth_handler.get() == nullptr) {
    const Mysqlx::Session::AuthenticateStart &authm =
        static_cast<const Mysqlx::Session::AuthenticateStart &>(
            *command.get_message());

    log_debug("%s.%u: Login attempt: mechanism=%s auth_data=%s",
              m_client->client_id(), m_id, authm.mech_name().c_str(),
              authm.auth_data().c_str());

    m_auth_handler = m_client->server().get_authentications().get_auth_handler(
        authm.mech_name(), this);
    if (!m_auth_handler.get()) {
      log_debug("%s.%u: Invalid authentication method %s",
                m_client->client_id(), m_id, authm.mech_name().c_str());
      m_encoder->send_error(ngs::Fatal(ER_NOT_SUPPORTED_AUTH_MODE,
                                       "Invalid authentication method %s",
                                       authm.mech_name().c_str()),
                            true);
      stop_auth();
      return true;
    } else {
      r = m_auth_handler->handle_start(authm.mech_name(), authm.auth_data(),
                                       authm.initial_response());
    }
  } else if (type == Mysqlx::ClientMessages::SESS_AUTHENTICATE_CONTINUE &&
             m_auth_handler.get()) {
    const Mysqlx::Session::AuthenticateContinue &authm =
        static_cast<const Mysqlx::Session::AuthenticateContinue &>(
            *command.get_message());

    r = m_auth_handler->handle_continue(authm.auth_data());
  } else {
    m_encoder->get_protocol_monitor().on_error_unknown_msg_type();
    log_debug(
        "%s: Unexpected message of type %i received during authentication",
        m_client->client_id(), type);
    m_encoder->send_error(ngs::Fatal(ER_X_BAD_MESSAGE, "Invalid message"),
                          true);
    stop_auth();
    return false;
  }

  switch (r.status) {
    case iface::Authentication::Status::k_succeeded:
      on_auth_success(r);
      break;

    case iface::Authentication::Status::k_failed:
      on_auth_failure(r);
      break;

    default:
      m_encoder->send_auth_continue(r.data);
  }

  return true;
}

void Session::on_auth_success(const iface::Authentication::Response &response) {
  proto().send_notice_client_id(m_client->client_id_num());

  log_debug("%s.%u: Login succeeded", m_client->client_id(), m_id);
  m_auth_handler.reset();
  m_state = State::k_ready;
  m_client->on_session_auth_success(this);
  m_encoder->send_auth_ok(response.data);  // send it last, so that
                                           // on_auth_success() can send session
                                           // specific notices
  m_failed_auth_count = 0;

  ++Global_status_variables::instance().m_accepted_sessions_count;
  ++Global_status_variables::instance().m_sessions_count;

  m_was_authenticated = true;
}

void Session::on_auth_failure_impl(
    const iface::Authentication::Response &response) {
  log_debug("%s.%u: Unsuccessful authentication attempt", m_client->client_id(),
            m_id);
  m_failed_auth_count++;

  ngs::Error_code error_send_back_to_user =
      get_authentication_access_denied_error();

  if (can_forward_error_code_to_client(response.error_code)) {
    error_send_back_to_user =
        ngs::Error(response.error_code, "%s", response.data.c_str());
  }

  error_send_back_to_user.severity = can_authenticate_again()
                                         ? ngs::Error_code::ERROR
                                         : ngs::Error_code::FATAL;

  m_encoder->send_error(error_send_back_to_user, true);

  // It is possible to use different auth methods therefore we should not
  // stop authentication in such case.
  if (!can_authenticate_again()) {
    log_info(ER_XPLUGIN_MAX_AUTH_ATTEMPTS_REACHED, m_client->client_id(), m_id);
    stop_auth();
  }

  m_auth_handler.reset();
}

void Session::on_auth_failure(const iface::Authentication::Response &response) {
  if (response.error_code == ER_MUST_CHANGE_PASSWORD &&
      !m_sql.password_expired()) {
    iface::Authentication::Response r{response.status, response.error_code,
                                      "Password for " MYSQLXSYS_ACCOUNT
                                      " account has been expired"};
    on_auth_failure_impl(r);
  } else {
    on_auth_failure_impl(response);
  }
}

ngs::Error_code Session::get_authentication_access_denied_error() const {
  const auto authentication_info = m_auth_handler->get_authentication_info();
  const char *is_using_password = authentication_info.m_was_using_password
                                      ? my_get_err_msg(ER_YES)
                                      : my_get_err_msg(ER_NO);
  std::string username = authentication_info.m_tried_account_name;
  std::string hostname = client().client_hostname_or_address();
  auto result = ngs::SQLError(ER_ACCESS_DENIED_ERROR, username.c_str(),
                              hostname.c_str(), is_using_password);

  if (can_authenticate_again())
    log_debug("Try to authenticate again, got: %s", result.message.c_str());
  return result;
}

bool Session::can_forward_error_code_to_client(const int error_code) {
  // Lets ignore ER_ACCESS_DENIED_ERROR it is used by the plugin to
  // return general authentication problem. It may have not too
  // accurate error message.
  static const std::set<int> allowed_error_codes{
      ER_DBACCESS_DENIED_ERROR,    ER_MUST_CHANGE_PASSWORD_LOGIN,
      ER_ACCOUNT_HAS_BEEN_LOCKED,  ER_SECURE_TRANSPORT_REQUIRED,
      ER_SERVER_OFFLINE_MODE,      ER_SERVER_OFFLINE_MODE_REASON,
      ER_SERVER_OFFLINE_MODE_USER, ER_BAD_DB_ERROR,
      ER_AUDIT_API_ABORT};

  return 0 < allowed_error_codes.count(error_code);
}

bool Session::can_authenticate_again() const {
  return m_failed_auth_count < k_max_auth_attempts;
}

// handle a message while in Ready state
bool Session::handle_ready_message(const ngs::Message_request &command) {
  // check if the session got killed
  if (m_sql.is_killed()) {
    m_encoder->send_result(ngs::Error_code(ER_QUERY_INTERRUPTED,
                                           "Query execution was interrupted",
                                           "70100", ngs::Error_code::FATAL));
    // close as fatal_error instead of killed. killed is for when the client
    // is idle
    on_close();
    return true;
  }

  switch (command.get_message_type()) {
    case Mysqlx::ClientMessages::SESS_CLOSE:
      m_state = State::k_closing;
      m_client->on_session_reset(this);
      return true;

    case Mysqlx::ClientMessages::CON_CLOSE:
      m_encoder->send_ok("bye!");
      on_close(Close_flags::k_force_close_client |
               Close_flags::k_update_old_state);
      return true;

    case Mysqlx::ClientMessages::SESS_RESET: {
      const auto &msg =
          static_cast<const Mysqlx::Session::Reset &>(*command.get_message());
      if (msg.has_keep_open() && msg.keep_open()) {
        on_reset();
        return true;
      }
      m_state = State::k_closing;
      m_client->on_session_reset(this);
      return true;
    }
  }

  try {
    const auto error = m_dispatcher.execute(command);
    switch (error.severity) {
      case ngs::Error_code::OK:
        return true;
      case ngs::Error_code::ERROR:
        return error.error != ER_UNKNOWN_COM_ERROR;
      case ngs::Error_code::FATAL:
        on_close();
        return true;
    }
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
  DBUG_TRACE;
  if (!m_sql.is_killed()) {
    if (!m_sql.kill())
      log_debug("%s: Could not interrupt client session",
                m_client->client_id());
  }
  on_close(Close_flags::k_update_old_state);
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

/** Checks whether things owned by the given user are visible to this session.
 Returns true if we're SUPER or the same user as the given one.
 If user is NULL, then it's only visible for SUPER users.
 */
bool Session::can_see_user(const std::string &user) const {
  const std::string owner = m_sql.get_authenticated_user_name();

  if (state() == iface::Session::State::k_ready && !owner.empty()) {
    if (m_sql.has_authenticated_user_a_super_priv() || (owner == user))
      return true;
  }
  return false;
}

void Session::update_status(Common_status_variable variable) {
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

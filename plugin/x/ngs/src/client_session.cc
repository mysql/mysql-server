/*
 * Copyright (c) 2015, 2019, Oracle and/or its affiliates. All rights reserved.
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

#include "plugin/x/ngs/include/ngs/client_session.h"

#include <stddef.h>
#include <sys/types.h>
#include "include/my_sys.h"

#include "plugin/x/ngs/include/ngs/interface/authentication_interface.h"
#include "plugin/x/ngs/include/ngs/interface/client_interface.h"
#include "plugin/x/ngs/include/ngs/interface/protocol_encoder_interface.h"
#include "plugin/x/ngs/include/ngs/interface/protocol_monitor_interface.h"
#include "plugin/x/ngs/include/ngs/interface/server_interface.h"
#include "plugin/x/ngs/include/ngs/log.h"
#include "plugin/x/src/xpl_error.h"

#undef ERROR  // Needed to avoid conflict with ERROR in mysqlx.pb.h
#include "plugin/x/ngs/include/ngs/protocol/protocol_protobuf.h"

namespace ngs {

// Code below this line is executed from the network thread
// ------------------------------------------------------------------------------------------------

Session::Session(Client_interface *client, Protocol_encoder_interface *proto,
                 const Session_id session_id)
    : m_client(client),  // don't hold a real reference to the parent to avoid
                         // circular reference
      m_encoder(proto),
      m_auth_handler(),
      m_state(k_authenticating),
      m_state_before_close(k_authenticating),
      m_id(session_id),
      m_thread_pending(0),
      m_thread_active(0) {
  log_debug("%s.%i: New session allocated by client", client->client_id(),
            session_id);

#ifndef WIN32
  mdbg_my_thread = pthread_self();
#endif
}

Session::~Session() {
  log_debug("%s: Delete session", m_client->client_id());
  check_thread();
}

void Session::on_close(const bool update_old_state) {
  if (m_state != k_closing) {
    if (update_old_state) m_state_before_close = m_state;
    m_state = k_closing;
    m_client->on_session_close(*this);
  }
}

void Session::set_proto(Protocol_encoder_interface *encode) {
  m_encoder = encode;
}

// Code below this line is executed from the worker thread
// ----------------------------------------------------------------------------

// Return value means true if message was handled, false if not.
// If message is handled, ownership of the object is passed on (and should be
// deleted by the callee)
bool Session::handle_message(ngs::Message_request &command) {
  if (m_state == k_authenticating) {
    return handle_auth_message(command);
  } else if (m_state == k_ready) {
    // handle session commands
    return handle_ready_message(command);
  }
  // msg not handled
  return false;
}

bool Session::handle_ready_message(Message_request &command) {
  switch (command.get_message_type()) {
    case Mysqlx::ClientMessages::SESS_CLOSE:
      m_state = k_closing;
      m_client->on_session_reset(*this);
      return true;

    case Mysqlx::ClientMessages::CON_CLOSE:
      m_encoder->send_ok("bye!");
      on_close(true);
      return true;

    case Mysqlx::ClientMessages::SESS_RESET: {
      const auto &msg =
          static_cast<const Mysqlx::Session::Reset &>(*command.get_message());
      if (msg.has_keep_open() && msg.keep_open()) {
        on_reset();
        return true;
      }
      m_state = k_closing;
      m_client->on_session_reset(*this);
      return true;
    }
  }
  return false;
}

void Session::stop_auth() {
  m_auth_handler.reset();

  // request termination
  m_client->on_session_close(*this);
}

bool Session::handle_auth_message(Message_request &command) {
  Authentication_interface::Response r;
  int8_t type = command.get_message_type();

  if (type == Mysqlx::ClientMessages::SESS_AUTHENTICATE_START &&
      m_auth_handler.get() == NULL) {
    const Mysqlx::Session::AuthenticateStart &authm =
        static_cast<const Mysqlx::Session::AuthenticateStart &>(
            *command.get_message());

    log_debug("%s.%u: Login attempt: mechanism=%s auth_data=%s",
              m_client->client_id(), m_id, authm.mech_name().c_str(),
              authm.auth_data().c_str());

    m_auth_handler =
        m_client->server().get_auth_handler(authm.mech_name(), this);
    if (!m_auth_handler.get()) {
      log_debug("%s.%u: Invalid authentication method %s",
                m_client->client_id(), m_id, authm.mech_name().c_str());
      m_encoder->send_error(
          Fatal(ER_NOT_SUPPORTED_AUTH_MODE, "Invalid authentication method %s",
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
    m_encoder->send_error(Fatal(ER_X_BAD_MESSAGE, "Invalid message"), true);
    stop_auth();
    return false;
  }

  switch (r.status) {
    case Authentication_interface::Succeeded:
      on_auth_success(r);
      break;

    case Authentication_interface::Failed:
      on_auth_failure(r);
      break;

    default:
      m_encoder->send_auth_continue(r.data);
  }

  return true;
}

void Session::on_auth_success(
    const Authentication_interface::Response &response) {
  log_debug("%s.%u: Login succeeded", m_client->client_id(), m_id);
  m_auth_handler.reset();
  m_state = k_ready;
  m_client->on_session_auth_success(*this);
  m_encoder->send_auth_ok(response.data);  // send it last, so that
                                           // on_auth_success() can send session
                                           // specific notices
  m_failed_auth_count = 0;
}

void Session::on_auth_failure(
    const Authentication_interface::Response &response) {
  log_debug("%s.%u: Unsuccessful authentication attempt", m_client->client_id(),
            m_id);
  m_failed_auth_count++;

  Error_code error_send_back_to_user = get_authentication_access_denied_error();

  if (can_forward_error_code_to_client(response.error_code)) {
    error_send_back_to_user =
        Error(response.error_code, "%s", response.data.c_str());
  }

  error_send_back_to_user.severity =
      can_authenticate_again() ? Error_code::ERROR : Error_code::FATAL;

  m_encoder->send_error(error_send_back_to_user, true);

  // It is possible to use different auth methods therefore we should not
  // stop authentication in such case.
  if (!can_authenticate_again()) {
    log_info(ER_XPLUGIN_MAX_AUTH_ATTEMPTS_REACHED, m_client->client_id(), m_id);
    stop_auth();
  }

  m_auth_handler.reset();
}

Error_code Session::get_authentication_access_denied_error() const {
  const auto authentication_info = m_auth_handler->get_authentication_info();
  const char *is_using_password = authentication_info.m_was_using_password
                                      ? my_get_err_msg(ER_YES)
                                      : my_get_err_msg(ER_NO);
  std::string username = authentication_info.m_tried_account_name;
  std::string hostname = client().client_hostname_or_address();
  auto result = SQLError(ER_ACCESS_DENIED_ERROR, username.c_str(),
                         hostname.c_str(), is_using_password);

  if (can_authenticate_again())
    log_debug("Try to authenticate again, got: %s", result.message.c_str());
  return result;
}

bool Session::can_forward_error_code_to_client(const int error_code) {
  // Lets ignore ER_ACCESS_DENIED_ERROR it is used by the plugin to
  // return general authentication problem. It may have not too
  // accurate error message.
  const static std::set<int> allowed_error_codes{
      ER_DBACCESS_DENIED_ERROR,   ER_MUST_CHANGE_PASSWORD_LOGIN,
      ER_ACCOUNT_HAS_BEEN_LOCKED, ER_SECURE_TRANSPORT_REQUIRED,
      ER_SERVER_OFFLINE_MODE,     ER_BAD_DB_ERROR};

  return 0 < allowed_error_codes.count(error_code);
}

bool Session::can_authenticate_again() const {
  return m_failed_auth_count < k_max_auth_attempts;
}
}  // namespace ngs

/*
 * Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.
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

#include "plugin/x/tests/driver/connector/connection_manager.h"

#include <iostream>
#include <sstream>
#include <utility>
#include <vector>

namespace details {

::Mysqlx::Connection::CapabilitiesSet setup_capability_set_from_bool(
    const std::string &name, const bool value) {
  ::Mysqlx::Connection::CapabilitiesSet capability_set;
  auto capability =
      capability_set.mutable_capabilities()->mutable_capabilities()->Add();

  capability->set_name(name);
  capability->mutable_value()->set_type(::Mysqlx::Datatypes::Any_Type_SCALAR);
  auto scalar = capability->mutable_value()->mutable_scalar();

  scalar->set_type(::Mysqlx::Datatypes::Scalar_Type_V_BOOL);
  scalar->set_v_bool(value);

  return capability_set;
}

std::string get_ip_mode_to_text(const xcl::Internet_protocol ip) {
  switch (ip) {
    case xcl::Internet_protocol::V4:
      return "IP4";

    case xcl::Internet_protocol::V6:
      return "IP6";

    case xcl::Internet_protocol::Any:
    default:
      return "ANY";
  }
}

}  // namespace details

Connection_manager::Connection_manager(const Connection_options &co,
                                       Variable_container *variables,
                                       const Console &console)
    : m_connection_options(co), m_variables(variables), m_console(console) {
  m_variables->make_special_variable(
      "%OPTION_CLIENT_USER%",
      new Variable_dynamic_string(m_connection_options.user));

  m_variables->make_special_variable(
      "%OPTION_CLIENT_PASSWORD%",
      new Variable_dynamic_string(m_connection_options.password));

  m_variables->make_special_variable(
      "%OPTION_CLIENT_HOST%",
      new Variable_dynamic_string(m_connection_options.host));

  m_variables->make_special_variable(
      "%OPTION_CLIENT_SOCKET%",
      new Variable_dynamic_string(m_connection_options.socket));

  m_variables->make_special_variable(
      "%OPTION_CLIENT_SCHEMA%",
      new Variable_dynamic_string(m_connection_options.schema));

  m_variables->make_special_variable(
      "%OPTION_CLIENT_PORT%",
      new Variable_dynamic_int(m_connection_options.port));

  m_variables->make_special_variable(
      "%OPTION_SSL_MODE%",
      new Variable_dynamic_string(m_connection_options.ssl_mode));

  m_active_holder.reset(new Session_holder(xcl::create_session(), m_console));

  m_session_holders[""] = m_active_holder;
}

Connection_manager::~Connection_manager() {
  std::vector<std::string> disconnect_following;

  for (const auto &connection : m_session_holders) {
    if (connection.first != "") {
      disconnect_following.push_back(connection.first);
    }
  }

  for (auto &name : disconnect_following) {
    safe_close(name);
  }

  if (m_session_holders.count("") > 0) {
    safe_close("");
  }
}

void Connection_manager::get_credentials(std::string *ret_user,
                                         std::string *ret_pass) {
  assert(ret_user);
  assert(ret_pass);

  *ret_user = m_connection_options.user;
  *ret_pass = m_connection_options.password;
}

void Connection_manager::safe_close(const std::string &name) {
  try {
    set_active(name, true);
    close_active(true, true);
  } catch (const std::exception &) {
  } catch (const xcl::XError &) {
  }
}

void Connection_manager::connect_default(
    const bool send_cap_password_expired, const bool client_interactive,
    const bool no_auth, const std::vector<std::string> &auth_methods) {
  m_console.print_verbose("Connecting...\n");

  auto session = m_active_holder->get_session();
  auto text_ip_mode =
      details::get_ip_mode_to_text(m_connection_options.ip_mode);

  m_active_holder->setup_ssl(m_connection_options);
  m_active_holder->setup_msg_callbacks(m_connection_options.trace_protocol);

  if (send_cap_password_expired) {
    session->set_capability(
        xcl::XSession::Capability_can_handle_expired_password, true);
  }

  if (client_interactive) {
    session->set_capability(xcl::XSession::Capability_client_interactive, true);
  }

  session->set_mysql_option(xcl::XSession::Mysqlx_option::Authentication_method,
                            auth_methods);

  if (m_connection_options.compatible) {
    session->set_mysql_option(
        xcl::XSession::Mysqlx_option::Authentication_method, "FALLBACK");
  }

  session->set_mysql_option(xcl::XSession::Mysqlx_option::Hostname_resolve_to,
                            text_ip_mode);

  xcl::XError error;

  if (no_auth) {
    error = m_active_holder->setup_connection(m_connection_options);
  } else {
    error = m_active_holder->setup_session(m_connection_options);
  }

  if (error) {
    // In case of configuration error, lets do safe_close to synchronize
    // closing of socket with exist of mysqlxtest (in other case mysqlxtest
    // is going to exit and after a while the connection is going to be
    // accepted on server side).
    if (CR_X_TLS_WRONG_CONFIGURATION != error.error() || no_auth) {
      session->get_protocol().get_connection().close();
    }

    throw error;
  }

  setup_variables(session);

  m_console.print_verbose("Connected client #", session->client_id(), "\n");
}

void Connection_manager::create(const std::string &name,
                                const std::string &user,
                                const std::string &password,
                                const std::string &db,
                                const std::vector<std::string> &auth_methods) {
  if (m_session_holders.count(name))
    throw std::runtime_error("a session named " + name + " already exists");

  m_console.print("connecting...\n");

  Connection_options co = m_connection_options;

  if (!user.empty()) {
    co.user = user;
    co.password = password;
  }

  if (!db.empty()) {
    co.schema = db;
  }

  auto session = xcl::create_session();
  std::shared_ptr<Session_holder> holder{
      new Session_holder(std::move(session), m_console)};

  holder->get_session()->set_mysql_option(
      xcl::XSession::Mysqlx_option::Hostname_resolve_to,
      details::get_ip_mode_to_text(m_connection_options.ip_mode));

  if (!auth_methods.empty())
    holder->get_session()->set_mysql_option(
        xcl::XSession::Mysqlx_option::Authentication_method, auth_methods);

  if (m_connection_options.compatible) {
    holder->get_session()->set_mysql_option(
        xcl::XSession::Mysqlx_option::Authentication_method, "FALLBACK");
  }

  holder->setup_ssl(co);
  holder->setup_msg_callbacks(co.trace_protocol);

  xcl::XError error;

  if (user == "-") {
    error = holder->setup_connection(co);
  } else {
    error = holder->setup_session(co);
  }

  if (error) {
    throw error;
  }

  m_active_holder = holder;
  m_session_holders[name] = holder;
  m_active_session_name = name;

  setup_variables(active_xsession());

  m_console.print("active session is now '", name, "'\n");

  m_console.print_verbose("Connected client #", active_xsession()->client_id(),
                          "\n");
}

void Connection_manager::abort_active() {
  if (m_active_holder) {
    if (!m_active_session_name.empty())
      std::cout << "aborting session " << m_active_session_name << "\n";
    /* Close connection first, to stop XSession from executing
     Disconnection flow */
    active_xconnection()->close();
    m_active_holder.reset();
    m_session_holders.erase(m_active_session_name);
    if (m_active_session_name != "") set_active("");
  } else {
    throw std::runtime_error("no active session");
  }
}

bool Connection_manager::is_default_active() {
  return m_active_session_name.empty();
}

void Connection_manager::close_active(const bool shutdown,
                                      const bool be_quiet) {
  if (m_active_holder) {
    if (m_active_session_name.empty() && !shutdown)
      throw std::runtime_error("cannot close default session");
    try {
      if (!m_active_session_name.empty() && !be_quiet)
        m_console.print("closing session ", m_active_session_name, "\n");

      if (active_xconnection()->state().is_connected()) {
        // send a close message and wait for the corresponding Ok message
        active_xprotocol()->send(Mysqlx::Connection::Close());
        xcl::XProtocol::Server_message_type_id msgid;
        xcl::XError error;
        Message_ptr msg{
            active_xprotocol()->recv_single_message(&msgid, &error)};

        if (error) throw error;

        if (!be_quiet) m_console.print(*msg);
        if (Mysqlx::ServerMessages::OK != msgid)
          throw xcl::XError(CR_COMMANDS_OUT_OF_SYNC,
                            "Disconnect was expecting Mysqlx.Ok(bye!), but "
                            "got the one above (one or more calls to -->recv "
                            "are probably missing)");

        std::string text = static_cast<Mysqlx::Ok *>(msg.get())->msg();
        if (text != "bye!" && text != "tchau!")
          throw xcl::XError(CR_COMMANDS_OUT_OF_SYNC,
                            "Disconnect was expecting Mysqlx.Ok(bye!), but "
                            "got the one above (one or more calls to -->recv "
                            "are probably missing)");

        if (!m_connection_options.dont_wait_for_disconnect) {
          Message_ptr msg{
              active_xprotocol()->recv_single_message(&msgid, &error)};

          if (!error && !be_quiet) {
            m_console.print_error("Was expecting closure but got message:",
                                  *msg);
          }
        }

        active_xconnection()->close();
      }
      m_session_holders.erase(m_active_session_name);
      if (!shutdown) set_active("", be_quiet);
    } catch (const std::exception &error) {
      active_xconnection()->close();
      m_session_holders.erase(m_active_session_name);
      if (!shutdown) set_active("", be_quiet);
      throw error;
    } catch (const xcl::XError &error) {
      active_xconnection()->close();
      m_session_holders.erase(m_active_session_name);
      if (!shutdown) set_active("", be_quiet);
      throw error;
    }
  } else {
    if (!shutdown) throw std::runtime_error("no active session");
  }
}

void Connection_manager::set_active(const std::string &name,
                                    const bool be_quiet) {
  if (m_session_holders.count(name) == 0) {
    std::string slist;
    for (auto it = m_session_holders.begin(); it != m_session_holders.end();
         ++it)
      slist.append(it->first).append(", ");
    if (!slist.empty()) slist.resize(slist.length() - 2);
    throw std::runtime_error("no session named '" + name + "': " + slist);
  }
  m_active_holder = m_session_holders[name];
  m_active_session_name = name;

  setup_variables(active_xsession());

  if (!be_quiet)
    m_console.print(
        "switched to session ",
        (m_active_session_name.empty() ? "default" : m_active_session_name),
        "\n");
}

Session_holder &Connection_manager::active_holder() {
  if (!m_active_holder) std::runtime_error("no active session");

  return *m_active_holder;
}

xcl::XSession *Connection_manager::active_xsession() {
  if (!m_active_holder) std::runtime_error("no active session");
  return m_active_holder->get_session();
}

xcl::XProtocol *Connection_manager::active_xprotocol() {
  return &active_xsession()->get_protocol();
}

xcl::XConnection *Connection_manager::active_xconnection() {
  return &active_xprotocol()->get_connection();
}

uint64_t Connection_manager::active_session_messages_received(
    const std::string &message_name) const {
  uint64_t result = 0;
  m_active_holder->try_get_number_of_received_messages(message_name, &result);

  return result;
}

void Connection_manager::setup_variables(xcl::XSession *session) {
  auto &connection = session->get_protocol().get_connection();
  m_variables->set("%ACTIVE_CLIENT_ID%", std::to_string(session->client_id()));

  m_variables->set("%ACTIVE_SOCKET_ID%",
                   std::to_string(connection.get_socket_fd()));
}

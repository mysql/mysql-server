/*
 * Copyright (c) 2017, 2023, Oracle and/or its affiliates.
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

#ifndef PLUGIN_X_TESTS_DRIVER_CONNECTOR_CONNECTION_MANAGER_H_
#define PLUGIN_X_TESTS_DRIVER_CONNECTOR_CONNECTION_MANAGER_H_

#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "errmsg.h"
#include "plugin/x/client/mysqlxclient/xconnection.h"
#include "plugin/x/client/mysqlxclient/xmessage.h"
#include "plugin/x/client/mysqlxclient/xprotocol.h"
#include "plugin/x/client/mysqlxclient/xsession.h"
#include "plugin/x/tests/driver/connector/session_holder.h"
#include "plugin/x/tests/driver/formatters/console.h"
#include "plugin/x/tests/driver/processor/variable_container.h"

using Message_ptr = std::unique_ptr<xcl::XProtocol::Message>;

class Connection_manager {
 public:
  Connection_manager(const Connection_options &co,
                     Variable_container *variables,
                     const Console &console_with_flow_history,
                     const Console &console);
  ~Connection_manager();

  void get_credentials(std::string *ret_user, std::string *ret_pass);
  void safe_close(const std::string &name);

  void connect_default(const bool send_cap_password_expired = false,
                       const bool client_interactive = false,
                       const bool no_auth = false,
                       const bool connect_attrs = true);
  void create(const std::string &name, const std::string &user,
              const std::string &password, const std::string &db,
              const std::vector<std::string> &auth_methods,
              const bool is_raw_connection);

  void abort_active();
  bool is_default_active();

  void close_active(const bool shutdown = false, const bool be_quiet = false);
  void set_active(const std::string &name, const bool be_quiet = false);

  xcl::XSession *active_xsession();
  xcl::XProtocol *active_xprotocol();
  xcl::XConnection *active_xconnection();
  Session_holder &active_holder();

  uint64_t active_session_messages_received(
      const std::string &message_name) const;

  void setup_variables(xcl::XSession *session);

 private:
  using Session_holder_ptr = std::shared_ptr<Session_holder>;
  using Map_name_vs_session = std::map<std::string, Session_holder_ptr>;

  Map_name_vs_session m_session_holders;
  Session_holder_ptr m_active_holder;
  std::string m_active_session_name;
  Connection_options m_default_connection_options;
  Variable_container *m_variables;
  const Console &m_console_with_flow_history;
  const Console &m_console;
};

#endif  // PLUGIN_X_TESTS_DRIVER_CONNECTOR_CONNECTION_MANAGER_H_

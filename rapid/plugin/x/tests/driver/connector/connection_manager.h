/*
 * Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef X_TESTS_DRIVER_CONNECTOR_CONNECTION_MANAGER_H_
#define X_TESTS_DRIVER_CONNECTOR_CONNECTION_MANAGER_H_

#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <utility>

#include "connector/session_holder.h"
#include "formatters/console.h"
#include "errmsg.h"
#include "mysqlxclient/xconnection.h"
#include "mysqlxclient/xmessage.h"
#include "mysqlxclient/xprotocol.h"
#include "mysqlxclient/xsession.h"
#include "processor/variable_container.h"


using Message_ptr = std::unique_ptr<xcl::XProtocol::Message>;

class Connection_manager {
 public:
  Connection_manager(const Connection_options &co,
                     Variable_container *variables,
                     const Console &console);
  ~Connection_manager();

  void get_credentials(std::string *ret_user, std::string *ret_pass);
  void safe_close(const std::string &name);

  void connect_default(const bool send_cap_password_expired = false,
                       const bool no_auth = false,
                       const bool use_plain_auth = false);
  void create(const std::string &name, const std::string &user,
              const std::string &password, const std::string &db, bool no_ssl);
  void abort_active();
  bool is_default_active();

  void close_active(const bool shutdown = false, const bool be_quiet = false);
  void set_active(const std::string &name, const bool be_quiet = false);

  xcl::XSession    *active_xsession();
  xcl::XProtocol   *active_xprotocol();
  xcl::XConnection *active_xconnection();
  Session_holder   &active_holder();

  uint64_t active_session_messages_received(
    const std::string &message_name) const;

 private:
  using Session_holder_ptr  = std::shared_ptr<Session_holder>;
  using Map_name_vs_session = std::map<std::string, Session_holder_ptr>;

  Map_name_vs_session m_session_holders;
  Session_holder_ptr  m_active_holder;
  std::string         m_active_session_name;
  Connection_options  m_connection_options;
  Variable_container *m_variables;
  const Console      &m_console;
};

#endif  // X_TESTS_DRIVER_CONNECTOR_CONNECTION_MANAGER_H_

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

#ifndef PLUGIN_X_SRC_XPL_SESSION_H_
#define PLUGIN_X_SRC_XPL_SESSION_H_

#include <string>
#include <vector>

#include "plugin/x/ngs/include/ngs/client_session.h"
#include "plugin/x/ngs/include/ngs/session_status_variables.h"
#include "plugin/x/src/crud_cmd_handler.h"
#include "plugin/x/src/expect/expect_stack.h"
#include "plugin/x/src/sql_data_context.h"
#include "plugin/x/src/xpl_global_status_variables.h"

namespace xpl {

class Sql_data_context;
class Dispatcher;
class Cursor_manager;
class Client;

class Session_options {
 public:
  Session_options() : m_send_warnings(true), m_send_xplugin_deprecation(true) {}

  void set_send_warnings(bool flag) { m_send_warnings = flag; }
  bool get_send_warnings() const { return m_send_warnings; }

  void set_send_xplugin_deprecation(bool flag) {
    m_send_xplugin_deprecation = flag;
  }
  bool get_send_xplugin_deprecation() const {
    return m_send_xplugin_deprecation;
  }

 private:
  bool m_send_warnings;
  bool m_send_xplugin_deprecation;
};

class Session : public ngs::Session {
 public:
  Session(ngs::Client_interface &client, ngs::Protocol_encoder_interface *proto,
          const Session_id session_id);
  virtual ~Session();

 public:  // impl ngs::Session_interface
  ngs::Error_code init() override;
  virtual void on_auth_success(
      const ngs::Authentication_interface::Response &response) override;
  virtual void on_auth_failure(
      const ngs::Authentication_interface::Response &response) override;

  virtual void mark_as_tls_session() override;
  virtual THD *get_thd() const override;
  ngs::Sql_session_interface &data_context() override { return m_sql; }

 public:
  using Variable =
      ngs::Common_status_variables::Variable ngs::Common_status_variables::*;

  Session_options &options() { return m_options; }
  ngs::Session_status_variables &get_status_variables() override {
    return m_status_variables;
  }

  bool can_see_user(const std::string &user) const;

  template <Variable variable>
  void update_status();

  template <Variable variable>
  void update_status(long param);

  void update_status(Variable variable);

 private:  // reimpl ngs::Session
  virtual void on_kill() override;
  virtual bool handle_ready_message(ngs::Message_request &command) override;

 private:
  Sql_data_context m_sql;
  Crud_command_handler m_crud_handler;
  Expectation_stack m_expect_stack;

  Session_options m_options;
  ngs::Session_status_variables m_status_variables;

  bool m_was_authenticated;
};

template <ngs::Common_status_variables::Variable ngs::Common_status_variables::
              *variable>
void Session::update_status() {
  ++(m_status_variables.*variable);
  ++(Global_status_variables::instance().*variable);
}

template <ngs::Common_status_variables::Variable ngs::Common_status_variables::
              *variable>
void Session::update_status(long param) {
  (m_status_variables.*variable) += param;
  (Global_status_variables::instance().*variable) += param;
}
}  // namespace xpl

#endif  // PLUGIN_X_SRC_XPL_SESSION_H_

/*
 * Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301  USA
 */

#ifndef _XPL_SESSION_H_
#define _XPL_SESSION_H_

#include <string>
#include <vector>
#include "ngs/client_session.h"

#include "sql_data_context.h"
#include "expect.h"
#include "crud_cmd_handler.h"
#include "xpl_session_status_variables.h"
#include "xpl_global_status_variables.h"

namespace xpl
{

class Sql_data_context;
class Dispatcher;
class Cursor_manager;
class Client;

class Session_options
{
public:
  Session_options()
  : m_send_warnings(true), m_send_xplugin_deprecation(true)
  {}

  void set_send_warnings(bool flag) { m_send_warnings = flag; }
  bool get_send_warnings() const { return m_send_warnings; }

  void set_send_xplugin_deprecation(bool flag) { m_send_xplugin_deprecation = flag; }
  bool get_send_xplugin_deprecation() const { return m_send_xplugin_deprecation; }

private:
  bool m_send_warnings;
  bool m_send_xplugin_deprecation;
};


class Session : public ngs::Session
{
public:
  Session(ngs::Client_interface &client, ngs::Protocol_encoder *proto, const Session_id session_id);
  virtual ~Session();

public: // impl ngs::Session_interface
  ngs::Error_code init();
  virtual void on_auth_success(const ngs::Authentication_handler::Response &response);
  virtual void on_auth_failure(const ngs::Authentication_handler::Response &response);

  virtual void mark_as_tls_session();
  virtual bool is_handled_by(const void *handler) const;

public:
  virtual Sql_data_context &data_context() { return m_sql; }
  Session_options &options() { return m_options; }
  Session_status_variables &get_status_variables() { return m_status_variables; }

  bool can_see_user(const std::string &user) const;

  template<Common_status_variables::Variable Common_status_variables::*variable>
  void update_status();
  template<Common_status_variables::Variable Common_status_variables::*variable>
  void update_status(long param);

  void update_status(Common_status_variables::Variable
                     Common_status_variables::*variable);

private: // reimpl ngs::Session
  virtual void on_kill();
  virtual bool handle_ready_message(ngs::Request &command);

private:
  Sql_data_context m_sql;
  Crud_command_handler m_crud_handler;
  Expectation_stack m_expect_stack;

  Session_options m_options;
  Session_status_variables m_status_variables;

  bool m_was_authenticated;
};


template<Common_status_variables::Variable Common_status_variables::*variable>
void Session::update_status()
{
  ++(m_status_variables.*variable);
  ++(Global_status_variables::instance().*variable);
}


template<Common_status_variables::Variable Common_status_variables::*variable>
void Session::update_status(long param)
{
  (m_status_variables.*variable) += param;
  (Global_status_variables::instance().*variable) += param;
}
} // namespace xpl

#endif  // _XPL_SESSION_H_

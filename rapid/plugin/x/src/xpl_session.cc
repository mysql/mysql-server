/*
 * Copyright (c) 2015, 2016 Oracle and/or its affiliates. All rights reserved.
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

#include "xpl_dispatcher.h"
#include "xpl_session.h"
#include "xpl_log.h"
#include "xpl_server.h"

#include "crud_cmd_handler.h"
#include "sql_data_context.h"
#include "notices.h"

#include "ngs/scheduler.h"
#include "ngs/interface/client_interface.h"
#include "ngs/ngs_error.h"
#include "ngs_common/protocol_protobuf.h"

#include <iostream>


xpl::Session::Session(ngs::Client_interface &client, ngs::Protocol_encoder *proto, const Session_id session_id)
: ngs::Session(client, proto, session_id),
  m_sql(proto),
  m_was_authenticated(false)
{
}


xpl::Session::~Session()
{
  if (m_was_authenticated)
    --Global_status_variables::instance().m_sessions_count;

  m_sql.deinit();
}


// handle a message while in Ready state
bool xpl::Session::handle_ready_message(ngs::Request &command)
{
  // check if the session got killed
  if (m_sql.is_killed())
  {
    m_encoder->send_result(ngs::Error_code(ER_QUERY_INTERRUPTED, "Query execution was interrupted", "70100", ngs::Error_code::FATAL));
    // close as fatal_error instead of killed. killed is for when the client is idle
    on_close();
    return true;
  }

  if (ngs::Session::handle_ready_message(command))
    return true;

  try
  {
    return dispatcher::dispatch_command(*this, m_crud_handler, m_expect_stack, command);
  }
  catch (ngs::Error_code &err)
  {
    m_encoder->send_result(err);
    on_close();
    return true;
  }
  catch (std::exception &exc)
  {
    // not supposed to happen, but catch exceptions as a last defense..
    log_error("%s: Unexpected exception dispatching command: %s\n", m_client.client_id(), exc.what());
    on_close();
    return true;
  }
  return false;
}


ngs::Error_code xpl::Session::init()
{
  const unsigned short port = m_client.client_port();
  const ngs::Connection_type type = m_client.connection().connection_type();

  return m_sql.init(port, type);
}


void xpl::Session::on_kill()
{
  if (!m_sql.is_killed())
  {
    if (!m_sql.kill())
      log_info("%s: Could not interrupt client session", m_client.client_id());
  }

  on_close(true);
}


void xpl::Session::on_auth_success(const ngs::Authentication_handler::Response &response)
{
  xpl::notices::send_client_id(proto(), m_client.client_id_num());
  ngs::Session::on_auth_success(response);

  ++Global_status_variables::instance().m_accepted_sessions_count;
  ++Global_status_variables::instance().m_sessions_count;

  m_was_authenticated = true;
}


void xpl::Session::on_auth_failure(const ngs::Authentication_handler::Response &response)
{
  if (response.error_code == ER_MUST_CHANGE_PASSWORD && !m_sql.password_expired())
  {
    ngs::Authentication_handler::Response r = {"Password for " MYSQLXSYS_ACCOUNT " account has been expired", response.status, response.error_code};
    ngs::Session::on_auth_failure(r);
  }
  else
    ngs::Session::on_auth_failure(response);

  ++Global_status_variables::instance().m_rejected_sessions_count;
}


void xpl::Session::mark_as_tls_session()
{
  data_context().set_connection_type(ngs::Connection_tls);
}


bool xpl::Session::is_handled_by(const void *handler) const
{
  return m_sql.get_thd() == handler;
}


/** Checks whether things owned by the given user are visible to this session.
 Returns true if we're SUPER or the same user as the given one.
 If user is NULL, then it's only visible for SUPER users.
 */
bool xpl::Session::can_see_user(const std::string &user) const
{
  const std::string owner = m_sql.get_authenticated_user_name();

  if (is_ready() && !owner.empty())
  {
    if (m_sql.has_authenticated_user_a_super_priv()
        || (owner == user))
      return true;
  }
  return false;
}


void xpl::Session::update_status(Common_status_variables::Variable
                                 Common_status_variables::*variable)
{
  ++(m_status_variables.*variable);
  ++(Global_status_variables::instance().*variable);
}

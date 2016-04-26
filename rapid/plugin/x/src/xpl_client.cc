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

#include "xpl_server.h"
#include "xpl_client.h"
#include "xpl_session.h"
#include "mysql_show_variable_wrapper.h"

#include "ngs/thread.h"

#include "cap_handles_expired_passwords.h"
#include "ngs/capabilities/configurator.h"
#include "ngs/capabilities/handler_readonly_value.h"

#include <boost/make_shared.hpp>
#include <boost/algorithm/string/join.hpp>
#include "xpl_plugin.h"
#include "mysql_variables.h"


using namespace xpl;

Client::Client(ngs::Connection_ptr connection, ngs::IServer *server, Client_id client_id,
               Protocol_monitor *pmon)
: ngs::Client(connection, server, client_id, *pmon),
  m_supports_expired_passwords(false),
  m_protocol_monitor(pmon)
{
  if (m_protocol_monitor)
    m_protocol_monitor->init(this);
}


Client::~Client()
{
  delete m_protocol_monitor;
}


void Client::on_session_close(ngs::Session *s)
{
  ngs::Client::on_session_close(s);
  if (s->state_before_close() != ngs::Session::Authenticating)
  {
    Global_status_variables::instance().increment_closed_sessions_count();
  }
}


void Client::on_session_reset(ngs::Session *s)
{
  ngs::Client::on_session_reset(s);
}


void Client::set_supports_expired_passwords(bool flag)
{
  m_supports_expired_passwords = flag;
}

bool Client::supports_expired_passwords()
{
  return m_supports_expired_passwords;
}

ngs::Capabilities_configurator *Client::capabilities_configurator()
{
  ngs::Capabilities_configurator *caps = ngs::Client::capabilities_configurator();

  // add our capabilities
  caps->add_handler(boost::make_shared<ngs::Capability_readonly_value>("node_type", "mysql"));
  caps->add_handler(boost::make_shared<ngs::Capability_readonly_value>("plugin.version", XPL_PLUGIN_VERSION_STRING));
  caps->add_handler(boost::make_shared<Cap_handles_expired_passwords>(boost::static_pointer_cast<xpl::Client>(shared_from_this())));

  return caps;
}

boost::shared_ptr<xpl::Session> Client::get_session()
{
  return boost::static_pointer_cast<xpl::Session>(session());
}


/** Close the client from another thread

This can be called from any thread, so care must be taken to not call
anything that's not thread safe from here.
 */
void Client::kill()
{
  if (m_state == Client_accepted)
  {
    disconnect_and_trigger_close();
    return;
  }

  m_session->on_kill();
  Global_status_variables::instance().increment_killed_sessions_count();
}


void Client::on_network_error(int error)
{
  ngs::Client::on_network_error(error);
  if (error != 0)
    Global_status_variables::instance().increment_connection_errors_count();
}


void Client::on_server_shutdown()
{
  boost::shared_ptr<ngs::Session> local_copy = m_session;

  if (local_copy)
    local_copy->on_kill();

  ngs::Client::on_server_shutdown();
}


void Client::on_auth_timeout()
{
  ngs::Client::on_auth_timeout();

  Global_status_variables::instance().increment_connection_errors_count();
}


bool Client::is_handler_thd(THD *thd)
{
  boost::shared_ptr<xpl::Session> session = get_session();

  return thd && session && (session->data_context().get_thd() == thd);
}


void Client::get_status_ssl_cipher_list(st_mysql_show_var * var)
{
  std::vector<std::string> ciphers = connection().options()->ssl_cipher_list();

  mysqld::xpl_show_var(var).assign(boost::join(ciphers, ":").c_str());
}


void Client::post_activate_tls()
{
  boost::shared_ptr<Session> session = get_session();
  if (session)
    session->data_context().set_connection_type(connection().options()->active_tls());
}


bool Client::is_localhost(const char *hostname)
{
  return hostname == mysqld::get_my_localhost();
}


void Protocol_monitor::init(Client *client)
{
  m_client = client;
}


namespace
{
template<void (Common_status_variables::*method)()>
inline void update_status_variable(boost::shared_ptr<xpl::Session> session)
{
  if (session)
    xpl::Server::update_status_variable<method>(session->get_status_variables());
}
} // namespace


void Protocol_monitor::on_notice_warning_send()
{
  update_status_variable<&Common_status_variables::inc_notice_warning_sent>(m_client->get_session());
}


void Protocol_monitor::on_notice_other_send()
{
  update_status_variable<&Common_status_variables::inc_notice_other_sent>(m_client->get_session());
}


void Protocol_monitor::on_error_send()
{
  update_status_variable<&Common_status_variables::inc_errors_sent>(m_client->get_session());
}


void Protocol_monitor::on_fatal_error_send()
{
  Global_status_variables::instance().increment_sessions_fatal_errors_count();
}


void Protocol_monitor::on_init_error_send()
{
  Global_status_variables::instance().increment_init_errors_count();
}


void Protocol_monitor::on_row_send()
{
  update_status_variable<&Common_status_variables::inc_rows_sent>(m_client->get_session());
}


void Protocol_monitor::on_send(long bytes_transferred)
{
  Global_status_variables::instance().inc_bytes_sent(bytes_transferred);

  boost::shared_ptr<xpl::Session> session(m_client->get_session());
  if (session)
    session->get_status_variables().inc_bytes_sent(bytes_transferred);
}


void Protocol_monitor::on_receive(long bytes_transferred)
{
  Global_status_variables::instance().inc_bytes_received(bytes_transferred);

  boost::shared_ptr<xpl::Session> session(m_client->get_session());
  if (session)
    session->get_status_variables().inc_bytes_received(bytes_transferred);
}

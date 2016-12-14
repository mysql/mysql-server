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
#include "xpl_session.h"
#include "mysql_show_variable_wrapper.h"

#include "ngs_common/string_formatter.h"
#include "ngs/thread.h"

#include "ngs/capabilities/configurator.h"
#include "ngs/capabilities/handler_readonly_value.h"
#include "cap_handles_expired_passwords.h"

#include "mysqlx_version.h"
#include "mysql_variables.h"
#include "xpl_client.h"

// needed for ip_to_hostname(), should probably be turned into a service
#include "hostname.h"


using namespace xpl;

Client::Client(ngs::Connection_ptr connection, ngs::Server_interface &server, Client_id client_id,
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
  ngs::free_object(m_protocol_monitor);
}


void Client::on_session_close(ngs::Session_interface &s)
{
  ngs::Client::on_session_close(s);
  if (s.state_before_close() != ngs::Session_interface::Authenticating)
  {
    ++Global_status_variables::instance().m_closed_sessions_count;
  }
}


void Client::on_session_reset(ngs::Session_interface &s)
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
  caps->add_handler(ngs::allocate_shared<ngs::Capability_readonly_value>("node_type", "mysql"));
  caps->add_handler(ngs::allocate_shared<Cap_handles_expired_passwords>(ngs::ref(*this)));

  return caps;
}

ngs::shared_ptr<xpl::Session> Client::get_session()
{
  return ngs::static_pointer_cast<xpl::Session>(session());
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
  ++Global_status_variables::instance().m_killed_sessions_count;
}


void Client::on_network_error(int error)
{
  ngs::Client::on_network_error(error);
  if (error != 0)
    ++Global_status_variables::instance().m_connection_errors_count;
}


void Client::on_server_shutdown()
{
  ngs::shared_ptr<ngs::Session_interface> local_copy = m_session;

  if (local_copy)
    local_copy->on_kill();

  ngs::Client::on_server_shutdown();
}


void Client::on_auth_timeout()
{
  ngs::Client::on_auth_timeout();

  ++Global_status_variables::instance().m_connection_errors_count;
}


bool Client::is_handler_thd(THD *thd)
{
  ngs::shared_ptr<ngs::Session_interface> session = this->session();

  return thd && session && (session->is_handled_by(thd));
}


void Client::get_status_ssl_cipher_list(st_mysql_show_var * var)
{
  std::vector<std::string> ciphers = connection().options()->ssl_cipher_list();

  mysqld::xpl_show_var(var).assign(ngs::join(ciphers, ":"));
}


std::string Client::resolve_hostname()
{
  std::string result;
  std::string socket_ip_string;
  uint16      socket_port;

  sockaddr_storage *addr = m_connection->peer_address(socket_ip_string, socket_port);

  if (NULL == addr)
  {
    log_error("%s: get peer address failed, can't resolve IP to hostname", m_id);
    return "";
  }

  char *hostname = NULL;
  uint connect_errors = 0;
  const int resolve_result = ip_to_hostname(addr, socket_ip_string.c_str(), &hostname, &connect_errors);

  if (RC_BLOCKED_HOST == resolve_result) {
    throw std::runtime_error("Host is blocked");
  }

  if (hostname) {
    result = hostname;

    if (!is_localhost(hostname))
      my_free(hostname);
  }

  return result;
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
template<xpl::Common_status_variables::Variable xpl::Common_status_variables::*variable>
inline void update_status(ngs::shared_ptr<xpl::Session> session)
{
  if (session)
    ++(session->get_status_variables().*variable);
  ++(Global_status_variables::instance().*variable);
}


template<xpl::Common_status_variables::Variable xpl::Common_status_variables::*variable>
inline void update_status(ngs::shared_ptr<xpl::Session> session, long param)
{
  if (session)
    (session->get_status_variables().*variable) += param;
  (Global_status_variables::instance().*variable) += param;
}
} // namespace


void Protocol_monitor::on_notice_warning_send()
{
  update_status<&Common_status_variables::m_notice_warning_sent>(m_client->get_session());
}


void Protocol_monitor::on_notice_other_send()
{
  update_status<&Common_status_variables::m_notice_other_sent>(m_client->get_session());
}


void Protocol_monitor::on_error_send()
{
  update_status<&Common_status_variables::m_errors_sent>(m_client->get_session());
}


void Protocol_monitor::on_fatal_error_send()
{
  ++Global_status_variables::instance().m_sessions_fatal_errors_count;
}


void Protocol_monitor::on_init_error_send()
{
  ++Global_status_variables::instance().m_init_errors_count;
}


void Protocol_monitor::on_row_send()
{
  update_status<&Common_status_variables::m_rows_sent>(m_client->get_session());
}


void Protocol_monitor::on_send(long bytes_transferred)
{
  update_status<&Common_status_variables::m_bytes_sent>(m_client->get_session(), bytes_transferred);
}


void Protocol_monitor::on_receive(long bytes_transferred)
{
  update_status<&Common_status_variables::m_bytes_received>(m_client->get_session(), bytes_transferred);
}


void Protocol_monitor::on_error_unknown_msg_type()
{
  update_status<&Common_status_variables::m_errors_unknown_message_type>(m_client->get_session());
}

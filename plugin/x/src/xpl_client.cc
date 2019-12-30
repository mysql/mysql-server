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

#include "plugin/x/src/xpl_client.h"

#include <stddef.h>
#include <sys/types.h>
#include <memory>
#include <stdexcept>

// needed for ip_to_hostname(), should probably be turned into a service
#include "my_dbug.h"
#include "my_inttypes.h"
#include "my_systime.h"  // my_sleep
#include "plugin/x/generated/mysqlx_version.h"
#include "plugin/x/ngs/include/ngs/thread.h"
#include "plugin/x/src/capabilities/configurator.h"
#include "plugin/x/src/capabilities/handler_expired_passwords.h"
#include "plugin/x/src/capabilities/handler_readonly_value.h"
#include "plugin/x/src/helper/string_formatter.h"
#include "plugin/x/src/mysql_show_variable_wrapper.h"
#include "plugin/x/src/mysql_variables.h"
#include "plugin/x/src/xpl_server.h"
#include "plugin/x/src/xpl_session.h"
#include "sql/hostname_cache.h"  // ip_to_hostname

namespace xpl {

Client::Client(std::shared_ptr<ngs::Vio_interface> connection,
               ngs::Server_interface &server, Client_id client_id,
               Protocol_monitor *pmon, const Global_timeouts &timeouts)
    : ngs::Client(connection, server, client_id, pmon, timeouts) {
  if (pmon) pmon->init(this);
}

Client::~Client() { ngs::free_object(m_protocol_monitor); }

Capabilities_configurator *Client::capabilities_configurator() {
  Capabilities_configurator *caps = ngs::Client::capabilities_configurator();

  // add our capabilities
  caps->add_handler(
      ngs::allocate_shared<Capability_readonly_value>("node_type", "mysql"));
  caps->add_handler(
      ngs::allocate_shared<Cap_handles_expired_passwords>(std::ref(*this)));

  return caps;
}

void Client::set_is_interactive(const bool flag) {
  m_is_interactive = flag;

  if (nullptr == m_session.get()) return;

  auto thd = m_session->get_thd();

  if (nullptr == thd) return;

  if (!m_session->data_context().attach()) {
    auto global_timeouts = get_global_timeouts();

    const auto timeout = m_is_interactive ? global_timeouts.interactive_timeout
                                          : global_timeouts.wait_timeout;
    m_decoder.set_wait_timeout(timeout);
    set_session_wait_timeout(thd, timeout);

    m_session->data_context().detach();
  }
}

/** Close the client from another thread

This can be called from any thread, so care must be taken to not call
anything that's not thread safe from here.
 */
void Client::kill() {
  if (m_state == ngs::Client_interface::State::k_accepted) {
    disconnect_and_trigger_close();
    return;
  }

  m_session->on_kill();
  ++Global_status_variables::instance().m_killed_sessions_count;
}

/* Check is a session assigned to this client has following thread data

   The method can be called from different thread/xpl_client.
 */
bool Client::is_handler_thd(const THD *thd) const {
  // When accessing the session we need to hold it in
  // shared_pointer to be sure that the session is
  // not reseted (by Mysqlx::Session::Reset) in middle
  // of this operations.
  MUTEX_LOCK(lock_session_exit, m_session_exit_mutex);
  auto session = this->session_smart_ptr();

  return thd && session && (session->get_thd() == thd);
}

void Client::get_status_ssl_cipher_list(SHOW_VAR *var) {
  std::vector<std::string> ciphers =
      Ssl_session_options(&connection()).ssl_cipher_list();

  mysqld::xpl_show_var(var).assign(join(ciphers, ":"));
}

std::string Client::resolve_hostname() {
  std::string result;
  std::string socket_ip_string;
  uint16 socket_port;

  DBUG_EXECUTE_IF("resolve_timeout", {
    int i = 0;
    int max_iterations = 1000;
    while (server().is_running() && i < max_iterations) {
      my_sleep(10000);
      ++i;
    }
  });

  sockaddr_storage *addr =
      m_connection->peer_addr(socket_ip_string, socket_port);

  if (NULL == addr) {
    log_debug("%s: get peer address failed, can't resolve IP to hostname",
              m_id);
    return "";
  }

  char *hostname = NULL;
  uint connect_errors = 0;
  const int resolve_result = ip_to_hostname(addr, socket_ip_string.c_str(),
                                            &hostname, &connect_errors);

  if (RC_BLOCKED_HOST == resolve_result) {
    throw std::runtime_error("Host is blocked");
  }

  if (hostname) {
    result = hostname;

    if (!is_localhost(hostname)) my_free(hostname);
  }

  return result;
}

bool Client::is_localhost(const char *hostname) {
  return hostname == mysqld::get_my_localhost();
}

void Protocol_monitor::init(Client *client) { m_client = client; }

namespace {

template <ngs::Common_status_variables::Variable ngs::Common_status_variables::
              *variable>
inline void update_status(ngs::Session_interface *session) {
  if (session) ++(session->get_status_variables().*variable);
  ++(Global_status_variables::instance().*variable);
}

template <ngs::Common_status_variables::Variable ngs::Common_status_variables::
              *variable>
inline void update_status(ngs::Session_interface *session,
                          const uint32_t value) {
  if (session) (session->get_status_variables().*variable) += value;
  (Global_status_variables::instance().*variable) += value;
}

}  // namespace

void Protocol_monitor::on_notice_warning_send() {
  update_status<&ngs::Common_status_variables::m_notice_warning_sent>(
      m_client->session());
}

void Protocol_monitor::on_notice_other_send() {
  update_status<&ngs::Common_status_variables::m_notice_other_sent>(
      m_client->session());
}

void Protocol_monitor::on_notice_global_send() {
  update_status<&ngs::Common_status_variables::m_notice_global_sent>(
      m_client->session());
}

void Protocol_monitor::on_error_send() {
  update_status<&ngs::Common_status_variables::m_errors_sent>(
      m_client->session());
}

void Protocol_monitor::on_fatal_error_send() {
  ++Global_status_variables::instance().m_sessions_fatal_errors_count;
}

void Protocol_monitor::on_init_error_send() {
  ++Global_status_variables::instance().m_init_errors_count;
}

void Protocol_monitor::on_row_send() {
  update_status<&ngs::Common_status_variables::m_rows_sent>(
      m_client->session());
}

void Protocol_monitor::on_send(const uint32_t bytes_transferred) {
  update_status<&ngs::Common_status_variables::m_bytes_sent>(
      m_client->session(), bytes_transferred);
}

void Protocol_monitor::on_send_compressed(const uint32_t bytes_transferred) {
  update_status<&ngs::Common_status_variables::m_bytes_sent_compressed_payload>(
      m_client->session(), bytes_transferred);
}

void Protocol_monitor::on_send_before_compression(
    const uint32_t bytes_transferred) {
  update_status<&ngs::Common_status_variables::m_bytes_sent_uncompressed_frame>(
      m_client->session(), bytes_transferred);
}

void Protocol_monitor::on_receive(const uint32_t bytes_transferred) {
  update_status<&ngs::Common_status_variables::m_bytes_received>(
      m_client->session(), bytes_transferred);
}

void Protocol_monitor::on_receive_compressed(const uint32_t bytes_transferred) {
  update_status<
      &ngs::Common_status_variables::m_bytes_received_compressed_payload>(
      m_client->session(), bytes_transferred);
}

void Protocol_monitor::on_receive_after_decompression(
    const uint32_t bytes_transferred) {
  update_status<
      &ngs::Common_status_variables::m_bytes_received_uncompressed_frame>(
      m_client->session(), bytes_transferred);
}

void Protocol_monitor::on_error_unknown_msg_type() {
  update_status<&ngs::Common_status_variables::m_errors_unknown_message_type>(
      m_client->session());
}

void Protocol_monitor::on_messages_sent(const uint32_t messages) {
  update_status<&ngs::Common_status_variables::m_messages_sent>(
      m_client->session(), messages);
}

}  // namespace xpl

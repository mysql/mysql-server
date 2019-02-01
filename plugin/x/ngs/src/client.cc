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

#include "plugin/x/ngs/include/ngs/client.h"

#ifndef WIN32
#include <arpa/inet.h>
#endif
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <algorithm>
#include <functional>
#include <sstream>

#include "my_dbug.h"

#include "my_macros.h"
#include "my_systime.h"  // my_sleep

#include "plugin/x/ngs/include/ngs/interface/protocol_monitor_interface.h"
#include "plugin/x/ngs/include/ngs/interface/server_interface.h"
#include "plugin/x/ngs/include/ngs/interface/session_interface.h"
#include "plugin/x/ngs/include/ngs/interface/ssl_context_interface.h"
#include "plugin/x/ngs/include/ngs/log.h"
#include "plugin/x/ngs/include/ngs/ngs_error.h"
#include "plugin/x/ngs/include/ngs/protocol/protocol_config.h"
#include "plugin/x/ngs/include/ngs/protocol/protocol_protobuf.h"
#include "plugin/x/ngs/include/ngs/protocol_encoder.h"
#include "plugin/x/ngs/include/ngs/scheduler.h"
#include "plugin/x/src/capabilities/handler_auth_mech.h"
#include "plugin/x/src/capabilities/handler_client_interactive.h"
#include "plugin/x/src/capabilities/handler_connection_attributes.h"
#include "plugin/x/src/capabilities/handler_readonly_value.h"
#include "plugin/x/src/capabilities/handler_tls.h"
#include "plugin/x/src/operations_factory.h"
#include "plugin/x/src/xpl_global_status_variables.h"
#include "plugin/x/src/xpl_performance_schema.h"

namespace ngs {

using Waiting_for_io_interface = Protocol_decoder::Waiting_for_io_interface;

namespace details {

class No_idle_processing : public Waiting_for_io_interface {
 public:
  bool has_to_report_idle_waiting() override { return false; }
  void on_idle_or_before_read() override {}

 private:
};

}  // namespace details

Client::Client(std::shared_ptr<Vio_interface> connection,
               Server_interface &server, Client_id client_id,
               Protocol_monitor_interface *pmon,
               const Global_timeouts &timeouts)
    : m_client_id(client_id),
      m_server(server),
      m_connection(connection),
      m_decoder(m_connection, pmon, m_server.get_config(),
                timeouts.wait_timeout, timeouts.read_timeout),
      m_client_addr("n/c"),
      m_client_port(0),
      m_state(Client_invalid),
      m_removed(false),
      m_protocol_monitor(pmon),
      m_session_exit_mutex(KEY_mutex_x_client_session_exit),
      m_msg_buffer(NULL),
      m_msg_buffer_size(0),
      m_supports_expired_passwords(false) {
  snprintf(m_id, sizeof(m_id), "%llu", static_cast<ulonglong>(client_id));
  m_decoder.set_wait_timeout(timeouts.wait_timeout);
  m_decoder.set_read_timeout(m_read_timeout = timeouts.read_timeout);
  m_write_timeout = timeouts.write_timeout;
}

Client::~Client() {
  log_debug("%s: Delete client", m_id);
  if (m_connection) m_connection->shutdown();

  if (m_msg_buffer) free_array(m_msg_buffer);
}

xpl::chrono::Time_point Client::get_accept_time() const {
  return m_accept_time;
}

void Client::reset_accept_time() {
  m_accept_time = xpl::chrono::now();
  m_server.restart_client_supervision_timer();
}

void Client::activate_tls() {
  log_debug("%s: enabling TLS for client", client_id());

  const auto connect_timeout =
      xpl::chrono::to_seconds(m_server.get_config()->connect_timeout);

  const auto real_connect_timeout =
      std::min<uint32_t>(connect_timeout, m_read_timeout);

  if (m_server.ssl_context()->activate_tls(&connection(),
                                           real_connect_timeout)) {
    session()->mark_as_tls_session();
  } else {
    log_debug("%s: Error during SSL handshake", client_id());
    disconnect_and_trigger_close();
  }
}

void Client::on_auth_timeout() {
  set_close_reason_if_non_fatal(Close_reason::k_connect_timeout);

  // XXX send an ERROR notice when it's available
  disconnect_and_trigger_close();
}

xpl::Capabilities_configurator *Client::capabilities_configurator() {
  std::vector<xpl::Capability_handler_ptr> handlers;

  handlers.push_back(allocate_shared<xpl::Capability_tls>(std::ref(*this)));
  handlers.push_back(
      allocate_shared<xpl::Capability_auth_mech>(std::ref(*this)));

  handlers.push_back(
      allocate_shared<xpl::Capability_readonly_value>("doc.formats", "text"));

  handlers.push_back(
      allocate_shared<xpl::Capability_client_interactive>(std::ref(*this)));

  handlers.push_back(
      ngs::allocate_shared<xpl::Capability_connection_attributes>());

  return ngs::allocate_object<xpl::Capabilities_configurator>(handlers);
}

void Client::get_capabilities(const Mysqlx::Connection::CapabilitiesGet &) {
  Memory_instrumented<xpl::Capabilities_configurator>::Unique_ptr configurator(
      capabilities_configurator());
  Memory_instrumented<Mysqlx::Connection::Capabilities>::Unique_ptr caps(
      configurator->get());

  m_encoder->send_message(Mysqlx::ServerMessages::CONN_CAPABILITIES, *caps);
}

void Client::set_capabilities(
    const Mysqlx::Connection::CapabilitiesSet &setcap) {
  Memory_instrumented<xpl::Capabilities_configurator>::Unique_ptr configurator(
      capabilities_configurator());
  Error_code error_code = configurator->prepare_set(setcap.capabilities());
  m_encoder->send_result(error_code);
  if (!error_code) {
    configurator->commit();
  }
}

bool Client::handle_session_connect_attr_set(ngs::Message_request &command) {
  const auto capabilities_set =
      static_cast<const Mysqlx::Connection::CapabilitiesSet &>(
          *command.get_message());
  const auto capabilities = capabilities_set.capabilities();
  // other capabilites are not allowed at this point
  if (capabilities.capabilities_size() != 1 ||
      capabilities.capabilities(0).name() != "session_connect_attrs") {
    log_debug("Only session_connect_attr capability is allowed at this point");
    m_encoder->send_result(
        ngs::Fatal(ER_X_CAPABILITY_SET_NOT_ALLOWED,
                   "Only session_connect_attr capability is allowed after"
                   " Session.Reset"));
  } else {
    set_capabilities(capabilities_set);
  }
  return true;
}

void Client::handle_message(Message_request &request) {
  auto s(session());

  log_message_recv(request);

  if (m_state == Client_accepted_with_session &&
      request.get_message_type() ==
          Mysqlx::ClientMessages::CON_CAPABILITIES_SET) {
    handle_session_connect_attr_set(request);
    return;
  }

  if (m_state != Client_accepted && s) {
    // pass the message to the session
    s->handle_message(request);
    return;
  }

  Client_state expected_state = Client_accepted;

  // there is no session before authentication, so we handle the messages
  // ourselves
  log_debug("%s: Client got message %i", client_id(),
            static_cast<int>(request.get_message_type()));

  switch (request.get_message_type()) {
    case Mysqlx::ClientMessages::CON_CLOSE:
      m_encoder->send_ok("bye!");
      set_close_reason_if_non_fatal(Close_reason::k_normal);
      disconnect_and_trigger_close();
      break;

    case Mysqlx::ClientMessages::SESS_RESET:
      // no-op, since we're only going to get called here before session is
      // authenticated
      break;

    case Mysqlx::ClientMessages::CON_CAPABILITIES_GET:
      get_capabilities(static_cast<const Mysqlx::Connection::CapabilitiesGet &>(
          *request.get_message()));
      break;

    case Mysqlx::ClientMessages::CON_CAPABILITIES_SET:
      set_capabilities(static_cast<const Mysqlx::Connection::CapabilitiesSet &>(
          *request.get_message()));
      break;

    case Mysqlx::ClientMessages::SESS_AUTHENTICATE_START:
      if (m_state.compare_exchange_strong(expected_state,
                                          Client_authenticating_first) &&
          server().is_running()) {
        log_debug("%s: Authenticating client...", client_id());

        // start redirecting incoming messages directly to the session
        if (s) {
          // forward the message to the pre-allocated session, rest of auth will
          // be handled by the session
          s->handle_message(request);
        }
        break;
      }
      // Fall through.

    default:
      // invalid message at this time
      m_protocol_monitor->on_error_unknown_msg_type();
      log_debug("%s: Invalid message %i received during client initialization",
                client_id(), request.get_message_type());
      m_encoder->send_result(Fatal(ER_X_BAD_MESSAGE, "Invalid message"));
      set_close_reason_if_non_fatal(Close_reason::k_error);
      disconnect_and_trigger_close();
      break;
  }
}

void Client::set_close_reason_if_non_fatal(const Close_reason reason) {
  switch (m_close_reason) {
    case Close_reason::k_normal:
    case Close_reason::k_none:
      m_close_reason = reason;
      return;

    default:
      return;
  }
}

void Client::disconnect_and_trigger_close() {
  set_close_reason_if_non_fatal(Close_reason::k_normal);

  m_state = Client_closing;

  m_connection->shutdown();
}

const char *Client::client_hostname_or_address() const {
  if (!m_client_host.empty()) return m_client_host.c_str();

  return m_client_addr.c_str();
}

void Client::on_read_timeout() {
  Mysqlx::Notice::Warning warning;
  const bool force_flush = true;

  set_close_reason_if_non_fatal(Close_reason::k_read_timeout);

  warning.set_level(Mysqlx::Notice::Warning::ERROR);
  warning.set_code(ER_IO_READ_ERROR);
  warning.set_msg("IO Read error: read_timeout exceeded");
  std::string warning_data;
  warning.SerializeToString(&warning_data);
  m_encoder->send_notice(Frame_type::k_warning, Frame_scope::k_global,
                         warning_data, force_flush);
}

// this will be called on socket errors, but also when halt_and_wait() is called
// which will shutdown the socket for reading and trigger a eof
// (meaning, closed for reads, but writes would still be ok)
void Client::on_network_error(const int error) {
  if (error == SOCKET_ETIMEDOUT || error == SOCKET_EAGAIN) {
    set_close_reason_if_non_fatal(Close_reason::k_write_timeout);
  }

  log_debug("%s, %i: on_network_error(error:%i)", client_id(), m_state.load(),
            error);

  if (m_state != Client_closing && error != 0)
    set_close_reason_if_non_fatal(Close_reason::k_net_error);

  m_state.exchange(Client_closing);
}

void Client::update_counters() {
  switch (m_close_reason) {
    case Close_reason::k_write_timeout:
    case Close_reason::k_read_timeout:
      ++xpl::Global_status_variables::instance().m_aborted_clients;
      ++xpl::Global_status_variables::instance().m_connection_errors_count;
      break;

    case Close_reason::k_connect_timeout:
    case Close_reason::k_net_error:
      ++xpl::Global_status_variables::instance().m_connection_errors_count;
      break;

    default:
      return;
  }
}

void Client::remove_client_from_server() {
  if (false == m_removed.exchange(true)) {
    update_counters();
    m_server.on_client_closed(*this);
  }
}

void Client::on_client_addr(const bool skip_resolve) {
  m_client_addr.resize(INET6_ADDRSTRLEN);

  switch (m_connection->get_type()) {
    case xpl::Connection_tcpip: {
      m_connection->peer_addr(m_client_addr, m_client_port);
    } break;

    case xpl::Connection_namedpipe:
    case xpl::Connection_unixsocket:  // fall through
      m_client_host = "localhost";
      return;

    default:
      return;
  }

  // turn IP to hostname for auth uses
  if (skip_resolve) return;

  m_client_host = "";

  try {
    m_client_host = resolve_hostname();
  } catch (...) {
    set_close_reason_if_non_fatal(Close_reason::k_reject);
    disconnect_and_trigger_close();

    throw;
  }
}

void Client::on_accept() {
  log_debug("%s: Accepted client connection from %s", client_id(),
            client_address());

  DBUG_EXECUTE_IF("client_accept_timeout", {
    std::int32_t i = 0;
    const std::int32_t max_iterations = 1000;
    while (m_server.is_running() && i < max_iterations) {
      my_sleep(10000);
      ++i;
    }
  });

  m_connection->set_thread_owner();

  // it can be accessed directly (no other thread access thus object)
  m_state = Client_accepted;

  set_encoder(allocate_object<Protocol_encoder>(
      m_connection,
      std::bind(&Client::on_network_error, this, std::placeholders::_1),
      m_protocol_monitor));

  // pre-allocate the initial session
  // this is also needed for the srv_session to correctly report us to the
  // audit.log as in the Pre-authenticate state
  if (!create_session()) {
    m_close_reason = Close_reason::k_error;
    disconnect_and_trigger_close();

    return;
  }

  if (xpl::Plugin_system_variables::m_enable_hello_notice)
    m_encoder->send_notice(Frame_type::k_server_hello, Frame_scope::k_global,
                           "", true);
}

void Client::on_session_auth_success(Session_interface &) {
  // this is called from worker thread
  Client_state expected = Client_authenticating_first;
  m_state.compare_exchange_strong(expected, Client_running);
}

void Client::on_session_close(Session_interface &s MY_ATTRIBUTE((unused))) {
  log_debug("%s: Session %i removed", client_id(), s.session_id());

  // no more open sessions, disconnect
  disconnect_and_trigger_close();

  if (s.state_before_close() != ngs::Session_interface::k_authenticating) {
    ++xpl::Global_status_variables::instance().m_closed_sessions_count;
  }

  remove_client_from_server();
}

void Client::on_session_reset(Session_interface &s MY_ATTRIBUTE((unused))) {
  log_debug("%s: Resetting session %i", client_id(), s.session_id());

  if (!create_session()) {
    m_state = Client_closing;
    return;
  }
  m_state = Client_accepted_with_session;
  m_encoder->send_ok();
}

void Client::on_server_shutdown() {
  log_debug("%s: closing client because of shutdown (state: %i)", client_id(),
            m_state.load());
  std::shared_ptr<ngs::Session_interface> local_copy = m_session;

  if (local_copy) local_copy->on_kill();

  // XXX send a server shutdown notice
  disconnect_and_trigger_close();
}

Protocol_monitor_interface &Client::get_protocol_monitor() {
  return *m_protocol_monitor;
}

void Client::set_encoder(Protocol_encoder_interface *enc) {
  m_encoder = Memory_instrumented<Protocol_encoder_interface>::Unique_ptr(enc);
  m_encoder->get_flusher()->set_write_timeout(m_write_timeout);
}

Error_code Client::read_one_message(Message_request *out_message) {
  const auto decode_error =
      m_decoder.read_and_decode(out_message, get_idle_processing());

  if (decode_error.was_peer_disconnected()) {
    on_network_error(0);
    out_message->reset(nullptr);
    return {};
  }

  const auto io_error = decode_error.get_io_error();
  if (0 != io_error) {
    if (io_error == SOCKET_ETIMEDOUT || io_error == SOCKET_EAGAIN) {
      on_read_timeout();
    }

    if (EBADF != io_error) on_network_error(io_error);

    return {};
  }

  return decode_error.get_logic_error();
}

void Client::run(const bool skip_name_resolve) {
  try {
    on_client_addr(skip_name_resolve);
    on_accept();

    while (m_state != Client_closing && m_session) {
      Message_request request;
      Error_code error = read_one_message(&request);

      // read could took some time, thus lets recheck the state
      if (m_state == Client_closing) break;

      if (error) {
        // !message and !error = EOF
        if (error) m_encoder->send_result(Fatal(error));
        disconnect_and_trigger_close();
        break;
      }

      handle_message(request);
    }
  } catch (std::exception &e) {
    log_error(ER_XPLUGIN_FORCE_STOP_CLIENT, client_id(), e.what());
  }

  {
    MUTEX_LOCK(lock, server().get_client_exit_mutex());
    m_state = Client_closed;

    remove_client_from_server();
  }
}

void Client::set_write_timeout(const uint32_t write_timeout) {
  m_encoder->get_flusher()->set_write_timeout(write_timeout);
}

void Client::set_read_timeout(const uint32_t read_timeout) {
  m_decoder.set_read_timeout(read_timeout);
  m_read_timeout = read_timeout;
}

void Client::set_wait_timeout(const uint32_t wait_timeout) {
  m_decoder.set_wait_timeout(wait_timeout);
}

Waiting_for_io_interface *Client::get_idle_processing() {
  if (nullptr == m_session) {
    static details::No_idle_processing no_idle;

    return &no_idle;
  }

  return &m_session->get_notice_output_queue().get_callbacks_waiting_for_io();
}

bool Client::create_session() {
  std::shared_ptr<Session_interface> session(
      m_server.create_session(*this, *m_encoder, 1));
  if (!session) {
    log_warning(ER_XPLUGIN_FAILED_TO_CREATE_SESSION_FOR_CONN, client_id(),
                m_client_addr.c_str());
    m_encoder->send_result(
        Fatal(ER_OUT_OF_RESOURCES, "Could not allocate new session"));
    return false;
  }

  Error_code error(session->init());
  if (error) {
    log_warning(ER_XPLUGIN_FAILED_TO_INITIALIZE_SESSION, client_id(),
                error.message.c_str());
    m_encoder->send_result(error);
    return false;
  }

  {
    MUTEX_LOCK(lock_session_exit, get_session_exit_mutex());
    m_session = session;
  }
  return true;
}
}  // namespace ngs

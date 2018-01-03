/*
 * Copyright (c) 2015, 2017, Oracle and/or its affiliates. All rights reserved.
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

#include "plugin/x/ngs/include/ngs/interface/protocol_monitor_interface.h"
#include "plugin/x/ngs/include/ngs/protocol_encoder.h"
#ifndef WIN32
#include <arpa/inet.h>
#endif
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <algorithm>
#include <functional>
#include <sstream>

#include "plugin/x/ngs/include/ngs/capabilities/handler_auth_mech.h"
#include "plugin/x/ngs/include/ngs/capabilities/handler_client_interactive.h"
#include "plugin/x/ngs/include/ngs/capabilities/handler_readonly_value.h"
#include "plugin/x/ngs/include/ngs/capabilities/handler_tls.h"
#include "plugin/x/ngs/include/ngs/interface/server_interface.h"
#include "plugin/x/ngs/include/ngs/interface/session_interface.h"
#include "plugin/x/ngs/include/ngs/log.h"
#include "plugin/x/ngs/include/ngs/ngs_error.h"
#include "plugin/x/ngs/include/ngs/protocol/protocol_config.h"
#include "plugin/x/ngs/include/ngs/scheduler.h"
#include "plugin/x/ngs/include/ngs_common/operations_factory.h"

#include "plugin/x/src/xpl_global_status_variables.h"

#undef ERROR // Needed to avoid conflict with ERROR in mysqlx.pb.h
#include "plugin/x/ngs/include/ngs_common/protocol_protobuf.h"


using namespace ngs;

Client::Client(Connection_ptr connection,
               Server_interface &server,
               Client_id client_id,
               Protocol_monitor_interface &pmon,
               const Global_timeouts &timeouts)
: m_client_id(client_id),
  m_server(server),
  m_connection(connection),
  m_client_addr("n/c"),
  m_client_port(0),
  m_state(Client_invalid),
  m_removed(false),
  m_protocol_monitor(pmon),
  m_close_reason(Not_closing),
  m_msg_buffer(NULL),
  m_msg_buffer_size(0),
  m_supports_expired_passwords(false)
{
  my_snprintf(m_id, sizeof(m_id), "%llu", static_cast<ulonglong>(client_id));
  m_wait_timeout = timeouts.wait_timeout;
  m_read_timeout = timeouts.read_timeout;
  m_write_timeout = timeouts.write_timeout;
}


Client::~Client()
{
  log_debug("%s: Delete client", m_id);
  if (m_connection)
    m_connection->close();

  if (m_msg_buffer)
    ngs::free_array(m_msg_buffer);
}


ngs::chrono::time_point Client::get_accept_time() const
{
  return m_accept_time;
}

void Client::reset_accept_time()
{
  m_accept_time = chrono::now();
  m_server.restart_client_supervision_timer();
}


void Client::activate_tls()
{
  log_debug("%s: enabling TLS for client", client_id());

  const auto connect_timeout = chrono::to_seconds(
      m_server.get_config()->connect_timeout);

  const auto real_connect_timeout = std::min<uint32_t>(connect_timeout,
      m_read_timeout);

  if (m_server.ssl_context()->activate_tls(connection(), real_connect_timeout))
  {
    if (connection().options()->active_tls())
      session()->mark_as_tls_session();
  }
  else
  {
    log_warning("%s: Error during SSL handshake", client_id());
    disconnect_and_trigger_close();
  }
}


void Client::on_auth_timeout()
{
  m_close_reason = Close_connect_timeout;

  //XXX send an ERROR notice when it's available
  disconnect_and_trigger_close();
}


Capabilities_configurator *Client::capabilities_configurator()
{
  std::vector<Capability_handler_ptr> handlers;

  handlers.push_back(ngs::allocate_shared<Capability_tls>(ngs::ref(*this)));
  handlers.push_back(ngs::allocate_shared<Capability_auth_mech>(ngs::ref(*this)));

  handlers.push_back(ngs::allocate_shared<Capability_readonly_value>("doc.formats", "text"));

  handlers.push_back(ngs::allocate_shared<Capability_client_interactive>(ngs::ref(*this)));

  return ngs::allocate_object<Capabilities_configurator>(handlers);
}


void Client::get_capabilities(const Mysqlx::Connection::CapabilitiesGet&)
{
  ngs::Memory_instrumented<Capabilities_configurator>::Unique_ptr configurator(capabilities_configurator());
  ngs::Memory_instrumented<Mysqlx::Connection::Capabilities>::Unique_ptr caps(configurator->get());

  m_encoder->send_message(Mysqlx::ServerMessages::CONN_CAPABILITIES, *caps);
}


void Client::set_capabilities(const Mysqlx::Connection::CapabilitiesSet &setcap)
{
  ngs::Memory_instrumented<Capabilities_configurator>::Unique_ptr configurator(capabilities_configurator());
  Error_code error_code = configurator->prepare_set(setcap.capabilities());
  m_encoder->send_result(error_code);
  if (!error_code)
  {
    configurator->commit();
  }
}


void Client::handle_message(Request &request)
{
  ngs::shared_ptr<Session_interface> s(session());

  log_message_recv(request);

  if (m_state != Client_accepted && s)
  {
    // pass the message to the session
    s->handle_message(request);
    return;
  }

  Client_state expected_state = Client_accepted;

  // there is no session before authentication, so we handle the messages ourselves
  log_debug("%s: Client got message %i", client_id(), request.get_type());

  switch (request.get_type())
  {
    case Mysqlx::ClientMessages::CON_CLOSE:
      m_encoder->send_ok("bye!");
      m_close_reason = Close_normal;
      disconnect_and_trigger_close();
      break;

    case Mysqlx::ClientMessages::SESS_RESET:
      // no-op, since we're only going to get called here before session is authenticated
      break;

    case Mysqlx::ClientMessages::CON_CAPABILITIES_GET:
      get_capabilities(static_cast<const Mysqlx::Connection::CapabilitiesGet&>(*request.message()));
      break;

    case Mysqlx::ClientMessages::CON_CAPABILITIES_SET:
      set_capabilities(static_cast<const Mysqlx::Connection::CapabilitiesSet&>(*request.message()));
      break;

    case Mysqlx::ClientMessages::SESS_AUTHENTICATE_START:
      if (m_state.compare_exchange_strong(expected_state, Client_authenticating_first) &&
          server().is_running())
      {
        log_debug("%s: Authenticating client...", client_id());

        ngs::shared_ptr<Session_interface> s(session());
        // start redirecting incoming messages directly to the session
        if (s)
        {
          // forward the message to the pre-allocated session, rest of auth will be handled by the session
          s->handle_message(request);
        }
        break;
      }
      // Fall through.

    default:
      // invalid message at this time
      m_protocol_monitor.on_error_unknown_msg_type();
      log_info("%s: Invalid message %i received during client initialization", client_id(), request.get_type());
      m_encoder->send_result(ngs::Fatal(ER_X_BAD_MESSAGE, "Invalid message"));
      m_close_reason = Close_error;
      disconnect_and_trigger_close();
      break;
  }
}


void Client::disconnect_and_trigger_close()
{
  if (m_close_reason == Not_closing)
    m_close_reason = Close_normal;

  shutdown_connection();
}

void Client::on_read_timeout(int error_code, const std::string &message) {
  Mysqlx::Notice::Warning warning;
  const bool force_flush = true;

  m_close_reason = Close_read_timeout;
  warning.set_level(Mysqlx::Notice::Warning::ERROR);
  warning.set_code(ER_IO_READ_ERROR);
  warning.set_msg("IO Read error: (" + std::to_string(error_code) +
      ", " + message + ") read_timeout exceeded");
  std::string warning_data;
  warning.SerializeToString(&warning_data);
  m_encoder->send_notice(Frame_type::WARNING, Frame_scope::GLOBAL,
      warning_data, force_flush);
}

// this will be called on socket errors, but also when halt_and_wait() is called
// which will shutdown the socket for reading and trigger a eof
// (meaning, closed for reads, but writes would still be ok)
void Client::on_network_error(int error)
{
  if (error == SOCKET_ETIMEDOUT || error == SOCKET_EAGAIN)
  {
    ++xpl::Global_status_variables::instance().m_aborted_clients;
    if (m_close_reason != Close_read_timeout)
      m_close_reason = Close_write_timeout;
  }
  if (error == 0)
    log_debug("%s: peer disconnected (state %i)", client_id(), m_state.load());
  else
    log_debug("%s: network error %i (state %i)", client_id(), error, m_state.load());
  if (m_close_reason == Not_closing && m_state != Client_closing && error != 0)
    m_close_reason = Close_net_error;

  m_state.exchange(Client_closing);

  if (m_session && (Client_authenticating_first == m_state || Client_running == m_state))
  {
    // trigger all sessions to close and stop whatever they're doing
    log_debug("%s: killing session", client_id());
    if (Session_interface::Closing != m_session->state())
      server().get_worker_scheduler()->post_and_wait(ngs::bind(&Client::on_kill, this, ngs::ref(*m_session)));
  }
}


void Client::on_kill(Session_interface&)
{
  m_session->on_kill();
}


void Client::remove_client_from_server()
{
  if (false == m_removed.exchange(true))
    m_server.on_client_closed(*this);
}


void Client::on_client_addr(const bool skip_resolve)
{
  m_client_addr.resize(INET6_ADDRSTRLEN);

  switch(m_connection->connection_type())
  {
  case Connection_tcpip:
  {
    m_connection->peer_address(m_client_addr, m_client_port);
  }
  break;

  case Connection_namedpipe:
  case Connection_unixsocket: // fall through
    m_client_host = "localhost";
    return;

  default:
    return;
  }

  // turn IP to hostname for auth uses
  if (skip_resolve)
    return;

  m_client_host = "";

  try
  {
    m_client_host = resolve_hostname();
  }
  catch(...)
  {
    m_close_reason = Close_reject;
    disconnect_and_trigger_close();

    throw;
  }
}

void Client::on_accept()
{
  log_debug("%s: Accepted client connection from %s", client_id(), client_address());

  m_connection->set_socket_thread_owner();

  // it can be accessed directly (no other thread access thus object)
  m_state = Client_accepted;

  set_encoder(ngs::allocate_object<Protocol_encoder>(m_connection,
        ngs::bind(&Client::on_network_error, this, ngs::placeholders::_1),
        ngs::ref(m_protocol_monitor)));

  // pre-allocate the initial session
  // this is also needed for the srv_session to correctly report us to the audit.log as in the Pre-authenticate state
  ngs::shared_ptr<Session_interface> session(m_server.create_session(*this, *m_encoder, 1));
  if (!session)
  {
    log_warning("%s: Error creating session for connection from %s", client_id(), m_client_addr.c_str());
    m_encoder->send_init_error(ngs::Fatal(ER_OUT_OF_RESOURCES, "Could not allocate session"));
  }
  else
  {
    ngs::Error_code error(session->init());
    if (error)
    {
      log_warning("%s: Error initializing session for connection: %s", client_id(), error.message.c_str());
      m_encoder->send_result(error);
      session.reset();
    }
    else
      m_session = session;
  }
  if (!session)
  {
    m_close_reason = Close_error;
    disconnect_and_trigger_close();
  }
}

void Client::on_session_auth_success(Session_interface&)
{
  // this is called from worker thread
  Client_state expected = Client_authenticating_first;
  m_state.compare_exchange_strong(expected, Client_running);
}


void Client::on_session_close(Session_interface &s MY_ATTRIBUTE((unused)))
{
  log_debug("%s: Session %i removed", client_id(), s.session_id());

  // no more open sessions, disconnect
  if (m_close_reason == Not_closing)
    m_close_reason = Close_normal;

  m_state = Client_closing;

  shutdown_connection();

  remove_client_from_server();
}


void Client::on_session_reset(Session_interface &s MY_ATTRIBUTE((unused)))
{
  log_debug("%s: Resetting session %i", client_id(), s.session_id());

  m_state = Client_accepted_with_session;
  ngs::shared_ptr<Session_interface> session(m_server.create_session(*this, *m_encoder, 1));
  if (!session)
  {
    log_warning("%s: Error creating session for connection from %s", client_id(), m_client_addr.c_str());
    m_encoder->send_result(ngs::Fatal(ER_OUT_OF_RESOURCES, "Could not allocate new session"));
    m_state = Client_closing;
  }
  else
  {
    ngs::Error_code error(session->init());
    if (error)
    {
      log_warning("%s: Error initializing session for connection: %s", client_id(), error.message.c_str());
      m_encoder->send_result(error);
      session.reset();
      m_state = Client_closing;
    }
    else
    {
      m_session = session;
      m_encoder->send_ok();
    }
  }
}


void Client::on_server_shutdown()
{
  log_info("%s: closing client because of shutdown (state: %i)", client_id(), m_state.load());
  //XXX send a server shutdown notice
  disconnect_and_trigger_close();
}

Protocol_monitor_interface &Client::get_protocol_monitor()
{
  return m_protocol_monitor;
}

void Client::set_encoder(ngs::Protocol_encoder_interface* enc) {
  m_encoder =
      ngs::Memory_instrumented<Protocol_encoder_interface>::Unique_ptr(enc);
  m_encoder->set_write_timeout(m_write_timeout);
}

void Client::get_last_error(int *out_error_code, std::string *out_message)
{
  ngs::Operations_factory operations_factory;
  System_interface::Shared_ptr system_interface(operations_factory.create_system_interface());

  system_interface->get_socket_error_and_message(*out_error_code, *out_message);
}

void Client::shutdown_connection()
{
  m_state = Client_closing;

  if (m_connection->shutdown(Connection_vio::Shutdown_recv) < 0)
  {
    int out_err;
    std::string out_strerr;

    get_last_error(&out_err, &out_strerr);
    log_debug("%s: connection shutdown error %s (%i)",
              client_id(),
              out_strerr.c_str(),
              out_err);
  }
}

Request *Client::read_one_message(Error_code &ret_error)
{
  union
  {
    char buffer[4];                             // Must be properly aligned
    longlong dummy;
  };
  uint32_t msg_size;

  // untill we get another message to process we mark the connection as idle (for PSF)
  m_connection->mark_idle();

  // read the frame
  ssize_t nread = m_connection->read(buffer, 4, m_wait_timeout);
  m_connection->mark_active();

  if (nread == 0) // EOF
  {
    on_network_error(0);
    return NULL;
  }
  if (nread < 0)
  {
    int out_err;
    std::string out_strerr;
    get_last_error(&out_err, &out_strerr);

    if (out_err == SOCKET_ETIMEDOUT || out_err == SOCKET_EAGAIN)
    {
      on_read_timeout(out_err, out_strerr);
    }

    if (!(out_err == EBADF && m_close_reason == Close_connect_timeout))
    {
      log_info("%s: ERROR reading from socket %s (%i)",
               client_id(),
               out_strerr.c_str(),
               out_err);
      on_network_error(out_err);
    }
    return NULL;
  }

  m_protocol_monitor.on_receive(static_cast<long>(nread));

#ifdef WORDS_BIGENDIAN
  std::swap(buffer[0], buffer[3]);
  std::swap(buffer[1], buffer[2]);
#endif
  const uint32_t* pdata = (uint32_t*)(buffer);
  msg_size = *pdata;

  if (msg_size > m_server.get_config()->max_message_size)
  {
    log_warning("%s: Message of size %u received, exceeding the limit of %i", client_id(), msg_size, m_server.get_config()->max_message_size);
    // invalid message size
    // Don't send error, just abort connection
    //ret_error = Fatal(ER_X_BAD_MESSAGE, "Message too large");
    return NULL;
  }

  if (0 == msg_size)
  {
    ret_error = Error(ER_X_BAD_MESSAGE, "Messages without payload are not supported");
    return NULL;
  }

  if (m_msg_buffer_size < msg_size)
  {
    m_msg_buffer_size = msg_size;
    ngs::reallocate_array(m_msg_buffer, m_msg_buffer_size, KEY_memory_x_recv_buffer);
  }

  nread = m_connection->read(&m_msg_buffer[0], msg_size, m_read_timeout);
  if (nread == 0) // EOF
  {
    log_info("%s: peer disconnected while reading message body", client_id());
    on_network_error(0);
    return NULL;
  }

  if (nread < 0)
  {
    int out_err;
    std::string out_strerr;

    get_last_error(&out_err, &out_strerr);
    if (out_err == SOCKET_ETIMEDOUT || out_err == SOCKET_EAGAIN)
    {
      on_read_timeout(out_err, out_strerr);
    }

    log_info("client_id:%s - %s while reading from socket, closing connection",
             client_id(),
             out_strerr.c_str());

    on_network_error(out_err);
    return NULL;
  }

  m_protocol_monitor.on_receive(static_cast<long>(nread));

  int8_t type = (int8_t)m_msg_buffer[0];
  Request_unique_ptr request(ngs::allocate_object<Request>(type));

  if (msg_size > 1)
    request->buffer(&m_msg_buffer[1], msg_size - 1);

  ret_error = m_decoder.parse(*request);

  return request.release();
}


void Client::run(const bool skip_name_resolve)
{
  try
  {
    on_client_addr(skip_name_resolve);
    on_accept();

    while (m_state != Client_closing && m_session)
    {
      Error_code error;
      Request_unique_ptr message(read_one_message(error));

      // read could took some time, thus lets recheck the state
      if (m_state == Client_closing)
        break;

      if (error || !message)
      {
        // !message and !error = EOF
        if (error)
          m_encoder->send_result(ngs::Fatal(error));
        disconnect_and_trigger_close();
        break;
      }

      handle_message(*message);
    }
  }
  catch (std::exception &e)
  {
    log_error("%s: Force stopping client because exception occurred: %s", client_id(), e.what());
  }

  {
    MUTEX_LOCK(lock, server().get_client_exit_mutex());
    m_state = Client_closed;

    remove_client_from_server();
  }
}

void Client::set_write_timeout(const uint32_t write_timeout) {
  m_encoder->set_write_timeout(write_timeout);
}

void Client::set_read_timeout(const uint32_t read_timeout) {
  m_read_timeout = read_timeout;
}

void Client::set_wait_timeout(const uint32_t wait_timeout) {
  m_wait_timeout = wait_timeout;
}

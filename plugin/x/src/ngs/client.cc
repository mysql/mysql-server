/*
 * Copyright (c) 2015, 2024, Oracle and/or its affiliates.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is designed to work with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have either included with
 * the program or referenced in the documentation.
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

#include "plugin/x/src/ngs/client.h"

#ifndef WIN32
#include <arpa/inet.h>
#endif
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <algorithm>
#include <cinttypes>
#include <functional>
#include <sstream>
#include <utility>
#include <vector>

#include "my_dbug.h"     // NOLINT(build/include_subdir)
#include "my_macros.h"   // NOLINT(build/include_subdir)
#include "my_systime.h"  // my_sleep NOLINT(build/include_subdir)

#include "plugin/x/protocol/stream/compression/compression_algorithm_lz4.h"
#include "plugin/x/protocol/stream/compression/compression_algorithm_zlib.h"
#include "plugin/x/protocol/stream/compression/compression_algorithm_zstd.h"
#include "plugin/x/src/capabilities/capability_compression.h"
#include "plugin/x/src/capabilities/handler_auth_mech.h"
#include "plugin/x/src/capabilities/handler_client_interactive.h"
#include "plugin/x/src/capabilities/handler_connection_attributes.h"
#include "plugin/x/src/capabilities/handler_readonly_value.h"
#include "plugin/x/src/capabilities/handler_tls.h"
#include "plugin/x/src/helper/multithread/xsync_point.h"
#include "plugin/x/src/interface/protocol_monitor.h"
#include "plugin/x/src/interface/server.h"
#include "plugin/x/src/interface/session.h"
#include "plugin/x/src/interface/ssl_context.h"
#include "plugin/x/src/ngs/log.h"
#include "plugin/x/src/ngs/protocol/protocol_config.h"
#include "plugin/x/src/ngs/protocol/protocol_protobuf.h"
#include "plugin/x/src/ngs/protocol_encoder.h"
#include "plugin/x/src/ngs/scheduler.h"
#include "plugin/x/src/notices.h"
#include "plugin/x/src/operations_factory.h"
#include "plugin/x/src/variables/xpl_global_status_variables.h"
#include "plugin/x/src/xpl_error.h"
#include "sql/debug_sync.h"

namespace ngs {

class Client::Client_idle_reporting : public xpl::iface::Waiting_for_io {
 public:
  using Waiting_for_io = xpl::iface::Waiting_for_io;
  explicit Client_idle_reporting(Client *client,
                                 Waiting_for_io *global_idle_reporting)
      : m_client(client), m_global_idle_reporting(global_idle_reporting) {}

  bool has_to_report_idle_waiting() override {
    m_global_need_reporting =
        m_global_idle_reporting &&
        m_global_idle_reporting->has_to_report_idle_waiting();

    return true;
  }

  bool on_idle_or_before_read() override {
    DBUG_TRACE;
    const auto state = m_client->get_state();

    if (state == xpl::iface::Client::State::k_running &&
        m_client->session()->data_context().is_killed()) {
      // Try to set the reason now, decide make the decision
      // about sending a notice, later on.
      m_client->set_close_reason_if_non_fatal(Close_reason::k_kill);
      return false;
    }

    if (state == Client::State::k_closed || state == Client::State::k_closing)
      return false;

    if (m_global_need_reporting && !m_client->protocol().is_building_row())
      return m_global_idle_reporting->on_idle_or_before_read();

    return true;
  }

 private:
  Client *m_client;
  Waiting_for_io *m_global_idle_reporting;
  bool m_global_need_reporting = false;
};

Client::Client(std::shared_ptr<xpl::iface::Vio> connection,
               xpl::iface::Server *server, Client_id client_id,
               xpl::iface::Protocol_monitor *pmon)
    : m_client_id(client_id),
      m_server(server),
      m_idle_reporting(new Client_idle_reporting(this, nullptr)),
      m_connection(connection),
      m_config(std::make_shared<Protocol_config>(m_server->get_config())),
      m_dispatcher(this),
      m_decoder(&m_dispatcher, m_connection, pmon, m_config),
      m_client_addr("n/c"),
      m_client_port(0),
      m_state(State::k_invalid),
      m_state_when_reason_changed(State::k_invalid),
      m_removed(false),
      m_protocol_monitor(pmon),
      m_session_exit_mutex(KEY_mutex_x_client_session_exit),
      m_msg_buffer(nullptr),
      m_msg_buffer_size(0),
      m_supports_expired_passwords(false) {
  snprintf(m_id, sizeof(m_id), "%llu", static_cast<ulonglong>(client_id));

  const auto &timeouts = m_config->m_global->m_timeouts;

  set_wait_timeout(timeouts.m_wait_timeout);
  set_write_timeout(timeouts.m_write_timeout);
  set_read_timeout(timeouts.m_read_timeout);
}

Client::~Client() {
  log_debug("%s: Delete client", m_id);
  if (m_connection) m_connection->shutdown();

  if (m_msg_buffer) free_array(m_msg_buffer);
}

xpl::chrono::Time_point Client::get_accept_time() const {
  return m_accept_time;
}

void Client::reset_accept_time() { m_accept_time = xpl::chrono::now(); }

void Client::activate_tls() {
  log_debug("%s: enabling TLS for client", client_id());

  const auto connect_timeout =
      xpl::chrono::to_seconds(m_server->get_config()->connect_timeout);

  const auto real_connect_timeout =
      std::min<uint32_t>(connect_timeout, m_read_timeout);

  if (m_server->ssl_context()->activate_tls(&connection(),
                                            real_connect_timeout)) {
    session()->mark_as_tls_session();
  } else {
    log_debug("%s: Error during SSL handshake", client_id());
    disconnect_and_trigger_close();
  }
}

void Client::on_auth_timeout() {
  set_close_reason_if_non_fatal(Close_reason::k_connect_timeout);

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

  handlers.push_back(ngs::allocate_shared<xpl::Capability_compression>(this));

  return ngs::allocate_object<xpl::Capabilities_configurator>(handlers);
}

void Client::get_capabilities(const Mysqlx::Connection::CapabilitiesGet &) {
  Memory_instrumented<xpl::Capabilities_configurator>::Unique_ptr configurator(
      capabilities_configurator());
  Memory_instrumented<Mysqlx::Connection::Capabilities>::Unique_ptr caps(
      configurator->get());

  m_encoder->send_protobuf_message(Mysqlx::ServerMessages::CONN_CAPABILITIES,
                                   *caps);
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

bool Client::handle_session_connect_attr_set(
    const ngs::Message_request &command) {
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

void Client::handle_message(Message_request *request) {
  auto s(session());

  log_message_recv(m_client_id, request);

  if (m_state == State::k_accepted_with_session &&
      request->get_message_type() ==
          Mysqlx::ClientMessages::CON_CAPABILITIES_SET) {
    handle_session_connect_attr_set(*request);
    return;
  }

  if (m_state != State::k_accepted && s) {
    // pass the message to the session
    s->handle_message(*request);
    return;
  }

  State expected_state = State::k_accepted;

  // there is no session before authentication, so we handle the messages
  // ourselves
  log_debug("%s: Client got message %i", client_id(),
            static_cast<int>(request->get_message_type()));

  switch (request->get_message_type()) {
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
          *request->get_message()));
      break;

    case Mysqlx::ClientMessages::CON_CAPABILITIES_SET:
      set_capabilities(static_cast<const Mysqlx::Connection::CapabilitiesSet &>(
          *request->get_message()));
      break;

    case Mysqlx::ClientMessages::SESS_AUTHENTICATE_START:
      if (m_state.compare_exchange_strong(expected_state,
                                          State::k_authenticating_first) &&
          server().is_running()) {
        log_debug("%s: Authenticating client...", client_id());

        // start redirecting incoming messages directly to the session
        if (s) {
          // forward the message to the pre-allocated session, rest of auth will
          // be handled by the session
          s->handle_message(*request);
        }
        break;
      }
      [[fallthrough]];

    default:
      // invalid message at this time
      m_protocol_monitor->on_error_unknown_msg_type();
      log_debug("%s: Invalid message %i received during client initialization",
                client_id(), request->get_message_type());
      m_encoder->send_result(Fatal(ER_X_BAD_MESSAGE, "Invalid message"));
      set_close_reason_if_non_fatal(Close_reason::k_error);
      disconnect_and_trigger_close();
      break;
  }
}

void Client::set_close_reason_if_non_fatal(const Close_reason new_reason) {
  Close_reason expected_reason = Close_reason::k_normal;
  if (!m_close_reason.compare_exchange_strong(expected_reason, new_reason)) {
    expected_reason = Close_reason::k_none;
    if (!m_close_reason.compare_exchange_strong(expected_reason, new_reason))
      return;
  }

  m_state_when_reason_changed = m_state.load();
}

void Client::disconnect_and_trigger_close() {
  set_close_reason_if_non_fatal(Close_reason::k_normal);

  if (m_session) m_session->get_notice_output_queue().encode_queued_items(true);

  m_state = State::k_closing;

  m_connection->shutdown();
}

const char *Client::client_hostname_or_address() const {
  if (!m_client_host.empty()) return m_client_host.c_str();

  return m_client_addr.c_str();
}

void Client::on_read_timeout() {
  set_close_reason_if_non_fatal(Close_reason::k_read_timeout);
  queue_up_disconnection_notice(ngs::Error_code(
      ER_IO_READ_ERROR, "IO Read error: read_timeout exceeded"));
}

// this will be called on socket errors, but also when halt_and_wait() is called
// which will shutdown the socket for reading and trigger a eof
// (meaning, closed for reads, but writes would still be ok)
void Client::on_network_error(const int error) {
  if (error == SOCKET_ETIMEDOUT || error == SOCKET_EAGAIN) {
    set_close_reason_if_non_fatal(Close_reason::k_write_timeout);
  }

  log_debug("%s, %" PRIu32 ": on_network_error(error:%i)", client_id(),
            static_cast<uint32_t>(m_state.load()), error);

  if (m_state != State::k_closing && error != 0)
    set_close_reason_if_non_fatal(Close_reason::k_net_error);

  m_state.exchange(State::k_closing);
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

    case Close_reason::k_server_shutdown:
    case Close_reason::k_kill:
    default:
      return;
  }
}

void Client::remove_client_from_server() {
  if (false == m_removed.exchange(true)) {
    update_counters();
    m_server->on_client_closed(*this);
  }
}

void Client::on_client_addr() {
  m_client_addr.resize(INET6_ADDRSTRLEN);

  switch (m_connection->get_type()) {
    case xpl::Connection_tcpip: {
      m_connection->peer_addr(&m_client_addr, &m_client_port);
    } break;

    case xpl::Connection_namedpipe:
    case xpl::Connection_unixsocket:
      [[fallthrough]];
      m_client_host = "localhost";
      return;

    default:
      return;
  }

  // turn IP to hostname for auth uses
  const bool skip_resolve = xpl::Plugin_system_variables::get_system_variable(
                                "skip_name_resolve") == "ON";
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
  DBUG_TRACE;
  log_debug("%s: Accepted client connection from %s (sock:%i)", client_id(),
            client_address(), m_connection->get_fd());

  DBUG_EXECUTE_IF("client_accept_timeout", {
    int32_t i = 0;
    const int32_t max_iterations = 1000;
    while (m_server->is_running() && i < max_iterations) {
      my_sleep(10000);
      ++i;
    }
  });

  XSYNC_POINT_CHECK(XSYNC_WAIT("gr_notice_bug_client_accept"),
                    XSYNC_WAKE("gr_notice_bug_broker_dispatch"));

  m_connection->set_thread_owner();

  auto expected = State::k_invalid;
  m_state.compare_exchange_strong(expected, State::k_accepted);

  set_encoder(ngs::allocate_object<Protocol_encoder>(
      m_connection,
      std::bind(&Client::on_network_error, this, std::placeholders::_1),
      m_protocol_monitor, &m_memory_block_pool));

  // pre-allocate the initial session
  // this is also needed for the srv_session to correctly report us to the
  // audit.log as in the Pre-authenticate state
  if (!create_session()) {
    m_close_reason = Close_reason::k_error;
    disconnect_and_trigger_close();

    return;
  }

  if (xpl::Plugin_system_variables::m_enable_hello_notice)
    m_encoder->send_notice(xpl::iface::Frame_type::k_server_hello,
                           xpl::iface::Frame_scope::k_global, "", true);
}

void Client::on_session_auth_success(xpl::iface::Session *) {
  log_debug("%s: on_session_auth_success", client_id());
  // this is called from worker thread
  State expected = State::k_authenticating_first;
  m_state.compare_exchange_strong(expected, State::k_running);

  if (Compression_algorithm::k_none != m_cached_compression_algorithm) {
    Compression_style style = m_cached_combine_msg
                                  ? Compression_style::k_group
                                  : Compression_style::k_multiple;

    if (m_cached_max_msg == 1) {
      style = Compression_style::k_single;
    }

    get_protocol_compression_or_install_it()->set_compression_options(
        m_cached_compression_algorithm, style, m_cached_max_msg,
        m_cached_compression_level);

    m_config->m_compression_algorithm = m_cached_compression_algorithm;
    m_config->m_compression_level = m_cached_compression_level;
  }
}

void Client::on_session_close(xpl::iface::Session *s [[maybe_unused]]) {
  log_debug("%s: Session %i removed", client_id(), s->session_id());

  // no more open sessions, disconnect
  disconnect_and_trigger_close();
  remove_client_from_server();
}

void Client::on_session_reset(xpl::iface::Session *s [[maybe_unused]]) {
  log_debug("%s: Resetting session %i", client_id(), s->session_id());

  if (!create_session()) {
    m_state = State::k_closing;
    return;
  }
  m_state = State::k_accepted_with_session;
  m_encoder->send_ok();
}

void Client::on_server_shutdown() {
  DBUG_TRACE;
  log_debug("%s: closing client because of shutdown (state: %" PRIu32 ")",
            client_id(), static_cast<uint32_t>(m_state.load()));
  if (m_session) {
    if (m_state != State::k_closed) {
      set_close_reason_if_non_fatal(Close_reason::k_server_shutdown);
      m_state = State::k_closing;
    }

    m_session->on_close(xpl::iface::Session::Close_flags::k_update_old_state);
  }
}

void Client::kill() {
  DBUG_TRACE;
  if (m_session) {
    if (m_state != State::k_closed) {
      set_close_reason_if_non_fatal(Close_reason::k_kill);
      m_state = State::k_closing;
    }
    m_session->on_kill();
  }
}

xpl::iface::Protocol_monitor &Client::get_protocol_monitor() {
  return *m_protocol_monitor;
}

void Client::set_encoder(xpl::iface::Protocol_encoder *enc) {
  m_encoder.reset(enc);
  m_encoder->get_flusher()->set_write_timeout(m_write_timeout);

  if (m_session) m_session->set_proto(m_encoder.get());
}

Error_code Client::read_one_message_and_dispatch() {
  DBUG_TRACE;
  const auto decode_error = m_decoder.read_and_decode(get_idle_processing());

  if (decode_error.was_peer_disconnected()) {
    on_network_error(0);
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

void Client::run() {
  try {
    on_client_addr();
    on_accept();

    while (m_state != State::k_closing && m_session) {
      Error_code error = read_one_message_and_dispatch();

      // read could took some time, thus lets recheck the state
      if (m_state == State::k_closing) break;

      // Error generated by decoding
      // not by request-response model
      if (error) {
        // !message and !error = EOF
        m_encoder->send_result(Fatal(error));
        disconnect_and_trigger_close();
        break;
      }
    }
  } catch (std::exception &e) {
    log_error(ER_XPLUGIN_FORCE_STOP_CLIENT, client_id(), e.what());
  }

  if (m_session) {
    queue_up_disconnection_notice_if_necessary();
    m_session->get_notice_output_queue().encode_queued_items(true);
  }

  {
    MUTEX_LOCK(lock, server().get_client_exit_mutex());
    m_state = State::k_closed;

    remove_client_from_server();
  }
}

void Client::set_write_timeout(const uint32_t write_timeout) {
  if (m_encoder) {
    m_encoder->get_flusher()->set_write_timeout(write_timeout);
  }
  m_write_timeout = write_timeout;
}

void Client::set_read_timeout(const uint32_t read_timeout) {
  m_decoder.set_read_timeout(read_timeout);
  m_read_timeout = read_timeout;
}

void Client::set_wait_timeout(const uint32_t wait_timeout) {
  m_decoder.set_wait_timeout(wait_timeout);
}

xpl::iface::Waiting_for_io *Client::get_idle_processing() {
  return m_idle_reporting.get();
}

Protocol_encoder_compression *Client::get_protocol_compression_or_install_it() {
  if (!m_is_compression_encoder_injected) {
    m_is_compression_encoder_injected = true;
    auto encoder = ngs::allocate_object<Protocol_encoder_compression>(
        std::move(m_encoder), m_protocol_monitor,
        std::bind(&Client::on_network_error, this, std::placeholders::_1),
        &m_memory_block_pool);
    set_encoder(encoder);
  }

  return reinterpret_cast<Protocol_encoder_compression *>(m_encoder.get());
}

void Client::configure_compression_opts(
    const Compression_algorithm algo, const int64_t max_msg, const bool combine,
    const xpl::Optional_value<int64_t> &level) {
  m_cached_compression_algorithm = algo;
  m_cached_max_msg = max_msg;
  m_cached_combine_msg = combine;
  m_cached_compression_level = get_adjusted_compression_level(algo, level);
}

bool Client::create_session() {
  std::shared_ptr<xpl::iface::Session> session(
      m_server->create_session(this, m_encoder.get(), 1));
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
    error.severity = Error_code::FATAL;
    m_encoder->send_result(error);
    return false;
  }

  // Prolong the life time of old session object (m_session),
  // in a way that object underhood is released after
  // unlocking "session-exit-mutex".
  std::shared_ptr<xpl::iface::Session> keep_alive = m_session;

  {
    MUTEX_LOCK(lock_session_exit, get_session_exit_mutex());
#if defined(ENABLED_DEBUG_SYNC)
    if (m_session) {
      DEBUG_SYNC(m_session->get_thd(), "syncpoint_create_session_locked");
    }
#endif  // defined(ENABLED_DEBUG_SYNC)
    m_session = session;

    m_idle_reporting.reset(new Client_idle_reporting(
        this,
        m_session->get_notice_output_queue().get_callbacks_waiting_for_io()));
  }

  return true;
}

namespace {
int32_t adjust_level(const xpl::Optional_value<int64_t> &level,
                     const int32_t default_, const int32_t min,
                     const int32_t max) {
  if (!level.has_value()) return default_ > max ? max : default_;
  if (level.value() < min) return min;
  if (level.value() > max) return max;
  return level.value();
}
}  // namespace

int32_t Client::get_adjusted_compression_level(
    const Compression_algorithm algo,
    const xpl::Optional_value<int64_t> &level) const {
  using Variables = xpl::Plugin_system_variables;
  switch (algo) {
    case Compression_algorithm::k_deflate:
      return adjust_level(
          level, *Variables::m_deflate_default_compression_level.value(),
          protocol::Compression_algorithm_zlib::get_level_min(),
          *Variables::m_deflate_max_client_compression_level.value());

    case Compression_algorithm::k_lz4:
      return adjust_level(
          level, *Variables::m_lz4_default_compression_level.value(),
          protocol::Compression_algorithm_lz4::get_level_min(),
          *Variables::m_lz4_max_client_compression_level.value());

    case Compression_algorithm::k_zstd:
      return adjust_level(
          level.has_value() && level.value() == 0
              ? xpl::Optional_value<int64_t>(1)
              : level,
          *Variables::m_zstd_default_compression_level.value(),
          protocol::Compression_algorithm_zstd::get_level_min(),
          *Variables::m_zstd_max_client_compression_level.value());

    case Compression_algorithm::k_none:  // fall-through
    default: {
    }
  }
  return 1;
}

void Client::queue_up_disconnection_notice(const Error_code &error) {
  auto notice = std::make_shared<Notice_descriptor>(
      Notice_type::k_warning,
      xpl::notices::serialize_warning(xpl::iface::Warning_level::k_error,
                                      error.error, error.message));
  m_session->get_notice_output_queue().emplace(notice);
}

void Client::queue_up_disconnection_notice_if_necessary() {
  if (State::k_running == m_state_when_reason_changed) {
    switch (m_close_reason) {
      case Close_reason::k_server_shutdown:
        queue_up_disconnection_notice(SQLError(ER_SERVER_SHUTDOWN));
        break;
      case Close_reason::k_kill:
        if (m_session->get_status_variables().m_fatal_errors_sent.load() == 0)
          queue_up_disconnection_notice(SQLError(ER_SESSION_WAS_KILLED));
        break;
      default: {
      }
    }
  }
}

}  // namespace ngs

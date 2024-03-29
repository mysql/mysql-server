/*
 * Copyright (c) 2015, 2023, Oracle and/or its affiliates.
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

#ifndef PLUGIN_X_SRC_CLIENT_H_
#define PLUGIN_X_SRC_CLIENT_H_

#include <atomic>
#include <memory>
#include <string>

#include "plugin/x/src/capabilities/configurator.h"
#include "plugin/x/src/helper/chrono.h"
#include "plugin/x/src/helper/multithread/mutex.h"
#include "plugin/x/src/interface/client.h"
#include "plugin/x/src/interface/protocol_encoder.h"
#include "plugin/x/src/interface/protocol_monitor.h"
#include "plugin/x/src/interface/server.h"
#include "plugin/x/src/interface/session.h"
#include "plugin/x/src/interface/vio.h"
#include "plugin/x/src/ngs/memory.h"
#include "plugin/x/src/ngs/protocol/message.h"
#include "plugin/x/src/ngs/protocol_decoder.h"
#include "plugin/x/src/ngs/protocol_encoder_compression.h"

#ifndef WIN32
#include <netinet/in.h>
#endif

namespace xpl {

class Client : public iface::Client {
 public:
  Client(std::shared_ptr<iface::Vio> connection, iface::Server *server,
         Client_id client_id, iface::Protocol_monitor *pmon);
  ~Client() override;

  Mutex &get_session_exit_mutex() override { return m_session_exit_mutex; }
  iface::Session *session() override { return m_session.get(); }
  std::shared_ptr<iface::Session> session_shared_ptr() const override {
    return m_session;
  }

 public:  // impl iface::Client
  void run() override;

  void activate_tls() override;

  void reset_accept_time() override;

  void on_auth_timeout() override;
  void on_server_shutdown() override;
  void kill() override;

  iface::Server &server() const override { return *m_server; }
  iface::Protocol_encoder &protocol() const override { return *m_encoder; }
  iface::Vio &connection() const override { return *m_connection; }

  void on_session_auth_success(iface::Session *s) override;
  void on_session_close(iface::Session *s) override;
  void on_session_reset(iface::Session *s) override;

  void disconnect_and_trigger_close() override;
  bool is_handler_thd(const THD *) const override;

  const char *client_address() const override { return m_client_addr.c_str(); }
  const char *client_hostname() const override { return m_client_host.c_str(); }
  const char *client_hostname_or_address() const override;
  const char *client_id() const override { return m_id; }
  Client_id client_id_num() const override { return m_client_id; }
  int client_port() const override { return m_client_port; }

  Client::State get_state() const override { return m_state.load(); }
  chrono::Time_point get_accept_time() const override;

  iface::Waiting_for_io *get_idle_processing() override;

  bool supports_expired_passwords() const override {
    return m_supports_expired_passwords;
  }
  void set_supports_expired_passwords(bool flag) override {
    m_supports_expired_passwords = flag;
  }

  bool is_interactive() const override { return m_is_interactive; }
  void set_is_interactive(const bool flag) override;

  void set_wait_timeout(const uint32_t) override;
  void set_read_timeout(const uint32_t) override;
  void set_write_timeout(const uint32_t) override;

  bool handle_session_connect_attr_set(const ngs::Message_request &command);

  void configure_compression_opts(
      const ngs::Compression_algorithm algo, const int64_t max_msg,
      const bool combine, const Optional_value<int64_t> &level) override;

  void handle_message(ngs::Message_request *message) override;

  std::string get_status_ssl_cipher_list() const;
  std::string get_status_compression_algorithm() const;
  std::string get_status_compression_level() const;

  ngs::Error_code read_one_message_and_dispatch();
  void set_encoder(iface::Protocol_encoder *enc);
  void set_session(iface::Session *session) { m_session.reset(session); }
  void set_idle_reporting(iface::Waiting_for_io *reporter) {
    m_idle_reporting.reset(reporter);
  }

 private:
  class Message_dispatcher
      : public ngs::Message_decoder::Message_dispatcher_interface {
   public:
    explicit Message_dispatcher(iface::Client *client) : m_client(client) {}

    void handle(ngs::Message_request *message) override {
      m_client->handle_message(message);
    }

   private:
    iface::Client *m_client;
  };

  class Client_idle_reporting;

 protected:
  char m_id[2 + sizeof(Client_id) * 2 + 1];  // 64bits in hex, plus 0x plus \0
  Client_id m_client_id;
  iface::Server *m_server;

  std::unique_ptr<iface::Waiting_for_io> m_idle_reporting;
  std::shared_ptr<iface::Vio> m_connection;
  std::shared_ptr<ngs::Protocol_config> m_config;
  // TODO(lkotula): benchmark m_memory_block_pool as global in Xpl (shouldn't be
  // in review)
  ngs::Memory_block_pool m_memory_block_pool{{10, k_minimum_page_size}};
  Message_dispatcher m_dispatcher;
  ngs::Protocol_decoder m_decoder;

  chrono::Time_point m_accept_time;

  ngs::Memory_instrumented<iface::Protocol_encoder>::Unique_ptr m_encoder;
  std::string m_client_addr;
  std::string m_client_host;
  uint16_t m_client_port;
  std::atomic<Client::State> m_state;
  std::atomic<Client::State> m_state_when_reason_changed;
  std::atomic<bool> m_removed;

  std::shared_ptr<iface::Session> m_session;

  iface::Protocol_monitor *m_protocol_monitor;

  mutable Mutex m_session_exit_mutex;

  enum class Close_reason {
    k_none,
    k_net_error,
    k_error,
    k_reject,
    k_normal,
    k_server_shutdown,
    k_kill,
    k_connect_timeout,
    k_write_timeout,
    k_read_timeout
  };

  std::atomic<Close_reason> m_close_reason{Close_reason::k_none};

  char *m_msg_buffer{nullptr};
  size_t m_msg_buffer_size{0};
  bool m_supports_expired_passwords{false};
  bool m_is_interactive{false};
  bool m_is_compression_encoder_injected{false};

  uint32_t m_read_timeout{0};
  uint32_t m_write_timeout{0};

  ngs::Compression_algorithm m_cached_compression_algorithm{
      ngs::Compression_algorithm::k_none};
  int64_t m_cached_max_msg{-1};
  bool m_cached_combine_msg{false};
  int32_t m_cached_compression_level{3};

  int32_t get_adjusted_compression_level(
      const ngs::Compression_algorithm algo,
      const Optional_value<int64_t> &level) const;

  Capabilities_configurator *capabilities_configurator();
  void get_capabilities(
      const Mysqlx::Connection::CapabilitiesGet &msg) override;
  void set_capabilities(
      const Mysqlx::Connection::CapabilitiesSet &msg) override;

  void remove_client_from_server();

  std::string resolve_hostname() const;
  virtual void on_network_error(const int error);
  void on_read_timeout();

  iface::Protocol_monitor &get_protocol_monitor();

 private:
  ngs::Protocol_encoder_compression *get_protocol_compression_or_install_it();

  Client(const Client &) = delete;
  Client &operator=(const Client &) = delete;

  void get_last_error(int *out_error_code, std::string *out_message);
  void set_close_reason_if_non_fatal(const Close_reason reason);
  void update_counters();
  void queue_up_disconnection_notice(const ngs::Error_code &error);
  void queue_up_disconnection_notice_if_necessary();

  void on_client_addr();
  void on_accept();
  bool create_session();
  bool is_localhost(const char *hostname) const;
};

}  // namespace xpl

#endif  // PLUGIN_X_SRC_CLIENT_H_

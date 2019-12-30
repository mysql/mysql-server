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

#ifndef PLUGIN_X_NGS_INCLUDE_NGS_CLIENT_H_
#define PLUGIN_X_NGS_INCLUDE_NGS_CLIENT_H_

#include <atomic>
#include <string>

#include "my_inttypes.h"
#include "plugin/x/ngs/include/ngs/interface/client_interface.h"
#include "plugin/x/ngs/include/ngs/interface/protocol_encoder_interface.h"
#include "plugin/x/ngs/include/ngs/interface/vio_interface.h"
#include "plugin/x/ngs/include/ngs/memory.h"
#include "plugin/x/ngs/include/ngs/protocol/message.h"
#include "plugin/x/ngs/include/ngs/protocol/page_pool.h"
#include "plugin/x/ngs/include/ngs/protocol_decoder.h"
#include "plugin/x/src/capabilities/configurator.h"
#include "plugin/x/src/global_timeouts.h"
#include "plugin/x/src/helper/chrono.h"
#include "plugin/x/src/helper/multithread/mutex.h"
#include "plugin/x/src/xpl_system_variables.h"

#ifndef WIN32
#include <netinet/in.h>
#endif

namespace ngs {
class Server_interface;

class Client : public Client_interface {
 public:
  Client(std::shared_ptr<Vio_interface> connection, Server_interface &server,
         Client_id client_id, Protocol_monitor_interface *pmon,
         const Global_timeouts &timeouts);
  ~Client() override;

  xpl::Mutex &get_session_exit_mutex() override { return m_session_exit_mutex; }
  Session_interface *session() override { return m_session.get(); }
  std::shared_ptr<Session_interface> session_smart_ptr() const override {
    return m_session;
  }

 public:  // impl ngs::Client_interface
  void run(const bool skip_resolve_name) override;

  void activate_tls() override;

  void reset_accept_time() override;

  void on_auth_timeout() override;
  void on_server_shutdown() override;

  Server_interface &server() const override { return m_server; }
  Protocol_encoder_interface &protocol() const override { return *m_encoder; }
  Vio_interface &connection() override { return *m_connection; }

  void on_session_auth_success(Session_interface &s) override;
  void on_session_close(Session_interface &s) override;
  void on_session_reset(Session_interface &s) override;

  void disconnect_and_trigger_close() override;
  bool is_handler_thd(const THD *) const override { return false; }

  const char *client_address() const override { return m_client_addr.c_str(); }
  const char *client_hostname() const override { return m_client_host.c_str(); }
  const char *client_hostname_or_address() const override;
  const char *client_id() const override { return m_id; }
  Client_id client_id_num() const override { return m_client_id; }
  int client_port() const override { return m_client_port; }

  State get_state() const override { return m_state.load(); }
  xpl::chrono::Time_point get_accept_time() const override;

  void set_supports_expired_passwords(bool flag) {
    m_supports_expired_passwords = flag;
  }

  bool is_interactive() const override { return m_is_interactive; }

  bool supports_expired_passwords() const override {
    return m_supports_expired_passwords;
  }

  void set_wait_timeout(const uint32_t) override;
  void set_read_timeout(const uint32_t) override;
  void set_write_timeout(const uint32_t) override;

  bool handle_session_connect_attr_set(ngs::Message_request &command);
  void handle_message(Message_request *message) override;

 private:
  class Message_dispatcher
      : public Message_decoder::Message_dispatcher_interface {
   public:
    explicit Message_dispatcher(Client_interface *client) : m_client(client) {}

    virtual void handle(Message_request *message) {
      m_client->handle_message(message);
    }

   private:
    Client_interface *m_client;
  };

 protected:
  char m_id[2 + sizeof(Client_id) * 2 + 1];  // 64bits in hex, plus 0x plus \0
  Client_id m_client_id;
  Server_interface &m_server;

  std::shared_ptr<Vio_interface> m_connection;
  std::shared_ptr<Protocol_config> m_config;
  // TODO(lkotula): benchmark m_memory_block_pool as global in Xpl (shouldn't be
  // in review)
  ngs::Memory_block_pool m_memory_block_pool{{10, k_minimum_page_size}};
  Message_dispatcher m_dispatcher;
  Protocol_decoder m_decoder;

  xpl::chrono::Time_point m_accept_time;

  Memory_instrumented<Protocol_encoder_interface>::Unique_ptr m_encoder;
  std::string m_client_addr;
  std::string m_client_host;
  uint16 m_client_port;
  std::atomic<State> m_state;
  std::atomic<bool> m_removed;

  std::shared_ptr<Session_interface> m_session;

  Protocol_monitor_interface *m_protocol_monitor;

  mutable xpl::Mutex m_session_exit_mutex;

  enum class Close_reason {
    k_none,
    k_net_error,
    k_error,
    k_reject,
    k_normal,
    k_connect_timeout,
    k_write_timeout,
    k_read_timeout
  };

  Close_reason m_close_reason{Close_reason::k_none};

  char *m_msg_buffer;
  size_t m_msg_buffer_size;
  bool m_supports_expired_passwords;
  bool m_is_interactive = false;
  bool m_is_compression_encoder_injected = false;

  uint32_t m_read_timeout = Global_timeouts::Default::k_read_timeout;
  uint32_t m_write_timeout = Global_timeouts::Default::k_write_timeout;

  Error_code read_one_message_and_dispatch();

  virtual xpl::Capabilities_configurator *capabilities_configurator();
  void get_capabilities(
      const Mysqlx::Connection::CapabilitiesGet &msg) override;
  void set_capabilities(
      const Mysqlx::Connection::CapabilitiesSet &msg) override;

  void remove_client_from_server();

  virtual std::string resolve_hostname() = 0;
  virtual void on_network_error(const int error);
  void on_read_timeout();

  Protocol_monitor_interface &get_protocol_monitor();

  void set_encoder(Protocol_encoder_interface *enc);

 private:
  Client(const Client &) = delete;
  Client &operator=(const Client &) = delete;

  xpl::iface::Waiting_for_io *get_idle_processing();
  void get_last_error(int *out_error_code, std::string *out_message);
  void set_close_reason_if_non_fatal(const Close_reason reason);
  void update_counters();

  void on_client_addr(const bool skip_resolve_name);
  void on_accept();
  bool create_session();
};

}  // namespace ngs

#endif  // PLUGIN_X_NGS_INCLUDE_NGS_CLIENT_H_

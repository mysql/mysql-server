/*
 * Copyright (c) 2015, 2017, Oracle and/or its affiliates. All rights reserved.
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

#ifndef _NGS_CLIENT_H_
#define _NGS_CLIENT_H_

#include "my_inttypes.h"
#include "ngs/capabilities/configurator.h"
#include "ngs/interface/client_interface.h"
#include "ngs/memory.h"
#include "ngs/protocol/message.h"
#include "ngs/protocol_decoder.h"
#include "ngs/protocol_encoder.h"
#include "ngs_common/atomic.h"
#include "ngs_common/chrono.h"
#include "ngs_common/connection_vio.h"

#ifndef WIN32
#include <netinet/in.h>
#endif


namespace ngs
{
  class Server_interface;

  class Client : public Client_interface
  {
  public:
    Client(Connection_ptr connection,
           Server_interface &server,
           Client_id client_id,
           Protocol_monitor_interface &pmon);
    virtual ~Client();

    Mutex &get_session_exit_mutex() override { return m_session_exit_mutex; }
    ngs::shared_ptr<Session_interface> session() override { return m_session; }

  public: // impl ngs::Client_interface
    void run(const bool skip_resolve_name) override;

    void activate_tls() override;

    void reset_accept_time() override;

    void on_auth_timeout() override;
    void on_server_shutdown() override;

    Server_interface &server() const override { return m_server; }
    Connection_vio  &connection() override { return *m_connection; };

    void on_session_auth_success(Session_interface &s) override;
    void on_session_close(Session_interface &s) override;
    void on_session_reset(Session_interface &s) override;

    void disconnect_and_trigger_close() override;

    const char *client_address() const override { return m_client_addr.c_str(); }
    const char *client_hostname() const override { return m_client_host.c_str(); }
    const char *client_id() const override { return m_id; }
    Client_id client_id_num() const override { return m_client_id; }
    int client_port() const override { return m_client_port; }

    Client_state  get_state() const override { return m_state.load(); };
    chrono::time_point get_accept_time() const override;

    void set_supports_expired_passwords(bool flag)
    {
      m_supports_expired_passwords = flag;
    }

    bool supports_expired_passwords() const override
    {
      return m_supports_expired_passwords;
    }

  protected:
    char m_id[2+sizeof(Client_id)*2+1]; // 64bits in hex, plus 0x plus \0
    Client_id m_client_id;
    Server_interface &m_server;
    Connection_ptr m_connection;

    Message_decoder m_decoder;

    ngs::chrono::time_point m_accept_time;

    ngs::Memory_instrumented<Protocol_encoder>::Unique_ptr m_encoder;
    std::string m_client_addr;
    std::string m_client_host;
    uint16      m_client_port;
    ngs::atomic<Client_state> m_state;
    ngs::atomic<bool> m_removed;

    ngs::shared_ptr<Session_interface> m_session;

    Protocol_monitor_interface &m_protocol_monitor;

    Mutex m_session_exit_mutex;

    enum {
      Not_closing,
      Close_net_error,
      Close_error,
      Close_reject,
      Close_normal,
      Close_connect_timeout
    } m_close_reason;

    char* m_msg_buffer;
    size_t m_msg_buffer_size;
    bool m_supports_expired_passwords;

    Request *read_one_message(Error_code &ret_error);

    virtual ngs::Capabilities_configurator *capabilities_configurator();
    void get_capabilities(const Mysqlx::Connection::CapabilitiesGet &msg);
    void set_capabilities(const Mysqlx::Connection::CapabilitiesSet &msg);

    void remove_client_from_server();

    void handle_message(Request &message);
    virtual std::string resolve_hostname() = 0;
    virtual void on_network_error(int error);

    Protocol_monitor_interface &get_protocol_monitor();

  private:
    Client(const Client &) = delete;
    Client &operator=(const Client &) = delete;

    void get_last_error(int &error_code, std::string &message);
    void shutdown_connection();

    void on_client_addr(const bool skip_resolve_name);
    void on_accept();
    void on_kill(Session_interface &session);
  };

} // namespace ngs

#endif

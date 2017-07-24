/*
 * Copyright (c) 2015, 2017 Oracle and/or its affiliates. All rights reserved.
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

#include "ngs/protocol_encoder.h"
#include "ngs/protocol_decoder.h"
#include "ngs/memory.h"
#include "ngs/protocol/message.h"
#include "ngs/interface/client_interface.h"
#include "ngs/capabilities/configurator.h"

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

    Mutex &get_session_exit_mutex() { return m_session_exit_mutex; }
    ngs::shared_ptr<Session_interface> session() { return m_session; }

  public: // impl ngs::Client_interface
    virtual void run(const bool skip_resolve_name);

    virtual void activate_tls();

    virtual void reset_accept_time();

    virtual void on_auth_timeout();
    virtual void on_server_shutdown();

    virtual Server_interface &server() const { return m_server; }
    virtual Connection_vio  &connection() { return *m_connection; };

    virtual void on_session_auth_success(Session_interface &s);
    virtual void on_session_close(Session_interface &s);
    virtual void on_session_reset(Session_interface &s);

    virtual void disconnect_and_trigger_close();

    virtual const char *client_address() const { return m_client_addr.c_str(); }
    virtual const char *client_hostname() const { return m_client_host.c_str(); }
    virtual const char *client_id() const { return m_id; }
    virtual Client_id client_id_num() const { return m_client_id; }
    virtual int       client_port() const { return m_client_port; }

    virtual Client_state  get_state() const { return m_state.load(); };
    virtual chrono::time_point get_accept_time() const;

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
    Client(const Client &);
    Client &operator=(const Client &);

    void get_last_error(int &error_code, std::string &message);
    void shutdown_connection();

    void on_client_addr(const bool skip_resolve_name);
    void on_accept();
    void on_kill(Session_interface &session);
  };

} // namespace ngs

#endif

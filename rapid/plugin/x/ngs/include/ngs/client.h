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

#ifndef _NGS_CLIENT_H_
#define _NGS_CLIENT_H_

#include "ngs/ngs_types.h"
#include "ngs/protocol_encoder.h"
#include "ngs/protocol_decoder.h"
#include "ngs/memory.h"
#include "ngs/protocol/message.h"
#include "ngs/client_session.h"
#include "ngs/capabilities/configurator.h"

#include <boost/enable_shared_from_this.hpp>
#include <boost/atomic.hpp>

#include "ngs_common/connection_vio.h"

#ifndef WIN32
#include <netinet/in.h>
#endif


namespace ngs
{
  class IServer;

  class Client : public Session::Delegate,
                 public boost::enable_shared_from_this<Client>, private boost::noncopyable
  {
  public:
    typedef uint64_t Client_id;

    enum Client_state
    {
      Client_invalid,
      Client_accepted,
      Client_accepted_with_session,
      Client_authenticating_first,
      Client_running,
      Client_closing,
      Client_closed
    };

  public:
    Client(Connection_ptr connection, IServer *server, Client_id client_id, IProtocol_monitor &pmon);

    virtual ~Client();

    virtual IServer *server() const { return m_server; }
    virtual Connection_vio  &connection() { return *m_connection; };

    virtual void activate_tls();
    virtual void post_activate_tls() = 0;

    void run(bool skip_resolve_name, sockaddr_in client_addr);

    virtual void on_auth_timeout();
    virtual void on_server_shutdown();

    virtual const char *client_address() const { return m_client_addr.c_str(); }
    virtual const std::string &client_hostname() const { return m_client_host; }
    virtual const char *client_id() const { return m_id; }
    Client_id client_id_num() const { return m_client_id; }
    int client_port() const { return m_client_port; }

    virtual ptime         get_accept_time() const;
    virtual void          reset_accept_time(const Client_state new_state = Client_accepted);
    virtual Client_state  get_state() const { return m_state.load(); };

    boost::shared_ptr<Session> session() { return m_session; }

    IProtocol_monitor &get_protocol_monitor()
    {
      return m_protocol_monitor;
    }

  public:
    virtual void on_session_auth_success(Session *s);
    virtual void on_session_close(Session *s);
    virtual void on_session_reset(Session *s);

    virtual void on_network_error(int error);

    static boost::atomics::atomic<int32_t> num_of_instances;

    Request_unique_ptr read_one_message(Error_code &ret_error);
    void disconnect_and_trigger_close();
    void shutdown_connection();

    Mutex &get_session_exit_mutex() { return m_session_exit_mutex; }

  protected:
    char m_id[2+sizeof(Client_id)*2+1]; // 64bits in hex, plus 0x plus \0
    Client_id m_client_id;
    IServer *m_server;
    Connection_ptr m_connection;

    Message_decoder m_decoder;

    boost::posix_time::ptime m_accept_time;

    Protocol_encoder *m_encoder;
    std::string m_client_addr;
    std::string m_client_host;
    int m_client_port;
    boost::atomics::atomic<Client_state> m_state;
    boost::atomics::atomic<bool> m_removed;

    boost::shared_ptr<Session> m_session;

    IProtocol_monitor &m_protocol_monitor;

    Mutex m_session_exit_mutex;

    enum {
      Not_closing,
      Close_net_error,
      Close_error,
      Close_reject,
      Close_normal,
      Close_connect_timeout
    } m_close_reason;

    virtual ngs::Capabilities_configurator *capabilities_configurator();

    void get_capabilities(const Mysqlx::Connection::CapabilitiesGet &msg);
    void set_capabilities(const Mysqlx::Connection::CapabilitiesSet &msg);

    void remove_client_from_server();

    void handle_message(Request &message);
    virtual bool is_localhost(const char *) = 0;

  private:
    void on_accept(const bool skip_resolve_name, const sockaddr_in *client_addr);
    void on_kill(Session &session);
  };

} // namespace ngs

#endif

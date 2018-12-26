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

#ifndef _XPL_CLIENT_H_
#define _XPL_CLIENT_H_


#include "ngs/client.h"
#include "ngs/protocol_monitor.h"

struct st_mysql_show_var;

namespace xpl
{
  class Session;

  class Client;

  class Protocol_monitor : public ngs::Protocol_monitor_interface
  {
  public:
    Protocol_monitor() : m_client(0) {}
    void init(Client *client);

    virtual void on_notice_warning_send();
    virtual void on_notice_other_send();
    virtual void on_error_send();
    virtual void on_fatal_error_send();
    virtual void on_init_error_send();
    virtual void on_row_send();
    virtual void on_send(long bytes_transferred);
    virtual void on_receive(long bytes_transferred);
    virtual void on_error_unknown_msg_type();

  private:
    Client *m_client;
  };

  class Client : public ngs::Client
  {
  public:
    Client(ngs::Connection_ptr connection, ngs::Server_interface &server, Client_id client_id, Protocol_monitor *pmon);
    virtual ~Client();

  public: // impl ngs::Client_interface
    virtual void on_session_close(ngs::Session_interface &s);
    virtual void on_session_reset(ngs::Session_interface &s);

    virtual void on_server_shutdown();
    virtual void on_auth_timeout();

  public: // impl ngs::Client
    virtual void on_network_error(int error);
    virtual std::string resolve_hostname();
    virtual ngs::Capabilities_configurator *capabilities_configurator();

  public:
    void set_supports_expired_passwords(bool flag);
    bool supports_expired_passwords();
    bool is_handler_thd(THD *thd);

    void get_status_ssl_cipher_list(st_mysql_show_var *var);

    void kill();
    ngs::shared_ptr<xpl::Session> get_session();

  private:
    bool is_localhost(const char *hostname);

    bool m_supports_expired_passwords;
    Protocol_monitor *m_protocol_monitor;
  };

  typedef ngs::shared_ptr<Client> Client_ptr;

} // namespace xpl


#endif // _XPL_CLIENT_H_

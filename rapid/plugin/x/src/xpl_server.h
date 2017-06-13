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

#ifndef _XPL_SERVER_H_
#define _XPL_SERVER_H_

#include <string>
#include <vector>

#include "ngs/server.h"
#include "ngs/memory.h"
#include "ngs/scheduler.h"
#include "ngs_common/connection_vio.h"
#include "ngs_common/atomic.h"
#include "xpl_session.h"
#include "mysql_show_variable_wrapper.h"
#include "xpl_global_status_variables.h"

#include <mysql/plugin.h>
#include "xpl_client.h"


namespace xpl
{

class Session;
class Sql_data_context;
class Server;
struct Ssl_config;

typedef ngs::shared_ptr<Server> Server_ptr;

class Server : public ngs::Server_delegate
{
public:
  Server(ngs::shared_ptr<ngs::Server_acceptors> acceptors,
         ngs::shared_ptr<ngs::Scheduler_dynamic> wscheduler,
         ngs::shared_ptr<ngs::Protocol_config> config);

  static int main(MYSQL_PLUGIN p);
  static int exit(MYSQL_PLUGIN p);

  template <void (Client::*method)(st_mysql_show_var *)>
  static void session_status_variable(THD *thd, st_mysql_show_var *var, char *buff);

  template <typename ReturnType, ReturnType (ngs::IOptions_session::*method)()>
  static void session_status_variable(THD *thd, st_mysql_show_var *var, char *buff);

  template <typename ReturnType, ReturnType (Server::*method)()>
  static void global_status_variable_server_with_return(THD *thd, st_mysql_show_var *var, char *buff);

  template <void (Server::*method)(st_mysql_show_var *)>
  static void global_status_variable(THD *thd, st_mysql_show_var *var, char *buff);

  template <typename ReturnType, xpl::Global_status_variables::Variable xpl::Global_status_variables::*variable>
  static void global_status_variable_server(THD *thd, st_mysql_show_var *var, char *buff);

  template <typename ReturnType, xpl::Common_status_variables::Variable xpl::Common_status_variables::*variable>
  static void common_status_variable(THD *thd, st_mysql_show_var *var, char *buff);

  template <typename ReturnType, ReturnType (ngs::IOptions_context::*method)()>
  static void global_status_variable(THD *thd, st_mysql_show_var *var, char *buff);

  ngs::Server &server() { return m_server; }

  ngs::Error_code kill_client(uint64_t client_id, Session &requester);

  std::string get_socket_file();
  std::string get_tcp_bind_address();
  std::string get_tcp_port();

  typedef ngs::Locked_container<Server, ngs::RWLock_readlock, ngs::RWLock> Server_with_lock;
  typedef ngs::Memory_instrumented<Server_with_lock>::Unique_ptr Server_ref;

  static Server_ref get_instance()
  {
    //TODO: ngs::Locked_container add container that supports shared_ptrs
    return instance ? Server_ref(ngs::allocate_object<Server_with_lock>(ngs::ref(*instance), ngs::ref(instance_rwl))) : Server_ref();
  }

private:
  static Client_ptr      get_client_by_thd(Server_ref &server, THD *thd);
  static void            verify_mysqlx_user_grants(Sql_data_context &context);

  bool on_net_startup();

  void net_thread();

  void start_verify_server_state_timer();
  bool on_verify_server_state();

  void plugin_system_variables_changed();

  virtual ngs::shared_ptr<ngs::Client_interface>  create_client(ngs::Connection_ptr connection);
  virtual ngs::shared_ptr<ngs::Session_interface> create_session(ngs::Client_interface &client,
                                                                   ngs::Protocol_encoder &proto,
                                                                   ngs::Session_interface::Session_id session_id);

  virtual bool will_accept_client(const ngs::Client_interface &client);
  virtual void did_accept_client(const ngs::Client_interface &client);
  virtual void did_reject_client(ngs::Server_delegate::Reject_reason reason);

  virtual void on_client_closed(const ngs::Client_interface &client);
  virtual bool is_terminating() const;

  static Server*      instance;
  static ngs::RWLock  instance_rwl;
  static MYSQL_PLUGIN plugin_ref;

  ngs::Client_interface::Client_id        m_client_id;
  ngs::atomic<int>                        m_num_of_connections;
  ngs::shared_ptr<ngs::Protocol_config>   m_config;
  ngs::shared_ptr<ngs::Server_acceptors>  m_acceptors;
  ngs::shared_ptr<ngs::Scheduler_dynamic> m_wscheduler;
  ngs::shared_ptr<ngs::Scheduler_dynamic> m_nscheduler;
  ngs::Mutex  m_accepting_mutex;
  ngs::Server m_server;

  static bool exiting;
  static bool is_exiting();
};


template <void (Client::*method)(st_mysql_show_var *)>
void Server::session_status_variable(THD *thd, st_mysql_show_var *var, char *buff)
{
  var->type= SHOW_UNDEF;
  var->value= buff;

  Server_ref server(get_instance());
  if (server)
  {
    ngs::unique_ptr<Mutex_lock> lock(new Mutex_lock((*server)->server().get_client_exit_mutex()));
    Client_ptr client = get_client_by_thd(server, thd);

    if (client)
      ((*client).*method)(var);
  }
}


template <typename ReturnType, ReturnType (ngs::IOptions_session::*method)()>
void Server::session_status_variable(THD *thd, st_mysql_show_var *var, char *buff)
{
  var->type= SHOW_UNDEF;
  var->value= buff;

  Server_ref server(get_instance());
  if (server)
  {
    ngs::unique_ptr<Mutex_lock> lock(new Mutex_lock((*server)->server().get_client_exit_mutex()));
    Client_ptr client = get_client_by_thd(server, thd);

    if (client)
    {
      ReturnType result = ((*client->connection().options()).*method)();
      mysqld::xpl_show_var(var).assign(result);
    }
  }
}


template <void (Server::*method)(st_mysql_show_var *)>
void Server::global_status_variable(THD *thd, st_mysql_show_var *var, char *buff)
{
  var->type= SHOW_UNDEF;
  var->value= buff;

  Server_ref server = get_instance();
  if (server)
  {
    Server* server_ptr = server->container();
    (server_ptr->*method)(var);
  }
}

template <typename ReturnType, ReturnType (Server::*method)()>
void Server::global_status_variable_server_with_return(THD *thd, st_mysql_show_var *var, char *buff)
{
  var->type= SHOW_UNDEF;
  var->value= buff;

  Server_ref server = get_instance();
  if (server)
  {
    Server* server_ptr = server->container();
    ReturnType result = (server_ptr->*method)();

    mysqld::xpl_show_var(var).assign(result);
  }
}


template <typename ReturnType, xpl::Global_status_variables::Variable xpl::Global_status_variables::*variable>
void Server::global_status_variable_server(THD *thd, st_mysql_show_var *var, char *buff)
{
  var->type= SHOW_UNDEF;
  var->value= buff;

  ReturnType result = (Global_status_variables::instance().*variable).load();
  mysqld::xpl_show_var(var).assign(result);
}


template <typename ReturnType, xpl::Common_status_variables::Variable xpl::Common_status_variables::*variable>
void Server::common_status_variable(THD *thd, st_mysql_show_var *var, char *buff)
{
  var->type = SHOW_UNDEF;
  var->value = buff;

  Server_ref server(get_instance());
  if (server)
  {
    ngs::unique_ptr<Mutex_lock> lock(new Mutex_lock((*server)->server().get_client_exit_mutex()));
    Client_ptr client = get_client_by_thd(server, thd);

    if (client)
    {
      ngs::shared_ptr<xpl::Session> client_session(client->get_session());
      if (client_session)
      {
        Common_status_variables &common_status = client_session->get_status_variables();
        ReturnType result = (common_status.*variable).load();
        mysqld::xpl_show_var(var).assign(result);
      }
      return;
    }
  }

  Common_status_variables &common_status = Global_status_variables::instance();
  ReturnType result = (common_status.*variable).load();
  mysqld::xpl_show_var(var).assign(result);
}


template <typename ReturnType, ReturnType (ngs::IOptions_context::*method)()>
void Server::global_status_variable(THD *thd, st_mysql_show_var *var, char *buff)
{
  var->type= SHOW_UNDEF;
  var->value= buff;

  Server_ref server = get_instance();
  if (!server || !(*server)->server().ssl_context())
     return;
  ngs::IOptions_context_ptr context = (*server)->server().ssl_context()->options();
  if (!context)
    return;

  ReturnType result = ((*context).*method)();

  mysqld::xpl_show_var(var).assign(result);
}

} // namespace xpl

#ifdef HAVE_YASSL
#define IS_YASSL_OR_OPENSSL(Y, O) Y
#else // HAVE_YASSL
#define IS_YASSL_OR_OPENSSL(Y, O) O
#endif // HAVE_YASSL

#endif  // _XPL_SERVER_H_

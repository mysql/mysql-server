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

#ifndef _NGS_SERVER_H_
#define _NGS_SERVER_H_

#include <stdint.h>

#include <list>
#include <vector>
#include <memory>

#include "ngs_common/chrono.h"
#include "my_global.h"

#include "ngs_common/connection_vio.h"
#include "ngs/interface/server_interface.h"
#include "ngs/interface/server_delegate.h"
#include "ngs/protocol/protocol_config.h"
#include "ngs/protocol_encoder.h"
#include "ngs/protocol_authentication.h"
#include "ngs/client_list.h"
#include "ngs/thread.h"
#include "ngs_common/bind.h"
#include "socket_events.h"


namespace ngs
{

class Server;
class Session_interface;
class Client_interface;
class Server_task_interface;
class Connection_acceptor_interface;
class Incoming_queue;
class Scheduler_dynamic;
class Server_acceptors;

class Server: public Server_interface
{
public:
  enum State {State_initializing, State_running, State_failure, State_terminating};

  Server(ngs::shared_ptr<Server_acceptors> acceptors,
         ngs::shared_ptr<Scheduler_dynamic> accept_scheduler,
         ngs::shared_ptr<Scheduler_dynamic> work_scheduler,
         Server_delegate *delegate,
         ngs::shared_ptr<Protocol_config> config);

  virtual Ssl_context *ssl_context() const { return m_ssl_context.get(); }

  bool prepare(Ssl_context_unique_ptr ssl_context, const bool skip_networking, const bool skip_name_resolve, const bool use_unix_sockets);

  void start();
  void start_failed();
  void stop(const bool is_called_from_timeout_handler = false);

  void close_all_clients();

  bool is_terminating();
  bool is_running();

  virtual ngs::shared_ptr<Protocol_config> get_config() const { return m_config; }
  ngs::shared_ptr<Scheduler_dynamic> get_worker_scheduler() const { return m_worker_scheduler; }
  Client_list &get_client_list() { return m_client_list; }
  Mutex &get_client_exit_mutex() { return m_client_exit_mutex; }

  virtual ngs::shared_ptr<Session_interface> create_session(Client_interface &client,
                                                              Protocol_encoder &proto,
                                                              int session_id);

  void on_client_closed(const Client_interface &client);

  Authentication_handler_ptr get_auth_handler(const std::string &name, Session_interface *session);
  void get_authentication_mechanisms(std::vector<std::string> &auth_mech, Client_interface &client);
  void add_authentication_mechanism(const std::string &name,
                                    Authentication_handler::create initiator,
                                    const bool allowed_only_with_secure_connection);

  void add_timer(const std::size_t delay_ms, ngs::function<bool ()> callback);

private:
  Server(const Server&);
  Server& operator=(const Server&);

  void run_task(ngs::shared_ptr<Server_task_interface> handler);
  void wait_for_clients_closure();
  void go_through_all_clients(ngs::function<void (Client_ptr)> callback);
  bool timeout_for_clients_validation();
  void wait_for_next_client();

  // accept one connection, create a connection object for the client and tell it to
  // start reading input
  void on_accept(Connection_acceptor_interface &acceptor);
  void start_client_supervision_timer(const chrono::duration &oldest_object_time_ms);
  void restart_client_supervision_timer();

  bool on_check_terminated_workers();

private:
  class Authentication_key
  {
  public:
    Authentication_key(const std::string &key_name, const bool key_should_be_tls_active)
    : name(key_name), must_be_secure_connection(key_should_be_tls_active)
    {
    }

    bool operator< (const Authentication_key &key) const
    {
      int result = name.compare(key.name);

      if (0 != result)
      {
        return result < 0;
      }

      return must_be_secure_connection < key.must_be_secure_connection;
    }

    const std::string name;
    const bool must_be_secure_connection;
  };

  typedef std::map<Authentication_key, Authentication_handler::create> Auth_handler_map;

  bool m_timer_running;
  bool m_skip_name_resolve;
  uint32 m_errors_while_accepting;

  ngs::shared_ptr<Server_acceptors> m_acceptors;
  ngs::shared_ptr<Scheduler_dynamic> m_accept_scheduler;
  ngs::shared_ptr<Scheduler_dynamic> m_worker_scheduler;
  ngs::shared_ptr<Protocol_config> m_config;

  Ssl_context_unique_ptr m_ssl_context;
  Sync_variable<State> m_state;
  Auth_handler_map m_auth_handlers;
  Client_list m_client_list;
  Server_delegate *m_delegate;
  Mutex m_client_exit_mutex;
};

} // namespace ngs

#endif // _NGS_SERVER_H_

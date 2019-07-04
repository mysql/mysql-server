/*
 * Copyright (c) 2015, 2018, Oracle and/or its affiliates. All rights reserved.
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

#ifndef PLUGIN_X_NGS_INCLUDE_NGS_SERVER_H_
#define PLUGIN_X_NGS_INCLUDE_NGS_SERVER_H_

#include <stdint.h>
#include <functional>
#include <list>
#include <memory>
#include <vector>

#include "my_inttypes.h"
#include "plugin/x/ngs/include/ngs/client_list.h"
#include "plugin/x/ngs/include/ngs/interface/authentication_interface.h"
#include "plugin/x/ngs/include/ngs/interface/document_id_generator_interface.h"
#include "plugin/x/ngs/include/ngs/interface/protocol_encoder_interface.h"
#include "plugin/x/ngs/include/ngs/interface/server_delegate.h"
#include "plugin/x/ngs/include/ngs/interface/server_interface.h"
#include "plugin/x/ngs/include/ngs/interface/server_task_interface.h"
#include "plugin/x/ngs/include/ngs/interface/sha256_password_cache_interface.h"
#include "plugin/x/ngs/include/ngs/interface/timeout_callback_interface.h"
#include "plugin/x/ngs/include/ngs/protocol/protocol_config.h"
#include "plugin/x/ngs/include/ngs/protocol_encoder.h"
#include "plugin/x/ngs/include/ngs/server_properties.h"
#include "plugin/x/ngs/include/ngs/socket_events.h"
#include "plugin/x/ngs/include/ngs/thread.h"
#include "plugin/x/src/helper/chrono.h"
#include "plugin/x/src/helper/multithread/mutex.h"
#include "plugin/x/src/helper/multithread/sync_variable.h"

namespace ngs {

class Server;
class Session_interface;
class Client_interface;
class Server_task_interface;
class Connection_acceptor_interface;
class Incoming_queue;
class Scheduler_dynamic;
class Socket_acceptors_task;

class Server : public Server_interface {
 public:
  using Task_context = Server_task_interface::Task_context;
  using Stop_cause = Server_task_interface::Stop_cause;
  using Server_task_vector = std::vector<Server_tasks_interface_ptr>;

  enum State {
    State_initializing,
    State_running,
    State_failure,
    State_terminating
  };

 public:
  Server(std::shared_ptr<Scheduler_dynamic> accept_scheduler,
         std::shared_ptr<Scheduler_dynamic> work_scheduler,
         Server_delegate *delegate, std::shared_ptr<Protocol_config> config,
         Server_properties *properties, const Server_task_vector &tasks,
         std::shared_ptr<Timeout_callback_interface> timeout_callback);

  virtual Ssl_context_interface *ssl_context() const override {
    return m_ssl_context.get();
  }

  bool prepare(std::unique_ptr<Ssl_context_interface> ssl_context,
               const bool skip_networking, const bool skip_name_resolve);

  void start();
  void start_failed();
  void stop(const bool is_called_from_timeout_handler = false);

  void close_all_clients();

  bool is_terminating();
  bool is_running() override;

  virtual std::shared_ptr<Protocol_config> get_config() const override {
    return m_config;
  }
  std::shared_ptr<Scheduler_dynamic> get_worker_scheduler() const override {
    return m_worker_scheduler;
  }
  Client_list &get_client_list() { return m_client_list; }
  xpl::Mutex &get_client_exit_mutex() override { return m_client_exit_mutex; }
  Client_ptr get_client(const THD *thd);

  virtual std::shared_ptr<Session_interface> create_session(
      Client_interface &client, Protocol_encoder_interface &proto,
      const int session_id) override;

  void on_client_closed(const Client_interface &client) override;

  Authentication_interface_ptr get_auth_handler(
      const std::string &name, Session_interface *session) override;
  void get_authentication_mechanisms(std::vector<std::string> &auth_mech,
                                     Client_interface &client) override;
  void add_authentication_mechanism(
      const std::string &name, Authentication_interface::Create initiator,
      const bool allowed_only_with_secure_connection);
  void add_sha256_password_cache(SHA256_password_cache_interface *cache);

  void add_callback(const std::size_t delay_ms, std::function<bool()> callback);
  bool reset_globals();
  Document_id_generator_interface &get_document_id_generator() const override {
    return *m_id_generator;
  }

 private:
  Server(const Server &);
  Server &operator=(const Server &);

  void run_task(std::shared_ptr<Server_task_interface> handler);
  void wait_for_clients_closure();
  void go_through_all_clients(std::function<void(Client_ptr)> callback);
  bool timeout_for_clients_validation();
  void wait_for_next_client();

  // accept one connection, create a connection object for the client and tell
  // it to start reading input
  void on_accept(Connection_acceptor_interface &acceptor);
  void start_client_supervision_timer(
      const xpl::chrono::Duration &oldest_object_time_ms);
  void restart_client_supervision_timer() override;

  bool on_check_terminated_workers();

 private:
  class Authentication_key {
   public:
    Authentication_key(const std::string &key_name,
                       const bool key_should_be_tls_active)
        : name(key_name), must_be_secure_connection(key_should_be_tls_active) {}

    bool operator<(const Authentication_key &key) const {
      int result = name.compare(key.name);

      if (0 != result) {
        return result < 0;
      }

      return must_be_secure_connection < key.must_be_secure_connection;
    }

    const std::string name;
    const bool must_be_secure_connection;
  };

  typedef std::map<Authentication_key, Authentication_interface::Create>
      Auth_handler_map;

  bool m_timer_running;
  bool m_skip_name_resolve;
  uint32 m_errors_while_accepting;
  SHA256_password_cache_interface *m_sha256_password_cache;

  std::shared_ptr<Socket_acceptors_task> m_acceptors;
  std::shared_ptr<Scheduler_dynamic> m_accept_scheduler;
  std::shared_ptr<Scheduler_dynamic> m_worker_scheduler;
  std::shared_ptr<Protocol_config> m_config;
  std::unique_ptr<Document_id_generator_interface> m_id_generator;

  std::unique_ptr<Ssl_context_interface> m_ssl_context;
  xpl::Sync_variable<State> m_state;
  Auth_handler_map m_auth_handlers;
  Client_list m_client_list;
  Server_delegate *m_delegate;
  xpl::Mutex m_client_exit_mutex;
  Server_properties *m_properties;
  Server_task_vector m_tasks;
  std::shared_ptr<Timeout_callback_interface> m_timeout_callback;
};

}  // namespace ngs

#endif  // PLUGIN_X_NGS_INCLUDE_NGS_SERVER_H_

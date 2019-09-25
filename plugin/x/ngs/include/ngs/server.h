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

#ifndef PLUGIN_X_NGS_INCLUDE_NGS_SERVER_H_
#define PLUGIN_X_NGS_INCLUDE_NGS_SERVER_H_

#include <stdint.h>

#include <cstdint>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "plugin/x/ngs/include/ngs/client_list.h"
#include "plugin/x/ngs/include/ngs/protocol/protocol_config.h"
#include "plugin/x/ngs/include/ngs/server_properties.h"
#include "plugin/x/ngs/include/ngs/socket_events.h"
#include "plugin/x/ngs/include/ngs/thread.h"
#include "plugin/x/src/helper/chrono.h"
#include "plugin/x/src/helper/multithread/mutex.h"
#include "plugin/x/src/helper/multithread/sync_variable.h"
#include "plugin/x/src/interface/authentication.h"
#include "plugin/x/src/interface/client.h"
#include "plugin/x/src/interface/document_id_generator.h"
#include "plugin/x/src/interface/protocol_encoder.h"
#include "plugin/x/src/interface/server.h"
#include "plugin/x/src/interface/server_delegate.h"
#include "plugin/x/src/interface/server_task.h"
#include "plugin/x/src/interface/session.h"
#include "plugin/x/src/interface/sha256_password_cache.h"
#include "plugin/x/src/interface/ssl_context.h"
#include "plugin/x/src/interface/timeout_callback.h"

namespace ngs {

class Server;
class Incoming_queue;
class Scheduler_dynamic;
class Socket_acceptors_task;

class Server : public xpl::iface::Server {
 public:
  using Task_context = xpl::iface::Server_task::Task_context;
  using Stop_cause = xpl::iface::Server_task::Stop_cause;
  using Server_task_vector =
      std::vector<std::shared_ptr<xpl::iface::Server_task>>;

  enum State {
    State_initializing,
    State_running,
    State_failure,
    State_terminating
  };

 public:
  Server(std::shared_ptr<Scheduler_dynamic> accept_scheduler,
         std::shared_ptr<Scheduler_dynamic> work_scheduler,
         xpl::iface::Server_delegate *delegate,
         std::shared_ptr<Protocol_global_config> config,
         Server_properties *properties, const Server_task_vector &tasks,
         std::shared_ptr<xpl::iface::Timeout_callback> timeout_callback);

  xpl::iface::Ssl_context *ssl_context() const override {
    return m_ssl_context.get();
  }

  bool prepare(std::unique_ptr<xpl::iface::Ssl_context> ssl_context,
               const bool skip_networking, const bool skip_name_resolve);

  void start();
  void start_failed();
  void stop(const bool is_called_from_timeout_handler = false);

  void close_all_clients();

  bool is_terminating();
  bool is_running() override;

  std::shared_ptr<Protocol_global_config> get_config() const override {
    return m_config;
  }
  std::shared_ptr<Scheduler_dynamic> get_worker_scheduler() const override {
    return m_worker_scheduler;
  }
  Client_list &get_client_list() { return m_client_list; }
  xpl::Mutex &get_client_exit_mutex() override { return m_client_exit_mutex; }
  std::shared_ptr<xpl::iface::Client> get_client(const THD *thd);

  std::shared_ptr<xpl::iface::Session> create_session(
      xpl::iface::Client *client, xpl::iface::Protocol_encoder *proto,
      const int session_id) override;

  void on_client_closed(const xpl::iface::Client &client) override;

  std::unique_ptr<xpl::iface::Authentication> get_auth_handler(
      const std::string &name, xpl::iface::Session *session) override;
  void get_authentication_mechanisms(std::vector<std::string> *auth_mech,
                                     const xpl::iface::Client &client) override;
  void add_authentication_mechanism(
      const std::string &name, xpl::iface::Authentication::Create initiator,
      const bool allowed_only_with_secure_connection);
  void add_sha256_password_cache(xpl::iface::SHA256_password_cache *cache);

  void add_callback(const std::size_t delay_ms, std::function<bool()> callback);
  bool reset_globals();
  xpl::iface::Document_id_generator &get_document_id_generator()
      const override {
    return *m_id_generator;
  }

 private:
  Server(const Server &);
  Server &operator=(const Server &);

  void run_task(std::shared_ptr<xpl::iface::Server_task> handler);
  void wait_for_clients_closure();
  void go_through_all_clients(
      std::function<void(std::shared_ptr<xpl::iface::Client>)> callback);
  bool timeout_for_clients_validation();
  void wait_for_next_client();

  // accept one connection, create a connection object for the client and tell
  // it to start reading input
  void on_accept(xpl::iface::Connection_acceptor &acceptor);
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

  typedef std::map<Authentication_key, xpl::iface::Authentication::Create>
      Auth_handler_map;

  bool m_timer_running;
  bool m_skip_name_resolve;
  uint32_t m_errors_while_accepting;
  xpl::iface::SHA256_password_cache *m_sha256_password_cache;

  std::shared_ptr<Socket_acceptors_task> m_acceptors;
  std::shared_ptr<Scheduler_dynamic> m_accept_scheduler;
  std::shared_ptr<Scheduler_dynamic> m_worker_scheduler;
  std::shared_ptr<Protocol_global_config> m_config;
  std::unique_ptr<xpl::iface::Document_id_generator> m_id_generator;

  std::unique_ptr<xpl::iface::Ssl_context> m_ssl_context;
  xpl::Sync_variable<State> m_state;
  Auth_handler_map m_auth_handlers;
  Client_list m_client_list;
  xpl::iface::Server_delegate *m_delegate;
  xpl::Mutex m_client_exit_mutex;
  Server_properties *m_properties;
  Server_task_vector m_tasks;
  std::shared_ptr<xpl::iface::Timeout_callback> m_timeout_callback;
};

}  // namespace ngs

#endif  // PLUGIN_X_NGS_INCLUDE_NGS_SERVER_H_

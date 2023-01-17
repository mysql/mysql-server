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

#ifndef PLUGIN_X_SRC_SERVER_SERVER_H_
#define PLUGIN_X_SRC_SERVER_SERVER_H_

#include <stdint.h>

#include <cstdint>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <string>
#include <vector>

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
#include "plugin/x/src/ngs/protocol/protocol_config.h"
#include "plugin/x/src/ngs/socket_events.h"
#include "plugin/x/src/ngs/thread.h"
#include "plugin/x/src/server/authentication_container.h"
#include "plugin/x/src/server/server_factory.h"

namespace ngs {

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
         std::shared_ptr<Protocol_global_config> config,
         Server_properties *properties, const Server_task_vector &tasks,
         std::shared_ptr<xpl::iface::Timeout_callback> timeout_callback);

  xpl::iface::Ssl_context *ssl_context() const override {
    return m_ssl_context.get();
  }

  bool reset() override;
  bool prepare() override;
  void delayed_start_tasks() override;
  void start_tasks() override;
  void start_failed() override;
  void stop() override;
  void gracefull_shutdown() override;

  void graceful_close_all_clients();

  bool is_terminating();
  bool is_running() override;

  std::shared_ptr<Protocol_global_config> get_config() const override {
    return m_config;
  }
  xpl::Authentication_container &get_authentications() override {
    return m_auth_handlers;
  }
  Client_list &get_client_list() override { return m_client_list; }
  xpl::Mutex &get_client_exit_mutex() override { return m_client_exit_mutex; }
  std::shared_ptr<xpl::iface::Client> get_client(const THD *thd) override;
  Error_code kill_client(const uint64_t client_id,
                         xpl::iface::Session *requester) override;

  std::shared_ptr<xpl::iface::Session> create_session(
      xpl::iface::Client *client, xpl::iface::Protocol_encoder *proto,
      const int session_id) override;

  void on_client_closed(const xpl::iface::Client &client) override;

  xpl::iface::Document_id_generator &get_document_id_generator()
      const override {
    return *m_id_generator;
  }

 private:
  void run_task(std::shared_ptr<xpl::iface::Server_task> handler);
  void wait_for_clients_closure();
  void go_through_all_clients(
      std::function<void(std::shared_ptr<xpl::iface::Client>)> callback);
  bool timeout_for_clients_validation();
  void wait_for_next_client();

  // accept one connection, create a connection object for the client and tell
  // it to start reading input
  void on_accept(xpl::iface::Connection_acceptor *acceptor);
  std::shared_ptr<xpl::iface::Client> will_accept_client(::Vio *vio);
  void start_client_supervision_timer(
      const xpl::chrono::Duration &oldest_object_time_ms);
  void restart_client_supervision_timer() override;
  bool on_check_terminated_workers();

 private:
  bool m_timer_running;
  bool m_stop_called;
  uint32_t m_errors_while_accepting;

  std::shared_ptr<Socket_acceptors_task> m_acceptors;
  std::shared_ptr<Scheduler_dynamic> m_accept_scheduler;
  std::shared_ptr<Scheduler_dynamic> m_worker_scheduler;
  std::shared_ptr<Protocol_global_config> m_config;
  std::unique_ptr<xpl::iface::Document_id_generator> m_id_generator;
  std::atomic<bool> m_gracefull_shutdown{false};

  std::unique_ptr<xpl::iface::Ssl_context> m_ssl_context;
  xpl::Sync_variable<State> m_state;
  xpl::Authentication_container m_auth_handlers;
  Client_list m_client_list;
  xpl::Mutex m_client_exit_mutex;
  Server_properties *m_properties;
  Server_task_vector m_tasks;
  std::unique_ptr<xpl::Server_factory> m_factory;
  std::shared_ptr<xpl::iface::Timeout_callback> m_timeout_callback;
};

}  // namespace ngs

#endif  // PLUGIN_X_SRC_SERVER_SERVER_H_

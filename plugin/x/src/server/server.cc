/*
 * Copyright (c) 2015, 2024, Oracle and/or its affiliates.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is designed to work with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have either included with
 * the program or referenced in the documentation.
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

#include "plugin/x/src/server/server.h"

#include <time.h>
#include <stdexcept>
#include <utility>

#include "mysql/service_ssl_wrapper.h"
#include "plugin/x/generated/mysqlx_version.h"
#include "plugin/x/src/helper/multithread/initializer.h"
#include "plugin/x/src/helper/multithread/xsync_point.h"
#include "plugin/x/src/interface/client.h"
#include "plugin/x/src/interface/connection_acceptor.h"
#include "plugin/x/src/interface/protocol_monitor.h"
#include "plugin/x/src/interface/server_task.h"
#include "plugin/x/src/interface/ssl_context.h"
#include "plugin/x/src/interface/vio.h"
#include "plugin/x/src/mysql_variables.h"
#include "plugin/x/src/ngs/document_id_generator.h"
#include "plugin/x/src/ngs/protocol/protocol_config.h"
#include "plugin/x/src/ngs/scheduler.h"
#include "plugin/x/src/ngs/server_client_timeout.h"
#include "plugin/x/src/ngs/socket_acceptors_task.h"
#include "plugin/x/src/ngs/vio_wrapper.h"
#include "plugin/x/src/server/builder/ssl_context_builder.h"
#include "plugin/x/src/sql_data_context.h"
#include "plugin/x/src/variables/system_variables.h"
#include "plugin/x/src/variables/xpl_global_status_variables.h"
#include "plugin/x/src/xpl_log.h"
#include "plugin/x/src/xpl_performance_schema.h"

#include "my_systime.h"  // my_sleep() NOLINT(build/include_subdir)

namespace ngs {

Server::Server(std::shared_ptr<Scheduler_dynamic> accept_scheduler,
               std::shared_ptr<Scheduler_dynamic> work_scheduler,
               std::shared_ptr<Protocol_global_config> config,
               Server_properties *properties, const Server_task_vector &tasks,
               std::shared_ptr<xpl::iface::Timeout_callback> timeout_callback)
    : m_timer_running(false),
      m_stop_called(false),
      m_errors_while_accepting(0),
      m_accept_scheduler(accept_scheduler),
      m_worker_scheduler(work_scheduler),
      m_config(config),
      m_id_generator(new Document_id_generator()),
      m_state(State_initializing, KEY_mutex_x_server_state_sync,
              KEY_cond_x_server_state_sync),
      m_client_exit_mutex(KEY_mutex_x_server_client_exit),
      m_properties(properties),
      m_tasks(tasks),
      m_factory(new xpl::Server_factory()),
      m_timeout_callback(timeout_callback) {}

void Server::run_task(std::shared_ptr<xpl::iface::Server_task> handler) {
  handler->pre_loop();

  while (m_state.is(State_running) && !m_gracefull_shutdown) {
    handler->loop();
  }

  handler->post_loop();
}

void Server::start_failed() {
  m_state.exchange(State_initializing, State_failure);

  for (auto &task : m_tasks) {
    task->stop(Stop_cause::k_abort);
  }
}

bool Server::is_running() {
  return m_state.is(State_running) && !mysqld::is_terminating() &&
         !m_stop_called;
}

bool Server::is_terminating() {
  State expected_states[] = {State_failure, State_terminating};
  return m_state.is(expected_states) || mysqld::is_terminating() ||
         m_stop_called;
}

void Server::delayed_start_tasks() {
  m_accept_scheduler->post([this]() {
    xpl::Server_thread_initializer thread_initializer;

    /* Wait until SQL api is ready,
       server shouldn't handle any client before that. */
    if (xpl::Sql_data_context::wait_api_ready(
            [this]() { return is_terminating(); })) {
      SYNC_POINT_CHECK("xplugin_init_wait");

      xpl::Sql_data_context sql_context;
      const bool admin_session = true;
      ngs::Error_code error = sql_context.init(admin_session);

      if (error) {
        log_error(ER_XPLUGIN_STARTUP_FAILED, error.message.c_str());
        return;
      }
      sql_context.switch_to_local_user(MYSQL_SESSION_USER);
      sql_context.attach();

      /* This method is executed inside a worker thread, thus
         its better not to swap another thread, this one can handle
         a task. */
      start_tasks();
    }
  });
}

void Server::start_tasks() {
  // We can't fetch the servers ssl config at plugin-load
  // this method allows to setup it at better time.
  m_ssl_context = xpl::Ssl_context_builder().get_result_context();

  if (m_state.exchange(State::State_initializing, State_running)) {
    for (auto task : m_tasks) {
      m_accept_scheduler->post([this, task]() { run_task(task); });
    }
  }
}

bool Server::prepare() {
  xpl::iface::Listener::On_connection on_connection =
      [this](xpl::iface::Connection_acceptor &acceptor) {
        on_accept(&acceptor);
      };
  Task_context context(on_connection, m_properties, &m_client_list);

  m_worker_scheduler->launch();
  m_accept_scheduler->launch();

  bool result = true;

  for (auto &task : m_tasks) {
    result = result && task->prepare(&context);
  }

  if (!result) {
    start_failed();

    return false;
  }

  m_timeout_callback->add_callback(1000, [this]() -> bool {
    this->on_check_terminated_workers();
    return true;
  });

  return true;
}

void Server::gracefull_shutdown() {
  log_debug("Server::graceful_shutdown state=%i",
            static_cast<int>(m_state.get()));
  m_gracefull_shutdown = true;

  if (m_state.exchange(State::State_initializing, State_failure)) {
    start_failed();
  }

  for (auto &task : m_tasks) {
    task->stop(Stop_cause::k_normal_shutdown);
  }

  graceful_close_all_clients();
}

/** Stop the network acceptor loop */
void Server::stop() {
  m_stop_called = true;
  if (m_state.exchange(State::State_initializing, State_failure)) {
    start_failed();
  }

  const State allowed_values[] = {State_failure, State_running,
                                  State_terminating};

  m_state.wait_for(allowed_values);
  if (State_terminating == m_state.set_and_return_old(State_terminating))
    return;

  for (auto &task : m_tasks) {
    task->stop(Stop_cause::k_normal_shutdown);
  }

  graceful_close_all_clients();
  wait_for_clients_closure();

  if (m_worker_scheduler) {
    m_worker_scheduler->stop();
    m_worker_scheduler.reset();
  }

  if (m_accept_scheduler) {
    m_accept_scheduler->stop();
    m_accept_scheduler.reset();
  }
}

struct Copy_client_not_closed {
  explicit Copy_client_not_closed(
      std::vector<std::shared_ptr<xpl::iface::Client>> *client_list)
      : m_client_list(client_list) {}

  bool operator()(std::shared_ptr<xpl::iface::Client> &client) {
    if (xpl::iface::Client::State::k_closed != client->get_state())
      m_client_list->push_back(client);

    // Continue enumerating
    return false;
  }

  std::vector<std::shared_ptr<xpl::iface::Client>> *m_client_list;
};

void Server::go_through_all_clients(
    std::function<void(std::shared_ptr<xpl::iface::Client>)> callback) {
  MUTEX_LOCK(lock_client_exit, m_client_exit_mutex);
  std::vector<std::shared_ptr<xpl::iface::Client>> not_closed_clients_list;
  Copy_client_not_closed matcher(&not_closed_clients_list);

  // Prolong life of clients when there are already in
  // Closing state. Client::close could access m_client_list
  // causing a deadlock thus we copied all elements
  m_client_list.enumerate(&matcher);

  for (auto client : not_closed_clients_list) {
    callback(client);
  }
}

void Server::graceful_close_all_clients() {
  using Client_ptr = std::shared_ptr<xpl::iface::Client>;
  go_through_all_clients(
      [](Client_ptr client) { client->on_server_shutdown(); });
}

void Server::wait_for_clients_closure() {
  size_t num_of_retries = 4 * 5;

  // For now lets pull the list, it should be rewritten
  // after implementation of Client timeout in closing state
  while (m_client_list.size() > 0) {
    if (0 == --num_of_retries) {
      const unsigned int num_of_clients =
          static_cast<unsigned int>(m_client_list.size());

      log_error(ER_XPLUGIN_DETECTED_HANGING_CLIENTS, num_of_clients);
      break;
    }
    my_sleep(250000);  // wait for 0.25s
  }
}

void Server::start_client_supervision_timer(
    const xpl::chrono::Duration &oldest_object_time_ms) {
  log_debug(
      "Supervision timer started %i ms",
      static_cast<int>(xpl::chrono::to_milliseconds(oldest_object_time_ms)));

  m_timer_running = true;

  m_timeout_callback->add_callback(
      static_cast<size_t>(xpl::chrono::to_milliseconds(oldest_object_time_ms)),
      std::bind(&Server::timeout_for_clients_validation, this));
}

void Server::restart_client_supervision_timer() {
  if (!m_timer_running) {
    start_client_supervision_timer(get_config()->connect_timeout);
  }
}

bool Server::timeout_for_clients_validation() {
  const xpl::chrono::Time_point time_oldest =
      xpl::chrono::now() - get_config()->connect_timeout;
  const xpl::chrono::Time_point time_to_release =
      time_oldest + get_config()->connect_timeout_hysteresis;

  Server_client_timeout client_validator(time_to_release);

  go_through_all_clients(
      [&client_validator](std::shared_ptr<xpl::iface::Client> client) {
        client_validator.validate_client_state(client);
      });

  if (xpl::chrono::is_valid(client_validator.get_oldest_client_accept_time())) {
    start_client_supervision_timer(
        client_validator.get_oldest_client_accept_time() - time_oldest);
  } else {
    start_client_supervision_timer(get_config()->connect_timeout);
  }

  return false;
}

std::shared_ptr<xpl::iface::Client> Server::will_accept_client(::Vio *vio) {
  auto clients = m_client_list.direct_access();
  auto connection = ngs::allocate_shared<Vio_wrapper>(vio);
  auto client = m_factory->create_client(this, connection);

  log_debug("num_of_connections: %i, max_num_of_connections: %i",
            static_cast<int>(clients->size()),
            static_cast<int>(xpl::Plugin_system_variables::m_max_connections));

  if (is_terminating()) return {};

  if (static_cast<int>(clients->size()) >=
      xpl::Plugin_system_variables::m_max_connections) {
    log_warning(ER_XPLUGIN_UNABLE_TO_ACCEPT_CONNECTION);
    ++xpl::Global_status_variables::instance().m_rejected_connections_count;
    return {};
  }

  clients->push_back(client);

  ++xpl::Global_status_variables::instance().m_accepted_connections_count;

  return client;
}

void Server::on_accept(xpl::iface::Connection_acceptor *connection_acceptor) {
  // That means that the event loop was just break in the stop()
  if (m_state.is(State_terminating)) return;

  // The server sends an audit event with information that its initialized
  // and we can handle incoming connections, still the state is updated
  // after sending the audit event. To synchronize with that state we must
  // wait here for srv_session api.
  if (xpl::Sql_data_context::wait_api_ready(
          [this]() { return is_terminating(); })) {
    ::Vio *vio = connection_acceptor->accept();

    if (nullptr == vio) {
      ++xpl::Global_status_variables::instance().m_connection_errors_count;
      ++xpl::Global_status_variables::instance()
            .m_connection_accept_errors_count;

      if (0 == (m_errors_while_accepting++ & 255)) {
        // error accepting client
        log_error(ER_XPLUGIN_FAILED_TO_ACCEPT_CLIENT);
      }
      const time_t microseconds_to_sleep = 100000;

      my_sleep(microseconds_to_sleep);

      return;
    }

    auto client = will_accept_client(vio);

    if (client) {
      // connection accepted, add to client list and start handshake etc
      client->reset_accept_time();

      Scheduler_dynamic::Task *task =
          ngs::allocate_object<Scheduler_dynamic::Task>(
              std::bind(&xpl::iface::Client::run, client));

      const uint64_t client_id = client->client_id_num();
      client.reset();

      // all references to client object should be removed at this thread
      if (!m_worker_scheduler->post(task)) {
        log_error(ER_XPLUGIN_FAILED_TO_SCHEDULE_CLIENT);
        free_object(task);
        m_client_list.remove(client_id);
      }

      restart_client_supervision_timer();
    }
  }
}

bool Server::on_check_terminated_workers() {
  if (m_worker_scheduler) {
    m_worker_scheduler->join_terminating_workers();
    return true;
  }
  return false;
}

std::shared_ptr<xpl::iface::Session> Server::create_session(
    xpl::iface::Client *client, xpl::iface::Protocol_encoder *proto,
    const int session_id) {
  if (is_terminating()) return std::shared_ptr<xpl::iface::Session>();

  return m_factory->create_session(client, proto, session_id);
}

void Server::on_client_closed(const xpl::iface::Client &client) {
  log_debug("%s: on_client_close", client.client_id());
  ++xpl::Global_status_variables::instance().m_closed_connections_count;
  // Lets first remove it, and then decrement the counters
  // in m_delegate.
  m_client_list.remove(client.client_id_num());
}

bool Server::reset() {
  if (m_client_list.size() != 0) return false;

  const State allowed_values[] = {State_failure, State_running,
                                  State_terminating};

  m_state.wait_for(allowed_values);

  m_ssl_context->reset();
  m_id_generator.reset(new Document_id_generator());
  m_factory.reset(new xpl::Server_factory());

  return true;
}

Error_code Server::kill_client(const uint64_t client_id,
                               xpl::iface::Session *requester) {
  std::unique_ptr<Mutex_lock> lock(
      new Mutex_lock(get_client_exit_mutex(), __FILE__, __LINE__));
  auto found_client = get_client_list().find(client_id);

  // Locking exit mutex of ensures that the client wont exit Client::run until
  // the kill command ends, and shared_ptr (found_client) will be released
  // before the exit_lock is released. Following ensures that the final instance
  // of Clients will be released in its thread (Scheduler, Client::run).

  if (found_client &&
      xpl::iface::Client::State::k_closed != found_client->get_state()) {
    if (client_id == requester->client().client_id_num()) {
      lock.reset();
      found_client->kill();
      return ngs::Success();
    }

    bool is_session = false;
    uint64_t mysql_session_id = 0;

    {
      MUTEX_LOCK(lock_session_exit, found_client->get_session_exit_mutex());
      auto session = found_client->session_shared_ptr();

      is_session = (nullptr != session.get());

      if (is_session)
        mysql_session_id = session->data_context().mysql_session_id();
    }

    if (is_session) {
      // try to kill the MySQL session
      ngs::Error_code error =
          requester->data_context().execute_kill_sql_session(mysql_session_id);
      if (error) {
        return error;
      }

      bool is_killed = false;
      {
        MUTEX_LOCK(lock_session_exit, found_client->get_session_exit_mutex());
        auto session = found_client->session_shared_ptr();

        if (session) is_killed = session->data_context().is_killed();
      }

      if (is_killed) {
        found_client->kill();
        return ngs::Success();
      }
    }

    return ngs::Error(ER_KILL_DENIED_ERROR, "Cannot kill client %s",
                      std::to_string(client_id).c_str());
  }

  return ngs::Error(ER_NO_SUCH_THREAD, "Unknown MySQLx client id %s",
                    std::to_string(client_id).c_str());
}

std::shared_ptr<xpl::iface::Client> Server::get_client(const THD *thd) {
  std::vector<std::shared_ptr<xpl::iface::Client>> clients;
  get_client_list().get_all_clients(&clients);

  for (auto &c : clients)
    if (c->is_handler_thd(thd)) return c;
  return {};
}

}  // namespace ngs

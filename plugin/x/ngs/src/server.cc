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

#include "plugin/x/ngs/include/ngs/server.h"

#include <time.h>

#include "plugin/x/generated/mysqlx_version.h"
#include "plugin/x/ngs/include/ngs/document_id_generator.h"
#include "plugin/x/ngs/include/ngs/interface/client_interface.h"
#include "plugin/x/ngs/include/ngs/interface/connection_acceptor_interface.h"
#include "plugin/x/ngs/include/ngs/interface/protocol_monitor_interface.h"
#include "plugin/x/ngs/include/ngs/interface/server_task_interface.h"
#include "plugin/x/ngs/include/ngs/interface/ssl_context_interface.h"
#include "plugin/x/ngs/include/ngs/interface/vio_interface.h"
#include "plugin/x/ngs/include/ngs/protocol/protocol_config.h"
#include "plugin/x/ngs/include/ngs/scheduler.h"
#include "plugin/x/ngs/include/ngs/server_client_timeout.h"
#include "plugin/x/ngs/include/ngs/socket_acceptors_task.h"
#include "plugin/x/ngs/include/ngs/vio_wrapper.h"
#include "plugin/x/src/xpl_log.h"

namespace ngs {

Server::Server(ngs::shared_ptr<Scheduler_dynamic> accept_scheduler,
               ngs::shared_ptr<Scheduler_dynamic> work_scheduler,
               Server_delegate *delegate,
               ngs::shared_ptr<Protocol_config> config,
               Server_properties *properties, const Server_task_vector &tasks,
               ngs::shared_ptr<Timeout_callback_interface> timeout_callback)
    : m_timer_running(false),
      m_skip_name_resolve(false),
      m_errors_while_accepting(0),
      m_accept_scheduler(accept_scheduler),
      m_worker_scheduler(work_scheduler),
      m_config(config),
      m_id_generator(new Document_id_generator()),
      m_state(State_initializing),
      m_delegate(delegate),
      m_properties(properties),
      m_tasks(tasks),
      m_timeout_callback(timeout_callback) {}

bool Server::prepare(std::unique_ptr<Ssl_context_interface> ssl_context,
                     const bool skip_networking, const bool skip_name_resolve) {
  Listener_interface::On_connection on_connection =
      [this](Connection_acceptor_interface &acceptor) { on_accept(acceptor); };

  Task_context context(on_connection, skip_networking, m_properties,
                       &m_client_list);
  m_skip_name_resolve = skip_name_resolve;
  m_ssl_context = ngs::move(ssl_context);

  bool result = true;

  for (auto &task : m_tasks) {
    result = result && task->prepare(&context);
  }

  if (result) {
    m_state.set(State_running);

    m_timeout_callback->add_callback(1000, [this]() -> bool {
      this->on_check_terminated_workers();
      return true;
    });

    return true;
  }

  return false;
}

void Server::run_task(ngs::shared_ptr<Server_task_interface> handler) {
  handler->pre_loop();

  while (m_state.is(State_running)) {
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
  return m_state.is(State_running) && !m_delegate->is_terminating();
}

bool Server::is_terminating() {
  return m_state.is(State_failure) || m_state.is(State_terminating) ||
         m_delegate->is_terminating();
}

void Server::start() {
  if (m_tasks.empty()) return;

  auto task_to_run_in_new_thread = ++m_tasks.begin();

  while (task_to_run_in_new_thread != m_tasks.end()) {
    Server_tasks_interface_ptr task = *task_to_run_in_new_thread++;

    m_accept_scheduler->post([this, task]() { run_task(task); });
  }

  run_task(*m_tasks.begin());
}

/** Stop the network acceptor loop */
void Server::stop(const bool is_called_from_timeout_handler) {
  const State allowed_values[] = {State_failure, State_running,
                                  State_terminating};

  m_state.wait_for(allowed_values);
  if (State_terminating == m_state.set_and_return_old(State_terminating))
    return;

  const Stop_cause cause = is_called_from_timeout_handler
                               ? Stop_cause::k_server_task_triggered_event
                               : Stop_cause::k_normal_shutdown;

  for (auto &task : m_tasks) {
    task->stop(cause);
  }

  close_all_clients();
  wait_for_clients_closure();

  if (m_worker_scheduler) {
    m_worker_scheduler->stop();
    m_worker_scheduler.reset();
  }
}

struct Copy_client_not_closed {
  Copy_client_not_closed(std::vector<ngs::Client_ptr> &client_list)
      : m_client_list(client_list) {}

  bool operator()(ngs::Client_ptr &client) {
    if (ngs::Client_interface::Client_closed != client->get_state())
      m_client_list.push_back(client);

    // Continue enumerating
    return false;
  }

  std::vector<ngs::Client_ptr> &m_client_list;
};

void Server::go_through_all_clients(ngs::function<void(Client_ptr)> callback) {
  MUTEX_LOCK(lock_client_exit, m_client_exit_mutex);
  std::vector<ngs::Client_ptr> client_list;
  Copy_client_not_closed matcher(client_list);

  // Prolong life of clients when there are already in
  // Closing state. Client::close could access m_client_list
  // causing a deadlock thus we copied all elements
  m_client_list.enumerate(matcher);

  std::for_each(client_list.begin(), client_list.end(), callback);
}

void Server::close_all_clients() {
  go_through_all_clients(
      ngs::bind(&Client_interface::on_server_shutdown, ngs::placeholders::_1));
}

void Server::wait_for_clients_closure() {
  size_t num_of_retries = 4 * 5;

  // TODO: For now lets pull the list, it should be rewriten
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
    const chrono::duration &oldest_object_time_ms) {
  log_debug("Supervision timer started %i ms",
            (int)chrono::to_milliseconds(oldest_object_time_ms));

  m_timer_running = true;

  m_timeout_callback->add_callback(
      static_cast<size_t>(chrono::to_milliseconds(oldest_object_time_ms)),
      ngs::bind(&Server::timeout_for_clients_validation, this));
}

void Server::restart_client_supervision_timer() {
  if (!m_timer_running) {
    start_client_supervision_timer(get_config()->connect_timeout);
  }
}

bool Server::timeout_for_clients_validation() {
  m_timer_running = false;

  log_debug("Supervision timeout - started client state verification");

  const chrono::time_point time_oldest =
      chrono::now() - get_config()->connect_timeout;
  const chrono::time_point time_to_release =
      time_oldest + get_config()->connect_timeout_hysteresis;

  Server_client_timeout client_validator(time_to_release);

  go_through_all_clients(
      ngs::bind(&Server_client_timeout::validate_client_state,
                &client_validator, ngs::placeholders::_1));

  if (chrono::is_valid(client_validator.get_oldest_client_accept_time())) {
    start_client_supervision_timer(
        client_validator.get_oldest_client_accept_time() - time_oldest);
  }
  return false;
}

void Server::on_accept(Connection_acceptor_interface &connection_acceptor) {
  // That means that the event loop was just break in the stop()
  if (m_state.is(State_terminating)) return;

  Vio *vio = connection_acceptor.accept();

  if (NULL == vio) {
    m_delegate->did_reject_client(Server_delegate::AcceptError);

    if (0 == (m_errors_while_accepting++ & 255)) {
      // error accepting client
      log_error(ER_XPLUGIN_FAILED_TO_ACCEPT_CLIENT);
    }
    const time_t microseconds_to_sleep = 100000;

    my_sleep(microseconds_to_sleep);

    return;
  }

  ngs::shared_ptr<Vio_interface> connection(
      ngs::allocate_shared<Vio_wrapper>(vio));
  ngs::shared_ptr<Client_interface> client(
      m_delegate->create_client(connection));

  if (m_delegate->will_accept_client(*client)) {
    m_delegate->did_accept_client(*client);

    // connection accepted, add to client list and start handshake etc
    client->reset_accept_time();
    m_client_list.add(client);

    Scheduler_dynamic::Task *task =
        ngs::allocate_object<Scheduler_dynamic::Task>(ngs::bind(
            &ngs::Client_interface::run, client, m_skip_name_resolve));

    const uint64_t client_id = client->client_id_num();
    client.reset();

    // all references to client object should be removed at this thread
    if (!m_worker_scheduler->post(task)) {
      log_error(ER_XPLUGIN_FAILED_TO_SCHEDULE_CLIENT);
      ngs::free_object(task);
      m_client_list.remove(client_id);
    }

    restart_client_supervision_timer();
  } else {
    m_delegate->did_reject_client(Server_delegate::TooManyConnections);
    log_warning(ER_XPLUGIN_UNABLE_TO_ACCEPT_CONNECTION);
  }
}

bool Server::on_check_terminated_workers() {
  if (m_worker_scheduler) {
    m_worker_scheduler->join_terminating_workers();
    return true;
  }
  return false;
}

ngs::shared_ptr<Session_interface> Server::create_session(
    Client_interface &client, Protocol_encoder_interface &proto,
    const int session_id) {
  if (is_terminating()) return ngs::shared_ptr<Session_interface>();

  return m_delegate->create_session(client, proto, session_id);
}

void Server::on_client_closed(const Client_interface &client) {
  log_debug("%s: on_client_close", client.client_id());
  m_delegate->on_client_closed(client);

  m_client_list.remove(client.client_id_num());
}

void Server::add_authentication_mechanism(
    const std::string &name, Authentication_interface::Create initiator,
    const bool allowed_only_with_secure_connection) {
  Authentication_key key(name, allowed_only_with_secure_connection);

  m_auth_handlers[key] = initiator;
}

void Server::add_sha256_password_cache(SHA256_password_cache_interface *cache) {
  m_sha256_password_cache = cache;
}

Authentication_interface_ptr Server::get_auth_handler(
    const std::string &name, Session_interface *session) {
  Connection_type type = session->client().connection().get_type();

  Authentication_key key(name, Connection_type_helper::is_secure_type(type));

  Auth_handler_map::const_iterator auth_handler = m_auth_handlers.find(key);

  if (auth_handler == m_auth_handlers.end())
    return Authentication_interface_ptr();

  return auth_handler->second(session, m_sha256_password_cache);
}

void Server::get_authentication_mechanisms(std::vector<std::string> &auth_mech,
                                           Client_interface &client) {
  const Connection_type type = client.connection().get_type();
  const bool is_secure = Connection_type_helper::is_secure_type(type);

  auth_mech.clear();

  auth_mech.reserve(m_auth_handlers.size());

  Auth_handler_map::const_iterator i = m_auth_handlers.begin();

  while (m_auth_handlers.end() != i) {
    if (i->first.must_be_secure_connection == is_secure)
      auth_mech.push_back(i->first.name);
    ++i;
  }
}

void Server::add_callback(const std::size_t delay_ms,
                          ngs::function<bool()> callback) {
  m_timeout_callback->add_callback(delay_ms, callback);
}

bool Server::reset_globals() {
  if (m_client_list.size() != 0) return false;

  const State allowed_values[] = {State_failure, State_running,
                                  State_terminating};

  m_state.wait_for(allowed_values);

  m_ssl_context->reset();
  m_id_generator.reset(new Document_id_generator());

  return true;
}

}  // namespace ngs

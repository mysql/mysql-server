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

#if !defined(MYSQL_DYNAMIC_PLUGIN) && defined(WIN32) && !defined(XPLUGIN_UNIT_TESTS)
// Needed for importing PERFORMANCE_SCHEMA plugin API.
#define MYSQL_DYNAMIC_PLUGIN 1
#endif // WIN32

#include "ngs/server.h"
#include "ngs/interface/client_interface.h"
#include "ngs/interface/connection_acceptor_interface.h"
#include "ngs/interface/server_task_interface.h"
#include "ngs/server_acceptors.h"
#include "ngs/scheduler.h"
#include "ngs/protocol_monitor.h"
#include "ngs/protocol/protocol_config.h"
#include "ngs_common/connection_vio.h"
#include "xpl_log.h"
#include "mysqlx_version.h"


using namespace ngs;

Server::Server(ngs::shared_ptr<Server_acceptors>  acceptors,
               ngs::shared_ptr<Scheduler_dynamic> accept_scheduler,
               ngs::shared_ptr<Scheduler_dynamic> work_scheduler,
               Server_delegate *delegate,
               ngs::shared_ptr<Protocol_config> config)
: m_timer_running(false),
  m_skip_name_resolve(false),
  m_errors_while_accepting(0),
  m_acceptors(acceptors),
  m_accept_scheduler(accept_scheduler),
  m_worker_scheduler(work_scheduler),
  m_config(config),
  m_state(State_initializing),
  m_delegate(delegate)
{
}

bool Server::prepare(Ssl_context_unique_ptr ssl_context, const bool skip_networking, const bool skip_name_resolve, const bool use_unix_sockets)
{
  Listener_interface::On_connection on_connection = ngs::bind(&Server::on_accept, this, ngs::placeholders::_1);

  m_skip_name_resolve = skip_name_resolve;
  m_ssl_context = ngs::move(ssl_context);

  const bool result = m_acceptors->prepare(on_connection, skip_networking, use_unix_sockets);

  if (result)
  {
    m_state.set(State_running);

    m_acceptors->add_timer(1000, ngs::bind(&Server::on_check_terminated_workers, this));

    return true;
  }

  return false;
}

void Server::run_task(ngs::shared_ptr<Server_task_interface> handler)
{
  handler->pre_loop();

  while (m_state.is(State_running))
  {
    handler->loop();
  }

  handler->post_loop();
}

void Server::start_failed()
{
  m_state.exchange(State_initializing, State_failure);
  m_acceptors->abort();
}

bool Server::is_running()
{
  return m_state.is(State_running) && !m_delegate->is_terminating();
}

bool Server::is_terminating()
{
  return m_state.is(State_failure) || m_state.is(State_terminating) || m_delegate->is_terminating();
}

void Server::start()
{
  Server_tasks_interfaces handlers = m_acceptors->create_server_tasks_for_listeners();
  Server_tasks_interfaces::iterator handler_iterator = handlers.begin();

  if (handler_iterator == handlers.end())
    return;

  ngs::shared_ptr<Server_task_interface> handler_to_run_in_current_thread = *(handler_iterator++);

  while(handlers.end() != handler_iterator)
  {
    m_accept_scheduler->post(ngs::bind(&Server::run_task, this, (*handler_iterator)));

    ++handler_iterator;
  }

  run_task(handler_to_run_in_current_thread);
}

/** Stop the network acceptor loop */
void Server::stop(const bool is_called_from_timeout_handler)
{
  const State allowed_values[] = {State_failure, State_running, State_terminating};

  m_state.wait_for(allowed_values);
  if (State_terminating == m_state.set_and_return_old(State_terminating))
    return;

  m_acceptors->stop(is_called_from_timeout_handler);

  close_all_clients();

  wait_for_clients_closure();

  if (m_worker_scheduler)
  {
    m_worker_scheduler->stop();
    m_worker_scheduler.reset();
  }
}

struct Copy_client_not_closed
{
  Copy_client_not_closed(std::vector<ngs::Client_ptr> &client_list)
  : m_client_list(client_list)
  {
  }

  bool operator() (ngs::Client_ptr &client)
  {
    if (ngs::Client_interface::Client_closed != client->get_state())
      m_client_list.push_back(client);

    // Continue enumerating
    return false;
  }

  std::vector<ngs::Client_ptr> &m_client_list;
};

void Server::go_through_all_clients(ngs::function<void (Client_ptr)> callback)
{
  Mutex_lock lock_client_exit(m_client_exit_mutex);
  std::vector<ngs::Client_ptr> client_list;
  Copy_client_not_closed matcher(client_list);

  // Prolong life of clients when there are already in
  // Closing state. Client::close could access m_client_list
  // causing a deadlock thus we copied all elements
  m_client_list.enumerate(matcher);

  std::for_each(client_list.begin(), client_list.end(), callback);
}

void Server::close_all_clients()
{
  go_through_all_clients(ngs::bind(&Client_interface::on_server_shutdown, ngs::placeholders::_1));
}

void Server::wait_for_clients_closure()
{
  size_t num_of_retries = 4 * 5;

  //TODO: For now lets pull the list, it should be rewriten
  // after implementation of Client timeout in closing state
  while (m_client_list.size() > 0)
  {
    if (0 == --num_of_retries)
    {
      const unsigned int num_of_clients = static_cast<unsigned int>(m_client_list.size());

      log_error("Detected %u hanging client", num_of_clients);
      break;
    }
    my_sleep(250000); // wait for 0.25s
  }
}

void Server::validate_client_state(chrono::time_point &oldest_client_time, const chrono::time_point& time_of_release, Client_ptr client)
{
  const chrono::time_point client_time = client->get_accept_time();
  const Client_interface::Client_state state = client->get_state();

  if (Client_interface::Client_accepted_with_session != state &&
      Client_interface::Client_running != state &&
      Client_interface::Client_closing != state)
  {
    if (client_time <= time_of_release)
    {
      log_info("%s: release triggered by timeout in state:%i", client->client_id(), static_cast<int>(client->get_state()));
      client->on_auth_timeout();
      return;
    }

    if (!chrono::is_valid(oldest_client_time) ||
        oldest_client_time > client_time)
    {
      oldest_client_time = client_time;
    }
  }
}

void Server::start_client_supervision_timer(const chrono::duration &oldest_object_time_ms)
{
  log_debug("Supervision timer started %i ms", (int)chrono::to_milliseconds(oldest_object_time_ms));

  m_timer_running = true;

  m_acceptors->add_timer(static_cast<size_t>(chrono::to_milliseconds(oldest_object_time_ms)),
            ngs::bind(&Server::timeout_for_clients_validation, this));
}

void Server::restart_client_supervision_timer()
{
  if (!m_timer_running)
  {
    start_client_supervision_timer(get_config()->connect_timeout);
  }
}

bool Server::timeout_for_clients_validation()
{
  m_timer_running = false;

  chrono::time_point oldest_object_time;

  log_info("Supervision timeout - started client state verification");

  chrono::time_point time_oldest = chrono::now() - get_config()->connect_timeout;
  chrono::time_point time_to_release = time_oldest + get_config()->connect_timeout_hysteresis;

  go_through_all_clients(ngs::bind(&Server::validate_client_state, this, ngs::ref(oldest_object_time), time_to_release, ngs::placeholders::_1));

  if (chrono::is_valid(oldest_object_time))
  {
    start_client_supervision_timer(oldest_object_time - time_oldest);
  }
  return false;
}


void Server::on_accept(Connection_acceptor_interface &connection_acceptor)
{
  // That means that the event loop was just break in the stop()
  if (m_state.is(State_terminating))
    return;

  Vio *vio = connection_acceptor.accept();

  if (NULL == vio)
  {
    m_delegate->did_reject_client(Server_delegate::AcceptError);

    if (0 == (m_errors_while_accepting++ & 255))
    {
      // error accepting client
      log_error("Error accepting client");
    }
    const time_t microseconds_to_sleep = 100000;

    my_sleep(microseconds_to_sleep);

    return;
  }

  Connection_ptr connection(ngs::allocate_shared<ngs::Connection_vio>(ngs::ref(*m_ssl_context), vio));
  ngs::shared_ptr<Client_interface> client(m_delegate->create_client(connection));

  if (m_delegate->will_accept_client(*client))
  {
    m_delegate->did_accept_client(*client);

    // connection accepted, add to client list and start handshake etc
    m_client_list.add(client);

    Scheduler_dynamic::Task *task = ngs::allocate_object<Scheduler_dynamic::Task>(ngs::bind(&ngs::Client_interface::run, client,
                    m_skip_name_resolve));

    const uint64_t client_id = client->client_id_num();
    client.reset();

    // all references to client object should be removed at this thread
    if (!m_worker_scheduler->post(task))
    {
      log_error("Internal error scheduling client for execution");
      ngs::free_object(task);
      m_client_list.remove(client_id);
    }

    restart_client_supervision_timer();
  }
  else
  {
    m_delegate->did_reject_client(Server_delegate::TooManyConnections);
    log_warning("Unable to accept connection, disconnecting client");
  }
}

bool Server::on_check_terminated_workers()
{
  if (m_worker_scheduler)
  {
    m_worker_scheduler->join_terminating_workers();
    return true;
  }
  return false;
}

ngs::shared_ptr<Session_interface> Server::create_session(Client_interface &client,
                                                  Protocol_encoder &proto,
                                                  int session_id)
{
  if (is_terminating())
    return ngs::shared_ptr<Session_interface>();

  return m_delegate->create_session(client, proto, session_id);
}


void Server::on_client_closed(const Client_interface &client)
{
  log_debug("%s: on_client_close", client.client_id());
  m_delegate->on_client_closed(client);

  m_client_list.remove(client.client_id_num());
}


void Server::add_authentication_mechanism(const std::string &name,
                                          Authentication_handler::create initiator,
                                          const bool allowed_only_with_secure_connection)
{
  Authentication_key key(name, allowed_only_with_secure_connection);

  m_auth_handlers[key] = initiator;
}

Authentication_handler_ptr Server::get_auth_handler(const std::string &name, Session_interface *session)
{
  Connection_type type = session->client().connection().connection_type();
  Authentication_key key(name, Connection_type_helper::is_secure_type(type));

  Auth_handler_map::const_iterator auth_handler = m_auth_handlers.find(key);

  if (auth_handler == m_auth_handlers.end())
    return Authentication_handler_ptr();

  return auth_handler->second(session);
}

void Server::get_authentication_mechanisms(std::vector<std::string> &auth_mech, Client_interface &client)
{
  bool tls_active = client.connection().options()->active_tls();

  auth_mech.clear();

  auth_mech.reserve(m_auth_handlers.size());

  Auth_handler_map::const_iterator i = m_auth_handlers.begin();

  while (m_auth_handlers.end() != i)
  {
    if (i->first.should_be_tls_active == tls_active)
      auth_mech.push_back(i->first.name);
    ++i;
  }
}

void Server::add_timer(const std::size_t delay_ms, ngs::function<bool ()> callback)
{
  m_acceptors->add_timer(delay_ms, callback);
}

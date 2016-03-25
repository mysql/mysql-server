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

#ifdef WIN32
// Needed for importing PERFORMANCE_SCHEMA plugin API.
#define MYSQL_DYNAMIC_PLUGIN 1
#endif // WIN32

#include "ngs/server.h"
#include "ngs/client.h"
#include "ngs/scheduler.h"
#include "ngs/protocol_monitor.h"
#include "ngs/protocol/protocol_config.h"
#include "ngs_common/connection_vio.h"
#include "xpl_log.h"

using namespace ngs;


Server::Server(my_socket tcp_socket, boost::shared_ptr<Scheduler_dynamic> work_scheduler,
               Server_delegate *delegate,
               boost::shared_ptr<Protocol_config> config)
: m_tcp_socket(tcp_socket),
  m_worker_scheduler(work_scheduler),
  m_timer_running(false),
  m_skip_name_resolve(false),
  m_acceptor_state(State_acceptor_initializing),
  m_state(State_initializing),
  m_delegate(delegate),
  m_config(config)
{
  m_evbase = event_base_new();
  if (!m_evbase)
    throw std::bad_alloc();
}

Server::~Server()
{
  for (std::vector<Timer_data*>::iterator it = m_timers.begin(); it != m_timers.end(); ++it)
  {
    evtimer_del(&(*it)->ev);
    delete *it;
  }

  event_base_free(m_evbase);

  if (INVALID_SOCKET != m_tcp_socket)
    Connection_vio::close_socket(m_tcp_socket);

//  stop();
}


void Server::set_ssl_context(Ssl_context_unique_ptr ssl_context)
{
  m_ssl_context = boost::move(ssl_context);
}


bool Server::setup_accept()
{
  if (INVALID_SOCKET == m_tcp_socket)
  {
    log_error("Tcp socket creation or bind failed");
    return false;
  }

  event_set(&m_tcp_event, m_tcp_socket, EV_READ|EV_PERSIST, &Server::on_accept, this);
  event_base_set(m_evbase, &m_tcp_event);

  event_add(&m_tcp_event, NULL);

  return true;
}


bool Server::prepare(const bool skip_networking, const bool skip_name_resolve)
{
  m_skip_name_resolve = skip_name_resolve;
  if (!skip_networking)
  {
    if (!setup_accept())
      return false;
    add_timer(1000, boost::bind(&Server::on_check_terminated_workers, this));
  }
  else
  {
    log_warning("X Plugin disabled because TCP network support disabled");
    return false;
  }
  return true;
}


bool Server::run()
{
  m_state.set(State_running);
  // run the libevent event loop
  while (m_state.is(State_running))
  {
    event_base_loop(m_evbase, 0);
  }
  m_acceptor_state.set(State_acceptor_stopped);
  return true;
}


void Server::start_failed()
{
  stop_accepting_connections();
  m_state.exchange(State_initializing, State_failure);
}

bool Server::is_running()
{
  return m_state.is(State_running) && !m_delegate->is_terminating();
}

bool Server::is_terminating()
{
  return m_state.is(State_terminating) || m_delegate->is_terminating();
}


void Server::timeout_call(int sock, short which, void *arg)
{
  Timer_data *data = (Timer_data*)arg;
  if (!data->callback())
  {
    evtimer_del(&data->ev);
    {
      Mutex_lock timer_lock(data->self->m_timers_mutex);
      data->self->m_timers.erase(std::remove(data->self->m_timers.begin(), data->self->m_timers.end(), data),
                data->self->m_timers.end());
    }
    delete data;
  }
  else
  {
    // schedule for another round
    evtimer_add(&data->ev, &data->tv);
  }
}


/** Register a callback to be executed in a fixed time interval.

The callback is called from the server's event loop thread, until either
the server is stopped or the callback returns false.

NOTE: This method may only be called from the same thread as the event loop.
*/
void Server::add_timer(std::size_t delay_ms, boost::function<bool ()> callback)
{
  Timer_data *data = new Timer_data();
  data->tv.tv_sec = delay_ms / 1000;
  data->tv.tv_usec = (delay_ms % 1000) * 1000;
  data->callback = callback;
  data->self = this;
  //XXX use persistent timer events after switch to libevent2
  evtimer_set(&data->ev, timeout_call, data);
  event_base_set(m_evbase, &data->ev);
  evtimer_add(&data->ev, &data->tv);

  Mutex_lock lock(m_timers_mutex);
  m_timers.push_back(data);
}


/** Stop the network acceptor loop

Must be called from acceptor thread */
void Server::stop()
{
  const State allowed_values[] = {State_failure, State_running, State_terminating};

  m_state.wait_for(allowed_values);
  if (State_terminating == m_state.set_and_return_old(State_terminating))
    return;
  //NOTE: this needs to be called from the same thread as the event loop (that is, from an event handler)
  event_base_loopbreak(m_evbase);

  // Release of server shouldn't depend on propagation of dtors
  // inside this object (sequence of fields). Best way is to manually
  // kill all clients and schedulers explicit in dtor.
  stop_accepting_connections();

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
    if (ngs::Client::Client_closed != client->get_state())
      m_client_list.push_back(client);

    // Continue enumerating
    return false;
  }

  std::vector<ngs::Client_ptr> &m_client_list;
};

void Server::go_through_all_clients(boost::function<void (Client_ptr)> callback)
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

void Server::stop_accepting_connections()
{
  const State_acceptor expected[] = {State_acceptor_initializing, State_acceptor_stopped};
  m_acceptor_state.wait_for_and_set(expected, State_acceptor_stopped);

  Connection_vio::close_socket(m_tcp_socket);
  m_tcp_socket = INVALID_SOCKET;
}

void Server::close_all_clients()
{
  go_through_all_clients(boost::bind(&Client::on_server_shutdown, _1));
}

void Server::wait_for_clients_closure()
{
  size_t num_of_retries = 4 * 5;

  //TODO: For now lets pull the list, it should be rewriten
  // after implementation of Client timeout in closing state
  // temporary fix ngs::Client::num_of_instances > 0
  while (m_client_list.size() > 0)
  {
    if (0 == --num_of_retries)
    {
      const unsigned int num_of_clients = m_client_list.size();

      log_error("Detected %u/%u hanging client", num_of_clients, ngs::Client::num_of_instances.load());
      break;
    }
    my_sleep(250000); // wait for 0.25s
  }
}

void Server::validate_client_state(ptime &oldest_client_time, const ptime& time_of_release, Client_ptr client)
{
  const ptime                client_time = client->get_accept_time();
  const Client::Client_state state = client->get_state();

  if (Client::Client_accepted_with_session != state &&
      Client::Client_running != state &&
      Client::Client_closing != state)
  {
    if (client_time <= time_of_release)
    {
      log_info("%s: release triggered by timeout in state:%i", client->client_id(), static_cast<int>(client->get_state()));
      client->on_auth_timeout();
      return;
    }

    if (oldest_client_time.is_not_a_date_time() ||
        oldest_client_time > client_time)
    {
      oldest_client_time = client_time;
    }
  }
}

void Server::start_client_supervision_timer(const time_duration &oldest_object_time_ms)
{
  log_debug("Supervision timer started %i ms", oldest_object_time_ms.total_milliseconds());

  m_timer_running = true;

  add_timer(static_cast<size_t>(oldest_object_time_ms.total_milliseconds()),
            boost::bind(&Server::timeout_for_clients_validation, this));
}

void Server::restart_client_supervision_timer()
{
  if (!m_timer_running)
  {
    start_client_supervision_timer(config()->connect_timeout);
  }
}

bool Server::timeout_for_clients_validation()
{
  m_timer_running = false;

  ptime oldest_object_time(not_a_date_time);

  log_info("Supervision timeout - started client state verification");

  ptime time_oldest = microsec_clock::universal_time() - config()->connect_timeout;
  ptime time_to_release = time_oldest + config()->connect_timeout_hysteresis;

  go_through_all_clients(boost::bind(&Server::validate_client_state, this, boost::ref(oldest_object_time), time_to_release, _1));

  if (!oldest_object_time.is_not_a_date_time())
  {
    start_client_supervision_timer(oldest_object_time - time_oldest);
  }
  return false;
}


void Server::on_accept(int sock, short what, void *ctx)
{
  Server *self = (Server*)ctx;
  struct sockaddr_in accept_address;
  socklen_t accept_len = sizeof(accept_address);

  // That means that the event loop was just break in the stop()
  if (self->m_state.is(State_terminating))
    return;

  int err = 0;
  std::string strerr;
  my_socket nsock = Connection_vio::accept(sock, (struct sockaddr*)&accept_address, accept_len, err, strerr);

  if (err != 0)
  {
    self->m_delegate->did_reject_client(Server_delegate::AcceptError);

    // error accepting client
    log_error("Error accepting client: "
              "Error code: %s (%d)",
              strerr.c_str(), err);
  }
  else
  {
    Connection_ptr connection(new ngs::Connection_vio(*self->m_ssl_context, nsock));
    boost::shared_ptr<Client> client(self->m_delegate->create_client(connection));

    if (self->m_delegate->will_accept_client(client))
    {
      self->m_delegate->did_accept_client(client);

      // connection accepted, add to client list and start handshake etc
      self->m_client_list.add(client);

      Scheduler_dynamic::Task *task = new Scheduler_dynamic::Task(boost::bind(&ngs::Client::run, client,
                      self->m_skip_name_resolve, accept_address));

      const uint64_t client_id = client->client_id_num();
      client.reset();

      // all references to client object should be removed at this thread
      if (!self->m_worker_scheduler->post(task))
      {
        log_error("Internal error scheduling client for execution");
        delete task;
        self->m_client_list.remove(client_id);
      }

      self->restart_client_supervision_timer();
    }
    else
    {
      self->m_delegate->did_reject_client(Server_delegate::TooManyConnections);
      log_warning("Unable to accept connection, disconnecting client");
    }
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

boost::shared_ptr<Session> Server::create_session(boost::shared_ptr<Client> client,
                                                  Protocol_encoder *proto,
                                                  int session_id)
{
  if (is_terminating())
    return boost::shared_ptr<Session>();

  return m_delegate->create_session(client, proto, session_id);
}


void Server::on_client_closed(boost::shared_ptr<Client> client)
{
  log_debug("%s: on_client_close", client->client_id());
  m_delegate->on_client_closed(client);

  m_client_list.remove(client);
}


void Server::add_authentication_mechanism(const std::string &name,
                                          Authentication_handler::create initiator,
                                          const bool allowed_only_with_tls)
{
  Auth_key key(name, allowed_only_with_tls);

  m_auth_handlers[key] = initiator;
}

Authentication_handler_ptr Server::get_auth_handler(const std::string &name, Session *session)
{
  bool     tls_active = session->client().connection().options()->active_tls();
  Auth_key key(name, tls_active);

  Auth_handler_map::const_iterator auth_handler = m_auth_handlers.find(key);

  if (auth_handler == m_auth_handlers.end())
    return Authentication_handler_ptr();

  return auth_handler->second(session);
}

void Server::get_authentication_mechanisms(std::vector<std::string> &auth_mech, Client &client)
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

Client_list::Client_list()
: m_clients_lock(KEY_rwlock_x_client_list_clients)
{
}

Client_list::~Client_list()
{
}

void Client_list::add(boost::shared_ptr<Client> client)
{
  RWLock_writelock guard(m_clients_lock);
  m_clients.push_back(client);
}

void Client_list::remove(boost::shared_ptr<Client> client)
{
  RWLock_writelock guard(m_clients_lock);
  m_clients.remove(client);
}

void Client_list::remove(const uint64_t client_id)
{
  RWLock_writelock guard(m_clients_lock);
  Match_client matcher(client_id);

  std::remove_if(m_clients.begin(), m_clients.end(), matcher);
}

Client_list::Match_client::Match_client(uint64_t client_id)
: m_id(client_id)
{
}

bool Client_list::Match_client::operator () (Client_ptr client)
{
  if (client->client_id_num() == m_id)
  {
    return true;
  }

  return false;
}

Client_ptr Client_list::find(uint64_t client_id)
{
  RWLock_readlock guard(m_clients_lock);
  Match_client matcher(client_id);

  std::list<Client_ptr>::iterator i = std::find_if(m_clients.begin(), m_clients.end(), matcher);

  if (m_clients.end() == i)
    return Client_ptr();

  return *i;
}


size_t Client_list::size()
{
  RWLock_readlock guard(m_clients_lock);

  return m_clients.size();
}


void Client_list::get_all_clients(std::vector<Client_ptr> &result)
{
  RWLock_readlock guard(m_clients_lock);

  result.clear();
  result.reserve(m_clients.size());

  std::copy(m_clients.begin(), m_clients.end(), std::back_inserter(result));
}

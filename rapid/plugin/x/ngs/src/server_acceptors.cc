/*
 * Copyright (c) 2016 Oracle and/or its affiliates. All rights reserved.
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

#include "ngs/server_acceptors.h"
#include "ngs_common/connection_vio.h"
#include "ngs/log.h"
#include <boost/bind.hpp>
#include <iterator>
#include <stdlib.h>


using namespace ngs;

namespace details
{

class Server_task_listener: public Server_task_interface
{
public:
  Server_task_listener(Listener_interface &listener)
  : m_listener(listener)
  {
  }

  void pre_loop()
  {
    m_listener.get_state().set(State_listener_running);
  }

  void post_loop()
  {
    m_listener.get_state().set(State_listener_stopped);
  }

  void loop()
  {
    m_listener.loop();
  }

private:
  Listener_interface &m_listener;
};

} // namespace details


class Server_acceptors::Server_task_time_and_event: public Server_task_interface
{
public:
  Server_task_time_and_event(Time_and_socket_events &event, Listener_interface::Sync_variable_state &state)
  : m_event(event), m_state(state)
  {
  }

  // Handler_interface
  void pre_loop()
  {
    m_state.set(State_listener_running);

    std::for_each(m_listeners.begin(),
                  m_listeners.end(),
                  &Server_task_time_and_event::set_listeners_state<State_listener_running>);
  }

  void post_loop()
  {
    m_state.set(State_listener_stopped);

    std::for_each(m_listeners.begin(),
                  m_listeners.end(),
                  &Server_task_time_and_event::set_listeners_state<State_listener_stopped>);
  }

  void loop()
  {
    m_event.loop();
  }

  // Handle_time_and_event
  void listener_register(Listener_interface *listener)
  {
    m_listeners.push_back(listener);
  }

private:

  template<State_listener state>
  static void set_listeners_state(Listener_interface *listener)
  {
    listener->get_state().set(state);
  }

  Time_and_socket_events &m_event;
  Listener_interface::Sync_variable_state &m_state;
  Server_acceptors::Listener_interfaces m_listeners;
};


Server_acceptors::Server_acceptors(
    Listener_factory_interface &listener_factory,
    const unsigned short tcp_port,
    const std::string &unix_socket_file_or_named_pipe, const uint32 backlog)
: m_tcp_socket(listener_factory.create_tcp_socket_listener(tcp_port, m_event, backlog)),
#if defined(HAVE_SYS_UN_H)
  m_unix_socket(listener_factory.create_unix_socket_listener(unix_socket_file_or_named_pipe, m_event, backlog)),
#endif
  m_time_and_event_state(State_listener_initializing),
  m_time_and_event_task(new Server_task_time_and_event(m_event, m_time_and_event_state))
{
}

bool Server_acceptors::prepare_impl(On_connection on_connection, const bool skip_networking, const bool use_unix_sockets_or_named_pipes)
{
  if (skip_networking)
    m_tcp_socket.reset();

  if (!use_unix_sockets_or_named_pipes)
    m_unix_socket.reset();

  Listener_interfaces listeners = get_array_of_listeners();

  if (listeners.empty())
  {
    log_warning("All I/O interfaces are disabled, X Protocol won't be accessible");

    return false;
  }

  const size_t number_of_prepared_listeners = std::count_if(
      listeners.begin(),
      listeners.end(),
      boost::bind(&Listener_interface::setup_listener, _1, on_connection));

  if (0 == number_of_prepared_listeners)
  {
    abort();

    log_error("Preparation of I/O interfaces failed, X Protocol won't be accessible");

    return false;
  }

  return true;
}

bool Server_acceptors::prepare(On_connection on_connection, const bool skip_networking, const bool use_unix_sockets_or_named_pipes)
{
  const bool result = prepare_impl(on_connection, skip_networking, use_unix_sockets_or_named_pipes);

  Listener_interfaces listeners = get_array_of_listeners();

  std::for_each(
      listeners.begin(),
      listeners.end(),
      Server_acceptors::report_listener_status);

  return result;
}

void Server_acceptors::abort()
{
  Listener_interfaces listeners = get_array_of_listeners();

  std::for_each(
      listeners.begin(),
      listeners.end(),
      &Server_acceptors::close_listener);

  m_time_and_event_state.set(State_listener_stopped);

  std::for_each(
      listeners.begin(),
      listeners.end(),
      Server_acceptors::mark_as_stopped);
}

void Server_acceptors::stop(const bool is_called_from_timeout_handler)
{
  Listener_interfaces listeners = get_array_of_listeners();

  m_event.break_loop();

  std::for_each(
      listeners.begin(),
      listeners.end(),
      &Server_acceptors::close_listener);

  if (!is_called_from_timeout_handler)
    m_time_and_event_state.wait_for(State_listener_stopped);

  std::for_each(
      listeners.begin(),
      listeners.end(),
      &Server_acceptors::wait_until_stopped);
}

bool Server_acceptors::was_unix_socket_or_named_pipe_configured()
{
  Listener_interface *listener = m_unix_socket.get();

  if (NULL == listener)
    return false;

  const State_listener allowed_states[] = {State_listener_prepared, State_listener_running};

  return listener->get_state().is(allowed_states);
}

void Server_acceptors::add_timer(const std::size_t delay_ms, boost::function<bool ()> callback)
{
  m_event.add_timer(delay_ms, callback);
}

Server_tasks_interfaces Server_acceptors::create_server_tasks_for_listeners()
{
  Listener_interfaces listeners = get_array_of_listeners();
  Server_tasks_interfaces  handlers;

  handlers.push_back(m_time_and_event_task);

  for(Listener_interfaces::iterator i = listeners.begin();
      listeners.end() != i;
      ++i)
  {
    Listener_interface *listener = (*i);

    if (!listener->get_state().is(State_listener_prepared))
      continue;

    if (listener->is_handled_by_socket_event())
    {
      m_time_and_event_task->listener_register(listener);
      continue;
    }

    boost::shared_ptr<Server_task_interface> handler(new details::Server_task_listener(*listener));
    handlers.push_back(handler);
  }

  return handlers;
}

Server_acceptors::Listener_interfaces Server_acceptors::get_array_of_listeners()
{
  Listener_interfaces result;

  if (m_tcp_socket)
    result.push_back(m_tcp_socket.get());

  if (m_unix_socket)
    result.push_back(m_unix_socket.get());

  return result;
}

void Server_acceptors::wait_until_stopped(Listener_interface *listener)
{
  if (listener->is_handled_by_socket_event())
    return;

  listener->get_state().wait_for(State_listener_stopped);
}

void Server_acceptors::close_listener(Listener_interface *listener)
{
  listener->close_listener();
}

void Server_acceptors::mark_as_stopped(Listener_interface *listener)
{
  listener->get_state().set(State_listener_stopped);
}

void Server_acceptors::report_listener_status(Listener_interface *listener)
{
  if (!listener->get_state().is(State_listener_prepared))
  {
    log_error("X Plugin failed to setup %s, with:", listener->get_name_and_configuration().c_str());
    log_error("%s", listener->get_last_error().c_str());

    const std::string listener_configuration_variable = listener->get_configuration_variable();

    if (!listener_configuration_variable.empty())
    {
      log_info("Please see the MySQL documentation for '%s' system variable to fix the error", listener_configuration_variable.c_str());
    }

    return;
  }

  log_info("X Plugin listens on %s", listener->get_name_and_configuration().c_str());
}

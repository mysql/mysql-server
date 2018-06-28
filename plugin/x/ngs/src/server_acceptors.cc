/*
 * Copyright (c) 2016, 2018, Oracle and/or its affiliates. All rights reserved.
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

#include "plugin/x/ngs/include/ngs/server_acceptors.h"

#include "my_config.h"

#include <stdlib.h>
#include <algorithm>
#include <cctype>
#include <iterator>

#include "plugin/x/ngs/include/ngs/log.h"
#include "plugin/x/ngs/include/ngs_common/bind.h"
#include "plugin/x/ngs/include/ngs_common/string_formatter.h"

using namespace ngs;

namespace details {

class Server_task_listener : public Server_task_interface {
 public:
  Server_task_listener(Listener_interface &listener) : m_listener(listener) {}

  void pre_loop() { m_listener.get_state().set(State_listener_running); }

  void post_loop() { m_listener.get_state().set(State_listener_stopped); }

  void loop() { m_listener.loop(); }

 private:
  Listener_interface &m_listener;
};

}  // namespace details

class Server_acceptors::Server_task_time_and_event
    : public Server_task_interface {
 public:
  Server_task_time_and_event(Socket_events &event,
                             Listener_interface::Sync_variable_state &state)
      : m_event(event), m_state(state) {}

  // Handler_interface
  void pre_loop() {
    m_state.set(State_listener_running);

    std::for_each(m_listeners.begin(), m_listeners.end(),
                  &Server_task_time_and_event::set_listeners_state<
                      State_listener_running>);
  }

  void post_loop() {
    m_state.set(State_listener_stopped);

    std::for_each(m_listeners.begin(), m_listeners.end(),
                  &Server_task_time_and_event::set_listeners_state<
                      State_listener_stopped>);
  }

  void loop() { m_event.loop(); }

  // Handle_time_and_event
  void listener_register(Listener_interface *listener) {
    m_listeners.push_back(listener);
  }

 private:
  template <State_listener state>
  static void set_listeners_state(Listener_interface *listener) {
    listener->get_state().set(state);
  }

  Socket_events &m_event;
  Listener_interface::Sync_variable_state &m_state;
  Server_acceptors::Listener_interfaces m_listeners;
};

Server_acceptors::Server_acceptors(Listener_factory_interface &listener_factory,
                                   const std::string &tcp_bind_address,
                                   const uint16 tcp_port,
                                   const uint32 tcp_port_open_timeout,
                                   const std::string &unix_socket_file,
                                   const uint32 backlog)
    : m_bind_address(tcp_bind_address),
      m_tcp_socket(listener_factory.create_tcp_socket_listener(
          m_bind_address, tcp_port, tcp_port_open_timeout, m_event, backlog)),
#if defined(HAVE_SYS_UN_H)
      m_unix_socket(listener_factory.create_unix_socket_listener(
          unix_socket_file, m_event, backlog)),
#endif
      m_time_and_event_state(State_listener_initializing),
      m_time_and_event_task(ngs::allocate_shared<Server_task_time_and_event>(
          ngs::ref(m_event), ngs::ref(m_time_and_event_state))),
      m_prepared(false) {
}

bool Server_acceptors::prepare_impl(On_connection on_connection,
                                    const bool skip_networking,
                                    const bool use_unix_sockets) {
  if (skip_networking) m_tcp_socket.reset();

  if (!use_unix_sockets) m_unix_socket.reset();

  Listener_interfaces listeners = get_array_of_listeners();

  if (listeners.empty()) {
    log_warning(ER_XPLUGIN_ALL_IO_INTERFACES_DISABLED);

    return false;
  }

  const size_t number_of_prepared_listeners =
      std::count_if(listeners.begin(), listeners.end(),
                    ngs::bind(&Listener_interface::setup_listener,
                              ngs::placeholders::_1, on_connection));

  if (0 == number_of_prepared_listeners) {
    abort();

    log_error(ER_XPLUGIN_FAILED_TO_PREPARE_IO_INTERFACES);

    return false;
  }

  return true;
}

bool Server_acceptors::prepare(On_connection on_connection,
                               const bool skip_networking,
                               const bool use_unix_sockets) {
  const bool result =
      prepare_impl(on_connection, skip_networking, use_unix_sockets);

  Listener_interfaces listeners = get_array_of_listeners();

  if (!listeners.empty() &&
      std::all_of(listeners.begin(), listeners.end(),
                  Server_acceptors::check_listener_status)) {
    std::string combined_status;
    std::size_t pos = listeners.size();
    while (pos > 1) {
      combined_status += listeners[pos - 1]->get_name_and_configuration() + " ";
      pos--;
    }
    combined_status += listeners[0]->get_name_and_configuration();

    auto first_non_blank_pos = combined_status.find_first_not_of("\t ");
    combined_status[first_non_blank_pos] =
        ::toupper(combined_status[first_non_blank_pos]);
    log_system(ER_XPLUGIN_LISTENER_STATUS_MSG, combined_status.c_str());
  }

  m_prepared = true;

  return result;
}

void Server_acceptors::abort() {
  Listener_interfaces listeners = get_array_of_listeners();

  std::for_each(listeners.begin(), listeners.end(),
                &Server_acceptors::close_listener);

  m_time_and_event_state.set(State_listener_stopped);

  std::for_each(listeners.begin(), listeners.end(),
                Server_acceptors::mark_as_stopped);
}

void Server_acceptors::stop(const bool is_called_from_timeout_handler) {
  Listener_interfaces listeners = get_array_of_listeners();

  m_event.break_loop();

  std::for_each(listeners.begin(), listeners.end(),
                &Server_acceptors::close_listener);

  if (!is_called_from_timeout_handler)
    m_time_and_event_state.wait_for(State_listener_stopped);

  std::for_each(listeners.begin(), listeners.end(),
                &Server_acceptors::wait_until_stopped);
}

bool Server_acceptors::is_listener_configured(Listener_interface *listener) {
  if (NULL == listener) return false;

  const State_listener allowed_states[] = {State_listener_prepared,
                                           State_listener_running};

  return listener->get_state().is(allowed_states);
}

bool Server_acceptors::was_unix_socket_configured() {
  return is_listener_configured(m_unix_socket.get());
}

bool Server_acceptors::was_tcp_server_configured(std::string &bind_address) {
  if (is_listener_configured(m_tcp_socket.get())) {
    bind_address = m_bind_address;
    return true;
  }

  return false;
}

bool Server_acceptors::was_prepared() const { return m_prepared; }

void Server_acceptors::add_timer(const std::size_t delay_ms,
                                 ngs::function<bool()> callback) {
  m_event.add_timer(delay_ms, callback);
}

Server_tasks_interfaces Server_acceptors::create_server_tasks_for_listeners() {
  Listener_interfaces listeners = get_array_of_listeners();
  Server_tasks_interfaces handlers;

  handlers.push_back(m_time_and_event_task);

  for (Listener_interfaces::iterator i = listeners.begin();
       listeners.end() != i; ++i) {
    Listener_interface *listener = (*i);

    if (!listener->get_state().is(State_listener_prepared)) continue;

    if (listener->is_handled_by_socket_event()) {
      m_time_and_event_task->listener_register(listener);
      continue;
    }

    ngs::shared_ptr<Server_task_interface> handler(
        ngs::allocate_shared<details::Server_task_listener>(
            ngs::ref(*listener)));
    handlers.push_back(handler);
  }

  return handlers;
}

Server_acceptors::Listener_interfaces
Server_acceptors::get_array_of_listeners() {
  Listener_interfaces result;

  if (m_tcp_socket) result.push_back(m_tcp_socket.get());

  if (m_unix_socket) result.push_back(m_unix_socket.get());

  return result;
}

void Server_acceptors::wait_until_stopped(Listener_interface *listener) {
  if (listener->is_handled_by_socket_event()) return;

  listener->get_state().wait_for(State_listener_stopped);
}

void Server_acceptors::close_listener(Listener_interface *listener) {
  listener->close_listener();
}

void Server_acceptors::mark_as_stopped(Listener_interface *listener) {
  listener->get_state().set(State_listener_stopped);
}

bool Server_acceptors::check_listener_status(Listener_interface *listener) {
  if (!listener->get_state().is(State_listener_prepared)) {
    log_error(ER_XPLUGIN_LISTENER_SETUP_FAILED,
              listener->get_name_and_configuration().c_str(),
              listener->get_last_error().c_str());

    std::string listener_configuration_variable =
        ngs::join(listener->get_configuration_variables(), "','");

    if (!listener_configuration_variable.empty()) {
      log_info(ER_XPLUGIN_LISTENER_SYS_VARIABLE_ERROR,
               listener_configuration_variable.c_str());
    }

    return false;
  }
  return true;
}

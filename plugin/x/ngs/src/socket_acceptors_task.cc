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

#include "plugin/x/ngs/include/ngs/socket_acceptors_task.h"

#include "my_config.h"

#include <stdlib.h>
#include <algorithm>
#include <iterator>

#include "plugin/x/ngs/include/ngs/log.h"
#include "plugin/x/ngs/include/ngs_common/string_formatter.h"

namespace ngs {

Socket_acceptors_task::Socket_acceptors_task(
    Listener_factory_interface &listener_factory,
    const std::string &tcp_bind_address, const uint16 tcp_port,
    const uint32 tcp_port_open_timeout, const std::string &unix_socket_file,
    const uint32 backlog, const std::shared_ptr<Socket_events_interface> &event)
    : m_event(event),
      m_bind_address(tcp_bind_address),
      m_tcp_socket(listener_factory.create_tcp_socket_listener(
          m_bind_address, tcp_port, tcp_port_open_timeout, *m_event, backlog)),
#if defined(HAVE_SYS_UN_H)
      m_unix_socket(listener_factory.create_unix_socket_listener(
          unix_socket_file, *m_event, backlog)),
#endif
      m_time_and_event_state(State_listener_initializing) {
}

bool Socket_acceptors_task::prepare_impl(
    Server_task_interface::Task_context *context) {
  if (context->m_skip_networking) {
    m_tcp_socket->close_listener();
    m_tcp_socket->report_properties(
        [context](const Server_property_ids id, const std::string &value) {
          (*context->m_properties)[id] = value;
        });

    m_tcp_socket.reset();
  }

  Listener_interfaces listeners = get_array_of_listeners();

  if (listeners.empty()) {
    log_warning(ER_XPLUGIN_ALL_IO_INTERFACES_DISABLED);

    return false;
  }

  const size_t number_of_prepared_listeners =
      std::count_if(listeners.begin(), listeners.end(),
                    [context](Listener_interface *l) -> bool {
                      return l->setup_listener(context->m_on_connection);
                    });

  if (0 == number_of_prepared_listeners) {
    stop(Stop_cause::k_server_task_triggered_event);
    log_error(ER_XPLUGIN_FAILED_TO_PREPARE_IO_INTERFACES);

    return false;
  }

  return true;
}

bool Socket_acceptors_task::prepare(
    Server_task_interface::Task_context *context) {
  const bool result = prepare_impl(context);
  Listener_interfaces listeners = get_array_of_listeners();
  Server_properties properties;

  for (auto &l : listeners) {
    log_listener_state(l);

    l->report_properties(
        [&properties](const Server_property_ids id, const std::string &value) {
          properties[id] = value;
        });
  }
  properties[ngs::Server_property_ids::k_number_of_interfaces] =
      std::to_string(listeners.size());

  context->m_properties->swap(properties);

  return result;
}

void Socket_acceptors_task::stop(const Stop_cause cause) {
  Listener_interfaces listeners = get_array_of_listeners();

  m_event->break_loop();

  for (auto &listener : listeners) listener->close_listener();

  switch (cause) {
    case Stop_cause::k_abort:
      m_time_and_event_state.set(State_listener_stopped);
      break;

    case Stop_cause::k_normal_shutdown:
      m_time_and_event_state.wait_for(State_listener_stopped);
      break;

    case Stop_cause::k_server_task_triggered_event:
      break;
  }
}

Socket_acceptors_task::Listener_interfaces
Socket_acceptors_task::get_array_of_listeners() {
  Listener_interfaces result;

  if (m_tcp_socket) result.push_back(m_tcp_socket.get());

  if (m_unix_socket) result.push_back(m_unix_socket.get());

  return result;
}

void Socket_acceptors_task::log_listener_state(Listener_interface *listener) {
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

    return;
  }

  log_info(ER_XPLUGIN_LISTENER_STATUS_MSG,
           listener->get_name_and_configuration().c_str());
}

void Socket_acceptors_task::pre_loop() {
  m_time_and_event_state.set(State_listener_running);
  auto listeners = get_array_of_listeners();

  for (auto &listener : listeners)
    listener->get_state().set(State_listener_running);
}

void Socket_acceptors_task::post_loop() {
  auto listeners = get_array_of_listeners();

  m_time_and_event_state.set(State_listener_stopped);

  for (auto &l : listeners) l->get_state().set(State_listener_stopped);
}

void Socket_acceptors_task::loop() { m_event->loop(); }

}  // namespace ngs

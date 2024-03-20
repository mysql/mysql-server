/*
 * Copyright (c) 2016, 2024, Oracle and/or its affiliates.
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

#include "plugin/x/src/ngs/socket_acceptors_task.h"

#include <algorithm>
#include <cstdlib>
#include <iterator>
#include <set>

#include "my_config.h"  // NOLINT(build/include_subdir)

#include "plugin/x/src/helper/multithread/xsync_point.h"
#include "plugin/x/src/helper/string_formatter.h"
#include "plugin/x/src/module_mysqlx.h"
#include "plugin/x/src/ngs/log.h"
#include "plugin/x/src/variables/system_variables.h"
#include "plugin/x/src/xpl_performance_schema.h"

extern bool check_address_is_wildcard(const char *address_value,
                                      size_t address_length);

namespace ngs {

Socket_acceptors_task::Socket_acceptors_task(
    const xpl::iface::Listener_factory &listener_factory,
    const std::string &multi_bind_address, const uint16_t tcp_port,
    const uint32_t tcp_port_open_timeout, const std::string &unix_socket_file,
    const uint32_t backlog,
    const std::shared_ptr<xpl::iface::Socket_events> &event)
    : m_listener_factory(listener_factory),
      m_event(event),
      m_multi_bind_address(multi_bind_address),
      m_tcp_port(tcp_port),
      m_tcp_port_open_timeout(tcp_port_open_timeout),
      m_unix_socket_file(unix_socket_file),
      m_backlog(backlog),
      m_time_and_event_state(xpl::iface::Listener::State::k_initializing,
                             KEY_mutex_x_socket_acceptors_sync,
                             KEY_cond_x_socket_acceptors_sync) {}

namespace {

bool parse_bind_address_value(const char *begin_address_value,
                              std::string *address_value,
                              std::string *network_namespace) {
  const char *namespace_separator = strchr(begin_address_value, '/');

  if (namespace_separator != nullptr) {
    if (begin_address_value == namespace_separator)
      /*
        Parse error: there is no character before '/',
        that is missed address value
      */
      return true;

    if (*(namespace_separator + 1) == 0)
      /*
        Parse error: there is no character immediately after '/',
        that is missed namespace name.
      */
      return true;

    /*
      Found namespace delimiter. Extract namespace and address values
    */
    *address_value = std::string(begin_address_value, namespace_separator);
    *network_namespace = std::string(namespace_separator + 1);
  } else {
    *address_value = begin_address_value;
  }
  return false;
}

void trim(std::string *value) {
  static const char *k_whitespace = " \n\r\t\f\v";
  const std::size_t first = value->find_first_not_of(k_whitespace);
  if (first == std::string::npos) {
    *value = "";
    return;
  }
  const std::size_t last = value->find_last_not_of(k_whitespace);
  if (last == std::string::npos) {
    *value = value->substr(first);
    return;
  }
  *value = value->substr(first, last - first + 1);
}

void split(const std::string &value, char delim,
           std::vector<std::string> *result) {
  std::stringstream ss(value);
  std::string item;
  while (std::getline(ss, item, delim)) result->push_back(item);
}

bool is_address_valid(const std::string &address, const bool is_multi_address,
                      std::string *host, std::string *net_namespace) {
  if (parse_bind_address_value(address.c_str(), host, net_namespace)) {
    log_error(ER_XPLUGIN_FAILED_TO_VALIDATE_ADDRESS, address.c_str(),
              "can't be parsed as an address");
    return false;
  }
  const bool is_wildcard =
      check_address_is_wildcard(host->c_str(), host->length());

  if (!net_namespace->empty() && is_wildcard) {
    log_error(ER_XPLUGIN_FAILED_TO_VALIDATE_ADDRESS, address.c_str(),
              "network namespace are not allowed for wildcards");
    return false;
  }

  if (is_wildcard && is_multi_address) {
    log_error(ER_XPLUGIN_FAILED_TO_VALIDATE_ADDRESS, address.c_str(),
              "wildcards are not allowed when there are more than one address");
    return false;
  }

  return true;
}
}  // namespace

void Socket_acceptors_task::prepare_listeners() {
  const bool skip_networking =
      xpl::Plugin_system_variables::get_system_variable("skip_networking") ==
      "ON";

  if (!skip_networking) {
    std::vector<std::string> addresses;
    split(m_multi_bind_address, ',', &addresses);

    const bool is_multi_address = addresses.size() > 1;
    for (auto &address : addresses) {
      trim(&address);
      std::string host, net_namespace;
      if (is_address_valid(address, is_multi_address, &host, &net_namespace))
        m_tcp_socket.push_back(m_listener_factory.create_tcp_socket_listener(
            host, net_namespace, m_tcp_port, m_tcp_port_open_timeout,
            m_event.get(), m_backlog));
    }
  }

  if (xpl::Plugin_system_variables::get_system_variable("socket") ==
      m_unix_socket_file) {
    log_warning(ER_INVALID_XPLUGIN_SOCKET_SAME_AS_SERVER);
  } else {
#if defined(HAVE_SYS_UN_H)
    m_unix_socket = m_listener_factory.create_unix_socket_listener(
        m_unix_socket_file, m_event.get(), m_backlog);
#endif
  }
}

bool Socket_acceptors_task::prepare_impl(
    xpl::iface::Server_task::Task_context *context) {
  Listener_interfaces listeners = get_array_of_listeners();

  if (listeners.empty()) {
    log_warning(ER_XPLUGIN_ALL_IO_INTERFACES_DISABLED);

    return false;
  }

  const size_t number_of_prepared_listeners =
      std::count_if(listeners.begin(), listeners.end(),
                    [context](xpl::iface::Listener *l) -> bool {
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
    xpl::iface::Server_task::Task_context *context) {
  prepare_listeners();

  const bool result = prepare_impl(context);
  Listener_interfaces listeners = get_array_of_listeners();
  Server_properties properties;
  std::set<std::string> configuration_variables;

  for (auto &l : listeners) {
    if (!l->report_status())
      configuration_variables.insert(l->get_configuration_variable());

    l->report_properties(
        [&properties](const Server_property_ids id, const std::string &value) {
          auto &p = properties[id];
          if (id == Server_property_ids::k_tcp_bind_address) {
            if (p.empty()) {
              p = value;
              return;
            }
            if (value == ngs::PROPERTY_NOT_CONFIGURED) return;
            if (p == ngs::PROPERTY_NOT_CONFIGURED) {
              p = value;
              return;
            }
            p += "," + value;
            return;
          }
          p = value;
        });
  }

  for (const auto &var : configuration_variables)
    log_info(ER_XPLUGIN_LISTENER_SYS_VARIABLE_ERROR, var.c_str());

  properties[Server_property_ids::k_number_of_interfaces] =
      std::to_string(listeners.size());

  // Make a local copy, to show the report at X Plugin start.
  m_properties = properties;

  if (context && context->m_properties) context->m_properties->swap(properties);
  return result;
}

void Socket_acceptors_task::stop(const Stop_cause cause) {
  Listener_interfaces listeners = get_array_of_listeners();

  m_event->break_loop();

  XSYNC_POINT_CHECK("xacceptor_stop_wait", "xacceptor_pre_loop_wait");

  switch (cause) {
    case Stop_cause::k_abort:
      m_time_and_event_state.set(xpl::iface::Listener::State::k_stopped);
      break;

    case Stop_cause::k_normal_shutdown:
      m_time_and_event_state.wait_for(xpl::iface::Listener::State::k_stopped);
      break;

    case Stop_cause::k_server_task_triggered_event:
      break;
  }
  XSYNC_POINT_CHECK(nullptr, "xacceptor_post_loop_wait");
}

void Socket_acceptors_task::show_startup_log(
    const Server_properties &properties) const {
  std::string combined_status;
  Server_properties::const_iterator i =
      properties.find(Server_property_ids::k_tcp_bind_address);
  if (i != properties.end()) {
    const auto &bind_address = i->second;
    if (!bind_address.empty() && bind_address != ngs::PROPERTY_NOT_CONFIGURED)
      combined_status += "Bind-address: '" + bind_address +
                         "' port: " + std::to_string(m_tcp_port);
  }
  i = properties.find(Server_property_ids::k_unix_socket);
  if (i != properties.end()) {
    const auto &unix_socket = i->second;
    if (!unix_socket.empty() && unix_socket != ngs::PROPERTY_NOT_CONFIGURED)
      combined_status +=
          (combined_status.empty() ? "Socket: " : ", socket: ") + unix_socket;
  }
  log_system(ER_XPLUGIN_LISTENER_STATUS_MSG, combined_status.c_str());
}

Socket_acceptors_task::Listener_interfaces
Socket_acceptors_task::get_array_of_listeners() {
  Listener_interfaces result;

  for (auto &s : m_tcp_socket)
    if (s) result.push_back(s.get());

  if (m_unix_socket) result.push_back(m_unix_socket.get());

  return result;
}

void Socket_acceptors_task::pre_loop() {
  log_debug("Socket_acceptors_task::pre_loop");

  m_time_and_event_state.set(xpl::iface::Listener::State::k_running);
  auto listeners = get_array_of_listeners();

  XSYNC_POINT_CHECK("xacceptor_pre_loop_wait");

  for (auto &listener : listeners) {
    listener->pre_loop();
  }

  show_startup_log(m_properties);
}

void Socket_acceptors_task::post_loop() {
  log_debug("Socket_acceptors_task::post_loop");
  auto listeners = get_array_of_listeners();

  m_time_and_event_state.set(xpl::iface::Listener::State::k_stopped);

  for (auto &listener : listeners) listener->close_listener();

  XSYNC_POINT_CHECK("xacceptor_post_loop_wait", "xacceptor_stop_wait");
}

void Socket_acceptors_task::loop() { m_event->loop(); }

}  // namespace ngs

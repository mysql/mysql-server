/*
 * Copyright (c) 2016, 2019, Oracle and/or its affiliates. All rights reserved.
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

#include "plugin/x/src/io/xpl_listener_tcp.h"

#include <errno.h>
#ifndef _WIN32
#include <netdb.h>
#endif

#include <memory>

#include "my_io.h"
#include "plugin/x/generated/mysqlx_version.h"
#include "plugin/x/src/helper/string_formatter.h"
#include "plugin/x/src/operations_factory.h"
#include "plugin/x/src/xpl_log.h"
#include "plugin/x/src/xpl_performance_schema.h"

#ifdef HAVE_SETNS
#include "sql/net_ns.h"
#endif

namespace xpl {

const char *BIND_ALL_ADDRESSES = "*";
const char *BIND_IPv4_ADDRESS = "0.0.0.0";
const char *BIND_IPv6_ADDRESS = "::";

class Tcp_creator {
 public:
  Tcp_creator(ngs::Operations_factory_interface &factory)
      : m_factory(factory),
        m_system_interface(m_factory.create_system_interface()) {}

  std::shared_ptr<addrinfo> resolve_bind_address(
      const std::string &bind_address, const unsigned short port,
      std::string &error_message) {
    struct addrinfo *result = nullptr;
    std::string service;
    std::vector<std::string> bind_addresses;
    String_formatter formatter;
    service = formatter.append(port).get_result();

    bind_addresses.push_back(bind_address);

    if (BIND_ALL_ADDRESSES == bind_address) {
      bind_addresses.clear();
      bind_addresses.push_back(BIND_IPv4_ADDRESS);

      if (is_ipv6_avaiable()) {
        log_info(ER_XPLUGIN_IPv6_AVAILABLE);
        bind_addresses.push_back(BIND_IPv6_ADDRESS);
      }
    }

    while (!bind_addresses.empty() && nullptr == result) {
      result = resolve_addr_info(bind_addresses.back(), service);

      bind_addresses.pop_back();
    }

    if (nullptr == result) {
      error_message = "can't resolve `hostname`";

      return std::shared_ptr<addrinfo>();
    }

    return std::shared_ptr<addrinfo>(
        result, std::bind(&ngs::System_interface::freeaddrinfo,
                          m_system_interface, std::placeholders::_1));
  }

  ngs::Socket_interface::Shared_ptr create_and_bind_socket(
      std::shared_ptr<addrinfo> ai, const uint32 backlog, int &error_code,
      std::string &error_message) {
    addrinfo *used_ai = nullptr;
    std::string errstr;

    ngs::Socket_interface::Shared_ptr result_socket =
        create_socket_from_addrinfo(ai.get(), KEY_socket_x_tcpip, AF_INET,
                                    &used_ai);

    if (nullptr == result_socket.get())
      result_socket = create_socket_from_addrinfo(ai.get(), KEY_socket_x_tcpip,
                                                  AF_INET6, &used_ai);

    if (nullptr == result_socket.get()) {
      m_system_interface->get_socket_error_and_message(error_code, errstr);

      error_message = String_formatter()
                          .append("`socket()` failed with error: ")
                          .append(errstr)
                          .append("(")
                          .append(error_code)
                          .append(")")
                          .get_result();

      return ngs::Socket_interface::Shared_ptr();
    }

#ifdef IPV6_V6ONLY
    /*
      For interoperability with older clients, IPv6 socket should
      listen on both IPv6 and IPv4 wildcard addresses.
      Turn off IPV6_V6ONLY option.

      NOTE: this will work starting from Windows Vista only.
      On Windows XP dual stack is not available, so it will not
      listen on the corresponding IPv4-address.
    */
    if (used_ai->ai_family == AF_INET6) {
      int option_flag = 0;

      if (result_socket->set_socket_opt(IPPROTO_IPV6, IPV6_V6ONLY,
                                        (char *)&option_flag,
                                        sizeof(option_flag))) {
        log_error(ER_XPLUGIN_FAILED_TO_RESET_IPV6_V6ONLY_FLAG,
                  (int)socket_errno);
      }
    }
#endif

    error_code = 0;

    {
      int one = 1;
      if (result_socket->set_socket_opt(SOL_SOCKET, SO_REUSEADDR,
                                        (const char *)&one, sizeof(one))) {
        log_error(ER_XPLUGIN_FAILED_TO_SET_SO_REUSEADDR_FLAG,
                  (int)m_system_interface->get_socket_errno());
      }
    }

    result_socket->set_socket_thread_owner();

    if (result_socket->bind((const struct sockaddr *)used_ai->ai_addr,
                            static_cast<socklen_t>(used_ai->ai_addrlen)) < 0) {
      // lets decide later if its an error or not
      m_system_interface->get_socket_error_and_message(error_code, errstr);

      error_message = String_formatter()
                          .append("`bind()` failed with error: ")
                          .append(errstr)
                          .append(" (")
                          .append(error_code)
                          .append(
                              "). Do you already have another mysqld server "
                              "running with Mysqlx ?")
                          .get_result();

      return ngs::Socket_interface::Shared_ptr();
    }

    if (result_socket->listen(backlog) < 0) {
      // lets decide later if its an error or not
      m_system_interface->get_socket_error_and_message(error_code, errstr);

      error_message = String_formatter()
                          .append("`listen()` failed with error: ")
                          .append(errstr)
                          .append("(")
                          .append(error_code)
                          .append(")")
                          .get_result();

      return ngs::Socket_interface::Shared_ptr();
    }

    m_used_address.resize(200, '\0');

    if (vio_getnameinfo((const struct sockaddr *)used_ai->ai_addr,
                        &m_used_address[0], m_used_address.length(), nullptr, 0,
                        NI_NUMERICHOST)) {
      m_used_address[0] = '\0';
    }

    m_used_address.resize(strlen(m_used_address.c_str()));

    return result_socket;
  }
  std::string get_used_address() { return m_used_address; }

 private:
  ngs::Socket_interface::Shared_ptr create_socket_from_addrinfo(
      addrinfo *ai, PSI_socket_key psi_key, const int family,
      addrinfo **used_ai) {
    for (addrinfo *cur_ai = ai; nullptr != cur_ai; cur_ai = cur_ai->ai_next) {
      if (family != cur_ai->ai_family) continue;

      ngs::Socket_interface::Shared_ptr result =
          m_factory.create_socket(psi_key, family, SOCK_STREAM, 0);

      if (INVALID_SOCKET != result->get_socket_fd()) {
        *used_ai = cur_ai;
        return result;
      }
    }

    return ngs::Socket_interface::Shared_ptr();
  }

  bool is_ipv6_avaiable() {
    ngs::Socket_interface::Shared_ptr socket(m_factory.create_socket(
        KEY_socket_x_diagnostics, AF_INET6, SOCK_STREAM, 0));
    const bool has_ipv6 = INVALID_SOCKET != socket->get_socket_fd();

    return has_ipv6;
  }

  struct addrinfo *resolve_addr_info(const std::string &address,
                                     const std::string service) {
    struct addrinfo hints;
    struct addrinfo *ai = nullptr;
    memset(&hints, 0, sizeof(hints));
    hints.ai_flags = AI_PASSIVE;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_family = AF_UNSPEC;

    if (!m_system_interface->getaddrinfo(address.c_str(), service.c_str(),
                                         &hints, &ai)) {
      return ai;
    }

    return nullptr;
  }

  std::string m_used_address;
  ngs::Operations_factory_interface &m_factory;
  ngs::System_interface::Shared_ptr m_system_interface;
};

Listener_tcp::Listener_tcp(Factory_ptr operations_factory,
                           std::string &bind_address,
                           const std::string &network_namespace,
                           const uint16 port, const uint32 port_open_timeout,
                           ngs::Socket_events_interface &event,
                           const uint32 backlog)
    : m_operations_factory(operations_factory),
      m_state(ngs::State_listener_initializing, KEY_mutex_x_listener_tcp_sync,
              KEY_cond_x_listener_tcp_sync),
      m_bind_address(bind_address),
      m_network_namespace(network_namespace),
      m_port(port),
      m_port_open_timeout(port_open_timeout),
      m_backlog(backlog),
      m_event(event) {}

Listener_tcp::~Listener_tcp() {
  // close_listener() can be called multiple times
  close_listener();
}

Listener_tcp::Sync_variable_state &Listener_tcp::get_state() { return m_state; }

std::string Listener_tcp::get_last_error() { return m_last_error; }

std::string Listener_tcp::get_name_and_configuration() const {
  return String_formatter()
      .append("bind-address: '")
      .append(m_bind_address)
      .append("' ")
      .append("port: ")
      .append(m_port)
      .get_result();
}

std::vector<std::string> Listener_tcp::get_configuration_variables() const {
  std::vector<std::string> result;

  result.push_back(MYSQLX_SYSTEM_VARIABLE_PREFIX("port"));
  result.push_back(MYSQLX_SYSTEM_VARIABLE_PREFIX("bind_address"));

  return result;
}

bool Listener_tcp::setup_listener(On_connection on_connection) {
  if (!m_state.is(ngs::State_listener_initializing)) return false;

  m_tcp_socket = create_socket();

  // create_socket in case of invalid socket or setup failure
  // is going to return nullptr
  if (nullptr == m_tcp_socket.get()) {
    close_listener();
    return false;
  }

  if (m_event.listen(m_tcp_socket, on_connection)) {
    m_state.set(ngs::State_listener_prepared);
    return true;
  }

  m_last_error = "event dispatcher couldn't register socket";
  m_tcp_socket.reset();
  close_listener();

  return false;
}

void Listener_tcp::close_listener() {
  // ngs::Socket_interface::close can be called multiple times
  // it invalidates the content of m_mysql_socket thus at next call
  // it does nothing
  //
  // Same applies to close_listener()
  m_state.set(ngs::State_listener_stopped);

  if (m_tcp_socket) m_tcp_socket->close();
}

void Listener_tcp::loop() {}

ngs::Socket_interface::Shared_ptr Listener_tcp::create_socket() {
  Tcp_creator creator(*m_operations_factory);
  int error_code;

  ngs::Socket_interface::Shared_ptr result_socket;
  ngs::System_interface::Shared_ptr system_interface(
      m_operations_factory->create_system_interface());

  log_debug("TCP Sockets address is '%s' and port is %i",
            m_bind_address.c_str(), (int)m_port);
  if (!m_network_namespace.empty()) {
#ifdef HAVE_SETNS
    if (set_network_namespace(m_network_namespace)) return nullptr;
#else
    log_error(ER_NETWORK_NAMESPACES_NOT_SUPPORTED);
    return nullptr;
#endif
  }
  std::shared_ptr<addrinfo> ai =
      creator.resolve_bind_address(m_bind_address, m_port, m_last_error);

  if (nullptr == ai.get()) return ngs::Socket_interface::Shared_ptr();

  for (uint32 waited = 0, retry = 1; waited <= m_port_open_timeout; ++retry) {
    result_socket =
        creator.create_and_bind_socket(ai, m_backlog, error_code, m_last_error);

    // Success, lets break the loop
    // `create_and_bind_socket` in case of invalid socket/failure
    //  returns empty pointer
    if (nullptr != result_socket.get()) {
      m_bind_address = creator.get_used_address();
      break;
    }

    // Critical failure, lets break the loop
    if (SOCKET_EADDRINUSE != system_interface->get_socket_errno()) break;

    log_info(ER_XPLUGIN_RETRYING_BIND_ON_PORT, (int)m_port);

    const int time_to_wait = retry * retry / 3 + 1;
    system_interface->sleep(time_to_wait);

    waited += time_to_wait;
  }
#ifdef HAVE_SETNS
  if (!m_network_namespace.empty() && restore_original_network_namespace())
    return nullptr;
#endif

  return result_socket;
}

std::string Listener_tcp::choose_property_value(
    const std::string &value) const {
  switch (m_state.get()) {
    case ngs::State_listener_prepared:
      return value;

    case ngs::State_listener_running:
      return value;

    case ngs::State_listener_stopped:
      return ngs::PROPERTY_NOT_CONFIGURED;

    default:
      return "";
  }
}

void Listener_tcp::report_properties(On_report_properties on_prop) {
  on_prop(ngs::Server_property_ids::k_tcp_bind_address,
          choose_property_value(m_bind_address));

  on_prop(ngs::Server_property_ids::k_tcp_port,
          choose_property_value(std::to_string(m_port)));
}

}  // namespace xpl

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

#include "plugin/x/src/io/xpl_listener_factory.h"

#include "plugin/x/src/io/xpl_listener_tcp.h"
#include "plugin/x/src/io/xpl_listener_unix_socket.h"
#include "plugin/x/src/operations_factory.h"

namespace xpl {

Listener_factory::Listener_factory() {
  m_operations_factory = std::make_shared<Operations_factory>();
}

std::unique_ptr<iface::Listener> Listener_factory::create_unix_socket_listener(
    const std::string &unix_socket_path, iface::Socket_events *event,
    const uint32_t backlog) const {
  return std::make_unique<Listener_unix_socket>(
      m_operations_factory, unix_socket_path, std::ref(*event), backlog);
}

std::unique_ptr<iface::Listener> Listener_factory::create_tcp_socket_listener(
    const std::string &bind_address, const std::string &network_namespace,
    const uint16_t port, const uint32_t port_open_timeout,
    iface::Socket_events *event, const uint32_t backlog) const {
  return std::make_unique<Listener_tcp>(
      m_operations_factory, bind_address, network_namespace, port,
      port_open_timeout, std::ref(*event), backlog);
}

}  // namespace xpl

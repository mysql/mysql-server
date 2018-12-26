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

#include "xpl_listener_factory.h"
#include "io/xpl_listener_tcp.h"
#include "io/xpl_listener_unix_socket.h"
#include "ngs_common/operations_factory.h"


namespace xpl {

Listener_factory::Listener_factory() {
  m_operations_factory = ngs::make_shared<ngs::Operations_factory>();
}

ngs::Listener_interface_ptr Listener_factory::create_unix_socket_listener(
    const std::string &unix_socket_path,
    ngs::Socket_events_interface &event,
    const uint32 backlog) {
  return ngs::Listener_interface_ptr(
      ngs::allocate_object<Listener_unix_socket>(
          m_operations_factory,
          unix_socket_path,
          ngs::ref(event),
          backlog));
}

ngs::Listener_interface_ptr Listener_factory::create_tcp_socket_listener(
    std::string &bind_address,
    const unsigned short port,
    const uint32 port_open_timeout,
    ngs::Socket_events_interface &event,
    const uint32 backlog) {
  return ngs::Listener_interface_ptr(
      ngs::allocate_object<Listener_tcp>(m_operations_factory,
                                         ngs::ref(bind_address),
                                         port,
                                         port_open_timeout,
                                         ngs::ref(event),
                                         backlog));
}

} // namespace xpl

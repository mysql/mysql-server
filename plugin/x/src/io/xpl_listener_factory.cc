/*
 * Copyright (c) 2016, 2017, Oracle and/or its affiliates. All rights reserved.
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

#include "plugin/x/src/io/xpl_listener_factory.h"

#include "plugin/x/ngs/include/ngs_common/operations_factory.h"
#include "plugin/x/src/io/xpl_listener_tcp.h"
#include "plugin/x/src/io/xpl_listener_unix_socket.h"

namespace xpl {

Listener_factory::Listener_factory() {
  m_operations_factory = ngs::make_shared<ngs::Operations_factory>();
}

ngs::Listener_interface_ptr Listener_factory::create_unix_socket_listener(
    const std::string &unix_socket_path, ngs::Socket_events_interface &event,
    const uint32 backlog) {
  return ngs::Listener_interface_ptr(ngs::allocate_object<Listener_unix_socket>(
      m_operations_factory, unix_socket_path, ngs::ref(event), backlog));
}

ngs::Listener_interface_ptr Listener_factory::create_tcp_socket_listener(
    std::string &bind_address, const unsigned short port,
    const uint32 port_open_timeout, ngs::Socket_events_interface &event,
    const uint32 backlog) {
  return ngs::Listener_interface_ptr(ngs::allocate_object<Listener_tcp>(
      m_operations_factory, ngs::ref(bind_address), port, port_open_timeout,
      ngs::ref(event), backlog));
}

}  // namespace xpl

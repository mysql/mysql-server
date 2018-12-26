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

#ifndef XPL_LISTENER_FACTORY_INTERFACE_H_
#define XPL_LISTENER_FACTORY_INTERFACE_H_

#include "ngs/interface/listener_factory_interface.h"
#include "ngs_common/operations_factory_interface.h"


namespace xpl {

class Listener_factory: public ngs::Listener_factory_interface {
public:
  Listener_factory();

  ngs::Listener_interface_ptr create_unix_socket_listener(
      const std::string &unix_socket_path,
      ngs::Socket_events_interface &event,
      const uint32 backlog);

  ngs::Listener_interface_ptr create_tcp_socket_listener(
      std::string &bind_address,
      const unsigned short port,
      const uint32 port_open_timeout,
      ngs::Socket_events_interface &event,
      const uint32 backlog);

private:
  ngs::Operations_factory_interface::Shared_ptr m_operations_factory;
};

} // namespace xpl

#endif // XPL_LISTENER_FACTORY_INTERFACE_H_

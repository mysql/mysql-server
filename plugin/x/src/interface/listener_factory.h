/*
 * Copyright (c) 2016, 2022, Oracle and/or its affiliates.
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

#ifndef PLUGIN_X_SRC_INTERFACE_LISTENER_FACTORY_H_
#define PLUGIN_X_SRC_INTERFACE_LISTENER_FACTORY_H_

#include <memory>
#include <string>

#include "plugin/x/src/interface/listener.h"
#include "plugin/x/src/interface/socket_events.h"
#include "plugin/x/src/ngs/memory.h"

namespace xpl {
namespace iface {

class Listener_factory {
 public:
  virtual ~Listener_factory() = default;

  virtual std::unique_ptr<Listener> create_unix_socket_listener(
      const std::string &unix_socket_path, Socket_events *event,
      const uint32_t backlog) const = 0;
  virtual std::unique_ptr<Listener> create_tcp_socket_listener(
      const std::string &bind_address, const std::string &network_namespace,
      const uint16_t port, const uint32_t port_open_timeout,
      Socket_events *event, const uint32_t backlog) const = 0;
};

}  // namespace iface
}  // namespace xpl

#endif  // PLUGIN_X_SRC_INTERFACE_LISTENER_FACTORY_H_

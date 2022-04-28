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

#ifndef PLUGIN_X_SRC_NGS_SOCKET_ACCEPTORS_TASK_H_
#define PLUGIN_X_SRC_NGS_SOCKET_ACCEPTORS_TASK_H_

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "plugin/x/src/interface/listener.h"
#include "plugin/x/src/interface/listener_factory.h"
#include "plugin/x/src/interface/server_task.h"
#include "plugin/x/src/interface/socket_events.h"
#include "plugin/x/src/server/server_properties.h"

namespace ngs {

class Socket_acceptors_task : public xpl::iface::Server_task {
 public:
  using On_connection = xpl::iface::Listener::On_connection;

 public:
  Socket_acceptors_task(
      const xpl::iface::Listener_factory &listener_factory,
      const std::string &multi_bind_address, const uint16_t tcp_port,
      const uint32_t tcp_port_open_timeout, const std::string &unix_socket_file,
      const uint32_t backlog,
      const std::shared_ptr<xpl::iface::Socket_events> &event);

  bool prepare(Task_context *context) override;
  void stop(const Stop_cause cause = Stop_cause::k_normal_shutdown) override;

 public:
  void pre_loop() override;
  void post_loop() override;
  void loop() override;

 private:
  using Listener_interfaces = std::vector<xpl::iface::Listener *>;
  class Server_task_time_and_event;

  bool prepare_impl(Task_context *context);
  void prepare_listeners();
  Listener_interfaces get_array_of_listeners();
  void show_startup_log(const Server_properties &properties) const;

  const xpl::iface::Listener_factory &m_listener_factory;
  std::shared_ptr<xpl::iface::Socket_events> m_event;
  std::string m_multi_bind_address;
  const uint16_t m_tcp_port;
  const uint32_t m_tcp_port_open_timeout;
  std::string m_unix_socket_file;
  std::vector<std::unique_ptr<xpl::iface::Listener>> m_tcp_socket;
  std::unique_ptr<xpl::iface::Listener> m_unix_socket;
  const uint32_t m_backlog;
  Server_properties m_properties;

  xpl::iface::Listener::Sync_variable_state m_time_and_event_state;
};

}  // namespace ngs

#endif  // PLUGIN_X_SRC_NGS_SOCKET_ACCEPTORS_TASK_H_

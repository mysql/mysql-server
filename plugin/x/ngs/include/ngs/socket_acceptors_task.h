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

#ifndef PLUGIN_X_NGS_INCLUDE_NGS_SERVER_ACCEPTORS_TASK_H_
#define PLUGIN_X_NGS_INCLUDE_NGS_SERVER_ACCEPTORS_TASK_H_

#include <string>
#include <vector>

#include "my_inttypes.h"
#include "plugin/x/ngs/include/ngs/interface/listener_factory_interface.h"
#include "plugin/x/ngs/include/ngs/interface/listener_interface.h"
#include "plugin/x/ngs/include/ngs/interface/server_task_interface.h"
#include "plugin/x/ngs/include/ngs/interface/socket_events_interface.h"
#include "plugin/x/ngs/include/ngs_common/smart_ptr.h"

namespace ngs {

class Socket_acceptors_task : public Server_task_interface {
 public:
  using On_connection = Listener_interface::On_connection;

 public:
  Socket_acceptors_task(Listener_factory_interface &listener_factory,
                        const std::string &tcp_bind_address,
                        const uint16 tcp_port,
                        const uint32 tcp_port_open_timeout,
                        const std::string &unix_socket_file,
                        const uint32 backlog,
                        const std::shared_ptr<Socket_events_interface> &event);

  bool prepare(Task_context *context) override;
  void stop(const Stop_cause cause = Stop_cause::k_normal_shutdown) override;

 public:
  void pre_loop() override;
  void post_loop() override;
  void loop() override;

 private:
  using Listener_interfaces = std::vector<Listener_interface *>;
  class Server_task_time_and_event;

  bool prepare_impl(Task_context *context);
  Listener_interfaces get_array_of_listeners();

  static bool is_listener_configured(Listener_interface *listener);
  static void log_listener_state(Listener_interface *listener);

  std::shared_ptr<Socket_events_interface> m_event;
  std::string m_bind_address;
  Listener_interface_ptr m_tcp_socket;
  Listener_interface_ptr m_unix_socket;

  Listener_interface::Sync_variable_state m_time_and_event_state;
};

}  // namespace ngs

#endif  // PLUGIN_X_NGS_INCLUDE_NGS_SERVER_ACCEPTORS_TASK_H_

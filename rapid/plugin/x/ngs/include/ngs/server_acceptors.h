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

#ifndef NGS_SERVER_ACCEPTORS_
#define NGS_SERVER_ACCEPTORS_

#include "ngs/interface/listener_interface.h"
#include "ngs/interface/listener_factory_interface.h"
#include "ngs/interface/server_task_interface.h"
#include <string>
#include <vector>
#include "ngs_common/smart_ptr.h"
#include "socket_events.h"


namespace ngs
{
typedef std::vector< ngs::shared_ptr<Server_task_interface> > Server_tasks_interfaces;

class Server_acceptors
{
public:
  typedef Listener_interface::On_connection On_connection;

  Server_acceptors(Listener_factory_interface &listener_factory,
                   const std::string &tcp_bind_address,
                   const uint16 tcp_port,
                   const uint32 tcp_port_open_timeout,
                   const std::string &unix_socket_file,
                   const uint32 backlog);

  bool prepare(On_connection on_connection,
               const bool skip_networking,
               const bool use_unix_sockets);
  void abort();
  void stop(const bool is_called_from_timeout_handler = false);

  bool was_unix_socket_configured();
  bool was_tcp_server_configured(std::string &bind_address);
  bool was_prepared() const;

  Server_tasks_interfaces create_server_tasks_for_listeners();
  void add_timer(const std::size_t delay_ms,
                 ngs::function<bool ()> callback);

private:
  typedef std::vector<Listener_interface *> Listener_interfaces;
  class Server_task_time_and_event;

  bool prepare_impl(On_connection on_connection, const bool skip_networking, const bool use_unix_sockets);
  Listener_interfaces get_array_of_listeners();

  static bool is_listener_configured(Listener_interface *listener);
  static void mark_as_stopped(Listener_interface *listener);
  static void wait_until_stopped(Listener_interface *listener);
  static void close_listener(Listener_interface *listener);
  static void report_listener_status(Listener_interface *listener);

  std::string m_bind_address;
  Listener_interface_ptr m_tcp_socket;
  Listener_interface_ptr m_unix_socket;

  Listener_interface::Sync_variable_state     m_time_and_event_state;
  ngs::shared_ptr<Server_task_time_and_event> m_time_and_event_task;
  Socket_events m_event;
  bool m_prepared;
};

}  // namespace ngs

#endif // NGS_SERVER_ACCEPTORS_

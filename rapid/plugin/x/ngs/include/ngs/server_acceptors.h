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

#ifndef NGS_SERVER_ACCEPTORS_
#define NGS_SERVER_ACCEPTORS_

#include <string>
#include <vector>

#include "my_inttypes.h"
#include "plugin/x/ngs/include/ngs/interface/listener_factory_interface.h"
#include "plugin/x/ngs/include/ngs/interface/listener_interface.h"
#include "plugin/x/ngs/include/ngs/interface/server_task_interface.h"
#include "plugin/x/ngs/include/ngs/socket_events.h"
#include "plugin/x/ngs/include/ngs_common/smart_ptr.h"


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

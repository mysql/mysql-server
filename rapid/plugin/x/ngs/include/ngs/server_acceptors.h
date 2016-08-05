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

#ifndef _NGS_SERVER_ACCEPTORS_
#define _NGS_SERVER_ACCEPTORS_

#include "ngs/interface/listener_interface.h"
#include "ngs/interface/listener_factory_interface.h"
#include "ngs/interface/server_task_interface.h"
#include "ngs/time_socket_events.h"
#include <string>
#include <vector>
#include <boost/scoped_ptr.hpp>
#include <boost/shared_ptr.hpp>


namespace ngs
{
typedef std::vector< boost::shared_ptr<Server_task_interface> > Server_tasks_interfaces;

class Server_acceptors
{
public:
  typedef Listener_interface::On_connection On_connection;

  Server_acceptors(Listener_factory_interface &listener_factory,
                   const unsigned short tcp_port,
                   const std::string &unix_socket_file_or_named_pipe,
                   const uint32 backlog);

  bool prepare(On_connection on_connection, const bool skip_networking, const bool use_unix_sockets_or_named_pipes);
  void abort();
  void stop(const bool is_called_from_timeout_handler = false);

  bool was_unix_socket_or_named_pipe_configured();

  Server_tasks_interfaces create_server_tasks_for_listeners();
  void add_timer(const std::size_t delay_ms, boost::function<bool ()> callback);

private:
  typedef std::vector<Listener_interface *> Listener_interfaces;
  class Server_task_time_and_event;

  bool prepare_impl(On_connection on_connection, const bool skip_networking, const bool use_unix_sockets_or_named_pipes);
  Listener_interfaces get_array_of_listeners();

  static void mark_as_stopped(Listener_interface *listener);
  static void wait_until_stopped(Listener_interface *listener);
  static void close_listener(Listener_interface *listener);
  static void report_listener_status(Listener_interface *listener);

  Listener_interface_ptr m_tcp_socket;
  Listener_interface_ptr m_unix_socket;

  Listener_interface::Sync_variable_state m_time_and_event_state;
  boost::shared_ptr<Server_task_time_and_event> m_time_and_event_task;
  Time_and_socket_events m_event;
};

}  // namespace ngs

#endif // _NGS_SERVER_ACCEPTORS_

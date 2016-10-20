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

#ifndef XPL_LISTENER_UNIX_SOCKET_H_
#define XPL_LISTENER_UNIX_SOCKET_H_

#include "ngs/socket_events_interface.h"
#include "ngs_common/socket_interface.h"
#include "ngs_common/operations_factory_interface.h"
#include "ngs/interface/listener_interface.h"


namespace xpl {

class Listener_unix_socket: public ngs::Listener_interface {
public:
  typedef ngs::Socket_interface::Shared_ptr Socket_ptr;

  Listener_unix_socket(ngs::Operations_factory_interface::Shared_ptr operations_factory,
                       const std::string &unix_socket_path,
                       ngs::Socket_events_interface &event,
                       const uint32 backlog);
  ~Listener_unix_socket();

  bool is_handled_by_socket_event();

  Sync_variable_state &get_state();
  std::string get_name_and_configuration() const;
  std::string get_last_error();
  std::vector<std::string> get_configuration_variables() const;

  bool setup_listener(On_connection on_connection);
  void close_listener();
  void loop();

private:
  ngs::Operations_factory_interface::Shared_ptr m_operations_factory;
  const std::string m_unix_socket_path;
  const uint32 m_backlog;

  std::string m_last_error;
  Sync_variable_state m_state;

  Socket_ptr m_unix_socket;
  ::ngs::Socket_events_interface &m_event;
};

} // namespace xpl

#endif // XPL_LISTENER_UNIX_SOCKET_H_

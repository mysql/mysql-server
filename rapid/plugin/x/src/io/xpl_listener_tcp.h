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

#ifndef XPL_LISTENER_TCP_H_
#define XPL_LISTENER_TCP_H_

#include "my_inttypes.h"
#include "plugin/x/ngs/include/ngs/interface/listener_interface.h"
#include "plugin/x/ngs/include/ngs/interface/listener_interface.h"
#include "plugin/x/ngs/include/ngs/interface/socket_events_interface.h"
#include "plugin/x/ngs/include/ngs_common/connection_vio.h"
#include "plugin/x/ngs/include/ngs_common/operations_factory_interface.h"
#include "plugin/x/ngs/include/ngs_common/socket_interface.h"


namespace xpl {

class Listener_tcp: public ngs::Listener_interface {
public:
  typedef ngs::Socket_interface                         Socket_interface;
  typedef ngs::Socket_interface::Shared_ptr             Socket_interface_ptr;
  typedef ngs::Operations_factory_interface::Shared_ptr Factory_ptr;

  Listener_tcp(Factory_ptr operations_factory,
               std::string &bind_address,
               const uint16 port,
               const uint32 port_open_timeout,
               ngs::Socket_events_interface &event,
               const uint32 backlog);
  ~Listener_tcp();

  bool is_handled_by_socket_event();

  Sync_variable_state &get_state();
  std::string get_last_error();
  std::string get_name_and_configuration() const;
  std::vector<std::string> get_configuration_variables() const;

  bool setup_listener(On_connection on_connection);
  void close_listener();
  void loop();

private:
  Socket_interface_ptr create_socket();

  Factory_ptr m_operations_factory;
  Sync_variable_state m_state;
  std::string &m_bind_address;
  const unsigned short m_port;
  const uint32 m_port_open_timeout;
  const uint32 m_backlog;
  Socket_interface_ptr m_tcp_socket;
  ngs::Socket_events_interface &m_event;
  std::string m_last_error;
};

} // namespace xpl

#endif // XPL_LISTENER_TCP_H_

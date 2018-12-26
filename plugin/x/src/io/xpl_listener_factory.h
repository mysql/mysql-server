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

#ifndef PLUGIN_X_SRC_IO_XPL_LISTENER_FACTORY_H_
#define PLUGIN_X_SRC_IO_XPL_LISTENER_FACTORY_H_

#include "my_inttypes.h"
#include "plugin/x/ngs/include/ngs/interface/listener_factory_interface.h"
#include "plugin/x/ngs/include/ngs_common/operations_factory_interface.h"

namespace xpl {

class Listener_factory : public ngs::Listener_factory_interface {
 public:
  Listener_factory();

  ngs::Listener_interface_ptr create_unix_socket_listener(
      const std::string &unix_socket_path, ngs::Socket_events_interface &event,
      const uint32 backlog);

  ngs::Listener_interface_ptr create_tcp_socket_listener(
      std::string &bind_address, const unsigned short port,
      const uint32 port_open_timeout, ngs::Socket_events_interface &event,
      const uint32 backlog);

 private:
  ngs::Operations_factory_interface::Shared_ptr m_operations_factory;
};

}  // namespace xpl

#endif  // PLUGIN_X_SRC_IO_XPL_LISTENER_FACTORY_H_

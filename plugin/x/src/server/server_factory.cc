/*
 * Copyright (c) 2019, 2022, Oracle and/or its affiliates.
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

#include "plugin/x/src/server/server_factory.h"

#include <memory>

#include "plugin/x/src/client.h"
#include "plugin/x/src/protocol_monitor.h"
#include "plugin/x/src/session.h"

namespace xpl {

Server_factory::Client_interface_ptr Server_factory::create_client(
    xpl::iface::Server *server, Vio_interface_ptr connection) {
  std::shared_ptr<xpl::iface::Client> result;

  result = ngs::allocate_shared<xpl::Client>(
      connection, server, ++m_client_id,
      ngs::allocate_object<xpl::Protocol_monitor>());

  return result;
}

xpl::Server_factory::Session_interface_ptr Server_factory::create_session(
    xpl::iface::Client *client, xpl::iface::Protocol_encoder *proto,
    const xpl::iface::Session::Session_id session_id) {
  return std::shared_ptr<iface::Session>(
      ngs::allocate_shared<xpl::Session>(client, proto, session_id));
}

}  // namespace xpl

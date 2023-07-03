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

#ifndef PLUGIN_X_SRC_SERVER_SERVER_FACTORY_H_
#define PLUGIN_X_SRC_SERVER_SERVER_FACTORY_H_

#include <memory>

#include "plugin/x/src/interface/client.h"
#include "plugin/x/src/interface/protocol_encoder.h"
#include "plugin/x/src/interface/server.h"
#include "plugin/x/src/interface/session.h"
#include "plugin/x/src/interface/vio.h"

namespace xpl {

class Server_factory {
 public:
  using Vio_interface_ptr = std::shared_ptr<iface::Vio>;
  using Client_interface_ptr = std::shared_ptr<iface::Client>;
  using Session_interface_ptr = std::shared_ptr<iface::Session>;

 public:
  Client_interface_ptr create_client(iface::Server *server,
                                     Vio_interface_ptr connection);
  Session_interface_ptr create_session(
      iface::Client *client, iface::Protocol_encoder *proto,
      const iface::Session::Session_id session_id);

 private:
  iface::Client::Client_id m_client_id = 0;
};

}  // namespace xpl

#endif  // PLUGIN_X_SRC_SERVER_SERVER_FACTORY_H_

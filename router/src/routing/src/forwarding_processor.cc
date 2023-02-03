/*
  Copyright (c) 2023, Oracle and/or its affiliates.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is also distributed with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have included with MySQL.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "forwarding_processor.h"

#include <memory>  // make_unique

#include "classic_connection_base.h"
#include "classic_forwarder.h"
#include "mysqlrouter/connection_pool_component.h"

stdx::expected<Processor::Result, std::error_code>
ForwardingProcessor::forward_server_to_client(bool noflush) {
  connection()->push_processor(
      std::make_unique<ServerToClientForwarder>(connection(), noflush));

  return Result::Again;
}

stdx::expected<Processor::Result, std::error_code>
ForwardingProcessor::forward_client_to_server(bool noflush) {
  connection()->push_processor(
      std::make_unique<ClientToServerForwarder>(connection(), noflush));

  return Result::Again;
}

/*
  Copyright (c) 2021, 2024, Oracle and/or its affiliates.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is designed to work with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have either included with
  the program or referenced in the documentation.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "classic_connection.h"

#include <memory>

#include "classic_flow.h"
#include "mysqlrouter/connection_pool.h"
#include "mysqlrouter/connection_pool_component.h"

void MysqlRoutingClassicConnection::async_run() {
  this->accepted();

  reset_to_initial();

  push_processor(std::make_unique<FlowProcessor>(this));

  call_next_function(Function::kLoop);
}

void MysqlRoutingClassicConnection::stash_server_conn() {
  auto &pool_comp = ConnectionPoolComponent::get_instance();
  auto pool = pool_comp.get(ConnectionPoolComponent::default_pool_name());

  if (pool && server_conn().is_open()) {
    if (auto &tr = tracer()) {
      tr.trace(Tracer::Event().stage(
          "pool::stashed: fd=" + std::to_string(server_conn().native_handle()) +
          ", " + server_conn().endpoint()));
    }

    auto ssl_mode = server_conn().ssl_mode();

    pool->stash(std::exchange(server_conn(),
                              TlsSwitchableConnection{
                                  nullptr, ssl_mode,
                                  ServerSideConnection::protocol_state_type{}}),
                this, context().connection_sharing_delay());
  }
}

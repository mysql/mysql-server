/*
  Copyright (c) 2020, 2023, Oracle and/or its affiliates.

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

#ifndef ROUTING_CLASSIC_CONNECTION_INCLUDED
#define ROUTING_CLASSIC_CONNECTION_INCLUDED

#include <functional>
#include <memory>

#include "classic_connection_base.h"
#include "processor.h"
#include "tracer.h"

class MysqlRoutingClassicConnection : public MysqlRoutingClassicConnectionBase {
 private:
  // constructor
  //
  // use ::create() instead.
  MysqlRoutingClassicConnection(
      MySQLRoutingContext &context, RouteDestination *route_destination,
      std::unique_ptr<ConnectionBase> client_connection,
      std::unique_ptr<RoutingConnectionBase> client_routing_connection,
      std::function<void(MySQLRoutingConnectionBase *)> remove_callback)
      : MysqlRoutingClassicConnectionBase{
            context, route_destination, std::move(client_connection),
            std::move(client_routing_connection), std::move(remove_callback)} {}

 public:
  // create a new shared_ptr<ThisClass>
  //
  template <typename... Args>
  [[nodiscard]] static std::shared_ptr<MysqlRoutingClassicConnectionBase>
  create(
      // clang-format off
      Args &&... args) {
    // clang-format on

    // can't use make_unique<> here as the constructor is private.
    return std::shared_ptr<MysqlRoutingClassicConnectionBase>(
        new MysqlRoutingClassicConnection(std::forward<Args>(args)...));
  }

  void async_run() override;
};

#endif

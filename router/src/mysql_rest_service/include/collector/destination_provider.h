/*
  Copyright (c) 2023, 2024, Oracle and/or its affiliates.

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

#ifndef ROUTER_SRC_MYSQL_REST_SERVICE_INCLUDE_COLLECTOR_DESTINATION_PROVIDER_H_
#define ROUTER_SRC_MYSQL_REST_SERVICE_INCLUDE_COLLECTOR_DESTINATION_PROVIDER_H_

#include <optional>
#include <vector>

#include "mrs/interface/ssl_configuration.h"
#include "tcp_address.h"

namespace collector {

class DestinationProvider {
 public:
  using Node = mysql_harness::TCPAddress;
  using SslConfiguration = mrs::SslConfiguration;

  enum WaitingOp { kNoWait, kWaitUntilAvaiable, kWaitUntilTimeout };

 public:
  virtual ~DestinationProvider() = default;

  virtual std::optional<Node> get_node(const WaitingOp) = 0;
  virtual bool is_node_supported(const Node &node) = 0;
  virtual const SslConfiguration &get_ssl_configuration() = 0;
};

}  // namespace collector

#endif  // ROUTER_SRC_MYSQL_REST_SERVICE_INCLUDE_COLLECTOR_DESTINATION_PROVIDER_H_

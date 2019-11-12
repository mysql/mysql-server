/*
  Copyright (c) 2015, 2019, Oracle and/or its affiliates. All rights reserved.

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

#ifndef PLUGIN_CONFIG_ROUTING_INCLUDED
#define PLUGIN_CONFIG_ROUTING_INCLUDED

#include <string>

#include "mysql/harness/config_option.h"
#include "mysql/harness/filesystem.h"  // Path
#include "mysqlrouter/routing.h"       // RoutingStrategy, AccessMode
#include "mysqlrouter/routing_export.h"
#include "protocol/protocol.h"  // Protocol::Type
#include "tcp_address.h"

class ROUTING_EXPORT RoutingPluginConfig {
 private:
  // is this [routing] entry for static routing or metadata-cache ?
  // it's mutable because we discover it while calling getter for
  // option destinations
  mutable bool metadata_cache_;

 public:
  /** @brief Constructor
   *
   * @param section from configuration file provided as ConfigSection
   */
  RoutingPluginConfig(const mysql_harness::ConfigSection *section);

  /** @brief `protocol` option read from configuration section */
  const Protocol::Type protocol;
  /** @brief `destinations` option read from configuration section */
  const std::string destinations;
  /** @brief `bind_port` option read from configuration section */
  const int bind_port;
  /** @brief `bind_address` option read from configuration section */
  const mysql_harness::TCPAddress bind_address;
  /** @brief `socket` option read from configuration section is stored as
   * named_socket */
  const mysql_harness::Path named_socket;
  /** @brief `connect_timeout` option read from configuration section */
  const int connect_timeout;
  /** @brief `mode` option read from configuration section */
  const routing::AccessMode mode;
  /** @brief `routing_strategy` option read from configuration section */
  routing::RoutingStrategy routing_strategy;
  /** @brief `max_connections` option read from configuration section */
  const int max_connections;
  /** @brief `max_connect_errors` option read from configuration section */
  const unsigned long long max_connect_errors;
  /** @brief `client_connect_timeout` option read from configuration section */
  const unsigned int client_connect_timeout;
  /** @brief Size of buffer to receive packets */
  const unsigned int net_buffer_length;
  /** @brief memory in kilobytes allocated for thread's stack */
  const unsigned int thread_stack_size;
};

#endif  // PLUGIN_CONFIG_ROUTING_INCLUDED

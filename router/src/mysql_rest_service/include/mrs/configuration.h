/*
  Copyright (c) 2021, 2024, Oracle and/or its affiliates.

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

#ifndef ROUTER_SRC_REST_MRS_SRC_MRS_CONFIG_H_
#define ROUTER_SRC_REST_MRS_SRC_MRS_CONFIG_H_

#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include <mysql.h>

#include "collector/destination_provider.h"
#include "helper/make_shared_ptr.h"
#include "helper/plugin_monitor.h"

namespace mrs {

enum Authentication { kAuthenticationNone, kAuthenticationBasic2Server };

class Configuration {
 public:  // Option fetched from configuration file
  std::string mysql_user_;
  std::string mysql_user_password_;
  std::string mysql_user_data_access_;
  std::string mysql_user_data_access_password_;

  std::chrono::seconds metadata_refresh_interval_;

  std::string routing_ro_;
  std::string routing_rw_;
  std::optional<uint64_t> router_id_;
  std::optional<uint64_t> host_autentication_rate_rps_;
  std::optional<uint64_t> account_autentication_rate_rps_;
  uint64_t authentication_rate_exceeded_block_for_;
  std::string router_name_;
  uint32_t default_mysql_cache_instances_;

  // how many seconds the schema monitor should wait before starting, for the
  // "mysql_user_data_access"  user to get a proper access granted
  std::chrono::seconds wait_for_metadata_schema_access_;

 public:  // Options fetched from other plugins
  bool is_https_;
  // todo(lkotula): remove ssl_
  //  SslConfiguration ssl_;

  std::shared_ptr<collector::DestinationProvider> provider_rw_;
  std::shared_ptr<collector::DestinationProvider> provider_ro_;
  std::string jwt_secret_;
  helper::MakeSharedPtr<helper::PluginMonitor> service_monitor_;
};

}  // namespace mrs

#endif  // ROUTER_SRC_REST_MRS_SRC_MRS_CONFIG_H_

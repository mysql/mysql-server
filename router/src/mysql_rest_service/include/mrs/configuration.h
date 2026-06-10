/*
  Copyright (c) 2021, 2026, Oracle and/or its affiliates.

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

#ifndef ROUTER_SRC_REST_MRS_SRC_MRS_CONFIG_H_
#define ROUTER_SRC_REST_MRS_SRC_MRS_CONFIG_H_

#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include <mysql.h>

#include "collector/destination_provider.h"
#include "helper/plugin_monitor.h"
#include "mysql/harness/make_shared_ptr.h"

#include "secure_string.h"  // NOLINT(build/include_subdir)

namespace mrs {

enum Authentication { kAuthenticationNone, kAuthenticationBasic2Server };

constexpr const char *kHostOnResolveFailed = "unknown";

class Configuration {
 public:  // Option fetched from configuration file
  std::string mysql_user_;
  mysql_harness::SecureString mysql_user_password_;
  std::string mysql_user_data_access_;
  mysql_harness::SecureString mysql_user_data_access_password_;

  std::chrono::milliseconds metadata_refresh_interval_;

  std::string routing_ro_;
  std::string routing_rw_;
  uint64_t router_id_;
  std::optional<std::string> router_name_;
  uint32_t default_mysql_cache_instances_;
  uint32_t http_port_;

  // how many seconds the schema monitor should wait before starting, for the
  // "mysql_user_data_access"  user to get a proper access granted
  std::chrono::seconds wait_for_metadata_schema_access_;

  // show the "in_development" services for this developer
  std::string developer_;
  std::string developer_debug_port_;

 public:  // Options fetched from other plugins
  bool is_https_;

  std::shared_ptr<collector::DestinationProvider> provider_rw_;
  std::shared_ptr<collector::DestinationProvider> provider_ro_;
  std::string jwt_secret_;
  mysql_harness::MakeSharedPtr<helper::PluginMonitor> service_monitor_;
};

}  // namespace mrs

#endif  // ROUTER_SRC_REST_MRS_SRC_MRS_CONFIG_H_

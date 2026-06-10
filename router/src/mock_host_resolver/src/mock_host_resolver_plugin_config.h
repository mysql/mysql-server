/*
  Copyright (c) 2026, Oracle and/or its affiliates.

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

#ifndef ROUTER_SRC_MOCK_HOST_RESOLVER_SRC_MOCK_RESOLVER_PLUGIN_CONFIG_H_
#define ROUTER_SRC_MOCK_HOST_RESOLVER_SRC_MOCK_RESOLVER_PLUGIN_CONFIG_H_

#include "mock_resolver.h"
#include "mysql/harness/config_option.h"
#include "mysql/harness/plugin_config.h"

class HostCachePluginConfig : public mysql_harness::BasePluginConfig {
 public:
  explicit HostCachePluginConfig(const mysql_harness::ConfigSection *section,
                                 ResolveActions &ra);

  std::string get_default(std::string_view option) const override;

  [[nodiscard]] bool is_required(std::string_view option) const override;

  ResolveActions &actions_;
};

#endif  // ROUTER_SRC_MOCK_HOST_RESOLVER_SRC_MOCK_RESOLVER_PLUGIN_CONFIG_H_

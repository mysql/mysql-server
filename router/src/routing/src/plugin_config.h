/*
  Copyright (c) 2015, 2023, Oracle and/or its affiliates.

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

#include "mysqlrouter/routing_plugin_export.h"

#include <string>

#include "mysql/harness/config_option.h"
#include "mysql/harness/plugin_config.h"
#include "routing_config.h"

/**
 * route specific plugin configuration.
 */
class ROUTING_PLUGIN_EXPORT RoutingPluginConfig
    : public mysql_harness::BasePluginConfig,
      public RoutingConfig {
 private:
  // is this [routing] entry for static routing or metadata-cache ?
  // it's mutable because we discover it while calling getter for
  // option destinations
  mutable bool metadata_cache_;

 public:
  /** Constructor.
   *
   * @param section from configuration file provided as ConfigSection
   */
  RoutingPluginConfig(const mysql_harness::ConfigSection *section);

  std::string get_default(const std::string &option) const override;
  bool is_required(const std::string &option) const override;

  uint16_t get_option_max_connections(
      const mysql_harness::ConfigSection *section);
};

#endif  // PLUGIN_CONFIG_ROUTING_INCLUDED

/*
  Copyright (c) 2016, 2018, Oracle and/or its affiliates. All rights reserved.

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

#ifndef METADATA_CACHE_PLUGIN_CONFIG_INCLUDED
#define METADATA_CACHE_PLUGIN_CONFIG_INCLUDED

#include "mysqlrouter/metadata_cache.h"

#include <chrono>
#include <map>
#include <string>
#include <vector>

#include <mysqlrouter/plugin_config.h>
#include "mysql/harness/config_parser.h"
#include "mysql/harness/plugin.h"
#include "tcp_address.h"

extern "C" {
extern mysql_harness::Plugin METADATA_API harness_plugin_metadata_cache;
}

class MetadataCachePluginConfig final : public mysqlrouter::BasePluginConfig {
 public:
  /** @brief Constructor
   *
   * @param section from configuration file provided as ConfigSection
   */
  MetadataCachePluginConfig(const mysql_harness::ConfigSection *section)
      : BasePluginConfig(section),
        bootstrap_addresses(
            get_bootstrap_servers(section, "bootstrap_server_addresses",
                                  metadata_cache::kDefaultMetadataPort)),
        user(get_option_string(section, "user")),
        ttl(get_option_milliseconds(section, "ttl", 0.0, 3600.0)),
        metadata_cluster(get_option_string(section, "metadata_cluster")),
        connect_timeout(
            get_uint_option<uint16_t>(section, "connect_timeout", 1)),
        read_timeout(get_uint_option<uint16_t>(section, "read_timeout", 1)),
        thread_stack_size(get_uint_option<uint32_t>(
            section, "thread_stack_size", 1, 65535)) {}

  /**
   * @param option name of the option
   */
  std::string get_default(const std::string &option) const override;
  bool is_required(const std::string &option) const override;

  /** @brief MySQL Metadata host to connect with */
  const std::vector<mysql_harness::TCPAddress> bootstrap_addresses;
  /** @brief User used for authenticating with MySQL Metadata */
  const std::string user;
  /** @brief TTL used for storing data in the cache */
  const std::chrono::milliseconds ttl;
  /** @brief Cluster in the metadata */
  const std::string metadata_cluster;
  /** @brief connect_timeout The time in seconds after which trying to connect
   * to metadata server timeouts */
  const unsigned int connect_timeout;
  /** @brief read_timeout The time in seconds after which read from metadata
   * server timeouts */
  const unsigned int read_timeout;
  /** @brief memory in kilobytes allocated for thread's stack */
  const unsigned int thread_stack_size;

 private:
  /** @brief Gets a list of metadata servers.
   *
   *
   * Throws std::invalid_argument on errors.
   *
   * @param section Instance of ConfigSection
   * @param option Option name in section
   * @param default_port Use this port when none was provided
   * @return std::vector<mysql_harness::TCPAddress>
   */
  std::vector<mysql_harness::TCPAddress> get_bootstrap_servers(
      const mysql_harness::ConfigSection *section, const std::string &option,
      uint16_t default_port);
};

#endif  // METADATA_CACHE_PLUGIN_CONFIG_INCLUDED

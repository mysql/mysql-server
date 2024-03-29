/*
  Copyright (c) 2022, 2023, Oracle and/or its affiliates.

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

#ifndef METADATA_CACHE_ROUTER_CS_OPTIONS_INCLUDED
#define METADATA_CACHE_ROUTER_CS_OPTIONS_INCLUDED

#include "cluster_metadata.h"

/** @class RouterClusterSetOptions
 *
 * @brief Represents the Router options in v2_cs_router_options view in the
 * metadata schema
 */
class RouterClusterSetOptions {
 public:
  /** @brief Pupulate the object by reading the options from the metadata
   *
   * @param session mysql server session to read metadata with
   * @param router_id id of the Router in the metadata
   *
   * @returns true if successful, false otherwise
   */
  bool read_from_metadata(mysqlrouter::MySQLSession &session,
                          const unsigned router_id);

  /** @brief Get the raw JSON string read from the metadata during the last
   * read_from_metadata() call
   */
  std::string get_string() const { return options_str_; }

  /** @brief Get the target_cluster assigned for a given Router in the metadata
   *
   * @param router_id id of the Router in the metadata
   *
   * @returns assigned target_cluster if read successful, std::nullopt otherwise
   */
  std::optional<mysqlrouter::TargetCluster> get_target_cluster(
      const unsigned router_id) const;

  /** @brief Get the stats updates ferquency value (in seconds) assigned for a
   * given Router in the metadata
   */
  std::chrono::seconds get_stats_updates_frequency() const;

  /** @brief Get the get_use_replica_primary_as_rw boolean value assigned for a
   * given Router in the metadata
   */
  bool get_use_replica_primary_as_rw() const;

 private:
  std::string get_router_option_str(const std::string &options,
                                    const std::string &name,
                                    const std::string &default_value,
                                    std::string &out_error) const;

  uint32_t get_router_option_uint(const std::string &options,
                                  const std::string &name,
                                  const uint32_t &default_value,
                                  std::string &out_error) const;

  uint32_t get_router_option_bool(const std::string &options,
                                  const std::string &name,
                                  const bool &default_value,
                                  std::string &out_error) const;

  std::string options_str_;
};

#endif  // METADATA_CACHE_ROUTER_CS_OPTIONS_INCLUDED
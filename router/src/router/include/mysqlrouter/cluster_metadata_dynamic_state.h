/*
  Copyright (c) 2018, 2022, Oracle and/or its affiliates.

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

#ifndef CLUSTER_METADATA_DYNAMIC_STATE_INCLUDED
#define CLUSTER_METADATA_DYNAMIC_STATE_INCLUDED

#include <memory>
#include <string>
#include <vector>

#include "mysqlrouter/cluster_metadata.h"

namespace mysql_harness {
class DynamicState;
}

/**
 * @brief ClusterMetadataDynamicState represents a dynamic state that the
 * metadata cache module wants to persist in the file.
 */
class ROUTER_LIB_EXPORT ClusterMetadataDynamicState {
 public:
  /**
   * @brief Creates and initializes a metadata cache dynamic state object.
   *
   * @param base_config pointer to the global dynamic state base object that
   * should be used to read and write metadata cache section.
   * @param cluster_type type of the cluster (GR or ReplicaSet)
   */
  ClusterMetadataDynamicState(mysql_harness::DynamicState *base_config,
                              mysqlrouter::ClusterType cluster_type);

  /**
   * @brief Destructor.
   */
  virtual ~ClusterMetadataDynamicState();

  /**
   * @brief Saves the current state in the associated global base object,
   * overwrites the current state in the global.
   *
   * @return success of operation
   * @retval true operation succeeded
   * @retval false operation failed
   */
  bool save();

  /**
   * @brief Loads the dynamic state from the associated global base object,
   * overwrites the current state with the loaded data.
   */
  void load();

  /**
   * @brief Saves the state to the output stream given as a parameter,
   * overwrites the stream content.
   *
   * @param state_stream stream where json content should be written to
   *
   * @return success of operation
   * @retval true operation succeeded
   * @retval false operation failed
   */
  bool save(std::ostream &state_stream);

  /**
   * @brief Sets the new value for the cluster type specific id in the state
   * object.
   *
   * @param cluster_type_specific_id new value of the cluster type specific id
   * to set
   */
  void set_cluster_type_specific_id(
      const std::string &cluster_type_specific_id);

  /**
   * @brief Sets the new value for the ClusterSet id in the state object.
   *
   * @param clusterset_id new value of the ClusterSet id
   * to set
   */
  void set_clusterset_id(const std::string &clusterset_id);

  /**
   * @brief Sets the new value for the cluster metadata server list in the state
   * object.
   *
   * @param metadata_servers vector of the new metadata servers to set
   */
  void set_metadata_servers(const std::vector<std::string> &metadata_servers);

  /**
   * @brief Reads the current cluster metadata server list from the state
   * object.
   *
   * @return vector containing current cluster metadata server list
   */
  std::vector<std::string> get_metadata_servers() const;

  /**
   * @brief Sets the new value for the last known metadata view_id of the
   * ReplicaSet cluster or ClusterSet.
   *
   * @param view_id last known metadata view_id of the ReplicaSet cluster
   */
  void set_view_id(const uint64_t view_id);

  /**
   * @brief Reads the current value of the last known metadata view_id of the
   * ReplicaSet cluster or ClusterSet from the state object.
   *
   * @return last known metadata view_id of the ReplicaSet cluster
   */
  unsigned get_view_id() const;

  /**
   * @brief Reads the current cluster type specific id from the state object.
   *
   * @return current cluster type specific id
   */
  std::string get_cluster_type_specific_id() const;

  /**
   * @brief Reads the current ClusterSet id from the state object.
   *
   * @return current cluster type specific id
   */
  std::string get_clusterset_id() const;

  /**
   * @brief Returns true if the metadata is configured to work with a
   * ClusterSet, false if a single Cluster
   *
   */
  bool is_clusterset() const;

 private:
  void save_section();

  struct Pimpl;
  std::unique_ptr<Pimpl> pimpl_;

  std::string cluster_type_specific_id_;
  std::string clusterset_id_;
  std::vector<std::string> metadata_servers_;
  uint64_t view_id_{0};

  bool changed_{false};

  mysqlrouter::ClusterType cluster_type_;
};

#endif  // CLUSTER_METADATA_DYNAMIC_STATE_INCLUDED

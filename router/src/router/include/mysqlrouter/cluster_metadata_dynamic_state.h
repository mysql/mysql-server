/*
  Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.

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

namespace mysql_harness {
class DynamicState;
}

/**
 * @brief ClusterMetadataDynamicState represents a dynamic state that the
 * metadata cache module wants to persist in the file.
 */
class ClusterMetadataDynamicState {
 public:
  /**
   * @brief Creates and initializes a metadata cache dynamic state object.
   *
   * @param base_config pointer to the global dynamic state base object that
   * should be used to read and write metadata cache section.
   */
  ClusterMetadataDynamicState(mysql_harness::DynamicState *base_config);

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
   * @brief Sets the new value for the group replication id in the state object.
   *
   * @param gr_id stream new value of the the group replication id to set
   */
  void set_group_replication_id(const std::string &gr_id);

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
   * @brief Reads the current replication group id from the state object.
   *
   * @return current replication group id
   */
  std::string get_gr_id() const;

 private:
  void save_section();

  struct Pimpl;
  std::unique_ptr<Pimpl> pimpl_;

  std::string gr_id_;
  std::vector<std::string> metadata_servers_;

  bool changed_{false};
  bool auto_save_;
};

#endif  // CLUSTER_METADATA_DYNAMIC_STATE_INCLUDED

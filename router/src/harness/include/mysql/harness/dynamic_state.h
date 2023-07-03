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

#ifndef MYSQL_HARNESS_DYNAMIC_STATE_INCLUDED
#define MYSQL_HARNESS_DYNAMIC_STATE_INCLUDED

#include <fstream>
#include <functional>
#include <iterator>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <vector>

#include "harness_export.h"

#ifdef RAPIDJSON_NO_SIZETYPEDEFINE
#include "my_rapidjson_size_t.h"
#endif

#include <rapidjson/document.h>

namespace mysql_harness {

// default allocator for rapidJson (MemoryPoolAllocator) is broken for
// SparcSolaris
using JsonAllocator = rapidjson::CrtAllocator;
using JsonValue = rapidjson::GenericValue<rapidjson::UTF8<>, JsonAllocator>;

/**
 * @brief DynamicState represents a MySQLRouter dynamic state object.
 *
 * It's meant to be used as a singleton that provides methods to read/update
 * sections from the specific modules requiring saving their runtime state.
 *
 * It handles the file handling synchronization, versioning and validation.
 *
 */
class HARNESS_EXPORT DynamicState {
 public:
  /**
   * @brief Creates and initializes dynamic state object.
   *
   * @param file_name path to the json file where the state is being stored
   */
  DynamicState(const std::string &file_name);

  /**
   * @brief Destructor.
   */
  ~DynamicState();

  /**
   * @brief Loads the json state object from the associated file, overwrites the
   * current with the file content.
   *
   * @return success of operation
   * @retval true operation succeeded
   * @retval false operation failed
   */
  bool load();

  /**
   * @brief Saves the json state object to the associated file, overwrites the
   * the file content.
   *
   * @param is_clusterset true if the metadata is configured to work with a
   * ClusterSet, false if a single Cluster
   * @param pretty if true the json data is written in a human readable json
   * format
   *
   * @return success of operation
   * @retval true operation succeeded
   * @retval false operation failed
   */
  bool save(bool is_clusterset, bool pretty = true);

  /**
   * @brief Saves the json state object to the output stream given as a
   * parameter, overwrites the stream content.
   *
   * @param output_stream stream where json content should be written to
   * @param is_clusterset true if the metadata is configured to work with a
   * ClusterSet, false if a single Cluster
   * @param pretty if true the json data is written in a human readable json
   * format
   *
   * @return success of operation
   * @retval true operation succeeded
   * @retval false operation failed
   */
  bool save_to_stream(std::ostream &output_stream, bool is_clusterset,
                      bool pretty = true);

  /**
   * @brief Returns selected state object section by its name.
   *
   * @param section_name name of the section to retrieve
   *
   * @return pointer to the rapidJson object containing the section, nullptr if
   * the section with a given name does not exist
   */
  std::unique_ptr<JsonValue> get_section(const std::string &section_name);

  /**
   * @brief Updates selected state object section.
   *
   * @param section_name    name of the section to update
   * @param value           rapidJson object to replaces the section value
   *
   * @return success of operation
   * @retval true operation succeeded
   * @retval false operation failed
   */
  bool update_section(const std::string &section_name, JsonValue &&value);

 private:
  bool load_from_stream(std::istream &input_stream);

  // throws std::runtime_error if does adhere to the schema
  void ensure_valid_against_schema();
  // throws std::runtime_error if version not compatible
  void ensure_version_compatibility();

  std::ifstream open_for_read();
  std::ofstream open_for_write();

  struct Pimpl;
  std::unique_ptr<Pimpl> pimpl_;
  std::string file_name_;
};

}  // namespace mysql_harness

#endif /* MYSQL_HARNESS_DYNAMIC_STATE_INCLUDED */

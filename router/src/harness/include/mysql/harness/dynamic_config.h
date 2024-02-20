/*
  Copyright (c) 2024, Oracle and/or its affiliates.

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

#ifndef MYSQL_HARNESS_DYNAMIC_CONFIG_INCLUDED
#define MYSQL_HARNESS_DYNAMIC_CONFIG_INCLUDED

#include <map>
#include <string>
#include <variant>

#ifdef RAPIDJSON_NO_SIZETYPEDEFINE
#include "my_rapidjson_size_t.h"
#endif

#include <rapidjson/document.h>

#include "harness_export.h"

namespace mysql_harness {

/** @class DynamicConfig
 *
 * @brief Respresents the current Router configuration. It is initialized at
 * start with the defaults and configuration from the configuration file(s).
 */
class HARNESS_EXPORT DynamicConfig {
 public:
  using OptionName = std::string;
  // std::monotstate is used as "option not set"
  using OptionValue =
      std::variant<std::monostate, int64_t, bool, double, std::string>;

  // The first srting is the plugin name. The second string is the plugin
  // section key if there are multiple plugin instances.
  // Examples:
  // for metadata_cache <"metadata_cache", "">
  // for routing:bootstrap_ro
  //        <"endoints", "bootstrap_ro">
  // for filelog <"loggers", "filelog">
  using SectionId = std::pair<std::string, std::string>;
  using SectionOptions = std::map<OptionName, OptionValue>;

  using JsonAllocator = rapidjson::CrtAllocator;
  using JsonDocument =
      rapidjson::GenericDocument<rapidjson::UTF8<>, JsonAllocator>;

  /**
   * Sets a given option in a given section to a specific value.
   *
   * @param section_id  identifier of a section for this operation
   * @param option_name name of the option inside a section for this operation
   * @param value       value to be set
   */
  void set_option_configured(const SectionId &section_id,
                             const OptionName &option_name,
                             const OptionValue &value);

  /**
   * Sets a default for an option in a given section to a specific value.
   *
   * @param section_id  identifier of a section for this operation
   * @param option_name name of the option inside a section for this operation
   * @param default_value_cluster default value for srtandalne cluster setup to
   *                              be set
   * @param default_value_clusterset  default value for the clusterset setup to
   *                                  be set
   */
  void set_option_default(const SectionId &section_id,
                          const OptionName &option_name,
                          const OptionValue &default_value_cluster,
                          const OptionValue &default_value_clusterset);

  /**
   * Sets a default for an option in a given section to a specific value.
   * Overload for more common case when cluster and clusterset values are the
   * same.
   *
   * @param section_id    identifier of a section for this operation
   * @param option_name   name of the option inside a section for this operation
   * @param default_value default value for both srtandalne cluster and
   *                      clusterset setup to be set
   */
  void set_option_default(const SectionId &section_id,
                          const OptionName &option_name,
                          const OptionValue &default_value);

  /**
   * @brief Type of the options stored in the dynamic configuration object.
   */
  enum class ValueType {
    ConfiguredValue,      // value currently configured
    DefaultForCluster,    // default value for the standalone cluster setup
    DefaultForClusterSet  // default value for the clusterset setup
  };

  /**
   * @brief Return the current configuration options and their values stored in
   * the dynamic configuration object.
   *
   * @param value_type  type of the options to be returned
   *
   * @returns JSON Document containing all the options of a selected type.
   */
  JsonDocument get_json(const ValueType value_type) const;

  /**
   * @brief Return the current configuration options and their values stored in
   * the dynamic configuration object as a string.
   *
   * @param value_type  type of the options to be returned
   *
   * @returns JSON string containing all the options of a selected type.
   */
  std::string get_json_as_string(const ValueType value_type) const;

  /**
   * Returns a singleton object of DynamicConfig class.
   */
  static DynamicConfig &instance();

  /**
   * Clear the DynamicConfig object (remove all registered sections with their
   * option values and defaults).
   */
  void clear();

 private:
  DynamicConfig() = default;
  DynamicConfig(const DynamicConfig &) = delete;
  DynamicConfig &operator=(const DynamicConfig &) = delete;

  void set_option(const ValueType value_type, const SectionId &section_id,
                  const OptionName &option_name, const OptionValue &value);

  struct SectionConfig {
    SectionOptions options;
  };
  using Config = std::map<SectionId, SectionConfig>;

  Config &get_config(const ValueType value_type);
  Config const &get_config(const ValueType value_type) const;

  // stores the currently configured values
  Config configured_;
  // stores the defaults for the standalone cluster configuration
  Config defaults_cluster_;
  // stores the defaults for the clusterset configuration
  Config defaults_clusterset_;
};

}  // namespace mysql_harness

#endif  // MYSQL_HARNESS_DYNAMIC_CONFIG_INCLUDED
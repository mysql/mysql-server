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

#ifndef MYSQL_HARNESS_SESSION_CONFIG_EXPOSER_INCLUDED
#define MYSQL_HARNESS_SESSION_CONFIG_EXPOSER_INCLUDED

#include <map>
#include <string>
#include <string_view>
#include <variant>

#include "harness_export.h"
#include "mysql/harness/config_parser.h"
#include "mysql/harness/dynamic_config.h"

namespace mysql_harness {

/** @class SectionConfigExposer
 *
 * @brief Base class for a plugin specific specializations. Lets the plugin
 * expose their initial and default configuration to the DynamicConfig object.
 */
class HARNESS_EXPORT SectionConfigExposer {
 public:
  using DC = mysql_harness::DynamicConfig;

  using OptionValue = DC::OptionValue;

  enum class Mode { ExposeInitialConfig, ExposeDefaultConfig };

  /**
   * Constructor.
   *
   * @param initial flag indicating if the initial or default configuration is
   * being shared.
   * @param default_section default section of the current configuration
   * @param section_id identifier of the plugin configuration in the Dynamic
   * config object.
   */
  SectionConfigExposer(bool initial,
                       const mysql_harness::ConfigSection &default_section,
                       const DC::SectionId &section_id)
      : mode_(initial ? Mode::ExposeInitialConfig : Mode::ExposeDefaultConfig),
        default_section_(default_section),
        section_id_(section_id) {}

  virtual ~SectionConfigExposer() = default;

  SectionConfigExposer(const SectionConfigExposer &) = delete;

 protected:
  /**
   * Exposes plugin instance configuration.
   */
  virtual void expose() = 0;

  const Mode mode_;
  const mysql_harness::ConfigSection &default_section_;

  const DC::SectionId section_id_;
  const DC::SectionId common_section_id_{"common", ""};

  /**
   * Exposes single option configuration.
   *
   * @param option option name
   * @param value configured (initial) value of the option
   * @param default_value default value for both Cluster and ClusterSet
   * configuration
   * @param is_common indicates whether the options is supposed to also be
   * shared in the "common" section of the configuration
   */
  void expose_option(std::string_view option, const OptionValue &value,
                     const OptionValue &default_value, bool is_common = false);

  /**
   * Exposes single option configuration (overload for options that have
   * different defaults for Cluster and for ClusterSet configuration).
   *
   * @param option option name
   * @param value configured (initial) value of the option
   * @param default_value_cluster default value for Cluster configuration
   * @param default_value_clusterset default value for ClusterSet configuration
   * @param is_common indicates whether the options is supposed to also be
   * shared in the "common" section of the configuration
   */
  void expose_option(std::string_view option, const OptionValue &value,
                     const OptionValue &default_value_cluster,
                     const OptionValue &default_value_clusterset,
                     bool is_common);

 private:
  void expose_str_option(std::string_view option, const OptionValue &value,
                         const OptionValue &default_value_cluster,
                         const OptionValue &default_value_clusterset,
                         bool is_common = false);

  void expose_int_option(std::string_view option, const OptionValue &value,
                         const OptionValue &default_value_cluster,
                         const OptionValue &default_value_clusterset,
                         bool is_common = false);

  void expose_double_option(std::string_view option, const OptionValue &value,
                            const OptionValue &default_value_cluster,
                            const OptionValue &default_value_clusterset,
                            bool is_common = false);

  void expose_bool_option(std::string_view option, const OptionValue &value,
                          const OptionValue &default_value_cluster,
                          const OptionValue &default_value_clusterset,
                          bool is_common = false);

  void expose_default(std::string_view option,
                      const auto &default_value_cluster,
                      const auto &default_value_clusterset, bool is_common) {
    DC::instance().set_option_default(
        section_id_, option, default_value_cluster, default_value_clusterset);
    if (is_common) {
      DC::instance().set_option_default(common_section_id_, option,
                                        default_value_cluster,
                                        default_value_clusterset);
    }
  }
};

}  // namespace mysql_harness

#endif  // MYSQL_HARNESS_SESSION_CONFIG_EXPOSER_INCLUDED

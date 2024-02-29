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

#include "mysql/harness/section_config_exposer.h"

#include <cassert>
#include <string_view>

#include "mysql/harness/config_option.h"

namespace mysql_harness {

void SectionConfigExposer::expose_option(std::string_view option,
                                         const OptionValue &value,
                                         const OptionValue &default_value,
                                         bool is_common) {
  if (std::holds_alternative<int64_t>(value) ||
      std::holds_alternative<int64_t>(default_value)) {
    expose_int_option(option, value, default_value, default_value, is_common);
  } else if (std::holds_alternative<std::string>(value) ||
             std::holds_alternative<std::string>(default_value)) {
    expose_str_option(option, value, default_value, default_value, is_common);
  } else if (std::holds_alternative<double>(value) ||
             std::holds_alternative<double>(default_value)) {
    expose_double_option(option, value, default_value, default_value,
                         is_common);
  } else if (std::holds_alternative<bool>(value) ||
             std::holds_alternative<bool>(default_value)) {
    expose_bool_option(option, value, default_value, default_value, is_common);
  } else {
    // skip
  }
}

void SectionConfigExposer::expose_option(
    std::string_view option, const OptionValue &value,
    const OptionValue &default_value_cluster,
    const OptionValue &default_value_clusterset, bool is_common) {
  if (std::holds_alternative<int64_t>(value) ||
      std::holds_alternative<int64_t>(default_value_cluster) ||
      std::holds_alternative<int64_t>(default_value_clusterset)) {
    expose_int_option(option, value, default_value_cluster,
                      default_value_clusterset, is_common);
  } else if (std::holds_alternative<std::string>(value) ||
             std::holds_alternative<std::string>(default_value_cluster) ||
             std::holds_alternative<std::string>(default_value_clusterset)) {
    expose_str_option(option, value, default_value_cluster,
                      default_value_clusterset, is_common);
  } else if (std::holds_alternative<double>(value) ||
             std::holds_alternative<double>(default_value_cluster) ||
             std::holds_alternative<double>(default_value_clusterset)) {
    expose_double_option(option, value, default_value_cluster,
                         default_value_clusterset, is_common);
  } else if (std::holds_alternative<bool>(value) ||
             std::holds_alternative<bool>(default_value_cluster) ||
             std::holds_alternative<bool>(default_value_clusterset)) {
    expose_bool_option(option, value, default_value_cluster,
                       default_value_clusterset, is_common);
  } else {
    // skip
  }
}

void SectionConfigExposer::expose_str_option(
    std::string_view option, const OptionValue &value,
    const OptionValue &default_value_cluster,
    const OptionValue &default_value_clusterset, bool is_common) {
  if (mode_ == Mode::ExposeInitialConfig) {
    DC::instance().set_option_configured(section_id_, option, value);
    if (is_common) {
      if (default_section_.has(option)) {
        DC::instance().set_option_configured(common_section_id_, option,
                                             default_section_.get(option));
      } else if (std::holds_alternative<std::string>(default_value_cluster)) {
        DC::instance().set_option_configured(common_section_id_, option,
                                             default_value_cluster);
      }
    }
  } else {
    assert(mode_ == Mode::ExposeDefaultConfig);
    expose_default(option, default_value_cluster, default_value_clusterset,
                   is_common);
  }
}

void SectionConfigExposer::expose_int_option(
    std::string_view option, const OptionValue &value,
    const OptionValue &default_value_cluster,
    const OptionValue &default_value_clusterset, bool is_common) {
  if (mode_ == Mode::ExposeInitialConfig) {
    DC::instance().set_option_configured(section_id_, option, value);
    if (is_common) {
      if (default_section_.has(option)) {
        try {
          const auto value = mysql_harness::option_as_int<int64_t>(
              default_section_.get(option), "");
          DC::instance().set_option_configured(common_section_id_, option,
                                               value);
          return;
        } catch (...) {
          // if it failed fallback to setting default
        }
      }
      DC::instance().set_option_configured(common_section_id_, option,
                                           default_value_cluster);
    }
  } else {
    assert(mode_ == Mode::ExposeDefaultConfig);
    expose_default(option, default_value_cluster, default_value_clusterset,
                   is_common);
  }
}

void SectionConfigExposer::expose_double_option(
    std::string_view option, const OptionValue &value,
    const OptionValue &default_value_cluster,
    const OptionValue &default_value_clusterset, bool is_common) {
  if (mode_ == Mode::ExposeInitialConfig) {
    DC::instance().set_option_configured(section_id_, option, value);
    if (is_common) {
      if (default_section_.has(option)) {
        try {
          const auto value =
              mysql_harness::option_as_double(default_section_.get(option), "");
          DC::instance().set_option_configured(common_section_id_, option,
                                               value);
          return;
        } catch (...) {
          // if it failed fallback to setting default
        }
      }
      DC::instance().set_option_configured(
          common_section_id_, option, std::get<double>(default_value_cluster));
    }
  } else {
    assert(mode_ == Mode::ExposeDefaultConfig);
    expose_default(option, default_value_cluster, default_value_clusterset,
                   is_common);
  }
}

void SectionConfigExposer::expose_bool_option(
    std::string_view option, const OptionValue &value,
    const OptionValue &default_value_cluster,
    const OptionValue &default_value_clusterset, bool is_common) {
  if (mode_ == Mode::ExposeInitialConfig) {
    DC::instance().set_option_configured(section_id_, option, value);
    if (is_common) {
      if (default_section_.has(option)) {
        DC::instance().set_option_configured(
            common_section_id_, option, default_section_.get(option) != "0");
      } else {
        DC::instance().set_option_configured(
            common_section_id_, option, std::get<bool>(default_value_cluster));
      }
    }
  } else {
    assert(mode_ == Mode::ExposeDefaultConfig);
    expose_default(option, default_value_cluster, default_value_clusterset,
                   is_common);
  }
}

}  // namespace mysql_harness

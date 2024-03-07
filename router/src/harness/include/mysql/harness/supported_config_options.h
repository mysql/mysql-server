/*
  Copyright (c) 2022, 2024, Oracle and/or its affiliates.

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

#ifndef MYSQL_HARNESS_SUPPORTED_OPTIONS
#define MYSQL_HARNESS_SUPPORTED_OPTIONS

#include <array>
#include <string_view>

namespace mysql_harness {
namespace loader {
namespace options {
constexpr std::string_view kOrigin{"origin"};
constexpr std::string_view kProgram{"program"};
constexpr std::string_view kLoggingFolder{"logging_folder"};
constexpr std::string_view kRuntimeFolder{"runtime_folder"};
constexpr std::string_view kDataFolder{"data_folder"};
constexpr std::string_view kPluginFolder{"plugin_folder"};
constexpr std::string_view kConfigFolder{"config_folder"};
constexpr std::string_view kUnknownConfigOption{"unknown_config_option"};
}  // namespace options
}  // namespace loader

static constexpr std::array loader_supported_options [[maybe_unused]]{
    loader::options::kOrigin,  //
    loader::options::kProgram,
    loader::options::kLoggingFolder,
    loader::options::kRuntimeFolder,
    loader::options::kDataFolder,
    loader::options::kPluginFolder,
    loader::options::kConfigFolder,
    loader::options::kUnknownConfigOption,
};

}  // namespace mysql_harness

#endif /* MYSQL_HARNESS_SUPPORTED_OPTIONS */

/*
  Copyright (c) 2017, 2022, Oracle and/or its affiliates.

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

#ifndef ROUTER_CONFIG_FILES_INCLUDED
#define ROUTER_CONFIG_FILES_INCLUDED

#include <cstddef>
#include <string>
#include <system_error>
#include <vector>

#include "mysql/harness/stdx/expected.h"
#include "mysqlrouter/router_export.h"

/*
 * @brief Converts configuration file name into legacy configuration file name.
 * @return legacy configuration file name (ends with .ini suffix) if
 * configuration file has .conf suffix, empty string otherwise
 */
std::string use_ini_extension(const std::string &file_name);

enum class ConfigFilePathValidatorErrc {
  kDuplicate = 1,
  kNotReadable = 2,
  kExtraWithoutMainConfig = 3,
  kNoConfigfile = 4,
};

namespace std {
template <>
struct is_error_code_enum<ConfigFilePathValidatorErrc> : true_type {};
}  // namespace std

std::error_code ROUTER_LIB_EXPORT make_error_code(ConfigFilePathValidatorErrc);

/*
 * @class ConfigFilePathValidator
 *
 * The class ConfigFiles encapsulates handling of configuration files
 * of different types. There are 3 types of configuration files: default
 * configuration files, configuration files, extra configuration files.
 */
class ROUTER_LIB_EXPORT ConfigFilePathValidator {
 public:
  /*
   * @brief Constructor with configuration files.
   *
   * There are 3 types of configuration files: default configuration files,
   * configuration files and extra configuration files.
   *
   * @param default_config_files list of configuration files which will be read
   *                             (if available) by default
   * @param config_files list of configuration files passed using command line
   * @param extra_config_files list of extra configuration files passed using
   *                           command line
   */
  ConfigFilePathValidator(std::vector<std::string> default_config_files,
                          std::vector<std::string> config_files,
                          std::vector<std::string> extra_config_files)
      : default_config_files_{std::move(default_config_files)},
        config_files_{std::move(config_files)},
        extra_config_files_{std::move(extra_config_files)} {}

  struct ValidateError {
    std::error_code ec;
    std::string current_filename;
    std::vector<std::string> paths_attempted;
  };

  stdx::expected<std::vector<std::string>, ValidateError> validate(
      bool main_config_file_required = true) const;

 private:
  std::vector<std::string> default_config_files_;
  std::vector<std::string> config_files_;
  std::vector<std::string> extra_config_files_;
};

#endif  // ROUTER_CONFIG_FILES_INCLUDED

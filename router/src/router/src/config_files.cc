/*
  Copyright (c) 2017, 2024, Oracle and/or its affiliates.

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
#include <algorithm>
#include <fstream>

#include "mysql/harness/filesystem.h"
#include "mysql/harness/stdx/expected.h"
#include "mysql/harness/utility/string.h"
#include "mysqlrouter/config_files.h"

std::string use_ini_extension(const std::string &file_name) {
  auto pos = file_name.find_last_of(".conf");
  if (pos == std::string::npos || (pos != file_name.length() - 1)) {
    return std::string();
  }
  return file_name.substr(0, pos - 4) + ".ini";
}

static bool contains(const std::vector<std::string> &container,
                     const std::string &file) {
  auto pos = std::find(container.begin(), container.end(), file);
  return pos != container.end();
}

stdx::expected<std::vector<std::string>, ConfigFilePathValidator::ValidateError>
ConfigFilePathValidator::validate(bool main_config_file_required) const {
  std::vector<std::string> available_config_files;
  std::vector<std::string> paths_attempted;

  auto collect_unique_files =
      [&available_config_files, &paths_attempted](
          const std::string &file, bool required,
          bool with_fallback) -> stdx::expected<void, ValidateError> {
    if (contains(available_config_files, file)) {
      return stdx::unexpected(ValidateError{
          make_error_code(ConfigFilePathValidatorErrc::kDuplicate), file,
          available_config_files});
    }

    mysql_harness::Path p(file);

    if (p.is_readable()) {
      available_config_files.push_back(file);
    } else {
      if (required) {
        return stdx::unexpected(ValidateError{
            make_error_code(ConfigFilePathValidatorErrc::kNotReadable), file,
            available_config_files});
      }
      paths_attempted.push_back(file);

      if (with_fallback) {
        std::string file_ini = use_ini_extension(file);

        if (!file_ini.empty()) {
          if (mysql_harness::Path(file_ini).is_readable()) {
            available_config_files.push_back(file_ini);
          } else {
            paths_attempted.push_back(file_ini);
          }
        }
      }
    }

    return {};
  };

  if (config_files_.empty()) {
    for (auto const &file : default_config_files_) {
      auto res = collect_unique_files(file, false, true);
      if (!res) {
        return stdx::unexpected(res.error());
      }
    }
  } else {
    for (auto const &file : config_files_) {
      auto res = collect_unique_files(file, true, false);
      if (!res) {
        return stdx::unexpected(res.error());
      }
    }
  }

  if (available_config_files.empty()) {
    if (!extra_config_files_.empty()) {
      // Can not have extra configuration files when we do not have other
      // configuration files
      return stdx::unexpected(ValidateError{
          make_error_code(ConfigFilePathValidatorErrc::kExtraWithoutMainConfig),
          "", paths_attempted});
    } else if (main_config_file_required) {
      return stdx::unexpected(ValidateError{
          make_error_code(ConfigFilePathValidatorErrc::kNoConfigfile), "",
          paths_attempted});
    }
  }

  for (auto const &file : extra_config_files_) {
    auto res = collect_unique_files(file, true, false);
    if (!res) {
      return stdx::unexpected(res.error());
    }
  }

  return available_config_files;
}

const std::error_category &config_file_path_validator_category() noexcept {
  class category_impl : public std::error_category {
   public:
    const char *name() const noexcept override {
      return "config_file_path_validator";
    }
    std::string message(int ev) const override {
      switch (static_cast<ConfigFilePathValidatorErrc>(ev)) {
        case ConfigFilePathValidatorErrc::kNoConfigfile:
          return "no config file";
        case ConfigFilePathValidatorErrc::kDuplicate:
          return "duplicate config file";
        case ConfigFilePathValidatorErrc::kExtraWithoutMainConfig:
          return "extra config without main config file";
        case ConfigFilePathValidatorErrc::kNotReadable:
          return "config file not readable";
      }

      return "(unrecognized error)";
    }
  };

  static category_impl instance;
  return instance;
}

std::error_code make_error_code(ConfigFilePathValidatorErrc e) {
  return {static_cast<int>(e), config_file_path_validator_category()};
}

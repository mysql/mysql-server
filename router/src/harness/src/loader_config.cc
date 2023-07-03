/*
  Copyright (c) 2017, 2023, Oracle and/or its affiliates.

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
#include "mysql/harness/loader_config.h"

#include <algorithm>
#include <cassert>
#include <sstream>
#include <stdexcept>
#include <string>

#include "mysql/harness/filesystem.h"
#include "mysql/harness/logging/logging.h"
#include "utilities.h"  // find_range_first

using mysql_harness::utility::find_range_first;

namespace mysql_harness {

void LoaderConfig::fill_and_check() {
  // Set the default value of library for all sections that do not
  // have the library set.
  for (auto &&elem : sections_) {
    if (!elem.second.has("library")) {
      const std::string &section_name = elem.first.first;

      // Section name is always a always stored as lowercase legal C
      // identifier, hence it is also legal as a file name, but we
      // assert that to make sure.
      assert(std::all_of(
          section_name.begin(), section_name.end(),
          [](const char ch) -> bool { return isalnum(ch) || ch == '_'; }));

      elem.second.set("library", section_name);
    }
  }

  // Check all sections to make sure that the values are correct.
  for (auto &&iter = sections_.begin(); iter != sections_.end(); ++iter) {
    const std::string &section_name = iter->second.name;
    const auto &seclist = find_range_first(sections_, section_name, iter);

    const std::string &library = seclist.first->second.get("library");
    auto library_mismatch = [&library](decltype(*seclist.first) &it) -> bool {
      return it.second.get("library") != library;
    };

    auto mismatch = find_if(seclist.first, seclist.second, library_mismatch);
    if (mismatch != seclist.second) {
      const auto &name = seclist.first->first;
      std::ostringstream buffer;
      buffer << "Library for section '" << name.first << ":" << name.second
             << "' does not match library in section '" << mismatch->first.first
             << ":" << mismatch->first.second;
      throw bad_section(buffer.str());
    }
  }

  std::string unknown_config_option_str = "warning";
  if (has_default("unknown_config_option")) {
    unknown_config_option_str = get_default("unknown_config_option");
    std::transform(unknown_config_option_str.begin(),
                   unknown_config_option_str.end(),
                   unknown_config_option_str.begin(), ::tolower);
    if (unknown_config_option_str != "warning" &&
        unknown_config_option_str != "error") {
      throw bad_option_value(
          "Invalid value for DEFAULT.unknown_config_option: '" +
          get_default("unknown_config_option") +
          "'. Allowed are: 'error' or 'warning'");
    }
  }
  this->error_on_unsupported_option = unknown_config_option_str == "error";
}

void LoaderConfig::read(const Path &path) {
  Config::read(
      path);  // throws derivatives of std::runtime_error, std::logic_error

  // This means it is checked after each file load, which might
  // require changes in the future if checks that cover the entire
  // configuration are added. Right now it just contain safety checks.
  fill_and_check();  // throws derivatives of std::runtime_error
}

bool LoaderConfig::logging_to_file() const {
  constexpr const char *kFolderOption = "logging_folder";
  return has_default(kFolderOption) && !get_default(kFolderOption).empty();
}

Path LoaderConfig::get_log_file() const {
  constexpr const char *kLogger = logging::kConfigSectionLogger;
  constexpr const char *kNone = logging::kNone;
  constexpr const char *kLogFilename = logging::kConfigOptionLogFilename;
  auto logging_folder = get_default("logging_folder");
  std::string log_filename;

  if (has(kLogger) && get(kLogger, kNone).has(kLogFilename) &&
      !get(kLogger, kNone).get(kLogFilename).empty())
    log_filename = get(kLogger, kNone).get(kLogFilename);
  // otherwise, set it to default
  else
    log_filename = logging::kDefaultLogFilename;

  return Path(logging_folder).join(log_filename);
}

}  // namespace mysql_harness

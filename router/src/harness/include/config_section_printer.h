/*
  Copyright (c) 2025, 2026, Oracle and/or its affiliates.

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

#ifndef MYSQLROUTER_CONFIG_SECTION_PRINTER_INCLUDED
#define MYSQLROUTER_CONFIG_SECTION_PRINTER_INCLUDED

#include "harness_export.h"

#include <algorithm>
#include <map>
#include <ostream>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "config_builder.h"

namespace mysql_harness {

class HARNESS_EXPORT ConfigSectionPrinter {
 public:
  ConfigSectionPrinter(
      std::ostream &ostream,
      const std::map<std::string, std::string> &config_cmdln_options,
      const std::string &section_name)
      : ostream_(ostream),
        config_cmdln_options_(config_cmdln_options),
        section_name_(section_name) {
    auto section_name_lc = section_name;
    std::transform(section_name_lc.begin(), section_name_lc.end(),
                   section_name_lc.begin(), ::tolower);

    used_sections_.insert(section_name_lc);
  }

  ConfigSectionPrinter &add_line(std::string_view key, const std::string &value,
                                 bool force_empty = false) {
    std::string key_s(key);
    std::string cmdln_option_key = section_name_ + "." + key_s;
    std::transform(cmdln_option_key.begin(), cmdln_option_key.end(),
                   cmdln_option_key.begin(), ::tolower);

    // cmdline options overwrite internal defaults.
    if (config_cmdln_options_.contains(cmdln_option_key)) {
      section_options_.emplace_back(key,
                                    config_cmdln_options_.at(cmdln_option_key));

      used_cmdln_options_.insert(key_s);
    } else if (!value.empty() || force_empty) {
      section_options_.emplace_back(key, value);
    }

    return *this;
  }

  ~ConfigSectionPrinter() {
    // got through all the command line options for this section and see if
    // there are some that user provided and we did not use them yet, now is
    // time to add them to our section
    for (const auto &cmdln_option : config_cmdln_options_) {
      const auto &cmdln_option_key = cmdln_option.first;
      const auto dot = cmdln_option_key.find('.');
      if (dot == std::string::npos) continue;
      const std::string section = cmdln_option_key.substr(0, dot);

      std::string section_name_lowerc = section_name_;
      std::transform(section_name_lowerc.begin(), section_name_lowerc.end(),
                     section_name_lowerc.begin(), ::tolower);

      if (section != section_name_lowerc) continue;

      const std::string option =
          cmdln_option_key.substr(dot + 1, cmdln_option_key.length() - dot - 1);

      if (!used_cmdln_options_.contains(option))
        section_options_.emplace_back(option, cmdln_option.second);
    }

    ostream_ << ConfigBuilder::build_section(section_name_, section_options_);
  }

  static void add_remaining_sections(
      std::ostream &ostream,
      const std::map<std::string, std::string> &config_cmdln_options) {
    std::string current_section;
    std::vector<ConfigBuilder::kv_type> section_options;

    for (const auto &cmdln_option : config_cmdln_options) {
      const auto &cmdln_option_key = cmdln_option.first;
      const auto dot = cmdln_option_key.find('.');
      // that should be checked before
      assert(dot != std::string::npos);
      const std::string section_name = cmdln_option_key.substr(0, dot);

      // MRS bootstrap is currently done as a separate step, if we add the
      // configuration overwrites here it will fail later complaining that there
      // already is a mysql_rest_service section
      if (section_name == "mysql_rest_service") {
        continue;
      }

      if (used_sections_.contains(section_name)) {
        continue;
      }

      if (section_name != current_section) {
        if (!current_section.empty()) {
          ostream << ConfigBuilder::build_section(current_section,
                                                  section_options);
        }
        current_section = section_name;
        section_options.clear();
      }

      const std::string option =
          cmdln_option_key.substr(dot + 1, cmdln_option_key.length() - dot - 1);

      section_options.emplace_back(option, cmdln_option.second);
    }

    if (!current_section.empty()) {
      ostream << ConfigBuilder::build_section(current_section, section_options);
    }
  }

 private:
  std::ostream &ostream_;
  const std::map<std::string, std::string> &config_cmdln_options_;
  const std::string section_name_;

  std::vector<ConfigBuilder::kv_type> section_options_;

  std::set<std::string> used_cmdln_options_;
  static std::set<std::string> used_sections_;
};

}  // namespace mysql_harness

#endif

/*
  Copyright (c) 2017, 2018, Oracle and/or its affiliates. All rights reserved.

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
#include "config_files.h"
#include "mysql/harness/filesystem.h"
#include "router_app.h"

#include <algorithm>
#include <fstream>

using mysqlrouter::string_format;
const std::string path_sep = ":";

std::string use_ini_extension(const std::string &file_name) {
  auto pos = file_name.find_last_of(".conf");
  if (pos == std::string::npos || (pos != file_name.length() - 1)) {
    return std::string();
  }
  return file_name.substr(0, pos - 4) + ".ini";
}

ConfigFiles::ConfigFiles(const std::vector<std::string> &default_config_files,
                         const std::vector<std::string> &config_files,
                         const std::vector<std::string> &extra_config_files) {
  auto config_file_containers = {&default_config_files, &config_files,
                                 &extra_config_files};

  for (const vector<string> *vec : config_file_containers) {
    for (const std::string &file : *vec) {
      auto pos = std::find(available_config_files_.begin(),
                           available_config_files_.end(), file);
      if (pos != available_config_files_.end()) {
        throw std::runtime_error(
            string_format("Duplicate configuration file: %s.", file.c_str()));
      }
      if (mysql_harness::Path(file).is_readable()) {
        available_config_files_.push_back(file);
        if (vec != &extra_config_files) {
          valid_config_count_++;
        }
        continue;
      }

      // if this is a default path we also check *.ini version to be backward
      // compatible with the previous router versions that used *.ini
      std::string file_ini;
      if (vec == &default_config_files) {
        file_ini = use_ini_extension(file);
        if (!file_ini.empty() && mysql_harness::Path(file_ini).is_readable()) {
          available_config_files_.push_back(file_ini);
          valid_config_count_++;
          continue;
        }
      }

      paths_attempted_.append(file).append(path_sep);
      if (!file_ini.empty()) paths_attempted_.append(file_ini).append(path_sep);
    }
  }

  // Can not have extra configuration files when we do not have other
  // configuration files
  if (!extra_config_files.empty() && valid_config_count_ == 0) {
    throw std::runtime_error(
        "Extra configuration files only work when other configuration files "
        "are available.");
  }
}

const std::vector<std::string> &ConfigFiles::available_config_files() const {
  return available_config_files_;
}

const std::string &ConfigFiles::paths_attempted() const {
  return paths_attempted_;
}

bool ConfigFiles::empty() const { return available_config_files_.empty(); }

size_t ConfigFiles::size() const { return available_config_files_.size(); }

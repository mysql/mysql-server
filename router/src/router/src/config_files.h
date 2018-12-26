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

#ifndef ROUTER_CONFIG_FILES_INCLUDED
#define ROUTER_CONFIG_FILES_INCLUDED

#include <cstddef>
#include <string>
#include <vector>

/*
 * @brief Converts configuration file name into legacy configuration file name.
 * @return legacy configuration file name (ends with .ini suffix) if
 * configuration file has .conf suffix, empty string otherwise
 */
std::string use_ini_extension(const std::string &file_name);

/*
 * @class ConfigFiles
 *
 * The class ConfigFiles encapsulates handling of configuration files
 * of different types. There are 3 types of configuration files: default
 * configuration files, configuration files, extra configuration files.
 */
class ConfigFiles {
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
  ConfigFiles(const std::vector<std::string> &default_config_files,
              const std::vector<std::string> &config_files,
              const std::vector<std::string> &extra_config_files);
  /*
   * @return vector of configuration file names that exist and can be opened for
   * reading.
   */
  const std::vector<std::string> &available_config_files() const;

  /*
   * @return list of comma separated configuration files that were checked if
   * they are available.
   */
  const std::string &paths_attempted() const;

  /*
   * @return true if there is at least 1 available configuration file, false
   * otherwise.
   */
  bool empty() const;

  /*
   * @return a number of available configuration files.
   */
  size_t size() const;

 private:
  // vector of available configuration file names
  std::vector<std::string> available_config_files_;

  // number of verified config files and defalt config files that were checked
  size_t valid_config_count_ = 0;

  // list of comma separated configuration files that were checked
  std::string paths_attempted_;
};

#endif  // ROUTER_CONFIG_FILES_INCLUDED

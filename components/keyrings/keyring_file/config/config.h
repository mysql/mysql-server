/* Copyright (c) 2021, 2024, Oracle and/or its affiliates.

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
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef KEYRING_FILE_CONFIG_INCLUDED
#define KEYRING_FILE_CONFIG_INCLUDED

#include <memory>
#include <string>
#include <vector>

namespace keyring_file::config {

/* Component path */
extern char *g_component_path;

/* Instance path */
extern char *g_instance_path;

/* Config details */
class Config_pod {
 public:
  std::string config_file_path_;
  bool read_only_;
};

/**
  Read configuration file

  @param [out] config_pod Configuration details
  @param [out] err        Error message

  @returns status of read operation
    @retval false Success
    @retval true  Failure
*/
bool find_and_read_config_file(std::unique_ptr<Config_pod> &config_pod,
                               std::string &err);

/**
  Create configuration vector

  @param [out] metadata Configuration data

  @returns status of read operation
    @retval false Success
    @retval true  Failure
*/
bool create_config(
    std::unique_ptr<std::vector<std::pair<std::string, std::string>>>
        &metadata);

}  // namespace keyring_file::config

#endif  // !KEYRING_FILE_CONFIG_INCLUDED

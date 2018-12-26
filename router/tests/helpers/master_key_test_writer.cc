/*
  Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.

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

#include <stdlib.h>
#include <fstream>
#include <iostream>
#include <sstream>

std::string get_master_key_file_path() {
#ifdef _WIN32
  char env_str[2000] = {0};
  size_t len = 0;
  int err_code;

  if ((err_code =
           getenv_s(&len, env_str, sizeof(env_str), "MASTER_KEY_PATH")) != 0)
    throw std::runtime_error("Failed to read MASTER_KEY_PATH variable: " +
                             std::to_string(err_code));
  return std::string(env_str);
#else
  return getenv("MASTER_KEY_PATH");
#endif
}

/*
 * MySQLRouter sets ROUTER_ID environment variable which can be used by
 * master-key-writer/master-key-writer to distinguish between routers, and
 * write/read appropriate master key.
 *
 * This function checks if ROUTER_ID variable is set.
 *
 * @return true if ROUTER_ID is set in environment, false otherwise.
 */
bool check_router_id() {
#ifdef _WIN32
  char env_str[2000] = {0};
  size_t len = 0;
  int err_code;

  return getenv_s(&len, env_str, sizeof(env_str), "ROUTER_ID") == 0;
#else
  return getenv("ROUTER_ID") != nullptr;
#endif
}

int main() {
  if (!check_router_id()) return 1;

  std::stringstream file_content;
  file_content << std::cin.rdbuf();
  std::string master_key = file_content.str();
  {
    std::ofstream output_file(get_master_key_file_path());
    output_file << master_key;
  }
  return 0;
}

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

#ifndef MYSQLROUTER_CONFIG_BUILDER_INCLUDED
#define MYSQLROUTER_CONFIG_BUILDER_INCLUDED

#include <string>
#include <utility>
#include <vector>

#include "mysql/harness/utility/string.h"

/**
 * builder for MySQL Router config files.
 *
 * generates a config-file strings from definition
 */
class ConfigBuilder {
 public:
  using kv_type = std::pair<std::string, std::string>;

  /**
   * build a config file section from key-value pairs.
   */
  static std::string build_section(const std::string &section,
                                   std::vector<kv_type> pairs) {
    std::vector<std::string> lines{"[" + section + "]"};

    for (const auto &pair : pairs) {
      lines.push_back(build_pair(pair));
    }
    return mysql_harness::join(lines, "\n");
  }

  /**
   * build a string from a key-value pair.
   */
  static std::string build_pair(const kv_type &pair) {
    return pair.first + "=" + pair.second;
  }
};

#endif

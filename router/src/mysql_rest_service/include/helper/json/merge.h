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

#include <optional>
#include <set>
#include <string>

namespace helper {
namespace json {

/**
 * @brief Merge fields from j2 into j1, which must be JSON objects.
 *
 * Returns a new JSON object with all fields of j1 plus fields of j2 which are
 * not in j1 (ie j2 has default values).
 *
 * If skip_attributes is given, then these fields will not be merged from j2
 * even if they're missing in j1.
 * If overwrite_attributes is given, then these fields will always be merged
 * from j2 even if they have values in j1 and even if they're missing in j1
 */
std::string merge_objects(const std::string &j1, const std::string &j2,
                          const std::set<std::string> &skip_attributes,
                          const std::set<std::string> &overwrite_attributes);

std::optional<std::string> merge_objects(
    std::optional<std::string> j1, std::optional<std::string> j2,
    const std::set<std::string> &skip_attributes,
    const std::set<std::string> &overwrite_attributes);

}  // namespace json
}  // namespace helper

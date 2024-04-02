/*
  Copyright (c) 2022, 2024, Oracle and/or its affiliates.

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

#ifndef ROUTER_SRC_REST_MRS_SRC_HELPER_JSON_TO_STRING_H_
#define ROUTER_SRC_REST_MRS_SRC_HELPER_JSON_TO_STRING_H_

#include <map>
#include <string>

#include "helper/json/rapid_json_to_text.h"

namespace helper {
namespace json {

/**
 * std::map that represents simple JSON object.
 *
 * This type can hold only JSON Objects that consist only
 * from string values, other types are going to be ignored
 * or converted.
 */
using MapObject = std::map<std::string, std::string>;

std::string to_string(const MapObject &map);

template <typename RapidJson>
std::string to_string(const RapidJson &v) {
  std::string out_result;
  rapid_json_to_text(v, out_result);
  return out_result;
}

}  // namespace json
}  // namespace helper

#endif  // ROUTER_SRC_REST_MRS_SRC_HELPER_JSON_TO_STRING_H_

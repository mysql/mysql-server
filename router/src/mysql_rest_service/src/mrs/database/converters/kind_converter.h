/*
  Copyright (c) 2024, 2026, Oracle and/or its affiliates.

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

#ifndef ROUTER_SRC_MYSQL_REST_SERVICE_SRC_MRS_DATABASE_CONVERTERS_KIND_CONVERTER_H_
#define ROUTER_SRC_MYSQL_REST_SERVICE_SRC_MRS_DATABASE_CONVERTERS_KIND_CONVERTER_H_

#include <map>
#include <stdexcept>
#include <string>

#include "mrs/database/entry/object.h"
#include "mysql/harness/string_utils.h"

namespace mrs {
namespace database {

class KindTypeConverter {
 public:
  using KindType = entry::KindType;

  void operator()(entry::KindType *out, const char *value) const {
    const static std::map<std::string, KindType> converter{
        {"PARAMETERS", KindType::PARAMETERS}, {"RESULT", KindType::RESULT}};

    if (!value) value = "";
    auto result = mysql_harness::make_upper(value);
    try {
      *out = converter.at(result);
    } catch (const std::exception &) {
      using namespace std::string_literals;
      throw std::runtime_error("Invalid value for Kind: "s + result);
    }
  }
};

}  // namespace database
}  // namespace mrs

#endif /* ROUTER_SRC_MYSQL_REST_SERVICE_SRC_MRS_DATABASE_CONVERTERS_KIND_CONVERTER_H_ \
        */

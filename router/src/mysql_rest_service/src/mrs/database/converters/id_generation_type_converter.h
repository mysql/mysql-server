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

#ifndef ROUTER_SRC_MYSQL_REST_SERVICE_SRC_MRS_DATABASE_CONVERTERS_ID_GENERATION_TYPE_CONVERTER_H_
#define ROUTER_SRC_MYSQL_REST_SERVICE_SRC_MRS_DATABASE_CONVERTERS_ID_GENERATION_TYPE_CONVERTER_H_

#include <stdexcept>
#include <string>

#include "mrs/database/converters/generic.h"
#include "mrs/database/entry/object.h"

namespace mrs {
namespace database {

class IdGenerationTypeConverter {
 public:
  void operator()(entry::IdGenerationType *out, const char *value) const {
    if (nullptr == value) {
      *out = entry::IdGenerationType::NONE;
      return;
    }

    if (mrs_strcasecmp(value, "auto_inc") == 0) {
      *out = entry::IdGenerationType::AUTO_INCREMENT;
    } else if (mrs_strcasecmp(value, "rev_uuid") == 0) {
      *out = entry::IdGenerationType::REVERSE_UUID;
    } else if (mrs_strcasecmp(value, "null") == 0) {
      *out = entry::IdGenerationType::NONE;
    } else {
      using namespace std::string_literals;
      throw std::runtime_error("Invalid value for IdGeneration: "s + value);
    }
  }
};

}  // namespace database
}  // namespace mrs

#endif /* ROUTER_SRC_MYSQL_REST_SERVICE_SRC_MRS_DATABASE_CONVERTERS_ID_GENERATION_TYPE_CONVERTER_H_ \
        */

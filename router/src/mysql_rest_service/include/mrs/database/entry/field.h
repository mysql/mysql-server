/*
  Copyright (c) 2021, 2024, Oracle and/or its affiliates.

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

#ifndef ROUTER_SRC_REST_MRS_SRC_MRS_DATABASE_ENTRY_FIELD_H_
#define ROUTER_SRC_REST_MRS_SRC_MRS_DATABASE_ENTRY_FIELD_H_

#include <optional>
#include <string>
#include <vector>

#include "mrs/database/entry/entry.h"
#include "mrs/database/entry/set_operation.h"
#include "mrs/database/entry/universal_id.h"

namespace mrs {
namespace database {
namespace entry {

struct Field {
  enum DataType {
    typeString,
    typeInt,
    typeDouble,
    typeBoolean,
    typeLong,
    typeTimestamp
  };
  enum Mode {
    modeIn,
    modeOut,
    modeInOut,
  };

  UniversalId id;
  std::string name;
  Mode mode;
  std::string bind_name;
  DataType data_type;
  std::string raw_data_type;
};

struct ResultObject {
  std::vector<Field> fields;
  std::string name;
  UniversalId id;
};

struct ResultSets {
  ResultObject input_parameters;
  std::vector<ResultObject> results;
};

}  // namespace entry
}  // namespace database
}  // namespace mrs

#endif  // ROUTER_SRC_REST_MRS_SRC_MRS_DATABASE_ENTRY_FIELD_H_

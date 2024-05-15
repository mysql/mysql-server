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

#ifndef ROUTER_SRC_REST_MRS_SRC_MRS_DATABASE_ENTRY_DB_OBJECT_H_
#define ROUTER_SRC_REST_MRS_SRC_MRS_DATABASE_ENTRY_DB_OBJECT_H_

#include <optional>
#include <string>
#include <vector>

#include "mrs/database/entry/entry.h"
#include "mrs/database/entry/field.h"
#include "mrs/database/entry/row_group_ownership.h"
#include "mrs/database/entry/row_user_ownership.h"
#include "mrs/database/entry/set_operation.h"
#include "mrs/database/entry/universal_id.h"

namespace mrs {
namespace database {
namespace entry {

class DbObject {
 public:
  enum Format : uint32_t { formatFeed = 1, formatItem = 2, formatMedia = 3 };
  enum PathType { typeTable, typeProcedure, typeFunction };
  EntryKey get_key() const { return {key_rest, id}; }

  UniversalId id;
  UniversalId service_id;
  UniversalId schema_id;
  std::string host;
  std::string host_alias;
  bool active_service;
  bool active_schema;
  bool active_object;
  std::string service_path;
  std::string schema_path;
  std::string object_path;
  uint64_t on_page;
  std::string db_schema;
  std::string db_table;
  bool requires_authentication;
  bool schema_requires_authentication;
  Operation::ValueType operation;
  Format format;
  std::optional<std::string> media_type;
  bool autodetect_media_type;
  bool deleted;
  PathType type;
  RowUserOwnership row_security;
  std::vector<RowGroupOwnership> row_group_security;
  std::string options_json;
  std::string options_json_schema;
  std::string options_json_service;
  ResultSets fields;
};

}  // namespace entry
}  // namespace database
}  // namespace mrs

#endif  // ROUTER_SRC_REST_MRS_SRC_MRS_DATABASE_ENTRY_DB_OBJECT_H_

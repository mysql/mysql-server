/*
  Copyright (c) 2021, 2026, Oracle and/or its affiliates.

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

#ifndef ROUTER_SRC_REST_MRS_SRC_MRS_DATABASE_ENTRY_DB_OBJECT_H_
#define ROUTER_SRC_REST_MRS_SRC_MRS_DATABASE_ENTRY_DB_OBJECT_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "mrs/database/entry/entry.h"
#include "mrs/database/entry/field.h"
#include "mrs/database/entry/object.h"
#include "mrs/database/entry/row_group_ownership.h"
#include "mrs/database/entry/row_user_ownership.h"
#include "mrs/database/entry/set_operation.h"
#include "mrs/database/entry/universal_id.h"

namespace mrs {
namespace database {
namespace entry {

struct ContentSetHasObjectDef {
  UniversalId content_set_id;
  // kind
  uint64_t priority;
  std::string language;
  std::string class_name;
  std::string name;
  std::optional<std::string> options;
};

struct DbObject {
  enum ObjectType {
    k_objectTypeTable,
    k_objectTypeProcedure,
    k_objectTypeFunction,
    k_objectTypeScript
  };
  enum Format : uint32_t { formatFeed = 1, formatItem = 2, formatMedia = 3 };

  UniversalId id;
  UniversalId schema_id;
  std::string name;
  std::string schema_name;
  std::string request_path;
  EnabledType enabled;
  ObjectType type;
  Operation::ValueType crud_operation;
  Format format;
  std::optional<uint64_t> items_per_page;
  std::optional<std::string> media_type;
  bool autodetect_media_type;
  bool requires_authentication;
  std::optional<std::string> auth_stored_procedure;
  std::optional<std::string> options;
  std::optional<std::string> metadata;

  std::vector<RowGroupOwnership> row_group_security;
  ResultSets fields;
  std::shared_ptr<Object> object_description;
  std::optional<ContentSetHasObjectDef> content_set_def;

  bool deleted{false};
};

}  // namespace entry
}  // namespace database
}  // namespace mrs

#endif  // ROUTER_SRC_REST_MRS_SRC_MRS_DATABASE_ENTRY_DB_OBJECT_H_

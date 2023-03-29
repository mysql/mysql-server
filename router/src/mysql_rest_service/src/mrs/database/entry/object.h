/*
  Copyright (c) 2022, 2023, Oracle and/or its affiliates.

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

#ifndef ROUTER_SRC_MYSQL_REST_SERVICE_SRC_MRS_DATABASE_ENTRY_OBJECT_H_
#define ROUTER_SRC_MYSQL_REST_SERVICE_SRC_MRS_DATABASE_ENTRY_OBJECT_H_

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "mrs/database/entry/entry.h"
#include "mrs/database/entry/set_operation.h"
#include "mrs/database/entry/universal_id.h"

namespace mrs {
namespace database {
namespace entry {

class ObjectField {
 public:
  using ColumnMapping = std::vector<std::pair<std::string, std::string>>;

  struct Reference {
    entry::UniversalId id;
    std::string schema_name;
    std::string object_name;
    ColumnMapping column_mapping;
    std::optional<entry::UniversalId> reduce_to_field_id;
    bool to_many;
    bool unnest;
    Operation::ValueType crud_operations;

    std::vector<std::shared_ptr<ObjectField>> fields;

    std::string table_alias;

    const ObjectField &reduced_to_field() const {
      if (!reduce_to_field_id) throw std::logic_error("invalid access");

      for (const auto &f : fields) {
        if (f->id == *reduce_to_field_id) return *f;
      }

      throw std::logic_error("bad metadata");
    }
  };

  entry::UniversalId id;
  std::optional<entry::UniversalId> parent_reference_id;
  std::string name;
  std::string db_name;
  bool enabled;
  bool allow_filtering;

  std::optional<Reference> reference;
};

class Object {
 public:
  std::string schema;
  std::string schema_object;

  std::vector<std::shared_ptr<ObjectField>> fields;
};

}  // namespace entry
}  // namespace database
}  // namespace mrs

#endif  // ROUTER_SRC_MYSQL_REST_SERVICE_SRC_MRS_DATABASE_ENTRY_OBJECT_H_

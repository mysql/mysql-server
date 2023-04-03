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

#include <map>
#include <memory>
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

class FieldSource {
 public:
  virtual ~FieldSource() = default;

  std::string schema;
  std::string table;

  std::string table_alias;

  Operation::ValueType crud_operations;

  inline bool create_allowed() const {
    return crud_operations & Operation::Values::valueCreate;
  }

  inline bool read_allowed() const {
    return crud_operations & Operation::Values::valueRead;
  }

  inline bool update_allowed() const {
    return crud_operations & Operation::Values::valueUpdate;
  }

  inline bool delete_allowed() const {
    return crud_operations & Operation::Values::valueDelete;
  }

  inline std::string table_key() const { return schema + "." + table; }
};

// the root table where all the joins and sub-selects start
class BaseTable : public FieldSource {
 public:
};

class ObjectField;

// tables that are joined to the root table or others
class JoinedTable : public FieldSource {
 public:
  using ColumnMapping = std::vector<std::pair<std::string, std::string>>;

  std::shared_ptr<entry::ObjectField> reduce_to_field;

  ColumnMapping column_mapping;
  bool to_many = false;
  bool unnest = false;
};

class Object;

class ObjectField {
 public:
  std::string name;
  int position;
  std::string db_name;
  std::string db_datatype;
  bool db_auto_inc = false;
  bool db_not_null = false;
  bool db_is_primary = false;
  bool db_is_unique = false;
  bool db_is_generated = false;
  bool enabled = true;
  bool allow_filtering = true;
  bool no_check = false;

  std::shared_ptr<FieldSource> source;
  std::shared_ptr<Object> nested_object;
};

class Object {
 public:
  std::string name;
  std::weak_ptr<Object> parent;

  // if more than 1 table, they're all to be joined together
  std::vector<std::shared_ptr<FieldSource>> base_tables;
  std::vector<std::shared_ptr<ObjectField>> fields;
};

}  // namespace entry
}  // namespace database
}  // namespace mrs

#endif  // ROUTER_SRC_MYSQL_REST_SERVICE_SRC_MRS_DATABASE_ENTRY_OBJECT_H_

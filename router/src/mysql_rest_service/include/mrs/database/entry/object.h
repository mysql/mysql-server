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

class Table;

enum class IdGenerationType {
  NONE,            // not auto-generated
  AUTO_INCREMENT,  // auto-increment by mysql
  REVERSE_UUID     // pre-generate as UUID_TO_BIN(UUID(), 1)
};

enum class ColumnType {
  UNKNOWN,
  INTEGER,
  DOUBLE,
  BOOLEAN,
  STRING,
  BINARY,
  GEOMETRY,
  JSON
};

enum class KindType { PARAMETERS, RESULT };

enum class ModeType { kNONE, kIN, kOUT, kIN_OUT };

class Column {
 public:
  std::weak_ptr<Table> table;
  std::string name;
  std::string datatype;
  ColumnType type = ColumnType::UNKNOWN;
  IdGenerationType id_generation = IdGenerationType::NONE;
  bool not_null = false;
  bool is_primary = false;
  bool is_unique = false;
  bool is_generated = false;
  bool is_foreign = false;
  uint32_t srid{0};

  bool is_auto_generated_id() const {
    return id_generation != IdGenerationType::NONE;
  }
};

class Table {
 public:
  virtual ~Table() = default;

  std::string schema;
  std::string table;

  std::string table_alias;

  std::vector<std::shared_ptr<Column>> columns;

  Operation::ValueType crud_operations;

  inline bool create_allowed() const {
    return crud_operations & Operation::Values::valueCreate;
  }

  inline bool update_allowed() const {
    return crud_operations & Operation::Values::valueUpdate;
  }

  inline bool delete_allowed() const {
    return crud_operations & Operation::Values::valueDelete;
  }

  inline std::string table_key() const { return schema + "." + table; }

  std::shared_ptr<Column> get_column(const std::string &name) const {
    for (const auto &c : columns) {
      if (c->name == name) return c;
    }
    return {};
  }

  std::vector<std::shared_ptr<Column>> primary_key() const {
    std::vector<std::shared_ptr<Column>> cols;
    for (const auto &c : columns)
      if (c->is_primary) cols.push_back(c);
    return cols;
  }
};

// the root table where all the joins and sub-selects start
class BaseTable : public Table {
 public:
};

class ObjectField;

// tables that are joined to the root table or others
class JoinedTable : public Table {
 public:
  using ColumnMapping =
      std::vector<std::pair<std::shared_ptr<Column>, std::shared_ptr<Column>>>;

  ColumnMapping column_mapping;
  bool to_many = false;
  bool unnest = false;

  bool enabled = true;  // is field that references the table is enabled
};

class Object;

class ObjectField {
 public:
  ObjectField() = default;
  virtual ~ObjectField() = default;

  ObjectField &operator=(const ObjectField &) = default;

  std::string name;
  int position = 0;
  bool enabled = true;  // include in the returned JSON object
  bool allow_filtering = true;
  bool allow_sorting = true;
  bool no_check = false;   // exclude from ETag checksum calculation
  bool no_update = false;  // disallow updates to this field
};

class DataField : public ObjectField {
 public:
  DataField() = default;
  DataField(const DataField &f) = default;

  DataField &operator=(const DataField &) = default;

  std::shared_ptr<Column> source;
  bool unnested_array = false;
};

class ParameterField : public DataField {
 public:
  ParameterField() = default;
  ParameterField(const ParameterField &f) = default;

  ParameterField &operator=(const ParameterField &) = default;

  ModeType mode{ModeType::kNONE};
};

class Object {
 public:
  std::string name;
  KindType kind;

  // if more than 1 table, they're all to be joined together
  std::vector<std::shared_ptr<Table>> base_tables;
  std::vector<std::shared_ptr<ObjectField>> fields;

  // used to determine if object can be updated
  bool unnests_to_value = false;

  inline std::shared_ptr<ObjectField> get_field(std::string_view name) const {
    for (const auto &f : fields) {
      if (f->name == name) return f;
    }
    return {};
  }

  inline std::shared_ptr<DataField> get_column_field(
      std::string_view column_name) const {
    for (const auto &f : fields) {
      if (auto df = std::dynamic_pointer_cast<DataField>(f); df) {
        if (df->source->name == column_name) return df;
      }
    }
    return {};
  }

  inline std::shared_ptr<BaseTable> get_base_table() const {
    return std::dynamic_pointer_cast<BaseTable>(base_tables.front());
  }
};

class ReferenceField : public ObjectField {
 public:
  std::shared_ptr<Object> nested_object;

  std::shared_ptr<JoinedTable> ref_table() const {
    return std::dynamic_pointer_cast<JoinedTable>(
        nested_object->base_tables.front());
  }

  bool is_array() const { return ref_table()->to_many; }
};

}  // namespace entry
}  // namespace database
}  // namespace mrs

#endif  // ROUTER_SRC_MYSQL_REST_SERVICE_SRC_MRS_DATABASE_ENTRY_OBJECT_H_

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

#include <functional>
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

class ObjectField;
class Column;
class Object;

class OwnerUserField {
 public:
  UniversalId uid;
  std::shared_ptr<Column> field;
};

class ObjectField {
 public:
  virtual ~ObjectField() = default;

  ObjectField(const ObjectField &) = default;
  ObjectField &operator=(const ObjectField &) = default;

  entry::UniversalId id;
  std::string name;
  int position = 0;
  bool enabled = true;  // include in the returned JSON object
  bool allow_filtering = true;
  bool allow_sorting = true;

 protected:
  ObjectField() = default;
};

class Column : public ObjectField {
 public:
  Column() = default;
  Column(const Column &f) = default;

  Column &operator=(const Column &) = default;

  std::string column_name;
  std::string datatype;
  ColumnType type = ColumnType::UNKNOWN;
  IdGenerationType id_generation = IdGenerationType::NONE;
  bool not_null = false;
  bool is_primary = false;
  bool is_unique = false;
  bool is_generated = false;
  bool is_foreign = false;
  bool is_row_owner = false;
  std::optional<bool> with_check;
  std::optional<bool> with_update;
  uint32_t srid{0};

  bool is_auto_generated_id() const {
    return is_primary && id_generation != IdGenerationType::NONE;
  }
};

// tables that are joined to the root table or others
class ForeignKeyReference : public ObjectField {
 public:
  ForeignKeyReference() = default;
  ForeignKeyReference(const ForeignKeyReference &) = default;

  ForeignKeyReference &operator=(const ForeignKeyReference &) = default;

  using ColumnMapping = std::vector<std::pair<std::string, std::string>>;

  std::shared_ptr<Table> ref_table;
  ColumnMapping column_mapping;
  bool to_many = false;
  bool unnest = false;
};

class ParameterField : public Column {
 public:
  ParameterField() = default;
  ParameterField(const ParameterField &f) = default;

  ParameterField &operator=(const ParameterField &) = default;

  ModeType mode{ModeType::kNONE};
};

class Table {
 public:
  virtual ~Table() = default;

  std::string schema;
  std::string table;

  std::string table_alias;

  std::vector<std::shared_ptr<ObjectField>> fields;
  std::optional<OwnerUserField> user_ownership_field;

  Operation::ValueType crud_operations;
  bool with_check_ = true;

  inline bool with_insert() const {
    return crud_operations & Operation::Values::valueCreate;
  }

  inline bool with_update() const {
    return crud_operations & Operation::Values::valueUpdate;
  }

  inline bool with_update(const Column &column) const {
    if (column.with_update.has_value()) return column.with_update.value();

    return crud_operations & Operation::Values::valueUpdate;
  }

  inline bool with_update_any_column() const {
    if (with_update()) return true;

    bool updatable = false;

    foreach_field<Column, bool>([&updatable](const Column &column) {
      if (column.with_update.value_or(false)) {
        updatable = true;
        return false;
      }
      return true;
    });

    return updatable;
  }

  inline bool with_delete() const {
    return crud_operations & Operation::Values::valueDelete;
  }

  inline bool with_check(const Column &column) const {
    if (column.with_check.has_value()) return column.with_check.value();
    // PKs always default to being checked and ignore table level CHECK
    if (column.is_primary) return true;

    return with_check_;
  }

  bool needs_etag() const;

  // used to determine if object can be updated
  bool unnests_to_value = false;

  inline std::shared_ptr<ObjectField> get_field(std::string_view name) const {
    for (const auto &f : fields) {
      if (f->name == name) return f;
    }
    return {};
  }

  inline std::shared_ptr<ObjectField> get_field_or_throw(
      std::string_view name) const {
    for (const auto &f : fields) {
      if (f->name == name) return f;
    }
    throw std::invalid_argument("Invalid object field reference " +
                                std::string(name));
  }

  inline std::shared_ptr<Column> get_column(
      const entry::UniversalId &id) const {
    for (const auto &f : fields) {
      if (auto df = std::dynamic_pointer_cast<Column>(f); df) {
        if (df->id == id) return df;
      }
    }
    return {};
  }

  inline std::shared_ptr<Column> get_column(
      std::string_view column_name) const {
    for (const auto &f : fields) {
      if (auto df = std::dynamic_pointer_cast<Column>(f); df) {
        if (df->column_name == column_name) return df;
      }
    }
    return {};
  }

  inline std::shared_ptr<Column> get_column_or_throw(
      std::string_view column_name) const {
    for (const auto &f : fields) {
      if (auto df = std::dynamic_pointer_cast<Column>(f); df) {
        if (df->column_name == column_name) return df;
      }
    }
    throw std::invalid_argument("Invalid column reference " +
                                std::string(column_name));
  }

  inline std::string table_key() const { return schema + "." + table; }

  std::shared_ptr<Column> get_column_with_field_name(
      const std::string &name) const {
    for (const auto &c : fields) {
      if (c->name == name) return std::dynamic_pointer_cast<Column>(c);
    }
    return {};
  }

  std::vector<const Column *> primary_key() const {
    std::vector<const Column *> cols;
    foreach_field<Column, bool>([&cols](const Column &column) {
      if (column.is_primary) cols.push_back(&column);
      return false;
    });
    return cols;
  }

  const Column *try_get_generated_id_column() const {
    return foreach_field<Column, const Column *>(
        [](const Column &column) -> const Column * {
          if (column.is_auto_generated_id()) return &column;
          return nullptr;
        });
  }

  const Column *try_get_row_ownership_column() const {
    return foreach_field<Column, const Column *>(
        [](const Column &column) -> const Column * {
          if (column.is_row_owner) return &column;
          return nullptr;
        });
  }

  template <typename T, typename R>
  R foreach_field(std::function<R(T &)> fn) const {
    for (auto &field : fields) {
      if (auto df = std::dynamic_pointer_cast<T>(field)) {
        if (auto r = fn(*df)) {
          return r;
        }
      }
    }
    return R();
  }

  template <typename T, typename R>
  R foreach_field(std::function<R(const T &)> fn) const {
    for (const auto &field : fields) {
      if (auto df = std::dynamic_pointer_cast<T>(field)) {
        if (auto r = fn(*df)) {
          return r;
        }
      }
    }
    return R();
  }

  template <typename R>
  R foreach_field(std::function<R(const Column &)> column_fn,
                  std::function<R(const ForeignKeyReference &)> fk_fn) const {
    for (const auto &field : fields) {
      if (auto df = std::dynamic_pointer_cast<Column>(field)) {
        if (auto r = column_fn(*df)) {
          return r;
        }
      } else if (auto df =
                     std::dynamic_pointer_cast<ForeignKeyReference>(field)) {
        if (auto r = fk_fn(*df)) {
          return r;
        }
      }
    }
    return R();
  }

  const ForeignKeyReference &get_reference_to_parent(
      const Table &parent) const {
    auto fk =
        parent.foreach_field<ForeignKeyReference, const ForeignKeyReference *>(
            [this](const ForeignKeyReference &ref) {
              if (ref.to_many && ref.ref_table.get() == this) {
                return &ref;
              }
              return static_cast<const ForeignKeyReference *>(nullptr);
            });

    if (!fk) throw std::logic_error("FK to parent not found");

    return *fk;
  }

  bool is_editable(bool &has_unnested_1n) const;

 protected:
  mutable std::optional<bool> needs_etag_;
  std::string as_graphql(int depth, bool extended) const;
};

class Object : public Table {
 public:
  std::string name;
  KindType kind;

  bool is_read_only() const;

  std::string as_graphql(bool extended = false) const;
};

using DualityView = Object;

}  // namespace entry
}  // namespace database
}  // namespace mrs

#endif  // ROUTER_SRC_MYSQL_REST_SERVICE_SRC_MRS_DATABASE_ENTRY_OBJECT_H_

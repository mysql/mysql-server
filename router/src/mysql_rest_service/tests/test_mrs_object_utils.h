/*
  Copyright (c) 2023, 2024, Oracle and/or its affiliates.

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

#ifndef ROUTER_SRC_MYSQL_REST_SERVICE_TESTS_TEST_MRS_OBJECT_UTILS_H_
#define ROUTER_SRC_MYSQL_REST_SERVICE_TESTS_TEST_MRS_OBJECT_UTILS_H_

#ifdef RAPIDJSON_NO_SIZETYPEDEFINE
#include "my_rapidjson_size_t.h"
#endif

#include <rapidjson/prettywriter.h>
#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>
#include "helper/json/text_to.h"
#include "mrs/database/entry/object.h"
#include "mrs/database/query_entries_object.h"
#include "mrs/interface/rest_error.h"
#include "mysqlrouter/mysql_session.h"

using mrs::database::entry::BaseTable;
using mrs::database::entry::Column;
using mrs::database::entry::DataField;
using mrs::database::entry::IdGenerationType;
using mrs::database::entry::JoinedTable;
using mrs::database::entry::Object;
using mrs::database::entry::ObjectField;
using mrs::database::entry::ReferenceField;
using mrs::database::entry::Table;

using mrs::database::entry::Operation;

namespace FieldFlag {
constexpr const int NOCHECK = 1 << 0;
// constexpr const int NOUPDATE = 1 << 1;

constexpr const int PRIMARY = 1 << 4;
constexpr const int UNIQUE = 1 << 5;
constexpr const int AUTO_INC = 1 << 6;
constexpr const int REV_UUID = 1 << 7;
constexpr const int NOFILTER = 1 << 8;
constexpr const int SORTABLE = 1 << 9;
constexpr const int DISABLED = 1 << 10;
}  // namespace FieldFlag

constexpr const Operation::ValueType kAllOperations =
    Operation::Values::valueRead | Operation::Values::valueCreate |
    Operation::Values::valueUpdate | Operation::Values::valueDelete;

constexpr const Operation::ValueType kNoCreate =
    Operation::Values::valueRead | Operation::Values::valueUpdate |
    Operation::Values::valueDelete;

constexpr const Operation::ValueType kNoUpdate =
    Operation::Values::valueRead | Operation::Values::valueCreate |
    Operation::Values::valueDelete;

constexpr const Operation::ValueType kNoDelete =
    Operation::Values::valueRead | Operation::Values::valueCreate |
    Operation::Values::valueUpdate;

class DatabaseQueryTest : public testing::Test {
 public:
  std::unique_ptr<mysqlrouter::MySQLSession> m_;

  void SetUp() override { m_ = std::make_unique<mysqlrouter::MySQLSession>(); }

  void TearDown() override {}
};

class ObjectBuilder {
 public:
  ObjectBuilder(const std::string &schema, const std::string &table,
                Operation::ValueType allowed_crud = kAllOperations) {
    m_object = std::make_shared<Object>();
    m_object->name = table;
    m_table = std::make_shared<BaseTable>();
    m_table->schema = schema;
    m_table->table = table;
    m_table->table_alias = "t";
    m_table->crud_operations = allowed_crud | Operation::Values::valueRead;

    m_object->base_tables.push_back(m_table);

    m_serial = std::make_shared<int>(0);
  }

  ObjectBuilder(const std::string &table,
                const std::vector<std::pair<std::string, std::string>> &mapping,
                Operation::ValueType allowed_crud = kAllOperations) {
    m_object = std::make_shared<Object>();
    m_object->name = table;
    auto join = std::make_shared<JoinedTable>();
    join->table = table;
    join->crud_operations = allowed_crud | Operation::Values::valueRead;
    m_column_mapping = mapping;
    m_table = join;

    m_object->base_tables.push_back(m_table);
  }

  ObjectBuilder &field(const std::string &name, int flags = 0) {
    return field(name, name, "", flags);
  }

  ObjectBuilder &column(const std::string &name, int flags = 0) {
    return field(name, name, "", flags | FieldFlag::DISABLED);
  }

  ObjectBuilder &column(const std::string &name, const std::string &db_type,
                        int flags = 0) {
    return field(name, name, db_type, flags | FieldFlag::DISABLED);
  }

  ObjectBuilder &field(const std::string &name, const std::string &db_name,
                       const std::string &db_type = "", int flags = 0) {
    auto field = std::make_shared<DataField>();
    field->name = name;
    field->enabled = (flags & FieldFlag::DISABLED) == 0;
    field->allow_filtering = (flags & FieldFlag::NOFILTER) == 0;
    field->allow_sorting = (flags & FieldFlag::SORTABLE) != 0;
    field->no_check = (flags & FieldFlag::NOCHECK) != 0;

    field->source = add_column(db_name, db_type, flags);

    m_object->fields.push_back(field);

    return *this;
  }

  ObjectBuilder &nest(const std::string &name, const ObjectBuilder &join,
                      int flags = 0) {
    assert(std::dynamic_pointer_cast<JoinedTable>(join.m_table));

    nest_join(name, join, flags, false);

    return *this;
  }

  ObjectBuilder &nest_list(const std::string &name, const ObjectBuilder &join,
                           int flags = 0) {
    assert(std::dynamic_pointer_cast<JoinedTable>(join.m_table));

    nest_join(name, join, flags, true);

    return *this;
  }

  ObjectBuilder &nest_unnested_list(const std::string &name,
                                    const ObjectBuilder &join) {
    assert(std::dynamic_pointer_cast<JoinedTable>(join.m_table));

    auto j = nest_join(name, join, 0, true);
    j->unnest = true;
    return *this;
  }

  ObjectBuilder &unnest(const ObjectBuilder &join) {
    assert(std::dynamic_pointer_cast<JoinedTable>(join.m_table));

    unnest_join(join, false);
    return *this;
  }

  ObjectBuilder &unnest_list(const ObjectBuilder &join) {
    assert(std::dynamic_pointer_cast<JoinedTable>(join.m_table));

    unnest_join(join, true);
    return *this;
  }

  ObjectBuilder &ref(std::shared_ptr<Table> *r) {
    *r = m_table;
    return *this;
  }

  ObjectBuilder &ref(std::shared_ptr<Object> *r) {
    *r = m_object;
    return *this;
  }

  std::shared_ptr<Object> root() const { return m_object; }

  operator std::shared_ptr<Object>() const { return m_object; }

#if 0
  void dump(int depth = 0) const {
    auto fmtbase = [](std::shared_ptr<Object> object) {
      std::string r;
      for (auto t : object->base_tables) {
        r.append(t->table).append(", ");
      }
      r.pop_back();
      r.pop_back();
      return "[" + r + "]";
    };

    std::cout << std::string(depth * 2, ' ') << object->name << " <- "
              << (!object->parent.expired() ? object->parent.lock()->name : " ")
              << " base=" << fmtbase(object) << "\n";
    auto fmt_source = [](std::shared_ptr<FieldSource> source) {
      if (source) {
        if (auto join = std::dynamic_pointer_cast<JoinedTable>(source); join)
          return " [join=" + join->table + " " + join->table_alias +
                 " to_many=" + (join->to_many ? "1" : "0") +
                 " unnest=" + (join->unnest ? "1" : "0") + "]";
        else
          return " [base=" + source->table + " " + source->table_alias + "]";
      }
      return std::string("[]");
    };

    for (const auto &f : object->fields) {
      std::cout << std::string(depth * 2, ' ')
                << (f->nested_object ? "  = " : "  - ") << f->name << "\t"
                << " col=" << f->db_name << fmt_source(f->source)
                << "  type=" << f->db_datatype << " nn=" << f->db_not_null
                << " pri=" << f->db_is_primary << " gen=" << f->db_is_generated
                << " enabled=" << f->enabled << " filt=" << f->allow_filtering
                << "\n";
      if (f->nested_object) {
        dump_object(f->nested_object, depth + 1);
      }
    }
  }
#endif

 protected:
  std::shared_ptr<Object> m_object;
  std::shared_ptr<Table> m_table;
  std::vector<std::pair<std::string, std::string>> m_column_mapping;
  std::shared_ptr<int> m_serial;

  std::shared_ptr<JoinedTable> nest_join(const std::string &name,
                                         const ObjectBuilder &join, int flags,
                                         bool to_many) {
    if (m_serial) fix_recursive(join.m_object);

    auto j = std::dynamic_pointer_cast<JoinedTable>(join.m_table);
    j->unnest = false;
    j->to_many = to_many;
    j->column_mapping = resolve_column_mapping(join, to_many);

    auto field = std::make_shared<ReferenceField>();
    field->name = name;
    field->no_check = (flags & FieldFlag::NOCHECK) != 0;
    field->enabled = (flags & FieldFlag::DISABLED) == 0;
    field->allow_filtering = (flags & FieldFlag::NOFILTER) == 0;

    field->nested_object = join.m_object;
    m_object->fields.push_back(field);

    return j;
  }

  std::shared_ptr<JoinedTable> unnest_join(const ObjectBuilder &join,
                                           bool to_many) {
    if (m_serial) fix_recursive(join.m_object);

    auto j = std::dynamic_pointer_cast<JoinedTable>(join.m_table);
    j->unnest = true;
    j->to_many = to_many;
    j->column_mapping = resolve_column_mapping(join, to_many);

    for (auto t : join.m_object->base_tables) {
      m_object->base_tables.push_back(t);
    }
    for (auto f : join.m_object->fields) {
      m_object->fields.push_back(f);
    }

    return j;
  }

  JoinedTable::ColumnMapping resolve_column_mapping(const ObjectBuilder &join,
                                                    bool to_many) {
    JoinedTable::ColumnMapping mapping;

    // to_many = 1:n, so if we have tbl1 - (1:n) - tbl2, the FK is at tbl2, so
    // the base table is tbl2
    // in a n:m relationship, the joiner table is the base table

    for (const auto &c : join.m_column_mapping) {
      auto base_table = (to_many ? join.m_table : m_table);
      auto ref_table = (to_many ? m_table : join.m_table);
      auto base_col = base_table->get_column(c.first);
      auto ref_col = ref_table->get_column(c.second);

      if (!base_col)
        throw std::logic_error(c.first + " not found in " + base_table->table);
      if (!ref_col)
        throw std::logic_error(c.second + " not found in " + ref_table->table);

      base_col->is_foreign = true;

      mapping.emplace_back(base_col, ref_col);
    }

    return mapping;
  }

  void fix_recursive(std::shared_ptr<Object> o) {
    for (const auto &table : o->base_tables) {
      if (table->table_alias.empty()) {
        table->table_alias = "t" + std::to_string(++*m_serial);
      }
      if (table->schema.empty()) {
        table->schema = m_table->schema;
      }
    }
    for (const auto &f : o->fields) {
      if (auto rf = std::dynamic_pointer_cast<ReferenceField>(f); rf) {
        fix_recursive(rf->nested_object);
      } else if (auto df = std::dynamic_pointer_cast<DataField>(f); df) {
        auto table = df->source->table.lock();
        if (table->table_alias.empty()) {
          table->table_alias = "t" + std::to_string(++*m_serial);
        }
        if (table->schema.empty()) {
          table->schema = m_table->schema;
        }
      }
    }
  }

  std::shared_ptr<Column> add_column(const std::string &name,
                                     const std::string &type, int flags) {
    auto column = std::make_shared<Column>();
    column->table = m_table;
    column->name = name;
    column->datatype = type;
    if (!type.empty())
      column->type = mrs::database::column_datatype_to_type(type);
    column->is_primary = (flags & FieldFlag::PRIMARY) != 0;
    column->is_unique = (flags & FieldFlag::UNIQUE) != 0;
    assert(!((flags & FieldFlag::AUTO_INC) && (flags & FieldFlag::REV_UUID)));
    if (flags & FieldFlag::AUTO_INC)
      column->id_generation =
          mrs::database::entry::IdGenerationType::AUTO_INCREMENT;
    else if (flags & FieldFlag::REV_UUID)
      column->id_generation =
          mrs::database::entry::IdGenerationType::REVERSE_UUID;
    else
      column->id_generation = mrs::database::entry::IdGenerationType::NONE;
    m_table->columns.push_back(column);

    return column;
  }
};

inline rapidjson::Document make_json(const std::string &json) {
  rapidjson::Document doc;
  helper::json::text_to(&doc, json);
  return doc;
}

inline std::string pprint_json(const std::string &json) {
  auto doc = make_json(json);

  rapidjson::StringBuffer json_buf;
  {
    rapidjson::PrettyWriter<rapidjson::StringBuffer> json_writer(json_buf);

    doc.Accept(json_writer);
  }

  return std::string(json_buf.GetString(), json_buf.GetLength());
}

#endif  // ROUTER_SRC_MYSQL_REST_SERVICE_TESTS_TEST_MRS_OBJECT_UTILS_H_

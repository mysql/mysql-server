/*
  Copyright (c) 2023, Oracle and/or its affiliates.

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

#include <memory>
#include <string>
#include <utility>
#include <vector>
#include "mrs/database/entry/object.h"

using mrs::database::entry::BaseTable;
using mrs::database::entry::FieldSource;
using mrs::database::entry::JoinedTable;
using mrs::database::entry::Object;
using mrs::database::entry::ObjectField;

inline std::shared_ptr<BaseTable> make_table(const std::string &schema,
                                             const std::string &table) {
  auto tbl = std::make_shared<BaseTable>();

  tbl->schema = schema;
  tbl->table = table;
  tbl->table_alias = "t";

  return tbl;
}

inline std::shared_ptr<Object> make_object(
    std::shared_ptr<Object> parent,
    std::vector<std::shared_ptr<FieldSource>> tables) {
  auto o = std::make_shared<Object>();
  o->parent = parent;
  o->base_tables = std::move(tables);
  return o;
}

inline void set_reduce_field(std::shared_ptr<JoinedTable> table,
                             const std::string &db_name) {
  auto field = std::make_shared<ObjectField>();
  field->db_name = db_name;
  field->source = table;
  table->reduce_to_field = field;
}

inline std::shared_ptr<JoinedTable> make_join(
    const std::string &schema, const std::string &table, int alias_num,
    const JoinedTable::ColumnMapping &mapping, bool to_many, bool unnest) {
  auto tbl = std::make_shared<JoinedTable>();

  tbl->schema = schema;
  tbl->table = table;
  tbl->table_alias = "t" + std::to_string(alias_num);
  tbl->column_mapping = mapping;
  tbl->to_many = to_many;
  tbl->unnest = unnest;

  return tbl;
}

inline std::shared_ptr<ObjectField> add_field(
    std::shared_ptr<Object> object, std::shared_ptr<FieldSource> source,
    const std::string &name, const std::string &db_name) {
  auto field = std::make_shared<ObjectField>();
  field->source = source;

  field->name = name;
  field->db_name = db_name;

  object->fields.push_back(field);

  return field;
}

inline std::shared_ptr<ObjectField> set_auto_inc(
    std::shared_ptr<ObjectField> field) {
  field->db_auto_inc = true;
  return field;
}

inline std::shared_ptr<ObjectField> set_primary(
    std::shared_ptr<ObjectField> field) {
  field->db_is_primary = true;
  return field;
}

inline std::shared_ptr<ObjectField> add_object_field(
    std::shared_ptr<Object> object, std::shared_ptr<FieldSource> source,
    const std::string &name, std::shared_ptr<Object> nested_object) {
  auto field = std::make_shared<ObjectField>();
  field->source = source;
  field->nested_object = nested_object;

  field->name = name;

  nested_object->parent = object;
  object->fields.push_back(field);

  return field;
}

inline void dump_object(std::shared_ptr<Object> object, int depth = 0) {
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

#endif  // ROUTER_SRC_MYSQL_REST_SERVICE_TESTS_TEST_MRS_OBJECT_UTILS_H_

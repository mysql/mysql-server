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
#include "mrs/database/duality_view/errors.h"
#include "mrs/database/entry/object.h"
#include "mrs/database/query_entry_object.h"
#include "mrs/interface/rest_error.h"
#include "mysql/harness/string_utils.h"
#include "mysqlrouter/mysql_session.h"

using mrs::database::entry::Column;
using mrs::database::entry::ForeignKeyReference;
using mrs::database::entry::IdGenerationType;
using mrs::database::entry::Object;
using mrs::database::entry::ObjectField;
using mrs::database::entry::Table;

using mrs::database::entry::Operation;

using mrs::database::entry::DualityView;

using mrs::database::DualityViewError;
using mrs::database::JSONInputError;

using MySQLError = mysqlrouter::MySQLSession::Error;

namespace FieldFlag {
constexpr const int PRIMARY = 1 << 4;
constexpr const int UNIQUE = 1 << 5;

constexpr const int DISABLED = 1 << 10;

constexpr const int AUTO_INC = 1 << 6;
constexpr const int REV_UUID = 1 << 7;
constexpr const int NOFILTER = 1 << 8;
constexpr const int SORTABLE = 1 << 9;

constexpr const int OWNER = 1 << 11;

constexpr const int WITH_NOCHECK = 1 << 0;
constexpr const int WITH_CHECK = 1 << 1;

constexpr const int WITH_NOUPDATE = 1 << 2;
constexpr const int WITH_FILTERING = 1 << 3;
constexpr const int WITH_SORTING = 1 << 4;
}  // namespace FieldFlag

namespace TableFlag {
constexpr const int WITH_NOINSERT = 0;
constexpr const int WITH_NOUPDATE = 0;
constexpr const int WITH_NODELETE = 0;
constexpr const int WITH_NOCHECK = (1 << 3);

constexpr const int WITH_INSERT = (1 << 0);
constexpr const int WITH_UPDATE = (1 << 1);
constexpr const int WITH_DELETE = (1 << 2);
constexpr const int WITH_CHECK = 0;
}  // namespace TableFlag

class DatabaseQueryTest : public testing::Test {
 public:
  std::unique_ptr<mysqlrouter::MySQLSession> m_;

  void SetUp() override { m_ = std::make_unique<mysqlrouter::MySQLSession>(); }

  void TearDown() override {}
};

class ViewBuilder {
 public:
  explicit ViewBuilder(const std::string &table, int with_flags = 0) {
    m_table = std::make_shared<Table>();
    m_table->table = table;
    apply_with_flags(m_table.get(), with_flags);
  }

  ViewBuilder &field(const std::string &name, const std::string &column_name,
                     int with_flags = 0) {
    auto column = std::make_shared<Column>();

    column->column_name = column_name;
    if (with_flags & FieldFlag::AUTO_INC) {
      column->id_generation = IdGenerationType::AUTO_INCREMENT;
    } else if (with_flags & FieldFlag::REV_UUID) {
      column->id_generation = IdGenerationType::REVERSE_UUID;
    }
    if (with_flags & FieldFlag::WITH_NOCHECK) {
      column->with_check = false;
    } else if (with_flags & FieldFlag::WITH_CHECK) {
      column->with_check = true;
    }
    if (with_flags & FieldFlag::WITH_NOUPDATE) {
      column->with_update = false;
    }
    if (with_flags & FieldFlag::OWNER) {
      m_table->user_ownership_field = {{}, column};
      column->is_row_owner = true;
    }

    column->name = name;
    column->enabled = !(with_flags & FieldFlag::DISABLED);
    column->allow_filtering = (with_flags & FieldFlag::WITH_FILTERING);
    column->allow_sorting = (with_flags & FieldFlag::WITH_SORTING);
    column->is_primary = (with_flags & FieldFlag::PRIMARY);

    m_table->fields.emplace_back(std::move(column));

    return *this;
  }

  ViewBuilder &field(const std::string &name, int with_flags = 0) {
    return field(name, name, with_flags);
  }

  ViewBuilder &field(const std::string &name, const std::string &column_name,
                     const std::string &datatype, int with_flags = 0) {
    field(name, column_name, with_flags);

    auto column = std::dynamic_pointer_cast<Column>(m_table->fields.back());
    column->datatype = datatype;
    column->type = mrs::database::column_datatype_to_type(column->datatype);

    return *this;
  }

  ViewBuilder &column(const std::string &column_name, int with_flags = 0) {
    field(column_name, column_name, with_flags);

    auto column = std::dynamic_pointer_cast<Column>(m_table->fields.back());
    column->enabled = false;

    return *this;
  }

  ViewBuilder &field_to_many(
      const std::string &name, ViewBuilder nested, bool unnest = false,
      const std::vector<std::pair<std::string, std::string>> &fk_mapping = {}) {
    auto fk = std::make_shared<ForeignKeyReference>();
    fk->ref_table = nested.m_table;
    fk->to_many = true;
    fk->unnest = unnest;
    fk->column_mapping = fk_mapping;
    for (const auto &c : fk_mapping) {
      if (!fk->ref_table->get_column(c.first)) {
        auto f = std::make_shared<Column>();
        f->column_name = c.first;
        f->enabled = false;
        fk->ref_table->fields.emplace_back(std::move(f));
      }
    }
    fk->name = name;
    fk->enabled = true;
    m_table->fields.emplace_back(std::move(fk));

    return *this;
  }

  ViewBuilder &field_to_one(
      const std::string &name, ViewBuilder nested, bool unnest = false,
      const std::vector<std::pair<std::string, std::string>> &fk_mapping = {}) {
    auto fk = std::make_shared<ForeignKeyReference>();
    fk->ref_table = nested.m_table;
    fk->to_many = false;
    fk->unnest = unnest;
    fk->column_mapping = fk_mapping;
    for (const auto &c : fk_mapping) {
      if (!m_table->get_column(c.first)) {
        auto f = std::make_shared<Column>();
        f->column_name = c.first;
        f->enabled = false;
        m_table->fields.emplace_back(std::move(f));
      }
    }

    fk->name = name;
    fk->enabled = true;

    m_table->fields.emplace_back(std::move(fk));

    return *this;
  }

  std::shared_ptr<DualityView> resolve(
      mysqlrouter::MySQLSession *session = nullptr, bool auto_column = false) {
    int serial = 0;

    resolve(session, m_table.get(), serial, auto_column);

    return std::static_pointer_cast<DualityView>(m_table);
  }

  std::shared_ptr<DualityView> root() { return resolve(); }

  void resolve_columns(mysqlrouter::MySQLSession *session, Table *table,
                       bool auto_column) {
    auto add_column = [table, auto_column](const std::string &name,
                                           const std::string &type, bool is_pk,
                                           bool is_unique, bool is_autoinc) {
      auto col = table->foreach_field<Column, Column *>(
          [name](Column &column) -> Column * {
            if (name == column.column_name) return &column;
            return nullptr;
          });

      if (!col && is_pk) {
        if (!auto_column) {
          std::cout
              << table->schema << "." << table->table << "." << name
              << " is a primary key in the DB, but is not included/enabled "
                 "in the duality view\n";
          assert(0);
        }
      }
      if (col && is_autoinc &&
          col->id_generation != IdGenerationType::AUTO_INCREMENT) {
        if (auto_column) {
          col->id_generation = IdGenerationType::AUTO_INCREMENT;
        } else {
          std::cout << table->schema << "." << table->table << "." << name
                    << " is AUTO_INCREMENT in the DB, but is not in the "
                       "duality view\n";
          assert(0);
        }
      }
      if (col) {
        col->datatype = type;
        col->type = mrs::database::column_datatype_to_type(type);
        col->is_primary = is_pk;
        col->is_unique = is_unique;
      }
      return false;
    };
    session->query(
        "SHOW COLUMNS IN `" + table->schema + "`.`" + table->table + "`",
        [add_column](const auto &row) {
          add_column(
              row[0], row[1],
              std::string_view(row[3]).find("PRI") != std::string_view::npos,
              std::string_view(row[3]).find("UNI") != std::string_view::npos,
              std::string_view(row[5]).find("auto_increment") !=
                  std::string_view::npos);
          return true;
        });
  }

  void resolve_references(mysqlrouter::MySQLSession *session, Table *table) {
    std::vector<ForeignKeyReference> fks;

    table->foreach_field<ForeignKeyReference,
                         bool>([session, table, &fks](ForeignKeyReference &fk) {
      std::string for_name;
      std::string ref_name;

      // mapping explicitly given
      if (!fk.column_mapping.empty()) return false;

      if (fk.to_many) {
        for_name = fk.ref_table->schema + "/" + fk.ref_table->table;
        ref_name = table->schema + "/" + table->table;
      } else {
        ref_name = fk.ref_table->schema + "/" + fk.ref_table->table;
        for_name = table->schema + "/" + table->table;
      }

      session->query(
          "select fk.id, ((select group_concat(concat(for_col_name, ':', "
          "ref_col_name)) from information_schema.innodb_foreign_cols where "
          "id=fk.id order by pos)) from information_schema.innodb_foreign fk "
          "where for_name = '" +
              for_name + "' and ref_name = '" + ref_name + "'",
          [&fk](const auto &row) {
            for (const auto &col_pair :
                 mysql_harness::split_string(row[1], ',', true)) {
              auto p = col_pair.find(':');
              if (fk.to_many) {
                fk.column_mapping.emplace_back(col_pair.substr(p + 1),
                                               col_pair.substr(0, p));
              } else {
                fk.column_mapping.emplace_back(col_pair.substr(0, p),
                                               col_pair.substr(p + 1));
              }
            }
            return true;
          });

      fks.push_back(fk);
      return false;
    });

    // add FK columns
    for (auto &fk : fks) {
      if (fk.to_many) {
        for (const auto &c : fk.column_mapping) {
          if (auto col = fk.ref_table->get_column(c.second); col) {
            col->is_foreign = true;
          } else {
            auto f = std::make_shared<Column>();
            f->column_name = c.second;
            f->enabled = false;
            f->is_foreign = true;
            fk.ref_table->fields.emplace_back(std::move(f));
          }
        }
      } else {
        for (const auto &c : fk.column_mapping) {
          if (auto col = table->get_column(c.first); col) {
            col->is_foreign = true;
          } else {
            auto f = std::make_shared<Column>();
            f->column_name = c.first;
            f->enabled = false;
            f->is_foreign = true;
            table->fields.emplace_back(std::move(f));
          }
        }
      }
    }
  }

  void resolve(mysqlrouter::MySQLSession *session, Table *table, int &serial,
               bool auto_column) {
    table->table_alias = "t" + std::to_string(serial++);

    table->foreach_field<ForeignKeyReference, int>(
        [table](ForeignKeyReference &fk) {
          fk.ref_table->schema = table->schema;
          return 0;
        });

    // - fill in column info for data fields
    // - add FK columns (disabled)
    // - add FK mappings

    if (session) {
      resolve_references(session, table);
      resolve_columns(session, table, auto_column);
    }

    table->foreach_field<ForeignKeyReference, int>(
        [this, session, &serial, auto_column](ForeignKeyReference &fk) {
          resolve(session, fk.ref_table.get(), serial, auto_column);
          return 0;
        });
  }

 protected:
  ViewBuilder() {}

  std::shared_ptr<Table> m_table;

  void apply_with_flags(Table *table, int flags) {
    table->crud_operations |=
        (flags & TableFlag::WITH_INSERT) ? Operation::Values::valueCreate : 0;
    table->crud_operations |=
        (flags & TableFlag::WITH_UPDATE) ? Operation::Values::valueUpdate : 0;
    table->crud_operations |=
        (flags & TableFlag::WITH_DELETE) ? Operation::Values::valueDelete : 0;
    table->with_check_ = (flags & TableFlag::WITH_NOCHECK) == 0;
  }
};

class DualityViewBuilder : public ViewBuilder {
 public:
  DualityViewBuilder(const std::string &name, const std::string &schema,
                     const std::string &table, int with_flags = 0) {
    auto view = std::make_shared<DualityView>();
    view->name = name;
    m_table = view;
    m_table->schema = schema;
    m_table->table = table;
    m_table->table_alias = "t";
    apply_with_flags(m_table.get(), with_flags);
  }

  DualityViewBuilder(const std::string &schema, const std::string &table,
                     int with_flags = 0)
      : DualityViewBuilder(table + "_dv", schema, table, with_flags) {}
};

inline rapidjson::Document make_json(const std::string &json) {
  rapidjson::Document doc;
  helper::json::text_to(&doc, json);
  return doc;
}

inline std::string pprint_json(const rapidjson::Document &doc) {
  rapidjson::StringBuffer json_buf;
  {
    rapidjson::PrettyWriter<rapidjson::StringBuffer> json_writer(json_buf);

    doc.Accept(json_writer);
  }

  return std::string(json_buf.GetString(), json_buf.GetLength());
}

inline std::string pprint_json(const std::string &json) {
  auto doc = make_json(json);
  if (!doc.IsObject()) {
    printf("ERROR parsing json: %s\n", json.c_str());
  }

  return pprint_json(doc);
}

inline std::string str_replace(std::string_view s, std::string_view from,
                               std::string_view to, bool all = true) {
  std::string str;
  int offs = from.length();
  str.reserve(s.length());

  if (from.empty()) {
    str.append(to);
    for (char c : s) {
      str.push_back(c);
      str.append(to);
    }
  } else {
    std::string::size_type start = 0, p = s.find(from);
    while (p != std::string::npos) {
      if (p > start) str.append(s, start, p - start);
      str.append(to);
      start = p + offs;
      if (!all) break;
      p = s.find(from, start);
    }
    if (start < s.length()) str.append(s, start, s.length() - start);
  }
  return str;
}

inline std::string fill_ids(const std::string &s, std::vector<int> &ids) {
  // <id#> replaces all occurrences with ids[#] and then increments ids[#]
  // <id#++> does the same, but increments ids[#] every time
  std::string r = s;
  for (size_t i = 0; i < ids.size(); i++) {
    auto before = r;
    r = str_replace(r, "<id" + std::to_string(i) + ">", std::to_string(ids[i]),
                    true);
    if (before != r) {
      ids[i]++;
    }

    for (;;) {
      before = r;
      r = str_replace(r, "<id" + std::to_string(i) + "++>",
                      std::to_string(ids[i]), false);
      if (r != before)
        ids[i]++;
      else
        break;
    }
  }
  return r;
}

#endif  // ROUTER_SRC_MYSQL_REST_SERVICE_TESTS_TEST_MRS_OBJECT_UTILS_H_

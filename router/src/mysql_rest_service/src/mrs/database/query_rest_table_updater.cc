/*
 * Copyright (c) 2023, 2024, Oracle and/or its affiliates.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is also distributed with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms, as
 * designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have included with MySQL.
 * This program is distributed in the hope that it will be useful,  but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
 * the GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "mrs/database/query_rest_table_updater.h"

#include <set>
#include "helper/json/rapid_json_to_text.h"
#include "helper/json/to_sqlstring.h"
#include "mrs/database/duality_view/check.h"
#include "mrs/database/duality_view/delete.h"
#include "mrs/database/duality_view/errors.h"
#include "mrs/database/duality_view/insert.h"
#include "mrs/database/duality_view/json_input.h"
#include "mrs/database/duality_view/select.h"
#include "mrs/database/duality_view/update.h"
#include "mrs/database/helper/object_checksum.h"
#include "mrs/database/query_rest_table_single_row.h"
#include "mrs/http/error.h"
#include "mrs/interface/rest_error.h"
#include "mysqld_error.h"

#include <iostream>

// TODO(alfredo) - refactor, replace to_many with is_foreign

namespace mrs {
namespace database {
namespace dv {

using interface::RestError;

/*

## Concurrent Updates with Etag

The basic algorithm for performing concurrent updates with etag is:

1. Start transaction
2. Compute ETag and lock rows to be updated
  - If row lock fails, abort
3. Compare ETag with the one sent in request
  - If ETag doesn't match, abort
4. Update rows
5. Commit

*/

using namespace helper::json::sql;

using MySQLSession = mysqlrouter::MySQLSession;

class RowInsert;
class RowUpdate;

//

DualityViewUpdater::DualityViewUpdater(
    std::shared_ptr<Object> view, const ObjectRowOwnership &row_ownership_info)
    : view_(view), m_row_ownership_info(row_ownership_info) {}

namespace {

// std::shared_ptr<entry::Column> get_generated_id_column(
//     const entry::Table &table) {
//   for (const auto &c : table.columns) {
//     if (c->id_generation != entry::IdGenerationType::NONE) {
//       return c;
//     }
//   }
//   return nullptr;
// }

// const Column *get_generated_id_column(const Table &table) {
//   return table.foreach_column<const Column *>(
//       [](const Column &column) -> const Column * {
//         if (column.id_generation != IdGenerationType::NONE) {
//           return &column;
//         }
//         return nullptr;
//       });
// }

}  // namespace

// template <typename T>
// void process_object_field(const Field &field, const Column &column,
//                           const rapidjson::Value &value, std::shared_ptr<T>
//                           op, const std::string &jptr) {
//   mysqlrouter::sqlstring tmp("?");
//   if (field->source->type == entry::ColumnType::JSON) {
//     tmp << helper::json::to_string(value);
//   } else if (field->source->type == entry::ColumnType::GEOMETRY) {
//     if (value.IsString()) {
//       tmp.reset("ST_AsGeoJSON(ST_GeomFromText(?,?))");
//       tmp << value.GetString();
//       tmp << field->source->srid;
//     } else {
//       tmp << helper::json::to_string(value);
//     }
//   } else if (value.IsBool()) {
//     tmp << value.GetBool();
//   } else {
//     tmp << value;
//   }
//   op->on_value(field, tmp);
// }

void safe_run(MySQLSession *session,
              const std::shared_ptr<DualityViewUpdater::Operation> &op,
              MySQLSession::Transaction *transaction_started = nullptr) {
  MySQLSession::Transaction safe_transaction;
  if (!transaction_started) {
    const bool is_consisten_snapshot = true;
    safe_transaction =
        MySQLSession::Transaction(session, is_consisten_snapshot);
    transaction_started = &safe_transaction;
  }

  try {
    op->run(session);

    transaction_started->commit();
  } catch (...) {
    throw;
  }
}

PrimaryKeyColumnValues DualityViewUpdater::insert(
    MySQLSession *session, const rapidjson::Document &doc) {
  if (view_->is_read_only()) throw_read_only();

  check(doc);
  auto root_insert = make_row_insert({}, view_, m_row_ownership_info);

  root_insert->process(JSONInputObject(doc.GetObject()));

  safe_run(session, root_insert);

  m_affected += root_insert->affected();

  return root_insert->primary_key();
}

PrimaryKeyColumnValues DualityViewUpdater::update(
    MySQLSession *session, const PrimaryKeyColumnValues &pk_values_a,
    const rapidjson::Document &doc, bool upsert) {
  PrimaryKeyColumnValues pk_values = pk_values_a;
  const bool is_consistent_snapshot = true;

  if (view_->is_read_only()) throw_read_only();

  check_primary_key(pk_values);

  check(doc, true);

  MySQLSession::Transaction transaction{session, is_consistent_snapshot};

  bool is_owned;
  const auto current_doc = select_one(session, pk_values, is_owned);
  if (!current_doc.IsObject()) {
    if (upsert && view_->with_insert()) return insert(session, doc);

    throw std::runtime_error("Row not found");
  } else {
    if (!is_owned) throw http::Error(HttpStatusCode::Forbidden);
  }

  std::shared_ptr<RowUpdate> root_update;

  check_etag_and_lock_rows(session, doc, pk_values);

  root_update =
      make_row_update(std::shared_ptr<DualityViewUpdater::Operation>{}, view_,
                      pk_values, row_ownership_info());

  root_update->process(
      JSONInputObject(doc.GetObject(), current_doc.GetObject()));

  // On success it commits.
  safe_run(session, root_update, &transaction);

  m_affected += root_update->affected();

  return root_update->primary_key();
}  // namespace dv

uint64_t DualityViewUpdater::delete_(
    MySQLSession *session, const PrimaryKeyColumnValues &pk_values_a) {
  PrimaryKeyColumnValues pk_values = pk_values_a;
  if (view_->is_read_only()) throw_read_only();

  const bool is_consistent_snapshot = true;

  check_primary_key(pk_values);

  MySQLSession::Transaction transaction{session, is_consistent_snapshot};

  // check_etag_and_lock_rows(session, doc, pk_values);

  auto del =
      std::make_shared<RowDelete>(view_, pk_values, row_ownership_info());

  del->process(JSONInputObject());

  // On success it commits.
  safe_run(session, del, &transaction);

  m_affected += del->affected();

  return del->affected();
}

uint64_t DualityViewUpdater::delete_(MySQLSession *session,
                                     const FilterObjectGenerator &filter) {
  const bool is_consistent_snapshot = true;

  auto result = filter.get_result();
  if (result.is_empty())
    throw std::runtime_error("Filter must contain valid JSON object.");
  if (filter.has_order())
    throw std::runtime_error("Filter must not contain ordering informations.");
  // Note: user given filter may try to filter by the owner_id to access rows
  // they're not allowed, but since the row_owner check is also done, worst
  // case the WHERE will match nothing

  MySQLSession::Transaction transaction{session, is_consistent_snapshot};

  auto del = std::make_shared<RowDeleteMany>(view_, std::move(result),
                                             row_ownership_info());

  del->process(JSONInputObject());

  safe_run(session, del, &transaction);

  return del->affected();
}

void DualityViewUpdater::check(const rapidjson::Document &doc,
                               bool for_update) const {
  if (!doc.IsObject()) throw_invalid_type(view_->table);

  Check checker(view_, m_row_ownership_info, for_update);

  checker.process(JSONInputObject(doc.GetObject()));
}

void DualityViewUpdater::check_primary_key(PrimaryKeyColumnValues &pk_values) {
  auto pk_cols = view_->primary_key();

  for (const auto &col : pk_cols) {
    if (pk_values.find(col->column_name) == pk_values.end()) {
      if (!m_row_ownership_info.is_owner_id(*view_, *col))
        throw RestError("Missing primary key column value for " +
                        col->column_name);
      else
        pk_values[col->column_name] = m_row_ownership_info.owner_user_id();
    }
  }

  if (std::any_of(pk_values.begin(), pk_values.end(), [pk_cols](const auto &c) {
        return std::find_if(pk_cols.begin(), pk_cols.end(),
                            [c](const auto &col) {
                              return c.first == col->column_name;
                            }) == pk_cols.end();
      })) {
    throw RestError("Invalid primary key column");
  }
}

std::string DualityViewUpdater::compute_etag_and_lock_rows(
    [[maybe_unused]] MySQLSession *session,
    [[maybe_unused]] const PrimaryKeyColumnValues &pk_values) const {
  // JsonQueryBuilder qb({}, true, true);
  // qb.process_object(m_object);

  // auto query = qb.query_one(pk_values);
  // auto row = session->query_one(query);

  // return compute_checksum(m_object, std::string_view((*row)[0]));
  return {};
}

void DualityViewUpdater::check_etag_and_lock_rows(
    [[maybe_unused]] MySQLSession *session,
    [[maybe_unused]] const rapidjson::Value &doc,
    [[maybe_unused]] const PrimaryKeyColumnValues &pk_values) const {
  // if (doc.HasMember("_metadata")) {
  //   const auto &metadata = doc["_metadata"];
  //   if (metadata.IsObject() && metadata.HasMember("etag")) {
  //     const auto &etag = metadata["etag"];
  //     if (etag.IsString()) {
  //       auto checksum = compute_etag_and_lock_rows(session, pk_values);

  //       if (etag.GetString() == checksum) {
  //         return;
  //       } else {
  //         throw interface::ETagMismatch();
  //       }
  //     }
  //     throw RestError("Invalid etag");
  //   }
  // }
  // if etag is missing, then just don't validate
}

rapidjson::Document DualityViewUpdater::select_one(
    MySQLSession *session, const PrimaryKeyColumnValues &pk_values,
    bool &is_owned) const {
  QueryRestTableSingleRow q(nullptr, false, false);

  q.query_entry(session, view_, pk_values, {}, "url", m_row_ownership_info,
                false, {}, true);
  if (q.response.empty()) return {};

  rapidjson::Document doc;
  doc.Parse(q.response.data(), q.response.size());

  is_owned = q.is_owned();

  return doc;
}

}  // namespace dv
}  // namespace database
}  // namespace mrs

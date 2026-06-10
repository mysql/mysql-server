/*
 * Copyright (c) 2023, 2026, Oracle and/or its affiliates.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is designed to work with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have either included with
 * the program or referenced in the documentation.
 *
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
#include "mrs/database/helper/object_checksum.h"
#include "mrs/database/json_mapper/check.h"
#include "mrs/database/json_mapper/delete.h"
#include "mrs/database/json_mapper/errors.h"
#include "mrs/database/json_mapper/insert.h"
#include "mrs/database/json_mapper/json_input.h"
#include "mrs/database/json_mapper/select.h"
#include "mrs/database/json_mapper/update.h"
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

JsonMappingUpdater::JsonMappingUpdater(
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

PrimaryKeyColumnValues JsonMappingUpdater::insert(
    MySQLSession *session, const rapidjson::Document &doc) {
  if (view_->is_read_only()) throw_read_only();

  check(doc);
  auto root_insert = make_row_insert({}, view_, m_row_ownership_info);

  root_insert->process(JSONInputObject(doc.GetObject()));

  root_insert->run(session);

  m_affected += root_insert->affected();

  return root_insert->primary_key();
}

PrimaryKeyColumnValues JsonMappingUpdater::update(
    MySQLSession *session, const PrimaryKeyColumnValues &pk_values_a,
    const rapidjson::Document &doc, bool upsert) {
  PrimaryKeyColumnValues pk_values = pk_values_a;

  if (view_->is_read_only()) throw_read_only();

  validate_primary_key_values(*view_, row_ownership_info(), pk_values);

  check(doc, true);

  bool is_owned;
  std::string current_doc;

  try {
    current_doc =
        select_one(session, pk_values, is_owned, RowLockType::FOR_UPDATE);
  } catch (const MySQLSession::Error &e) {
    // ER_LOCK_NOWAIT happens when SELECT ... FOR UPDATE NOWAIT fails because
    // someone else locked the row (e.g. another user updating the same row
    // at the same time via MRS)
    if (e.code() == ER_LOCK_NOWAIT) throw interface::ETagMismatch();
  }

  if (current_doc.empty()) {
    if (upsert && view_->with_insert()) {
      auto root_insert = make_row_insert({}, view_, m_row_ownership_info);

      root_insert->process(JSONInputObject(doc.GetObject()));

      root_insert->run(session);

      m_affected += root_insert->affected();

      return root_insert->primary_key();
    }

    throw std::runtime_error("Row not found");
  } else {
    if (!is_owned) throw http::Error(HttpStatusCode::Forbidden);
  }

  std::shared_ptr<RowUpdate> root_update;

  check_etag(current_doc, doc);

  root_update =
      make_row_update(std::shared_ptr<JsonMappingUpdater::Operation>{}, view_,
                      pk_values, row_ownership_info());

  {
    rapidjson::Document old_doc;
    old_doc.Parse(current_doc.data(), current_doc.size());

    root_update->process(JSONInputObject(doc.GetObject(), old_doc.GetObject()));
  }

  // On success it commits.
  root_update->run(session);

  m_affected += root_update->affected();

  return root_update->primary_key();
}

uint64_t JsonMappingUpdater::delete_(
    MySQLSession *session, const PrimaryKeyColumnValues &pk_values_a) {
  PrimaryKeyColumnValues pk_values = pk_values_a;
  if (view_->is_read_only()) throw_read_only();

  const bool is_consistent_snapshot = false;

  validate_primary_key_values(*view_, row_ownership_info(), pk_values);

  MySQLSession::Transaction transaction{session, is_consistent_snapshot};

  auto del =
      std::make_shared<RowDelete>(view_, pk_values, row_ownership_info());

  del->process(JSONInputObject());

  del->run(session);

  transaction.commit();

  m_affected += del->affected();

  return del->affected();
}

uint64_t JsonMappingUpdater::delete_(MySQLSession *session,
                                     const FilterObjectGenerator &filter) {
  const bool is_consistent_snapshot = false;

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

  del->run(session);

  transaction.commit();

  return del->affected();
}

bool JsonMappingUpdater::has_references() const {
  return view_->foreach_field<ForeignKeyReference, bool>(
      [](const ForeignKeyReference &) { return true; });
}

void JsonMappingUpdater::check(const rapidjson::Document &doc,
                               bool for_update) const {
  if (!doc.IsObject()) throw_invalid_type(view_->table);

  Check checker(view_, m_row_ownership_info, for_update);

  checker.process(JSONInputObject(doc.GetObject()));
}

void JsonMappingUpdater::check_etag(const std::string &original_doc,
                                    const rapidjson::Document &new_doc) const {
  if (new_doc.HasMember("_metadata")) {
    const auto &metadata = new_doc["_metadata"];
    if (metadata.IsObject() && metadata.HasMember("etag")) {
      const auto &etag = metadata["etag"];
      if (etag.IsString()) {
        auto checksum = compute_checksum(view_, original_doc);
        if (etag.GetString() == checksum) {
          return;
        } else {
          throw interface::ETagMismatch();
        }
      }
      throw RestError("Invalid etag");
    }
  }
  // if etag is missing, then just don't validate
}

std::string JsonMappingUpdater::select_one(
    MySQLSession *session, const PrimaryKeyColumnValues &pk_values,
    bool &is_owned, RowLockType lock_rows) const {
  QueryRestTableSingleRow q(nullptr, false, false, lock_rows);

  q.query_entry(session, view_, pk_values, {}, "url", m_row_ownership_info, {},
                false, {}, true);
  if (q.response.empty()) return {};

  is_owned = q.is_owned();

  return q.response;
}

}  // namespace dv
}  // namespace database
}  // namespace mrs

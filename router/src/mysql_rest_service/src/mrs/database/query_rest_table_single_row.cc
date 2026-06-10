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

#include "mrs/database/query_rest_table_single_row.h"
#include <stdexcept>
#include "helper/json/to_string.h"
#include "mrs/database/helper/object_checksum.h"
#include "mrs/database/json_mapper/common.h"
#include "mrs/database/json_mapper/select.h"

namespace mrs {
namespace database {

QueryRestTableSingleRow::QueryRestTableSingleRow(
    const JsonTemplateFactory *factory, bool encode_bigints_as_string,
    const bool include_links, const RowLockType lock_rows,
    uint64_t max_execution_time_ms)
    : QueryRestTable(factory, encode_bigints_as_string, include_links,
                     max_execution_time_ms),
      lock_rows_(lock_rows) {}

void QueryRestTableSingleRow::query_entry(
    MySQLSession *session, std::shared_ptr<database::entry::Object> object,
    const PrimaryKeyColumnValues &pk, const dv::ObjectFieldFilter &field_filter,
    const std::string &url_route, const ObjectRowOwnership &row_ownership,
    const FilterObjectGenerator &fog, const bool compute_etag,
    std::function<std::string()> commit, const bool fetch_any_owner) {
  assert(!fog.has_where() && !fog.has_order());

  PrimaryKeyColumnValues complete_pk(pk);

  dv::validate_primary_key_values(
      *object, fetch_any_owner ? ObjectRowOwnership() : row_ownership,
      complete_pk);

  commit_ = std::move(commit);
  object_ = object;
  compute_etag_ = compute_etag;
  metadata_received_ = false;
  items = 0;
  config_ = {0, 0, false, url_route};
  field_filter_ = &field_filter;

  build_query(field_filter, url_route, row_ownership, complete_pk,
              fetch_any_owner, fog);

  execute(session);
}

void QueryRestTableSingleRow::on_row(const ResultRow &r) {
  if (!response.empty())
    throw std::runtime_error(
        "Querying single row, from a table. Received multiple.");

  std::string gtid;
  if (commit_) gtid = commit_();

  response = post_process_json(
      object_, field_filter_ ? *field_filter_ : ObjectFieldFilter{}, gtid, r[0],
      compute_etag_);

  is_owned_ = r[1] && strcmp(r[1], "1") == 0;

  ++items;
}

void QueryRestTableSingleRow::build_query(
    const ObjectFieldFilter &field_filter, const std::string &url_route,
    const ObjectRowOwnership &row_ownership, const PrimaryKeyColumnValues &pk,
    bool fetch_any_owner, const FilterObjectGenerator &fog) {
  assert(!pk.empty());

  auto where =
      build_where(fetch_any_owner ? ObjectRowOwnership() : row_ownership);
  extend_where(where, fog);

  dv::JsonQueryBuilder qb(field_filter, row_ownership,
                          lock_rows_ == RowLockType::FOR_UPDATE,
                          encode_bigints_as_strings_);
  qb.process_view(object_);

  std::vector<mysqlrouter::sqlstring> fields;
  if (!qb.select_items().is_empty()) fields.push_back(qb.select_items());
  if (include_links_) {
    fields.emplace_back(
        "'links', JSON_ARRAY(JSON_OBJECT('rel', 'self', "
        "'href', CONCAT(?,'/',CONCAT_WS(',',?))))");
    fields.back() << url_route << dv::format_key(*object_, pk);
  }

  if (where.is_empty()) {
    where = {"WHERE "};
    where.append_preformatted(dv::format_where_expr(*object_, pk));
  } else {
    where.append_preformatted_sep(" AND ", dv::format_where_expr(*object_, pk));
  }

  mysqlrouter::sqlstring row_owner_check;
  if (row_ownership.enabled()) {
    row_owner_check = row_ownership.owner_check_expr(object_->table_alias);
  } else {
    row_owner_check = mysqlrouter::sqlstring("1");
  }

  if (max_execution_time_ms_ > 0) {
    query_ =
        "SELECT /*+ MAX_EXECUTION_TIME(?) */ JSON_OBJECT(?), ? as is_owned "
        "FROM ? ?";
    query_ << max_execution_time_ms_;
  } else {
    query_ = "SELECT JSON_OBJECT(?), ? as is_owned FROM ? ?";
  }
  query_ << fields << row_owner_check << qb.from_clause() << where;

  if (lock_rows_ == RowLockType::FOR_UPDATE)
    query_.append_preformatted(" FOR UPDATE NOWAIT");
}

}  // namespace database
}  // namespace mrs

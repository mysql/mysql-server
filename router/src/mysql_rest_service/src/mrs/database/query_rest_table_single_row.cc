/*
  Copyright (c) 2021, 2024, Oracle and/or its affiliates.

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

#include "mrs/database/query_rest_table_single_row.h"
#include <stdexcept>
#include "helper/json/to_string.h"
#include "mrs/database/duality_view/select.h"
#include "mrs/database/helper/object_checksum.h"

namespace mrs {
namespace database {

static void json_object_fast_append(std::string &jo, const std::string &key,
                                    const std::string &value) {
  // remove closing }
  jo.pop_back();
  // add metadata sub-object

  jo.append(", \"");
  jo.append(key);
  jo.append("\":");
  jo.append(value);
  jo.push_back('}');
}

QueryRestTableSingleRow::QueryRestTableSingleRow(
    const JsonTemplateFactory *factory, bool encode_bigints_as_string,
    const bool include_links)
    : QueryRestTable(factory, encode_bigints_as_string, include_links) {}

void QueryRestTableSingleRow::query_entry(
    MySQLSession *session, std::shared_ptr<database::entry::Object> object,
    const PrimaryKeyColumnValues &pk, const dv::ObjectFieldFilter &field_filter,
    const std::string &url_route, const ObjectRowOwnership &row_ownership,
    const bool compute_etag, const std::string &metadata_gtid,
    const bool fetch_any_owner) {
  object_ = object;
  compute_etag_ = compute_etag;
  metadata_received_ = false;
  metadata_gtid_ = metadata_gtid;
  items = 0;
  config_ = {0, 0, false, url_route};
  field_filter_ = &field_filter;

  build_query(field_filter, url_route, row_ownership, pk, fetch_any_owner);

  execute(session);
}

void QueryRestTableSingleRow::on_row(const ResultRow &r) {
  std::map<std::string, std::string> metadata_;
  if (!response.empty())
    throw std::runtime_error(
        "Querying single row, from a table. Received multiple.");

  if (!metadata_gtid_.empty()) {
    metadata_.insert({"gtid", metadata_gtid_});
  }
  response = post_process_json(
      object_, field_filter_ ? *field_filter_ : ObjectFieldFilter{}, {}, r[0],
      compute_etag_);

  if (!metadata_.empty()) {
    json_object_fast_append(response, "_metadata",
                            helper::json::to_string(metadata_));
  }

  is_owned_ = r[1] && strcmp(r[1], "1") == 0;

  ++items;
}

void QueryRestTableSingleRow::build_query(
    const ObjectFieldFilter &field_filter, const std::string &url_route,
    const ObjectRowOwnership &row_ownership, const PrimaryKeyColumnValues &pk,
    bool fetch_any_owner) {
  assert(!pk.empty());

  auto where =
      build_where(fetch_any_owner ? ObjectRowOwnership() : row_ownership);

  dv::JsonQueryBuilder qb(field_filter, row_ownership, false,
                          // compute_etag_,
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

  query_ = "SELECT JSON_OBJECT(?), ? as is_owned FROM ? ?;";
  query_ << fields << row_owner_check << qb.from_clause() << where;
}

}  // namespace database
}  // namespace mrs

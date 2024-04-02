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
#include "mrs/database/helper/object_checksum.h"
#include "mrs/database/helper/object_query.h"

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

QueryRestTableSingleRow::QueryRestTableSingleRow(bool encode_bigints_as_string,
                                                 const bool include_links)
    : encode_bigints_as_string_{encode_bigints_as_string},
      include_links_{include_links} {}

void QueryRestTableSingleRow::query_entries(
    MySQLSession *session, std::shared_ptr<database::entry::Object> object,
    const ObjectFieldFilter &field_filter, const PrimaryKeyColumnValues &pk,
    const std::string &url_route, bool compute_etag,
    const std::string &metadata_gtid) {
  object_ = object;
  compute_etag_ = compute_etag;
  metadata_gtid_ = metadata_gtid;
  field_filter_ = &field_filter;

  response = "";
  items = 0;
  build_query(object, pk, url_route);

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

  response = r[0];
  if (compute_etag_) {
    // calc etag and strip filtered fields
    process_document_etag_and_filter(object_, *field_filter_, metadata_,
                                     &response);
  } else if (!metadata_.empty()) {
    json_object_fast_append(response, "_metadata",
                            helper::json::to_string(metadata_));
  }

  ++items;
}

void QueryRestTableSingleRow::build_query(
    std::shared_ptr<database::entry::Object> object,
    const PrimaryKeyColumnValues &pk, const std::string &url_route) {
  JsonQueryBuilder qb(*field_filter_, false, compute_etag_,
                      encode_bigints_as_string_);
  qb.process_object(object);

  std::vector<mysqlrouter::sqlstring> fields;
  if (!qb.select_items().is_empty()) fields.push_back(qb.select_items());
  if (include_links_) {
    fields.emplace_back(
        "'links', JSON_ARRAY(JSON_OBJECT('rel', 'self', "
        "'href', CONCAT(?,'/',CONCAT_WS(',',?))))");
    fields.back() << url_route << format_key(object->get_base_table(), pk);
  }

  query_ = "SELECT JSON_OBJECT(?) FROM ? WHERE ?;";
  query_ << fields << qb.from_clause()
         << format_where_expr(object->get_base_table(), pk);
}

}  // namespace database
}  // namespace mrs

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

#include "mrs/database/query_rest_table.h"

#include <algorithm>
#include <map>
#include <stdexcept>

#include "helper/json/rapid_json_to_text.h"
#include "helper/json/text_to.h"
#include "helper/json/to_string.h"
#include "mrs/database/filter_object_generator.h"
#include "mrs/database/helper/object_checksum.h"
#include "mrs/database/helper/object_query.h"
#include "mrs/json/json_template_factory.h"
#include "mrs/json/response_json_template.h"
#include "mysql/harness/logging/logging.h"
#include "mysql/harness/utility/string.h"

IMPORT_LOG_FUNCTIONS()

namespace mrs {
namespace database {

using sqlstring = mysqlrouter::sqlstring;
using MySQLSession = mysqlrouter::MySQLSession;

QueryRestTable::QueryRestTable(const JsonTemplateFactory *factory,
                               bool encode_bigints_as_strings,
                               bool include_links)
    : factory_{factory},
      encode_bigints_as_strings_{encode_bigints_as_strings},
      include_links_{include_links} {}

QueryRestTable::QueryRestTable(bool encode_bigints_as_strings,
                               bool include_links)
    : QueryRestTable(nullptr, encode_bigints_as_strings, include_links) {}

void QueryRestTable::query_entries(
    MySQLSession *session, std::shared_ptr<database::entry::Object> object,
    const ObjectFieldFilter &field_filter, const uint32_t offset,
    const uint32_t limit, const std::string &url_route,
    const bool is_default_limit, const ObjectRowOwnership &row_ownership,
    const FilterObjectGenerator &fog, const bool compute_etag) {
  create_serializer();
  object_ = object;
  compute_etag_ = compute_etag;
  metadata_received_ = false;
  items = 0;
  config_ = {offset, limit, is_default_limit, url_route};

  build_query(field_filter, offset, limit + 1, url_route, row_ownership, fog);

  serializer_->begin();
  execute(session);
  if (!metadata_received_) on_metadata(0, nullptr);
  serializer_->finish();

  response = serializer_->get_result();
}

void QueryRestTable::on_metadata(unsigned number, MYSQL_FIELD *fields) {
  metadata_received_ = true;
  Query::on_metadata(number, fields);
  columns_.clear();
  for (unsigned int i = 0; i < number; ++i) {
    columns_.emplace_back(&fields[i]);
  }
  serializer_->begin_resultset(config_.offset, config_.limit,
                               config_.is_default_limit, config_.url_route,
                               columns_);
}

void QueryRestTable::on_row(const ResultRow &r) {
  if (compute_etag_) {
    std::string doc = r[0];
    // calc etag and strip filtered fields
    process_document_etag_and_filter(object_, *field_filter_, {}, &doc);
    serializer_->push_json_document(doc.c_str());
  } else {
    serializer_->push_json_document(r[0]);
  }
  ++items;
}

const sqlstring &QueryRestTable::build_where(
    const ObjectRowOwnership &row_ownership) {
  using MatchLevel = RowGroupOwnership::MatchLevel;
  static std::map<MatchLevel, sqlstring> operations{
      {MatchLevel::kHigher, ">"},
      {MatchLevel::kEqualOrHigher, ">="},
      {MatchLevel::kEqual, "="},
      {MatchLevel::kLowerOrEqual, "<="},
      {MatchLevel::kLower, "<"}};

  static std::string empty;
  auto &user_ownership_column =
      row_ownership.enabled() ? row_ownership.owner_column_name() : empty;

  if (user_ownership_column.empty() && row_ownership.row_groups().empty())
    return mysqlrouter::sqlstring::empty;

  where_.reset("WHERE !");
  if (row_ownership.row_groups().empty()) {
    if (!row_ownership.enabled()) {
      where_.reset("WHERE ! is NULL");

      where_ << user_ownership_column;
      return where_;
    }

    where_.reset(
        "WHERE (! IN (WITH RECURSIVE cte As ("
        "SELECT a.id as id FROM mysql_rest_service_metadata.mrs_user a WHERE "
        "a.id = ? "
        "UNION ALL "
        "SELECT h.user_id as id FROM "
        "mysql_rest_service_metadata.mrs_user_hierarchy as h "
        "JOIN cte c ON c.id=h.reporting_to_user_id"
        ") SELECT * FROM cte) OR ! is NULL)");

    where_ << user_ownership_column << row_ownership.owner_user_id()
           << user_ownership_column;

    return where_;
  }

  if (user_ownership_column.empty()) {
    std::string query{"WHERE ("};
    for (size_t i = 0; i < row_ownership.row_groups().size(); ++i) {
      query +=
          " ! in (WITH RECURSIVE cte AS("
          "SELECT user_group_id, parent_group_id, level FROM "
          "mysql_rest_service_metadata.mrs_user_group_hierarchy WHERE "
          "user_group_id in (?) AND group_hierarchy_type_id=? "
          "UNION ALL "
          "SELECT p.user_group_id, p.parent_group_id, p.level "
          "FROM mysql_rest_service_metadata.mrs_user_group_hierarchy as p JOIN "
          "cte on p.user_group_id = cte.parent_group_id AND "
          "group_hierarchy_type_id=? ) "
          "SELECT parent_group_id FROM cte  WHERE level ! ? ) OR ";
    }

    query += "(";
    for (size_t i = 0; i < row_ownership.row_groups().size(); ++i) {
      query += i != 0 ? "AND ! is NULL " : "! is NULL ";
    }
    query += ")) ";

    where_.reset(query.c_str());

    for (auto &group : row_ownership.row_groups()) {
      where_ << group.row_group_ownership_column << row_ownership.user_groups()
             << group.hierarhy_id << group.hierarhy_id
             << operations[group.match] << group.level;
    }

    for (auto &group : row_ownership.row_groups()) {
      where_ << group.row_group_ownership_column;
    }

    return where_;
  }

  std::string query{
      "WHERE (! IN (WITH RECURSIVE cte As ("
      "SELECT a.id as id FROM mysql_rest_service_metadata.mrs_user a WHERE"
      "a.id = ? "
      "UNION ALL "
      "SELECT h.user_id as id FROM "
      "mysql_rest_service_metadata.mrs_user_hierarchy as h "
      "JOIN cte c ON c.id=h.reporting_to_user_id"
      ") SELECT * FROM cte) OR "};

  for (size_t i = 0; i < row_ownership.row_groups().size(); ++i) {
    query +=
        " ! in (WITH RECURSIVE cte AS("
        "SELECT user_group_id, parent_group_id, level FROM "
        "mysql_rest_service_metadata.mrs_user_group_hierarchy WHERE "
        "user_group_id in (?) AND group_hierarchy_type_id=? "
        "UNION ALL "
        "SELECT p.user_group_id, p.parent_group_id, p.level "
        "FROM mysql_rest_service_metadata.mrs_user_group_hierarchy as p JOIN "
        "cte on p.user_group_id = cte.parent_group_id AND "
        "group_hierarchy_type_id=? ) "
        "SELECT parent_group_id FROM cte  WHERE level ! ? ) OR ";
  }

  query += "( ! is NULL ";
  for (size_t i = 0; i < row_ownership.row_groups().size(); ++i) {
    query += "AND ! is NULL ";
  }
  query += ")) ";
  where_.reset(query.c_str());
  where_ << row_ownership.owner_column_name() << row_ownership.owner_user_id();

  for (auto &group : row_ownership.row_groups()) {
    where_ << group.row_group_ownership_column << row_ownership.user_groups()
           << group.hierarhy_id << group.hierarhy_id << operations[group.match]
           << group.level;
  }

  where_ << user_ownership_column;
  for (auto &group : row_ownership.row_groups()) {
    where_ << group.row_group_ownership_column;
  }

  return where_;
}

const sqlstring &QueryRestTable::build_where(
    const RowUserOwnership &row_user, UserId *user_id,
    const std::vector<RowGroupOwnership> &row_groups,
    const std::set<UniversalId> &user_groups) {
  using MatchLevel = RowGroupOwnership::MatchLevel;
  static std::map<MatchLevel, sqlstring> operations{
      {MatchLevel::kHigher, ">"},
      {MatchLevel::kEqualOrHigher, ">="},
      {MatchLevel::kEqual, "="},
      {MatchLevel::kLowerOrEqual, "<="},
      {MatchLevel::kLower, "<"}};

  static std::string empty;
  auto &user_ownership_column =
      row_user.user_ownership_enforced ? row_user.user_ownership_column : empty;

  if (user_ownership_column.empty() && row_groups.empty())
    return sqlstring::empty;

  where_.reset("WHERE !");
  if (row_groups.empty()) {
    if (!user_id) {
      where_.reset("WHERE ! is NULL");

      where_ << user_ownership_column;
      return where_;
    }

    where_.reset(
        "WHERE (! IN (WITH RECURSIVE cte As ("
        "SELECT a.id as id FROM mysql_rest_service_metadata.mrs_user a WHERE "
        "a.id = ? "
        "UNION ALL "
        "SELECT h.user_id as id FROM "
        "mysql_rest_service_metadata.mrs_user_hierarchy as h "
        "JOIN cte c ON c.id=h.reporting_to_user_id"
        ") SELECT * FROM cte) OR ! is NULL)");

    where_ << user_ownership_column << to_sqlstring(*user_id)
           << user_ownership_column;

    return where_;
  }

  if (user_ownership_column.empty()) {
    std::string query{"WHERE ("};
    for (size_t i = 0; i < row_groups.size(); ++i) {
      query +=
          " ! in (WITH RECURSIVE cte AS("
          "SELECT user_group_id, parent_group_id, level FROM "
          "mysql_rest_service_metadata.mrs_user_group_hierarchy WHERE "
          "user_group_id in (?) AND group_hierarchy_type_id=? "
          "UNION ALL "
          "SELECT p.user_group_id, p.parent_group_id, p.level "
          "FROM mysql_rest_service_metadata.mrs_user_group_hierarchy as p JOIN "
          "cte on p.user_group_id = cte.parent_group_id AND "
          "group_hierarchy_type_id=? ) "
          "SELECT parent_group_id FROM cte  WHERE level ! ? ) OR ";
    }

    query += "(";
    for (size_t i = 0; i < row_groups.size(); ++i) {
      query += i != 0 ? "AND ! is NULL " : "! is NULL ";
    }
    query += ")) ";

    where_.reset(query.c_str());

    for (auto &group : row_groups) {
      where_ << group.row_group_ownership_column << user_groups
             << group.hierarhy_id << group.hierarhy_id
             << operations[group.match] << group.level;
    }

    for (auto &group : row_groups) {
      where_ << group.row_group_ownership_column;
    }

    return where_;
  }

  std::string query{
      "WHERE (! IN (WITH RECURSIVE cte As ("
      "SELECT a.id as id FROM mysql_rest_service_metadata.mrs_user a WHERE"
      "a.id = ? "
      "UNION ALL "
      "SELECT h.user_id as id FROM "
      "mysql_rest_service_metadata.mrs_user_hierarchy as h "
      "JOIN cte c ON c.id=h.reporting_to_user_id"
      ") SELECT * FROM cte) OR "};

  for (size_t i = 0; i < row_groups.size(); ++i) {
    query +=
        " ! in (WITH RECURSIVE cte AS("
        "SELECT user_group_id, parent_group_id, level FROM "
        "mysql_rest_service_metadata.mrs_user_group_hierarchy WHERE "
        "user_group_id in (?) AND group_hierarchy_type_id=? "
        "UNION ALL "
        "SELECT p.user_group_id, p.parent_group_id, p.level "
        "FROM mysql_rest_service_metadata.mrs_user_group_hierarchy as p JOIN "
        "cte on p.user_group_id = cte.parent_group_id AND "
        "group_hierarchy_type_id=? ) "
        "SELECT parent_group_id FROM cte  WHERE level ! ? ) OR ";
  }

  query += "( ! is NULL ";
  for (size_t i = 0; i < row_groups.size(); ++i) {
    query += "AND ! is NULL ";
  }
  query += ")) ";
  where_.reset(query.c_str());
  where_ << user_ownership_column << to_sqlstring(*user_id);

  for (auto &group : row_groups) {
    where_ << group.row_group_ownership_column << user_groups
           << group.hierarhy_id << group.hierarhy_id << operations[group.match]
           << group.level;
  }

  where_ << user_ownership_column;
  for (auto &group : row_groups) {
    where_ << group.row_group_ownership_column;
  }

  return where_;
}

void extend_where(sqlstring &where, const FilterObjectGenerator &fog) {
  using namespace std::literals::string_literals;

  const auto &result = fog.get_result();
  if (result.is_empty()) return;

  if (fog.has_where()) {
    bool is_empty = where.is_empty();

    sqlstring r{"? ? ?"};
    r << where << sqlstring(is_empty ? "WHERE" : "AND") << result;
    where = r;
    return;
  }

  where.append_preformatted(result);
}

void QueryRestTable::build_query(const ObjectFieldFilter &field_filter,
                                 const uint32_t offset, const uint32_t limit,
                                 const std::string &url,
                                 const ObjectRowOwnership &row_ownership,
                                 const FilterObjectGenerator &fog) {
  auto where = build_where(row_ownership);
  extend_where(where, fog);

  JsonQueryBuilder qb(field_filter, false, false, false,
                      encode_bigints_as_strings_);

  qb.process_object(object_);

  query_ = sqlstring("SELECT JSON_OBJECT(?) as doc FROM ? ? LIMIT ?,?");
  std::vector<sqlstring> json_object_fields;

  if (!qb.select_items().is_empty())
    json_object_fields.push_back(qb.select_items());

  auto pk_columns = format_key_names(object_->get_base_table());

  if (include_links_) {
    if (pk_columns.is_empty()) {
      static sqlstring empty_links{"'links', JSON_ARRAY()"};
      json_object_fields.push_back(empty_links);
    } else {
      sqlstring fmt{
          "'links', "
          "JSON_ARRAY(JSON_OBJECT('rel','self','href',CONCAT(?,'/',"
          "CONCAT_WS(',',?))))"};
      fmt << url << pk_columns;
      json_object_fields.push_back(fmt);
    }
  }

  query_ << json_object_fields;
  query_ << qb.from_clause();
  query_ << where << offset << limit;
}

void QueryRestTable::create_serializer() {
  mrs::json::JsonTemplateFactory factory_instance;
  auto factory = factory_ ? factory_ : &factory_instance;

  serializer_ = factory->create_template(
      JsonTemplateType::kStandard, encode_bigints_as_strings_, include_links_);
}

}  // namespace database
}  // namespace mrs

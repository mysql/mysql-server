/*
  Copyright (c) 2024, 2026, Oracle and/or its affiliates.

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

#include "mrs/database/query_entries_db_service.h"

#include <string>

#include "helper/mysql_row.h"
#include "mrs/database/helper/query_audit_log_maxid.h"

#include "mysql/harness/string_utils.h"

namespace mrs {
namespace database {

const mysqlrouter::sqlstring k_s_enabled{"s.enabled"};
const mysqlrouter::sqlstring k_s_enabled_and_published{
    "s.enabled and s.published"};

QueryEntriesDbService::QueryEntriesDbService(
    SupportedMrsMetadataVersion version, std::optional<uint64_t> router_id)
    : db_version_{version} {
  // Alias `service_id`, used by QueryChangesDbService
  query_ =
      "SELECT * FROM (SELECT"
      "  s.id as service_id, s.url_host_id as url_host_id, s.url_context_root "
      "as url_context_root, s.url_protocol,"
      "  !, s.comments, s.options,"
      "  s.auth_path, s.auth_completed_url, s.auth_completed_url_validation,"
      "  s.auth_completed_page_content, s.enable_sql_endpoint,"
      "  s.custom_metadata_schema !"
      " FROM mysql_rest_service_metadata.`service` as s ) as parent ";

  if (db_version_ == mrs::interface::kSupportedMrsMetadataVersion_2)
    query_ << k_s_enabled << mysqlrouter::sqlstring{""};
  else {
    if (!router_id) {
      query_ << k_s_enabled_and_published;
    } else {
      mysqlrouter::sqlstring service_is_enabled{
          "IF(s.id in (select rs.service_id "
          " from mysql_rest_service_metadata.router_services rs"
          " WHERE rs.router_id = ?),true, (s.published = 1 AND s.enabled = 1 "
          "AND"
          " (SELECT 0=COUNT(r.id) from mysql_rest_service_metadata.router r"
          " WHERE r.id=?))) "};
      service_is_enabled << router_id.value() << router_id.value();

      query_ << service_is_enabled;
    }
    query_ << mysqlrouter::sqlstring{", s.name, s.metadata, s.published"};
  }
}

void QueryEntriesDbService::force_query_all() { query_all_ = true; }

uint64_t QueryEntriesDbService::get_last_update() { return audit_log_id_; }

void QueryEntriesDbService::query_entries(MySQLSession *session) {
  entries.clear();
  query_all_ = false;

  QueryAuditLogMaxId query_audit_id;

  auto audit_log_id = query_audit_id.query_max_id(session);
  execute(session);

  audit_log_id_ = audit_log_id;
}

void QueryEntriesDbService::on_row(const ResultRow &row) {
  using MySQLRow = helper::MySQLRow;
  entries.emplace_back();

  helper::MySQLRow mysql_row(row, metadata_, num_of_metadata_);
  DbService &entry = entries.back();

  mysql_row.unserialize_with_converter(&entry.id, entry::UniversalId::from_raw);
  mysql_row.unserialize_with_converter(&entry.url_host_id,
                                       entry::UniversalId::from_raw);
  mysql_row.unserialize(&entry.url_context_root);
  mysql_row.unserialize_with_converter(&entry.url_protocols,
                                       MySQLRow::set_from_string);
  mysql_row.unserialize(&entry.enabled);
  mysql_row.unserialize(&entry.comment);
  mysql_row.unserialize(&entry.options);
  mysql_row.unserialize(&entry.auth_path);
  mysql_row.unserialize(&entry.auth_completed_url);
  mysql_row.unserialize(&entry.auth_completed_url_validation);
  mysql_row.unserialize(&entry.auth_completed_page_content);
  mysql_row.unserialize(&entry.enable_sql_endpoint);
  mysql_row.unserialize(&entry.custom_metadata_schema);

  if (db_version_ >= mrs::interface::kSupportedMrsMetadataVersion_3) {
    mysql_row.unserialize(&entry.name);
    mysql_row.unserialize(&entry.metadata);
    mysql_row.unserialize(&entry.published);
  }

  entry.deleted = false;
}

}  // namespace database
}  // namespace mrs

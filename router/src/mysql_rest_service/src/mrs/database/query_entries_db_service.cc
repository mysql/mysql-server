/*
  Copyright (c) 2024, Oracle and/or its affiliates.

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

#include "mrs/database/query_entries_db_service.h"

#include <string>

#include "helper/mysql_row.h"
#include "mrs/database/helper/query_audit_log_maxid.h"
#include "mysql/harness/string_utils.h"

namespace mrs {
namespace database {

QueryEntriesDbService::QueryEntriesDbService(
    SupportedMrsMetadataVersion version)
    : db_version_{version} {
  query_ =
      "SELECT * FROM (SELECT"
      "  s.id, s.url_host_id, s.url_context_root, s.url_protocol,"
      "  s.enabled, s.comments, s.options,"
      "  s.auth_path, s.auth_completed_url, s.auth_completed_url_validation,"
      "  s.auth_completed_page_content, s.enable_sql_endpoint,"
      "  s.custom_metadata_schema !"
      " FROM mysql_rest_service_metadata.`service` as s ) as parent ";

  if (db_version_ >= mrs::interface::kSupportedMrsMetadataVersion_3)
    query_ << mysqlrouter::sqlstring{", s.published, s.in_development"};
  else
    query_ << mysqlrouter::sqlstring{""};
}

uint64_t QueryEntriesDbService::get_last_update() { return audit_log_id_; }

void QueryEntriesDbService::query_entries(MySQLSession *session) {
  entries.clear();

  QueryAuditLogMaxId query_audit_id;
  MySQLSession::Transaction transaction(session);

  auto audit_log_id = query_audit_id.query_max_id(session);
  execute(session);

  transaction.commit();

  audit_log_id_ = audit_log_id;
}

void QueryEntriesDbService::on_row(const ResultRow &row) {
  entries.emplace_back();

  helper::MySQLRow mysql_row(row, metadata_, num_of_metadata_);
  DbService &entry = entries.back();

  auto set_from_string = [](std::set<std::string> *out, const char *in) {
    out->clear();
    for (const auto &s : mysql_harness::split_string(in, ',', false)) {
      out->insert(s);
    }
  };

  mysql_row.unserialize_with_converter(&entry.id, entry::UniversalId::from_raw);
  mysql_row.unserialize_with_converter(&entry.url_host_id,
                                       entry::UniversalId::from_raw);
  mysql_row.unserialize(&entry.url_context_root);
  mysql_row.unserialize_with_converter(&entry.url_protocols, set_from_string);
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
    mysql_row.unserialize(&entry.published);
    mysql_row.unserialize(&entry.in_development);
  }

  entry.deleted = false;
}

}  // namespace database
}  // namespace mrs

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

#include "mrs/database/query_entries_auth_app.h"

#include "helper/mysql_row.h"
#include "mrs/database/helper/query_audit_log_maxid.h"

namespace mrs {
namespace database {

uint64_t QueryEntriesAuthApp::get_last_update() { return audit_log_id_; }

QueryEntriesAuthApp::QueryEntriesAuthApp() {
  query_ =
      "SELECT * FROM (SELECT a.id, service_id, s.url_context_root, v.name, "
      "a.name as app_name,  "
      "  a.enabled and "
      "    v.enabled, a.url, v.validation_url,  a.access_token, a.app_id, "
      "  CONCAT(IF(s.url_protocol=\"HTTPS\",\"https://\",\"http://\"),h.name) "
      "    as host, "
      "  CONCAT(IF(s.url_protocol=\"HTTPS\",\"https://\",\"http://\"),(select "
      "  a.alias from mysql_rest_service_metadata.`url_host_alias` as a where "
      "    h.id=a.url_host_id limit 1)) as host_alias,"
      "  a.url_direct_auth,"
      "  a.limit_to_registered_users, a.default_role_id,"
      "  s.auth_path, s.options, s.auth_completed_url, "
      "s.auth_completed_page_content, "
      "  a.id as auth_app_id, auth_vendor_id, h.id as url_host_id"
      " FROM mysql_rest_service_metadata.auth_app as a "
      "JOIN mysql_rest_service_metadata.`auth_vendor` as v on a.auth_vendor_id "
      "= v.id "
      "JOIN mysql_rest_service_metadata.service as s on a.service_id = s.id "
      "JOIN mysql_rest_service_metadata.url_host as h on s.url_host_id = h.id "
      ") as subtable ";
}

void QueryEntriesAuthApp::query_entries(MySQLSession *session) {
  QueryAuditLogMaxId query_audit_id;
  query(session, "START TRANSACTION");
  auto audit_log_id = query_audit_id.query_max_id(session);

  execute(session);
  query(session, "COMMIT");
  audit_log_id_ = audit_log_id;
}

void QueryEntriesAuthApp::on_row(const ResultRow &row) {
  entries.emplace_back();

  helper::MySQLRow mysql_row(row, metadata_, no_od_metadata_);
  AuthApp &entry = entries.back();

  mysql_row.unserialize_with_converter(&entry.id, entry::UniversalId::from_raw);
  mysql_row.unserialize_with_converter(&entry.service_id,
                                       entry::UniversalId::from_raw);
  mysql_row.unserialize(&entry.service_name);
  mysql_row.unserialize(&entry.vendor_name);
  mysql_row.unserialize(&entry.app_name);
  mysql_row.unserialize(&entry.active);
  mysql_row.unserialize(&entry.url);
  mysql_row.unserialize(&entry.url_validation);
  mysql_row.unserialize(&entry.app_token);
  mysql_row.unserialize(&entry.app_id);
  // TODO(lkotula): Take the host from SERVICE instance! (Shouldn't be in
  // review)
  mysql_row.unserialize(&entry.host);
  mysql_row.unserialize(&entry.host_alias);
  mysql_row.unserialize(&entry.url_access_token);
  mysql_row.unserialize(&entry.limit_to_registered_users);
  mysql_row.unserialize_with_converter(&entry.default_role_id,
                                       entry::UniversalId::from_raw);
  mysql_row.unserialize(&entry.auth_path);
  mysql_row.unserialize(&entry.options);
  mysql_row.unserialize(&entry.redirect);
  mysql_row.unserialize(&entry.redirection_default_page);
  mysql_row.skip();
  mysql_row.unserialize_with_converter(&entry.vendor_id,
                                       entry::UniversalId::from_raw);

  entry.deleted = false;
}

}  // namespace database
}  // namespace mrs

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

#include "mrs/database/query_entries_auth_app.h"

#include "helper/mysql_row.h"
#include "helper/string/hex.h"
#include "mrs/database/helper/query_audit_log_maxid.h"
#include "mysql/harness/string_utils.h"

namespace mrs {
namespace database {

using Entries = QueryEntriesAuthApp::Entries;

namespace v2 {

uint64_t QueryEntriesAuthApp::get_last_update() { return audit_log_id_; }

QueryEntriesAuthApp::QueryEntriesAuthApp() {
  query_ =
      "SELECT * FROM (SELECT a.id, HEX(service_id),"
      "v.name, "
      "a.name as app_name, "
      "  a.enabled and "
      "    v.enabled, a.url, v.validation_url,  a.access_token, a.app_id, "
      "  a.url_direct_auth,"
      "  a.limit_to_registered_users, a.default_role_id,"
      "  a.id as auth_app_id, auth_vendor_id "
      " FROM mysql_rest_service_metadata.auth_app as a "
      "JOIN mysql_rest_service_metadata.`auth_vendor` as v on a.auth_vendor_id "
      "= v.id "
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
  entries_.emplace_back();

  helper::MySQLRow mysql_row(row, metadata_, num_of_metadata_);
  AuthApp &entry = entries_.back();

  auto set_from_string = [](std::set<entry::UniversalId> *out, const char *in) {
    std::set<std::string> result;
    helper::MySQLRow::set_from_string(&result, in);

    out->clear();
    for (const auto &s : result) {
      auto binary =
          helper::string::unhex<std::string,
                                helper::string::get_unhex_character>(s);
      auto id = entry::UniversalId::from_cstr(binary.c_str(), binary.length());
      if (!id.empty()) out->insert(id);
    }
  };

  mysql_row.unserialize_with_converter(&entry.id, entry::UniversalId::from_raw);
  mysql_row.unserialize_with_converter(&entry.service_ids, set_from_string);
  mysql_row.unserialize(&entry.vendor_name);
  mysql_row.unserialize(&entry.app_name);
  mysql_row.unserialize(&entry.active);
  mysql_row.unserialize(&entry.url);
  mysql_row.unserialize(&entry.url_validation);
  mysql_row.unserialize(&entry.app_token);
  mysql_row.unserialize(&entry.app_id);
  mysql_row.unserialize(&entry.url_access_token);
  mysql_row.unserialize(&entry.limit_to_registered_users);
  mysql_row.unserialize_with_converter(&entry.default_role_id,
                                       entry::UniversalId::from_raw);
  // Field used for audit_log matching
  mysql_row.skip(/*a.id as auth_app_id*/);
  mysql_row.unserialize_with_converter(&entry.vendor_id,
                                       entry::UniversalId::from_raw);

  entry.deleted = false;
}

const Entries &QueryEntriesAuthApp::get_entries() { return entries_; }

}  // namespace v2

namespace v3 {

QueryEntriesAuthApp::QueryEntriesAuthApp() {
  mysqlrouter::sqlstring select_services{
      "(SELECT GROUP_CONCAT(DISTINCT HEX(`shaa`.`service_id`) ORDER BY "
      "auth_app_id ASC "
      "SEPARATOR ',')  FROM "
      "`mysql_rest_service_metadata`.`service_has_auth_app` as `shaa` "
      " WHERE `shaa`.`auth_app_id`=a.id "
      " GROUP BY `shaa`.`auth_app_id`)"};

  query_ =
      "SELECT * FROM (SELECT a.id,  ! ,"
      "v.name, "
      "a.name as app_name,  "
      "  a.enabled and "
      "    v.enabled, a.url, v.validation_url,  a.access_token, a.app_id, "
      "  a.url_direct_auth,"
      "  a.limit_to_registered_users, a.default_role_id,"
      "  a.id as auth_app_id, auth_vendor_id"
      " FROM mysql_rest_service_metadata.auth_app as a "
      "JOIN mysql_rest_service_metadata.`auth_vendor` as v on a.auth_vendor_id "
      "= v.id "
      ") as subtable ";

  query_ << select_services;
}

}  // namespace v3
}  // namespace database
}  // namespace mrs

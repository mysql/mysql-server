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

#include "mrs/database/query_entries_content_file.h"

#include "helper/mysql_row.h"
#include "mrs/database/helper/query_audit_log_maxid.h"

namespace mrs {
namespace database {

QueryEntriesContentFile::QueryEntriesContentFile() {
  query_ =
      "SELECT * FROM (SELECT f.id as content_file_id, content_set_id, f.size, "
      "   h.name, "
      "   service.url_context_root as service_path, s.request_path as "
      "   set_path, f.request_path as file_path,"
      "   service.enabled as srv_enabled, s.enabled as set_enabled, f.enabled "
      "as enabled,"
      "   s.requires_auth as set_requires_auth,f.requires_auth as "
      "   requires_auth, "
      "   s.service_id,"
      "    s.options as db_options, service.options as service_options,"
      "    h.id as url_host_id"
      " FROM mysql_rest_service_metadata.content_file as f"
      " JOIN mysql_rest_service_metadata.content_set as s ON "
      "f.content_set_id=s.id"
      " JOIN mysql_rest_service_metadata.service as service ON "
      "service.id=s.service_id"
      " JOIN mysql_rest_service_metadata.url_host as h ON "
      "h.id=service.url_host_id) as parent ";
}

uint64_t QueryEntriesContentFile::get_last_update() { return audit_log_id_; }

void QueryEntriesContentFile::query_entries(MySQLSession *session) {
  QueryAuditLogMaxId query_audit_id;

  entries.clear();

  query(session, "START TRANSACTION");
  auto audit_log_id = query_audit_id.query_max_id(session);
  execute(session);
  query(session, "COMMIT");

  audit_log_id_ = audit_log_id;
}

void QueryEntriesContentFile::on_row(const ResultRow &row) {
  entries.emplace_back();

  helper::MySQLRow mysql_row(row, metadata_, no_od_metadata_);
  auto &entry = entries.back();

  mysql_row.unserialize_with_converter(&entry.id, entry::UniversalId::from_raw);
  mysql_row.unserialize_with_converter(&entry.content_set_id,
                                       entry::UniversalId::from_raw);
  mysql_row.unserialize(&entry.size);
  mysql_row.unserialize(&entry.host);
  mysql_row.unserialize(&entry.service_path);
  mysql_row.unserialize(&entry.schema_path);
  mysql_row.unserialize(&entry.file_path);
  mysql_row.unserialize(&entry.active_service);
  mysql_row.unserialize(&entry.active_set);
  mysql_row.unserialize(&entry.active_file);
  mysql_row.unserialize(&entry.schema_requires_authentication);
  mysql_row.unserialize(&entry.requires_authentication);
  mysql_row.unserialize_with_converter(&entry.service_id,
                                       entry::UniversalId::from_raw);
  mysql_row.unserialize(&entry.options_json_schema);
  mysql_row.unserialize(&entry.options_json_service);

  entry.deleted = false;
}

}  // namespace database
}  // namespace mrs

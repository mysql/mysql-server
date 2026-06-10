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

#include "mrs/database/query_entries_content_file.h"

#include "helper/mysql_row.h"
#include "mrs/database/helper/query_audit_log_maxid.h"

namespace mrs {
namespace database {
const mysqlrouter::sqlstring k_option_field{",f.options "};
const mysqlrouter::sqlstring k_empty;

QueryEntriesContentFile::QueryEntriesContentFile(const Version version)
    : version_{version} {
  query_ =
      "SELECT * FROM (SELECT "
      "    f.id as content_file_id,"
      "    f.content_set_id as content_set_id,"
      "    f.request_path,"
      "    f.requires_auth,"
      "    f.enabled,"
      "    f.size !"
      " FROM mysql_rest_service_metadata.content_file as f) as cf";
  query_ << (version == Version::kSupportedMrsMetadataVersion_3 ? k_option_field
                                                                : k_empty);
}

uint64_t QueryEntriesContentFile::get_last_update() { return audit_log_id_; }

void QueryEntriesContentFile::query_entries(MySQLSession *session) {
  QueryAuditLogMaxId query_audit_id;

  entries.clear();

  auto audit_log_id = query_audit_id.query_max_id(session);
  execute(session);

  audit_log_id_ = audit_log_id;
}

void QueryEntriesContentFile::on_row(const ResultRow &row) {
  entries.emplace_back();

  helper::MySQLRow mysql_row(row, metadata_, num_of_metadata_);
  auto &entry = entries.back();

  mysql_row.unserialize_with_converter(&entry.id, entry::UniversalId::from_raw);
  mysql_row.unserialize_with_converter(&entry.content_set_id,
                                       entry::UniversalId::from_raw);
  mysql_row.unserialize(&entry.request_path);
  mysql_row.unserialize(&entry.requires_authentication);
  mysql_row.unserialize(&entry.enabled);
  mysql_row.unserialize(&entry.size);

  if (version_ == Version::kSupportedMrsMetadataVersion_3) {
    mysql_row.unserialize(&entry.options);
  }

  entry.deleted = false;
}

}  // namespace database
}  // namespace mrs

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

#include "mrs/database/query_entries_url_host.h"

#include <string>

#include "helper/mysql_row.h"
#include "mrs/database/helper/query_audit_log_maxid.h"
#include "mysql/harness/string_utils.h"

namespace mrs {
namespace database {

QueryEntriesUrlHost::QueryEntriesUrlHost() {
  query_ =
      "SELECT * FROM (SELECT h.id, h.name, GROUP_CONCAT(a.alias SEPARATOR ',')"
      "    FROM mysql_rest_service_metadata.`url_host` as h JOIN"
      "    mysql_rest_service_metadata.`url_host_alias` as a"
      "    on h.id = a.url_host_id"
      "    GROUP BY h.id, h.name) as parent ";
}

uint64_t QueryEntriesUrlHost::get_last_update() { return audit_log_id_; }

void QueryEntriesUrlHost::query_entries(MySQLSession *session) {
  entries.clear();

  QueryAuditLogMaxId query_audit_id;
  MySQLSession::Transaction transaction(session);

  auto audit_log_id = query_audit_id.query_max_id(session);
  execute(session);

  transaction.commit();

  audit_log_id_ = audit_log_id;
}

void QueryEntriesUrlHost::on_row(const ResultRow &row) {
  entries.emplace_back();

  helper::MySQLRow mysql_row(row, metadata_, num_of_metadata_);
  UrlHost &entry = entries.back();

  auto set_from_string = [](std::set<std::string> *out, const char *in) {
    out->clear();
    for (const auto &s : mysql_harness::split_string(in, ',', false)) {
      out->insert(s);
    }
  };

  mysql_row.unserialize_with_converter(&entry.id, entry::UniversalId::from_raw);
  mysql_row.unserialize(&entry.name);
  mysql_row.unserialize_with_converter(&entry.aliases, set_from_string);
}

}  // namespace database
}  // namespace mrs

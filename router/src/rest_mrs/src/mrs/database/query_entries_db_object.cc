/*
  Copyright (c) 2021, 2022, Oracle and/or its affiliates.

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

#include "mrs/database/query_entries_db_object.h"

#include <map>

#include "helper/mysql_row.h"
#include "mrs/database/helper/query_audit_log_maxid.h"
#include "mrs/database/query_entry_group_row_security.h"

namespace mrs {
namespace database {

QueryEntryDbObject::QueryEntryDbObject() {
  query_ =
      "SELECT * FROM (SELECT "
      "  o.id as id, s.id as sservice_id, db.id as schema_id, "
      "  h.name, (select a.alias from "
      "mysql_rest_service_metadata.`url_host_alias` as a where "
      "h.id=a.url_host_id limit 1) as alias, "
      "     o.requires_auth, db.requires_auth as schema_requires_auth, "
      "    (o.enabled and db.enabled and s.enabled) as `active`, "
      "    url_context_root as `service_path`, "
      "    db.request_path as `schema_path`, "
      "    o.request_path as `object_path`, "
      "    COALESCE(o.items_per_page, db.items_per_page) as `on_page`, "
      "    db.name as `db_schema`, "
      "    o.name as `db_table`, "
      " o.crud_operation + 0, "
      " o.format + 0, "
      " o.media_type, "
      " o.auto_detect_media_type, "
      "    s.id as service_id, o.id as db_object_id, db.id as db_schema_id, "
      "    h.id as url_host_id, o.object_type, o.row_user_ownership_enforced,"
      "    o.row_user_ownership_column"
      " FROM mysql_rest_service_metadata.`db_object` as o "
      "  JOIN mysql_rest_service_metadata.`db_schema` as db on "
      "      o.db_schema_id = db.id "
      "  JOIN mysql_rest_service_metadata.`service` as s on "
      "      db.service_id = s.id "
      "JOIN mysql_rest_service_metadata.`url_host` as h on "
      "s.url_host_id = h.id  ) as parent ";
}

uint64_t QueryEntryDbObject::get_last_update() { return audit_log_id_; }

void QueryEntryDbObject::query_entries(MySQLSession *session) {
  entries.clear();

  QueryAuditLogMaxId query_audit_id;
  query(session, "START TRANSACTION");
  auto audit_log_id = query_audit_id.query(session);
  query(session);

  QueryEntryGroupRowSecurity gs;
  for (auto &e : entries) {
    gs.query_group_row_security(session, e.id);
    e.row_group_security = std::move(gs.get_result());
  }

  query(session, "COMMIT");

  audit_log_id_ = audit_log_id;
}

template <typename Map>
auto get_map_converter(Map *map, const typename Map::mapped_type value) {
  return [map, value](auto *out, const char *v) {
    auto e = v ? map->find(v) : map->end();

    if (e != map->end()) {
      *out = e->second;
      return;
    }
    *out = value;
  };
}

void QueryEntryDbObject::on_row(const Row &row) {
  entries.emplace_back();

  static std::map<std::string, DbObject::PathType> path_types{
      {"TABLE", DbObject::typeTable}, {"PROCEDURE", DbObject::typeProcedure}};

  helper::MySQLRow mysql_row(row);
  DbObject &entry = entries.back();

  auto path_type_converter =
      get_map_converter(&path_types, DbObject::typeTable);
  mysql_row.unserialize(&entry.id);
  mysql_row.unserialize(&entry.service_id);
  mysql_row.unserialize(&entry.schema_id);
  mysql_row.unserialize(&entry.host);
  mysql_row.unserialize(&entry.host_alias);
  mysql_row.unserialize(&entry.requires_authentication);
  mysql_row.unserialize(&entry.schema_requires_authentication);
  mysql_row.unserialize(&entry.active);
  mysql_row.unserialize(&entry.service_path);
  mysql_row.unserialize(&entry.schema_path);
  mysql_row.unserialize(&entry.object_path);
  mysql_row.unserialize(&entry.on_page);
  mysql_row.unserialize(&entry.db_schema);
  mysql_row.unserialize(&entry.db_table);

  mysql_row.unserialize(&entry.operation);
  mysql_row.unserialize(&entry.format);
  mysql_row.unserialize(&entry.media_type);
  mysql_row.unserialize(&entry.autodetect_media_type);

  mysql_row.skip(4);
  mysql_row.unserialize_with_converter(&entry.type, path_type_converter);
  mysql_row.unserialize(&entry.row_security.user_ownership_enforced);
  mysql_row.unserialize(&entry.row_security.user_ownership_column);

  QueryEntryGroupRowSecurity group_security;
  //  group_security.

  entry.deleted = false;
}

}  // namespace database
}  // namespace mrs

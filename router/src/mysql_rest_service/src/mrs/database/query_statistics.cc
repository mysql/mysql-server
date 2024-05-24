/*
  Copyright (c) 2023, 2024, Oracle and/or its affiliates.

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

#include "mrs/database/query_statistics.h"

#include <vector>

#include "helper/container/generic.h"
#include "helper/json/serializer_to_text.h"
#include "mrs/router_observation_entities.h"

namespace mrs {
namespace database {

void QueryStatistics::update_statistics(MySQLSession *session,
                                        uint64_t router_id, uint64_t timespan,
                                        const Snapshot &snap) {
  query_.reset(
      "INSERT INTO "
      "mysql_rest_service_metadata.router_status("
      "router_id, timespan, mysql_connections, mysql_queries, "
      "http_requests_get, "
      "http_requests_post, "
      "http_requests_put, http_requests_delete, active_mysql_connections, "
      "details) VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");

  std::vector<uint32_t> direct_ids{
      kEntityCounterMySQLConnectionsCreated, kEntityCounterMySQLQueries,
      kEntityCounterHttpRequestGet,          kEntityCounterHttpRequestPost,
      kEntityCounterHttpRequestPut,          kEntityCounterHttpRequestDelete,
      kEntityCounterMySQLConnectionsActive};

  helper::json::SerializerToText stt;
  {
    auto obj = stt.add_object();
    for (size_t i = 0; i < snap.size(); ++i) {
      if (helper::container::has(direct_ids, i)) continue;
      if (snap[i].first.empty()) continue;

      obj->member_add_value(snap[i].first, snap[i].second);
    }
  }

  query_ << router_id << timespan;
  for (auto id : direct_ids) {
    query_ << snap[id].second;
  }

  query_ << stt.get_result();
  execute(session);
}

}  // namespace database
}  // namespace mrs

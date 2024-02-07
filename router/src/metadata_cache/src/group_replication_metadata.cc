/*
  Copyright (c) 2016, 2024, Oracle and/or its affiliates.

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

#include "group_replication_metadata.h"

#include <cassert>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <sstream>

#include "mysql/harness/logging/logging.h"
#include "mysqlrouter/metadata_cache.h"
#include "mysqlrouter/mysql_session.h"

using mysqlrouter::MySQLSession;
IMPORT_LOG_FUNCTIONS()

// throws metadata_cache::metadata_error
std::map<std::string, GroupReplicationMember> fetch_group_replication_members(
    MySQLSession &connection, bool &single_primary) {
  std::map<std::string, GroupReplicationMember> members;

  // replication_group_members.member_role field was introduced in 8.0.2, otoh
  // group_replication_primary_member gets removed in 8.3 so we need 2 different
  // queries depending on a server version
  const bool has_member_role_field = connection.server_version() >= 80002;
  std::string query_sql;
  if (has_member_role_field) {
    query_sql =
        "SELECT member_id, member_host, member_port, member_state, "
        "member_role, @@group_replication_single_primary_mode FROM "
        "performance_schema.replication_group_members"
        " WHERE channel_name = 'group_replication_applier'";
  } else {
    query_sql =
        "SELECT member_id, member_host, member_port, member_state, "
        "IF(g.primary_uuid = '' OR member_id = g.primary_uuid, 'PRIMARY', "
        "'SECONDARY') as member_role, @@group_replication_single_primary_mode "
        "FROM (SELECT IFNULL(variable_value, '') AS primary_uuid FROM "
        "performance_schema.global_status WHERE variable_name = "
        "'group_replication_primary_member') g, "
        "performance_schema.replication_group_members WHERE channel_name = "
        "'group_replication_applier'";
  }

  auto result_processor = [&members, &single_primary,
                           &query_sql](const MySQLSession::Row &row) -> bool {
    // clang-format off
    //
    // example response from node that is still part of GR (normally should see itself and all other GR members):
    // +--------------------------------------+-------------+-------------+--------------+-------------+-----------------------------------------+
    // | member_id                            | member_host | member_port | member_state | member_role | @@group_replication_single_primary_mode |
    // +--------------------------------------+-------------+-------------+--------------+-------------+-----------------------------------------|
    // | 02a397c0-3294-11ee-aa10-ded79a7fabde | 127.0.0.1   |        6000 | ONLINE       | PRIMARY     |                                       1 |
    // | 0a4aa3d8-3294-11ee-a664-aa061e8647d0 | 127.0.0.1   |        6001 | ONLINE       | SECONDARY   |                                       1 |
    // | 11e3d1b4-3294-11ee-ab38-d7b91b5c794c | 127.0.0.1   |        6002 | ONLINE       | SECONDARY   |                                       1 |
    // +--------------------------------------+-------------+-------------+--------------+-------------+-----------------------------------------+
    // clang-format on

    if (row.size() != 6) {  // TODO write a testcase for this
      throw metadata_cache::metadata_error(
          "Unexpected number of fields in resultset from group_replication "
          "query. "
          "Expected = 6, got = " +
          std::to_string(row.size()));
    }

    // read fields from row
    const char *member_id = row[0];
    const char *member_host = row[1];
    const char *member_port = row[2];
    const char *member_state = row[3];
    const char *member_role = row[4];
    single_primary =
        row[5] && (strcmp(row[5], "1") == 0 || strcmp(row[5], "ON") == 0);

    if (!member_id || !member_host || !member_port || !member_state ||
        !member_role) {
      log_warning("Query %s returned %s, %s, %s, %s, %s %s", query_sql.c_str(),
                  row[0], row[0], row[1], row[2], row[3], row[4]);
      throw metadata_cache::metadata_error(
          "Unexpected value in group_replication_metadata query results");
    }

    // populate GroupReplicationMember with data from row
    GroupReplicationMember member;
    member.member_id = member_id;
    member.host = member_host;
    member.port = static_cast<uint16_t>(std::atoi(member_port));
    if (std::strcmp(member_state, "ONLINE") == 0)
      member.state = GroupReplicationMember::State::Online;
    else if (std::strcmp(member_state, "OFFLINE") == 0)
      member.state = GroupReplicationMember::State::Offline;
    else if (std::strcmp(member_state, "UNREACHABLE") == 0)
      member.state = GroupReplicationMember::State::Unreachable;
    else if (std::strcmp(member_state, "RECOVERING") == 0)
      member.state = GroupReplicationMember::State::Recovering;
    else if (std::strcmp(member_state, "ERROR") == 0)
      member.state = GroupReplicationMember::State::Error;
    else {
      log_info("Unknown state %s in replication_group_members table for %s",
               member_state, member_id);
      member.state = GroupReplicationMember::State::Other;
    }

    member.role = (std::strcmp(member_role, "PRIMARY") == 0)
                      ? GroupReplicationMember::Role::Primary
                      : GroupReplicationMember::Role::Secondary;

    // add GroupReplicationMember to map that will be returned
    members[member_id] = member;

    return true;  // false = I don't want more rows
  };

  // get current topology (as seen by this node)
  try {
    connection.query(query_sql, result_processor);

  } catch (const MySQLSession::Error &e) {
    throw metadata_cache::metadata_error(e.what());
  } catch (const metadata_cache::metadata_error &) {
    throw;
  } catch (...) {
    assert(
        0);  // don't expect anything else to be thrown -> catch dev's attention
    throw;   // in production, rethrow anyway just in case
  }

  return members;
}

const char *to_string(GroupReplicationMember::State member_state) {
  switch (member_state) {
    case GroupReplicationMember::State::Online:
      return "Online";
    case GroupReplicationMember::State::Recovering:
      return "Recovering";
    case GroupReplicationMember::State::Unreachable:
      return "Unreachable";
    case GroupReplicationMember::State::Offline:
      return "Offline";
    case GroupReplicationMember::State::Error:
      return "Error";
    case GroupReplicationMember::State::Other:
        /* fallthrough  */;
  }

  return "Other";
}

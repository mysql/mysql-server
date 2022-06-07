/*
  Copyright (c) 2016, 2022, Oracle and/or its affiliates.

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
static std::string find_group_replication_primary_member(
    MySQLSession &connection) {
  // NOTE: In single-primary mode, this function will return primary node ID as
  //       seen by this node (provided this node is currently part of GR),
  //       but in multi-primary node, it will always return <empty>.
  //       Such is behavior of group_replication_primary_member variable.

  std::string primary_member;

  auto result_processor =
      [&primary_member](const MySQLSession::Row &row) -> bool {
    // Typical response is shown below. If this node is part of group
    // replication AND we're in SM mode, 'Value' will show the primary node,
    // else, it will be empty.
    // clang-format off
    // +----------------------------------+--------------------------------------+
    // | Variable_name                    | Value                                |
    // +----------------------------------+--------------------------------------+
    // | group_replication_primary_member | 3acfe4ca-861d-11e6-9e56-08002741aeb6 |
    // +----------------------------------+--------------------------------------+
    // clang-format on

    if (row.size() != 2) {  // TODO write a testcase for this
      throw metadata_cache::metadata_error(
          "Unexpected number of fields in the status response. "
          "Expected = 2, got = " +
          std::to_string(row.size()));
    }

    assert(!strcmp(row[0], "group_replication_primary_member"));
    primary_member = row[1] ? row[1] : "";
    return false;  // false = I don't want more rows
  };

  // Get primary node (as seen by this node). primary_member will contain ID of
  // the primary node (such as "3acfe4ca-861d-11e6-9e56-08002741aeb6"), or "" if
  // this node is not (currently) part of GR It will also be empty if we're
  // running GR in multi-primary mode.
  try {
    connection.query("show status like 'group_replication_primary_member'",
                     result_processor);
  } catch (const MySQLSession::Error &e) {
    throw metadata_cache::metadata_error(e.what());
  } catch (const metadata_cache::metadata_error &) {
    throw;
  } catch (...) {
    assert(
        0);  // don't expect anything else to be thrown -> catch dev's attention
    throw;   // in production, rethrow anyway just in case
  }

  return primary_member;
}

// throws metadata_cache::metadata_error
std::map<std::string, GroupReplicationMember> fetch_group_replication_members(
    MySQLSession &connection, bool &single_primary) {
  std::map<std::string, GroupReplicationMember> members;

  // who's the primary node? (throws metadata_cache::metadata_error)
  std::string primary_member =
      find_group_replication_primary_member(connection);

  auto result_processor = [&members, &primary_member, &single_primary](
                              const MySQLSession::Row &row) -> bool {
    // clang-format off
    // example response from node that left GR (sees only itself):
    // +--------------------------------------+-------------+-------------+--------------+-----------------------------------------+
    // | member_id                            | member_host | member_port | member_state | @@group_replication_single_primary_mode |
    // +--------------------------------------+-------------+-------------+--------------+-----------------------------------------+
    // | 30ec658e-861d-11e6-9988-08002741aeb6 | ubuntu      |        3310 | OFFLINE      |                                       1 |
    // +--------------------------------------+-------------+-------------+--------------+-----------------------------------------+
    //
    // example response from node that is still part of GR (normally should see itself and all other GR members):
    // +--------------------------------------+-------------+-------------+--------------+-----------------------------------------+
    // | member_id                            | member_host | member_port | member_state | @@group_replication_single_primary_mode |
    // +--------------------------------------+-------------+-------------+--------------+-----------------------------------------+
    // | 3acfe4ca-861d-11e6-9e56-08002741aeb6 | ubuntu      |        3320 | ONLINE       |                                       1 |
    // | 4c08b4a2-861d-11e6-a256-08002741aeb6 | ubuntu      |        3330 | ONLINE       |                                       1 |
    // +--------------------------------------+-------------+-------------+--------------+-----------------------------------------+
    // clang-format on

    if (row.size() != 5) {  // TODO write a testcase for this
      throw metadata_cache::metadata_error(
          "Unexpected number of fields in resultset from group_replication "
          "query. "
          "Expected = 5, got = " +
          std::to_string(row.size()));
    }

    // read fields from row
    const char *member_id = row[0];
    const char *member_host = row[1];
    const char *member_port = row[2];
    const char *member_state = row[3];
    single_primary =
        row[4] && (strcmp(row[4], "1") == 0 || strcmp(row[4], "ON") == 0);
    if (!member_id || !member_host || !member_port || !member_state) {
      log_warning("Query %s returned %s, %s, %s, %s, %s",
                  "SELECT member_id, member_host, member_port, member_state, "
                  "@@group_replication_single_primary_mode"
                  " FROM performance_schema.replication_group_members"
                  " WHERE channel_name = 'group_replication_applier'",
                  row[0], row[1], row[2], row[3], row[4]);
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

    // if single_primary == true, we're in single-primary mode, implying at most
    // 1 Primary(RW) node if single_primary == false, we're in multi-primary
    // mode, implying all nodes are Primary(RW)
    if (primary_member == member.member_id || !single_primary)
      member.role = GroupReplicationMember::Role::Primary;
    else
      member.role = GroupReplicationMember::Role::Secondary;

    // add GroupReplicationMember to map that will be returned
    members[member_id] = member;

    return true;  // false = I don't want more rows
  };

  // TODO optimisation some day:
  // Query executed in find_group_replication_primary_member() can be optimised
  // away by blending into the following query.  Unit tests will also have to be
  // updated.

  // get current topology (as seen by this node)
  try {
    connection.query(
        "SELECT member_id, member_host, member_port, member_state, "
        "@@group_replication_single_primary_mode"
        " FROM performance_schema.replication_group_members"
        " WHERE channel_name = 'group_replication_applier'",
        result_processor);

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

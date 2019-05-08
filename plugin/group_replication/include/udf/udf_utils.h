/* Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.

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
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef PLUGIN_GR_INCLUDE_UDF_UTILS_H
#define PLUGIN_GR_INCLUDE_UDF_UTILS_H

#include "my_dbug.h"
#include "plugin/group_replication/include/group_actions/group_action.h"
#include "plugin/group_replication/include/member_version.h"

const char *const member_offline_or_minority_str =
    "Member must be ONLINE and in the majority partition.";
const char *const unreachable_member_on_group_str =
    "All members in the group must be reachable.";
const char *const recovering_member_on_group_str =
    "A member is joining the group, wait for it to be ONLINE.";

/**
 * Result data type for user_has_gr_admin_privilege.
 * There are three cases:
 *
 * error: There was an error fetching the user's privileges
 * ok: The user has the required privileges
 * no_privilege: The user does *not* have the required privileges
 *
 * In the no_privilege case, the result contains the user's name and host for
 * the caller to create an helpful error message.
 */
enum class privilege_status { ok, no_privilege, error };
class privilege_result {
 public:
  privilege_status status;
  char const *get_user() const {
    DBUG_ASSERT(status == privilege_status::no_privilege &&
                "get_user() can only be called if status == no_privilege");
    return user;
  }
  char const *get_host() const {
    DBUG_ASSERT(status == privilege_status::no_privilege &&
                "get_host() can only be called if status == no_privilege");
    return host;
  }
  static privilege_result success() {
    return privilege_result(privilege_status::ok);
  }
  static privilege_result error() {
    return privilege_result(privilege_status::error);
  }
  static privilege_result no_privilege(char const *user, char const *host) {
    return privilege_result(user, host);
  }

 private:
  char const *user;
  char const *host;
  privilege_result(privilege_status status)
      : status(status), user(nullptr), host(nullptr) {
    DBUG_ASSERT(status != privilege_status::no_privilege &&
                "privilege_result(status) can only be called if status != "
                "no_privilege");
  }
  privilege_result(char const *user, char const *host)
      : status(privilege_status::no_privilege), user(user), host(host) {}
};

/**
  @class UDF_counter
  Class used to increase an atomic value when UDF functions are being
  initialized. If initialization fails the value will be decreased.

  number_udfs_running works together with plugin_is_stopping so when group
  replication is stopping, all new udf will fail to start and server will
  wait for the running ones to finish.
*/

class UDF_counter {
 public:
  static std::atomic<int> number_udfs_running;
  static void terminated() { number_udfs_running--; }
  static bool is_zero() { return number_udfs_running == 0; }

  UDF_counter() : success(false) { number_udfs_running++; }
  ~UDF_counter() {
    if (!success) number_udfs_running--;
  }

  void succeeded() { success = true; }

 private:
  bool success;
};

/**
 * Checks whether the user has GROUP_REPLICATION_ADMIN privilege.
 *
 * @retval privilege_result::error if there was an error fetching the user's
 * privileges
 * @retval privilege_result::no_privilege if the user does not have the
 * privilege
 * @retval privilege_result::success if the user has the privilege
 */
privilege_result user_has_gr_admin_privilege();

/**
 * Logs the privilege status of @c privilege into @c message.
 *
 * @param privilege the result of @c user_has_gr_admin_privilege()
 * @param[out] message the buffer where the log message will be written
 */
void log_privilege_status_result(privilege_result const &privilege,
                                 char *message);

/**
 * Checks whether the server is ONLINE and belongs to the majority partition.
 *
 * @retval true if the member is online and in the majority partition
 * @retval false otherwise
 */
bool member_online_with_majority();

/**
 * Checks if an unreachable member exists in the group
 *
 * @retval true if an unreachable member exists
 * @retval false otherwise
 */
bool group_contains_unreachable_member();

/**
 * Checks if a member in recovery exists in the group
 *
 * @retval true if a recovering member exists
 * @retval false otherwise
 */
bool group_contains_recovering_member();

/**
 * Logs the group action @c action_name result from @c result_area into
 * @c result_message.
 *
 * @param result_area describes the log message level
 * @param action_name group action name
 * @param[out] result_message buffer where the log message will be written
 * @param[out] length size of the log message written to @c result_message
 */
void log_group_action_result_message(Group_action_diagnostics *result_area,
                                     const char *action_name,
                                     char *result_message,
                                     unsigned long *length);

/**
 * Checks if tables are locked, and logs to @c message if so.
 *
 * @param[out] message buffer where the log message will be written to
 * @retval true if tables are not locked
 * @retval false if tables are locked (@c message is written to)
 */
bool check_locked_tables(char *message);

/**
 * Checks whether the group contains a member older than the specified version.
 *
 * @param min_required_version Minimum version required
 * @returns true if there is some older member, false otherwise
 */
bool group_contains_member_older_than(
    Member_version const &min_required_version);

#endif /* PLUGIN_GR_INCLUDE_UDF_UTILS_H */

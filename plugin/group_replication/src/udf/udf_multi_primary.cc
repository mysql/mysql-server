/* Copyright (c) 2018, 2023, Oracle and/or its affiliates.

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

#include "plugin/group_replication/include/udf/udf_multi_primary.h"
#include "plugin/group_replication/include/group_actions/multi_primary_migration_action.h"
#include "plugin/group_replication/include/plugin.h"
#include "plugin/group_replication/include/udf/udf_utils.h"

static char *group_replication_switch_to_multi_primary_mode(
    UDF_INIT *, UDF_ARGS *, char *result, unsigned long *length,
    unsigned char *is_null, unsigned char *error) {
  DBUG_TRACE;

  *is_null = 0;  // result is not null
  *error = 0;

  if (local_member_info && !local_member_info->in_primary_mode()) {
    const char *return_message = "The group is already on multi-primary mode.";
    size_t return_length = strlen(return_message);
    strcpy(result, return_message);
    *length = return_length;
    return result;
  }

  my_thread_id udf_thread_id = 0;
  if (current_thd) udf_thread_id = current_thd->thread_id();

  Multi_primary_migration_action group_action(udf_thread_id);

  Group_action_diagnostics execution_message_area;
  group_action_coordinator->coordinate_action_execution(
      &group_action, &execution_message_area,
      Group_action_message::ACTION_UDF_SWITCH_TO_MULTI_PRIMARY_MODE);
  if (log_group_action_result_message(
          &execution_message_area,
          "group_replication_switch_to_multi_primary_mode", result, length)) {
    *error = 1;
  }

  return result;
}

static bool group_replication_switch_to_multi_primary_mode_init(
    UDF_INIT *initid, UDF_ARGS *args, char *message) {
  DBUG_TRACE;

  /*
    Increment only after verifying the plugin is not stopping
    Do NOT increment before accessing volatile plugin structures.
    Stop is checked again after increment as the plugin might have stopped
  */
  if (get_plugin_is_stopping()) {
    std::snprintf(message, MYSQL_ERRMSG_SIZE, member_offline_or_minority_str);
    return true;
  }
  UDF_counter udf_counter;

  if (get_plugin_is_stopping()) {
    std::snprintf(message, MYSQL_ERRMSG_SIZE, member_offline_or_minority_str);
    return true;
  }

  if (args->arg_count > 0) {
    my_stpcpy(message, "Wrong arguments: This function takes no arguments.");
    return true;
  }

  privilege_result privilege = user_has_gr_admin_privilege();
  bool has_privileges = (privilege.status == privilege_status::ok);
  if (!has_privileges) {
    log_privilege_status_result(privilege, message);
    return true;
  }

  bool has_locked_tables = check_locked_tables(message);
  if (!has_locked_tables) return true;

  bool plugin_online = member_online_with_majority();
  if (!plugin_online) {
    std::snprintf(message, MYSQL_ERRMSG_SIZE, member_offline_or_minority_str);
    return true;
  }

  bool is_a_member_in_recovery = group_contains_recovering_member();
  if (is_a_member_in_recovery) {
    std::snprintf(message, MYSQL_ERRMSG_SIZE, recovering_member_on_group_str);
    return true;
  }

  bool is_a_member_unreachable = group_contains_unreachable_member();
  if (is_a_member_unreachable) {
    std::snprintf(message, MYSQL_ERRMSG_SIZE, unreachable_member_on_group_str);
    return true;
  }
  if (Charset_service::set_return_value_charset(initid)) return true;

  initid->maybe_null = false;
  udf_counter.succeeded();
  return false;
}

static void group_replication_switch_to_multi_primary_mode_deinit(UDF_INIT *) {
  UDF_counter::terminated();
}

udf_descriptor switch_to_multi_primary_udf() {
  return {"group_replication_switch_to_multi_primary_mode",
          Item_result::STRING_RESULT,
          reinterpret_cast<Udf_func_any>(
              group_replication_switch_to_multi_primary_mode),
          group_replication_switch_to_multi_primary_mode_init,
          group_replication_switch_to_multi_primary_mode_deinit};
}

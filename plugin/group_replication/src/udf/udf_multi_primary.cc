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

#include "plugin/group_replication/include/udf/udf_multi_primary.h"
#include "plugin/group_replication/include/group_actions/multi_primary_migration_action.h"
#include "plugin/group_replication/include/plugin.h"
#include "plugin/group_replication/include/udf/udf_utils.h"

static char *group_replication_switch_to_multi_primary_mode(
    UDF_INIT *, UDF_ARGS *, char *result, unsigned long *length,
    unsigned char *, unsigned char *) {
  DBUG_ENTER("group_replication_switch_to_multi_primary_mode");

  if (local_member_info && !local_member_info->in_primary_mode()) {
    const char *return_message = "The group is already on multi-primary mode.";
    size_t return_length = strlen(return_message);
    strcpy(result, return_message);
    *length = return_length;
    DBUG_RETURN(result);
  }

  my_thread_id udf_thread_id = 0;
  if (current_thd) udf_thread_id = current_thd->thread_id();

  Multi_primary_migration_action group_action(udf_thread_id);

  Group_action_diagnostics execution_message_area;
  group_action_coordinator->coordinate_action_execution(
      &group_action, &execution_message_area);
  log_group_action_result_message(
      &execution_message_area, "group_replication_switch_to_multi_primary_mode",
      result, length);

  DBUG_RETURN(result);
}

static bool group_replication_switch_to_multi_primary_mode_init(
    UDF_INIT *initid, UDF_ARGS *args, char *message) {
  DBUG_ENTER("group_replication_switch_to_multi_primary_mode_init");

  if (args->arg_count > 0) {
    my_stpcpy(message, "Wrong arguments: This function takes no arguments.");
    DBUG_RETURN(1);
  }

  privilege_result privilege = user_has_gr_admin_privilege();
  bool has_privileges = (privilege.status == privilege_status::ok);
  if (!has_privileges) {
    log_privilege_status_result(privilege, message);
    DBUG_RETURN(2);
  }

  bool has_locked_tables = check_locked_tables(message);
  if (!has_locked_tables) DBUG_RETURN(3);

  bool plugin_online = member_online_with_majority();
  if (!plugin_online) {
    std::snprintf(message, MYSQL_ERRMSG_SIZE, member_offline_or_minority_str);
    DBUG_RETURN(4);
  }

  initid->maybe_null = 0;
  DBUG_RETURN(0);
}

static void group_replication_switch_to_multi_primary_mode_deinit(UDF_INIT *) {}

udf_descriptor switch_to_multi_primary_udf() {
  return {"group_replication_switch_to_multi_primary_mode",
          Item_result::STRING_RESULT,
          reinterpret_cast<Udf_func_any>(
              group_replication_switch_to_multi_primary_mode),
          group_replication_switch_to_multi_primary_mode_init,
          group_replication_switch_to_multi_primary_mode_deinit};
}

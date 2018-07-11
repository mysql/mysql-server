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

#include "plugin/group_replication/include/udf/udf_single_primary.h"
#include "plugin/group_replication/include/group_actions/primary_election_action.h"
#include "plugin/group_replication/include/plugin.h"
#include "plugin/group_replication/include/udf/udf_utils.h"

static char *group_replication_set_as_primary(UDF_INIT *, UDF_ARGS *args,
                                              char *result,
                                              unsigned long *length,
                                              unsigned char *,
                                              unsigned char *) {
  DBUG_ENTER("group_replication_set_as_primary");

  size_t ulength = 0;
  if (!args->args[0] || !(ulength = args->lengths[0])) {
    const char *return_message =
        "Wrong arguments: You need to specify a server uuid.";
    size_t return_length = strlen(return_message);
    strcpy(result, return_message);
    *length = return_length;
    DBUG_RETURN(result);
  }

  // Double checking for dynamic values
  if (!binary_log::Uuid::is_valid(args->args[0], ulength)) {
    const char *return_message =
        "Wrong arguments: The server uuid is not valid.";
    size_t return_length = strlen(return_message);
    strcpy(result, return_message);
    *length = return_length;
    DBUG_RETURN(result);
  }

  std::string uuid = args->arg_count > 0 ? args->args[0] : "";
  if (group_member_mgr) {
    Group_member_info *member_info =
        group_member_mgr->get_group_member_info(uuid);
    if (member_info == nullptr) {
      const char *return_message =
          "The requested uuid is not a member of the group.";
      size_t return_length = strlen(return_message);
      strcpy(result, return_message);
      *length = return_length;
      DBUG_RETURN(result);
    } else {
      delete member_info;
    }
  }

  std::string current_primary_uuid;
  if (group_member_mgr->get_primary_member_uuid(current_primary_uuid)) {
    if (!current_primary_uuid.compare(uuid)) {
      const char *return_message =
          "The requested member is already the current group primary.";
      size_t return_length = strlen(return_message);
      strcpy(result, return_message);
      *length = return_length;
      DBUG_RETURN(result);
    }
  } else {
    // This case means the group changed to MPM since this UDF was initialized.
    const char *return_message =
        "The group is now in multi-primary mode. Use "
        "group_replication_switch_to_single_primary_mode.";
    size_t return_length = strlen(return_message);
    strcpy(result, return_message);
    *length = return_length;
    DBUG_RETURN(result);
  }

  my_thread_id udf_thread_id = 0;
  if (current_thd) udf_thread_id = current_thd->thread_id();

  Primary_election_action group_action(uuid, udf_thread_id);
  Group_action_diagnostics execution_message_area;
  group_action_coordinator->coordinate_action_execution(
      &group_action, &execution_message_area);
  log_group_action_result_message(&execution_message_area,
                                  "group_replication_set_as_primary", result,
                                  length);

  DBUG_RETURN(result);
}

static bool group_replication_set_as_primary_init(UDF_INIT *init_id,
                                                  UDF_ARGS *args,
                                                  char *message) {
  DBUG_ENTER("group_replication_set_as_primary_init");

  if (args->arg_count != 1 || args->arg_type[0] != STRING_RESULT ||
      args->lengths[0] == 0) {
    my_stpcpy(message, "Wrong arguments: You need to specify a server uuid.");
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

  const char *uuid = args->args[0];
  // We can do this test here for dynamic values (e.g.: SQL query values)
  if (uuid != nullptr) {
    size_t length = 0;
    if (uuid) length = strlen(uuid);
    if (!binary_log::Uuid::is_valid(uuid, length)) {
      my_stpcpy(message, "Wrong arguments: The server uuid is not valid.");
      DBUG_RETURN(5);
    }

    if (group_member_mgr) {
      Group_member_info *member_info =
          group_member_mgr->get_group_member_info(uuid);
      if (member_info == NULL) {
        const char *return_message =
            "The requested uuid is not a member of the group.";
        strcpy(message, return_message);
        DBUG_RETURN(6);
      } else {
        delete member_info;
      }
    }
  }

  if (local_member_info && !local_member_info->in_primary_mode()) {
    const char *return_message =
        "In multi-primary mode."
        " Use group_replication_switch_to_single_primary_mode.";
    strcpy(message, return_message);
    DBUG_RETURN(7);
  }

  init_id->maybe_null = 0;
  DBUG_RETURN(0);
}

static void group_replication_set_as_primary_deinit(UDF_INIT *) {}

udf_descriptor set_as_primary_udf() {
  return {"group_replication_set_as_primary", Item_result::STRING_RESULT,
          reinterpret_cast<Udf_func_any>(group_replication_set_as_primary),
          group_replication_set_as_primary_init,
          group_replication_set_as_primary_deinit};
}

static char *group_replication_switch_to_single_primary_mode(
    UDF_INIT *, UDF_ARGS *args, char *result, unsigned long *length,
    unsigned char *, unsigned char *) {
  DBUG_ENTER("group_replication_switch_to_single_primary_mode");

  if (local_member_info && local_member_info->in_primary_mode()) {
    const char *return_message;
    if (args->arg_count > 0)
      return_message =
          "Already in single-primary mode."
          " Did you mean to use group_replication_set_as_primary?";
    else
      return_message = "The group is already on single-primary mode.";

    size_t return_length = strlen(return_message);
    strcpy(result, return_message);
    *length = return_length;

    DBUG_RETURN(result);
  }

  std::string uuid =
      (args->arg_count == 1 && args->args[0] != nullptr) ? args->args[0] : "";
  if (args->arg_count > 0) {
    size_t ulength = 0;
    if (!args->args[0] || !(ulength = args->lengths[0])) {
      const char *return_message =
          "Wrong arguments: You need to specify a server uuid.";
      size_t return_length = strlen(return_message);
      strcpy(result, return_message);
      *length = return_length;
      DBUG_RETURN(result);
    }

    // Double checking for dynamic values
    if (!binary_log::Uuid::is_valid(args->args[0], ulength)) {
      const char *return_message =
          "Wrong arguments: The server uuid is not valid.";
      size_t return_length = strlen(return_message);
      strcpy(result, return_message);
      *length = return_length;
      DBUG_RETURN(result);
    }

    if (group_member_mgr) {
      Group_member_info *member_info =
          group_member_mgr->get_group_member_info(uuid);
      if (member_info == nullptr) {
        const char *return_message =
            "The requested uuid is not a member of the group.";
        size_t return_length = strlen(return_message);
        strcpy(result, return_message);
        *length = return_length;
        DBUG_RETURN(result);
      } else {
        delete member_info;
      }
    }
  }

  my_thread_id udf_thread_id = 0;
  if (current_thd) udf_thread_id = current_thd->thread_id();
  Primary_election_action group_action(uuid, udf_thread_id);

  Group_action_diagnostics execution_message_area;
  group_action_coordinator->coordinate_action_execution(
      &group_action, &execution_message_area);
  log_group_action_result_message(
      &execution_message_area,
      "group_replication_switch_to_single_primary_mode", result, length);

  DBUG_RETURN(result);
}

static bool group_replication_switch_to_single_primary_mode_init(
    UDF_INIT *initid, UDF_ARGS *args, char *message) {
  DBUG_ENTER("group_replication_switch_to_single_primary_mode_init");

  if (args->arg_count > 1 ||
      (args->arg_count == 1 &&
       (args->arg_type[0] != STRING_RESULT || args->lengths[0] == 0))) {
    my_stpcpy(message,
              "Wrong arguments: This function either takes no arguments"
              " or a single server uuid.");
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

  // We can do this test here for dynamic values (e.g.: SQL query values)
  if (args->arg_count == 1 && args->args[0] != nullptr) {
    const char *uuid = args->args[0];
    size_t length = strlen(uuid);
    if (length == 0 || !binary_log::Uuid::is_valid(uuid, length)) {
      my_stpcpy(message, "Wrong arguments: The server uuid is not valid.");
      DBUG_RETURN(5);
    }

    if (group_member_mgr) {
      Group_member_info *member_info =
          group_member_mgr->get_group_member_info(uuid);
      if (member_info == NULL) {
        const char *return_message =
            "The requested uuid is not a member of the group.";
        strcpy(message, return_message);
        DBUG_RETURN(6);
      } else {
        delete member_info;
      }
    }
  }

  initid->maybe_null = 0;
  DBUG_RETURN(0);
}

static void group_replication_switch_to_single_primary_mode_deinit(UDF_INIT *) {
}

udf_descriptor switch_to_single_primary_udf() {
  return {"group_replication_switch_to_single_primary_mode",
          Item_result::STRING_RESULT,
          reinterpret_cast<Udf_func_any>(
              group_replication_switch_to_single_primary_mode),
          group_replication_switch_to_single_primary_mode_init,
          group_replication_switch_to_single_primary_mode_deinit};
}

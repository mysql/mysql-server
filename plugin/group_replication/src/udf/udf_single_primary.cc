/* Copyright (c) 2018, 2019, Oracle and/or its affiliates. All rights reserved.

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
  DBUG_TRACE;

  size_t ulength = 0;
  if (!args->args[0] || !(ulength = args->lengths[0])) {
    const char *return_message =
        "Wrong arguments: You need to specify a server uuid.";
    size_t return_length = strlen(return_message);
    strcpy(result, return_message);
    *length = return_length;
    return result;
  }

  // Double checking for dynamic values
  if (!binary_log::Uuid::is_valid(args->args[0], ulength)) {
    const char *return_message =
        "Wrong arguments: The server uuid is not valid.";
    size_t return_length = strlen(return_message);
    strcpy(result, return_message);
    *length = return_length;
    return result;
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
      return result;
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
      return result;
    }
  } else {
    // This case means the group changed to MPM since this UDF was initialized.
    const char *return_message =
        "The group is now in multi-primary mode. Use "
        "group_replication_switch_to_single_primary_mode.";
    size_t return_length = strlen(return_message);
    strcpy(result, return_message);
    *length = return_length;
    return result;
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

  return result;
}

static bool group_replication_set_as_primary_init(UDF_INIT *init_id,
                                                  UDF_ARGS *args,
                                                  char *message) {
  DBUG_TRACE;

  UDF_counter udf_counter;

  if (get_plugin_is_stopping()) {
    std::snprintf(message, MYSQL_ERRMSG_SIZE, member_offline_or_minority_str);
    return true;
  }

  if (args->arg_count != 1 || args->arg_type[0] != STRING_RESULT ||
      args->lengths[0] == 0) {
    my_stpcpy(message, "Wrong arguments: You need to specify a server uuid.");
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

  const char *uuid = args->args[0];
  // We can do this test here for dynamic values (e.g.: SQL query values)
  if (uuid != nullptr) {
    size_t length = 0;
    if (uuid) length = strlen(uuid);
    if (!binary_log::Uuid::is_valid(uuid, length)) {
      my_stpcpy(message, "Wrong arguments: The server uuid is not valid.");
      return true;
    }

    if (group_member_mgr) {
      Group_member_info *member_info =
          group_member_mgr->get_group_member_info(uuid);
      if (member_info == NULL) {
        const char *return_message =
            "The requested uuid is not a member of the group.";
        strcpy(message, return_message);
        return true;
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
    return true;
  }

  init_id->maybe_null = 0;
  udf_counter.succeeded();
  return false;
}

static void group_replication_set_as_primary_deinit(UDF_INIT *) {
  UDF_counter::terminated();
}

udf_descriptor set_as_primary_udf() {
  return {"group_replication_set_as_primary", Item_result::STRING_RESULT,
          reinterpret_cast<Udf_func_any>(group_replication_set_as_primary),
          group_replication_set_as_primary_init,
          group_replication_set_as_primary_deinit};
}

static char *group_replication_switch_to_single_primary_mode(
    UDF_INIT *, UDF_ARGS *args, char *result, unsigned long *length,
    unsigned char *, unsigned char *) {
  DBUG_TRACE;

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

    return result;
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
      return result;
    }

    // Double checking for dynamic values
    if (!binary_log::Uuid::is_valid(args->args[0], ulength)) {
      const char *return_message =
          "Wrong arguments: The server uuid is not valid.";
      size_t return_length = strlen(return_message);
      strcpy(result, return_message);
      *length = return_length;
      return result;
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
        return result;
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

  return result;
}

static bool group_replication_switch_to_single_primary_mode_init(
    UDF_INIT *initid, UDF_ARGS *args, char *message) {
  DBUG_TRACE;

  UDF_counter udf_counter;

  if (get_plugin_is_stopping()) {
    std::snprintf(message, MYSQL_ERRMSG_SIZE, member_offline_or_minority_str);
    return true;
  }

  DBUG_EXECUTE_IF("group_replication_hold_udf_after_plugin_is_stopping", {
    const char act[] =
        "now signal signal.group_replication_resume_udf "
        "wait_for signal.group_replication_resume_udf_continue";
    DBUG_ASSERT(!debug_sync_set_action(current_thd, STRING_WITH_LEN(act)));
  });

  if (args->arg_count > 1 ||
      (args->arg_count == 1 &&
       (args->arg_type[0] != STRING_RESULT || args->lengths[0] == 0))) {
    my_stpcpy(message,
              "Wrong arguments: This function either takes no arguments"
              " or a single server uuid.");
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

  // We can do this test here for dynamic values (e.g.: SQL query values)
  if (args->arg_count == 1 && args->args[0] != nullptr) {
    const char *uuid = args->args[0];
    size_t length = strlen(uuid);
    if (length == 0 || !binary_log::Uuid::is_valid(uuid, length)) {
      my_stpcpy(message, "Wrong arguments: The server uuid is not valid.");
      return true;
    }

    if (group_member_mgr) {
      Group_member_info *member_info =
          group_member_mgr->get_group_member_info(uuid);
      if (member_info == NULL) {
        const char *return_message =
            "The requested uuid is not a member of the group.";
        strcpy(message, return_message);
        return true;
      } else {
        delete member_info;
      }
    }
  }

  initid->maybe_null = 0;
  udf_counter.succeeded();
  return false;
}

static void group_replication_switch_to_single_primary_mode_deinit(UDF_INIT *) {
  UDF_counter::terminated();
}

udf_descriptor switch_to_single_primary_udf() {
  return {"group_replication_switch_to_single_primary_mode",
          Item_result::STRING_RESULT,
          reinterpret_cast<Udf_func_any>(
              group_replication_switch_to_single_primary_mode),
          group_replication_switch_to_single_primary_mode_init,
          group_replication_switch_to_single_primary_mode_deinit};
}

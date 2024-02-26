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

#include "plugin/group_replication/include/udf/udf_single_primary.h"
#include "plugin/group_replication/include/group_actions/primary_election_action.h"
#include "plugin/group_replication/include/plugin.h"
#include "plugin/group_replication/include/udf/udf_utils.h"

static char *group_replication_set_as_primary(UDF_INIT *, UDF_ARGS *args,
                                              char *result,
                                              unsigned long *length,
                                              unsigned char *is_null,
                                              unsigned char *error) {
  DBUG_TRACE;

  const char *action_name = "group_replication_set_as_primary";
  *is_null = 0;  // result is not null
  *error = 0;

  std::string uuid =
      (args->arg_count >= 1 && args->args[0] != nullptr) ? args->args[0] : "";
  size_t ulength = (args->arg_count > 0) ? args->lengths[0] : 0;
  if (args->arg_count > 0) {
    const char *return_message = nullptr;
    bool invalid_uuid = validate_uuid_parameter(uuid, ulength, &return_message);

    if (invalid_uuid) {
      *error = 1;
      throw_udf_error(action_name, return_message);
      return result;
    }
  }
  int32 running_transactions_timeout =
      ((args->arg_count >= 2 && args->args[1] != nullptr)
           ? (*reinterpret_cast<long long *>(args->args[1]))
           : -1);
  if (args->arg_count >= 2 && (running_transactions_timeout < 0 ||
                               running_transactions_timeout > 3600)) {
    throw_udf_error(
        "group_replication_set_as_primary",
        "Valid range for running_transactions_timeout is 0 to 3600.");
    *error = 1;
    return result;
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

  Primary_election_action group_action(uuid, udf_thread_id,
                                       running_transactions_timeout);
  Group_action_diagnostics execution_message_area;
  group_action_coordinator->coordinate_action_execution(
      &group_action, &execution_message_area,
      Group_action_message::ACTION_UDF_SET_PRIMARY);
  if (log_group_action_result_message(&execution_message_area, action_name,
                                      result, length)) {
    *error = 1;
  }

  return result;
}

static bool group_replication_set_as_primary_init(UDF_INIT *init_id,
                                                  UDF_ARGS *args,
                                                  char *message) {
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
  if (args->arg_count > 2) {
    my_stpcpy(message, "Wrong arguments: UDF accepts maximum of 2 parameters.");
    return true;
  }
  if (args->arg_count == 0 || args->lengths[0] == 0 ||
      args->arg_type[0] != STRING_RESULT) {
    my_stpcpy(message, "Wrong arguments: You need to specify a server uuid.");
    return true;
  }
  if (args->arg_count >= 2 && args->arg_type[1] != INT_RESULT) {
    my_stpcpy(
        message,
        "Wrong arguments: Second parameter `running_transactions_timeout` must "
        "be type integer between 0 - 3600 (seconds).");
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

  const char *uuid_arg = args->args[0];
  if (uuid_arg != nullptr) {
    size_t ulength = args->lengths[0];  // We have validated length > 0
    std::string uuid = args->args[0];
    const char *return_message = nullptr;
    bool invalid_uuid = validate_uuid_parameter(uuid, ulength, &return_message);

    if (invalid_uuid) {
      my_stpcpy(message, return_message);
      return true;
    }
  }

  if (args->arg_count >= 2) {
    Group_member_info_list *all_members_info =
        (group_member_mgr == nullptr ? nullptr
                                     : group_member_mgr->get_all_members());
    bool is_version_lower_for_running_transactions_timeout = false;
    Member_version version_introducing_running_transactions_timeout(
        MEMBER_VERSION_INTRODUCING_RUNNING_TRANSACTION_TIMEOUT);
    for (Group_member_info *member : *all_members_info) {
      if (member->get_member_version() <
          version_introducing_running_transactions_timeout) {
        is_version_lower_for_running_transactions_timeout = true;
      }
      delete member;
    }
    delete all_members_info;
    if (is_version_lower_for_running_transactions_timeout) {
      const char *return_message =
          "The optional timeout argument in group_replication_set_as_primary() "
          "UDF is only supported when all group members have version 8.0.29 or "
          "higher.";
      strcpy(message, return_message);
      return true;
    }
  }

  if (local_member_info && !local_member_info->in_primary_mode()) {
    const char *return_message =
        "In multi-primary mode."
        " Use group_replication_switch_to_single_primary_mode.";
    strcpy(message, return_message);
    return true;
  }

  if (Charset_service::set_return_value_charset(init_id) ||
      Charset_service::set_args_charset(args))
    return true;

  init_id->maybe_null = false;
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
    unsigned char *is_null, unsigned char *error) {
  DBUG_TRACE;

  const char *action_name = "group_replication_switch_to_single_primary_mode";
  *is_null = 0;  // result is not null
  *error = 0;

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
  size_t ulength = (args->arg_count > 0) ? args->lengths[0] : 0;
  if (args->arg_count > 0) {
    const char *return_message = nullptr;
    bool invalid_uuid = validate_uuid_parameter(uuid, ulength, &return_message);

    if (invalid_uuid) {
      *error = 1;
      throw_udf_error(action_name, return_message);
      return result;
    }
  }

  my_thread_id udf_thread_id = 0;
  if (current_thd) udf_thread_id = current_thd->thread_id();
  Primary_election_action group_action(uuid, udf_thread_id);

  Group_action_diagnostics execution_message_area;
  group_action_coordinator->coordinate_action_execution(
      &group_action, &execution_message_area,
      uuid.empty()
          ? Group_action_message::ACTION_UDF_SWITCH_TO_SINGLE_PRIMARY_MODE
          : Group_action_message::
                ACTION_UDF_SWITCH_TO_SINGLE_PRIMARY_MODE_UUID);
  if (log_group_action_result_message(&execution_message_area, action_name,
                                      result, length)) {
    *error = 1;
  }

  return result;
}

static bool group_replication_switch_to_single_primary_mode_init(
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

  DBUG_EXECUTE_IF("group_replication_hold_udf_after_plugin_is_stopping", {
    const char act[] =
        "now signal signal.group_replication_resume_udf "
        "wait_for signal.group_replication_resume_udf_continue";
    assert(!debug_sync_set_action(current_thd, STRING_WITH_LEN(act)));
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
    std::string uuid =
        (args->arg_count == 1 && args->args[0] != nullptr) ? args->args[0] : "";
    size_t ulength = args->lengths[0];
    if (args->arg_count > 0) {
      const char *return_message = nullptr;
      bool invalid_uuid =
          validate_uuid_parameter(uuid, ulength, &return_message);

      if (invalid_uuid) {
        my_stpcpy(message, return_message);
        return true;
      }
    }
  }
  if (Charset_service::set_return_value_charset(initid) ||
      Charset_service::set_args_charset(args))
    return true;

  initid->maybe_null = false;
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

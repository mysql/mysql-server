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

#include "plugin/group_replication/include/plugin_udf_functions.h"
#include <include/mysql/components/services/dynamic_privilege.h>
#include <include/mysql/components/services/udf_registration.h>
#include <mysql/components/my_service.h>
#include <mysql/plugin.h>
#include "plugin/group_replication/include/group_actions/multi_primary_migration_action.h"
#include "plugin/group_replication/include/group_actions/primary_election_action.h"
#include "plugin/group_replication/include/plugin.h"
#include "sql/auth/auth_acls.h"
#include "sql/debug_sync.h"

typedef std::tuple<const char *, enum Item_result, Udf_func_any, Udf_func_init,
                   Udf_func_deinit>
    udf_function_tuple;

void log_result_message(Group_action_diagnostics *result_area,
                        const char *action_name, char *result_message,
                        unsigned long *length) {
  switch (result_area->get_execution_message_level()) {
    case Group_action_diagnostics::GROUP_ACTION_LOG_ERROR:
      my_error(ER_GRP_RPL_UDF_ERROR, MYF(ME_ERRORLOG), action_name,
               result_area->get_execution_message().c_str());
      break;
    case Group_action_diagnostics::GROUP_ACTION_LOG_WARNING:
      my_stpcpy(result_message, result_area->get_execution_message().c_str());
      *length = result_area->get_execution_message().length();

      if (current_thd)
        push_warning(current_thd, Sql_condition::SL_WARNING,
                     ER_GRP_RPL_UDF_ERROR,
                     result_area->get_warning_message().c_str());
      break;
    case Group_action_diagnostics::GROUP_ACTION_LOG_INFO:
      my_stpcpy(result_message, result_area->get_execution_message().c_str());
      *length = result_area->get_execution_message().length();
      break;
    default:
      /* purecov: begin inspected */
      std::string result = "The operation ";
      result.append(action_name);
      result.append("completed successfully");
      my_stpcpy(result_message, result.c_str());
      *length = result.length();
      /* purecov: end */
  }
}

bool check_user_privileges(char *message) {
  THD *thd = current_thd;

  if (!thd) return false;

  // if not a super user
  if (!(thd->security_context() &&
        thd->security_context()->master_access() & SUPER_ACL)) {
    SERVICE_TYPE(registry) *plugin_reg = mysql_plugin_registry_acquire();

    if (!plugin_reg) {
      /* purecov: begin inspected */
      std::stringstream ss;
      ss << " Error checking the user privileges. Check the log for more "
            "details or restart the server.";
      ss.getline(message, MAX_FIELD_WIDTH, '\0');
      return false;
      /* purecov: end */
    }

    bool has_global_grant = false;
    {
      my_service<SERVICE_TYPE(global_grants_check)> service(
          "global_grants_check", plugin_reg);
      if (service.is_valid()) {
        has_global_grant = service->has_global_grant(
            reinterpret_cast<Security_context_handle>(thd->security_context()),
            STRING_WITH_LEN("GROUP_REPLICATION_ADMIN"));
      }
      /* As my_service goes out of scope the service is unregistered using r */
    }
    mysql_plugin_registry_release(plugin_reg);
    if (!has_global_grant) {
      std::stringstream ss;
      ss << " User '" << thd->security_context()->priv_user().str << "'@'"
         << thd->security_context()->priv_host().str
         << "'. needs SUPER or GROUP_REPLICATION_ADMIN privileges.";
      ss.getline(message, MAX_FIELD_WIDTH, '\0');

      return false;
    }
  }  // end if !super_user
  return true;
}

bool check_locked_tables(char *message) {
  THD *thd = current_thd;

  if (!thd) return false;

  if (thd && thd->locked_tables_mode) {
    std::stringstream ss;
    ss << "Can't execute the given operation because you have active locked "
          "tables.";
    ss.getline(message, MAX_FIELD_WIDTH, '\0');
    return false;
  }
  return true;
}

bool check_member_status(char *message) {
  Mutex_autolock auto_lock_mutex(get_plugin_running_lock());
  std::string error = "";

  if (!plugin_is_group_replication_running() || local_member_info == NULL ||
      local_member_info->get_recovery_status() ==
          Group_member_info::MEMBER_OFFLINE ||
      local_member_info->get_recovery_status() ==
          Group_member_info::MEMBER_IN_RECOVERY ||
      local_member_info->get_recovery_status() ==
          Group_member_info::MEMBER_ERROR ||
      (group_partition_handler != NULL &&
       group_partition_handler->is_member_on_partition())) {
    error.assign("The member needs to be ONLINE and in a reachable partition.");
  }

  if (error.empty()) return true;

  std::stringstream ss;
  ss << error;
  ss.getline(message, MAX_FIELD_WIDTH, '\0');
  return false;
}

static char *group_replication_set_as_primary(UDF_INIT *, UDF_ARGS *args,
                                              char *result,
                                              unsigned long *length, char *,
                                              char *) {
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

  // Real implementation

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
  } else {  // This case means the group change to MPM while the UDF is inited
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

  Group_action *group_action = new Primary_election_action(uuid, udf_thread_id);
  Group_action_diagnostics *execution_message_area =
      new Group_action_diagnostics();
  group_action_coordinator->coordinate_action_execution(group_action,
                                                        execution_message_area);
  log_result_message(execution_message_area, "group_replication_set_as_primary",
                     result, length);
  delete execution_message_area;
  delete group_action;

  DBUG_RETURN(result);
}

static bool group_replication_set_as_primary_init(UDF_INIT *initid,
                                                  UDF_ARGS *args,
                                                  char *message) {
  DBUG_ENTER("group_replication_set_as_primary_init");

  if (args->arg_count != 1 || args->arg_type[0] != STRING_RESULT ||
      args->lengths[0] == 0) {
    my_stpcpy(message, "Wrong arguments: You need to specify a server uuid.");
    DBUG_RETURN(1);
  }

  bool has_privileges = check_user_privileges(message);
  if (!has_privileges) DBUG_RETURN(2);

  bool has_locked_tables = check_locked_tables(message);
  if (!has_locked_tables) DBUG_RETURN(3);

  bool plugin_online = check_member_status(message);
  if (!plugin_online) DBUG_RETURN(4);

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

  initid->maybe_null = 0;
  DBUG_RETURN(0);
}

static void group_replication_set_as_primary_deinit(UDF_INIT *) {}

static char *group_replication_switch_to_single_primary_mode(
    UDF_INIT *, UDF_ARGS *args, char *result, unsigned long *length, char *,
    char *) {
  DBUG_ENTER("group_replication_switch_to_single_primary_mode");

  if (local_member_info && local_member_info->in_primary_mode()) {
    const char *return_message;
    if (args->arg_count > 0)
      return_message =
          "Already in single-primary mode."
          " Did you meant to use group_replication_set_as_primary.";
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
  Group_action *group_action = new Primary_election_action(uuid, udf_thread_id);

  Group_action_diagnostics *execution_message_area =
      new Group_action_diagnostics();
  group_action_coordinator->coordinate_action_execution(group_action,
                                                        execution_message_area);
  log_result_message(execution_message_area,
                     "group_replication_switch_to_single_primary_mode", result,
                     length);
  delete execution_message_area;
  delete group_action;

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

  bool has_privileges = check_user_privileges(message);
  if (!has_privileges) DBUG_RETURN(2);

  bool has_locked_tables = check_locked_tables(message);
  if (!has_locked_tables) DBUG_RETURN(3);

  bool plugin_online = check_member_status(message);
  if (!plugin_online) DBUG_RETURN(4);

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

static char *group_replication_switch_to_multi_primary_mode(
    UDF_INIT *, UDF_ARGS *, char *result, unsigned long *length, char *,
    char *) {
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

  Group_action *group_action =
      new Multi_primary_migration_action(udf_thread_id);

  Group_action_diagnostics *execution_message_area =
      new Group_action_diagnostics();
  group_action_coordinator->coordinate_action_execution(group_action,
                                                        execution_message_area);
  log_result_message(execution_message_area,
                     "group_replication_switch_to_multi_primary_mode", result,
                     length);
  delete execution_message_area;
  delete group_action;

  DBUG_RETURN(result);
}

static bool group_replication_switch_to_multi_primary_mode_init(
    UDF_INIT *initid, UDF_ARGS *args, char *message) {
  DBUG_ENTER("group_replication_switch_to_multi_primary_mode_init");

  if (args->arg_count > 0) {
    my_stpcpy(message, "Wrong arguments: This function takes no arguments.");
    DBUG_RETURN(1);
  }

  bool has_privileges = check_user_privileges(message);
  if (!has_privileges) DBUG_RETURN(2);

  bool has_locked_tables = check_locked_tables(message);
  if (!has_locked_tables) DBUG_RETURN(3);

  bool plugin_online = check_member_status(message);
  if (!plugin_online) DBUG_RETURN(4);

  initid->maybe_null = 0;
  DBUG_RETURN(0);
}

static void group_replication_switch_to_multi_primary_mode_deinit(UDF_INIT *) {}

bool install_udf_functions() {
  bool error = false;
  SERVICE_TYPE(registry) *plugin_reg = mysql_plugin_registry_acquire();

  if (!plugin_reg) {
    LogPluginErr(
        ERROR_LEVEL,
        ER_GRP_RPL_UDF_REGISTER_SERVICE_ERROR); /* purecov: inspected */
    return true;                                /* purecov: inspected */
  }

  {
    my_service<SERVICE_TYPE(udf_registration)> service("udf_registration",
                                                       plugin_reg);
    if ((error != service.is_valid())) {
      udf_function_tuple set_as_primary(
          "group_replication_set_as_primary", Item_result::STRING_RESULT,
          (Udf_func_any)group_replication_set_as_primary,
          group_replication_set_as_primary_init,
          group_replication_set_as_primary_deinit);
      udf_function_tuple switch_to_single_primary_mode(
          "group_replication_switch_to_single_primary_mode",
          Item_result::STRING_RESULT,
          (Udf_func_any)group_replication_switch_to_single_primary_mode,
          group_replication_switch_to_single_primary_mode_init,
          group_replication_switch_to_single_primary_mode_deinit);
      udf_function_tuple switch_to_multi_primary_mode(
          "group_replication_switch_to_multi_primary_mode",
          Item_result::STRING_RESULT,
          (Udf_func_any)group_replication_switch_to_multi_primary_mode,
          group_replication_switch_to_multi_primary_mode_init,
          group_replication_switch_to_multi_primary_mode_deinit);

      std::list<udf_function_tuple> function_list;
      function_list.push_back(set_as_primary);
      function_list.push_back(switch_to_single_primary_mode);
      function_list.push_back(switch_to_multi_primary_mode);

      for (udf_function_tuple udf_tuple : function_list) {
        error = service->udf_register(
            std::get<0>(udf_tuple), std::get<1>(udf_tuple),
            std::get<2>(udf_tuple), std::get<3>(udf_tuple),
            std::get<4>(udf_tuple));
        if (error) {
          LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_UDF_REGISTER_ERROR,
                       std::get<0>(udf_tuple)); /* purecov: inspected */
          break;                                /* purecov: inspected */
        }
      }

      if (error) {
        /* purecov: begin inspected */
        int was_present;
        for (udf_function_tuple udf_tuple : function_list) {
          // don't care about errors since we are already erroring out
          service->udf_unregister(std::get<0>(udf_tuple), &was_present);

          if (!was_present) break;
        }
        /* purecov: end */
      }
    } else {
      LogPluginErr(
          ERROR_LEVEL,
          ER_GRP_RPL_UDF_REGISTER_SERVICE_ERROR); /* purecov: inspected */
    }
  }
  mysql_plugin_registry_release(plugin_reg);
  return error;
}

bool uninstall_udf_functions() {
  bool error = false;

  SERVICE_TYPE(registry) *plugin_reg = mysql_plugin_registry_acquire();

  if (!plugin_reg) {
    LogPluginErr(ERROR_LEVEL,
                 ER_GRP_RPL_UDF_UNREGISTER_ERROR); /* purecov: inspected */
    return true;                                   /* purecov: inspected */
  }

  {
    my_service<SERVICE_TYPE(udf_registration)> service("udf_registration",
                                                       plugin_reg);
    if ((error != service.is_valid())) {
      std::list<const char *> function_list;
      function_list.push_back("group_replication_set_as_primary");
      function_list.push_back(
          "group_replication_switch_to_single_primary_mode");
      function_list.push_back("group_replication_switch_to_multi_primary_mode");

      int was_present;
      for (const char *udf_name : function_list) {
        // don't care about the functions not being there
        error += service->udf_unregister(udf_name, &was_present);
      }
    }

    if (error) {
      LogPluginErr(ERROR_LEVEL,
                   ER_GRP_RPL_UDF_UNREGISTER_ERROR); /* purecov: inspected */
    }
  }
  mysql_plugin_registry_release(plugin_reg);
  return error;
}

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

#include "plugin/group_replication/include/udf/udf_utils.h"
#include "mysql/components/my_service.h"
#include "mysql/components/services/dynamic_privilege.h"
#include "mysql/components/services/mysql_runtime_error_service.h"
#include "plugin/group_replication/include/plugin.h"
#include "sql/auth/auth_acls.h"

std::atomic<int> UDF_counter::number_udfs_running(0);

privilege_result user_has_gr_admin_privilege() {
  THD *thd = current_thd;
  privilege_result result = privilege_result::error();
  bool super_user = false;

  if (thd == nullptr) {
    /* purecov: begin inspected */
    goto end;
    /* purecov: end */
  }

  super_user = (thd->security_context() != nullptr &&
                thd->security_context()->master_access() & SUPER_ACL);
  if (super_user) {
    result = privilege_result::success();
  } else {
    SERVICE_TYPE(registry) *plugin_registry = mysql_plugin_registry_acquire();

    if (plugin_registry == nullptr) {
      /* purecov: begin inspected */
      goto end;
      /* purecov: end */
    }

    bool has_global_grant = false;
    {
      my_service<SERVICE_TYPE(global_grants_check)> service(
          "global_grants_check", plugin_registry);
      if (service.is_valid()) {
        has_global_grant = service->has_global_grant(
            reinterpret_cast<Security_context_handle>(thd->security_context()),
            STRING_WITH_LEN("GROUP_REPLICATION_ADMIN"));
      } else {
        /* purecov: begin inspected */
        mysql_plugin_registry_release(plugin_registry);
        goto end;
        /* purecov: end */
      }
      /* service goes out of scope. It is destroyed and unregistered using
         plugin_registry. */
    }
    mysql_plugin_registry_release(plugin_registry);
    if (has_global_grant) {
      result = privilege_result::success();
    } else {
      result = privilege_result::no_privilege(
          thd->security_context()->priv_user().str,
          thd->security_context()->priv_host().str);
    }
  }
end:
  return result;
}

void log_privilege_status_result(privilege_result const &privilege,
                                 char *message) {
  switch (privilege.status) {
    case privilege_status::error:
      // Something is wrong and we were unable to access MySQL services.
      std::snprintf(
          message, MYSQL_ERRMSG_SIZE,
          "Error checking the user privileges. Check the log for more "
          "details or restart the server.");
      break;
    case privilege_status::no_privilege:
      std::snprintf(
          message, MYSQL_ERRMSG_SIZE,
          "User '%s'@'%s' needs SUPER or GROUP_REPLICATION_ADMIN privileges.",
          privilege.get_user(), privilege.get_host());
      break;
    case privilege_status::ok:
      break;
  }
}

bool member_online_with_majority() {
  if (!plugin_is_group_replication_running()) return false;

  bool const not_online = local_member_info == nullptr ||
                          local_member_info->get_recovery_status() !=
                              Group_member_info::MEMBER_ONLINE;
  bool const on_partition = group_partition_handler != nullptr &&
                            group_partition_handler->is_member_on_partition();
  if (not_online || on_partition) {
    return false;
  }
  return true;
}

bool group_contains_unreachable_member() {
  if (group_member_mgr) {
    if (group_member_mgr->is_unreachable_member_present()) {
      return true;
    }
  }
  return false;
}

bool group_contains_recovering_member() {
  if (group_member_mgr) {
    if (group_member_mgr->is_recovering_member_present()) {
      return true;
    }
  }
  return false;
}

bool validate_uuid_parameter(std::string &uuid, size_t length,
                             const char **error_message) {
  if (uuid.empty() || length == 0) {
    *error_message = server_uuid_not_present_str;
    return true;
  }

  if (!binary_log::Uuid::is_valid(uuid.c_str(), length)) {
    *error_message = server_uuid_not_valid_str;
    return true;
  }

  if (group_member_mgr) {
    std::unique_ptr<Group_member_info> member_info{
        group_member_mgr->get_group_member_info(uuid)};
    if (member_info.get() == nullptr) {
      *error_message = server_uuid_not_on_group_str;
      return true;
    }
  }

  return false;
}

bool throw_udf_error(const char *action_name, const char *error_message,
                     bool log_error) {
  SERVICE_TYPE(registry) *registry = NULL;
  if ((registry = get_plugin_registry())) {
    my_service<SERVICE_TYPE(mysql_runtime_error)> svc_error(
        "mysql_runtime_error", registry);
    if (svc_error.is_valid()) {
      mysql_error_service_emit_printf(svc_error, ER_GRP_RPL_UDF_ERROR, MYF(0),
                                      action_name, error_message);
      if (log_error)
        LogErr(ERROR_LEVEL, ER_GRP_RPL_SERVER_UDF_ERROR, action_name,
               error_message);
      return false;
    }
  }
  // Log the error in case we can't do much
  LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_SERVER_UDF_ERROR, action_name,
               error_message);
  return true;
}

bool log_group_action_result_message(Group_action_diagnostics *result_area,
                                     const char *action_name,
                                     char *result_message,
                                     unsigned long *length) {
  bool error = false;
  switch (result_area->get_execution_message_level()) {
    case Group_action_diagnostics::GROUP_ACTION_LOG_ERROR:
      throw_udf_error(action_name, result_area->get_execution_message().c_str(),
                      true);
      error = true;
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
      result.append(" completed successfully");
      my_stpcpy(result_message, result.c_str());
      *length = result.length();
      /* purecov: end */
  }
  return error;
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

bool group_contains_member_older_than(
    Member_version const &min_required_version) {
  bool constexpr OLDER_MEMBER_EXISTS = true;
  bool constexpr ALL_MEMBERS_OK = false;
  bool result = OLDER_MEMBER_EXISTS;

  std::vector<Group_member_info *> *members =
      group_member_mgr->get_all_members();
  auto it =
      std::find_if(members->begin(), members->end(),
                   [&min_required_version](Group_member_info *member) {
                     return member->get_member_version() < min_required_version;
                   });

  result = (it == members->end() ? ALL_MEMBERS_OK : OLDER_MEMBER_EXISTS);

  // Cleanup.
  for (auto *member : *members) delete member;
  delete members;

  return result;
}

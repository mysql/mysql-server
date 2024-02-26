/* Copyright (c) 2021, 2023, Oracle and/or its affiliates.

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

#include "plugin/group_replication/include/udf/udf_member_actions.h"
#include <cinttypes>
#include "mutex_lock.h"
#include "mysql/plugin.h"
#include "plugin/group_replication/include/plugin.h"
#include "plugin/group_replication/include/plugin_handlers/member_actions_handler.h"
#include "plugin/group_replication/include/udf/udf_utils.h"

static bool group_replication_enable_member_action_init(UDF_INIT *init_id,
                                                        UDF_ARGS *args,
                                                        char *message) {
  UDF_counter udf_counter;

  if (args->arg_count != 2) {
    my_stpcpy(message, "UDF takes 2 arguments.");
    return true;
  }

  if (args->arg_type[0] != STRING_RESULT || args->lengths[0] == 0) {
    my_stpcpy(message, "UDF first argument must be a string.");
    return true;
  }

  if (args->arg_type[1] != STRING_RESULT || args->lengths[1] == 0) {
    my_stpcpy(message, "UDF second argument must be a string.");
    return true;
  }

  privilege_result privilege = user_has_gr_admin_privilege();
  bool has_privileges = (privilege.status == privilege_status::ok);
  if (!has_privileges) {
    log_privilege_status_result(privilege, message);
    return true;
  }

  std::pair<bool, std::string> error_pair = check_super_read_only_is_disabled();
  if (error_pair.first) {
    my_stpcpy(message, error_pair.second.c_str());
    return true;
  }

  if (Charset_service::set_return_value_charset(init_id) ||
      Charset_service::set_args_charset(args)) {
    /* purecov: begin inspected */
    return true;
    /* purecov: end */
  }

  init_id->maybe_null = false;
  udf_counter.succeeded();
  return false;
}

static void group_replication_enable_member_action_deinit(UDF_INIT *) {
  UDF_counter::terminated();
}

static char *group_replication_enable_member_action(UDF_INIT *, UDF_ARGS *args,
                                                    char *result,
                                                    unsigned long *length,
                                                    unsigned char *is_null,
                                                    unsigned char *error) {
  const char *action_name = "group_replication_enable_member_action";
  *is_null = 0;  // result is not null
  *error = 0;
  const char *return_message = nullptr;
  bool throw_error = false;
  std::pair<bool, std::string> error_pair;
  bool im_the_primary = false;
  bool im_offline = false;

  std::string name = args->args[0] != nullptr ? args->args[0] : "";
  std::string stage = args->args[1] != nullptr ? args->args[1] : "";

  /*
    Try to acquire a read lock on the plugin_running_lock, which
    will succeed if no write lock is acquired by START or STOP
    GROUP_REPLICATION.
    We cannot wait until a read lock can be acquired because that
    can cause a deadlock between the UDF and STOP, more precisely:
    1) STOP acquires the write lock;
    2) UDF is called and waits for the read lock;
    3) STOP does call `terminate_plugin_modules()` which does wait
       that all ongoing UDFs do terminate.

    If it succeeds to acquire it, a RAII Checkable_rwlock::Guard
    object will keep the acquired lock until it leaves scope, either
    by success or by any error that might occur.
  */
  Checkable_rwlock::Guard g(*get_plugin_running_lock(),
                            Checkable_rwlock::TRY_READ_LOCK);
  if (!g.is_rdlocked()) {
    return_message =
        "It cannot be called while START or STOP "
        "GROUP_REPLICATION is ongoing.";
    throw_error = true;
    goto end;
  }

  im_the_primary =
      member_online_with_majority() && local_member_info->in_primary_mode() &&
      local_member_info->get_role() == Group_member_info::MEMBER_ROLE_PRIMARY;
  im_offline = !plugin_is_group_replication_running();
  if (!(im_the_primary || im_offline)) {
    return_message = "Member must be the primary or OFFLINE.";
    throw_error = true;
    goto end;
  }

  error_pair = member_actions_handler->enable_action(name, stage);
  if (error_pair.first) {
    return_message = error_pair.second.c_str();
    throw_error = true;
    goto end;
  }

  return_message = "OK";

end:
  *length = strlen(return_message);
  strcpy(result, return_message);
  if (throw_error) {
    *error = 1;
    throw_udf_error(action_name, return_message);
  }
  return result;
}

udf_descriptor enable_member_action_udf() {
  return {
      "group_replication_enable_member_action", Item_result::STRING_RESULT,
      reinterpret_cast<Udf_func_any>(group_replication_enable_member_action),
      group_replication_enable_member_action_init,
      group_replication_enable_member_action_deinit};
}

static bool group_replication_disable_member_action_init(UDF_INIT *init_id,
                                                         UDF_ARGS *args,
                                                         char *message) {
  UDF_counter udf_counter;

  if (args->arg_count != 2) {
    my_stpcpy(message, "UDF takes 2 arguments.");
    return true;
  }

  if (args->arg_type[0] != STRING_RESULT || args->lengths[0] == 0) {
    my_stpcpy(message, "UDF first argument must be a string.");
    return true;
  }

  if (args->arg_type[1] != STRING_RESULT || args->lengths[1] == 0) {
    my_stpcpy(message, "UDF second argument must be a string.");
    return true;
  }

  privilege_result privilege = user_has_gr_admin_privilege();
  bool has_privileges = (privilege.status == privilege_status::ok);
  if (!has_privileges) {
    log_privilege_status_result(privilege, message);
    return true;
  }

  std::pair<bool, std::string> error_pair = check_super_read_only_is_disabled();
  if (error_pair.first) {
    my_stpcpy(message, error_pair.second.c_str());
    return true;
  }

  if (Charset_service::set_return_value_charset(init_id) ||
      Charset_service::set_args_charset(args)) {
    /* purecov: begin inspected */
    return true;
    /* purecov: end */
  }

  init_id->maybe_null = false;
  udf_counter.succeeded();
  return false;
}

static void group_replication_disable_member_action_deinit(UDF_INIT *) {
  UDF_counter::terminated();
}

static char *group_replication_disable_member_action(UDF_INIT *, UDF_ARGS *args,
                                                     char *result,
                                                     unsigned long *length,
                                                     unsigned char *is_null,
                                                     unsigned char *error) {
  const char *action_name = "group_replication_disable_member_action";
  *is_null = 0;  // result is not null
  *error = 0;
  const char *return_message = nullptr;
  bool throw_error = false;
  std::pair<bool, std::string> error_pair;
  bool im_the_primary = false;
  bool im_offline = false;

  std::string name = args->args[0] != nullptr ? args->args[0] : "";
  std::string stage = args->args[1] != nullptr ? args->args[1] : "";

  /*
    Try to acquire a read lock on the plugin_running_lock, which
    will succeed if no write lock is acquired by START or STOP
    GROUP_REPLICATION.
    We cannot wait until a read lock can be acquired because that
    can cause a deadlock between the UDF and STOP, more precisely:
    1) STOP acquires the write lock;
    2) UDF is called and waits for the read lock;
    3) STOP does call `terminate_plugin_modules()` which does wait
       that all ongoing UDFs do terminate.

    If it succeeds to acquire it, a RAII Checkable_rwlock::Guard
    object will keep the acquired lock until it leaves scope, either
    by success or by any error that might occur.
  */
  Checkable_rwlock::Guard g(*get_plugin_running_lock(),
                            Checkable_rwlock::TRY_READ_LOCK);
  if (!g.is_rdlocked()) {
    return_message =
        "It cannot be called while START or STOP "
        "GROUP_REPLICATION is ongoing.";
    throw_error = true;
    goto end;
  }

  im_the_primary =
      member_online_with_majority() && local_member_info->in_primary_mode() &&
      local_member_info->get_role() == Group_member_info::MEMBER_ROLE_PRIMARY;
  im_offline = !plugin_is_group_replication_running();
  if (!(im_the_primary || im_offline)) {
    return_message = "Member must be the primary or OFFLINE.";
    throw_error = true;
    goto end;
  }

  error_pair = member_actions_handler->disable_action(name, stage);
  if (error_pair.first) {
    return_message = error_pair.second.c_str();
    throw_error = true;
    goto end;
  }

  return_message = "OK";

end:
  *length = strlen(return_message);
  strcpy(result, return_message);
  if (throw_error) {
    *error = 1;
    throw_udf_error(action_name, return_message);
  }
  return result;
}

udf_descriptor disable_member_action_udf() {
  return {
      "group_replication_disable_member_action", Item_result::STRING_RESULT,
      reinterpret_cast<Udf_func_any>(group_replication_disable_member_action),
      group_replication_disable_member_action_init,
      group_replication_disable_member_action_deinit};
}

static bool group_replication_reset_member_actions_init(UDF_INIT *init_id,
                                                        UDF_ARGS *args,
                                                        char *message) {
  UDF_counter udf_counter;

  if (args->arg_count != 0) {
    my_stpcpy(message, "UDF takes 0 arguments.");
    return true;
  }

  privilege_result privilege = user_has_gr_admin_privilege();
  bool has_privileges = (privilege.status == privilege_status::ok);
  if (!has_privileges) {
    log_privilege_status_result(privilege, message);
    return true;
  }

  std::pair<bool, std::string> error_pair = check_super_read_only_is_disabled();
  if (error_pair.first) {
    my_stpcpy(message, error_pair.second.c_str());
    return true;
  }

  if (Charset_service::set_return_value_charset(init_id) ||
      Charset_service::set_args_charset(args)) {
    /* purecov: begin inspected */
    return true;
    /* purecov: end */
  }

  init_id->maybe_null = false;
  udf_counter.succeeded();
  return false;
}

static void group_replication_reset_member_actions_deinit(UDF_INIT *) {
  UDF_counter::terminated();
}

static char *group_replication_reset_member_actions(UDF_INIT *, UDF_ARGS *args,
                                                    char *result,
                                                    unsigned long *length,
                                                    unsigned char *is_null,
                                                    unsigned char *error) {
  const char *action_name = "group_replication_reset_member_actions";
  *is_null = 0;  // result is not null
  *error = 0;
  const char *return_message = nullptr;
  bool throw_error = false;

  /*
    Try to acquire a read lock on the plugin_running_lock, which
    will succeed if no write lock is acquired by START or STOP
    GROUP_REPLICATION.
    We cannot wait until a read lock can be acquired because that
    can cause a deadlock between the UDF and STOP, more precisely:
    1) STOP acquires the write lock;
    2) UDF is called and waits for the read lock;
    3) STOP does call `terminate_plugin_modules()` which does wait
       that all ongoing UDFs do terminate.

    If it succeeds to acquire it, a RAII Checkable_rwlock::Guard
    object will keep the acquired lock until it leaves scope, either
    by success or by any error that might occur.
  */
  Checkable_rwlock::Guard g(*get_plugin_running_lock(),
                            Checkable_rwlock::TRY_READ_LOCK);
  if (!g.is_rdlocked()) {
    return_message =
        "It cannot be called while START or STOP "
        "GROUP_REPLICATION is ongoing.";
    throw_error = true;
    goto end;
  }

  if (plugin_is_group_replication_running()) {
    return_message =
        "Member must be OFFLINE to reset its member actions configuration.";
    throw_error = true;
    goto end;
  }

  if (member_actions_handler->reset_to_default_actions_configuration()) {
    return_message = "Unable to reset member actions configuration.";
    throw_error = true;
    goto end;
  }

  return_message = "OK";

end:
  *length = strlen(return_message);
  strcpy(result, return_message);
  if (throw_error) {
    *error = 1;
    throw_udf_error(action_name, return_message);
  }
  return result;
}

udf_descriptor reset_member_actions_udf() {
  return {
      "group_replication_reset_member_actions", Item_result::STRING_RESULT,
      reinterpret_cast<Udf_func_any>(group_replication_reset_member_actions),
      group_replication_reset_member_actions_init,
      group_replication_reset_member_actions_deinit};
}

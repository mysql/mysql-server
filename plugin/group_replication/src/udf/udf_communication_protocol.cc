/* Copyright (c) 2018, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "plugin/group_replication/include/udf/udf_communication_protocol.h"
#include <algorithm>
#include <cinttypes>
#include <string>
#include "mysql/plugin.h"
#include "plugin/group_replication/include/group_actions/communication_protocol_action.h"
#include "plugin/group_replication/include/member_version.h"
#include "plugin/group_replication/include/mysql_version_gcs_protocol_map.h"
#include "plugin/group_replication/include/plugin.h"
#include "plugin/group_replication/include/udf/udf_utils.h"

/*
 * Minimum version required of all group members to fire the
 * group_replication_set_communication_protocol action.
 */
static const Member_version min_version_required(0x080016);

static bool group_replication_get_communication_protocol_init(UDF_INIT *initid,
                                                              UDF_ARGS *args,
                                                              char *message) {
  bool constexpr FAILURE = true;
  bool constexpr SUCCESS = false;
  bool result = FAILURE;

  /*
    Increment only after verifying the plugin is not stopping
    Do NOT increment before accessing volatile plugin structures.
    Stop is checked again after increment as the plugin might have stopped
  */
  if (get_plugin_is_stopping()) {
    std::snprintf(message, MYSQL_ERRMSG_SIZE, member_offline_or_minority_str);
    return result;
  }
  UDF_counter udf_counter;

  if (args->arg_count != 0) {
    std::snprintf(message, MYSQL_ERRMSG_SIZE, "UDF does not take arguments.");
    goto end;
  }

  if (get_plugin_is_stopping()) {
    std::snprintf(message, MYSQL_ERRMSG_SIZE, member_offline_or_minority_str);
    goto end;
  }

  if (!member_online_with_majority()) {
    std::snprintf(message, MYSQL_ERRMSG_SIZE, member_offline_or_minority_str);
    goto end;
  }
  if (Charset_service::set_return_value_charset(initid)) goto end;

  result = SUCCESS;
  udf_counter.succeeded();
end:
  return result;
}

static void group_replication_get_communication_protocol_deinit(UDF_INIT *) {
  UDF_counter::terminated();
}

static char *group_replication_get_communication_protocol(
    UDF_INIT *, UDF_ARGS *, char *result, unsigned long *length,
    unsigned char *is_null, unsigned char *error) {
  /* According to sql/udf_example.cc, result has at least 255 bytes */
  unsigned long constexpr MAX_SAFE_LENGTH = 255;
  *is_null = 0;  // result is not null
  *error = 0;

  Gcs_protocol_version gcs_protocol = gcs_module->get_protocol_version();

  std::string mysql_version =
      convert_to_mysql_version(gcs_protocol).get_version_string();

  std::snprintf(result, MAX_SAFE_LENGTH, "%s", mysql_version.c_str());
  *length = std::strlen(result);

  return result;
}

udf_descriptor get_communication_protocol_udf() {
  return {"group_replication_get_communication_protocol",
          Item_result::STRING_RESULT,
          reinterpret_cast<Udf_func_any>(
              group_replication_get_communication_protocol),
          group_replication_get_communication_protocol_init,
          group_replication_get_communication_protocol_deinit};
}

const char *const wrong_nr_args_str =
    "UDF takes one version string argument with format major.minor.patch";
const char *const invalid_format_str =
    "'%s' is not version string argument with format major.minor.patch";
const char *const value_outside_domain_str = "%s is not between %s and %s";
const char *const wrong_value_for_paxos_single_leader =
    "group_replication_paxos_single_leader must be OFF when choosing a version "
    "lower than 8.0.27.";

static bool group_replication_set_communication_protocol_init(UDF_INIT *initid,
                                                              UDF_ARGS *args,
                                                              char *message) {
  bool constexpr FAILURE = true;
  bool constexpr SUCCESS = false;
  bool result = FAILURE;

  /*
    Increment only after verifying the plugin is not stopping
    Do NOT increment before accessing volatile plugin structures.
    Stop is checked again after increment as the plugin might have stopped
  */
  if (get_plugin_is_stopping()) {
    std::snprintf(message, MYSQL_ERRMSG_SIZE, member_offline_or_minority_str);
    return result;
  }
  UDF_counter udf_counter;

  privilege_result privilege = privilege_result::error();
  auto const &min_version = convert_to_mysql_version(Gcs_protocol_version::V1);

  // Validate nr. of arguments.
  bool const wrong_number_of_args =
      (args->arg_count != 1 || args->lengths[0] == 0);
  bool const wrong_arg_type =
      (!wrong_number_of_args && args->arg_type[0] != STRING_RESULT);
  if (wrong_number_of_args || wrong_arg_type) {
    std::snprintf(message, MYSQL_ERRMSG_SIZE, wrong_nr_args_str);
    goto end;
  }

  if (get_plugin_is_stopping()) {
    std::snprintf(message, MYSQL_ERRMSG_SIZE, member_offline_or_minority_str);
    goto end;
  }

  if (group_contains_unreachable_member()) {
    std::snprintf(message, MYSQL_ERRMSG_SIZE, unreachable_member_on_group_str);
    goto end;
  }

  if (group_contains_recovering_member()) {
    std::snprintf(message, MYSQL_ERRMSG_SIZE, recovering_member_on_group_str);
    goto end;
  }

  if (!member_online_with_majority()) {
    std::snprintf(message, MYSQL_ERRMSG_SIZE, member_offline_or_minority_str);
    goto end;
  }

  // Check privileges.
  privilege = user_has_gr_admin_privilege();
  log_privilege_status_result(privilege, message);
  switch (privilege.status) {
    case privilege_status::error:
      // Something is wrong and we were unable to access MySQL services.
    case privilege_status::no_privilege:
      goto end;
    case privilege_status::ok:
      break;
  }

  if (args->args[0] != nullptr) {
    // Validate argument.
    if (!valid_mysql_version_string(args->args[0])) {
      std::snprintf(message, MYSQL_ERRMSG_SIZE, invalid_format_str,
                    args->args[0]);
      goto end;
    }

    // Validate argument domain: [first-GR-version, my-version]
    auto const requested_version = convert_to_member_version(args->args[0]);
    auto const &my_version = local_member_info->get_member_version();
    bool const valid_version =
        (min_version <= requested_version && requested_version <= my_version);
    if (!valid_version) {
      std::snprintf(message, MYSQL_ERRMSG_SIZE, value_outside_domain_str,
                    requested_version.get_version_string().c_str(),
                    min_version.get_version_string().c_str(),
                    my_version.get_version_string().c_str());
      goto end;
    }
  }
  if (Charset_service::set_return_value_charset(initid) ||
      Charset_service::set_args_charset(args))
    goto end;

  result = SUCCESS;
  udf_counter.succeeded();
end:
  return result;
}

static void group_replication_set_communication_protocol_deinit(UDF_INIT *) {
  UDF_counter::terminated();
}

static char *group_replication_set_communication_protocol(
    UDF_INIT *, UDF_ARGS *args, char *result, unsigned long *length,
    unsigned char *is_null, unsigned char *error) {
  const char *action_name = "group_replication_set_communication_protocol";
  /* According to sql/udf_example.cc, result has at least 255 bytes */
  unsigned long constexpr MAX_SAFE_LENGTH = 255;
  bool valid_version = false;
  Member_version requested_version(0);
  Member_version const &min_version =
      convert_to_mysql_version(Gcs_protocol_version::V1);
  Member_version my_version(0);
  *is_null = 0;  // result is not null
  *error = 0;

  if (args->args[0] == nullptr) {
    std::snprintf(result, MAX_SAFE_LENGTH, wrong_nr_args_str);
    *length = std::strlen(result);
    *error = 1;
    throw_udf_error(action_name, result);
    return result;
  }

  // Check whether the group supports this action.
  if (group_contains_member_older_than(min_version_required)) {
    std::snprintf(result, MAX_SAFE_LENGTH,
                  "This action requires all members of the group to have at "
                  "least version %s",
                  min_version_required.get_version_string().c_str());
    *length = std::strlen(result);
    *error = 1;
    throw_udf_error(action_name, result);
    return result;
  }

  // Validate argument.
  if (!valid_mysql_version_string(args->args[0])) {
    std::snprintf(result, MAX_SAFE_LENGTH, invalid_format_str, args->args[0]);
    *length = std::strlen(result);
    *error = 1;
    throw_udf_error(action_name, result);
    return result;
  }

  // Validate argument domain: [first-GR-version, my-version]
  requested_version = convert_to_member_version(args->args[0]);
  my_version = local_member_info->get_member_version();
  valid_version =
      (min_version <= requested_version && requested_version <= my_version);
  if (!valid_version) {
    std::snprintf(result, MAX_SAFE_LENGTH, value_outside_domain_str,
                  requested_version.get_version_string().c_str(),
                  min_version.get_version_string().c_str(),
                  my_version.get_version_string().c_str());
    *length = std::strlen(result);
    *error = 1;
    throw_udf_error(action_name, result);
    return result;
  }

  // If we request a transition from a version above 8.0.27 to a version
  //  under 8.0.27, we must ensure that the group was started with
  //  PAXOS Single Leader support with value OFF.
  Member_version const version_that_supports_paxos_single_leader(
      FIRST_PROTOCOL_WITH_SUPPORT_FOR_CONSENSUS_LEADERS);
  if (my_version >= version_that_supports_paxos_single_leader &&
      requested_version < version_that_supports_paxos_single_leader &&
      local_member_info->get_allow_single_leader()) {
    std::snprintf(result, MAX_SAFE_LENGTH, wrong_value_for_paxos_single_leader);
    *length = std::strlen(result);
    *error = 1;
    throw_udf_error(action_name, result);
    return result;
  }

  Gcs_protocol_version gcs_protocol =
      convert_to_gcs_protocol(requested_version, my_version);

  Communication_protocol_action group_action(gcs_protocol);
  Group_action_diagnostics action_diagnostics;
  group_action_coordinator->coordinate_action_execution(
      &group_action, &action_diagnostics,
      Group_action_message::ACTION_UDF_COMMUNICATION_PROTOCOL_MESSAGE);
  if (log_group_action_result_message(
          &action_diagnostics, "group_replication_set_communication_protocol",
          result, length)) {
    *error = 1;
  }
  return result;
}

udf_descriptor set_communication_protocol_udf() {
  return {"group_replication_set_communication_protocol",
          Item_result::STRING_RESULT,
          reinterpret_cast<Udf_func_any>(
              group_replication_set_communication_protocol),
          group_replication_set_communication_protocol_init,
          group_replication_set_communication_protocol_deinit};
}

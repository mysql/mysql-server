/* Copyright (c) 2024, 2026, Oracle and/or its affiliates.

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

#include <mysql/components/service_implementation.h>
#include <mysql/components/services/group_replication_management_service.h>

#include "plugin/group_replication/include/leave_group_on_failure.h"
#include "plugin/group_replication/include/plugin.h"
#include "plugin/group_replication/include/services/management/management.h"

std::chrono::steady_clock::time_point GR_start_time_maintain::gr_start_time =
    std::chrono::steady_clock::time_point::min();

void GR_start_time_maintain::reset_start_time() {
  gr_start_time = std::chrono::steady_clock::now();
}

bool GR_start_time_maintain::check_if_quarantine_time_passed(
    int quarantime_time, unsigned int *seconds_since_member_join) {
  auto time_now = std::chrono::steady_clock::now();
  auto time_diff =
      std::chrono::duration_cast<std::chrono::seconds>(time_now - gr_start_time)
          .count();
  *seconds_since_member_join = time_diff;
  return gr_start_time != std::chrono::steady_clock::time_point::min() &&
         time_diff > quarantime_time;
}

////////////////////////////////////////////////////////////////////////////////
namespace gr {
namespace gr_management {
DEFINE_METHOD(eject_status, eject,
              (int quarantine_time_in_seconds,
               unsigned int *seconds_since_member_join)) {
  DBUG_TRACE;
  if (local_member_info == nullptr || group_member_mgr == nullptr) {
    return GR_RM_NOT_A_MEMBER;
  }
  if (!local_member_info->in_primary_mode()) {
    return GR_RM_NOT_IN_SINGLE_PRIMARY_MODE;
  }
  if (local_member_info->get_role() !=
      Group_member_info::MEMBER_ROLE_SECONDARY) {
    return GR_RM_NOT_A_SECONDARY_MEMBER;
  }
  if (group_member_mgr->get_number_of_members() < 3) {
    return GR_RM_NUMBER_OF_MEMBERS_LESS_THAN_THREE;
  }
  if (!GR_start_time_maintain::check_if_quarantine_time_passed(
          quarantine_time_in_seconds, seconds_since_member_join)) {
    return GR_RM_QUARANTINE_PERIOD_NOT_OVER;
  }

  std::string error_message("Service call to leave the group.");
  leave_group_on_failure::mask leave_actions;
  leave_actions.set(leave_group_on_failure::STOP_APPLIER, true);
  leave_actions.set(leave_group_on_failure::HANDLE_EXIT_STATE_ACTION, true);
  leave_actions.set(leave_group_on_failure::HANDLE_AUTO_REJOIN, true);
  leave_group_on_failure::leave(leave_actions, 0, nullptr,
                                error_message.c_str());
  return GR_RM_SUCCESS_LEFT_GROUP;
}

DEFINE_BOOL_METHOD(is_member_online_or_recovering, ()) {
  DBUG_TRACE;

  if (!plugin_is_group_replication_running()) return false;

  if (nullptr == local_member_info) return false;

  const Group_member_info::Group_member_status member_status =
      local_member_info->get_recovery_status();
  if (member_status == Group_member_info::MEMBER_ONLINE ||
      member_status == Group_member_info::MEMBER_IN_RECOVERY) {
    return true;
  }

  return false;
}
}  // namespace gr_management
}  // namespace gr
BEGIN_SERVICE_IMPLEMENTATION(group_replication,
                             group_replication_management_service_v1)
gr::gr_management::eject, gr::gr_management::is_member_online_or_recovering,
    END_SERVICE_IMPLEMENTATION();

bool register_group_replication_management_services() {
  DBUG_TRACE;

  DBUG_EXECUTE_IF("group_replication_management_service", return false;);

  my_service<SERVICE_TYPE(registry_registration)> reg("registry_registration",
                                                      get_plugin_registry());
  using group_replication_management_service_t =
      SERVICE_TYPE_NO_CONST(group_replication_management_service_v1);
  return reg->register_service(
      GROUP_REPLICATION_MANAGEMENT_SERVICE_NAME,
      reinterpret_cast<my_h_service>(
          const_cast<group_replication_management_service_t *>(
              &SERVICE_IMPLEMENTATION(
                  group_replication,
                  group_replication_management_service_v1))));
}

bool unregister_group_replication_management_services() {
  DBUG_TRACE;
  my_service<SERVICE_TYPE(registry_registration)> reg("registry_registration",
                                                      get_plugin_registry());
  return reg->unregister(GROUP_REPLICATION_MANAGEMENT_SERVICE_NAME);
}

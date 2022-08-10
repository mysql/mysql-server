/* Copyright (c) 2021, 2022, Oracle and/or its affiliates.

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

#include "plugin/group_replication/include/services/status_service/status_service.h"
#include <mysql/components/my_service.h>
#include <mysql/components/services/group_replication_status_service.h>
#include <mysql/components/services/registry.h>
#include "plugin/group_replication/include/plugin.h"
#include "plugin/group_replication/include/udf/udf_utils.h"

namespace gr {
namespace status_service {

bool is_group_in_single_primary_mode_internal() {
  DBUG_TRACE;

  if (!plugin_is_group_replication_running()) return false;

  if (nullptr == local_member_info) return false;

  const Group_member_info::Group_member_status member_status =
      local_member_info->get_recovery_status();
  if (member_status != Group_member_info::MEMBER_ONLINE &&
      member_status != Group_member_info::MEMBER_IN_RECOVERY) {
    return false;
  }

  return local_member_info->in_primary_mode();
}

/*
 Service implementation.
*/
DEFINE_BOOL_METHOD(gr_status_service_is_group_in_single_primary_mode, ()) {
  DBUG_TRACE;
  return is_group_in_single_primary_mode_internal();
}

DEFINE_BOOL_METHOD(
    gr_status_service_is_group_in_single_primary_mode_and_im_the_primary, ()) {
  DBUG_TRACE;
  return (is_group_in_single_primary_mode_internal() &&
          local_member_info->get_role() ==
              Group_member_info::MEMBER_ROLE_PRIMARY);
}

DEFINE_BOOL_METHOD(
    gr_status_service_is_group_in_single_primary_mode_and_im_a_secondary, ()) {
  DBUG_TRACE;
  return (is_group_in_single_primary_mode_internal() &&
          local_member_info->get_role() ==
              Group_member_info::MEMBER_ROLE_SECONDARY);
}

DEFINE_BOOL_METHOD(gr_status_service_is_member_online_with_group_majority, ()) {
  DBUG_TRACE;
  return member_online_with_majority();
}

BEGIN_SERVICE_IMPLEMENTATION(group_replication,
                             group_replication_status_service_v1)
gr_status_service_is_group_in_single_primary_mode,
    gr_status_service_is_group_in_single_primary_mode_and_im_the_primary,
    gr_status_service_is_group_in_single_primary_mode_and_im_a_secondary,
    gr_status_service_is_member_online_with_group_majority,
    END_SERVICE_IMPLEMENTATION();

/*
 Service registration.
*/
bool register_gr_status_service() {
  DBUG_TRACE;

  my_service<SERVICE_TYPE(registry_registration)> reg("registry_registration",
                                                      get_plugin_registry());
  using group_replication_status_service_v1_t =
      SERVICE_TYPE_NO_CONST(group_replication_status_service_v1);
  return reg->register_service(
      "group_replication_status_service_v1.group_replication",
      reinterpret_cast<my_h_service>(
          const_cast<group_replication_status_service_v1_t *>(
              &SERVICE_IMPLEMENTATION(group_replication,
                                      group_replication_status_service_v1))));
}

bool unregister_gr_status_service() {
  DBUG_TRACE;
  my_service<SERVICE_TYPE(registry_registration)> reg("registry_registration",
                                                      get_plugin_registry());
  return reg->unregister(
      "group_replication_status_service_v1.group_replication");
}

}  // namespace status_service
}  // namespace gr

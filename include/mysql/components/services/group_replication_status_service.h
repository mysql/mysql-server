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

#ifndef GROUP_REPLICATION_STATUS_SERVICE_H
#define GROUP_REPLICATION_STATUS_SERVICE_H

#include <mysql/components/service.h>
#include <stddef.h>

/**
  @ingroup group_components_services_inventory

  A service to get the status of a member of Group Replication.

  This is only available if the component is on a server with Group
  Replication plugin installed.

  @code
  SERVICE_TYPE(registry) *plugin_registry = mysql_plugin_registry_acquire();
  my_service<SERVICE_TYPE(group_replication_status_service_v1)> svc(
      "group_replication_status_service_v1", plugin_registry);

  if (svc.is_valid()) {
    bool error = svc->...
  }
  @endcode
*/
BEGIN_SERVICE_DEFINITION(group_replication_status_service_v1)

/**
  Checks if this member is part of a group in single-primary mode.

  @return status
    @retval true  this member is part of a group in single-primary mode
    @retval false otherwise (including case where member is even not running
                  Group Replication)
*/
DECLARE_BOOL_METHOD(is_group_in_single_primary_mode, ());

/**
  Checks if this member is part of a group in single-primary mode and if
  this member is the primary.

  @return status
    @retval true  this member is part of a group in single-primary mode
                  and is the primary
    @retval false otherwise
*/
DECLARE_BOOL_METHOD(is_group_in_single_primary_mode_and_im_the_primary, ());

/**
  Checks if this member is part of a group in single-primary mode and if
  this member is a secondary.

  @return status
    @retval true  this member is part of a group in single-primary mode
                  and is a secondary
    @retval false otherwise
*/
DECLARE_BOOL_METHOD(is_group_in_single_primary_mode_and_im_a_secondary, ());

/**
  Checks if this member is ONLINE and part of the group majority.

  @return status
    @retval true  this member is ONLINE and part of the group majority
    @retval false otherwise
*/
DECLARE_BOOL_METHOD(is_member_online_with_group_majority, ());

END_SERVICE_DEFINITION(group_replication_status_service_v1)

#endif /* GROUP_REPLICATION_STATUS_SERVICE_H */

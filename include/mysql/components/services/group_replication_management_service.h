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

#ifndef GROUP_REPLICATION_MANAGEMENT_SERVICE_H
#define GROUP_REPLICATION_MANAGEMENT_SERVICE_H

#include <mysql/components/service.h>
#include <stddef.h>

enum eject_status {
  GR_RM_SUCCESS_LEFT_GROUP,
  GR_RM_NOT_IN_SINGLE_PRIMARY_MODE,
  GR_RM_NOT_A_SECONDARY_MEMBER,
  GR_RM_NUMBER_OF_MEMBERS_LESS_THAN_THREE,
  GR_RM_QUARANTINE_PERIOD_NOT_OVER,
  GR_RM_NOT_A_MEMBER
};

BEGIN_SERVICE_DEFINITION(group_replication_management_service_v1)

DECLARE_METHOD(enum eject_status, eject,
               (int quarantine_time_in_seconds,
                unsigned int *seconds_since_member_join));

/**
  Checks if this member is ONLINE or RECOVERING.

  @return status
    @retval true  this member is ONLINE or RECOVERING
    @retval false otherwise
*/
DECLARE_BOOL_METHOD(is_member_online_or_recovering, ());

END_SERVICE_DEFINITION(group_replication_management_service_v1)

#endif /* GROUP_REPLICATION_MANAGEMENT_SERVICE_H */

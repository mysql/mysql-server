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

#ifndef GR_MANAGEMENT_SERVICES_H
#define GR_MANAGEMENT_SERVICES_H

#include <mysql/components/services/registry.h>
#include <mysql/service_plugin_registry.h>
#include <chrono>
#include <ctime>
#include <string>

// Service name of a service used to leave the group with rejoin option.
#define GROUP_REPLICATION_MANAGEMENT_SERVICE_NAME \
  "group_replication.group_replication_management"

/**
  @class GR_start_time_maintain

  This class is used to maintain the timestamp of GR start.
  During rejoin GR reset this timestamp to the current time.
*/
class GR_start_time_maintain {
 private:
  // Timestamp of GR Start.
  static std::chrono::steady_clock::time_point gr_start_time;

 public:
  /**
   Resets the gr_start_time to now.
   During rejoin GR start time is reset to now.
 */
  static void reset_start_time();

  /**
   Calculates time difference between GR start and now and does the time
   comparison with quarantine time.

   @param[in] quarantime_time Quarantine time
   @param[out] seconds_since_member_join Time difference between GR start and
   now

   @retval True Difference between GR start and now is bigger then quarantine
   @retval False Difference between GR start and now is smaller then quarantine
 */
  static bool check_if_quarantine_time_passed(
      int quarantime_time, unsigned int *seconds_since_member_join);
};

/*
 Registers the group_replication_management service.
 Service group_replication_management can be used to request the member to leave
 the group with the rejoin option set.
*/
bool register_group_replication_management_services();

/*
 Unregisters the group_replication_management service.
*/
bool unregister_group_replication_management_services();
#endif  // GR_MANAGEMENT_SERVICES_H

/* Copyright (c) 2024, Oracle and/or its affiliates.

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

#ifndef GROUP_REPLICATION_FLOW_CONTROL_METRICS_SERVICE_H
#define GROUP_REPLICATION_FLOW_CONTROL_METRICS_SERVICE_H

#include <mysql/components/service.h>
#include <stddef.h>

/**
  @ingroup group_components_services_inventory

  A service that retrieve extra flow control stats from a member.

  This only works if the component is on a server with group replication
  running.

  @code
  SERVICE_TYPE(registry) *plugin_registry = mysql_plugin_registry_acquire();
  my_service<SERVICE_TYPE(group_replication_flow_control_metrics_service)> svc(
      "group_replication_message_service_send", plugin_registry);

  uint64_t value;
  if (svc.is_valid()) {
    bool error = svc->gr_flow_control_throttle_count(&value);
  }
  @endcode
*/
BEGIN_SERVICE_DEFINITION(group_replication_flow_control_metrics_service)

/**
  This function SHALL be called whenever the caller wants to
  read number of throttled transactions on this server since Group Replication
  start.

  @param[out] counts            number of transactions throttled

  @return false success, true on failure. Failure can happen if not able to
  read values from Group Replication metrics.
*/
DECLARE_BOOL_METHOD(get_throttle_count, (uint64_t * counts));

/**
  This function SHALL be called whenever the caller wants to
  read sum of time of throttled transactions on this server since Group
  Replication start.

  @param[out] time_sum          time in microseconds of transactions throttled

  @return false success, true on failure. Failure can happen if not able to
  read values from Group Replication metrics.
*/
DECLARE_BOOL_METHOD(get_throttle_time_sum, (uint64_t * time_sum));

/**
  This function SHALL be called whenever the caller wants to
  read number of active throttled transactions on this server.

  @param[out] active_count      number of active transactions being throttled

  @return false success, true on failure. Failure can happen if not able to
  read values from Group Replication metrics.
*/
DECLARE_BOOL_METHOD(get_throttle_active_count, (uint64_t * active_count));

/**
  This function SHALL be called whenever the caller wants to
  read timestamp last transaction was throttled on this server since Group
  Replication start.

  @param[out] timestamp              string representation of date. Buffer
                                     shall be bigger than
                                     MAX_DATE_STRING_REP_LENGTH.

  @return false success, true on failure. Failure can happen if not able to
  read values from Group Replication metrics.
*/
DECLARE_BOOL_METHOD(get_throttle_last_throttle_timestamp, (char *timestamp));

END_SERVICE_DEFINITION(group_replication_flow_control_metrics_service)

#endif /* GROUP_REPLICATION_FLOW_CONTROL_METRICS_SERVICE_H */

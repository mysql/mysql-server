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

#include "plugin/group_replication/include/services/flow_control/get_metrics.h"
#include <mysql/components/my_service.h>
#include <mysql/components/services/group_replication_flow_control_metrics_service.h>
#include <mysql/components/services/registry.h>
#include "plugin/group_replication/include/plugin.h"
#include "plugin/group_replication/include/plugin_handlers/metrics_handler.h"

namespace gr {
namespace flow_control_metrics_service {

DEFINE_BOOL_METHOD(get_throttle_count, (uint64_t * value)) {
  DBUG_TRACE;

  *value = metrics_handler->get_flow_control_throttle_count();

  return false;
}

DEFINE_BOOL_METHOD(get_throttle_time_sum, (uint64_t * value)) {
  DBUG_TRACE;

  *value = metrics_handler->get_flow_control_throttle_time();

  return false;
}

DEFINE_BOOL_METHOD(get_throttle_active_count, (uint64_t * value)) {
  DBUG_TRACE;

  *value = metrics_handler->get_flow_control_throttle_active();

  return false;
}

DEFINE_BOOL_METHOD(get_throttle_last_throttle_timestamp, (char *buffer)) {
  DBUG_TRACE;

  uint64_t microseconds_since_epoch =
      metrics_handler->get_flow_control_throttle_last_throttle_timestamp();

  if (microseconds_since_epoch > 0) {
    microseconds_to_datetime_str(microseconds_since_epoch, buffer, 6);
  }

  return false;
}

BEGIN_SERVICE_IMPLEMENTATION(group_replication,
                             group_replication_flow_control_metrics_service)
get_throttle_count, get_throttle_time_sum, get_throttle_active_count,
    get_throttle_last_throttle_timestamp, END_SERVICE_IMPLEMENTATION();

/*
 Service registration.
*/
bool register_gr_flow_control_metrics_service() {
  DBUG_TRACE;

  my_service<SERVICE_TYPE(registry_registration)> reg("registry_registration",
                                                      get_plugin_registry());
  using group_replication_flow_control_metrics_service_t =
      SERVICE_TYPE_NO_CONST(group_replication_flow_control_metrics_service);
  return reg->register_service(
      "group_replication_flow_control_metrics_service.group_replication",
      reinterpret_cast<my_h_service>(
          const_cast<group_replication_flow_control_metrics_service_t *>(
              &SERVICE_IMPLEMENTATION(
                  group_replication,
                  group_replication_flow_control_metrics_service))));
}

bool unregister_gr_flow_control_metrics_service() {
  DBUG_TRACE;
  my_service<SERVICE_TYPE(registry_registration)> reg("registry_registration",
                                                      get_plugin_registry());
  return reg->unregister(
      "group_replication_flow_control_metrics_service.group_replication");
}

}  // namespace flow_control_metrics_service
}  // namespace gr

/* Copyright (c) 2025, 2026, Oracle and/or its affiliates.

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

#include "plugin/group_replication/include/plugin_handlers/primary_election_most_uptodate.h"

#include <mysql/components/services/group_replication_elect_prefers_most_updated_service.h>
#include "my_dbug.h"
#include "plugin/group_replication/include/plugin.h"
#include "plugin/group_replication/include/plugin_utils.h"
#include "plugin/group_replication/include/services/system_variable/get_system_variable.h"

bool Primary_election_most_update::is_enabled() {
  DBUG_TRACE;
  int error = 0;
  bool enabled_value = false;
  Get_system_variable get_system_variable;

  error = get_system_variable.get_most_uptodate(enabled_value);

  if (error) {
    return false;
  }
  return enabled_value;
}

void Primary_election_most_update::update_status(
    unsigned long long int micro_seconds, uint64_t delta) {
  SERVICE_TYPE(registry) *plugin_registry = mysql_plugin_registry_acquire();
  const my_service<SERVICE_TYPE(group_replication_primary_election)> svc(
      "group_replication_primary_election", plugin_registry);

  if (svc.is_valid()) {
    char timestamp[MAX_DATE_STRING_REP_LENGTH];
    if (micro_seconds != 0) {
      microseconds_to_datetime_str(micro_seconds, timestamp, 6);
    } else {
      timestamp[0] = '\0';
    }
    svc->update_primary_election_status(timestamp, delta);
  }

  mysql_plugin_registry_release(plugin_registry);
}

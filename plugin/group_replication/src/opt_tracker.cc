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

#include <string>

#include "plugin/group_replication/include/opt_tracker.h"

#include <mysql/components/services/mysql_option_tracker.h>
#include <mysql/components/util/weak_service_reference.h>
#include "mysql/components/library_mysys/option_tracker_usage.h"
#include "plugin/group_replication/include/plugin.h"

static const std::string s_name("mysql_option_tracker_option");
static const std::string f_name_group_replication("Group Replication");
static const std::string c_name_group_replication("group_replication plugin");
typedef weak_service_reference<SERVICE_TYPE(mysql_option_tracker_option),
                               c_name_group_replication, s_name>
    srv_weak_option_option;
unsigned long long opt_option_tracker_usage_group_replication_plugin = 0;
static bool cb(unsigned long long new_value) {
  opt_option_tracker_usage_group_replication_plugin = new_value;
  return false;
}
static bool cb_define_failed = false;

void track_group_replication_available() {
  srv_weak_option_option::init(
      server_services_references_module->registry_service,
      server_services_references_module->registry_registration_service,
      [&](SERVICE_TYPE(mysql_option_tracker_option) * opt) {
        return 0 != opt->define(f_name_group_replication.c_str(),
                                c_name_group_replication.c_str(), 0) ||
               option_usage_read_counter(
                   f_name_group_replication.c_str(),
                   &opt_option_tracker_usage_group_replication_plugin,
                   server_services_references_module->registry_service) ||
               (cb_define_failed = option_usage_register_callback(
                    f_name_group_replication.c_str(), cb,
                    server_services_references_module->registry_service));
      },
      true);
}

void track_group_replication_unavailable() {
  srv_weak_option_option::deinit(
      server_services_references_module->registry_service,
      server_services_references_module->registry_registration_service,
      [&](SERVICE_TYPE(mysql_option_tracker_option) * opt) {
        if (!cb_define_failed &&
            option_usage_unregister_callback(
                f_name_group_replication.c_str(), cb,
                server_services_references_module->registry_service)) {
          return true;
        }
        return 0 != opt->undefine(f_name_group_replication.c_str());
      });
}

void track_group_replication_enabled(bool enabled) {
  auto svc = srv_weak_option_option::get_service();
  if (nullptr != svc) {
    svc->set_enabled(f_name_group_replication.c_str(), enabled ? 1 : 0);

    if (enabled) {
      ++opt_option_tracker_usage_group_replication_plugin;
    }
  }
}

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

#include "health_monitor_option_usage.h"

#include "mysql/components/library_mysys/option_tracker_usage.h"
#include "mysql/components/services/mysql_option_tracker.h"
#include "mysql/components/util/weak_service_reference.h"

#include "component_register.h"
#include "component_service.h"

namespace hm {

const std::string k_component_name("component_health_monitor");
const std::string k_option_name("Health Monitor");
const std::string k_tracker_name("mysql_option_tracker_option");

// Table access usage count. Incremented for each table access.
unsigned long long Option_Usage::usage_count{0};

// Callback to set the usage count.
static bool cb_set_usage_count(unsigned long long new_value) {
  Option_Usage::usage_count = new_value;
  return false;
}
// True if callback registration failed.
static bool cb_error{false};
using weak_option =
    weak_service_reference<SERVICE_TYPE(mysql_option_tracker_option),
                           k_component_name, k_tracker_name>;

// Register with the option tracker component.
bool Option_Usage::usage_register() {
  bool const ret = weak_option::init(
      registry_srv, registry_reg_srv,
      [&](SERVICE_TYPE(mysql_option_tracker_option) * opt) {
        return 0 != opt->define(k_option_name.c_str(), k_component_name.c_str(),
                                1) ||
               option_usage_read_counter(k_option_name.c_str(),
                                         &Option_Usage::usage_count,
                                         registry_srv) ||
               (cb_error = option_usage_register_callback(
                    k_option_name.c_str(), cb_set_usage_count, registry_srv));
      });
  return ret;
}

// Deregister from the option tracker component.
bool Option_Usage::usage_deregister() {
  return weak_option::deinit(
      registry_srv, registry_reg_srv,
      [&](SERVICE_TYPE(mysql_option_tracker_option) * opt) {
        if (!cb_error &&
            option_usage_unregister_callback(
                k_option_name.c_str(), cb_set_usage_count, registry_srv)) {
          return true;
        }
        return 0 != opt->undefine(k_option_name.c_str());
      });
}

}  // namespace hm
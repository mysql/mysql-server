/*
  Copyright (c) 2024, 2026, Oracle and/or its affiliates.

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
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include <memory>
#include <string>

#include "mysql/components/component_implementation.h"
#include "mysql/components/library_mysys/option_tracker_usage.h"
#include "mysql/components/service.h"
#include "mysql/components/services/mysql_option_tracker.h"
#include "mysql/components/services/registry.h"
#include "mysql/components/util/weak_service_reference.h"

#include "option_usage.h"
#include "tm_global.h"
#include "tm_required_services.h"

namespace telemetry {

const std::string c_name("component_telemetry"),
    opt_name("mysql_option_tracker_option"), c_option_name("MySQL Telemetry");

typedef weak_service_reference<SERVICE_TYPE(mysql_option_tracker_option),
                               c_name, opt_name>
    weak_option;

/** Protected by g_option_usage_mutex */
unsigned long long opt_option_tracker_usage_otel_component = 0;
static bool cb(unsigned long long new_value) {
  opt_option_tracker_usage_otel_component = new_value;
  return false;
}
static bool cb_define_failed = false;

/** Protected by g_option_usage_mutex */
static bool option_usage_is_initialized = false;

bool otel_component_option_usage_init() {
  bool result;

  option_usage_is_initialized = false;

  mutex_srv->lock(&g_option_usage_mutex, __FILE__, __LINE__);

  result = weak_option::init(
      reg_srv, reg_reg, [&](SERVICE_TYPE(mysql_option_tracker_option) * opt) {
        return 0 != opt->define(c_option_name.c_str(), c_name.c_str(), 1) ||
               option_usage_read_counter(
                   c_option_name.c_str(),
                   &opt_option_tracker_usage_otel_component, reg_srv) ||
               (cb_define_failed = option_usage_register_callback(
                    c_option_name.c_str(), cb, reg_srv));
      });

  option_usage_is_initialized = true;
  mutex_srv->unlock(&g_option_usage_mutex, __FILE__, __LINE__);

  return result;
}

bool otel_component_option_usage_deinit() {
  bool result;

  mutex_srv->lock(&g_option_usage_mutex, __FILE__, __LINE__);

  if (option_usage_is_initialized) {
    result = weak_option::deinit(
        reg_srv, reg_reg, [&](SERVICE_TYPE(mysql_option_tracker_option) * opt) {
          if (!cb_define_failed && option_usage_unregister_callback(
                                       c_option_name.c_str(), cb, reg_srv)) {
            return true;
          }
          return 0 != opt->undefine(c_option_name.c_str());
        });
  } else {
    result = false;
  }

  option_usage_is_initialized = false;
  mutex_srv->unlock(&g_option_usage_mutex, __FILE__, __LINE__);

  return result;
}

bool otel_component_option_usage_set() {
  bool const result = false;

  mutex_srv->lock(&g_option_usage_mutex, __FILE__, __LINE__);

  ++opt_option_tracker_usage_otel_component;

  mutex_srv->unlock(&g_option_usage_mutex, __FILE__, __LINE__);

  return result;
}

}  // namespace telemetry

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

#include "option_usage.h"
#include <string>
#include "connection_control.h"
#include "mysql/components/component_implementation.h"
#include "mysql/components/library_mysys/option_usage_data.h"
#include "mysql/components/service.h"
#include "mysql/components/services/mysql_option_tracker.h"
#include "mysql/components/util/weak_service_reference.h"

static const std::string c_name("connection_control plugin"),
    opt_name("mysql_option_tracker_option"),
    c_option_name("Connection DoS control");
typedef weak_service_reference<SERVICE_TYPE(mysql_option_tracker_option),
                               c_name, opt_name>
    weak_option;
static Option_usage_data *option_usage{nullptr};

bool connection_control_plugin_option_usage_init() {
  assert(option_usage == nullptr);
  option_usage =
      new (std::nothrow) Option_usage_data(c_option_name.c_str(), reg_srv);
  return weak_option::init(
      reg_srv, reg_reg, [&](SERVICE_TYPE(mysql_option_tracker_option) * opt) {
        return 0 != opt->define(
                        c_option_name.c_str(), c_name.c_str(),
                        g_variables.failed_connections_threshold > 0 ? 1 : 0);
      });
}

bool connection_control_plugin_option_usage_deinit() {
  if (option_usage) {
    delete option_usage;
    option_usage = nullptr;
  }
  return weak_option::deinit(
      reg_srv, reg_reg, [&](SERVICE_TYPE(mysql_option_tracker_option) * opt) {
        return 0 != opt->undefine(c_option_name.c_str());
      });
}

bool connection_control_plugin_option_usage_set(unsigned long every_nth_time) {
  return option_usage->set_sampled(true, every_nth_time);
}

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

#include "sql/json_duality_view/option_usage.h"

#include "mysql/components/library_mysys/option_tracker_usage.h"

#include <string>

static const std::string c_name{"mysql_server"};
static const std::string o_name{"JSON Duality View"};

unsigned long long option_tracker_json_duality_view_usage_count;

static bool set_json_duality_view_usage_count(unsigned long long new_value) {
  option_tracker_json_duality_view_usage_count = new_value;
  return false;
}

static bool cb_json_duality_view_define_failed = false;

bool jdv_options_usage_init(SERVICE_TYPE(mysql_option_tracker_option) * opt,
                            SERVICE_TYPE(registry) * srv_registry) {
  bool error = opt->define(o_name.c_str(), c_name.c_str(), true);

  error |= option_usage_read_counter(
      o_name.c_str(), &option_tracker_json_duality_view_usage_count,
      srv_registry);

  cb_json_duality_view_define_failed = option_usage_register_callback(
      o_name.c_str(), set_json_duality_view_usage_count, srv_registry);

  error |= cb_json_duality_view_define_failed;

  return error;
}

bool jdv_options_usage_deinit(SERVICE_TYPE(mysql_option_tracker_option) * opt,
                              SERVICE_TYPE(registry) * srv_registry) {
  bool error = false;
  if (!cb_json_duality_view_define_failed)
    error = option_usage_unregister_callback(
        o_name.c_str(), set_json_duality_view_usage_count, srv_registry);

  error |= (0 != opt->undefine(o_name.c_str()));
  return error;
}

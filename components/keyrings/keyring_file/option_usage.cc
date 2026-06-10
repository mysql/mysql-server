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

#include "option_usage.h"
#include <cassert>
#include "keyring_file.h"
#include "mysql/components/component_implementation.h"
#include "mysql/components/library_mysys/option_tracker_usage.h"
#include "mysql/components/service.h"
#include "mysql/components/services/mysql_option_tracker.h"
#include "mysql/components/util/weak_service_reference.h"

const std::string c_name("component_keyring_file"),
    opt_name("mysql_option_tracker_option"), c_option_name("File keyring");

typedef weak_service_reference<SERVICE_TYPE(mysql_option_tracker_option),
                               c_name, opt_name>
    weak_option;

unsigned long long opt_option_tracker_usage_file_keyring = 0;
static bool cb(unsigned long long new_value) {
  opt_option_tracker_usage_file_keyring = new_value;
  return false;
}
static bool cb_define_failed = false;
bool keyring_file_component_option_usage_init() {
  /*
    We do not keep an active reference because this component is not supposed
    to be loaded via INSTALL COMPONENT.
    It's supposed to be added via the manifest file and if it was keeping a
    reference it would be impossible to unload the option tracker component.
  */
  bool const ret = weak_option::init(
      SERVICE_PLACEHOLDER(registry), SERVICE_PLACEHOLDER(registry_registration),
      [&](SERVICE_TYPE(mysql_option_tracker_option) * opt) {
        return 0 != opt->define(c_option_name.c_str(), c_name.c_str(), 1) ||
               option_usage_read_counter(c_option_name.c_str(),
                                         &opt_option_tracker_usage_file_keyring,
                                         SERVICE_PLACEHOLDER(registry)) ||
               (cb_define_failed = option_usage_register_callback(
                    c_option_name.c_str(), cb, SERVICE_PLACEHOLDER(registry)));
      },
      false);
  return ret;
}

bool keyring_file_component_option_usage_deinit() {
  return weak_option::deinit(
      SERVICE_PLACEHOLDER(registry), SERVICE_PLACEHOLDER(registry_registration),
      [&](SERVICE_TYPE(mysql_option_tracker_option) * opt) {
        if (!cb_define_failed &&
            option_usage_unregister_callback(c_option_name.c_str(), cb,
                                             SERVICE_PLACEHOLDER(registry))) {
          return true;
        }
        return 0 != opt->undefine(c_option_name.c_str());
      });
}

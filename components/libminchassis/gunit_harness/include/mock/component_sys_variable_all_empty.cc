/* Copyright (c) 2023, 2024, Oracle and/or its affiliates.

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

#include <stdio.h>
#include "mysql/components/component_implementation.h"
#include "mysql/components/service_implementation.h"
#include "mysql/components/services/component_sys_var_service.h"

namespace mysql_service_simple_error_log_spc {

static DEFINE_BOOL_METHOD(register_variable,
                          (const char * /*component_name*/,
                           const char * /*name*/, int /*flags*/,
                           const char * /*comment*/,
                           mysql_sys_var_check_func /*check*/,
                           mysql_sys_var_update_func /*update*/,
                           void * /*check_arg*/, void * /*variable_value*/)) {
  return false;
}

static DEFINE_BOOL_METHOD(get_variable, (const char * /*component_name*/,
                                         const char * /*name*/, void ** /*val*/,
                                         size_t * /*out_length_of_val*/)) {
  return false;
}

static DEFINE_BOOL_METHOD(unregister_variable, (const char * /*component_name*/,
                                                const char * /*name*/)) {
  return false;
}

}  // namespace mysql_service_simple_error_log_spc

BEGIN_SERVICE_IMPLEMENTATION(HARNESS_COMPONENT_NAME,
                             component_sys_variable_register)
mysql_service_simple_error_log_spc::register_variable,
    mysql_service_simple_error_log_spc::get_variable,
    END_SERVICE_IMPLEMENTATION();

BEGIN_SERVICE_IMPLEMENTATION(HARNESS_COMPONENT_NAME,
                             component_sys_variable_unregister)
mysql_service_simple_error_log_spc::unregister_variable,
    END_SERVICE_IMPLEMENTATION();

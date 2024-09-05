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

#include <stdio.h>
#include "mysql/components/component_implementation.h"
#include "mysql/components/service_implementation.h"
#include "mysql/components/services/udf_registration.h"

namespace udf_registration_all_empty_spc {

static DEFINE_BOOL_METHOD(udf_register,
                          (const char * /*func_name*/,
                           enum Item_result /*return_type*/,
                           Udf_func_any /*func*/, Udf_func_init /*init_func*/,
                           Udf_func_deinit /*deinit_func*/)) {
  return 0;
}

static DEFINE_BOOL_METHOD(udf_unregister,
                          (const char * /*name*/, int * /*was_present*/)) {
  return 0;
}

}  // namespace udf_registration_all_empty_spc

BEGIN_SERVICE_IMPLEMENTATION(HARNESS_COMPONENT_NAME, udf_registration)
udf_registration_all_empty_spc::udf_register,
    udf_registration_all_empty_spc::udf_unregister END_SERVICE_IMPLEMENTATION();
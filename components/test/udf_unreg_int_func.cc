/* Copyright (c) 2017, 2024, Oracle and/or its affiliates.

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

/* This test component unregister the UDF myfunc_int in the init method
   (install) and deinit (uninstall) is empty. */

#include <ctype.h>
#include <mysql/components/component_implementation.h>
#include <mysql/components/service_implementation.h>
#include <mysql/components/services/udf_registration.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <string>

REQUIRES_SERVICE_PLACEHOLDER(udf_registration);
REQUIRES_SERVICE_PLACEHOLDER(udf_registration_aggregate);

/**************************************************************************************/

static mysql_service_status_t init() {
  int was_present = 0;
  for (int i = 0; i < 10; i++) {
    mysql_service_udf_registration->udf_unregister("myfunc_int", &was_present);
    if (was_present != 0) break;
  }
  return false;
}

static mysql_service_status_t deinit() { return false; }

BEGIN_COMPONENT_PROVIDES(test_udf_registration)
END_COMPONENT_PROVIDES();

BEGIN_COMPONENT_REQUIRES(test_udf_registration)
REQUIRES_SERVICE(udf_registration),
    REQUIRES_SERVICE(udf_registration_aggregate), END_COMPONENT_REQUIRES();

BEGIN_COMPONENT_METADATA(test_udf_registration)
METADATA("mysql.author", "Oracle Corporation"),
    METADATA("mysql.license", "GPL"), METADATA("test_property", "1"),
    END_COMPONENT_METADATA();

DECLARE_COMPONENT(test_udf_registration, "mysql:test_udf_registration")
init, deinit END_DECLARE_COMPONENT();

DECLARE_LIBRARY_COMPONENTS &COMPONENT_REF(test_udf_registration)
    END_DECLARE_LIBRARY_COMPONENTS

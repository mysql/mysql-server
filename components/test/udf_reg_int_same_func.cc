/* Copyright (c) 2017, 2018, Oracle and/or its affiliates. All rights reserved.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License, version 2.0,
as published by the Free Software Foundation.

This program is also distributed with certain software (including
but not limited to OpenSSL) that is licensed under separate terms,
as designated in a particular file or component or in included license
documentation.  The authors of MySQL hereby grant you an additional
permission to link the program and your derivative works with the
separately licensed software that they have included with MySQL.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License, version 2.0, for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/* This test component register the UDF myfunc_int in init (install) and
   unregister it in deiniti (uninstall).
   It is used for the test case to register/unregister the same method as
   another test component has registered before (or afterwards). */

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

/***************************************************************************
** UDF long long function.
** Arguments:
** initid       Return value from xxxx_init
** args         The same structure as to xxx_init. This structure
**              contains values for all parameters.
**              Note that the functions MUST check and convert all
**              to the type it wants!  Null values are represented by
**              a NULL pointer
** is_null      If the result is null, one should store 1 here.
** error        If something goes fatally wrong one should store 1 here.
**
** This function should return the result as a long long
***************************************************************************/

/* This function returns the sum of all arguments */

long long myfunc_int(UDF_INIT *, UDF_ARGS *args, char *, char *) {
  long long val = 0;
  unsigned i;

  for (i = 0; i < args->arg_count; i++) {
    val += *((long long *)args->args[i]);
  }
  return val;
}

bool myfunc_int_init(UDF_INIT *, UDF_ARGS *, char *) { return 0; }

/**************************************************************************************/

static mysql_service_status_t init() {
  bool ret_int = false;
  ret_int = mysql_service_udf_registration->udf_register(
      "myfunc_int", INT_RESULT, (Udf_func_any)myfunc_int,
      (Udf_func_init)myfunc_int_init, NULL);
  return ret_int;
}

static mysql_service_status_t deinit() {
  int was_present = 0;
  for (int i = 0; i < 10; i++) {
    mysql_service_udf_registration->udf_unregister("myfunc_int", &was_present);
    if (was_present != 0) break;
  }
  return false;
}

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

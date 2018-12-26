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

/* This test component register the avgcost method in init (install) and
   unregister it in deinit (uninstall). */

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
** Syntax for the new aggregate commands are:
** create aggregate function <function_name> returns {string|real|integer}
**                soname <name_of_shared_library>
**
** Syntax for avgcost: avgcost( t.quantity, t.price )
**      with t.quantity=integer, t.price=double
** (this example is provided by Andreas F. Bobak <bobak@relog.ch>)
****************************************************************************/

struct avgcost_data {
  unsigned long long count;
  long long totalquantity;
  double totalprice;
};

/*
** Average Cost Aggregate Function.
*/
bool avgcost_init(UDF_INIT *initid, UDF_ARGS *args, char *message) {
  struct avgcost_data *data;

  if (args->arg_count != 2) {
    strcpy(message,
           "wrong number of arguments: AVGCOST() requires two arguments");
    return 1;
  }

  if ((args->arg_type[0] != INT_RESULT) || (args->arg_type[1] != REAL_RESULT)) {
    strcpy(message,
           "wrong argument type: AVGCOST() requires an INT and a REAL");
    return 1;
  }

  /*
  **    force arguments to double.
  */
  /*args->arg_type[0]   = REAL_RESULT;
    args->arg_type[1]   = REAL_RESULT;*/

  initid->maybe_null = 0;  /* The result may be null */
  initid->decimals = 4;    /* We want 4 decimals in the result */
  initid->max_length = 20; /* 6 digits + . + 10 decimals */

  if (!(data = new (std::nothrow) avgcost_data)) {
    strcpy(message, "Couldn't allocate memory");
    return 1;
  }
  data->totalquantity = 0;
  data->totalprice = 0.0;

  initid->ptr = (char *)data;

  return 0;
}

void avgcost_deinit(UDF_INIT *initid) {
  void *void_ptr = initid->ptr;
  avgcost_data *data = static_cast<avgcost_data *>(void_ptr);
  delete data;
}

/* This is needed to get things to work in MySQL 4.1.1 and above */

void avgcost_clear(UDF_INIT *initid, unsigned char *, unsigned char *) {
  struct avgcost_data *data = (struct avgcost_data *)initid->ptr;
  data->totalprice = 0.0;
  data->totalquantity = 0;
  data->count = 0;
}

void avgcost_add(UDF_INIT *initid, UDF_ARGS *args, unsigned char *,
                 unsigned char *) {
  if (args->args[0] && args->args[1]) {
    struct avgcost_data *data = (struct avgcost_data *)initid->ptr;
    long long quantity = *((long long *)args->args[0]);
    long long newquantity = data->totalquantity + quantity;
    double price = *((double *)args->args[1]);

    data->count++;

    if (((data->totalquantity >= 0) && (quantity < 0)) ||
        ((data->totalquantity < 0) && (quantity > 0))) {
      /*
      **        passing from + to - or from - to +
      */
      if (((quantity < 0) && (newquantity < 0)) ||
          ((quantity > 0) && (newquantity > 0))) {
        data->totalprice = price * (double)newquantity;
      }
      /*
      **        sub q if totalq > 0
      **        add q if totalq < 0
      */
      else {
        price = data->totalprice / (double)data->totalquantity;
        data->totalprice = price * (double)newquantity;
      }
      data->totalquantity = newquantity;
    } else {
      data->totalquantity += quantity;
      data->totalprice += price * (double)quantity;
    }

    if (data->totalquantity == 0) data->totalprice = 0.0;
  }
}

double avgcost(UDF_INIT *initid, UDF_ARGS *, unsigned char *is_null,
               unsigned char *) {
  struct avgcost_data *data = (struct avgcost_data *)initid->ptr;
  if (!data->count || !data->totalquantity) {
    *is_null = 1;
    return 0.0;
  }

  *is_null = 0;
  return data->totalprice / (double)data->totalquantity;
}

/**************************************************************************************/

static mysql_service_status_t init() {
  bool ret_avgcost = false;
  ret_avgcost = mysql_service_udf_registration_aggregate->udf_register(
      "avgcost", REAL_RESULT, (Udf_func_any)avgcost, avgcost_init,
      avgcost_deinit, avgcost_add, avgcost_clear);
  return ret_avgcost;
}

static mysql_service_status_t deinit() {
  int was_present = 0;
  for (int i = 0; i < 10; i++) {
    mysql_service_udf_registration->udf_unregister("avgcost", &was_present);
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

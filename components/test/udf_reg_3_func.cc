/* Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.

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

/* This test component register 3 UDFsin the init method (install) and 
   unregister them in deinit (uninstall). */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <mysql/components/component_implementation.h>
#include <mysql/components/service_implementation.h>
#include <mysql/components/services/udf_registration.h>
#include <string>

REQUIRES_SERVICE_PLACEHOLDER(udf_registration);
REQUIRES_SERVICE_PLACEHOLDER(udf_registration_aggregate);

/***************************************************************************
** UDF double function.
** Arguments:
** initid       Structure filled by xxx_init
** args         The same structure as to xxx_init. This structure
**              contains values for all parameters.
**              Note that the functions MUST check and convert all
**              to the type it wants!  Null values are represented by
**              a NULL pointer
** is_null      If the result is null, one should store 1 here.
** error        If something goes fatally wrong one should store 1 here.
**
** This function should return the result.
***************************************************************************/

bool myfunc_double_init(UDF_INIT *initid, UDF_ARGS *args, char *message)
{
  unsigned i;

  if (!args->arg_count)
  {
    strcpy(message,"myfunc_double must have at least one argument");
    return 1;
  }
  /*
  ** As this function wants to have everything as strings, force all arguments
  ** to strings.
  */
  for (i=0 ; i < args->arg_count; i++)
    args->arg_type[i]=STRING_RESULT;
  initid->maybe_null=1;         /* The result may be null */
  initid->decimals=2;           /* We want 2 decimals in the result */
  initid->max_length=6;         /* 3 digits + . + 2 decimals */
  return 0;
}


double myfunc_double(UDF_INIT *, UDF_ARGS *args,
                     char *is_null, char *)
{
  unsigned long val = 0;
  unsigned long v = 0;
  unsigned i, j;

  for (i = 0; i < args->arg_count; i++)
  {
    if (args->args[i] == NULL)
      continue;
    val += args->lengths[i];
    for (j=args->lengths[i] ; j-- > 0 ;)
      v += args->args[i][j];
  }
  if (val)
    return (double) v/ (double) val;
  *is_null=1;
  return 0.0;
}

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

long long myfunc_int(UDF_INIT *, UDF_ARGS *args,
                    char *,
                    char *)
{ 
  long long val = 0;
  unsigned i;
  
  for (i = 0; i < args->arg_count; i++)
  { 
    if (args->args[i] == NULL)
      continue;
    switch (args->arg_type[i]) {        
    case STRING_RESULT:                 /* Add string lengths */
      val += args->lengths[i];
      break;
    case INT_RESULT:                    /* Add numbers */
      val += *((long long*) args->args[i]);
      break;
    case REAL_RESULT:                   /* Add numers as long long */
      val += (long long) *((double*) args->args[i]);
      break;
    default:
      break;
    }
  }
  return val;
}


bool myfunc_int_init(UDF_INIT *, UDF_ARGS *, char *)
{ 
  return 0;
}

/***************************************************************************
** Syntax for the new aggregate commands are:
** create aggregate function <function_name> returns {string|real|integer}
**                soname <name_of_shared_library>
**
** Syntax for avgcost: avgcost( t.quantity, t.price )
**      with t.quantity=integer, t.price=double
** (this example is provided by Andreas F. Bobak <bobak@relog.ch>)
****************************************************************************/


struct avgcost_data
{ 
  unsigned long long  count;
  long long     totalquantity;
  double        totalprice;
};

/*
** Average Cost Aggregate Function.
*/
bool avgcost_init( UDF_INIT* initid, UDF_ARGS* args, char* message )
{ 
  struct avgcost_data*  data;
  
  if (args->arg_count != 2)
  { 
    strcpy(
           message,
           "wrong number of arguments: AVGCOST() requires two arguments"
           );
    return 1;
  }
  
  if ((args->arg_type[0] != INT_RESULT) || (args->arg_type[1] != REAL_RESULT) )
  { 
    strcpy(
           message,
           "wrong argument type: AVGCOST() requires an INT and a REAL"
           );
    return 1;
  }
  
  /*
  **    force arguments to double.
  */
  /*args->arg_type[0]   = REAL_RESULT;
    args->arg_type[1]   = REAL_RESULT;*/

  initid->maybe_null    = 0;            /* The result may be null */
  initid->decimals      = 4;            /* We want 4 decimals in the result */
  initid->max_length    = 20;           /* 6 digits + . + 10 decimals */

  if (!(data = new (std::nothrow) avgcost_data))
  {
    strcpy(message,"Couldn't allocate memory");
    return 1;
  }
  data->totalquantity   = 0;
  data->totalprice      = 0.0;

  initid->ptr = (char*)data;

  return 0;
}

void avgcost_deinit( UDF_INIT* initid )
{
  void *void_ptr= initid->ptr;
  avgcost_data *data= static_cast<avgcost_data*>(void_ptr);
  delete data;
}


/* This is needed to get things to work in MySQL 4.1.1 and above */

void avgcost_clear(UDF_INIT* initid, char*, char*)
{
  struct avgcost_data* data = (struct avgcost_data*)initid->ptr;
  data->totalprice=     0.0;
  data->totalquantity=  0;
  data->count=          0;
}

void avgcost_add(UDF_INIT* initid, UDF_ARGS* args, char*, char*)
{
  if (args->args[0] && args->args[1])
  {
    struct avgcost_data* data   = (struct avgcost_data*)initid->ptr;
    long long quantity          = *((long long*)args->args[0]);
    long long newquantity       = data->totalquantity + quantity;
    double price                = *((double*)args->args[1]);

    data->count++;

    if (   ((data->totalquantity >= 0) && (quantity < 0))
           || ((data->totalquantity <  0) && (quantity > 0)) )
    {
      /*
      **        passing from + to - or from - to +
      */
      if (   ((quantity < 0) && (newquantity < 0))
             || ((quantity > 0) && (newquantity > 0)) )
      {
        data->totalprice        = price * (double)newquantity;
      }
      /*
      **        sub q if totalq > 0
      **        add q if totalq < 0
      */
      else
      {
        price             = data->totalprice / (double)data->totalquantity;
        data->totalprice  = price * (double)newquantity;
      }
      data->totalquantity = newquantity;
    }
    else
    {
      data->totalquantity       += quantity;
      data->totalprice          += price * (double)quantity;
    }

    if (data->totalquantity == 0)
      data->totalprice = 0.0;
  }
}

double avgcost( UDF_INIT* initid, UDF_ARGS*, char* is_null, char*)
{
  struct avgcost_data* data = (struct avgcost_data*)initid->ptr;
  if (!data->count || !data->totalquantity)
  {
    *is_null = 1;
    return 0.0;
  }

  *is_null = 0;
  return data->totalprice/(double)data->totalquantity;
}


/**************************************************************************************/

static mysql_service_status_t init()
{
  bool ret_int= false;
  ret_int= mysql_service_udf_registration->udf_register("myfunc_int",
                                                    INT_RESULT,
                                                    (Udf_func_any)myfunc_int,
                                                    (Udf_func_init)myfunc_int_init,
                                                    NULL);
//                                                    (Udf_func_deinit)myfunc_double_deinit);
  bool ret_double= false;
  ret_double= mysql_service_udf_registration->udf_register("myfunc_double",
                                                    REAL_RESULT,
                                                    (Udf_func_any)myfunc_double,
                                                    (Udf_func_init)myfunc_double_init,
                                                    NULL);
  bool ret_avgcost= false;
  ret_avgcost= mysql_service_udf_registration_aggregate->udf_register("avgcost",
                                                        REAL_RESULT,
                                                        (Udf_func_any)avgcost,
                                                        (Udf_func_init)avgcost_init,
                                                        (Udf_func_deinit)avgcost_deinit,
                                                        (Udf_func_add)avgcost_add,
                                                        (Udf_func_clear)avgcost_clear);
  return ret_int && ret_double && ret_avgcost;
}

static mysql_service_status_t deinit()
{
  int was_present= 0;
  for (int i=0; i<10; i++)
  {  
    mysql_service_udf_registration->udf_unregister("myfunc_double",
                                                 &was_present);
    if (was_present != 0) break;
  }

  was_present= 0;
  for (int i=0; i<10; i++)
  {  
    mysql_service_udf_registration->udf_unregister("myfunc_int",
                                                 &was_present);
    if (was_present != 0) break;
  }

  was_present= 0;
  for (int i=0; i<10; i++)
  {  
    mysql_service_udf_registration->udf_unregister("avgcost",
                                                 &was_present);
    if (was_present != 0) break;
  }
  return false;
}

BEGIN_COMPONENT_PROVIDES(test_udf_registration)
END_COMPONENT_PROVIDES()


BEGIN_COMPONENT_REQUIRES(test_udf_registration)
  REQUIRES_SERVICE(udf_registration)
  REQUIRES_SERVICE(udf_registration_aggregate)
END_COMPONENT_REQUIRES()

BEGIN_COMPONENT_METADATA(test_udf_registration)
  METADATA("mysql.author", "Oracle Corporation")
  METADATA("mysql.license", "GPL")
  METADATA("test_property", "1")
END_COMPONENT_METADATA()

DECLARE_COMPONENT(test_udf_registration, "mysql:test_udf_registration")
  init,
  deinit
END_DECLARE_COMPONENT()

DECLARE_LIBRARY_COMPONENTS
  &COMPONENT_REF(test_udf_registration)
END_DECLARE_LIBRARY_COMPONENTS

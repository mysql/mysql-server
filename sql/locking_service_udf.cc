/* Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "my_global.h"
#include "mysql_com.h"             // UDF_INIT
#include "locking_service.h"       // acquire_locking_service_locks

#include <string.h>

/*
  These functions are provided as UDFs rather than built-in SQL functions
  to improve flexibility - it is easier to change the functionality of
  UDFs than built-in functions as UDFs are a weaker contract with the
  user about their functionality.

  Note that these UDFs does not use the locking service plugin API as this
  is not possible with the current UDF framework implementation.
*/

// Common initialization code for get_read_lock and get_write_lock
static inline my_bool init_acquire(UDF_INIT *initid, UDF_ARGS *args, char *message)
{
  initid->maybe_null= FALSE;
  initid->decimals= 0;
  initid->max_length= 1;
  initid->ptr= NULL;
  initid->const_item= 0;
  initid->extension= NULL;

  // At least three arguments - namespace, lock, timeout
  if (args->arg_count < 3)
  {
    strcpy(message,
           "Requires at least three arguments: (namespace,lock(...),timeout).");
    return TRUE;
  }

  // Timeout is the last argument, should be INT
  if (args->arg_type[args->arg_count - 1] != INT_RESULT)
  {
    strcpy(message, "Wrong argument type - expected integer.");
    return TRUE;
  }

  // All other arguments should be strings
  for (size_t i= 0; i < (args->arg_count - 1); i++)
  {
    if (args->arg_type[i] != STRING_RESULT)
    {
      strcpy(message, "Wrong argument type - expected string.");
      return TRUE;
    }
  }

  return FALSE;
}


C_MODE_START

my_bool service_get_read_locks_init(UDF_INIT *initid, UDF_ARGS *args,
                                   char *message)
{
  return init_acquire(initid, args, message);
}


long long service_get_read_locks(UDF_INIT *initid, UDF_ARGS *args,
                                 char *is_null, char *error)
{
  const char *lock_namespace= args->args[0];
  long long timeout= *((long long*)args->args[args->arg_count - 1]);
  // For the UDF 1 == success, 0 == failure.
  return !acquire_locking_service_locks(
            NULL, lock_namespace,
            const_cast<const char**>(&args->args[1]),
            args->arg_count - 2,
            LOCKING_SERVICE_READ,
            static_cast<ulong>(timeout));
}


my_bool service_get_write_locks_init(UDF_INIT *initid, UDF_ARGS *args,
                                    char *message)
{
  return init_acquire(initid, args, message);
}


long long service_get_write_locks(UDF_INIT *initid, UDF_ARGS *args,
                                  char *is_null, char *error)
{
  const char *lock_namespace= args->args[0];
  long long timeout= *((long long*)args->args[args->arg_count - 1]);
  // For the UDF 1 == success, 0 == failure.
  return !acquire_locking_service_locks(
            NULL, lock_namespace,
            const_cast<const char**>(&args->args[1]),
            args->arg_count - 2,
            LOCKING_SERVICE_WRITE,
            static_cast<ulong>(timeout));
}


my_bool service_release_locks_init(UDF_INIT *initid, UDF_ARGS *args,
                                   char *message)
{
  initid->maybe_null= FALSE;
  initid->decimals= 0;
  initid->max_length= 1;
  initid->ptr= NULL;
  initid->const_item= 0;
  initid->extension= NULL;

  // Only one argument - lock_namespace (string)
  if (args->arg_count != 1)
  {
    strcpy(message, "Requires one argument: (lock_namespace).");
    return TRUE;
  }
  if (args->arg_type[0] != STRING_RESULT)
  {
    strcpy(message, "Wrong argument type - expected string.");
    return TRUE;
  }

  return FALSE;
}


long long service_release_locks(UDF_INIT *initid, UDF_ARGS *args,
                                char *is_null, char *error)
{
  const char *lock_namespace= args->args[0];
  // For the UDF 1 == success, 0 == failure.
  return !release_locking_service_locks(NULL, lock_namespace);
}

C_MODE_END

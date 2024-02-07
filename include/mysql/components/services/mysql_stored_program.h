/* Copyright (c) 2022, 2024, Oracle and/or its affiliates.

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

#ifndef MYSQL_STORED_PROGRAM_H
#define MYSQL_STORED_PROGRAM_H

#include <mysql/components/service.h>
#include <cstddef>  // size_t
#include <cstdint>  // intXX_t
#include "bits/mysql_stored_program_bits.h"
#include "defs/mysql_string_defs.h"

DEFINE_SERVICE_HANDLE(stored_program_handle);
DEFINE_SERVICE_HANDLE(stored_program_runtime_context);
DEFINE_SERVICE_HANDLE(time_zone_handle);
DEFINE_SERVICE_HANDLE(external_program_handle);

/**
  @file
  Services for reading and storing various stored program properties
  of the server stored program's object and its contexts.
*/

BEGIN_SERVICE_DEFINITION(mysql_stored_program_metadata_query)

/**
  Get stored program data

  Accepted keys and corresponding data type

  "sp_name"        -> mysql_cstring_with_length *
  "database_name"  -> mysql_cstring_with_length *
  "qualified_name" -> mysql_cstring_with_length *
  "sp_language"    -> mysql_cstring_with_length *
  "sp_body"        -> mysql_cstring_with_length *
  "sp_type"        -> uint16_t
  "argument_count" -> uint32_t

  @param [in]  sp_handle Handle to stored procedure structure
  @param [in]  key       Metadata name
  @param [out] value     Metadata value

  @returns Status of operation
  @retval false Success
  @retval true  Failure
*/

DECLARE_BOOL_METHOD(get, (stored_program_handle sp_handle, const char *key,
                          void *value));

END_SERVICE_DEFINITION(mysql_stored_program_metadata_query)

/*
 * Argument-related services:
 */

BEGIN_SERVICE_DEFINITION(mysql_stored_program_argument_metadata_query)

/**
  Get stored program argument metadata

  "argument_name" -> const char *
  "sql_type"      -> uint64_t
  "in_variable"   -> boolean
  "out_variable"  -> boolean
  "is_signed"     -> boolean (Applicable to numeric data types)
  "is_nullable"   -> boolean
  "byte_length"   -> uint64_t
  "char_length"   -> uint64_t (Applicable to string data types)
  "charset"       -> char const *

  @param [in]  sp_handle    Handle to stored procedure structure
  @param [in]  index        Argument index
  @param [in]  key          Metadata name
  @param [out] value        Metadata value


  @returns status of get operation
  @retval false Success
  @retval true  Failure
*/

DECLARE_BOOL_METHOD(get, (stored_program_handle sp_handle, uint16_t index,
                          const char *key, void *value));

END_SERVICE_DEFINITION(mysql_stored_program_argument_metadata_query)

BEGIN_SERVICE_DEFINITION(mysql_stored_program_return_metadata_query)

/**
  Get stored program return value metadata

  "argument_name" -> const char *
  "sql_type"      -> uint64_t
  "is_signed"     -> boolean (Applicable to numeric data types)
  "is_nullable"   -> boolean
  "byte_length"   -> uint64_t
  "char_length"   -> uint64_t (Applicable to string data types)
  "charset"       -> char const *

  @param [in]  sp_handle    Handle to stored procedure structure
  @param [in]  key          Metadata name
  @param [out] value        Metadata value


  @returns status of get operation
  @retval false Success
  @retval true  Failure
*/

DECLARE_BOOL_METHOD(get, (stored_program_handle sp_handle, const char *key,
                          void *value));

END_SERVICE_DEFINITION(mysql_stored_program_return_metadata_query)

BEGIN_SERVICE_DEFINITION(mysql_stored_program_field_name)

/**
  Returns the field name of the return value
  @param [in]  sp_runtime_context  stored program runtime context.
                                   If null, current runtime context
                                   will be used.
  @param [out] value               The field name of the return value


  @returns status of get operation
  @retval false Success
  @retval true  Failure
*/

DECLARE_BOOL_METHOD(get, (stored_program_runtime_context sp_runtime_context,
                          char const **value));

END_SERVICE_DEFINITION(mysql_stored_program_field_name)

BEGIN_SERVICE_DEFINITION(mysql_stored_program_runtime_argument_year)

/**
  @param [in]  sp_runtime_context  stored program runtime context.
                                   If null, current runtime context will
  be used.
  @param [in]  index               Argument position
  @param [out] year                Year
  @param [out] is_null             Flag to indicate if value is null

  @returns Status of operation
  @retval false Success
  @retval true  Error
*/

DECLARE_BOOL_METHOD(get, (stored_program_runtime_context sp_runtime_context,
                          uint16_t index, uint32_t *year, bool *is_null));

/**
  @param [in]  sp_runtime_context  stored program runtime context.
                                   If null, current runtime context will be
                                   used.
  @param [in] index                Argument location
  @param [in] year                 Year

  @returns Status of operation
  @retval false Success
  @retval true  Error
*/

DECLARE_BOOL_METHOD(set, (stored_program_runtime_context sp_runtime_context,
                          uint16_t index, uint32_t year));

END_SERVICE_DEFINITION(mysql_stored_program_runtime_argument_year)

BEGIN_SERVICE_DEFINITION(mysql_stored_program_runtime_argument_time)

/**
  @param [in]  sp_runtime_context  stored program runtime context.
                                   If null, current runtime context will
  be used.
  @param [in]  index               Argument position
  @param [out] hour                Hour of the day
  @param [out] minute              Minute of the hour
  @param [out] second              Second of the minute
  @param [out] micro               Micro second of the second
  @param [out] negative            Is negative
  @param [out] is_null             Flag to indicate if value is null

  @returns Status of operation
  @retval false Success
  @retval true  Error
*/

DECLARE_BOOL_METHOD(get, (stored_program_runtime_context sp_runtime_context,
                          uint16_t index, uint32_t *hour, uint32_t *minute,
                          uint32_t *second, uint64_t *micro, bool *negative,
                          bool *is_null));

/**
  @param [in]  sp_runtime_context  stored program runtime context.
                                   If null, current runtime context will be
                                   used.
  @param [in] index                Argument location
  @param [in] hour                 Hour of the day
  @param [in] minute               Minute of the hour
  @param [in] second               Second of the minute
  @param [in] micro                Micro second of the second
  @param [in] negative             Is negative
  @param [in] decimals             Precision information

  @returns Status of operation
  @retval false Success
  @retval true  Error
*/

DECLARE_BOOL_METHOD(set, (stored_program_runtime_context sp_runtime_context,
                          uint16_t index, uint32_t hour, uint32_t minute,
                          uint32_t second, uint64_t micro, bool negative,
                          uint8_t decimals));

END_SERVICE_DEFINITION(mysql_stored_program_runtime_argument_time)

BEGIN_SERVICE_DEFINITION(mysql_stored_program_runtime_argument_date)

/**
  @param [in]  sp_runtime_context  stored program runtime context.
                                   If null, current runtime context will be
                                   used.
  @param [in]  index               Argument position
  @param [out] year                Year information
  @param [out] month               Month of the year
  @param [out] day                 Day of the month
  @param [out] is_null             Flag to indicate if value is null

  @returns Status of operation
  @retval false Success
  @retval true  Error
*/

DECLARE_BOOL_METHOD(get, (stored_program_runtime_context sp_runtime_context,
                          uint16_t index, uint32_t *year, uint32_t *month,
                          uint32_t *day, bool *is_null));

/**
  @param [in] sp_runtime_context  stored program runtime context.
                                  If null, current runtime context will be
                                  used.
  @param [in] index               Argument position
  @param [in] year                Year information
  @param [in] month               Month of the year
  @param [in] day                 Day of the month

  @returns Status of operation
  @retval false Success
  @retval true  Error
*/

DECLARE_BOOL_METHOD(set, (stored_program_runtime_context sp_runtime_context,
                          uint16_t index, uint32_t year, uint32_t month,
                          uint32_t day));

END_SERVICE_DEFINITION(mysql_stored_program_runtime_argument_date)

BEGIN_SERVICE_DEFINITION(mysql_stored_program_runtime_argument_datetime)

/**
  @param [in]  sp_runtime_context  stored program runtime context.
                                   If null, current runtime context will
  be used.
  @param [in]  index               Argument position
  @param [out] year                Year part
  @param [out] month               Month of the year
  @param [out] day                 Day of the month
  @param [out] hour                Hour of the day
  @param [out] minute              Minute of the hour
  @param [out] second              Second of the minute
  @param [out] micro               Micro second of the second
  @param [out] negative            Is negative
  @param [out] time_zone_offset    Time zone offset in seconds
  @param [out] is_null             Flag to indicate if value is null

  @returns Status of operation
  @retval false Success
  @retval true  Error
*/

DECLARE_BOOL_METHOD(get, (stored_program_runtime_context sp_runtime_context,
                          uint16_t index, uint32_t *year, uint32_t *month,
                          uint32_t *day, uint32_t *hour, uint32_t *minute,
                          uint32_t *second, uint64_t *micro, bool *negative,
                          int32_t *time_zone_offset, bool *is_null));

/**
  @param [in] sp_runtime_context  stored program runtime context.
                                  If null, current runtime context will be
                                  used.
  @param [in] index               Argument position
  @param [in] year                Year part
  @param [in] month               Month of the year
  @param [in] day                 Day of the month
  @param [in] hour                Hour of the day
  @param [in] minute              Minute of the hour
  @param [in] second              Second of the minute
  @param [in] micro               Micro second of the second
  @param [in] negative            Is negative
  @param [in] decimals            Precision information
  @param [in] time_zone_offset    Time zone offset in seconds
  @param [in] time_zone_aware     Is time zone aware

  @returns Status of operation
  @retval false Success
  @retval true  Error
*/

DECLARE_BOOL_METHOD(set, (stored_program_runtime_context sp_runtime_context,
                          uint16_t index, uint32_t year, uint32_t month,
                          uint32_t day, uint32_t hour, uint32_t minute,
                          uint32_t second, uint64_t micro, bool negative,
                          uint32_t decimals, int32_t time_zone_offset,
                          bool time_zone_aware));

END_SERVICE_DEFINITION(mysql_stored_program_runtime_argument_datetime)

BEGIN_SERVICE_DEFINITION(mysql_stored_program_runtime_argument_timestamp)

/**
  @param [in]  sp_runtime_context  stored program runtime context.
                                   If null, current runtime context will
  be used.
  @param [in]  index               Argument position
  @param [out] year                Year part
  @param [out] month               Month of the year
  @param [out] day                 Day of the month
  @param [out] hour                Hour of the day
  @param [out] minute              Minute of the hour
  @param [out] second              Second of the minute
  @param [out] micro               Micro second of the second
  @param [out] negative            Is negative
  @param [out] time_zone_offset    Time zone offset in seconds
  @param [out] is_null             Flag to indicate if value is null

  @returns Status of operation
  @retval false Success
  @retval true  Error
*/

DECLARE_BOOL_METHOD(get, (stored_program_runtime_context sp_runtime_context,
                          uint16_t index, uint32_t *year, uint32_t *month,
                          uint32_t *day, uint32_t *hour, uint32_t *minute,
                          uint32_t *second, uint64_t *micro, bool *negative,
                          int32_t *time_zone_offset, bool *is_null));

/**
  @param [in] sp_runtime_context  stored program runtime context.
                                  If null, current runtime context will be
                                  used.
  @param [in] index               Argument position
  @param [in] year                Year part
  @param [in] month               Month of the year
  @param [in] day                 Day of the month
  @param [in] hour                Hour of the day
  @param [in] minute              Minute of the hour
  @param [in] second              Second of the minute
  @param [in] micro               Micro second of the second
  @param [in] negative            Is negative
  @param [in] decimals            Precision information
  @param [in] time_zone_offset    Time zone offset in seconds
  @param [in] time_zone_aware     Is time zone aware

  @returns Status of operation
  @retval false Success
  @retval true  Error
*/

DECLARE_BOOL_METHOD(set, (stored_program_runtime_context sp_runtime_context,
                          uint16_t index, uint32_t year, uint32_t month,
                          uint32_t day, uint32_t hour, uint32_t minute,
                          uint32_t second, uint64_t micro, bool negative,
                          uint32_t decimals, int32_t time_zone_offset,
                          bool time_zone_aware));

END_SERVICE_DEFINITION(mysql_stored_program_runtime_argument_timestamp)

BEGIN_SERVICE_DEFINITION(mysql_stored_program_runtime_argument_null)

/**
  Set null value

  @param [in] sp_runtime_context  stored program runtime context.
                                  If null, current runtime context will be
                                  used.
  @param [in] index               Argument position

  @returns Status of operation
  @retval false Success
  @retval true  Error
*/

DECLARE_BOOL_METHOD(set, (stored_program_runtime_context sp_runtime_context,
                          uint16_t index));
END_SERVICE_DEFINITION(mysql_stored_program_runtime_argument_null)

BEGIN_SERVICE_DEFINITION(mysql_stored_program_runtime_argument_string)

/**
  Get value of a string argument

  @param [in]      sp_runtime_context  stored program runtime context.
                                       If null, current runtime context will be
                                       used.
  @param [in]      index               Argument position
  @param [out]     value               A pointer to the current value
  @param [out]     length              Length of the current value
  @param [out]     is_null             Flag to indicate if value is null

  @returns Status of operation
  @retval false Success
  @retval true  Error
*/

DECLARE_BOOL_METHOD(get, (stored_program_runtime_context sp_runtime_context,
                          uint16_t index, char const **value, size_t *length,
                          bool *is_null));
/**
  Set value of a string argument

  @param [in] sp_runtime_context  stored program runtime context.
                                  If null, current runtime context will be
                                  used.
  @param [in] index               Argument position
  @param [in] string              Value of the argument
  @param [in] length              Length of the string

  @returns Status of operation
  @retval false Success
  @retval true  Error
*/

DECLARE_BOOL_METHOD(set, (stored_program_runtime_context sp_runtime_context,
                          uint16_t index, char const *string, size_t length));

END_SERVICE_DEFINITION(mysql_stored_program_runtime_argument_string)

BEGIN_SERVICE_DEFINITION(mysql_stored_program_runtime_argument_int)

/**
  Get value of an int argument

  @param [in]  sp_runtime_context  stored program runtime context.
                                   If null, current runtime context will be
                                   used.
  @param [in]  index               Argument position
  @param [out] result              Value of the argument
  @param [out] is_null             Flag to indicate if value is null

  @returns Status of operation
  @retval false Success
  @retval true  Error
*/

DECLARE_BOOL_METHOD(get, (stored_program_runtime_context sp_runtime_context,
                          uint16_t index, int64_t *result, bool *is_null));

/**
  Set value of an int argument

  @param [in] sp_runtime_context  stored program runtime context.
                                  If null, current runtime context will be
                                  used.
  @param [in] index               Argument position
  @param [in] value               Value to be set

  @returns Status of operation
  @retval false Success
  @retval true  Error
*/

DECLARE_BOOL_METHOD(set, (stored_program_runtime_context sp_runtime_context,
                          uint16_t index, int64_t value));

END_SERVICE_DEFINITION(mysql_stored_program_runtime_argument_int)

BEGIN_SERVICE_DEFINITION(mysql_stored_program_runtime_argument_unsigned_int)

/**
  Get value of an unsigned int argument

  @param [in]  sp_runtime_context  stored program runtime context.
                                   If null, current runtime context will be
                                   used.
  @param [in]  index               Argument position
  @param [out] result              Value of the argument
  @param [out] is_null             Flag to indicate if value is null

  @returns Status of operation
  @retval false Success
  @retval true  Error
*/

DECLARE_BOOL_METHOD(get, (stored_program_runtime_context sp_runtime_context,
                          uint16_t index, uint64_t *result, bool *is_null));

/**
  Set value of an unsigned int argument

  @param [in] sp_runtime_context  stored program runtime context.
                                  If null, current runtime context will be
                                  used.
  @param [in] index               Argument position
  @param [in] value               Value to be set

  @returns Status of operation
  @retval false Success
  @retval true  Error
*/

DECLARE_BOOL_METHOD(set, (stored_program_runtime_context sp_runtime_context,
                          uint16_t index, uint64_t value));

END_SERVICE_DEFINITION(mysql_stored_program_runtime_argument_unsigned_int)

BEGIN_SERVICE_DEFINITION(mysql_stored_program_runtime_argument_float)

/**
  Get a float value

  @param [in]  sp_runtime_context  stored program runtime context.
                                   If null, current runtime context will be
                                   used.
  @param [in]  index               Argument position
  @param [out] result              Value of the argument
  @param [out] is_null             Flag to indicate if value is null

  @returns Status of operation
  @retval false Success
  @retval true  Error
*/

DECLARE_BOOL_METHOD(get, (stored_program_runtime_context sp_runtime_context,
                          uint16_t index, double *result, bool *is_null));

/**
  Set value of a float argument

  @param [in] sp_runtime_context  stored program runtime context.
                                  If null, current runtime context will be
                                  used.
  @param [in] index               Argument position
  @param [in] value               value to be set

  @returns Status of operation
  @retval false Success
  @retval true  Error
*/

DECLARE_BOOL_METHOD(set, (stored_program_runtime_context sp_runtime_context,
                          uint16_t index, double value));

END_SERVICE_DEFINITION(mysql_stored_program_runtime_argument_float)

BEGIN_SERVICE_DEFINITION(mysql_stored_program_return_value_year)

/**
  @param [in]  sp_runtime_context  stored program runtime context.
                                   If null, current runtime context will be
                                   used.
  @param [in] year                 Year

  @returns Status of operation
  @retval false Success
  @retval true  Error
*/

DECLARE_BOOL_METHOD(set, (stored_program_runtime_context sp_runtime_context,
                          uint32_t year));

END_SERVICE_DEFINITION(mysql_stored_program_return_value_year)

BEGIN_SERVICE_DEFINITION(mysql_stored_program_return_value_time)

/**
  @param [in]  sp_runtime_context  stored program runtime context.
                                   If null, current runtime context will be
                                   used.
  @param [in] hour                 Hour of the day
  @param [in] minute               Minute of the hour
  @param [in] second               Second of the minute
  @param [in] micro                Micro second of the second
  @param [in] negative             Is negative
  @param [in] decimals             Precision information

  @returns Status of operation
  @retval false Success
  @retval true  Error
*/

DECLARE_BOOL_METHOD(set, (stored_program_runtime_context sp_runtime_context,
                          uint32_t hour, uint32_t minute, uint32_t second,
                          uint64_t micro, bool negative, uint8_t decimals));

END_SERVICE_DEFINITION(mysql_stored_program_return_value_time)

BEGIN_SERVICE_DEFINITION(mysql_stored_program_return_value_date)

/**
  @param [in] sp_runtime_context  stored program runtime context.
                                  If null, current runtime context will be
                                  used.
  @param [in] year                Year information
  @param [in] month               Month of the year
  @param [in] day                 Day of the month

  @returns Status of operation
  @retval false Success
  @retval true  Error
*/

DECLARE_BOOL_METHOD(set, (stored_program_runtime_context sp_runtime_context,
                          uint32_t year, uint32_t month, uint32_t day));

END_SERVICE_DEFINITION(mysql_stored_program_return_value_date)

BEGIN_SERVICE_DEFINITION(mysql_stored_program_return_value_datetime)

/**
  @param [in] sp_runtime_context  stored program runtime context.
                                  If null, current runtime context will be
                                  used.
  @param [in] year                Year part
  @param [in] month               Month of the year
  @param [in] day                 Day of the month
  @param [in] hour                Hour of the day
  @param [in] minute              Minute of the hour
  @param [in] second              Second of the minute
  @param [in] micro               Micro second of the second
  @param [in] negative            Is negative
  @param [in] decimals            Precision information
  @param [in] time_zone_offset    Time zone offset in seconds
  @param [in] time_zone_aware     Is time zone aware

  @returns Status of operation
  @retval false Success
  @retval true  Error
*/

DECLARE_BOOL_METHOD(set, (stored_program_runtime_context sp_runtime_context,
                          uint32_t year, uint32_t month, uint32_t day,
                          uint32_t hour, uint32_t minute, uint32_t second,
                          uint64_t micro, bool negative, uint32_t decimals,
                          int32_t time_zone_offset, bool time_zone_aware));

END_SERVICE_DEFINITION(mysql_stored_program_return_value_datetime)

BEGIN_SERVICE_DEFINITION(mysql_stored_program_return_value_timestamp)

/**
  @param [in] sp_runtime_context  stored program runtime context.
                                  If null, current runtime context will be
                                  used.
  @param [in] year                Year part
  @param [in] month               Month of the year
  @param [in] day                 Day of the month
  @param [in] hour                Hour of the day
  @param [in] minute              Minute of the hour
  @param [in] second              Second of the minute
  @param [in] micro               Micro second of the second
  @param [in] negative            Is negative
  @param [in] decimals            Precision information
  @param [in] time_zone_offset    Time zone offset in seconds
  @param [in] time_zone_aware     Is time zone aware

  @returns Status of operation
  @retval false Success
  @retval true  Error
*/

DECLARE_BOOL_METHOD(set, (stored_program_runtime_context sp_runtime_context,
                          uint32_t year, uint32_t month, uint32_t day,
                          uint32_t hour, uint32_t minute, uint32_t second,
                          uint64_t micro, bool negative, uint32_t decimals,
                          int32_t time_zone_offset, bool time_zone_aware));

END_SERVICE_DEFINITION(mysql_stored_program_return_value_timestamp)

BEGIN_SERVICE_DEFINITION(mysql_stored_program_return_value_null)

/**
  Set null value

  @param [in] sp_runtime_context  stored program runtime context.
                                  If null, current runtime context will be
                                  used.

  @returns Status of operation
  @retval false Success
  @retval true  Error
*/

DECLARE_BOOL_METHOD(set, (stored_program_runtime_context sp_runtime_context));
END_SERVICE_DEFINITION(mysql_stored_program_return_value_null)

BEGIN_SERVICE_DEFINITION(mysql_stored_program_return_value_string)

/**
  Set value of a string return value

  @param [in] sp_runtime_context  stored program runtime context.
                                  If null, current runtime context will be
                                  used.
  @param [in] string              Value of the argument
  @param [in] length              Length of the string

  @returns Status of operation
  @retval false Success
  @retval true  Error
*/

DECLARE_BOOL_METHOD(set, (stored_program_runtime_context sp_runtime_context,
                          char const *string, size_t length));

END_SERVICE_DEFINITION(mysql_stored_program_return_value_string)

BEGIN_SERVICE_DEFINITION(mysql_stored_program_return_value_int)

/**
  Set value of an int return value

  @param [in] sp_runtime_context  stored program runtime context.
                                  If null, current runtime context will be
                                  used.
  @param [in] value               Value to be set

  @returns Status of operation
  @retval false Success
  @retval true  Error
*/

DECLARE_BOOL_METHOD(set, (stored_program_runtime_context sp_runtime_context,
                          int64_t value));

END_SERVICE_DEFINITION(mysql_stored_program_return_value_int)

BEGIN_SERVICE_DEFINITION(mysql_stored_program_return_value_unsigned_int)

/**
  Set value of an unsigned int return value

  @param [in] sp_runtime_context  stored program runtime context.
                                  If null, current runtime context will be
                                  used.
  @param [in] value               Value to be set

  @returns Status of operation
  @retval false Success
  @retval true  Error
*/

DECLARE_BOOL_METHOD(set, (stored_program_runtime_context sp_runtime_context,
                          uint64_t value));

END_SERVICE_DEFINITION(mysql_stored_program_return_value_unsigned_int)

BEGIN_SERVICE_DEFINITION(mysql_stored_program_return_value_float)

/**
  Set value of a float return value

  @param [in] sp_runtime_context  stored program runtime context.
                                  If null, current runtime context will be
                                  used.
  @param [in] value               value to be set

  @returns Status of operation
  @retval false Success
  @retval true  Error
*/

DECLARE_BOOL_METHOD(set, (stored_program_runtime_context sp_runtime_context,
                          double value));

END_SERVICE_DEFINITION(mysql_stored_program_return_value_float)

/**
  @ingroup group_components_services_stored_programs

  Service to get and change the stored program's external language handle.

  In general, the server stored program object does handle
  the lifetime of the external language objects.
  It initiates the creation, parsing, execution and destruction of such objects.
  But, in rare cases, the external language component may request the server to
  change its external language object. Example of such cases are:
   * The stored program is aborted. The external language object needs to be
     destroyed and detached from the server object.
   * The session was reset. Either by explicit user demand, or when incompatible
     changes happen such as timezone change. All of its objects need to be
     destroyed and detached.
   * The limit of external language objects is reached. The oldest objects
     may be destroyed and detached.
   * The limit of external language sessions is reached. The oldest sessions
     may be destroyed, together with their language objects.
   * The external language object is changed. The new object needs to replace
     the existing one.
  @note Both the external language session and external language object
        services support lazy (re)initialization. The current object may be
        detached and destroyed. When another request to
        the same stored program comes, a new object will be created.

  Used approximately as follows:

  @code
  // Get the current external_program_handle.
  external_program_handle old_sp = nullptr;
  if (SERVICE_PLACEHOLDER(mysql_stored_program_external_program_handle)
                             ->get(stored_program_handle, &old_sp))
    return 1;

  // Detach and destroy the current external_program_handle.
  if (SERVICE_PLACEHOLDER(mysql_stored_program_external_program_handle)
                             ->set(stored_program_handle, nullptr))
    return 1;
  if (old_sp) destroy_stored_program(old_sp);

  // Create and attach a new external_program_handle.
  external_program_handle old_sp = create_stored_program();
  return SERVICE_PLACEHOLDER(mysql_stored_program_external_program_handle)
                             ->set(stored_program_handle, nullptr);

  external_program_handle new_sp = create_stored_program();
  return SERVICE_PLACEHOLDER(mysql_stored_program_external_program_handle)
                             ->set(stored_program_handle, new_sp);

  @endcode
*/
BEGIN_SERVICE_DEFINITION(mysql_stored_program_external_program_handle)

/**
  Obtain the currently attached Language Component's Stored Program
  from the server object.
  @note: Only stored_program_handle belonging to current thread are supported.

  @param [in] sp      stored_program_handle
  @param [out] value  Language Component's Stored Program.

  @returns Status of operation
  @retval false Success
  @retval true  Error
*/

DECLARE_BOOL_METHOD(get,
                    (stored_program_handle sp, external_program_handle *value));

/**
  Attach or detach the Language Component's Stored Program from the server
  object. In order to detach the current Stored Program from the server
   - provide nullptr value.
  @note: Only stored_program_handle belonging to current thread are supported.

  @param [in] sp      stored_program_handle
  @param [in] value   Language Component's Stored Program.
                      Use nullptr to detach the current value from the server.

  @returns Status of operation
  @retval false Success
  @retval true  Error
*/

DECLARE_BOOL_METHOD(set,
                    (stored_program_handle sp, external_program_handle value));

END_SERVICE_DEFINITION(mysql_stored_program_external_program_handle)
#endif /* MYSQL_STORED_PROGRAM_H */

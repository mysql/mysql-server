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

#include "mysql_stored_program_imp.h"
#include <cstring>    // strcmp
#include "my_time.h"  // check_datetime_range
#include "mysql/components/services/bits/stored_program_bits.h"  // stored_program_argument_type
#include "mysql_time.h"
#include "sql/current_thd.h"
#include "sql/item_timefunc.h"  // Item_time_literal
#include "sql/sp_cache.h"       // sp_cache
#include "sql/sp_head.h"        // sp_head
#include "sql/sp_pcontext.h"    // sp_runtime_ctx
#include "sql/sp_rcontext.h"
#include "sql/tztime.h"  // Time_zone

constexpr auto MYSQL_SUCCESS = 0;
constexpr auto MYSQL_FAILURE = 1;

/**
  Implementation of the mysql_stored_program services
*/
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
  @note Have the key at least 7 characters long, with unique first 8 characters.

  @param [in]  sp_handle Handle to stored procedure structure
  @param [in]  key       Metadata name
  @param [out] value     Metadata value

  @returns Status of operation
  @retval MYSQL_SUCCESS Success
  @retval MYSQL_FAILURE Failure
*/

DEFINE_BOOL_METHOD(mysql_stored_program_metadata_query_imp::get,
                   (stored_program_handle sp_handle, const char *key,
                    void *value)) {
  auto sp = reinterpret_cast<sp_head *>(sp_handle);
  if (strcmp("sp_name", key) == 0)
    *reinterpret_cast<MYSQL_LEX_STRING *>(value) = sp->m_name;
  else if (strcmp("database_name", key) == 0)
    *reinterpret_cast<MYSQL_LEX_STRING *>(value) = sp->m_db;
  else if (strcmp("qualified_name", key) == 0)
    *reinterpret_cast<MYSQL_LEX_STRING *>(value) = sp->m_qname;
  else if (strcmp("sp_language", key) == 0)
    *reinterpret_cast<MYSQL_LEX_CSTRING *>(value) = sp->m_chistics->language;
  else if (strcmp("sp_body", key) == 0)
    *reinterpret_cast<MYSQL_LEX_CSTRING *>(value) = sp->m_body;
  else if (strcmp("sp_type", key) == 0)
    switch (sp->m_type) {
      case enum_sp_type::FUNCTION:
        *reinterpret_cast<uint16_t *>(value) =
            MYSQL_STORED_PROGRAM_DATA_QUERY_TYPE_FUNCTION;
        break;
      case enum_sp_type::PROCEDURE:
        *reinterpret_cast<uint16_t *>(value) =
            MYSQL_STORED_PROGRAM_DATA_QUERY_TYPE_PROCEDURE;
        break;
      default:
        return MYSQL_FAILURE;  // unknown type
    }
  else if (strcmp("argument_count", key) == 0)
    *reinterpret_cast<uint32_t *>(value) =
        sp->get_root_parsing_context()->context_var_count();
  else
    return MYSQL_FAILURE;
  return MYSQL_SUCCESS;
}

/*
 * Argument-related services:
 */

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
  @note Have the key at least 7 characters long, with unique first 8 characters.

  @returns status of get operation
  @retval MYSQL_SUCCESS Success
  @retval MYSQL_FAILURE Failure
*/
static int get_field_metadata_internal(Create_field &field, bool input,
                                       bool output, const char *key,
                                       void *value) {
  if (strcmp("argument_name", key) == 0)
    *reinterpret_cast<char const **>(value) = field.field_name;
  else if (strcmp("in_variable", key) == 0)
    *reinterpret_cast<bool *>(value) = input;
  else if (strcmp("out_variable", key) == 0)
    *reinterpret_cast<bool *>(value) = output;
  else if (strcmp("is_signed", key) == 0)
    *reinterpret_cast<bool *>(value) = !field.is_unsigned;
  else if (strcmp("is_nullable", key) == 0)
    *reinterpret_cast<bool *>(value) = field.is_nullable;
  else if (strcmp("sql_type", key) == 0)
    switch (field.sql_type) {
      case MYSQL_TYPE_DECIMAL:
        *reinterpret_cast<uint64_t *>(value) = MYSQL_SP_ARG_TYPE_DECIMAL;
        break;
      case MYSQL_TYPE_TINY:
        *reinterpret_cast<uint64_t *>(value) = MYSQL_SP_ARG_TYPE_TINY;
        break;
      case MYSQL_TYPE_SHORT:
        *reinterpret_cast<uint64_t *>(value) = MYSQL_SP_ARG_TYPE_SHORT;
        break;
      case MYSQL_TYPE_LONG:
        *reinterpret_cast<uint64_t *>(value) = MYSQL_SP_ARG_TYPE_LONG;
        break;
      case MYSQL_TYPE_FLOAT:
        *reinterpret_cast<uint64_t *>(value) = MYSQL_SP_ARG_TYPE_FLOAT;
        break;
      case MYSQL_TYPE_DOUBLE:
        *reinterpret_cast<uint64_t *>(value) = MYSQL_SP_ARG_TYPE_DOUBLE;
        break;
      case MYSQL_TYPE_NULL:
        *reinterpret_cast<uint64_t *>(value) = MYSQL_SP_ARG_TYPE_NULL;
        break;
      case MYSQL_TYPE_TIMESTAMP:
        *reinterpret_cast<uint64_t *>(value) = MYSQL_SP_ARG_TYPE_TIMESTAMP;
        break;
      case MYSQL_TYPE_LONGLONG:
        *reinterpret_cast<uint64_t *>(value) = MYSQL_SP_ARG_TYPE_LONGLONG;
        break;
      case MYSQL_TYPE_INT24:
        *reinterpret_cast<uint64_t *>(value) = MYSQL_SP_ARG_TYPE_INT24;
        break;
      case MYSQL_TYPE_DATE:
        *reinterpret_cast<uint64_t *>(value) = MYSQL_SP_ARG_TYPE_DATE;
        break;
      case MYSQL_TYPE_TIME:
        *reinterpret_cast<uint64_t *>(value) = MYSQL_SP_ARG_TYPE_TIME;
        break;
      case MYSQL_TYPE_DATETIME:
        *reinterpret_cast<uint64_t *>(value) = MYSQL_SP_ARG_TYPE_DATETIME;
        break;
      case MYSQL_TYPE_YEAR:
        *reinterpret_cast<uint64_t *>(value) = MYSQL_SP_ARG_TYPE_YEAR;
        break;
      case MYSQL_TYPE_NEWDATE:
        *reinterpret_cast<uint64_t *>(value) = MYSQL_SP_ARG_TYPE_NEWDATE;
        break;
      case MYSQL_TYPE_VARCHAR:
        *reinterpret_cast<uint64_t *>(value) = MYSQL_SP_ARG_TYPE_VARCHAR;
        break;
      case MYSQL_TYPE_BIT:
        *reinterpret_cast<uint64_t *>(value) = MYSQL_SP_ARG_TYPE_BIT;
        break;
      case MYSQL_TYPE_TIMESTAMP2:
        *reinterpret_cast<uint64_t *>(value) = MYSQL_SP_ARG_TYPE_TIMESTAMP2;
        break;
      case MYSQL_TYPE_DATETIME2:
        *reinterpret_cast<uint64_t *>(value) = MYSQL_SP_ARG_TYPE_DATETIME2;
        break;
      case MYSQL_TYPE_TIME2:
        *reinterpret_cast<uint64_t *>(value) = MYSQL_SP_ARG_TYPE_TIME2;
        break;
      case MYSQL_TYPE_TYPED_ARRAY:
        *reinterpret_cast<uint64_t *>(value) = MYSQL_SP_ARG_TYPE_TYPED_ARRAY;
        break;
      case MYSQL_TYPE_INVALID:
        *reinterpret_cast<uint64_t *>(value) = MYSQL_SP_ARG_TYPE_INVALID;
        break;
      case MYSQL_TYPE_BOOL:
        *reinterpret_cast<uint64_t *>(value) = MYSQL_SP_ARG_TYPE_BOOL;
        break;
      case MYSQL_TYPE_JSON:
        *reinterpret_cast<uint64_t *>(value) = MYSQL_SP_ARG_TYPE_JSON;
        break;
      case MYSQL_TYPE_NEWDECIMAL:
        *reinterpret_cast<uint64_t *>(value) = MYSQL_SP_ARG_TYPE_NEWDECIMAL;
        break;
      case MYSQL_TYPE_ENUM:
        *reinterpret_cast<uint64_t *>(value) = MYSQL_SP_ARG_TYPE_ENUM;
        break;
      case MYSQL_TYPE_SET:
        *reinterpret_cast<uint64_t *>(value) = MYSQL_SP_ARG_TYPE_SET;
        break;
      case MYSQL_TYPE_TINY_BLOB:
        *reinterpret_cast<uint64_t *>(value) = MYSQL_SP_ARG_TYPE_TINY_BLOB;
        break;
      case MYSQL_TYPE_MEDIUM_BLOB:
        *reinterpret_cast<uint64_t *>(value) = MYSQL_SP_ARG_TYPE_MEDIUM_BLOB;
        break;
      case MYSQL_TYPE_LONG_BLOB:
        *reinterpret_cast<uint64_t *>(value) = MYSQL_SP_ARG_TYPE_LONG_BLOB;
        break;
      case MYSQL_TYPE_BLOB:
        *reinterpret_cast<uint64_t *>(value) = MYSQL_SP_ARG_TYPE_BLOB;
        break;
      case MYSQL_TYPE_VAR_STRING:
        *reinterpret_cast<uint64_t *>(value) = MYSQL_SP_ARG_TYPE_VAR_STRING;
        break;
      case MYSQL_TYPE_STRING:
        *reinterpret_cast<uint64_t *>(value) = MYSQL_SP_ARG_TYPE_STRING;
        break;
      case MYSQL_TYPE_GEOMETRY:
        *reinterpret_cast<uint64_t *>(value) = MYSQL_SP_ARG_TYPE_GEOMETRY;
        break;
      default:
        return MYSQL_FAILURE;
    }
  else if (strcmp("byte_length", key) == 0)
    *reinterpret_cast<size_t *>(value) = field.pack_length();
  else if (strcmp("char_length", key) == 0)
    *reinterpret_cast<size_t *>(value) = field.key_length();
  else if (strcmp("charset", key) == 0)
    *reinterpret_cast<char const **>(value) = field.charset->csname;
  else if (strcmp("decimals", key) == 0)
    *reinterpret_cast<uint32_t *>(value) = field.decimals;
  else
    return MYSQL_FAILURE;
  return MYSQL_SUCCESS;
}

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
  @note Have the key at least 7 characters long, with unique first 8 characters.

  @param [in]  sp_handle    Handle to stored procedure structure
  @param [in]  index        Argument index
  @param [in]  key          Metadata name
  @param [out] value        Metadata value

  @returns status of get operation
  @retval MYSQL_SUCCESS Success
  @retval MYSQL_FAILURE Failure
*/

DEFINE_BOOL_METHOD(mysql_stored_program_argument_metadata_query_imp::get,
                   (stored_program_handle sp_handle, uint16_t index,
                    const char *key, void *value)) {
  auto sp = reinterpret_cast<sp_head *>(sp_handle);
  auto context = sp->get_root_parsing_context();
  auto variable = context->find_variable(index);
  auto input = variable->mode == sp_variable::enum_mode::MODE_IN ||
               variable->mode == sp_variable::enum_mode::MODE_INOUT;
  auto output = variable->mode == sp_variable::enum_mode::MODE_OUT ||
                variable->mode == sp_variable::enum_mode::MODE_INOUT;

  return get_field_metadata_internal(variable->field_def, input, output, key,
                                     value);
}
/**
  Get stored program return metadata

  "argument_name" -> const char *
  "sql_type"      -> uint64_t
  "in_variable"   -> boolean
  "out_variable"  -> boolean
  "is_signed"     -> boolean (Applicable to numeric data types)
  "is_nullable"   -> boolean
  "byte_length"   -> uint64_t
  "char_length"   -> uint64_t (Applicable to string data types)
  "charset"       -> char const *
  @note Have the key at least 7 characters long, with unique first 8 characters.

  @param [in]  sp_handle    Handle to stored procedure structure
  @param [in]  key          Metadata name
  @param [out] value        Metadata value

  @returns status of get operation
  @retval MYSQL_SUCCESS Success
  @retval MYSQL_FAILURE Failure
*/

DEFINE_BOOL_METHOD(mysql_stored_program_return_metadata_query_imp::get,
                   (stored_program_handle sp_handle, const char *key,
                    void *value)) {
  auto sp = reinterpret_cast<sp_head *>(sp_handle);
  return get_field_metadata_internal(sp->m_return_field_def, false, true, key,
                                     value);
}

auto static set_variable(stored_program_runtime_context sp_runtime_context,
                         Item *item, int index) -> int {
  assert(index >= 0);
  if (index < 0) return MYSQL_FAILURE;
  auto runtime_context = reinterpret_cast<sp_rcontext *>(sp_runtime_context);
  if (runtime_context == nullptr) runtime_context = current_thd->sp_runtime_ctx;
  return runtime_context->set_variable(current_thd, index, &item);
}

auto static set_return_value(stored_program_runtime_context sp_runtime_context,
                             Item *item) -> int {
  auto runtime_context = reinterpret_cast<sp_rcontext *>(sp_runtime_context);
  if (runtime_context == nullptr) runtime_context = current_thd->sp_runtime_ctx;
  return runtime_context->set_return_value(current_thd, &item);
}

auto static get_item(stored_program_runtime_context sp_runtime_context,
                     int index) -> Item * {
  assert(index >= 0);
  if (index < 0) return nullptr;
  auto runtime_context = reinterpret_cast<sp_rcontext *>(sp_runtime_context);
  if (runtime_context == nullptr) runtime_context = current_thd->sp_runtime_ctx;
  return runtime_context->get_item(index);
}

auto static get_return_field(stored_program_runtime_context sp_runtime_context)
    -> Field * {
  auto runtime_context = reinterpret_cast<sp_rcontext *>(sp_runtime_context);
  if (runtime_context == nullptr) runtime_context = current_thd->sp_runtime_ctx;
  return runtime_context->get_return_field();
}

/**
  Returns the field name of the return value
  @param [in]  sp_runtime_context  stored program runtime context.
                                   If null, current runtime context
                                   will be used.
  @param [out] value               Metadata value


  @returns status of get operation
  @retval false Success
  @retval true  Failure
*/

DEFINE_BOOL_METHOD(mysql_stored_program_field_name_imp::get_name,
                   (stored_program_runtime_context sp_runtime_context,
                    char const **value)) {
  auto field = get_return_field(sp_runtime_context);
  if (field == nullptr) return MYSQL_FAILURE;
  *value = field->field_name;
  return MYSQL_SUCCESS;
}

/**
  @param [in]  sp_runtime_context  stored program runtime context.
                                   If null, current runtime context will
  be used.
  @param [in]  index               Argument location
  @param [out] year                Year
  @param [out] is_null             Flag to indicate if value is null

  @returns Status of operation
  @retval MYSQL_SUCCESS Success
  @retval MYSQL_FAILURE Failure
*/

DEFINE_BOOL_METHOD(mysql_stored_program_runtime_argument_year_imp::get,
                   (stored_program_runtime_context sp_runtime_context,
                    uint16_t index, uint32_t *year, bool *is_null)) {
  auto item = get_item(sp_runtime_context, index);
  if (item == nullptr) return MYSQL_FAILURE;
  *is_null = item->is_null();
  if (!*is_null) *year = item->val_int();
  return MYSQL_SUCCESS;
}

/**
  @param [in]  sp_runtime_context  stored program runtime context.
                                   If null, current runtime context will
  be used.
  @param [in]  index               Argument location
  @param [out] hour                Hour of the day
  @param [out] minute              Minute of the hour
  @param [out] second              Second of the minute
  @param [out] micro               Micro second of the second
  @param [out] negative            Is negative
  @param [out] is_null             Flag to indicate if value is null

  @returns Status of operation
  @retval MYSQL_SUCCESS Success
  @retval MYSQL_FAILURE Failure
*/

DEFINE_BOOL_METHOD(mysql_stored_program_runtime_argument_time_imp::get,
                   (stored_program_runtime_context sp_runtime_context,
                    uint16_t index, uint32_t *hour, uint32_t *minute,
                    uint32_t *second, uint64_t *micro, bool *negative,
                    bool *is_null)) {
  auto item = get_item(sp_runtime_context, index);
  if (item == nullptr) return MYSQL_FAILURE;
  *is_null = item->is_null();
  if (*is_null) return MYSQL_SUCCESS;
  auto date = MYSQL_TIME{};
  item->get_time(&date);
  *hour = date.hour;
  *minute = date.minute;
  *second = date.second;
  *micro = date.second_part;
  *negative = date.neg;
  return MYSQL_SUCCESS;
}

/**
  Helper function that retrieves runtime argument value for DATETIME
  and TIMESTAMP types.

  @param [in]  sp_runtime_context  stored program runtime context.
                                   If null, current runtime context will
  be used.
  @param [in]  index               Argument location
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
  @retval MYSQL_SUCCESS Success
  @retval MYSQL_FAILURE Failure
*/

static int runtime_argument_datetime_get(
    stored_program_runtime_context sp_runtime_context, uint16_t index,
    uint32_t *year, uint32_t *month, uint32_t *day, uint32_t *hour,
    uint32_t *minute, uint32_t *second, uint64_t *micro, bool *negative,
    int32_t *time_zone_offset, bool *is_null) {
  auto item = get_item(sp_runtime_context, index);
  if (item == nullptr) return MYSQL_FAILURE;
  *is_null = item->is_null();
  if (*is_null) return MYSQL_SUCCESS;
  auto date = MYSQL_TIME{};
  item->get_time(&date);
  *year = date.year;
  *month = date.month;
  *day = date.day;
  *hour = date.hour;
  *minute = date.minute;
  *second = date.second;
  *micro = date.second_part;
  *negative = date.neg;
  *time_zone_offset = date.time_zone_displacement;
  assert(date.time_type == enum_mysql_timestamp_type::MYSQL_TIMESTAMP_DATETIME);
  return MYSQL_SUCCESS;
}

/**
  @param [in]  sp_runtime_context  stored program runtime context.
                                   If null, current runtime context will
  be used.
  @param [in]  index               Argument location
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
  @retval MYSQL_SUCCESS Success
  @retval MYSQL_FAILURE Failure
*/

DEFINE_BOOL_METHOD(mysql_stored_program_runtime_argument_datetime_imp::get,
                   (stored_program_runtime_context sp_runtime_context,
                    uint16_t index, uint32_t *year, uint32_t *month,
                    uint32_t *day, uint32_t *hour, uint32_t *minute,
                    uint32_t *second, uint64_t *micro, bool *negative,
                    int32_t *time_zone_offset, bool *is_null)) {
  return runtime_argument_datetime_get(sp_runtime_context, index, year, month,
                                       day, hour, minute, second, micro,
                                       negative, time_zone_offset, is_null);
}

/**
  @param [in]  sp_runtime_context  stored program runtime context.
                                   If null, current runtime context will
  be used.
  @param [in]  index               Argument location
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
  @retval MYSQL_SUCCESS Success
  @retval MYSQL_FAILURE Failure
*/

DEFINE_BOOL_METHOD(mysql_stored_program_runtime_argument_timestamp_imp::get,
                   (stored_program_runtime_context sp_runtime_context,
                    uint16_t index, uint32_t *year, uint32_t *month,
                    uint32_t *day, uint32_t *hour, uint32_t *minute,
                    uint32_t *second, uint64_t *micro, bool *negative,
                    int32_t *time_zone_offset, bool *is_null)) {
  return runtime_argument_datetime_get(sp_runtime_context, index, year, month,
                                       day, hour, minute, second, micro,
                                       negative, time_zone_offset, is_null);
}

/**
  @param [in]  sp_runtime_context  stored program runtime context.
                                   If null, current runtime context will be
                                   used.
  @param [in] index                Argument location
  @param [in] year                 Year

  @returns Status of operation
  @retval MYSQL_SUCCESS Success
  @retval MYSQL_FAILURE Failure
*/

DEFINE_BOOL_METHOD(mysql_stored_program_runtime_argument_year_imp::set,
                   (stored_program_runtime_context sp_runtime_context,
                    uint16_t index, uint32_t year)) {
  Item *item = new Item_int(static_cast<long long>(year));
  return set_variable(sp_runtime_context, item, index);
}

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
  @retval MYSQL_SUCCESS Success
  @retval MYSQL_FAILURE Failure
*/

DEFINE_BOOL_METHOD(mysql_stored_program_runtime_argument_time_imp::set,
                   (stored_program_runtime_context sp_runtime_context,
                    uint16_t index, uint32_t hour, uint32_t minute,
                    uint32_t second, uint64_t micro, bool negative,
                    uint8_t decimals)) {
  auto time = MYSQL_TIME{static_cast<unsigned int>(0),
                         static_cast<unsigned int>(0),
                         static_cast<unsigned int>(0),
                         static_cast<unsigned int>(hour),
                         static_cast<unsigned int>(minute),
                         static_cast<unsigned int>(second),
                         static_cast<unsigned long>(micro),
                         negative,
                         MYSQL_TIMESTAMP_TIME,
                         {}};
  if (check_datetime_range(time)) return MYSQL_FAILURE;
  auto item = new Item_time_literal(&time, decimals);
  return set_variable(sp_runtime_context, item, index);
}

/**
  @param [in]  sp_runtime_context  stored program runtime context.
                                   If null, current runtime context will be
                                   used.
  @param [in]  index               Argument location
  @param [out] year                Year information
  @param [out] month               Month of the year
  @param [out] day                 Day of the month
  @param [out] is_null             Flag to indicate if value is null

  @returns Status of operation
  @retval MYSQL_SUCCESS Success
  @retval MYSQL_FAILURE Failure
*/

DEFINE_BOOL_METHOD(mysql_stored_program_runtime_argument_date_imp::get,
                   (stored_program_runtime_context sp_runtime_context,
                    uint16_t index, uint32_t *year, uint32_t *month,
                    uint32_t *day, bool *is_null)) {
  auto item = get_item(sp_runtime_context, index);
  if (item == nullptr) return MYSQL_FAILURE;
  *is_null = item->is_null();
  if (*is_null) return MYSQL_SUCCESS;
  auto date = MYSQL_TIME{};
  item->get_date(&date, TIME_FUZZY_DATE);
  *year = date.year;
  *month = date.month;
  *day = date.day;
  return MYSQL_SUCCESS;
}

/**
  @param [in] sp_runtime_context  stored program runtime context.
                                  If null, current runtime context will be
                                  used.
  @param [in] index               Argument location
  @param [in] year                Year information
  @param [in] month               Month of the year
  @param [in] day                 Day of the month

  @returns Status of operation
  @retval MYSQL_SUCCESS Success
  @retval MYSQL_FAILURE Failure
*/

DEFINE_BOOL_METHOD(mysql_stored_program_runtime_argument_date_imp::set,
                   (stored_program_runtime_context sp_runtime_context,
                    uint16_t index, uint32_t year, uint32_t month,
                    uint32_t day)) {
  auto time = MYSQL_TIME{
      year, month, day, {}, {}, {}, {}, {}, MYSQL_TIMESTAMP_DATE, {}};
  if (check_datetime_range(time)) return MYSQL_FAILURE;
  auto item = new Item_date_literal(&time);
  return set_variable(sp_runtime_context, item, index);
}

/**
  Helper function that sets runtime argument value for DATETIME
  and TIMESTAMP types.

  @param [in] sp_runtime_context  stored program runtime context.
                                  If null, current runtime context will be
                                  used.
  @param [in] index               Argument location
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
  @retval MYSQL_SUCCESS Success
  @retval MYSQL_FAILURE Failure
*/

static int runtime_argument_datetime_set(
    stored_program_runtime_context sp_runtime_context, uint16_t index,
    uint32_t year, uint32_t month, uint32_t day, uint32_t hour, uint32_t minute,
    uint32_t second, uint64_t micro, bool negative, uint32_t decimals,
    int32_t time_zone_offset, bool time_zone_aware) {
  enum_mysql_timestamp_type ts_type{};
  if (time_zone_aware) {
    ts_type = enum_mysql_timestamp_type::MYSQL_TIMESTAMP_DATETIME_TZ;
  } else {
    ts_type = enum_mysql_timestamp_type::MYSQL_TIMESTAMP_DATETIME;
  }
  auto time = MYSQL_TIME{static_cast<unsigned int>(year),
                         static_cast<unsigned int>(month),
                         static_cast<unsigned int>(day),
                         static_cast<unsigned int>(hour),
                         static_cast<unsigned int>(minute),
                         static_cast<unsigned int>(second),
                         static_cast<unsigned long>(micro),
                         negative,
                         ts_type,
                         static_cast<int>(time_zone_offset)};
  if (check_datetime_range(time)) return MYSQL_FAILURE;
  auto item =
      new Item_datetime_literal(&time, decimals, current_thd->time_zone());
  return set_variable(sp_runtime_context, item, index);
}

/**
  @param [in] sp_runtime_context  stored program runtime context.
                                  If null, current runtime context will be
                                  used.
  @param [in] index               Argument location
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
  @retval MYSQL_SUCCESS Success
  @retval MYSQL_FAILURE Failure
*/

DEFINE_BOOL_METHOD(mysql_stored_program_runtime_argument_datetime_imp::set,
                   (stored_program_runtime_context sp_runtime_context,
                    uint16_t index, uint32_t year, uint32_t month, uint32_t day,
                    uint32_t hour, uint32_t minute, uint32_t second,
                    uint64_t micro, bool negative, uint32_t decimals,
                    int32_t time_zone_offset, bool time_zone_aware)) {
  return runtime_argument_datetime_set(
      sp_runtime_context, index, year, month, day, hour, minute, second, micro,
      negative, decimals, time_zone_offset, time_zone_aware);
}

/**
  @param [in] sp_runtime_context  stored program runtime context.
                                  If null, current runtime context will be
                                  used.
  @param [in] index               Argument location
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
  @retval MYSQL_SUCCESS Success
  @retval MYSQL_FAILURE Failure
*/

DEFINE_BOOL_METHOD(mysql_stored_program_runtime_argument_timestamp_imp::set,
                   (stored_program_runtime_context sp_runtime_context,
                    uint16_t index, uint32_t year, uint32_t month, uint32_t day,
                    uint32_t hour, uint32_t minute, uint32_t second,
                    uint64_t micro, bool negative, uint32_t decimals,
                    int32_t time_zone_offset, bool time_zone_aware)) {
  return runtime_argument_datetime_set(
      sp_runtime_context, index, year, month, day, hour, minute, second, micro,
      negative, decimals, time_zone_offset, time_zone_aware);
}

/**
  Set null value

  @param [in] sp_runtime_context  stored program runtime context.
                                  If null, current runtime context will be
                                  used.
  @param [in] index               Argument location

  @returns Status of operation
  @retval MYSQL_SUCCESS Success
  @retval MYSQL_FAILURE Failure
*/

DEFINE_BOOL_METHOD(mysql_stored_program_runtime_argument_null_imp::set,
                   (stored_program_runtime_context sp_runtime_context,
                    uint16_t index)) {
  auto item = new Item_null{};
  return set_variable(sp_runtime_context, item, index);
}

/**
  Get value of a string argument
  @note: A pointer to the original data is returned. No guarantee is provided
         that the original data will not be consecutively modified.
         If the data is to be stored, it needs to be copied locally.

  @param [in]      sp_runtime_context  stored program runtime context.
                                       If null, current runtime context will
  be used.
  @param [in]      index               Argument location
  @param [out]     value               A pointer to the original string
  @param [out]     length              Length of the original string
  @param [out]     is_null             Flag to indicate if value is null

  @returns Status of operation
  @retval MYSQL_SUCCESS Success
  @retval MYSQL_FAILURE Failure
*/

DEFINE_BOOL_METHOD(mysql_stored_program_runtime_argument_string_imp::get,
                   (stored_program_runtime_context sp_runtime_context,
                    uint16_t index, char const **value, size_t *length,
                    bool *is_null)) {
  auto item = get_item(sp_runtime_context, index);
  if (item == nullptr) return MYSQL_FAILURE;
  *is_null = item->is_null();
  if (*is_null) return MYSQL_SUCCESS;
  auto temp = String{};
  auto string = item->val_str(&temp);
  // HCS-8941: fix the bug when service called for non-string types: in case
  // this string owns the buffer, the buffer will be freed when this function
  // exits
  if (string->is_alloced()) {
    *value = nullptr;
    return MYSQL_FAILURE;
  }
  *value = string->c_ptr();
  *length = string->length();
  return MYSQL_SUCCESS;
}
/**
  Set value of a string argument

  @param [in] sp_runtime_context  stored program runtime context.
                                  If null, current runtime context will be
                                  used.
  @param [in] index               Argument location
  @param [in] string              Value of the argument
  @param [in] length              Length of the string

  @returns Status of operation
  @retval MYSQL_SUCCESS Success
  @retval MYSQL_FAILURE Failure
*/

DEFINE_BOOL_METHOD(mysql_stored_program_runtime_argument_string_imp::set,
                   (stored_program_runtime_context sp_runtime_context,
                    uint16_t index, char const *string, size_t length)) {
  auto item = new Item_string(string, length, &my_charset_bin);
  return set_variable(sp_runtime_context, item, index);
}

/**
  Get value of an int argument

  @param [in]  sp_runtime_context  stored program runtime context.
                                   If null, current runtime context will be
                                   used.
  @param [in]  index               Argument location
  @param [out] result              Value of the argument
  @param [out] is_null             Flag to indicate if value is null

  @returns Status of operation
  @retval MYSQL_SUCCESS Success
  @retval MYSQL_FAILURE Failure
*/

DEFINE_BOOL_METHOD(mysql_stored_program_runtime_argument_int_imp::get,
                   (stored_program_runtime_context sp_runtime_context,
                    uint16_t index, int64_t *result, bool *is_null)) {
  auto item = get_item(sp_runtime_context, index);
  if (item == nullptr) return MYSQL_FAILURE;
  *is_null = item->is_null();
  if (!*is_null) *result = item->val_int();
  return MYSQL_SUCCESS;
}

/**
  Set value of an int argument

  @param [in] sp_runtime_context  stored program runtime context.
                                  If null, current runtime context will be
                                  used.
  @param [in] index               Argument location
  @param [in] value               Value to be set

  @returns Status of operation
  @retval MYSQL_SUCCESS Success
  @retval MYSQL_FAILURE Failure
*/

DEFINE_BOOL_METHOD(mysql_stored_program_runtime_argument_int_imp::set,
                   (stored_program_runtime_context sp_runtime_context,
                    uint16_t index, int64_t value)) {
  Item *item = new Item_int(static_cast<long long>(value));
  return set_variable(sp_runtime_context, item, index);
}

/**
  Get value of an unsigned int argument

  @param [in]  sp_runtime_context  stored program runtime context.
                                   If null, current runtime context will be
                                   used.
  @param [in]  index               Argument location
  @param [out] result              Value of the argument
  @param [out] is_null             Flag to indicate if value is null

  @returns Status of operation
  @retval MYSQL_SUCCESS Success
  @retval MYSQL_FAILURE Failure
*/

DEFINE_BOOL_METHOD(mysql_stored_program_runtime_argument_unsigned_int_imp::get,
                   (stored_program_runtime_context sp_runtime_context,
                    uint16_t index, uint64_t *result, bool *is_null)) {
  auto item = get_item(sp_runtime_context, index);
  if (item == nullptr) return MYSQL_FAILURE;
  *is_null = item->is_null();
  if (!*is_null) *result = item->val_uint();
  return MYSQL_SUCCESS;
}

/**
  Set value of an unsigned int argument

  @param [in] sp_runtime_context  stored program runtime context.
                                  If null, current runtime context will be
                                  used.
  @param [in] index               Argument location
  @param [in] value               Value to be set

  @returns Status of operation
  @retval MYSQL_SUCCESS Success
  @retval MYSQL_FAILURE Failure
*/

DEFINE_BOOL_METHOD(mysql_stored_program_runtime_argument_unsigned_int_imp::set,
                   (stored_program_runtime_context sp_runtime_context,
                    uint16_t index, uint64_t value)) {
  Item *item = new Item_int(static_cast<unsigned long long>(value));
  return set_variable(sp_runtime_context, item, index);
}

/**
  Get a float time value

  @param [in]  sp_runtime_context  stored program runtime context.
                                   If null, current runtime context will be
                                   used.
  @param [in]  index               Argument location
  @param [out] result              Value of the argument
  @param [out] is_null             Flag to indicate if value is null

  @returns Status of operation
  @retval MYSQL_SUCCESS Success
  @retval MYSQL_FAILURE Failure
*/

DEFINE_BOOL_METHOD(mysql_stored_program_runtime_argument_float_imp::get,
                   (stored_program_runtime_context sp_runtime_context,
                    uint16_t index, double *result, bool *is_null)) {
  auto item = get_item(sp_runtime_context, index);
  if (item == nullptr) return MYSQL_FAILURE;
  *is_null = item->is_null();
  if (!*is_null) *result = item->val_real();
  return MYSQL_SUCCESS;
}

/**
  Set value of a float argument

  @param [in] sp_runtime_context  stored program runtime context.
                                  If null, current runtime context will be
                                  used.
  @param [in] index               Argument location
  @param [in] value               value to be set

  @returns Status of operation
  @retval MYSQL_SUCCESS Success
  @retval MYSQL_FAILURE Failure
*/

DEFINE_BOOL_METHOD(mysql_stored_program_runtime_argument_float_imp::set,
                   (stored_program_runtime_context sp_runtime_context,
                    uint16_t index, double value)) {
  Item *item = new Item_float(value, DECIMAL_NOT_SPECIFIED);
  return set_variable(sp_runtime_context, item, index);
}

/**
  @param [in]  sp_runtime_context  stored program runtime context.
                                   If null, current runtime context will be
                                   used.
  @param [in] year                 Year

  @returns Status of operation
  @retval MYSQL_SUCCESS Success
  @retval MYSQL_FAILURE Failure
*/

DEFINE_BOOL_METHOD(mysql_stored_program_return_value_year_imp::set,
                   (stored_program_runtime_context sp_runtime_context,
                    uint32_t year)) {
  Item *item = new Item_int(static_cast<long long>(year));
  return set_return_value(sp_runtime_context, item);
}

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
  @retval MYSQL_SUCCESS Success
  @retval MYSQL_FAILURE Failure
*/

DEFINE_BOOL_METHOD(mysql_stored_program_return_value_time_imp::set,
                   (stored_program_runtime_context sp_runtime_context,
                    uint32_t hour, uint32_t minute, uint32_t second,
                    uint64_t micro, bool negative, uint8_t decimals)) {
  auto time = MYSQL_TIME{static_cast<unsigned int>(0),
                         static_cast<unsigned int>(0),
                         static_cast<unsigned int>(0),
                         static_cast<unsigned int>(hour),
                         static_cast<unsigned int>(minute),
                         static_cast<unsigned int>(second),
                         static_cast<unsigned long>(micro),
                         negative,
                         MYSQL_TIMESTAMP_TIME,
                         {}};
  if (check_datetime_range(time)) return MYSQL_FAILURE;
  auto item = new Item_time_literal(&time, decimals);
  return set_return_value(sp_runtime_context, item);
}

/**
  @param [in] sp_runtime_context  stored program runtime context.
                                  If null, current runtime context will be
                                  used.
  @param [in] year                Year information
  @param [in] month               Month of the year
  @param [in] day                 Day of the month

  @returns Status of operation
  @retval MYSQL_SUCCESS Success
  @retval MYSQL_FAILURE Failure
*/

DEFINE_BOOL_METHOD(mysql_stored_program_return_value_date_imp::set,
                   (stored_program_runtime_context sp_runtime_context,
                    uint32_t year, uint32_t month, uint32_t day)) {
  auto time = MYSQL_TIME{
      year, month, day, {}, {}, {}, {}, {}, MYSQL_TIMESTAMP_DATE, {}};
  if (check_datetime_range(time)) return MYSQL_FAILURE;
  auto item = new Item_date_literal(&time);
  return set_return_value(sp_runtime_context, item);
}

/**
  Helper function that sets return value for DATETIME
  and TIMESTAMP types.

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
  @retval MYSQL_SUCCESS Success
  @retval MYSQL_FAILURE Failure
*/

static int return_value_datetime_set(
    stored_program_runtime_context sp_runtime_context, uint32_t year,
    uint32_t month, uint32_t day, uint32_t hour, uint32_t minute,
    uint32_t second, uint64_t micro, bool negative, uint32_t decimals,
    int32_t time_zone_offset, bool time_zone_aware) {
  enum_mysql_timestamp_type ts_type{};
  if (time_zone_aware) {
    ts_type = enum_mysql_timestamp_type::MYSQL_TIMESTAMP_DATETIME_TZ;
  } else {
    ts_type = enum_mysql_timestamp_type::MYSQL_TIMESTAMP_DATETIME;
  }
  auto time = MYSQL_TIME{year,
                         month,
                         day,
                         hour,
                         minute,
                         second,
                         static_cast<unsigned long>(micro),
                         negative,
                         ts_type,
                         time_zone_offset};
  if (check_datetime_range(time)) return MYSQL_FAILURE;
  auto *item =
      new Item_datetime_literal(&time, decimals, current_thd->time_zone());
  return set_return_value(sp_runtime_context, item);
}

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
  @retval MYSQL_SUCCESS Success
  @retval MYSQL_FAILURE Failure
*/

DEFINE_BOOL_METHOD(mysql_stored_program_return_value_datetime_imp::set,
                   (stored_program_runtime_context sp_runtime_context,
                    uint32_t year, uint32_t month, uint32_t day, uint32_t hour,
                    uint32_t minute, uint32_t second, uint64_t micro,
                    bool negative, uint32_t decimals, int32_t time_zone_offset,
                    bool time_zone_aware)) {
  return return_value_datetime_set(sp_runtime_context, year, month, day, hour,
                                   minute, second, micro, negative, decimals,
                                   time_zone_offset, time_zone_aware);
}

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
  @retval MYSQL_SUCCESS Success
  @retval MYSQL_FAILURE Failure
*/

DEFINE_BOOL_METHOD(mysql_stored_program_return_value_timestamp_imp::set,
                   (stored_program_runtime_context sp_runtime_context,
                    uint32_t year, uint32_t month, uint32_t day, uint32_t hour,
                    uint32_t minute, uint32_t second, uint64_t micro,
                    bool negative, uint32_t decimals, int32_t time_zone_offset,
                    bool time_zone_aware)) {
  return return_value_datetime_set(sp_runtime_context, year, month, day, hour,
                                   minute, second, micro, negative, decimals,
                                   time_zone_offset, time_zone_aware);
}

/**
  Set null value

  @param [in] sp_runtime_context  stored program runtime context.
                                  If null, current runtime context will be
                                  used.

  @returns Status of operation
  @retval MYSQL_SUCCESS Success
  @retval MYSQL_FAILURE Failure
*/

DEFINE_BOOL_METHOD(mysql_stored_program_return_value_null_imp::set,
                   (stored_program_runtime_context sp_runtime_context)) {
  auto item = new Item_null{};
  return set_return_value(sp_runtime_context, item);
}

/**
  Set value of a string return value

  @param [in] sp_runtime_context  stored program runtime context.
                                  If null, current runtime context will be
                                  used.
  @param [in] string              Value of the argument
  @param [in] length              Length of the string

  @returns Status of operation
  @retval MYSQL_SUCCESS Success
  @retval MYSQL_FAILURE Failure
*/

DEFINE_BOOL_METHOD(mysql_stored_program_return_value_string_imp::set,
                   (stored_program_runtime_context sp_runtime_context,
                    char const *string, size_t length)) {
  auto item = new Item_string(string, length, &my_charset_bin);
  return set_return_value(sp_runtime_context, item);
}

/**
  Set value of an int return value

  @param [in] sp_runtime_context  stored program runtime context.
                                  If null, current runtime context will be
                                  used.
  @param [in] value               Value to be set

  @returns Status of operation
  @retval MYSQL_SUCCESS Success
  @retval MYSQL_FAILURE Failure
*/

DEFINE_BOOL_METHOD(mysql_stored_program_return_value_int_imp::set,
                   (stored_program_runtime_context sp_runtime_context,
                    int64_t value)) {
  Item *item = new Item_int(static_cast<long long>(value));
  return set_return_value(sp_runtime_context, item);
}

/**
  Set value of an unsigned int return value

  @param [in] sp_runtime_context  stored program runtime context.
                                  If null, current runtime context will be
                                  used.
  @param [in] value               Value to be set

  @returns Status of operation
  @retval MYSQL_SUCCESS Success
  @retval MYSQL_FAILURE Failure
*/

DEFINE_BOOL_METHOD(mysql_stored_program_return_value_unsigned_int_imp::set,
                   (stored_program_runtime_context sp_runtime_context,
                    uint64_t value)) {
  Item *item = new Item_int(static_cast<unsigned long long>(value));
  return set_return_value(sp_runtime_context, item);
}

/**
  Set value of a float return value

  @param [in] sp_runtime_context  stored program runtime context.
                                  If null, current runtime context will be
                                  used.
  @param [in] value               value to be set

  @returns Status of operation
  @retval MYSQL_SUCCESS Success
  @retval MYSQL_FAILURE Failure
*/

DEFINE_BOOL_METHOD(mysql_stored_program_return_value_float_imp::set,
                   (stored_program_runtime_context sp_runtime_context,
                    double value)) {
  Item *item = new Item_float(value, DECIMAL_NOT_SPECIFIED);
  return set_return_value(sp_runtime_context, item);
}

/**
 * @brief Ensure the sp_head is part of the current THD.
 *
 * @param sp - sp_head pointer.
 * @return true if the sp_head is part of the current THD.
 * @return false if not.
 */
static auto is_sp_in_current_thd(sp_head *sp) -> bool {
  assert(sp);
  if (!sp) return false;

  if (sp_cache_has(current_thd->sp_func_cache, sp)) return true;
  if (sp_cache_has(current_thd->sp_proc_cache, sp)) return true;

  assert(false);
  return false;
}

DEFINE_BOOL_METHOD(mysql_stored_program_external_program_handle_imp::get,
                   (stored_program_handle sp_handle,
                    external_program_handle *value)) {
  assert(value);
  if (!value) return MYSQL_FAILURE;

  auto sp = reinterpret_cast<sp_head *>(sp_handle);
  if (!is_sp_in_current_thd(sp)) return MYSQL_FAILURE;
  *value = sp->get_external_program_handle();
  return MYSQL_SUCCESS;
}

DEFINE_BOOL_METHOD(mysql_stored_program_external_program_handle_imp::set,
                   (stored_program_handle sp_handle,
                    external_program_handle value)) {
  auto sp = reinterpret_cast<sp_head *>(sp_handle);
  if (!is_sp_in_current_thd(sp)) return MYSQL_FAILURE;
  return sp->set_external_program_handle(value);
}

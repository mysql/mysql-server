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

#ifndef MYSQL_STORED_PROGRAM_IMP_H
#define MYSQL_STORED_PROGRAM_IMP_H

#include <cstdint>
#include "mysql/components/services/mysql_string.h"

#include <mysql/components/service_implementation.h>
#include <mysql/components/services/mysql_stored_program.h>

/**
  Implementation of the mysql_stored_program services
*/
class mysql_stored_program_metadata_query_imp {
 public:
  static DEFINE_BOOL_METHOD(get, (stored_program_handle sp_handle,
                                  const char *key, void *value));
};

/*
 * Argument-related services:
 */

class mysql_stored_program_argument_metadata_query_imp {
 public:
  static DEFINE_BOOL_METHOD(get,
                            (stored_program_handle sp_handle, uint16_t index,
                             const char *key, void *value));
};

class mysql_stored_program_return_metadata_query_imp {
 public:
  static DEFINE_BOOL_METHOD(get, (stored_program_handle sp_handle,
                                  const char *key, void *value));
};

class mysql_stored_program_field_name_imp {
 public:
  static DEFINE_BOOL_METHOD(get_name,
                            (stored_program_runtime_context sp_runtime_context,
                             char const **value));
};

class mysql_stored_program_runtime_argument_year_imp {
 public:
  static DEFINE_BOOL_METHOD(get,
                            (stored_program_runtime_context sp_runtime_context,
                             uint16_t index, uint32_t *year, bool *is_null));
  static DEFINE_BOOL_METHOD(set,
                            (stored_program_runtime_context sp_runtime_context,
                             uint16_t index, uint32_t year));
};

class mysql_stored_program_runtime_argument_time_imp {
 public:
  static DEFINE_BOOL_METHOD(get,
                            (stored_program_runtime_context sp_runtime_context,
                             uint16_t index, uint32_t *hour, uint32_t *minute,
                             uint32_t *second, uint64_t *micro, bool *negative,
                             bool *is_null));
  static DEFINE_BOOL_METHOD(set,
                            (stored_program_runtime_context sp_runtime_context,
                             uint16_t index, uint32_t hour, uint32_t minute,
                             uint32_t second, uint64_t micro, bool negative,
                             uint8_t decimals));
};

class mysql_stored_program_runtime_argument_date_imp {
 public:
  static DEFINE_BOOL_METHOD(get,
                            (stored_program_runtime_context sp_runtime_context,
                             uint16_t index, uint32_t *year, uint32_t *month,
                             uint32_t *day, bool *is_null));
  static DEFINE_BOOL_METHOD(set,
                            (stored_program_runtime_context sp_runtime_context,
                             uint16_t index, uint32_t year, uint32_t month,
                             uint32_t day));
};

class mysql_stored_program_runtime_argument_datetime_imp {
 public:
  static DEFINE_BOOL_METHOD(get,
                            (stored_program_runtime_context sp_runtime_context,
                             uint16_t index, uint32_t *year, uint32_t *month,
                             uint32_t *day, uint32_t *hour, uint32_t *minute,
                             uint32_t *second, uint64_t *micro, bool *negative,
                             int32_t *time_zone_offset, bool *is_null));
  static DEFINE_BOOL_METHOD(
      set, (stored_program_runtime_context sp_runtime_context, uint16_t index,
            uint32_t year, uint32_t month, uint32_t day, uint32_t hour,
            uint32_t minute, uint32_t second, uint64_t micro, bool negative,
            uint32_t decimals, int32_t time_zone_offset, bool time_zone_aware));
};

class mysql_stored_program_runtime_argument_timestamp_imp {
 public:
  static DEFINE_BOOL_METHOD(get,
                            (stored_program_runtime_context sp_runtime_context,
                             uint16_t index, uint32_t *year, uint32_t *month,
                             uint32_t *day, uint32_t *hour, uint32_t *minute,
                             uint32_t *second, uint64_t *micro, bool *negative,
                             int32_t *time_zone_offset, bool *is_null));
  static DEFINE_BOOL_METHOD(
      set, (stored_program_runtime_context sp_runtime_context, uint16_t index,
            uint32_t year, uint32_t month, uint32_t day, uint32_t hour,
            uint32_t minute, uint32_t second, uint64_t micro, bool negative,
            uint32_t decimals, int32_t time_zone_offset, bool time_zone_aware));
};

class mysql_stored_program_runtime_argument_null_imp {
 public:
  static DEFINE_BOOL_METHOD(set,
                            (stored_program_runtime_context sp_runtime_context,
                             uint16_t index));
};

class mysql_stored_program_runtime_argument_string_imp {
 public:
  static DEFINE_BOOL_METHOD(get,
                            (stored_program_runtime_context sp_runtime_context,
                             uint16_t index, char const **buffer,
                             size_t *out_len, bool *is_null));
  static DEFINE_BOOL_METHOD(set,
                            (stored_program_runtime_context sp_runtime_context,
                             uint16_t index, char const *string,
                             size_t length));
};

class mysql_stored_program_runtime_argument_string_charset_imp {
 public:
  static DEFINE_BOOL_METHOD(set,
                            (stored_program_runtime_context sp_runtime_context,
                             uint16_t index, char const *string, size_t length,
                             CHARSET_INFO_h charset));
};

class mysql_stored_program_runtime_argument_int_imp {
 public:
  static DEFINE_BOOL_METHOD(get,
                            (stored_program_runtime_context sp_runtime_context,
                             uint16_t index, int64_t *result, bool *is_null));
  static DEFINE_BOOL_METHOD(set,
                            (stored_program_runtime_context sp_runtime_context,
                             uint16_t index, int64_t value));
};

class mysql_stored_program_runtime_argument_unsigned_int_imp {
 public:
  static DEFINE_BOOL_METHOD(get,
                            (stored_program_runtime_context sp_runtime_context,
                             uint16_t index, uint64_t *result, bool *is_null));
  static DEFINE_BOOL_METHOD(set,
                            (stored_program_runtime_context sp_runtime_context,
                             uint16_t index, uint64_t value));
};

class mysql_stored_program_runtime_argument_float_imp {
 public:
  static DEFINE_BOOL_METHOD(get,
                            (stored_program_runtime_context sp_runtime_context,
                             uint16_t index, double *result, bool *is_null));
  static DEFINE_BOOL_METHOD(set,
                            (stored_program_runtime_context sp_runtime_context,
                             uint16_t index, double value));
};

class mysql_stored_program_return_value_year_imp {
 public:
  static DEFINE_BOOL_METHOD(set,
                            (stored_program_runtime_context sp_runtime_context,
                             uint32_t year));
};

class mysql_stored_program_return_value_time_imp {
 public:
  static DEFINE_BOOL_METHOD(set,
                            (stored_program_runtime_context sp_runtime_context,
                             uint32_t hour, uint32_t minute, uint32_t second,
                             uint64_t micro, bool negative, uint8_t decimals));
};

class mysql_stored_program_return_value_date_imp {
 public:
  static DEFINE_BOOL_METHOD(set,
                            (stored_program_runtime_context sp_runtime_context,
                             uint32_t year, uint32_t month, uint32_t day));
};

class mysql_stored_program_return_value_datetime_imp {
 public:
  static DEFINE_BOOL_METHOD(set,
                            (stored_program_runtime_context sp_runtime_context,
                             uint32_t year, uint32_t month, uint32_t day,
                             uint32_t hour, uint32_t minute, uint32_t second,
                             uint64_t micro, bool negative, uint32_t decimals,
                             int32_t time_zone_offset, bool time_zone_aware));
};

class mysql_stored_program_return_value_timestamp_imp {
 public:
  static DEFINE_BOOL_METHOD(set,
                            (stored_program_runtime_context sp_runtime_context,
                             uint32_t year, uint32_t month, uint32_t day,
                             uint32_t hour, uint32_t minute, uint32_t second,
                             uint64_t micro, bool negative, uint32_t decimals,
                             int32_t time_zone_offset, bool time_zone_aware));
};

class mysql_stored_program_return_value_null_imp {
 public:
  static DEFINE_BOOL_METHOD(
      set, (stored_program_runtime_context sp_runtime_context));
};

class mysql_stored_program_return_value_string_imp {
 public:
  static DEFINE_BOOL_METHOD(set,
                            (stored_program_runtime_context sp_runtime_context,
                             char const *string, size_t length));
};

class mysql_stored_program_return_value_string_charset_imp {
 public:
  static DEFINE_BOOL_METHOD(set,
                            (stored_program_runtime_context sp_runtime_context,
                             char const *string, size_t length,
                             CHARSET_INFO_h charset));
};

class mysql_stored_program_return_value_int_imp {
 public:
  static DEFINE_BOOL_METHOD(set,
                            (stored_program_runtime_context sp_runtime_context,
                             int64_t value));
};

class mysql_stored_program_return_value_unsigned_int_imp {
 public:
  static DEFINE_BOOL_METHOD(set,
                            (stored_program_runtime_context sp_runtime_context,
                             uint64_t value));
};

class mysql_stored_program_return_value_float_imp {
 public:
  static DEFINE_BOOL_METHOD(set,
                            (stored_program_runtime_context sp_runtime_context,
                             double value));
};

class mysql_stored_program_external_program_handle_imp {
 public:
  static DEFINE_BOOL_METHOD(get, (stored_program_handle sp,
                                  external_program_handle *value));

  static DEFINE_BOOL_METHOD(set, (stored_program_handle sp,
                                  external_program_handle value));
};
#endif /* MYSQL_STORED_PROGRAM_IMP_H */

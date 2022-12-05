/*  Copyright (c) 2022, Oracle and/or its affiliates.

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
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*/

#include "mysql/components/services/bulk_data_service.h"
#include <assert.h>
#include <cstdint>
#include "field_types.h"
#include "my_byteorder.h"
#include "my_time.h"
#include "mysql/components/service_implementation.h"
#include "mysql/components/services/log_builtins.h"
#include "mysql_time.h"
#include "mysqld_error.h"
#include "sql/field.h"
#include "sql/my_decimal.h"
#include "sql/sql_class.h"
#include "sql/sql_gipk.h"
#include "sql/sql_time.h"
#include "sql/tztime.h"

namespace Bulk_data_convert {

/** Log details of error during data conversion.
@param[in]  text_col  input column from CSV file
@param[in]  mesg      error message to append to */
static void log_conversion_error(const Column_text &text_col,
                                 std::string mesg) {
  std::ostringstream err_strm;
  std::string in_value(text_col.m_data_ptr, text_col.m_data_len);
  err_strm << "BULK LOAD Conversion: " << mesg << in_value;
  LogErr(INFORMATION_LEVEL, ER_BULK_LOADER_INFO, err_strm.str().c_str());
}

/** Create an interger column converting data from CSV text.
@param[in]   text_col       input column in text read from CSV
@param[in]   charset        character set for the input column data
@param[in]   field          table column metadata
@param[out]  sql_col        converted column in MySQL storage format
@param[out]  error_details  the error details
@return error code. */
template <typename S, typename U>
static int format_int_column(const Column_text &text_col,
                             const CHARSET_INFO *charset, const Field *field,
                             Column_mysql &sql_col,
                             Bulk_load_error_location_details &error_details) {
  int err = 0;
  const char *end;

  auto field_num = (const Field_num *)field;
  bool is_unsigned = field_num->is_unsigned();

  auto val = charset->cset->strntoull10rnd(charset, text_col.m_data_ptr,
                                           text_col.m_data_len, is_unsigned,
                                           &end, &err);
  if (err != 0) {
    error_details.column_type = "integer";
    log_conversion_error(text_col, "Integer conversion failed for: ");
    return ER_LOAD_BULK_DATA_WRONG_VALUE_FOR_FIELD;
  }

  if (is_unsigned && val > std::numeric_limits<U>::max()) {
    error_details.column_type = "integer";
    log_conversion_error(text_col, "Unsigned Integer out of range: ");
    return ER_LOAD_BULK_DATA_WRONG_VALUE_FOR_FIELD;
  }

  if (!is_unsigned) {
    auto signed_val = static_cast<int64_t>(val);

    if (signed_val < std::numeric_limits<S>::min() ||
        signed_val > std::numeric_limits<S>::max()) {
      error_details.column_type = "integer";
      log_conversion_error(text_col, "Integer out of range: ");
      return ER_LOAD_BULK_DATA_WRONG_VALUE_FOR_FIELD;
    }
  }
  sql_col.m_int_data = val;
  return 0;
}

/** Create a char/varchar column converting data to MySQL stoage format.
@param[in]   text_col       input column in text read from CSV
@param[in]   charset        character set for the input column data
@param[in]   field          table column metadata
@param[in]   length_size    number of bytes needed for storing varchar data
length
@param[out]  sql_col        converted column in MySQL storage format
@param[out]  error_details  error location details
@return error code. */
static int format_char_column(const Column_text &text_col,
                              const CHARSET_INFO *charset, const Field *field,
                              size_t length_size, Column_mysql &sql_col,
                              Bulk_load_error_location_details &error_details) {
  auto field_str = (const Field_str *)field;

  const CHARSET_INFO *field_charset = field_str->charset();
  auto field_size = field->field_length;
  auto field_char_size = field_size / field_charset->mbmaxlen;

  assert(sql_col.m_data_len >= (length_size + field_size));

  char *field_begin = sql_col.m_data_ptr;
  char *field_data = field_begin + length_size;

  const char *error_pos = nullptr;
  const char *convert_error_pos = nullptr;
  const char *end_pos = nullptr;

  size_t copy_size = well_formed_copy_nchars(
      field_charset, field_data, field_size, charset, text_col.m_data_ptr,
      text_col.m_data_len, field_char_size, &error_pos, &convert_error_pos,
      &end_pos);

  if (error_pos != nullptr || convert_error_pos != nullptr) {
    error_details.column_type = "string";
    log_conversion_error(text_col, "Invalid Input String: ");
    return ER_LOAD_BULK_DATA_WRONG_VALUE_FOR_FIELD;
  }

  if (end_pos < text_col.m_data_ptr + text_col.m_data_len) {
    error_details.column_type = "string";
    log_conversion_error(text_col, "Input String too long: ");
    return ER_LOAD_BULK_DATA_WRONG_VALUE_FOR_FIELD;
  }

  /* For char[] column need to fill padding characters. */
  if (length_size == 0 && copy_size < field_size) {
    size_t fill_size = field_size - copy_size;
    char *fill_pos = field_data + copy_size;

    field_charset->cset->fill(field_charset, fill_pos, fill_size,
                              field_charset->pad_char);
  }

  if (length_size == 0) {
    return 0;
  }

  /* Write length for varchar column. */
  if (length_size == 1) {
    *field_begin = static_cast<unsigned char>(copy_size);
    return 0;
  }

  assert(length_size == 2);
  int2store(field_begin, static_cast<uint16_t>(copy_size));

  return 0;
}

/** Create a FLOAT column converting data to MySQL stoage format.
@param[in]   text_col       input column in text read from CSV
@param[in]   charset        character set for the input column data
@param[in]   field          table column metadata
@param[out]  sql_col        converted column in MySQL storage format
@param[out]  error_details  the error details
@return error code. */
static int format_float_column(
    const Column_text &text_col, const CHARSET_INFO *charset,
    const Field *field, Column_mysql &sql_col,
    Bulk_load_error_location_details &error_details) {
  int conv_error;
  const char *end;
  double nr = my_strntod(charset, text_col.m_data_ptr, text_col.m_data_len,
                         &end, &conv_error);
  const auto converted_len = (size_t)(end - text_col.m_data_ptr);
  if (conv_error != 0 || end == text_col.m_data_ptr ||
      converted_len != text_col.m_data_len) {
    error_details.column_type = "float";
    log_conversion_error(text_col, "Invalid Float Data: ");
    return ER_LOAD_BULK_DATA_WRONG_VALUE_FOR_FIELD;
  }

  auto field_float = (const Field_float *)field;

  if (field_float->is_unsigned() && nr < 0) {
    error_details.column_type = "float";
    log_conversion_error(text_col, "Signed Float for unsigned type: ");
    return ER_LOAD_BULK_DATA_WRONG_VALUE_FOR_FIELD;
  }

  if (field_float->truncate(&nr, FLT_MAX) != Field_real::TR_OK) {
    error_details.column_type = "float";
    log_conversion_error(text_col, "Invalid value for type: ");
    return ER_LOAD_BULK_DATA_WRONG_VALUE_FOR_FIELD;
  }

  floatstore((uchar *)sql_col.m_data_ptr, nr);

  return 0;
}

/** Create a DOUBLE column converting data to MySQL stoage format.
@param[in]   text_col       input column in text read from CSV
@param[in]   charset        character set for the input column data
@param[in]   field          table column metadata
@param[out]  sql_col        converted column in MySQL storage format
@param[out]  error_details  the error details
@return error code. */
static int format_double_column(
    const Column_text &text_col, const CHARSET_INFO *charset,
    const Field *field, Column_mysql &sql_col,
    Bulk_load_error_location_details &error_details) {
  int conv_error;
  const char *end;
  double nr = my_strntod(charset, text_col.m_data_ptr, text_col.m_data_len,
                         &end, &conv_error);
  const auto converted_len = (size_t)(end - text_col.m_data_ptr);
  if (conv_error != 0 || end == text_col.m_data_ptr ||
      converted_len != text_col.m_data_len) {
    error_details.column_type = "double";
    log_conversion_error(text_col, "Invalid Float Data: ");
    return ER_LOAD_BULK_DATA_WRONG_VALUE_FOR_FIELD;
  }

  auto field_double = (const Field_double *)field;
  if (field_double->is_unsigned() && nr < 0) {
    error_details.column_type = "double";
    log_conversion_error(text_col, "Signed Double for unsigned type: ");
    return ER_LOAD_BULK_DATA_WRONG_VALUE_FOR_FIELD;
  }

  if (field_double->truncate(&nr, FLT_MAX) != Field_real::TR_OK) {
    error_details.column_type = "double";
    log_conversion_error(text_col, "Invalid value for type: ");
    return ER_LOAD_BULK_DATA_WRONG_VALUE_FOR_FIELD;
  }

  doublestore((uchar *)sql_col.m_data_ptr, nr);

  return 0;
}

/** Create a DECIMAL column converting data to MySQL stoage format.
@param[in]   text_col       input column in text read from CSV
@param[in]   charset        character set for the input column data
@param[in]   field          table column metadata
@param[out]  sql_col        converted column in MySQL storage format
@param[out]  error_details  the error details
@return error code. */
static int format_decimal_column(
    const Column_text &text_col, const CHARSET_INFO *charset,
    const Field *field, Column_mysql &sql_col,
    Bulk_load_error_location_details &error_details) {
  auto field_dec = (const Field_new_decimal *)field;
  my_decimal decimal_value;

  int err = str2my_decimal(
      E_DEC_FATAL_ERROR & ~(E_DEC_OVERFLOW | E_DEC_BAD_NUM),
      text_col.m_data_ptr, text_col.m_data_len, charset, &decimal_value);

  if (err == E_DEC_OK) {
    auto precision = field_dec->precision;
    auto scale = field_dec->dec;
    assert(sql_col.m_data_len >= (size_t)decimal_bin_size(precision, scale));

    auto field_begin = (unsigned char *)sql_col.m_data_ptr;
    err = my_decimal2binary(E_DEC_FATAL_ERROR & ~E_DEC_OVERFLOW, &decimal_value,
                            field_begin, precision, scale);
  }

  if (err != E_DEC_OK) {
    error_details.column_type = "decimal";
    log_conversion_error(text_col, "Invalid Decimal Data: ");
    return ER_LOAD_BULK_DATA_WRONG_VALUE_FOR_FIELD;
  }

  if (field_dec->is_unsigned() && decimal_value.sign()) {
    error_details.column_type = "decimal";
    log_conversion_error(text_col, "Signed Decimal for unsigned type: ");
    return ER_LOAD_BULK_DATA_WRONG_VALUE_FOR_FIELD;
  }
  return 0;
}

/** Create a DATETIME column converting data to MySQL stoage format.
@param[in]   thd            session THD
@param[in]   text_col       input column in text read from CSV
@param[in]   charset        character set for the input column data
@param[in]   field          table column metadata
@param[out]  sql_col        converted column in MySQL storage format
@param[out]  error_details  the error details
@return error code. */
static int format_datetime_column(
    THD *thd, const Column_text &text_col, const CHARSET_INFO *charset,
    const Field *field, Column_mysql &sql_col,
    Bulk_load_error_location_details &error_details) {
  auto field_date = (const Field_temporal *)field;
  auto flags = field_date->get_date_flags(thd);

  MYSQL_TIME ltime;
  MYSQL_TIME_STATUS status;
  /* Convert input to MySQL TIME. */
  bool res = str_to_datetime(charset, text_col.m_data_ptr, text_col.m_data_len,
                             &ltime, flags, &status);

  /* Adjust value to the column precision. */
  if (!res && status.warnings == 0) {
    res = my_datetime_adjust_frac(&ltime, field_date->get_fractional_digits(),
                                  &status.warnings, flags & TIME_FRAC_TRUNCATE);
  }

  /* Check for error in conversion. */
  if (res || (status.warnings != 0)) {
    error_details.column_type = "datetime";
    log_conversion_error(text_col, "Invalid DATETIME: ");
    return ER_LOAD_BULK_DATA_WRONG_VALUE_FOR_FIELD;
  }

  MYSQL_TIME *time = &ltime;
  MYSQL_TIME tz_ltime;

  if (ltime.time_type == MYSQL_TIMESTAMP_DATETIME_TZ) {
    tz_ltime = ltime;
    time = &tz_ltime;

    const Time_zone *tz = thd->time_zone();

    if (convert_time_zone_displacement(tz, &tz_ltime)) {
      error_details.column_type = "datetime";
      log_conversion_error(text_col, "TZ displacement failed: ");
      return ER_LOAD_BULK_DATA_WRONG_VALUE_FOR_FIELD;
    }

    /* Check for boundary conditions by converting to a timeval */
    my_timeval tm_not_used;
    res = datetime_with_no_zero_in_date_to_timeval(&tz_ltime, *tz, &tm_not_used,
                                                   &status.warnings);
    if (res || status.warnings != 0) {
      error_details.column_type = "datetime";
      log_conversion_error(text_col, "TZ boundary check failed: ");
      return ER_LOAD_BULK_DATA_WRONG_VALUE_FOR_FIELD;
    }
  }

  auto packed = TIME_to_longlong_datetime_packed(*time);
  auto field_begin = (unsigned char *)sql_col.m_data_ptr;

  my_datetime_packed_to_binary(packed, field_begin,
                               field_date->get_fractional_digits());

  return 0;
}

/** Create a DATE column converting data to MySQL stoage format.
@param[in]   thd            session THD
@param[in]   text_col       input column in text read from CSV
@param[in]   charset        character set for the input column data
@param[in]   field          table column metadata
@param[out]  sql_col        converted column in MySQL storage format
@param[out]  error_details  error location details
@return error code. */
static int format_date_column(THD *thd, const Column_text &text_col,
                              const CHARSET_INFO *charset, const Field *field,
                              Column_mysql &sql_col,
                              Bulk_load_error_location_details &error_details) {
  auto field_date = (const Field_temporal *)field;
  auto flags = field_date->get_date_flags(thd);

  MYSQL_TIME ltime;
  MYSQL_TIME_STATUS status;

  /* Convert input to MySQL TIME. */
  bool res = str_to_datetime(charset, text_col.m_data_ptr, text_col.m_data_len,
                             &ltime, flags, &status);

  /* Adjust value to the column precision. */
  if (!res && status.warnings == 0) {
    res = my_datetime_adjust_frac(&ltime, field_date->get_fractional_digits(),
                                  &status.warnings, flags & TIME_FRAC_TRUNCATE);
  }

  /* Check for error in conversion. */
  if (res || (status.warnings != 0)) {
    error_details.column_type = "date";
    log_conversion_error(text_col, "Invalid DATE: ");
    return ER_LOAD_BULK_DATA_WRONG_VALUE_FOR_FIELD;
  }

  MYSQL_TIME *time = &ltime;
  MYSQL_TIME tz_ltime;

  if (ltime.time_type == MYSQL_TIMESTAMP_DATETIME_TZ) {
    tz_ltime = ltime;
    time = &tz_ltime;

    const Time_zone *tz = thd->time_zone();

    if (convert_time_zone_displacement(tz, &tz_ltime)) {
      error_details.column_type = "date";
      log_conversion_error(text_col, "TZ displacement failed: ");
      return ER_LOAD_BULK_DATA_WRONG_VALUE_FOR_FIELD;
    }

    /* Check for boundary conditions by converting to a timeval */
    my_timeval tm_not_used;
    res = datetime_with_no_zero_in_date_to_timeval(&tz_ltime, *tz, &tm_not_used,
                                                   &status.warnings);
    if (res || status.warnings != 0) {
      error_details.column_type = "date";
      log_conversion_error(text_col, "TZ boundary check failed: ");
      return ER_LOAD_BULK_DATA_WRONG_VALUE_FOR_FIELD;
    }
  }

  if (non_zero_time(*time)) {
    error_details.column_type = "date";
    log_conversion_error(text_col, "DATE includes TIME: ");
    return ER_LOAD_BULK_DATA_WRONG_VALUE_FOR_FIELD;
  }

  /* Convert to storage format. */
  auto field_begin = (unsigned char *)sql_col.m_data_ptr;
  my_date_to_binary(time, field_begin);

  return 0;
}

/** Create a TIME column converting data to MySQL stoage format.
@param[in]   thd            session THD
@param[in]   text_col       input column in text read from CSV
@param[in]   charset        character set for the input column data
@param[in]   field          table column metadata
@param[out]  sql_col        converted column in MySQL storage format
@param[out]  error_details  the error details
@return error code. */
static int format_time_column(THD *thd, const Column_text &text_col,
                              const CHARSET_INFO *charset, const Field *field,
                              Column_mysql &sql_col,
                              Bulk_load_error_location_details &error_details) {
  auto field_date = (const Field_temporal *)field;
  auto flags = field_date->get_date_flags(thd);

  MYSQL_TIME ltime;
  MYSQL_TIME_STATUS status;

  /* Convert input to MySQL TIME. */
  bool res = str_to_time(charset, text_col.m_data_ptr, text_col.m_data_len,
                         &ltime, flags, &status);

  /* Adjust value to the column precision. */
  if (!res && status.warnings == 0) {
    res = my_datetime_adjust_frac(&ltime, field_date->get_fractional_digits(),
                                  &status.warnings, flags & TIME_FRAC_TRUNCATE);
  }

  /* Check for error in conversion. */
  if (res || (status.warnings != 0)) {
    error_details.column_type = "time";
    log_conversion_error(text_col, "Invalid TIME: ");
    return ER_LOAD_BULK_DATA_WRONG_VALUE_FOR_FIELD;
  }

  MYSQL_TIME *time = &ltime;
  MYSQL_TIME tz_ltime;
  my_timeval tm;

  if (ltime.time_type == MYSQL_TIMESTAMP_DATETIME_TZ) {
    tz_ltime = ltime;
    time = &tz_ltime;

    const Time_zone *tz = thd->time_zone();

    if (convert_time_zone_displacement(tz, &tz_ltime)) {
      error_details.column_type = "time";
      log_conversion_error(text_col, "TZ displacement failed: ");
      return ER_LOAD_BULK_DATA_WRONG_VALUE_FOR_FIELD;
    }

    /* Check for boundary conditions by converting to a timeval */
    res = datetime_with_no_zero_in_date_to_timeval(&tz_ltime, *tz, &tm,
                                                   &status.warnings);
    if (res || status.warnings != 0) {
      error_details.column_type = "time";
      log_conversion_error(text_col, "TZ boundary check failed: ");
      return ER_LOAD_BULK_DATA_WRONG_VALUE_FOR_FIELD;
    }
  }

  if (non_zero_date(*time)) {
    error_details.column_type = "time";
    log_conversion_error(text_col, "TIME includes DATE: ");
    return ER_LOAD_BULK_DATA_WRONG_VALUE_FOR_FIELD;
  }

  auto packed = TIME_to_longlong_time_packed(*time);
  /* Convert to storage format. */
  auto field_begin = (unsigned char *)sql_col.m_data_ptr;
  my_time_packed_to_binary(packed, field_begin,
                           field_date->get_fractional_digits());

  return 0;
}

/** Create a row in converting column data to MySQL stoage format.
@param[in]      thd             session THD
@param[in]      table_share     shared table object
@param[in]      text_rows       input rows with columns in text read from CSV
@param[in]      text_row_index  current text row index
@param[in,out]  buffer          the input buffer
@param[in,out]  buffer_length   input buffer length
@param[in]      charset         character set for the input row data
@param[out]     sql_rows        converted rows in MySQL storage format
@param[in]      sql_row_index   current sql row index
@param[out]     completed       if all rows are processed
@param[out]     error_details   the error details
@return error code. */
static int format_row(THD *thd, const TABLE_SHARE *table_share,
                      const Rows_text &text_rows, size_t text_row_index,
                      char *&buffer, size_t &buffer_length,
                      const CHARSET_INFO *charset, Rows_mysql &sql_rows,
                      size_t sql_row_index, bool &completed,
                      Bulk_load_error_location_details &error_details) {
  /* Check if buffer is fully consumed. */
  if (buffer_length == 0) {
    completed = false;
    return 0;
  }
  completed = true;
  /* Loop through all the columns and convert input data. */
  int err = 0;

  auto text_row_offset = text_rows.get_row_offset(text_row_index);
  auto sql_row_offset = sql_rows.get_row_offset(sql_row_index);

  for (size_t index = 0; index < table_share->fields; ++index) {
    auto field = table_share->field[index];
    auto field_size = field->pack_length_in_rec();

    auto &text_col = text_rows.read_column(text_row_offset, index);
    auto &sql_col = sql_rows.get_column(sql_row_offset, index);

    /* No space left in buffer. */
    if (field_size > buffer_length) {
      completed = false;
      break;
    }

    sql_col.m_data_ptr = buffer;
    sql_col.m_data_len = field_size;
    sql_col.m_int_data = 0;
    sql_col.m_type = static_cast<int>(field->type());
    sql_col.m_is_null = text_col.m_data_ptr == nullptr;

    buffer += field_size;
    buffer_length -= field_size;

    if (sql_col.m_is_null) {
      if (!field->is_nullable()) {
        LogErr(INFORMATION_LEVEL, ER_BULK_LOADER_INFO,
               "NULL value found for NOT NULL field!");
        error_details.column_name = field->field_name;
        error_details.column_input_data =
            std::string(text_col.m_data_ptr, text_col.m_data_len);
        err = ER_LOAD_BULK_DATA_WARN_NULL_TO_NOTNULL;
        break;
      }
      continue;
    }

    /* TODO-1: Check for hidden and virtual columns.*/
    /* TODO-2: Check for column level constraints. */
    /* TODO-3: We could have better interfacing if we can get an interface
    for a field to get the data in stoage format. Currently we follow the
    ::store interface that writes the data to the row buffer stored in
    TABLE object. */
    switch (field->type()) {
      case MYSQL_TYPE_TINY:
        /* Column type TINYINT */
        err = format_int_column<int8_t, uint8_t>(text_col, charset, field,
                                                 sql_col, error_details);
        break;
      case MYSQL_TYPE_SHORT:
        /* Column type SMALLINT */
        err = format_int_column<int16_t, uint16_t>(text_col, charset, field,
                                                   sql_col, error_details);
        break;
      case MYSQL_TYPE_INT24:
        /* Column type MEDIUMINT */
        err = format_int_column<int32_t, uint32_t>(text_col, charset, field,
                                                   sql_col, error_details);
        break;
      case MYSQL_TYPE_LONG:
        /* Column type INT */
        err = format_int_column<int32_t, uint32_t>(text_col, charset, field,
                                                   sql_col, error_details);
        break;
      case MYSQL_TYPE_LONGLONG:
        /* Column type BIG */
        err = format_int_column<int64_t, uint64_t>(text_col, charset, field,
                                                   sql_col, error_details);
        break;
      case MYSQL_TYPE_STRING:
        /* Column type CHAR(n) */
        err = format_char_column(text_col, charset, field, 0, sql_col,
                                 error_details);
        break;
      case MYSQL_TYPE_VARCHAR: {
        /* Column type VARCHAR(n) */
        auto var_field = static_cast<Field_varstring *>(field);
        err = format_char_column(text_col, charset, field,
                                 var_field->get_length_bytes(), sql_col,
                                 error_details);
        break;
      }
      case MYSQL_TYPE_NEWDECIMAL:
        /* Column type DECIMAL(p,s) */
        err = format_decimal_column(text_col, charset, field, sql_col,
                                    error_details);
        break;
      case MYSQL_TYPE_FLOAT:
        err = format_float_column(text_col, charset, field, sql_col,
                                  error_details);
        break;
      case MYSQL_TYPE_DOUBLE:
        err = format_double_column(text_col, charset, field, sql_col,
                                   error_details);
        break;
      case MYSQL_TYPE_DATETIME:
        /* Column type DATETIME */
        err = format_datetime_column(thd, text_col, charset, field, sql_col,
                                     error_details);
        break;
      case MYSQL_TYPE_DATE:
        /* Column type DATE */
        err = format_date_column(thd, text_col, charset, field, sql_col,
                                 error_details);
        break;
      case MYSQL_TYPE_TIME:
        /* Column type TIME */
        err = format_time_column(thd, text_col, charset, field, sql_col,
                                 error_details);
        break;
      default: {
        std::ostringstream err_strm;
        String type_string(64);
        field->sql_type(type_string);
        err_strm << "BULK LOAD not supported for data type: "
                 << type_string.c_ptr_safe();
        LogErr(INFORMATION_LEVEL, ER_BULK_LOADER_INFO, err_strm.str().c_str());
        err = ER_NOT_SUPPORTED_YET;
        break;
      }
    }
    if (err != 0) {
      error_details.column_name = field->field_name;
      error_details.column_input_data =
          std::string(text_col.m_data_ptr, text_col.m_data_len);
      break;
    }
  }
  return err;
}

DEFINE_METHOD(int, mysql_format,
              (THD * thd, const TABLE *table, const Rows_text &text_rows,
               size_t &next_index, char *buffer, size_t buffer_length,
               const CHARSET_INFO *charset, Rows_mysql &sql_rows,
               Bulk_load_error_location_details &error_details)) {
  int err = 0;
  auto share = table->s;

  size_t max_index = text_rows.get_num_rows() - 1;

  assert(next_index <= max_index);

  if (next_index > max_index) {
    return ER_INTERNAL_ERROR;
  }

  size_t num_rows = max_index - next_index + 1;
  sql_rows.set_num_rows(num_rows);

  size_t sql_index = 0;

  for (sql_index = 0; sql_index < num_rows; ++sql_index) {
    assert(next_index <= max_index);

    bool completed = false;
    err = format_row(thd, share, text_rows, next_index, buffer, buffer_length,
                     charset, sql_rows, sql_index, completed, error_details);

    if (!completed || err != 0) {
      break;
    }
    ++next_index;
  }

  sql_rows.set_num_rows(sql_index);
  return err;
}

DEFINE_METHOD(bool, is_killed, (THD * thd)) {
  return (thd->killed != THD::NOT_KILLED);
}
}  // namespace Bulk_data_convert

namespace Bulk_data_load {

DEFINE_METHOD(void *, begin,
              (THD * thd, const TABLE *table, size_t &num_threads)) {
  auto ctx = table->file->bulk_load_begin(thd, num_threads);
  return ctx;
}

DEFINE_METHOD(bool, load,
              (THD * thd, void *ctx, const TABLE *table,
               const Rows_mysql &sql_rows, size_t thread)) {
  int err = table->file->bulk_load_execute(thd, ctx, thread, sql_rows);
  return (err == 0);
}

DEFINE_METHOD(bool, end,
              (THD * thd, void *ctx, const TABLE *table, bool error)) {
  int err = table->file->bulk_load_end(thd, ctx, error);
  return (err == 0);
}

bool check_for_deprecated_use(Field_float *field) {
  if (!field->not_fixed) {
    std::ostringstream err_strm;
    String type_string(64);
    err_strm << "LOAD DATA doesn't support fixed size FLOAT columns, they "
                "are deprecated. Decimals should be used instead.";
    LogErr(INFORMATION_LEVEL, ER_BULK_LOADER_INFO, err_strm.str().c_str());
    my_error(ER_FEATURE_UNSUPPORTED, MYF(0),
             "fixed size FLOAT column (deprecated)", "By LOAD BULK DATA");
    return false;
  } else if (field->is_unsigned()) {
    std::ostringstream err_strm;
    String type_string(64);
    err_strm << "Bulk load doesn't support UNSIGNED FLOAT columns. CHECK "
                "constraints can be used in instead.";
    LogErr(INFORMATION_LEVEL, ER_BULK_LOADER_INFO, err_strm.str().c_str());
    my_error(ER_FEATURE_UNSUPPORTED, MYF(0), "UNSIGNED FLOAT column",
             "By LOAD BULK DATA");
    return false;
  }
  return true;
}

bool check_for_deprecated_use(Field_double *field) {
  if (!field->not_fixed) {
    std::ostringstream err_strm;
    String type_string(64);
    err_strm << "LOAD DATA doesn't support fixed size DOUBLE columns, they "
                "are deprecated. Decimals should be used instead.";
    LogErr(INFORMATION_LEVEL, ER_BULK_LOADER_INFO, err_strm.str().c_str());
    my_error(ER_FEATURE_UNSUPPORTED, MYF(0),
             "fixed size DOUBLE column (deprecated)", "By LOAD BULK DATA");
    return false;
  } else if (field->is_unsigned()) {
    std::ostringstream err_strm;
    String type_string(64);
    err_strm << "Bulk load doesn't support UNSIGNED DOUBLE columns. CHECK "
                "constraints can be used in instead.";
    LogErr(INFORMATION_LEVEL, ER_BULK_LOADER_INFO, err_strm.str().c_str());
    my_error(ER_FEATURE_UNSUPPORTED, MYF(0), "UNSIGNED DOUBLE column",
             "By LOAD BULK DATA");
    return false;
  }
  return true;
}

bool check_for_deprecated_use(Field_new_decimal *field) {
  if (field->is_unsigned()) {
    std::ostringstream err_strm;
    String type_string(64);
    err_strm << "Bulk load doesn't support UNSIGNED DECIMAL columns. CHECK "
                "constraints can be used in instead.";
    LogErr(INFORMATION_LEVEL, ER_BULK_LOADER_INFO, err_strm.str().c_str());
    my_error(ER_FEATURE_UNSUPPORTED, MYF(0), "UNSIGNED DECIMAL column",
             "By LOAD BULK DATA");
    return false;
  }
  return true;
}

bool check_for_deprecated_use(Field *field) {
  if (field->type() == MYSQL_TYPE_FLOAT) {
    auto field_float = dynamic_cast<Field_float *>(field);
    assert(field_float);
    return check_for_deprecated_use(field_float);
  } else if (field->type() == MYSQL_TYPE_DOUBLE) {
    auto field_double = dynamic_cast<Field_double *>(field);
    assert(field_double);
    return check_for_deprecated_use(field_double);
  } else if (field->type() == MYSQL_TYPE_NEWDECIMAL) {
    auto field_new_decimal = dynamic_cast<Field_new_decimal *>(field);
    assert(field_new_decimal);
    return check_for_deprecated_use(field_new_decimal);
  } else {
    /* Other types have no deprecation rules for now */
    return true;
  }
}

DEFINE_METHOD(bool, is_table_supported, (const TABLE *table)) {
  auto share = table->s;

  if (table_has_generated_invisible_primary_key(table)) {
    std::ostringstream err_strm;
    String type_string(64);
    err_strm << "LOAD BULK DATA not supported for tables with generated "
                "invisible primary key.";
    LogErr(INFORMATION_LEVEL, ER_BULK_LOADER_INFO, err_strm.str().c_str());
    my_error(ER_FEATURE_UNSUPPORTED, MYF(0), "GENERATED/INVISIBLE PRIMARY KEY",
             "By LOAD BULK DATA");
    return false;
  }

  if (table->triggers != nullptr) {
    std::ostringstream err_strm;
    String type_string(64);
    err_strm << "LOAD BULK DATA not supported for tables with triggers.";
    LogErr(INFORMATION_LEVEL, ER_BULK_LOADER_INFO, err_strm.str().c_str());
    my_error(ER_FEATURE_UNSUPPORTED, MYF(0), "TRIGGER", "By LOAD BULK DATA");
    return false;
  }

  if (table->table_check_constraint_list != nullptr) {
    std::ostringstream err_strm;
    String type_string(64);
    err_strm << "LOAD BULK DATA not supported for tables with CHECK "
                "constraints.";
    LogErr(INFORMATION_LEVEL, ER_BULK_LOADER_INFO, err_strm.str().c_str());
    my_error(ER_FEATURE_UNSUPPORTED, MYF(0), "CHECK constraint",
             "By LOAD BULK DATA");
    return false;
  }

  for (size_t index = 0; index < share->fields; ++index) {
    auto field = share->field[index];
    if (field->is_virtual_gcol()) {
      std::ostringstream err_strm;
      String type_string(64);
      field->sql_type(type_string);
      err_strm << "LOAD BULK DATA not supported for tables with virtual "
                  "generated columns.";
      LogErr(INFORMATION_LEVEL, ER_BULK_LOADER_INFO, err_strm.str().c_str());
      my_error(ER_FEATURE_UNSUPPORTED, MYF(0), "VIRTUAL/GENERATED columns",
               "By LOAD BULK DATA");
      return false;
    }

    switch (field->type()) {
      case MYSQL_TYPE_TINY:
      case MYSQL_TYPE_SHORT:
      case MYSQL_TYPE_INT24:
      case MYSQL_TYPE_LONG:
      case MYSQL_TYPE_LONGLONG:
      case MYSQL_TYPE_STRING:
      case MYSQL_TYPE_VARCHAR:
      case MYSQL_TYPE_NEWDECIMAL:
      case MYSQL_TYPE_FLOAT:
      case MYSQL_TYPE_DOUBLE:
      case MYSQL_TYPE_DATETIME:
      case MYSQL_TYPE_DATE:
      case MYSQL_TYPE_TIME:
        check_for_deprecated_use(field);
        continue;
      default:
        std::ostringstream err_strm;
        String type_string(64);
        field->sql_type(type_string);
        err_strm << "LOAD BULK DATA not supported for data type: "
                 << type_string.c_ptr_safe();
        LogErr(INFORMATION_LEVEL, ER_BULK_LOADER_INFO, err_strm.str().c_str());
        my_error(ER_FEATURE_UNSUPPORTED, MYF(0), type_string.c_ptr_safe(),
                 "By LOAD BULK DATA");
        return false;
    }
  }
  return true;
}

}  // namespace Bulk_data_load

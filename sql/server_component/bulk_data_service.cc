/*  Copyright (c) 2022, 2026, Oracle and/or its affiliates.

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
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*/

#include "mysql/components/services/bulk_data_service.h"
#include <assert.h>
#include <cstdint>
#include <thread>
#include "field_types.h"
#include "my_byteorder.h"
#include "my_time.h"
#include "mysql/components/service_implementation.h"
#include "mysql/components/services/log_builtins.h"
#include "mysql_time.h"
#include "mysqld_error.h"
#include "sql-common/json_dom.h"
#include "sql-common/my_decimal.h"
#include "sql/field.h"
#include "sql/item_strfunc.h"
#include "sql/sql_class.h"
#include "sql/sql_gipk.h"
#include "sql/sql_time.h"
#include "sql/strfunc.h"  // find_type2
#include "sql/tztime.h"
#include "vector-common/vector_conversion.h"

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

/** Create an integer column converting data from CSV text.
@param[in]   text_col         input column in text read from CSV
@param[in]   charset          character set for the input column data
@param[in]   field            table column metadata
@param[in]   write_in_buffer  write integer data in column buffer
@param[out]  sql_col          converted column in MySQL storage format
@param[out]  error_details  the error details
@return error code. */
template <typename S, typename U>
static int format_int_column(const Column_text &text_col,
                             const CHARSET_INFO *charset, const Field *field,
                             bool write_in_buffer, Column_mysql &sql_col,
                             Bulk_load_error_location_details &error_details) {
  int err = 0;
  const char *end;

  auto field_num = (const Field_num *)field;

  /* If field is nullptr, then it is generated rowid column */
  bool is_unsigned = (field == nullptr) ? true : field_num->is_unsigned();

  if (field == nullptr) {
    /* If field is nullptr, then it is generated rowid column */
    sql_col.m_int_data = text_col.m_row_id;
  } else {
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
  }

  /* Write the integer bytes in the buffer. */
  if (write_in_buffer) {
    /* This is written to temp files to be consumed later part of execution.
    We don't bother about BE/LE order here. */
    if (sql_col.m_type == MYSQL_TYPE_LONGLONG) {
      memcpy(sql_col.get_data(), (void *)(&sql_col.m_int_data),
             sizeof(uint64_t));
      sql_col.m_data_len = sizeof(uint64_t);
      return 0;
    }

    /* Unsigned integer less than or equal to four bytes. */
    if (is_unsigned) {
      /* Data is already checked to be within the range of S. */
      auto data_4 = static_cast<uint32_t>(sql_col.m_int_data);

      memcpy(sql_col.get_data(), (void *)(&data_4), sizeof(uint32_t));
      sql_col.m_data_len = sizeof(uint32_t);
      return 0;
    }

    /* Signed integer less than or equal to four bytes. */
    auto signed_val = static_cast<int64_t>(sql_col.m_int_data);
    /* Data is already checked to be within the range of S. */
    auto data_4 = static_cast<int32_t>(signed_val);

    memcpy(sql_col.get_data(), (void *)(&data_4), sizeof(int32_t));
    sql_col.m_data_len = sizeof(int32_t);
  }
  return 0;
}

/** Create a blob column.
@param[in]   field        table column metadata
@param[in]   from_cs      charset of the input string from CSV.
@param[in]   text_col     input column in text read from CSV.
@param[out]  sql_col      converted column in MySQL storage format
@param[out]  length_size  number of bytes used to write the length
@param[out]  error_details  error location details
@return 0 on success, error code on failure. */
static int format_blob_column(Field *field, const CHARSET_INFO *from_cs,
                              const Column_text &text_col,
                              Column_mysql &sql_col, size_t &length_size,
                              Bulk_load_error_location_details &error_details) {
  assert(!sql_col.m_is_null);
  auto field_str = (const Field_str *)field;
  const CHARSET_INFO *field_charset = field_str->charset();

  switch (sql_col.m_type) {
    case MYSQL_TYPE_TINY_BLOB:
      length_size = 1;
      break;
    case MYSQL_TYPE_BLOB:
      length_size = 2;
      break;
    case MYSQL_TYPE_MEDIUM_BLOB:
      length_size = 3;
      break;
    case MYSQL_TYPE_GEOMETRY:
      [[fallthrough]];
    case MYSQL_TYPE_JSON:
      [[fallthrough]];
    case MYSQL_TYPE_VECTOR:
    case MYSQL_TYPE_LONG_BLOB:
      length_size = 4;
      break;
    default:
      assert(0);
      break;
  }

  char *field_begin = sql_col.get_data();
  char *field_data = field_begin + length_size;

  size_t copy_size = text_col.m_data_len;

  if (text_col.is_ext()) {
    /* Column data stored externally. */
    memcpy(field_data, text_col.m_data_ptr, copy_size);

  } else {
    const char *error_pos = nullptr;
    const char *convert_error_pos = nullptr;
    const char *end_pos = nullptr;
    const size_t nchars = text_col.m_data_len;
    auto field_size = sql_col.m_data_len;

    if (sql_col.m_type == MYSQL_TYPE_VECTOR) {
      assert(!text_col.is_ext());
      const size_t len = text_col.m_data_len;

      uint32 input_dims = get_dimensions(len, Field_vector::precision);
      if (input_dims > Field_vector::max_dimensions) {
        error_details.m_column_length = len;
        error_details.column_input_data = text_col.m_data_ptr;
        return ER_TO_VECTOR_CONVERSION;
      }

      assert(input_dims != 0);

      /* Refer to Item_func_from_vector::per_value_chars */
      const uint32 per_value_chars = 16;
      uint32 out_length = input_dims * per_value_chars;

#ifndef NDEBUG
      const uint32 max_output_bytes =
          (Field_vector::max_dimensions * per_value_chars);
      assert(out_length <= max_output_bytes);
#endif /* NDEBUG */

      /* Refer to Field_vector::store(). */
      const char *from = text_col.m_data_ptr;
      for (uint32 i = 0; i < input_dims; i++) {
        float to_store = 0;
        memcpy(&to_store, from + sizeof(float) * i, sizeof(float));
        if (std::isnan(to_store) || std::isinf(to_store)) {
          error_details.m_column_length = len;
          error_details.column_input_data = text_col.m_data_ptr;
          return ER_TO_VECTOR_CONVERSION;
        }
      }

      auto ptr = std::make_unique<char[]>(out_length);
      if (from_vector_to_string(text_col.m_data_ptr, input_dims, ptr.get(),
                                &out_length)) {
        error_details.m_column_length = len;
        error_details.column_input_data = text_col.m_data_ptr;
        return ER_TO_VECTOR_CONVERSION;
      }
    }

    if (field_charset == &my_charset_bin) {
      /* If the charset of the field is binary, then the column data in the CSV
      would also be binary. Don't do any charset conversions. */
      if (text_col.m_data_len > sql_col.m_data_len) {
        error_details.m_column_length = sql_col.m_data_len;
        return ER_TOO_BIG_FIELDLENGTH;
      }
      memcpy(field_data, text_col.m_data_ptr, text_col.m_data_len);
      copy_size = text_col.m_data_len;
    } else {
      copy_size = well_formed_copy_nchars(
          field_charset, field_data, field_size, from_cs, text_col.m_data_ptr,
          text_col.m_data_len, nchars, &error_pos, &convert_error_pos,
          &end_pos);

      if (error_pos != nullptr || convert_error_pos != nullptr) {
        error_details.column_type = "text";
        log_conversion_error(text_col, "Invalid Input String: ");
        return ER_INVALID_CHARACTER_STRING;
      }

      if (end_pos < text_col.m_data_ptr + text_col.m_data_len) {
        error_details.m_column_length = sql_col.m_data_len;
        return ER_TOO_BIG_FIELDLENGTH;
      }
    }

    if (sql_col.m_type == MYSQL_TYPE_JSON) {
      std::string errmsg;
      auto error_handler = [&errmsg](const char *err, size_t) { errmsg = err; };

      auto depth_handler = []() {};

      auto dom =
          Json_dom::parse(field_data, copy_size, error_handler, depth_handler);
      if (dom.get() == nullptr) {
        error_details.m_error_mesg = errmsg;
        return ER_BULK_JSON_INVALID;
      }
      String value;
      Bulk_load::Json_serialization_error_handler error_object;
      bool failure = json_binary::serialize(dom.get(), error_object, &value);
      if (failure) {
        /* failure */
        error_details.m_error_mesg = error_object.get_error();
        return ER_BULK_JSON_INVALID;
      }
      memcpy(field_data, value.ptr(), value.length());
      copy_size = value.length();
    }
  }

  switch (sql_col.m_type) {
    case MYSQL_TYPE_TINY_BLOB:
      *field_begin = static_cast<uint8_t>(copy_size);
      break;
    case MYSQL_TYPE_BLOB:
      int2store(field_begin, static_cast<uint16_t>(copy_size));
      break;
    case MYSQL_TYPE_MEDIUM_BLOB:
      int3store(field_begin, copy_size);
      break;
    case MYSQL_TYPE_GEOMETRY:
      [[fallthrough]];
    case MYSQL_TYPE_JSON:
      [[fallthrough]];
    case MYSQL_TYPE_VECTOR:
      [[fallthrough]];
    case MYSQL_TYPE_LONG_BLOB:
      int4store(field_begin, copy_size);
      break;
    default:
      assert(0);
      break;
  }

  sql_col.m_data_len = copy_size;

  return 0;
}

/** Create a char/varchar column converting data to MySQL storage format.
@param[in]   text_col     input column in text read from CSV
@param[in]   charset      character set for the input column data
@param[in]   field        table column metadata
@param[in]   write_length write length of column data if variable length
@param[in]   col_meta     column metadata
@param[in]   single_byte  if true, allocation is done assuming single byte
                          return ER_TOO_BIG_FIELDLENGTH if not enough
@param[out]  sql_col      converted column in MySQL storage format
@param[out]  length_size  number of bytes used to write the length
@param[out]  error_details  error location details
@return error code. */
static int format_char_column(const Column_text &text_col,
                              const CHARSET_INFO *charset, const Field *field,
                              bool write_length, const Column_meta &col_meta,
                              bool single_byte, Column_mysql &sql_col,
                              size_t &length_size,
                              Bulk_load_error_location_details &error_details) {
  auto field_str = (const Field_str *)field;
  const CHARSET_INFO *field_charset = field_str->charset();

  auto field_char_size = field_str->char_length_cache;
  auto field_size = sql_col.m_data_len;

  /* We consider character data as fixed length if it can be adjusted within
  single byte char allocation, e.g. for CHAR(N), we take N bytes as the fixed
  length and if it exceeds N bytes because of multi-byte characters we consider
  it as variable length and write as varchar in length + data format. The idea
  here is to avoid allocating too much fixed length unused space. */
  bool fixed_length =
      col_meta.m_is_fixed_len || col_meta.m_fixed_len_if_set_in_row;
  length_size = 0;

  if (write_length) {
    length_size = col_meta.m_is_single_byte_len ? 1 : 2;
  }

  /* For non-key, fixed length char data adjusted within single byte length, we
  skip writing length byte(s). In such case, row header is marked to indicate
  that length bytes are not present for fixed length types. This added
  complexity helps in saving temp storage size for fixed length char. */
  bool no_length_char =
      single_byte && col_meta.m_fixed_len_if_set_in_row && !col_meta.m_is_key;

  if (col_meta.m_is_fixed_len || no_length_char) {
    length_size = 0;
  }

  char *field_begin = sql_col.get_data();
  char *field_data = field_begin + length_size;

  const char *error_pos = nullptr;
  const char *convert_error_pos = nullptr;
  const char *end_pos = nullptr;

  size_t copy_size{0};

  if (text_col.m_data_len > field_size) {
    error_details.m_column_length = sql_col.m_data_len;
    return ER_TOO_BIG_FIELDLENGTH;
  }

  if (text_col.is_ext()) {
    assert(text_col.m_data_len == 20);
    memcpy(field_data, text_col.m_data_ptr, text_col.m_data_len);
    copy_size = text_col.m_data_len;
  } else {
    copy_size = well_formed_copy_nchars(
        field_charset, field_data, field_size, charset, text_col.m_data_ptr,
        text_col.m_data_len, field_char_size, &error_pos, &convert_error_pos,
        &end_pos);

    if (end_pos < text_col.m_data_ptr + text_col.m_data_len) {
      /* The error is expected when fixed_length = true, where we try to adjust
      the data within character length limit. The data could not be fit in
      such limit here which is possible for multi-byte character set. We
      return from here and retry with variable length format - mysql_format() */
      if (fixed_length && single_byte) {
        error_details.m_column_length = sql_col.m_data_len;
        return ER_TOO_BIG_FIELDLENGTH;
      }
      error_details.column_type = "string";
      log_conversion_error(text_col, "Input String too long: ");
      return ER_LOAD_BULK_DATA_WRONG_VALUE_FOR_FIELD;
    }

    if (error_pos != nullptr || convert_error_pos != nullptr) {
      error_details.column_type = "string";
      log_conversion_error(text_col, "Invalid Input String: ");
      return ER_LOAD_BULK_DATA_WRONG_VALUE_FOR_FIELD;
    }
  }

  auto data_length = copy_size;

  /* For char[] column need to fill padding characters. */
  if (fixed_length && copy_size < field_size) {
    size_t fill_size = field_size - copy_size;
    char *fill_pos = field_data + copy_size;

    field_charset->cset->fill(field_charset, fill_pos, fill_size,
                              field_charset->pad_char);
    data_length = field_size;
  }

  sql_col.set_data(field_data);
  sql_col.m_data_len = data_length;

  if (length_size == 0) {
    return 0;
  }

  assert(write_length);

  /* Write length for varchar column. */
  if (length_size == 1) {
    *field_begin = static_cast<unsigned char>(data_length);
    return 0;
  }

  assert(length_size == 2);
  int2store(field_begin, static_cast<uint16_t>(data_length));

  return 0;
}

/** Create a FLOAT column converting data to MySQL storage format.
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

  float4store((uchar *)sql_col.get_data(), nr);

  return 0;
}

/** Create a DOUBLE column converting data to MySQL storage format.
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

  if (field_double->truncate(&nr, DBL_MAX) != Field_real::TR_OK) {
    error_details.column_type = "double";
    log_conversion_error(text_col, "Invalid value for type: ");
    return ER_LOAD_BULK_DATA_WRONG_VALUE_FOR_FIELD;
  }

  float8store((uchar *)sql_col.get_data(), nr);

  return 0;
}

/** Create a DECIMAL column converting data to MySQL storage format.
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

    auto field_begin = (unsigned char *)sql_col.get_data();
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

/** Create a DATETIME column converting data to MySQL storage format.
@param[in]   thd            session THD
@param[in]   text_col       input column in text read from CSV
@param[in]   charset        character set for the input column data
@param[in]   field          table column metadata
@param[out]  sql_col        converted column in MySQL storage format
@param[out]  error_details  the error details
@return error code. */
int format_datetime_column(THD *thd, const Column_text &text_col,
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
  auto field_begin = (unsigned char *)sql_col.get_data();

  my_datetime_packed_to_binary(packed, field_begin,
                               field_date->get_fractional_digits());

  return 0;
}

/** Create a DATE column converting data to MySQL storage format.
@param[in]   thd            session THD
@param[in]   text_col       input column in text read from CSV
@param[in]   charset        character set for the input column data
@param[in]   field          table column metadata
@param[out]  sql_col        converted column in MySQL storage format
@param[out]  error_details  error location details
@return error code. */
int format_date_column(THD *thd, const Column_text &text_col,
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

  datetime_to_date(time);
  Date_val date = Date_val(*time);

  /* Convert to storage format. */
  auto field_begin = (unsigned char *)sql_col.get_data();
  date.store_date(field_begin);

  return 0;
}

class Row_header {
 public:
  enum Flag {
    /** If there is one or more NULL data in current row. */
    HAS_NULL_DATA = 1,
    /** Character data is fixed length. */
    IS_FIXED_CHAR = 2,
    /** Don't define flag beyond this maximum. */
    FLAG_MAX = 16
  };

  /** Matches MAX_FIELDS in SQL. We need separate definition here as we have
  array of this size allocated from stack. If SQL increases the value in future
  we need to re-evaluate and possibly go for dynamic allocation. We don't want
  to use dynamic allocation unconditionally as it impacts performance. */
  const static size_t MAX_NULLABLE_BYTES = 512;

  /** Construct header.
  @param[in]  row_meta row metadata. */
  explicit Row_header(const Row_meta &row_meta);

  /** Serialize header into a buffer.
  @param[in,out]  buffer  buffer to write to
  @param[in]      length  buffer length
  @return true iff successful. */
  bool serialize(char *buffer, size_t length);

  /** De-Serialize header from a buffer.
  @param[in]  buffer  buffer to write to
  @param[in]  length  buffer length
  @return true iff successful. */
  bool deserialize(const char *buffer, size_t length);

  /** Add length to row.
  @param[in]  add  length to add */
  void add_length(size_t add) { m_row_length += add; }

  /** @return current row length. */
  size_t get_row_length() const { return m_row_length; }

  /** Set specific flag.
  @param[in]  flag  flag to set */
  void set(Flag flag) { m_flags |= static_cast<uint16_t>(1 << (flag - 1)); }

  /** Check if a specific flag is set.
  @param[in]  flag  flag to check
  @return true iff set. */
  bool is_set(Flag flag) const {
    return ((m_flags & static_cast<uint16_t>(1 << (flag - 1))) > 0);
  }

  /** Set the column value as NULL in header.
  @param[in]  col_meta  column metadata */
  void set_column_null(const Column_meta &col_meta);

  /** check if column value is NULL in header.
  @param[in]  col_meta  column metadata
  @return true iff NULL */
  bool is_column_null(const Column_meta &col_meta) const;

  /** @return total header length. */
  size_t header_length() const {
    return m_null_bitmap_length + sizeof(m_row_length) + sizeof(m_flags);
  }

 private:
  /** NULL bitmap for the row. Needed only while sorting by key. */
  std::array<unsigned char, MAX_NULLABLE_BYTES> m_null_bitmap;

  /** Actual length of bitmap in bytes. Must be less than or equal to
  MAX_NULLABLE_BYTES. */
  size_t m_null_bitmap_length{};

  /** Current row length. */
  uint16_t m_row_length{};

  /** Row flags : 2 bytes, maximum 16 flags */
  uint16_t m_flags{};
};

Row_header::Row_header(const Row_meta &metadata) {
  m_null_bitmap_length = metadata.m_bitmap_length;
  memset(m_null_bitmap.data(), 0, m_null_bitmap_length);
  m_row_length = 0;
  m_flags = 0;
}

void Row_header::set_column_null(const Column_meta &col_meta) {
  assert(col_meta.m_is_nullable);

  unsigned char &null_byte = m_null_bitmap[col_meta.m_null_byte];
  null_byte |= static_cast<unsigned char>(1 << col_meta.m_null_bit);
}

bool Row_header::is_column_null(const Column_meta &col_meta) const {
  const unsigned char &null_byte = m_null_bitmap[col_meta.m_null_byte];
  return ((null_byte & static_cast<unsigned char>(1 << col_meta.m_null_bit)) !=
          0);
}

bool Row_header::serialize(char *buffer, size_t length) {
  if (length < header_length()) {
    return false;
  }

  int2store((uchar *)buffer, m_row_length);
  buffer += sizeof(uint16_t);

  int2store((uchar *)buffer, m_flags);
  buffer += sizeof(uint16_t);

  memcpy(buffer, m_null_bitmap.data(), m_null_bitmap_length);
  return true;
}

bool Row_header::deserialize(const char *buffer, size_t length) {
  if (length < header_length()) {
    return false;
  }
  m_row_length = uint2korr((const uchar *)buffer);
  buffer += sizeof(uint16_t);

  m_flags = uint2korr((const uchar *)buffer);
  buffer += sizeof(uint16_t);

  auto dest = m_null_bitmap.data();
  memcpy(dest, buffer, m_null_bitmap_length);
  return true;
}

/** Create a TIME column converting data to MySQL storage format.
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

  MYSQL_TIME *my_time = &ltime;
  MYSQL_TIME tz_ltime;
  my_timeval tm;

  if (ltime.time_type == MYSQL_TIMESTAMP_DATETIME_TZ) {
    tz_ltime = ltime;
    my_time = &tz_ltime;

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

  if (non_zero_date(*my_time)) {
    error_details.column_type = "time";
    log_conversion_error(text_col, "TIME includes DATE: ");
    return ER_LOAD_BULK_DATA_WRONG_VALUE_FOR_FIELD;
  }

  Time_val time = Time_val(*my_time);
  /* Convert to storage format. */
  auto field_begin = (unsigned char *)sql_col.get_data();
  time.store_time(field_begin, field_date->get_fractional_digits());

  return 0;
}

/** Create a TIMESTAMP column converting data to MySQL storage format.
@param[in]   thd            session THD
@param[in]   text_col       input column in text read from CSV
@param[in]   charset        character set for the input column data
@param[in]   field          table column metadata
@param[out]  sql_col        converted column in MySQL storage format
@param[out]  error_details  the error details
@return error code. */
static int format_timestamp_column(
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
    error_details.column_type = "timestamp";
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
      error_details.column_type = "timestamp";
      log_conversion_error(text_col, "TZ displacement failed: ");
      return ER_LOAD_BULK_DATA_WRONG_VALUE_FOR_FIELD;
    }

    /* Check for boundary conditions by converting to a timeval */
    my_timeval tm_not_used;
    res = datetime_with_no_zero_in_date_to_timeval(&tz_ltime, *tz, &tm_not_used,
                                                   &status.warnings);
    if (res || status.warnings != 0) {
      error_details.column_type = "timestamp";
      log_conversion_error(text_col, "TZ boundary check failed: ");
      return ER_LOAD_BULK_DATA_WRONG_VALUE_FOR_FIELD;
    }
  }

  my_timeval tm;
  if (datetime_with_no_zero_in_date_to_timeval(time, *thd->time_zone(), &tm,
                                               &status.warnings)) {
    tm.m_tv_sec = tm.m_tv_usec = 0;
  }
  if (tm.m_tv_sec > TYPE_TIMESTAMP_MAX_VALUE) {
    tm.m_tv_sec = tm.m_tv_usec = 0;
    status.warnings |= MYSQL_TIME_WARN_OUT_OF_RANGE;
  }

  if (status.warnings != 0) {
    error_details.column_type = "timestamp";
    log_conversion_error(text_col, "Invalid TIMESTAMP: ");
    return ER_LOAD_BULK_DATA_WRONG_VALUE_FOR_FIELD;
  }

  auto field_begin = (unsigned char *)sql_col.get_data();
  my_timestamp_to_binary(&tm, field_begin, field_date->get_fractional_digits());

  return 0;
}

/** Create a YEAR column converting data to MySQL storage format.
@param[in]   text_col       input column in text read from CSV
@param[in]   charset        character set for the input column data
@param[out]  sql_col        converted column in MySQL storage format
@param[out]  error_details  the error details
@return error code. */
static int format_year_column(const Column_text &text_col,
                              const CHARSET_INFO *charset,
                              Column_mysql &sql_col,
                              Bulk_load_error_location_details &error_details) {
  constexpr const uint MIN_YEAR{1901}; /* minimum 4 digits year */
  constexpr const uint MAX_YEAR{2155}; /* maximum 4 digits year */

  int err = 0;
  const char *end;

  long long val = charset->cset->strntoull10rnd(
      charset, text_col.m_data_ptr, text_col.m_data_len, 0, &end, &err);
  if (err != 0) {
    error_details.column_type = "year";
    log_conversion_error(text_col, "Integer conversion failed for: ");
    return ER_LOAD_BULK_DATA_WRONG_VALUE_FOR_FIELD;
  }

  if (val < 0 || (val >= 100 && val < MIN_YEAR) || val > MAX_YEAR) {
    error_details.column_type = "year";
    log_conversion_error(text_col, "Unsigned Integer out of range: ");
    return ER_LOAD_BULK_DATA_WRONG_VALUE_FOR_FIELD;
  }

  if (val != 0 || text_col.m_data_len != 4) {
    if (val < 70)
      val += 100;  // 2000 - 2069
    else if (val > 1900)
      val -= 1900;
  }

  sql_col.m_int_data = val;

  /* accurate mysql row format, because Loader::Thread_data::store_int_col()
     doesn't treat YEAR type specially. */
  sql_col.m_data_len = 1;
  *sql_col.get_data() = static_cast<unsigned char>(val);

  return 0;
}

/** Create a BIT column converting data to MySQL storage format.
@param[in]   text_col       input column in text read from CSV
@param[out]  sql_col        converted column in MySQL storage format
@param[out]  error_details  the error details
@return error code. */
static int format_bit_column(const Column_text &text_col, Column_mysql &sql_col,
                             Bulk_load_error_location_details &error_details) {
  if (text_col.m_data_len != sql_col.m_data_len) {
    error_details.column_type = "bit";
    log_conversion_error(text_col, "Input Binary string size wrong: ");
    return ER_LOAD_BULK_DATA_WRONG_VALUE_FOR_FIELD;
  }

  /* BIT type is binary string as it is */
  memcpy(sql_col.get_data(), text_col.m_data_ptr, text_col.m_data_len);

  return 0;
}

/** Create a ENUM column converting data to MySQL storage format.
@param[in]   text_col       input column in text read from CSV
@param[in]   charset        character set for the input column data
@param[in]   field          table column metadata
@param[out]  sql_col        converted column in MySQL storage format
@param[out]  error_details  the error details
@return error code. */
static int format_enum_column(const Column_text &text_col,
                              const CHARSET_INFO *charset, const Field *field,
                              Column_mysql &sql_col,
                              Bulk_load_error_location_details &error_details) {
  auto field_enum = (const Field_enum *)field;
  const CHARSET_INFO *field_charset = field_enum->charset();

  auto from = text_col.m_data_ptr;
  auto length = text_col.m_data_len;

  char buff[STRING_BUFFER_USUAL_SIZE];
  String tmpstr(buff, sizeof(buff), &my_charset_bin);

  /* Convert character set if necessary */
  if (String::needs_conversion_on_storage(length, charset, field_charset)) {
    uint dummy_errors;
    tmpstr.copy(from, length, charset, field_charset, &dummy_errors);
    from = tmpstr.ptr();
    length = tmpstr.length();
  }

  /* Remove end space */
  length = field_charset->cset->lengthsp(field_charset, from, length);
  uint tmp = find_type2(field_enum->typelib, from, length, field_charset);
  if (!tmp) {
    if (length > 5) {
      /* Can't be more than 99999 enums */
      error_details.column_type = "enum";
      log_conversion_error(text_col, "Invalid value for Enum: ");
      return ER_LOAD_BULK_DATA_WRONG_VALUE_FOR_FIELD;
    }

    /* This is for reading numbers with LOAD DATA INFILE */
    int err = 0;
    const char *end;
    tmp = (uint)my_strntoul(charset, from, length, 10, &end, &err);
    if (err || end != from + length || tmp > field_enum->typelib->count) {
      error_details.column_type = "enum";
      log_conversion_error(text_col, "Invalid value for Enum: ");
      return ER_LOAD_BULK_DATA_WRONG_VALUE_FOR_FIELD;
    }
  }

  sql_col.m_int_data = tmp;

  /* accurate mysql row format, because Loader::Thread_data::store_int_col()
     doesn't treat ENUM type specially. */
  sql_col.m_data_len = field_enum->pack_length();

  /* At this point, should be stored in little-endian as mysql row format. */
  switch (sql_col.m_data_len) {
    case 1:
      sql_col.get_data()[0] = (uchar)tmp;
      break;
    case 2:
      int2store(sql_col.get_data(), (unsigned short)tmp);
      break;
    case 3:
      int3store(sql_col.get_data(), (long)tmp);
      break;
    default:
      assert(false);
      break;
  }

  return 0;
}

/** Create a SET column converting data to MySQL storage format.
@param[in]   text_col       input column in text read from CSV
@param[in]   charset        character set for the input column data
@param[in]   field          table column metadata
@param[out]  sql_col        converted column in MySQL storage format
@param[out]  error_details  the error details
@return error code. */
static int format_set_column(const Column_text &text_col,
                             const CHARSET_INFO *charset, const Field *field,
                             Column_mysql &sql_col,
                             Bulk_load_error_location_details &error_details) {
  auto field_set = (const Field_enum *)field;
  const CHARSET_INFO *field_charset = field_set->charset();

  auto from = text_col.m_data_ptr;
  auto length = text_col.m_data_len;

  char buff[STRING_BUFFER_USUAL_SIZE];
  String tmpstr(buff, sizeof(buff), &my_charset_bin);

  /* Convert character set if necessary */
  if (String::needs_conversion_on_storage(length, charset, field_charset)) {
    uint dummy_errors;
    tmpstr.copy(from, length, charset, field_charset, &dummy_errors);
    from = tmpstr.ptr();
    length = tmpstr.length();
  }

  bool got_warning = false;
  const char *not_used;
  uint not_used2;
  ulonglong tmp = find_set(field_set->typelib, from, length, field_charset,
                           &not_used, &not_used2, &got_warning);

  if (!tmp && length && length < 22) {
    /* This is for reading numbers with LOAD DATA INFILE */
    int err = 0;
    const char *end;
    tmp = my_strntoull(charset, from, length, 10, &end, &err);
    if (err || end != from + length ||
        (field_set->typelib->count < 64 &&
         tmp >= (1ULL << field_set->typelib->count))) {
      error_details.column_type = "set";
      log_conversion_error(text_col, "Invalid value for Set: ");
      return ER_LOAD_BULK_DATA_WRONG_VALUE_FOR_FIELD;
    }
  } else if (got_warning) {
    error_details.column_type = "set";
    log_conversion_error(text_col, "Invalid value for Set: ");
    return ER_LOAD_BULK_DATA_WRONG_VALUE_FOR_FIELD;
  }

  sql_col.m_int_data = tmp;

  /* accurate mysql row format, because Loader::Thread_data::store_int_col()
     doesn't treat SET type specially. */
  sql_col.m_data_len = field_set->pack_length();

  /* At this point, should be stored in little-endian as mysql row format */
  switch (sql_col.m_data_len) {
    case 1:
      sql_col.get_data()[0] = (uchar)tmp;
      break;
    case 2:
      int2store(sql_col.get_data(), (unsigned short)tmp);
      break;
    case 3:
      int3store(sql_col.get_data(), (long)tmp);
      break;
    case 4:
      int4store(sql_col.get_data(), (long)tmp);
      break;
    default:
      assert(false);
      break;
  }

  return 0;
}

/** Create a row in converting column data to MySQL storage format.
@param[in]   thd              session THD
@param[in]   table_share      shared table object
@param[in]   text_rows        input rows with columns in text read from CSV
@param[in]   text_row_index   current text row index
@param[in]   buffer           buffer to write to
@param[in]   buffer_length    buffer length
@param[in]   charset          character set for the input row data
@param[in]   metadata         row metadata
@param[out]  sql_rows         converted row in MySQL storage format
@param[in]   sql_row_index    current sql row index
@param[in]   single_byte_char assume single byte char length is enough
@param[out]  completed       if all rows are processed
@param[out]  error_details   the error details
@return 0 on success or error code  */
static int format_row(THD *thd, const TABLE_SHARE *table_share,
                      const Rows_text &text_rows, size_t text_row_index,
                      char *&buffer, size_t &buffer_length,
                      const CHARSET_INFO *charset, const Row_meta &metadata,
                      Rows_mysql &sql_rows, size_t sql_row_index,
                      bool single_byte_char, bool &completed,
                      Bulk_load_error_location_details &error_details) {
  const auto row_begin = buffer;

  /* For error cases, we don't consume the buffer and revert to saved values. */
  auto saved_buffer = buffer;
  auto saved_buffer_length = buffer_length;

  /* For sorted data load, we format the row by processing each column in same
  order as it appears in table and don't bother about the Primary Key. In this
  case with_keys is FALSE.

  For unsorted load, we format the row by processing Primary key columns first
  and only holding the key columns in Rows_mysql. The non-key column follows
  the key columns and all column data is written to the output buffer. In this
  case with_keys is TRUE. The Key columns in Rows_mysql rows are used for
  sorting the data. The rows in buffer is written to temp files in order of
  keys using data pointer to the output buffer. */
  bool with_keys = (metadata.m_keys != 0);
  auto header_buffer = buffer;

  Row_header header(metadata);
  auto header_length = with_keys ? header.header_length() : 0;

  /* Check if buffer is fully consumed. */
  if (buffer_length < header_length) {
    completed = false;
    return 0;
  }

  buffer_length -= header_length;
  buffer += header_length;

  completed = true;
  /* Loop through all the columns and convert input data. */
  int err = 0;

  auto text_row_offset = text_rows.get_row_offset(text_row_index);
  auto sql_row_offset = sql_rows.get_row_offset(sql_row_index);

  size_t sql_index = 0;
  bool has_null_data = false;

  for (const auto &col_meta : metadata.m_columns) {
    auto text_index = col_meta.m_field_index;
    Field *field = nullptr;

    /* The table_share does not know about the generated clustered
    index.  But text_rows contain the generated row id. The variable is_rowid
    indicates whether the current column is the generated row id. */
    const bool is_rowid =
        metadata.dbrowid_is_pk && col_meta.m_field_name == "DB_ROW_ID";

    assert(text_index < table_share->fields ||
           (is_rowid && text_index == UINT16_MAX));

    if (is_rowid) {
      text_index = 0;
    }

    if (!is_rowid) {
      field = table_share->field[text_index];

      if (table_share->is_missing_primary_key()) {
        text_index++;
      }
    }

    auto &text_col = text_rows.read_column(text_row_offset, text_index);

    /* Only primary key can contain externally stored fields. */
    assert(!text_col.is_ext() || metadata.is_pk);

    /* With keys we are interested to fill only the key columns. */
    bool use_temp = (with_keys && sql_index >= metadata.m_keys);
    Column_mysql col_temp;

    auto &sql_col =
        use_temp ? col_temp : sql_rows.get_column(sql_row_offset, sql_index);

    ++sql_index;

    bool fixed_length =
        col_meta.m_is_fixed_len || col_meta.m_fixed_len_if_set_in_row;

    auto field_size = static_cast<size_t>(fixed_length && single_byte_char
                                              ? col_meta.m_fixed_len
                                              : col_meta.m_max_len);

    /* Two bytes more for varchar data length. Eight bytes for integer types. */
    if (buffer_length < field_size + 2 || buffer_length < sizeof(uint64_t)) {
      /* No space left in buffer. */
      completed = false;
      break;
    }
    size_t length_size = 0;

    sql_col.set_data(buffer);
    sql_col.m_data_len = field_size;
    sql_col.m_int_data = 0;

    /* If field is a nullptr, then it is generated rowid column */
    sql_col.m_is_null =
        (field == nullptr) ? false : (text_col.m_data_ptr == nullptr);

    assert(!is_rowid || !sql_col.m_is_null);
    assert(!is_rowid || field == nullptr);

    /* If field is a nullptr, then it is generated rowid column */
    sql_col.m_type = (field == nullptr) ? MYSQL_TYPE_LONGLONG
                                        : static_cast<int>(field->type());

    if (sql_col.m_type == MYSQL_TYPE_BLOB) {
      /* Get more accurate blob type. */
      sql_col.m_type = get_blob_type_from_length(field->max_data_length());
    }

    if (sql_col.m_is_null) {
      sql_col.row(row_begin);

      if (!field->is_nullable()) {
        LogErr(INFORMATION_LEVEL, ER_BULK_LOADER_INFO,
               "NULL value found for NOT NULL field!");
        error_details.column_name = field->field_name;
        error_details.column_input_data =
            std::string(text_col.m_data_ptr, text_col.m_data_len);
        err = ER_LOAD_BULK_DATA_WARN_NULL_TO_NOTNULL;
        break;
      }
      /* NULL bitmap is created for saving temporary data with keys. */
      if (with_keys) {
        header.set_column_null(col_meta);
        has_null_data = true;
      }
      continue;
    }

    /* TODO-4: We could have better interfacing if we can get an interface
    for a field to get the data in storage format. Currently we follow the
    ::store interface that writes the data to the row buffer stored in
    TABLE object. */
    switch (sql_col.m_type) {
      case MYSQL_TYPE_TINY:
        /* Column type TINYINT */
        err = format_int_column<int8_t, uint8_t>(
            text_col, charset, field, with_keys, sql_col, error_details);
        break;
      case MYSQL_TYPE_SHORT:
        /* Column type SMALLINT */
        err = format_int_column<int16_t, uint16_t>(
            text_col, charset, field, with_keys, sql_col, error_details);
        break;
      case MYSQL_TYPE_INT24:
        /* Column type MEDIUMINT */
        err = format_int_column<int32_t, uint32_t>(
            text_col, charset, field, with_keys, sql_col, error_details);
        break;
      case MYSQL_TYPE_LONG:
        /* Column type INT */
        err = format_int_column<int32_t, uint32_t>(
            text_col, charset, field, with_keys, sql_col, error_details);
        break;
      case MYSQL_TYPE_LONGLONG:
        /* Column type BIG */
        err = format_int_column<int64_t, uint64_t>(
            text_col, charset, field, with_keys, sql_col, error_details);
        break;
      case MYSQL_TYPE_VECTOR:
      case MYSQL_TYPE_BLOB:
        [[fallthrough]];
      case MYSQL_TYPE_TINY_BLOB:
        [[fallthrough]];
      case MYSQL_TYPE_MEDIUM_BLOB:
        [[fallthrough]];
      case MYSQL_TYPE_GEOMETRY:
        [[fallthrough]];
      case MYSQL_TYPE_JSON:
        [[fallthrough]];
      case MYSQL_TYPE_LONG_BLOB:
        err = format_blob_column(field, charset, text_col, sql_col, length_size,
                                 error_details);
        break;
      case MYSQL_TYPE_STRING:
        if (field->real_type() == MYSQL_TYPE_ENUM) {
          /* Column type ENUM */
          err = format_enum_column(text_col, charset, field, sql_col,
                                   error_details);
          break;
        } else if (field->real_type() == MYSQL_TYPE_SET) {
          /* Column type SET */
          err = format_set_column(text_col, charset, field, sql_col,
                                  error_details);
          break;
        }
        /* Column type CHAR(n) */
        [[fallthrough]];
      case MYSQL_TYPE_VARCHAR:
        /* Column type VARCHAR(n) */
        err = format_char_column(text_col, charset, field, with_keys, col_meta,
                                 single_byte_char, sql_col, length_size,
                                 error_details);
        break;
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
      case MYSQL_TYPE_YEAR:
        /* Column type YEAR */
        err = format_year_column(text_col, charset, sql_col, error_details);
        break;
      case MYSQL_TYPE_BIT:
        /* Column type BIT */
        err = format_bit_column(text_col, sql_col, error_details);
        break;
      case MYSQL_TYPE_TIMESTAMP:
        /* Column type TIMESTAMP */
        err = format_timestamp_column(thd, text_col, charset, field, sql_col,
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

    auto total_data_length = sql_col.m_data_len + length_size;
    assert(total_data_length <= buffer_length);

    if (total_data_length > buffer_length) {
      /* No space left in buffer. */
      completed = false;
      break;
    }
    buffer += total_data_length;
    buffer_length -= total_data_length;
    header.add_length(total_data_length);
  }

  if (with_keys && completed && err == 0) {
    if (single_byte_char) {
      header.set(Row_header::Flag::IS_FIXED_CHAR);
    }
    if (has_null_data) {
      header.set(Row_header::Flag::HAS_NULL_DATA);
    }
    bool success = header.serialize(header_buffer, header_length);
    assert(success);
    if (!success) {
      LogErr(INFORMATION_LEVEL, ER_IB_MSG_1381,
             "Bulk Load: Error writing NULL bitmap");
      err = ER_INTERNAL_ERROR;
    }
  }

  if (!completed || err != 0) {
    buffer = saved_buffer;
    buffer_length = saved_buffer_length;
  }

  return err;
}

/** Fill data in column from raw format.
@param[in]     row_begin      record begin pointer
@param[in,out] buffer         input raw data buffer
@param[in]     buffer_length  buffer length
@param[in]     col_meta       column metadata
@param[in]     header         row header
@param[in]     marked_fixed   if the row is marked as fixed length
@param[out]    col_length     column length
@param[out]    sql_col        column data
@return error code. */
static int fill_column_data(char *row_begin, char *buffer, size_t buffer_length,
                            const Column_meta &col_meta,
                            const Row_header &header, bool marked_fixed,
                            size_t &col_length, Column_mysql &sql_col) {
  sql_col.m_type = col_meta.m_type;
  sql_col.m_is_null = header.is_column_null(col_meta);
  sql_col.m_int_data = 0;
  sql_col.set_data(nullptr);
  sql_col.m_data_len = 0;
  col_length = 0;

#ifndef NDEBUG
  if (col_meta.m_is_pk) {
    /* Primary key columns cannot be null. */
    const bool is_col_null = header.is_column_null(col_meta);
    assert(!col_meta.m_is_key || !is_col_null);
  }
#endif /* NDEBUG */

  if (sql_col.m_is_null) {
    assert(col_meta.m_is_nullable);
    sql_col.row(row_begin);
    return 0;
  }

  switch (col_meta.m_type) {
    case MYSQL_TYPE_TINY_BLOB: {
      auto data_len = static_cast<uint8_t>(buffer[0]);
      sql_col.m_data_len = data_len;
      col_length = sql_col.m_data_len + 1;
      sql_col.set_data(buffer);
      return col_length > buffer_length ? ER_DATA_OUT_OF_RANGE : 0;
    } break;
    case MYSQL_TYPE_BLOB: {
      auto data_len = uint2korr(buffer);
      sql_col.m_data_len = data_len;
      col_length = sql_col.m_data_len + 2;
      sql_col.set_data(buffer);
      return col_length > buffer_length ? ER_DATA_OUT_OF_RANGE : 0;
    } break;
    case MYSQL_TYPE_MEDIUM_BLOB: {
      auto data_len = uint3korr(buffer);
      sql_col.m_data_len = data_len;
      col_length = sql_col.m_data_len + 3;
      sql_col.set_data(buffer);
      return col_length > buffer_length ? ER_DATA_OUT_OF_RANGE : 0;
    } break;
    case MYSQL_TYPE_GEOMETRY:
      [[fallthrough]];
    case MYSQL_TYPE_JSON:
      [[fallthrough]];
    case MYSQL_TYPE_VECTOR:
      [[fallthrough]];
    case MYSQL_TYPE_LONG_BLOB: {
      auto data_len = uint4korr(buffer);
      sql_col.m_data_len = data_len;
      col_length = sql_col.m_data_len + 4;
      sql_col.set_data(buffer);
      return col_length > buffer_length ? ER_DATA_OUT_OF_RANGE : 0;
    } break;
    default:
      /* Nothing. */
      break;
  }

  /* Check format_int_column() case write_in_buffer. */
  /* The another types (ENUM, SET, YEAR) must be excluded.
     (MYSQL_TYPE_STRING, MYSQL_TYPE_YEAR) */
  if (col_meta.is_integer() && sql_col.m_type != MYSQL_TYPE_STRING &&
      sql_col.m_type != MYSQL_TYPE_YEAR) {
    sql_col.set_data(buffer);

    if (sql_col.m_type == MYSQL_TYPE_LONGLONG) {
      sql_col.m_data_len = sizeof(uint64_t);

      assert(sql_col.m_data_len <= buffer_length);

      if (buffer_length < sql_col.m_data_len) {
        return ER_DATA_OUT_OF_RANGE;
      }
      col_length = sql_col.m_data_len;

      memcpy((void *)(&sql_col.m_int_data), sql_col.get_data(),
             sizeof(uint64_t));
      return 0;
    }

    /* Integer less than or equal to four bytes. */
    sql_col.m_data_len = sizeof(uint32_t);
    assert(sql_col.m_data_len <= buffer_length);

    if (buffer_length < sql_col.m_data_len) {
      return ER_DATA_OUT_OF_RANGE;
    }
    col_length = sql_col.m_data_len;

    /* Unsigned integer less than or equal to four bytes. */
    if (col_meta.m_is_unsigned) {
      uint32_t data_4 = 0;
      memcpy((void *)(&data_4), sql_col.get_data(), sizeof(uint32_t));
      sql_col.m_int_data = static_cast<uint64_t>(data_4);

      return 0;
    }

    /* Signed integer less than or equal to four bytes. */
    int32_t data_4 = 0;
    memcpy((void *)(&data_4), sql_col.get_data(), sizeof(int32_t));

    auto signed_val = static_cast<int64_t>(data_4);
    sql_col.m_int_data = static_cast<uint64_t>(signed_val);

    return 0;
  }

  /* For non-key, fixed length char data adjusted within single byte length,
  we skip writing length byte(s). In such case, row header is marked to
  indicate that length bytes are not present for fixed length types. This
  added added complexity helps in saving temp storage size for fixed length
  char. */
  bool no_length_char =
      marked_fixed && col_meta.m_fixed_len_if_set_in_row && !col_meta.m_is_key;

  if (col_meta.m_is_fixed_len || no_length_char) {
    sql_col.m_data_len = col_meta.m_fixed_len;
    sql_col.set_data(buffer);
    col_length = sql_col.m_data_len;

    if (col_meta.is_integer()) {
      /* needs sql_col.m_int_data for (ENUM, SET, YEAR) */
      switch (sql_col.m_data_len) {
        case 1:
          sql_col.m_int_data = buffer[0];
          break;
        case 2:
          sql_col.m_int_data = uint2korr(buffer);
          break;
        case 3:
          sql_col.m_int_data = uint3korr(buffer);
          break;
        case 4:
          sql_col.m_int_data = uint4korr(buffer);
          break;
        default:
          assert(false);
          break;
      }
    }

    assert(col_length <= buffer_length);
    return col_length > buffer_length ? ER_DATA_OUT_OF_RANGE : 0;
  }

  /* Variable length data. */
  size_t len_size = col_meta.m_is_single_byte_len ? 1 : 2;
  sql_col.set_data(buffer + len_size);

  if (col_meta.m_is_single_byte_len) {
    auto data_len = *(reinterpret_cast<unsigned char *>(buffer));
    sql_col.m_data_len = data_len;
  } else {
    auto data_len = uint2korr(buffer);
    sql_col.m_data_len = data_len;
  }
  col_length = sql_col.m_data_len + len_size;
  assert(col_length <= buffer_length);

  return col_length > buffer_length ? ER_DATA_OUT_OF_RANGE : 0;
}

/** Fill data in row from raw format.
@param[in,out] buffer         input raw data buffer
@param[in]     buffer_length  buffer length
@param[in]     fill_keys      true if keys to be filled otherwise the entire
row
@param[in]     metadata       row metadata
@param[in,out] header         row header
@param[in,out] sql_rows       row bunch to fill data
@param[in]     row_num        index of the row within row bunch
@param[out]    row_length     row length
@return error code. */
static int fill_row_data(char *buffer, size_t buffer_length, bool fill_keys,
                         const Row_meta &metadata, Row_header &header,
                         Rows_mysql &sql_rows, size_t row_num,
                         size_t &row_length) {
  row_length = 0;
  const auto row_begin = buffer;

  /* Not enough length left for header. */
  if (!header.deserialize(buffer, buffer_length)) {
    return 0;
  }
  bool fixed_length = header.is_set(Row_header::Flag::IS_FIXED_CHAR);

  auto header_length = header.header_length();
  row_length = header_length + header.get_row_length();

  /* Not enough length left for the row. */
  if (buffer_length < row_length) {
    assert(fill_keys);
    row_length = 0;
    return 0;
  }

  buffer += header_length;
  buffer_length -= header_length;

  size_t num_cols = sql_rows.get_num_cols();
  assert(!fill_keys || metadata.m_keys == num_cols);
  assert(fill_keys || metadata.m_num_columns == num_cols);

  size_t loop_count = 0;
  auto sql_row_offset = sql_rows.get_row_offset(row_num);

  for (const auto &col_meta : metadata.m_columns) {
    if (loop_count >= num_cols) {
      break;
    }
    assert(!fill_keys || col_meta.m_is_key);
    size_t col_index = 0;

    if (fill_keys) {
      col_index = loop_count;
    } else {
      col_index = static_cast<size_t>(col_meta.m_index);
      if (col_meta.m_is_prefix_key) {
        continue;
      }
    }
    auto &sql_col = sql_rows.get_column(sql_row_offset, col_index);
    ++loop_count;

    size_t consumed_length = 0;
    auto err = fill_column_data(row_begin, buffer, buffer_length, col_meta,
                                header, fixed_length, consumed_length, sql_col);
    if (err != 0) {
      return err;
    }
    assert(buffer_length >= consumed_length);
    buffer += consumed_length;
    buffer_length -= consumed_length;
  }
  return 0;
}

DEFINE_METHOD(int, mysql_format_using_key,
              (const Row_meta &metadata, const Rows_mysql &sql_keys,
               size_t key_offset, Rows_mysql &sql_rows, size_t sql_index)) {
  Row_header header(metadata);
  size_t row_length = 0;
  /* Get to the beginning of the row from first key. */
#if 0
  const auto &first_key = sql_keys.read_column(key_offset, 0);
  char *buffer = (first_key.m_is_null) ? first_key.m_data_ptr : first_key.m_data_ptr - metadata.m_first_key_len;

  /* In the case of secondary indexes, the key columns can be null. So, look
  for the first column that is not null. */
  size_t i = 0;
  for (i = 0; i < metadata.m_num_columns; ++i) {
    const auto &key = sql_keys.read_column(key_offset, i);
    if (key.m_data_ptr == nullptr) {
      continue;
    }
    break;
  }

  if (i == metadata.m_num_columns) {
    return ER_INTERNAL_ERROR;
  }
#endif

  const size_t first_col = 0;
  const auto &key = sql_keys.read_column(key_offset, first_col);

#if 0
  // const auto &colmeta = metadata.m_columns[first_col];
  /* number of bytes to store the length of column. */
  size_t len = 0;

  if (!colmeta.m_is_fixed_len) {
    len = colmeta.m_is_single_byte_len ? 1 : 2;
  }
#endif

  /* We cannot calculate this buffer, if all of them are null.  It is possible
   * for secondary indexes. */
  char *buffer = key.get_row_begin(metadata, first_col);
#if 0
  char *buffer = key.m_is_null
                     ? key.get_data()
                     : key.get_data() - len - metadata.m_header_length;
  // buffer -= metadata.m_header_length;
#endif

  /* We have already parsed the keys and the row must follow the pointer. Need
  to be updated if we support larger rows. */
  const size_t max_row_length = 64 * 1024;

  auto err = fill_row_data(buffer, max_row_length, false, metadata, header,
                           sql_rows, sql_index, row_length);
  return err;
}

DEFINE_METHOD(int, mysql_format_from_raw,
              (char *buffer, size_t buffer_length, const Row_meta &metadata,
               size_t start_index, size_t &consumed_length,
               Rows_mysql &sql_rows)) {
  consumed_length = 0;
  Row_header header(metadata);

  size_t max_index = sql_rows.get_num_rows();
  size_t sql_index = start_index;
  int err = 0;

  const bool fill_keys = true;

  for (sql_index = start_index; sql_index < max_index; ++sql_index) {
    size_t row_length = 0;
    err = fill_row_data(buffer, buffer_length, fill_keys, metadata, header,
                        sql_rows, sql_index, row_length);
    assert(buffer_length >= row_length);
    if (err != 0 || row_length == 0 || buffer_length < row_length) {
      break;
    }
    consumed_length += row_length;
    buffer += row_length;
    buffer_length -= row_length;
  }
  sql_rows.set_num_rows(sql_index);
  return err;
}

DEFINE_METHOD(int, mysql_format,
              (THD * thd, const TABLE *table, const Rows_text &text_rows,
               size_t &next_index, char *buffer, size_t &buffer_length,
               const CHARSET_INFO *charset, const Row_meta &metadata,
               Rows_mysql &sql_rows,
               Bulk_load_error_location_details &error_details)) {
  int err = 0;
  auto share = table->s;

  size_t num_text_rows = text_rows.get_num_rows();

  assert(next_index < num_text_rows);

  if (next_index >= num_text_rows || num_text_rows == 0) {
    return ER_INTERNAL_ERROR;
  }

  auto num_rows = num_text_rows - next_index;

  size_t sql_start_index = sql_rows.get_num_rows();
  size_t sql_max_index = sql_start_index + num_rows - 1;

  /* Pre allocate. */
  auto sql_num_rows = sql_max_index + 1;
  sql_rows.set_num_rows(sql_num_rows);
  assert(sql_num_rows == sql_max_index + 1);

  size_t sql_index = 0;

  for (sql_index = sql_start_index; sql_index <= sql_max_index; ++sql_index) {
    assert(next_index < num_text_rows);

    bool completed = false;
    /* First attempt assuming all fixed length char fits in single byte limit.
     */
    err = format_row(thd, share, text_rows, next_index, buffer, buffer_length,
                     charset, metadata, sql_rows, sql_index, true, completed,
                     error_details);

    if (err == ER_TOO_BIG_FIELDLENGTH) {
      /* Re-try with multi-byte allocation. All char columns are formatted as
      varchar for temp store. */
      err = format_row(thd, share, text_rows, next_index, buffer, buffer_length,
                       charset, metadata, sql_rows, sql_index, false, completed,
                       error_details);
    }

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

/* Written based on innobase_mysql_cmp() */
DEFINE_METHOD(int, compare_keys,
              (const Column_mysql &key1, const Column_mysql &key2,
               const Column_meta &col_meta)) {
  assert(col_meta.m_compare == Column_meta::Compare::MYSQL);

  auto type = static_cast<enum_field_types>(col_meta.m_type);
  int ret = 0;

  auto data_uptr1 = reinterpret_cast<const uchar *>(key1.get_data());
  auto data_uptr2 = reinterpret_cast<const uchar *>(key2.get_data());

  switch (type) {
    case MYSQL_TYPE_DATE: {
      assert(key1.m_data_len == 3);
      assert(key2.m_data_len == 3);
      const uint32_t k1 = uint3korr(data_uptr1);
      const uint32_t k2 = uint3korr(data_uptr2);
      if (k1 < k2) {
        ret = -1;
      } else if (k1 > k2) {
        ret = 1;
      } else {
        assert(k1 == k2);
        assert(ret == 0);
      }
    } break;
    case MYSQL_TYPE_FLOAT: {
      assert(key1.m_data_len >= sizeof(float));
      assert(key2.m_data_len >= sizeof(float));

      float fval1 = float4get(data_uptr1);
      float fval2 = float4get(data_uptr2);

      if (fval1 > fval2) {
        ret = 1;
      } else if (fval1 < fval2) {
        ret = -1;
      } else {
        assert(ret == 0);
        assert(fval1 == fval2);
      }
      break;
    }
    case MYSQL_TYPE_DOUBLE: {
      assert(key1.m_data_len >= sizeof(double));
      assert(key2.m_data_len >= sizeof(double));

      double dval1 = float8get(data_uptr1);
      double dval2 = float8get(data_uptr2);

      if (dval1 > dval2) {
        ret = 1;
      } else if (dval1 < dval2) {
        ret = -1;
      } else {
        assert(ret == 0);
        assert(dval1 == dval2);
      }
      break;
    }
    case MYSQL_TYPE_STRING:
    case MYSQL_TYPE_VARCHAR: {
      auto cs = static_cast<const CHARSET_INFO *>(col_meta.m_charset);
      auto l1 = key1.m_data_len;
      auto l2 = key2.m_data_len;

      if (type == MYSQL_TYPE_STRING && cs->pad_attribute == NO_PAD) {
        l1 = cs->cset->lengthsp(cs, key1.get_data(), l1);
        l2 = cs->cset->lengthsp(cs, key2.get_data(), l2);
      }
      ret = cs->coll->strnncollsp(cs, data_uptr1, l1, data_uptr2, l2);
      break;
    }
    default:
      assert(false);
      break;
  }
  return ret;
}

/** Fill column metadata type related information from  mysql field structure.
@param[in]  field     MySQL field from TABLE
@param[out] col_meta  column metadata object to fill */
static void set_data_type(const Field *field, Column_meta &col_meta) {
  col_meta.m_is_nullable = field->is_nullable();
  col_meta.m_is_unsigned = field->is_unsigned();
  col_meta.m_index = field->field_index();
  col_meta.m_type = field->type();
  auto type = field->type();

  if (col_meta.m_type == MYSQL_TYPE_BLOB) {
    /* Get more accurate blob type. */
    col_meta.m_type = get_blob_type_from_length(field->max_data_length());
  }

  switch (type) {
    case MYSQL_TYPE_TINY:
    case MYSQL_TYPE_SHORT:
    case MYSQL_TYPE_INT24:
    case MYSQL_TYPE_LONG:
    case MYSQL_TYPE_LONGLONG:
    case MYSQL_TYPE_YEAR:
      col_meta.m_compare = col_meta.m_is_unsigned
                               ? Column_meta::Compare::INTEGER_UNSIGNED
                               : Column_meta::Compare::INTEGER_SIGNED;
      break;
    case MYSQL_TYPE_DATE:
      col_meta.m_compare = Column_meta::Compare::MYSQL;
      break;
    case MYSQL_TYPE_NEWDECIMAL:
    case MYSQL_TYPE_DATETIME:
    case MYSQL_TYPE_TIME:
    case MYSQL_TYPE_TIMESTAMP:
    case MYSQL_TYPE_BIT:
      col_meta.m_compare = Column_meta::Compare::BINARY;
      break;
    case MYSQL_TYPE_BLOB:
      /* For blobs, comparison function will be used only if they are part of
      a prefix key. */
      col_meta.m_compare = Column_meta::Compare::BINARY;
      break;
    default:
      if (field->real_type() == MYSQL_TYPE_ENUM ||
          field->real_type() == MYSQL_TYPE_SET) {
        col_meta.m_is_unsigned = true;
        col_meta.m_compare = Column_meta::Compare::INTEGER_UNSIGNED;
        break;
      }
      assert(type == MYSQL_TYPE_STRING || type == MYSQL_TYPE_VARCHAR ||
             type == MYSQL_TYPE_FLOAT || type == MYSQL_TYPE_DOUBLE ||
             type == MYSQL_TYPE_JSON || type == MYSQL_TYPE_GEOMETRY ||
             type == MYSQL_TYPE_VECTOR);
      col_meta.m_compare = Column_meta::Compare::MYSQL;
      break;
  }
}

static bool is_field_part_of_sk(const TABLE *table, const Field *field) {
  auto table_share = table->s;

  for (size_t keynr = 0; keynr < table_share->keys; ++keynr) {
    if (keynr == table_share->primary_key) {
      continue;
    }
    if (field->part_of_key.is_set(keynr)) {
      return true;
    }
  }

  return false;
}

/** Fill column metadata from mysql field structure.
@param[in]  field     MySQL field from TABLE
@param[out] col_meta  column metadata object to fill */
static void fill_column_metadata(const Field *field, Column_meta &col_meta) {
  set_data_type(field, col_meta);

  col_meta.m_field_name = field->field_name;
  col_meta.m_is_key = false;
  col_meta.m_is_desc_key = false;
  col_meta.m_is_prefix_key = false;
  col_meta.m_is_fixed_len = true;
  col_meta.m_charset = nullptr;
  col_meta.m_field_index = field->field_index();

  col_meta.m_fixed_len_if_set_in_row = false;
  col_meta.m_fixed_len = field->pack_length_in_rec();
  col_meta.m_max_len = col_meta.m_fixed_len;

  col_meta.m_is_single_byte_len = (col_meta.m_fixed_len <= 255);

  auto type = field->type();

  if (((type == MYSQL_TYPE_STRING || type == MYSQL_TYPE_VARCHAR) &&
       col_meta.m_compare == Column_meta::Compare::MYSQL) ||
      col_meta.can_be_stored_externally()) {
    auto field_str = (const Field_str *)field;
    const CHARSET_INFO *field_charset = field_str->charset();
    col_meta.m_charset = static_cast<const void *>(field_charset);

    auto field_size = field->field_length;

    /* Fixed length for string datatype is in number of characters. This is
    because Innodb stores fixed length char fields as varchar if the length
    exceeds char length because of multi-byte characters. */
    col_meta.m_fixed_len = field->char_length();
    col_meta.m_is_fixed_len = false;
    col_meta.m_max_len = field_size;
  }

  if (type == MYSQL_TYPE_STRING) {
    /* If all columns are within the character size limit then the row is set
    to have fixed length for all character columns. */
    col_meta.m_fixed_len_if_set_in_row = true;
  }
  col_meta.m_null_byte = 0;
  col_meta.m_null_bit = 0;
}

static bool add_index_columns(TABLE_SHARE *table_share, const KEY &key,
                              Row_meta &row_meta, const KEY &pk,
                              bool &all_key_int, bool &all_key_int_signed_asc) {
  assert(table_share != nullptr);

  all_key_int = true;
  all_key_int_signed_asc = true;
  std::vector<bool> field_added(table_share->fields, false);

  auto &columns = row_meta.m_columns;

  /* The column index in the secondary index. */
  auto col_index = 0;

  for (size_t index = 0; index < key.actual_key_parts; ++index) {
    KEY_PART_INFO &key_part = key.key_part[index];
    auto key_field = key_part.field;

    Column_meta col_meta;
    col_meta.m_is_pk = false;
    fill_column_metadata(key_field, col_meta);

    /* The column index in the secondary index. */
    col_meta.m_index = col_index++;

    col_meta.m_is_key = true;
    col_meta.m_is_desc_key = key_part.key_part_flag & HA_REVERSE_SORT;

    if (!col_meta.is_integer()) {
      all_key_int = false;
    }

    if (col_meta.m_is_desc_key ||
        col_meta.m_compare != Column_meta::Compare::INTEGER_SIGNED) {
      all_key_int_signed_asc = false;
    }

    if (key_part.key_part_flag & HA_PART_KEY_SEG) {
      col_meta.m_max_len = key_part.length;
      col_meta.m_fixed_len = col_meta.m_max_len;

      auto type = key_field->type();
      if ((type == MYSQL_TYPE_STRING || type == MYSQL_TYPE_VARCHAR) &&
          col_meta.m_compare == Column_meta::Compare::MYSQL) {
        auto charset = key_field->charset();
        if (charset->mbmaxlen > 0) {
          col_meta.m_fixed_len = col_meta.m_max_len / charset->mbmaxlen;
        }
      }
      col_meta.m_is_prefix_key = true;
    }

    auto field_index = key_field->field_index();
    field_added[field_index] = true;
    col_meta.m_null_byte = col_meta.m_index / 8;
    col_meta.m_null_bit = col_meta.m_index % 8;
    columns.push_back(col_meta);
    row_meta.m_key_length += col_meta.m_fixed_len;
    row_meta.m_approx_row_len += col_meta.m_fixed_len;
  }

  if (table_share->is_missing_primary_key()) {
    row_meta.dbrowid_is_pk = true;
    Column_meta col_meta;
    col_meta.m_field_name = "DB_ROW_ID";
    col_meta.m_field_index = UINT16_MAX;
    col_meta.m_is_pk = false;
    col_meta.m_is_key = true;
    col_meta.m_is_prefix_key = false;
    col_meta.m_index = col_index++;
    col_meta.m_is_nullable = false;
    col_meta.m_is_unsigned = true;
    col_meta.m_type = MYSQL_TYPE_LONGLONG;
    col_meta.m_fixed_len = 8; /* Refer to DATA_ROW_ID_LEN */
    col_meta.m_max_len = 8;   /* Refer to DATA_ROW_ID_LEN */
    col_meta.m_compare = Column_meta::Compare::INTEGER_UNSIGNED;
    col_meta.m_is_desc_key = false;
    col_meta.m_null_byte = col_meta.m_index / 8;
    col_meta.m_null_bit = col_meta.m_index % 8;
    all_key_int_signed_asc = false;
    row_meta.m_approx_row_len += col_meta.m_fixed_len;
    columns.push_back(col_meta);
  } else {
    for (size_t index = 0; index < pk.user_defined_key_parts; ++index) {
      auto &key_part = pk.key_part[index];
      auto key_field = key_part.field;
      auto field_index = key_field->field_index();
      if (field_added[field_index]) {
        continue;
      }
      Column_meta col_meta;
      col_meta.m_is_pk = false;
      fill_column_metadata(key_field, col_meta);
      /* The column index in the secondary index. */
      col_meta.m_index = col_index++;

      row_meta.m_approx_row_len += col_meta.m_fixed_len;
      col_meta.m_is_key = true;

      col_meta.m_null_byte = col_meta.m_index / 8;
      col_meta.m_null_bit = col_meta.m_index % 8;

      assert(col_meta.m_null_byte < Row_header::MAX_NULLABLE_BYTES);

      if (col_meta.m_null_byte >= Row_header::MAX_NULLABLE_BYTES) {
        return false;
      }

      columns.push_back(col_meta);
      field_added[field_index] = true;
    }
  }
  return true;
}

/* have_key is true if sorting is needed (CSV data is unordered) */
bool get_row_metadata_for_pk(THD *thd [[maybe_unused]], const TABLE *table,
                             bool have_key, Row_meta &row_meta) {
  auto table_share = table->s;
  KEY *primary_key = nullptr;

  if (table_share->primary_key < table_share->keys) {
    primary_key = &table->key_info[table_share->primary_key];
  }

  row_meta.is_pk = true;

  if (primary_key == nullptr) {
    row_meta.m_name = "GEN_CLUST_INDEX";
    row_meta.dbrowid_is_pk = true;
    have_key = false; /* sorting should not be required. */
  } else {
    row_meta.m_name = primary_key->name;
    row_meta.dbrowid_is_pk = false;
    row_meta.m_keys = have_key ? primary_key->user_defined_key_parts : 0;
  }

  row_meta.m_bitmap_length = 0;
  row_meta.m_header_length = 0;

  row_meta.m_non_keys = 0;
  row_meta.m_key_length = 0;
  row_meta.m_key_type = Row_meta::Key_type::ANY;

  row_meta.m_num_columns =
      table_share->fields + (table_share->is_missing_primary_key() ? 1 : 0);
  row_meta.m_first_key_len = 0;

  std::vector<bool> field_added(table_share->fields, false);
  auto &columns = row_meta.m_columns;
  auto &columns_text_order = row_meta.m_columns_text_order;

  /* Just reserve 1 extra, which will be needed when DB_ROW_ID is added. */
  columns.reserve(table_share->fields + 1);
  columns_text_order.reserve(table_share->fields + 1);

  bool all_key_int_signed_asc = true;
  bool all_key_int = true;

  size_t col_index = 0;

  /* Add all key columns. */
  if (primary_key == nullptr) {
    /* The auto generated DB_ROW_ID is the pk. */
    Column_meta col_meta;
    col_meta.m_field_name = "DB_ROW_ID";
    col_meta.m_is_pk = true;
    col_meta.m_index = col_index++;
    col_meta.m_is_nullable = false;
    col_meta.m_is_unsigned = true;
    col_meta.m_type = MYSQL_TYPE_LONGLONG;
    col_meta.m_compare = Column_meta::Compare::INTEGER_UNSIGNED;
    col_meta.m_fixed_len = 8; /* Refer to DATA_ROW_ID_LEN */
    col_meta.m_max_len = 8;   /* Refer to DATA_ROW_ID_LEN */
    columns.push_back(col_meta);
    const auto last_index = columns.size() - 1;
    columns_text_order.push_back(&columns[last_index]);
  } else {
    for (size_t index = 0; index < row_meta.m_keys; ++index) {
      auto &key_part = primary_key->key_part[index];
      auto key_field = key_part.field;

      Column_meta col_meta;
      col_meta.m_is_pk = true;
      fill_column_metadata(key_field, col_meta);
      if (table_share->is_missing_primary_key()) {
        col_meta.m_index = col_index++;
      }
      col_meta.m_is_part_of_sk = is_field_part_of_sk(table, key_field);

      col_meta.m_is_key = true;
      col_meta.m_is_desc_key = key_part.key_part_flag & HA_REVERSE_SORT;

      if (!col_meta.is_integer()) {
        all_key_int = false;
      }

      if (col_meta.m_is_desc_key ||
          col_meta.m_compare != Column_meta::Compare::INTEGER_SIGNED) {
        all_key_int_signed_asc = false;
      }

      if (key_part.key_part_flag & HA_PART_KEY_SEG) {
        col_meta.m_max_len = key_part.length;
        col_meta.m_fixed_len = col_meta.m_max_len;

        auto type = key_field->type();
        if ((type == MYSQL_TYPE_STRING || type == MYSQL_TYPE_VARCHAR) &&
            col_meta.m_compare == Column_meta::Compare::MYSQL) {
          auto charset = key_field->charset();
          if (charset->mbmaxlen > 0) {
            col_meta.m_fixed_len = col_meta.m_max_len / charset->mbmaxlen;
          }
        }
        col_meta.m_is_prefix_key = true;

      } else {
        auto field_index = key_field->field_index();
        /* For non-prefix index the column doesn't need to be added again. */
        field_added[field_index] = true;
        col_meta.m_null_byte = field_index / 8;
        col_meta.m_null_bit = field_index % 8;
      }
      columns.push_back(col_meta);
      const auto last_index = columns.size() - 1;
      columns_text_order.push_back(&columns[last_index]);
      assert(columns.size() == columns_text_order.size());

      if (!col_meta.is_integer()) {
        row_meta.m_key_length += col_meta.m_fixed_len;
      }
      row_meta.m_approx_row_len += col_meta.m_fixed_len;
    }
  }

  if (have_key && all_key_int) {
    row_meta.m_key_type = all_key_int_signed_asc
                              ? Row_meta::Key_type::INT_SIGNED_ASC
                              : Row_meta::Key_type::INT;
  }

  /* Add other columns */
  for (size_t index = 0; index < table_share->fields; ++index) {
    auto field = table_share->field[index];

    if (field_added[index]) {
      continue;
    }

    Column_meta col_meta;
    col_meta.m_is_pk = true;
    fill_column_metadata(field, col_meta);
    if (table_share->is_missing_primary_key()) {
      col_meta.m_index = col_index++;
    }
    col_meta.m_is_part_of_sk = is_field_part_of_sk(table, field);
    row_meta.m_approx_row_len += col_meta.m_fixed_len;

    col_meta.m_null_byte = index / 8;
    col_meta.m_null_bit = index % 8;

    assert(col_meta.m_null_byte < Row_header::MAX_NULLABLE_BYTES);

    if (col_meta.m_null_byte >= Row_header::MAX_NULLABLE_BYTES) {
      return false;
    }

    columns.push_back(col_meta);
    const auto last_index = columns.size() - 1;
    columns_text_order.push_back(&columns[last_index]);
    assert(columns.size() == columns_text_order.size());

    field_added[index] = true;
    ++row_meta.m_non_keys;
  }

  assert(columns_text_order.size() == columns.size());

  std::sort(
      columns_text_order.begin(), columns_text_order.end(),
      [](const auto &p1, const auto &p2) { return p1->m_index < p2->m_index; });

  row_meta.m_n_blob_cols = std::count_if(
      columns_text_order.begin(), columns_text_order.end(),
      [](const auto &p) { return p->can_be_stored_externally(); });

#ifndef NDEBUG
  if (table_share->is_missing_primary_key()) {
    assert(columns.size() <= table_share->fields + 1);
    assert(columns_text_order.size() <= table_share->fields + 1);
  } else {
    assert(columns.size() <= table_share->fields);
    assert(columns_text_order.size() <= table_share->fields);
  }
#endif /* NDEBUG */

  /* Calculate NULL bitmap length. */
  if (have_key) {
    auto bitmap_size = (row_meta.m_num_columns / 8);

    if (row_meta.m_num_columns % 8 > 0) {
      ++bitmap_size;
    }
    assert(bitmap_size <= Row_header::MAX_NULLABLE_BYTES);
    if (bitmap_size > Row_header::MAX_NULLABLE_BYTES) {
      return false;
    }
    row_meta.m_bitmap_length = bitmap_size;

    Row_header header(row_meta);
    row_meta.m_header_length = header.header_length();

    auto &first_key_col = columns[0];

    if (!first_key_col.m_is_fixed_len) {
      row_meta.m_first_key_len = first_key_col.m_is_single_byte_len ? 1 : 2;
    }
  }
  row_meta.m_approx_row_len += row_meta.m_header_length;
  return true;
}

bool get_row_metadata_for_sk(THD *thd [[maybe_unused]], const TABLE *table,
                             size_t keynr, Row_meta &row_meta) {
  assert(table != nullptr);
  auto table_share = table->s;
  assert(table_share->primary_key < table_share->keys ||
         table_share->is_missing_primary_key());

  const KEY &pk = table->key_info[table_share->primary_key];

  const size_t pk_key_parts =
      table_share->is_missing_primary_key() ? 1 : pk.user_defined_key_parts;

  bool all_key_int = true;
  bool all_key_int_signed_asc = true;
  const KEY &key = table->key_info[keynr];
  auto &columns = row_meta.m_columns;
  row_meta.is_pk = false;
  row_meta.m_key_length = 0;
  row_meta.m_name = key.name;
  row_meta.m_bitmap_length = 0;
  row_meta.m_header_length = 0;
  row_meta.m_keys = key.user_defined_key_parts + pk_key_parts;
  row_meta.m_non_keys = 0;

  if (!add_index_columns(table_share, key, row_meta, pk, all_key_int,
                         all_key_int_signed_asc)) {
    return false;
  }
  row_meta.m_num_columns = columns.size();
  row_meta.m_keys = columns.size();

  if (all_key_int) {
    row_meta.m_key_type = all_key_int_signed_asc
                              ? Row_meta::Key_type::INT_SIGNED_ASC
                              : Row_meta::Key_type::INT;
  } else {
    row_meta.m_key_type = Row_meta::Key_type::ANY;
  }
  auto bitmap_size = (row_meta.m_num_columns / 8);

  if (row_meta.m_num_columns % 8 > 0) {
    ++bitmap_size;
  }
  assert(bitmap_size <= Row_header::MAX_NULLABLE_BYTES);
  if (bitmap_size > Row_header::MAX_NULLABLE_BYTES) {
    return false;
  }
  row_meta.m_bitmap_length = bitmap_size;

  Row_header header(row_meta);
  row_meta.m_header_length = header.header_length();

  auto &first_key_col = columns[0];

  if (!first_key_col.m_is_fixed_len) {
    row_meta.m_first_key_len = first_key_col.m_is_single_byte_len ? 1 : 2;
  }
  return true;
}

DEFINE_METHOD(bool, get_table_metadata,
              (THD * thd [[maybe_unused]], const TABLE *table,
               Table_meta &table_meta)) {
  auto table_share = table->s;
  table_meta.m_n_keys = table_share->keys;
  table_meta.m_keynr_pk = table_share->primary_key;

  table_meta.m_table_name =
      std::string(table_share->table_name.str, table_share->table_name.length);

  if (table_share->is_missing_primary_key()) {
    table_meta.m_n_keys++;
    table_meta.m_keynr_pk = 0;
    table_meta.dbrowid_is_pk = true;
    table->file->bulk_load_get_row_id_range(table_meta.min_row_id_value,
                                            table_meta.max_row_id_value);
  }

  return true;
}

DEFINE_METHOD(bool, get_row_metadata_all,
              (THD * thd, const TABLE *table, bool have_key [[maybe_unused]],
               std::vector<Row_meta> &row_meta_all)) {
  assert(row_meta_all.size() == 0);
  assert(table != nullptr);
  auto table_share = table->s;
  assert(table_share->primary_key < table_share->keys ||
         table_share->is_missing_primary_key());
  row_meta_all.reserve(table_share->keys);
  Row_meta default_row_meta;

  bool success{true};

  if (table_share->is_missing_primary_key()) {
    // There is no explicit primary key
    row_meta_all.push_back(default_row_meta);
    Row_meta &row_meta = row_meta_all.back();
    row_meta.dbrowid_is_pk = true;
    success = get_row_metadata_for_pk(thd, table, have_key, row_meta);
  }

  for (size_t keynr = 0; success && keynr < table_share->keys; ++keynr) {
    row_meta_all.push_back(default_row_meta);
    Row_meta &row_meta = row_meta_all.back();
    if (keynr == table_share->primary_key) {
      assert(!table_share->is_missing_primary_key());
      success = get_row_metadata_for_pk(thd, table, have_key, row_meta);
    } else {
      success = get_row_metadata_for_sk(thd, table, keynr, row_meta);
    }
  }

#ifndef NDEBUG
  for (const auto &row_meta : row_meta_all) {
    for (const auto &col_meta : row_meta.m_columns) {
      assert(col_meta.m_index < row_meta.m_num_columns);
    }
  }
#endif /* NDEBUG */

  return success;
}

}  // namespace Bulk_data_convert

namespace Bulk_data_load {

using Blob_context = void *;

DEFINE_METHOD(void *, begin,
              (THD * thd, const TABLE *table, size_t keynr, size_t data_size,
               size_t memory, size_t num_threads)) {
  auto ctx =
      table->file->bulk_load_begin(thd, keynr, data_size, memory, num_threads);
  return ctx;
}

DEFINE_METHOD(bool, load,
              (THD * thd, void *ctx, const TABLE *table,
               const Rows_mysql &sql_rows, size_t thread,
               Bulk_load::Stat_callbacks &wait_cbks)) {
  int err =
      table->file->bulk_load_execute(thd, ctx, thread, sql_rows, wait_cbks);
  return (err == 0);
}

DEFINE_METHOD(bool, open_blob,
              (THD * thd, void *load_ctx, const TABLE *table,
               Blob_context &blob_ctx, unsigned char *blobref, size_t thread)) {
  assert(load_ctx != nullptr);

  int err = table->file->open_blob(thd, load_ctx, thread, blob_ctx, blobref);
  return (err == 0);
}

DEFINE_METHOD(bool, write_blob,
              (THD * thd, void *load_ctx, const TABLE *table,
               Blob_context blob_ctx, unsigned char *blobref, size_t thread,
               const unsigned char *data, size_t data_len)) {
  int err = table->file->write_blob(thd, load_ctx, thread, blob_ctx, blobref,
                                    data, data_len);
  return (err == 0);
}

DEFINE_METHOD(bool, close_blob,
              (THD * thd, void *load_ctx, const TABLE *table,
               Blob_context blob_ctx, unsigned char *blobref, size_t thread)) {
  int err = table->file->close_blob(thd, load_ctx, thread, blob_ctx, blobref);
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
    err_strm << "LOAD DATA ALGORITHM = BULK doesn't support fixed size FLOAT"
                " columns, they are deprecated. Please use DECIMAL type.";
    LogErr(INFORMATION_LEVEL, ER_BULK_LOADER_INFO, err_strm.str().c_str());
    my_error(ER_FEATURE_UNSUPPORTED, MYF(0),
             "fixed size FLOAT column (deprecated)",
             "LOAD DATA ALGORITHM = BULK");
    return false;
  } else if (field->is_unsigned()) {
    std::ostringstream err_strm;
    err_strm << "LOAD DATA ALGORITHM = BULK doesn't support UNSIGNED FLOAT"
                " columns, they are deprecated.";
    LogErr(INFORMATION_LEVEL, ER_BULK_LOADER_INFO, err_strm.str().c_str());
    my_error(ER_FEATURE_UNSUPPORTED, MYF(0),
             "UNSIGNED FLOAT column (deprecated)",
             "LOAD DATA ALGORITHM = BULK");
    return false;
  }
  return true;
}

bool check_for_deprecated_use(Field_double *field) {
  if (!field->not_fixed) {
    std::ostringstream err_strm;
    err_strm << "LOAD DATA ALGORITHM = BULK doesn't support fixed size DOUBLE"
                " columns, they are deprecated. Please use DECIMAL type.";
    LogErr(INFORMATION_LEVEL, ER_BULK_LOADER_INFO, err_strm.str().c_str());
    my_error(ER_FEATURE_UNSUPPORTED, MYF(0),
             "fixed size DOUBLE column (deprecated)",
             "LOAD DATA ALGORITHM = BULK");
    return false;
  } else if (field->is_unsigned()) {
    std::ostringstream err_strm;
    err_strm << "LOAD DATA ALGORITHM = BULK doesn't support UNSIGNED DOUBLE"
                " columns, they are deprecated.";
    LogErr(INFORMATION_LEVEL, ER_BULK_LOADER_INFO, err_strm.str().c_str());
    my_error(ER_FEATURE_UNSUPPORTED, MYF(0),
             "UNSIGNED DOUBLE column (deprecated)",
             "LOAD DATA ALGORITHM = BULK");
    return false;
  }
  return true;
}

bool check_for_deprecated_use(Field_new_decimal *field) {
  if (field->is_unsigned()) {
    std::ostringstream err_strm;
    err_strm << "LOAD DATA ALGORITHM = BULK doesn't support UNSIGNED DECIMAL"
                "columns.";
    LogErr(INFORMATION_LEVEL, ER_BULK_LOADER_INFO, err_strm.str().c_str());
    my_error(ER_FEATURE_UNSUPPORTED, MYF(0),
             "UNSIGNED DECIMAL column (deprecated)",
             "LOAD DATA ALGORITHM = BULK");
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

DEFINE_METHOD(size_t, get_se_memory_size, (THD * thd, const TABLE *table)) {
  return table->file->bulk_load_available_memory(thd);
}

DEFINE_METHOD(bool, copy_existing_data,
              (void *ctx, const TABLE *duplicate_table, size_t thread,
               Bulk_load::Stat_callbacks &wait_cbks)) {
  int err = duplicate_table->file->bulk_load_copy_existing_data(ctx, thread,
                                                                wait_cbks);
  return err == 0;
}

DEFINE_METHOD(
    bool, set_source_table_data,
    (void *ctx, const TABLE *duplicate_table,
     const std::vector<Bulk_load::Source_table_data> &source_table_data)) {
  return duplicate_table->file->bulk_load_set_source_table_data(
      ctx, source_table_data);
}

DEFINE_METHOD(bool, is_table_supported, (THD * thd, const TABLE *table)) {
  auto share = table->s;

  if (table_has_generated_invisible_primary_key(table)) {
    std::ostringstream err_strm;
    err_strm << "LOAD DATA ALGORITHM = BULK not supported for tables with"
                " generated invisible primary key.";
    LogErr(INFORMATION_LEVEL, ER_BULK_LOADER_INFO, err_strm.str().c_str());
    my_error(ER_FEATURE_UNSUPPORTED, MYF(0), "GENERATED/INVISIBLE PRIMARY KEY",
             "LOAD DATA ALGORITHM = BULK");
    return false;
  }

  if (table->triggers != nullptr) {
    std::ostringstream err_strm;
    err_strm << "LOAD DATA ALGORITHM = BULK not supported for tables with"
                " triggers.";
    LogErr(INFORMATION_LEVEL, ER_BULK_LOADER_INFO, err_strm.str().c_str());
    my_error(ER_FEATURE_UNSUPPORTED, MYF(0), "TRIGGER",
             "LOAD DATA ALGORITHM = BULK");
    return false;
  }

  if (table->table_check_constraint_list != nullptr) {
    std::ostringstream err_strm;
    err_strm << "LOAD DATA ALGORITHM = BULK not supported for tables with"
                "CHECK constraints.";
    LogErr(INFORMATION_LEVEL, ER_BULK_LOADER_INFO, err_strm.str().c_str());
    my_error(ER_FEATURE_UNSUPPORTED, MYF(0), "CHECK constraint",
             "LOAD DATA ALGORITHM = BULK");
    return false;
  }

  for (size_t keynr = 0; keynr < share->keys; ++keynr) {
    const auto &key = table->key_info[keynr];

    if (key.algorithm == HA_KEY_ALG_RTREE) {
      my_error(ER_FEATURE_UNSUPPORTED, MYF(0), "Spatial Index",
               "LOAD DATA ALGORITHM = BULK");
      return false;
    }

    if (key.is_functional_index()) {
      my_error(ER_FEATURE_UNSUPPORTED, MYF(0), "Functional Index",
               "LOAD DATA ALGORITHM = BULK");
      return false;
    }
  }

  for (size_t keynr = 0; keynr < share->keys; ++keynr) {
    const auto &key = table->key_info[keynr];

    for (size_t ind = 0; ind < key.user_defined_key_parts; ++ind) {
      auto &key_part = key.key_part[ind];
      if (key_part.key_part_flag & HA_PART_KEY_SEG) {
        std::ostringstream err_strm;
        err_strm << "LOAD DATA ALGORITHM = BULK not supported for tables with "
                    "Prefix Key";

        LogErr(INFORMATION_LEVEL, ER_BULK_LOADER_INFO, err_strm.str().c_str());

        my_error(ER_FEATURE_UNSUPPORTED, MYF(0), "Prefix Key",
                 "LOAD DATA ALGORITHM = BULK");
        return false;
      }
    }
  }

#ifdef MYSQL_AI
  size_t n_vector_fields = 0;
#endif /* MYSQL_AI */

  for (size_t index = 0; index < share->fields; ++index) {
    auto field = share->field[index];

    switch (field->real_type()) {
      case MYSQL_TYPE_VECTOR:
#ifdef MYSQL_AI
        ++n_vector_fields;
        [[fallthrough]];
#endif /* MYSQL_AI */
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
      case MYSQL_TYPE_DATETIME2:
      case MYSQL_TYPE_NEWDATE:
      case MYSQL_TYPE_TIME2:
      case MYSQL_TYPE_BLOB:
      case MYSQL_TYPE_JSON:
      case MYSQL_TYPE_GEOMETRY:
      case MYSQL_TYPE_YEAR:
      case MYSQL_TYPE_BIT:
      case MYSQL_TYPE_TIMESTAMP2:
      case MYSQL_TYPE_ENUM:
      case MYSQL_TYPE_SET:
        if (!check_for_deprecated_use(field)) {
          return false;
        }
        continue;
      default:
        std::ostringstream log_strm;
        String type_string(64);
        field->sql_type(type_string);
        log_strm << "LOAD DATA ALGORITHM = BULK not supported for data type: "
                 << type_string.c_ptr_safe();
        LogErr(INFORMATION_LEVEL, ER_BULK_LOADER_INFO, log_strm.str().c_str());

        std::ostringstream err_strm;
        err_strm << type_string.c_ptr_safe() << " column type";
        my_error(ER_FEATURE_UNSUPPORTED, MYF(0), err_strm.str().c_str(),
                 "LOAD DATA ALGORITHM = BULK");
        return false;
    }
  }

  if (share->has_secondary_engine()) {
    my_error(ER_BULK_LOAD_SECONDARY_ENGINE, MYF(0));
    std::ostringstream err_strm;
    err_strm << "LOAD DATA ALGORITHM = BULK not supported for tables with "
                "Secondary Engine";
    LogErr(INFORMATION_LEVEL, ER_BULK_LOADER_INFO, err_strm.str().c_str());
    return false;
  }

#ifdef MYSQL_AI
  if (n_vector_fields == 0) {
    LogErr(INFORMATION_LEVEL, ER_BULK_LOADER_INFO,
           "LOAD DATA ALGORITHM=BULK not supported for tables without VECTOR "
           "columns");
    my_error(ER_FEATURE_UNSUPPORTED, MYF(0), "Table without VECTOR columns",
             "LOAD DATA ALGORITHM=BULK");
    return false;
  }
#endif /* MYSQL_AI */

  if (!table->file->bulk_load_check(thd)) {
    /* Innodb already raises the error. */
    return false;
  }
  return true;
}

}  // namespace Bulk_data_load

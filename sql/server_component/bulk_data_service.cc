/*  Copyright (c) 2022, 2024, Oracle and/or its affiliates.

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
#include "field_types.h"
#include "my_byteorder.h"
#include "my_time.h"
#include "mysql/components/service_implementation.h"
#include "mysql/components/services/log_builtins.h"
#include "mysql_time.h"
#include "mysqld_error.h"
#include "sql-common/my_decimal.h"
#include "sql/field.h"
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

  /* Write the integer bytes in the buffer. */
  if (write_in_buffer) {
    /* This is written to temp files to be consumed later part of execution.
    We don't bother about BE/LE order here. */
    if (sql_col.m_type == MYSQL_TYPE_LONGLONG) {
      memcpy(sql_col.m_data_ptr, (void *)(&sql_col.m_int_data),
             sizeof(uint64_t));
      sql_col.m_data_len = sizeof(uint64_t);
      return 0;
    }

    /* Unsigned integer less than or equal to four bytes. */
    if (is_unsigned) {
      /* Data is already checked to be within the range of S. */
      auto data_4 = static_cast<uint32_t>(sql_col.m_int_data);

      memcpy(sql_col.m_data_ptr, (void *)(&data_4), sizeof(uint32_t));
      sql_col.m_data_len = sizeof(uint32_t);
      return 0;
    }

    /* Signed integer less than or equal to four bytes. */
    auto signed_val = static_cast<int64_t>(sql_col.m_int_data);
    /* Data is already checked to be within the range of S. */
    auto data_4 = static_cast<int32_t>(signed_val);

    memcpy(sql_col.m_data_ptr, (void *)(&data_4), sizeof(int32_t));
    sql_col.m_data_len = sizeof(int32_t);
  }
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

  char *field_begin = sql_col.m_data_ptr;
  char *field_data = field_begin + length_size;

  const char *error_pos = nullptr;
  const char *convert_error_pos = nullptr;
  const char *end_pos = nullptr;

  size_t copy_size = well_formed_copy_nchars(
      field_charset, field_data, field_size, charset, text_col.m_data_ptr,
      text_col.m_data_len, field_char_size, &error_pos, &convert_error_pos,
      &end_pos);

  if (end_pos < text_col.m_data_ptr + text_col.m_data_len) {
    /* The error is expected when fixed_length = true, where we try to adjust
    the data within character length limit. The data could not be fit in
    such limit here which is possible for multi-byte character set. We
    return from here and retry with variable length format - mysql_format() */
    if (fixed_length && single_byte) {
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

  auto data_length = copy_size;

  /* For char[] column need to fill padding characters. */
  if (fixed_length && copy_size < field_size) {
    size_t fill_size = field_size - copy_size;
    char *fill_pos = field_data + copy_size;

    field_charset->cset->fill(field_charset, fill_pos, fill_size,
                              field_charset->pad_char);
    data_length = field_size;
  }

  sql_col.m_data_ptr = field_data;
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

  float4store((uchar *)sql_col.m_data_ptr, nr);

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

  if (field_double->truncate(&nr, FLT_MAX) != Field_real::TR_OK) {
    error_details.column_type = "double";
    log_conversion_error(text_col, "Invalid value for type: ");
    return ER_LOAD_BULK_DATA_WRONG_VALUE_FOR_FIELD;
  }

  float8store((uchar *)sql_col.m_data_ptr, nr);

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

/** Create a DATETIME column converting data to MySQL storage format.
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

/** Create a DATE column converting data to MySQL storage format.
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
  assert(!col_meta.m_is_key);

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
@return error code. */
static int format_row(THD *thd, const TABLE_SHARE *table_share,
                      const Rows_text &text_rows, size_t text_row_index,
                      char *&buffer, size_t &buffer_length,
                      const CHARSET_INFO *charset, const Row_meta &metadata,
                      Rows_mysql &sql_rows, size_t sql_row_index,
                      bool single_byte_char, bool &completed,
                      Bulk_load_error_location_details &error_details) {
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
    auto text_index = col_meta.m_index;

    assert(text_index < table_share->fields);
    auto field = table_share->field[text_index];

    auto &text_col = text_rows.read_column(text_row_offset, text_index);

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

    sql_col.m_data_ptr = buffer;
    sql_col.m_data_len = field_size;
    sql_col.m_int_data = 0;
    sql_col.m_type = static_cast<int>(field->type());
    sql_col.m_is_null = text_col.m_data_ptr == nullptr;

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
    switch (field->type()) {
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
      case MYSQL_TYPE_STRING:
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
@param[in,out] buffer         input raw data buffer
@param[in]     buffer_length  buffer length
@param[in]     col_meta       column metadata
@param[in]     header         row header
@param[in]     marked_fixed   if the row is marked as fixed length
@param[out]    col_length     column length
@param[out]    sql_col        column data
@return error code. */
static int fill_column_data(char *buffer, size_t buffer_length,
                            const Column_meta &col_meta,
                            const Row_header &header, bool marked_fixed,
                            size_t &col_length, Column_mysql &sql_col) {
  sql_col.m_type = col_meta.m_type;
  sql_col.m_is_null =
      col_meta.m_is_key ? false : header.is_column_null(col_meta);
  sql_col.m_int_data = 0;
  sql_col.m_data_ptr = nullptr;
  sql_col.m_data_len = 0;
  col_length = 0;

  assert(!col_meta.m_is_key || !header.is_column_null(col_meta));

  if (sql_col.m_is_null) {
    return 0;
  }
  /* Check format_int_column() case write_in_buffer. */
  if (col_meta.is_integer()) {
    sql_col.m_data_ptr = buffer;

    if (sql_col.m_type == MYSQL_TYPE_LONGLONG) {
      sql_col.m_data_len = sizeof(uint64_t);
      assert(sql_col.m_data_len <= buffer_length);

      if (buffer_length < sql_col.m_data_len) {
        return ER_DATA_OUT_OF_RANGE;
      }
      col_length = sql_col.m_data_len;

      memcpy((void *)(&sql_col.m_int_data), sql_col.m_data_ptr,
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
      memcpy((void *)(&data_4), sql_col.m_data_ptr, sizeof(uint32_t));
      sql_col.m_int_data = static_cast<uint64_t>(data_4);

      return 0;
    }

    /* Signed integer less than or equal to four bytes. */
    int32_t data_4 = 0;
    memcpy((void *)(&data_4), sql_col.m_data_ptr, sizeof(int32_t));

    auto signed_val = static_cast<int64_t>(data_4);
    sql_col.m_int_data = static_cast<uint64_t>(signed_val);

    return 0;
  }

  /* For non-key, fixed length char data adjusted within single byte length, we
  skip writing length byte(s). In such case, row header is marked to indicate
  that length bytes are not present for fixed length types. This added
  added complexity helps in saving temp storage size for fixed length char. */
  bool no_length_char =
      marked_fixed && col_meta.m_fixed_len_if_set_in_row && !col_meta.m_is_key;

  if (col_meta.m_is_fixed_len || no_length_char) {
    sql_col.m_data_len = col_meta.m_fixed_len;
    sql_col.m_data_ptr = buffer;
    col_length = sql_col.m_data_len;

    assert(col_length <= buffer_length);
    return col_length > buffer_length ? ER_DATA_OUT_OF_RANGE : 0;
  }

  /* Variable length data. */
  size_t len_size = col_meta.m_is_single_byte_len ? 1 : 2;
  sql_col.m_data_ptr = buffer + len_size;

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
@param[in]     fill_keys      true if keys to be filled otherwise the entire row
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
    auto err = fill_column_data(buffer, buffer_length, col_meta, header,
                                fixed_length, consumed_length, sql_col);
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
  const auto &first_key = sql_keys.read_column(key_offset, 0);
  char *buffer = first_key.m_data_ptr - metadata.m_first_key_len;
  buffer -= metadata.m_header_length;

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

  for (sql_index = start_index; sql_index < max_index; ++sql_index) {
    size_t row_length = 0;
    err = fill_row_data(buffer, buffer_length, true, metadata, header, sql_rows,
                        sql_index, row_length);
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

  auto data_uptr1 = reinterpret_cast<const uchar *>(key1.m_data_ptr);
  auto data_uptr2 = reinterpret_cast<const uchar *>(key2.m_data_ptr);

  switch (type) {
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
        l1 = cs->cset->lengthsp(cs, key1.m_data_ptr, l1);
        l2 = cs->cset->lengthsp(cs, key2.m_data_ptr, l2);
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

  auto type = field->type();
  col_meta.m_type = static_cast<int>(type);

  switch (type) {
    case MYSQL_TYPE_TINY:
    case MYSQL_TYPE_SHORT:
    case MYSQL_TYPE_INT24:
    case MYSQL_TYPE_LONG:
    case MYSQL_TYPE_LONGLONG:
      col_meta.m_compare = col_meta.m_is_unsigned
                               ? Column_meta::Compare::INTEGER_UNSIGNED
                               : Column_meta::Compare::INTEGER_SIGNED;
      break;
    case MYSQL_TYPE_NEWDECIMAL:
    case MYSQL_TYPE_DATETIME:
    case MYSQL_TYPE_DATE:
    case MYSQL_TYPE_TIME:
      col_meta.m_compare = Column_meta::Compare::BINARY;
      break;
    default:
      assert(type == MYSQL_TYPE_STRING || type == MYSQL_TYPE_VARCHAR ||
             type == MYSQL_TYPE_FLOAT || type == MYSQL_TYPE_DOUBLE);
      col_meta.m_compare = Column_meta::Compare::MYSQL;
      break;
  }
}

/** Fill column metadata from mysql field structure.
@param[in]  field     MySQL field from TABLE
@param[out] col_meta  column metadata object to fill */
static void fill_column_metadata(const Field *field, Column_meta &col_meta) {
  set_data_type(field, col_meta);

  col_meta.m_is_key = false;
  col_meta.m_is_desc_key = false;
  col_meta.m_is_prefix_key = false;
  col_meta.m_is_fixed_len = true;
  col_meta.m_charset = nullptr;

  col_meta.m_fixed_len_if_set_in_row = false;
  col_meta.m_fixed_len = field->pack_length_in_rec();
  col_meta.m_max_len = col_meta.m_fixed_len;

  col_meta.m_is_single_byte_len = (col_meta.m_fixed_len <= 255);

  auto type = field->type();

  if (type == MYSQL_TYPE_STRING || type == MYSQL_TYPE_VARCHAR) {
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

DEFINE_METHOD(bool, get_row_metadata,
              (THD *, const TABLE *table, bool have_key, Row_meta &metadata)) {
  auto table_share = table->s;

  if (table_share->keys < 1 || table_share->primary_key >= table_share->keys) {
    return false;
  }

  const auto &primary_key = table->key_info[table_share->primary_key];

  metadata.m_bitmap_length = 0;
  metadata.m_header_length = 0;
  metadata.m_keys = have_key ? primary_key.user_defined_key_parts : 0;
  metadata.m_non_keys = 0;
  metadata.m_key_length = 0;
  metadata.m_key_type = Row_meta::Key_type::ANY;
  metadata.m_num_columns = table_share->fields;
  metadata.m_first_key_len = 0;

  std::vector<bool> field_added(table_share->fields, false);
  auto &columns = metadata.m_columns;

  bool all_key_int_signed_asc = true;
  bool all_key_int = true;

  /* Add all key columns. */
  for (size_t index = 0; index < metadata.m_keys; ++index) {
    auto &key_part = primary_key.key_part[index];
    auto key_field = key_part.field;

    Column_meta col_meta;
    fill_column_metadata(key_field, col_meta);

    col_meta.m_is_key = true;
    col_meta.m_is_desc_key = key_part.key_part_flag & HA_REVERSE_SORT;
    col_meta.m_is_nullable = false;

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
      if (type == MYSQL_TYPE_STRING || type == MYSQL_TYPE_VARCHAR) {
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

    if (!col_meta.is_integer()) {
      metadata.m_key_length += col_meta.m_fixed_len;
    }
    metadata.m_approx_row_len += col_meta.m_fixed_len;
  }

  if (have_key && all_key_int) {
    metadata.m_key_type = all_key_int_signed_asc
                              ? Row_meta::Key_type::INT_SIGNED_ASC
                              : Row_meta::Key_type::INT;
  }

  /* Add other columns */
  for (size_t index = 0; index < table_share->fields; ++index) {
    auto field = table_share->field[index];

    if (field->is_gcol()) {
      return false;
    }

    if (field_added[index]) {
      continue;
    }

    Column_meta col_meta;
    fill_column_metadata(field, col_meta);
    metadata.m_approx_row_len += col_meta.m_fixed_len;

    col_meta.m_null_byte = index / 8;
    col_meta.m_null_bit = index % 8;

    assert(col_meta.m_null_byte < Row_header::MAX_NULLABLE_BYTES);

    if (col_meta.m_null_byte >= Row_header::MAX_NULLABLE_BYTES) {
      return false;
    }

    columns.push_back(col_meta);

    field_added[index] = true;
    ++metadata.m_non_keys;
  }

  /* Calculate NULL bitmap length. */
  if (have_key) {
    auto bitmap_size = (metadata.m_num_columns / 8);

    if (metadata.m_num_columns % 8 > 0) {
      ++bitmap_size;
    }
    assert(bitmap_size <= Row_header::MAX_NULLABLE_BYTES);
    if (bitmap_size > Row_header::MAX_NULLABLE_BYTES) {
      return false;
    }
    metadata.m_bitmap_length = bitmap_size;

    Row_header header(metadata);
    metadata.m_header_length = header.header_length();

    auto &first_key_col = columns[0];

    if (!first_key_col.m_is_fixed_len) {
      metadata.m_first_key_len = first_key_col.m_is_single_byte_len ? 1 : 2;
    }
  }
  metadata.m_approx_row_len += metadata.m_header_length;
  return true;
}

}  // namespace Bulk_data_convert

namespace Bulk_data_load {

DEFINE_METHOD(void *, begin,
              (THD * thd, const TABLE *table, size_t data_size, size_t memory,
               size_t num_threads)) {
  auto ctx = table->file->bulk_load_begin(thd, data_size, memory, num_threads);
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

DEFINE_METHOD(bool, is_table_supported, (THD * thd, const TABLE *table)) {
  auto share = table->s;

  if (share->keys < 1 || share->primary_key == MAX_KEY) {
    std::ostringstream err_strm;
    err_strm << "LOAD DATA ALGORITHM = BULK not supported for tables without"
                " PRIMARY KEY.";
    LogErr(INFORMATION_LEVEL, ER_BULK_LOADER_INFO, err_strm.str().c_str());
    my_error(ER_TABLE_NO_PRIMARY_KEY, MYF(0), table->alias);
    return false;
  }

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

  for (size_t index = 0; index < share->fields; ++index) {
    auto field = share->field[index];
    if (field->is_gcol()) {
      std::ostringstream err_strm;
      err_strm << "LOAD DATA ALGORITHM = BULK not supported for tables with "
                  "generated columns.";
      LogErr(INFORMATION_LEVEL, ER_BULK_LOADER_INFO, err_strm.str().c_str());
      my_error(ER_FEATURE_UNSUPPORTED, MYF(0), "GENERATED columns",
               "LOAD DATA ALGORITHM = BULK");
      return false;
    }

    const auto &primary_key = table->key_info[share->primary_key];

    /* TODO: Support Prefix Key in Innodb load and sorting.*/
    for (size_t ind = 0; ind < primary_key.user_defined_key_parts; ++ind) {
      auto &key_part = primary_key.key_part[ind];
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

    switch (field->real_type()) {
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

  if (!table->file->bulk_load_check(thd)) {
    /* Innodb already raises the error. */
    return false;
  }
  return true;
}

}  // namespace Bulk_data_load

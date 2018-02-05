/*
 * Copyright (c) 2015, 2018, Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is also distributed with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have included with MySQL.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */

#ifndef X_CLIENT_MYSQLXCLIENT_XROW_H_
#define X_CLIENT_MYSQLXCLIENT_XROW_H_

#include <cstdint>
#include <memory>
#include <set>
#include <string>

#include "mysqlxclient/xdatetime.h"
#include "mysqlxclient/xdecimal.h"
#include "mysqlxclient/xmessage.h"

namespace xcl {

/**
  Namespace containing functions to decode "fields" placed in Row message.
 */
namespace row_decoder {

using Row_str = const char *;
using Row_set = std::set<std::string>;

bool buffer_to_u64(const std::string &buffer, uint64_t *out_result);
bool buffer_to_s64(const std::string &buffer, int64_t *out_result);
bool buffer_to_float(const std::string &buffer, float *out_result);
bool buffer_to_double(const std::string &buffer, double *out_result);
bool buffer_to_time(const std::string &buffer, Time *out_time);
bool buffer_to_decimal(const std::string &buffer, Decimal *out_result);
bool buffer_to_set(const std::string &buffer, Row_set *out_result);
bool buffer_to_datetime(const std::string &buffer, DateTime *out_result,
                        const bool has_time);
bool buffer_to_string_set(const std::string &buffer, std::string *out_result);
bool buffer_to_string(const std::string &buffer, Row_str *out_result,
                      size_t *rlength);

}  // namespace row_decoder

/**
  Column types supported by the client library
*/
enum class Column_type {
  SINT,
  UINT,
  DOUBLE,
  FLOAT,
  BYTES,
  TIME,
  DATETIME,
  SET,
  ENUM,
  BIT,
  DECIMAL
};

/**
  Structure holding column information.

  This structure is a compact version of "Mysqlx::Resultset::ColumnMetaData".
*/
struct Column_metadata {
  Column_type type;
  std::string name;
  std::string original_name;
  std::string table;
  std::string original_table;
  std::string schema;
  std::string catalog;

  bool has_content_type;
  uint64_t collation;
  uint32_t fractional_digits;
  uint32_t length;
  uint32_t flags;
  uint32_t content_type;
};

/**
  Interface wrapping "::Mysqlx::Resultset::Row" message.

  "Row" messages holds fields as row data, which must be
  converted to "C++" types using decoders present in
  "xcl::row_decoder" namespace. The interface encapsulates
  "Row" messages and "row_decoder" functions to make
  easy in use interface.
*/
class XRow {
 public:
  /** Alias for Row protobuf message. */
  using Row = ::Mysqlx::Resultset::Row;
  /** Alias for set of strings used for MySQL "SET" type. */
  using String_set = std::set<std::string>;

 public:
  virtual ~XRow() = default;

  /** Validate the data placed in XRow. */
  virtual bool valid() const = 0;

  /** Get number of fields in row. */
  virtual int32_t get_number_of_fields() const = 0;

  /**
    Check field if its empty.

    @param field_index    index of the field/column to check

    @return Result of accessing the data
      @retval == true     field contains value
      @retval == false     field is empty
  */
  virtual bool is_null(const int32_t field_index) const = 0;

  /**
    Get field data as int64_t value.

    Method validates if column type stored in Column_metadata is equal
    to "Column_type::SINT" also if the field "raw" data contains enough
    bytes for conversion to int64. Getter is going to fail when one of
    those checks fails.

    @param field_index    index of the field/column to get
    @param[out] out_data  int32_t result. "null" pointer is accepted,
                          conversion is done nevertheless but returning
                          the result is omitted.

    @return Result of accessing the data
      @retval == true     OK
      @retval == false    getter failed
  */
  virtual bool get_int64(const int32_t field_index,
                         int64_t *out_data) const = 0;

  /**
    Get field data as uint64_t value.

    Method validates if column type stored in Column_metadata is equal
    to "Column_type::UINT" also if the field "raw" data contains enough
    bytes for conversion to uint64. Getter is going to fail when one of
    those checks fails.

    @param field_index    index of the field/column to get
    @param[out] out_data  uint32_t result. "null" pointer is accepted,
                          conversion is done nevertheless but returning
                          the result is omitted.

    @return Result of accessing the data
      @retval == true     OK
      @retval == false    getter failed
  */
  virtual bool get_uint64(const int32_t field_index,
                          uint64_t *out_data) const = 0;

  /**
    Get field data as double value.

    Method validates if column type stored in Column_metadata is equal
    to "Column_type::DOUBLE" also if the field "raw" data contains enough
    bytes for conversion to double. Getter is going to fail when one of
    those checks fails.

    @param field_index    index of the field/column to get
    @param[out] out_data  double result. "null" pointer is accepted, conversion
                          is done nevertheless but returning the result
                          is omitted.

    @return Result of accessing the data
      @retval == true     OK
      @retval == false    getter failed
  */
  virtual bool get_double(const int32_t field_index,
                          double *out_data) const = 0;

  /**
    Get field data as float value.

    Method validates if column type stored in Column_metadata is equal
    to "Column_type::FLOAT" also if the field "raw" data contains enough
    bytes for conversion to float. Getter is going to fail when one of
    those checks fails.

    @param field_index    index of the field/column to get
    @param[out] out_data  float result. "null" pointer is accepted, conversion
                          is done nevertheless but returning the result
                          is omitted.

    @return Result of accessing the data
      @retval == true     OK
      @retval == false    getter failed
  */
  virtual bool get_float(const int32_t field_index, float *out_data) const = 0;

  /**
    Get field data as string value.

    Method validates if column type stored in Column_metadata is equal
    to "Column_type::BYTES" also if the field "raw" data contains enough
    bytes for conversion to string. Getter is going to fail when one of
    those checks fails.

    @param field_index    index of the field/column to get
    @param[out] out_data  string result. "null" pointer is accepted, conversion
                          is done nevertheless but returning the result
                          is omitted.

    @return Result of accessing the data
      @retval == true     OK
      @retval == false    getter failed
  */
  virtual bool get_string(const int32_t field_index,
                          std::string *out_data) const = 0;

  /**
    Get field data as "c" string value.

    Method validates if column type stored in Column_metadata is equal
    to "Column_type::BYTES" also if the field "raw" data contains enough
    bytes for conversion to "c" string. Getter is going to fail when one of
    those checks fails.

    @param field_index          index of the field/column to get
    @param[out] out_data        "c" string result. "null" pointer is accepted,
                                conversion is done nevertheless but returning
                                the result is omitted.
    @param[out] out_data_length length of the returned "c" string

    @return Result of accessing the data
      @retval == true     OK
      @retval == false    getter failed
  */
  virtual bool get_string(const int32_t field_index, const char **out_data,
                          size_t *out_data_length) const = 0;

  /**
    Get field data as decimal value.

    Method validates if column type stored in Column_metadata is equal
    to "Column_type::DECIMAL" also if the field "raw" data contains enough
    bytes for conversion to Decimal. Getter is going to fail when one of
    those checks fails.

    @param field_index    index of the field/column to get
    @param[out] out_data  decimal result. "null" pointer is accepted,
                          conversion is done nevertheless but returning
                          the result is omitted.

    @return Result of accessing the data
      @retval == true     OK
      @retval == false    getter failed
  */
  virtual bool get_decimal(const int32_t field_index,
                           Decimal *out_data) const = 0;

  /**
    Get field data as enum value as string.

    Method validates if column type stored in Column_metadata is equal
    to "Column_type::ENUM" also if the field "raw" data contains enough
    bytes for conversion to string. Getter is going to fail when one of
    those checks fails.

    @param field_index    index of the field/column to get
    @param[out] out_data  string result. "null" pointer is accepted,
                          conversion is done nevertheless but returning
                          the result is omitted.

    @return Result of accessing the data
      @retval == true     OK
      @retval == false    getter failed
  */
  virtual bool get_enum(const int32_t field_index,
                        std::string *out_data) const = 0;

  /**
    Get field data as enum value as "c" string.

    Method validates if column type stored in Column_metadata is equal
    to "Column_type::ENUM" also if the field "raw" data contains enough
    bytes for conversion to "c" string. Getter is going to fail when one of
    those checks fails.

    @param field_index          index of the field/column to get
    @param[out] out_data        "c" string result. "null" pointer is accepted,
                                conversion is done nevertheless but returning
                                the result is omitted
    @param[out] out_data_length length of the returned "c" string

    @return Result of accessing the data
      @retval == true     OK
      @retval == false    getter failed
  */
  virtual bool get_enum(const int32_t field_index, const char **out_data,
                        size_t *out_data_length) const = 0;

  /**
    Get field data as Time value.

    Method validates if column type stored in Column_metadata is equal
    to "Column_type::TIME" also if the field "raw" data contains enough
    bytes for conversion to Time. Getter is going to fail when one of
    those checks fails.

    @param field_index    index of the field/column to get
    @param[out] out_data  Time result. "null" pointer is accepted,
                          conversion is done nevertheless but returning
                          the result is omitted.

    @return Result of accessing the data
      @retval == true     OK
      @retval == false    getter failed
  */
  virtual bool get_time(const int32_t field_index, Time *out_data) const = 0;

  /**
    Get field data as DataTime value.

    Method validates if column type stored in Column_metadata is equal
    to "Column_type::DATETIME" also if the field "raw" data contains enough
    bytes for conversion to DateTime. Getter is going to fail when one of
    those checks fails.

    @param field_index    index of the field/column to get
    @param[out] out_data  DataTime result. "null" pointer is accepted,
                          conversion is done nevertheless but returning
                          the result is omitted.

    @return Result of accessing the data
      @retval == true     OK
      @retval == false    getter failed
  */
  virtual bool get_datetime(const int32_t field_index,
                            DateTime *out_data) const = 0;

  /**
    Get field data as set of strings value.

    Method validates if column type stored in Column_metadata is equal
    to "Column_type::SET" also if the field "raw" data contains enough
    bytes for conversion to set<string>. Getter is going to fail when
    one of those checks fails.

    @param field_index    index of the field/column to get
    @param[out] out_data  set<string> result. "null" pointer is accepted,
                          conversion is done nevertheless but returning
                          the result is omitted.

    @return Result of accessing the data
      @retval == true     OK
      @retval == false    getter failed
  */
  virtual bool get_set(const int32_t field_index,
                       String_set *out_data) const = 0;

  /**
    Get field data as boolean value.

    Method validates if column type stored in Column_metadata is equal
    to "Column_type::BIT" also if the field "raw" data contains enough
    bytes for conversion to bool. Getter is going to fail when
    one of those checks fails.

    @param field_index    index of the field/column to get
    @param[out] out_data  bool result. "null" pointer is accepted,
                          conversion is done nevertheless but returning
                          the result is omitted.

    @return Result of accessing the data
      @retval == true     OK
      @retval == false    getter failed
  */
  virtual bool get_bit(const int32_t field_index, bool *out_data) const = 0;

  /**
    Get field data as uint64_t boolean.

    Method validates if column type stored in Column_metadata is equal
    to "Column_type::BIT" also if the field "raw" data contains enough
    bytes for conversion to uint64. Getter is going to fail when
    one of those checks fails.

    @param field_index    index of the field/column to get
    @param[out] out_data  uint64_t result. "null" pointer is accepted,
                          conversion is done nevertheless but returning
                          the result is omitted.

    @return Result of accessing the data
      @retval == true     OK
      @retval == false    getter failed
  */
  virtual bool get_bit(const int32_t field_index, uint64_t *out_data) const = 0;

  /**
    Get field data and convert it to string.

    In case of null value, the method returns "null" string in all other
    cases its is going to call one of: get_int64_t, get_uint64_t, get_bit,
    get_string, get_set... based on  column type. The resulting "C++"
    type is going to be converted to string (out_data).
    If "field" to "C++ data type" conversion is going to fail the method
    is going fail.

    @param field_index    index of the field/column to convert
    @param[out] out_data  field converted to string. "null" pointer
                          is accepted, conversion is done still returning
                          the result is omitted.

    @return Result of accessing the data
      @retval == true     OK
      @retval == false    getter failed
  */
  virtual bool get_field_as_string(const int32_t field_index,
                                   std::string *out_data) const = 0;
};

}  // namespace xcl

#endif  // X_CLIENT_MYSQLXCLIENT_XROW_H_

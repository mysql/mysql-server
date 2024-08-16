/* Copyright (c) 2024, Oracle and/or its affiliates.

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

#ifndef SERVER_TELEMETRY_ATTRIBUTE_BITS_H
#define SERVER_TELEMETRY_ATTRIBUTE_BITS_H

enum log_attribute_type {

  LOG_ATTRIBUTE_BOOLEAN,  // bool
  LOG_ATTRIBUTE_INT32,
  LOG_ATTRIBUTE_UINT32,
  LOG_ATTRIBUTE_INT64,
  LOG_ATTRIBUTE_UINT64,
  LOG_ATTRIBUTE_DOUBLE,
  LOG_ATTRIBUTE_STRING,
  LOG_ATTRIBUTE_STRING_VIEW
};

/*
  This structure is used to pass attribute values in APIs.

  For simplicity, it is a struct instead of a union.
  The struct sizeof() is not a concern, as data is passed by address, and is
  never used for long term storage.
*/
struct log_attribute_value {
  bool bool_value;
  int32_t int32_value;
  uint32_t uint32_value;
  int64_t int64_value;
  uint64_t uint64_value;
  double double_value;
  const char *string_value;  // for STRING and STRING_VIEW
  size_t string_length;      // for STRING_VIEW
};

struct log_attribute_t {
  const char *name;
  enum log_attribute_type type;
  log_attribute_value value;

  void set_bool(const char *attr_name, bool v) {
    name = attr_name;
    type = LOG_ATTRIBUTE_BOOLEAN;
    value.bool_value = v;
  }

  void set_int32(const char *attr_name, int32_t v) {
    name = attr_name;
    type = LOG_ATTRIBUTE_INT32;
    value.int32_value = v;
  }

  void set_uint32(const char *attr_name, uint32_t v) {
    name = attr_name;
    type = LOG_ATTRIBUTE_UINT32;
    value.uint32_value = v;
  }

  void set_int64(const char *attr_name, int64_t v) {
    name = attr_name;
    type = LOG_ATTRIBUTE_INT64;
    value.int64_value = v;
  }

  void set_uint64(const char *attr_name, uint64_t v) {
    name = attr_name;
    type = LOG_ATTRIBUTE_UINT64;
    value.uint64_value = v;
  }

  void set_double(const char *attr_name, double v) {
    name = attr_name;
    type = LOG_ATTRIBUTE_DOUBLE;
    value.double_value = v;
  }

  void set_string(const char *attr_name, const char *v) {
    name = attr_name;
    type = LOG_ATTRIBUTE_STRING;
    value.string_value = v;
  }

  void set_string_view(const char *attr_name, const char *v, size_t len) {
    name = attr_name;
    type = LOG_ATTRIBUTE_STRING_VIEW;
    value.string_value = v;
    value.string_length = len;
  }
};

#endif /* SERVER_TELEMETRY_ATTRIBUTE_BITS_H */

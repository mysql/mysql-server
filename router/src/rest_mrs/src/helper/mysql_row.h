/*
  Copyright (c) 2021, 2022, Oracle and/or its affiliates.

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
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef ROUTER_SRC_REST_MRS_SRC_HELPER_MYSQL_ROW_H_
#define ROUTER_SRC_REST_MRS_SRC_HELPER_MYSQL_ROW_H_

#include <cstdlib>
#include <map>
#include <optional>
#include <string>

#include "mysqlrouter/mysql_session.h"

#include "helper/mysql_time.h"
#include "helper/optional.h"

namespace helper {

class MySQLRow {
 public:
  using Row = mysqlrouter::MySQLSession::Row;

 public:
  MySQLRow(const Row &row) : row_{row} {}

  void skip(uint32_t to_skip) { field_index_ += to_skip; }

  template <typename FieldType>
  void unserialize(FieldType *out_field) {
    auto in_value = row_[field_index_++];

    convert(out_field, in_value);
  }

  template <typename FieldType, typename Converter>
  void unserialize_with_converter(FieldType *out_field,
                                  const Converter &converter) {
    auto in_value = row_[field_index_++];

    converter(out_field, in_value);
  }

  template <typename FieldType>
  void unserialize(bool *has_field, FieldType *out_field) {
    auto in_value = row_[field_index_++];

    *has_field = in_value != nullptr;

    if (!in_value) {
      return;
    }

    convert(out_field, in_value);
  }

  template <typename FieldType>
  void unserialize(std::optional<FieldType> *out_field) {
    auto in_value = row_[field_index_++];

    out_field->reset();

    if (in_value) {
      FieldType out_value;
      convert(&out_value, in_value);
      *out_field = std::move(out_value);
    }
  }

  template <typename FieldType>
  void unserialize(helper::Optional<FieldType> *out_field) {
    auto in_value = row_[field_index_++];

    out_field->reset();

    if (in_value) {
      FieldType out_value;
      convert(&out_value, in_value);
      *out_field = std::move(out_value);
    }
  }

 private:
  void convert(bool *out_value, const char *in_value) {
    if (!in_value) {
      *out_value = {};
      return;
    }

    if (isalpha(in_value[0])) {
      static std::map<std::string, bool> conversion{
          {"false", false}, {"FALSE", false}, {"true", true}, {"TRUE", true}};

      *out_value = conversion[in_value];
      return;
    }

    *out_value = atoi(in_value);
  }

  void convert(std::string *out_value, const char *in_value) {
    if (in_value)
      *out_value = in_value;
    else
      *out_value = "";
  }

  void convert(uint32_t *out_value, const char *in_value) {
    *out_value = std::stoul(in_value);
  }

  void convert(int32_t *out_value, const char *in_value) {
    *out_value = atoi(in_value);
  }

  void convert(uint64_t *out_value, const char *in_value) {
    char *out_value_end;
    *out_value = strtoull(in_value, &out_value_end, 10);
  }

  void convert(std::vector<uint64_t> *out_value, const char *v) {
    char *endptr = const_cast<char *>(v);
    while (v && *v != 0) {
      out_value->push_back(strtoull(v, &endptr, 10));

      while (endptr && !(isalnum(*endptr) || *endptr == '-') && *endptr != 0)
        ++endptr;
      v = endptr;
    }
  }

  void convert(helper::DateTime *out_value, const char *in_value) {
    out_value->from_string(in_value);
  }

  void convert(void * /*out_value*/, const char * /*in_value*/) {}

  uint32_t field_index_{0};
  const Row &row_;
};

}  // namespace helper

#endif  // ROUTER_SRC_REST_MRS_SRC_HELPER_MYSQL_ROW_H_

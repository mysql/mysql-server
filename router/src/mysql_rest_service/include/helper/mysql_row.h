/*
  Copyright (c) 2021, 2024, Oracle and/or its affiliates.

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

#include <cassert>
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
  using ResultRow = mysqlrouter::MySQLSession::ResultRow;

 public:
  MySQLRow(const ResultRow &row, MYSQL_FIELD *fields, unsigned number)
      : row_{row}, fields_{fields}, no_of_fields_{number} {}

  ~MySQLRow() {
    // At the end of execution "field_index_" contains array index to next
    // field to read. Thus it may be interpreted as number of consumed fields.
    //
    // If the assert fails it means that Query fetched more fields that
    // the user-code unserialized (missing `unserialize` calls).
    //
    // If the user code had finished fetching data, and there are still
    // unserialized fields/data, then the user code should call
    // `skip` to mark that those fields will be not unserialized.
    assert(field_index_ == no_of_fields_ &&
           "Number of consumed fields should be equal to number or provided "
           "fields.");
  }

  void skip(uint32_t to_skip = 1) { field_index_ += to_skip; }

  template <typename FieldType>
  void unserialize(FieldType *out_field) {
    assert((no_of_fields_ == 0 || field_index_ < no_of_fields_) &&
           "Index out of boundary.");
    auto in_value = row_[field_index_++];

    convert(field_index_ - 1, out_field, in_value);
  }

  template <typename FieldType>
  void unserialize(FieldType *out_field, FieldType value_default) {
    assert((no_of_fields_ == 0 || field_index_ < no_of_fields_) &&
           "Index out of boundary.");
    auto in_value = row_[field_index_++];

    if (in_value)
      convert(field_index_ - 1, out_field, in_value);
    else
      *out_field = value_default;
  }

  template <typename FieldType, typename Converter>
  void unserialize_with_converter(FieldType *out_field,
                                  const Converter &converter) {
    assert((no_of_fields_ == 0 || field_index_ < no_of_fields_) &&
           "Index out of boundary.");
    auto in_value = row_[field_index_++];

    converter(out_field, in_value);
  }

  template <typename FieldType, typename Converter>
  void unserialize_with_converter(std::optional<FieldType> *out_field,
                                  const Converter &converter) {
    assert((no_of_fields_ == 0 || field_index_ < no_of_fields_) &&
           "Index out of boundary.");
    auto in_value = row_[field_index_++];

    out_field->reset();

    if (in_value) converter(out_field, in_value);
  }

  template <typename FieldType, typename Converter>
  void unserialize_with_converter(helper::Optional<FieldType> *out_field,
                                  const Converter &converter) {
    assert((no_of_fields_ == 0 || field_index_ < no_of_fields_) &&
           "Index out of boundary.");
    auto in_value = row_[field_index_++];

    out_field->reset();

    if (in_value) {
      FieldType r;
      converter(&r, in_value);
      *out_field = std::move(r);
    }
  }

  template <typename FieldType>
  void unserialize(std::optional<FieldType> *out_field) {
    assert((no_of_fields_ == 0 || field_index_ < no_of_fields_) &&
           "Index out of boundary.");
    auto in_value = row_[field_index_++];

    out_field->reset();

    if (in_value) {
      FieldType out_value;
      convert(field_index_ - 1, &out_value, in_value);
      *out_field = std::move(out_value);
    }
  }

  template <typename FieldType>
  void unserialize(helper::Optional<FieldType> *out_field) {
    assert((no_of_fields_ == 0 || field_index_ < no_of_fields_) &&
           "Index out of boundary.");
    auto in_value = row_[field_index_++];

    out_field->reset();

    if (in_value) {
      FieldType out_value;
      convert(field_index_ - 1, &out_value, in_value);
      *out_field = std::move(out_value);
    }
  }

 private:
  void convert(unsigned field_index, bool *out_value, const char *in_value) {
    if (!in_value) {
      *out_value = {};
      return;
    }

    if (fields_) {
      if (field_index < no_of_fields_) {
        if (fields_[field_index].type == MYSQL_TYPE_BIT) {
          *out_value = *reinterpret_cast<const uint8_t *>(in_value) != 0;
          return;
        }
      }
    }
    if (isalpha(in_value[0])) {
      static std::map<std::string, bool> conversion{
          {"false", false}, {"FALSE", false}, {"true", true}, {"TRUE", true}};

      *out_value = conversion[in_value];
      return;
    }

    *out_value = atoi(in_value);
  }

  void convert([[maybe_unused]] unsigned field_index, std::string *out_value,
               const char *in_value) {
    if (in_value)
      *out_value = in_value;
    else
      *out_value = "";
  }

  void convert([[maybe_unused]] unsigned field_index, uint32_t *out_value,
               const char *in_value) {
    if (in_value)
      *out_value = std::stoul(in_value);
    else
      *out_value = 0;
  }

  void convert([[maybe_unused]] unsigned field_index, int32_t *out_value,
               const char *in_value) {
    if (in_value)
      *out_value = atoi(in_value);
    else
      *out_value = 0;
  }

  void convert([[maybe_unused]] unsigned field_index, uint64_t *out_value,
               const char *in_value) {
    char *out_value_end;
    *out_value = strtoull(in_value, &out_value_end, 10);
  }

  void convert([[maybe_unused]] unsigned field_index,
               std::vector<uint64_t> *out_value, const char *v) {
    char *endptr = const_cast<char *>(v);
    while (v && *v != 0) {
      out_value->push_back(strtoull(v, &endptr, 10));

      while (endptr && !(isalnum(*endptr) || *endptr == '-') && *endptr != 0)
        ++endptr;
      v = endptr;
    }
  }

  void convert([[maybe_unused]] unsigned field_index,
               helper::DateTime *out_value, const char *in_value) {
    out_value->from_string(in_value);
  }

  //  void convert(void * /*out_value*/, const char * /*in_value*/) {}

 public:
  uint32_t field_index_{0};
  const ResultRow &row_;
  MYSQL_FIELD *fields_;
  unsigned no_of_fields_;
};

}  // namespace helper

#endif  // ROUTER_SRC_REST_MRS_SRC_HELPER_MYSQL_ROW_H_

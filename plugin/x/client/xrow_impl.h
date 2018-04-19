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

#ifndef X_CLIENT_XROW_IMPL_H_
#define X_CLIENT_XROW_IMPL_H_

#include <cstdint>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "plugin/x/client/message_holder.h"
#include "plugin/x/client/mysqlxclient/xrow.h"
#include "plugin/x/client/xcontext.h"

namespace xcl {

class XRow_impl : public XRow {
 public:
  using Metadata = std::vector<Column_metadata>;

 public:
  explicit XRow_impl(Metadata *metadata, Context *context);
  ~XRow_impl() override = default;

  int32_t get_number_of_fields() const override;
  bool valid() const override;

  bool is_null(const int32_t field_index) const override;
  bool get_int64(const int32_t field_index, int64_t *out_data) const override;
  bool get_uint64(const int32_t field_index, uint64_t *out_data) const override;
  bool get_double(const int32_t field_index, double *out_data) const override;
  bool get_float(const int32_t field_index, float *out_data) const override;
  bool get_string(const int32_t field_index,
                  std::string *out_data) const override;
  bool get_string(const int32_t field_index, const char **out_data,
                  size_t *out_data_length) const override;
  bool get_enum(const int32_t field_index,
                std::string *out_data) const override;
  bool get_enum(const int32_t field_index, const char **out_data,
                size_t *out_data_length) const override;
  bool get_decimal(const int32_t field_index, Decimal *out_data) const override;
  bool get_time(const int32_t field_index, Time *out_data) const override;
  bool get_datetime(const int32_t field_index,
                    DateTime *out_data) const override;
  bool get_set(const int32_t field_index,
               std::set<std::string> *out_data) const override;
  bool get_bit(const int32_t field_index, bool *out_data) const override;
  bool get_bit(const int32_t field_index, uint64_t *out_data) const override;

  bool get_field_as_string(const int32_t field_index,
                           std::string *out_data) const override;

  void clean();
  void set_row(std::unique_ptr<Row> &&row);

 private:
  bool get_string_based_field(const Column_type expected_type,
                              const int32_t field_index, const char **out_data,
                              size_t *out_data_length) const;

  std::unique_ptr<Row> m_row;
  Metadata *m_metadata;
  Context *m_context;
};

}  // namespace xcl

#endif  // X_CLIENT_XROW_IMPL_H_

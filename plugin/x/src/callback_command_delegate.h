/*
 * Copyright (c) 2015, 2024, Oracle and/or its affiliates.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is designed to work with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have either included with
 * the program or referenced in the documentation.
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

#ifndef PLUGIN_X_SRC_CALLBACK_COMMAND_DELEGATE_H_
#define PLUGIN_X_SRC_CALLBACK_COMMAND_DELEGATE_H_

#include <sys/types.h>
#include <functional>

#include <cstdint>
#include <string>
#include <vector>

#include "plugin/x/src/ngs/command_delegate.h"

struct CHARSET_INFO;

namespace xpl {
class Callback_command_delegate : public ngs::Command_delegate {
 public:
  struct Field_value {
    Field_value();
    Field_value(const Field_value &other);
    explicit Field_value(const char *str, size_t length);

    explicit Field_value(const longlong &num, bool unsign = false);
    explicit Field_value(const decimal_t &decimal);
    explicit Field_value(const double num);
    explicit Field_value(const MYSQL_TIME &time);

    Field_value &operator=(const Field_value &other);

    ~Field_value();
    union {
      longlong v_long;
      double v_double;
      decimal_t v_decimal;
      MYSQL_TIME v_time;
      std::string *v_string;
    } value;
    bool is_unsigned;
    bool is_string;
  };

  struct Row_data {
    Row_data() = default;
    Row_data(const Row_data &other);
    Row_data &operator=(const Row_data &other);
    ~Row_data();
    void clear();
    std::vector<Field_value *> fields;

   private:
    void clone_fields(const Row_data &other);
  };

  using Start_row_callback = std::function<Row_data *()>;
  using End_row_callback = std::function<bool(Row_data *)>;

  Callback_command_delegate();
  Callback_command_delegate(Start_row_callback start_row,
                            End_row_callback end_row);

  void set_callbacks(Start_row_callback start_row, End_row_callback end_row);

  void reset() override;

  enum cs_text_or_binary representation() const override {
    return CS_TEXT_REPRESENTATION;
  }

 private:
  Start_row_callback m_start_row;
  End_row_callback m_end_row;
  Row_data *m_current_row = nullptr;

 private:
  int start_row() override;

  int end_row() override;

  void abort_row() override;

  ulong get_client_capabilities() override;

  /****** Getting data ******/
  int get_null() override;

  int get_integer(longlong value) override;

  int get_longlong(longlong value, uint32_t unsigned_flag) override;

  int get_decimal(const decimal_t *value) override;

  int get_double(double value, uint32_t decimals) override;

  int get_date(const MYSQL_TIME *value) override;

  int get_time(const MYSQL_TIME *value, uint32_t decimals) override;

  int get_datetime(const MYSQL_TIME *value, uint32_t decimals) override;

  int get_string(const char *const value, size_t length,
                 const CHARSET_INFO *const valuecs) override;
};
}  // namespace xpl

#endif  // PLUGIN_X_SRC_CALLBACK_COMMAND_DELEGATE_H_

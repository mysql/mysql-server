/*
 * Copyright (c) 2018, 2023, Oracle and/or its affiliates.
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

#ifndef PLUGIN_X_SRC_CAPABILITIES_HANDLER_CONNECTION_ATTRIBUTES_H_
#define PLUGIN_X_SRC_CAPABILITIES_HANDLER_CONNECTION_ATTRIBUTES_H_

#include <vector>

#include "plugin/x/src/capabilities/handler.h"

namespace xpl {

class Capability_connection_attributes : public Capability_handler {
 public:
  std::string name() const override { return "session_connect_attrs"; }
  bool is_settable() const override { return true; }
  bool is_gettable() const override { return false; }

  void commit() override;

 private:
  void get_impl(::Mysqlx::Datatypes::Any *any) override;
  ngs::Error_code set_impl(const ::Mysqlx::Datatypes::Any &any) override;
  bool is_supported_impl() const override { return true; }

  std::vector<unsigned char> create_buffer();
  void log_size_exceeded(const char *const name, const std::size_t value,
                         const std::size_t max_value) const;
  unsigned char *write_length_encoded_string(unsigned char *buf,
                                             const std::string &string);
  ngs::Error_code validate_field(
      const Mysqlx::Datatypes::Object_ObjectField &field) const;
  void log_capability_corrupted() const;

  std::vector<std::pair<std::string, std::string>> m_attributes;
  std::size_t m_attributes_length = 0;
  static const std::size_t k_max_key_size = 32;
  static const std::size_t k_max_value_size = 1024;
  static const std::size_t k_max_buffer_size = 64 * 1024;
};

}  // namespace xpl

#endif  // PLUGIN_X_SRC_CAPABILITIES_HANDLER_CONNECTION_ATTRIBUTES_H_

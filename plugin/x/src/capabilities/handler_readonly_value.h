/*
 * Copyright (c) 2015, 2019, Oracle and/or its affiliates. All rights reserved.
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

#ifndef PLUGIN_X_SRC_CAPABILITIES_HANDLER_READONLY_VALUE_H_
#define PLUGIN_X_SRC_CAPABILITIES_HANDLER_READONLY_VALUE_H_

#include "plugin/x/ngs/include/ngs/mysqlx/setter_any.h"
#include "plugin/x/src/capabilities/handler.h"

namespace xpl {

class Capability_readonly_value : public Capability_handler {
 public:
  template <typename ValueType>
  Capability_readonly_value(const std::string &cap_name, const ValueType &value)
      : m_name(cap_name) {
    ngs::Setter_any::set_scalar(&m_value, value);
  }

  std::string name() const override { return m_name; }
  bool is_settable() const override { return false; }
  bool is_gettable() const override { return true; }

  void commit() override {}

 private:
  void get_impl(::Mysqlx::Datatypes::Any *any) override {
    any->CopyFrom(m_value);
  }
  ngs::Error_code set_impl(const ::Mysqlx::Datatypes::Any &) override {
    return ngs::Error(ER_X_CAPABILITIES_PREPARE_FAILED,
                      "CapabilitiesSet not supported for the %s capability",
                      name().c_str());
  }
  bool is_supported_impl() const override { return true; }

  const std::string m_name;
  ::Mysqlx::Datatypes::Any m_value;
};

}  // namespace xpl

#endif  // PLUGIN_X_SRC_CAPABILITIES_HANDLER_READONLY_VALUE_H_

/*
 * Copyright (c) 2019, 2024, Oracle and/or its affiliates.
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

#ifndef PLUGIN_X_SRC_HELPER_OPTIONAL_VALUE_H_
#define PLUGIN_X_SRC_HELPER_OPTIONAL_VALUE_H_

#include <cassert>

namespace xpl {

template <typename Type>
class Optional_value {
 public:
  Optional_value() : m_value{Type()}, m_has_value{false} {}

  explicit Optional_value(const Type &value)
      : m_value{value}, m_has_value{true} {}

  Optional_value &operator=(const Type &value) {
    m_value = value;
    m_has_value = true;

    return *this;
  }

  bool get_value(Type *out_value) const {
    if (!m_has_value) return false;

    if (out_value) *out_value = m_value;

    return true;
  }

  bool has_value() const { return m_has_value; }
  const Type &value() const {
    assert(m_has_value);
    return m_value;
  }
  void reset() { m_has_value = false; }

 private:
  Type m_value;
  bool m_has_value{false};
};

}  // namespace  xpl

#endif  // PLUGIN_X_SRC_HELPER_OPTIONAL_VALUE_H_

/*
 * Copyright (c) 2019, Oracle and/or its affiliates. All rights reserved.
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

#ifndef PLUGIN_X_SRC_SET_VARIABLE_H_
#define PLUGIN_X_SRC_SET_VARIABLE_H_

#include <algorithm>
#include <utility>
#include <vector>

#include "my_inttypes.h"  // NOLINT(build/include_subdir)
#include "typelib.h"      // NOLINT(build/include_subdir)

namespace xpl {

class Set_variable {
 public:
  Set_variable(std::vector<const char *> labels)
      : m_labels(std::move(labels)),
        m_typelib{m_labels.size() - 1, "", m_labels.data(), nullptr} {}

  ulonglong *value() { return &m_value; }
  TYPELIB *typelib() { return &m_typelib; }
  const ulonglong &get_value() const { return m_value; }
  const std::vector<const char *> &get_labels() const { return m_labels; }

 private:
  ulonglong m_value{0};
  std::vector<const char *> m_labels;
  TYPELIB m_typelib;
};

}  // namespace xpl

#endif  // PLUGIN_X_SRC_SET_VARIABLE_H_

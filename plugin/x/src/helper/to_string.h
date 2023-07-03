/*
 * Copyright (c) 2016, 2022, Oracle and/or its affiliates.
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

#ifndef PLUGIN_X_SRC_HELPER_TO_STRING_H_
#define PLUGIN_X_SRC_HELPER_TO_STRING_H_

#include <string>
#include "m_string.h"

namespace xpl {
namespace detail {

template <typename T>
inline std::string to_string(const my_gcvt_arg_type arg_type, T value) {
  char buffer[100];
  my_gcvt(value, arg_type, sizeof(buffer) - 1, buffer, nullptr);
  return buffer;
}

}  // namespace detail

template <typename T>
inline std::string to_string(T value) {
  return std::to_string(value);
}

template <>
inline std::string to_string<double>(double value) {
  return detail::to_string(MY_GCVT_ARG_DOUBLE, value);
}

template <>
inline std::string to_string<float>(float value) {
  return detail::to_string(MY_GCVT_ARG_FLOAT, value);
}

}  // namespace xpl

#endif  // PLUGIN_X_SRC_HELPER_TO_STRING_H_

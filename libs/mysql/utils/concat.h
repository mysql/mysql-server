/* Copyright (c) 2023, 2026, Oracle and/or its affiliates.

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

/// @file
///
/// Convenience function that concatenates arbitrary arguments, by
/// feeding them to an ostringstream.

#ifndef MYSQL_UTILS_CONCAT_H
#define MYSQL_UTILS_CONCAT_H

#include <sstream>                       // ostringstream
#include <string>                        // string
#include <utility>                       // ignore
#include "mysql/utils/call_and_catch.h"  // call_and_catch

/// @addtogroup GroupLibsMysqlUtils
/// @{

namespace mysql::utils::throwing {

/// Stream all the arguments to a stringstream and return the resulting string.
///
/// @tparam Args_t types of the arguments.
///
/// @param args Arguments to concatenate.
///
/// @return The resulting std::string.
///
/// @throws bad_alloc if an out-of-memory condition occurs.
template <class... Args_t>
[[nodiscard]] std::string concat(Args_t &&...args) {
  std::ostringstream out;
  // std::ignore required to workaround
  // https://github.com/llvm/llvm-project/issues/140205
  std::ignore = (out << ... << std::forward<Args_t>(args));
  return out.str();
}

}  // namespace mysql::utils::throwing

namespace mysql::utils {

/// Stream all the arguments to a stringstream and return the resulting string.
///
/// @param args Arguments to concatenate.
///
/// @return std::optional<std::string>, holding a value on success, or holding
/// no value if an out-of-memory condition occurs.
template <class... Args_t>
[[nodiscard]] std::optional<std::string> concat(Args_t &&...args) noexcept {
  return call_and_catch(
      [&] { return throwing::concat(std::forward<Args_t>(args)...); });
}

}  // namespace mysql::utils
/// @}

#endif  // MYSQL_UTILS_CONCAT_H

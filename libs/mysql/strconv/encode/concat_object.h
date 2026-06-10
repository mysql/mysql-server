// Copyright (c) 2025, 2026, Oracle and/or its affiliates.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version 2.0,
// as published by the Free Software Foundation.
//
// This program is designed to work with certain software (including
// but not limited to OpenSSL) that is licensed under separate terms,
// as designated in a particular file or component or in included license
// documentation.  The authors of MySQL hereby grant you an additional
// permission to link the program and your derivative works with the
// separately licensed software that they have either included with
// the program or referenced in the documentation.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License, version 2.0, for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

#ifndef MYSQL_STRCONV_ENCODE_CONCAT_OBJECT_H
#define MYSQL_STRCONV_ENCODE_CONCAT_OBJECT_H

/// @file
/// Experimental API header

#include <tuple>  // tuple

/// @addtogroup GroupLibsMysqlStrconv
/// @{

namespace mysql::strconv {

/// Helper type that wraps the varargs of `concat` in a struct containing a
/// tuple.
template <class... Args_tp>
struct Concat_object {
  using Tuple_t = std::tuple<const Args_tp &...>;

  /// Construct a new Concat_object whose tuple elements are the given
  /// arguments.
  explicit Concat_object(const Args_tp &...args) : m_args(args...) {}

  /// Tuple holding objects to be concatenated.
  Tuple_t m_args;
};

}  // namespace mysql::strconv

// addtogroup GroupLibsMysqlStrconv
/// @}

#endif  // ifndef MYSQL_STRCONV_ENCODE_CONCAT_OBJECT_H

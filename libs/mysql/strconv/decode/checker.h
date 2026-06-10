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

#ifndef MYSQL_STRCONV_DECODE_CHECKER_H
#define MYSQL_STRCONV_DECODE_CHECKER_H

/// @file
/// Experimental API header

#include <concepts>                        // invocable
#include "mysql/meta/is_specialization.h"  // Is_specialization

/// @addtogroup GroupLibsMysqlStrconv
/// @{

namespace mysql::strconv {

// Forward declaration
class Parser;

/// True for types that are invocable without arguments.
///
/// Checkers should test that a parsed object is valid, and if not, call
/// `Parser::set_parse_error` in the Parser (which should be
/// captured).
template <class Test>
concept Is_checker_function = std::invocable<Test>;

/// Class holding a checker function, used to check the validity of a parsed
/// value.
///
/// This type only encapsulates an std::invocable object. By using a
/// distinguished wrapper type rather than using the invocable type directly, we
/// avoid making the definition of `operator|` for parse options (see
/// parse_options.h) apply to all invocable types.
template <Is_checker_function Checker_function_tp>
class Checker {
 public:
  using Checker_function_t = Checker_function_tp;

  /// Can't default-construct a lambda.
  Checker() = delete;

  /// Construct a Checker from the given checker function.
  explicit Checker(const Checker_function_t &checker_function)
      : m_checker_function(checker_function) {}

  /// Invoke the checker function.
  void check() const { m_checker_function(); }

 private:
  /// The checker function.
  Checker_function_t m_checker_function;
};

/// True for all Checker specializations.
template <class Test>
concept Is_checker = mysql::meta::Is_specialization<Test, Checker>;

}  // namespace mysql::strconv

// addtogroup GroupLibsMysqlStrconv
/// @}

#endif  // ifndef MYSQL_STRCONV_DECODE_CHECKER_H

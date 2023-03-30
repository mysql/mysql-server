/*
  Copyright (c) 2018, 2023, Oracle and/or its affiliates.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is also distributed with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have included with MySQL.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef MYSQLROUTER_MATCHER_INCLUDED
#define MYSQLROUTER_MATCHER_INCLUDED

#include <algorithm>

// single char matcher
//
// API is inspired by PEGTL.

namespace Matcher {

/**
 * matches a Range of characters.
 */
template <char S, char E>
class Range {
 public:
  static constexpr bool match(char c) {
    static_assert(S <= E, "S <= E");
    return (S <= c) && (c <= E);
  }
};

/**
 * check if a initizalizer list contains a character.
 *
 * @note: could be constexpr with C++20
 */
bool contains(char c, const std::initializer_list<char> &l) {
  return std::find(l.begin(), l.end(), c) != l.end();
}

/**
 * matches one character in a list of possible candidates.
 */
template <char... Arg>
class One;

template <char Arg>
class One<Arg> {
 public:
  static bool match(char c) { return Arg == c; }
};

template <char... Arg>
class One {
 public:
  static bool match(char c) { return contains(c, {Arg...}); }
};

/**
 * matches Rules left-to-right with OR.
 */
template <class... Rules>
class Sor;

// empty case, is false.
template <>
class Sor<> {
 public:
  static bool match(char /* c */) { return false; }
};

// generic case
template <class... Rules>
class Sor {
 public:
  static bool match(char c) {
    bool result = false;

    // emulate fold-expression for C++11 and later
    //
    //   result = Rules::match(c) || ...;
    //
    // See https://en.cppreference.com/w/cpp/language/fold

    // 'swallow' is just a vehicle and unused.
    using swallow = bool[sizeof...(Rules)];
    (void)swallow{result = result || Rules::match(c)...};

    return result;
  }
};

/**
 * [0-9].
 */
using Digit = Range<'0', '9'>;

/**
 * [a-z].
 */
using Lower = Range<'a', 'z'>;

/**
 * [A-Z].
 */
using Upper = Range<'A', 'Z'>;

/**
 * [a-zA-Z].
 */
using Alpha = Sor<Lower, Upper>;

/**
 * [a-zA-Z0-9].
 */
using Alnum = Sor<Alpha, Digit>;

}  // namespace Matcher

#endif

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

#ifndef MYSQL_STRCONV_DECODE_REPEAT_H
#define MYSQL_STRCONV_DECODE_REPEAT_H

/// @file
/// Experimental API header

#include <cassert>   // assert
#include <concepts>  // integral
#include <limits>    // numeric_limits

/// @addtogroup GroupLibsMysqlStrconv
/// @{

namespace mysql::strconv {

namespace detail {
/// Common base class for the Repeat and Repeat_optional classes, providing
/// functionality for representing the upper bound on the number of repetitions.
/// Provides a constructor that takes an integral value for the upper bound on
/// the number of repetitions, as well as several static factory functions for
/// creating Repeat_optional objects with specific repetition ranges.
template <class Repeat_optional_t>
class Repeat_base {
 public:
  /// Construct a new object with the given maximum.
  explicit Repeat_base(std::integral auto max) : m_max(std::size_t(max)) {}

  /// Return a Repeat_optional object representing zero or one instances.
  [[nodiscard]] static Repeat_optional_t optional() {
    return Repeat_optional_t{1};
  }

  /// Return a Repeat object representing between 0 and `min_arg`
  /// repetitions.
  [[nodiscard]] static Repeat_optional_t at_most(std::integral auto max_arg) {
    return Repeat_optional_t{max_arg};
  }

  /// Return a Repeat_optional object representing any number of repetitions
  /// from 0 and up (bounded only by std::numeric_limits).
  [[nodiscard]] static Repeat_optional_t any() {
    return Repeat_optional_t{std::numeric_limits<std::size_t>::max()};
  }

  /// Return the maximum number of repetitions, inclusive.
  [[nodiscard]] std::size_t max() const { return m_max; }

 private:
  /// The maximum number of repetitions, inclusive.
  std::size_t m_max;
};
}  // namespace detail

/// Represents a range of integers for which the lower bound is 0, representing
/// the number of repetitions of a token when parsing a string.
///
/// This class is similar to Repeat, but limited to ranges that begin at zero.
/// Examples:
/// - Repeat_optional(3): up to 3 repetitions (0 to 3)
/// - Repeat_optional::optional(): 0 or 1 repetitions (analogous to the regex
///   syntax "?")
/// - Repeat_optional::any(): 0 or more repetitions (analogous to the regex
///   syntax "*")
///
/// The reason we define a distinct class for this case, rather than use
/// `Repeat` with a specified lower bound of 0, is that some functions taking a
/// number of repetitions as argument cannot fail when 0 is an allowed number of
/// repetitions. Such functions can therefore return void and/or omit the
/// [[nodiscard]] attribute when the argument is of type `Repeat_optional`.
class Repeat_optional : public detail::Repeat_base<Repeat_optional> {
 public:
  /// Default-construct a Repeat_optional object representing a range of 0..1
  /// repetitions, inclusive.
  Repeat_optional() : Repeat_optional(1) {}

  /// Construct an object representing a range of 0 up to the given number of
  /// repetitions.
  explicit Repeat_optional(std::integral auto max) : Repeat_base(max) {}

  /// Return the mimimum number of repetitions, inclusive; the return value is
  /// always 0.
  [[nodiscard]] constexpr std::size_t min() const { return 0; }
};

/// Represents a range of integers specifying the number of times a token or
/// pattern should be repeated when parsing a string.
///
/// The range is defined by a minimum and maximum value, which can be used to
/// constrain the number of repetitions.
///
/// Examples:
/// - Repeat::one(): exactly 1 repetition
/// - Repeat::exact(3): exactly 3 repetitions
/// - Repeat::range(2, 5): between 2 and 5 repetitions
/// - Repeat::at_least(1): 1 or more repetitions (analogous to regex syntax "+")
class Repeat : public detail::Repeat_base<Repeat_optional> {
 public:
  /// Default-construct a Repeat object representing exactly 1 repetition.
  Repeat() : Repeat(1) {}

  /// Construct an object representing exactly the given number of repetitions
  explicit Repeat(std::integral auto count) : Repeat(count, count) {}

  /// Construct an object representing a range of at least `min_arg` and at most
  /// `max_arg` repetitions.
  template <std::integral Int_t>
  explicit Repeat(Int_t min_arg, Int_t max_arg)
      : Repeat_base(std::size_t(max_arg)), m_min(std::size_t(min_arg)) {
    if constexpr (std::signed_integral<Int_t>) {
      assert(min_arg >= 0);
    }
    assert(max_arg >= min_arg);
    if constexpr (std::numeric_limits<Int_t>::digits >
                  std::numeric_limits<std::size_t>::digits) {
      assert(max_arg <= Int_t(std::numeric_limits<std::size_t>::max()));
    }
  }

  /// Return a Repeat object representing `min_arg` or more repetitions
  /// (bounded only by std::numeric_limits)
  template <std::integral Int_t>
  [[nodiscard]] static Repeat at_least(Int_t min_arg) {
    if constexpr (std::numeric_limits<Int_t>::digits >
                  std::numeric_limits<std::size_t>::digits) {
      assert(min_arg <= Int_t(std::numeric_limits<std::size_t>::max()));
    }
    return Repeat{std::size_t(min_arg),
                  std::numeric_limits<std::size_t>::max()};
  }

  /// Return a Repeat object representing a range of at least `min_arg` and
  /// at most `max_arg` repetitions.
  template <std::integral Int_t>
  [[nodiscard]] static Repeat range(Int_t min_arg, Int_t max_arg) {
    return Repeat{min_arg, max_arg};
  }

  /// Return a Repeat object representing exactly the given number of
  /// repetitions.
  [[nodiscard]] static Repeat exact(std::integral auto count) {
    return Repeat{count};
  }

  /// Return a Repeat object representing exactly one repetition.
  [[nodiscard]] static Repeat one() { return Repeat{}; }

  /// Return the mimimum number of repetitions, inclusive.
  [[nodiscard]] std::size_t min() const { return m_min; }

 private:
  /// The mimimum number of repetitions, inclusive.
  std::size_t m_min;
};

/// True if Test is either Repeat or Repeat_optional
template <class Test>
concept Is_repeat =
    std::same_as<Test, Repeat> || std::same_as<Test, Repeat_optional>;

}  // namespace mysql::strconv

// addtogroup GroupLibsMysqlStrconv
/// @}

#endif  // ifndef MYSQL_STRCONV_DECODE_REPEAT_H

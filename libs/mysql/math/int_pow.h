// Copyright (c) 2024, 2026, Oracle and/or its affiliates.
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

#ifndef MYSQL_MATH_INT_POW_H
#define MYSQL_MATH_INT_POW_H

/// @file
/// Experimental API header

#include <bit>       // bit_width
#include <concepts>  // integral
#include <limits>    // numeric_limits

/// @addtogroup GroupLibsMysqlMath
/// @{

namespace mysql::math {

/// Return pow(base, exponent), where the exponent is an integer.
///
/// This does not check for overflow.
///
/// Complexity: For constexpr arguments, reduces to a compile-time constant.
/// Otherwise, logarithmic in the exponent. The number of multiplications is
/// equal to the 2-logarithm of the exponent, plus the number of 1-bits in the
/// exponent.
template <class Value_t>
[[nodiscard]] constexpr Value_t int_pow(Value_t base, unsigned exponent) {
  // We use the following equality:
  //
  //   pow(b, n - k) = pow(b, n) / pow(b, k), or equivalently,
  //   pow(b, n) = pow(b, k) * pow(b, n - k).
  //
  // With b = base, n = exponent, k = floor(exponent / 2), we get
  //
  //   n - k = k, if n is even;
  //   n - k = k + 1, if n is odd.
  //
  // Using the equality pow(b, k + 1) = pow(b, k) * b, we obtain:
  //
  //   pow(b, n) = pow(b, k) * pow(b, n - k)
  //             = pow(b, k) * pow(b, k),     if n is even
  //   pow(b, n) = pow(b, k) * pow(b, n - k)
  //             = pow(b, k) * pow(b, k + 1)
  //             = pow(b, k) * pow(b, k) * b, if n is odd
  if (exponent == 0) return 1;
  Value_t ret = int_pow(base, exponent >> 1);
  ret *= ret;
  if ((exponent & 1) == 1) ret *= base;
  return ret;
}

/// Return the floor of the base-`base` logarithm of
/// numeric_limits<decltype(base>)>::max().
///
/// @tparam base base of the logarithm
///
/// Complexity: reduces to a compile-time constant.
template <auto base>
  requires std::integral<decltype(base)> && (base >= 2)
[[nodiscard]] constexpr unsigned int_log_max() {
  // Count how many times we can divide by `base` until the result becomes 0.
  decltype(base) v = std::numeric_limits<decltype(base)>::max();
  unsigned ret{0};
  while (v >= base) {
    v /= base;
    ++ret;
  }
  return ret;
}

}  // namespace mysql::math

namespace mysql::math::detail {

/// Return the base-`base` logarithm of value, assuming that
/// `value < pow(base, 2 * bound)`, where `bound` is a power of two.
///
/// @tparam Value_t Data type
///
/// @tparam base The base of the logarithm.
///
/// @tparam bound Power of two, such that value < pow(base, 2 * bound).
///
/// @param value Value to test.
///
/// @return the base-`base` logarithm of `value`.
template <auto base, unsigned bound, std::unsigned_integral Value_t>
  requires std::same_as<decltype(base), Value_t> && (base >= 2)
[[nodiscard]] constexpr unsigned int_log_helper(Value_t value) {
  // Make use of the equality log_b(v/n) = log_b(v) - log_b(n), which holds for
  // all positive real numbers b, v, n, for the usual real-valued logarithm.
  //
  // When n is a power of b and v >= n, the analogous formula holds for the
  // integer-valued, integer-argument logarithm, i.e.:
  //   int(log_b(int(v/n))) = int(log_b(int(v))) - int(log_b(int(n)))
  //                        = int(log_b(v)) - log_b(n)
  //
  // This gives us the recursive formula:
  //
  //   int(log_b(v)) = int(log_b(int(v / n))) + log_b(n), if v >= n
  //   int(log_b(v)) = int(log_b(v)),                     otherwise
  //
  // Using n = pow(base, bound), log_b(n) can be computed at compile time, and
  // the recursive calls int(log_b(int(v/n))) and int(log_b(v)) both have half
  // the bound. Thus, only log2(bound) recursive calls are needed.
  if constexpr (bound == 0) return 0;
  constexpr Value_t base_to_power = int_pow(base, bound);
  if (value >= base_to_power) {
    return int_log_helper<base, bound / 2>(Value_t(value / base_to_power)) +
           bound;
  } else {
    return int_log_helper<base, bound / 2>(value);
  }
}

}  // namespace mysql::math::detail

namespace mysql::math {

/// Return the base-`base` logarithm of `value`.
///
/// @tparam base Base of the logarithm.
///
/// @tparam Value_t The integral data type.
///
/// @param value Value to compute the logarithm for.
///
/// @return int(log_base(value)), or 0 if value == 0.
///
/// Complexity: For constexpr arguments, reduces to a constant. Otherwise,
/// logarithmic in the `base` logarithm of std::numeric_limits<Value_t>::max().
/// The number of divisions is the floor of log2(int_log_max<Value_t, base>()),
/// and the denominator is a compile-time constant which is a power of `base`,
/// so that the compiler may use denominator-specific optimizations such as
/// shift-right instead of division operations.
template <auto base, std::unsigned_integral Value_t>
  requires std::same_as<decltype(base), Value_t> && (base >= 2)
[[nodiscard]] constexpr unsigned int_log(Value_t value) {
  // NOLINTNEXTLINE(clang-analyzer-core.BitwiseShift): warning is spurious
  constexpr unsigned bound = 1U << (std::bit_width(int_log_max<base>()) - 1);
  return detail::int_log_helper<base, bound>(value);
}

}  // namespace mysql::math

// addtogroup GroupLibsMysqlMath
/// @}

#endif  // ifndef MYSQL_MATH_INT_POW_H

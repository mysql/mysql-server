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

#ifndef MYSQL_MATH_SUMMATION_H
#define MYSQL_MATH_SUMMATION_H

/// @file
/// Experimental API header

#include <iterator>     // sentinel_for
#include <numeric>      // accumulate
#include <type_traits>  // is_arithmetic_v

/// @addtogroup GroupLibsMysqlMath
/// @{

namespace mysql::math {

/// Tracks the state of the Kahan summation algorithm, which produces a sum over
/// a sequence of floating point numbers with very low numeric error, using an
/// internal error compensation term.
///
/// @tparam Value_tp The numeric type used in internal computations, and to
/// store the result.
template <class Value_tp = double>
class Kahan_sum {
 public:
  using Value_t = Value_tp;

  explicit Kahan_sum(Value_t value = 0) : m_sum(value) {}

  /// Return the current approximated sum.
  [[nodiscard]] explicit operator Value_t() const { return m_sum; }

  /// In-place add the given value to this object.
  Kahan_sum &operator+=(const Value_t &value) {
    Value_t compensated_value = value - m_compensation;
    Value_t new_sum = m_sum + compensated_value;
    m_compensation = (new_sum - m_sum) - compensated_value;
    m_sum = new_sum;
    return *this;
  }

  /// In-place subtract the given value from this object.
  Kahan_sum &operator-=(const Value_t &value) { return *this += -value; }

  /// Return a new object holding the sum of this object and the given value.
  [[nodiscard]] Kahan_sum operator+(const Value_t &value) const {
    Kahan_sum ret = *this;
    ret += value;
    return ret;
  }

  /// Return a new object holding this object minus the given value.
  [[nodiscard]] Kahan_sum operator-(const Value_t &value) const {
    Kahan_sum ret = *this;
    ret -= value;
    return ret;
  }

 private:
  Value_t m_sum;
  Value_t m_compensation{0};
};  // class Kahan_sum

/// Compute the sum of values in the given range, with very low numeric error.
template <class Value_t = double, std::input_iterator Iterator_t>
[[nodiscard]] Value_t kahan_sum(const Iterator_t &first,
                                const std::sentinel_for<Iterator_t> auto &last,
                                Value_t init = 0) {
  return (Value_t)std::accumulate(first, last, Kahan_sum(init));
}

/// Compute the sum of values in the first sequence, minus the sum of values in
/// the second sequence. When the return type is floating point, the result is
/// exact if it is at most `mysql::math::max_exact_int<Return_t>`.
///
/// This uses an algorithm that avoids overflow in intermediate computations
/// as long as the exact result is small, and uses a summation algorithm that
/// minmizes floating point errors if the result is large.
///
/// @tparam Result_t Numeric type for the result.
///
/// @tparam Iterator1_t Type of iterator to first sequence.
///
/// @tparam Iterator2_t Type of iterator to second sequence.
///
/// @param begin1 Iterator to beginning of first sequence.
///
/// @param end1 Sentinel of first sequence.
///
/// @param begin2 Iterator to beginning of second sequence.
///
/// @param end2 Sentinel of second sequence.
///
/// @param init Initial value. Defaults to 0.
///
/// @return Return_t The sum of all values in the range from begin1 to end1,
/// minus the sum of all values in the range from begin2 to end2.
template <class Result_t = long double, std::input_iterator Iterator1_t,
          std::input_iterator Iterator2_t>
  requires std::is_arithmetic_v<Result_t> &&
           std::unsigned_integral<decltype(*std::declval<Iterator1_t>())> &&
           std::unsigned_integral<decltype(*std::declval<Iterator2_t>())>
[[nodiscard]] Result_t sequence_sum_difference(
    const Iterator1_t &begin1, const std::sentinel_for<Iterator1_t> auto &end1,
    const Iterator2_t &begin2, const std::sentinel_for<Iterator2_t> auto &end2,
    uint64_t init = 0) {
  uint64_t sum{init};

  // Read and step the iterator. Subtract from sum. If the result becomes
  // negative, negate and return true; otherwise, return false.
  auto step = [&](auto &it) {
    uint64_t value = *it;
    ++it;
    if (value > sum) {
      sum = value - sum;
      return false;
    }
    sum -= value;
    return true;
  };

  // Add sum(it..last) to the sum and return the result. Do all computations in
  // double since they may overflow.
  auto sum_tail = [&](auto &it, auto last) {
    return kahan_sum(it, last, Result_t(sum));
  };

  // Subtract *it1 from sum until sum becomes negative.
  // Then negate, and subtract *it2 from sum until sum becomes negative.
  // Then negate and start over.
  auto it1{begin1};
  auto it2{begin2};
  while (true) {
    do {
      // Invariant: sum == sum(begin1..it1) - sum(begin2..it2) >= 0
      if (it2 == end2) {
        // Add sum(it1..end1) and return the result.
        return sum_tail(it1, end1);
      }
    } while (step(it2));
    do {
      // Invariant: sum == sum(begin2..it2) - sum(begin1..it1) >= 0
      if (it1 == end1) {
        // Add sum(it2..end2) and return the negated result.
        auto ret = sum_tail(it2, end2);
        // Don't negate 0.0, to avoid returning -0.0
        if (ret > 0) ret = -ret;
        return ret;
      }
    } while (step(it1));
  }
}

}  // namespace mysql::math

// addtogroup GroupLibsMysqlMath
/// @}

#endif  // ifndef MYSQL_MATH_SUMMATION_H

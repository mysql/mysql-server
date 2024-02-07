/* Copyright (c) 2020, 2024, Oracle and/or its affiliates.

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

#include "sql/join_optimizer/print_utils.h"

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <algorithm>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

#include "sql/item_cmpfunc.h"
#include "sql/join_optimizer/relational_expression.h"
#include "sql/mem_root_array.h"

using std::string;
using std::vector;

std::string StringPrintf(const char *fmt, ...) {
  std::string result;
  char buf[256];

  va_list ap;
  va_start(ap, fmt);
  int bytes_needed = vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);

  if (bytes_needed >= static_cast<int>(sizeof(buf))) {
    // Too large for our stack buffer, so we need to allocate.
    result.resize(bytes_needed + 1);
    va_start(ap, fmt);
    vsnprintf(&result[0], bytes_needed + 1, fmt, ap);
    va_end(ap);
    result.resize(bytes_needed);
  } else {
    result.assign(buf, bytes_needed);
  }
  return result;
}

std::string GenerateExpressionLabel(const RelationalExpression *expr) {
  vector<Item *> all_join_conditions;
  all_join_conditions.insert(all_join_conditions.end(),
                             expr->equijoin_conditions.begin(),
                             expr->equijoin_conditions.end());
  all_join_conditions.insert(all_join_conditions.end(),
                             expr->join_conditions.begin(),
                             expr->join_conditions.end());

  string label = ItemsToString(all_join_conditions);
  switch (expr->type) {
    case RelationalExpression::MULTI_INNER_JOIN:
    case RelationalExpression::TABLE:
      assert(false);
    case RelationalExpression::INNER_JOIN:
    case RelationalExpression::STRAIGHT_INNER_JOIN:
      break;
    case RelationalExpression::LEFT_JOIN:
      label = "[left] " + label;
      break;
    case RelationalExpression::SEMIJOIN:
      label = "[semi] " + label;
      break;
    case RelationalExpression::ANTIJOIN:
      label = "[anti] " + label;
      break;
    case RelationalExpression::FULL_OUTER_JOIN:
      label = "[full] " + label;
      break;
  }
  return label;
}

namespace {

/// The maximal number of digits we use in decimal numbers (e.g. "123456" or
/// "0.00123").
constexpr int kPlainNumberLength = 6;

/// The maximal number of digits in engineering format mantissas, e.g.
/// "12.3e+6".
constexpr int kMantissaLength = 3;

/// The  smallest number (absolute value) that we do not format as "0".
constexpr double kMinNonZeroNumber = 1.0e-12;

/// For decimal numbers, include enough decimals to ensure that any rounding
/// error is less than `<number>*10^kLogPrecision` (i.e. less than 1%).
constexpr int kLogPrecision = -2;

/// The smallest number (absolute value) that we format as decimal (rather than
/// engineering format).
const double kMinPlainFormatNumber =
    std::pow(10, 1 - kPlainNumberLength - kLogPrecision);

/// Find the number of integer digits (i.e. those before the decimal point) in
/// 'd' when represented as a decimal number.
int IntegerDigits(double d) {
  return d == 0.0 ? 1
                  : std::max(1, 1 + static_cast<int>(
                                        std::floor(std::log10(std::abs(d)))));
}

/**
   Format 'd' as a decimal number with enough decimals to get a rounding error
   less than d*10^log_precision, without any trailing fractional zeros.
*/
std::string DecimalFormat(double d, int log_precision) {
  assert(d != 0.0);
  constexpr int max_digits = 18;
  assert(IntegerDigits(d + 0.5) <= max_digits);

  // The position of the first nonzero digit, relative to the decimal point.
  const int first_nonzero_digit_pos =
      static_cast<int>(std::floor(std::log10(std::abs(d))));

  // The number of decimals needed for the required precision.
  const int decimals = std::max(0, -log_precision - first_nonzero_digit_pos);

  // Add space for sign, decimal point and zero termination.
  char buff[max_digits + 3];
  // NOTE: We cannot use %f, since MSVC and GCC round 0.5 in different
  // directions, so tests would not be reproducible between platforms.
  // Format/round using my_fcvt() instead.
  my_fcvt(d, decimals, buff, nullptr);
  if (strchr(buff, '.') == nullptr) {
    return buff;
  } else {
    // Remove trailing fractional zeros.
    return std::regex_replace(buff, std::regex("[.]?0+$"), "");
  }
}

/**
   Format 'd' in engineering format, i.e. `<mantissa>e<sign><exponent>`
   where 1.0<=mantissa<1000.0 and exponent is a multiple of 3.
*/
std::string EngineeringFormat(double d) {
  assert(d != 0.0);
  int exp = std::floor(std::log10(std::abs(d)) / 3.0) * 3;
  double mantissa = d / std::pow(10.0, exp);
  std::ostringstream stream;

  if (mantissa + 0.5 * std::pow(10, 3 - kMantissaLength) < 1000.0) {
    stream << DecimalFormat(mantissa, 1 - kMantissaLength) << "e"
           << std::showpos << exp;
  } else {
    // Cover the case where the mantissa will be rounded up to give an extra
    // digit. For example, if d=999500000 and kMantissaLength=3, we want it to
    // be formatted as "1e+9" rather than "1000e+6".
    stream << DecimalFormat(mantissa / 1000.0, 1 - kMantissaLength) << "e"
           << std::showpos << exp + 3;
  }
  return stream.str();
}

/// Integer exponentiation.
uint64_t constexpr Power(uint64_t base, int power) {
  assert(power >= 0);
  uint64_t result = 1;
  for (int i = 0; i < power; i++) {
    result *= base;
  }
  return result;
}

}  // Anonymous namespace.

std::string FormatNumberReadably(double d) {
  if (std::abs(d) < kMinNonZeroNumber) {
    return "0";
  } else if (std::abs(d) < kMinPlainFormatNumber ||
             IntegerDigits(d + 0.5) > kPlainNumberLength) {
    return EngineeringFormat(d);
  } else {
    return DecimalFormat(d, kLogPrecision);
  }
}

std::string FormatNumberReadably(uint64_t l) {
  constexpr uint64_t limit = Power(10, kPlainNumberLength);
  if (l >= limit) {
    return EngineeringFormat(l);
  } else {
    return std::to_string(l);
  }
}

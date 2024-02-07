/*
  Copyright (c) 2019, 2024, Oracle and/or its affiliates.

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
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef MYSQL_HARNESS_CONFIG_OPTION_INCLUDED
#define MYSQL_HARNESS_CONFIG_OPTION_INCLUDED

#include <charconv>  // from_chars
#include <chrono>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>

#include "harness_export.h"

namespace mysql_harness {

double HARNESS_EXPORT
option_as_double(const std::string &value, const std::string &option_desc,
                 double min_value = 0,
                 double max_value = std::numeric_limits<double>::max());

/**
 * Gets an integer using the given option value.
 *
 * Gets an integer using the given option value. The type can be
 * any integer type such as uint16_t, int8_t and bool.
 *
 * The min_value argument can be used to set a minimum value for
 * the option. For example, when 0 (zero) is not allowed, min_value
 * can be set to 1. The maximum value is whatever the maximum of the
 * use type is.
 *
 * Throws std::invalid_argument on errors.
 *
 * @param value Option value
 * @param option_desc Option name
 * @param min_value Minimum value
 * @param max_value Maximum value
 * @return value read from the configuration
 */
template <typename T>
T option_as_int(const std::string_view &value, const std::string &option_desc,
                T min_value = std::numeric_limits<T>::min(),
                T max_value = std::numeric_limits<T>::max()) {
  const char *start = value.data();
  const char *end = start + value.size();

  // from_chars has no support for <bool>, map it to uint8_t
  using integral_type = std::conditional_t<std::is_same_v<T, bool>, uint8_t, T>;

  integral_type int_result;
  const auto [ptr, ec]{std::from_chars(start, end, int_result)};

  if (ptr == end && ec == std::errc{}) {
    // before comparing, cast back to the target_type
    //
    // without the cast, MSVC warns: warning C4804: '<=': unsafe use of type
    // 'bool' in operation
    if (int_result <= static_cast<integral_type>(max_value) &&
        int_result >= static_cast<integral_type>(min_value)) {
      return int_result;
    }
  }

  throw std::invalid_argument(option_desc + " needs value between " +
                              std::to_string(min_value) + " and " +
                              std::to_string(max_value) + " inclusive, was '" +
                              std::string(value) + "'");
}

/**
 * Get a unsigned integer.
 *
 * use option_as_int<T> instead.
 */
template <typename T>
T option_as_uint(const std::string_view &value, const std::string &option_desc,
                 T min_value = std::numeric_limits<T>::min(),
                 T max_value = std::numeric_limits<T>::max()) {
  return option_as_int<T>(value, option_desc, min_value, max_value);
}

template <typename T>
class IntOption {
 public:
  constexpr IntOption(T min_value = std::numeric_limits<T>::min(),
                      T max_value = std::numeric_limits<T>::max())
      : min_value_{min_value}, max_value_{max_value} {}

  T operator()(const std::string &value, const std::string &option_desc) {
    return mysql_harness::option_as_int(value, option_desc, min_value_,
                                        max_value_);
  }

 private:
  T min_value_;
  T max_value_;
};

class StringOption {
 public:
  std::string operator()(const std::string &value,
                         const std::string & /* option_desc */) {
    return value;
  }
};

class BoolOption {
 public:
  bool operator()(const std::string &value, const std::string &option_desc) {
    if (value == "true" || value == "1") return true;
    if (value == "false" || value == "0") return false;

    throw std::invalid_argument(
        option_desc + " needs a value of either 0, 1, false or true, was '" +
        value + "'");
  }
};

template <typename V>
class FloatingPointOption {
 public:
  using value_type = V;

  FloatingPointOption(
      value_type min_value = 0,
      value_type max_value = std::numeric_limits<value_type>::max())
      : min_value_{min_value}, max_value_{max_value} {}

  value_type operator()(const std::string &value,
                        const std::string &option_desc) {
    return mysql_harness::option_as_double(value, option_desc, min_value_,
                                           max_value_);
  }

 private:
  value_type min_value_;
  value_type max_value_;
};

using DoubleOption = FloatingPointOption<double>;

template <typename Dur>
class DurationOption : public DoubleOption {
 public:
  using duration_type = Dur;
  using __base = DoubleOption;

  using __base::__base;

  duration_type operator()(const std::string &value,
                           const std::string &option_desc) {
    double result = __base::operator()(value, option_desc);

    return std::chrono::duration_cast<duration_type>(
        std::chrono::duration<double>(result));
  }
};

/**
 * a double option with milli-second precision.
 *
 * input is seconds as double.
 * output is a std::chrono::millisecond
 */
using MilliSecondsOption = DurationOption<std::chrono::milliseconds>;

}  // namespace mysql_harness
#endif

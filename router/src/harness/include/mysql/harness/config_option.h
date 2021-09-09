/*
  Copyright (c) 2019, 2021, Oracle and/or its affiliates.

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

#ifndef MYSQL_HARNESS_CONFIG_OPTION_INCLUDED
#define MYSQL_HARNESS_CONFIG_OPTION_INCLUDED

#include <limits>
#include <sstream>
#include <string>
#include <system_error>

#include "mysql/harness/config_parser.h"
#include "mysql/harness/stdx/attribute.h"  // STDX_NONNULL
#include "mysql/harness/stdx/expected.h"

namespace mysql_harness {
enum class option_errc {
  empty = 1,
  not_found,
};
}  // namespace mysql_harness

namespace std {
template <>
struct is_error_code_enum<mysql_harness::option_errc> : public std::true_type {
};
}  // namespace std

namespace mysql_harness {
inline const std::error_category &option_category() noexcept {
  class option_category_impl : public std::error_category {
   public:
    const char *name() const noexcept override { return "option"; }
    std::string message(int ev) const override {
      switch (static_cast<option_errc>(ev)) {
        case option_errc::empty:
          return "needs a value";
        case option_errc::not_found:
          return "not found";
        default:
          return "unknown";
      }
    }
  };

  static option_category_impl instance;
  return instance;
}

inline std::error_code make_error_code(option_errc e) noexcept {
  return std::error_code(static_cast<int>(e), option_category());
}

class ConfigOption {
 public:
  ConfigOption(std::string_view name, std::string_view default_value)
      : name_{std::move(name)},
        is_required_{false},
        default_value_{std::move(default_value)} {
    if (name_.empty()) {
      throw std::invalid_argument("expected 'name' to be non-empty");
    }
  }

  explicit ConfigOption(std::string_view name)
      : name_{std::move(name)}, is_required_{true} {
    if (name.empty()) {
      throw std::invalid_argument("expected 'name' to be non-empty");
    }
  }

  STDX_NONNULL stdx::expected<std::string, std::error_code> get_option_string(
      const mysql_harness::ConfigSection *section) const {
    std::string value;
    try {
      value = section->get(name_);
    } catch (const mysql_harness::bad_option &) {
      if (is_required()) {
        return stdx::make_unexpected(make_error_code(option_errc::not_found));
      }
    }

    if (value.empty()) {
      if (is_required()) {
        return stdx::make_unexpected(make_error_code(option_errc::empty));
      } else {
        value = default_value_;
      }
    }

    return value;
  }

  std::string name() const { return name_; }
  bool is_required() const { return is_required_; }
  std::string default_value() const { return default_value_; }

 private:
  const std::string name_;
  const bool is_required_;
  const std::string default_value_;
};

/** @brief Gets an unsigned integer using the given option value
 *
 * Gets an unsigned integer using the given option value. The type can be
 * any unsigned integer type such as uint16_t.
 *
 * The min_value argument can be used to set a minimum value for
 * the option. For example, when 0 (zero) is not allowed, min_value
 * can be set to 0. The maximum value is whatever the maximum of the
 * use type is.
 *
 * Throws std::invalid_argument on errors.
 *
 * @param value Option value
 * @param option_name Option name
 * @param min_value Minimum value
 * @param max_value Maximum value
 * @return value read from the configuration
 */
template <typename T>
T option_as_uint(const std::string &value, const std::string &option_name,
                 T min_value = 0, T max_value = std::numeric_limits<T>::max()) {
  static_assert(std::numeric_limits<T>::max() <=
                std::numeric_limits<unsigned long long>::max());

  // strtoul[l] allows negative values and silently negates them to positive
  // values:
  //
  // -1 -> 18446744073709551615
  //
  // what we don't want.
  //
  // Fail, if the value param starts with ^\s*-

  // skip WS
  const char *start = value.c_str();
  for (; ::isspace(*start); ++start)
    ;

  if (*start != '-') {
    // behaviour differs on empty value:
    //
    // maxosx sets: returns 0, *rest == value.c_str(), sets errno=EINVAL
    // linux sets: returns 0, *rest == value.c_str()
    char *rest;
    errno = 0;  // reset errno to see ERANGE.
    const unsigned long long toul = std::strtoull(start, &rest, 10);
    const T result = static_cast<T>(toul);

    if (start != rest && *rest == '\0' && result <= max_value &&
        result >= min_value && result == toul && errno == 0) {
      return result;
    }
  }

  std::ostringstream os;
  os << option_name << " needs value between " << std::to_string(min_value)
     << " and " << std::to_string(max_value) << " inclusive, was '" << value
     << "'";
  throw std::invalid_argument(os.str());
}

}  // namespace mysql_harness
#endif

/*
  Copyright (c) 2020, 2022, Oracle and/or its affiliates.

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
#ifndef MYSQL_HARNESS_STDX_FILESYSTEM_INCLUDED
#define MYSQL_HARNESS_STDX_FILESYSTEM_INCLUDED

#include <string>
#include <system_error>
#include <type_traits>

#include "mysql/harness/stdx_export.h"

// std::filesystem from C++17 on top of C++14

namespace stdx {
namespace filesystem {
class path {
 public:
  using value_type = char;
  using string_type = std::basic_string<value_type>;

  path() = default;

  path(string_type source) : native_path_{std::move(source)} {}

  path(const path &p) = default;
  path(path &&p) = default;

  const value_type *c_str() const noexcept { return native().c_str(); }
  const string_type &native() const noexcept { return native_path_; }

  operator string_type() const { return native_path_; }

 private:
  string_type native_path_;
};

/**
 * get current path.
 *
 * @throws std::system_error on error
 *
 * @returns current path
 */
HARNESS_STDX_EXPORT path current_path();

/**
 * get current path.
 *
 * sets ec on error.
 *
 * @returns current path
 */
HARNESS_STDX_EXPORT path current_path(std::error_code &ec) noexcept;

HARNESS_STDX_EXPORT bool remove(const path &p, std::error_code &ec) noexcept;

}  // namespace filesystem
}  // namespace stdx

#endif

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

#ifndef MYSQL_STRCONV_ENCODE_STRING_COUNTER_H
#define MYSQL_STRCONV_ENCODE_STRING_COUNTER_H

/// @file
/// Experimental API header

#include <string_view>                           // string_view
#include "mysql/strconv/encode/string_target.h"  // String_target_interface

/// @addtogroup GroupLibsMysqlStrconv
/// @{

namespace mysql::strconv {

/// Class that serves as the target for encode(..., Is_string_target), which
/// never writes anything and only stores the size.
class String_counter : public detail::String_target_interface<String_counter> {
 protected:
  /// Construct a new object.
  ///
  /// This is hidden from user code. These object are only meant to be created
  /// internally by the framework.
  String_counter() = default;

 public:
  static constexpr Target_type target_type = Target_type::counter;

  // Allow move, but not copy.
  //
  // Deleting copy semantics protects against the mistake of making a
  // `encode_impl` function take its second parameter by value.
  String_counter(const String_counter &) = delete;
  String_counter(String_counter &&) noexcept = default;
  String_counter &operator=(const String_counter &) = delete;
  String_counter &operator=(String_counter &&) noexcept = default;
  ~String_counter() = default;

  /// Increment the size by `sv.size()`.
  void write_raw(const std::string_view &sv) { advance(sv.size()); }

  /// Increment the size by 1.
  void write_char(int) { advance(1); }

  /// Increment the size by the size of the given object.
  void write(const Is_format auto &format, const auto &object) {
    resolve_format_and_write(format, object);
  }

  /// Increment the size by the size of the given string. This overload enables
  /// writing string literals directly.
  void write(const Is_format auto &format, const std::string_view &sv) {
    resolve_format_and_write(format, sv);
  }

  /// Increment the size by `size`.
  void advance(std::size_t size) { m_size += size; }

  /// Return the current size.
  [[nodiscard]] std::size_t size() const { return m_size; }

 private:
  /// The current size.
  std::size_t m_size{0};
};  // class String_counter

}  // namespace mysql::strconv

namespace mysql::strconv::detail {

/// String_counter subclass that can be instantiated.
///
/// We hide this in the detail namespace because the class is not supposed to be
/// instantiated in user code, only in this framework.
class Constructible_string_counter : public String_counter {
 public:
  Constructible_string_counter() = default;
};

static_assert(!std::copy_constructible<Constructible_string_counter>);
static_assert(!std::copy_constructible<String_counter>);
static_assert(std::movable<Constructible_string_counter>);
static_assert(std::movable<String_counter>);

}  // namespace mysql::strconv::detail

// addtogroup GroupLibsMysqlStrconv
/// @}

#endif  // ifndef MYSQL_STRCONV_ENCODE_STRING_COUNTER_H

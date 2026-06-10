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

#ifndef MYSQL_STRCONV_ENCODE_STRING_TARGET_H
#define MYSQL_STRCONV_ENCODE_STRING_TARGET_H

/// @file
/// Experimental API header

#include <concepts>                                // derived_from
#include "mysql/strconv/encode/concat_object.h"    // Concat_object
#include "mysql/strconv/formats/resolve_format.h"  // resolve_format

/// @addtogroup GroupLibsMysqlStrconv
/// @{

namespace mysql::strconv {
// Forward declarations
class String_counter;
class String_writer;
}  // namespace mysql::strconv

namespace mysql::strconv::detail {

/// Top of the hierarchy.
class String_target_base {};

/// True if `encode_impl` can be invoked with the given format type and
/// object type.
template <class Format_t, class Object_t>
concept Can_invoke_encode_impl =
    requires(Format_t format, String_counter counter, String_writer writer,
             Object_t obj) {
      encode_impl(format, counter, obj);
      encode_impl(format, writer, obj);
    };

/// CRTP base class providing common helpers needed by `String_writer` and
/// `String_counter`, namely, the function to resolve the format.
template <class Self_tp>
class String_target_interface : public String_target_base {
 private:
  using Self_t = Self_tp;

  /// Helper type predicate used by detail::resolve_format. It has the static
  /// constexpr bool member variable `value` which is true if encode_impl has
  /// been defined for the given Format and Object.
  ///
  /// @tparam Format_t Format to test.
  ///
  /// @tparam Object_t Object type to test.
  template <class Format_t, class Object_t>
  struct Can_invoke_encode_impl_pred
      : public std::bool_constant<Can_invoke_encode_impl<Format_t, Object_t>> {
  };

 public:
  /// Depending on the subclass, write or compute the size of multiple objects
  /// to this String_target.
  template <class... Args_t>
  void concat(const Is_format auto &format, const Args_t &...args) {
    self().write(format, Concat_object<Args_t...>(args...));
  }

 protected:
  /// Resolve the format, using the rules to deduce format based on default
  /// format and parent format, and write the given object using the resolved
  /// format.
  template <class Object_t>
  void resolve_format_and_write(const Is_format auto &format,
                                const Object_t &object) {
    encode_impl(resolve_format<Conversion_direction::encode,
                               Can_invoke_encode_impl_pred>(format, object),
                self(), object);
  }

 private:
  [[nodiscard]] Self_t &self() { return static_cast<Self_t &>(*this); }
};  // class String_target_interface

}  // namespace mysql::strconv::detail

namespace mysql::strconv {

// The type of string target.
// NOLINTNEXTLINE(performance-enum-size): silence clang-tidy's pointless hint
enum class Target_type { counter, writer };

/// Concept that holds for String_counter and String_writer.
template <class Test>
concept Is_string_target = std::derived_from<Test, detail::String_target_base>;

}  // namespace mysql::strconv

// addtogroup GroupLibsMysqlStrconv
/// @}

#endif  // ifndef MYSQL_STRCONV_ENCODE_STRING_TARGET_H

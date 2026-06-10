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

#ifndef MYSQL_STRCONV_FORMATS_RESOLVE_FORMAT_H
#define MYSQL_STRCONV_FORMATS_RESOLVE_FORMAT_H

/// @file
/// Experimental API header
///
/// This defines the internal function detail::resolve_format, which is used
/// to find the correct Format class to use for formatting or parsing.

#include <cassert>                         // assert
#include <concepts>                        // same_as
#include <utility>                         // declval
#include "mysql/strconv/formats/format.h"  // Is_format

/// @addtogroup GroupLibsMysqlStrconv
/// @{

namespace mysql::strconv::detail {

/// True if there is a default format type defined for <Format_t, Object_t>.
template <class Format_t, class Object_t>
concept Has_default_format =  // this comment helps clang-format
    requires(Format_t format, Object_t object) {
      { get_default_format(format, object) } -> Is_format;
    };

/// True if there is a parent format type defined for Format_t.
template <class Format_t>
concept Has_parent_format = requires(Format_t format) { format.parent(); };

/// The type of the default format for <Format_t, Object_t>.
template <class Format_t, class Object_t>
using Default_format_type = decltype(get_default_format(
    std::declval<Format_t>(), std::declval<Object_t>()));

/// True if Func_t::call(Format_t, Object_t) is defined.
template <template <class, class> class Can_invoke_t, class Format_t,
          class Object_t>
concept Has_impl = Can_invoke_t<Format_t, Object_t>::value;

/// True if <Format_t, Object_t> has a default format and Func_t::call is
/// defined for <Default_format_type<Format_t, Object_t>, Object_t>.
template <template <class, class> class Can_invoke_t, class Format_t,
          class Object_t>
concept Has_default_impl =
    Has_default_format<Format_t, Object_t> &&
    Has_impl<Can_invoke_t, Default_format_type<Format_t, Object_t>, Object_t>;

/// Return the format to pass to the implementation function, given the format
/// and object type passed by the user to the API. The three overloads,
/// together, have the effect of using the encode_impl with exactly the
/// requested Format_t type if it exists; otherwise, use the encode_impl
/// using exactly the default format if it exists; otherwise, recursively,
/// perform the same checks for the parent format. Returns void if no
/// implementation is found.
///
/// @tparam Can_invoke_t Two-argument type predicate. When instantiated with a
/// format type and an object type, it should have the member `value` equal to
/// true if there is an implementation of `encode_impl`/`decode_impl`for
/// that combination of arguments.
///
/// @tparam Object_t Type of object to check.
///
/// @tparam Format_t Type of format to check.
///
/// @param format Format to check.
///
/// @param object Object to check.
///
/// return: If there is an implementation for `Format_t`, returns `format`.
/// Otherwise, if there is an implementation for the default format, returns the
/// default format. Otherwise, if there is a parent format, recursively checks
/// that. If no format is found this way, the return type is void.
template <template <class, class> class Can_invoke_t, class Object_t,
          Is_format Format_t>
constexpr void do_resolve_format(const Format_t &format [[maybe_unused]],
                                 const Object_t &object [[maybe_unused]]) {
  assert(false);
}

template <template <class, class> class Can_invoke_t, class Object_t,
          Is_format Format_t>
  requires Has_impl<Can_invoke_t, Format_t, Object_t>
constexpr Format_t do_resolve_format(const Format_t &format,
                                     const Object_t &object [[maybe_unused]]) {
  return format;
}

template <template <class, class> class Can_invoke_t, class Object_t,
          Is_format Format_t>
  requires(!Has_impl<Can_invoke_t, Format_t, Object_t> &&
           Has_default_impl<Can_invoke_t, Format_t, Object_t>)
constexpr auto do_resolve_format(const Format_t &format,
                                 const Object_t &object) {
  return get_default_format(format, object);
}

template <template <class, class> class Can_invoke_t, class Object_t,
          Is_format Format_t>
  requires(!Has_impl<Can_invoke_t, Format_t, Object_t> &&
           !Has_default_impl<Can_invoke_t, Format_t, Object_t> &&
           Has_parent_format<Format_t>)
constexpr auto do_resolve_format(const Format_t &format,
                                 const Object_t &object) {
  return do_resolve_format<Can_invoke_t>(format.parent(), object);
}

/// The return type of `do_resolve_format`.
template <template <class, class> class Can_invoke_t, class Format_t,
          class Object_t>
using Resolved_format_type = decltype(do_resolve_format<Can_invoke_t>(
    std::declval<Format_t>(), std::declval<Object_t>()));

// NOLINTNEXTLINE(performance-enum-size): silence clang-tidy's pointless hint
enum class Conversion_direction { decode, encode };

/// Return the format to pass to the implementation function, given the format
/// and object type passed by the user to the API. This checks for
/// implementations for the format itself, its default format, and the parent
/// format(s). Fails to compile if no implementation is found.
///
/// @tparam Can_invoke_t Two-argument type predicate. When instantiated with a
/// format type and an object type, it should have the member `value` equal to
/// true if there is an implementation of `encode_impl`/`decode_impl`for
/// that combination of format/object arguments.
///
/// @tparam Object_t Type of object to check.
///
/// @tparam Format_t Type of format to check.
///
/// @param format Format to check.
///
/// @param object Object to check.
///
/// @return If there is an implementation for `Format_t`, returns `format`.
/// Otherwise, if there is an implementation for the default format, returns the
/// default format. Otherwise, if there is a parent format, recursively checks
/// that. If no format is found this way, generates a compilation error.
template <Conversion_direction conversion_direction,
          template <class, class> class Can_invoke_t, class Object_t,
          Is_format Format_t>
constexpr auto resolve_format(const Format_t &format, const Object_t &object) {
  using Return_t = Resolved_format_type<Can_invoke_t, Format_t, Object_t>;
  constexpr bool can_resolve = !std::same_as<Return_t, void>;
  if constexpr (can_resolve) {
    return do_resolve_format<Can_invoke_t>(format, object);
  } else {
// static_assert accepts only string literals for the message, so when we
// need to compute a message it can only be implemented using a macro.
// NOLINTBEGIN(cppcoreguidelines-macro-usage)
#define RESOLVE_FORMAT_ERROR_MESSAGE(TOFROM, PROTOTYPE_ARGS)                  \
  "No `mysql::strconv::" TOFROM                                               \
  "_string_impl` for `Format_t` and "                                         \
  "`Object_t`, because "                                                      \
  "(1) There is no direct implementation defined with the prototype "         \
  "`mysql::strconv::" TOFROM "_string_impl(const Format_t &, " PROTOTYPE_ARGS \
  ") (or there are more than one, ambiguously); and "                         \
  "(2) there is no default format or no implementation for the "              \
  "default format, i.e., either "                                             \
  "`get_default_format(const Format_t &, const Object_t &)` "                 \
  "is not defined or `mysql::strconv::" TOFROM                                \
  "_string_impl(const /*default format*/ &, " PROTOTYPE_ARGS                  \
  ")` is not defined (or is defined more than once, ambiguously); and "       \
  "(3) there is no parent format or no implementation for the parent "        \
  "format, i.e., either `Format_t::parent()` is not defined or "              \
  "(1), (2), (3) fail also when replacing `Format_t` by "                     \
  "`decltype(Format_t::parent())`. "                                          \
  "If you believe you have implemented the necessary functions, maybe "       \
  "there are mistakes in their prototypes; to debug this, double-check "      \
  "the namespace, `const` and `&` qualifiers, and/or try to invoke `" TOFROM  \
  "_string_impl` directly and check the compilation error."
    // NOLINTEND(cppcoreguidelines-macro-usage)

    // clang-tidy evaluates this even if it shouldn't.
    // NOLINTBEGIN
    if constexpr (conversion_direction == Conversion_direction::encode) {
      static_assert(  // this comment helps clang-format
          can_resolve, RESOLVE_FORMAT_ERROR_MESSAGE(
                           "to", "Is_string_target auto &, const Object_t &"));
    } else {
      static_assert(  // this comment helps clang-format
          can_resolve,
          RESOLVE_FORMAT_ERROR_MESSAGE("from", "Parser &, Object_t &"));
    }
    static_assert(Has_impl<Can_invoke_t, Format_t, Object_t>);
    static_assert(Has_default_impl<Can_invoke_t, Format_t, Object_t>);
    static_assert(Has_parent_format<Format_t>);
    // NOLINTEND
#undef RESOLVE_FORMAT_ERROR_MESSAGE
  }
}

}  // namespace mysql::strconv::detail

// addtogroup GroupLibsMysqlStrconv
/// @}

#endif  // ifndef MYSQL_STRCONV_FORMATS_RESOLVE_FORMAT_H

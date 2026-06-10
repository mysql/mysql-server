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
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA.

#ifndef MYSQL_UTILS_CALL_AND_CATCH_H
#define MYSQL_UTILS_CALL_AND_CATCH_H

/// @file
/// Experimental API header

#include <concepts>                     // invocable
#include <functional>                   // reference_wrapper
#include <optional>                     // optional
#include <type_traits>                  // conditional_t
#include "mysql/utils/return_status.h"  // Return_status

/// @addtogroup GroupLibsMysqlUtils
/// @{

namespace mysql::utils {

/// The return type for any call_and_catch(f, args...) call where f(args...)
/// returns Type.
/// @cond DOXYGEN_DOES_NOT_UNDERSTAND_THIS
template <class Type>
using Call_and_catch_type = std::conditional_t<
    std::same_as<Type, void>,
    Return_status,  // void -> Return_status
    std::optional<std::conditional_t<
        std::is_reference_v<Type>,
        // T & -> optional<reference_wrapper<T>>
        std::reference_wrapper<std::remove_reference_t<Type>>,
        // T -> optional<T>
        Type>>>;
/// @endcond

/// Calls a function, catches exceptions from it, and wraps the exception status
/// in the return value.
///
/// @param function Any function, possibly throwing.
///
/// @param args Parameters passed to the function.
///
/// @return If `function` is declared `noexcept`, this is equivalent to calling
/// `function` directly and returning the result. Otherwise, if `function`
/// returns void, this function returns mysql::utils::Return_status: `error`
/// indicates out-of-memory and `ok` indicates success. Otherwise, if `function`
/// returns a non-reference type, say `T`, this function returns
/// `std::optional<T>`, which stores the return value on success, and uses no
/// value on out-of-memory. Otherwise, `function` returns a reference type, say
/// `T &`, this function returns `std::optional<std::reference_wrapper<T>>`,
/// which holds a wrapper around the returned reference on success, and holds no
/// value on error.
template <class Function_t, class... Args_t>
  requires std::invocable<Function_t, Args_t...>
[[nodiscard]] decltype(auto) call_and_catch(const Function_t &function,
                                            Args_t &&...args) noexcept {
  auto call_function = [&]() -> decltype(auto) {
    return function(std::forward<Args_t>(args)...);
  };
  using Return_t = decltype(call_function());
  if constexpr (noexcept(function(std::forward<Args_t>(args)...))) {
    return call_function();
  } else if constexpr (std::same_as<Return_t, void>) {
    try {
      call_function();
      return Return_status::ok;
    } catch (...) {
      return Return_status::error;
    }
  } else {
    try {
      if constexpr (std::is_reference_v<Return_t>)
        return std::make_optional(std::ref(call_function()));
      else
        return std::make_optional(call_function());
    } catch (...) {
      return Call_and_catch_type<Return_t>();
    }
  }
}

/// Whether `conditional_call_and_catch` should be enabled or not.
// NOLINTNEXTLINE(performance-enum-size): silence clang-tidy's pointless hint
enum class Shall_catch { no, yes };

/// Call `function`, and if `shall_catch` is true, catch exceptions and wrap
/// them in the return value. Otherwise, just call the function and return what
/// the function returns.
///
/// @see call_and_catch
template <Shall_catch shall_catch, class Function_t, class... Args_t>
[[nodiscard]] decltype(auto) conditional_call_and_catch(
    const Function_t &function,
    Args_t &&...args)  // this comment helps clang-format
    noexcept(noexcept(function(std::forward<Args_t>(args)...)) ||
             shall_catch == Shall_catch::yes) {
  if constexpr (shall_catch == Shall_catch::yes) {
    return call_and_catch(function, std::forward<Args_t>(args)...);
  } else {
    return function(std::forward<Args_t>(args)...);
  }
}

/// Helper macro to define a function that returns the result of a single
/// expression, and has a conditional noexcept clause deduced from whether that
/// expression is noexcept or not. This expands to
/// `noexcept(noexcept(X)) { return (X); }`.
///
/// @code
/// class C { /*...*/ };
///
/// // for rvalue reference C, f never throws
/// void f(C &&) noexcept {}
///
/// // for const lvalue reference C, f may throw (maybe it needs to allocate)
/// void f(const C &) {}
///
/// // Call f(t). If f(t) may throw, catch any exceptions and return
/// // Call_and_catch_type<decltype(f(t))>. If f(t) is noexcept, just return
/// // what f(t) returned.
/// template <class T>
/// auto wrap_f(T &&t) {
///   return call_and_catch(
///     [&]() DEDUCED_NOEXCEPT_FUNCTION(f(std::forward<T>(t))));
/// }
/// @endcode
///
/// See also Boost's BOOST_HOF_RETURNS.
// Can't avoid macro here
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define DEDUCED_NOEXCEPT_FUNCTION(X) \
  noexcept(noexcept(X)) { return (X); }

}  // namespace mysql::utils

// addtogroup GroupLibsMysqlUtils
/// @}

#endif  // ifndef MYSQL_UTILS_CALL_AND_CATCH_H

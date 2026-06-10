// Copyright (c) 2023, 2026, Oracle and/or its affiliates.
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

#ifndef MYSQL_UTILS_RETURN_STATUS_H
#define MYSQL_UTILS_RETURN_STATUS_H

/// @file
/// Experimental API header

/// @addtogroup GroupLibsMysqlUtils
/// @{

#include <concepts>  // invocable
#include <utility>   // forward

namespace mysql::utils {

/// @brief Simple, strongly-typed enumeration to indicate internal status:
/// ok, error
enum class Return_status {
  ok,     ///< operation succeeded
  error,  ///< operation failed
};

// A default-constructed return status should be 'ok'.
static_assert(Return_status{} == Return_status::ok);

/// Helper that calls the given function and returns its result, or returns
/// `Return_status::ok` if the function returns `void`.
///
/// Use case: Suppose a function `f` has two overloads: one that can fail and
/// one that cannot fail. Then the overload that can fail should return
/// `Return_status` and have the `[[nodiscard]]` attribute and the overload that
/// cannot fail should return void, as follows:
///
/// @code
/// void f(/*args...*/) { this overload cannot fail }
/// [[nodiscard]] Return_status f(/*args...*/) { this overload can fail }
/// @code
///
/// Then, a function template `w1` that invokes `f` and forwards the
/// return status (`w1` is a "wrapper"), can be defined like:
///
/// @code
/// [[nodiscard]] auto w1(/*args...*/) {
///   ...
///   return w1(/*args...*/);
/// }
/// @endcode
///
/// This makes `w1` inherit both the return type and the  `[[nodiscard]]`
/// attribute from the overload of `f` that it invokes, since `[[nodiscard]]` is
/// automatically dropped when the return type is deduced to `void`.
///
/// (If the non-failing overload of `f` would return `Return_status::ok` always,
/// `w1` would return `Return_status::ok` always, and have the `[[nodiscard]]`
/// attribute always, which could force callers to check the return value even
/// in cases it is known to always be `ok`).
///
/// Now, `void_to_ok` is usable in cases where a function template `w2` needs to
/// invoke `f` and forward the return status, *and* must always return
/// `Return_status`, perhaps because `w2` has error cases that do not depend on
/// errors in the invocation of `f`. For example:
///
/// @code
/// [[nodiscard]] Return_status w2(/*args...*/) {
///   ...
///   if (/*condition1*/) return Return_status::ok;
///   ...
///   if (/*condition2*/) return Return_status::error;
///   ...
///   if (/*condition3*/) return void_to_ok([&] { return f(/*args...*/); });
///   ...
/// }
/// @endcode
///
/// @tparam Return_t The return type of this function. If Func_t returns
/// non-void, Func_t's return type must be convertible to Return_t. The default
/// is Return_status.
///
/// @tparam Func_t The type of the function to call.
///
/// @tparam Args_t Types of the arguments.
///
/// @param func Function to call.
///
/// @param args Arguments that will be forwarded to the function.
///
/// @return Return_t: if `func` returns non-void, casts the result to `Return_t`
/// and returns it; otherwise returns a default-constructed `Return_t` value.
template <class Return_t = Return_status, class Func_t, class... Args_t>
  requires std::invocable<Func_t, Args_t...>
[[nodiscard]] Return_t void_to_ok(const Func_t &func, Args_t &&...args) {
  if constexpr (std::same_as<std::invoke_result_t<Func_t, Args_t...>, void>) {
    func(std::forward<Args_t>(args)...);
    return Return_t{};
  } else {
    static_assert(
        std::convertible_to<std::invoke_result_t<Func_t, Args_t...>, Return_t>);
    return func(std::forward<Args_t>(args)...);
  }
}

}  // namespace mysql::utils

/// @}

#endif  // MYSQL_UTILS_RETURN_STATUS_H

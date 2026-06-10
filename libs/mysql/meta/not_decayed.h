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

#ifndef MYSQL_META_NOT_DECAYED_H
#define MYSQL_META_NOT_DECAYED_H

/// @file
/// Experimental API header

#include <concepts>     // same_as
#include <type_traits>  // bool_constant

/// @addtogroup GroupLibsMysqlMeta
/// @{

namespace mysql::meta::detail {

template <class T, class... Args>
struct Is_decayed_helper : public std::false_type {};

template <class T, class U>
struct Is_decayed_helper<T, U>
    : public std::bool_constant<std::same_as<T, std::decay_t<U>>> {};

}  // namespace mysql::meta::detail

namespace mysql::meta {

/// false if `Args` is exactly one type, say `A`, and `std::decay_t<A>` equals
/// `Type`.
///
/// The use case is to constrain constructors taking (variadic) forwarding
/// reference arguments, so they cannot be invoked as copy constructor. Here is
/// a typical example:
///
/// @code
/// template <class T>
/// class Wrapper {
///  public:
///   // Copy constructor
///   Wrapper(const Wrapper &) = default;
///
///   // Forwarding constructor, invoking the constructor of m_wrapped.
///   template <class... Args>
///       requires Not_decayed<Wrapper<T>, Args_t...>
///   Wrapper(Args &&... args) : m_wrapped(std::forward<Args>(args)...) {}
///
///  private:
///   T m_wrapped;
/// };
///
/// Wrapper<int> w1(1);  // invokes forwarding constructor
/// Wrapper<int> w2(w1); // invokes copy constructor
/// @endcode
///
/// Here, we expect that w2's copy constructor is invoked. If we would omit the
/// `Not_decayed` constraint, the forwarding constructor would have been
/// "better" according to C++'s overload resolution rules. The `Not_decayed`
/// constraint excludes the forwarding constructor from the candidates and thus
/// makes the copy constructor be the only viable overload.
///
/// See also https://akrzemi1.wordpress.com/2013/10/10/too-perfect-forwarding/
template <class Type, class... Args>
concept Not_decayed = (!detail::Is_decayed_helper<Type, Args...>::value);

}  // namespace mysql::meta

// addtogroup GroupLibsMysqlMeta
/// @}

#endif  // ifndef MYSQL_META_NOT_DECAYED_H

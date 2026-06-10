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

#ifndef MYSQL_UTILS_TUPLE_FIND_H
#define MYSQL_UTILS_TUPLE_FIND_H

/// @file
/// Experimental API header

#include <tuple>  // tuple

/// @addtogroup GroupLibsMysqlUtils
/// @{

namespace mysql::utils {

/// Primary template for helper struct used to define Tuple_find_index.
///
/// This has the member constant `value` set to the smallest number N >= index
/// such that the type of the Nth component satisfies the predicate, or no such
/// member constant if there is no such component.
template <class Tuple, template <class> class Pred, std::size_t index = 0>
struct Tuple_find_helper {};

/// Specialization of Tuple_find_helper to the case where the component at
/// position `index` satisfies the predicate.
template <class Tuple, template <class> class Pred, std::size_t index>
  requires(index < std::tuple_size_v<Tuple> &&
           Pred<std::tuple_element_t<index, Tuple>>::value)
struct Tuple_find_helper<Tuple, Pred, index>
    : public std::integral_constant<std::size_t, index> {};

/// Specialization of Tuple_find_helper to the case where the component at
/// position `index` does not satisfy the predicate.
template <class Tuple, template <class> class Pred, std::size_t index>
  requires(index < std::tuple_size_v<Tuple> &&
           !Pred<std::tuple_element_t<index, Tuple>>::value)
struct Tuple_find_helper<Tuple, Pred, index>
    : public Tuple_find_helper<Tuple, Pred, index + 1> {};

/// Primary template for helper struct used to define Tuple_find_index.
///
/// This has the member constant `value` set to the number of positions N >=
/// index such that the type of the Nth component satisfies the predicate.
template <class Tuple, template <class> class Pred, std::size_t index = 0>
struct Tuple_count_helper : public std::integral_constant<std::size_t, 0> {};

/// Specialization of Tuple_count_helper to the case where index is in range.
template <class Tuple, template <class> class Pred, std::size_t index>
  requires(index < std::tuple_size_v<Tuple>)
struct Tuple_count_helper<Tuple, Pred, index>
    : public std::integral_constant<
          std::size_t,
          // Recursively compute the value for index + 1.
          Tuple_count_helper<Tuple, Pred, index + 1>::value +
              // Increment by 1 if the predicate matches.
              (Pred<std::tuple_element_t<index, Tuple>>::value ? 1 : 0)> {};

namespace detail {
/// Struct template with with one template argument, having a member type
/// Predicate, which is a type predicate that holds for types that are equal to
/// the template argument of the (outer) struct.
template <class Type1>
struct Is_same_helper {
  template <class Type2>
  using Predicate = std::is_same<Type1, Type2>;
};
}  // namespace detail

/// Index of the first element of the tuple-like type whose type matches the
/// type predicate.
///
/// This is undefined if no element type matches the predicate.
///
/// @tparam Tuple Any tuple-like object
/// (https://en.cppreference.com/w/cpp/utility/tuple/tuple-like).
///
/// @tparam Pred Type predicate, i.e., a template class such that
/// `Pred<T>::value` is a truth value. This may, for example, be a
/// UnaryTypeTrait
/// (https://en.cppreference.com/w/cpp/named_req/UnaryTypeTrait.html)
///
/// @code
/// // The first element type that satisfies is_integral is `int` at index 1.
/// static_assert(
///   Tuple_find_index<std::is_integral, std::tuple<float, int>> == 1);
/// @endcode
template <class Tuple, template <class> class Pred>
constexpr std::size_t Tuple_find_index =
    Tuple_find_helper<Tuple, Pred, 0>::value;

/// The first element type in the tuple-like type that matches the type
/// predicate.
///
/// This is undefined if no element type matches the predicate.
///
/// @tparam Tuple Any tuple-like object
/// (https://en.cppreference.com/w/cpp/utility/tuple/tuple-like).
///
/// @tparam Pred Type predicate, i.e., a template class such that
/// `Pred<T>::value` is a truth value. This may, for example, be a
/// UnaryTypeTrait
/// (https://en.cppreference.com/w/cpp/named_req/UnaryTypeTrait.html)
///
/// @code
/// // The first element type that satisfies is_integral is `short`.
/// static_assert(std::same_as<
///   Tuple_find<std::is_integral, std::tuple<std::string, float, short, int>>,
///   short>);
/// @endcode
template <class Tuple, template <class> class Pred>
using Tuple_find = Tuple_find_helper<Tuple, Pred>::type;

/// True if at least one element type in the tuple-like type mathes the type
/// predicate.
///
/// tparam Tuple Any tuple-like object
/// (https://en.cppreference.com/w/cpp/utility/tuple/tuple-like).
///
/// tparam Pred Type predicate, i.e., a template class such that
/// `Pred<T>::value` is a truth value. This may, for example, be a
/// UnaryTypeTrait
/// (https://en.cppreference.com/w/cpp/named_req/UnaryTypeTrait.html)
///
/// @code
/// // `int` matches is_integral.
/// static_assert(
///   Tuple_has_matching_element_type<std::is_integral,
///                                   std::tuple<std::string, float, int>>);
/// // neither `string` nor `float` matches is_integral.
/// static_assert(
///   !Tuple_has_matching_element_type<std::is_integral,
///                                    std::tuple<std::string, float>>);
/// @endcode
template <class Tuple, template <class> class Pred>
concept Tuple_has_matching_element_type =
    requires { typename Tuple_find_helper<Tuple, Pred, 0>::value; };

/// Return the value of the first component of the tuple-like object whose type
/// matches the given type predicate.
///
/// @tparam Tuple Any tuple-like object
/// (https://en.cppreference.com/w/cpp/utility/tuple/tuple-like).
///
/// @tparam Pred Type predicate, i.e., a template class such that
/// `Pred<T>::value` is a truth value. This may, for example, be a
/// UnaryTypeTrait
/// (https://en.cppreference.com/w/cpp/named_req/UnaryTypeTrait.html)
///
/// @code
/// // The first element whose type satisfies is_integral is the 9 (an int).
/// assert(tuple_find(std::is_integral, std::tuple("x", 2.1, 9)) == 9);
/// @endcode
template <template <class> class Pred, class Tuple>
auto tuple_find(const Tuple &tuple) {
  return std::get<Tuple_find_index<Tuple, Pred>>(tuple);
}

/// True if the tuple has an element of the given type.
///
/// tparam Tuple Any tuple-like object
/// (https://en.cppreference.com/w/cpp/utility/tuple/tuple-like).
///
/// tparam Type The type to look for.
template <class Tuple, class Type>
concept Tuple_has_element_type = Tuple_has_matching_element_type<
    Tuple, detail::Is_same_helper<Type>::template Predicate>;

/// The number of tuple element types that match the given predicate.
///
/// @tparam Tuple Any tuple-like object
/// (https://en.cppreference.com/w/cpp/utility/tuple/tuple-like).
///
/// @tparam Pred Type predicate, i.e., a template class such that
/// `Pred<T>::value` is a truth value. This may, for example, be a
/// UnaryTypeTrait
/// (https://en.cppreference.com/w/cpp/named_req/UnaryTypeTrait.html)
template <class Tuple, template <class> class Pred>
constexpr std::size_t Tuple_matching_element_type_count =
    Tuple_count_helper<Tuple, Pred>::value;

}  // namespace mysql::utils

// addtogroup GroupLibsMysqlUtils
/// @}

#endif  // ifndef MYSQL_UTILS_TUPLE_FIND_H

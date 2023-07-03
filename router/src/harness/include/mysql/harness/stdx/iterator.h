/*
  Copyright (c) 2022, Oracle and/or its affiliates.

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

#ifndef MYSQL_HARNESS_STDX_ITERATOR_H_
#define MYSQL_HARNESS_STDX_ITERATOR_H_

#include <type_traits>

#include "mysql/harness/stdx/type_traits.h"  // remove_cvref_t

// from C++20

namespace stdx {
namespace impl {
template <class T, bool = std::is_array_v<T>>
struct indirectly_readable_traits_array;

template <class T>
struct indirectly_readable_traits_array<T, true> {
  using value_type = std::remove_cv_t<std::remove_extent_t<T>>;
};

template <class T, bool = std::is_object_v<T>>
struct indirectly_readable_traits_pointer {};

template <class T>
struct indirectly_readable_traits_pointer<T, true> {
  using value_type = std::remove_cv_t<T>;
};

template <class T, typename = std::void_t<>>
struct has_value_type : std::false_type {};

template <class T>
struct has_value_type<T,
                      std::void_t<typename stdx::remove_cvref_t<T>::value_type>>
    : std::true_type {};

template <class T, bool = has_value_type<T>::value>
struct indirectly_readable_traits_member_value_type;

template <class T>
struct indirectly_readable_traits_member_value_type<T, true> {
  using value_type = typename T::value_type;
};

template <class T, typename = std::void_t<>>
struct has_element_type : std::false_type {};

template <class T>
struct has_element_type<T, std::void_t<typename T::element_type>>
    : std::true_type {};

template <class T, bool = has_element_type<T>::value>
struct indirectly_readable_traits_member_element_type;

template <class T>
struct indirectly_readable_traits_member_element_type<T, true> {
  using value_type = typename T::element_type;
};

template <class T, typename = std::void_t<>>
struct has_reference : std::false_type {};

template <class T>
struct has_reference<T, std::void_t<typename T::reference>> : std::true_type {};

template <class T, bool = has_reference<T>::value>
struct iter_reference;

template <class T>
struct iter_reference<T, false> {
  using reference = decltype(*std::declval<T &>());
};

template <class T>
struct iter_reference<T, true> {
  using reference = typename T::reference;
};

}  // namespace impl

template <class T, class Enable = void>
struct indirectly_readable_traits {};

template <class T>
struct indirectly_readable_traits<T *, std::enable_if_t<std::is_object_v<T>>>
    : impl::indirectly_readable_traits_pointer<T> {};

template <class T>
struct indirectly_readable_traits<T, std::enable_if_t<std::is_array_v<T>>>
    : impl::indirectly_readable_traits_array<T> {};

template <class T>
struct indirectly_readable_traits<
    T, std::enable_if_t<impl::has_value_type<T>::value>>
    : impl::indirectly_readable_traits_member_value_type<
          stdx::remove_cvref_t<T>> {};

template <class T>
struct indirectly_readable_traits<
    T, std::enable_if_t<impl::has_element_type<T>::value>>
    : impl::indirectly_readable_traits_member_element_type<T> {};

template <class T>
struct indirectly_readable_traits<const T> : indirectly_readable_traits<T> {};

template <class T>
using iter_value_t =
    typename indirectly_readable_traits<stdx::remove_cvref_t<T>>::value_type;

template <class T>
using iter_reference_t =
    typename impl::iter_reference<stdx::remove_cvref_t<T>>::reference;

}  // namespace stdx

#endif

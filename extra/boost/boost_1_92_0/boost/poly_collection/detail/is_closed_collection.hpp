/* Copyright 2024 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * See http://www.boost.org/libs/poly_collection for library home page.
 */

#ifndef BOOST_POLY_COLLECTION_DETAIL_IS_CLOSED_COLLECTION_HPP
#define BOOST_POLY_COLLECTION_DETAIL_IS_CLOSED_COLLECTION_HPP

#if defined(_MSC_VER)
#pragma once
#endif

#include <boost/mp11/algorithm.hpp>
#include <boost/mp11/set.hpp>
#include <boost/poly_collection/detail/is_moveable.hpp>
#include <boost/type_traits/make_void.hpp>
#include <type_traits>

namespace boost{

namespace poly_collection{

namespace detail{

template<typename Model,typename=void>
struct is_closed_collection:std::false_type{};

template<typename Model>
struct is_closed_collection<
  Model,void_t<typename Model::acceptable_type_list>
>:std::true_type
{
  using type_list=typename Model::acceptable_type_list;

  static_assert(
    mp11::mp_is_set<type_list>::value,
    "all types in a closed collection must be distinct");

  static_assert(
    mp11::mp_all_of<type_list,is_moveable>::value,
    "all types of a closed collection must be nothrow move constructible "
    "or else move constructible and move assignable");
};
  
} /* namespace poly_collection::detail */

} /* namespace poly_collection */

} /* namespace boost */

#endif

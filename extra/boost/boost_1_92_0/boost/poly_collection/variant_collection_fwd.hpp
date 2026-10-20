/* Copyright 2024 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * See http://www.boost.org/libs/poly_collection for library home page.
 */

#ifndef BOOST_POLY_COLLECTION_VARIANT_COLLECTION_FWD_HPP
#define BOOST_POLY_COLLECTION_VARIANT_COLLECTION_FWD_HPP

#if defined(_MSC_VER)
#pragma once
#endif

#include <boost/mp11/list.hpp>
#include <memory>

namespace boost{

namespace poly_collection{

namespace detail{
template<typename... Ts> struct variant_model;
}

template<typename TypeList>
using variant_collection_value_type=
  typename mp11::mp_rename<TypeList,detail::variant_model>::value_type;

template<
  typename TypeList,
  typename Allocator=std::allocator<variant_collection_value_type<TypeList>>
>
class variant_collection;

template<typename... Ts>
using variant_collection_of=variant_collection<mp11::mp_list<Ts...>>;

template<typename TypeList,typename Allocator>
bool operator==(
  const variant_collection<TypeList,Allocator>& x,
  const variant_collection<TypeList,Allocator>& y);

template<typename TypeList,typename Allocator>
bool operator!=(
  const variant_collection<TypeList,Allocator>& x,
  const variant_collection<TypeList,Allocator>& y);

template<typename TypeList,typename Allocator>
void swap(
  variant_collection<TypeList,Allocator>& x,
  variant_collection<TypeList,Allocator>& y);

} /* namespace poly_collection */

using poly_collection::variant_collection;
using poly_collection::variant_collection_of;

} /* namespace boost */

#endif

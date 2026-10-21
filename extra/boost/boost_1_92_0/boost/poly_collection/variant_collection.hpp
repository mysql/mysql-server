/* Copyright 2024 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * See http://www.boost.org/libs/poly_collection for library home page.
 */

#ifndef BOOST_POLY_COLLECTION_VARIANT_COLLECTION_HPP
#define BOOST_POLY_COLLECTION_VARIANT_COLLECTION_HPP

#if defined(_MSC_VER)
#pragma once
#endif

#include <boost/poly_collection/variant_collection_fwd.hpp>
#include <boost/mp11/list.hpp>
#include <boost/poly_collection/detail/variant_model.hpp>
#include <boost/poly_collection/detail/poly_collection.hpp>
#include <utility>

namespace boost{

namespace poly_collection{

template<typename TypeList,typename Allocator>
class variant_collection:
  public common_impl::poly_collection<
    mp11::mp_rename<TypeList,detail::variant_model>,Allocator>
{
  using base_type=common_impl::poly_collection<
    mp11::mp_rename<TypeList,detail::variant_model>,Allocator>;

  base_type&       base()noexcept{return *this;}
  const base_type& base()const noexcept{return *this;}

public:
  using base_type::base_type;

  variant_collection()=default;
  variant_collection(const variant_collection& x)=default;
  variant_collection(variant_collection&& x)=default;
  variant_collection& operator=(const variant_collection& x)=default;
  variant_collection& operator=(variant_collection&& x)=default;

  template<typename TL,typename A> 
  friend bool operator==(
    const variant_collection<TL,A>&,const variant_collection<TL,A>&);
};

template<typename TypeList,typename Allocator>
bool operator==(
  const variant_collection<TypeList,Allocator>& x,
  const variant_collection<TypeList,Allocator>& y)
{
  return x.base()==y.base();
}

template<typename TypeList,typename Allocator>
bool operator!=(
  const variant_collection<TypeList,Allocator>& x,
  const variant_collection<TypeList,Allocator>& y)
{
  return !(x==y);
}

// TODO: other rel operators?

template<typename TypeList,typename Allocator>
void swap(
  variant_collection<TypeList,Allocator>& x,
  variant_collection<TypeList,Allocator>& y)
{
  x.swap(y);
}

} /* namespace  */

using poly_collection::variant_collection;

} /* namespace boost */

#endif

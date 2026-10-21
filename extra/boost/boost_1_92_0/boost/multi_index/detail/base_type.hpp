/* Copyright 2003-2025 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * See http://www.boost.org/libs/multi_index for library home page.
 */

#ifndef BOOST_MULTI_INDEX_DETAIL_BASE_TYPE_HPP
#define BOOST_MULTI_INDEX_DETAIL_BASE_TYPE_HPP

#if defined(_MSC_VER)
#pragma once
#endif

#include <boost/config.hpp> /* keep it first to prevent nasty warns in MSVC */
#include <boost/mp11/integral.hpp>
#include <boost/mp11/list.hpp>
#include <boost/mp11/utility.hpp>
#include <boost/multi_index/detail/index_base.hpp>
#include <boost/multi_index/detail/is_index_list.hpp>
#include <boost/static_assert.hpp>

namespace boost{

namespace multi_index{

namespace detail{

/* Mp11 machinery to construct a linear hierarchy of indices out of
 * an index list.
 */

template<typename N,typename Mp11IndexSpecifierList,typename SuperMeta>
using nth_layer_index=typename mp11::mp_at<Mp11IndexSpecifierList,N>::
  template index_class<SuperMeta>::type;

template<int N,typename Value,typename IndexSpecifierList,typename Allocator>
struct nth_layer
{
  using Mp11IndexSpecifierList=detail::mp11_index_list<IndexSpecifierList>;
  using type=mp11::mp_eval_if_c<
    N==mp11::mp_size<Mp11IndexSpecifierList>::value,
    index_base<Value,IndexSpecifierList,Allocator>,
    nth_layer_index,
    mp11::mp_int<N>,
    Mp11IndexSpecifierList,
    nth_layer<N+1,Value,IndexSpecifierList,Allocator>
  >;
};

template<typename Value,typename IndexSpecifierList,typename Allocator>
struct multi_index_base_type:
  nth_layer<0,Value,IndexSpecifierList,Allocator>
{
  BOOST_STATIC_ASSERT(detail::is_index_list<IndexSpecifierList>::value);
};

} /* namespace multi_index::detail */

} /* namespace multi_index */

} /* namespace boost */

#endif

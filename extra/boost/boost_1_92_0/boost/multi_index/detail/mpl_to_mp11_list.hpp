/* Copyright 2003-2025 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * See http://www.boost.org/libs/multi_index for library home page.
 */

#ifndef BOOST_MULTI_INDEX_DETAIL_MPL_TO_MP11_LIST_HPP
#define BOOST_MULTI_INDEX_DETAIL_MPL_TO_MP11_LIST_HPP

#if defined(_MSC_VER)
#pragma once
#endif

#if defined(BOOST_MULTI_INDEX_ENABLE_MPL_SUPPORT)
#include <boost/config.hpp> /* keep it first to prevent nasty warns in MSVC */
#include <boost/mp11/list.hpp>
#define BOOST_MULTI_INDEX_BLOCK_BOOSTDEP_HEADER \
  <boost/mpl/begin_end.hpp>
#include BOOST_MULTI_INDEX_BLOCK_BOOSTDEP_HEADER
#undef BOOST_MULTI_INDEX_BLOCK_BOOSTDEP_HEADER
#define BOOST_MULTI_INDEX_BLOCK_BOOSTDEP_HEADER \
  <boost/mpl/deref.hpp>
#include BOOST_MULTI_INDEX_BLOCK_BOOSTDEP_HEADER
#undef BOOST_MULTI_INDEX_BLOCK_BOOSTDEP_HEADER
#define BOOST_MULTI_INDEX_BLOCK_BOOSTDEP_HEADER \
  <boost/mpl/next.hpp>
#include BOOST_MULTI_INDEX_BLOCK_BOOSTDEP_HEADER
#undef BOOST_MULTI_INDEX_BLOCK_BOOSTDEP_HEADER

namespace boost{

namespace multi_index{

namespace detail{

template<typename First,typename Last,typename... Ts>
struct mpl_to_mp11_list_impl:mpl_to_mp11_list_impl<
  typename mpl::next<First>::type,Last,
  Ts...,typename mpl::deref<First>::type
>{};

template<typename Last,typename... Ts>
struct mpl_to_mp11_list_impl<Last,Last,Ts...>
{
  using type=mp11::mp_list<Ts...>;
};

template<typename TypeList>
using mpl_to_mp11_list=typename mpl_to_mp11_list_impl<
  typename mpl::begin<TypeList>::type,
  typename boost::mpl::end<TypeList>::type
>::type;

} /* namespace multi_index::detail */

} /* namespace multi_index */

} /* namespace boost */
#endif
#endif

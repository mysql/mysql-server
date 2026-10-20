/* Copyright 2003-2025 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * See http://www.boost.org/libs/multi_index for library home page.
 */

#ifndef BOOST_MULTI_INDEX_DETAIL_IS_INDEX_LIST_HPP
#define BOOST_MULTI_INDEX_DETAIL_IS_INDEX_LIST_HPP

#if defined(_MSC_VER)
#pragma once
#endif

#include <boost/config.hpp> /* keep it first to prevent nasty warns in MSVC */

#if defined(BOOST_MULTI_INDEX_ENABLE_MPL_SUPPORT)
#include <boost/multi_index/detail/mpl_to_mp11_list.hpp>
#define BOOST_MULTI_INDEX_BLOCK_BOOSTDEP_HEADER \
  <boost/mpl/empty.hpp>
#include BOOST_MULTI_INDEX_BLOCK_BOOSTDEP_HEADER
#undef BOOST_MULTI_INDEX_BLOCK_BOOSTDEP_HEADER
#define BOOST_MULTI_INDEX_BLOCK_BOOSTDEP_HEADER \
  <boost/mpl/is_sequence.hpp>
#include BOOST_MULTI_INDEX_BLOCK_BOOSTDEP_HEADER
#undef BOOST_MULTI_INDEX_BLOCK_BOOSTDEP_HEADER
#include <type_traits>

namespace boost{

namespace multi_index{

namespace detail{

/* an index list is a non-empty MPL sequence */

template<typename T>
struct is_index_list
{
  BOOST_STATIC_CONSTANT(bool,mpl_sequence=mpl::is_sequence<T>::value);
  BOOST_STATIC_CONSTANT(bool,non_empty=!(std::conditional<
    mpl_sequence,mpl::empty<T>,std::false_type>::type::value));
  BOOST_STATIC_CONSTANT(bool,value=mpl_sequence&&non_empty);
};

template<typename IndexList>
using mp11_index_list=mpl_to_mp11_list<IndexList>;

} /* namespace multi_index::detail */

} /* namespace multi_index */

} /* namespace boost */
#else
#include <type_traits>

namespace boost{

namespace multi_index{

namespace detail{

template<typename T>
struct is_index_list:std::false_type{};

/* an index list is a non-empty Mp11 list */
template<template<typename...> class L,typename T,typename... Ts>
struct is_index_list<L<T,Ts...>>:std::true_type{};

template<typename IndexList>
using mp11_index_list=IndexList;

} /* namespace multi_index::detail */

} /* namespace multi_index */

} /* namespace boost */
#endif
#endif

/* Copyright 2003-2025 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * See http://www.boost.org/libs/multi_index for library home page.
 */

#ifndef BOOST_MULTI_INDEX_TAG_HPP
#define BOOST_MULTI_INDEX_TAG_HPP

#if defined(_MSC_VER)
#pragma once
#endif

#include <boost/config.hpp> /* keep it first to prevent nasty warns in MSVC */
#include <boost/multi_index/detail/no_duplicate_tags.hpp>
#include <boost/static_assert.hpp>
#include <boost/type_traits/is_base_and_derived.hpp>

/* A Mp11 list containing types used as tag names for indices in get()
 * functions.
 * If BOOST_MULTI_INDEX_ENABLE_MPL_SUPPORT is defined, the old
 * MPL-based definition of tag is kept.
 */

#if defined(BOOST_MULTI_INDEX_ENABLE_MPL_SUPPORT)
#include <boost/multi_index/detail/mpl_to_mp11_list.hpp>
#define BOOST_MULTI_INDEX_BLOCK_BOOSTDEP_HEADER \
  <boost/mpl/identity.hpp>
#include BOOST_MULTI_INDEX_BLOCK_BOOSTDEP_HEADER
#undef BOOST_MULTI_INDEX_BLOCK_BOOSTDEP_HEADER
#define BOOST_MULTI_INDEX_BLOCK_BOOSTDEP_HEADER \
  <boost/mpl/transform.hpp>
#include BOOST_MULTI_INDEX_BLOCK_BOOSTDEP_HEADER
#undef BOOST_MULTI_INDEX_BLOCK_BOOSTDEP_HEADER
#define BOOST_MULTI_INDEX_BLOCK_BOOSTDEP_HEADER \
  <boost/mpl/vector.hpp>
#include BOOST_MULTI_INDEX_BLOCK_BOOSTDEP_HEADER
#undef BOOST_MULTI_INDEX_BLOCK_BOOSTDEP_HEADER
#include <boost/preprocessor/facilities/intercept.hpp> 
#include <boost/preprocessor/repetition/enum_binary_params.hpp> 
#include <boost/preprocessor/repetition/enum_params.hpp> 

/* A wrapper of mpl::vector used to hide MPL from the user.
 * tag contains types used as tag names for indices in get() functions.
 */

/* This user_definable macro limits the number of elements of a tag;
 * useful for shortening resulting symbol names (MSVC++ 6.0, for instance,
 * has problems coping with very long symbol names.)
 */

#if !defined(BOOST_MULTI_INDEX_LIMIT_TAG_SIZE)
#define BOOST_MULTI_INDEX_LIMIT_TAG_SIZE BOOST_MPL_LIMIT_VECTOR_SIZE
#endif

#if BOOST_MULTI_INDEX_LIMIT_TAG_SIZE<BOOST_MPL_LIMIT_VECTOR_SIZE
#define BOOST_MULTI_INDEX_TAG_SIZE BOOST_MULTI_INDEX_LIMIT_TAG_SIZE
#else
#define BOOST_MULTI_INDEX_TAG_SIZE BOOST_MPL_LIMIT_VECTOR_SIZE
#endif

namespace boost{

namespace multi_index{

namespace detail{

struct tag_marker{};

template<typename T>
struct is_tag
{
  BOOST_STATIC_CONSTANT(bool,value=(is_base_and_derived<tag_marker,T>::value));
};

} /* namespace multi_index::detail */

template<
  BOOST_PP_ENUM_BINARY_PARAMS(
    BOOST_MULTI_INDEX_TAG_SIZE,
    typename T,
    =mpl::na BOOST_PP_INTERCEPT) 
>
struct tag:private detail::tag_marker
{
  /* The mpl::transform pass produces shorter symbols (without
   * trailing mpl::na's.)
   */

  typedef typename mpl::transform<
    mpl::vector<BOOST_PP_ENUM_PARAMS(BOOST_MULTI_INDEX_TAG_SIZE,T)>,
    mpl::identity<mpl::_1>
  >::type type;

  BOOST_STATIC_ASSERT(
    detail::no_duplicate_tags<detail::mpl_to_mp11_list<type>>::value);
};

template<typename TagList>
using mp11_tag_list=detail::mpl_to_mp11_list<typename TagList::type>;

} /* namespace multi_index */

} /* namespace boost */

#undef BOOST_MULTI_INDEX_TAG_SIZE
#else
namespace boost{

namespace multi_index{

namespace detail{

struct tag_marker{};

template<typename T>
struct is_tag
{
  BOOST_STATIC_CONSTANT(bool,value=(is_base_and_derived<tag_marker,T>::value));
};

} /* namespace multi_index::detail */

template<typename... Ts>
struct tag:private detail::tag_marker
{
  BOOST_STATIC_ASSERT(detail::no_duplicate_tags<tag>::value);
};

template<typename TagList>
using mp11_tag_list=TagList;

} /* namespace multi_index */

} /* namespace boost */
#endif
#endif

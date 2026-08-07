/* Copyright 2003-2025 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * See http://www.boost.org/libs/multi_index for library home page.
 */

#ifndef BOOST_MULTI_INDEX_DETAIL_PROMOTES_ARG_HPP
#define BOOST_MULTI_INDEX_DETAIL_PROMOTES_ARG_HPP

#if defined(_MSC_VER)
#pragma once
#endif

#include <boost/config.hpp> /* keep it first to prevent nasty warns in MSVC */
#include <boost/detail/workaround.hpp>
#include <type_traits>

/* Metafunctions to check if f(arg1,arg2) promotes either arg1 to the type of
 * arg2 or viceversa. By default, (i.e. if it cannot be determined), no
 * promotion is assumed.
 */

#if BOOST_WORKAROUND(BOOST_MSVC,<1400)

namespace boost{

namespace multi_index{

namespace detail{

template<typename F,typename Arg1,typename Arg2>
struct promotes_1st_arg:std::false_type{};

template<typename F,typename Arg1,typename Arg2>
struct promotes_2nd_arg:std::false_type{};

} /* namespace multi_index::detail */

} /* namespace multi_index */

} /* namespace boost */

#else

#include <boost/mp11/function.hpp>
#include <boost/multi_index/detail/is_transparent.hpp>
#include <boost/type_traits/is_convertible.hpp>

namespace boost{

namespace multi_index{

namespace detail{
  
template<typename F,typename Arg1,typename Arg2>
struct promotes_1st_arg:
  mp11::mp_and<
    mp11::mp_not<is_transparent<F,Arg1,Arg2> >,
    is_convertible<const Arg1,Arg2>,
    is_transparent<F,Arg2,Arg2>
  >
{};

template<typename F,typename Arg1,typename Arg2>
struct promotes_2nd_arg:
  mp11::mp_and<
    mp11::mp_not<is_transparent<F,Arg1,Arg2> >,
    is_convertible<const Arg2,Arg1>,
    is_transparent<F,Arg1,Arg1>
  >
{};

} /* namespace multi_index::detail */

} /* namespace multi_index */

} /* namespace boost */

#endif
#endif

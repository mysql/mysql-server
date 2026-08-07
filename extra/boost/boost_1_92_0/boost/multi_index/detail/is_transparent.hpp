/* Copyright 2003-2025 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * See http://www.boost.org/libs/multi_index for library home page.
 */

#ifndef BOOST_MULTI_INDEX_DETAIL_IS_TRANSPARENT_HPP
#define BOOST_MULTI_INDEX_DETAIL_IS_TRANSPARENT_HPP

#if defined(_MSC_VER)
#pragma once
#endif

#include <boost/config.hpp> /* keep it first to prevent nasty warns in MSVC */
#include <boost/type_traits/intrinsics.hpp>
#include <type_traits>

namespace boost{

namespace multi_index{

namespace detail{

/* Metafunction that checks if f(arg,arg2) executes without argument type
 * conversion. By default (i.e. when it cannot be determined) it evaluates to
 * true.
 */

template<typename F,typename Arg1,typename Arg2,typename=void>
struct is_transparent:std::true_type{};

} /* namespace multi_index::detail */

} /* namespace multi_index */

} /* namespace boost */

#if !defined(BOOST_NO_SFINAE_EXPR)&& \
    (defined(BOOST_NO_CXX11_FINAL)||defined(BOOST_IS_FINAL))

#include <boost/core/enable_if.hpp>
#include <boost/type_traits/declval.hpp>
#include <boost/type_traits/function_traits.hpp>
#include <boost/type_traits/is_class.hpp>
#include <boost/type_traits/is_final.hpp>
#include <boost/type_traits/is_function.hpp>
#include <boost/type_traits/is_same.hpp>
#include <boost/type_traits/remove_pointer.hpp>

namespace boost{

namespace multi_index{

namespace detail{

struct not_is_transparent_result_type{};

template<typename F,typename Arg1,typename Arg2>
struct is_transparent_class_helper:F
{
  using F::operator();
  template<typename T,typename Q>
  not_is_transparent_result_type operator()(const T&,const Q&)const;
};

template<typename F,typename Arg1,typename Arg2,typename=void>
struct is_transparent_class:std::true_type{};

template<typename F,typename Arg1,typename Arg2>
struct is_transparent_class<
  F,Arg1,Arg2,
  typename enable_if<
    is_same<
      decltype(
        declval<const is_transparent_class_helper<F,Arg1,Arg2> >()(
          declval<const Arg1&>(),declval<const Arg2&>())
      ),
      not_is_transparent_result_type
    >
  >::type
>:std::false_type{};

template<typename F,typename Arg1,typename Arg2>
struct is_transparent<
  F,Arg1,Arg2,
  typename enable_if_c<
    is_class<F>::value&&
    !is_final<F>::value /* is_transparent_class_helper derives from F */
  >::type
>:is_transparent_class<F,Arg1,Arg2>{};

template<typename F,typename Arg1,typename Arg2,typename=void>
struct is_transparent_function:std::true_type{};

template<typename F,typename Arg1,typename Arg2>
struct is_transparent_function<
  F,Arg1,Arg2,
  typename enable_if_c<
    !(is_same<typename function_traits<F>::arg1_type,const Arg1&>::value||
      is_same<typename function_traits<F>::arg1_type,Arg1>::value)
    ||
    !(is_same<typename function_traits<F>::arg2_type,const Arg2&>::value||
      is_same<typename function_traits<F>::arg2_type,Arg2>::value)
  >::type
>:std::false_type{};

template<typename F,typename Arg1,typename Arg2>
struct is_transparent<
  F,Arg1,Arg2,
  typename enable_if<
    is_function<typename remove_pointer<F>::type>
  >::type
>:is_transparent_function<typename remove_pointer<F>::type,Arg1,Arg2>{};

} /* namespace multi_index::detail */

} /* namespace multi_index */

} /* namespace boost */

#endif
#endif

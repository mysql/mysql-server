/* Copyright 2024 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * See http://www.boost.org/libs/poly_collection for library home page.
 */

#ifndef BOOST_POLY_COLLECTION_DETAIL_COPYABLE_HPP
#define BOOST_POLY_COLLECTION_DETAIL_COPYABLE_HPP

#if defined(_MSC_VER)
#pragma once
#endif

#include <functional>
#include <type_traits>

namespace boost{

namespace poly_collection{

namespace detail{

/* wraps T if it is not copyable */

template<typename T,typename=void>
struct copyable_impl{using type=T;};

template<typename T>
struct reference_wrapper
{
  reference_wrapper(T& x):p{&x}{}

  operator T&()const noexcept{return *p;}

  T* p;
};

template<typename T>
struct copyable_impl<
  T,
  typename std::enable_if<!std::is_copy_constructible<T>::value>::type
>
{using type=reference_wrapper<const T>;};

template<typename T>
using copyable=typename copyable_impl<T>::type;

template<typename T>
copyable<T> make_copyable(const T& x){return x;}

} /* namespace poly_collection::detail */

} /* namespace poly_collection */

} /* namespace boost */

#endif

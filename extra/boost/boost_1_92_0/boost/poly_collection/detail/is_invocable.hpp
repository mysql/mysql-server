/* Copyright 2016-2026 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * See http://www.boost.org/libs/poly_collection for library home page.
 */

#ifndef BOOST_POLY_COLLECTION_DETAIL_IS_INVOCABLE_HPP
#define BOOST_POLY_COLLECTION_DETAIL_IS_INVOCABLE_HPP

#if defined(_MSC_VER)
#pragma once
#endif

#include <type_traits>

#if __cpp_lib_is_invocable>=201703L
namespace boost{

namespace poly_collection{

namespace detail{

template <typename F,typename... Args>
using is_invocable=std::is_invocable<F,Args...>;

template <typename R,typename F,typename... Args>
using is_invocable_r=std::is_invocable_r<R,F,Args...>;

} /* namespace poly_collection::detail */

} /* namespace poly_collection */

} /* namespace boost */
#else
#include <functional>

namespace boost{

namespace poly_collection{

namespace detail{

template <typename F,typename... Args>
struct is_invocable:
  std::is_constructible<
    std::function<void(Args...)>,
    std::reference_wrapper<typename std::remove_reference<F>::type>
  >
{};

template <typename R,typename F,typename... Args>
struct is_invocable_r:
  std::is_constructible<
    std::function<R(Args...)>,
    std::reference_wrapper<typename std::remove_reference<F>::type>
  >
{};

} /* namespace poly_collection::detail */

} /* namespace poly_collection */

} /* namespace boost */
#endif

#endif

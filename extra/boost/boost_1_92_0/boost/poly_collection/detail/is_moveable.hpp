/* Copyright 2024 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * See http://www.boost.org/libs/poly_collection for library home page.
 */

#ifndef BOOST_POLY_COLLECTION_DETAIL_IS_MOVEABLE_HPP
#define BOOST_POLY_COLLECTION_DETAIL_IS_MOVEABLE_HPP

#if defined(_MSC_VER)
#pragma once
#endif

#include <type_traits>

namespace boost{

namespace poly_collection{

namespace detail{

template<typename T> struct is_moveable:std::integral_constant<
  bool,
  std::is_move_constructible<typename std::decay<T>::type>::value&&
  (std::is_move_assignable<typename std::decay<T>::type>::value||
   std::is_nothrow_move_constructible<typename std::decay<T>::type>::value)
>{};

} /* namespace poly_collection::detail */

} /* namespace poly_collection */

} /* namespace boost */

#endif

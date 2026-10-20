/* Copyright 2024 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * See http://www.boost.org/libs/poly_collection for library home page.
 */

#ifndef BOOST_POLY_COLLECTION_DETAIL_SEGMENT_MAP_HPP
#define BOOST_POLY_COLLECTION_DETAIL_SEGMENT_MAP_HPP

#if defined(_MSC_VER)
#pragma once
#endif

#include <boost/poly_collection/detail/size_t_map.hpp>
#include <boost/poly_collection/detail/type_info_map.hpp>

namespace boost{

namespace poly_collection{

namespace detail{

template<typename Key> struct segment_map_helper;

template<> struct segment_map_helper<std::type_info>
{
  template<typename... Args> using fn=type_info_map<Args...>;
};

template<> struct segment_map_helper<std::size_t>
{
  template<typename... Args> using fn=size_t_map<Args...>;
};

template<typename Key,typename... Args>
using segment_map=typename segment_map_helper<Key>::template fn<Args...>;

} /* namespace poly_collection::detail */

} /* namespace poly_collection */

} /* namespace boost */

#endif

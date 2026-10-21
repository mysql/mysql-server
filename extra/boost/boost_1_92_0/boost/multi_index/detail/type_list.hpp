/* Copyright 2003-2025 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * See http://www.boost.org/libs/multi_index for library home page.
 */

#ifndef BOOST_MULTI_INDEX_DETAIL_TYPE_LIST_HPP
#define BOOST_MULTI_INDEX_DETAIL_TYPE_LIST_HPP

#if defined(_MSC_VER)
#pragma once
#endif

#if defined(BOOST_MULTI_INDEX_ENABLE_MPL_SUPPORT)
#define BOOST_MULTI_INDEX_BLOCK_BOOSTDEP_HEADER \
  <boost/mpl/push_front.hpp>
#include BOOST_MULTI_INDEX_BLOCK_BOOSTDEP_HEADER
#undef BOOST_MULTI_INDEX_BLOCK_BOOSTDEP_HEADER
#define BOOST_MULTI_INDEX_BLOCK_BOOSTDEP_HEADER \
  <boost/mpl/vector.hpp>
#include BOOST_MULTI_INDEX_BLOCK_BOOSTDEP_HEADER
#undef BOOST_MULTI_INDEX_BLOCK_BOOSTDEP_HEADER

namespace boost{

namespace multi_index{

namespace detail{

using empty_type_list=mpl::vector0<>;

template<typename TypeList,typename T>
using type_list_push_front=typename mpl::push_front<TypeList,T>::type;

} /* namespace multi_index::detail */

} /* namespace multi_index */

} /* namespace boost */
#else
#include <boost/mp11/list.hpp>

namespace boost{

namespace multi_index{

namespace detail{

using empty_type_list=mp11::mp_list<>;

template<typename TypeList,typename T>
using type_list_push_front=mp11::mp_push_front<TypeList,T>;

} /* namespace multi_index::detail */

} /* namespace multi_index */

} /* namespace boost */
#endif
#endif

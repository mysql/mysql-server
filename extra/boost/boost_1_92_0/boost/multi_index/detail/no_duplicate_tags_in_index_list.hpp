/* Copyright 2003-2025 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * See http://www.boost.org/libs/multi_index for library home page.
 */

#ifndef BOOST_MULTI_INDEX_DETAIL_NO_DUPLICATE_TAGS_IN_INDEX_LIST_HPP
#define BOOST_MULTI_INDEX_DETAIL_NO_DUPLICATE_TAGS_IN_INDEX_LIST_HPP

#if defined(_MSC_VER)
#pragma once
#endif

#include <boost/config.hpp> /* keep it first to prevent nasty warns in MSVC */
#include <boost/multi_index/tag.hpp>
#include <boost/mp11/algorithm.hpp>
#include <boost/mp11/list.hpp>
#include <type_traits>

namespace boost{

namespace multi_index{

namespace detail{

/* checks duplication of tags across all indices of a container */

template<typename Index>
using index_tag_list=mp11::mp_rename<
  mp11_tag_list<typename Index::tag_list>,mp11::mp_list>;

template<typename IndexList>
using no_duplicate_tags_in_index_list=no_duplicate_tags<
  mp11::mp_flatten<mp11::mp_transform<index_tag_list,IndexList>>>;

} /* namespace multi_index::detail */

} /* namespace multi_index */

} /* namespace boost */

#endif

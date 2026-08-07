/* Copyright 2003-2025 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * See http://www.boost.org/libs/multi_index for library home page.
 */

#ifndef BOOST_MULTI_INDEX_DETAIL_ORD_INDEX_ARGS_HPP
#define BOOST_MULTI_INDEX_DETAIL_ORD_INDEX_ARGS_HPP

#if defined(_MSC_VER)
#pragma once
#endif

#include <boost/config.hpp> /* keep it first to prevent nasty warns in MSVC */
#include <boost/mp11/utility.hpp>
#include <boost/multi_index/tag.hpp>
#include <boost/static_assert.hpp>
#include <functional>
#include <type_traits>

namespace boost{

namespace multi_index{

namespace detail{

/* Oredered index specifiers can be instantiated in two forms:
 *
 *   (ordered_unique|ordered_non_unique)<
 *     KeyFromValue,Compare=std::less<KeyFromValue::result_type> >
 *   (ordered_unique|ordered_non_unique)<
 *     TagList,KeyFromValue,Compare=std::less<KeyFromValue::result_type> >
 *
 * index_args implements the machinery to accept this argument-dependent
 * polymorphism.
 */

template<typename Arg1,typename Arg2,typename Arg3>
struct ordered_index_args
{
  typedef is_tag<Arg1> full_form;

  typedef mp11::mp_if<
    full_form,
    Arg1,
    tag< > >                                     tag_list_type;
  typedef mp11::mp_if<
    full_form,
    Arg2,
    Arg1>                                        key_from_value_type;
  typedef mp11::mp_if<
    full_form,
    Arg3,
    Arg2>                                        supplied_compare_type;
  typedef mp11::mp_eval_if_c<
    !std::is_void<supplied_compare_type>::value,
    supplied_compare_type,
    std::less,
    typename key_from_value_type::result_type
  >                                              compare_type;

  BOOST_STATIC_ASSERT(is_tag<tag_list_type>::value);
  BOOST_STATIC_ASSERT(!std::is_void<key_from_value_type>::value);
  BOOST_STATIC_ASSERT(!std::is_void<compare_type>::value);
};

} /* namespace multi_index::detail */

} /* namespace multi_index */

} /* namespace boost */

#endif
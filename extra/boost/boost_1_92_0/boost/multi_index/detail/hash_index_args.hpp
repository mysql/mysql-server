/* Copyright 2003-2025 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * See http://www.boost.org/libs/multi_index for library home page.
 */

#ifndef BOOST_MULTI_INDEX_DETAIL_HASH_INDEX_ARGS_HPP
#define BOOST_MULTI_INDEX_DETAIL_HASH_INDEX_ARGS_HPP

#if defined(_MSC_VER)
#pragma once
#endif

#include <boost/config.hpp> /* keep it first to prevent nasty warns in MSVC */
#include <boost/container_hash/hash.hpp>
#include <boost/mp11/utility.hpp>
#include <boost/multi_index/tag.hpp>
#include <boost/static_assert.hpp>
#include <boost/type_traits/is_same.hpp>
#include <functional>
#include <type_traits>

namespace boost{

namespace multi_index{

namespace detail{

/* Hashed index specifiers can be instantiated in two forms:
 *
 *   (hashed_unique|hashed_non_unique)<
 *     KeyFromValue,
 *     Hash=boost::hash<KeyFromValue::result_type>,
 *     Pred=std::equal_to<KeyFromValue::result_type> >
 *   (hashed_unique|hashed_non_unique)<
 *     TagList,
 *     KeyFromValue,
 *     Hash=boost::hash<KeyFromValue::result_type>,
 *     Pred=std::equal_to<KeyFromValue::result_type> >
 *
 * hashed_index_args implements the machinery to accept this
 * argument-dependent polymorphism.
 */

template<typename Arg1,typename Arg2,typename Arg3,typename Arg4>
struct hashed_index_args
{
  typedef is_tag<Arg1> full_form;

  typedef mp11::mp_if<
    full_form,
    Arg1,
    tag< > >                                  tag_list_type;
  typedef mp11::mp_if<
    full_form,
    Arg2,
    Arg1>                                     key_from_value_type;
  typedef mp11::mp_if<
    full_form,
    Arg3,
    Arg2>                                     supplied_hash_type;
  typedef mp11::mp_eval_if_c<
    !std::is_void<supplied_hash_type>::value,
    supplied_hash_type,
    boost::hash,
    typename key_from_value_type::result_type
  >                                           hash_type;
  typedef mp11::mp_if<
    full_form,
    Arg4,
    Arg3>                                     supplied_pred_type;
  typedef mp11::mp_eval_if_c<
    !std::is_void<supplied_pred_type>::value,
    supplied_pred_type,
    std::equal_to,
    typename key_from_value_type::result_type
  >                                           pred_type;

  BOOST_STATIC_ASSERT(is_tag<tag_list_type>::value);
  BOOST_STATIC_ASSERT(!std::is_void<key_from_value_type>::value);
  BOOST_STATIC_ASSERT(!std::is_void<hash_type>::value);
  BOOST_STATIC_ASSERT(!std::is_void<pred_type>::value);
};

} /* namespace multi_index::detail */

} /* namespace multi_index */

} /* namespace boost */

#endif

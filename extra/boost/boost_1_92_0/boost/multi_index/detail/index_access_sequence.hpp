/* Copyright 2003-2021 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * See http://www.boost.org/libs/multi_index for library home page.
 */

#ifndef BOOST_MULTI_INDEX_DETAIL_INDEX_ACCESS_SEQUENCE_HPP
#define BOOST_MULTI_INDEX_DETAIL_INDEX_ACCESS_SEQUENCE_HPP

#if defined(_MSC_VER)
#pragma once
#endif

#include <boost/config.hpp> /* keep it first to prevent nasty warns in MSVC */
#include <boost/mp11/list.hpp>
#include <boost/mp11/utility.hpp>
#include <boost/multi_index_container_fwd.hpp>

namespace boost{

namespace multi_index{

namespace detail{

/* Successive access to the indices of a multi_index_container. Used as dst in
 * backbone function extract_(x,dst) to retrieve the destination indices
 * where iterators referring to x must be transferred to (in merging
 * operations).
 */

template<typename MultiIndexContainer,int N=0>
struct index_access_sequence;

struct index_access_sequence_terminal
{
  index_access_sequence_terminal(void*){}
};

template<typename MultiIndexContainer,int N>
struct index_access_sequence_normal
{
  MultiIndexContainer* p;

  index_access_sequence_normal(MultiIndexContainer* p_):p(p_){}

  typename nth_index<MultiIndexContainer,N>::type&
  get(){return p->template get<N>();}

  index_access_sequence<MultiIndexContainer,N+1>
  next(){return index_access_sequence<MultiIndexContainer,N+1>(p);}
};

template<typename MultiIndexContainer,int N>
using index_access_sequence_base=mp11::mp_if_c<
  N<mp11::mp_size<typename MultiIndexContainer::index_type_list>::value,
  index_access_sequence_normal<MultiIndexContainer,N>,
  index_access_sequence_terminal
>;

template<typename MultiIndexContainer,int N>
struct index_access_sequence:index_access_sequence_base<MultiIndexContainer,N>
{
  typedef index_access_sequence_base<
    MultiIndexContainer,N>                     super;
 
  index_access_sequence(MultiIndexContainer* p):super(p){}
};

} /* namespace multi_index::detail */

} /* namespace multi_index */

} /* namespace boost */

#endif

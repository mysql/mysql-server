/* Copyright 2003-2025 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * See http://www.boost.org/libs/multi_index for library home page.
 */

#ifndef BOOST_MULTI_INDEX_DETAIL_AUGMENTED_STDTUPLE_HPP
#define BOOST_MULTI_INDEX_DETAIL_AUGMENTED_STDTUPLE_HPP

#if defined(_MSC_VER)
#pragma once
#endif

#include <boost/config.hpp> /* keep it first to prevent nasty warns in MSVC */
#include <boost/mp11/algorithm.hpp>
#include <boost/mp11/function.hpp>
#include <boost/mp11/list.hpp>
#include <boost/mp11/integer_sequence.hpp>
#include <boost/mp11/integral.hpp>
#include <boost/tuple/tuple.hpp>
#include <tuple>
#include <type_traits>

namespace boost{

namespace multi_index{

namespace detail{

/* augmented_stdtuple<Ts...> derives from std::tuple<Ts...> and provides the
 * following non-standard constructors:
 * 
 * - augmented_stdtuple(const Ts& = Ts()...) (this signature is not legal C++,
 *   shown for explanatory purposes).
 * - augmented_stdtuple(boost::tuple<Ts...>) if sizeof...(Ts) does not exceed
 *   the maximum length of a boost::tuple (10 as of this writing).
 */

template<typename T,typename U>
using is_convertible_to_cref=std::is_convertible<T,const U&>;

template<typename FromList,typename ToList,typename=void>
struct are_convertible_to_crefs:mp11::mp_false{};

template<typename FromList,typename ToList>
struct are_convertible_to_crefs<
  FromList,ToList,
  typename std::enable_if<
    (mp11::mp_size<FromList>::value<=mp11::mp_size<ToList>::value)
  >::type
>:
mp11::mp_apply<
  mp11::mp_and,
  mp11::mp_transform<
    is_convertible_to_cref,
    FromList,
    mp11::mp_take_c<ToList,mp11::mp_size<FromList>::value>
  >
>
{};

template<typename... Ts>
struct augmented_stdtuple:std::tuple<Ts...>
{
private:
  using super=std::tuple<Ts...>;
  static constexpr std::size_t N=sizeof...(Ts);

  template<typename... Args>
  using are_convertible_to_crefs=
    typename detail::are_convertible_to_crefs<mp11::mp_list<Args...>,super>;

  struct boost_tuple_too_short{};
  using boost_tuple_arg=mp11::mp_eval_if_c<
    (mp11::mp_size<boost::tuple<>>::value < N),
    boost_tuple_too_short,
    boost::tuple,Ts...
  >;

  struct full_args_ctor{};
  augmented_stdtuple(full_args_ctor,const Ts&... args):super(args...){}

  struct partial_args_ctor{};
  template<std::size_t... Is,typename... Args>
  augmented_stdtuple(
    partial_args_ctor,mp11::index_sequence<Is...>,Args&&... args):
    augmented_stdtuple(
      full_args_ctor{},
      std::forward<Args>(args)...,
      typename std::tuple_element<sizeof...(Args)+Is,super>::type()...)
  {}

  struct boost_tuple_ctor{};
  template<std::size_t... Is>
  augmented_stdtuple(
    boost_tuple_ctor,mp11::index_sequence<Is...>,const boost_tuple_arg& x):
    super(tuples::get<Is>(x)...)
  {}

public:
  template<
    typename... Args,
    typename std::enable_if<
      are_convertible_to_crefs<Args&&...>::value
    >::type* =nullptr
  >
  augmented_stdtuple(Args&&... args):
    augmented_stdtuple(
      partial_args_ctor(),mp11::make_index_sequence<N-sizeof...(Args)>(),
      std::forward<Args>(args)...)
  {}

  augmented_stdtuple(const super& x):super(x){}

  augmented_stdtuple(const boost_tuple_arg& x):
    augmented_stdtuple(
      boost_tuple_ctor(),mp11::make_index_sequence<N>(),x)
  {}
};

} /* namespace multi_index::detail */

} /* namespace multi_index */

} /* namespace boost */

#endif

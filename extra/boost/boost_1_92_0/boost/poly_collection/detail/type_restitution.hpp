/* Copyright 2016-2024 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * See http://www.boost.org/libs/poly_collection for library home page.
 */

#ifndef BOOST_POLY_COLLECTION_DETAIL_TYPE_RESTITUTION_HPP
#define BOOST_POLY_COLLECTION_DETAIL_TYPE_RESTITUTION_HPP

#if defined(_MSC_VER)
#pragma once
#endif

#include <boost/config.hpp>
#include <boost/detail/workaround.hpp>
#include <boost/mp11/algorithm.hpp>
#include <boost/mp11/bind.hpp>
#include <boost/mp11/function.hpp>
#include <boost/mp11/list.hpp>
#include <boost/poly_collection/detail/is_closed_collection.hpp>
#include <boost/poly_collection/detail/functional.hpp>
#include <boost/poly_collection/detail/iterator_traits.hpp>
#include <type_traits>
#include <utility>

#if defined(BOOST_MSVC)
#pragma warning(push)
#pragma warning(disable:4714) /* marked as __forceinline not inlined */
#endif

namespace boost{

namespace poly_collection{

struct all_types;

namespace detail{

/* If a list of restituted types contain all_types, replaces it with the
 * list of all acceptable types of the associated closed collection.
 */

template<typename Model,typename L,typename=void>
struct restitution_list_impl
{
  static_assert(
    !mp11::mp_contains<L,all_types>::value,
    "all_types can't be used with open collections");

  using type=L;
};

template<typename Model,typename L>
struct restitution_list_impl<
  Model,L,
  typename std::enable_if<is_closed_collection<Model>::value>::type
>
{
  using type=typename std::conditional<
    mp11::mp_contains<L,all_types>::value,
    typename Model::acceptable_type_list,
    L
  >::type;
};

template<typename Model,typename... Ts>
using restitution_list=
  typename restitution_list_impl<Model,mp11::mp_list<Ts...>>::type;

/* Calculates if the given types cover the entire set of acceptable types for
 * the associated closed collection.
 */

namespace is_total_restitution_impl
{
template<typename Model,typename L,typename=void>
struct helper:std::false_type{};

#if BOOST_WORKAROUND(BOOST_MSVC,<=1900)
template<typename L1,typename L2>
struct is_contained;

template<template <typename...> class L1,typename L2>
struct is_contained<L1<>,L2>:std::true_type{};

template<template <typename...> class L1,typename T,typename... Ts,typename L2>
struct is_contained<L1<T,Ts...>,L2>:is_contained<L1<Ts...>,L2>
{
  using super=is_contained<L1<Ts...>,L2>;
  static constexpr bool value=super::value&&mp11::mp_contains<L2,T>::value;
};

template<typename Model,typename L>
struct helper<
  Model,L,
  typename std::enable_if<
    is_closed_collection<Model>::value&&
    is_contained<typename Model::acceptable_type_list,L>::value
  >::type
>:std::true_type{};
#else
template<typename Model,typename L>
struct helper<
  Model,L,
  typename std::enable_if<
    is_closed_collection<Model>::value&&
    mp11::mp_all_of_q<
      typename Model::acceptable_type_list,
      mp11::mp_bind_front<mp11::mp_contains,L>
    >::value
  >::type
>:std::true_type{};
#endif
} /* namespace is_total_restitution_impl */

template<typename Model,typename L>
using is_total_restitution=
  mp11::mp_bool<is_total_restitution_impl::helper<Model,L>::value>;

/* Given types Ts..., a const type_index& info and a local_base_iterator
 * it, we denote by restitute<Ts...>(info,it):
 *   - a local_iterator<Ti> from it, if info==index<Ti>() for some Ti in
 *     Ts...
 *   - it otherwise,
 * where type_index and index are the type indexing facilities of the
 * underlying polymorphic model.
 * 
 * Using this notation, restitute_range<IsTotal,Ts...>(f,args...)(s) resolves
 * to f(restitute<Ts...>(info,begin),restitute<Ts...>(info,end),args...) where
 * info=s.type_info(), begin=s.begin(), end=s.end(). If the restitution is
 * total, f(begin,end,args...) (the base, unrestituted case) is not only
 * not called ever, but also not instantiated.
 */

template<typename IsTotal,typename F,typename... Ts>
struct restitute_range_class;
       
template<typename IsTotal,typename F,typename T,typename... Ts>
struct restitute_range_class<IsTotal,F,T,Ts...>:
  restitute_range_class<IsTotal,F,Ts...>
{
  using super=restitute_range_class<IsTotal,F,Ts...>;
  using super::super;
  
  template<typename SegmentInfo>
  BOOST_FORCEINLINE auto operator()(SegmentInfo&& s)
    ->decltype(std::declval<F>()(s.begin(),s.end()))
  {
    using traits=iterator_traits<decltype(s.begin())>;
    using local_iterator=typename traits::template local_iterator<T>;

    if(s.type_info()==traits::template index<T>())
      return (this->f)(
        local_iterator{s.begin()},local_iterator{s.end()});
    else
      return super::operator()(std::forward<SegmentInfo>(s));
  }
};

template<typename F,typename T>
struct restitute_range_class<std::true_type /* total */,F,T>
{
  restitute_range_class(const F& f):f(f){}

  template<typename SegmentInfo>
  BOOST_FORCEINLINE auto operator()(SegmentInfo&& s)
    ->decltype(std::declval<F>()(s.begin(),s.end()))
  {
    using traits=iterator_traits<decltype(s.begin())>;
    using local_iterator=typename traits::template local_iterator<T>;

    return f(local_iterator{s.begin()},local_iterator{s.end()});
  }

  F f;
};

template<typename IsTotal,typename F>
struct restitute_range_class<IsTotal,F>
{
  restitute_range_class(const F& f):f(f){}
  
  template<typename SegmentInfo>
  BOOST_FORCEINLINE auto operator()(SegmentInfo&& s)
    ->decltype(std::declval<F>()(s.begin(),s.end()))
  {
    return f(s.begin(),s.end());
  }

  F f;
};

template<
  typename Model,typename... Ts,typename F,typename... Args,
  typename RestitutionList=restitution_list<Model,Ts...>
>
auto restitute_range(const F& f,Args&&... args)->mp11::mp_apply<
  restitute_range_class,
  mp11::mp_append<
    mp11::mp_list<
      is_total_restitution<Model,RestitutionList>,
      decltype(tail_closure(f,std::forward<Args>(args)...))
    >,
    RestitutionList
  >
>
{
  return tail_closure(f,std::forward<Args>(args)...);
}

/* restitute_iterator<IsTotal,Ts...>(f,args2...)(info,it,args1...) resolves to
 * f(restitute<Ts...>(info,it),args1...,args2...).
 */

template<typename IsTotal,typename F,typename... Ts>
struct restitute_iterator_class;
       
template<typename IsTotal,typename F,typename T,typename... Ts>
struct restitute_iterator_class<IsTotal,F,T,Ts...>:
  restitute_iterator_class<IsTotal,F,Ts...>
{
  using super=restitute_iterator_class<IsTotal,F,Ts...>;
  using super::super;
  
  template<
    typename TypeIndex,typename Iterator,typename... Args,
    typename Traits=iterator_traits<typename std::decay<Iterator>::type>,
    typename LocalIterator=typename Traits::template local_iterator<T>
  >
  BOOST_FORCEINLINE auto operator()(
    const TypeIndex& info,Iterator&& it,Args&&... args)
    ->decltype(
      std::declval<F>()(LocalIterator{it},std::forward<Args>(args)...))
  {
    if(static_cast<const typename Traits::type_index&>(info)==
       Traits::template index<T>()){
      return (this->f)(LocalIterator{it},std::forward<Args>(args)...);
    }
    else{
      return super::operator()(
        info,std::forward<Iterator>(it),std::forward<Args>(args)...);
    }
  }
};

template<typename F,typename T>
struct restitute_iterator_class<std::true_type /* total */,F,T>
{
  restitute_iterator_class(const F& f):f(f){}
  
  template<
    typename TypeIndex,typename Iterator,typename... Args,
    typename Traits=iterator_traits<typename std::decay<Iterator>::type>,
    typename LocalIterator=typename Traits::template local_iterator<T>
  >
  BOOST_FORCEINLINE auto operator()(
    const TypeIndex& info,Iterator&& it,Args&&... args)
    ->decltype(
      std::declval<F>()(LocalIterator{it},std::forward<Args>(args)...))
  {
    return f(LocalIterator{it},std::forward<Args>(args)...);
  }

  F f;
};

template<typename IsTotal,typename F>
struct restitute_iterator_class<IsTotal,F>
{
  restitute_iterator_class(const F& f):f(f){}
  
  template<typename TypeIndex,typename Iterator,typename... Args>
  BOOST_FORCEINLINE auto operator()(
    const TypeIndex&,Iterator&& it,Args&&... args)
    ->decltype(
      std::declval<F>()
        (std::forward<Iterator>(it),std::forward<Args>(args)...))
  {
    return f(std::forward<Iterator>(it),std::forward<Args>(args)...);
  }

  F f;
};

template<
  typename Model,typename... Ts,typename F,typename... Args,
  typename RestitutionList=restitution_list<Model,Ts...>
>
auto restitute_iterator(const F& f,Args&&... args)->mp11::mp_apply<
  restitute_iterator_class,
  mp11::mp_append<
    mp11::mp_list<
      is_total_restitution<Model,RestitutionList>,
      decltype(tail_closure(f,std::forward<Args>(args)...))
    >,
    RestitutionList
  >
>
{
  return tail_closure(f,std::forward<Args>(args)...);
}

} /* namespace poly_collection::detail */

} /* namespace poly_collection */

} /* namespace boost */

#if defined(BOOST_MSVC)
#pragma warning(pop) /* C4714 */
#endif

#endif

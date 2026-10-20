/* Copyright 2024 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * See http://www.boost.org/libs/poly_collection for library home page.
 */

#ifndef BOOST_POLY_COLLECTION_DETAIL_FIXED_VARIANT_ITERATOR_HPP
#define BOOST_POLY_COLLECTION_DETAIL_FIXED_VARIANT_ITERATOR_HPP

#if defined(_MSC_VER)
#pragma once
#endif

#include <boost/config.hpp>
#include <boost/detail/workaround.hpp>
#include <boost/iterator/iterator_facade.hpp>
#include <boost/poly_collection/detail/fixed_variant.hpp>
#include <type_traits>

namespace boost{

namespace poly_collection{

namespace detail{

/* Iterator over the sequence of T subojects within a range of
 * fixed_variant_closure<T,fixed_variant<...>>s.
 */

template<typename Variant,typename T>
class fixed_variant_alternative_iterator:
  public boost::iterator_facade<
    fixed_variant_alternative_iterator<Variant,T>,
    T,
    boost::random_access_traversal_tag
  >
{
  static constexpr std::size_t Stride=sizeof(
    fixed_variant_impl::fixed_variant_closure<
      typename std::remove_const<T>::type,Variant>);

public:
  fixed_variant_alternative_iterator()=default;
  fixed_variant_alternative_iterator(T* p)noexcept:p{p}{}
  fixed_variant_alternative_iterator(
    const fixed_variant_alternative_iterator&)=default;
  fixed_variant_alternative_iterator& operator=(
    const fixed_variant_alternative_iterator&)=default;

  template<
    typename NonConstT,
    typename std::enable_if<
      std::is_same<T,const NonConstT>::value>::type* =nullptr
  >
  fixed_variant_alternative_iterator(
    const fixed_variant_alternative_iterator<Variant,NonConstT>& x)noexcept:
    p{x.p}{}

  template<
    typename NonConstT,
    typename std::enable_if<
      std::is_same<T,const NonConstT>::value>::type* =nullptr
  >
  fixed_variant_alternative_iterator& operator=(
    const fixed_variant_alternative_iterator<Variant,NonConstT>& x)noexcept
  {
    p=x.p;
    return *this;
  }

  /* interoperability with T* */

  fixed_variant_alternative_iterator& operator=(T* p_)noexcept
    {p=p_;return *this;}
  operator T*()const noexcept{return p;}

  std::size_t stride()const noexcept{return Stride;}

private:
  template<typename,typename>
  friend class fixed_variant_alternative_iterator;

  using char_pointer=typename std::conditional<
    std::is_const<T>::value,
    const char*,
    char*
  >::type;

  static char_pointer char_ptr(T* p)noexcept
    {return reinterpret_cast<char_pointer>(p);}
  static T*           value_ptr(char_pointer p)noexcept
    {return reinterpret_cast<T*>(p);}

  friend class boost::iterator_core_access;

  T& dereference()const noexcept{return *p;}
  bool equal(const fixed_variant_alternative_iterator& x)const noexcept
    {return p==x.p;}
  void increment()noexcept{p=value_ptr(char_ptr(p)+Stride);}
  void decrement()noexcept{p=value_ptr(char_ptr(p)-Stride);}
  template<typename Integral>
  void advance(Integral n)noexcept
    {p=value_ptr(char_ptr(p)+n*(std::ptrdiff_t)Stride);}
  std::ptrdiff_t distance_to(
    const fixed_variant_alternative_iterator& x)const noexcept
    {return (char_ptr(x.p)-char_ptr(p))/(std::ptrdiff_t)Stride;}          

  T* p;
};

/* Iterator over the sequence of fixed_variant base objects within a range of
 * fixed_variant_closure<T,fixed_variant<...>>s. As T is not part of the
 * iterator definition, the stride between values is a run-time value.
 */

template<typename Variant>
class fixed_variant_iterator:
  public boost::iterator_facade<
    fixed_variant_iterator<Variant>,
    Variant,
    boost::random_access_traversal_tag
  >
{
public:
  fixed_variant_iterator()=default;
  fixed_variant_iterator(Variant* p,std::size_t stride)noexcept:
    p{p},stride_{stride}{}
  fixed_variant_iterator(const fixed_variant_iterator&)=default;
  fixed_variant_iterator& operator=(const fixed_variant_iterator&)=default;

  template<
    typename NonConstVariant,
    typename std::enable_if<
      std::is_same<Variant,const NonConstVariant>::value>::type* =nullptr
  >
  fixed_variant_iterator(
    const fixed_variant_iterator<NonConstVariant>& x)noexcept:
    p{x.p},stride_{x.stride_}{}

  template<
    typename NonConstVariant,
    typename std::enable_if<
      std::is_same<Variant,const NonConstVariant>::value>::type* =nullptr
  >
  fixed_variant_iterator& operator=(
    const fixed_variant_iterator<NonConstVariant>& x)noexcept
  {
    p=x.p;stride_=x.stride_;
    return *this;
  }

  /* interoperability with Variant* */

  fixed_variant_iterator& operator=(Variant* p_)noexcept{p=p_;return *this;}
  operator Variant*()const noexcept{return p;}

  /* interoperability with fixed_variant_alternative_iterator<Variant,T> */

#include <boost/poly_collection/detail/begin_no_sanitize.hpp>

  template<
    typename T,
    typename NonConstT=typename std::remove_const<T>::type,
    typename NonConstVariant=typename std::remove_const<Variant>::type,
    typename std::enable_if<
      mp11::mp_contains<NonConstVariant,NonConstT>::value&&
      (!std::is_const<Variant>::value||std::is_const<T>::value)
    >::type* =nullptr
  >
  BOOST_POLY_COLLECTION_NO_SANITIZE explicit operator
    fixed_variant_alternative_iterator<NonConstVariant,T>()const noexcept
  {
    return p?std::addressof(unsafe_get<NonConstT>(*p)):nullptr;
  }

#include <boost/poly_collection/detail/end_no_sanitize.hpp>

  std::size_t stride()const noexcept{return stride_;}

private:
  template<typename>
  friend class fixed_variant_iterator;

  using char_pointer=typename std::conditional<
    std::is_const<Variant>::value,
    const char*,
    char*
  >::type;

  static char_pointer char_ptr(Variant* p)noexcept
    {return reinterpret_cast<char_pointer>(p);}
  static Variant*       value_ptr(char_pointer p)noexcept
    {return reinterpret_cast<Variant*>(p);}

  friend class boost::iterator_core_access;

  Variant& dereference()const noexcept{return *p;}
  bool equal(const fixed_variant_iterator& x)const noexcept{return p==x.p;}
  void increment()noexcept{p=value_ptr(char_ptr(p)+stride_);}
  void decrement()noexcept{p=value_ptr(char_ptr(p)-stride_);}
  template<typename Integral>
  void advance(Integral n)noexcept
    {p=value_ptr(char_ptr(p)+n*(std::ptrdiff_t)stride_);}
  std::ptrdiff_t distance_to(const fixed_variant_iterator& x)const noexcept
    {return (char_ptr(x.p)-char_ptr(p))/(std::ptrdiff_t)stride_;}          

  Variant*    p;
  std::size_t stride_;
};

} /* namespace poly_collection::detail */

} /* namespace poly_collection */

} /* namespace boost */

#endif

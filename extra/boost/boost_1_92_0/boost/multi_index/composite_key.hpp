/* Copyright 2003-2025 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * See http://www.boost.org/libs/multi_index for library home page.
 */

#ifndef BOOST_MULTI_INDEX_COMPOSITE_KEY_HPP
#define BOOST_MULTI_INDEX_COMPOSITE_KEY_HPP

#if defined(_MSC_VER)
#pragma once
#endif

#include <boost/config.hpp> /* keep it first to prevent nasty warns in MSVC */
#include <boost/container_hash/hash_fwd.hpp>
#include <boost/core/enable_if.hpp>
#include <boost/core/ref.hpp>
#include <boost/multi_index/detail/augmented_stdtuple.hpp>
#include <boost/multi_index/detail/cons_stdtuple.hpp>
#include <boost/mp11/algorithm.hpp>
#include <boost/mp11/integer_sequence.hpp>
#include <boost/mp11/list.hpp>
#include <boost/mp11/utility.hpp>
#include <boost/static_assert.hpp>
#include <boost/tuple/tuple.hpp>
#include <boost/type_traits/is_convertible.hpp>
#include <boost/type_traits/is_same.hpp>
#include <functional>
#include <type_traits>

/* A composite key stores n key extractors and "computes" the
 * result on a given value as a packed reference to the value and
 * the composite key itself. Actual invocations to the component
 * key extractors are lazily performed when executing an operation
 * on composite_key results (equality, comparison, hashing.)
 * As the other key extractors in Boost.MultiIndex, composite_key<T,...>
 * is  overloaded to work on chained pointers to T and reference_wrappers
 * of T.
 */

namespace boost{

template<class T> class reference_wrapper; /* fwd decl. */

namespace multi_index{

namespace detail{

/* Given R = composite_key_result<composite_key<Value,KeyFromValues...>>,
 * instantiate_with_transformed_keys<Class,F,R> is the type
 * Class< F<KeyFromValues::result_type>... >.
 */

template<template<typename...> class F>
struct instantiate_with_transformed_keys_helper
{
  template<typename KeyFromValue>
  using fn=F<typename KeyFromValue::result_type>;
};

template<
  template<typename...> class Class,template<typename...> class F,
  typename CompositeKeyResult
>
using instantiate_with_transformed_keys=mp11::mp_rename<
  mp11::mp_transform_q<
    instantiate_with_transformed_keys_helper<F>,
    typename CompositeKeyResult::composite_key_type::key_extractor_tuple
  >,
  Class
>;

/* used for defining equality and comparison ops of composite_key_result */

struct cons_generic_operator_equal
{
  cons_generic_operator_equal get_head()const{return {};}
  cons_generic_operator_equal get_tail()const{return {};}

  template<typename T,typename Q>
  bool operator()(const T& x,const Q& y)const{return x==y;}
};

struct cons_generic_operator_less
{
  cons_generic_operator_less get_head()const{return {};}
  cons_generic_operator_less get_tail()const{return {};}

  template<typename T,typename Q>
  bool operator()(const T& x,const Q& y)const{return x<y;}
};

/* Supporting functions for the implementation of equality, comparison and
 * hashing operations of composite_key_result.
 *
 * equal_* checks for equality between composite_key_results and
 * between those and tuples, accepting a tuple of basic equality functors.
 * compare_* does lexicographical comparison.
 * hash_* computes a combination of elementwise hash values.
 */

template<typename Value1,typename Value2,typename EqualCons>
bool equal_ckey_ckey(
  const tuples::null_type&,const Value1&,
  const tuples::null_type&,const Value2&,
  const EqualCons&)
{
  return true;
}

template<
  typename KeyCons1,typename Value1,typename KeyCons2, typename Value2,
  typename EqualCons
>
bool equal_ckey_ckey(
  const KeyCons1& c0,const Value1& v0,const KeyCons2& c1,const Value2& v1,
  const EqualCons& eq)
{
  if(!eq.get_head()(c0.get_head()(v0),c1.get_head()(v1)))return false;
  return equal_ckey_ckey(c0.get_tail(),v0,c1.get_tail(),v1,eq.get_tail());
}

template<typename Value,typename EqualCons>
bool equal_ckey_cval(
  const tuples::null_type&,const Value&,const tuples::null_type&,
  const EqualCons&)
{
  return true;
}

template<typename KeyCons,typename Value,typename ValCons,typename EqualCons>
bool equal_ckey_cval(
  const KeyCons& c,const Value& v,const ValCons& vc,const EqualCons& eq)
{
  if(!eq.get_head()(c.get_head()(v),vc.get_head()))return false;
  return equal_ckey_cval(c.get_tail(),v,vc.get_tail(),eq.get_tail());
}

template<typename Value,typename EqualCons>
bool equal_cval_ckey(
  const tuples::null_type&,const tuples::null_type&,const Value&,
  const EqualCons&)
{
  return true;
}

template<typename ValCons,typename KeyCons,typename Value,typename EqualCons>
bool equal_cval_ckey(
  const ValCons& vc,const KeyCons& c,const Value& v,const EqualCons& eq)
{
  if(!eq.get_head()(vc.get_head(),c.get_head()(v)))return false;
  return equal_cval_ckey(vc.get_tail(),c.get_tail(),v,eq.get_tail());
}

template<
  typename Value1,typename KeyCons2,typename Value2,typename CompareCons
>
bool compare_ckey_ckey(
  const tuples::null_type&,const Value1&,
  const KeyCons2&,const Value2&,
  const CompareCons&)
{
  return false;
}

template<
  typename KeyCons1,typename Value1,typename Value2,typename CompareCons
>
bool compare_ckey_ckey(
  const KeyCons1&,const Value1&,const tuples::null_type&,const Value2&,
  const CompareCons&)
{
  return false;
}

template<typename Value1,typename Value2,typename CompareCons>
bool compare_ckey_ckey(
  const tuples::null_type&,const Value1&,
  const tuples::null_type&,const Value2&,
  const CompareCons&)
{
  return false;
}

template<
  typename KeyCons1,typename Value1,typename KeyCons2, typename Value2,
  typename CompareCons
>
bool compare_ckey_ckey(
  const KeyCons1& c0,const Value1& v0,
  const KeyCons2& c1,const Value2& v1,
  const CompareCons& comp)
{
  if(comp.get_head()(c0.get_head()(v0),c1.get_head()(v1)))return true;
  if(comp.get_head()(c1.get_head()(v1),c0.get_head()(v0)))return false;
  return compare_ckey_ckey(c0.get_tail(),v0,c1.get_tail(),v1,comp.get_tail());
}

template<typename Value,typename ValCons,typename CompareCons>
bool compare_ckey_cval(
  const tuples::null_type&,const Value&,const ValCons&,const CompareCons&)
{
  return false;
}

template<typename KeyCons,typename Value,typename CompareCons>
bool compare_ckey_cval(
  const KeyCons&,const Value&,const tuples::null_type&,const CompareCons&)
{
  return false;
}

template<typename Value,typename CompareCons>
bool compare_ckey_cval(
  const tuples::null_type&,const Value&,const tuples::null_type&,
  const CompareCons&)
{
  return false;
}

template<typename KeyCons,typename Value,typename ValCons,typename CompareCons>
bool compare_ckey_cval(
  const KeyCons& c,const Value& v,const ValCons& vc,
  const CompareCons& comp)
{
  if(comp.get_head()(c.get_head()(v),vc.get_head()))return true;
  if(comp.get_head()(vc.get_head(),c.get_head()(v)))return false;
  return compare_ckey_cval(c.get_tail(),v,vc.get_tail(),comp.get_tail());
}

template<typename ValCons,typename Value,typename CompareCons>
bool compare_cval_ckey(
  const ValCons&,const tuples::null_type&,const Value&,const CompareCons&)
{
  return false;
}

template<typename KeyCons,typename Value,typename CompareCons>
bool compare_cval_ckey(
  const tuples::null_type&,const KeyCons&,const Value&,const CompareCons&)
{
  return false;
}

template<typename Value,typename CompareCons>
bool compare_cval_ckey(
  const tuples::null_type&,const tuples::null_type&,const Value&,
  const CompareCons&)
{
  return false;
}

template<typename ValCons,typename KeyCons,typename Value,typename CompareCons>
bool compare_cval_ckey(
  const ValCons& vc,const KeyCons& c,const Value& v,
  const CompareCons& comp)
{
  if(comp.get_head()(vc.get_head(),c.get_head()(v)))return true;
  if(comp.get_head()(c.get_head()(v),vc.get_head()))return false;
  return compare_cval_ckey(vc.get_tail(),c.get_tail(),v,comp.get_tail());
}

template<typename Value>
std::size_t hash_ckey(
  const tuples::null_type&,const Value&,const tuples::null_type&,
  std::size_t carry)
{
  return carry;
}

template<typename KeyCons,typename Value,typename HashCons>
std::size_t hash_ckey(
    const KeyCons& c,const Value& v,const HashCons& h,std::size_t carry=0)
{
  /* same hashing formula as boost::hash_combine */

  carry^=h.get_head()(c.get_head()(v))+0x9e3779b9+(carry<<6)+(carry>>2);
  return hash_ckey(c.get_tail(),v,h.get_tail(),carry);
}

inline std::size_t hash_cval(
  const tuples::null_type&,const tuples::null_type&,std::size_t carry)
{
    return carry;
}

template<typename ValCons,typename HashCons>
std::size_t hash_cval(const ValCons& vc,const HashCons& h,std::size_t carry=0)
{
  carry^=h.get_head()(vc.get_head())+0x9e3779b9+(carry<<6)+(carry>>2);
  return hash_cval(vc.get_tail(),h.get_tail(),carry);
}

} /* namespace multi_index::detail */

/* composite_key_result */

#if defined(BOOST_MSVC)
#pragma warning(push)
#pragma warning(disable:4512)
#endif

template<typename CompositeKey>
struct composite_key_result
{
  typedef CompositeKey                            composite_key_type;
  typedef typename composite_key_type::value_type value_type;

  composite_key_result(
    const composite_key_type& composite_key_,const value_type& value_):
    composite_key(composite_key_),value(value_)
  {}

  const composite_key_type& composite_key;
  const value_type&         value;
};

#if defined(BOOST_MSVC)
#pragma warning(pop)
#endif

/* composite_key */

template<typename Value,typename KeyFromValue,typename... KeyFromValues>
struct composite_key:
  private detail::augmented_stdtuple<KeyFromValue,KeyFromValues...>
{
private:
  typedef detail::augmented_stdtuple<KeyFromValue,KeyFromValues...> super;

public:
  typedef std::tuple<KeyFromValue,KeyFromValues...> key_extractor_tuple;
  typedef Value                                     value_type;
  typedef composite_key_result<composite_key>       result_type;

  using super::super;
  composite_key()=default;
  composite_key(const composite_key&)=default;

  const key_extractor_tuple& key_extractors()const{return *this;}
  key_extractor_tuple&       key_extractors(){return *this;}

  template<typename ChainedPtr>
  typename disable_if<
    is_convertible<const ChainedPtr&,const value_type&>,result_type>::type
  operator()(const ChainedPtr& x)const
  {
    return operator()(*x);
  }

  result_type operator()(const value_type& x)const
  {
    return result_type(*this,x);
  }

  result_type operator()(const reference_wrapper<const value_type>& x)const
  {
    return result_type(*this,x.get());
  }

  result_type operator()(const reference_wrapper<value_type>& x)const
  {
    return result_type(*this,x.get());
  }
};

/* comparison operators */

/* == */

template<typename CompositeKey1,typename CompositeKey2>
inline bool operator==(
  const composite_key_result<CompositeKey1>& x,
  const composite_key_result<CompositeKey2>& y)
{
  typedef typename CompositeKey1::key_extractor_tuple key_extractor_tuple1;
  typedef typename CompositeKey2::key_extractor_tuple key_extractor_tuple2;

  BOOST_STATIC_ASSERT(
    std::tuple_size<key_extractor_tuple1>::value==
    std::tuple_size<key_extractor_tuple2>::value);

  return detail::equal_ckey_ckey(
    detail::make_cons_stdtuple(x.composite_key.key_extractors()),x.value,
    detail::make_cons_stdtuple(y.composite_key.key_extractors()),y.value,
    detail::cons_generic_operator_equal());
}

template<typename CompositeKey,typename... Values>
inline bool operator==(
  const composite_key_result<CompositeKey>& x,
  const boost::tuple<Values...>& y)
{
  typedef typename CompositeKey::key_extractor_tuple key_extractor_tuple;
  typedef boost::tuple<Values...>                    key_tuple;
  
  BOOST_STATIC_ASSERT(
    std::tuple_size<key_extractor_tuple>::value==
    std::tuple_size<key_tuple>::value);

  return detail::equal_ckey_cval(
    detail::make_cons_stdtuple(x.composite_key.key_extractors()),x.value,
    y,detail::cons_generic_operator_equal());
}

template<typename... Values,typename CompositeKey>
inline bool operator==(
  const boost::tuple<Values...>& x,
  const composite_key_result<CompositeKey>& y)
{
  typedef typename CompositeKey::key_extractor_tuple key_extractor_tuple;
  typedef tuple<Values...>                           key_tuple;
  
  BOOST_STATIC_ASSERT(
    std::tuple_size<key_extractor_tuple>::value==
    std::tuple_size<key_tuple>::value);

  return detail::equal_cval_ckey(
    x,detail::make_cons_stdtuple(y.composite_key.key_extractors()),
    y.value,detail::cons_generic_operator_equal());
}

template<typename CompositeKey,typename... Values>
inline bool operator==(
  const composite_key_result<CompositeKey>& x,
  const std::tuple<Values...>& y)
{
  typedef typename CompositeKey::key_extractor_tuple key_extractor_tuple;
  typedef std::tuple<Values...>                      key_tuple;
  
  BOOST_STATIC_ASSERT(
    std::tuple_size<key_extractor_tuple>::value==
    std::tuple_size<key_tuple>::value);

  return detail::equal_ckey_cval(
    detail::make_cons_stdtuple(x.composite_key.key_extractors()),x.value,
    detail::make_cons_stdtuple(y),detail::cons_generic_operator_equal());
}

template<typename... Values,typename CompositeKey>
inline bool operator==(
  const std::tuple<Values...>& x,
  const composite_key_result<CompositeKey>& y)
{
  typedef typename CompositeKey::key_extractor_tuple key_extractor_tuple;
  typedef std::tuple<Values...>                      key_tuple;

  BOOST_STATIC_ASSERT(
    std::tuple_size<key_extractor_tuple>::value==
    std::tuple_size<key_tuple>::value);

  return detail::equal_cval_ckey(
    detail::make_cons_stdtuple(x),
    detail::make_cons_stdtuple(y.composite_key.key_extractors()),y.value,
    detail::cons_generic_operator_equal());
}

/* < */

template<typename CompositeKey1,typename CompositeKey2>
inline bool operator<(
  const composite_key_result<CompositeKey1>& x,
  const composite_key_result<CompositeKey2>& y)
{
  return detail::compare_ckey_ckey(
    detail::make_cons_stdtuple(x.composite_key.key_extractors()),x.value,
    detail::make_cons_stdtuple(y.composite_key.key_extractors()),y.value,
    detail::cons_generic_operator_less());
}

template<typename CompositeKey,typename... Values>
inline bool operator<(
  const composite_key_result<CompositeKey>& x,
  const boost::tuple<Values...>& y)
{
  return detail::compare_ckey_cval(
    detail::make_cons_stdtuple(x.composite_key.key_extractors()),x.value,
    y,detail::cons_generic_operator_less());
}

template<typename... Values,typename CompositeKey>
inline bool operator<(
  const boost::tuple<Values...>& x,
  const composite_key_result<CompositeKey>& y)
{
  return detail::compare_cval_ckey(
    x,detail::make_cons_stdtuple(y.composite_key.key_extractors()),
    y.value,detail::cons_generic_operator_less());
}

template<typename CompositeKey,typename... Values>
inline bool operator<(
  const composite_key_result<CompositeKey>& x,
  const std::tuple<Values...>& y)
{
  return detail::compare_ckey_cval(
    detail::make_cons_stdtuple(x.composite_key.key_extractors()),x.value,
    detail::make_cons_stdtuple(y),detail::cons_generic_operator_less());
}

template<typename... Values,typename CompositeKey>
inline bool operator<(
  const std::tuple<Values...>& x,
  const composite_key_result<CompositeKey>& y)
{
  return detail::compare_cval_ckey(
    detail::make_cons_stdtuple(x),
    detail::make_cons_stdtuple(y.composite_key.key_extractors()),y.value,
    detail::cons_generic_operator_less());
}

/* rest of comparison operators */

#define BOOST_MULTI_INDEX_CK_COMPLETE_COMP_OPS(t1,t2,a1,a2)                  \
template<t1,t2> inline bool operator!=(const a1& x,const a2& y)              \
{                                                                            \
  return !(x==y);                                                            \
}                                                                            \
                                                                             \
template<t1,t2> inline bool operator>(const a1& x,const a2& y)               \
{                                                                            \
  return y<x;                                                                \
}                                                                            \
                                                                             \
template<t1,t2> inline bool operator>=(const a1& x,const a2& y)              \
{                                                                            \
  return !(x<y);                                                             \
}                                                                            \
                                                                             \
template<t1,t2> inline bool operator<=(const a1& x,const a2& y)              \
{                                                                            \
  return !(y<x);                                                             \
}

BOOST_MULTI_INDEX_CK_COMPLETE_COMP_OPS(
  typename CompositeKey1,
  typename CompositeKey2,
  composite_key_result<CompositeKey1>,
  composite_key_result<CompositeKey2>
)

BOOST_MULTI_INDEX_CK_COMPLETE_COMP_OPS(
  typename CompositeKey,
  typename... Values,
  composite_key_result<CompositeKey>,
  boost::tuple<Values...>
)

BOOST_MULTI_INDEX_CK_COMPLETE_COMP_OPS(
  typename... Values,
  typename CompositeKey,
  boost::tuple<Values...>,
  composite_key_result<CompositeKey>
)

BOOST_MULTI_INDEX_CK_COMPLETE_COMP_OPS(
  typename CompositeKey,
  typename... Values,
  composite_key_result<CompositeKey>,
  std::tuple<Values...>
)

BOOST_MULTI_INDEX_CK_COMPLETE_COMP_OPS(
  typename... Values,
  typename CompositeKey,
  std::tuple<Values...>,
  composite_key_result<CompositeKey>
)

#undef BOOST_MULTI_INDEX_CK_COMPLETE_COMP_OPS

/* composite_key_equal_to */

template<typename Pred,typename... Preds>
struct composite_key_equal_to:
  private detail::augmented_stdtuple<Pred,Preds...>
{
private:
  typedef detail::augmented_stdtuple<Pred,Preds...> super;

public:
  typedef std::tuple<Pred,Preds...> key_eq_tuple;

  using super::super;
  composite_key_equal_to()=default;
  composite_key_equal_to(const composite_key_equal_to&)=default;

  const key_eq_tuple& key_eqs()const{return *this;}
  key_eq_tuple&       key_eqs(){return *this;}

  template<typename CompositeKey1,typename CompositeKey2>
  bool operator()(
    const composite_key_result<CompositeKey1> & x,
    const composite_key_result<CompositeKey2> & y)const
  {
    typedef typename CompositeKey1::key_extractor_tuple key_extractor_tuple1;
    typedef typename CompositeKey2::key_extractor_tuple key_extractor_tuple2;

    BOOST_STATIC_ASSERT(
      std::tuple_size<key_extractor_tuple1>::value<=
      std::tuple_size<key_eq_tuple>::value&&
      std::tuple_size<key_extractor_tuple1>::value==
      std::tuple_size<key_extractor_tuple2>::value);

    return detail::equal_ckey_ckey(
      detail::make_cons_stdtuple(x.composite_key.key_extractors()),x.value,
      detail::make_cons_stdtuple(y.composite_key.key_extractors()),y.value,
      detail::make_cons_stdtuple(key_eqs()));
  }
  
  template<typename CompositeKey,typename... Values>
  bool operator()(
    const composite_key_result<CompositeKey>& x,
    const boost::tuple<Values...>& y)const
  {
    typedef typename CompositeKey::key_extractor_tuple key_extractor_tuple;
    typedef boost::tuple<Values...>                    key_tuple;

    BOOST_STATIC_ASSERT(
      std::tuple_size<key_extractor_tuple>::value<=
      std::tuple_size<key_eq_tuple>::value&&
      std::tuple_size<key_extractor_tuple>::value==
      std::tuple_size<key_tuple>::value);

    return detail::equal_ckey_cval(
      detail::make_cons_stdtuple(x.composite_key.key_extractors()),x.value,
      y,detail::make_cons_stdtuple(key_eqs()));
  }

  template<typename... Values,typename CompositeKey>
  bool operator()(
    const boost::tuple<Values...>& x,
    const composite_key_result<CompositeKey>& y)const
  {
    typedef typename CompositeKey::key_extractor_tuple key_extractor_tuple;
    typedef boost::tuple<Values...>                    key_tuple;

    BOOST_STATIC_ASSERT(
      std::tuple_size<key_tuple>::value<=
      std::tuple_size<key_eq_tuple>::value&&
      std::tuple_size<key_tuple>::value==
      std::tuple_size<key_extractor_tuple>::value);

    return detail::equal_cval_ckey(
      x,detail::make_cons_stdtuple(y.composite_key.key_extractors()),
      y.value,detail::make_cons_stdtuple(key_eqs()));
  }

  template<typename CompositeKey,typename... Values>
  bool operator()(
    const composite_key_result<CompositeKey>& x,
    const std::tuple<Values...>& y)const
  {
    typedef typename CompositeKey::key_extractor_tuple key_extractor_tuple;
    typedef std::tuple<Values...>                      key_tuple;

    BOOST_STATIC_ASSERT(
      std::tuple_size<key_extractor_tuple>::value<=
      std::tuple_size<key_eq_tuple>::value&&
      std::tuple_size<key_extractor_tuple>::value==
      std::tuple_size<key_tuple>::value);

    return detail::equal_ckey_cval(
      detail::make_cons_stdtuple(x.composite_key.key_extractors()),x.value,
      detail::make_cons_stdtuple(y),
      detail::make_cons_stdtuple(key_eqs()));
  }

  template<typename... Values,typename CompositeKey>
  bool operator()(
    const std::tuple<Values...>& x,
    const composite_key_result<CompositeKey>& y)const
  {
    typedef typename CompositeKey::key_extractor_tuple key_extractor_tuple;
    typedef std::tuple<Values...>                      key_tuple;

    BOOST_STATIC_ASSERT(
      std::tuple_size<key_tuple>::value<=
      std::tuple_size<key_eq_tuple>::value&&
      std::tuple_size<key_tuple>::value==
      std::tuple_size<key_extractor_tuple>::value);

    return detail::equal_cval_ckey(
      detail::make_cons_stdtuple(x),
      detail::make_cons_stdtuple(y.composite_key.key_extractors()),
      y.value,detail::make_cons_stdtuple(key_eqs()));
  }
};

/* composite_key_compare */

template<typename Compare,typename... Compares>
struct composite_key_compare:
  private detail::augmented_stdtuple<Compare,Compares...>
{
private:
  typedef detail::augmented_stdtuple<Compare,Compares...> super;

public:
  typedef std::tuple<Compare,Compares...> key_comp_tuple;

  using super::super;
  composite_key_compare()=default;
  composite_key_compare(const composite_key_compare&)=default;

  const key_comp_tuple& key_comps()const{return *this;}
  key_comp_tuple&       key_comps(){return *this;}

  template<typename CompositeKey1,typename CompositeKey2>
  bool operator()(
    const composite_key_result<CompositeKey1> & x,
    const composite_key_result<CompositeKey2> & y)const
  {
    typedef typename CompositeKey1::key_extractor_tuple key_extractor_tuple1;
    typedef typename CompositeKey2::key_extractor_tuple key_extractor_tuple2;

    BOOST_STATIC_ASSERT(
      std::tuple_size<key_extractor_tuple1>::value<=
      std::tuple_size<key_comp_tuple>::value||
      std::tuple_size<key_extractor_tuple2>::value<=
      std::tuple_size<key_comp_tuple>::value);

    return detail::compare_ckey_ckey(
      detail::make_cons_stdtuple(x.composite_key.key_extractors()),x.value,
      detail::make_cons_stdtuple(y.composite_key.key_extractors()),y.value,
      detail::make_cons_stdtuple(key_comps()));
  }
  
  template<typename CompositeKey,typename Value>
  bool operator()(
    const composite_key_result<CompositeKey>& x,
    const Value& y)const
  {
    return operator()(x,boost::make_tuple(boost::cref(y)));
  }

  template<typename CompositeKey,typename... Values>
  bool operator()(
    const composite_key_result<CompositeKey>& x,
    const boost::tuple<Values...>& y)const
  {
    typedef typename CompositeKey::key_extractor_tuple key_extractor_tuple;
    typedef boost::tuple<Values...>                    key_tuple;

    BOOST_STATIC_ASSERT(
      std::tuple_size<key_extractor_tuple>::value<=
      std::tuple_size<key_comp_tuple>::value||
      std::tuple_size<key_tuple>::value<=
      std::tuple_size<key_comp_tuple>::value);

    return detail::compare_ckey_cval(
      detail::make_cons_stdtuple(x.composite_key.key_extractors()),x.value,
      y,detail::make_cons_stdtuple(key_comps()));
  }

  template<typename Value,typename CompositeKey>
  bool operator()(
    const Value& x,
    const composite_key_result<CompositeKey>& y)const
  {
    return operator()(boost::make_tuple(boost::cref(x)),y);
  }

  template<typename... Values,typename CompositeKey>
  bool operator()(
    const boost::tuple<Values...>& x,
    const composite_key_result<CompositeKey>& y)const
  {
    typedef typename CompositeKey::key_extractor_tuple key_extractor_tuple;
    typedef boost::tuple<Values...>                    key_tuple;

    BOOST_STATIC_ASSERT(
      std::tuple_size<key_tuple>::value<=
      std::tuple_size<key_comp_tuple>::value||
      std::tuple_size<key_extractor_tuple>::value<=
      std::tuple_size<key_comp_tuple>::value);

    return detail::compare_cval_ckey(
      x,detail::make_cons_stdtuple(y.composite_key.key_extractors()),
      y.value,detail::make_cons_stdtuple(key_comps()));
  }

  template<typename CompositeKey,typename... Values>
  bool operator()(
    const composite_key_result<CompositeKey>& x,
    const std::tuple<Values...>& y)const
  {
    typedef typename CompositeKey::key_extractor_tuple key_extractor_tuple;
    typedef std::tuple<Values...>                      key_tuple;

    BOOST_STATIC_ASSERT(
      std::tuple_size<key_extractor_tuple>::value<=
      std::tuple_size<key_comp_tuple>::value||
      std::tuple_size<key_tuple>::value<=
      std::tuple_size<key_comp_tuple>::value);

    return detail::compare_ckey_cval(
      detail::make_cons_stdtuple(x.composite_key.key_extractors()),x.value,
      detail::make_cons_stdtuple(y),
      detail::make_cons_stdtuple(key_comps()));
  }

  template<typename... Values,typename CompositeKey>
  bool operator()(
    const std::tuple<Values...>& x,
    const composite_key_result<CompositeKey>& y)const
  {
    typedef typename CompositeKey::key_extractor_tuple key_extractor_tuple;
    typedef std::tuple<Values...>                      key_tuple;

    BOOST_STATIC_ASSERT(
      std::tuple_size<key_tuple>::value<=
      std::tuple_size<key_comp_tuple>::value||
      std::tuple_size<key_extractor_tuple>::value<=
      std::tuple_size<key_comp_tuple>::value);

    return detail::compare_cval_ckey(
      detail::make_cons_stdtuple(x),
      detail::make_cons_stdtuple(y.composite_key.key_extractors()),
      y.value,detail::make_cons_stdtuple(key_comps()));
  }
};

/* composite_key_hash */

template<typename Hash,typename... Hashes>
struct composite_key_hash:private detail::augmented_stdtuple<Hash,Hashes...>
{
private:
  typedef detail::augmented_stdtuple<Hash,Hashes...> super;

public:
  typedef std::tuple<Hash,Hashes...> key_hasher_tuple;

  using super::super;
  composite_key_hash()=default;
  composite_key_hash(const composite_key_hash&)=default;

  const key_hasher_tuple& key_hash_functions()const{return *this;}
  key_hasher_tuple&       key_hash_functions(){return *this;}

  template<typename CompositeKey>
  std::size_t operator()(const composite_key_result<CompositeKey> & x)const
  {
    typedef typename CompositeKey::key_extractor_tuple key_extractor_tuple;

    BOOST_STATIC_ASSERT(
      std::tuple_size<key_extractor_tuple>::value==
      std::tuple_size<key_hasher_tuple>::value);

    return detail::hash_ckey(
      detail::make_cons_stdtuple(x.composite_key.key_extractors()),
      x.value,detail::make_cons_stdtuple(key_hash_functions()));
  }
  
  template<typename... Values>
  std::size_t operator()(const boost::tuple<Values...>& x)const
  {
    typedef boost::tuple<Values...> key_tuple;

    BOOST_STATIC_ASSERT(
      std::tuple_size<key_tuple>::value==
      std::tuple_size<key_hasher_tuple>::value);

    return detail::hash_cval(
      x,detail::make_cons_stdtuple(key_hash_functions()));
  }

  template<typename... Values>
  std::size_t operator()(const std::tuple<Values...>& x)const
  {
    typedef std::tuple<Values...> key_tuple;

    BOOST_STATIC_ASSERT(
      std::tuple_size<key_tuple>::value==
      std::tuple_size<key_hasher_tuple>::value);

    return detail::hash_cval(
      detail::make_cons_stdtuple(x),
      detail::make_cons_stdtuple(key_hash_functions()));
  }
};

/* Instantiations of the former functors with "natural" basic components:
 * composite_key_result_equal_to uses std::equal_to of the values.
 * composite_key_result_less     uses std::less.
 * composite_key_result_greater  uses std::greater.
 * composite_key_result_hash     uses boost::hash.
 */

template<typename CompositeKeyResult>
struct composite_key_result_equal_to:
  private detail::instantiate_with_transformed_keys<
    boost::multi_index::composite_key_equal_to,std::equal_to,CompositeKeyResult
  >
{
private:
  typedef detail::instantiate_with_transformed_keys<
    boost::multi_index::composite_key_equal_to,std::equal_to,CompositeKeyResult
  > super;

public:
  typedef CompositeKeyResult  first_argument_type;
  typedef first_argument_type second_argument_type;
  typedef bool                result_type;

  using super::operator();
};

template<typename CompositeKeyResult>
struct composite_key_result_less:
  private detail::instantiate_with_transformed_keys<
    boost::multi_index::composite_key_compare,std::less,CompositeKeyResult
  >
{
private:
  typedef detail::instantiate_with_transformed_keys<
    boost::multi_index::composite_key_compare,std::less,CompositeKeyResult
  > super;

public:
  typedef CompositeKeyResult  first_argument_type;
  typedef first_argument_type second_argument_type;
  typedef bool                result_type;

  using super::operator();
};

template<typename CompositeKeyResult>
struct composite_key_result_greater:
  private detail::instantiate_with_transformed_keys<
    boost::multi_index::composite_key_compare,std::greater,CompositeKeyResult
  >
{
private:
  typedef detail::instantiate_with_transformed_keys<
    boost::multi_index::composite_key_compare,std::greater,CompositeKeyResult
  > super;

public:
  typedef CompositeKeyResult  first_argument_type;
  typedef first_argument_type second_argument_type;
  typedef bool                result_type;

  using super::operator();
};

template<typename CompositeKeyResult>
struct composite_key_result_hash:
  private detail::instantiate_with_transformed_keys<
    boost::multi_index::composite_key_hash,boost::hash,CompositeKeyResult
  >
{
private:
  typedef detail::instantiate_with_transformed_keys<
    boost::multi_index::composite_key_hash,boost::hash,CompositeKeyResult
  > super;

public:
  typedef CompositeKeyResult argument_type;
  typedef std::size_t        result_type;

  using super::operator();
};

} /* namespace multi_index */

} /* namespace boost */

/* Specializations of std::equal_to, std::less, std::greater and boost::hash
 * for composite_key_results enabling interoperation with tuples of values.
 */

namespace std{

template<typename CompositeKey>
struct equal_to<boost::multi_index::composite_key_result<CompositeKey> >:
  boost::multi_index::composite_key_result_equal_to<
    boost::multi_index::composite_key_result<CompositeKey>
  >
{
};

template<typename CompositeKey>
struct less<boost::multi_index::composite_key_result<CompositeKey> >:
  boost::multi_index::composite_key_result_less<
    boost::multi_index::composite_key_result<CompositeKey>
  >
{
};

template<typename CompositeKey>
struct greater<boost::multi_index::composite_key_result<CompositeKey> >:
  boost::multi_index::composite_key_result_greater<
    boost::multi_index::composite_key_result<CompositeKey>
  >
{
};

} /* namespace std */

namespace boost{

template<typename CompositeKey>
struct hash<boost::multi_index::composite_key_result<CompositeKey> >:
  boost::multi_index::composite_key_result_hash<
    boost::multi_index::composite_key_result<CompositeKey>
  >
{
};

} /* namespace boost */

#endif

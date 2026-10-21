/* Copyright 2024-2026 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * See http://www.boost.org/libs/poly_collection for library home page.
 */

#ifndef BOOST_POLY_COLLECTION_DETAIL_FIXED_VARIANT_HPP
#define BOOST_POLY_COLLECTION_DETAIL_FIXED_VARIANT_HPP

#if defined(_MSC_VER)
#pragma once
#endif

#include <boost/config.hpp>
#include <boost/core/addressof.hpp>
#include <boost/detail/workaround.hpp>
#include <boost/mp11/algorithm.hpp>
#include <boost/mp11/function.hpp>
#include <boost/mp11/list.hpp>
#include <boost/mp11/set.hpp>
#include <boost/mp11/tuple.hpp>
#include <boost/mp11/utility.hpp>
#include <boost/poly_collection/detail/is_equality_comparable.hpp>
#include <boost/poly_collection/detail/is_nothrow_eq_comparable.hpp>
#include <boost/type_traits/has_equal_to.hpp>
#include <boost/type_traits/has_greater.hpp>
#include <boost/type_traits/has_greater_equal.hpp>
#include <boost/type_traits/has_less.hpp>
#include <boost/type_traits/has_less_equal.hpp>
#include <boost/type_traits/has_not_equal_to.hpp>
#include <boost/type_traits/is_constructible.hpp>
#include <cstddef>
#include <limits>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace boost{

namespace poly_collection{

namespace fixed_variant_impl{

/* fixed_variant behaves as a variant without the possibility to change the
 * initial alternative. The referenced value does not belong in fixed_variant
 * but assumed to be right before it, which layout is implemented by
 * fixed_variant_closure<T,fixed_variant<...>>. This approach allows us to pack
 * same-alternative fixed_variants optimally, without reserving space for
 * the largest alternative type.
 */

template<typename... Ts>
class fixed_variant
{
  static_assert(
    !mp11::mp_empty<fixed_variant>::value,
    "the variant can't be specified with zero types");
  static_assert(
    mp11::mp_is_set<fixed_variant>::value,
    "all types in the variant must be distinct");

  static constexpr std::size_t N=sizeof...(Ts);
  using index_type=mp11::mp_cond<
    mp11::mp_bool<
      (N<=(std::numeric_limits<unsigned char>::max)())>,unsigned char,
    mp11::mp_bool<
      (N<=(std::numeric_limits<unsigned short>::max)())>,unsigned short,
    mp11::mp_true, std::size_t
  >;

public:
  template<
    typename T,
    std::size_t I=mp11::mp_find<fixed_variant,T>::value,
    typename std::enable_if<
      (I<mp11::mp_size<fixed_variant>::value)>::type* =nullptr
  >
  explicit fixed_variant(const T&):index_{static_cast<index_type>(I)}{}

  std::size_t index()const noexcept{return index_;}
  bool        valueless_by_exception()const noexcept{return false;}

#if BOOST_WORKAROUND(BOOST_MSVC,<1920)
  /* spurious C2248 when perfect forwarding fixed_variant */
#else
protected:
  fixed_variant(const fixed_variant&)=default;
  fixed_variant& operator=(const fixed_variant&)=default;
#endif

private:
  index_type index_;
};

template<typename T>
struct fixed_variant_store
{
  template<typename... Args>
  fixed_variant_store(Args&&... args):value{std::forward<Args>(args)...}{}

  T value;
};

template<typename T,typename Base>
class fixed_variant_closure:public fixed_variant_store<T>,public Base
{
public:
  template<
    typename... Args,
    typename std::enable_if<
      std::is_constructible<T,Args&&...>::value
    >::type* =nullptr
  >
  fixed_variant_closure(Args&&... args)
    noexcept(std::is_nothrow_constructible<T,Args&&...>::value):
    fixed_variant_store<T>{std::forward<Args>(args)...},
    Base{this->value}
    {}

  fixed_variant_closure(const fixed_variant_closure&)=default;
  fixed_variant_closure(fixed_variant_closure&&)=default;
  fixed_variant_closure& operator=(fixed_variant_closure&&)=default;

  template<
    typename Q=T,
    typename std::enable_if<
      detail::is_equality_comparable<Q>::value,bool>::type* =nullptr
  >
  bool operator==(const fixed_variant_closure& x)const
    noexcept(detail::is_nothrow_equality_comparable<T>::value)
  {
    return this->value==x.value;
  }
};

struct bad_variant_access:std::exception
{
  bad_variant_access()noexcept{}
  const char* what()const noexcept{return "bad variant access";}
};

template<typename T> struct variant_size;
template<typename... Ts>
struct variant_size<fixed_variant<Ts...>>:
  std::integral_constant<std::size_t, sizeof...(Ts)>{};
template<typename T> struct variant_size<const T>:variant_size<T>{};

#ifndef BOOST_NO_CXX14_VARIABLE_TEMPLATES
template<typename T>
constexpr std::size_t variant_size_v=variant_size<T>::value;
#endif

template<std::size_t I,typename T,typename=void> struct variant_alternative;

template<typename T> struct is_fixed_variant:std::false_type{};
template<typename... Ts>
struct is_fixed_variant<fixed_variant<Ts...>>:std::true_type{};

template<typename T,typename Q> 
struct transfer_cref{using type=Q;};
template<typename T,typename Q>
struct transfer_cref<const T,Q>{using type=const Q;};
template<typename T,typename Q>
struct transfer_cref<T&,Q>
{using type=typename transfer_cref<T,Q>::type&;};
template<typename T,typename Q>
struct transfer_cref<T&&,Q>
{using type=typename transfer_cref<T,Q>::type&&;};

template<std::size_t I,typename V>
struct variant_alternative<
  I,V,
  typename std::enable_if<
    is_fixed_variant<typename std::decay<V>::type>::value
  >::type
>
{
  using type=typename transfer_cref<
    V,mp11::mp_at_c<typename std::decay<V>::type,I>>::type;
};

template<std::size_t I,typename V>
using variant_alternative_t=typename variant_alternative<I,V>::type;

template<typename T,typename... Ts>
bool holds_alternative(const fixed_variant<Ts...>& x)noexcept
{
  static_assert(
    mp11::mp_contains<fixed_variant<Ts...>,T>::value,
    "type must be one of the variant alternatives");
  return x.index()==mp11::mp_find<fixed_variant<Ts...>,T>::value;
}

template<typename T,typename... Ts>
const T& unsafe_get(const fixed_variant<Ts...>& x)
{
  return static_cast<const fixed_variant_closure<T,fixed_variant<Ts...>>&>
    (x).value;
}

template<typename T,typename... Ts>
T& unsafe_get(fixed_variant<Ts...>& x)
{
  return const_cast<T&>(
    unsafe_get<T>(const_cast<const fixed_variant<Ts...>&>(x)));
}

template<typename T,typename... Ts>
const T&& unsafe_get(const fixed_variant<Ts...>&& x)
{
  return std::move(unsafe_get<T>(x));
}

template<typename T,typename... Ts>
T&& unsafe_get(fixed_variant<Ts...>&& x)
{
  return std::move(unsafe_get<T>(x));
}

template<std::size_t I,typename V>
variant_alternative_t<I,V&&> unsafe_get(V&& x)
{
  using raw_variant=typename std::decay<V>::type;

  return unsafe_get<mp11::mp_at_c<raw_variant,I>>(std::forward<V>(x));
}

template<std::size_t I,typename V>
variant_alternative_t<I,V&&> get(V&& x)
{
  using raw_variant=typename std::decay<V>::type;
  static_assert(
    I<mp11::mp_size<raw_variant>::value,
    "index must be less than the number of alternatives");

  if(x.index()!=I)throw bad_variant_access{};
  else return unsafe_get<I>(std::forward<V>(x));
}

template<typename T,typename V>
auto get(V&& x)->decltype(unsafe_get<T>(std::forward<V>(x)))
{
  using raw_variant=typename std::decay<V>::type;
  static_assert(
    mp11::mp_contains<raw_variant,T>::value,
    "type must be one of the variant alternatives");

  if(!holds_alternative<T>(x))throw bad_variant_access{};
  else return unsafe_get<T>(std::forward<V>(x));
}

template<std::size_t I,typename... Ts>
variant_alternative_t<I,fixed_variant<Ts...>>*
get_if(fixed_variant<Ts...>* px)noexcept
{
  if(!px||px->index()!=I)return nullptr;
  else return std::addressof(unsafe_get<I>(*px));
}

template<std::size_t I,typename... Ts>
const variant_alternative_t<I,fixed_variant<Ts...>>*
get_if(const fixed_variant<Ts...>* px)noexcept
{
  return get_if<I>(const_cast<fixed_variant<Ts...>*>(px));
}

template<typename T,typename... Ts>
T* get_if(fixed_variant<Ts...>* px)noexcept
{
  if(!px||!holds_alternative<T>(*px))return nullptr;
  else return std::addressof(unsafe_get<T>(*px));
}

template<typename T,typename... Ts>
const T* get_if(const fixed_variant<Ts...>* px)noexcept
{
  return get_if<T>(const_cast<fixed_variant<Ts...>*>(px));
}

struct deduced;

template<typename R,typename F,typename... Vs>
struct return_type_impl
{
  using type=R;
};

template<typename F,typename... Vs>
struct return_type_impl<deduced,F,Vs...>
{
  using type=decltype(std::declval<F>()(get<0>(std::declval<Vs>())...));
};

template<typename R,typename F,typename... Vs>
using return_type=typename return_type_impl<R,F,Vs...>::type;

template<typename R,typename F,typename... Vs>
struct visit_helper;

template<typename R,typename F>
struct visit_helper<R,F>
{
  F&& f;

  R operator()(){return std::forward<F>(f)();}
};

template<typename F>
struct visit_helper<void,F>
{
  F&& f;

  void operator()(){(void)std::forward<F>(f)();}
};

template<
  typename R=deduced,typename F,
  typename ReturnType=return_type<R,F&&>
>
ReturnType visit(F&& f)
{
  return visit_helper<ReturnType,F>{std::forward<F>(f)}();
}

template<typename R,typename F,typename V>
struct visit_helper<R,F,V>
{
  F&& f;
  V&& x;

  template<typename I>
  R operator()(I)
  {
    return std::forward<F>(f)(unsafe_get<I::value>(std::forward<V>(x)));
  }
};

template<typename F,typename V>
struct visit_helper<void,F,V>
{
  F&& f;
  V&& x;

  template<typename I>
  void operator()(I)
  {
    (void)std::forward<F>(f)(unsafe_get<I::value>(std::forward<V>(x)));
  }
};

template<
  typename R=deduced,typename F,typename V,
  typename ReturnType=return_type<R,F&&,V&&>
>
ReturnType visit(F&& f,V&& x)
{
  using raw_variant=typename std::decay<V>::type;

  return mp11::mp_with_index<mp11::mp_size<raw_variant>::value>(
    x.index(),
    visit_helper<ReturnType,F,V>{std::forward<F>(f),std::forward<V>(x)});
}

template<typename R,typename F,typename V,typename I>
struct bound_f
{
  F&& f;
  V&& x;

  template<typename... Args>
  R operator()(Args&&... xs)
  {
    return std::forward<F>(f)(
      unsafe_get<I::value>(std::forward<V>(x)),std::forward<Args>(xs)...);
  }
};

template<typename F,typename V,typename I>
struct bound_f<void,F,V,I>
{
  F&& f;
  V&& x;

  template<typename... Args>
  void operator()(Args&&... xs)
  {
    (void)std::forward<F>(f)(
      unsafe_get<I::value>(std::forward<V>(x)),std::forward<Args>(xs)...);
  }
};

template<typename R,typename F>
struct bound_visit;

template<typename R,typename F,typename V,typename... Vs>
struct visit_helper<R,F,V,Vs...>
{
  F&&                 f;
  V&&                 x;
  std::tuple<Vs&&...> xs;

  template<typename I>
  R operator()(I)
  {
    return mp11::tuple_apply(
      bound_visit<R,bound_f<R,F,V,I>>{{std::forward<F>(f),std::forward<V>(x)}},
      std::move(xs));
  }
};

template<
  typename R=deduced,typename F,typename V1,typename V2,typename... Vs,
  typename ReturnType=return_type<R,F&&,V1&&,V2&&,Vs&&...>
>
ReturnType visit(F&& f,V1&& x1,V2&& x2,Vs&&... xs)
{
  using raw_variant=typename std::decay<V1>::type;

  return mp11::mp_with_index<mp11::mp_size<raw_variant>::value>(
    x1.index(),
    visit_helper<ReturnType,F,V1,V2,Vs...>{
      std::forward<F>(f),std::forward<V1>(x1),
      std::forward_as_tuple(std::forward<V2>(x2),std::forward<Vs>(xs)...)});
}

template<typename R,typename F>
struct bound_visit
{
  F&& f;

  template<typename... Vs>
  R operator()(Vs&&... xs)
  {
    return visit<R>(std::forward<F>(f),std::forward<Vs>(xs)...);
  }
};

template<typename R,typename V,typename... Fs>
struct return_type_by_index_impl
{
  using type=R;
};

template<typename V,typename F,typename... Fs>
struct return_type_by_index_impl<deduced,V,F,Fs...>
{
  using type=decltype(std::declval<F>()(get<0>(std::declval<V>())));
};

template<typename R,typename V,typename... Fs>
using return_type_by_index=typename return_type_by_index_impl<R,V,Fs...>::type;

template<typename R,typename V,typename... Fs>
struct visit_by_index_helper
{
  V&&                 x;
  std::tuple<Fs&&...> fs;

  template<typename I>
  R operator()(I)
  {
    return std::get<I::value>(std::move(fs))(
      unsafe_get<I::value>(std::forward<V>(x)));
  }
};

template<typename V,typename... Fs>
struct visit_by_index_helper<void,V,Fs...>
{
  V&&                 x;
  std::tuple<Fs&&...> fs;

  template<typename I>
  void operator()(I)
  {
    (void)std::get<I::value>(std::move(fs))(
      unsafe_get<I::value>(std::forward<V>(x)));
  }
};

template<
  typename R=deduced,typename V,typename... Fs,
  typename ReturnType=return_type_by_index<R,V&&,Fs&&...>
>
ReturnType visit_by_index(V&& x,Fs&&... fs)
{
  using raw_variant=typename std::decay<V>::type;
  static_assert(
    mp11::mp_size<raw_variant>::value==sizeof...(Fs),
    "the number of function objects must be the same as that of the "
    "alternative types in the variant");

  return mp11::mp_with_index<mp11::mp_size<raw_variant>::value>(
    x.index(),
    visit_by_index_helper<ReturnType,V,Fs...>{
      std::forward<V>(x),
      std::forward_as_tuple(std::forward<Fs>(fs)...)});
}

template<typename RelOp,typename ... Ts>
struct relop_helper
{
  const fixed_variant<Ts...> &x,&y;

  template<typename I> bool operator()(I)const
  {
    return RelOp{}(unsafe_get<I::value>(x),unsafe_get<I::value>(y));
  }
};

struct eq_
{
  template<typename T>
  bool operator()(const T& x,const T& y)const{return x==y;}
};

template<typename T>
using eq_constraint=has_equal_to<T,T,bool>;

template<
  typename... Ts,
  typename std::enable_if<
    mp11::mp_all<eq_constraint<Ts>...>::value>::type* =nullptr
>
bool operator==(
  const fixed_variant<Ts...>& x,const fixed_variant<Ts...>& y)
{
  return
    x.index()==y.index()&&
    mp11::mp_with_index<sizeof...(Ts)>(x.index(),relop_helper<eq_,Ts...>{x,y});
}

struct neq_
{
  template<typename T>
  bool operator()(const T& x,const T& y)const{return x!=y;}
};

template<typename T>
using neq_constraint=has_not_equal_to<T,T,bool>;

template<
  typename... Ts,
  typename std::enable_if<
    mp11::mp_all<neq_constraint<Ts>...>::value>::type* =nullptr
>
bool operator!=(
  const fixed_variant<Ts...>& x,const fixed_variant<Ts...>& y)
{
  return
    x.index()!=y.index()||
    mp11::mp_with_index<sizeof...(Ts)>(
      x.index(),relop_helper<neq_,Ts...>{x,y});
}

struct lt_
{
  template<typename T>
  bool operator()(const T& x,const T& y)const{return x<y;}
};

template<typename T>
using lt_constraint=has_less<T,T,bool>;

template<
  typename... Ts,
  typename std::enable_if<
    mp11::mp_all<lt_constraint<Ts>...>::value>::type* =nullptr
>
bool operator<(
  const fixed_variant<Ts...>& x,const fixed_variant<Ts...>& y)
{
  return
    x.index()<y.index()||
    (x.index()==y.index()&&mp11::mp_with_index<sizeof...(Ts)>(
      x.index(),relop_helper<lt_,Ts...>{x,y}));
}

struct lte_
{
  template<typename T>
  bool operator()(const T& x,const T& y)const{return x<=y;}
};

template<typename T>
using lte_constraint=has_less_equal<T,T,bool>;

template<
  typename... Ts,
  typename std::enable_if<
    mp11::mp_all<lte_constraint<Ts>...>::value>::type* =nullptr
>
bool operator<=(
  const fixed_variant<Ts...>& x,const fixed_variant<Ts...>& y)
{
  return
    x.index()<y.index()||
    (x.index()==y.index()&&mp11::mp_with_index<sizeof...(Ts)>(
      x.index(),relop_helper<lte_,Ts...>{x,y}));
}

struct gt_
{
  template<typename T>
  bool operator()(const T& x,const T& y)const{return x>y;}
};

template<typename T>
using gt_constraint=has_greater<T,T,bool>;

template<
  typename... Ts,
  typename std::enable_if<
    mp11::mp_all<gt_constraint<Ts>...>::value>::type* =nullptr
>
bool operator>(
  const fixed_variant<Ts...>& x,const fixed_variant<Ts...>& y)
{
  return
    x.index()>y.index()||
    (x.index()==y.index()&&mp11::mp_with_index<sizeof...(Ts)>(
      x.index(),relop_helper<gt_,Ts...>{x,y}));
}

struct gte_
{
  template<typename T>
  bool operator()(const T& x,const T& y)const{return x>=y;}
};

template<typename T>
using gte_constraint=has_greater_equal<T,T,bool>;

template<
  typename... Ts,
  typename std::enable_if<
    mp11::mp_all<gte_constraint<Ts>...>::value>::type* =nullptr
>
bool operator>=(
  const fixed_variant<Ts...>& x,const fixed_variant<Ts...>& y)
{
  return
    x.index()>y.index()||
    (x.index()==y.index()&&mp11::mp_with_index<sizeof...(Ts)>(
      x.index(),relop_helper<gte_,Ts...>{x,y}));
}

} /* namespace poly_collection::fixed_variant_impl */

using boost::poly_collection::fixed_variant_impl::variant_size;

#ifndef BOOST_NO_CXX14_VARIABLE_TEMPLATES
using boost::poly_collection::fixed_variant_impl::variant_size_v;
#endif

using boost::poly_collection::fixed_variant_impl::bad_variant_access;
using boost::poly_collection::fixed_variant_impl::variant_alternative;
using boost::poly_collection::fixed_variant_impl::variant_alternative_t;
using boost::poly_collection::fixed_variant_impl::visit;
using boost::poly_collection::fixed_variant_impl::visit_by_index;
using boost::poly_collection::fixed_variant_impl::holds_alternative;
using boost::poly_collection::fixed_variant_impl::get;
using boost::poly_collection::fixed_variant_impl::unsafe_get;
using boost::poly_collection::fixed_variant_impl::get_if;

} /* namespace poly_collection */

} /* namespace boost */

#endif

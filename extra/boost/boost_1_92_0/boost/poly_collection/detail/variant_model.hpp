/* Copyright 2024 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * See http://www.boost.org/libs/poly_collection for library home page.
 */

#ifndef BOOST_POLY_COLLECTION_DETAIL_VARIANT_MODEL_HPP
#define BOOST_POLY_COLLECTION_DETAIL_VARIANT_MODEL_HPP

#if defined(_MSC_VER)
#pragma once
#endif

#if defined(_MSC_VER)
#pragma once
#endif

#include <boost/config.hpp>
#include <boost/core/addressof.hpp>
#include <boost/mp11/algorithm.hpp>
#include <boost/mp11/list.hpp>
#include <boost/mp11/set.hpp>
#include <boost/poly_collection/detail/is_acceptable.hpp>
#include <boost/poly_collection/detail/fixed_variant.hpp>
#include <boost/poly_collection/detail/fixed_variant_iterator.hpp>
#include <boost/poly_collection/detail/packed_segment.hpp>
#include <boost/poly_collection/detail/segment_backend.hpp>
#include <memory>
#include <type_traits>
#include <typeinfo>
#include <utility>

#if !defined(BOOST_NO_CXX17_HDR_VARIANT)
#include <variant>
#endif

namespace boost{namespace variant2{

template<class... T> class variant;

}} /* namespace boost::variant2 */

namespace boost_poly_collection_invoke_visit_variant2{

/* defined here to avoid ADL ambiguities with visit */

template<typename F,typename... Ts>
auto invoke_visit(F&& f,const boost::variant2::variant<Ts...>&x)->
  decltype(visit(std::forward<F>(f),x))
{
  return visit(std::forward<F>(f),x);
}

} /* namespace boost_poly_collection_invoke_visit_variant2 */

namespace boost{

namespace poly_collection{

namespace detail{

/* model for variant_collection */

template<typename... Ts>
struct variant_model;

template<typename V,typename...Ts>
struct variant_model_is_subvariant:std::false_type{};

template<
  typename TL1,typename TL2,
  typename TS1=mp11::mp_set_union<mp11::mp_list<>,TL1>,
  typename TS2=mp11::mp_set_union<mp11::mp_list<>,TL2>
>
struct variant_model_is_subset:std::integral_constant<
  bool,
  mp11::mp_size<mp11::mp_set_union<TS1,TS2>>::value==
  mp11::mp_size<TS2>::value
>{};

template<typename... Us,typename... Ts>
struct variant_model_is_subvariant<
  fixed_variant_impl::fixed_variant<Us...>,
  Ts...
>:variant_model_is_subset<mp11::mp_list<Us...>,mp11::mp_list<Ts...>>{};

template<typename... Us,typename... Ts>
struct variant_model_is_subvariant<
  variant2::variant<Us...>,
  Ts...
>:variant_model_is_subset<mp11::mp_list<Us...>,mp11::mp_list<Ts...>>{};

#if !defined(BOOST_NO_CXX17_HDR_VARIANT)
template<typename... Us,typename... Ts>
struct variant_model_is_subvariant<
  std::variant<Us...>,
  Ts...
>:variant_model_is_subset<mp11::mp_list<Us...>,mp11::mp_list<Ts...>>{};
#endif

template<typename F,typename... Ts>
auto invoke_visit(F&& f,const fixed_variant_impl::fixed_variant<Ts...>&x)->
  decltype(boost::poly_collection::visit(std::forward<F>(f),x))
{
  return boost::poly_collection::visit(std::forward<F>(f),x);
}

using boost_poly_collection_invoke_visit_variant2::invoke_visit;

#if !defined(BOOST_NO_CXX17_HDR_VARIANT)
template<typename F,typename... Ts>
auto invoke_visit(F&& f,const std::variant<Ts...>&x)->
  decltype(std::visit(std::forward<F>(f),x))
{
  return std::visit(std::forward<F>(f),x);
}
#endif

template<typename... Ts>
struct variant_model
{
  using value_type=fixed_variant_impl::fixed_variant<Ts...>;
  using type_index=std::size_t;
  using acceptable_type_list=mp11::mp_list<Ts...>;
  template<typename T>
  struct is_terminal: /* using makes VS2015 choke, hence we derive */
    mp11::mp_contains<acceptable_type_list,T>{};
  template<typename T>
  struct is_implementation:std::integral_constant< /* idem */
    bool,
    is_terminal<T>::value||
    variant_model_is_subvariant<T,Ts...>::value
  >{};

private:
  template<typename T>
  using enable_if_terminal=
    typename std::enable_if<is_terminal<T>::value>::type*;
  template<typename T>
  using enable_if_not_terminal=
    typename std::enable_if<!is_terminal<T>::value>::type*;

public:
  template<typename T> static type_index index()
  {
    return mp11::mp_find<acceptable_type_list,T>::value;
  }

  template<typename T,enable_if_terminal<T> =nullptr>
  static type_index subindex(const T&){return index<T>();}

private:
  template<typename... Qs>
  struct subindex_lambda
  {
    template<typename I>
    std::size_t operator()(I)const
    {
      return mp11::mp_find<
        acceptable_type_list,mp11::mp_at<mp11::mp_list<Qs...>,I>>::value;
    }
  };

public:
  template<
    template<typename...> class V,typename... Qs,
    enable_if_not_terminal<V<Qs...>> =nullptr>
  static type_index subindex(const V<Qs...>& x)
  {
    static constexpr auto not_found=mp11::mp_size<acceptable_type_list>::value;
    auto i=x.index();
    if(i>=sizeof...(Qs))return not_found;
    else return mp11::mp_with_index<sizeof...(Qs)>(
      i,subindex_lambda<Qs...>{});
  }

  template<typename T,enable_if_terminal<T> =nullptr>
  static void* subaddress(T& x){return boost::addressof(x);}

  template<typename T,enable_if_terminal<T> =nullptr>
  static const void* subaddress(const T& x){return boost::addressof(x);}

  template<typename T,enable_if_not_terminal<T> =nullptr>
  static void* subaddress(T& x)
  {
    return const_cast<void*>(subaddress(const_cast<const T&>(x)));
  }
  
private:
  struct subaddress_visitor
  {
    template<typename T>
    const void* operator()(const T& x)const
    {
      return static_cast<const void*>(boost::addressof(x));
    }
  };

public:
  template<typename T,enable_if_not_terminal<T> =nullptr>
  static const void* subaddress(const T& x)
  {
    return invoke_visit(subaddress_visitor{},x);
  }

  template<typename T,enable_if_terminal<T> =nullptr>
  static const std::type_info& subtype_info(const T& x)
  {
    return typeid(x);
  }

private:
  struct subtype_info_visitor
  {
    template<typename T>
    const std::type_info& operator()(const T&)const{return typeid(T);}
  };

public:
  template<typename T,enable_if_not_terminal<T> =nullptr>
  static const std::type_info& subtype_info(const T& x)
  {
    return invoke_visit(subtype_info_visitor{},x);
  }

  using base_iterator=fixed_variant_iterator<value_type>;
  using const_base_iterator=fixed_variant_iterator<const value_type>;
  using base_sentinel=value_type*;
  using const_base_sentinel=const value_type*;
  template<typename T>
  using iterator=fixed_variant_alternative_iterator<value_type,T>;
  template<typename T>
  using const_iterator=fixed_variant_alternative_iterator<value_type,const T>;
  template<typename Allocator>
  using segment_backend=detail::segment_backend<variant_model,Allocator>;
  template<typename T,typename Allocator>
  using segment_backend_implementation=
    packed_segment<variant_model,T,Allocator>;

  static base_iterator nonconst_iterator(const_base_iterator it)
  {
    return base_iterator{
      const_cast<value_type*>(static_cast<const value_type*>(it)),it.stride()};
  }

  template<typename T>
  static iterator<T> nonconst_iterator(const_iterator<T> it)
  {
    return {const_cast<T*>(static_cast<const T*>(it))};
  }

private:
  template<typename,typename,typename>
  friend class packed_segment;

  template<typename T>
  using final_type=fixed_variant_impl::fixed_variant_closure<
     T,fixed_variant_impl::fixed_variant<Ts...>>;

  template<typename T>
  static const value_type* value_ptr(const T* p)noexcept
  {
    return reinterpret_cast<const final_type<T>*>(p);
  }
};

} /* namespace poly_collection::detail */

} /* namespace poly_collection */

} /* namespace boost */

#endif

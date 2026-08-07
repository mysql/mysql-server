/* Copyright 2022-2023 Christian Mazakas.
 * Copyright 2024 Braden Ganetsky.
 * Copyright 2025 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * See https://www.boost.org/libs/bloom for library home page.
 */

#ifndef BOOST_BLOOM_DETAIL_TYPE_TRAITS_HPP
#define BOOST_BLOOM_DETAIL_TYPE_TRAITS_HPP

#include <boost/config.hpp>
#include <boost/type_traits/make_void.hpp>
#include <cstddef>
#include <iterator>
#include <type_traits>
#include <utility>

namespace boost{
namespace bloom{
namespace detail{
namespace is_nothrow_swappable_helper_detail{

using std::swap;

template<typename T,typename=void>
struct is_nothrow_swappable_helper
{
  constexpr static bool value=false;
};

template <typename T>
struct is_nothrow_swappable_helper<
  T,
  boost::void_t<decltype(swap(std::declval<T&>(),std::declval<T&>()))>
>
{
  constexpr static bool value=
    noexcept(swap(std::declval<T&>(),std::declval<T&>()));
};

} /* namespace is_nothrow_swappable_helper_detail */

template <class T>
struct is_nothrow_swappable:std::integral_constant<
  bool,
  is_nothrow_swappable_helper_detail::is_nothrow_swappable_helper<T>::value
>{};

#define BOOST_BLOOM_STATIC_ASSERT_IS_NOTHROW_SWAPPABLE(T) \
static_assert(                                            \
  boost::bloom::detail::is_nothrow_swappable< T >::value, \
  #T " must be nothrow swappable")

template<typename T>
struct is_cv_unqualified_object:std::integral_constant<
  bool,
  !std::is_const<T>::value&&
  !std::is_volatile<T>::value&&
  !std::is_function<T>::value&&
  !std::is_reference<T>::value&&
  !std::is_void<T>::value
>{};

#define BOOST_BLOOM_STATIC_ASSERT_IS_CV_UNQUALIFIED_OBJECT(T) \
static_assert(                                                \
  boost::bloom::detail::is_cv_unqualified_object< T >::value, \
  #T " must be a cv-unqualified object type")

template<typename T>
struct remove_cvref
{
  using type=
    typename std::remove_cv<typename std::remove_reference<T>::type>::type;
};

template<typename T>
using remove_cvref_t=typename remove_cvref<T>::type;

template<typename T,typename=void>
struct is_transparent:std::false_type{};

template<typename T>
struct is_transparent<T,void_t<typename T::is_transparent>>:std::true_type{};

template<typename T,class Q=void>
using enable_if_transparent_t=
  typename std::enable_if<is_transparent<T>::value,Q>::type;

template<typename T>
struct is_integral_or_extended_integral:std::is_integral<T>{};
template<typename T>
struct is_unsigned_or_extended_unsigned:std::is_unsigned<T>{};

#if defined(__SIZEOF_INT128__)

#if defined(BOOST_GCC)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

template<>
struct is_integral_or_extended_integral<__int128>:std::true_type{};
template<>
struct is_integral_or_extended_integral<unsigned __int128>:std::true_type{};
template<>
struct is_unsigned_or_extended_unsigned<unsigned __int128>:std::true_type{};

#if defined(BOOST_GCC)
#pragma GCC diagnostic pop
#endif

#endif

template<typename T>
struct is_unsigned_integral_or_extended_unsigned_integral:
  std::integral_constant<
    bool,
    is_integral_or_extended_integral<T>::value&&
    is_unsigned_or_extended_unsigned<T>::value
  >
{};

template<typename T,template <typename...> class Trait>
struct is_array_of:std::false_type{};

template<typename T,std::size_t N,template <typename...> class Trait>
struct is_array_of<T[N],Trait>:Trait<T>{};

template<typename T> struct array_size:
  std::integral_constant<std::size_t,0>{};
template<typename T,std::size_t N> struct array_size<T[N]>:
  std::integral_constant<std::size_t,N>{};

template<std::size_t N>
struct is_power_of_two:std::integral_constant<bool,(N!=0)&&((N&(N-1))==0)>{};

#if defined(BOOST_NO_CXX20_HDR_CONCEPTS)
template<typename Iterator>
using is_forward_iterator=std::is_base_of<
  std::forward_iterator_tag,
  typename std::iterator_traits<Iterator>::iterator_category
>;
#else
template<typename Iterator>
using is_forward_iterator=std::integral_constant<
  bool,
  std::forward_iterator<Iterator>
>;
#endif

#define BOOST_BLOOM_STATIC_ASSERT_IS_FORWARD_ITERATOR(Iterator) \
static_assert(                                                  \
  boost::bloom::detail::is_forward_iterator< Iterator >::value, \
  #Iterator " must be a forward iterator")

} /* namespace detail */
} /* namespace bloom */
} /* namespace boost */

#endif

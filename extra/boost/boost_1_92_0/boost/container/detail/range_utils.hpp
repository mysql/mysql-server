//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2024-2024. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////

#ifndef BOOST_CONTAINER_DETAIL_RANGE_UTILS_HPP
#define BOOST_CONTAINER_DETAIL_RANGE_UTILS_HPP

#ifndef BOOST_CONFIG_HPP
#  include <boost/config.hpp>
#endif

#if defined(BOOST_HAS_PRAGMA_ONCE)
#  pragma once
#endif

#include <boost/container/detail/workaround.hpp>
#include <boost/container/detail/std_fwd.hpp>   //forward declares ::std::from_range_t

//! \file
//!   Defines boost::container::from_range_t and the boost::container::from_range
//!   tag, used to disambiguate the range-based members of the containers.
//!
//!   When the standard library provides ::std::from_range_t (see
//!   BOOST_CONTAINER_DETAIL_STD_FROM_RANGE_SUPPORT in std_fwd.hpp), from_range_t
//!   is a reference to it -following the same technique as
//!   boost::container::piecewise_construct_t and boost::container::allocator_arg_t-
//!   so the tag is the very same type as the standard one and interoperates with
//!   ::std::from_range, yet the heavyweight <ranges> header is not required.
//!   Otherwise, there is no standard type to refer to and from_range_t is defined
//!   as a standalone type.

namespace boost {
namespace container {

#ifndef BOOST_CONTAINER_DOXYGEN_INVOKED

#if defined(BOOST_CONTAINER_DETAIL_STD_FROM_RANGE_SUPPORT)

   template <int Dummy = 0>
   struct std_from_range_holder
   {
      static ::std::from_range_t *dummy;
   };

   template <int Dummy>                                            //Silence null-reference compiler warnings
   ::std::from_range_t *std_from_range_holder<Dummy>::dummy = reinterpret_cast< ::std::from_range_t * >(0x1234);

typedef const ::std::from_range_t & from_range_t;

//! A instance of type from_range_t
static from_range_t from_range = *std_from_range_holder<>::dummy;

#else //!defined(BOOST_CONTAINER_DETAIL_STD_FROM_RANGE_SUPPORT)

//The standard library does not provide ::std::from_range_t, so there is nothing
//to interoperate with: from_range_t is a standalone type.
struct from_range_t {};

//! A instance of type from_range_t
static const from_range_t from_range = from_range_t();

#endif   //defined(BOOST_CONTAINER_DETAIL_STD_FROM_RANGE_SUPPORT)

#else    //BOOST_CONTAINER_DOXYGEN_INVOKED

//! The from_range_t struct is an empty structure type used as a unique type to
//! disambiguate the constructors and member functions that build or replace the
//! contents of a container from a container-compatible range.
typedef unspecified from_range_t;

//! A instance of type from_range_t
static from_range_t from_range;

#endif   //#ifndef BOOST_CONTAINER_DOXYGEN_INVOKED

///@cond

namespace dtl {

//Range begin()/end() access that depends on neither <ranges> nor <iterator>.
namespace adl_range {

//When decltype/trailing-return-types are available (C++11), adl_range below also
//resolves ranges whose begin()/end() are free functions found only through ADL,
//matching std::ranges::begin/std::ranges::end reachability. This needs neither
//<ranges> nor <iterator> nor a (UB and fragile) forward declaration of
//std::begin/std::end: member begin()/end() is detected directly and the ADL
//fallback already reaches std::begin/std::end for types in namespace std (e.g.
//std::valarray). In C++03 adl_range supports member begin()/end() and C arrays.
#if !defined(BOOST_NO_CXX11_DECLTYPE) && !defined(BOOST_NO_CXX11_TRAILING_RESULT_TYPES)
#  define BOOST_CONTAINER_RANGE_UTILS_ADL_FREE_FUNCTIONS
#endif

#if defined(BOOST_CONTAINER_RANGE_UTILS_ADL_FREE_FUNCTIONS)

//Member begin()/end() is preferred; when there is no member, an unqualified
//begin()/end() call is used, so free begin()/end() reachable through ADL are
//picked up (this also reaches std::begin/std::end for types in namespace std,
//such as std::valarray). C arrays have a dedicated overload. This mirrors
//std::ranges::begin/std::ranges::end reachability, without including <iterator>
//or forward declaring std::begin/std::end.
//
//Priority tags (int beats long in overload resolution) ensure the member form
//wins when present and the ADL free-function form is only the fallback. The C&
//parameter deduces const/non-const, selecting the right iterator type.
template<class C>
inline auto adl_begin_impl(C &c, int) -> decltype(c.begin())
{  return c.begin();  }

template<class C>
inline auto adl_begin_impl(C &c, long) -> decltype(begin(c))
{  return begin(c);  }

template<class C>
inline auto adl_end_impl(C &c, int) -> decltype(c.end())
{  return c.end();  }

template<class C>
inline auto adl_end_impl(C &c, long) -> decltype(end(c))
{  return end(c);  }

template<class C>
inline auto adl_begin(C &c) -> decltype(adl_begin_impl(c, 0))
{  return adl_begin_impl(c, 0);  }

template<class C>
inline auto adl_end(C &c) -> decltype(adl_end_impl(c, 0))
{  return adl_end_impl(c, 0);  }

//C arrays (more specialized than the generic adl_begin/adl_end templates above).
template<class T, std::size_t N>
inline T* adl_begin(T (&array)[N])
{  return array;  }

template<class T, std::size_t N>
inline T* adl_end(T (&array)[N])
{  return array + N;  }

#else //C++03: decltype/trailing return types are unavailable.

//Member begin()/end() (every standard and Boost container, std::initializer_list,
//std::span, std::string_view, ...) plus a dedicated overload for C arrays.
//Free begin()/end() found only through ADL are not supported in this mode.
//Written in a C++03-compatible way (no decltype/auto/trailing return type); the
//const-reference overload is picked for const ranges via partial ordering.
template<class C>
inline typename C::iterator adl_begin(C &c)
{  return c.begin();  }

template<class C>
inline typename C::const_iterator adl_begin(const C &c)
{  return c.begin();  }

template<class C>
inline typename C::iterator adl_end(C &c)
{  return c.end();  }

template<class C>
inline typename C::const_iterator adl_end(const C &c)
{  return c.end();  }

template<class T, std::size_t N>
inline T* adl_begin(T (&array)[N])
{  return array;  }

template<class T, std::size_t N>
inline T* adl_end(T (&array)[N])
{  return array + N;  }

#endif   //BOOST_CONTAINER_RANGE_UTILS_ADL_FREE_FUNCTIONS

#undef BOOST_CONTAINER_RANGE_UTILS_ADL_FREE_FUNCTIONS

}  //namespace adl_range

struct from_range_construct_use
{
   //Avoid warnings of unused "from_range"
   from_range_construct_use()
   {  (void)&::boost::container::from_range;   }
};

}  //namespace dtl {

///@endcond

}  //namespace container {
}  //namespace boost {

#endif   //BOOST_CONTAINER_DETAIL_RANGE_UTILS_HPP

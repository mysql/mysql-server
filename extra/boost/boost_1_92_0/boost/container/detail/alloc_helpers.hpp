//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2014-2015. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////
#ifndef BOOST_CONTAINER_DETAIL_ALLOC_TRAITS_HPP
#define BOOST_CONTAINER_DETAIL_ALLOC_TRAITS_HPP

#ifndef BOOST_CONFIG_HPP
#  include <boost/config.hpp>
#endif

#if defined(BOOST_HAS_PRAGMA_ONCE)
#  pragma once
#endif

// move
#include <boost/move/adl_move_swap.hpp>
#include <boost/move/utility_core.hpp>
#include <boost/container/detail/mpl.hpp>
#include <boost/assert.hpp>

namespace boost {
namespace container {
namespace dtl {

template<class AllocatorType>
inline void swap_alloc(AllocatorType &, AllocatorType &, dtl::false_type)
   BOOST_NOEXCEPT_OR_NOTHROW
{}

template<class AllocatorType>
inline void swap_alloc(AllocatorType &l, AllocatorType &r, dtl::true_type)
{  boost::adl_move_swap(l, r);   }

template<class AllocatorType>
inline void assign_alloc(AllocatorType &, const AllocatorType &, dtl::false_type)
   BOOST_NOEXCEPT_OR_NOTHROW
{}

template<class AllocatorType>
inline void assign_alloc(AllocatorType &l, const AllocatorType &r, dtl::true_type)
{  l = r;   }

template<class AllocatorType>
inline void move_alloc(AllocatorType &, AllocatorType &, dtl::false_type)
   BOOST_NOEXCEPT_OR_NOTHROW
{}

template<class AllocatorType>
inline void move_alloc(AllocatorType &l, AllocatorType &r, dtl::true_type)
{  l = ::boost::move(r);   }

template<class SizeType, class LimitSizeType, bool = sizeof(SizeType)<= sizeof(LimitSizeType)>
struct limit_by_stored_size_type
{
   static BOOST_CONTAINER_FORCEINLINE SizeType clamp(SizeType val)
   {  return val; }

   static BOOST_CONTAINER_FORCEINLINE void set(LimitSizeType &val, SizeType v)
   {  val = v; }

   template <class F>
   BOOST_CONTAINER_FORCEINLINE static void call_if_overflows(SizeType, F)
   {}
};

template<class SizeType, class LimitSizeType>
struct limit_by_stored_size_type<SizeType, LimitSizeType, false>
{
   static BOOST_CONTAINER_FORCEINLINE SizeType clamp(SizeType val)
   {  return val <= LimitSizeType(-1) ? val : LimitSizeType(-1);  }

   static void set(LimitSizeType &dst, SizeType val)
   {
      BOOST_ASSERT(LimitSizeType(-1) >= val);
      dst = static_cast<LimitSizeType>(val);
   }

   template <class F>
   BOOST_CONTAINER_FORCEINLINE static void call_if_overflows(SizeType v, F f)
   {
      if(LimitSizeType(-1) < v)
         f();
   }
};

}  //namespace dtl {
}  //namespace container {
}  //namespace boost {

#endif   //#ifndef BOOST_CONTAINER_DETAIL_ALLOC_TRAITS_HPP

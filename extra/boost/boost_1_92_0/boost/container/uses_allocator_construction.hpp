//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2025-2025. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////

#ifndef BOOST_CONTAINER_USES_ALLOCATOR_CONSTRUCTION_HPP
#define BOOST_CONTAINER_USES_ALLOCATOR_CONSTRUCTION_HPP

#ifndef BOOST_CONFIG_HPP
#  include <boost/config.hpp>
#endif

#if defined(BOOST_HAS_PRAGMA_ONCE)
#  pragma once
#endif

#include <boost/container/detail/config_begin.hpp>
#include <boost/container/detail/workaround.hpp>
#include <boost/container/detail/dispatch_uses_allocator.hpp>
#include <boost/move/utility_core.hpp>
#if defined(BOOST_NO_CXX11_VARIADIC_TEMPLATES)
#include <boost/move/detail/fwd_macros.hpp>
#endif

namespace boost {
namespace container {

#if defined(BOOST_CONTAINER_DOXYGEN_INVOKED) || !defined(BOOST_NO_CXX11_VARIADIC_TEMPLATES)

//! <b>Effects</b>: Creates an object of the given type T by means of uses-allocator
//!   construction (see `uses_allocator`) at the uninitialized memory, where:
//!   
//!   * `p` is the memory location where the object will be placed
//!   * `alloc_arg` is the allocator argument whose type AllocArg will be used to evaluate uses_allocator<T, AllocArg>::value
//!   * `args` are the arguments to pass to T's constructor.
//!
//! <b>Returns</b>: Pointer to the newly-created object of type T
//!
//! <b>Throws</b>: Any exception thrown by the constructor of T.
template< class T, class AllocArg, class... Args >
T* uninitialized_construct_using_allocator(T* p, BOOST_FWD_REF(AllocArg) alloc_arg, BOOST_FWD_REF(Args)... args)
{
   boost::container::dtl::allocator_traits_dummy<T> atd;
   boost::container::dtl::dispatch_uses_allocator
      (atd, boost::forward<AllocArg>(alloc_arg), p, boost::forward<Args>(args)...);
   return p;
}

//! <b>Effects</b>: eates an object of the given type T by means of uses-allocator construction
//!   (see `uses_allocator`), where:
//!   
//!   * `alloc_arg` is the allocator argument whose type AllocArg will be used to evaluate uses_allocator<T, AllocArg>::value
//!   * `args` are the arguments to pass to T's constructor.
//!
//! <b>Returns</b>: The newsly created object of type T
//!
//! <b>Throws</b>: Any exception thrown by the constructor of T.
template< class T, class AllocArg, class... Args >
T make_obj_using_allocator(BOOST_FWD_REF(AllocArg) alloc_arg, BOOST_FWD_REF(Args)... args)
{
   return boost::container::dtl::construct_dispatch_uses_allocator<T>
      (boost::forward<AllocArg>(alloc_arg), boost::forward<Args>(args)...);
}

#else //BOOST_NO_CXX11_VARIADIC_TEMPLATES

#define BOOST_CONTAINER_USES_ALLOCATOR_CONSTRUCTION_CODE(N) \
   template < typename T, typename AllocArg BOOST_MOVE_I##N BOOST_MOVE_CLASS##N >\
   inline T* uninitialized_construct_using_allocator\
      (T* p, BOOST_FWD_REF(AllocArg) alloc_arg BOOST_MOVE_I##N BOOST_MOVE_UREF##N)\
   {\
      boost::container::dtl::allocator_traits_dummy<T> atd;\
      boost::container::dtl::dispatch_uses_allocator\
         (atd, boost::forward<AllocArg>(alloc_arg), p BOOST_MOVE_I##N BOOST_MOVE_FWD##N);\
      return p;\
   }\
   \
   template <class T, class AllocArg BOOST_MOVE_I##N BOOST_MOVE_CLASS##N >\
   inline T make_obj_using_allocator\
      (BOOST_FWD_REF(AllocArg) alloc_arg BOOST_MOVE_I##N BOOST_MOVE_UREF##N)\
   {\
      return boost::container::dtl::construct_dispatch_uses_allocator<T>\
         (boost::forward<AllocArg>(alloc_arg) BOOST_MOVE_I##N BOOST_MOVE_FWD##N);\
   }\
//
BOOST_MOVE_ITERATE_0TO9(BOOST_CONTAINER_USES_ALLOCATOR_CONSTRUCTION_CODE)
#undef BOOST_CONTAINER_USES_ALLOCATOR_CONSTRUCTION_CODE

#endif   //BOOST_NO_CXX11_VARIADIC_TEMPLATES

}} //namespace boost::container

#include <boost/container/detail/config_end.hpp>

#endif   //BOOST_CONTAINER_USES_ALLOCATOR_CONSTRUCTION_HPP

//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2025-2025. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////
#ifndef BOOST_CONTAINER_DETAIL_OPERATOR_NEW_HELPERS_HPP
#define BOOST_CONTAINER_DETAIL_OPERATOR_NEW_HELPERS_HPP

#ifndef BOOST_CONFIG_HPP
#  include <boost/config.hpp>
#endif

#if defined(BOOST_HAS_PRAGMA_ONCE)
#  pragma once
#endif

#include <boost/container/detail/std_fwd.hpp>
#include <boost/container/throw_exception.hpp>
#include <boost/container/detail/type_traits.hpp>

#if !defined(__cpp_aligned_new)
#include <boost/container/detail/aligned_allocation.hpp>
#endif

namespace boost {
namespace container {
namespace dtl {

//For GCC and clang there are several cases where __STDCPP_DEFAULT_NEW_ALIGNMENT__
//is not properly synchronized with the default alignment of malloc. Examples:
//
// - On Unix platforms, a programmer uses jemalloc, mimalloc that have historically a lower
//    default alignment (to waste less memory)
//
// - On Windows platforms, the allocator is provided by MSVCRT o UCRT that uses HeapAlloc
//    (e.g. 8 byte alignment for x86 and 16 bytes for x64)
//
// - On Apple platforms the default malloc implementation has a reduced defaykt alignment
//    even on ARM64 platforms.
BOOST_CONTAINER_FORCEINLINE bool operator_new_raw_overaligned(std::size_t alignment)
{
   //In MacOs, the default allocator can return data aligned to 8 bytes
   #if defined(__APPLE__)
   return alignment > 8u;
   //GCC-clang on Mingw-w64 has problems with malloc (MSVCRT / UCRT) alignment not matching
   //__STDCPP_DEFAULT_NEW_ALIGNMENT__, since HeapAlloc alignment is 8 for 32 bit targets 
   #elif !defined(__cpp_aligned_new) || (defined(_WIN32) && !defined(_WIN64) && !defined(_MSC_VER))
   return alignment > 2*sizeof(void*);
   #else
   return alignment > __STDCPP_DEFAULT_NEW_ALIGNMENT__;
   #endif
}

BOOST_CONTAINER_FORCEINLINE void* operator_new_raw_allocate(const std::size_t size, const std::size_t alignment)
{
   (void)alignment;
   if(operator_new_raw_overaligned(alignment)) {
      #if defined(__cpp_aligned_new)
      return ::operator new(size, std::align_val_t(alignment));
      #else
      //C++ requires zero-sized allocations to return a non-null pointer
      return aligned_allocate(alignment, !size ? 1 : size);
      #endif
   }
   else{
      return ::operator new(size);
   }
}

BOOST_CONTAINER_FORCEINLINE void operator_delete_raw_deallocate
   (void* const ptr, const std::size_t size, const std::size_t alignment) BOOST_NOEXCEPT_OR_NOTHROW
{
   (void)size;
   (void)alignment;
   if(operator_new_raw_overaligned(alignment)) {
      #if defined(__cpp_aligned_new)
         # if defined(__cpp_sized_deallocation)
         ::operator delete(ptr, size, std::align_val_t(alignment));
         #else
         ::operator delete(ptr, std::align_val_t(alignment));
         # endif
      #else
         aligned_deallocate(ptr);
      #endif
   }
   else {
      # if defined(__cpp_sized_deallocation)
      ::operator delete(ptr, size);
      #else
      ::operator delete(ptr);
      # endif
   }
}

template <class T>
BOOST_CONTAINER_FORCEINLINE T* operator_new_allocate(std::size_t count)
{
   const std::size_t max_count = std::size_t(-1)/(2*sizeof(T));
   if(BOOST_UNLIKELY(count > max_count))
      throw_bad_alloc();
   return static_cast<T*>(operator_new_raw_allocate(count*sizeof(T), alignment_of<T>::value));
}

template <class T>
BOOST_CONTAINER_FORCEINLINE void operator_delete_deallocate(T* ptr, std::size_t n) BOOST_NOEXCEPT_OR_NOTHROW
{
   operator_delete_raw_deallocate((void*)ptr, n * sizeof(T), alignment_of<T>::value);
}


}  //namespace dtl {
}  //namespace container {
}  //namespace boost {

#endif   //#ifndef BOOST_CONTAINER_DETAIL_OPERATOR_NEW_HELPERS_HPP

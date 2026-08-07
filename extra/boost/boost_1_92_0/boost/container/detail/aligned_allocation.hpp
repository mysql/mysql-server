//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2025-2025. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////
#ifndef BOOST_CONTAINER_DETAIL_ALIGNED_ALLOCATION_HPP
#define BOOST_CONTAINER_DETAIL_ALIGNED_ALLOCATION_HPP

#ifndef BOOST_CONFIG_HPP
#  include <boost/config.hpp>
#endif

#if defined(BOOST_HAS_PRAGMA_ONCE)
#  pragma once
#endif

// Platform detection
#if defined(_WIN32) && !defined(__CYGWIN__)
   #define BOOST_CONTAINER_HAS_ALIGNED_MALLOC
#elif BOOST_CXX_VERSION >= 201703L
   #define BOOST_CONTAINER_HAS_ALIGNED_ALLOC
#elif defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L)
   //Note: in most C++ compilers __STDC_VERSION__ is not defined, but just in case
   #define BOOST_CONTAINER_HAS_ALIGNED_ALLOC
#else
   #include <unistd.h>  //Include it to detect POSIX features
   #if defined(_POSIX_C_SOURCE) && (_POSIX_C_SOURCE >= 200112L)
      #define BOOST_CONTAINER_HAS_POSIX_MEMALIGN
   #elif defined(__APPLE__)
      //All recent Apple OSes (macOS 10.6+, iOS 3.0+, tvOS 9.0+, watchOS 2.0+) support posix_memalign
      #define BOOST_CONTAINER_HAS_POSIX_MEMALIGN
   #elif defined(__ANDROID__)
      #if (__ANDROID_API__ >= 28)
         #define BOOST_CONTAINER_HAS_ALIGNED_ALLOC
      #else
         #define BOOST_CONTAINER_HAS_POSIX_MEMALIGN
      #endif
   #endif
#endif

// Include
#if defined(BOOST_CONTAINER_HAS_ALIGNED_MALLOC)
   #include <malloc.h>
#elif defined(BOOST_CONTAINER_HAS_POSIX_MEMALIGN)
   #include <stdlib.h>
#elif defined(BOOST_CONTAINER_HAS_ALIGNED_ALLOC)
   #include <stdlib.h>
#else
   #include <stdlib.h> //for malloc
#endif

namespace boost {
namespace container {
namespace dtl {

#if defined(BOOST_CONTAINER_HAS_POSIX_MEMALIGN)

inline void* aligned_allocate(std::size_t al, std::size_t sz)
{
   void *ptr;
   // posix_memalign requires aligned multiple of void*
   if (al < sizeof(void*))
      al = sizeof(void*);
   int ret = posix_memalign(&ptr, al, sz);
   if (ret != 0)
      return 0;
   return ptr;
}

#elif defined(BOOST_CONTAINER_HAS_ALIGNED_ALLOC)

inline void* aligned_allocate(std::size_t al, std::size_t sz)
{
   // Some aligned_allocate are based on posix_memalign so require also minimal alignment
   if (al < sizeof(void*))
      al = sizeof(void*);

   // aligned_allocate requires size to be a multiple of alignment
   std::size_t rounded_size = std::size_t(sz + al - 1u) & ~std::size_t(al - 1);

   //Check for rounded size overflow
   return rounded_size ? ::aligned_alloc(al, rounded_size) : 0;
}

#elif defined(BOOST_CONTAINER_HAS_ALIGNED_MALLOC)

inline void* aligned_allocate(std::size_t al, std::size_t sz)
{
   return _aligned_malloc(sz, al);
}

#else

inline void* aligned_allocate(std::size_t al, std::size_t sz)
{
   //Make room for a back pointer metadata
   void* const mptr = malloc(sz + sizeof(void*) + al);
   if (!mptr)
      return 0;

   //Now align the returned pointer (which will be aligned at least to sizeof(void*)
   const std::size_t raw_addr = reinterpret_cast<std::size_t>(mptr);
   const std::size_t offset = sizeof(void*);
   void *const ptr = reinterpret_cast<void*>((raw_addr + offset + al - 1u) & ~(al - 1u));

   // Store the original pointer just before the aligned address
   void** const backpointer = reinterpret_cast<void**>(ptr) - 1;
   *backpointer = mptr;
   return ptr;
}

#endif

#if defined(BOOST_CONTAINER_HAS_ALIGNED_ALLOC) || defined(BOOST_CONTAINER_HAS_POSIX_MEMALIGN)

inline void aligned_deallocate(void* ptr)
{
   if (!ptr)
      return;
   free(ptr);
}

#elif defined(BOOST_CONTAINER_HAS_ALIGNED_MALLOC)

inline void aligned_deallocate(void* ptr)
{
   _aligned_free(ptr);  //_aligned_free supports NULL ptr
}

#else

inline void aligned_deallocate(void* ptr)
{
    // Obtain backpointer data and free it
    void** storage = reinterpret_cast<void**>(ptr) - 1;
    free(*storage);
}

#endif//

}  //namespace dtl {
}  //namespace container {
}  //namespace boost {

#endif   //#ifndef BOOST_CONTAINER_DETAIL_ALIGNED_ALLOCATION_HPP

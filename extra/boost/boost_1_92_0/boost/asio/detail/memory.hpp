//
// detail/memory.hpp
// ~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2026 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_ASIO_DETAIL_MEMORY_HPP
#define BOOST_ASIO_DETAIL_MEMORY_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include <boost/asio/detail/config.hpp>
#include <cstddef>
#include <cstdlib>
#include <memory>
#include <new>
#include <boost/asio/detail/cstdint.hpp>
#include <boost/asio/detail/throw_exception.hpp>

#if !defined(BOOST_ASIO_HAS_STD_ALIGNED_ALLOC) \
  && defined(BOOST_ASIO_HAS_BOOST_ALIGN)
# include <boost/align/aligned_alloc.hpp>
#endif // !defined(BOOST_ASIO_HAS_STD_ALIGNED_ALLOC)
       //   && defined(BOOST_ASIO_HAS_BOOST_ALIGN)

namespace boost {
namespace asio {
BOOST_ASIO_INLINE_NAMESPACE_BEGIN
namespace detail {

using std::allocate_shared;
using std::make_shared;
using std::shared_ptr;
using std::weak_ptr;
using std::addressof;

#if defined(BOOST_ASIO_HAS_STD_TO_ADDRESS)
using std::to_address;
#else // defined(BOOST_ASIO_HAS_STD_TO_ADDRESS)
template <typename T>
inline T* to_address(T* p) { return p; }
template <typename T>
inline const T* to_address(const T* p) { return p; }
template <typename T>
inline volatile T* to_address(volatile T* p) { return p; }
template <typename T>
inline const volatile T* to_address(const volatile T* p) { return p; }
#endif // defined(BOOST_ASIO_HAS_STD_TO_ADDRESS)

inline void* align(std::size_t alignment,
    std::size_t size, void*& ptr, std::size_t& space)
{
  return std::align(alignment, size, ptr, space);
}

template <typename T, typename Allocator, typename... Args>
T* allocate_object(const Allocator& a, Args&&... args)
{
  typename std::allocator_traits<Allocator>::template rebind_alloc<T> alloc(a);
  T* raw = std::allocator_traits<decltype(alloc)>::allocate(alloc, 1);
#if !defined(BOOST_ASIO_NO_EXCEPTIONS)
  try
#endif // !defined(BOOST_ASIO_NO_EXCEPTIONS)
  {
    return new (raw) T(static_cast<Args&&>(args)...);
  }
#if !defined(BOOST_ASIO_NO_EXCEPTIONS)
  catch (...)
  {
    std::allocator_traits<decltype(alloc)>::deallocate(alloc, raw, 1);
    throw;
  }
#endif // !defined(BOOST_ASIO_NO_EXCEPTIONS)
}

template <typename Allocator, typename T>
void deallocate_object(const Allocator& a, T* ptr)
{
  typename std::allocator_traits<Allocator>::template rebind_alloc<T> alloc(a);
  std::allocator_traits<decltype(alloc)>::destroy(alloc, ptr);
  std::allocator_traits<decltype(alloc)>::deallocate(alloc, ptr, 1);
}

} // namespace detail

using std::allocator_arg_t;
# define BOOST_ASIO_USES_ALLOCATOR(t) \
  namespace std { \
    template <typename Allocator> \
    struct uses_allocator<t, Allocator> : true_type {}; \
  } \
  /**/
# define BOOST_ASIO_REBIND_ALLOC(alloc, t) \
  typename std::allocator_traits<alloc>::template rebind_alloc<t>
  /**/

inline void* aligned_new(std::size_t align, std::size_t size)
{
#if defined(BOOST_ASIO_HAS_STD_ALIGNED_ALLOC)
  align = (align < BOOST_ASIO_DEFAULT_ALIGN) ? BOOST_ASIO_DEFAULT_ALIGN : align;
  size = (size % align == 0) ? size : size + (align - size % align);
  void* ptr = std::aligned_alloc(align, size);
  if (!ptr)
  {
    std::bad_alloc ex;
    boost::asio::detail::throw_exception(ex);
  }
  return ptr;
#elif defined(BOOST_ASIO_HAS_BOOST_ALIGN)
  align = (align < BOOST_ASIO_DEFAULT_ALIGN) ? BOOST_ASIO_DEFAULT_ALIGN : align;
  size = (size % align == 0) ? size : size + (align - size % align);
  void* ptr = boost::alignment::aligned_alloc(align, size);
  if (!ptr)
  {
    std::bad_alloc ex;
    boost::asio::detail::throw_exception(ex);
  }
  return ptr;
#elif defined(BOOST_ASIO_MSVC)
  align = (align < BOOST_ASIO_DEFAULT_ALIGN) ? BOOST_ASIO_DEFAULT_ALIGN : align;
  size = (size % align == 0) ? size : size + (align - size % align);
  void* ptr = _aligned_malloc(size, align);
  if (!ptr)
  {
    std::bad_alloc ex;
    boost::asio::detail::throw_exception(ex);
  }
  return ptr;
#else // defined(BOOST_ASIO_MSVC)
  (void)align;
  return ::operator new(size);
#endif // defined(BOOST_ASIO_MSVC)
}

inline void aligned_delete(void* ptr)
{
#if defined(BOOST_ASIO_HAS_STD_ALIGNED_ALLOC)
  std::free(ptr);
#elif defined(BOOST_ASIO_HAS_BOOST_ALIGN)
  boost::alignment::aligned_free(ptr);
#elif defined(BOOST_ASIO_MSVC)
  _aligned_free(ptr);
#else // defined(BOOST_ASIO_MSVC)
  ::operator delete(ptr);
#endif // defined(BOOST_ASIO_MSVC)
}

BOOST_ASIO_INLINE_NAMESPACE_END
} // namespace asio
} // namespace boost

#endif // BOOST_ASIO_DETAIL_MEMORY_HPP

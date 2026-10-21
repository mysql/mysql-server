//
// detail/posix_thread.hpp
// ~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2026 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_ASIO_DETAIL_POSIX_THREAD_HPP
#define BOOST_ASIO_DETAIL_POSIX_THREAD_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include <boost/asio/detail/config.hpp>

#if defined(BOOST_ASIO_HAS_PTHREADS)

#include <cstddef>
#include <pthread.h>
#include <boost/asio/detail/memory.hpp>

#include <boost/asio/detail/push_options.hpp>

namespace boost {
namespace asio {
BOOST_ASIO_INLINE_NAMESPACE_BEGIN
namespace detail {

extern "C"
{
  BOOST_ASIO_DECL void* BOOST_ASIO_VERSIONED_NAME(detail_posix_thread_function)(
      void* arg);
}

class posix_thread
{
public:
  // Construct in a non-joinable state.
  posix_thread() noexcept
    : arg_(0)
  {
  }

  // Constructor.
  template <typename Function>
  posix_thread(Function f, unsigned int = 0)
    : posix_thread(std::allocator_arg, std::allocator<void>(), f)
  {
  }

  // Construct with custom allocator.
  template <typename Allocator, typename Function>
  posix_thread(allocator_arg_t, const Allocator& a,
      Function f, unsigned int = 0)
    : arg_(start_thread(allocate_object<func<Function, Allocator>>(a, f, a)))
  {
  }

  // Move constructor.
  posix_thread(posix_thread&& other) noexcept
    : arg_(other.arg_)
  {
    other.arg_ = 0;
  }

  // Destructor.
  BOOST_ASIO_DECL ~posix_thread();

  // Move assignment.
  posix_thread& operator=(posix_thread&& other) noexcept
  {
    arg_ = other.arg_;
    other.arg_ = 0;
    return *this;
  }

  // Whether the thread can be joined.
  bool joinable() const
  {
    return !!arg_;
  }

  // Wait for the thread to exit.
  BOOST_ASIO_DECL void join();

  // Get number of CPUs.
  BOOST_ASIO_DECL static std::size_t hardware_concurrency();

private:
  friend void* BOOST_ASIO_VERSIONED_NAME(detail_posix_thread_function)(
      void* arg);

  class func_base
  {
  public:
    virtual ~func_base() {}
    virtual void run() = 0;
    virtual void destroy() = 0;
    ::pthread_t thread_;
  };

  template <typename Function, typename Allocator>
  class func
    : public func_base
  {
  public:
    func(Function f, const Allocator& a)
      : f_(f),
        allocator_(a)
    {
    }

    virtual void run()
    {
      f_();
    }

    virtual void destroy()
    {
      deallocate_object(allocator_, this);
    }

  private:
    Function f_;
    Allocator allocator_;
  };

  BOOST_ASIO_DECL func_base* start_thread(func_base* arg);

  func_base* arg_;
};

} // namespace detail
BOOST_ASIO_INLINE_NAMESPACE_END
} // namespace asio
} // namespace boost

#include <boost/asio/detail/pop_options.hpp>

#if defined(BOOST_ASIO_HEADER_ONLY)
# include <boost/asio/detail/impl/posix_thread.ipp>
#endif // defined(BOOST_ASIO_HEADER_ONLY)

#endif // defined(BOOST_ASIO_HAS_PTHREADS)

#endif // BOOST_ASIO_DETAIL_POSIX_THREAD_HPP

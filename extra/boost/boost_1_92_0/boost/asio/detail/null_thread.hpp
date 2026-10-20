//
// detail/null_thread.hpp
// ~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2026 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_ASIO_DETAIL_NULL_THREAD_HPP
#define BOOST_ASIO_DETAIL_NULL_THREAD_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include <boost/asio/detail/config.hpp>

#if !defined(BOOST_ASIO_HAS_THREADS)

#include <boost/asio/detail/throw_error.hpp>
#include <boost/asio/error.hpp>

#include <boost/asio/detail/push_options.hpp>

namespace boost {
namespace asio {
BOOST_ASIO_INLINE_NAMESPACE_BEGIN
namespace detail {

class null_thread
{
public:
  // Construct in a non-joinable state.
  null_thread() noexcept
  {
  }

  // Constructor.
  template <typename Function>
  null_thread(Function, unsigned int = 0)
  {
    boost::asio::detail::throw_error(
        boost::asio::error::operation_not_supported, "thread");
  }

  // Construct with custom allocator.
  template <typename Allocator, typename Function>
  null_thread(allocator_arg_t, const Allocator&, Function, unsigned int = 0)
  {
    boost::asio::detail::throw_error(
        boost::asio::error::operation_not_supported, "thread");
  }

  // Move constructor.
  null_thread(null_thread&&) noexcept
  {
  }

  // Destructor.
  ~null_thread()
  {
  }

  // Move assignment.
  null_thread& operator=(null_thread&&) noexcept
  {
    return *this;
  }

  // Whether the thread can be joined.
  bool joinable() const
  {
    return false;
  }

  // Wait for the thread to exit.
  void join()
  {
  }

  // Get number of CPUs.
  static std::size_t hardware_concurrency()
  {
    return 1;
  }
};

} // namespace detail
BOOST_ASIO_INLINE_NAMESPACE_END
} // namespace asio
} // namespace boost

#include <boost/asio/detail/pop_options.hpp>

#endif // !defined(BOOST_ASIO_HAS_THREADS)

#endif // BOOST_ASIO_DETAIL_NULL_THREAD_HPP

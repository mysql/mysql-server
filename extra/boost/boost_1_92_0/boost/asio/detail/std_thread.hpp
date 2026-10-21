//
// detail/std_thread.hpp
// ~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2026 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_ASIO_DETAIL_STD_THREAD_HPP
#define BOOST_ASIO_DETAIL_STD_THREAD_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include <boost/asio/detail/config.hpp>
#include <thread>
#include <boost/asio/detail/memory.hpp>

#include <boost/asio/detail/push_options.hpp>

namespace boost {
namespace asio {
BOOST_ASIO_INLINE_NAMESPACE_BEGIN
namespace detail {

class std_thread
{
public:
  // Construct in a non-joinable state.
  std_thread() noexcept
    : thread_()
  {
  }

  // Constructor.
  template <typename Function>
  std_thread(Function f, unsigned int = 0)
    : thread_(f)
  {
  }

  // Construct with custom allocator.
  template <typename Allocator, typename Function>
  std_thread(allocator_arg_t, const Allocator&, Function f, unsigned int = 0)
    : thread_(f)
  {
  }

  // Move constructor.
  std_thread(std_thread&& other) noexcept
    : thread_(static_cast<std::thread&&>(other.thread_))
  {
  }

  // Destructor.
  ~std_thread()
  {
  }

  // Move assignment.
  std_thread& operator=(std_thread&& other) noexcept
  {
    thread_ = static_cast<std::thread&&>(other.thread_);
    return *this;
  }

  // Whether the thread can be joined.
  bool joinable() const
  {
    return thread_.joinable();
  }

  // Wait for the thread to exit.
  void join()
  {
    if (thread_.joinable())
      thread_.join();
  }

  // Get number of CPUs.
  static std::size_t hardware_concurrency()
  {
    return std::thread::hardware_concurrency();
  }

private:
  std::thread thread_;
};

} // namespace detail
BOOST_ASIO_INLINE_NAMESPACE_END
} // namespace asio
} // namespace boost

#include <boost/asio/detail/pop_options.hpp>

#endif // BOOST_ASIO_DETAIL_STD_THREAD_HPP

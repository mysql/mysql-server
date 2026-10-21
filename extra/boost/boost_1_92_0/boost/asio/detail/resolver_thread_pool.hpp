//
// detail/resolver_thread_pool.hpp
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2026 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_ASIO_DETAIL_RESOLVER_THREAD_POOL_HPP
#define BOOST_ASIO_DETAIL_RESOLVER_THREAD_POOL_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include <boost/asio/detail/config.hpp>
#include <boost/asio/execution_context.hpp>
#include <boost/asio/detail/mutex.hpp>
#include <boost/asio/detail/resolve_op.hpp>
#include <boost/asio/detail/scheduler.hpp>
#include <boost/asio/detail/thread_group.hpp>

#if defined(BOOST_ASIO_HAS_IOCP)
# include <boost/asio/detail/win_iocp_io_context.hpp>
#else // defined(BOOST_ASIO_HAS_IOCP)
# include <boost/asio/detail/scheduler.hpp>
#endif // defined(BOOST_ASIO_HAS_IOCP)

#include <boost/asio/detail/push_options.hpp>

namespace boost {
namespace asio {
BOOST_ASIO_INLINE_NAMESPACE_BEGIN
namespace detail {

class resolver_thread_pool :
  public execution_context_service_base<resolver_thread_pool>
{
public:
#if defined(BOOST_ASIO_HAS_IOCP)
  typedef class win_iocp_io_context scheduler_impl;
#else
  typedef class scheduler scheduler_impl;
#endif

  // Constructor.
  BOOST_ASIO_DECL resolver_thread_pool(execution_context& context);

  // Destructor.
  BOOST_ASIO_DECL ~resolver_thread_pool();

  // Destroy all user-defined handler objects owned by the service.
  BOOST_ASIO_DECL void shutdown();

  // Perform any fork-related housekeeping.
  BOOST_ASIO_DECL void notify_fork(execution_context::fork_event fork_ev);

  // Helper function to start an asynchronous resolve operation.
  BOOST_ASIO_DECL void start_resolve_op(resolve_op* op);

  // Get the underlying scheduler implementation.
  scheduler_impl& scheduler()
  {
    return scheduler_;
  }

private:
  // Helper class to run the work scheduler in a thread.
  class work_scheduler_runner;

  // Start the work scheduler if it's not already running.
  BOOST_ASIO_DECL void start_work_threads();

  // The scheduler implementation used to post completions.
  scheduler_impl& scheduler_;

  // Mutex to protect access to internal data.
  boost::asio::detail::mutex mutex_;

  // Private scheduler used for performing asynchronous host resolution.
  scheduler_impl work_scheduler_;

  // Threads used for running the work scheduler's run loop.
  thread_group<execution_context::allocator<void>> work_threads_;

  // The number of threads used to run the work scheduler.
  unsigned int num_work_threads_;

  // Whether the scheduler locking is enabled.
  bool scheduler_locking_;

  // Whether the scheduler has been shut down.
  bool shutdown_;
};

} // namespace detail
BOOST_ASIO_INLINE_NAMESPACE_END
} // namespace asio
} // namespace boost

#include <boost/asio/detail/pop_options.hpp>

#if defined(BOOST_ASIO_HEADER_ONLY)
# include <boost/asio/detail/impl/resolver_thread_pool.ipp>
#endif // defined(BOOST_ASIO_HEADER_ONLY)

#endif // BOOST_ASIO_DETAIL_RESOLVER_THREAD_POOL_HPP

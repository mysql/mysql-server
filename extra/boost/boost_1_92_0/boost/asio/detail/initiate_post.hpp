//
// detail/initiate_post.hpp
// ~~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2026 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_ASIO_DETAIL_INITIATE_POST_HPP
#define BOOST_ASIO_DETAIL_INITIATE_POST_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include <boost/asio/detail/config.hpp>
#include <boost/asio/associated_allocator.hpp>
#include <boost/asio/associated_executor.hpp>
#include <boost/asio/detail/work_dispatcher.hpp>
#include <boost/asio/execution/allocator.hpp>
#include <boost/asio/execution/blocking.hpp>
#include <boost/asio/execution/relationship.hpp>
#include <boost/asio/prefer.hpp>
#include <boost/asio/require.hpp>

#include <boost/asio/detail/push_options.hpp>

namespace boost {
namespace asio {
BOOST_ASIO_INLINE_NAMESPACE_BEGIN
namespace detail {

class initiate_post
{
public:
  template <typename CompletionHandler>
  void operator()(CompletionHandler&& handler,
      enable_if_t<
        execution::is_executor<
          associated_executor_t<decay_t<CompletionHandler>>
        >::value
      >* = 0) const
  {
    associated_executor_t<decay_t<CompletionHandler>> ex(
        (get_associated_executor)(handler));

    associated_allocator_t<decay_t<CompletionHandler>> alloc(
        (get_associated_allocator)(handler));

    boost::asio::prefer(
        boost::asio::require(ex, execution::blocking.never),
        execution::relationship.fork,
        execution::allocator(alloc)
      ).execute(
        boost::asio::detail::bind_handler(
          static_cast<CompletionHandler&&>(handler)));
  }

  template <typename CompletionHandler>
  void operator()(CompletionHandler&& handler,
      enable_if_t<
        !execution::is_executor<
          associated_executor_t<decay_t<CompletionHandler>>
        >::value
      >* = 0) const
  {
    associated_executor_t<decay_t<CompletionHandler>> ex(
        (get_associated_executor)(handler));

    associated_allocator_t<decay_t<CompletionHandler>> alloc(
        (get_associated_allocator)(handler));

    ex.post(boost::asio::detail::bind_handler(
          static_cast<CompletionHandler&&>(handler)), alloc);
  }
};

template <typename Executor>
class initiate_post_with_executor
{
public:
  typedef Executor executor_type;

  explicit initiate_post_with_executor(const Executor& ex)
    : ex_(ex)
  {
  }

  executor_type get_executor() const noexcept
  {
    return ex_;
  }

  template <typename CompletionHandler, typename Function>
  void operator()(CompletionHandler&& handler, Function&&,
      enable_if_t<
        execution::is_executor<
          conditional_t<true, executor_type, CompletionHandler>
        >::value
      >* = 0,
      enable_if_t<
        !is_work_dispatcher_required<
          decay_t<Function>,
          decay_t<CompletionHandler>,
          Executor
        >::value
      >* = 0) const
  {
    associated_allocator_t<decay_t<CompletionHandler>> alloc(
        (get_associated_allocator)(handler));

    boost::asio::prefer(
        boost::asio::require(ex_, execution::blocking.never),
        execution::relationship.fork,
        execution::allocator(alloc)
      ).execute(
        boost::asio::detail::bind_handler(
          static_cast<CompletionHandler&&>(handler)));
  }

  template <typename CompletionHandler, typename Function>
  void operator()(CompletionHandler&& handler, Function&& function,
      enable_if_t<
        execution::is_executor<
          conditional_t<true, executor_type, CompletionHandler>
        >::value
      >* = 0,
      enable_if_t<
        is_work_dispatcher_required<
          decay_t<Function>,
          decay_t<CompletionHandler>,
          Executor
        >::value
      >* = 0) const
  {
    typedef decay_t<CompletionHandler> handler_t;
    typedef decay_t<Function> function_t;

    typedef associated_executor_t<handler_t, Executor> handler_ex_t;
    handler_ex_t handler_ex((get_associated_executor)(handler, ex_));

    associated_allocator_t<handler_t> alloc(
        (get_associated_allocator)(handler));

    boost::asio::prefer(
        boost::asio::require(ex_, execution::blocking.never),
        execution::relationship.fork,
        execution::allocator(alloc)
      ).execute(
        work_dispatcher<function_t, handler_t, handler_ex_t>(
          static_cast<Function&&>(function),
          static_cast<CompletionHandler&&>(handler), handler_ex));
  }

  template <typename CompletionHandler, typename Function>
  void operator()(CompletionHandler&& handler, Function&&,
      enable_if_t<
        !execution::is_executor<
          conditional_t<true, executor_type, CompletionHandler>
        >::value
      >* = 0,
      enable_if_t<
        !is_work_dispatcher_required<
          decay_t<Function>,
          decay_t<CompletionHandler>,
          Executor
        >::value
      >* = 0) const
  {
    associated_allocator_t<decay_t<CompletionHandler>> alloc(
        (get_associated_allocator)(handler));

    ex_.post(boost::asio::detail::bind_handler(
          static_cast<CompletionHandler&&>(handler)), alloc);
  }

  template <typename CompletionHandler, typename Function>
  void operator()(CompletionHandler&& handler, Function&& function,
      enable_if_t<
        !execution::is_executor<
          conditional_t<true, executor_type, CompletionHandler>
        >::value
      >* = 0,
      enable_if_t<
        is_work_dispatcher_required<
          decay_t<Function>,
          decay_t<CompletionHandler>,
          Executor
        >::value
      >* = 0) const
  {
    typedef decay_t<CompletionHandler> handler_t;
    typedef decay_t<Function> function_t;

    typedef associated_executor_t<handler_t, Executor> handler_ex_t;
    handler_ex_t handler_ex((get_associated_executor)(handler, ex_));

    associated_allocator_t<handler_t> alloc(
        (get_associated_allocator)(handler));

    ex_.post(work_dispatcher<function_t, handler_t, handler_ex_t>(
          static_cast<Function&&>(function),
          static_cast<CompletionHandler&&>(handler), handler_ex), alloc);
  }

private:
  Executor ex_;
};

} // namespace detail
BOOST_ASIO_INLINE_NAMESPACE_END
} // namespace asio
} // namespace boost

#include <boost/asio/detail/pop_options.hpp>

#endif // BOOST_ASIO_DETAIL_INITIATE_POST_HPP

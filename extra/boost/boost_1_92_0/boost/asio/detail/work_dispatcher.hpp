//
// detail/work_dispatcher.hpp
// ~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2026 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_ASIO_DETAIL_WORK_DISPATCHER_HPP
#define BOOST_ASIO_DETAIL_WORK_DISPATCHER_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include <boost/asio/detail/config.hpp>
#include <boost/asio/detail/bind_handler.hpp>
#include <boost/asio/detail/type_traits.hpp>
#include <boost/asio/associated_executor.hpp>
#include <boost/asio/associated_allocator.hpp>
#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/execution/executor.hpp>
#include <boost/asio/execution/allocator.hpp>
#include <boost/asio/execution/blocking.hpp>
#include <boost/asio/execution/outstanding_work.hpp>
#include <boost/asio/prefer.hpp>

#include <boost/asio/detail/push_options.hpp>

namespace boost {
namespace asio {
BOOST_ASIO_INLINE_NAMESPACE_BEGIN
namespace detail {

struct empty_work_function
{
  void operator()() const noexcept
  {
  }
};

template <typename Function, typename = void>
struct work_result
{
};

template <typename Function>
struct work_result<Function, void_t<result_of_t<decay_t<Function>()>>>
{
  typedef decay_t<result_of_t<decay_t<Function>()>> type;
};

template <typename Function>
using work_result_t = typename work_result<Function>::type;

template <typename Function, typename Handler,
    typename Executor, typename = void>
struct is_work_dispatcher_required : true_type
{
};

template <typename Handler, typename Executor>
struct is_work_dispatcher_required<empty_work_function, Handler, Executor,
    enable_if_t<
      is_same<
        typename associated_executor<Handler,
          Executor>::asio_associated_executor_is_unspecialised,
        void
      >::value
    >> : false_type
{
};

template <typename Function,
    bool IsVoid = is_void<result_of_t<Function()>>::value,
    bool IsClass = is_class<Function>::value>
class work_dispatcher_function
{
  Function function_;

public:
  template <typename F>
  work_dispatcher_function(F&& function)
    : function_(static_cast<F&&>(function))
  {
  }

  work_dispatcher_function(const work_dispatcher_function& other)
    : function_(other.function_)
  {
  }

  work_dispatcher_function(work_dispatcher_function&& other)
    : function_(static_cast<Function&&>(other.function_))
  {
  }

  template <typename Handler>
  auto bind_result(Handler&& handler)
    -> decltype(
      boost::asio::detail::move_bind_handler(
        static_cast<Handler&&>(handler),
        static_cast<Function&&>(function_)()))
  {
    return boost::asio::detail::move_bind_handler(
        static_cast<Handler&&>(handler),
        static_cast<Function&&>(function_)());
  }
};

template <typename Function>
class work_dispatcher_function<Function, false, true> : Function
{
public:
  template <typename F>
  work_dispatcher_function(F&& function)
    : Function(static_cast<F&&>(function))
  {
  }

  work_dispatcher_function(const work_dispatcher_function& other)
    : Function(static_cast<const Function&>(other))
  {
  }

  work_dispatcher_function(work_dispatcher_function&& other)
    : Function(static_cast<Function&&>(other))
  {
  }

  template <typename Handler>
  auto bind_result(Handler&& handler)
    -> decltype(
      boost::asio::detail::move_bind_handler(
        static_cast<Handler&&>(handler),
        static_cast<Function&&>(*static_cast<Function*>(this))()))
  {
    return boost::asio::detail::move_bind_handler(
        static_cast<Handler&&>(handler),
        static_cast<Function&&>(*static_cast<Function*>(this))());
  }
};

template <typename Function>
class work_dispatcher_function<Function, true, false>
{
  Function function_;

public:
  template <typename F>
  work_dispatcher_function(F&& function)
    : function_(static_cast<Function&&>(function))
  {
  }

  work_dispatcher_function(const work_dispatcher_function& other)
    : function_(other.function_)
  {
  }

  work_dispatcher_function(work_dispatcher_function&& other)
    : function_(static_cast<Function&&>(other.function_))
  {
  }

  template <typename Handler>
  auto bind_result(Handler&& handler)
    -> decltype(boost::asio::detail::bind_handler(
      static_cast<Handler&&>(handler)))
  {
    static_cast<Function&&>(function_)();
    return boost::asio::detail::bind_handler(
        static_cast<Handler&&>(handler));
  }
};

template <typename Function>
class work_dispatcher_function<Function, true, true> : Function
{
public:
  template <typename F>
  work_dispatcher_function(F&& function)
    : Function(static_cast<F&&>(function))
  {
  }

  work_dispatcher_function(const work_dispatcher_function& other)
    : Function(static_cast<const Function&>(other))
  {
  }

  work_dispatcher_function(work_dispatcher_function&& other)
    : Function(static_cast<Function&&>(other))
  {
  }

  template <typename Handler>
  auto bind_result(Handler&& handler)
    -> decltype(boost::asio::detail::bind_handler(
      static_cast<Handler&&>(handler)))
  {
    static_cast<Function&&>(*static_cast<Function*>(this))();
    return boost::asio::detail::bind_handler(
        static_cast<Handler&&>(handler));
  }
};

template <typename Function, typename Handler,
    typename Executor, typename = void>
class work_dispatcher : work_dispatcher_function<Function>
{
public:
  template <typename F, typename CompletionHandler>
  work_dispatcher(F&& function, CompletionHandler&& handler,
      const Executor& handler_ex)
    : work_dispatcher_function<Function>(static_cast<F&&>(function)),
      handler_(static_cast<CompletionHandler&&>(handler)),
      executor_(boost::asio::prefer(handler_ex,
          execution::outstanding_work.tracked))
  {
  }

  work_dispatcher(const work_dispatcher& other)
    : work_dispatcher_function<Function>(other),
      handler_(other.handler_),
      executor_(other.executor_)
  {
  }

  work_dispatcher(work_dispatcher&& other)
    : work_dispatcher_function<Function>(
        static_cast<work_dispatcher_function<Function>&&>(other)),
      handler_(static_cast<Handler&&>(other.handler_)),
      executor_(static_cast<work_executor_type&&>(other.executor_))
  {
  }

  void operator()()
  {
    associated_allocator_t<Handler> alloc((get_associated_allocator)(handler_));
    boost::asio::prefer(executor_, execution::allocator(alloc)).execute(
        this->bind_result(static_cast<Handler&&>(handler_)));
  }

private:
  typedef decay_t<
      prefer_result_t<const Executor&,
        execution::outstanding_work_t::tracked_t
      >
    > work_executor_type;

  Handler handler_;
  work_executor_type executor_;
};

#if !defined(BOOST_ASIO_NO_TS_EXECUTORS)

template <typename Function, typename Handler, typename Executor>
class work_dispatcher<Function, Handler, Executor,
    enable_if_t<!execution::is_executor<Executor>::value>>
  : work_dispatcher_function<Function>
{
public:
  template <typename F, typename CompletionHandler>
  work_dispatcher(F&& function, CompletionHandler&& handler,
      const Executor& handler_ex)
    : work_dispatcher_function<Function>(static_cast<F&&>(function)),
      work_(handler_ex),
      handler_(static_cast<CompletionHandler&&>(handler))
  {
  }

  work_dispatcher(const work_dispatcher& other)
    : work_dispatcher_function<Function>(other),
      work_(other.work_),
      handler_(other.handler_)
  {
  }

  work_dispatcher(work_dispatcher&& other)
    : work_dispatcher_function<Function>(
        static_cast<work_dispatcher_function<Function>&&>(*this)),
      work_(static_cast<executor_work_guard<Executor>&&>(other.work_)),
      handler_(static_cast<Handler&&>(other.handler_))
  {
  }

  void operator()()
  {
    associated_allocator_t<Handler> alloc((get_associated_allocator)(handler_));
    work_.get_executor().dispatch(
        this->bind_result(static_cast<Handler&&>(handler_)), alloc);
    work_.reset();
  }

private:
  executor_work_guard<Executor> work_;
  Handler handler_;
};

#endif // !defined(BOOST_ASIO_NO_TS_EXECUTORS)

} // namespace detail
BOOST_ASIO_INLINE_NAMESPACE_END
} // namespace asio
} // namespace boost

#include <boost/asio/detail/pop_options.hpp>

#endif // BOOST_ASIO_DETAIL_WORK_DISPATCHER_HPP

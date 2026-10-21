//
// Copyright (c) 2022 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_COBALT_IMPL_CHANNEL_HPP
#define BOOST_COBALT_IMPL_CHANNEL_HPP

#include <boost/cobalt/channel.hpp>
#include <boost/cobalt/result.hpp>

#include <boost/asio/post.hpp>

namespace boost::cobalt
{

#if !defined(BOOST_COBALT_NO_PMR)
template<typename T>
inline channel<T>::channel(
    std::size_t limit,
    executor executor,
    pmr::memory_resource * resource)
    : buffer_(limit, pmr::polymorphic_allocator<T>(resource)), executor_(executor) {}
#else
template<typename T>
inline channel<T>::channel(
    std::size_t limit,
    executor executor)
    : buffer_(limit), executor_(executor) {}
#endif

template<typename T>
auto channel<T>::get_executor() -> const executor_type &  {return executor_;}

template<typename T>
bool channel<T>::is_open() const {return !is_closed_;}


template<typename T>
channel<T>::~channel()
{
  while (!read_queue_.empty())
    read_queue_.front().awaited_from.reset();

  while (!write_queue_.empty())
    write_queue_.front().awaited_from.reset();

}

template<typename T>
void channel<T>::close()
{
  is_closed_ = true;
  while (!read_queue_.empty())
  {
    auto & op = read_queue_.front();
    op.unlink();
    op.cancelled = true;
    op.cancel_slot.clear();

    if (op.awaited_from)
      asio::post(executor_, std::move(op.awaited_from));
  }
  while (!write_queue_.empty())
  {
    auto & op = write_queue_.front();
    op.unlink();
    op.cancelled = true;
    op.closed = true;
    op.cancel_slot.clear();
    if (op.awaited_from)
      asio::post(executor_, std::move(op.awaited_from));
  }
}


template<typename T>
struct channel<T>::read_op::cancel_impl
{
  read_op * op;
  cancel_impl(read_op * op) : op(op) {}
  void operator()(asio::cancellation_type)
  {
    op->cancelled = true;
    op->unlink();
    if (op->awaited_from)
      asio::post(
          op->chn->executor_,
          std::move(op->awaited_from));
    op->cancel_slot.clear();
  }
};

template<typename T>
template<typename Promise>
std::coroutine_handle<void> channel<T>::read_op::await_suspend(std::coroutine_handle<Promise> h)
{
  if (cancelled)
    return h; // already interrupted.

  if constexpr (requires {h.promise().get_cancellation_slot();})
    if ((cancel_slot = h.promise().get_cancellation_slot()).is_connected())
      cancel_slot.emplace<cancel_impl>(this);

  if (awaited_from)
    boost::throw_exception(std::runtime_error("already-awaited"), loc);
  awaited_from.reset(h.address());
  // currently nothing to read
  if constexpr (requires {h.promise().begin_transaction();})
    begin_transaction = +[](void * p){std::coroutine_handle<Promise>::from_address(p).promise().begin_transaction();};

  if (chn->write_queue_.empty())
  {
    chn->read_queue_.push_back(*this);
    return std::noop_coroutine();
  }
  else
  {
    cancel_slot.clear();
    auto & op = chn->write_queue_.front();
    // transactional_unlink can interrupt or cancel `op` through `race`, so we need to check.
    op.direct = true;
    if constexpr (std::is_copy_constructible_v<T>)
    {
      if (op.ref.index() == 0)
        direct = std::move(*variant2::get<0>(op.ref));
      else
        direct = *variant2::get<1>(op.ref);
    }
    else
      direct = std::move(*op.ref);

    op.transactional_unlink();
    BOOST_ASSERT(op.awaited_from);
    BOOST_ASSERT(awaited_from);

    asio::post(chn->executor_, std::move(awaited_from));
    return op.awaited_from.release();
  }
}


template<typename T>
T channel<T>::read_op::await_resume()
{
  return await_resume(as_result_tag{}).value(loc);
}

template<typename T>
std::tuple<system::error_code, T> channel<T>::read_op::await_resume(const struct as_tuple_tag &)
{
  auto res = await_resume(as_result_tag{});

  if (res.has_error())
    return {res.error(), T{}};
  else
    return {system::error_code{}, std::move(*res)};

}

template<typename T>
system::result<T> channel<T>::read_op::await_resume(const struct as_result_tag &)
{
  if (cancel_slot.is_connected())
    cancel_slot.clear();

  if (chn->is_closed_ && chn->buffer_.empty() && !direct)
  {
    constexpr static boost::source_location loc{BOOST_CURRENT_LOCATION};
    return {system::in_place_error, asio::error::broken_pipe, &loc};
  }

  if (cancelled)
  {
    constexpr static boost::source_location loc{BOOST_CURRENT_LOCATION};
    return {system::in_place_error, asio::error::operation_aborted, &loc};
  }

  T value = chn->buffer_.empty() ? std::move(*direct) : std::move(chn->buffer_.front());
  if (!chn->buffer_.empty())
  {
    chn->buffer_.pop_front();
    if (direct)
      chn->buffer_.push_back(std::move(*direct));
  }

  if (!chn->write_queue_.empty())
  {
    auto &op = chn->write_queue_.front();
    BOOST_ASSERT(chn->read_queue_.empty());
    if (op.await_ready())
    {
      op.unlink();
      if (!op.cancelled && !op.closed)
      {
        op.direct = true;
        if constexpr (std::is_copy_constructible_v<T>)
        {
          if (op.ref.index() == 0)
            chn->buffer_.push_back(std::move(*variant2::get<0>(op.ref)));
          else
            chn->buffer_.push_back(*variant2::get<1>(op.ref));
        }
        else
          chn->buffer_.push_back(std::move(*op.ref));
      }
      BOOST_ASSERT(op.awaited_from);
      asio::post(chn->executor_, std::move(op.awaited_from));
    }
  }
  return {system::in_place_value, std::move(value)};
}

template<typename T>
struct channel<T>::write_op::cancel_impl
{
  write_op * op;
  cancel_impl(write_op * op) : op(op) {}
  void operator()(asio::cancellation_type)
  {
    op->cancelled = true;
    op->unlink();
    if (op->awaited_from)
      asio::post(
        op->chn->executor_, std::move(op->awaited_from));
    op->cancel_slot.clear();
  }
};

template<typename T>
template<typename Promise>
std::coroutine_handle<void> channel<T>::write_op::await_suspend(std::coroutine_handle<Promise> h)
{
  if (cancelled)
    return h; // already interrupted.


  if constexpr (requires {h.promise().get_cancellation_slot();})
    if ((cancel_slot = h.promise().get_cancellation_slot()).is_connected())
      cancel_slot.emplace<cancel_impl>(this);

  awaited_from.reset(h.address());
  if constexpr (requires {h.promise().begin_transaction();})
    begin_transaction = +[](void * p){std::coroutine_handle<Promise>::from_address(p).promise().begin_transaction();};

  BOOST_ASSERT(this->chn->buffer_.full());
  if (chn->read_queue_.empty())
  {
    chn->write_queue_.push_back(*this);
    return std::noop_coroutine();
  }
  else
  {
    cancel_slot.clear();
    auto & op = chn->read_queue_.front();
    if constexpr (std::is_copy_constructible_v<T>)
    {
      if (ref.index() == 0)
        op.direct.emplace(std::move(*variant2::get<0>(ref)));
      else
        op.direct.emplace(*variant2::get<1>(ref));
    }
    else
      op.direct.emplace(std::move(*ref));

    direct = true;
    op.transactional_unlink();

    BOOST_ASSERT(op.awaited_from);
    BOOST_ASSERT(awaited_from);
    asio::post(chn->executor_, std::move(awaited_from));

    return op.awaited_from.release();
  }
}

template<typename T>
std::tuple<system::error_code> channel<T>::write_op::await_resume(const struct as_tuple_tag &)
{
  return await_resume(as_result_tag{}).error();
}

template<typename T>
void channel<T>::write_op::await_resume()
{
  await_resume(as_result_tag{}).value(loc);
}

template<typename T>
system::result<void>  channel<T>::write_op::await_resume(const struct as_result_tag &)
{
  if (cancel_slot.is_connected())
    cancel_slot.clear();

  if (closed)
  {
    constexpr static boost::source_location loc{BOOST_CURRENT_LOCATION};
    return {system::in_place_error, asio::error::broken_pipe, &loc};
  }

  if (cancelled)
  {
    constexpr static boost::source_location loc{BOOST_CURRENT_LOCATION};
    return {system::in_place_error, asio::error::operation_aborted, &loc};
  }

  if (!direct)
  {
    BOOST_ASSERT(!chn->buffer_.full());
    if constexpr (std::is_copy_constructible_v<T>)
    {
      if (ref.index() == 0)
        chn->buffer_.push_back(std::move(*variant2::get<0>(ref)));
      else
        chn->buffer_.push_back(*variant2::get<1>(ref));
    }
    else
      chn->buffer_.push_back(std::move(*ref));
  }

  if (!chn->read_queue_.empty())
  {
    auto & op = chn->read_queue_.front();
    BOOST_ASSERT(chn->write_queue_.empty());
    if (op.await_ready())
    {
      // unlink?
      op.unlink();
      BOOST_ASSERT(op.awaited_from);
      asio::post(chn->executor_, std::move(op.awaited_from));
    }
  }
  return system::in_place_value;
}

struct channel<void>::read_op::cancel_impl
{
  read_op * op;
  cancel_impl(read_op * op) : op(op) {}
  void operator()(asio::cancellation_type)
  {
    op->cancelled = true;
    op->unlink();
    if (op->awaited_from)
      asio::post(op->chn->executor_, std::move(op->awaited_from));
    op->cancel_slot.clear();
  }
};

struct channel<void>::write_op::cancel_impl
{
  write_op * op;
  cancel_impl(write_op * op) : op(op) {}
  void operator()(asio::cancellation_type)
  {
    op->cancelled = true;
    op->unlink();
    if (op->awaited_from)
      asio::post(op->chn->executor_, std::move(op->awaited_from));
    op->cancel_slot.clear();
  }
};

template<typename Promise>
std::coroutine_handle<void> channel<void>::read_op::await_suspend(std::coroutine_handle<Promise> h)
{
  if (cancelled)
    return h; // already interrupted.

  if constexpr (requires {h.promise().get_cancellation_slot();})
    if ((cancel_slot = h.promise().get_cancellation_slot()).is_connected())
      cancel_slot.emplace<cancel_impl>(this);

  if (awaited_from)
    boost::throw_exception(std::runtime_error("already-awaited"), loc);
  awaited_from.reset(h.address());

  if constexpr (requires {h.promise().begin_transaction();})
    begin_transaction = +[](void * p){std::coroutine_handle<Promise>::from_address(p).promise().begin_transaction();};

  // nothing to read currently, enqueue
  if (chn->write_queue_.empty())
  {
    chn->read_queue_.push_back(*this);
    return std::noop_coroutine();
  }
  else // we're good, we can read, so we'll do that, but we need to post, so we need to initialize a transactin.
  {
    cancel_slot.clear();
    auto & op = chn->write_queue_.front();
    op.direct = true;
    direct = true;
    op.transactional_unlink();

    BOOST_ASSERT(op.awaited_from);
    BOOST_ASSERT(awaited_from);
    asio::post(chn->executor_, std::move(awaited_from));
    return op.awaited_from.release();
  }
}


template<typename Promise>
std::coroutine_handle<void> channel<void>::write_op::await_suspend(std::coroutine_handle<Promise> h)
{
  if (cancelled)
    return h; // already interrupted.

  if constexpr (requires {h.promise().get_cancellation_slot();})
    if ((cancel_slot = h.promise().get_cancellation_slot()).is_connected())
      cancel_slot.emplace<cancel_impl>(this);

  awaited_from.reset(h.address());
  // currently nothing to read
  if constexpr (requires {h.promise().begin_transaction();})
    begin_transaction = +[](void * p){std::coroutine_handle<Promise>::from_address(p).promise().begin_transaction();};

  if (chn->read_queue_.empty())
  {
    chn->write_queue_.push_back(*this);
    return std::noop_coroutine();
  }
  else
  {
    cancel_slot.clear();
    auto & op = chn->read_queue_.front();
    op.direct = true; // let interrupt_await know that we'll be resuming it!
    direct = true;
    op.transactional_unlink();

    BOOST_ASSERT(op.awaited_from);
    BOOST_ASSERT(awaited_from);

    asio::post(chn->executor_, std::move(awaited_from));
    return op.awaited_from.release();
  }
}

}

#endif //BOOST_COBALT_IMPL_CHANNEL_HPP

//
// Copyright (c) 2024 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_COBALT_EXPERIMENTAL_YIELD_CONTEXT_HPP
#define BOOST_COBALT_EXPERIMENTAL_YIELD_CONTEXT_HPP

#include <boost/cobalt/experimental/frame.hpp>
#include <boost/cobalt/concepts.hpp>

#include <boost/asio/spawn.hpp>
#include <coroutine>

template<typename Executor>
struct std::coroutine_handle<boost::asio::basic_yield_context<Executor>>
{
  constexpr operator coroutine_handle<>() const noexcept { return coroutine_handle<>::from_address(address()); }

  constexpr explicit operator bool() const noexcept { return true; }

  constexpr bool done() const noexcept { return false; }
  void operator()() const noexcept {}

  void resume() const noexcept {frame_->promiseresume();}
  void destroy() const noexcept {frame_->destroy();}

  boost::asio::basic_yield_context<Executor> & promise() const noexcept { return frame_->promise; }

  constexpr void* address() const noexcept { return frame_; }

  struct yield_context_frame :
      boost::cobalt::experimental::frame<yield_context_frame, boost::asio::basic_yield_context<Executor>>
  {
    using boost::cobalt::experimental::frame<yield_context_frame, boost::asio::basic_yield_context<Executor>>::frame;
    void resume()
    {
      lifetime.resume();
    }
    void destroy()
    {
      // destroy the lifetime.
      auto lf = std::move(lifetime);
    }

    boost::asio::detail::spawn_handler_base<Executor> lifetime{this->promise};
  };

  coroutine_handle(yield_context_frame & frame) : frame_(&frame) {}
 private:
  yield_context_frame * frame_;
};

namespace boost::cobalt::experimental
{

template<awaitable_type Aw, typename Executor>
auto await(Aw && aw, boost::asio::basic_yield_context<Executor> ctx)
{
  if (!std::forward<Aw>(aw).await_ready())
  {

    using ch = std::coroutine_handle<boost::asio::basic_yield_context<Executor>>;
    typename ch::yield_context_frame fr{std::move(ctx)};
    ch h{fr};
    ctx.spawned_thread_->suspend_with(
        [&]
        {
          using rt = decltype(std::forward<Aw>(aw).await_suspend(h));
          if constexpr (std::is_void_v<rt>)
            std::forward<Aw>(aw).await_suspend(h);
          else if constexpr (std::is_same_v<rt, bool>)
          {
            if (!std::forward<Aw>(aw).await_suspend(h))
              ctx.spawned_thread_->resume();
          }
          else
            std::forward<Aw>(aw).await_suspend(h).resume();
        }
    );

  }
  return std::forward<Aw>(aw).await_resume();

}

template<typename Aw, typename Executor>
  requires requires (Aw && aw) {{std::forward<Aw>(aw).operator co_await()} -> awaitable_type; }
auto await(Aw && aw, boost::asio::basic_yield_context<Executor> ctx)
{
  return await(std::forward<Aw>(aw).operator co_await(), std::move(ctx));
}

template<typename Aw, typename Executor>
requires requires (Aw && aw) {{operator co_await(std::forward<Aw>(aw))} -> awaitable_type; }
auto await(Aw && aw, boost::asio::basic_yield_context<Executor> ctx)
{
  return await(operator co_await(std::forward<Aw>(aw)), std::move(ctx));
}

}


#endif //BOOST_COBALT_EXPERIMENTAL_YIELD_CONTEXT_HPP

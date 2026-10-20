//
// Copyright (c) 2024 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_COBALT_EXPERIMENTAL_COMPOSITION_HPP
#define BOOST_COBALT_EXPERIMENTAL_COMPOSITION_HPP

#include <boost/cobalt/op.hpp>
#include <boost/cobalt/detail/sbo_resource.hpp>


namespace boost::cobalt::detail
{

template<typename ... Args>
struct composition_promise
    :
      promise_cancellation_base<asio::cancellation_slot, asio::enable_total_cancellation>,
      enable_await_allocator<composition_promise<Args...>>,
      enable_await_executor<composition_promise<Args...>>
{
  void get_return_object() {}

  using promise_cancellation_base<asio::cancellation_slot, asio::enable_total_cancellation>::await_transform;
  using enable_await_allocator<composition_promise<Args...>>::await_transform;
  using enable_await_executor<composition_promise<Args...>>::await_transform;

  using handler_type = completion_handler<Args...>;

  using allocator_type = typename handler_type::allocator_type;
  allocator_type get_allocator() const {return handler.get_allocator();}
#if !defined(BOOST_COBALT_NO_PMR)
  using resource_type = pmr::memory_resource;
#else
  using resource_type = cobalt::detail::sbo_resource;
#endif

  template<typename ... Args_, typename Initiation, typename ... InitArgs>
  BOOST_COBALT_MSVC_NOINLINE
  auto await_transform(asio::deferred_async_operation<void(Args_...), Initiation, InitArgs...> op_)
  {
    struct deferred_op : op<Args_...>
    {
      asio::deferred_async_operation<void(Args_...), Initiation, InitArgs...> op_;
      deferred_op(asio::deferred_async_operation<void(Args_...), Initiation, InitArgs...> op_,
                  resource_type * resource)
          : op_(std::move(op_)), resource(resource) {}

      void initiate(cobalt::completion_handler<Args_...> complete) override
      {
        std::move(op_)(std::move(complete));
      }

      resource_type * resource;

      typename op<Args_...>::awaitable_base operator co_await()
      {
        return static_cast<op<Args_...>&&>(*this).operator co_await().replace_resource(this->resource);
      }
    };

    return cobalt::as_tuple(deferred_op{std::move(op_), handler.get_allocator().resource()});
  }

  template<typename Op>
    requires requires (Op && op, resource_type* res)
    {
      {static_cast<Op>(op).operator co_await().replace_resource(res)} -> awaitable_type<composition_promise>;
    }
  BOOST_COBALT_MSVC_NOINLINE
  auto await_transform(Op && op_)
  {
    struct replacing_op
    {
      Op op;

      resource_type * resource;

      auto operator co_await()
      {
        return std::forward<Op>(op).operator co_await().replace_resource(this->resource);
      }
    };

    return cobalt::as_tuple(replacing_op{std::forward<Op>(op_), handler.get_allocator().resource()});
  }


  using executor_type = typename handler_type::executor_type ;
  const executor & get_executor() const {return handler.get_executor();}

  template<typename ... Ts>
  static void * operator new(const std::size_t size, Ts & ... args)
  {
    using tt = std::pair<resource_type *, std::size_t>;

    // | memory_resource | size_t | <padding> | coroutine.
    constexpr auto block_size =  sizeof(tt) / sizeof(std::max_align_t)
                                 + (sizeof(tt) % sizeof(std::max_align_t) ? 1 : 0);


    auto res = std::get<sizeof... (Ts) - 1>(std::tie(args...)).get_allocator().resource();
    const auto p = res->allocate(size + (block_size * sizeof(std::max_align_t)));
    new (p) tt(res, size);
    return static_cast<std::max_align_t*>(p) + block_size;
  }

  static void operator delete(void * raw) noexcept
  {
    using tt = std::pair<resource_type *, std::size_t>;

    // | memory_resource | size_t | <padding> | coroutine.
    constexpr auto block_size =  sizeof(tt) / sizeof(std::max_align_t)
                                 + (sizeof(tt) % sizeof(std::max_align_t) ? 1 : 0);

    const auto p = static_cast<std::max_align_t*>(raw) - block_size;

    const auto tp = *reinterpret_cast<tt*>(p);
    const auto res = tp.first;
    const auto size = tp.second;

    res->deallocate(p, size +  (block_size * sizeof(std::max_align_t)));
  }

  completion_handler<Args...> handler;

  template<typename ... Ts>
  composition_promise(Ts && ... args) : handler(std::move(std::get<sizeof... (Ts) - 1>(std::tie(args...))))
  {

  }

  void unhandled_exception() { throw ; }
  constexpr static std::suspend_never initial_suspend() {return {};}

  void return_value(std::tuple<Args ...> args)
  {
    handler.result.emplace(std::move(args));
  }


  struct final_awaitable
  {
    constexpr bool await_ready() noexcept  {return false;}
    completion_handler<Args...> handler;

    BOOST_COBALT_MSVC_NOINLINE
    std::coroutine_handle<void> await_suspend(std::coroutine_handle<composition_promise> h) noexcept
    {
      auto exec = handler.get_executor();
      auto ho = handler.self.release();
      detail::self_destroy(h, exec);

      if (handler.completed_immediately != nullptr
       && *handler.completed_immediately == completed_immediately_t::initiating)
        //not maybe here, because we don't go through the immediate executor
      {
        *handler.completed_immediately = completed_immediately_t::yes;
        return std::noop_coroutine();
      }

      return ho;
    }

    constexpr void await_resume() noexcept {}
  };

  BOOST_COBALT_MSVC_NOINLINE
  auto final_suspend() noexcept
  {
    return final_awaitable{std::move(handler)};
  }
};

}

template<typename ... Args>
struct std::coroutine_traits<void, boost::cobalt::completion_handler<Args...>>
{
  using promise_type = ::boost::cobalt::detail::composition_promise<Args...>;
};

template<typename T0, typename ... Args>
struct std::coroutine_traits<void, T0, boost::cobalt::completion_handler<Args...>>
{
  using promise_type = ::boost::cobalt::detail::composition_promise<Args...>;
};

template<typename T0, typename T1, typename ... Args>
struct std::coroutine_traits<void, T0, T1, boost::cobalt::completion_handler<Args...>>
{
  using promise_type = ::boost::cobalt::detail::composition_promise<Args...>;
};


template<typename T0, typename T1, typename T2, typename ... Args>
struct std::coroutine_traits<void, T0, T1, T2, boost::cobalt::completion_handler<Args...>>
{
  using promise_type = ::boost::cobalt::detail::composition_promise<Args...>;
};


template<typename T0, typename T1, typename T2, typename T3, typename ... Args>
struct std::coroutine_traits<void, T0, T1, T2, T3, boost::cobalt::completion_handler<Args...>>
{
  using promise_type = ::boost::cobalt::detail::composition_promise<Args...>;
};


template<typename T0, typename T1, typename T2, typename T3, typename T4, typename ... Args>
struct std::coroutine_traits<void, T0, T1, T2, T3, T4, boost::cobalt::completion_handler<Args...>>
{
  using promise_type = ::boost::cobalt::detail::composition_promise<Args...>;
};


template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5, typename ... Args>
struct std::coroutine_traits<void, T0, T1, T2, T3, T4, T5, boost::cobalt::completion_handler<Args...>>
{
  using promise_type = ::boost::cobalt::detail::composition_promise<Args...>;
};


template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename ... Args>
struct std::coroutine_traits<void, T0, T1, T2, T3, T4, T5, T6, boost::cobalt::completion_handler<Args...>>
{
  using promise_type = ::boost::cobalt::detail::composition_promise<Args...>;
};


template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename ... Args>
struct std::coroutine_traits<void, T0, T1, T2, T3, T4, T5, T6, T7, boost::cobalt::completion_handler<Args...>>
{
  using promise_type = ::boost::cobalt::detail::composition_promise<Args...>;
};


template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8, typename ... Args>
struct std::coroutine_traits<void, T0, T1, T2, T3, T4, T5, T6, T7, T8, boost::cobalt::completion_handler<Args...>>
{
  using promise_type = ::boost::cobalt::detail::composition_promise<Args...>;
};

#endif //BOOST_COBALT_EXPERIMENTAL_COMPOSITION_HPP

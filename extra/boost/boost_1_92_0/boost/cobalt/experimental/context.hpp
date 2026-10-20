// Copyright (c) 2024 Klemens D. Morgenstern
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#ifndef BOOST_COBALT_EXPERIMENTAL_CONTEXT_HPP
#define BOOST_COBALT_EXPERIMENTAL_CONTEXT_HPP

#include <boost/callable_traits/args.hpp>
#include <boost/context/fiber.hpp>
#include <boost/context/fixedsize_stack.hpp>

#include <boost/cobalt/concepts.hpp>
#include <boost/cobalt/experimental/frame.hpp>
#include <boost/cobalt/config.hpp>
#include <coroutine>
#include <new>

namespace boost::cobalt::experimental
{

template<typename, typename ...>
struct context;

namespace detail
{

template<typename Promise>
struct context_frame : frame<context_frame<Promise>,  Promise>
{
  boost::context::fiber caller, callee;

  void (*after_resume)(context_frame *, void *) = nullptr;
  void * after_resume_p;

  template<typename ... Args>
    requires std::constructible_from<Promise, Args...>
  context_frame(Args && ... args) : frame<context_frame, Promise>(args...) {}

  template<typename ... Args>
    requires (!std::constructible_from<Promise, Args...> && std::is_default_constructible_v<Promise>)
  context_frame(Args && ...) {}

  void resume()
  {
    callee = std::move(callee).resume();
    if (auto af = std::exchange(after_resume, nullptr))
      af(this, after_resume_p);
  }
  void destroy()
  {
    auto c = std::exchange(callee, {});
    this->~context_frame();
  }

  template<typename Awaitable>
  auto do_resume(void * )
  {
    return +[](context_frame * this_, void * p)
    {
      auto aw_ = static_cast<Awaitable*>(p);
      auto h = std::coroutine_handle<Promise>::from_address(this_) ;
      aw_->await_suspend(h);
    };
  }

  template<typename Awaitable>
  auto do_resume(bool * )
  {
    return +[](context_frame * this_, void * p)
    {
      auto aw_ = static_cast<Awaitable*>(p);
      auto h = std::coroutine_handle<Promise>::from_address(this_) ;
      if (!aw_->await_suspend(h))
        h.resume();
    };
  }

  template<typename Awaitable, typename Promise_>
  auto do_resume(std::coroutine_handle<Promise_> * )
  {
    return +[](context_frame * this_, void * p)
    {
      auto aw_ = static_cast<Awaitable*>(p);
      auto h = std::coroutine_handle<Promise>::from_address(this_) ;
      aw_->await_suspend(h).resume();
    };
  }


  template<typename Awaitable>
  auto do_await(Awaitable aw)
  {
    if (!aw.await_ready())
    {
      after_resume_p = & aw;
      after_resume = do_resume<Awaitable>(
          static_cast<decltype(aw.await_suspend(std::declval<std::coroutine_handle<Promise>>()))*>(nullptr)
          );
      caller = std::move(caller).resume();
    }
    return aw.await_resume();
  }

  template<typename Handle, typename ... Args>
  context<Handle, Args...> get_context()
  {
    return context<Handle, Args...>{this};
  }

};


template<typename Traits, typename Promise, typename ... Args>
struct stack_allocator : boost::context::fixedsize_stack {};

template<typename Traits, typename Promise, typename ... Args>
requires requires {Promise::operator new(std::size_t{});}
struct stack_allocator<Traits, Promise, Args...>
{
  boost::context::stack_context allocate()
  {
    const auto size = Traits::default_size();
    const auto p = Promise::operator new(size);

    boost::context::stack_context sctx;
    sctx.size = size;
    sctx.sp = static_cast< char * >( p) + sctx.size;
    return sctx;
  }

  void deallocate( boost::context::stack_context & sctx) noexcept
  {
    void * vp = static_cast< char * >( sctx.sp) - sctx.size;
    Promise::operator delete(vp, sctx.size);
  }
};

template<typename Traits, typename Promise, typename ... Args>
  requires requires {Promise::operator new(std::size_t{}, std::decay_t<Args&>()...);}
struct stack_allocator<Traits, Promise, Args...>
{
  std::tuple<Args&...> args;

  boost::context::stack_context allocate()
  {
    const auto size = Traits::default_size();
    const auto p = std::apply(
        [size](auto & ... args_)
        {
          return Promise::operator new(size, args_...);
        }, args);

    boost::context::stack_context sctx;
    sctx.size = size;
    sctx.sp = static_cast< char * >( p) + sctx.size;
    return sctx;
  }

  void deallocate( boost::context::stack_context & sctx) noexcept
  {
    void * vp = static_cast< char * >( sctx.sp) - sctx.size;
    Promise::operator delete(vp, sctx.size);
  }
};


struct await_transform_base
{
  struct dummy {};
  void await_transform(dummy);
};

template<typename T>
struct await_transform_impl : await_transform_base, T
{
};

template<typename T>
concept has_await_transform = ! requires (await_transform_impl<T> & p) {p.await_transform(await_transform_base::dummy{});};

template<typename Promise, typename Context, typename Func, typename ... Args>
void do_return(std::true_type /* is_void */, Promise& promise, Context ctx, Func && func, Args && ... args)
{
  std::forward<Func>(func)(ctx, std::forward<Args>(args)...);
  promise.return_void();
}

template<typename Promise, typename Context, typename Func, typename ... Args>
void do_return(std::false_type /* is_void */, Promise& promise, Context ctx, Func && func, Args && ... args)
{
  promise.return_value(std::forward<Func>(func)(ctx, std::forward<Args>(args)...));
}


}

template<typename Return, typename ... Args>
struct context
{
  using return_type = Return;
  using promise_type = typename std::coroutine_traits<Return, Args...>::promise_type;

        promise_type & promise()       {return frame_->promise;}
  const promise_type & promise() const {return frame_->promise;}
  template<typename Return_, typename ... Args_>
    requires std::same_as<promise_type, typename context<Return_, Args_...>::promise_type>
  constexpr operator context<Return_, Args_...>() const
  {
    return {frame_};
  }

  template<typename Awaitable>
    requires (detail::has_await_transform<promise_type> &&
        requires (promise_type & pro, Awaitable && aw)
        {
          {pro.await_transform(std::forward<Awaitable>(aw))} -> awaitable_type<promise_type>;
        })
  auto await(Awaitable && aw)
  {
    return frame_->do_await(frame_->promise.await_transform(std::forward<Awaitable>(aw)));
  }

  template<typename Awaitable>
    requires (detail::has_await_transform<promise_type> &&
        requires (promise_type & pro, Awaitable && aw)
        {
          {pro.await_transform(std::forward<Awaitable>(aw).operator co_await())} -> awaitable_type<promise_type>;
        })
  auto await(Awaitable && aw)
  {
    return frame_->do_await(frame_->promise.await_transform(std::forward<Awaitable>(aw)).operator co_await());
  }
  template<typename Awaitable>
    requires (detail::has_await_transform<promise_type> &&
        requires (promise_type & pro, Awaitable && aw)
        {
          {operator co_await(pro.await_transform(std::forward<Awaitable>(aw)))} -> awaitable_type<promise_type>;
        })
  auto await(Awaitable && aw)
  {
    return frame_->do_await(operator co_await(frame_->promise.await_transform(std::forward<Awaitable>(aw))));
  }
  template<awaitable_type<promise_type> Awaitable>
    requires (!detail::has_await_transform<promise_type> )
  auto await(Awaitable && aw)
  {
    return frame_->do_await(std::forward<Awaitable>(aw));
  }

  template<typename Awaitable>
    requires (!detail::has_await_transform<promise_type>
          && requires (Awaitable && aw) {{operator co_await(std::forward<Awaitable>(aw))} -> awaitable_type<promise_type>;})
  auto await(Awaitable && aw)
  {
    return frame_->do_await(operator co_await(std::forward<Awaitable>(aw)));
  }

  template<typename Awaitable>
    requires (!detail::has_await_transform<promise_type>
            && requires (Awaitable && aw) {{std::forward<Awaitable>(aw).operator co_await()} -> awaitable_type<promise_type>;})
  auto await(Awaitable && aw)
  {
    return frame_->do_await(std::forward<Awaitable>(aw).operator co_await());
  }

  template<typename Yield>
    requires requires (promise_type & pro, Yield && value) {{pro.yield_value(std::forward<Yield>(value))} -> awaitable_type<promise_type>;}
  auto yield(Yield && value)
  {
    frame_->do_await(frame_->promise.yield_value(std::forward<Yield>(value)));
  }

 private:

  context(detail::context_frame<promise_type> * frame) : frame_(frame) {}
  template<typename, typename ...>
  friend struct context;

  //template<typename >
  friend struct detail::context_frame<promise_type>;

  detail::context_frame<promise_type> * frame_;
};

template<typename Return, typename ... Args, std::invocable<context<Return, Args...>, Args...> Func, typename StackAlloc>
auto make_context(Func && func, std::allocator_arg_t, StackAlloc  && salloc, Args && ... args)
{
  auto sctx_ = salloc.allocate();

  using promise_type = typename std::coroutine_traits<Return, Args...>::promise_type;
  void * p = static_cast<char*>(sctx_.sp) - sizeof(detail::context_frame<promise_type>);
  auto sz = sctx_.size - sizeof(detail::context_frame<promise_type>);

  if (auto diff = reinterpret_cast<std::uintptr_t>(p) % alignof(detail::context_frame<promise_type>); diff != 0u)
  {
    p = static_cast<char*>(p) - diff;
    sz -= diff;
  }

  boost::context::preallocated psc{p, sz, sctx_};
  auto f = new (p) detail::context_frame<promise_type>(args...);

  auto res = f->promise.get_return_object();

  constexpr auto is_always_lazy =
        requires (promise_type & pro) {{pro.initial_suspend()} -> std::same_as<std::suspend_always>;}
      && noexcept(f->promise.initial_suspend());

  struct invoker
  {
    detail::context_frame<promise_type> * frame;
    mutable Func func;
    mutable std::tuple<Args...> args;

    invoker(detail::context_frame<promise_type> * frame, Func && func, Args && ...  args)
        : frame(frame), func(std::forward<Func>(func)), args(std::forward<Args>(args)...)
    {
    }

    boost::context::fiber operator()(boost::context::fiber && f) const
    {
      auto & promise = frame->promise;
      frame->caller = std::move(f);

      try
      {
        if (!is_always_lazy)
          frame->do_await(promise.initial_suspend());

        std::apply(
            [&](auto && ... args_)
            {
              auto ctx = frame->template get_context<Return, Args...>();
              using return_type = decltype(std::forward<Func>(func)(ctx, std::forward<Args>(args_)...));

              detail::do_return(std::is_void<return_type>{}, frame->promise, ctx,
                                std::forward<Func>(func), std::forward<Args>(args_)...);
            },
            std::move(args));
      }
      catch (boost::context::detail::forced_unwind &) { throw; }
      catch (...) {promise.unhandled_exception();}

      static_assert(noexcept(promise.final_suspend()));
      frame->do_await(promise.final_suspend());
      return std::move(frame->caller);
    }

  };

  f->callee = boost::context::fiber{
    std::allocator_arg, psc, std::forward<StackAlloc>(salloc),
    invoker(f, std::forward<Func>(func), std::forward<Args>(args)...)};

  if constexpr (is_always_lazy)
    f->promise.initial_suspend();
  else
    f->resume();

  return res;
}

template<typename Return, typename ... Args, std::invocable<context<Return, Args...>, Args...> Func>
auto make_context(Func && func, Args && ... args)
{
  return make_context<Return>(std::forward<Func>(func), std::allocator_arg,
                      boost::context::fixedsize_stack(), std::forward<Args>(args)...);
}


template<typename ... Args, typename Func, typename StackAlloc>
auto make_context(Func && func, std::allocator_arg_t, StackAlloc  && salloc, Args && ... args)
{
  return make_context<typename std::tuple_element_t<0u, callable_traits::args_t<Func>>::return_type>(
      std::forward<Func>(func), std::allocator_arg, std::forward<StackAlloc>(salloc), std::forward<Args>(args)...
      );
}


template<typename ... Args, typename Func>
auto make_context(Func && func, Args && ... args)
{
  return make_context<typename std::tuple_element_t<0u, callable_traits::args_t<Func>>::return_type>(
      std::forward<Func>(func), std::forward<Args>(args)...
  );
}

}

#endif //BOOST_COBALT_EXPERIMENTAL_CONTEXT_HPP

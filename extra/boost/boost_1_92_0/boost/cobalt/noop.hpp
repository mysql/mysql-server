//
// Copyright (c) 2024 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_COBALT_NOOP_HPP
#define BOOST_COBALT_NOOP_HPP

#include <type_traits>
#include <utility>

namespace boost::cobalt
{


// tag::outline[]
// This is a tag type allowing the creation of promises or generators without creating a coroutine.
template<typename T = void>
struct noop
{
  template<typename ... Args>
  constexpr noop(Args && ... args) noexcept(std::is_nothrow_constructible_v<T, Args&&...>)
    : value(std::forward<Args>(args)...)
  {
  }
  // end::outline[]
  T value;

  constexpr static bool await_ready() {return true;}
  template<typename P>
  constexpr static void await_suspend(std::coroutine_handle<P>) {}
  constexpr T await_resume() {return std::move(value);}

  // tag::outline[]
};
// end::outline[]

template<> struct noop<void>
{
  constexpr static bool await_ready() {return true;}
  template<typename P>
  constexpr static void await_suspend(std::coroutine_handle<P>) {}
  constexpr static void await_resume() {}
};


template<typename T> noop(      T &&) -> noop<T>;
template<typename T> noop(const T & ) -> noop<T>;
noop() -> noop<void>;

}

#endif //BOOST_COBALT_NOOP_HPP

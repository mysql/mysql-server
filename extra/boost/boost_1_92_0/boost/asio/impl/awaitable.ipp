//
// impl/awaitable.ipp
// ~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2026 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_ASIO_IMPL_AWAITABLE_IPP
#define BOOST_ASIO_IMPL_AWAITABLE_IPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include <boost/asio/detail/config.hpp>

#if defined(BOOST_ASIO_HAS_CO_AWAIT)

#include <boost/asio/awaitable.hpp>
#include <boost/asio/detail/call_stack.hpp>

#include <boost/asio/detail/push_options.hpp>

namespace boost {
namespace asio {
BOOST_ASIO_INLINE_NAMESPACE_BEGIN
namespace detail {

void awaitable_launch_context::launch(void (*pump_fn)(void*), void* arg)
{
  call_stack<awaitable_launch_context>::context ctx(this);
  pump_fn(arg);
}

bool awaitable_launch_context::is_launching()
{
  return !!call_stack<awaitable_launch_context>::contains(this);
}

} // namespace detail
BOOST_ASIO_INLINE_NAMESPACE_END
} // namespace asio
} // namespace boost

#include <boost/asio/detail/pop_options.hpp>

#endif // defined(BOOST_ASIO_HAS_CO_AWAIT)

#endif // BOOST_ASIO_IMPL_AWAITABLE_IPP

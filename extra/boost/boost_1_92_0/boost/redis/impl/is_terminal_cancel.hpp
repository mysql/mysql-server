//
// Copyright (c) 2025 Marcelo Zimbres Silva (mzimbres@gmail.com),
// Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REDIS_IS_TERMINAL_CANCEL_HPP
#define BOOST_REDIS_IS_TERMINAL_CANCEL_HPP

#include <boost/asio/cancellation_type.hpp>

namespace boost::redis::detail {

constexpr bool is_terminal_cancel(asio::cancellation_type_t cancel_state)
{
   return (cancel_state & asio::cancellation_type_t::terminal) != asio::cancellation_type_t::none;
}

}  // namespace boost::redis::detail

#endif
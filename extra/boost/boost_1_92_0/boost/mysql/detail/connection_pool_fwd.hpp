//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_CONNECTION_POOL_FWD_HPP
#define BOOST_MYSQL_DETAIL_CONNECTION_POOL_FWD_HPP

#include <boost/mysql/detail/config.hpp>

#include <chrono>

namespace boost {
namespace mysql {

class pooled_connection;
class any_connection;

namespace detail {

template <class ConnectionType, class ClockType>
class basic_connection_node;

template <class ConnectionType, class ClockType, class ConnectionWrapper>
class basic_pool_impl;

using connection_node = basic_connection_node<any_connection, std::chrono::steady_clock>;
using pool_impl = basic_pool_impl<any_connection, std::chrono::steady_clock, pooled_connection>;

BOOST_MYSQL_DECL void return_connection(pool_impl& pool, connection_node& node, bool should_reset) noexcept;
BOOST_MYSQL_DECL any_connection& get_connection(connection_node& node) noexcept;

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif

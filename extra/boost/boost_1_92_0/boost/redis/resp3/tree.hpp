/* Copyright (c) 2018-2025 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef BOOST_REDIS_RESP3_TREE_HPP
#define BOOST_REDIS_RESP3_TREE_HPP

#include <boost/redis/resp3/node.hpp>

#include <vector>
#include <string_view>

namespace boost::redis::resp3 {

/// A RESP3 tree that owns its data.
template <class String, class Allocator = std::allocator<basic_node<String>>>
using basic_tree = std::vector<basic_node<String>, Allocator>;

/// A RESP3 tree that owns its data.
using tree = basic_tree<std::string>;

/// A RESP3 tree whose data are `std::string_views`.
using view_tree = basic_tree<std::string_view>;

}

#endif  // BOOST_REDIS_RESP3_RESPONSE_HPP

/* Copyright (c) 2018-2025 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef BOOST_REDIS_RESP3_NODE_HPP
#define BOOST_REDIS_RESP3_NODE_HPP

#include <boost/redis/resp3/type.hpp>

#include <cstddef>
#include <string>
#include <string_view>

namespace boost::redis::resp3 {

/** @brief A node in the response tree.
 *
 *  RESP3 can contain recursive data structures, like a map of sets of
 *  vectors. This class is called a node
 *  because it can be seen as the element of the response tree. It
 *  is a template so that users can use it with any string type, like
 *  `std::string` or `boost::static_string`.
 *
 *  @tparam String A `std::string`-like type.
 */
template <class String>
struct basic_node {
   /// The RESP3 type of the data in this node.
   type data_type = type::invalid;

   /// The number of elements of an aggregate.
   std::size_t aggregate_size{};

   /// The depth of this node in the response tree.
   std::size_t depth{};

   /// The actual data. For aggregate types this is usually empty.
   String value{};
};

/** @brief Compares a node for equality.
 *  @relates basic_node
 *
 *  @param a Left hand side node object.
 *  @param b Right hand side node object.
 */
template <class String>
bool operator==(basic_node<String> const& a, basic_node<String> const& b)
{
   // clang-format off
   return a.aggregate_size == b.aggregate_size
       && a.depth == b.depth
       && a.data_type == b.data_type
       && a.value == b.value;
   // clang-format on
};

/** @brief Inequality operator for RESP3 nodes.
 *  @relates basic_node
 *
 *  @param a Left hand side node object.
 *  @param b Right hand side node object.
 */
template <class String>
bool operator!=(basic_node<String> const& a, basic_node<String> const& b)
{
   return !(a == b);
};

/// A node in the response tree that owns its data.
using node = basic_node<std::string>;

/// A node in the response tree that does not own its data.
using node_view = basic_node<std::string_view>;

}  // namespace boost::redis::resp3

#endif  // BOOST_REDIS_RESP3_NODE_HPP

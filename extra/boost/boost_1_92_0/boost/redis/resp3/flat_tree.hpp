//
// Copyright (c) 2025 Marcelo Zimbres Silva (mzimbres@gmail.com),
// Nikolai Vladimirov (nvladimirov.work@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REDIS_RESP3_FLAT_TREE_HPP
#define BOOST_REDIS_RESP3_FLAT_TREE_HPP

#include <boost/redis/resp3/node.hpp>
#include <boost/redis/resp3/tree.hpp>

#include <boost/core/span.hpp>

#include <cstddef>
#include <iterator>
#include <memory>

namespace boost::redis {

namespace adapter::detail {
template <class> class general_aggregate;
}  // namespace adapter::detail

namespace resp3 {

namespace detail {

struct flat_buffer {
   std::unique_ptr<char[]> data;
   std::size_t size = 0u;
   std::size_t capacity = 0u;
   std::size_t reallocs = 0u;
};

}  // namespace detail

/** @brief A generic response that stores data contiguously.
 *
 * Implements a container of RESP3 nodes. It's similar to @ref boost::redis::resp3::tree,
 * but node data is stored contiguously. This allows for amortized no allocations
 * when re-using `flat_tree` objects. Like `tree`, it can contain the response
 * to several Redis commands or several server pushes. Use @ref get_total_msgs
 * to obtain how many responses this object contains.
 *
 * Objects are typically created by the user and passed to @ref connection::async_exec
 * to be used as response containers. Once populated, they can be used as a const range
 * of @ref resp3::node_view objects. The usual random access range methods (like @ref at, @ref size or
 * @ref front) are provided. Once populated, `flat_tree` can't be modified,
 * except for @ref clear and assignment.
 *
 * `flat_tree` models `std::ranges::contiguous_range`.
 *
 * A `flat_tree` is conceptually similar to a pair of `std::vector` objects, one holding
 * @ref resp3::node_view objects, and another owning the the string data that these views
 * point to. The node capacity and the data capacity are the capacities of these two vectors.
 */
class flat_tree {
public:
   /**
    * @brief The type of the iterators returned by @ref begin and @ref end.
    *
    * It is guaranteed to be a contiguous iterator. While this is currently a pointer,
    * users shouldn't rely on this fact, as the exact implementation may change between releases.
    */
   using iterator = const node_view*;

   /**
    * @brief The type of the iterators returned by @ref rbegin and @ref rend.
    *
    * As with @ref iterator, users should treat this type as an unspecified
    * contiguous iterator type rather than assuming a specific type.
    */
   using reverse_iterator = std::reverse_iterator<iterator>;

   /**
    * @brief Default constructor.
    *
    * Constructs an empty tree, with no nodes, zero node capacity and zero data capacity.
    *
    * @par Exception safety
    * No-throw guarantee.
    */
   flat_tree() = default;

   /**
    * @brief Move constructor.
    *
    * Constructs a tree by taking ownership of the nodes in `other`.
    *
    * @par Object lifetimes
    * Iterators, pointers and references to the nodes and strings in `other` remain valid.
    *
    * @par Exception safety
    * No-throw guarantee.
    */
   flat_tree(flat_tree&& other) noexcept = default;

   /**
    * @brief Copy constructor.
    *
    * Constructs a tree by copying the nodes in `other`. After the copy,
    * `*this` and `other` have independent lifetimes (usual copy semantics).
    *
    * @par Exception safety
    * Strong guarantee. Memory allocations might throw.
    */
   flat_tree(flat_tree const& other);

   /**
    * @brief Move assignment.
    *
    * Replaces the nodes in `*this` by taking ownership of the nodes in `other`.
    * `other` is left in a valid but unspecified state.
    *
    * @par Object lifetimes
    * Iterators, pointers and references to the nodes and strings in `other` remain valid.
    * Iterators, pointers and references to the nodes and strings in `*this` are invalidated.
    *
    * @par Exception safety
    * No-throw guarantee.
    */
   flat_tree& operator=(flat_tree&& other) = default;

   /**
    * @brief Copy assignment.
    *
    * Replaces the nodes in `*this` by copying the nodes in `other`.
    * After the copy, `*this` and `other` have independent lifetimes (usual copy semantics).
    *
    * @par Object lifetimes
    * Iterators, pointers and references to the nodes and strings in `*this` are invalidated.
    *
    * @par Exception safety
    * Basic guarantee. Memory allocations might throw.
    */
   flat_tree& operator=(const flat_tree& other);

   /**
    * @brief Returns an iterator to the first element of the node range.
    *
    * @par Exception safety
    * No-throw guarantee.
    *
    * @returns An iterator to the first node.
    */
   iterator begin() const noexcept { return data(); }

   /**
    * @brief Returns an iterator past the last element in the node range.
    *
    * @par Exception safety
    * No-throw guarantee.
    *
    * @returns An iterator past the last element in the node range.
    */
   iterator end() const noexcept { return data() + size(); }

   /**
    * @brief Returns an iterator to the first element of the reversed node range.
    *
    * Allows iterating the range of nodes in reverse order.
    *
    * @par Exception safety
    * No-throw guarantee.
    *
    * @returns An iterator to the first node of the reversed range.
    */
   reverse_iterator rbegin() const noexcept { return reverse_iterator{end()}; }

   /**
    * @brief Returns an iterator past the last element of the reversed node range.
    *
    * Allows iterating the range of nodes in reverse order.
    *
    * @par Exception safety
    * No-throw guarantee.
    *
    * @returns An iterator past the last element of the reversed node range.
    */
   reverse_iterator rend() const noexcept { return reverse_iterator{begin()}; }

   /**
    * @brief Returns a reference to the node at the specified position (checked access).
    *
    * @par Exception safety
    * Strong guarantee. Throws `std::out_of_range` if `i >= size()`.
    *
    * @param i Position of the node to return.
    * @returns A reference to the node at position `i`.
    */
   const node_view& at(std::size_t i) const;

   /**
    * @brief Returns a reference to the node at the specified position (unchecked access).
    *
    * @par Precondition
    * `i < size()`.
    *
    * @par Exception safety
    * No-throw guarantee.
    *
    * @param i Position of the node to return.
    * @returns A reference to the node at position `i`.
    */
   const node_view& operator[](std::size_t i) const noexcept { return get_view()[i]; }

   /**
    * @brief Returns a reference to the first node.
    *
    * @par Precondition
    * `!empty()`.
    *
    * @par Exception safety
    * No-throw guarantee.
    *
    * @returns A reference to the first node.
    */
   const node_view& front() const noexcept { return get_view().front(); }

   /**
    * @brief Returns a reference to the last node.
    *
    * @par Precondition
    * `!empty()`.
    *
    * @par Exception safety
    * No-throw guarantee.
    *
    * @returns A reference to the last node.
    */
   const node_view& back() const noexcept { return get_view().back(); }

   /**
    * @brief Returns a pointer to the underlying node storage.
    *
    * @par Exception safety
    * No-throw guarantee.
    *
    * @returns A pointer to the underlying node array.
    */
   const node_view* data() const noexcept { return view_tree_.data(); }

   /**
    * @brief Checks whether the tree is empty.
    *
    * @par Exception safety
    * No-throw guarantee.
    *
    * @returns `true` if the tree contains no nodes, `false` otherwise.
    */
   bool empty() const noexcept { return size() == 0u; }

   /**
    * @brief Returns the number of nodes in the tree.
    *
    * @par Exception safety
    * No-throw guarantee.
    *
    * @returns The number of nodes.
    */
   std::size_t size() const noexcept { return node_tmp_offset_; }

   /** @brief Reserves capacity for incoming data.
    *
    * Adding nodes (e.g. by passing the tree to `async_exec`)
    * won't cause reallocations until the data or node capacities
    * are exceeded, following the usual vector semantics.
    * The implementation might reserve more capacity than the one requested.
    * 
    * @par Object lifetimes
    * References to the nodes and strings in `*this` are invalidated.
    *
    * @par Exception safety
    * Basic guarantee. Memory allocations might throw.
    *
    * @param bytes Number of bytes to reserve for data.
    * @param nodes Number of nodes to reserve.
    */
   void reserve(std::size_t bytes, std::size_t nodes);

   /** @brief Clears the tree so it contains no nodes.
    * 
    * Calling this function removes every node, making
    * the range contain no nodes, and @ref get_total_msgs
    * return zero. It does not modify the object's capacity.
    * 
    * To re-use a `flat_tree` for several requests,
    * use `clear()` before each `async_exec` call.
    *
    * @par Object lifetimes
    * References to the nodes and strings in `*this` are invalidated.
    *
    * @par Exception safety
    * No-throw guarantee.
    */
   void clear() noexcept;

   /** @brief Returns the size of the data buffer, in bytes.
    * 
    * You may use this function to calculate how much capacity
    * should be reserved for data when calling @ref reserve.
    *
    * @par Exception safety
    * No-throw guarantee.
    *
    * @returns The number of bytes in use in the data buffer.
    */
   auto data_size() const noexcept -> std::size_t { return data_tmp_offset_; }

   /** @brief Returns the capacity of the node container.
    *
    * @par Exception safety
    * No-throw guarantee. 
    *
    * @returns The capacity of the object, in number of nodes.
    */
   auto capacity() const noexcept -> std::size_t { return view_tree_.capacity(); }

   /** @brief Returns the capacity of the data buffer, in bytes.
    *
    * Note that the actual capacity of the data buffer may be bigger
    * than the one requested by @ref reserve.
    *
    * @par Exception safety
    * No-throw guarantee. 
    *
    * @returns The capacity of the data buffer, in bytes.
    */
   auto data_capacity() const noexcept -> std::size_t { return data_.capacity; }

   /** @brief Returns the number of memory reallocations that took place in the data buffer.
    *
    * This function returns how many reallocations in the data buffer were performed and
    * can be useful to determine how much memory to reserve upfront.
    * 
    * @par Exception safety
    * No-throw guarantee.
    *
    * @returns The number of times that the data buffer reallocated its memory.
    */
   auto get_reallocs() const noexcept -> std::size_t { return data_.reallocs; }

   /** @brief Returns the number of complete RESP3 messages contained in this object.
    *
    * This value is equal to the number of nodes in the tree with a depth of zero.
    *
    * @par Exception safety
    * No-throw guarantee.
    *
    * @returns The number of complete RESP3 messages contained in this object.
    */
   std::size_t get_total_msgs() const noexcept { return total_msgs_; }

private:
   template <class> friend class adapter::detail::general_aggregate;

   span<const node_view> get_view() const noexcept { return {data(), size()}; }
   void notify_init();
   void notify_done();

   // Push a new node to the response
   void push(node_view const& node);

   detail::flat_buffer data_;
   view_tree view_tree_;
   std::size_t total_msgs_ = 0u;

   // flat_tree supports a "temporary working area" for incrementally reading messages.
   // Nodes in the tmp area are not part of the object representation until they
   // are committed with notify_done().
   // These offsets delimit this area.
   std::size_t node_tmp_offset_ = 0u;
   std::size_t data_tmp_offset_ = 0u;
};

/**
 * @brief Equality operator.
 * @relates flat_tree
 *
 * Two trees are equal if they contain the same nodes in the same order.
 * Capacities are not taken into account.
 *
 * @par Exception safety
 * No-throw guarantee.
 */
bool operator==(flat_tree const&, flat_tree const&);

/**
 * @brief Inequality operator.
 * @relates flat_tree
 *
 * @par Exception safety
 * No-throw guarantee.
 */
inline bool operator!=(flat_tree const& lhs, flat_tree const& rhs) { return !(lhs == rhs); }

}  // namespace resp3
}  // namespace boost::redis

#endif  // BOOST_REDIS_RESP3_FLAT_TREE_HPP

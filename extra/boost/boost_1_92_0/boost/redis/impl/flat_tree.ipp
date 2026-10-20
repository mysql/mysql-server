//
// Copyright (c) 2025 Marcelo Zimbres Silva (mzimbres@gmail.com),
// Nikolai Vladimirov (nvladimirov.work@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/redis/resp3/flat_tree.hpp>
#include <boost/redis/resp3/node.hpp>
#include <boost/redis/resp3/tree.hpp>

#include <boost/assert.hpp>
#include <boost/throw_exception.hpp>

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <stdexcept>
#include <string_view>

namespace boost::redis::resp3 {

namespace detail {

// Updates string views by performing pointer arithmetic
inline void rebase_strings(view_tree& nodes, const char* old_base, const char* new_base)
{
   for (auto& nd : nodes) {
      if (!nd.value.empty()) {
         const auto offset = nd.value.data() - old_base;
         BOOST_ASSERT(offset >= 0);
         nd.value = {new_base + offset, nd.value.size()};
      }
   }
}

// --- Operations in flat_buffer ---

// Compute the new capacity upon reallocation. We always use powers of 2,
// starting in 512, to prevent many small allocations
inline std::size_t compute_capacity(std::size_t current, std::size_t requested)
{
   std::size_t res = (std::max)(current, static_cast<std::size_t>(512u));
   while (res < requested)
      res *= 2u;
   return res;
}

// Copy construction
inline flat_buffer copy_construct(const flat_buffer& other)
{
   flat_buffer res{{}, other.size, 0u, 0u};

   if (other.size > 0u) {
      const std::size_t capacity = compute_capacity(0u, other.size);
      res.data.reset(new char[capacity]);
      res.capacity = capacity;
      res.reallocs = 1u;
      std::copy(other.data.get(), other.data.get() + other.size, res.data.get());
   }

   return res;
}

// Copy assignment
inline void copy_assign(flat_buffer& buff, const flat_buffer& other)
{
   // Make space if required
   if (buff.capacity < other.size) {
      const std::size_t capacity = compute_capacity(buff.capacity, other.size);
      buff.data.reset(new char[capacity]);
      buff.capacity = capacity;
      ++buff.reallocs;
   }

   // Copy the contents
   std::copy(other.data.get(), other.data.get() + other.size, buff.data.get());
   buff.size = other.size;
}

// Grows the buffer until reaching a target size.
// Might rebase the strings in nodes
inline void grow(flat_buffer& buff, std::size_t new_capacity, view_tree& nodes)
{
   if (new_capacity <= buff.capacity)
      return;

   // Compute the actual capacity that we will be using
   new_capacity = compute_capacity(buff.capacity, new_capacity);

   // Allocate space
   std::unique_ptr<char[]> new_buffer{new char[new_capacity]};

   // Copy any data into the newly allocated space
   const char* data_before = buff.data.get();
   char* data_after = new_buffer.get();
   std::copy(data_before, data_before + buff.size, data_after);

   // Update the string views so they don't dangle
   rebase_strings(nodes, data_before, data_after);

   // Replace the buffer. Note that size hasn't changed here
   buff.data = std::move(new_buffer);
   buff.capacity = new_capacity;
   ++buff.reallocs;
}

// Erases the first num_bytes bytes from the buffer by moving
// the remaining bytes forward. Rebases the strings in nodes as required.
inline void erase_first(flat_buffer& buff, std::size_t num_bytes, view_tree& nodes)
{
   BOOST_ASSERT(num_bytes <= buff.size);
   if (num_bytes > 0u) {
      // If we have any data to move, we should always have a buffer
      BOOST_ASSERT(buff.data.get() != nullptr);

      // Record the old base
      const char* old_base = buff.data.get() + num_bytes;

      // Move all that we're gonna keep to the start of the buffer
      auto bytes_left = buff.size - num_bytes;
      std::memmove(buff.data.get(), old_base, bytes_left);

      // Rebase strings
      rebase_strings(nodes, old_base, buff.data.get());
   }
}

// Appends a string to the buffer.
// Might rebase the string in nodes, but doesn't append any new node.
inline std::string_view append(flat_buffer& buff, std::string_view value, view_tree& nodes)
{
   // If there is nothing to copy, do nothing
   if (value.empty())
      return value;

   // Make space for the new string
   const std::size_t new_size = buff.size + value.size();
   grow(buff, new_size, nodes);

   // Copy the new value
   const std::size_t offset = buff.size;
   std::copy(value.data(), value.data() + value.size(), buff.data.get() + offset);
   buff.size = new_size;
   return {buff.data.get() + offset, value.size()};
}

}  // namespace detail

flat_tree::flat_tree(flat_tree const& other)
: data_{detail::copy_construct(other.data_)}
, view_tree_{other.view_tree_}
, total_msgs_{other.total_msgs_}
, node_tmp_offset_{other.node_tmp_offset_}
, data_tmp_offset_{other.data_tmp_offset_}
{
   detail::rebase_strings(view_tree_, other.data_.data.get(), data_.data.get());
}

flat_tree& flat_tree::operator=(const flat_tree& other)
{
   if (this != &other) {
      // Copy the data
      detail::copy_assign(data_, other.data_);

      // Copy the nodes
      view_tree_ = other.view_tree_;
      detail::rebase_strings(view_tree_, other.data_.data.get(), data_.data.get());

      // Copy the other fields
      total_msgs_ = other.total_msgs_;
      node_tmp_offset_ = other.node_tmp_offset_;
      data_tmp_offset_ = other.data_tmp_offset_;
   }

   return *this;
}

void flat_tree::reserve(std::size_t bytes, std::size_t nodes)
{
   // Space for the strings
   detail::grow(data_, bytes, view_tree_);

   // Space for the nodes
   view_tree_.reserve(nodes);
}

void flat_tree::clear() noexcept
{
   // Discard everything except for the tmp area
   view_tree_.erase(view_tree_.begin(), view_tree_.begin() + node_tmp_offset_);
   node_tmp_offset_ = 0u;

   // Do the same for the data area
   detail::erase_first(data_, data_tmp_offset_, view_tree_);
   data_tmp_offset_ = 0u;

   // We now have no messages
   total_msgs_ = 0u;
}

void flat_tree::push(node_view const& nd)
{
   // Add the string
   const std::string_view str = detail::append(data_, nd.value, view_tree_);

   // Add the node
   view_tree_.push_back({
      nd.data_type,
      nd.aggregate_size,
      nd.depth,
      str,
   });
}

void flat_tree::notify_init()
{
   // Discard any data in the tmp area, as it belongs to an operation that never finished
   BOOST_ASSERT(node_tmp_offset_ <= view_tree_.size());
   BOOST_ASSERT(data_tmp_offset_ <= data_.size);
   view_tree_.resize(node_tmp_offset_);
   data_.size = data_tmp_offset_;
}

void flat_tree::notify_done()
{
   ++total_msgs_;
   node_tmp_offset_ = view_tree_.size();
   data_tmp_offset_ = data_.size;
}

const node_view& flat_tree::at(std::size_t i) const
{
   if (i >= size())
      BOOST_THROW_EXCEPTION(std::out_of_range("flat_tree::at"));
   return view_tree_[i];
}

bool operator==(flat_tree const& a, flat_tree const& b)
{
   // data is already taken into account by comparing the nodes.
   // Only committed nodes should be taken into account.
   return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin()) &&
          a.get_total_msgs() == b.get_total_msgs();
}

}  // namespace boost::redis::resp3

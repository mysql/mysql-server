/* Copyright (c) 2018-2025 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <boost/redis/detail/read_buffer.hpp>

#include <boost/assert.hpp>
#include <boost/core/make_span.hpp>

#include <utility>

namespace boost::redis::detail {

read_buffer::read_buffer(config cfg) noexcept
: cfg_{cfg}
{
}

bool read_buffer::needs_rotation(std::size_t append_size) const noexcept
{
   if (consumed_ == 0u)
     return false;

   // If preparing would cause the capacity to have to be increased we first
   // discard already consumed data to perhaps avoid the reallocation.
   auto const capacity = buffer_.capacity();
   auto const size = buffer_.size();
   auto const remaining = capacity - size;
   if (remaining < append_size)
     return true;

   // Rotate if increasing the buffer size would require more than max.
   return buffer_.size() > (cfg_.max_size - append_size);
}

read_buffer::prepare_result read_buffer::prepare(std::size_t append_size)
{
   BOOST_ASSERT(prepared_begin_ == buffer_.size());

   if (append_size > cfg_.max_size) {
      return {0u, error::exceeds_maximum_read_buffer_size};
   }

   std::size_t rotated = 0u;
   if (needs_rotation(append_size)) {
      BOOST_ASSERT(consumed_ != 0u);
      buffer_.erase(buffer_.begin(), buffer_.begin() + consumed_);
      rotated = buffer_.size();

      BOOST_ASSERT(consumed_ <= prepared_begin_);
      prepared_begin_ -= consumed_;
      consumed_ = 0u;
   }

   auto const new_size = prepared_begin_ + append_size;

   if (new_size > cfg_.max_size) {
      return {rotated, error::exceeds_maximum_read_buffer_size};
   }

   {
      auto const start_capacity = buffer_.capacity();
      auto const start_size = buffer_.size();
      buffer_.resize(new_size);
      auto const end_capacity = buffer_.capacity();
      if (end_capacity > start_capacity)
         rotated += start_size;
   }

   return {rotated, {}};
}

void read_buffer::commit(std::size_t read_size)
{
   BOOST_ASSERT(buffer_.size() >= (prepared_begin_ + read_size));
   buffer_.resize(prepared_begin_ + read_size);
   prepared_begin_ = buffer_.size();
}

auto read_buffer::get_prepared() noexcept -> span_type
{
   auto const size = buffer_.size();
   return make_span(buffer_.data() + prepared_begin_, size - prepared_begin_);
}

auto read_buffer::get_commited() const noexcept -> std::string_view
{
   return {buffer_.data() + consumed_, prepared_begin_ - consumed_};
}

void read_buffer::clear()
{
   buffer_.clear();
   consumed_ = 0;
   prepared_begin_ = 0;
}

std::size_t read_buffer::consume(std::size_t n)
{
   // For convenience, if the requested size is larger than the
   // committed buffer we cap it to the maximum.
   auto const consumable = prepared_begin_ - consumed_;
   if (n > consumable)
      n = consumable;

   consumed_ += n;
   BOOST_ASSERT(consumed_ <= prepared_begin_);
   return n;
}

void read_buffer::reserve(std::size_t n) { buffer_.reserve(n); }

bool operator==(read_buffer const& lhs, read_buffer const& rhs)
{
   return lhs.buffer_ == rhs.buffer_ && lhs.prepared_begin_ == rhs.prepared_begin_;
}

bool operator!=(read_buffer const& lhs, read_buffer const& rhs) { return !(lhs == rhs); }

}  // namespace boost::redis::detail

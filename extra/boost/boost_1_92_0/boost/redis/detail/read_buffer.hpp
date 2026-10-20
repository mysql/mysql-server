/* Copyright (c) 2018-2026 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef BOOST_REDIS_READ_BUFFER_HPP
#define BOOST_REDIS_READ_BUFFER_HPP

#include <boost/core/span.hpp>
#include <boost/system/error_code.hpp>

#include <cstddef>
#include <string_view>
#include <utility>
#include <vector>

namespace boost::redis::detail {

// Buffer class used in read operations that implements lazy rotations.  It is
// split in three main parts
//
//    1. Consumed: Bytes that can be discarded but haven't to avoid rotating
//       the buffer unnecessarily.
//
//    2. Commited: Area with data that is in use by the client code.
//
//    3. Prepared: Area waiting for data to be copied into it.
//
// The dynamics of the read-buffer is exemplified below
//
//    1. The buffer is empty and app start
//
//       ||
//
//    2. Client code calls "prepare" to reserve n bytes at the end of the
//       buffer
//
//       |+++++++++++++++++|
//
//    3. Bytes are read from the socket into that area and commited with the
//       commit member function
//
//       |-----------|
//
//    4. The steps above are repeated until the amount of data needed by the
//       application is reached
//
//       |-----------|
//       |-----------+++++++++++++++++| Prepare
//       |-----------------| Commit
//       |-----------------++++++++++++++++| Prepare
//       |---------------------------| Commit
//       ...
//
//    5. Commited bytes are processed by the client and consumed. The consume
//       op won't discard any data but increase an offset instead
//
//       |============---------------| Consume
//
//    6. If preparing would cause the capacity to have to be increased we first
//       discard already consumed data to perhaps avoid the reallocation
//
//       |---------------| After the rotation
//       |---------------++++++++++++++++| When prepare returns
//
// Buffer rotations can in principle be implemented both in the consume and in
// the prepare operation, the latter however produces better results because
// Redis commands are often very small e.g. "+OK\r\n" and a single read from
// the socket might bring in multiple responses, for example
//
//                |    Contains 100 responses     |
//                |                               |
//       |=========-------------------------------|
//
// Consuming each response one by one would produce
//
//       |=========-------------------------------|
//       |===========-----------------------------|
//       |=============---------------------------|
//       |===============-------------------------|
//       |=================-----------------------|
//       |===================---------------------|
//
// When a prepare call comes we would like the consumed bytes to be zero so no
// reallocation must be performed, that can only be implemented in the consume
// op if it rotates eagerly i.e. on every consume call, something we don't want.

class read_buffer {
public:
   using span_type = span<char>;

   struct prepare_result {
      std::size_t rotated;
      system::error_code ec;
   };

   // See config.hpp for the meaning of these parameters.
   struct config {
      std::size_t max_size = static_cast<std::size_t>(-1);
   };

   read_buffer() noexcept = default;
   read_buffer(config cfg) noexcept;

   // Prepare the buffer to receive more data.
   [[nodiscard]]
   auto prepare(std::size_t append_size) -> prepare_result;

   [[nodiscard]]
   auto get_prepared() noexcept -> span_type;

   void commit(std::size_t read_size);

   [[nodiscard]]
   auto get_commited() const noexcept -> std::string_view;

   void clear();

   auto consume(std::size_t n) -> std::size_t;

   void reserve(std::size_t n);

   friend bool operator==(read_buffer const& lhs, read_buffer const& rhs);

   friend bool operator!=(read_buffer const& lhs, read_buffer const& rhs);

   void set_config(config const& cfg) noexcept { cfg_ = cfg; };

   // Returns the total size: consumed + commited + prepared.
   std::size_t size() const noexcept
     { return buffer_.size(); }

   std::size_t capacity() const noexcept
     { return buffer_.capacity(); }

private:
   bool needs_rotation(std::size_t append_size) const noexcept;

   config cfg_ = config{};
   std::vector<char> buffer_;
   std::size_t consumed_ = 0;
   std::size_t prepared_begin_ = 0;
};

}  // namespace boost::redis::detail

#endif  // BOOST_REDIS_READ_BUFFER_HPP

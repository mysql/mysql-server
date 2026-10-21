/* Copyright (c) 2018-2024 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef BOOST_REDIS_USAGE_HPP
#define BOOST_REDIS_USAGE_HPP

#include <cstddef>

namespace boost::redis {

/** @brief Connection usage information.
 *
 *  @note To simplify the implementation, @ref commands_sent and
 *  @ref bytes_sent are computed just before writing to
 *  the socket. On error, they might not represent exactly
 *  what has been received by the Redis server.
 */
struct usage {
   /// Number of commands sent.
   std::size_t commands_sent = 0;

   /// Number of bytes sent.
   std::size_t bytes_sent = 0;

   /// Number of responses received.
   std::size_t responses_received = 0;

   /// Number of pushes received.
   std::size_t pushes_received = 0;

   /// Number of response-bytes received.
   std::size_t response_bytes_received = 0;

   /// Number of push-bytes received.
   std::size_t push_bytes_received = 0;

   /// Number of bytes rotated in the read buffer.
   std::size_t bytes_rotated = 0;
};

}  // namespace boost::redis

#endif  // BOOST_REDIS_USAGE_HPP

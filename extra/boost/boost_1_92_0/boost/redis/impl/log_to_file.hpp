//
// Copyright (c) 2025 Marcelo Zimbres Silva (mzimbres@gmail.com),
// Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REDIS_LOG_TO_STDERR_HPP
#define BOOST_REDIS_LOG_TO_STDERR_HPP

#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <string_view>

namespace boost::redis::detail {

// Shared by several ipp files
inline void log_to_file(FILE* f, std::string_view msg, const char* prefix = "(Boost.Redis) ")
{
   // If the message is empty, data() might return a null pointer
   const char* msg_ptr = msg.empty() ? "" : msg.data();

   // Precision should be an int when passed to fprintf. Technically,
   // message could be larger than INT_MAX. Impose a sane limit on message sizes
   // to prevent memory problems
   auto precision = static_cast<int>((std::min)(msg.size(), static_cast<std::size_t>(0xffffu)));

   // Log the message. None of our messages should contain NULL bytes, so this should be OK.
   // We choose fprintf over std::clog because it's safe in multi-threaded environments.
   std::fprintf(f, "%s%.*s\n", prefix, precision, msg_ptr);
}

}  // namespace boost::redis::detail

#endif
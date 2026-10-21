//
// Copyright (c) 2025 Marcelo Zimbres Silva (mzimbres@gmail.com),
// Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/redis/impl/log_to_file.hpp>
#include <boost/redis/logger.hpp>

#include <cstdio>
#include <string_view>

namespace boost::redis {

logger::logger(level l)
: lvl{l}
, fn{[](level, std::string_view msg) {
   detail::log_to_file(stderr, msg);
}}
{ }

}  // namespace boost::redis

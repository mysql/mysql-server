//
// Copyright (c) 2025 Marcelo Zimbres Silva (mzimbres@gmail.com),
// Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REDIS_SUBSCRIPTION_TRACKER_HPP
#define BOOST_REDIS_SUBSCRIPTION_TRACKER_HPP

#include <set>
#include <string>

namespace boost::redis {

class request;

namespace detail {

class subscription_tracker {
   std::set<std::string> channels_;
   std::set<std::string> pchannels_;

public:
   subscription_tracker() = default;
   void clear();
   void commit_changes(const request& req);
   void compose_subscribe_request(request& to) const;
};

}  // namespace detail
}  // namespace boost::redis

#endif

//
// Copyright (c) 2025 Marcelo Zimbres Silva (mzimbres@gmail.com),
// Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/redis/detail/subscription_tracker.hpp>
#include <boost/redis/request.hpp>

#include <boost/assert.hpp>

#include <string>

namespace boost::redis::detail {

void subscription_tracker::clear()
{
   channels_.clear();
   pchannels_.clear();
}

void subscription_tracker::commit_changes(const request& req)
{
   for (const auto& ch : request_access::pubsub_changes(req)) {
      std::string channel{req.payload().substr(ch.channel_offset, ch.channel_size)};
      switch (ch.type) {
         case pubsub_change_type::subscribe:    channels_.insert(std::move(channel)); break;
         case pubsub_change_type::unsubscribe:  channels_.erase(std::move(channel)); break;
         case pubsub_change_type::psubscribe:   pchannels_.insert(std::move(channel)); break;
         case pubsub_change_type::punsubscribe: pchannels_.erase(std::move(channel)); break;
         default:                               BOOST_ASSERT(false);
      }
   }
}

void subscription_tracker::compose_subscribe_request(request& to) const
{
   to.push_range("SUBSCRIBE", channels_);
   to.push_range("PSUBSCRIBE", pchannels_);
}

}  // namespace boost::redis::detail

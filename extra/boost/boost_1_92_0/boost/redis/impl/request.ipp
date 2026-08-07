/* Copyright (c) 2018-2024 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <boost/redis/request.hpp>
#include <boost/redis/resp3/serialization.hpp>

#include <cstddef>
#include <string_view>

namespace boost::redis::detail {

auto has_response(std::string_view cmd) -> bool
{
   if (cmd == "SUBSCRIBE")
      return true;
   if (cmd == "PSUBSCRIBE")
      return true;
   if (cmd == "UNSUBSCRIBE")
      return true;
   if (cmd == "PUNSUBSCRIBE")
      return true;
   return false;
}

request make_hello_request()
{
   request req;
   req.hello();
   return req;
}

}  // namespace boost::redis::detail

namespace boost::redis {

void request::append(const request& other)
{
   // Remember the old payload size, to update offsets
   std::size_t old_offset = payload_.size();

   // Add the payload
   payload_ += other.payload_;
   commands_ += other.commands_;
   expected_responses_ += other.expected_responses_;

   // Add the pubsub changes. Offsets need to be updated
   pubsub_changes_.reserve(pubsub_changes_.size() + other.pubsub_changes_.size());
   for (const auto& change : other.pubsub_changes_) {
      pubsub_changes_.push_back({
         change.type,
         change.channel_offset + old_offset,
         change.channel_size,
      });
   }
}

void request::add_pubsub_arg(detail::pubsub_change_type type, std::string_view value)
{
   // Add the argument
   resp3::add_bulk(payload_, value);

   // Track the change.
   // The final \r\n adds 2 bytes
   std::size_t offset = payload_.size() - value.size() - 2u;
   pubsub_changes_.push_back({type, offset, value.size()});
}

void request::hello() { push("HELLO", "3"); }

void request::hello(std::string_view username, std::string_view password)
{
   push("HELLO", "3", "AUTH", username, password);
}

void request::hello_setname(std::string_view client_name)
{
   push("HELLO", "3", "SETNAME", client_name);
}

void request::hello_setname(
   std::string_view username,
   std::string_view password,
   std::string_view client_name)
{
   push("HELLO", "3", "AUTH", username, password, "SETNAME", client_name);
}

}  // namespace boost::redis

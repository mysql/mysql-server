//
// Copyright (c) 2026 Marcelo Zimbres Silva (mzimbres@gmail.com),
// Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/redis/push_parser.hpp>
#include <boost/redis/resp3/type.hpp>

#include <algorithm>
#include <optional>
#include <string_view>

namespace boost::redis {

namespace detail {

// Advances the iterator past all nodes belonging to the current root message.
// Returns a pointer to the next root node (depth == 0) or end.
inline const resp3::node_view* skip_current_message(
   const resp3::node_view* current,
   const resp3::node_view* end)
{
   return std::find_if(current, end, [](const resp3::node_view& n) {
      return n.depth == 0u;
   });
}

// Attempts to parse a push message starting at 'current'.
// If successful, populates 'msg' and returns true.
// If not a valid pubsub message (message/pmessage), returns false.
inline bool try_parse_push_view(
   const resp3::node_view*& current,
   const resp3::node_view* end,
   push_view& msg)
{
   if (current == end)
      return false;

   // Root must be a push type
   if (current->data_type != resp3::type::push || current->depth != 0u) {
      current = skip_current_message(current + 1, end);
      return false;
   }

   // Move to first child (message type)
   ++current;
   if (current == end || current->depth != 1u || current->data_type != resp3::type::blob_string) {
      current = skip_current_message(current, end);
      return false;
   }

   // Determine the message type
   bool is_pmessage;
   if (current->value == "message") {
      is_pmessage = false;
   } else if (current->value == "pmessage") {
      is_pmessage = true;
   } else {
      // Not interested in this message
      current = skip_current_message(current, end);
      return false;
   }

   // For pmessage, the matched pattern goes next
   if (is_pmessage) {
      ++current;
      if (
         current == end || current->depth != 1u || current->data_type != resp3::type::blob_string) {
         current = skip_current_message(current, end);
         return false;
      }
      msg.pattern = current->value;
   } else {
      msg.pattern = std::nullopt;
   }

   // Channel
   ++current;
   if (current == end || current->depth != 1 || current->data_type != resp3::type::blob_string) {
      current = skip_current_message(current, end);
      return false;
   }
   msg.channel = current->value;

   // Payload
   ++current;
   if (current == end || current->depth != 1 || current->data_type != resp3::type::blob_string) {
      current = skip_current_message(current, end);
      return false;
   }
   msg.payload = current->value;

   // We're done. We should be at the end of the message
   ++current;
   if (current != end && current->depth != 0u) {
      current = skip_current_message(current, end);
      return false;
   }

   return true;
}

}  // namespace detail

void push_parser::advance() noexcept
{
   while (first_ != last_) {
      if (detail::try_parse_push_view(first_, last_, current_)) {
         return;
      }
   }
   done_ = true;
}

}  // namespace boost::redis

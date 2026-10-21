//
// Copyright (c) 2018-2026 Marcelo Zimbres Silva (mzimbres@gmail.com),
// Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REDIS_RECEIVE_FSM_HPP
#define BOOST_REDIS_RECEIVE_FSM_HPP

#include <boost/asio/cancellation_type.hpp>
#include <boost/system/error_code.hpp>

// Sans-io algorithm for async_receive2, as a finite state machine

namespace boost::redis::detail {

struct connection_state;

struct receive_action {
   enum class action_type
   {
      setup_cancellation,  // Set up the cancellation types supported by the composed operation
      wait,                // Wait for a message to appear in the receive channel
      drain_channel,       // Empty the receive channel
      immediate,           // Call async_immediate
      done,                // Complete
   };

   action_type type;
   system::error_code ec;

   receive_action(action_type type) noexcept
   : type{type}
   { }

   receive_action(system::error_code ec) noexcept
   : type{action_type::done}
   , ec{ec}
   { }
};

class receive_fsm {
   int resume_point_{0};

public:
   receive_fsm() = default;

   receive_action resume(
      connection_state& st,
      system::error_code ec,
      asio::cancellation_type_t cancel_state);
};

}  // namespace boost::redis::detail

#endif

//
// Copyright (c) 2025 Marcelo Zimbres Silva (mzimbres@gmail.com),
// Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REDIS_RUN_FSM_HPP
#define BOOST_REDIS_RUN_FSM_HPP

#include <boost/redis/detail/connect_params.hpp>

#include <boost/asio/cancellation_type.hpp>
#include <boost/system/error_code.hpp>

// Sans-io algorithm for async_run, as a finite state machine

namespace boost::redis::detail {

// Forward decls
struct connection_state;

// What should we do next?
enum class run_action_type
{
   done,                   // Call the final handler
   immediate,              // Call asio::async_immediate
   connect,                // Transport connection establishment
   sentinel_resolve,       // Contact Sentinels to resolve the master's address
   parallel_group,         // Run the reader, writer and friends
   cancel_receive,         // Cancel the receiver channel
   wait_for_reconnection,  // Sleep for the reconnection period
};

struct run_action {
   run_action_type type;
   system::error_code ec;

   run_action(run_action_type type) noexcept
   : type{type}
   { }

   run_action(system::error_code ec) noexcept
   : type{run_action_type::done}
   , ec{ec}
   { }
};

class run_fsm {
   int resume_point_{0};
   system::error_code stored_ec_;

public:
   run_fsm() = default;

   run_action resume(
      connection_state& st,
      system::error_code ec,
      asio::cancellation_type_t cancel_state);
};

connect_params make_run_connect_params(const connection_state& st);

}  // namespace boost::redis::detail

#endif  // BOOST_REDIS_CONNECTOR_HPP

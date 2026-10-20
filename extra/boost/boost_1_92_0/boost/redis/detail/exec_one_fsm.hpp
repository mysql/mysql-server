//
// Copyright (c) 2025 Marcelo Zimbres Silva (mzimbres@gmail.com),
// Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REDIS_EXEC_ONE_FSM_HPP
#define BOOST_REDIS_EXEC_ONE_FSM_HPP

#include <boost/redis/adapter/any_adapter.hpp>
#include <boost/redis/resp3/parser.hpp>

#include <boost/asio/cancellation_type.hpp>
#include <boost/system/error_code.hpp>

#include <cstddef>

// Sans-io algorithm for async_exec_one, as a finite state machine

namespace boost::redis::detail {

class multiplexer;

// What should we do next?
enum class exec_one_action_type
{
   done,       // Call the final handler
   write,      // Write the request
   read_some,  // Read into the read buffer
};

struct exec_one_action {
   exec_one_action_type type;
   system::error_code ec;

   exec_one_action(exec_one_action_type type) noexcept
   : type{type}
   { }

   exec_one_action(system::error_code ec) noexcept
   : type{exec_one_action_type::done}
   , ec{ec}
   { }
};

class exec_one_fsm {
   int resume_point_{0};
   any_adapter adapter_;
   std::size_t remaining_responses_;
   resp3::parser parser_;

public:
   exec_one_fsm(any_adapter resp, std::size_t expected_responses)
   : adapter_(std::move(resp))
   , remaining_responses_(expected_responses)
   { }

   // Instead of using the read_buffer directly we use its facade in the
   // multiplexer because it keeps track of usage information.
   exec_one_action resume(
      multiplexer& mpx,
      system::error_code ec,
      std::size_t bytes_transferred,
      asio::cancellation_type_t cancel_state);
};

}  // namespace boost::redis::detail

#endif  // BOOST_REDIS_CONNECTOR_HPP

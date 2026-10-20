//
// Copyright (c) 2025 Marcelo Zimbres Silva (mzimbres@gmail.com),
// Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REDIS_EXEC_FSM_HPP
#define BOOST_REDIS_EXEC_FSM_HPP

#include <boost/redis/detail/multiplexer.hpp>

#include <boost/asio/cancellation_type.hpp>
#include <boost/system/error_code.hpp>

#include <cstddef>
#include <memory>

// Sans-io algorithm for async_exec, as a finite state machine

namespace boost::redis::detail {

struct connection_state;

// What should we do next?
enum class exec_action_type
{
   setup_cancellation,  // Set up the cancellation types supported by the composed operation
   immediate,           // Invoke asio::async_immediate to avoid re-entrancy problems
   done,                // Call the final handler
   notify_writer,       // Notify the writer task
   wait_for_response,   // Wait to be notified
};

class exec_action {
   exec_action_type type_;
   system::error_code ec_;
   std::size_t bytes_read_;

public:
   exec_action(exec_action_type type) noexcept
   : type_{type}
   { }

   exec_action(system::error_code ec, std::size_t bytes_read = 0u) noexcept
   : type_{exec_action_type::done}
   , ec_{ec}
   , bytes_read_{bytes_read}
   { }

   exec_action_type type() const { return type_; }
   system::error_code error() const { return ec_; }
   std::size_t bytes_read() const { return bytes_read_; }
};

class exec_fsm {
   int resume_point_{0};
   std::shared_ptr<multiplexer::elem> elem_;

public:
   exec_fsm(std::shared_ptr<multiplexer::elem> elem) noexcept
   : elem_(std::move(elem))
   { }

   exec_action resume(
      bool connection_is_open,
      connection_state& st,
      asio::cancellation_type_t cancel_state);
};

}  // namespace boost::redis::detail

#endif  // BOOST_REDIS_CONNECTOR_HPP

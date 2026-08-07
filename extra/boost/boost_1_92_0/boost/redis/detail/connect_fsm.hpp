//
// Copyright (c) 2025 Marcelo Zimbres Silva (mzimbres@gmail.com),
// Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REDIS_CONNECT_FSM_HPP
#define BOOST_REDIS_CONNECT_FSM_HPP

#include <boost/asio/cancellation_type.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/system/error_code.hpp>

// Sans-io algorithm for redis_stream::async_connect, as a finite state machine

namespace boost::redis::detail {

struct buffered_logger;

// What transport is redis_stream using?
enum class transport_type
{
   tcp,          // plaintext TCP
   tcp_tls,      // TLS over TCP
   unix_socket,  // UNIX domain sockets
};

struct redis_stream_state {
   transport_type type{transport_type::tcp};
   bool ssl_stream_used{false};
};

// What should we do next?
enum class connect_action_type
{
   unix_socket_close,    // Close the UNIX socket, to discard state
   unix_socket_connect,  // Connect to the UNIX socket
   tcp_resolve,          // Name resolution
   tcp_connect,          // TCP connect
   ssl_stream_reset,     // Re-create the SSL stream, to discard state
   ssl_handshake,        // SSL handshake
   done,                 // Complete the async op
};

struct connect_action {
   connect_action_type type;
   system::error_code ec;

   connect_action(connect_action_type type) noexcept
   : type{type}
   { }

   connect_action(system::error_code ec) noexcept
   : type{connect_action_type::done}
   , ec{ec}
   { }
};

class connect_fsm {
   int resume_point_{0};
   buffered_logger* lgr_{nullptr};

public:
   connect_fsm(buffered_logger& lgr) noexcept
   : lgr_(&lgr)
   { }

   connect_action resume(
      system::error_code ec,
      const asio::ip::tcp::resolver::results_type& resolver_results,
      redis_stream_state& st,
      asio::cancellation_type_t cancel_state);

   connect_action resume(
      system::error_code ec,
      const asio::ip::tcp::endpoint& selected_endpoint,
      redis_stream_state& st,
      asio::cancellation_type_t cancel_state);

   connect_action resume(
      system::error_code ec,
      redis_stream_state& st,
      asio::cancellation_type_t cancel_state);

};  // namespace boost::redis::detail

}  // namespace boost::redis::detail

#endif

/* Copyright (c) 2018-2024 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef BOOST_REDIS_ERROR_HPP
#define BOOST_REDIS_ERROR_HPP

#include <boost/system/error_code.hpp>

namespace boost::redis {

/// Generic errors.
enum class error
{
   /// Invalid RESP3 type.
   invalid_data_type = 1,

   /// Can't parse the string as a number.
   not_a_number,

   /// The maximum depth of a nested response was exceeded.
   exceeeds_max_nested_depth,

   /// Got non boolean value.
   unexpected_bool_value,

   /// Expected field value is empty.
   empty_field,

   /// Expects a simple RESP3 type but got an aggregate.
   expects_resp3_simple_type,

   /// Expects aggregate.
   expects_resp3_aggregate,

   /// Expects a map but got other aggregate.
   expects_resp3_map,

   /// Expects a set aggregate but got something else.
   expects_resp3_set,

   /// Nested response not supported.
   nested_aggregate_not_supported,

   /// Got RESP3 simple error.
   resp3_simple_error,

   /// Got RESP3 blob_error.
   resp3_blob_error,

   /// Aggregate container has incompatible size.
   incompatible_size,

   /// Not a double
   not_a_double,

   /// Got RESP3 null.
   resp3_null,

   /// There is no stablished connection.
   not_connected,

   /// Resolve timeout
   resolve_timeout,

   /// Connect timeout
   connect_timeout,

   /// The server didn't answer the health checks on time and didn't send any data during the health check period.
   pong_timeout,

   /// SSL handshake timeout
   ssl_handshake_timeout,

   /// (Deprecated) Can't receive push synchronously without blocking
   sync_receive_push_failed,

   /// Incompatible node depth.
   incompatible_node_depth,

   /// The setup request sent during connection establishment failed (the name is historical).
   resp3_hello,

   /// The configuration specified a UNIX socket address, but UNIX sockets are not supported by the system.
   unix_sockets_unsupported,

   /// The configuration specified UNIX sockets with SSL, which is not supported.
   unix_sockets_ssl_unsupported,

   /// Reading data from the socket would exceed the maximum size allowed of the read buffer.
   exceeds_maximum_read_buffer_size,

   /// Timeout while writing data to the server.
   write_timeout,

   /// The configuration specified UNIX sockets with Sentinel, which is not supported.
   sentinel_unix_sockets_unsupported,

   /// No Sentinel could be used to obtain the address of the Redis server.
   /// Sentinels might be unreachable, have authentication misconfigured or may not know about
   /// the configured master. Turn logging on for details.
   sentinel_resolve_failed,

   /// The contacted server is not a master as expected.
   /// This is likely a transient failure caused by a Sentinel failover in progress.
   role_check_failed,

   /// Expects a RESP3 string, but got a different data type.
   expects_resp3_string,

   /// Expects a RESP3 array, but got a different data type.
   expects_resp3_array,

   /// A @ref basic_connection::async_receive2 operation is already running.
   /// Only one of such operations might be running at any point in time.
   already_running,
};

/**
 * @brief Creates a error_code object from an error.
 *
 * @param e Error code.
 */
auto make_error_code(error e) -> system::error_code;

}  // namespace boost::redis

namespace std {

template <>
struct is_error_code_enum<::boost::redis::error> : std::true_type { };

}  // namespace std

#endif  // BOOST_REDIS_ERROR_HPP

/* Copyright (c) 2018-2024 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <boost/redis/error.hpp>

#include <boost/assert.hpp>

namespace boost::redis {
namespace detail {

struct error_category_impl : system::error_category {
   virtual ~error_category_impl() = default;

   auto name() const noexcept -> char const* override { return "boost.redis"; }

   auto message(int ev) const -> std::string override
   {
      switch (static_cast<error>(ev)) {
         case error::invalid_data_type: return "Invalid resp3 type.";
         case error::not_a_number:
            return "Can't convert string to number (maybe forgot to upgrade to RESP3?).";
         case error::exceeeds_max_nested_depth:
            return "Exceeds the maximum number of nested responses.";
         case error::unexpected_bool_value:          return "Unexpected bool value.";
         case error::empty_field:                    return "Expected field value is empty.";
         case error::expects_resp3_simple_type:      return "Expects a resp3 simple type.";
         case error::expects_resp3_aggregate:        return "Expects resp3 aggregate.";
         case error::expects_resp3_map:              return "Expects resp3 map.";
         case error::expects_resp3_set:              return "Expects resp3 set.";
         case error::nested_aggregate_not_supported: return "Nested aggregate not_supported.";
         case error::resp3_simple_error:             return "Got RESP3 simple-error.";
         case error::resp3_blob_error:               return "Got RESP3 blob-error.";
         case error::incompatible_size:              return "Aggregate container has incompatible size.";
         case error::not_a_double:                   return "Not a double.";
         case error::resp3_null:                     return "Got RESP3 null.";
         case error::not_connected:                  return "Not connected.";
         case error::resolve_timeout:                return "Resolve timeout.";
         case error::connect_timeout:                return "Connect timeout.";
         case error::pong_timeout:                   return "Pong timeout.";
         case error::ssl_handshake_timeout:          return "SSL handshake timeout.";
         case error::sync_receive_push_failed:
            return "Can't receive server push synchronously without blocking.";
         case error::incompatible_node_depth: return "Incompatible node depth.";
         case error::resp3_hello:
            return "The server response to the setup request sent during connection establishment "
                   "contains an error.";
         case error::unix_sockets_unsupported:
            return "The configuration specified a UNIX socket address, but UNIX sockets are not "
                   "supported by the system.";
         case error::unix_sockets_ssl_unsupported:
            return "The configuration specified UNIX sockets with SSL, which is not supported.";
         case error::exceeds_maximum_read_buffer_size:
            return "Reading data from the socket would exceed the maximum size allowed of the read "
                   "buffer.";
         case error::write_timeout: return "Timeout while writing data to the server.";
         case error::sentinel_unix_sockets_unsupported:
            return "The configuration specified UNIX sockets with Sentinel, which is not "
                   "supported.";
         case error::sentinel_resolve_failed:
            return "No Sentinel could be used to obtain the address of the Redis server.";
         case error::role_check_failed:
            return "The contacted server does not have the expected role. "
                   "This is likely a transient failure caused by a Sentinel failover in progress.";
         case error::expects_resp3_string:
            return "Expects a RESP3 string, but got a different data type.";
         case error::expects_resp3_array:
            return "Expects a RESP3 array, but got a different data type.";
         case error::already_running:
            return "An async_receive2 operation is already running. Only one of such operations "
                   "might be running at any point in time.";
         default: BOOST_ASSERT(false); return "Boost.Redis error.";
      }
   }
};

auto category() -> system::error_category const&
{
   static error_category_impl instance;
   return instance;
}

}  // namespace detail

auto make_error_code(error e) -> system::error_code
{
   return system::error_code{static_cast<int>(e), detail::category()};
}

}  // namespace boost::redis

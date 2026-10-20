//
// Copyright (c) 2025 Marcelo Zimbres Silva (mzimbres@gmail.com),
// Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/redis/adapter/any_adapter.hpp>
#include <boost/redis/config.hpp>
#include <boost/redis/detail/connect_params.hpp>
#include <boost/redis/detail/connection_state.hpp>
#include <boost/redis/detail/coroutine.hpp>
#include <boost/redis/detail/multiplexer.hpp>
#include <boost/redis/detail/run_fsm.hpp>
#include <boost/redis/error.hpp>
#include <boost/redis/impl/is_terminal_cancel.hpp>
#include <boost/redis/impl/log_utils.hpp>
#include <boost/redis/impl/sentinel_utils.hpp>
#include <boost/redis/impl/setup_request_utils.hpp>

#include <boost/asio/cancellation_type.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/local/basic_endpoint.hpp>  // for BOOST_ASIO_HAS_LOCAL_SOCKETS
#include <boost/system/error_code.hpp>

namespace boost::redis::detail {

inline system::error_code check_config(const config& cfg)
{
   if (!cfg.unix_socket.empty()) {
      if (cfg.use_ssl)
         return error::unix_sockets_ssl_unsupported;
      if (use_sentinel(cfg))
         return error::sentinel_unix_sockets_unsupported;
#ifndef BOOST_ASIO_HAS_LOCAL_SOCKETS
      return error::unix_sockets_unsupported;
#endif
   }
   return system::error_code{};
}

inline void compose_ping_request(const config& cfg, request& to)
{
   to.clear();
   to.push("PING", cfg.health_check_id);
}

inline void on_setup_done(const multiplexer::elem& elm, connection_state& st)
{
   const auto ec = elm.get_error();
   if (ec) {
      if (ec == error::resp3_hello) {
         // This is the most common case, and the only one that generates a string diagnostic
         log_err(st.logger, "Setup request execution failed: ", st.diagnostic);
      } else {
         // Something else went wrong (e.g. network error while running the request).
         log_err(st.logger, "Setup request execution failed: ", ec);
      }
   } else {
      log_info(st.logger, "Setup request execution: success");
   }
}

inline any_address_view get_server_address(const connection_state& st)
{
   if (st.cfg.unix_socket.empty()) {
      return {st.cfg.addr, st.cfg.use_ssl};
   } else {
      return any_address_view{st.cfg.unix_socket};
   }
}

template <>
struct log_traits<any_address_view> {
   static inline void log(std::string& to, any_address_view value)
   {
      if (value.type() == transport_type::unix_socket) {
         to += '\'';
         to += value.unix_socket();
         to += '\'';
      } else {
         log_traits<address>::log(to, value.tcp_address());
         to += value.type() == transport_type::tcp_tls ? " (TLS enabled)" : " (TLS disabled)";
      }
   }
};

run_action run_fsm::resume(
   connection_state& st,
   system::error_code ec,
   asio::cancellation_type_t cancel_state)
{
   switch (resume_point_) {
      BOOST_REDIS_CORO_INITIAL

      // Check config
      ec = check_config(st.cfg);
      if (ec) {
         log_err(st.logger, "Invalid configuration: ", ec);
         stored_ec_ = ec;
         BOOST_REDIS_YIELD(resume_point_, 1, run_action_type::immediate)
         return stored_ec_;
      }

      // Clear any remainder from previous runs
      st.tracker.clear();

      // Compose the PING request. This only depends on the config, so it can be done just once
      compose_ping_request(st.cfg, st.ping_req);

      if (use_sentinel(st.cfg)) {
         // Sentinel request. Same as above
         compose_sentinel_request(st.cfg);

         // Bootstrap the sentinel list with the ones configured by the user
         st.sentinels = st.cfg.sentinel.addresses;
      }

      for (;;) {
         // Sentinel resolve, if required. This leaves the address in st.cfg.address
         if (use_sentinel(st.cfg)) {
            // This operation does the logging for us.
            BOOST_REDIS_YIELD(resume_point_, 2, run_action_type::sentinel_resolve)

            // Check for cancellations
            if (is_terminal_cancel(cancel_state)) {
               log_debug(st.logger, "Run: cancelled (4)");
               return {asio::error::operation_aborted};
            }

            // Check for errors
            if (ec)
               goto sleep_and_reconnect;
         }

         // Try to connect
         log_info(st.logger, "Trying to connect to Redis server at ", get_server_address(st));
         BOOST_REDIS_YIELD(resume_point_, 4, run_action_type::connect)

         // Check for cancellations
         if (is_terminal_cancel(cancel_state)) {
            log_debug(st.logger, "Run: cancelled (1)");
            return system::error_code(asio::error::operation_aborted);
         }

         if (ec) {
            // There was an error. Skip to the reconnection loop
            log_err(
               st.logger,
               "Failed to connect to Redis server at ",
               get_server_address(st),
               ": ",
               ec);
            goto sleep_and_reconnect;
         }

         // We were successful
         log_info(st.logger, "Connected to Redis server at ", get_server_address(st));

         // Initialization
         st.mpx.reset();
         st.diagnostic.clear();
         compose_setup_request(st.cfg, st.tracker, st.setup_req);

         // Add the setup request to the multiplexer
         if (st.setup_req.get_commands() != 0u) {
            auto elm = make_elem(st.setup_req, make_any_adapter_impl(setup_adapter{st}));
            elm->set_done_callback([&elem_ref = *elm, &st] {
               on_setup_done(elem_ref, st);
            });
            st.mpx.add(elm);
         }

         // Run the tasks
         BOOST_REDIS_YIELD(resume_point_, 5, run_action_type::parallel_group)

         // Store any error yielded by the tasks for later
         stored_ec_ = ec;

         // We've lost connection or otherwise been cancelled.
         // Remove from the multiplexer the required requests.
         st.mpx.cancel_on_conn_lost();

         // The receive operation must be cancelled because channel
         // subscription does not survive a reconnection but requires
         // re-subscription.
         BOOST_REDIS_YIELD(resume_point_, 6, run_action_type::cancel_receive)

         // Restore the error
         ec = stored_ec_;

         // Check for cancellations
         if (is_terminal_cancel(cancel_state)) {
            log_debug(st.logger, "Run: cancelled (2)");
            return system::error_code(asio::error::operation_aborted);
         }

sleep_and_reconnect:

         // If we are not going to try again, we're done
         if (st.cfg.reconnect_wait_interval.count() == 0) {
            return ec;
         }

         // Wait for the reconnection interval
         BOOST_REDIS_YIELD(resume_point_, 7, run_action_type::wait_for_reconnection)

         // Check for cancellations
         if (is_terminal_cancel(cancel_state)) {
            log_debug(st.logger, "Run: cancelled (3)");
            return system::error_code(asio::error::operation_aborted);
         }
      }
   }

   // We should never get here
   BOOST_ASSERT(false);
   return system::error_code();
}

connect_params make_run_connect_params(const connection_state& st)
{
   return {
      get_server_address(st),
      st.cfg.resolve_timeout,
      st.cfg.connect_timeout,
      st.cfg.ssl_handshake_timeout,
   };
}

}  // namespace boost::redis::detail

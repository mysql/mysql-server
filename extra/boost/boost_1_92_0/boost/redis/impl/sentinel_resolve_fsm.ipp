//
// Copyright (c) 2025 Marcelo Zimbres Silva (mzimbres@gmail.com),
// Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REDIS_SENTINEL_RESOLVE_FSM_IPP
#define BOOST_REDIS_SENTINEL_RESOLVE_FSM_IPP

#include <boost/redis/adapter/any_adapter.hpp>
#include <boost/redis/config.hpp>
#include <boost/redis/detail/connect_params.hpp>
#include <boost/redis/detail/connection_state.hpp>
#include <boost/redis/detail/coroutine.hpp>
#include <boost/redis/detail/sentinel_resolve_fsm.hpp>
#include <boost/redis/error.hpp>
#include <boost/redis/impl/is_terminal_cancel.hpp>
#include <boost/redis/impl/log_utils.hpp>
#include <boost/redis/impl/sentinel_utils.hpp>

#include <boost/asio/error.hpp>
#include <boost/assert.hpp>

#include <cstddef>
#include <random>
#include <string_view>

namespace boost::redis::detail {

// Logs an error at info level, and also stores it in the state,
// so it can be logged at error level if all Sentinels fail.
template <class... Args>
void log_sentinel_error(connection_state& st, std::size_t current_idx, const Args&... args)
{
   st.diagnostic += "\n  ";
   std::size_t size_before = st.diagnostic.size();
   format_log_args(st.diagnostic, "Sentinel at ", st.sentinels[current_idx], ": ", args...);
   log_info(st.logger, std::string_view{st.diagnostic}.substr(size_before));
}

sentinel_action sentinel_resolve_fsm::resume(
   connection_state& st,
   system::error_code ec,
   asio::cancellation_type_t cancel_state)
{
   switch (resume_point_) {
      BOOST_REDIS_CORO_INITIAL

      st.diagnostic.clear();

      log_info(
         st.logger,
         "Trying to resolve the address of ",
         st.cfg.sentinel.server_role == role::master ? "master" : "a replica of master",
         " '",
         st.cfg.sentinel.master_name,
         "' using Sentinel");

      // Try all Sentinels in order. Upon any errors, save the diagnostic and try with the next one.
      // If none of them are available, print an error diagnostic and fail.
      for (idx_ = 0u; idx_ < st.sentinels.size(); ++idx_) {
         log_debug(st.logger, "Trying to contact Sentinel at ", st.sentinels[idx_]);

         // Try to connect
         BOOST_REDIS_YIELD(resume_point_, 1, st.sentinels[idx_])

         // Check for cancellations
         if (is_terminal_cancel(cancel_state)) {
            log_debug(st.logger, "Sentinel resolve: cancelled (1)");
            return system::error_code(asio::error::operation_aborted);
         }

         // Check for errors
         if (ec) {
            log_sentinel_error(st, idx_, "connection establishment error: ", ec);
            continue;
         }

         // Execute the Sentinel request
         log_debug(st.logger, "Executing Sentinel request at ", st.sentinels[idx_]);
         st.sentinel_resp_nodes.clear();
         BOOST_REDIS_YIELD(resume_point_, 2, sentinel_action::request())

         // Check for cancellations
         if (is_terminal_cancel(cancel_state)) {
            log_debug(st.logger, "Sentinel resolve: cancelled (2)");
            return system::error_code(asio::error::operation_aborted);
         }

         // Check for errors
         if (ec) {
            log_sentinel_error(st, idx_, "error while executing request: ", ec);
            continue;
         }

         // Parse the response
         sentinel_response resp;
         ec = parse_sentinel_response(st.sentinel_resp_nodes, st.cfg.sentinel.server_role, resp);

         if (ec) {
            if (ec == error::resp3_simple_error || ec == error::resp3_blob_error) {
               log_sentinel_error(st, idx_, "responded with an error: ", resp.diagnostic);
            } else if (ec == error::resp3_null) {
               log_sentinel_error(st, idx_, "doesn't know about the configured master");
            } else {
               log_sentinel_error(
                  st,
                  idx_,
                  "error parsing response (maybe forgot to upgrade to RESP3?): ",
                  ec);
            }

            continue;
         }

         // When asking for replicas, we might get no replicas
         if (st.cfg.sentinel.server_role == role::replica && resp.replicas.empty()) {
            log_sentinel_error(st, idx_, "the configured master has no replicas");
            continue;
         }

         // Store the resulting address in a well-known place
         if (st.cfg.sentinel.server_role == role::master) {
            st.cfg.addr = resp.master_addr;
         } else {
            // Choose a random replica
            std::uniform_int_distribution<std::size_t> dist{0u, resp.replicas.size() - 1u};
            const auto idx = dist(st.eng.get());
            st.cfg.addr = resp.replicas[idx];
         }

         // Sentinel knows about this master. Log and update our config
         log_info(
            st.logger,
            "Sentinel at ",
            st.sentinels[idx_],
            " resolved the server address to ",
            st.cfg.addr);

         update_sentinel_list(st.sentinels, idx_, resp.sentinels, st.cfg.sentinel.addresses);

         return system::error_code();
      }

      // No Sentinel resolved our address
      log_err(
         st.logger,
         "Failed to resolve the address of ",
         st.cfg.sentinel.server_role == role::master ? "master" : "a replica of master",
         " '",
         st.cfg.sentinel.master_name,
         "'. Tried the following Sentinels:",
         st.diagnostic);
      return {error::sentinel_resolve_failed};
   }

   // We should never get here
   BOOST_ASSERT(false);
   return system::error_code();
}

connect_params make_sentinel_connect_params(const config& cfg, const address& addr)
{
   return {
      any_address_view{addr, cfg.sentinel.use_ssl},
      cfg.sentinel.resolve_timeout,
      cfg.sentinel.connect_timeout,
      cfg.sentinel.ssl_handshake_timeout,
   };
}

any_adapter make_sentinel_adapter(connection_state& st)
{
   return any_adapter(st.sentinel_resp_nodes);
}

}  // namespace boost::redis::detail

#endif

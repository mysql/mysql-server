/* Copyright (c) 2018-2024 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef BOOST_REDIS_SETUP_REQUEST_UTILS_HPP
#define BOOST_REDIS_SETUP_REQUEST_UTILS_HPP

#include <boost/redis/config.hpp>
#include <boost/redis/detail/connection_state.hpp>
#include <boost/redis/detail/subscription_tracker.hpp>
#include <boost/redis/error.hpp>
#include <boost/redis/impl/sentinel_utils.hpp>  // use_sentinel
#include <boost/redis/request.hpp>
#include <boost/redis/resp3/node.hpp>
#include <boost/redis/resp3/type.hpp>
#include <boost/redis/response.hpp>

#include <cstddef>

namespace boost::redis::detail {

// Modifies config::setup to make a request suitable to be sent
// to the server using async_exec
inline void compose_setup_request(
   const config& cfg,
   const subscription_tracker& pubsub_st,
   request& req)
{
   // Clear any previous contents
   req.clear();

   // Set the appropriate flags
   request_access::set_priority(req, true);
   req.get_config().cancel_if_unresponded = true;
   req.get_config().cancel_on_connection_lost = true;

   if (cfg.use_setup) {
      // We should use the provided request as-is
      req.append(cfg.setup);
   } else {
      // We're not using the setup request as-is, but should compose one based on
      // the values passed by the user

      // Which parts of the command should we send?
      // Don't send AUTH if the user is the default and the password is empty.
      // Other users may have empty passwords.
      // Note that this is just an optimization.
      bool send_auth = !(
         cfg.username.empty() || (cfg.username == "default" && cfg.password.empty()));
      bool send_setname = !cfg.clientname.empty();

      // Gather everything we can in a HELLO command
      if (send_auth && send_setname)
         req.hello_setname(cfg.username, cfg.password, cfg.clientname);
      else if (send_auth)
         req.hello(cfg.username, cfg.password);
      else if (send_setname)
         req.hello_setname(cfg.clientname);
      else
         req.hello();

      // SELECT is independent of HELLO
      if (cfg.database_index && cfg.database_index.value() != 0)
         req.push("SELECT", cfg.database_index.value());
   }

   // When using Sentinel, we should add a role check.
   // This must happen after the other commands, as it requires authentication.
   if (use_sentinel(cfg))
      req.push("ROLE");

   // Add any subscription commands require to restore the PubSub state
   pubsub_st.compose_subscribe_request(req);
}

class setup_adapter {
   connection_state* st_;
   std::size_t response_idx_{0u};
   bool role_seen_{false};

   system::error_code on_node_impl(const resp3::node_view& nd)
   {
      // An error node is always an error
      switch (nd.data_type) {
         case resp3::type::simple_error:
         case resp3::type::blob_error:   st_->diagnostic = nd.value; return error::resp3_hello;
         default:                        ;
      }

      // When using Sentinel, we add a ROLE command at the end.
      // We need to ensure that this instance is a master.
      // ROLE may be followed by subscribe requests, but these don't expect any response.
      if (use_sentinel(st_->cfg) && response_idx_ == st_->setup_req.get_expected_responses() - 1u) {
         // ROLE's response should be an array of at least 1 element
         if (nd.depth == 0u) {
            if (nd.data_type != resp3::type::array)
               return error::invalid_data_type;
            if (nd.aggregate_size == 0u)
               return error::incompatible_size;
         }

         // The first node should be 'master' if we're connecting to a primary,
         // 'slave' if we're connecting to a replica
         if (nd.depth == 1u && !role_seen_) {
            role_seen_ = true;
            if (nd.data_type != resp3::type::blob_string)
               return error::invalid_data_type;

            const char* expected_role = st_->cfg.sentinel.server_role == role::master ? "master"
                                                                                      : "slave";
            if (nd.value != expected_role)
               return error::role_check_failed;
         }
      }

      return system::error_code();
   }

public:
   explicit setup_adapter(connection_state& st) noexcept
   : st_(&st)
   { }

   void on_init() { }
   void on_done() { ++response_idx_; }
   void on_node(const resp3::node_view& node, system::error_code& ec) { ec = on_node_impl(node); }
};

}  // namespace boost::redis::detail

#endif  // BOOST_REDIS_RUNNER_HPP

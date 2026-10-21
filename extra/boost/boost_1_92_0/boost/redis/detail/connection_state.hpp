//
// Copyright (c) 2025 Marcelo Zimbres Silva (mzimbres@gmail.com),
// Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REDIS_CONNECTION_STATE_HPP
#define BOOST_REDIS_CONNECTION_STATE_HPP

#include <boost/redis/config.hpp>
#include <boost/redis/detail/multiplexer.hpp>
#include <boost/redis/detail/subscription_tracker.hpp>
#include <boost/redis/logger.hpp>
#include <boost/redis/request.hpp>
#include <boost/redis/resp3/flat_tree.hpp>
#include <boost/redis/response.hpp>

#include <random>
#include <string>
#include <vector>

namespace boost::redis::detail {

// A random engine that gets seeded lazily.
// Seeding with std::random_device is not trivial and might fail.
class lazy_random_engine {
   bool seeded_{};
   std::minstd_rand eng_;

public:
   lazy_random_engine() = default;
   std::minstd_rand& get()
   {
      if (!seeded_) {
         eng_.seed(static_cast<std::minstd_rand::result_type>(std::random_device{}()));
         seeded_ = true;
      }
      return eng_;
   }
};

// Contains all the members in connection that don't depend on the Executor.
// Makes implementing sans-io algorithms easier
struct connection_state {
   buffered_logger logger;
   config cfg{};
   multiplexer mpx{};
   std::string diagnostic{};  // Used by the setup request and Sentinel
   request setup_req{};
   request ping_req{};
   subscription_tracker tracker{};
   bool receive2_running{false}, receive2_cancelled{false};

   // Sentinel stuff
   lazy_random_engine eng{};
   std::vector<address> sentinels{};
   resp3::flat_tree sentinel_resp_nodes{};  // for parsing
};

}  // namespace boost::redis::detail

#endif  // BOOST_REDIS_CONNECTOR_HPP

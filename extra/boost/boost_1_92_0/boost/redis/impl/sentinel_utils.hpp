//
// Copyright (c) 2025 Marcelo Zimbres Silva (mzimbres@gmail.com),
// Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_REDIS_SENTINEL_UTILS_HPP
#define BOOST_REDIS_SENTINEL_UTILS_HPP

#include <boost/redis/config.hpp>
#include <boost/redis/error.hpp>
#include <boost/redis/resp3/flat_tree.hpp>
#include <boost/redis/resp3/node.hpp>
#include <boost/redis/resp3/type.hpp>

#include <boost/assert.hpp>
#include <boost/core/span.hpp>
#include <boost/system/error_code.hpp>

#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include <algorithm>

namespace boost::redis::detail {

// Returns true if Sentinel should be used
inline bool use_sentinel(const config& cfg) { return !cfg.sentinel.addresses.empty(); }

// Composes the request to send to Sentinel modifying cfg.sentinel.setup
inline void compose_sentinel_request(config& cfg)
{
   // These commands should go after the user-supplied setup, as this might involve authentication.
   // We ask for the master even when connecting to replicas to correctly detect when the master doesn't exist
   cfg.sentinel.setup.push("SENTINEL", "GET-MASTER-ADDR-BY-NAME", cfg.sentinel.master_name);
   if (cfg.sentinel.server_role == role::replica)
      cfg.sentinel.setup.push("SENTINEL", "REPLICAS", cfg.sentinel.master_name);
   cfg.sentinel.setup.push("SENTINEL", "SENTINELS", cfg.sentinel.master_name);

   // Note that we don't care about request flags because this is a one-time request
}

// Helper
inline system::error_code sentinel_check_errors(const resp3::node_view& nd, std::string& diag)
{
   switch (nd.data_type) {
      case resp3::type::simple_error: diag = nd.value; return error::resp3_simple_error;
      case resp3::type::blob_error:   diag = nd.value; return error::resp3_blob_error;
      default:                        return system::error_code();
   }
};

// Parses a list of replicas or sentinels
inline system::error_code parse_server_list(
   const resp3::flat_tree& tree,
   std::size_t& index,
   std::string& diag,
   std::vector<address>& out)
{
   const auto& root = tree.at(index);
   BOOST_ASSERT(root.depth == 0u);

   // If the command failed, this will be an error
   if (auto ec = sentinel_check_errors(root, diag))
      return ec;

   // The root node must be an array
   if (root.data_type != resp3::type::array)
      return error::expects_resp3_array;
   const std::size_t num_servers = root.aggregate_size;
   ++index;

   // Each element in the array represents a server
   out.resize(num_servers);
   for (std::size_t i = 0u; i < num_servers; ++i) {
      // A server is a map (resp3) or array (resp2, currently unsupported)
      const auto& server_node = tree.at(index);
      BOOST_ASSERT(server_node.depth == 1u);
      if (server_node.data_type != resp3::type::map)
         return error::expects_resp3_map;
      const std::size_t num_key_values = server_node.aggregate_size;
      ++index;

      // The server object is composed by a set of key/value pairs.
      // Skip everything except for the ones we care for.
      bool ip_seen = false, port_seen = false;
      for (std::size_t j = 0; j < num_key_values; ++j) {
         // Key. It should be a string
         const auto& key_node = tree.at(index);
         BOOST_ASSERT(key_node.depth == 2u);
         if (key_node.data_type != resp3::type::blob_string)
            return error::expects_resp3_string;
         const std::string_view key = key_node.value;
         ++index;

         // Value. All values seem to be strings, too.
         const auto& value_node = tree.at(index);
         BOOST_ASSERT(value_node.depth == 2u);
         if (value_node.data_type != resp3::type::blob_string)
            return error::expects_resp3_string;

         // Record it
         if (key == "ip") {
            ip_seen = true;
            out[i].host = value_node.value;
         } else if (key == "port") {
            port_seen = true;
            out[i].port = value_node.value;
         }

         ++index;
      }

      // Check that the response actually contained the fields we wanted
      if (!ip_seen || !port_seen)
         return error::empty_field;
   }

   // Done
   return system::error_code();
}

// Parses the output of SENTINEL GET-MASTER-ADDR-BY-NAME
inline system::error_code parse_get_master_addr_by_name(
   const resp3::flat_tree& tree,
   std::size_t& index,
   std::string& diag,
   address& out)
{
   const auto& root_node = tree.at(index);
   BOOST_ASSERT(root_node.depth == 0u);

   // Check for errors
   if (auto ec = sentinel_check_errors(root_node, diag))
      return ec;

   // If the root node is NULL, Sentinel doesn't know about this master.
   // We use resp3_null to signal this fact. This doesn't reach the end user.
   // If this is the case, SENTINEL REPLICAS and SENTINEL SENTINELS will fail.
   // We exit here so the diagnostic is clean.
   if (root_node.data_type == resp3::type::null) {
      return error::resp3_null;
   }

   // If the root node is an array, an IP and port follow
   if (root_node.data_type != resp3::type::array)
      return error::expects_resp3_array;
   if (root_node.aggregate_size != 2u)
      return error::incompatible_size;
   ++index;

   // IP
   const auto& ip_node = tree.at(index);
   BOOST_ASSERT(ip_node.depth == 1u);
   if (ip_node.data_type != resp3::type::blob_string)
      return error::expects_resp3_string;
   out.host = ip_node.value;
   ++index;

   // Port
   const auto& port_node = tree.at(index);
   BOOST_ASSERT(port_node.depth == 1u);
   if (port_node.data_type != resp3::type::blob_string)
      return error::expects_resp3_string;
   out.port = port_node.value;
   ++index;

   return system::error_code();
}

// The output type of parse_sentinel_response
struct sentinel_response {
   std::string diagnostic;         // In case the server returned an error
   address master_addr;            // Always populated
   std::vector<address> replicas;  // Populated only when connecting to replicas
   std::vector<address> sentinels;
};

// Parses an array of nodes into a sentinel_response.
// The request originating this response should be:
//    <user-supplied commands, as per sentinel_config::setup>
//    SENTINEL GET-MASTER-ADDR-BY-NAME
//    SENTINEL REPLICAS (only if server_role is replica)
//    SENTINEL SENTINELS
// SENTINEL SENTINELS and SENTINEL REPLICAS error when the master name is unknown. Error nodes
// should be allowed in the node array.
// This means that we can't use generic_response, since its adapter errors on error nodes.
// SENTINEL GET-MASTER-ADDR-BY-NAME is sent even when connecting to replicas
//    for better diagnostics when the master name is unknown.
// Note that the tree should originate from a valid RESP3 message
//   (i.e. we won't check that the first node has depth 0.)
inline system::error_code parse_sentinel_response(
   const resp3::flat_tree& tree,
   role server_role,
   sentinel_response& out)
{
   // Clear the output
   out.diagnostic.clear();
   out.sentinels.clear();
   out.replicas.clear();

   // Index-based access
   std::size_t index = 0;

   // User-supplied commands are before the ones added by us.
   // Find out how many responses should we skip
   const std::size_t num_lib_msgs = server_role == role::master ? 2u : 3u;
   if (tree.get_total_msgs() < num_lib_msgs)
      return error::incompatible_size;
   const std::size_t num_user_msgs = tree.get_total_msgs() - num_lib_msgs;

   // Go through all the responses to user-supplied requests checking for errors
   BOOST_ASSERT(tree.at(index).depth == 0u);
   for (std::size_t remaining_roots = num_user_msgs + 1u;; ++index) {
      // Exit at node N+1
      const auto& node = tree.at(index);
      if (node.depth == 0u && --remaining_roots == 0u)
         break;

      // This is a user-supplied message. Check for errors
      if (auto ec = sentinel_check_errors(node, out.diagnostic))
         return ec;
   }

   // SENTINEL GET-MASTER-ADDR-BY-NAME
   if (auto ec = parse_get_master_addr_by_name(tree, index, out.diagnostic, out.master_addr))
      return ec;

   // SENTINEL REPLICAS
   if (server_role == role::replica) {
      if (auto ec = parse_server_list(tree, index, out.diagnostic, out.replicas))
         return ec;
   }

   // SENTINEL SENTINELS
   if (auto ec = parse_server_list(tree, index, out.diagnostic, out.sentinels))
      return ec;

   // Done
   return system::error_code();
}

// Updates the internal Sentinel list.
// to should never be empty
inline void update_sentinel_list(
   std::vector<address>& to,
   std::size_t current_index,               // the one to maintain and place first
   span<const address> gossip_sentinels,    // the ones that SENTINEL SENTINELS returned
   span<const address> bootstrap_sentinels  // the ones the user supplied
)
{
   BOOST_ASSERT(!to.empty());

   // Remove everything, except the Sentinel that succeeded
   if (current_index != 0u)
      std::swap(to.front(), to[current_index]);
   to.resize(1u);

   // Add one group. These Sentinels are always unique and don't include the one we're currently connected to.
   to.insert(to.end(), gossip_sentinels.begin(), gossip_sentinels.end());

   // Insert any user-supplied sentinels, if not already present.
   // This is O(n^2), but is okay because n will be small.
   // The list can't be sorted, anyway
   for (const auto& sentinel : bootstrap_sentinels) {
      if (std::find(to.begin(), to.end(), sentinel) == to.end())
         to.push_back(sentinel);
   }
}

}  // namespace boost::redis::detail

#endif

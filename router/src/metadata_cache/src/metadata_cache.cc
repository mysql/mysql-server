/*
  Copyright (c) 2016, 2018, Oracle and/or its affiliates. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is also distributed with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have included with MySQL.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

/**
 * @defgroup MDC Metadata Cache
 *
 * Synopsis
 * ========
 *
 * Metadata Cache plugin communicates with Metadata and Group Replication
 * exposed by the cluster to obtain its topology and availablity information.
 * The digest of this information is then exposed to Routing Plugin in form of a
 * routing table.
 * Key components:
 * - Metadata Cache API - interface through which it exposes its service
 * - Refresh Mechansim - responsible for updating routing table
 *
 *
 *
 *
 *
 * Glossary
 * ========
 *
 * replicaset = group of servers that contain the same data; in simple cases
 *              replicaset and cluster are interchangeable, but in case of
 *              sharded cluster this no longer applies, as the cluster will be
 *              composed of multiple replicasets, each handling a different
 *              shard.
 *
 * MD = metadata, several tables residing on the metadata server, which (among
 *      other things) contain cluster topology information. It reflects the
 *      desired "as it should be" version of topology.
 *
 * GR = Group Replication, a server-side plugin responsible for synchronising
 *      data between cluster nodes. It exposes certain dynamic tables (views)
 *      that we query to obtain health status of the cluster. It reflects the
 *      real "as it actually is" version of topology.
 *
 * MDC = MD Cache, this plugin (the code that you're reading now).
 *
 * MM = multi-master, a replication mode in which all GR members are RW.
 *
 * SM = single-master, a replication mode in which only one GR member is RW,
 *      rest are RO.
 *
 * ATTOW = contraction for "at the time of writing".
 *
 * [xx] (where x is a digit) = reference to a note in Notes section.
 *
 *
 *
 *
 *
 * Refresh Mechanism
 * =================
 *
 * @note
 * To keep docs simpler, all below describes how MDC behaves in case of handling
 * just one replicaset. It has been designed to handle more than one, however,
 * ATTOW we don't test it with more than one, so we are uncertain if it would
 * actually deliver on that promise.  This is also the reason why throughout the
 * MDC code there are data structures that are collections of replicasets and
 * for loops that iterate over them, yet in reality we always deal with just one
 * element in those containers and such loops iterate only once.
 *
 *
 *
 *
 *
 * ## Overview
 * MDC refresh runs in its own thread and periodically queries both MD and GR
 * for status, then updates routing table which is queried by the Routing
 * Plugin. Its entry point is `MetadataCache::start()`, which (indirectly) runs
 * a "forever loop" in `MetadataCache::refresh_thread()`, which in turn is
 * responsible for perodically running `MetadataCache::refresh()`.
 *
 * `MetadataCache::refresh()` is the "workhorse" of refresh mechanism.
 *
 *
 *
 *
 *
 * ## Refresh trigger
 * `MetadataCache::refresh_thread()` call to `MetadataCache::refresh()` can be
 * triggered in 2 ways:
 * - `<TTL>` seconds passed since last refresh
 * - emergency mode (replicaset is flagged to have at least one node
 * unreachable).
 *
 * It's implemented by running a sleep loop between refreshes. The loop sleeps 1
 * second at a time, until `<TTL>` iterations have gone or emergency mode is
 * enabled.
 *
 *
 *
 * ### Emergency mode
 * Emergency mode is entered, when Routing Plugin discovers that it's unable to
 * connect to a node that's declared by MDC as routable (node that is labelled
 * as writable or readonly). In such situation, it will flag the replicaset as
 * missing a node, and MDC will react by increasing refresh rate to 1/s (if it
 * is currently lower).
 *
 * This emergency mode will stay enabled, until routing table resulting from
 * most recent MD and GR query is different from the one before it _AND_ the
 * replicaset is in RW mode.
 *
 * @note
 * The reason why we require the routing table to be different before we disable
 * the emergency mode, is because it usually takes several seconds for GR to
 * figure out that some node went down. Thus we want to wait until GR gives us a
 * topology that reflects the change. This strategy might have a bug however
 * [05].
 *
 * @note
 * The reason why we require the replicaset to be in RW mode before we disable
 * the emergency mode, is the assumption that the user wants the replicaset to
 * be RW and if it is in RO, it is undergoing a failure. This assumption is
 * probably flawed [06].
 *
 *
 *
 *
 *
 * ## Refresh process
 * Once refresh is called, it goes through the following stages:
 * - Stage 1: Query MD
 * - Stage 2: Query GR, combine results with MD, determine availability
 * - Stage 3: Update routing table
 *
 * In subsequent sections each stage is described in more detail
 *
 *
 *
 *
 *
 * ### Stage 1: Query MD
 * This stage can be divided into two major substages:
 *   1. Connect to MD server
 *   2. Extract MD information
 *
 *
 *
 * #### Stage 1.1: Connect to MD server
 *
 * Implemented in: `ClusterMetadata::connect()`
 *
 * MDC starts with a list of MD servers written in the configuration file, such
 * as:
 *
 *     bootstrap_server_addresses = mysql://192.168.56.101:3310,
 *                                  mysql://192.168.56.101:3320,
 *                                  mysql://192.168.56.101:3330
 *
 * It iterates through the list and tries to connect to each one, until
 * connection succeeds.
 *
 * @note
 * This behavior might change in near future, because it does not ensure that
 * connected MD server holds valid MD data [01].
 *
 * @note
 * Iteration always starting from 1st server on the list might also change [02].
 *
 * @note
 * New connection is always established and old one closed off, even if old one
 * is still alive and usable.
 *
 *
 *
 * #### Stage 1.2: Extract MD Information
 *
 * Implemented in: `ClusterMetadata::fetch_instances_from_metadata_server()`
 *
 * Using connection established in Stage 1.1, MDC runs a SQL query which
 * extracts a list of nodes (GR members) belonging to the replicaset. Note that
 * this the configured "should be" view of replicaset topology, which might not
 * correspond to actual topology, if for example some nodes became unavailable,
 * changed their role or new nodes were added without updating MD in the
 * server.
 *
 * @note
 * ATTOW, if this query fails, whole refresh process fails [03].
 *
 *
 *
 *
 *
 * ### Stage 2: Query GR, combine results with MD, determine availability
 *
 * Implemented in: `ClusterMetadata::update_replicaset_status()`
 *
 * Here MDC iterates through the list of GR members obtained from MD in Stage
 * 1.2, until it finds a "trustworthy" GR node. A "trustworthy" GR node is one
 * that passes the following substages:
 *   1. successfully connects
 *   2. successfully responds to 2 GR status SQL queries
 *   3. is part of quorum (regardless of whether it's available or not)
 *
 * If MDC doesn't find a "trustworthy" node, it clears the routing table,
 * resulting in Routing Plugin not routing any new connections.
 *
 * @note
 * Since Stage 2 got its list of candidate GR nodes from MD server, it follows
 * that MDC will never query any nodes not present in MD for GR status.
 *
 * @note
 * Any routing table updates will not go into effect until Stage 3, where it is
 * applied.
 *
 * @note
 * ATTOW clearing routing table will not automatically close off old
 * connections. This is a bug which is addressed by upcoming WL#11954.
 *
 *
 *
 * #### Stage 2.1: Connect to GR node
 *
 * Implemented in: `ClusterMetadata::update_replicaset_status()`
 *
 * New connection to GR node is established (on failure, Stage 2 progresses to
 * next iteration).
 *
 * @note
 * Since connection to MD server in Stage 1.1 is not closed after that stage
 * finishes, there's an optimisation done for when connecting to a GR member
 * that's the same node as the MD server - in such case, the connection is
 * simply re-used rather than new one opened.
 *
 *
 *
 * #### Stage 2.2: Extract GR status
 *
 * Implemented in: `fetch_group_replication_members()` and
 *                   `find_group_replication_primary_member()`
 *
 * Two SQL queries are ran and combined to produce a status report of all nodes
 * seen by this node (which would be the entire replicaset if it was in perfect
 * health, or its subset if some nodes became unavailable or the replicaset was
 * experiencing a split-brain scenario):
 *
 *   1. determine the PRIMARY member of the replicaset (if there is more than
 *      one, such as in MM setups, the first one is returned and the rest are
 *      ignored)
 *
 *   2. get the membership and health status of all GR nodes, as seen by this
 *      node
 *
 * If either SQL query fails to execute, Stage 2 iterates to next GR node.
 *
 * @note
 * ATTOW, 1st query is always ran, regardless of whether we're in MM mode or
 * not. As all nodes are PRIMARY in MM setups, we could optimise this query away
 * in MM setups.
 *
 *
 *
 * #### Stage 2.3: Quorum test
 *
 * Implemented in: `ClusterMetadata::update_replicaset_status()` and
 *                   `ClusterMetadata::check_replicaset_status()`
 *
 * MD and GR data collected up to now are compared, to see if GR node just
 * queried belongs to an available replicaset (or to an available replicaset
 * partition, if replicaset has partitioned). For a replicaset (partition) to
 * be considered available, it has to have quorum, that is, meet the following
 * condition:
 *
 *     count(ONLINE nodes) + count(RECOVERING nodes)
 *         is greater than
 *     1/2 * count(all original nodes according to MD).
 *
 * If particular GR node does not meet the quorum condition, Stage 2 iterates
 * to next GR node.
 *
 * OTOH, if GR node is part of quorum, Stage 2 will not iterate further,
 * because it would be pointless (it's not possible to find a member that's
 * part of another quorum, because there can only be one quorum, the one we
 * just found). This matters, because having quorum does not automatically
 * imply being available, as next paragraph explains.
 *
 * The availability test will resolve node's replicaset to be in of the 4
 * possible states:
 * - Unavailable (this node is not part of quorum)
 * - UnavailableRecovering (quorum is met, but it consists of only RECOVERING
 *   nodes - this is a rare cornercase)
 * - AvailableWritable (quorum is met, at least one RW node present)
 * - AvailableReadOnly (quorum is met, no RW nodes present)
 *
 * As already mentioned, reaching 1st of the 4 above states will result in
 * Stage 2 iterating to the next node. OTOH, achieving one of remaining 3
 * states will cause MDC to move on to Stage 3, where it sets the routing table
 * accordingly.
 *
 *
 * ##### GR-MD dishorenecy
 *
 * ATTOW, our Router has a certain limitation: it assumes that MD contains an
 * exact set or superset of nodes in GR. The user is normally expected to use
 * MySQL Shell to reconfigure the replicaset, which automatically updates both
 * GR and MD, keeping them in sync. But if for some reason the user tinkers with
 * GR directly and adds nodes without updating MD accordingly,
 * availablity/quorum calculations will be skewed. We run checks to detect such
 * situation, and log a warning like so:
 *
 *     log_error("Member %s:%d (%s) found in replicaset, yet is not defined in
 *     metadata!"
 *
 * but beyond that we just act defensively by having our quorum calculation be
 * conservative, and error on the side of caution when such discrepancy happens
 * (quorum becomes harder to reach than if MD contained all GR members).
 *
 * Quorum is evaluated in code as follows:
 *
 *     bool have_quorum = (quorum_count > member_status.size()/2);
 *
 * - `quorum_count` is the sum of PRIMARY, SECONDARY and RECOVERING nodes that
 *   appear in MD _and_ GR
 * - `member_status.size()` is the sum of all nodes in GR, regardless of if they
 *   show up in MD or not
 * - any nodes in MD but not in GR will be marked as Unavailable, this is an
 *   expected scenario, and we react correctly; they do not increment
 *   `quorum_count` nor `member_status.size()`
 * - any nodes in GR but not in MD will never become routing destinations,
 *   but they will increment the `member_status.size()`, making quorum harder to
 *   reach.
 *
 * To illustrate how our quorum calculation will behave when GR and MD get out
 * sync, below are some example scenarios:
 *
 * ###### Scenario 1
 *
 *     MD defines nodes A, B, C
 *     GR defines nodes A, B, C, D, E
 *     A, B are alive; C, D, E are dead
 *
 * Availability calculation should deem replicaset to be unavailable, because
 * only 2 of 5 nodes are alive, even though looking purely from MD
 * point-of-view, 2 of its 3 nodes are still alive, thus could be considered a
 * quorum. In such case:
 *
 *     quorum_count = 2 (A and B)
 *     member_status.size() = 5
 *
 * and thus:
 *
 *     have_quorum = (2 > 5/2) = false
 *
 * ###### Scenario 2
 *
 *     MD defines nodes A, B, C
 *     GR defines nodes A, B, C, D, E
 *     A, B are dead, C, D, E are alive
 *
 * Availability calculation, if fully GR-aware, could deem replicaset as
 * available, because looking from purely GR perspective, 3 of 5 nodes form
 * quorum. OTOH, looking from MD perspective, only 1 of 3 its nodes (C) is
 * alive.
 *
 * Our availability calculation prefers to err on the side of caution. So here
 * the availability is judged as not available, even though it could be. But
 * that's the price we pay in exchange for the safety the algorithm provides
 * demonstrated in the previous scenario:
 *
 *     quorum_count = 1 (C)
 *     member_status.size() = 5
 *
 * and thus:
 *
 *     have_quorum = (1 > 5/2) = false
 *
 * ###### Scenario 3
 *
 *     MD defines nodes A, B, C
 *     GR defines nodes       C, D, E
 *     A, B are not reported by GR, C, D, E are alive
 *
 * According to GR, there's a quorum between nodes C, D and E. However, from MD
 * point-of-view, A and B went missing and only C is known to be alive.
 *
 * Again, our available calculation prefers to err on the safe side:
 *
 *     quorum_count = 1 (C)
 *     member_status.size() = 5
 *
 * and thus:
 *
 *     have_quorum = (1 > 5/2) = false
 *
 *
 * ##### Why don't we just use GR data (and do away with Metadata)?
 *
 * Need for cluster configuration aside, there is another challenge. GR can
 * provide IP/hostnames of nodes as it sees them from its own perspective, but
 * those IP/hostnames might not be externally-reachable. OTOH, MD tables provide
 * external IP/hostnames which Router relies upon to reach the GR nodes.
 *
 *
 *
 *
 *
 * ### Stage 3: Update routing table
 *
 * Implemented in: `MetadataCache::refresh()`
 *
 * Once stage 2 is complete, the resulting routing table from Stage 2 is
 * applied. It is also compared to the old routing table and if there is a
 * difference between them, two things happen:
 *
 * 1. Appropriate log messages are issued advising of availability change.
 *
 * 2. A check is run if replicaset is in RW mode. If it is, emergency mode is
 *    called off (see "Emergency mode" section for more information).
 *
 *
 *
 *
 *
 * ##NOTES
 *
 * ### Emergency mode
 * [05] Imagine a scenario where a replicaset is perfectly healthy, but Routing
 *      Plugin has a network hickup and fails to connect to one of its nodes. As
 *      a result, it will flag the replicaset as missing a node, triggerring
 *      emergency mode. Emergency mode will only be turned off after routing
 *      table changes (the assumption is that the current one is stale and we're
 *      waiting for an updated one reflecting the problem Routing Plugin
 *      observed). However, since the replicaset is healthy, as long as it stays
 *      that way no such update will come, leaving emergency mode enabled
 *      indefinitely. This has been reported as BUG#27065614
 *
 * [06] Requiring replicaset to be available in RW mode before disabling
 *      emergency mode has a flaw: if replicaset is placed in super-read-only
 *      mode, it is possible for PRIMARY node to be read-only.
 *
 *
 * ### Stage 1.1
 * [01] There has been a recent concern ATTOW, that MD returned might be stale,
 *      if MD server node is in RECOVERING state. This assumes the MD server is
 *      also deployed on an InnoDB cluster.
 *
 * [02] It might be better to always start from the last successfully-connected
 *      server, rather than 1st on the list, to avoid unneccessary connection
 *      attempts when 1st server is dead.
 *
 *
 * ### Stage 1.2
 * [03] If MD-fetching SQL statement fails to execute or process properly, it
 *      will raise an exception that is caught by the topmost catch of the
 *      refresh process, meaning, another MD server will not be queried. This is
 *      a bug, reported as BUG#28082473
 */

#ifdef _WIN32
#define NOMINMAX
#endif

#include "metadata_cache.h"
#include "common.h"
#include "mysql/harness/logging/logging.h"

#include <cassert>
#include <cmath>  // fabs()
#include <memory>
#include <vector>

IMPORT_LOG_FUNCTIONS()

MetadataCache::MetadataCache(
    const std::vector<mysql_harness::TCPAddress> &bootstrap_servers,
    std::shared_ptr<MetaData>
        cluster_metadata,  // this could be changed to UniquePtr
    std::chrono::milliseconds ttl, const mysqlrouter::SSLOptions &ssl_options,
    const std::string &cluster, size_t thread_stack_size)
    : refresh_thread_(thread_stack_size) {
  std::string host;
  for (auto s : bootstrap_servers) {
    metadata_cache::ManagedInstance bootstrap_server_instance;
    host = (s.addr == "localhost" ? "127.0.0.1" : s.addr);
    bootstrap_server_instance.host = host;
    bootstrap_server_instance.port = s.port;
    metadata_servers_.push_back(bootstrap_server_instance);
  }
  ttl_ = ttl;
  cluster_name_ = cluster;
  terminate_ = false;
  meta_data_ = cluster_metadata;
  ssl_options_ = ssl_options;
  refresh();
}

void *MetadataCache::run_thread(void *context) {
  MetadataCache *metadata_cache = static_cast<MetadataCache *>(context);
  metadata_cache->refresh_thread();
  return nullptr;
}

void MetadataCache::refresh_thread() {
  mysql_harness::rename_thread("MDC Refresh");
  log_info("Starting metadata cache refresh thread");

  // this will be only useful if the TTL is set to some value that is more than
  // 1 second
  const std::chrono::milliseconds kTerminateOrForcedRefreshCheckInterval =
      std::chrono::seconds(1);

  while (!terminate_) {
    refresh();

    auto ttl_left = ttl_;
    // wait for up to TTL until next refresh, unless some replicaset loses an
    // online (primary or secondary) server - in that case, "emergency mode" is
    // enabled and we refresh every 1s until "emergency mode" is called off.
    while (ttl_left > std::chrono::milliseconds(0)) {
      if (terminate_) return;

      auto sleep_for =
          std::min(ttl_left, kTerminateOrForcedRefreshCheckInterval);
      std::this_thread::sleep_for(sleep_for);
      ttl_left -= sleep_for;

      {
        std::lock_guard<std::mutex> lock(
            replicasets_with_unreachable_nodes_mtx_);

        if (!replicasets_with_unreachable_nodes_.empty())
          break;  // we're in "emergency mode", don't wait until TTL expires
      }
    }
  }
}

/**
 * Connect to the metadata servers and refresh the metadata information in the
 * cache.
 */
void MetadataCache::start() { refresh_thread_.run(&run_thread, this); }

/**
 * Stop the refresh thread.
 */
void MetadataCache::stop() noexcept {
  terminate_ = true;
  refresh_thread_.join();
}

/**
 * Return a list of servers that are part of a replicaset.
 *
 * @param replicaset_name The replicaset that is being looked up.
 */
std::vector<metadata_cache::ManagedInstance> MetadataCache::replicaset_lookup(
    const std::string &replicaset_name) {
  std::lock_guard<std::mutex> lock(cache_refreshing_mutex_);
  auto replicaset = replicaset_data_.find(replicaset_name);

  if (replicaset == replicaset_data_.end()) {
    log_warning("Replicaset '%s' not available", replicaset_name.c_str());
    return {};
  }
  return replicaset_data_[replicaset_name].members;
}

bool metadata_cache::ManagedInstance::operator==(
    const ManagedInstance &other) const {
  return mysql_server_uuid == other.mysql_server_uuid &&
         replicaset_name == other.replicaset_name && role == other.role &&
         mode == other.mode &&
         std::fabs(weight - other.weight) <
             0.001 &&  // 0.001 = reasonable guess, change if needed
         host == other.host &&
         location == other.location && port == other.port &&
         version_token == other.version_token && xport == other.xport;
}

inline bool compare_instance_lists(const MetaData::ReplicaSetsByName &map_a,
                                   const MetaData::ReplicaSetsByName &map_b) {
  if (map_a.size() != map_b.size()) return false;
  auto ai = map_a.begin();
  auto bi = map_b.begin();
  for (; ai != map_a.end(); ++ai, ++bi) {
    if ((ai->first != bi->first) ||
        (ai->second.members.size() != bi->second.members.size()))
      return false;
    auto a = ai->second.members.begin();
    auto b = bi->second.members.begin();
    for (; a != ai->second.members.end(); ++a, ++b) {
      if (!(*a == *b)) return false;
    }
  }
  return true;
}

static const char *str_mode(metadata_cache::ServerMode mode) {
  switch (mode) {
    case metadata_cache::ServerMode::ReadWrite:
      return "RW";
    case metadata_cache::ServerMode::ReadOnly:
      return "RO";
    case metadata_cache::ServerMode::Unavailable:
      return "n/a";
    default:
      return "?";
  }
}

/**
 * Refresh the metadata information in the cache.
 */
void MetadataCache::refresh() {
  // fetch metadata
  for (auto &metadata_server : metadata_servers_) {
    if (!meta_data_->connect(metadata_server)) {
      log_error("Failed to connect to metadata server %s",
                metadata_server.mysql_server_uuid.c_str());
      continue;
    }
    bool result = fetch_metadata_from_connected_instance();
    if (result) return;  // successfully updated metadata
  }

  // we failed to fetch metadata from any of the metadata servers
  log_error("Failed connecting with any of the metadata servers");
  // clearing metadata
  {
    bool clearing;
    {
      std::lock_guard<std::mutex> lock(cache_refreshing_mutex_);
      clearing = !replicaset_data_.empty();
      if (clearing) replicaset_data_.clear();
    }
    if (clearing) {
      log_info("... cleared current routing table as a precaution");
      on_instances_changed(/*md_servers_reachable=*/false);
    }
  }
}

bool MetadataCache::fetch_metadata_from_connected_instance() {
  try {
    // Fetch the metadata and store it in a temporary variable.
    std::map<std::string, metadata_cache::ManagedReplicaSet>
        replicaset_data_temp = meta_data_->fetch_instances(cluster_name_);
    bool changed = false;

    {
      // Ensure that the refresh does not result in an inconsistency during the
      // lookup.
      std::lock_guard<std::mutex> lock(cache_refreshing_mutex_);
      if (!compare_instance_lists(replicaset_data_, replicaset_data_temp)) {
        replicaset_data_ = replicaset_data_temp;
        changed = true;
      }
    }

    // we want to trigger those actions not only if the metadata has really
    // changed but also when something external (like unsuccessful client
    // connection) triggered the refresh so that we werified if this wasn't
    // false alarm and turn it off if it was
    if (changed) {
      log_info(
          "Potential changes detected in cluster '%s' after metadata refresh",
          cluster_name_.c_str());
      // dump some informational/debugging information about the replicasets
      if (replicaset_data_.empty())
        log_error("Metadata for cluster '%s' is empty!", cluster_name_.c_str());
      else {
        log_info("Metadata for cluster '%s' has %i replicasets:",
                 cluster_name_.c_str(), (int)replicaset_data_.size());
        for (auto &rs : replicaset_data_) {
          log_info(
              "'%s' (%i members, %s)", rs.first.c_str(),
              (int)rs.second.members.size(),
              rs.second.single_primary_mode ? "single-master" : "multi-master");
          for (auto &mi : rs.second.members) {
            log_info("    %s:%i / %i - role=%s mode=%s", mi.host.c_str(),
                     mi.port, mi.xport, mi.role.c_str(), str_mode(mi.mode));

            if (mi.mode == metadata_cache::ServerMode::ReadWrite) {
              // If we were running with a primary or secondary node gone
              // missing before (in so-called "emergency mode"), we trust that
              // the update fixed the problem. This is wrong behavior that
              // should be fixed, see notes [05] and [06] in Notes section of
              // Metadata Cache module in Doxygen.
              std::lock_guard<std::mutex> lock(
                  replicasets_with_unreachable_nodes_mtx_);
              auto rs_with_unreachable_node =
                  replicasets_with_unreachable_nodes_.find(rs.first);
              if (rs_with_unreachable_node !=
                  replicasets_with_unreachable_nodes_.end()) {
                // disable "emergency mode" for this replicaset
                replicasets_with_unreachable_nodes_.erase(
                    rs_with_unreachable_node);
              }
            }
          }
        }
      }

      on_instances_changed(/*md_servers_reachable=*/true);
    }

    /* Not sure about this, the metadata server could be stored elsewhere

    // Fetch the set of servers in the primary replicaset. These servers
    // store the metadata information.
    std::vector<metadata_cache::ManagedInstance> metadata_servers_temp_ =
      replicaset_lookup(cluster_name_);
    // If the metadata replicaset contains servers, replace the current list
    // of metadata servers with the new list.
    if (!metadata_servers_temp_.empty()) {
      std::lock_guard<std::mutex> lock(metadata_servers_mutex_);
      metadata_servers_ = metadata_servers_temp_;
    }*/
  } catch (const std::runtime_error &exc) {
    // fetching the meatadata failed
    log_error("Failed fetching metadata: %s", exc.what());
    return false;
  }

  return true;
}

void MetadataCache::on_instances_changed(const bool md_servers_reachable) {
  std::lock_guard<std::mutex> lock(replicaset_instances_change_callbacks_mtx_);

  for (auto &replicaset_clb : listeners_) {
    const std::string replicaset_name = replicaset_clb.first;
    auto res = replicaset_lookup(replicaset_name);

    for (auto each : listeners_[replicaset_name]) {
      each->notify(res, md_servers_reachable);
    }
  }
}

void MetadataCache::mark_instance_reachability(
    const std::string &instance_id, metadata_cache::InstanceStatus status) {
  // If the status is that the primary or secondary instance is physically
  // unreachable, we enable "emergency mode" (temporarily increase the refresh
  // rate to 1/s if currently lower) until the replicaset routing table
  // reflects this reality (or at least that is the the intent; in practice
  // this mechanism is buggy - see Metadata Cache module documentation in
  // Doxygen, section "Emergency mode")

  std::lock_guard<std::mutex> lock(cache_refreshing_mutex_);
  // the replicaset that the given instance belongs to
  metadata_cache::ManagedInstance *instance = nullptr;
  metadata_cache::ManagedReplicaSet *replicaset = nullptr;
  for (auto &rs : replicaset_data_) {
    for (auto &inst : rs.second.members) {
      if (inst.mysql_server_uuid == instance_id) {
        instance = &inst;
        replicaset = &rs.second;
        break;
      }
    }
    if (replicaset) break;
  }

  // If the instance got marked as invalid we want to trigger metadata-cache
  // update ASAP to aviod keeping try to route to that instance
  if (replicaset) {
    std::lock_guard<std::mutex> lplock(replicasets_with_unreachable_nodes_mtx_);
    switch (status) {
      case metadata_cache::InstanceStatus::Reachable:
        break;
      case metadata_cache::InstanceStatus::InvalidHost:
        log_warning(
            "Instance '%s:%i' [%s] of replicaset '%s' is invalid. Increasing "
            "metadata cache refresh frequency.",
            instance->host.c_str(), instance->port, instance_id.c_str(),
            replicaset->name.c_str());
        replicasets_with_unreachable_nodes_.insert(replicaset->name);
        break;
      case metadata_cache::InstanceStatus::Unreachable:
        log_warning(
            "Instance '%s:%i' [%s] of replicaset '%s' is unreachable. "
            "Increasing metadata cache refresh frequency.",
            instance->host.c_str(), instance->port, instance_id.c_str(),
            replicaset->name.c_str());
        replicasets_with_unreachable_nodes_.insert(replicaset->name);
        break;
      case metadata_cache::InstanceStatus::Unusable:
        break;
    }
  }
}

bool MetadataCache::wait_primary_failover(const std::string &replicaset_name,
                                          int timeout) {
  log_debug("Waiting for failover to happen in '%s' for %is",
            replicaset_name.c_str(), timeout);
  time_t stime = std::time(NULL);
  while (std::time(NULL) - stime <= timeout) {
    {
      std::lock_guard<std::mutex> lock(cache_refreshing_mutex_);
      if (replicasets_with_unreachable_nodes_.count(replicaset_name) == 0) {
        return true;
      }
    }
    std::this_thread::sleep_for(metadata_cache::kDefaultMetadataTTL);
  }
  return false;
}

void MetadataCache::add_listener(
    const std::string &replicaset_name,
    metadata_cache::ReplicasetStateListenerInterface *listener) {
  std::lock_guard<std::mutex> lock(replicaset_instances_change_callbacks_mtx_);
  listeners_[replicaset_name].insert(listener);
}

void MetadataCache::remove_listener(
    const std::string &replicaset_name,
    metadata_cache::ReplicasetStateListenerInterface *listener) {
  std::lock_guard<std::mutex> lock(replicaset_instances_change_callbacks_mtx_);
  listeners_[replicaset_name].erase(listener);
}

/*
  Copyright (c) 2016, 2019, Oracle and/or its affiliates. All rights reserved.

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

#ifndef MYSQLROUTER_METADATA_CACHE_INCLUDED
#define MYSQLROUTER_METADATA_CACHE_INCLUDED

#include <atomic>
#include <exception>
#include <list>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

#include "mysql_router_thread.h"
#include "mysqlrouter/datatypes.h"
#include "mysqlrouter/utils.h"
#include "tcp_address.h"

#ifdef _WIN32
#ifdef metadata_cache_DEFINE_STATIC
#define METADATA_API
#else
#ifdef metadata_cache_EXPORTS
#define METADATA_API __declspec(dllexport)
#else
#define METADATA_API __declspec(dllimport)
#endif
#endif
#else
#define METADATA_API
#endif

namespace metadata_cache {

extern const uint16_t kDefaultMetadataPort;
extern const std::string kDefaultMetadataAddress;
extern const std::string kDefaultMetadataUser;
extern const std::string kDefaultMetadataPassword;
extern const std::chrono::milliseconds kDefaultMetadataTTL;
extern const std::string kDefaultMetadataCluster;
extern const unsigned int kDefaultConnectTimeout;
extern const unsigned int kDefaultReadTimeout;

enum class ReplicasetStatus {
  AvailableWritable,
  AvailableReadOnly,
  UnavailableRecovering,
  Unavailable
};

enum class ServerMode { ReadWrite, ReadOnly, Unavailable };

enum class InstanceStatus {
  Reachable,
  InvalidHost,  // Network connection cannot even be attempted (ie bad IP)
  Unreachable,  // TCP connection cannot be opened
  Unusable      // TCP connection can be opened but session can't be opened
};

/** @class ManagedInstance
 *
 * Class ManagedInstance represents a server managed by the topology.
 */
class METADATA_API ManagedInstance {
 public:
  ManagedInstance() = default;
  ManagedInstance(const std::string &p_replicaset_name,
                  const std::string &p_mysql_server_uuid,
                  const std::string &p_role, const ServerMode p_mode,
                  const float p_weight, const unsigned int p_version_token,
                  const std::string &p_host, const uint16_t p_port,
                  const uint16_t p_xport);

  using TCPAddress = mysql_harness::TCPAddress;
  explicit ManagedInstance(const TCPAddress &addr);
  operator TCPAddress() const;
  bool operator==(const ManagedInstance &other) const;

  /** @brief The name of the replicaset to which the server belongs */
  std::string replicaset_name;
  /** @brief The uuid of the MySQL server */
  std::string mysql_server_uuid;
  /** @brief The role of the server */
  std::string role;
  /** @brief The mode of the server */
  ServerMode mode;
  /** @brief The server weight */
  float weight;
  /** @brief The version token of the server */
  unsigned int version_token;
  /** @brief The host name on which the server is running */
  std::string host;
  /** The port number in which the server is running */
  uint16_t port;
  /** The X protocol port number in which the server is running */
  uint16_t xport;
};

/** @class ManagedReplicaSet
 * Represents a replicaset (a GR group)
 */
class METADATA_API ManagedReplicaSet {
 public:
  /** @brief The name of the replica set */
  std::string name;
#ifdef not_used_yet
  /** @brief The group_name as known to the GR subsystem */
  std::string group_id;
  /** @brief The id of the group view from GR. Changes with topology changes */
  std::string group_view_id;
#endif
  /** @brief List of the members that belong to the group */
  std::vector<metadata_cache::ManagedInstance> members;

  /** @brief Whether replicaset is in single_primary_mode (from PFS) */
  bool single_primary_mode;
};

/** @class connection_error
 *
 * Class that represents all the exceptions thrown while trying to
 * connect with a node managed by the topology.
 *
 */
class connection_error : public std::runtime_error {
 public:
  explicit connection_error(const std::string &what_arg)
      : std::runtime_error(what_arg) {}
};

/** @class metadata_error
 * Class that represents all the exceptions that are thrown while fetching the
 * metadata.
 *
 */
class metadata_error : public std::runtime_error {
 public:
  explicit metadata_error(const std::string &what_arg)
      : std::runtime_error(what_arg) {}
};

/** @class LookupResult
 *
 * Class holding result after looking up data in the cache.
 */
class METADATA_API LookupResult {
 public:
  /** @brief Constructor */
  LookupResult(const std::vector<ManagedInstance> &instance_vector_)
      : instance_vector(instance_vector_) {}

  /** @brief List of ManagedInstance objects */
  const std::vector<metadata_cache::ManagedInstance> instance_vector;
};

/**
 * @brief Abstract class that provides interface for listener on
 *        replicaset status changes.
 *
 *        When state of replicaset is changed, notify function is called.
 */
class METADATA_API ReplicasetStateListenerInterface {
 public:
  /**
   * @brief Callback function that is called when state of replicaset is
   * changed.
   *
   * @param instances allowed nodes
   * @param md_servers_reachable true if metadata changed, false if metadata
   * unavailable
   */
  virtual void notify(const LookupResult &instances,
                      const bool md_servers_reachable) = 0;

  ReplicasetStateListenerInterface() = default;
  // disable copy as it isn't needed right now. Feel free to enable
  // must be explicitly defined though.
  explicit ReplicasetStateListenerInterface(
      const ReplicasetStateListenerInterface &) = delete;
  ReplicasetStateListenerInterface &operator=(
      const ReplicasetStateListenerInterface &) = delete;
  virtual ~ReplicasetStateListenerInterface();
};

/**
 * @brief Abstract class that provides interface for adding and removing
 *        observers on replicaset status changes.
 *
 *        When state of replicaset is changed, then
 * ReplicasetStateListenerInterface::notify function is called for every
 * registered observer.
 */
class METADATA_API ReplicasetStateNotifierInterface {
 public:
  /**
   * @brief Register observer that is notified when there is a change in the
   * replicaset nodes setup/state discovered.
   *
   * @param replicaset_name name of the replicaset
   * @param listener Observer object that is notified when replicaset nodes
   * state is changed.
   *
   * @throw std::runtime_error if metadata cache not initialized
   */
  virtual void add_listener(const std::string &replicaset_name,
                            ReplicasetStateListenerInterface *listener) = 0;

  /**
   * @brief Unregister observer previously registered with add_listener()
   *
   * @param replicaset_name name of the replicaset
   * @param listener Observer object that should be unregistered.
   *
   * @throw std::runtime_error if metadata cache not initialized
   */
  virtual void remove_listener(const std::string &replicaset_name,
                               ReplicasetStateListenerInterface *listener) = 0;

  ReplicasetStateNotifierInterface() = default;
  // disable copy as it isn't needed right now. Feel free to enable
  // must be explicitly defined though.
  explicit ReplicasetStateNotifierInterface(
      const ReplicasetStateNotifierInterface &) = delete;
  ReplicasetStateNotifierInterface &operator=(
      const ReplicasetStateNotifierInterface &) = delete;
  virtual ~ReplicasetStateNotifierInterface();
};

class METADATA_API MetadataCacheAPIBase
    : public ReplicasetStateNotifierInterface {
 public:
  /** @brief Initialize a MetadataCache object and start caching
   *
   * The metadata_cache::cache_init function will initialize a MetadataCache
   * object using the given arguments and store it globally using the given
   * cache_name.
   *
   * Parameters host, port, user, password are used to setup the connection with
   * the metadata server.
   *
   * Cache name given by cache_name can be empty, but must be unique.
   *
   * The parameters connection_timeout and connection_attempts are used when
   * connected to the metadata server.
   *
   * Throws a std::runtime_error when the cache object was already
   * initialized.
   *
   * @param group_replication_id id of the replication group
   * @param metadata_servers The list of cluster metadata servers
   * @param user_credentials MySQL Metadata username and password
   * @param ttl The time to live for the cached data
   * @param ssl_options SSL relatd options for connection
   * @param cluster_name The name of the cluster to be used.
   * @param connect_timeout The time in seconds after which trying to connect
   *                        to metadata server should time out.
   * @param read_timeout The time in seconds after which read from metadata
   *                     server should time out.
   * @param thread_stack_size memory in kilobytes allocated for thread's stack
   * @param use_gr_notifications Flag indicating if the metadata cache should
   *                             use GR notifications as an additional trigger
   *                             for metadata refresh
   */
  virtual void cache_init(
      const std::string &group_replication_id,
      const std::vector<mysql_harness::TCPAddress> &metadata_servers,
      const mysqlrouter::UserCredentials &user_credentials,
      std::chrono::milliseconds ttl, const mysqlrouter::SSLOptions &ssl_options,
      const std::string &cluster_name, int connect_timeout, int read_timeout,
      size_t thread_stack_size = mysql_harness::kDefaultStackSizeInKiloBytes,
      bool use_gr_notifications = false) = 0;

  virtual void instance_name(const std::string &inst_name) = 0;
  virtual std::string instance_name() const = 0;

  virtual bool is_initialized() noexcept = 0;

  /**
   * @brief Start the metadata cache
   */
  virtual void cache_start() = 0;

  /**
   * @brief Teardown the metadata cache
   */
  virtual void cache_stop() noexcept = 0;

  /** @brief Returns list of managed server in a HA replicaset
   * * Returns a list of MySQL servers managed by the topology for the given
   * HA replicaset.
   *
   * @param replicaset_name ID of the HA replicaset
   * @return List of ManagedInstance objects
   */
  virtual LookupResult lookup_replicaset(
      const std::string &replicaset_name) = 0;

  /** @brief Update the status of the instance
   *
   * Called when an instance from a replicaset cannot be reached for one reason
   * or another. When an instance becomes unreachable, an emergency mode is set
   * (the rate of refresh of the metadata cache increases to once per second)
   * and lasts until disabled after a suitable change in the metadata cache is
   * discovered.
   *
   * @param instance_id - the mysql_server_uuid that identifies the server
   * instance
   * @param status - the status of the instance
   */
  virtual void mark_instance_reachability(const std::string &instance_id,
                                          InstanceStatus status) = 0;

  /** @brief Wait until there's a primary member in the replicaset
   *
   * To be called when the master of a single-master replicaset is down and
   * we want to wait until one becomes elected.
   *
   * @param replicaset_name - the name of the replicaset
   * @param timeout - amount of time to wait for a failover, in seconds
   * @return true if a primary member exists
   */
  virtual bool wait_primary_failover(const std::string &replicaset_name,
                                     int timeout) = 0;

  /**
   * @brief Register observer that is notified when there is a change in the
   * replicaset nodes setup/state discovered.
   *
   * @param replicaset_name name of the replicaset
   * @param listener Observer object that is notified when replicaset nodes
   * state is changed.
   */
  virtual void add_listener(const std::string &replicaset_name,
                            ReplicasetStateListenerInterface *listener) = 0;

  /**
   * @brief Unregister observer previously registered with add_listener()
   *
   * @param replicaset_name name of the replicaset
   * @param listener Observer object that should be unregistered.
   */
  virtual void remove_listener(const std::string &replicaset_name,
                               ReplicasetStateListenerInterface *listener) = 0;

  MetadataCacheAPIBase() = default;
  // disable copy as it isn't needed right now. Feel free to enable
  // must be explicitly defined though.
  explicit MetadataCacheAPIBase(const MetadataCacheAPIBase &) = delete;
  MetadataCacheAPIBase &operator=(const MetadataCacheAPIBase &) = delete;
  virtual ~MetadataCacheAPIBase() {}

  struct RefreshStatus {
    uint64_t refresh_failed;
    uint64_t refresh_succeeded;
    std::chrono::system_clock::time_point last_refresh_succeeded;
    std::chrono::system_clock::time_point last_refresh_failed;

    std::string last_metadata_server_host;
    uint16_t last_metadata_server_port;
  };

  virtual RefreshStatus get_refresh_status() = 0;
  virtual std::string group_replication_id() const = 0;
  virtual std::string cluster_name() const = 0;
  virtual std::chrono::milliseconds ttl() const = 0;
};

class METADATA_API MetadataCacheAPI : public MetadataCacheAPIBase {
 public:
  static MetadataCacheAPIBase *instance();

  void cache_init(
      const std::string &group_replication_id,
      const std::vector<mysql_harness::TCPAddress> &metadata_servers,
      const mysqlrouter::UserCredentials &user_credentials,
      std::chrono::milliseconds ttl, const mysqlrouter::SSLOptions &ssl_options,
      const std::string &cluster_name, int connect_timeout, int read_timeout,
      size_t thread_stack_size, bool use_gr_notifications) override;

  void instance_name(const std::string &inst_name) override;
  std::string instance_name() const override;

  std::string group_replication_id() const override;
  std::string cluster_name() const override;
  std::chrono::milliseconds ttl() const override;

  bool is_initialized() noexcept override { return is_initialized_; }
  void cache_start() override;

  void cache_stop() noexcept override;

  LookupResult lookup_replicaset(const std::string &replicaset_name) override;

  void mark_instance_reachability(const std::string &instance_id,
                                  InstanceStatus status) override;

  bool wait_primary_failover(const std::string &replicaset_name,
                             int timeout) override;

  void add_listener(const std::string &replicaset_name,
                    ReplicasetStateListenerInterface *listener) override;
  void remove_listener(const std::string &replicaset_name,
                       ReplicasetStateListenerInterface *listener) override;

  RefreshStatus get_refresh_status() override;

 private:
  std::string inst_name_;

  std::atomic<bool> is_initialized_{false};
  MetadataCacheAPI() {}
  MetadataCacheAPI(const MetadataCacheAPI &) = delete;
  MetadataCacheAPI &operator=(const MetadataCacheAPI &) = delete;
};

}  // namespace metadata_cache

#endif  // MYSQLROUTER_METADATA_CACHE_INCLUDED

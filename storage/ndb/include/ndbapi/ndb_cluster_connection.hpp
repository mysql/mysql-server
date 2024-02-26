/*
   Copyright (c) 2004, 2023, Oracle and/or its affiliates.

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
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/


#ifndef CLUSTER_CONNECTION_HPP
#define CLUSTER_CONNECTION_HPP
#include <ndb_types.h>

class Ndb_cluster_connection_node_iter
{
  friend class Ndb_cluster_connection_impl;
public:
  Ndb_cluster_connection_node_iter() : scan_state(~0),
				       init_pos(0),
				       cur_pos(0) {}
private:
  unsigned char scan_state;
  unsigned char init_pos;
  unsigned char cur_pos;
};

class Ndb;
class NdbWaitGroup;

/**
 * @class Ndb_cluster_connection
 * @brief Represents a connection to a cluster of storage nodes.
 *
 * Any NDB application program should begin with the creation of a
 * single Ndb_cluster_connection object, and should make use of one
 * and only one Ndb_cluster_connection. The application connects to
 * a cluster management server when this object's connect() method is called.
 * By using the wait_until_ready() method it is possible to wait
 * for the connection to reach one or more storage nodes.
 */
class Ndb_cluster_connection {
public:
  /**
   * Create a connection to a cluster of storage nodes
   *
   * @param connectstring The connectstring for where to find the
   *                      management server
   */
  Ndb_cluster_connection(const char * connectstring = 0);

  /**
   * Create a connection to a cluster of storage nodes
   *
   * @param connectstring The connectstring for where to find the
   *                      management server
   * @param force_api_nodeid The nodeid to use for this API node, will
   *                       override any nodeid=<nodeid> specified in
   *                       connectstring
   */
  Ndb_cluster_connection(const char * connectstring, int force_api_nodeid);

#ifndef DOXYGEN_SHOULD_SKIP_INTERNAL
  Ndb_cluster_connection(const char * connectstring,
                         Ndb_cluster_connection *main_connection);
  Ndb_cluster_connection(const char * connectstring,
                         Ndb_cluster_connection *main_connection,
                         int force_api_nodeid);
#endif
  ~Ndb_cluster_connection();

  /**
   * Set data node neighbour of the connection. This will be used for optimal
   * placement of transaction coordinator.
   *
   * In normal cases this method, if used, is called when
   * ndb_cluster_connection is created before query threads are started.
   *
   * Note that this method may change internal state of ndb_cluster_connection
   * shared by all threads using it.  This state is not thread safe and can at
   * the time change occur cause a non optimal node selection.
   *
   * Also any outstanding iterators (Ndb_cluster_connection_node_iter) may
   * become invalid when method is called.  This may result in a non optimal
   * node selection the next time the iterator is used.
   */
  void set_data_node_neighbour(Uint32 neighbour_node);

  /**
   * Set a name on the connection, which will be reported in cluster log
   * and in ndbinfo.processes.
   * For the name to be visible, this must be called prior to connect().
   */
  void set_name(const char *name);

  /**
   * For each Ndb_cluster_connection, NDB publishes a URI in the ndbinfo
   * processes table. A user may customize this URI using set_service_uri().
   *
   * By default the published URI takes the form ndb://x.x.x.x/, where x.x.x.x
   * is the IPv4 address of the node. This default URI has scheme "ndb",
   * port 0, host set to null, and empty path, as described below.
   *
   * If set_service_uri() is called prior to connect(), the URI will be
   * published immediately upon connection. If called after the cluster
   * connection is established, the URI will be published after a delay
   * of up to HeartbeatIntervalDbApi msec.
   *
   * @param scheme The URI scheme. The scheme may contain only lowercase
   *   letters, numbers, and the characters ".", "+", and "-".
   *   It will be truncated to 16 characters.
   * @param host The URI network address or hostname.
   *   Host will be truncated to 48 characters, which is sufficient space for
   *   an IPv6 network address, but not necessarily for a domain name.
   *   If host is null, each data node will report the network address from
   *   its own connection to this node. An Ndb_cluster_connection that uses
   *   a variety of transporters or network addresses to connect to different
   *   data nodes will appear in multiple rows of the ndbinfo.processes
   *   table.
   * @param port The URI port. If 0, no port component will not be published.
   * @param path The URI path, possibly followed by a query component beginning
   *    with the character "?". The combined path and query will be truncated
   *    to 128 characters. It may not begin with a double slash.
   *
   * @return 0 on success, 1 on syntax error in scheme or path component
   */
  int set_service_uri(const char * scheme, const char * host, int port,
                      const char * path);

  /**
   * Set timeout
   *
   * Used as a timeout when talking to the management server,
   * helps limit the amount of time that we may block when connecting
   *
   * Basically just calls ndb_mgm_set_timeout(h,ms).
   *
   * The default is 30 seconds.
   *
   * @param timeout_ms millisecond timeout. As with ndb_mgm_set_timeout,
   *                   only increments of 1000 are really supported,
   *                   with not too many guarantees about calls completing
   *                   in any hard amount of time.
   * @return 0 on success
   */
  int set_timeout(int timeout_ms);

  /**
   * Connect to a cluster management server
   *
   * @param no_retries specifies the number of retries to attempt
   *        in the event of connection failure; a negative value 
   *        will result in the attempt to connect being repeated 
   *        indefinitely
   *
   * @param retry_delay_in_seconds specifies how often retries should
   *        be performed
   *
   * @param verbose specifies if the method should print a report of its progress
   *
   * @return 0 = success, 
   *         1 = recoverable error,
   *        -1 = non-recoverable error
   */
  int connect(int no_retries=30, int retry_delay_in_seconds=1, int verbose=0);

#ifndef DOXYGEN_SHOULD_SKIP_INTERNAL
  int start_connect_thread(int (*connect_callback)(void)= 0);
#endif

  /**
   * Wait until the requested connection with one or more storage nodes is successful
   *
   * @param timeout_for_first_alive   Number of seconds to wait until
   *                                  first live node is detected
   * @param timeout_after_first_alive Number of seconds to wait after
   *                                  first live node is detected
   *
   * @return = 0 all nodes live,
   *         > 0 at least one node live,
   *         < 0 error
   */
  int wait_until_ready(int timeout_for_first_alive,
		       int timeout_after_first_alive);

  /**
   * Lock creation of ndb-objects
   *   Needed to iterate over created ndb objects
   */
  void lock_ndb_objects() const;

  /**
   * Unlock creation of ndb-objects
   */
  void unlock_ndb_objects() const;

  /**
   * Iterator of ndb-objects
   * @param p Pointer to last returned ndb-object
   *          NULL - returns first object
   * @note lock_ndb_objects should be used before using this function
   *       and unlock_ndb_objects should be used after
   */
  const Ndb* get_next_ndb_object(const Ndb* p);
  
  int get_latest_error() const;
  const char *get_latest_error_msg() const;

  /**
   * Enable/disable auto-reconnect
   * @param value 0 = false, 1 = true
   */
  void set_auto_reconnect(int value);
  int get_auto_reconnect() const;

  /**
   *  Get system.name from cluster configuration
   */
  const char * get_system_name() const;

  /**
   * Collect client statistics for all Ndb objects in this connection
   * Note that this locks the ndb objects while collecting data.
   *
   * See Ndb::ClientStatistics for suggested array size and offset
   * meanings
   *
   * @param statsArr   Pointer to array of Uint64 values for stats
   * @param sz         Size of array
   * @return Number of stats array values written
   */
  Uint32 collect_client_stats(Uint64* statsArr, Uint32 sz);

 /**
  * Get/set the minimum time in milliseconds that can lapse until the adaptive 
  * send mechanism forces all pending signals to be sent. 
  * The default value is 10, and the allowed range is from 1 to 10.
  */
 void set_max_adaptive_send_time(Uint32 milliseconds);
 Uint32 get_max_adaptive_send_time();

  /**
   * Configuration handling of the receiver thread(s).
   * We can set the number of receiver threads, we can set the cpu to bind
   * the receiver thread to. We can also set the level of when we activate
   * the receiver thread as the receiver, before this level the normal
   * user threads are used to receive signals. If we set the level to
   * 16 or higher we will never use receive threads as receivers.
   *
   * Note that level 0 is a special value which will always keep the
   * receive thread active, *and* allow it to keep the poll right
   * for its own exclusive usage. Thus user threads will effectively
   * be blocked from being receiver. For this setting care should be
   * taken to ensure that the receive thread will not compete with the
   * user thread for CPU resources. It should preferably be locked
   * to a CPU for its own exclusive usage.
   *
   * By default we have one receiver thread, this thread is not locked to
   * any specific CPU and the level is 8.
   * 
   * The number of receive threads can only be set at a time before the
   * connect call is made to connect to the other nodes in the cluster.
   * The other methods can be called at any time.
   * Currently we don't support setting number of receive threads to anything
   * else than 1 and no config variable for setting is implemented yet.
   *
   * All methods return -1 as an error indication
   */
  int set_num_recv_threads(Uint32 num_recv_threads);
  int get_num_recv_threads() const;
  int unset_recv_thread_cpu(Uint32 recv_thread_id);
  int set_recv_thread_cpu(Uint16 cpuid)
  {
    Uint16 cpuid2 = cpuid;
    return set_recv_thread_cpu(&cpuid2, 1U);
  }
  int set_recv_thread_cpu(Uint16 *cpuid_array,
                          Uint32 array_len,
                          Uint32 recv_thread_id = 0);
  int set_recv_thread_activation_threshold(Uint32 threshold);
  int get_recv_thread_activation_threshold() const;

#ifndef DOXYGEN_SHOULD_SKIP_INTERNAL
  int get_no_ready();
  const char *get_connectstring(char *buf, int buf_sz) const;
  int get_connected_port() const;
  const char *get_connected_host() const;

  // Get generation of the configuration used to configure the NdbApi
  Uint32 get_config_generation() const;

  void set_optimized_node_selection(int val);

  unsigned no_db_nodes();
  unsigned max_api_nodeid() const;
  unsigned max_nodegroup();
  unsigned node_id();
  unsigned get_connect_count() const;
  unsigned get_min_db_version() const;
  unsigned get_min_api_version() const;

  void init_get_next_node(Ndb_cluster_connection_node_iter &iter);
  unsigned int get_next_node(Ndb_cluster_connection_node_iter &iter);
  unsigned int get_next_alive_node(Ndb_cluster_connection_node_iter &iter);
  unsigned get_active_ndb_objects() const;

  Uint64 *get_latest_trans_gci();
  NdbWaitGroup * create_ndb_wait_group(int size);
  bool release_ndb_wait_group(NdbWaitGroup *);

  /**
   * wait for nodes in list to get connected...
   * @return #nodes connected, or -1 on error
   */
  int wait_until_ready(const int * nodes, int cnt, int timeout);
#endif

private:
  friend class Ndb;
  friend class NdbImpl;
  friend class Ndb_cluster_connection_impl;
  friend class SignalSender;
  friend class NdbWaitGroup;
  friend class NDBT_Context;
  class Ndb_cluster_connection_impl & m_impl;
  Ndb_cluster_connection(Ndb_cluster_connection_impl&);

  Ndb_cluster_connection(const Ndb_cluster_connection&); // Not impl.
  Ndb_cluster_connection& operator=(const Ndb_cluster_connection&);
};

#endif

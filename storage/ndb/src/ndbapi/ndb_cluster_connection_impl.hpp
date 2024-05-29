/*
   Copyright (c) 2004, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef CLUSTER_CONNECTION_IMPL_HPP
#define CLUSTER_CONNECTION_IMPL_HPP

#include <NdbMutex.h>
#include <NodeBitmask.hpp>
#include <Vector.hpp>
#include <cstdint>
#include <ndb_cluster_connection.hpp>
#include "DictCache.hpp"
#include "kernel/ndb_limits.h"

extern NdbMutex *g_ndb_connection_mutex;

class TransporterFacade;
class ConfigRetriever;
struct NdbThread;
struct ndb_mgm_configuration;
class Ndb;

extern "C" {
void *run_ndb_cluster_connection_connect_thread(void *);
}

struct NdbApiConfig {
  NdbApiConfig()
      : m_scan_batch_size(MAX_SCAN_BATCH_SIZE),
        m_batch_byte_size(SCAN_BATCH_SIZE),
        m_batch_size(DEF_BATCH_SIZE),
        m_waitfor_timeout(120000),
        m_default_queue_option(0),
        m_default_hashmap_size(0),
        m_verbose(0) {}

  Uint32 m_scan_batch_size;
  Uint32 m_batch_byte_size;
  Uint32 m_batch_size;
  Uint32 m_waitfor_timeout;  // in milli seconds...
  Uint32 m_default_queue_option;
  Uint32 m_default_hashmap_size;
  Uint32 m_verbose;
};

class Ndb_cluster_connection_impl : public Ndb_cluster_connection {
  Ndb_cluster_connection_impl(const char *connectstring,
                              Ndb_cluster_connection *main_connection,
                              int force_api_nodeid);
  ~Ndb_cluster_connection_impl();

  void do_test();

  void init_get_next_node(Ndb_cluster_connection_node_iter &iter);
  Uint32 get_next_node(Ndb_cluster_connection_node_iter &iter);
  Uint32 get_next_alive_node(Ndb_cluster_connection_node_iter &iter);

  inline unsigned get_connect_count() const;
  inline unsigned get_min_db_version() const;
  inline unsigned get_min_api_version() const;

 public:
  inline Uint64 *get_latest_trans_gci() { return &m_latest_trans_gci; }

 private:
  friend class Ndb;
  friend class NdbImpl;
  friend class NdbWaitGroup;
  friend void *run_ndb_cluster_connection_connect_thread(void *);
  friend class Ndb_cluster_connection;
  friend class NdbEventBuffer;
  friend class SignalSender;
  friend class NDBT_Context;

  static Int32 const MAX_PROXIMITY_GROUP = INT32_MAX;
  static Int32 const INVALID_PROXIMITY_GROUP = INT32_MIN;
  static Int32 const DATA_NODE_NEIGHBOUR_PROXIMITY_ADJUSTMENT = -30;
  static Uint32 const HINT_COUNT_BITS = 10;
  static Uint32 const HINT_COUNT_HALF = (1 << (HINT_COUNT_BITS - 1));
  static Uint32 const HINT_COUNT_MASK =
      (HINT_COUNT_HALF | (HINT_COUNT_HALF - 1));

  struct Node {
    Node(Uint32 _g = 0, Uint32 _id = 0)
        : this_group_idx(0),
          next_group_idx(0),
          config_group(_g),  // between 0 and 200
          adjusted_group(_g),
          id(_id),
          hint_count(0) {}
    Uint32 this_group_idx;  // First index of node with same group
    Uint32 next_group_idx;  // Next index of node not with same node, or 0.
    Uint32 config_group;    // Proximity group from cluster connection config
    Int32 adjusted_group;   // Proximity group adjusted via ndbapi calls
    Uint32 id;
    /**
     * Counts how many times node was chosen for hint when
     * more than one were possible.
     */
    Uint32 hint_count;
  };

  NdbNodeBitmask m_db_nodes;
  NdbMutex *m_nodes_proximity_mutex;
  Vector<Node> m_nodes_proximity;
  Uint16 m_location_domain_id[MAX_NODES];
  Uint32 m_my_node_id;
  Uint32 m_max_api_nodeid;
  Uint32 m_my_location_domain_id;
  int init_nodes_vector(Uint32 nodeid, const ndb_mgm_configuration *config);
  int configure(Uint32 nodeid, const ndb_mgm_configuration *config);
  void connect_thread();
  void set_name(const char *name);
  void configure_tls(const char *search_path, int mgm_tls_level);
  const char *get_tls_certificate_path() const;
  int set_service_uri(const char *, const char *, int, const char *);
  void set_data_node_neighbour(Uint32 neighbour_node);
  void adjust_node_proximity(Uint32 node_id, Int32 adjustment);
  Uint32 get_db_nodes(Uint8 nodesarray[MAX_NDB_NODES]) const;
  Uint32 get_unconnected_nodes() const;

  /**
   * Select the "closest" node
   */
  Uint32 select_node(NdbImpl *impl_ndb, const Uint16 *nodes, Uint32 cnt);
  /**
   * Select primary or if primary in other location domain
   * choose a node in the same location domain
   */
  Uint32 select_location_based(NdbImpl *impl_ndb, const Uint16 *nodes,
                               Uint32 cnt);
  /**
   * Choose node in same location domain if one exists, otherwise
   * make no choice.
   */
  Uint32 select_any(NdbImpl *impl_ndb);

  int connect(int no_retries, int retry_delay_in_seconds, int verbose);

  Ndb_cluster_connection *m_main_connection;
  GlobalDictCache *m_globalDictCache;
  TransporterFacade *m_transporter_facade;
  ConfigRetriever *m_config_retriever;
  NdbThread *m_connect_thread;
  int (*m_connect_callback)(void);

  int m_conn_default_optimized_node_selection;
  int m_run_connect_thread;
  NdbMutex *m_event_add_drop_mutex;
  Uint64 m_latest_trans_gci;

  NdbMutex *m_new_delete_ndb_mutex;
  NdbCondition *m_new_delete_ndb_cond;
  Ndb *m_first_ndb_object;
  Uint64 link_ndb_object(Ndb *);
  void unlink_ndb_object(Ndb *);

  BaseString m_latest_error_msg;
  unsigned m_latest_error;

  // Scan batch configuration parameters
  NdbApiConfig m_ndbapiconfig;

  // Avoid transid reuse with Block ref reuse
  Vector<Uint32> m_next_transids;
  Uint32 get_next_transid(Uint32 reference) const;
  void set_next_transid(Uint32 reference, Uint32 value);

  // Closest data node neighbour
  Uint32 m_data_node_neighbour;

  // Number of Ndb object creations
  Uint64 m_num_created_ndb_objects;

  // Base offset for stats, from Ndb objects that are no
  // longer with us
  Uint64 globalApiStatsBaseline[Ndb::NumClientStatistics];

  NdbWaitGroup *m_multi_wait_group;

  // Service URI in ndbinfo.processes
  BaseString m_uri_scheme, m_uri_host, m_uri_path;
  int m_uri_port;

  // system.name copied from configuration
  BaseString m_system_name;

  // Config generation of used configuration
  Uint32 m_config_generation{0};

  // TLS Certificate Search Path
  const char *m_tls_search_path{nullptr};

  // Some connection requires TLS
  bool m_tls_requirement{false};
};

#endif

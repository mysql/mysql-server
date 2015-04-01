/*
 Copyright (c) 2011, 2015, Oracle and/or its affiliates. All rights
 reserved.
 
 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; version 2 of
 the License.
 
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 02110-1301  USA
 */
#ifndef NDBMEMCACHE_CONFIGURATION_H
#define NDBMEMCACHE_CONFIGURATION_H

#ifndef __cplusplus
#error "This file is for C++ only"
#endif

#include <string.h>

#include <NdbApi.hpp>

#include "ClusterConnectionPool.h"
#include "KeyPrefix.h"
#include "QueryPlan.h"

/********************
 NDB Configuration.
 
 The Configuration class contains a configuration and supplies it via a 
 public API.  
 
 A set of configuration reader classes, config_v0, config_v1, etc. read 
 the configuration from its backing store.  config_v0 simply constructs 
 a hard-coded config used in the first prototype.  config_v1 reads a 
 configuration from a particular set of tables in the "ndbmemcache" schema 
 on the primary cluster.  This metadata defines key prefixes and possible
 connections to additional clusters. 
 
 The configuration readers provide a single public method, read_configuration().  
 The readers use the two protected API methods of Configuration,
 storeConnection() and storePrefix(), to store the configuration there.
*********************/


// Forward declarations for Configuration and all config readers:
class Configuration;
class config_v0;
class config_v1;
class config_v1_0;
class config_v1_1;
class config_v1_2;

enum config_ver_enum {
  CONFIG_VER_UNKNOWN, 
  CONFIG_VER_UNSUPPORTED,
  CONFIG_VER_0,
  CONFIG_VER_1_0,
  CONFIG_VER_1_1,
  CONFIG_VER_1_2
};
 
class Configuration {
  friend class config_v0;
  friend class config_v1;
  friend class config_v1_0;
  friend class config_v1_1;
  friend class config_v1_2;
  
  public:
  /* public methods */
  Configuration() : nclusters(0), nprefixes(0), 
                    config_version(CONFIG_VER_UNKNOWN) {}; 
  Configuration(Configuration *);
  
  bool connectToPrimary();
  bool openAllConnections();
  bool readConfiguration();
  void disconnectAll();
  ClusterConnectionPool * getConnectionPoolById(int cluster_id) const;// inlined
  const KeyPrefix * getDefaultPrefix() const;                         // inlined
  const KeyPrefix * getPrefixForKey(const char *key, int nkey) const;
  const KeyPrefix * getPrefixByInfo(const prefix_info_t info) const;
  const KeyPrefix * getPrefix(int id) const;                          // inlined
  const KeyPrefix * getNextPrefixForCluster(uint cluster_id, const KeyPrefix *) const;
  void setPrimaryConnectString(const char *);                         // inlined
  void setServerRole(const char *);                                   // inlined
  const char * getServerRole();                                       // inlined
  bool prefetchDictionary();
  double figureInFlightTransactions(int cluster_id) const;
  bool canReloadOnline() const;
  int waitForReconfSignal();
    
  /* public instance variables */

  unsigned int generation;   /* Set by GlobalConfigManager */
  unsigned int nclusters;
  unsigned int nprefixes;
  unsigned int max_tps;      /* target throughput, transactions per second */
  struct {
    uint64_t for_ndb_engine;
    uint64_t for_default_engine;
  } initial_cas;

  protected:
  /* The protected methods and variables are shared with the config_XXX 
     classes */
  int storeConnection(const char *connectstring, unsigned int usec_rtt);
  int storePrefix(KeyPrefix &prefix);
  void storeCAS(uint64_t ndb_engine_cas, uint64_t default_engine_cas);

  const char *primary_connect_string;
  int onlineReloadFlag;
  int (*reload_waiter)(Ndb_cluster_connection *, const char *);

  private:
  /* private methods */
  void store_default_prefix();
  config_ver_enum get_supported_version();
  bool fetch_meta_record(QueryPlan *plan, Ndb *db, const char *version);
  
  /* private instance variables */
  const char *server_role;
  config_ver_enum config_version;
  Ndb_cluster_connection *primary_conn;
  const char * connect_strings[MAX_CLUSTERS];
  KeyPrefix * prefixes[MAX_KEY_PREFIXES];
};


class config_v0 {
public:
  /* Methods */
  config_v0(Configuration * cf) : conf(*cf) {};
  bool read_configuration();
  
  /* Instance variables */
  Configuration &conf;  
};


/* Inline methods */

inline void Configuration::setServerRole(const char *role) {
  server_role = role;
};

inline const char * Configuration::getServerRole() {
  return server_role;
};

inline void Configuration::setPrimaryConnectString(const char *mgm) {
  primary_connect_string = mgm;
};

inline ClusterConnectionPool * Configuration::getConnectionPoolById(int id) const 
{
  return get_connection_pool_for_cluster(connect_strings[id]);
};

inline const KeyPrefix * Configuration::getDefaultPrefix() const {
  return prefixes[0];
};

inline const KeyPrefix * Configuration::getPrefix(int id) const {
  return prefixes[id];
};

inline bool Configuration::canReloadOnline() const {
  return (bool) onlineReloadFlag;
}


/* Singleton Factory Method; implemented in ndb_configuration.cc */
Configuration & get_Configuration();

#endif

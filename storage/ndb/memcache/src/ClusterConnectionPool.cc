/*
 Copyright (c) 2011, 2014, Oracle and/or its affiliates. All rights
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

#include <my_config.h>
#include <stdio.h>
#include <assert.h>

/* C++ files must define __STDC_FORMAT_MACROS in order to get PRIu64 */
#define __STDC_FORMAT_MACROS 
#include <inttypes.h>
#include <pthread.h>

#include <memcached/extension_loggers.h>


#include "debug.h"
#include "ClusterConnectionPool.h"
#include "LookupTable.h"

#include <portlib/NdbSleep.h>

extern EXTENSION_LOGGER_DESCRIPTOR *logger;

LookupTable<ClusterConnectionPool> * conn_pool_map = 0;
pthread_mutex_t conn_pool_map_lock = PTHREAD_MUTEX_INITIALIZER;

extern struct hash_ops string_to_pointer_hash;  //  from config_v1.cc


ClusterConnectionPool *get_connection_pool_for_cluster(const char * name) {
  ClusterConnectionPool *p = 0;
  if(conn_pool_map != 0) {
    if(name == 0) name = "[default]";
    if(pthread_mutex_lock(& conn_pool_map_lock) == 0) {
      p = conn_pool_map->find(name);
      pthread_mutex_unlock(& conn_pool_map_lock);
    }
  }
  return p;
}


void store_connection_pool_for_cluster(const char *name, 
                                       ClusterConnectionPool *p) {
  DEBUG_ENTER();
  if(name == 0) name = "[default]";
  // int name_len = strlen(name);

  if(pthread_mutex_lock(& conn_pool_map_lock) == 0) {
    if(conn_pool_map == 0) {
      conn_pool_map = new LookupTable<ClusterConnectionPool>();
    }

    assert(conn_pool_map->find(name) == 0);
    conn_pool_map->insert(name, p);
    pthread_mutex_unlock(& conn_pool_map_lock);
  }
}                                       


ClusterConnectionPool::ClusterConnectionPool(const char *s) : 
  connect_string(s) , 
  main_conn(0),  
  pool_size(0),
  custom_data_ptr(0)
{
  for(int i = 0 ; i < MAX_CONNECT_POOL ; i++) {
    pool_connections[i] = 0;
  }
  store_connection_pool_for_cluster(s, this);
}


/* connect() is a static method. 
   The usage pattern is this:
   Use ClusterConnectionPool::connect() to get an Ndb_cluster_connection.
   Then instantiate a connection pool.
   Then use setMainConnection() to use the existing connection in the pool.
*/

Ndb_cluster_connection * ClusterConnectionPool::connect(const char *connectstring) {
  DEBUG_ENTER_METHOD("ClusterConnectionPool::connect");
  int conn_retries = 0;
  if(connectstring == 0) connectstring = "";
  Ndb_cluster_connection *c = new Ndb_cluster_connection(connectstring);
  
  /* Set name that appears in the cluster log file */
  c->set_name("memcached");
  
  while(1) {
    conn_retries++;
    int r = c->connect(2,1,0);
    if(r == 0)         // success 
      break;
    else if(r == -1)   // unrecoverable error
      return NULL;
    else if (r == 1) { // recoverable error
      if(conn_retries == 5)
        return NULL;
      else {
        // sleep 1 second
        NdbSleep_MilliSleep(1000);
      }
    }
  }
  
  int ready_nodes = c->wait_until_ready(5, 5);
  if(ready_nodes < 0) {
    logger->log(LOG_WARNING, 0, 
                "Timeout waiting for cluster \"%s\" to become ready (%d).\n", 
                connectstring, ready_nodes);
    return NULL;
  }
  
  logger->log(LOG_WARNING, 0, "Connected to \"%s\" as node id %d.\n", 
              connectstring, c->node_id());
  if(ready_nodes > 0) 
    logger->log(LOG_WARNING, 0, "Only %d storage nodes are ready.\n", ready_nodes);
  fflush(stderr);  /* so we get the message ASAP */
  
  return c;
}


/* Destructor.
   Delete the main connection and all the others.
   If this cluster is the primary cluster, beware!
   The configuration may have a pointer to it (primary_conn) that will be
   invalidated.
*/
ClusterConnectionPool::~ClusterConnectionPool() {
  for(unsigned int i = 0 ; i < pool_size ; i++) {
    if(pool_connections[i]) {
      delete pool_connections[i];
      pool_connections[i] = 0;
    }
  }

  /* pool_connections[0] was the main connection -- don't delete it again :) */
}


/* addPooledConnection() 
*/
Ndb_cluster_connection * ClusterConnectionPool::addPooledConnection() {
  DEBUG_ENTER_METHOD("ClusterConnectionPool::addPooledConnection");
  Ndb_cluster_connection *conn;
  
  if(pool_size >= MAX_CONNECT_POOL) return 0;  

  conn = connect(connect_string);
  
  if(conn && conn->node_id()) {
    pool_connections[pool_size++] = conn;
  }
  else {
    logger->log(LOG_WARNING, 0, "   Failed to grow connection pool.\n");
    delete conn;
    conn = 0;
  }
  return conn;
}


void ClusterConnectionPool::add_stats(const char *prefix,
                                      ADD_STAT add_stat, 
                                      const void *cookie) {
  char key[128];
  char val[128];
  int klen, vlen;
  Uint64 ndb_stats[Ndb::NumClientStatistics];
  DEBUG_ENTER();

  Ndb db(main_conn);
  
  for(unsigned int i = 0 ; i < pool_size ; i++) {
    pool_connections[i]->collect_client_stats(ndb_stats, Ndb::NumClientStatistics);
  
    for(int s = 0 ; s < Ndb::NumClientStatistics ; s++) {
      klen = sprintf(key, "%s_conn%d_%s", prefix, i, db.getClientStatName(s));
      vlen = sprintf(val, "%llu", ndb_stats[s]);
      add_stat(key, klen, val, vlen, cookie);
    }
  }
}

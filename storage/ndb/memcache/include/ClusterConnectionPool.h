/*
 Copyright (c) 2011, 2017, Oracle and/or its affiliates. All rights reserved.
 
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
#ifndef NDBMEMCACHE_CLUSTERCONNECTIONPOOL_H
#define NDBMEMCACHE_CLUSTERCONNECTIONPOOL_H

#define MAX_CONNECT_POOL 4

#include <memcached/engine.h>

#include <NdbApi.hpp>

#include "NdbInstance.h"

class ClusterConnectionPool;

/*  ClusterConnectionPool.cc keeps a global hash table of
 *  ClusterConnectionPool instances, keyed by connect string 
 */  
ClusterConnectionPool * get_connection_pool_for_cluster(const char * connect_string);


class ClusterConnectionPool {

/* public instance variables */
public:
  const char * const connect_string;   /*< cluster connect string */
  unsigned int usec_rtt;         /*< estimated newtork latency within cluster */

/* private instance variables */
private:
  Ndb_cluster_connection *main_conn;                                      
  unsigned int pool_size;                  
  Ndb_cluster_connection * pool_connections[MAX_CONNECT_POOL];
  void * custom_data_ptr;

/* public class methods */
public:
  static Ndb_cluster_connection * connect(const char *connectstring);

/* public instance methods */
public:
  ClusterConnectionPool(const char *s = 0);
  ~ClusterConnectionPool();  
  void setMainConnection(Ndb_cluster_connection *);                  // inlined
  Ndb_cluster_connection *getMainConnection() const      { return main_conn; };
  unsigned int getPoolSize() const                       { return pool_size; }; 

  /** After startup time, create an additional connection in the pool.
      Returns the new connection
  */
  Ndb_cluster_connection * addPooledConnection();

  /** Get the connection numbered "my_id modulo pool_size" */
  Ndb_cluster_connection *getPooledConnection(int my_id) const;      // inlined
  
  /** Get aggregated NDB API client statistics */
  void add_stats(const char *, ADD_STAT, const void *);

  /** Set/get a user-supplied pointer */
  void setCustomData(void *p);
  void * getCustomData();
};


/* Inline functions */

inline Ndb_cluster_connection * ClusterConnectionPool::getPooledConnection(int i) const {
  return pool_connections[i % pool_size];
}

inline void ClusterConnectionPool::setCustomData(void *p) {
  custom_data_ptr = p;
}

inline void * ClusterConnectionPool::getCustomData(void) {
  return custom_data_ptr;
}

#endif

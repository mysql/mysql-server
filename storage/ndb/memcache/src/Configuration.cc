/*
 Copyright (c) 2011, 2019, Oracle and/or its affiliates. All rights reserved.
 
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
#include "my_config.h"
#include <unistd.h>
#include <stdlib.h>  
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <time.h>

#include "ndb_version.h"
#include "NdbApi.hpp"

#include <memcached/extension_loggers.h>
#include <memcached/util.h>

#include "ndbmemcache_global.h"
#include "debug.h"
#include "timing.h"
#include "Configuration.h"
#include "Config_v1.h"
#include "TableSpec.h"
#include "QueryPlan.h"
#include "Operation.h"
#include "ndb_error_logger.h"

extern EXTENSION_LOGGER_DESCRIPTOR *logger;


/// TODO:  You can't actually use config v0 unless mc_backstore exists.


/*******************  Configuration **************************/
/*** Public API ***/

Configuration::Configuration(Configuration *old) :
  nclusters(0), 
  nprefixes(0), 
  primary_connect_string(old->primary_connect_string),
  server_role(old->server_role),
  config_version(CONFIG_VER_UNKNOWN),
  primary_conn(old->primary_conn)  {}


bool Configuration::connectToPrimary() {
  char timestamp[40];
  time_t now;
  tm tm_buf;
  
  /* Build the startup timestamp for the log message */
  time(&now);
  localtime_r(&now, &tm_buf);
  strftime(timestamp, sizeof(timestamp),
           "%d-%b-%Y %T %Z", &tm_buf);
  
  /* ndb_init() must be the first call into the NDB API */
  ndb_init();
  
  /* Connect to the primary cluster */
  logger->log(LOG_WARNING, 0, 
              "%s NDB Memcache %s started [NDB %d.%d.%d; MySQL %d.%d.%d]\n" 
              "Contacting primary management server (%s) ... \n", timestamp, 
              VERSION, NDB_VERSION_MAJOR, NDB_VERSION_MINOR, NDB_VERSION_BUILD,
              NDB_MYSQL_VERSION_MAJOR, NDB_MYSQL_VERSION_MINOR, 
              NDB_MYSQL_VERSION_BUILD, primary_connect_string);
              
  primary_conn = ClusterConnectionPool::connect(primary_connect_string);

  if(primary_conn) {    
    return true;
  }
  else {
    logger->log(LOG_WARNING, 0, "FAILED.\n");
    return false;
  }
}


bool Configuration::openAllConnections() {
  DEBUG_ENTER_METHOD("Configuration::openAllConnections");
  Ndb_cluster_connection *conn;
  unsigned int n_open = 0;

  for(unsigned int i = 0; i < nclusters ; i++) {
    ClusterConnectionPool *pool = getConnectionPoolById(i);

    /* if the connect string is NULL, or empty, or identical to the primary 
       connect string, then just copy the pointer to the primary connection */
    if((pool->connect_string == 0) 
      || (* (pool->connect_string) == 0)
      || (!strcmp(pool->connect_string, primary_connect_string)))
    {
      conn = primary_conn;
    }
    else 
    {
      conn = ClusterConnectionPool::connect(pool->connect_string);
    }
    pool->setMainConnection(conn);
    if(conn) n_open++;
  }

  return (n_open == nclusters);
}


/* Prefetch dictionary objects over the network into the local cache (which
   belongs to each Ndb_cluster_connection) now, so clients don't have to wait
   for them to be fetched at runtime.
*/
bool Configuration::prefetchDictionary() {
  DEBUG_ENTER_METHOD("Configuration::prefetchDictionary");
  unsigned int ok = 0;
  for(unsigned int i = 0 ; i < nprefixes ; i++) {
    /* Instantiate an Ndb and a QueryPlan, then discard them. 
       The QueryPlan constructor will make calls into NdbDictionary's 
       getTable() and getColumn() methods. 
    */ 
    if(prefixes[i]->info.use_ndb) {
      int cl = prefixes[i]->info.cluster_id;
      Ndb thisDb(getConnectionPoolById(cl)->getMainConnection());
      thisDb.init();
      QueryPlan thisPlan(&thisDb, prefixes[i]->table);
      if(thisPlan.initialized) ok++;
      else logger->log(LOG_WARNING, 0, "Error: unable to create a query plan "
                       "for key prefix \"%s\"\n", prefixes[i]->prefix);
    }
    else /* this prefix does not use NDB */
      ok++;
  }  
  return (ok == nprefixes);
}


bool Configuration::readConfiguration() {  
  bool status = false;
  
  if(config_version == CONFIG_VER_UNKNOWN)
    config_version = get_supported_version();
  
  store_default_prefix();

  if(config_version == CONFIG_VER_1_2) {
    config_v1_2 cfg(this);
    status = cfg.read_configuration();
  }
  else if(config_version == CONFIG_VER_1_1) {
    config_v1_1 cfg(this);
    status = cfg.read_configuration();
  }
  else if(config_version == CONFIG_VER_1_0) {
    config_v1_0 cfg(this);
    status = cfg.read_configuration();
  }

  return status;
}
  

const KeyPrefix * Configuration::getPrefixByInfo(const prefix_info_t info) const {
  assert(info.prefix_id < nprefixes);
  return prefixes[info.prefix_id];
}


const KeyPrefix * Configuration::getPrefixForKey(const char *key, int nkey) const {  
  /*  The key prefixes are sorted -- they have been stored in order, so 
      we can conduct this binary search on them.
   */  
  int low = 1;    // The first actual prefix is prefixes[1]
  int high = nprefixes - 1;
  int mid;
  int cmp;
  
  while ( low <= high ) {
    mid = (low + high) / 2;
    cmp = prefixes[mid]->cmp(key, nkey);
    
    if(cmp > 0) 
      high = mid - 1;
    else if (cmp < 0)
      low = mid + 1;
    else
      return prefixes[mid];
  }
  return prefixes[0];  // The special default prefix
}


const KeyPrefix * Configuration::getNextPrefixForCluster(unsigned int cluster_id, 
                                                         const KeyPrefix *k) const {
  unsigned int i = 0;

  if(k) {
    while(prefixes[i] != k && i < nprefixes) i++;  // find k in the list
    i++;  // then advance one more
  }
    
  while(i < nprefixes && prefixes[i]->info.cluster_id != cluster_id) i++;

  if(i >= nprefixes) return 0;
  else return prefixes[i];
}


void Configuration::disconnectAll() {
  DEBUG_ENTER_METHOD(" Configuration::disconnectAll");
  for(unsigned int i = 0; i < nclusters; i++) {
    ClusterConnectionPool *p = getConnectionPoolById(i);
    delete p;
  }
}


double Configuration::figureInFlightTransactions(int cluster_id) const {
  /*  How many NDB objects are needed to meet performance expectations?
      We know max_tps and RTT.   
      We expect a transaction to be in-flight for 5 * RTT, and we need to 
      meet max_tps.
      TO DO: This calculation only works for in-memory data.  If data is on
      disk, we have to figure 5 ms seek times.
   */  
  double tx_time_in_usec =  getConnectionPoolById(cluster_id)->usec_rtt * 5;
  double tx_per_ndb_per_sec = 1000000 / tx_time_in_usec;
  double total_ndb_objects = max_tps / tx_per_ndb_per_sec;
  return total_ndb_objects;
}  


/*******************  Configuration **************************/
/*** Protected API ***/

int Configuration::storePrefix(KeyPrefix &prefix) {
  if(prefix.prefix_len == 0) {
    /* A zero-length prefix replaces the default prefix */
    delete prefixes[0];
    prefixes[0] = new KeyPrefix(prefix);
    return 0;
  }    
  int prefix_id = nprefixes++;
  prefix.info.prefix_id = prefix_id;
  prefixes[prefix_id] = new KeyPrefix(prefix);                    
  if(nprefixes > 2) {
    /* The config reader must store prefixes in ascending string order. */
    assert(strcmp(prefixes[prefix_id]->prefix, prefixes[prefix_id - 1]->prefix) > 0);
  }
  return prefix_id;
}


int Configuration::storeConnection(const char *connectstring, 
                                   unsigned int usec_rtt) {
  int cluster_id = nclusters++;

  ClusterConnectionPool *pool = get_connection_pool_for_cluster(connectstring);
  if(pool == 0) {
    pool = new ClusterConnectionPool(connectstring);
  }
  pool->usec_rtt = usec_rtt; 

  connect_strings[cluster_id] = connectstring;
  return cluster_id;
}


void Configuration::storeCAS(uint64_t ndb_engine_cas, uint64_t default_engine_cas) {
  initial_cas.for_default_engine = default_engine_cas;
  initial_cas.for_ndb_engine = ndb_engine_cas;  
}


/*******************  Configuration **************************/
/*** Private methods ***/

config_ver_enum Configuration::get_supported_version() {

  Ndb db(primary_conn);
  db.init(1);
  TableSpec ts_meta("ndbmemcache.meta", "application,metadata_version", "");
  QueryPlan plan(& db, &ts_meta);
  // "initialized" is set only if the ndbmemcache.meta table exists:
  if(plan.initialized) {
    if(fetch_meta_record(&plan, &db, "1.2")) {
      DEBUG_PRINT("1.2");
      return CONFIG_VER_1_2;
    }
    if(fetch_meta_record(&plan, &db, "1.1")) {
      DEBUG_PRINT("1.1");
      logger->log(LOG_WARNING, 0, "\n"
                  "Configuration schema version 1.1 is installed. To upgrade\n"
                  "to version 1.2, run the update_to_1.2.sql script "
                  "and restart memcached.\n");
      return CONFIG_VER_1_1;
    }
    if(fetch_meta_record(&plan, &db, "1.0")) {
      DEBUG_PRINT("1.0");
      return CONFIG_VER_1_0;
    }
    if(fetch_meta_record(&plan, &db, "1.0a")) {
      DEBUG_PRINT("1.0a");
      logger->log(LOG_WARNING, 0, "\nThe configuration schema from prototype2 is"
                  " no longer supported.\nPlease drop your ndbmemcache database,"
                  " run the new metadata.sql script, and try again.\n\n");
      return CONFIG_VER_UNSUPPORTED;      
    }
  }
  return CONFIG_VER_0;
}  


bool Configuration::fetch_meta_record(QueryPlan *plan, Ndb *db,
                                      const char *version) {
  DEBUG_ENTER_METHOD("Configuration::fetch_meta_record");
  bool result = false;

  Operation op(plan, OP_READ);

  op.key_buffer = (char *) malloc(op.requiredKeyBuffer());
  op.buffer     = (char *) malloc(op.requiredBuffer());
  
  NdbTransaction *tx = db->startTransaction();
  if(tx) {
    op.setKeyPart(COL_STORE_KEY + 0, "ndbmemcache", strlen("ndbmemcache"));
    op.setKeyPart(COL_STORE_KEY + 1, version, strlen(version));
    op.readTuple(tx);
    tx->execute(NdbTransaction::Commit);
    if(tx->getNdbError().classification == NdbError::NoError)
      result = true;
    tx->close();
  }
  else {
    log_ndb_error(db->getNdbError());
  }
  
  free(op.key_buffer);
  free(op.buffer);
  return result;
}


void Configuration::store_default_prefix() {
  KeyPrefix pfx("");
  pfx.table = 0;
  pfx.info.usable = 1;
  
  /* The default prefix has a length of zero and a prefix ID of zero.
     It defines "cache-only" operations with no database access. 
     If the configuration supplies a zero-length prefix config, 
     that config will override this one.
  */
  pfx.info.prefix_id = 0;
  
  /* cache-only reads */
  pfx.info.do_mc_read = 1;
  pfx.info.do_db_read = 0;
  
  /* cache-only writes */
  pfx.info.do_mc_write = 1;
  pfx.info.do_db_write = 0;
  
  /* cache-only deletes */
  pfx.info.do_mc_delete = 1;
  pfx.info.do_db_delete = 0;
  
  /* cache-only flushes */
  pfx.info.do_db_flush = 0;
  
  /* cache-only conditionals */
  pfx.info.has_math_col = 0;
  pfx.info.has_cas_col = 0;
  
  assert(nprefixes == 0);
  prefixes[0] = new KeyPrefix(pfx);  
  nprefixes = 1;
}


int Configuration::waitForReconfSignal() {
  assert(reload_waiter != 0);
  
  return reload_waiter(primary_conn, server_role);
}


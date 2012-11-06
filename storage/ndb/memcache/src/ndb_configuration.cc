/*
 Copyright (c) 2011, Oracle and/or its affiliates. All rights
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
#include <stdio.h>
#include <pthread.h>

#include <memcached/extension_loggers.h>

#include <NdbApi.hpp>

#include "Configuration.h"
#include "ndb_configuration.h"
#include "debug.h"
#include "workitem.h"
#include "NdbInstance.h"
#include "thread_identifier.h"
#include "Scheduler.h"
#include "ExternalValue.h"

/* A static global variable */
extern EXTENSION_LOGGER_DESCRIPTOR *logger;

/* From ndb_pipeline */
extern int workitem_class_id;
extern int workitem_actual_inline_buffer_size;


/* An external Function */
extern "C" {
  void cache_set_initial_cas_id(uint64_t cas);    /* In cache-src/items.c */
}

Configuration * active_config = 0;
Configuration * next_config = 0;
Configuration * stale_config = 0;

/* This function has C++ linkage */
Configuration & get_Configuration() {
  return *active_config;
};


/* This function has C linkage */
bool connect_to_primary_cluster(const char *connectstring,
                                const char *server_role) {
  DEBUG_ENTER();
  active_config = new Configuration;
  active_config->setPrimaryConnectString(connectstring);
  active_config->setServerRole(server_role);
  return active_config->connectToPrimary();
}


bool read_configuration(Configuration *cf) {
  const char *method[4] = {
    "is ignored", 
    "uses NDB only", 
    "uses local cache only", 
    "uses NDB with local cache" 
  };
  int npref;
  int log_msg_sz = 0;
  const int log_buf_sz = 2048;
  char logmsg[log_buf_sz];

  if(cf->readConfiguration()) {
    const KeyPrefix *p = cf->getDefaultPrefix();
    npref = cf->nprefixes;
    unsigned pGET = (p->info.do_mc_read * 2) + p->info.do_db_read;
    unsigned pSET = (p->info.do_mc_write * 2) + p->info.do_db_write;
    unsigned pDEL = (p->info.do_mc_delete * 2) + p->info.do_db_delete;
    
    logger->log(LOG_WARNING, NULL,
                "Retrieved %d key prefix%s for server role \"%s\".\n"
                "The default behavior is that: \n"
                "    GET %s\n    SET %s\n    DELETE %s.\n",
                cf->nprefixes,
                cf->nprefixes == 1 ? "" : "es", cf->getServerRole(),
                method[pGET], method[pSET], method[pDEL]); 
                   
    if(npref > 1) {  /* List all non-default prefixes */
      log_msg_sz = snprintf(logmsg, log_buf_sz - log_msg_sz, 
                            "The %d explicitly defined key prefix%s ",
                            npref - 1, npref == 2 ? " is" : "es are");
      for(int i = 1 ; i < npref ; i ++) {
        log_msg_sz += snprintf(logmsg + log_msg_sz, log_buf_sz - log_msg_sz, 
                              "%s\"%s\" (%s)", 
                              i > 1 ? (i == npref - 1 ? " and " : ", ") : "",
                              cf->getPrefix(i)->prefix,
                              cf->getPrefix(i)->table ?
                              cf->getPrefix(i)->table->table_name : "");
      }
      snprintf(logmsg + log_msg_sz, log_buf_sz - log_msg_sz, "\n");
      logger->log(LOG_WARNING, NULL, logmsg);
    }
    return true;
  }
  return false;
}


/* This function has C linkage */
bool get_config() {
  return read_configuration(active_config);
}


/* This function has C linkage */
bool open_connections_to_all_clusters() {
  return active_config->openAllConnections();
}


/* This function has C linkage */
bool prefetch_dictionary_objects() {
  return active_config->prefetchDictionary();
}


/* This function has C linkage */
void set_initial_cas_ids(unsigned int *hi, ndbmc_atomic32_t *lo) {
  /* Set the initial CAS for the default engine: */
  /* XXXXX disabled.  Because we're linking with the actual default engine,
     we don't have the opportunity to coordinate CAS IDs between the two
     engines. */
  // cache_set_initial_cas_id(active_config->initial_cas.for_default_engine); 
  
  /* Set the initial CAS for the NDB engine: */
  *hi = active_config->initial_cas.for_ndb_engine >> 32;
  *lo = active_config->initial_cas.for_ndb_engine & 0xFFFFFFFF;
}

/* This function has C linkage */
prefix_info_t get_prefix_info_for_key(int nkey, const char *key) {
  const KeyPrefix *prefix = active_config->getPrefixForKey(key, nkey);
  return prefix->info;
}


/* This function has C linkage */
void disconnect_all() {
  /* Run only at shutdown time.  Disabled to silence a warning about 
     "Deleting Ndb_cluster_connection with Ndb-object not deleted" */
  // active_config->disconnectAll();
}


/* This function has C linkage */
void print_debug_startup_info() {
  size_t wi1 = 1 << workitem_class_id; 
  size_t wi2 = sizeof(workitem) - WORKITEM_MIN_INLINE_BUF;
  size_t wi3 = workitem_actual_inline_buffer_size;
  
  DEBUG_PRINT("  sizeof Ndb           : %lu", sizeof(Ndb));
  DEBUG_PRINT("  sizeof NdbInstance   : %lu", sizeof(NdbInstance));
  DEBUG_PRINT("  sizeof workitem      : %lu (%lu + buffer: %lu)", wi1, wi2, wi3);
  DEBUG_PRINT("  sizeof ExternalValue : %lu", sizeof(ExternalValue));
}


void reconfigure(Scheduler *s) {
  DEBUG_ENTER();

  next_config = new Configuration(active_config);
  read_configuration(next_config);
  if(s->global_reconfigure(next_config)) {
    /* There is no garbage collection here, but there could be if Configuration
       had a carefully-written destructor. */
    stale_config = active_config;
    active_config = next_config;
    next_config = 0;
    logger->log(LOG_WARNING, 0, "ONLINE RECONFIGURATION COMPLETE");
  }
  else {
    logger->log(LOG_WARNING, 0, 
                "Online configuration aborted -- not supported by scheduler.");
  }
}


extern "C" { 
  void * run_reconfig_listener_thread(void *);
}


void * run_reconfig_listener_thread(void *p) {
  thread_identifier tid;
  tid.pipeline = 0;
  strcpy(tid.name,"config_listener");
  set_thread_id(&tid);
  
  DEBUG_ENTER();
  Scheduler * sched = (Scheduler *) p;
  
  while(1) {
    int i = active_config->waitForReconfSignal(); 
    
    if(i == 0) {
      DEBUG_PRINT("will listen again.");
    }
    else if(i == 1) {
      DEBUG_PRINT("reconfiguring");
      reconfigure(sched);
    }
    else {
      DEBUG_PRINT("error (%d); exiting.", i);
      break;
    }
  }
  return 0;
}


/* This function has C linkage */
void start_reconfig_listener(void *scheduler) {
  DEBUG_ENTER();
  if(active_config->canReloadOnline()) {
    pthread_t thd_id;
    
    DEBUG_PRINT("Starting thread.");
    pthread_create(& thd_id, NULL, run_reconfig_listener_thread, scheduler);
  }
  else {
    DEBUG_PRINT("Not supported.");
  }
}

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
#include "my_config.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>

#include "config.h"

#include "default_engine.h"

#include <memcached/util.h>
#include <memcached/config_parser.h>
#include <memcached/extension.h>
#include <memcached/extension_loggers.h>

#include "ndb_engine.h"
#include "ndb_configuration.h"
#include "workitem.h"
#include "ndb_engine_private.h"
#include "debug.h"
#include "Scheduler.h"
#include "thread_identifier.h"
#include "timing.h"
#include "ndb_error_logger.h"

/* Global variables */
EXTENSION_LOGGER_DESCRIPTOR *logger;

/* Static and local to this file */
const char * set_ops[] = { "","add","set","replace","append","prepend","cas" };


static inline struct ndb_engine* ndb_handle(ENGINE_HANDLE* handle) 
{
  return (struct ndb_engine*) handle;
}

static inline struct default_engine* default_handle(struct ndb_engine *eng) 
{
  return (struct default_engine*) eng->m_default_engine;
}


/*********** PRIVATE UTILITY FUNCTIONS BEGIN HERE ***********************/

ndb_pipeline * get_my_pipeline_config(struct ndb_engine *eng) {
  const thread_identifier * thread_id;
  
  /* Try to fetch the pipeline from the thread identity */
  thread_id = get_thread_id();
  if(thread_id) {
    return thread_id->pipeline;
  }
  else {
    /* On the first call from each thread, initialize the pipeline */
    return ndb_pipeline_initialize(eng);
  }
}


/*********** FUNCTIONS IMPLEMENTING THE PUBLISHED API BEGIN HERE ********/

ENGINE_ERROR_CODE create_instance(uint64_t interface, 
                                  GET_SERVER_API get_server_api,
                                  ENGINE_HANDLE **handle ) {
  
  struct ndb_engine *ndb_eng;
  const char * env_connectstring;
  ENGINE_ERROR_CODE return_status;
  
  SERVER_HANDLE_V1 *api = get_server_api();
  if (interface != 1 || api == NULL) {
    return ENGINE_ENOTSUP;
  }
  
  ndb_eng = malloc(sizeof(struct ndb_engine));
  if(ndb_eng == NULL) {
    return ENGINE_ENOMEM;
  }
 
  logger = api->extension->get_extension(EXTENSION_LOGGER);
    
  ndb_eng->npipelines = 0;
  ndb_eng->connected  = false;

  ndb_eng->engine.interface.interface = 1;
  ndb_eng->engine.get_info         = ndb_get_info;
  ndb_eng->engine.initialize       = ndb_initialize;
  ndb_eng->engine.destroy          = ndb_destroy;
  ndb_eng->engine.allocate         = ndb_allocate;
  ndb_eng->engine.remove           = ndb_remove;
  ndb_eng->engine.release          = ndb_release;
  ndb_eng->engine.get              = ndb_get;
  ndb_eng->engine.get_stats        = ndb_get_stats;
  ndb_eng->engine.reset_stats      = ndb_reset_stats;
  ndb_eng->engine.store            = ndb_store;
  ndb_eng->engine.arithmetic       = ndb_arithmetic;
  ndb_eng->engine.flush            = ndb_flush;
  ndb_eng->engine.unknown_command  = ndb_unknown_command;
  ndb_eng->engine.item_set_cas     = item_set_cas;           /* reused */
  ndb_eng->engine.get_item_info    = ndb_get_item_info; 
  ndb_eng->engine.get_stats_struct = NULL;
  ndb_eng->engine.aggregate_stats  = NULL;
  ndb_eng->engine.tap_notify       = NULL;
  ndb_eng->engine.get_tap_iterator = NULL;
  ndb_eng->engine.errinfo          = NULL;
  
  ndb_eng->server = *api;
  ndb_eng->get_server_api = get_server_api;
  
  /* configuration, with default values*/
  ndb_eng->startup_options.connectstring = "localhost:1186";
  ndb_eng->startup_options.server_role   = "default_role";
  ndb_eng->startup_options.scheduler     = 0;
  ndb_eng->startup_options.debug_enable  = false;
  ndb_eng->startup_options.debug_detail  = false;
  ndb_eng->startup_options.reconf_enable = true;

  /* Now let NDB_CONNECTSRING environment variable override the default */
  env_connectstring = getenv("NDB_CONNECTSTRING");
  if(env_connectstring)
    ndb_eng->startup_options.connectstring = env_connectstring;

  /* Set engine informational structure */
  ndb_eng->info.info.description = "NDB Memcache " VERSION;
  ndb_eng->info.info.num_features = 3;
  ndb_eng->info.info.features[0].feature = ENGINE_FEATURE_CAS;
  ndb_eng->info.info.features[0].description = NULL;
  ndb_eng->info.info.features[1].feature = ENGINE_FEATURE_PERSISTENT_STORAGE;
  ndb_eng->info.info.features[1].description = NULL;
  ndb_eng->info.info.features[2].feature = ENGINE_FEATURE_LRU;
  ndb_eng->info.info.features[2].description = NULL;
 
  /* Now call create_instace() for the default engine */
  return_status = default_engine_create_instance(interface, get_server_api, 
                                                 & (ndb_eng->m_default_engine));

  if(return_status == ENGINE_SUCCESS)
    *handle = (ENGINE_HANDLE*) &ndb_eng->engine;
  
  return return_status;
}


/*** get_info ***/
static const engine_info* ndb_get_info(ENGINE_HANDLE* handle)
{
  return & ndb_handle(handle)->info.info;
}


/*** initialize ***/
static ENGINE_ERROR_CODE ndb_initialize(ENGINE_HANDLE* handle,
                                        const char* config_str) 
{   
  int i, nthreads, debug_level;
  time_point_t pump_time = 0;
  ENGINE_ERROR_CODE return_status;
  struct ndb_engine *ndb_eng = ndb_handle(handle);
  struct default_engine *def_eng = default_handle(ndb_eng);
  scheduler_options sched_opts;
  
  /* Process options for both the ndb engine and the default engine:  */
  read_cmdline_options(ndb_eng, def_eng, config_str);

  /* Initalize the debug library */
  if(ndb_eng->startup_options.debug_detail)
    debug_level = 2;
  else if(ndb_eng->startup_options.debug_enable)
    debug_level = 1;
  else debug_level = 0;
  DEBUG_INIT(NULL, debug_level);
  DEBUG_ENTER();
  
  /* Connect to the Primary cluster */
  if(!(connect_to_primary_cluster(ndb_eng->startup_options.connectstring,
                                  ndb_eng->startup_options.server_role))) {
     logger->log(LOG_WARNING, 0, "Could not connect to NDB.  Shutting down.\n");
     return ENGINE_FAILED;
  }
  ndb_eng->connected = true;

  /* Read configuration */
  if(!(get_config())) {
     logger->log(LOG_WARNING, 0, "Failed to read configuration -- shutting down.\n"
                 "(Did you run ndb_memcache_metadata.sql?)\n");
     return ENGINE_FAILED;
  }

  /* Connect to additional clusters */
  if(! open_connections_to_all_clusters()) {
    logger->log(LOG_WARNING, 0, "open_connections_to_all_clusters() failed \n");
   return ENGINE_FAILED;
  }
  
  /* Initialize Thread-Specific Storage */
  initialize_thread_id_key();
  
  /* Fetch some settings from the memcached core */
  fetch_core_settings(ndb_eng, def_eng);
  nthreads = ndb_eng->server_options.nthreads;

  /* Initialize the error handler */
  ndb_error_logger_init(def_eng->server.core, ndb_eng->server_options.verbose);

  logger->log(LOG_WARNING, NULL, "Server started with %d threads.\n", nthreads);
  logger->log(LOG_WARNING, NULL, "Priming the pump ... ");
  timing_point(& pump_time);

  /* prefetch data dictionary objects */
  prefetch_dictionary_objects();

  /* Build the scheduler options structure */
  sched_opts.nthreads = ndb_eng->server_options.nthreads;
  sched_opts.max_clients = ndb_eng->server_options.maxconns;

  /* Allocate and initailize the pipelines, and their schedulers.  
     This will take some time; each pipeline creates slab and pool allocators,
     and each scheduler may preallocate a large number of Ndb objects and 
     transactions, requiring multiple round trips to the data nodes.  We 
     do this now to avoid the latency cost of setting up those objects at
     runtime. 
     When the first request comes in, the pipeline, scheduler, and thread
     will all get stitched together.
  */  
  ndb_eng->pipelines  = malloc(nthreads * sizeof(void *));
  for(i = 0 ; i < nthreads ; i++) {
    ndb_eng->pipelines[i] = get_request_pipeline(i, ndb_eng);
    if(! scheduler_initialize(ndb_eng->pipelines[i], & sched_opts)) {
      logger->log(LOG_WARNING, NULL, "Illegal scheduler: \"%s\"\n", 
                  ndb_eng->startup_options.scheduler); 
      abort();
    }
  }
  
  logger->log(LOG_WARNING, NULL, "done [%5.3f sec].\n", 
              (double) (timing_point(& pump_time) / (double) 1000000000));

  /* Now initialize the default engine with no options (it already has them) */
  return_status = def_eng->engine.initialize(ndb_eng->m_default_engine, "");
  
  if(return_status == ENGINE_SUCCESS) {
    set_initial_cas_ids(& ndb_eng->cas_hi, & ndb_eng->cas_lo);
  }

  print_debug_startup_info();

  /* Listen for reconfiguration signals */
  if(ndb_eng->startup_options.reconf_enable) {
    start_reconfig_listener(ndb_eng->pipelines[0]);
  }
  
  return return_status;
}

                          
/*** destroy ***/
static void ndb_destroy(ENGINE_HANDLE* handle, bool force) 
{
  struct ndb_engine* ndb_eng;
  struct default_engine *def_eng;
  DEBUG_ENTER();

  ndb_eng = ndb_handle(handle);
  def_eng = default_handle(ndb_eng);

  // TODO: Shutdown just the Scheduler Global
  for(atomic_int32_t i = 0 ; i < ndb_eng->npipelines; i ++) {
    ndb_pipeline *p = ndb_eng->pipelines[i];
    if(p) {
      scheduler_shutdown(p);
      ndb_pipeline_free(p);
    }
  }

  disconnect_all();  
  def_eng->engine.destroy(ndb_eng->m_default_engine, force);
}


/* CALL FLOWS
   ----------
   GET:       eng.get(), eng.get_item_info()*, eng.release()*
   DELETE:    eng.remove()
   SET (etc): eng.allocate(), eng.item_set_cas(), eng.get_item_info(), 
                 eng.store(), eng.release()*
   INCR:      eng.arithmetic()
   FLUSH:     eng.flush()

   * Called only on success (ENGINE_SUCCESS or ENGINE_EWOULDBLOCK)
*/
 


/*** Release scheduler resources and free workitem ****/
void release_and_free(workitem *wqitem) {
  DEBUG_PRINT("Releasing workitem %d.%d.", wqitem->pipeline->id, wqitem->id);
  scheduler_release(wqitem->pipeline, wqitem);  
  workitem_free(wqitem);
}


/*** allocate ***/

/* Allocate gets a struct item from the slab allocator, and fills in 
   everything but the value.  It seems like we can just pass this on to 
   the default engine; we'll intercept it later in store().
   This is also called directly from finalize_read() in the commit thread.
*/
static ENGINE_ERROR_CODE ndb_allocate(ENGINE_HANDLE* handle,
                                           const void* cookie,
                                           item **item,
                                           const void* key,
                                           const size_t nkey,
                                           const size_t nbytes,
                                           const int flags,
                                           const rel_time_t exptime)
{
  struct ndb_engine* ndb_eng;
  struct default_engine *def_eng;
  DEBUG_ENTER_DETAIL();

  ndb_eng = ndb_handle(handle);
  def_eng = default_handle(ndb_eng);

  return def_eng->engine.allocate(ndb_eng->m_default_engine, cookie, item, 
                                  key, nkey, nbytes, flags, exptime);
}


/*** remove ***/
static ENGINE_ERROR_CODE ndb_remove(ENGINE_HANDLE* handle,
                                         const void* cookie,
                                         const void* key,
                                         const size_t nkey,
                                         uint64_t cas,
                                         uint16_t vbucket  __attribute__((unused)))
{
  struct ndb_engine* ndb_eng = ndb_handle(handle);
  struct default_engine *def_eng = default_handle(ndb_eng);
  ndb_pipeline *pipeline = get_my_pipeline_config(ndb_eng);
  ENGINE_ERROR_CODE return_status = ENGINE_KEY_ENOENT;
  prefix_info_t prefix;
  workitem *wqitem;

  /* Is this a callback after completed I/O? */
  wqitem = ndb_eng->server.cookie->get_engine_specific(cookie);
  if(wqitem) {
    DEBUG_PRINT_DETAIL("Got callback: %s", wqitem->status->comment);
    ndb_eng->server.cookie->store_engine_specific(cookie, wqitem->previous); //pop
    release_and_free(wqitem);
    return wqitem->status->status;
  }

  prefix = get_prefix_info_for_key(nkey, key);
  DEBUG_PRINT_DETAIL("prefix: %d", prefix.prefix_id);

  /* DELETE.  
     You should attempt the cache delete, regardless of whether the database 
     delete succeeds or fails.  So, we simply perform the cache delete first, 
     here, and then queue the database delete. 
  */

  if(prefix.do_mc_delete) {                         /* Cache Delete */ 
    hash_item *it = item_get(def_eng, key, nkey);
    return_status = ENGINE_KEY_ENOENT;
    if (it != NULL) {
      // ACTUALLY NO??? 
      /* In the binary protocol there is such a thing as a CAS delete.
         This is the CAS check.  If we will also be deleting from the database, 
         there are two possibilities:
          1: The CAS matches; perform the delete.
          2: The CAS doesn't match; delete the item because it's stale.
         Therefore we skip the check altogether if(do_db_delete).
      */
      if(! prefix.do_db_delete)
        if(cas && (cas != item_get_cas(it)))
          return ENGINE_KEY_EEXISTS;
      
      item_unlink(def_eng, it);
      item_release(def_eng, it);
      return_status  = ENGINE_SUCCESS;
    }
  }

  if(prefix.do_db_delete) {                        /* Database Delete */
    wqitem = new_workitem_for_delete_op(pipeline, prefix, cookie, nkey, key, & cas);
    DEBUG_PRINT("creating workitem %d.%d", pipeline->id, wqitem->id);
    return_status = scheduler_schedule(pipeline, wqitem);
    if(return_status != ENGINE_EWOULDBLOCK) {
      release_and_free(wqitem);
    }
  }
  
  return return_status;
}


/*** release ***/
static void ndb_release(ENGINE_HANDLE* handle, const void *cookie,
                        item* item)
{
  struct ndb_engine* ndb_eng = ndb_handle(handle);
  struct default_engine *def_eng = default_handle(ndb_eng);

  workitem *wqitem = ndb_eng->server.cookie->get_engine_specific(cookie);  
  if(wqitem) {
    ndb_eng->server.cookie->store_engine_specific(cookie, wqitem->previous);
    release_and_free(wqitem);
  }
  
  if(item && (item != wqitem)) {
    DEBUG_PRINT_DETAIL("Releasing a hash item.");
    item_release(def_eng, (hash_item *) item);
  }
}  


/*** get ***/
static ENGINE_ERROR_CODE ndb_get(ENGINE_HANDLE* handle,
                                 const void* cookie,
                                 item** item,
                                 const void* key,
                                 const int nkey,
                                 uint16_t vbucket __attribute__((unused)))
{
  struct ndb_engine* ndb_eng = ndb_handle(handle);
  ndb_pipeline *pipeline = get_my_pipeline_config(ndb_eng);
  struct workitem *wqitem;
  prefix_info_t prefix;
  ENGINE_ERROR_CODE return_status = ENGINE_KEY_ENOENT;

  wqitem = ndb_eng->server.cookie->get_engine_specific(cookie);

  /* Is this a callback after completed I/O? */
  if(wqitem && ! wqitem->base.complete) {  
    DEBUG_PRINT_DETAIL("Got read callback on workitem %d.%d: %s",
                wqitem->pipeline->id, wqitem->id, wqitem->status->comment);
    *item = wqitem->cache_item;
    wqitem->base.complete = 1;
    return_status = wqitem->status->status;

    /* On success the workitem will be read in ndb_get_item_info, then released.
       Otherwise: */
    if(return_status != ENGINE_SUCCESS) {
      ndb_eng->server.cookie->store_engine_specific(cookie, wqitem->previous);//pop
      release_and_free(wqitem);
    }
    
    return return_status;  
  }

  prefix = get_prefix_info_for_key(nkey, key);

  /* Cache read */
  /* FIXME: Use the public APIs */
  if(prefix.do_mc_read) {
    *item = item_get(default_handle(ndb_eng), key, nkey);
    if (*item != NULL) {
      DEBUG_PRINT(" cache hit");
      return ENGINE_SUCCESS;
    }
    DEBUG_PRINT(" cache miss");
  }

  /* Build and send the NDB transaction */
  if(prefix.do_db_read) {
    wqitem = new_workitem_for_get_op(wqitem, pipeline, prefix, cookie, nkey, key);
    DEBUG_PRINT("creating workitem %d.%d", pipeline->id, wqitem->id);
    return_status = scheduler_schedule(pipeline, wqitem);
    if(! ((return_status == ENGINE_EWOULDBLOCK) || (return_status == ENGINE_SUCCESS))) {
      /* On error we must pop and free */
      ndb_eng->server.cookie->store_engine_specific(cookie, wqitem->previous);
      release_and_free(wqitem);
    }
  }
  
  return return_status;
}


/*** get_stats ***/
static ENGINE_ERROR_CODE ndb_get_stats(ENGINE_HANDLE* handle,
                                       const void *cookie,
                                       const char *stat_key,
                                       int nkey,
                                       ADD_STAT add_stat)
{
  struct ndb_engine* ndb_eng = ndb_handle(handle);
  struct default_engine *def_eng = default_handle(ndb_eng);
  ndb_pipeline *pipeline = get_my_pipeline_config(ndb_eng); 
    
  DEBUG_ENTER_DETAIL();
  
  if(stat_key) { 
    if(strncasecmp(stat_key, "menu", 4) == 0)
      return stats_menu(add_stat, cookie);
    
   if((strncasecmp(stat_key, "ndb", 3) == 0)       || 
       (strncasecmp(stat_key, "scheduler", 9) == 0) ||
       (strncasecmp(stat_key, "reconf", 6) == 0)    ||
       (strncasecmp(stat_key, "errors", 6) == 0))
    {
      /* NDB Engine stats */
      pipeline_add_stats(pipeline, stat_key, add_stat, cookie);
      return ENGINE_SUCCESS;
    }
  }

  /* Default engine stats */
  return def_eng->engine.get_stats(ndb_eng->m_default_engine, cookie,
                                   stat_key, nkey, add_stat);
}


/*** reset_stats ***/
static void ndb_reset_stats(ENGINE_HANDLE* handle, 
                            const void *cookie)
{
  struct ndb_engine* ndb_eng = ndb_handle(handle);
  struct default_engine *def_eng = default_handle(ndb_eng);
  /* ndb_pipeline *pipeline = get_my_pipeline_config(ndb_eng); */
  /* DEBUG_ENTER(); */
  def_eng->engine.reset_stats(ndb_eng->m_default_engine, cookie);
}


/*** store ***/

static ENGINE_ERROR_CODE ndb_store(ENGINE_HANDLE* handle,
                                   const void *cookie,
                                   item* item,
                                   uint64_t *cas,
                                   ENGINE_STORE_OPERATION op,
                                   uint16_t vbucket  __attribute__((unused)))
{
  struct ndb_engine* ndb_eng = ndb_handle(handle);
  ndb_pipeline *pipeline = get_my_pipeline_config(ndb_eng);
  ENGINE_ERROR_CODE return_status = ENGINE_NOT_STORED;
  prefix_info_t prefix;

  /* Is this a callback after completed I/O? */  
  workitem *wqitem = ndb_eng->server.cookie->get_engine_specific(cookie);  
  if(wqitem) {
    DEBUG_PRINT_DETAIL("Got callback on workitem %d.%d: %s",
                pipeline->id, wqitem->id, wqitem->status->comment);
    return wqitem->status->status;
  }

  prefix = get_prefix_info_for_key(hash_item_get_key_len(item), 
                                   hash_item_get_key(item));


  /* Build and send the NDB transaction. 
     If there is also a cache operation, it must be deferred until we know
     whether the database operation has succeeded or failed.
  */
  if(prefix.do_db_write) {
    wqitem = new_workitem_for_store_op(pipeline, op, prefix, cookie, item, cas);
    DEBUG_PRINT("[%s] prefix %d; CAS %llu; use mc/db: %d/%d  --  creating workitem %d.%d",
                set_ops[op], prefix.prefix_id, cas ? *cas : 0,
                prefix.do_mc_write, prefix.do_db_write,
                pipeline->id, wqitem->id);
    return_status = scheduler_schedule(pipeline, wqitem);
    if(! ((return_status == ENGINE_EWOULDBLOCK) || (return_status == ENGINE_SUCCESS))) {
      ndb_eng->server.cookie->store_engine_specific(cookie, wqitem->previous);//pop
      release_and_free(wqitem);      
    }
  }
  else if(prefix.do_mc_write) {
    DEBUG_PRINT("[%s] prefix %d; CAS %llu; use mc/db: %d/%d --  cache-only store.",
                set_ops[op], prefix.prefix_id, cas ? *cas : 0,
                prefix.do_mc_write, prefix.do_db_write);
    return_status = store_item(default_handle(ndb_eng), item, cas, op, cookie);
  }
  
  return return_status;  
}


/*** arithmetic ***/
static ENGINE_ERROR_CODE ndb_arithmetic(ENGINE_HANDLE* handle,
                                        const void* cookie,
                                        const void* key,
                                        const int nkey,
                                        const bool increment,
                                        const bool create,
                                        const uint64_t delta,
                                        const uint64_t initial,
                                        const rel_time_t exptime,
                                        uint64_t *cas,
                                        uint64_t *result,
                                        uint16_t vbucket)
{
  struct ndb_engine* ndb_eng = ndb_handle(handle);
  struct default_engine *def_eng = default_handle(ndb_eng);
  ndb_pipeline *pipeline = get_my_pipeline_config(ndb_eng);
  struct workitem *wqitem;
  prefix_info_t prefix;
  ENGINE_ERROR_CODE return_status;  
  
  /* Is this a callback after completed I/O? */
  wqitem = ndb_eng->server.cookie->get_engine_specific(cookie);
  if(wqitem && ! wqitem->base.complete) {
    DEBUG_PRINT_DETAIL("Got arithmetic callback: %s", wqitem->status->comment);
    return_status = wqitem->status->status;  
    wqitem->base.complete = 1;
    *result = wqitem->math_value;
    /* There will be no call to release(), so pop and free now. */
    ndb_eng->server.cookie->store_engine_specific(cookie, wqitem->previous);
    release_and_free(wqitem);
    return return_status;
  }
  
  prefix = get_prefix_info_for_key(nkey, key);
  DEBUG_PRINT("prefix: %d   delta: %d  create: %d   initial: %d ", 
                     prefix.prefix_id, (int) delta, (int) create, (int) initial);

  /* For cache-only prefixes, forward this to the default engine */
  if(! prefix.use_ndb) {
    return def_eng->engine.arithmetic(ndb_eng->m_default_engine, cookie,
                                      key, nkey, increment, create, 
                                      delta, initial, exptime, cas,
                                      result, vbucket);    
  }
  
  /* A math operation contains both a read and a write. */
  if(! (prefix.has_math_col && prefix.do_db_read && prefix.do_db_write)) {
    logger->log(LOG_WARNING, 0, "NDB INCR/DECR is not allowed for this key.\n");
    DEBUG_PRINT("REJECTED : %d %d %d", (int) prefix.has_math_col,
                       (int) prefix.do_db_read , (int)  prefix.do_db_write);
    return ENGINE_NOT_STORED; 
  }
  
  wqitem = new_workitem_for_arithmetic(pipeline, prefix, cookie, key, nkey,
                                       increment, create, delta, initial, cas);
  DEBUG_PRINT("creating workitem %d.%d", pipeline->id, wqitem->id);

  return_status = scheduler_schedule(pipeline, wqitem);

  if(! ((return_status == ENGINE_EWOULDBLOCK) || (return_status == ENGINE_SUCCESS)))
    release_and_free(wqitem);

  return return_status;
}

/*** flush ***/
static ENGINE_ERROR_CODE ndb_flush(ENGINE_HANDLE* handle,
                                   const void* cookie, time_t when)
{                                   
/* 
   Notes on flush:
   The server will call *only* into ndb_flush (not to allocate or release).
   The NDB engine ignores the "when" parameter.    
   Flush operations have special handling, outside of the scheduler.
   They are performed synchronously. 
   And we always send the flush command to the cache engine.   
*/
  struct ndb_engine* ndb_eng;
  struct default_engine *def_eng;
  ndb_pipeline *pipeline;
  DEBUG_ENTER();

  ndb_eng = ndb_handle(handle);
  def_eng = default_handle(ndb_eng);
  pipeline = get_my_pipeline_config(ndb_eng);

  (void) def_eng->engine.flush(ndb_eng->m_default_engine, cookie, when);
  // TODO: Why not return ENGINE_EWOULDBLOCK first?
  return pipeline_flush_all(pipeline);
}


/*** unknown_command ***/
static ENGINE_ERROR_CODE ndb_unknown_command(ENGINE_HANDLE* handle,
                                             const void* cookie,
                                             protocol_binary_request_header *request,
                                             ADD_RESPONSE response)
{
  struct ndb_engine* ndb_eng;
  struct default_engine *def_eng;
  DEBUG_ENTER();

  ndb_eng = ndb_handle(handle);
  def_eng = default_handle(ndb_eng);

  return def_eng->engine.unknown_command(ndb_eng->m_default_engine, cookie, 
                                         request, response);
}


/*** get_item_info ***/
static bool ndb_get_item_info(ENGINE_HANDLE *handle, 
                              const void *cookie,
                              const item* item, 
                              item_info *item_info)
{
  struct ndb_engine* ndb_eng = ndb_handle(handle);
  struct default_engine *def_eng = default_handle(ndb_eng);

  workitem *wqitem = ndb_eng->server.cookie->get_engine_specific(cookie);

  if(wqitem == NULL) {
    DEBUG_PRINT_DETAIL(" cache-only");
    return def_eng->engine.get_item_info(handle, cookie, item, item_info);
  }

  if (item_info->nvalue < 1) {
    DEBUG_PRINT_DETAIL("nvalue too small.");
    return false;
  }
  
  if(wqitem->base.has_value) {
    /* Use the workitem. */
    item_info->cas = wqitem->cas ? *(wqitem->cas) : 0;
    item_info->exptime = 0;  
    item_info->nbytes = wqitem->value_size;
    item_info->flags = wqitem->math_flags;
    item_info->clsid = slabs_clsid(default_handle(ndb_eng), wqitem->value_size);
    item_info->nkey = wqitem->base.nkey;
    item_info->nvalue = 1;  /* how many iovecs */
    item_info->key = wqitem->key;
    item_info->value[0].iov_base = wqitem->value_ptr;
    item_info->value[0].iov_len = wqitem->value_size;   
    DEBUG_PRINT_DETAIL("workitem %d.%d [%s].", wqitem->pipeline->id, wqitem->id,
                       workitem_get_operation(wqitem));
    return true;
  }
  else {
    /* Use a hash item */
    hash_item *it = (hash_item*) item;
    item_info->cas = hash_item_get_cas(it);
    item_info->exptime = it->exptime;
    item_info->nbytes = wqitem ? wqitem->value_size : 0;
    item_info->flags = it->flags;
    item_info->clsid = it->slabs_clsid;
    item_info->nkey = it->nkey;
    item_info->nvalue = 1;
    item_info->key = hash_item_get_key(it);
    item_info->value[0].iov_base = hash_item_get_data(it);
    item_info->value[0].iov_len = item_info->nbytes;
    if(item_info->nbytes) {
      DEBUG_PRINT_DETAIL("hash_item [KEY: %.*s][CAS: %llu][nbytes: %d].", it->nkey,
                          hash_item_get_key(it), item_info->cas, item_info->nbytes);
    }
    else {
      DEBUG_PRINT_DETAIL(" new hash_item");
    }
    return true;
  }
}


/* read_cmdline_options requires duplicating code from the default engine. 
   If the default engine supports a new option, you will need to add it here.
   We create a single config_items structure containing options for both 
   engines.
*/
void read_cmdline_options(struct ndb_engine *ndb, struct default_engine *se,
                          const char * conf)
{
  int did_parse;
  DEBUG_ENTER();

  did_parse = 0;   /* 0 = success from parse_config() */

  if (conf != NULL) {
    struct config_item items[] = {
      /* NDB OPTIONS */
      { .key = "connectstring",
        .datatype = DT_STRING,
        .value.dt_string = (char **) &(ndb->startup_options.connectstring) },
      { .key = "role",
        .datatype = DT_STRING,
        .value.dt_string = (char **) &(ndb->startup_options.server_role) },
      { .key = "scheduler",
        .datatype = DT_STRING,
        .value.dt_string = (char **) &(ndb->startup_options.scheduler) },
#ifdef DEBUG_OUTPUT
      { .key = "debug",
        .datatype = DT_BOOL,
        .value.dt_bool = &(ndb->startup_options.debug_enable) },
      { .key = "detail",
        .datatype = DT_BOOL,
        .value.dt_bool = &(ndb->startup_options.debug_detail) },
#endif
      { .key = "reconf",
        .datatype = DT_BOOL,
        .value.dt_bool = &(ndb->startup_options.reconf_enable) },
      
      /* DEFAULT ENGINE OPTIONS */
      { .key = "use_cas",
        .datatype = DT_BOOL,
        .value.dt_bool = &se->config.use_cas },
      { .key = "verbose",
        .datatype = DT_SIZE,
        .value.dt_size = &se->config.verbose },
      { .key = "eviction",
        .datatype = DT_BOOL,
        .value.dt_bool = &se->config.evict_to_free },
      { .key = "cache_size",
        .datatype = DT_SIZE,
        .value.dt_size = &se->config.maxbytes },
      { .key = "preallocate",
        .datatype = DT_BOOL,
        .value.dt_bool = &se->config.preallocate },
      { .key = "factor",
        .datatype = DT_FLOAT,
        .value.dt_float = &se->config.factor },
      { .key = "chunk_size",
        .datatype = DT_SIZE,
        .value.dt_size = &se->config.chunk_size },
      { .key = "item_size_max",
        .datatype = DT_SIZE,
        .value.dt_size = &se->config.item_size_max },
      { .key = "config_file",
        .datatype = DT_CONFIGFILE },
      { .key = NULL}
    };
    
    did_parse = se->server.core->parse_config(conf, items, stderr);
  }
  switch(did_parse) {
      case -1:
        logger->log(LOG_WARNING, NULL, 
                    "Unknown tokens in config string \"%s\"\n", conf);
        break;
      case 1:
        logger->log(LOG_WARNING, NULL, 
                    "Illegal values in config string: \"%s\"\n", conf);
        break;
      case 0: /* success */
        break;
  }
  
  global_max_item_size = se->config.item_size_max;
}


int fetch_core_settings(struct ndb_engine *engine,
                         struct default_engine *se) {

  /* Set up a struct config_item containing the keys we're interested in. */
  struct config_item items[] = {    
    { .key = "cas_enabled",
      .datatype = DT_BOOL,
      .value.dt_bool = &engine->server_options.cas_enabled },
    { .key = "maxconns",
      .datatype = DT_SIZE,
      .value.dt_size = &engine->server_options.maxconns },
    { .key = "num_threads",
      .datatype = DT_SIZE,
      .value.dt_size = &engine->server_options.nthreads },
    { .key = "verbosity",
      .datatype = DT_SIZE,
      .value.dt_size = &engine->server_options.verbose },
    { .key = NULL }
  };
  
  DEBUG_ENTER();

  /* This will call "stats settings" and parse the output into the config */
  return se->server.core->get_config(items);
}


ENGINE_ERROR_CODE stats_menu(ADD_STAT add_stat, const void *cookie) {
  char key[128];
  char val[128];
  int klen, vlen;
  
  klen = sprintf(key, "ndb");
  vlen = sprintf(val, "          NDB Engine: NDBAPI statistics");
  add_stat(key, klen, val, vlen, cookie);
  
  klen = sprintf(key, "errors");
  vlen = sprintf(val, "       NDB Engine: Error message counters");
  add_stat(key, klen, val, vlen, cookie);

  klen = sprintf(key, "scheduler");
  vlen = sprintf(val, "    NDB Engine: Scheduler internal statistics");
  add_stat(key, klen, val, vlen, cookie);
  
  klen = sprintf(key, "reconf");
  vlen = sprintf(val, "       NDB Engine: Current configuration version");
  add_stat(key, klen, val, vlen, cookie);
  
  klen = sprintf(key, "settings");
  vlen = sprintf(val, "     Server core: configurable settings");
  add_stat(key, klen, val, vlen, cookie);
  
  klen = sprintf(key, "reset");
  vlen = sprintf(val, "        Server core: reset counters");
  add_stat(key, klen, val, vlen, cookie);

  klen = sprintf(key, "detail");
  vlen = sprintf(val, "       Server core: use stats detail on|off|dump");
  add_stat(key, klen, val, vlen, cookie);
   
  klen = sprintf(key, "aggregate");
  vlen = sprintf(val, "    Server core: aggregated");
  add_stat(key, klen, val, vlen, cookie);
  
  klen = sprintf(key, "slabs");
  vlen = sprintf(val, "        Cache Engine: allocator");
  add_stat(key, klen, val, vlen, cookie);  
  
  klen = sprintf(key, "items");
  vlen = sprintf(val, "        Cache Engine: itemes cached");
  add_stat(key, klen, val, vlen, cookie);  

  klen = sprintf(key, "sizes");
  vlen = sprintf(val, "        Cache Engine: items per allocation class");
  add_stat(key, klen, val, vlen, cookie);  

  klen = sprintf(key, "vbucket");
  vlen = sprintf(val, "      Cache Engine: dump vbucket table");
  add_stat(key, klen, val, vlen, cookie);  

  klen = sprintf(key, "scrub");
  vlen = sprintf(val, "        Cache Engine: scrubber status");
  add_stat(key, klen, val, vlen, cookie);
  
  return ENGINE_SUCCESS;
}


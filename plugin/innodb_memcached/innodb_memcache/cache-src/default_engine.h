/*
  Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.
  
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
#ifndef MEMCACHED_DEFAULT_ENGINE_H
#define MEMCACHED_DEFAULT_ENGINE_H

#include "config.h"

#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

#include <memcached/engine.h>
#include <memcached/util.h>
#include <memcached/visibility.h>

/* Slab sizing definitions. */
#define POWER_SMALLEST 1
#define POWER_LARGEST  200
#define CHUNK_ALIGN_BYTES 8
#define DONT_PREALLOC_SLABS
#define MAX_NUMBER_OF_SLAB_CLASSES (POWER_LARGEST + 1)

/** How long an object can reasonably be assumed to be locked before
    harvesting it on a low memory condition. */
#define TAIL_REPAIR_TIME (3 * 3600)


/* Forward decl */
struct default_engine;

#include "trace.h"
#include "items.h"
#include "assoc.h"
#include "hash.h"
#include "slabs.h"

   /* Flags */
#define ITEM_WITH_CAS 1

#define ITEM_LINKED (1<<8)

/* temp */
#define ITEM_SLABBED (2<<8)

struct config {
   bool use_cas;
   size_t verbose;
   rel_time_t oldest_live;
   bool evict_to_free;
   size_t maxbytes;
   bool preallocate;
   float factor;
   size_t chunk_size;
   size_t item_size_max;
   bool ignore_vbucket;
   bool vb0;
};

MEMCACHED_PUBLIC_API
ENGINE_ERROR_CODE create_instance(uint64_t interface,
                                  GET_SERVER_API get_server_api,
                                  ENGINE_HANDLE **handle);

/**
 * Statistic information collected by the default engine
 */
struct engine_stats {
   pthread_mutex_t lock;
   uint64_t evictions;
   uint64_t reclaimed;
   uint64_t curr_bytes;
   uint64_t curr_items;
   uint64_t total_items;
};

struct engine_scrubber {
   pthread_mutex_t lock;
   bool running;
   uint64_t visited;
   uint64_t cleaned;
   time_t started;
   time_t stopped;
};

enum vbucket_state {
    VBUCKET_STATE_DEAD    = 0,
    VBUCKET_STATE_ACTIVE  = 1,
    VBUCKET_STATE_REPLICA = 2,
    VBUCKET_STATE_PENDING = 3
};

struct vbucket_info {
    int state : 2;
};

#define NUM_VBUCKETS 65536

/**
 * Definition of the private instance data used by the default engine.
 *
 * This is currently "work in progress" so it is not as clean as it should be.
 */
typedef engine_info engine_info_type;
struct default_engine {
   ENGINE_HANDLE_V1 engine;
   SERVER_HANDLE_V1 server;
   GET_SERVER_API get_server_api;

   /**
    * Is the engine initalized or not
    */
   bool initialized;

   struct assoc assoc;
   struct slabs slabs;
   struct items items;

   /**
    * The cache layer (item_* and assoc_*) is currently protected by
    * this single mutex
    */
   pthread_mutex_t cache_lock;

   struct config config;
   struct engine_stats stats;
   struct engine_scrubber scrubber;
   union {
       engine_info_type engine_info;
       char buffer[sizeof(engine_info) +
                   (sizeof(feature_info) * LAST_REGISTERED_ENGINE_FEATURE)];
   } info;

   char vbucket_infos[NUM_VBUCKETS];
};

char* item_get_data(const hash_item* item);
const void* item_get_key(const hash_item* item);
void item_set_cas(ENGINE_HANDLE *handle, const void *cookie,
                  item* item, uint64_t val);
uint64_t item_get_cas(const hash_item* item);
uint8_t item_get_clsid(const hash_item* item);

#ifdef __cplusplus
}
#endif

#endif

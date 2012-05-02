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
#ifndef NDBMEMCACHE_NDB_PIPELINE_H
#define NDBMEMCACHE_NDB_PIPELINE_H

#include <pthread.h>

#include <memcached/types.h>

#include "workqueue.h"
#include "ndb_engine.h"
#include "Scheduler.h"

/* This structure is used in both C and C++ code, requiring a small hack: */
#ifdef __cplusplus
#define CPP_SCHEDULER Scheduler
#else 
#define CPP_SCHEDULER void
#endif

/* In each pipeline there lives an allocator, which is used for workitems, 
   key buffers, and row buffers.  In design, it is very similar to
   memcached's own slabs allocator. These defines configure the smallest
   and largest slab classes (as powers of 2) and the size of a slab. 
*/
#define ALLIGATOR_POWER_SMALLEST 4      
#define ALLIGATOR_POWER_LARGEST 16
#define ALLIGATOR_SLAB_SIZE (128 * 1024)
#define ALLIGATOR_ARRAY_SIZE (ALLIGATOR_POWER_LARGEST+1)

/* In a workitem, the inline key buffer will be at least this size.
   In fact we will round sizeof(workitem) up to the size of whatever class
   it is allocated from, and use whatever is left in the inline buffer, too.
*/
#define WORKITEM_MIN_INLINE_BUF 40 

/* Forward declarations */
struct workitem;

struct allocation_reference;  /* An opaque type */ 
typedef struct allocation_reference allocation_reference;   


typedef struct memory_pool {
  struct request_pipeline *pipeline;                         /*! the pipeline */
  allocation_reference *head;                    /*! a private opaque pointer */
  unsigned long size;            /*! the currently allocated size of the pool */
  unsigned long total;         /*! total allocated up to the most recent free */
} memory_pool;


typedef struct {
  size_t          size;        /*! size of items in this class */
  unsigned int    perslab;     /*! number of items per slab */
  void **         list;        /*! the list of pointers */
  unsigned int    list_size;   /*! current size of the list */
  unsigned int    free_idx;    /*! the first free slot */
  size_t          total;       /*! total memory in use by this class */
  pthread_mutex_t lock;        /*! mutex for this class */
} allocator_slab_class;



typedef struct request_pipeline {
  unsigned int id;              /*! each pipeline has an id */
  unsigned int nworkitems;      /*! counter used to give each workitem an id */
  struct ndb_engine *engine;    
  pthread_t worker_thread_id;   
  allocator_slab_class alligator[ALLIGATOR_ARRAY_SIZE];  /*! an allocator */
  CPP_SCHEDULER *scheduler;
  memory_pool *pool;            /*! has the whole lifetime of the pipeline */
} ndb_pipeline;




/* These functions have C linkage: */
DECLARE_FUNCTIONS_WITH_C_LINKAGE

/** initialize a new request pipeline for a memcached NDB engine thread */
ndb_pipeline * ndb_pipeline_initialize(struct ndb_engine *);

/** create a generic request pipeline */
ndb_pipeline * get_request_pipeline(int thd_id, struct ndb_engine *);

/** call into a pipeline for its own statistics */
void pipeline_add_stats(ndb_pipeline *, const char *key, ADD_STAT, const void *);

/** execute a "flush_all" operation */
ENGINE_ERROR_CODE pipeline_flush_all(ndb_pipeline *);


/***** SCHEDULER APIS *****/

/** Global initialization of scheduler, at startup time */
void * scheduler_initialize(ndb_pipeline *, scheduler_options *);

/** shutdown a scheduler */
void scheduler_shutdown(ndb_pipeline *);

/** pass a workitem to the configured scheduler, for execution */
ENGINE_ERROR_CODE scheduler_schedule(ndb_pipeline *, struct workitem *);

/** release the resources that were used by a completed operation */
void scheduler_release(ndb_pipeline *, struct workitem *);


/***** MEMORY MANAGEMENT APIS *****/

/* High-level (pool) API. 
   memory_pool_alloc() uses the pipeline slab allocator to allocate small
   areas, and the system malloc() to allocate larger ones.
   A single call to memory_pool_free() will free all objects allocated 
   from the pool.  
*/
/** create a pool of allocations, all with the same life cycle */
memory_pool * pipeline_create_memory_pool(ndb_pipeline *);

/** allocate a block from pool p */
void * memory_pool_alloc(memory_pool *p, size_t sz);

/** free all objects in pool p; p can be reused */
void memory_pool_free(memory_pool *p);

/** destroy and free pool p itself */
void memory_pool_destroy(memory_pool *p);


/* Lower-level (slab) API. */

/** get the size class needed for an object allocation 
 @return 0 if object_size is 0
 -1 if object_size is too large
 */
int pipeline_get_size_class_id(size_t object_size);

/** allocate an object in a given size class */
void * pipeline_alloc(ndb_pipeline *, int size_class_id);

/** free an object obtained from pipeline_alloc() */
void pipeline_free(ndb_pipeline *, void * object, int size_class_id );


END_FUNCTIONS_WITH_C_LINKAGE

#endif

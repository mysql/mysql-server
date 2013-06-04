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
#include <stdlib.h>
#include <pthread.h>
#include <stdio.h>

/* C++ files must define __STDC_FORMAT_MACROS in order to get PRIu64 */
#define __STDC_FORMAT_MACROS 
#include <inttypes.h>

#include "config.h"
#include "ndb_configuration.h"
#include "Configuration.h"
#include "Scheduler.h"
#include "workitem.h"
#include "ndb_engine.h"
#include "debug.h"
#include "thread_identifier.h"
#include "ndb_worker.h"

#include "schedulers/Stockholm.h"
#include "schedulers/S_sched.h"
#include "ndb_error_logger.h"

#define DEFAULT_SCHEDULER S::SchedulerWorker

/* globals (exported; also used by workitem.c) */
int workitem_class_id;
int workitem_actual_inline_buffer_size;

/* file-scope private variables */
static int pool_slab_class_id;

/* The private internal structure of a allocation_reference */
struct allocation_reference {
  void * pointer;               /*! allocated region (or next array) */
  struct {
    unsigned  is_header   :  1;   /*! is this cell an array header? */ 
    unsigned  sys_malloc  :  1;   /*! set for malloc() allocations */
    unsigned  slab_class  :  6;   /*! slab class of the allocation */
    unsigned  cells_total : 10;   /*! total cells in this array */
    unsigned  cells_idx   : 10;   /*! index of next free cell */
    unsigned  _reserved   :  4;   /*! for future use */
  } d;
};

/* declarations of private utility functions: */
Scheduler * get_scheduler_instance(ndb_engine *);
void init_allocator(ndb_pipeline *);
int init_slab_class(allocator_slab_class *c, int size);
int malloc_new_slab(allocator_slab_class *c);
void init_pool_header(allocation_reference *head, int slab_class);


/* The public API */

/* Attach a new pipeline to an NDB worker thread. 
   Some initialization has already occured when the main single-thread startup
   called get_request_pipeline().  But this is the first call into a pipeline
   from its worker thread.  It will initialize the thread's identifier, and 
   attach the pipeline to its scheduler.
*/
ndb_pipeline * ndb_pipeline_initialize(struct ndb_engine *engine) {
  bool did_inc;
  unsigned int id;
  thread_identifier * tid;

  /* Get my pipeline id */
  do {
    id = engine->npipelines;
    did_inc = atomic_cmp_swap_int(& engine->npipelines, id, id + 1);
  } while(did_inc == false);
  
  /* Fetch the partially initialized pipeline */
  ndb_pipeline * self = (ndb_pipeline *) engine->pipelines[id];

  /* Sanity checks */
  assert(self->id == id);
  assert(self->engine == engine);
  
  /* Set the pthread id */
  self->worker_thread_id = pthread_self(); 

  /* Create and set a thread identity */
  tid = (thread_identifier *) memory_pool_alloc(self->pool, sizeof(thread_identifier));
  tid->pipeline = self;
  sprintf(tid->name, "worker.%d", self->id);
  set_thread_id(tid);

  /* Fetch and attach the scheduler */
  self->scheduler = (Scheduler *) engine->schedulers[self->id];
  self->scheduler->attach_thread(tid);
    
  return self;
}


/* Allocate and initialize a generic request pipeline.
   In unit test code, this can be called with a NULL engine pointer -- 
   it will still initialize a usable slab allocator and memory pool 
   which can be tested.  
*/
ndb_pipeline * get_request_pipeline(int thd_id, struct ndb_engine *engine) { 
  /* Allocate the pipeline */
  ndb_pipeline *self = (ndb_pipeline *) malloc(sizeof(ndb_pipeline)); 
  
  /* Initialize */  
  self->engine = engine;
  self->id = thd_id;
  self->nworkitems = 0;
    
  /* Say hi to the alligator */  
  init_allocator(self);
  
  /* Create a memory pool */
  self->pool = pipeline_create_memory_pool(self);
    
  return self;
}


void pipeline_add_stats(ndb_pipeline *self, 
                        const char *stat_key,
                        ADD_STAT add_stat, 
                        const void *cookie) {
  char key[128];

  DEBUG_ENTER();
  const Configuration & conf = get_Configuration();

  if(strncasecmp(stat_key,"ndb",3) == 0) {
    for(unsigned int i = 0 ; i < conf.nclusters ; i ++) {
      sprintf(key, "cl%d", i);
      conf.getConnectionPoolById(i)->add_stats(key, add_stat, cookie);
    }
  }
  else if(strncasecmp(stat_key,"errors",6) == 0) {
    ndb_error_logger_stats(add_stat, cookie);    
  }
  else if((strncasecmp(stat_key,"scheduler",9) == 0)
          || (strncasecmp(stat_key,"reconf",6) == 0)) {
    self->scheduler->add_stats(stat_key, add_stat, cookie);
  }
}
  

ENGINE_ERROR_CODE pipeline_flush_all(ndb_pipeline *self) {
  return ndb_flush_all(self);
}


/* The scheduler API */

void * scheduler_initialize(ndb_pipeline *self, scheduler_options *options) {
  Scheduler *s = 0;
  const char *cf = self->engine->startup_options.scheduler;
  options->config_string = 0;
  
  if(cf == 0 || *cf == 0) {
    s = new DEFAULT_SCHEDULER;
  }
  else if(!strncasecmp(cf, "stockholm", 9)) {
    s = new Scheduler_stockholm;
    options->config_string = & cf[9];
  }
  else if(!strncasecmp(cf,"S", 1)) {
    s = new S::SchedulerWorker;
    options->config_string = & cf[1];
  }
  else {
    return NULL;
  }
  
  s->init(self->id, options);

  return (void *) s;
}


void scheduler_shutdown(ndb_pipeline *self) {  
  self->scheduler->shutdown();
}


ENGINE_ERROR_CODE scheduler_schedule(ndb_pipeline *self, struct workitem *item) {
  return self->scheduler->schedule(item);
}


void scheduler_release(ndb_pipeline *self, struct workitem *item) {
  self->scheduler->release(item);
}



/* The slab allocator API */

int pipeline_get_size_class_id(size_t object_size) {
  int cls = 1;
  
  if(object_size) {
    object_size--;
    while(object_size >>= 1)    /* keep shifting */
      cls++;   
    
    if (cls < ALLIGATOR_POWER_SMALLEST)  cls = ALLIGATOR_POWER_SMALLEST;
    if (cls > ALLIGATOR_POWER_LARGEST)   cls = -1;
  }
  else 
    cls = 0;
  
  return cls;
}


void * pipeline_alloc(ndb_pipeline *self, int class_id) {
  allocator_slab_class *c;
  void * ptr = 0;
  
  if(class_id < ALLIGATOR_POWER_SMALLEST) return 0;
  if(class_id > ALLIGATOR_POWER_LARGEST)  return 0;

  c = & self->alligator[class_id];

  // common case alloc() is to pop a pointer from the list
  if(! pthread_mutex_lock(& c->lock)) {
    if(c->free_idx || malloc_new_slab(c)) 
      ptr = c->list[-- c->free_idx];   // pop
      pthread_mutex_unlock(& c->lock);
  }

  return ptr;
}


void pipeline_free(ndb_pipeline *self, void * ptr, int class_id ) {  
  if(class_id < ALLIGATOR_POWER_SMALLEST) return;
  if(class_id > ALLIGATOR_POWER_LARGEST)  return;
  
  allocator_slab_class *c = & self->alligator[class_id];

  /* begin critical section */
  if(! pthread_mutex_lock(& c->lock)) {
    if(c->free_idx == c->list_size) {   /* list is full; must grow */
      void **new_list;
      new_list = (void **) realloc(c->list, c->list_size * 2 * sizeof(void *));
      if(new_list) {
        c->list = new_list;
        c->list_size *= 2;
        c->list[c->free_idx++] = ptr;  // push
      }
    }
    else {      
      // common case free() is simply to push the freed pointer onto the list
      c->list[c->free_idx++] = ptr;  // push
    }
    pthread_mutex_unlock(& c->lock);     
  }
  /* end critical section */
}


/*** The high-level (pool) API */

memory_pool * pipeline_create_memory_pool(ndb_pipeline *self) {
  memory_pool *p;

  /* Use slab class 6 (64 bytes) for the first array in a new pool. */
  const int initial_slab_class = 6; 

  /* Initialize the global static class id */
  if(pool_slab_class_id == 0) {
    pool_slab_class_id = pipeline_get_size_class_id(sizeof(memory_pool));
  }

  /* Get a pool header */
  p = (memory_pool *) pipeline_alloc(self, pool_slab_class_id); 
  p->pipeline = self;
  
  /* Get an array. */
  p->head = (allocation_reference *) pipeline_alloc(self, initial_slab_class);

  /* Count it in the stats */
  p->total = (1 << initial_slab_class);  /* just the root array */
  p->size = 0;
  
  /* Initialize the array header */
  init_pool_header(& p->head[0], initial_slab_class);
  
  return p;
}


void * memory_pool_alloc(memory_pool *p, size_t sz) {  
  if(p->head[0].d.cells_idx == p->head[0].d.cells_total) {
    /* We must add a new list.  Make it twice as big as the previous one. */
    allocation_reference *old_head = p->head;
    int slab_class = old_head->d.slab_class;
    if(slab_class < ALLIGATOR_POWER_LARGEST) slab_class++;

    p->head = (allocation_reference *) pipeline_alloc(p->pipeline, slab_class);
    init_pool_header(p->head, slab_class);
    p->head->pointer = old_head;
    p->size += (1 << slab_class);
  }
  
  allocation_reference &r = p->head[p->head->d.cells_idx++];

  int slab_class = pipeline_get_size_class_id(sz);
  if(slab_class == -1) {  // large areas use system malloc
    r.d.sys_malloc = 1;
    r.pointer = malloc(sz);
    p->size += sz;
  }
  else {  // small areas use slab allocator
    r.d.sys_malloc = 0;
    r.d.slab_class = slab_class;
    r.pointer = pipeline_alloc(p->pipeline, r.d.slab_class);
    p->size += (1 << r.d.slab_class);
  }

  return r.pointer;
}


void memory_pool_free(memory_pool *pool) {  
  allocation_reference *next = pool->head;;
  allocation_reference *array;
  
  pool->total += pool->size; pool->size = 0;  // reset the size counter
  do {
    array = next;
    next = (allocation_reference *) array->pointer;
    for(unsigned int i = 1; i < array->d.cells_idx ; i++) {  // free each block
      allocation_reference &r = array[i];
      if(r.d.sys_malloc) 
        free(r.pointer);
      else
        pipeline_free(pool->pipeline, r.pointer, r.d.slab_class);
    }
    if(next) {  // if this isn't the last array, free it 
      pipeline_free(pool->pipeline, array, array->d.slab_class);
    }
    else {  // reset the slot counter
      array->d.cells_idx = 1;
    }
  } while(next);

  /* Reset the head */
  pool->head = array;
}


void memory_pool_destroy(memory_pool *pool) {
  assert(pool_slab_class_id > 0);
  pipeline_free(pool->pipeline, pool->head, pool->head[0].d.slab_class);
  pipeline_free(pool->pipeline, pool, pool_slab_class_id);
}


/* private utility functions follow */

void init_allocator(ndb_pipeline *self) {  
  for(int i = 0, size = 1 ; i <= ALLIGATOR_POWER_LARGEST ; i++) {
    init_slab_class(& self->alligator[i], size);
    size *= 2;
  }
  
  /* Set the static global workitem information, but only once */
  if(self->id == 0) { 
    workitem_class_id = pipeline_get_size_class_id(sizeof(struct workitem));
    size_t sz = self->alligator[workitem_class_id].size;
    workitem_actual_inline_buffer_size = 
      WORKITEM_MIN_INLINE_BUF + (sz - sizeof(struct workitem));
    DEBUG_PRINT("workitem slab class: %d, inline buffer: %d", 
                workitem_class_id, workitem_actual_inline_buffer_size);
  }
  
  /* Pre-allocate a new slab for certain special classes. */
  malloc_new_slab(& self->alligator[5]);  /* for key buffers */
  malloc_new_slab(& self->alligator[6]);  /* for key buffers and memory pools*/
  malloc_new_slab(& self->alligator[7]);  /* for key buffers */
  malloc_new_slab(& self->alligator[8]);  /* for key buffers */
  if(workitem_class_id > 8) 
    malloc_new_slab(& self->alligator[workitem_class_id]);   /* for workitems */
  malloc_new_slab(& self->alligator[13]);  /* The 8KB class, for row buffers */
  malloc_new_slab(& self->alligator[14]);  /* The 16KB class for 13K rows */
}


int init_slab_class(allocator_slab_class *c, int size) {
  c->size = size;
  c->perslab = ALLIGATOR_SLAB_SIZE / size;
  c->list = 0;
  c->list_size = 0;
  c->free_idx = 0;
  c->total = 0;
  return pthread_mutex_init(& c->lock, NULL);
}


/* malloc_new_slab: 
   get a slab from malloc() and add it to a class.
   once the scheduler has been started, you must hold p->lock to call this. 
*/
int malloc_new_slab(allocator_slab_class *c) {  
  unsigned int num = c->perslab;
  void **new_list;
  char *ptr;

  if (c->list_size < num) {
    new_list = (void **) realloc(c->list, num * sizeof(void *));
    if (new_list == 0)
      return 0;
    c->list = new_list;
    c->list_size = num;
  }

  void **cur = c->list;
  ptr = (char *) malloc(ALLIGATOR_SLAB_SIZE);
  if (ptr == 0) return 0;
  for (unsigned int i = 0; i < num; i++) {
    *cur = ptr;       /* push the pointer onto the list */
    cur++;            /* bump the list forward one position */
    ptr += c->size;   /* bump the pointer to the next block */
  }
  c->free_idx += num;
  c->total += ALLIGATOR_SLAB_SIZE;
 
  return 1;
}

/* init_pool_header()
*/
void init_pool_header(allocation_reference *head, int slab_class) {
  head->pointer = 0;
  head->d.is_header = 1;
  head->d.slab_class = slab_class;
  head->d.cells_total = (1 << slab_class) / sizeof(allocation_reference);
  head->d.cells_idx = 1;
}

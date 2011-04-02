/* -*- Mode: C; tab-width: 4; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 *
 * Thread-local slabs. 
 * Adapted from memcached's standard slabs allocator. 
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <sys/types.h>

#include "tls_slabs.h"


/*
 * Forward Declarations
 */
static int do_tls_slabs_newslab(tls_slabs_t * self, const unsigned int id);


/*
 * Figures out which slab class (chunk size) is required to store an item of
 * a given size.
 *
 * Given object size, return id to use when allocating/freeing memory for object
 * 0 means error: can't store such a large object
 */

unsigned int tls_slabs_clsid(tls_slabs_t * self, const size_t size) {
  int res = 1;
  
  if (size == 0) return 0;
  while (size > self->slabclass[res].size) res++;
  
  /* an attempt to allocate a too-big tls_slab is a programmer error */
  assert(res < self->power_largest);

  return res;
}


/**
 * Determines the chunk sizes and initializes the slab class descriptors
 * accordingly.
 */
tls_slabs_t * tls_slabs_init(size_t chunk_size, double growth_factor, 
                              size_t max_size, size_t mem_limit,
                              char * mesgbuffer) {

  unsigned int i, size;
  char *mesg = mesgbuffer;

  tls_slabs_t * self = (tls_slabs_t *) calloc(1, sizeof(tls_slabs_t)); 
  
  pthread_mutex_init( &self->lock, NULL);
  self->mem_limit = mem_limit;
  
  memset(self->slabclass, 0, sizeof(self->slabclass));
  
  for(size = chunk_size, i = 1; size < max_size * growth_factor ; i++) {
    /* Make sure items are always n-byte aligned */
    if (size % CHUNK_ALIGN_BYTES)
      size += CHUNK_ALIGN_BYTES - (size % CHUNK_ALIGN_BYTES);
    
    self->slabclass[i].size = size;
    self->slabclass[i].perslab = max_size / self->slabclass[i].size;
    size *= growth_factor;    
  }

  mesg += sprintf(mesg, "  Class %2u: %6u item%s @ %6u bytes ... (factor %f)\n", 
                  1, self->slabclass[1].perslab,
                  self->slabclass[1].perslab == 1 ? " " : "s",  
                  self->slabclass[1].size, growth_factor);
  mesg += sprintf(mesg, "  Class %2u: %6u item%s @ %6u bytes.\n", i-1,
                  self->slabclass[i-1].perslab,
                  self->slabclass[i-1].perslab == 1 ? " " : "s",  
                  self->slabclass[i-1].size);
  
  self->power_largest = i;

  return self;
}


static int tls_grow_slab_list (tls_slabs_t * self, const unsigned int id) {
  tls_slabclass_t *p = & self->slabclass[id];
  if (p->slabs == p->list_size) {
    size_t new_size =  (p->list_size != 0) ? p->list_size * 2 : 16;
    void *new_list = realloc(p->slab_list, new_size * sizeof(void *));
    if (new_list == 0) return 0;
    p->list_size = new_size;
    p->slab_list = new_list;
  }
  return 1;
}

static int do_tls_slabs_newslab(tls_slabs_t * self, const unsigned int id) {
  tls_slabclass_t *p = & self->slabclass[id];
  int len = p->size * p->perslab;
  char *ptr;
  
  if ((self->mem_limit && self->mem_malloced + len > self->mem_limit && p->slabs > 0) ||
      (tls_grow_slab_list(self, id) == 0) ||
      ((ptr = malloc((size_t)len)) == 0)) {
    
    return 0;
  }
  
  memset(ptr, 0, (size_t)len);
  p->end_page_ptr = ptr;
  p->end_page_free = p->perslab;
  
  p->slab_list[p->slabs++] = ptr;
  self->mem_malloced += len;
  
  return 1;
}


static void *do_tls_slabs_alloc(tls_slabs_t * self, const size_t size, unsigned int id) {
  tls_slabclass_t *p;
  void *ret = NULL;
  
  assert(id >= 1 && id <= self->power_largest);
  
  p = &self->slabclass[id];
    
  /* fail unless we have space at the end of a recently allocated page,
   we have something on our freelist, or we could allocate a new page */
  if (! (p->end_page_ptr != 0 || p->sl_curr != 0 ||
         do_tls_slabs_newslab(self, id) != 0)) {
    /* We don't have more memory available */
    ret = NULL;
  } else if (p->sl_curr != 0) {
    /* return off our freelist */
    ret = p->slots[--p->sl_curr];
  } else {
    /* if we recently allocated a whole page, return from that */
    assert(p->end_page_ptr != NULL);
    ret = p->end_page_ptr;
    if (--p->end_page_free != 0) {
      p->end_page_ptr = ((caddr_t)p->end_page_ptr) + p->size;
    } else {
      p->end_page_ptr = 0;
    }
  }
  
  if (ret) {
    p->requested += size;
  }
  
  return ret;
}

static void do_tls_slabs_free(tls_slabs_t * self, void *ptr, const size_t size, unsigned int id) {
  tls_slabclass_t *p;
  
  assert(id >= 1 && id <= self->power_largest);
  
  p = &self->slabclass[id];
    
  if (p->sl_curr == p->sl_total) { /* need more space on the free list */
    int new_size = (p->sl_total != 0) ? p->sl_total * 2 : 16;  /* 16 is arbitrary */
    void **new_slots = realloc(p->slots, new_size * sizeof(void *));
    if (new_slots == 0)
      return;
    p->slots = new_slots;
    p->sl_total = new_size;
  }
  p->slots[p->sl_curr++] = ptr;
  p->requested -= size;
  return;
}

void * tls_slabs_alloc(tls_slabs_t * self, size_t size) {
  void *ret;
  
  pthread_mutex_lock(& self->lock);
  ret = do_tls_slabs_alloc(self, size, tls_slabs_clsid(self, size));
  pthread_mutex_unlock(& self->lock);
  return ret;
}

void tls_slabs_free(tls_slabs_t * self, void *ptr, size_t size) {
  pthread_mutex_lock(& self->lock);
  do_tls_slabs_free(self, ptr, size, tls_slabs_clsid(self, size));
  pthread_mutex_unlock(& self->lock);
}


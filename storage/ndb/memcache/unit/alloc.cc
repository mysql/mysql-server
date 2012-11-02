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
#include <assert.h>
#include <pthread.h>

#include "config.h"
#include "atomics.h"
#include "ndb_pipeline.h"

#include "all_tests.h"

#define TEST_ALLOC_BLOCKS 34

int run_allocator_test(QueryPlan *, Ndb *, int v) {
  struct request_pipeline *p = get_request_pipeline(0, NULL);
  
  memory_pool *p1 = pipeline_create_memory_pool(p);
  int sz = 13;
  uint tot = 0;
  void *v1, *v2;
  for(int i = 0 ; i < TEST_ALLOC_BLOCKS ; i++) {
    v1 = memory_pool_alloc(p1, sz);     tot += sz;
    v2 = memory_pool_alloc(p1, sz + 1); tot += sz + 1;
    sz = (int) (sz * 1.25);
  }

  detail(v, "Total requested: %d  granted: %lu \n", tot, p1->size);
  require(p1->size >= tot);
  /* Get total before freeing the pool */
  uint old_total = p1->size + p1->total;
  memory_pool_free(p1);
  /* The new total must equal the old_total */
  require(old_total == p1->total);

  memory_pool_destroy(p1);
  memory_pool_destroy(p->pool);  /* also destroy the pipeline's own pool */
  
  for(int i = ALLIGATOR_POWER_SMALLEST ; i < ALLIGATOR_POWER_LARGEST ; i++) {
    int list_size   = p->alligator[i].list_size;
    int free_slot   = p->alligator[i].free_idx;
    size_t alloc_sz = p->alligator[i].total;
    
    detail(v, "Class %d idx %d used %lu \n", i, list_size - free_slot, alloc_sz);
    /* After we destroy the pool, every slab must have 0 allocated blocks */
    require(list_size - free_slot == 0);
    /* But it must have a non-zero size, indicating that it has been used */
    require(alloc_sz > 0);
  }
  pass;
}  


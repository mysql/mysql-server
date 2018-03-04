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
#include <string.h>
#include <assert.h>

#include <memcached/engine.h>

#include "debug.h"
#include "workitem.h"
#include "default_engine.h"
#include "ndb_engine.h"


extern int workitem_class_id;   /* from ndb_pipeline.cc */
extern int workitem_actual_inline_buffer_size;  /* from ndb_pipeline.cc */

const char * workitem_get_key_suffix(workitem *item) {
  return item->key + (item->base.nkey - item->base.nsuffix);
}


bool workitem_allocate_rowbuffer_1(workitem *i, size_t buffersize) {  
  i->rowbuf1_cls = pipeline_get_size_class_id(buffersize);
  i->row_buffer_1 = pipeline_alloc(i->pipeline, i->rowbuf1_cls);
  DEBUG_PRINT_DETAIL(" %d [cls %d]", buffersize, i->rowbuf1_cls);

  return (i->row_buffer_1);
}

bool workitem_allocate_rowbuffer_2(workitem *i, size_t buffersize) {  
  i->rowbuf2_cls = pipeline_get_size_class_id(buffersize);
  i->row_buffer_2 = pipeline_alloc(i->pipeline, i->rowbuf2_cls);
  DEBUG_PRINT_DETAIL(" %d [cls %d]", buffersize, i->rowbuf2_cls);
  
  return (i->row_buffer_2);
}


void workitem__initialize(workitem *, ndb_pipeline *, int, prefix_info_t, 
                          const void *, int, const char *);

void workitem__initialize(workitem *item, ndb_pipeline *pipeline, int verb, 
                          prefix_info_t prefix, const void *cookie,
                          int nkey, const char *key)
{
  int sz;
  memset(item, 0, sizeof(workitem));      /* zero out the item */
  item->base.nkey = nkey;  
  item->base.verb = verb;
  item->prefix_info = prefix;
  item->pipeline = pipeline;
  item->cookie = cookie;   
  item->key = key;
  item->id = pipeline->nworkitems++;
  
  sz = workitem_get_key_buf_size(nkey);
  if(sz > workitem_actual_inline_buffer_size) {
    item->keybuf1_cls = pipeline_get_size_class_id(sz);
    item->ndb_key_buffer = pipeline_alloc(pipeline, item->keybuf1_cls);
  }
  else {
    item->ndb_key_buffer = & item->inline_buffer.buffer[0];
  }
}


workitem *new_workitem_for_store_op(ndb_pipeline *pipeline, int verb, 
                                    prefix_info_t prefix,
                                    const void *cookie, hash_item *i,
                                    uint64_t * cas)
{
  workitem *newitem;
  newitem = pipeline_alloc(pipeline, workitem_class_id);
  if(newitem == NULL) return NULL;

  workitem__initialize(newitem, pipeline, verb, prefix, cookie, 
                       i->nkey, hash_item_get_key(i));
  newitem->cache_item = i;
  newitem->cas = cas;
  *cas = hash_item_get_cas(i);
    
  return newitem;
}


workitem *new_workitem_for_delete_op(ndb_pipeline *pipeline,
                                     prefix_info_t prefix, const void *cookie,
                                     int nkey, const char *key, uint64_t *cas)
{
  workitem *newitem;
  newitem = pipeline_alloc(pipeline, workitem_class_id);
  if(newitem == NULL) return NULL;
  
  workitem__initialize(newitem, pipeline, OP_DELETE, prefix, cookie, nkey, key);
  newitem->cas = cas;
  
  return newitem;
}


workitem *new_workitem_for_get_op(workitem *previous, ndb_pipeline *pipeline,
                                  prefix_info_t prefix, const void *cookie,
                                  int nkey, const char *key)
{
  workitem *newitem;
  newitem = pipeline_alloc(pipeline, workitem_class_id);
  if(newitem == NULL) return NULL;
    
  workitem__initialize(newitem, pipeline, OP_READ, prefix, cookie, nkey, key);

  /* Make a new copy of the key and store it in key buffer #2.
     The original copy (in the connection request) may become invalid.  */
  if((workitem_actual_inline_buffer_size - 3) > (2 * nkey)) {    
   /* use space at the end of the inline buffer */
    newitem->key_buffer_2 = 
        & newitem->inline_buffer.buffer[0] + workitem_actual_inline_buffer_size - nkey;
  }
  else {
    newitem->keybuf2_cls = pipeline_get_size_class_id(nkey);
    newitem->key_buffer_2 = pipeline_alloc(newitem->pipeline, newitem->keybuf2_cls);
  }
  memcpy(newitem->key_buffer_2, key, nkey);
  newitem->key = newitem->key_buffer_2;  /* refer to our own copy now. */

  /* For a multi-key get, "previous" may be non-null */
  newitem->previous = previous;
  
  return newitem;
}


workitem *new_workitem_for_arithmetic(ndb_pipeline *pipeline, 
                                      prefix_info_t prefix, const void *cookie,
                                      const char *key, int nkey,
                                      bool increment, bool create,
                                      uint64_t delta, uint64_t initial, 
                                      uint64_t *cas)
{
  workitem *item;
  item = new_workitem_for_get_op(NULL, pipeline, prefix, cookie, nkey, key);
  item->base.verb = OP_ARITHMETIC;
  item->base.math_incr = increment;
  item->base.math_create = create;  
  item->math_flags = delta;
  item->math_value = initial;
  item->cas = cas;
  return item;
}                                       


const char * workitem_get_operation(workitem *item) {
  /* From memcached/types.h: */
  const char * verbs1[] = { "NONE", "add", "set", "replace", 
                            "append", "prepend", "cas" };

  /* From ndbmemcache_global.h: */
  const char * verbs2[] = { "read", "delete", "arithmetic", "scan"};

  if(item->base.verb >= OP_READ) {
    return verbs2[item->base.verb - OP_READ];
  }
  else {
    return verbs1[item->base.verb];
  }
}


void workitem_free(workitem *item)
{
  /* It's OK to free all of these; pipeline_free() with class_id 0 is a no-op */
  pipeline_free(item->pipeline, item->row_buffer_1, item->rowbuf1_cls);
  pipeline_free(item->pipeline, item->row_buffer_2, item->rowbuf2_cls);
  pipeline_free(item->pipeline, item->ndb_key_buffer, item->keybuf1_cls);
  pipeline_free(item->pipeline, item->key_buffer_2, item->keybuf2_cls);

  pipeline_free(item->pipeline, item, workitem_class_id);
}


size_t workitem_get_key_buf_size(int nkey) {
  size_t bufsz;
  bufsz = nkey + 3;       // at least key + 2 length bytes + null terminator
  return (bufsz < 9) ? 9 : bufsz;  // A packed DECIMAL could need 9 bytes
}


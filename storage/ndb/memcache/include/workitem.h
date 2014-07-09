/*
 Copyright (c) 2011, 2013, Oracle and/or its affiliates. All rights reserved.
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

#ifndef NDBMEMCACHE_WORKITEM_H
#define NDBMEMCACHE_WORKITEM_H

#include "KeyPrefix.h"
#include "hash_item_util.h"
#include "ndb_pipeline.h"
#include "status_block.h"

#ifdef __cplusplus
#define CPP
class QueryPlan;
class ExternalValue;
class NdbInstance;
#else
#define CPP struct
#endif

typedef struct workitem {
  struct {
    unsigned nkey        : 8;  /*! length of key */
    unsigned nsuffix     : 8;  /*! length of key after prefix */
    unsigned verb        : 4;  /*! READ, DELETE, ADD, STORE, etc. */
    unsigned math_incr   : 1;  /*! incr, or decr ? */
    unsigned math_create : 1;  /*! create record if not existing */
    unsigned use_ext_val : 1;  /*! special handling: external large values */
    unsigned has_value   : 1;  /*! are we able to use a no-copy value? */
    unsigned retries     : 3;  /*! how many times this job has been retried */
    unsigned complete    : 1;  /*! is this operation finished? */
    unsigned _unused_2   : 2;  /*! */
    unsigned reschedule  : 1;  /*! inform scheduler to send and poll again */
    unsigned cas_owner   : 1;  /*! set if the NDB engine must create a CAS ID */
  } base;
  unsigned int id;
  struct workitem *previous;   /*! used to chain workitems in multi-key get */
  prefix_info_t prefix_info;   /*! essential info for the key prefix */
  uint64_t * cas;              /*! in/out CAS */
  uint32_t math_flags;         /*! IN: math_delta  OUT: flags */
  uint64_t math_value;         /*! IN: incr initial value; OUT: incr result */
  hash_item * cache_item;      /*! used for write requests */
  ndb_pipeline *pipeline;      /*! pointer back to request pipeline */
  CPP NdbInstance *ndb_instance;
                               /*! pointer to ndb instance, if applicable */
  const void *cookie;          /*! memcached's connection cookie */
  CPP QueryPlan *plan;         /*! QueryPlan for resolving this request */
  CPP ExternalValue *ext_val;  /*! ExternalValue */
  const char *key;             /*! pointer to the key */
  void * next_step;            /*! a worker_step function in ndb_worker.cc */
  status_block *status;        /*! A static status_block in ndb_worker.cc */
  char *value_ptr;             /*! No-copy value -- Record::decodeNoCopy() */
  size_t value_size;           /*! size of value (no-copy or in hash_item) */
  char *row_buffer_1;          /*! A buffer used for data rows */
  char *row_buffer_2;          /*! A buffer used for data rows */
  char *ndb_key_buffer;        /*! The key as encoded for NDB */
  char *key_buffer_2;          /*! An extra copy of the memcache key */
  unsigned char rowbuf1_cls;   /*! Slab class id for row_buffer_1 */
  unsigned char rowbuf2_cls;   /*! Slab class id for row_buffer_2 */
  unsigned char keybuf1_cls;   /*! Slab class of ndb key; 0 = stored inline */
  unsigned char keybuf2_cls;   /*! Slab class id for key_buffer_2 */
  union {
    char buffer[WORKITEM_MIN_INLINE_BUF];  
    uint64_t coerce_8byte_alignment;
  } inline_buffer;             /*! Must be the final item */
} workitem;


/* This API has C linkage */
DECLARE_FUNCTIONS_WITH_C_LINKAGE

/*! Create a workitem to use for SET operations.  It contains a link to a hash
    item, and it does not include an inline buffer. 
*/
workitem *new_workitem_for_store_op(ndb_pipeline *pipeline,
                                     int verb,  prefix_info_t prefix,
                                     const void *cookie, hash_item *mc_item,
                                     uint64_t *cas);

/*! Create a workitem to use for DELETE operations.  
    It will include an inline buffer large enough to encode the key.
*/   
workitem *new_workitem_for_delete_op(ndb_pipeline *pipeline,
                                     prefix_info_t prefix, const void *cookie,
                                     int nkey, const char *key, uint64_t *cas);

/*! Create a workitem to use for GET operations.  
    It will include an inline buffer large enough for two copies of the key.
*/ 
workitem *new_workitem_for_get_op(workitem *prev, ndb_pipeline *pipeline, 
                                  prefix_info_t prefix, const void *cookie, 
                                  int nkey, const char *key);

/*! Create a workitem to use for INCR and DECR operations.  
*/ 
workitem *new_workitem_for_arithmetic(ndb_pipeline *pipeline, 
                                      prefix_info_t prefix, const void *cookie,
                                      const char *key, int nkey,
                                      bool increment, bool create,
                                      uint64_t delta, uint64_t initial, 
                                      uint64_t *cas);

/*! Allocate an extra (non-inline) buffer.  The workitem owns the buffer,
    and the buffer will be freed by workitem_free().  
*/
bool workitem_allocate_rowbuffer_1(workitem *, size_t);

/*! Allocate an extra (non-inline) buffer.  The workitem owns the buffer,
    and the buffer will be freed by workitem_free().  
*/
bool workitem_allocate_rowbuffer_2(workitem *, size_t);

/*! returns the name of the memcached operation stored in the workitem.
*/
const char * workitem_get_operation(workitem *);

/*!  Free the workitem.  Also free any extra row_buffer that was allocated
     using workitem_allocate_buffer().
*/
void workitem_free(workitem *);

/*!  Return the part of the key past the recognized prefix
*/
const char * workitem_get_key_suffix(workitem *item);

/*!  Return the size of key buffer needed for a key of length "nkey" 
*/
size_t workitem_get_key_buf_size(int nkey);

END_FUNCTIONS_WITH_C_LINKAGE
    
#endif

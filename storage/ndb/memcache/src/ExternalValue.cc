/*
 Copyright (c) 2011, 2015, Oracle and/or its affiliates. All rights
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

#include <my_config.h>
#include <arpa/inet.h>
#include <assert.h>
#include <NdbApi.hpp>

#include "workitem.h"
#include "NdbInstance.h"
#include "Operation.h"
#include "Scheduler.h"
#include "status_block.h"
#include "ExpireTime.h"
#include "ExternalValue.h"
#include "TabSeparatedValues.h"
#include "ndb_error_logger.h"

/* Externs */
extern EXTENSION_LOGGER_DESCRIPTOR *logger;
extern status_block status_block_memcache_error;
extern status_block status_block_misc_error;
extern status_block status_block_too_big;
extern status_block status_block_cas_mismatch;
extern status_block status_block_item_not_found;
extern void worker_set_cas(ndb_pipeline *p, uint64_t *cas);

/* Callback functions from ndb_worker.cc */
extern ndb_async_callback callback_main;
extern ndb_async_callback callback_close;
extern worker_step worker_finalize_write;
extern worker_step worker_commit;
extern worker_step worker_close;
extern worker_step worker_append;

/* Callbacks defined here */
ndb_async_callback callback_ext_parts_read;
ndb_async_callback callback_ext_write;

worker_step finalize_append;
worker_step delete_after_header_read;

int pad8(int);
inline int pad8(int sz) {
  int bad_offset = sz % 8;
  if(bad_offset) sz += (8 - bad_offset);
  return sz;
}


/*************************************************************************/
/* Public static class methods */

/* Called from Configuration reader
*/
TableSpec * ExternalValue::createContainerRecord(const char *sqltab) {
  TableSpec *t = new TableSpec(sqltab, "id,part", "content");
  return t;
}


/* This is called from FLUSH_ALL.
   It returns the number of parts deleted.
   It uses a memory pool, passed in, to allocate key buffers.
*/
int ExternalValue::do_delete(memory_pool *mpool, NdbTransaction *delTx, 
                             QueryPlan *plan, Operation & op) {
  Uint32 id, nparts = 0;
  QueryPlan * extern_plan = plan->extern_store;
  
  if(extern_plan 
     && ! (op.isNull(COL_STORE_EXT_SIZE) || op.isNull(COL_STORE_EXT_ID))) {

    /* How many parts? */
    Uint32 stripe_size = extern_plan->val_record->value_length;
    Uint32 len = op.getIntValue(COL_STORE_EXT_SIZE);
    id  = op.getIntValue(COL_STORE_EXT_ID);  
    nparts = len / stripe_size;
    if(len % stripe_size) nparts += 1;

    /* Delete them */
    int key_size = extern_plan->key_record->rec_size;
    
    for(Uint32 i = 0; i < nparts ; i++) {
      Operation part_op(extern_plan);
      part_op.key_buffer = (char *) memory_pool_alloc(mpool, key_size);
      
      part_op.clearKeyNullBits();
      part_op.setKeyPartInt(COL_STORE_KEY + 0, id);
      part_op.setKeyPartInt(COL_STORE_KEY + 1, i);    
      part_op.deleteTuple(delTx);
    }
  }
  return nparts;
}


bool ExternalValue::setupKey(workitem *item, Operation &op) { 
  const TableSpec & spec = * (item->plan->spec);
  op.key_buffer = item->ndb_key_buffer;
  const char *dbkey = workitem_get_key_suffix(item);
  
  return op.setKey(spec.nkeycols, dbkey, item->base.nsuffix);
}


/* Operation starters called from ndb_worker.  These are static so we don't 
   have to allocate the ExternalValue unless its first step succeeds. 
*/
op_status_t ExternalValue::do_delete(workitem *item) {
  return do_read_header(item, callback_main, delete_after_header_read);
}


op_status_t ExternalValue::do_write(workitem *item) {
  uint32_t & len = item->cache_item->nbytes;
  
  if(len > item->plan->max_value_len) {
    return op_overflow;
  }

  if(item->base.verb == OPERATION_ADD) {
    /* In this case we need to create an object, but then delete it on error */
    ExternalValue *ext_val = new ExternalValue(item);
    op_status_t r = ext_val->do_insert();
    if(r != op_prepared)
      delete ext_val;
    return r;
  }
  else
    return do_read_header(item, callback_ext_write, 0);
}


/* Read the header with an Exclusive lock, and execute NoCommit */
op_status_t ExternalValue::do_read_header(workitem *item,
                                          ndb_async_callback the_callback,
                                          worker_step the_next_step) {
  DEBUG_ENTER_DETAIL();
  Operation op(item->plan, OP_READ);
  op.key_buffer = item->ndb_key_buffer;
  
  op.readSelectedColumns();
  op.readColumn(COL_STORE_EXT_ID);
  op.readColumn(COL_STORE_EXT_SIZE);
  op.readColumn(COL_STORE_CAS);
  
  if(! setupKey(item, op)) {
    return op_bad_key;
  }
  
  workitem_allocate_rowbuffer_1(item, op.requiredBuffer());
  op.buffer = item->row_buffer_1;
  
  NdbTransaction *tx = op.startTransaction(item->ndb_instance->db);
  if(! tx) {
    log_ndb_error(item->ndb_instance->db->getNdbError());
    return op_failed;
  }
  if(! op.readTuple(tx, NdbOperation::LM_Exclusive)) {
    log_ndb_error(tx->getNdbError());
    tx->close();
    return op_failed;
  }
  
  item->next_step = (void *) the_next_step;
  Scheduler::execute(tx, NdbTransaction::NoCommit, the_callback, item, YIELD);
  return op_prepared;
}


void ExternalValue::append_after_read(NdbTransaction *tx, workitem *item) {
  DEBUG_PRINT_DETAIL(" %d.%d", item->pipeline->id, item->id);
  
  char * inline_val = 0;
  size_t current_len = 0;
  uint32_t & affix_len = item->cache_item->nbytes;
  
  Operation readop(item->plan, OP_READ);
  readop.buffer = item->row_buffer_1;
    
  /* Several possibilities: 
   A. the old value was short, and the new value is also short.
   B. the old value was short and the new value is long.
   C. the old value is long and the new value is of an allowable length.
   D. the new value is too long.
   */
  if(readop.isNull(COL_STORE_EXT_SIZE)) { /* old value is short */
    readop.getStringValueNoCopy(COL_STORE_VALUE, & inline_val, & current_len);
    if(! item->plan->shouldExternalizeValue(current_len + affix_len)) {
      /* (A) new value is short; restart using standard code path  */
      item->base.use_ext_val = 0;
      return worker_append(tx, item);
    }
  }
  else { /* old value is long */
    current_len = readop.getIntValue(COL_STORE_EXT_SIZE);
  }
  
  if(current_len + affix_len > item->plan->max_value_len) {   // (D) too long
    item->status = & status_block_too_big;
    return worker_close(tx, item);
  }
  
  /* Possibilities (B) and (C) remain.  Instantiate an ExternalValue.  */
  
  assert(item->ext_val == 0);
  item->ext_val = new ExternalValue(item, tx);

  /* Generate a new CAS */
  if(item->ext_val->do_server_cas) {
    worker_set_cas(item->pipeline, item->cas);  
    hash_item_set_cas(item->cache_item, * item->cas);
  }
  
  if(! item->ext_val->old_hdr.readFromHeader(readop)) {
    /* (B) old value was short */
    return item->ext_val->affix_short(current_len, inline_val);
  }

  /* (C) old value is long.  Read the parts. */
  if(item->base.verb == OPERATION_PREPEND)  
    item->ext_val->readParts();
  else {
    bool r = item->ext_val->readFinalPart();
    /* If value ends on a part boundary, skip reading it */
    if(! r) return item->ext_val->append(); 
  }

  Scheduler::execute(tx, NdbTransaction::NoCommit, 
                     callback_ext_parts_read, item, RESCHEDULE);
}


/*************************************************************************/
/* Public non-static instance methods */

/* Constructor */
ExternalValue::ExternalValue(workitem *item, NdbTransaction *t) :
  old_hdr(item->plan->extern_store->val_record->value_length),  // (part size)
  new_hdr(item->plan->extern_store->val_record->value_length),
  expire_time(item),
  tx(t),
  wqitem(item),
  ext_plan(item->plan->extern_store),
  value(0),
  value_size_in_header(item->plan->row_record->value_length),
  stored_cas(0)
{
  DEBUG_ENTER();
  do_server_cas = (item->prefix_info.has_cas_col && item->cas);
  wqitem->ext_val = this;
  pool = pipeline_create_memory_pool(wqitem->pipeline);
}


/* Destructor */
ExternalValue::~ExternalValue() {
  DEBUG_ENTER_DETAIL();
  memory_pool_free(pool);
  memory_pool_destroy(pool);
  wqitem->ext_val = 0;
}


/* Called after a read operation */
void ExternalValue::worker_read_external(Operation &op,
                                         NdbTransaction *the_read_tx) {
  tx = the_read_tx;
  old_hdr.readFromHeader(op);

  if(expire_time.stored_item_has_expired(op)) {
    DEBUG_PRINT("EXPIRED");
    deleteParts();
    delete_expired_item(wqitem, tx);
    return;
  }

  if(wqitem->prefix_info.has_flags_col && ! op.isNull(COL_STORE_FLAGS))
    wqitem->math_flags = htonl(op.getIntValue(COL_STORE_FLAGS));
  else if(wqitem->plan->static_flags)
    wqitem->math_flags = htonl(wqitem->plan->static_flags);
  else
    wqitem->math_flags = 0;
  
  readParts();
  Scheduler::execute(tx, NdbTransaction::Commit, 
                     callback_ext_parts_read, wqitem, RESCHEDULE);
}


/*************************************************************************/
/* Spec Methods */

void ExternalValue::Spec::setLength(int len) {
  length = len;
  nparts = length / part_size;
  if(length % part_size) nparts += 1;
}


bool ExternalValue::Spec::readFromHeader(Operation &op) {
  if(op.isNull(COL_STORE_EXT_ID))
    return false;
  else
    id = op.getIntValue(COL_STORE_EXT_ID);

  if(op.isNull(COL_STORE_EXT_SIZE))
    return false;
  
  setLength(op.getIntValue(COL_STORE_EXT_SIZE));
  DEBUG_PRINT_DETAIL("%llu/%lu (%d parts of size %lu)", id, length, nparts, part_size);
  return true;
}


/*************************************************************************/
/*  Private Methods */


inline void ExternalValue::finalize_write() {
  wqitem->next_step = (void *) worker_finalize_write;
  Scheduler::execute(tx, NdbTransaction::Commit, 
                     callback_main, wqitem, RESCHEDULE);
}


op_status_t ExternalValue::do_insert() {
  if(! insert()) {
    return op_overflow;
  }

  wqitem->next_step = (void *) worker_finalize_write;
  
  Scheduler::execute(tx, NdbTransaction::Commit, callback_main, wqitem, YIELD);
  return op_prepared;  
}


inline void ExternalValue::readStoredCas(Operation &op) {
  if(wqitem->plan->spec->cas_column)
    stored_cas = op.getBigUnsignedValue(COL_STORE_CAS);
}


void ExternalValue::insert_after_header_read() {
  bool r = insert();

  if(r)
    finalize_write();
  else {
    log_ndb_error(tx->getNdbError());
    wqitem->status = & status_block_misc_error;
    worker_commit(tx, wqitem);
  }
}


void ExternalValue::update_after_header_read() {
  /* Read the length, id, and stored cas from the header row */
  DEBUG_ENTER_DETAIL();
  Operation read_op(wqitem->plan, OP_READ);
  read_op.buffer = wqitem->row_buffer_1;
  old_hdr.readFromHeader(read_op);

  /* Do the CAS check */
  readStoredCas(read_op);
  if(wqitem->base.verb == OPERATION_CAS && *wqitem->cas != stored_cas) {
    DEBUG_PRINT("CAS Mismatch: IN:%llu  STORED:%llu", * wqitem->cas, stored_cas);
    * wqitem->cas = 0ULL;  // set cas=0 in the response
    wqitem->status = & status_block_cas_mismatch;
    return worker_commit(tx, wqitem);    
  }

  /* Set up the new value */
  new_hdr.id = old_hdr.id ? old_hdr.id : ext_plan->getAutoIncrement();
  new_hdr.setLength(wqitem->cache_item->nbytes);
  value = hash_item_get_data(wqitem->cache_item);

  update();
  
  finalize_write();
}


bool ExternalValue::update() {
  /* If the old value was long, delete the parts */
  if(shouldExternalize(old_hdr.length))
    deleteParts();
  
  /* Get a new Operation on the header row */
  Operation write_op(wqitem);
  
  /* Set the key */
  setupKey(wqitem, write_op);
    
  /* Use row buffer 2 */
  workitem_allocate_rowbuffer_2(wqitem, write_op.requiredBuffer());
  write_op.buffer = wqitem->row_buffer_2;
  write_op.setNullBits();

  /* Generate a new CAS */
  worker_set_cas(wqitem->pipeline, wqitem->cas);        // generate a new value
  hash_item_set_cas(wqitem->cache_item, * wqitem->cas); // store it
    
  /* Write the main row */
  setMiscColumns(write_op);
  setValueColumns(write_op);

  write_op.updateTuple(tx);

  /* If the new value is long, create parts */
  if(shouldExternalize(new_hdr.length))
    insertParts(value, new_hdr.length, new_hdr.nparts, 0);

  return true;
}


bool ExternalValue::deleteParts() {
  int key_size = pad8(ext_plan->key_record->rec_size);
  char * key_buffer = (char *) memory_pool_alloc(pool, old_hdr.nparts * key_size);

  for(int i = 0; i < old_hdr.nparts ; i++) {
    Operation part_op(ext_plan);
    part_op.key_buffer = key_buffer + (i * key_size);
    
    part_op.clearKeyNullBits();
    part_op.setKeyPartInt(COL_STORE_KEY + 0, old_hdr.id);
    part_op.setKeyPartInt(COL_STORE_KEY + 1, i);
    
    part_op.deleteTuple(tx);
  }
  return true;
}


bool ExternalValue::readParts() {
  int key_size = pad8(ext_plan->key_record->rec_size);
  int row_size = pad8(ext_plan->val_record->rec_size);
  
  char * key_buffer = (char *) memory_pool_alloc(pool, old_hdr.nparts * key_size);
  value = (char *) memory_pool_alloc(pool, old_hdr.nparts * row_size);
  
  if(key_buffer == 0 || value == 0) 
    return false;
  
  for(int i = 0; i < old_hdr.nparts ; i++) {
    Operation part_op(ext_plan, OP_READ);
    part_op.key_buffer = key_buffer + (i * key_size);
    part_op.buffer = value + (i * row_size);
    
    part_op.clearKeyNullBits();
    part_op.setKeyPartInt(COL_STORE_KEY + 0, old_hdr.id);
    part_op.setKeyPartInt(COL_STORE_KEY + 1, i);
    
    part_op.readTuple(tx, NdbOperation::LM_SimpleRead);
  }
  return true;
}


bool ExternalValue::readFinalPart() {
  /* This is used in append().  If the old value ends exactly on a part 
     boundary, we skip reading it. */
  if(old_hdr.nparts % old_hdr.part_size == 0) 
    return false;
  
  Operation part_op(ext_plan, OP_READ);
  part_op.key_buffer = (char *) 
    memory_pool_alloc(pool, part_op.requiredKeyBuffer());
  workitem_allocate_rowbuffer_2(wqitem, part_op.requiredBuffer());
  part_op.buffer = wqitem->row_buffer_2;

  part_op.clearKeyNullBits();
  part_op.setKeyPartInt(COL_STORE_KEY + 0, old_hdr.id);
  part_op.setKeyPartInt(COL_STORE_KEY + 1, old_hdr.nparts - 1);

  part_op.readTuple(tx, NdbOperation::LM_SimpleRead);

  return true;
}  
  

bool ExternalValue::insertParts(char * val, size_t val_length, int nparts, int offset) {
  const size_t part_size = new_hdr.part_size;
  const Uint64 ext_id    = new_hdr.id;
  assert(part_size);
  assert(ext_id);
  assert(nparts);

  Operation null_op(ext_plan);
  int key_size = pad8(null_op.requiredKeyBuffer());
  int row_size = pad8(null_op.requiredBuffer());
  
  char * key_buffer = (char *) memory_pool_alloc(pool, nparts * key_size);
  char * row_buffer = (char *) memory_pool_alloc(pool, nparts * row_size);
  
  if(key_buffer == 0 || row_buffer == 0) 
    return false;

  size_t this_part_size = part_size;
  size_t nleft = val_length;
  int i = 0;
  while(nleft) {
    this_part_size = (nleft > part_size ? part_size : nleft);

    const char * start = val + (i * part_size);
    
    Operation part_op(ext_plan);
    part_op.key_buffer = key_buffer + (i * key_size);
    part_op.buffer = row_buffer + (i * row_size);
    
    part_op.clearKeyNullBits();
    part_op.setKeyPartInt(COL_STORE_KEY + 0, ext_id);
    part_op.setKeyPartInt(COL_STORE_KEY + 1, offset + i);
    
    part_op.setColumnInt(COL_STORE_KEY + 0, ext_id);
    part_op.setColumnInt(COL_STORE_KEY + 1, offset + i);
    part_op.setColumn(COL_STORE_VALUE, start, this_part_size);
    
    part_op.insertTuple(tx);

    nleft -= this_part_size;
    i++;
  }
  if(this_part_size == part_size) {
    DEBUG_PRINT("%d parts of size %d exactly", nparts, part_size);
  }
  else {
    DEBUG_PRINT("%d part%s of size %d + 1 part of size %d", 
                nparts-1, (nparts == 2 ? "" : "s"), part_size, this_part_size);
  }
  return true;
}                       


bool ExternalValue::updatePart(int id, int part, char * val, size_t len) {
  if(len == 0) 
    return true;
    
  Operation op(ext_plan);
  
  op.key_buffer = (char *) memory_pool_alloc(pool, op.requiredKeyBuffer());
  op.buffer = (char *)     memory_pool_alloc(pool, op.requiredBuffer());

  op.clearKeyNullBits();
  op.setKeyPartInt(COL_STORE_KEY + 0, id);
  op.setKeyPartInt(COL_STORE_KEY + 1, part);
  
  op.setColumnInt(COL_STORE_KEY + 0, id);
  op.setColumnInt(COL_STORE_KEY + 1, part);
  op.setColumn(COL_STORE_VALUE, val, len);
  
  return op.updateTuple(tx);
}


void ExternalValue::setMiscColumns(Operation & op) const {
  /* Set the CAS value in the header row */
  if(do_server_cas) 
    op.setColumnBigUnsigned(COL_STORE_CAS, * wqitem->cas);  
  
  /* Set expire time */
  rel_time_t exptime = hash_item_get_exptime(wqitem->cache_item);
  if(exptime && wqitem->prefix_info.has_expire_col) {
    time_t abs_expires = 
      wqitem->pipeline->engine->server.core->abstime(exptime);
    op.setColumnInt(COL_STORE_EXPIRES, abs_expires); 
  }
  
  /* Set flags */
  if(wqitem->prefix_info.has_flags_col) {
    uint32_t flags = hash_item_get_flags(wqitem->cache_item);
    op.setColumnInt(COL_STORE_FLAGS, ntohl(flags));
  }
}


void ExternalValue::setValueColumns(Operation & op) const {
  const char *dbkey = workitem_get_key_suffix(wqitem);
  op.setKeyFieldsInRow(wqitem->plan->spec->nkeycols, dbkey, wqitem->base.nsuffix);
  
  if(shouldExternalize(new_hdr.length)) {
    /* Long value */
    DEBUG_PRINT_DETAIL(" [long]");
    op.setColumnNull(COL_STORE_VALUE);
    op.setColumnInt(COL_STORE_EXT_ID, new_hdr.id);
    op.setColumnInt(COL_STORE_EXT_SIZE, new_hdr.length);
  }
  else {
    /* Short value */
    DEBUG_PRINT_DETAIL(" [short]");
    op.setColumn(COL_STORE_VALUE, value, new_hdr.length);
    op.setColumnNull(COL_STORE_EXT_SIZE);
  }
}


bool ExternalValue::startTransaction(Operation &op) {
  if(! tx) {
    tx = op.startTransaction(wqitem->ndb_instance->db);
  }
  if(! tx) {
    log_ndb_error(wqitem->ndb_instance->db->getNdbError());
  }
  return (bool) tx;
}
  

bool ExternalValue::insert() {
  DEBUG_ENTER_DETAIL();

  /* Set the id, length, and parts count */
  new_hdr.setLength(wqitem->cache_item->nbytes);
  if(shouldExternalize(new_hdr.length)) {
    new_hdr.id = ext_plan->getAutoIncrement();
  }
  value = hash_item_get_data(wqitem->cache_item);
  
  /* Get an Operation */
  Operation op(wqitem);
  
  /* Set the key */
  if(! setupKey(wqitem, op))
    return false;
  
  if(! startTransaction(op))
    return false;
  
  /* Allocate the row buffer */
  workitem_allocate_rowbuffer_2(wqitem, op.requiredBuffer());
  op.buffer = wqitem->row_buffer_2;
  op.setNullBits();

  /* Generate a new CAS */
  worker_set_cas(wqitem->pipeline, wqitem->cas);        // generate a new value
  hash_item_set_cas(wqitem->cache_item, * wqitem->cas); // store it
  
  /* Store the row */
  setMiscColumns(op);
  setValueColumns(op);
  
  /* Insert Row */
  op.insertTuple(tx); 

  /* Insert parts */
  if(shouldExternalize(new_hdr.length))
    insertParts(value, new_hdr.length, new_hdr.nparts, 0);

  return true;
}


/* Take the existing short inline value and affix the new value to it */
void ExternalValue::affix_short(int current_len, char * current_val) {
  DEBUG_ENTER_DETAIL();

  const char * affix_val = hash_item_get_data(wqitem->cache_item);
  const size_t affix_len = wqitem->cache_item->nbytes;
  const size_t len = current_len + affix_len;
  
  if(shouldExternalize(len) && (old_hdr.id == 0))
    new_hdr.id = ext_plan->getAutoIncrement();
  else 
    new_hdr.id = old_hdr.id;
  new_hdr.setLength(len);

  value = (char *) memory_pool_alloc(pool, new_hdr.length);

  /* Rewrite the value */
  if(wqitem->base.verb == OPERATION_APPEND) {
    memcpy(value, current_val, current_len);
    memcpy(value + current_len, affix_val, affix_len);
  }
  else {
    assert(wqitem->base.verb == OPERATION_PREPEND);
    memcpy(value, affix_val, affix_len);
    memcpy(value + affix_len, current_val, current_len);
  }
  * (value + new_hdr.length) = 0;
  
  Operation op(wqitem);
  workitem_allocate_rowbuffer_2(wqitem, op.requiredBuffer());
  op.buffer = wqitem->row_buffer_2;
  setMiscColumns(op);
  setValueColumns(op);
  op.updateTuple(tx);

  if(shouldExternalize(len)) 
    insertParts(value, new_hdr.length, new_hdr.nparts, 0);
  
  finalize_write();
}


void ExternalValue::prepend() {
  DEBUG_ENTER_DETAIL();
  assert(wqitem->base.verb == OPERATION_PREPEND);
  /* So far: we have read the header into old_hdr via wqitem->row_buffer_1 
     and read the parts into this->value.  Now rewrite the value. */
  
  const char * affix_val = hash_item_get_data(wqitem->cache_item);
  const size_t affix_len = wqitem->cache_item->nbytes;
  
  new_hdr.id = old_hdr.id;
  new_hdr.setLength(old_hdr.length + affix_len);

  char * new_value = (char *) memory_pool_alloc(pool, new_hdr.length);
  memcpy(new_value, affix_val, affix_len);
  readLongValueIntoBuffer(new_value + affix_len);

  /* It's OK to overwrite the old pointer; readParts() allocated it from a pool
     and the pool still knows to free it */
  value = new_value;
  
  update();

  finalize_write();
}


void ExternalValue::append() {
  const size_t part_size = old_hdr.part_size;
  uint32_t & affix_len = wqitem->cache_item->nbytes;
  char * affix_val = hash_item_get_data(wqitem->cache_item);

  new_hdr.id = old_hdr.id;
  new_hdr.setLength(old_hdr.length + affix_len);
  int nparts = new_hdr.nparts - old_hdr.nparts;    

  if(old_hdr.length % old_hdr.part_size == 0) {
    /* Old value ended on part boundary; just add new parts */
    insertParts(affix_val, affix_len, nparts, old_hdr.nparts);
    DEBUG_PRINT(" Update optimized away.  %d new parts", nparts);
  }
  else {
    /* Update final part, and insert any needed new parts. */    
    /* readFinalPart() has read the last part into row_buffer_2 */
    char * read_val = 0;
    size_t read_len = 0;
    Operation readop(ext_plan, OP_READ);
    readop.buffer = wqitem->row_buffer_2;
    readop.getStringValueNoCopy(COL_STORE_VALUE, & read_val, & read_len);

    /* There is still room in that buffer to hold the rest of a part */
    size_t buf_space = part_size - read_len;
    size_t update_len = affix_len < buf_space ? affix_len : buf_space;
    memcpy(read_val + read_len, affix_val, update_len);

    updatePart(old_hdr.id, old_hdr.nparts - 1, read_val, read_len + update_len);
    
    if(affix_len > update_len) 
      insertParts(affix_val + update_len, affix_len - update_len,
                  nparts, old_hdr.nparts);
    DEBUG_PRINT(" %d byte part update + %d new parts", update_len, nparts);
  }
  
  /* Write the new header.  The key is already set from previous header read. */
  Operation hdr_op(wqitem);
  hdr_op.buffer = (char *) memory_pool_alloc(pool, hdr_op.requiredBuffer());
  hdr_op.setNullBits();
  setMiscColumns(hdr_op);
  setValueColumns(hdr_op);
  hdr_op.updateTuple(tx);
  
  wqitem->next_step = (void *) finalize_append;
  Scheduler::execute(tx, NdbTransaction::Commit, 
                     callback_main, wqitem, RESCHEDULE);
}


void ExternalValue::warnMissingParts() const {
  logger->log(LOG_WARNING, 0, 
              "Expected parts in external long value table but did not find them.\n"
              " -- Table %s, ext_id %d.\n"
              " -- Memcache Key: %.*s\n", 
              ext_plan->spec->table_name, old_hdr.id, 
              wqitem->base.nkey, wqitem->key);
}


int ExternalValue::readLongValueIntoBuffer(char * buf) const {
  int row_size = pad8(ext_plan->val_record->rec_size);
  int ncopied = 0;

  /* Copy all of the parts */
  for(int i = 0 ; i < old_hdr.nparts; i++) {
    Operation op(ext_plan, value + (row_size * i));
    ncopied += op.copyValue(COL_STORE_VALUE, buf + ncopied);
  }
  return ncopied;
}


void ExternalValue::build_hash_item() const {
  struct default_engine * se =  (struct default_engine *) 
    wqitem->pipeline->engine->m_default_engine;
  
  /* item_alloc(engine, key, nkey, flags, exptime, nbytes, cookie) */
  hash_item * item = item_alloc(se, wqitem->key, wqitem->base.nkey, 
                                wqitem->math_flags, 
                                expire_time.local_cache_expire_time,
                                old_hdr.length + 3, wqitem->cookie);
  
  if(item) {
    /* Now populate the item with the result */
    memcpy(hash_item_get_key(item), wqitem->key, wqitem->base.nkey); // the key
    
    char * data_ptr = hash_item_get_data(item);
    size_t ncopied = readLongValueIntoBuffer(data_ptr);

    /* Append \r\n\0 */
    * (data_ptr + ncopied)     = '\r';
    * (data_ptr + ncopied + 1) = '\n';
    * (data_ptr + ncopied + 2) = '\0';
    
    /* Point to it in the workitem */
    wqitem->cache_item = item;
    wqitem->value_size = ncopied;
    
    /* store it in the local cache */
    if(wqitem->prefix_info.do_mc_read) {
      ENGINE_ERROR_CODE status;
      status = store_item(se, item, wqitem->cas, OPERATION_SET, wqitem->cookie);
      if(status != ENGINE_SUCCESS)
        wqitem->status = & status_block_memcache_error;
    }
  }
  else {
    DEBUG_PRINT("Failed to allocate hash_item");
    wqitem->status = & status_block_memcache_error;
  }
}


/*************************************************************************/
/* Callbacks and worker steps */

void delete_after_header_read(NdbTransaction *tx, workitem *wqitem) {
  DEBUG_PRINT_DETAIL(" %d.%d", wqitem->pipeline->id, wqitem->id);
  
  Operation op(wqitem->plan, OP_READ);
  op.key_buffer = wqitem->ndb_key_buffer;  // The key is already set.
  op.buffer = wqitem->row_buffer_1;

  assert(wqitem->ext_val == 0);
  wqitem->ext_val = new ExternalValue(wqitem, tx);
  
  if(wqitem->ext_val->old_hdr.readFromHeader(op)) {
    wqitem->ext_val->deleteParts();
  }
  op.deleteTuple(tx);
  
  Scheduler::execute(tx, NdbTransaction::Commit, 
                     callback_main, wqitem, RESCHEDULE);
}


void callback_ext_parts_read(int, NdbTransaction *tx, void *itemptr) {
  workitem *wqitem = (workitem *) itemptr;
  DEBUG_PRINT_DETAIL(" %d.%d", wqitem->pipeline->id, wqitem->id);
  assert(wqitem->ext_val);
  
  if(tx->getNdbError().classification == NdbError::NoError) {
    switch(wqitem->base.verb) {
      case OP_READ:
        wqitem->ext_val->build_hash_item();
        worker_close(tx, wqitem);
        return;
      case OPERATION_APPEND:
        wqitem->ext_val->append();
        return;
      case OPERATION_PREPEND:
        wqitem->ext_val->prepend();
        return;
      default:
        assert(0);
    }
  }
  else if(tx->getNdbError().classification == NdbError::NoDataFound) {
    wqitem->ext_val->warnMissingParts();
  }
  else {
    log_ndb_error(tx->getNdbError());
  }

  wqitem->status = & status_block_misc_error;
  worker_commit(tx, wqitem);
}
 
 
/* callback_ext_write() is a callback after a header read on a write or update
   operation (memcache SET, REPLACE, or CAS).  If the header row was not found,
   treat the operation as an insert; if the header row was found, treat it as
   an update. 
*/
void callback_ext_write(int result,  NdbTransaction *tx, void *itemptr) {
  workitem * wqitem = (workitem *) itemptr;
  DEBUG_PRINT_DETAIL(" %d.%d", wqitem->pipeline->id, wqitem->id);

  assert(wqitem->ext_val == 0);
  wqitem->ext_val = new ExternalValue(wqitem, tx);

  if(tx->getNdbError().classification == NdbError::NoError) {
    wqitem->ext_val->update_after_header_read();
    return;
  }
  else if(tx->getNdbError().classification == NdbError::NoDataFound &&
          wqitem->base.verb != OPERATION_REPLACE) {
    wqitem->ext_val->insert_after_header_read();
    return;
  }
  
  callback_main(result, tx, itemptr);  /* Done */
}


void finalize_append(NdbTransaction *tx, workitem *wqitem) {
  /* After appending to an item, expire it from the local cache. */
  if(wqitem->prefix_info.do_mc_write || wqitem->prefix_info.do_mc_read) {
    struct default_engine * def_eng = (struct default_engine *)
      wqitem->pipeline->engine->m_default_engine;
    const char *dbkey = workitem_get_key_suffix(wqitem);
    hash_item *it = item_get(def_eng, dbkey, wqitem->base.nsuffix);  
    if(it) {
      item_unlink(def_eng, it);
      item_release(def_eng, it);
    }
  }
  
  worker_close(tx, wqitem);
}

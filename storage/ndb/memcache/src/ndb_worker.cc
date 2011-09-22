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
/* System headers */
#define __STDC_FORMAT_MACROS 
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <inttypes.h>
#include <ctype.h>

/* Memcache headers */
#include "memcached/types.h"
#include <memcached/extension_loggers.h>

/* NDB headers */
#include "NdbApi.hpp"

/* NDB Memcache headers */
#include "atomics.h"
#include "ndbmemcache_global.h"
#include "hash_item_util.h"
#include "workitem.h"
#include "Configuration.h"
#include "DataTypeHandler.h"
#include "TabSeparatedValues.h"
#include "debug.h"
#include "Operation.h"
#include "NdbInstance.h"
#include "status_block.h"
#include "Operation.h"
#include "Scheduler.h"
#include "ndb_engine.h"
#include "hash_item_util.h"
#include "ndb_worker.h"

typedef void ndb_async_callback(int, NdbTransaction *, void *);

ndb_async_callback incr_callback;     // callback for incr/decr
ndb_async_callback rewrite_callback;  // callback for append/prepend
ndb_async_callback DB_callback;       // callback for all others
 
op_status_t worker_do_read(workitem *, bool with_cas); 
op_status_t worker_do_write(workitem *, bool with_cas); 
op_status_t worker_do_delete(workitem *, bool with_cas); 
op_status_t worker_do_math(workitem *wqitem, bool with_cas);

void worker_set_cas(ndb_pipeline *, uint64_t *);
bool finalize_read(workitem *);
bool finalize_write(workitem *, bool);
int build_cas_routine(NdbInterpretedCode *r, int cas_col, uint64_t cas_val);
bool scan_delete(NdbInstance *, QueryPlan *);


extern EXTENSION_LOGGER_DESCRIPTOR *logger;

/* Prototype for ndb_allocate() from ndb_engine.c: */
ENGINE_ERROR_CODE ndb_allocate(ENGINE_HANDLE* handle,
                               const void* cookie,
                               item **item,
                               const void* key,
                               const size_t nkey,
                               const size_t nbytes,
                               const int flags,
                               const rel_time_t exptime);

status_block status_block_generic_success = 
  { ENGINE_SUCCESS , "Transaction succeeded"          };

status_block status_block_item_not_found =  
  { ENGINE_KEY_ENOENT,  "Item Not Found"              };

status_block status_block_misc_error = 
  { ENGINE_FAILED, "Transaction failed"               };

status_block status_block_memcache_error = 
  { ENGINE_FAILED, "Cache level error"                };

status_block status_block_cas_mismatch = 
  { ENGINE_KEY_EEXISTS, "CAS mismatch"                };

status_block status_block_bad_add = 
  { ENGINE_NOT_STORED, "Duplicate key on insert"      };

status_block status_block_bad_replace =
  { ENGINE_NOT_STORED, "Tuple not found"              };
  

void worker_set_cas(ndb_pipeline *p, uint64_t *cas) {  
  /* Be careful here --  ndbmc_atomic32_t might be a signed type.
     Shitfting of signed types behaves differently. */
  bool did_inc;
  uint32_t cas_lo;
  uint32_t & cas_hi = p->engine->cas_hi;
  do {
    cas_lo = p->engine->cas_lo;    
    did_inc = atomic_cmp_swap_int(& p->engine->cas_lo, cas_lo, cas_lo + 1);
  } while(! did_inc);
  *cas = uint64_t(cas_lo) | (uint64_t(cas_hi) << 32);
  DEBUG_PRINT("hi:%lx lo:%lx cas:%llx (%llu)", cas_hi, cas_lo, *cas, *cas);
}
  
/* worker_prepare_operation(): 
   Called from the scheduler. 
   Returns true if executeAsynchPrepare() has been called on the item.
*/
op_status_t worker_prepare_operation(workitem *newitem) {
  bool server_cas = (newitem->prefix_info.has_cas_col && newitem->cas);
  op_status_t r;

  /* Jump table */
  switch(newitem->base.verb) {
    case OP_READ:
      r = worker_do_read(newitem, server_cas);
      break;
      
    case OPERATION_APPEND:
    case OPERATION_PREPEND:
      if(newitem->plan->spec->nvaluecols > 1) {
        /* APPEND/PREPEND is currently not supported for tsv */
        r = op_not_supported;
      }
      else {
        r = worker_do_read(newitem, server_cas);
      }
      break;

    case OP_DELETE: 
      r = worker_do_delete(newitem, server_cas);
      break;

    case OPERATION_SET:
    case OPERATION_ADD:
    case OPERATION_REPLACE:
    case OPERATION_CAS:
      r = worker_do_write(newitem, server_cas);
      break;

    case OP_ARITHMETIC:
      r = worker_do_math(newitem, server_cas);
      break;
      
    default:
      r= op_not_supported;
  }

  return r;
}


op_status_t worker_do_delete(workitem *wqitem, bool server_cas) {  
  DEBUG_ENTER();
  
  QueryPlan *plan = wqitem->plan;
  const char *dbkey = workitem_get_key_suffix(wqitem);
  Operation op(plan, OP_DELETE, wqitem->ndb_key_buffer);
  const NdbOperation *ndb_op = 0;

  NdbTransaction *tx = op.startTransaction();
  DEBUG_ASSERT(tx);
  
  op.clearKeyNullBits();
  op.setKeyPart(COL_STORE_KEY, dbkey, wqitem->base.nsuffix);
  
  if(server_cas && * wqitem->cas) {
    // ndb_op = op.deleteTupleCAS(tx, & options);  
  }
  else {
    ndb_op = op.deleteTuple(tx);    
  }

  /* Check for errors */
  if(ndb_op == 0) {
    const NdbError & err = tx->getNdbError();
    if(err.status != NdbError::Success) {
      logger->log(LOG_WARNING, 0, "deleteTuple(): %s\n", err.message);
      tx->close();
      return op_failed;
    }
  }

  /* Prepare for execution */   
  tx->executeAsynchPrepare(NdbTransaction::Commit, DB_callback, (void *) wqitem);
  return op_async_prepared;
}


op_status_t worker_do_write(workitem *wqitem, bool server_cas) {
  DEBUG_PRINT("%s", workitem_get_operation(wqitem));

  uint64_t cas_in = *wqitem->cas;                       // read old value
  worker_set_cas(wqitem->pipeline, wqitem->cas);        // generate a new value
  hash_item_set_cas(wqitem->cache_item, * wqitem->cas); // store it

  const NdbOperation *ndb_op = 0;
  QueryPlan *plan = wqitem->plan;
  Operation op(plan, wqitem->base.verb, wqitem->ndb_key_buffer);
  const char *dbkey = workitem_get_key_suffix(wqitem);
  bool op_ok;

  /* Set the key */
  op.clearKeyNullBits();
  op_ok = op.setKeyPart(COL_STORE_KEY, dbkey, wqitem->base.nsuffix);
  if(! op_ok) return op_overflow;

  /* Allocate and encode the buffer for the row */ 
  workitem_allocate_rowbuffer_1(wqitem, op.requiredBuffer());
  op.buffer = wqitem->row_buffer_1;

  /* Set the row */
  op.clearNullBits();  // no need to re-test for overflow...
  op.setColumn(COL_STORE_KEY, dbkey, wqitem->base.nsuffix);
  
  if(plan->spec->nvaluecols > 1) {
    /* Multiple Value Columns */
    TabSeparatedValues tsv(hash_item_get_data(wqitem->cache_item), 
                           plan->spec->nvaluecols, wqitem->cache_item->nbytes); 
    int idx = 0;
    do {
      if(tsv.getLength()) {
        op_ok = op.setColumn(COL_STORE_VALUE+idx, tsv.getPointer(), tsv.getLength());
        if(! op_ok) return op_overflow;
      }
      else {
        op.setColumnNull(COL_STORE_VALUE+idx);
      }
      idx++;
    } while (tsv.advance());
  }
  else {
    /* Just one value column */
    op_ok = op.setColumn(COL_STORE_VALUE, hash_item_get_data(wqitem->cache_item),
                         wqitem->cache_item->nbytes);
    if(! op_ok) return op_overflow;
  }

  if(server_cas) {
    op.setColumnBigUnsigned(COL_STORE_CAS, * wqitem->cas);   // the cas
  }

  if(wqitem->plan->dup_numbers) {
    if(isdigit(* hash_item_get_data(wqitem->cache_item)) && 
       wqitem->cache_item->nbytes < 32) {      // Copy string representation 
      uint64_t number;
      const int len = wqitem->cache_item->nbytes;
      char value[32];
      for(int i = 0 ; i  < len ; i++) 
        value[i] = * (hash_item_get_data(wqitem->cache_item) + i); 
      value[len] = 0;
      if(safe_strtoull(value, &number)) { // numeric: set the math column
        DEBUG_PRINT(" dup_numbers -- %d", (int) number );
        op.setColumnBigUnsigned(COL_STORE_MATH, number);
      }
      else {  // non-numeric
        DEBUG_PRINT(" dup_numbers but non-numeric: %.*s *** ", len, value);
        op.setColumnNull(COL_STORE_MATH);
      }
    }
    else op.setColumnNull(COL_STORE_MATH);      
  }

  /* Start the transaction */
  NdbTransaction *tx = op.startTransaction();
  if(! tx) {
    logger->log(LOG_WARNING, 0, "tx: %s \n", plan->db->getNdbError().message);
    DEBUG_ASSERT(false);
  }
  
  if(wqitem->base.verb == OPERATION_REPLACE) {
    DEBUG_PRINT(" [REPLACE] \"%.*s\"", wqitem->base.nkey, wqitem->key);
    ndb_op = op.updateTuple(tx);
  }
  else if(wqitem->base.verb == OPERATION_ADD) {
    DEBUG_PRINT(" [ADD]     \"%.*s\"", wqitem->base.nkey, wqitem->key);
    ndb_op = op.insertTuple(tx);
  }
  else if(wqitem->base.verb == OPERATION_CAS) {    
    if(server_cas) {
      /* NdbOperation.hpp says: "All data is copied out of the OperationOptions 
         structure (and any subtended structures) at operation definition time."      
      */
      DEBUG_PRINT(" [CAS UPDATE:%llu]     \"%.*s\"", cas_in, wqitem->base.nkey, wqitem->key);
      const Uint32 program_size = 25;
      Uint32 program[program_size];
      NdbInterpretedCode cas_code(plan->table, program, program_size);
      NdbOperation::OperationOptions options;
      build_cas_routine(& cas_code, plan->cas_column_id, cas_in);
      options.optionsPresent = NdbOperation::OperationOptions::OO_INTERPRETED;
      options.interpretedCode = & cas_code;
      ndb_op = op.updateInterpreted(tx, & options);
    }
  }
  else if(wqitem->base.verb == OPERATION_SET) {
    DEBUG_PRINT(" [SET]     \"%.*s\"", wqitem->base.nkey, wqitem->key);
    ndb_op = op.writeTuple(tx);
  }

  /* Error case; operation has not been built */
  if(! ndb_op) {
    logger->log(LOG_WARNING, 0, "error building NDB operation: %s\n", 
                tx->getNdbError().message);
    DEBUG_PRINT("NDB operation failed.  workitem %d.%d", wqitem->pipeline->id,
                wqitem->id);
    tx->close();
    return op_failed;
  }

  tx->executeAsynchPrepare(NdbTransaction::Commit, DB_callback, (void *) wqitem);
  return op_async_prepared;
}


op_status_t worker_do_read(workitem *wqitem, bool server_cas) {
  DEBUG_ENTER();

  QueryPlan *plan = wqitem->plan;
  const char *dbkey = workitem_get_key_suffix(wqitem);

  /* Use the workitem's inline buffer as a key buffer; 
     allocate a new result buffer large enough for the result */
  Operation op(plan, OP_READ, wqitem->ndb_key_buffer);
  workitem_allocate_rowbuffer_1(wqitem, op.requiredBuffer());  
  op.buffer = wqitem->row_buffer_1;

  /* Copy the key into the key buffer, ecnoding it for NDB */
  op.clearKeyNullBits();
  op.setKeyPart(COL_STORE_KEY, dbkey, wqitem->base.nsuffix);  

  /* Start a transaction, and call NdbTransaction::readTuple() */
  NdbTransaction *tx = op.startTransaction();
  DEBUG_ASSERT(tx);

  if(! op.readTuple(tx)) {
    logger->log(LOG_WARNING, 0, "readTuple(): %s\n", tx->getNdbError().message);
    tx->close();
    return op_failed;
  }

  /* Save the workitem in the transaction and prepare for async execution */   
  if(wqitem->base.verb == OPERATION_APPEND || wqitem->base.verb == OPERATION_PREPEND) 
  {
    DEBUG_PRINT("In read() portion of APPEND.  Value = %s", 
                hash_item_get_data(wqitem->cache_item));
    tx->executeAsynchPrepare(NdbTransaction::NoCommit, rewrite_callback, (void *) wqitem);
  }
  else 
  {
    tx->executeAsynchPrepare(NdbTransaction::Commit, DB_callback, (void *) wqitem);
  }

  return op_async_prepared;
}


op_status_t worker_do_math(workitem *wqitem, bool server_cas) {
  DEBUG_PRINT("create: %d   retries: %d", 
                     wqitem->base.math_create, wqitem->base.retries);
  worker_set_cas(wqitem->pipeline, wqitem->cas);

  /*
   Begin transaction
     1. readTuple  (LM_Exclusive)
     2. if(create_flag) 
          insertTuple, setting value to initial_value - delta (AO_IgnoreError)
     3. updateTuple (interpreted: add delta to value)
   Execute (Commit)
   
   Then look at the error codes from all 3 operations to see what happened:
   
   read  insert  update  response
   --------------------------------------------------------------------------
   626   0       0       row was created.  return initial_value.
   0     630     0       row existed.  return fetched_value + delta.
   x     x       626     failure due to race with concurrent delete
  */

  QueryPlan *plan = wqitem->plan;
  const char *dbkey = workitem_get_key_suffix(wqitem);

  /* This transaction involves up to three NdbOperations. */
  const NdbOperation *ndbop1 = 0;
  const NdbOperation *ndbop2 = 0;
  const NdbOperation *ndbop3 = 0;
  
  /* "Operation" is really just a header-only library for convenience and 
     safety.  We use 2 of them here -- one for the read, the other for the
     update and insert.  This is only because they will make slightly different
     use of records and buffers.   Both will use the inline key buffer.
  */
  Operation op1(plan, OP_READ,       wqitem->ndb_key_buffer);
  Operation op2(plan, OPERATION_ADD, wqitem->ndb_key_buffer);

  if(! wqitem->base.retries) {
    /* Allocate & populate row buffers for these operations: 
       We need one for the read and one for the insert.  */
    size_t needed = op1.requiredBuffer();
    workitem_allocate_rowbuffer_1(wqitem, needed);
    workitem_allocate_rowbuffer_2(wqitem, needed);    
    op1.buffer = wqitem->row_buffer_1;
    op2.buffer = wqitem->row_buffer_2;

    /* The two items share a key buffer, so we encode the key just once */
    op1.clearKeyNullBits();
    op1.setKeyPart(COL_STORE_KEY, dbkey, wqitem->base.nsuffix);  
    
    /* The insert operation also needs the key written into the row */
    op2.clearNullBits();
    op2.setColumn(COL_STORE_KEY, dbkey, wqitem->base.nsuffix);

    /* CAS */
    if(server_cas) op2.setColumnBigUnsigned(COL_STORE_CAS, * wqitem->cas);
    
    /* In "dup_numbers" mode, also null out the text version of the value */
    if(wqitem->plan->dup_numbers) {
      op2.setColumnNull(COL_STORE_VALUE);
    }
  } 
  
  /* Use an op (either one) to start the transaction */
  NdbTransaction *tx = op1.startTransaction();
  
  /* NdbOperation #1: READ */
  {
    ndbop1 = op1.readMasked(tx, plan->math_mask_r, NdbOperation::LM_Exclusive);
    if(! ndbop1) {
      logger->log(LOG_WARNING, 0, "readMasked(): %s\n", tx->getNdbError().message);
      tx->close();
      return op_failed; 
    }
  }

  /* NdbOperation #2: INSERT (AO_IgnoreError) */
  if(wqitem->base.math_create) {
    /* Offset the initial value to compensate for the update */
    uint64_t initial_value;
    if(wqitem->base.math_incr)
      initial_value = wqitem->math_value - wqitem->math_delta;  // incr
    else
      initial_value = wqitem->math_value + wqitem->math_delta;  // decr
    op2.setColumnBigUnsigned(COL_STORE_MATH, initial_value);

    /* If this insert gets an error, the transaction should continue. */
    NdbOperation::OperationOptions options;
    options.optionsPresent = NdbOperation::OperationOptions::OO_ABORTOPTION;  
    options.abortOption = NdbOperation::AO_IgnoreError;

    ndbop2 = op2.insertMasked(tx, plan->math_mask_i, & options); 
    if(! ndbop2) {
      logger->log(LOG_WARNING, 0, "insertMasked(): %s\n", tx->getNdbError().message);
      tx->close();
      return op_failed;
    }
  }

  /* NdbOperation #3: Interpreted Update */
  {
    NdbOperation::OperationOptions options;
    const Uint32 program_size = 32;
    Uint32 program[program_size];
    NdbInterpretedCode code(plan->table, program, program_size);

    if(wqitem->base.math_incr) {                                  // incr
      code.add_val(plan->math_column_id, wqitem->math_delta);   
      code.interpret_exit_ok();
    }
    else {                                                        // decr
      const Uint32 Rdel = 1, Rcol = 2, Rres = 3;       // registers 1,2,3
      const Uint32 SUB_ZERO = 0;                       // a label

      code.load_const_u64(Rdel, wqitem->math_delta);   // load R1, delta
      code.read_attr     (Rcol, plan->math_column_id); // load R2, math_col
      code.branch_gt     (Rdel, Rcol, SUB_ZERO);       // if R1 > R2 goto SUB_ZERO
      code.sub_reg       (Rres, Rcol, Rdel);           // let R3 = R2 - R1
      code.write_attr    (plan->math_column_id, Rres); // Store into column
      code.interpret_exit_ok();
      code.def_label     (SUB_ZERO);
      code.load_const_u64(Rres, 0);                    // Set to zero
      code.write_attr    (plan->math_column_id, Rres); // Store into column 
      code.interpret_exit_ok();
    }

    code.finalise();
 
    options.optionsPresent = NdbOperation::OperationOptions::OO_INTERPRETED;
    options.interpretedCode = & code;
   
    ndbop3 = op2.updateInterpreted(tx, & options, plan->math_mask_u);
    if(! ndbop3) {
      logger->log(LOG_WARNING, 0, "updateInterpreted(): %s\n", tx->getNdbError().message);
      tx->close();
      return op_failed;
    }
  }

  tx->executeAsynchPrepare(NdbTransaction::Commit, incr_callback, (void *) wqitem);
  return op_async_prepared;
}


void DB_callback(int result, NdbTransaction *tx, void *itemptr) {
  workitem *wqitem = (workitem *) itemptr;
  ndb_pipeline * & pipeline = wqitem->pipeline;
  status_block * return_status;
  bool tx_did_match = false;
    
  /************** Error handling ***********/  
  /* No Error */
  if(tx->getNdbError().classification == NdbError::NoError) {
    tx_did_match = true;
    DEBUG_PRINT("Success.");
    return_status = & status_block_generic_success;
  }
  /* CAS mismatch; interpreted code aborted with interpret_exit_nok() */
  else if(tx->getNdbError().code == 2010) {
    DEBUG_PRINT("CAS mismatch.");
    * wqitem->cas = 0ULL;  // set cas=0 in the response
    return_status = & status_block_cas_mismatch;    
  }
  /* No Data Found */
  else if(tx->getNdbError().classification == NdbError::NoDataFound) {
    /* Error code should be 626 */
    DEBUG_PRINT("NoDataFound [%d].", tx->getNdbError().code);
    if(wqitem->cas) * wqitem->cas = 0ULL;
    return_status = 
      (wqitem->base.verb == OPERATION_REPLACE ?
        & status_block_bad_replace :  & status_block_item_not_found);
  }  
  /* Duplicate key on insert */
  else if(tx->getNdbError().code == 630) {
    DEBUG_PRINT("Duplicate key on insert.");
    if(wqitem->cas) * wqitem->cas = 0ULL;
    return_status = & status_block_bad_add;    
  }
  /* Some other error */
  else  {
    DEBUG_PRINT("[%d]: %s", 
                       tx->getNdbError().code, tx->getNdbError().message);
    return_status = & status_block_misc_error;
  }
  
  switch(wqitem->base.verb) {
    case OP_READ:
      if(tx_did_match) 
        if(finalize_read(wqitem) == false)
          return_status = & status_block_memcache_error;
      break;
    case OPERATION_SET:
    case OPERATION_ADD:
    case OPERATION_REPLACE:
    case OPERATION_CAS:
    case OPERATION_APPEND:
    case OPERATION_PREPEND:
      finalize_write(wqitem, tx_did_match);
      break;
    case OP_DELETE:
      break;
    default:
      assert("How did we get here?" == 0);
  }
  
  tx->close();
  
  // If this was a synchronous call, the server is waiting for us 
  if(wqitem->base.is_sync) {
    wqitem->status = return_status;
    pipeline->engine->server.cookie->store_engine_specific(wqitem->cookie, wqitem); 
    pipeline->scheduler->yield(wqitem);
  }
  else {
    /* The workitem was allocated back in the engine thread; if used in a
       callback, it would be freed there, too.  But we must free it here.
    */
    pipeline->engine->server.cookie->store_engine_specific(wqitem->cookie, wqitem->previous);
    pipeline->scheduler->io_completed(wqitem);
    workitem_free(wqitem);
  }
}


/* Middle-step callback for APPEND and PREPEND */
void rewrite_callback(int result, NdbTransaction *tx, void *itemptr) {
  workitem *item = (workitem *) itemptr;
  DEBUG_PRINT("%d.%d", item->pipeline->id, item->id);
 
  /* Check the transaction status */
  if(tx->getNdbError().classification == NdbError::NoDataFound) {
    item->status = & status_block_bad_replace;
    tx->close();
    item->pipeline->engine->server.cookie->store_engine_specific(item->cookie, item); 
    item->pipeline->scheduler->yield(item);
    return;
  }
  else if(tx->getNdbError().classification != NdbError::NoError) {
    return DB_callback(result, tx, itemptr);
  }  

  /* Strings and lengths: */
  char * current_val = 0; 
  size_t current_len = 0;
  const char * affix_val = hash_item_get_data(item->cache_item);
  const size_t affix_len = item->cache_item->nbytes;
  
  /* worker_do_read() has already written the key into item->ndb_key_buffer. 
     The result is sitting in wqitem->row_buffer_1. 
     Read the value.
  */  
  Operation readop(item->plan, OP_READ);
  readop.buffer = item->row_buffer_1;
  assert(readop.nValues() == 1);
  readop.getStringValueNoCopy(COL_STORE_VALUE + 0, & current_val, & current_len);
    
  /* Generate a new CAS */
  worker_set_cas(item->pipeline, item->cas);  
  hash_item_set_cas(item->cache_item, * item->cas);

  /* Prepare a write operation */
  Operation op(item->plan, item->base.verb, item->ndb_key_buffer);
  const NdbOperation *ndb_op = 0;  
  
  /* Allocate a buffer for the new value */ 
  size_t max_len = op.requiredBuffer();
  workitem_allocate_rowbuffer_2(item, max_len);
  op.buffer = item->row_buffer_2;

  /* Rewrite the value */
  size_t total_len = affix_len + current_len;
  if(total_len > max_len) total_len = max_len;
  if(item->base.verb == OPERATION_APPEND) {
    memcpy(current_val + current_len, affix_val, total_len - current_len);
  }
  else {
    assert(item->base.verb == OPERATION_PREPEND);
    memmove(current_val + affix_len, current_val, current_len);
    memcpy(current_val, affix_val, affix_len); 
  }
  * (current_val + total_len) = 0;
  DEBUG_PRINT("New value: %s", current_val);
  
  /* Set the row */
  op.clearNullBits();
  op.setColumn(COL_STORE_KEY, workitem_get_key_suffix(item), item->base.nsuffix);
  op.setColumn(COL_STORE_VALUE, current_val, total_len);
  if(item->prefix_info.has_cas_col) 
    op.setColumnBigUnsigned(COL_STORE_CAS, * item->cas);
  ndb_op = op.updateTuple(tx);

  if(ndb_op) {
    // Inform the scheduler that this item must be re-polled
    item->pipeline->scheduler->reschedule(item);
    tx->executeAsynchPrepare(NdbTransaction::Commit, DB_callback, (void *) item);
  }
  else {
    /* Error case; operation has not been built */
    DEBUG_PRINT("NDB operation failed.  workitem %d.%d", item->pipeline->id,
                item->id);
    tx->close();
    // pipeline->scheduler->close(item);
    workitem_free(item);
  }
}


/* Dedicated callback function for INCR and DECR operations
*/
void incr_callback(int result, NdbTransaction *tx, void *itemptr) {
  workitem *wqitem = (workitem *) itemptr;
  ndb_pipeline * & pipeline = wqitem->pipeline;
  status_block * return_status = 0;
  ENGINE_ERROR_CODE io_status = ENGINE_SUCCESS;

  /*  read  insert  update cr_flag response
      ------------------------------------------------------------------------
      626   0       0        0     return NOT_FOUND.
      626   0       0        1     row was created.  return initial_value.
      0     x       0        x     row existed.  return fetched_value + delta.
      x     x       626      x     failure due to race with concurrent delete.
  */

  const NdbOperation *ndbop1, *ndbop2, *ndbop3;
  int tx_result = tx->getNdbError().code;
  int r_read = -1;
  int r_insert = -1;
  int r_update = -1;

  ndbop1 = tx->getNextCompletedOperation(NULL);
  r_read = ndbop1->getNdbError().code;
  
  if(ndbop1) {  /* ndbop1 is the read operation */
    if(wqitem->base.math_create) {
      ndbop2 = tx->getNextCompletedOperation(ndbop1);  /* the insert */
      r_insert = ndbop2->getNdbError().code;
    }
    else {
      ndbop2 = ndbop1;  /* no insert (create flag was not set) */
      r_insert = 0;
    }
    if(ndbop2) {
      ndbop3 = tx->getNextCompletedOperation(ndbop2);  /* the update */
      r_update = ndbop3->getNdbError().code;
    }
  }
  DEBUG_PRINT("tx: %d   r_read: %d   r_insert: %d   r_update: %d   create: %d",
              tx_result, r_read, r_insert, r_update, wqitem->base.math_create);
  
  if(r_read == 626 && ! wqitem->base.math_create) {
    /* row did not exist, and create flag was not set */
    return_status = & status_block_item_not_found;
  }
  else if(r_read == 0 && r_update == 0) {
    /* row existed.  return fetched_value +/- delta. */
    Operation op(wqitem->plan, OP_READ);
    op.buffer = wqitem->row_buffer_1;
    uint64_t stored = op.getBigUnsignedValue(COL_STORE_MATH);
    if(wqitem->base.math_incr) {              // incr
      wqitem->math_value = stored + wqitem->math_delta;
    }
    else {                                    // decr
      if(wqitem->math_delta > stored)
        wqitem->math_value = 0; // underflow < 0 is not allowed
      else
        wqitem->math_value = stored - wqitem->math_delta;
    }
      
    return_status = & status_block_generic_success;
  }  
  else if(r_read == 626 && r_insert == 0 && r_update == 0) {
    /* row was created.   Return initial_value.
       wqitem->math_value is already set to the initial_value :)  */
    return_status = & status_block_generic_success;    
  }
  else if(r_read == -1 || r_insert == -1 || r_update == -1) {
    /* Total failure */
    logger->log(LOG_WARNING, 0, "incr/decr: total failure.\n");
    io_status = ENGINE_FAILED;  
  }
  else if(r_update == 626) {
    /*  failure due to race with concurrent delete */
    if(wqitem->base.retries++ < 3) {       // try again:
      tx->close();      
      (void) worker_do_math(wqitem, wqitem->prefix_info.has_cas_col); 
      return;       
    }
    else { 
      logger->log(LOG_WARNING, 0, "incr/decr: giving up, too many retries.\n");
      io_status = ENGINE_FAILED;
    }
  }
  
  tx->close();
  
  if(wqitem->base.is_sync) {
    wqitem->status = return_status;
    pipeline->engine->server.cookie->store_engine_specific(wqitem->cookie, wqitem); 
    pipeline->scheduler->yield(wqitem);    
  }
  else {
    /* The workitem was allocated back in the engine thread; if used in a
       callback, it would be freed there, too.  But we must free it here.  */
    pipeline->engine->server.cookie->store_engine_specific(wqitem->cookie, wqitem->previous);
    pipeline->scheduler->io_completed(wqitem);
    workitem_free(wqitem);
  }
}


bool finalize_read(workitem *wqitem) {
  DEBUG_ENTER();
  
  bool need_hash_item;
  Operation op(wqitem->plan, OP_READ);
  op.buffer = wqitem->row_buffer_1;
 
  if(wqitem->prefix_info.has_cas_col) { /* FIXME: little-endian-only */
    wqitem->cas = (uint64_t *) op.getPointer(COL_STORE_CAS);  
  }
      
  /* Try to send the value from the row_buffer without copying it. */
  if(    (! wqitem->prefix_info.do_mc_read)
     && op.nValues() == 1
     && ! (op.isNull(COL_STORE_VALUE) && wqitem->plan->dup_numbers)
     && op.getStringValueNoCopy(COL_STORE_VALUE, & wqitem->value_ptr, & wqitem->value_size)
     && op.appendCRLF(COL_STORE_VALUE, wqitem->value_size))
  {
    /* The workitem's value_ptr and value_size were set above. */
    DEBUG_PRINT("using no-copy buffer.");
    wqitem->base.has_value = true;
    need_hash_item = false;
  }
  else need_hash_item = true;
  
  if(need_hash_item) 
    return build_hash_item(wqitem, op);
  
  /* Using the row_buffer.
     "cache_item == workitem" is a sort of code here, required because memcached 
     expects us to return a non-zero item.  In ndb_release() we will look 
     for this and use it to prevent double-freeing of the workitem. 
   */
  wqitem->cache_item = (hash_item *) wqitem;  
  return true;
}


/*  Allocate a hash table item, populate it with the original key 
    and the results from the read, then store it.    
 */
bool build_hash_item(workitem *wqitem, Operation &op) {
  DEBUG_ENTER();
  ndb_pipeline * & pipeline = wqitem->pipeline;
  struct default_engine *se;
  size_t nbytes;
     
  se = (struct default_engine *) pipeline->engine->m_default_engine;      
  nbytes = op.getStringifiedLength() + 2;  /* 2 bytes for \r\n */
  
  /* Allocate a hash item */
  /* item_alloc(engine, key, nkey, flags, exptime, nbytes, cookie) */
  hash_item * item = item_alloc(se, wqitem->key, wqitem->base.nkey, 
                                0, 0, nbytes, wqitem->cookie);
  
  if(item) {
    /* Now populate the item with the result */
    size_t ncopied = 0;
    memcpy(hash_item_get_key(item), wqitem->key, wqitem->base.nkey); // the key
    char * data_ptr = hash_item_get_data(item);
    
    if(wqitem->plan->dup_numbers && op.isNull(COL_STORE_VALUE)
       && ! (op.isNull(COL_STORE_MATH))) {
      /* in dup_numbers mode, copy the math value */
      ncopied = op.copyValue(COL_STORE_MATH, data_ptr);
      ncopied-- ; // drop the trailing null
    }
    else {
      /* Build a result containing each column */
      for(int i = 0 ; i < op.nValues() ; i++) {
        if(i) * (data_ptr + ncopied++) = '\t';
        ncopied += op.copyValue(COL_STORE_VALUE + i, data_ptr + ncopied);
      }
    }

    /* pad the value with \r\n -- memcached expects it there. */
    * (data_ptr + ncopied)     = '\r';
    * (data_ptr + ncopied + 1) = '\n';
    * (data_ptr + ncopied + 2) = '\0';
    DEBUG_PRINT("nbytes: %d   ncopied: %d", nbytes, ncopied + 2);
    
    /* Point to it in the workitem */
    wqitem->cache_item = item;
    wqitem->value_size = ncopied;
    
    /* store it in the local cache? */
    // fixme: probably nbytes is wrong
    if(wqitem->prefix_info.do_mc_read) {
      return (store_item(se, item, wqitem->cas, OPERATION_SET, wqitem->cookie) 
              == ENGINE_SUCCESS);
    }
    return true;
  }
  DEBUG_PRINT("Failure.  Item: %p", item);
  return false;
}


bool finalize_write(workitem *wqitem, bool tx_did_match) {
  struct default_engine *se;
  ndb_pipeline * & pipeline = wqitem->pipeline;
  se = (struct default_engine *) pipeline->engine->m_default_engine;      

  /* If the write was succesful, update the local cache */
  /* Possible bugs here: 
     (1) store_item will store nbytes as length, which is wrong.
     (2) The CAS may be incorrect.
  */
  if(wqitem->prefix_info.do_mc_write && tx_did_match) {
    return (store_item(se, wqitem->cache_item, 
                       hash_item_get_cas_ptr(wqitem->cache_item),
                       OPERATION_SET, wqitem->cookie) == ENGINE_SUCCESS);
  }
  return true;
}


int build_cas_routine(NdbInterpretedCode *r, int cas_col, uint64_t cas_val) {
  const Uint32 R1 = 1;  // a register
  const Uint32 R2 = 2;  // a register
  const Uint32 MISMATCH = 0;  // a branch label
  
  DEBUG_PRINT("cas_col: %d,  cas_val: %llu", cas_col, cas_val);
  
  /* Branch on cas_value != cas_column */
  r->load_const_u64(R1, cas_val);            // load the CAS into R1
  r->read_attr(R2, cas_col);                 // read the cas column into R2
  r->branch_ne(R1, R2, MISMATCH);            // if(R1 != R2) goto MISMATCH
  
  /* Here is the cas_value == cas_column branch: */
  r->interpret_exit_ok();                    // allow operation to succeed
  
  /* Here is the cas_value != cas_column branch: */
  r->def_label(MISMATCH);
  r->interpret_exit_nok(2010);               // abort the operation
  
  return r->finalise();                      // resolve the label/branch
}


/* Flush all is a fully synchronous operation -- 
 the memcache server is waiting for a response, and the thread is blocked.
*/
ENGINE_ERROR_CODE ndb_flush_all(ndb_pipeline *pipeline) {
  DEBUG_ENTER();
  const Configuration &conf = get_Configuration();
  
  DEBUG_PRINT(" %d prefixes", conf.nprefixes);
  for(int i = 0 ; i < conf.nprefixes ; i++) {
    NdbInstance *inst = 0;
    const KeyPrefix *p = conf.getPrefix(i);
    if(p->info.use_ndb && p->info.do_db_flush) {
      ClusterConnectionPool *pool = conf.getConnectionPoolById(p->info.cluster_id);
      Ndb_cluster_connection *conn = pool->getMainConnection();
      inst = new NdbInstance(conn, conf.nprefixes, 128);
      QueryPlan *plan = inst->getPlanForPrefix(p);
      if(plan->keyIsPrimaryKey()) {
        /* To flush, scan the table and delete every row */
        DEBUG_PRINT("prefix %d - deleting from %s", i, p->table->table_name);
        scan_delete(inst, plan);      
      }
      else DEBUG_PRINT("prefix %d - not scanning table %s -- accees path "
                       "is not primary key", i, p->table->table_name);
    }
    else DEBUG_PRINT("prefix %d - not scanning table %s -- use_ndb:%d flush:%d",
                     i, p->table ? p->table->table_name : "",
                     p->info.use_ndb, p->info.do_db_flush);
    if(inst) delete inst;
  }
  
  return ENGINE_SUCCESS;
}


bool scan_delete(NdbInstance *inst, QueryPlan *plan) {
  DEBUG_ENTER();
  int check;
  bool rescan;
  int res = 0;
  const int max_batch_size = 1000;
  int batch_size = 1;
  int delTxRowCount = 0;
  int force_send = 1;
  struct {
    unsigned short scans;
    unsigned short errors;
    unsigned short rows;
    unsigned short commit_batches;
  } stats = {0, 0, 0, 0 };
  
  /* To securely scan a whole table, use an outer transaction only for the scan, 
     but take over each lock in an inner transaction (with a row count) that 
     deletes 1000 rows per transaction 
  */  
  do {
    stats.scans += 1;
    rescan = false;
    NdbTransaction *scanTx = inst->db->startTransaction();
    NdbTransaction *delTx = inst->db->startTransaction();
    NdbScanOperation *scan = scanTx->getNdbScanOperation(plan->table);
    scan->readTuplesExclusive();
    
    /* execute NoCommit */
    if((res = scanTx->execute(NdbTransaction::NoCommit)) != 0) 
      logger->log(LOG_WARNING, 0, "execute(NoCommit): %s\n", 
                  scanTx->getNdbError().message);
    
    /* scan and delete.  delTx takes over the lock. */
    while(scan->nextResult(true) == 0) {
      do {
        if((res = scan->deleteCurrentTuple(delTx)) == 0) {
          delTxRowCount += 1;
        }
        else {      
          logger->log(LOG_WARNING, 0, "deleteCurrentTuple(): %s\n", 
                      scanTx->getNdbError().message);
        }
       } while((check = scan->nextResult(false)) == 0);
      
      /* execute a batch (NoCommit) */
      if(check != -1) {
        res = delTx->execute(NdbTransaction::NoCommit,
                             NdbOperation::AbortOnError, force_send);
        if(res != 0) {
          stats.errors += 1;
          if(delTx->getNdbError().code == 827) { 
            /* DataMemory is full, and the kernel could not create a Copy Tuple
               for a deleted row.  Rollback this batch, turn off force-send 
               (for throttling), make batches smalller, and trigger a
               rescan to clean up these rows. */
            rescan = true;
            delTx->execute(NdbTransaction::Rollback);
            delTx->close();
            delTx = inst->db->startTransaction();
            delTxRowCount = 0;
            if(batch_size > 1) batch_size /= 2;
            force_send = 0;
          }
          else {
            logger->log(LOG_WARNING, 0, "execute(NoCommit): %s\n", 
                        delTx->getNdbError().message);
          }
        }
      }
      
      /* Execute & commit a batch */
      if(delTxRowCount >= batch_size) {
        stats.commit_batches += 1;
        res = delTx->execute(NdbTransaction::Commit, 
                             NdbOperation::AbortOnError, force_send);
        if(res != 0) {
          stats.errors++;
          logger->log(LOG_WARNING, 0, "execute(Commit): %s\n", 
                      delTx->getNdbError().message);
        }
        stats.rows += delTxRowCount;
        delTxRowCount = 0;
        delTx->close();
        delTx = inst->db->startTransaction();
        batch_size *= 2;
        if(batch_size > max_batch_size) {
          batch_size = max_batch_size;
          force_send = 1;
        }
      }
    }
    /* Final execute & commit */
    res = delTx->execute(NdbTransaction::Commit);
    delTx->close();
    scanTx->close();

  } while(rescan);
  
  logger->log(EXTENSION_LOG_INFO, 0, "Flushed all rows from %s.%s: "
              "Scans: %d  Batches: %d  Rows: %d  Errors: %d",
              plan->spec->schema_name, plan->spec->table_name, 
              stats.scans, stats.commit_batches, stats.rows, stats.errors);

  return (res == 0);
}


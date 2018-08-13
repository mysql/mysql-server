/*
 Copyright (c) 2011, 2018, Oracle and/or its affiliates. All rights reserved.
 
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

/* configure defines */
#include "my_config.h"

/* System headers */
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <inttypes.h>
#include <ctype.h>
#include <arpa/inet.h>

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
#include "ExpireTime.h"
#include "ExternalValue.h"
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
#include "ndb_error_logger.h"
#include "ndb_engine_errors.h"

/**********************************************************
  Schedduler::schedule()
    worker_prepare_operation(workitem *) 
      WorkerStep1::do_op()
        NdbTransaction::executeAsynchPrepare() with ndb_async_callback 
                ...
   ndb_async_callback 
     * (workitem->next_step)() 
************************************************************/     

/*  The first phase of any operation is implemented as a method which begins
    an NDB async transaction and returns an op_status_t to the scheduler.
*/

class WorkerStep1 {
public:
  WorkerStep1(struct workitem *);
  op_status_t do_append();           // begin an append/prepend operation
  op_status_t do_read();             // begin a read operation 
  op_status_t do_write();            // begin a SET/ADD/REPLACE operation
  op_status_t do_delete();           // begin a delete operation 
  op_status_t do_math();             // begin an INCR/DECR operation
  
private:
  /* Private member variables */
  workitem *wqitem;
  NdbTransaction *tx;
  QueryPlan * &plan;

  /* Private methods*/
  bool setKeyForReading(Operation &);
  bool startTransaction(Operation &);
};


/* Whenever an NDB async operation completes, control returns to a
   callback function defined in executeAsyncPrepare().
   In case of common errors, the main callback closes the tx and yields.
   The incr callback has special-case error handling for increments.

   typedef void ndb_async_callback(int, NdbTransaction *, void *);
*/

ndb_async_callback callback_main;     // general purpose callback
ndb_async_callback callback_incr;     // special callback for incr/decr
ndb_async_callback callback_close;    // just call worker_close() 


/* 
   The next step is a function that conforms to the worker_step signature.
   It must either call yield() or reschedule(), and is also responsible for 
   closing the transaction.  The signature is in ndb_worker.h: 

   FIXME: yield() and reschedule() no longer exist --- what must it do ?????

   typedef void worker_step(NdbTransaction *, workitem *);
*/

worker_step worker_close;             /* Close tx and yield scheduler */
worker_step worker_commit;            /* exec(Commit) if needed before close */
worker_step worker_append; 
worker_step worker_check_read;
worker_step worker_finalize_read;
worker_step worker_finalize_write;

/*****************************************************************************/

/* Misc utility functions */
void worker_set_cas(ndb_pipeline *, uint64_t *);
int build_cas_routine(NdbInterpretedCode *r, int cas_col, uint64_t cas_val);
void build_hash_item(workitem *, Operation &,  ExpireTime &);

/* Extern pointers */
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

/* Return Status Descriptions */
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
  
status_block status_block_idx_insert = 
  { ENGINE_NOT_STORED, "Cannot insert via unique index" };

status_block status_block_too_big = 
  { ENGINE_E2BIG, "Value too large"                   };

status_block status_block_no_mem =
  { ENGINE_ENOMEM, "NDB out of data memory"           };

status_block status_block_temp_failure = 
  { ENGINE_TMPFAIL, "NDB Temporary Error"             };

status_block status_block_op_not_supported =
  { ENGINE_ENOTSUP, "Operation not supported"         };

status_block status_block_op_bad_key =
  { ENGINE_EINVAL,  "Invalid Key"                     };


/*  valgrind will complain that setting "* wqitem->cas = x" is an invalid
    write of 8 bytes.  But this is not a bug, just an artifact of the unorthodox
    way memcached allocates the (optional) 8 byte CAS ID past the end of a
    defined structure.
*/
void worker_set_cas(ndb_pipeline *p, uint64_t *cas) {
  /* Be careful here --  atomic_int32_t might be a signed type.
     Shitfting of signed types behaves differently. */
  bool did_inc;
  uint32_t cas_lo;
  uint32_t & cas_hi = p->engine->cas_hi;
  do {
    cas_lo = p->engine->cas_lo;    
    did_inc = atomic_cmp_swap_int(& p->engine->cas_lo, cas_lo, cas_lo + 1);
  } while(! did_inc);
  *cas = uint64_t(cas_lo) | (uint64_t(cas_hi) << 32);
  DEBUG_PRINT_DETAIL("hi:%lx lo:%lx cas:%llx (%llu)", cas_hi, cas_lo, *cas, *cas);
}


/* worker_set_ext_flag():
   Determine whether a workitem should take the special "external values" path.
   Sets item->base.use_ext_val
*/
void worker_set_ext_flag(workitem *item) {
  bool result = false; 
  
  if(item->plan->canHaveExternalValue()) {
    switch(item->base.verb) {
      /* INSERTS only need the special path if the value is large */
      case OPERATION_ADD:
        result = item->plan->shouldExternalizeValue(item->cache_item->nbytes);
        break;

      case OP_ARITHMETIC:
        result = false;
        break;
      
      default:
        result = true;
    }
  }
  item->base.use_ext_val = result;
  DEBUG_PRINT_DETAIL(" %d.%d: %s", item->pipeline->id, item->id, result ? "T" : "F");
}


/* worker_prepare_operation(): 
   Called from the scheduler. 
   Returns op_prepared if Scheduler::execute() has been called on the item.
*/
op_status_t worker_prepare_operation(workitem *newitem) {
  WorkerStep1 worker(newitem);
  op_status_t r;

  worker_set_ext_flag(newitem);

  /* Jump table */
  switch(newitem->base.verb) {
    case OP_READ:
      r = worker.do_read();
      break;
      
    case OPERATION_APPEND:
    case OPERATION_PREPEND:
      r = worker.do_append();
      break;

    case OP_DELETE: 
      r = worker.do_delete();
      break;

    case OPERATION_SET:
    case OPERATION_ADD:
    case OPERATION_REPLACE:
    case OPERATION_CAS:
      r = worker.do_write();
      break;

    case OP_ARITHMETIC:
      r = worker.do_math();
      break;
      
    default:
      r = op_not_supported;
  }

  switch(r) {
    case op_not_supported:
      newitem->status = & status_block_op_not_supported;
      break;

    case op_failed:
      newitem->status = & status_block_misc_error;
      break;

    case op_bad_key:
      newitem->status = & status_block_op_bad_key;
      break;

    case op_overflow:
      newitem->status = & status_block_too_big;
      break;

    case op_prepared:
      break;
  }

  return r;
}


/***************** STEP ONE OPERATIONS ***************************************/

bool WorkerStep1::startTransaction(Operation & op) {
  tx = op.startTransaction(wqitem->ndb_instance->db);
  if(tx) {
    return true;
  }
  log_ndb_error(wqitem->ndb_instance->db->getNdbError());
  return false;
}


WorkerStep1::WorkerStep1(workitem *newitem) :
  wqitem(newitem), 
  tx(0),
  plan(newitem->plan) 
{
  /* Set cas_owner in workitem.
     (Further refine the semantics of this.  Does it depend on do_mc_read?)
  */  
    newitem->base.cas_owner = (newitem->prefix_info.has_cas_col);
};


op_status_t WorkerStep1::do_delete() {
  DEBUG_ENTER_DETAIL();

  if(wqitem->base.use_ext_val) {
    return ExternalValue::do_delete(wqitem);
  }
  
  const NdbOperation *ndb_op = 0;
  Operation op(plan, OP_DELETE);
  
  op.key_buffer = wqitem->ndb_key_buffer;
  const char *dbkey = workitem_get_key_suffix(wqitem);
  if(! op.setKey(plan->spec->nkeycols, dbkey, wqitem->base.nsuffix)) {
    return op_overflow;
  }

  if(! startTransaction(op))
    return op_failed;

  /* Here we could also support op.deleteTupleCAS(tx, & options)
     but the protocol is ambiguous about whether this is allowed.
  */ 
  ndb_op = op.deleteTuple(tx);    
  
  /* Check for errors */
  if(ndb_op == 0) {
    const NdbError & err = tx->getNdbError();
    if(err.status != NdbError::Success) {
      log_ndb_error(err);
      tx->close();
      return op_failed;
    }
  }
  
  /* Prepare for execution */   
  Scheduler::execute(tx, NdbTransaction::Commit, callback_main, wqitem, YIELD);
  return op_prepared;
}


op_status_t WorkerStep1::do_write() {
  DEBUG_PRINT_DETAIL("%s", workitem_get_operation(wqitem));

  if(wqitem->base.use_ext_val) {
    return ExternalValue::do_write(wqitem);
  }
  
  uint64_t cas_in = *wqitem->cas;                  // read old value
  if(wqitem->base.cas_owner) {
    worker_set_cas(wqitem->pipeline, wqitem->cas);    // generate a new value
    hash_item_set_cas(wqitem->cache_item, * wqitem->cas); // store it
  }
  
  const NdbOperation *ndb_op = 0;
  Operation op(wqitem);
  const char *dbkey = workitem_get_key_suffix(wqitem);
  bool op_ok;
  
  /* Set the key */
  op_ok = op.setKey(plan->spec->nkeycols, dbkey, wqitem->base.nsuffix);
  if(! op_ok) {
    return op_overflow;
  }

  /* Allocate and encode the buffer for the row */
  workitem_allocate_rowbuffer_1(wqitem, op.requiredBuffer());
  op.buffer = wqitem->row_buffer_1;
  
  /* Set the row */
  op.setNullBits();
  op.setKeyFieldsInRow(plan->spec->nkeycols,  dbkey, wqitem->base.nsuffix);
  
  if(plan->spec->nvaluecols > 1) {
    /* Multiple Value Columns */
    TabSeparatedValues tsv(hash_item_get_data(wqitem->cache_item), 
                           plan->spec->nvaluecols, wqitem->cache_item->nbytes); 
    int idx = 0;
    do {
      if(tsv.getLength()) {
        op_ok = op.setColumn(COL_STORE_VALUE+idx, tsv.getPointer(), tsv.getLength());
        if(! op_ok) {
          return op_overflow;
        }
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
    if(! op_ok) {
      return op_overflow;
    }
  }
  
  if(wqitem->base.cas_owner) {
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
        DEBUG_PRINT_DETAIL(" dup_numbers -- %d", (int) number );
        op.setColumnBigUnsigned(COL_STORE_MATH, number);
      }
      else {  // non-numeric
        DEBUG_PRINT_DETAIL(" dup_numbers but non-numeric: %.*s *** ", len, value);
        op.setColumnNull(COL_STORE_MATH);
      }
    }
    else op.setColumnNull(COL_STORE_MATH);      
  }

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
  
  /* Start the transaction */
  if(! startTransaction(op))
    return op_failed;

  if(wqitem->base.verb == OPERATION_REPLACE) {
    DEBUG_PRINT(" [REPLACE] \"%.*s\"", wqitem->base.nkey, wqitem->key);
    ndb_op = op.updateTuple(tx);
  }
  else if(wqitem->base.verb == OPERATION_ADD) {
    DEBUG_PRINT(" [ADD]     \"%.*s\"", wqitem->base.nkey, wqitem->key);
    ndb_op = op.insertTuple(tx);
  }
  else if(wqitem->base.verb == OPERATION_CAS) {    
    if(wqitem->base.cas_owner) {
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
      ndb_op = op.updateTuple(tx, & options);
    }
  }
  else if(wqitem->base.verb == OPERATION_SET) {
    DEBUG_PRINT(" [SET]     \"%.*s\"", wqitem->base.nkey, wqitem->key);
    ndb_op = op.writeTuple(tx);
  }
  
  /* Error case; operation has not been built */
  if(! ndb_op) {
    log_ndb_error(tx->getNdbError());
    DEBUG_PRINT("NDB operation failed.  workitem %d.%d", wqitem->pipeline->id,
                wqitem->id);
    tx->close();
    return op_failed;
  }
  
  wqitem->next_step = (void *) worker_finalize_write;
  Scheduler::execute(tx, NdbTransaction::Commit, callback_main, wqitem, YIELD);
  return op_prepared;  
}


op_status_t WorkerStep1::do_read() {
  DEBUG_ENTER_DETAIL();
  
  Operation op(plan, OP_READ);
  if(! setKeyForReading(op)) {
    return op_overflow;
  }

  NdbOperation::LockMode lockmode;
  NdbTransaction::ExecType commitflag;
  if(plan->canUseCommittedRead()) {
    lockmode = NdbOperation::LM_CommittedRead;
    commitflag = NdbTransaction::Commit;
  }
  else {
    lockmode = NdbOperation::LM_Read;
    commitflag = NdbTransaction::NoCommit;
  }
  
  if(! op.readTuple(tx, lockmode)) {
    log_ndb_error(tx->getNdbError());
    tx->close();
    return op_failed;
  }
  
  /* Save the workitem in the transaction and prepare for async execution */ 
  wqitem->next_step = (void *) 
    (wqitem->base.use_ext_val ? worker_check_read : worker_finalize_read);
  Scheduler::execute(tx, commitflag, callback_main, wqitem, YIELD);
  return op_prepared;  
}


op_status_t WorkerStep1::do_append() {
  DEBUG_ENTER_DETAIL();
  
  /* APPEND/PREPEND is currently not supported for tsv */
  if(wqitem->plan->spec->nvaluecols > 1) {
    return op_not_supported;
  }
  Operation op(plan, OP_READ);
  if(! setKeyForReading(op)) {
    return op_overflow;
  }
  
  /* Read with an exculsive lock */
  if(! op.readTuple(tx, NdbOperation::LM_Exclusive)) {
    log_ndb_error(tx->getNdbError());
    tx->close();
    return op_failed;
  }
  
  /* Prepare for async execution */
  wqitem->next_step = (void *) worker_append;
  Scheduler::execute(tx, NdbTransaction::NoCommit, callback_main, wqitem, YIELD);
  return op_prepared;
}


bool WorkerStep1::setKeyForReading(Operation &op) {

  /* Use the workitem's inline key buffer */
  op.key_buffer = wqitem->ndb_key_buffer;
  
  /*  Allocate a new result buffer large enough for the result.
   Add 2 bytes to hold potential \r\n in a no-copy result. */
  workitem_allocate_rowbuffer_1(wqitem, op.requiredBuffer() + 2);
  op.buffer = wqitem->row_buffer_1;
  
  /* set the key */
  op.clearKeyNullBits();
  const char *dbkey = workitem_get_key_suffix(wqitem);
  if(! op.setKey(plan->spec->nkeycols, dbkey, wqitem->base.nsuffix))
    return false;
  
  /* Start a transaction */
  return startTransaction(op);
}



op_status_t WorkerStep1::do_math() {
  DEBUG_PRINT_DETAIL("create: %d   retries: %d",
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
  Operation op1(plan, OP_READ, wqitem->ndb_key_buffer);
  Operation op2(wqitem);    // insert
  Operation op3(wqitem);    // update

  op1.readSelectedColumns();
  op1.readColumn(COL_STORE_MATH);
  
  if(! wqitem->base.retries) {
    /* Allocate & populate row buffers for these operations: 
     We need one for the read and one for the insert.  */
    size_t needed = op1.requiredBuffer();
    workitem_allocate_rowbuffer_1(wqitem, needed);
    workitem_allocate_rowbuffer_2(wqitem, needed);    
    op1.buffer = wqitem->row_buffer_1;
    op2.buffer = wqitem->row_buffer_2;
    op3.buffer = wqitem->row_buffer_2;
    
    /* The two items share a key buffer, so we encode the key just once */
    op1.setKey(plan->spec->nkeycols, dbkey, wqitem->base.nsuffix);
    
    /* The insert operation also needs the key written into the row */
    op2.clearNullBits();
    op2.setKeyFieldsInRow(plan->spec->nkeycols, dbkey, wqitem->base.nsuffix);
    
    /* CAS */
    if(wqitem->base.cas_owner) {
      op1.readColumn(COL_STORE_CAS);
      op2.setColumnBigUnsigned(COL_STORE_CAS, * wqitem->cas);
      op3.setColumnBigUnsigned(COL_STORE_CAS, * wqitem->cas);
    }
    /* In "dup_numbers" mode, also null out the text version of the value */
    if(wqitem->plan->dup_numbers) {
      op2.setColumnNull(COL_STORE_VALUE);
      op3.setColumnNull(COL_STORE_VALUE);
    }
  } 
  
  /* Use an op (either one) to start the transaction */
  if(! startTransaction(op1))
    return op_failed;

  /* NdbOperation #1: READ */
  {
    ndbop1 = op1.readTuple(tx, NdbOperation::LM_Exclusive);
    if(! ndbop1) {
      log_ndb_error(tx->getNdbError());
      tx->close();
      return op_failed; 
    }
  }
  
  /* NdbOperation #2: INSERT (AO_IgnoreError) */
  if(wqitem->base.math_create) {
    /* Offset the initial value to compensate for the update */
    uint64_t initial_value;
    if(wqitem->base.math_incr)
      initial_value = wqitem->math_value - wqitem->math_flags;  // incr
    else
      initial_value = wqitem->math_value + wqitem->math_flags;  // decr
    op2.setColumnBigUnsigned(COL_STORE_MATH, initial_value);
    
    /* If this insert gets an error, the transaction should continue. */
    NdbOperation::OperationOptions options;
    options.optionsPresent = NdbOperation::OperationOptions::OO_ABORTOPTION;  
    options.abortOption = NdbOperation::AO_IgnoreError;
    
    ndbop2 = op2.insertTuple(tx, & options); 
    if(! ndbop2) {
      log_ndb_error(tx->getNdbError());
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
      code.add_val(plan->math_column_id, wqitem->math_flags);   
      code.interpret_exit_ok();
    }
    else {                                                        // decr
      const Uint32 Rdel = 1, Rcol = 2, Rres = 3;       // registers 1,2,3
      const Uint32 SUB_ZERO = 0;                       // a label
      
      code.load_const_u64(Rdel, wqitem->math_flags);   // load R1, delta
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
    
    ndbop3 = op3.updateTuple(tx, & options);
    if(! ndbop3) {
      log_ndb_error(tx->getNdbError());
      tx->close();
      return op_failed;
    }
  }
  
  Scheduler::execute(tx,NdbTransaction::Commit, callback_incr, wqitem, YIELD);
  return op_prepared;
}


/***************** NDB CALLBACKS *********************************************/
void callback_main(int, NdbTransaction *tx, void *itemptr) {
  workitem *wqitem = (workitem *) itemptr;
    
  /************** Error handling ***********/  
  /* No Error */
  if(tx->getNdbError().classification == NdbError::NoError) {
    DEBUG_PRINT("Success.");
    wqitem->status = & status_block_generic_success;
    if(wqitem->next_step) {
      /* Control moves forward to the next step of the operation */
      worker_step * next_step = (worker_step *) wqitem->next_step;
      wqitem->next_step = 0;
      next_step(tx, wqitem);
      return;
    }
  }
  /* CAS mismatch; interpreted code aborted with interpret_exit_nok() */
  else if(tx->getNdbError().code == 2010) {
    DEBUG_PRINT("CAS mismatch.");
    * wqitem->cas = 0ULL;  // set cas=0 in the response. see note re. valgrind.
    wqitem->status = & status_block_cas_mismatch;    
  }
  /* No Data Found */
  else if(tx->getNdbError().classification == NdbError::NoDataFound) {
    /* Error code should be 626 */
    DEBUG_PRINT("NoDataFound [%d].", tx->getNdbError().code);
    if(wqitem->cas) * wqitem->cas = 0ULL;   // see note re. valgrind
    switch(wqitem->base.verb) {
      case OPERATION_REPLACE:
      case OPERATION_APPEND:
      case OPERATION_PREPEND:
        wqitem->status = & status_block_bad_replace;
        break;
      default:
        wqitem->status = & status_block_item_not_found;
        break;
    }
  }  
  /* Duplicate key on insert */
  else if(tx->getNdbError().code == 630) {
    DEBUG_PRINT("Duplicate key on insert.");
    if(wqitem->cas) * wqitem->cas = 0ULL;   // see note re. valgrind
    wqitem->status = & status_block_bad_add;    
  }
  /* Overload Error, e.g. 410 "REDO log files overloaded" */
  else if(tx->getNdbError().classification == NdbError::OverloadError) {
    log_ndb_error(tx->getNdbError());
    wqitem->status = & status_block_temp_failure;
  }
  /* Attempt to insert via unique index access */
  else if(tx->getNdbError().code == 897) {
    wqitem->status = & status_block_idx_insert;
  }
  /* Out of memory */
  else if(tx->getNdbError().code == 827 ||
          tx->getNdbError().code == 921)
  {
    log_ndb_error(tx->getNdbError());
    wqitem->status = & status_block_no_mem;
  }
  /* Some other error.
     The get("dummy") in mtr's memcached_wait_for_ready.inc script will often
     get a 241 or 284 error here.
  */
  else  {
    log_ndb_error(tx->getNdbError());
    wqitem->status = & status_block_misc_error;
  }

  worker_commit(tx, wqitem);
}


void callback_incr(int result, NdbTransaction *tx, void *itemptr) {
  workitem *wqitem = (workitem *) itemptr;
  // ndb_pipeline * & pipeline = wqitem->pipeline;
  
  /*  read  insert  update cr_flag response
   ------------------------------------------------------------------------
   626   0       0        0     return NOT_FOUND.
   626   0       0        1     row was created.  return initial_value.
   0     x       0        x     row existed.  return fetched_value + delta.
   x     x       626      x     failure due to race with concurrent delete.
   */
  
  const NdbOperation *ndbop1, *ndbop2, *ndbop3;
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
  DEBUG_PRINT_DETAIL("r_read: %d   r_insert: %d   r_update: %d   create: %d",
              r_read, r_insert, r_update, wqitem->base.math_create);
  
  if(r_read == 626 && ! wqitem->base.math_create) {
    /* row did not exist, and create flag was not set */
    wqitem->status = & status_block_item_not_found;
  }
  else if(r_read == 0 && r_update == 0) {
    /* row existed.  return fetched_value +/- delta. */
    Operation op(wqitem->plan, OP_READ);
    op.buffer = wqitem->row_buffer_1;
    uint64_t stored = op.getBigUnsignedValue(COL_STORE_MATH);
    if(wqitem->base.math_incr) {              // incr
      wqitem->math_value = stored + wqitem->math_flags;
    }
    else {                                    // decr
      if(wqitem->math_flags > stored)
        wqitem->math_value = 0; // underflow < 0 is not allowed
      else
        wqitem->math_value = stored - wqitem->math_flags;
    }
    
    wqitem->status = & status_block_generic_success;
  }  
  else if(r_read == 626 && r_insert == 0 && r_update == 0) {
    /* row was created.   Return initial_value.
     wqitem->math_value is already set to the initial_value :)  */
    wqitem->status = & status_block_generic_success;
  }
  else if(r_read == -1 || r_insert == -1 || r_update == -1) {
    /* Total failure */
    logger->log(LOG_WARNING, 0, "incr/decr: total failure.\n");
    wqitem->status = & status_block_misc_error;
  }
  else if(r_update == 626) {
    /*  failure due to race with concurrent delete */
    // TODO: design a test for this code.  Does it require reschedule()?
    if(wqitem->base.retries++ < 3) {       // try again:
      tx->close();
      op_status_t r = worker_prepare_operation(wqitem); 
      if(r == op_prepared)
        return;  /* retry is in progress */
      else
        wqitem->status = & status_block_misc_error;
    }
    else { 
      logger->log(LOG_WARNING, 0, "incr/decr: giving up, too many retries.\n");
      wqitem->status = & status_block_misc_error;
    }
  }

  worker_close(tx, wqitem);  
}


void callback_close(int result, NdbTransaction *tx, void *itemptr) {
  if(result) log_ndb_error(tx->getNdbError());
  workitem *wqitem = (workitem *) itemptr;
  worker_close(tx, wqitem);
}


/***************** WORKER STEPS **********************************************/

void worker_commit(NdbTransaction *tx, workitem *item) {
  /* If the transaction has not been committed, we need to send an empty 
     execute call and commit it.  Otherwise close() will block. */
  if(tx->commitStatus() == NdbTransaction::Started) {
    Scheduler::execute(tx, NdbTransaction::Commit, callback_close, item, RESCHEDULE);
  }
  else 
    worker_close(tx, item);
}


void worker_close(NdbTransaction *tx, workitem *wqitem) {
  DEBUG_PRINT_DETAIL("%d.%d", wqitem->pipeline->id, wqitem->id);
  ndb_pipeline * & pipeline = wqitem->pipeline;

  if(wqitem->ext_val)
    delete wqitem->ext_val;

  pipeline->scheduler->close(tx, wqitem);
}


void worker_append(NdbTransaction *tx, workitem *item) {
  if(item->base.use_ext_val) {
    ExternalValue::append_after_read(tx, item);
    return;
  }
    
  DEBUG_PRINT("%d.%d", item->pipeline->id, item->id);
 
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
  if(readop.nValues() != 1) {
    return worker_close(tx, item);
  }
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
  DEBUG_PRINT_DETAIL("New value: %.*s%s", total_len < 100 ? total_len : 100,
                     current_val, total_len > 100 ? " ..." : "");
  
  /* Set the row */
  op.setNullBits();
  op.setKeyFieldsInRow(item->plan->spec->nkeycols, 
                       workitem_get_key_suffix(item), item->base.nsuffix);
  op.setColumn(COL_STORE_VALUE, current_val, total_len);
  if(item->prefix_info.has_cas_col) 
    op.setColumnBigUnsigned(COL_STORE_CAS, * item->cas);
  ndb_op = op.updateTuple(tx);

  if(ndb_op) {
    // Inform the scheduler that this item must be re-polled
    item->next_step = (void *) worker_finalize_write;
    Scheduler::execute(tx, NdbTransaction::Commit, callback_main, item, RESCHEDULE);
  }
  else {
    /* Error case; operation has not been built */
    DEBUG_PRINT("NDB operation failed.  workitem %d.%d", item->pipeline->id,
                item->id);
    worker_close(tx, item);
  }
}


void worker_check_read(NdbTransaction *tx, workitem *wqitem) {
  Operation op(wqitem->plan, OP_READ);
  op.buffer = wqitem->row_buffer_1;

  if(op.isNull(COL_STORE_EXT_SIZE)) {
    worker_finalize_read(tx, wqitem);
  }
  else {
    ExternalValue *ext_val = new ExternalValue(wqitem);
    ext_val->worker_read_external(op, tx);
  }
}


void delete_expired_item(workitem *wqitem, NdbTransaction *tx) {
  DEBUG_PRINT(" Deleting [%d.%d]", wqitem->pipeline->id, wqitem->id);
  Operation op(wqitem);
  op.deleteTuple(tx);
  wqitem->status = & status_block_item_not_found;
  Scheduler::execute(tx, NdbTransaction::Commit, callback_close, wqitem, RESCHEDULE);
}


void worker_finalize_read(NdbTransaction *tx, workitem *wqitem) {
  ExpireTime exp_time(wqitem);
  Operation op(wqitem->plan, OP_READ);
  op.buffer = wqitem->row_buffer_1;
 
  if(exp_time.stored_item_has_expired(op)) {
    delete_expired_item(wqitem, tx);
    return;
  }
 
  if(wqitem->prefix_info.has_flags_col && ! op.isNull(COL_STORE_FLAGS))
    wqitem->math_flags = htonl(op.getIntValue(COL_STORE_FLAGS));
  else if(wqitem->plan->static_flags)
    wqitem->math_flags = htonl(wqitem->plan->static_flags);
  else
    wqitem->math_flags = 0;

  if(wqitem->prefix_info.has_cas_col) {
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
    DEBUG_PRINT("%d.%d using no-copy buffer.", wqitem->pipeline->id, wqitem->id);
    wqitem->base.has_value = true;
    /* "cache_item == workitem" is a sort of code, required because memcached
        expects us to return a non-zero item.  In ndb_release() we will look 
        for this and use it to prevent double-freeing of the workitem.  */
    wqitem->cache_item = (hash_item *) wqitem;    
  }
  else {
    /* Copy the value into a new buffer */
    DEBUG_PRINT("%d.%d copying value.", wqitem->pipeline->id, wqitem->id);
    build_hash_item(wqitem, op, exp_time);
  }

  worker_commit(tx, wqitem);
}


void worker_finalize_write(NdbTransaction *tx, workitem *wqitem) {
  if(wqitem->prefix_info.do_mc_write) {
    /* If the write was successful, update the local cache */
    /* Possible bugs here: 
     (1) store_item will store nbytes as length, which is wrong.
     (2) The CAS may be incorrect.
     Status as of Feb. 2013: 
        Memcapable INCR/DECR/APPEND/PREPEND tests fail when
        local caching is enabled.
    */
    ndb_pipeline * & pipeline = wqitem->pipeline;
    struct default_engine * se;
    se = (struct default_engine *) pipeline->engine->m_default_engine;    
    ENGINE_ERROR_CODE status;

    status = store_item(se, wqitem->cache_item, 
                   hash_item_get_cas_ptr(wqitem->cache_item),
                   OPERATION_SET, wqitem->cookie);
    if (status != ENGINE_SUCCESS) {
      wqitem->status = & status_block_memcache_error;
    }
  }
  
  worker_close(tx, wqitem);
}


/*****************************************************************************/



/*  Allocate a hash table item, populate it with the original key 
    and the results from the read, then store it.    
 */
void build_hash_item(workitem *wqitem, Operation &op, ExpireTime & exp_time) {
  DEBUG_ENTER();
  ndb_pipeline * & pipeline = wqitem->pipeline;
  struct default_engine *se;
  se = (struct default_engine *) pipeline->engine->m_default_engine;      

  size_t nbytes = op.getStringifiedLength() + 2;  /* 2 bytes for \r\n */
    
  /* Allocate a hash item */
  /* item_alloc(engine, key, nkey, flags, exptime, nbytes, cookie) */
  hash_item * item = item_alloc(se, wqitem->key, wqitem->base.nkey, 
                                wqitem->math_flags,
                                exp_time.local_cache_expire_time, 
                                nbytes, wqitem->cookie);
  
  if(item) {
    /* Now populate the item with the result */
    size_t ncopied = 0;
    memcpy(hash_item_get_key(item), wqitem->key, wqitem->base.nkey); // the key
    char * data_ptr = hash_item_get_data(item);
    
    /* Maybe use the math column as the value */
    if(    wqitem->plan->hasMathColumn() 
        && (! op.isNull(COL_STORE_MATH))
        && ( (op.nValues() == 0)
             || (wqitem->plan->dup_numbers && op.isNull(COL_STORE_VALUE)) 
           )  
       ) {
      ncopied = op.copyValue(COL_STORE_MATH, data_ptr);
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
      uint64_t *cas = hash_item_get_cas_ptr(item);
      ENGINE_ERROR_CODE status;
      status = store_item(se, item, cas, OPERATION_SET, wqitem->cookie);
      if(status != ENGINE_SUCCESS)
        wqitem->status = & status_block_memcache_error;
    }
  }
  else {
    DEBUG_PRINT("Failure.  Item: %p", item);
    wqitem->status = & status_block_memcache_error;
  }
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


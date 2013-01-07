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
#include <assert.h>
#include <ctype.h>

/* Memcache headers */
#include "memcached/types.h"
#include <memcached/extension_loggers.h>

/* NDB headers */
#include "NdbApi.hpp"

/* NDB Memcache headers */
#include "ndbmemcache_global.h"
#include "Configuration.h"
#include "ExternalValue.h"
#include "debug.h"
#include "Operation.h"
#include "NdbInstance.h"
#include "ndb_pipeline.h"
#include "ndb_error_logger.h"
#include "ndb_worker.h"

/* Extern pointers */
extern EXTENSION_LOGGER_DESCRIPTOR *logger;

/* Scan helpers */

// nextResult() return values:
enum { 
  fetchError         = -1,
  fetchOK            =  0,
  fetchScanFinished  =  1,
  fetchCacheEmpty    =  2
};

enum { fetchFromThisBatch = false, fetchNewBatchFromKernel = true };
enum { SendImmediate = true, sendDeferred = false };

bool scan_delete(NdbInstance *, QueryPlan *);
bool scan_delete_ext_val(ndb_pipeline *, NdbInstance *, QueryPlan *);


/*************** SYNCHRONOUS IMPLEMENTATION OF "FLUSH ALL" ******************/

/* Flush all is a fully synchronous operation -- 
 the memcache server is waiting for a response, and the thread is blocked.
 */
ENGINE_ERROR_CODE ndb_flush_all(ndb_pipeline *pipeline) {
  DEBUG_ENTER();
  bool r;
  const Configuration &conf = get_Configuration();
  
  DEBUG_PRINT(" %d prefixes", conf.nprefixes);
  for(unsigned int i = 0 ; i < conf.nprefixes ; i++) {
    const KeyPrefix *pfx = conf.getPrefix(i);
    if(pfx->info.use_ndb && pfx->info.do_db_flush) {
      ClusterConnectionPool *pool = conf.getConnectionPoolById(pfx->info.cluster_id);
      Ndb_cluster_connection *conn = pool->getMainConnection();
      NdbInstance inst(conn, 128);
      QueryPlan plan(inst.db, pfx->table);
      if(plan.pk_access) {
        // To flush, scan the table and delete every row
        if(plan.canHaveExternalValue()) {
          DEBUG_PRINT("prefix %d - doing ExternalValue delete");
          r = scan_delete_ext_val(pipeline, &inst, &plan);
        }
        else {
          DEBUG_PRINT("prefix %d - deleting from %s", i, pfx->table->table_name);
          r = scan_delete(&inst, &plan);
        }
        if(! r) logger->log(LOG_WARNING, 0, "-- FLUSH_ALL Failed.\n");
      }
      else DEBUG_PRINT("prefix %d - not scanning table %s -- accees path "
                       "is not primary key", i, pfx->table->table_name);
    }
    else DEBUG_PRINT("prefix %d - not scanning table %s -- use_ndb:%d flush:%d",
                     i, pfx->table ? pfx->table->table_name : "",
                     pfx->info.use_ndb, pfx->info.do_db_flush);
  }
  
  return ENGINE_SUCCESS;
}


bool scan_delete(NdbInstance *inst, QueryPlan *plan) {
  DEBUG_ENTER();
  bool rescan, fetch_option;
  int rFetch, rExec, rDel, batch_size, rows_deleted;
  int error_status = 0;
  const unsigned int max_errors = 100000;
  
  struct {
    unsigned int errors;
    unsigned int rows;
    unsigned short scans;
    unsigned short commit_batches;
  } stats = {0, 0, 0, 0 };
  
  /* The outer loop performs an initial table scan and then possibly some 
     rescans, which are triggered whenever some rows have been scanned but, 
     due to an error condition, have not been deleted.
   */
  do {
    batch_size = 1;   /* slow start. */
    stats.scans += 1;
    rescan = false;
    
    NdbTransaction *scanTx = inst->db->startTransaction();
    NdbScanOperation *scan = scanTx->getNdbScanOperation(plan->table);
    
    /* Express intent to read with exclusive lock; execute NoCommit */
    scan->readTuplesExclusive();
    rExec = scanTx->execute(NdbTransaction::NoCommit);
    if(rExec != 0) {
      stats.errors++;
      error_status = log_ndb_error(scanTx->getNdbError());
      break;
    }
    
    /* Within a scan, this loop iterates over batches.
       Batches are committed whenever the batch_size has been reached.
       Batch size starts at 1 and doubles when a batch is succesful, 
       until it reaches the result cache size.
     */
    while(1) {
      stats.commit_batches++;
      NdbTransaction *delTx = inst->db->startTransaction();
      rows_deleted = 0;
      fetch_option = fetchNewBatchFromKernel; 
      bool fetch_more;
      
      /* The inner loop iterates over rows within a batch */      
      do {
        fetch_more = false;
        rFetch = scan->nextResult(fetch_option, SendImmediate);
        switch(rFetch) {
          case fetchError:
            stats.errors++;
            error_status = log_ndb_error(scan->getNdbError());
            break; 
            
          case fetchOK:
            rDel = scan->deleteCurrentTuple(delTx);
            if(rDel == 0) {
              fetch_more = ( ++rows_deleted < batch_size);
              fetch_option = fetchFromThisBatch;
            }
            else {
              stats.errors++;
              error_status = log_ndb_error(delTx->getNdbError());
            }
            break;
            
          case fetchScanFinished:        
          case fetchCacheEmpty:
          default:
            break;        
        }
      } while(fetch_more); /* break out of the inner loop to here */
      
      /* Quit now if errors were serious */
      if(error_status > ERR_TEMP)
        break;

      /* Execute the batch */
      rExec = delTx->execute(NdbTransaction::Commit, NdbOperation::AbortOnError, 1);
      if(rExec == 0) {
        stats.rows += rows_deleted;
        if(rFetch != fetchCacheEmpty) 
          batch_size *= 2;
      }
      else {
        stats.errors++;
        error_status = log_ndb_error(delTx->getNdbError());
        if(batch_size > 1) 
          batch_size /= 2;
        rescan = true;
      }
      
      delTx->close();
      
      if(rFetch == fetchScanFinished || (stats.errors > max_errors))
        break;
    } /* break out of the batch loop to here */
    
    scanTx->close();
  } while(rescan && (error_status < ERR_PERM) && stats.errors < max_errors);
  
  logger->log(LOG_WARNING, 0, "Flushed rows from %s.%s: "
              "Scans: %d  Batches: %d  Rows: %d  Errors: %d",
              plan->spec->schema_name, plan->spec->table_name, 
              stats.scans, stats.commit_batches, stats.rows, stats.errors);
  
  return (stats.rows || ! stats.errors);  
}


/* External Values require a different version of FLUSH_ALL, which preserves
   the referential integrity between the main table and the parts table
   while deleting.   This one uses the NdbRecord variant of a scan and commits
   once for each row of the main table.
*/
bool scan_delete_ext_val(ndb_pipeline *pipeline, NdbInstance *inst, 
                         QueryPlan *plan) {
  DEBUG_ENTER();
  int r, ext_rows, error_status = 0;
  bool fetch_more;
  struct {
    Uint32 main_rows;
    Uint32 ext_rows;
    Uint32 errors;
  } stats = {0, 0, 0 };

  /* Need KeyInfo when performing scanning delete */
  NdbScanOperation::ScanOptions opts;
  opts.optionsPresent=NdbScanOperation::ScanOptions::SO_SCANFLAGS;
  opts.scan_flags=NdbScanOperation::SF_KeyInfo;
  
  memory_pool * pool = pipeline_create_memory_pool(pipeline);
  NdbTransaction *scanTx = inst->db->startTransaction();
  Operation op(plan, OP_SCAN);  
  op.readSelectedColumns();
  op.readColumn(COL_STORE_EXT_SIZE);
  op.readColumn(COL_STORE_EXT_ID);
    
  NdbScanOperation *scan = op.scanTable(scanTx, NdbOperation::LM_Exclusive, &opts);
  r = scanTx->execute(NdbTransaction::NoCommit); 
  
  if(r == 0) {   /* Here's the scan loop */
    do {
      fetch_more = false;
      r = scan->nextResult((const char **) & op.buffer, true, true);
      if(r == fetchOK) {
        fetch_more = true;      
        NdbTransaction * delTx = inst->db->startTransaction();

        op.deleteCurrentTuple(scan, delTx);                        // main row
        ext_rows = ExternalValue::do_delete(pool, delTx, plan, op);  // parts

        r = delTx->execute(NdbTransaction::Commit, 
                           NdbOperation::AbortOnError, 
                           SendImmediate);
        if(r)
          error_status = log_ndb_error(delTx->getNdbError()), stats.errors++;
        else 
          stats.main_rows++, stats.ext_rows += ext_rows;
   
        memory_pool_free(pool);
        delTx->close();
      }
      else {
        break;
      }
    } while(fetch_more && (error_status < ERR_PERM));
  }
  
  memory_pool_destroy(pool);
  scanTx->close();

  logger->log(LOG_WARNING, 0, "Flushed %d rows from %s plus "
              "%d rows from %s.  Errors: %d\n",  
              stats.main_rows, plan->spec->table_name, 
              stats.ext_rows, plan->extern_store->spec->table_name,
              stats.errors);
  
  return (stats.main_rows || ! stats.errors);
}



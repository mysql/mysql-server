
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


#include "Operation.h"


/* 
   Class Operation originated as a header-only class and formed a bridge 
   between the "high-level" application code in ndb_worker.cc and the 
   low-level details of using the NDB API.

   This proved difficult to debug.  On many platforms, gdb's support for
   inlined functions from header-only classes is poor.

   A few methods are always compiled here.
*/


Operation::Operation(QueryPlan *p, int o, char *kbuf) : key_buffer(kbuf), 
                                                        plan(p), 
                                                        op(o)                                                        
{
  if(op == OP_READ) record = plan->val_record;
  else if(op == OP_FLUSH) record = plan->key_record;  // scanning delete 
  else record = plan->row_record;
}


/* Methods for reading columns from the response */


bool Operation::getStringValueNoCopy(int idx, char **dstptr, 
                                     size_t *lenptr) const {
  if(record->isNull(idx, buffer)) {
    *dstptr = 0;
    *lenptr = 0;
    return true;
  }
  return record->decodeNoCopy(idx, dstptr, lenptr, buffer);
}


size_t Operation::copyValue(int idx, char *dest) const {
  if(record->isNull(idx, buffer)) {
    *dest = 0;
    return 0;
  }
  return record->decodeCopy(idx, dest, buffer);
}


/* NdbTransaction method wrappers */

NdbTransaction * Operation::startTransaction() const {
  char hash_buffer[512];
  return plan->db->startTransaction(plan->key_record->ndb_record, key_buffer,
                                    hash_buffer, 512);
}

NdbIndexScanOperation * Operation::scanIndex(NdbTransaction *tx,
                                             NdbIndexScanOperation::IndexBound *bound) {
  /* MUST BE ORDERED ASC; used by configuration to read key_prefixes */
  NdbScanOperation::ScanOptions opts;
  opts.optionsPresent = NdbScanOperation::ScanOptions::SO_SCANFLAGS;
  opts.scan_flags = NdbScanOperation::SF_OrderBy;
  
  return tx->scanIndex(plan->key_record->ndb_record,            // scan key    
                       plan->row_record->ndb_record,            // row record  
                       NdbOperation::LM_Read,                   // lock mode   
                       (const unsigned char*) 0,                // result mask 
                       bound,                                   // bound       
                       & opts,                            
                       sizeof(opts));
}


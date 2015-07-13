
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

#include "Operation.h"
#include "TabSeparatedValues.h"


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
  set_default_record();
}

Operation::Operation(workitem *i, Uint32 mask) : key_buffer(i->ndb_key_buffer), 
                                                 plan(i->plan),
                                                 op(i->base.verb)
{
  set_default_record();
  if(mask) {
    row_mask[0] = mask & 0x000000FF;
    row_mask[1] = mask & 0x0000FF00;
    row_mask[2] = mask & 0x00FF0000;
    row_mask[3] = mask & 0xFF000000;
  }
}

Operation::Operation(QueryPlan *p, char * buf) : buffer(buf),
                                                 plan(p),
                                                 op(OP_READ)
{
  set_default_record();
}


void Operation::set_default_record() {
  row_mask[3] = row_mask[2] = row_mask[1] = row_mask[0] = 0;
  key_mask[3] = key_mask[2] = key_mask[1] = key_mask[0] = 0;
  read_mask_ptr = 0;
  
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

NdbTransaction * Operation::startTransaction(Ndb *db) const {
  char hash_buffer[512];
  return db->startTransaction(plan->key_record->ndb_record, key_buffer,
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


bool Operation::setKey(int nparts, const char *dbkey, size_t key_len ) {
  bool r = true;
  
  clearKeyNullBits();
  if(nparts > 1) {
    TabSeparatedValues tsv(dbkey, nparts, key_len);
    int idx = 0;
    do {
      if(tsv.getLength()) {
        DEBUG_PRINT("Set key part %d [%.*s]", idx, tsv.getLength(), tsv.getPointer());
        if(! setKeyPart(COL_STORE_KEY+idx, tsv.getPointer(), tsv.getLength()))
          return false;
      }
      else {
        DEBUG_PRINT("Set key part NULL: %d ", idx);
        setKeyPartNull(COL_STORE_KEY+idx);
      }
      idx++;
    } while (tsv.advance());
  }
  else { 
    r = setKeyPart(COL_STORE_KEY, dbkey, key_len);
  }
  return r;
}


bool Operation::setFieldsInRow(int offset, const char * desc,
                               int nparts, const char *val, size_t len ) {
  bool r = true;
  
  if(nparts > 1) {
    TabSeparatedValues tsv(val, nparts, len);
    int idx = 0;
    do {
      if(tsv.getLength()) {
        DEBUG_PRINT("Set %s part %d [%.*s]", desc, idx, tsv.getLength(), tsv.getPointer());
        if(! setColumn(offset+idx, tsv.getPointer(), tsv.getLength()))
          return false;
      }
      else {
        DEBUG_PRINT("Set %s part NULL: %d ", desc, idx);
        setColumnNull(offset+idx);
      }
      idx++;
    } while (tsv.advance());
  }
  else {
    r = setColumn(offset, val, len);
  }
  return r;
}



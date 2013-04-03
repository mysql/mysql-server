/*
 Copyright (c) 2012, Oracle and/or its affiliates. All rights
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


#include <NdbApi.hpp>
#include <node.h>

#include "adapter_global.h"
#include "Operation.h"

Operation::Operation() : row_buffer(0), key_buffer(0),
                         row_record(0), key_record(0),
                         read_mask_ptr(0),
                         lmode(NdbOperation::LM_SimpleRead),
                         options(0)
{
  row_mask[3] = row_mask[2] = row_mask[1] = row_mask[0] = 0;
}


NdbTransaction * Operation::startTransaction(Ndb *db) const {
  char hash_buffer[512];
  return db->startTransaction(key_record->getNdbRecord(), key_buffer,
                              hash_buffer, 512);
}


NdbIndexScanOperation * Operation::scanIndex(NdbTransaction *tx,
                                             NdbIndexScanOperation::IndexBound *bound) {
  NdbScanOperation::ScanOptions opts;
  opts.optionsPresent = NdbScanOperation::ScanOptions::SO_SCANFLAGS;
  opts.scan_flags = NdbScanOperation::SF_OrderBy;
  
  return tx->scanIndex(key_record->getNdbRecord(),    // scan key    
                       row_record->getNdbRecord(),    // row record  
                       lmode,                         // lock mode   
                       (const unsigned char*) 0,      // result mask 
                       bound,                         // bound       
                       & opts,                            
                       sizeof(opts));
}


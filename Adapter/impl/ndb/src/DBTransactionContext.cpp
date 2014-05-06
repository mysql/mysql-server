/*
 Copyright (c) 2014, Oracle and/or its affiliates. All rights
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

#include "adapter_global.h"
#include "AsyncNdbContext.h"
#include "DBSessionImpl.h"
#include "NdbWrappers.h"
#include "DBTransactionContext.h"

extern void setJsWrapper(DBTransactionContext *);

DBTransactionContext::DBTransactionContext(DBSessionImpl *impl) :
  parent(impl),
  next(0),
  ndbTransaction(0),
  definedOperations(),
  definedScan(0),
  executedOperations(0),
  tcNodeId(0),
  opIterator(0),
  opListSize(0)
{
  setJsWrapper(this);
}

DBTransactionContext::~DBTransactionContext() {
  clear();
  jsWrapper.Dispose();
}

void DBTransactionContext::newOperationList(int size) {
  if(definedOperations) {
    delete[] definedOperations;
  }
  if(size > 0) {
    definedOperations = new KeyOperation[size];
  }
  opIterator = 0;
  opListSize = size;
}

KeyOperation * DBTransactionContext::addOperation() {
  assert(opIterator < opListSize);
  KeyOperation * op = & definedOperations[opIterator];
  opIterator++;
  return op;
}

void DBTransactionContext::defineScan(ScanOperation *scanHelper) {
  assert(opListSize == 0);
  assert(definedScan == 0);
  definedScan = scanHelper;
}


const NdbError & DBTransactionContext::getNdbError() {
  return ndbTransaction ? ndbTransaction->getNdbError() : parent->getNdbError();
}

bool DBTransactionContext::tryImmediateStartTransaction() {
  if(parent->canOpenImmediate()) {
    startTransaction();
    return true;
  }
  return false;
}

void DBTransactionContext::startTransaction() {
  assert(ndbTransaction == 0);
  KeyOperation & op = definedOperations[0];
  bool startWithHint = false;
  
  if(! definedScan) {
    assert(opListSize > 0);
    startWithHint = (op.key_buffer != 0);
  }

  if(startWithHint) {
    char hash_buffer[512];        
    ndbTransaction = 
      parent->ndb->startTransaction(op.key_record->getNdbRecord(), 
                                         op.key_buffer, hash_buffer, 512);
  } else {
    ndbTransaction = parent->ndb->startTransaction();
  }

  if(ndbTransaction) {
    tcNodeId = ndbTransaction->getConnectedNodeId();
    parent->registerTxOpen(tcNodeId);
  }  
}

NdbScanOperation * DBTransactionContext::prepareAndExecuteScan() {
  NdbScanOperation * scanop;
  if(! ndbTransaction) {
    startTransaction();  
  }
  scanop = definedScan->prepareScan();
  ndbTransaction->execute(NdbTransaction::NoCommit, NdbOperation::AO_IgnoreError, 1);
  return scanop;
}

int DBTransactionContext::execute(NdbTransaction::ExecType execType,
                                  NdbOperation::AbortOption abortOption, 
                                  int forceSend) {
  if(! ndbTransaction) {
    startTransaction();
  }
  prepareOperations();  
  return ndbTransaction->execute(execType, abortOption, forceSend);
}

int DBTransactionContext::executeAsynch(int execType, int abortOption, int forceSend, 
                                        v8::Persistent<v8::Function> callback) {
  prepareOperations();
  return parent->asyncContext->executeAsynch(ndbTransaction, execType, 
                                             abortOption, forceSend, callback);
}                    

/// TODO: prepareOperations
//  Requires API change to Operation, adding opcode

inline void DBTransactionContext::closeTransaction() {
  assert(ndbTransaction);
  ndbTransaction->close();   
  parent->registerTxClosed(tcNodeId);
  ndbTransaction = 0;
  tcNodeId = 0;
}


bool DBTransactionContext::clear() {
  /* Cannot clear if NdbTransaction is still open */
  if(ndbTransaction) return false;

  ndbTransaction = 0;
  newOperationList(0);  // free the definedOperations list
  definedScan = 0; 
  executedOperations = 0;  // js code may maintain a reference; freed via GC

  return true;
}


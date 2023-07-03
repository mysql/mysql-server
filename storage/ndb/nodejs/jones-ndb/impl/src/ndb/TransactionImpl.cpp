/*
 Copyright (c) 2014, 2022, Oracle and/or its affiliates.
 
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

#include <NdbApi.hpp>

#include "adapter_global.h"
#include "AsyncNdbContext.h"
#include "SessionImpl.h"
#include "NdbWrappers.h"
#include "TransactionImpl.h"
#include "BatchImpl.h"

extern void setJsWrapper(TransactionImpl *);
extern Local<Object> getWrappedObject(BatchImpl *set);

const char * modes[4] = { "Prepare ","NoCommit","Commit  ","Rollback" };

TransactionImpl::TransactionImpl(SessionImpl *impl, v8::Isolate *iso) :
  token(0),
  isolate(iso),
  parentSessionImpl(impl),
  next(0),
  ndbTransaction(0),
  tcNodeId(0),
  openOperationSet(0)
{
  setJsWrapper(this);
  emptyOpSet = new BatchImpl(this, 0);
  emptyOpSetWrapper.Reset(isolate, getWrappedObject(emptyOpSet));
}

TransactionImpl::~TransactionImpl() {
  DEBUG_MARKER(UDEB_DETAIL);
//  jsWrapper.Reset();
//  jsWrapper.MakeWeak();
}


const NdbError & TransactionImpl::getNdbError() {
  if(ndbTransaction) {           // transaction is open
    return ndbTransaction->getNdbError();
  } else {                       // startTransaction() failed
    return parentSessionImpl->getNdbError();
  }
}

bool TransactionImpl::tryImmediateStartTransaction(KeyOperation * op) {
  token = parentSessionImpl->registerIntentToOpen();
  if(token == -1) {
    startTransaction(op);
    return true;
  }
  return false;
}

void TransactionImpl::startTransaction(KeyOperation * op) {
  assert(ndbTransaction == 0);
  bool startWithHint = (op && op->key_buffer && op->key_record->partitionKey());

  if(startWithHint) {
    char hash_buffer[512];        
    ndbTransaction = parentSessionImpl->ndb->
      startTransaction(op->key_record->getNdbRecord(),
                       op->key_buffer, hash_buffer, 512);
  } else {
    ndbTransaction = parentSessionImpl->ndb->startTransaction();
  }

  tcNodeId = ndbTransaction ? ndbTransaction->getConnectedNodeId() : 0;
  DEBUG_PRINT("START TRANSACTION %s TC Node %d", 
              startWithHint ? "[with hint]" : "[ no hint ]", tcNodeId);
}

int TransactionImpl::prepareAndExecuteScan(ScanOperation *scan) {
  if(! ndbTransaction) {
    startTransaction(NULL);
  }
  scan->prepareScan(ndbTransaction);
  return ndbTransaction->execute(NdbTransaction::NoCommit, NdbOperation::AO_IgnoreError, 1);
}

int TransactionImpl::prepareAndExecuteQuery(QueryOperation *query) {
  if(! ndbTransaction) {
    startTransaction(NULL);
  }
  if(! query->createNdbQuery(ndbTransaction)) {
    DEBUG_PRINT("%d %s", query->getNdbError().code, query->getNdbError().message);
    return -1;
  }
  return ndbTransaction->execute(NdbTransaction::NoCommit, NdbOperation::AO_IgnoreError, 1);
}

void TransactionImpl::closeTransaction() {
  openOperationSet->saveNdbErrors();
  ndbTransaction->close();
}

void TransactionImpl::registerClose() {
  ndbTransaction = 0;
  openOperationSet->transactionIsClosed();
  parentSessionImpl->registerTxClosed(token, tcNodeId);
}

int TransactionImpl::execute(BatchImpl *operations, 
                             int _execType, int _abortOption, int force) {
  int rval;
  int opListSize = operations->size;
  openOperationSet = operations;
  NdbTransaction::ExecType execType = static_cast<NdbTransaction::ExecType>(_execType);
  NdbOperation::AbortOption abortOption = static_cast<NdbOperation::AbortOption>(_abortOption);
  bool doClose = (execType != NdbTransaction::NoCommit);
  
  if(! ndbTransaction) {
    startTransaction(operations->getKeyOperation(0));
  }
  operations->prepare(ndbTransaction);

  if(operations->hasBlobReadOperations()) {
    ndbTransaction->execute(NdbTransaction::NoCommit);
    DEBUG_PRINT("BLOB EXECUTE DONE");
  }

  rval = ndbTransaction->execute(execType, abortOption, force);
  DEBUG_PRINT("EXECUTE sync : %s %d operation%s %s => return: %d error: %d",
              modes[execType], 
              opListSize, 
              (opListSize == 1 ? "" : "s"), 
              (doClose ? " & close transaction" : ""),
              rval,
              ndbTransaction->getNdbError().code);
  if(doClose) {
    closeTransaction();
    operations->transactionIsClosed();
  }
  return rval;
}

int TransactionImpl::executeAsynch(BatchImpl *operations,  
                                   int execType, int abortOption, int forceSend,
                                   v8::Local<v8::Function> callback) {
  assert(ndbTransaction);
  operations->prepare(ndbTransaction);
  openOperationSet = operations;
  int opListSize = operations->size;
  DEBUG_PRINT("EXECUTE async: %s %d operation%s", modes[execType], 
              opListSize, (opListSize == 1 ? "" : "s"));
  return parentSessionImpl->asyncContext->
    executeAsynch(this, ndbTransaction, execType, abortOption, forceSend,callback);
}                    

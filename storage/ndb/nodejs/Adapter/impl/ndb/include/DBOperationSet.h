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

#ifndef nodejs_adapter_DBOperationSet_h
#define nodejs_adapter_DBOperationSet_h

#include "KeyOperation.h"
#include "DBTransactionContext.h"
#include "BlobHandler.h"

class DBOperationSet {
friend class DBTransactionContext;
public:
  DBOperationSet(DBTransactionContext *, int size);
  ~DBOperationSet();
  void setError(int n, const NdbError &);
  const NdbError * getError(int n);
  KeyOperation * getKeyOperation(int n);
  bool tryImmediateStartTransaction();
  int execute(int execType, int abortOption, int forceSend);
  int executeAsynch(int execType, int abortOption, int forceSend,
                    v8::Persistent<v8::Function> execCompleteCallback);
  void prepare(NdbTransaction *);
  const NdbError & getNdbError();
  void registerClosedTransaction();
  BlobHandler * getBlobHandler(int);
  bool hasBlobReadOperations();

private:
  KeyOperation * keyOperations;
  const NdbOperation ** const ops;
  const NdbError ** const errors;
  int size;
  bool doesReadBlobs;
  DBTransactionContext *txContext;
};

inline DBOperationSet::DBOperationSet(DBTransactionContext * ctx, int _sz) :
  keyOperations(new KeyOperation[_sz]),
  ops(new const NdbOperation *[_sz]),
  errors(new const NdbError *[_sz]),
  size(_sz),
  doesReadBlobs(false),
  txContext(ctx)                        {};

inline void DBOperationSet::setError(int n, const NdbError & err) {
  errors[n] = & err;
  ops[n] = NULL;
}

inline const NdbError * DBOperationSet::getError(int n) {
  if(size > n) {
    return (ops[n] ? & ops[n]->getNdbError() : errors[n]);
  }
  return 0;
}

inline KeyOperation * DBOperationSet::getKeyOperation(int n) {
  return & keyOperations[n];
}

inline int DBOperationSet::execute(int execType, int abortOption, int forceSend) {
  return txContext->execute(this, execType, abortOption, forceSend);
}

inline int DBOperationSet::executeAsynch(int execType, int abortOption, int forceSend,
                                   v8::Persistent<v8::Function> callback) {
  return txContext->executeAsynch(this, execType, abortOption, forceSend, callback);
}

inline const NdbError & DBOperationSet::getNdbError() {
  return txContext->getNdbError();
}

inline void DBOperationSet::registerClosedTransaction() {
  txContext->registerClose();
}

inline BlobHandler * DBOperationSet::getBlobHandler(int n) {
  return keyOperations[n].blobHandler;
}

inline bool DBOperationSet::hasBlobReadOperations() {
  return doesReadBlobs;
}

#endif

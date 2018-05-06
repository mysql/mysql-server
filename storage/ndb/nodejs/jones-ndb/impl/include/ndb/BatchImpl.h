/*
 Copyright (c) 2013, 2016, Oracle and/or its affiliates. All rights reserved.
 
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

#ifndef nodejs_adapter_BatchImpl_h
#define nodejs_adapter_BatchImpl_h

#include "KeyOperation.h"
#include "TransactionImpl.h"
#include "BlobHandler.h"

class BatchImpl {
friend class TransactionImpl;
public:
  BatchImpl(TransactionImpl *, int size);
  ~BatchImpl();
  const NdbError * getError(int n);
  KeyOperation * getKeyOperation(int n);
  bool tryImmediateStartTransaction();
  int execute(int execType, int abortOption, int forceSend);
  int executeAsynch(int execType, int abortOption, int forceSend,
                    v8::Handle<v8::Function> execCompleteCallback);
  const NdbError & getNdbError();   // get NdbError from TransactionImpl
  void registerClosedTransaction();

protected:
  void prepare(NdbTransaction *);
  void saveNdbErrors();
  BlobHandler * getBlobHandler(int);
  bool hasBlobReadOperations();
  void setOperationNdbError(int, const NdbError &);
  void transactionIsClosed();

private:
  KeyOperation * keyOperations;
  const NdbOperation ** const ops;
  NdbError * const errors;
  int size;
  bool doesReadBlobs;
  TransactionImpl *transactionImpl;
  NdbError * transactionNdbError;
};

inline KeyOperation * BatchImpl::getKeyOperation(int n) {
  return & keyOperations[n];
}

inline int BatchImpl::execute(int execType, int abortOption, int forceSend) {
  return transactionImpl->execute(this, execType, abortOption, forceSend);
}

inline int BatchImpl::executeAsynch(int execType, int abortOption, int forceSend,
                                   v8::Handle<v8::Function> callback) {
  return transactionImpl->executeAsynch(this, execType, abortOption, forceSend, callback);
}

inline void BatchImpl::registerClosedTransaction() {
  transactionImpl->registerClose();
}

inline BlobHandler * BatchImpl::getBlobHandler(int n) {
  return keyOperations[n].blobHandler;
}

inline bool BatchImpl::hasBlobReadOperations() {
  return doesReadBlobs;
}

#endif

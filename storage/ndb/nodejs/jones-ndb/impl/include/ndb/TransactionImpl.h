/*
 Copyright (c) 2014, 2024, Oracle and/or its affiliates.

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License, version 2.0,
 as published by the Free Software Foundation.

 This program is designed to work with certain software (including
 but not limited to OpenSSL) that is licensed under separate terms,
 as designated in a particular file or component or in included license
 documentation.  The authors of MySQL hereby grant you an additional
 permission to link the program and your derivative works with the
 separately licensed software that they have either included with
 the program or referenced in the documentation.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License, version 2.0, for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef NODEJS_ADAPTER_INCLUDE_DBTRANSACTIONCONTEXT_H
#define NODEJS_ADAPTER_INCLUDE_DBTRANSACTIONCONTEXT_H

#include "QueryOperation.h"
#include "ScanOperation.h"

class SessionImpl;
class BatchImpl;

/* TransactionImpl takes the place of NdbTransaction,
   allowing operations to be declared before an NdbTransaction is open,
   and consolidating open, execute+commit, and close into a single async
   call.
*/

class TransactionImpl {
 public:
  /* The object holds a persistent reference to the JavaScript object
     which serves as its wrapper.  This allows the TransactionImpl
     to be reused in JavaScript many times without creating a new wrapper
     each time.
  */
  v8::Local<v8::Object> getJsWrapper() const { return jsWrapper.Get(isolate); }

  /****** Executing Operations *******/

  int prepareAndExecuteScan(ScanOperation *);

  int prepareAndExecuteQuery(QueryOperation *);

  /* If it is possible to open the NdbTransaction without blocking, do so,
     and return true.  Otherwise return false.  This can be used as a
     conditional barrier to choose executeAsynch() over execute().
  */
  bool tryImmediateStartTransaction(KeyOperation *);

  /* Async open in worker thread.
   */
  void startTransaction(KeyOperation *);

  /* Execute transaction using synchronous NDB API in a worker thread.
     If an NdbTransaction is not yet open, one will be started, using
     the table and key of the first defined primary key operation as a hint.
     Any pending operations will be run.
     If execType is COMMIT or ROLLBACK, the NdbTransaction will be closed.

     The JavaScript wrapper for this function is Async.
     execute() runs in a uv worker thread.
  */
  int execute(BatchImpl *operations, int execType, int abortOption,
              int forceSend);

  /* Execute transaction and key-operations using asynchronous NDB API.
     This runs immediately.  The transaction must have already been started.
     executeAsynch() runs in the JS main thread.
  */
  int executeAsynch(BatchImpl *operations, int execType, int abortOption,
                    int forceSend,
                    v8::Local<v8::Function> execCompleteCallback);

  /* Close the NDB Transaction.  This could happen in a worker thread.
   */
  void closeTransaction();

  /* Inform TransactionImpl that NdbTransaction has been clsoed.
     This always happens in the JS main thread.
  */
  void registerClose();

  /* Fetch an empty BatchImpl that can be used for stand-alone
     COMMIT and ROLLBACK calls.
  */
  v8::Local<v8::Object> getWrappedEmptyOperationSet() const {
    return emptyOpSetWrapper.Get(isolate);
  }

  /****** Accessing operation errors *******/

  /* Get NDB error on NdbTransaction (if defined)
     Otherwise get NDB error on Ndb.
  */
  const NdbError &getNdbError();

 protected:
  friend class SessionImpl;
  friend void setJsWrapper(TransactionImpl *);
  friend class AsyncExecCall;

  /* Protected constructor & destructor are used by SessionImpl */
  TransactionImpl(SessionImpl *, v8::Isolate *);
  ~TransactionImpl();

  /* Reset state for next user.
     Returns true on success.
     Returns false if the current state does not allow clearing, (e.g. due to
     internal NdbTransaction in open state).
  */
  bool isClosed() const;

 private:
  int64_t token;
  v8::Persistent<v8::Object> jsWrapper;
  v8::Persistent<v8::Object> emptyOpSetWrapper;
  v8::Isolate *isolate;
  BatchImpl *emptyOpSet;
  SessionImpl *const parentSessionImpl;
  TransactionImpl *next;
  NdbTransaction *ndbTransaction;
  int tcNodeId;
  BatchImpl *openOperationSet;
};

inline bool TransactionImpl::isClosed() const { return !(bool)ndbTransaction; }

#endif

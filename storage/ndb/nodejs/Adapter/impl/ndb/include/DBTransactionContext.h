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

#ifndef NODEJS_ADAPTER_INCLUDE_DBTRANSACTIONCONTEXT_H
#define NODEJS_ADAPTER_INCLUDE_DBTRANSACTIONCONTEXT_H

#include "ScanOperation.h"

class DBSessionImpl;
class DBOperationSet;

/* DBTransactionContext takes the place of NdbTransaction, 
   allowing operations to be declared before an NdbTransaction is open, 
   and consolidating open, execute+commit, and close into a single async
   call.
*/

class DBTransactionContext {
public:

  /* The object holds a persistent reference to the JavaScript object 
     which serves as its wrapper.  This allows the DBTransactionContext 
     to be reused in JavaScript many times without creating a new wrapper
     each time.
  */
  v8::Handle<v8::Value> getJsWrapper() const;


  /****** Executing Operations *******/

  int prepareAndExecuteScan(ScanOperation *);

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
  int execute(DBOperationSet *operations,
              int execType, int abortOption, int forceSend);

  /* Execute transaction and key-operations using asynchronous NDB API.
     This runs immediately.  The transaction must have already been started.     
     executeAsynch() runs in the JS main thread.     
  */
  int executeAsynch(DBOperationSet *operations,
                    int execType, int abortOption, int forceSend,
                    v8::Persistent<v8::Function> execCompleteCallback);

  /* Close the NDB Transaction.  This could happen in a worker thread.
  */
  void closeTransaction();  

  /* Inform DBTransactionContext that NdbTransaction has been clsoed.
     This always happens in the JS main thread.
  */
  void registerClose();

  /* Fetch an empty DBOperationSet that can be used for stand-alone 
     COMMIT and ROLLBACK calls.
  */
  v8::Handle<v8::Value> getWrappedEmptyOperationSet() const;


  /****** Accessing operation errors *******/

  /* Get NDB error on NdbTransaction (if defined)
     Otherwise get NDB error on Ndb.
  */
  const NdbError & getNdbError();

protected:  
  friend class DBSessionImpl;
  friend void setJsWrapper(DBTransactionContext *);
  friend class AsyncExecCall;

  /* Protected constructor & destructor are used by DBSessionImpl */
  DBTransactionContext(DBSessionImpl *);
  ~DBTransactionContext();
  
  /* Reset state for next user.
     Returns true on success. 
     Returns false if the current state does not allow clearing, (e.g. due to 
     internal NdbTransaction in open state).  
  */
  bool isClosed() const;

private: 
  int64_t                   token;
  v8::Persistent<v8::Value> jsWrapper;
  v8::Persistent<v8::Value> emptyOpSetWrapper;
  DBOperationSet *          emptyOpSet;  
  DBSessionImpl * const     parent;
  DBTransactionContext *    next;
  NdbTransaction *          ndbTransaction; 
  int                       tcNodeId;
};

inline v8::Handle<v8::Value> DBTransactionContext::getJsWrapper() const {
  return jsWrapper;
}

inline v8::Handle<v8::Value> DBTransactionContext::getWrappedEmptyOperationSet() const {
  return emptyOpSetWrapper;
}

inline bool DBTransactionContext::isClosed() const {
  return ! (bool) ndbTransaction;
}

#endif

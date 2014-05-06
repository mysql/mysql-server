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
class PendingOperationSet;

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


  /****** Preparing Operations *******/

  /* Declare the size of the next list of operations to be defined.
     DBTransactionContext will allocate an array of <size> operations.
  */
  void newOperationList(int size); 

  /* Add an operation to a transaction context. 
     After calling, the user will set features in the KeyOperation object.
     At execute() time, DBSessionImpl will obtain and execute an NdbOperation
     for each valid KeyOperation, then free the whole array. 
  */
  KeyOperation * getNextOperation();
  
  /*  Define a scan operation in this transaction context.
      The caller has already created the DBScanHelper describing the scan.
  */
  void defineScan(ScanOperation * scanHelper);


  /****** Executing Operations *******/

  /*  Execute the defined scan. 
      This call: 
      (1) Prepares the scan operation.
      (2) Runs Execute + NoCommit so that the user can start reading results.

      The async wrapper for this call will getNdbError() on the NdbTransaction;
      after a TimeoutExpired error, the call can be run again to retry.
      
      The JavaScript wrapper for this function is Async.
  */  
  NdbScanOperation * prepareAndExecuteScan();

  /* If it is possible to open the NdbTransaction without blocking, do so,
     and return true.  Otherwise return false.  This can be used as a 
     conditional barrier to choose executeAsynch() over execute().
  */
  bool tryImmediateStartTransaction();

  /* Async open in worker thread.
  */
  void startTransaction();

  /* Execute transaction using synchronous NDB API in a worker thread.
     If an NdbTransaction is not yet open, one will be started, using 
     the table and key of the first defined primary key operation as a hint.
     Any pending operations will be run.
     If execType is COMMIT or ROLLBACK, the NdbTransaction will be closed.
     
     The JavaScript wrapper for this function is Async.
     execute() runs in a uv worker thread.
  */
  int execute( NdbTransaction::ExecType execType,
               NdbOperation::AbortOption abortOption, 
               int forceSend);

  /* Execute transaction and key-operations using asynchronous NDB API.
     This runs immediately.  The transaction must have already been started.     
     executeAsynch() runs in the JS main thread.     
  */
  int executeAsynch(int execType, int abortOption, int forceSend,
                     v8::Persistent<v8::Function> execCompleteCallback);


  /****** Accessing operation errors *******/

  /* Get pending operations.  
     This returns a PendingOperationSet for the most recently executed
     set of operations, which can then be used to access individual operation 
     errors as neded.
  */
  PendingOperationSet * getPendingOperations() const;

  /* Get NDB error on NdbTransaction (if defined)
     Otherwise get NDB error on Ndb.
  */
  const NdbError & getNdbError();

protected:  
  friend class DBSessionImpl;
  friend void setJsWrapper(DBTransactionContext *);

  /* Protected constructor & destructor are used by DBSessionImpl */
  DBTransactionContext(DBSessionImpl *);
  ~DBTransactionContext();

  /* Methods called internally */
  void prepareOperations();
  void closeTransaction();

  /* Reset state for next user.
     Returns true on success. 
     Returns false if the current state does not allow clearing, (e.g. due to 
     internal NdbTransaction in open state).  
  */
  bool clear();

private: 
  v8::Persistent<v8::Value> jsWrapper;
  DBSessionImpl * const     parent;
  DBTransactionContext *    next;
  NdbTransaction *          ndbTransaction; 
  KeyOperation *            definedOperations; 
  ScanOperation *           definedScan;
  PendingOperationSet *     executedOperations;
  int                       tcNodeId;
  int                       opIterator;
  int                       opListSize;
};

inline v8::Handle<v8::Value> DBTransactionContext::getJsWrapper() const {
  return jsWrapper;
}

#endif

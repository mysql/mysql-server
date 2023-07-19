/*
 Copyright (c) 2014, 2023, Oracle and/or its affiliates.
 
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

#ifndef NODEJS_ADAPTER_INCLUDE_DBSESSIONIMPL_H
#define NODEJS_ADAPTER_INCLUDE_DBSESSIONIMPL_H

/* 
  SessionImpl maintains an Ndb and a set of TransactionImpl objects. 
  
*/ 

#include "stdint.h"

class TransactionImpl;
class AsyncNdbContext;
class Ndb;
class Ndb_cluster_connection;
struct NdbError;
namespace v8 { class Isolate; }

class CachedTransactionsAccountant {
protected:
  friend class TransactionImpl;
  
  CachedTransactionsAccountant(Ndb_cluster_connection *, int maxTransactions);
  ~CachedTransactionsAccountant();
 
  /* Calling sequence in to CachedTransactionsAccountant:
     Call registerIntentToOpen() before opening a transaction.
     Based on return value:
       -1    - start transaction immediate
       other - start transcation async 
     Store return value as token. 
     After open, call NdbTransaction::getConnectedNodeId() to fetch node id.
     After close of transaction, call registerTxClosed with token and node id.
  */

  /* registerIntentToOpen() decrements all non-zero counters. 
     Its return value is a bitmap token indicating which counters were decremented.
     The special value of -1 indicates that all counters were non-zero, and 
     that therefore immediate (synchronous) startTransaction() is allowed.
      
     In other words: if it is known that there is a cached API Connect Record 
     for each data node, then startTransaction() is guaranteed not to block 
     no matter which TC is selected, so it can be called from the main thread.
  */
  int64_t registerIntentToOpen();
  void registerTxClosed(int64_t token, int nodeId);

private:
  /* Methods */
  void  tallySetNodeId(int);
  void  tallySetMaskedNodeIds(int64_t);
  void  tallyClear();
  int   tallyCountSetNodeIds();

  /* Data Members */
  uint64_t tc_bitmap;
  unsigned short nDataNodes, concurrency, cacheConcurrency, maxConcurrency;
};


class SessionImpl : public CachedTransactionsAccountant {
public: 

  /* Constructor.
  */
  SessionImpl(Ndb_cluster_connection *conn, 
                AsyncNdbContext * asyncNdbContext,
                const char *defaultDatabase,
                int maxTransactions);
    
  /* Public destructor.
     SessionImpl owns an Ndb object, which will be closed. 
  */
  ~SessionImpl();
  
  /* This replaces Ndb::startTransaction().
     Returns a TransactionImpl, or null if none are available.
     If null, the caller should queue the request and retry it after
     releasing a TransactionImpl.
  */
  TransactionImpl * seizeTransaction(v8::Isolate *);

  /* Release a previously seized transaction. 
     Returns 0 on success.
     Returns -1 if the transaction's current state does not allow it to be 
     released; the caller must execute (COMMIT or ROLLBACK) before releasing.
  */
  bool releaseTransaction(TransactionImpl *);

  /* Free all TransactionImpls.
     This must be done in the main thread.
  */
  void freeTransactions();

  /* Replaces Ndb::getNdbError().
  */
  const NdbError & getNdbError() const;

private:  
  friend class TransactionImpl;
  friend class ListTablesCall;
  friend class GetTableCall;
  friend class QueryOperation;

  int maxNdbTransactions;
  int nContexts;
  Ndb *ndb;
  AsyncNdbContext * asyncContext;
  TransactionImpl * freeList;
};


#endif


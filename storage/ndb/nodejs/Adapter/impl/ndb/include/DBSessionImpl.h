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

#ifndef NODEJS_ADAPTER_INCLUDE_DBSESSIONIMPL_H
#define NODEJS_ADAPTER_INCLUDE_DBSESSIONIMPL_H

/* 
  DBSessionImpl takes the place of Ndb. 
  It maintains an Ndb and a set of DBTransactionContext objects. 
*/ 

class DBTransactionContext;
class AsyncNdbContext;


class CachedTransactionsAccountant {
protected:
  friend class DBTransactionContext;
  
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
  short nDataNodes;
  short concurrency;
  short cacheConcurrency;
  short maxConcurrency;
};


class DBSessionImpl : public CachedTransactionsAccountant {
public: 

  /* Constructor.
  */
  DBSessionImpl(Ndb_cluster_connection *conn, 
                AsyncNdbContext * asyncNdbContext,
                const char *defaultDatabase,
                int maxTransactions);
    
  /* Public destructor.
     DBSessionImpl owns an Ndb object, which will be closed. 
  */
  ~DBSessionImpl();
  
  /* This replaces Ndb::startTransaction().
     Returns a DBTransactionContext, or null if none are available.
     If null, the caller should queue the request and retry it after
     releasing a DBTransactionContext.
  */
  DBTransactionContext * seizeTransaction();

  /* Release a previously seized transaction. 
     Returns 0 on success.
     Returns -1 if the transaction's current state does not allow it to be 
     released; the caller must execute (COMMIT or ROLLBACK) before releasing.
  */
  bool releaseTransaction(DBTransactionContext *);

  /* Free all DBTransactionContexts.
     This must be done in the main thread.
  */
  void freeTransactions();

  /* Replaces Ndb::getNdbError().
  */
  const NdbError & getNdbError() const;
  
private:  
  friend class DBTransactionContext;
  friend class ListTablesCall;
  friend class GetTableCall;

  int maxNdbTransactions;
  int nContexts;
  Ndb *ndb;
  AsyncNdbContext * asyncContext;
  DBTransactionContext * freeList;
};


#endif


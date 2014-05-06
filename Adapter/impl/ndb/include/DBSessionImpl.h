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
  
  CachedTransactionsAccountant(Ndb_cluster_connection *);
  ~CachedTransactionsAccountant();

  /* Returns true if the number of non-zero elements in the 
     cachedTransactionsPerTC is equal to the number of data nodes.
     In other words: if it is known that there is a cached API Connect Record 
     for each data node, then startTransaction() is guaranteed not to block 
     no matter which TC is selected.     
  */
  bool canOpenImmediate();

  /* Bookkeeping methods for the cachedTransactionsPerTC array. 
     If code in the JS main thread knows the connected node id, it can use the
     specific version of the call.
     These return 0 if nodeId is a valid data node id, otherwise -1.
  */   
  int registerTxOpen(int nodeId);               // decrement if non-zero
  int registerTxClosed(int nodeId);             // increment 

private:
  int nDataNodes;
  int nNonZeroTallies;
  class TcTally { 
  public:
    TcTally() : nodeId(0), txCount(0) {};
    uint8_t nodeId;
    uint8_t txCount;
  };
  TcTally cachedTransactionsPerTC[48];
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
  int releaseTransaction(DBTransactionContext *);

  /* Replaces Ndb::getNdbError().
  */
  const NdbError & getNdbError() const;
  
  friend class DBTransactionContext;

private:  
  int maxNdbTransactions;
  int nContexts;
  Ndb *ndb;
  AsyncNdbContext * asyncContext;
  DBTransactionContext * freeList;
};


#endif


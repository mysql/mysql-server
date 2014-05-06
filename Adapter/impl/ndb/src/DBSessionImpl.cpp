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
#include "KeyOperation.h"
#include "DBSessionImpl.h"
#include "DBTransactionContext.h"

//////////
/////////////////
///////////////////////// CachedTransactionsAccountant
/////////////////
//////////


CachedTransactionsAccountant::CachedTransactionsAccountant(Ndb_cluster_connection *conn) :
  nDataNodes(conn->no_db_nodes()),
  nNonZeroTallies(0),
  cachedTransactionsPerTC()
{
  assert(nDataNodes > 0);
}


int CachedTransactionsAccountant::registerTxOpen(int nodeId) {
  for(int i = 0 ; i < nDataNodes ; i++) {
    TcTally & tally = cachedTransactionsPerTC[i];

    if(tally.nodeId == nodeId) {
      if(tally.txCount > 0) {
        tally.txCount--;
        if(tally.txCount == 0) {
          nNonZeroTallies--;
          assert(nNonZeroTallies >= 0);
        }
      }    
      return 0;
    }
    if(tally.nodeId == 0) {  // define this data node at end of list 
      tally.nodeId = nodeId;
      return 0;
    } 
  }
  return -1;
}


int CachedTransactionsAccountant::registerTxClosed(int nodeId) {
  for(int i = 0 ; i < nDataNodes ; i++) {
    TcTally & tally = cachedTransactionsPerTC[i];
    if(tally.nodeId == nodeId) {
      tally.txCount++;
      if(tally.txCount == 1) {
        nNonZeroTallies++;
        assert(nNonZeroTallies <= nDataNodes);
      }
      return 0;
    }
  }
  return -1;
}


bool CachedTransactionsAccountant::canOpenImmediate() {
  return (nNonZeroTallies == nDataNodes);
}



//////////
/////////////////
///////////////////////// DBSessionImpl
/////////////////
//////////

DBSessionImpl::DBSessionImpl(Ndb_cluster_connection *conn, 
                             AsyncNdbContext * asyncNdbContext,
                             const char *defaultDatabase,
                             int maxTransactions) :
  CachedTransactionsAccountant(conn),
  maxNdbTransactions(maxTransactions),
  nContexts(0),
  asyncContext(asyncNdbContext),
  freeList(0)
{
  ndb = new Ndb(conn, defaultDatabase);
  ndb->init(maxTransactions * 2);
}


DBSessionImpl::~DBSessionImpl() {
  delete ndb;
  while(freeList) {
    DBTransactionContext * ctx = freeList;
    freeList = ctx->next;
    delete ctx;
  }
}


DBTransactionContext * DBSessionImpl::seizeTransaction() {
  DBTransactionContext * ctx;
  
  /* Is there a context on the freelist? */
  if(freeList) {
    ctx = freeList;
    freeList = ctx->next;
    return ctx;
  }

  /* Can we produce a new context? */
  if(nContexts < maxNdbTransactions) {
    ctx = new DBTransactionContext(this);
    nContexts++;
    return ctx;  
  }

  return 0;
}


int DBSessionImpl::releaseTransaction(DBTransactionContext *ctx) {
  assert(ctx->parent == this);
  int status = ctx->clear();
  if(status == 0) {
    ctx->next = freeList;
    freeList = ctx;
  }
  return status;
}


const NdbError & DBSessionImpl::getNdbError() const {
  return ndb->getNdbError();
}


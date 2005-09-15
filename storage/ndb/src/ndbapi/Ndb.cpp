/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */




/*****************************************************************************
Name:          Ndb.cpp
******************************************************************************/

#include <ndb_global.h>


#include "NdbApiSignal.hpp"
#include "NdbImpl.hpp"
#include <NdbOperation.hpp>
#include <NdbTransaction.hpp>
#include <NdbEventOperation.hpp>
#include <NdbEventOperationImpl.hpp>
#include <NdbRecAttr.hpp>
#include <md5_hash.hpp>
#include <NdbSleep.h>
#include <NdbOut.hpp>
#include <ndb_limits.h>
#include "API.hpp"
#include <NdbEnv.h>
#include <BaseString.hpp>

/****************************************************************************
void connect();

Connect to any node which has no connection at the moment.
****************************************************************************/
NdbTransaction* Ndb::doConnect(Uint32 tConNode) 
{
  Uint32        tNode;
  Uint32        tAnyAlive = 0;
  int TretCode= 0;

  DBUG_ENTER("Ndb::doConnect");

  if (tConNode != 0) {
    TretCode = NDB_connect(tConNode);
    if ((TretCode == 1) || (TretCode == 2)) {
//****************************************************************************
// We have connections now to the desired node. Return
//****************************************************************************
      DBUG_RETURN(getConnectedNdbTransaction(tConNode));
    } else if (TretCode != 0) {
      tAnyAlive = 1;
    }//if
  }//if
//****************************************************************************
// We will connect to any node. Make sure that we have connections to all
// nodes.
//****************************************************************************
  if (theImpl->m_optimized_node_selection)
  {
    Ndb_cluster_connection_node_iter &node_iter= 
      theImpl->m_node_iter;
    theImpl->m_ndb_cluster_connection.init_get_next_node(node_iter);
    while ((tNode= theImpl->m_ndb_cluster_connection.get_next_node(node_iter)))
    {
      TretCode= NDB_connect(tNode);
      if ((TretCode == 1) ||
	  (TretCode == 2))
      {
//****************************************************************************
// We have connections now to the desired node. Return
//****************************************************************************
	DBUG_RETURN(getConnectedNdbTransaction(tNode));
      } else if (TretCode != 0) {
	tAnyAlive= 1;
      }//if
      DBUG_PRINT("info",("tried node %d, TretCode %d, error code %d, %s",
			 tNode, TretCode, getNdbError().code,
			 getNdbError().message));
    }
  }
  else // just do a regular round robin
  {
    Uint32 tNoOfDbNodes= theImpl->theNoOfDBnodes;
    Uint32 &theCurrentConnectIndex= theImpl->theCurrentConnectIndex;
    UintR Tcount = 0;
    do {
      theCurrentConnectIndex++;
      if (theCurrentConnectIndex >= tNoOfDbNodes)
	theCurrentConnectIndex = 0;

      Tcount++;
      tNode= theImpl->theDBnodes[theCurrentConnectIndex];
      TretCode= NDB_connect(tNode);
      if ((TretCode == 1) ||
	  (TretCode == 2))
      {
//****************************************************************************
// We have connections now to the desired node. Return
//****************************************************************************
	DBUG_RETURN(getConnectedNdbTransaction(tNode));
      } else if (TretCode != 0) {
	tAnyAlive= 1;
      }//if
      DBUG_PRINT("info",("tried node %d TretCode %d", tNode, TretCode));
    } while (Tcount < tNoOfDbNodes);
  }
//****************************************************************************
// We were unable to find a free connection. If no node alive we will report
// error code for cluster failure otherwise connection failure.
//****************************************************************************
  if (tAnyAlive == 1) {
#ifdef VM_TRACE
    ndbout << "TretCode = " << TretCode << endl;
#endif
    theError.code = 4006;
  } else {
    theError.code = 4009;
  }//if
  DBUG_RETURN(NULL);
}

int 
Ndb::NDB_connect(Uint32 tNode) 
{
//****************************************************************************
// We will perform seize of a transaction record in DBTC in the specified node.
//***************************************************************************
  
  int	         tReturnCode;
  TransporterFacade *tp = TransporterFacade::instance();

  DBUG_ENTER("Ndb::NDB_connect");

  bool nodeAvail = tp->get_node_alive(tNode);
  if(nodeAvail == false){
    DBUG_RETURN(0);
  }
  
  NdbTransaction * tConArray = theConnectionArray[tNode];
  if (tConArray != NULL) {
    DBUG_RETURN(2);
  }
  
  NdbTransaction * tNdbCon = getNdbCon();	// Get free connection object.
  if (tNdbCon == NULL) {
    DBUG_RETURN(4);
  }//if
  NdbApiSignal*	tSignal = getSignal();		// Get signal object
  if (tSignal == NULL) {
    releaseNdbCon(tNdbCon);
    DBUG_RETURN(4);
  }//if
  if (tSignal->setSignal(GSN_TCSEIZEREQ) == -1) {
    releaseNdbCon(tNdbCon);
    releaseSignal(tSignal);
    DBUG_RETURN(4);
  }//if
  tSignal->setData(tNdbCon->ptr2int(), 1);
//************************************************
// Set connection pointer as NdbTransaction object
//************************************************
  tSignal->setData(theMyRef, 2);	// Set my block reference
  tNdbCon->Status(NdbTransaction::Connecting); // Set status to connecting
  Uint32 nodeSequence;
  tReturnCode= sendRecSignal(tNode, WAIT_TC_SEIZE, tSignal,
                             0, &nodeSequence);
  releaseSignal(tSignal); 
  if ((tReturnCode == 0) && (tNdbCon->Status() == NdbTransaction::Connected)) {
    //************************************************
    // Send and receive was successful
    //************************************************
    NdbTransaction* tPrevFirst = theConnectionArray[tNode];
    tNdbCon->setConnectedNodeId(tNode, nodeSequence);
    
    tNdbCon->setMyBlockReference(theMyRef);
    theConnectionArray[tNode] = tNdbCon;
    tNdbCon->theNext = tPrevFirst;
    DBUG_RETURN(1);
  } else {
    releaseNdbCon(tNdbCon);
//****************************************************************************
// Unsuccessful connect is indicated by 3.
//****************************************************************************
    DBUG_PRINT("info",
	       ("unsuccessful connect tReturnCode %d, tNdbCon->Status() %d",
		tReturnCode, tNdbCon->Status()));
    DBUG_RETURN(3);
  }//if
}//Ndb::NDB_connect()

NdbTransaction *
Ndb::getConnectedNdbTransaction(Uint32 nodeId){
  NdbTransaction* next = theConnectionArray[nodeId];
  theConnectionArray[nodeId] = next->theNext;
  next->theNext = NULL;

  return next;
}//Ndb::getConnectedNdbTransaction()

/*****************************************************************************
disconnect();

Remark:        Disconnect all connections to the database. 
*****************************************************************************/
void 
Ndb::doDisconnect()
{
  DBUG_ENTER("Ndb::doDisconnect");
  NdbTransaction* tNdbCon;
  CHECK_STATUS_MACRO_VOID;

  Uint32 tNoOfDbNodes = theImpl->theNoOfDBnodes;
  Uint8 *theDBnodes= theImpl->theDBnodes;
  DBUG_PRINT("info", ("theNoOfDBnodes=%d", tNoOfDbNodes));
  UintR i;
  for (i = 0; i < tNoOfDbNodes; i++) {
    Uint32 tNode = theDBnodes[i];
    tNdbCon = theConnectionArray[tNode];
    while (tNdbCon != NULL) {
      NdbTransaction* tmpNdbCon = tNdbCon;
      tNdbCon = tNdbCon->theNext;
      releaseConnectToNdb(tmpNdbCon);
    }//while
  }//for
  tNdbCon = theTransactionList;
  while (tNdbCon != NULL) {
    NdbTransaction* tmpNdbCon = tNdbCon;
    tNdbCon = tNdbCon->theNext;
    releaseConnectToNdb(tmpNdbCon);
  }//while
  DBUG_VOID_RETURN;
}//Ndb::disconnect()

/*****************************************************************************
int waitUntilReady(int timeout);

Return Value:   Returns 0 if the Ndb is ready within timeout seconds.
                Returns -1 otherwise.
Remark:         Waits until a node has status != 0
*****************************************************************************/ 
int
Ndb::waitUntilReady(int timeout)
{
  DBUG_ENTER("Ndb::waitUntilReady");
  int secondsCounter = 0;
  int milliCounter = 0;
  int noChecksSinceFirstAliveFound = 0;
  int id;

  if (theInitState != Initialised) {
    // Ndb::init is not called
    theError.code = 4256;
    DBUG_RETURN(-1);
  }

  while (theNode == 0) {
    if (secondsCounter >= timeout)
    {
      theError.code = 4269;
      DBUG_RETURN(-1);
    }
    NdbSleep_MilliSleep(100);
    milliCounter += 100;
    if (milliCounter >= 1000) {
      secondsCounter++;
      milliCounter = 0;
    }//if
  }

  if (theImpl->m_ndb_cluster_connection.wait_until_ready
      (timeout-secondsCounter,30) < 0)
  {
    theError.code = 4009;
    DBUG_RETURN(-1);
  }

  DBUG_RETURN(0);
}

/*****************************************************************************
NdbTransaction* startTransaction();

Return Value:   Returns a pointer to a connection object.
                Return NULL otherwise.
Remark:         Start transaction. Synchronous.
*****************************************************************************/ 
NdbTransaction* 
Ndb::startTransaction(const NdbDictionary::Table *table,
		      const char * keyData, Uint32 keyLen)
{
  DBUG_ENTER("Ndb::startTransaction");

  if (theInitState == Initialised) {
    theError.code = 0;
    checkFailedNode();
    /**
     * If the user supplied key data
     * We will make a qualified quess to which node is the primary for the
     * the fragment and contact that node
     */
    Uint32 nodeId;
    NdbTableImpl* impl;
    if(table != 0 && keyData != 0 && (impl= &NdbTableImpl::getImpl(*table))) 
    {
      Uint32 hashValue;
      {
	Uint32 buf[4];
	if((UintPtr(keyData) & 7) == 0 && (keyLen & 3) == 0)
	{
	  md5_hash(buf, (const Uint64*)keyData, keyLen >> 2);
	}
	else
	{
	  Uint64 tmp[1000];
	  tmp[keyLen/8] = 0;
	  memcpy(tmp, keyData, keyLen);
	  md5_hash(buf, tmp, (keyLen+3) >> 2);	  
	}
	hashValue= buf[1];
      }
      const Uint16 *nodes;
      Uint32 cnt= impl->get_nodes(hashValue, &nodes);
      if(cnt)
	nodeId= nodes[0];
      else
	nodeId= 0;
    } else {
      nodeId = 0;
    }//if

    {
      NdbTransaction *trans= startTransactionLocal(0, nodeId);
      DBUG_PRINT("exit",("start trans: 0x%x transid: 0x%llx",
			 trans, trans ? trans->getTransactionId() : 0));
      DBUG_RETURN(trans);
    }
  } else {
    DBUG_RETURN(NULL);
  }//if
}//Ndb::startTransaction()

/*****************************************************************************
NdbTransaction* hupp(NdbTransaction* pBuddyTrans);

Return Value:   Returns a pointer to a connection object.
                Connected to the same node as pBuddyTrans
                and also using the same transction id
Remark:         Start transaction. Synchronous.
*****************************************************************************/ 
NdbTransaction* 
Ndb::hupp(NdbTransaction* pBuddyTrans)
{
  DBUG_ENTER("Ndb::hupp");

  DBUG_PRINT("enter", ("trans: 0x%x",pBuddyTrans));

  Uint32 aPriority = 0;
  if (pBuddyTrans == NULL){
    DBUG_RETURN(startTransaction());
  }

  if (theInitState == Initialised) {
    theError.code = 0;
    checkFailedNode();

    Uint32 nodeId = pBuddyTrans->getConnectedNodeId();
    NdbTransaction* pCon = startTransactionLocal(aPriority, nodeId);
    if(pCon == NULL)
      DBUG_RETURN(NULL);

    if (pCon->getConnectedNodeId() != nodeId){
      // We could not get a connection to the desired node
      // release the connection and return NULL
      closeTransaction(pCon);
      theError.code = 4006;
      DBUG_RETURN(NULL);
    }
    pCon->setTransactionId(pBuddyTrans->getTransactionId());
    pCon->setBuddyConPtr((Uint32)pBuddyTrans->getTC_ConnectPtr());
    DBUG_PRINT("exit", ("hupp trans: 0x%x transid: 0x%llx",
			pCon, pCon ? pCon->getTransactionId() : 0));
    DBUG_RETURN(pCon);
  } else {
    DBUG_RETURN(NULL);
  }//if
}//Ndb::hupp()

NdbTransaction* 
Ndb::startTransactionLocal(Uint32 aPriority, Uint32 nodeId)
{
#ifdef VM_TRACE
  char buf[255];
  const char* val = NdbEnv_GetEnv("NDB_TRANSACTION_NODE_ID", buf, 255);
  if(val != 0){
    nodeId = atoi(val);
  }
#endif

  DBUG_ENTER("Ndb::startTransactionLocal");
  DBUG_PRINT("enter", ("nodeid: %d", nodeId));

  if(unlikely(theRemainingStartTransactions == 0))
  {
    theError.code = 4006;
    DBUG_RETURN(0);
  }
  
  NdbTransaction* tConnection;
  Uint64 tFirstTransId = theFirstTransId;
  tConnection = doConnect(nodeId);
  if (tConnection == NULL) {
    DBUG_RETURN(NULL);
  }//if

  theRemainingStartTransactions--;
  NdbTransaction* tConNext = theTransactionList;
  tConnection->init();
  theTransactionList = tConnection;        // into a transaction list.
  tConnection->next(tConNext);   // Add the active connection object
  tConnection->setTransactionId(tFirstTransId);
  tConnection->thePriority = aPriority;
  if ((tFirstTransId & 0xFFFFFFFF) == 0xFFFFFFFF) {
    //---------------------------------------------------
// Transaction id rolling round. We will start from
// consecutive identity 0 again.
//---------------------------------------------------
    theFirstTransId = ((tFirstTransId >> 32) << 32);      
  } else {
    theFirstTransId = tFirstTransId + 1;
  }//if
#ifdef VM_TRACE
  if (tConnection->theListState != NdbTransaction::NotInList) {
    printState("startTransactionLocal %x", tConnection);
    abort();
  }
#endif
  DBUG_RETURN(tConnection);
}//Ndb::startTransactionLocal()

/*****************************************************************************
void closeTransaction(NdbTransaction* aConnection);

Parameters:     aConnection: the connection used in the transaction.
Remark:         Close transaction by releasing the connection and all operations.
*****************************************************************************/
void
Ndb::closeTransaction(NdbTransaction* aConnection)
{
  DBUG_ENTER("Ndb::closeTransaction");
  NdbTransaction* tCon;
  NdbTransaction* tPreviousCon;

  if (aConnection == NULL) {
//-----------------------------------------------------
// closeTransaction called on NULL pointer, destructive
// application behaviour.
//-----------------------------------------------------
#ifdef VM_TRACE
    printf("NULL into closeTransaction\n");
#endif
    DBUG_VOID_RETURN;
  }//if
  CHECK_STATUS_MACRO_VOID;
  
  tCon = theTransactionList;
  theRemainingStartTransactions++;
  
  DBUG_PRINT("info",("close trans: 0x%x transid: 0x%llx",
		     aConnection, aConnection->getTransactionId()));
  DBUG_PRINT("info",("magic number: 0x%x TCConPtr: 0x%x theMyRef: 0x%x 0x%x",
		     aConnection->theMagicNumber, aConnection->theTCConPtr,
		     aConnection->theMyRef, getReference()));

  if (aConnection == tCon) {		// Remove the active connection object
    theTransactionList = tCon->next();	// from the transaction list.
  } else { 
    while (aConnection != tCon) {
      if (tCon == NULL) {
//-----------------------------------------------------
// closeTransaction called on non-existing transaction
//-----------------------------------------------------

	if(aConnection->theError.code == 4008){
	  /**
	   * When a SCAN timed-out, returning the NdbTransaction leads
	   * to reuse. And TC crashes when the API tries to reuse it to
	   * something else...
	   */
#ifdef VM_TRACE
	  printf("Scan timeout:ed NdbTransaction-> "
		 "not returning it-> memory leak\n");
#endif
	  DBUG_VOID_RETURN;
	}

#ifdef VM_TRACE
	printf("Non-existing transaction into closeTransaction\n");
	abort();
#endif
	DBUG_VOID_RETURN;
      }//if
      tPreviousCon = tCon;
      tCon = tCon->next();
    }//while
    tPreviousCon->next(tCon->next());
  }//if
  
  aConnection->release();
  
  if(aConnection->theError.code == 4008){
    /**
     * Something timed-out, returning the NdbTransaction leads
     * to reuse. And TC crashes when the API tries to reuse it to
     * something else...
     */
#ifdef VM_TRACE
    printf("Con timeout:ed NdbTransaction-> not returning it-> memory leak\n");
#endif
    DBUG_VOID_RETURN;
  }
  
  if (aConnection->theReleaseOnClose == false) {
    /**
     * Put it back in idle list for that node
     */
    Uint32 nodeId = aConnection->getConnectedNodeId();
    aConnection->theNext = theConnectionArray[nodeId];
    theConnectionArray[nodeId] = aConnection;
    DBUG_VOID_RETURN;
  } else {
    aConnection->theReleaseOnClose = false;
    releaseNdbCon(aConnection);
  }//if
  DBUG_VOID_RETURN;
}//Ndb::closeTransaction()

/*****************************************************************************
int* NdbTamper(int aAction, int aNode);

Parameters: aAction     Specifies what action to be taken
            1: Lock global checkpointing    Can only be sent to master DIH, Parameter aNode ignored.
            2: UnLock global checkpointing    Can only be sent to master DIH, Parameter aNode ignored.
	    3: Crash node

           aNode        Specifies which node the action will be taken
     	  -1: Master DIH 
       	0-16: Nodnumber

Return Value: -1 Error  .
                
Remark:         Sends a signal to DIH.
*****************************************************************************/ 
int 
Ndb::NdbTamper(TamperType aAction, int aNode)
{
  NdbTransaction*	tNdbConn;
  NdbApiSignal		tSignal(theMyRef);
  int			tNode;
  int                   tAction;
  int			ret_code;

#ifdef CUSTOMER_RELEASE
  return -1;
#else
  DBUG_ENTER("Ndb::NdbTamper");
  CHECK_STATUS_MACRO;
  checkFailedNode();

  theRestartGCI = 0;
  switch (aAction) {
// Translate enum to integer. This is done because the SCI layer
// expects integers. 
     case LockGlbChp:
        tAction = 1;
        break;
     case UnlockGlbChp:
        tAction = 2;
	break;
     case CrashNode:
        tAction = 3;
        break;
     case ReadRestartGCI:
	tAction = 4;
	break;
     default:
        theError.code = 4102;
        DBUG_RETURN(-1);
  }

  tNdbConn = getNdbCon();	// Get free connection object
  if (tNdbConn == NULL) {
    theError.code = 4000;
    DBUG_RETURN(-1);
  }
  tSignal.setSignal(GSN_DIHNDBTAMPER);
  tSignal.setData (tAction, 1);
  tSignal.setData(tNdbConn->ptr2int(),2);
  tSignal.setData(theMyRef,3);		// Set return block reference
  tNdbConn->Status(NdbTransaction::Connecting); // Set status to connecting
  TransporterFacade *tp = TransporterFacade::instance();
  if (tAction == 3) {
    tp->lock_mutex();
    tp->sendSignal(&tSignal, aNode);
    tp->unlock_mutex();
    releaseNdbCon(tNdbConn);
  } else if ( (tAction == 2) || (tAction == 1) ) {
    tp->lock_mutex();
    tNode = tp->get_an_alive_node();
    if (tNode == 0) {
      theError.code = 4002;
      releaseNdbCon(tNdbConn);
      DBUG_RETURN(-1);
    }//if
    ret_code = tp->sendSignal(&tSignal,aNode);
    tp->unlock_mutex();
    releaseNdbCon(tNdbConn);
    DBUG_RETURN(ret_code);
  } else {
    do {
      tp->lock_mutex();
      // Start protected area
      tNode = tp->get_an_alive_node();
      tp->unlock_mutex();
      // End protected area
      if (tNode == 0) {
        theError.code = 4009;
        releaseNdbCon(tNdbConn);
        DBUG_RETURN(-1);
      }//if
      ret_code = sendRecSignal(tNode, WAIT_NDB_TAMPER, &tSignal, 0);
      if (ret_code == 0) {  
        if (tNdbConn->Status() != NdbTransaction::Connected) {
          theRestartGCI = 0;
        }//if
        releaseNdbCon(tNdbConn);
        DBUG_RETURN(theRestartGCI);
      } else if ((ret_code == -5) || (ret_code == -2)) {
        TRACE_DEBUG("Continue DIHNDBTAMPER when node failed/stopping");
      } else {
        DBUG_RETURN(-1);
      }//if
    } while (1);
  }
  DBUG_RETURN(0);
#endif
}
#if 0
/****************************************************************************
NdbSchemaCon* startSchemaTransaction();

Return Value:   Returns a pointer to a schema connection object.
                Return NULL otherwise.
Remark:         Start schema transaction. Synchronous.
****************************************************************************/ 
NdbSchemaCon* 
Ndb::startSchemaTransaction()
{
  NdbSchemaCon* tSchemaCon;
  if (theSchemaConToNdbList != NULL) {
    theError.code = 4321;
    return NULL;
  }//if
  tSchemaCon = new NdbSchemaCon(this);
  if (tSchemaCon == NULL) {
    theError.code = 4000;
    return NULL;
  }//if 
  theSchemaConToNdbList = tSchemaCon;
  return tSchemaCon;  
}
/*****************************************************************************
void closeSchemaTransaction(NdbSchemaCon* aSchemaCon);

Parameters:     aSchemaCon: the schemacon used in the transaction.
Remark:         Close transaction by releasing the schemacon and all schemaop.
*****************************************************************************/
void
Ndb::closeSchemaTransaction(NdbSchemaCon* aSchemaCon)
{
  if (theSchemaConToNdbList != aSchemaCon) {
    abort();
    return;
  }//if
  aSchemaCon->release();
  delete aSchemaCon;
  theSchemaConToNdbList = NULL;
  return;
}//Ndb::closeSchemaTransaction()
#endif

/*****************************************************************************
void RestartGCI(int aRestartGCI);

Remark:		Set theRestartGCI on the NDB object
*****************************************************************************/
void
Ndb::RestartGCI(int aRestartGCI)
{
  theRestartGCI = aRestartGCI;
}

/****************************************************************************
int getBlockNumber(void);

Remark:		
****************************************************************************/
int
Ndb::getBlockNumber()
{
  return theNdbBlockNumber;
}

NdbDictionary::Dictionary *
Ndb::getDictionary() const {
  return theDictionary;
}

/****************************************************************************
int getNodeId();

Remark:		
****************************************************************************/
int
Ndb::getNodeId()
{
  return theNode;
}

/****************************************************************************
Uint64 getTupleIdFromNdb( Uint32 aTableId, Uint32 cacheSize );

Parameters:     aTableId : The TableId.
                cacheSize: Prefetch this many values
Remark:		Returns a new TupleId to the application.
                The TupleId comes from SYSTAB_0 where SYSKEY_0 = TableId.
                It is initialized to (TableId << 48) + 1 in NdbcntrMain.cpp.
****************************************************************************/
Uint64
Ndb::getAutoIncrementValue(const char* aTableName, Uint32 cacheSize)
{
  DBUG_ENTER("getAutoIncrementValue");
  BaseString internal_tabname(internalize_table_name(aTableName));

  Ndb_local_table_info *info=
    theDictionary->get_local_table_info(internal_tabname, false);
  if (info == 0)
    DBUG_RETURN(~(Uint64)0);
  const NdbTableImpl *table= info->m_table_impl;
  Uint64 tupleId = getTupleIdFromNdb(table->m_tableId, cacheSize);
  DBUG_PRINT("info", ("value %ul", (ulong) tupleId));
  DBUG_RETURN(tupleId);
}

Uint64
Ndb::getAutoIncrementValue(const NdbDictionary::Table * aTable, Uint32 cacheSize)
{
  DBUG_ENTER("getAutoIncrementValue");
  if (aTable == 0)
    DBUG_RETURN(~(Uint64)0);
  const NdbTableImpl* table = & NdbTableImpl::getImpl(*aTable);
  Uint64 tupleId = getTupleIdFromNdb(table->m_tableId, cacheSize);
  DBUG_PRINT("info", ("value %ul", (ulong) tupleId));
  DBUG_RETURN(tupleId);
}

Uint64 
Ndb::getTupleIdFromNdb(const char* aTableName, Uint32 cacheSize)
{
  const NdbTableImpl* table = theDictionary->getTable(aTableName);
  if (table == 0)
    return ~(Uint64)0;
  return getTupleIdFromNdb(table->m_tableId, cacheSize);
}

Uint64
Ndb::getTupleIdFromNdb(Uint32 aTableId, Uint32 cacheSize)
{
  DBUG_ENTER("getTupleIdFromNdb");
  if ( theFirstTupleId[aTableId] != theLastTupleId[aTableId] )
  {
    theFirstTupleId[aTableId]++;
    DBUG_PRINT("info", ("next cached value %ul", 
                        (ulong) theFirstTupleId[aTableId]));
    DBUG_RETURN(theFirstTupleId[aTableId]);
  }
  else // theFirstTupleId == theLastTupleId
  {
    DBUG_PRINT("info",("reading %u values from database", 
                       (cacheSize == 0) ? 1 : cacheSize));
    DBUG_RETURN(opTupleIdOnNdb(aTableId, (cacheSize == 0) ? 1 : cacheSize, 0));
  }
}

Uint64
Ndb::readAutoIncrementValue(const char* aTableName)
{
  DBUG_ENTER("readAutoIncrementValue");
  const NdbTableImpl* table = theDictionary->getTable(aTableName);
  if (table == 0) {
    theError= theDictionary->getNdbError();
    DBUG_RETURN(~(Uint64)0);
  }
  Uint64 tupleId = readTupleIdFromNdb(table->m_tableId);
  DBUG_PRINT("info", ("value %ul", (ulong) tupleId));
  DBUG_RETURN(tupleId);
}

Uint64
Ndb::readAutoIncrementValue(const NdbDictionary::Table * aTable)
{
  DBUG_ENTER("readAutoIncrementValue");
  if (aTable == 0)
    DBUG_RETURN(~(Uint64)0);
  const NdbTableImpl* table = & NdbTableImpl::getImpl(*aTable);
  Uint64 tupleId = readTupleIdFromNdb(table->m_tableId);
  DBUG_PRINT("info", ("value %ul", (ulong) tupleId));
  DBUG_RETURN(tupleId);
}

Uint64
Ndb::readTupleIdFromNdb(Uint32 aTableId)
{
  if ( theFirstTupleId[aTableId] == theLastTupleId[aTableId] )
    // Cache is empty, check next in database
    return opTupleIdOnNdb(aTableId, 0, 3);

  return theFirstTupleId[aTableId] + 1;
}

bool
Ndb::setAutoIncrementValue(const char* aTableName, Uint64 val, bool increase)
{
  DBUG_ENTER("setAutoIncrementValue");
  BaseString internal_tabname(internalize_table_name(aTableName));

  Ndb_local_table_info *info=
    theDictionary->get_local_table_info(internal_tabname, false);
  if (info == 0) {
    theError= theDictionary->getNdbError();
    DBUG_RETURN(false);
  }
  const NdbTableImpl* table= info->m_table_impl;
  DBUG_RETURN(setTupleIdInNdb(table->m_tableId, val, increase));
}

bool
Ndb::setAutoIncrementValue(const NdbDictionary::Table * aTable, Uint64 val, bool increase)
{
  DBUG_ENTER("setAutoIncrementValue");
  if (aTable == 0)
    DBUG_RETURN(~(Uint64)0);
  const NdbTableImpl* table = & NdbTableImpl::getImpl(*aTable);
  DBUG_RETURN(setTupleIdInNdb(table->m_tableId, val, increase));
}

bool
Ndb::setTupleIdInNdb(const char* aTableName, Uint64 val, bool increase )
{
  DBUG_ENTER("setTupleIdInNdb(const char*, ...)");
  const NdbTableImpl* table = theDictionary->getTable(aTableName);
  if (table == 0) {
    theError= theDictionary->getNdbError();
    DBUG_RETURN(false);
  }
  DBUG_RETURN(setTupleIdInNdb(table->m_tableId, val, increase));
}

bool
Ndb::setTupleIdInNdb(Uint32 aTableId, Uint64 val, bool increase )
{
  DBUG_ENTER("setTupleIdInNdb(Uint32, ...)");
  if (increase)
  {
    if (theFirstTupleId[aTableId] != theLastTupleId[aTableId])
    {
      // We have a cache sequence
      if (val <= theFirstTupleId[aTableId]+1)
	DBUG_RETURN(false);
      if (val <= theLastTupleId[aTableId])
      {
	theFirstTupleId[aTableId] = val - 1;
	DBUG_RETURN(true);
      }
      // else continue;
    }
    DBUG_RETURN((opTupleIdOnNdb(aTableId, val, 2) == val));
  }
  else
    DBUG_RETURN((opTupleIdOnNdb(aTableId, val, 1) == val));
}

Uint64
Ndb::opTupleIdOnNdb(Uint32 aTableId, Uint64 opValue, Uint32 op)
{
  DBUG_ENTER("Ndb::opTupleIdOnNdb");
  DBUG_PRINT("enter", ("table=%u value=%llu op=%u", aTableId, opValue, op));

  NdbTransaction*     tConnection;
  NdbOperation*      tOperation= 0; // Compiler warning if not initialized
  Uint64             tValue;
  NdbRecAttr*        tRecAttrResult;
  int                result;
  Uint64 ret;

  CHECK_STATUS_MACRO_ZERO;

  BaseString currentDb(getDatabaseName());
  BaseString currentSchema(getDatabaseSchemaName());

  setDatabaseName("sys");
  setDatabaseSchemaName("def");
  tConnection = this->startTransaction();
  if (tConnection == NULL)
    goto error_return;

  if (usingFullyQualifiedNames())
    tOperation = tConnection->getNdbOperation("SYSTAB_0");
  else
    tOperation = tConnection->getNdbOperation("sys/def/SYSTAB_0");
  if (tOperation == NULL)
    goto error_handler;

  switch (op)
    {
    case 0:
      tOperation->interpretedUpdateTuple();
      tOperation->equal("SYSKEY_0", aTableId );
      tOperation->incValue("NEXTID", opValue);
      tRecAttrResult = tOperation->getValue("NEXTID");

      if (tConnection->execute( Commit ) == -1 )
        goto error_handler;

      tValue = tRecAttrResult->u_64_value();

      theFirstTupleId[aTableId] = tValue - opValue;
      theLastTupleId[aTableId]  = tValue - 1;
      ret = theFirstTupleId[aTableId];
      break;
    case 1:
      tOperation->updateTuple();
      tOperation->equal("SYSKEY_0", aTableId );
      tOperation->setValue("NEXTID", opValue);

      if (tConnection->execute( Commit ) == -1 )
        goto error_handler;

      theFirstTupleId[aTableId] = ~(Uint64)0;
      theLastTupleId[aTableId]  = ~(Uint64)0;
      ret = opValue;
      break;
    case 2:
      tOperation->interpretedUpdateTuple();
      tOperation->equal("SYSKEY_0", aTableId );
      tOperation->load_const_u64(1, opValue);
      tOperation->read_attr("NEXTID", 2);
      tOperation->branch_le(2, 1, 0);
      tOperation->write_attr("NEXTID", 1);
      tOperation->interpret_exit_ok();
      tOperation->def_label(0);
      tOperation->interpret_exit_nok(9999);
      
      if ( (result = tConnection->execute( Commit )) == -1 )
        goto error_handler;
      
      if (result == 9999)
        ret = ~(Uint64)0;
      else
      {
        theFirstTupleId[aTableId] = theLastTupleId[aTableId] = opValue - 1;
	ret = opValue;
      }
      break;
    case 3:
      tOperation->readTuple();
      tOperation->equal("SYSKEY_0", aTableId );
      tRecAttrResult = tOperation->getValue("NEXTID");
      if (tConnection->execute( Commit ) == -1 )
        goto error_handler;
      ret = tRecAttrResult->u_64_value();
      break;
    default:
      goto error_handler;
    }

  this->closeTransaction(tConnection);

  // Restore current name space
  setDatabaseName(currentDb.c_str());
  setDatabaseSchemaName(currentSchema.c_str());

  DBUG_RETURN(ret);

  error_handler:
    theError.code = tConnection->theError.code;
    this->closeTransaction(tConnection);
  error_return:
    // Restore current name space
    setDatabaseName(currentDb.c_str());
    setDatabaseSchemaName(currentSchema.c_str());

  DBUG_PRINT("error", ("ndb=%d con=%d op=%d",
             theError.code,
             tConnection ? tConnection->theError.code : -1,
             tOperation ? tOperation->theError.code : -1));
  DBUG_RETURN(~(Uint64)0);
}

Uint32
convertEndian(Uint32 Data)
{
#ifdef WORDS_BIGENDIAN
  Uint32 t1, t2, t3, t4;
  t4 = (Data >> 24) & 255;
  t3 = (Data >> 16) & 255;
  t4 = t4 + (t3 << 8);
  t2 = (Data >> 8) & 255;
  t4 = t4 + (t2 << 16);
  t1 = Data & 255;
  t4 = t4 + (t1 << 24);
  return t4;
#else
  return Data;
#endif
}
const char * Ndb::getCatalogName() const
{
  return theImpl->m_dbname.c_str();
}


void Ndb::setCatalogName(const char * a_catalog_name)
{
  if (a_catalog_name)
  {
    theImpl->m_dbname.assign(a_catalog_name);
    theImpl->update_prefix();
  }
}


const char * Ndb::getSchemaName() const
{
  return theImpl->m_schemaname.c_str();
}


void Ndb::setSchemaName(const char * a_schema_name)
{
  if (a_schema_name) {
    theImpl->m_schemaname.assign(a_schema_name);
    theImpl->update_prefix();
  }
}
 
/*
Deprecated functions
*/
const char * Ndb::getDatabaseName() const
{
  return getCatalogName();
}
 
void Ndb::setDatabaseName(const char * a_catalog_name)
{
  setCatalogName(a_catalog_name);
}
 
const char * Ndb::getDatabaseSchemaName() const
{
  return getSchemaName();
}
 
void Ndb::setDatabaseSchemaName(const char * a_schema_name)
{
  setSchemaName(a_schema_name);
}
 
bool Ndb::usingFullyQualifiedNames()
{
  return fullyQualifiedNames;
}
 
const char *
Ndb::externalizeTableName(const char * internalTableName, bool fullyQualifiedNames)
{
  if (fullyQualifiedNames) {
    register const char *ptr = internalTableName;
   
    // Skip database name
    while (*ptr && *ptr++ != table_name_separator);
    // Skip schema name
    while (*ptr && *ptr++ != table_name_separator);
    return ptr;
  }
  else
    return internalTableName;
}

const char *
Ndb::externalizeTableName(const char * internalTableName)
{
  return externalizeTableName(internalTableName, usingFullyQualifiedNames());
}

const char *
Ndb::externalizeIndexName(const char * internalIndexName, bool fullyQualifiedNames)
{
  if (fullyQualifiedNames) {
    register const char *ptr = internalIndexName;
   
    // Scan name from the end
    while (*ptr++); ptr--; // strend
    while (ptr >= internalIndexName && *ptr != table_name_separator)
      ptr--;
     
    return ptr + 1;
  }
  else
    return internalIndexName;
}

const char *
Ndb::externalizeIndexName(const char * internalIndexName)
{
  return externalizeIndexName(internalIndexName, usingFullyQualifiedNames());
}


const BaseString
Ndb::internalize_table_name(const char *external_name) const
{
  BaseString ret;
  DBUG_ENTER("internalize_table_name");
  DBUG_PRINT("enter", ("external_name: %s", external_name));

  if (fullyQualifiedNames)
  {
    /* Internal table name format <db>/<schema>/<table>
       <db>/<schema> is already available in m_prefix
       so just concat the two strings
     */
    ret.assfmt("%s%s",
               theImpl->m_prefix.c_str(),
               external_name);
  }
  else
    ret.assign(external_name);

  DBUG_PRINT("exit", ("internal_name: %s", ret.c_str()));
  DBUG_RETURN(ret);
}


const BaseString
Ndb::internalize_index_name(const NdbTableImpl * table,
                           const char * external_name) const
{
  BaseString ret;
  DBUG_ENTER("internalize_index_name");
  DBUG_PRINT("enter", ("external_name: %s, table_id: %d",
                       external_name, table ? table->m_tableId : ~0));
  if (!table)
  {
    DBUG_PRINT("error", ("!table"));
    return ret;
  }

  if (fullyQualifiedNames)
  {
    /* Internal index name format <db>/<schema>/<tabid>/<table> */
    ret.assfmt("%s%d%c%s",
               theImpl->m_prefix.c_str(),
               table->m_tableId,
               table_name_separator,
               external_name);
  }
  else
    ret.assign(external_name);

  DBUG_PRINT("exit", ("internal_name: %s", ret.c_str()));
  DBUG_RETURN(ret);
}


const BaseString
Ndb::getDatabaseFromInternalName(const char * internalName)
{
  char * databaseName = new char[strlen(internalName) + 1];
  strcpy(databaseName, internalName);
  register char *ptr = databaseName;
   
  /* Scan name for the first table_name_separator */
  while (*ptr && *ptr != table_name_separator)
    ptr++;
  *ptr = '\0';
  BaseString ret = BaseString(databaseName);
  delete [] databaseName;
  return ret;
}
 
const BaseString
Ndb::getSchemaFromInternalName(const char * internalName)
{
  char * schemaName = new char[strlen(internalName)];
  register const char *ptr1 = internalName;
   
  /* Scan name for the second table_name_separator */
  while (*ptr1 && *ptr1 != table_name_separator)
    ptr1++;
  strcpy(schemaName, ptr1 + 1);
  register char *ptr = schemaName;
  while (*ptr && *ptr != table_name_separator)
    ptr++;
  *ptr = '\0';
  BaseString ret = BaseString(schemaName);
  delete [] schemaName;
  return ret;
}

// ToDo set event buffer size
NdbEventOperation* Ndb::createEventOperation(const char* eventName)
{
  DBUG_ENTER("Ndb::createEventOperation");
  NdbEventOperation* tOp= theEventBuffer->createEventOperation(eventName,
							       theError);
  if (tOp)
  {
    // keep track of all event operations
    NdbEventOperationImpl *op=
      NdbEventBuffer::getEventOperationImpl(tOp);
    op->m_next= theImpl->m_ev_op;
    op->m_prev= 0;
    theImpl->m_ev_op= op;
    if (op->m_next)
      op->m_next->m_prev= op;
  }

  DBUG_RETURN(tOp);
}

int Ndb::dropEventOperation(NdbEventOperation* tOp)
{
  DBUG_ENTER("Ndb::dropEventOperation");
  // remove it from list
  NdbEventOperationImpl *op=
    NdbEventBuffer::getEventOperationImpl(tOp);
  if (op->m_next)
    op->m_next->m_prev= op->m_prev;
  if (op->m_prev)
    op->m_prev->m_next= op->m_next;
  else
    theImpl->m_ev_op= op->m_next;

  assert(theImpl->m_ev_op == 0 || theImpl->m_ev_op->m_prev == 0);

  theEventBuffer->dropEventOperation(tOp);
  DBUG_RETURN(0);
}

NdbEventOperation *Ndb::getEventOperation(NdbEventOperation* tOp)
{
  NdbEventOperationImpl *op;
  if (tOp)
    op= NdbEventBuffer::getEventOperationImpl(tOp)->m_next;
  else
    op= theImpl->m_ev_op;
  if (op)
    return op->m_facade;
  return 0;
}

int
Ndb::pollEvents(int aMillisecondNumber, Uint64 *latestGCI)
{
  return theEventBuffer->pollEvents(aMillisecondNumber, latestGCI);
}

NdbEventOperation *Ndb::nextEvent()
{
  return theEventBuffer->nextEvent();
}

Uint64 Ndb::getLatestGCI()
{
  return theEventBuffer->getLatestGCI();
}

void Ndb::setReportThreshEventGCISlip(unsigned thresh)
{
  theEventBuffer->m_gci_slip_thresh= thresh;
}

void Ndb::setReportThreshEventFreeMem(unsigned thresh)
{
  theEventBuffer->m_free_thresh= thresh;
}

#ifdef VM_TRACE
#include <NdbMutex.h>
extern NdbMutex *ndb_print_state_mutex;

static bool
checkdups(NdbTransaction** list, unsigned no)
{
  for (unsigned i = 0; i < no; i++)
    for (unsigned j = i + 1; j < no; j++)
      if (list[i] == list[j])
        return true;
  return false;
}
void
Ndb::printState(const char* fmt, ...)
{
  char buf[200];
  va_list ap;
  va_start(ap, fmt);
  vsprintf(buf, fmt, ap);
  va_end(ap);
  NdbMutex_Lock(ndb_print_state_mutex);
  bool dups = false;
  unsigned i;
  ndbout << buf << " ndb=" << hex << this << dec;
#ifndef NDB_WIN32
  ndbout << " thread=" << (int)pthread_self();
#endif
  ndbout << endl;
  for (unsigned n = 0; n < MAX_NDB_NODES; n++) {
    NdbTransaction* con = theConnectionArray[n];
    if (con != 0) {
      ndbout << "conn " << n << ":" << endl;
      while (con != 0) {
        con->printState();
        con = con->theNext;
      }
    }
  }
  ndbout << "prepared: " << theNoOfPreparedTransactions<< endl;
  if (checkdups(thePreparedTransactionsArray, theNoOfPreparedTransactions)) {
    ndbout << "!! DUPS !!" << endl;
    dups = true;
  }
  for (i = 0; i < theNoOfPreparedTransactions; i++)
    thePreparedTransactionsArray[i]->printState();
  ndbout << "sent: " << theNoOfSentTransactions<< endl;
  if (checkdups(theSentTransactionsArray, theNoOfSentTransactions)) {
    ndbout << "!! DUPS !!" << endl;
    dups = true;
  }
  for (i = 0; i < theNoOfSentTransactions; i++)
    theSentTransactionsArray[i]->printState();
  ndbout << "completed: " << theNoOfCompletedTransactions<< endl;
  if (checkdups(theCompletedTransactionsArray, theNoOfCompletedTransactions)) {
    ndbout << "!! DUPS !!" << endl;
    dups = true;
  }
  for (i = 0; i < theNoOfCompletedTransactions; i++)
    theCompletedTransactionsArray[i]->printState();
  NdbMutex_Unlock(ndb_print_state_mutex);
}
#endif



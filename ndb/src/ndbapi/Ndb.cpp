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
#include <pthread.h>

#include "NdbApiSignal.hpp"
#include "NdbImpl.hpp"
#include <NdbOperation.hpp>
#include <NdbConnection.hpp>
#include <NdbEventOperation.hpp>
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
NdbConnection* Ndb::doConnect(Uint32 tConNode) 
{
  Uint32        tNode;
  Uint32        i = 0;;
  Uint32        tAnyAlive = 0;
  int TretCode;

  if (tConNode != 0) {
    TretCode = NDB_connect(tConNode);
    if ((TretCode == 1) || (TretCode == 2)) {
//****************************************************************************
// We have connections now to the desired node. Return
//****************************************************************************
      return getConnectedNdbConnection(tConNode);
    } else if (TretCode != 0) {
      tAnyAlive = 1;
    }//if
  }//if
//****************************************************************************
// We will connect to any node. Make sure that we have connections to all
// nodes.
//****************************************************************************
  Uint32 tNoOfDbNodes = theNoOfDBnodes;
  i = theCurrentConnectIndex;
  UintR Tcount = 0;
  do {
    if (i >= tNoOfDbNodes) {
      i = 0;
    }//if
    Tcount++;
    tNode = theDBnodes[i];
    TretCode = NDB_connect(tNode);
    if ((TretCode == 1) || (TretCode == 2)) {
//****************************************************************************
// We have connections now to the desired node. Return
//****************************************************************************
      if (theCurrentConnectIndex == i) {
        theCurrentConnectCounter++;
        if (theCurrentConnectCounter == 8) {
	  theCurrentConnectCounter = 1;
	  theCurrentConnectIndex++;
	}//if
      } else {
	// Set to 2 because we have already connected to a node 
	// when we get here.
        theCurrentConnectCounter = 2;
        theCurrentConnectIndex = i;
      }//if
      return getConnectedNdbConnection(tNode);
    } else if (TretCode != 0) {
      tAnyAlive = 1;
    }//if
    i++;
  } while (Tcount < tNoOfDbNodes);
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
  return NULL;
}

int 
Ndb::NDB_connect(Uint32 tNode) 
{
//****************************************************************************
// We will perform seize of a transaction record in DBTC in the specified node.
//***************************************************************************
  
  int	         tReturnCode;
  TransporterFacade *tp = TransporterFacade::instance();

  bool nodeAvail = tp->get_node_alive(tNode);
  if(nodeAvail == false){
    return 0;
  }
  
  NdbConnection * tConArray = theConnectionArray[tNode];
  if (tConArray != NULL) {
    return 2;
  }
  
  NdbConnection * tNdbCon = getNdbCon();	// Get free connection object.
  if (tNdbCon == NULL) {
    return 4;
  }//if
  NdbApiSignal*	tSignal = getSignal();		// Get signal object
  if (tSignal == NULL) {
    releaseNdbCon(tNdbCon);
    return 4;
  }//if
  if (tSignal->setSignal(GSN_TCSEIZEREQ) == -1) {
    releaseNdbCon(tNdbCon);
    releaseSignal(tSignal);
    return 4;
  }//if
  tSignal->setData(tNdbCon->ptr2int(), 1);
//************************************************
// Set connection pointer as NdbConnection object
//************************************************
  tSignal->setData(theMyRef, 2);	// Set my block reference
  tNdbCon->Status(NdbConnection::Connecting); // Set status to connecting
  Uint32 nodeSequence;
  { // send and receive signal
    Guard guard(tp->theMutexPtr);
    nodeSequence = tp->getNodeSequence(tNode);
    bool node_is_alive = tp->get_node_alive(tNode);
    if (node_is_alive) { 
      tReturnCode = tp->sendSignal(tSignal, tNode);  
      releaseSignal(tSignal); 
      if (tReturnCode != -1) {
        theWaiter.m_node = tNode;  
        theWaiter.m_state = WAIT_TC_SEIZE;  
        tReturnCode = receiveResponse(); 
      }//if
    } else {
      releaseSignal(tSignal);
      tReturnCode = -1;
    }//if
  }
  if ((tReturnCode == 0) && (tNdbCon->Status() == NdbConnection::Connected)) {
    //************************************************
    // Send and receive was successful
    //************************************************
    NdbConnection* tPrevFirst = theConnectionArray[tNode];
    tNdbCon->setConnectedNodeId(tNode, nodeSequence);
    
    tNdbCon->setMyBlockReference(theMyRef);
    theConnectionArray[tNode] = tNdbCon;
    tNdbCon->theNext = tPrevFirst;
    return 1;
  } else {
    releaseNdbCon(tNdbCon);
//****************************************************************************
// Unsuccessful connect is indicated by 3.
//****************************************************************************
    return 3;
  }//if
}//Ndb::NDB_connect()

NdbConnection *
Ndb::getConnectedNdbConnection(Uint32 nodeId){
  NdbConnection* next = theConnectionArray[nodeId];
  theConnectionArray[nodeId] = next->theNext;
  next->theNext = NULL;

  return next;
}//Ndb::getConnectedNdbConnection()

/*****************************************************************************
disconnect();

Remark:        Disconnect all connections to the database. 
*****************************************************************************/
void 
Ndb::doDisconnect()
{
  DBUG_ENTER("Ndb::doDisconnect");
  NdbConnection* tNdbCon;
  CHECK_STATUS_MACRO_VOID;

  DBUG_PRINT("info", ("theNoOfDBnodes=%d", theNoOfDBnodes));
  Uint32 tNoOfDbNodes = theNoOfDBnodes;
  UintR i;
  for (i = 0; i < tNoOfDbNodes; i++) {
    Uint32 tNode = theDBnodes[i];
    tNdbCon = theConnectionArray[tNode];
    while (tNdbCon != NULL) {
      NdbConnection* tmpNdbCon = tNdbCon;
      tNdbCon = tNdbCon->theNext;
      releaseConnectToNdb(tmpNdbCon);
    }//while
  }//for
  tNdbCon = theTransactionList;
  while (tNdbCon != NULL) {
    NdbConnection* tmpNdbCon = tNdbCon;
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

  do {
    if ((id = theNode) != 0) {
      unsigned int foundAliveNode = 0;
      TransporterFacade *tp = TransporterFacade::instance();
      tp->lock_mutex();
      for (unsigned int i = 0; i < theNoOfDBnodes; i++) {
	const NodeId nodeId = theDBnodes[i];
	//************************************************
	// If any node is answering, ndb is answering
	//************************************************
	if (tp->get_node_alive(nodeId) != 0) {
	  foundAliveNode++;
	}//if
      }//for
      
      tp->unlock_mutex();
      if (foundAliveNode == theNoOfDBnodes) {
	DBUG_RETURN(0);
      }//if
      if (foundAliveNode > 0) {
	noChecksSinceFirstAliveFound++;
      }//if
      if (noChecksSinceFirstAliveFound > 30) {
	DBUG_RETURN(0);
      }//if
    }//if theNode != 0
    if (secondsCounter >= timeout)
      break;
    NdbSleep_MilliSleep(100);
    milliCounter += 100;
    if (milliCounter >= 1000) {
      secondsCounter++;
      milliCounter = 0;
    }//if
  } while (1);
  if (id == 0) {
    theError.code = 4269;
    DBUG_RETURN(-1);
  }
  if (noChecksSinceFirstAliveFound > 0) {
    DBUG_RETURN(0);
  }//if
  theError.code = 4009;
  DBUG_RETURN(-1);
}

/*****************************************************************************
NdbConnection* startTransaction();

Return Value:   Returns a pointer to a connection object.
                Return NULL otherwise.
Remark:         Start transaction. Synchronous.
*****************************************************************************/ 
NdbConnection* 
Ndb::startTransaction(Uint32 aPriority, const char * keyData, Uint32 keyLen)
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
    if(keyData != 0) {
      Uint32 fragmentId = computeFragmentId(keyData, keyLen);
      nodeId     = guessPrimaryNode(fragmentId);
    } else {
      nodeId = 0;
    }//if
    DBUG_RETURN(startTransactionLocal(aPriority, nodeId));
  } else {
    DBUG_RETURN(NULL);
  }//if
}//Ndb::startTransaction()

/*****************************************************************************
NdbConnection* hupp(NdbConnection* pBuddyTrans);

Return Value:   Returns a pointer to a connection object.
                Connected to the same node as pBuddyTrans
                and also using the same transction id
Remark:         Start transaction. Synchronous.
*****************************************************************************/ 
NdbConnection* 
Ndb::hupp(NdbConnection* pBuddyTrans)
{
  DBUG_ENTER("Ndb::hupp");

  Uint32 aPriority = 0;
  if (pBuddyTrans == NULL){
    DBUG_RETURN(startTransaction());
  }

  if (theInitState == Initialised) {
    theError.code = 0;
    checkFailedNode();

    Uint32 nodeId = pBuddyTrans->getConnectedNodeId();
    NdbConnection* pCon = startTransactionLocal(aPriority, nodeId);
    if(pCon == NULL)
      DBUG_RETURN(NULL);

    if (pCon->getConnectedNodeId() != nodeId){
      // We could not get a connection to the desired node
      // release the connection and return NULL
      closeTransaction(pCon);
      DBUG_RETURN(NULL);
    }
    pCon->setTransactionId(pBuddyTrans->getTransactionId());
    pCon->setBuddyConPtr((Uint32)pBuddyTrans->getTC_ConnectPtr());
    DBUG_RETURN(pCon);
  } else {
    DBUG_RETURN(NULL);
  }//if
}//Ndb::hupp()

NdbConnection* 
Ndb::startTransactionDGroup(Uint32 aPriority, const char * keyData, int type)
{

  char DGroup[4];
  if ((keyData == NULL) ||
      (type > 1)) {
    theError.code = 4118;
    return NULL;
  }//if
  if (theInitState == Initialised) {
    theError.code = 0;
    checkFailedNode();
  /**
   * If the user supplied key data
   * We will make a qualified quess to which node is the primary for the
   * the fragment and contact that node
   */
    Uint32 fragmentId;
    if (type == 0) {
      DGroup[0] = keyData[0];
      DGroup[1] = keyData[1];
      DGroup[2] = 0x30;
      DGroup[3] = 0x30;
      fragmentId = computeFragmentId(&DGroup[0], 4);
    } else {
      Uint32 hashValue = ((keyData[0] - 0x30) * 10) + (keyData[1] - 0x30);
      fragmentId = getFragmentId(hashValue);    
    }//if
    Uint32 nodeId     = guessPrimaryNode(fragmentId);
    return startTransactionLocal(aPriority, nodeId);
  } else {
    return NULL;
  }//if
}//Ndb::startTransaction()

NdbConnection* 
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

  NdbConnection* tConnection;
  Uint64 tFirstTransId = theFirstTransId;
  tConnection = doConnect(nodeId);
  if (tConnection == NULL) {
    DBUG_RETURN(NULL);
  }//if
  NdbConnection* tConNext = theTransactionList;
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
  if (tConnection->theListState != NdbConnection::NotInList) {
    printState("startTransactionLocal %x", tConnection);
    abort();
  }
#endif
  DBUG_PRINT("exit", ("transaction id: %d", tConnection->getTransactionId()));
  DBUG_RETURN(tConnection);
}//Ndb::startTransactionLocal()

/*****************************************************************************
void closeTransaction(NdbConnection* aConnection);

Parameters:     aConnection: the connection used in the transaction.
Remark:         Close transaction by releasing the connection and all operations.
*****************************************************************************/
void
Ndb::closeTransaction(NdbConnection* aConnection)
{
  DBUG_ENTER("Ndb::closeTransaction");

  NdbConnection* tCon;
  NdbConnection* tPreviousCon;

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
	   * When a SCAN timed-out, returning the NdbConnection leads
	   * to reuse. And TC crashes when the API tries to reuse it to
	   * something else...
	   */
#ifdef VM_TRACE
	  printf("Scan timeout:ed NdbConnection-> "
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
     * Something timed-out, returning the NdbConnection leads
     * to reuse. And TC crashes when the API tries to reuse it to
     * something else...
     */
#ifdef VM_TRACE
    printf("Con timeout:ed NdbConnection-> not returning it-> memory leak\n");
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
  NdbConnection*	tNdbConn;
  NdbApiSignal		tSignal(theMyRef);
  int			tNode;
  int                   tAction;
  int			ret_code;

#ifdef CUSTOMER_RELEASE
  return -1;
#else
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
        return -1;
  }

  tNdbConn = getNdbCon();	// Get free connection object
  if (tNdbConn == NULL) {
    theError.code = 4000;
    return -1;
  }
  tSignal.setSignal(GSN_DIHNDBTAMPER);
  tSignal.setData (tAction, 1);
  tSignal.setData(tNdbConn->ptr2int(),2);
  tSignal.setData(theMyRef,3);		// Set return block reference
  tNdbConn->Status(NdbConnection::Connecting); // Set status to connecting
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
      return -1;
    }//if
    ret_code = tp->sendSignal(&tSignal,aNode);
    tp->unlock_mutex();
    releaseNdbCon(tNdbConn);
    return ret_code;
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
        return -1;
      }//if
      ret_code = sendRecSignal(tNode, WAIT_NDB_TAMPER, &tSignal, 0);
      if (ret_code == 0) {  
        if (tNdbConn->Status() != NdbConnection::Connected) {
          theRestartGCI = 0;
        }//if
        releaseNdbCon(tNdbConn);
        return theRestartGCI;
      } else if ((ret_code == -5) || (ret_code == -2)) {
        TRACE_DEBUG("Continue DIHNDBTAMPER when node failed/stopping");
      } else {
        return -1;
      }//if
    } while (1);
  }
  return 0;
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
#define DEBUG_TRACE(msg) \
//  ndbout << __FILE__ << " line: " << __LINE__ << " msg: " << msg << endl

Uint64
Ndb::getAutoIncrementValue(const char* aTableName, Uint32 cacheSize)
{
  DEBUG_TRACE("getAutoIncrementValue");
  const NdbTableImpl* table = theDictionary->getTable(aTableName);
  if (table == 0)
    return ~0;
  Uint64 tupleId = getTupleIdFromNdb(table->m_tableId, cacheSize);
  return tupleId;
}

Uint64
Ndb::getAutoIncrementValue(const NdbDictionary::Table * aTable, Uint32 cacheSize)
{
  DEBUG_TRACE("getAutoIncrementValue");
  if (aTable == 0)
    return ~0;
  const NdbTableImpl* table = & NdbTableImpl::getImpl(*aTable);
  Uint64 tupleId = getTupleIdFromNdb(table->m_tableId, cacheSize);
  return tupleId;
}

Uint64 
Ndb::getTupleIdFromNdb(const char* aTableName, Uint32 cacheSize)
{
  const NdbTableImpl* table = theDictionary->getTable(aTableName);
  if (table == 0)
    return ~0;
  return getTupleIdFromNdb(table->m_tableId, cacheSize);
}

Uint64
Ndb::getTupleIdFromNdb(Uint32 aTableId, Uint32 cacheSize)
{
  if ( theFirstTupleId[aTableId] != theLastTupleId[aTableId] )
  {
    theFirstTupleId[aTableId]++;
    return theFirstTupleId[aTableId];
  }
  else // theFirstTupleId == theLastTupleId
  {
    return opTupleIdOnNdb(aTableId, cacheSize, 0);
  }
}

Uint64
Ndb::readAutoIncrementValue(const char* aTableName)
{
  DEBUG_TRACE("readtAutoIncrementValue");
  const NdbTableImpl* table = theDictionary->getTable(aTableName);
  if (table == 0) {
    theError= theDictionary->getNdbError();
    return ~0;
  }
  Uint64 tupleId = readTupleIdFromNdb(table->m_tableId);
  return tupleId;
}

Uint64
Ndb::readAutoIncrementValue(const NdbDictionary::Table * aTable)
{
  DEBUG_TRACE("readtAutoIncrementValue");
  if (aTable == 0)
    return ~0;
  const NdbTableImpl* table = & NdbTableImpl::getImpl(*aTable);
  Uint64 tupleId = readTupleIdFromNdb(table->m_tableId);
  return tupleId;
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
  DEBUG_TRACE("setAutoIncrementValue " << val);
  const NdbTableImpl* table = theDictionary->getTable(aTableName);
  if (table == 0) {
    theError= theDictionary->getNdbError();
    return false;
  }
  return setTupleIdInNdb(table->m_tableId, val, increase);
}

bool
Ndb::setAutoIncrementValue(const NdbDictionary::Table * aTable, Uint64 val, bool increase)
{
  DEBUG_TRACE("setAutoIncrementValue " << val);
  if (aTable == 0)
    return ~0;
  const NdbTableImpl* table = & NdbTableImpl::getImpl(*aTable);
  return setTupleIdInNdb(table->m_tableId, val, increase);
}

bool 
Ndb::setTupleIdInNdb(const char* aTableName, Uint64 val, bool increase )
{
  DEBUG_TRACE("setTupleIdInNdb");
  const NdbTableImpl* table = theDictionary->getTable(aTableName);
  if (table == 0) {
    theError= theDictionary->getNdbError();
    return false;
  }
  return setTupleIdInNdb(table->m_tableId, val, increase);
}

bool
Ndb::setTupleIdInNdb(Uint32 aTableId, Uint64 val, bool increase )
{
  DEBUG_TRACE("setTupleIdInNdb");
  if (increase)
  {
    if (theFirstTupleId[aTableId] != theLastTupleId[aTableId])
    {
      // We have a cache sequence
      if (val <= theFirstTupleId[aTableId]+1)
	return false;
      if (val <= theLastTupleId[aTableId])
      {
	theFirstTupleId[aTableId] = val - 1;
	return true;
      }
      // else continue;
    }    
    return (opTupleIdOnNdb(aTableId, val, 2) == val);
  }
  else
    return (opTupleIdOnNdb(aTableId, val, 1) == val);
}

Uint64
Ndb::opTupleIdOnNdb(Uint32 aTableId, Uint64 opValue, Uint32 op)
{
  DEBUG_TRACE("opTupleIdOnNdb");

  NdbConnection*     tConnection;
  NdbOperation*      tOperation;
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

  return ret;

  error_handler:
    theError.code = tConnection->theError.code;
    this->closeTransaction(tConnection);
  error_return:
    // Restore current name space
    setDatabaseName(currentDb.c_str());
    setDatabaseSchemaName(currentSchema.c_str());

  return ~0;
}

static const Uint32 MAX_KEY_LEN_64_WORDS = 4;
static const Uint32 MAX_KEY_LEN_32_WORDS = 8;
static const Uint32 MAX_KEY_LEN_BYTES    = 32;

Uint32
Ndb::computeFragmentId(const char * keyData, Uint32 keyLen)
{
  Uint64 tempData[MAX_KEY_LEN_64_WORDS];
  
  const Uint32 usedKeyLen = (keyLen + 3) >> 2; // In words
  const char * usedKeyData = 0;
  
  /**
   * If   key data buffer is not aligned (on 64 bit boundary)
   *   or key len is not a multiple of 4
   * Use temp data
   */
  if(((((UintPtr)keyData) & 7) == 0) && ((keyLen & 3) == 0)) {
    usedKeyData = keyData;
  } else {
    memcpy(&tempData[0], keyData, keyLen);
    const int slack = keyLen & 3;
    if(slack > 0) {
      memset(&((char *)&tempData[0])[keyLen], 0, (4 - slack));
    }//if
    usedKeyData = (char *)&tempData[0];
  }//if
  
  Uint32 hashValue = md5_hash((Uint64 *)usedKeyData, usedKeyLen);

  hashValue >>= startTransactionNodeSelectionData.kValue;
  return getFragmentId(hashValue);
}//Ndb::computeFragmentId()

Uint32
Ndb::getFragmentId(Uint32 hashValue)
{
  Uint32 fragmentId = hashValue &
    startTransactionNodeSelectionData.hashValueMask;
  if(fragmentId < startTransactionNodeSelectionData.hashpointerValue) {
    fragmentId = hashValue &
                 ((startTransactionNodeSelectionData.hashValueMask << 1) + 1);
  }//if
  return fragmentId;
}

Uint32
Ndb::guessPrimaryNode(Uint32 fragmentId){
  //ASSERT(((fragmentId > 0) && fragmentId <
  // startTransactionNodeSelectionData.noOfFragments), "Invalid fragementId");

  return startTransactionNodeSelectionData.fragment2PrimaryNodeMap[fragmentId];
}

void
Ndb::StartTransactionNodeSelectionData::init(Uint32 noOfNodes,
                                             Uint32 nodeIds[]) {
  kValue           = 6;
  noOfFragments    = 2 * noOfNodes;

  /**
   * Compute hashValueMask and hashpointerValue
   */
  {
    Uint32 topBit = (1 << 31);
    for(int i = 31; i>=0; i--){
      if((noOfFragments & topBit) != 0)
	break;
      topBit >>= 1;
    }
    hashValueMask    = topBit - 1;
    hashpointerValue = noOfFragments - (hashValueMask + 1);
  }
  
  /**
   * This initialization depends on
   * the fact that:
   *  primary node for fragment i = i % noOfNodes
   *
   * This algorithm should be implemented in Dbdih
   */
  {
    if (fragment2PrimaryNodeMap != 0)
      abort();

    fragment2PrimaryNodeMap = new Uint32[noOfFragments];
    Uint32 i;  
    for(i = 0; i<noOfNodes; i++){
      fragment2PrimaryNodeMap[i] = nodeIds[i];
    }
    
    // Sort them (bubble sort)
    for(i = 0; i<noOfNodes-1; i++)
      for(Uint32 j = i+1; j<noOfNodes; j++)
	if(fragment2PrimaryNodeMap[i] > fragment2PrimaryNodeMap[j]){
	  Uint32 tmp = fragment2PrimaryNodeMap[i];
	  fragment2PrimaryNodeMap[i] = fragment2PrimaryNodeMap[j];
	  fragment2PrimaryNodeMap[j] = tmp;
	}
    
    for(i = 0; i<noOfNodes; i++){
      fragment2PrimaryNodeMap[i+noOfNodes] = fragment2PrimaryNodeMap[i];
    }
  }
}

void
Ndb::StartTransactionNodeSelectionData::release(){
  delete [] fragment2PrimaryNodeMap;
  fragment2PrimaryNodeMap = 0;
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
  return theDataBase;
}
 
void Ndb::setCatalogName(const char * a_catalog_name)
{
  if (a_catalog_name) {
    snprintf(theDataBase, sizeof(theDataBase), "%s",
             a_catalog_name ? a_catalog_name : "");
    
    int len = snprintf(prefixName, sizeof(prefixName), "%s%c%s%c",
                       theDataBase, table_name_separator,
                       theDataBaseSchema, table_name_separator);
    prefixEnd = prefixName + (len < (int) sizeof(prefixName) ? len : 
                              sizeof(prefixName) - 1);
  }
}
 
const char * Ndb::getSchemaName() const
{
  return theDataBaseSchema;
}
 
void Ndb::setSchemaName(const char * a_schema_name)
{
  if (a_schema_name) {
    snprintf(theDataBaseSchema, sizeof(theDataBase), "%s",
             a_schema_name ? a_schema_name : "");

    int len = snprintf(prefixName, sizeof(prefixName), "%s%c%s%c",
                       theDataBase, table_name_separator,
                       theDataBaseSchema, table_name_separator);
    prefixEnd = prefixName + (len < (int) sizeof(prefixName) ? len : 
                              sizeof(prefixName) - 1);
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

const char *
Ndb::internalizeTableName(const char * externalTableName)
{
  if (fullyQualifiedNames) {
    strncpy(prefixEnd, externalTableName, NDB_MAX_TAB_NAME_SIZE);
    return prefixName;
  }
  else
    return externalTableName;
}
 
const char *
Ndb::internalizeIndexName(const NdbTableImpl * table,
                          const char * externalIndexName)
{
  if (fullyQualifiedNames) {
    char tableId[10];
    sprintf(tableId, "%d", table->m_tableId);
    Uint32 tabIdLen = strlen(tableId);
    strncpy(prefixEnd, tableId, tabIdLen);
    prefixEnd[tabIdLen] = table_name_separator;
    strncpy(prefixEnd + tabIdLen + 1, 
	    externalIndexName, NDB_MAX_TAB_NAME_SIZE);
    return prefixName;
  }
  else
    return externalIndexName;
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

NdbEventOperation* Ndb::createEventOperation(const char* eventName,
					     const int bufferLength)
{
  NdbEventOperation* tOp;

  tOp = new NdbEventOperation(this, eventName, bufferLength);

  if (tOp->getState() != NdbEventOperation::CREATED) {
    delete tOp;
    tOp = NULL;
  }

  //now we have to look up this event in dict

  return tOp;
}

int Ndb::dropEventOperation(NdbEventOperation* op) {
  delete op;
  return 0;
}

NdbGlobalEventBufferHandle* Ndb::getGlobalEventBufferHandle()
{
  return theGlobalEventBufferHandle;
}

//void Ndb::monitorEvent(NdbEventOperation *op, NdbEventCallback cb, void* rs)
//{
//}

int
Ndb::pollEvents(int aMillisecondNumber)
{
  return NdbEventOperation::wait(theGlobalEventBufferHandle,
				 aMillisecondNumber);
}

#ifdef VM_TRACE
#include <NdbMutex.h>
static NdbMutex print_state_mutex = NDB_MUTEX_INITIALIZER;
static bool
checkdups(NdbConnection** list, unsigned no)
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
  NdbMutex_Lock(&print_state_mutex);
  bool dups = false;
  ndbout << buf << " ndb=" << hex << this << dec;
#ifndef NDB_WIN32
  ndbout << " thread=" << (int)pthread_self();
#endif
  ndbout << endl;
  for (unsigned n = 0; n < MAX_NDB_NODES; n++) {
    NdbConnection* con = theConnectionArray[n];
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
  for (unsigned i = 0; i < theNoOfPreparedTransactions; i++)
    thePreparedTransactionsArray[i]->printState();
  ndbout << "sent: " << theNoOfSentTransactions<< endl;
  if (checkdups(theSentTransactionsArray, theNoOfSentTransactions)) {
    ndbout << "!! DUPS !!" << endl;
    dups = true;
  }
  for (unsigned i = 0; i < theNoOfSentTransactions; i++)
    theSentTransactionsArray[i]->printState();
  ndbout << "completed: " << theNoOfCompletedTransactions<< endl;
  if (checkdups(theCompletedTransactionsArray, theNoOfCompletedTransactions)) {
    ndbout << "!! DUPS !!" << endl;
    dups = true;
  }
  for (unsigned i = 0; i < theNoOfCompletedTransactions; i++)
    theCompletedTransactionsArray[i]->printState();
  NdbMutex_Unlock(&print_state_mutex);
}
#endif



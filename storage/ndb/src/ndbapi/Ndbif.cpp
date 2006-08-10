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


#include <ndb_global.h>

#include "NdbApiSignal.hpp"
#include "NdbImpl.hpp"
#include <NdbTransaction.hpp>
#include <NdbOperation.hpp>
#include <NdbIndexOperation.hpp>
#include <NdbScanOperation.hpp>
#include <NdbRecAttr.hpp>
#include <NdbReceiver.hpp>
#include "API.hpp"
#include "NdbEventOperationImpl.hpp"

#include <signaldata/TcCommit.hpp>
#include <signaldata/TcKeyFailConf.hpp>
#include <signaldata/TcKeyConf.hpp>
#include <signaldata/TestOrd.hpp>
#include <signaldata/CreateIndx.hpp>
#include <signaldata/DropIndx.hpp>
#include <signaldata/TcIndx.hpp>
#include <signaldata/TransIdAI.hpp>
#include <signaldata/ScanFrag.hpp>
#include <signaldata/ScanTab.hpp>
#include <signaldata/SumaImpl.hpp>

#include <ndb_limits.h>
#include <NdbOut.hpp>
#include <NdbTick.h>

#include <EventLogger.hpp>
extern EventLogger g_eventLogger;

/******************************************************************************
 * int init( int aNrOfCon, int aNrOfOp );
 *
 * Return Value:   Return 0 : init was successful.
 *                Return -1: In all other case.  
 * Parameters:	aNrOfCon : Number of connections offered to the application.
 *		aNrOfOp : Number of operations offered to the application.
 * Remark:		Create pointers and idle list Synchronous.
 ****************************************************************************/ 
int
Ndb::init(int aMaxNoOfTransactions)
{
  DBUG_ENTER("Ndb::init");

  int i;
  int aNrOfCon;
  int aNrOfOp;
  int tMaxNoOfTransactions;
  NdbApiSignal* tSignal[16];	// Initiate free list of 16 signal objects
  if (theInitState != NotInitialised) {
    switch(theInitState){
    case InitConfigError:
      theError.code = 4117;
      break;
    default:
      theError.code = 4104;
      break;
    }
    DBUG_RETURN(-1);
  }//if
  theInitState = StartingInit;
  TransporterFacade * theFacade =  theImpl->m_transporter_facade;
  theFacade->lock_mutex();
  
  const int tBlockNo = theFacade->open(this,
                                       executeMessage, 
                                       statusMessage);  
  if ( tBlockNo == -1 ) {
    theError.code = 4105;
    theFacade->unlock_mutex();
    DBUG_RETURN(-1); // no more free blocknumbers
  }//if
  
  theNdbBlockNumber = tBlockNo;

  theFacade->unlock_mutex();
  
  theDictionary->setTransporter(this, theFacade);
  
  aNrOfCon = theImpl->theNoOfDBnodes;
  aNrOfOp = 2*theImpl->theNoOfDBnodes;
  
  // Create connection object in a linked list 
  if((createConIdleList(aNrOfCon)) == -1){
    theError.code = 4000;
    goto error_handler;
  }
  
  // Create operations in a linked list
  if((createOpIdleList(aNrOfOp)) == -1){       
    theError.code = 4000;
    goto error_handler;
  }
  

  tMaxNoOfTransactions = aMaxNoOfTransactions;
  theMaxNoOfTransactions = tMaxNoOfTransactions;
  theRemainingStartTransactions= tMaxNoOfTransactions;  
  thePreparedTransactionsArray = new NdbTransaction* [tMaxNoOfTransactions];
  theSentTransactionsArray = new NdbTransaction* [tMaxNoOfTransactions];
  theCompletedTransactionsArray = new NdbTransaction* [tMaxNoOfTransactions];
  
  if ((thePreparedTransactionsArray == NULL) ||
      (theSentTransactionsArray == NULL) ||
      (theCompletedTransactionsArray == NULL)) {
    goto error_handler;
  }//if
  
  for (i = 0; i < tMaxNoOfTransactions; i++) {
    thePreparedTransactionsArray[i] = NULL;
    theSentTransactionsArray[i] = NULL;
    theCompletedTransactionsArray[i] = NULL;
  }//for     
  for (i = 0; i < 16; i++){
    tSignal[i] = getSignal();
    if(tSignal[i] == NULL) {
      theError.code = 4000;
      goto error_handler;
    }
  }
  for (i = 0; i < 16; i++)
    releaseSignal(tSignal[i]);
  theInitState = Initialised; 
  DBUG_RETURN(0);
  
error_handler:
  ndbout << "error_handler" << endl;
  releaseTransactionArrays();
  delete theDictionary;
  theImpl->m_transporter_facade->close(theNdbBlockNumber, 0);
  DBUG_RETURN(-1);
}

void
Ndb::releaseTransactionArrays()
{
  DBUG_ENTER("Ndb::releaseTransactionArrays");
  if (thePreparedTransactionsArray != NULL) {
    delete [] thePreparedTransactionsArray;
  }//if
  if (theSentTransactionsArray != NULL) {
    delete [] theSentTransactionsArray;
  }//if
  if (theCompletedTransactionsArray != NULL) {
    delete [] theCompletedTransactionsArray;
  }//if
  DBUG_VOID_RETURN;
}//Ndb::releaseTransactionArrays()

void
Ndb::executeMessage(void* NdbObject,
                    NdbApiSignal * aSignal,
                    LinearSectionPtr ptr[3])
{
  Ndb* tNdb = (Ndb*)NdbObject;
  tNdb->handleReceivedSignal(aSignal, ptr);
}

void Ndb::connected(Uint32 ref)
{
// cluster connect, a_node == own reference
  theMyRef= ref;
  Uint32 tmpTheNode= refToNode(ref);
  Uint64 tBlockNo= refToBlock(ref);
  if (theNdbBlockNumber >= 0){
    assert(theMyRef == numberToRef(theNdbBlockNumber, tmpTheNode));
  }
  
  TransporterFacade * theFacade =  theImpl->m_transporter_facade;
  int i, n= 0;
  for (i = 1; i < MAX_NDB_NODES; i++){
    if (theFacade->getIsDbNode(i)){
      theImpl->theDBnodes[n] = i;
      n++;
    }
  }
  theImpl->theNoOfDBnodes = n;
  
  theFirstTransId = ((Uint64)tBlockNo << 52)+
    ((Uint64)tmpTheNode << 40);
  theFirstTransId += theFacade->m_max_trans_id;
  //      assert(0);
  DBUG_PRINT("info",("connected with ref=%x, id=%d, no_db_nodes=%d, first_trans_id=%lx",
		     theMyRef,
		     tmpTheNode,
		     theImpl->theNoOfDBnodes,
		     theFirstTransId));
  theCommitAckSignal = new NdbApiSignal(theMyRef);

  theDictionary->m_receiver.m_reference= theMyRef;
  theNode= tmpTheNode; // flag that Ndb object is initialized
}

void Ndb::report_node_connected(Uint32 nodeId)
{
  if (theEventBuffer)
  {
    // node connected
    // eventOperations in the ndb object should be notified
    theEventBuffer->report_node_connected(nodeId);
  }
}

void
Ndb::statusMessage(void* NdbObject, Uint32 a_node, bool alive, bool nfComplete)
{
  DBUG_ENTER("Ndb::statusMessage");
  DBUG_PRINT("info", ("a_node: %u  alive: %u  nfComplete: %u",
                      a_node, alive, nfComplete));
  Ndb* tNdb = (Ndb*)NdbObject;
  if (alive) {
    if (nfComplete) {
      // cluster connect, a_node == own reference
      tNdb->connected(a_node);
      DBUG_VOID_RETURN;
    }//if
    tNdb->report_node_connected(a_node);
  } else {
    if (nfComplete) {
      tNdb->report_node_failure_completed(a_node);
    } else {
      tNdb->report_node_failure(a_node);
    }//if
  }//if
  NdbDictInterface::execNodeStatus(&tNdb->theDictionary->m_receiver,
				   a_node, alive, nfComplete);
  DBUG_VOID_RETURN;
}

void
Ndb::report_node_failure(Uint32 node_id)
{
  /**
   * We can only set the state here since this object can execute 
   * simultaneously. 
   * 
   * This method is only called by ClusterMgr (via lots of methods)
   */

  theImpl->the_release_ind[node_id] = 1;
  // must come after
  theImpl->the_release_ind[0] = 1;
  theImpl->theWaiter.nodeFail(node_id);
  return;
}//Ndb::report_node_failure()


void
Ndb::report_node_failure_completed(Uint32 node_id)
{
  if (theEventBuffer)
  {
    // node failed
    // eventOperations in the ndb object should be notified
    theEventBuffer->report_node_failure(node_id);
    if(!theImpl->m_transporter_facade->theClusterMgr->isClusterAlive())
    {
      // cluster is unavailable, 
      // eventOperations in the ndb object should be notified
      theEventBuffer->completeClusterFailed();
    }
  }
  
  abortTransactionsAfterNodeFailure(node_id);

}//Ndb::report_node_failure_completed()

/***************************************************************************
void abortTransactionsAfterNodeFailure();

Remark:   Abort all transactions in theSentTransactionsArray after connection 
          to one node has failed
****************************************************************************/
void	
Ndb::abortTransactionsAfterNodeFailure(Uint16 aNodeId)
{  
  Uint32 tNoSentTransactions = theNoOfSentTransactions;
  for (int i = tNoSentTransactions - 1; i >= 0; i--) {
    NdbTransaction* localCon = theSentTransactionsArray[i];
    if (localCon->getConnectedNodeId() == aNodeId) {
      const NdbTransaction::SendStatusType sendStatus = localCon->theSendStatus;
      if (sendStatus == NdbTransaction::sendTC_OP || 
	  sendStatus == NdbTransaction::sendTC_COMMIT) {
        /*
        A transaction was interrupted in the prepare phase by a node
        failure. Since the transaction was not found in the phase
        after the node failure it cannot have been committed and
        we report a normal node failure abort.
        */
	localCon->setOperationErrorCodeAbort(4010);
        localCon->theCompletionStatus = NdbTransaction::CompletedFailure;
      } else if (sendStatus == NdbTransaction::sendTC_ROLLBACK) {
        /*
        We aimed for abort and abort we got even if it was by a node
        failure. We will thus report it as a success.
        */
        localCon->theCompletionStatus = NdbTransaction::CompletedSuccess;
      } else {
#ifdef VM_TRACE
        printState("abortTransactionsAfterNodeFailure %x", this);
        abort();
#endif
      }
      /*
      All transactions arriving here have no connection to the kernel
      intact since the node was failing and they were aborted. Thus we
      set commit state to Aborted and set state to release on close.
      */
      localCon->theReturnStatus = NdbTransaction::ReturnFailure;
      localCon->theCommitStatus = NdbTransaction::Aborted;
      localCon->theReleaseOnClose = true;
      completedTransaction(localCon);
    }
    else if(localCon->report_node_failure(aNodeId))
    {
      completedTransaction(localCon);
    }
  }//for
  return;
}//Ndb::abortTransactionsAfterNodeFailure()

/****************************************************************************
void handleReceivedSignal(NdbApiSignal* aSignal);

Parameters:     aSignal: The signal object.
Remark:         Send all operations belonging to this connection. 
*****************************************************************************/
void	
Ndb::handleReceivedSignal(NdbApiSignal* aSignal, LinearSectionPtr ptr[3])
{
  NdbOperation* tOp;
  NdbIndexOperation* tIndexOp;
  NdbTransaction* tCon;
  int tReturnCode = -1;
  const Uint32* tDataPtr = aSignal->getDataPtr();
  const Uint32 tWaitState = theImpl->theWaiter.m_state;
  const Uint32 tSignalNumber = aSignal->readSignalNumber();
  const Uint32 tFirstData = *tDataPtr;
  const Uint32 tLen = aSignal->getLength();
  void * tFirstDataPtr;
  NdbWaiter *t_waiter;

  /*
    In order to support 64 bit processes in the application we need to use
    id's rather than a direct pointer to the object used. It is also a good
    idea that one cannot corrupt the application code by sending a corrupt
    memory pointer.
    
    All signals received by the API requires the first data word to be such
    an id to the receiving object.
  */
  
  switch (tSignalNumber){
  case GSN_TCKEYCONF:
    {
      tFirstDataPtr = int2void(tFirstData);
      if (tFirstDataPtr == 0) goto InvalidSignal;

      const TcKeyConf * const keyConf = (TcKeyConf *)tDataPtr;
      const BlockReference aTCRef = aSignal->theSendersBlockRef;

      tCon = void2con(tFirstDataPtr);
      if ((tCon->checkMagicNumber() == 0) &&
          (tCon->theSendStatus == NdbTransaction::sendTC_OP)) {
        tReturnCode = tCon->receiveTCKEYCONF(keyConf, tLen);
        if (tReturnCode != -1) {
          completedTransaction(tCon);
        }//if

	if(TcKeyConf::getMarkerFlag(keyConf->confInfo)){
	  NdbTransaction::sendTC_COMMIT_ACK(theImpl->m_transporter_facade,
                                            theCommitAckSignal,
                                            keyConf->transId1, 
                                            keyConf->transId2,
                                            aTCRef);
	}
      
	return;
      }//if
      goto InvalidSignal;
      
      return;
    }
  case GSN_TRANSID_AI:{
    tFirstDataPtr = int2void(tFirstData);
    NdbReceiver* tRec;
    if (tFirstDataPtr && (tRec = void2rec(tFirstDataPtr)) && 
	tRec->checkMagicNumber() && (tCon = tRec->getTransaction()) &&
	tCon->checkState_TransId(((const TransIdAI*)tDataPtr)->transId)){
      Uint32 com;
      if(aSignal->m_noOfSections > 0){
	com = tRec->execTRANSID_AI(ptr[0].p, ptr[0].sz);
      } else {
	com = tRec->execTRANSID_AI(tDataPtr + TransIdAI::HeaderLength, 
				   tLen - TransIdAI::HeaderLength);
      }

      if(com == 0)
	return;
      
      switch(tRec->getType()){
      case NdbReceiver::NDB_OPERATION:
      case NdbReceiver::NDB_INDEX_OPERATION:
	if(tCon->OpCompleteSuccess() != -1){
	  completedTransaction(tCon);
	}
	return;
      case NdbReceiver::NDB_SCANRECEIVER:
	tCon->theScanningOp->receiver_delivered(tRec);
	theImpl->theWaiter.m_state = (((WaitSignalType) tWaitState) == WAIT_SCAN ? 
				      (Uint32) NO_WAIT : tWaitState);
	break;
      default:
	goto InvalidSignal;
      }
      break;
    } else {
      /**
       * This is ok as transaction can have been aborted before TRANSID_AI
       * arrives (if TUP on  other node than TC)
       */
      return;
    }
  }
  case GSN_TCKEY_FAILCONF:
    {
      tFirstDataPtr = int2void(tFirstData);
      const TcKeyFailConf * failConf = (TcKeyFailConf *)tDataPtr;
      const BlockReference aTCRef = aSignal->theSendersBlockRef;
      if (tFirstDataPtr != 0){
	tOp = void2rec_op(tFirstDataPtr);
	
	if (tOp->checkMagicNumber(false) == 0) {
	  tCon = tOp->theNdbCon;
	  if (tCon != NULL) {
	    if ((tCon->theSendStatus == NdbTransaction::sendTC_OP) ||
		(tCon->theSendStatus == NdbTransaction::sendTC_COMMIT)) {
	      tReturnCode = tCon->receiveTCKEY_FAILCONF(failConf);
	      if (tReturnCode != -1) {
		completedTransaction(tCon);
	      }//if
	    }//if
	  }
	}
      } else {
#ifdef VM_TRACE
	ndbout_c("Recevied TCKEY_FAILCONF wo/ operation");
#endif
      }
      if(tFirstData & 1){
	NdbTransaction::sendTC_COMMIT_ACK(theImpl->m_transporter_facade,
                                          theCommitAckSignal,
                                          failConf->transId1, 
                                          failConf->transId2,
                                          aTCRef);
      }
      return;
    }
  case GSN_TCKEY_FAILREF:
    {
      tFirstDataPtr = int2void(tFirstData);
      if(tFirstDataPtr != 0){
	tOp = void2rec_op(tFirstDataPtr);
	if (tOp->checkMagicNumber(false) == 0) {
	  tCon = tOp->theNdbCon;
	  if (tCon != NULL) {
	    if ((tCon->theSendStatus == NdbTransaction::sendTC_OP) ||
		(tCon->theSendStatus == NdbTransaction::sendTC_ROLLBACK)) {
	      tReturnCode = tCon->receiveTCKEY_FAILREF(aSignal);
	      if (tReturnCode != -1) {
		completedTransaction(tCon);
		return;
	      }//if
	    }//if
	  }//if
	}//if
      }
#ifdef VM_TRACE
      ndbout_c("Recevied TCKEY_FAILREF wo/ operation");
#endif
      return;
      return;
    }
  case GSN_TCKEYREF:
    {
      tFirstDataPtr = int2void(tFirstData);
      if (tFirstDataPtr == 0) goto InvalidSignal;

      tOp = void2rec_op(tFirstDataPtr);
      if (tOp->checkMagicNumber() == 0) {
	tCon = tOp->theNdbCon;
	if (tCon != NULL) {
	  if (tCon->theSendStatus == NdbTransaction::sendTC_OP) {
	    tReturnCode = tOp->receiveTCKEYREF(aSignal);
	    if (tReturnCode != -1) {
	      completedTransaction(tCon);
	      return;
	    }//if
	    break;
	  }//if
	}//if
      } //if
      goto InvalidSignal;
      return;
    } 
  case GSN_TC_COMMITCONF:
    {
      tFirstDataPtr = int2void(tFirstData);
      if (tFirstDataPtr == 0) goto InvalidSignal;

      const TcCommitConf * const commitConf = (TcCommitConf *)tDataPtr;
      const BlockReference aTCRef = aSignal->theSendersBlockRef;
      
      tCon = void2con(tFirstDataPtr);
      if ((tCon->checkMagicNumber() == 0) &&
	  (tCon->theSendStatus == NdbTransaction::sendTC_COMMIT)) {
	tReturnCode = tCon->receiveTC_COMMITCONF(commitConf);
	if (tReturnCode != -1) {
	  completedTransaction(tCon);
	}//if

	if(tFirstData & 1){
	  NdbTransaction::sendTC_COMMIT_ACK(theImpl->m_transporter_facade,
                                            theCommitAckSignal,
                                            commitConf->transId1, 
                                            commitConf->transId2,
                                            aTCRef);
	}
	return;
      }
      goto InvalidSignal;
      return;
    }

  case GSN_TC_COMMITREF:
    {
      tFirstDataPtr = int2void(tFirstData);
      if (tFirstDataPtr == 0) goto InvalidSignal;

      tCon = void2con(tFirstDataPtr);
      if ((tCon->checkMagicNumber() == 0) &&
	  (tCon->theSendStatus == NdbTransaction::sendTC_COMMIT)) {
	tReturnCode = tCon->receiveTC_COMMITREF(aSignal);
	if (tReturnCode != -1) {
	  completedTransaction(tCon);
	}//if
      }//if
      return;
    }
  case GSN_TCROLLBACKCONF:
    {
      tFirstDataPtr = int2void(tFirstData);
      if (tFirstDataPtr == 0) goto InvalidSignal;

      tCon = void2con(tFirstDataPtr);
      if ((tCon->checkMagicNumber() == 0) &&
	  (tCon->theSendStatus == NdbTransaction::sendTC_ROLLBACK)) {
	tReturnCode = tCon->receiveTCROLLBACKCONF(aSignal);
	if (tReturnCode != -1) {
	  completedTransaction(tCon);
	}//if
      }//if
      return;
    }
  case GSN_TCROLLBACKREF:
    {
      tFirstDataPtr = int2void(tFirstData);
      if (tFirstDataPtr == 0) goto InvalidSignal;

      tCon = void2con(tFirstDataPtr);
      if ((tCon->checkMagicNumber() == 0) &&
	  (tCon->theSendStatus == NdbTransaction::sendTC_ROLLBACK)) {
	tReturnCode = tCon->receiveTCROLLBACKREF(aSignal);
	if (tReturnCode != -1) {
	  completedTransaction(tCon);
	}//if
      }//if
      return;
    }
  case GSN_TCROLLBACKREP:
    {
      tFirstDataPtr = int2void(tFirstData);
      if (tFirstDataPtr == 0) goto InvalidSignal;

      tCon = void2con(tFirstDataPtr);
      if (tCon->checkMagicNumber() == 0) {
	tReturnCode = tCon->receiveTCROLLBACKREP(aSignal);
	if (tReturnCode != -1) {
	  completedTransaction(tCon);
	}//if
      }//if
      return;
    }
  case GSN_TCSEIZECONF:
    {
      tFirstDataPtr = int2void(tFirstData);
      if (tFirstDataPtr == 0) goto InvalidSignal;

      if (tWaitState != WAIT_TC_SEIZE) {
	goto InvalidSignal;
      }//if
      tCon = void2con(tFirstDataPtr);
      if (tCon->checkMagicNumber() != 0) {
	goto InvalidSignal;
      }//if
      tReturnCode = tCon->receiveTCSEIZECONF(aSignal);
      if (tReturnCode != -1) {
	theImpl->theWaiter.m_state = NO_WAIT;
      } else {
	goto InvalidSignal;
      }//if
      break;
    }
  case GSN_TCSEIZEREF:
    {
      tFirstDataPtr = int2void(tFirstData);
      if (tFirstDataPtr == 0) goto InvalidSignal;

      if (tWaitState != WAIT_TC_SEIZE) {
	return;
      }//if
      tCon = void2con(tFirstDataPtr);
      if (tCon->checkMagicNumber() != 0) {
	return;
      }//if
      tReturnCode = tCon->receiveTCSEIZEREF(aSignal);
      if (tReturnCode != -1) {
	theImpl->theWaiter.m_state = NO_WAIT;
      } else {
        return;
      }//if
      break;
    }
  case GSN_TCRELEASECONF:
    {
      tFirstDataPtr = int2void(tFirstData);
      if (tFirstDataPtr == 0) goto InvalidSignal;

      if (tWaitState != WAIT_TC_RELEASE) {
	goto InvalidSignal;
      }//if
      tCon = void2con(tFirstDataPtr);
      if (tCon->checkMagicNumber() != 0) {
	goto InvalidSignal;
      }//if
      tReturnCode = tCon->receiveTCRELEASECONF(aSignal);
      if (tReturnCode != -1) {
	theImpl->theWaiter.m_state = NO_WAIT;
      }//if
      break;
    } 
  case GSN_TCRELEASEREF:
    {
      tFirstDataPtr = int2void(tFirstData);
      if (tFirstDataPtr == 0) goto InvalidSignal;

      if (tWaitState != WAIT_TC_RELEASE) {
	goto InvalidSignal;
      }//if
      tCon = void2con(tFirstDataPtr);
      if (tCon->checkMagicNumber() != 0) {
	goto InvalidSignal;
      }//if
      tReturnCode = tCon->receiveTCRELEASEREF(aSignal);
      if (tReturnCode != -1) {
	theImpl->theWaiter.m_state = NO_WAIT;
      }//if
      break;
    }
      
  case GSN_GET_TABINFOREF:
  case GSN_GET_TABINFO_CONF:
  case GSN_CREATE_TABLE_REF:
  case GSN_CREATE_TABLE_CONF:
  case GSN_DROP_TABLE_CONF:
  case GSN_DROP_TABLE_REF:
  case GSN_ALTER_TABLE_CONF:
  case GSN_ALTER_TABLE_REF:
  case GSN_CREATE_INDX_CONF:
  case GSN_CREATE_INDX_REF:
  case GSN_DROP_INDX_CONF:
  case GSN_DROP_INDX_REF:
  case GSN_CREATE_EVNT_CONF:
  case GSN_CREATE_EVNT_REF:
  case GSN_DROP_EVNT_CONF:
  case GSN_DROP_EVNT_REF:
  case GSN_LIST_TABLES_CONF:
  case GSN_CREATE_FILE_REF:
  case GSN_CREATE_FILE_CONF:
  case GSN_CREATE_FILEGROUP_REF:
  case GSN_CREATE_FILEGROUP_CONF:
  case GSN_DROP_FILE_REF:
  case GSN_DROP_FILE_CONF:
  case GSN_DROP_FILEGROUP_REF:
  case GSN_DROP_FILEGROUP_CONF:
  case GSN_WAIT_GCP_CONF:
  case GSN_WAIT_GCP_REF:
    NdbDictInterface::execSignal(&theDictionary->m_receiver,
				 aSignal, ptr);
    return;
    
  case GSN_SUB_REMOVE_CONF:
  case GSN_SUB_REMOVE_REF:
    return; // ignore these signals
  case GSN_SUB_START_CONF:
  case GSN_SUB_START_REF:
  case GSN_SUB_STOP_CONF:
  case GSN_SUB_STOP_REF:
    NdbDictInterface::execSignal(&theDictionary->m_receiver,
				 aSignal, ptr);
    return;
  case GSN_SUB_GCP_COMPLETE_REP:
  {
    const SubGcpCompleteRep * const rep=
      CAST_CONSTPTR(SubGcpCompleteRep, aSignal->getDataPtr());
    theEventBuffer->execSUB_GCP_COMPLETE_REP(rep);
    return;
  }
  case GSN_SUB_TABLE_DATA:
  {
    const SubTableData * const sdata=
      CAST_CONSTPTR(SubTableData, aSignal->getDataPtr());
    const Uint32 oid = sdata->senderData;
    NdbEventOperationImpl *op= (NdbEventOperationImpl*)int2void(oid);

    if (unlikely(op == 0 || op->m_magic_number != NDB_EVENT_OP_MAGIC_NUMBER))
    {
      g_eventLogger.error("dropped GSN_SUB_TABLE_DATA due to wrong magic "
			  "number");
      return ;
    }

    // Accumulate DIC_TAB_INFO for TE_ALTER events
    if (sdata->operation == NdbDictionary::Event::_TE_ALTER &&
        !op->execSUB_TABLE_DATA(aSignal, ptr))
      return;

    for (int i= aSignal->m_noOfSections;i < 3; i++) {
      ptr[i].p = NULL;
      ptr[i].sz = 0;
    }
    DBUG_PRINT("info",("oid=senderData: %d, gci: %d, operation: %d, "
		       "tableId: %d",
		       sdata->senderData, sdata->gci, sdata->operation,
		       sdata->tableId));

    theEventBuffer->insertDataL(op,sdata, ptr);
    return;
  }
  case GSN_DIHNDBTAMPER:
    {
      tFirstDataPtr = int2void(tFirstData);
      if (tFirstDataPtr == 0) goto InvalidSignal;
      
      if (tWaitState != WAIT_NDB_TAMPER)
	return;
      tCon = void2con(tFirstDataPtr);
      if (tCon->checkMagicNumber() != 0)
	return;
      tReturnCode = tCon->receiveDIHNDBTAMPER(aSignal);
      if (tReturnCode != -1)
	theImpl->theWaiter.m_state = NO_WAIT;
      break;
    }
  case GSN_SCAN_TABCONF:
    {
      tFirstDataPtr = int2void(tFirstData);
      assert(tFirstDataPtr);
      assert(void2con(tFirstDataPtr));
      assert(void2con(tFirstDataPtr)->checkMagicNumber() == 0);
      if(tFirstDataPtr && 
	 (tCon = void2con(tFirstDataPtr)) && (tCon->checkMagicNumber() == 0)){
	
	if(aSignal->m_noOfSections > 0){
	  tReturnCode = tCon->receiveSCAN_TABCONF(aSignal, 
						  ptr[0].p, ptr[0].sz);
	} else {
	  tReturnCode = 
	    tCon->receiveSCAN_TABCONF(aSignal, 
				      tDataPtr + ScanTabConf::SignalLength, 
				      tLen - ScanTabConf::SignalLength);
	}
	if (tReturnCode != -1 && tWaitState == WAIT_SCAN)
	  theImpl->theWaiter.m_state = NO_WAIT;
	break;
      } else {
	goto InvalidSignal;
      }
    }
  case GSN_SCAN_TABREF:
    {
      tFirstDataPtr = int2void(tFirstData);
      if (tFirstDataPtr == 0) goto InvalidSignal;
      
      tCon = void2con(tFirstDataPtr);
      
      assert(tFirstDataPtr != 0 && 
	     void2con(tFirstDataPtr)->checkMagicNumber() == 0);
      
      if (tCon->checkMagicNumber() == 0){
	tReturnCode = tCon->receiveSCAN_TABREF(aSignal);
	if (tReturnCode != -1 && tWaitState == WAIT_SCAN){
	  theImpl->theWaiter.m_state = NO_WAIT;
	}
	break;
      }
      goto InvalidSignal;
    }
  case GSN_KEYINFO20: {
    tFirstDataPtr = int2void(tFirstData);
    NdbReceiver* tRec;
    if (tFirstDataPtr && (tRec = void2rec(tFirstDataPtr)) &&
	tRec->checkMagicNumber() && (tCon = tRec->getTransaction()) &&
	tCon->checkState_TransId(&((const KeyInfo20*)tDataPtr)->transId1)){
      
      Uint32 len = ((const KeyInfo20*)tDataPtr)->keyLen;
      Uint32 info = ((const KeyInfo20*)tDataPtr)->scanInfo_Node;
      int com = -1;
      if(aSignal->m_noOfSections > 0 && len == ptr[0].sz){
	com = tRec->execKEYINFO20(info, ptr[0].p, len);
      } else if(len == tLen - KeyInfo20::HeaderLength){
	com = tRec->execKEYINFO20(info, tDataPtr+KeyInfo20::HeaderLength, len);
      }
      
      switch(com){
      case 1:
	tCon->theScanningOp->receiver_delivered(tRec);
	theImpl->theWaiter.m_state = (((WaitSignalType) tWaitState) == WAIT_SCAN ? 
			      (Uint32) NO_WAIT : tWaitState);
	break;
      case 0:
	break;
      case -1:
	goto InvalidSignal;
      }
      break;
    } else {
      /**
       * This is ok as transaction can have been aborted before KEYINFO20
       * arrives (if TUP on  other node than TC)
       */
      return;
    }
  }
  case GSN_TCINDXCONF:{
    tFirstDataPtr = int2void(tFirstData);
    if (tFirstDataPtr == 0) goto InvalidSignal;

    const TcIndxConf * const indxConf = (TcIndxConf *)tDataPtr;
    const BlockReference aTCRef = aSignal->theSendersBlockRef;
    tCon = void2con(tFirstDataPtr);
    if ((tCon->checkMagicNumber() == 0) &&
	(tCon->theSendStatus == NdbTransaction::sendTC_OP)) {
      tReturnCode = tCon->receiveTCINDXCONF(indxConf, tLen);
      if (tReturnCode != -1) { 
	completedTransaction(tCon);
      }//if
    }//if
    
    if(TcIndxConf::getMarkerFlag(indxConf->confInfo)){
      NdbTransaction::sendTC_COMMIT_ACK(theImpl->m_transporter_facade,
                                        theCommitAckSignal,
                                        indxConf->transId1, 
                                        indxConf->transId2,
                                        aTCRef);
    }
    return;
  }
  case GSN_TCINDXREF:{
    tFirstDataPtr = int2void(tFirstData);
    if (tFirstDataPtr == 0) goto InvalidSignal;

    tIndexOp = void2rec_iop(tFirstDataPtr);
    if (tIndexOp->checkMagicNumber() == 0) {
      tCon = tIndexOp->theNdbCon;
      if (tCon != NULL) {
	if (tCon->theSendStatus == NdbTransaction::sendTC_OP) {
	  tReturnCode = tIndexOp->receiveTCINDXREF(aSignal);
	  if (tReturnCode != -1) {
	    completedTransaction(tCon);
	  }//if
	  return;
	}//if
      }//if
    }//if
    goto InvalidSignal;
    return;
  } 
  default:
    goto InvalidSignal;
  }//swich

  t_waiter= &theImpl->theWaiter;
  if (t_waiter->get_state() == NO_WAIT && tWaitState != NO_WAIT)
  {
    /*
      If our waiter object is the owner of the "poll rights", then we
      can simply return, we will return from this routine to the
      place where external_poll was called. From there it will move
      the "poll ownership" to a new thread if available.

      If our waiter object doesn't own the "poll rights", then we must
      signal the thread from where this waiter object called
      its conditional wait. This will wake up this thread so that it
      can continue its work.
    */
    TransporterFacade *tp= theImpl->m_transporter_facade;
    if (tp->get_poll_owner() != t_waiter)
    {
      /*
         Wake up the thread waiting for response and remove it from queue
         of objects waiting for receive completion
      */
      tp->remove_from_cond_wait_queue(t_waiter);
      t_waiter->cond_signal();
    }
  }//if
  return;

 InvalidSignal:
#ifdef VM_TRACE
  ndbout_c("Ndbif: Error Ndb::handleReceivedSignal "
	   "(GSN=%d, theImpl->theWaiter.m_state=%d)"
	   " sender = (Block: %d Node: %d)",
	   tSignalNumber,
	   tWaitState,
	   refToBlock(aSignal->theSendersBlockRef),
	   refToNode(aSignal->theSendersBlockRef));
#endif
#ifdef NDB_NO_DROPPED_SIGNAL
  abort();
#endif
  
  return;
}//Ndb::handleReceivedSignal()


/*****************************************************************************
void completedTransaction(NdbTransaction* aCon);

Remark:   One transaction has been completed.
          Remove it from send array and put it into the completed
          transaction array. Finally check if it is time to wake
          up a poller.
******************************************************************************/
void	
Ndb::completedTransaction(NdbTransaction* aCon)
{
  Uint32 tTransArrayIndex = aCon->theTransArrayIndex;
  Uint32 tNoSentTransactions = theNoOfSentTransactions;
  Uint32 tNoCompletedTransactions = theNoOfCompletedTransactions;
  if ((tNoSentTransactions > 0) && (aCon->theListState == NdbTransaction::InSendList) &&
      (tTransArrayIndex < tNoSentTransactions)) {
    NdbTransaction* tMoveCon = theSentTransactionsArray[tNoSentTransactions - 1];

    theCompletedTransactionsArray[tNoCompletedTransactions] = aCon;
    aCon->theTransArrayIndex = tNoCompletedTransactions;
    if (tMoveCon != aCon) {
      tMoveCon->theTransArrayIndex = tTransArrayIndex;
      theSentTransactionsArray[tTransArrayIndex] = tMoveCon;
    }//if
    theSentTransactionsArray[tNoSentTransactions - 1] = NULL;
    theNoOfCompletedTransactions = tNoCompletedTransactions + 1;

    theNoOfSentTransactions = tNoSentTransactions - 1;
    aCon->theListState = NdbTransaction::InCompletedList;
    aCon->handleExecuteCompletion();
    if ((theMinNoOfEventsToWakeUp != 0) &&
        (theNoOfCompletedTransactions >= theMinNoOfEventsToWakeUp)) {
      theMinNoOfEventsToWakeUp = 0;
      TransporterFacade *tp = theImpl->m_transporter_facade;
      NdbWaiter *t_waiter= &theImpl->theWaiter;
      if (tp->get_poll_owner() != t_waiter) {
        /*
          When we come here, this is executed by the thread owning the "poll
          rights". This thread is not where our waiter object belongs.
          Thus we wake up the thread owning this waiter object but first
          we must remove it from the conditional wait queue so that we
          don't assign it as poll owner later on.
        */
        tp->remove_from_cond_wait_queue(t_waiter);
        t_waiter->cond_signal();
      }
      return;
    }//if
  } else {
    ndbout << "theNoOfSentTransactions = " << (int) theNoOfSentTransactions;
    ndbout << " theListState = " << (int) aCon->theListState;
    ndbout << " theTransArrayIndex = " << aCon->theTransArrayIndex;
    ndbout << endl << flush;
#ifdef VM_TRACE
    printState("completedTransaction abort");
    abort();
#endif
  }//if
}//Ndb::completedTransaction()

/*****************************************************************************
void reportCallback(NdbTransaction** aCopyArray, Uint32 aNoOfCompletedTrans);

Remark:   Call the callback methods of the completed transactions.
******************************************************************************/
void	
Ndb::reportCallback(NdbTransaction** aCopyArray, Uint32 aNoOfCompletedTrans)
{
  Uint32         i;
  if (aNoOfCompletedTrans > 0) {
    for (i = 0; i < aNoOfCompletedTrans; i++) {
      void* anyObject = aCopyArray[i]->theCallbackObject;
      NdbAsynchCallback aCallback = aCopyArray[i]->theCallbackFunction;
      int tResult = 0;
      if (aCallback != NULL) {
        if (aCopyArray[i]->theReturnStatus == NdbTransaction::ReturnFailure) {
          tResult = -1;
        }//if
        (*aCallback)(tResult, aCopyArray[i], anyObject);
      }//if
    }//for
  }//if
}//Ndb::reportCallback()

/*****************************************************************************
Uint32 pollCompleted(NdbTransaction** aCopyArray);

Remark:   Transfer the data from the completed transaction to a local array.
          This support is used by a number of the poll-methods.
******************************************************************************/
Uint32	
Ndb::pollCompleted(NdbTransaction** aCopyArray)
{
  check_send_timeout();
  Uint32         i;
  Uint32 tNoCompletedTransactions = theNoOfCompletedTransactions;
  if (tNoCompletedTransactions > 0) {
    for (i = 0; i < tNoCompletedTransactions; i++) {
      aCopyArray[i] = theCompletedTransactionsArray[i];
      if (aCopyArray[i]->theListState != NdbTransaction::InCompletedList) {
        ndbout << "pollCompleted error ";
        ndbout << (int) aCopyArray[i]->theListState << endl;
	abort();
      }//if
      theCompletedTransactionsArray[i] = NULL;
      aCopyArray[i]->theListState = NdbTransaction::NotInList;
    }//for
  }//if
  theNoOfCompletedTransactions = 0;
  return tNoCompletedTransactions;
}//Ndb::pollCompleted()

void
Ndb::check_send_timeout()
{
  Uint32 timeout = theImpl->m_transporter_facade->m_waitfor_timeout;
  NDB_TICKS current_time = NdbTick_CurrentMillisecond();
  assert(current_time >= the_last_check_time);
  if (current_time - the_last_check_time > 1000) {
    the_last_check_time = current_time;
    Uint32 no_of_sent = theNoOfSentTransactions;
    for (Uint32 i = 0; i < no_of_sent; i++) {
      NdbTransaction* a_con = theSentTransactionsArray[i];
      if ((current_time - a_con->theStartTransTime) > timeout)
      {
#ifdef VM_TRACE
        a_con->printState();
	Uint32 t1 = a_con->theTransactionId;
	Uint32 t2 = a_con->theTransactionId >> 32;
	ndbout_c("4012 [%.8x %.8x]", t1, t2);
	//abort();
#endif
        a_con->theReleaseOnClose = true;
        a_con->setOperationErrorCodeAbort(4012);
	a_con->theCommitStatus = NdbTransaction::NeedAbort;
        a_con->theCompletionStatus = NdbTransaction::CompletedFailure;
        a_con->handleExecuteCompletion();
        remove_sent_list(i);
        insert_completed_list(a_con);
        no_of_sent--;
        i--;
      }//if
    }//for
  }//if
}

void
Ndb::remove_sent_list(Uint32 list_index)
{
  Uint32 last_index = theNoOfSentTransactions - 1;
  if (list_index < last_index) {
    NdbTransaction* t_con = theSentTransactionsArray[last_index];
    theSentTransactionsArray[list_index] = t_con;
  }//if
  theNoOfSentTransactions = last_index;
  theSentTransactionsArray[last_index] = 0;
}

Uint32
Ndb::insert_completed_list(NdbTransaction* a_con)
{
  Uint32 no_of_comp = theNoOfCompletedTransactions;
  theCompletedTransactionsArray[no_of_comp] = a_con;
  theNoOfCompletedTransactions = no_of_comp + 1;
  a_con->theListState = NdbTransaction::InCompletedList;
  a_con->theTransArrayIndex = no_of_comp;
  return no_of_comp;
}

Uint32
Ndb::insert_sent_list(NdbTransaction* a_con)
{
  Uint32 no_of_sent = theNoOfSentTransactions;
  theSentTransactionsArray[no_of_sent] = a_con;
  theNoOfSentTransactions = no_of_sent + 1;
  a_con->theListState = NdbTransaction::InSendList;
  a_con->theTransArrayIndex = no_of_sent;
  return no_of_sent;
}

/*****************************************************************************
void sendPrepTrans(int forceSend);

Remark: Send a batch of transactions prepared for sending to the NDB kernel.  
******************************************************************************/
void
Ndb::sendPrepTrans(int forceSend)
{
  // Always called when holding mutex on TransporterFacade
  /*
     We will send a list of transactions to the NDB kernel. Before
     sending we check the following.
     1) Node connected to is still alive
        Checked by both checking node status and node sequence
     2) Send buffer can handle the size of messages we are planning to send
        So far this is just a fake check but will soon be a real check
     When the connected node has failed we abort the transaction without
     responding anymore to the node since the kernel will clean up
     automatically.
     When sendBuffer cannot handle anymore messages then we will also abort
     transaction but by communicating to the kernel since it is still alive
     and we keep a small space for messages like that.
  */
  Uint32 i;
  TransporterFacade* tp = theImpl->m_transporter_facade;
  Uint32 no_of_prep_trans = theNoOfPreparedTransactions;
  for (i = 0; i < no_of_prep_trans; i++) {
    NdbTransaction * a_con = thePreparedTransactionsArray[i];
    thePreparedTransactionsArray[i] = NULL;
    Uint32 node_id = a_con->getConnectedNodeId();
    if ((tp->getNodeSequence(node_id) == a_con->theNodeSequence) &&
         tp->get_node_alive(node_id) ||
         (tp->get_node_stopping(node_id) && 
         ((a_con->theSendStatus == NdbTransaction::sendABORT) ||
          (a_con->theSendStatus == NdbTransaction::sendABORTfail) ||
          (a_con->theSendStatus == NdbTransaction::sendCOMMITstate) ||
          (a_con->theSendStatus == NdbTransaction::sendCompleted)))) {
      /*
      We will send if
      1) Node is alive and sequences are correct OR
      2) Node is stopping and we only want to commit or abort
      In a graceful stop situation we want to ensure quick aborts
      of all transactions and commits and thus we allow aborts and
      commits to continue but not normal operations.
      */
      if (tp->check_send_size(node_id, a_con->get_send_size())) {
        if (a_con->doSend() == 0) {
          NDB_TICKS current_time = NdbTick_CurrentMillisecond();
          a_con->theStartTransTime = current_time;
          continue;
        } else {
          /*
          Although all precautions we did not manage to send the operations
          Must have been a dropped connection on the transporter side.
          We don't expect to be able to continue using this connection so
          we will treat it as a node failure.
          */
          TRACE_DEBUG("Send problem even after checking node status");
        }//if
      } else {
        /*
        The send buffer is currently full or at least close to. We will
        not allow a send to continue. We will set the connection so that
        it is indicated that we need to abort the transaction. If we were
        trying to commit or abort and got a send buffer we will not try
        again and will thus set the state to Aborted to avoid a more or
        less eternal loop of tries.
        */
        if (a_con->theSendStatus == NdbTransaction::sendOperations) {
          a_con->setOperationErrorCodeAbort(4021);
          a_con->theCommitStatus = NdbTransaction::NeedAbort;
          TRACE_DEBUG("Send buffer full and sendOperations");
        } else {
          a_con->setOperationErrorCodeAbort(4026);
          a_con->theCommitStatus = NdbTransaction::Aborted;
          TRACE_DEBUG("Send buffer full, set state to Aborted");
        }//if
      }//if
    } else {
#ifdef VM_TRACE
      a_con->printState();
#endif
      if ((tp->getNodeSequence(node_id) == a_con->theNodeSequence) &&
          tp->get_node_stopping(node_id)) {
        /*
        The node we are connected to is currently in an early stopping phase
        of a graceful stop. We will not send the prepared transactions. We
        will simply refuse and let the application code handle the abort.
        */
        TRACE_DEBUG("Abort a transaction when stopping a node");
        a_con->setOperationErrorCodeAbort(4023);
        a_con->theCommitStatus = NdbTransaction::NeedAbort;
      } else {
        /*
        The node is hard dead and we cannot continue. We will also release
        the connection to the free pool.
        */
        TRACE_DEBUG("The node was stone dead, inform about abort");
        a_con->setOperationErrorCodeAbort(4025);
        a_con->theReleaseOnClose = true;
        a_con->theTransactionIsStarted = false;
        a_con->theCommitStatus = NdbTransaction::Aborted;
      }//if
    }//if
    a_con->theReturnStatus = NdbTransaction::ReturnFailure;
    a_con->theCompletionStatus = NdbTransaction::CompletedFailure;
    a_con->handleExecuteCompletion();
    insert_completed_list(a_con);
  }//for
  theNoOfPreparedTransactions = 0;
  if (forceSend == 0) {
     tp->checkForceSend(theNdbBlockNumber);
  } else if (forceSend == 1) {
     tp->forceSend(theNdbBlockNumber);
  }//if
  return;
}//Ndb::sendPrepTrans()

/*****************************************************************************
void waitCompletedTransactions(int aMilliSecondsToWait, int noOfEventsToWaitFor);

Remark:   First send all prepared operations and then check if there are any
          transactions already completed. Do not wait for not completed
          transactions.
******************************************************************************/
void	
Ndb::waitCompletedTransactions(int aMilliSecondsToWait, 
			       int noOfEventsToWaitFor,
                               PollGuard *poll_guard)
{
  theImpl->theWaiter.m_state = NO_WAIT; 
  /**
   * theImpl->theWaiter.m_state = NO_WAIT; 
   * To ensure no messup with synchronous node fail handling
   * (see ReportFailure)
   */
  int waitTime = aMilliSecondsToWait;
  NDB_TICKS currTime = NdbTick_CurrentMillisecond();
  NDB_TICKS maxTime = currTime + (NDB_TICKS)waitTime;
  theMinNoOfEventsToWakeUp = noOfEventsToWaitFor;
  do {
    if (waitTime < 1000) waitTime = 1000;
    poll_guard->wait_for_input(waitTime);
    if (theNoOfCompletedTransactions >= (Uint32)noOfEventsToWaitFor) {
      break;
    }//if
    theMinNoOfEventsToWakeUp = noOfEventsToWaitFor;
    waitTime = (int)(maxTime - NdbTick_CurrentMillisecond());
  } while (waitTime > 0);
}//Ndb::waitCompletedTransactions()

void Ndb::cond_signal()
{
  NdbCondition_Signal(theImpl->theWaiter.m_condition);
}
/*****************************************************************************
void sendPreparedTransactions(int forceSend = 0);

Remark:   First send all prepared operations and then check if there are any
          transactions already completed. Do not wait for not completed
          transactions.
******************************************************************************/
void	
Ndb::sendPreparedTransactions(int forceSend)
{
  theImpl->m_transporter_facade->lock_mutex();
  sendPrepTrans(forceSend);
  theImpl->m_transporter_facade->unlock_mutex();
  return;
}//Ndb::sendPreparedTransactions()

/*****************************************************************************
int sendPollNdb(int aMillisecondNumber, int minNoOfEventsToWakeup = 1, int forceSend = 0);

Remark:   First send all prepared operations and then check if there are any
          transactions already completed. Wait for not completed
          transactions until the specified number have completed or until the
          timeout has occured. Timeout zero means no waiting time.
******************************************************************************/
int	
Ndb::sendPollNdb(int aMillisecondNumber, int minNoOfEventsToWakeup, int forceSend)
{
  /*
    The PollGuard has an implicit call of unlock_and_signal through the
    ~PollGuard method. This method is called implicitly by the compiler
    in all places where the object is out of context due to a return,
    break, continue or simply end of statement block
  */
  PollGuard pg(theImpl->m_transporter_facade, &theImpl->theWaiter,
               theNdbBlockNumber);
  sendPrepTrans(forceSend);
  return poll_trans(aMillisecondNumber, minNoOfEventsToWakeup, &pg);
}

int
Ndb::poll_trans(int aMillisecondNumber, int minNoOfEventsToWakeup,
                PollGuard *pg)
{
  NdbTransaction* tConArray[1024];
  Uint32         tNoCompletedTransactions;
  if ((minNoOfEventsToWakeup <= 0) ||
      ((Uint32)minNoOfEventsToWakeup > theNoOfSentTransactions)) {
    minNoOfEventsToWakeup = theNoOfSentTransactions;
  }//if
  if ((theNoOfCompletedTransactions < (Uint32)minNoOfEventsToWakeup) &&
      (aMillisecondNumber > 0)) {
    waitCompletedTransactions(aMillisecondNumber, minNoOfEventsToWakeup, pg);
    tNoCompletedTransactions = pollCompleted(tConArray);
  } else {
    tNoCompletedTransactions = pollCompleted(tConArray);
  }//if
  pg->unlock_and_signal();
  reportCallback(tConArray, tNoCompletedTransactions);
  return tNoCompletedTransactions;
}

/*****************************************************************************
int pollNdb(int aMillisecondNumber, int minNoOfEventsToWakeup);

Remark:   Check if there are any transactions already completed. Wait for not
          completed transactions until the specified number have completed or
          until the timeout has occured. Timeout zero means no waiting time.
******************************************************************************/
int	
Ndb::pollNdb(int aMillisecondNumber, int minNoOfEventsToWakeup)
{
  /*
    The PollGuard has an implicit call of unlock_and_signal through the
    ~PollGuard method. This method is called implicitly by the compiler
    in all places where the object is out of context due to a return,
    break, continue or simply end of statement block
  */
  PollGuard pg(theImpl->m_transporter_facade, &theImpl->theWaiter,
               theNdbBlockNumber);
  return poll_trans(aMillisecondNumber, minNoOfEventsToWakeup, &pg);
}

int
Ndb::sendRecSignal(Uint16 node_id,
		   Uint32 aWaitState,
		   NdbApiSignal* aSignal,
                   Uint32 conn_seq,
                   Uint32 *ret_conn_seq)
{
  /*
  In most situations 0 is returned.
  In error cases we have 5 different cases
  -1: Send ok, time out in waiting for reply
  -2: Node has failed
  -3: Send buffer not full, send failed yet
  -4: Send buffer full
  -5: Node is currently stopping
  */

  int return_code;
  Uint32 read_conn_seq;
  TransporterFacade* tp = theImpl->m_transporter_facade;
  Uint32 send_size = 1; // Always sends one signal only
  // Protected area
  /*
    The PollGuard has an implicit call of unlock_and_signal through the
    ~PollGuard method. This method is called implicitly by the compiler
    in all places where the object is out of context due to a return,
    break, continue or simply end of statement block
  */
  PollGuard poll_guard(tp,&theImpl->theWaiter,theNdbBlockNumber);
  read_conn_seq= tp->getNodeSequence(node_id);
  if (ret_conn_seq)
    *ret_conn_seq= read_conn_seq;
  if ((tp->get_node_alive(node_id)) &&
      ((read_conn_seq == conn_seq) ||
       (conn_seq == 0))) {
    if (tp->check_send_size(node_id, send_size)) {
      return_code = tp->sendSignal(aSignal, node_id);
      if (return_code != -1) {
        return poll_guard.wait_n_unlock(WAITFOR_RESPONSE_TIMEOUT,node_id,
                                         aWaitState, false);
      } else {
	return_code = -3;
      }
    } else {
      return_code = -4;
    }//if
  } else {
    if ((tp->get_node_stopping(node_id)) &&
        ((read_conn_seq == conn_seq) ||
         (conn_seq == 0))) {
      return_code = -5;
    } else {
      return_code = -2;
    }//if
  }//if
  return return_code;
  // End of protected area
}//Ndb::sendRecSignal()

void
NdbTransaction::sendTC_COMMIT_ACK(TransporterFacade *tp,
                                  NdbApiSignal * aSignal,
                                  Uint32 transId1, Uint32 transId2, 
                                  Uint32 aTCRef){
#ifdef MARKER_TRACE
  ndbout_c("Sending TC_COMMIT_ACK(0x%.8x, 0x%.8x) to -> %d",
	   transId1,
	   transId2,
	   refToNode(aTCRef));
#endif  
  aSignal->theTrace                = TestOrd::TraceAPI;
  aSignal->theReceiversBlockNumber = DBTC;
  aSignal->theVerId_signalNumber   = GSN_TC_COMMIT_ACK;
  aSignal->theLength               = 2;

  Uint32 * dataPtr = aSignal->getDataPtrSend();
  dataPtr[0] = transId1;
  dataPtr[1] = transId2;
  tp->sendSignalUnCond(aSignal, refToNode(aTCRef));
}

int
NdbImpl::send_event_report(Uint32 *data, Uint32 length)
{
  NdbApiSignal aSignal(m_ndb.theMyRef);
  TransporterFacade *tp = m_transporter_facade;
  aSignal.theTrace                = TestOrd::TraceAPI;
  aSignal.theReceiversBlockNumber = CMVMI;
  aSignal.theVerId_signalNumber   = GSN_EVENT_REP;
  aSignal.theLength               = length;
  memcpy((char *)aSignal.getDataPtrSend(), (char *)data, length*4);

  Uint32 tNode;
  Ndb_cluster_connection_node_iter node_iter;
  m_ndb_cluster_connection.init_get_next_node(node_iter);
  while ((tNode= m_ndb_cluster_connection.get_next_node(node_iter)))
  {
    if(tp->get_node_alive(tNode)){
      tp->sendSignal(&aSignal, tNode);
      return 0;
    }
  }
  return 1;
}

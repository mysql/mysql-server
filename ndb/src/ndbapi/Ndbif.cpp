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
#include "NdbOperation.hpp"
#include "NdbIndexOperation.hpp"
#include "NdbScanOperation.hpp"
#include "NdbConnection.hpp"
#include "NdbRecAttr.hpp"
#include "NdbReceiver.hpp"
#include "API.hpp"

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

#include <ndb_limits.h>
#include <NdbOut.hpp>
#include <NdbTick.h>


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
  TransporterFacade * theFacade =  TransporterFacade::instance();
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
  
  aNrOfCon = theNoOfDBnodes;
  aNrOfOp = 2*theNoOfDBnodes;
  
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
  
  tMaxNoOfTransactions = aMaxNoOfTransactions * 3;
  if (tMaxNoOfTransactions > 1024) {
    tMaxNoOfTransactions = 1024;
  }//if
  theMaxNoOfTransactions = tMaxNoOfTransactions;
  
  thePreparedTransactionsArray = new NdbConnection* [tMaxNoOfTransactions];
  theSentTransactionsArray = new NdbConnection* [tMaxNoOfTransactions];
  theCompletedTransactionsArray = new NdbConnection* [tMaxNoOfTransactions];
  
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
  while ( theConIdleList != NULL )
    freeNdbCon();
  while ( theSignalIdleList != NULL )
    freeSignal();
  while (theRecAttrIdleList != NULL)
    freeRecAttr(); 
  while (theOpIdleList != NULL)
    freeOperation();
  
  delete theDictionary;
  TransporterFacade::instance()->close(theNdbBlockNumber, 0);
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
  theMyRef= ref;
  Uint32 tmpTheNode= refToNode(ref);
  Uint64 tBlockNo= refToBlock(ref);
  if (theNdbBlockNumber >= 0){
    assert(theMyRef == numberToRef(theNdbBlockNumber, tmpTheNode));
  }
  
  TransporterFacade * theFacade =  TransporterFacade::instance();
  int i;
  theNoOfDBnodes= 0;
  for (i = 1; i < MAX_NDB_NODES; i++){
    if (theFacade->getIsDbNode(i)){
      theDBnodes[theNoOfDBnodes] = i;
      theNoOfDBnodes++;
    }
  }
  theFirstTransId = ((Uint64)tBlockNo << 52)+
    ((Uint64)tmpTheNode << 40);
  theFirstTransId += theFacade->m_max_trans_id;
  //      assert(0);
  DBUG_PRINT("info",("connected with ref=%x, id=%d, no_db_nodes=%d, first_trans_id=%lx",
		     theMyRef,
		     tmpTheNode,
		     theNoOfDBnodes,
		     theFirstTransId));
  startTransactionNodeSelectionData.init(theNoOfDBnodes, theDBnodes);
  theCommitAckSignal = new NdbApiSignal(theMyRef);

  theDictionary->m_receiver.m_reference= theMyRef;
  theNode= tmpTheNode; // flag that Ndb object is initialized
}

void
Ndb::statusMessage(void* NdbObject, Uint32 a_node, bool alive, bool nfComplete)
{
  DBUG_ENTER("Ndb::statusMessage");
  Ndb* tNdb = (Ndb*)NdbObject;
  if (alive) {
    if (nfComplete) {
      tNdb->connected(a_node);
      DBUG_VOID_RETURN;
    }//if
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
  the_release_ind[node_id] = 1;
  theWaiter.nodeFail(node_id);
}//Ndb::report_node_failure()


void
Ndb::report_node_failure_completed(Uint32 node_id)
{
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
    NdbConnection* localCon = theSentTransactionsArray[i];
    if (localCon->getConnectedNodeId() == aNodeId ) {
      const NdbConnection::SendStatusType sendStatus = localCon->theSendStatus;
      if (sendStatus == NdbConnection::sendTC_OP || sendStatus == NdbConnection::sendTC_COMMIT) {
        /*
        A transaction was interrupted in the prepare phase by a node
        failure. Since the transaction was not found in the phase
        after the node failure it cannot have been committed and
        we report a normal node failure abort.
        */
	localCon->setOperationErrorCodeAbort(4010);
        localCon->theCompletionStatus = NdbConnection::CompletedFailure;
      } else if (sendStatus == NdbConnection::sendTC_ROLLBACK) {
        /*
        We aimed for abort and abort we got even if it was by a node
        failure. We will thus report it as a success.
        */
        localCon->theCompletionStatus = NdbConnection::CompletedSuccess;
      } else {
#ifdef VM_TRACE
        printState("abortTransactionsAfterNodeFailure %x", this);
        abort();
#endif
      }//
      /*
      All transactions arriving here have no connection to the kernel
      intact since the node was failing and they were aborted. Thus we
      set commit state to Aborted and set state to release on close.
      */
      localCon->theCommitStatus = NdbConnection::Aborted;
      localCon->theReleaseOnClose = true;
      completedTransaction(localCon);
    }//if
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
  NdbConnection* tCon;
  int tReturnCode = -1;
  const Uint32* tDataPtr = aSignal->getDataPtr();
  const Uint32 tWaitState = theWaiter.m_state;
  const Uint32 tSignalNumber = aSignal->readSignalNumber();
  const Uint32 tFirstData = *tDataPtr;
  const Uint32 tLen = aSignal->getLength();
  void * tFirstDataPtr;

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
          (tCon->theSendStatus == NdbConnection::sendTC_OP)) {
        tReturnCode = tCon->receiveTCKEYCONF(keyConf, tLen);
        if (tReturnCode != -1) {
          completedTransaction(tCon);
        }//if

	if(TcKeyConf::getMarkerFlag(keyConf->confInfo)){
	  NdbConnection::sendTC_COMMIT_ACK(theCommitAckSignal,
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
      
      if(com == 1){
	switch(tRec->getType()){
	case NdbReceiver::NDB_OPERATION:
	case NdbReceiver::NDB_INDEX_OPERATION:
	  if(tCon->OpCompleteSuccess() != -1){
	    completedTransaction(tCon);
	    return;
	  }
	  break;
	case NdbReceiver::NDB_SCANRECEIVER:
	  tCon->theScanningOp->receiver_delivered(tRec);
	  theWaiter.m_state = (((WaitSignalType) tWaitState) == WAIT_SCAN ? 
			       (Uint32) NO_WAIT : tWaitState);
	  break;
	default:
	  goto InvalidSignal;
	}
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
	    if ((tCon->theSendStatus == NdbConnection::sendTC_OP) ||
		(tCon->theSendStatus == NdbConnection::sendTC_COMMIT)) {
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
	NdbConnection::sendTC_COMMIT_ACK(theCommitAckSignal,
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
	if (tOp->checkMagicNumber() == 0) {
	  tCon = tOp->theNdbCon;
	  if (tCon != NULL) {
	    if ((tCon->theSendStatus == NdbConnection::sendTC_OP) ||
		(tCon->theSendStatus == NdbConnection::sendTC_ROLLBACK)) {
	      tReturnCode = tCon->receiveTCKEY_FAILREF(aSignal);
	      if (tReturnCode != -1) {
		completedTransaction(tCon);
		return;
	      }//if
	    }//if
	  }//if
	}//if
      } else {
#ifdef VM_TRACE
	ndbout_c("Recevied TCKEY_FAILREF wo/ operation");
#endif
      }
      break;
    }
  case GSN_TCKEYREF:
    {
      tFirstDataPtr = int2void(tFirstData);
      if (tFirstDataPtr == 0) goto InvalidSignal;

      tOp = void2rec_op(tFirstDataPtr);
      if (tOp->checkMagicNumber() == 0) {
	tCon = tOp->theNdbCon;
	if (tCon != NULL) {
	  if (tCon->theSendStatus == NdbConnection::sendTC_OP) {
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
	  (tCon->theSendStatus == NdbConnection::sendTC_COMMIT)) {
	tReturnCode = tCon->receiveTC_COMMITCONF(commitConf);
	if (tReturnCode != -1) {
	  completedTransaction(tCon);
	}//if

	if(tFirstData & 1){
	  NdbConnection::sendTC_COMMIT_ACK(theCommitAckSignal,
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
	  (tCon->theSendStatus == NdbConnection::sendTC_COMMIT)) {
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
	  (tCon->theSendStatus == NdbConnection::sendTC_ROLLBACK)) {
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
	  (tCon->theSendStatus == NdbConnection::sendTC_ROLLBACK)) {
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
	theWaiter.m_state = NO_WAIT;
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
	theWaiter.m_state = NO_WAIT;
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
	theWaiter.m_state = NO_WAIT;
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
	theWaiter.m_state = NO_WAIT;
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
    NdbDictInterface::execSignal(&theDictionary->m_receiver,
				 aSignal, ptr);
    break;
    
  case GSN_SUB_META_DATA:
  case GSN_SUB_REMOVE_CONF:
  case GSN_SUB_REMOVE_REF:
    break; // ignore these signals
  case GSN_SUB_GCP_COMPLETE_REP:
  case GSN_SUB_START_CONF:
  case GSN_SUB_START_REF:
  case GSN_SUB_TABLE_DATA:
  case GSN_SUB_STOP_CONF:
  case GSN_SUB_STOP_REF:
    NdbDictInterface::execSignal(&theDictionary->m_receiver,
				 aSignal, ptr);
    break;

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
	theWaiter.m_state = NO_WAIT;
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
	  theWaiter.m_state = NO_WAIT;
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
	  theWaiter.m_state = NO_WAIT;
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
	theWaiter.m_state = (((WaitSignalType) tWaitState) == WAIT_SCAN ? 
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
	(tCon->theSendStatus == NdbConnection::sendTC_OP)) {
      tReturnCode = tCon->receiveTCINDXCONF(indxConf, tLen);
      if (tReturnCode != -1) { 
	completedTransaction(tCon);
      }//if
    }//if
    
    if(TcIndxConf::getMarkerFlag(indxConf->confInfo)){
      NdbConnection::sendTC_COMMIT_ACK(theCommitAckSignal,
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
	if (tCon->theSendStatus == NdbConnection::sendTC_OP) {
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
  }//switch
  
  if (theWaiter.m_state == NO_WAIT) {
    // Wake up the thread waiting for response
    NdbCondition_Signal(theWaiter.m_condition);
  }//if
  return;

 InvalidSignal:
#ifdef VM_TRACE
  ndbout_c("Ndbif: Error Ndb::handleReceivedSignal "
	   "(GSN=%d, theWaiter.m_state=%d)"
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
void completedTransaction(NdbConnection* aCon);

Remark:   One transaction has been completed.
          Remove it from send array and put it into the completed
          transaction array. Finally check if it is time to wake
          up a poller.
******************************************************************************/
void	
Ndb::completedTransaction(NdbConnection* aCon)
{
  Uint32 tTransArrayIndex = aCon->theTransArrayIndex;
  Uint32 tNoSentTransactions = theNoOfSentTransactions;
  Uint32 tNoCompletedTransactions = theNoOfCompletedTransactions;
  if ((tNoSentTransactions > 0) && (aCon->theListState == NdbConnection::InSendList) &&
      (tTransArrayIndex < tNoSentTransactions)) {
    NdbConnection* tMoveCon = theSentTransactionsArray[tNoSentTransactions - 1];

    theCompletedTransactionsArray[tNoCompletedTransactions] = aCon;
    aCon->theTransArrayIndex = tNoCompletedTransactions;
    if (tMoveCon != aCon) {
      tMoveCon->theTransArrayIndex = tTransArrayIndex;
      theSentTransactionsArray[tTransArrayIndex] = tMoveCon;
    }//if
    theSentTransactionsArray[tNoSentTransactions - 1] = NULL;
    theNoOfCompletedTransactions = tNoCompletedTransactions + 1;

    theNoOfSentTransactions = tNoSentTransactions - 1;
    aCon->theListState = NdbConnection::InCompletedList;
    aCon->handleExecuteCompletion();
    if ((theMinNoOfEventsToWakeUp != 0) &&
        (theNoOfCompletedTransactions >= theMinNoOfEventsToWakeUp)) {
      theMinNoOfEventsToWakeUp = 0;
      NdbCondition_Signal(theWaiter.m_condition);
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
void reportCallback(NdbConnection** aCopyArray, Uint32 aNoOfCompletedTrans);

Remark:   Call the callback methods of the completed transactions.
******************************************************************************/
void	
Ndb::reportCallback(NdbConnection** aCopyArray, Uint32 aNoOfCompletedTrans)
{
  Uint32         i;
  if (aNoOfCompletedTrans > 0) {
    for (i = 0; i < aNoOfCompletedTrans; i++) {
      void* anyObject = aCopyArray[i]->theCallbackObject;
      NdbAsynchCallback aCallback = aCopyArray[i]->theCallbackFunction;
      int tResult = 0;
      if (aCallback != NULL) {
        if (aCopyArray[i]->theReturnStatus == NdbConnection::ReturnFailure) {
          tResult = -1;
        }//if
        (*aCallback)(tResult, aCopyArray[i], anyObject);
      }//if
    }//for
  }//if
}//Ndb::reportCallback()

/*****************************************************************************
Uint32 pollCompleted(NdbConnection** aCopyArray);

Remark:   Transfer the data from the completed transaction to a local array.
          This support is used by a number of the poll-methods.
******************************************************************************/
Uint32	
Ndb::pollCompleted(NdbConnection** aCopyArray)
{
  check_send_timeout();
  Uint32         i;
  Uint32 tNoCompletedTransactions = theNoOfCompletedTransactions;
  if (tNoCompletedTransactions > 0) {
    for (i = 0; i < tNoCompletedTransactions; i++) {
      aCopyArray[i] = theCompletedTransactionsArray[i];
      if (aCopyArray[i]->theListState != NdbConnection::InCompletedList) {
        ndbout << "pollCompleted error ";
        ndbout << (int) aCopyArray[i]->theListState << endl;
	abort();
      }//if
      theCompletedTransactionsArray[i] = NULL;
      aCopyArray[i]->theListState = NdbConnection::NotInList;
    }//for
  }//if
  theNoOfCompletedTransactions = 0;
  return tNoCompletedTransactions;
}//Ndb::pollCompleted()

void
Ndb::check_send_timeout()
{
  NDB_TICKS current_time = NdbTick_CurrentMillisecond();
  if (current_time - the_last_check_time > 1000) {
    the_last_check_time = current_time;
    Uint32 no_of_sent = theNoOfSentTransactions;
    for (Uint32 i = 0; i < no_of_sent; i++) {
      NdbConnection* a_con = theSentTransactionsArray[i];
      if ((current_time - a_con->theStartTransTime) >
          WAITFOR_RESPONSE_TIMEOUT) {
#ifdef VM_TRACE
        a_con->printState();
	Uint32 t1 = a_con->theTransactionId;
	Uint32 t2 = a_con->theTransactionId >> 32;
	ndbout_c("[%.8x %.8x]", t1, t2);
	abort();
#endif
        a_con->setOperationErrorCodeAbort(4012);
        a_con->theCommitStatus = NdbConnection::Aborted;
        a_con->theCompletionStatus = NdbConnection::CompletedFailure;
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
    NdbConnection* t_con = theSentTransactionsArray[last_index];
    theSentTransactionsArray[list_index] = t_con;
  }//if
  theNoOfSentTransactions = last_index;
  theSentTransactionsArray[last_index] = 0;
}

Uint32
Ndb::insert_completed_list(NdbConnection* a_con)
{
  Uint32 no_of_comp = theNoOfCompletedTransactions;
  theCompletedTransactionsArray[no_of_comp] = a_con;
  theNoOfCompletedTransactions = no_of_comp + 1;
  a_con->theListState = NdbConnection::InCompletedList;
  a_con->theTransArrayIndex = no_of_comp;
  return no_of_comp;
}

Uint32
Ndb::insert_sent_list(NdbConnection* a_con)
{
  Uint32 no_of_sent = theNoOfSentTransactions;
  theSentTransactionsArray[no_of_sent] = a_con;
  theNoOfSentTransactions = no_of_sent + 1;
  a_con->theListState = NdbConnection::InSendList;
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
  TransporterFacade* tp = TransporterFacade::instance();
  Uint32 no_of_prep_trans = theNoOfPreparedTransactions;
  for (i = 0; i < no_of_prep_trans; i++) {
    NdbConnection * a_con = thePreparedTransactionsArray[i];
    thePreparedTransactionsArray[i] = NULL;
    Uint32 node_id = a_con->getConnectedNodeId();
    if ((tp->getNodeSequence(node_id) == a_con->theNodeSequence) &&
         tp->get_node_alive(node_id) ||
         (tp->get_node_stopping(node_id) && 
         ((a_con->theSendStatus == NdbConnection::sendABORT) ||
          (a_con->theSendStatus == NdbConnection::sendABORTfail) ||
          (a_con->theSendStatus == NdbConnection::sendCOMMITstate) ||
          (a_con->theSendStatus == NdbConnection::sendCompleted)))) {
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
        if (a_con->theSendStatus == NdbConnection::sendOperations) {
          a_con->setOperationErrorCodeAbort(4021);
          a_con->theCommitStatus = NdbConnection::NeedAbort;
          TRACE_DEBUG("Send buffer full and sendOperations");
        } else {
          a_con->setOperationErrorCodeAbort(4026);
          a_con->theCommitStatus = NdbConnection::Aborted;
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
        a_con->theCommitStatus = NdbConnection::NeedAbort;
      } else {
        /*
        The node is hard dead and we cannot continue. We will also release
        the connection to the free pool.
        */
        TRACE_DEBUG("The node was stone dead, inform about abort");
        a_con->setOperationErrorCodeAbort(4025);
        a_con->theReleaseOnClose = true;
        a_con->theTransactionIsStarted = false;
        a_con->theCommitStatus = NdbConnection::Aborted;
      }//if
    }//if
    a_con->theCompletionStatus = NdbConnection::CompletedFailure;
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
			       int noOfEventsToWaitFor)
{
  theWaiter.m_state = NO_WAIT; 
  /**
   * theWaiter.m_state = NO_WAIT; 
   * To ensure no messup with synchronous node fail handling
   * (see ReportFailure)
   */
  int waitTime = aMilliSecondsToWait;
  NDB_TICKS maxTime = NdbTick_CurrentMillisecond() + (NDB_TICKS)waitTime;
  theMinNoOfEventsToWakeUp = noOfEventsToWaitFor;
  do {
    if (waitTime < 1000) waitTime = 1000;
    NdbCondition_WaitTimeout(theWaiter.m_condition,
			     (NdbMutex*)theWaiter.m_mutex,
			     waitTime);
    if (theNoOfCompletedTransactions >= (Uint32)noOfEventsToWaitFor) {
      break;
    }//if
    theMinNoOfEventsToWakeUp = noOfEventsToWaitFor;
    waitTime = (int)(maxTime - NdbTick_CurrentMillisecond());
  } while (waitTime > 0);
  return;
}//Ndb::waitCompletedTransactions()

/*****************************************************************************
void sendPreparedTransactions(int forceSend = 0);

Remark:   First send all prepared operations and then check if there are any
          transactions already completed. Do not wait for not completed
          transactions.
******************************************************************************/
void	
Ndb::sendPreparedTransactions(int forceSend)
{
  TransporterFacade::instance()->lock_mutex();
  sendPrepTrans(forceSend);
  TransporterFacade::instance()->unlock_mutex();
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
  NdbConnection* tConArray[1024];
  Uint32         tNoCompletedTransactions;

  //theCurrentConnectCounter = 0;
  //theCurrentConnectIndex++;
  TransporterFacade::instance()->lock_mutex();
  sendPrepTrans(forceSend);
  if ((minNoOfEventsToWakeup <= 0) ||
      ((Uint32)minNoOfEventsToWakeup > theNoOfSentTransactions)) {
    minNoOfEventsToWakeup = theNoOfSentTransactions;
  }//if
  if ((theNoOfCompletedTransactions < (Uint32)minNoOfEventsToWakeup) &&
      (aMillisecondNumber > 0)) {
    waitCompletedTransactions(aMillisecondNumber, minNoOfEventsToWakeup);
    tNoCompletedTransactions = pollCompleted(tConArray);
  } else {
    tNoCompletedTransactions = pollCompleted(tConArray);
  }//if
  TransporterFacade::instance()->unlock_mutex();
  reportCallback(tConArray, tNoCompletedTransactions);
  return tNoCompletedTransactions;
}//Ndb::sendPollNdb()

/*****************************************************************************
int pollNdb(int aMillisecondNumber, int minNoOfEventsToWakeup);

Remark:   Check if there are any transactions already completed. Wait for not
          completed transactions until the specified number have completed or
          until the timeout has occured. Timeout zero means no waiting time.
******************************************************************************/
int	
Ndb::pollNdb(int aMillisecondNumber, int minNoOfEventsToWakeup)
{
  NdbConnection* tConArray[1024];
  Uint32         tNoCompletedTransactions;

  //theCurrentConnectCounter = 0;
  //theCurrentConnectIndex++;
  TransporterFacade::instance()->lock_mutex();
  if ((minNoOfEventsToWakeup == 0) ||
      ((Uint32)minNoOfEventsToWakeup > theNoOfSentTransactions)) {
    minNoOfEventsToWakeup = theNoOfSentTransactions;
  }//if
  if ((theNoOfCompletedTransactions < (Uint32)minNoOfEventsToWakeup) &&
      (aMillisecondNumber > 0)) {
    waitCompletedTransactions(aMillisecondNumber, minNoOfEventsToWakeup);
    tNoCompletedTransactions = pollCompleted(tConArray);
  } else {
    tNoCompletedTransactions = pollCompleted(tConArray);
  }//if
  TransporterFacade::instance()->unlock_mutex();
  reportCallback(tConArray, tNoCompletedTransactions);
  return tNoCompletedTransactions;
}//Ndb::sendPollNdbWithoutWait()

/*****************************************************************************
int receiveOptimisedResponse();

Return:  0 - Response received
        -1 - Timeout occured waiting for response
        -2 - Node failure interupted wait for response

******************************************************************************/
int	
Ndb::receiveResponse(int waitTime){
  int tResultCode;
  TransporterFacade::instance()->checkForceSend(theNdbBlockNumber);
  
  theWaiter.wait(waitTime);
  
  if(theWaiter.m_state == NO_WAIT) {
    tResultCode = 0;
  } else {

#ifdef VM_TRACE
    ndbout << "ERR: receiveResponse - theWaiter.m_state = ";
    ndbout << theWaiter.m_state << endl;
#endif

    if (theWaiter.m_state == WAIT_NODE_FAILURE){
      tResultCode = -2;
    } else {
      tResultCode = -1;
    }
    theWaiter.m_state = NO_WAIT;
  }
  return tResultCode;
}//Ndb::receiveResponse()

int
Ndb::sendRecSignal(Uint16 node_id,
		   Uint32 aWaitState,
		   NdbApiSignal* aSignal,
                   Uint32 conn_seq)
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
  TransporterFacade* tp = TransporterFacade::instance();
  Uint32 send_size = 1; // Always sends one signal only 
  tp->lock_mutex();
  // Protected area
  if ((tp->get_node_alive(node_id)) &&
      ((tp->getNodeSequence(node_id) == conn_seq) ||
       (conn_seq == 0))) {
    if (tp->check_send_size(node_id, send_size)) {
      return_code = tp->sendSignal(aSignal, node_id);
      if (return_code != -1) {
        theWaiter.m_node = node_id;
        theWaiter.m_state = aWaitState;
        return_code = receiveResponse();
      } else {
	return_code = -3;
      }
    } else {
      return_code = -4;
    }//if
  } else {
    if ((tp->get_node_stopping(node_id)) &&
        ((tp->getNodeSequence(node_id) == conn_seq) ||
         (conn_seq == 0))) {
      return_code = -5;
    } else {
      return_code = -2;
    }//if
  }//if
  tp->unlock_mutex();
  // End of protected area
  return return_code;
}//Ndb::sendRecSignal()

void
NdbConnection::sendTC_COMMIT_ACK(NdbApiSignal * aSignal,
				 Uint32 transId1, Uint32 transId2, 
				 Uint32 aTCRef){
#ifdef MARKER_TRACE
  ndbout_c("Sending TC_COMMIT_ACK(0x%.8x, 0x%.8x) to -> %d",
	   transId1,
	   transId2,
	   refToNode(aTCRef));
#endif  
  TransporterFacade *tp = TransporterFacade::instance();
  aSignal->theTrace                = TestOrd::TraceAPI;
  aSignal->theReceiversBlockNumber = DBTC;
  aSignal->theVerId_signalNumber   = GSN_TC_COMMIT_ACK;
  aSignal->theLength               = 2;

  Uint32 * dataPtr = aSignal->getDataPtrSend();
  dataPtr[0] = transId1;
  dataPtr[1] = transId2;

  tp->sendSignal(aSignal, refToNode(aTCRef));
}

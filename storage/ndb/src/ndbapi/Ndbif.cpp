/* Copyright (c) 2003, 2018, Oracle and/or its affiliates. All rights reserved.

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


#include <ndb_global.h>

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
#include <signaldata/SumaImpl.hpp>
#include <signaldata/NodeFailRep.hpp>
#include <signaldata/NFCompleteRep.hpp>
#include <signaldata/AllocNodeId.hpp>

#include <ndb_limits.h>
#include <NdbOut.hpp>
#include <NdbTick.h>
#ifndef DBUG_OFF
#include <NdbSleep.h>
#endif
#include <EventLogger.hpp>

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
  theEventBuffer->m_mutex = theImpl->m_mutex;

  const Uint32 tRef = theImpl->open(theFacade);

#ifndef DBUG_OFF
  if(DBUG_EVALUATE_IF("sleep_in_ndbinit", true, false))
  {
    fprintf(stderr, "Ndb::init() (%p) taking a break\n", this);
    NdbSleep_MilliSleep(20000);
    fprintf(stderr, "Ndb::init() resuming\n");
  }
#endif

  if (tRef == 0)
  {
    theError.code = 4105;
    DBUG_RETURN(-1); // no more free blocknumbers
  }//if
  
  Uint32 nodeId = refToNode(tRef);
  theNdbBlockNumber = refToBlock(tRef);

  if (nodeId > 0)
  {
    connected(Uint32(tRef));
  }

  /* Now that we have this block open, set the first transid for
   * this block from ndb_cluster_connection
   */
  theFirstTransId |= theImpl->m_ndb_cluster_connection.
    get_next_transid(theNdbBlockNumber);

  /* Init cached min node version */
  theFacade->lock_poll_mutex();
  theCachedMinDbNodeVersion = theFacade->getMinDbNodeVersion();
  theFacade->unlock_poll_mutex();
  
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

  /* Force visibility of Ndb object initialisation work before marking it initialised */
  theFacade->lock_poll_mutex();
  theFacade->unlock_poll_mutex();
  theInitState = Initialised; 

  DBUG_RETURN(0);
  
error_handler:
  ndbout << "error_handler" << endl;
  releaseTransactionArrays();
  delete theDictionary;
  theImpl->close();
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

void Ndb::connected(Uint32 ref)
{
// cluster connect, a_node == own reference
  theMyRef= ref;
  Uint32 tmpTheNode= refToNode(ref);
  Uint64 tBlockNo= refToBlock(ref);
  if (theNdbBlockNumber >= 0){
    assert(theMyRef == numberToRef(theNdbBlockNumber, tmpTheNode));
  }
  
  Uint32 cnt =
    theImpl->m_ndb_cluster_connection.get_db_nodes(theImpl->theDBnodes);
  theImpl->theNoOfDBnodes = cnt;
  
  theFirstTransId += ((Uint64)tBlockNo << 52)+
    ((Uint64)tmpTheNode << 40);
  //      assert(0);
  DBUG_PRINT("info",("connected with ref=%x, id=%d, no_db_nodes=%d, first_trans_id: 0x%lx",
		     theMyRef,
		     tmpTheNode,
		     theImpl->theNoOfDBnodes,
		     (long) theFirstTransId));
  theCommitAckSignal = new NdbApiSignal(theMyRef);

  theDictionary->m_receiver.m_reference= theMyRef;
  theNode= tmpTheNode; // flag that Ndb object is initialized
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
  assert(node_id < NDB_ARRAY_SIZE(theImpl->the_release_ind));
  if (node_id < NDB_ARRAY_SIZE(theImpl->the_release_ind))
  {
    theImpl->the_release_ind[node_id] = 1;
    // must come after
    theImpl->the_release_ind[0] = 1;
    theImpl->theWaiter.nodeFail(node_id);
  }
  return;
}//Ndb::report_node_failure()


void
Ndb::report_node_failure_completed(Uint32 node_id)
{
  if (theEventBuffer)
  {
    // node failed
    // eventOperations in the ndb object should be notified
    theEventBuffer->report_node_failure_completed(node_id);
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
        printState("abortTransactionsAfterNodeFailure %lx", (long)this);
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

NdbTransaction*
NdbImpl::lookupTransactionFromOperation(const TcKeyConf * conf)
{
  assert(TcKeyConf::getNoOfOperations(conf->confInfo) > 0);
  Uint32 opPtr = conf->operations[0].apiOperationPtr;
  void * voidptr = int2void(opPtr);
  if (voidptr)
  {
    NdbReceiver* rec = void2rec(voidptr);
    if (rec)
    {
      return rec->getTransaction(rec->getType());
    }
  }
  return 0;
}

/****************************************************************************
void trp_deliver_signal(NdbApiSignal* aSignal);

Parameters:     aSignal: The signal object.
Remark:         Send all operations belonging to this connection. 
*****************************************************************************/
void
NdbImpl::trp_deliver_signal(const NdbApiSignal * aSignal,
                            const LinearSectionPtr ptr[3])
{
  Ndb *myNdb = &m_ndb;
  int tReturnCode = -1;
  const Uint32* tDataPtr = aSignal->getDataPtr();
  const Uint32 tSignalNumber = aSignal->readSignalNumber();
  Ndb::InitType tInitState = myNdb->theInitState;
  const Uint32 tFirstData = *tDataPtr;
  const Uint32 tLen = aSignal->getLength();
  Uint32 secs = aSignal->m_noOfSections;
  Uint32 bytesReceived = ((aSignal->getLength() << 2) +
                          ((secs > 2)? ptr[2].sz << 2: 0) + 
                          ((secs > 1)? ptr[1].sz << 2: 0) +
                          ((secs > 0)? ptr[0].sz << 2: 0));

  NdbOperation* tOp;
  NdbIndexOperation* tIndexOp;
  NdbTransaction* tCon;

  /* Check that Ndb object is properly setup to handle the signal */
  if (tInitState != Ndb::Initialised)
    return;

  void *tFirstDataPtr = int2void(tFirstData);
  const Uint32 tWaitState = theWaiter.get_state();
  Uint32 tNewState = tWaitState;

  /* Update cached Min Db node version */
  myNdb->theCachedMinDbNodeVersion = m_transporter_facade->getMinDbNodeVersion();

  if (likely(recordGSN(tSignalNumber)))
  {
    incClientStat(Ndb::BytesRecvdCount, bytesReceived);
  }

  /*
    In order to support 64 bit processes in the application we need to use
    id's rather than a direct pointer to the object used. It is also a good
    idea that one cannot corrupt the application code by sending a corrupt
    memory pointer.
    
    All traffic signals received by the API requires the first data word to be
    such an id to the receiving object.
  */
  
  switch (tSignalNumber)
  {
  case GSN_TCKEYCONF:
  case GSN_TCINDXCONF:
  {
    const TcKeyConf * const keyConf = (TcKeyConf *)tDataPtr;
    if (tFirstData != RNIL)
    {
      tCon = void2con(tFirstDataPtr);
    }
    else
    {
      tCon = lookupTransactionFromOperation(keyConf);
    }
    if (likely(tCon != NULL))
    {
      const Uint32 magicNumber = tCon->getMagicNumberFromObject();
      NdbTransaction::SendStatusType tSendStatus = tCon->theSendStatus;
      const BlockReference aTCRef = aSignal->theSendersBlockRef;
      const bool marker = TcKeyConf::getMarkerFlag(keyConf->confInfo);

      if (likely((magicNumber == tCon->getMagicNumber()) &&
                 (tSendStatus == NdbTransaction::sendTC_OP)))
      {
        tReturnCode = tCon->receiveTCKEYCONF(keyConf, tLen);
        /**
         * BUG#19643174
         * ------------
         * Ensure that we always send TC_COMMIT_ACK before we report
         * transaction as completed, this avoids races where the API
         * user starts another activity before we've completed the
         * sending of TC_COMMIT_ACK. This is mostly a problem for
         * test applications that e.g. want to check for memory
         * leaks after a transaction has completed. We only do this
         * action if requested by the API user (should only be
         * requested by test application).
         */
        if (unlikely(marker && send_TC_COMMIT_ACK_immediate_flag))
        {
          NdbTransaction::sendTC_COMMIT_ACK(this,
                                            myNdb->theCommitAckSignal,
                                            keyConf->transId1,
                                            keyConf->transId2,
                                            aTCRef,
                                            send_TC_COMMIT_ACK_immediate_flag);
          if (tReturnCode != -1)
          {
            myNdb->completedTransaction(tCon);
          }
          return;
        }
        if (tReturnCode != -1)
        {
          myNdb->completedTransaction(tCon);
        }
        if (marker)
        {
          NdbTransaction::sendTC_COMMIT_ACK(this,
                                            myNdb->theCommitAckSignal,
                                            keyConf->transId1,
                                            keyConf->transId2,
                                            aTCRef,
                                            false);
        }
        return;
      }
    }
    const bool marker = TcKeyConf::getMarkerFlag(keyConf->confInfo);
    const BlockReference aTCRef = aSignal->theSendersBlockRef;
    if (marker)
    {
      /**
       * We must send the TC_COMMIT_ACK even if we "reject" signal!
       */
      NdbTransaction::sendTC_COMMIT_ACK(this,
                                        myNdb->theCommitAckSignal,
                                        keyConf->transId1,
                                        keyConf->transId2,
                                        aTCRef,
                                        send_TC_COMMIT_ACK_immediate_flag);
    }
    goto InvalidSignal;
    return;
  }
  case GSN_TRANSID_AI:
  {
    if (likely(tFirstDataPtr != NULL))
    {
      NdbReceiver* const tRec = void2rec(tFirstDataPtr);
      Uint32 magicNumber = tRec->getMagicNumberFromObject();
      Uint32 num_sections = aSignal->m_noOfSections;
      NdbReceiver::ReceiverType type = tRec->getType();

      if (unlikely(magicNumber != tRec->getMagicNumber()))
      {
#ifdef NDB_NO_DROPPED_SIGNAL
        abort();
#endif
        return;
      }
      tCon = tRec->getTransaction(type);
      if (likely(((tCon != NULL) &&
                   tCon->checkState_TransId(
                     ((const TransIdAI*)tDataPtr)->transId))))
      {
        void *owner = (void*)tRec->getOwner();
        Uint32 com;
        if (num_sections > 0)
        {
          if (type == NdbReceiver::NDB_QUERY_OPERATION)
          {
            NdbQueryOperationImpl* impl_owner = (NdbQueryOperationImpl*)owner;
            com = impl_owner->execTRANSID_AI(ptr[0].p, ptr[0].sz);
          }
          else
          {
            com = tRec->execTRANSID_AI(ptr[0].p, ptr[0].sz);
          }
        }
        else
        {
          DBUG_EXECUTE_IF("ndb_delay_transid_ai",
            {
              fprintf(stderr,
                      "NdbImpl::trp_deliver_signal() (%p)"
                      " taking a break before TRANSID_AI\n",
                      this);
              NdbSleep_MilliSleep(1000);
              fprintf(stderr, "NdbImpl::trp_deliver_signal() resuming\n");
            });

          /**
           * Note that prior to V7.6.2 we assumed that all 'QUERY'
           * results were returned as 'long' signals. The version
           * check ndbd_spj_api_support_short_TRANSID_AI() function
           * has been added to allow the sender to check if the 
           * QUERY-receiver support short (and 'packed') TRANSID_AI.
           */
          if (type == NdbReceiver::NDB_QUERY_OPERATION)
          {
            NdbQueryOperationImpl* impl_owner = (NdbQueryOperationImpl*)owner;
            com = impl_owner->execTRANSID_AI(tDataPtr + TransIdAI::HeaderLength, 
                                             tLen - TransIdAI::HeaderLength);
          }
          else
          {
            com = tRec->execTRANSID_AI(tDataPtr + TransIdAI::HeaderLength, 
                                       tLen - TransIdAI::HeaderLength);
          }
        }
        {
          BlockReference ref = aSignal->theSendersBlockRef;
          NodeId dbNode = tCon->theDBnode;
          NodeId senderNode = refToNode(ref);
          incClientStat(Ndb::ReadRowCount, 1);
          if (senderNode == dbNode)
          {
            incClientStat(Ndb::TransLocalReadRowCount,1);
          }
        }
        if (com == 0)
        {
          return;
        }
        switch (type)
        {
        case NdbReceiver::NDB_OPERATION:
        case NdbReceiver::NDB_INDEX_OPERATION:
        {
          if (tCon->OpCompleteSuccess() != -1)
          { //More completions pending?
            myNdb->completedTransaction(tCon);
          }
          return;
        }
        case NdbReceiver::NDB_SCANRECEIVER:
        {
          tCon->theScanningOp->receiver_delivered(tRec);
          tNewState = (((WaitSignalType) tWaitState) == WAIT_SCAN ? 
                       (Uint32) NO_WAIT : tWaitState);
          break;
        }
        case NdbReceiver::NDB_QUERY_OPERATION:
        {
          // Handled differently whether it is a scan or lookup
          NdbQueryOperationImpl* impl_owner = (NdbQueryOperationImpl*)owner;
          if (impl_owner->getQueryDef().isScanQuery())
          {
            tNewState = (((WaitSignalType) tWaitState) == WAIT_SCAN ? 
                       (Uint32) NO_WAIT : tWaitState);
            break;
          }
          else
          {
            if (tCon->OpCompleteSuccess() != -1)
            { //More completions pending?
              myNdb->completedTransaction(tCon);
            }
            return;
          }
        }
        default:
        {
          goto InvalidSignal;
        }
        }
        break;
      }
      else
      {
        /**
         * This is ok as transaction can have been aborted before TRANSID_AI
         * arrives (if TUP on  other node than TC)
         */
        return;
      }
    }
    else
    {
      return;
    }
  }
  case GSN_SCAN_TABCONF:
  {
    tCon = void2con(tFirstDataPtr);
    if (unlikely(tFirstDataPtr == NULL))
    {
      goto InvalidSignal;
    }
    Uint32 magicNumber = tCon->getMagicNumberFromObject();
    Uint32 num_sections = aSignal->m_noOfSections;
    Uint32 sz;
    Uint32 *sig_ptr;

    if (unlikely(magicNumber != tCon->getMagicNumber()))
    {
      goto InvalidSignal;
    }
    if (num_sections > 0)
    {
      sig_ptr = ptr[0].p;
      sz = ptr[0].sz;
    }
    else
    {
      sig_ptr = (Uint32*)tDataPtr + ScanTabConf::SignalLength, 
      sz = tLen - ScanTabConf::SignalLength;
    }
    tReturnCode = tCon->receiveSCAN_TABCONF(aSignal, sig_ptr, sz);
    if (tReturnCode != -1 && tWaitState == WAIT_SCAN)
    {
      tNewState = NO_WAIT;
    }
    break;
  }
  case GSN_TC_COMMITCONF:
  {
    const TcCommitConf * const commitConf = (TcCommitConf *)tDataPtr;
    const BlockReference aTCRef = aSignal->theSendersBlockRef;

    if (tFirstDataPtr == 0)
    {
      goto invalid0;
    }
    tCon = void2con(tFirstDataPtr);
    if ((tCon->checkMagicNumber() == 0) &&
        (tCon->theSendStatus == NdbTransaction::sendTC_COMMIT))
    {
      tReturnCode = tCon->receiveTC_COMMITCONF(commitConf, tLen);
      if (unlikely((tFirstData & 1) && send_TC_COMMIT_ACK_immediate_flag))
      {
        NdbTransaction::sendTC_COMMIT_ACK(this,
                                          myNdb->theCommitAckSignal,
                                          commitConf->transId1, 
                                          commitConf->transId2,
                                          aTCRef,
                                          true);
        if (tReturnCode != -1)
        {
          myNdb->completedTransaction(tCon);
        }
        return;
      }
      if (tReturnCode != -1)
      {
        myNdb->completedTransaction(tCon);
      }
      if (tFirstData & 1)
      {
        NdbTransaction::sendTC_COMMIT_ACK(this,
                                          myNdb->theCommitAckSignal,
                                          commitConf->transId1, 
                                          commitConf->transId2,
                                          aTCRef,
                                          false);
      }
      return;
    }
  invalid0:
    if(tFirstData & 1)
    {
      /**
       * We must send TC_COMMIT_ACK regardless if we "reject" signal!
       */
      NdbTransaction::sendTC_COMMIT_ACK(this,
                                        myNdb->theCommitAckSignal,
                                        commitConf->transId1,
                                        commitConf->transId2,
                                        aTCRef,
                                        send_TC_COMMIT_ACK_immediate_flag);
    }
    goto InvalidSignal;
    return;
  }
  case GSN_TCROLLBACKCONF:
  {
    if (tFirstDataPtr == 0)
    {
      goto InvalidSignal;
    }
    tCon = void2con(tFirstDataPtr);
    if ((tCon->checkMagicNumber() == 0) &&
        (tCon->theSendStatus == NdbTransaction::sendTC_ROLLBACK))
    {
      tReturnCode = tCon->receiveTCROLLBACKCONF(aSignal);
      if (tReturnCode != -1)
      {
        myNdb->completedTransaction(tCon);
      }
    }
    return;
  }
  case GSN_KEYINFO20:
  {
    NdbReceiver* tRec;
    if (tFirstDataPtr &&
        (tRec = void2rec(tFirstDataPtr)) &&
        tRec->checkMagicNumber() &&
        (tCon = tRec->getTransaction(tRec->getType())) &&
        tCon->checkState_TransId(&((const KeyInfo20*)tDataPtr)->transId1))
    {
      Uint32 len = ((const KeyInfo20*)tDataPtr)->keyLen;
      Uint32 info = ((const KeyInfo20*)tDataPtr)->scanInfo_Node;
      int com = -1;
      if (aSignal->m_noOfSections > 0 && len == ptr[0].sz)
      {
        com = tRec->execKEYINFO20(info, ptr[0].p, len);
      }
      else if (len == tLen - KeyInfo20::HeaderLength)
      {
        com = tRec->execKEYINFO20(info, tDataPtr+KeyInfo20::HeaderLength, len);
      }
      
      switch(com)
      {
      case 1:
        tCon->theScanningOp->receiver_delivered(tRec);
        tNewState = (((WaitSignalType) tWaitState) == WAIT_SCAN ? 
                     (Uint32) NO_WAIT : tWaitState);
	break;
      case 0:
        break;
      case -1:
        goto InvalidSignal;
      }
      break;
    }
    else
    {
      /**
       * This is ok as transaction can have been aborted before KEYINFO20
       * arrives (if TUP on  other node than TC)
       */
      return;
    }
  }
  case GSN_TCKEYREF:
  {
    if (tFirstDataPtr == 0)
    {
      goto InvalidSignal;
    }
    const NdbReceiver* const receiver = void2rec(tFirstDataPtr);
    if (!receiver->checkMagicNumber())
    {
      goto InvalidSignal; 
    }
    tCon = receiver->getTransaction(receiver->getType());
    if (tCon != NULL)
    {
      if (tCon->theSendStatus == NdbTransaction::sendTC_OP)
      {
        if (receiver->getType()==NdbReceiver::NDB_QUERY_OPERATION)
        {
          NdbQueryOperationImpl* tmp =
            (NdbQueryOperationImpl*)(receiver->m_owner);
          if (tmp->execTCKEYREF(aSignal) &&
              tCon->OpCompleteFailure() != -1)
          {
            myNdb->completedTransaction(tCon);
            return;
          }
        }
        else
        {
          tOp = (NdbOperation*)(receiver->getOwner());
          /* NB! NdbOperation::checkMagicNumber() returns 0 if it *is* 
           * an NdbOperation.*/
          if (tOp->checkMagicNumber() != 0)
            goto InvalidSignal;
          tReturnCode = tOp->receiveTCKEYREF(aSignal);
          if (tReturnCode != -1)
          {
            myNdb->completedTransaction(tCon);
            return;
          }//if
        }//if
        break;
      }
    }
    goto InvalidSignal;
    return;
  } 
  case GSN_TCINDXREF:
  {
    if (tFirstDataPtr == 0)
    {
      goto InvalidSignal;
    }
    const NdbReceiver* const receiver = void2rec(tFirstDataPtr);
    if (!receiver->checkMagicNumber())
    {
      goto InvalidSignal;
    }
    tIndexOp = (NdbIndexOperation*)(receiver->getOwner());
    if (tIndexOp->checkMagicNumber() == 0)
    {
      tCon = tIndexOp->theNdbCon;
      if (tCon != NULL)
      {
        if (tCon->theSendStatus == NdbTransaction::sendTC_OP)
        {
          tReturnCode = tIndexOp->receiveTCINDXREF(aSignal);
          if (tReturnCode != -1)
          {
            myNdb->completedTransaction(tCon);
          }
          return;
        }
      }
    }
    goto InvalidSignal;
    return;
  } 
  case GSN_TC_COMMITREF:
  {
    if (tFirstDataPtr == 0)
    {
      goto InvalidSignal;
    }
    tCon = void2con(tFirstDataPtr);
    if ((tCon->checkMagicNumber() == 0) &&
        (tCon->theSendStatus == NdbTransaction::sendTC_COMMIT))
    {
      tReturnCode = tCon->receiveTC_COMMITREF(aSignal);
      if (tReturnCode != -1)
      {
        myNdb->completedTransaction(tCon);
      }
    }
    return;
  }
  case GSN_TCROLLBACKREF:
  {
    if (tFirstDataPtr == 0)
    {
      goto InvalidSignal;
    }
    tCon = void2con(tFirstDataPtr);
    if ((tCon->checkMagicNumber() == 0) &&
        (tCon->theSendStatus == NdbTransaction::sendTC_ROLLBACK))
    {
      tReturnCode = tCon->receiveTCROLLBACKREF(aSignal);
      if (tReturnCode != -1)
      {
        myNdb->completedTransaction(tCon);
      }
    }
    return;
  }
  case GSN_TCROLLBACKREP:
  {
    if (tFirstDataPtr == 0)
    {
      goto InvalidSignal;
    }
    tCon = void2con(tFirstDataPtr);
    if (tCon->checkMagicNumber() == 0)
    {
      tReturnCode = tCon->receiveTCROLLBACKREP(aSignal);
      if (tReturnCode != -1)
      {
        myNdb->completedTransaction(tCon);
      }
    }
    return;
  }
  case GSN_SCAN_TABREF:
  {
    if (tFirstDataPtr == 0)
    {
      goto InvalidSignal;
    }
    tCon = void2con(tFirstDataPtr);
    if (tCon->checkMagicNumber() == 0)
    {
      tReturnCode = tCon->receiveSCAN_TABREF(aSignal);
      if (tReturnCode != -1 && tWaitState == WAIT_SCAN)
      {
        tNewState = NO_WAIT;
      }
      break;
    }
    goto InvalidSignal;
  }
  case GSN_TCSEIZECONF:
  {
    if (tFirstDataPtr == 0)
    {
      goto InvalidSignal;
    }
    if (tWaitState != WAIT_TC_SEIZE)
    {
      goto InvalidSignal;
    }
    tCon = void2con(tFirstDataPtr);
    if (tCon->checkMagicNumber() != 0)
    {
      goto InvalidSignal;
    }
    tReturnCode = tCon->receiveTCSEIZECONF(aSignal);
    if (tReturnCode != -1)
    {
      tNewState = NO_WAIT;
    }
    else
    {
      goto InvalidSignal;
    }
    break;
  }
  case GSN_TCSEIZEREF:
  {
    if (tFirstDataPtr == 0)
    {
      goto InvalidSignal;
    }
    if (tWaitState != WAIT_TC_SEIZE)
    {
      return;
    }
    tCon = void2con(tFirstDataPtr);
    if (tCon->checkMagicNumber() != 0)
    {
      return;
    }
    tReturnCode = tCon->receiveTCSEIZEREF(aSignal);
    if (tReturnCode != -1)
    {
      tNewState = NO_WAIT;
    }
    else
    {
      return;
    }
    break;
  }
  case GSN_TCRELEASECONF:
  {
    if (tFirstDataPtr == 0)
    {
      goto InvalidSignal;
    }
    if (tWaitState != WAIT_TC_RELEASE)
    {
      goto InvalidSignal;
    }
    tCon = void2con(tFirstDataPtr);
    if (tCon->checkMagicNumber() != 0)
    {
      goto InvalidSignal;
    }
    tReturnCode = tCon->receiveTCRELEASECONF(aSignal);
    if (tReturnCode != -1)
    {
      tNewState = NO_WAIT;
    }
    break;
  } 
  case GSN_TCRELEASEREF:
  {
    if (tFirstDataPtr == 0)
    {
      goto InvalidSignal;
    }
    if (tWaitState != WAIT_TC_RELEASE)
    {
      goto InvalidSignal;
    }
    tCon = void2con(tFirstDataPtr);
    if (tCon->checkMagicNumber() != 0)
    {
      goto InvalidSignal;
    }
    tReturnCode = tCon->receiveTCRELEASEREF(aSignal);
    if (tReturnCode != -1)
    {
      tNewState = NO_WAIT;
    }
    break;
  }
  case GSN_TCKEY_FAILCONF:
  {
    const TcKeyFailConf * failConf = (TcKeyFailConf *)tDataPtr;
    const BlockReference aTCRef = aSignal->theSendersBlockRef;
    if (tFirstDataPtr != 0)
    {
      const NdbReceiver* const receiver = void2rec(tFirstDataPtr);
      if (!receiver->checkMagicNumber())
      {
        goto InvalidSignal;
      }
      tOp = (NdbOperation*)(receiver->getOwner());
      if (tOp->checkMagicNumber(false) == 0)
      {
        tCon = tOp->theNdbCon;
        if (tCon != NULL)
        {
          if ((tCon->theSendStatus == NdbTransaction::sendTC_OP) ||
              (tCon->theSendStatus == NdbTransaction::sendTC_COMMIT))
          {
            tReturnCode = tCon->receiveTCKEY_FAILCONF(failConf);
            if(tFirstData & 1)
            {
              NdbTransaction::sendTC_COMMIT_ACK(this,
                                                myNdb->theCommitAckSignal,
                                                failConf->transId1, 
                                                failConf->transId2,
                                                aTCRef,
                                            send_TC_COMMIT_ACK_immediate_flag);
            }
            if (tReturnCode != -1)
            {
              myNdb->completedTransaction(tCon);
            }
            return;
          }
        }
      }
    }
    else
    {
#ifdef VM_TRACE
      ndbout_c("Recevied TCKEY_FAILCONF wo/ operation");
#endif
    }
    if(tFirstData & 1)
    {
      NdbTransaction::sendTC_COMMIT_ACK(this,
                                        myNdb->theCommitAckSignal,
                                        failConf->transId1, 
                                        failConf->transId2,
                                        aTCRef,
                                        send_TC_COMMIT_ACK_immediate_flag);
    }
    return;
  }
  case GSN_TCKEY_FAILREF:
  {
    if (tFirstDataPtr != 0)
    {
      const NdbReceiver* const receiver = void2rec(tFirstDataPtr);
      if (!receiver->checkMagicNumber())
      {
        goto InvalidSignal;
      }
      tOp = (NdbOperation*)(receiver->getOwner());
      if (tOp->checkMagicNumber(false) == 0)
      {
        tCon = tOp->theNdbCon;
        if (tCon != NULL)
        {
          if ((tCon->theSendStatus == NdbTransaction::sendTC_OP) ||
              (tCon->theSendStatus == NdbTransaction::sendTC_ROLLBACK))
          {
            tReturnCode = tCon->receiveTCKEY_FAILREF(aSignal);
            if (tReturnCode != -1)
            {
              myNdb->completedTransaction(tCon);
              return;
            }
          }
        }
      }
    }
#ifdef VM_TRACE
    ndbout_c("Recevied TCKEY_FAILREF wo/ operation");
#endif
    return;
  }
  case GSN_CLOSE_COMREQ:
  {
    m_transporter_facade->perform_close_clnt(this);
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
  case GSN_INDEX_STAT_CONF:
  case GSN_INDEX_STAT_REF:
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
  case GSN_SCHEMA_TRANS_BEGIN_CONF:
  case GSN_SCHEMA_TRANS_BEGIN_REF:
  case GSN_SCHEMA_TRANS_END_CONF:
  case GSN_SCHEMA_TRANS_END_REF:
  case GSN_SCHEMA_TRANS_END_REP:
  case GSN_WAIT_GCP_CONF:
  case GSN_WAIT_GCP_REF:
  case GSN_CREATE_HASH_MAP_REF:
  case GSN_CREATE_HASH_MAP_CONF:
  case GSN_CREATE_FK_REF:
  case GSN_CREATE_FK_CONF:
  case GSN_DROP_FK_REF:
  case GSN_DROP_FK_CONF:
  {
    NdbDictInterface::execSignal(&myNdb->theDictionary->m_receiver,
				 aSignal,
                                 ptr);
    return;
  }  
  case GSN_SUB_REMOVE_CONF:
  case GSN_SUB_REMOVE_REF:
  {
    return; // ignore these signals
  }
  case GSN_SUB_START_CONF:
  case GSN_SUB_START_REF:
  case GSN_SUB_STOP_CONF:
  case GSN_SUB_STOP_REF:
  {
    const Uint64 latestGCI = myNdb->getLatestGCI();
    NdbDictInterface::execSignal(&myNdb->theDictionary->m_receiver,
				 aSignal,
                                 ptr);
    if (tWaitState == WAIT_EVENT && myNdb->getLatestGCI() != latestGCI)
    {
      tNewState = NO_WAIT;
      break;
    }
    return;
  }
  case GSN_SUB_GCP_COMPLETE_REP:
  {
    const Uint64 latestGCI = myNdb->getLatestGCI();
    const SubGcpCompleteRep * const rep=
      CAST_CONSTPTR(SubGcpCompleteRep, aSignal->getDataPtr());
    myNdb->theEventBuffer->execSUB_GCP_COMPLETE_REP(rep, tLen);
    if (tWaitState == WAIT_EVENT && myNdb->getLatestGCI() != latestGCI)
    {
      tNewState = NO_WAIT;
      break;
    }
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
      g_eventLogger->error("dropped GSN_SUB_TABLE_DATA due to wrong magic "
                           "number");
      DBUG_EXECUTE_IF("ndb_crash_on_drop_SUB_TABLE_DATA", DBUG_SUICIDE(););
      return ;
    }

    // Accumulate DIC_TAB_INFO for TE_ALTER events
    if (SubTableData::getOperation(sdata->requestInfo) == 
	NdbDictionary::Event::_TE_ALTER &&
        !op->execSUB_TABLE_DATA(aSignal, ptr))
    {
      return;
    }
    
    LinearSectionPtr copy[3];
    for (int i = 0; i<aSignal->m_noOfSections; i++)
    {
      copy[i] = ptr[i];
    }
    for (int i = aSignal->m_noOfSections; i < 3; i++)
    {
      copy[i].p = NULL;
      copy[i].sz = 0;
    }
    DBUG_PRINT("info",("oid=senderData: %d, gci{hi/lo}: %d/%d, operation: %d, "
		       "tableId: %d",
		       sdata->senderData, sdata->gci_hi, sdata->gci_lo,
		       SubTableData::getOperation(sdata->requestInfo),
		       sdata->tableId));

    myNdb->theEventBuffer->insertDataL(op, sdata, tLen, copy);
    return;
  }
  case GSN_API_REGCONF:
  case GSN_CONNECT_REP:
  {
    return; // Ignore
  }
  case GSN_NODE_FAILREP:
  {
    const NodeFailRep *rep = CAST_CONSTPTR(NodeFailRep,
                                           aSignal->getDataPtr());
    Uint32 len = NodeFailRep::getNodeMaskLength(aSignal->getLength());
    assert(len == NodeBitmask::Size); // only full length in ndbapi
    for (Uint32 i = BitmaskImpl::find_first(len, rep->theAllNodes);
         i != BitmaskImpl::NotFound;
         i = BitmaskImpl::find_next(len, rep->theAllNodes, i + 1))
    {
      if (i <= MAX_DATA_NODE_ID)
      {
        // Ndbif only cares about data-nodes (so far??)
        myNdb->report_node_failure(i);
      }
    }

    NdbDictInterface::execSignal(&myNdb->theDictionary->m_receiver,
                                 aSignal,
                                 ptr);
    break;
  }
  case GSN_NF_COMPLETEREP:
  {
    const NFCompleteRep *rep = CAST_CONSTPTR(NFCompleteRep,
                                             aSignal->getDataPtr());
    myNdb->report_node_failure_completed(rep->failedNodeId);
    break;
  }
  case GSN_TAKE_OVERTCCONF:
  {
    myNdb->abortTransactionsAfterNodeFailure(tFirstData); // theData[0]
    break;
  }
  case GSN_ALLOC_NODEID_CONF:
  {
    const AllocNodeIdConf *rep = CAST_CONSTPTR(AllocNodeIdConf,
                                               aSignal->getDataPtr());
    Uint32 nodeId = rep->nodeId;
    myNdb->connected(numberToRef(myNdb->theNdbBlockNumber, nodeId));
    break;
  }
  default:
  {
    tFirstDataPtr = NULL;
    goto InvalidSignal;
  }
  }//swich

  if (tNewState != tWaitState)
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
    theWaiter.signal(tNewState);
  }
  return;

InvalidSignal:
#ifdef VM_TRACE
  ndbout_c("Ndbif: Error NdbImpl::trp_deliver_signal "
	   "(tFirstDataPtr=%p, GSN=%d, theWaiter.m_state=%d)"
	   " sender = (Block: %d Node: %d)",
           tFirstDataPtr,
	   tSignalNumber,
	   tWaitState,
	   refToBlock(aSignal->theSendersBlockRef),
	   refToNode(aSignal->theSendersBlockRef));
#endif
#ifdef NDB_NO_DROPPED_SIGNAL
  abort();
#endif
  
  return;
}

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

    if (theImpl->wakeHandler == 0)
    {
      if ((theMinNoOfEventsToWakeUp != 0) &&
          (theNoOfCompletedTransactions >= theMinNoOfEventsToWakeUp))
      {
        theMinNoOfEventsToWakeUp = 0;
        theImpl->theWaiter.signal(NO_WAIT);
        return;
      }
    }
    else
    {
      /**
       * This is for multi-wait handling
       */
      theImpl->wakeHandler->notifyTransactionCompleted(this);
    }
  } else {
    ndbout << "theNoOfSentTransactions = " << (int) theNoOfSentTransactions;
    ndbout << " theListState = " << (int) aCon->theListState;
    ndbout << " theTransArrayIndex = " << aCon->theTransArrayIndex;
    ndbout << endl << flush;
#ifdef VM_TRACE
    printState("completedTransaction abort");
    //abort();
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
  const Uint32 timeout = theImpl->get_ndbapi_config_parameters().m_waitfor_timeout;
  const Uint64 current_time = NdbTick_CurrentMillisecond();
  assert(current_time >= the_last_check_time);
#ifndef DBUG_OFF
  if(DBUG_EVALUATE_IF("early_trans_timeout", true, false))
  {
    fprintf(stderr, "Forcing immediate timeout check in Ndb::check_send_timeout()\n");
    the_last_check_time = current_time - 1000 - 1;
  }
#endif
  if (current_time - the_last_check_time > 1000) {
    the_last_check_time = current_time;
    Uint32 no_of_sent = theNoOfSentTransactions;
    for (Uint32 i = 0; i < no_of_sent; i++) {
      NdbTransaction* a_con = theSentTransactionsArray[i];
#ifndef DBUG_OFF
      if(DBUG_EVALUATE_IF("early_trans_timeout", true, false))
      {
        fprintf(stderr, "Inducing early timeout in Ndb::check_send_timeout()\n");
        a_con->theStartTransTime = current_time - timeout - 1;
      }
#endif
      if ((current_time - a_con->theStartTransTime) > timeout)
      {
#ifdef VM_TRACE
        a_con->printState();
	Uint32 t1 = (Uint32) a_con->theTransactionId;
	Uint32 t2 = a_con->theTransactionId >> 32;
	ndbout_c("4012 [%.8x %.8x]", t1, t2);
	//abort();
#endif
        a_con->theReleaseOnClose = true;
	a_con->theError.code = 4012;
        a_con->setOperationErrorCodeAbort(4012);
	a_con->theCommitStatus = NdbTransaction::NeedAbort;
        a_con->theCompletionStatus = NdbTransaction::CompletedFailure;
        a_con->theReturnStatus = NdbTransaction::ReturnFailure;
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
  // Always called when holding the trp_client::lock()
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
  theCachedMinDbNodeVersion = theImpl->m_transporter_facade->getMinDbNodeVersion();
  Uint32 no_of_prep_trans = theNoOfPreparedTransactions;
  for (i = 0; i < no_of_prep_trans; i++) {
    NdbTransaction * a_con = thePreparedTransactionsArray[i];
    thePreparedTransactionsArray[i] = NULL;
    Uint32 node_id = a_con->getConnectedNodeId();
    if ((theImpl->getNodeSequence(node_id) == a_con->theNodeSequence) &&
        (theImpl->get_node_alive(node_id) || theImpl->get_node_stopping(node_id)))
    {
      /*
      We will send if
      1) Node is alive and sequences are correct OR
      2) Node is stopping and we only want to commit or abort
      In a graceful stop situation we want to ensure quick aborts
      of all transactions and commits and thus we allow aborts and
      commits to continue but not normal operations.
      */
      if (theImpl->check_send_size(node_id, a_con->get_send_size())) {
        if (a_con->doSend() == 0) {
          const Uint64 current_time = NdbTick_CurrentMillisecond();
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
    a_con->theReturnStatus = NdbTransaction::ReturnFailure;
    a_con->theCompletionStatus = NdbTransaction::CompletedFailure;
    a_con->handleExecuteCompletion();
    insert_completed_list(a_con);
  }//for
  theNoOfPreparedTransactions = 0;
  int did_send = theImpl->do_forceSend(forceSend);
  if(forceSend) {
    theImpl->incClientStat(Ndb::ForcedSendsCount, 1);
  }
  else {
    theImpl->incClientStat(did_send ? Ndb::UnforcedSendsCount : Ndb::DeferredSendsCount, 1);
  }
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
  theImpl->theWaiter.set_node(0);
  theImpl->theWaiter.set_state(WAIT_TRANS);

  /**
   * theImpl->theWaiter.set_node(0)
   * To ensure no messup with synchronous node fail handling
   * (see ReportFailure)
   */
  int waitTime = aMilliSecondsToWait;
  const NDB_TICKS start = NdbTick_getCurrentTicks();
  theMinNoOfEventsToWakeUp = noOfEventsToWaitFor;
  theImpl->incClientStat(Ndb::WaitExecCompleteCount, 1);
  do {
    int maxsleep = waitTime;
#ifndef DBUG_OFF
    if(DBUG_EVALUATE_IF("early_trans_timeout", true, false))
    {
      maxsleep = waitTime > 10 ? 10 : waitTime;
    }
#endif
    poll_guard->wait_for_input(maxsleep);
    if (theNoOfCompletedTransactions >= (Uint32)noOfEventsToWaitFor) {
      break;
    }//if
    theMinNoOfEventsToWakeUp = noOfEventsToWaitFor;
    const NDB_TICKS now = NdbTick_getCurrentTicks();
    waitTime = aMilliSecondsToWait - 
      (int)NdbTick_Elapsed(start,now).milliSec();
#ifndef DBUG_OFF
    if(DBUG_EVALUATE_IF("early_trans_timeout", true, false))
    {
      fprintf(stderr, "Inducing early timeout in Ndb::waitCompletedTransactions()\n");
      break;
    }
#endif
  } while (waitTime > 0);
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
  theImpl->lock();
  sendPrepTrans(forceSend);
  theImpl->unlock();
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
  PollGuard pg(* theImpl);
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
  theMinNoOfEventsToWakeUp = 0; // no more wakup
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
  PollGuard pg(* theImpl);
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
  Uint32 send_size = 1; // Always sends one signal only
  // Protected area
  /*
    The PollGuard has an implicit call of unlock_and_signal through the
    ~PollGuard method. This method is called implicitly by the compiler
    in all places where the object is out of context due to a return,
    break, continue or simply end of statement block
  */
  theImpl->incClientStat(WaitMetaRequestCount, 1);
  PollGuard poll_guard(* theImpl);

  /**
   * Either we supply the correct conn_seq and ret_conn_seq == 0
   *     or we supply conn_seq == 0 and ret_conn_seq != 0
   */
  read_conn_seq= theImpl->getNodeSequence(node_id);
  bool ok =
    (conn_seq == read_conn_seq && ret_conn_seq == 0) ||
    (conn_seq == 0 && ret_conn_seq != 0);

  if (ret_conn_seq)
    *ret_conn_seq= read_conn_seq;
  if ((theImpl->get_node_alive(node_id)) && ok)
  {
    if (theImpl->check_send_size(node_id, send_size)) {
      return_code = theImpl->sendSignal(aSignal, node_id);
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
    if ((theImpl->get_node_stopping(node_id)) && ok)
    {
      return_code = -5;
    } else {
      return_code = -2;
    }//if
  }//if
  return return_code;
  // End of protected area
}//Ndb::sendRecSignal()

void
NdbTransaction::sendTC_COMMIT_ACK(NdbImpl * impl,
                                  NdbApiSignal * aSignal,
                                  Uint32 transId1, Uint32 transId2,
                                  Uint32 aTCRef,
                                  bool send_immediate)
{
#ifdef MARKER_TRACE
  ndbout_c("Sending TC_COMMIT_ACK(0x%.8x, 0x%.8x) to -> %d",
	   transId1,
	   transId2,
	   refToNode(aTCRef));
#endif  
  aSignal->theTrace                = TestOrd::TraceAPI;
  aSignal->theReceiversBlockNumber = refToBlock(aTCRef);
  aSignal->theVerId_signalNumber   = GSN_TC_COMMIT_ACK;
  aSignal->theLength               = 2;

  Uint32 * dataPtr = aSignal->getDataPtrSend();
  dataPtr[0] = transId1;
  dataPtr[1] = transId2;
  if (likely(!send_immediate))
  {
    impl->safe_noflush_sendSignal(aSignal, refToNode(aTCRef));
  }
  else
  {
    /**
     * To protect against TC_COMMIT_ACK being raced by DUMP_STATE_ORD
     * we route TC_COMMIT_ACK through the same path as DUMP_STATE_ORD.
     */
    dataPtr[2] = aTCRef;
    aSignal->theLength = 3;
    aSignal->theReceiversBlockNumber = CMVMI;
    impl->safe_sendSignal(aSignal, refToNode(aTCRef));
  }
}

void NdbImpl::set_TC_COMMIT_ACK_immediate(bool flag)
{
  send_TC_COMMIT_ACK_immediate_flag = flag;
}

int
NdbImpl::send_dump_state_all(Uint32 *dumpStateCodeArray,
                             Uint32 len)
{
  NdbApiSignal aSignal(m_ndb.theMyRef);
  init_dump_state_signal(&aSignal, dumpStateCodeArray, len);
  return send_to_nodes(&aSignal, false, true);
}

void
NdbImpl::init_dump_state_signal(NdbApiSignal *aSignal,
                                Uint32 *dumpStateCodeArray,
                                Uint32 len)
{
  Uint32 *theData = aSignal->getDataPtrSend();
  aSignal->theTrace                = TestOrd::TraceAPI;
  aSignal->theReceiversBlockNumber = CMVMI;
  aSignal->theVerId_signalNumber   = GSN_DUMP_STATE_ORD;
  aSignal->theLength               = len;
  for (Uint32 i = 0; i < 25; i++)
  {
    if (i < len)
    {
      theData[i] = dumpStateCodeArray[i];
    }
    else
    {
      theData[i] = 0;
    }
  }
}

int
NdbImpl::send_event_report(bool is_poll_owner,
                           Uint32 *data, Uint32 length)
{
  NdbApiSignal aSignal(m_ndb.theMyRef);
  aSignal.theTrace                = TestOrd::TraceAPI;
  aSignal.theReceiversBlockNumber = CMVMI;
  aSignal.theVerId_signalNumber   = GSN_EVENT_REP;
  aSignal.theLength               = length;
  memcpy((char *)aSignal.getDataPtrSend(), (char *)data, length*4);

  return send_to_nodes(&aSignal, is_poll_owner, false);
}

/**
 * Return 0 to indicate success, 1 means no successful send.
 * If send_to_all is true success means successfully sent to
 * all nodes.
 */
int
NdbImpl::send_to_nodes(NdbApiSignal *aSignal,
                       bool is_poll_owner,
                       bool send_to_all)
{
  int ret;
  Uint32 tNode;

  if (!is_poll_owner)
  {
    /**
     * NdbImpl inherits from trp_client and this object needs to be locked
     * before we can send to a node. If we call this when we are poll owner
     * we need not lock anything more.
     */
    lock();
  }
  Ndb_cluster_connection_node_iter node_iter;
  m_ndb_cluster_connection.init_get_next_node(node_iter);
  while ((tNode= m_ndb_cluster_connection.get_next_node(node_iter)))
  {
    if (send_to_node(aSignal, tNode, is_poll_owner) == 0)
    {
      /* Successful send */
      if (!send_to_all)
      {
        ret = 0;
        goto done;
      }
    }
    else if (send_to_all)
    {
      ret = 1;
      goto done;
    }
  }
  if (send_to_all)
  {
    ret = 0;
  }
  else
  {
    ret = 1;
  }
done:
  if (!is_poll_owner)
  {
    flush_send_buffers();
    unlock();
  }
  return ret;
}

/**
 * Return 0 to indicate success, nonzero means no success.
 */
int
NdbImpl::send_to_node(NdbApiSignal *aSignal,
                      Uint32 tNode,
                      bool is_poll_owner)
{
  int ret_code = 1;
  if (get_node_alive(tNode))
  {
    if (is_poll_owner)
    {
      ret_code = safe_sendSignal(aSignal, tNode);
    }
    else
    {
      ret_code = raw_sendSignal(aSignal, tNode);
    }
  }
  return ret_code;
}

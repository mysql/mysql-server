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
Name:          NdbConnection.C
Include:
Link:
Author:        UABMNST Mona Natterkvist UAB/B/UL                         
Date:          970829
Version:       0.1
Description:   Interface between TIS and NDB
Documentation:
Adjust:  971022  UABMNST   First version.
*****************************************************************************/
#include "NdbOut.hpp"
#include "NdbConnection.hpp"
#include "NdbOperation.hpp"
#include "NdbScanOperation.hpp"
#include "NdbIndexOperation.hpp"
#include "NdbApiSignal.hpp"
#include "TransporterFacade.hpp"
#include "API.hpp"
#include "NdbBlob.hpp"
#include <ndb_limits.h>

#include <signaldata/TcKeyConf.hpp>
#include <signaldata/TcIndx.hpp>
#include <signaldata/TcCommit.hpp>
#include <signaldata/TcKeyFailConf.hpp>
#include <signaldata/TcHbRep.hpp>

/*****************************************************************************
NdbConnection( Ndb* aNdb );

Return Value:  None
Parameters:    aNdb: Pointers to the Ndb object 
Remark:        Creates a connection object. 
*****************************************************************************/
NdbConnection::NdbConnection( Ndb* aNdb ) :
  theSendStatus(NotInit),
  theCallbackFunction(NULL),
  theCallbackObject(NULL),
  theTransArrayIndex(0),
  theStartTransTime(0),
  theErrorLine(0),
  theErrorOperation(NULL),
  theNdb(aNdb),
  theNext(NULL),
  theFirstOpInList(NULL),
  theLastOpInList(NULL),
  theFirstExecOpInList(NULL),
  theLastExecOpInList(NULL),
  theCompletedFirstOp(NULL),
  theNoOfOpSent(0),
  theNoOfOpCompleted(0),
  theNoOfOpFetched(0),
  theMyRef(0),
  theTCConPtr(0),
  theTransactionId(0),
  theGlobalCheckpointId(0),
  theStatus(NotConnected),
  theCompletionStatus(NotCompleted), 
  theCommitStatus(NotStarted),
  theMagicNumber(0xFE11DC),
  theTransactionIsStarted(false),
  theDBnode(0),
  theReleaseOnClose(false),
  // Cursor operations
  m_waitForReply(true),
  m_theFirstCursorOperation(NULL),
  m_theLastCursorOperation(NULL),
  m_firstExecutedCursorOp(NULL),
  // Scan operations
  theScanFinished(0),
  theCurrentScanRec(NULL),
  thePreviousScanRec(NULL),
  theScanningOp(NULL),
  theBuddyConPtr(0xFFFFFFFF),
  theBlobFlag(false)
{
  theListState = NotInList;
  theError.code = 0;
  theId = theNdb->theNdbObjectIdMap->map(this);
}//NdbConnection::NdbConnection()

/*****************************************************************************
~NdbConnection();

Remark:        Deletes the connection object. 
*****************************************************************************/
NdbConnection::~NdbConnection()
{
  theNdb->theNdbObjectIdMap->unmap(theId, this);
}//NdbConnection::~NdbConnection()

/*****************************************************************************
void init();

Remark:         Initialise connection object for new transaction. 
*****************************************************************************/
void	
NdbConnection::init()
{
  theListState            = NotInList;
  theInUseState           = true;
  theTransactionIsStarted = false;
  theScanFinished = 0;
  theNext		  = NULL;

  theFirstOpInList	  = NULL;
  theLastOpInList	  = NULL;

  theScanningOp            = NULL;

  theFirstExecOpInList	  = NULL;
  theLastExecOpInList	  = NULL;

  theCurrentScanRec  = NULL;
  thePreviousScanRec = NULL;

  theCompletedFirstOp	  = NULL;

  theGlobalCheckpointId   = 0;
  theCommitStatus         = Started;
  theCompletionStatus     = NotCompleted;
  m_abortOption           = AbortOnError;

  theError.code		  = 0;
  theErrorLine		  = 0;
  theErrorOperation	  = NULL;

  theReleaseOnClose       = false;
  theSimpleState          = true;
  theSendStatus           = InitState;
  theMagicNumber          = 0x37412619;
  // Cursor operations
  m_waitForReply            = true;
  m_theFirstCursorOperation = NULL;
  m_theLastCursorOperation  = NULL;
  m_firstExecutedCursorOp   = 0;
  theBuddyConPtr            = 0xFFFFFFFF;
  //
  theBlobFlag = false;
}//NdbConnection::init()

/*****************************************************************************
setOperationErrorCode(int anErrorCode);

Remark:        Sets an error code on the connection object from an 
               operation object. 
*****************************************************************************/
void
NdbConnection::setOperationErrorCode(int anErrorCode)
{
  if (theError.code == 0)
    theError.code = anErrorCode;
}//NdbConnection::setOperationErrorCode()

/*****************************************************************************
setOperationErrorCodeAbort(int anErrorCode);

Remark:        Sets an error code on the connection object from an 
               operation object. 
*****************************************************************************/
void
NdbConnection::setOperationErrorCodeAbort(int anErrorCode)
{
  if (theTransactionIsStarted == false) {
    theCommitStatus = Aborted;
  } else if ((m_abortOption == AbortOnError) && 
	     (theCommitStatus != Committed) &&
             (theCommitStatus != Aborted)) {
    theCommitStatus = NeedAbort;
  }//if
  if (theError.code == 0)
    theError.code = anErrorCode;
}//NdbConnection::setOperationErrorCodeAbort()

/*****************************************************************************
setErrorCode(int anErrorCode);

Remark:        Sets an error indication on the connection object. 
*****************************************************************************/
void
NdbConnection::setErrorCode(int anErrorCode)
{
  if (theError.code == 0)
    theError.code = anErrorCode;
}//NdbConnection::setErrorCode()

/*****************************************************************************
void handleExecuteCompletion(void);

Remark:        Handle time-out on a transaction object. 
*****************************************************************************/
void
NdbConnection::handleExecuteCompletion()
{
  
  if (theCompletionStatus == CompletedFailure) {
    NdbOperation* tOpTemp = theFirstExecOpInList;
    while (tOpTemp != NULL) {
/*****************************************************************************
 *	Ensure that all executing operations report failed for each 
 *      read attribute when failure occurs. 
 *      We do not want any operations to report both failure and 
 *      success on different read attributes.
 ****************************************************************************/
      tOpTemp->handleFailedAI_ElemLen();
      tOpTemp = tOpTemp->next();
    }//while
    theReturnStatus = ReturnFailure;
  }//if
  /***************************************************************************
   *	  Move the NdbOperation objects from the list of executing 
   *      operations to list of completed
   **************************************************************************/
  NdbOperation* tFirstExecOp = theFirstExecOpInList;
  NdbOperation* tLastExecOp = theLastExecOpInList;
  if (tLastExecOp != NULL) {
    tLastExecOp->next(theCompletedFirstOp);
    theCompletedFirstOp = tFirstExecOp;
    theFirstExecOpInList = NULL;
    theLastExecOpInList = NULL;
  }//if
  theSendStatus = InitState;
  return;
}//NdbConnection::handleExecuteCompletion()

/*****************************************************************************
int execute(ExecType aTypeOfExec, CommitType aTypeOfCommit, int forceSend);

Return Value:  Return 0 : execute was successful.
               Return -1: In all other case.  
Parameters :   aTypeOfExec: Type of execute.
Remark:        Initialise connection object for new transaction. 
*****************************************************************************/
int 
NdbConnection::execute(ExecType aTypeOfExec, 
		       AbortOption abortOption,
		       int forceSend)
{
  if (! theBlobFlag)
    return executeNoBlobs(aTypeOfExec, abortOption, forceSend);

  // execute prepared ops in batches, as requested by blobs

  ExecType tExecType;
  NdbOperation* tPrepOp;

  do {
    tExecType = aTypeOfExec;
    tPrepOp = theFirstOpInList;
    while (tPrepOp != NULL) {
      bool batch = false;
      NdbBlob* tBlob = tPrepOp->theBlobList;
      while (tBlob != NULL) {
        if (tBlob->preExecute(tExecType, batch) == -1)
          return -1;
        tBlob = tBlob->theNext;
      }
      if (batch) {
        // blob asked to execute all up to here now
        tExecType = NoCommit;
        break;
      }
      tPrepOp = tPrepOp->next();
    }
    // save rest of prepared ops if batch
    NdbOperation* tRestOp;
    NdbOperation* tLastOp;
    if (tPrepOp != NULL) {
      tRestOp = tPrepOp->next();
      tPrepOp->next(NULL);
      tLastOp = theLastOpInList;
      theLastOpInList = tPrepOp;
    }
    if (tExecType == Commit) {
      NdbOperation* tOp = theCompletedFirstOp;
      while (tOp != NULL) {
        NdbBlob* tBlob = tOp->theBlobList;
        while (tBlob != NULL) {
          if (tBlob->preCommit() == -1)
            return -1;
          tBlob = tBlob->theNext;
        }
        tOp = tOp->next();
      }
    }
    if (executeNoBlobs(tExecType, abortOption, forceSend) == -1)
        return -1;
    {
      NdbOperation* tOp = theCompletedFirstOp;
      while (tOp != NULL) {
        NdbBlob* tBlob = tOp->theBlobList;
        while (tBlob != NULL) {
          // may add new operations if batch
          if (tBlob->postExecute(tExecType) == -1)
            return -1;
          tBlob = tBlob->theNext;
        }
        tOp = tOp->next();
      }
    }
    // add saved prepared ops if batch
    if (tPrepOp != NULL && tRestOp != NULL) {
      if (theFirstOpInList == NULL)
        theFirstOpInList = tRestOp;
      else
        theLastOpInList->next(tRestOp);
      theLastOpInList = tLastOp;
    }
  } while (theFirstOpInList != NULL || tExecType != aTypeOfExec);

  return 0;
}

int 
NdbConnection::executeNoBlobs(ExecType aTypeOfExec, 
                              AbortOption abortOption,
                              int forceSend)
{
//------------------------------------------------------------------------
// We will start by preparing all operations in the transaction defined
// since last execute or since beginning. If this works ok we will continue
// by calling the poll with wait method. This method will return when
// the NDB kernel has completed its task or when 10 seconds have passed.
// The NdbConnectionCallBack-method will receive the return code of the
// transaction. The normal methods of reading error codes still apply.
//------------------------------------------------------------------------
  Ndb* tNdb = theNdb;

  m_waitForReply = false;
  executeAsynchPrepare(aTypeOfExec, NULL, NULL, abortOption);
  if (m_waitForReply){
    while (1) {
      int noOfComp = tNdb->sendPollNdb((3 * WAITFOR_RESPONSE_TIMEOUT),
                                       1, forceSend);
      if (noOfComp == 0) {
        /** 
         * This timeout situation can occur if NDB crashes.
         */
        ndbout << "This timeout should never occur, execute(..)" << endl;
        setOperationErrorCodeAbort(4012);  // Error code for "Cluster Failure"
        return -1;
      }//if

      /*
       * Check that the completed transactions include this one.  There
       * could be another thread running asynchronously.  Even in pure
       * async case rollback is done synchronously.
       */
      if (theListState != NotInList)
        continue;
#ifdef VM_TRACE
      unsigned anyway = 0;
      for (unsigned i = 0; i < theNdb->theNoOfPreparedTransactions; i++)
        anyway += theNdb->thePreparedTransactionsArray[i] == this;
      for (unsigned i = 0; i < theNdb->theNoOfSentTransactions; i++)
        anyway += theNdb->theSentTransactionsArray[i] == this;
      for (unsigned i = 0; i < theNdb->theNoOfCompletedTransactions; i++)
        anyway += theNdb->theCompletedTransactionsArray[i] == this;
      if (anyway) {
        theNdb->printState("execute %x", this);
        abort();
      }
#endif
      if (theReturnStatus == ReturnFailure) {
        return -1;
      }//if
      break;
    }
  }
  return 0;
}//NdbConnection::execute()

/*****************************************************************************
void executeAsynchPrepare(ExecType           aTypeOfExec,
                          NdbAsynchCallback  callBack,
                          void*              anyObject,
                          CommitType         aTypeOfCommit);

Return Value:  No return value
Parameters :   aTypeOfExec:   Type of execute.
               anyObject:     An object provided in the callback method
               callBack:      The callback method
               aTypeOfCommit: What to do when read/updated/deleted records 
                              are missing or inserted records already exist.

Remark:        Prepare a part of a transaction in an asynchronous manner. 
*****************************************************************************/
void 
NdbConnection::executeAsynchPrepare( ExecType           aTypeOfExec,
                                     NdbAsynchCallback  aCallback,
                                     void*              anyObject,
                                     AbortOption abortOption)
{
  /**
   * Reset error.code on execute
   */
  theError.code = 0;
  NdbCursorOperation* tcOp = m_theFirstCursorOperation;
  if (tcOp != 0){
    // Execute any cursor operations
    while (tcOp != NULL) {
      int tReturnCode;
      tReturnCode = tcOp->executeCursor(theDBnode);
      if (tReturnCode == -1) {
        return;
      }//if
      tcOp = (NdbCursorOperation*)tcOp->next();
    } // while
    m_theLastCursorOperation->next(m_firstExecutedCursorOp);
    m_firstExecutedCursorOp = m_theFirstCursorOperation;
    // Discard cursor operations, since these are also
    // in the complete operations list we do not need
    // to release them.
    m_theFirstCursorOperation = m_theLastCursorOperation = NULL;
  }

  bool tTransactionIsStarted = theTransactionIsStarted;
  NdbOperation*	tLastOp = theLastOpInList;
  Ndb* tNdb = theNdb;
  CommitStatusType tCommitStatus = theCommitStatus;
  Uint32 tnoOfPreparedTransactions = tNdb->theNoOfPreparedTransactions;

  theReturnStatus     = ReturnSuccess;
  theCallbackFunction = aCallback;
  theCallbackObject   = anyObject;
  m_abortOption   = abortOption;
  //  SendStatusType tSendStatus = theSendStatus;
  
//  if (tSendStatus != InitState) {
/****************************************************************************
 * The application is obviously doing strange things. We should probably
 * report to the application the problem in some manner. Since we don't have
 * a good way of handling the problem we avoid discovering the problem.
 * Should be handled at some point in time.
 ****************************************************************************/
//    return;
//  }
  m_waitForReply = true;
  tNdb->thePreparedTransactionsArray[tnoOfPreparedTransactions] = this;
  theTransArrayIndex = tnoOfPreparedTransactions;
  theListState = InPreparedList;
  tNdb->theNoOfPreparedTransactions = tnoOfPreparedTransactions + 1;

  if(tCommitStatus == Committed){
    tCommitStatus = Started;
    tTransactionIsStarted = false;
  }

  if ((tCommitStatus != Started) ||
      (aTypeOfExec == Rollback)) {
/*****************************************************************************
 *	Rollback have been ordered on a started transaction. Call rollback.
 *      Could also be state problem or previous problem which leads to the 
 *      same action.
 ****************************************************************************/
    if (aTypeOfExec == Rollback) {
      if (theTransactionIsStarted == false) {
	theCommitStatus = Aborted;
	theSendStatus = sendCompleted;
      } else {
	theSendStatus = sendABORT;
      }
    } else {
      theSendStatus = sendABORTfail;
    }//if
    return;
  }//if
  if (tTransactionIsStarted == true) {
    if (tLastOp != NULL) {
      if (aTypeOfExec == Commit) {
/*****************************************************************************
 *	Set commit indicator on last operation when commit has been ordered
 *      and also a number of operations.
******************************************************************************/
        tLastOp->theCommitIndicator = 1;
      }//if
    } else {
      if (aTypeOfExec == Commit) {
	/**********************************************************************
	 *   A Transaction have been started and no more operations exist. 
	 *   We will use the commit method.
	 *********************************************************************/
        theSendStatus = sendCOMMITstate;
        return;
      } else {
	/**********************************************************************
	 * We need to put it into the array of completed transactions to 
	 * ensure that we report the completion in a proper way. 
	 * We cannot do this here since that would endanger the completed
	 * transaction array since that is also updated from the receiver 
	 * thread and thus we need to do it under mutex lock and thus we 
	 * set the sendStatus to ensure that the send method will
	 * put it into the completed array.
	 **********************************************************************/
        theSendStatus = sendCompleted;
	return;		// No Commit with no operations is OK
      }//if
    }//if
  } else if (tTransactionIsStarted == false) {
    NdbOperation* tFirstOp = theFirstOpInList;
    if (tLastOp != NULL) {
      tFirstOp->setStartIndicator();
      if (aTypeOfExec == Commit) {
        tLastOp->theCommitIndicator = 1;
      }//if
    } else {
      /***********************************************************************
       *    No operations are defined and we have not started yet. 
       *    Simply return OK. Set commit status if Commit.
       ***********************************************************************/
      if (aTypeOfExec == Commit) {
        theCommitStatus = Committed;
      }//if
      /***********************************************************************
       * We need to put it into the array of completed transactions to
       * ensure that we report the completion in a proper way. We
       * cannot do this here since that would endanger the completed
       * transaction array since that is also updated from the
       * receiver thread and thus we need to do it under mutex lock
       * and thus we set the sendStatus to ensure that the send method
       * will put it into the completed array.
       ***********************************************************************/
      theSendStatus = sendCompleted;
      return;
    }//if
  }

  NdbOperation* tOp = theFirstOpInList;
  theCompletionStatus = NotCompleted;
  while (tOp) {
    int tReturnCode;
    NdbOperation* tNextOp = tOp->next();

    tReturnCode = tOp->prepareSend(theTCConPtr, theTransactionId);
    if (tReturnCode == -1) {
      theSendStatus = sendABORTfail;
      return;
    }//if

    /*************************************************************************
     * Now that we have successfully prepared the send of this operation we 
     * move it to the list of executing operations and remove it from the 
     * list of defined operations.
     ************************************************************************/
    tOp = tNextOp;
  } 

  NdbOperation* tLastOpInList = theLastOpInList;
  NdbOperation* tFirstOpInList = theFirstOpInList;

  theFirstOpInList = NULL;
  theLastOpInList = NULL;
  theFirstExecOpInList = tFirstOpInList;
  theLastExecOpInList = tLastOpInList;

  theCompletionStatus = CompletedSuccess;
  theNoOfOpSent		= 0;
  theNoOfOpCompleted	= 0;
  theSendStatus = sendOperations;
  return;
}//NdbConnection::executeAsynchPrepare()

void NdbConnection::close()
{
  theNdb->closeTransaction(this);
}

int NdbConnection::refresh(){
  return sendTC_HBREP();
}

/*****************************************************************************
int sendTC_HBREP();

Return Value:  No return value.  
Parameters :   None.
Remark:        Order NDB to refresh the timeout counter of the transaction. 
******************************************************************************/
int 	
NdbConnection::sendTC_HBREP()		// Send a TC_HBREP signal;
{
  NdbApiSignal* tSignal;
  Ndb* tNdb = theNdb;
  Uint32 tTransId1, tTransId2;

  tSignal = tNdb->getSignal();
  if (tSignal == NULL) {
    return -1;
  }

  if (tSignal->setSignal(GSN_TC_HBREP) == -1) {
    return -1;
  }

  TcHbRep * const tcHbRep = CAST_PTR(TcHbRep, tSignal->getDataPtrSend());
  
  tcHbRep->apiConnectPtr = theTCConPtr;    
  
  tTransId1 = (Uint32) theTransactionId;
  tTransId2 = (Uint32) (theTransactionId >> 32);
  tcHbRep->transId1      = tTransId1;
  tcHbRep->transId2      = tTransId2;
 
  TransporterFacade *tp = TransporterFacade::instance();
  tp->lock_mutex(); 
  const int res = tp->sendSignal(tSignal,theDBnode);
  tp->unlock_mutex(); 
  tNdb->releaseSignal(tSignal);

  if (res == -1){
    return -1;
  }    
  
  return 0;
}//NdbConnection::sendTC_HBREP()

/*****************************************************************************
int doSend();

Return Value:  Return 0 : send was successful.
               Return -1: In all other case.  
Remark:        Send all operations belonging to this connection.
               The caller of this method has the responsibility to remove the
               object from the prepared transactions array on the Ndb-object.
*****************************************************************************/
int
NdbConnection::doSend()
{
  /*
  This method assumes that at least one operation have been defined. This
  is ensured by the caller of this routine (=execute).
  */

  switch(theSendStatus){
  case sendOperations: {
    NdbOperation * tOp = theFirstExecOpInList;
    do {
      NdbOperation* tNextOp = tOp->next();
      const Uint32 lastFlag = ((tNextOp == NULL) ? 1 : 0);
      const int tReturnCode = tOp->doSend(theDBnode, lastFlag);
      if (tReturnCode == -1) {
        theReturnStatus = ReturnFailure;
        break;
      }//if
      tOp = tNextOp;
    } while (tOp != NULL);
    Ndb* tNdb = theNdb;
    theSendStatus = sendTC_OP;
    theTransactionIsStarted = true;
    tNdb->insert_sent_list(this);
    return 0;
  }//case
  case sendABORT:
  case sendABORTfail:{
  /***********************************************************************
   * Rollback have been ordered on a not started transaction. 
   * Simply return OK and set abort status.
   ***********************************************************************/
    if (theSendStatus == sendABORTfail) {
      theReturnStatus = ReturnFailure;
    }//if
    if (sendROLLBACK() == 0) {
      return 0;
    }//if
    break;
  }//case
  case sendCOMMITstate:
    if (sendCOMMIT() == 0) {
      return 0;
    }//if
    break;
  case sendCompleted:
    theNdb->insert_completed_list(this); 
    return 0;
  default:
    ndbout << "Inconsistent theSendStatus = " << theSendStatus << endl;
    abort();
    break;
  }//switch
  setOperationErrorCodeAbort(4002);
  theReleaseOnClose = true;
  theTransactionIsStarted = false;
  theCommitStatus = Aborted;
  return -1;
}//NdbConnection::doSend()

/**************************************************************************
int sendROLLBACK();

Return Value:  Return -1 if send unsuccessful.  
Parameters :   None.
Remark:        Order NDB to rollback the transaction. 
**************************************************************************/
int 	
NdbConnection::sendROLLBACK()      // Send a TCROLLBACKREQ signal;
{
  Ndb* tNdb = theNdb;
  if ((theTransactionIsStarted == true) &&
      (theCommitStatus != Committed) &&
      (theCommitStatus != Aborted)) {
/**************************************************************************
 *	The user did not perform any rollback but simply closed the
 *      transaction. We must rollback Ndb since Ndb have been contacted.
 *************************************************************************/
    NdbApiSignal tSignal(tNdb->theMyRef);
    Uint32 tTransId1, tTransId2;
    TransporterFacade *tp = TransporterFacade::instance();
    int	  tReturnCode;

    tTransId1 = (Uint32) theTransactionId;
    tTransId2 = (Uint32) (theTransactionId >> 32);
    tSignal.setSignal(GSN_TCROLLBACKREQ);
    tSignal.setData(theTCConPtr, 1);
    tSignal.setData(tTransId1, 2);
    tSignal.setData(tTransId2, 3);
    tReturnCode = tp->sendSignal(&tSignal,theDBnode);
    if (tReturnCode != -1) {
      theSendStatus = sendTC_ROLLBACK;
      tNdb->insert_sent_list(this);
      return 0;
    }//if
   /*********************************************************************
    * It was not possible to abort the transaction towards the NDB kernel
    * and thus we put it into the array of completed transactions that
    * are ready for reporting to the application.
    *********************************************************************/
    return -1;
  } else {
    /*
     It is not necessary to abort the transaction towards the NDB kernel and
     thus we put it into the array of completed transactions that are ready
     for reporting to the application.
     */
    theSendStatus = sendCompleted;
    tNdb->insert_completed_list(this);
    return 0;
    ;
  }//if
}//NdbConnection::sendROLLBACK()

/***************************************************************************
int sendCOMMIT();

Return Value:  Return 0 : send was successful.
               Return -1: In all other case.  
Parameters :   None.
Remark:        Order NDB to commit the transaction. 
***************************************************************************/
int 	
NdbConnection::sendCOMMIT()    // Send a TC_COMMITREQ signal;
{
  NdbApiSignal tSignal(theNdb->theMyRef);
  Uint32 tTransId1, tTransId2;
  TransporterFacade *tp = TransporterFacade::instance(); 
  int	  tReturnCode;

  tTransId1 = (Uint32) theTransactionId;
  tTransId2 = (Uint32) (theTransactionId >> 32);
  tSignal.setSignal(GSN_TC_COMMITREQ);
  tSignal.setData(theTCConPtr, 1);
  tSignal.setData(tTransId1, 2);
  tSignal.setData(tTransId2, 3);
      
  tReturnCode = tp->sendSignal(&tSignal,theDBnode);
  if (tReturnCode != -1) {
    theSendStatus = sendTC_COMMIT;
    theNdb->insert_sent_list(this);
    return 0;
  } else {
    return -1;
  }//if
}//NdbConnection::sendCOMMIT()

/******************************************************************************
void release();

Remark:         Release all operations.
******************************************************************************/
void 
NdbConnection::release(){
  if (theTransactionIsStarted == true && theScanningOp != NULL )
    stopScan();
    
  releaseOperations();
  if ( (theTransactionIsStarted == true) &&
      ((theCommitStatus != Committed) &&
       (theCommitStatus != Aborted))) {
/****************************************************************************
 *	The user did not perform any rollback but simply closed the
 *      transaction. We must rollback Ndb since Ndb have been contacted.
******************************************************************************/
    execute(Rollback);
  }//if
  theMagicNumber = 0xFE11DC;
  theInUseState = false;
#ifdef VM_TRACE
  if (theListState != NotInList) {
    theNdb->printState("release %x", this);
    abort();
  }
#endif
}//NdbConnection::release()

void
NdbConnection::releaseOps(NdbOperation* tOp){
  while (tOp != NULL) {
    NdbOperation* tmp = tOp;
    tOp->release();
    tOp = tOp->next();
    theNdb->releaseOperation(tmp);
  }//while
}

/******************************************************************************
void releaseOperations();

Remark:         Release all operations.
******************************************************************************/
void 
NdbConnection::releaseOperations()
{
  // Release any open scans
  releaseCursorOperations(m_theFirstCursorOperation);
  releaseCursorOperations(m_firstExecutedCursorOp);
  
  releaseOps(theCompletedFirstOp);
  releaseOps(theFirstOpInList);
  releaseOps(theFirstExecOpInList);

  theCompletedFirstOp = NULL;
  theFirstOpInList = NULL;
  theFirstExecOpInList = NULL;
  theLastOpInList = NULL;
  theLastExecOpInList = NULL;
  theScanningOp = NULL;
  m_theFirstCursorOperation = NULL;
  m_theLastCursorOperation = NULL;
  m_firstExecutedCursorOp = NULL;
}//NdbConnection::releaseOperations()

void 
NdbConnection::releaseCompletedOperations()
{
  releaseOps(theCompletedFirstOp);
  theCompletedFirstOp = NULL;
}//NdbConnection::releaseOperations()

/******************************************************************************
void releaseCursorOperations();

Remark:         Release all cursor operations. 
                (NdbScanOperation and NdbIndexOperation)
******************************************************************************/
void 
NdbConnection::releaseCursorOperations(NdbCursorOperation* cursorOp)
{
  while(cursorOp != 0){
    NdbCursorOperation* next = (NdbCursorOperation*)cursorOp->next();
    cursorOp->release();
    if (cursorOp->cursorType() == NdbCursorOperation::ScanCursor)
      theNdb->releaseScanOperation((NdbScanOperation*)cursorOp);
    else 
      theNdb->releaseOperation(cursorOp);
    cursorOp = next;
  }
}//NdbConnection::releaseCursorOperations()

/*****************************************************************************
NdbOperation* getNdbOperation(const char* aTableName);

Return Value    Return a pointer to a NdbOperation object if getNdbOperation 
                was succesful.
                Return NULL : In all other case. 	
Parameters:     aTableName : Name of the database table. 	
Remark:         Get an operation from NdbOperation idlelist and get the 
                NdbConnection object 
		who was fetch by startTransaction pointing to this  operation  
		getOperation will set the theTableId in the NdbOperation object.
                synchronous
******************************************************************************/
NdbOperation*
NdbConnection::getNdbOperation(const char* aTableName)
{
  if (theCommitStatus == Started){
    NdbTableImpl* table = theNdb->theDictionary->getTable(aTableName);
    if (table != 0){
      return getNdbOperation(table);
    } else {
      setErrorCode(theNdb->theDictionary->getNdbError().code);
      return NULL;
    }//if
  }

  setOperationErrorCodeAbort(4114);
  
  return NULL;
}//NdbConnection::getNdbOperation()

/*****************************************************************************
NdbOperation* getNdbOperation(const char* anIndexName, const char* aTableName);

Return Value    Return a pointer to a NdbOperation object if getNdbOperation 
                was succesful.
                Return NULL : In all other case. 	
Parameters:     anIndexName : Name of the index to use. 	
		aTableName  : Name of the database table. 	
Remark:         Get an operation from NdbOperation idlelist and get the 
                NdbConnection object 
		who was fetch by startTransaction pointing to this  operation  
		getOperation will set the theTableId in the NdbOperation object.
                synchronous
******************************************************************************/
NdbOperation*
NdbConnection::getNdbOperation(const char* anIndexName, const char* aTableName)
{
  if ((theError.code == 0) &&
      (theCommitStatus == Started)){
    NdbIndexImpl* index = 
      theNdb->theDictionary->getIndex(anIndexName, aTableName);
    NdbTableImpl* table = theNdb->theDictionary->getTable(aTableName);
    NdbTableImpl* indexTable = 
      theNdb->theDictionary->getIndexTable(index, table);
    if (indexTable != 0){
      return getNdbOperation(indexTable);
    } else {
      setErrorCode(theNdb->theDictionary->getNdbError().code);
      return NULL;
    }//if
  } else {
    if (theError.code == 0) {
      setOperationErrorCodeAbort(4114);
    }//if

    return NULL;
  }//if    
}//NdbConnection::getNdbOperation()

/*****************************************************************************
NdbOperation* getNdbOperation(int aTableId);

Return Value    Return a pointer to a NdbOperation object if getNdbOperation 
                was succesful.
                Return NULL: In all other case. 	
Parameters:     tableId : Id of the database table beeing deleted.
Remark:         Get an operation from NdbOperation object idlelist and 
                get the NdbConnection object who was fetch by 
                startTransaction pointing to this operation 
  	        getOperation will set the theTableId in the NdbOperation 
                object, synchronous.
*****************************************************************************/
NdbOperation*
NdbConnection::getNdbOperation(NdbTableImpl * tab, NdbOperation* aNextOp)
{ 
  NdbOperation* tOp;

  if (theScanningOp != NULL){
    setErrorCode(4607);
    return NULL;
  }
  
  tOp = theNdb->getOperation();
  if (tOp == NULL)
    goto getNdbOp_error1;
  if (aNextOp == NULL) {
    if (theLastOpInList != NULL) {
       theLastOpInList->next(tOp);
       theLastOpInList = tOp;
    } else {
       theLastOpInList = tOp;
       theFirstOpInList = tOp;
    }//if
    tOp->next(NULL);
  } else {
    // add before the given op
    if (theFirstOpInList == aNextOp) {
      theFirstOpInList = tOp;
    } else {
      NdbOperation* aLoopOp = theFirstOpInList;
      while (aLoopOp != NULL && aLoopOp->next() != aNextOp)
        aLoopOp = aLoopOp->next();
      assert(aLoopOp != NULL);
      aLoopOp->next(tOp);
    }
    tOp->next(aNextOp);
  }
  if (tOp->init(tab, this) != -1) {
    return tOp;
  } else {
    theNdb->releaseOperation(tOp);
  }//if
  return NULL;
  
 getNdbOp_error1:
  setOperationErrorCodeAbort(4000);
  return NULL;
}//NdbConnection::getNdbOperation()

// NdbScanOperation
/*****************************************************************************
NdbScanOperation* getNdbScanOperation(const char* aTableName);

Return Value    Return a pointer to a NdbScanOperation object if getNdbScanOperation was succesful.
                Return NULL : In all other case. 	
Parameters:     aTableName : Name of the database table. 	
Remark:         Get an operation from NdbScanOperation idlelist and get the NdbConnection object 
		who was fetch by startTransaction pointing to this  operation  
		getOperation will set the theTableId in the NdbOperation object.synchronous
******************************************************************************/
NdbScanOperation*
NdbConnection::getNdbScanOperation(const char* aTableName)
{
  if (theCommitStatus == Started){
    NdbTableImpl* tab = theNdb->theDictionary->getTable(aTableName);
    if (tab != 0){
      return getNdbScanOperation(tab);
    } else {
      setOperationErrorCodeAbort(theNdb->theError.code);
      return NULL;
    }//if
  } 
  
  setOperationErrorCodeAbort(4114);
  return NULL;
}//NdbConnection::getNdbScanOperation()

/*****************************************************************************
NdbScanOperation* getNdbScanOperation(const char* anIndexName, const char* aTableName);

Return Value    Return a pointer to a NdbScanOperation object if getNdbScanOperation was succesful.
                Return NULL : In all other case. 	
Parameters:     anIndexName : Name of the index to use. 	
                aTableName : Name of the database table. 	
Remark:         Get an operation from NdbScanOperation idlelist and get the NdbConnection object 
		who was fetch by startTransaction pointing to this  operation  
		getOperation will set the theTableId in the NdbOperation object.synchronous
******************************************************************************/
NdbScanOperation*
NdbConnection::getNdbScanOperation(const char* anIndexName, const char* aTableName)
{
  if (theCommitStatus == Started){
    NdbIndexImpl* index = 
      theNdb->theDictionary->getIndex(anIndexName, aTableName);
    NdbTableImpl* table = theNdb->theDictionary->getTable(aTableName);
    NdbTableImpl* indexTable = 
      theNdb->theDictionary->getIndexTable(index, table);
    if (indexTable != 0){
      return getNdbScanOperation(indexTable);
    } else {
      setOperationErrorCodeAbort(theNdb->theError.code);
      return NULL;
    }//if
  } 
  
  setOperationErrorCodeAbort(4114);
  return NULL;
}//NdbConnection::getNdbScanOperation()

/*****************************************************************************
NdbScanOperation* getNdbScanOperation(int aTableId);

Return Value    Return a pointer to a NdbOperation object if getNdbOperation was succesful.
                Return NULL: In all other case. 	
Parameters:     tableId : Id of the database table beeing deleted.
Remark:         Get an operation from NdbScanOperation object idlelist and get the NdbConnection 
                object who was fetch by startTransaction pointing to this  operation 
  	        getOperation will set the theTableId in the NdbOperation object, synchronous.
*****************************************************************************/
NdbScanOperation*
NdbConnection::getNdbScanOperation(NdbTableImpl * tab)
{ 
  NdbScanOperation* tOp;
  
  tOp = theNdb->getScanOperation();
  if (tOp == NULL)
    goto getNdbOp_error1;
  
  // Link scan operation into list of cursor operations
  if (m_theLastCursorOperation == NULL)
    m_theFirstCursorOperation = m_theLastCursorOperation = tOp;
  else {
    m_theLastCursorOperation->next(tOp);
    m_theLastCursorOperation = tOp;
  }
  tOp->next(NULL);
  if (tOp->init(tab, this) != -1) {
    return tOp;
  } else {
    theNdb->releaseScanOperation(tOp);
  }//if
  return NULL;

getNdbOp_error1:
  setOperationErrorCodeAbort(4000);
  return NULL;
}//NdbConnection::getNdbScanOperation()



// IndexOperation
/*****************************************************************************
NdbIndexOperation* getNdbIndexOperation(const char* anIndexName,
					const char* aTableName);

Return Value    Return a pointer to a NdbOperation object if getNdbScanOperation was succesful.
                Return NULL : In all other case. 	
Parameters:     aTableName : Name of the database table. 	
Remark:         Get an operation from NdbScanOperation idlelist and get the NdbConnection object 
		who was fetch by startTransaction pointing to this  operation  
		getOperation will set the theTableId in the NdbScanOperation object.synchronous
******************************************************************************/
NdbIndexOperation*
NdbConnection::getNdbIndexOperation(const char* anIndexName, 
                                    const char* aTableName)
{
  if (theCommitStatus == Started) {
    NdbTableImpl * table = theNdb->theDictionary->getTable(aTableName);
    NdbIndexImpl * index = theNdb->theDictionary->getIndex(anIndexName,
							   aTableName);
    if(table != 0 && index != 0){
      return getNdbIndexOperation(index, table);
    }
    
    if(index == 0){
      setOperationErrorCodeAbort(4243);
      return NULL;
    }

    // table == 0
    setOperationErrorCodeAbort(theNdb->theError.code);
    return NULL;
  } 
  
  setOperationErrorCodeAbort(4114);
  return 0;
}//NdbConnection::getNdbIndexOperation()

/*****************************************************************************
NdbIndexOperation* getNdbIndexOperation(int anIndexId, int aTableId);

Return Value    Return a pointer to a NdbIndexOperation object if getNdbIndexOperation was succesful.
                Return NULL: In all other case. 	
Parameters:     tableId : Id of the database table beeing deleted.
Remark:         Get an operation from NdbIndexOperation object idlelist and get the NdbConnection 
                object who was fetch by startTransaction pointing to this  operation 
  	        getOperation will set the theTableId in the NdbIndexOperation object, synchronous.
*****************************************************************************/
NdbIndexOperation*
NdbConnection::getNdbIndexOperation(NdbIndexImpl * anIndex, 
				    NdbTableImpl * aTable,
                                    NdbOperation* aNextOp)
{ 
  NdbIndexOperation* tOp;
  
  tOp = theNdb->getIndexOperation();
  if (tOp == NULL)
    goto getNdbOp_error1;
  if (aNextOp == NULL) {
    if (theLastOpInList != NULL) {
       theLastOpInList->next(tOp);
       theLastOpInList = tOp;
    } else {
       theLastOpInList = tOp;
       theFirstOpInList = tOp;
    }//if
    tOp->next(NULL);
  } else {
    // add before the given op
    if (theFirstOpInList == aNextOp) {
      theFirstOpInList = tOp;
    } else {
      NdbOperation* aLoopOp = theFirstOpInList;
      while (aLoopOp != NULL && aLoopOp->next() != aNextOp)
        aLoopOp = aLoopOp->next();
      assert(aLoopOp != NULL);
      aLoopOp->next(tOp);
    }
    tOp->next(aNextOp);
  }
  if (tOp->indxInit(anIndex, aTable, this)!= -1) {
    return tOp;
  } else {
    theNdb->releaseOperation(tOp);
  }//if
  return NULL;
  
 getNdbOp_error1:
  setOperationErrorCodeAbort(4000);
  return NULL;
}//NdbConnection::getNdbIndexOperation()

/*******************************************************************************
int  receiveDIHNDBTAMPER(NdbApiSignal* aSignal)

Return Value:  Return 0 : receiveDIHNDBTAMPER was successful.
               Return -1: In all other case.
Parameters:    aSignal: The signal object pointer.
Remark:        Sets theRestartGCI in the NDB object. 
*******************************************************************************/
int			
NdbConnection::receiveDIHNDBTAMPER(NdbApiSignal* aSignal)
{
  if (theStatus != Connecting) {
    return -1;
  } else {
    theNdb->RestartGCI((Uint32)aSignal->readData(2));
    theStatus = Connected;
  }//if
  return 0;  
}//NdbConnection::receiveDIHNDBTAMPER()

/*******************************************************************************
int  receiveTCSEIZECONF(NdbApiSignal* aSignal);

Return Value:  Return 0 : receiveTCSEIZECONF was successful.
               Return -1: In all other case.
Parameters:    aSignal: The signal object pointer.
Remark:        Sets TC Connect pointer at reception of TCSEIZECONF. 
*******************************************************************************/
int			
NdbConnection::receiveTCSEIZECONF(NdbApiSignal* aSignal)
{
  if (theStatus != Connecting)
  {
    return -1;
  } else
  {
    theTCConPtr = (Uint32)aSignal->readData(2);
    theStatus = Connected;
  }
  return 0;
}//NdbConnection::receiveTCSEIZECONF()

/*******************************************************************************
int  receiveTCSEIZEREF(NdbApiSignal* aSignal);

Return Value:  Return 0 : receiveTCSEIZEREF was successful.
               Return -1: In all other case.
Parameters:    aSignal: The signal object pointer.
Remark:        Sets TC Connect pointer. 
*******************************************************************************/
int			
NdbConnection::receiveTCSEIZEREF(NdbApiSignal* aSignal)
{
  if (theStatus != Connecting)
  {
    return -1;
  } else
  {
    theStatus = ConnectFailure;
    theNdb->theError.code = aSignal->readData(2);
    return 0;
  }
}//NdbConnection::receiveTCSEIZEREF()

/*******************************************************************************
int  receiveTCRELEASECONF(NdbApiSignal* aSignal);

Return Value:  Return 0 : receiveTCRELEASECONF was successful.
               Return -1: In all other case.
Parameters:    aSignal: The signal object pointer.
Remark:         DisConnect TC Connect pointer to NDBAPI. 
*******************************************************************************/
int			
NdbConnection::receiveTCRELEASECONF(NdbApiSignal* aSignal)
{
  if (theStatus != DisConnecting)
  {
    return -1;
  } else
  {
    theStatus = NotConnected;
  }
  return 0;
}//NdbConnection::receiveTCRELEASECONF()

/*******************************************************************************
int  receiveTCRELEASEREF(NdbApiSignal* aSignal);

Return Value:  Return 0 : receiveTCRELEASEREF was successful.
               Return -1: In all other case.
Parameters:    aSignal: The signal object pointer.
Remark:        DisConnect TC Connect pointer to NDBAPI Failure. 
*******************************************************************************/
int			
NdbConnection::receiveTCRELEASEREF(NdbApiSignal* aSignal)
{
  if (theStatus != DisConnecting) {
    return -1;
  } else {
    theStatus = ConnectFailure;
    theNdb->theError.code = aSignal->readData(2);
    return 0;
  }//if
}//NdbConnection::receiveTCRELEASEREF()

/******************************************************************************
int  receiveTC_COMMITCONF(NdbApiSignal* aSignal);

Return Value:  Return 0 : receiveTC_COMMITCONF was successful.
               Return -1: In all other case.
Parameters:    aSignal: The signal object pointer.
Remark:        
******************************************************************************/
int			
NdbConnection::receiveTC_COMMITCONF(const TcCommitConf * commitConf)
{ 
  if(theStatus != Connected){
    return -1;
  }
  theCommitStatus = Committed;
  theCompletionStatus = CompletedSuccess;
  return 0;
}//NdbConnection::receiveTC_COMMITCONF()

/******************************************************************************
int  receiveTC_COMMITREF(NdbApiSignal* aSignal);

Return Value:  Return 0 : receiveTC_COMMITREF was successful.
               Return -1: In all other case.
Parameters:    aSignal: The signal object pointer.
Remark:        
******************************************************************************/
int			
NdbConnection::receiveTC_COMMITREF(NdbApiSignal* aSignal)
{
  if(theStatus != Connected){
    return -1;
  }
  const TcCommitRef * const ref = CAST_CONSTPTR(TcCommitRef, aSignal->getDataPtr());
  setOperationErrorCodeAbort(ref->errorCode);
  theCommitStatus = Aborted;
  theCompletionStatus = CompletedFailure;
  return 0;
}//NdbConnection::receiveTC_COMMITREF()

/*******************************************************************************
int  receiveTCROLLBACKCONF(NdbApiSignal* aSignal);

Return Value:  Return 0 : receiveTCROLLBACKCONF was successful.
               Return -1: In all other case.
Parameters:    aSignal: The signal object pointer.
Remark:        
*******************************************************************************/
int			
NdbConnection::receiveTCROLLBACKCONF(NdbApiSignal* aSignal)
{
  if(theStatus != Connected){
    return -1;
  }
  theCommitStatus = Aborted;
  theCompletionStatus = CompletedSuccess;
  return 0;
}//NdbConnection::receiveTCROLLBACKCONF()

/*******************************************************************************
int  receiveTCROLLBACKREF(NdbApiSignal* aSignal);

Return Value:  Return 0 : receiveTCROLLBACKREF was successful.
               Return -1: In all other case.
Parameters:    aSignal: The signal object pointer.
Remark:        
*******************************************************************************/
int			
NdbConnection::receiveTCROLLBACKREF(NdbApiSignal* aSignal)
{
  if(theStatus != Connected){
    return -1;
  }
  setOperationErrorCodeAbort(aSignal->readData(2));
  theCommitStatus = Aborted;
  theCompletionStatus = CompletedFailure;
  return 0;
}//NdbConnection::receiveTCROLLBACKREF()

/*****************************************************************************
int receiveTCROLLBACKREP( NdbApiSignal* aSignal)

Return Value:   Return 0 : send was succesful.
                Return -1: In all other case.   
Parameters:     aSignal: the signal object that contains the 
                TCROLLBACKREP signal from TC.
Remark:         Handles the reception of the ROLLBACKREP signal.
*****************************************************************************/
int
NdbConnection::receiveTCROLLBACKREP( NdbApiSignal* aSignal)
{
  Uint64 tRecTransId, tCurrTransId;
  Uint32 tTmp1, tTmp2;

  if (theStatus != Connected) {
    return -1;
  }//if
/*****************************************************************************
Check that we are expecting signals from this transaction and that it doesn't
belong to a transaction already completed. Simply ignore messages from other 
transactions.
******************************************************************************/
  tTmp1 = aSignal->readData(2);
  tTmp2 = aSignal->readData(3);
  tRecTransId = (Uint64)tTmp1 + ((Uint64)tTmp2 << 32);
  tCurrTransId = this->getTransactionId();
  if (tCurrTransId != tRecTransId) {
    return -1;
  }//if
  theError.code = aSignal->readData(4);	// Override any previous errors

/**********************************************************************/
/*	A serious error has occured. This could be due to deadlock or */
/*	lack of resources or simply a programming error in NDB. This  */
/*	transaction will be aborted. Actually it has already been     */
/*	and we only need to report completion and return with the     */
/*	error code to the application.				      */
/**********************************************************************/
  theCompletionStatus = CompletedFailure;
  theCommitStatus = Aborted;
  return 0;
}//NdbConnection::receiveTCROLLBACKREP()

/*******************************************************************************
int  receiveTCKEYCONF(NdbApiSignal* aSignal, Uint32 long_short_ind);

Return Value:  Return 0 : receiveTCKEYCONF was successful.
               Return -1: In all other case.
Parameters:    aSignal: The signal object pointer.
Remark:        
*******************************************************************************/
int			
NdbConnection::receiveTCKEYCONF(const TcKeyConf * keyConf, Uint32 aDataLength)
{
  Uint64 tRecTransId;
  NdbOperation* tOp;
  Uint32 tConditionFlag;

  const Uint32 tTemp = keyConf->confInfo;
  const Uint32 tTmp1 = keyConf->transId1;
  const Uint32 tTmp2 = keyConf->transId2;
/******************************************************************************
Check that we are expecting signals from this transaction and that it
doesn't belong to a transaction already completed. Simply ignore messages
from other transactions.
******************************************************************************/
  tRecTransId = (Uint64)tTmp1 + ((Uint64)tTmp2 << 32);

  const Uint32 tNoOfOperations = TcKeyConf::getNoOfOperations(tTemp);
  const Uint32 tCommitFlag = TcKeyConf::getCommitFlag(tTemp);
  tConditionFlag = (Uint32)(((aDataLength - 5) >> 1) - tNoOfOperations);
  tConditionFlag |= (Uint32)(tNoOfOperations > 10);
  tConditionFlag |= (Uint32)(tNoOfOperations <= 0);
  tConditionFlag |= (Uint32)(theTransactionId - tRecTransId);
  tConditionFlag |= (Uint32)(theStatus - Connected);

  if (tConditionFlag == 0) {
    const Uint32* tPtr = (Uint32 *)&keyConf->operations[0];
    for (Uint32 i = 0; i < tNoOfOperations ; i++) {
      tOp = theNdb->void2rec_op(theNdb->int2void(*tPtr));
      tPtr++;
      const Uint32 tAttrInfoLen = *tPtr;
      tPtr++;
      if (tOp && tOp->checkMagicNumber() != -1) {
	tOp->TCOPCONF(tAttrInfoLen);
      } else {
 	return -1;
      }//if
    }//for
    Uint32 tNoComp = theNoOfOpCompleted;
    Uint32 tNoSent = theNoOfOpSent;
    Uint32 tGCI    = keyConf->gci;
    if (tCommitFlag == 1) {
      theCommitStatus = Committed;
      theGlobalCheckpointId = tGCI;
    } else if ((tNoComp >= tNoSent) &&
               (theLastExecOpInList->theCommitIndicator == 1)){
/**********************************************************************/
// We sent the transaction with Commit flag set and received a CONF with
// no Commit flag set. This is clearly an anomaly.
/**********************************************************************/
      theError.code = 4011;
      theCompletionStatus = CompletedFailure;
      theCommitStatus = Aborted;
      return 0;
    }//if
    if (tNoComp >= tNoSent) {
      return 0;	// No more operations to wait for
    }//if
     // Not completed the reception yet.
  }//if
  return -1;
}//NdbConnection::receiveTCKEYCONF()

/*****************************************************************************
int receiveTCKEY_FAILCONF( NdbApiSignal* aSignal)

Return Value:   Return 0 : receive was completed.
                Return -1: In all other case.   
Parameters:     aSignal: the signal object that contains the 
                TCKEY_FAILCONF signal from TC.
Remark:         Handles the reception of the TCKEY_FAILCONF signal.
*****************************************************************************/
int
NdbConnection::receiveTCKEY_FAILCONF(const TcKeyFailConf * failConf)
{
  Uint64 tRecTransId, tCurrTransId;
  Uint32 tTmp1, tTmp2;
  NdbOperation*	tOp;
  if (theStatus != Connected) {
    return -1;
  }//if
  /*
  Check that we are expecting signals from this transaction and that it 
  doesn't belong  to a transaction already completed. Simply ignore 
  messages from other transactions.
  */
  tTmp1 = failConf->transId1;
  tTmp2 = failConf->transId2;
  tRecTransId = (Uint64)tTmp1 + ((Uint64)tTmp2 << 32);
  tCurrTransId = this->getTransactionId();
  if (tCurrTransId != tRecTransId) {
    return -1;
  }//if
  /*
  A node failure of the TC node occured. The transaction has
  been committed.
  */
  theCommitStatus = Committed;
  tOp = theFirstExecOpInList;
  while (tOp != NULL) {
    /*
    Check if the transaction expected read values...
    If it did some of them might have gotten lost even if we succeeded
    in committing the transaction.
    */
    if (tOp->theAI_ElementLen != 0) {
      theCompletionStatus = CompletedFailure;
      setOperationErrorCodeAbort(4115);
      break;
    }//if
    if (tOp->theCurrentRecAttr != NULL) {
      theCompletionStatus = CompletedFailure;
      setOperationErrorCodeAbort(4115);
      break;
    }//if
    tOp = tOp->next();
  }//while   
  theReleaseOnClose = true;
  return 0;
}//NdbConnection::receiveTCKEY_FAILCONF()

/*************************************************************************
int receiveTCKEY_FAILREF( NdbApiSignal* aSignal)

Return Value:   Return 0 : receive was completed.
                Return -1: In all other case.   
Parameters:     aSignal: the signal object that contains the 
                TCKEY_FAILREF signal from TC.
Remark:         Handles the reception of the TCKEY_FAILREF signal.
**************************************************************************/
int
NdbConnection::receiveTCKEY_FAILREF(NdbApiSignal* aSignal)
{
  Uint64 tRecTransId, tCurrTransId;
  Uint32 tTmp1, tTmp2;

  if (theStatus != Connected) {
    return -1;
  }//if
  /*
  Check that we are expecting signals from this transaction and
  that it doesn't belong to a transaction already
  completed. Simply ignore messages from other transactions.
  */
  tTmp1 = aSignal->readData(2);
  tTmp2 = aSignal->readData(3);
  tRecTransId = (Uint64)tTmp1 + ((Uint64)tTmp2 << 32);
  tCurrTransId = this->getTransactionId();
  if (tCurrTransId != tRecTransId) {
    return -1;
  }//if
  /*
  We received an indication of that this transaction was aborted due to a
  node failure.
  */
  if (theSendStatus == sendTC_ROLLBACK) {
    /*
    We were in the process of sending a rollback anyways. We will
    report it as a success.
    */
    theCompletionStatus = CompletedSuccess;
  } else {
    theCompletionStatus = CompletedFailure;
    theError.code = 4031;
  }//if
  theReleaseOnClose = true;
  theCommitStatus = Aborted;
  return 0;
}//NdbConnection::receiveTCKEY_FAILREF()

/*******************************************************************************
int  receiveTCINDXCONF(NdbApiSignal* aSignal, Uint32 long_short_ind);

Return Value:  Return 0 : receiveTCINDXCONF was successful.
               Return -1: In all other case.
Parameters:    aSignal: The signal object pointer.
Remark:        
*******************************************************************************/
int			
NdbConnection::receiveTCINDXCONF(const TcIndxConf * indxConf, Uint32 aDataLength)
{
  Uint64 tRecTransId;
  Uint32 tConditionFlag;

  const Uint32 tTemp = indxConf->confInfo;
  const Uint32 tTmp1 = indxConf->transId1;
  const Uint32 tTmp2 = indxConf->transId2;
/******************************************************************************
Check that we are expecting signals from this transaction and that it
doesn't belong to a transaction already completed. Simply ignore messages
from other transactions.
******************************************************************************/
  tRecTransId = (Uint64)tTmp1 + ((Uint64)tTmp2 << 32);

  const Uint32 tNoOfOperations = TcIndxConf::getNoOfOperations(tTemp);
  const Uint32 tCommitFlag = TcKeyConf::getCommitFlag(tTemp);

  tConditionFlag = (Uint32)(((aDataLength - 5) >> 1) - tNoOfOperations);
  tConditionFlag |= (Uint32)(tNoOfOperations > 10);
  tConditionFlag |= (Uint32)(tNoOfOperations <= 0);
  tConditionFlag |= (Uint32)(theTransactionId - tRecTransId);
  tConditionFlag |= (Uint32)(theStatus - Connected);

  if (tConditionFlag == 0) {
    const Uint32* tPtr = (Uint32 *)&indxConf->operations[0];
    for (Uint32 i = 0; i < tNoOfOperations ; i++) {
      NdbIndexOperation* tOp = theNdb->void2rec_iop(theNdb->int2void(*tPtr));
      tPtr++;
      const Uint32 tAttrInfoLen = *tPtr;
      tPtr++;
      if (tOp && tOp->checkMagicNumber() != -1) {
	tOp->TCOPCONF(tAttrInfoLen);
      } else {
	return -1;
      }//if
    }//for
    Uint32 tNoComp = theNoOfOpCompleted;
    Uint32 tNoSent = theNoOfOpSent;
    Uint32 tGCI    = indxConf->gci;
    if (tCommitFlag == 1) {
      theCommitStatus = Committed;
      theGlobalCheckpointId = tGCI;
    } else if ((tNoComp >= tNoSent) &&
               (theLastExecOpInList->theCommitIndicator == 1)){
/**********************************************************************/
// We sent the transaction with Commit flag set and received a CONF with
// no Commit flag set. This is clearly an anomaly.
/**********************************************************************/
      theError.code = 4011;
      theCompletionStatus = CompletedFailure;
      theCommitStatus = Aborted;
      return 0;
    }//if
    if (tNoComp >= tNoSent) {
      return 0;	// No more operations to wait for
    }//if
     // Not completed the reception yet.
  }//if
  return -1;
}//NdbConnection::receiveTCINDXCONF()

/*****************************************************************************
int receiveTCINDXREF( NdbApiSignal* aSignal)

Return Value:   Return 0 : send was succesful.
                Return -1: In all other case.   
Parameters:     aSignal: the signal object that contains the 
                TCINDXREF signal from TC.
Remark:         Handles the reception of the TCINDXREF signal.
*****************************************************************************/
int
NdbConnection::receiveTCINDXREF( NdbApiSignal* aSignal)
{
  Uint64 tRecTransId, tCurrTransId;
  Uint32 tTmp1, tTmp2;

  if (theStatus != Connected) {
    return -1;
  }//if
/*****************************************************************************
Check that we are expecting signals from this transaction and that it doesn't
belong to a transaction already completed. Simply ignore messages from other 
transactions.
******************************************************************************/
  tTmp1 = aSignal->readData(2);
  tTmp2 = aSignal->readData(3);
  tRecTransId = (Uint64)tTmp1 + ((Uint64)tTmp2 << 32);
  tCurrTransId = this->getTransactionId();
  if (tCurrTransId != tRecTransId) {
    return -1;
  }//if
  theError.code = aSignal->readData(4);	// Override any previous errors

/**********************************************************************/
/*	A serious error has occured. This could be due to deadlock or */
/*	lack of resources or simply a programming error in NDB. This  */
/*	transaction will be aborted. Actually it has already been     */
/*	and we only need to report completion and return with the     */
/*	error code to the application.				      */
/**********************************************************************/
  theCompletionStatus = CompletedFailure;
  theCommitStatus = Aborted;
  return 0;
}//NdbConnection::receiveTCINDXREF()

/*******************************************************************************
int OpCompletedFailure();

Return Value:  Return 0 : OpCompleteSuccess was successful.
               Return -1: In all other case.
Parameters:    aErrorCode: The error code.
Remark:        An operation was completed with failure.
*******************************************************************************/
int 
NdbConnection::OpCompleteFailure()
{
  Uint32 tNoComp = theNoOfOpCompleted;
  Uint32 tNoSent = theNoOfOpSent;
  theCompletionStatus = CompletedFailure;
  tNoComp++;
  theNoOfOpCompleted = tNoComp;
  if (tNoComp == tNoSent) {
    //------------------------------------------------------------------------
    //If the transaction consists of only simple reads we can set
    //Commit state Aborted.  Otherwise this simple operation cannot
    //decide the success of the whole transaction since a simple
    //operation is not really part of that transaction.
    //------------------------------------------------------------------------
    if (theSimpleState == 1) {
      theCommitStatus = Aborted;
    }//if
    return 0;	// Last operation received
  } else if (tNoComp > tNoSent) {
    setOperationErrorCodeAbort(4113);	// Too many operations, 
                                        // stop waiting for more
    return 0;
  } else {
    return -1;	// Continue waiting for more signals
  }//if
}//NdbConnection::OpCompleteFailure()

/******************************************************************************
int OpCompleteSuccess();

Return Value:  Return 0 : OpCompleteSuccess was successful.
               Return -1: In all other case.  
Remark:        An operation was completed with success.
*******************************************************************************/
int 
NdbConnection::OpCompleteSuccess()
{
  Uint32 tNoComp = theNoOfOpCompleted;
  Uint32 tNoSent = theNoOfOpSent;
  tNoComp++;
  theNoOfOpCompleted = tNoComp;
  if (tNoComp == tNoSent) { // Last operation completed
    if (theSimpleState == 1) {
      theCommitStatus = Committed;
    }//if
    return 0;
  } else if (tNoComp < tNoSent) {
    return -1;	// Continue waiting for more signals
  } else {
    setOperationErrorCodeAbort(4113);	// Too many operations, 
                                        // stop waiting for more
    theCompletionStatus = CompletedFailure;
    return 0;
  }//if
}//NdbConnection::OpCompleteSuccess()

/******************************************************************************
 int            getGCI();

Remark:		Get global checkpoint identity of the transaction
*******************************************************************************/
int
NdbConnection::getGCI()
{
  if (theCommitStatus == Committed) {
    return theGlobalCheckpointId;
  }//if
  return 0;
}//NdbConnection::getGCI()

/*******************************************************************************
Uint64 getTransactionId(void);

Remark:        Get the transaction identity. 
*******************************************************************************/
Uint64
NdbConnection::getTransactionId()
{
  return theTransactionId;
}//NdbConnection::getTransactionId()

NdbConnection::CommitStatusType
NdbConnection::commitStatus()
{
  return theCommitStatus;
}//NdbConnection::commitStatus()

int 
NdbConnection::getNdbErrorLine()
{
  return theErrorLine;
}

NdbOperation*
NdbConnection::getNdbErrorOperation()
{
  return theErrorOperation;
}//NdbConnection::getNdbErrorOperation()

const NdbOperation * 
NdbConnection::getNextCompletedOperation(const NdbOperation * current) const {
  if(current == 0)
    return theCompletedFirstOp;
  return current->theNext;
}

#ifdef VM_TRACE
#define CASE(x) case x: ndbout << " " << #x; break
void
NdbConnection::printState()
{
  ndbout << "con=" << hex << this << dec;
  ndbout << " node=" << getConnectedNodeId();
  switch (theStatus) {
  CASE(NotConnected);
  CASE(Connecting);
  CASE(Connected);
  CASE(DisConnecting);
  CASE(ConnectFailure);
  default: ndbout << theStatus;
  }
  switch (theListState) {
  CASE(NotInList);
  CASE(InPreparedList);
  CASE(InSendList);
  CASE(InCompletedList);
  default: ndbout << theListState;
  }
  switch (theSendStatus) {
  CASE(NotInit);
  CASE(InitState);
  CASE(sendOperations);
  CASE(sendCompleted);
  CASE(sendCOMMITstate);
  CASE(sendABORT);
  CASE(sendABORTfail);
  CASE(sendTC_ROLLBACK);
  CASE(sendTC_COMMIT);
  CASE(sendTC_OP);
  default: ndbout << theSendStatus;
  }
  switch (theCommitStatus) {
  CASE(NotStarted);
  CASE(Started);
  CASE(Committed);
  CASE(Aborted);
  CASE(NeedAbort);
  default: ndbout << theCommitStatus;
  }
  switch (theCompletionStatus) {
  CASE(NotCompleted);
  CASE(CompletedSuccess);
  CASE(CompletedFailure);
  CASE(DefinitionFailure);
  default: ndbout << theCompletionStatus;
  }
  ndbout << endl;
}
#undef CASE
#endif

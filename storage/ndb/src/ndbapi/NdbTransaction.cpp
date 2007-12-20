/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#include <ndb_global.h>
#include <NdbOut.hpp>
#include <NdbTransaction.hpp>
#include <NdbOperation.hpp>
#include <NdbScanOperation.hpp>
#include <NdbIndexScanOperation.hpp>
#include <NdbIndexOperation.hpp>
#include "NdbApiSignal.hpp"
#include "TransporterFacade.hpp"
#include "API.hpp"
#include "NdbBlob.hpp"

#include <signaldata/TcKeyConf.hpp>
#include <signaldata/TcIndx.hpp>
#include <signaldata/TcCommit.hpp>
#include <signaldata/TcKeyFailConf.hpp>
#include <signaldata/TcHbRep.hpp>
#include <signaldata/TcRollbackRep.hpp>

/*****************************************************************************
NdbTransaction( Ndb* aNdb );

Return Value:  None
Parameters:    aNdb: Pointers to the Ndb object 
Remark:        Creates a connection object. 
*****************************************************************************/
NdbTransaction::NdbTransaction( Ndb* aNdb ) :
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
  theCompletedLastOp(NULL),
  theNoOfOpSent(0),
  theNoOfOpCompleted(0),
  theMyRef(0),
  theTCConPtr(0),
  theTransactionId(0),
  theGlobalCheckpointId(0),
  p_latest_trans_gci(0),
  theStatus(NotConnected),
  theCompletionStatus(NotCompleted), 
  theCommitStatus(NotStarted),
  theMagicNumber(0xFE11DC),
  theTransactionIsStarted(false),
  theDBnode(0),
  theReleaseOnClose(false),
  // Scan operations
  m_waitForReply(true),
  m_theFirstScanOperation(NULL),
  m_theLastScanOperation(NULL),
  m_firstExecutedScanOp(NULL),
  // Scan operations
  theScanningOp(NULL),
  theBuddyConPtr(0xFFFFFFFF),
  theBlobFlag(false),
  thePendingBlobOps(0)
{
  theListState = NotInList;
  theError.code = 0;
  //theId = NdbObjectIdMap::InvalidId;
  theId = theNdb->theImpl->theNdbObjectIdMap.map(this);

#define CHECK_SZ(mask, sz) assert((sizeof(mask)/sizeof(mask[0])) == sz)

  CHECK_SZ(m_db_nodes, NdbNodeBitmask::Size);
  CHECK_SZ(m_failed_db_nodes, NdbNodeBitmask::Size);
}//NdbTransaction::NdbTransaction()

/*****************************************************************************
~NdbTransaction();

Remark:        Deletes the connection object. 
*****************************************************************************/
NdbTransaction::~NdbTransaction()
{
  DBUG_ENTER("NdbTransaction::~NdbTransaction");
  theNdb->theImpl->theNdbObjectIdMap.unmap(theId, this);
  DBUG_VOID_RETURN;
}//NdbTransaction::~NdbTransaction()

/*****************************************************************************
void init();

Remark:         Initialise connection object for new transaction. 
*****************************************************************************/
int
NdbTransaction::init()
{
  theListState            = NotInList;
  theInUseState           = true;
  theTransactionIsStarted = false;
  theNext		  = NULL;

  theFirstOpInList	  = NULL;
  theLastOpInList	  = NULL;

  theScanningOp            = NULL;

  theFirstExecOpInList	  = NULL;
  theLastExecOpInList	  = NULL;

  theCompletedFirstOp	  = NULL;
  theCompletedLastOp	  = NULL;

  theGlobalCheckpointId   = 0;
  p_latest_trans_gci      =
    theNdb->theImpl->m_ndb_cluster_connection.get_latest_trans_gci();
  theCommitStatus         = Started;
  theCompletionStatus     = NotCompleted;

  theError.code		  = 0;
  theErrorLine		  = 0;
  theErrorOperation	  = NULL;

  theReleaseOnClose       = false;
  theSimpleState          = true;
  theSendStatus           = InitState;
  theMagicNumber          = 0x37412619;
  // Scan operations
  m_waitForReply            = true;
  m_theFirstScanOperation = NULL;
  m_theLastScanOperation  = NULL;
  m_firstExecutedScanOp   = 0;
  theBuddyConPtr            = 0xFFFFFFFF;
  //
  theBlobFlag = false;
  thePendingBlobOps = 0;
  if (theId == NdbObjectIdMap::InvalidId)
  {
    theId = theNdb->theImpl->theNdbObjectIdMap.map(this);
    if (theId == NdbObjectIdMap::InvalidId)
    {
      theError.code = 4000;
      return -1;
    }
  }
  return 0;

}//NdbTransaction::init()

/*****************************************************************************
setOperationErrorCode(int error);

Remark:        Sets an error code on the connection object from an 
               operation object. 
*****************************************************************************/
void
NdbTransaction::setOperationErrorCode(int error)
{
  DBUG_ENTER("NdbTransaction::setOperationErrorCode");
  setErrorCode(error);
  DBUG_VOID_RETURN;
}

/*****************************************************************************
setOperationErrorCodeAbort(int error);

Remark:        Sets an error code on the connection object from an 
               operation object. 
*****************************************************************************/
void
NdbTransaction::setOperationErrorCodeAbort(int error, int abortOption)
{
  DBUG_ENTER("NdbTransaction::setOperationErrorCodeAbort");
  if (theTransactionIsStarted == false) {
    theCommitStatus = Aborted;
  } else if ((theCommitStatus != Committed) &&
             (theCommitStatus != Aborted)) {
    theCommitStatus = NeedAbort;
  }//if
  setErrorCode(error);
  DBUG_VOID_RETURN;
}

/*****************************************************************************
setErrorCode(int anErrorCode);

Remark:        Sets an error indication on the connection object. 
*****************************************************************************/
void
NdbTransaction::setErrorCode(int error)
{
  DBUG_ENTER("NdbTransaction::setErrorCode");
  DBUG_PRINT("enter", ("error: %d, theError.code: %d", error, theError.code));

  if (theError.code == 0)
    theError.code = error;

  DBUG_VOID_RETURN;
}//NdbTransaction::setErrorCode()

int
NdbTransaction::restart(){
  DBUG_ENTER("NdbTransaction::restart");
  if(theCompletionStatus == CompletedSuccess){
    releaseCompletedOperations();
    Uint64 tTransid = theNdb->theFirstTransId;
    theTransactionId = tTransid;
    if ((tTransid & 0xFFFFFFFF) == 0xFFFFFFFF) {
      theNdb->theFirstTransId = (tTransid >> 32) << 32;
    } else {
      theNdb->theFirstTransId = tTransid + 1;
    }
    theCommitStatus = Started;
    theCompletionStatus = NotCompleted;
    theTransactionIsStarted = false;
    DBUG_RETURN(0);
  }
  DBUG_PRINT("error",("theCompletionStatus != CompletedSuccess"));
  DBUG_RETURN(-1);
}

/*****************************************************************************
void handleExecuteCompletion(void);

Remark:        Handle time-out on a transaction object. 
*****************************************************************************/
void
NdbTransaction::handleExecuteCompletion()
{
  /***************************************************************************
   *	  Move the NdbOperation objects from the list of executing 
   *      operations to list of completed
   **************************************************************************/
  NdbOperation* tFirstExecOp = theFirstExecOpInList;
  NdbOperation* tLastExecOp = theLastExecOpInList;
  if (tLastExecOp != NULL) {
    tLastExecOp->next(theCompletedFirstOp);
    theCompletedFirstOp = tFirstExecOp;
    if (theCompletedLastOp == NULL)
      theCompletedLastOp = tLastExecOp;
    theFirstExecOpInList = NULL;
    theLastExecOpInList = NULL;
  }//if
  theSendStatus = InitState;
  return;
}//NdbTransaction::handleExecuteCompletion()

/*****************************************************************************
int execute(ExecType aTypeOfExec, CommitType aTypeOfCommit, int forceSend);

Return Value:  Return 0 : execute was successful.
               Return -1: In all other case.  
Parameters :   aTypeOfExec: Type of execute.
Remark:        Initialise connection object for new transaction. 
*****************************************************************************/
int 
NdbTransaction::execute(ExecType aTypeOfExec, 
			NdbOperation::AbortOption abortOption,
			int forceSend)
{
  NdbError savedError= theError;
  DBUG_ENTER("NdbTransaction::execute");
  DBUG_PRINT("enter", ("aTypeOfExec: %d, abortOption: %d", 
		       aTypeOfExec, abortOption));

  if (! theBlobFlag)
    DBUG_RETURN(executeNoBlobs(aTypeOfExec, abortOption, forceSend));

  /*
   * execute prepared ops in batches, as requested by blobs
   * - blob error does not terminate execution
   * - blob error sets error on operation
   * - if error on operation skip blob calls
   */

  ExecType tExecType;
  NdbOperation* tPrepOp;
  NdbOperation* tCompletedFirstOp = NULL;
  NdbOperation* tCompletedLastOp = NULL;

  int ret = 0;
  do {
    tExecType = aTypeOfExec;
    tPrepOp = theFirstOpInList;
    while (tPrepOp != NULL) {
      if (tPrepOp->theError.code == 0) {
        bool batch = false;
        NdbBlob* tBlob = tPrepOp->theBlobList;
        while (tBlob != NULL) {
          if (tBlob->preExecute(tExecType, batch) == -1)
	  {
            ret = -1;
	    if(savedError.code==0)
	      savedError= theError;
	  }
          tBlob = tBlob->theNext;
        }
        if (batch) {
          // blob asked to execute all up to here now
          tExecType = NoCommit;
          break;
        }
      }
      tPrepOp = tPrepOp->next();
    }

    // save rest of prepared ops if batch
    NdbOperation* tRestOp= 0;
    NdbOperation* tLastOp= 0;
    if (tPrepOp != NULL) {
      tRestOp = tPrepOp->next();
      tPrepOp->next(NULL);
      tLastOp = theLastOpInList;
      theLastOpInList = tPrepOp;
    }

    if (tExecType == Commit) {
      NdbOperation* tOp = theCompletedFirstOp;
      while (tOp != NULL) {
        if (tOp->theError.code == 0) {
          NdbBlob* tBlob = tOp->theBlobList;
          while (tBlob != NULL) {
            if (tBlob->preCommit() == -1)
	    {
	      ret = -1;
	      if(savedError.code==0)
		savedError= theError;
	    }
            tBlob = tBlob->theNext;
          }
        }
        tOp = tOp->next();
      }
    }

    // completed ops are in unspecified order
    if (theCompletedFirstOp != NULL) {
      if (tCompletedFirstOp == NULL) {
        tCompletedFirstOp = theCompletedFirstOp;
        tCompletedLastOp = theCompletedLastOp;
      } else {
        tCompletedLastOp->next(theCompletedFirstOp);
        tCompletedLastOp = theCompletedLastOp;
      }
      theCompletedFirstOp = NULL;
      theCompletedLastOp = NULL;
    }

    if (executeNoBlobs(tExecType, 
		       NdbOperation::DefaultAbortOption,
		       forceSend) == -1)
    {
      if(savedError.code==0)
	savedError= theError;
      
      DBUG_RETURN(-1);
    }
    
#ifdef ndb_api_crash_on_complex_blob_abort
    assert(theFirstOpInList == NULL && theLastOpInList == NULL);
#else
    theFirstOpInList = theLastOpInList = NULL;
#endif

    {
      NdbOperation* tOp = theCompletedFirstOp;
      while (tOp != NULL) {
        if (tOp->theError.code == 0) {
          NdbBlob* tBlob = tOp->theBlobList;
          while (tBlob != NULL) {
            // may add new operations if batch
            if (tBlob->postExecute(tExecType) == -1)
	    {
              ret = -1;
	      if(savedError.code==0)
		savedError= theError;
	    }
            tBlob = tBlob->theNext;
          }
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
    assert(theFirstOpInList == NULL || tExecType == NoCommit);
  } while (theFirstOpInList != NULL || tExecType != aTypeOfExec);

  if (tCompletedFirstOp != NULL) {
    tCompletedLastOp->next(theCompletedFirstOp);
    theCompletedFirstOp = tCompletedFirstOp;
    if (theCompletedLastOp == NULL)
      theCompletedLastOp = tCompletedLastOp;
  }
#if ndb_api_count_completed_ops_after_blob_execute
  { NdbOperation* tOp; unsigned n = 0;
    for (tOp = theCompletedFirstOp; tOp != NULL; tOp = tOp->next()) n++;
    ndbout << "completed ops: " << n << endl;
  }
#endif

  if(savedError.code!=0 && theError.code==4350) // Trans already aborted
      theError= savedError;

  DBUG_RETURN(ret);
}

int 
NdbTransaction::executeNoBlobs(NdbTransaction::ExecType aTypeOfExec, 
			       NdbOperation::AbortOption abortOption,
			       int forceSend)
{
  DBUG_ENTER("NdbTransaction::executeNoBlobs");
  DBUG_PRINT("enter", ("aTypeOfExec: %d, abortOption: %d", 
		       aTypeOfExec, abortOption));

//------------------------------------------------------------------------
// We will start by preparing all operations in the transaction defined
// since last execute or since beginning. If this works ok we will continue
// by calling the poll with wait method. This method will return when
// the NDB kernel has completed its task or when 10 seconds have passed.
// The NdbTransactionCallBack-method will receive the return code of the
// transaction. The normal methods of reading error codes still apply.
//------------------------------------------------------------------------
  Ndb* tNdb = theNdb;

  Uint32 timeout = theNdb->theImpl->m_transporter_facade->m_waitfor_timeout;
  m_waitForReply = false;
  executeAsynchPrepare(aTypeOfExec, NULL, NULL, abortOption);
  if (m_waitForReply){
    while (1) {
      int noOfComp = tNdb->sendPollNdb(3 * timeout, 1, forceSend);
      if (noOfComp == 0) {
        /*
         * Just for fun, this is only one of two places where
         * we could hit this error... It's quite possible we
         * hit it in Ndbif.cpp in Ndb::check_send_timeout()
         *
         * We behave rather similarly in both places.
         * Hitting this is certainly a bug though...
         */
        g_eventLogger.error("WARNING: Timeout in executeNoBlobs() waiting for "
                            "response from NDB data nodes. This should NEVER "
                            "occur. You have likely hit a NDB Bug. Please "
                            "file a bug.");
        DBUG_PRINT("error",("This timeout should never occure, execute()"));
        g_eventLogger.error("Forcibly trying to rollback txn (%p"
                            ") to try to clean up data node resources.",
                            this);
        executeNoBlobs(NdbTransaction::Rollback);
        theError.code = 4012;
        theError.status= NdbError::PermanentError;
        theError.classification= NdbError::TimeoutExpired;
        setOperationErrorCodeAbort(4012); // ndbd timeout
        DBUG_RETURN(-1);
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
        DBUG_RETURN(-1);
      }//if
      break;
    }
  }
  thePendingBlobOps = 0;
  DBUG_RETURN(0);
}//NdbTransaction::execute()

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
NdbTransaction::executeAsynchPrepare(NdbTransaction::ExecType aTypeOfExec,
                                     NdbAsynchCallback  aCallback,
                                     void*              anyObject,
                                     NdbOperation::AbortOption abortOption)
{
  DBUG_ENTER("NdbTransaction::executeAsynchPrepare");
  DBUG_PRINT("enter", ("aTypeOfExec: %d, aCallback: 0x%lx, anyObject: Ox%lx",
		       aTypeOfExec, (long) aCallback, (long) anyObject));

  /**
   * Reset error.code on execute
   */
  if (theError.code != 0)
    DBUG_PRINT("enter", ("Resetting error %d on execute", theError.code));
  /**
   * for timeout (4012) we want sendROLLBACK to behave differently.
   * Else, normal behaviour of reset errcode
   */
  if (theError.code != 4012)
    theError.code = 0;
  NdbScanOperation* tcOp = m_theFirstScanOperation;
  if (tcOp != 0){
    // Execute any cursor operations
    while (tcOp != NULL) {
      int tReturnCode;
      tReturnCode = tcOp->executeCursor(theDBnode);
      if (tReturnCode == -1) {
        DBUG_VOID_RETURN;
      }//if
      tcOp = (NdbScanOperation*)tcOp->next();
    } // while
    m_theLastScanOperation->next(m_firstExecutedScanOp);
    m_firstExecutedScanOp = m_theFirstScanOperation;
    // Discard cursor operations, since these are also
    // in the complete operations list we do not need
    // to release them.
    m_theFirstScanOperation = m_theLastScanOperation = NULL;
  }

  bool tTransactionIsStarted = theTransactionIsStarted;
  NdbOperation*	tLastOp = theLastOpInList;
  Ndb* tNdb = theNdb;
  CommitStatusType tCommitStatus = theCommitStatus;
  Uint32 tnoOfPreparedTransactions = tNdb->theNoOfPreparedTransactions;

  theReturnStatus     = ReturnSuccess;
  theCallbackFunction = aCallback;
  theCallbackObject   = anyObject;
  m_waitForReply = true;
  tNdb->thePreparedTransactionsArray[tnoOfPreparedTransactions] = this;
  theTransArrayIndex = tnoOfPreparedTransactions;
  theListState = InPreparedList;
  tNdb->theNoOfPreparedTransactions = tnoOfPreparedTransactions + 1;

  if ((tCommitStatus != Started) ||
      (aTypeOfExec == Rollback)) {
/*****************************************************************************
 *	Rollback have been ordered on a started transaction. Call rollback.
 *      Could also be state problem or previous problem which leads to the 
 *      same action.
 ****************************************************************************/
    if (aTypeOfExec == Rollback) {
      if (theTransactionIsStarted == false || theSimpleState) {
	theCommitStatus = Aborted;
	theSendStatus = sendCompleted;
      } else {
	theSendStatus = sendABORT;
      }
    } else {
      theSendStatus = sendABORTfail;
    }//if
    if (theCommitStatus == Aborted){
      DBUG_PRINT("exit", ("theCommitStatus: Aborted"));
      setErrorCode(4350);
    }
    DBUG_VOID_RETURN;
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
      if (aTypeOfExec == Commit && !theSimpleState) {
	/**********************************************************************
	 *   A Transaction have been started and no more operations exist. 
	 *   We will use the commit method.
	 *********************************************************************/
        theSendStatus = sendCOMMITstate;
	DBUG_VOID_RETURN;
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
	DBUG_VOID_RETURN; // No Commit with no operations is OK
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
      DBUG_VOID_RETURN;
    }//if
  }

  NdbOperation* tOp = theFirstOpInList;
  theCompletionStatus = NotCompleted;
  while (tOp) {
    int tReturnCode;
    NdbOperation* tNextOp = tOp->next();
    tReturnCode = tOp->prepareSend(theTCConPtr, theTransactionId, abortOption);
    if (tReturnCode == -1) {
      theSendStatus = sendABORTfail;
      DBUG_VOID_RETURN;
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
  NdbNodeBitmask::clear(m_db_nodes);
  NdbNodeBitmask::clear(m_failed_db_nodes);
  DBUG_VOID_RETURN;
}//NdbTransaction::executeAsynchPrepare()

void
NdbTransaction::executeAsynch(ExecType aTypeOfExec,
                              NdbAsynchCallback aCallback,
                              void* anyObject,
                              NdbOperation::AbortOption abortOption,
                              int forceSend)
{
  executeAsynchPrepare(aTypeOfExec, aCallback, anyObject, abortOption);
  theNdb->sendPreparedTransactions(forceSend);
}

void NdbTransaction::close()
{
  theNdb->closeTransaction(this);
}

int NdbTransaction::refresh(){
  return sendTC_HBREP();
}

/*****************************************************************************
int sendTC_HBREP();

Return Value:  No return value.  
Parameters :   None.
Remark:        Order NDB to refresh the timeout counter of the transaction. 
******************************************************************************/
int 	
NdbTransaction::sendTC_HBREP()		// Send a TC_HBREP signal;
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
 
  TransporterFacade *tp = theNdb->theImpl->m_transporter_facade;
  tp->lock_mutex(); 
  const int res = tp->sendSignal(tSignal,theDBnode);
  tp->unlock_mutex(); 
  tNdb->releaseSignal(tSignal);

  if (res == -1){
    return -1;
  }    
  
  return 0;
}//NdbTransaction::sendTC_HBREP()

/*****************************************************************************
int doSend();

Return Value:  Return 0 : send was successful.
               Return -1: In all other case.  
Remark:        Send all operations belonging to this connection.
               The caller of this method has the responsibility to remove the
               object from the prepared transactions array on the Ndb-object.
*****************************************************************************/
int
NdbTransaction::doSend()
{
  DBUG_ENTER("NdbTransaction::doSend");

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
    DBUG_RETURN(0);
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
      DBUG_RETURN(0);
    }//if
    break;
  }//case
  case sendCOMMITstate:
    if (sendCOMMIT() == 0) {
      DBUG_RETURN(0);
    }//if
    break;
  case sendCompleted:
    theNdb->insert_completed_list(this); 
    DBUG_RETURN(0);
  default:
    ndbout << "Inconsistent theSendStatus = "
	   << (Uint32) theSendStatus << endl;
    abort();
    break;
  }//switch
  setOperationErrorCodeAbort(4002);
  theReleaseOnClose = true;
  theTransactionIsStarted = false;
  theCommitStatus = Aborted;
  DBUG_RETURN(-1);
}//NdbTransaction::doSend()

/**************************************************************************
int sendROLLBACK();

Return Value:  Return -1 if send unsuccessful.  
Parameters :   None.
Remark:        Order NDB to rollback the transaction. 
**************************************************************************/
int 	
NdbTransaction::sendROLLBACK()      // Send a TCROLLBACKREQ signal;
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
    TransporterFacade *tp = theNdb->theImpl->m_transporter_facade;
    int	  tReturnCode;

    tTransId1 = (Uint32) theTransactionId;
    tTransId2 = (Uint32) (theTransactionId >> 32);
    tSignal.setSignal(GSN_TCROLLBACKREQ);
    tSignal.setData(theTCConPtr, 1);
    tSignal.setData(tTransId1, 2);
    tSignal.setData(tTransId2, 3);
    if(theError.code == 4012)
    {
      g_eventLogger.error("Sending TCROLLBACKREQ with Bad flag");
      tSignal.setLength(tSignal.getLength() + 1); // + flags
      tSignal.setData(0x1, 4); // potentially bad data
    }
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
}//NdbTransaction::sendROLLBACK()

/***************************************************************************
int sendCOMMIT();

Return Value:  Return 0 : send was successful.
               Return -1: In all other case.  
Parameters :   None.
Remark:        Order NDB to commit the transaction. 
***************************************************************************/
int 	
NdbTransaction::sendCOMMIT()    // Send a TC_COMMITREQ signal;
{
  NdbApiSignal tSignal(theNdb->theMyRef);
  Uint32 tTransId1, tTransId2;
  TransporterFacade *tp = theNdb->theImpl->m_transporter_facade;
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
}//NdbTransaction::sendCOMMIT()

/******************************************************************************
void release();

Remark:         Release all operations.
******************************************************************************/
void 
NdbTransaction::release(){
  releaseOperations();
  if ( (theTransactionIsStarted == true) &&
       ((theCommitStatus != Committed) &&
	(theCommitStatus != Aborted))) {
    /************************************************************************
     *	The user did not perform any rollback but simply closed the
     *      transaction. We must rollback Ndb since Ndb have been contacted.
     ************************************************************************/
    if (!theSimpleState)
    {
      execute(Rollback);
    }
  }//if
  theMagicNumber = 0xFE11DC;
  theInUseState = false;
#ifdef VM_TRACE
  if (theListState != NotInList) {
    theNdb->printState("release %x", this);
    abort();
  }
#endif
}//NdbTransaction::release()

void
NdbTransaction::releaseOps(NdbOperation* tOp){
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
NdbTransaction::releaseOperations()
{
  // Release any open scans
  releaseScanOperations(m_theFirstScanOperation);
  releaseScanOperations(m_firstExecutedScanOp);
  
  releaseOps(theCompletedFirstOp);
  releaseOps(theFirstOpInList);
  releaseOps(theFirstExecOpInList);

  theCompletedFirstOp = NULL;
  theCompletedLastOp = NULL;
  theFirstOpInList = NULL;
  theFirstExecOpInList = NULL;
  theLastOpInList = NULL;
  theLastExecOpInList = NULL;
  theScanningOp = NULL;
  m_theFirstScanOperation = NULL;
  m_theLastScanOperation = NULL;
  m_firstExecutedScanOp = NULL;
}//NdbTransaction::releaseOperations()

void 
NdbTransaction::releaseCompletedOperations()
{
  releaseOps(theCompletedFirstOp);
  theCompletedFirstOp = NULL;
  theCompletedLastOp = NULL;
}//NdbTransaction::releaseOperations()

/******************************************************************************
void releaseScanOperations();

Remark:         Release all cursor operations. 
                (NdbScanOperation and NdbIndexOperation)
******************************************************************************/
void 
NdbTransaction::releaseScanOperations(NdbIndexScanOperation* cursorOp)
{
  while(cursorOp != 0){
    NdbIndexScanOperation* next = (NdbIndexScanOperation*)cursorOp->next();
    cursorOp->release();
    theNdb->releaseScanOperation(cursorOp);
    cursorOp = next;
  }
}//NdbTransaction::releaseScanOperations()

/*****************************************************************************
void releaseScanOperation();

Remark:         Release scan op when hupp'ed trans closed (save memory)
******************************************************************************/
void 
NdbTransaction::releaseExecutedScanOperation(NdbIndexScanOperation* cursorOp)
{
  DBUG_ENTER("NdbTransaction::releaseExecutedScanOperation");
  DBUG_PRINT("enter", ("this: 0x%lx  op: 0x%lx", (long) this, (long) cursorOp));
  
  releaseScanOperation(&m_firstExecutedScanOp, 0, cursorOp);
  
  DBUG_VOID_RETURN;
}//NdbTransaction::releaseExecutedScanOperation()

bool
NdbTransaction::releaseScanOperation(NdbIndexScanOperation** listhead,
				     NdbIndexScanOperation** listtail,
				     NdbIndexScanOperation* op)
{
  if (* listhead == op)
  {
    * listhead = (NdbIndexScanOperation*)op->theNext;
    if (listtail && *listtail == op)
    {
      assert(* listhead == 0);
      * listtail = 0;
    }
      
  }
  else
  {
    NdbIndexScanOperation* tmp = * listhead;
    while (tmp != NULL)
    {
      if (tmp->theNext == op)
      {
	tmp->theNext = (NdbIndexScanOperation*)op->theNext;
	if (listtail && *listtail == op)
	{
	  assert(op->theNext == 0);
	  *listtail = tmp;
	}
	break;
      }
      tmp = (NdbIndexScanOperation*)tmp->theNext;
    }
    if (tmp == NULL)
      op = NULL;
  }
  
  if (op != NULL)
  {
    op->release();
    theNdb->releaseScanOperation(op);
    return true;
  }
  
  return false;
}

/*****************************************************************************
NdbOperation* getNdbOperation(const char* aTableName);

Return Value    Return a pointer to a NdbOperation object if getNdbOperation 
                was succesful.
                Return NULL : In all other case. 	
Parameters:     aTableName : Name of the database table. 	
Remark:         Get an operation from NdbOperation idlelist and get the 
                NdbTransaction object 
		who was fetch by startTransaction pointing to this  operation  
		getOperation will set the theTableId in the NdbOperation object.
                synchronous
******************************************************************************/
NdbOperation*
NdbTransaction::getNdbOperation(const char* aTableName)
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
}//NdbTransaction::getNdbOperation()

/*****************************************************************************
NdbOperation* getNdbOperation(int aTableId);

Return Value    Return a pointer to a NdbOperation object if getNdbOperation 
                was succesful.
                Return NULL: In all other case. 	
Parameters:     tableId : Id of the database table beeing deleted.
Remark:         Get an operation from NdbOperation object idlelist and 
                get the NdbTransaction object who was fetch by 
                startTransaction pointing to this operation 
  	        getOperation will set the theTableId in the NdbOperation 
                object, synchronous.
*****************************************************************************/
NdbOperation*
NdbTransaction::getNdbOperation(const NdbTableImpl * tab, NdbOperation* aNextOp)
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
}//NdbTransaction::getNdbOperation()

NdbOperation* NdbTransaction::getNdbOperation(const NdbDictionary::Table * table)
{
  if (table)
    return getNdbOperation(& NdbTableImpl::getImpl(*table));
  else
    return NULL;
}//NdbTransaction::getNdbOperation()

// NdbScanOperation
/*****************************************************************************
NdbScanOperation* getNdbScanOperation(const char* aTableName);

Return Value    Return a pointer to a NdbScanOperation object if getNdbScanOperation was succesful.
                Return NULL : In all other case. 	
Parameters:     aTableName : Name of the database table. 	
Remark:         Get an operation from NdbScanOperation idlelist and get the NdbTransaction object 
		who was fetch by startTransaction pointing to this  operation  
		getOperation will set the theTableId in the NdbOperation object.synchronous
******************************************************************************/
NdbScanOperation*
NdbTransaction::getNdbScanOperation(const char* aTableName)
{
  if (theCommitStatus == Started){
    NdbTableImpl* tab = theNdb->theDictionary->getTable(aTableName);
    if (tab != 0){
      return getNdbScanOperation(tab);
    } else {
      setOperationErrorCodeAbort(theNdb->theDictionary->m_error.code);
      return NULL;
    }//if
  } 
  
  setOperationErrorCodeAbort(4114);
  return NULL;
}//NdbTransaction::getNdbScanOperation()

/*****************************************************************************
NdbScanOperation* getNdbIndexScanOperation(const char* anIndexName, const char* aTableName);

Return Value    Return a pointer to a NdbIndexScanOperation object if getNdbIndexScanOperation was succesful.
                Return NULL : In all other case. 	
Parameters:     anIndexName : Name of the index to use. 	
                aTableName : Name of the database table. 	
Remark:         Get an operation from NdbIndexScanOperation idlelist and get the NdbTransaction object 
		who was fetch by startTransaction pointing to this  operation  
		getOperation will set the theTableId in the NdbIndexScanOperation object.synchronous
******************************************************************************/
NdbIndexScanOperation*
NdbTransaction::getNdbIndexScanOperation(const char* anIndexName, 
					const char* aTableName)
{
  NdbIndexImpl* index = 
    theNdb->theDictionary->getIndex(anIndexName, aTableName);
  if (index == 0)
  {
    setOperationErrorCodeAbort(theNdb->theDictionary->getNdbError().code);
    return 0;
  }
  NdbTableImpl* table = theNdb->theDictionary->getTable(aTableName);
  if (table == 0)
  {
    setOperationErrorCodeAbort(theNdb->theDictionary->getNdbError().code);
    return 0;
  }

  return getNdbIndexScanOperation(index, table);
}

NdbIndexScanOperation*
NdbTransaction::getNdbIndexScanOperation(const NdbIndexImpl* index,
					const NdbTableImpl* table)
{
  if (theCommitStatus == Started){
    const NdbTableImpl * indexTable = index->getIndexTable();
    if (indexTable != 0){
      NdbIndexScanOperation* tOp = getNdbScanOperation(indexTable);
      if(tOp)
      {
	tOp->m_currentTable = table;
        // Mark that this really an NdbIndexScanOperation
        tOp->m_type = NdbOperation::OrderedIndexScan;
      }
      return tOp;
    } else {
      setOperationErrorCodeAbort(4271);
      return NULL;
    }//if
  } 
  
  setOperationErrorCodeAbort(4114);
  return NULL;
}//NdbTransaction::getNdbIndexScanOperation()

NdbIndexScanOperation* 
NdbTransaction::getNdbIndexScanOperation(const NdbDictionary::Index * index)
{ 
  if (index)
  {
    const NdbDictionary::Table *table=
      theNdb->theDictionary->getTable(index->getTable());

    if (table)
      return getNdbIndexScanOperation(index, table);

    setOperationErrorCodeAbort(theNdb->theDictionary->getNdbError().code);
    return NULL;
  }
  setOperationErrorCodeAbort(4271);
  return NULL;
}

NdbIndexScanOperation* 
NdbTransaction::getNdbIndexScanOperation(const NdbDictionary::Index * index,
					const NdbDictionary::Table * table)
{
  if (index && table)
    return getNdbIndexScanOperation(& NdbIndexImpl::getImpl(*index),
				    & NdbTableImpl::getImpl(*table));
  setOperationErrorCodeAbort(4271);
  return NULL;
}//NdbTransaction::getNdbIndexScanOperation()

/*****************************************************************************
NdbScanOperation* getNdbScanOperation(int aTableId);

Return Value    Return a pointer to a NdbScanOperation object if getNdbScanOperation was succesful.
                Return NULL: In all other case. 	
Parameters:     tableId : Id of the database table beeing deleted.
Remark:         Get an operation from NdbScanOperation object idlelist and get the NdbTransaction 
                object who was fetch by startTransaction pointing to this  operation 
  	        getOperation will set the theTableId in the NdbScanOperation object, synchronous.
*****************************************************************************/
NdbIndexScanOperation*
NdbTransaction::getNdbScanOperation(const NdbTableImpl * tab)
{ 
  NdbIndexScanOperation* tOp;
  
  tOp = theNdb->getScanOperation();
  if (tOp == NULL)
    goto getNdbOp_error1;
  
  if (tOp->init(tab, this) != -1) {
    define_scan_op(tOp);
    // Mark that this NdbIndexScanOperation is used as NdbScanOperation
    tOp->m_type = NdbOperation::TableScan; 
    return tOp;
  } else {
    theNdb->releaseScanOperation(tOp);
  }//if
  return NULL;

getNdbOp_error1:
  setOperationErrorCodeAbort(4000);
  return NULL;
}//NdbTransaction::getNdbScanOperation()

void
NdbTransaction::remove_list(NdbOperation*& list, NdbOperation* op){
  NdbOperation* tmp= list;
  if(tmp == op)
    list = op->next();
  else {
    while(tmp && tmp->next() != op) tmp = tmp->next();
    if(tmp)
      tmp->next(op->next());
  }
  op->next(NULL);
}

void
NdbTransaction::define_scan_op(NdbIndexScanOperation * tOp){
  // Link scan operation into list of cursor operations
  if (m_theLastScanOperation == NULL)
    m_theFirstScanOperation = m_theLastScanOperation = tOp;
  else {
    m_theLastScanOperation->next(tOp);
    m_theLastScanOperation = tOp;
  }
  tOp->next(NULL);
}

NdbScanOperation* 
NdbTransaction::getNdbScanOperation(const NdbDictionary::Table * table)
{
  if (table)
    return getNdbScanOperation(& NdbTableImpl::getImpl(*table));
  else
    return NULL;
}//NdbTransaction::getNdbScanOperation()


// IndexOperation
/*****************************************************************************
NdbIndexOperation* getNdbIndexOperation(const char* anIndexName,
					const char* aTableName);

Return Value    Return a pointer to a NdbOperation object if getNdbIndexOperation was succesful.
                Return NULL : In all other case. 	
Parameters:     aTableName : Name of the database table. 	
Remark:         Get an operation from NdbIndexOperation idlelist and get the NdbTransaction object 
		who was fetch by startTransaction pointing to this operation  
		getOperation will set the theTableId in the NdbIndexOperation object.synchronous
******************************************************************************/
NdbIndexOperation*
NdbTransaction::getNdbIndexOperation(const char* anIndexName, 
                                    const char* aTableName)
{
  if (theCommitStatus == Started) {
    NdbTableImpl * table = theNdb->theDictionary->getTable(aTableName);
    NdbIndexImpl * index;

    if (table == 0)
    {
      setOperationErrorCodeAbort(theNdb->theDictionary->getNdbError().code);
      return NULL;
    }

    if (table->m_frm.get_data())
    {
      // This unique index is defined from SQL level
      static const char* uniqueSuffix= "$unique";
      BaseString uniqueIndexName(anIndexName);
      uniqueIndexName.append(uniqueSuffix);
      index = theNdb->theDictionary->getIndex(uniqueIndexName.c_str(),
					      aTableName);      
    }
    else
      index = theNdb->theDictionary->getIndex(anIndexName,
					      aTableName);
    if(table != 0 && index != 0){
      return getNdbIndexOperation(index, table);
    }
    
    if(index == 0){
      setOperationErrorCodeAbort(4243);
      return NULL;
    }

    setOperationErrorCodeAbort(4243);
    return NULL;
  } 
  
  setOperationErrorCodeAbort(4114);
  return 0;
}//NdbTransaction::getNdbIndexOperation()

/*****************************************************************************
NdbIndexOperation* getNdbIndexOperation(int anIndexId, int aTableId);

Return Value    Return a pointer to a NdbIndexOperation object if getNdbIndexOperation was succesful.
                Return NULL: In all other case. 	
Parameters:     tableId : Id of the database table beeing deleted.
Remark:         Get an operation from NdbIndexOperation object idlelist and get the NdbTransaction 
                object who was fetch by startTransaction pointing to this  operation 
  	        getOperation will set the theTableId in the NdbIndexOperation object, synchronous.
*****************************************************************************/
NdbIndexOperation*
NdbTransaction::getNdbIndexOperation(const NdbIndexImpl * anIndex, 
				    const NdbTableImpl * aTable,
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
}//NdbTransaction::getNdbIndexOperation()

NdbIndexOperation* 
NdbTransaction::getNdbIndexOperation(const NdbDictionary::Index * index)
{ 
  if (index)
  {
    const NdbDictionary::Table *table=
      theNdb->theDictionary->getTable(index->getTable());

    if (table)
      return getNdbIndexOperation(index, table);

    setOperationErrorCodeAbort(theNdb->theDictionary->getNdbError().code);
    return NULL;
  }
  setOperationErrorCodeAbort(4271);
  return NULL;
}

NdbIndexOperation* 
NdbTransaction::getNdbIndexOperation(const NdbDictionary::Index * index,
				    const NdbDictionary::Table * table)
{
  if (index && table)
    return getNdbIndexOperation(& NdbIndexImpl::getImpl(*index),
				& NdbTableImpl::getImpl(*table));
  
  setOperationErrorCodeAbort(4271);
  return NULL;
}//NdbTransaction::getNdbIndexOperation()


/*******************************************************************************
int  receiveDIHNDBTAMPER(NdbApiSignal* aSignal)

Return Value:  Return 0 : receiveDIHNDBTAMPER was successful.
               Return -1: In all other case.
Parameters:    aSignal: The signal object pointer.
Remark:        Sets theRestartGCI in the NDB object. 
*******************************************************************************/
int			
NdbTransaction::receiveDIHNDBTAMPER(NdbApiSignal* aSignal)
{
  if (theStatus != Connecting) {
    return -1;
  } else {
    theNdb->RestartGCI((Uint32)aSignal->readData(2));
    theStatus = Connected;
  }//if
  return 0;  
}//NdbTransaction::receiveDIHNDBTAMPER()

/*******************************************************************************
int  receiveTCSEIZECONF(NdbApiSignal* aSignal);

Return Value:  Return 0 : receiveTCSEIZECONF was successful.
               Return -1: In all other case.
Parameters:    aSignal: The signal object pointer.
Remark:        Sets TC Connect pointer at reception of TCSEIZECONF. 
*******************************************************************************/
int			
NdbTransaction::receiveTCSEIZECONF(NdbApiSignal* aSignal)
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
}//NdbTransaction::receiveTCSEIZECONF()

/*******************************************************************************
int  receiveTCSEIZEREF(NdbApiSignal* aSignal);

Return Value:  Return 0 : receiveTCSEIZEREF was successful.
               Return -1: In all other case.
Parameters:    aSignal: The signal object pointer.
Remark:        Sets TC Connect pointer. 
*******************************************************************************/
int			
NdbTransaction::receiveTCSEIZEREF(NdbApiSignal* aSignal)
{
  DBUG_ENTER("NdbTransaction::receiveTCSEIZEREF");
  if (theStatus != Connecting)
  {
    DBUG_RETURN(-1);
  } else
  {
    theStatus = ConnectFailure;
    theNdb->theError.code = aSignal->readData(2);
    DBUG_PRINT("info",("error code %d, %s",
		       theNdb->getNdbError().code,
		       theNdb->getNdbError().message));
    DBUG_RETURN(0);
  }
}//NdbTransaction::receiveTCSEIZEREF()

/*******************************************************************************
int  receiveTCRELEASECONF(NdbApiSignal* aSignal);

Return Value:  Return 0 : receiveTCRELEASECONF was successful.
               Return -1: In all other case.
Parameters:    aSignal: The signal object pointer.
Remark:         DisConnect TC Connect pointer to NDBAPI. 
*******************************************************************************/
int			
NdbTransaction::receiveTCRELEASECONF(NdbApiSignal* aSignal)
{
  if (theStatus != DisConnecting)
  {
    return -1;
  } else
  {
    theStatus = NotConnected;
  }
  return 0;
}//NdbTransaction::receiveTCRELEASECONF()

/*******************************************************************************
int  receiveTCRELEASEREF(NdbApiSignal* aSignal);

Return Value:  Return 0 : receiveTCRELEASEREF was successful.
               Return -1: In all other case.
Parameters:    aSignal: The signal object pointer.
Remark:        DisConnect TC Connect pointer to NDBAPI Failure. 
*******************************************************************************/
int			
NdbTransaction::receiveTCRELEASEREF(NdbApiSignal* aSignal)
{
  if (theStatus != DisConnecting) {
    return -1;
  } else {
    theStatus = ConnectFailure;
    theNdb->theError.code = aSignal->readData(2);
    return 0;
  }//if
}//NdbTransaction::receiveTCRELEASEREF()

/******************************************************************************
int  receiveTC_COMMITCONF(NdbApiSignal* aSignal);

Return Value:  Return 0 : receiveTC_COMMITCONF was successful.
               Return -1: In all other case.
Parameters:    aSignal: The signal object pointer.
Remark:        
******************************************************************************/
int			
NdbTransaction::receiveTC_COMMITCONF(const TcCommitConf * commitConf)
{ 
  if(checkState_TransId(&commitConf->transId1)){
    theCommitStatus = Committed;
    theCompletionStatus = CompletedSuccess;
    theGlobalCheckpointId = commitConf->gci;
    // theGlobalCheckpointId == 0 if NoOp transaction
    if (theGlobalCheckpointId)
      *p_latest_trans_gci = theGlobalCheckpointId;
    return 0;
  } else {
#ifdef NDB_NO_DROPPED_SIGNAL
    abort();
#endif
  }
  return -1;
}//NdbTransaction::receiveTC_COMMITCONF()

/******************************************************************************
int  receiveTC_COMMITREF(NdbApiSignal* aSignal);

Return Value:  Return 0 : receiveTC_COMMITREF was successful.
               Return -1: In all other case.
Parameters:    aSignal: The signal object pointer.
Remark:        
******************************************************************************/
int			
NdbTransaction::receiveTC_COMMITREF(NdbApiSignal* aSignal)
{
  const TcCommitRef * ref = CAST_CONSTPTR(TcCommitRef, aSignal->getDataPtr());
  if(checkState_TransId(&ref->transId1)){
    setOperationErrorCodeAbort(ref->errorCode);
    theCommitStatus = Aborted;
    theCompletionStatus = CompletedFailure;
    theReturnStatus = ReturnFailure;
    return 0;
  } else {
#ifdef NDB_NO_DROPPED_SIGNAL
    abort();
#endif
  }

  return -1;
}//NdbTransaction::receiveTC_COMMITREF()

/******************************************************************************
int  receiveTCROLLBACKCONF(NdbApiSignal* aSignal);

Return Value:  Return 0 : receiveTCROLLBACKCONF was successful.
               Return -1: In all other case.
Parameters:    aSignal: The signal object pointer.
Remark:        
******************************************************************************/
int			
NdbTransaction::receiveTCROLLBACKCONF(NdbApiSignal* aSignal)
{
  if(checkState_TransId(aSignal->getDataPtr() + 1)){
    theCommitStatus = Aborted;
    theCompletionStatus = CompletedSuccess;
    return 0;
  } else {
#ifdef NDB_NO_DROPPED_SIGNAL
    abort();
#endif
  }

  return -1;
}//NdbTransaction::receiveTCROLLBACKCONF()

/*******************************************************************************
int  receiveTCROLLBACKREF(NdbApiSignal* aSignal);

Return Value:  Return 0 : receiveTCROLLBACKREF was successful.
               Return -1: In all other case.
Parameters:    aSignal: The signal object pointer.
Remark:        
*******************************************************************************/
int			
NdbTransaction::receiveTCROLLBACKREF(NdbApiSignal* aSignal)
{
  if(checkState_TransId(aSignal->getDataPtr() + 1)){
    setOperationErrorCodeAbort(aSignal->readData(4));
    theCommitStatus = Aborted;
    theCompletionStatus = CompletedFailure;
    theReturnStatus = ReturnFailure;
    return 0;
  } else {
#ifdef NDB_NO_DROPPED_SIGNAL
    abort();
#endif
  }

  return -1;
}//NdbTransaction::receiveTCROLLBACKREF()

/*****************************************************************************
int receiveTCROLLBACKREP( NdbApiSignal* aSignal)

Return Value:   Return 0 : send was succesful.
                Return -1: In all other case.   
Parameters:     aSignal: the signal object that contains the 
                TCROLLBACKREP signal from TC.
Remark:         Handles the reception of the ROLLBACKREP signal.
*****************************************************************************/
int
NdbTransaction::receiveTCROLLBACKREP( NdbApiSignal* aSignal)
{
  DBUG_ENTER("NdbTransaction::receiveTCROLLBACKREP");

  /****************************************************************************
Check that we are expecting signals from this transaction and that it doesn't
belong to a transaction already completed. Simply ignore messages from other 
transactions.
  ****************************************************************************/
  if(checkState_TransId(aSignal->getDataPtr() + 1)){
    theError.code = aSignal->readData(4);// Override any previous errors
    if (aSignal->getLength() == TcRollbackRep::SignalLength)
    {
      // Signal may contain additional error data
      theError.details = (char *) aSignal->readData(5);
    }

    /**********************************************************************/
    /*	A serious error has occured. This could be due to deadlock or */
    /*	lack of resources or simply a programming error in NDB. This  */
    /*	transaction will be aborted. Actually it has already been     */
    /*	and we only need to report completion and return with the     */
    /*	error code to the application.				      */
    /**********************************************************************/
    theCompletionStatus = CompletedFailure;
    theCommitStatus = Aborted;
    theReturnStatus = ReturnFailure;
    DBUG_RETURN(0);
  } else {
#ifdef NDB_NO_DROPPED_SIGNAL
    abort();
#endif
  }

  DBUG_RETURN(-1);
}//NdbTransaction::receiveTCROLLBACKREP()

/*******************************************************************************
int  receiveTCKEYCONF(NdbApiSignal* aSignal, Uint32 long_short_ind);

Return Value:  Return 0 : receiveTCKEYCONF was successful.
               Return -1: In all other case.
Parameters:    aSignal: The signal object pointer.
Remark:        
*******************************************************************************/
int			
NdbTransaction::receiveTCKEYCONF(const TcKeyConf * keyConf, Uint32 aDataLength)
{
  NdbReceiver* tOp;
  const Uint32 tTemp = keyConf->confInfo;
  /***************************************************************************
Check that we are expecting signals from this transaction and that it
doesn't belong to a transaction already completed. Simply ignore messages
from other transactions.
  ***************************************************************************/
  if(checkState_TransId(&keyConf->transId1)){

    const Uint32 tNoOfOperations = TcKeyConf::getNoOfOperations(tTemp);
    const Uint32 tCommitFlag = TcKeyConf::getCommitFlag(tTemp);

    const Uint32* tPtr = (Uint32 *)&keyConf->operations[0];
    Uint32 tNoComp = theNoOfOpCompleted;
    for (Uint32 i = 0; i < tNoOfOperations ; i++) {
      tOp = theNdb->void2rec(theNdb->int2void(*tPtr++));
      const Uint32 tAttrInfoLen = *tPtr++;
      if (tOp && tOp->checkMagicNumber()) {
	Uint32 done = tOp->execTCOPCONF(tAttrInfoLen);
	if(tAttrInfoLen > TcKeyConf::DirtyReadBit){
	  Uint32 node = tAttrInfoLen & (~TcKeyConf::DirtyReadBit);
	  NdbNodeBitmask::set(m_db_nodes, node);
	  if(NdbNodeBitmask::get(m_failed_db_nodes, node) && !done)
	  {
	    done = 1;
	    tOp->setErrorCode(4119);
	    theCompletionStatus = CompletedFailure;
	    theReturnStatus = NdbTransaction::ReturnFailure;
	  }	    
	}
	tNoComp += done;
      } else {
 	return -1;
      }//if
    }//for
    Uint32 tNoSent = theNoOfOpSent;
    theNoOfOpCompleted = tNoComp;
    Uint32 tGCI    = keyConf->gci;
    if (tCommitFlag == 1) {
      theCommitStatus = Committed;
      theGlobalCheckpointId = tGCI;
      if (tGCI) // Read(dirty) only transaction doesnt get GCI
      {
	*p_latest_trans_gci = tGCI;
      }
    } else if ((tNoComp >= tNoSent) &&
               (theLastExecOpInList->theCommitIndicator == 1)){
      
      
/**********************************************************************/
// We sent the transaction with Commit flag set and received a CONF with
// no Commit flag set. This is clearly an anomaly.
/**********************************************************************/
      theError.code = 4011;
      theCompletionStatus = CompletedFailure;
      theReturnStatus = NdbTransaction::ReturnFailure;
      theCommitStatus = Aborted;
      return 0;
    }//if
    if (tNoComp >= tNoSent) {
      return 0;	// No more operations to wait for
    }//if
     // Not completed the reception yet.
  } else {
#ifdef NDB_NO_DROPPED_SIGNAL
    abort();
#endif
  }
  
  return -1;
}//NdbTransaction::receiveTCKEYCONF()

/*****************************************************************************
int receiveTCKEY_FAILCONF( NdbApiSignal* aSignal)

Return Value:   Return 0 : receive was completed.
                Return -1: In all other case.   
Parameters:     aSignal: the signal object that contains the 
                TCKEY_FAILCONF signal from TC.
Remark:         Handles the reception of the TCKEY_FAILCONF signal.
*****************************************************************************/
int
NdbTransaction::receiveTCKEY_FAILCONF(const TcKeyFailConf * failConf)
{
  NdbOperation*	tOp;
  /*
    Check that we are expecting signals from this transaction and that it 
    doesn't belong  to a transaction already completed. Simply ignore 
    messages from other transactions.
  */
  if(checkState_TransId(&failConf->transId1)){
    /*
      A node failure of the TC node occured. The transaction has
      been committed.
    */
    theCommitStatus = Committed;
    tOp = theFirstExecOpInList;
    while (tOp != NULL) {
      /*
       * Check if the transaction expected read values...
       * If it did some of them might have gotten lost even if we succeeded
       * in committing the transaction.
       */
      switch(tOp->theOperationType){
      case NdbOperation::UpdateRequest:
      case NdbOperation::InsertRequest:
      case NdbOperation::DeleteRequest:
      case NdbOperation::WriteRequest:
	tOp = tOp->next();
	break;
      case NdbOperation::ReadRequest:
      case NdbOperation::ReadExclusive:
      case NdbOperation::OpenScanRequest:
      case NdbOperation::OpenRangeScanRequest:
	theCompletionStatus = CompletedFailure;
	theReturnStatus = NdbTransaction::ReturnFailure;
	setOperationErrorCodeAbort(4115);
	tOp = NULL;
	break;
      case NdbOperation::NotDefined:
      case NdbOperation::NotDefined2:
	assert(false);
	break;
      }//if
    }//while   
    theReleaseOnClose = true;
    return 0;
  } else {
#ifdef VM_TRACE
    ndbout_c("Recevied TCKEY_FAILCONF wo/ operation");
#endif
  }
  return -1;
}//NdbTransaction::receiveTCKEY_FAILCONF()

/*************************************************************************
int receiveTCKEY_FAILREF( NdbApiSignal* aSignal)

Return Value:   Return 0 : receive was completed.
                Return -1: In all other case.   
Parameters:     aSignal: the signal object that contains the 
                TCKEY_FAILREF signal from TC.
Remark:         Handles the reception of the TCKEY_FAILREF signal.
**************************************************************************/
int
NdbTransaction::receiveTCKEY_FAILREF(NdbApiSignal* aSignal)
{
  /*
    Check that we are expecting signals from this transaction and
    that it doesn't belong to a transaction already
    completed. Simply ignore messages from other transactions.
  */
  if(checkState_TransId(aSignal->getDataPtr()+1)){
    /*
      We received an indication of that this transaction was aborted due to a
      node failure.
    */
    if (theSendStatus == NdbTransaction::sendTC_ROLLBACK) {
      /*
	We were in the process of sending a rollback anyways. We will
	report it as a success.
      */
      theCompletionStatus = NdbTransaction::CompletedSuccess;
    } else {
      theReturnStatus = NdbTransaction::ReturnFailure;
      theCompletionStatus = NdbTransaction::CompletedFailure;
      theError.code = 4031;
    }//if
    theReleaseOnClose = true;
    theCommitStatus = NdbTransaction::Aborted;
    return 0;
  } else {
#ifdef VM_TRACE
    ndbout_c("Recevied TCKEY_FAILREF wo/ operation");
#endif
  }
  return -1;
}//NdbTransaction::receiveTCKEY_FAILREF()

/******************************************************************************
int  receiveTCINDXCONF(NdbApiSignal* aSignal, Uint32 long_short_ind);

Return Value:  Return 0 : receiveTCINDXCONF was successful.
               Return -1: In all other case.
Parameters:    aSignal: The signal object pointer.
Remark:        
******************************************************************************/
int			
NdbTransaction::receiveTCINDXCONF(const TcIndxConf * indxConf, 
				 Uint32 aDataLength)
{
  if(checkState_TransId(&indxConf->transId1)){
    const Uint32 tTemp = indxConf->confInfo;
    const Uint32 tNoOfOperations = TcIndxConf::getNoOfOperations(tTemp);
    const Uint32 tCommitFlag = TcKeyConf::getCommitFlag(tTemp);
    
    const Uint32* tPtr = (Uint32 *)&indxConf->operations[0];
    Uint32 tNoComp = theNoOfOpCompleted;
    for (Uint32 i = 0; i < tNoOfOperations ; i++) {
      NdbReceiver* tOp = theNdb->void2rec(theNdb->int2void(*tPtr));
      tPtr++;
      const Uint32 tAttrInfoLen = *tPtr;
      tPtr++;
      if (tOp && tOp->checkMagicNumber()) {
	tNoComp += tOp->execTCOPCONF(tAttrInfoLen);
      } else {
	return -1;
      }//if
    }//for
    Uint32 tNoSent = theNoOfOpSent;
    Uint32 tGCI    = indxConf->gci;
    theNoOfOpCompleted = tNoComp;
    if (tCommitFlag == 1) {
      theCommitStatus = Committed;
      theGlobalCheckpointId = tGCI;
      if (tGCI) // Read(dirty) only transaction doesnt get GCI
      {
	*p_latest_trans_gci = tGCI;
      }
    } else if ((tNoComp >= tNoSent) &&
               (theLastExecOpInList->theCommitIndicator == 1)){

      /**********************************************************************/
      // We sent the transaction with Commit flag set and received a CONF with
      // no Commit flag set. This is clearly an anomaly.
      /**********************************************************************/
      theError.code = 4011;
      theCompletionStatus = NdbTransaction::CompletedFailure;
      theCommitStatus = NdbTransaction::Aborted;
      theReturnStatus = NdbTransaction::ReturnFailure;
      return 0;
    }//if
    if (tNoComp >= tNoSent) {
      return 0;	// No more operations to wait for
    }//if
     // Not completed the reception yet.
  } else {
#ifdef NDB_NO_DROPPED_SIGNAL
    abort();
#endif
  }

  return -1;
}//NdbTransaction::receiveTCINDXCONF()

/*******************************************************************************
int OpCompletedFailure();

Return Value:  Return 0 : OpCompleteSuccess was successful.
               Return -1: In all other case.
Parameters:    aErrorCode: The error code.
Remark:        An operation was completed with failure.
*******************************************************************************/
int 
NdbTransaction::OpCompleteFailure(NdbOperation* op)
{
  Uint32 tNoComp = theNoOfOpCompleted;
  Uint32 tNoSent = theNoOfOpSent;

  tNoComp++;
  theNoOfOpCompleted = tNoComp;
  
  return (tNoComp == tNoSent) ? 0 : -1;
}//NdbTransaction::OpCompleteFailure()

/******************************************************************************
int OpCompleteSuccess();

Return Value:  Return 0 : OpCompleteSuccess was successful.
               Return -1: In all other case.  
Remark:        An operation was completed with success.
*******************************************************************************/
int 
NdbTransaction::OpCompleteSuccess()
{
  Uint32 tNoComp = theNoOfOpCompleted;
  Uint32 tNoSent = theNoOfOpSent;
  tNoComp++;
  theNoOfOpCompleted = tNoComp;
  if (tNoComp == tNoSent) { // Last operation completed
    return 0;
  } else if (tNoComp < tNoSent) {
    return -1;	// Continue waiting for more signals
  } else {
    setOperationErrorCodeAbort(4113);	// Too many operations, 
                                        // stop waiting for more
    theCompletionStatus = NdbTransaction::CompletedFailure;
    theReturnStatus = NdbTransaction::ReturnFailure;
    return 0;
  }//if
}//NdbTransaction::OpCompleteSuccess()

/******************************************************************************
 int            getGCI();

Remark:		Get global checkpoint identity of the transaction
*******************************************************************************/
int
NdbTransaction::getGCI()
{
  if (theCommitStatus == NdbTransaction::Committed) {
    return theGlobalCheckpointId;
  }//if
  return 0;
}//NdbTransaction::getGCI()

/*******************************************************************************
Uint64 getTransactionId(void);

Remark:        Get the transaction identity. 
*******************************************************************************/
Uint64
NdbTransaction::getTransactionId()
{
  return theTransactionId;
}//NdbTransaction::getTransactionId()

NdbTransaction::CommitStatusType
NdbTransaction::commitStatus()
{
  return theCommitStatus;
}//NdbTransaction::commitStatus()

int 
NdbTransaction::getNdbErrorLine()
{
  return theErrorLine;
}

NdbOperation*
NdbTransaction::getNdbErrorOperation()
{
  return theErrorOperation;
}//NdbTransaction::getNdbErrorOperation()

const NdbOperation * 
NdbTransaction::getNextCompletedOperation(const NdbOperation * current) const {
  if(current == 0)
    return theCompletedFirstOp;
  return current->theNext;
}

#ifdef VM_TRACE
#define CASE(x) case x: ndbout << " " << #x; break
void
NdbTransaction::printState()
{
  ndbout << "con=" << hex << this << dec;
  ndbout << " node=" << getConnectedNodeId();
  switch (theStatus) {
  CASE(NotConnected);
  CASE(Connecting);
  CASE(Connected);
  CASE(DisConnecting);
  CASE(ConnectFailure);
  default: ndbout << (Uint32) theStatus;
  }
  switch (theListState) {
  CASE(NotInList);
  CASE(InPreparedList);
  CASE(InSendList);
  CASE(InCompletedList);
  default: ndbout << (Uint32) theListState;
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
  default: ndbout << (Uint32) theSendStatus;
  }
  switch (theCommitStatus) {
  CASE(NotStarted);
  CASE(Started);
  CASE(Committed);
  CASE(Aborted);
  CASE(NeedAbort);
  default: ndbout << (Uint32) theCommitStatus;
  }
  switch (theCompletionStatus) {
  CASE(NotCompleted);
  CASE(CompletedSuccess);
  CASE(CompletedFailure);
  CASE(DefinitionFailure);
  default: ndbout << (Uint32) theCompletionStatus;
  }
  ndbout << endl;
}
#undef CASE
#endif

int
NdbTransaction::report_node_failure(Uint32 id){
  NdbNodeBitmask::set(m_failed_db_nodes, id);
  if(!NdbNodeBitmask::get(m_db_nodes, id))
  {
    return 0;
  }
  
  /**
   *   Arrived
   *   TCKEYCONF   TRANSIDAI
   * 1)   -           -
   * 2)   -           X
   * 3)   X           -
   * 4)   X           X
   */
  NdbOperation* tmp = theFirstExecOpInList;
  const Uint32 len = TcKeyConf::DirtyReadBit | id;
  Uint32 tNoComp = theNoOfOpCompleted;
  Uint32 tNoSent = theNoOfOpSent;
  Uint32 count = 0;
  while(tmp != 0)
  {
    if(tmp->theReceiver.m_expected_result_length == len && 
       tmp->theReceiver.m_received_result_length == 0)
    {
      count++;
      tmp->theError.code = 4119;
    }
    tmp = tmp->next();
  }
  tNoComp += count;
  theNoOfOpCompleted = tNoComp;
  if(count)
  {
    theReturnStatus = NdbTransaction::ReturnFailure;
    if(tNoComp == tNoSent)
    {
      theError.code = 4119;
      theCompletionStatus = NdbTransaction::CompletedFailure;    
      return 1;
    }
  }
  return 0;
}

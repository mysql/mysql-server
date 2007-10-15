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
#include <Ndb.hpp>
#include <NdbScanOperation.hpp>
#include <NdbIndexScanOperation.hpp>
#include <NdbTransaction.hpp>
#include "NdbApiSignal.hpp"
#include <NdbOut.hpp>
#include "NdbDictionaryImpl.hpp"
#include <NdbBlob.hpp>

#include <NdbRecAttr.hpp>
#include <NdbReceiver.hpp>

#include <stdlib.h>
#include <NdbSqlUtil.hpp>
#include <AttributeHeader.hpp>

#include <signaldata/ScanTab.hpp>
#include <signaldata/KeyInfo.hpp>
#include <signaldata/AttrInfo.hpp>
#include <signaldata/TcKeyReq.hpp>

#define DEBUG_NEXT_RESULT 0

NdbScanOperation::NdbScanOperation(Ndb* aNdb, NdbOperation::Type aType) :
  NdbOperation(aNdb, aType),
  m_transConnection(NULL)
{
  theParallelism = 0;
  m_allocated_receivers = 0;
  m_prepared_receivers = 0;
  m_api_receivers = 0;
  m_conf_receivers = 0;
  m_sent_receivers = 0;
  m_receivers = 0;
  m_array = new Uint32[1]; // skip if on delete in fix_receivers
  theSCAN_TABREQ = 0;
  m_executed = false;
  m_scan_buffer= NULL;
}

NdbScanOperation::~NdbScanOperation()
{
  for(Uint32 i = 0; i<m_allocated_receivers; i++){
    m_receivers[i]->release();
    theNdb->releaseNdbScanRec(m_receivers[i]);
  }
  delete[] m_array;
}

void
NdbScanOperation::setErrorCode(int aErrorCode){
  NdbTransaction* tmp = theNdbCon;
  theNdbCon = m_transConnection;
  NdbOperation::setErrorCode(aErrorCode);
  theNdbCon = tmp;
}

void
NdbScanOperation::setErrorCodeAbort(int aErrorCode){
  NdbTransaction* tmp = theNdbCon;
  theNdbCon = m_transConnection;
  NdbOperation::setErrorCodeAbort(aErrorCode);
  theNdbCon = tmp;
}

  
/*****************************************************************************
 * int init();
 *
 * Return Value:  Return 0 : init was successful.
 *                Return -1: In all other case.  
 * Remark:        Initiates operation record after allocation.
 *****************************************************************************/
int
NdbScanOperation::init(const NdbTableImpl* tab, NdbTransaction* myConnection)
{
  m_transConnection = myConnection;
  //NdbConnection* aScanConnection = theNdb->startTransaction(myConnection);
  theNdb->theRemainingStartTransactions++; // will be checked in hupp...
  NdbTransaction* aScanConnection = theNdb->hupp(myConnection);
  if (!aScanConnection){
    theNdb->theRemainingStartTransactions--;
    setErrorCodeAbort(theNdb->getNdbError().code);
    return -1;
  }

  // NOTE! The hupped trans becomes the owner of the operation
  if(NdbOperation::init(tab, aScanConnection, false) != 0){
    theNdb->theRemainingStartTransactions--;
    return -1;
  }
  
  initInterpreter();
  
  theStatus = GetValue;
  theOperationType = OpenScanRequest;
  theNdbCon->theMagicNumber = 0xFE11DF;
  theNoOfTupKeyLeft = tab->m_noOfDistributionKeys;
  m_read_range_no = 0;
  m_executed = false;
  return 0;
}

int 
NdbScanOperation::readTuples(NdbScanOperation::LockMode lm,
			     Uint32 scan_flags, 
			     Uint32 parallel,
			     Uint32 batch)
{
  m_ordered = m_descending = false;
  Uint32 fragCount = m_currentTable->m_fragmentCount;

  if (parallel > fragCount || parallel == 0) {
     parallel = fragCount;
  }

  // It is only possible to call openScan if 
  //  1. this transcation don't already  contain another scan operation
  //  2. this transaction don't already contain other operations
  //  3. theScanOp contains a NdbScanOperation
  if (theNdbCon->theScanningOp != NULL){
    setErrorCode(4605);
    return -1;
  }

  theNdbCon->theScanningOp = this;
  bool tupScan = (scan_flags & SF_TupScan);

#if 0 // XXX temp for testing
  { char* p = getenv("NDB_USE_TUPSCAN");
    if (p != 0) {
      unsigned n = atoi(p); // 0-10
      if ((unsigned int) (::time(0) % 10) < n) tupScan = true;
    }
  }
#endif
  if (scan_flags & SF_DiskScan)
  {
    tupScan = true;
    m_no_disk_flag = false;
  }
  
  bool rangeScan = false;
  if ( (int) m_accessTable->m_indexType ==
       (int) NdbDictionary::Index::OrderedIndex)
  {
    if (m_currentTable == m_accessTable){
      // Old way of scanning indexes, should not be allowed
      m_currentTable = theNdb->theDictionary->
	getTable(m_currentTable->m_primaryTable.c_str());
      assert(m_currentTable != NULL);
    }
    assert (m_currentTable != m_accessTable);
    // Modify operation state
    theStatus = GetValue;
    theOperationType  = OpenRangeScanRequest;
    rangeScan = true;
    tupScan = false;
  }
  
  if (rangeScan && (scan_flags & SF_OrderBy))
    parallel = fragCount;
  
  theParallelism = parallel;    
  
  if(fix_receivers(parallel) == -1){
    setErrorCodeAbort(4000);
    return -1;
  }
  
  theSCAN_TABREQ = (!theSCAN_TABREQ ? theNdb->getSignal() : theSCAN_TABREQ);
  if (theSCAN_TABREQ == NULL) {
    setErrorCodeAbort(4000);
    return -1;
  }//if
  
  theSCAN_TABREQ->setSignal(GSN_SCAN_TABREQ);
  ScanTabReq * req = CAST_PTR(ScanTabReq, theSCAN_TABREQ->getDataPtrSend());
  req->apiConnectPtr = theNdbCon->theTCConPtr;
  req->tableId = m_accessTable->m_id;
  req->tableSchemaVersion = m_accessTable->m_version;
  req->storedProcId = 0xFFFF;
  req->buddyConPtr = theNdbCon->theBuddyConPtr;
  req->first_batch_size = batch; // Save user specified batch size
  
  Uint32 reqInfo = 0;
  ScanTabReq::setParallelism(reqInfo, parallel);
  ScanTabReq::setScanBatch(reqInfo, 0);
  ScanTabReq::setRangeScanFlag(reqInfo, rangeScan);
  ScanTabReq::setTupScanFlag(reqInfo, tupScan);
  req->requestInfo = reqInfo;

  m_keyInfo = (scan_flags & SF_KeyInfo) ? 1 : 0;
  setReadLockMode(lm);

  Uint64 transId = theNdbCon->getTransactionId();
  req->transId1 = (Uint32) transId;
  req->transId2 = (Uint32) (transId >> 32);

  NdbApiSignal* tSignal = theSCAN_TABREQ->next();
  if(!tSignal)
  {
    theSCAN_TABREQ->next(tSignal = theNdb->getSignal());
  }
  theLastKEYINFO = tSignal;
  
  tSignal->setSignal(GSN_KEYINFO);
  theKEYINFOptr = ((KeyInfo*)tSignal->getDataPtrSend())->keyData;
  theTotalNrOfKeyWordInSignal= 0;

  getFirstATTRINFOScan();
  return 0;
}

void
NdbScanOperation::setReadLockMode(LockMode lockMode)
{
  bool lockExcl, lockHoldMode, readCommitted;
  switch (lockMode)
  {
    case LM_CommittedRead:
      lockExcl= false;
      lockHoldMode= false;
      readCommitted= true;
      break;
    case LM_SimpleRead:
    case LM_Read:
      lockExcl= false;
      lockHoldMode= true;
      readCommitted= false;
      break;
    case LM_Exclusive:
      lockExcl= true;
      lockHoldMode= true;
      readCommitted= false;
      m_keyInfo= 1;
      break;
    default:
      /* Not supported / invalid. */
      assert(false);
  }
  theLockMode= lockMode;
  ScanTabReq *req= CAST_PTR(ScanTabReq, theSCAN_TABREQ->getDataPtrSend());
  Uint32 reqInfo= req->requestInfo;
  ScanTabReq::setLockMode(reqInfo, lockExcl);
  ScanTabReq::setHoldLockFlag(reqInfo, lockHoldMode);
  ScanTabReq::setReadCommittedFlag(reqInfo, readCommitted);
  req->requestInfo= reqInfo;
}

int
NdbScanOperation::fix_receivers(Uint32 parallel){
  assert(parallel > 0);
  if(parallel > m_allocated_receivers){
    const Uint32 sz = parallel * (4*sizeof(char*)+sizeof(Uint32));

    /* Allocate as Uint64 to ensure proper alignment for pointers. */
    Uint64 * tmp = new Uint64[(sz+7)/8];
    if (tmp == NULL)
    {
      setErrorCodeAbort(4000);
      return -1;
    }
    // Save old receivers
    memcpy(tmp, m_receivers, m_allocated_receivers*sizeof(char*));
    delete[] m_array;
    m_array = (Uint32*)tmp;
    
    m_receivers = (NdbReceiver**)tmp;
    m_api_receivers = m_receivers + parallel;
    m_conf_receivers = m_api_receivers + parallel;
    m_sent_receivers = m_conf_receivers + parallel;
    m_prepared_receivers = (Uint32*)(m_sent_receivers + parallel);

    // Only get/init "new" receivers
    NdbReceiver* tScanRec;
    for (Uint32 i = m_allocated_receivers; i < parallel; i ++) {
      tScanRec = theNdb->getNdbScanRec();
      if (tScanRec == NULL) {
	setErrorCodeAbort(4000);
	return -1;
      }//if
      m_receivers[i] = tScanRec;
      tScanRec->init(NdbReceiver::NDB_SCANRECEIVER, false, this);
    }
    m_allocated_receivers = parallel;
  }
  
  reset_receivers(parallel, 0);
  return 0;
}

/**
 * Move receiver from send array to conf:ed array
 */
void
NdbScanOperation::receiver_delivered(NdbReceiver* tRec){
  if(theError.code == 0){
    if(DEBUG_NEXT_RESULT)
      ndbout_c("receiver_delivered");
    
    Uint32 idx = tRec->m_list_index;
    Uint32 last = m_sent_receivers_count - 1;
    if(idx != last){
      NdbReceiver * move = m_sent_receivers[last];
      m_sent_receivers[idx] = move;
      move->m_list_index = idx;
    }
    m_sent_receivers_count = last;
    
    last = m_conf_receivers_count;
    m_conf_receivers[last] = tRec;
    m_conf_receivers_count = last + 1;
    tRec->m_current_row = 0;
  }
}

/**
 * Remove receiver as it's completed
 */
void
NdbScanOperation::receiver_completed(NdbReceiver* tRec){
  if(theError.code == 0){
    if(DEBUG_NEXT_RESULT)
      ndbout_c("receiver_completed");
    
    Uint32 idx = tRec->m_list_index;
    Uint32 last = m_sent_receivers_count - 1;
    if(idx != last){
      NdbReceiver * move = m_sent_receivers[last];
      m_sent_receivers[idx] = move;
      move->m_list_index = idx;
    }
    m_sent_receivers_count = last;
  }
}

/*****************************************************************************
 * int getFirstATTRINFOScan( U_int32 aData )
 *
 * Return Value:  Return 0:   Successful
 *      	  Return -1:  All other cases
 * Parameters:    None: 	   Only allocate the first signal.
 * Remark:        When a scan is defined we need to use this method instead 
 *                of insertATTRINFO for the first signal. 
 *                This is because we need not to mess up the code in 
 *                insertATTRINFO with if statements since we are not 
 *                interested in the TCKEYREQ signal.
 *****************************************************************************/
int
NdbScanOperation::getFirstATTRINFOScan()
{
  NdbApiSignal* tSignal;

  tSignal = theNdb->getSignal();
  if (tSignal == NULL){
    setErrorCodeAbort(4000);      
    return -1;    
  }
  tSignal->setSignal(m_attrInfoGSN);
  /* The offset 8 is for 3 words of header + 5 words of section sizes. */
  theAI_LenInCurrAI = 8;
  theATTRINFOptr = &tSignal->getDataPtrSend()[8];
  theFirstATTRINFO = tSignal;
  theCurrentATTRINFO = tSignal;
  theCurrentATTRINFO->next(NULL);

  return 0;
}

/**
 * Constats for theTupleKeyDefined[][0]
 */
#define SETBOUND_EQ 1
#define FAKE_PTR 2
#define API_PTR 3

#define WAITFOR_SCAN_TIMEOUT 120000

int
NdbScanOperation::executeCursor(int nodeId){
  NdbTransaction * tCon = theNdbCon;
  TransporterFacade* tp = theNdb->theImpl->m_transporter_facade;
  Guard guard(tp->theMutexPtr);

  Uint32 magic = tCon->theMagicNumber;
  Uint32 seq = tCon->theNodeSequence;

  if (tp->get_node_alive(nodeId) &&
      (tp->getNodeSequence(nodeId) == seq)) {

    /**
     * Only call prepareSendScan first time (incase of restarts)
     *   - check with theMagicNumber
     */
    tCon->theMagicNumber = 0x37412619;
    if(magic != 0x37412619 && 
       prepareSendScan(tCon->theTCConPtr, tCon->theTransactionId) == -1)
      return -1;
    
    
    if (doSendScan(nodeId) == -1)
      return -1;

    m_executed= true; // Mark operation as executed
    return 0;
  } else {
    if (!(tp->get_node_stopping(nodeId) &&
	  (tp->getNodeSequence(nodeId) == seq))){
      TRACE_DEBUG("The node is hard dead when attempting to start a scan");
      setErrorCode(4029);
      tCon->theReleaseOnClose = true;
    } else {
      TRACE_DEBUG("The node is stopping when attempting to start a scan");
      setErrorCode(4030);
    }//if
    tCon->theCommitStatus = NdbTransaction::Aborted;
  }//if
  return -1;
}


int NdbScanOperation::nextResult(bool fetchAllowed, bool forceSend)
{
  int res;

  if (unlikely(m_attribute_record != 0))
  {
    setErrorCodeAbort(4284);
    return -1;
  }

  if ((res = nextResultImpl(fetchAllowed, forceSend)) == 0) {
    // handle blobs
    NdbBlob* tBlob = theBlobList;
    while (tBlob != 0) {
      if (tBlob->atNextResult() == -1)
        return -1;
      tBlob = tBlob->theNext;
    }
    /*
     * Flush blob part ops on behalf of user because
     * - nextResult is analogous to execute(NoCommit)
     * - user is likely to want blob value before next execute
     */
    if (m_transConnection->executePendingBlobOps() == -1)
      return -1;
    return 0;
  }
  return res;
}

/* nextResult() for NdbRecord operation. */
int
NdbScanOperation::nextResult(const char * & out_row,
                             bool fetchAllowed, bool forceSend)
{
  int res;

  if (unlikely(m_attribute_record == 0))
  {
    setErrorCodeAbort(4284);
    return -1;
  }

  if ((res = nextResultNdbRecord(out_row, fetchAllowed, forceSend)) == 0) {
    NdbBlob* tBlob= theBlobList;
    NdbRecAttr *getvalue_recattr= theReceiver.theFirstRecAttr;
    if (unlikely(((UintPtr)tBlob | (UintPtr)getvalue_recattr) != 0))
    {
      Uint32 idx= m_current_api_receiver;
      assert(idx < m_api_receivers_count);
      const NdbReceiver *receiver= m_api_receivers[m_current_api_receiver];
      Uint32 pos= 0;

      /* First take care of any getValue(). */
      while (getvalue_recattr != NULL)
      {
        const char *attr_data;
        Uint32 attr_size;
        if (receiver->getScanAttrData(attr_data, attr_size, pos) == -1)
          return -1;
        if (!getvalue_recattr->receive_data((const Uint32 *)attr_data,
                                            attr_size))
          return -1;                            // purecov: deadcode
        getvalue_recattr= getvalue_recattr->next();
      }

      /* Handle blobs. */
      if (tBlob)
      {
        Uint32 infoword;                          // Not used for blobs
        Uint32 key_length;
        const char *key_data;
        res= receiver->get_keyinfo20(infoword, key_length, key_data);
        if (res == -1)
          return -1;

        do
        {
          if (tBlob->atNextResultNdbRecord(key_data, key_length*4) == -1)
            return -1;
          tBlob= tBlob->theNext;
        } while (tBlob != 0);
        /* Flush blob part ops on behalf of user. */
        if (m_transConnection->executePendingBlobOps() == -1)
          return -1;
      }
    }
    return 0;
  }
  return res;
}

int
NdbScanOperation::nextResultNdbRecord(const char * & out_row,
                                      bool fetchAllowed, bool forceSend)
{
  if (m_ordered)
    return ((NdbIndexScanOperation*)this)->next_result_ordered_ndbrecord
      (out_row, fetchAllowed, forceSend);

  /* Return a row immediately if any is available. */
  while (m_current_api_receiver < m_api_receivers_count)
  {
    NdbReceiver *tRec= m_api_receivers[m_current_api_receiver];
    if (tRec->nextResult())
    {
      out_row= tRec->get_row();
      return 0;
    }
    m_current_api_receiver++;
  }

  if (!fetchAllowed)
  {
    /*
      Application wants to be informed that no more rows are available
      immediately.
    */
    return 2;
  }

  /* Now we have to wait for more rows (or end-of-file on all receivers). */
  Uint32 nodeId = theNdbCon->theDBnode;
  TransporterFacade* tp = theNdb->theImpl->m_transporter_facade;
  int retVal= 2;
  Uint32 idx, last;
  /*
    The rest needs to be done under mutex due to synchronization with receiver
    thread.
  */
  PollGuard poll_guard(tp, &theNdb->theImpl->theWaiter,
                       theNdb->theNdbBlockNumber);

  const Uint32 seq= theNdbCon->theNodeSequence;

  if(theError.code)
  {
    goto err4;
  }

  if(seq == tp->getNodeSequence(nodeId) &&
     send_next_scan(m_current_api_receiver, false) == 0)
  {
    idx= m_current_api_receiver;
    last= m_api_receivers_count;
    Uint32 timeout= tp->m_waitfor_timeout;

    do {
      if (theError.code){
        setErrorCode(theError.code);
        return -1;
      }

      Uint32 cnt= m_conf_receivers_count;
      Uint32 sent= m_sent_receivers_count;

      if (cnt > 0)
      {
        /* New receivers with completed batches available. */
        memcpy(m_api_receivers+last, m_conf_receivers, cnt * sizeof(char*));
        last+= cnt;
        m_conf_receivers_count= 0;
      }
      else if (retVal == 2 && sent > 0)
      {
        /* No completed... */
        int ret_code= poll_guard.wait_scan(3*timeout, nodeId, forceSend);
        if (ret_code == 0 && seq == tp->getNodeSequence(nodeId)) {
          continue;
        } else if(ret_code == -1){
          retVal= -1;
        } else {
          idx= last;
          retVal= -2; //return_code;
        }
      }
      else if (retVal == 2)
      {
        /**
         * No completed & no sent -> EndOfData
         */
        theError.code= -1; // make sure user gets error if he tries again
        return 1;
      }

      if (retVal == 0)
        break;

      while (idx < last)
      {
        NdbReceiver* tRec= m_api_receivers[idx];
        if (tRec->nextResult())
        {
          out_row= tRec->get_row();
          retVal= 0;
          break;
        }
        idx++;
      }
    } while(retVal == 2);
  } else {
    retVal = -3;
  }

  m_api_receivers_count= last;
  m_current_api_receiver= idx;

  switch(retVal)
  {
  case 0:
  case 1:
  case 2:
    return retVal;
  case -1:
    setErrorCode(4008); // Timeout
    break;
  case -2:
    setErrorCode(4028); // Node fail
    break;
  case -3: // send_next_scan -> return fail (set error-code self)
    if(theError.code == 0)
      setErrorCode(4028); // seq changed = Node fail
    break;
  case -4:
err4:
    setErrorCode(theError.code);
    break;
  }

  theNdbCon->theTransactionIsStarted= false;
  theNdbCon->theReleaseOnClose= true;
  return -1;
}

int NdbScanOperation::nextResultImpl(bool fetchAllowed, bool forceSend)
{
  if(m_ordered)
    return ((NdbIndexScanOperation*)this)->next_result_ordered(fetchAllowed,
							       forceSend);
  
  /**
   * Check current receiver
   */
  int retVal = 2;
  Uint32 idx = m_current_api_receiver;
  Uint32 last = m_api_receivers_count;
  m_curr_row = 0;

  if(DEBUG_NEXT_RESULT)
    ndbout_c("nextResult(%d) idx=%d last=%d", fetchAllowed, idx, last);
  
  /**
   * Check next buckets
   */
  for(; idx < last; idx++){
    NdbReceiver* tRec = m_api_receivers[idx];
    if(tRec->nextResult()){
      m_curr_row = tRec->copyout(theReceiver);
      retVal = 0;
      break;
    }
  }
    
  /**
   * We have advanced atleast one bucket
   */
  if(!fetchAllowed || !retVal){
    m_current_api_receiver = idx;
    if(DEBUG_NEXT_RESULT) ndbout_c("return %d", retVal);
    return retVal;
  }
  
  Uint32 nodeId = theNdbCon->theDBnode;
  TransporterFacade* tp = theNdb->theImpl->m_transporter_facade;
  /*
    The PollGuard has an implicit call of unlock_and_signal through the
    ~PollGuard method. This method is called implicitly by the compiler
    in all places where the object is out of context due to a return,
    break, continue or simply end of statement block
  */
  PollGuard poll_guard(tp, &theNdb->theImpl->theWaiter,
                       theNdb->theNdbBlockNumber);

  const Uint32 seq = theNdbCon->theNodeSequence;

  if(theError.code)
  {
    goto err4;
  }
  
  if(seq == tp->getNodeSequence(nodeId) && send_next_scan(idx, false) == 0)
  {
      
    idx = m_current_api_receiver;
    last = m_api_receivers_count;
    Uint32 timeout = tp->m_waitfor_timeout;
      
    do {
      if(theError.code){
	setErrorCode(theError.code);
	if(DEBUG_NEXT_RESULT) ndbout_c("return -1");
	return -1;
      }
      
      Uint32 cnt = m_conf_receivers_count;
      Uint32 sent = m_sent_receivers_count;

      if(DEBUG_NEXT_RESULT)
	ndbout_c("idx=%d last=%d cnt=%d sent=%d", idx, last, cnt, sent);
	
      if(cnt > 0){
	/**
	 * Just move completed receivers
	 */
	memcpy(m_api_receivers+last, m_conf_receivers, cnt * sizeof(char*));
	last += cnt;
	m_conf_receivers_count = 0;
      } else if(retVal == 2 && sent > 0){
	/**
	 * No completed...
	 */
        int ret_code= poll_guard.wait_scan(3*timeout, nodeId, forceSend);
	if (ret_code == 0 && seq == tp->getNodeSequence(nodeId)) {
	  continue;
	} else if(ret_code == -1){
	  retVal = -1;
	} else {
	  idx = last;
	  retVal = -2; //return_code;
	}
      } else if(retVal == 2){
	/**
	 * No completed & no sent -> EndOfData
	 */
	theError.code = -1; // make sure user gets error if he tries again
	if(DEBUG_NEXT_RESULT) ndbout_c("return 1");
	return 1;
      }
	
      if(retVal == 0)
	break;
	
      for(; idx < last; idx++){
	NdbReceiver* tRec = m_api_receivers[idx];
	if(tRec->nextResult()){
	  m_curr_row = tRec->copyout(theReceiver);      
	  retVal = 0;
	  break;
	}
      }
    } while(retVal == 2);
  } else {
    retVal = -3;
  }
    
  m_api_receivers_count = last;
  m_current_api_receiver = idx;
    
  switch(retVal){
  case 0:
  case 1:
  case 2:
    if(DEBUG_NEXT_RESULT) ndbout_c("return %d", retVal);
    return retVal;
  case -1:
    setErrorCode(4008); // Timeout
    break;
  case -2:
    setErrorCode(4028); // Node fail
    break;
  case -3: // send_next_scan -> return fail (set error-code self)
    if(theError.code == 0)
      setErrorCode(4028); // seq changed = Node fail
    break;
  case -4:
err4:
    setErrorCode(theError.code);
    break;
  }
    
  theNdbCon->theTransactionIsStarted = false;
  theNdbCon->theReleaseOnClose = true;
  if(DEBUG_NEXT_RESULT) ndbout_c("return %d", retVal);
  return -1;
}

int
NdbScanOperation::send_next_scan(Uint32 cnt, bool stopScanFlag)
{
  if(cnt > 0){
    NdbApiSignal tSignal(theNdb->theMyRef);
    tSignal.setSignal(GSN_SCAN_NEXTREQ);
    
    Uint32* theData = tSignal.getDataPtrSend();
    theData[0] = theNdbCon->theTCConPtr;
    theData[1] = stopScanFlag == true ? 1 : 0;
    Uint64 transId = theNdbCon->theTransactionId;
    theData[2] = transId;
    theData[3] = (Uint32) (transId >> 32);
    
    /**
     * Prepare ops
     */
    Uint32 last = m_sent_receivers_count;
    Uint32 * prep_array = (cnt > 21 ? m_prepared_receivers : theData + 4);
    Uint32 sent = 0;
    for(Uint32 i = 0; i<cnt; i++){
      NdbReceiver * tRec = m_api_receivers[i];
      if((prep_array[sent] = tRec->m_tcPtrI) != RNIL)
      {
	m_sent_receivers[last+sent] = tRec;
	tRec->m_list_index = last+sent;
	tRec->prepareSend();
	sent++;
      }
    }
    memmove(m_api_receivers, m_api_receivers+cnt, 
	    (theParallelism-cnt) * sizeof(char*));
    
    int ret = 0;
    if(sent)
    {
      Uint32 nodeId = theNdbCon->theDBnode;
      TransporterFacade * tp = theNdb->theImpl->m_transporter_facade;
      if(cnt > 21){
	tSignal.setLength(4);
	LinearSectionPtr ptr[3];
	ptr[0].p = prep_array;
	ptr[0].sz = sent;
	ret = tp->sendSignal(&tSignal, nodeId, ptr, 1);
      } else {
	tSignal.setLength(4+sent);
	ret = tp->sendSignal(&tSignal, nodeId);
      }
    }
    m_sent_receivers_count = last + sent;
    m_api_receivers_count -= cnt;
    m_current_api_receiver = 0;
    
    return ret;
  }
  return 0;
}

int 
NdbScanOperation::prepareSend(Uint32  TC_ConnectPtr, Uint64  TransactionId)
{
  printf("NdbScanOperation::prepareSend\n");
  abort();
  return 0;
}

int 
NdbScanOperation::doSend(int ProcessorId)
{
  printf("NdbScanOperation::doSend\n");
  return 0;
}

void NdbScanOperation::close(bool forceSend, bool releaseOp)
{
  DBUG_ENTER("NdbScanOperation::close");
  DBUG_PRINT("enter", ("this: 0x%lx  tcon: 0x%lx  con: 0x%lx  force: %d  release: %d",
                       (long) this,
                       (long) m_transConnection, (long) theNdbCon,
                       forceSend, releaseOp));

  if(m_transConnection){
    if(DEBUG_NEXT_RESULT)
      ndbout_c("close() theError.code = %d "
	       "m_api_receivers_count = %d "
	       "m_conf_receivers_count = %d "
	       "m_sent_receivers_count = %d",
	       theError.code, 
	       m_api_receivers_count,
	       m_conf_receivers_count,
	       m_sent_receivers_count);
    
    TransporterFacade* tp = theNdb->theImpl->m_transporter_facade;
    /*
      The PollGuard has an implicit call of unlock_and_signal through the
      ~PollGuard method. This method is called implicitly by the compiler
      in all places where the object is out of context due to a return,
      break, continue or simply end of statement block
    */
    PollGuard poll_guard(tp, &theNdb->theImpl->theWaiter,
                         theNdb->theNdbBlockNumber);
    close_impl(tp, forceSend, &poll_guard);
  }

  NdbConnection* tCon = theNdbCon;
  NdbConnection* tTransCon = m_transConnection;
  theNdbCon = NULL;
  m_transConnection = NULL;

  if (tTransCon && releaseOp) 
  {
    NdbIndexScanOperation* tOp = (NdbIndexScanOperation*)this;

    bool ret = true;
    if (theStatus != WaitResponse)
    {
      /**
       * Not executed yet
       */
      ret = 
	tTransCon->releaseScanOperation(&tTransCon->m_theFirstScanOperation,
					&tTransCon->m_theLastScanOperation,
					tOp);
    }
    else
    {
      ret = tTransCon->releaseScanOperation(&tTransCon->m_firstExecutedScanOp,
					    0, tOp);
    }
    assert(ret);
  }
  
  tCon->theScanningOp = 0;
  theNdb->closeTransaction(tCon);
  theNdb->theRemainingStartTransactions--;
  DBUG_VOID_RETURN;
}

void
NdbScanOperation::execCLOSE_SCAN_REP(){
  m_conf_receivers_count = 0;
  m_sent_receivers_count = 0;
}

void NdbScanOperation::release()
{
  if(theNdbCon != 0 || m_transConnection != 0){
    close();
  }
  for(Uint32 i = 0; i<m_allocated_receivers; i++){
    m_receivers[i]->release();
  }
  if (m_scan_buffer)
  {
    delete[] m_scan_buffer;
    m_scan_buffer= NULL;
  }

  NdbOperation::release();
  
  if(theSCAN_TABREQ)
  {
    theNdb->releaseSignal(theSCAN_TABREQ);
    theSCAN_TABREQ = 0;
  }
}

/***************************************************************************
int prepareSendScan(Uint32 aTC_ConnectPtr,
                    Uint64 aTransactionId)

Return Value:   Return 0 : preparation of send was succesful.
                Return -1: In all other case.   
Parameters:     aTC_ConnectPtr: the Connect pointer to TC.
		aTransactionId:	the Transaction identity of the transaction.
Remark:         Puts the the final data into ATTRINFO signal(s)  after this 
                we know the how many signal to send and their sizes
***************************************************************************/
int NdbScanOperation::prepareSendScan(Uint32 aTC_ConnectPtr,
				      Uint64 aTransactionId){
  if (theInterpretIndicator != 1 ||
      (theOperationType != OpenScanRequest &&
       theOperationType != OpenRangeScanRequest)) {
    setErrorCodeAbort(4005);
    return -1;
  }

  theErrorLine = 0;

  // In preapareSendInterpreted we set the sizes (word 4-8) in the
  // first ATTRINFO signal.
  if (prepareSendInterpreted() == -1)
    return -1;

  /*
    When using getValue() in ordered scans, we need to request "behind the
    scenes" any part of the primary key that is not request explicitly by the
    application, so that we will be able to perform the necessary merge sort.

    When using NdbRecord, this is not needed (as the NdbRecord used in ordered
    scans is required to include the full primary key).
  */
  if(!m_attribute_record && m_ordered){
    ((NdbIndexScanOperation*)this)->fix_get_values();
  }
  
  theCurrentATTRINFO->setLength(theAI_LenInCurrAI);

  /**
   * Prepare all receivers
   */
  theReceiver.prepareSend();
  bool keyInfo = m_keyInfo;
  Uint32 key_size= keyInfo ?
    (m_attribute_record ? m_attribute_record->m_keyLenInWords :
                          m_currentTable->m_keyLenInWords) :
    0;
  /**
   * The number of records sent by each LQH is calculated and the kernel
   * is informed of this number by updating the SCAN_TABREQ signal
   */
  ScanTabReq * req = CAST_PTR(ScanTabReq, theSCAN_TABREQ->getDataPtrSend());
  Uint32 batch_size = req->first_batch_size; // User specified
  Uint32 batch_byte_size, first_batch_size;
  theReceiver.calculate_batch_size(key_size,
                                   theParallelism,
                                   batch_size,
                                   batch_byte_size,
                                   first_batch_size,
                                   m_attribute_record);
  ScanTabReq::setScanBatch(req->requestInfo, batch_size);
  req->batch_byte_size= batch_byte_size;
  req->first_batch_size= first_batch_size;

  /**
   * Set keyinfo flag
   *  (Always keyinfo when using blobs)
   */
  Uint32 reqInfo = req->requestInfo;
  ScanTabReq::setKeyinfoFlag(reqInfo, keyInfo);
  ScanTabReq::setNoDiskFlag(reqInfo, m_no_disk_flag);
  req->requestInfo = reqInfo;
  
  if (theStatus == UseNdbRecord)
  {
    Uint32 extra_size= 0;
    if (unlikely(theReceiver.theFirstRecAttr != NULL))
      extra_size= calcGetValueSize();

    assert(theParallelism > 0);
    Uint32 rowsize= m_receivers[0]->ndbrecord_rowsize(m_attribute_record,
                                                      key_size,
                                                      m_read_range_no,
                                                      extra_size);
    Uint32 bufsize= batch_size*rowsize;
    char *buf= new char[bufsize*theParallelism];
    if (!buf)
    {
      setErrorCodeAbort(4000); // "Memory allocation error"
      return -1;
    }
    assert(!m_scan_buffer);
    m_scan_buffer= buf;

    for (Uint32 i = 0; i<theParallelism; i++)
    {
      m_receivers[i]->do_setup_ndbrecord(m_attribute_record, batch_size,
                                         key_size, m_read_range_no,
                                         rowsize, buf,
                                         theReceiver.m_record.m_column_count);
      buf+= bufsize;
    }
  }
  else
  {
    for(Uint32 i = 0; i<theParallelism; i++){
      if (m_receivers[i]->do_get_value(&theReceiver, batch_size, 
                                       key_size, 
                                       m_read_range_no))
      {
        return -1;
      }
    }
  }
  return 0;
}

/*
  Compute extra space needed to buffer getValue() results in NdbRecord
  scans.
 */
Uint32
NdbScanOperation::calcGetValueSize()
{
  Uint32 size= 0;
  const NdbRecAttr *ra= theReceiver.theFirstRecAttr;
  while (ra != NULL)
  {
    size+= sizeof(Uint32) + ra->getColumn()->getSizeInBytes();
    ra= ra->next();
  }
  return size;
}

/*****************************************************************************
int doSend()

Return Value:   Return >0 : send was succesful, returns number of signals sent
                Return -1: In all other case.   
Parameters:     aProcessorId: Receiving processor node
Remark:         Sends the ATTRINFO signal(s)
*****************************************************************************/
int
NdbScanOperation::doSendScan(int aProcessorId)
{
  Uint32 tSignalCount = 0;
  NdbApiSignal* tSignal;
 
  if (theInterpretIndicator != 1 ||
      (theOperationType != OpenScanRequest &&
       theOperationType != OpenRangeScanRequest)) {
      setErrorCodeAbort(4005);
      return -1;
  }
  
  assert(theSCAN_TABREQ != NULL);
  tSignal = theSCAN_TABREQ;
  
  Uint32 tupKeyLen = theTupKeyLen;
  Uint32 aTC_ConnectPtr = theNdbCon->theTCConPtr;
  Uint64 transId = theNdbCon->theTransactionId;
  
  // Update the "attribute info length in words" in SCAN_TABREQ before 
  // sending it. This could not be done in openScan because 
  // we created the ATTRINFO signals after the SCAN_TABREQ signal.
  ScanTabReq * const req = CAST_PTR(ScanTabReq, tSignal->getDataPtrSend());
  if (unlikely(theTotalCurrAI_Len > ScanTabReq::MaxTotalAttrInfo)) {
    setErrorCode(4257);
    return -1;
  }
  req->attrLenKeyLen = (tupKeyLen << 16) | theTotalCurrAI_Len;
  Uint32 tmp = req->requestInfo;
  ScanTabReq::setDistributionKeyFlag(tmp, theDistrKeyIndicator_);
  req->distributionKey = theDistributionKey;
  req->requestInfo = tmp;
  tSignal->setLength(ScanTabReq::StaticLength + theDistrKeyIndicator_);

  TransporterFacade *tp = theNdb->theImpl->m_transporter_facade;
  LinearSectionPtr ptr[3];
  ptr[0].p = m_prepared_receivers;
  ptr[0].sz = theParallelism;
  if (tp->sendSignal(tSignal, aProcessorId, ptr, 1) == -1) {
    setErrorCode(4002);
    return -1;
  } 

  if (tupKeyLen > 0){
    // must have at least one signal since it contains attrLen for bounds
    assert(theLastKEYINFO != NULL);
    tSignal = theLastKEYINFO;
    tSignal->setLength(KeyInfo::HeaderLength + theTotalNrOfKeyWordInSignal);
    
    assert(theSCAN_TABREQ->next() != NULL);
    tSignal = theSCAN_TABREQ->next();
    
    NdbApiSignal* last;
    do {
      KeyInfo * keyInfo = CAST_PTR(KeyInfo, tSignal->getDataPtrSend());
      keyInfo->connectPtr = aTC_ConnectPtr;
      keyInfo->transId[0] = Uint32(transId);
      keyInfo->transId[1] = Uint32(transId >> 32);
      
      if (tp->sendSignal(tSignal,aProcessorId) == -1){
	setErrorCode(4002);
	return -1;
      }
      
      tSignalCount++;
      last = tSignal;
      tSignal = tSignal->next();
    } while(last != theLastKEYINFO);
  }
  
  tSignal = theFirstATTRINFO;
  while (tSignal != NULL) {
    AttrInfo * attrInfo = CAST_PTR(AttrInfo, tSignal->getDataPtrSend());
    attrInfo->connectPtr = aTC_ConnectPtr;
    attrInfo->transId[0] = Uint32(transId);
    attrInfo->transId[1] = Uint32(transId >> 32);
    
    if (tp->sendSignal(tSignal,aProcessorId) == -1){
      setErrorCode(4002);
      return -1;
    }
    tSignalCount++;
    tSignal = tSignal->next();
  }    
  theStatus = WaitResponse;  

  m_curr_row = 0;
  m_sent_receivers_count = theParallelism;
  if(m_ordered)
  {
    m_current_api_receiver = theParallelism;
    m_api_receivers_count = theParallelism;
  }
  
  return tSignalCount;
}//NdbOperation::doSendScan()

/*****************************************************************************
 * NdbOperation* takeOverScanOp(NdbTransaction* updateTrans);
 *
 * Parameters:     The update transactions NdbTransaction pointer.
 * Return Value:   A reference to the transferred operation object 
 *                   or NULL if no success.
 * Remark:         Take over the scanning transactions NdbOperation 
 *                 object for a tuple to an update transaction, 
 *                 which is the last operation read in nextScanResult()
 *		   (theNdbCon->thePreviousScanRec)
 *
 *     FUTURE IMPLEMENTATION:   (This note was moved from header file.)
 *     In the future, it will even be possible to transfer 
 *     to a NdbTransaction on another Ndb-object.  
 *     In this case the receiving NdbTransaction-object must call 
 *     a method receiveOpFromScan to actually receive the information.  
 *     This means that the updating transactions can be placed
 *     in separate threads and thus increasing the parallelism during
 *     the scan process. 
 ****************************************************************************/
int
NdbScanOperation::getKeyFromKEYINFO20(Uint32* data, Uint32 & size)
{
  NdbRecAttr * tRecAttr = m_curr_row;
  if(tRecAttr)
  {
    const Uint32 * src = (Uint32*)tRecAttr->aRef();

    assert(tRecAttr->get_size_in_bytes() > 0);
    assert(tRecAttr->get_size_in_bytes() < 65536);
    const Uint32 len = (tRecAttr->get_size_in_bytes() + 3)/4-1;

    assert(size >= len);
    memcpy(data, src, 4*len);
    size = len;
    return 0;
  }
  return -1;
}

NdbOperation*
NdbScanOperation::takeOverScanOp(OperationType opType, NdbTransaction* pTrans)
{
  if (m_attribute_record)
  {
    setErrorCodeAbort(4284);
    return NULL;
  }

  /*
    Get the first NdbRecAttr object of the row, which contains the 'KeyInfo'
    data from KEYINFO20, with the scanInfo_Node value from KEYINFO20 appended
    after the data.
  */
  NdbRecAttr * tRecAttr = m_curr_row;
  if(tRecAttr)
  {
    if (!m_keyInfo)
    {
      // Cannot take over lock if no keyinfo was requested
      setErrorCodeAbort(4604);
      return NULL;
    }
    NdbOperation * newOp = pTrans->getNdbOperation(m_currentTable);
    if (newOp == NULL){
      return NULL;
    }
    pTrans->theSimpleState = 0;
    
    assert(tRecAttr->get_size_in_bytes() > 0);
    assert(tRecAttr->get_size_in_bytes() < 65536);
    const Uint32 len = (tRecAttr->get_size_in_bytes() + 3)/4-1;
    
    newOp->theTupKeyLen = len;
    newOp->theOperationType = opType;
    newOp->m_abortOption = AbortOnError;
    switch (opType) {
    case (ReadRequest):
      newOp->theLockMode = theLockMode;
      // Fall through
    case (DeleteRequest):
      newOp->theStatus = GetValue;
      break;
    default:
      newOp->theStatus = SetValue;
    }
    const Uint32 * src = (Uint32*)tRecAttr->aRef();
    const Uint32 tScanInfo = src[len] & 0x3FFFF;
    const Uint32 tTakeOverFragment = src[len] >> 20;
    {
      UintR scanInfo = 0;
      TcKeyReq::setTakeOverScanFlag(scanInfo, 1);
      TcKeyReq::setTakeOverScanFragment(scanInfo, tTakeOverFragment);
      TcKeyReq::setTakeOverScanInfo(scanInfo, tScanInfo);
      newOp->theScanInfo = scanInfo;
      newOp->theDistrKeyIndicator_ = 1;
      newOp->theDistributionKey = tTakeOverFragment;
    }

    // Copy the first 8 words of key info from KEYINF20 into TCKEYREQ
    TcKeyReq * tcKeyReq = CAST_PTR(TcKeyReq,newOp->theTCREQ->getDataPtrSend());
    Uint32 i = 0;
    for (i = 0; i < TcKeyReq::MaxKeyInfo && i < len; i++) {
      tcKeyReq->keyInfo[i] = * src++;
    }
    
    if(i < len){
      NdbApiSignal* tSignal = theNdb->getSignal();
      newOp->theTCREQ->next(tSignal); 
      
      Uint32 left = len - i;
      while(tSignal && left > KeyInfo::DataLength){
	tSignal->setSignal(GSN_KEYINFO);
	KeyInfo * keyInfo = CAST_PTR(KeyInfo, tSignal->getDataPtrSend());
	memcpy(keyInfo->keyData, src, 4 * KeyInfo::DataLength);
	src += KeyInfo::DataLength;
	left -= KeyInfo::DataLength;

	tSignal->next(theNdb->getSignal());
	tSignal = tSignal->next();
      }

      if(tSignal && left > 0){
	tSignal->setSignal(GSN_KEYINFO);
	KeyInfo * keyInfo = CAST_PTR(KeyInfo, tSignal->getDataPtrSend());
	memcpy(keyInfo->keyData, src, 4 * left);
      }      
    }
    // create blob handles automatically
    if (opType == DeleteRequest && m_currentTable->m_noOfBlobs != 0) {
      for (unsigned i = 0; i < m_currentTable->m_columns.size(); i++) {
	NdbColumnImpl* c = m_currentTable->m_columns[i];
	assert(c != 0);
	if (c->getBlobType()) {
	  if (newOp->getBlobHandle(pTrans, c) == NULL)
	    return NULL;
	}
      }
    }
    
    return newOp;
  }
  return 0;
}

NdbOperation*
NdbScanOperation::takeOverScanOpNdbRecord(OperationType opType,
                                          NdbTransaction* pTrans,
                                          const NdbRecord *record,
                                          char *row,
                                          const unsigned char *mask)
{
  int res;

  if (!m_attribute_record)
  {
    setErrorCodeAbort(4284);
    return NULL;
  }
  if (!record)
  {
    setErrorCodeAbort(4285);
    return NULL;
  }
  if (!m_keyInfo)
  {
    // Cannot take over lock if no keyinfo was requested
    setErrorCodeAbort(4604);
    return NULL;
  }

  NdbOperation *op= pTrans->getNdbOperation(record->table, NULL, true);
  if (!op)
    return NULL;

  pTrans->theSimpleState= 0;
  op->theStatus= NdbOperation::UseNdbRecord;
  op->theOperationType= opType;
  op->m_abortOption= AbortOnError;
  op->m_key_record= NULL;       // This means m_key_row has KEYINFO20 data
  op->m_attribute_record= record;
  /*
    The m_key_row pointer is only valid until next call of
    nextResult(fetchAllowed=true). But that is ok, since the lock is also
    only valid until that time, so the application must execute() the new
    operation before then.
   */

  /* Now find the current row, and extract keyinfo. */
  Uint32 idx= m_current_api_receiver;
  if (idx >= m_api_receivers_count)
    return NULL;
  const NdbReceiver *receiver= m_api_receivers[m_current_api_receiver];
  Uint32 infoword;
  res= receiver->get_keyinfo20(infoword, op->m_keyinfo_length, op->m_key_row);
  if (res==-1)
    return NULL;
  Uint32 scanInfo= 0;
  TcKeyReq::setTakeOverScanFlag(scanInfo, 1);
  Uint32 fragment= infoword >> 20;
  TcKeyReq::setTakeOverScanFragment(scanInfo, fragment);
  TcKeyReq::setTakeOverScanInfo(scanInfo, infoword & 0x3FFFF);
  op->theScanInfo= scanInfo;
  op->theDistrKeyIndicator_= 1;
  op->theDistributionKey= fragment;

  op->m_attribute_row= row;
  record->copyMask(op->m_read_mask, mask);
  switch (opType)
  {
    case ReadRequest:
      op->theLockMode= theLockMode;
      /*
        Apart from taking over the row lock, we also support reading again,
        though typical usage will probably use an empty mask to read nothing.
      */
      op->theReceiver.getValues(record, row);

      if (unlikely(record->flags & NdbRecord::RecHasBlob))
      {
        if (op->getBlobHandlesNdbRecord(pTrans) == -1)
          return NULL;
      }

      break;
    case UpdateRequest:
      if (unlikely(record->flags & NdbRecord::RecHasBlob))
      {
        if (op->getBlobHandlesNdbRecord(pTrans) == -1)
          return NULL;
      }

      break;
    case DeleteRequest:
      /* Create blob handles if any, to properly delete all blob parts. */
      if (unlikely(record->flags & NdbRecord::RecTableHasBlob))
      {
        if (op->getBlobHandlesDelete(pTrans) == -1)
          return NULL;
      }
      break;
    default:
      assert(false);
      return NULL;
  }

  return op;
}

NdbBlob*
NdbScanOperation::getBlobHandle(const char* anAttrName)
{
  m_keyInfo = 1;
  return NdbOperation::getBlobHandle(m_transConnection, 
				     m_currentTable->getColumn(anAttrName));
}

NdbBlob*
NdbScanOperation::getBlobHandle(Uint32 anAttrId)
{
  m_keyInfo = 1;
  return NdbOperation::getBlobHandle(m_transConnection, 
				     m_currentTable->getColumn(anAttrId));
}

NdbRecAttr*
NdbScanOperation::getValue_NdbRecord_scan(const NdbColumnImpl* attrInfo,
                                          char* aValue)
{
  int res;
  Uint32 ah;
  NdbRecAttr *ra;
  AttributeHeader::init(&ah, attrInfo->m_attrId, 0);
  res= insertATTRINFO(ah);
  if (res==-1)
    return NULL;
  theInitialReadSize= theTotalCurrAI_Len - 5;
  ra= theReceiver.getValue(attrInfo, aValue);
  if (!ra)
  {
    setErrorCodeAbort(4000);
    return NULL;
  }
  theErrorLine++;
  return ra;
}

NdbRecAttr*
NdbScanOperation::getValue_impl(const NdbColumnImpl *attrInfo, char *aValue)
{
  if (theStatus == UseNdbRecord)
    return getValue_NdbRecord_scan(attrInfo, aValue);
  else
    return NdbOperation::getValue_impl(attrInfo, aValue);
}

NdbIndexScanOperation::NdbIndexScanOperation(Ndb* aNdb)
  : NdbScanOperation(aNdb, NdbOperation::OrderedIndexScan)
{
}

NdbIndexScanOperation::~NdbIndexScanOperation(){
}

int
NdbIndexScanOperation::setBound(const char* anAttrName, int type, 
				const void* aValue)
{
  return setBound(m_accessTable->getColumn(anAttrName), type, aValue);
}

int
NdbIndexScanOperation::setBound(Uint32 anAttrId, int type, 
				const void* aValue)
{
  return setBound(m_accessTable->getColumn(anAttrId), type, aValue);
}

int
NdbIndexScanOperation::equal_impl(const NdbColumnImpl* anAttrObject, 
				  const char* aValue)
{
  return setBound(anAttrObject, BoundEQ, aValue);
}

NdbRecAttr*
NdbIndexScanOperation::getValue_impl(const NdbColumnImpl* attrInfo, 
				     char* aValue){
  if (theStatus == UseNdbRecord)
    return getValue_NdbRecord_scan(attrInfo, aValue);

  if(!m_ordered){
    return NdbScanOperation::getValue_impl(attrInfo, aValue);
  }
  
  int id = attrInfo->m_attrId;                       // In "real" table
  assert(m_accessTable->m_index);
  int sz = (int)m_accessTable->m_index->m_key_ids.size();
  // first check handles pseudo-column
  if(id >= sz || (id = m_accessTable->m_index->m_key_ids[id]) == -1){
    return NdbScanOperation::getValue_impl(attrInfo, aValue);
  }
  
  assert(id < NDB_MAX_NO_OF_ATTRIBUTES_IN_KEY);
  Uint32 marker = theTupleKeyDefined[id][0];
  
  if(marker == SETBOUND_EQ){
    return NdbScanOperation::getValue_impl(attrInfo, aValue);
  } else if(marker == API_PTR){
    return NdbScanOperation::getValue_impl(attrInfo, aValue);
  }
  
  assert(marker == FAKE_PTR);
  
  UintPtr oldVal;
  oldVal = theTupleKeyDefined[id][1];
#if (SIZEOF_CHARP == 8)
  oldVal = oldVal | (((UintPtr)theTupleKeyDefined[id][2]) << 32);
#endif
  theTupleKeyDefined[id][0] = API_PTR;

  NdbRecAttr* tmp = (NdbRecAttr*)oldVal;
  tmp->setup(attrInfo, aValue);

  return tmp;
}

#include <AttributeHeader.hpp>
/*
 * Define bound on index column in range scan.
 */
int
NdbIndexScanOperation::setBound(const NdbColumnImpl* tAttrInfo, 
				int type, const void* aValue)
{
  if (!tAttrInfo)
  {
    setErrorCodeAbort(4318);    // Invalid attribute
    return -1;
  }
  if (theOperationType == OpenRangeScanRequest &&
      (0 <= type && type <= 4)) {
    // insert bound type
    Uint32 currLen = theTotalNrOfKeyWordInSignal;
    Uint32 remaining = KeyInfo::DataLength - currLen;
    bool tDistrKey = tAttrInfo->m_distributionKey;

    Uint32 len = 0;
    if (aValue != NULL)
      if (! tAttrInfo->get_var_length(aValue, len)) {
        setErrorCodeAbort(4209);
        return -1;
      }

    // insert attribute header
    Uint32 tIndexAttrId = tAttrInfo->m_attrId;
    Uint32 sizeInWords = (len + 3) / 4;
    AttributeHeader ah(tIndexAttrId, sizeInWords << 2);
    const Uint32 ahValue = ah.m_value;

    const Uint32 align = (UintPtr(aValue) & 7);
    const bool aligned = (tDistrKey && type == BoundEQ) ? 
      (align == 0) : (align & 3) == 0;

    /*
      The nobytes flag is false if there are extra padding bytes at the end,
      which we need to zero out.
    */
    const bool nobytes = (len & 0x3) == 0;
    const Uint32 totalLen = 2 + sizeInWords;
    Uint32 tupKeyLen = theTupKeyLen;
    union {
      Uint32 tempData[2000];
      Uint64 __my_align;
    };
    Uint64 *valPtr;
    if(remaining > totalLen && aligned && nobytes){
      Uint32 * dst = theKEYINFOptr + currLen;
      * dst ++ = type;
      * dst ++ = ahValue;
      memcpy(dst, aValue, 4 * sizeInWords);
      theTotalNrOfKeyWordInSignal = currLen + totalLen;
      valPtr = (Uint64*)aValue;
    } else {
      if(!aligned || !nobytes){
	tempData[0] = type;
	tempData[1] = ahValue;
	tempData[2 + (len >> 2)] = 0;
        memcpy(tempData+2, aValue, len);
	insertBOUNDS(tempData, 2+sizeInWords);
	valPtr = (Uint64*)(tempData+2);
      } else {
	Uint32 buf[2] = { type, ahValue };
	insertBOUNDS(buf, 2);
	insertBOUNDS((Uint32*)aValue, sizeInWords);
	valPtr = (Uint64*)aValue;
      }
    }
    theTupKeyLen = tupKeyLen + totalLen;

    /**
     * Do sorted stuff
     */

    /**
     * The primary keys for an ordered index is defined in the beginning
     * so it's safe to use [tIndexAttrId] 
     * (instead of looping as in NdbOperation::equal_impl)
     */
    if(type == BoundEQ && tDistrKey && !m_multi_range)
    {
      theNoOfTupKeyLeft--;
      return handle_distribution_key(tAttrInfo, valPtr, len);
    }
    return 0;
  } else {
    setErrorCodeAbort(4228);    // XXX wrong code
    return -1;
  }
}

int
NdbIndexScanOperation::insertBOUNDS(Uint32 * data, Uint32 sz){
  Uint32 len;
  Uint32 remaining = KeyInfo::DataLength - theTotalNrOfKeyWordInSignal;
  Uint32 * dst = theKEYINFOptr + theTotalNrOfKeyWordInSignal;
  do {
    len = (sz < remaining ? sz : remaining);
    memcpy(dst, data, 4 * len);
    
    if(sz >= remaining){
      NdbApiSignal* tCurr = theLastKEYINFO;
      tCurr->setLength(KeyInfo::MaxSignalLength);
      NdbApiSignal* tSignal = tCurr->next();
      if(tSignal)
	;
      else if((tSignal = theNdb->getSignal()) != 0)
      {
	tCurr->next(tSignal);
	tSignal->setSignal(GSN_KEYINFO);
      } else {
	goto error;
      }
      theLastKEYINFO = tSignal;
      theKEYINFOptr = dst = ((KeyInfo*)tSignal->getDataPtrSend())->keyData;
      remaining = KeyInfo::DataLength;
      sz -= len;
      data += len;
    } else {
      len = (KeyInfo::DataLength - remaining) + len;
      break;
    }
  } while(true);   
  theTotalNrOfKeyWordInSignal = len;
  return 0;

error:
  setErrorCodeAbort(4228);    // XXX wrong code
  return -1;
}

int
NdbIndexScanOperation::ndbrecord_insert_bound(const NdbRecord *key_record,
                                              Uint32 column_index,
                                              const char *row,
                                              Uint32 bound_type)
{
  char buf[256];
  Uint32 currLen= theTotalNrOfKeyWordInSignal;
  Uint32 remaining= KeyInfo::DataLength - currLen;
  const NdbRecord::Attr *column= &key_record->columns[column_index];

  bool is_null= column->is_null(row);
  Uint32 len= 0;
  const void *aValue= row+column->offset;

  if (!is_null)
  {
    bool len_ok;
    /* Support for special mysqld varchar format in keys. */
    if (column->flags & NdbRecord::IsMysqldShrinkVarchar)
    {
      len_ok= column->shrink_varchar(row, len, buf);
      aValue= buf;
    }
    else
    {
      len_ok= column->get_var_length(row, len);
    }
    if (!len_ok) {
      setErrorCodeAbort(4209);
      return -1;
    }
  }

  /* Insert attribute header. */
  Uint32 tIndexAttrId= column->index_attrId;
  Uint32 sizeInWords= (len + 3) / 4;
  AttributeHeader ah(tIndexAttrId, sizeInWords << 2);
  const Uint32 ahValue= ah.m_value;
  const bool aligned= (UintPtr(aValue) & 3) == 0;

  /*
    The nobytes flag is false if there are extra padding bytes at the end,
    which we need to zero out.
  */
  const bool nobytes= (len & 0x3) == 0;
  const Uint32 totalLen= 2 + sizeInWords;
  Uint32 tupKeyLen= theTupKeyLen;
  if (remaining > totalLen && aligned && nobytes){
    Uint32 * dst= theKEYINFOptr + currLen;
    * dst ++ = bound_type;
    * dst ++ = ahValue;
    memcpy(dst, aValue, 4 * sizeInWords);
    theTotalNrOfKeyWordInSignal= currLen + totalLen;
  } else {
    if(!aligned || !nobytes){
      Uint32 tempData[2000];
      if (len > sizeof(tempData))
        len= sizeof(tempData);
      tempData[0] = bound_type;
      tempData[1] = ahValue;
      tempData[2 + (len >> 2)] = 0;
      memcpy(tempData+2, aValue, len);
      insertBOUNDS(tempData, 2+sizeInWords);
    } else {
      Uint32 buf[2] = { bound_type, ahValue };
      insertBOUNDS(buf, 2);
      insertBOUNDS((Uint32*)aValue, sizeInWords);
    }
  }
  theTupKeyLen= tupKeyLen + totalLen;

  return 0;
}

Uint32
NdbIndexScanOperation::getKeyFromSCANTABREQ(Uint32* data, Uint32 size)
{
  DBUG_ENTER("NdbIndexScanOperation::getKeyFromSCANTABREQ");
  assert(size >= theTotalNrOfKeyWordInSignal);
  size = theTotalNrOfKeyWordInSignal;
  NdbApiSignal* tSignal = theSCAN_TABREQ->next();
  Uint32 pos = 0;
  while (pos < size) {
    assert(tSignal != NULL);
    Uint32* tData = tSignal->getDataPtrSend();
    Uint32 rem = size - pos;
    if (rem > KeyInfo::DataLength)
      rem = KeyInfo::DataLength;
    Uint32 i = 0;
    while (i < rem) {
      data[pos + i] = tData[KeyInfo::HeaderLength + i];
      i++;
    }
    pos += rem;
  }
  DBUG_DUMP("key", (uchar*) data, size << 2);
  DBUG_RETURN(size);
}

int
NdbIndexScanOperation::readTuples(LockMode lm,
				  Uint32 scan_flags,
				  Uint32 parallel,
				  Uint32 batch)
{
  const bool order_by = scan_flags & SF_OrderBy;
  const bool order_desc = scan_flags & SF_Descending;
  const bool read_range_no = scan_flags & SF_ReadRangeNo;
  m_multi_range = scan_flags & SF_MultiRange;

  int res = NdbScanOperation::readTuples(lm, scan_flags, parallel, batch);
  if(!res && read_range_no)
  {
    m_read_range_no = 1;
    Uint32 word = 0;
    AttributeHeader::init(&word, AttributeHeader::RANGE_NO, 0);
    if(insertATTRINFO(word) == -1)
      res = -1;
  }
  if(!res && order_by){
    m_ordered = true;
    if (order_desc) {
      m_descending = true;
      ScanTabReq * req = CAST_PTR(ScanTabReq, theSCAN_TABREQ->getDataPtrSend());
      ScanTabReq::setDescendingFlag(req->requestInfo, true);
    }
    Uint32 cnt = m_accessTable->getNoOfColumns() - 1;
    m_sort_columns = cnt; // -1 for NDB$NODE
    m_current_api_receiver = m_sent_receivers_count;
    m_api_receivers_count = m_sent_receivers_count;
    
    if (!m_attribute_record)
    {
      for(Uint32 i = 0; i<cnt; i++){
        const NdbColumnImpl* key = m_accessTable->m_index->m_columns[i];
        const NdbColumnImpl* col = m_currentTable->getColumn(key->m_keyInfoPos);
        NdbRecAttr* tmp = NdbScanOperation::getValue_impl(col, (char*)-1);
        UintPtr newVal = UintPtr(tmp);
        theTupleKeyDefined[i][0] = FAKE_PTR;
        theTupleKeyDefined[i][1] = (newVal & 0xFFFFFFFF);
#if (SIZEOF_CHARP == 8)
        theTupleKeyDefined[i][2] = (newVal >> 32);
#endif
      }
    }
  }
  m_this_bound_start = 0;
  m_first_bound_word = theKEYINFOptr;
  
  return res;
}

void
NdbIndexScanOperation::fix_get_values(){
  /**
   * Loop through all getValues and set buffer pointer to "API" pointer
   */
  NdbRecAttr * curr = theReceiver.theFirstRecAttr;
  Uint32 cnt = m_accessTable->getNoOfColumns() - 1;
  assert(cnt <  NDB_MAX_NO_OF_ATTRIBUTES_IN_KEY);
  
  for(Uint32 i = 0; i<cnt; i++){
    Uint32 val = theTupleKeyDefined[i][0];
    switch(val){
    case FAKE_PTR:
      curr->setup(curr->m_column, 0);
    case API_PTR:
      curr = curr->next();
      break;
    case SETBOUND_EQ:
      break;
#ifdef VM_TRACE
    default:
      abort();
#endif
    }
  }
}

int
NdbIndexScanOperation::compare(Uint32 skip, Uint32 cols, 
			       const NdbReceiver* t1, 
			       const NdbReceiver* t2){

  NdbRecAttr * r1 = t1->m_rows[t1->m_current_row];
  NdbRecAttr * r2 = t2->m_rows[t2->m_current_row];

  r1 = (skip ? r1->next() : r1);
  r2 = (skip ? r2->next() : r2);
  const int jdir = 1 - 2 * (int)m_descending;
  assert(jdir == 1 || jdir == -1);
  while(cols > 0){
    Uint32 * d1 = (Uint32*)r1->aRef();
    Uint32 * d2 = (Uint32*)r2->aRef();
    unsigned r1_null = r1->isNULL();
    if((r1_null ^ (unsigned)r2->isNULL())){
      return (r1_null ? -1 : 1) * jdir;
    }
    const NdbColumnImpl & col = NdbColumnImpl::getImpl(* r1->m_column);
    Uint32 len1 = r1->get_size_in_bytes();
    Uint32 len2 = r2->get_size_in_bytes();
    if(!r1_null){
      const NdbSqlUtil::Type& sqlType = NdbSqlUtil::getType(col.m_type);
      int r = (*sqlType.m_cmp)(col.m_cs, d1, len1, d2, len2, true);
      if(r){
	assert(r != NdbSqlUtil::CmpUnknown);
	return r * jdir;
      }
    }
    cols--;
    r1 = r1->next();
    r2 = r2->next();
  }
  return 0;
}

int
NdbIndexScanOperation::compare_ndbrecord(const NdbReceiver *r1,
                                         const NdbReceiver *r2) const
{
  Uint32 i;
  int jdir= 1 - 2 * (int)m_descending;
  const NdbRecord *record= m_attribute_record;

  assert(record->flags & NdbRecord::RecHasAllKeys);
  assert(jdir == 1 || jdir == -1);

  const char *a_row= r1->peek_row();
  const char *b_row= r2->peek_row();

  /* First compare range_no if needed. */
  if (m_read_range_no)
  {
    Uint32 a_range_no= uint4korr(a_row+record->m_row_size);
    Uint32 b_range_no= uint4korr(b_row+record->m_row_size);
   if (a_range_no != b_range_no)
      return (a_range_no < b_range_no ? -1 : 1);
  }

  for (i= 0; i<record->key_index_length; i++)
  {
    const NdbRecord::Attr *col= &record->columns[record->key_indexes[i]];

    bool a_is_null= col->is_null(a_row);
    bool b_is_null= col->is_null(b_row);
    if (a_is_null)
    {
      if (!b_is_null)
        return -1 * jdir;
    }
    else
    {
      if (b_is_null)
        return 1 * jdir;

      Uint32 offset= col->offset;
      Uint32 maxSize= col->maxSize;
      const char *a_ptr= a_row + offset;
      const char *b_ptr= b_row + offset;
      void *info= col->charset_info;
      int res=
        (*col->compare_function)(info, a_ptr, maxSize, b_ptr, maxSize, true);
      if (res)
      {
        assert(res != NdbSqlUtil::CmpUnknown);
        return res * jdir;
      }
    }
  }

  return 0;
}

/*
  This function does the merge-sort of the parallel ordered index scans, needed
  to return a single sorted stream of rows to the application.

  Before returning a row, it needs to ensure that at least one row is available
  from all fragments (except fragments whose scan has finished), so that it is
  possible to know which one to return the next row from.
*/
int
NdbIndexScanOperation::next_result_ordered(bool fetchAllowed,
					   bool forceSend){
  
  m_curr_row = 0;
  Uint32 u_idx = 0, u_last = 0;
  Uint32 s_idx   = m_current_api_receiver; // first sorted
  Uint32 s_last  = theParallelism;         // last sorted

  NdbReceiver** arr = m_api_receivers;
  /*
    We might actually be reading off the end of the m_api_receivers array here,
    in the case where s_idx==s_last. But that does not hurt, as the
    m_conf_receivers array follows m_api_receivers (we won't read off the end
    of malloc()), and we won't use the value in this case.
  */
  NdbReceiver* tRec = arr[s_idx];
  
  if(DEBUG_NEXT_RESULT) ndbout_c("nextOrderedResult(%d) nextResult: %d",
				 fetchAllowed, 
				 (s_idx < s_last ? tRec->nextResult() : 0));
  
  if(DEBUG_NEXT_RESULT) ndbout_c("u=[%d %d] s=[%d %d]", 
				 u_idx, u_last,
				 s_idx, s_last);
  
  /*
    The entries in m_api_receivers below m_current_api_receiver are fragment
    scans that have finished, and the ones above we previously ensured have
    at least one row available. So we only need to check the fragment from
    which we last returned a row for having more rows available.

    There is a special case m_api_receivers==theParallelism for when
    nextResult() is called for the first time, in which case we need to fetch
    and sort receivers for all fragments.
  */
  bool fetchNeeded = (s_idx == s_last) || !tRec->nextResult();
  
  if(fetchNeeded){
    if(fetchAllowed){
      /*
        We wait for all receivers to have received the next batch of rows (in
        the receiver thread), and transfer them to the m_api_receivers array
        under mutex protection.
      */
      if(DEBUG_NEXT_RESULT) ndbout_c("performing fetch...");
      TransporterFacade* tp = theNdb->theImpl->m_transporter_facade;
      /*
        The PollGuard has an implicit call of unlock_and_signal through the
        ~PollGuard method. This method is called implicitly by the compiler
        in all places where the object is out of context due to a return,
        break, continue or simply end of statement block
      */
      PollGuard poll_guard(tp, &theNdb->theImpl->theWaiter,
                           theNdb->theNdbBlockNumber);
      if(theError.code)
	return -1;
      Uint32 seq = theNdbCon->theNodeSequence;
      Uint32 nodeId = theNdbCon->theDBnode;
      Uint32 timeout = tp->m_waitfor_timeout;
      if(seq == tp->getNodeSequence(nodeId) &&
	 !send_next_scan_ordered(s_idx)){
	Uint32 tmp = m_sent_receivers_count;
	s_idx = m_current_api_receiver; 
	while(m_sent_receivers_count > 0 && !theError.code){
          int ret_code= poll_guard.wait_scan(3*timeout, nodeId, forceSend);
	  if (ret_code == 0 && seq == tp->getNodeSequence(nodeId)) {
	    continue;
	  }
	  if(DEBUG_NEXT_RESULT) ndbout_c("return -1");
	  if(ret_code == -1){
	    setErrorCode(4008);
	  } else {
	    setErrorCode(4028);
	  }
	  return -1;
	}
	
	if(theError.code){
	  setErrorCode(theError.code);
	  if(DEBUG_NEXT_RESULT) ndbout_c("return -1");
	  return -1;
	}
	
	u_idx = 0;
	u_last = m_conf_receivers_count;
	m_conf_receivers_count = 0;
	memcpy(arr, m_conf_receivers, u_last * sizeof(char*));
	
	if(DEBUG_NEXT_RESULT) ndbout_c("sent: %d recv: %d", tmp, u_last);
      } else {
	setErrorCode(4028);
	return -1;
      }
    } else {
      if(DEBUG_NEXT_RESULT) ndbout_c("return 2");
      return 2;
    }
  } else {
    u_idx = s_idx;
    u_last = s_idx + 1;
    s_idx++;
  }
  
  if(DEBUG_NEXT_RESULT) ndbout_c("u=[%d %d] s=[%d %d]", 
				 u_idx, u_last,
				 s_idx, s_last);

  /*
    At this point, u_idx points to the first entry in m_api_receivers array
    that needs sorting, and u_last points to just after the last entry to be
    sorted. s_idx points to the first sorted entry.

    We now move the entries u_idx..(u_last-1) into the correct position in the
    sorted part of the m_api_receivers array (after s_idx), and return the
    first row of the first receiver to the application.
  */

  Uint32 cols = m_sort_columns + m_read_range_no;
  Uint32 skip = m_keyInfo;      // Skip keyinfo from sort (if present)
  while(u_idx < u_last){
    u_last--;
    tRec = arr[u_last];
    
    /* ToDo: Do binary search instead to find place. */
    Uint32 place = s_idx;
    for(; place < s_last; place++){
      if(compare(skip, cols, tRec, arr[place]) <= 0){
	break;
      }
    }
    
    if(place != s_idx){
      if(DEBUG_NEXT_RESULT) 
	ndbout_c("memmove(%d, %d, %d)", s_idx-1, s_idx, (place - s_idx));
      memmove(arr+s_idx-1, arr+s_idx, sizeof(arr[0])*(place - s_idx));
    }
    
    if(DEBUG_NEXT_RESULT) ndbout_c("putting %d @ %d", u_last, place - 1);
    m_api_receivers[place-1] = tRec;
    s_idx--;
  }

  if(DEBUG_NEXT_RESULT) ndbout_c("u=[%d %d] s=[%d %d]", 
				 u_idx, u_last,
				 s_idx, s_last);
  
  m_current_api_receiver = s_idx;
  
  if(DEBUG_NEXT_RESULT)
    for(Uint32 i = s_idx; i<s_last; i++)
      ndbout_c("%p", arr[i]);
  
  tRec = m_api_receivers[s_idx];    
  if(s_idx < s_last && tRec->nextResult()){
    m_curr_row = tRec->copyout(theReceiver);      
    if(DEBUG_NEXT_RESULT) ndbout_c("return 0");
    return 0;
  }

  theError.code = -1;
  if(DEBUG_NEXT_RESULT) ndbout_c("return 1");
  return 1;
}

/* NdbRecord version of next_result_ordered. */
int
NdbIndexScanOperation::next_result_ordered_ndbrecord(const char * & out_row,
                                                     bool fetchAllowed,
                                                     bool forceSend)
{
  Uint32 current;

  /*
    Retrieve more rows if necessary, then sort the array of receivers.

    The special case m_current_api_receiver==theParallelism is for the
    initial call, where we need to wait for and sort all receviers.
  */
  if (m_current_api_receiver==theParallelism ||
      !m_api_receivers[m_current_api_receiver]->nextResult())
  {
    if (!fetchAllowed)
      return 2;                                 // No more data available now

    /* Wait for all receivers to be retrieved. */
    int count= ordered_send_scan_wait_for_all(forceSend);
    if (count == -1)
      return -1;

    /*
      Insert all newly retrieved receivers in sorted array.
      The receivers are left in m_conf_receivers for us to move into place.
    */
    current= m_current_api_receiver;
    for (int i= 0; i < count; i++)
      ordered_insert_receiver(current--, m_conf_receivers[i]);
    m_current_api_receiver= current;
  }
  else
  {
    /*
      Just make sure the first receiver (from which we just returned a row, so
      it may no longer be in the correct sort position) is placed correctly.
    */
    current= m_current_api_receiver;
    ordered_insert_receiver(current + 1, m_api_receivers[current]);
  }

  /* Now just return the next row (if any). */
  if (current < theParallelism && m_api_receivers[current]->nextResult())
  {
    out_row=  m_api_receivers[current]->get_row();
    return 0;
  }
  else
  {
    theError.code= -1;
    return 1;                                   // End-of-file
  }
}

/* Insert a newly fully-retrieved receiver in the correct sorted place. */
void
NdbIndexScanOperation::ordered_insert_receiver(Uint32 start,
                                               NdbReceiver *receiver)
{
  /*
    Binary search to find the position of the first receiver with no rows
    smaller than the first row for this receiver. We need to insert this
    receiver just before that position.
  */
  Uint32 first= start;
  Uint32 last= theParallelism;
  while (first < last)
  {
    Uint32 idx= (first+last)/2;
    int res= compare_ndbrecord(receiver, m_api_receivers[idx]);
    if (res <= 0)
      last= idx;
    else
      first= idx+1;
  }

  /* Move down any receivers that go before this one, then insert it. */
  if (last > start)
    memmove(&m_api_receivers[start-1],
            &m_api_receivers[start],
            (last - start) * sizeof(m_api_receivers[0]));
  m_api_receivers[last-1]= receiver;
}

/*
  This method is called during (NdbRecord) ordered index scans when all rows
  from one batch of one fragment scan are exhausted (identified by
  m_current_api_receiver).

  It sends a SCAN_NEXTREQ signal for the fragment and waits for the batch to
  be fully received.

  As a special case, it is also called at the start of the scan. In this case,
  no signal is sent, it just waits for the initial batch to be fully received
  from all fragments.

  The method returns -1 for error, and otherwise the number of fragments that
  were received (this will be 0 or 1, except for the initial call where it
  will be equal to theParallelism).

  The NdbReceiver object(s) are left in the m_conf_receivers array. Note that
  it is safe to read from m_conf_receivers without mutex protection immediately
  after return from this method; as all fragments are fully received no new
  receivers can enter that array until the next call to this method.
*/
int
NdbIndexScanOperation::ordered_send_scan_wait_for_all(bool forceSend)
{
  TransporterFacade* tp= theNdb->theImpl->m_transporter_facade;

  PollGuard poll_guard(tp, &theNdb->theImpl->theWaiter,
                       theNdb->theNdbBlockNumber);
  if(theError.code)
    return -1;

  Uint32 seq= theNdbCon->theNodeSequence;
  Uint32 nodeId= theNdbCon->theDBnode;
  Uint32 timeout= tp->m_waitfor_timeout;
  if (seq == tp->getNodeSequence(nodeId) &&
      !send_next_scan_ordered(m_current_api_receiver))
  {
    while (m_sent_receivers_count > 0 && !theError.code)
    {
      int ret_code= poll_guard.wait_scan(3*timeout, nodeId, forceSend);
      if (ret_code == 0 && seq == tp->getNodeSequence(nodeId))
        continue;
      if(ret_code == -1){
        setErrorCode(4008);
      } else {
        setErrorCode(4028);
      }
      return -1;
    }

    if(theError.code){
      setErrorCode(theError.code);
      return -1;
    }

    Uint32 new_receivers= m_conf_receivers_count;
    m_conf_receivers_count= 0;
    assert(new_receivers<=1 || new_receivers==theParallelism);
    return new_receivers;
  } else {
    setErrorCode(4028);
    return -1;
  }
}

/*
  This method is used in ordered index scan to acknowledge the reception of
  one batch of fragment scan rows and request the sending of another batch (it
  sends a SCAN_NEXTREQ signal with one scan fragment record pointer).

  It is called with the argument IDX set to the value of
  m_current_api_receiver, the receiver for the fragment scan to acknowledge.
  This receiver is moved from the m_api_receivers array to the
  m_sent_receivers array.

  This method is called with the PollGuard mutex held on the transporter.
*/
int
NdbIndexScanOperation::send_next_scan_ordered(Uint32 idx)
{
  if(idx == theParallelism)
    return 0;
  
  NdbReceiver* tRec = m_api_receivers[idx];
  NdbApiSignal tSignal(theNdb->theMyRef);
  tSignal.setSignal(GSN_SCAN_NEXTREQ);
  
  Uint32 last = m_sent_receivers_count;
  Uint32* theData = tSignal.getDataPtrSend();
  Uint32* prep_array = theData + 4;
  
  m_current_api_receiver = idx + 1;
  if((prep_array[0] = tRec->m_tcPtrI) == RNIL)
  {
    if(DEBUG_NEXT_RESULT)
      ndbout_c("receiver completed, don't send");
    return 0;
  }
  
  theData[0] = theNdbCon->theTCConPtr;
  theData[1] = 0;
  Uint64 transId = theNdbCon->theTransactionId;
  theData[2] = transId;
  theData[3] = (Uint32) (transId >> 32);
  
  /**
   * Prepare ops
   */
  m_sent_receivers[last] = tRec;
  tRec->m_list_index = last;
  tRec->prepareSend();
  m_sent_receivers_count = last + 1;
  
  Uint32 nodeId = theNdbCon->theDBnode;
  TransporterFacade * tp = theNdb->theImpl->m_transporter_facade;
  tSignal.setLength(4+1);
  int ret= tp->sendSignal(&tSignal, nodeId);
  return ret;
}

int
NdbScanOperation::close_impl(TransporterFacade* tp, bool forceSend,
                             PollGuard *poll_guard)
{
  Uint32 seq = theNdbCon->theNodeSequence;
  Uint32 nodeId = theNdbCon->theDBnode;
  
  if(seq != tp->getNodeSequence(nodeId))
  {
    theNdbCon->theReleaseOnClose = true;
    return -1;
  }
  
  Uint32 timeout = tp->m_waitfor_timeout;
  /**
   * Wait for outstanding
   */
  while(theError.code == 0 && m_sent_receivers_count)
  {
    int return_code= poll_guard->wait_scan(3*timeout, nodeId, forceSend);
    switch(return_code){
    case 0:
      break;
    case -1:
      setErrorCode(4008);
    case -2:
      m_api_receivers_count = 0;
      m_conf_receivers_count = 0;
      m_sent_receivers_count = 0;
      theNdbCon->theReleaseOnClose = true;
      return -1;
    }
  }

  if(theError.code)
  {
    m_api_receivers_count = 0;
    m_current_api_receiver = m_ordered ? theParallelism : 0;
  }


  /**
   * move all conf'ed into api
   *   so that send_next_scan can check if they needs to be closed
   */
  Uint32 api = m_api_receivers_count;
  Uint32 conf = m_conf_receivers_count;

  if(m_ordered)
  {
    /**
     * Ordered scan, keep the m_api_receivers "to the right"
     */
    memmove(m_api_receivers, m_api_receivers+m_current_api_receiver, 
	    (theParallelism - m_current_api_receiver) * sizeof(char*));
    api = (theParallelism - m_current_api_receiver);
    m_api_receivers_count = api;
  }
  
  if(DEBUG_NEXT_RESULT)
    ndbout_c("close_impl: [order api conf sent curr parr] %d %d %d %d %d %d",
	     m_ordered, api, conf, 
	     m_sent_receivers_count, m_current_api_receiver, theParallelism);
  
  if(api+conf)
  {
    /**
     * There's something to close
     *   setup m_api_receivers (for send_next_scan)
     */
    memcpy(m_api_receivers+api, m_conf_receivers, conf * sizeof(char*));
    m_api_receivers_count = api + conf;
    m_conf_receivers_count = 0;
  }
  
  // Send close scan
  if(send_next_scan(api+conf, true) == -1)
  {
    theNdbCon->theReleaseOnClose = true;
    return -1;
  }
  
  /**
   * wait for close scan conf
   */
  while(m_sent_receivers_count+m_api_receivers_count+m_conf_receivers_count)
  {
    int return_code= poll_guard->wait_scan(3*timeout, nodeId, forceSend);
    switch(return_code){
    case 0:
      break;
    case -1:
      setErrorCode(4008);
    case -2:
      m_api_receivers_count = 0;
      m_conf_receivers_count = 0;
      m_sent_receivers_count = 0;
      theNdbCon->theReleaseOnClose = true;
      return -1;
    }
  }
  
  return 0;
}

void
NdbScanOperation::reset_receivers(Uint32 parallell, Uint32 ordered){
  for(Uint32 i = 0; i<parallell; i++){
    m_receivers[i]->m_list_index = i;
    m_prepared_receivers[i] = m_receivers[i]->getId();
    m_sent_receivers[i] = m_receivers[i];
    m_conf_receivers[i] = 0;
    m_api_receivers[i] = 0;
    m_receivers[i]->prepareSend();
  }
  
  m_api_receivers_count = 0;
  m_current_api_receiver = 0;
  m_sent_receivers_count = 0;
  m_conf_receivers_count = 0;
}

int
NdbScanOperation::restart(bool forceSend)
{
  
  TransporterFacade* tp = theNdb->theImpl->m_transporter_facade;
  /*
    The PollGuard has an implicit call of unlock_and_signal through the
    ~PollGuard method. This method is called implicitly by the compiler
    in all places where the object is out of context due to a return,
    break, continue or simply end of statement block
  */
  PollGuard poll_guard(tp, &theNdb->theImpl->theWaiter,
                       theNdb->theNdbBlockNumber);
  Uint32 nodeId = theNdbCon->theDBnode;
  
  {
    int res;
    if((res= close_impl(tp, forceSend, &poll_guard)))
    {
      return res;
    }
  }
  
  /**
   * Reset receivers
   */
  reset_receivers(theParallelism, m_ordered);
  
  theError.code = 0;
  if (doSendScan(nodeId) == -1)
    return -1;
  return 0;
}

int
NdbIndexScanOperation::reset_bounds(bool forceSend){
  int res;
  
  {
    TransporterFacade* tp = theNdb->theImpl->m_transporter_facade;
    /*
      The PollGuard has an implicit call of unlock_and_signal through the
      ~PollGuard method. This method is called implicitly by the compiler
      in all places where the object is out of context due to a return,
      break, continue or simply end of statement block
    */
    PollGuard poll_guard(tp, &theNdb->theImpl->theWaiter,
                         theNdb->theNdbBlockNumber);
    res= close_impl(tp, forceSend, &poll_guard);
  }

  if(!res)
  {
    theError.code = 0;
    reset_receivers(theParallelism, m_ordered);
    
    theLastKEYINFO = theSCAN_TABREQ->next();
    theKEYINFOptr = ((KeyInfo*)theLastKEYINFO->getDataPtrSend())->keyData;
    theTupKeyLen = 0;
    theTotalNrOfKeyWordInSignal = 0;
    theNoOfTupKeyLeft = m_accessTable->m_noOfDistributionKeys;
    theDistrKeyIndicator_ = 0;
    m_this_bound_start = 0;
    m_first_bound_word = theKEYINFOptr;
    m_transConnection
      ->remove_list((NdbOperation*&)m_transConnection->m_firstExecutedScanOp,
		    this);
    m_transConnection->define_scan_op(this);
    return 0;
  }
  return res;
}

int
NdbIndexScanOperation::end_of_bound(Uint32 no)
{
  DBUG_ENTER("end_of_bound");
  DBUG_PRINT("info", ("Range number %u", no));
  /* Check that SF_MultiRange has been specified if more
     than one range is specified */
  if (no > 0 && !m_multi_range)
    DBUG_RETURN(-1);
  if(no < (1 << 12)) // Only 12-bits no of ranges
  {
    Uint32 bound_head = * m_first_bound_word;
    bound_head |= (theTupKeyLen - m_this_bound_start) << 16 | (no << 4);
    * m_first_bound_word = bound_head;
    
    m_first_bound_word = theKEYINFOptr + theTotalNrOfKeyWordInSignal;
    m_this_bound_start = theTupKeyLen;
    DBUG_RETURN(0);
  }
  DBUG_RETURN(-1);
}

int
NdbIndexScanOperation::get_range_no()
{
  if (m_attribute_record)
  {
    Uint32 idx= m_current_api_receiver;
    if (idx >= m_api_receivers_count)
      return -1;

    const NdbReceiver *tRec= m_api_receivers[m_current_api_receiver];
    return tRec->get_range_no();
  }

  NdbRecAttr* tRecAttr = m_curr_row;
  if(m_read_range_no && tRecAttr)
  {
    /* Need to skip first keyinfo NdbRecAttr entry, if present. */
    if(m_keyInfo)
      tRecAttr = tRecAttr->next();
    Uint32 ret = *(Uint32*)tRecAttr->aRef();
    return ret;
  }
  return -1;
}

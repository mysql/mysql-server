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
#include <Ndb.hpp>
#include <NdbScanOperation.hpp>
#include <NdbIndexScanOperation.hpp>
#include <NdbConnection.hpp>
#include <NdbResultSet.hpp>
#include "NdbApiSignal.hpp"
#include <NdbOut.hpp>
#include "NdbDictionaryImpl.hpp"

#include <NdbRecAttr.hpp>
#include <NdbReceiver.hpp>

#include <stdlib.h>
#include <NdbSqlUtil.hpp>

#include <signaldata/ScanTab.hpp>
#include <signaldata/KeyInfo.hpp>
#include <signaldata/AttrInfo.hpp>
#include <signaldata/TcKeyReq.hpp>

NdbScanOperation::NdbScanOperation(Ndb* aNdb) :
  NdbOperation(aNdb),
  m_resultSet(0),
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
}

NdbScanOperation::~NdbScanOperation()
{
  for(Uint32 i = 0; i<m_allocated_receivers; i++){
    m_receivers[i]->release();
    theNdb->releaseNdbScanRec(m_receivers[i]);
  }
  delete[] m_array;
  if (m_resultSet)
    delete m_resultSet;
}

NdbResultSet* 
NdbScanOperation::getResultSet()
{
  if (!m_resultSet)
    m_resultSet = new NdbResultSet(this);

  return m_resultSet;
}



void
NdbScanOperation::setErrorCode(int aErrorCode){
  NdbConnection* tmp = theNdbCon;
  theNdbCon = m_transConnection;
  NdbOperation::setErrorCode(aErrorCode);
  theNdbCon = tmp;
}

void
NdbScanOperation::setErrorCodeAbort(int aErrorCode){
  NdbConnection* tmp = theNdbCon;
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
NdbScanOperation::init(const NdbTableImpl* tab, NdbConnection* myConnection)
{
  m_transConnection = myConnection;
  //NdbConnection* aScanConnection = theNdb->startTransaction(myConnection);
  NdbConnection* aScanConnection = theNdb->hupp(myConnection);
  if (!aScanConnection){
    setErrorCodeAbort(theNdb->getNdbError().code);
    return -1;
  }

  // NOTE! The hupped trans becomes the owner of the operation
  if(NdbOperation::init(tab, aScanConnection) != 0){
    return -1;
  }
  
  initInterpreter();
  
  theStatus = GetValue;
  theOperationType = OpenScanRequest;
  theNdbCon->theMagicNumber = 0xFE11DF;

  return 0;
}

NdbResultSet* NdbScanOperation::readTuples(NdbScanOperation::LockMode lm,
					   Uint32 batch, 
					   Uint32 parallel)
{
  m_ordered = 0;

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
    return 0;
  }

  theNdbCon->theScanningOp = this;
  theLockMode = lm;

  bool lockExcl, lockHoldMode, readCommitted;
  switch(lm){
  case NdbScanOperation::LM_Read:
    lockExcl = false;
    lockHoldMode = true;
    readCommitted = false;
    break;
  case NdbScanOperation::LM_Exclusive:
    lockExcl = true;
    lockHoldMode = true;
    readCommitted = false;
    break;
  case NdbScanOperation::LM_CommittedRead:
    lockExcl = false;
    lockHoldMode = false;
    readCommitted = true;
    break;
  default:
    setErrorCode(4003);
    return 0;
  }

  m_keyInfo = lockExcl ? 1 : 0;

  bool range = false;
  if (m_accessTable->m_indexType == NdbDictionary::Index::OrderedIndex ||
      m_accessTable->m_indexType == NdbDictionary::Index::UniqueOrderedIndex){
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
    range = true;
  }
  
  theParallelism = parallel;

  if(fix_receivers(parallel) == -1){
    setErrorCodeAbort(4000);
    return 0;
  }
  
  theSCAN_TABREQ = (!theSCAN_TABREQ ? theNdb->getSignal() : theSCAN_TABREQ);
  if (theSCAN_TABREQ == NULL) {
    setErrorCodeAbort(4000);
    return 0;
  }//if
  
  ScanTabReq * req = CAST_PTR(ScanTabReq, theSCAN_TABREQ->getDataPtrSend());
  req->apiConnectPtr = theNdbCon->theTCConPtr;
  req->tableId = m_accessTable->m_tableId;
  req->tableSchemaVersion = m_accessTable->m_version;
  req->storedProcId = 0xFFFF;
  req->buddyConPtr = theNdbCon->theBuddyConPtr;
  
  Uint32 reqInfo = 0;
  ScanTabReq::setParallelism(reqInfo, parallel);
  ScanTabReq::setScanBatch(reqInfo, 0);
  ScanTabReq::setLockMode(reqInfo, lockExcl);
  ScanTabReq::setHoldLockFlag(reqInfo, lockHoldMode);
  ScanTabReq::setReadCommittedFlag(reqInfo, readCommitted);
  ScanTabReq::setRangeScanFlag(reqInfo, range);
  req->requestInfo = reqInfo;

  Uint64 transId = theNdbCon->getTransactionId();
  req->transId1 = (Uint32) transId;
  req->transId2 = (Uint32) (transId >> 32);

  NdbApiSignal* tSignal = 
    theFirstKEYINFO;

  theFirstKEYINFO = (tSignal ? tSignal : tSignal = theNdb->getSignal());
  theLastKEYINFO = tSignal;
  
  tSignal->setSignal(GSN_KEYINFO);
  theKEYINFOptr = ((KeyInfo*)tSignal->getDataPtrSend())->keyData;
  theTotalNrOfKeyWordInSignal= 0;

  getFirstATTRINFOScan();
  return getResultSet();
}

int
NdbScanOperation::fix_receivers(Uint32 parallel){
  assert(parallel > 0);
  if(parallel > m_allocated_receivers){
    const Uint32 sz = parallel * (4*sizeof(char*)+sizeof(Uint32));

    Uint32 * tmp = new Uint32[(sz+3)/4];
    // Save old receivers
    memcpy(tmp+parallel, m_receivers, m_allocated_receivers*sizeof(char*));
    delete[] m_array;
    m_array = tmp;
    
    m_prepared_receivers = tmp;
    m_receivers = (NdbReceiver**)(tmp + parallel);
    m_api_receivers = m_receivers + parallel;
    m_conf_receivers = m_api_receivers + parallel;
    m_sent_receivers = m_conf_receivers + parallel;

    // Only get/init "new" receivers
    NdbReceiver* tScanRec;
    for (Uint32 i = m_allocated_receivers; i < parallel; i ++) {
      tScanRec = theNdb->getNdbScanRec();
      if (tScanRec == NULL) {
	setErrorCodeAbort(4000);
	return -1;
      }//if
      m_receivers[i] = tScanRec;
      tScanRec->init(NdbReceiver::NDB_SCANRECEIVER, this);
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
    tRec->m_list_index = last;
    tRec->m_current_row = 0;
  }
}

/**
 * Remove receiver as it's completed
 */
void
NdbScanOperation::receiver_completed(NdbReceiver* tRec){
  if(theError.code == 0){
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


/*
 * After setBound() are done, move the accumulated ATTRINFO signals to
 * a separate list.  Then continue with normal scan.
 */
#if 0
int
NdbIndexScanOperation::saveBoundATTRINFO()
{
  theCurrentATTRINFO->setLength(theAI_LenInCurrAI);
  theBoundATTRINFO = theFirstATTRINFO;
  theTotalBoundAI_Len = theTotalCurrAI_Len;
  theTotalCurrAI_Len = 5;
  theBoundATTRINFO->setData(theTotalBoundAI_Len, 4);
  theBoundATTRINFO->setData(0, 5);
  theBoundATTRINFO->setData(0, 6);
  theBoundATTRINFO->setData(0, 7);
  theBoundATTRINFO->setData(0, 8);
  theStatus = GetValue;

  int res = getFirstATTRINFOScan();

  /**
   * Define each key with getValue (if ordered)
   *   unless the one's with EqBound
   */
  if(!res && m_ordered){

    /**
     * If setBound EQ
     */
    Uint32 i = 0;
    while(theTupleKeyDefined[i][0] == SETBOUND_EQ)
      i++;
    
    
    Uint32 cnt = m_accessTable->getNoOfColumns() - 1;
    m_sort_columns = cnt - i;
    for(; i<cnt; i++){
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
  return res;
}
#endif

#define WAITFOR_SCAN_TIMEOUT 120000

int
NdbScanOperation::executeCursor(int nodeId){
  NdbConnection * tCon = theNdbCon;
  TransporterFacade* tp = TransporterFacade::instance();
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
    tCon->theCommitStatus = NdbConnection::Aborted;
  }//if
  return -1;
}

#define DEBUG_NEXT_RESULT 0

int NdbScanOperation::nextResult(bool fetchAllowed)
{
  if(m_ordered)
    return ((NdbIndexScanOperation*)this)->next_result_ordered(fetchAllowed);
  
  /**
   * Check current receiver
   */
  int retVal = 2;
  Uint32 idx = m_current_api_receiver;
  Uint32 last = m_api_receivers_count;

  if(DEBUG_NEXT_RESULT)
    ndbout_c("nextResult(%d) idx=%d last=%d", fetchAllowed, idx, last);
  
  /**
   * Check next buckets
   */
  for(; idx < last; idx++){
    NdbReceiver* tRec = m_api_receivers[idx];
    if(tRec->nextResult()){
      tRec->copyout(theReceiver);      
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
  TransporterFacade* tp = TransporterFacade::instance();
  Guard guard(tp->theMutexPtr);
  Uint32 seq = theNdbCon->theNodeSequence;
  if(seq == tp->getNodeSequence(nodeId) && send_next_scan(idx, false) == 0){
      
    idx = m_current_api_receiver;
    last = m_api_receivers_count;
      
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
	theNdb->theWaiter.m_node = nodeId;
	theNdb->theWaiter.m_state = WAIT_SCAN;
	int return_code = theNdb->receiveResponse(WAITFOR_SCAN_TIMEOUT);
	if (return_code == 0 && seq == tp->getNodeSequence(nodeId)) {
	  continue;
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
	  tRec->copyout(theReceiver);      
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
    break;
  }
    
  theNdbCon->theTransactionIsStarted = false;
  theNdbCon->theReleaseOnClose = true;
  if(DEBUG_NEXT_RESULT) ndbout_c("return -1", retVal);
  return -1;
}

int
NdbScanOperation::send_next_scan(Uint32 cnt, bool stopScanFlag){  
  if(cnt > 0 || stopScanFlag){
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
    for(Uint32 i = 0; i<cnt; i++){
      NdbReceiver * tRec = m_api_receivers[i];
      m_sent_receivers[last+i] = tRec;
      tRec->m_list_index = last+i;
      prep_array[i] = tRec->m_tcPtrI;
      tRec->prepareSend();
    }
    memcpy(&m_api_receivers[0], &m_api_receivers[cnt], cnt * sizeof(char*));
    
    Uint32 nodeId = theNdbCon->theDBnode;
    TransporterFacade * tp = TransporterFacade::instance();
    int ret;
    if(cnt > 21){
      tSignal.setLength(4);
      LinearSectionPtr ptr[3];
      ptr[0].p = prep_array;
      ptr[0].sz = cnt;
      ret = tp->sendFragmentedSignal(&tSignal, nodeId, ptr, 1);
    } else {
      tSignal.setLength(4+cnt);
      ret = tp->sendSignal(&tSignal, nodeId);
    }

    m_sent_receivers_count = last + cnt + stopScanFlag;
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

void NdbScanOperation::closeScan()
{
  if(m_transConnection){
    if(DEBUG_NEXT_RESULT)
      ndbout_c("closeScan() theError.code = %d "
	       "m_api_receivers_count = %d "
	       "m_conf_receivers_count = %d "
	       "m_sent_receivers_count = %d",
	       theError.code, 
	       m_api_receivers_count,
	       m_conf_receivers_count,
	       m_sent_receivers_count);
    
    TransporterFacade* tp = TransporterFacade::instance();
    Guard guard(tp->theMutexPtr);
    close_impl(tp);
    
  } while(0);
  
  theNdbCon->theScanningOp = 0;
  theNdb->closeTransaction(theNdbCon);
  
  theNdbCon = 0;
  m_transConnection = NULL;
}

void
NdbScanOperation::execCLOSE_SCAN_REP(){
  m_api_receivers_count = 0;
  m_conf_receivers_count = 0;
  m_sent_receivers_count = 0;
}

void NdbScanOperation::release()
{
  if(theNdbCon != 0 || m_transConnection != 0){
    closeScan();
  }
  for(Uint32 i = 0; i<m_allocated_receivers; i++){
    m_receivers[i]->release();
  }
  if(theSCAN_TABREQ)
  {
    theNdb->releaseSignal(theSCAN_TABREQ);
    theSCAN_TABREQ = 0;
  }
  NdbOperation::release();
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
  
  if(m_ordered){
    ((NdbIndexScanOperation*)this)->fix_get_values();
  }
  
  theCurrentATTRINFO->setLength(theAI_LenInCurrAI);

  /**
   * Prepare all receivers
   */
  theReceiver.prepareSend();
  bool keyInfo = m_keyInfo;
  Uint32 key_size = keyInfo ? m_currentTable->m_keyLenInWords : 0;
  /**
   * The number of records sent by each LQH is calculated and the kernel
   * is informed of this number by updating the SCAN_TABREQ signal
   */
  Uint32 batch_size, batch_byte_size, first_batch_size;
  theReceiver.calculate_batch_size(key_size,
                                   theParallelism,
                                   batch_size,
                                   batch_byte_size,
                                   first_batch_size);
  ScanTabReq * req = CAST_PTR(ScanTabReq, theSCAN_TABREQ->getDataPtrSend());
  ScanTabReq::setScanBatch(req->requestInfo, batch_size);
  req->batch_byte_size= batch_byte_size;
  req->first_batch_size= first_batch_size;

  /**
   * Set keyinfo flag
   *  (Always keyinfo when using blobs)
   */
  Uint32 reqInfo = req->requestInfo;
  ScanTabReq::setKeyinfoFlag(reqInfo, keyInfo);
  req->requestInfo = reqInfo;
  
  for(Uint32 i = 0; i<theParallelism; i++){
    m_receivers[i]->do_get_value(&theReceiver, batch_size, key_size);
  }
  return 0;
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
  if (tSignal->setSignal(GSN_SCAN_TABREQ) == -1) {
    setErrorCode(4001);
    return -1;
  }
  
  Uint32 tupKeyLen = theTupKeyLen;
  Uint32 len = theTotalNrOfKeyWordInSignal;
  Uint32 aTC_ConnectPtr = theNdbCon->theTCConPtr;
  Uint64 transId = theNdbCon->theTransactionId;
  
  // Update the "attribute info length in words" in SCAN_TABREQ before 
  // sending it. This could not be done in openScan because 
  // we created the ATTRINFO signals after the SCAN_TABREQ signal.
  ScanTabReq * const req = CAST_PTR(ScanTabReq, tSignal->getDataPtrSend());
  req->attrLenKeyLen = (tupKeyLen << 16) | theTotalCurrAI_Len;
  
  TransporterFacade *tp = TransporterFacade::instance();
  LinearSectionPtr ptr[3];
  ptr[0].p = m_prepared_receivers;
  ptr[0].sz = theParallelism;
  if (tp->sendFragmentedSignal(tSignal, aProcessorId, ptr, 1) == -1) {
    setErrorCode(4002);
    return -1;
  } 

  if (tupKeyLen > 0){
    // must have at least one signal since it contains attrLen for bounds
    assert(theLastKEYINFO != NULL);
    tSignal = theLastKEYINFO;
    tSignal->setLength(KeyInfo::HeaderLength + theTotalNrOfKeyWordInSignal);
    
    assert(theFirstKEYINFO != NULL);
    tSignal = theFirstKEYINFO;
    
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
  return tSignalCount;
}//NdbOperation::doSendScan()

/*****************************************************************************
 * NdbOperation* takeOverScanOp(NdbConnection* updateTrans);
 *
 * Parameters:     The update transactions NdbConnection pointer.
 * Return Value:   A reference to the transferred operation object 
 *                   or NULL if no success.
 * Remark:         Take over the scanning transactions NdbOperation 
 *                 object for a tuple to an update transaction, 
 *                 which is the last operation read in nextScanResult()
 *		   (theNdbCon->thePreviousScanRec)
 *
 *     FUTURE IMPLEMENTATION:   (This note was moved from header file.)
 *     In the future, it will even be possible to transfer 
 *     to a NdbConnection on another Ndb-object.  
 *     In this case the receiving NdbConnection-object must call 
 *     a method receiveOpFromScan to actually receive the information.  
 *     This means that the updating transactions can be placed
 *     in separate threads and thus increasing the parallelism during
 *     the scan process. 
 ****************************************************************************/
int
NdbScanOperation::getKeyFromKEYINFO20(Uint32* data, unsigned size)
{
  Uint32 idx = m_current_api_receiver;
  Uint32 last = m_api_receivers_count;

  Uint32 row;
  NdbReceiver * tRec;
  NdbRecAttr * tRecAttr;
  if(idx < last && (tRec = m_api_receivers[idx]) 
     && ((row = tRec->m_current_row) <= tRec->m_defined_rows)
     && (tRecAttr = tRec->m_rows[row-1])){

    const Uint32 * src = (Uint32*)tRecAttr->aRef();
    memcpy(data, src, 4*size);
    return 0;
  }
  return -1;
}

NdbOperation*
NdbScanOperation::takeOverScanOp(OperationType opType, NdbConnection* pTrans){
  
  Uint32 idx = m_current_api_receiver;
  Uint32 last = m_api_receivers_count;

  Uint32 row;
  NdbReceiver * tRec;
  NdbRecAttr * tRecAttr;
  if(idx < last && (tRec = m_api_receivers[idx]) 
     && ((row = tRec->m_current_row) <= tRec->m_defined_rows)
     && (tRecAttr = tRec->m_rows[row-1])){
    
    NdbOperation * newOp = pTrans->getNdbOperation(m_currentTable);
    if (newOp == NULL){
      return NULL;
    }
    pTrans->theSimpleState = 0;
    
    const Uint32 len = (tRecAttr->attrSize() * tRecAttr->arraySize() + 3)/4-1;

    newOp->theTupKeyLen = len;
    newOp->theOperationType = opType;
    if (opType == DeleteRequest) {
      newOp->theStatus = GetValue;  
    } else {
      newOp->theStatus = SetValue;  
    }
    
    const Uint32 * src = (Uint32*)tRecAttr->aRef();
    const Uint32 tScanInfo = src[len] & 0x3FFFF;
    const Uint32 tTakeOverNode = src[len] >> 20;
    {
      UintR scanInfo = 0;
      TcKeyReq::setTakeOverScanFlag(scanInfo, 1);
      TcKeyReq::setTakeOverScanNode(scanInfo, tTakeOverNode);
      TcKeyReq::setTakeOverScanInfo(scanInfo, tScanInfo);
      newOp->theScanInfo = scanInfo;
    }

    // Copy the first 8 words of key info from KEYINF20 into TCKEYREQ
    TcKeyReq * tcKeyReq = CAST_PTR(TcKeyReq,newOp->theTCREQ->getDataPtrSend());
    Uint32 i = 0;
    for (i = 0; i < TcKeyReq::MaxKeyInfo && i < len; i++) {
      tcKeyReq->keyInfo[i] = * src++;
    }
    
    if(i < len){
      NdbApiSignal* tSignal = theNdb->getSignal();
      newOp->theFirstKEYINFO = tSignal;      
      
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

NdbIndexScanOperation::NdbIndexScanOperation(Ndb* aNdb)
  : NdbScanOperation(aNdb)
{
}

NdbIndexScanOperation::~NdbIndexScanOperation(){
}

int
NdbIndexScanOperation::setBound(const char* anAttrName, int type, 
				const void* aValue, Uint32 len)
{
  return setBound(m_accessTable->getColumn(anAttrName), type, aValue, len);
}

int
NdbIndexScanOperation::setBound(Uint32 anAttrId, int type, 
				const void* aValue, Uint32 len)
{
  return setBound(m_accessTable->getColumn(anAttrId), type, aValue, len);
}

int
NdbIndexScanOperation::equal_impl(const NdbColumnImpl* anAttrObject, 
				  const char* aValue, 
				  Uint32 len){
  return setBound(anAttrObject, BoundEQ, aValue, len);
}

NdbRecAttr*
NdbIndexScanOperation::getValue_impl(const NdbColumnImpl* attrInfo, 
				     char* aValue){
  if(!m_ordered){
    return NdbScanOperation::getValue_impl(attrInfo, aValue);
  }
  
  int id = attrInfo->m_attrId;                // In "real" table
  assert(m_accessTable->m_index);
  int sz = (int)m_accessTable->m_index->m_key_ids.size();
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
				int type, const void* aValue, Uint32 len)
{
  if (theOperationType == OpenRangeScanRequest &&
      (0 <= type && type <= 4) &&
      len <= 8000) {
    // insert bound type
    Uint32 currLen = theTotalNrOfKeyWordInSignal;
    Uint32 remaining = KeyInfo::DataLength - currLen;
    Uint32 sizeInBytes = tAttrInfo->m_attrSize * tAttrInfo->m_arraySize;

    // normalize char bound
    CHARSET_INFO* cs = tAttrInfo->m_cs;
    Uint32 xfrmData[2000];
    if (cs != NULL && aValue != NULL) {
      // current limitation: strxfrm does not increase length
      assert(cs->strxfrm_multiply == 1);
      unsigned n =
      (*cs->coll->strnxfrm)(cs,
                            (uchar*)xfrmData, sizeof(xfrmData),
                            (const uchar*)aValue, sizeInBytes);
      while (n < sizeInBytes)
        ((uchar*)xfrmData)[n++] = 0x20;
      aValue = (char*)xfrmData;
    }
    if (len != sizeInBytes && (len != 0)) {
      setErrorCodeAbort(4209);
      return -1;
    }
    // insert attribute header
    len = aValue != NULL ? sizeInBytes : 0;
    Uint32 tIndexAttrId = tAttrInfo->m_attrId;
    Uint32 sizeInWords = (len + 3) / 4;
    AttributeHeader ah(tIndexAttrId, sizeInWords);
    const Uint32 ahValue = ah.m_value;

    const bool aligned = (UintPtr(aValue) & 3) == 0;
    const bool nobytes = (len & 0x3) == 0;
    const Uint32 totalLen = 2 + sizeInWords;
    Uint32 tupKeyLen = theTupKeyLen;
    if(remaining > totalLen &&  aligned && nobytes){
      Uint32 * dst = theKEYINFOptr + currLen;
      * dst ++ = type;
      * dst ++ = ahValue;
      memcpy(dst, aValue, 4 * sizeInWords);
      theTotalNrOfKeyWordInSignal = currLen + totalLen;
    } else {
      if(!aligned || !nobytes){
	Uint32 tempData[2002];
	tempData[0] = type;
	tempData[1] = ahValue;
        memcpy(tempData+2, aValue, len);
        while ((len & 0x3) != 0)
          ((char*)&tempData[2])[len++] = 0;
	insertBOUNDS(tempData, 2+sizeInWords);
      } else {
	Uint32 buf[2] = { type, ahValue };
	insertBOUNDS(buf, 2);
	insertBOUNDS((Uint32*)aValue, sizeInWords);
      }
    }
    theTupKeyLen = tupKeyLen + totalLen;

    /**
     * Do sorted stuff
     */

    /**
     * The primary keys for an ordered index is defined in the beginning
     * so it's safe to use [tIndexAttrId] 
     * (instead of looping as is NdbOperation::equal_impl)
     */
    if(type == BoundEQ && !theTupleKeyDefined[tIndexAttrId][0]){
      theNoOfTupKeyDefined++;
      theTupleKeyDefined[tIndexAttrId][0] = SETBOUND_EQ;
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
  } while(sz >= 0);   
  theTotalNrOfKeyWordInSignal = len;
  return 0;

error:
  setErrorCodeAbort(4228);    // XXX wrong code
  return -1;
}

NdbResultSet*
NdbIndexScanOperation::readTuples(LockMode lm,
				  Uint32 batch,
				  Uint32 parallel,
				  bool order_by){
  NdbResultSet * rs = NdbScanOperation::readTuples(lm, batch, 0);
  if(rs && order_by){
    m_ordered = 1;
    Uint32 cnt = m_accessTable->getNoOfColumns() - 1;
    m_sort_columns = cnt; // -1 for NDB$NODE
    m_current_api_receiver = m_sent_receivers_count;
    m_api_receivers_count = m_sent_receivers_count;
    
    m_sort_columns = cnt;
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
  return rs;
}

void
NdbIndexScanOperation::fix_get_values(){
  /**
   * Loop through all getValues and set buffer pointer to "API" pointer
   */
  NdbRecAttr * curr = theReceiver.theFirstRecAttr;
  Uint32 cnt = m_accessTable->getNoOfColumns() - 1;
  assert(cnt <  NDB_MAX_NO_OF_ATTRIBUTES_IN_KEY);
  
  const NdbIndexImpl * idx = m_accessTable->m_index;
  const NdbTableImpl * tab = m_currentTable;
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
  
  while(cols > 0){
    Uint32 * d1 = (Uint32*)r1->aRef();
    Uint32 * d2 = (Uint32*)r2->aRef();
    unsigned r1_null = r1->isNULL();
    if((r1_null ^ (unsigned)r2->isNULL())){
      return (r1_null ? -1 : 1);
    }
    const NdbColumnImpl & col = NdbColumnImpl::getImpl(* r1->m_column);
    Uint32 size = (r1->theAttrSize * r1->theArraySize + 3) / 4;
    if(!r1_null){
      const NdbSqlUtil::Type& sqlType = NdbSqlUtil::getType(col.m_extType);
      int r = (*sqlType.m_cmp)(col.m_cs, d1, d2, size, size);
      if(r){
	assert(r != NdbSqlUtil::CmpUnknown);
	return r;
      }
    }
    cols--;
    r1 = r1->next();
    r2 = r2->next();
  }
  return 0;
}

int
NdbIndexScanOperation::next_result_ordered(bool fetchAllowed){
  
  Uint32 u_idx = 0, u_last = 0;
  Uint32 s_idx   = m_current_api_receiver; // first sorted
  Uint32 s_last  = theParallelism;         // last sorted

  NdbReceiver** arr = m_api_receivers;
  NdbReceiver* tRec = arr[s_idx];
  
  if(DEBUG_NEXT_RESULT) ndbout_c("nextOrderedResult(%d) nextResult: %d",
				 fetchAllowed, 
				 (s_idx < s_last ? tRec->nextResult() : 0));
  
  if(DEBUG_NEXT_RESULT) ndbout_c("u=[%d %d] s=[%d %d]", 
				 u_idx, u_last,
				 s_idx, s_last);
  
  bool fetchNeeded = (s_idx == s_last) || !tRec->nextResult();
  
  if(fetchNeeded){
    if(fetchAllowed){
      if(DEBUG_NEXT_RESULT) ndbout_c("performing fetch...");
      TransporterFacade* tp = TransporterFacade::instance();
      Guard guard(tp->theMutexPtr);
      Uint32 seq = theNdbCon->theNodeSequence;
      Uint32 nodeId = theNdbCon->theDBnode;
      if(seq == tp->getNodeSequence(nodeId) && !send_next_scan_ordered(s_idx)){
	Uint32 tmp = m_sent_receivers_count;
	s_idx = m_current_api_receiver; 
	while(m_sent_receivers_count > 0 && !theError.code){
	  theNdb->theWaiter.m_node = nodeId;
	  theNdb->theWaiter.m_state = WAIT_SCAN;
	  int return_code = theNdb->receiveResponse(WAITFOR_SCAN_TIMEOUT);
	  if (return_code == 0 && seq == tp->getNodeSequence(nodeId)) {
	    continue;
	  }
	  if(DEBUG_NEXT_RESULT) ndbout_c("return -1");
	  return -1;
	}
	
	u_idx = 0;
	u_last = m_conf_receivers_count;
	m_conf_receivers_count = 0;
	memcpy(arr, m_conf_receivers, u_last * sizeof(char*));
	
	if(DEBUG_NEXT_RESULT) ndbout_c("sent: %d recv: %d", tmp, u_last);
	if(theError.code){
	  setErrorCode(theError.code);
	  if(DEBUG_NEXT_RESULT) ndbout_c("return -1");
	  return -1;
	}
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


  Uint32 cols = m_sort_columns;
  Uint32 skip = m_keyInfo;
  while(u_idx < u_last){
    u_last--;
    tRec = arr[u_last];
    
    // Do binary search instead to find place
    Uint32 place = s_idx;
    for(; place < s_last; place++){
      if(compare(skip, cols, tRec, arr[place]) <= 0){
	break;
      }
    }
    
    if(place != s_idx){
      if(DEBUG_NEXT_RESULT) 
	ndbout_c("memmove(%d, %d, %d)", s_idx-1, s_idx, (place - s_idx));
      memmove(arr+s_idx-1, arr+s_idx, sizeof(char*)*(place - s_idx));
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
    tRec->copyout(theReceiver);      
    if(DEBUG_NEXT_RESULT) ndbout_c("return 0");
    return 0;
  }

  theError.code = -1;
  if(DEBUG_NEXT_RESULT) ndbout_c("return 1");
  return 1;
}

int
NdbIndexScanOperation::send_next_scan_ordered(Uint32 idx){  
  if(idx == theParallelism)
    return 0;
  
  NdbApiSignal tSignal(theNdb->theMyRef);
  tSignal.setSignal(GSN_SCAN_NEXTREQ);
  
  Uint32* theData = tSignal.getDataPtrSend();
  theData[0] = theNdbCon->theTCConPtr;
  theData[1] = 0;
  Uint64 transId = theNdbCon->theTransactionId;
  theData[2] = transId;
  theData[3] = (Uint32) (transId >> 32);
  
  /**
   * Prepare ops
   */
  Uint32 last = m_sent_receivers_count;
  Uint32 * prep_array = theData + 4;

  NdbReceiver * tRec = m_api_receivers[idx];
  m_sent_receivers[last] = tRec;
  tRec->m_list_index = last;
  prep_array[0] = tRec->m_tcPtrI;
  tRec->prepareSend();
  
  m_sent_receivers_count = last + 1;
  m_current_api_receiver = idx + 1;
  
  Uint32 nodeId = theNdbCon->theDBnode;
  TransporterFacade * tp = TransporterFacade::instance();
  tSignal.setLength(4+1);
  return tp->sendSignal(&tSignal, nodeId);
}

int
NdbScanOperation::close_impl(TransporterFacade* tp){
  Uint32 seq = theNdbCon->theNodeSequence;
  Uint32 nodeId = theNdbCon->theDBnode;
  
  if(seq != tp->getNodeSequence(nodeId)){
    theNdbCon->theReleaseOnClose = true;
    return -1;
  }
  
  while(theError.code == 0 && m_sent_receivers_count){
    theNdb->theWaiter.m_node = nodeId;
    theNdb->theWaiter.m_state = WAIT_SCAN;
    int return_code = theNdb->receiveResponse(WAITFOR_SCAN_TIMEOUT);
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

  if(m_api_receivers_count+m_conf_receivers_count){
    // Send close scan
    if(send_next_scan(0, true) == -1){ // Close scan
      theNdbCon->theReleaseOnClose = true;
      return -1;
    }
  }
  
  /**
   * wait for close scan conf
   */
  while(m_sent_receivers_count+m_api_receivers_count+m_conf_receivers_count){
    theNdb->theWaiter.m_node = nodeId;
    theNdb->theWaiter.m_state = WAIT_SCAN;
    int return_code = theNdb->receiveResponse(WAITFOR_SCAN_TIMEOUT);
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
  m_sent_receivers_count = parallell;
  m_conf_receivers_count = 0;
  
  if(ordered){
    m_current_api_receiver = parallell;
    m_api_receivers_count = parallell;
  }
}

int
NdbScanOperation::restart()
{
  
  TransporterFacade* tp = TransporterFacade::instance();
  Guard guard(tp->theMutexPtr);
  Uint32 nodeId = theNdbCon->theDBnode;
  
  {
    int res;
    if((res= close_impl(tp)))
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
NdbIndexScanOperation::reset_bounds(){
  int res;
  
  {
    TransporterFacade* tp = TransporterFacade::instance();
    Guard guard(tp->theMutexPtr);
    res= close_impl(tp);
  }

  if(!res)
  {
    theError.code = 0;
    reset_receivers(theParallelism, m_ordered);
    
    theLastKEYINFO = theFirstKEYINFO;
    theKEYINFOptr = ((KeyInfo*)theFirstKEYINFO->getDataPtrSend())->keyData;
    theTupKeyLen = 0;
    theTotalNrOfKeyWordInSignal = 0;
    m_transConnection
      ->remove_list((NdbOperation*&)m_transConnection->m_firstExecutedScanOp,
		    this);
    m_transConnection->define_scan_op(this);
    return 0;
  }
  return res;
}

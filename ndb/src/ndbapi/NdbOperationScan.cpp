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

#include "NdbOperation.hpp"
#include "NdbScanReceiver.hpp"

#include <signaldata/TcKeyReq.hpp>
#include <signaldata/ScanTab.hpp>
#include <signaldata/ScanFrag.hpp>
#include <signaldata/KeyInfo.hpp>


/******************************************************************************
 * int openScanRead();
 *****************************************************************************/
int
NdbOperation::openScanRead(Uint32 aParallelism)
{
  aParallelism = checkParallelism(aParallelism);

  if ((theNdbCon->theCommitStatus != NdbConnection::Started) &&
      (theStatus != Init) &&
      (aParallelism == 0)) {
    setErrorCode(4200);
    return -1;
  }
  return openScan(aParallelism, false, false, false);
}

/****************************************************************************
 * int openScanExclusive();
 ****************************************************************************/
int
NdbOperation::openScanExclusive(Uint32 aParallelism)
{
  aParallelism = checkParallelism(aParallelism);
  
  if ((theNdbCon->theCommitStatus != NdbConnection::Started) &&
      (theStatus != Init) &&
      (aParallelism == 0)) {
    setErrorCode(4200);
    return -1;
  }
  return openScan(aParallelism, true, true, false);
}

/******************************************************************************
 * int openScanReadHoldLock();
 *****************************************************************************/
int
NdbOperation::openScanReadHoldLock(Uint32 aParallelism)
{
  aParallelism = checkParallelism(aParallelism);

  if ((theNdbCon->theCommitStatus != NdbConnection::Started) &&
      (theStatus != Init) &&
      (aParallelism == 0)) {
    setErrorCode(4200);
    return -1;
  }
  return openScan(aParallelism, false, true, false);
}

/******************************************************************************
 * int openScanReadCommitted();
 *****************************************************************************/
int
NdbOperation::openScanReadCommitted(Uint32 aParallelism)
{
  aParallelism = checkParallelism(aParallelism);

  if ((theNdbCon->theCommitStatus != NdbConnection::Started) &&
      (theStatus != Init) &&
      (aParallelism == 0)) {
    setErrorCode(4200);
    return -1;
  }
  return openScan(aParallelism, false, false, true);
}

/****************************************************************************
 * int checkParallelism();
 * Remark  If the parallelism is set wrong the number of scan-operations 
 *         will not correspond to the number of TRANSID_AI signals returned 
 * 	   from NDB and the result will be a crash, therefore
 *	   we adjust it or return an error if the value is totally wrong.
 ****************************************************************************/
int
NdbOperation::checkParallelism(Uint32 aParallelism)
{
  if (aParallelism == 0) {
    setErrorCodeAbort(4232);
    return 0;
  }
  if (aParallelism > 16) {
    if (aParallelism <= 240) {
      
      /**
       * If tscanConcurrency > 16 it must be a multiple of 16
       */
      if (((aParallelism >> 4) << 4) < aParallelism) {
        aParallelism = ((aParallelism >> 4) << 4) + 16;
      }//if

      /*---------------------------------------------------------------*/
      /*  We cannot have a parallelism > 16 per node  	               */
      /*---------------------------------------------------------------*/
      if ((aParallelism / theNdb->theNoOfDBnodes) > 16) {
        aParallelism = theNdb->theNoOfDBnodes * 16;
      }//if

    } else {
      setErrorCodeAbort(4232);
      aParallelism = 0;
    }//if
  }//if
  return aParallelism;
}//NdbOperation::checkParallelism()

/**********************************************************************
 * int openScan();
 *************************************************************************/
int
NdbOperation::openScan(Uint32 aParallelism, 
		       bool lockMode, bool lockHoldMode, bool readCommitted)
{
  aParallelism = checkParallelism(aParallelism);
  if(aParallelism == 0){
    return 0;
  }
  NdbScanReceiver* tScanRec;
  // It is only possible to call openScan if 
  //  1. this transcation don't already  contain another scan operation
  //  2. this transaction don't already contain other operations
  //  3. theScanOp contains a NdbScanOperation
  if (theNdbCon->theScanningOp != NULL){
    setErrorCode(4605);
    return -1;
  }

  if ((theNdbCon->theFirstOpInList != this) ||
      (theNdbCon->theLastOpInList != this)) {
    setErrorCode(4603);
    return -1;
  }
  theNdbCon->theScanningOp = this;
  
  initScan();
  theParallelism = aParallelism;

  // If the scan is on ordered index then it is a range scan
  if (m_currentTable->m_indexType == NdbDictionary::Index::OrderedIndex ||
      m_currentTable->m_indexType == NdbDictionary::Index::UniqueOrderedIndex) {
    assert(m_currentTable == m_accessTable);
    m_currentTable = theNdb->theDictionary->getTable(m_currentTable->m_primaryTable.c_str());
    assert(m_currentTable != NULL);
    // Modify operation state
    theStatus = SetBound;
    theOperationType  = OpenRangeScanRequest;
  }

  theScanReceiversArray = new NdbScanReceiver* [aParallelism];
  if (theScanReceiversArray == NULL){
    setErrorCodeAbort(4000);
    return -1;
  }
  
  for (Uint32 i = 0; i < aParallelism; i ++) {
    tScanRec = theNdb->getNdbScanRec();
    if (tScanRec == NULL) {
      setErrorCodeAbort(4000);
      return -1;
    }//if
    tScanRec->init(this, lockMode); 
    theScanReceiversArray[i] = tScanRec;
  }

  theSCAN_TABREQ = theNdb->getSignal();
  if (theSCAN_TABREQ == NULL) {
    setErrorCodeAbort(4000);
    return -1;
  }//if
  ScanTabReq * const scanTabReq = CAST_PTR(ScanTabReq, theSCAN_TABREQ->getDataPtrSend());
  scanTabReq->apiConnectPtr = theNdbCon->theTCConPtr;
  scanTabReq->tableId = m_accessTable->m_tableId;
  scanTabReq->tableSchemaVersion = m_accessTable->m_version;
  scanTabReq->storedProcId = 0xFFFF;
  scanTabReq->buddyConPtr = theNdbCon->theBuddyConPtr;
  
  Uint32 reqInfo = 0;
  ScanTabReq::setParallelism(reqInfo, aParallelism);
  ScanTabReq::setLockMode(reqInfo, lockMode);
  ScanTabReq::setHoldLockFlag(reqInfo, lockHoldMode);
  ScanTabReq::setReadCommittedFlag(reqInfo, readCommitted);
  if (theOperationType == OpenRangeScanRequest)
    ScanTabReq::setRangeScanFlag(reqInfo, true);
  scanTabReq->requestInfo = reqInfo;

  Uint64 transId = theNdbCon->getTransactionId();
  scanTabReq->transId1 = (Uint32) transId;
  scanTabReq->transId2 = (Uint32) (transId >> 32);

  for (Uint32 i = 0; i < 16 && i < aParallelism ; i++) {
    scanTabReq->apiOperationPtr[i] = theScanReceiversArray[i]->ptr2int();
  }//for

  // Create one additional SCAN_TABINFO for each
  // 16 of parallelism
  NdbApiSignal* tSignal;  
  Uint32 tParallelism = aParallelism;
  while (tParallelism > 16) {
    tSignal = theNdb->getSignal();
    if (tSignal == NULL) {
      setErrorCodeAbort(4000);
      return -1;
    }//if
    if (tSignal->setSignal(GSN_SCAN_TABINFO) == -1) {
      setErrorCode(4001);
      return -1;
    }    
    tSignal->next(theFirstSCAN_TABINFO_Send);
    theFirstSCAN_TABINFO_Send = tSignal;
    tParallelism -= 16;
  }//while

  // Format all SCAN_TABINFO signals
  tParallelism = 16;
  tSignal = theFirstSCAN_TABINFO_Send;
  while (tSignal != NULL) {
    tSignal->setData(theNdbCon->theTCConPtr, 1);
    for (int i = 0; i < 16 ; i++) {
      tSignal->setData(theScanReceiversArray[i + tParallelism]->ptr2int(), i + 2);
    }//for
    tSignal = tSignal->next();
    tParallelism += 16;
  }//while

  getFirstATTRINFOScan();
  return 0;
}//NdbScanOperation::openScan()

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
NdbOperation::getFirstATTRINFOScan()
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

/*
 * After setBound() are done, move the accumulated ATTRINFO signals to
 * a separate list.  Then continue with normal scan.
 */
int
NdbOperation::saveBoundATTRINFO()
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
  return getFirstATTRINFOScan();
}

/*****************************************************************************
 * void releaseScan()
 *
 * Return Value    No return value. 
 * Parameters:     No parameters.
 * Remark:         Release objects after scanning.
 *****************************************************************************/
void
NdbOperation::releaseScan()
{
  NdbScanReceiver* tScanRec;
  TransporterFacade::instance()->lock_mutex();
  for (Uint32 i = 0; i < theParallelism && theScanReceiversArray != NULL; i++) {
    tScanRec = theScanReceiversArray[i];
    if (tScanRec != NULL) {
      tScanRec->release();
      tScanRec->next(NULL);
    } 
  }
  TransporterFacade::instance()->unlock_mutex();
  releaseSignals();  

  if (theScanReceiversArray != NULL) {
    for (Uint32 i = 0; i < theParallelism; i++) {
      NdbScanReceiver* tScanRec;
      tScanRec = theScanReceiversArray[i];
      if (tScanRec != NULL) {
	theNdb->releaseNdbScanRec(tScanRec);
	theScanReceiversArray[i] = NULL;
      } 
    }
    
    delete [] theScanReceiversArray;
  }//if
  theScanReceiversArray = NULL;

  if (theSCAN_TABREQ != NULL){
    theNdb->releaseSignal(theSCAN_TABREQ);
    theSCAN_TABREQ = NULL;
  }
}

void NdbOperation::releaseSignals(){
  theNdb->releaseSignalsInList(&theFirstSCAN_TABINFO_Send);
  theFirstSCAN_TABINFO_Send = NULL;
  theLastSCAN_TABINFO_Send = NULL;
  //  theNdb->releaseSignalsInList(&theFirstSCAN_TABINFO_Recv);

  while(theFirstSCAN_TABINFO_Recv != NULL){
    NdbApiSignal* tmp = theFirstSCAN_TABINFO_Recv;
    theFirstSCAN_TABINFO_Recv = tmp->next();
    delete tmp;
  }
  theFirstSCAN_TABINFO_Recv = NULL;
  theLastSCAN_TABINFO_Recv = NULL;
  if (theSCAN_TABCONF_Recv != NULL){
    //    theNdb->releaseSignal(theSCAN_TABCONF_Recv);
    delete theSCAN_TABCONF_Recv;
    theSCAN_TABCONF_Recv = NULL;
  }
}


void NdbOperation::prepareNextScanResult(){
  NdbScanReceiver* tScanRec;
  for (Uint32 i = 0; i < theParallelism; i++) {
    tScanRec = theScanReceiversArray[i];
    assert(tScanRec != NULL);
    tScanRec->prepareNextScanResult();
    tScanRec->next(NULL);
  }
  releaseSignals();
}

/******************************************************************************
 * void initScan();
 *
 * Return Value:  Return 0 : init was successful.
 *                Return -1: In all other case.  
 * Remark:        Initiates operation record after allocation.
 *****************************************************************************/
void
NdbOperation::initScan()
{
  theTotalRecAI_Len = 0;
  theCurrRecAI_Len  = 0;
  theStatus         = GetValue;
  theOperationType  = OpenScanRequest;
  theCurrentRecAttr = theFirstRecAttr;
  theScanInfo       = 0;
  theMagicNumber    = 0xABCDEF01;
  theTotalCurrAI_Len = 5;

  theFirstLabel = NULL;
  theLastLabel  = NULL;
  theFirstBranch  = NULL;
  theLastBranch  = NULL;
  
  theFirstCall  = NULL;
  theLastCall  = NULL;
  theFirstSubroutine  = NULL;
  theLastSubroutine  = NULL;

  theNoOfLabels         = 0;
  theNoOfSubroutines    = 0;
  
  theSubroutineSize = 0;
  theInitialReadSize = 0;
  theInterpretedSize = 0;
  theFinalUpdateSize = 0;
  theFinalReadSize = 0;
  theInterpretIndicator = 1;


  theFirstSCAN_TABINFO_Send  = NULL;
  theLastSCAN_TABINFO_Send  = NULL;
  theFirstSCAN_TABINFO_Recv = NULL;
  theLastSCAN_TABINFO_Recv = NULL;
  theSCAN_TABCONF_Recv = NULL;

  theScanReceiversArray = NULL;

  theTotalBoundAI_Len = 0;
  theBoundATTRINFO = NULL;
  return;
}

NdbOperation* NdbOperation::takeOverForDelete(NdbConnection* updateTrans){
  return takeOverScanOp(DeleteRequest, updateTrans);
}

NdbOperation* NdbOperation::takeOverForUpdate(NdbConnection* updateTrans){
  return takeOverScanOp(UpdateRequest, updateTrans);
}
/******************************************************************************
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
 *****************************************************************************/
NdbOperation*
NdbOperation::takeOverScanOp(OperationType opType, NdbConnection* updateTrans)
{
  if (opType != UpdateRequest && opType != DeleteRequest) {
    setErrorCode(4604); 
    return NULL;
  }
   
  const NdbScanReceiver* tScanRec = theNdbCon->thePreviousScanRec;
  if (tScanRec == NULL){
    // No operation read by nextScanResult
    setErrorCode(4609); 
    return NULL;
  }

  if (tScanRec->theFirstKEYINFO20_Recv == NULL){
    // No KEYINFO20 received
    setErrorCode(4608); 
    return NULL;
  }
  
  NdbOperation * newOp = updateTrans->getNdbOperation(m_currentTable);
  if (newOp == NULL){
    return NULL;
  }
  
  /**
   * Copy and caclulate attributes from the scanned operation to the
   * new operation
   */ 
  const KeyInfo20 * const firstKeyInfo20 = 
    CAST_CONSTPTR(KeyInfo20, tScanRec->theFirstKEYINFO20_Recv->getDataPtr());
  const Uint32 totalKeyLen = firstKeyInfo20->keyLen;
  newOp->theTupKeyLen = totalKeyLen;
  
  newOp->theOperationType = opType;
  if (opType == DeleteRequest) {
    newOp->theStatus = GetValue;  
  } else {
    newOp->theStatus = SetValue;  
  }
  const Uint32 tScanInfo = firstKeyInfo20->scanInfo_Node & 0xFFFF;
  const Uint32 tTakeOverNode = firstKeyInfo20->scanInfo_Node >> 16;
  {
    UintR scanInfo = 0;
    TcKeyReq::setTakeOverScanFlag(scanInfo, 1);
    TcKeyReq::setTakeOverScanNode(scanInfo, tTakeOverNode);
    TcKeyReq::setTakeOverScanInfo(scanInfo, tScanInfo);
    newOp->theScanInfo = scanInfo;
  }

  /**
   * Copy received KEYINFO20 signals into TCKEYREQ and KEYINFO signals
   * put them in list of the new op
   */
  TcKeyReq * const tcKeyReq = 
    CAST_PTR(TcKeyReq, newOp->theTCREQ->getDataPtrSend());
  
  // Copy the first 8 words of key info from KEYINF20 into TCKEYREQ
  for (Uint32 i = 0; i < TcKeyReq::MaxKeyInfo; i++) {
    tcKeyReq->keyInfo[i] = firstKeyInfo20->keyData[i];
  }
  if (totalKeyLen > TcKeyReq::MaxKeyInfo) {

    Uint32 keyWordsCopied = TcKeyReq::MaxKeyInfo;
 
    // Create KEYINFO signals in newOp
    for (Uint32 i = keyWordsCopied; i < totalKeyLen; i += KeyInfo::DataLength){
      NdbApiSignal* tSignal = theNdb->getSignal();
      if (tSignal == NULL){
	setErrorCodeAbort(4000);
	return NULL;
      }
      if (tSignal->setSignal(GSN_KEYINFO) == -1){
	setErrorCodeAbort(4001);
	return NULL;
      }
      tSignal->next(newOp->theFirstKEYINFO);
      newOp->theFirstKEYINFO = tSignal;
    }

    // Init pointers to KEYINFO20 signal 
    NdbApiSignal* currKeyInfo20 = tScanRec->theFirstKEYINFO20_Recv;
    const KeyInfo20 * keyInfo20 = 
      CAST_CONSTPTR(KeyInfo20, currKeyInfo20->getDataPtr());
    Uint32 posInKeyInfo20 = keyWordsCopied;

    // Init pointers to KEYINFO signal 
    NdbApiSignal* currKeyInfo = newOp->theFirstKEYINFO;
    KeyInfo * keyInfo = CAST_PTR(KeyInfo, currKeyInfo->getDataPtrSend());
    Uint32 posInKeyInfo = 0;

    // Copy from KEYINFO20 to KEYINFO
    while(keyWordsCopied < totalKeyLen){
      keyInfo->keyData[posInKeyInfo++] = keyInfo20->keyData[posInKeyInfo20++];
      keyWordsCopied++;
      if(keyWordsCopied >= totalKeyLen)
	break;
      if (posInKeyInfo20 >= 
	  (currKeyInfo20->getLength()-KeyInfo20::HeaderLength)){
	currKeyInfo20  = currKeyInfo20->next();
	keyInfo20      = CAST_CONSTPTR(KeyInfo20, currKeyInfo20->getDataPtr());
	posInKeyInfo20 = 0;
      }
      if (posInKeyInfo >= KeyInfo::DataLength){
	currKeyInfo  = currKeyInfo->next();
	keyInfo      = CAST_PTR(KeyInfo, currKeyInfo->getDataPtrSend());
	posInKeyInfo = 0;
      }	
    }
  }

  // create blob handles automatically
  if (opType == DeleteRequest && m_currentTable->m_noOfBlobs != 0) {
    for (unsigned i = 0; i < m_currentTable->m_columns.size(); i++) {
      NdbColumnImpl* c = m_currentTable->m_columns[i];
      assert(c != 0);
      if (c->getBlobType()) {
        if (newOp->getBlobHandle(updateTrans, c) == NULL)
          return NULL;
      }
    }
  }

  return newOp;
}

int
NdbOperation::getKeyFromKEYINFO20(Uint32* data, unsigned size)
{
  const NdbScanReceiver* tScanRec = theNdbCon->thePreviousScanRec;
  NdbApiSignal* tSignal = tScanRec->theFirstKEYINFO20_Recv;
  unsigned pos = 0;
  unsigned n = 0;
  while (pos < size) {
    if (n == 20) {
      tSignal = tSignal->next();
      n = 0;
    }
    const unsigned h = KeyInfo20::HeaderLength;
    data[pos++] = tSignal->getDataPtrSend()[h + n++];
  }
  return 0;
}

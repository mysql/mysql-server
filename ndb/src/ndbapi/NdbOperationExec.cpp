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


/***************************************************************************
Name:          NdbOperationExec.C 
Include:
Link:
Author:        UABRONM Mikael Ronström UAB/M/MT        Jonas Kamf UAB/M/MT 
Date:          2001-10-16
Version:       1.2
Description:   
Documentation:
***************************************************************************/

#include <NdbOperation.hpp>
#include <NdbConnection.hpp>
#include "NdbApiSignal.hpp"
#include <Ndb.hpp>
#include <NdbRecAttr.hpp>
#include "NdbUtil.hpp"

#include "Interpreter.hpp"
#include <AttributeHeader.hpp>
#include <signaldata/TcKeyReq.hpp>
#include <signaldata/KeyInfo.hpp>
#include <signaldata/AttrInfo.hpp>
#include <signaldata/ScanTab.hpp>

#include <ndb_version.h>

#include "API.hpp"
#include <NdbOut.hpp>


/******************************************************************************
int doSend()

Return Value:   Return >0 : send was succesful, returns number of signals sent
                Return -1: In all other case.   
Parameters:     aProcessorId: Receiving processor node
Remark:         Sends the ATTRINFO signal(s)
******************************************************************************/
int
NdbOperation::doSendScan(int aProcessorId)
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
  // Update the "attribute info length in words" in SCAN_TABREQ before 
  // sending it. This could not be done in openScan because 
  // we created the ATTRINFO signals after the SCAN_TABREQ signal.
  ScanTabReq * const scanTabReq = CAST_PTR(ScanTabReq, tSignal->getDataPtrSend());
  scanTabReq->attrLen = theTotalCurrAI_Len;
  if (theOperationType == OpenRangeScanRequest)
    scanTabReq->attrLen += theTotalBoundAI_Len;
  TransporterFacade *tp = TransporterFacade::instance();
  if (tp->sendSignal(tSignal, aProcessorId) == -1) {
    setErrorCode(4002);
    return -1;
  } 
  tSignalCount++;

  tSignal = theFirstSCAN_TABINFO_Send;   
  while (tSignal != NULL){
    if (tp->sendSignal(tSignal, aProcessorId)) {
      setErrorCode(4002);
      return -1;
    }
    tSignalCount++;
    tSignal = tSignal->next();
  }

  if (theOperationType == OpenRangeScanRequest) {
    // must have at least one signal since it contains attrLen for bounds
    assert(theBoundATTRINFO != NULL);
    tSignal = theBoundATTRINFO;
    while (tSignal != NULL) {
      if (tp->sendSignal(tSignal,aProcessorId) == -1){
        setErrorCode(4002);
        return -1;
      }
      tSignalCount++;
      tSignal = tSignal->next();
    }
  }

  tSignal = theFirstATTRINFO;
  while (tSignal != NULL) {
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

void
NdbOperation::setLastFlag(NdbApiSignal* signal, Uint32 lastFlag)
{
  TcKeyReq * const req = CAST_PTR(TcKeyReq, signal->getDataPtrSend());
  TcKeyReq::setExecuteFlag(req->requestInfo, lastFlag);
}

/******************************************************************************
int doSend()

Return Value:   Return >0 : send was succesful, returns number of signals sent
                Return -1: In all other case.   
Parameters:     aProcessorId: Receiving processor node
Remark:         Sends the TCKEYREQ signal and optional KEYINFO and ATTRINFO 
                signals.
******************************************************************************/
int
NdbOperation::doSend(int aNodeId, Uint32 lastFlag)
{
  int tReturnCode;
  int tSignalCount = 0;
  assert(theTCREQ != NULL);
  setLastFlag(theTCREQ, lastFlag);
  TransporterFacade *tp = TransporterFacade::instance();
  tReturnCode = tp->sendSignal(theTCREQ, aNodeId);
  tSignalCount++;
  if (tReturnCode == -1) {
    return -1;
  }
  NdbApiSignal *tSignal = theFirstKEYINFO;
  while (tSignal != NULL) {
    NdbApiSignal* tnextSignal = tSignal->next();
    tReturnCode = tp->sendSignal(tSignal, aNodeId);
    tSignal = tnextSignal;
    if (tReturnCode == -1) {
      return -1;
    }
    tSignalCount++;
  }//while
  tSignal = theFirstATTRINFO;
  while (tSignal != NULL) {
    NdbApiSignal* tnextSignal = tSignal->next();
    tReturnCode = tp->sendSignal(tSignal, aNodeId);
    tSignal = tnextSignal;
    if (tReturnCode == -1) {
      return -1;
    }
    tSignalCount++;
  }//while
  theNdbCon->OpSent();
  return tSignalCount;
}//NdbOperation::doSend()

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
int NdbOperation::prepareSendScan(Uint32 aTC_ConnectPtr,
				   Uint64 aTransactionId){

  if (theInterpretIndicator != 1 ||
      (theOperationType != OpenScanRequest &&
       theOperationType != OpenRangeScanRequest)) {
    setErrorCodeAbort(4005);
    return -1;
  }

  if (theStatus == SetBound) {
    saveBoundATTRINFO();
    theStatus = GetValue;
  }

  theErrorLine = 0;

  // In preapareSendInterpreted we set the sizes (word 4-8) in the
  // first ATTRINFO signal.
  if (prepareSendInterpreted() == -1)
    return -1;
  
  const Uint32 transId1 = (Uint32) (aTransactionId & 0xFFFFFFFF);
  const Uint32 transId2 = (Uint32) (aTransactionId >> 32);
  
  if (theOperationType == OpenRangeScanRequest) {
    NdbApiSignal* tSignal = theBoundATTRINFO;
    do{
      tSignal->setData(aTC_ConnectPtr, 1);
      tSignal->setData(transId1, 2);
      tSignal->setData(transId2, 3);
      tSignal = tSignal->next();
    } while (tSignal != NULL);
  }
  theCurrentATTRINFO->setLength(theAI_LenInCurrAI);
  NdbApiSignal* tSignal = theFirstATTRINFO;
  do{
    tSignal->setData(aTC_ConnectPtr, 1);
    tSignal->setData(transId1, 2);
    tSignal->setData(transId2, 3);
    tSignal = tSignal->next();
  } while (tSignal != NULL);
  return 0;
}

/***************************************************************************
int prepareSend(Uint32 aTC_ConnectPtr,
                Uint64 aTransactionId)

Return Value:   Return 0 : preparation of send was succesful.
                Return -1: In all other case.   
Parameters:     aTC_ConnectPtr: the Connect pointer to TC.
		aTransactionId:	the Transaction identity of the transaction.
Remark:         Puts the the data into TCKEYREQ signal and optional KEYINFO and ATTRINFO signals.
***************************************************************************/
int
NdbOperation::prepareSend(Uint32 aTC_ConnectPtr, Uint64 aTransId)
{
  Uint32 tTransId1, tTransId2;
  Uint32 tReqInfo;
  Uint32 tInterpretInd = theInterpretIndicator;
  
  theErrorLine = 0;

  if (tInterpretInd != 1) {
    OperationType tOpType = theOperationType;
    OperationStatus tStatus = theStatus;
    if ((tOpType == UpdateRequest) ||
	(tOpType == InsertRequest) ||
	(tOpType == WriteRequest)) {
      if (tStatus != SetValue) {
        setErrorCodeAbort(4116);
        return -1;
      }//if
    } else if ((tOpType == ReadRequest) || (tOpType == ReadExclusive) ||
	       (tOpType == DeleteRequest)) {
      if (tStatus != GetValue) {
        setErrorCodeAbort(4116);
        return -1;
      }//if
    } else {
      setErrorCodeAbort(4005);      
      return -1;
    }//if
  } else {
    if (prepareSendInterpreted() == -1) {
      return -1;
    }//if
  }//if
  
//-------------------------------------------------------------
// We start by filling in the first 9 unconditional words of the
// TCKEYREQ signal.
//-------------------------------------------------------------
  TcKeyReq * const tcKeyReq = CAST_PTR(TcKeyReq, theTCREQ->getDataPtrSend());

  Uint32 tTotalCurrAI_Len = theTotalCurrAI_Len;
  Uint32 tTableId = m_currentTable->m_tableId;
  Uint32 tSchemaVersion = m_currentTable->m_version;
  
  tcKeyReq->apiConnectPtr      = aTC_ConnectPtr;
  tcKeyReq->apiOperationPtr    = ptr2int();
  // Check if too much attrinfo have been defined
  if (tTotalCurrAI_Len > TcKeyReq::MaxTotalAttrInfo){
    setErrorCodeAbort(4257);
    return -1;
  }
  Uint32 TattrLen = 0;
  tcKeyReq->setAttrinfoLen(TattrLen, tTotalCurrAI_Len);
  tcKeyReq->setAPIVersion(TattrLen, NDB_VERSION);
  tcKeyReq->attrLen            = TattrLen;

  tcKeyReq->tableId            = tTableId;
  tcKeyReq->tableSchemaVersion = tSchemaVersion;
  tTransId1 = (Uint32) aTransId;
  tTransId2 = (Uint32) (aTransId >> 32);
  
//-------------------------------------------------------------
// Simple is simple if simple or both start and commit is set.
//-------------------------------------------------------------
// Temporarily disable simple stuff
  Uint8 tSimpleIndicator = 0;
//  Uint8 tSimpleIndicator = theSimpleIndicator;
  Uint8 tCommitIndicator = theCommitIndicator;
  Uint8 tStartIndicator = theStartIndicator;
//  if ((theNdbCon->theLastOpInList == this) && (theCommitIndicator == 0))
//    abort();
// Temporarily disable simple stuff
  Uint8 tSimpleAlt = 0;
//  Uint8 tSimpleAlt = tStartIndicator & tCommitIndicator;
  tSimpleIndicator = tSimpleIndicator | tSimpleAlt;

//-------------------------------------------------------------
// Simple state is set if start and commit is set and it is
// a read request. Otherwise it is set to zero.
//-------------------------------------------------------------
  Uint8 tReadInd = (theOperationType == ReadRequest);
  Uint8 tSimpleState = tReadInd & tSimpleAlt;
  theNdbCon->theSimpleState = tSimpleState;

  tcKeyReq->transId1           = tTransId1;
  tcKeyReq->transId2           = tTransId2;
  
  tReqInfo = 0;
  if (tTotalCurrAI_Len <= TcKeyReq::MaxAttrInfo) {
    tcKeyReq->setAIInTcKeyReq(tReqInfo, tTotalCurrAI_Len);
  } else {
    tcKeyReq->setAIInTcKeyReq(tReqInfo, TcKeyReq::MaxAttrInfo);
  }//if

  tcKeyReq->setSimpleFlag(tReqInfo, tSimpleIndicator);
  tcKeyReq->setCommitFlag(tReqInfo, tCommitIndicator);
  tcKeyReq->setStartFlag(tReqInfo, tStartIndicator);
  const Uint8 tInterpretIndicator = theInterpretIndicator;
  tcKeyReq->setInterpretedFlag(tReqInfo, tInterpretIndicator);

  Uint8 tDirtyIndicator = theDirtyIndicator;
  OperationType tOperationType = theOperationType;
  Uint32 tTupKeyLen = theTupKeyLen;
  Uint8 abortOption = theNdbCon->m_abortOption;

  tcKeyReq->setDirtyFlag(tReqInfo, tDirtyIndicator);
  tcKeyReq->setOperationType(tReqInfo, tOperationType);
  tcKeyReq->setKeyLength(tReqInfo, tTupKeyLen);
  tcKeyReq->setAbortOption(tReqInfo, abortOption);
  
  Uint8 tDistrKeyIndicator = theDistrKeyIndicator;
  Uint8 tDistrGroupIndicator = theDistrGroupIndicator;
  Uint8 tDistrGroupType = theDistrGroupType;
  Uint8 tScanIndicator = theScanInfo & 1;

  tcKeyReq->setDistributionGroupFlag(tReqInfo, tDistrGroupIndicator);
  tcKeyReq->setDistributionGroupTypeFlag(tReqInfo, tDistrGroupType);
  tcKeyReq->setDistributionKeyFlag(tReqInfo, tDistrKeyIndicator);
  tcKeyReq->setScanIndFlag(tReqInfo, tScanIndicator);

  tcKeyReq->requestInfo  = tReqInfo;

//-------------------------------------------------------------
// The next step is to fill in the upto three conditional words.
//-------------------------------------------------------------
  Uint32* tOptionalDataPtr = &tcKeyReq->scanInfo;
  Uint32 tDistrGHIndex = tScanIndicator;
  Uint32 tDistrKeyIndex = tDistrGHIndex + tDistrGroupIndicator;

  Uint32 tScanInfo = theScanInfo;
  Uint32 tDistributionGroup = theDistributionGroup;
  Uint32 tDistrKeySize = theDistrKeySize;

  tOptionalDataPtr[0] = tScanInfo;
  tOptionalDataPtr[tDistrGHIndex] = tDistributionGroup;
  tOptionalDataPtr[tDistrKeyIndex] = tDistrKeySize;

//-------------------------------------------------------------
// The next is step is to compress the key data part of the
// TCKEYREQ signal.
//-------------------------------------------------------------
  Uint32 tKeyIndex = tDistrKeyIndex + tDistrKeyIndicator;
  Uint32* tKeyDataPtr = &tOptionalDataPtr[tKeyIndex];
  Uint32 Tdata1 = tcKeyReq->keyInfo[0];
  Uint32 Tdata2 = tcKeyReq->keyInfo[1];
  Uint32 Tdata3 = tcKeyReq->keyInfo[2];
  Uint32 Tdata4 = tcKeyReq->keyInfo[3];
  Uint32 Tdata5;

  tKeyDataPtr[0] = Tdata1;
  tKeyDataPtr[1] = Tdata2;
  tKeyDataPtr[2] = Tdata3;
  tKeyDataPtr[3] = Tdata4;
  if (tTupKeyLen > 4) {
    Tdata1 = tcKeyReq->keyInfo[4];
    Tdata2 = tcKeyReq->keyInfo[5];
    Tdata3 = tcKeyReq->keyInfo[6];
    Tdata4 = tcKeyReq->keyInfo[7];

    tKeyDataPtr[4] = Tdata1;
    tKeyDataPtr[5] = Tdata2;
    tKeyDataPtr[6] = Tdata3;
    tKeyDataPtr[7] = Tdata4;
  }//if
//-------------------------------------------------------------
// Finally we also compress the ATTRINFO part of the signal.
// We optimise by using the if-statement for sending KEYINFO
// signals to calculating the new Attrinfo Index.
//-------------------------------------------------------------
  Uint32 tAttrInfoIndex;  

  if (tTupKeyLen > TcKeyReq::MaxKeyInfo) {
    /**
     *	Set transid, TC connect ptr and length in the KEYINFO signals
     */
    NdbApiSignal* tSignal = theFirstKEYINFO;
    Uint32 remainingKey = tTupKeyLen - TcKeyReq::MaxKeyInfo;
    do {
      Uint32* tSigDataPtr = tSignal->getDataPtrSend();
      NdbApiSignal* tnextSignal = tSignal->next();
      tSigDataPtr[0] = aTC_ConnectPtr;
      tSigDataPtr[1] = tTransId1;
      tSigDataPtr[2] = tTransId2;
      if (remainingKey > KeyInfo::DataLength) {
	// The signal is full
	tSignal->setLength(KeyInfo::MaxSignalLength);
	remainingKey -= KeyInfo::DataLength;
      }
      else {
	// Last signal
	tSignal->setLength(KeyInfo::HeaderLength + remainingKey);
	remainingKey = 0;
      }
      tSignal = tnextSignal;
    } while (tSignal != NULL);
    tAttrInfoIndex = tKeyIndex + TcKeyReq::MaxKeyInfo;
  } else {
    tAttrInfoIndex = tKeyIndex + tTupKeyLen;
  }//if

//-------------------------------------------------------------
// Perform the Attrinfo packing in the TCKEYREQ signal started
// above.
//-------------------------------------------------------------
  Uint32* tAIDataPtr = &tOptionalDataPtr[tAttrInfoIndex];
  Tdata1 = tcKeyReq->attrInfo[0];
  Tdata2 = tcKeyReq->attrInfo[1];
  Tdata3 = tcKeyReq->attrInfo[2];
  Tdata4 = tcKeyReq->attrInfo[3];
  Tdata5 = tcKeyReq->attrInfo[4];

  theTCREQ->setLength(tcKeyReq->getAIInTcKeyReq(tReqInfo) +
                      tAttrInfoIndex + TcKeyReq::StaticLength);
  tAIDataPtr[0] = Tdata1;
  tAIDataPtr[1] = Tdata2;
  tAIDataPtr[2] = Tdata3;
  tAIDataPtr[3] = Tdata4;
  tAIDataPtr[4] = Tdata5;

/***************************************************
*  Send the ATTRINFO signals.
***************************************************/
  if (tTotalCurrAI_Len > 5) {
    // Set the last signal's length.
    NdbApiSignal* tSignal = theFirstATTRINFO;
    theCurrentATTRINFO->setLength(theAI_LenInCurrAI);
    do {
      Uint32* tSigDataPtr = tSignal->getDataPtrSend();
      NdbApiSignal* tnextSignal = tSignal->next();
      tSigDataPtr[0] = aTC_ConnectPtr;
      tSigDataPtr[1] = tTransId1;
      tSigDataPtr[2] = tTransId2;
      tSignal = tnextSignal;
    } while (tSignal != NULL);
  }//if
  NdbRecAttr* tRecAttrObject = theFirstRecAttr;
  theStatus = WaitResponse;
  theCurrentRecAttr = tRecAttrObject;
  return 0;
}//NdbOperation::prepareSend()

/***************************************************************************
int prepareSendInterpreted()

Make preparations to send an interpreted operation.
Return Value:   Return 0 : succesful.
                Return -1: In all other case.   
***************************************************************************/
int
NdbOperation::prepareSendInterpreted()
{
  Uint32 tTotalCurrAI_Len = theTotalCurrAI_Len;
  Uint32 tInitReadSize = theInitialReadSize;
  if (theStatus == ExecInterpretedValue) {
    if (insertATTRINFO(Interpreter::EXIT_OK_LAST) != -1) {
//-------------------------------------------------------------------------
// Since we read the total length before inserting the last entry in the
// signals we need to add one to the total length.
//-------------------------------------------------------------------------

      theInterpretedSize = (tTotalCurrAI_Len + 1) -
       (tInitReadSize + 5);

    } else {
      return -1;
    }//if
  } else if (theStatus == FinalGetValue) {

    theFinalReadSize = tTotalCurrAI_Len -
      (tInitReadSize + theInterpretedSize + theFinalUpdateSize + 5);

  } else if (theStatus == SetValueInterpreted) {

    theFinalUpdateSize = tTotalCurrAI_Len -
       (tInitReadSize + theInterpretedSize + 5);

  } else if (theStatus == SubroutineEnd) {

    theSubroutineSize = tTotalCurrAI_Len -
      (tInitReadSize + theInterpretedSize + 
         theFinalUpdateSize + theFinalReadSize + 5);

  } else if (theStatus == GetValue) {
    theInitialReadSize = tTotalCurrAI_Len - 5;
  } else {
    setErrorCodeAbort(4116);
    return -1;
  }

  while (theFirstBranch != NULL) {
    Uint32 tRelAddress;
    Uint32 tLabelAddress = 0;
    int     tAddress = -1;
    NdbBranch* tNdbBranch = theFirstBranch;
    Uint32 tBranchLabel = tNdbBranch->theBranchLabel;
    NdbLabel* tNdbLabel = theFirstLabel;
    if (tBranchLabel >= theNoOfLabels) {
      setErrorCodeAbort(4221);
      return -1;
    }//if

    // Find the label address
    while (tNdbLabel != NULL) {
      for(tLabelAddress = 0; tLabelAddress<16; tLabelAddress++){
	const Uint32 labelNo = tNdbLabel->theLabelNo[tLabelAddress];
	if(tBranchLabel == labelNo){
	  tAddress = tNdbLabel->theLabelAddress[tLabelAddress];
	  break;
	}
      }
      
      if(tAddress != -1)
	break;
      tNdbLabel = tNdbLabel->theNext;
    }//while
    if (tAddress == -1) {
//-------------------------------------------------------------------------
// We were unable to find any label which the branch refers to. This means
// that the application have not programmed the interpreter program correctly.
//-------------------------------------------------------------------------
      setErrorCodeAbort(4222);
      return -1;
    }//if
    if (tNdbLabel->theSubroutine[tLabelAddress] != tNdbBranch->theSubroutine) {
      setErrorCodeAbort(4224);
      return -1;
    }//if
    // Now it is time to update the signal data with the relative branch jump.
    if (tAddress < int(tNdbBranch->theBranchAddress)) {
      tRelAddress = (tNdbBranch->theBranchAddress - tAddress) << 16;
      
      // Indicate backward jump direction
      tRelAddress = tRelAddress + (1 << 31);

    } else if (tAddress > int(tNdbBranch->theBranchAddress)) {
      tRelAddress = (tAddress - tNdbBranch->theBranchAddress) << 16;
    } else {
       setErrorCodeAbort(4223);
       return -1;
    }//if
    NdbApiSignal* tSignal = tNdbBranch->theSignal;
    Uint32 tReadData = tSignal->readData(tNdbBranch->theSignalAddress);
    tSignal->setData((tRelAddress + tReadData), tNdbBranch->theSignalAddress);
      
    theFirstBranch = theFirstBranch->theNext;
    theNdb->releaseNdbBranch(tNdbBranch);
  }//while

  while (theFirstCall != NULL) {
    Uint32 tSubroutineCount = 0;
    int     tAddress = -1;
    NdbSubroutine* tNdbSubroutine;
    NdbCall* tNdbCall = theFirstCall;
    if (tNdbCall->theSubroutine >= theNoOfSubroutines) {
      setErrorCodeAbort(4221);
      return -1;
    }//if
// Find the subroutine address
    tNdbSubroutine = theFirstSubroutine;
    while (tNdbSubroutine != NULL) {
      tSubroutineCount += 16;
      if (tNdbCall->theSubroutine < tSubroutineCount) {
// Subroutine Found
        Uint32 tSubroutineAddress = tNdbCall->theSubroutine - (tSubroutineCount - 16);
        tAddress = tNdbSubroutine->theSubroutineAddress[tSubroutineAddress];
        break;
      }//if
      tNdbSubroutine = tNdbSubroutine->theNext;
    }//while
    if (tAddress == -1) {
      setErrorCodeAbort(4222);
      return -1;
    }//if
// Now it is time to update the signal data with the relative branch jump.
    NdbApiSignal* tSignal = tNdbCall->theSignal;
    Uint32 tReadData = tSignal->readData(tNdbCall->theSignalAddress);
    tSignal->setData(((tAddress << 16) + tReadData), tNdbCall->theSignalAddress);
      
    theFirstCall = theFirstCall->theNext;
    theNdb->releaseNdbCall(tNdbCall);
  }//while
  
  Uint32 tInitialReadSize = theInitialReadSize;
  Uint32 tInterpretedSize = theInterpretedSize;
  Uint32 tFinalUpdateSize = theFinalUpdateSize;
  Uint32 tFinalReadSize   = theFinalReadSize;
  Uint32 tSubroutineSize  = theSubroutineSize;
  if (theOperationType != OpenScanRequest &&
      theOperationType != OpenRangeScanRequest) {
    TcKeyReq * const tcKeyReq = CAST_PTR(TcKeyReq, theTCREQ->getDataPtrSend());

    tcKeyReq->attrInfo[0] = tInitialReadSize;
    tcKeyReq->attrInfo[1] = tInterpretedSize;
    tcKeyReq->attrInfo[2] = tFinalUpdateSize;
    tcKeyReq->attrInfo[3] = tFinalReadSize;
    tcKeyReq->attrInfo[4] = tSubroutineSize;
  } else {
    // If a scan is defined we use the first ATTRINFO instead of TCKEYREQ.
    theFirstATTRINFO->setData(tInitialReadSize, 4);
    theFirstATTRINFO->setData(tInterpretedSize, 5);
    theFirstATTRINFO->setData(tFinalUpdateSize, 6);
    theFirstATTRINFO->setData(tFinalReadSize, 7);
    theFirstATTRINFO->setData(tSubroutineSize, 8);  
  }//if
  return 0;
}//NdbOperation::prepareSendInterpreted()

/***************************************************************************
int TCOPCONF(int anAttrInfoLen)

Return Value:   Return 0 : send was succesful.
                Return -1: In all other case.   
Parameters:     anAttrInfoLen: The length of the attribute information from TC.
Remark:         Handles the reception of the TC[KEY/INDX]CONF signal.
***************************************************************************/
void
NdbOperation::TCOPCONF(Uint32 anAttrInfoLen)
{
  Uint32 tCurrRecLen = theCurrRecAI_Len;
  if (theStatus == WaitResponse) {
    theTotalRecAI_Len = anAttrInfoLen;
    if (anAttrInfoLen == tCurrRecLen) {
      Uint32 tAI_ElemLen = theAI_ElementLen;
      NdbRecAttr* tCurrRecAttr = theCurrentRecAttr;
      theStatus = Finished;

      if ((tAI_ElemLen == 0) &&
          (tCurrRecAttr == NULL)) {
        NdbRecAttr* tRecAttr = theFirstRecAttr;
        while (tRecAttr != NULL) {
          if (tRecAttr->copyoutRequired())	// copy to application buffer
            tRecAttr->copyout();
          tRecAttr = tRecAttr->next();
        }
        theNdbCon->OpCompleteSuccess();
        return;
      } else if (tAI_ElemLen != 0) {
        setErrorCode(4213);
        theNdbCon->OpCompleteFailure();
        return;
      } else {
        setErrorCode(4214);
        theNdbCon->OpCompleteFailure();
        return;
      }//if
    } else if (anAttrInfoLen > tCurrRecLen) {
      return;
    } else {
      theStatus = Finished;

      if (theAI_ElementLen != 0) {
        setErrorCode(4213);
        theNdbCon->OpCompleteFailure();
        return;
      }//if
      if (theCurrentRecAttr != NULL) {
        setErrorCode(4214);
        theNdbCon->OpCompleteFailure();
        return;
      }//if
      theNdbCon->OpCompleteFailure();
      return;
    }//if
  } else {
    setErrorCode(4004);
  }//if
  return;
}//NdbOperation::TCKEYOPCONF()

int
NdbOperation::checkState_TransId(NdbApiSignal* aSignal)
{
  Uint64 tRecTransId, tCurrTransId;
  Uint32 tTmp1, tTmp2;

  if (theStatus != WaitResponse) {
#ifdef NDB_NO_DROPPED_SIGNAL
    abort();
#endif
    return -1;
  }//if

  tTmp1 = aSignal->readData(2);
  tTmp2 = aSignal->readData(3);

  tRecTransId = (Uint64)tTmp1 + ((Uint64)tTmp2 << 32);
  tCurrTransId = theNdbCon->getTransactionId();
  if (tCurrTransId != tRecTransId) {
#ifdef NDB_NO_DROPPED_SIGNAL
    abort();
#endif
    return -1;
  }//if
  return 0;
}//NdbOperation::checkState_TransId()

/***************************************************************************
int receiveTCKEYREF( NdbApiSignal* aSignal)

Return Value:   Return 0 : send was succesful.
                Return -1: In all other case.   
Parameters:     aSignal: the signal object that contains the TCKEYREF signal from TC.
Remark:         Handles the reception of the TCKEYREF signal.
***************************************************************************/
int
NdbOperation::receiveTCKEYREF( NdbApiSignal* aSignal)
{
  if (checkState_TransId(aSignal) == -1) {
    return -1;
  }//if

  theStatus = Finished;
  
  theNdbCon->theReturnStatus = NdbConnection::ReturnFailure;
  //-------------------------------------------------------------------------//
  // If the transaction this operation belongs to consists only of simple reads
  // we set the error code on the transaction object. 
  // If the transaction consists of other types of operations we set 
  // the error code only on the operation since the simple read is not really 
  // part of this transaction and we can not decide the status of the whole 
  // transaction based on this operation.
  //-------------------------------------------------------------------------//
  if (theNdbCon->theSimpleState == 0) {
    theError.code = aSignal->readData(4);
    theNdbCon->setOperationErrorCodeAbort(aSignal->readData(4));
    return theNdbCon->OpCompleteFailure();
  } else {
    theError.code = aSignal->readData(4);
    return theNdbCon->OpCompleteSuccess();
  }
  
}//NdbOperation::receiveTCKEYREF()

/***************************************************************************
int receiveREAD_CONF( NdbApiSignal* aSignal)

Return Value:   Return 0 : send was succesful.
                Return -1: In all other case.   
Parameters:     aSignal: the signal object that contains the READCONF signal from TUP.
Remark:         Handles the reception of the READCONF signal.
***************************************************************************/
int
NdbOperation::receiveREAD_CONF(const Uint32* aDataPtr, Uint32 aDataLength)
{
  Uint64 tRecTransId, tCurrTransId;
  Uint32 tCondFlag = (Uint32)(theStatus - WaitResponse);
  Uint32 tTotLen = aDataPtr[3];

  tRecTransId = (Uint64)aDataPtr[1] + ((Uint64)aDataPtr[2] << 32);
  tCurrTransId = theNdbCon->getTransactionId();
  tCondFlag |= (Uint32)((tRecTransId - tCurrTransId) != (Uint64)0);
  tCondFlag |= (Uint32)(aDataLength < 4);

  if (tCondFlag == 0) {
    theTotalRecAI_Len = tTotLen;
    int tRetValue = receiveREAD_AI((Uint32*)&aDataPtr[4], (aDataLength - 4));
    if (theStatus == Finished) {
      return tRetValue;
    } else {
      theStatus = Finished;
      return theNdbCon->OpCompleteFailure();
    }//if
  }//if
#ifdef NDB_NO_DROPPED_SIGNAL
  abort();
#endif
  return -1;
}//NdbOperation::receiveREAD_CONF()

/***************************************************************************
int receiveTRANSID_AI( NdbApiSignal* aSignal)

Return Value:   Return 0 : send was succesful.
                Return -1: In all other case.   
Parameters:     aSignal: the signal object that contains the TRANSID_AI signal.
Remark:         Handles the reception of the TRANSID_AI signal.
***************************************************************************/
int
NdbOperation::receiveTRANSID_AI(const Uint32* aDataPtr, Uint32 aDataLength)
{
  Uint64 tRecTransId, tCurrTransId;
  Uint32 tCondFlag = (Uint32)(theStatus - WaitResponse);

  tRecTransId = (Uint64)aDataPtr[1] + ((Uint64)aDataPtr[2] << 32);
  tCurrTransId = theNdbCon->getTransactionId();
  tCondFlag |= (Uint32)((tRecTransId - tCurrTransId) != (Uint64)0);
  tCondFlag |= (Uint32)(aDataLength < 3);

  if (tCondFlag == 0) {
    return receiveREAD_AI((Uint32*)&aDataPtr[3], (aDataLength - 3));
  }//if
#ifdef NDB_NO_DROPPED_SIGNAL
  abort();
#endif
  return -1;
}//NdbOperation::receiveTRANSID_AI()

/***************************************************************************
int receiveREAD_AI( NdbApiSignal* aSignal, int aLength, int aStartPos)

Return Value:   Return 0 : send was succesoccurredful.
                Return -1: In all other case.   
Parameters:     aSignal: the signal object that contains the LEN_ATTRINFO11 signal.
                aLength:
		aStartPos: 
Remark:         Handles the reception of the LEN_ATTRINFO11 signal.
***************************************************************************/
int
NdbOperation::receiveREAD_AI(Uint32* aDataPtr, Uint32 aLength)
{

  register Uint32  tAI_ElementLen = theAI_ElementLen;
  register Uint32* tCurrElemPtr   = theCurrElemPtr;
  if (theError.code == 0) {
  // If inconsistency error occurred we will still continue
  // receiving signals since we need to know whether commit
  // has occurred.

    register Uint32  tData;
    for (register Uint32 i = 0; i < aLength ; i++, aDataPtr++)
    {
      // Code to receive Attribute Information 
      tData = *aDataPtr;
      if (tAI_ElementLen != 0) {
        tAI_ElementLen--;
        *tCurrElemPtr = tData;
        tCurrElemPtr++;
        continue;
      } else {
      // Waiting for a new attribute element
        NdbRecAttr* tWorkingRecAttr;
	
        tWorkingRecAttr = theCurrentRecAttr;
	AttributeHeader ah(tData);
        const Uint32 tAttrId = ah.getAttributeId();
	const Uint32 tAttrSize = ah.getDataSize();
        if ((tWorkingRecAttr != NULL) &&
            (tWorkingRecAttr->attrId() == tAttrId)) {
          ;
        } else {
          setErrorCode(4211);
          break;
        }//if
        theCurrentRecAttr = tWorkingRecAttr->next();
	NdbColumnImpl * col = m_currentTable->getColumn(tAttrId);
        if (ah.isNULL()) {
	  // Return a Null value from the NDB to the attribute. 
	  if(col != 0 && col->m_nullable) {
	    tWorkingRecAttr->setNULL();
	    tAI_ElementLen = 0;
	  } else {
	    setErrorCode(4212);
	    break;
	  }//if
        } else	{
	  // Return a value from the NDB to the attribute. 
	  tWorkingRecAttr->setNotNULL();
	  const Uint32 sizeInBytes = col->m_attrSize * col->m_arraySize;
	  const Uint32 sizeInWords = (sizeInBytes + 3) / 4;
	  tAI_ElementLen = tAttrSize;
	  tCurrElemPtr = (Uint32*)tWorkingRecAttr->aRef();
	  if (sizeInWords == tAttrSize){
            continue;
          } else {
	    setErrorCode(4201);
	    break;
	  }//if
        }//if
      }//if
    }//for
  }//if
  Uint32 tCurrRecLen = theCurrRecAI_Len;
  Uint32 tTotRecLen = theTotalRecAI_Len;
  theAI_ElementLen = tAI_ElementLen;
  theCurrElemPtr = tCurrElemPtr;
  tCurrRecLen = tCurrRecLen + aLength;
  theCurrRecAI_Len = tCurrRecLen; // Update Current Received AI Length
  if (tTotRecLen == tCurrRecLen){		// Operation completed
    NdbRecAttr* tCurrRecAttr = theCurrentRecAttr;
    theStatus = Finished;
    
    NdbConnection* tNdbCon = theNdbCon;
    if ((tAI_ElementLen == 0) &&
        (tCurrRecAttr == NULL)) {
      NdbRecAttr* tRecAttr = theFirstRecAttr;
      while (tRecAttr != NULL) {
	if (tRecAttr->copyoutRequired())	// copy to application buffer
	  tRecAttr->copyout();
	tRecAttr = tRecAttr->next();
      }
      return tNdbCon->OpCompleteSuccess();
    } else if (tAI_ElementLen != 0) {
      setErrorCode(4213);
      return tNdbCon->OpCompleteFailure();
    } else {
      setErrorCode(4214);
      return tNdbCon->OpCompleteFailure();
    }//if
  } 
  else if ((tCurrRecLen > tTotRecLen) &&
           (tTotRecLen > 0)) { /* == 0 if TCKEYCONF not yet received */
    setErrorCode(4215);
    theStatus = Finished;
    
    return theNdbCon->OpCompleteFailure(); 
  }//if
  return -1;	// Continue waiting for more signals of this operation
}//NdbOperation::receiveREAD_AI()

void
NdbOperation::handleFailedAI_ElemLen()
{
  NdbRecAttr* tRecAttr = theFirstRecAttr;
  while (tRecAttr != NULL) {
    tRecAttr->setUNDEFINED();
    tRecAttr = tRecAttr->next();
  }//while
}//NdbOperation::handleFailedAI_ElemLen()





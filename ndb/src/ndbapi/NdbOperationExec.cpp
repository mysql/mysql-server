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
  NdbApiSignal *tSignal = theTCREQ->next();
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
  
  Uint8 tSimpleIndicator = theSimpleIndicator;
  Uint8 tCommitIndicator = theCommitIndicator;
  Uint8 tStartIndicator = theStartIndicator;
  Uint8 tInterpretIndicator = theInterpretIndicator;

//-------------------------------------------------------------
// Simple state is set if start and commit is set and it is
// a read request. Otherwise it is set to zero.
//-------------------------------------------------------------
  Uint8 tReadInd = (theOperationType == ReadRequest);
  Uint8 tSimpleState = tReadInd & tSimpleIndicator;

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
  tcKeyReq->setInterpretedFlag(tReqInfo, tInterpretIndicator);

  Uint8 tDirtyIndicator = theDirtyIndicator;
  OperationType tOperationType = theOperationType;
  Uint32 tTupKeyLen = theTupKeyLen;
  Uint8 abortOption =
    m_abortOption != -1 ? m_abortOption : theNdbCon->m_abortOption;

  tcKeyReq->setDirtyFlag(tReqInfo, tDirtyIndicator);
  tcKeyReq->setOperationType(tReqInfo, tOperationType);
  tcKeyReq->setKeyLength(tReqInfo, tTupKeyLen);
  
  // A simple read is always ignore error
  abortOption = tSimpleIndicator ? IgnoreError : abortOption;
  tcKeyReq->setAbortOption(tReqInfo, abortOption);
  
  Uint8 tDistrKeyIndicator = theDistrKeyIndicator_;
  Uint8 tScanIndicator = theScanInfo & 1;

  tcKeyReq->setDistributionKeyFlag(tReqInfo, tDistrKeyIndicator);
  tcKeyReq->setScanIndFlag(tReqInfo, tScanIndicator);

  tcKeyReq->requestInfo  = tReqInfo;

//-------------------------------------------------------------
// The next step is to fill in the upto three conditional words.
//-------------------------------------------------------------
  Uint32* tOptionalDataPtr = &tcKeyReq->scanInfo;
  Uint32 tDistrGHIndex = tScanIndicator;
  Uint32 tDistrKeyIndex = tDistrGHIndex;

  Uint32 tScanInfo = theScanInfo;
  Uint32 tDistrKey = theDistributionKey;

  tOptionalDataPtr[0] = tScanInfo;
  tOptionalDataPtr[tDistrKeyIndex] = tDistrKey;

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
    NdbApiSignal* tSignal = theTCREQ->next();
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
  theStatus = WaitResponse;
  theReceiver.prepareSend();
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
    if (insertATTRINFO(Interpreter::EXIT_OK) != -1) {
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
  theReceiver.prepareSend();
  return 0;
}//NdbOperation::prepareSendInterpreted()

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

  AbortOption ao = (AbortOption)
    (m_abortOption != -1 ? m_abortOption : theNdbCon->m_abortOption);
  theReceiver.m_received_result_length = ~0;

  theStatus = Finished;
  // blobs want this
  if (m_abortOption != IgnoreError)
  {
    theNdbCon->theReturnStatus = NdbConnection::ReturnFailure;
  }
  theError.code = aSignal->readData(4);
  theNdbCon->setOperationErrorCodeAbort(aSignal->readData(4), ao);

  if(theOperationType != ReadRequest || !theSimpleIndicator) // not simple read
    return theNdbCon->OpCompleteFailure(ao, m_abortOption != IgnoreError);
  
  /**
   * If TCKEYCONF has arrived
   *   op has completed (maybe trans has completed)
   */
  if(theReceiver.m_expected_result_length)
  {
    return theNdbCon->OpCompleteFailure(AbortOnError);
  }
  
  return -1;
}


void
NdbOperation::handleFailedAI_ElemLen()
{
  NdbRecAttr* tRecAttr = theReceiver.theFirstRecAttr;
  while (tRecAttr != NULL) {
    tRecAttr->setNULL();
    tRecAttr = tRecAttr->next();
  }//while
}//NdbOperation::handleFailedAI_ElemLen()





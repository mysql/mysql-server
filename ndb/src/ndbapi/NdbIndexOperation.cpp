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
#include <NdbIndexOperation.hpp>
#include <NdbResultSet.hpp>
#include <Ndb.hpp>
#include <NdbConnection.hpp>
#include "NdbApiSignal.hpp"
#include <AttributeHeader.hpp>
#include <signaldata/TcIndx.hpp>
#include <signaldata/TcKeyReq.hpp>
#include <signaldata/IndxKeyInfo.hpp>
#include <signaldata/IndxAttrInfo.hpp>

NdbIndexOperation::NdbIndexOperation(Ndb* aNdb) :
  NdbOperation(aNdb),
  m_theIndex(NULL)
{
  m_tcReqGSN = GSN_TCINDXREQ;
  m_attrInfoGSN = GSN_INDXATTRINFO;
  m_keyInfoGSN = GSN_INDXKEYINFO;

  /**
   * Change receiver type
   */
  theReceiver.init(NdbReceiver::NDB_INDEX_OPERATION, this);
}

NdbIndexOperation::~NdbIndexOperation()
{
}

/*****************************************************************************
 * int indxInit();
 *
 * Return Value:  Return 0 : init was successful.
 *                Return -1: In all other case.  
 * Remark:        Initiates operation record after allocation.
 *****************************************************************************/
int
NdbIndexOperation::indxInit(const NdbIndexImpl * anIndex,
			    const NdbTableImpl * aTable, 
			    NdbConnection* myConnection)
{
  NdbOperation::init(aTable, myConnection);

  switch (anIndex->m_type) {
  case(NdbDictionary::Index::UniqueHashIndex):
    break;
  case(NdbDictionary::Index::Undefined):
  case(NdbDictionary::Index::HashIndex):
  case(NdbDictionary::Index::UniqueOrderedIndex):
  case(NdbDictionary::Index::OrderedIndex):
    setErrorCodeAbort(4003);
    return -1;
  }
  m_theIndex = anIndex;
  m_accessTable = anIndex->m_table;
  theNoOfTupKeyLeft = m_accessTable->getNoOfPrimaryKeys();
  return 0;
}

int NdbIndexOperation::readTuple(NdbOperation::LockMode lm)
{ 
  switch(lm) {
  case LM_Read:
    return readTuple();
    break;
  case LM_Exclusive:
    return readTupleExclusive();
    break;
  case LM_CommittedRead:
    return readTuple();
    break;
  default:
    return -1;
  };
}

int NdbIndexOperation::insertTuple()
{
  setErrorCode(4200);
  return -1;
}

int NdbIndexOperation::readTuple()
{
  // First check that index is unique

  return NdbOperation::readTuple();
}

int NdbIndexOperation::readTupleExclusive()
{
  // First check that index is unique

  return NdbOperation::readTupleExclusive();
}

int NdbIndexOperation::simpleRead()
{
  // First check that index is unique

  return NdbOperation::readTuple();
}

int NdbIndexOperation::dirtyRead()
{
  // First check that index is unique

  return NdbOperation::readTuple();
}

int NdbIndexOperation::committedRead()
{
  // First check that index is unique

  return NdbOperation::readTuple();
}

int NdbIndexOperation::updateTuple()
{
  // First check that index is unique

  return NdbOperation::updateTuple();
}

int NdbIndexOperation::deleteTuple()
{
  // First check that index is unique

  return NdbOperation::deleteTuple();
}

int NdbIndexOperation::dirtyUpdate()
{
  // First check that index is unique

  return NdbOperation::dirtyUpdate();
}

int NdbIndexOperation::interpretedUpdateTuple()
{
  // First check that index is unique

  return NdbOperation::interpretedUpdateTuple();
}

int NdbIndexOperation::interpretedDeleteTuple()
{
  // First check that index is unique

  return NdbOperation::interpretedDeleteTuple();
}

int 
NdbIndexOperation::prepareSend(Uint32 aTC_ConnectPtr, Uint64  aTransactionId)
{
  Uint32 tTransId1, tTransId2;
  Uint32 tReqInfo;
  Uint32 tSignalCount = 0;
  Uint32 tInterpretInd = theInterpretIndicator;
  
  theErrorLine = 0;

  if (tInterpretInd != 1) {
    OperationType tOpType = theOperationType;
    OperationStatus tStatus = theStatus;
    if ((tOpType == UpdateRequest) ||
         (tOpType == InsertRequest) ||
         (tOpType == WriteRequest)) {
      if (tStatus != SetValue) {
        setErrorCodeAbort(4506);
        return -1;
      }//if
    } else if ((tOpType == ReadRequest) || (tOpType == ReadExclusive) ||
         (tOpType == DeleteRequest)) {
      if (tStatus != GetValue) {
        setErrorCodeAbort(4506);
        return -1;
      }//if
    } else {
      setErrorCodeAbort(4507);      
      return -1;
    }//if
  } else {
    if (prepareSendInterpreted() == -1) {
      return -1;
    }//if
  }//if
  
//-------------------------------------------------------------
// We start by filling in the first 8 unconditional words of the
// TCINDXREQ signal.
//-------------------------------------------------------------
  TcKeyReq * tcKeyReq = 
    CAST_PTR(TcKeyReq, theTCREQ->getDataPtrSend());

  Uint32 tTotalCurrAI_Len = theTotalCurrAI_Len;
  Uint32 tIndexId = m_theIndex->m_indexId;
  Uint32 tSchemaVersion = m_theIndex->m_version;
  
  tcKeyReq->apiConnectPtr      = aTC_ConnectPtr;
  tcKeyReq->senderData         = ptr2int();
  tcKeyReq->attrLen            = tTotalCurrAI_Len;
  tcKeyReq->tableId            = tIndexId;
  tcKeyReq->tableSchemaVersion = tSchemaVersion;

  tTransId1 = (Uint32) aTransactionId;
  tTransId2 = (Uint32) (aTransactionId >> 32);
  
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
  //theNdbCon->theSimpleState = tSimpleState;

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
  Uint32 tIndexLen = theTupKeyLen;
  Uint8 abortOption = theNdbCon->m_abortOption;

  tcKeyReq->setDirtyFlag(tReqInfo, tDirtyIndicator);
  tcKeyReq->setOperationType(tReqInfo, tOperationType);
  tcKeyReq->setKeyLength(tReqInfo, tIndexLen);
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
  if (tIndexLen > 4) {
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
// Finally we also compress the INDXATTRINFO part of the signal.
// We optimise by using the if-statement for sending INDXKEYINFO
// signals to calculating the new Attrinfo Index.
//-------------------------------------------------------------
  Uint32 tAttrInfoIndex;  

  if (tIndexLen > TcKeyReq::MaxKeyInfo) {
    /**
     *	Set transid and TC connect ptr in the INDXKEYINFO signals
     */
    NdbApiSignal* tSignal = theTCREQ->next();
    Uint32 remainingKey = tIndexLen - TcKeyReq::MaxKeyInfo;

    do {
      Uint32* tSigDataPtr = tSignal->getDataPtrSend();
      NdbApiSignal* tnextSignal = tSignal->next();
      tSignalCount++;
      tSigDataPtr[0] = aTC_ConnectPtr;
      tSigDataPtr[1] = tTransId1;
      tSigDataPtr[2] = tTransId2;
      if (remainingKey > IndxKeyInfo::DataLength) {
	// The signal is full
	tSignal->setLength(IndxKeyInfo::MaxSignalLength);
	remainingKey -= IndxKeyInfo::DataLength;
      }
      else {
	// Last signal
	tSignal->setLength(IndxKeyInfo::HeaderLength + remainingKey);
	remainingKey = 0;
      }
      tSignal = tnextSignal;
    } while (tSignal != NULL);
    tAttrInfoIndex = tKeyIndex + TcKeyReq::MaxKeyInfo;
  } else {
    tAttrInfoIndex = tKeyIndex + tIndexLen;
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
*  Send the INDXATTRINFO signals.
***************************************************/
  if (tTotalCurrAI_Len > 5) {
    // Set the last signal's length.
    NdbApiSignal* tSignal = theFirstATTRINFO;
    theCurrentATTRINFO->setLength(theAI_LenInCurrAI);
    do {
      Uint32* tSigDataPtr = tSignal->getDataPtrSend();
      NdbApiSignal* tnextSignal = tSignal->next();
      tSignalCount++;
      tSigDataPtr[0] = aTC_ConnectPtr;
      tSigDataPtr[1] = tTransId1;
      tSigDataPtr[2] = tTransId2;
      tSignal = tnextSignal;
    } while (tSignal != NULL);
  }//if
  theStatus = WaitResponse;
  theReceiver.prepareSend();
  return 0;
}

/***************************************************************************
int receiveTCINDXREF( NdbApiSignal* aSignal)

Return Value:   Return 0 : send was succesful.
                Return -1: In all other case.   
Parameters:     aSignal: the signal object that contains the TCINDXREF signal from TC.
Remark:         Handles the reception of the TCKEYREF signal.
***************************************************************************/
int
NdbIndexOperation::receiveTCINDXREF( NdbApiSignal* aSignal)
{
  return NdbOperation::receiveTCKEYREF(aSignal);
}//NdbIndexOperation::receiveTCINDXREF()

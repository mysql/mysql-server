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
  m_theIndex(NULL),
  m_theIndexLen(0),
  m_theNoOfIndexDefined(0)
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
  m_theIndexLen = 0;
  m_theNoOfIndexDefined = 0;
  for (Uint32 i=0; i<NDB_MAX_ATTRIBUTES_IN_INDEX; i++)
    for (int j=0; j<3; j++)
      m_theIndexDefined[i][j] = false;  
  
  TcIndxReq * const tcIndxReq = CAST_PTR(TcIndxReq, theTCREQ->getDataPtrSend());
  tcIndxReq->scanInfo = 0;
  theKEYINFOptr = &tcIndxReq->keyInfo[0];
  theATTRINFOptr = &tcIndxReq->attrInfo[0];
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

int NdbIndexOperation::equal_impl(const NdbColumnImpl* tAttrInfo, 
                                  const char* aValuePassed, 
                                  Uint32 aVariableKeyLen)
{
  register Uint32 tAttrId;
  
  Uint32 tData;
  Uint32 tKeyInfoPosition;
  const char* aValue = aValuePassed;
  Uint32 xfrmData[1024];
  Uint32 tempData[1024];
  
  if ((theStatus == OperationDefined) &&
      (aValue != NULL) &&
      (tAttrInfo != NULL )) {
    /************************************************************************
     *	Start by checking that the attribute is an index key. 
     *      This value is also the word order in the tuple key of this 
     *      tuple key attribute. 
     *      Then check that this tuple key has not already been defined. 
     *      Finally check if all tuple key attributes have been defined. If
     *	this is true then set Operation state to tuple key defined.
     ************************************************************************/
    tAttrId = tAttrInfo->m_attrId;
    tKeyInfoPosition = tAttrInfo->m_keyInfoPos;
    Uint32 i = 0;
    
    // Check that the attribute is part if the index attributes
    // by checking if it is a primary key attribute of index table
    if (tAttrInfo->m_pk) {
      Uint32 tKeyDefined = theTupleKeyDefined[0][2];
      Uint32 tKeyAttrId = theTupleKeyDefined[0][0];
      do {
	if (tKeyDefined == false) {
	  goto keyEntryFound;
	} else {
	  if (tKeyAttrId != tAttrId) {
	    /******************************************************************
	     * We read the key defined variable in advance. 
	     * It could potentially read outside its area when 
	     * i = MAXNROFTUPLEKEY - 1, 
	     * it is not a problem as long as the variable 
	     * theTupleKeyDefined is defined
	     * in the middle of the object. 
	     * Reading wrong data and not using it causes no problems.
	     *****************************************************************/
	    i++;
	    tKeyAttrId = theTupleKeyDefined[i][0];
	    tKeyDefined = theTupleKeyDefined[i][2];
	    continue;
	  } else {
	    goto equal_error2;
	  }//if
	}//if
      } while (i < NDB_MAX_ATTRIBUTES_IN_INDEX);
      goto equal_error2;
    } else {
      goto equal_error1;
    }
    /**************************************************************************
     *	Now it is time to retrieve the tuple key data from the pointer supplied
     *      by the application. 
     *      We have to retrieve the size of the attribute in words and bits.
     *************************************************************************/
  keyEntryFound:
    m_theIndexDefined[i][0] = tAttrId;
    m_theIndexDefined[i][1] = tKeyInfoPosition; 
    m_theIndexDefined[i][2] = true;

    Uint32 sizeInBytes = tAttrInfo->m_attrSize * tAttrInfo->m_arraySize;
    const char* aValueToWrite = aValue;

    CHARSET_INFO* cs = tAttrInfo->m_cs;
    if (cs != 0) {
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

    Uint32 bitsInLastWord = 8 * (sizeInBytes & 3) ;
    Uint32 totalSizeInWords = (sizeInBytes + 3)/4;// Inc. bits in last word
    Uint32 sizeInWords = sizeInBytes / 4;         // Exc. bits in last word

    if (true){ //tArraySize != 0) {
      Uint32 tIndexLen = m_theIndexLen;

      m_theIndexLen = tIndexLen + totalSizeInWords;
      if ((aVariableKeyLen == sizeInBytes) ||
	  (aVariableKeyLen == 0)) {
	;
      } else {
	goto equal_error3;
      }
    }
#if 0
    else {
      /************************************************************************
       * The attribute is a variable array. We need to use the length parameter
       * to know the size of this attribute in the key information and 
       * variable area. A key is however not allowed to be larger than 4 
       * kBytes and this is checked for variable array attributes
       * used as keys.
       ***********************************************************************/
      Uint32 tMaxVariableKeyLenInWord = (MAXTUPLEKEYLENOFATTERIBUTEINWORD -
					 tKeyInfoPosition);
      tAttrSizeInBits = aVariableKeyLen << 3;
      tAttrSizeInWords = tAttrSizeInBits >> 5;
      tAttrBitsInLastWord = tAttrSizeInBits - (tAttrSizeInWords << 5);
      tAttrLenInWords = ((tAttrSizeInBits + 31) >> 5);
      if (tAttrLenInWords > tMaxVariableKeyLenInWord) {
	setErrorCodeAbort(4207);
	return -1;
      }//if
      m_theIndexLen = m_theIndexLen + tAttrLenInWords;
    }//if
#endif

    /*************************************************************************
     *	Check if the pointer of the value passed is aligned on a 4 byte 
     *      boundary. If so only assign the pointer to the internal variable 
     *      aValue. If it is not aligned then we start by copying the value to 
     *      tempData and use this as aValue instead.
     *************************************************************************/
    const int attributeSize = sizeInBytes;
    const int slack = sizeInBytes & 3;
    int tDistrKey = tAttrInfo->m_distributionKey;
    int tDistrGroup = tAttrInfo->m_distributionGroup;
    if ((((UintPtr)aValue & 3) != 0) || (slack != 0)){
      memcpy(&tempData[0], aValue, attributeSize);
      aValue = (char*)&tempData[0];
      if(slack != 0) {
	char * tmp = (char*)&tempData[0];
	memset(&tmp[attributeSize], 0, (4 - slack));
      }//if
    }//if
    OperationType tOpType = theOperationType;
    if ((tDistrKey != 1) && (tDistrGroup != 1)) {
      ;
    } else if (tDistrKey == 1) {
      theDistrKeySize += totalSizeInWords;
      theDistrKeyIndicator = 1;
    } else {
      Uint32 TsizeInBytes = sizeInBytes;
      Uint32 TbyteOrderFix = 0;
      char*   TcharByteOrderFix = (char*)&TbyteOrderFix;
      if (tAttrInfo->m_distributionGroupBits == 8) {
	char tFirstChar = aValue[TsizeInBytes - 2];
	char tSecondChar = aValue[TsizeInBytes - 2];
	TcharByteOrderFix[0] = tFirstChar;
	TcharByteOrderFix[1] = tSecondChar;
	TcharByteOrderFix[2] = 0x30;
	TcharByteOrderFix[3] = 0x30;
	theDistrGroupType = 0;
      } else {
	TbyteOrderFix = ((aValue[TsizeInBytes - 2] - 0x30) * 10) 
	  + (aValue[TsizeInBytes - 1] - 0x30);
	theDistrGroupType = 1;
      }//if
      theDistributionGroup = TbyteOrderFix;
      theDistrGroupIndicator = 1;
    }//if
    /**************************************************************************
     *	If the operation is an insert request and the attribute is stored then
     *      we also set the value in the stored part through putting the 
     *      information in the INDXATTRINFO signals.
     *************************************************************************/
    if ((tOpType == InsertRequest) ||
	(tOpType == WriteRequest)) {
      if (!tAttrInfo->m_indexOnly){
        // invalid data can crash kernel
        if (cs != NULL &&
            (*cs->cset->well_formed_len)(cs,
                                         aValueToWrite,
                                         aValueToWrite + sizeInBytes,
                                         sizeInBytes) != sizeInBytes)
          goto equal_error4;
	Uint32 ahValue;
	Uint32 sz = totalSizeInWords;
	AttributeHeader::init(&ahValue, tAttrId, sz);
	insertATTRINFO( ahValue );
	insertATTRINFOloop((Uint32*)aValueToWrite, sizeInWords);
	if (bitsInLastWord != 0) {
	  tData = *(Uint32*)(aValueToWrite + (sizeInWords << 2));
	  tData = convertEndian(tData);
	  tData = tData & ((1 << bitsInLastWord) - 1);
	  tData = convertEndian(tData);
	  insertATTRINFO( tData );
	}//if
      }//if
    }//if

    /**************************************************************************
     *	Store the Key information in the TCINDXREQ and INDXKEYINFO signals. 
     *************************************************************************/
    if (insertKEYINFO(aValue, tKeyInfoPosition, 
		      totalSizeInWords, bitsInLastWord) != -1) {
      /************************************************************************
       * Add one to number of tuple key attributes defined. 
       * If all have been defined then set the operation state to indicate 
       * that tuple key is defined. 
       * Thereby no more search conditions are allowed in this version.
       ***********************************************************************/
      Uint32 tNoIndexDef = m_theNoOfIndexDefined;
      Uint32 tErrorLine = theErrorLine;
      int tNoIndexAttrs = m_theIndex->m_columns.size();
      unsigned char tInterpretInd = theInterpretIndicator;
      tNoIndexDef++;
      m_theNoOfIndexDefined = tNoIndexDef;
      tErrorLine++;
      theErrorLine = tErrorLine;
      if (int(tNoIndexDef) == tNoIndexAttrs) {
	if (tOpType == UpdateRequest) {
	  if (tInterpretInd == 1) {
	    theStatus = GetValue;
	  } else {
	    theStatus = SetValue;
	  }//if
	  return 0;
	} else if ((tOpType == ReadRequest) || (tOpType == DeleteRequest) ||
		   (tOpType == ReadExclusive)) {
	  theStatus = GetValue;
          // create blob handles automatically
          if (tOpType == DeleteRequest && m_currentTable->m_noOfBlobs != 0) {
            for (unsigned i = 0; i < m_currentTable->m_columns.size(); i++) {
              NdbColumnImpl* c = m_currentTable->m_columns[i];
              assert(c != 0);
              if (c->getBlobType()) {
                if (getBlobHandle(theNdbCon, c) == NULL)
                  return -1;
              }
            }
          }
	  return 0;
	} else if ((tOpType == InsertRequest) || (tOpType == WriteRequest)) {
	  theStatus = SetValue;
	  return 0;
	} else {
	  setErrorCodeAbort(4005);
	  return -1;
	}//if
      }//if
      return 0;
    } else {
     
      return -1;
    }//if
  } else {
    if (theStatus != OperationDefined) {
      return -1;
    }//if

    if (aValue == NULL) {
      setErrorCodeAbort(4505);
      return -1;
    }//if
    
    if ( tAttrInfo == NULL ) {      
      setErrorCodeAbort(4004);
      return -1;
    }//if
  }//if
  return -1;

 equal_error1:
  setErrorCodeAbort(4205);
  return -1;

 equal_error2:
  setErrorCodeAbort(4206);
  return -1;

 equal_error3:
  setErrorCodeAbort(4209);
  return -1;
 
 equal_error4:
  setErrorCodeAbort(744);
  return -1;
}

int NdbIndexOperation::executeCursor(int aProcessorId)
{
  printf("NdbIndexOperation::executeCursor NYI\n");
  // NYI
  return -1;
}
void
NdbIndexOperation::setLastFlag(NdbApiSignal* signal, Uint32 lastFlag)
{
  TcIndxReq * const req = CAST_PTR(TcIndxReq, signal->getDataPtrSend());
  TcKeyReq::setExecuteFlag(req->requestInfo, lastFlag);
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
  TcIndxReq * const tcIndxReq = 
    CAST_PTR(TcIndxReq, theTCREQ->getDataPtrSend());

  Uint32 tTotalCurrAI_Len = theTotalCurrAI_Len;
  Uint32 tIndexId = m_theIndex->m_indexId;
  Uint32 tSchemaVersion = m_theIndex->m_version;
  
  tcIndxReq->apiConnectPtr      = aTC_ConnectPtr;
  tcIndxReq->senderData         = ptr2int();
  tcIndxReq->attrLen            = tTotalCurrAI_Len;
  tcIndxReq->indexId            = tIndexId;
  tcIndxReq->indexSchemaVersion = tSchemaVersion;

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

  tcIndxReq->transId1           = tTransId1;
  tcIndxReq->transId2           = tTransId2;
  
  tReqInfo = 0;

  if (tTotalCurrAI_Len <= TcIndxReq::MaxAttrInfo) {
    tcIndxReq->setAIInTcIndxReq(tReqInfo, tTotalCurrAI_Len);
  } else {
    tcIndxReq->setAIInTcIndxReq(tReqInfo, TcIndxReq::MaxAttrInfo);
  }//if

  tcIndxReq->setSimpleFlag(tReqInfo, tSimpleIndicator);
  tcIndxReq->setCommitFlag(tReqInfo, tCommitIndicator);
  tcIndxReq->setStartFlag(tReqInfo, tStartIndicator);
  const Uint8 tInterpretIndicator = theInterpretIndicator;
  tcIndxReq->setInterpretedFlag(tReqInfo, tInterpretIndicator);

  Uint8 tDirtyIndicator = theDirtyIndicator;
  OperationType tOperationType = theOperationType;
  Uint32 tIndexLen = m_theIndexLen;
  Uint8 abortOption = theNdbCon->m_abortOption;

  tcIndxReq->setDirtyFlag(tReqInfo, tDirtyIndicator);
  tcIndxReq->setOperationType(tReqInfo, tOperationType);
  tcIndxReq->setIndexLength(tReqInfo, tIndexLen);
  tcIndxReq->setCommitType(tReqInfo, abortOption);
  
  Uint8 tDistrKeyIndicator = theDistrKeyIndicator;
  Uint8 tDistrGroupIndicator = theDistrGroupIndicator;
  Uint8 tDistrGroupType = theDistrGroupType;
  Uint8 tScanIndicator = theScanInfo & 1;

  tcIndxReq->setDistributionGroupFlag(tReqInfo, tDistrGroupIndicator);
  tcIndxReq->setDistributionGroupTypeFlag(tReqInfo, tDistrGroupType);
  tcIndxReq->setDistributionKeyFlag(tReqInfo, tDistrKeyIndicator);
  tcIndxReq->setScanIndFlag(tReqInfo, tScanIndicator);

  tcIndxReq->requestInfo  = tReqInfo;

//-------------------------------------------------------------
// The next step is to fill in the upto three conditional words.
//-------------------------------------------------------------
  Uint32* tOptionalDataPtr = &tcIndxReq->scanInfo;
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
  Uint32 Tdata1 = tcIndxReq->keyInfo[0];
  Uint32 Tdata2 = tcIndxReq->keyInfo[1];
  Uint32 Tdata3 = tcIndxReq->keyInfo[2];
  Uint32 Tdata4 = tcIndxReq->keyInfo[3];
  Uint32 Tdata5;

  tKeyDataPtr[0] = Tdata1;
  tKeyDataPtr[1] = Tdata2;
  tKeyDataPtr[2] = Tdata3;
  tKeyDataPtr[3] = Tdata4;
  if (tIndexLen > 4) {
    Tdata1 = tcIndxReq->keyInfo[4];
    Tdata2 = tcIndxReq->keyInfo[5];
    Tdata3 = tcIndxReq->keyInfo[6];
    Tdata4 = tcIndxReq->keyInfo[7];

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

  if (tIndexLen > TcIndxReq::MaxKeyInfo) {
    /**
     *	Set transid and TC connect ptr in the INDXKEYINFO signals
     */
    NdbApiSignal* tSignal = theFirstKEYINFO;
    Uint32 remainingKey = tIndexLen - TcIndxReq::MaxKeyInfo;

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
    tAttrInfoIndex = tKeyIndex + TcIndxReq::MaxKeyInfo;
  } else {
    tAttrInfoIndex = tKeyIndex + tIndexLen;
  }//if

//-------------------------------------------------------------
// Perform the Attrinfo packing in the TCKEYREQ signal started
// above.
//-------------------------------------------------------------
  Uint32* tAIDataPtr = &tOptionalDataPtr[tAttrInfoIndex];
  Tdata1 = tcIndxReq->attrInfo[0];
  Tdata2 = tcIndxReq->attrInfo[1];
  Tdata3 = tcIndxReq->attrInfo[2];
  Tdata4 = tcIndxReq->attrInfo[3];
  Tdata5 = tcIndxReq->attrInfo[4];

  theTCREQ->setLength(tcIndxReq->getAIInTcIndxReq(tReqInfo) + 
                      tAttrInfoIndex + TcIndxReq::StaticLength);
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

void NdbIndexOperation::closeScan()
{
  printf("NdbIndexOperation::closeScan NYI\n");
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
  const TcIndxRef * const tcIndxRef = CAST_CONSTPTR(TcIndxRef, aSignal->getDataPtr());

  if (checkState_TransId(aSignal) == -1) {
    return -1;
  }//if

  theStatus = Finished;
  
  theNdbCon->theReturnStatus = NdbConnection::ReturnFailure;
  Uint32 errorCode = tcIndxRef->errorCode;
  theError.code = errorCode;
  theNdbCon->setOperationErrorCodeAbort(errorCode);
  return theNdbCon->OpCompleteFailure(theNdbCon->m_abortOption);
}//NdbIndexOperation::receiveTCINDXREF()




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


/******************************************************************************
Name:          NdbOperationSearch.C
Include:
Link:
Author:        UABMNST Mona Natterkvist UAB/B/SD                         
Date:          970829
Version:       0.1
Description:   Interface between TIS and NDB
Documentation:
Adjust:  971022  UABMNST   First version.
	 971206  UABRONM
 *****************************************************************************/
#include "API.hpp"

#include <NdbOperation.hpp>
#include "NdbApiSignal.hpp"
#include <NdbConnection.hpp>
#include <Ndb.hpp>
#include "NdbImpl.hpp"
#include <NdbOut.hpp>

#include <AttributeHeader.hpp>
#include <signaldata/TcKeyReq.hpp>
#include <signaldata/KeyInfo.hpp>
#include "NdbDictionaryImpl.hpp"
#include <md5_hash.hpp>

/******************************************************************************
CondIdType equal(const char* anAttrName, char* aValue, Uint32 aVarKeylen);

Return Value    Return 0 : Equal was successful.
                Return -1: In all other case. 
Parameters:     anAttrName : Attribute name for search condition..
                aValue : Referense to the search value.
		aVariableKeylen : The length of key in bytes  
Remark:         Defines search condition with equality anAttrName.
******************************************************************************/
int
NdbOperation::equal_impl(const NdbColumnImpl* tAttrInfo, 
                         const char* aValuePassed, 
                         Uint32 aVariableKeyLen)
{
  register Uint32 tAttrId;
  
  Uint32 tData;
  Uint32 tKeyInfoPosition;
  const char* aValue = aValuePassed;
  Uint64 xfrmData[512];
  Uint64 tempData[512];

  if ((theStatus == OperationDefined) &&
      (aValue != NULL) &&
      (tAttrInfo != NULL )) {
/******************************************************************************
 *	Start by checking that the attribute is a tuple key. 
 *      This value is also the word order in the tuple key of this 
 *      tuple key attribute. 
 *      Then check that this tuple key has not already been defined. 
 *      Finally check if all tuple key attributes have been defined. If
 *	this is true then set Operation state to tuple key defined.
 *****************************************************************************/
    tAttrId = tAttrInfo->m_attrId;
    tKeyInfoPosition = tAttrInfo->m_keyInfoPos;
    bool tDistrKey = tAttrInfo->m_distributionKey;

    Uint32 i = 0;
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
      } while (i < NDB_MAX_NO_OF_ATTRIBUTES_IN_KEY);
      goto equal_error2;
    } else {
      goto equal_error1;
    }
    /*************************************************************************
     *	Now it is time to retrieve the tuple key data from the pointer supplied
     *      by the application. 
     *      We have to retrieve the size of the attribute in words and bits.
     *************************************************************************/
  keyEntryFound:
    theTupleKeyDefined[i][0] = tAttrId;
    theTupleKeyDefined[i][1] = tKeyInfoPosition; 
    theTupleKeyDefined[i][2] = true;

    Uint32 sizeInBytes = tAttrInfo->m_attrSize * tAttrInfo->m_arraySize;
    {
      /************************************************************************
       * Check if the pointer of the value passed is aligned on a 4 byte 
       * boundary. If so only assign the pointer to the internal variable 
       * aValue. If it is not aligned then we start by copying the value to 
       * tempData and use this as aValue instead.
       ***********************************************************************/
      const int attributeSize = sizeInBytes;
      const int slack = sizeInBytes & 3;
      const int align = UintPtr(aValue) & 7;

      if (((align & 3) != 0) || (slack != 0) || (tDistrKey && (align != 0)))
      {
	((Uint32*)tempData)[attributeSize >> 2] = 0;
	memcpy(&tempData[0], aValue, attributeSize);
	aValue = (char*)&tempData[0];
      }//if
    }
    const char* aValueToWrite = aValue;
    
    CHARSET_INFO* cs = tAttrInfo->m_cs;
    if (cs != 0) {
      // current limitation: strxfrm does not increase length
      assert(cs->strxfrm_multiply <= 1);
      ((Uint32*)xfrmData)[sizeInBytes >> 2] = 0;
      unsigned n =
      (*cs->coll->strnxfrm)(cs,
                            (uchar*)xfrmData, sizeof(xfrmData),
                            (const uchar*)aValue, sizeInBytes);
      while (n < sizeInBytes)
        ((uchar*)xfrmData)[n++] = 0x20;
      aValue = (char*)xfrmData;
    }

    Uint32 totalSizeInWords = (sizeInBytes + 3)/4; // Inc. bits in last word
    
    if (true){ //tArraySize != 0) {
      Uint32 tTupKeyLen = theTupKeyLen;
      
      theTupKeyLen = tTupKeyLen + totalSizeInWords;
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
       ************************************************************************/
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
      theTupKeyLen = theTupKeyLen + tAttrLenInWords;
    }//if
#endif

    OperationType tOpType = theOperationType;
    /**************************************************************************
     *	If the operation is an insert request and the attribute is stored then
     *      we also set the value in the stored part through putting the 
     *      information in the ATTRINFO signals.
     *************************************************************************/
    if ((tOpType == InsertRequest) ||
	(tOpType == WriteRequest)) {
      // invalid data can crash kernel
      if (cs != NULL &&
	  (*cs->cset->well_formed_len)(cs,
				       aValueToWrite,
				       aValueToWrite + sizeInBytes,
				       sizeInBytes) != sizeInBytes)
	goto equal_error4;
      Uint32 ahValue;
      const Uint32 sz = totalSizeInWords;

      // XXX
      if(m_accessTable == m_currentTable)
      {
	AttributeHeader::init(&ahValue, tAttrId, sz);
      }
      else
      {
	assert(m_accessTable->m_index);
	int attr_id_current_table = 
	  m_accessTable->m_index->m_columns[tAttrId]->m_keyInfoPos;
	AttributeHeader::init(&ahValue, attr_id_current_table, sz);
      }
      
      insertATTRINFO( ahValue );
      insertATTRINFOloop((Uint32*)aValueToWrite, sz);
    }//if
    
    /**************************************************************************
     *	Store the Key information in the TCKEYREQ and KEYINFO signals. 
     *************************************************************************/
    if (insertKEYINFO(aValue, tKeyInfoPosition, totalSizeInWords) != -1) {
      /************************************************************************
       * Add one to number of tuple key attributes defined. 
       * If all have been defined then set the operation state to indicate 
       * that tuple key is defined. 
       * Thereby no more search conditions are allowed in this version.
       ***********************************************************************/
      Uint32 tNoKeysDef = theNoOfTupKeyLeft - 1;
      Uint32 tErrorLine = theErrorLine;
      unsigned char tInterpretInd = theInterpretIndicator;
      theNoOfTupKeyLeft = tNoKeysDef;
      tErrorLine++;
      theErrorLine = tErrorLine;

      if(tDistrKey)
	handle_distribution_key((Uint64*)aValue, totalSizeInWords);
      
      if (tNoKeysDef == 0) {	
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
	return 0;
      }//if
    } else {
      return -1;
    }//if
    return 0;
  }
  
  if (aValue == NULL) {
    // NULL value in primary key
    setErrorCodeAbort(4505);
    return -1;
  }//if
  
  if ( tAttrInfo == NULL ) {      
    // Attribute name not found in table
    setErrorCodeAbort(4004);
    return -1;
  }//if

  if (theStatus == GetValue || theStatus == SetValue){
    // All pk's defined
    setErrorCodeAbort(4225);
    return -1;
  }//if

  ndbout_c("theStatus: %d", theStatus);
  
  // If we come here, set a general errorcode
  // and exit
  setErrorCodeAbort(4200);
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

/******************************************************************************
 * Uint64 setTupleId( void )
 *
 * Return Value:  Return > 0: OK 
 *                Return 0 : setTupleId failed
 * Parameters:     
 * Remark:
 *****************************************************************************/
Uint64
NdbOperation::setTupleId()
{
  if (theStatus != OperationDefined)
  {
    return 0;
  }
  Uint64 tTupleId = theNdb->getTupleIdFromNdb(m_currentTable->m_tableId);
  if (tTupleId == ~(Uint64)0){
    setErrorCodeAbort(theNdb->theError.code);
    return 0;
  }
  if (equal((Uint32)0, tTupleId) == -1)
    return 0;

  return tTupleId;
}

/******************************************************************************
 * int insertKEYINFO(const char* aValue, aStartPosition, 
 *                   anAttrSizeInWords, Uint32 anAttrBitsInLastWord);
 *
 * Return Value:   Return 0 : insertKEYINFO was succesful.
 *                 Return -1: In all other case.   
 * Parameters:     aValue: the data to insert into KEYINFO.
 *    		   aStartPosition : Start position for Tuplekey in 
 *                                  KEYINFO (TCKEYREQ).
 *                 aKeyLenInByte : Length of tuplekey or part of tuplekey
 *                 anAttrBitsInLastWord : Nr of bits in last word. 
 * Remark:         Puts the the data into either TCKEYREQ signal 
 *                 or KEYINFO signal.
 *****************************************************************************/
int
NdbOperation::insertKEYINFO(const char* aValue,
			    register Uint32 aStartPosition,
			    register Uint32 anAttrSizeInWords)
{
  NdbApiSignal* tSignal;
  NdbApiSignal* tCurrentKEYINFO;
  //register NdbApiSignal* tTCREQ = theTCREQ;
  register Uint32 tAttrPos;
  Uint32 tPosition;
  Uint32 tEndPos;
  Uint32 tPos;
  Uint32 signalCounter;
  Uint32 tData;

/*****************************************************************************
 *	Calculate the end position of the attribute in the key information.  *
 *	Since the first attribute starts at position one we need to subtract *
 *	one to get the correct end position.				     *
 *	We must also remember the last word with only partial information.   *
 *****************************************************************************/
  tEndPos = aStartPosition + anAttrSizeInWords - 1;

  if ((tEndPos < 9)) {
    register Uint32 tkeyData = *(Uint32*)aValue;
    //TcKeyReq* tcKeyReq = CAST_PTR(TcKeyReq, tTCREQ->getDataPtrSend());
    register Uint32* tDataPtr = (Uint32*)aValue;
    tAttrPos = 1;
    register Uint32* tkeyDataPtr = theKEYINFOptr + aStartPosition - 1;
    // (Uint32*)&tcKeyReq->keyInfo[aStartPosition - 1];
    do {
      tDataPtr++;
      *tkeyDataPtr = tkeyData;
      if (tAttrPos < anAttrSizeInWords) {
        ;
      } else {
        return 0;
      }//if
      tkeyData = *tDataPtr;
      tkeyDataPtr++;
      tAttrPos++;
    } while (1);
    return 0;
  }//if
/*****************************************************************************
 *	Allocate all the KEYINFO signals needed for this key before starting *
 *	to fill the signals with data. This simplifies error handling and    *
 *      avoids duplication of code.					     *
 *****************************************************************************/
  tAttrPos = 0;
  signalCounter = 1;
  while(tEndPos > theTotalNrOfKeyWordInSignal)
  {
    tSignal = theNdb->getSignal();
    if (tSignal == NULL)
    {
      setErrorCodeAbort(4000);
      return -1;
    }
    if (tSignal->setSignal(m_keyInfoGSN) == -1)
    {
      setErrorCodeAbort(4001);
      return -1;
    }
    if (theTCREQ->next() != NULL)
       theLastKEYINFO->next(tSignal);
    else
      theTCREQ->next(tSignal);

    theLastKEYINFO = tSignal;
    theLastKEYINFO->next(NULL);
    theTotalNrOfKeyWordInSignal += 20;
  }

/*****************************************************************************
 *	Change to variable tPosition for more appropriate naming of rest of  *
 *	the code. We must set up current KEYINFO already here if the last    *
 *	word is a word which is set at LastWordLabel and at the same time    *
 *	this is the first word in a KEYINFO signal.			     *
 *****************************************************************************/
  tPosition = aStartPosition;
  tCurrentKEYINFO = theTCREQ->next();
 
/*****************************************************************************
 *	Start by filling up Key information in the 8 words allocated in the  *
 *	TC[KEY/INDX]REQ signal.						     *
 *****************************************************************************/
  while (tPosition < 9)
  {
    theKEYINFOptr[tPosition-1] = * (Uint32*)(aValue + (tAttrPos << 2));
    tAttrPos++;
    if (anAttrSizeInWords == tAttrPos)
      goto LastWordLabel;
    tPosition++;
  }

/*****************************************************************************
 *	We must set up the start position of the writing of Key information  *
 *	before we start the writing of KEYINFO signals. If the start is not  *
 *	the first word of the first KEYINFO signals then we must step forward*
 *	to the proper KEYINFO signal and set the signalCounter properly.     *
 *	signalCounter is set to the actual position in the signal ( = 4 for  *
 *	first key word in KEYINFO signal.				     *
 *****************************************************************************/
  tPos = 8;
  while ((tPosition - tPos) > 20)
  {
    tCurrentKEYINFO = tCurrentKEYINFO->next();
    tPos += 20;
  }
  signalCounter = tPosition - tPos + 3;    

/*****************************************************************************
 *	The loop that actually fills in the Key information into the KEYINFO *
 *	signals. Can be optimised by writing larger chunks than 4 bytes at a *
 *	time.								     *
 *****************************************************************************/
  do
  {
    if (signalCounter > 23)
    {
      tCurrentKEYINFO = tCurrentKEYINFO->next();
      signalCounter = 4;
    }
    tCurrentKEYINFO->setData(*(Uint32*)(aValue + (tAttrPos << 2)), 
			     signalCounter);
    tAttrPos++;
    if (anAttrSizeInWords == tAttrPos)
      goto LastWordLabel;
    tPosition++;
    signalCounter++;
  } while (1);

LastWordLabel:
  return 0;
}

int
NdbOperation::getKeyFromTCREQ(Uint32* data, unsigned size)
{
  assert(m_accessTable != 0 && m_accessTable->m_keyLenInWords != 0);
  assert(m_accessTable->m_keyLenInWords == size);
  unsigned pos = 0;
  while (pos < 8 && pos < size) {
    data[pos] = theKEYINFOptr[pos];
    pos++;
  }
  NdbApiSignal* tSignal = theTCREQ->next();
  unsigned n = 0;
  while (pos < size) {
    if (n == 20) {
      tSignal = tSignal->next();
      n = 0;
    }
    data[pos++] = tSignal->getDataPtrSend()[3 + n++];
  }
  return 0;
}

int
NdbOperation::handle_distribution_key(const Uint64* value, Uint32 len)
{
  if(theDistrKeyIndicator_ == 1 || 
     (theNoOfTupKeyLeft > 0 && m_accessTable->m_noOfDistributionKeys > 1))
  {
    return 0;
  }
  
  if(m_accessTable->m_noOfDistributionKeys == 1)
  {
    setPartitionHash(value, len);
  }
  else 
  {
    /**
     * Copy distribution key to linear memory
     */
    NdbColumnImpl* const * cols = m_accessTable->m_columns.getBase();
    Uint32 len = 0;
    Uint64 tmp[1000];

    Uint32 chunk = 8;
    Uint32* dst = (Uint32*)tmp;
    NdbApiSignal* tSignal = theTCREQ;
    Uint32* src = ((TcKeyReq*)tSignal->getDataPtrSend())->keyInfo;
    if(tSignal->readSignalNumber() == GSN_SCAN_TABREQ)
    {
      tSignal = tSignal->next();
      src = ((KeyInfo*)tSignal->getDataPtrSend())->keyData;
      chunk = KeyInfo::DataLength;
    }

    for(unsigned i = m_accessTable->m_columns.size(); i>0; cols++, i--)
    {
      if (!(* cols)->getPrimaryKey())
	continue;
      
      NdbColumnImpl* tAttrInfo = * cols;
      Uint32 sizeInBytes = tAttrInfo->m_attrSize * tAttrInfo->m_arraySize;
      Uint32 currLen = (sizeInBytes + 3) >> 2;
      if (tAttrInfo->getDistributionKey())
      {
	while (currLen >= chunk)
	{
	  memcpy(dst, src, 4*chunk);
	  dst += chunk;
	  tSignal = tSignal->next();
	  src = ((KeyInfo*)tSignal->getDataPtrSend())->keyData;
	  currLen -= chunk;
	  chunk = KeyInfo::DataLength;
	}

	memcpy(dst, src, 4*currLen);
	dst += currLen;
	src += currLen;
	chunk -= currLen;
      }
      else
      {
	while (currLen >= chunk)
	{
	  tSignal = tSignal->next();
	  src = ((KeyInfo*)tSignal->getDataPtrSend())->keyData;
	  currLen -= chunk;
	  chunk = KeyInfo::DataLength;
	}
	
	src += currLen;
	chunk -= currLen;
      }
    }
    setPartitionHash(tmp, dst - (Uint32*)tmp);
  }
  return 0;
}

void
NdbOperation::setPartitionHash(Uint32 value)
{
  union {
    Uint32 tmp32;
    Uint64 tmp64;
  };

  tmp32 = value;
  setPartitionHash(&tmp64, 1);
}

void
NdbOperation::setPartitionHash(const Uint64* value, Uint32 len)
{
  Uint32 buf[4];
  md5_hash(buf, value, len);
  setPartitionId(buf[1]);
}

void
NdbOperation::setPartitionId(Uint32 value)
{
  theDistributionKey = value;
  theDistrKeyIndicator_ = 1;
}

Uint32
NdbOperation::getPartitionId() const 
{
  return theDistributionKey;
}

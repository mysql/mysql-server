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
 * Name:          NdbOperationDefine.C
 * Include:
 * Link:
 * Author:        UABMNST Mona Natterkvist UAB/B/SD                         
 * Date:          970829
 * Version:       0.1
 * Description:   Interface between TIS and NDB
 * Documentation:
 * Adjust:  971022  UABMNST   First version.
 *****************************************************************************/
#include "NdbOperation.hpp"
#include "NdbApiSignal.hpp"
#include "NdbConnection.hpp"
#include "Ndb.hpp"
#include "NdbRecAttr.hpp"
#include "NdbUtil.hpp"
#include "NdbOut.hpp"
#include "NdbImpl.hpp"
#include <NdbIndexScanOperation.hpp>
#include "NdbBlob.hpp"

#include <Interpreter.hpp>

#include <AttributeHeader.hpp>
#include <signaldata/TcKeyReq.hpp>

/*****************************************************************************
 * int insertTuple();
 *****************************************************************************/
int
NdbOperation::insertTuple()
{
  NdbConnection* tNdbCon = theNdbCon;
  int tErrorLine = theErrorLine;
  if (theStatus == Init) {
    theStatus = OperationDefined;
    theOperationType = InsertRequest;
    tNdbCon->theSimpleState = 0;
    theErrorLine = tErrorLine++;
    theLockMode = LM_Exclusive;
    return 0; 
  } else {
    setErrorCode(4200);
    return -1;
  }//if
}//NdbOperation::insertTuple()
/******************************************************************************
 * int updateTuple();
 *****************************************************************************/
int
NdbOperation::updateTuple()
{  
  NdbConnection* tNdbCon = theNdbCon;
  int tErrorLine = theErrorLine;
  if (theStatus == Init) {
    theStatus = OperationDefined;
    tNdbCon->theSimpleState = 0;
    theOperationType = UpdateRequest;  
    theErrorLine = tErrorLine++;
    theLockMode = LM_Exclusive;
    return 0; 
  } else {
    setErrorCode(4200);
    return -1;
  }//if
}//NdbOperation::updateTuple()
/*****************************************************************************
 * int writeTuple();
 *****************************************************************************/
int
NdbOperation::writeTuple()
{  
  NdbConnection* tNdbCon = theNdbCon;
  int tErrorLine = theErrorLine;
  if (theStatus == Init) {
    theStatus = OperationDefined;
    tNdbCon->theSimpleState = 0;
    theOperationType = WriteRequest;  
    theErrorLine = tErrorLine++;
    theLockMode = LM_Exclusive;
    return 0; 
  } else {
    setErrorCode(4200);
    return -1;
  }//if
}//NdbOperation::writeTuple()
/******************************************************************************
 * int readTuple();
 *****************************************************************************/
int
NdbOperation::readTuple(NdbOperation::LockMode lm)
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
/******************************************************************************
 * int readTuple();
 *****************************************************************************/
int
NdbOperation::readTuple()
{ 
  NdbConnection* tNdbCon = theNdbCon;
  int tErrorLine = theErrorLine;
  if (theStatus == Init) {
    theStatus = OperationDefined;
    tNdbCon->theSimpleState = 0;
    theOperationType = ReadRequest;
    theErrorLine = tErrorLine++;
    theLockMode = LM_Read;
    return 0;
  } else {
    setErrorCode(4200);
    return -1;
  }//if
}//NdbOperation::readTuple()

/*****************************************************************************
 * int deleteTuple();
 *****************************************************************************/
int
NdbOperation::deleteTuple()
{
  NdbConnection* tNdbCon = theNdbCon;
  int tErrorLine = theErrorLine;
  if (theStatus == Init) {
    theStatus = OperationDefined;  
    tNdbCon->theSimpleState = 0;
    theOperationType = DeleteRequest;
    theErrorLine = tErrorLine++;
    theLockMode = LM_Exclusive;
    return 0;
  } else {
    setErrorCode(4200);
    return -1;
  }//if
}//NdbOperation::deleteTuple()

/******************************************************************************
 * int readTupleExclusive();
 *****************************************************************************/
int
NdbOperation::readTupleExclusive()
{ 
  NdbConnection* tNdbCon = theNdbCon;
  int tErrorLine = theErrorLine;
  if (theStatus == Init) {
    theStatus = OperationDefined;
    tNdbCon->theSimpleState = 0;
    theOperationType = ReadExclusive;
    theErrorLine = tErrorLine++;
    theLockMode = LM_Exclusive;
    return 0;
  } else {
    setErrorCode(4200);
    return -1;
  }//if
}//NdbOperation::readTupleExclusive()

/*****************************************************************************
 * int simpleRead();
 *****************************************************************************/
int
NdbOperation::simpleRead()
{
  int tErrorLine = theErrorLine;
  if (theStatus == Init) {
    theStatus = OperationDefined;
    theOperationType = ReadRequest;
    theSimpleIndicator = 1;
    theErrorLine = tErrorLine++;
    theLockMode = LM_CommittedRead;
    return 0;
  } else {
    setErrorCode(4200);
    return -1;
  }//if
}//NdbOperation::simpleRead()

/*****************************************************************************
 * int dirtyRead();
 *****************************************************************************/
int
NdbOperation::dirtyRead()
{
  return committedRead();
}//NdbOperation::dirtyRead()

/*****************************************************************************
 * int committedRead();
 *****************************************************************************/
int
NdbOperation::committedRead()
{
  int tErrorLine = theErrorLine;
  if (theStatus == Init) {
    theStatus = OperationDefined;
    theOperationType = ReadRequest;
    theSimpleIndicator = 1;
    theDirtyIndicator = 1;
    theErrorLine = tErrorLine++;
    theLockMode = LM_CommittedRead;
    return 0;
  } else {
    setErrorCode(4200);
    return -1;
  }//if
}//NdbOperation::committedRead()

/*****************************************************************************
 * int dirtyUpdate();
 ****************************************************************************/
int
NdbOperation::dirtyUpdate()
{
  NdbConnection* tNdbCon = theNdbCon;
  int tErrorLine = theErrorLine;
  if (theStatus == Init) {
    theStatus = OperationDefined;
    theOperationType = UpdateRequest;
    tNdbCon->theSimpleState = 0;
    theSimpleIndicator = 1;
    theDirtyIndicator = 1;
    theErrorLine = tErrorLine++;
    theLockMode = LM_CommittedRead;
    return 0;
  } else {
    setErrorCode(4200);
    return -1;
  }//if
}//NdbOperation::dirtyUpdate()

/******************************************************************************
 * int dirtyWrite();
 *****************************************************************************/
int
NdbOperation::dirtyWrite()
{
  NdbConnection* tNdbCon = theNdbCon;
  int tErrorLine = theErrorLine;
  if (theStatus == Init) {
    theStatus = OperationDefined;
    theOperationType = WriteRequest;
    tNdbCon->theSimpleState = 0;
    theSimpleIndicator = 1;
    theDirtyIndicator = 1;
    theErrorLine = tErrorLine++;
    theLockMode = LM_CommittedRead;
    return 0;
  } else {
    setErrorCode(4200);
    return -1;
  }//if
}//NdbOperation::dirtyWrite()

/******************************************************************************
 * int interpretedUpdateTuple();
 ****************************************************************************/
int
NdbOperation::interpretedUpdateTuple()
{
  NdbConnection* tNdbCon = theNdbCon;
  int tErrorLine = theErrorLine;
  if (theStatus == Init) {
    theStatus = OperationDefined;
    tNdbCon->theSimpleState = 0;
    theOperationType = UpdateRequest;
    theAI_LenInCurrAI = 25;
    theLockMode = LM_Exclusive;
    theErrorLine = tErrorLine++;
    initInterpreter();
    return 0;
  } else {
    setErrorCode(4200);
    return -1;
  }//if
}//NdbOperation::interpretedUpdateTuple()

/*****************************************************************************
 * int interpretedDeleteTuple();
 *****************************************************************************/
int
NdbOperation::interpretedDeleteTuple()
{
  NdbConnection* tNdbCon = theNdbCon;
  int tErrorLine = theErrorLine;
  if (theStatus == Init) {
    theStatus = OperationDefined;
    tNdbCon->theSimpleState = 0;
    theOperationType = DeleteRequest;

    theErrorLine = tErrorLine++;
    theAI_LenInCurrAI = 25;
    theLockMode = LM_Exclusive;
    initInterpreter();
    return 0;
  } else {
    setErrorCode(4200);
    return -1;
  }//if
}//NdbOperation::interpretedDeleteTuple()



/******************************************************************************
 * int getValue(AttrInfo* tAttrInfo, char* aRef )
 *
 * Return Value   Return 0 : GetValue was successful.
 *                Return -1: In all other case. 
 * Parameters:    tAttrInfo : Attribute object of the retrieved attribute 
 *                            value.
 * Remark:        Define an attribute to retrieve in query.
 *****************************************************************************/
NdbRecAttr*
NdbOperation::getValue_impl(const NdbColumnImpl* tAttrInfo, char* aValue)
{
  NdbRecAttr* tRecAttr;
  if ((tAttrInfo != NULL) &&
      (!tAttrInfo->m_indexOnly) && 
      (theStatus != Init)){
    if (theStatus != GetValue) {
      if (theInterpretIndicator == 1) {
	if (theStatus == FinalGetValue) {
	  ; // Simply continue with getValue
	} else if (theStatus == ExecInterpretedValue) {
	  if (insertATTRINFO(Interpreter::EXIT_OK) == -1)
	    return NULL;
	  theInterpretedSize = theTotalCurrAI_Len -
	    (theInitialReadSize + 5);
	} else if (theStatus == SetValueInterpreted) {
	  theFinalUpdateSize = theTotalCurrAI_Len - 
	    (theInitialReadSize + theInterpretedSize + 5);
	} else {
	  setErrorCodeAbort(4230);
	  return NULL;
	}//if
	// MASV - How would execution come here?
	theStatus = FinalGetValue;
      } else {
	setErrorCodeAbort(4230);
	return NULL;
      }//if
    }//if
    Uint32 ah;
    AttributeHeader::init(&ah, tAttrInfo->m_attrId, 0);
    if (insertATTRINFO(ah) != -1) {	
      // Insert Attribute Id into ATTRINFO part. 
      
      /************************************************************************
       * Get a Receive Attribute object and link it into the operation object.
       ***********************************************************************/
      if((tRecAttr = theReceiver.getValue(tAttrInfo, aValue)) != 0){
	theErrorLine++;
	return tRecAttr;
      } else {  
	setErrorCodeAbort(4000);
	return NULL;
      }
    } else {
      return NULL;
    }//if insertATTRINFO failure
  } else {
    if (tAttrInfo == NULL) {
      setErrorCodeAbort(4004);      
      return NULL;
    }//if
    if (tAttrInfo->m_indexOnly){
      setErrorCodeAbort(4208);
      return NULL;
    }//if
  }//if
  setErrorCodeAbort(4200);
  return NULL;
}

/*****************************************************************************
 * int setValue(AttrInfo* tAttrInfo, char* aValue, Uint32 len)
 *
 * Return Value:  Return 0 : SetValue was succesful.
 *                Return -1: In all other case.   
 * Parameters:    tAttrInfo : Attribute object where the attribute 
 *                            info exists.
 *                aValue : Reference to the variable with the new value.
 *		  len    : Length of the value
 * Remark:        Define a attribute to set in a query.
******************************************************************************/
int
NdbOperation::setValue( const NdbColumnImpl* tAttrInfo, 
			const char* aValuePassed, Uint32 len)
{
  int tReturnCode;
  Uint32 tAttrId;
  Uint32 tData;
  Uint32 tempData[2000];
  OperationType tOpType = theOperationType;
  OperationStatus tStatus = theStatus;

  
  if ((tOpType == UpdateRequest) ||
      (tOpType == WriteRequest)) {
    if (theInterpretIndicator == 0) {
      if (tStatus == SetValue) {
        ;
      } else {
        setErrorCodeAbort(4234);
        return -1;
      }//if
    } else {
      if (tStatus == GetValue) {
        theInitialReadSize = theTotalCurrAI_Len - 5;
      } else if	(tStatus == ExecInterpretedValue) {
	//--------------------------------------------------------------------
	// We insert an exit from interpretation since we are now starting 
	// to set values in the tuple by setValue.
	//--------------------------------------------------------------------
        if (insertATTRINFO(Interpreter::EXIT_OK) == -1){
          return -1;
	}
        theInterpretedSize = theTotalCurrAI_Len - 
          (theInitialReadSize + 5);
      } else if (tStatus == SetValueInterpreted) {
        ; // Simply continue adding new setValue
      } else {
	//--------------------------------------------------------------------
	// setValue used in the wrong context. Application coding error.
	//-------------------------------------------------------------------
        setErrorCodeAbort(4234); //Wrong error code
        return -1;
      }//if
      theStatus = SetValueInterpreted;
    }//if
  } else if (tOpType == InsertRequest) {
    if ((theStatus != SetValue) && (theStatus != OperationDefined)) {
      setErrorCodeAbort(4234);
      return -1;
    }//if
  } else if (tOpType == ReadRequest || tOpType == ReadExclusive) {
    setErrorCodeAbort(4504);
    return -1;
  } else if (tOpType == DeleteRequest) {
    setErrorCodeAbort(4504);
    return -1;
  } else if (tOpType == OpenScanRequest || tOpType == OpenRangeScanRequest) {
    setErrorCodeAbort(4228);
    return -1;
  } else {
    //---------------------------------------------------------------------
    // setValue with undefined operation type. 
    // Probably application coding error.
    //---------------------------------------------------------------------
    setErrorCodeAbort(4108);
    return -1;
  }//if
  if (tAttrInfo == NULL) {
    setErrorCodeAbort(4004);      
    return -1;
  }//if
  if (tAttrInfo->m_pk) {
    if (theOperationType == InsertRequest) {
      return equal_impl(tAttrInfo, aValuePassed, len);
    } else {
      setErrorCodeAbort(4202);      
      return -1;
    }//if
  }//if
  if (len > 8000) {
    setErrorCodeAbort(4216);
    return -1;
  }//if
  
  tAttrId = tAttrInfo->m_attrId;
  const char *aValue = aValuePassed; 
  Uint32 ahValue;
  if (aValue == NULL) {
    if (tAttrInfo->m_nullable) {
      AttributeHeader& ah = AttributeHeader::init(&ahValue, tAttrId, 0);
      ah.setNULL();
      insertATTRINFO(ahValue);
      // Insert Attribute Id with the value
      // NULL into ATTRINFO part. 
      return 0;
    } else {
      /***********************************************************************
       * Setting a NULL value on a NOT NULL attribute is not allowed.
       **********************************************************************/
      setErrorCodeAbort(4203);      
      return -1;
    }//if
  }//if
  
  // Insert Attribute Id into ATTRINFO part. 
  const Uint32 sizeInBytes = tAttrInfo->m_attrSize * tAttrInfo->m_arraySize;

  CHARSET_INFO* cs = tAttrInfo->m_cs;
  // invalid data can crash kernel
  if (cs != NULL &&
      (*cs->cset->well_formed_len)(cs,
                                   aValue,
                                   aValue + sizeInBytes,
                                   sizeInBytes) != sizeInBytes) {
    setErrorCodeAbort(744);
    return -1;
  }
#if 0
  tAttrSize = tAttrInfo->theAttrSize;
  tArraySize = tAttrInfo->theArraySize;
  if (tArraySize == 0) {
    setErrorCodeAbort(4201);      
    return -1;
  }//if
  tAttrSizeInBits = tAttrSize*tArraySize;
  tAttrSizeInWords = tAttrSizeInBits >> 5;
#endif
  const Uint32 bitsInLastWord = 8 * (sizeInBytes & 3) ;
  if (len != sizeInBytes && (len != 0)) {
    setErrorCodeAbort(4209);
    return -1;
  }//if
  const Uint32 totalSizeInWords = (sizeInBytes + 3)/4; // Including bits in last word
  const Uint32 sizeInWords = sizeInBytes / 4;          // Excluding bits in last word
  AttributeHeader& ah = AttributeHeader::init(&ahValue, tAttrId, 
					      totalSizeInWords);
  insertATTRINFO( ahValue );

  /***********************************************************************
   * Check if the pointer of the value passed is aligned on a 4 byte boundary.
   * If so only assign the pointer to the internal variable aValue. 
   * If it is not aligned then we start by copying the value to tempData and 
   * use this as aValue instead.
   *************************************************************************/
  const int attributeSize = sizeInBytes;
  const int slack = sizeInBytes & 3;
  
  if (((UintPtr)aValue & 3) != 0 || (slack != 0)){
    memcpy(&tempData[0], aValue, attributeSize);
    aValue = (char*)&tempData[0];
    if(slack != 0) {
      char * tmp = (char*)&tempData[0];
      memset(&tmp[attributeSize], 0, (4 - slack));
    }//if
  }//if
  
  tReturnCode = insertATTRINFOloop((Uint32*)aValue, sizeInWords);
  if (tReturnCode == -1) {
    return tReturnCode;
  }//if
  if (bitsInLastWord != 0) {
    tData = *(Uint32*)(aValue + sizeInWords*4);
    tData = convertEndian(tData);
    tData = tData & ((1 << bitsInLastWord) - 1);
    tData = convertEndian(tData);
    tReturnCode = insertATTRINFO(tData);
    if (tReturnCode == -1) {
      return tReturnCode;
    }//if
  }//if
  theErrorLine++;  
  return 0;
}//NdbOperation::setValue()

NdbBlob*
NdbOperation::getBlobHandle(NdbConnection* aCon, const NdbColumnImpl* tAttrInfo)
{
  NdbBlob* tBlob = theBlobList;
  NdbBlob* tLastBlob = NULL;
  while (tBlob != NULL) {
    if (tBlob->theColumn == tAttrInfo)
      return tBlob;
    tLastBlob = tBlob;
    tBlob = tBlob->theNext;
  }
  tBlob = theNdb->getNdbBlob();
  if (tBlob == NULL)
    return NULL;
  if (tBlob->atPrepare(aCon, this, tAttrInfo) == -1) {
    theNdb->releaseNdbBlob(tBlob);
    return NULL;
  }
  if (tLastBlob == NULL)
    theBlobList = tBlob;
  else
    tLastBlob->theNext = tBlob;
  tBlob->theNext = NULL;
  theNdbCon->theBlobFlag = true;
  return tBlob;
}

/****************************************************************************
 * int insertATTRINFO( Uint32 aData );
 *
 * Return Value:   Return 0 : insertATTRINFO was succesful.
 *                 Return -1: In all other case.   
 * Parameters:     aData: the data to insert into ATTRINFO.
 * Remark:         Puts the the data into either TCKEYREQ signal or 
 *                 ATTRINFO signal.
 *****************************************************************************/
int
NdbOperation::insertATTRINFO( Uint32 aData )
{
  NdbApiSignal* tSignal;
  register Uint32 tAI_LenInCurrAI = theAI_LenInCurrAI;
  register Uint32* tAttrPtr = theATTRINFOptr;
  register Uint32 tTotCurrAILen = theTotalCurrAI_Len;

  if (tAI_LenInCurrAI >= 25) {
    Ndb* tNdb = theNdb;
    NdbApiSignal* tFirstAttrinfo = theFirstATTRINFO;
    tAI_LenInCurrAI = 3;
    tSignal = tNdb->getSignal();
    if (tSignal != NULL) {
      tSignal->setSignal(m_attrInfoGSN);
      tAttrPtr = &tSignal->getDataPtrSend()[3];
      if (tFirstAttrinfo == NULL) {
        tSignal->next(NULL);
        theFirstATTRINFO = tSignal;
        theCurrentATTRINFO = tSignal;
      } else {
        NdbApiSignal* tCurrentAttrinfoBeforeUpdate = theCurrentATTRINFO;
        tSignal->next(NULL);
        theCurrentATTRINFO = tSignal;
        tCurrentAttrinfoBeforeUpdate->next(tSignal);
      }//if
    } else {
      goto insertATTRINFO_error1;
    }//if
  }//if
  *tAttrPtr = aData;
  tAttrPtr++;
  tTotCurrAILen++;
  tAI_LenInCurrAI++;
  theTotalCurrAI_Len = tTotCurrAILen;
  theAI_LenInCurrAI = tAI_LenInCurrAI;
  theATTRINFOptr = tAttrPtr;
  return 0;

insertATTRINFO_error1:
  setErrorCodeAbort(4000);
  return -1;

}//NdbOperation::insertATTRINFO()

/*****************************************************************************
 * int insertATTRINFOloop(Uint32* aDataPtr, Uint32 aLength );
 *
 * Return Value:  Return 0 : insertATTRINFO was succesful.
 *                Return -1: In all other case.   
 * Parameters:    aDataPtr: Pointer to the data to insert into ATTRINFO.
 *                aLength: Length of data to be copied
 * Remark:        Puts the the data into either TCKEYREQ signal or 
 *                ATTRINFO signal.
 *****************************************************************************/
int
NdbOperation::insertATTRINFOloop(register const Uint32* aDataPtr, 
				 register Uint32 aLength)
{
  NdbApiSignal* tSignal;
  register Uint32 tAI_LenInCurrAI = theAI_LenInCurrAI;
  register Uint32 tTotCurrAILen = theTotalCurrAI_Len;
  register Uint32* tAttrPtr = theATTRINFOptr;  
  Ndb* tNdb = theNdb;

  while (aLength > 0) {
    if (tAI_LenInCurrAI >= 25) {
      NdbApiSignal* tFirstAttrinfo = theFirstATTRINFO;
      tAI_LenInCurrAI = 3;
      tSignal = tNdb->getSignal();
      if (tSignal != NULL) {
        tSignal->setSignal(m_attrInfoGSN);
        tAttrPtr = &tSignal->getDataPtrSend()[3];
        if (tFirstAttrinfo == NULL) {
          tSignal->next(NULL);
          theFirstATTRINFO = tSignal;
          theCurrentATTRINFO = tSignal;
        } else {
          NdbApiSignal* tCurrentAttrinfoBeforeUpdate = theCurrentATTRINFO;
          tSignal->next(NULL);
          theCurrentATTRINFO = tSignal;
          tCurrentAttrinfoBeforeUpdate->next(tSignal);
        }//if
      } else {
        goto insertATTRINFO_error1;
      }//if
    }//if
    {
      register Uint32 tData = *aDataPtr;
      aDataPtr++;
      aLength--;
      tAI_LenInCurrAI++;
      *tAttrPtr = tData;
      tAttrPtr++;
      tTotCurrAILen++;
    }
  }//while
  theATTRINFOptr = tAttrPtr;
  theTotalCurrAI_Len = tTotCurrAILen;
  theAI_LenInCurrAI = tAI_LenInCurrAI;
  return 0;

insertATTRINFO_error1:
  setErrorCodeAbort(4000);
  return -1;

}//NdbOperation::insertATTRINFOloop()





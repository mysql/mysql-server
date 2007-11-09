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
#include <NdbOperation.hpp>
#include "NdbApiSignal.hpp"
#include <NdbTransaction.hpp>
#include <Ndb.hpp>
#include <NdbRecAttr.hpp>
#include "NdbUtil.hpp"
#include "NdbOut.hpp"
#include "NdbImpl.hpp"
#include <NdbIndexScanOperation.hpp>
#include <NdbBlob.hpp>

#include <Interpreter.hpp>

#include <AttributeHeader.hpp>
#include <signaldata/TcKeyReq.hpp>

/*****************************************************************************
 * int insertTuple();
 *****************************************************************************/
int
NdbOperation::insertTuple()
{
  NdbTransaction* tNdbCon = theNdbCon;
  int tErrorLine = theErrorLine;
  if (theStatus == Init) {
    theStatus = OperationDefined;
    theOperationType = InsertRequest;
    tNdbCon->theSimpleState = 0;
    theErrorLine = tErrorLine++;
    theLockMode = LM_Exclusive;
    m_abortOption = AbortOnError;
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
  NdbTransaction* tNdbCon = theNdbCon;
  int tErrorLine = theErrorLine;
  if (theStatus == Init) {
    theStatus = OperationDefined;
    tNdbCon->theSimpleState = 0;
    theOperationType = UpdateRequest;  
    theErrorLine = tErrorLine++;
    theLockMode = LM_Exclusive;
    m_abortOption = AbortOnError;
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
  NdbTransaction* tNdbCon = theNdbCon;
  int tErrorLine = theErrorLine;
  if (theStatus == Init) {
    theStatus = OperationDefined;
    tNdbCon->theSimpleState = 0;
    theOperationType = WriteRequest;  
    theErrorLine = tErrorLine++;
    theLockMode = LM_Exclusive;
    m_abortOption = AbortOnError;
    return 0; 
  } else {
    setErrorCode(4200);
    return -1;
  }//if
}//NdbOperation::writeTuple()
/*****************************************************************************
 * int deleteTuple();
 *****************************************************************************/
int
NdbOperation::deleteTuple()
{
  NdbTransaction* tNdbCon = theNdbCon;
  int tErrorLine = theErrorLine;
  if (theStatus == Init) {
    theStatus = OperationDefined;  
    tNdbCon->theSimpleState = 0;
    theOperationType = DeleteRequest;
    theErrorLine = tErrorLine++;
    theLockMode = LM_Exclusive;
    m_abortOption = AbortOnError;
    return 0;
  } else {
    setErrorCode(4200);
    return -1;
  }//if
}//NdbOperation::deleteTuple()

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
    return committedRead();
    break;
  case LM_SimpleRead:
    return simpleRead();
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
  NdbTransaction* tNdbCon = theNdbCon;
  int tErrorLine = theErrorLine;
  if (theStatus == Init) {
    theStatus = OperationDefined;
    tNdbCon->theSimpleState = 0;
    theOperationType = ReadRequest;
    theErrorLine = tErrorLine++;
    theLockMode = LM_Read;
    m_abortOption = AO_IgnoreError;
    return 0;
  } else {
    setErrorCode(4200);
    return -1;
  }//if
}//NdbOperation::readTuple()

/******************************************************************************
 * int readTupleExclusive();
 *****************************************************************************/
int
NdbOperation::readTupleExclusive()
{ 
  NdbTransaction* tNdbCon = theNdbCon;
  int tErrorLine = theErrorLine;
  if (theStatus == Init) {
    theStatus = OperationDefined;
    tNdbCon->theSimpleState = 0;
    theOperationType = ReadExclusive;
    theErrorLine = tErrorLine++;
    theLockMode = LM_Exclusive;
    m_abortOption = AO_IgnoreError;
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
  NdbTransaction* tNdbCon = theNdbCon;
  int tErrorLine = theErrorLine;
  if (theStatus == Init) {
    theStatus = OperationDefined;
    theOperationType = ReadRequest;
    theSimpleIndicator = 1;
    theDirtyIndicator = 0;
    theErrorLine = tErrorLine++;
    theLockMode = LM_SimpleRead;
    m_abortOption = AO_IgnoreError;
    tNdbCon->theSimpleState = 0;
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
    m_abortOption = AO_IgnoreError;
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
  NdbTransaction* tNdbCon = theNdbCon;
  int tErrorLine = theErrorLine;
  if (theStatus == Init) {
    theStatus = OperationDefined;
    theOperationType = UpdateRequest;
    tNdbCon->theSimpleState = 0;
    theSimpleIndicator = 1;
    theDirtyIndicator = 1;
    theErrorLine = tErrorLine++;
    theLockMode = LM_CommittedRead;
    m_abortOption = AbortOnError;
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
  NdbTransaction* tNdbCon = theNdbCon;
  int tErrorLine = theErrorLine;
  if (theStatus == Init) {
    theStatus = OperationDefined;
    theOperationType = WriteRequest;
    tNdbCon->theSimpleState = 0;
    theSimpleIndicator = 1;
    theDirtyIndicator = 1;
    theErrorLine = tErrorLine++;
    theLockMode = LM_CommittedRead;
    m_abortOption = AbortOnError;
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
  NdbTransaction* tNdbCon = theNdbCon;
  int tErrorLine = theErrorLine;
  if (theStatus == Init) {
    theStatus = OperationDefined;
    tNdbCon->theSimpleState = 0;
    theOperationType = UpdateRequest;
    theAI_LenInCurrAI = 25;
    theLockMode = LM_Exclusive;
    theErrorLine = tErrorLine++;
    m_abortOption = AbortOnError;
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
  NdbTransaction* tNdbCon = theNdbCon;
  int tErrorLine = theErrorLine;
  if (theStatus == Init) {
    theStatus = OperationDefined;
    tNdbCon->theSimpleState = 0;
    theOperationType = DeleteRequest;

    theErrorLine = tErrorLine++;
    theAI_LenInCurrAI = 25;
    theLockMode = LM_Exclusive;
    m_abortOption = AbortOnError;
    initInterpreter();
    return 0;
  } else {
    setErrorCode(4200);
    return -1;
  }//if
}//NdbOperation::interpretedDeleteTuple()

void
NdbOperation::setReadLockMode(LockMode lockMode)
{
  /* We only support changing lock mode for read operations at this time. */
  assert(theOperationType == ReadRequest || theOperationType == ReadExclusive);
  switch (lockMode) {
  case LM_CommittedRead: /* TODO, check theNdbCon->theSimpleState */
    theOperationType= ReadRequest;
    theSimpleIndicator= 1;
    theDirtyIndicator= 1;
    break;
  case LM_SimpleRead: /* TODO, check theNdbCon->theSimpleState */
    theOperationType= ReadRequest;
    theSimpleIndicator= 1;
    theDirtyIndicator= 0;
    break;
  case LM_Read:
    theNdbCon->theSimpleState= 0;
    theOperationType= ReadRequest;
    theSimpleIndicator= 0;
    theDirtyIndicator= 0;
    break;
  case LM_Exclusive:
    theNdbCon->theSimpleState= 0;
    theOperationType= ReadExclusive;
    theSimpleIndicator= 0;
    theDirtyIndicator= 0;
    break;
  default:
    /* Not supported / invalid. */
    assert(false);
  }
  theLockMode= lockMode;
}


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
      (theStatus != Init)){
    m_no_disk_flag &= (tAttrInfo->m_storageType == NDB_STORAGETYPE_DISK ? 0:1);
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
    AttributeHeader ah(tAttrInfo->m_attrId, 0);
    if (insertATTRINFO(ah.m_value) != -1) {	
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
			const char* aValuePassed)
{
  DBUG_ENTER("NdbOperation::setValue");
  DBUG_PRINT("enter", ("col: %s  op:%d  val: 0x%lx",
                       tAttrInfo->m_name.c_str(), theOperationType,
                       (long) aValuePassed));

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
        DBUG_RETURN(-1);
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
          DBUG_RETURN(-1);
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
        DBUG_RETURN(-1);
      }//if
      theStatus = SetValueInterpreted;
    }//if
  } else if (tOpType == InsertRequest) {
    if ((theStatus != SetValue) && (theStatus != OperationDefined)) {
      setErrorCodeAbort(4234);
      DBUG_RETURN(-1);
    }//if
  } else if (tOpType == ReadRequest || tOpType == ReadExclusive) {
    setErrorCodeAbort(4504);
    DBUG_RETURN(-1);
  } else if (tOpType == DeleteRequest) {
    setErrorCodeAbort(4504);
    DBUG_RETURN(-1);
  } else if (tOpType == OpenScanRequest || tOpType == OpenRangeScanRequest) {
    setErrorCodeAbort(4228);
    DBUG_RETURN(-1);
  } else {
    //---------------------------------------------------------------------
    // setValue with undefined operation type. 
    // Probably application coding error.
    //---------------------------------------------------------------------
    setErrorCodeAbort(4108);
    DBUG_RETURN(-1);
  }//if
  if (tAttrInfo == NULL) {
    setErrorCodeAbort(4004);      
    DBUG_RETURN(-1);
  }//if
  if (tAttrInfo->m_pk) {
    if (theOperationType == InsertRequest) {
      DBUG_RETURN(equal_impl(tAttrInfo, aValuePassed));
    } else {
      setErrorCodeAbort(4202);      
      DBUG_RETURN(-1);
    }//if
  }//if
  
  // Insert Attribute Id into ATTRINFO part. 
  tAttrId = tAttrInfo->m_attrId;
  m_no_disk_flag &= (tAttrInfo->m_storageType == NDB_STORAGETYPE_DISK ? 0:1);
  const char *aValue = aValuePassed; 
  if (aValue == NULL) {
    if (tAttrInfo->m_nullable) {
      AttributeHeader ah(tAttrId, 0);
      ah.setNULL();
      insertATTRINFO(ah.m_value);
      // Insert Attribute Id with the value
      // NULL into ATTRINFO part. 
      DBUG_RETURN(0);
    } else {
      /***********************************************************************
       * Setting a NULL value on a NOT NULL attribute is not allowed.
       **********************************************************************/
      setErrorCodeAbort(4203);      
      DBUG_RETURN(-1);
    }//if
  }//if
  
  Uint32 len;
  if (! tAttrInfo->get_var_length(aValue, len)) {
    setErrorCodeAbort(4209);
    DBUG_RETURN(-1);
  }

  const Uint32 sizeInBytes = len;
  const Uint32 bitsInLastWord = 8 * (sizeInBytes & 3) ;
  
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
  
  // Excluding bits in last word
  const Uint32 sizeInWords = sizeInBytes / 4;          
  AttributeHeader ah(tAttrId, sizeInBytes);
  insertATTRINFO( ah.m_value );

  /***********************************************************************
   * Check if the pointer of the value passed is aligned on a 4 byte boundary.
   * If so only assign the pointer to the internal variable aValue. 
   * If it is not aligned then we start by copying the value to tempData and 
   * use this as aValue instead.
   *************************************************************************/
  
  tReturnCode = insertATTRINFOloop((Uint32*)aValue, sizeInWords);
  if (tReturnCode == -1) {
    DBUG_RETURN(tReturnCode);
  }//if
  if (bitsInLastWord != 0) {
    tData = *(Uint32*)(aValue + sizeInWords*4);
    tData = convertEndian(tData);
    tData = tData & ((1 << bitsInLastWord) - 1);
    tData = convertEndian(tData);
    tReturnCode = insertATTRINFO(tData);
    if (tReturnCode == -1) {
      DBUG_RETURN(tReturnCode);
    }//if
  }//if
  theErrorLine++;  
  DBUG_RETURN(0);
}//NdbOperation::setValue()


int
NdbOperation::setAnyValue(Uint32 any_value)
{
  const NdbColumnImpl* impl =
    &NdbColumnImpl::getImpl(* NdbDictionary::Column::ANY_VALUE);
  OperationType tOpType = theOperationType;

  switch(tOpType){
  case DeleteRequest:{
    Uint32 ah;
    AttributeHeader::init(&ah, AttributeHeader::ANY_VALUE, 4);
    if (insertATTRINFO(ah) != -1 && insertATTRINFO(any_value) != -1 ) 
    {
      return 0;
    }
  }
  default:
    return setValue(impl, (const char *)&any_value);
  }

  setErrorCodeAbort(4000);
  return -1;
}


NdbBlob*
NdbOperation::getBlobHandle(NdbTransaction* aCon, const NdbColumnImpl* tAttrInfo)
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

NdbOperation::AbortOption
NdbOperation::getAbortOption() const
{
  return (AbortOption)m_abortOption;
}

int
NdbOperation::setAbortOption(AbortOption ao)
{
  switch(ao)
  {
    case AO_IgnoreError:
    case AbortOnError:
      m_abortOption= ao;
      return 0;
    default:
      return -1;
  }
}

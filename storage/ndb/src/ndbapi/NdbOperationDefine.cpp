/*
   Copyright (c) 2003, 2013, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "API.hpp"
#include "NdbOut.hpp"
#include <NdbBlob.hpp>

#include <Interpreter.hpp>
#include <NdbInterpretedCode.hpp>
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
    if (tAttrInfo->m_storageType == NDB_STORAGETYPE_DISK)
    {
      m_flags &= ~Uint8(OF_NO_DISK);
    }
    if (theStatus != GetValue) {
      if (theStatus == UseNdbRecord)
        /* This path for extra GetValues for NdbRecord */
        return getValue_NdbRecord(tAttrInfo, aValue);
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
        /* Final read, after running interpreted instructions. */
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

NdbRecAttr*
NdbOperation::getValue_NdbRecord(const NdbColumnImpl* tAttrInfo, char* aValue)
{
  NdbRecAttr* tRecAttr;

  if (tAttrInfo->m_storageType == NDB_STORAGETYPE_DISK)
  {
    m_flags &= ~Uint8(OF_NO_DISK);
  }

  /*
    For getValue with NdbRecord operations, we just allocate the NdbRecAttr,
    the signal data will be constructed later.
  */
  if((tRecAttr = theReceiver.getValue(tAttrInfo, aValue)) != 0) {
    theErrorLine++;
    return tRecAttr;
  } else {
    setErrorCodeAbort(4000);
    return NULL;
  }
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
                       tAttrInfo ? tAttrInfo->m_name.c_str() : "NULL",
                       theOperationType, (long) aValuePassed));

  int tReturnCode;
  Uint32 tAttrId;
  Uint32 tData;
  Uint32 tempData[ NDB_MAX_TUPLE_SIZE_IN_WORDS ];
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
  if (tAttrInfo->m_storageType == NDB_STORAGETYPE_DISK)
  {
    m_flags &= ~Uint8(OF_NO_DISK);
  }
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
  OperationType tOpType = theOperationType;

  if (theStatus == UseNdbRecord)
  {
    /* Method not allowed for NdbRecord, use OperationOptions or 
       ScanOptions structure instead */
    setErrorCodeAbort(4515);
    return -1;
  }

  const NdbColumnImpl* impl =
    &NdbColumnImpl::getImpl(* NdbDictionary::Column::ANY_VALUE);

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

int
NdbOperation::setOptimize(Uint32 options)
{
  return setValue(&NdbColumnImpl::getImpl(*NdbDictionary::Column::OPTIMIZE),
                  (const char*)&options);
}

/* Non-const variant of getBlobHandle - can return existing blob
 * handles, or create new ones for non-NdbRecord operations 
 */
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

  /*
   * For NdbRecord PK, unique index and scan operations, we only fetch existing 
   * blob handles here, creation must be done by requesting the blob in the 
   * NdbRecord and mask when creating the operation.
   * For NdbRecAttr PK, IK and scan operations, we allow Blob handles
   * to be created here.  Note that NdbRecAttr PK and unique index ops are handled
   * differently to NdbRecAttr scan operations.
   */
  if (m_attribute_record)
  {
    setErrorCodeAbort(4288);
    return NULL;
  }

  /* Check key fully defined for key operations */
  switch (theStatus)
  {
  case TupleKeyDefined:
  case GetValue:
  case SetValue:
  case FinalGetValue:
  case ExecInterpretedValue:
  case SetValueInterpreted:
    /* All ok states to create a Blob Handle in */
    break;
  default:
  {
    /* Unexpected state to be obtaining Blob handle */
    /* Invalid usage of blob attribute */
    setErrorCodeAbort(4264);
    return NULL;
  }
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

/* const variant of getBlobHandle - only returns existing blob handles */
NdbBlob*
NdbOperation::getBlobHandle(NdbTransaction* aCon, const NdbColumnImpl* tAttrInfo) const
{
  NdbBlob* tBlob = theBlobList;
  while (tBlob != NULL) {
    if (tBlob->theColumn == tAttrInfo)
      return tBlob;
    tBlob = tBlob->theNext;
  }

  /*
    Const method - cannot create a new BLOB handle, NdbRecord
    or NdbRecAttr
  */
  setErrorCodeAbort(4288);
  return NULL;
}

/*
  This is used to set up a blob handle for an NdbRecord operation.

  It allocates the NdbBlob object, initialises it, and links it into the
  operation.

  There are two cases for how to set up the primary key info:
    1. Normal primary key or hash index key operations. The keyinfo argument
       is passed as NULL, and the key value is read from the NdbRecord and
       row passed from the application.
    2. Take-over scan operation. The keyinfo argument points to a buffer
       containing KEYINFO20 data.

  For a scan operation, there is no key info to set up at prepare time.
*/
NdbBlob *
NdbOperation::linkInBlobHandle(NdbTransaction *aCon,
                               const NdbColumnImpl *column,
                               NdbBlob * & lastPtr)
{
  int res;

  NdbBlob *bh= theNdb->getNdbBlob();
  if (bh == NULL)
    return NULL;

  if (theOperationType == OpenScanRequest ||
      theOperationType == OpenRangeScanRequest)
  {
    res= bh->atPrepareNdbRecordScan(aCon, this, column);
  }
  else if (m_key_record == NULL)
  {
    /* This means that we have a scan take-over operation, and we should
       obtain the key from KEYINFO20 data.
    */
    res= bh->atPrepareNdbRecordTakeover(aCon, this, column,
                                        m_key_row, m_keyinfo_length*4);
  }
  else
  {
    res= bh->atPrepareNdbRecord(aCon, this, column, m_key_record, m_key_row);
  }
  if (res == -1)
  {
    theNdb->releaseNdbBlob(bh);
    return NULL;
  }
  if (lastPtr)
    lastPtr->theNext= bh;
  else
    theBlobList= bh;
  lastPtr= bh;
  bh->theNext= NULL;
  theNdbCon->theBlobFlag= true;

  return bh;
}

/*
 * Setup blob handles for an NdbRecord operation.
 *
 * Create blob handles for all requested blob columns.
 *
 * For read request, store the pointers to blob handles in the row.
 */
int
NdbOperation::getBlobHandlesNdbRecord(NdbTransaction* aCon, 
                                      const Uint32 * m_read_mask)
{
  NdbBlob *lastBlob= NULL;

  for (Uint32 i= 0; i<m_attribute_record->noOfColumns; i++)
  {
    const NdbRecord::Attr *col= &m_attribute_record->columns[i];
    if (!(col->flags & NdbRecord::IsBlob))
      continue;

    Uint32 attrId= col->attrId;
    if (!BitmaskImpl::get((NDB_MAX_ATTRIBUTES_IN_TABLE+31)>>5,
                          m_read_mask, attrId))
      continue;

    const NdbColumnImpl *tableColumn= m_currentTable->getColumn(attrId);
    assert(tableColumn != NULL);

    NdbBlob *bh= linkInBlobHandle(aCon, tableColumn, lastBlob);
    if (bh == NULL)
      return -1;

    if (theOperationType == ReadRequest || theOperationType == ReadExclusive)
    {
      /*
       * For read request, it is safe to cast away const-ness for the
       * m_attribute_row.
       */
      memcpy((char *)&m_attribute_row[col->offset], &bh, sizeof(bh));
    }
  }

  return 0;
}

/*
  For a delete, we need to create blob handles for all table blob columns,
  so that we can be sure to delete all blob parts for the row.
  If checkReadset is true, we also check that the caller is not asking to 
  read any blobs as part of the delete.
*/
int
NdbOperation::getBlobHandlesNdbRecordDelete(NdbTransaction* aCon,
                                            bool checkReadSet,
                                            const Uint32 * m_read_mask)
{
  NdbBlob *lastBlob= NULL;

  assert(theOperationType == DeleteRequest);

  for (Uint32 i= 0; i < m_currentTable->m_columns.size(); i++)
  {
    const NdbColumnImpl* c= m_currentTable->m_columns[i];
    assert(c != 0);
    if (!c->getBlobType())
      continue;

    if (checkReadSet &&
        (BitmaskImpl::get((NDB_MAX_ATTRIBUTES_IN_TABLE+31)>>5,
                          m_read_mask, c->m_attrId)))
    {
      /* Blobs are not allowed in NdbRecord delete result record */
      setErrorCodeAbort(4511);
      return -1;
    }

    NdbBlob *bh= linkInBlobHandle(aCon, c, lastBlob);
    if (bh == NULL)
      return -1;
  }

  return 0;
}

NdbRecAttr*
NdbOperation::getVarValue(const NdbColumnImpl* tAttrInfo,
                          char* aBareValue, Uint16* aLenLoc)
{
  NdbRecAttr* ra = getValue(tAttrInfo, aBareValue);
  if (ra != NULL) {
    assert(aLenLoc != NULL);
    ra->m_getVarValue = aLenLoc;
  }
  return ra;
}

int
NdbOperation::setVarValue(const NdbColumnImpl* tAttrInfo,
                          const char* aBareValue, const Uint16& aLen)
{
  DBUG_ENTER("NdbOperation::setVarValue");
  DBUG_PRINT("info", ("aLen=%u", (Uint32)aLen));

  // wl3717_todo not optimal..
  const Uint32 MaxTupleSizeInLongWords= (NDB_MAX_TUPLE_SIZE + 7)/ 8;
  Uint64 buf[ MaxTupleSizeInLongWords ];
  assert( aLen < (NDB_MAX_TUPLE_SIZE - 2) );
  unsigned char* p = (unsigned char*)buf;
  p[0] = (aLen & 0xff);
  p[1] = (aLen >> 8);
  memcpy(&p[2], aBareValue, aLen);
  if (setValue(tAttrInfo, (char*)buf) == -1)
    DBUG_RETURN(-1);
  DBUG_RETURN(0);
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
      tSignal->setSignal(m_attrInfoGSN, refToBlock(theNdbCon->m_tcRef));
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
        tSignal->setSignal(m_attrInfoGSN, refToBlock(theNdbCon->m_tcRef));
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
  if (theStatus == UseNdbRecord)
  {
    /* Method not allowed for NdbRecord, use OperationOptions or 
       ScanOptions structure instead */
    setErrorCodeAbort(4515);
    return -1;
  }

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


int
NdbOperation::prepareGetLockHandleNdbRecord()
{
  /* This method is used to perform the correct actions
   * when the OO_LOCKHANDLE flag is set on an NdbRecord
   * operation.
   */
  assert(theLockHandle == NULL);
  theLockHandle = theNdbCon->getLockHandle();
  if (!theLockHandle)
  {
    return 4000; /* Memory allocation issue */
  }

  assert(! theLockHandle->isLockRefValid());

  assert(m_attribute_record);
  theLockHandle->m_table = m_attribute_record->table;
  assert(theLockHandle->m_table);
  
  NdbRecAttr* ra = 
    getValue_NdbRecord(&NdbColumnImpl::getImpl(*NdbDictionary::Column::LOCK_REF),
                       (char*) &theLockHandle->m_lockRef);
  
  if (!ra)
  {
    /* Assume error code set */
    assert(theError.code);
    return theError.code;
  }

  theLockHandle->m_state = NdbLockHandle::PREPARED;

  return 0;
}

/*
 * handleOperationOptions
 * static member for setting operation options
 * Called when defining operations, from NdbTransaction and
 * NdbScanOperation
 */
int
NdbOperation::handleOperationOptions (const OperationType type,
                                      const OperationOptions *opts,
                                      const Uint32 sizeOfOptions,
                                      NdbOperation *op)
{
  /* Check options size for versioning... */
  if (unlikely((sizeOfOptions != 0) && 
               (sizeOfOptions != sizeof(OperationOptions))))
  {
    // Handle different sized OperationOptions
    // Probably smaller is old version, larger is new version.
    
    // No other versions currently supported
    // Invalid or unsupported OperationOptions structure
    return 4297;
  }

  bool isScanTakeoverOp = (op->m_key_record == NULL); 
  
  if (opts->optionsPresent & OperationOptions::OO_ABORTOPTION)
  {
    /* User defined operation abortoption : Allowed for 
     * any operation 
     */
    switch (opts->abortOption)
    {
    case AO_IgnoreError:
    case AbortOnError:
    {
      op->m_abortOption=opts->abortOption;
      break;
    }
    default:
      // Non-specific abortoption
      // Invalid AbortOption
      return 4296;
    }
  } 

  if ((opts->optionsPresent & OperationOptions::OO_GETVALUE) &&
      (opts->numExtraGetValues > 0))
  {
    if (opts->extraGetValues == NULL)
    {
      // Incorrect combination of OperationOptions optionsPresent, 
      // extraGet/SetValues ptr and numExtraGet/SetValues
      return 4512;
    }

    // Only certain operation types allow extra GetValues
    // Update could be made to support it in future
    if (type == ReadRequest ||
        type == ReadExclusive ||
        type == DeleteRequest)
    {
      // Could be readTuple(), or lockCurrentTuple().
      // We perform old-school NdbRecAttr reads on
      // these values.
      for (unsigned int i=0; i < opts->numExtraGetValues; i++)
      {
        GetValueSpec *pvalSpec 
          = &(opts->extraGetValues[i]);

        pvalSpec->recAttr=NULL;

        if (pvalSpec->column == NULL)
        {
          // Column is NULL in Get/SetValueSpec structure
          return 4295;
        }

        NdbRecAttr *pra=
          op->getValue_NdbRecord(&NdbColumnImpl::getImpl(*pvalSpec->column),
                                 (char *) pvalSpec->appStorage);
        
        if (pra == NULL)
        {
          return -1;
        }

        pvalSpec->recAttr = pra;
      }
    }
    else
    {
      // Bad operation type for GetValue
      switch (type)
      {
      case WriteRequest : 
      case UpdateRequest :
      {
        return 4502;
        // GetValue not allowed in Update operation
      }
      case InsertRequest :
      {
        return 4503;
        // GetValue not allowed in Insert operation
      }
      default :
        return 4118;
        // Parameter error in API call
      }
    }
  }

  if ((opts->optionsPresent & OperationOptions::OO_SETVALUE) &&
      (opts->numExtraSetValues > 0))
  {
    if (opts->extraSetValues == NULL)
    {
      // Incorrect combination of OperationOptions optionsPresent, 
      // extraGet/SetValues ptr and numExtraGet/SetValues
      return 4512;
    }

    if ((type == InsertRequest) ||
        (type == UpdateRequest) ||
        (type == WriteRequest))
    {
      /* Could be insert/update/writeTuple() or 
       * updateCurrentTuple()
       */
      // Validate SetValuesSpec
      for (Uint32 i=0; i< opts->numExtraSetValues; i++)
      {
        const NdbDictionary::Column *pcol=opts->extraSetValues[i].column;
        const void *pvalue=opts->extraSetValues[i].value;

        if (pcol == NULL)
        {
          // Column is NULL in Get/SetValueSpec structure
          return 4295;
        }

        if (type == UpdateRequest && pcol->getPrimaryKey())
        {
          // It is not possible to update a primary key column.
          // It can be set like this for insert and write (but it
          // still needs to be included in the key NdbRecord and row).
          return 4202;
        }

        if (pvalue == NULL)
        {
          if (!pcol->getNullable())
          {
            // Trying to set a NOT NULL attribute to NULL
            return 4203;
          }
        }
          
        NdbDictionary::Column::Type colType=pcol->getType();
          
        if ((colType == NdbDictionary::Column::Blob) ||
            (colType == NdbDictionary::Column::Text))
        {
          // Invalid usage of blob attribute
          return 4264;
        }          
      }

      // Store details of extra set values for later
      op->m_extraSetValues = opts->extraSetValues;
      op->m_numExtraSetValues = opts->numExtraSetValues;
    }
    else
    {
      // Set value and Read/Delete etc is incompatible
      return 4204;
    }
  }

  if (opts->optionsPresent & OperationOptions::OO_PARTITION_ID)
  {
    /* Should not have any blobs defined at this stage */
    assert(op->theBlobList == NULL);

    /* Not allowed for scan takeover ops */
    if (unlikely(isScanTakeoverOp))
    {
      return 4510;
      /* User-specified partition id not allowed for scan 
       * takeover operation 
       */
    }
    /* Only allowed for pk ops on user defined partitioned tables
     * or when defining an unlock operation
     */
    if (unlikely( ! (((op->m_attribute_record->flags & 
                       NdbRecord::RecHasUserDefinedPartitioning) &&
                      (op->m_key_record->table->m_index == NULL)) ||
                     (type == UnlockRequest))))
    {
      /* Explicit partitioning info not allowed for table and operation*/
      return 4546;
    }
    op->theDistributionKey=opts->partitionId;
    op->theDistrKeyIndicator_= 1;       
  }
    
  if (opts->optionsPresent & OperationOptions::OO_INTERPRETED)
  {
    /* Check the operation type is valid */
    if (! ((type == ReadRequest)   ||
           (type == ReadExclusive) ||
           (type == UpdateRequest) ||
           (type == DeleteRequest)))
      /* NdbInterpretedCode not supported for operation type */
      return 4539;
    
    /* Check the program's for the same table as the
     * operation, within a major version number
     * Perhaps NdbInterpretedCode should not contain the table
     */
    const NdbDictionary::Table* codeTable= opts->interpretedCode->getTable();
    if (codeTable != NULL)
    {
      NdbTableImpl* impl= &NdbTableImpl::getImpl(*codeTable);
      
      if ((impl->m_id != (int) op->m_attribute_record->tableId) ||
          (table_version_major(impl->m_version) != 
           table_version_major(op->m_attribute_record->tableVersion)))
        return 4524; // NdbInterpretedCode is for different table`
    }

    /* Check the program's finalised */
    if ((opts->interpretedCode->m_flags & 
         NdbInterpretedCode::Finalised) == 0)
      return 4519; // NdbInterpretedCode::finalise() not called.

    op->m_interpreted_code = opts->interpretedCode;
  }

  if (opts->optionsPresent & OperationOptions::OO_ANYVALUE)
  {
    /* Any operation can have an ANYVALUE set */
    op->m_any_value = opts->anyValue;
    op->m_flags |= OF_USE_ANY_VALUE;
  }

  if (opts->optionsPresent & OperationOptions::OO_CUSTOMDATA)
  {
    /* Set the operation's customData ptr */
    op->m_customData = opts->customData;
  }

  if (opts->optionsPresent & OperationOptions::OO_LOCKHANDLE)
  {
    if (unlikely(op->theNdb->getMinDbNodeVersion() <
                 NDBD_UNLOCK_OP_SUPPORTED))
    {
      /* Function not implemented yet */
      return 4003;
    }

    /* Check that this is a pk read with a lock 
     * No need to worry about Blob lock upgrade issues as
     * Blobs have not been handled at this stage
     */
    if (((type != ReadRequest) &&
         (type != ReadExclusive)) ||
        (op->m_key_record &&
         (op->m_key_record->flags & NdbRecord::RecIsIndex)) ||
        ((op->theLockMode != LM_Read) &&
         (op->theLockMode != LM_Exclusive)))
    {
      return 4549; /* getLockHandle only supported for primary key read with a lock */
    }

    int prepareRc = op->prepareGetLockHandleNdbRecord();      
    if (prepareRc != 0)
    {
      return prepareRc;
    }    
  }

  if (opts->optionsPresent & OperationOptions::OO_QUEUABLE)
  {
    op->m_flags |= OF_QUEUEABLE;
  }

  if (opts->optionsPresent & OperationOptions::OO_NOT_QUEUABLE)
  {
    op->m_flags &= ~Uint8(OF_QUEUEABLE);
  }

  if (opts->optionsPresent & OperationOptions::OO_DEFERRED_CONSTAINTS)
  {
    op->m_flags |= OF_DEFERRED_CONSTRAINTS;
  }

  if (opts->optionsPresent & OperationOptions::OO_DISABLE_FK)
  {
    op->m_flags |= OF_DISABLE_FK;
  }

  return 0;
}

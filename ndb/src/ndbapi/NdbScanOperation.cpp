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
 * Name:          NdbScanOperation.cpp
 * Include:
 * Link:
 * Author:        UABMASD Martin Sköld INN/V Alzato
 * Date:          2002-04-01
 * Version:       0.1
 * Description:   Table scan support
 * Documentation:
 * Adjust:  2002-04-01  UABMASD   First version.
 ****************************************************************************/

#include <ndb_global.h>
#include <Ndb.hpp>
#include <NdbScanOperation.hpp>
#include <NdbConnection.hpp>
#include <NdbResultSet.hpp>
#include "NdbApiSignal.hpp"
#include <NdbOut.hpp>
#include "NdbDictionaryImpl.hpp"
#include "NdbBlob.hpp"

NdbScanOperation::NdbScanOperation(Ndb* aNdb) :
  NdbCursorOperation(aNdb),
  m_transConnection(NULL),
  m_autoExecute(false),
  m_updateOp(false),
  m_deleteOp(false),
  m_setValueList(new SetValueRecList())
{
}

NdbScanOperation::~NdbScanOperation()
{
  if (m_setValueList) delete m_setValueList;
}

NdbCursorOperation::CursorType
NdbScanOperation::cursorType()
{
  return NdbCursorOperation::ScanCursor;
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
NdbScanOperation::init(NdbTableImpl* tab, NdbConnection* myConnection)
{
  m_transConnection = myConnection;
  //NdbConnection* aScanConnection = theNdb->startTransaction(myConnection);
  NdbConnection* aScanConnection = theNdb->hupp(myConnection);
  if (!aScanConnection){
    setErrorCodeAbort(theNdb->getNdbError().code);
    return -1;
  }
  aScanConnection->theFirstOpInList = this;
  aScanConnection->theLastOpInList = this;
  NdbCursorOperation::cursInit();
  // NOTE! The hupped trans becomes the owner of the operation
  return NdbOperation::init(tab, aScanConnection); 
}

NdbResultSet* NdbScanOperation::readTuples(Uint32 parallell, 
					   NdbCursorOperation::LockMode lm)
{
  int res = 0;
  switch(lm){
  case NdbCursorOperation::LM_Read:
    parallell = (parallell == 0 ? 240 : parallell);
    res = openScan(parallell, false, true, false);
    break;
  case NdbCursorOperation::LM_Exclusive:
    parallell = (parallell == 0 ? 1 : parallell);
    res = openScan(parallell, true, true, false);
    break;
  case NdbCursorOperation::LM_Dirty:
    parallell = (parallell == 0 ? 240 : parallell);
    res = openScan(parallell, false, false, true);
    break;
  default:
    res = -1;
    setErrorCode(4003);
  }
  if(res == -1){
    return NULL;
  }
  theNdbCon->theFirstOpInList = 0;
  theNdbCon->theLastOpInList = 0;

  return getResultSet();
}

int NdbScanOperation::updateTuples(Uint32 parallelism)
{
  if (openScanExclusive(parallelism) == -1) {
    return -1;
  }
  theNdbCon->theFirstOpInList = 0;
  theNdbCon->theLastOpInList = 0;

  m_updateOp = true;

  return 0;
}

int NdbScanOperation::deleteTuples(Uint32 parallelism)
{
  if (openScanExclusive(parallelism) == -1) {
    return -1;
  }
  theNdbCon->theFirstOpInList = 0;
  theNdbCon->theLastOpInList = 0;

  m_deleteOp = true;

  return 0;
}

int  NdbScanOperation::setValue(const char* anAttrName, const char* aValue, Uint32 len) 
{
  // Check if attribute exist
  if (m_currentTable->getColumn(anAttrName) == NULL)
    return -1;
  
  m_setValueList->add(anAttrName, aValue, len);
  return 0;
}

int  NdbScanOperation::setValue(const char* anAttrName, Int32 aValue) 
{
  // Check if attribute exist
  if (m_currentTable->getColumn(anAttrName) == NULL)
    return -1;

  m_setValueList->add(anAttrName, aValue);
  return 0;
}

int  NdbScanOperation::setValue(const char* anAttrName, Uint32 aValue) 
{
  // Check if attribute exist
  if (m_currentTable->getColumn(anAttrName) == NULL)
    return -1;

  m_setValueList->add(anAttrName, aValue);
  return 0;
}

int  NdbScanOperation::setValue(const char* anAttrName, Uint64 aValue) 
{
  // Check if attribute exist
  if (m_currentTable->getColumn(anAttrName) == NULL)
    return -1;

  m_setValueList->add(anAttrName, aValue);
  return 0;
}

int  NdbScanOperation::setValue(const char* anAttrName, Int64 aValue) 
{
  // Check if attribute exist
  if (m_currentTable->getColumn(anAttrName) == NULL)
    return -1;

  m_setValueList->add(anAttrName, aValue);
  return 0;
}

int  NdbScanOperation::setValue(const char* anAttrName, float aValue) 
{
  // Check if attribute exist
  if (m_currentTable->getColumn(anAttrName) == NULL)
    return -1;

  m_setValueList->add(anAttrName, aValue);
  return 0;
}

int  NdbScanOperation::setValue(const char* anAttrName, double aValue) 
{
  // Check if attribute exist
  if (m_currentTable->getColumn(anAttrName) == NULL)
    return -1;

  m_setValueList->add(anAttrName, aValue);
  return 0;
}


int  NdbScanOperation::setValue(Uint32 anAttrId, const char* aValue, Uint32 len) 
{
  // Check if attribute exist
  if (m_currentTable->getColumn(anAttrId) == NULL)
    return -1;

  m_setValueList->add(anAttrId, aValue, len);
  return 0;
}

int  NdbScanOperation::setValue(Uint32 anAttrId, Int32 aValue) 
{
  // Check if attribute exist
  if (m_currentTable->getColumn(anAttrId) == NULL)
    return -1;

  m_setValueList->add(anAttrId, aValue);
  return 0;
}

int  NdbScanOperation::setValue(Uint32 anAttrId, Uint32 aValue) 
{
  // Check if attribute exist
  if (m_currentTable->getColumn(anAttrId) == NULL)
    return -1;

  m_setValueList->add(anAttrId, aValue);
  return 0;
}

int  NdbScanOperation::setValue(Uint32 anAttrId, Uint64 aValue) 
{
  // Check if attribute exist
  if (m_currentTable->getColumn(anAttrId) == NULL)
    return -1;

  m_setValueList->add(anAttrId, aValue);
  return 0;
}

int  NdbScanOperation::setValue(Uint32 anAttrId, Int64 aValue) 
{
  // Check if attribute exist
  if (m_currentTable->getColumn(anAttrId) == NULL)
    return -1;

  m_setValueList->add(anAttrId, aValue);
  return 0;
}

int  NdbScanOperation::setValue(Uint32 anAttrId, float aValue) 
{
  // Check if attribute exist
  if (m_currentTable->getColumn(anAttrId) == NULL)
    return -1;

  m_setValueList->add(anAttrId, aValue);
  return 0;
}

int  NdbScanOperation::setValue(Uint32 anAttrId, double aValue) 
{
  // Check if attribute exist
  if (m_currentTable->getColumn(anAttrId) == NULL)
    return -1;

  m_setValueList->add(anAttrId, aValue);
  return 0;
}

NdbBlob*
NdbScanOperation::getBlobHandle(const char* anAttrName)
{
  return NdbOperation::getBlobHandle(m_transConnection, m_currentTable->getColumn(anAttrName));
}

NdbBlob*
NdbScanOperation::getBlobHandle(Uint32 anAttrId)
{
  return NdbOperation::getBlobHandle(m_transConnection, m_currentTable->getColumn(anAttrId));
}

// Private methods

int NdbScanOperation::executeCursor(int ProcessorId)
{
  int result = theNdbCon->executeScan();
  // If the scan started ok and we are updating or deleting
  // iterate over all tuples
  if ((m_updateOp) || (m_deleteOp)) {
    NdbOperation* newOp;
 
    while ((result != -1) && (nextResult() == 0)) {
      if (m_updateOp) {
        newOp = takeOverScanOp(UpdateRequest, m_transConnection);
        // Pass setValues from scan operation to new operation
        m_setValueList->iterate(SetValueRecList::callSetValueFn, *newOp);
	// No need to call updateTuple since scan was taken over for update 
	// it should be the same with delete - MASV
	//  newOp->updateTuple(); 
      }
      else if (m_deleteOp) {
        newOp = takeOverScanOp(DeleteRequest, m_transConnection);
	//  newOp->deleteTuple();
      }
#if 0
     // takeOverScanOp will take over the lock that scan aquired
      // the lock is released when nextScanResult is called 
      // That means that the "takeover" has to be sent to the kernel 
      // before nextScanresult is called - MASV
      if (m_autoExecute){
	m_transConnection->execute(NoCommit);
      }
#else
      m_transConnection->execute(NoCommit);
#endif
    }
    closeScan();
  }
  
  return result;
}

int NdbScanOperation::nextResult(bool fetchAllowed)
{
  int result = theNdbCon->nextScanResult(fetchAllowed);
  if (result == -1){
    // Move the error code from hupped transaction
    // to the real trans
    const NdbError err = theNdbCon->getNdbError();
    m_transConnection->setOperationErrorCode(err.code);
  }
  if (result == 0) {
    // handle blobs
    NdbBlob* tBlob = theBlobList;
    while (tBlob != NULL) {
      if (tBlob->atNextResult() == -1)
        return -1;
      tBlob = tBlob->theNext;
    }
  }
  return result;
}

int 
NdbScanOperation::prepareSend(Uint32  TC_ConnectPtr, Uint64  TransactionId)
{
  printf("NdbScanOperation::prepareSend\n");
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
  if(theNdbCon){
    if (theNdbCon->stopScan() == -1)
      theError = theNdbCon->getNdbError();
    theNdb->closeTransaction(theNdbCon);
    theNdbCon = 0;
  }
  m_transConnection = NULL;
}

void NdbScanOperation::release(){
  closeScan();
  NdbCursorOperation::release();
}

void SetValueRecList::add(const char* anAttrName, const char* aValue, Uint32 len) 
{
  SetValueRec* newSetValueRec = new SetValueRec();

  newSetValueRec->stype = SetValueRec::SET_STRING_ATTR1;
  newSetValueRec->anAttrName = strdup(anAttrName);
  newSetValueRec->stringStruct.aStringValue = (char *) malloc(len);
  strlcpy(newSetValueRec->stringStruct.aStringValue, aValue, len);
  if (!last)
    first = last = newSetValueRec;
  else {
    last->next = newSetValueRec;
    last = newSetValueRec;  
  }
}

void SetValueRecList::add(const char* anAttrName, Int32 aValue) 
{
  SetValueRec* newSetValueRec = new SetValueRec();

  newSetValueRec->stype = SetValueRec::SET_INT32_ATTR1;
  newSetValueRec->anAttrName = strdup(anAttrName);
  newSetValueRec->anInt32Value = aValue;
  if (!last)
    first = last = newSetValueRec;
  else {
    last->next = newSetValueRec;
    last = newSetValueRec;  
  }
}

void SetValueRecList::add(const char* anAttrName, Uint32 aValue) 
{
  SetValueRec* newSetValueRec = new SetValueRec();

  newSetValueRec->stype = SetValueRec::SET_UINT32_ATTR1;
  newSetValueRec->anAttrName = strdup(anAttrName);
  newSetValueRec->anUint32Value = aValue;
  if (!last)
    first = last = newSetValueRec;
  else {
    last->next = newSetValueRec;
    last = newSetValueRec;  
  }
}

void SetValueRecList::add(const char* anAttrName, Int64 aValue) 
{
  SetValueRec* newSetValueRec = new SetValueRec();

  newSetValueRec->stype = SetValueRec::SET_INT64_ATTR1;
  newSetValueRec->anAttrName = strdup(anAttrName);
  newSetValueRec->anInt64Value = aValue;  
  if (!last)
    first = last = newSetValueRec;
  else {
    last->next = newSetValueRec;
    last = newSetValueRec;  
  }
}

void SetValueRecList::add(const char* anAttrName, Uint64 aValue) 
{
  SetValueRec* newSetValueRec = new SetValueRec();

  newSetValueRec->stype = SetValueRec::SET_UINT64_ATTR1;
  newSetValueRec->anAttrName = strdup(anAttrName);
  newSetValueRec->anUint64Value = aValue;  
  if (!last)
    first = last = newSetValueRec;
  else {
    last->next = newSetValueRec;
    last = newSetValueRec;  
  }
}

void SetValueRecList::add(const char* anAttrName, float aValue) 
{
  SetValueRec* newSetValueRec = new SetValueRec();

  newSetValueRec->stype = SetValueRec::SET_FLOAT_ATTR1;
  newSetValueRec->anAttrName = strdup(anAttrName);
  newSetValueRec->aFloatValue = aValue;  
  if (!last)
    first = last = newSetValueRec;
  else {
    last->next = newSetValueRec;
    last = newSetValueRec;  
  }
}

void SetValueRecList::add(const char* anAttrName, double aValue) 
{
  SetValueRec* newSetValueRec = new SetValueRec();

  newSetValueRec->stype = SetValueRec::SET_DOUBLE_ATTR1;
  newSetValueRec->anAttrName = strdup(anAttrName);
  newSetValueRec->aDoubleValue = aValue;  
  if (!last)
    first = last = newSetValueRec;
  else {
    last->next = newSetValueRec;
    last = newSetValueRec;  
  }
}

void SetValueRecList::add(Uint32 anAttrId, const char* aValue, Uint32 len) 
{
  SetValueRec* newSetValueRec = new SetValueRec();

  newSetValueRec->stype = SetValueRec::SET_STRING_ATTR2;
  newSetValueRec->anAttrId = anAttrId;
  newSetValueRec->stringStruct.aStringValue = (char *) malloc(len);
  strlcpy(newSetValueRec->stringStruct.aStringValue, aValue, len);
   if (!last)
    first = last = newSetValueRec;
  else {
    last->next = newSetValueRec;
    last = newSetValueRec;  
  }
}

void SetValueRecList::add(Uint32 anAttrId, Int32 aValue) 
{
  SetValueRec* newSetValueRec = new SetValueRec();

  newSetValueRec->stype = SetValueRec::SET_INT32_ATTR2;
  newSetValueRec->anAttrId = anAttrId;
  newSetValueRec->anInt32Value = aValue;
  last->next = newSetValueRec;
  last = newSetValueRec;  
}

void SetValueRecList::add(Uint32 anAttrId, Uint32 aValue) 
{
  SetValueRec* newSetValueRec = new SetValueRec();

  newSetValueRec->stype = SetValueRec::SET_UINT32_ATTR2;
  newSetValueRec->anAttrId = anAttrId;
  newSetValueRec->anUint32Value = aValue;
  if (!last)
    first = last = newSetValueRec;
  else {
    last->next = newSetValueRec;
    last = newSetValueRec;  
  }
}

void SetValueRecList::add(Uint32 anAttrId, Int64 aValue) 
{
  SetValueRec* newSetValueRec = new SetValueRec();

  newSetValueRec->stype = SetValueRec::SET_INT64_ATTR2;
  newSetValueRec->anAttrId = anAttrId;
  newSetValueRec->anInt64Value = aValue;
  if (!last)
    first = last = newSetValueRec;
  else {
    last->next = newSetValueRec;
    last = newSetValueRec;  
  }
}

void SetValueRecList::add(Uint32 anAttrId, Uint64 aValue) 
{
  SetValueRec* newSetValueRec = new SetValueRec();

  newSetValueRec->stype = SetValueRec::SET_UINT64_ATTR2;
  newSetValueRec->anAttrId = anAttrId;
  newSetValueRec->anUint64Value = aValue;
  if (!last)
    first = last = newSetValueRec;
  else {
    last->next = newSetValueRec;
    last = newSetValueRec;  
  }
}

void SetValueRecList::add(Uint32 anAttrId, float aValue) 
{
  SetValueRec* newSetValueRec = new SetValueRec();

  newSetValueRec->stype = SetValueRec::SET_FLOAT_ATTR2;
  newSetValueRec->anAttrId = anAttrId;
  newSetValueRec->aFloatValue = aValue;
   if (!last)
    first = last = newSetValueRec;
  else {
    last->next = newSetValueRec;
    last = newSetValueRec;  
  }
}

void SetValueRecList::add(Uint32 anAttrId, double aValue) 
{
  SetValueRec* newSetValueRec = new SetValueRec();

  newSetValueRec->stype = SetValueRec::SET_DOUBLE_ATTR2;
  newSetValueRec->anAttrId = anAttrId;
  newSetValueRec->aDoubleValue = aValue;
  if (!last)
    first = last = newSetValueRec;
  else {
    last->next = newSetValueRec;
    last = newSetValueRec;  
  }
}

void 
SetValueRecList::callSetValueFn(SetValueRec& aSetValueRec, NdbOperation& oper)
{
  switch(aSetValueRec.stype) {
  case(SetValueRec::SET_STRING_ATTR1):
    oper.setValue(aSetValueRec.anAttrName, aSetValueRec.stringStruct.aStringValue, aSetValueRec.stringStruct.len);
    break;
  case(SetValueRec::SET_INT32_ATTR1):
    oper.setValue(aSetValueRec.anAttrName, aSetValueRec.anInt32Value);
    break;
  case(SetValueRec::SET_UINT32_ATTR1):
    oper.setValue(aSetValueRec.anAttrName, aSetValueRec.anUint32Value);
    break;
  case(SetValueRec::SET_INT64_ATTR1):
    oper.setValue(aSetValueRec.anAttrName, aSetValueRec.anInt64Value);
    break;
  case(SetValueRec::SET_UINT64_ATTR1):
    oper.setValue(aSetValueRec.anAttrName, aSetValueRec.anUint64Value);
    break;
  case(SetValueRec::SET_FLOAT_ATTR1):
    oper.setValue(aSetValueRec.anAttrName, aSetValueRec.aFloatValue);
    break;
  case(SetValueRec::SET_DOUBLE_ATTR1):
    oper.setValue(aSetValueRec.anAttrName, aSetValueRec.aDoubleValue);
    break;
  case(SetValueRec::SET_STRING_ATTR2):
    oper.setValue(aSetValueRec.anAttrId, aSetValueRec.stringStruct.aStringValue, aSetValueRec.stringStruct.len);
    break;
  case(SetValueRec::SET_INT32_ATTR2):
    oper.setValue(aSetValueRec.anAttrId, aSetValueRec.anInt32Value);
    break;
  case(SetValueRec::SET_UINT32_ATTR2):
    oper.setValue(aSetValueRec.anAttrId, aSetValueRec.anUint32Value);
    break;
  case(SetValueRec::SET_INT64_ATTR2):
    oper.setValue(aSetValueRec.anAttrId, aSetValueRec.anInt64Value);
    break;
  case(SetValueRec::SET_UINT64_ATTR2):
    oper.setValue(aSetValueRec.anAttrId, aSetValueRec.anUint64Value);
    break;
  case(SetValueRec::SET_FLOAT_ATTR2):
    oper.setValue(aSetValueRec.anAttrId, aSetValueRec.aFloatValue);
    break;
  case(SetValueRec::SET_DOUBLE_ATTR2):
    oper.setValue(aSetValueRec.anAttrId, aSetValueRec.aDoubleValue);
    break;
  }
}

SetValueRec::~SetValueRec() 
{
  if ((stype == SET_STRING_ATTR1) ||
      (stype == SET_INT32_ATTR1)  ||
      (stype == SET_UINT32_ATTR1) ||
      (stype == SET_INT64_ATTR1) ||
      (stype == SET_UINT64_ATTR1) ||
      (stype == SET_FLOAT_ATTR1) ||
      (stype == SET_DOUBLE_ATTR1))
    free(anAttrName);

  if ((stype == SET_STRING_ATTR1) ||
      (stype == SET_STRING_ATTR2))
    free(stringStruct.aStringValue);
  if (next) delete next;
  next = 0;
}

int
NdbScanOperation::equal_impl(const NdbColumnImpl* anAttrObject, 
			     const char* aValue, 
			     Uint32 len){
  return setBound(anAttrObject, BoundEQ, aValue, len);
}



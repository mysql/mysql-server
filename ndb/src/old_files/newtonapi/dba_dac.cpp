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


#include "dba_internal.hpp"
#include <NdbSleep.h>

static void
DBA__ErrorMapping(const NdbError & err, DBA_Error_t * status){
  switch(err.classification){
  case NdbError::ConstraintViolation:
    * status = DBA_CONSTRAINT_VIOLATION;
    break;
    
  case NdbError::NoDataFound:
    * status = DBA_NO_DATA;
    break;
    
  case NdbError::TemporaryResourceError:
  case NdbError::NodeRecoveryError:
    * status = DBA_TEMPORARY_ERROR;
    break;
    
  case NdbError::InsufficientSpace:
    * status = DBA_INSUFFICIENT_SPACE;
    break;
    
  case NdbError::UnknownResultError:
    * status = DBA_UNKNOWN_RESULT;
    break;
    
  case NdbError::OverloadError:
    * status = DBA_OVERLOAD;
    break;
    
  case NdbError::TimeoutExpired:
    * status = DBA_TIMEOUT;
    break;

  case NdbError::SchemaError:
    * status = DBA_SCHEMA_ERROR;
    break;

  case NdbError::ApplicationError:
    * status = DBA_APPLICATION_ERROR;
    break;

  case NdbError::InternalError:
  default:
    * status = DBA_NDB_ERROR;
    break;
  }
}

/**
 * Map between NDB error codes and DBA error codes
 */
static
void
DBA__CallbackErrorCodeMapping(int errorCode, 
			      NdbConnection * connection,
			      DBA_Error_t * status,
			      DBA_ErrorCode_t * errCode) {
  if(errorCode == 0){
    * status  = DBA_NO_ERROR;
    * errCode = 0;
    return;
  }
  const NdbError & err = connection->getNdbError();
  DBA__ErrorMapping(err, status);
  * errCode = err.code;
}

/**
 * When startTransaction fails
 */
static
void
DBA__ConnectionErrorMapping(Ndb * theNdb){
  const NdbError & err = theNdb->getNdbError();

  DBA_Error_t status;
  DBA__ErrorMapping(err, &status);
  
  DBA__SetLatestError(status, err.code,
		      err.message);
}

/**
 * When getNdbOperation fails
 */
static
void
DBA__OperationErrorMapping(Ndb * theNdb, NdbConnection * con){
  const NdbError & err = theNdb->getNdbError();

  DBA_Error_t status;
  DBA__ErrorMapping(err, &status);
  
  DBA__SetLatestError(status, err.code,
		      err.message);
}

/**
 * When equal/get/set value fails
 */
static
void
DBA__EqualErrorMapping(Ndb * theNdb, NdbConnection * con, NdbOperation * op){
  const NdbError & err = theNdb->getNdbError();

  DBA_Error_t status;
  DBA__ErrorMapping(err, &status);
  
  DBA__SetLatestError(status, err.code,
		      err.message);
}

static
void 
NewtonCallback(int errorCode,
	       NdbConnection * connection,
	       void * anyObject){
  
  DBA_AsyncCallbackFn_t CbFunc = (DBA_AsyncCallbackFn_t)anyObject;
  DBA_ReqId_t  ReqId = (DBA_ReqId_t) connection;
  
  DBA_Error_t     Status = (DBA_Error_t) errorCode;
  DBA_ErrorCode_t Impl_Status ;
  
  DBA__CallbackErrorCodeMapping(errorCode, connection, &Status, &Impl_Status);
  
  DBA__TheNdb->closeTransaction(connection);
  
  DBA__RecvTransactions++;

  CbFunc(ReqId, Status, Impl_Status);
}

/**
 * Start transaction
 */
NdbConnection *
startTransaction(){
  NdbConnection * con = DBA__TheNdb->startTransaction();
  if(con != 0)
    return con;

  const int _t = (DBA__SentTransactions - DBA__RecvTransactions);
  const int t = (_t>0?_t:-_t);
  
  if(!(DBA__TheNdb->getNdbError().code == 4006 && t > 1000)){
    DBA_DEBUG("DBA__TheNdb->getNdbError() = " << 
	      DBA__TheNdb->getNdbError());
  }
  
  int sum = 0;
  int sleepTime = 10;
  for(; con == 0 && sum < DBA__StartTransactionTimout; ){
    NdbMutex_Unlock(DBA__TheNewtonMutex);
    NdbSleep_MilliSleep(sleepTime);
    NdbMutex_Lock(DBA__TheNewtonMutex);
    con = DBA__TheNdb->startTransaction();
    
    sum       += sleepTime;
    sleepTime += 10;
  }
  
  return con;
}

#define CHECK_BINDINGS(Bindings) \
  if(!DBA__ValidBinding(Bindings)){ \
    DBA__SetLatestError(DBA_APPLICATION_ERROR, 0, "Invalid bindings"); \
    return DBA_INVALID_REQID; \
  } 

#define CHECK_BINDINGS2(Bindings, NbBindings) \
  if(!DBA__ValidBindings(Bindings, NbBindings)){ \
    DBA__SetLatestError(DBA_APPLICATION_ERROR, 0, "Invalid bindings"); \
    return DBA_INVALID_REQID; \
  }

#define CHECK_CONNECTION(Connection) \
  if(Connection == 0){ \
    DBA__ConnectionErrorMapping(DBA__TheNdb); \
    NdbMutex_Unlock(DBA__TheNewtonMutex); \
    return DBA_INVALID_REQID; \
  }

#define CHECK_OPERATION(Connection, Operation) \
  if(Operation == 0){ \
    DBA__OperationErrorMapping(DBA__TheNdb, Connection); \
    DBA__TheNdb->closeTransaction(Connection); \
    NdbMutex_Unlock(DBA__TheNewtonMutex); \
    return DBA_INVALID_REQID; \
  }

#define EQUAL_ERROR(Connection, Operation) { \
    DBA__EqualErrorMapping(DBA__TheNdb, Connection, Operation); \
    DBA__TheNdb->closeTransaction(Connection); \
    NdbMutex_Unlock(DBA__TheNewtonMutex); \
    return DBA_INVALID_REQID; \
  }

/****** THIS LINE IS 80 CHARACTERS WIDE - DO *NOT* EXCEED 80 CHARACTERS! ****/

extern "C" 
DBA_ReqId_t
DBA_ReadRows( const DBA_Binding_t* pBindings, void* const * _pData,
	      int NbRows,
	      DBA_AsyncCallbackFn_t CbFunc ) {
  
  CHECK_BINDINGS(pBindings);
  
  NdbMutex_Lock(DBA__TheNewtonMutex);
  NdbConnection * con = startTransaction();
  
  CHECK_CONNECTION(con);
  
  for(int i = 0; i<NbRows; i++){
    NdbOperation  * op  = con->getNdbOperation(pBindings->tableName);
    
    CHECK_OPERATION(con, op);
    
    op->simpleRead();
    
    void * pData = _pData[i];
    
    if(!DBA__EqualGetValue(op, pBindings, pData)){
      EQUAL_ERROR(con, op);
    }
  }

  con->executeAsynchPrepare(Commit,
			    NewtonCallback,
			    (void*)CbFunc);
  
  DBA__SentTransactions++;
  
  NdbMutex_Unlock(DBA__TheNewtonMutex);
  return (DBA_ReqId_t) con;
}

extern "C"
DBA_ReqId_t
DBA_ArrayReadRows( const DBA_Binding_t* pBindings, void * pData,
		   int NbRows,
		   DBA_AsyncCallbackFn_t CbFunc ){
  CHECK_BINDINGS(pBindings);

  NdbMutex_Lock(DBA__TheNewtonMutex);
  NdbConnection * con = startTransaction();

  CHECK_CONNECTION(con);
  
  for(int i = 0; i<NbRows; i++){
    NdbOperation * op  = con->getNdbOperation(pBindings->tableName);

    CHECK_OPERATION(con, op);
    
    op->simpleRead();
    
    if(!DBA__EqualGetValue(op, pBindings, 
			   ((char*)pData)+i*pBindings->structSz)){
      EQUAL_ERROR(con, op);
    }
  }

  con->executeAsynchPrepare(Commit,
			    NewtonCallback,
			    (void*)CbFunc);
  
  DBA__SentTransactions++;

  NdbMutex_Unlock(DBA__TheNewtonMutex);
  return (DBA_ReqId_t) con;
}

extern "C"
DBA_ReqId_t
DBA_MultiReadRow(const DBA_Binding_t * const * pBindings,
		 void * const * pData,
		 int NbBindings,
		 DBA_AsyncCallbackFn_t CbFunc ) {
  CHECK_BINDINGS2(pBindings, NbBindings);
    
  NdbMutex_Lock(DBA__TheNewtonMutex);
  NdbConnection * con = startTransaction();
  
  CHECK_CONNECTION(con);
  
  for(int i = 0; i<NbBindings; i++){
    NdbOperation * op  = con->getNdbOperation(pBindings[i]->tableName);
    
    CHECK_OPERATION(con, op);

    op->simpleRead();
    
    if(!DBA__EqualGetValue(op, pBindings[i], pData[i])){
      EQUAL_ERROR(con, op);
    }
  }
  
  con->executeAsynchPrepare(Commit,
			    NewtonCallback,
			    (void*)CbFunc);

  DBA__SentTransactions++;
  
  NdbMutex_Unlock(DBA__TheNewtonMutex);
  return (DBA_ReqId_t) con;
}

/****** THIS LINE IS 80 CHARACTERS WIDE - DO *NOT* EXCEED 80 CHARACTERS! ****/

extern "C" 
DBA_ReqId_t
DBA_InsertRows( const DBA_Binding_t* pBindings, const void * const * _pData,
		int NbRows,
		DBA_AsyncCallbackFn_t CbFunc ) {
  CHECK_BINDINGS(pBindings);

  NdbMutex_Lock(DBA__TheNewtonMutex);
  NdbConnection * con = startTransaction();

  CHECK_CONNECTION(con);
  
  for(int i = 0; i<NbRows; i++){
    NdbOperation  * op  = con->getNdbOperation(pBindings->tableName);

    CHECK_OPERATION(con, op);

    op->insertTuple();
    
    const void * pData = _pData[i];

    if(!DBA__EqualSetValue(op, pBindings, pData)){
      EQUAL_ERROR(con, op);
    }
  }

  con->executeAsynchPrepare(Commit,
			    NewtonCallback,
			    (void*)CbFunc);
  
  
  DBA__SentTransactions++;

  NdbMutex_Unlock(DBA__TheNewtonMutex);
  return (DBA_ReqId_t) con;
}

extern "C"
DBA_ReqId_t
DBA_ArrayInsertRows( const DBA_Binding_t* pBindings, const void * pData,
		     int NbRows,
		     DBA_AsyncCallbackFn_t CbFunc ){
  CHECK_BINDINGS(pBindings);

  NdbMutex_Lock(DBA__TheNewtonMutex);
  NdbConnection * con = startTransaction();

  CHECK_CONNECTION(con);
  
  for(int i = 0; i<NbRows; i++){
    NdbOperation  * op  = con->getNdbOperation(pBindings->tableName);

    CHECK_OPERATION(con, op);

    op->insertTuple();
    
    if(!DBA__EqualSetValue(op, pBindings, 
			   ((char*)pData)+i*pBindings->structSz)){
      EQUAL_ERROR(con, op);
    }
  }

  con->executeAsynchPrepare(Commit,
			    NewtonCallback,
			    (void*)CbFunc);
  
  DBA__SentTransactions++;

  NdbMutex_Unlock(DBA__TheNewtonMutex);
  return (DBA_ReqId_t) con;
}

extern "C"
DBA_ReqId_t
DBA_MultiInsertRow(const DBA_Binding_t * const * pBindings,
		   const void * const * pData,
		   int NbBindings,
		   DBA_AsyncCallbackFn_t CbFunc ) {
  CHECK_BINDINGS2(pBindings, NbBindings);

  NdbMutex_Lock(DBA__TheNewtonMutex);
  NdbConnection * con = startTransaction();

  CHECK_CONNECTION(con);
  
  for(int i = 0; i<NbBindings; i++){
    NdbOperation  * op  = con->getNdbOperation(pBindings[i]->tableName);

    CHECK_OPERATION(con, op);

    op->insertTuple();

    if(!DBA__EqualSetValue(op, pBindings[i], pData[i])){
      EQUAL_ERROR(con, op);
    }
  }
  
  con->executeAsynchPrepare(Commit,
			    NewtonCallback,
			    (void*)CbFunc);
  
  DBA__SentTransactions++;

  NdbMutex_Unlock(DBA__TheNewtonMutex);
  return (DBA_ReqId_t) con;
}

/****** THIS LINE IS 80 CHARACTERS WIDE - DO *NOT* EXCEED 80 CHARACTERS! ****/

extern "C" 
DBA_ReqId_t
DBA_UpdateRows( const DBA_Binding_t* pBindings, const void * const * _pData,
		int NbRows,
		DBA_AsyncCallbackFn_t CbFunc ) {
  CHECK_BINDINGS(pBindings);

  NdbMutex_Lock(DBA__TheNewtonMutex);
  NdbConnection * con = startTransaction();

  CHECK_CONNECTION(con);
  
  for(int i = 0; i<NbRows; i++){
    NdbOperation  * op  = con->getNdbOperation(pBindings->tableName);

    CHECK_OPERATION(con, op);

    op->updateTuple();
    
    const void * pData = _pData[i];

    if(!DBA__EqualSetValue(op, pBindings, pData)){
      EQUAL_ERROR(con, op);
    }
  }

  con->executeAsynchPrepare(Commit,
			    NewtonCallback,
			    (void*)CbFunc);
  
  
  DBA__SentTransactions++;

  NdbMutex_Unlock(DBA__TheNewtonMutex);
  return (DBA_ReqId_t) con;
}

extern "C"
DBA_ReqId_t
DBA_ArrayUpdateRows( const DBA_Binding_t* pBindings, const void * pData,
		     int NbRows,
		     DBA_AsyncCallbackFn_t CbFunc ){
  CHECK_BINDINGS(pBindings);

  NdbMutex_Lock(DBA__TheNewtonMutex);
  NdbConnection * con = startTransaction();

  CHECK_CONNECTION(con);
  
  for(int i = 0; i<NbRows; i++){
    NdbOperation  * op  = con->getNdbOperation(pBindings->tableName);

    CHECK_OPERATION(con, op);

    op->updateTuple();
    
    if(!DBA__EqualSetValue(op, pBindings, 
				   ((char*)pData)+i*pBindings->structSz)){
      EQUAL_ERROR(con, op);
    }
  }

  con->executeAsynchPrepare(Commit,
			    NewtonCallback,
			    (void*)CbFunc);
  
  DBA__SentTransactions++;

  NdbMutex_Unlock(DBA__TheNewtonMutex);
  return (DBA_ReqId_t) con;
}

extern "C"
DBA_ReqId_t
DBA_MultiUpdateRow(const DBA_Binding_t * const * pBindings,
		   const void * const * pData,
		   int NbBindings,
		   DBA_AsyncCallbackFn_t CbFunc ) {
  CHECK_BINDINGS2(pBindings, NbBindings);

  NdbMutex_Lock(DBA__TheNewtonMutex);
  NdbConnection * con = startTransaction();

  CHECK_CONNECTION(con);
  
  for(int i = 0; i<NbBindings; i++){
    NdbOperation  * op  = con->getNdbOperation(pBindings[i]->tableName);

    CHECK_OPERATION(con, op);

    op->updateTuple();
    
    if(!DBA__EqualSetValue(op, pBindings[i], pData[i])){
      EQUAL_ERROR(con, op);
    }
  }
  
  con->executeAsynchPrepare(Commit,
			    NewtonCallback,
			    (void*)CbFunc);
  
  DBA__SentTransactions++;

  NdbMutex_Unlock(DBA__TheNewtonMutex);
  return (DBA_ReqId_t) con;
}

/****** THIS LINE IS 80 CHARACTERS WIDE - DO *NOT* EXCEED 80 CHARACTERS! ****/

extern "C" 
DBA_ReqId_t
DBA_WriteRows( const DBA_Binding_t* pBindings, const void * const * _pData,
	       int NbRows,
	       DBA_AsyncCallbackFn_t CbFunc ) {
  CHECK_BINDINGS(pBindings);

  NdbMutex_Lock(DBA__TheNewtonMutex);
  NdbConnection * con = startTransaction();

  CHECK_CONNECTION(con);
  
  for(int i = 0; i<NbRows; i++){
    NdbOperation  * op  = con->getNdbOperation(pBindings->tableName);

    CHECK_OPERATION(con, op);

    op->writeTuple();
    
    const void * pData = _pData[i];

    if(!DBA__EqualSetValue(op, pBindings, pData)){
      EQUAL_ERROR(con, op);
    }
  }

  con->executeAsynchPrepare(Commit,
			    NewtonCallback,
			    (void*)CbFunc);
  
  
  DBA__SentTransactions++;

  NdbMutex_Unlock(DBA__TheNewtonMutex);
  return (DBA_ReqId_t) con;
}

extern "C"
DBA_ReqId_t
DBA_ArrayWriteRows( const DBA_Binding_t* pBindings, const void * pData,
		    int NbRows,
		    DBA_AsyncCallbackFn_t CbFunc ){
  CHECK_BINDINGS(pBindings);

  NdbMutex_Lock(DBA__TheNewtonMutex);
  NdbConnection * con = startTransaction();

  CHECK_CONNECTION(con);
  
  for(int i = 0; i<NbRows; i++){
    NdbOperation  * op  = con->getNdbOperation(pBindings->tableName);

    CHECK_OPERATION(con, op);

    op->writeTuple();
    
    if(!DBA__EqualSetValue(op, pBindings, 
				   ((char*)pData)+i*pBindings->structSz)){
      EQUAL_ERROR(con, op);
    }
  }

  con->executeAsynchPrepare(Commit,
			    NewtonCallback,
			    (void*)CbFunc);
  
  DBA__SentTransactions++;

  NdbMutex_Unlock(DBA__TheNewtonMutex);
  return (DBA_ReqId_t) con;
}

extern "C"
DBA_ReqId_t
DBA_MultiWriteRow(const DBA_Binding_t * const * pBindings,
		  const void * const * pData,
		  int NbBindings,
		  DBA_AsyncCallbackFn_t CbFunc ) {
  CHECK_BINDINGS2(pBindings, NbBindings);
  
  NdbMutex_Lock(DBA__TheNewtonMutex);
  NdbConnection * con = startTransaction();
  
  CHECK_CONNECTION(con);

  for(int i = 0; i<NbBindings; i++){
    NdbOperation  * op  = con->getNdbOperation(pBindings[i]->tableName);

    CHECK_OPERATION(con, op);

    op->writeTuple();
    
    if(!DBA__EqualSetValue(op, pBindings[i], pData[i])){
      EQUAL_ERROR(con, op);
    }
  }
  
  con->executeAsynchPrepare(Commit,
			    NewtonCallback,
			    (void*)CbFunc);
  
  DBA__SentTransactions++;

  NdbMutex_Unlock(DBA__TheNewtonMutex);
  return (DBA_ReqId_t) con;
}

/****** THIS LINE IS 80 CHARACTERS WIDE - DO *NOT* EXCEED 80 CHARACTERS! ****/

extern "C" 
DBA_ReqId_t
DBA_DeleteRows( const DBA_Binding_t* pBindings, const void * const * _pData,
		int NbRows,
		DBA_AsyncCallbackFn_t CbFunc ) {
  CHECK_BINDINGS(pBindings);

  NdbMutex_Lock(DBA__TheNewtonMutex);
  NdbConnection * con = startTransaction();

  CHECK_CONNECTION(con);

  for(int i = 0; i<NbRows; i++){
    NdbOperation  * op  = con->getNdbOperation(pBindings->tableName);

    CHECK_OPERATION(con, op);

    op->deleteTuple();
    
    const void * pData = _pData[i];
    
    if(!DBA__Equal(op, pBindings, pData)){
      EQUAL_ERROR(con, op);
    }
  }

  con->executeAsynchPrepare(Commit,
			    NewtonCallback,
			    (void*)CbFunc);
  
  DBA__SentTransactions++;
  
  NdbMutex_Unlock(DBA__TheNewtonMutex);
  return (DBA_ReqId_t) con;
}

extern "C"
DBA_ReqId_t
DBA_ArrayDeleteRows( const DBA_Binding_t* pBindings, const void * pData,
		     int NbRows,
		     DBA_AsyncCallbackFn_t CbFunc ){
  CHECK_BINDINGS(pBindings);

  NdbMutex_Lock(DBA__TheNewtonMutex);
  NdbConnection * con = startTransaction();

  CHECK_CONNECTION(con);

  for(int i = 0; i<NbRows; i++){
    NdbOperation  * op  = con->getNdbOperation(pBindings->tableName);

    CHECK_OPERATION(con, op);

    op->deleteTuple();
    
    if(!DBA__Equal(op, pBindings, 
			   ((char*)pData)+i*pBindings->structSz)){
      EQUAL_ERROR(con, op);
    }
  }

  con->executeAsynchPrepare(Commit,
			    NewtonCallback,
			    (void*)CbFunc);
  
  DBA__SentTransactions++;

  NdbMutex_Unlock(DBA__TheNewtonMutex);
  return (DBA_ReqId_t) con;
}

extern "C"
DBA_ReqId_t
DBA_MultiDeleteRow(const DBA_Binding_t * const * pBindings,
		   const void * const * pData,
		   int NbBindings,
		   DBA_AsyncCallbackFn_t CbFunc ) {
  CHECK_BINDINGS2(pBindings, NbBindings);

  NdbMutex_Lock(DBA__TheNewtonMutex);
  NdbConnection * con = startTransaction();

  CHECK_CONNECTION(con);
  
  for(int i = 0; i<NbBindings; i++){
    NdbOperation  * op  = con->getNdbOperation(pBindings[i]->tableName);

    CHECK_OPERATION(con, op);

    op->deleteTuple();
    
    if(!DBA__Equal(op, pBindings[i], pData[i])){
      EQUAL_ERROR(con, op);
    }
  }
  
  con->executeAsynchPrepare(Commit,
			    NewtonCallback,
			    (void*)CbFunc);
  
  DBA__SentTransactions++;

  NdbMutex_Unlock(DBA__TheNewtonMutex);
  return (DBA_ReqId_t) con;
}

/****** THIS LINE IS 80 CHARACTERS WIDE - DO *NOT* EXCEED 80 CHARACTERS! ****/

bool
DBA__EqualGetValue(NdbOperation * op,
		   const DBA_Binding_t* pBindings,
		   void * pData){
  for(int i = 0; i<pBindings->noOfKeys; i++){
    if(op->equal(pBindings->keyIds[i], 
		 (const char*)pData+pBindings->keyOffsets[i]) == -1){
      return false;
    }
  }
  
  for(int i = 0; i<pBindings->noOfColumns; i++){
    if(op->getValue(pBindings->columnIds[i], 
		    (char*)pData+pBindings->columnOffsets[i]) == 0){
      return false;
    }
  }

  for(int i = 0; i<pBindings->noOfSubBindings; i++){
    void * tData = *(void**)((char *)pData+pBindings->subBindingOffsets[i]);
    const DBA_Binding_t * tBinding = pBindings->subBindings[i];
    if(!DBA__EqualGetValue(op, tBinding, tData))
      return false;
  }
  
  return true;
}

bool
DBA__EqualSetValue(NdbOperation * op,
		   const DBA_Binding_t* pBindings,
		   const void * pData){
  
  for(int i = 0; i<pBindings->noOfKeys; i++){
    if(op->equal(pBindings->keyIds[i], 
		 (const char*)pData+pBindings->keyOffsets[i]) == -1){
      return false;
    }
  }
  
  for(int i = 0; i<pBindings->noOfColumns; i++){
    if(op->setValue(pBindings->columnIds[i], 
		    (char*)pData+pBindings->columnOffsets[i]) == -1){
      return false;
    }
  }

  for(int i = 0; i<pBindings->noOfSubBindings; i++){
    void * tData = * (void**)((char *)pData+pBindings->subBindingOffsets[i]);
    const DBA_Binding_t * tBinding = pBindings->subBindings[i];
    if(!DBA__EqualSetValue(op, tBinding, tData))
      return false;
  }
  
  return true;
}

bool
DBA__Equal(NdbOperation * op,
	   const DBA_Binding_t* pBindings,
	   const void * pData){
  
  for(int i = 0; i<pBindings->noOfKeys; i++)
    if(op->equal(pBindings->keyIds[i], 
		 (const char*)pData+pBindings->keyOffsets[i]) == -1){
      return false;
    }
  
  for(int i = 0; i<pBindings->noOfSubBindings; i++){
    void * tData = *(void**)((char *)pData+pBindings->subBindingOffsets[i]);
    const DBA_Binding_t * tBinding = pBindings->subBindings[i];
    if(!DBA__Equal(op, tBinding, tData))
      return false;
  }
  
  return true;
}


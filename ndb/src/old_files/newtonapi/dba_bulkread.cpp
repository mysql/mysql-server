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

struct DBA__BulkReadData {
  const DBA_Binding_t * const * pBindings; // The bindings
  DBA_BulkReadResultSet_t * pData;       // The data 
  
  int NbRows;                               // NbRows per binding
  int NbBindings;                           // NbBindings
  int TotalRows;                            // Total rows (NbRows*NbBindings)
  
  DBA_AsyncCallbackFn_t CbFunc;             // Users callback
  DBA_ReqId_t RequestId;                    // Users request id
  DBA_Error_t Status;                       // Request status
  DBA_ErrorCode_t ErrorCode;                /**< Request error
					       Only valid if request is
					       aborted */
  
  int RowsSubmitted;                        // No of read sent to NDB
  int RowsAcknowledged;                     // No of read responses
  int OpPerTrans;                           // Operations per transaction
  
  struct Index {
    int binding;
    int row;
    int datarow;

    void init() { row = binding = datarow = 0;}
    void next(int rows) { 
      datarow++; row++; 
      if(row == rows){ row = 0; binding++; } 
    }
  };
  Index lastSend;
  Index nextSend;
  
  /**
   * If "simple" bulkread
   * use this storage
   */
  const DBA_Binding_t    * bindings[1];

  DBA__BulkReadData() {
    RequestId = DBA_INVALID_REQID;
  }
  void ProcessBulkRead();
  bool ProcessCallback(int errorCode, NdbConnection * connection);
};

static
void 
NewtonCallback(int errorCode,
	       NdbConnection * connection,
	       void * anyObject){

  DBA__BulkReadData * brd = (DBA__BulkReadData*)anyObject;
  
  brd->ProcessCallback(errorCode, connection);

  DBA__TheNdb->closeTransaction(connection);

  if(brd->RowsSubmitted == brd->TotalRows){
    
    /**
     * The entire bulk read is finished,
     * call users callback
     */
    DBA_ReqId_t reqId = brd->RequestId;
    
    // Invalidate BulkReadData
    brd->RequestId = DBA_INVALID_REQID;

    brd->CbFunc(reqId, brd->Status, brd->ErrorCode);
    return;
  }
  
  brd->ProcessBulkRead();
}

/**
 * A BulkReadData structure
 */
static DBA__BulkReadData theBRD;

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

DBA_ReqId_t
DBA_BulkReadRows(const DBA_Binding_t * pBindings,
		 DBA_BulkReadResultSet_t pData[],
		 int NbRows,
		 DBA_AsyncCallbackFn_t CbFunc ){

  CHECK_BINDINGS(pBindings);

  DBA__BulkReadData * brd = &theBRD;
  
  NdbMutex_Lock(DBA__TheNewtonMutex);

  if(brd->RequestId != DBA_INVALID_REQID){
    DBA__SetLatestError(DBA_ERROR, 0,
			"DBA only permits 1 concurrent bulkread");
    
    NdbMutex_Unlock(DBA__TheNewtonMutex);
    return DBA_ERROR;
  }
  
  theBRD.RequestId = 1;
  
  /**
   * 
   */
  brd->bindings[0] = pBindings;
  brd->pBindings   = brd->bindings;
  brd->pData       = pData;

  /**
   * Control data
   */
  brd->NbRows         = NbRows;
  brd->NbBindings     = 1;
  brd->TotalRows      = NbRows;
  brd->CbFunc         = CbFunc;
  brd->Status         = DBA_NO_ERROR;
  brd->ErrorCode      = 0;
  brd->OpPerTrans     = DBA__BulkReadCount;

  brd->RowsSubmitted    = 0;
  brd->RowsAcknowledged = 0;

  brd->lastSend.init();
  brd->nextSend.init();

  brd->ProcessBulkRead();
  NdbMutex_Unlock(DBA__TheNewtonMutex);
  
  return brd->RequestId;
}

DBA_ReqId_t
DBA_BulkMultiReadRows(const DBA_Binding_t * const * pBindings,
		      DBA_BulkReadResultSet_t pData[],
		      int NbBindings,
		      int NbRows,
		      DBA_AsyncCallbackFn_t CbFunc ){

  CHECK_BINDINGS2(pBindings, NbBindings);

  DBA__BulkReadData * brd = &theBRD;

  NdbMutex_Lock(DBA__TheNewtonMutex);
  
  if(brd->RequestId != DBA_INVALID_REQID){
    DBA__SetLatestError(DBA_ERROR, 0,
			"DBA only permits 1 concurrent bulkread");

    NdbMutex_Unlock(DBA__TheNewtonMutex);
    return DBA_ERROR;
  }
  
  brd->RequestId = 1;
  
  /**
   * 
   */
  brd->pBindings  = pBindings;
  brd->pData      = pData;

  /**
   * Control data
   */
  brd->NbRows        = NbRows;
  brd->NbBindings    = NbBindings;
  brd->TotalRows     = (NbRows * NbBindings);
  brd->CbFunc        = CbFunc;
  brd->Status        = DBA_NO_ERROR;
  brd->ErrorCode     = 0;
  brd->OpPerTrans    = DBA__BulkReadCount;

  brd->RowsSubmitted    = 0;
  brd->RowsAcknowledged = 0;
  
  brd->lastSend.init();
  brd->nextSend.init();
  
  brd->ProcessBulkRead();
  
  NdbMutex_Unlock(DBA__TheNewtonMutex);  

  return brd->RequestId;
}

bool
DBA__BulkReadData::ProcessCallback(int errorCode, NdbConnection * con){

  Index tmp = lastSend;
  const NdbOperation * op = con->getNextCompletedOperation(0);
  
  for(int i = 0; i<OpPerTrans && RowsAcknowledged < RowsSubmitted; i++){
    require(op != 0);
    if(op->getNdbError().code == 0)
      pData[tmp.datarow].RowFoundIndicator = 1;
    else
      pData[tmp.datarow].RowFoundIndicator = 0;

    RowsAcknowledged++;
    tmp.next(NbRows);
    op = con->getNextCompletedOperation(op);
  }
  return true;
}

void
DBA__BulkReadData::ProcessBulkRead(){

  NdbConnection * con = DBA__TheNdb->startTransaction();

  Index tmp = nextSend;

  for(int i = 0; i<OpPerTrans && RowsSubmitted < TotalRows; i++){
    
    const DBA_Binding_t * binding = pBindings[tmp.binding];
    void * data = pData[tmp.datarow].DataPtr;
    
    NdbOperation  * op  = con->getNdbOperation(binding->tableName);
    
    op->simpleRead();
    
    require(DBA__EqualGetValue(op, binding, data));
    
    RowsSubmitted++;
    tmp.next(NbRows);
  }

  con->executeAsynchPrepare(Commit,
			    NewtonCallback,
			    (void*)this,
			    CommitAsMuchAsPossible);

  lastSend = nextSend;
  nextSend = tmp;
}

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

#include "DbUtil.hpp"

#include <ndb_version.h>

#include <signaldata/WaitGCP.hpp>
#include <signaldata/KeyInfo.hpp>
#include <signaldata/AttrInfo.hpp>
#include <signaldata/TcKeyConf.hpp>
#include <signaldata/TcKeyFailConf.hpp>
#include <signaldata/GetTabInfo.hpp>
#include <signaldata/DictTabInfo.hpp>

#include <signaldata/UtilSequence.hpp>
#include <signaldata/UtilPrepare.hpp>
#include <signaldata/UtilRelease.hpp>
#include <signaldata/UtilExecute.hpp>
#include <signaldata/UtilLock.hpp>

#include <SectionReader.hpp>
#include <Interpreter.hpp>
#include <AttributeHeader.hpp>

#include <NdbTick.h>


/**************************************************************************
 * ------------------------------------------------------------------------
 *  MODULE:       Startup
 * ------------------------------------------------------------------------
 * 
 *  Constructors, startup, initializations
 **************************************************************************/

DbUtil::DbUtil(const Configuration & conf) :
  SimulatedBlock(DBUTIL, conf),
  c_runningPrepares(c_preparePool),
  c_runningPreparedOperations(c_preparedOperationPool),
  c_seizingTransactions(c_transactionPool),
  c_runningTransactions(c_transactionPool),
  c_lockQueues(c_lockQueuePool)
{
  BLOCK_CONSTRUCTOR(DbUtil);
  
  // Add received signals
  addRecSignal(GSN_STTOR, &DbUtil::execSTTOR);
  addRecSignal(GSN_NDB_STTOR, &DbUtil::execNDB_STTOR);
  addRecSignal(GSN_DUMP_STATE_ORD, &DbUtil::execDUMP_STATE_ORD);
  addRecSignal(GSN_CONTINUEB, &DbUtil::execCONTINUEB);
  
  //addRecSignal(GSN_TCSEIZEREF, &DbUtil::execTCSEIZEREF);
  addRecSignal(GSN_TCSEIZECONF, &DbUtil::execTCSEIZECONF);
  addRecSignal(GSN_TCKEYCONF, &DbUtil::execTCKEYCONF);
  addRecSignal(GSN_TCKEYREF, &DbUtil::execTCKEYREF);
  addRecSignal(GSN_TCROLLBACKREP, &DbUtil::execTCROLLBACKREP);

  //addRecSignal(GSN_TCKEY_FAILCONF, &DbUtil::execTCKEY_FAILCONF);
  //addRecSignal(GSN_TCKEY_FAILREF, &DbUtil::execTCKEY_FAILREF);
  addRecSignal(GSN_TRANSID_AI, &DbUtil::execTRANSID_AI);

  /**
   *  Sequence Service
   */
  addRecSignal(GSN_UTIL_SEQUENCE_REQ, &DbUtil::execUTIL_SEQUENCE_REQ);
  // Debug
  addRecSignal(GSN_UTIL_SEQUENCE_REF, &DbUtil::execUTIL_SEQUENCE_REF);
  addRecSignal(GSN_UTIL_SEQUENCE_CONF, &DbUtil::execUTIL_SEQUENCE_CONF);

  /**
   * Locking
   */
  addRecSignal(GSN_UTIL_CREATE_LOCK_REQ,  &DbUtil::execUTIL_CREATE_LOCK_REQ);
  addRecSignal(GSN_UTIL_DESTROY_LOCK_REQ, &DbUtil::execUTIL_DESTORY_LOCK_REQ);
  addRecSignal(GSN_UTIL_LOCK_REQ,  &DbUtil::execUTIL_LOCK_REQ);
  addRecSignal(GSN_UTIL_UNLOCK_REQ, &DbUtil::execUTIL_UNLOCK_REQ);

  /**
   *  Backend towards Dict
   */
  addRecSignal(GSN_GET_TABINFOREF, &DbUtil::execGET_TABINFOREF);
  addRecSignal(GSN_GET_TABINFO_CONF, &DbUtil::execGET_TABINFO_CONF);

  /**
   *  Prepare / Execute / Release Services
   */
  addRecSignal(GSN_UTIL_PREPARE_REQ,  &DbUtil::execUTIL_PREPARE_REQ);
  addRecSignal(GSN_UTIL_PREPARE_CONF, &DbUtil::execUTIL_PREPARE_CONF);
  addRecSignal(GSN_UTIL_PREPARE_REF,  &DbUtil::execUTIL_PREPARE_REF);

  addRecSignal(GSN_UTIL_EXECUTE_REQ,  &DbUtil::execUTIL_EXECUTE_REQ);
  addRecSignal(GSN_UTIL_EXECUTE_CONF, &DbUtil::execUTIL_EXECUTE_CONF);
  addRecSignal(GSN_UTIL_EXECUTE_REF,  &DbUtil::execUTIL_EXECUTE_REF);

  addRecSignal(GSN_UTIL_RELEASE_REQ,  &DbUtil::execUTIL_RELEASE_REQ);
  addRecSignal(GSN_UTIL_RELEASE_CONF, &DbUtil::execUTIL_RELEASE_CONF);
  addRecSignal(GSN_UTIL_RELEASE_REF,  &DbUtil::execUTIL_RELEASE_REF);

  c_pagePool.setSize(100);
  c_preparePool.setSize(1);            // one parallel prepare at a time
  c_preparedOperationPool.setSize(5);  // three hardcoded, two for test
  c_operationPool.setSize(64);         // 64 parallel operations
  c_transactionPool.setSize(32);       // 16 parallel transactions
  c_attrMappingPool.setSize(100);
  c_dataBufPool.setSize(6000);	       // 6000*11*4 = 264K > 8k+8k*16 = 256k
  {
    SLList<Prepare> tmp(c_preparePool);
    PreparePtr ptr;
    while(tmp.seize(ptr))
      new (ptr.p) Prepare(c_pagePool);
    tmp.release();
  }
  {
    SLList<Operation> tmp(c_operationPool);
    OperationPtr ptr;
    while(tmp.seize(ptr))
      new (ptr.p) Operation(c_dataBufPool, c_dataBufPool, c_dataBufPool);
    tmp.release();
  }
  {
    SLList<PreparedOperation> tmp(c_preparedOperationPool);
    PreparedOperationPtr ptr;
    while(tmp.seize(ptr))
      new (ptr.p) PreparedOperation(c_attrMappingPool, 
				    c_dataBufPool, c_dataBufPool);
    tmp.release();
  }
  {
    SLList<Transaction> tmp(c_transactionPool);
    TransactionPtr ptr;
    while(tmp.seize(ptr))
      new (ptr.p) Transaction(c_pagePool, c_operationPool);
    tmp.release();
  }

  c_lockQueuePool.setSize(5);
  c_lockElementPool.setSize(5);
  c_lockQueues.setSize(8);
}

DbUtil::~DbUtil()
{
}

BLOCK_FUNCTIONS(DbUtil);

void 
DbUtil::releasePrepare(PreparePtr prepPtr) {
  prepPtr.p->preparePages.release();
  c_runningPrepares.release(prepPtr);  // Automatic release in pool
}

void 
DbUtil::releasePreparedOperation(PreparedOperationPtr prepOpPtr) {
  prepOpPtr.p->attrMapping.release();
  prepOpPtr.p->attrInfo.release();
  prepOpPtr.p->rsInfo.release();
  prepOpPtr.p->pkBitmask.clear();
  c_preparedOperationPool.release(prepOpPtr);  // No list holding these structs
}

void
DbUtil::releaseTransaction(TransactionPtr transPtr){
  transPtr.p->executePages.release();
  OperationPtr opPtr;
  for(transPtr.p->operations.first(opPtr); opPtr.i != RNIL; 
      transPtr.p->operations.next(opPtr)){
    opPtr.p->attrInfo.release();
    opPtr.p->keyInfo.release();
    opPtr.p->rs.release();
    if (opPtr.p->prepOp != 0 && opPtr.p->prepOp_i != RNIL) {
      if (opPtr.p->prepOp->releaseFlag) {
	PreparedOperationPtr prepOpPtr;
	prepOpPtr.i = opPtr.p->prepOp_i;
	prepOpPtr.p = opPtr.p->prepOp;
	releasePreparedOperation(prepOpPtr);
      }
    }
  }
  transPtr.p->operations.release();
  c_runningTransactions.release(transPtr);
}

void
DbUtil::execSTTOR(Signal* signal) 
{
  jamEntry();                            

  const Uint32 startphase = signal->theData[1];
  
  if(startphase == 1){
    c_transId[0] = (number() << 20) + (getOwnNodeId() << 8);
    c_transId[1] = 0;
  }
  
  if(startphase == 6){
    hardcodedPrepare();
    connectTc(signal);
  }
  
  signal->theData[0] = 0;
  signal->theData[3] = 1;
  signal->theData[4] = 6;
  signal->theData[5] = 255;
  sendSignal(NDBCNTR_REF, GSN_STTORRY, signal, 6, JBB);

  return;
}

void
DbUtil::execNDB_STTOR(Signal* signal) 
{
  (void)signal;  // Don't want compiler warning

  jamEntry();                            
}


/***************************
 *  Seize a number of TC records 
 *  to use for Util transactions
 */

void
DbUtil::connectTc(Signal* signal){
  
  TransactionPtr ptr;
  while(c_seizingTransactions.seize(ptr)){
    signal->theData[0] = ptr.i << 1; // See TcCommitConf
    signal->theData[1] = reference();
    sendSignal(DBTC_REF, GSN_TCSEIZEREQ, signal, 2, JBB);
  }  
}

void
DbUtil::execTCSEIZECONF(Signal* signal){
  jamEntry();
  
  TransactionPtr ptr;
  ptr.i = signal->theData[0] >> 1;
  c_seizingTransactions.getPtr(ptr, signal->theData[0] >> 1);
  ptr.p->connectPtr = signal->theData[1];
  
  c_seizingTransactions.release(ptr);
}


/**************************************************************************
 * ------------------------------------------------------------------------
 *  MODULE:       Misc
 * ------------------------------------------------------------------------
 *
 *  ContinueB, Dump
 **************************************************************************/

void
DbUtil::execCONTINUEB(Signal* signal){
  jamEntry();
  const Uint32 Tdata0 = signal->theData[0];
  
  switch(Tdata0){
  default:
    ndbrequire(0);
  }
}

void
DbUtil::execDUMP_STATE_ORD(Signal* signal){
  jamEntry();

  /****************************************************************************
   *  SEQUENCE SERVICE
   * 
   *  200 : Simple test of Public Sequence Interface
   *  ----------------------------------------------
   *  - Sends a SEQUENCE_REQ signal to Util (itself)
   */
  const Uint32 tCase = signal->theData[0];
  if(tCase == 200){
    jam()
    ndbout << "--------------------------------------------------" << endl;
    UtilSequenceReq * req = (UtilSequenceReq*)signal->getDataPtrSend();
    Uint32 seqId = 1;
    Uint32 reqTy = UtilSequenceReq::CurrVal;

    if(signal->length() > 1) seqId = signal->theData[1];
    if(signal->length() > 2) reqTy = signal->theData[2];
    
    req->senderData = 12;
    req->sequenceId = seqId;
    req->requestType = reqTy;

    sendSignal(DBUTIL_REF, GSN_UTIL_SEQUENCE_REQ, 
	       signal, UtilSequenceReq::SignalLength, JBB);
  }

  /****************************************************************************/
  /* // Obsolete tests, should be rewritten for long signals!!
  if(tCase == 210){
    jam();
    ndbout << "--------------------------------------------------" << endl;
    const Uint32 pageSizeInWords = 128;
    Uint32 propPage[pageSizeInWords];
    LinearWriter w(&propPage[0], 128);
    w.first();
    w.add(UtilPrepareReq::NoOfOperations, 1);
    w.add(UtilPrepareReq::OperationType, UtilPrepareReq::Delete);
    w.add(UtilPrepareReq::TableName,      "sys/def/SYSTAB_0");
    w.add(UtilPrepareReq::AttributeName,  "SYSKEY_0"); // AttrNo = 0
    Uint32 length = w.getWordsUsed();
    ndbassert(length <= pageSizeInWords);

    sendUtilPrepareReqSignals(signal, propPage, length);
  }
  if(tCase == 211){
    jam();
    ndbout << "--------------------------------------------------" << endl;
    const Uint32 pageSizeInWords = 128;
    Uint32 propPage[pageSizeInWords];
    LinearWriter w(&propPage[0],128);
    w.first();
    w.add(UtilPrepareReq::NoOfOperations, 1);
    w.add(UtilPrepareReq::OperationType,  UtilPrepareReq::Insert);
    w.add(UtilPrepareReq::TableName,      "sys/def/SYSTAB_0");
    w.add(UtilPrepareReq::AttributeName,  "SYSKEY_0");  // AttrNo = 0
    w.add(UtilPrepareReq::AttributeName,  "NEXTID");    // AttrNo = 1
    Uint32 length = w.getWordsUsed();
    ndbassert(length <= pageSizeInWords);

    sendUtilPrepareReqSignals(signal, propPage, length);
  }
  if(tCase == 212){
    jam();
    ndbout << "--------------------------------------------------" << endl;
    const Uint32 pageSizeInWords = 128;
    Uint32 propPage[pageSizeInWords];
    LinearWriter w(&propPage[0],128);
    w.first();
    w.add(UtilPrepareReq::NoOfOperations, 1);
    w.add(UtilPrepareReq::OperationType,  UtilPrepareReq::Update);
    w.add(UtilPrepareReq::TableName,      "sys/def/SYSTAB_0");
    w.add(UtilPrepareReq::AttributeName,  "SYSKEY_0");  // AttrNo = 0
    w.add(UtilPrepareReq::AttributeName,  "NEXTID");    // AttrNo = 1
    Uint32 length = w.getWordsUsed();
    ndbassert(length <= pageSizeInWords);

    sendUtilPrepareReqSignals(signal, propPage, length);
  }
  if(tCase == 213){
    jam();
    ndbout << "--------------------------------------------------" << endl;
    const Uint32 pageSizeInWords = 128;
    Uint32 propPage[pageSizeInWords];
    LinearWriter w(&propPage[0],128);
    w.first();
    w.add(UtilPrepareReq::NoOfOperations, 1);
    w.add(UtilPrepareReq::OperationType, UtilPrepareReq::Read);
    w.add(UtilPrepareReq::TableName,      "sys/def/SYSTAB_0");
    w.add(UtilPrepareReq::AttributeName,  "SYSKEY_0"); // AttrNo = 0
    Uint32 length = w.getWordsUsed();
    ndbassert(length <= pageSizeInWords);

    sendUtilPrepareReqSignals(signal, propPage, length);
  }
  if(tCase == 214){
    jam();
    ndbout << "--------------------------------------------------" << endl;
    const Uint32 pageSizeInWords = 128;
    Uint32 propPage[pageSizeInWords];
    LinearWriter w(&propPage[0], 128);
    w.first();
    w.add(UtilPrepareReq::NoOfOperations, 1);
    w.add(UtilPrepareReq::OperationType, UtilPrepareReq::Delete);
    w.add(UtilPrepareReq::TableId, (unsigned int)0);	// SYSTAB_0
    w.add(UtilPrepareReq::AttributeId, (unsigned int)0);// SYSKEY_0
    Uint32 length = w.getWordsUsed();
    ndbassert(length <= pageSizeInWords);

    sendUtilPrepareReqSignals(signal, propPage, length);
  }
  if(tCase == 215){
    jam();
    ndbout << "--------------------------------------------------" << endl;
    const Uint32 pageSizeInWords = 128;
    Uint32 propPage[pageSizeInWords];
    LinearWriter w(&propPage[0],128);
    w.first();
    w.add(UtilPrepareReq::NoOfOperations, 1);
    w.add(UtilPrepareReq::OperationType,  UtilPrepareReq::Insert);
    w.add(UtilPrepareReq::TableId, (unsigned int)0);	 // SYSTAB_0
    w.add(UtilPrepareReq::AttributeId, (unsigned int)0); // SYSKEY_0
    w.add(UtilPrepareReq::AttributeId, 1);		 // NEXTID
    Uint32 length = w.getWordsUsed();
    ndbassert(length <= pageSizeInWords);

    sendUtilPrepareReqSignals(signal, propPage, length);
  }
  if(tCase == 216){
    jam();
    ndbout << "--------------------------------------------------" << endl;
    const Uint32 pageSizeInWords = 128;
    Uint32 propPage[pageSizeInWords];
    LinearWriter w(&propPage[0],128);
    w.first();
    w.add(UtilPrepareReq::NoOfOperations, 1);
    w.add(UtilPrepareReq::OperationType,  UtilPrepareReq::Update);
    w.add(UtilPrepareReq::TableId, (unsigned int)0);	// SYSTAB_0
    w.add(UtilPrepareReq::AttributeId, (unsigned int)0);// SYSKEY_0
    w.add(UtilPrepareReq::AttributeId, 1);		// NEXTID
    Uint32 length = w.getWordsUsed();
    ndbassert(length <= pageSizeInWords);

    sendUtilPrepareReqSignals(signal, propPage, length);
  }
  if(tCase == 217){
    jam();
    ndbout << "--------------------------------------------------" << endl;
    const Uint32 pageSizeInWords = 128;
    Uint32 propPage[pageSizeInWords];
    LinearWriter w(&propPage[0],128);
    w.first();
    w.add(UtilPrepareReq::NoOfOperations, 1);
    w.add(UtilPrepareReq::OperationType, UtilPrepareReq::Read);
    w.add(UtilPrepareReq::TableId, (unsigned int)0);	// SYSTAB_0
    w.add(UtilPrepareReq::AttributeId, (unsigned int)0);// SYSKEY_0
    Uint32 length = w.getWordsUsed();
    ndbassert(length <= pageSizeInWords);

    sendUtilPrepareReqSignals(signal, propPage, length);
  }
  */
  /****************************************************************************/
  /* // Obsolete tests, should be rewritten for long signals!!
  if(tCase == 220){
    jam();
    ndbout << "--------------------------------------------------" << endl;    
    Uint32 prepI = signal->theData[1];
    Uint32 length = signal->theData[2];
    Uint32 attributeValue0 = signal->theData[3];
    Uint32 attributeValue1a = signal->theData[4];
    Uint32 attributeValue1b = signal->theData[5];
    ndbrequire(prepI != 0);

    UtilExecuteReq * req = (UtilExecuteReq *)signal->getDataPtrSend();

    req->senderData   = 221;
    req->prepareId    = prepI;
    req->totalDataLen = length;  // Including headers
    req->offset       = 0;

    AttributeHeader::init(&req->attrData[0], 0, 1);  // AttrNo 0, DataSize
    req->attrData[1] = attributeValue0;              // AttrValue 
    AttributeHeader::init(&req->attrData[2], 1, 2);  // AttrNo 1, DataSize
    req->attrData[3] = attributeValue1a;             // AttrValue 
    req->attrData[4] = attributeValue1b;             // AttrValue 

    printUTIL_EXECUTE_REQ(stdout, signal->getDataPtrSend(), 3 + 5,0);
    sendSignal(DBUTIL_REF, GSN_UTIL_EXECUTE_REQ, signal, 3 + 5, JBB);
  }
*/
  /****************************************************************************
   *  230 : PRINT STATE
   */
#ifdef ARRAY_GUARD
  if(tCase == 230){
    jam();

    ndbout << "--------------------------------------------------" << endl;
    if (signal->length() <= 1) {
      ndbout << "Usage: DUMP 230 <recordType> <recordNo>" << endl 
	     << "[1] Print Prepare (running) records" << endl
	     << "[2] Print PreparedOperation records" << endl
	     << "[3] Print Transaction records" << endl
	     << "[4] Print Operation records" << endl
	     << "Ex. \"dump 230 1 2\" prints Prepare record no 2." << endl
	     << endl
	     << "210 : PREPARE_REQ DELETE SYSTAB_0 SYSKEY_0" << endl
	     << "211 : PREPARE_REQ INSERT SYSTAB_0 SYSKEY_0 NEXTID" << endl
	     << "212 : PREPARE_REQ UPDATE SYSTAB_0 SYSKEY_0 NEXTID" << endl
	     << "213 : PREPARE_REQ READ   SYSTAB_0 SYSKEY_0" << endl
	     << "214 : PREPARE_REQ DELETE SYSTAB_0 SYSKEY_0 using id" << endl
	     << "215 : PREPARE_REQ INSERT SYSTAB_0 SYSKEY_0 NEXTID using id" << endl
	     << "216 : PREPARE_REQ UPDATE SYSTAB_0 SYSKEY_0 NEXTID using id" << endl
	     << "217 : PREPARE_REQ READ   SYSTAB_0 SYSKEY_0 using id" << endl
	     << "220 : EXECUTE_REQ <PrepId> <Len> <Val1> <Val2a> <Val2b>" <<endl
	     << "299 : Crash system (using ndbrequire(0))" 
	     << endl
	     << "Ex. \"dump 220 3 5 1 0 17 \" prints Prepare record no 2." 
	     << endl;
      return;
    }

    switch (signal->theData[1]) {
    case 1:
      // ** Print a specific record **
      if (signal->length() >= 3) {
	PreparePtr prepPtr;
	if (!c_preparePool.isSeized(signal->theData[2])) {
	  ndbout << "Prepare Id: " << signal->theData[2] 
		 << " (Not seized!)" << endl;
	} else {
	  c_preparePool.getPtr(prepPtr, signal->theData[2]);
	  prepPtr.p->print();
	}
	return;
      }	

      // ** Print all records **
      PreparePtr prepPtr;
      if (!c_runningPrepares.first(prepPtr)) {
	ndbout << "No Prepare records exist" << endl;
	return;
      }
      
      while (!prepPtr.isNull()) {
	prepPtr.p->print();
	c_runningPrepares.next(prepPtr);
      }
      return;

    case 2:
      // ** Print a specific record **
      if (signal->length() >= 3) {
	if (!c_preparedOperationPool.isSeized(signal->theData[2])) {
	  ndbout << "PreparedOperation Id: " << signal->theData[2] 
		 << " (Not seized!)" << endl;
	  return;
	}
	ndbout << "PreparedOperation Id: " << signal->theData[2] << endl;
	PreparedOperationPtr prepOpPtr;
	c_runningPreparedOperations.getPtr(prepOpPtr, signal->theData[2]);
	prepOpPtr.p->print();
	return;
      }

      // ** Print all records **
      PreparedOperationPtr prepOpPtr;
      if (!c_runningPreparedOperations.first(prepOpPtr)) {
	ndbout << "No PreparedOperations exist" << endl;
	return;
      }
      while (!prepOpPtr.isNull()) {
	ndbout << "[-PreparedOperation no " << prepOpPtr.i << ":"; 
	prepOpPtr.p->print();
	ndbout << "]";
	c_runningPreparedOperations.next(prepOpPtr);
      }
      return;

    case 3:
      // ** Print a specific record **
      if (signal->length() >= 3) {
	ndbout << "Print specific record not implemented." << endl;
	return;
      }

      // ** Print all records **
      ndbout << "Print all records not implemented, specify an Id." << endl;
      return;

    case 4:
      ndbout << "Not implemented" << endl;
      return;

    default:
      ndbout << "Unknown input (try without any data)" << endl;
      return;
    }      
  }
#endif
  if(tCase == 240 && signal->getLength() == 2){
    MutexManager::ActiveMutexPtr ptr;
    ndbrequire(c_mutexMgr.seize(ptr));
    ptr.p->m_mutexId = signal->theData[1];
    Callback c = { safe_cast(&DbUtil::mutex_created), ptr.i };
    ptr.p->m_callback = c;
    c_mutexMgr.create(signal, ptr);
    ndbout_c("c_mutexMgr.create ptrI=%d mutexId=%d", ptr.i, ptr.p->m_mutexId);
  }

  if(tCase == 241 && signal->getLength() == 2){
    MutexManager::ActiveMutexPtr ptr;
    ndbrequire(c_mutexMgr.seize(ptr));
    ptr.p->m_mutexId = signal->theData[1];
    Callback c = { safe_cast(&DbUtil::mutex_locked), ptr.i };
    ptr.p->m_callback = c;
    c_mutexMgr.lock(signal, ptr);
    ndbout_c("c_mutexMgr.lock ptrI=%d mutexId=%d", ptr.i, ptr.p->m_mutexId);
  }

  if(tCase == 242 && signal->getLength() == 2){
    MutexManager::ActiveMutexPtr ptr;
    ptr.i = signal->theData[1];
    c_mutexMgr.getPtr(ptr);
    Callback c = { safe_cast(&DbUtil::mutex_unlocked), ptr.i };
    ptr.p->m_callback = c;
    c_mutexMgr.unlock(signal, ptr);
    ndbout_c("c_mutexMgr.unlock ptrI=%d mutexId=%d", ptr.i, ptr.p->m_mutexId);
  }
  
  if(tCase == 243 && signal->getLength() == 3){
    MutexManager::ActiveMutexPtr ptr;
    ndbrequire(c_mutexMgr.seize(ptr));
    ptr.p->m_mutexId = signal->theData[1];
    ptr.p->m_mutexKey = signal->theData[2];
    Callback c = { safe_cast(&DbUtil::mutex_destroyed), ptr.i };
    ptr.p->m_callback = c;
    c_mutexMgr.destroy(signal, ptr);
    ndbout_c("c_mutexMgr.destroy ptrI=%d mutexId=%d key=%d", 
	     ptr.i, ptr.p->m_mutexId, ptr.p->m_mutexKey);
  }
}

void
DbUtil::mutex_created(Signal* signal, Uint32 ptrI, Uint32 retVal){
  MutexManager::ActiveMutexPtr ptr; ptr.i = ptrI;
  c_mutexMgr.getPtr(ptr);
  ndbout_c("mutex_created - mutexId=%d, retVal=%d", 
	   ptr.p->m_mutexId, retVal);
  c_mutexMgr.release(ptrI);
}

void
DbUtil::mutex_destroyed(Signal* signal, Uint32 ptrI, Uint32 retVal){
  MutexManager::ActiveMutexPtr ptr; ptr.i = ptrI;
  c_mutexMgr.getPtr(ptr);
  ndbout_c("mutex_destroyed - mutexId=%d, retVal=%d", 
	   ptr.p->m_mutexId, retVal); 
  c_mutexMgr.release(ptrI);
}

void
DbUtil::mutex_locked(Signal* signal, Uint32 ptrI, Uint32 retVal){
  MutexManager::ActiveMutexPtr ptr; ptr.i = ptrI;
  c_mutexMgr.getPtr(ptr);
  ndbout_c("mutex_locked - mutexId=%d, retVal=%d key=%d ptrI=%d", 
	   ptr.p->m_mutexId, retVal, ptr.p->m_mutexKey, ptrI);
  if(retVal)
    c_mutexMgr.release(ptrI);
}

void
DbUtil::mutex_unlocked(Signal* signal, Uint32 ptrI, Uint32 retVal){
  MutexManager::ActiveMutexPtr ptr; ptr.i = ptrI;
  c_mutexMgr.getPtr(ptr);
  ndbout_c("mutex_unlocked - mutexId=%d, retVal=%d", 
	   ptr.p->m_mutexId, retVal); 
  if(!retVal)
    c_mutexMgr.release(ptrI);
}
   
void
DbUtil::execUTIL_SEQUENCE_REF(Signal* signal){
  jamEntry();
  ndbout << "UTIL_SEQUENCE_REF" << endl;
  printUTIL_SEQUENCE_REF(stdout, signal->getDataPtrSend(), signal->length(), 0);
}

void
DbUtil::execUTIL_SEQUENCE_CONF(Signal* signal){
  jamEntry();
  ndbout << "UTIL_SEQUENCE_CONF" << endl;
  printUTIL_SEQUENCE_CONF(stdout, signal->getDataPtrSend(), signal->length(),0);
}

void
DbUtil::execUTIL_PREPARE_CONF(Signal* signal){
  jamEntry();
  ndbout << "UTIL_PREPARE_CONF" << endl;
  printUTIL_PREPARE_CONF(stdout, signal->getDataPtrSend(), signal->length(), 0);
}

void
DbUtil::execUTIL_PREPARE_REF(Signal* signal){
  jamEntry();
  ndbout << "UTIL_PREPARE_REF" << endl;
  printUTIL_PREPARE_REF(stdout, signal->getDataPtrSend(), signal->length(), 0);
}

void 
DbUtil::execUTIL_EXECUTE_CONF(Signal* signal) {
  jamEntry();
  ndbout << "UTIL_EXECUTE_CONF" << endl;
  printUTIL_EXECUTE_CONF(stdout, signal->getDataPtrSend(), signal->length(), 0);
}

void 
DbUtil::execUTIL_EXECUTE_REF(Signal* signal) {
  jamEntry();

  ndbout << "UTIL_EXECUTE_REF" << endl;
  printUTIL_EXECUTE_REF(stdout, signal->getDataPtrSend(), signal->length(), 0);
}

void 
DbUtil::execUTIL_RELEASE_CONF(Signal* signal) {
  jamEntry();
  ndbout << "UTIL_RELEASE_CONF" << endl;
}

void 
DbUtil::execUTIL_RELEASE_REF(Signal* signal) {
  jamEntry();

  ndbout << "UTIL_RELEASE_REF" << endl;
}

void
DbUtil::sendUtilPrepareRef(Signal* signal, UtilPrepareRef::ErrorCode error, 
			   Uint32 recipient, Uint32 senderData){
  UtilPrepareRef * ref = (UtilPrepareRef *)signal->getDataPtrSend();
  ref->errorCode = error;
  ref->senderData = senderData;

  sendSignal(recipient, GSN_UTIL_PREPARE_REF, signal, 
	     UtilPrepareRef::SignalLength, JBB);
}

void
DbUtil::sendUtilExecuteRef(Signal* signal, UtilExecuteRef::ErrorCode error, 
			   Uint32 TCerror, Uint32 recipient, Uint32 senderData){
  
  UtilExecuteRef * ref = (UtilExecuteRef *)signal->getDataPtrSend();
  ref->senderData  = senderData;
  ref->errorCode   = error;
  ref->TCErrorCode = TCerror;

  sendSignal(recipient, GSN_UTIL_EXECUTE_REF, signal, 
	     UtilPrepareRef::SignalLength, JBB);
}


/**************************************************************************
 * ------------------------------------------------------------------------
 *  MODULE:       Prepare service
 * ------------------------------------------------------------------------
 *
 *  Prepares a transaction by storing info in some structs
 **************************************************************************/

void 
DbUtil::execUTIL_PREPARE_REQ(Signal* signal)
{
  jamEntry();  
  
  /****************
   * Decode Signal
   ****************/
  UtilPrepareReq * req = (UtilPrepareReq *)signal->getDataPtr();
  const Uint32 senderRef    = req->senderRef;
  const Uint32 senderData   = req->senderData;

  if(signal->getNoOfSections() == 0) {
    // Missing prepare data
    jam();
    releaseSections(signal);
    sendUtilPrepareRef(signal, UtilPrepareRef::MISSING_PROPERTIES_SECTION,
		       senderRef, senderData);
    return;
  }

  PreparePtr prepPtr;
  SegmentedSectionPtr ptr;
  
  jam();
  if(!c_runningPrepares.seize(prepPtr)) {
    jam();
    releaseSections(signal);
    sendUtilPrepareRef(signal, UtilPrepareRef::PREPARE_SEIZE_ERROR,
		       senderRef, senderData);
    return;
  };
  signal->getSection(ptr, UtilPrepareReq::PROPERTIES_SECTION);
  const Uint32 noPages  = (ptr.sz + sizeof(Page32)) / sizeof(Page32);
  ndbassert(noPages > 0);
  if (!prepPtr.p->preparePages.seize(noPages)) {
    jam();
    releaseSections(signal);
    sendUtilPrepareRef(signal, UtilPrepareRef::PREPARE_PAGES_SEIZE_ERROR,
		       senderRef, senderData);
    c_preparePool.release(prepPtr);
    return;
  }
  // Save SimpleProperties
  Uint32* target = &prepPtr.p->preparePages.getPtr(0)->data[0];
  copy(target, ptr);
  prepPtr.p->prepDataLen = ptr.sz;
  // Release long signal sections
  releaseSections(signal);
  // Check table properties with DICT
  SimplePropertiesSectionReader reader(ptr, getSectionSegmentPool());
  prepPtr.p->clientRef = senderRef;
  prepPtr.p->clientData = senderData;
  // Release long signal sections
  releaseSections(signal);
  readPrepareProps(signal, &reader, prepPtr.i);
}

void DbUtil::readPrepareProps(Signal* signal,
			      SimpleProperties::Reader* reader, 
			      Uint32 senderData)
{
  jam();
#if 0
  printf("DbUtil::readPrepareProps: Received SimpleProperties:\n");
  reader->printAll(ndbout);
#endif
  ndbrequire(reader->first());
  ndbrequire(reader->getKey() == UtilPrepareReq::NoOfOperations);
  ndbrequire(reader->getUint32() == 1);      // Only one op/trans implemented
  
  ndbrequire(reader->next());
  ndbrequire(reader->getKey() == UtilPrepareReq::OperationType);
  
  ndbrequire(reader->next());
  UtilPrepareReq::KeyValue tableKey = 
    (UtilPrepareReq::KeyValue) reader->getKey();
  ndbrequire((tableKey == UtilPrepareReq::TableName) ||
	     (tableKey == UtilPrepareReq::TableId));

  /************************
   * Ask Dict for metadata
   ************************/
  {
    GetTabInfoReq * req = (GetTabInfoReq *)signal->getDataPtrSend();
    req->senderRef = reference();
    req->senderData = senderData;           
    if (tableKey == UtilPrepareReq::TableName) {
      jam();
      char tableName[MAX_TAB_NAME_SIZE];
      req->requestType = GetTabInfoReq::RequestByName |
	GetTabInfoReq::LongSignalConf;

      req->tableNameLen = reader->getValueLen();    // Including trailing \0
      
      /********************************************
       * Code signal data and send signals to DICT
       ********************************************/

      ndbrequire(req->tableNameLen < MAX_TAB_NAME_SIZE); 
      reader->getString((char*)tableName);
      LinearSectionPtr ptr[1];
      ptr[0].p = (Uint32*)tableName; 
      ptr[0].sz = req->tableNameLen; 
      sendSignal(DBDICT_REF, GSN_GET_TABINFOREQ, signal, 
		 GetTabInfoReq::SignalLength, JBB, ptr,1);

    }
    else { // (tableKey == UtilPrepareReq::TableId)
      jam();
      req->requestType = GetTabInfoReq::RequestById |
	GetTabInfoReq::LongSignalConf;
      req->tableId = reader->getUint32();
      sendSignal(DBDICT_REF, GSN_GET_TABINFOREQ, signal, 
		 GetTabInfoReq::SignalLength, JBB);
    }

  }
}

/**
 *  @note  We assume that this signal comes due to a request related
 *         to a Prepare struct.  DictTabInfo:s 'senderData' denotes
 *         the Prepare struct related to the request.
 */
void
DbUtil::execGET_TABINFO_CONF(Signal* signal){
  jamEntry();

  if(!assembleFragments(signal)){
    jam();
    return;
  }

  /****************
   * Decode signal 
   ****************/
  GetTabInfoConf * const conf = (GetTabInfoConf*)signal->getDataPtr();
  const Uint32  prepI    = conf->senderData;
  const Uint32  totalLen = conf->totalLen;
  
  SegmentedSectionPtr dictTabInfoPtr;
  signal->getSection(dictTabInfoPtr, GetTabInfoConf::DICT_TAB_INFO);
  ndbrequire(dictTabInfoPtr.sz == totalLen);
  
  PreparePtr prepPtr;
  c_runningPrepares.getPtr(prepPtr, prepI);
  prepareOperation(signal, prepPtr);
}

void
DbUtil::execGET_TABINFOREF(Signal* signal){
  jamEntry();

  GetTabInfoRef * ref = (GetTabInfoRef *)signal->getDataPtr();
  Uint32 prepI = ref->senderData;
#define EVENT_DEBUG
#if 0 //def EVENT_DEBUG
  ndbout << "Signal GET_TABINFOREF received." << endl;
  ndbout << "Error Code: " << ref->errorCode << endl;

  switch (ref->errorCode) {
  case GetTabInfoRef::InvalidTableId:
    ndbout << "      Msg:  Invalid table id" << endl;
    break;
  case GetTabInfoRef::TableNotDefined:
    ndbout << "      Msg:  Table not defined" << endl;
    break;
  case GetTabInfoRef::TableNameToLong:
    ndbout << "      Msg:  Table node too long" << endl;
    break;
  default:
    ndbout << "      Msg:  Unknown error returned from Dict" << endl;
    break;
  }  
#endif

  PreparePtr prepPtr;
  c_runningPrepares.getPtr(prepPtr, prepI);

  sendUtilPrepareRef(signal, UtilPrepareRef::DICT_TAB_INFO_ERROR,
		     prepPtr.p->clientRef, prepPtr.p->clientData);

  releasePrepare(prepPtr);
}


/******************************************************************************
 * Prepare Operation
 * 
 * Using a prepare record, prepare an operation (i.e. create PreparedOperation).
 * Info from both Pepare request (PreparePages) and DictTabInfo is used.
 * 
 * Algorithm:
 * -# Seize AttrbuteMapping
 *    - Lookup in preparePages how many attributes should be prepared
 *    - Seize AttributeMapping
 * -# For each attributes in preparePages 
 *    - Lookup id and isPK in dictInfoPages
 *    - Store "no -> (AttributeId, Position)" in AttributeMapping
 * -# For each map in AttributeMapping
 *    - if (isPK) then assign offset
 ******************************************************************************/
void
DbUtil::prepareOperation(Signal* signal, PreparePtr prepPtr) 
{
  jam();
  
  /*******************************************
   * Seize and store PreparedOperation struct
   *******************************************/
  PreparedOperationPtr prepOpPtr;  
  if(!c_runningPreparedOperations.seize(prepOpPtr)) {
    jam();
    releaseSections(signal);
    sendUtilPrepareRef(signal, UtilPrepareRef::PREPARED_OPERATION_SEIZE_ERROR,
		       prepPtr.p->clientRef, prepPtr.p->clientData);
    releasePrepare(prepPtr);
    return;
  }
  prepPtr.p->prepOpPtr = prepOpPtr;

  /********************
   * Read request info
   ********************/
  SimplePropertiesLinearReader prepPagesReader(&prepPtr.p->preparePages.getPtr(0)->data[0], 
					       prepPtr.p->prepDataLen);
  
  ndbrequire(prepPagesReader.first());
  ndbrequire(prepPagesReader.getKey() == UtilPrepareReq::NoOfOperations);
  const Uint32 noOfOperations = prepPagesReader.getUint32();
  ndbrequire(noOfOperations == 1);

  ndbrequire(prepPagesReader.next());
  ndbrequire(prepPagesReader.getKey() == UtilPrepareReq::OperationType);
  const Uint32 operationType = prepPagesReader.getUint32();
  
  ndbrequire(prepPagesReader.next());
  
  char tableName[MAX_TAB_NAME_SIZE];
  Uint32 tableId;
  UtilPrepareReq::KeyValue tableKey = 
    (UtilPrepareReq::KeyValue) prepPagesReader.getKey();
  if (tableKey == UtilPrepareReq::TableId) {
    jam();
    tableId = prepPagesReader.getUint32();
  }
  else {
    jam();
    ndbrequire(prepPagesReader.getKey() == UtilPrepareReq::TableName);
    ndbrequire(prepPagesReader.getValueLen() <= MAX_TAB_NAME_SIZE);
    prepPagesReader.getString(tableName);
  }
  /******************************************************************
   * Seize AttributeMapping (by counting no of attribs in prepPages)
   ******************************************************************/
  Uint32 noOfAttributes = 0;   // No of attributes in PreparePages (used later)
  while(prepPagesReader.next()) {
    if (tableKey == UtilPrepareReq::TableName) {
      jam();
      ndbrequire(prepPagesReader.getKey() == UtilPrepareReq::AttributeName);
    } else {
      jam();
      ndbrequire(prepPagesReader.getKey() == UtilPrepareReq::AttributeId);
    }
    noOfAttributes++;
  }
  ndbrequire(prepPtr.p->prepOpPtr.p->attrMapping.seize(noOfAttributes));
  if (operationType == UtilPrepareReq::Read) {
    ndbrequire(prepPtr.p->prepOpPtr.p->rsInfo.seize(noOfAttributes));
  }
  /***************************************
   * For each attribute name, lookup info 
   ***************************************/
  // Goto start of attribute names 
  ndbrequire(prepPagesReader.first() && prepPagesReader.next() && 
	     prepPagesReader.next());
  
  DictTabInfo::Table tableDesc; tableDesc.init();
  AttrMappingBuffer::DataBufferIterator attrMappingIt;
  ndbrequire(prepPtr.p->prepOpPtr.p->attrMapping.first(attrMappingIt));

  ResultSetBuffer::DataBufferIterator rsInfoIt;
  if (operationType == UtilPrepareReq::Read) {
    ndbrequire(prepPtr.p->prepOpPtr.p->rsInfo.first(rsInfoIt));
  }

  Uint32 noOfPKAttribsStored = 0;
  Uint32 noOfNonPKAttribsStored = 0;
  Uint32 attrLength = 0;
  Uint32 pkAttrLength = 0;
  char attrNameRequested[MAX_ATTR_NAME_SIZE];
  Uint32 attrIdRequested;

  while(prepPagesReader.next()) {
    UtilPrepareReq::KeyValue attributeKey = 
      (UtilPrepareReq::KeyValue) prepPagesReader.getKey();    
    
    ndbrequire((attributeKey == UtilPrepareReq::AttributeName) ||
	       (attributeKey == UtilPrepareReq::AttributeId));
    if (attributeKey == UtilPrepareReq::AttributeName) {
      jam();
      ndbrequire(prepPagesReader.getValueLen() <= MAX_ATTR_NAME_SIZE);
      
      prepPagesReader.getString(attrNameRequested);
    } else {
      jam();
      attrIdRequested = prepPagesReader.getUint32();
    }
    /*****************************************
     * Copy DictTabInfo into tableDesc struct
     *****************************************/
      
    SegmentedSectionPtr ptr;
    signal->getSection(ptr, GetTabInfoConf::DICT_TAB_INFO);
    SimplePropertiesSectionReader dictInfoReader(ptr, getSectionSegmentPool());

    SimpleProperties::UnpackStatus unpackStatus;
    unpackStatus = SimpleProperties::unpack(dictInfoReader, &tableDesc, 
					    DictTabInfo::TableMapping, 
					    DictTabInfo::TableMappingSize, 
					    true, true);
    ndbrequire(unpackStatus == SimpleProperties::Break);
    
    /************************
     * Lookup in DictTabInfo
     ************************/
    DictTabInfo::Attribute attrDesc; attrDesc.init();
    char attrName[MAX_ATTR_NAME_SIZE];
    Uint32 attrId;
    bool attributeFound = false;
    Uint32 noOfKeysFound = 0;     // # PK attrs found before attr in DICTdata
    Uint32 noOfNonKeysFound = 0;  // # nonPK attrs found before attr in DICTdata
    for (Uint32 i=0; i<tableDesc.NoOfAttributes; i++) {
      if (tableKey == UtilPrepareReq::TableName) {
	jam();
	ndbrequire(dictInfoReader.getKey() == DictTabInfo::AttributeName);
	ndbrequire(dictInfoReader.getValueLen() <= MAX_ATTR_NAME_SIZE);
	dictInfoReader.getString(attrName);
      } else { // (tableKey == UtilPrepareReq::TableId)
	jam();
	dictInfoReader.next(); // Skip name
	ndbrequire(dictInfoReader.getKey() == DictTabInfo::AttributeId);
	attrId = dictInfoReader.getUint32();
      }
      unpackStatus = SimpleProperties::unpack(dictInfoReader, &attrDesc, 
					      DictTabInfo::AttributeMapping, 
					      DictTabInfo::AttributeMappingSize,
					      true, true);
      ndbrequire(unpackStatus == SimpleProperties::Break);
      //attrDesc.print(stdout);
      
      if (attrDesc.AttributeKeyFlag) { jam(); noOfKeysFound++; } 
      else                           { jam(); noOfNonKeysFound++; }
      if (attributeKey == UtilPrepareReq::AttributeName) {
	if (strcmp(attrName, attrNameRequested) == 0) {
	  attributeFound = true;
	  break;  
	}
      }
      else // (attributeKey == UtilPrepareReq::AttributeId)
	if (attrId == attrIdRequested) {
	  attributeFound = true;
	  break;  
	}
      
      // Move to next attribute
      ndbassert(dictInfoReader.getKey() == DictTabInfo::AttributeEnd);
      dictInfoReader.next();
    }
    
    /**********************
     * Attribute not found
     **********************/
    if (!attributeFound) {
      jam(); 
      releaseSections(signal);
      sendUtilPrepareRef(signal, 
			 UtilPrepareRef::DICT_TAB_INFO_ERROR,
			 prepPtr.p->clientRef, prepPtr.p->clientData);
      infoEvent("UTIL: Unknown attribute requested: %s in table: %s",
		attrNameRequested, tableName);
      releasePreparedOperation(prepOpPtr);
      releasePrepare(prepPtr);
      return;
    }
    
    /**************************************************************
     * Attribute found - store in mapping  (AttributeId, Position)
     **************************************************************/
    AttributeHeader & attrMap = 
      AttributeHeader::init(attrMappingIt.data, 
			    attrDesc.AttributeId,    // 1. Store AttrId
			    0);
    
    if (attrDesc.AttributeKeyFlag) {
      // ** Attribute belongs to PK **
      prepOpPtr.p->pkBitmask.set(attrDesc.AttributeId);
      attrMap.setDataSize(noOfKeysFound - 1);        // 2. Store Position
      noOfPKAttribsStored++;
    } else {
      attrMap.setDataSize(0x3fff);                   // 2. Store Position (fake)
      noOfNonPKAttribsStored++;

      /***********************************************************
       * Error: Read nonPK Attr before all PK attr have been read
       ***********************************************************/
      if (noOfPKAttribsStored != tableDesc.NoOfKeyAttr) {
	jam(); 
	releaseSections(signal);
	sendUtilPrepareRef(signal, 
			   UtilPrepareRef::DICT_TAB_INFO_ERROR,
			   prepPtr.p->clientRef, prepPtr.p->clientData);
	infoEvent("UTIL: Non-PK attr not allowed before "
		  "all PK attrs have been defined, table: %s",
		  tableName);
	releasePreparedOperation(prepOpPtr);
	releasePrepare(prepPtr);
	return;
      }
    }
#if 0
    ndbout << "BEFORE: attrLength: " << attrLength << endl;
#endif
    {
      int len = 0;
      switch (attrDesc.AttributeSize) {
      case DictTabInfo::an8Bit:
	len = (attrDesc.AttributeArraySize + 3)/ 4;
	break;
      case DictTabInfo::a16Bit:
	len = (attrDesc.AttributeArraySize + 1) / 2;
	break;
      case DictTabInfo::a32Bit:
	len = attrDesc.AttributeArraySize;
	break;
      case DictTabInfo::a64Bit:
	len = attrDesc.AttributeArraySize * 2;
      break;
      case DictTabInfo::a128Bit:
	len = attrDesc.AttributeArraySize * 4;
	break;
      }
      attrLength += len;
      if (attrDesc.AttributeKeyFlag)
	pkAttrLength += len;

      if (operationType == UtilPrepareReq::Read) {
	AttributeHeader::init(rsInfoIt.data, 
			      attrDesc.AttributeId,    // 1. Store AttrId
			      len);
	prepOpPtr.p->rsInfo.next(rsInfoIt, 1);
      }
    }
#if 0
    ndbout << ": AttributeSize: " << attrDesc.AttributeSize << endl;
    ndbout << ": AttributeArraySize: " << attrDesc.AttributeArraySize << endl;
    ndbout << "AFTER: attrLength: " << attrLength << endl;
#endif    
    //attrMappingIt.print(stdout);
    //prepPtr.p->prepOpPtr.p->attrMapping.print(stdout);
    prepPtr.p->prepOpPtr.p->attrMapping.next(attrMappingIt, 1);
  }   

  /***************************
   * Error: Not all PKs found 
   ***************************/
  if (noOfPKAttribsStored != tableDesc.NoOfKeyAttr) {
    jam(); 
    releaseSections(signal);
    sendUtilPrepareRef(signal, 
		       UtilPrepareRef::DICT_TAB_INFO_ERROR,
		       prepPtr.p->clientRef, prepPtr.p->clientData);
    infoEvent("UTIL: Not all primary key attributes requested for table: %s",
	      tableName);
    releasePreparedOperation(prepOpPtr);
    releasePrepare(prepPtr);
    return;
  }

#if 0
  AttrMappingBuffer::ConstDataBufferIterator tmpIt;
  for (prepPtr.p->prepOpPtr.p->attrMapping.first(tmpIt); tmpIt.curr.i != RNIL;
       prepPtr.p->prepOpPtr.p->attrMapping.next(tmpIt)) {
    AttributeHeader* ah = (AttributeHeader *) tmpIt.data;
    ah->print(stdout);
  }
#endif
  
  /**********************************************
   * Preparing of PreparedOperation signal train 
   **********************************************/
  Uint32 static_len = TcKeyReq::StaticLength;
  prepOpPtr.p->tckey.tableId = tableDesc.TableId;
  prepOpPtr.p->tckey.tableSchemaVersion = tableDesc.TableVersion;
  prepOpPtr.p->noOfKeyAttr = tableDesc.NoOfKeyAttr;
  prepOpPtr.p->keyLen = tableDesc.KeyLength; // Total no of words in PK
  if (prepOpPtr.p->keyLen > TcKeyReq::MaxKeyInfo) {
    jam();
    prepOpPtr.p->tckeyLenInBytes = (static_len + TcKeyReq::MaxKeyInfo) * 4;
  } else {
    jam();
    prepOpPtr.p->tckeyLenInBytes = (static_len + prepOpPtr.p->keyLen) * 4;
  }
  prepOpPtr.p->keyDataPos = static_len;  // Start of keyInfo[] in tckeyreq
  
  Uint32 requestInfo = 0;
  TcKeyReq::setAbortOption(requestInfo, TcKeyReq::AbortOnError);
  TcKeyReq::setKeyLength(requestInfo, tableDesc.KeyLength);  
  switch(operationType) {
  case(UtilPrepareReq::Read):
    prepOpPtr.p->rsLen =
      attrLength + 
      tableDesc.NoOfKeyAttr +
      noOfNonPKAttribsStored;          // Read needs a resultset
    prepOpPtr.p->noOfAttr = tableDesc.NoOfKeyAttr + noOfNonPKAttribsStored;
    prepOpPtr.p->tckey.attrLen = prepOpPtr.p->noOfAttr;
    TcKeyReq::setOperationType(requestInfo, ZREAD);
    break;
  case(UtilPrepareReq::Update):
    prepOpPtr.p->rsLen = 0; 
    prepOpPtr.p->noOfAttr = tableDesc.NoOfKeyAttr + noOfNonPKAttribsStored;
    prepOpPtr.p->tckey.attrLen = attrLength + prepOpPtr.p->noOfAttr;
    TcKeyReq::setOperationType(requestInfo, ZUPDATE);
    break;
  case(UtilPrepareReq::Insert):
    prepOpPtr.p->rsLen = 0; 
    prepOpPtr.p->noOfAttr = tableDesc.NoOfKeyAttr + noOfNonPKAttribsStored;
    prepOpPtr.p->tckey.attrLen = attrLength + prepOpPtr.p->noOfAttr;
    TcKeyReq::setOperationType(requestInfo, ZINSERT);
    break;
  case(UtilPrepareReq::Delete):
    // The number of attributes should equal the size of the primary key
    ndbrequire(tableDesc.KeyLength == attrLength);
    prepOpPtr.p->rsLen = 0; 
    prepOpPtr.p->noOfAttr = tableDesc.NoOfKeyAttr;
    prepOpPtr.p->tckey.attrLen = 0;
    TcKeyReq::setOperationType(requestInfo, ZDELETE);
    break;
  case(UtilPrepareReq::Write):
    prepOpPtr.p->rsLen = 0; 
    prepOpPtr.p->noOfAttr = tableDesc.NoOfKeyAttr + noOfNonPKAttribsStored;
    prepOpPtr.p->tckey.attrLen = attrLength + prepOpPtr.p->noOfAttr;
    TcKeyReq::setOperationType(requestInfo, ZWRITE);
    break;
  }
  TcKeyReq::setAIInTcKeyReq(requestInfo, 0);  // Attrinfo sent separately
  prepOpPtr.p->tckey.requestInfo = requestInfo;

  /****************************
   * Confirm completed prepare
   ****************************/
  UtilPrepareConf * conf = (UtilPrepareConf *)signal->getDataPtr();
  conf->senderData = prepPtr.p->clientData;
  conf->prepareId = prepPtr.p->prepOpPtr.i;

  releaseSections(signal);
  sendSignal(prepPtr.p->clientRef, GSN_UTIL_PREPARE_CONF, signal, 
	     UtilPrepareConf::SignalLength, JBB);

#if 0
  prepPtr.p->prepOpPtr.p->print();
#endif
  releasePrepare(prepPtr);
}


void 
DbUtil::execUTIL_RELEASE_REQ(Signal* signal){
  jamEntry();  
  
  UtilReleaseReq * req = (UtilReleaseReq *)signal->getDataPtr();
  const Uint32 clientRef      = signal->senderBlockRef();
  const Uint32 prepareId      = req->prepareId;
  const Uint32 senderData     = req->senderData;

#if 0
  /**
   * This only works in when ARRAY_GUARD is defined (debug-mode)
   */
  if (!c_preparedOperationPool.isSeized(prepareId)) {
    UtilReleaseRef * ref = (UtilReleaseRef *)signal->getDataPtr();
    ref->prepareId = prepareId;
    ref->errorCode = UtilReleaseRef::NO_SUCH_PREPARE_SEIZED;
    sendSignal(clientRef, GSN_UTIL_RELEASE_REF, signal, 
	       UtilReleaseRef::SignalLength, JBB);
  }
#endif
  PreparedOperationPtr prepOpPtr;
  c_preparedOperationPool.getPtr(prepOpPtr, prepareId);
  
  releasePreparedOperation(prepOpPtr);
  
  UtilReleaseConf * const conf = (UtilReleaseConf*)signal->getDataPtrSend();
  conf->senderData = senderData;
  sendSignal(clientRef, GSN_UTIL_RELEASE_CONF, signal, 
	     UtilReleaseConf::SignalLength, JBB);
}


/**************************************************************************
 * ------------------------------------------------------------------------
 *  MODULE:       Sequence Service
 * ------------------------------------------------------------------------
 *
 *  A service with a stored incrementable number
 **************************************************************************/

void
DbUtil::hardcodedPrepare() {
  /**
   * Prepare SequenceCurrVal (READ)
   */
  {
    PreparedOperationPtr ptr;
    ndbrequire(c_preparedOperationPool.seizeId(ptr, 0));
    ptr.p->keyLen = 1;
    ptr.p->tckey.attrLen = 1;
    ptr.p->rsLen = 3;
    ptr.p->tckeyLenInBytes = (TcKeyReq::StaticLength +
                              ptr.p->keyLen + ptr.p->tckey.attrLen) * 4;
    ptr.p->keyDataPos = TcKeyReq::StaticLength; 
    ptr.p->tckey.tableId = 0;
    Uint32 requestInfo = 0;
    TcKeyReq::setAbortOption(requestInfo, TcKeyReq::CommitIfFailFree);
    TcKeyReq::setOperationType(requestInfo, ZREAD);
    TcKeyReq::setKeyLength(requestInfo, 1);
    TcKeyReq::setAIInTcKeyReq(requestInfo, 1);
    ptr.p->tckey.requestInfo = requestInfo;
    ptr.p->tckey.tableSchemaVersion = 1;

    // This is actually attr data
    AttributeHeader::init(&ptr.p->tckey.distrGroupHashValue, 1, 0); 
    
    ndbrequire(ptr.p->rsInfo.seize(1));
    ResultSetInfoBuffer::DataBufferIterator it; 
    ptr.p->rsInfo.first(it);
    AttributeHeader::init(it.data, 1, 2); // Attribute 1 - 2 data words
  }

  /**
   * Prepare SequenceNextVal (UPDATE)
   */
  {
    PreparedOperationPtr ptr;
    ndbrequire(c_preparedOperationPool.seizeId(ptr, 1));
    ptr.p->keyLen = 1;
    ptr.p->rsLen = 3;
    ptr.p->tckeyLenInBytes = (TcKeyReq::StaticLength + ptr.p->keyLen + 5) * 4;
    ptr.p->keyDataPos = TcKeyReq::StaticLength; 
    ptr.p->tckey.attrLen = 11;
    ptr.p->tckey.tableId = 0;
    Uint32 requestInfo = 0;
    TcKeyReq::setAbortOption(requestInfo, TcKeyReq::CommitIfFailFree);
    TcKeyReq::setOperationType(requestInfo, ZUPDATE);
    TcKeyReq::setKeyLength(requestInfo, 1);
    TcKeyReq::setAIInTcKeyReq(requestInfo, 5);
    TcKeyReq::setInterpretedFlag(requestInfo, 1);
    ptr.p->tckey.requestInfo = requestInfo;
    ptr.p->tckey.tableSchemaVersion = 1;
    
    // Signal is packed, which is why attrInfo is at distrGroupHashValue 
    // position 
    Uint32 * attrInfo = &ptr.p->tckey.distrGroupHashValue;
    attrInfo[0] = 0; // IntialReadSize
    attrInfo[1] = 5; // InterpretedSize
    attrInfo[2] = 0; // FinalUpdateSize
    attrInfo[3] = 1; // FinalReadSize
    attrInfo[4] = 0; // SubroutineSize
    
    { // AttrInfo
      ndbrequire(ptr.p->attrInfo.seize(6));
      AttrInfoBuffer::DataBufferIterator it;
      ptr.p->attrInfo.first(it);
      * it.data = Interpreter::Read(1, 6);
      ndbrequire(ptr.p->attrInfo.next(it));
      * it.data = Interpreter::LoadConst16(7, 1);
      ndbrequire(ptr.p->attrInfo.next(it));
      * it.data = Interpreter::Add(7, 6, 7);
      ndbrequire(ptr.p->attrInfo.next(it));
      * it.data = Interpreter::Write(1, 7);
      ndbrequire(ptr.p->attrInfo.next(it));
      * it.data = Interpreter::ExitOK();
      
      ndbrequire(ptr.p->attrInfo.next(it));
      AttributeHeader::init(it.data, 1, 0);
    }
    
    { // ResultSet
      ndbrequire(ptr.p->rsInfo.seize(1));
      ResultSetInfoBuffer::DataBufferIterator it; 
      ptr.p->rsInfo.first(it);
      AttributeHeader::init(it.data, 1, 2); // Attribute 1 - 2 data words
    }
  }

  /**
   * Prepare CreateSequence (INSERT)
   */
  {
    PreparedOperationPtr ptr;
    ndbrequire(c_preparedOperationPool.seizeId(ptr, 2));
    ptr.p->keyLen = 1;
    ptr.p->tckey.attrLen = 5;
    ptr.p->rsLen = 0;
    ptr.p->tckeyLenInBytes = (TcKeyReq::StaticLength +
                              ptr.p->keyLen + ptr.p->tckey.attrLen) * 4;
    ptr.p->keyDataPos = TcKeyReq::StaticLength;
    ptr.p->tckey.tableId = 0;
    Uint32 requestInfo = 0;
    TcKeyReq::setAbortOption(requestInfo, TcKeyReq::CommitIfFailFree);
    TcKeyReq::setOperationType(requestInfo, ZINSERT);
    TcKeyReq::setKeyLength(requestInfo, 1);
    TcKeyReq::setAIInTcKeyReq(requestInfo, 0);
    ptr.p->tckey.requestInfo = requestInfo;
    ptr.p->tckey.tableSchemaVersion = 1;
  }
}

void
DbUtil::execUTIL_SEQUENCE_REQ(Signal* signal){
  jamEntry();

  UtilSequenceReq * req = (UtilSequenceReq*)signal->getDataPtr();
  
  PreparedOperation * prepOp;
  
  switch(req->requestType){
  case UtilSequenceReq::CurrVal:
    prepOp = c_preparedOperationPool.getPtr(0); //c_SequenceCurrVal
    break;
  case UtilSequenceReq::NextVal:
    prepOp = c_preparedOperationPool.getPtr(1); //c_SequenceNextVal
    break;
  case UtilSequenceReq::Create:
    prepOp = c_preparedOperationPool.getPtr(2); //c_CreateSequence
    break;
  default:
    ndbrequire(false);
  }
  
  /**
   * 1 Transaction with 1 operation
   */
  TransactionPtr transPtr;
  ndbrequire(c_runningTransactions.seize(transPtr));
  
  OperationPtr opPtr;
  ndbrequire(transPtr.p->operations.seize(opPtr));
  
  ndbrequire(opPtr.p->rs.seize(prepOp->rsLen));
  ndbrequire(opPtr.p->keyInfo.seize(prepOp->keyLen));

  transPtr.p->gsn = GSN_UTIL_SEQUENCE_REQ;
  transPtr.p->clientRef = signal->senderBlockRef();
  transPtr.p->clientData = req->senderData;
  transPtr.p->sequence.sequenceId = req->sequenceId;
  transPtr.p->sequence.requestType = req->requestType;
  
  opPtr.p->prepOp   = prepOp;
  opPtr.p->prepOp_i = RNIL;

  KeyInfoBuffer::DataBufferIterator it; 
  opPtr.p->keyInfo.first(it);
  it.data[0] = transPtr.p->sequence.sequenceId;

  if(req->requestType == UtilSequenceReq::Create){
    ndbrequire(opPtr.p->attrInfo.seize(5));
    AttrInfoBuffer::DataBufferIterator it;   

    opPtr.p->attrInfo.first(it);
    AttributeHeader::init(it.data, 0, 1);

    ndbrequire(opPtr.p->attrInfo.next(it));
    * it.data = transPtr.p->sequence.sequenceId;

    ndbrequire(opPtr.p->attrInfo.next(it));
    AttributeHeader::init(it.data, 1, 2);
    
    ndbrequire(opPtr.p->attrInfo.next(it));
    * it.data = 0;
    
    ndbrequire(opPtr.p->attrInfo.next(it));
    * it.data = 0;
  }
  
  runTransaction(signal, transPtr);
}

int
DbUtil::getResultSet(Signal* signal, const Transaction * transP,
		     struct LinearSectionPtr sectionsPtr[]) {
  OperationPtr opPtr;
  ndbrequire(transP->operations.first(opPtr));
  ndbrequire(transP->operations.hasNext(opPtr) == false);

  int noAttr = 0;
  int dataSz = 0;
  Uint32* tmpBuf = signal->theData + 25;
  const Uint32* headerBuffer = tmpBuf;

  const ResultSetBuffer & rs = opPtr.p->rs;
  ResultSetInfoBuffer::ConstDataBufferIterator it;

  // extract headers
  for(rs.first(it); it.curr.i != RNIL; ) {
    *tmpBuf++ = it.data[0];
    rs.next(it, ((AttributeHeader*)&it.data[0])->getDataSize() + 1);
    noAttr++;
  }

  if (noAttr == 0)
    return 0;

  const Uint32* dataBuffer = tmpBuf;

  // extract data
  for(rs.first(it); it.curr.i != RNIL; ) {
    int sz = ((AttributeHeader*)&it.data[0])->getDataSize();
    rs.next(it,1);
    for (int i = 0; i < sz; i++) {
      *tmpBuf++ = *it.data;
      rs.next(it,1);
      dataSz++;
    }
  }

  sectionsPtr[UtilExecuteReq::HEADER_SECTION].p = (Uint32 *)headerBuffer;
  sectionsPtr[UtilExecuteReq::HEADER_SECTION].sz = noAttr;
  sectionsPtr[UtilExecuteReq::DATA_SECTION].p = (Uint32 *)dataBuffer;
  sectionsPtr[UtilExecuteReq::DATA_SECTION].sz = dataSz;

  return 1;
}

void
DbUtil::reportSequence(Signal* signal, const Transaction * transP){
  OperationPtr opPtr;
  ndbrequire(transP->operations.first(opPtr));
  ndbrequire(transP->operations.hasNext(opPtr) == false);
  
  if(transP->errorCode == 0){
    jam(); // OK

    UtilSequenceConf * ret = (UtilSequenceConf *)signal->getDataPtrSend();
    ret->senderData = transP->clientData;
    ret->sequenceId = transP->sequence.sequenceId;
    ret->requestType = transP->sequence.requestType;
    
    bool ok = false;
    switch(transP->sequence.requestType){
    case UtilSequenceReq::CurrVal:
    case UtilSequenceReq::NextVal:{
      ok = true;
      ndbrequire(opPtr.p->rsRecv == 3);
      
      ResultSetBuffer::DataBufferIterator rsit;  
      ndbrequire(opPtr.p->rs.first(rsit));
      
      ret->sequenceValue[0] = rsit.data[1];
      ret->sequenceValue[1] = rsit.data[2];
      break;
    }
    case UtilSequenceReq::Create:
      ok = true;
      ret->sequenceValue[0] = 0;
      ret->sequenceValue[1] = 0;
      break;
    }
    ndbrequire(ok);
    sendSignal(transP->clientRef, GSN_UTIL_SEQUENCE_CONF, signal, 
	       UtilSequenceConf::SignalLength, JBB);
    return;
  }
  
  UtilSequenceRef::ErrorCode errCode = UtilSequenceRef::TCError;

  switch(transP->sequence.requestType)
    {
    case UtilSequenceReq::CurrVal:
    case UtilSequenceReq::NextVal:{
      if (transP->errorCode == 626)
	errCode = UtilSequenceRef::NoSuchSequence;
      break;
    }
    case UtilSequenceReq::Create:
      break;
    }

  UtilSequenceRef * ret = (UtilSequenceRef *)signal->getDataPtrSend();
  ret->senderData = transP->clientData;
  ret->sequenceId = transP->sequence.sequenceId;
  ret->requestType = transP->sequence.requestType;
  ret->errorCode = (Uint32)errCode;
  sendSignal(transP->clientRef, GSN_UTIL_SEQUENCE_REF, signal, 
	     UtilSequenceRef::SignalLength, JBB);
}
#if 0
  Ndb ndb("ndb","def");
  NdbConnection* tConnection = ndb.startTransaction();
  NdbOperation* tOperation = tConnection->getNdbOperation("SYSTAB_0");     
  
  //#if 0 && API_CODE
  if( tOperation != NULL ) {
    tOperation->interpretedUpdateTuple();
    tOperation->equal((U_Int32)0, keyValue );
    tNextId_Result = tOperation->getValue((U_Int32)1);
    tOperation->incValue((U_Int32)1, (U_Int32)8192);
    
    if (tConnection->execute( Commit ) != -1 ) {
      U_Int64 tValue = tNextId_Result->u_64_value();   // Read result value
      theFirstTransId = tValue;
      theLastTransId  = tValue + 8191;
      closeTransaction(tConnection);
      return startTransactionLocal(aPriority, nodeId);
    }
  }
  /**
   * IntialReadSize = 0;
   * InterpretedSize = incValue(1);
   * FinalUpdateSize = 0;
   * FinalReadSize = 1; // Read value
   * SubroutineSize = 0;
   */
#endif


/**************************************************************************
 * ------------------------------------------------------------------------
 *  MODULE:       Transaction execution request
 * ------------------------------------------------------------------------
 *  
 *  Handle requests to execute a prepared transaction
 **************************************************************************/

void 
DbUtil::execUTIL_EXECUTE_REQ(Signal* signal) 
{
  jamEntry();

  UtilExecuteReq * req = (UtilExecuteReq *)signal->getDataPtr();
  const Uint32  clientRef      = req->senderRef;
  const Uint32  clientData     = req->senderData;
  const Uint32  prepareId      = req->getPrepareId();
  const bool    releaseFlag    = req->getReleaseFlag();

  if(signal->getNoOfSections() == 0) {
    // Missing prepare data
    jam();
    releaseSections(signal);
    sendUtilExecuteRef(signal, UtilExecuteRef::MissingDataSection, 
		       0, clientRef, clientData);
    return;
  }
  /*******************************
   * Get PreparedOperation struct
   *******************************/
  PreparedOperationPtr prepOpPtr;
  c_runningPreparedOperations.first(prepOpPtr);
  while (!prepOpPtr.isNull() && prepOpPtr.i != prepareId) 
    c_runningPreparedOperations.next(prepOpPtr);
  
  if (prepOpPtr.i != prepareId) {
    jam();
    releaseSections(signal);
    sendUtilExecuteRef(signal, UtilExecuteRef::IllegalPrepareId,
		       0, clientRef, clientData);
    return;
  }

  prepOpPtr.p->releaseFlag = releaseFlag;

  TransactionPtr  transPtr;
  OperationPtr    opPtr;
  SegmentedSectionPtr headerPtr, dataPtr;

  signal->getSection(headerPtr, UtilExecuteReq::HEADER_SECTION);
  SectionReader headerReader(headerPtr, getSectionSegmentPool());
  signal->getSection(dataPtr, UtilExecuteReq::DATA_SECTION);
  SectionReader dataReader(dataPtr, getSectionSegmentPool());

#if 0 //def EVENT_DEBUG
  // Debugging
  printf("DbUtil::execUTIL_EXECUTEL_REQ: Headers (%u): ", headerPtr.sz);
  Uint32 word;
  while(headerReader.getWord(&word))
    printf("H'%.8x ", word);
  printf("\n");
  printf("DbUtil::execUTIL_EXECUTEL_REQ: Data (%u): ", dataPtr.sz);
  headerReader.reset();
  while(dataReader.getWord(&word))
    printf("H'%.8x ", word);
  printf("\n");
  dataReader.reset();
#endif
  
//  Uint32 totalDataLen = headerPtr.sz + dataPtr.sz;

  /************************************************************
   * Seize Transaction record
   ************************************************************/
  ndbrequire(c_runningTransactions.seize(transPtr));
  transPtr.p->gsn        = GSN_UTIL_EXECUTE_REQ;
  transPtr.p->clientRef  = clientRef;
  transPtr.p->clientData = clientData;
  ndbrequire(transPtr.p->operations.seize(opPtr));
  opPtr.p->prepOp   = prepOpPtr.p;
  opPtr.p->prepOp_i = prepOpPtr.i;
  
#if 0 //def EVENT_DEBUG
  printf("opPtr.p->rs.seize( %u )\n", prepOpPtr.p->rsLen);
#endif
  ndbrequire(opPtr.p->rs.seize(prepOpPtr.p->rsLen));
  
 /***********************************************************
   * Store signal data on linear memory in Transaction record 
   ***********************************************************/
  KeyInfoBuffer* keyInfo = &opPtr.p->keyInfo;
  AttrInfoBuffer* attrInfo = &opPtr.p->attrInfo;
  AttributeHeader header;
  Uint32* tempBuf = signal->theData + 25;
  bool dataComplete = true;

  while(headerReader.getWord((Uint32 *)&header)) {
    Uint32* bufStart = tempBuf;
    header.insertHeader(tempBuf++);
    for(unsigned int i = 0; i < header.getDataSize(); i++) {
      if (!dataReader.getWord(tempBuf++)) {
	dataComplete = false;
	break;
      } 
    }
    bool res = true;

#if 0 //def EVENT_DEBUG
    if (TcKeyReq::getOperationType(prepOpPtr.p->tckey.requestInfo) ==
	TcKeyReq::Read) {
      if(prepOpPtr.p->pkBitmask.get(header.getAttributeId()))
	printf("PrimaryKey\n");
    }
    printf("AttrId %u Hdrsz %d Datasz %u \n",
	   header.getAttributeId(),
	   header.getHeaderSize(),
	   header.getDataSize());
#endif

    if(prepOpPtr.p->pkBitmask.get(header.getAttributeId()))
      // A primary key attribute
      res = keyInfo->append(bufStart + header.getHeaderSize(), 
			    header.getDataSize());

    switch (TcKeyReq::getOperationType(prepOpPtr.p->tckey.requestInfo)) {
    case ZREAD:
      res &= attrInfo->append(bufStart, header.getHeaderSize());
      break;
    case ZDELETE:
      // no attrinfo for Delete
      break;
    default:
      res &= attrInfo->append(bufStart,
			      header.getHeaderSize() + header.getDataSize());
    }

    if (!res) {
      // Failed to allocate buffer data
      jam();
      releaseSections(signal);
      sendUtilExecuteRef(signal, UtilExecuteRef::AllocationError, 
			 0, clientRef, clientData);
      releaseTransaction(transPtr);    
      return;
    }
  }
  if (!dataComplete) {
    // Missing data in data section
    jam();
    releaseSections(signal);
    sendUtilExecuteRef(signal, UtilExecuteRef::MissingData, 
		       0, clientRef, clientData);
    releaseTransaction(transPtr);    
    return;
  }

  const Uint32 l1 = prepOpPtr.p->tckey.attrLen;
  const Uint32 l2 = 
    prepOpPtr.p->attrInfo.getSize() + opPtr.p->attrInfo.getSize();

  if (TcKeyReq::getOperationType(prepOpPtr.p->tckey.requestInfo) != ZREAD){
    ndbrequire(l1 == l2);
  } else {
#if 0
    ndbout_c("TcKeyReq::Read");
#endif
  }

  releaseSections(signal);
  transPtr.p->noOfRetries = 3;
  runTransaction(signal, transPtr);
}

/**************************************************************************
 * ------------------------------------------------------------------------
 *  MODULE:       General transaction machinery
 * ------------------------------------------------------------------------
 *   Executes a prepared transaction
 **************************************************************************/
void
DbUtil::runTransaction(Signal* signal, TransactionPtr transPtr){

  /* Init transaction */
  transPtr.p->sent = 0;
  transPtr.p->recv = 0;
  transPtr.p->errorCode = 0;
  getTransId(transPtr.p);
  
  OperationPtr opPtr;
  ndbrequire(transPtr.p->operations.first(opPtr));
  
  /* First operation */
  Uint32 start = 0;
  TcKeyReq::setStartFlag(start, 1);
  runOperation(signal, transPtr, opPtr, start);
  transPtr.p->sent ++;
  
  /* Rest of operations */
  start = 0;
  while(opPtr.i != RNIL){
    runOperation(signal, transPtr, opPtr, start);
    transPtr.p->sent ++;
  }
  //transPtr.p->print();
}

void
DbUtil::runOperation(Signal* signal, TransactionPtr & transPtr, 
		     OperationPtr & opPtr, Uint32 start) {
  Uint32 opI = opPtr.i;
  Operation * op = opPtr.p;
  const PreparedOperation * pop = op->prepOp;

  if(!transPtr.p->operations.next(opPtr)){
    TcKeyReq::setCommitFlag(start, 1);   // Last operation
    TcKeyReq::setExecuteFlag(start, 1);
  }
  
#if 0 //def EVENT_DEBUG
  if (TcKeyReq::getOperationType(pop->tckey.requestInfo) ==
      TcKeyReq::Read) {
    printf("TcKeyReq::Read runOperation\n");
  }
#endif

  /**
   * Init operation w.r.t result set
   */
  initResultSet(op->rs, pop->rsInfo);
  op->rs.first(op->rsIterator);
  op->rsRecv = 0;
#if 0 //def EVENT_DEBUG
  printf("pop->rsLen %u\n", pop->rsLen);
#endif
  op->rsExpect = 0;
  op->transPtrI = transPtr.i;
  
  TcKeyReq * tcKey = (TcKeyReq*)signal->getDataPtrSend();
  //ndbout << "*** 6 ***"<< endl; pop->print();
  memcpy(tcKey, &pop->tckey, pop->tckeyLenInBytes);
  //ndbout << "*** 6b ***"<< endl; 
  //printTCKEYREQ(stdout, signal->getDataPtrSend(), 
  //              pop->tckeyLenInBytes >> 2, 0);
  tcKey->apiConnectPtr = transPtr.p->connectPtr;
  tcKey->senderData = opI;
  tcKey->transId1 = transPtr.p->transId[0];
  tcKey->transId2 = transPtr.p->transId[1];
  tcKey->requestInfo |= start;
  
#if 0 //def EVENT_DEBUG
  // Debugging
  printf("DbUtil::runOperation: KEYINFO\n");
  op->keyInfo.print(stdout);
  printf("DbUtil::runOperation: ATTRINFO\n");
  op->attrInfo.print(stdout);
#endif

  /**
   * Key Info
   */
  //KeyInfoBuffer::DataBufferIterator kit; 
  KeyInfoIterator kit;
  op->keyInfo.first(kit);
  Uint32 *keyDst = ((Uint32*)tcKey) + pop->keyDataPos;
  for(Uint32 i = 0; i<8 && kit.curr.i != RNIL; i++, op->keyInfo.next(kit)){
    keyDst[i] = * kit.data;
  }
  //ndbout << "*** 7 ***" << endl;
  //printTCKEYREQ(stdout, signal->getDataPtrSend(), 
  //		pop->tckeyLenInBytes >> 2, 0);

#if 0 //def EVENT_DEBUG
    printf("DbUtil::runOperation: sendSignal(DBTC_REF, GSN_TCKEYREQ, signal, %d , JBB)\n",  pop->tckeyLenInBytes >> 2);
    printTCKEYREQ(stdout, signal->getDataPtr(), pop->tckeyLenInBytes >> 2,0);
#endif
  sendSignal(DBTC_REF, GSN_TCKEYREQ, signal, pop->tckeyLenInBytes >> 2, JBB);
  
  /**
   * More the 8 words of key info not implemented
   */
  // ndbrequire(kit.curr.i == RNIL); // Yes it is

  /**
   * KeyInfo
   */
  KeyInfo* keyInfo = (KeyInfo *)signal->getDataPtrSend();
  keyInfo->connectPtr = transPtr.p->connectPtr;
  keyInfo->transId[0] = transPtr.p->transId[0];
  keyInfo->transId[1] = transPtr.p->transId[1];
  sendKeyInfo(signal, keyInfo, op->keyInfo, kit);

  /**
   * AttrInfo
   */
  AttrInfo* attrInfo = (AttrInfo *)signal->getDataPtrSend();
  attrInfo->connectPtr = transPtr.p->connectPtr;
  attrInfo->transId[0] = transPtr.p->transId[0];
  attrInfo->transId[1] = transPtr.p->transId[1];

  AttrInfoIterator ait;
  pop->attrInfo.first(ait);
  sendAttrInfo(signal, attrInfo, pop->attrInfo, ait);
  
  op->attrInfo.first(ait);
  sendAttrInfo(signal, attrInfo, op->attrInfo, ait);
}

void
DbUtil::sendKeyInfo(Signal* signal, 
		    KeyInfo* keyInfo,
		    const KeyInfoBuffer & keyBuf,
		    KeyInfoIterator & kit)
{
  while(kit.curr.i != RNIL) {
    Uint32 *keyDst = keyInfo->keyData;
    Uint32 keyDataLen = 0;
    for(Uint32 i = 0; i<KeyInfo::DataLength && kit.curr.i != RNIL; 
	i++, keyBuf.next(kit)){
      keyDst[i] = * kit.data;
      keyDataLen++;
    }
#if 0 //def EVENT_DEBUG
    printf("DbUtil::sendKeyInfo: sendSignal(DBTC_REF, GSN_KEYINFO, signal, %d , JBB)\n", KeyInfo::HeaderLength + keyDataLen);
#endif
    sendSignal(DBTC_REF, GSN_KEYINFO, signal, 
	       KeyInfo::HeaderLength + keyDataLen, JBB);
  }
}

void
DbUtil::sendAttrInfo(Signal* signal, 
		     AttrInfo* attrInfo, 
		     const AttrInfoBuffer & attrBuf,
		     AttrInfoIterator & ait)
{
  while(ait.curr.i != RNIL) {
    Uint32 *attrDst = attrInfo->attrData;
    Uint32 i = 0;
    for(i = 0; i<AttrInfo::DataLength && ait.curr.i != RNIL; 
	i++, attrBuf.next(ait)){
      attrDst[i] = * ait.data;
    }
#if 0 //def EVENT_DEBUG
    printf("DbUtil::sendAttrInfo: sendSignal(DBTC_REF, GSN_ATTRINFO, signal, %d , JBB)\n", AttrInfo::HeaderLength + i);
#endif
    sendSignal(DBTC_REF, GSN_ATTRINFO, signal, 
	       AttrInfo::HeaderLength + i, JBB);
  }
}

void
DbUtil::initResultSet(ResultSetBuffer & rs, 
		      const ResultSetInfoBuffer & rsi){
  
  ResultSetBuffer::DataBufferIterator rsit;
  rs.first(rsit);
  
  ResultSetInfoBuffer::ConstDataBufferIterator rsiit;
  for(rsi.first(rsiit); rsiit.curr.i != RNIL; rsi.next(rsiit)){
    ndbrequire(rsit.curr.i != RNIL);
    
    rsit.data[0] = rsiit.data[0];
#if 0 //def EVENT_DEBUG
    printf("Init resultset %u, sz %d\n",
	   rsit.curr.i,
	   ((AttributeHeader*)&rsit.data[0])->getDataSize() + 1);
#endif
    rs.next(rsit, ((AttributeHeader*)&rsit.data[0])->getDataSize() + 1);
  }
}

void
DbUtil::getTransId(Transaction * transP){

  Uint32 tmp[2];
  tmp[0] = c_transId[0];
  tmp[1] = c_transId[1];

  transP->transId[0] = tmp[0];
  transP->transId[1] = tmp[1];
  
  c_transId[1] = tmp[1] + 1;
}



/**************************************************************************
 * ------------------------------------------------------------------------
 *  MODULE:       Post Execute 
 * ------------------------------------------------------------------------
 *
 *  Handles result from a sent transaction
 **************************************************************************/

/**
 * execTRANSID_AI
 *
 * Receive result from transaction
 *
 * NOTE: This codes assumes that 
 *       TransidAI::DataLength = ResultSetBuffer::getSegmentSize() * n
 */
void
DbUtil::execTRANSID_AI(Signal* signal){
  jamEntry();
#if 0 //def EVENT_DEBUG
  ndbout_c("File: %s line: %u",__FILE__,__LINE__);
#endif

  const Uint32 opI      = signal->theData[0];
  const Uint32 transId1 = signal->theData[1];
  const Uint32 transId2 = signal->theData[2];
  const Uint32 dataLen  = signal->length() - 3;

  Operation * opP = c_operationPool.getPtr(opI);
  TransactionPtr transPtr;
  c_runningTransactions.getPtr(transPtr, opP->transPtrI);

  ndbrequire(transId1 == transPtr.p->transId[0] && 
	     transId2 == transPtr.p->transId[1]);
  opP->rsRecv += dataLen;
  
  /**
   * Save result
   */
  const Uint32 *src = &signal->theData[3];
  ResultSetBuffer::DataBufferIterator rs = opP->rsIterator;

  ndbrequire(opP->rs.import(rs,src,dataLen));
  opP->rs.next(rs, dataLen);
  opP->rsIterator = rs;

  if(!opP->complete()){
   jam();
   return;
  }

  transPtr.p->recv++;
  if(!transPtr.p->complete()){
    jam();
    return;
  }

  finishTransaction(signal, transPtr);
}

void
DbUtil::execTCKEYCONF(Signal* signal){
  jamEntry();
#if 0 //def EVENT_DEBUG
  ndbout_c("File: %s line: %u",__FILE__,__LINE__);
#endif
  
  TcKeyConf * keyConf = (TcKeyConf*)signal->getDataPtr();

  //const Uint32 gci      = keyConf->gci;
  const Uint32 transI   = keyConf->apiConnectPtr >> 1;
  const Uint32 confInfo = keyConf->confInfo;
  const Uint32 transId1 = keyConf->transId1;
  const Uint32 transId2 = keyConf->transId2;
  
  Uint32 recv = 0;
  const Uint32 ops = TcKeyConf::getNoOfOperations(confInfo);  
  for(Uint32 i = 0; i<ops; i++){
    OperationPtr opPtr;
    c_operationPool.getPtr(opPtr, keyConf->operations[i].apiOperationPtr);
    
    ndbrequire(opPtr.p->transPtrI == transI);
    opPtr.p->rsExpect += keyConf->operations[i].attrInfoLen;
    if(opPtr.p->complete()){
      recv++;
    }
  }	

  /**
   * Check commit ack marker flag
   */
  if (TcKeyConf::getMarkerFlag(confInfo)){
    signal->theData[0] = transId1;
    signal->theData[1] = transId2;
    sendSignal(DBTC_REF, GSN_TC_COMMIT_ACK, signal, 2, JBB);    
  }//if

  TransactionPtr transPtr;
  c_runningTransactions.getPtr(transPtr, transI);
  ndbrequire(transId1 == transPtr.p->transId[0] && 
	     transId2 == transPtr.p->transId[1]);
  
  transPtr.p->recv += recv;
  if(!transPtr.p->complete()){
    jam();
    return;
  }
  finishTransaction(signal, transPtr);
}

void
DbUtil::execTCKEYREF(Signal* signal){
  jamEntry();
#if 0 //def EVENT_DEBUG
  ndbout_c("File: %s line: %u",__FILE__,__LINE__);
#endif

  const Uint32 transI   = signal->theData[0] >> 1;
  const Uint32 transId1 = signal->theData[1];
  const Uint32 transId2 = signal->theData[2];
  const Uint32 errCode  = signal->theData[3];

  TransactionPtr transPtr;
  c_runningTransactions.getPtr(transPtr, transI);
  ndbrequire(transId1 == transPtr.p->transId[0] && 
	     transId2 == transPtr.p->transId[1]);

  //if(getClassification(errCode) == PermanentError){
  //}
  
  //ndbout << "Transaction error (code: " << errCode << ")" << endl;
  
  transPtr.p->errorCode = errCode;
  finishTransaction(signal, transPtr);
}

void
DbUtil::execTCROLLBACKREP(Signal* signal){
  jamEntry();
#if 0 //def EVENT_DEBUG
  ndbout_c("File: %s line: %u",__FILE__,__LINE__);
#endif

  const Uint32 transI   = signal->theData[0] >> 1;
  const Uint32 transId1 = signal->theData[1];
  const Uint32 transId2 = signal->theData[2];
  const Uint32 errCode  = signal->theData[3];

  TransactionPtr transPtr;
  c_runningTransactions.getPtr(transPtr, transI);
  ndbrequire(transId1 == transPtr.p->transId[0] && 
	     transId2 == transPtr.p->transId[1]);

  //if(getClassification(errCode) == PermanentError){
  //}
  
#if 0 //def EVENT_DEBUG
  ndbout << "Transaction error (code: " << errCode << ")" << endl;
#endif
 
  if(transPtr.p->noOfRetries > 0){
    transPtr.p->noOfRetries--;
    switch(errCode){
    case 266:
    case 410:
    case 1204:
#if 0
      ndbout_c("errCode: %d noOfRetries: %d -> retry", 
	       errCode, transPtr.p->noOfRetries);
#endif
      runTransaction(signal, transPtr);
      return;
    }
  }

  transPtr.p->errorCode = errCode;
  finishTransaction(signal, transPtr);
}

void 
DbUtil::finishTransaction(Signal* signal, TransactionPtr transPtr){
#if 0 //def EVENT_DEBUG
  ndbout_c("Transaction %x %x completed %s",
	   transPtr.p->transId[0], 
	   transPtr.p->transId[1],
	   transPtr.p->errorCode == 0 ? "OK" : "FAILED");
#endif

  /* 
     How to find the correct RS?  Could we have multi-RS/transaction?

  Operation * opP = c_operationPool.getPtr(opI);

  ResultSetBuffer::DataBufferIterator rsit;  
  ndbrequire(opP->rs.first(rsit));
  ndbout << "F Result: " << rsit.data << endl;

  while (opP->rs.next(rsit)) {
    ndbout << "R Result: " << rsit.data << endl;
  }
  */

  switch(transPtr.p->gsn){
  case GSN_UTIL_SEQUENCE_REQ:
    jam();
    reportSequence(signal, transPtr.p);
    break;
  case GSN_UTIL_EXECUTE_REQ:
    if (transPtr.p->errorCode) {
      UtilExecuteRef * ret = (UtilExecuteRef *)signal->getDataPtrSend();
      ret->senderData = transPtr.p->clientData;
      ret->errorCode = UtilExecuteRef::TCError;
      ret->TCErrorCode = transPtr.p->errorCode;
      sendSignal(transPtr.p->clientRef, GSN_UTIL_EXECUTE_REF, signal, 
		 UtilExecuteRef::SignalLength, JBB);
    } else {
      struct LinearSectionPtr sectionsPtr[UtilExecuteReq::NoOfSections];
      UtilExecuteConf * ret = (UtilExecuteConf *)signal->getDataPtrSend();
      ret->senderData = transPtr.p->clientData;
      if (getResultSet(signal, transPtr.p, sectionsPtr)) {
#if 0 //def EVENT_DEBUG
	for (int j = 0; j < 2; j++) {
	  printf("Result set %u %u\n", j,sectionsPtr[j].sz);
	  for (int i=0; i < sectionsPtr[j].sz; i++)
	    printf("H'%.8x ", sectionsPtr[j].p[i]);
	  printf("\n");
	}
#endif
	sendSignal(transPtr.p->clientRef, GSN_UTIL_EXECUTE_CONF, signal, 
		   UtilExecuteConf::SignalLength, JBB,
		   sectionsPtr, UtilExecuteReq::NoOfSections);
      } else
	sendSignal(transPtr.p->clientRef, GSN_UTIL_EXECUTE_CONF, signal, 
		   UtilExecuteConf::SignalLength, JBB);
    } 
    break;
  default:
    ndbrequire(0);
    break;
  }
  releaseTransaction(transPtr);
}

void
DbUtil::execUTIL_LOCK_REQ(Signal * signal){
  jamEntry();
  UtilLockReq * req = (UtilLockReq*)signal->getDataPtr();
  const Uint32 lockId = req->lockId;

  LockQueuePtr lockQPtr;
  if(!c_lockQueues.find(lockQPtr, lockId)){
    jam();
    sendLOCK_REF(signal, req, UtilLockRef::NoSuchLock);
    return;
  }

//  const Uint32 requestInfo = req->requestInfo;
  const Uint32 senderNode = refToNode(req->senderRef);
  if(senderNode != getOwnNodeId() && senderNode != 0){
    jam();
    sendLOCK_REF(signal, req, UtilLockRef::DistributedLockNotSupported);    
    return;
  }

  LocalDLFifoList<LockQueueElement> queue(c_lockElementPool,
					  lockQPtr.p->m_queue);
  if(req->requestInfo & UtilLockReq::TryLock && !queue.isEmpty()){
    jam();
    sendLOCK_REF(signal, req, UtilLockRef::LockAlreadyHeld);
    return;
  }
  
  LockQueueElementPtr lockEPtr;
  if(!c_lockElementPool.seize(lockEPtr)){
    jam();
    sendLOCK_REF(signal, req, UtilLockRef::OutOfLockRecords);
    return;
  }
  
  lockEPtr.p->m_senderRef = req->senderRef;
  lockEPtr.p->m_senderData = req->senderData;
  
  if(queue.isEmpty()){
    jam();
    sendLOCK_CONF(signal, lockQPtr.p, lockEPtr.p);
  }
  
  queue.add(lockEPtr);
}

void
DbUtil::execUTIL_UNLOCK_REQ(Signal* signal){
  jamEntry();
  
  UtilUnlockReq * req = (UtilUnlockReq*)signal->getDataPtr();
  const Uint32 lockId = req->lockId;
  
  LockQueuePtr lockQPtr;
  if(!c_lockQueues.find(lockQPtr, lockId)){
    jam();
    sendUNLOCK_REF(signal, req, UtilUnlockRef::NoSuchLock);
    return;
  }

  LocalDLFifoList<LockQueueElement> queue(c_lockElementPool, 
					  lockQPtr.p->m_queue);
  LockQueueElementPtr lockEPtr;
  if(!queue.first(lockEPtr)){
    jam();
    sendUNLOCK_REF(signal, req, UtilUnlockRef::NotLockOwner);
    return;
  }

  if(lockQPtr.p->m_lockKey != req->lockKey){
    jam();
    sendUNLOCK_REF(signal, req, UtilUnlockRef::NotLockOwner);
    return;
  }

  sendUNLOCK_CONF(signal, lockQPtr.p, lockEPtr.p);
  queue.release(lockEPtr);
  
  if(queue.first(lockEPtr)){
    jam();
    sendLOCK_CONF(signal, lockQPtr.p, lockEPtr.p);
    return;
  }
}

void
DbUtil::sendLOCK_REF(Signal* signal, 
		     const UtilLockReq * req, UtilLockRef::ErrorCode err){
  const Uint32 senderData = req->senderData;
  const Uint32 senderRef = req->senderRef;
  const Uint32 lockId = req->lockId;

  UtilLockRef * ref = (UtilLockRef*)signal->getDataPtrSend();
  ref->senderData = senderData;
  ref->senderRef = reference();
  ref->lockId = lockId;
  ref->errorCode = err;
  sendSignal(senderRef, GSN_UTIL_LOCK_REF, signal, 
	     UtilLockRef::SignalLength, JBB);
}

void
DbUtil::sendLOCK_CONF(Signal* signal, 
		      LockQueue * lockQP, 
		      LockQueueElement * lockEP){
  const Uint32 senderData = lockEP->m_senderData;
  const Uint32 senderRef = lockEP->m_senderRef;
  const Uint32 lockId = lockQP->m_lockId;
  const Uint32 lockKey = ++lockQP->m_lockKey;

  UtilLockConf * conf = (UtilLockConf*)signal->getDataPtrSend();
  conf->senderData = senderData;
  conf->senderRef = reference();
  conf->lockId = lockId;
  conf->lockKey = lockKey;
  sendSignal(senderRef, GSN_UTIL_LOCK_CONF, signal,
	     UtilLockConf::SignalLength, JBB);
}

void
DbUtil::sendUNLOCK_REF(Signal* signal, 
		       const UtilUnlockReq* req, UtilUnlockRef::ErrorCode err){
  
  const Uint32 senderData = req->senderData;
  const Uint32 senderRef = req->senderRef;
  const Uint32 lockId = req->lockId;
  
  UtilUnlockRef * ref = (UtilUnlockRef*)signal->getDataPtrSend();
  ref->senderData = senderData;
  ref->senderRef = reference();
  ref->lockId = lockId;
  ref->errorCode = err;
  sendSignal(senderRef, GSN_UTIL_UNLOCK_REF, signal, 
	     UtilUnlockRef::SignalLength, JBB);
}

void 
DbUtil::sendUNLOCK_CONF(Signal* signal, 
			LockQueue * lockQP, 
			LockQueueElement * lockEP){
  const Uint32 senderData = lockEP->m_senderData;
  const Uint32 senderRef = lockEP->m_senderRef;
  const Uint32 lockId = lockQP->m_lockId;
  ++lockQP->m_lockKey;
  
  UtilUnlockConf * conf = (UtilUnlockConf*)signal->getDataPtrSend();
  conf->senderData = senderData;
  conf->senderRef = reference();
  conf->lockId = lockId;
  sendSignal(senderRef, GSN_UTIL_UNLOCK_CONF, signal,
	     UtilUnlockConf::SignalLength, JBB);
}

void
DbUtil::execUTIL_CREATE_LOCK_REQ(Signal* signal){
  jamEntry();
  UtilCreateLockReq req = * (UtilCreateLockReq*)signal->getDataPtr();

  UtilCreateLockRef::ErrorCode err = UtilCreateLockRef::OK;

  do {
    LockQueuePtr lockQPtr;
    if(c_lockQueues.find(lockQPtr, req.lockId)){
      jam();
      err = UtilCreateLockRef::LockIdAlreadyUsed;
      break;
    }

    if(req.lockType != UtilCreateLockReq::Mutex){
      jam();
      err = UtilCreateLockRef::UnsupportedLockType;
      break;
    }
    
    if(!c_lockQueues.seize(lockQPtr)){
      jam();
      err = UtilCreateLockRef::OutOfLockQueueRecords;
      break;
    }      

    new (lockQPtr.p) LockQueue(req.lockId);
    c_lockQueues.add(lockQPtr);

    UtilCreateLockConf * conf = (UtilCreateLockConf*)signal->getDataPtrSend();
    conf->senderData = req.senderData;
    conf->senderRef = reference();
    conf->lockId = req.lockId;
    
    sendSignal(req.senderRef, GSN_UTIL_CREATE_LOCK_CONF, signal, 
	       UtilCreateLockConf::SignalLength, JBB);
    return;
  } while(false);

  UtilCreateLockRef * ref = (UtilCreateLockRef*)signal->getDataPtrSend();
  ref->senderData = req.senderData;
  ref->senderRef = reference();
  ref->lockId = req.lockId;
  ref->errorCode = err;

  sendSignal(req.senderRef, GSN_UTIL_CREATE_LOCK_REF, signal, 
	     UtilCreateLockRef::SignalLength, JBB);
}

void
DbUtil::execUTIL_DESTORY_LOCK_REQ(Signal* signal){
  jamEntry();
  
  UtilDestroyLockReq req = * (UtilDestroyLockReq*)signal->getDataPtr();
  UtilDestroyLockRef::ErrorCode err = UtilDestroyLockRef::OK;
  do {
    LockQueuePtr lockQPtr;
    if(!c_lockQueues.find(lockQPtr, req.lockId)){
      jam();
      err = UtilDestroyLockRef::NoSuchLock;
      break;
    }
    
    LocalDLFifoList<LockQueueElement> queue(c_lockElementPool, 
					    lockQPtr.p->m_queue);
    LockQueueElementPtr lockEPtr;
    if(!queue.first(lockEPtr)){
      jam();
      err = UtilDestroyLockRef::NotLockOwner;
      break;
    }
    
    if(lockQPtr.p->m_lockKey != req.lockKey){
      jam();
      err = UtilDestroyLockRef::NotLockOwner;
      break;
    }
    
    /**
     * OK
     */
    
    // Inform all in lock queue that queue has been destroyed
    UtilLockRef * ref = (UtilLockRef*)signal->getDataPtrSend();
    ref->lockId = req.lockId;
    ref->errorCode = UtilLockRef::NoSuchLock;
    ref->senderRef = reference();
    LockQueueElementPtr loopPtr = lockEPtr;      
    for(queue.next(loopPtr); !loopPtr.isNull(); queue.next(loopPtr)){
      jam();
      ref->senderData = loopPtr.p->m_senderData;
      const Uint32 senderRef = loopPtr.p->m_senderRef;
      sendSignal(senderRef, GSN_UTIL_LOCK_REF, signal, 
		 UtilLockRef::SignalLength, JBB);
    }
    queue.release();
    c_lockQueues.release(lockQPtr);
    
    // Send Destroy conf
    UtilDestroyLockConf* conf=(UtilDestroyLockConf*)signal->getDataPtrSend();
    conf->senderData = req.senderData;
    conf->senderRef = reference();
    conf->lockId = req.lockId;
    sendSignal(req.senderRef, GSN_UTIL_DESTROY_LOCK_CONF, signal,
	       UtilDestroyLockConf::SignalLength, JBB);
    return;
  } while(false);

  UtilDestroyLockRef * ref = (UtilDestroyLockRef*)signal->getDataPtrSend();
  ref->senderData = req.senderData;
  ref->senderRef = reference();
  ref->lockId = req.lockId;
  ref->errorCode = err;
  sendSignal(req.senderRef, GSN_UTIL_DESTROY_LOCK_REF, signal,
	     UtilDestroyLockRef::SignalLength, JBB);
}

template class ArrayPool<DbUtil::Page32>;

/*
   Copyright (c) 2003, 2018, Oracle and/or its affiliates. All rights reserved.

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
#include <signaldata/NodeFailRep.hpp>

#include <signaldata/UtilSequence.hpp>
#include <signaldata/UtilPrepare.hpp>
#include <signaldata/UtilRelease.hpp>
#include <signaldata/UtilExecute.hpp>
#include <signaldata/UtilLock.hpp>

#include <SectionReader.hpp>
#include <Interpreter.hpp>
#include <AttributeHeader.hpp>

#include <NdbTick.h>

#include <EventLogger.hpp>
extern EventLogger * g_eventLogger;

#include <signaldata/DbinfoScan.hpp>
#include <signaldata/TransIdAI.hpp>

#define JAM_FILE_ID 400


/**************************************************************************
 * ------------------------------------------------------------------------
 *  MODULE:       Startup
 * ------------------------------------------------------------------------
 * 
 *  Constructors, startup, initializations
 **************************************************************************/

DbUtil::DbUtil(Block_context& ctx) :
  SimulatedBlock(DBUTIL, ctx),
  c_runningPrepares(c_preparePool),
  c_seizingTransactions(c_transactionPool),
  c_runningTransactions(c_transactionPool),
  c_lockQueues(c_lockQueuePool)
{
  BLOCK_CONSTRUCTOR(DbUtil);
  
  // Add received signals
  addRecSignal(GSN_READ_CONFIG_REQ, &DbUtil::execREAD_CONFIG_REQ);
  addRecSignal(GSN_STTOR, &DbUtil::execSTTOR);
  addRecSignal(GSN_NDB_STTOR, &DbUtil::execNDB_STTOR);
  addRecSignal(GSN_DUMP_STATE_ORD, &DbUtil::execDUMP_STATE_ORD);
  addRecSignal(GSN_DBINFO_SCANREQ, &DbUtil::execDBINFO_SCANREQ);
  addRecSignal(GSN_CONTINUEB, &DbUtil::execCONTINUEB);
  addRecSignal(GSN_NODE_FAILREP, &DbUtil::execNODE_FAILREP);
  
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
}

DbUtil::~DbUtil()
{
}

BLOCK_FUNCTIONS(DbUtil)

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
    opPtr.p->transPtrI = RNIL;
    if (opPtr.p->prepOp != 0 && opPtr.p->prepOp_i != RNIL) {
      if (opPtr.p->prepOp->releaseFlag) {
	PreparedOperationPtr prepOpPtr;
	prepOpPtr.i = opPtr.p->prepOp_i;
	prepOpPtr.p = opPtr.p->prepOp;
	releasePreparedOperation(prepOpPtr);
      }
    }
  }
  while (transPtr.p->operations.releaseFirst());
  c_runningTransactions.release(transPtr);
}

void 
DbUtil::execREAD_CONFIG_REQ(Signal* signal)
{
  jamEntry();

  const ReadConfigReq * req = (ReadConfigReq*)signal->getDataPtr();

  Uint32 ref = req->senderRef;
  Uint32 senderData = req->senderData;

  const ndb_mgm_configuration_iterator * p = 
    m_ctx.m_config.getOwnConfigIterator();
  ndbrequire(p != 0);

  c_pagePool.setSize(10);
  c_preparePool.setSize(1);            // one parallel prepare at a time
  c_preparedOperationPool.setSize(6);  // three hardcoded, one for setval, two for test
  c_operationPool.setSize(64);         // 64 parallel operations
  c_transactionPool.setSize(32);       // 16 parallel transactions
  c_attrMappingPool.setSize(100);
  c_dataBufPool.setSize(6000);	       // 6000*11*4 = 264K > 8k+8k*16 = 256k
  {
    SLList<Prepare> tmp(c_preparePool);
    PreparePtr ptr;
    while (tmp.seizeFirst(ptr))
      new (ptr.p) Prepare(c_pagePool);
    while (tmp.releaseFirst());
  }
  {
    SLList<Operation> tmp(c_operationPool);
    OperationPtr ptr;
    while (tmp.seizeFirst(ptr))
      new (ptr.p) Operation(c_dataBufPool, c_dataBufPool, c_dataBufPool);
    while (tmp.releaseFirst());
  }
  {
    SLList<PreparedOperation> tmp(c_preparedOperationPool);
    PreparedOperationPtr ptr;
    while (tmp.seizeFirst(ptr))
      new (ptr.p) PreparedOperation(c_attrMappingPool, 
				    c_dataBufPool, c_dataBufPool);
    while (tmp.releaseFirst());
  }
  {
    SLList<Transaction> tmp(c_transactionPool);
    TransactionPtr ptr;
    while (tmp.seizeFirst(ptr))
      new (ptr.p) Transaction(c_pagePool, c_operationPool);
    while (tmp.releaseFirst());
  }

  c_lockQueuePool.setSize(5);
  c_lockElementPool.setSize(4*MAX_NDB_NODES);
  c_lockQueues.setSize(8);

  ReadConfigConf * conf = (ReadConfigConf*)signal->getDataPtrSend();
  conf->senderRef = reference();
  conf->senderData = senderData;
  sendSignal(ref, GSN_READ_CONFIG_CONF, signal, 
	     ReadConfigConf::SignalLength, JBB);
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
  
  if(startphase == 6)
  {
    jam();

    /**
     * 1) get systab_0 table-id
     * 2) run hardcodedPrepare (for sequences)
     * 3) connectTc()
     * 4) STTORRY
     */

    /**
     * We need to find table-id of SYSTAB_0, as it can be after upgrade
     *   we don't know what it will be...
     */
    get_systab_tableid(signal);

    return;
  }
  
  signal->theData[0] = 0;
  signal->theData[3] = 1;
  signal->theData[4] = 6;
  signal->theData[5] = 255;
  sendSignal(NDBCNTR_REF, GSN_STTORRY, signal, 6, JBB);

  return;
}

void
DbUtil::get_systab_tableid(Signal* signal)
{
  static char NAME[] = "sys/def/SYSTAB_0";

  GetTabInfoReq * req = (GetTabInfoReq *)signal->getDataPtrSend();
  req->senderRef = reference();
  req->senderData = RNIL;
  req->schemaTransId = 0;
  req->requestType = GetTabInfoReq::RequestByName |
    GetTabInfoReq::LongSignalConf;

  req->tableNameLen = sizeof(NAME);

  /********************************************
   * Code signal data and send signals to DICT
   ********************************************/

  Uint32 buf[(sizeof(NAME)+3)/4];
  ndbrequire(sizeof(buf) >= sizeof(NAME));
  memcpy(buf, NAME, sizeof(NAME));

  LinearSectionPtr ptr[1];
  ptr[0].p = buf;
  ptr[0].sz = sizeof(buf) / sizeof(Uint32);
  sendSignal(DBDICT_REF, GSN_GET_TABINFOREQ, signal,
             GetTabInfoReq::SignalLength, JBB, ptr,1);
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
  while (c_seizingTransactions.seizeFirst(ptr)){
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
  ptr.p->connectRef = signal->theData[2];

  c_seizingTransactions.release(ptr);

  if (c_seizingTransactions.isEmpty())
  {
    jam();
    signal->theData[0] = 0;
    signal->theData[3] = 1;
    signal->theData[4] = 6;
    signal->theData[5] = 255;
    sendSignal(NDBCNTR_REF, GSN_STTORRY, signal, 6, JBB);
  }
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
  //const Uint32 Tdata0 = signal->theData[0];

  ndbrequire(0);
}

void
DbUtil::execNODE_FAILREP(Signal* signal){
  jamEntry();
  const NodeFailRep * rep = (NodeFailRep*)signal->getDataPtr();
  NdbNodeBitmask failed; 
  failed.assign(NdbNodeBitmask::Size, rep->theNodes);

  /* Block level cleanup */
  for(unsigned i = 1; i < MAX_NDB_NODES; i++) {
    jam();
    if(failed.get(i)) {
      jam();
      Uint32 elementsCleaned = simBlockNodeFailure(signal, i); // No callback
      ndbassert(elementsCleaned == 0); // No distributed fragmented signals
      (void) elementsCleaned; // Remove compiler warning
    }//if
  }//for
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
    jam();
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
	c_preparedOperationPool.getPtr(prepOpPtr, signal->theData[2]);
	prepOpPtr.p->print();
	return;
      }

      // ** Print all records **
#if 0 // not implemented
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
#endif
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
    c_mutexMgr.lock(signal, ptr, true);
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
    Callback c = { safe_cast(&DbUtil::mutex_destroyed), ptr.i };
    ptr.p->m_callback = c;
    c_mutexMgr.destroy(signal, ptr);
    ndbout_c("c_mutexMgr.destroy ptrI=%d mutexId=%d", 
	     ptr.i, ptr.p->m_mutexId);
  }

  if (tCase == 244)
  {
    jam();
    DLHashTable<LockQueueInstance>::Iterator iter;
    Uint32 bucket = signal->theData[1];
    if (signal->getLength() == 1)
    {
      bucket = 0;
      infoEvent("Starting dumping of DbUtil::Locks");
    }
    c_lockQueues.next(bucket, iter);

    for (Uint32 i = 0; i<32 || iter.bucket == bucket; i++)
    {
      if (iter.curr.isNull())
      {
        infoEvent("Dumping of DbUtil::Locks - done");
        return;
      }
      
      infoEvent("LockQueue %u", iter.curr.p->m_lockId);
      iter.curr.p->m_queue.dump_queue(c_lockElementPool, this);
      c_lockQueues.next(iter);
    }
    signal->theData[0] = 244;
    signal->theData[1] = iter.bucket;
    sendSignal(reference(),  GSN_DUMP_STATE_ORD, signal, 2, JBB);
    return;
  }
}

void DbUtil::execDBINFO_SCANREQ(Signal *signal)
{
  DbinfoScanReq req= *(DbinfoScanReq*)signal->theData;
  const Ndbinfo::ScanCursor* cursor =
    CAST_CONSTPTR(Ndbinfo::ScanCursor, DbinfoScan::getCursorPtr(&req));
  Ndbinfo::Ratelimit rl;

  jamEntry();

  switch(req.tableId){
  case Ndbinfo::POOLS_TABLEID:
  {
    Ndbinfo::pool_entry pools[] =
    {
      { "Page",
        c_pagePool.getUsed(),
        c_pagePool.getSize(),
        c_pagePool.getEntrySize(),
        c_pagePool.getUsedHi(),
        { 0,0,0,0 }},
      { "Prepare",
        c_preparePool.getUsed(),
        c_preparePool.getSize(),
        c_preparePool.getEntrySize(),
        c_preparePool.getUsedHi(),
        { 0,0,0,0 }},
      { "Prepared Operation",
        c_preparedOperationPool.getUsed(),
        c_preparedOperationPool.getSize(),
        c_preparedOperationPool.getEntrySize(),
        c_preparedOperationPool.getUsedHi(),
        { 0,0,0,0 }},
      { "Operation",
        c_operationPool.getUsed(),
        c_operationPool.getSize(),
        c_operationPool.getEntrySize(),
        c_operationPool.getUsedHi(),
        { 0,0,0,0 }},
      { "Transaction",
        c_transactionPool.getUsed(),
        c_transactionPool.getSize(),
        c_transactionPool.getEntrySize(),
        c_transactionPool.getUsedHi(),
        { 0,0,0,0 }},
      { "Attribute Mapping",
        c_attrMappingPool.getUsed(),
        c_attrMappingPool.getSize(),
        c_attrMappingPool.getEntrySize(),
        c_attrMappingPool.getUsedHi(),
        { 0,0,0,0 }},
      { "Data Buffer",
        c_dataBufPool.getUsed(),
        c_dataBufPool.getSize(),
        c_dataBufPool.getEntrySize(),
        c_dataBufPool.getUsedHi(),
        { 0,0,0,0 }},
      { NULL, 0,0,0,0, { 0,0,0,0 }}
    };

    const size_t num_config_params =
      sizeof(pools[0].config_params) / sizeof(pools[0].config_params[0]);
    Uint32 pool = cursor->data[0];
    BlockNumber bn = blockToMain(number());
    while(pools[pool].poolname)
    {
      jam();
      Ndbinfo::Row row(signal, req);
      row.write_uint32(getOwnNodeId());
      row.write_uint32(bn);           // block number
      row.write_uint32(instance());   // block instance
      row.write_string(pools[pool].poolname);
      row.write_uint64(pools[pool].used);
      row.write_uint64(pools[pool].total);
      row.write_uint64(pools[pool].used_hi);
      row.write_uint64(pools[pool].entry_size);
      for (size_t i = 0; i < num_config_params; i++)
        row.write_uint32(pools[pool].config_params[i]);
      ndbinfo_send_row(signal, req, row, rl);
      pool++;
      if (rl.need_break(req))
      {
        jam();
        ndbinfo_send_scan_break(signal, req, rl, pool);
        return;
      }
    }
    break;
  }
  default:
    break;
  }

  ndbinfo_send_scan_conf(signal, req, rl);
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
  ndbout_c("mutex_locked - mutexId=%d, retVal=%d ptrI=%d", 
	   ptr.p->m_mutexId, retVal, ptrI);
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
			   Uint32 recipient, Uint32 senderData,
                           Uint32 errCode2){
  UtilPrepareRef * ref = (UtilPrepareRef *)signal->getDataPtrSend();
  ref->errorCode = error;
  ref->senderData = senderData;
  ref->dictErrCode = errCode2;
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
  const Uint32 schemaTransId= req->schemaTransId;

  if(signal->getNoOfSections() == 0) {
    // Missing prepare data
    jam();
    sendUtilPrepareRef(signal, UtilPrepareRef::MISSING_PROPERTIES_SECTION,
		       senderRef, senderData);
    return;
  }

  PreparePtr prepPtr;
  SegmentedSectionPtr ptr;
  SectionHandle handle(this, signal);
  
  jam();

  if(ERROR_INSERTED(19000))
  {
    jam();
    CLEAR_ERROR_INSERT_VALUE;
    g_eventLogger->info("Simulating DBUTIL prepare seize fail");
    releaseSections(handle);
    sendUtilPrepareRef(signal, UtilPrepareRef::PREPARE_SEIZE_ERROR,
		       senderRef, senderData);
    return;
  }
  if (!c_runningPrepares.seizeFirst(prepPtr))
  {
    jam();
    releaseSections(handle);
    sendUtilPrepareRef(signal, UtilPrepareRef::PREPARE_SEIZE_ERROR,
		       senderRef, senderData);
    return;
  };
  handle.getSection(ptr, UtilPrepareReq::PROPERTIES_SECTION);
  const Uint32 noPages  = (ptr.sz + sizeof(Page32)) / sizeof(Page32);
  ndbassert(noPages > 0);
  if (!prepPtr.p->preparePages.seize(noPages)) {
    jam();
    releaseSections(handle);
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
  releaseSections(handle);
  // Check table properties with DICT
  SimplePropertiesLinearReader reader(&prepPtr.p->preparePages.getPtr(0)->data[0],
                                      prepPtr.p->prepDataLen);
  prepPtr.p->clientRef = senderRef;
  prepPtr.p->clientData = senderData;
  prepPtr.p->schemaTransId = schemaTransId;
  // Read the properties
  readPrepareProps(signal, &reader, prepPtr);
}

void DbUtil::readPrepareProps(Signal* signal,
			      SimpleProperties::Reader* reader, 
			      PreparePtr prepPtr)
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
  if (tableKey == UtilPrepareReq::ScanTakeOverInd)
  {
    reader->next();
    tableKey = (UtilPrepareReq::KeyValue) reader->getKey();
  }
  if (tableKey == UtilPrepareReq::ReorgInd)
  {
    reader->next();
    tableKey = (UtilPrepareReq::KeyValue) reader->getKey();
  }

  ndbrequire((tableKey == UtilPrepareReq::TableName) ||
	     (tableKey == UtilPrepareReq::TableId));

  /************************
   * Ask Dict for metadata
   ************************/
  {
    GetTabInfoReq * req = (GetTabInfoReq *)signal->getDataPtrSend();
    req->senderRef = reference();
    req->senderData = prepPtr.i;
    req->schemaTransId = prepPtr.p->schemaTransId;
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
  
  SectionHandle handle(this, signal);
  SegmentedSectionPtr dictTabInfoPtr;
  handle.getSection(dictTabInfoPtr, GetTabInfoConf::DICT_TAB_INFO);
  ndbrequire(dictTabInfoPtr.sz == totalLen);
  
  if (prepI != RNIL)
  {
    jam();
    PreparePtr prepPtr;
    c_runningPrepares.getPtr(prepPtr, prepI);
    prepareOperation(signal, prepPtr, dictTabInfoPtr);
    releaseSections(handle);
    return;
  }
  else
  {
    jam();
    // get_systab_tableid
    releaseSections(handle);
    hardcodedPrepare(signal, conf->tableId);
    return;
  }
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
		     prepPtr.p->clientRef, prepPtr.p->clientData,
                     ref->errorCode);

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
DbUtil::prepareOperation(Signal* signal,
			 PreparePtr prepPtr,
			 SegmentedSectionPtr ptr)
{
  jam();
  
  /*******************************************
   * Seize and store PreparedOperation struct
   *******************************************/
  PreparedOperationPtr prepOpPtr;  
  if(!c_preparedOperationPool.seize(prepOpPtr)) {
    jam();
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
  prepOpPtr.p->operationType =(UtilPrepareReq::OperationTypeValue)operationType;
  
  ndbrequire(prepPagesReader.next());
  
  char tableName[MAX_TAB_NAME_SIZE];
  Uint32 tableId;
  UtilPrepareReq::KeyValue tableKey = 
    (UtilPrepareReq::KeyValue) prepPagesReader.getKey();

  bool scanTakeOver = false;
  bool reorg = false;
  if (tableKey == UtilPrepareReq::ScanTakeOverInd)
  {
    scanTakeOver = true;
    prepPagesReader.next();
    tableKey = (UtilPrepareReq::KeyValue) prepPagesReader.getKey();
  }

  if (tableKey == UtilPrepareReq::ReorgInd)
  {
    reorg = true;
    prepPagesReader.next();
    tableKey = (UtilPrepareReq::KeyValue) prepPagesReader.getKey();
  }

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
  
  if (scanTakeOver)
    prepPagesReader.next();

  if (reorg)
    prepPagesReader.next();

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
      attrIdRequested= ~0u;
    } else {
      jam();
      attrIdRequested = prepPagesReader.getUint32();
    }
    /*****************************************
     * Copy DictTabInfo into tableDesc struct
     *****************************************/
      
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
    Uint32 attrId= ~(Uint32)0;
    bool attributeFound = false;
    Uint32 noOfKeysFound = 0;     // # PK attrs found before attr in DICTdata
    Uint32 noOfNonKeysFound = 0;  // # nonPK attrs found before attr in DICTdata
    for (Uint32 i=0; i<tableDesc.NoOfAttributes; i++) {
      if (tableKey == UtilPrepareReq::TableName) {
	jam();
	ndbrequire(dictInfoReader.getKey() == DictTabInfo::AttributeName);
	ndbrequire(dictInfoReader.getValueLen() <= MAX_ATTR_NAME_SIZE);
	dictInfoReader.getString(attrName);
	attrId= ~(Uint32)0; // attrId not used
      } else { // (tableKey == UtilPrepareReq::TableId)
	jam();
	dictInfoReader.next(); // Skip name
	ndbrequire(dictInfoReader.getKey() == DictTabInfo::AttributeId);
	attrId = dictInfoReader.getUint32();
	attrName[0]= '\0'; // attrName not used
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
    AttributeHeader attrMap(attrDesc.AttributeId,    // 1. Store AttrId
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
    *(attrMappingIt.data) = attrMap.m_value;
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

      if (operationType == UtilPrepareReq::Read) {
	AttributeHeader::init(rsInfoIt.data, 
			      attrDesc.AttributeId,    // 1. Store AttrId
			      len << 2);
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
  Uint32 requestInfo = 0;
  if (scanTakeOver)
  {
    static_len ++;
    TcKeyReq::setScanIndFlag(requestInfo, 1);
  }
  if (reorg)
  {
    TcKeyReq::setReorgFlag(requestInfo, 1);
  }
  prepOpPtr.p->tckey.tableId = tableDesc.TableId;
  prepOpPtr.p->tckey.tableSchemaVersion = tableDesc.TableVersion;
  prepOpPtr.p->noOfKeyAttr = tableDesc.NoOfKeyAttr;
  prepOpPtr.p->tckeyLen = static_len;
  prepOpPtr.p->keyDataPos = static_len;  // Start of keyInfo[] in tckeyreq
  
  TcKeyReq::setAbortOption(requestInfo, TcKeyReq::AbortOnError);
  TcKeyReq::setKeyLength(requestInfo, tableDesc.KeyLength);  
  switch((UtilPrepareReq::OperationTypeValue)operationType) {
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
  case(UtilPrepareReq::Probe):
    // The number of attributes should equal the size of the primary key
    ndbrequire(tableDesc.KeyLength == attrLength);
    prepOpPtr.p->rsLen = 0;
    prepOpPtr.p->noOfAttr = tableDesc.NoOfKeyAttr;
    prepOpPtr.p->tckey.attrLen = 0;
    TcKeyReq::setOperationType(requestInfo, ZREAD);
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
DbUtil::hardcodedPrepare(Signal* signal, Uint32 SYSTAB_0)
{
  /**
   * Prepare SequenceCurrVal (READ)
   */
  Uint32 keyLen = 1;
  {
    PreparedOperationPtr ptr;
    ndbrequire(c_preparedOperationPool.seizeId(ptr, 0));
    ptr.p->tckey.attrLen = 1;
    ptr.p->rsLen = 3;
    ptr.p->tckeyLen = TcKeyReq::StaticLength + keyLen + ptr.p->tckey.attrLen;
    ptr.p->keyDataPos = TcKeyReq::StaticLength; 
    ptr.p->tckey.tableId = SYSTAB_0;
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
    AttributeHeader::init(it.data, 1, 2 << 2); // Attribute 1 - 2 data words
  }

  /**
   * Prepare SequenceNextVal (UPDATE)
   */
  {
    PreparedOperationPtr ptr;
    ndbrequire(c_preparedOperationPool.seizeId(ptr, 1));
    ptr.p->rsLen = 3;
    ptr.p->tckeyLen = TcKeyReq::StaticLength + keyLen + 5;
    ptr.p->keyDataPos = TcKeyReq::StaticLength; 
    ptr.p->tckey.attrLen = 11;
    ptr.p->tckey.tableId = SYSTAB_0;
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
      AttributeHeader::init(it.data, 1, 2 << 2); // Attribute 1 - 2 data words
    }
  }

  /**
   * Prepare CreateSequence (INSERT)
   */
  {
    PreparedOperationPtr ptr;
    ndbrequire(c_preparedOperationPool.seizeId(ptr, 2));
    ptr.p->tckey.attrLen = 5;
    ptr.p->rsLen = 0;
    ptr.p->tckeyLen = TcKeyReq::StaticLength + keyLen + ptr.p->tckey.attrLen;
    ptr.p->keyDataPos = TcKeyReq::StaticLength;
    ptr.p->tckey.tableId = SYSTAB_0;
    Uint32 requestInfo = 0;
    TcKeyReq::setAbortOption(requestInfo, TcKeyReq::CommitIfFailFree);
    TcKeyReq::setOperationType(requestInfo, ZINSERT);
    TcKeyReq::setKeyLength(requestInfo, 1);
    TcKeyReq::setAIInTcKeyReq(requestInfo, 0);
    ptr.p->tckey.requestInfo = requestInfo;
    ptr.p->tckey.tableSchemaVersion = 1;
  }

  /**
   * Prepare SetSequence (UPDATE)
   */
  {
    PreparedOperationPtr ptr;
    ndbrequire(c_preparedOperationPool.seizeId(ptr, 3));
    ptr.p->rsLen = 0;
    ptr.p->tckeyLen = TcKeyReq::StaticLength + keyLen + 5;
    ptr.p->keyDataPos = TcKeyReq::StaticLength;
    ptr.p->tckey.attrLen = 9;
    ptr.p->tckey.tableId = SYSTAB_0;
    Uint32 requestInfo = 0;
    TcKeyReq::setAbortOption(requestInfo, TcKeyReq::CommitIfFailFree);
    TcKeyReq::setOperationType(requestInfo, ZUPDATE);
    TcKeyReq::setKeyLength(requestInfo, 1);
    TcKeyReq::setAIInTcKeyReq(requestInfo, 5);
    TcKeyReq::setInterpretedFlag(requestInfo, 1);
    ptr.p->tckey.requestInfo = requestInfo;
    ptr.p->tckey.tableSchemaVersion = 1;

    Uint32 * attrInfo = &ptr.p->tckey.distrGroupHashValue;
    attrInfo[0] = 0; // IntialReadSize
    attrInfo[1] = 4; // InterpretedSize
    attrInfo[2] = 0; // FinalUpdateSize
    attrInfo[3] = 0; // FinalReadSize
    attrInfo[4] = 0; // SubroutineSize
  }

  connectTc(signal);
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
  case UtilSequenceReq::SetVal:{
    prepOp = c_preparedOperationPool.getPtr(3);
    break;
  }
  default:
    ndbrequire(false);
    prepOp = 0; // remove warning
  }
  
  /**
   * 1 Transaction with 1 operation
   */
  TransactionPtr transPtr;
  ndbrequire(c_runningTransactions.seizeFirst(transPtr));
  
  OperationPtr opPtr;
  ndbrequire(transPtr.p->operations.seizeFirst(opPtr));
  ndbrequire(opPtr.p->transPtrI == RNIL);
  ndbrequire(opPtr.p->keyInfo.seize(1));

  transPtr.p->gci_hi = 0;
  transPtr.p->gci_lo = 0;
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
    AttributeHeader::init(it.data, 0, 1 << 2);

    ndbrequire(opPtr.p->attrInfo.next(it));
    * it.data = transPtr.p->sequence.sequenceId;

    ndbrequire(opPtr.p->attrInfo.next(it));
    AttributeHeader::init(it.data, 1, 2 << 2);
    
    ndbrequire(opPtr.p->attrInfo.next(it));
    * it.data = 0;
    
    ndbrequire(opPtr.p->attrInfo.next(it));
    * it.data = 0;
  }
  
  if(req->requestType == UtilSequenceReq::SetVal)
  { // AttrInfo
    ndbrequire(opPtr.p->attrInfo.seize(4));
    AttrInfoBuffer::DataBufferIterator it;
    opPtr.p->attrInfo.first(it);
    * it.data = Interpreter::LoadConst32(7);
    ndbrequire(opPtr.p->attrInfo.next(it));
    * it.data = req->value;
    ndbrequire(opPtr.p->attrInfo.next(it));
    * it.data = Interpreter::Write(1, 7);
    ndbrequire(opPtr.p->attrInfo.next(it))
    * it.data = Interpreter::ExitOK();
  }
 
  transPtr.p->noOfRetries = 3;
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
    rs.next(it, AttributeHeader::getDataSize(it.data[0]) + 1);
    noAttr++;
  }

  if (noAttr == 0)
    return 0;

  const Uint32* dataBuffer = tmpBuf;

  // extract data
  for(rs.first(it); it.curr.i != RNIL; )
  {
    jam();
    int sz = AttributeHeader::getDataSize(it.data[0]);
    rs.next(it,1);
    for (int i = 0; i < sz; i++)
    {
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
    case UtilSequenceReq::SetVal:
      ok = true;
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
    case UtilSequenceReq::SetVal:
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
  ret->TCErrorCode = transP->errorCode;
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
  const Uint32  scanTakeOver   = req->scanTakeOver;

  if(signal->getNoOfSections() == 0) {
    // Missing prepare data
    jam();
    sendUtilExecuteRef(signal, UtilExecuteRef::MissingDataSection, 
		       0, clientRef, clientData);
    return;
  }
  /*******************************
   * Get PreparedOperation struct
   *******************************/
  PreparedOperationPtr prepOpPtr;
  c_preparedOperationPool.getPtr(prepOpPtr, prepareId);

  prepOpPtr.p->releaseFlag = releaseFlag;

  TransactionPtr  transPtr;
  OperationPtr    opPtr;
  SectionHandle handle(this, signal);
  SegmentedSectionPtr headerPtr, dataPtr;

  handle.getSection(headerPtr, UtilExecuteReq::HEADER_SECTION);
  SectionReader headerReader(headerPtr, getSectionSegmentPool());
  handle.getSection(dataPtr, UtilExecuteReq::DATA_SECTION);
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
  ndbrequire(c_runningTransactions.seizeFirst(transPtr));
  transPtr.p->gci_hi = 0;
  transPtr.p->gci_lo = 0;
  transPtr.p->gsn        = GSN_UTIL_EXECUTE_REQ;
  transPtr.p->clientRef  = clientRef;
  transPtr.p->clientData = clientData;
  ndbrequire(transPtr.p->operations.seizeFirst(opPtr));
  ndbrequire(opPtr.p->transPtrI == RNIL);
  opPtr.p->prepOp   = prepOpPtr.p;
  opPtr.p->prepOp_i = prepOpPtr.i;
  opPtr.p->m_scanTakeOver = scanTakeOver;

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

    switch (prepOpPtr.p->operationType) {
    case UtilPrepareReq::Read:
      res &= attrInfo->append(bufStart, header.getHeaderSize());
      break;
    case UtilPrepareReq::Delete:
    case UtilPrepareReq::Probe:
      // no attrinfo for Delete
      break;
    case UtilPrepareReq::Insert:
    case UtilPrepareReq::Update:
    case UtilPrepareReq::Write:
      res &= attrInfo->append(bufStart,
			      header.getHeaderSize() + header.getDataSize());
    }

    if (!res) {
      // Failed to allocate buffer data
      jam();
      releaseSections(handle);
      sendUtilExecuteRef(signal, UtilExecuteRef::AllocationError, 
			 0, clientRef, clientData);
      releaseTransaction(transPtr);    
      return;
    }
  }
  if (!dataComplete) {
    // Missing data in data section
    jam();
    releaseSections(handle);
    sendUtilExecuteRef(signal, UtilExecuteRef::MissingData, 
		       0, clientRef, clientData);
    releaseTransaction(transPtr);    
    return;
  }

#if 0
  const Uint32 l1 = prepOpPtr.p->tckey.attrLen;
  const Uint32 l2 = 
    prepOpPtr.p->attrInfo.getSize() + opPtr.p->attrInfo.getSize();

  if (TcKeyReq::getOperationType(prepOpPtr.p->tckey.requestInfo) != ZREAD){
    ndbrequire(l1 == l2);
  } else {
    ndbout_c("TcKeyReq::Read");
  }
#endif

  releaseSections(handle);
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
  op->rsRecv = 0;
#if 0 //def EVENT_DEBUG
  printf("pop->rsLen %u\n", pop->rsLen);
#endif
  op->rsExpect = 0;
  op->transPtrI = transPtr.i;
  
  TcKeyReq * tcKey = (TcKeyReq*)signal->getDataPtrSend();
  //ndbout << "*** 6 ***"<< endl; pop->print();
  memcpy(tcKey, &pop->tckey, 4*pop->tckeyLen);
  //ndbout << "*** 6b ***"<< endl; 
  //printTCKEYREQ(stdout, signal->getDataPtrSend(), 
  //              pop->tckeyLenInBytes >> 2, 0);
  tcKey->apiConnectPtr = transPtr.p->connectPtr;
  tcKey->senderData = opI;
  tcKey->transId1 = transPtr.p->transId[0];
  tcKey->transId2 = transPtr.p->transId[1];
  tcKey->requestInfo |= start;

  if (TcKeyReq::getScanIndFlag(tcKey->requestInfo))
  {
    tcKey->scanInfo = op->m_scanTakeOver;
  }
  
#if 0 //def EVENT_DEBUG
  // Debugging
  printf("DbUtil::runOperation: KEYINFO\n");
  op->keyInfo.print(stdout);
  printf("DbUtil::runOperation: ATTRINFO\n");
  op->attrInfo.print(stdout);
#endif
  
  Uint32 attrLen = pop->attrInfo.getSize() + op->attrInfo.getSize();
  Uint32 keyLen = op->keyInfo.getSize();
  tcKey->attrLen = attrLen + TcKeyReq::getAIInTcKeyReq(tcKey->requestInfo);
  TcKeyReq::setKeyLength(tcKey->requestInfo, keyLen);
  
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
  Uint32 sigLen = pop->tckeyLen + (keyLen > 8 ? 8 : keyLen);
  sendSignal(transPtr.p->connectRef, GSN_TCKEYREQ, signal, sigLen, JBB);
  
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
  sendKeyInfo(signal, transPtr.p->connectRef,
              keyInfo, op->keyInfo, kit);

  /**
   * AttrInfo
   */
  AttrInfo* attrInfo = (AttrInfo *)signal->getDataPtrSend();
  attrInfo->connectPtr = transPtr.p->connectPtr;
  attrInfo->transId[0] = transPtr.p->transId[0];
  attrInfo->transId[1] = transPtr.p->transId[1];

  AttrInfoIterator ait;
  pop->attrInfo.first(ait);
  sendAttrInfo(signal, transPtr.p->connectRef,
               attrInfo, pop->attrInfo, ait);
  
  op->attrInfo.first(ait);
  sendAttrInfo(signal, transPtr.p->connectRef,
               attrInfo, op->attrInfo, ait);
}

void
DbUtil::sendKeyInfo(Signal* signal, 
                    Uint32 tcRef,
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
    sendSignal(tcRef, GSN_KEYINFO, signal,
	       KeyInfo::HeaderLength + keyDataLen, JBB);
  }
}

void
DbUtil::sendAttrInfo(Signal* signal, 
                     Uint32 tcRef,
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
    sendSignal(tcRef, GSN_ATTRINFO, signal,
	       AttrInfo::HeaderLength + i, JBB);
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
  SectionHandle handle(this, signal);
  SegmentedSectionPtr dataPtr;
  bool longSignal = (handle.m_cnt == 1);
  Uint32 dataLen;

  if (longSignal)
  {
    ndbrequire(handle.getSection(dataPtr, 0));
    dataLen = dataPtr.sz;
  }
  else
  {
    dataLen = signal->length() - 3;
  }

  bool validSignal = false;
  Operation * opP;
  TransactionPtr transPtr;
  do
  {
    /* Lookup op record carefully, it may have been released if the
     * transaction was aborted and the TRANSID_AI was delayed
     */
    OperationPtr opPtr;
    opPtr.i = opI;
    c_operationPool.getPtrIgnoreAlloc(opPtr);
    opP = opPtr.p;
    
    /* Use transPtrI == RNIL as test of op record validity */
    if (opP->transPtrI == RNIL)
    {
      jam();
      break;
    }
    
#ifdef ARRAY_GUARD
    /* Op was valid, do normal debug-only allocation double-check */
    ndbrequire(c_operationPool.isSeized(opI));
#endif

    /* Valid op record must always point to allocated transaction record */
    c_runningTransactions.getPtr(transPtr, opP->transPtrI);

    /* Transaction may have different transid since this op was
     * executed - e.g. if it is retried due to a temp error.
     */
    validSignal = (transId1 == transPtr.p->transId[0] && 
                   transId2 == transPtr.p->transId[1]);
  } while(0);
  
  if (unlikely(!validSignal))
  {
    /* Can get strays as TRANSID_AI takes a different path
     * to LQHKEYCONF/TCKEYCONF/LQHKEYREF/TCKEYREF/TCROLLBACKREP
     * and we may have retried (with different transid), or
     * given up since then
     */
    jam();
    releaseSections(handle);
    return;
  }

  jam();

  opP->rsRecv += dataLen;
  
  /**
   * Save result
   */
  if (longSignal)
  {
    SectionSegment * ptrP = dataPtr.p;
    while (dataLen > NDB_SECTION_SEGMENT_SZ)
    {
      ndbrequire(opP->rs.append(ptrP->theData, NDB_SECTION_SEGMENT_SZ));
      dataLen -= NDB_SECTION_SEGMENT_SZ;
      ptrP = g_sectionSegmentPool.getPtr(ptrP->m_nextSegment);
    }
    ndbrequire(opP->rs.append(ptrP->theData, dataLen));

    releaseSections(handle);
  }
  else
  {
    const Uint32 *src = &signal->theData[3];
    ndbrequire(opP->rs.append(src, dataLen));
  }

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

  Uint32 gci_lo = 0;
  const Uint32 gci_hi   = keyConf->gci_hi;
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

  if (TcKeyConf::getCommitFlag(confInfo))
  {
    jam();
    gci_lo = keyConf->operations[ops].apiOperationPtr;
  }

  TransactionPtr transPtr;
  c_runningTransactions.getPtr(transPtr, transI);

  /**
   * Check commit ack marker flag
   */
  if (TcKeyConf::getMarkerFlag(confInfo))
  {
    jam();
    signal->theData[0] = transId1;
    signal->theData[1] = transId2;
    sendSignal(transPtr.p->connectRef, GSN_TC_COMMIT_ACK, signal, 2, JBB);    
  }//if

  ndbrequire(transId1 == transPtr.p->transId[0] && 
	     transId2 == transPtr.p->transId[1]);

  if (TcKeyConf::getCommitFlag(confInfo))
  {
    jam();
    transPtr.p->gci_hi = gci_hi;
    transPtr.p->gci_lo = gci_lo;
  }

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
      ret->gci_hi = transPtr.p->gci_hi;
      ret->gci_lo = transPtr.p->gci_lo;
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

  UtilLockReq req = *(UtilLockReq*)signal->getDataPtr();

  LockQueuePtr lockQPtr;
  if(!c_lockQueues.find(lockQPtr, req.lockId))
  {
    jam();
    sendLOCK_REF(signal, &req, UtilLockRef::NoSuchLock);
    return;
  }

  const Uint32 senderNode = refToNode(req.senderRef);
  if(senderNode != getOwnNodeId() && senderNode != 0)
  {
    jam();
    sendLOCK_REF(signal, &req, UtilLockRef::DistributedLockNotSupported);    
    return;
  }
  
  Uint32 res = lockQPtr.p->m_queue.lock(this, c_lockElementPool, &req);
  switch(res){
  case UtilLockRef::OK:
    jam();
    sendLOCK_CONF(signal, &req);
    return;
  case UtilLockRef::OutOfLockRecords:
    jam();
    sendLOCK_REF(signal, &req, UtilLockRef::OutOfLockRecords);
    return;
  case UtilLockRef::InLockQueue:
    jam();
    if (req.requestInfo & UtilLockReq::Notify)
    {
      jam();
      sendLOCK_REF(signal, &req, UtilLockRef::InLockQueue);
    }
    return;
  case UtilLockRef::LockAlreadyHeld:
    jam();
    ndbassert(req.requestInfo & UtilLockReq::TryLock);
    sendLOCK_REF(signal, &req, UtilLockRef::LockAlreadyHeld);
    return;
  default:
    jam();
    ndbassert(false);
    sendLOCK_REF(signal, &req, (UtilLockRef::ErrorCode)res);
    return;
  }
}

void
DbUtil::execUTIL_UNLOCK_REQ(Signal* signal)
{
  jamEntry();
  
  UtilUnlockReq req = *(UtilUnlockReq*)signal->getDataPtr();
  
  LockQueuePtr lockQPtr;
  if(!c_lockQueues.find(lockQPtr, req.lockId))
  {
    jam();
    sendUNLOCK_REF(signal, &req, UtilUnlockRef::NoSuchLock);
    return;
  }

  Uint32 res = lockQPtr.p->m_queue.unlock(this, c_lockElementPool, &req);
  switch(res){
  case UtilUnlockRef::OK:
    jam();
  case UtilUnlockRef::NotLockOwner: {
    jam();
    UtilUnlockConf * conf = (UtilUnlockConf*)signal->getDataPtrSend();
    conf->senderData = req.senderData;
    conf->senderRef = reference();
    conf->lockId = req.lockId;
    sendSignal(req.senderRef, GSN_UTIL_UNLOCK_CONF, signal,
               UtilUnlockConf::SignalLength, JBB);
    break;
  }
  case UtilUnlockRef::NotInLockQueue:
    jam();
  default:
    jam();
    ndbassert(false);
    sendUNLOCK_REF(signal, &req, (UtilUnlockRef::ErrorCode)res);
    break;
  }
  
  /**
   * Unlock can make other(s) acquie lock
   */
  UtilLockReq lockReq;
  LockQueue::Iterator iter;
  if (lockQPtr.p->m_queue.first(this, c_lockElementPool, iter))
  {
    int res;
    while ((res = lockQPtr.p->m_queue.checkLockGrant(iter, &lockReq)) > 0)
    {
      jam();
      /**
       *
       */
      if (res == 2)
      {
        jam();
        sendLOCK_CONF(signal, &lockReq);
      }        
      
      if (!lockQPtr.p->m_queue.next(iter))
        break;
    }
  }
}

void
DbUtil::sendLOCK_REF(Signal* signal, 
                     const UtilLockReq * req, UtilLockRef::ErrorCode err)
{
  const Uint32 senderData = req->senderData;
  const Uint32 senderRef = req->senderRef;
  const Uint32 lockId = req->lockId;
  const Uint32 extra = req->extra;

  UtilLockRef * ref = (UtilLockRef*)signal->getDataPtrSend();
  ref->senderData = senderData;
  ref->senderRef = reference();
  ref->lockId = lockId;
  ref->errorCode = err;
  ref->extra = extra;
  sendSignal(senderRef, GSN_UTIL_LOCK_REF, signal, 
	     UtilLockRef::SignalLength, JBB);
}

void
DbUtil::sendLOCK_CONF(Signal* signal, const UtilLockReq * req)
{
  const Uint32 senderData = req->senderData;
  const Uint32 senderRef = req->senderRef;
  const Uint32 lockId = req->lockId;
  const Uint32 extra = req->extra;

  UtilLockConf * conf = (UtilLockConf*)signal->getDataPtrSend();
  conf->senderData = senderData;
  conf->senderRef = reference();
  conf->lockId = lockId;
  conf->extra = extra;
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

    new (lockQPtr.p) LockQueueInstance(req.lockId);
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
    if(!c_lockQueues.find(lockQPtr, req.lockId))
    {
      jam();
      err = UtilDestroyLockRef::NoSuchLock;
      break;
    }
    
    LockQueue::Iterator iter;
    if (lockQPtr.p->m_queue.first(this, c_lockElementPool, iter) == false)
    {
      jam();
      err = UtilDestroyLockRef::NotLockOwner;
      break;
    }
    
    if (! (iter.m_curr.p->m_req.senderData == req.senderData &&
           iter.m_curr.p->m_req.senderRef == req.senderRef &&
           (! (iter.m_curr.p->m_req.requestInfo & UtilLockReq::SharedLock)) &&
           iter.m_curr.p->m_req.requestInfo & UtilLockReq::Granted))
    {
      jam();
      err = UtilDestroyLockRef::NotLockOwner;
      break;
    }
    
    /**
     * OK
     */

    while (lockQPtr.p->m_queue.next(iter))
    {
      jam();
      sendLOCK_REF(signal, &iter.m_curr.p->m_req, UtilLockRef::NoSuchLock);
    }

    lockQPtr.p->m_queue.clear(c_lockElementPool);
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

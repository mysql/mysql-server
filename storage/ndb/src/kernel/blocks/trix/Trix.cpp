/*
   Copyright (c) 2003, 2018, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "Trix.hpp"

#include <cstring>
#include <string.h>
#include <kernel_types.h>
#include <NdbOut.hpp>

#include <signaldata/ReadNodesConf.hpp>
#include <signaldata/NodeFailRep.hpp>
#include <signaldata/DumpStateOrd.hpp>
#include <signaldata/GetTabInfo.hpp>
#include <signaldata/DictTabInfo.hpp>
#include <signaldata/CopyData.hpp>
#include <signaldata/BuildIndxImpl.hpp>
#include <signaldata/BuildFKImpl.hpp>
#include <signaldata/SumaImpl.hpp>
#include <signaldata/UtilPrepare.hpp>
#include <signaldata/UtilExecute.hpp>
#include <signaldata/UtilRelease.hpp>
#include <SectionReader.hpp>
#include <AttributeHeader.hpp>
#include <signaldata/TcKeyReq.hpp>

#include <signaldata/DbinfoScan.hpp>
#include <signaldata/TransIdAI.hpp>
#include <signaldata/WaitGCP.hpp>

#define JAM_FILE_ID 433


#define CONSTRAINT_VIOLATION 893
#define TUPLE_NOT_FOUND 626
#define FK_NO_PARENT_ROW_EXISTS 21033

static
bool
check_timeout(Uint32 errCode)
{
  switch(errCode){
  case 266:
    return true;
  }
  return false;
}

#define DEBUG(x) { ndbout << "TRIX::" << x << endl; }

/**
 *
 */
Trix::Trix(Block_context& ctx) :
  SimulatedBlock(TRIX, ctx),
  c_theNodes(c_theNodeRecPool),
  c_masterNodeId(0),
  c_masterTrixRef(0),
  c_noNodesFailed(0),
  c_noActiveNodes(0),
  c_theSubscriptions(c_theSubscriptionRecPool)
{
  BLOCK_CONSTRUCTOR(Trix);

  // Add received signals
  addRecSignal(GSN_READ_CONFIG_REQ,  &Trix::execREAD_CONFIG_REQ);
  addRecSignal(GSN_STTOR,  &Trix::execSTTOR);
  addRecSignal(GSN_NDB_STTOR,  &Trix::execNDB_STTOR); // Forwarded from DICT
  addRecSignal(GSN_READ_NODESCONF, &Trix::execREAD_NODESCONF);
  addRecSignal(GSN_READ_NODESREF, &Trix::execREAD_NODESREF);
  addRecSignal(GSN_NODE_FAILREP, &Trix::execNODE_FAILREP);
  addRecSignal(GSN_INCL_NODEREQ, &Trix::execINCL_NODEREQ);
  addRecSignal(GSN_DUMP_STATE_ORD, &Trix::execDUMP_STATE_ORD);
  addRecSignal(GSN_DBINFO_SCANREQ, &Trix::execDBINFO_SCANREQ);

  // Index build
  addRecSignal(GSN_BUILD_INDX_IMPL_REQ, &Trix::execBUILD_INDX_IMPL_REQ);
  // Dump testing
  addRecSignal(GSN_BUILD_INDX_IMPL_CONF, &Trix::execBUILD_INDX_IMPL_CONF);
  addRecSignal(GSN_BUILD_INDX_IMPL_REF, &Trix::execBUILD_INDX_IMPL_REF);

  addRecSignal(GSN_COPY_DATA_IMPL_REQ, &Trix::execCOPY_DATA_IMPL_REQ);
  addRecSignal(GSN_BUILD_FK_IMPL_REQ, &Trix::execBUILD_FK_IMPL_REQ);

  addRecSignal(GSN_UTIL_PREPARE_CONF, &Trix::execUTIL_PREPARE_CONF);
  addRecSignal(GSN_UTIL_PREPARE_REF, &Trix::execUTIL_PREPARE_REF);
  addRecSignal(GSN_UTIL_EXECUTE_CONF, &Trix::execUTIL_EXECUTE_CONF);
  addRecSignal(GSN_UTIL_EXECUTE_REF, &Trix::execUTIL_EXECUTE_REF);
  addRecSignal(GSN_UTIL_RELEASE_CONF, &Trix::execUTIL_RELEASE_CONF);
  addRecSignal(GSN_UTIL_RELEASE_REF, &Trix::execUTIL_RELEASE_REF);


  // Suma signals
  addRecSignal(GSN_SUB_CREATE_CONF, &Trix::execSUB_CREATE_CONF);
  addRecSignal(GSN_SUB_CREATE_REF, &Trix::execSUB_CREATE_REF);
  addRecSignal(GSN_SUB_REMOVE_CONF, &Trix::execSUB_REMOVE_CONF);
  addRecSignal(GSN_SUB_REMOVE_REF, &Trix::execSUB_REMOVE_REF);
  addRecSignal(GSN_SUB_SYNC_CONF, &Trix::execSUB_SYNC_CONF);
  addRecSignal(GSN_SUB_SYNC_REF, &Trix::execSUB_SYNC_REF);
  addRecSignal(GSN_SUB_SYNC_CONTINUE_REQ, &Trix::execSUB_SYNC_CONTINUE_REQ);
  addRecSignal(GSN_SUB_TABLE_DATA, &Trix::execSUB_TABLE_DATA);

  addRecSignal(GSN_WAIT_GCP_REF, &Trix::execWAIT_GCP_REF);
  addRecSignal(GSN_WAIT_GCP_CONF, &Trix::execWAIT_GCP_CONF);

  // index stats
  addRecSignal(GSN_INDEX_STAT_IMPL_REQ, &Trix::execINDEX_STAT_IMPL_REQ);
  addRecSignal(GSN_GET_TABINFO_CONF, &Trix::execGET_TABINFO_CONF);
  addRecSignal(GSN_GET_TABINFOREF, &Trix::execGET_TABINFO_REF);

  // index stats sys tables
  c_statGetMetaDone = false;
}

/**
 *
 */
Trix::~Trix()
{
}

void 
Trix::execREAD_CONFIG_REQ(Signal* signal)
{
  jamEntry();

  const ReadConfigReq * req = (ReadConfigReq*)signal->getDataPtr();

  Uint32 ref = req->senderRef;
  Uint32 senderData = req->senderData;

  const ndb_mgm_configuration_iterator * p = 
    m_ctx.m_config.getOwnConfigIterator();
  ndbrequire(p != 0);

  c_maxUIBuildBatchSize = 64;
  ndb_mgm_get_int_parameter(p, CFG_DB_UI_BUILD_MAX_BATCHSIZE,
                            &c_maxUIBuildBatchSize);

  c_maxFKBuildBatchSize = 64;
  ndb_mgm_get_int_parameter(p, CFG_DB_FK_BUILD_MAX_BATCHSIZE,
                            &c_maxFKBuildBatchSize);

  c_maxReorgBuildBatchSize = 64;
  ndb_mgm_get_int_parameter(p, CFG_DB_REORG_BUILD_MAX_BATCHSIZE,
                            &c_maxReorgBuildBatchSize);

  // Allocate pool sizes
  c_theAttrOrderBufferPool.setSize(100);
  c_theSubscriptionRecPool.setSize(100);
  c_statOpPool.setSize(5);

  SubscriptionRecord_list subscriptions(c_theSubscriptionRecPool);
  SubscriptionRecPtr subptr;
  while (subscriptions.seizeFirst(subptr) == true) {
    new (subptr.p) SubscriptionRecord(c_theAttrOrderBufferPool);
  }
  while (subscriptions.releaseFirst());

  ReadConfigConf * conf = (ReadConfigConf*)signal->getDataPtrSend();
  conf->senderRef = reference();
  conf->senderData = senderData;
  sendSignal(ref, GSN_READ_CONFIG_CONF, signal, 
	     ReadConfigConf::SignalLength, JBB);
}

/**
 *
 */
void Trix::execSTTOR(Signal* signal) 
{
  jamEntry();                            

  //const Uint32 startphase   = signal->theData[1];
  const Uint32 theSignalKey = signal->theData[6];
  
  signal->theData[0] = theSignalKey;
  signal->theData[3] = 1;
  signal->theData[4] = 255; // No more start phases from missra
  sendSignal(NDBCNTR_REF, GSN_STTORRY, signal, 5, JBB);
  return;
}//Trix::execSTTOR()

/**
 *
 */
void Trix::execNDB_STTOR(Signal* signal) 
{
  jamEntry();                            
  BlockReference ndbcntrRef = signal->theData[0];	 
  Uint16 startphase = signal->theData[2];      /* RESTART PHASE           */
  Uint16 mynode = signal->theData[1];		 
  //Uint16 restarttype = signal->theData[3];	 
  //UintR configInfo1 = signal->theData[6];     /* CONFIGRATION INFO PART 1 */
  //UintR configInfo2 = signal->theData[7];     /* CONFIGRATION INFO PART 2 */
  switch (startphase) {
  case 3:
    jam();
    /* SYMBOLIC START PHASE 4             */
    /* ABSOLUTE PHASE 5                   */
    /* REQUEST NODE IDENTITIES FROM DBDIH */
    signal->theData[0] = calcTrixBlockRef(mynode);
    sendSignal(ndbcntrRef, GSN_READ_NODESREQ, signal, 1, JBB);
    return;
    break;
  case 6:
    break;
  default:
    break;
  }
}

/**
 *
 */
void Trix::execREAD_NODESCONF(Signal* signal)
{
  jamEntry();

  ReadNodesConf * const  readNodes = (ReadNodesConf *)signal->getDataPtr();
  //Uint32 noOfNodes   = readNodes->noOfNodes;
  NodeRecPtr nodeRecPtr;

  c_masterNodeId = readNodes->masterNodeId;
  c_masterTrixRef = RNIL;
  c_noNodesFailed = 0;

  for(unsigned i = 0; i < MAX_NDB_NODES; i++) {
    jam();
    if(NdbNodeBitmask::get(readNodes->allNodes, i)) {
      // Node is defined
      jam();
      ndbrequire(c_theNodes.getPool().seizeId(nodeRecPtr, i));
      c_theNodes.addFirst(nodeRecPtr);
      nodeRecPtr.p->trixRef = calcTrixBlockRef(i);
      if (i == c_masterNodeId) {
        c_masterTrixRef = nodeRecPtr.p->trixRef;
      }
      if(NdbNodeBitmask::get(readNodes->inactiveNodes, i)){
        // Node is not active
	jam();
	/**-----------------------------------------------------------------
	 * THIS NODE IS DEFINED IN THE CLUSTER BUT IS NOT ALIVE CURRENTLY.
	 * WE ADD THE NODE TO THE SET OF FAILED NODES AND ALSO SET THE
	 * BLOCKSTATE TO BUSY TO AVOID ADDING TRIGGERS OR INDEXES WHILE 
	 * NOT ALL NODES ARE ALIVE.
	 *------------------------------------------------------------------*/
	arrGuard(c_noNodesFailed, MAX_NDB_NODES);
	nodeRecPtr.p->alive = false;
	c_noNodesFailed++;
	c_blockState = Trix::NODE_FAILURE;
      }
      else {
        // Node is active
        jam();
        c_noActiveNodes++;
        nodeRecPtr.p->alive = true;
      }
    }
  }
  if (c_noNodesFailed == 0) {
    c_blockState = Trix::STARTED;
  }
}

/**
 *
 */
void Trix::execREAD_NODESREF(Signal* signal)
{
  // NYI
}

/**
 *
 */
void Trix::execNODE_FAILREP(Signal* signal)
{
  jamEntry();
  NodeFailRep * const  nodeFail = (NodeFailRep *) signal->getDataPtr();

  //Uint32 failureNr    = nodeFail->failNo;
  //Uint32 numberNodes  = nodeFail->noOfNodes;
  Uint32 masterNodeId = nodeFail->masterNodeId;

  NodeRecPtr nodeRecPtr;

  for(c_theNodes.first(nodeRecPtr); 
      nodeRecPtr.i != RNIL; 
      c_theNodes.next(nodeRecPtr)) {
    if(NdbNodeBitmask::get(nodeFail->theNodes, nodeRecPtr.i)) {
      nodeRecPtr.p->alive = false;
      c_noNodesFailed++;
      c_noActiveNodes--;      
    }
  }
  if (c_masterNodeId != masterNodeId) {
    c_masterNodeId = masterNodeId;
    NodeRecord* nodeRec = c_theNodes.getPtr(masterNodeId);
    c_masterTrixRef = nodeRec->trixRef;
  }
}

/**
 *
 */
void Trix::execINCL_NODEREQ(Signal* signal)
{
  jamEntry();
  UintR node_id = signal->theData[1];
  NodeRecord* nodeRec = c_theNodes.getPtr(node_id);
  nodeRec->alive = true;
  c_noNodesFailed--;
  c_noActiveNodes++;      
  nodeRec->trixRef = calcTrixBlockRef(node_id);
  if (c_noNodesFailed == 0) {
    c_blockState = Trix::STARTED;
  }  
}

// Debugging
void
Trix::execDUMP_STATE_ORD(Signal* signal)
{
  jamEntry();

  DumpStateOrd * dumpStateOrd = (DumpStateOrd *)signal->getDataPtr();

  switch(dumpStateOrd->args[0]) {
  case(300): {// ok
    // index2 -T; index2 -I -n10000; index2 -c
    // all dump 300 0 0 0 0 0 4 2
    // select_count INDEX0000
    std::memmove(signal->theData,
                 signal->theData + 1,
                 BuildIndxImplReq::SignalLength * sizeof(signal->theData[0]));
    BuildIndxImplReq * buildIndxReq = (BuildIndxImplReq *)signal->getDataPtrSend();
    
    buildIndxReq->senderRef = reference(); // return to me
    buildIndxReq->parallelism = 10;
    Uint32 indexColumns[1] = {1};
    Uint32 keyColumns[1] = {0};
    struct LinearSectionPtr ls_ptr[3];
    ls_ptr[0].p = indexColumns;
    ls_ptr[0].sz = 1;
    ls_ptr[1].p = keyColumns;
    ls_ptr[1].sz = 1;
    sendSignal(reference(),
	       GSN_BUILD_INDX_IMPL_REQ,
	       signal,
	       BuildIndxImplReq::SignalLength,
	       JBB, ls_ptr, 2);
    break;
  }
  case(301): { // ok
    // index2 -T; index2 -I -n10000; index2 -c -p
    // all dump 301 0 0 0 0 0 4 2
    // select_count INDEX0000
    std::memmove(signal->theData,
                 signal->theData + 1,
                 BuildIndxImplReq::SignalLength * sizeof(signal->theData[0]));
    BuildIndxImplReq * buildIndxReq = (BuildIndxImplReq *)signal->getDataPtrSend();
    
    buildIndxReq->senderRef = reference(); // return to me
    buildIndxReq->parallelism = 10;
    Uint32 indexColumns[2] = {0, 1};
    Uint32 keyColumns[1] = {0};
    struct LinearSectionPtr ls_ptr[3];
    ls_ptr[0].p = indexColumns;
    ls_ptr[0].sz = 2;
    ls_ptr[1].p = keyColumns;
    ls_ptr[1].sz = 1;
    sendSignal(reference(),
	       GSN_BUILD_INDX_IMPL_REQ,
	       signal,
	       BuildIndxImplReq::SignalLength,
	       JBB, ls_ptr, 2);
    break;
  }
  case(302): { // ok
    // index -T; index -I -n1000; index -c -p
    // all dump 302 0 0 0 0 0 4 2
    // select_count PNUMINDEX0000
    std::memmove(signal->theData,
                 signal->theData + 1,
                 BuildIndxImplReq::SignalLength * sizeof(signal->theData[0]));
    BuildIndxImplReq * buildIndxReq = (BuildIndxImplReq *)signal->getDataPtrSend();
    
    buildIndxReq->senderRef = reference(); // return to me
    buildIndxReq->parallelism = 10;
    Uint32 indexColumns[3] = {0, 3, 5};
    Uint32 keyColumns[1] = {0};
    struct LinearSectionPtr ls_ptr[3];
    ls_ptr[0].p = indexColumns;
    ls_ptr[0].sz = 3;
    ls_ptr[1].p = keyColumns;
    ls_ptr[1].sz = 1;
    sendSignal(reference(),
	       GSN_BUILD_INDX_IMPL_REQ,
	       signal,
	       BuildIndxImplReq::SignalLength,
	       JBB, ls_ptr, 2);
    break;
  }
  case(303): { // ok
    // index -T -2; index -I -2 -n1000; index -c -p
    // all dump 303 0 0 0 0 0 4 2
    // select_count PNUMINDEX0000
    std::memmove(signal->theData,
                 signal->theData + 1,
                 BuildIndxImplReq::SignalLength * sizeof(signal->theData[0]));
    BuildIndxImplReq * buildIndxReq = (BuildIndxImplReq *)signal->getDataPtrSend();
    
    buildIndxReq->senderRef = reference(); // return to me
    buildIndxReq->parallelism = 10;
    Uint32 indexColumns[3] = {0, 3, 5};
    Uint32 keyColumns[2] = {0, 1};
    struct LinearSectionPtr ls_ptr[3];
    ls_ptr[0].p = indexColumns;
    ls_ptr[0].sz = 3;
    ls_ptr[1].p = keyColumns;
    ls_ptr[1].sz = 2;
    sendSignal(reference(),
	       GSN_BUILD_INDX_IMPL_REQ,
	       signal,
	       BuildIndxImplReq::SignalLength,
	       JBB, ls_ptr, 2);
    break;
  }
  case(304): { // ok
    // index -T -L; index -I -L -n1000; index -c -p
    // all dump 304 0 0 0 0 0 4 2
    // select_count PNUMINDEX0000
    std::memmove(signal->theData,
                 signal->theData + 1,
                 BuildIndxImplReq::SignalLength * sizeof(signal->theData[0]));
    BuildIndxImplReq * buildIndxReq = (BuildIndxImplReq *)signal->getDataPtrSend();
    
    buildIndxReq->senderRef = reference(); // return to me
    buildIndxReq->parallelism = 10;
    Uint32 indexColumns[3] = {0, 3, 5};
    Uint32 keyColumns[1] = {0};
    struct LinearSectionPtr ls_ptr[3];
    ls_ptr[0].p = indexColumns;
    ls_ptr[0].sz = 3;
    ls_ptr[1].p = keyColumns;
    ls_ptr[1].sz = 1;
    sendSignal(reference(),
	       GSN_BUILD_INDX_IMPL_REQ,
	       signal,
	       BuildIndxImplReq::SignalLength,
	       JBB, ls_ptr, 2);
    break;
  }
  case(305): { // ok
    // index -T -2 -L; index -I -2 -L -n1000; index -c -p
    // all dump 305 0 0 0 0 0 4 2
    // select_count PNUMINDEX0000
    std::memmove(signal->theData,
                 signal->theData + 1,
                 BuildIndxImplReq::SignalLength * sizeof(signal->theData[0]));
    BuildIndxImplReq * buildIndxReq = (BuildIndxImplReq *)signal->getDataPtrSend();
    
    buildIndxReq->senderRef = reference(); // return to me
    buildIndxReq->parallelism = 10;
    Uint32 indexColumns[3] = {0, 3, 5};
    Uint32 keyColumns[2] = {0, 1};
    struct LinearSectionPtr ls_ptr[3];
    ls_ptr[0].p = indexColumns;
    ls_ptr[0].sz = 3;
    ls_ptr[1].p = keyColumns;
    ls_ptr[1].sz = 2;
    sendSignal(reference(),
	       GSN_BUILD_INDX_IMPL_REQ,
	       signal,
	       BuildIndxImplReq::SignalLength,
	       JBB, ls_ptr, 2);
    break;
  }
  default: {
    // Ignore
  }
  }

  if (signal->theData[0] == DumpStateOrd::SchemaResourceSnapshot)
  {
    RSS_AP_SNAPSHOT_SAVE(c_theSubscriptionRecPool);
    RSS_AP_SNAPSHOT_SAVE(c_statOpPool);
    return;
  }

  if (signal->theData[0] == DumpStateOrd::SchemaResourceCheckLeak)
  {
    RSS_AP_SNAPSHOT_CHECK(c_theSubscriptionRecPool);
    RSS_AP_SNAPSHOT_CHECK(c_statOpPool);
    return;
  }
  
  if (signal->theData[0] == 8004)
  {
    infoEvent("TRIX: c_theSubscriptionRecPool size: %u free: %u",
              c_theSubscriptionRecPool.getSize(),
              c_theSubscriptionRecPool.getNoOfFree());
    return;
  }

}

void Trix::execDBINFO_SCANREQ(Signal *signal)
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
      { "Attribute Order Buffer",
        c_theAttrOrderBufferPool.getUsed(),
        c_theAttrOrderBufferPool.getSize(),
        c_theAttrOrderBufferPool.getEntrySize(),
        c_theAttrOrderBufferPool.getUsedHi(),
        { 0,0,0,0 },
        0},
      { "Subscription Record",
        c_theSubscriptionRecPool.getUsed(),
        c_theSubscriptionRecPool.getSize(),
        c_theSubscriptionRecPool.getEntrySize(),
        c_theSubscriptionRecPool.getUsedHi(),
        { 0,0,0,0 },
        0},
      { NULL, 0,0,0,0,{0,0,0,0},0}
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
      row.write_uint32(GET_RG(pools[pool].record_type));
      row.write_uint32(GET_TID(pools[pool].record_type));
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

// Build index
void Trix:: execBUILD_INDX_IMPL_REQ(Signal* signal)
{
  jamEntry();
  DBUG_ENTER("Trix:: execBUILD_INDX_IMPL_REQ");

  const BuildIndxImplReq
    buildIndxReqData = *(const BuildIndxImplReq*)signal->getDataPtr(),
    *buildIndxReq = &buildIndxReqData;

  // Seize a subscription record
  SubscriptionRecPtr subRecPtr;
  SubscriptionRecord* subRec;
  SectionHandle handle(this, signal);

  if (ERROR_INSERTED_CLEAR(18000))
  {
    sendSignalWithDelay(reference(), GSN_BUILD_INDX_IMPL_REQ, signal, 1000,
                        signal->getLength(), &handle);
    DBUG_VOID_RETURN;
  }

  if (!c_theSubscriptions.getPool().seizeId(subRecPtr, buildIndxReq->buildId)) {
    jam();
    // Failed to allocate subscription record
    BuildIndxRef* buildIndxRef = (BuildIndxRef*)signal->getDataPtrSend();

    buildIndxRef->errorCode = BuildIndxRef::AllocationFailure;
    releaseSections(handle);
    sendSignal(buildIndxReq->senderRef, GSN_BUILD_INDX_IMPL_REF, signal,
               BuildIndxRef::SignalLength, JBB);
    DBUG_VOID_RETURN;
  }
  c_theSubscriptions.addFirst(subRecPtr);

  subRec = subRecPtr.p;
  subRec->errorCode = BuildIndxRef::NoError;
  subRec->userReference = buildIndxReq->senderRef;
  subRec->connectionPtr = buildIndxReq->senderData;
  subRec->schemaTransId = buildIndxReq->transId;
  subRec->subscriptionId = buildIndxReq->buildId;
  subRec->subscriptionKey = buildIndxReq->buildKey;
  subRec->indexType = buildIndxReq->indexType;
  subRec->sourceTableId = buildIndxReq->tableId;
  subRec->targetTableId = buildIndxReq->indexId;
  subRec->parallelism = c_maxUIBuildBatchSize;
  subRec->expectedConf = 0;
  subRec->subscriptionCreated = false;
  subRec->pendingSubSyncContinueConf = false;
  subRec->prepareId = RNIL;
  subRec->requestType = INDEX_BUILD;
  subRec->fragCount = 0;
  subRec->fragId = ZNIL;
  subRec->m_rows_processed = 0;
  subRec->m_flags = SubscriptionRecord::RF_WAIT_GCP; // Todo make configurable
  subRec->m_gci = 0;
  if (buildIndxReq->requestType & BuildIndxImplReq::RF_NO_DISK)
  {
    subRec->m_flags |= SubscriptionRecord::RF_NO_DISK;
  }

  // Get column order segments
  Uint32 noOfSections = handle.m_cnt;
  if (noOfSections > 0) {
    jam();
    SegmentedSectionPtr ptr;
    handle.getSection(ptr, BuildIndxImplReq::INDEX_COLUMNS);
    append(subRec->attributeOrder, ptr, getSectionSegmentPool());
    subRec->noOfIndexColumns = ptr.sz;
  }
  if (noOfSections > 1) {
    jam();
    SegmentedSectionPtr ptr;
    handle.getSection(ptr, BuildIndxImplReq::KEY_COLUMNS);
    append(subRec->attributeOrder, ptr, getSectionSegmentPool());
    subRec->noOfKeyColumns = ptr.sz;
  }

#if 0
  // Debugging
  printf("Trix:: execBUILD_INDX_IMPL_REQ: Attribute order:\n");
  subRec->attributeOrder.print(stdout);
#endif

  releaseSections(handle);
  prepareInsertTransactions(signal, subRecPtr);
  DBUG_VOID_RETURN;
}

void Trix:: execBUILD_INDX_IMPL_CONF(Signal* signal)
{
  printf("Trix:: execBUILD_INDX_IMPL_CONF\n");
}

void Trix:: execBUILD_INDX_IMPL_REF(Signal* signal)
{
  printf("Trix:: execBUILD_INDX_IMPL_REF\n");
}

void Trix::execUTIL_PREPARE_CONF(Signal* signal)
{
  jamEntry();
  UtilPrepareConf * utilPrepareConf = (UtilPrepareConf *)signal->getDataPtr();
  SubscriptionRecPtr subRecPtr;
  SubscriptionRecord* subRec;

  subRecPtr.i = utilPrepareConf->senderData;
  if ((subRec = c_theSubscriptions.getPtr(subRecPtr.i)) == NULL) {
    printf("Trix::execUTIL_PREPARE_CONF: Failed to find subscription data %u\n", subRecPtr.i);
    return;
  }
  if (subRec->requestType == STAT_UTIL)
  {
    statUtilPrepareConf(signal, subRec->m_statPtrI);
    return;
  }
  subRecPtr.p = subRec;
  subRec->prepareId = utilPrepareConf->prepareId;
  setupSubscription(signal, subRecPtr);
}

void Trix::execUTIL_PREPARE_REF(Signal* signal)
{
  jamEntry();
  UtilPrepareRef * utilPrepareRef = (UtilPrepareRef *)signal->getDataPtr();
  SubscriptionRecPtr subRecPtr;
  SubscriptionRecord* subRec;

  subRecPtr.i = utilPrepareRef->senderData;
  if ((subRec = c_theSubscriptions.getPtr(subRecPtr.i)) == NULL) {
    printf("Trix::execUTIL_PREPARE_REF: Failed to find subscription data %u\n", subRecPtr.i);
    return;
  }
  if (subRec->requestType == STAT_UTIL)
  {
    statUtilPrepareRef(signal, subRec->m_statPtrI);
    return;
  }
  subRecPtr.p = subRec;
  subRec->errorCode = (BuildIndxRef::ErrorCode)utilPrepareRef->errorCode;
  switch (utilPrepareRef->errorCode) {
  case UtilPrepareRef::PREPARE_SEIZE_ERROR:
  case UtilPrepareRef::PREPARE_PAGES_SEIZE_ERROR:
  case UtilPrepareRef::PREPARED_OPERATION_SEIZE_ERROR:
  case UtilPrepareRef::DICT_TAB_INFO_ERROR:
    subRec->errorCode = BuildIndxRef::UtilBusy;
    break;
  case UtilPrepareRef::MISSING_PROPERTIES_SECTION:
    subRec->errorCode = BuildIndxRef::BadRequestType;
    break;
  default:
    ndbabort();
  }

  UtilReleaseConf* conf = (UtilReleaseConf*)signal->getDataPtrSend();
  conf->senderData = subRecPtr.i;
  execUTIL_RELEASE_CONF(signal);
}

void Trix::execUTIL_EXECUTE_CONF(Signal* signal)
{
  jamEntry();
  UtilExecuteConf * utilExecuteConf = (UtilExecuteConf *)signal->getDataPtr();
  SubscriptionRecPtr subRecPtr;
  SubscriptionRecord* subRec;

  const Uint32 gci_hi = utilExecuteConf->gci_hi;
  const Uint32 gci_lo = utilExecuteConf->gci_lo;
  const Uint64 gci = gci_lo | (Uint64(gci_hi) << 32);

  subRecPtr.i = utilExecuteConf->senderData;
  if ((subRec = c_theSubscriptions.getPtr(subRecPtr.i)) == NULL) {
    printf("rix::execUTIL_EXECUTE_CONF: Failed to find subscription data %u\n", subRecPtr.i);
    return;
  }
  if (subRec->requestType == STAT_UTIL)
  {
    statUtilExecuteConf(signal, subRec->m_statPtrI);
    return;
  }
  subRecPtr.p = subRec;
  subRec->expectedConf--;

  if (gci > subRecPtr.p->m_gci)
  {
    jam();
    subRecPtr.p->m_gci = gci;
  }

  checkParallelism(signal, subRec);
  if (subRec->expectedConf == 0)
  {
    if (subRec->m_flags & SubscriptionRecord::RF_WAIT_GCP)
    {
      jam();
      wait_gcp(signal, subRecPtr);
      return;
    }
    buildComplete(signal, subRecPtr);
  }
}

void Trix::execUTIL_EXECUTE_REF(Signal* signal)
{
  jamEntry();
  UtilExecuteRef * utilExecuteRef = (UtilExecuteRef *)signal->getDataPtr();
  SubscriptionRecPtr subRecPtr;
  SubscriptionRecord* subRec;

  subRecPtr.i = utilExecuteRef->senderData;
  if ((subRec = c_theSubscriptions.getPtr(subRecPtr.i)) == NULL) {
    printf("Trix::execUTIL_EXECUTE_REF: Failed to find subscription data %u\n", subRecPtr.i);
    return;
  }
  if (subRec->requestType == STAT_UTIL)
  {
    statUtilExecuteRef(signal, subRec->m_statPtrI);
    return;
  }
  subRecPtr.p = subRec;
  ndbrequire(utilExecuteRef->errorCode == UtilExecuteRef::TCError);
  if(utilExecuteRef->TCErrorCode == CONSTRAINT_VIOLATION)
  {
    jam();
    buildFailed(signal, subRecPtr, BuildIndxRef::IndexNotUnique);
  }
  else if (check_timeout(utilExecuteRef->TCErrorCode))
  {
    jam();
    buildFailed(signal, subRecPtr, BuildIndxRef::DeadlockError);
  }
  else if (subRec->requestType == FK_BUILD &&
           utilExecuteRef->TCErrorCode == TUPLE_NOT_FOUND)
  {
    jam();
    buildFailed(signal, subRecPtr,
                (BuildIndxRef::ErrorCode)FK_NO_PARENT_ROW_EXISTS);
  }
  else
  {
    jam();
    buildFailed(signal, subRecPtr,
                (BuildIndxRef::ErrorCode)utilExecuteRef->TCErrorCode);
  }
}

void Trix::execSUB_CREATE_CONF(Signal* signal)
{
  jamEntry();
  DBUG_ENTER("Trix::execSUB_CREATE_CONF");
  SubCreateConf * subCreateConf = (SubCreateConf *)signal->getDataPtr();
  SubscriptionRecPtr subRecPtr;
  SubscriptionRecord* subRec;

  subRecPtr.i = subCreateConf->senderData;
  if ((subRec = c_theSubscriptions.getPtr(subRecPtr.i)) == NULL) {
    printf("Trix::execSUB_CREATE_CONF: Failed to find subscription data %u\n", subRecPtr.i);
    DBUG_VOID_RETURN;
  }
  subRec->subscriptionCreated = true;
  subRecPtr.p = subRec;

  DBUG_PRINT("info",("i: %u subscriptionId: %u, subscriptionKey: %u",
		     subRecPtr.i, subRecPtr.p->subscriptionId,
		     subRecPtr.p->subscriptionKey));

  startTableScan(signal, subRecPtr);
  DBUG_VOID_RETURN;
}

void Trix::execSUB_CREATE_REF(Signal* signal)
{
  jamEntry();
  DBUG_ENTER("Trix::execSUB_CREATE_REF");

  SubCreateRef * subCreateRef = (SubCreateRef *)signal->getDataPtr();
  SubscriptionRecPtr subRecPtr;
  SubscriptionRecord* subRec;

  subRecPtr.i = subCreateRef->senderData;
  if ((subRec = c_theSubscriptions.getPtr(subRecPtr.i)) == NULL)
  {
    printf("Trix::execSUB_CREATE_REF: Failed to find subscription data %u\n", subRecPtr.i);
    return;
  }
  subRecPtr.p = subRec;
  subRecPtr.p->errorCode = (BuildIndxRef::ErrorCode)subCreateRef->errorCode;

  UtilReleaseReq * const req = (UtilReleaseReq*)signal->getDataPtrSend();
  req->prepareId = subRecPtr.p->prepareId;
  req->senderData = subRecPtr.i;

  sendSignal(DBUTIL_REF, GSN_UTIL_RELEASE_REQ, signal,
	     UtilReleaseReq::SignalLength, JBB);

  DBUG_VOID_RETURN;
}

void Trix::execSUB_SYNC_CONF(Signal* signal)
{
  jamEntry();
  DBUG_ENTER("Trix::execSUB_SYNC_CONF");
  SubSyncConf * subSyncConf = (SubSyncConf *)signal->getDataPtr();
  SubscriptionRecPtr subRecPtr;
  SubscriptionRecord* subRec;
  
  subRecPtr.i = subSyncConf->senderData;
  if ((subRec = c_theSubscriptions.getPtr(subRecPtr.i)) == NULL) {
    printf("Trix::execSUB_SYNC_CONF: Failed to find subscription data %u\n",
	   subRecPtr.i);
    DBUG_VOID_RETURN;
  }

  subRecPtr.p = subRec;
  subRec->expectedConf--;
  checkParallelism(signal, subRec);
  if (subRec->expectedConf == 0)
  {
    if (subRec->m_flags & SubscriptionRecord::RF_WAIT_GCP)
    {
      jam();
      wait_gcp(signal, subRecPtr);
      DBUG_VOID_RETURN;
    }
    buildComplete(signal, subRecPtr);
  }
  DBUG_VOID_RETURN;
}

void Trix::execSUB_SYNC_REF(Signal* signal)
{
  jamEntry();
  DBUG_ENTER("Trix::execSUB_SYNC_REF");
  SubSyncRef * subSyncRef = (SubSyncRef *)signal->getDataPtr();
  SubscriptionRecPtr subRecPtr;
  SubscriptionRecord* subRec;

  subRecPtr.i = subSyncRef->senderData;
  if ((subRec = c_theSubscriptions.getPtr(subRecPtr.i)) == NULL) {
    printf("Trix::execSUB_SYNC_REF: Failed to find subscription data %u\n", subRecPtr.i);
    DBUG_VOID_RETURN;
  }
  subRecPtr.p = subRec;
  buildFailed(signal, subRecPtr,
              (BuildIndxRef::ErrorCode)subSyncRef->errorCode);
  DBUG_VOID_RETURN;
}

void Trix::execSUB_SYNC_CONTINUE_REQ(Signal* signal)
{
  SubSyncContinueReq  * subSyncContinueReq = 
    (SubSyncContinueReq *) signal->getDataPtr();
  
  SubscriptionRecPtr subRecPtr;
  SubscriptionRecord* subRec;
  subRecPtr.i = subSyncContinueReq->subscriberData;
  if ((subRec = c_theSubscriptions.getPtr(subRecPtr.i)) == NULL) {
    printf("Trix::execSUB_SYNC_CONTINUE_REQ: Failed to find subscription data %u\n", subRecPtr.i);
    return;
  }
  subRecPtr.p = subRec;
  subRec->pendingSubSyncContinueConf = true;
  subRec->syncPtr = subSyncContinueReq->senderData;
  checkParallelism(signal, subRec);
}

void Trix::execSUB_TABLE_DATA(Signal* signal)
{
  jamEntry();
  DBUG_ENTER("Trix::execSUB_TABLE_DATA");
  SubTableData * subTableData = (SubTableData *)signal->getDataPtr();
  SubscriptionRecPtr subRecPtr;
  SubscriptionRecord* subRec;
  subRecPtr.i = subTableData->senderData;
  if ((subRec = c_theSubscriptions.getPtr(subRecPtr.i)) == NULL) {
    printf("Trix::execSUB_TABLE_DATA: Failed to find subscription data %u\n", subRecPtr.i);
    DBUG_VOID_RETURN;
  }
  subRecPtr.p = subRec;
  switch(subRecPtr.p->requestType){
  case INDEX_BUILD:
    executeBuildInsertTransaction(signal, subRecPtr);
    break;
  case REORG_COPY:
  case REORG_DELETE:
    executeReorgTransaction(signal, subRecPtr, subTableData->takeOver);
    break;
  case FK_BUILD:
    executeBuildFKTransaction(signal, subRecPtr);
    break;
  case STAT_UTIL:
    ndbabort();
  case STAT_CLEAN:
    {
      StatOp& stat = statOpGetPtr(subRecPtr.p->m_statPtrI);
      statCleanExecute(signal, stat);
    }
    break;
  case STAT_SCAN:
    {
      StatOp& stat = statOpGetPtr(subRecPtr.p->m_statPtrI);
      statScanExecute(signal, stat);
    }
    break;
  }

  subRecPtr.p->m_rows_processed++;

  DBUG_VOID_RETURN;
}

void Trix::setupSubscription(Signal* signal, SubscriptionRecPtr subRecPtr)
{
  jam();
  DBUG_ENTER("Trix::setupSubscription");
  SubscriptionRecord* subRec = subRecPtr.p;
  SubCreateReq * subCreateReq = (SubCreateReq *)signal->getDataPtrSend();
//  Uint32 listLen = subRec->noOfIndexColumns + subRec->noOfKeyColumns;
  subCreateReq->senderRef = reference();
  subCreateReq->senderData = subRecPtr.i;
  subCreateReq->subscriptionId = subRec->subscriptionId;
  subCreateReq->subscriptionKey = subRec->subscriptionKey;
  subCreateReq->tableId = subRec->sourceTableId;
  subCreateReq->subscriptionType = SubCreateReq::SingleTableScan;
  subCreateReq->schemaTransId = subRec->schemaTransId;

  DBUG_PRINT("info",("i: %u subscriptionId: %u, subscriptionKey: %u",
		     subRecPtr.i, subCreateReq->subscriptionId,
		     subCreateReq->subscriptionKey));

  D("SUB_CREATE_REQ tableId: " << subRec->sourceTableId);

  sendSignal(SUMA_REF, GSN_SUB_CREATE_REQ, 
	     signal, SubCreateReq::SignalLength, JBB);

  DBUG_VOID_RETURN;
}

void Trix::startTableScan(Signal* signal, SubscriptionRecPtr subRecPtr)
{
  jam();

  Uint32 attributeList[MAX_ATTRIBUTES_IN_TABLE * 2];
  SubscriptionRecord* subRec = subRecPtr.p;
  AttrOrderBuffer::DataBufferIterator iter;

  Uint32 cnt = 0;
  bool moreAttributes = subRec->attributeOrder.first(iter);
  if (subRec->requestType == FK_BUILD)
  {
    jam();
    // skip over key columns
    ndbrequire(subRec->attributeOrder.position(iter, subRec->noOfKeyColumns));
  }

  while (moreAttributes) {
    attributeList[cnt++] = *iter.data;
    moreAttributes = subRec->attributeOrder.next(iter);
  }

  // Merge index and key column segments
  LinearSectionPtr orderPtr[3];
  Uint32 noOfSections;
  orderPtr[0].p = attributeList;
  orderPtr[0].sz = cnt;
  noOfSections = 1;

  SubSyncReq * subSyncReq = (SubSyncReq *)signal->getDataPtrSend();
  subSyncReq->senderRef = reference();
  subSyncReq->senderData = subRecPtr.i;
  subSyncReq->subscriptionId = subRec->subscriptionId;
  subSyncReq->subscriptionKey = subRec->subscriptionKey;
  subSyncReq->part = SubscriptionData::TableData;
  subSyncReq->requestInfo = 0;
  subSyncReq->fragCount = subRec->fragCount;
  subSyncReq->fragId = subRec->fragId;
  subSyncReq->batchSize = subRec->parallelism;

  if (subRec->m_flags & SubscriptionRecord::RF_NO_DISK)
  {
    jam();
    subSyncReq->requestInfo |= SubSyncReq::NoDisk;
  }

  if (subRec->m_flags & SubscriptionRecord::RF_TUP_ORDER)
  {
    jam();
    subSyncReq->requestInfo |= SubSyncReq::TupOrder;
  }
  
  if (subRec->requestType == REORG_COPY)
  {
    jam();
    subSyncReq->requestInfo |= SubSyncReq::LM_Exclusive;
  }
  else if (subRec->requestType == REORG_DELETE)
  {
    jam();
    subSyncReq->requestInfo |= SubSyncReq::LM_Exclusive;
    subSyncReq->requestInfo |= SubSyncReq::ReorgDelete;
  }
  else if (subRec->requestType == STAT_CLEAN)
  {
    jam();
    StatOp& stat = statOpGetPtr(subRecPtr.p->m_statPtrI);
    StatOp::Clean& clean = stat.m_clean;
    orderPtr[1].p = clean.m_bound;
    orderPtr[1].sz = clean.m_boundSize;
    noOfSections = 2;
    subSyncReq->requestInfo |= SubSyncReq::LM_CommittedRead;
    subSyncReq->requestInfo |= SubSyncReq::RangeScan;
  }
  else if (subRec->requestType == STAT_SCAN)
  {
    jam();
    orderPtr[1].p = 0;
    orderPtr[1].sz = 0;
    noOfSections = 2;
    subSyncReq->requestInfo |= SubSyncReq::LM_CommittedRead;
    subSyncReq->requestInfo |= SubSyncReq::RangeScan;
    subSyncReq->requestInfo |= SubSyncReq::StatScan;
  }
  subRecPtr.p->expectedConf = 1;

  DBUG_PRINT("info",("i: %u subscriptionId: %u, subscriptionKey: %u",
		     subRecPtr.i, subSyncReq->subscriptionId,
		     subSyncReq->subscriptionKey));

  D("SUB_SYNC_REQ fragId: " << subRec->fragId <<
    " fragCount: " << subRec->fragCount <<
    " requestInfo: " << hex << subSyncReq->requestInfo);

  sendSignal(SUMA_REF, GSN_SUB_SYNC_REQ,
	     signal, SubSyncReq::SignalLength, JBB, orderPtr, noOfSections);
}

void Trix::prepareInsertTransactions(Signal* signal, 
				     SubscriptionRecPtr subRecPtr)
{
  SubscriptionRecord* subRec = subRecPtr.p;
  UtilPrepareReq * utilPrepareReq = 
    (UtilPrepareReq *)signal->getDataPtrSend();
  
  jam();
  utilPrepareReq->senderRef = reference();
  utilPrepareReq->senderData = subRecPtr.i;
  utilPrepareReq->schemaTransId = subRec->schemaTransId;

  const Uint32 pageSizeInWords = 128;
  Uint32 propPage[pageSizeInWords];
  LinearWriter w(&propPage[0],128);
  w.first();
  w.add(UtilPrepareReq::NoOfOperations, 1);
  w.add(UtilPrepareReq::OperationType, UtilPrepareReq::Write);
  w.add(UtilPrepareReq::TableId, subRec->targetTableId);
  // Add index attributes in increasing order and one PK attribute
  for(Uint32 i = 0; i < subRec->noOfIndexColumns + 1; i++)
    w.add(UtilPrepareReq::AttributeId, i);

#if 0
  // Debugging
  SimplePropertiesLinearReader reader(propPage, w.getWordsUsed());
  printf("Trix::prepareInsertTransactions: Sent SimpleProperties:\n");
  reader.printAll(ndbout);
#endif

  struct LinearSectionPtr sectionsPtr[UtilPrepareReq::NoOfSections];
  sectionsPtr[UtilPrepareReq::PROPERTIES_SECTION].p = propPage;
  sectionsPtr[UtilPrepareReq::PROPERTIES_SECTION].sz = w.getWordsUsed();
  sendSignal(DBUTIL_REF, GSN_UTIL_PREPARE_REQ, signal,
	     UtilPrepareReq::SignalLength, JBB, 
	     sectionsPtr, UtilPrepareReq::NoOfSections);
}

void Trix::executeBuildInsertTransaction(Signal* signal,
                                         SubscriptionRecPtr subRecPtr)
{
  jam();
  SubscriptionRecord* subRec = subRecPtr.p;
  UtilExecuteReq * utilExecuteReq = 
    (UtilExecuteReq *)signal->getDataPtrSend();

  utilExecuteReq->senderRef = reference();
  utilExecuteReq->senderData = subRecPtr.i;
  utilExecuteReq->prepareId = subRec->prepareId;
#if 0
  printf("Header size %u\n", headerPtr.sz);
  for(int i = 0; i < headerPtr.sz; i++)
    printf("H'%.8x ", headerBuffer[i]);
  printf("\n");
  
  printf("Data size %u\n", dataPtr.sz);
  for(int i = 0; i < dataPtr.sz; i++)
    printf("H'%.8x ", dataBuffer[i]);
  printf("\n");
#endif
  // Save scan result in linear buffers
  SectionHandle handle(this, signal);
  SegmentedSectionPtr headerPtr, dataPtr;

  handle.getSection(headerPtr, 0);
  handle.getSection(dataPtr, 1);

  Uint32* headerBuffer = signal->theData + 25;
  Uint32* dataBuffer = headerBuffer + headerPtr.sz;

  copy(headerBuffer, headerPtr);
  copy(dataBuffer, dataPtr);
  releaseSections(handle);

  // Calculate packed key size
  Uint32 noOfKeyData = 0;
  for(Uint32 i = 0; i < headerPtr.sz; i++) {
    AttributeHeader* keyAttrHead = (AttributeHeader *) headerBuffer + i;

    // Filter out NULL attributes
    if (keyAttrHead->isNULL())
      return;

    if (i < subRec->noOfIndexColumns)
      // Renumber index attributes in consequtive order
      keyAttrHead->setAttributeId(i);
    else
      // Calculate total size of PK attribute
      noOfKeyData += keyAttrHead->getDataSize();
  }
  // Increase expected CONF count
  subRec->expectedConf++;

  // Pack key attributes
  AttributeHeader::init(headerBuffer + subRec->noOfIndexColumns,
			subRec->noOfIndexColumns,
			noOfKeyData << 2);

  struct LinearSectionPtr sectionsPtr[UtilExecuteReq::NoOfSections];
  sectionsPtr[UtilExecuteReq::HEADER_SECTION].p = headerBuffer;
  sectionsPtr[UtilExecuteReq::HEADER_SECTION].sz = 
    subRec->noOfIndexColumns + 1;
  sectionsPtr[UtilExecuteReq::DATA_SECTION].p = dataBuffer;
  sectionsPtr[UtilExecuteReq::DATA_SECTION].sz = dataPtr.sz;
  sendSignal(DBUTIL_REF, GSN_UTIL_EXECUTE_REQ, signal,
	     UtilExecuteReq::SignalLength, JBB,
	     sectionsPtr, UtilExecuteReq::NoOfSections);
}

void Trix::executeReorgTransaction(Signal* signal,
                                   SubscriptionRecPtr subRecPtr,
                                   Uint32 takeOver)
{
  jam();
  SubscriptionRecord* subRec = subRecPtr.p;
  UtilExecuteReq * utilExecuteReq =
    (UtilExecuteReq *)signal->getDataPtrSend();

  const Uint32 tScanInfo = takeOver & 0x3FFFF;
  const Uint32 tTakeOverFragment = takeOver >> 20;
  {
    UintR scanInfo = 0;
    TcKeyReq::setTakeOverScanFlag(scanInfo, 1);
    TcKeyReq::setTakeOverScanFragment(scanInfo, tTakeOverFragment);
    TcKeyReq::setTakeOverScanInfo(scanInfo, tScanInfo);
    utilExecuteReq->scanTakeOver = scanInfo;
  }

  utilExecuteReq->senderRef = reference();
  utilExecuteReq->senderData = subRecPtr.i;
  utilExecuteReq->prepareId = subRec->prepareId;
#if 0
  printf("Header size %u\n", headerPtr.sz);
  for(int i = 0; i < headerPtr.sz; i++)
    printf("H'%.8x ", headerBuffer[i]);
  printf("\n");

  printf("Data size %u\n", dataPtr.sz);
  for(int i = 0; i < dataPtr.sz; i++)
    printf("H'%.8x ", dataBuffer[i]);
  printf("\n");
#endif
  // Increase expected CONF count
  subRec->expectedConf++;

  SectionHandle handle(this, signal);
  sendSignal(DBUTIL_REF, GSN_UTIL_EXECUTE_REQ, signal,
	     UtilExecuteReq::SignalLength, JBB,
	     &handle);
}

void
Trix::wait_gcp(Signal* signal, SubscriptionRecPtr subRecPtr, Uint32 delay)
{
  WaitGCPReq * req = (WaitGCPReq*)signal->getDataPtrSend();
  req->senderRef = reference();
  req->senderData = subRecPtr.i;
  req->requestType = WaitGCPReq::CurrentGCI;

  if (delay == 0)
  {
    jam();
    sendSignal(DBDIH_REF, GSN_WAIT_GCP_REQ, signal,
               WaitGCPReq::SignalLength, JBB);
  }
  else
  {
    jam();
    sendSignalWithDelay(DBDIH_REF, GSN_WAIT_GCP_REQ, signal,
                        delay, WaitGCPReq::SignalLength);
  }
}

void
Trix::execWAIT_GCP_REF(Signal* signal)
{
  WaitGCPRef ref = *(WaitGCPRef*)signal->getDataPtr();

  SubscriptionRecPtr subRecPtr;
  c_theSubscriptions.getPtr(subRecPtr, ref.senderData);
  wait_gcp(signal, subRecPtr, 100);
}

void
Trix::execWAIT_GCP_CONF(Signal* signal)
{
  WaitGCPConf * conf = (WaitGCPConf*)signal->getDataPtr();
  
  SubscriptionRecPtr subRecPtr;
  c_theSubscriptions.getPtr(subRecPtr, conf->senderData);

  const Uint32 gci_hi = conf->gci_hi;
  const Uint32 gci_lo = conf->gci_lo;
  const Uint64 gci = gci_lo | (Uint64(gci_hi) << 32);
  
  if (gci > subRecPtr.p->m_gci)
  {
    jam();
    buildComplete(signal, subRecPtr);
  }
  else
  {
    jam();
    wait_gcp(signal, subRecPtr, 100);
  }
}

void Trix::buildComplete(Signal* signal, SubscriptionRecPtr subRecPtr)
{
  SubRemoveReq * const req = (SubRemoveReq*)signal->getDataPtrSend();
  req->senderRef       = reference();
  req->senderData      = subRecPtr.i;
  req->subscriptionId  = subRecPtr.p->subscriptionId;
  req->subscriptionKey = subRecPtr.p->subscriptionKey;
  sendSignal(SUMA_REF, GSN_SUB_REMOVE_REQ, signal,
	     SubRemoveReq::SignalLength, JBB);
}

void Trix::buildFailed(Signal* signal, 
		       SubscriptionRecPtr subRecPtr, 
		       BuildIndxRef::ErrorCode errorCode)
{
  SubscriptionRecord* subRec = subRecPtr.p;

  subRec->errorCode = errorCode;
  // Continue accumulating since we currently cannot stop SUMA
  subRec->expectedConf--;
  checkParallelism(signal, subRec);
  if (subRec->expectedConf == 0)
    buildComplete(signal, subRecPtr);
}

void
Trix::execSUB_REMOVE_REF(Signal* signal){
  jamEntry();
  //@todo
  ndbabort();
}

void
Trix::execSUB_REMOVE_CONF(Signal* signal){
  jamEntry();

  SubRemoveConf * const conf = (SubRemoveConf*)signal->getDataPtrSend();

  SubscriptionRecPtr subRecPtr;
  c_theSubscriptions.getPtr(subRecPtr, conf->senderData);

  if(subRecPtr.p->prepareId != RNIL){
    jam();

    UtilReleaseReq * const req = (UtilReleaseReq*)signal->getDataPtrSend();
    req->prepareId = subRecPtr.p->prepareId;
    req->senderData = subRecPtr.i;

    sendSignal(DBUTIL_REF, GSN_UTIL_RELEASE_REQ, signal, 
	       UtilReleaseReq::SignalLength , JBB);  
    return;
  }
  
  {
    UtilReleaseConf * const conf = (UtilReleaseConf*)signal->getDataPtrSend();
    conf->senderData = subRecPtr.i;
    execUTIL_RELEASE_CONF(signal);
  }
}

void
Trix::execUTIL_RELEASE_REF(Signal* signal){
  jamEntry();
  ndbabort();
}

void
Trix::execUTIL_RELEASE_CONF(Signal* signal){
  
  UtilReleaseConf * const conf = (UtilReleaseConf*)signal->getDataPtrSend();
  
  SubscriptionRecPtr subRecPtr;
  c_theSubscriptions.getPtr(subRecPtr, conf->senderData);
  
  switch(subRecPtr.p->requestType){
  case REORG_COPY:
  case REORG_DELETE:
    if (subRecPtr.p->errorCode == BuildIndxRef::NoError)
    {
      jam();
      // Build is complete, reply to original sender
      CopyDataImplConf* conf = (CopyDataImplConf*)signal->getDataPtrSend();
      conf->senderRef = reference(); //wl3600_todo ok?
      conf->senderData = subRecPtr.p->connectionPtr;

      sendSignal(subRecPtr.p->userReference, GSN_COPY_DATA_IMPL_CONF, signal,
                 CopyDataImplConf::SignalLength , JBB);

      infoEvent("%s table %u processed %llu rows",
                subRecPtr.p->requestType == REORG_COPY ?
                "reorg-copy" : "reorg-delete",
                subRecPtr.p->sourceTableId,
                subRecPtr.p->m_rows_processed);
    } else {
      jam();
      // Build failed, reply to original sender
      CopyDataImplRef* ref = (CopyDataImplRef*)signal->getDataPtrSend();
      ref->senderRef = reference();
      ref->senderData = subRecPtr.p->connectionPtr;
      ref->errorCode = subRecPtr.p->errorCode;

      sendSignal(subRecPtr.p->userReference, GSN_COPY_DATA_IMPL_REF, signal,
                 CopyDataImplRef::SignalLength , JBB);
    }
    break;
  case INDEX_BUILD:
    if (subRecPtr.p->errorCode == BuildIndxRef::NoError) {
      jam();
      // Build is complete, reply to original sender
      BuildIndxImplConf* buildIndxConf =
        (BuildIndxImplConf*)signal->getDataPtrSend();
      buildIndxConf->senderRef = reference(); //wl3600_todo ok?
      buildIndxConf->senderData = subRecPtr.p->connectionPtr;

      sendSignal(subRecPtr.p->userReference, GSN_BUILD_INDX_IMPL_CONF, signal,
                 BuildIndxConf::SignalLength , JBB);

      infoEvent("index-build table %u index: %u processed %llu rows",
                subRecPtr.p->sourceTableId,
                subRecPtr.p->targetTableId,
                subRecPtr.p->m_rows_processed);
    } else {
      jam();
      // Build failed, reply to original sender
      BuildIndxImplRef* buildIndxRef =
        (BuildIndxImplRef*)signal->getDataPtrSend();
      buildIndxRef->senderRef = reference();
      buildIndxRef->senderData = subRecPtr.p->connectionPtr;
      buildIndxRef->errorCode = subRecPtr.p->errorCode;

      sendSignal(subRecPtr.p->userReference, GSN_BUILD_INDX_IMPL_REF, signal,
                 BuildIndxRef::SignalLength , JBB);
    }
    break;
  case FK_BUILD:
    if (subRecPtr.p->errorCode == BuildIndxRef::NoError)
    {
      jam();
      // Build is complete, reply to original sender
      BuildFKImplConf* buildFKConf =
        CAST_PTR(BuildFKImplConf, signal->getDataPtrSend());
      buildFKConf->senderRef = reference();
      buildFKConf->senderData = subRecPtr.p->connectionPtr;

      sendSignal(subRecPtr.p->userReference, GSN_BUILD_FK_IMPL_CONF, signal,
                 BuildFKImplConf::SignalLength , JBB);

      infoEvent("fk-build parent table: %u child table: %u processed %llu rows",
                subRecPtr.p->targetTableId,
                subRecPtr.p->sourceTableId,
                subRecPtr.p->m_rows_processed);
    }
    else
    {
      jam();
      // Build failed, reply to original sender
      BuildFKImplRef* buildFKRef =
        (BuildFKImplRef*)signal->getDataPtrSend();
      buildFKRef->senderRef = reference();
      buildFKRef->senderData = subRecPtr.p->connectionPtr;
      buildFKRef->errorCode = subRecPtr.p->errorCode;

      sendSignal(subRecPtr.p->userReference, GSN_BUILD_FK_IMPL_REF, signal,
                 BuildFKImplRef::SignalLength , JBB);
    }
    break;
  case STAT_UTIL:
    ndbrequire(subRecPtr.p->errorCode == BuildIndxRef::NoError);
    statUtilReleaseConf(signal, subRecPtr.p->m_statPtrI);
    return;
  case STAT_CLEAN:
    {
      subRecPtr.p->prepareId = RNIL;
      StatOp& stat = statOpGetPtr(subRecPtr.p->m_statPtrI);
      statCleanRelease(signal, stat);
    }
    return;
  case STAT_SCAN:
    {
      subRecPtr.p->prepareId = RNIL;
      StatOp& stat = statOpGetPtr(subRecPtr.p->m_statPtrI);
      statScanRelease(signal, stat);
    }
    return;
  }

  // Release subscription record
  subRecPtr.p->attributeOrder.release();
  c_theSubscriptions.release(subRecPtr.i);
}

void Trix::checkParallelism(Signal* signal, SubscriptionRecord* subRec)
{
  if ((subRec->pendingSubSyncContinueConf) &&
      (subRec->expectedConf == 1)) {
    jam();
    SubSyncContinueConf  * subSyncContinueConf = 
      (SubSyncContinueConf *) signal->getDataPtrSend();
    subSyncContinueConf->subscriptionId = subRec->subscriptionId;
    subSyncContinueConf->subscriptionKey = subRec->subscriptionKey;
    subSyncContinueConf->senderData = subRec->syncPtr;
    sendSignal(SUMA_REF, GSN_SUB_SYNC_CONTINUE_CONF, signal, 
	       SubSyncContinueConf::SignalLength , JBB);  
    subRec->pendingSubSyncContinueConf = false;
    return;
  }
}

// CopyData
void
Trix::execCOPY_DATA_IMPL_REQ(Signal* signal)
{
  jamEntry();

  const CopyDataImplReq reqData = *(const CopyDataImplReq*)signal->getDataPtr();
  const CopyDataImplReq *req = &reqData;

  // Seize a subscription record
  SubscriptionRecPtr subRecPtr;
  SectionHandle handle(this, signal);

  if (!c_theSubscriptions.seizeFirst(subRecPtr))
  {
    jam();
    // Failed to allocate subscription record
    releaseSections(handle);

    CopyDataImplRef* ref = (CopyDataRef*)signal->getDataPtrSend();

    ref->errorCode = -1; // XXX CopyDataImplRef::AllocationFailure;
    ref->senderData = req->senderData;
    ref->transId = req->transId;
    sendSignal(req->senderRef, GSN_COPY_DATA_IMPL_REF, signal,
               CopyDataImplRef::SignalLength, JBB);
    return;
  }

  SubscriptionRecord* subRec = subRecPtr.p;
  subRec->errorCode = BuildIndxRef::NoError;
  subRec->userReference = req->senderRef;
  subRec->connectionPtr = req->senderData;
  subRec->schemaTransId = req->transId;
  subRec->subscriptionId = rand();
  subRec->subscriptionKey = rand();
  subRec->indexType = RNIL;
  subRec->sourceTableId = req->srcTableId;
  subRec->targetTableId = req->dstTableId;
  subRec->parallelism = c_maxReorgBuildBatchSize;
  subRec->expectedConf = 0;
  subRec->subscriptionCreated = false;
  subRec->pendingSubSyncContinueConf = false;
  subRec->prepareId = req->transId;
  subRec->fragCount = req->srcFragments;
  subRec->fragId = ZNIL;
  subRec->m_rows_processed = 0;
  subRec->m_flags = SubscriptionRecord::RF_WAIT_GCP; // Todo make configurable
  subRec->m_gci = 0;
  switch(req->requestType){
  case CopyDataImplReq::ReorgCopy:
    jam();
    subRec->requestType = REORG_COPY;
    break;
  case CopyDataImplReq::ReorgDelete:
    subRec->requestType = REORG_DELETE;
    break;
  default:
    jamLine(req->requestType);
    ndbabort();
  }

  if (req->requestInfo & CopyDataReq::TupOrder)
  {
    jam();
    subRec->m_flags |= SubscriptionRecord::RF_TUP_ORDER;
  }

  // Get column order segments
  Uint32 noOfSections = handle.m_cnt;
  if (noOfSections > 0) {
    jam();
    SegmentedSectionPtr ptr;
    handle.getSection(ptr, 0);
    append(subRec->attributeOrder, ptr, getSectionSegmentPool());
    subRec->noOfIndexColumns = ptr.sz;
  }

  if (noOfSections > 1) {
    jam();
    SegmentedSectionPtr ptr;
    handle.getSection(ptr, 1);
    append(subRec->attributeOrder, ptr, getSectionSegmentPool());
    subRec->noOfKeyColumns = ptr.sz;
  }

  D("COPY_DATA_IMPL_REQ srctableId: " << subRec->sourceTableId <<
    " targetTableId: " << subRec->targetTableId <<
    " fragCount: " << subRec->fragCount <<
    " requestType: " << subRec->requestType <<
    " flags: " << hex << subRec->m_flags);

  releaseSections(handle);
  {
    UtilPrepareReq * utilPrepareReq =
      (UtilPrepareReq *)signal->getDataPtrSend();

    utilPrepareReq->senderRef = reference();
    utilPrepareReq->senderData = subRecPtr.i;
    utilPrepareReq->schemaTransId = subRec->schemaTransId;

    const Uint32 pageSizeInWords = 128;
    Uint32 propPage[pageSizeInWords];
    LinearWriter w(&propPage[0],128);
    w.first();
    w.add(UtilPrepareReq::NoOfOperations, 1);
    if (subRec->requestType == REORG_COPY)
    {
      w.add(UtilPrepareReq::OperationType, UtilPrepareReq::Write);
    }
    else
    {
      w.add(UtilPrepareReq::OperationType, UtilPrepareReq::Delete);
    }
    if (!(req->requestInfo & CopyDataReq::NoScanTakeOver))
    {
      w.add(UtilPrepareReq::ScanTakeOverInd, 1);
    }
    w.add(UtilPrepareReq::ReorgInd, 1);
    w.add(UtilPrepareReq::TableId, subRec->targetTableId);

    AttrOrderBuffer::DataBufferIterator iter;
    ndbrequire(subRec->attributeOrder.first(iter));
    
    for(Uint32 i = 0; i < subRec->noOfIndexColumns; i++)
    {
      w.add(UtilPrepareReq::AttributeId, * iter.data);
      subRec->attributeOrder.next(iter);
    }
    
    struct LinearSectionPtr sectionsPtr[UtilPrepareReq::NoOfSections];
    sectionsPtr[UtilPrepareReq::PROPERTIES_SECTION].p = propPage;
    sectionsPtr[UtilPrepareReq::PROPERTIES_SECTION].sz = w.getWordsUsed();
    sendSignal(DBUTIL_REF, GSN_UTIL_PREPARE_REQ, signal,
               UtilPrepareReq::SignalLength, JBB,
               sectionsPtr, UtilPrepareReq::NoOfSections);
  }
}

// BuildFK
void
Trix::execBUILD_FK_IMPL_REQ(Signal* signal)
{
  jamEntry();

  const BuildFKImplReq reqData = *(const BuildFKImplReq*)signal->getDataPtr();
  const BuildFKImplReq *req = &reqData;

  // Seize a subscription record
  SubscriptionRecPtr subRecPtr;
  SectionHandle handle(this, signal);

  if (!c_theSubscriptions.seizeFirst(subRecPtr))
  {
    jam();
    // Failed to allocate subscription record
    releaseSections(handle);

    BuildFKImplRef* ref = (BuildFKImplRef*)signal->getDataPtrSend();

    ref->errorCode = -1; // XXX BuildFKImplRef::AllocationFailure;
    ref->senderData = req->senderData;
    sendSignal(req->senderRef, GSN_BUILD_FK_IMPL_REF, signal,
               BuildFKImplRef::SignalLength, JBB);
    return;
  }

  SubscriptionRecord* subRec = subRecPtr.p;
  subRec->errorCode = BuildIndxRef::NoError;
  subRec->userReference = req->senderRef;
  subRec->connectionPtr = req->senderData;
  subRec->schemaTransId = req->transId;
  subRec->subscriptionId = rand();
  subRec->subscriptionKey = rand();
  subRec->indexType = RNIL;
  subRec->sourceTableId = req->childTableId;
  subRec->targetTableId = req->parentTableId;
  subRec->parallelism = c_maxFKBuildBatchSize;
  subRec->expectedConf = 0;
  subRec->subscriptionCreated = false;
  subRec->pendingSubSyncContinueConf = false;
  subRec->prepareId = req->transId;
  subRec->fragCount = 0; // all
  subRec->fragId = ZNIL;
  subRec->m_rows_processed = 0;
  subRec->m_flags = 0;
  subRec->m_gci = 0;
  subRec->requestType = FK_BUILD;

  // TODO...check if there is a scenario where this is not optimal
  subRec->m_flags |= SubscriptionRecord::RF_TUP_ORDER;

  // as we don't support index on disk...
  subRec->m_flags |= SubscriptionRecord::RF_NO_DISK;

  // Get parent columns...
  {
    SegmentedSectionPtr ptr;
    handle.getSection(ptr, 0);
    append(subRec->attributeOrder, ptr, getSectionSegmentPool());
    subRec->noOfKeyColumns = ptr.sz;
  }

  {
    // Get child columns...
    SegmentedSectionPtr ptr;
    handle.getSection(ptr, 1);
    append(subRec->attributeOrder, ptr, getSectionSegmentPool());
    subRec->noOfIndexColumns = ptr.sz;
  }

  ndbrequire(subRec->noOfKeyColumns == subRec->noOfIndexColumns);

  releaseSections(handle);

  {
    UtilPrepareReq * utilPrepareReq =
      (UtilPrepareReq *)signal->getDataPtrSend();

    utilPrepareReq->senderRef = reference();
    utilPrepareReq->senderData = subRecPtr.i;
    utilPrepareReq->schemaTransId = subRec->schemaTransId;

    const Uint32 pageSizeInWords = 128;
    Uint32 propPage[pageSizeInWords];
    LinearWriter w(&propPage[0],128);
    w.first();
    w.add(UtilPrepareReq::NoOfOperations, 1);
    w.add(UtilPrepareReq::OperationType, UtilPrepareReq::Probe);
    w.add(UtilPrepareReq::TableId, subRec->targetTableId);

    // key is always in 0
    AttrOrderBuffer::DataBufferIterator iter;
    ndbrequire(subRec->attributeOrder.first(iter));
    for(Uint32 i = 0; i < subRec->noOfKeyColumns; i++)
    {
      w.add(UtilPrepareReq::AttributeId, * iter.data);
      subRec->attributeOrder.next(iter);
    }

    struct LinearSectionPtr sectionsPtr[UtilPrepareReq::NoOfSections];
    sectionsPtr[UtilPrepareReq::PROPERTIES_SECTION].p = propPage;
    sectionsPtr[UtilPrepareReq::PROPERTIES_SECTION].sz = w.getWordsUsed();
    sendSignal(DBUTIL_REF, GSN_UTIL_PREPARE_REQ, signal,
               UtilPrepareReq::SignalLength, JBB,
               sectionsPtr, UtilPrepareReq::NoOfSections);
  }
}

void
Trix::executeBuildFKTransaction(Signal* signal,
                                SubscriptionRecPtr subRecPtr)
{
  jam();
  SubscriptionRecord* subRec = subRecPtr.p;
  UtilExecuteReq * utilExecuteReq =
    CAST_PTR(UtilExecuteReq, signal->getDataPtrSend());

  utilExecuteReq->senderRef = reference();
  utilExecuteReq->senderData = subRecPtr.i;
  utilExecuteReq->prepareId = subRec->prepareId;

  // Save scan result in linear buffers
  SectionHandle handle(this, signal);
  SegmentedSectionPtr headerPtr, dataPtr;

  handle.getSection(headerPtr, 0);
  handle.getSection(dataPtr, 1);

  Uint32* headerBuffer = signal->theData + 25;
  Uint32* dataBuffer = headerBuffer + headerPtr.sz;

  copy(headerBuffer, headerPtr);
  copy(dataBuffer, dataPtr);
  releaseSections(handle);

  AttrOrderBuffer::ConstDataBufferIterator iter;
  ndbrequire(subRec->attributeOrder.first(iter));
  for(Uint32 i = 0; i < headerPtr.sz; i++)
  {
    AttributeHeader* keyAttrHead = (AttributeHeader *) headerBuffer + i;

    // Filter out NULL attributes
    if (keyAttrHead->isNULL())
      return;

    /**
     * UTIL_EXECUTE header section expects real attrid (same as passed in
     * UTIL_PREPARE).  SUMA sends child attrid, replace it by parent attrid.
     */
    keyAttrHead->setAttributeId(*iter.data);
    subRec->attributeOrder.next(iter);
  }
  // Increase expected CONF count
  subRec->expectedConf++;

  struct LinearSectionPtr sectionsPtr[UtilExecuteReq::NoOfSections];
  sectionsPtr[UtilExecuteReq::HEADER_SECTION].p = headerBuffer;
  sectionsPtr[UtilExecuteReq::HEADER_SECTION].sz = subRec->noOfKeyColumns;
  sectionsPtr[UtilExecuteReq::DATA_SECTION].p = dataBuffer;
  sectionsPtr[UtilExecuteReq::DATA_SECTION].sz = dataPtr.sz;
  sendSignal(DBUTIL_REF, GSN_UTIL_EXECUTE_REQ, signal,
	     UtilExecuteReq::SignalLength, JBB,
	     sectionsPtr, UtilExecuteReq::NoOfSections);
}

// index stats

Trix::StatOp&
Trix::statOpGetPtr(Uint32 statPtrI)
{
  ndbrequire(statPtrI != RNIL);
  return *c_statOpPool.getPtr(statPtrI);
}

bool
Trix::statOpSeize(Uint32& statPtrI)
{
  StatOpPtr statPtr;
  if (ERROR_INSERTED(18001) ||
      !c_statOpPool.seize(statPtr))
  {
    jam();
    CLEAR_ERROR_INSERT_VALUE;
    D("statOpSeize: seize statOp failed");
    return false;
  }
#ifdef VM_TRACE
  memset(statPtr.p, 0xf3, sizeof(*statPtr.p));
#endif
  new (statPtr.p) StatOp;
  statPtrI = statPtr.i;
  StatOp& stat = statOpGetPtr(statPtrI);
  stat.m_ownPtrI = statPtrI;

  SubscriptionRecPtr subRecPtr;
  if (ERROR_INSERTED(18002) ||
      !c_theSubscriptions.seizeFirst(subRecPtr))
  {
    jam();
    CLEAR_ERROR_INSERT_VALUE;
    c_statOpPool.release(statPtr);
    D("statOpSeize: seize subRec failed");
    return false;
  }
  SubscriptionRecord* subRec = subRecPtr.p;
  subRec->m_statPtrI = stat.m_ownPtrI;
  stat.m_subRecPtrI = subRecPtr.i;

  D("statOpSeize" << V(statPtrI) << V(subRecPtr.i));
  return true;
}

void
Trix::statOpRelease(StatOp& stat)
{
  StatOp::Util& util = stat.m_util;
  D("statOpRelease" << V(stat));

  if (stat.m_subRecPtrI != RNIL)
  {
    jam();
    SubscriptionRecord* subRec = c_theSubscriptions.getPtr(stat.m_subRecPtrI);
    ndbrequire(subRec->prepareId == RNIL);
    subRec->attributeOrder.release();
    c_theSubscriptions.release(stat.m_subRecPtrI);
    stat.m_subRecPtrI = RNIL;
  }
  ndbrequire(util.m_prepareId == RNIL);
  c_statOpPool.release(stat.m_ownPtrI);
}

void
Trix::execINDEX_STAT_IMPL_REQ(Signal* signal)
{
  jamEntry();
  const IndexStatImplReq* req = (const IndexStatImplReq*)signal->getDataPtr();

  Uint32 statPtrI = RNIL;
  if (!statOpSeize(statPtrI))
  {
    jam();
    const IndexStatImplReq reqCopy = *req;
    statOpRef(signal, &reqCopy, IndexStatRef::NoFreeStatOp, __LINE__);
    return;
  }
  StatOp& stat = statOpGetPtr(statPtrI);
  stat.m_req = *req;
  stat.m_requestType = req->requestType;

  // set request name for cluster log message
  switch (stat.m_requestType) {
  case IndexStatReq::RT_CLEAN_NEW:
    jam();
    stat.m_requestName = "clean new";
    break;
  case IndexStatReq::RT_CLEAN_OLD:
    jam();
    stat.m_requestName = "clean old";
    break;
  case IndexStatReq::RT_CLEAN_ALL:
    jam();
    stat.m_requestName = "clean all";
    break;
  case IndexStatReq::RT_SCAN_FRAG:
    jam();
    stat.m_requestName = "scan frag";
    break;
  case IndexStatReq::RT_DROP_HEAD:
    jam();
    stat.m_requestName = "drop head";
    break;
  default:
    ndbabort();
  }

  SubscriptionRecord* subRec = c_theSubscriptions.getPtr(stat.m_subRecPtrI);
  subRec->prepareId = RNIL;
  subRec->errorCode = BuildIndxRef::NoError;

  // sys tables are not recreated so do this only once
  if (!c_statGetMetaDone)
  {
    jam();
    statMetaGetHead(signal, stat);
    return;
  }
  statGetMetaDone(signal, stat);
}

// sys tables metadata

const Trix::SysColumn
Trix::g_statMetaHead_column[] = {
  { 0, "index_id",
    true
  },
  { 1, "index_version",
    true
  },
  { 2, "table_id",
    false
  },
  { 3, "frag_count",
    false
  },
  { 4, "value_format",
    false
  },
  { 5, "sample_version",
    false
  },
  { 6, "load_time",
    false
  },
  { 7, "sample_count",
    false
  },
  { 8, "key_bytes",
    false
  }
};

const Trix::SysColumn
Trix::g_statMetaSample_column[] = {
  { 0, "index_id",
    true
  },
  { 1, "index_version",
    true
  },
  { 2, "sample_version",
    true
  },
  { 3, "stat_key",
    true
  },
  { 4, "stat_value",
    false
  }
};

const Trix::SysTable
Trix::g_statMetaHead = {
  NDB_INDEX_STAT_DB "/" NDB_INDEX_STAT_SCHEMA "/" NDB_INDEX_STAT_HEAD_TABLE,
  ~(Uint32)0,
  sizeof(g_statMetaHead_column)/sizeof(g_statMetaHead_column[0]),
  g_statMetaHead_column
};

const Trix::SysTable
Trix::g_statMetaSample = {
  NDB_INDEX_STAT_DB "/" NDB_INDEX_STAT_SCHEMA "/" NDB_INDEX_STAT_SAMPLE_TABLE,
  ~(Uint32)0,
  sizeof(g_statMetaSample_column)/sizeof(g_statMetaSample_column[0]),
  g_statMetaSample_column
};

const Trix::SysIndex
Trix::g_statMetaSampleX1 = {
  // indexes are always in "sys"
  "sys" "/" NDB_INDEX_STAT_SCHEMA "/%u/" NDB_INDEX_STAT_SAMPLE_INDEX1,
  ~(Uint32)0,
  ~(Uint32)0
};

void
Trix::statMetaGetHead(Signal* signal, StatOp& stat)
{
  D("statMetaGetHead" << V(stat));
  StatOp::Meta& meta = stat.m_meta;
  meta.m_cb.m_callbackFunction = safe_cast(&Trix::statMetaGetHeadCB);
  meta.m_cb.m_callbackData = stat.m_ownPtrI;
  const char* name = g_statMetaHead.name;
  sendGetTabInfoReq(signal, stat, name);
}

void
Trix::statMetaGetHeadCB(Signal* signal, Uint32 statPtrI, Uint32 ret)
{
  StatOp& stat = statOpGetPtr(statPtrI);
  D("statMetaGetHeadCB" << V(stat) << V(ret));
  StatOp::Meta& meta = stat.m_meta;
  if (ret != 0)
  {
    jam();
    Uint32 supress[] = { GetTabInfoRef::TableNotDefined, 0 };
    statOpError(signal, stat, ret, __LINE__, supress);
    return;
  }
  g_statMetaHead.tableId = meta.m_conf.tableId;
  statMetaGetSample(signal, stat);
}

void
Trix::statMetaGetSample(Signal* signal, StatOp& stat)
{
  D("statMetaGetSample" << V(stat));
  StatOp::Meta& meta = stat.m_meta;
  meta.m_cb.m_callbackFunction = safe_cast(&Trix::statMetaGetSampleCB);
  meta.m_cb.m_callbackData = stat.m_ownPtrI;
  const char* name = g_statMetaSample.name;
  sendGetTabInfoReq(signal, stat, name);
}

void
Trix::statMetaGetSampleCB(Signal* signal, Uint32 statPtrI, Uint32 ret)
{
  StatOp& stat = statOpGetPtr(statPtrI);
  D("statMetaGetSampleCB" << V(stat) << V(ret));
  StatOp::Meta& meta = stat.m_meta;
  if (ret != 0)
  {
    jam();
    statOpError(signal, stat, ret, __LINE__);
    return;
  }
  g_statMetaSample.tableId = meta.m_conf.tableId;
  statMetaGetSampleX1(signal, stat);
}

void
Trix::statMetaGetSampleX1(Signal* signal, StatOp& stat)
{
  D("statMetaGetSampleX1" << V(stat));
  StatOp::Meta& meta = stat.m_meta;
  meta.m_cb.m_callbackFunction = safe_cast(&Trix::statMetaGetSampleX1CB);
  meta.m_cb.m_callbackData = stat.m_ownPtrI;
  const char* name_fmt = g_statMetaSampleX1.name;
  char name[MAX_TAB_NAME_SIZE];
  BaseString::snprintf(name, sizeof(name), name_fmt, g_statMetaSample.tableId);
  sendGetTabInfoReq(signal, stat, name);
}

void
Trix::statMetaGetSampleX1CB(Signal* signal, Uint32 statPtrI, Uint32 ret)
{
  StatOp& stat = statOpGetPtr(statPtrI);
  D("statMetaGetSampleX1CB" << V(stat) << V(ret));
  StatOp::Meta& meta = stat.m_meta;
  if (ret != 0)
  {
    jam();
    statOpError(signal, stat, ret, __LINE__);
    return;
  }
  g_statMetaSampleX1.tableId = g_statMetaSample.tableId;
  g_statMetaSampleX1.indexId = meta.m_conf.tableId;
  statGetMetaDone(signal, stat);
}

void
Trix::sendGetTabInfoReq(Signal* signal, StatOp& stat, const char* name)
{
  D("sendGetTabInfoReq" << V(stat) << V(name));
  GetTabInfoReq* req = (GetTabInfoReq*)signal->getDataPtrSend();

  Uint32 name_len = (Uint32)strlen(name) + 1;
  Uint32 name_len_words = (name_len + 3 ) / 4;
  Uint32 name_buf[32];
  ndbrequire(name_len_words <= 32);
  memset(name_buf, 0, sizeof(name_buf));
  memcpy(name_buf, name, name_len);

  req->senderData = stat.m_ownPtrI;
  req->senderRef = reference();
  req->requestType = GetTabInfoReq::RequestByName |
                     GetTabInfoReq::LongSignalConf;
  req->tableNameLen = name_len;
  req->schemaTransId = 0;
  LinearSectionPtr ptr[3];
  ptr[0].p = name_buf;
  ptr[0].sz = name_len_words;
  sendSignal(DBDICT_REF, GSN_GET_TABINFOREQ,
             signal, GetTabInfoReq::SignalLength, JBB, ptr, 1);
}

void
Trix::execGET_TABINFO_CONF(Signal* signal)
{
  jamEntry();
  if (!assembleFragments(signal)) {
    jam();
    return;
  }
  const GetTabInfoConf* conf = (const GetTabInfoConf*)signal->getDataPtr();
  StatOp& stat = statOpGetPtr(conf->senderData);
  D("execGET_TABINFO_CONF" << V(stat));
  StatOp::Meta& meta = stat.m_meta;
  meta.m_conf = *conf;

  // do not need DICTTABINFO
  SectionHandle handle(this, signal);
  releaseSections(handle);

  execute(signal, meta.m_cb, 0);
}

void
Trix::execGET_TABINFO_REF(Signal* signal)
{
  jamEntry();
  const GetTabInfoRef* ref = (const GetTabInfoRef*)signal->getDataPtr();
  StatOp& stat = statOpGetPtr(ref->senderData);
  D("execGET_TABINFO_REF" << V(stat));
  StatOp::Meta& meta = stat.m_meta;

  ndbrequire(ref->errorCode != 0);
  execute(signal, meta.m_cb, ref->errorCode);
}

// continue after metadata retrieval

void
Trix::statGetMetaDone(Signal* signal, StatOp& stat)
{
  const IndexStatImplReq* req = &stat.m_req;
  StatOp::Data& data = stat.m_data;
  SubscriptionRecord* subRec = c_theSubscriptions.getPtr(stat.m_subRecPtrI);
  D("statGetMetaDone" << V(stat));

  // c_statGetMetaDone = true;

  subRec->requestType = STAT_UTIL;
  // fill in constant part
  ndbrequire(req->fragCount != 0);
  data.m_indexId = req->indexId;
  data.m_indexVersion = req->indexVersion;
  data.m_fragCount = req->fragCount;
  statHeadRead(signal, stat);
}

// head table ops

void
Trix::statHeadRead(Signal* signal, StatOp& stat)
{
  StatOp::Util& util = stat.m_util;
  StatOp::Send& send = stat.m_send;
  D("statHeadRead" << V(stat));

  util.m_not_found = false;
  util.m_cb.m_callbackFunction = safe_cast(&Trix::statHeadReadCB);
  util.m_cb.m_callbackData = stat.m_ownPtrI;
  send.m_sysTable = &g_statMetaHead;
  send.m_operationType = UtilPrepareReq::Read;
  statUtilPrepare(signal, stat);
}

void
Trix::statHeadReadCB(Signal* signal, Uint32 statPtrI, Uint32 ret)
{
  StatOp& stat = statOpGetPtr(statPtrI);
  StatOp::Data& data = stat.m_data;
  StatOp::Util& util = stat.m_util;
  D("statHeadReadCB" << V(stat) << V(ret));

  ndbrequire(ret == 0);
  data.m_head_found = !util.m_not_found;
  statReadHeadDone(signal, stat);
}

void
Trix::statHeadInsert(Signal* signal, StatOp& stat)
{
  StatOp::Util& util = stat.m_util;
  StatOp::Send& send = stat.m_send;
  D("statHeadInsert" << V(stat));

  util.m_cb.m_callbackFunction = safe_cast(&Trix::statHeadInsertCB);
  util.m_cb.m_callbackData = stat.m_ownPtrI;
  send.m_sysTable = &g_statMetaHead;
  send.m_operationType = UtilPrepareReq::Insert;
  statUtilPrepare(signal, stat);
}

void
Trix::statHeadInsertCB(Signal* signal, Uint32 statPtrI, Uint32 ret)
{
  StatOp& stat = statOpGetPtr(statPtrI);
  D("statHeadInsertCB" << V(stat) << V(ret));

  ndbrequire(ret == 0);
  statInsertHeadDone(signal, stat);
}

void
Trix::statHeadUpdate(Signal* signal, StatOp& stat)
{
  StatOp::Util& util = stat.m_util;
  StatOp::Send& send = stat.m_send;
  D("statHeadUpdate" << V(stat));

  util.m_cb.m_callbackFunction = safe_cast(&Trix::statHeadUpdateCB);
  util.m_cb.m_callbackData = stat.m_ownPtrI;
  send.m_sysTable = &g_statMetaHead;
  send.m_operationType = UtilPrepareReq::Update;
  statUtilPrepare(signal, stat);
}

void
Trix::statHeadUpdateCB(Signal* signal, Uint32 statPtrI, Uint32 ret)
{
  StatOp& stat = statOpGetPtr(statPtrI);
  D("statHeadUpdateCB" << V(stat) << V(ret));

  ndbrequire(ret == 0);
  statUpdateHeadDone(signal, stat);
}

void
Trix::statHeadDelete(Signal* signal, StatOp& stat)
{
  StatOp::Util& util = stat.m_util;
  StatOp::Send& send = stat.m_send;
  D("statHeadDelete" << V(stat));

  util.m_cb.m_callbackFunction = safe_cast(&Trix::statHeadDeleteCB);
  util.m_cb.m_callbackData = stat.m_ownPtrI;
  send.m_sysTable = &g_statMetaHead;
  send.m_operationType = UtilPrepareReq::Delete;
  statUtilPrepare(signal, stat);
}

void
Trix::statHeadDeleteCB(Signal* signal, Uint32 statPtrI, Uint32 ret)
{
  StatOp& stat = statOpGetPtr(statPtrI);
  D("statHeadDeleteCB" << V(stat) << V(ret));

  ndbrequire(ret == 0);
  statDeleteHeadDone(signal, stat);
}

// util (PK ops, only HEAD for now)

void
Trix::statUtilPrepare(Signal* signal, StatOp& stat)
{
  StatOp::Util& util = stat.m_util;
  D("statUtilPrepare" << V(stat));

  util.m_prepareId = RNIL;
  statSendPrepare(signal, stat);
}

void
Trix::statUtilPrepareConf(Signal* signal, Uint32 statPtrI)
{
  StatOp& stat = statOpGetPtr(statPtrI);
  StatOp::Util& util = stat.m_util;
  StatOp::Send& send = stat.m_send;
  D("statUtilPrepareConf" << V(stat));

  const UtilPrepareConf* utilConf =
    (const UtilPrepareConf*)signal->getDataPtr();
  util.m_prepareId = utilConf->prepareId;

  const Uint32 ot = send.m_operationType;
  if ((ERROR_INSERTED(18011) && ot == UtilPrepareReq::Read) ||
      (ERROR_INSERTED(18012) && ot != UtilPrepareReq::Read))
  {
    jam();
    CLEAR_ERROR_INSERT_VALUE;
    UtilExecuteRef* utilRef =
      (UtilExecuteRef*)signal->getDataPtrSend();
    utilRef->senderData = stat.m_ownPtrI;
    utilRef->errorCode = UtilExecuteRef::AllocationError;
    utilRef->TCErrorCode = 0;
    sendSignal(reference(), GSN_UTIL_EXECUTE_REF,
               signal, UtilExecuteRef::SignalLength, JBB);
    return;
  }

  statUtilExecute(signal, stat);
}

void
Trix::statUtilPrepareRef(Signal* signal, Uint32 statPtrI)
{
  StatOp& stat = statOpGetPtr(statPtrI);
  D("statUtilPrepareRef" << V(stat));

  const UtilPrepareRef* utilRef =
    (const UtilPrepareRef*)signal->getDataPtr();
  Uint32 errorCode = utilRef->errorCode;
  ndbrequire(errorCode != 0);

  switch (errorCode) {
  case UtilPrepareRef::PREPARE_SEIZE_ERROR:
  case UtilPrepareRef::PREPARE_PAGES_SEIZE_ERROR:
  case UtilPrepareRef::PREPARED_OPERATION_SEIZE_ERROR:
    errorCode = IndexStatRef::BusyUtilPrepare;
    break;
  case UtilPrepareRef::DICT_TAB_INFO_ERROR:
    errorCode = IndexStatRef::InvalidSysTable;
    break;
  case UtilPrepareRef::MISSING_PROPERTIES_SECTION:
  default:
    ndbabort();
  }
  statOpError(signal, stat, errorCode, __LINE__);
}

void
Trix::statUtilExecute(Signal* signal, StatOp& stat)
{
  StatOp::Util& util = stat.m_util;
  StatOp::Send& send = stat.m_send;
  D("statUtilExecute" << V(stat));

  send.m_prepareId = util.m_prepareId;
  statSendExecute(signal, stat);
}

void
Trix::statUtilExecuteConf(Signal* signal, Uint32 statPtrI)
{
  StatOp& stat = statOpGetPtr(statPtrI);
  StatOp::Attr& attr = stat.m_attr;
  StatOp::Send& send = stat.m_send;
  D("statUtilExecuteConf" << V(stat));

  if (send.m_operationType == UtilPrepareReq::Read)
  {
    jam();
    SectionHandle handle(this, signal);
    Uint32 rattr[20];
    Uint32 rdata[2048];
    attr.m_attr = rattr;
    attr.m_attrMax = 20;
    attr.m_attrSize = 0;
    attr.m_data = rdata;
    attr.m_dataMax = 2048;
    attr.m_dataSize = 0;
    {
      SegmentedSectionPtr ssPtr;
      handle.getSection(ssPtr, 0);
      ::copy(rattr, ssPtr);
    }
    {
      SegmentedSectionPtr ssPtr;
      handle.getSection(ssPtr, 1);
      ::copy(rdata, ssPtr);
    }
    releaseSections(handle);

    const SysTable& sysTable = *send.m_sysTable;
    for (Uint32 i = 0; i < sysTable.columnCount; i++)
    {
      jam();
      statDataIn(stat, i);
    }
  }

  statUtilRelease(signal, stat);
}

void
Trix::statUtilExecuteRef(Signal* signal, Uint32 statPtrI)
{
  StatOp& stat = statOpGetPtr(statPtrI);
  StatOp::Util& util = stat.m_util;
  StatOp::Send& send = stat.m_send;
  D("statUtilExecuteRef" << V(stat));

  const UtilExecuteRef* utilRef =
    (const UtilExecuteRef*)signal->getDataPtr();
  Uint32 errorCode = utilRef->errorCode;
  ndbrequire(errorCode != 0);

  switch (errorCode) {
  case UtilExecuteRef::TCError:
    errorCode = utilRef->TCErrorCode;
    ndbrequire(errorCode != 0);
    if (send.m_operationType == UtilPrepareReq::Read &&
        errorCode == ZNOT_FOUND)
    {
      jam();
      util.m_not_found = true;
      errorCode = 0;
    }
    break;
  case UtilExecuteRef::AllocationError:
    errorCode = IndexStatRef::BusyUtilExecute;
    break;
  default:
    ndbabort();
  }

  if (errorCode != 0)
  {
    jam();
    statOpError(signal, stat, errorCode, __LINE__);
    return;
  }
  statUtilRelease(signal, stat);
}

void
Trix::statUtilRelease(Signal* signal, StatOp& stat)
{
  StatOp::Util& util = stat.m_util;
  StatOp::Send& send = stat.m_send;
  D("statUtilRelease" << V(stat));

  send.m_prepareId = util.m_prepareId;
  statSendRelease(signal, stat);
}

void
Trix::statUtilReleaseConf(Signal* signal, Uint32 statPtrI)
{
  StatOp& stat = statOpGetPtr(statPtrI);
  StatOp::Util& util = stat.m_util;
  D("statUtilReleaseConf" << V(stat));

  util.m_prepareId = RNIL;
  execute(signal, util.m_cb, 0);
}

// continue after head table ops

void
Trix::statReadHeadDone(Signal* signal, StatOp& stat)
{
  //UNUSED StatOp::Data& data = stat.m_data;
  D("statReadHeadDone" << V(stat));

  switch (stat.m_requestType) {
  case IndexStatReq::RT_CLEAN_NEW:
    jam();
    // Fall through
  case IndexStatReq::RT_CLEAN_OLD:
    jam();
    // Fall through
  case IndexStatReq::RT_CLEAN_ALL:
    jam();
    statCleanBegin(signal, stat);
    break;

  case IndexStatReq::RT_SCAN_FRAG:
    jam();
    statScanBegin(signal, stat);
    break;

  case IndexStatReq::RT_DROP_HEAD:
    jam();
    statDropBegin(signal, stat);
    break;

  default:
    ndbabort();
  }
}

void
Trix::statInsertHeadDone(Signal* signal, StatOp& stat)
{
  D("statInsertHeadDone" << V(stat));

  switch (stat.m_requestType) {
  case IndexStatReq::RT_SCAN_FRAG:
    jam();
    statScanEnd(signal, stat);
    break;
  default:
    ndbabort();
  }
}

void
Trix::statUpdateHeadDone(Signal* signal, StatOp& stat)
{
  D("statUpdateHeadDone" << V(stat));

  switch (stat.m_requestType) {
  case IndexStatReq::RT_SCAN_FRAG:
    jam();
    statScanEnd(signal, stat);
    break;
  default:
    ndbabort();
  }
}

void
Trix::statDeleteHeadDone(Signal* signal, StatOp& stat)
{
  D("statDeleteHeadDone" << V(stat));

  switch (stat.m_requestType) {
  case IndexStatReq::RT_DROP_HEAD:
    jam();
    statDropEnd(signal, stat);
    break;
  default:
    ndbabort();
  }
}

// clean

void
Trix::statCleanBegin(Signal* signal, StatOp& stat)
{
  const IndexStatImplReq* req = &stat.m_req;
  StatOp::Data& data = stat.m_data;
  D("statCleanBegin" << V(stat));

  if (data.m_head_found == true)
  {
    jam();
    if (data.m_tableId != req->tableId &&
        stat.m_requestType != IndexStatReq::RT_CLEAN_ALL)
    {
      jam();
      // must run ndb_index_stat --drop
      statOpError(signal, stat, IndexStatRef::InvalidSysTableData, __LINE__);
      return;
    }
  }
  else
  {
    if (stat.m_requestType != IndexStatReq::RT_CLEAN_ALL)
    {
      jam();
      // happens normally on first stats scan
      stat.m_requestType = IndexStatReq::RT_CLEAN_ALL;
    }
  }
  statCleanPrepare(signal, stat);
}

void
Trix::statCleanPrepare(Signal* signal, StatOp& stat)
{
  const IndexStatImplReq* req = &stat.m_req;
  StatOp::Data& data = stat.m_data;
  StatOp::Clean& clean = stat.m_clean;
  StatOp::Send& send = stat.m_send;
  SubscriptionRecord* subRec = c_theSubscriptions.getPtr(stat.m_subRecPtrI);
  D("statCleanPrepare" << V(stat));

  // count of deleted samples is just for info
  clean.m_cleanCount = 0;

  const Uint32 ao_list[] = {
    0,  // INDEX_ID
    1,  // INDEX_VERSION
    2,  // SAMPLE_VERSION
    3   // STAT_KEY
  };
  const Uint32 ao_size = sizeof(ao_list)/sizeof(ao_list[0]);

  ndbrequire(req->fragId == ZNIL);
  subRec->m_flags = 0;
  subRec->requestType = STAT_CLEAN;
  subRec->schemaTransId = req->transId;
  subRec->userReference = 0; // not used
  subRec->connectionPtr = RNIL;
  subRec->subscriptionId = rand();
  subRec->subscriptionKey = rand();
  subRec->prepareId = RNIL;
  subRec->indexType = 0; // not used
  subRec->sourceTableId = g_statMetaSampleX1.indexId;
  subRec->targetTableId = RNIL;
  subRec->noOfIndexColumns = ao_size;
  subRec->noOfKeyColumns = 0;
  subRec->parallelism = 16;  // remains hardcoded for now
  subRec->fragCount = 0;
  subRec->fragId = ZNIL;
  subRec->syncPtr = RNIL;
  subRec->errorCode = BuildIndxRef::NoError;
  subRec->subscriptionCreated = false;
  subRec->pendingSubSyncContinueConf = false;
  subRec->expectedConf = 0;
  subRec->m_rows_processed = 0;
  subRec->m_gci = 0;

  AttrOrderBuffer& ao_buf = subRec->attributeOrder;
  ndbrequire(ao_buf.isEmpty());
  ao_buf.append(ao_list, ao_size);

  // create TUX bounds
  clean.m_bound[0] = TuxBoundInfo::BoundEQ;
  clean.m_bound[1] = AttributeHeader(0, 4).m_value;
  clean.m_bound[2] = data.m_indexId;
  clean.m_bound[3] = TuxBoundInfo::BoundEQ;
  clean.m_bound[4] = AttributeHeader(1, 4).m_value;
  clean.m_bound[5] = data.m_indexVersion;
  Uint32 boundCount;
  switch (stat.m_requestType) {
  case IndexStatReq::RT_CLEAN_NEW:
    D("statCleanPrepare delete sample versions > " << data.m_sampleVersion);
    clean.m_bound[6] = TuxBoundInfo::BoundLT;
    clean.m_bound[7] = AttributeHeader(2, 4).m_value;
    clean.m_bound[8] = data.m_sampleVersion;
    boundCount = 3;
    break;
  case IndexStatReq::RT_CLEAN_OLD:
    D("statCleanPrepare delete sample versions < " << data.m_sampleVersion);
    clean.m_bound[6] = TuxBoundInfo::BoundGT;
    clean.m_bound[7] = AttributeHeader(2, 4).m_value;
    clean.m_bound[8] = data.m_sampleVersion;
    boundCount = 3;
    break;
  case IndexStatReq::RT_CLEAN_ALL:
    D("statCleanPrepare delete all sample versions");
    boundCount = 2;
    break;
  default:
    boundCount = 0; /* Silence compiler warning */
    ndbabort();
  }
  clean.m_boundSize = 3 * boundCount;

  // TRIX traps the CONF
  send.m_sysTable = &g_statMetaSample;
  send.m_operationType = UtilPrepareReq::Delete;
  statSendPrepare(signal, stat);
}

void
Trix::statCleanExecute(Signal* signal, StatOp& stat)
{
  StatOp::Data& data = stat.m_data;
  StatOp::Send& send = stat.m_send;
  StatOp::Clean& clean = stat.m_clean;
  SubscriptionRecord* subRec = c_theSubscriptions.getPtr(stat.m_subRecPtrI);
  D("statCleanExecute" << V(stat));

  CRASH_INSERTION(18025);

  SectionHandle handle(this, signal);
  ndbrequire(handle.m_cnt == 2);

  // ATTR_INFO
  AttributeHeader ah[4];
  SegmentedSectionPtr ptr0;
  handle.getSection(ptr0, SubTableData::ATTR_INFO);
  ndbrequire(ptr0.sz == 4);
  ::copy((Uint32*)ah, ptr0);
  ndbrequire(ah[0].getAttributeId() == 0 && ah[0].getDataSize() == 1);
  ndbrequire(ah[1].getAttributeId() == 1 && ah[1].getDataSize() == 1);
  ndbrequire(ah[2].getAttributeId() == 2 && ah[2].getDataSize() == 1);
  // read via TUP rounds bytes to words
  const Uint32 kz = ah[3].getDataSize();
  ndbrequire(ah[3].getAttributeId() == 3 && kz != 0);

  // AFTER_VALUES
  // avmax = other pk attributes + length + max index stat key size
  const Uint32 avmax = 3 + 1 + MAX_INDEX_STAT_KEY_SIZE;
  Uint32 av[avmax];
  SegmentedSectionPtr ptr1;
  handle.getSection(ptr1, SubTableData::AFTER_VALUES);
  ndbrequire(ptr1.sz <= avmax);
  ::copy(av, ptr1);
  ndbrequire(data.m_indexId == av[0]);
  ndbrequire(data.m_indexVersion == av[1]);
  data.m_sampleVersion = av[2];
  data.m_statKey = &av[3];
  const unsigned char* kp = (const unsigned char*)data.m_statKey;
  const Uint32 kb = kp[0] + (kp[1] << 8);
  // key is not empty
  ndbrequire(kb != 0);
  ndbrequire(kz == ((2 + kb) + 3) / 4);

  clean.m_cleanCount++;
  releaseSections(handle);

  const Uint32 rt = stat.m_requestType;
  if ((ERROR_INSERTED(18021) && rt == IndexStatReq::RT_CLEAN_NEW) ||
      (ERROR_INSERTED(18022) && rt == IndexStatReq::RT_CLEAN_OLD) ||
      (ERROR_INSERTED(18023) && rt == IndexStatReq::RT_CLEAN_ALL))
  {
    jam();
    CLEAR_ERROR_INSERT_VALUE;
    UtilExecuteRef* utilRef =
      (UtilExecuteRef*)signal->getDataPtrSend();
    utilRef->senderData = stat.m_ownPtrI;
    utilRef->errorCode = UtilExecuteRef::TCError;
    utilRef->TCErrorCode = 626;
    sendSignal(reference(), GSN_UTIL_EXECUTE_REF,
               signal, UtilExecuteRef::SignalLength, JBB);
    subRec->expectedConf++;
    return;
  }

  // TRIX traps the CONF
  send.m_sysTable = &g_statMetaSample;
  send.m_operationType = UtilPrepareReq::Delete;
  send.m_prepareId = subRec->prepareId;
  subRec->expectedConf++;
  statSendExecute(signal, stat);
}

void
Trix::statCleanRelease(Signal* signal, StatOp& stat)
{
  SubscriptionRecord* subRec = c_theSubscriptions.getPtr(stat.m_subRecPtrI);
  D("statCleanRelease" << V(stat) << V(subRec->errorCode));

  if (subRec->errorCode != 0)
  {
    jam();
    statOpError(signal, stat, subRec->errorCode, __LINE__);
    return;
  }
  statCleanEnd(signal, stat);
}

void
Trix::statCleanEnd(Signal* signal, StatOp& stat)
{
  D("statCleanEnd" << V(stat));
  statOpSuccess(signal, stat);
}

// scan

void
Trix::statScanBegin(Signal* signal, StatOp& stat)
{
  const IndexStatImplReq* req = &stat.m_req;
  StatOp::Data& data = stat.m_data;
  D("statScanBegin" << V(stat));

  if (data.m_head_found == true &&
      data.m_tableId != req->tableId)
  {
    jam();
    statOpError(signal, stat, IndexStatRef::InvalidSysTableData, __LINE__);
    return;
  }
  data.m_tableId = req->tableId;
  statScanPrepare(signal, stat);
}

void
Trix::statScanPrepare(Signal* signal, StatOp& stat)
{
  const IndexStatImplReq* req = &stat.m_req;
  StatOp::Data& data = stat.m_data;
  StatOp::Scan& scan = stat.m_scan;
  StatOp::Send& send = stat.m_send;
  SubscriptionRecord* subRec = c_theSubscriptions.getPtr(stat.m_subRecPtrI);
  D("statScanPrepare" << V(stat));

  // update sample version prior to scan
  if (data.m_head_found == false)
    data.m_sampleVersion = 0;
  data.m_sampleVersion += 1;

  // zero totals
  scan.m_sampleCount = 0;
  scan.m_keyBytes = 0;

  const Uint32 ao_list[] = {
    AttributeHeader::INDEX_STAT_KEY,
    AttributeHeader::INDEX_STAT_VALUE
  };
  const Uint32 ao_size = sizeof(ao_list)/sizeof(ao_list[0]);

  ndbrequire(req->fragId != ZNIL);
  subRec->m_flags = 0;
  subRec->requestType = STAT_SCAN;
  subRec->schemaTransId = req->transId;
  subRec->userReference = 0; // not used
  subRec->connectionPtr = RNIL;
  subRec->subscriptionId = rand();
  subRec->subscriptionKey = rand();
  subRec->prepareId = RNIL;
  subRec->indexType = 0; // not used
  subRec->sourceTableId = data.m_indexId;
  subRec->targetTableId = RNIL;
  subRec->noOfIndexColumns = ao_size;
  subRec->noOfKeyColumns = 0;
  subRec->parallelism = 16;   // remains hardcoded for now
  subRec->fragCount = 0; // XXX Suma currently checks all frags
  subRec->fragId = req->fragId;
  subRec->syncPtr = RNIL;
  subRec->errorCode = BuildIndxRef::NoError;
  subRec->subscriptionCreated = false;
  subRec->pendingSubSyncContinueConf = false;
  subRec->expectedConf = 0;
  subRec->m_rows_processed = 0;
  subRec->m_gci = 0;

  AttrOrderBuffer& ao_buf = subRec->attributeOrder;
  ndbrequire(ao_buf.isEmpty());
  ao_buf.append(ao_list, ao_size);

  // TRIX traps the CONF
  send.m_sysTable = &g_statMetaSample;
  send.m_operationType = UtilPrepareReq::Insert;
  statSendPrepare(signal, stat);
}

void
Trix::statScanExecute(Signal* signal, StatOp& stat)
{
  StatOp::Data& data = stat.m_data;
  StatOp::Scan& scan = stat.m_scan;
  StatOp::Send& send = stat.m_send;
  SubscriptionRecord* subRec = c_theSubscriptions.getPtr(stat.m_subRecPtrI);
  D("statScanExecute" << V(stat));

  CRASH_INSERTION(18026);

  SectionHandle handle(this, signal);
  ndbrequire(handle.m_cnt == 2);

  // ATTR_INFO
  AttributeHeader ah[2];
  SegmentedSectionPtr ptr0;
  handle.getSection(ptr0, SubTableData::ATTR_INFO);
  ndbrequire(ptr0.sz == 2);
  ::copy((Uint32*)ah, ptr0);
  ndbrequire(ah[0].getAttributeId() == AttributeHeader::INDEX_STAT_KEY);
  ndbrequire(ah[1].getAttributeId() == AttributeHeader::INDEX_STAT_VALUE);
  // read via TUP rounds bytes to words
  const Uint32 kz = ah[0].getDataSize();
  const Uint32 vz = ah[1].getDataSize();
  ndbrequire(kz != 0 && vz != 0);

  // AFTER_VALUES
  // avmax = length + max key size + length + max value size
  const Uint32 avmax = 2 + MAX_INDEX_STAT_KEY_SIZE + MAX_INDEX_STAT_VALUE_SIZE;
  Uint32 av[avmax];
  SegmentedSectionPtr ptr1;
  handle.getSection(ptr1, SubTableData::AFTER_VALUES);
  ndbrequire(ptr1.sz <= avmax);
  ::copy(av, ptr1);
  data.m_statKey = &av[0];
  data.m_statValue = &av[kz];
  const unsigned char* kp = (const unsigned char*)data.m_statKey;
  const unsigned char* vp = (const unsigned char*)data.m_statValue;
  const Uint32 kb = kp[0] + (kp[1] << 8);
  const Uint32 vb = vp[0] + (vp[1] << 8);
  // key and value are not empty
  ndbrequire(kb != 0 && vb != 0);
  ndbrequire(kz == ((2 + kb) + 3) / 4);
  ndbrequire(vz == ((2 + vb) + 3) / 4);

  scan.m_sampleCount++;
  scan.m_keyBytes += kb;
  releaseSections(handle);

  if (ERROR_INSERTED(18024))
  {
    jam();
    CLEAR_ERROR_INSERT_VALUE;
    UtilExecuteRef* utilRef =
      (UtilExecuteRef*)signal->getDataPtrSend();
    utilRef->senderData = stat.m_ownPtrI;
    utilRef->errorCode = UtilExecuteRef::TCError;
    utilRef->TCErrorCode = 630;
    sendSignal(reference(), GSN_UTIL_EXECUTE_REF,
               signal, UtilExecuteRef::SignalLength, JBB);
    subRec->expectedConf++;
    return;
  }

  // TRIX traps the CONF
  send.m_sysTable = &g_statMetaSample;
  send.m_operationType = UtilPrepareReq::Insert;
  send.m_prepareId = subRec->prepareId;
  subRec->expectedConf++;
  statSendExecute(signal, stat);
}

void
Trix::statScanRelease(Signal* signal, StatOp& stat)
{
  StatOp::Data& data = stat.m_data;
  StatOp::Scan& scan = stat.m_scan;
  SubscriptionRecord* subRec = c_theSubscriptions.getPtr(stat.m_subRecPtrI);
  D("statScanRelease" << V(stat) << V(subRec->errorCode));

  if (subRec->errorCode != 0)
  {
    jam();
    statOpError(signal, stat, subRec->errorCode, __LINE__);
    return;
  }
  subRec->requestType = STAT_UTIL;

  const Uint32 now = (Uint32)time(0);
  data.m_loadTime = now;
  data.m_sampleCount = scan.m_sampleCount;
  data.m_keyBytes = scan.m_keyBytes;
  data.m_valueFormat = MAX_INDEX_STAT_VALUE_FORMAT;

  if (data.m_head_found == false)
  {
    jam();
    statHeadInsert(signal, stat);
  }
  else
  {
    jam();
    statHeadUpdate(signal, stat);
  }
}

void
Trix::statScanEnd(Signal* signal, StatOp& stat)
{
  StatOp::Data& data = stat.m_data;
  const IndexStatImplReq* req = &stat.m_req;
  D("statScanEnd" << V(stat));

  /*
   * TRIX reports stats load time to TUX for proper stats monitoring.
   * Passing this via DBDICT RT_START_MON is not feasible.  For MT-LQH
   * we prefer DbtuxProxy to avoid introducing MT-LQH into TRIX.
   */

#ifdef trix_index_stat_rep_to_tux_instance
  Uint32 instanceKey = getInstanceKey(req->indexId, req->fragId);
  BlockReference tuxRef = numberToRef(DBTUX, instanceKey, getOwnNodeId());
#else
  BlockReference tuxRef = DBTUX_REF;
#endif

  IndexStatRep* rep = (IndexStatRep*)signal->getDataPtrSend();
  rep->senderRef = reference();
  rep->senderData = 0;
  rep->requestType = IndexStatRep::RT_UPDATE_CONF;
  rep->requestFlag = 0;
  rep->indexId = req->indexId;
  rep->indexVersion = req->indexVersion;
  rep->tableId = req->tableId;
  rep->fragId = req->fragId;
  rep->loadTime = data.m_loadTime;
  sendSignal(tuxRef, GSN_INDEX_STAT_REP,
             signal, IndexStatRep::SignalLength, JBB);

  statOpSuccess(signal, stat);
}

// drop

void
Trix::statDropBegin(Signal* signal, StatOp& stat)
{
  StatOp::Data& data = stat.m_data;
  D("statDropBegin" << V(stat));

  if (data.m_head_found == true)
  {
    jam();
    statHeadDelete(signal, stat);
    return;
  }
  statDropEnd(signal, stat);
}

void
Trix::statDropEnd(Signal* signal, StatOp& stat)
{
  D("statDropEnd");
  statOpSuccess(signal, stat);
}

// send

void
Trix::statSendPrepare(Signal* signal, StatOp& stat)
{
  StatOp::Send& send = stat.m_send;
  const IndexStatImplReq* req = &stat.m_req;
  const SysTable& sysTable = *send.m_sysTable;
  D("statSendPrepare" << V(stat));

  UtilPrepareReq* utilReq =
    (UtilPrepareReq*)signal->getDataPtrSend();
  utilReq->senderData = stat.m_ownPtrI;
  utilReq->senderRef = reference();
  utilReq->schemaTransId = req->transId;

  Uint32 wbuf[256];
  LinearWriter w(&wbuf[0], sizeof(wbuf) >> 2);

  w.first();
  w.add(UtilPrepareReq::NoOfOperations, 1);
  w.add(UtilPrepareReq::OperationType, send.m_operationType);
  w.add(UtilPrepareReq::TableId, sysTable.tableId);

  Uint32 i;
  for (i = 0; i < sysTable.columnCount; i++) {
    const SysColumn& c = sysTable.columnList[i];
    switch (send.m_operationType) {
    case UtilPrepareReq::Read:
    case UtilPrepareReq::Insert:
    case UtilPrepareReq::Update:
      jam();
      w.add(UtilPrepareReq::AttributeId, i);
      break;
    case UtilPrepareReq::Delete:
      jam();
      if (c.keyFlag)
        w.add(UtilPrepareReq::AttributeId, i);
      break;
    default:
      ndbabort();
    }
  }

  LinearSectionPtr ptr[3];
  ptr[0].p = &wbuf[0];
  ptr[0].sz = w.getWordsUsed();
  sendSignal(DBUTIL_REF, GSN_UTIL_PREPARE_REQ,
             signal, UtilPrepareReq::SignalLength, JBB, ptr, 1);
}

void
Trix::statSendExecute(Signal* signal, StatOp& stat)
{
  D("statSendExecute" << V(stat));
  StatOp::Send& send = stat.m_send;
  StatOp::Attr& attr = stat.m_attr;
  const SysTable& sysTable = *send.m_sysTable;

  UtilExecuteReq* utilReq =
    (UtilExecuteReq*)signal->getDataPtrSend();
  utilReq->senderData = stat.m_ownPtrI;
  utilReq->senderRef = reference();
  utilReq->prepareId = send.m_prepareId;
  utilReq->scanTakeOver = 0;

  Uint32 wattr[20];
  Uint32 wdata[2048];
  attr.m_attr = wattr;
  attr.m_attrMax = 20;
  attr.m_attrSize = 0;
  attr.m_data = wdata;
  attr.m_dataMax = 2048;
  attr.m_dataSize = 0;

  for (Uint32 i = 0; i < sysTable.columnCount; i++) {
    const SysColumn& c = sysTable.columnList[i];
    switch (send.m_operationType) {
    case UtilPrepareReq::Read:
    case UtilPrepareReq::Insert:
    case UtilPrepareReq::Update:
      jam();
      statDataOut(stat, i);
      break;
    case UtilPrepareReq::Delete:
      jam();
      if (c.keyFlag)
        statDataOut(stat, i);
      break;
    default:
      ndbabort();
    }
  }

  LinearSectionPtr ptr[3];
  ptr[0].p = attr.m_attr;
  ptr[0].sz = attr.m_attrSize;
  ptr[1].p = attr.m_data;
  ptr[1].sz = attr.m_dataSize;
  sendSignal(DBUTIL_REF, GSN_UTIL_EXECUTE_REQ,
             signal, UtilExecuteReq::SignalLength, JBB, ptr, 2);
}

void
Trix::statSendRelease(Signal* signal, StatOp& stat)
{
  D("statSendRelease" << V(stat));
  StatOp::Send& send = stat.m_send;
  ndbrequire(send.m_prepareId != RNIL);

  UtilReleaseReq* utilReq =
    (UtilReleaseReq*)signal->getDataPtrSend();
  utilReq->senderData = stat.m_ownPtrI;
  utilReq->prepareId = send.m_prepareId;
  sendSignal(DBUTIL_REF, GSN_UTIL_RELEASE_REQ,
             signal, UtilReleaseReq::SignalLength, JBB);
}

// data

void
Trix::statDataPtr(StatOp& stat, Uint32 i, Uint32*& dptr, Uint32& bytes)
{
  StatOp::Data& data = stat.m_data;
  StatOp::Send& send = stat.m_send;

  const SysTable& sysTable = *send.m_sysTable;
  ndbrequire(i < sysTable.columnCount);
  //UNUSED const SysColumn& c = sysTable.columnList[i];

  if (&sysTable == &g_statMetaHead)
  {
    switch (i) {
    case 0:
      dptr = &data.m_indexId;
      bytes = 4;
      break;
    case 1:
      dptr = &data.m_indexVersion;
      bytes = 4;
      break;
    case 2:
      dptr = &data.m_tableId;
      bytes = 4;
      break;
    case 3:
      dptr = &data.m_fragCount;
      bytes = 4;
      break;
    case 4:
      dptr = &data.m_valueFormat;
      bytes = 4;
      break;
    case 5:
      dptr = &data.m_sampleVersion;
      bytes = 4;
      break;
    case 6:
      dptr = &data.m_loadTime;
      bytes = 4;
      break;
    case 7:
      dptr = &data.m_sampleCount;
      bytes = 4;
      break;
    case 8:
      dptr = &data.m_keyBytes;
      bytes = 4;
      break;
    default:
      ndbabort();
    }
    return;
  }

  if (&sysTable == &g_statMetaSample)
  {
    switch (i) {
    case 0:
      dptr = &data.m_indexId;
      bytes = 4;
      break;
    case 1:
      dptr = &data.m_indexVersion;
      bytes = 4;
      break;
    case 2:
      dptr = &data.m_sampleVersion;
      bytes = 4;
      break;
    case 3:
      {
        dptr = data.m_statKey;
        const uchar* p = (uchar*)dptr;
        ndbrequire(p != 0);
        bytes = 2 + p[0] + (p[1] << 8);
      }
      break;
    case 4:
      {
        dptr = data.m_statValue;
        const uchar* p = (uchar*)dptr;
        ndbrequire(p != 0);
        bytes = 2 + p[0] + (p[1] << 8);
      }
      break;
    default:
      ndbabort();
    }
    return;
  }

  ndbabort();
}

void
Trix::statDataOut(StatOp& stat, Uint32 i)
{
  StatOp::Attr& attr = stat.m_attr;
  Uint32* dptr = 0;
  Uint32 bytes = 0;
  statDataPtr(stat, i, dptr, bytes);

  ndbrequire(attr.m_attrSize + 1 <= attr.m_attrMax);
  AttributeHeader::init(&attr.m_attr[attr.m_attrSize], i, bytes);
  attr.m_attrSize++;

  Uint32 words = (bytes + 3) / 4;
  ndbrequire(attr.m_dataSize + words <= attr.m_dataMax);
  Uint8* dst = (Uint8*)&attr.m_data[attr.m_dataSize];
  memcpy(dst, dptr, bytes);
  while (bytes < words * 4)
    dst[bytes++] = 0;
  attr.m_dataSize += words;
  D("statDataOut" << V(i) << V(bytes) << hex << V(dptr[0]));
}

void
Trix::statDataIn(StatOp& stat, Uint32 i)
{
  StatOp::Attr& attr = stat.m_attr;
  Uint32* dptr = 0;
  Uint32 bytes = 0;
  statDataPtr(stat, i, dptr, bytes);

  ndbrequire(attr.m_attrSize + 1 <= attr.m_attrMax);
  const AttributeHeader& ah = attr.m_attr[attr.m_attrSize];
  attr.m_attrSize++;

  ndbrequire(ah.getByteSize() == bytes);
  Uint32 words = (bytes + 3) / 4;
  ndbrequire(attr.m_dataSize + words <= attr.m_dataMax);
  const char* src = (const char*)&attr.m_data[attr.m_dataSize];
  memcpy(dptr, src, bytes);
  attr.m_dataSize += words;
  D("statDataIn" << V(i) << V(bytes) << hex << V(dptr[0]));
}

// abort ongoing

void
Trix::statAbortUtil(Signal* signal, StatOp& stat)
{
  StatOp::Util& util = stat.m_util;
  D("statAbortUtil" << V(stat));

  ndbrequire(util.m_prepareId != RNIL);
  util.m_cb.m_callbackFunction = safe_cast(&Trix::statAbortUtilCB);
  util.m_cb.m_callbackData = stat.m_ownPtrI;
  statUtilRelease(signal, stat);
}

void
Trix::statAbortUtilCB(Signal* signal, Uint32 statPtrI, Uint32 ret)
{
  StatOp& stat = statOpGetPtr(statPtrI);
  D("statAbortUtilCB" << V(stat) << V(ret));

  ndbrequire(ret == 0);
  statOpAbort(signal, stat);
}

// conf and ref

void
Trix::statOpSuccess(Signal* signal, StatOp& stat)
{
  StatOp::Data& data = stat.m_data;
  D("statOpSuccess" << V(stat));

  if (stat.m_requestType == IndexStatReq::RT_SCAN_FRAG)
    statOpEvent(stat, "I", "created %u samples", data.m_sampleCount);

  statOpConf(signal, stat);
  statOpRelease(stat);
}

void
Trix::statOpConf(Signal* signal, StatOp& stat)
{
  const IndexStatImplReq* req = &stat.m_req;
  D("statOpConf" << V(stat));

  IndexStatImplConf* conf = (IndexStatImplConf*)signal->getDataPtrSend();
  conf->senderRef = reference();
  conf->senderData = req->senderData;
  sendSignal(req->senderRef, GSN_INDEX_STAT_IMPL_CONF,
             signal, IndexStatImplConf::SignalLength, JBB);
}

void
Trix::statOpError(Signal* signal, StatOp& stat,
                  Uint32 errorCode, Uint32 errorLine,
                  const Uint32 * supress)
{
  D("statOpError" << V(stat) << V(errorCode) << V(errorLine));

  if (supress)
  {
    for (Uint32 i = 0; supress[i] != 0; i++)
    {
      if (errorCode == supress[i])
      {
        goto do_supress;
      }
    }
  }
  statOpEvent(stat, "W", "error %u line %u", errorCode, errorLine);

do_supress:
  ndbrequire(stat.m_errorCode == 0);
  stat.m_errorCode = errorCode;
  stat.m_errorLine = errorLine;
  statOpAbort(signal, stat);
}

void
Trix::statOpAbort(Signal* signal, StatOp& stat)
{
  StatOp::Util& util = stat.m_util;
  D("statOpAbort" << V(stat));
  
  if (util.m_prepareId != RNIL)
  {
    jam();
    // returns here when done
    statAbortUtil(signal, stat);
    return;
  }
  statOpRef(signal, stat);
  statOpRelease(stat);
}

void
Trix::statOpRef(Signal* signal, StatOp& stat)
{
  const IndexStatImplReq* req = &stat.m_req;
  D("statOpRef" << V(stat));

  statOpRef(signal, req, stat.m_errorCode, stat.m_errorLine);
}

void
Trix::statOpRef(Signal* signal, const IndexStatImplReq* req,
                Uint32 errorCode, Uint32 errorLine)
{
  D("statOpRef" << V(errorCode) << V(errorLine));

  IndexStatImplRef* ref = (IndexStatImplRef*)signal->getDataPtrSend();
  ref->senderRef = reference();
  ref->senderData = req->senderData;
  ref->errorCode = errorCode;
  ref->errorLine = errorLine;
  sendSignal(req->senderRef, GSN_INDEX_STAT_IMPL_REF,
             signal, IndexStatImplRef::SignalLength, JBB);
}

void
Trix::statOpEvent(StatOp& stat, const char* level, const char* msg, ...)
{
  //UNUSED const IndexStatImplReq* req = &stat.m_req;
  StatOp::Data& data = stat.m_data;

  char tmp1[100];
  va_list ap;
  va_start(ap, msg);
  BaseString::vsnprintf(tmp1, sizeof(tmp1), msg, ap);
  va_end(ap);

  char tmp2[100];
  BaseString::snprintf(tmp2, sizeof(tmp2),
                       "index %u stats version %u: %s: %s",
                       data.m_indexId, data.m_sampleVersion,
                       stat.m_requestName, tmp1);

  D("statOpEvent" << V(level) << V(tmp2));

  if (level[0] == 'I')
    infoEvent("%s", tmp2);
  if (level[0] == 'W')
    warningEvent("%s", tmp2);
}

// debug

class NdbOut&
operator<<(NdbOut& out, const Trix::StatOp& stat)
{
  out << "[";
  out << " i:" << stat.m_ownPtrI;
  out << " head_found:" << stat.m_data.m_head_found;
  out << " ]";
  return out;
}


BLOCK_FUNCTIONS(Trix)

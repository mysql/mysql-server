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

#include "Trix.hpp"

#include <string.h>
#include <kernel_types.h>
#include <NdbOut.hpp>

#include <signaldata/ReadNodesConf.hpp>
#include <signaldata/NodeFailRep.hpp>
#include <signaldata/DumpStateOrd.hpp>
#include <signaldata/GetTabInfo.hpp>
#include <signaldata/DictTabInfo.hpp>
#include <signaldata/BuildIndx.hpp>
#include <signaldata/SumaImpl.hpp>
#include <signaldata/UtilPrepare.hpp>
#include <signaldata/UtilExecute.hpp>
#include <signaldata/UtilRelease.hpp>
#include <SectionReader.hpp>
#include <AttributeHeader.hpp>

#define CONSTRAINT_VIOLATION 893

#define DEBUG(x) { ndbout << "TRIX::" << x << endl; }

/**
 *
 */
Trix::Trix(const Configuration & conf) :
  SimulatedBlock(TRIX, conf),
  c_theNodes(c_theNodeRecPool),
  c_masterNodeId(0),
  c_masterTrixRef(0),
  c_noNodesFailed(0),
  c_noActiveNodes(0),
  c_theSubscriptions(c_theSubscriptionRecPool)
{
  BLOCK_CONSTRUCTOR(Trix);

  // Add received signals
  addRecSignal(GSN_STTOR,  &Trix::execSTTOR);
  addRecSignal(GSN_NDB_STTOR,  &Trix::execNDB_STTOR); // Forwarded from DICT
  addRecSignal(GSN_READ_NODESCONF, &Trix::execREAD_NODESCONF);
  addRecSignal(GSN_READ_NODESREF, &Trix::execREAD_NODESREF);
  addRecSignal(GSN_NODE_FAILREP, &Trix::execNODE_FAILREP);
  addRecSignal(GSN_INCL_NODEREQ, &Trix::execINCL_NODEREQ);
  addRecSignal(GSN_DUMP_STATE_ORD, &Trix::execDUMP_STATE_ORD);

  // Index build
  addRecSignal(GSN_BUILDINDXREQ, &Trix::execBUILDINDXREQ);
  // Dump testing
  addRecSignal(GSN_BUILDINDXCONF, &Trix::execBUILDINDXCONF);
  addRecSignal(GSN_BUILDINDXREF, &Trix::execBUILDINDXREF);


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
  addRecSignal(GSN_SUB_META_DATA, &Trix::execSUB_META_DATA);
  addRecSignal(GSN_SUB_TABLE_DATA, &Trix::execSUB_TABLE_DATA);

  // Allocate pool sizes
  c_theAttrOrderBufferPool.setSize(100);
  c_theSubscriptionRecPool.setSize(100);

  ArrayList<SubscriptionRecord> subscriptions(c_theSubscriptionRecPool);
  SubscriptionRecPtr subptr;
  while(subscriptions.seize(subptr) == true) {
    new (subptr.p) SubscriptionRecord(c_theAttrOrderBufferPool);
  }
  subscriptions.release();
}

/**
 *
 */
Trix::~Trix()
{
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
    if(NodeBitmask::get(readNodes->allNodes, i)) {
      // Node is defined
      jam();
      ndbrequire(c_theNodes.seizeId(nodeRecPtr, i));
      nodeRecPtr.p->trixRef = calcTrixBlockRef(i);
      if (i == c_masterNodeId) {
        c_masterTrixRef = nodeRecPtr.p->trixRef;
      }
      if(NodeBitmask::get(readNodes->inactiveNodes, i)){
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
    if(NodeBitmask::get(nodeFail->theNodes, nodeRecPtr.i)) {
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
    BuildIndxReq * buildIndxReq = (BuildIndxReq *)signal->getDataPtrSend();
    
    MEMCOPY_NO_WORDS(buildIndxReq, 
		     signal->theData + 1, 
		     BuildIndxReq::SignalLength);
    buildIndxReq->setUserRef(reference()); // return to me
    buildIndxReq->setParallelism(10);
    Uint32 indexColumns[1] = {1};
    Uint32 keyColumns[1] = {0};
    struct LinearSectionPtr orderPtr[2];
    buildIndxReq->setColumnOrder(indexColumns, 1, keyColumns, 1, orderPtr);
    sendSignal(reference(),
	       GSN_BUILDINDXREQ,
	       signal,
	       BuildIndxReq::SignalLength,
	       JBB,
	       orderPtr,
	       BuildIndxReq::NoOfSections);
    break;
  }
  case(301): { // ok
    // index2 -T; index2 -I -n10000; index2 -c -p
    // all dump 301 0 0 0 0 0 4 2
    // select_count INDEX0000
    BuildIndxReq * buildIndxReq = (BuildIndxReq *)signal->getDataPtrSend();
    
    MEMCOPY_NO_WORDS(buildIndxReq, 
		     signal->theData + 1, 
		     BuildIndxReq::SignalLength);
    buildIndxReq->setUserRef(reference()); // return to me
    buildIndxReq->setParallelism(10);
    Uint32 indexColumns[2] = {0, 1};
    Uint32 keyColumns[1] = {0};
    struct LinearSectionPtr orderPtr[2];
    buildIndxReq->setColumnOrder(indexColumns, 2, keyColumns, 1, orderPtr);
    sendSignal(reference(),
	       GSN_BUILDINDXREQ,
	       signal,
	       BuildIndxReq::SignalLength,
	       JBB,
	       orderPtr,
	       BuildIndxReq::NoOfSections);
    break;
  }
  case(302): { // ok
    // index -T; index -I -n1000; index -c -p
    // all dump 302 0 0 0 0 0 4 2
    // select_count PNUMINDEX0000
    BuildIndxReq * buildIndxReq = (BuildIndxReq *)signal->getDataPtrSend();
    
    MEMCOPY_NO_WORDS(buildIndxReq, 
		     signal->theData + 1, 
		     BuildIndxReq::SignalLength);
    buildIndxReq->setUserRef(reference()); // return to me
    buildIndxReq->setParallelism(10);
    Uint32 indexColumns[3] = {0, 3, 5};
    Uint32 keyColumns[1] = {0};
    struct LinearSectionPtr orderPtr[2];
    buildIndxReq->setColumnOrder(indexColumns, 3, keyColumns, 1, orderPtr);
    sendSignal(reference(),
	       GSN_BUILDINDXREQ,
	       signal,
	       BuildIndxReq::SignalLength,
	       JBB,
	       orderPtr,
	       BuildIndxReq::NoOfSections);
    break;
  }
  case(303): { // ok
    // index -T -2; index -I -2 -n1000; index -c -p
    // all dump 303 0 0 0 0 0 4 2
    // select_count PNUMINDEX0000
    BuildIndxReq * buildIndxReq = (BuildIndxReq *)signal->getDataPtrSend();
    
    MEMCOPY_NO_WORDS(buildIndxReq, 
		     signal->theData + 1, 
		     BuildIndxReq::SignalLength);
    buildIndxReq->setUserRef(reference()); // return to me
    buildIndxReq->setParallelism(10);
    Uint32 indexColumns[3] = {0, 3, 5};
    Uint32 keyColumns[2] = {0, 1};
    struct LinearSectionPtr orderPtr[2];
    buildIndxReq->setColumnOrder(indexColumns, 3, keyColumns, 2, orderPtr);
    sendSignal(reference(),
	       GSN_BUILDINDXREQ,
	       signal,
	       BuildIndxReq::SignalLength,
	       JBB,
	       orderPtr,
	       BuildIndxReq::NoOfSections);
    break;
  }
  case(304): { // ok
    // index -T -L; index -I -L -n1000; index -c -p
    // all dump 304 0 0 0 0 0 4 2
    // select_count PNUMINDEX0000
    BuildIndxReq * buildIndxReq = (BuildIndxReq *)signal->getDataPtrSend();
    
    MEMCOPY_NO_WORDS(buildIndxReq, 
		     signal->theData + 1, 
		     BuildIndxReq::SignalLength);
    buildIndxReq->setUserRef(reference()); // return to me
    buildIndxReq->setParallelism(10);
    Uint32 indexColumns[3] = {0, 3, 5};
    Uint32 keyColumns[1] = {0};
    struct LinearSectionPtr orderPtr[2];
    buildIndxReq->setColumnOrder(indexColumns, 3, keyColumns, 1, orderPtr);
    sendSignal(reference(),
	       GSN_BUILDINDXREQ,
	       signal,
	       BuildIndxReq::SignalLength,
	       JBB,
	       orderPtr,
	       BuildIndxReq::NoOfSections);
    break;
  }
  case(305): { // ok
    // index -T -2 -L; index -I -2 -L -n1000; index -c -p
    // all dump 305 0 0 0 0 0 4 2
    // select_count PNUMINDEX0000
    BuildIndxReq * buildIndxReq = (BuildIndxReq *)signal->getDataPtrSend();
    
    MEMCOPY_NO_WORDS(buildIndxReq, 
		     signal->theData + 1, 
		     BuildIndxReq::SignalLength);
    buildIndxReq->setUserRef(reference()); // return to me
    buildIndxReq->setParallelism(10);
    Uint32 indexColumns[3] = {0, 3, 5};
    Uint32 keyColumns[2] = {0, 1};
    struct LinearSectionPtr orderPtr[2];
    buildIndxReq->setColumnOrder(indexColumns, 3, keyColumns, 2, orderPtr);
    sendSignal(reference(),
	       GSN_BUILDINDXREQ,
	       signal,
	       BuildIndxReq::SignalLength,
	       JBB,
	       orderPtr,
	       BuildIndxReq::NoOfSections);
    break;
  }
  default: {
    // Ignore
  }
  }
}

// Build index
/**
 *
 */
void Trix:: execBUILDINDXREQ(Signal* signal)
{
  jamEntry();
  BuildIndxReq * buildIndxReq = (BuildIndxReq *)signal->getDataPtr();

  // Seize a subscription record
  SubscriptionRecPtr subRecPtr;
  SubscriptionRecord* subRec;
  
  if (!c_theSubscriptions.seizeId(subRecPtr, buildIndxReq->getBuildId())) {
    // Failed to allocate subscription record
    BuildIndxRef * buildIndxRef = (BuildIndxRef *)signal->getDataPtrSend();

    buildIndxRef->setErrorCode(BuildIndxRef::AllocationFailure);
    releaseSections(signal);
    sendSignal(buildIndxReq->getUserRef(), 
	       GSN_BUILDINDXREF, signal, BuildIndxRef::SignalLength, JBB);
    return;
  }
  subRec = subRecPtr.p;
  subRec->errorCode = BuildIndxRef::NoError;
  subRec->userReference = buildIndxReq->getUserRef();
  subRec->connectionPtr = buildIndxReq->getConnectionPtr();
  subRec->subscriptionId = buildIndxReq->getBuildId();
  subRec->subscriptionKey = buildIndxReq->getBuildKey();
  subRec->indexType = buildIndxReq->getIndexType();
  subRec->sourceTableId = buildIndxReq->getTableId();
  subRec->targetTableId = buildIndxReq->getIndexId();
  subRec->parallelism = buildIndxReq->getParallelism();
  subRec->expectedConf = 0;
  subRec->subscriptionCreated = false;
  subRec->pendingSubSyncContinueConf = false;
  subRec->prepareId = RNIL;

  // Get column order segments
  Uint32 noOfSections = signal->getNoOfSections();
  if(noOfSections > 0) {
    SegmentedSectionPtr ptr;
    signal->getSection(ptr, BuildIndxReq::INDEX_COLUMNS);
    append(subRec->attributeOrder, ptr, getSectionSegmentPool());
    subRec->noOfIndexColumns = ptr.sz;
  }
  if(noOfSections > 1) {
    SegmentedSectionPtr ptr;
    signal->getSection(ptr, BuildIndxReq::KEY_COLUMNS);
    append(subRec->attributeOrder, ptr, getSectionSegmentPool());
    subRec->noOfKeyColumns = ptr.sz;
  }
#if 0
  // Debugging
  printf("Trix:: execBUILDINDXREQ: Attribute order:\n");
  subRec->attributeOrder.print(stdout);
#endif
  releaseSections(signal);
  prepareInsertTransactions(signal, subRecPtr);
}

void Trix:: execBUILDINDXCONF(Signal* signal)
{
  printf("Trix:: execBUILDINDXCONF\n");
}

void Trix:: execBUILDINDXREF(Signal* signal)
{
  printf("Trix:: execBUILDINDXREF\n");
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
  subRecPtr.p = subRec;
  subRec->errorCode = BuildIndxRef::InternalError;
}

void Trix::execUTIL_EXECUTE_CONF(Signal* signal)
{
  jamEntry();
  UtilExecuteConf * utilExecuteConf = (UtilExecuteConf *)signal->getDataPtr();
  SubscriptionRecPtr subRecPtr;
  SubscriptionRecord* subRec;

  subRecPtr.i = utilExecuteConf->senderData;
  if ((subRec = c_theSubscriptions.getPtr(subRecPtr.i)) == NULL) {
    printf("rix::execUTIL_EXECUTE_CONF: Failed to find subscription data %u\n", subRecPtr.i);
    return;
  }
  subRecPtr.p = subRec;
  subRec->expectedConf--;
  checkParallelism(signal, subRec);
  if (subRec->expectedConf == 0)
    buildComplete(signal, subRecPtr);
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
  subRecPtr.p = subRec;
  ndbrequire(utilExecuteRef->errorCode == UtilExecuteRef::TCError);
  if(utilExecuteRef->TCErrorCode == CONSTRAINT_VIOLATION)
    buildFailed(signal, subRecPtr, BuildIndxRef::IndexNotUnique);
  else
    buildFailed(signal, subRecPtr, BuildIndxRef::InternalError);
}

void Trix::execSUB_CREATE_CONF(Signal* signal)
{
  jamEntry();
  SubCreateConf * subCreateConf = (SubCreateConf *)signal->getDataPtr();
  SubscriptionRecPtr subRecPtr;
  SubscriptionRecord* subRec;

  subRecPtr.i = subCreateConf->subscriberData;
  if ((subRec = c_theSubscriptions.getPtr(subRecPtr.i)) == NULL) {
    printf("Trix::execSUB_CREATE_CONF: Failed to find subscription data %u\n", subRecPtr.i);
    return;
  }
  ndbrequire(subRec->subscriptionId == subCreateConf->subscriptionId);
  ndbrequire(subRec->subscriptionKey == subCreateConf->subscriptionKey);
  subRec->subscriptionCreated = true;
  subRecPtr.p = subRec;
  setupTableScan(signal, subRecPtr);
}

void Trix::execSUB_CREATE_REF(Signal* signal)
{
  jamEntry();
  // THIS SIGNAL IS NEVER SENT FROM SUMA?
  /*
  SubCreateRef * subCreateRef = (SubCreateRef *)signal->getDataPtr();
  SubscriptionRecPtr subRecPtr;
  SubscriptionRecord* subRec;

  subRecPtr.i = subCreateRef->subscriberData;
  if ((subRec = c_theSubscriptions.getPtr(subRecPtr.i)) == NULL) {
    printf("Trix::execSUB_CREATE_REF: Failed to find subscription data %u\n", subRecPtr.i);
    return;
  }
  subRecPtr.p = subRec;
  buildFailed(signal, subRecPtr, BuildIndxRef::InternalError);
  */
}

void Trix::execSUB_SYNC_CONF(Signal* signal)
{
  jamEntry();
  SubSyncConf * subSyncConf = (SubSyncConf *)signal->getDataPtr();
  SubscriptionRecPtr subRecPtr;
  SubscriptionRecord* subRec;
  
  subRecPtr.i = subSyncConf->subscriberData;
  if ((subRec = c_theSubscriptions.getPtr(subRecPtr.i)) == NULL) {
    printf("Trix::execSUB_SYNC_CONF: Failed to find subscription data %u\n", subRecPtr.i);
    return;
  }
  ndbrequire(subRec->subscriptionId == subSyncConf->subscriptionId);
  ndbrequire(subRec->subscriptionKey == subSyncConf->subscriptionKey);
  subRecPtr.p = subRec;
  if(subSyncConf->part == SubscriptionData::MetaData)
    startTableScan(signal, subRecPtr);
  else {
    subRec->expectedConf--;
    checkParallelism(signal, subRec);
    if (subRec->expectedConf == 0)
      buildComplete(signal, subRecPtr);
  }
}

void Trix::execSUB_SYNC_REF(Signal* signal)
{
  jamEntry();
  SubSyncRef * subSyncRef = (SubSyncRef *)signal->getDataPtr();
  SubscriptionRecPtr subRecPtr;
  SubscriptionRecord* subRec;

  subRecPtr.i = subSyncRef->subscriberData;
  if ((subRec = c_theSubscriptions.getPtr(subRecPtr.i)) == NULL) {
    printf("Trix::execSUB_SYNC_REF: Failed to find subscription data %u\n", subRecPtr.i);
    return;
  }
  subRecPtr.p = subRec;
  buildFailed(signal, subRecPtr, BuildIndxRef::InternalError);
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
  checkParallelism(signal, subRec);
}

void Trix::execSUB_META_DATA(Signal* signal)
{
  jamEntry();
}

void Trix::execSUB_TABLE_DATA(Signal* signal)
{
  jamEntry();
  SubTableData * subTableData = (SubTableData *)signal->getDataPtr();
  SubscriptionRecPtr subRecPtr;
  SubscriptionRecord* subRec;
  subRecPtr.i = subTableData->subscriberData;
  if ((subRec = c_theSubscriptions.getPtr(subRecPtr.i)) == NULL) {
    printf("Trix::execSUB_TABLE_DATA: Failed to find subscription data %u\n", subRecPtr.i);
    return;
  }
  subRecPtr.p = subRec;
  SegmentedSectionPtr headerPtr, dataPtr;
  if (!signal->getSection(headerPtr, 0)) {
    printf("Trix::execSUB_TABLE_DATA: Failed to get header section\n");
  }
  if (!signal->getSection(dataPtr, 1)) {
    printf("Trix::execSUB_TABLE_DATA: Failed to get data section\n");
  }
  executeInsertTransaction(signal, subRecPtr, headerPtr, dataPtr);
}

void Trix::setupSubscription(Signal* signal, SubscriptionRecPtr subRecPtr)
{
  Uint32 attributeList[MAX_ATTRIBUTES_IN_TABLE * 2];
  SubCreateReq * subCreateReq = (SubCreateReq *)signal->getDataPtrSend();
  SubscriptionRecord* subRec = subRecPtr.p;
//  Uint32 listLen = subRec->noOfIndexColumns + subRec->noOfKeyColumns;
  AttrOrderBuffer::DataBufferIterator iter;
  Uint32 i = 0;

  jam();
  bool moreAttributes = subRec->attributeOrder.first(iter);
  while (moreAttributes) {
    attributeList[i++] = *iter.data;
    moreAttributes = subRec->attributeOrder.next(iter);
  }
  // Merge index and key column segments
  struct LinearSectionPtr orderPtr[3];
  orderPtr[0].p = attributeList;
  orderPtr[0].sz = subRec->attributeOrder.getSize();


  subCreateReq->subscriberRef = reference();
  subCreateReq->subscriberData = subRecPtr.i;
  subCreateReq->subscriptionId = subRec->subscriptionId;
  subCreateReq->subscriptionKey = subRec->subscriptionKey;
  subCreateReq->tableId = subRec->sourceTableId;
  subCreateReq->subscriptionType = SubCreateReq::SingleTableScan;
  
  sendSignal(SUMA_REF, GSN_SUB_CREATE_REQ, 
	     signal, SubCreateReq::SignalLength, JBB, orderPtr, 1);
}

void Trix::setupTableScan(Signal* signal, SubscriptionRecPtr subRecPtr)
{
  SubSyncReq * subSyncReq = (SubSyncReq *)signal->getDataPtrSend();

  jam();
  subSyncReq->subscriptionId = subRecPtr.i;
  subSyncReq->subscriptionKey = subRecPtr.p->subscriptionKey;
  subSyncReq->part = SubscriptionData::MetaData;
  sendSignal(SUMA_REF, GSN_SUB_SYNC_REQ, 
	     signal, SubSyncReq::SignalLength, JBB);
}

void Trix::startTableScan(Signal* signal, SubscriptionRecPtr subRecPtr)
{
  jam();
  subRecPtr.p->expectedConf = 1;
  SubSyncReq * subSyncReq = (SubSyncReq *)signal->getDataPtrSend();

  subSyncReq->subscriptionId = subRecPtr.i;
  subSyncReq->subscriptionKey = subRecPtr.p->subscriptionKey;
  subSyncReq->part = SubscriptionData::TableData;
  sendSignal(SUMA_REF, GSN_SUB_SYNC_REQ, 
	     signal, SubSyncReq::SignalLength, JBB);
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

void Trix::executeInsertTransaction(Signal* signal, 
				    SubscriptionRecPtr subRecPtr,
				    SegmentedSectionPtr headerPtr,
				    SegmentedSectionPtr dataPtr)
{
  jam();
  SubscriptionRecord* subRec = subRecPtr.p;
  UtilExecuteReq * utilExecuteReq = 
    (UtilExecuteReq *)signal->getDataPtrSend();
  Uint32* headerBuffer = signal->theData + 25;
  Uint32*  dataBuffer = headerBuffer + headerPtr.sz;

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
  copy(headerBuffer, headerPtr);
  copy(dataBuffer, dataPtr);

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
			noOfKeyData);

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
  ndbrequire(false);
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
  ndbrequire(false);
}

void
Trix::execUTIL_RELEASE_CONF(Signal* signal){
  
  UtilReleaseConf * const conf = (UtilReleaseConf*)signal->getDataPtrSend();
  
  SubscriptionRecPtr subRecPtr;
  c_theSubscriptions.getPtr(subRecPtr, conf->senderData);
  
  if(subRecPtr.p->errorCode == BuildIndxRef::NoError){
    // Build is complete, reply to original sender
    BuildIndxConf * buildIndxConf = (BuildIndxConf *)signal->getDataPtrSend();
    buildIndxConf->setUserRef(subRecPtr.p->userReference);
    buildIndxConf->setConnectionPtr(subRecPtr.p->connectionPtr);
    buildIndxConf->setRequestType(BuildIndxReq::RT_TRIX);
    buildIndxConf->setIndexType(subRecPtr.p->indexType);
    buildIndxConf->setTableId(subRecPtr.p->sourceTableId);
    buildIndxConf->setIndexId(subRecPtr.p->targetTableId);
    
    sendSignal(subRecPtr.p->userReference, GSN_BUILDINDXCONF, signal, 
	       BuildIndxConf::SignalLength , JBB);  
  } else {
    // Build failed, reply to original sender
    BuildIndxRef * buildIndxRef = (BuildIndxRef *)signal->getDataPtrSend();
    buildIndxRef->setUserRef(subRecPtr.p->userReference);
    buildIndxRef->setConnectionPtr(subRecPtr.p->connectionPtr);
    buildIndxRef->setRequestType(BuildIndxReq::RT_TRIX);
    buildIndxRef->setIndexType(subRecPtr.p->indexType);
    buildIndxRef->setTableId(subRecPtr.p->sourceTableId);
    buildIndxRef->setIndexId(subRecPtr.p->targetTableId);
    buildIndxRef->setErrorCode(subRecPtr.p->errorCode);
    
    sendSignal(subRecPtr.p->userReference, GSN_BUILDINDXREF, signal, 
	       BuildIndxRef::SignalLength , JBB);  
  }

  // Release subscription record
  subRecPtr.p->attributeOrder.release();
  c_theSubscriptions.release(subRecPtr.i);
}

void Trix::checkParallelism(Signal* signal, SubscriptionRecord* subRec)
{
  if ((subRec->pendingSubSyncContinueConf) &&
      (subRec->expectedConf < subRec->parallelism)) {
    SubSyncContinueConf  * subSyncContinueConf = 
      (SubSyncContinueConf *) signal->getDataPtrSend();
    subSyncContinueConf->subscriptionId = subRec->subscriptionId;
    subSyncContinueConf->subscriptionKey = subRec->subscriptionKey;
    sendSignal(SUMA_REF, GSN_SUB_SYNC_CONTINUE_CONF, signal, 
	       SubSyncContinueConf::SignalLength , JBB);  
    subRec->pendingSubSyncContinueConf = false;
  }
}

BLOCK_FUNCTIONS(Trix);

template void append(DataBuffer<15>&,SegmentedSectionPtr,SectionSegmentPool&);

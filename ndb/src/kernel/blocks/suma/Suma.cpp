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

#include "Suma.hpp"

#include <ndb_version.h>

#include <NdbTCP.h>
#include <Bitmask.hpp>
#include <SimpleProperties.hpp>

#include <signaldata/NodeFailRep.hpp>
#include <signaldata/ReadNodesConf.hpp>

#include <signaldata/ListTables.hpp>
#include <signaldata/GetTabInfo.hpp>
#include <signaldata/GetTableId.hpp>
#include <signaldata/DictTabInfo.hpp>
#include <signaldata/SumaImpl.hpp>
#include <signaldata/ScanFrag.hpp>
#include <signaldata/TransIdAI.hpp>
#include <signaldata/CreateTrig.hpp>
#include <signaldata/AlterTrig.hpp>
#include <signaldata/DropTrig.hpp>
#include <signaldata/FireTrigOrd.hpp>
#include <signaldata/TrigAttrInfo.hpp>
#include <signaldata/CheckNodeGroups.hpp>
#include <signaldata/GCPSave.hpp>
#include <GrepError.hpp>

#include <DebuggerNames.hpp>

//#define HANDOVER_DEBUG
//#define NODEFAIL_DEBUG
//#define NODEFAIL_DEBUG2
//#define DEBUG_SUMA_SEQUENCE
//#define EVENT_DEBUG
//#define EVENT_PH3_DEBUG
//#define EVENT_DEBUG2

/**
 * @todo:
 * SUMA crashes if an index is created at the same time as
 * global replication. Very easy to reproduce using testIndex.
 * Note: This only happens occasionally, but is quite easy to reprod.
 */

Uint32 g_subPtrI = RNIL;
static const Uint32 SUMA_SEQUENCE = 0xBABEBABE;


/**************************************************************
 *
 * Start of suma
 *
 */

#define PRINT_ONLY 0
static Uint32 g_TypeOfStart = NodeState::ST_ILLEGAL_TYPE;

void
Suma::getNodeGroupMembers(Signal* signal) {
  jam();
  /**
   * Ask DIH for nodeGroupMembers
   */
  CheckNodeGroups * sd = (CheckNodeGroups*)signal->getDataPtrSend();
  sd->blockRef = reference();
  sd->requestType =
    CheckNodeGroups::Direct |
    CheckNodeGroups::GetNodeGroupMembers;
  sd->nodeId = getOwnNodeId();
  EXECUTE_DIRECT(DBDIH, GSN_CHECKNODEGROUPSREQ, signal, 
		 CheckNodeGroups::SignalLength);
  jamEntry();
  
  c_nodeGroup = sd->output;
  c_noNodesInGroup = 0;
  for (int i = 0; i < MAX_NDB_NODES; i++) {
    if (sd->mask.get(i)) {
      if (i == getOwnNodeId()) c_idInNodeGroup = c_noNodesInGroup;
      c_nodesInGroup[c_noNodesInGroup] = i;
      c_noNodesInGroup++;
    }
  }

  //  ndbout_c("c_noNodesInGroup=%d", c_noNodesInGroup);
  ndbrequire(c_noNodesInGroup >= 0); // at least 1 node in the nodegroup

#ifdef NODEFAIL_DEBUG
  for (Uint32 i = 0; i < c_noNodesInGroup; i++) {
    ndbout_c ("Suma: NodeGroup %u, me %u, me in group %u, member[%u] %u",
	      c_nodeGroup, getOwnNodeId(), c_idInNodeGroup,
	      i, c_nodesInGroup[i]);
  }
#endif
}

void
Suma::execSTTOR(Signal* signal) {
  jamEntry();                            
  
  const Uint32 startphase  = signal->theData[1];
  const Uint32 typeOfStart = signal->theData[7];

#ifdef NODEFAIL_DEBUG
  ndbout_c ("SUMA::execSTTOR startphase = %u, typeOfStart = %u",
	    startphase, typeOfStart);

#endif

  if(startphase == 1){
    jam();
    c_restartLock = true;
  }

  if(startphase == 3){
    jam();
    g_TypeOfStart = typeOfStart;
    signal->theData[0] = reference();
    sendSignal(NDBCNTR_REF, GSN_READ_NODESREQ, signal, 1, JBB);

#if 0

    /**
     * Debug
     */

    
    SubscriptionPtr subPtr;
    Ptr<SyncRecord> syncPtr;
    ndbrequire(c_subscriptions.seize(subPtr));
    ndbrequire(c_syncPool.seize(syncPtr));
    

    ndbout_c("Suma: subPtr.i = %d syncPtr.i = %d", subPtr.i, syncPtr.i);

    subPtr.p->m_syncPtrI = syncPtr.i;
    subPtr.p->m_subscriptionType = SubCreateReq::DatabaseSnapshot;
    syncPtr.p->m_subscriptionPtrI = subPtr.i;
    syncPtr.p->ptrI = syncPtr.i;
    g_subPtrI = subPtr.i;
    //    sendSTTORRY(signal);
#endif    
    return;
  }

  if(startphase == 5) {
    getNodeGroupMembers(signal);
    if (g_TypeOfStart == NodeState::ST_NODE_RESTART) {
      jam();
      for (Uint32 i = 0; i < c_noNodesInGroup; i++) {
	Uint32 ref = calcSumaBlockRef(c_nodesInGroup[i]);
	if (ref != reference())
	  sendSignal(ref, GSN_SUMA_START_ME, signal,
		     1 /*SumaStartMe::SignalLength*/, JBB);
      }
    }
  }
  
  if(startphase == 7) {
    c_restartLock = false; // may be set false earlier with HANDOVER_REQ
    
    if (g_TypeOfStart != NodeState::ST_NODE_RESTART) {
      for( int i = 0; i < NO_OF_BUCKETS; i++) {
	if (getResponsibleSumaNodeId(i) == refToNode(reference())) {
	  // I'm running this bucket
#ifdef EVENT_DEBUG
	  ndbout_c("bucket %u set to true", i);
#endif
	  c_buckets[i].active = true;
	}
      }
    }

    if(g_TypeOfStart == NodeState::ST_INITIAL_START &&
       c_masterNodeId == getOwnNodeId()) {
      jam();
      createSequence(signal);
      return;
    }//if
  }//if
  

  sendSTTORRY(signal);
  
  return;
}

void
Suma::createSequence(Signal* signal)
{
  jam();

  UtilSequenceReq * req = (UtilSequenceReq*)signal->getDataPtrSend();
  
  req->senderData  = RNIL;
  req->sequenceId  = SUMA_SEQUENCE;
  req->requestType = UtilSequenceReq::Create;
#ifdef DEBUG_SUMA_SEQUENCE
  ndbout_c("SUMA: Create sequence");
#endif
  sendSignal(DBUTIL_REF, GSN_UTIL_SEQUENCE_REQ, 
	     signal, UtilSequenceReq::SignalLength, JBB);
  // execUTIL_SEQUENCE_CONF will call createSequenceReply()
}

void
Suma::createSequenceReply(Signal* signal,
			  UtilSequenceConf * conf,
			  UtilSequenceRef * ref)
{
  jam();

  if (ref != NULL)
    ndbrequire(false);

  sendSTTORRY(signal);
}

void
Suma::execREAD_NODESCONF(Signal* signal){
  jamEntry();
  ReadNodesConf * const conf = (ReadNodesConf *)signal->getDataPtr();
 
  c_aliveNodes.clear();
  c_preparingNodes.clear();

  Uint32 count = 0;
  for(Uint32 i = 0; i < MAX_NDB_NODES; i++){
    if(NodeBitmask::get(conf->allNodes, i)){
      jam();
      
      count++;

      NodePtr node;
      ndbrequire(c_nodes.seize(node));
      
      node.p->nodeId = i;
      if(NodeBitmask::get(conf->inactiveNodes, i)){
	jam();
	node.p->alive = 0;
      } else {
	jam();
	node.p->alive = 1;
	c_aliveNodes.set(i);
      }
    } else
      jam();
  }
  c_masterNodeId = conf->masterNodeId;
  ndbrequire(count == conf->noOfNodes);

  sendSTTORRY(signal);
}

#if 0
void
Suma::execREAD_CONFIG_REQ(Signal* signal) 
{
  const ReadConfigReq * req = (ReadConfigReq*)signal->getDataPtr();
  Uint32 ref = req->senderRef;
  Uint32 senderData = req->senderData;
  ndbrequire(req->noOfParameters == 0);

  jamEntry();

  const ndb_mgm_configuration_iterator * p = 
    theConfiguration.getOwnConfigIterator();
  ndbrequire(p != 0);
  
  ndbrequire(!ndb_mgm_get_int_parameter(p, CFG_DB_NO_REDOLOG_FILES, 
					&cnoLogFiles));
  ndbrequire(cnoLogFiles > 0);

  ndbrequire(!ndb_mgm_get_int_parameter(p, CFG_LQH_FRAG, &cfragrecFileSize));
  ndbrequire(!ndb_mgm_get_int_parameter(p, CFG_LQH_TABLE, &ctabrecFileSize));
  ndbrequire(!ndb_mgm_get_int_parameter(p, CFG_LQH_TC_CONNECT, 
					&ctcConnectrecFileSize));
  clogFileFileSize       = 4 * cnoLogFiles;
  ndbrequire(!ndb_mgm_get_int_parameter(p, CFG_LQH_SCAN, &cscanrecFileSize));
  cmaxAccOps = cscanrecFileSize * MAX_PARALLEL_SCANS_PER_FRAG;

  initRecords();
  initialiseRecordsLab(signal, 0, ref, senderData);
  
  return;
}//Dblqh::execSIZEALT_REP()
#endif

void
Suma::sendSTTORRY(Signal* signal){
  signal->theData[0] = 0;
  signal->theData[3] = 1;
  signal->theData[4] = 3;
  signal->theData[5] = 5;
  signal->theData[6] = 7;
  signal->theData[7] = 255; // No more start phases from missra
  sendSignal(NDBCNTR_REF, GSN_STTORRY, signal, 8, JBB);
}

void
Suma::execNDB_STTOR(Signal* signal) 
{
  jamEntry();                            
}

void
Suma::execCONTINUEB(Signal* signal){
  jamEntry();
}

void
SumaParticipant::execCONTINUEB(Signal* signal) 
{
  jamEntry();
}

/*****************************************************************************
 * 
 * Node state handling
 *
 *****************************************************************************/

void Suma::execAPI_FAILREQ(Signal* signal) 
{
  jamEntry();
  Uint32 failedApiNode = signal->theData[0];
  //BlockReference retRef = signal->theData[1];

  c_failedApiNodes.set(failedApiNode);
  bool found = removeSubscribersOnNode(signal, failedApiNode);

  if(!found){
    jam();
    c_failedApiNodes.clear(failedApiNode);
  }
}//execAPI_FAILREQ()

bool
SumaParticipant::removeSubscribersOnNode(Signal *signal, Uint32 nodeId)
{
  bool found = false;

  SubscriberPtr i_subbPtr;
  c_dataSubscribers.first(i_subbPtr);
  while(!i_subbPtr.isNull()){
    SubscriberPtr subbPtr = i_subbPtr;
    c_dataSubscribers.next(i_subbPtr);
    jam();
    if (refToNode(subbPtr.p->m_subscriberRef) == nodeId) {
      jam();
      c_dataSubscribers.remove(subbPtr);
      c_removeDataSubscribers.add(subbPtr);
      found = true;
    }
  }
  if(found){
    jam();
    sendSubStopReq(signal);
  }
  return found;
}

void
SumaParticipant::sendSubStopReq(Signal *signal){
  static bool remove_lock = false;
  jam();

  if(remove_lock) {
    jam();
    return;
  }
  remove_lock = true;

  SubscriberPtr subbPtr;
  c_removeDataSubscribers.first(subbPtr);
  if (subbPtr.isNull()){
    jam();
#if 0
    signal->theData[0] = failedApiNode;
    signal->theData[1] = reference();
    sendSignal(retRef, GSN_API_FAILCONF, signal, 2, JBB);
#endif
    c_failedApiNodes.clear();

    remove_lock = false;
    return;
  }

  SubscriptionPtr subPtr;
  c_subscriptions.getPtr(subPtr, subbPtr.p->m_subPtrI);

  SubStopReq * const req = (SubStopReq*)signal->getDataPtrSend();
  req->senderRef       = reference();
  req->senderData      = subbPtr.i;
  req->subscriberRef   = subbPtr.p->m_subscriberRef;
  req->subscriberData  = subbPtr.p->m_subscriberData;
  req->subscriptionId  = subPtr.p->m_subscriptionId;
  req->subscriptionKey = subPtr.p->m_subscriptionKey;
  req->part = SubscriptionData::TableData;

  sendSignal(SUMA_REF, GSN_SUB_STOP_REQ, signal, SubStopReq::SignalLength, JBB);
}

void
SumaParticipant::execSUB_STOP_CONF(Signal* signal){
  jamEntry();

  SubStopConf * const conf = (SubStopConf*)signal->getDataPtr();

  //  Uint32 subscriberData = conf->subscriberData;
  //  Uint32 subscriberRef = conf->subscriberRef;

  Subscription key; 
  key.m_subscriptionId = conf->subscriptionId;
  key.m_subscriptionKey = conf->subscriptionKey;

  SubscriptionPtr subPtr;
  if(c_subscriptions.find(subPtr, key)) {
    jam();
    if (subPtr.p->m_markRemove) {
      jam();
      ndbrequire(false);
      ndbrequire(subPtr.p->m_nSubscribers > 0);
      subPtr.p->m_nSubscribers--;
      if (subPtr.p->m_nSubscribers == 0){
	jam();
	completeSubRemoveReq(signal, subPtr);
      }
    }
  }

  sendSubStopReq(signal);
}

void
SumaParticipant::execSUB_STOP_REF(Signal* signal){
  jamEntry();
  SubStopRef * const ref = (SubStopRef*)signal->getDataPtr();

  Uint32 subscriptionId = ref->subscriptionId;
  Uint32 subscriptionKey = ref->subscriptionKey;
  Uint32 part = ref->part;
  Uint32 subscriberData = ref->subscriberData;
  Uint32 subscriberRef = ref->subscriberRef;
  //  Uint32 err = ref->err;

  if(!ref->isTemporary()){
    ndbrequire(false);
  }

  SubStopReq * const req = (SubStopReq*)signal->getDataPtrSend();
  req->subscriberRef = subscriberRef;
  req->subscriberData = subscriberData;
  req->subscriptionId = subscriptionId;
  req->subscriptionKey = subscriptionKey;
  req->part = part;

  sendSignal(SUMA_REF, GSN_SUB_STOP_REQ, signal, SubStopReq::SignalLength, JBB);
}

void
Suma::execNODE_FAILREP(Signal* signal){
  jamEntry();

  NodeFailRep * const rep = (NodeFailRep*)signal->getDataPtr();
  
  bool changed = false;

  NodePtr nodePtr;
#ifdef NODEFAIL_DEBUG
  ndbout_c("Suma: nodefailrep");
#endif
  c_nodeFailGCI = getFirstGCI(signal);

  for(c_nodes.first(nodePtr); nodePtr.i != RNIL; c_nodes.next(nodePtr)){
    if(NodeBitmask::get(rep->theNodes, nodePtr.p->nodeId)){
      if(nodePtr.p->alive){
	ndbassert(c_aliveNodes.get(nodePtr.p->nodeId));
	changed = true;
	jam();
      } else {
	ndbassert(!c_aliveNodes.get(nodePtr.p->nodeId));
	jam();
      }
      
      if (c_preparingNodes.get(nodePtr.p->nodeId)) {
	jam();
	// we are currently preparing this node that died
	// it's ok just to clear and go back to waiting for it to start up
	Restart.resetNode(calcSumaBlockRef(nodePtr.p->nodeId));
	c_preparingNodes.clear(nodePtr.p->nodeId);
      } else if (c_handoverToDo) {
	jam();
	// TODO what if I'm a SUMA that is currently restarting and the SUMA
	// responsible for restarting me is the one that died?

	// a node has failed whilst handover is going on
	// let's check if we're in the process of handover with that node
	c_handoverToDo = false;
	for( int i = 0; i < NO_OF_BUCKETS; i++) {
	  if (c_buckets[i].handover) {
	    // I'm doing handover, but is it with the dead node?
	    if (getResponsibleSumaNodeId(i) == nodePtr.p->nodeId) {
	      // so it was the dead node, has handover started?
	      if (c_buckets[i].handover_started) {
		jam();
		// we're not ok and will have lost data!
		// set not active to indicate this -
		// this will generate takeover behaviour
		c_buckets[i].active = false;
		c_buckets[i].handover_started = false;
	      } // else we're ok to revert back to state before 
	      c_buckets[i].handover = false;
	    } else {
	      jam();
	      // ok, we're doing handover with a different node
	      c_handoverToDo = true;
	    }
	  }
	}
      }

      c_failoverBuffer.nodeFailRep();

      nodePtr.p->alive = 0;
      c_aliveNodes.clear(nodePtr.p->nodeId); // this has to be done after the loop above
    }
  }
}

void
Suma::execINCL_NODEREQ(Signal* signal){
  jamEntry();
  
  //const Uint32 senderRef = signal->theData[0];
  const Uint32 inclNode  = signal->theData[1];

  NodePtr node;
  for(c_nodes.first(node); node.i != RNIL; c_nodes.next(node)){
    jam();
    const Uint32 nodeId = node.p->nodeId;
    if(inclNode == nodeId){
      jam();
      
      ndbrequire(node.p->alive == 0);
      ndbrequire(!c_aliveNodes.get(nodeId));
      
      for (Uint32 j = 0; j < c_noNodesInGroup; j++) {
        jam();
	if (c_nodesInGroup[j] == nodeId) {
	  // the starting node is part of my node group
          jam();
	  c_preparingNodes.set(nodeId); // set as being prepared
	  for (Uint32 i = 0; i < c_noNodesInGroup; i++) {
            jam();
	    if (i == c_idInNodeGroup) {
              jam();
	      // I'm responsible for restarting this SUMA
	      // ALL dict's should have meta data info so it is ok to start
	      Restart.startNode(signal, calcSumaBlockRef(nodeId));
	      break;
	    }//if
	    if (c_aliveNodes.get(c_nodesInGroup[i])) {
              jam();
	      break; // another Suma takes care of this
	    }//if
	  }//for
	  break;
	}//if
      }//for

      node.p->alive = 1;
      c_aliveNodes.set(nodeId);

      break;
    }//if
  }//for

#if 0 // if we include this DIH's got to be prepared, later if needed...
  signal->theData[0] = reference();
  
  sendSignal(senderRef, GSN_INCL_NODECONF, signal, 1, JBB);
#endif
}

void
Suma::execSIGNAL_DROPPED_REP(Signal* signal){
  jamEntry();
  ndbrequire(false);
}

/********************************************************************
 *
 * Dump state
 *
 */

void
Suma::execDUMP_STATE_ORD(Signal* signal){
  jamEntry();

  Uint32 tCase = signal->theData[0];
  if(tCase >= 8000 && tCase <= 8003){
    SubscriptionPtr subPtr;
    c_subscriptions.getPtr(subPtr, g_subPtrI);
    
    Ptr<SyncRecord> syncPtr;
    c_syncPool.getPtr(syncPtr, subPtr.p->m_syncPtrI);
    
    if(tCase == 8000){
      syncPtr.p->startMeta(signal);
    }
    
    if(tCase == 8001){
      syncPtr.p->startScan(signal);
    }

    if(tCase == 8002){
      syncPtr.p->startTrigger(signal);
    }
    
    if(tCase == 8003){
      subPtr.p->m_subscriptionType = SubCreateReq::SingleTableScan;
      LocalDataBuffer<15> attrs(c_dataBufferPool, syncPtr.p->m_attributeList);
      Uint32 tab = 0;
      Uint32 att[] = { 0, 1, 1 };
      syncPtr.p->m_tableList.append(&tab, 1);
      attrs.append(att, 3);
    }
  }

  if(tCase == 8004){
    infoEvent("Suma: c_subscriberPool  size: %d free: %d",
	      c_subscriberPool.getSize(),
	      c_subscriberPool.getNoOfFree());

    infoEvent("Suma: c_tablePool  size: %d free: %d",
	      c_tablePool_.getSize(),
	      c_tablePool_.getNoOfFree());

    infoEvent("Suma: c_subscriptionPool  size: %d free: %d",
	      c_subscriptionPool.getSize(),
	      c_subscriptionPool.getNoOfFree());

    infoEvent("Suma: c_syncPool  size: %d free: %d",
	      c_syncPool.getSize(),
	      c_syncPool.getNoOfFree());

    infoEvent("Suma: c_dataBufferPool  size: %d free: %d",
	      c_dataBufferPool.getSize(),
	      c_dataBufferPool.getNoOfFree());
  }
}

/********************************************************************
 *
 * Convert a table name (db+schema+tablename) to tableId
 *
 */

#if 0
void
SumaParticipant::convertNameToId(SubscriptionPtr subPtr, Signal * signal)
{
  jam();
  if(subPtr.p->m_currentTable < subPtr.p->m_maxTables) {
    jam();

    GetTableIdReq * req = (GetTableIdReq *)signal->getDataPtrSend();
    char * tableName = subPtr.p->m_tableNames[subPtr.p->m_currentTable];
    const Uint32 strLen = strlen(tableName) + 1; // NULL Terminated
    req->senderRef  = reference();
    req->senderData = subPtr.i;
    req->len        = strLen;

    LinearSectionPtr ptr[1];
    ptr[0].p  = (Uint32*)tableName;
    ptr[0].sz = strLen;

    sendSignal(DBDICT_REF,
	       GSN_GET_TABLEID_REQ, 
	       signal, 
	       GetTableIdReq::SignalLength,
	       JBB,
	       ptr,
	       1);
  } else {
    jam();
    sendSubCreateConf(signal, subPtr.p->m_subscriberRef, subPtr);
  }
}
#endif


void 
SumaParticipant::addTableId(Uint32 tableId,
			    SubscriptionPtr subPtr, SyncRecord *psyncRec)
{
#ifdef NODEFAIL_DEBUG
  ndbout_c("SumaParticipant::addTableId(%u,%u,%u), current_table=%u",
	   tableId, subPtr.i, psyncRec, subPtr.p->m_currentTable);
#endif
  subPtr.p->m_tables[tableId] = 1;
  subPtr.p->m_currentTable++;
  if(psyncRec != NULL)
    psyncRec->m_tableList.append(&tableId, 1);  
}

#if 0
void 
SumaParticipant::execGET_TABLEID_CONF(Signal * signal)
{
  jamEntry();

  GetTableIdConf* conf = (GetTableIdConf *)signal->getDataPtr();
  Uint32 tableId = conf->tableId;
  //Uint32 schemaVersion = conf->schemaVersion;  
  Uint32 senderData = conf->senderData;

  SubscriptionPtr subPtr;
  Ptr<SyncRecord> syncPtr;

  c_subscriptions.getPtr(subPtr, senderData);
  c_syncPool.getPtr(syncPtr, subPtr.p->m_syncPtrI);  

  /*
   * add to m_tableList
   */
  addTableId(tableId, subPtr, syncPtr.p);

  convertNameToId(subPtr, signal);
}

void 
SumaParticipant::execGET_TABLEID_REF(Signal * signal)
{
  jamEntry();
  GetTableIdRef const * ref = (GetTableIdRef *)signal->getDataPtr();
  Uint32 senderData         = ref->senderData;
  //  Uint32 err                = ref->err;
  
  SubscriptionPtr subPtr;
  c_subscriptions.getPtr(subPtr, senderData);
  Uint32 subData = subPtr.p->m_subscriberData;
  SubCreateRef * reff = (SubCreateRef*)ref;
  /**
   * @todo: map ref->err to GrepError.
   */
  reff->err = GrepError::SELECTED_TABLE_NOT_FOUND;
  reff->subscriberData = subData;
  sendSignal(subPtr.p->m_subscriberRef,
	     GSN_SUB_CREATE_REF, 
	     signal, 
	     SubCreateRef::SignalLength,
	     JBB);
}
#endif


/*************************************************************
 *
 * Creation of subscription id's
 *
 ************************************************************/

void 
Suma::execCREATE_SUBID_REQ(Signal* signal) 
{
  jamEntry();

  CRASH_INSERTION(13001);

  CreateSubscriptionIdReq const * req =
    (CreateSubscriptionIdReq*)signal->getDataPtr();
  SubscriberPtr subbPtr;
  if(!c_subscriberPool.seize(subbPtr)){
    jam();
    sendSubIdRef(signal, GrepError::SUBSCRIPTION_ID_NOMEM);
    return;
  }

  subbPtr.p->m_subscriberRef  = signal->getSendersBlockRef(); 
  subbPtr.p->m_senderData     = req->senderData;
  subbPtr.p->m_subscriberData = subbPtr.i;

  UtilSequenceReq * utilReq = (UtilSequenceReq*)signal->getDataPtrSend();
   
  utilReq->senderData  = subbPtr.p->m_subscriberData;
  utilReq->sequenceId  = SUMA_SEQUENCE;
  utilReq->requestType = UtilSequenceReq::NextVal;
  sendSignal(DBUTIL_REF, GSN_UTIL_SEQUENCE_REQ, 
	     signal, UtilSequenceReq::SignalLength, JBB);
}

void
Suma::execUTIL_SEQUENCE_CONF(Signal* signal)
{
  jamEntry();

  CRASH_INSERTION(13002);

  UtilSequenceConf * conf = (UtilSequenceConf*)signal->getDataPtr();
#ifdef DEBUG_SUMA_SEQUENCE
  ndbout_c("SUMA: Create sequence conf");
#endif
  if(conf->requestType == UtilSequenceReq::Create) {
    jam();
    createSequenceReply(signal, conf, NULL);
    return;
  }

  Uint32 subId = conf->sequenceValue[0];
  Uint32 subData = conf->senderData;

  SubscriberPtr subbPtr;
  c_subscriberPool.getPtr(subbPtr,subData);
  

  CreateSubscriptionIdConf * subconf = (CreateSubscriptionIdConf*)conf;
  subconf->subscriptionId = subId;
  subconf->subscriptionKey =(getOwnNodeId() << 16) | (subId & 0xFFFF);
  subconf->subscriberData = subbPtr.p->m_senderData;
  
  sendSignal(subbPtr.p->m_subscriberRef, GSN_CREATE_SUBID_CONF, signal,
	     CreateSubscriptionIdConf::SignalLength, JBB);

  c_subscriberPool.release(subbPtr);
}

void
Suma::execUTIL_SEQUENCE_REF(Signal* signal)
{
  jamEntry();
  UtilSequenceRef * ref = (UtilSequenceRef*)signal->getDataPtr();

  if(ref->requestType == UtilSequenceReq::Create) {
    jam();
    createSequenceReply(signal, NULL, ref);
    return;
  }

  Uint32 subData = ref->senderData;

  SubscriberPtr subbPtr;
  c_subscriberPool.getPtr(subbPtr,subData);
  sendSubIdRef(signal, GrepError::SEQUENCE_ERROR);
  c_subscriberPool.release(subbPtr);
  return;
}//execUTIL_SEQUENCE_REF()


void
SumaParticipant::sendSubIdRef(Signal* signal, Uint32 errCode){
  jam();
  CreateSubscriptionIdRef  * ref = 
    (CreateSubscriptionIdRef *)signal->getDataPtrSend();

  ref->err = errCode;
  sendSignal(signal->getSendersBlockRef(), 
	     GSN_CREATE_SUBID_REF,
	     signal, 
	     CreateSubscriptionIdRef::SignalLength,
	     JBB);
  
  releaseSections(signal);  
  return;
}

/**********************************************************
 * Suma participant interface
 *
 * Creation of subscriptions
 */

void
SumaParticipant::execSUB_CREATE_REQ(Signal* signal) {
#ifdef NODEFAIL_DEBUG
  ndbout_c("SumaParticipant::execSUB_CREATE_REQ");
#endif
  jamEntry();                            

  CRASH_INSERTION(13003);

  const SubCreateReq req = *(SubCreateReq*)signal->getDataPtr();    
  
  const Uint32 subId   = req.subscriptionId;
  const Uint32 subKey  = req.subscriptionKey;
  const Uint32 subRef  = req.subscriberRef;
  const Uint32 subData = req.subscriberData;
  const Uint32 type    = req.subscriptionType & SubCreateReq::RemoveFlags;
  const Uint32 flags   = req.subscriptionType & SubCreateReq::GetFlags;
  const bool addTableFlag = (flags & SubCreateReq::AddTableFlag) != 0;
  const bool restartFlag  = (flags & SubCreateReq::RestartFlag)  != 0;

  const Uint32 sender = signal->getSendersBlockRef();

  Subscription key;
  key.m_subscriptionId  = subId;
  key.m_subscriptionKey = subKey;

  SubscriptionPtr subPtr;
  Ptr<SyncRecord> syncPtr;
  
  if (addTableFlag) {
    ndbrequire(restartFlag);  //TODO remove this

    if(!c_subscriptions.find(subPtr, key)) {
      jam();
      sendSubCreateRef(signal, req, GrepError::SUBSCRIPTION_NOT_FOUND);
      return;
    }
    jam();
    c_syncPool.getPtr(syncPtr, subPtr.p->m_syncPtrI);
  } else {
    // Check that id/key is unique
    if(c_subscriptions.find(subPtr, key)) {
      jam();
      sendSubCreateRef(signal, req, GrepError::SUBSCRIPTION_ID_NOT_UNIQUE);
      return;
    }
    if(!c_subscriptions.seize(subPtr)) {
      jam();
      sendSubCreateRef(signal, req, GrepError::NOSPACE_IN_POOL);
      return;
    }
    if(!c_syncPool.seize(syncPtr)) {
      jam();
      sendSubCreateRef(signal, req, GrepError::NOSPACE_IN_POOL);
      return;
    }
    jam();
    subPtr.p->m_subscriberRef    = subRef;
    subPtr.p->m_subscriberData   = subData;
    subPtr.p->m_subscriptionId   = subId;
    subPtr.p->m_subscriptionKey  = subKey;
    subPtr.p->m_subscriptionType = type;
  
    /**
     * ok to memset? Support on all compilers
     * @todo find out if memset is supported by all compilers
     */
    memset(subPtr.p->m_tables,0,MAX_TABLES);
    subPtr.p->m_maxTables    = 0;
    subPtr.p->m_currentTable = 0;
    subPtr.p->m_syncPtrI   = syncPtr.i;
    subPtr.p->m_markRemove = false;
    subPtr.p->m_nSubscribers = 0;

    c_subscriptions.add(subPtr);

    syncPtr.p->m_subscriptionPtrI = subPtr.i;
    syncPtr.p->m_doSendSyncData   = true;
    syncPtr.p->ptrI               = syncPtr.i;
    syncPtr.p->m_locked           = false;
    syncPtr.p->m_error            = false;
  }

  if (restartFlag || 
      type == SubCreateReq::TableEvent) {

    syncPtr.p->m_doSendSyncData = false;

    ndbrequire(type != SubCreateReq::SingleTableScan);
    jam();

    if (subPtr.p->m_tables[req.tableId] != 0) {
      ndbrequire(false); //TODO remove
      jam();
      sendSubCreateRef(signal, req, GrepError::SELECTED_TABLE_ALREADY_ADDED);
      return;
    }
    if (addTableFlag) {
      ndbrequire(type != SubCreateReq::TableEvent);
      jam();
    }
    subPtr.p->m_maxTables++;
    addTableId(req.tableId, subPtr, syncPtr.p);
  } else {
    switch(type){
    case SubCreateReq::SingleTableScan:
      {
	jam();
	syncPtr.p->m_tableList.append(&req.tableId, 1);
	if(signal->getNoOfSections() > 0){
	  SegmentedSectionPtr ptr;
	  signal->getSection(ptr, SubCreateReq::ATTRIBUTE_LIST);
	  LocalDataBuffer<15> attrBuf(c_dataBufferPool,syncPtr.p->m_attributeList);
	  append(attrBuf, ptr, getSectionSegmentPool());
	}
      }
    break;
#if 0
    case SubCreateReq::SelectiveTableSnapshot:
      /**
       * Tables specified by the user that does not exist
       * in the database are just ignored. No error message
       * is given, nor does the db nodes crash
       * @todo: Memory is not release here (used tableBuf)
       */
      {
	if(signal->getNoOfSections() == 0 ){
	  jam();
	  sendSubCreateRef(signal, req, GrepError::WRONG_NO_OF_SECTIONS);
	  return;
	}

	jam();      
	SegmentedSectionPtr ptr;
	signal->getSection(ptr,0);// SubCreateReq::TABLE_LIST);
	SimplePropertiesSectionReader r0(ptr, getSectionSegmentPool());
	Uint32 i=0;
	char table[MAX_TAB_NAME_SIZE];
	r0.reset();
	r0.first();
	while(true){
	  if ((r0.getValueType() != SimpleProperties::StringValue) ||
	      (r0.getValueLen() <= 0)) {
	    releaseSections(signal);
	    ndbrequire(false);
	  }
	  r0.getString(table);
	  strcpy(subPtr.p->m_tableNames[i],table);
	  i++;
	  if(!r0.next())
	    break;
	}
	releaseSections(signal);
	subPtr.p->m_maxTables    = i;
	subPtr.p->m_currentTable = 0;
	releaseSections(signal);
	convertNameToId(subPtr, signal);
	return;
      }
    break;
#endif
    case SubCreateReq::DatabaseSnapshot:
      {
	jam();
      }
    break;
    default:
      ndbrequire(false);
    }
  }

  sendSubCreateConf(signal, sender, subPtr);

  return;
}

void
SumaParticipant::sendSubCreateConf(Signal* signal, Uint32 sender,
				   SubscriptionPtr subPtr)
{
  SubCreateConf * const conf = (SubCreateConf*)signal->getDataPtrSend();      
  conf->subscriptionId       = subPtr.p->m_subscriptionId;
  conf->subscriptionKey      = subPtr.p->m_subscriptionKey;
  conf->subscriberData       = subPtr.p->m_subscriberData;
  sendSignal(sender, GSN_SUB_CREATE_CONF, signal,
	     SubCreateConf::SignalLength, JBB);
}

void
SumaParticipant::sendSubCreateRef(Signal* signal, const SubCreateReq& req, Uint32 errCode){
  jam();
  SubCreateRef * ref = (SubCreateRef *)signal->getDataPtrSend();
  ref->subscriberRef  = reference();
  ref->subscriberData = req.subscriberData;
  ref->err = errCode;
  releaseSections(signal);
  sendSignal(signal->getSendersBlockRef(), GSN_SUB_CREATE_REF, signal, 
	     SubCreateRef::SignalLength, JBB);
  return;
}












Uint32
SumaParticipant::getFirstGCI(Signal* signal) {
  if (c_lastCompleteGCI == RNIL) {
    ndbout_c("WARNING: c_lastCompleteGCI == RNIL");
    return 0;
  }
  return c_lastCompleteGCI+3;
}

/**********************************************************
 *
 * Setting upp trigger for subscription
 *
 */

void 
SumaParticipant::execSUB_SYNC_REQ(Signal* signal) {
  jamEntry();

  CRASH_INSERTION(13004);
#ifdef EVENT_PH3_DEBUG
  ndbout_c("SumaParticipant::execSUB_SYNC_REQ");
#endif

  SubSyncReq * const req = (SubSyncReq*)signal->getDataPtr();

  SubscriptionPtr subPtr;
  Subscription key; 
  key.m_subscriptionId = req->subscriptionId;
  key.m_subscriptionKey = req->subscriptionKey;
  
  if(!c_subscriptions.find(subPtr, key)){
    jam();
    sendSubSyncRef(signal, GrepError::SUBSCRIPTION_ID_NOT_FOUND);
    return;
  }

  /**
   * @todo Tomas, do you really need to do this?
   */
  if(subPtr.p->m_subscriptionType == SubCreateReq::TableEvent) {
    jam();
    subPtr.p->m_subscriberData = req->subscriberData;
  }

  bool ok = false;
  SubscriptionData::Part part = (SubscriptionData::Part)req->part;
  
  Ptr<SyncRecord> syncPtr;
  c_syncPool.getPtr(syncPtr, subPtr.p->m_syncPtrI);
  switch(part){
  case SubscriptionData::MetaData:
    ok = true;
    jam();
    if (subPtr.p->m_subscriptionType == SubCreateReq::DatabaseSnapshot) {
      TableList::DataBufferIterator it;
      syncPtr.p->m_tableList.first(it);
      if(it.isNull()) {
	/**
	 * Get all tables from dict
	 */
	ListTablesReq * req = (ListTablesReq*)signal->getDataPtrSend();
	req->senderRef   = reference();
	req->senderData  = syncPtr.i;
	req->requestData = 0;
	/**
	 * @todo: accomodate scan of index tables?
	 */
	req->setTableType(DictTabInfo::UserTable);

	sendSignal(DBDICT_REF, GSN_LIST_TABLES_REQ, signal, 
		   ListTablesReq::SignalLength, JBB);
	break;
      }
    }

    syncPtr.p->startMeta(signal);
    break;
  case SubscriptionData::TableData: {
    ok = true;
    jam();
    syncPtr.p->startScan(signal);
    break;
  }
  }
  ndbrequire(ok);
}

void
SumaParticipant::sendSubSyncRef(Signal* signal, Uint32 errCode){
  jam();
  SubSyncRef  * ref = 
    (SubSyncRef *)signal->getDataPtrSend();
  ref->err = errCode;
  sendSignal(signal->getSendersBlockRef(), 
	     GSN_SUB_SYNC_REF, 
	     signal, 
	     SubSyncRef::SignalLength,
	     JBB);
	     
  releaseSections(signal);  
  return;
}

/**********************************************************
 * Dict interface
 */

void
SumaParticipant::execLIST_TABLES_CONF(Signal* signal){
  jamEntry();
  CRASH_INSERTION(13005);
  ListTablesConf* const conf = (ListTablesConf*)signal->getDataPtr();
  SyncRecord* tmp = c_syncPool.getPtr(conf->senderData);
  tmp->runLIST_TABLES_CONF(signal);
}


void
SumaParticipant::execGET_TABINFOREF(Signal* signal){
  jamEntry();
  GetTabInfoRef* const ref = (GetTabInfoRef*)signal->getDataPtr();
  SyncRecord* tmp = c_syncPool.getPtr(ref->senderData);
  tmp->runGET_TABINFOREF(signal);
}

void
SumaParticipant::execGET_TABINFO_CONF(Signal* signal){
  jamEntry();

  CRASH_INSERTION(13006);

  if(!assembleFragments(signal)){
    return;
  }
  
  GetTabInfoConf* conf = (GetTabInfoConf*)signal->getDataPtr();
  
  Uint32 tableId = conf->tableId;
  Uint32 senderData = conf->senderData;

  SyncRecord* tmp = c_syncPool.getPtr(senderData);
  ndbrequire(parseTable(signal, conf, tableId, tmp));
  tmp->runGET_TABINFO_CONF(signal);
}

bool
SumaParticipant::parseTable(Signal* signal, GetTabInfoConf* conf, Uint32 tableId,
			    SyncRecord* syncPtr_p){

  SegmentedSectionPtr ptr;
  signal->getSection(ptr, GetTabInfoConf::DICT_TAB_INFO);
  
  SimplePropertiesSectionReader it(ptr, getSectionSegmentPool());
  
  SimpleProperties::UnpackStatus s;
  DictTabInfo::Table tableDesc; tableDesc.init();
  s = SimpleProperties::unpack(it, &tableDesc, 
			       DictTabInfo::TableMapping, 
			       DictTabInfo::TableMappingSize, 
			       true, true);
  
  ndbrequire(s == SimpleProperties::Break);

  TablePtr tabPtr;
  c_tables.find(tabPtr, tableId);
  
  if(!tabPtr.isNull() &&
     tabPtr.p->m_schemaVersion != tableDesc.TableVersion){
    jam();

    tabPtr.p->release(* this);

    // oops wrong schema version in stored tabledesc
    // we need to find all subscriptions with old table desc
    // and all subscribers to this
    // hopefully none
    c_tables.release(tabPtr);
    tabPtr.setNull();
    DLHashTable<SumaParticipant::Subscription>::Iterator i_subPtr;
    c_subscriptions.first(i_subPtr);
    SubscriptionPtr subPtr;
    for(;!i_subPtr.isNull();c_subscriptions.next(i_subPtr)){
      jam();
      c_subscriptions.getPtr(subPtr, i_subPtr.curr.i);
      SyncRecord* tmp = c_syncPool.getPtr(subPtr.p->m_syncPtrI);
      if (tmp == syncPtr_p) {
	jam();
	continue;
      }
      if (subPtr.p->m_tables[tableId]) {
	jam();
	subPtr.p->m_tables[tableId] = 0; // remove this old table reference
	TableList::DataBufferIterator it;
	for(tmp->m_tableList.first(it);!it.isNull();tmp->m_tableList.next(it)) {
	  jam();
	  if (*it.data == tableId){
	    jam();
	    Uint32 *pdata = it.data;
	    tmp->m_tableList.next(it);
	    for(;!it.isNull();tmp->m_tableList.next(it)) {
	      jam();
	      *pdata = *it.data;
	      pdata = it.data;
	    }
	    *pdata = RNIL; // todo remove this last item...
	    break;
	  }
	}
      }
    }
  }

  if (tabPtr.isNull()) {
    jam();
    /**
     * Uninitialized table record
     */
    ndbrequire(c_tables.seize(tabPtr));
    new (tabPtr.p) Table;
    tabPtr.p->m_schemaVersion = RNIL;
    tabPtr.p->m_tableId = tableId;
    tabPtr.p->m_hasTriggerDefined[0] = 0;
    tabPtr.p->m_hasTriggerDefined[1] = 0;
    tabPtr.p->m_hasTriggerDefined[2] = 0;
    tabPtr.p->m_triggerIds[0] = ILLEGAL_TRIGGER_ID;
    tabPtr.p->m_triggerIds[1] = ILLEGAL_TRIGGER_ID;
    tabPtr.p->m_triggerIds[2] = ILLEGAL_TRIGGER_ID;
#if 0
    ndbout_c("Get tab info conf %d", tableId);
#endif
    c_tables.add(tabPtr);
  }

  if(tabPtr.p->m_attributes.getSize() != 0){
    jam();
    return true;
  }

  /**
   * Initialize table object
   */
  Uint32 noAttribs = tableDesc.NoOfAttributes;
  Uint32 notFixed = (tableDesc.NoOfNullable+tableDesc.NoOfVariable);
  tabPtr.p->m_schemaVersion = tableDesc.TableVersion;
  
  // The attribute buffer
  LocalDataBuffer<15> attrBuf(c_dataBufferPool, tabPtr.p->m_attributes);
  
  // Temporary buffer
  DataBuffer<15> theRest(c_dataBufferPool);

  if(!attrBuf.seize(noAttribs)){
    ndbrequire(false);
    return false;
  }
  
  if(!theRest.seize(notFixed)){
    ndbrequire(false);
    return false;
  }
  
  DataBuffer<15>::DataBufferIterator attrIt; // Fixed not nullable
  DataBuffer<15>::DataBufferIterator restIt; // variable + nullable
  attrBuf.first(attrIt);
  theRest.first(restIt);
  
  for(Uint32 i = 0; i < noAttribs; i++) {
    DictTabInfo::Attribute attrDesc; attrDesc.init();
    s = SimpleProperties::unpack(it, &attrDesc, 
				 DictTabInfo::AttributeMapping, 
				 DictTabInfo::AttributeMappingSize, 
				 true, true);
    ndbrequire(s == SimpleProperties::Break);

    if (!attrDesc.AttributeNullableFlag 
	/* && !attrDesc.AttributeVariableFlag */) {
      jam();
      * attrIt.data = attrDesc.AttributeId;
      attrBuf.next(attrIt);
    } else {
      jam();
      * restIt.data = attrDesc.AttributeId;
      theRest.next(restIt);
    }
    
    // Move to next attribute
    it.next();
  }

  /**
   * Put the rest in end of attrBuf
   */
  theRest.first(restIt);
  for(; !restIt.isNull(); theRest.next(restIt)){
    * attrIt.data = * restIt.data;
    attrBuf.next(attrIt);
  }

  theRest.release();
  
  return true;
}

void
SumaParticipant::execDI_FCOUNTCONF(Signal* signal){
  jamEntry();
  
  CRASH_INSERTION(13007);

  const Uint32 senderData = signal->theData[3];
  SyncRecord* tmp = c_syncPool.getPtr(senderData);
  tmp->runDI_FCOUNTCONF(signal);
}

void 
SumaParticipant::execDIGETPRIMCONF(Signal* signal){
  jamEntry();
  
  CRASH_INSERTION(13008);

  const Uint32 senderData = signal->theData[1];
  SyncRecord* tmp = c_syncPool.getPtr(senderData);
  tmp->runDIGETPRIMCONF(signal);
}

void
SumaParticipant::execCREATE_TRIG_CONF(Signal* signal){
  jamEntry();

  CRASH_INSERTION(13009);

  CreateTrigConf * const conf = (CreateTrigConf*)signal->getDataPtr();

  const Uint32 senderData = conf->getConnectionPtr();
  SyncRecord* tmp = c_syncPool.getPtr(senderData);
  tmp->runCREATE_TRIG_CONF(signal);
  
  /**
   * dodido
   * @todo: I (Johan) dont know what to do here. Jonas, what do you mean?
   */
}

void
SumaParticipant::execCREATE_TRIG_REF(Signal* signal){
  jamEntry();
  ndbrequire(false);
}

void
SumaParticipant::execDROP_TRIG_CONF(Signal* signal){
  jamEntry();

  CRASH_INSERTION(13010);

  DropTrigConf * const conf = (DropTrigConf*)signal->getDataPtr();

  const Uint32 senderData = conf->getConnectionPtr();
  SyncRecord* tmp = c_syncPool.getPtr(senderData);
  tmp->runDROP_TRIG_CONF(signal);
}

void
SumaParticipant::execDROP_TRIG_REF(Signal* signal){
  jamEntry();

  DropTrigRef * const ref = (DropTrigRef*)signal->getDataPtr();

  const Uint32 senderData = ref->getConnectionPtr();
  SyncRecord* tmp = c_syncPool.getPtr(senderData);
  tmp->runDROP_TRIG_CONF(signal);
}

/*************************************************************************
 *
 *
 */

void
SumaParticipant::SyncRecord::runLIST_TABLES_CONF(Signal* signal){
  jam();

  ListTablesConf * const conf = (ListTablesConf*)signal->getDataPtr();
  const Uint32 len = signal->length() - ListTablesConf::HeaderLength;

  SubscriptionPtr subPtr;
  suma.c_subscriptions.getPtr(subPtr, m_subscriptionPtrI);

  for (unsigned i = 0; i < len; i++) {
    subPtr.p->m_maxTables++;
    suma.addTableId(ListTablesConf::getTableId(conf->tableData[i]), subPtr, this);
  }

  //  for (unsigned i = 0; i < len; i++)
  //    conf->tableData[i] = ListTablesConf::getTableId(conf->tableData[i]);
  //  m_tableList.append(&conf->tableData[0], len);

#if 0 
  TableList::DataBufferIterator it;
  int i = 0;
  for(m_tableList.first(it);!it.isNull();m_tableList.next(it)) {
    ndbout_c("%u listtableconf tableid %d", i++, *it.data);
  }
#endif

  if(len == ListTablesConf::DataLength){
    jam();
    // we expect more LIST_TABLE_CONF
    return;
  }

#if 0
  subPtr.p->m_currentTable = 0;
  subPtr.p->m_maxTables    = 0;

  TableList::DataBufferIterator it;
  for(m_tableList.first(it); !it.isNull(); m_tableList.next(it)) {
    subPtr.p->m_maxTables++;
    suma.addTableId(*it.data, subPtr, NULL);
#ifdef NODEFAIL_DEBUG
    ndbout_c(" listtableconf tableid %d",*it.data);
#endif
  }
#endif
  
  startMeta(signal);
}

void
SumaParticipant::SyncRecord::startMeta(Signal* signal){
  jam();
  m_currentTable = 0;
  nextMeta(signal);
}

/**
 * m_tableList only contains UserTables
 */
void
SumaParticipant::SyncRecord::nextMeta(Signal* signal){
  jam();
  
  TableList::DataBufferIterator it;
  if(!m_tableList.position(it, m_currentTable)){
    completeMeta(signal);
    return;
  }

  GetTabInfoReq * req = (GetTabInfoReq *)signal->getDataPtrSend();
  req->senderRef = suma.reference();
  req->senderData = ptrI;
  req->requestType = 
    GetTabInfoReq::RequestById | GetTabInfoReq::LongSignalConf;
  req->tableId = * it.data;

#if 0
  ndbout_c("GET_TABINFOREQ id %d", req->tableId);
#endif
  suma.sendSignal(DBDICT_REF, GSN_GET_TABINFOREQ, signal, 
		  GetTabInfoReq::SignalLength, JBB);
}

void
SumaParticipant::SyncRecord::runGET_TABINFOREF(Signal* signal)
{
  jam();

  SubscriptionPtr subPtr;
  suma.c_subscriptions.getPtr(subPtr, m_subscriptionPtrI);
  ndbrequire(subPtr.p->m_syncPtrI == ptrI);

  Uint32 type = subPtr.p->m_subscriptionType;

  bool do_continue = false;
  switch (type) {
  case SubCreateReq::TableEvent:
    jam();
    break;
  case SubCreateReq::DatabaseSnapshot:
    jam();
    do_continue = true;
    break;
  case SubCreateReq::SelectiveTableSnapshot:
    jam();
    do_continue = true;
    break;
  case SubCreateReq::SingleTableScan:
    jam();
    break;
  default:
    ndbrequire(false);
    break;
  }

  if (! do_continue) {
    m_error = true;
    completeMeta(signal);
    return;
  }

  m_currentTable++;
  nextMeta(signal);
  return;

  // now we need to clean-up
}


void
SumaParticipant::SyncRecord::runGET_TABINFO_CONF(Signal* signal){
  jam();
  
  GetTabInfoConf * const conf = (GetTabInfoConf*)signal->getDataPtr();
  //  const Uint32 gci = conf->gci;
  const Uint32 tableId = conf->tableId;
  TableList::DataBufferIterator it;
  
  ndbrequire(m_tableList.position(it, m_currentTable));
  ndbrequire(* it.data == tableId);
  
  SubscriptionPtr subPtr;
  suma.c_subscriptions.getPtr(subPtr, m_subscriptionPtrI);
  ndbrequire(subPtr.p->m_syncPtrI == ptrI);
  
  SegmentedSectionPtr ptr;
  signal->getSection(ptr, GetTabInfoConf::DICT_TAB_INFO);

  SubMetaData * data = (SubMetaData*)signal->getDataPtrSend();
  /** 
   * sending lastCompleteGCI. Used by Lars in interval calculations
   * incremenet by one, since last_CompleteGCI is the not the current gci.
   */
  data->gci = suma.c_lastCompleteGCI + 1;
  data->tableId = tableId;
  data->senderData = subPtr.p->m_subscriberData;
#if PRINT_ONLY
  ndbout_c("GSN_SUB_META_DATA Table %d", tableId);
#else

  bool okToSend = m_doSendSyncData;

  /*
   * If it is a selectivetablesnapshot and the table is not part of the 
   * subscription, then do not send anything, just continue.
   * If it is a tablevent, don't send regardless since the APIs are not
   * interested in meta data.
   */
  if(subPtr.p->m_subscriptionType == SubCreateReq::SelectiveTableSnapshot)
    if(!subPtr.p->m_tables[tableId])
      okToSend = false;

  if(okToSend) {
    if(refToNode(subPtr.p->m_subscriberRef) == 0){
      jam();
      suma.EXECUTE_DIRECT(refToBlock(subPtr.p->m_subscriberRef),
			  GSN_SUB_META_DATA,
			  signal, 
			  SubMetaData::SignalLength); 
      jamEntry();
      suma.releaseSections(signal);
    } else {
      jam();
      suma.sendSignal(subPtr.p->m_subscriberRef, 
		      GSN_SUB_META_DATA,
		      signal, 
		      SubMetaData::SignalLength, JBB);
    }
  }
#endif
  
  TablePtr tabPtr;
  ndbrequire(suma.c_tables.find(tabPtr, tableId));
  
  LocalDataBuffer<15> fragBuf(suma.c_dataBufferPool, tabPtr.p->m_fragments);
  if(fragBuf.getSize() == 0){
    /**
     * We need to gather fragment info
     */
    jam();
    signal->theData[0] = RNIL;
    signal->theData[1] = tableId;
    signal->theData[2] = ptrI;
    suma.sendSignal(DBDIH_REF, GSN_DI_FCOUNTREQ, signal, 3, JBB);    
    return;
  }
  
  m_currentTable++;
  nextMeta(signal);
}

void 
SumaParticipant::SyncRecord::runDI_FCOUNTCONF(Signal* signal){
  jam();

  const Uint32 userPtr = signal->theData[0];
  const Uint32 fragCount = signal->theData[1];
  const Uint32 tableId = signal->theData[2];

  ndbrequire(userPtr == RNIL && signal->length() == 5);

  TablePtr tabPtr;
  ndbrequire(suma.c_tables.find(tabPtr, tableId));
  
  LocalDataBuffer<15> fragBuf(suma.c_dataBufferPool,  tabPtr.p->m_fragments);  
  ndbrequire(fragBuf.getSize() == 0);
  
  m_currentFragment = fragCount;
  signal->theData[0] = RNIL;
  signal->theData[1] = ptrI;
  signal->theData[2] = tableId;
  signal->theData[3] = 0; // Frag no
  suma.sendSignal(DBDIH_REF, GSN_DIGETPRIMREQ, signal, 4, JBB);
}

void
SumaParticipant::SyncRecord::runDIGETPRIMCONF(Signal* signal){
  jam();

  const Uint32 userPtr = signal->theData[0];
  //const Uint32 senderData = signal->theData[1];
  const Uint32 nodeCount = signal->theData[6];
  const Uint32 tableId = signal->theData[7];
  const Uint32 fragNo = signal->theData[8];
  
  ndbrequire(userPtr == RNIL && signal->length() == 9);
  ndbrequire(nodeCount > 0 && nodeCount <= MAX_REPLICAS);
  
  TablePtr tabPtr;
  ndbrequire(suma.c_tables.find(tabPtr, tableId));
  LocalDataBuffer<15> fragBuf(suma.c_dataBufferPool,  tabPtr.p->m_fragments);  

  /**
   * Add primary node for fragment to list
   */
  FragmentDescriptor fd;
  fd.m_fragDesc.m_nodeId = signal->theData[2];
  fd.m_fragDesc.m_fragmentNo = fragNo;
  signal->theData[2] = fd.m_dummy;
  fragBuf.append(&signal->theData[2], 1);
  
  const Uint32 nextFrag = fragNo + 1;
  if(nextFrag == m_currentFragment){
    /**
     * Complete frag info for table
     */
    m_currentTable++;
    nextMeta(signal);
    return;
  }
  signal->theData[0] = RNIL;
  signal->theData[1] = ptrI;
  signal->theData[2] = tableId;
  signal->theData[3] = nextFrag; // Frag no
  suma.sendSignal(DBDIH_REF, GSN_DIGETPRIMREQ, signal, 4, JBB);
}

void
SumaParticipant::SyncRecord::completeMeta(Signal* signal){
  jam();
  SubscriptionPtr subPtr;
  suma.c_subscriptions.getPtr(subPtr, m_subscriptionPtrI);
  ndbrequire(subPtr.p->m_syncPtrI == ptrI);
  
#if PRINT_ONLY
  ndbout_c("GSN_SUB_SYNC_CONF (meta)");
#else
 
  suma.releaseSections(signal);

  if (m_error) {
    SubSyncRef * const ref = (SubSyncRef*)signal->getDataPtrSend();
    ref->subscriptionId = subPtr.p->m_subscriptionId;
    ref->subscriptionKey = subPtr.p->m_subscriptionKey;
    ref->part = SubscriptionData::MetaData;
    ref->subscriberData = subPtr.p->m_subscriberData;
    ref->errorCode = SubSyncRef::Undefined;
    suma.sendSignal(subPtr.p->m_subscriberRef, GSN_SUB_SYNC_REF, signal,
		    SubSyncRef::SignalLength, JBB);
  } else {
    SubSyncConf * const conf = (SubSyncConf*)signal->getDataPtrSend();
    conf->subscriptionId = subPtr.p->m_subscriptionId;
    conf->subscriptionKey = subPtr.p->m_subscriptionKey;
    conf->part = SubscriptionData::MetaData;
    conf->subscriberData = subPtr.p->m_subscriberData;
    suma.sendSignal(subPtr.p->m_subscriberRef, GSN_SUB_SYNC_CONF, signal,
		    SubSyncConf::SignalLength, JBB);
  }
#endif
}

/**********************************************************
 *
 * Scan interface
 *
 */

void
SumaParticipant::SyncRecord::startScan(Signal* signal){
  jam();
  
  /**
   * Get fraginfo
   */
  m_currentTable = 0;
  m_currentFragment = 0;
  
  nextScan(signal);
}

bool
SumaParticipant::SyncRecord::getNextFragment(TablePtr * tab, 
					     FragmentDescriptor * fd){
  jam();
  SubscriptionPtr subPtr;
  suma.c_subscriptions.getPtr(subPtr, m_subscriptionPtrI);
  TableList::DataBufferIterator tabIt;
  DataBuffer<15>::DataBufferIterator fragIt;
  
  m_tableList.position(tabIt, m_currentTable);
  for(; !tabIt.curr.isNull(); m_tableList.next(tabIt), m_currentTable++){
    TablePtr tabPtr;
    ndbrequire(suma.c_tables.find(tabPtr, * tabIt.data));
    if(subPtr.p->m_subscriptionType == SubCreateReq::SelectiveTableSnapshot) 
      {
	if(!subPtr.p->m_tables[tabPtr.p->m_tableId]) {
	  *tab = tabPtr;
	  return true;
	}
      }
    LocalDataBuffer<15> fragBuf(suma.c_dataBufferPool,  tabPtr.p->m_fragments);
    
    fragBuf.position(fragIt, m_currentFragment);
    for(; !fragIt.curr.isNull(); fragBuf.next(fragIt), m_currentFragment++){
      FragmentDescriptor tmp;
      tmp.m_dummy = * fragIt.data;
      if(tmp.m_fragDesc.m_nodeId == suma.getOwnNodeId()){
	* fd = tmp;
	* tab = tabPtr;
	return true;
      }
    }
    m_currentFragment = 0;
  }
  return false;
}

void
SumaParticipant::SyncRecord::nextScan(Signal* signal){
  jam();
  TablePtr tabPtr;
  FragmentDescriptor fd;
  SubscriptionPtr subPtr;
  if(!getNextFragment(&tabPtr, &fd)){
    jam();
    completeScan(signal);
    return;
  }
  suma.c_subscriptions.getPtr(subPtr, m_subscriptionPtrI);
  ndbrequire(subPtr.p->m_syncPtrI == ptrI);
 
  if(subPtr.p->m_subscriptionType == SubCreateReq::SelectiveTableSnapshot) {
    jam();
    if(!subPtr.p->m_tables[tabPtr.p->m_tableId]) {
      /*
       * table is not part of the subscription. Check next table
       */
      m_currentTable++;
      nextScan(signal);
      return;
    }
  }

  DataBuffer<15>::Head head = m_attributeList;
  if(head.getSize() == 0){
    head = tabPtr.p->m_attributes;
  }
  LocalDataBuffer<15> attrBuf(suma.c_dataBufferPool, head);
  
  ScanFragReq * req = (ScanFragReq *)signal->getDataPtrSend();
  const Uint32 parallelism = 16;
  const Uint32 attrLen = 5 + attrBuf.getSize();

  req->senderData = m_subscriptionPtrI;
  req->resultRef = suma.reference();
  req->tableId = tabPtr.p->m_tableId;
  req->requestInfo = 0;
  req->savePointId = 0;
  ScanFragReq::setLockMode(req->requestInfo, 0);
  ScanFragReq::setHoldLockFlag(req->requestInfo, 0);
  ScanFragReq::setKeyinfoFlag(req->requestInfo, 0);
  ScanFragReq::setAttrLen(req->requestInfo, attrLen);
  req->fragmentNoKeyLen = fd.m_fragDesc.m_fragmentNo;
  req->schemaVersion = tabPtr.p->m_schemaVersion;
  req->transId1 = 0;
  req->transId2 = (SUMA << 20) + (suma.getOwnNodeId() << 8);
  req->clientOpPtr = (ptrI << 16);
  req->batch_size_rows= 16;
  req->batch_size_bytes= 0;
  suma.sendSignal(DBLQH_REF, GSN_SCAN_FRAGREQ, signal, 
		  ScanFragReq::SignalLength, JBB);
  
  signal->theData[0] = ptrI;
  signal->theData[1] = 0;
  signal->theData[2] = (SUMA << 20) + (suma.getOwnNodeId() << 8);
  
  // Return all
  signal->theData[3] = attrBuf.getSize();
  signal->theData[4] = 0;
  signal->theData[5] = 0;
  signal->theData[6] = 0;
  signal->theData[7] = 0;
  
  Uint32 dataPos = 8;
  DataBuffer<15>::DataBufferIterator it;
  for(attrBuf.first(it); !it.curr.isNull(); attrBuf.next(it)){
    AttributeHeader::init(&signal->theData[dataPos++], * it.data, 0);
    if(dataPos == 25){
      suma.sendSignal(DBLQH_REF, GSN_ATTRINFO, signal, 25, JBB);
	dataPos = 3;
    }
  }
  if(dataPos != 3){
    suma.sendSignal(DBLQH_REF, GSN_ATTRINFO, signal, dataPos, JBB);
  }
  
  m_currentTableId = tabPtr.p->m_tableId;
  m_currentNoOfAttributes = attrBuf.getSize();        
}


void
SumaParticipant::execSCAN_FRAGREF(Signal* signal){
  jamEntry();

//  ScanFragRef * const ref = (ScanFragRef*)signal->getDataPtr();
  ndbrequire(false);
}

void
SumaParticipant::execSCAN_FRAGCONF(Signal* signal){
  jamEntry();

  CRASH_INSERTION(13011);

  ScanFragConf * const conf = (ScanFragConf*)signal->getDataPtr();
  
  const Uint32 completed = conf->fragmentCompleted;
  const Uint32 senderData = conf->senderData;
  const Uint32 completedOps = conf->completedOps;

  SubscriptionPtr subPtr;
  c_subscriptions.getPtr(subPtr, senderData);
  
  if(completed != 2){
    jam();
    
#if PRINT_ONLY
    SubSyncContinueConf * const conf = 
      (SubSyncContinueConf*)signal->getDataPtrSend();  
    conf->subscriptionId = subPtr.p->m_subscriptionId;
    conf->subscriptionKey = subPtr.p->m_subscriptionKey;
    execSUB_SYNC_CONTINUE_CONF(signal);
#else
    SubSyncContinueReq * const req = (SubSyncContinueReq*)signal->getDataPtrSend();
    req->subscriberData = subPtr.p->m_subscriberData;
    req->noOfRowsSent = completedOps;
    sendSignal(subPtr.p->m_subscriberRef, GSN_SUB_SYNC_CONTINUE_REQ, signal,
	       SubSyncContinueReq::SignalLength, JBB);
#endif
    return;
  }

  ndbrequire(completedOps == 0);
  
  SyncRecord* tmp = c_syncPool.getPtr(subPtr.p->m_syncPtrI);
  
  tmp->m_currentFragment++;
  tmp->nextScan(signal);
}

void
SumaParticipant::execSUB_SYNC_CONTINUE_CONF(Signal* signal){
  jamEntry();
  
  CRASH_INSERTION(13012);

  SubSyncContinueConf * const conf = 
    (SubSyncContinueConf*)signal->getDataPtr();  
  
  SubscriptionPtr subPtr;
  Subscription key; 
  key.m_subscriptionId = conf->subscriptionId;
  key.m_subscriptionKey = conf->subscriptionKey;
  
  ndbrequire(c_subscriptions.find(subPtr, key));

  ScanFragNextReq * req = (ScanFragNextReq *)signal->getDataPtrSend();
  req->senderData = subPtr.i;
  req->closeFlag = 0;
  req->transId1 = 0;
  req->transId2 = (SUMA << 20) + (getOwnNodeId() << 8);
  req->batch_size_rows = 16;
  req->batch_size_bytes = 0;
  sendSignal(DBLQH_REF, GSN_SCAN_NEXTREQ, signal, 
	     ScanFragNextReq::SignalLength, JBB);
}

void
SumaParticipant::SyncRecord::completeScan(Signal* signal){
  jam();
  //  m_tableList.release();

  SubscriptionPtr subPtr;
  suma.c_subscriptions.getPtr(subPtr, m_subscriptionPtrI);
  ndbrequire(subPtr.p->m_syncPtrI == ptrI);
  
#if PRINT_ONLY
  ndbout_c("GSN_SUB_SYNC_CONF (data)");
#else
  SubSyncConf * const conf = (SubSyncConf*)signal->getDataPtrSend();
  conf->subscriptionId = subPtr.p->m_subscriptionId;
  conf->subscriptionKey = subPtr.p->m_subscriptionKey;
  conf->part = SubscriptionData::TableData;
  conf->subscriberData = subPtr.p->m_subscriberData;
  suma.sendSignal(subPtr.p->m_subscriberRef, GSN_SUB_SYNC_CONF, signal,
		  SubSyncConf::SignalLength, JBB);
#endif
}

void
SumaParticipant::execSCAN_HBREP(Signal* signal){
  jamEntry();
#if 0
  ndbout << "execSCAN_HBREP" << endl << hex;
  for(int i = 0; i<signal->length(); i++){
    ndbout << signal->theData[i] << " ";
    if(((i + 1) % 8) == 0)
      ndbout << endl << hex;
  }
  ndbout << endl;
#endif
}

/**********************************************************
 *
 * Suma participant interface
 *
 * Creation of subscriber
 *
 */

void
SumaParticipant::execSUB_START_REQ(Signal* signal){
  jamEntry();
#ifdef NODEFAIL_DEBUG
  ndbout_c("Suma::execSUB_START_REQ");
#endif

  CRASH_INSERTION(13013);

  if (c_restartLock) {
    jam();
    //    ndbout_c("c_restartLock");
    if (RtoI(signal->getSendersBlockRef(), false) == RNIL) {
      jam();
      sendSubStartRef(signal, /** Error Code */ 0, true);
      return;
    }
    // only allow other Suma's in the nodegroup to come through for restart purposes
  }

  Subscription key; 

  SubStartReq * const req = (SubStartReq*)signal->getDataPtr();

  Uint32 senderRef            = req->senderRef;
  Uint32 senderData           = req->senderData;
  Uint32 subscriberData       = req->subscriberData;
  Uint32 subscriberRef        = req->subscriberRef;
  SubscriptionData::Part part = (SubscriptionData::Part)req->part;
  key.m_subscriptionId        = req->subscriptionId;
  key.m_subscriptionKey       = req->subscriptionKey;

  SubscriptionPtr subPtr;
  if(!c_subscriptions.find(subPtr, key)){
    jam();
    sendSubStartRef(signal, /** Error Code */ 0);
    return;
  }
  
  Ptr<SyncRecord> syncPtr;
  c_syncPool.getPtr(syncPtr, subPtr.p->m_syncPtrI);
  if (syncPtr.p->m_locked) {
    jam();
#if 0
    ndbout_c("Locked");
#endif
    sendSubStartRef(signal, /** Error Code */ 0, true);
    return;
  }
  syncPtr.p->m_locked = true;

  SubscriberPtr subbPtr;
  if(!c_subscriberPool.seize(subbPtr)){
    jam();
    syncPtr.p->m_locked = false;
    sendSubStartRef(signal, /** Error Code */ 0);
    return;
  }

  Uint32 type = subPtr.p->m_subscriptionType;

  subbPtr.p->m_senderRef  = senderRef;
  subbPtr.p->m_senderData = senderData;

  switch (type) {
  case SubCreateReq::TableEvent:
    jam();
    // we want the data to return to the API not DICT
    subbPtr.p->m_subscriberRef = subscriberRef;
    //    ndbout_c("start ref = %u", signal->getSendersBlockRef());
    //    ndbout_c("ref = %u", subbPtr.p->m_subscriberRef);
    // we use the subscription id for now, should really be API choice
    subbPtr.p->m_subscriberData = subscriberData;

#if 0
    if (RtoI(signal->getSendersBlockRef(), false) == RNIL) {
      jam();
      for (Uint32 i = 0; i < c_noNodesInGroup; i++) {
	Uint32 ref = calcSumaBlockRef(c_nodesInGroup[i]);
	if (ref != reference()) {
	  jam();
	  sendSubStartReq(subPtr, subbPtr, signal, ref);
	} else
	  jam();
      }
    }
#endif
    break;
  case SubCreateReq::DatabaseSnapshot:
  case SubCreateReq::SelectiveTableSnapshot:
    jam();
    subbPtr.p->m_subscriberRef = GREP_REF;
    subbPtr.p->m_subscriberData = subPtr.p->m_subscriberData;
    break;
  case SubCreateReq::SingleTableScan:
    jam();
    subbPtr.p->m_subscriberRef = subPtr.p->m_subscriberRef;
    subbPtr.p->m_subscriberData = subPtr.p->m_subscriberData;
  }
  
  subbPtr.p->m_subPtrI = subPtr.i;
  subbPtr.p->m_firstGCI = RNIL;
  if (type == SubCreateReq::TableEvent)
    subbPtr.p->m_lastGCI = 0;
  else
    subbPtr.p->m_lastGCI = RNIL; // disable usage of m_lastGCI
  bool ok = false;
  
  switch(part){
  case SubscriptionData::MetaData:
    ok = true;
    jam();
    c_metaSubscribers.add(subbPtr);
    sendSubStartComplete(signal, subbPtr, 0, part);
    break;
  case SubscriptionData::TableData: 
    ok = true;
    jam();
    c_prepDataSubscribers.add(subbPtr);
    syncPtr.p->startTrigger(signal);
    break;
  }
  ndbrequire(ok);
}

void
SumaParticipant::sendSubStartComplete(Signal* signal,
				      SubscriberPtr subbPtr, 
				      Uint32 firstGCI,
				      SubscriptionData::Part part){
  jam();

  SubscriptionPtr subPtr;
  c_subscriptions.getPtr(subPtr, subbPtr.p->m_subPtrI);

  Ptr<SyncRecord> syncPtr;
  c_syncPool.getPtr(syncPtr, subPtr.p->m_syncPtrI);
  syncPtr.p->m_locked = false;

  SubStartConf * const conf = (SubStartConf*)signal->getDataPtrSend();    
  
  conf->senderRef       = reference();
  conf->senderData      = subbPtr.p->m_senderData;
  conf->subscriptionId  = subPtr.p->m_subscriptionId;
  conf->subscriptionKey = subPtr.p->m_subscriptionKey;
  conf->firstGCI = firstGCI;
  conf->part = (Uint32) part;
  
  conf->subscriberData = subPtr.p->m_subscriberData;
  sendSignal(subPtr.p->m_subscriberRef, GSN_SUB_START_CONF, signal,
	     SubStartConf::SignalLength, JBB);
}

#if 0
void
SumaParticipant::sendSubStartRef(SubscriptionPtr subPtr,
				 Signal* signal, Uint32 errCode,
				 bool temporary){
  jam();
  SubStartRef * ref = (SubStartRef *)signal->getDataPtrSend();
  xxx ref->senderRef       = reference();
  xxx ref->senderData      = subPtr.p->m_senderData;
  ref->subscriptionId  = subPtr.p->m_subscriptionId;
  ref->subscriptionKey = subPtr.p->m_subscriptionKey;
  ref->part            = (Uint32) subPtr.p->m_subscriptionType;
  ref->subscriberData  = subPtr.p->m_subscriberData;
  ref->err             = errCode;
  if (temporary) {
    jam();
    ref->setTemporary();
  }
  releaseSections(signal);
  sendSignal(subPtr.p->m_subscriberRef, GSN_SUB_START_REF, signal, 
	     SubStartRef::SignalLength, JBB);
}
#endif
void
SumaParticipant::sendSubStartRef(Signal* signal, Uint32 errCode,
				 bool temporary){
  jam();
  SubStartRef * ref = (SubStartRef *)signal->getDataPtrSend();
  ref->senderRef  = reference();
  ref->err = errCode;
  if (temporary) {
    jam();
    ref->setTemporary();
  }
  releaseSections(signal);
  sendSignal(signal->getSendersBlockRef(), GSN_SUB_START_REF, signal, 
	     SubStartRef::SignalLength, JBB);
}

/**********************************************************
 *
 * Trigger admin interface
 *
 */

void
SumaParticipant::SyncRecord::startTrigger(Signal* signal){
  jam();
  m_currentTable = 0;
  m_latestTriggerId = RNIL;
  nextTrigger(signal);
}

void
SumaParticipant::SyncRecord::nextTrigger(Signal* signal){
  jam();

  TableList::DataBufferIterator it;
  
  if(!m_tableList.position(it, m_currentTable)){
    completeTrigger(signal);
    return;
  }

  SubscriptionPtr subPtr;
  suma.c_subscriptions.getPtr(subPtr, m_subscriptionPtrI);
  ndbrequire(subPtr.p->m_syncPtrI == ptrI);
  const Uint32 RT_BREAK = 48;
  Uint32 latestTriggerId = 0;
  for(Uint32 i = 0; i<RT_BREAK && !it.isNull(); i++, m_tableList.next(it)){   
    TablePtr tabPtr;
#if 0
    ndbout_c("nextTrigger tableid %u", *it.data);
#endif
    ndbrequire(suma.c_tables.find(tabPtr, *it.data));

    AttributeMask attrMask;
    createAttributeMask(attrMask, tabPtr.p);

    for(Uint32 j = 0; j<3; j++){
      i++;
      latestTriggerId = (tabPtr.p->m_schemaVersion << 18) |
	(j << 16) | tabPtr.p->m_tableId;
      if(tabPtr.p->m_hasTriggerDefined[j] == 0) {
	ndbrequire(tabPtr.p->m_triggerIds[j] == ILLEGAL_TRIGGER_ID);
#if 0
	ndbout_c("DEFINING trigger on table %u[%u]", tabPtr.p->m_tableId, j);
#endif
	CreateTrigReq * const req = (CreateTrigReq*)signal->getDataPtrSend();
	req->setUserRef(SUMA_REF);
	req->setConnectionPtr(ptrI);
	req->setTriggerType(TriggerType::SUBSCRIPTION_BEFORE);
	req->setTriggerActionTime(TriggerActionTime::TA_DETACHED);
	req->setMonitorReplicas(true);
	req->setMonitorAllAttributes(false);
	req->setReceiverRef(SUMA_REF);
	req->setTriggerId(latestTriggerId);
	req->setTriggerEvent((TriggerEvent::Value)j);
	req->setTableId(tabPtr.p->m_tableId);
	req->setAttributeMask(attrMask);
	suma.sendSignal(DBTUP_REF, GSN_CREATE_TRIG_REQ, 
			signal, CreateTrigReq::SignalLength, JBB);

      } else {
	/**
	 * Faking that a trigger has been created in order to
	 * simulate the proper behaviour.
	 * Perhaps this should be a dummy signal instead of 
	 * (ab)using CREATE_TRIG_CONF.
	 */ 
	CreateTrigConf * conf = (CreateTrigConf*)signal->getDataPtrSend();
	conf->setConnectionPtr(ptrI);
	conf->setTableId(tabPtr.p->m_tableId);
	conf->setTriggerId(latestTriggerId);
	suma.sendSignal(SUMA_REF,GSN_CREATE_TRIG_CONF,
			signal, CreateTrigConf::SignalLength, JBB);
	  
      }

    }
    m_currentTable++;
  }
  m_latestTriggerId = latestTriggerId;
}

void
SumaParticipant::SyncRecord::createAttributeMask(AttributeMask& mask, 
						 Table * table){
  jam();
  mask.clear();
  DataBuffer<15>::DataBufferIterator it;
  LocalDataBuffer<15> attrBuf(suma.c_dataBufferPool, table->m_attributes);
  for(attrBuf.first(it); !it.curr.isNull(); attrBuf.next(it)){
    mask.set(* it.data);
  }
}

void
SumaParticipant::SyncRecord::runCREATE_TRIG_CONF(Signal* signal){
  jam();
  
  CreateTrigConf * const conf = (CreateTrigConf*)signal->getDataPtr();
  const Uint32 triggerId = conf->getTriggerId();
  Uint32 type = (triggerId >> 16) & 0x3;
  Uint32 tableId = conf->getTableId();
  
  TablePtr tabPtr;
  ndbrequire(suma.c_tables.find(tabPtr, tableId));

  ndbrequire(type < 3);
  tabPtr.p->m_triggerIds[type] = triggerId;
  tabPtr.p->m_hasTriggerDefined[type]++;

  if(triggerId == m_latestTriggerId){
    jam();
    nextTrigger(signal);
  }
}

void
SumaParticipant::SyncRecord::completeTrigger(Signal* signal){
  jam();
  SubscriptionPtr subPtr;
  CRASH_INSERTION(13013);
#ifdef EVENT_PH3_DEBUG
  ndbout_c("SumaParticipant: trigger completed");
#endif
  Uint32 gci;
  suma.c_subscriptions.getPtr(subPtr, m_subscriptionPtrI);
  ndbrequire(subPtr.p->m_syncPtrI == ptrI);

  SubscriberPtr subbPtr;
  {
    bool found = false;

    for(suma.c_prepDataSubscribers.first(subbPtr);
	!subbPtr.isNull(); suma.c_prepDataSubscribers.next(subbPtr)) {
      jam();
      if(subbPtr.p->m_subPtrI == subPtr.i) {
	jam();
	found = true;
	break;
      }
    }
    ndbrequire(found);
    gci = suma.getFirstGCI(signal);
    subbPtr.p->m_firstGCI = gci;
    suma.c_prepDataSubscribers.remove(subbPtr);
    suma.c_dataSubscribers.add(subbPtr);
  }
  suma.sendSubStartComplete(signal, subbPtr, gci,  SubscriptionData::TableData);
}

void
SumaParticipant::SyncRecord::startDropTrigger(Signal* signal){
  jam();
  m_currentTable = 0;
  m_latestTriggerId = RNIL;
  nextDropTrigger(signal);
}

void
SumaParticipant::SyncRecord::nextDropTrigger(Signal* signal){
  jam();

  TableList::DataBufferIterator it;
  
  if(!m_tableList.position(it, m_currentTable)){
    completeDropTrigger(signal);
    return;
  }

  SubscriptionPtr subPtr;
  suma.c_subscriptions.getPtr(subPtr, m_subscriptionPtrI);
  ndbrequire(subPtr.p->m_syncPtrI == ptrI);

  const Uint32 RT_BREAK = 48;
  Uint32 latestTriggerId = 0;
  for(Uint32 i = 0; i<RT_BREAK && !it.isNull(); i++, m_tableList.next(it)){
    jam();
    TablePtr tabPtr;
#if 0
    ndbout_c("nextDropTrigger tableid %u", *it.data);
#endif
    ndbrequire(suma.c_tables.find(tabPtr, * it.data));

    for(Uint32 j = 0; j<3; j++){
      jam();
      ndbrequire(tabPtr.p->m_triggerIds[j] != ILLEGAL_TRIGGER_ID);
      i++;
      latestTriggerId = tabPtr.p->m_triggerIds[j];
      if(tabPtr.p->m_hasTriggerDefined[j] == 1) {
	jam();

	DropTrigReq * const req = (DropTrigReq*)signal->getDataPtrSend();
	req->setConnectionPtr(ptrI);
	req->setUserRef(SUMA_REF); // Sending to myself
	req->setRequestType(DropTrigReq::RT_USER);
	req->setTriggerType(TriggerType::SUBSCRIPTION_BEFORE);
	req->setTriggerActionTime(TriggerActionTime::TA_DETACHED);
	req->setIndexId(RNIL);

	req->setTableId(tabPtr.p->m_tableId);
	req->setTriggerId(latestTriggerId);
	req->setTriggerEvent((TriggerEvent::Value)j);

#if 0
	ndbout_c("DROPPING trigger %u = %u %u %u on table %u[%u]",
		 latestTriggerId,TriggerType::SUBSCRIPTION_BEFORE,
		 TriggerActionTime::TA_DETACHED, j, tabPtr.p->m_tableId, j);
#endif
	suma.sendSignal(DBTUP_REF, GSN_DROP_TRIG_REQ,
			signal, DropTrigReq::SignalLength, JBB);
      } else {
	jam();
	ndbrequire(tabPtr.p->m_hasTriggerDefined[j] > 1);
	/**
	 * Faking that a trigger has been dropped in order to
	 * simulate the proper behaviour.
	 * Perhaps this should be a dummy signal instead of 
	 * (ab)using DROP_TRIG_CONF.
	 */ 
	DropTrigConf * conf = (DropTrigConf*)signal->getDataPtrSend();
	conf->setConnectionPtr(ptrI);
	conf->setTableId(tabPtr.p->m_tableId);
	conf->setTriggerId(latestTriggerId);
	suma.sendSignal(SUMA_REF,GSN_DROP_TRIG_CONF,
			signal, DropTrigConf::SignalLength, JBB);
      }
    }
    m_currentTable++;
  }
  m_latestTriggerId = latestTriggerId;
}

void
SumaParticipant::SyncRecord::runDROP_TRIG_REF(Signal* signal){
  jam();
  DropTrigRef * const ref = (DropTrigRef*)signal->getDataPtr();
  if (ref->getErrorCode() != DropTrigRef::TriggerNotFound){
    ndbrequire(false);
  }
  const Uint32 triggerId = ref->getTriggerId();
  Uint32 tableId = ref->getTableId();
  runDropTrig(signal, triggerId, tableId);
}

void
SumaParticipant::SyncRecord::runDROP_TRIG_CONF(Signal* signal){
  jam();
  
  DropTrigConf * const conf = (DropTrigConf*)signal->getDataPtr();
  const Uint32 triggerId = conf->getTriggerId();
  Uint32 tableId = conf->getTableId();
  runDropTrig(signal, triggerId, tableId);
}

void
SumaParticipant::SyncRecord::runDropTrig(Signal* signal,
					 Uint32 triggerId,
					 Uint32 tableId){
  Uint32 type = (triggerId >> 16) & 0x3;
  
  TablePtr tabPtr;
  ndbrequire(suma.c_tables.find(tabPtr, tableId));

  ndbrequire(type < 3);
  ndbrequire(tabPtr.p->m_triggerIds[type] == triggerId);
  tabPtr.p->m_hasTriggerDefined[type]--;
  if (tabPtr.p->m_hasTriggerDefined[type] == 0) {
    jam();
    tabPtr.p->m_triggerIds[type] = ILLEGAL_TRIGGER_ID;
  }
  if(triggerId == m_latestTriggerId){
    jam();
    nextDropTrigger(signal);
  }
}

void
SumaParticipant::SyncRecord::completeDropTrigger(Signal* signal){
  jam();
  SubscriptionPtr subPtr;
  CRASH_INSERTION(13014);
#if 0
  ndbout_c("trigger completed");
#endif

  suma.c_subscriptions.getPtr(subPtr, m_subscriptionPtrI);
  ndbrequire(subPtr.p->m_syncPtrI == ptrI);

  bool found = false;
  SubscriberPtr subbPtr;
  for(suma.c_prepDataSubscribers.first(subbPtr);
      !subbPtr.isNull(); suma.c_prepDataSubscribers.next(subbPtr)) {
    jam();
    if(subbPtr.p->m_subPtrI == subPtr.i) {
      jam();
      found = true;
      break;
    }
  }
  ndbrequire(found);
  suma.sendSubStopComplete(signal, subbPtr);
}

/**********************************************************
 * Scan data interface
 *
 * Assumption: one execTRANSID_AI contains all attr info
 *
 */

#define SUMA_BUF_SZ1 MAX_KEY_SIZE_IN_WORDS + MAX_TUPLE_SIZE_IN_WORDS
#define SUMA_BUF_SZ MAX_ATTRIBUTES_IN_TABLE + SUMA_BUF_SZ1

static Uint32 f_bufferLock = 0;
static Uint32 f_buffer[SUMA_BUF_SZ];
static Uint32 f_trigBufferSize = 0;
static Uint32 b_bufferLock = 0;
static Uint32 b_buffer[SUMA_BUF_SZ];
static Uint32 b_trigBufferSize = 0;

void
SumaParticipant::execTRANSID_AI(Signal* signal){
  jamEntry();

  CRASH_INSERTION(13015);
  TransIdAI * const data = (TransIdAI*)signal->getDataPtr();
  const Uint32 opPtrI = data->connectPtr;
  const Uint32 length = signal->length() - 3;

  if(f_bufferLock == 0){
    f_bufferLock = opPtrI;
  } else {
    ndbrequire(f_bufferLock == opPtrI);
  }
  
  Ptr<SyncRecord> syncPtr;
  c_syncPool.getPtr(syncPtr, (opPtrI >> 16));
  
  Uint32 sum = 0;
  Uint32 * dst = f_buffer + MAX_ATTRIBUTES_IN_TABLE;
  Uint32 * headers = f_buffer;
  const Uint32 * src = &data->attrData[0];
  const Uint32 * const end = &src[length];
  
  const Uint32 attribs = syncPtr.p->m_currentNoOfAttributes;
  for(Uint32 i = 0; i<attribs; i++){
    Uint32 tmp = * src++;
    * headers++ = tmp;
    Uint32 len = AttributeHeader::getDataSize(tmp);
    
    memcpy(dst, src, 4 * len);
    dst += len;
    src += len;
    sum += len;
  }
  
  ndbrequire(src == end);

  /**
   * Send data to subscriber
   */
  LinearSectionPtr ptr[3];
  ptr[0].p = f_buffer;
  ptr[0].sz = attribs;
  
  ptr[1].p = f_buffer + MAX_ATTRIBUTES_IN_TABLE;
  ptr[1].sz = sum;

  SubscriptionPtr subPtr;
  c_subscriptions.getPtr(subPtr, syncPtr.p->m_subscriptionPtrI);
  
  /**
   * Initialize signal
   */  
  SubTableData * sdata = (SubTableData*)signal->getDataPtrSend();
  Uint32 ref = subPtr.p->m_subscriberRef;
  sdata->tableId = syncPtr.p->m_currentTableId;
  sdata->senderData = subPtr.p->m_subscriberData;
  sdata->operation = 3; // Scan
  sdata->gci = 1; // Undefined
#if PRINT_ONLY
  ndbout_c("GSN_SUB_TABLE_DATA (scan) #attr: %d len: %d", attribs, sum);
#else
  sendSignal(ref,
	     GSN_SUB_TABLE_DATA,
	     signal, 
	     SubTableData::SignalLength, JBB,
	     ptr, 2);
#endif
  
  /**
   * Reset f_bufferLock
   */
  f_bufferLock = 0;
}

/**********************************************************
 *
 * Trigger data interface
 *
 */

void
SumaParticipant::execTRIG_ATTRINFO(Signal* signal){
  jamEntry();
  
  CRASH_INSERTION(13016);
  TrigAttrInfo* const trg = (TrigAttrInfo*)signal->getDataPtr();
  const Uint32 trigId = trg->getTriggerId();

  const Uint32 dataLen = signal->length() - TrigAttrInfo::StaticLength;

  if(trg->getAttrInfoType() == TrigAttrInfo::BEFORE_VALUES){
    jam();

    ndbrequire(b_bufferLock == trigId);

    memcpy(b_buffer + b_trigBufferSize, trg->getData(), 4 * dataLen);
    b_trigBufferSize += dataLen;
    // printf("before values %u %u %u\n",trigId, dataLen,  b_trigBufferSize);
  } else {
    jam();

    if(f_bufferLock == 0){
      f_bufferLock = trigId;
      f_trigBufferSize = 0;
      b_bufferLock = trigId;
      b_trigBufferSize = 0;
    } else {
      ndbrequire(f_bufferLock == trigId);
    }

    memcpy(f_buffer + f_trigBufferSize, trg->getData(), 4 * dataLen);
    f_trigBufferSize += dataLen;
  }
}

#ifdef NODEFAIL_DEBUG2
static int theCounts[64] = {0};
#endif

Uint32 
Suma::getStoreBucket(Uint32 v)
{
  // id will contain id to responsible suma or 
  // RNIL if we don't have nodegroup info yet

  const Uint32 N = NO_OF_BUCKETS;
  const Uint32 D = v % N;            // Distibution key
  return D;
}

Uint32 
Suma::getResponsibleSumaNodeId(Uint32 D)
{
  // id will contain id to responsible suma or 
  // RNIL if we don't have nodegroup info yet

  Uint32 id;

  if (c_restartLock) {
    jam();
    //    ndbout_c("c_restartLock");
    id = RNIL;
  } else {
    jam();
    const Uint32 n = c_noNodesInGroup; // Number nodes in node group
    const Uint32 C1 = D / n;
    const Uint32 C2 = D - C1*n; // = D % n;
    const Uint32 C = C2 + C1 % n;
    for (Uint32 i = 0; i < n; i++) {
      jam();
      id = c_nodesInGroup[(C + i) % n];
      if (c_aliveNodes.get(id) &&
	  !c_preparingNodes.get(id)) {
        jam();
	break;
      }//if
    }
#ifdef NODEFAIL_DEBUG2
    theCounts[id]++;
    ndbout_c("Suma:responsible n=%u, D=%u, id = %u, count=%u",
	     n,D, id, theCounts[id]);
#endif
  }
  return id;
}

Uint32
SumaParticipant::decideWhoToSend(Uint32 nBucket, Uint32 gci){
  bool replicaFlag = true;
  Uint32 nId = RNIL;

  // bucket active/not active set by GCP_COMPLETE
  if (c_buckets[nBucket].active) {
    if (c_buckets[nBucket].handover && c_buckets[nBucket].handoverGCI <= gci) {
      jam();
      replicaFlag = true; // let the other node send this
      nId = RNIL;
      // mark this as started, if we get a node failiure now we have some lost stuff
      c_buckets[nBucket].handover_started = true;
    } else {
      jam();
      replicaFlag = false;
      nId = refToNode(reference());
    }
  } else {
    nId  = getResponsibleSumaNodeId(nBucket);
    replicaFlag = !(nId == refToNode(reference()));
    
    if (!replicaFlag) {
      if (!c_buckets[nBucket].handover) {
	jam();
	// appearently a node has failed and we are taking over sending
	// from that bucket.  Now we need to go back to latest completed
	// GCI.  Handling will depend on Subscriber and Subscription
	
	// TODO, for now we make an easy takeover
	if (gci < c_nodeFailGCI)
	  c_lastInconsistentGCI = gci;
	
	// we now have responsability for this bucket and we're actively
	// sending from that
	c_buckets[nBucket].active = true;
#ifdef HANDOVER_DEBUG
	ndbout_c("Takeover Bucket %u", nBucket);
#endif
      } else if (c_buckets[nBucket].handoverGCI > gci) {
	jam();
	replicaFlag = true; // handover going on, but don't start sending yet
	nId = RNIL;
      } else {
	jam();
#ifdef HANDOVER_DEBUG
	ndbout_c("Possible error: Will send from GCI = %u", gci);
#endif
	}
    }
  }
  
#ifdef NODEFAIL_DEBUG2
  ndbout_c("Suma:bucket %u, responsible id = %u, replicaFlag = %u",
	   nBucket, nId, (Uint32)replicaFlag);
#endif
  return replicaFlag;
}

void
SumaParticipant::execFIRE_TRIG_ORD(Signal* signal){
  jamEntry();

  CRASH_INSERTION(13016);
  FireTrigOrd* const trg = (FireTrigOrd*)signal->getDataPtr();
  const Uint32 trigId    = trg->getTriggerId();
  const Uint32 hashValue = trg->getHashValue();
  const Uint32 gci       = trg->getGCI();
  const Uint32 event     = trg->getTriggerEvent();
  const Uint32 triggerId = trg->getTriggerId();
  Uint32 tableId         = triggerId & 0xFFFF;

  ndbrequire(f_bufferLock == trigId);
  
#ifdef EVENT_DEBUG2
  ndbout_c("SumaParticipant::execFIRE_TRIG_ORD");
#endif

  Uint32 sz = trg->getNoOfPrimaryKeyWords()+trg->getNoOfAfterValueWords();
  ndbrequire(sz == f_trigBufferSize);

  /**
   * Reformat as "all headers" + "all data"
   */
  Uint32 dataLen   = 0;
  Uint32 noOfAttrs = 0;
  Uint32 * src     = f_buffer;
  Uint32 * headers = signal->theData + 25;
  Uint32 * dst     = signal->theData + 25 + MAX_ATTRIBUTES_IN_TABLE;

  LinearSectionPtr ptr[3];
  int nptr;

  ptr[0].p  = headers;
  ptr[1].p  = dst;

  while(sz > 0){
    jam();
    Uint32 tmp = * src ++;
    * headers ++ = tmp;
    Uint32 len = AttributeHeader::getDataSize(tmp);
    memcpy(dst, src, 4 * len);
    dst += len;
    src += len;
    
    noOfAttrs++;
    dataLen += len;
    sz -= (1 + len);
  }
  ndbrequire(sz == 0);

  ptr[0].sz = noOfAttrs;
  ptr[1].sz = dataLen;

  if (b_trigBufferSize > 0) {
    jam();
    ptr[2].p  = b_buffer;
    ptr[2].sz = b_trigBufferSize;
    nptr = 3;
  } else {
    jam();
    nptr = 2;
  }

  // right now only for tableEvent
  bool replicaFlag = decideWhoToSend(getStoreBucket(hashValue), gci);

  /**
   * Signal to subscriber(s)
   */
  SubTableData * data = (SubTableData*)signal->getDataPtrSend();//trg;
  data->gci            = gci;
  data->tableId        = tableId;
  data->operation      = event;
  data->noOfAttributes = noOfAttrs;
  data->dataSize       =  dataLen;

  SubscriberPtr subbPtr;
  for(c_dataSubscribers.first(subbPtr); !subbPtr.isNull();
      c_dataSubscribers.next(subbPtr)){
    if (subbPtr.p->m_firstGCI > gci) {
#ifdef EVENT_DEBUG
      ndbout_c("m_firstGCI = %u, gci = %u", subbPtr.p->m_firstGCI, gci);
#endif
      jam();
      // we're either restarting or it's a newly created subscriber
      // and waiting for the right gci
      continue;
    }

    jam();

    const Uint32 ref = subbPtr.p->m_subscriberRef;
    //    ndbout_c("ref = %u", ref);
    const Uint32 subdata = subbPtr.p->m_subscriberData;
    data->senderData = subdata;
    /*
     * get subscription ptr for this subscriber
     */
    SubscriptionPtr subPtr;
    c_subscriptions.getPtr(subPtr, subbPtr.p->m_subPtrI);

    if(!subPtr.p->m_tables[tableId]) {
      jam();
      continue;
      //continue in for-loop if the table is not part of 
      //the subscription. Otherwise, send data to subscriber.
    }
   
    if (subPtr.p->m_subscriptionType == SubCreateReq::TableEvent) {
      if (replicaFlag) {
	jam();
	c_failoverBuffer.subTableData(gci,NULL,0);
	continue;
      }
      jam();
      Uint32 tmp = data->logType;
      if (c_lastInconsistentGCI == data->gci) {
	data->setGCINotConsistent();
      }

#ifdef HANDOVER_DEBUG
      {
	static int aLongGCIName = 0;
	if (data->gci != aLongGCIName) {
	  aLongGCIName = data->gci;
	  ndbout_c("sent from GCI = %u", aLongGCIName);
	}
      }
#endif
      sendSignal(ref, GSN_SUB_TABLE_DATA, signal,
                 SubTableData::SignalLength, JBB, ptr, nptr);
      data->logType = tmp;
    } else {
      ndbassert(refToNode(ref) == 0 || refToNode(ref) == getOwnNodeId());
      jam();
#if PRINT_ONLY
      ndbout_c("GSN_SUB_TABLE_DATA to %s: op: %d #attr: %d len: %d",
	       getBlockName(refToBlock(ref)), 
	       noOfAttrs, dataLen);
    
#else
#ifdef HANDOVER_DEBUG
      {
	static int aLongGCIName2 = 0;
	if (data->gci != aLongGCIName2) {
	  aLongGCIName2 = data->gci;
	  ndbout_c("(EXECUTE_DIRECT) sent from GCI = %u to %u", aLongGCIName2, ref);
	}
      }
#endif
      EXECUTE_DIRECT(refToBlock(ref), GSN_SUB_TABLE_DATA, signal,
		     SubTableData::SignalLength);  
      jamEntry();
#endif    
    }
  }
  
  /**
   * Reset f_bufferLock
   */
  f_bufferLock = 0;
  b_bufferLock = 0;
}

void
SumaParticipant::execSUB_GCP_COMPLETE_REP(Signal* signal){
  jamEntry();

  SubGcpCompleteRep * rep = (SubGcpCompleteRep*)signal->getDataPtrSend();

  Uint32 gci = rep->gci;
  c_lastCompleteGCI = gci;

  /**
   * always send SUB_GCP_COMPLETE_REP to Grep (so 
   * Lars can do funky stuff calculating intervals,
   * even before the subscription is started
   */
  rep->senderRef  = reference();
  rep->senderData = 0; //ignored in grep
  EXECUTE_DIRECT(refToBlock(GREP_REF), GSN_SUB_GCP_COMPLETE_REP, signal,
		 SubGcpCompleteRep::SignalLength);  

  /**
   * Signal to subscriber(s)
   */

  SubscriberPtr subbPtr;
  SubscriptionPtr subPtr;
  c_dataSubscribers.first(subbPtr);
  for(; !subbPtr.isNull(); c_dataSubscribers.next(subbPtr)){

    if (subbPtr.p->m_firstGCI > gci) {
      jam();
      // we don't send SUB_GCP_COMPLETE_REP for incomplete GCI's
      continue;
    }

    const Uint32 ref = subbPtr.p->m_subscriberRef;
    rep->senderRef  = ref;
    rep->senderData = subbPtr.p->m_subscriberData;

    c_subscriptions.getPtr(subPtr, subbPtr.p->m_subPtrI);
#if PRINT_ONLY
    ndbout_c("GSN_SUB_GCP_COMPLETE_REP to %s:",
	     getBlockName(refToBlock(ref)));
#else
    /**
     * Ignore sending to GREP (since we sent earlier)
     */
    if (ref == GREP_REF) {
      jam();
      continue;
    }

    CRASH_INSERTION(13018);

    if (subPtr.p->m_subscriptionType == SubCreateReq::TableEvent)
      {
	jam();
	sendSignal(ref, GSN_SUB_GCP_COMPLETE_REP, signal,
		   SubGcpCompleteRep::SignalLength, JBB);
      }
    else
      {
	jam();
	ndbassert(refToNode(ref) == 0 || refToNode(ref) == getOwnNodeId());
	EXECUTE_DIRECT(refToBlock(ref), GSN_SUB_GCP_COMPLETE_REP, signal,
		       SubGcpCompleteRep::SignalLength);  
	jamEntry();
      }
#endif    
  }

  if (c_handoverToDo) {
    jam();
    c_handoverToDo = false;
    for( int i = 0; i < NO_OF_BUCKETS; i++) {
      if (c_buckets[i].handover) {
	if (c_buckets[i].handoverGCI > gci) {
	  jam();
	  c_handoverToDo = true; // still waiting for the right GCI
	  break; /* since all handover should happen at the same time
		  * we can break here
		  */
	} else {
	  c_buckets[i].handover = false;
#ifdef HANDOVER_DEBUG
	  ndbout_c("Handover Bucket %u", i);
#endif
	  if (getResponsibleSumaNodeId(i) == refToNode(reference())) {
	    // my bucket to be handed over to me
	    ndbrequire(!c_buckets[i].active);
	    jam();
	    c_buckets[i].active = true;
	  } else {
	    // someone else's bucket to handover to
	    ndbrequire(c_buckets[i].active);
	    jam();
	    c_buckets[i].active = false;
	  }
	}
      }
    }
  }
}

/***********************************************************
 *
 * Embryo to syncronize the Suma's so as to know if a subscriber
 * has received a GCP_COMPLETE from all suma's or not
 *
 */

void
SumaParticipant::runSUB_GCP_COMPLETE_ACC(Signal* signal){
  jam();

  SubGcpCompleteAcc * const acc = (SubGcpCompleteAcc*)signal->getDataPtr();

  Uint32 gci = acc->rep.gci;

#ifdef EVENT_DEBUG
  ndbout_c("SumaParticipant::runSUB_GCP_COMPLETE_ACC gci = %u", gci);
#endif

  c_failoverBuffer.subGcpCompleteRep(gci);
}

void
Suma::execSUB_GCP_COMPLETE_ACC(Signal* signal){
  jamEntry();

  if (RtoI(signal->getSendersBlockRef(), false) != RNIL) {
    jam();
    // Ack from other SUMA
    runSUB_GCP_COMPLETE_ACC(signal);
    return;
  }

  jam();
  // Ack from User and not an acc from other SUMA, redistribute in nodegroup

  SubGcpCompleteAcc * const acc = (SubGcpCompleteAcc*)signal->getDataPtr();
  Uint32 gci = acc->rep.gci;
  Uint32 senderRef  = acc->rep.senderRef;
  Uint32 subscriberData = acc->rep.subscriberData;
  
#ifdef EVENT_DEBUG
  ndbout_c("Suma::execSUB_GCP_COMPLETE_ACC gci = %u", gci);
#endif
  bool moreToCome = false;

  SubscriberPtr subbPtr;
  for(c_dataSubscribers.first(subbPtr);
      !subbPtr.isNull(); c_dataSubscribers.next(subbPtr)){
#ifdef EVENT_DEBUG
    ndbout_c("Suma::execSUB_GCP_COMPLETE_ACC %u == %u && %u == %u",
	     subbPtr.p->m_subscriberRef,
	     senderRef,
	     subbPtr.p->m_subscriberData,
	     subscriberData);
#endif
    if (subbPtr.p->m_subscriberRef == senderRef &&
	subbPtr.p->m_subscriberData == subscriberData) {
      jam();
#ifdef EVENT_DEBUG
      ndbout_c("Suma::execSUB_GCP_COMPLETE_ACC gci = FOUND SUBSCRIBER");
#endif
      subbPtr.p->m_lastGCI = gci;
    } else if (subbPtr.p->m_lastGCI < gci) {
      jam();
      if (subbPtr.p->m_firstGCI <= gci)
	moreToCome = true;
    } else
      jam();
  }
  
  if (!moreToCome) {
    // tell the other SUMA's that I'm done with this GCI
    jam();
    for (Uint32 i = 0; i < c_noNodesInGroup; i++) {
      Uint32 id = c_nodesInGroup[i];
      Uint32 ref = calcSumaBlockRef(id);
      if ((ref != reference()) && c_aliveNodes.get(id)) {
	jam();
	sendSignal(ref, GSN_SUB_GCP_COMPLETE_ACC, signal,
		   SubGcpCompleteAcc::SignalLength, JBB);
      } else
	jam();
    }
  }
}

static Uint32 tmpFailoverBuffer[512];
//SumaParticipant::FailoverBuffer::FailoverBuffer(DataBuffer<15>::DataBufferPool & p)
//  :  m_dataList(p), 
SumaParticipant::FailoverBuffer::FailoverBuffer()
  :
     c_gcis(tmpFailoverBuffer), c_sz(512), c_first(0), c_next(0), c_full(false)
{
}

bool SumaParticipant::FailoverBuffer::subTableData(Uint32 gci, Uint32 *src, int sz)
{
  bool ok = true;

  if (c_full) {
    ok = false;
#ifdef EVENT_DEBUG
    ndbout_c("Suma::FailoverBuffer::SubTableData buffer full gci=%u");
#endif
  } else {
    c_gcis[c_next] = gci;
    c_next++;
    if (c_next == c_sz) c_next = 0;
    if (c_next == c_first)
      c_full = true;
    //    ndbout_c("%u %u %u",c_first,c_next,c_sz);
  }
  return ok;
}
bool SumaParticipant::FailoverBuffer::subGcpCompleteRep(Uint32 gci)
{
  bool ok = true;

  //  ndbout_c("Empty");
  while (true) {
    if (c_first == c_next && !c_full)
      break;
    if (c_gcis[c_first] > gci)
      break;
    c_full = false;
    c_first++;
    if (c_first == c_sz) c_first = 0;
    //    ndbout_c("%u %u %u : ",c_first,c_next,c_sz);
  }

  return ok;
}
bool SumaParticipant::FailoverBuffer::nodeFailRep()
{
  bool ok = true;
  while (true) {
    if (c_first == c_next && !c_full)
      break;

#ifdef EVENT_DEBUG
    ndbout_c("Suma::FailoverBuffer::NodeFailRep resending gci=%u", c_gcis[c_first]);
#endif
    c_full = false;
    c_first++;
    if (c_first == c_sz) c_first = 0;
  }
  return ok;
}

/**********************************************************
 * Suma participant interface
 *
 * Stopping and removing of subscriber
 *
 */

void
SumaParticipant::execSUB_STOP_REQ(Signal* signal){
  jamEntry();
  
  CRASH_INSERTION(13019);

  SubStopReq * const req = (SubStopReq*)signal->getDataPtr();
  Uint32 senderRef      = signal->getSendersBlockRef();
  Uint32 senderData     = req->senderData;
  Uint32 subscriberRef  = req->subscriberRef;
  Uint32 subscriberData = req->subscriberData;
  SubscriptionPtr subPtr;
  Subscription key; 
  key.m_subscriptionId  = req->subscriptionId;
  key.m_subscriptionKey = req->subscriptionKey;
  Uint32 part = req->part;
  
  if (key.m_subscriptionKey == 0 &&
      key.m_subscriptionId == 0 &&
      subscriberData == 0) {
    SubStopConf* conf = (SubStopConf*)signal->getDataPtrSend();
    
    conf->senderRef       = reference();
    conf->senderData      = senderData;
    conf->subscriptionId  = key.m_subscriptionId;
    conf->subscriptionKey = key.m_subscriptionKey;
    conf->subscriberData  = subscriberData;

    sendSignal(senderRef, GSN_SUB_STOP_CONF, signal,
	       SubStopConf::SignalLength, JBB);

    removeSubscribersOnNode(signal, refToNode(subscriberRef));
    return;
  }

  if(!c_subscriptions.find(subPtr, key)){
    jam();
    sendSubStopRef(signal, GrepError::SUBSCRIPTION_ID_NOT_FOUND);
    return;
  }
  
  ndbrequire(part == SubscriptionData::TableData);

  SubscriberPtr subbPtr;
  if (senderRef == reference()){
    jam();
    c_subscriberPool.getPtr(subbPtr, senderData);
    ndbrequire(subbPtr.p->m_subPtrI == subPtr.i && 
	       subbPtr.p->m_subscriberRef == subscriberRef &&
	       subbPtr.p->m_subscriberData == subscriberData);
    c_removeDataSubscribers.remove(subbPtr);
  } else {
    bool found = false;
    jam();
    c_dataSubscribers.first(subbPtr);
    for (;!subbPtr.isNull(); c_dataSubscribers.next(subbPtr)){
      jam();
      if (subbPtr.p->m_subPtrI == subPtr.i && 
	  subbPtr.p->m_subscriberRef == subscriberRef &&
	  subbPtr.p->m_subscriberData == subscriberData){
	//	ndbout_c("STOP_REQ: before c_dataSubscribers.release");
	jam();
	c_dataSubscribers.remove(subbPtr);
	found = true;
	break;
      }
    }
    /**
     * If we didn't find anyone, send ref
     */
    if (!found) {
      jam();
      sendSubStopRef(signal, GrepError::SUBSCRIBER_NOT_FOUND);
      return;
    }
  }

  subbPtr.p->m_senderRef  = senderRef; // store ref to requestor
  subbPtr.p->m_senderData = senderData; // store ref to requestor
  c_prepDataSubscribers.add(subbPtr);

  Ptr<SyncRecord> syncPtr;
  c_syncPool.getPtr(syncPtr, subPtr.p->m_syncPtrI);
  if (syncPtr.p->m_locked) {
    jam();
    sendSubStopRef(signal, /** Error Code */ 0, true);
    return;
  }
  syncPtr.p->m_locked = true;

  syncPtr.p->startDropTrigger(signal);
}

void
SumaParticipant::sendSubStopComplete(Signal* signal, SubscriberPtr subbPtr){
  jam();

  CRASH_INSERTION(13020);

  SubscriptionPtr subPtr;
  c_subscriptions.getPtr(subPtr, subbPtr.p->m_subPtrI);

  Ptr<SyncRecord> syncPtr;
  c_syncPool.getPtr(syncPtr, subPtr.p->m_syncPtrI);
  syncPtr.p->m_locked = false;

  SubStopConf * const conf = (SubStopConf*)signal->getDataPtrSend();
  
  conf->senderRef = reference();
  conf->senderData = subbPtr.p->m_senderData;
  conf->subscriptionId  = subPtr.p->m_subscriptionId;
  conf->subscriptionKey = subPtr.p->m_subscriptionKey;
  conf->subscriberData  = subbPtr.p->m_subscriberData;
  Uint32 senderRef = subbPtr.p->m_senderRef;

  c_prepDataSubscribers.release(subbPtr);
  sendSignal(senderRef, GSN_SUB_STOP_CONF, signal,
	     SubStopConf::SignalLength, JBB);
}

void
SumaParticipant::sendSubStopRef(Signal* signal, Uint32 errCode,
				bool temporary){
  jam();
  SubStopRef  * ref = (SubStopRef *)signal->getDataPtrSend();
  ref->senderRef = reference();
  ref->errorCode = errCode;
  if (temporary) {
    ref->setTemporary();
  }
  sendSignal(signal->getSendersBlockRef(), 
	     GSN_SUB_STOP_REF, 
	     signal, 
	     SubStopRef::SignalLength,
	     JBB);
  return;
}

/**************************************************************
 *
 * Removing subscription
 *
 */

void
SumaParticipant::execSUB_REMOVE_REQ(Signal* signal) {
  jamEntry();

  Uint32 senderRef = signal->getSendersBlockRef();

  CRASH_INSERTION(13021);

  const SubRemoveReq req = *(SubRemoveReq*)signal->getDataPtr();
  SubscriptionPtr subPtr;
  Subscription key;
  key.m_subscriptionId  = req.subscriptionId;
  key.m_subscriptionKey = req.subscriptionKey;
  
  if(!c_subscriptions.find(subPtr, key)) {
    jam();
    sendSubRemoveRef(signal, req, (Uint32) GrepError::SUBSCRIPTION_ID_NOT_FOUND);
    return;
  }
  
  int count = 0;
  {
    jam();
    SubscriberPtr i_subbPtr;
    for(c_prepDataSubscribers.first(i_subbPtr);
	!i_subbPtr.isNull(); c_prepDataSubscribers.next(i_subbPtr)){
      jam();
      if( i_subbPtr.p->m_subPtrI == subPtr.i ) {
	jam();
	sendSubRemoveRef(signal, req, /* ErrorCode */ 0, true);
	return;
	//	c_prepDataSubscribers.release(subbPtr);
      }
    }
    c_dataSubscribers.first(i_subbPtr);
    while(!i_subbPtr.isNull()){
      jam();
      SubscriberPtr subbPtr = i_subbPtr;
      c_dataSubscribers.next(i_subbPtr);
      if( subbPtr.p->m_subPtrI == subPtr.i ) {
	jam();
	sendSubRemoveRef(signal, req, /* ErrorCode */ 0, true);
	return;
	/* Unfinished/untested code.  If remove should be possible
	 * even if subscribers are left these have to be stopped 
	 * first. See m_markRemove, m_nSubscribers. We need also to
	 * block remove for this subscription so that multiple
	 * removes is not possible...
	 */
	c_dataSubscribers.remove(subbPtr);
	c_removeDataSubscribers.add(subbPtr);
	count++;
      }
    }
    c_metaSubscribers.first(i_subbPtr);
    while(!i_subbPtr.isNull()){
      jam();
      SubscriberPtr subbPtr = i_subbPtr;
      c_metaSubscribers.next(i_subbPtr);
      if( subbPtr.p->m_subPtrI == subPtr.i ){
	jam();
	c_metaSubscribers.release(subbPtr);
      }
    }
  }

  subPtr.p->m_senderRef  = senderRef;
  subPtr.p->m_senderData = req.senderData;

  if (count > 0){
    jam();
    ndbrequire(false); // code not finalized
    subPtr.p->m_markRemove = true;
    subPtr.p->m_nSubscribers = count;
    sendSubStopReq(signal);
  } else {
    completeSubRemoveReq(signal, subPtr);
  }
}

void
SumaParticipant::completeSubRemoveReq(Signal* signal, SubscriptionPtr subPtr) {
  Uint32 subscriptionId  = subPtr.p->m_subscriptionId;
  Uint32 subscriptionKey = subPtr.p->m_subscriptionKey;
  Uint32 senderRef       = subPtr.p->m_senderRef;
  Uint32 senderData      = subPtr.p->m_senderData;

  {
    Ptr<SyncRecord> syncPtr;
    c_syncPool.getPtr(syncPtr, subPtr.p->m_syncPtrI);
	
    syncPtr.p->release();
    c_syncPool.release(syncPtr);
  }

  //  if (subPtr.p->m_subscriptionType != SubCreateReq::TableEvent) {
  //    jam();
  //    senderRef = subPtr.p->m_subscriberRef;
  //  }
  c_subscriptions.release(subPtr);

  /**
   * I was the last subscription to be remove so clear c_tables
   */
#if 0
  ndbout_c("c_subscriptionPool.getSize() %d c_subscriptionPool.getNoOfFree()%d",
	   c_subscriptionPool.getSize(),c_subscriptionPool.getNoOfFree()+1);
#endif

  if(c_subscriptionPool.getSize() == c_subscriptionPool.getNoOfFree()+1) {
    jam();
#if 0
    ndbout_c("SUB_REMOVE_REQ:Clearing c_tables");
#endif
    KeyTable<Table>::Iterator it;
    for(c_tables.first(it); !it.isNull(); ){
      
      it.curr.p->release(* this);
      
      TablePtr tabPtr = it.curr;
      
      c_tables.next(it);
      c_tables.release(tabPtr);
    }
  }
  
  SubRemoveConf * const conf = (SubRemoveConf*)signal->getDataPtrSend();
  conf->senderRef            = reference();
  conf->senderData           = senderData;
  conf->subscriptionId       = subscriptionId;
  conf->subscriptionKey      = subscriptionKey;

  sendSignal(senderRef, GSN_SUB_REMOVE_CONF, signal,
	     SubRemoveConf::SignalLength, JBB);
}

void
SumaParticipant::sendSubRemoveRef(Signal* signal, const SubRemoveReq& req,
				  Uint32 errCode, bool temporary){
  jam();
  SubRemoveRef  * ref = (SubRemoveRef *)signal->getDataPtrSend();
  ref->senderRef  = reference();
  ref->senderData = req.senderData;
  ref->err = errCode;
  if (temporary)
    ref->setTemporary();
  releaseSections(signal);
  sendSignal(signal->getSendersBlockRef(), GSN_SUB_REMOVE_REF, 
	     signal, SubRemoveRef::SignalLength, JBB);
  return;
}

void
SumaParticipant::Table::release(SumaParticipant & suma){
  jam();

  LocalDataBuffer<15> attrBuf(suma.c_dataBufferPool, m_attributes);
  attrBuf.release();

  LocalDataBuffer<15> fragBuf(suma.c_dataBufferPool, m_fragments);
  fragBuf.release();
}

void
SumaParticipant::SyncRecord::release(){
  jam();
  m_tableList.release();

  LocalDataBuffer<15> attrBuf(suma.c_dataBufferPool, m_attributeList);
  attrBuf.release();  
}


/**************************************************************
 *
 * Restarting remote node functions, master functionality
 * (slave does nothing special)
 * - triggered on INCL_NODEREQ calling startNode
 * - included node will issue START_ME when it's ready to start
 * the subscribers
 *
 */

Suma::Restart::Restart(Suma& s) : suma(s) {
  for (int i = 0; i < MAX_REPLICAS; i++) {
    c_okToStart[i]      = false;
    c_waitingToStart[i] = false;
  }
};

void
Suma::Restart::resetNode(Uint32 sumaRef)
{
  jam();
  int I = suma.RtoI(sumaRef);
  c_okToStart[I] = false;
  c_waitingToStart[I] = false;
}

void
Suma::Restart::startNode(Signal* signal, Uint32 sumaRef)
{
  jam();
  resetNode(sumaRef);

  // right now we can only handle restarting one node
  // at a time in a node group

  createSubscription(signal, sumaRef);
}

void 
Suma::Restart::createSubscription(Signal* signal, Uint32 sumaRef) {
  jam();
  suma.c_subscriptions.first(c_subPtr);
  nextSubscription(signal, sumaRef);
}

void 
Suma::Restart::nextSubscription(Signal* signal, Uint32 sumaRef) {
  jam();
  if (c_subPtr.isNull()) {
    jam();
    completeSubscription(signal, sumaRef);
    return;
  }
  SubscriptionPtr subPtr;
  subPtr.i = c_subPtr.curr.i;
  subPtr.p = suma.c_subscriptions.getPtr(subPtr.i);

  suma.c_subscriptions.next(c_subPtr);

  SubCreateReq * req = (SubCreateReq *)signal->getDataPtrSend();
      
  req->subscriberRef    = suma.reference();
  req->subscriberData   = subPtr.i;
  req->subscriptionId   = subPtr.p->m_subscriptionId;
  req->subscriptionKey  = subPtr.p->m_subscriptionKey;
  req->subscriptionType = subPtr.p->m_subscriptionType |
    SubCreateReq::RestartFlag;

  switch (subPtr.p->m_subscriptionType) {
  case SubCreateReq::TableEvent:
  case SubCreateReq::SelectiveTableSnapshot:
  case SubCreateReq::DatabaseSnapshot: {
    jam();
      
    Ptr<SyncRecord> syncPtr;
    suma.c_syncPool.getPtr(syncPtr, subPtr.p->m_syncPtrI);
    syncPtr.p->m_tableList.first(syncPtr.p->m_tableList_it);

    ndbrequire(!syncPtr.p->m_tableList_it.isNull());

    req->tableId = *syncPtr.p->m_tableList_it.data;
      
#if 0
    for (int i = 0; i < MAX_TABLES; i++)
      if (subPtr.p->m_tables[i]) {
	req->tableId = i;
	break;
      }
#endif

    suma.sendSignal(sumaRef, GSN_SUB_CREATE_REQ, signal,
		    SubCreateReq::SignalLength+1 /*to get table Id*/, JBB);
    return;
  }
  case SubCreateReq::SingleTableScan :
    // TODO
    jam();
    return;
  }
  ndbrequire(false);
}

void 
Suma::execSUB_CREATE_CONF(Signal* signal) {
  jamEntry();
#ifdef NODEFAIL_DEBUG
  ndbout_c("Suma::execSUB_CREATE_CONF");
#endif

  const Uint32 senderRef = signal->senderBlockRef();

  SubCreateConf * const conf = (SubCreateConf *)signal->getDataPtr();

  Subscription key;
  const Uint32 subscriberData = conf->subscriberData;
  key.m_subscriptionId        = conf->subscriptionId;
  key.m_subscriptionKey       = conf->subscriptionKey;
  
  SubscriptionPtr subPtr;
  ndbrequire(c_subscriptions.find(subPtr, key));

  switch(subPtr.p->m_subscriptionType) {
  case SubCreateReq::TableEvent:
  case SubCreateReq::SelectiveTableSnapshot:
  case SubCreateReq::DatabaseSnapshot:
    {
      Ptr<SyncRecord> syncPtr;
      c_syncPool.getPtr(syncPtr, subPtr.p->m_syncPtrI);

      syncPtr.p->m_tableList.next(syncPtr.p->m_tableList_it);
      if (syncPtr.p->m_tableList_it.isNull()) {
	jam();
	SubSyncReq *req = (SubSyncReq *)signal->getDataPtrSend();
    
	req->subscriptionId  = key.m_subscriptionId;
	req->subscriptionKey = key.m_subscriptionKey;
	req->subscriberData  = subscriberData;
	req->part            = (Uint32) SubscriptionData::MetaData;

	sendSignal(senderRef, GSN_SUB_SYNC_REQ, signal,
		   SubSyncReq::SignalLength, JBB);
      } else {
	jam();
	SubCreateReq * req = (SubCreateReq *)signal->getDataPtrSend();
      
	req->subscriberRef    = reference();
	req->subscriberData   = subPtr.i;
	req->subscriptionId   = subPtr.p->m_subscriptionId;
	req->subscriptionKey  = subPtr.p->m_subscriptionKey;
	req->subscriptionType = subPtr.p->m_subscriptionType |
	  SubCreateReq::RestartFlag |
	  SubCreateReq::AddTableFlag;

	req->tableId = *syncPtr.p->m_tableList_it.data;

	sendSignal(senderRef, GSN_SUB_CREATE_REQ, signal,
		   SubCreateReq::SignalLength+1 /*to get table Id*/, JBB);
      }
    }
    return;
  case SubCreateReq::SingleTableScan:
    ndbrequire(false);
  }
  ndbrequire(false);
}

void 
Suma::execSUB_CREATE_REF(Signal* signal) {
  jamEntry();
#ifdef NODEFAIL_DEBUG
  ndbout_c("Suma::execSUB_CREATE_REF");
#endif
  //ndbrequire(false);
}

void 
Suma::execSUB_SYNC_CONF(Signal* signal) {
  jamEntry();
#ifdef NODEFAIL_DEBUG
  ndbout_c("Suma::execSUB_SYNC_CONF");
#endif
  Uint32 sumaRef = signal->getSendersBlockRef();

  SubSyncConf *conf = (SubSyncConf *)signal->getDataPtr();
  Subscription key;

  key.m_subscriptionId            = conf->subscriptionId;
  key.m_subscriptionKey           = conf->subscriptionKey;
  //  SubscriptionData::Part part     = (SubscriptionData::Part)conf->part;
  //  const Uint32 subscriberData     = conf->subscriberData;

  SubscriptionPtr subPtr;
  c_subscriptions.find(subPtr, key);

  switch(subPtr.p->m_subscriptionType) {
  case SubCreateReq::TableEvent:
  case SubCreateReq::SelectiveTableSnapshot:
  case SubCreateReq::DatabaseSnapshot:
    jam();
    Restart.nextSubscription(signal, sumaRef);
    return;
  case SubCreateReq::SingleTableScan:
    ndbrequire(false);
    return;
  }
  ndbrequire(false);
}

void 
Suma::execSUB_SYNC_REF(Signal* signal) {
  jamEntry();
#ifdef NODEFAIL_DEBUG
  ndbout_c("Suma::execSUB_SYNC_REF");
#endif
  //ndbrequire(false);
}

void
Suma::execSUMA_START_ME(Signal* signal) {
  jamEntry();
#ifdef NODEFAIL_DEBUG
  ndbout_c("Suma::execSUMA_START_ME");
#endif

  Restart.runSUMA_START_ME(signal, signal->getSendersBlockRef());
}

void
Suma::Restart::runSUMA_START_ME(Signal* signal, Uint32 sumaRef) {
  int I = suma.RtoI(sumaRef);

  // restarting Suma is ready for SUB_START_REQ
  if (c_waitingToStart[I]) {
    // we've waited with startSubscriber since restarting suma was not ready
    c_waitingToStart[I] = false;
    startSubscriber(signal, sumaRef);
  } else {
    // do startSubscriber as soon as its time
    c_okToStart[I] = true;
  }
}

void 
Suma::Restart::completeSubscription(Signal* signal, Uint32 sumaRef) {
  jam();
  int I = suma.RtoI(sumaRef);

  if (c_okToStart[I]) {// otherwise will start when START_ME comes
    c_okToStart[I] = false;
    startSubscriber(signal, sumaRef);
  } else {
    c_waitingToStart[I] = true;
  }
}

void 
Suma::Restart::startSubscriber(Signal* signal, Uint32 sumaRef) {
  jam();
  suma.c_dataSubscribers.first(c_subbPtr);
  nextSubscriber(signal, sumaRef);
}

void
Suma::Restart::sendSubStartReq(SubscriptionPtr subPtr, SubscriberPtr subbPtr,
			       Signal* signal, Uint32 sumaRef)
{
  jam();
  SubStartReq * req = (SubStartReq *)signal->getDataPtrSend();
      
  req->senderRef        = suma.reference();
  req->senderData       = subbPtr.p->m_senderData;
  req->subscriptionId   = subPtr.p->m_subscriptionId;
  req->subscriptionKey  = subPtr.p->m_subscriptionKey;
  req->part             = SubscriptionData::TableData;
  req->subscriberData   = subbPtr.p->m_subscriberData;
  req->subscriberRef    = subbPtr.p->m_subscriberRef;
      
  // restarting suma will not respond to this until startphase 5
  // since it is not until then data copying has been completed
#ifdef NODEFAIL_DEBUG
  ndbout_c("Suma::Restart::sendSubStartReq sending GSN_SUB_START_REQ id=%u key=%u",
	   req->subscriptionId, req->subscriptionKey);
#endif
  suma.sendSignal(sumaRef, GSN_SUB_START_REQ,
		  signal, SubStartReq::SignalLength2, JBB);
}

void 
Suma::execSUB_START_CONF(Signal* signal) {
  jamEntry();
#ifdef NODEFAIL_DEBUG
  ndbout_c("Suma::execSUB_START_CONF");
#endif
  Uint32 sumaRef = signal->getSendersBlockRef();
  Restart.nextSubscriber(signal, sumaRef);
}

void 
Suma::execSUB_START_REF(Signal* signal) {
  jamEntry();
#ifdef NODEFAIL_DEBUG
  ndbout_c("Suma::execSUB_START_REF");
#endif
  //ndbrequire(false);
}

void 
Suma::Restart::nextSubscriber(Signal* signal, Uint32 sumaRef) {
  jam();
  if (c_subbPtr.isNull()) {
    jam();
    completeSubscriber(signal, sumaRef);
    return;
  }
  
  SubscriberPtr subbPtr = c_subbPtr;
  suma.c_dataSubscribers.next(c_subbPtr);

  /*
   * get subscription ptr for this subscriber
   */

  SubscriptionPtr subPtr;
  suma.c_subscriptions.getPtr(subPtr, subbPtr.p->m_subPtrI);
  switch (subPtr.p->m_subscriptionType) {
  case SubCreateReq::TableEvent:
  case SubCreateReq::SelectiveTableSnapshot:
  case SubCreateReq::DatabaseSnapshot:
    {
      jam();
      sendSubStartReq(subPtr, subbPtr, signal, sumaRef);
#if 0
      SubStartReq * req = (SubStartReq *)signal->getDataPtrSend();
      
      req->senderRef        = reference();
      req->senderData       = subbPtr.p->m_senderData;
      req->subscriptionId   = subPtr.p->m_subscriptionId;
      req->subscriptionKey  = subPtr.p->m_subscriptionKey;
      req->part             = SubscriptionData::TableData;
      req->subscriberData   = subbPtr.p->m_subscriberData;
      req->subscriberRef    = subbPtr.p->m_subscriberRef;
      
      // restarting suma will not respond to this until startphase 5
      // since it is not until then data copying has been completed
#ifdef NODEFAIL_DEBUG
      ndbout_c("Suma::nextSubscriber sending GSN_SUB_START_REQ id=%u key=%u",
	       req->subscriptionId, req->subscriptionKey);
#endif
      suma.sendSignal(sumaRef, GSN_SUB_START_REQ,
		      signal, SubStartReq::SignalLength2, JBB);
#endif
    }
  return;
  case SubCreateReq::SingleTableScan:
    ndbrequire(false);
    return;
  }
  ndbrequire(false);
}

void 
Suma::Restart::completeSubscriber(Signal* signal, Uint32 sumaRef) {
  completeRestartingNode(signal, sumaRef);
}

void
Suma::Restart::completeRestartingNode(Signal* signal, Uint32 sumaRef) {
  jam();
  SumaHandoverReq * req = (SumaHandoverReq *)signal->getDataPtrSend();

  req->gci = suma.getFirstGCI(signal);

  suma.sendSignal(sumaRef, GSN_SUMA_HANDOVER_REQ, signal,
		  SumaHandoverReq::SignalLength, JBB);
}

// only run on restarting suma

void
Suma::execSUMA_HANDOVER_REQ(Signal* signal)
{
  jamEntry();
  //  Uint32 sumaRef = signal->getSendersBlockRef();
  SumaHandoverReq const * req = (SumaHandoverReq *)signal->getDataPtr();

  Uint32 gci = req->gci;
  Uint32 new_gci = getFirstGCI(signal);

  if (new_gci > gci) {
    gci = new_gci;
  }

  { // all recreated subscribers at restarting SUMA start at same GCI
    SubscriberPtr subbPtr;
    for(c_dataSubscribers.first(subbPtr);
	!subbPtr.isNull();
	c_dataSubscribers.next(subbPtr)){
      subbPtr.p->m_firstGCI = gci;
    }
  }

#ifdef NODEFAIL_DEBUG
  ndbout_c("Suma::execSUMA_HANDOVER_REQ, gci = %u", gci);
#endif

  c_handoverToDo = false;
  c_restartLock = false;
  {
#ifdef HANDOVER_DEBUG
    int c = 0;
#endif
    for( int i = 0; i < NO_OF_BUCKETS; i++) {
      jam();
      if (getResponsibleSumaNodeId(i) == refToNode(reference())) {
#ifdef HANDOVER_DEBUG
	c++;
#endif
        jam();
	c_buckets[i].active = false;
	c_buckets[i].handoverGCI = gci;
	c_buckets[i].handover = true;
	c_buckets[i].handover_started = false;
	c_handoverToDo = true;
      }
    }
#ifdef HANDOVER_DEBUG
    ndbout_c("prepared handover of bucket %u buckets", c);
#endif
  }

  for (Uint32 i = 0; i < c_noNodesInGroup; i++) {
    jam();
    Uint32 ref = calcSumaBlockRef(c_nodesInGroup[i]);
    if (ref != reference()) {
      jam();
      sendSignal(ref, GSN_SUMA_HANDOVER_CONF, signal,
		 SumaHandoverConf::SignalLength, JBB);
    }//if
  }
}

// only run on all but restarting suma
void
Suma::execSUMA_HANDOVER_CONF(Signal* signal) {
  jamEntry();
  Uint32 sumaRef = signal->getSendersBlockRef();
  SumaHandoverConf const * conf = (SumaHandoverConf *)signal->getDataPtr();

  Uint32 gci = conf->gci;

#ifdef HANDOVER_DEBUG
  ndbout_c("Suma::execSUMA_HANDOVER_CONF, gci = %u", gci);
#endif

  /* TODO, if we are restarting several SUMA's (>2 in a nodegroup)
   * we have to collect all these conf's before proceding
   */

  // restarting node is now prepared and ready
  c_preparingNodes.clear(refToNode(sumaRef)); /* !! important to do before
					       * below since it affects
					       * getResponsibleSumaNodeId()
					       */

  c_handoverToDo = false;
  // mark all active buckets really belonging to restarting SUMA
  for( int i = 0; i < NO_OF_BUCKETS; i++) {
    if (c_buckets[i].active) {
      // I'm running this bucket
      if (getResponsibleSumaNodeId(i) == refToNode(sumaRef)) {
	// but it should really be the restarted node
	c_buckets[i].handoverGCI = gci;
	c_buckets[i].handover = true;
	c_buckets[i].handover_started = false;
	c_handoverToDo = true;
      }
    }
  }
}

template void append(DataBuffer<11>&,SegmentedSectionPtr,SectionSegmentPool&);


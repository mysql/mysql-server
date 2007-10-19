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

#include <my_config.h>
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
#include <signaldata/CreateTab.hpp>
#include <signaldata/DropTab.hpp>
#include <signaldata/AlterTable.hpp>
#include <signaldata/AlterTab.hpp>
#include <signaldata/DihFragCount.hpp>
#include <signaldata/SystemError.hpp>

#include <ndbapi/NdbDictionary.hpp>

#include <DebuggerNames.hpp>
#include <../dbtup/Dbtup.hpp>
#include <../dbdih/Dbdih.hpp>

//#define HANDOVER_DEBUG
//#define NODEFAIL_DEBUG
//#define NODEFAIL_DEBUG2
//#define DEBUG_SUMA_SEQUENCE
//#define EVENT_DEBUG
//#define EVENT_PH3_DEBUG
//#define EVENT_DEBUG2
#if 0
#undef DBUG_ENTER
#undef DBUG_PRINT
#undef DBUG_RETURN
#undef DBUG_VOID_RETURN

#define DBUG_ENTER(a) {ndbout_c("%s:%d >%s", __FILE__, __LINE__, a);}
#define DBUG_PRINT(a,b) {ndbout << __FILE__ << ":" << __LINE__ << " " << a << ": "; ndbout_c b ;}
#define DBUG_RETURN(a) { ndbout_c("%s:%d <", __FILE__, __LINE__); return(a); }
#define DBUG_VOID_RETURN { ndbout_c("%s:%d <", __FILE__, __LINE__); return; }
#endif

/**
 * @todo:
 * SUMA crashes if an index is created at the same time as
 * global replication. Very easy to reproduce using testIndex.
 * Note: This only happens occasionally, but is quite easy to reprod.
 */

Uint32 g_subPtrI = RNIL;
static const Uint32 SUMA_SEQUENCE = 0xBABEBABE;

static const Uint32 MAX_CONCURRENT_GCP = 2;

/**************************************************************
 *
 * Start of suma
 *
 */

#define PRINT_ONLY 0

void
Suma::getNodeGroupMembers(Signal* signal)
{
  jam();
  DBUG_ENTER("Suma::getNodeGroupMembers");
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
  c_nodes_in_nodegroup_mask.assign(sd->mask);
  c_noNodesInGroup = c_nodes_in_nodegroup_mask.count();
  Uint32 i, pos= 0;
  
  for (i = 0; i < MAX_NDB_NODES; i++) {
    if (sd->mask.get(i)) 
    {
      c_nodesInGroup[pos++] = i;
    }
  }
  
  const Uint32 replicas= c_noNodesInGroup;

  Uint32 buckets= 1;
  for(i = 1; i <= replicas; i++)
    buckets *= i;
  
  for(i = 0; i<buckets; i++)
  {
    Bucket* ptr= c_buckets+i;
    for(Uint32 j= 0; j< replicas; j++)
    {
      ptr->m_nodes[j] = c_nodesInGroup[(i + j) % replicas];
    }
  }
  
  c_no_of_buckets= buckets;
  ndbrequire(c_noNodesInGroup > 0); // at least 1 node in the nodegroup

#ifndef DBUG_OFF
  for (Uint32 i = 0; i < c_noNodesInGroup; i++) {
    DBUG_PRINT("exit",("Suma: NodeGroup %u, me %u, "
		       "member[%u] %u",
		       c_nodeGroup, getOwnNodeId(), 
		       i, c_nodesInGroup[i]));
  }
#endif

  DBUG_VOID_RETURN;
}

void 
Suma::execREAD_CONFIG_REQ(Signal* signal)
{
  jamEntry();

  const ReadConfigReq * req = (ReadConfigReq*)signal->getDataPtr();

  Uint32 ref = req->senderRef;
  Uint32 senderData = req->senderData;

  const ndb_mgm_configuration_iterator * p = 
    m_ctx.m_config.getOwnConfigIterator();
  ndbrequire(p != 0);

  // SumaParticipant
  Uint32 noTables, noAttrs;
  ndb_mgm_get_int_parameter(p, CFG_DB_NO_TABLES,  
			    &noTables);
  ndb_mgm_get_int_parameter(p, CFG_DB_NO_ATTRIBUTES,  
			    &noAttrs);

  c_tablePool.setSize(noTables);
  c_tables.setSize(noTables);
  
  c_subscriptions.setSize(noTables);
  c_subscriberPool.setSize(2*noTables);
  
  c_subscriptionPool.setSize(noTables);
  c_syncPool.setSize(2);
  c_dataBufferPool.setSize(noAttrs);

  // Calculate needed gcp pool as 10 records + the ones needed
  // during a possible api timeout
  Uint32 dbApiHbInterval, gcpInterval;
  ndb_mgm_get_int_parameter(p, CFG_DB_API_HEARTBEAT_INTERVAL,
			    &dbApiHbInterval);
  ndb_mgm_get_int_parameter(p, CFG_DB_GCP_INTERVAL,
                            &gcpInterval);
  c_gcp_pool.setSize(10 + (4*dbApiHbInterval)/gcpInterval);
  
  c_page_chunk_pool.setSize(50);

  {
    SLList<SyncRecord> tmp(c_syncPool);
    Ptr<SyncRecord> ptr;
    while(tmp.seize(ptr))
      new (ptr.p) SyncRecord(* this, c_dataBufferPool);
    tmp.release();
  }

  // Suma
  c_masterNodeId = getOwnNodeId();

  c_nodeGroup = c_noNodesInGroup = 0;
  for (int i = 0; i < MAX_REPLICAS; i++) {
    c_nodesInGroup[i]   = 0;
  }

  m_first_free_page= RNIL;
  
  c_no_of_buckets = 0;
  memset(c_buckets, 0, sizeof(c_buckets));
  for(Uint32 i = 0; i<NO_OF_BUCKETS; i++)
  {
    Bucket* bucket= c_buckets+i;
    bucket->m_buffer_tail = RNIL;
    bucket->m_buffer_head.m_page_id = RNIL;
    bucket->m_buffer_head.m_page_pos = Buffer_page::DATA_WORDS;
  }
  
  m_max_seen_gci = 0;      // FIRE_TRIG_ORD
  m_max_sent_gci = 0;      // FIRE_TRIG_ORD -> send
  m_last_complete_gci = 0; // SUB_GCP_COMPLETE_REP
  m_gcp_complete_rep_count = 0;
  m_out_of_buffer_gci = 0;
 
  c_startup.m_wait_handover= false; 
  c_failedApiNodes.clear();

  ReadConfigConf * conf = (ReadConfigConf*)signal->getDataPtrSend();
  conf->senderRef = reference();
  conf->senderData = senderData;
  sendSignal(ref, GSN_READ_CONFIG_CONF, signal, 
	     ReadConfigConf::SignalLength, JBB);
}

void
Suma::execSTTOR(Signal* signal) {
  jamEntry();                            

  DBUG_ENTER("Suma::execSTTOR");
  const Uint32 startphase  = signal->theData[1];
  const Uint32 typeOfStart = signal->theData[7];

  DBUG_PRINT("info",("startphase = %u, typeOfStart = %u",
		     startphase, typeOfStart));

  if(startphase == 3)
  {
    jam();
    ndbrequire((m_tup = (Dbtup*)globalData.getBlock(DBTUP)) != 0);
    signal->theData[0] = reference();
    sendSignal(NDBCNTR_REF, GSN_READ_NODESREQ, signal, 1, JBB);
    DBUG_VOID_RETURN;
  }

  if(startphase == 5)
  {
    if (ERROR_INSERTED(13029)) /* Hold startphase 5 */
    {
      sendSignalWithDelay(SUMA_REF, GSN_STTOR, signal,
                          30, signal->getLength());
      DBUG_VOID_RETURN;
    }

    c_startup.m_restart_server_node_id = 0;    
    getNodeGroupMembers(signal);
    if (typeOfStart == NodeState::ST_NODE_RESTART ||
	typeOfStart == NodeState::ST_INITIAL_NODE_RESTART)
    {
      jam();
      
      send_start_me_req(signal);
      return;
    }
  }
  
  if(startphase == 7)
  {
    if (typeOfStart != NodeState::ST_NODE_RESTART &&
	typeOfStart != NodeState::ST_INITIAL_NODE_RESTART)
    {
      for( Uint32 i = 0; i < c_no_of_buckets; i++)
      {
	if (get_responsible_node(i) == getOwnNodeId())
	{
	  // I'm running this bucket
	  DBUG_PRINT("info",("bucket %u set to true", i));
	  m_active_buckets.set(i);
	  ndbout_c("m_active_buckets.set(%d)", i);
	}
      }
    }
    
    if(!m_active_buckets.isclear())
    {
      NdbNodeBitmask tmp;
      Uint32 bucket = 0;
      while ((bucket = m_active_buckets.find(bucket)) != Bucket_mask::NotFound)
      {
	tmp.set(get_responsible_node(bucket, c_nodes_in_nodegroup_mask));
	bucket++;
      }
      
      ndbassert(tmp.get(getOwnNodeId()));
      m_gcp_complete_rep_count = tmp.count();// I contribute 1 gcp complete rep
    }
    else
      m_gcp_complete_rep_count = 0; // I contribute 1 gcp complete rep
    
    if(typeOfStart == NodeState::ST_INITIAL_START &&
       c_masterNodeId == getOwnNodeId())
    {
      jam();
      createSequence(signal);
      DBUG_VOID_RETURN;
    }//if
    
    if (ERROR_INSERTED(13030))
    {
      ndbout_c("Dont start handover");
      return;
    }
  }//if
  
  if(startphase == 100)
  {
    /**
     * Allow API's to connect
     */
    sendSTTORRY(signal);
    return;
  }

  if(startphase == 101)
  {
    if (typeOfStart == NodeState::ST_NODE_RESTART ||
	typeOfStart == NodeState::ST_INITIAL_NODE_RESTART)
    {
      /**
       * Handover code here
       */
      c_startup.m_wait_handover= true;
      check_start_handover(signal);
      return;
    }
  }
  sendSTTORRY(signal);
  
  DBUG_VOID_RETURN;
}

void
Suma::send_start_me_req(Signal* signal)
{
  Uint32 nodeId= c_startup.m_restart_server_node_id;
  do {
    nodeId = c_alive_nodes.find(nodeId + 1);
    
    if(nodeId == getOwnNodeId())
      continue;
    if(nodeId == NdbNodeBitmask::NotFound)
    {
      nodeId = 0;
      continue;
    }
    break;
  } while(true);
  

  infoEvent("Suma: asking node %d to recreate subscriptions on me", nodeId);
  c_startup.m_restart_server_node_id= nodeId;
  sendSignal(calcSumaBlockRef(nodeId), 
	     GSN_SUMA_START_ME_REQ, signal, 1, JBB);
}

void
Suma::execSUMA_START_ME_REF(Signal* signal)
{
  const SumaStartMeRef* ref= (SumaStartMeRef*)signal->getDataPtr();
  ndbrequire(ref->errorCode == SumaStartMeRef::Busy);

  infoEvent("Suma: node %d refused %d", 
	    c_startup.m_restart_server_node_id, ref->errorCode);

  c_startup.m_restart_server_node_id++;
  send_start_me_req(signal);
}

void
Suma::execSUMA_START_ME_CONF(Signal* signal)
{
  infoEvent("Suma: node %d has completed restoring me", 
	    c_startup.m_restart_server_node_id);
  sendSTTORRY(signal);  
  c_startup.m_restart_server_node_id= 0;
}

void
Suma::createSequence(Signal* signal)
{
  jam();
  DBUG_ENTER("Suma::createSequence");

  UtilSequenceReq * req = (UtilSequenceReq*)signal->getDataPtrSend();
  
  req->senderData  = RNIL;
  req->sequenceId  = SUMA_SEQUENCE;
  req->requestType = UtilSequenceReq::Create;
  sendSignal(DBUTIL_REF, GSN_UTIL_SEQUENCE_REQ, 
	     signal, UtilSequenceReq::SignalLength, JBB);
  // execUTIL_SEQUENCE_CONF will call createSequenceReply()
  DBUG_VOID_RETURN;
}

void
Suma::createSequenceReply(Signal* signal,
			  UtilSequenceConf * conf,
			  UtilSequenceRef * ref)
{
  jam();

  if (ref != NULL)
  {
    switch ((UtilSequenceRef::ErrorCode)ref->errorCode)
    {
      case UtilSequenceRef::NoSuchSequence:
        ndbrequire(false);
      case UtilSequenceRef::TCError:
      {
        char buf[128];
        snprintf(buf, sizeof(buf),
                 "Startup failed during sequence creation. TC error %d",
                 ref->TCErrorCode);
        progError(__LINE__, NDBD_EXIT_RESOURCE_ALLOC_ERROR, buf);
      }
    }
    ndbrequire(false);
  }

  sendSTTORRY(signal);
}

void
Suma::execREAD_NODESCONF(Signal* signal){
  jamEntry();
  ReadNodesConf * const conf = (ReadNodesConf *)signal->getDataPtr();
 
  if(getNodeState().getNodeRestartInProgress())
  {
    c_alive_nodes.assign(NdbNodeBitmask::Size, conf->startedNodes);
    c_alive_nodes.set(getOwnNodeId()); 
  }
  else
  {
    c_alive_nodes.assign(NdbNodeBitmask::Size, conf->startingNodes);
    NdbNodeBitmask tmp;
    tmp.assign(NdbNodeBitmask::Size, conf->startedNodes);
    ndbrequire(tmp.isclear()); // No nodes can be started during SR
  }
  
  c_masterNodeId = conf->masterNodeId;
  
  sendSTTORRY(signal);
}

void
Suma::execAPI_START_REP(Signal* signal)
{
  Uint32 nodeId = signal->theData[0];
  c_connected_nodes.set(nodeId);
  
  check_start_handover(signal);
}

void
Suma::check_start_handover(Signal* signal)
{
  if(c_startup.m_wait_handover)
  {
    NodeBitmask tmp;
    tmp.assign(c_connected_nodes);
    tmp.bitAND(c_subscriber_nodes);
    if(!c_subscriber_nodes.equal(tmp))
    {
      return;
    }
    
    c_startup.m_wait_handover= false;
    send_handover_req(signal);
  }
}

void
Suma::send_handover_req(Signal* signal)
{
  c_startup.m_handover_nodes.assign(c_alive_nodes);
  c_startup.m_handover_nodes.bitAND(c_nodes_in_nodegroup_mask);
  c_startup.m_handover_nodes.clear(getOwnNodeId());
  Uint32 gci= (m_last_complete_gci >> 32) + 3;
  
  SumaHandoverReq* req= (SumaHandoverReq*)signal->getDataPtrSend();
  char buf[255];
  c_startup.m_handover_nodes.getText(buf);
  infoEvent("Suma: initiate handover with nodes %s GCI: %d",
	    buf, gci);

  req->gci = gci;
  req->nodeId = getOwnNodeId();
  
  NodeReceiverGroup rg(SUMA, c_startup.m_handover_nodes);
  sendSignal(rg, GSN_SUMA_HANDOVER_REQ, signal, 
	     SumaHandoverReq::SignalLength, JBB);
}

void
Suma::sendSTTORRY(Signal* signal){
  signal->theData[0] = 0;
  signal->theData[3] = 1;
  signal->theData[4] = 3;
  signal->theData[5] = 5;
  signal->theData[6] = 7;
  signal->theData[7] = 100;
  signal->theData[8] = 101;
  signal->theData[9] = 255; // No more start phases from missra
  sendSignal(NDBCNTR_REF, GSN_STTORRY, signal, 10, JBB);
}

void
Suma::execNDB_STTOR(Signal* signal) 
{
  jamEntry();                            
}

void
Suma::execCONTINUEB(Signal* signal){
  jamEntry();
  Uint32 type= signal->theData[0];
  switch(type){
  case SumaContinueB::RELEASE_GCI:
  {
    Uint32 gci_hi = signal->theData[2];
    Uint32 gci_lo = signal->theData[3];
    Uint64 gci = gci_lo | (Uint64(gci_hi) << 32);
    release_gci(signal, signal->theData[1], gci);
    return;
  }
  case SumaContinueB::RESEND_BUCKET:
  {
    Uint32 min_gci_hi = signal->theData[2];
    Uint32 min_gci_lo = signal->theData[5];
    Uint32 last_gci_hi = signal->theData[4];
    Uint32 last_gci_lo = signal->theData[6];
    Uint64 min_gci = min_gci_lo | (Uint64(min_gci_hi) << 32);
    Uint64 last_gci = last_gci_lo | (Uint64(last_gci_hi) << 32);
    resend_bucket(signal, 
		  signal->theData[1], 
		  min_gci,
		  signal->theData[3],
		  last_gci);
    return;
  }
  case SumaContinueB::OUT_OF_BUFFER_RELEASE:
    out_of_buffer_release(signal, signal->theData[1]);
    return;
  }
}

/*****************************************************************************
 * 
 * Node state handling
 *
 *****************************************************************************/

void Suma::execAPI_FAILREQ(Signal* signal) 
{
  jamEntry();
  DBUG_ENTER("Suma::execAPI_FAILREQ");
  Uint32 failedApiNode = signal->theData[0];
  //BlockReference retRef = signal->theData[1];

  if (c_startup.m_restart_server_node_id &&
      c_startup.m_restart_server_node_id != RNIL)
  {
    jam();
    sendSignalWithDelay(reference(), GSN_API_FAILREQ, signal,
                        200, signal->getLength());
    return;
  }

  if (c_failedApiNodes.get(failedApiNode))
  {
    jam();
    return;
  }

  if (!c_subscriber_nodes.get(failedApiNode))
  {
    jam();
    return;
  }

  c_failedApiNodes.set(failedApiNode);
  c_connected_nodes.clear(failedApiNode);
  bool found = removeSubscribersOnNode(signal, failedApiNode);

  if(!found){
    jam();
    c_failedApiNodes.clear(failedApiNode);
  }

  SubGcpCompleteAck * const ack = (SubGcpCompleteAck*)signal->getDataPtr();
  Ptr<Gcp_record> gcp;
  for(c_gcp_list.first(gcp); !gcp.isNull(); c_gcp_list.next(gcp))
  {
    jam();
    ack->rep.gci_hi = gcp.p->m_gci >> 32;
    ack->rep.gci_lo = gcp.p->m_gci & 0xFFFFFFFF;
    if(gcp.p->m_subscribers.get(failedApiNode))
    {
      jam();
      gcp.p->m_subscribers.clear(failedApiNode);
      ack->rep.senderRef = numberToRef(0, failedApiNode);
      sendSignal(SUMA_REF, GSN_SUB_GCP_COMPLETE_ACK, signal, 
		 SubGcpCompleteAck::SignalLength, JBB);
    }
  }

  c_subscriber_nodes.clear(failedApiNode);
  
  check_start_handover(signal);

  DBUG_VOID_RETURN;
}//execAPI_FAILREQ()

bool
Suma::removeSubscribersOnNode(Signal *signal, Uint32 nodeId)
{
  DBUG_ENTER("Suma::removeSubscribersOnNode");
  bool found = false;

  KeyTable<Table>::Iterator it;
  LINT_INIT(it.bucket);
  LINT_INIT(it.curr.p);
  for(c_tables.first(it);!it.isNull();c_tables.next(it))
  {
    LocalDLList<Subscriber> subbs(c_subscriberPool,it.curr.p->c_subscribers);
    SubscriberPtr i_subbPtr;
    for(subbs.first(i_subbPtr);!i_subbPtr.isNull();)
    {
      SubscriberPtr subbPtr = i_subbPtr;
      subbs.next(i_subbPtr);
      jam();
      if (refToNode(subbPtr.p->m_senderRef) == nodeId) {
	jam();
	subbs.remove(subbPtr);
	c_removeDataSubscribers.add(subbPtr);
	found = true;
      }
    }
    if (subbs.isEmpty())
    {
      // ToDo handle this
    }
  }
  if(found){
    jam();
    sendSubStopReq(signal);
  }
  DBUG_RETURN(found);
}

void
Suma::sendSubStopReq(Signal *signal, bool unlock){
  static bool remove_lock = false;
  jam();
  DBUG_ENTER("Suma::sendSubStopReq");

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
    DBUG_VOID_RETURN;
  }

  if(remove_lock && !unlock) {
    jam();
    DBUG_VOID_RETURN;
  }
  remove_lock = true;

  SubscriptionPtr subPtr;
  c_subscriptions.getPtr(subPtr, subbPtr.p->m_subPtrI);

  SubStopReq * const req = (SubStopReq*)signal->getDataPtrSend();
  req->senderRef       = reference();
  req->senderData      = subbPtr.i;
  req->subscriberRef   = subbPtr.p->m_senderRef;
  req->subscriberData  = subbPtr.p->m_senderData;
  req->subscriptionId  = subPtr.p->m_subscriptionId;
  req->subscriptionKey = subPtr.p->m_subscriptionKey;
  req->part = SubscriptionData::TableData;

  sendSignal(SUMA_REF,GSN_SUB_STOP_REQ,signal,SubStopReq::SignalLength,JBB);
  DBUG_VOID_RETURN;
}

void
Suma::execSUB_STOP_CONF(Signal* signal){
  jamEntry();
  DBUG_ENTER("Suma::execSUB_STOP_CONF");
  ndbassert(signal->getNoOfSections() == 0);
  sendSubStopReq(signal,true);
  DBUG_VOID_RETURN;
}

void
Suma::execSUB_STOP_REF(Signal* signal){
  jamEntry();
  DBUG_ENTER("Suma::execSUB_STOP_REF");
  ndbassert(signal->getNoOfSections() == 0);

  SubStopRef * const ref = (SubStopRef*)signal->getDataPtr();

  Uint32 senderData      = ref->senderData;
  Uint32 subscriptionId  = ref->subscriptionId;
  Uint32 subscriptionKey = ref->subscriptionKey;
  Uint32 part            = ref->part;
  Uint32 subscriberData  = ref->subscriberData;
  Uint32 subscriberRef   = ref->subscriberRef;

  if(ref->errorCode != 1411){
    ndbrequire(false);
  }

  SubStopReq * const req = (SubStopReq*)signal->getDataPtrSend();
  req->senderRef       = reference();
  req->senderData      = senderData;
  req->subscriberRef   = subscriberRef;
  req->subscriberData  = subscriberData;
  req->subscriptionId  = subscriptionId;
  req->subscriptionKey = subscriptionKey;
  req->part = part;

  sendSignal(SUMA_REF,GSN_SUB_STOP_REQ,signal,SubStopReq::SignalLength,JBB);

  DBUG_VOID_RETURN;
}

void
Suma::execNODE_FAILREP(Signal* signal){
  jamEntry();
  DBUG_ENTER("Suma::execNODE_FAILREP");
  ndbassert(signal->getNoOfSections() == 0);

  const NodeFailRep * rep = (NodeFailRep*)signal->getDataPtr();
  NdbNodeBitmask failed; failed.assign(NdbNodeBitmask::Size, rep->theNodes);
  
  if(failed.get(Restart.nodeId))
  {
    Restart.resetRestart(signal);
  }

  if (ERROR_INSERTED(13032))
  {
    Uint32 node = c_subscriber_nodes.find(0);
    if (node != NodeBitmask::NotFound)
    {
      ndbout_c("Inserting API_FAILREQ node: %u", node);
      signal->theData[0] = node;
      EXECUTE_DIRECT(QMGR, GSN_API_FAILREQ, signal, 1);
    }
  }
  
  NdbNodeBitmask tmp;
  tmp.assign(c_alive_nodes);
  tmp.bitANDC(failed);

  NdbNodeBitmask takeover_nodes;

  if(c_nodes_in_nodegroup_mask.overlaps(failed))
  {
    for( Uint32 i = 0; i < c_no_of_buckets; i++) 
    {
      if(m_active_buckets.get(i))
	continue;
      else if(m_switchover_buckets.get(i))
      {
	Uint32 state= c_buckets[i].m_state;
	if((state & Bucket::BUCKET_HANDOVER) && 
	   failed.get(get_responsible_node(i)))
	{
	  m_active_buckets.set(i);
	  m_switchover_buckets.clear(i);
	  ndbout_c("aborting handover");
	} 
	else if(state & Bucket::BUCKET_STARTING)
	{
	  progError(__LINE__, NDBD_EXIT_SYSTEM_ERROR, 
		    "Nodefailure during SUMA takeover");
	}
      }
      else if(get_responsible_node(i, tmp) == getOwnNodeId())
      {
	start_resend(signal, i);
      }
    }
  }
  
  c_alive_nodes.assign(tmp);
  
  DBUG_VOID_RETURN;
}

void
Suma::execINCL_NODEREQ(Signal* signal){
  jamEntry();
  
  const Uint32 senderRef = signal->theData[0];
  const Uint32 nodeId  = signal->theData[1];

  ndbrequire(!c_alive_nodes.get(nodeId));
  c_alive_nodes.set(nodeId);
  
  signal->theData[0] = reference();
  sendSignal(senderRef, GSN_INCL_NODECONF, signal, 1, JBB);
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

static unsigned
count_subscribers(const DLList<Suma::Subscriber> &subs)
{
  unsigned n= 0;
  Suma::SubscriberPtr i_subbPtr;
  subs.first(i_subbPtr);
  while(!i_subbPtr.isNull()){
    n++;
    subs.next(i_subbPtr);
  }
  return n;
}

void
Suma::execDUMP_STATE_ORD(Signal* signal){
  jamEntry();

  Uint32 tCase = signal->theData[0];
#if 0
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
#endif
  if(tCase == 8004){
    infoEvent("Suma: c_subscriberPool  size: %d free: %d",
	      c_subscriberPool.getSize(),
	      c_subscriberPool.getNoOfFree());

    infoEvent("Suma: c_tablePool  size: %d free: %d",
	      c_tablePool.getSize(),
	      c_tablePool.getNoOfFree());

    infoEvent("Suma: c_subscriptionPool  size: %d free: %d",
	      c_subscriptionPool.getSize(),
	      c_subscriptionPool.getNoOfFree());

    infoEvent("Suma: c_syncPool  size: %d free: %d",
	      c_syncPool.getSize(),
	      c_syncPool.getNoOfFree());

    infoEvent("Suma: c_dataBufferPool  size: %d free: %d",
	      c_dataBufferPool.getSize(),
	      c_dataBufferPool.getNoOfFree());

    infoEvent("Suma: c_metaSubscribers count: %d",
	      count_subscribers(c_metaSubscribers));
#if 0
    infoEvent("Suma: c_dataSubscribers count: %d",
	      count_subscribers(c_dataSubscribers));
    infoEvent("Suma: c_prepDataSubscribers count: %d",
	      count_subscribers(c_prepDataSubscribers));
#endif
    infoEvent("Suma: c_removeDataSubscribers count: %d",
	      count_subscribers(c_removeDataSubscribers));
  }

  if(tCase == 8005)
  {
    for(Uint32 i = 0; i<c_no_of_buckets; i++)
    {
      Bucket* ptr= c_buckets + i;
      infoEvent("Bucket %d %d%d-%x switch gci: %d max_acked_gci: %d max_gci: %d tail: %d head: %d",
		i, 
		m_active_buckets.get(i),
		m_switchover_buckets.get(i),
		ptr->m_state,
		ptr->m_switchover_gci,
		ptr->m_max_acked_gci,
		ptr->m_buffer_head.m_max_gci,
		ptr->m_buffer_tail,
		ptr->m_buffer_head.m_page_id);
    }
  }  

  if (tCase == 8006)
  {
    SET_ERROR_INSERT_VALUE(13029);
  }

  if (tCase == 8007)
  {
    c_startup.m_restart_server_node_id = MAX_NDB_NODES + 1;
    SET_ERROR_INSERT_VALUE(13029);
  }

  if (tCase == 8008)
  {
    CLEAR_ERROR_INSERT_VALUE;
  }

  if (tCase == 8010)
  {
    char buf1[255], buf2[255];
    c_subscriber_nodes.getText(buf1);
    c_connected_nodes.getText(buf2);
    infoEvent("c_subscriber_nodes: %s", buf1);
    infoEvent("c_connected_nodes: %s", buf2);
  }

  if (tCase == 8009)
  {
    if (ERROR_INSERTED(13030))
    {
      CLEAR_ERROR_INSERT_VALUE;
      sendSTTORRY(signal);
    }
    else
    {
      SET_ERROR_INSERT_VALUE(13030);
    }
    return;
  }
}

/*************************************************************
 *
 * Creation of subscription id's
 *
 ************************************************************/

void 
Suma::execCREATE_SUBID_REQ(Signal* signal) 
{
  jamEntry();
  DBUG_ENTER("Suma::execCREATE_SUBID_REQ");
  ndbassert(signal->getNoOfSections() == 0);
  CRASH_INSERTION(13001);

  CreateSubscriptionIdReq const * req =
    (CreateSubscriptionIdReq*)signal->getDataPtr();
  SubscriberPtr subbPtr;
  if(!c_subscriberPool.seize(subbPtr)){
    jam();
    sendSubIdRef(signal, req->senderRef, req->senderData, 1412);
    DBUG_VOID_RETURN;
  }
  DBUG_PRINT("info",("c_subscriberPool  size: %d free: %d",
		     c_subscriberPool.getSize(),
		     c_subscriberPool.getNoOfFree()));

  subbPtr.p->m_senderRef  = req->senderRef; 
  subbPtr.p->m_senderData = req->senderData;

  UtilSequenceReq * utilReq = (UtilSequenceReq*)signal->getDataPtrSend();
  utilReq->senderData  = subbPtr.i;
  utilReq->sequenceId  = SUMA_SEQUENCE;
  utilReq->requestType = UtilSequenceReq::NextVal;
  sendSignal(DBUTIL_REF, GSN_UTIL_SEQUENCE_REQ, 
	     signal, UtilSequenceReq::SignalLength, JBB);

  DBUG_VOID_RETURN;
}

void
Suma::execUTIL_SEQUENCE_CONF(Signal* signal)
{
  jamEntry();
  DBUG_ENTER("Suma::execUTIL_SEQUENCE_CONF");
  ndbassert(signal->getNoOfSections() == 0);
  CRASH_INSERTION(13002);

  UtilSequenceConf * conf = (UtilSequenceConf*)signal->getDataPtr();
  if(conf->requestType == UtilSequenceReq::Create) {
    jam();
    createSequenceReply(signal, conf, NULL);
    DBUG_VOID_RETURN;
  }

  Uint64 subId;
  memcpy(&subId,conf->sequenceValue,8);
  SubscriberPtr subbPtr;
  c_subscriberPool.getPtr(subbPtr,conf->senderData);

  CreateSubscriptionIdConf * subconf = (CreateSubscriptionIdConf*)conf;
  subconf->senderRef      = reference();
  subconf->senderData     = subbPtr.p->m_senderData;
  subconf->subscriptionId = (Uint32)subId;
  subconf->subscriptionKey =(getOwnNodeId() << 16) | (Uint32)(subId & 0xFFFF);
  
  sendSignal(subbPtr.p->m_senderRef, GSN_CREATE_SUBID_CONF, signal,
	     CreateSubscriptionIdConf::SignalLength, JBB);

  c_subscriberPool.release(subbPtr);
  DBUG_PRINT("info",("c_subscriberPool  size: %d free: %d",
		     c_subscriberPool.getSize(),
		     c_subscriberPool.getNoOfFree()));
  DBUG_VOID_RETURN;
}

void
Suma::execUTIL_SEQUENCE_REF(Signal* signal)
{
  jamEntry();
  DBUG_ENTER("Suma::execUTIL_SEQUENCE_REF");
  ndbassert(signal->getNoOfSections() == 0);
  UtilSequenceRef * ref = (UtilSequenceRef*)signal->getDataPtr();
  Uint32 err= ref->errorCode;

  if(ref->requestType == UtilSequenceReq::Create) {
    jam();
    createSequenceReply(signal, NULL, ref);
    DBUG_VOID_RETURN;
  }

  Uint32 subData = ref->senderData;

  SubscriberPtr subbPtr;
  c_subscriberPool.getPtr(subbPtr,subData);
  sendSubIdRef(signal, subbPtr.p->m_senderRef, subbPtr.p->m_senderData, err);
  c_subscriberPool.release(subbPtr);
  DBUG_PRINT("info",("c_subscriberPool  size: %d free: %d",
		     c_subscriberPool.getSize(),
		     c_subscriberPool.getNoOfFree()));
  DBUG_VOID_RETURN;
}//execUTIL_SEQUENCE_REF()


void
Suma::sendSubIdRef(Signal* signal,
			      Uint32 senderRef, Uint32 senderData, Uint32 errCode)
{
  jam();
  DBUG_ENTER("Suma::sendSubIdRef");
  CreateSubscriptionIdRef  * ref = 
    (CreateSubscriptionIdRef *)signal->getDataPtrSend();

  ref->senderRef  = reference();
  ref->senderData = senderData;
  ref->errorCode  = errCode;
  sendSignal(senderRef, 
	     GSN_CREATE_SUBID_REF,
	     signal, 
	     CreateSubscriptionIdRef::SignalLength,
	     JBB);
  
  releaseSections(signal);
  DBUG_VOID_RETURN;
}

/**********************************************************
 * Suma participant interface
 *
 * Creation of subscriptions
 */

void 
Suma::addTableId(Uint32 tableId,
			    SubscriptionPtr subPtr, SyncRecord *psyncRec)
{
  DBUG_ENTER("Suma::addTableId");
  DBUG_PRINT("enter",("tableId: %u subPtr.i: %u", tableId, subPtr.i));
  subPtr.p->m_tableId= tableId;
  if(psyncRec != NULL)
    psyncRec->m_tableList.append(&tableId, 1);
  DBUG_VOID_RETURN;
}

void
Suma::execSUB_CREATE_REQ(Signal* signal)
{
  jamEntry();                            
  DBUG_ENTER("Suma::execSUB_CREATE_REQ");
  ndbassert(signal->getNoOfSections() == 0);
  CRASH_INSERTION(13003);

  const SubCreateReq req = *(SubCreateReq*)signal->getDataPtr();    
  
  const Uint32 subRef  = req.senderRef;
  const Uint32 subData = req.senderData;
  const Uint32 subId   = req.subscriptionId;
  const Uint32 subKey  = req.subscriptionKey;
  const Uint32 type    = req.subscriptionType & SubCreateReq::RemoveFlags;
  const Uint32 flags   = req.subscriptionType & SubCreateReq::GetFlags;
  const bool addTableFlag = (flags & SubCreateReq::AddTableFlag) != 0;
  const bool restartFlag  = (flags & SubCreateReq::RestartFlag)  != 0;
  const Uint32 reportAll = (flags & SubCreateReq::ReportAll) ?
    Subscription::REPORT_ALL : 0;
  const Uint32 reportSubscribe = (flags & SubCreateReq::ReportSubscribe) ?
    Subscription::REPORT_SUBSCRIBE : 0;
  const Uint32 tableId = req.tableId;
  Subscription::State state = (Subscription::State) req.state;
  if (signal->getLength() != SubCreateReq::SignalLength2)
  {
    /*
      api or restarted by older version
      if restarted by old version, do the best we can
    */
    state = Subscription::DEFINED;
  }

  Subscription key;
  key.m_subscriptionId  = subId;
  key.m_subscriptionKey = subKey;

  DBUG_PRINT("enter",("key.m_subscriptionId: %u, key.m_subscriptionKey: %u",
		      key.m_subscriptionId, key.m_subscriptionKey));

  SubscriptionPtr subPtr;

  if (addTableFlag) {
    ndbrequire(restartFlag);  //TODO remove this

    if(!c_subscriptions.find(subPtr, key)) {
      jam();
      sendSubCreateRef(signal, 1407);
      DBUG_VOID_RETURN;
    }
    jam();
    if (restartFlag)
    {
      ndbrequire(type != SubCreateReq::SingleTableScan);
      ndbrequire(req.tableId != subPtr.p->m_tableId);
      ndbrequire(type != SubCreateReq::TableEvent);
      addTableId(req.tableId, subPtr, 0);
    }
  } else {
    if (c_startup.m_restart_server_node_id && 
        subRef != calcSumaBlockRef(c_startup.m_restart_server_node_id))
    {
      /**
       * only allow "restart_server" Suma's to come through 
       * for restart purposes
       */
      jam();
      sendSubCreateRef(signal, 1415);
      DBUG_VOID_RETURN;
    }
    // Check that id/key is unique
    if(c_subscriptions.find(subPtr, key)) {
      jam();
      sendSubCreateRef(signal, 1415);
      DBUG_VOID_RETURN;
    }
    if(!c_subscriptions.seize(subPtr)) {
      jam();
      sendSubCreateRef(signal, 1412);
      DBUG_VOID_RETURN;
    }
    DBUG_PRINT("info",("c_subscriptionPool  size: %d free: %d",
		       c_subscriptionPool.getSize(),
		       c_subscriptionPool.getNoOfFree()));
    jam();
    subPtr.p->m_senderRef        = subRef;
    subPtr.p->m_senderData       = subData;
    subPtr.p->m_subscriptionId   = subId;
    subPtr.p->m_subscriptionKey  = subKey;
    subPtr.p->m_subscriptionType = type;
    subPtr.p->m_options          = reportSubscribe | reportAll;
    subPtr.p->m_tableId          = tableId;
    subPtr.p->m_table_ptrI       = RNIL;
    subPtr.p->m_state            = state;
    subPtr.p->n_subscribers      = 0;
    subPtr.p->m_current_sync_ptrI = RNIL;

    fprintf(stderr, "table %d options %x\n", subPtr.p->m_tableId, subPtr.p->m_options);
    DBUG_PRINT("info",("Added: key.m_subscriptionId: %u, key.m_subscriptionKey: %u",
		       key.m_subscriptionId, key.m_subscriptionKey));

    c_subscriptions.add(subPtr);
  }

  SubCreateConf * const conf = (SubCreateConf*)signal->getDataPtrSend();
  conf->senderRef  = reference();
  conf->senderData = subPtr.p->m_senderData;
  sendSignal(subRef, GSN_SUB_CREATE_CONF, signal, SubCreateConf::SignalLength, JBB);
  DBUG_VOID_RETURN;
}

void
Suma::sendSubCreateRef(Signal* signal, Uint32 errCode)
{
  jam();
  SubCreateRef * ref = (SubCreateRef *)signal->getDataPtrSend();
  ref->errorCode  = errCode;
  sendSignal(signal->getSendersBlockRef(), GSN_SUB_CREATE_REF, signal, 
	     SubCreateRef::SignalLength, JBB);
  return;
}

/**********************************************************
 *
 * Setting upp trigger for subscription
 *
 */

void 
Suma::execSUB_SYNC_REQ(Signal* signal)
{
  jamEntry();
  DBUG_ENTER("Suma::execSUB_SYNC_REQ");
  ndbassert(signal->getNoOfSections() <= 1);
  CRASH_INSERTION(13004);

  SubSyncReq * const req = (SubSyncReq*)signal->getDataPtr();

  SubscriptionPtr subPtr;
  Subscription key; 
  key.m_subscriptionId = req->subscriptionId;
  key.m_subscriptionKey = req->subscriptionKey;

  DBUG_PRINT("enter",("key.m_subscriptionId: %u, key.m_subscriptionKey: %u",
		      key.m_subscriptionId, key.m_subscriptionKey));

  if(!c_subscriptions.find(subPtr, key))
  {
    jam();
    DBUG_PRINT("info",("Not found"));
    sendSubSyncRef(signal, 1407);
    DBUG_VOID_RETURN;
  }

  bool ok = false;
  SubscriptionData::Part part = (SubscriptionData::Part)req->part;
  
  Ptr<SyncRecord> syncPtr;
  if(!c_syncPool.seize(syncPtr))
  {
    jam();
    sendSubSyncRef(signal, 1416);
    DBUG_VOID_RETURN;
  }
  DBUG_PRINT("info",("c_syncPool  size: %d free: %d",
		     c_syncPool.getSize(),
		     c_syncPool.getNoOfFree()));

  syncPtr.p->m_senderRef        = req->senderRef;
  syncPtr.p->m_senderData       = req->senderData;
  syncPtr.p->m_subscriptionPtrI = subPtr.i;
  syncPtr.p->ptrI               = syncPtr.i;
  syncPtr.p->m_error            = 0;

  subPtr.p->m_current_sync_ptrI = syncPtr.i;

  {
    jam();
    syncPtr.p->m_tableList.append(&subPtr.p->m_tableId, 1);
    if(signal->getNoOfSections() > 0){
      SegmentedSectionPtr ptr(0,0,0);
      signal->getSection(ptr, SubSyncReq::ATTRIBUTE_LIST);
      LocalDataBuffer<15> attrBuf(c_dataBufferPool,syncPtr.p->m_attributeList);
      append(attrBuf, ptr, getSectionSegmentPool());
      releaseSections(signal);
    }
  }

  TablePtr tabPtr;
  initTable(signal,subPtr.p->m_tableId,tabPtr,syncPtr);
  tabPtr.p->n_subscribers++;
  if (subPtr.p->m_options & Subscription::REPORT_ALL)
    tabPtr.p->m_reportAll = true;
  DBUG_PRINT("info",("Suma::Table[%u]::n_subscribers: %u",
		     tabPtr.p->m_tableId, tabPtr.p->n_subscribers));
  DBUG_VOID_RETURN;

  switch(part){
  case SubscriptionData::MetaData:
    ndbrequire(false);
#if 0
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
#endif
    break;
  case SubscriptionData::TableData: {
    ok = true;
    jam();
    syncPtr.p->startScan(signal);
    break;
  }
  }
  ndbrequire(ok);
  DBUG_VOID_RETURN;
}

void
Suma::sendSubSyncRef(Signal* signal, Uint32 errCode){
  jam();
  SubSyncRef * ref= (SubSyncRef *)signal->getDataPtrSend();
  ref->errorCode = errCode;
  releaseSections(signal);
  sendSignal(signal->getSendersBlockRef(), 
	     GSN_SUB_SYNC_REF, 
	     signal, 
	     SubSyncRef::SignalLength,
	     JBB);
  return;
}

/**********************************************************
 * Dict interface
 */

#if 0
void
Suma::execLIST_TABLES_CONF(Signal* signal){
  jamEntry();
  CRASH_INSERTION(13005);
  ListTablesConf* const conf = (ListTablesConf*)signal->getDataPtr();
  SyncRecord* tmp = c_syncPool.getPtr(conf->senderData);
  tmp->runLIST_TABLES_CONF(signal);
}
#endif


/*************************************************************************
 *
 *
 */
#if 0
void
Suma::Table::runLIST_TABLES_CONF(Signal* signal){
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
#endif


int 
Suma::initTable(Signal *signal, Uint32 tableId, TablePtr &tabPtr,
			   SubscriberPtr subbPtr)
{
  DBUG_ENTER("Suma::initTable SubscriberPtr");
  DBUG_PRINT("enter",("tableId: %d", tableId));

  int r= initTable(signal,tableId,tabPtr);

  {
    LocalDLList<Subscriber> subscribers(c_subscriberPool,
					tabPtr.p->c_subscribers);
    subscribers.add(subbPtr);
  }

  DBUG_PRINT("info",("added subscriber: %i", subbPtr.i));
  
  if (r)
  {
    jam();
    // we have to wait getting tab info
    DBUG_RETURN(1);
  }

  if (tabPtr.p->setupTrigger(signal, *this))
  {
    jam();
    // we have to wait for triggers to be setup
    DBUG_RETURN(1);
  }

  int ret = completeOneSubscriber(signal, tabPtr, subbPtr);
  if (ret == -1)
  {
    jam();
    LocalDLList<Subscriber> subscribers(c_subscriberPool,
					tabPtr.p->c_subscribers);
    subscribers.release(subbPtr);
  }
  completeInitTable(signal, tabPtr);
  DBUG_RETURN(0);
}

int 
Suma::initTable(Signal *signal, Uint32 tableId, TablePtr &tabPtr,
			   Ptr<SyncRecord> syncPtr)
{
  jam();
  DBUG_ENTER("Suma::initTable Ptr<SyncRecord>");
  DBUG_PRINT("enter",("tableId: %d", tableId));

  int r= initTable(signal,tableId,tabPtr);

  {
    LocalDLList<SyncRecord> syncRecords(c_syncPool,tabPtr.p->c_syncRecords);
    syncRecords.add(syncPtr);
  }

  if (r)
  {
    // we have to wait getting tab info
    DBUG_RETURN(1);
  }
  completeInitTable(signal, tabPtr);
  DBUG_RETURN(0);
}

int
Suma::initTable(Signal *signal, Uint32 tableId, TablePtr &tabPtr)
{
  jam();
  DBUG_ENTER("Suma::initTable");

  if (!c_tables.find(tabPtr, tableId) ||
      tabPtr.p->m_state == Table::DROPPED ||
      tabPtr.p->m_state == Table::ALTERED)
  {
    // table not being prepared
    // seize a new table, initialize and add to c_tables
    ndbrequire(c_tablePool.seize(tabPtr));
    DBUG_PRINT("info",("c_tablePool  size: %d free: %d",
		       c_tablePool.getSize(),
		       c_tablePool.getNoOfFree()));
    new (tabPtr.p) Table;

    tabPtr.p->m_tableId= tableId;
    tabPtr.p->m_ptrI= tabPtr.i;
    tabPtr.p->n_subscribers = 0;
    DBUG_PRINT("info",("Suma::Table[%u,i=%u]::n_subscribers: %u",
		       tabPtr.p->m_tableId, tabPtr.i, tabPtr.p->n_subscribers));

    tabPtr.p->m_reportAll = false;

    tabPtr.p->m_error         = 0;
    tabPtr.p->m_schemaVersion = RNIL;
    tabPtr.p->m_state = Table::DEFINING;
    tabPtr.p->m_drop_subbPtr.p = 0;
    for (int j= 0; j < 3; j++)
    {
      tabPtr.p->m_hasTriggerDefined[j] = 0;
      tabPtr.p->m_hasOutstandingTriggerReq[j] = 0;
      tabPtr.p->m_triggerIds[j] = ILLEGAL_TRIGGER_ID;
    }

    c_tables.add(tabPtr);

    GetTabInfoReq * req = (GetTabInfoReq *)signal->getDataPtrSend();
    req->senderRef = reference();
    req->senderData = tabPtr.i;
    req->requestType = 
      GetTabInfoReq::RequestById | GetTabInfoReq::LongSignalConf;
    req->tableId = tableId;

    DBUG_PRINT("info",("GET_TABINFOREQ id %d", req->tableId));

    if (ERROR_INSERTED(13031))
    {
      jam();
      CLEAR_ERROR_INSERT_VALUE;
      GetTabInfoRef* ref = (GetTabInfoRef*)signal->getDataPtrSend();
      ref->tableId = tableId;
      ref->senderData = tabPtr.i;
      ref->errorCode = GetTabInfoRef::TableNotDefined;
      sendSignal(reference(), GSN_GET_TABINFOREF, signal, 
		 GetTabInfoRef::SignalLength, JBB);
      DBUG_RETURN(1);
    }

    sendSignal(DBDICT_REF, GSN_GET_TABINFOREQ, signal,
	       GetTabInfoReq::SignalLength, JBB);
    DBUG_RETURN(1);
  }
  if (tabPtr.p->m_state == Table::DEFINING)
  {
    DBUG_RETURN(1);
  }
  // ToDo should be a ref signal instead
  ndbrequire(tabPtr.p->m_state == Table::DEFINED);
  DBUG_RETURN(0);
}

int
Suma::completeOneSubscriber(Signal *signal, TablePtr tabPtr, SubscriberPtr subbPtr)
{
  jam();
  DBUG_ENTER("Suma::completeOneSubscriber");

  if (tabPtr.p->m_error &&
      (c_startup.m_restart_server_node_id == 0 ||
       tabPtr.p->m_state != Table::DROPPED))
  {
    jam();
    sendSubStartRef(signal,subbPtr,tabPtr.p->m_error,
		    SubscriptionData::TableData);
    tabPtr.p->n_subscribers--;
    DBUG_RETURN(-1);
  }
  else
  {
    jam();
    SubscriptionPtr subPtr;
    c_subscriptions.getPtr(subPtr, subbPtr.p->m_subPtrI);
    subPtr.p->m_table_ptrI= tabPtr.i;
    sendSubStartComplete(signal,subbPtr, m_last_complete_gci + 3,
			 SubscriptionData::TableData);
  }
  DBUG_RETURN(0);
}

void
Suma::completeAllSubscribers(Signal *signal, TablePtr tabPtr)
{
  jam();
  DBUG_ENTER("Suma::completeAllSubscribers");
  // handle all subscribers
  {
    LocalDLList<Subscriber> subscribers(c_subscriberPool,
					tabPtr.p->c_subscribers);
    SubscriberPtr subbPtr;
    for(subscribers.first(subbPtr); !subbPtr.isNull();)
    {
      jam();
      Ptr<Subscriber> tmp = subbPtr;
      subscribers.next(subbPtr);
      int ret = completeOneSubscriber(signal, tabPtr, tmp);
      if (ret == -1)
      {
	jam();
	subscribers.release(tmp);
      }
    }
  }
  DBUG_VOID_RETURN;
}

void
Suma::completeInitTable(Signal *signal, TablePtr tabPtr)
{
  jam();
  DBUG_ENTER("Suma::completeInitTable");

  // handle all syncRecords
  while (!tabPtr.p->c_syncRecords.isEmpty())
  {
    Ptr<SyncRecord> syncPtr;
    {
      LocalDLList<SyncRecord> syncRecords(c_syncPool,
					tabPtr.p->c_syncRecords);
      syncRecords.first(syncPtr);
      syncRecords.remove(syncPtr);
    }
    syncPtr.p->ptrI = syncPtr.i;
    if (tabPtr.p->m_error == 0)
    {
      jam();
      syncPtr.p->startScan(signal);
    }
    else
    {
      jam();
      syncPtr.p->completeScan(signal, tabPtr.p->m_error);
      tabPtr.p->n_subscribers--;
    }
  }
  
  if (tabPtr.p->m_error)
  {
    DBUG_PRINT("info",("Suma::Table[%u]::n_subscribers: %u",
		       tabPtr.p->m_tableId, tabPtr.p->n_subscribers));
    tabPtr.p->checkRelease(*this);
  }
  else
  {
    tabPtr.p->m_state = Table::DEFINED;
  }

  DBUG_VOID_RETURN;
}


void
Suma::execGET_TABINFOREF(Signal* signal){
  jamEntry();
  GetTabInfoRef* ref = (GetTabInfoRef*)signal->getDataPtr();
  Uint32 tableId = ref->tableId;
  Uint32 senderData = ref->senderData;
  GetTabInfoRef::ErrorCode errorCode =
    (GetTabInfoRef::ErrorCode) ref->errorCode;
  int do_resend_request = 0;
  TablePtr tabPtr;
  c_tablePool.getPtr(tabPtr, senderData);
  switch (errorCode)
  {
  case GetTabInfoRef::TableNotDefined:
    // wrong state
    break;
  case GetTabInfoRef::InvalidTableId:
    // no such table
    break;
  case GetTabInfoRef::Busy:
    do_resend_request = 1;
    break;
  case GetTabInfoRef::TableNameTooLong:
    ndbrequire(false);
    break;
  case GetTabInfoRef::NoFetchByName:
    break;
  }
  if (do_resend_request)
  {
    GetTabInfoReq * req = (GetTabInfoReq *)signal->getDataPtrSend();
    req->senderRef = reference();
    req->senderData = senderData;
    req->requestType = 
      GetTabInfoReq::RequestById | GetTabInfoReq::LongSignalConf;
    req->tableId = tableId;
    sendSignalWithDelay(DBDICT_REF, GSN_GET_TABINFOREQ, signal,
                        30, GetTabInfoReq::SignalLength);
    return;
  }
  tabPtr.p->m_state = Table::DROPPED;
  tabPtr.p->m_error = errorCode;
  completeAllSubscribers(signal, tabPtr);
  completeInitTable(signal, tabPtr);
}

void
Suma::execGET_TABINFO_CONF(Signal* signal){
  jamEntry();

  CRASH_INSERTION(13006);

  if(!assembleFragments(signal)){
    return;
  }
  
  GetTabInfoConf* conf = (GetTabInfoConf*)signal->getDataPtr();
  Uint32 tableId = conf->tableId;
  TablePtr tabPtr;
  c_tablePool.getPtr(tabPtr, conf->senderData);
  SegmentedSectionPtr ptr(0,0,0);
  signal->getSection(ptr, GetTabInfoConf::DICT_TAB_INFO);
  ndbrequire(tabPtr.p->parseTable(ptr, *this));
  releaseSections(signal);
  /**
   * We need to gather fragment info
   */
  jam();
  DihFragCountReq* req = (DihFragCountReq*)signal->getDataPtrSend();
  req->m_connectionData = RNIL;
  req->m_tableRef = tableId;
  req->m_senderData = tabPtr.i;
  sendSignal(DBDIH_REF, GSN_DI_FCOUNTREQ, signal, 
             DihFragCountReq::SignalLength, JBB);
}

bool
Suma::Table::parseTable(SegmentedSectionPtr ptr,
			Suma &suma)
{
  DBUG_ENTER("Suma::Table::parseTable");
  
  SimplePropertiesSectionReader it(ptr, suma.getSectionSegmentPool());
  
  SimpleProperties::UnpackStatus s;
  DictTabInfo::Table tableDesc; tableDesc.init();
  s = SimpleProperties::unpack(it, &tableDesc, 
			       DictTabInfo::TableMapping, 
			       DictTabInfo::TableMappingSize, 
			       true, true);

  jam();
  suma.suma_ndbrequire(s == SimpleProperties::Break);

#if 0
  //ToDo handle this
  if(table_version_major(m_schemaVersion) !=
     table_version_major(tableDesc.TableVersion)){
    jam();

    release(* this);

    // oops wrong schema version in stored tabledesc
    // we need to find all subscriptions with old table desc
    // and all subscribers to this
    // hopefully none
    c_tables.release(tabPtr);
    DBUG_PRINT("info",("c_tablePool  size: %d free: %d",
		       suma.c_tablePool.getSize(),
		       suma.c_tablePool.getNoOfFree()));
    tabPtr.setNull();
    DLHashTable<Suma::Subscription>::Iterator i_subPtr;
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
      if (subPtr.p->m_tables.get(tableId)) {
	jam();
	subPtr.p->m_tables.clear(tableId); // remove this old table reference
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
#endif

  m_noOfAttributes = tableDesc.NoOfAttributes;
  DBUG_RETURN(true);
}

void 
Suma::execDI_FCOUNTREF(Signal* signal)
{
  jamEntry();
  DBUG_ENTER("Suma::execDI_FCOUNTREF");
  DihFragCountRef * const ref = (DihFragCountRef*)signal->getDataPtr();
  switch ((DihFragCountRef::ErrorCode) ref->m_error)
  {
  case DihFragCountRef::ErroneousTableState:
    jam();
    if (ref->m_tableStatus == Dbdih::TabRecord::TS_CREATING)
    {
      const Uint32 tableId = ref->m_senderData;
      const Uint32 tabPtr_i = ref->m_tableRef;      
      DihFragCountReq * const req = (DihFragCountReq*)signal->getDataPtrSend();

      req->m_connectionData = RNIL;
      req->m_tableRef = tabPtr_i;
      req->m_senderData = tableId;
      sendSignalWithDelay(DBDIH_REF, GSN_DI_FCOUNTREQ, signal, 
                          DihFragCountReq::SignalLength, 
                          DihFragCountReq::RetryInterval);
      DBUG_VOID_RETURN;
    }
    ndbrequire(false);
  default:
    ndbrequire(false);
  }

  DBUG_VOID_RETURN;
}

void 
Suma::execDI_FCOUNTCONF(Signal* signal)
{
  jamEntry();
  DBUG_ENTER("Suma::execDI_FCOUNTCONF");
  ndbassert(signal->getNoOfSections() == 0);
  DihFragCountConf * const conf = (DihFragCountConf*)signal->getDataPtr();
  const Uint32 userPtr = conf->m_connectionData;
  const Uint32 fragCount = conf->m_fragmentCount;
  const Uint32 tableId = conf->m_tableRef;

  ndbrequire(userPtr == RNIL && signal->length() == 5);

  TablePtr tabPtr;
  tabPtr.i= conf->m_senderData;
  ndbrequire((tabPtr.p= c_tablePool.getPtr(tabPtr.i)) != 0);
  ndbrequire(tabPtr.p->m_tableId == tableId);

  LocalDataBuffer<15> fragBuf(c_dataBufferPool, tabPtr.p->m_fragments);
  ndbrequire(fragBuf.getSize() == 0);
  
  tabPtr.p->m_fragCount = fragCount;

  signal->theData[0] = RNIL;
  signal->theData[1] = tabPtr.i;
  signal->theData[2] = tableId;
  signal->theData[3] = 0; // Frag no
  sendSignal(DBDIH_REF, GSN_DIGETPRIMREQ, signal, 4, JBB);

  DBUG_VOID_RETURN;
}

void
Suma::execDIGETPRIMCONF(Signal* signal){
  jamEntry();
  DBUG_ENTER("Suma::execDIGETPRIMCONF");
  ndbassert(signal->getNoOfSections() == 0);

  const Uint32 userPtr = signal->theData[0];
  const Uint32 nodeCount = signal->theData[6];
  const Uint32 tableId = signal->theData[7];
  const Uint32 fragNo = signal->theData[8];
  
  ndbrequire(userPtr == RNIL && signal->length() == 9);
  ndbrequire(nodeCount > 0 && nodeCount <= MAX_REPLICAS);
  
  TablePtr tabPtr;
  tabPtr.i= signal->theData[1];
  ndbrequire((tabPtr.p= c_tablePool.getPtr(tabPtr.i)) != 0);
  ndbrequire(tabPtr.p->m_tableId == tableId);

  {
    LocalDataBuffer<15> fragBuf(c_dataBufferPool,tabPtr.p->m_fragments);  
    
    /**
     * Add primary node for fragment to list
     */
    FragmentDescriptor fd;
    fd.m_fragDesc.m_nodeId = signal->theData[2];
    fd.m_fragDesc.m_fragmentNo = fragNo;
    signal->theData[2] = fd.m_dummy;
    fragBuf.append(&signal->theData[2], 1);
  }
  
  const Uint32 nextFrag = fragNo + 1;
  if(nextFrag == tabPtr.p->m_fragCount)
  {
    /**
     * Complete frag info for table
     * table is not up to date
     */

    if (tabPtr.p->c_subscribers.isEmpty())
    {
      completeInitTable(signal,tabPtr);
      DBUG_VOID_RETURN;
    }
    tabPtr.p->setupTrigger(signal, *this);
    DBUG_VOID_RETURN;
  }
  signal->theData[0] = RNIL;
  signal->theData[1] = tabPtr.i;
  signal->theData[2] = tableId;
  signal->theData[3] = nextFrag; // Frag no
  sendSignal(DBDIH_REF, GSN_DIGETPRIMREQ, signal, 4, JBB);

  DBUG_VOID_RETURN;
}

#if 0
void
Suma::SyncRecord::completeTableInit(Signal* signal)
{
  jam();
  SubscriptionPtr subPtr;
  suma.c_subscriptions.getPtr(subPtr, m_subscriptionPtrI);
  
#if PRINT_ONLY
  ndbout_c("GSN_SUB_SYNC_CONF (meta)");
#else
 
  suma.releaseSections(signal);

  if (m_error) {
    SubSyncRef * const ref = (SubSyncRef*)signal->getDataPtrSend();
    ref->senderRef = suma.reference();
    ref->senderData = subPtr.p->m_senderData;
    ref->errorCode = SubSyncRef::Undefined;
    suma.sendSignal(subPtr.p->m_senderRef, GSN_SUB_SYNC_REF, signal,
		    SubSyncRef::SignalLength, JBB);
  } else {
    SubSyncConf * const conf = (SubSyncConf*)signal->getDataPtrSend();
    conf->senderRef = suma.reference();
    conf->senderData = subPtr.p->m_senderData;
    suma.sendSignal(subPtr.p->m_senderRef, GSN_SUB_SYNC_CONF, signal,
		    SubSyncConf::SignalLength, JBB);
  }
#endif
}
#endif

/**********************************************************
 *
 * Scan interface
 *
 */

void
Suma::SyncRecord::startScan(Signal* signal)
{
  jam();
  DBUG_ENTER("Suma::SyncRecord::startScan");
  
  /**
   * Get fraginfo
   */
  m_currentTable = 0;
  m_currentFragment = 0;
  nextScan(signal);
  DBUG_VOID_RETURN;
}

bool
Suma::SyncRecord::getNextFragment(TablePtr * tab, 
					     FragmentDescriptor * fd)
{
  jam();
  SubscriptionPtr subPtr;
  suma.c_subscriptions.getPtr(subPtr, m_subscriptionPtrI);
  TableList::DataBufferIterator tabIt;
  DataBuffer<15>::DataBufferIterator fragIt;
  
  m_tableList.position(tabIt, m_currentTable);
  for(; !tabIt.curr.isNull(); m_tableList.next(tabIt), m_currentTable++)
  {
    TablePtr tabPtr;
    ndbrequire(suma.c_tables.find(tabPtr, * tabIt.data));
    LocalDataBuffer<15> fragBuf(suma.c_dataBufferPool,  tabPtr.p->m_fragments);
    
    fragBuf.position(fragIt, m_currentFragment);
    for(; !fragIt.curr.isNull(); fragBuf.next(fragIt), m_currentFragment++)
    {
      FragmentDescriptor tmp;
      tmp.m_dummy = * fragIt.data;
      if(tmp.m_fragDesc.m_nodeId == suma.getOwnNodeId()){
	* fd = tmp;
	* tab = tabPtr;
	return true;
      }
    }
    m_currentFragment = 0;

    tabPtr.p->n_subscribers--;
    DBUG_PRINT("info",("Suma::Table[%u]::n_subscribers: %u",
		       tabPtr.p->m_tableId, tabPtr.p->n_subscribers));
    tabPtr.p->checkRelease(suma);
  }
  return false;
}

void
Suma::SyncRecord::nextScan(Signal* signal)
{
  jam();
  DBUG_ENTER("Suma::SyncRecord::nextScan");
  TablePtr tabPtr;
  FragmentDescriptor fd;
  SubscriptionPtr subPtr;
  if(!getNextFragment(&tabPtr, &fd)){
    jam();
    completeScan(signal);
    DBUG_VOID_RETURN;
  }
  suma.c_subscriptions.getPtr(subPtr, m_subscriptionPtrI);
 
  DataBuffer<15>::Head head = m_attributeList;
  LocalDataBuffer<15> attrBuf(suma.c_dataBufferPool, head);
  
  ScanFragReq * req = (ScanFragReq *)signal->getDataPtrSend();
  const Uint32 parallelism = 16;
  const Uint32 attrLen = 5 + attrBuf.getSize();

  req->senderData = ptrI;
  req->resultRef = suma.reference();
  req->tableId = tabPtr.p->m_tableId;
  req->requestInfo = 0;
  req->savePointId = 0;
  ScanFragReq::setLockMode(req->requestInfo, 0);
  ScanFragReq::setHoldLockFlag(req->requestInfo, 1);
  ScanFragReq::setKeyinfoFlag(req->requestInfo, 0);
  ScanFragReq::setAttrLen(req->requestInfo, attrLen);
  req->fragmentNoKeyLen = fd.m_fragDesc.m_fragmentNo;
  req->schemaVersion = tabPtr.p->m_schemaVersion;
  req->transId1 = 0;
  req->transId2 = (SUMA << 20) + (suma.getOwnNodeId() << 8);
  req->clientOpPtr = (ptrI << 16);
  req->batch_size_rows= parallelism;
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

  DBUG_VOID_RETURN;
}


void
Suma::execSCAN_FRAGREF(Signal* signal){
  jamEntry();

//  ScanFragRef * const ref = (ScanFragRef*)signal->getDataPtr();
  ndbrequire(false);
}

void
Suma::execSCAN_FRAGCONF(Signal* signal){
  jamEntry();
  DBUG_ENTER("Suma::execSCAN_FRAGCONF");
  ndbassert(signal->getNoOfSections() == 0);
  CRASH_INSERTION(13011);

  ScanFragConf * const conf = (ScanFragConf*)signal->getDataPtr();
  
  const Uint32 completed = conf->fragmentCompleted;
  const Uint32 senderData = conf->senderData;
  const Uint32 completedOps = conf->completedOps;

  Ptr<SyncRecord> syncPtr;
  c_syncPool.getPtr(syncPtr, senderData);
  
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
    req->subscriberData = syncPtr.p->m_senderData;
    req->noOfRowsSent = completedOps;
    sendSignal(syncPtr.p->m_senderRef, GSN_SUB_SYNC_CONTINUE_REQ, signal,
	       SubSyncContinueReq::SignalLength, JBB);
#endif
    DBUG_VOID_RETURN;
  }

  ndbrequire(completedOps == 0);
  
  syncPtr.p->m_currentFragment++;
  syncPtr.p->nextScan(signal);
  DBUG_VOID_RETURN;
}

void
Suma::execSUB_SYNC_CONTINUE_CONF(Signal* signal){
  jamEntry();
  ndbassert(signal->getNoOfSections() == 0);
  
  CRASH_INSERTION(13012);

  SubSyncContinueConf * const conf = 
    (SubSyncContinueConf*)signal->getDataPtr();  
  
  SubscriptionPtr subPtr;
  Subscription key; 
  key.m_subscriptionId = conf->subscriptionId;
  key.m_subscriptionKey = conf->subscriptionKey;
  
  ndbrequire(c_subscriptions.find(subPtr, key));

  ScanFragNextReq * req = (ScanFragNextReq *)signal->getDataPtrSend();
  req->senderData = subPtr.p->m_current_sync_ptrI;
  req->closeFlag = 0;
  req->transId1 = 0;
  req->transId2 = (SUMA << 20) + (getOwnNodeId() << 8);
  req->batch_size_rows = 16;
  req->batch_size_bytes = 0;
  sendSignal(DBLQH_REF, GSN_SCAN_NEXTREQ, signal, 
	     ScanFragNextReq::SignalLength, JBB);
}

void
Suma::SyncRecord::completeScan(Signal* signal, int error)
{
  jam();
  DBUG_ENTER("Suma::SyncRecord::completeScan");
  //  m_tableList.release();

#if PRINT_ONLY
  ndbout_c("GSN_SUB_SYNC_CONF (data)");
#else
  if (error == 0)
  {
    SubSyncConf * const conf = (SubSyncConf*)signal->getDataPtrSend();
    conf->senderRef = suma.reference();
    conf->senderData = m_senderData;
    suma.sendSignal(m_senderRef, GSN_SUB_SYNC_CONF, signal,
		    SubSyncConf::SignalLength, JBB);
  }
  else
  {
    SubSyncRef * const ref = (SubSyncRef*)signal->getDataPtrSend();
    ref->senderRef = suma.reference();
    ref->senderData = m_senderData;
    suma.sendSignal(m_senderRef, GSN_SUB_SYNC_REF, signal,
		    SubSyncRef::SignalLength, JBB);
  }
#endif

  release();
  
  Ptr<Subscription> subPtr;
  suma.c_subscriptions.getPtr(subPtr, m_subscriptionPtrI);
  ndbrequire(subPtr.p->m_current_sync_ptrI == ptrI);
  subPtr.p->m_current_sync_ptrI = RNIL;

  suma.c_syncPool.release(ptrI);
  DBUG_PRINT("info",("c_syncPool  size: %d free: %d",
		     suma.c_syncPool.getSize(),
		     suma.c_syncPool.getNoOfFree()));
  DBUG_VOID_RETURN;
}

void
Suma::execSCAN_HBREP(Signal* signal){
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
Suma::execSUB_START_REQ(Signal* signal){
  jamEntry();
  ndbassert(signal->getNoOfSections() == 0);
  DBUG_ENTER("Suma::execSUB_START_REQ");
  SubStartReq * const req = (SubStartReq*)signal->getDataPtr();

  CRASH_INSERTION(13013);
  Uint32 senderRef            = req->senderRef;
  Uint32 senderData           = req->senderData;
  Uint32 subscriberData       = req->subscriberData;
  Uint32 subscriberRef        = req->subscriberRef;
  SubscriptionData::Part part = (SubscriptionData::Part)req->part;

  Subscription key; 
  key.m_subscriptionId        = req->subscriptionId;
  key.m_subscriptionKey       = req->subscriptionKey;

  if (c_startup.m_restart_server_node_id && 
      senderRef != calcSumaBlockRef(c_startup.m_restart_server_node_id))
  {
    /**
     * only allow "restart_server" Suma's to come through 
     * for restart purposes
     */
    jam();
    Uint32 err = c_startup.m_restart_server_node_id != RNIL ? 1405 : 
      SubStartRef::NF_FakeErrorREF;
    
    sendSubStartRef(signal, err);
    DBUG_VOID_RETURN;
  }
  
  SubscriptionPtr subPtr;
  if(!c_subscriptions.find(subPtr, key)){
    jam();
    sendSubStartRef(signal, 1407);
    DBUG_VOID_RETURN;
  }
  
  if (subPtr.p->m_state == Subscription::LOCKED) {
    jam();
    DBUG_PRINT("info",("Locked"));
    sendSubStartRef(signal, 1411);
    DBUG_VOID_RETURN;
  }

  if (subPtr.p->m_state == Subscription::DROPPED &&
      c_startup.m_restart_server_node_id == 0) {
    jam();
    DBUG_PRINT("info",("Dropped"));
    sendSubStartRef(signal, 1418);
    DBUG_VOID_RETURN;
  }

  ndbrequire(subPtr.p->m_state == Subscription::DEFINED ||
             c_startup.m_restart_server_node_id);

  SubscriberPtr subbPtr;
  if(!c_subscriberPool.seize(subbPtr)){
    jam();
    sendSubStartRef(signal, 1412);
    DBUG_VOID_RETURN;
  }

  if (c_startup.m_restart_server_node_id == 0 && 
      !c_connected_nodes.get(refToNode(subscriberRef)))
    
  {
    jam();
    sendSubStartRef(signal, SubStartRef::PartiallyConnected);
    return;
  }
  
  DBUG_PRINT("info",("c_subscriberPool  size: %d free: %d",
		     c_subscriberPool.getSize(),
		     c_subscriberPool.getNoOfFree()));

  c_subscriber_nodes.set(refToNode(subscriberRef));

  // setup subscription record
  if (subPtr.p->m_state == Subscription::DEFINED)
    subPtr.p->m_state = Subscription::LOCKED;
  // store these here for later use
  subPtr.p->m_senderRef  = senderRef;
  subPtr.p->m_senderData = senderData;

  // setup subscriber record
  subbPtr.p->m_senderRef  = subscriberRef;
  subbPtr.p->m_senderData = subscriberData;
  subbPtr.p->m_subPtrI= subPtr.i;

  DBUG_PRINT("info",("subscriber: %u[%u,%u] subscription: %u[%u,%u] "
		     "tableId: %u id: %u key: %u",
		     subbPtr.i, subbPtr.p->m_senderRef, subbPtr.p->m_senderData,
		     subPtr.i,  subPtr.p->m_senderRef,  subPtr.p->m_senderData,
		     subPtr.p->m_tableId,
		     subPtr.p->m_subscriptionId,subPtr.p->m_subscriptionKey));

  TablePtr tabPtr;
  switch(part){
  case SubscriptionData::MetaData:
    jam();
    c_metaSubscribers.add(subbPtr);
    sendSubStartComplete(signal, subbPtr, 0, part);
    DBUG_VOID_RETURN;
  case SubscriptionData::TableData: 
    jam();
    initTable(signal,subPtr.p->m_tableId,tabPtr,subbPtr);
    tabPtr.p->n_subscribers++;
    if (subPtr.p->m_options & Subscription::REPORT_ALL)
      tabPtr.p->m_reportAll = true;
    DBUG_PRINT("info",("Suma::Table[%u]::n_subscribers: %u",
		       tabPtr.p->m_tableId, tabPtr.p->n_subscribers));
    DBUG_VOID_RETURN;
  }
  ndbrequire(false);
}

void
Suma::sendSubStartComplete(Signal* signal,
			   SubscriberPtr subbPtr, 
			   Uint64 firstGCI,
			   SubscriptionData::Part part)
{
  jam();
  DBUG_ENTER("Suma::sendSubStartComplete");

  SubscriptionPtr subPtr;
  c_subscriptions.getPtr(subPtr, subbPtr.p->m_subPtrI);
  ndbrequire(subPtr.p->m_state == Subscription::LOCKED ||
             (subPtr.p->m_state == Subscription::DROPPED &&
              c_startup.m_restart_server_node_id));
  if (subPtr.p->m_state == Subscription::LOCKED)
  {
    jam();
    subPtr.p->m_state = Subscription::DEFINED;
  }
  subPtr.p->n_subscribers++;

  DBUG_PRINT("info",("subscriber: %u[%u,%u] subscription: %u[%u,%u] "
		     "tableId: %u[i=%u] id: %u key: %u",
		     subbPtr.i, subbPtr.p->m_senderRef, subbPtr.p->m_senderData,
		     subPtr.i,  subPtr.p->m_senderRef,  subPtr.p->m_senderData,
		     subPtr.p->m_tableId, subPtr.p->m_table_ptrI,
		     subPtr.p->m_subscriptionId,subPtr.p->m_subscriptionKey));

  SubStartConf * const conf = (SubStartConf*)signal->getDataPtrSend();
  
  conf->senderRef       = reference();
  conf->senderData      = subPtr.p->m_senderData;
  conf->subscriptionId  = subPtr.p->m_subscriptionId;
  conf->subscriptionKey = subPtr.p->m_subscriptionKey;
  conf->firstGCI        = firstGCI;
  conf->part            = (Uint32) part;

  DBUG_PRINT("info",("subscriber: %u id: %u key: %u", subbPtr.i,
		     subPtr.p->m_subscriptionId,subPtr.p->m_subscriptionKey));
  sendSignal(subPtr.p->m_senderRef, GSN_SUB_START_CONF, signal,
	     SubStartConf::SignalLength, JBB);

  reportAllSubscribers(signal, NdbDictionary::Event::_TE_SUBSCRIBE,
                       subPtr, subbPtr);

  DBUG_VOID_RETURN;
}

void
Suma::sendSubStartRef(Signal* signal, Uint32 errCode)
{
  jam();
  SubStartRef * ref = (SubStartRef *)signal->getDataPtrSend();
  ref->senderRef = reference();
  ref->errorCode = errCode;
  releaseSections(signal);
  sendSignal(signal->getSendersBlockRef(), GSN_SUB_START_REF, signal, 
	     SubStartRef::SignalLength, JBB);
}
void
Suma::sendSubStartRef(Signal* signal,
				 SubscriberPtr subbPtr, Uint32 error,
				 SubscriptionData::Part part)
{
  jam();

  SubscriptionPtr subPtr;
  c_subscriptions.getPtr(subPtr, subbPtr.p->m_subPtrI);

  ndbrequire(subPtr.p->m_state == Subscription::LOCKED ||
             (subPtr.p->m_state == Subscription::DROPPED &&
              c_startup.m_restart_server_node_id));
  if (subPtr.p->m_state == Subscription::LOCKED)
  {
    jam();
    subPtr.p->m_state = Subscription::DEFINED;
  }

  SubStartRef * ref= (SubStartRef *)signal->getDataPtrSend();
  ref->senderRef        = reference();
  ref->senderData       = subPtr.p->m_senderData;
  ref->subscriptionId   = subPtr.p->m_subscriptionId;
  ref->subscriptionKey  = subPtr.p->m_subscriptionKey;
  ref->part             = (Uint32) part;
  ref->errorCode        = error;

  sendSignal(subPtr.p->m_senderRef, GSN_SUB_START_REF, signal, 
	     SubStartRef::SignalLength, JBB);
}

/**********************************************************
 * Suma participant interface
 *
 * Stopping and removing of subscriber
 *
 */

void
Suma::execSUB_STOP_REQ(Signal* signal){
  jamEntry();
  ndbassert(signal->getNoOfSections() == 0);
  DBUG_ENTER("Suma::execSUB_STOP_REQ");
  
  CRASH_INSERTION(13019);

  SubStopReq * const req = (SubStopReq*)signal->getDataPtr();
  Uint32 senderRef      = req->senderRef;
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
      subscriberData == 0)
  {
    SubStopConf* conf = (SubStopConf*)signal->getDataPtrSend();
    
    conf->senderRef       = reference();
    conf->senderData      = senderData;
    conf->subscriptionId  = key.m_subscriptionId;
    conf->subscriptionKey = key.m_subscriptionKey;
    conf->subscriberData  = subscriberData;

    sendSignal(senderRef, GSN_SUB_STOP_CONF, signal,
	       SubStopConf::SignalLength, JBB);

    removeSubscribersOnNode(signal, refToNode(senderRef));
    DBUG_VOID_RETURN;
  }

  if (c_startup.m_restart_server_node_id && 
      senderRef != calcSumaBlockRef(c_startup.m_restart_server_node_id))
  {
    /**
     * only allow "restart_server" Suma's to come through 
     * for restart purposes
     */
    jam();
    Uint32 err = c_startup.m_restart_server_node_id != RNIL ? 1405 : 
      SubStopRef::NF_FakeErrorREF;
    
    sendSubStopRef(signal, err);
    DBUG_VOID_RETURN;
  }

  if(!c_subscriptions.find(subPtr, key)){
    jam();
    DBUG_PRINT("error", ("not found"));
    sendSubStopRef(signal, 1407);
    DBUG_VOID_RETURN;
  }
  
  if (subPtr.p->m_state == Subscription::LOCKED) {
    jam();
    DBUG_PRINT("error", ("locked"));
    sendSubStopRef(signal, 1411);
    DBUG_VOID_RETURN;
  }

  ndbrequire(part == SubscriptionData::TableData);

  TablePtr tabPtr;
  tabPtr.i = subPtr.p->m_table_ptrI;
  if (tabPtr.i == RNIL ||
      !(tabPtr.p = c_tables.getPtr(tabPtr.i)) ||
      tabPtr.p->m_tableId != subPtr.p->m_tableId)
  {
    jam();
    DBUG_PRINT("error", ("no such table id %u[i=%u]",
			 subPtr.p->m_tableId, subPtr.p->m_table_ptrI));
    sendSubStopRef(signal, 1417);
    DBUG_VOID_RETURN;
  }

  if (tabPtr.p->m_drop_subbPtr.p != 0) {
    jam();
    DBUG_PRINT("error", ("table locked"));
    sendSubStopRef(signal, 1420);
    DBUG_VOID_RETURN;
  }

  DBUG_PRINT("info",("subscription: %u tableId: %u[i=%u] id: %u key: %u",
		     subPtr.i, subPtr.p->m_tableId, tabPtr.i,
		     subPtr.p->m_subscriptionId,subPtr.p->m_subscriptionKey));

  SubscriberPtr subbPtr;
  if (senderRef == reference()){
    jam();
    c_subscriberPool.getPtr(subbPtr, senderData);
    ndbrequire(subbPtr.p->m_subPtrI == subPtr.i && 
	       subbPtr.p->m_senderRef == subscriberRef &&
	       subbPtr.p->m_senderData == subscriberData);
    c_removeDataSubscribers.remove(subbPtr);
  }
  else
  {
    jam();
    LocalDLList<Subscriber>
      subscribers(c_subscriberPool,tabPtr.p->c_subscribers);

    DBUG_PRINT("info",("search: subscription: %u, ref: %u, data: %d",
		       subPtr.i, subscriberRef, subscriberData));
    for (subscribers.first(subbPtr);!subbPtr.isNull();subscribers.next(subbPtr))
    {
      jam();
      DBUG_PRINT("info",
		 ("search: subscription: %u, ref: %u, data: %u, subscriber %u", 
		  subbPtr.p->m_subPtrI, subbPtr.p->m_senderRef,
		  subbPtr.p->m_senderData, subbPtr.i));
      if (subbPtr.p->m_subPtrI == subPtr.i &&
	  subbPtr.p->m_senderRef == subscriberRef &&
	  subbPtr.p->m_senderData == subscriberData)
      {
	jam();
	DBUG_PRINT("info",("found"));
	break;
      }
    }
    /**
     * If we didn't find anyone, send ref
     */
    if (subbPtr.isNull()) {
      jam();
      DBUG_PRINT("error", ("subscriber not found"));
      sendSubStopRef(signal, 1407);
      DBUG_VOID_RETURN;
    }
    subscribers.remove(subbPtr);
  }

  subPtr.p->m_senderRef  = senderRef; // store ref to requestor
  subPtr.p->m_senderData = senderData; // store ref to requestor

  tabPtr.p->m_drop_subbPtr = subbPtr;

  if (subPtr.p->m_state == Subscription::DEFINED)
  {
    jam();
    subPtr.p->m_state = Subscription::LOCKED;
  }

  if (tabPtr.p->m_state == Table::DROPPED)
    // not ALTERED here since trigger must be removed
  {
    jam();
    tabPtr.p->n_subscribers--;
    DBUG_PRINT("info",("Suma::Table[%u]::n_subscribers: %u",
		       tabPtr.p->m_tableId, tabPtr.p->n_subscribers));
    tabPtr.p->checkRelease(*this);
    sendSubStopComplete(signal, tabPtr.p->m_drop_subbPtr);
    tabPtr.p->m_drop_subbPtr.p = 0;
  }
  else
  {
    jam();
    tabPtr.p->dropTrigger(signal,*this);
  }
  DBUG_VOID_RETURN;
}

void
Suma::sendSubStopComplete(Signal* signal, SubscriberPtr subbPtr)
{
  jam();
  DBUG_ENTER("Suma::sendSubStopComplete");
  CRASH_INSERTION(13020);

  DBUG_PRINT("info",("removed subscriber: %i", subbPtr.i));

  SubscriptionPtr subPtr;
  c_subscriptions.getPtr(subPtr, subbPtr.p->m_subPtrI);

  Uint32 senderRef= subPtr.p->m_senderRef;
  Uint32 senderData= subPtr.p->m_senderData;

  subPtr.p->n_subscribers--;
  ndbassert( subPtr.p->m_state == Subscription::LOCKED ||
	     subPtr.p->m_state == Subscription::DROPPED );
  if ( subPtr.p->m_state == Subscription::LOCKED )
  {
    jam();
    subPtr.p->m_state = Subscription::DEFINED;
    if (subPtr.p->n_subscribers == 0)
    {
      jam();
#if 1
      subPtr.p->m_table_ptrI = RNIL;
#else
      TablePtr tabPtr;
      tabPtr.i = subPtr.p->m_table_ptrI;
      if ((tabPtr.p= c_tablePool.getPtr(tabPtr.i)) &&
	  (tabPtr.p->m_state == Table::DROPPED ||
	   tabPtr.p->m_state == Table::ALTERED) &&
	  false)
      {
	// last subscriber, and table is dropped
	// safe to drop subscription
	c_subscriptions.release(subPtr);
	DBUG_PRINT("info",("c_subscriptionPool  size: %d free: %d",
			   c_subscriptionPool.getSize(),
			   c_subscriptionPool.getNoOfFree()));
      }
      else
      {
	subPtr.p->m_table_ptrI = RNIL;
      }
      ndbassert(tabPtr.p != 0);
#endif
    }
  }
  else if ( subPtr.p->n_subscribers == 0 )
  {
    // subscription is marked to be removed
    // and there are no subscribers left
    jam();
    ndbassert(subPtr.p->m_state == Subscription::DROPPED);
    completeSubRemove(subPtr);
  }

  // let subscriber know that subscrber is stopped
  {
    const Uint64 gci = get_current_gci(signal);
    SubTableData * data  = (SubTableData*)signal->getDataPtrSend();
    data->gci_hi         = Uint32(gci >> 32);
    data->gci_lo         = Uint32(gci);
    data->tableId        = 0;
    data->requestInfo    = 0;
    SubTableData::setOperation(data->requestInfo, 
			       NdbDictionary::Event::_TE_STOP);
    SubTableData::setNdbdNodeId(data->requestInfo,
				getOwnNodeId());
    data->senderData     = subbPtr.p->m_senderData;
    sendSignal(subbPtr.p->m_senderRef, GSN_SUB_TABLE_DATA, signal,
	       SubTableData::SignalLength, JBB);
  }
  
  SubStopConf * const conf = (SubStopConf*)signal->getDataPtrSend();
  
  conf->senderRef= reference();
  conf->senderData= senderData;

  sendSignal(senderRef, GSN_SUB_STOP_CONF, signal,
	     SubStopConf::SignalLength, JBB);

  c_subscriberPool.release(subbPtr);
  DBUG_PRINT("info",("c_subscriberPool  size: %d free: %d",
		     c_subscriberPool.getSize(),
		     c_subscriberPool.getNoOfFree()));

  reportAllSubscribers(signal, NdbDictionary::Event::_TE_UNSUBSCRIBE,
                       subPtr, subbPtr);

  DBUG_VOID_RETURN;
}

// report new started subscriber to all other subscribers
void
Suma::reportAllSubscribers(Signal *signal,
                           NdbDictionary::Event::_TableEvent table_event,
                           SubscriptionPtr subPtr,
                           SubscriberPtr subbPtr)
{
  const Uint64 gci = get_current_gci(signal);
  SubTableData * data  = (SubTableData*)signal->getDataPtrSend();

  if (table_event == NdbDictionary::Event::_TE_SUBSCRIBE &&
      !c_startup.m_restart_server_node_id)
  {
    data->gci_hi         = Uint32(gci >> 32);
    data->gci_lo         = Uint32(gci);
    data->tableId        = subPtr.p->m_tableId;
    data->requestInfo    = 0;
    SubTableData::setOperation(data->requestInfo, 
			       NdbDictionary::Event::_TE_ACTIVE);
    SubTableData::setNdbdNodeId(data->requestInfo, getOwnNodeId());
    SubTableData::setReqNodeId(data->requestInfo, 
			       refToNode(subbPtr.p->m_senderRef));
    data->changeMask     = 0;
    data->totalLen       = 0;
    data->senderData     = subbPtr.p->m_senderData;
    sendSignal(subbPtr.p->m_senderRef, GSN_SUB_TABLE_DATA, signal,
               SubTableData::SignalLength, JBB);
  }

  if (!(subPtr.p->m_options & Subscription::REPORT_SUBSCRIBE))
  {
    return;
  }
  if (subPtr.p->n_subscribers == 0)
  {
    ndbrequire(table_event != NdbDictionary::Event::_TE_SUBSCRIBE);
    return;
  }
 
//#ifdef VM_TRACE
  ndbout_c("reportAllSubscribers  subPtr.i: %d  subPtr.p->n_subscribers: %d",
           subPtr.i, subPtr.p->n_subscribers);
//#endif
  data->gci_hi         = Uint32(gci >> 32);
  data->gci_lo         = Uint32(gci);
  data->tableId        = subPtr.p->m_tableId;
  data->requestInfo    = 0;
  SubTableData::setOperation(data->requestInfo, table_event);
  SubTableData::setNdbdNodeId(data->requestInfo, getOwnNodeId());
  data->changeMask     = 0;
  data->totalLen       = 0;
  
  TablePtr tabPtr;
  c_tables.getPtr(tabPtr, subPtr.p->m_table_ptrI);
  LocalDLList<Subscriber> subbs(c_subscriberPool, tabPtr.p->c_subscribers);
  SubscriberPtr i_subbPtr;
  for(subbs.first(i_subbPtr); !i_subbPtr.isNull(); subbs.next(i_subbPtr))
  {
    if (i_subbPtr.p->m_subPtrI == subPtr.i)
    {
      SubTableData::setReqNodeId(data->requestInfo, 
				 refToNode(subbPtr.p->m_senderRef));
      data->senderData = i_subbPtr.p->m_senderData;
      sendSignal(i_subbPtr.p->m_senderRef, GSN_SUB_TABLE_DATA, signal,
                 SubTableData::SignalLength, JBB);
//#ifdef VM_TRACE
      ndbout_c("sent %s(%d) to node %d, req_nodeid: %d  senderData: %d",
               table_event == NdbDictionary::Event::_TE_SUBSCRIBE ?
               "SUBSCRIBE" : "UNSUBSCRIBE", (int) table_event,
               refToNode(i_subbPtr.p->m_senderRef),
               refToNode(subbPtr.p->m_senderRef), data->senderData
               );
//#endif
      if (i_subbPtr.i != subbPtr.i)
      {
	SubTableData::setReqNodeId(data->requestInfo, 
				   refToNode(i_subbPtr.p->m_senderRef));
	
        data->senderData = subbPtr.p->m_senderData;
        sendSignal(subbPtr.p->m_senderRef, GSN_SUB_TABLE_DATA, signal,
                   SubTableData::SignalLength, JBB);
//#ifdef VM_TRACE
        ndbout_c("sent %s(%d) to node %d, req_nodeid: %d  senderData: %d",
                 table_event == NdbDictionary::Event::_TE_SUBSCRIBE ?
                 "SUBSCRIBE" : "UNSUBSCRIBE", (int) table_event,
                 refToNode(subbPtr.p->m_senderRef),
                 refToNode(i_subbPtr.p->m_senderRef), data->senderData
                 );
//#endif
      }
    }
  }
}

void
Suma::sendSubStopRef(Signal* signal, Uint32 errCode)
{
  jam();
  DBUG_ENTER("Suma::sendSubStopRef");
  SubStopRef  * ref = (SubStopRef *)signal->getDataPtrSend();
  ref->senderRef = reference();
  ref->errorCode = errCode;
  sendSignal(signal->getSendersBlockRef(), 
	     GSN_SUB_STOP_REF, 
	     signal, 
	     SubStopRef::SignalLength,
	     JBB);
  DBUG_VOID_RETURN;
}

/**********************************************************
 *
 * Trigger admin interface
 *
 */

int
Suma::Table::setupTrigger(Signal* signal,
			  Suma &suma)
{
  jam();
  DBUG_ENTER("Suma::Table::setupTrigger");

  int ret= 0;
  
  AttributeMask attrMask;
  createAttributeMask(attrMask, suma);

  for(Uint32 j = 0; j<3; j++)
  {
    Uint32 triggerId = (m_schemaVersion << 18) | (j << 16) | m_ptrI;
    if(m_hasTriggerDefined[j] == 0)
    {
      suma.suma_ndbrequire(m_triggerIds[j] == ILLEGAL_TRIGGER_ID);
      DBUG_PRINT("info",("DEFINING trigger on table %u[%u]", m_tableId, j));
      CreateTrigReq * const req = (CreateTrigReq*)signal->getDataPtrSend();
      req->setUserRef(SUMA_REF);
      req->setConnectionPtr(m_ptrI);
      req->setTriggerType(TriggerType::SUBSCRIPTION_BEFORE);
      req->setTriggerActionTime(TriggerActionTime::TA_DETACHED);
      req->setMonitorReplicas(true);
      //req->setMonitorAllAttributes(j == TriggerEvent::TE_DELETE);
      req->setMonitorAllAttributes(true);
      req->setReceiverRef(SUMA_REF);
      req->setTriggerId(triggerId);
      req->setTriggerEvent((TriggerEvent::Value)j);
      req->setTableId(m_tableId);
      req->setAttributeMask(attrMask);
      req->setReportAllMonitoredAttributes(m_reportAll);
      suma.sendSignal(DBTUP_REF, GSN_CREATE_TRIG_REQ, 
		      signal, CreateTrigReq::SignalLength, JBB);
      ret= 1;
    }
    else
    {
      m_hasTriggerDefined[j]++;
      DBUG_PRINT("info",("REFCOUNT trigger on table %u[%u] %u",
			 m_tableId, j, m_hasTriggerDefined[j]));
    }
  }
  DBUG_RETURN(ret);
}

void
Suma::Table::createAttributeMask(AttributeMask& mask,
                                            Suma &suma)
{
  jam();
  mask.clear();
  for(Uint32 i = 0; i<m_noOfAttributes; i++)
    mask.set(i);
}

void
Suma::execCREATE_TRIG_CONF(Signal* signal){
  jamEntry();
  DBUG_ENTER("Suma::execCREATE_TRIG_CONF");
  ndbassert(signal->getNoOfSections() == 0);
  CreateTrigConf * const conf = (CreateTrigConf*)signal->getDataPtr();
  const Uint32 triggerId = conf->getTriggerId();
  Uint32 type = (triggerId >> 16) & 0x3;
  Uint32 tableId = conf->getTableId();


  DBUG_PRINT("enter", ("type: %u tableId: %u[i=%u==%u]",
		       type, tableId,conf->getConnectionPtr(),triggerId & 0xFFFF));
 
  TablePtr tabPtr;
  c_tables.getPtr(tabPtr, conf->getConnectionPtr());
  ndbrequire(tabPtr.p->m_tableId == tableId);
  ndbrequire(tabPtr.p->m_state == Table::DEFINING);

  ndbrequire(type < 3);
  tabPtr.p->m_triggerIds[type] = triggerId;
  ndbrequire(tabPtr.p->m_hasTriggerDefined[type] == 0);
  tabPtr.p->m_hasTriggerDefined[type] = 1;

  if (type == 2)
  {
    completeAllSubscribers(signal, tabPtr);
    completeInitTable(signal,tabPtr);
    DBUG_VOID_RETURN;
  }
  DBUG_VOID_RETURN;
}

void
Suma::execCREATE_TRIG_REF(Signal* signal){
  jamEntry();
  DBUG_ENTER("Suma::execCREATE_TRIG_REF");
  ndbassert(signal->getNoOfSections() == 0);  
  CreateTrigRef * const ref = (CreateTrigRef*)signal->getDataPtr();
  const Uint32 triggerId = ref->getTriggerId();
  Uint32 type = (triggerId >> 16) & 0x3;
  Uint32 tableId = ref->getTableId();
  
  DBUG_PRINT("enter", ("type: %u tableId: %u[i=%u==%u]",
		       type, tableId,ref->getConnectionPtr(),triggerId & 0xFFFF));
 
  TablePtr tabPtr;
  c_tables.getPtr(tabPtr, ref->getConnectionPtr());
  ndbrequire(tabPtr.p->m_tableId == tableId);
  ndbrequire(tabPtr.p->m_state == Table::DEFINING);

  tabPtr.p->m_error= ref->getErrorCode();

  ndbrequire(type < 3);

  if (type == 2)
  {
    completeAllSubscribers(signal, tabPtr);
    completeInitTable(signal,tabPtr);
    DBUG_VOID_RETURN;
  }

  DBUG_VOID_RETURN;
}

void
Suma::Table::dropTrigger(Signal* signal,Suma& suma)
{
  jam();
  DBUG_ENTER("Suma::dropTrigger");
  
  m_hasOutstandingTriggerReq[0] =
    m_hasOutstandingTriggerReq[1] =
    m_hasOutstandingTriggerReq[2] = 1;
  for(Uint32 j = 0; j<3; j++){
    jam();
    suma.suma_ndbrequire(m_triggerIds[j] != ILLEGAL_TRIGGER_ID);
    if(m_hasTriggerDefined[j] == 1) {
      jam();

      DropTrigReq * const req = (DropTrigReq*)signal->getDataPtrSend();
      req->setConnectionPtr(m_ptrI);
      req->setUserRef(SUMA_REF); // Sending to myself
      req->setRequestType(DropTrigReq::RT_USER);
      req->setTriggerType(TriggerType::SUBSCRIPTION_BEFORE);
      req->setTriggerActionTime(TriggerActionTime::TA_DETACHED);
      req->setIndexId(RNIL);

      req->setTableId(m_tableId);
      req->setTriggerId(m_triggerIds[j]);
      req->setTriggerEvent((TriggerEvent::Value)j);

      DBUG_PRINT("info",("DROPPING trigger %u = %u %u %u on table %u[%u]",
			 m_triggerIds[j],
			 TriggerType::SUBSCRIPTION_BEFORE,
			 TriggerActionTime::TA_DETACHED,
			 j,
			 m_tableId, j));
      suma.sendSignal(DBTUP_REF, GSN_DROP_TRIG_REQ,
		      signal, DropTrigReq::SignalLength, JBB);
    } else {
      jam();
      suma.suma_ndbrequire(m_hasTriggerDefined[j] > 1);
      runDropTrigger(signal,m_triggerIds[j],suma);
    }
  }
  DBUG_VOID_RETURN;
}

void
Suma::execDROP_TRIG_REF(Signal* signal){
  jamEntry();
  DBUG_ENTER("Suma::execDROP_TRIG_REF");
  ndbassert(signal->getNoOfSections() == 0);
  DropTrigRef * const ref = (DropTrigRef*)signal->getDataPtr();
  if (ref->getErrorCode() != DropTrigRef::TriggerNotFound)
  {
    ndbrequire(false);
  }
  TablePtr tabPtr;
  c_tables.getPtr(tabPtr, ref->getConnectionPtr());
  ndbrequire(ref->getTableId() == tabPtr.p->m_tableId);

  tabPtr.p->runDropTrigger(signal, ref->getTriggerId(), *this);
  DBUG_VOID_RETURN;
}

void
Suma::execDROP_TRIG_CONF(Signal* signal){
  jamEntry();
  DBUG_ENTER("Suma::execDROP_TRIG_CONF");
  ndbassert(signal->getNoOfSections() == 0);

  DropTrigConf * const conf = (DropTrigConf*)signal->getDataPtr();
  TablePtr tabPtr;
  c_tables.getPtr(tabPtr, conf->getConnectionPtr());
  ndbrequire(conf->getTableId() == tabPtr.p->m_tableId);

  tabPtr.p->runDropTrigger(signal, conf->getTriggerId(),*this);
  DBUG_VOID_RETURN;
}

void
Suma::Table::runDropTrigger(Signal* signal,
				       Uint32 triggerId,
				       Suma &suma)
{
  jam();
  Uint32 type = (triggerId >> 16) & 0x3;

  suma.suma_ndbrequire(type < 3);
  suma.suma_ndbrequire(m_triggerIds[type] == triggerId);
  suma.suma_ndbrequire(m_hasTriggerDefined[type] > 0);
  suma.suma_ndbrequire(m_hasOutstandingTriggerReq[type] == 1);
  m_hasTriggerDefined[type]--;
  m_hasOutstandingTriggerReq[type] = 0;
  if (m_hasTriggerDefined[type] == 0)
  {
    jam();
    m_triggerIds[type] = ILLEGAL_TRIGGER_ID;
  }
  if( m_hasOutstandingTriggerReq[0] ||
      m_hasOutstandingTriggerReq[1] ||
      m_hasOutstandingTriggerReq[2])
  {
    // more to come
    jam();
    return;
  }

#if 0
  ndbout_c("trigger completed");
#endif


  n_subscribers--;
  DBUG_PRINT("info",("Suma::Table[%u]::n_subscribers: %u",
		     m_tableId, n_subscribers));
  checkRelease(suma);

  suma.sendSubStopComplete(signal, m_drop_subbPtr);
  m_drop_subbPtr.p = 0;
}

void Suma::suma_ndbrequire(bool v) { ndbrequire(v); }

void
Suma::Table::checkRelease(Suma &suma)
{
  jam();
  DBUG_ENTER("Suma::Table::checkRelease");
  if (n_subscribers == 0)
  {
    jam();
    suma.suma_ndbrequire(m_hasTriggerDefined[0] == 0);
    suma.suma_ndbrequire(m_hasTriggerDefined[1] == 0);
    suma.suma_ndbrequire(m_hasTriggerDefined[2] == 0);
    if (!c_subscribers.isEmpty())
    {
      LocalDLList<Subscriber>
	subscribers(suma.c_subscriberPool,c_subscribers);
      SubscriberPtr subbPtr;
      for (subscribers.first(subbPtr);!subbPtr.isNull();
	   subscribers.next(subbPtr))
      {
	jam();
	DBUG_PRINT("info",("subscriber: %u", subbPtr.i));
      }
      suma.suma_ndbrequire(false);
    }
    if (!c_syncRecords.isEmpty())
    {
      LocalDLList<SyncRecord>
	syncRecords(suma.c_syncPool,c_syncRecords);
      Ptr<SyncRecord> syncPtr;
      for (syncRecords.first(syncPtr);!syncPtr.isNull();
	   syncRecords.next(syncPtr))
      {
	jam();
	DBUG_PRINT("info",("syncRecord: %u", syncPtr.i));
      }
      suma.suma_ndbrequire(false);
    }
    release(suma);
    suma.c_tables.remove(m_ptrI);
    suma.c_tablePool.release(m_ptrI);
    DBUG_PRINT("info",("c_tablePool  size: %d free: %d",
		       suma.c_tablePool.getSize(),
		       suma.c_tablePool.getNoOfFree()));
  }
  else
  {
    DBUG_PRINT("info",("n_subscribers: %d", n_subscribers));
  }
  DBUG_VOID_RETURN;
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
Suma::execTRANSID_AI(Signal* signal)
{
  jamEntry();
  DBUG_ENTER("Suma::execTRANSID_AI");

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
  Uint32 ref = subPtr.p->m_senderRef;
  sdata->tableId = syncPtr.p->m_currentTableId;
  sdata->senderData = subPtr.p->m_senderData;
  sdata->requestInfo = 0;
  SubTableData::setOperation(sdata->requestInfo, 
			     NdbDictionary::Event::_TE_SCAN); // Scan
  sdata->gci_hi = 0; // Undefined
  sdata->gci_lo = 0;
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

  DBUG_VOID_RETURN;
}

/**********************************************************
 *
 * Trigger data interface
 *
 */

void
Suma::execTRIG_ATTRINFO(Signal* signal)
{
  jamEntry();
  DBUG_ENTER("Suma::execTRIG_ATTRINFO");

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

  
  DBUG_VOID_RETURN;
}

#ifdef NODEFAIL_DEBUG2
static int theCounts[64] = {0};
#endif

Uint32 
Suma::get_responsible_node(Uint32 bucket) const
{
  // id will contain id to responsible suma or 
  // RNIL if we don't have nodegroup info yet

  jam();
  Uint32 node;
  const Bucket* ptr= c_buckets + bucket;
  for(Uint32 i = 0; i<MAX_REPLICAS; i++)
  {
    node= ptr->m_nodes[i];
    if(c_alive_nodes.get(node))
    {
      break;
    }
  }
  
  
#ifdef NODEFAIL_DEBUG2
  if(node != 0)
  {
    theCounts[node]++;
    ndbout_c("Suma:responsible n=%u, D=%u, id = %u, count=%u",
	     n,D, id, theCounts[node]);
  }
#endif
  return node;
}

Uint32 
Suma::get_responsible_node(Uint32 bucket, const NdbNodeBitmask& mask) const
{
  jam();
  Uint32 node;
  const Bucket* ptr= c_buckets + bucket;
  for(Uint32 i = 0; i<MAX_REPLICAS; i++)
  {
    node= ptr->m_nodes[i];
    if(mask.get(node))
    {
      return node;
    }
  }
  
  return 0;
}

bool
Suma::check_switchover(Uint32 bucket, Uint64 gci)
{
  const Uint32 send_mask = (Bucket::BUCKET_STARTING | Bucket::BUCKET_TAKEOVER);
  bool send = c_buckets[bucket].m_state & send_mask;
  ndbassert(m_switchover_buckets.get(bucket));
  if(unlikely(gci > c_buckets[bucket].m_switchover_gci))
  {
    return send;
  }
  return !send;
}

static 
Uint32 
reformat(Signal* signal, LinearSectionPtr ptr[3],
	 Uint32 * src_1, Uint32 sz_1,
	 Uint32 * src_2, Uint32 sz_2)
{
  Uint32 noOfAttrs = 0, dataLen = 0;
  Uint32 * headers = signal->theData + 25;
  Uint32 * dst     = signal->theData + 25 + MAX_ATTRIBUTES_IN_TABLE;
  
  ptr[0].p  = headers;
  ptr[1].p  = dst;
  
  while(sz_1 > 0){
    jam();
    Uint32 tmp = * src_1 ++;
    * headers ++ = tmp;
    Uint32 len = AttributeHeader::getDataSize(tmp);
    memcpy(dst, src_1, 4 * len);
    dst += len;
    src_1 += len;
      
    noOfAttrs++;
    dataLen += len;
    sz_1 -= (1 + len);
  }
  assert(sz_1 == 0);
  
  ptr[0].sz = noOfAttrs;
  ptr[1].sz = dataLen;
  
  ptr[2].p = src_2;
  ptr[2].sz = sz_2;
  
  return sz_2 > 0 ? 3 : 2;
}

void
Suma::execFIRE_TRIG_ORD(Signal* signal)
{
  jamEntry();
  DBUG_ENTER("Suma::execFIRE_TRIG_ORD");
  ndbassert(signal->getNoOfSections() == 0);
  
  CRASH_INSERTION(13016);
  FireTrigOrd* const trg = (FireTrigOrd*)signal->getDataPtr();
  const Uint32 trigId    = trg->getTriggerId();
  const Uint32 hashValue = trg->getHashValue();
  const Uint32 gci_hi    = trg->getGCI();
  const Uint32 gci_lo    = trg->m_gci_lo;
  const Uint64 gci = gci_lo | (Uint64(gci_hi) << 32);
  const Uint32 event     = trg->getTriggerEvent();
  const Uint32 any_value = trg->getAnyValue();
  TablePtr tabPtr;
  tabPtr.i               = trigId & 0xFFFF;

  ndbassert(gci > m_last_complete_gci);

  DBUG_PRINT("enter",("tabPtr.i=%u", tabPtr.i));
  ndbrequire(f_bufferLock == trigId);
  /**
   * Reset f_bufferLock
   */
  f_bufferLock = 0;
  b_bufferLock = 0;
  
  ndbrequire((tabPtr.p = c_tablePool.getPtr(tabPtr.i)) != 0);
  Uint32 tableId = tabPtr.p->m_tableId;
  
  Uint32 bucket= hashValue % c_no_of_buckets;
  m_max_seen_gci = (gci > m_max_seen_gci ? gci : m_max_seen_gci);
  if(m_active_buckets.get(bucket) || 
     (m_switchover_buckets.get(bucket) && (check_switchover(bucket, gci))))
  {
    m_max_sent_gci = (gci > m_max_sent_gci ? gci : m_max_sent_gci);
    Uint32 sz = trg->getNoOfPrimaryKeyWords()+trg->getNoOfAfterValueWords();
    ndbrequire(sz == f_trigBufferSize);
    
    LinearSectionPtr ptr[3];
    const Uint32 nptr= reformat(signal, ptr, 
				f_buffer, sz, b_buffer, b_trigBufferSize);
    Uint32 ptrLen= 0;
    for(Uint32 i =0; i < nptr; i++)
      ptrLen+= ptr[i].sz;    
    /**
     * Signal to subscriber(s)
     */
    ndbrequire((tabPtr.p = c_tablePool.getPtr(tabPtr.i)) != 0);
    
    SubTableData * data = (SubTableData*)signal->getDataPtrSend();//trg;
    data->gci_hi         = gci_hi;
    data->gci_lo         = gci_lo;
    data->tableId        = tableId;
    data->requestInfo    = 0;
    SubTableData::setOperation(data->requestInfo, event);
    data->logType        = 0;
    data->anyValue       = any_value;
    data->totalLen       = ptrLen;
    
    {
      LocalDLList<Subscriber> list(c_subscriberPool,tabPtr.p->c_subscribers);
      SubscriberPtr subbPtr;
      for(list.first(subbPtr); !subbPtr.isNull(); list.next(subbPtr))
      {
	DBUG_PRINT("info",("GSN_SUB_TABLE_DATA to node %d",
			   refToNode(subbPtr.p->m_senderRef)));
	data->senderData = subbPtr.p->m_senderData;
	sendSignal(subbPtr.p->m_senderRef, GSN_SUB_TABLE_DATA, signal,
		   SubTableData::SignalLength, JBB, ptr, nptr);
      }
    }
  }
  else 
  {
    const uint buffer_header_sz = 4;
    Uint32* dst;
    Uint32 sz = f_trigBufferSize + b_trigBufferSize + buffer_header_sz;
    if((dst = get_buffer_ptr(signal, bucket, gci, sz)))
    {
      * dst++ = tableId;
      * dst++ = tabPtr.p->m_schemaVersion;
      * dst++ = (event << 16) | f_trigBufferSize;
      * dst++ = any_value;
      memcpy(dst, f_buffer, f_trigBufferSize << 2);
      dst += f_trigBufferSize;
      memcpy(dst, b_buffer, b_trigBufferSize << 2);
    }
  }
  
  DBUG_VOID_RETURN;
}

void
Suma::execSUB_GCP_COMPLETE_REP(Signal* signal)
{
  jamEntry();
  ndbassert(signal->getNoOfSections() == 0);

  SubGcpCompleteRep * rep = (SubGcpCompleteRep*)signal->getDataPtrSend();
  Uint32 gci_hi = rep->gci_hi;
  Uint32 gci_lo = rep->gci_lo;
  Uint64 gci = gci_lo | (Uint64(gci_hi) << 32);
  Uint32 flags = rep->flags;

#ifdef VM_TRACE
  if (m_gcp_monitor == 0)
  {
  }
  else if (gci_hi == Uint32(m_gcp_monitor >> 32))
  {
    ndbrequire(gci_lo == Uint32(m_gcp_monitor) + 1);
  }
  else
  {
    ndbrequire(gci_hi == Uint32(m_gcp_monitor >> 32) + 1);
    ndbrequire(gci_lo == 0);
  }
  m_gcp_monitor = gci;
#endif

  m_last_complete_gci = gci;
  m_max_seen_gci = (gci > m_max_seen_gci ? gci : m_max_seen_gci);

  /**
   * 
   */
  if(!m_switchover_buckets.isclear())
  {
    NdbNodeBitmask takeover_nodes;
    NdbNodeBitmask handover_nodes;
    Uint32 i = m_switchover_buckets.find(0);
    for(; i != Bucket_mask::NotFound; i = m_switchover_buckets.find(i + 1))
    {
      if(gci > c_buckets[i].m_switchover_gci)
      {
	Uint32 state = c_buckets[i].m_state;
	m_switchover_buckets.clear(i);
	printf("%u/%u (%u/%u) switchover complete bucket %d state: %x", 
	       Uint32(gci >> 32),
	       Uint32(gci),
	       Uint32(c_buckets[i].m_switchover_gci >> 32),
	       Uint32(c_buckets[i].m_switchover_gci),
	       i, state);

	if(state & Bucket::BUCKET_STARTING)
	{
	  /**
	   * NR case
	   */
	  m_active_buckets.set(i);
	  c_buckets[i].m_state &= ~(Uint32)Bucket::BUCKET_STARTING;
	  ndbout_c("starting");
	  m_gcp_complete_rep_count = 1;
	}
	else if(state & Bucket::BUCKET_TAKEOVER)
	{
	  /**
	   * NF case
	   */
	  Bucket* bucket= c_buckets + i;
	  Page_pos pos= bucket->m_buffer_head;
	  ndbrequire(pos.m_max_gci < gci);

	  Buffer_page* page= (Buffer_page*)
	    m_tup->c_page_pool.getPtr(pos.m_page_id);
	  ndbout_c("takeover %d", pos.m_page_id);
	  page->m_max_gci_hi = pos.m_max_gci >> 32;
          page->m_max_gci_lo = pos.m_max_gci & 0xFFFFFFFF;
          ndbassert(pos.m_max_gci != 0);
	  page->m_words_used = pos.m_page_pos;
	  page->m_next_page = RNIL;
	  memset(&bucket->m_buffer_head, 0, sizeof(bucket->m_buffer_head));
	  bucket->m_buffer_head.m_page_id = RNIL;
	  bucket->m_buffer_head.m_page_pos = Buffer_page::DATA_WORDS + 1;

	  m_active_buckets.set(i);
	  c_buckets[i].m_state &= ~(Uint32)Bucket::BUCKET_TAKEOVER;
	  takeover_nodes.set(c_buckets[i].m_switchover_node);
	}
	else
	{
	  /**
	   * NR, living node
	   */
	  ndbrequire(state & Bucket::BUCKET_HANDOVER);
	  c_buckets[i].m_state &= ~(Uint32)Bucket::BUCKET_HANDOVER;
	  handover_nodes.set(c_buckets[i].m_switchover_node);
	  ndbout_c("handover");
	}
      }
    }
    ndbassert(handover_nodes.count() == 0 || 
	      m_gcp_complete_rep_count > handover_nodes.count());
    m_gcp_complete_rep_count -= handover_nodes.count();
    m_gcp_complete_rep_count += takeover_nodes.count();

    if(getNodeState().startLevel == NodeState::SL_STARTING && 
       m_switchover_buckets.isclear() && 
       c_startup.m_handover_nodes.isclear())
    {
      sendSTTORRY(signal);
    }
  }

  if(ERROR_INSERTED(13010))
  {
    CLEAR_ERROR_INSERT_VALUE;
    ndbout_c("Don't send GCP_COMPLETE_REP(%llu)", gci);
    return;
  }

  /**
   * Signal to subscribers
   */
  rep->gci_hi = gci_hi;
  rep->gci_lo = gci_lo;
  rep->flags = flags;
  rep->senderRef  = reference();
  rep->gcp_complete_rep_count = m_gcp_complete_rep_count;
  
  if(m_gcp_complete_rep_count && !c_subscriber_nodes.isclear())
  {
    CRASH_INSERTION(13033);
    
    NodeReceiverGroup rg(API_CLUSTERMGR, c_subscriber_nodes);
    sendSignal(rg, GSN_SUB_GCP_COMPLETE_REP, signal,
	       SubGcpCompleteRep::SignalLength, JBB);
    
    Ptr<Gcp_record> gcp;
    if(c_gcp_list.seize(gcp))
    {
      gcp.p->m_gci = gci;
      gcp.p->m_subscribers = c_subscriber_nodes;
    }
  }
  
  /**
   * Add GCP COMPLETE REP to buffer
   */
  for(Uint32 i = 0; i<c_no_of_buckets; i++)
  {
    if(m_active_buckets.get(i))
      continue;

    if (!c_subscriber_nodes.isclear())
    {
      //Uint32* dst;
      get_buffer_ptr(signal, i, gci, 0);
    }
  }

  if(m_out_of_buffer_gci && gci > m_out_of_buffer_gci)
  {
    infoEvent("Reenable event buffer");
    m_out_of_buffer_gci = 0;
  }
}

void
Suma::execCREATE_TAB_CONF(Signal *signal)
{
  jamEntry();
  DBUG_ENTER("Suma::execCREATE_TAB_CONF");

#if 0
  CreateTabConf * const conf = (CreateTabConf*)signal->getDataPtr();
  Uint32 tableId = conf->senderData;

  TablePtr tabPtr;
  initTable(signal,tableId,tabPtr);
#endif
  DBUG_VOID_RETURN;
}

void
Suma::execDROP_TAB_CONF(Signal *signal)
{
  jamEntry();
  DBUG_ENTER("Suma::execDROP_TAB_CONF");
  ndbassert(signal->getNoOfSections() == 0);

  DropTabConf * const conf = (DropTabConf*)signal->getDataPtr();
  Uint32 senderRef= conf->senderRef;
  Uint32 tableId= conf->tableId;

  TablePtr tabPtr;
  if (!c_tables.find(tabPtr, tableId) ||
      tabPtr.p->m_state == Table::DROPPED ||
      tabPtr.p->m_state == Table::ALTERED)
  {
    DBUG_VOID_RETURN;
  }

  DBUG_PRINT("info",("drop table id: %d[i=%u]", tableId, tabPtr.i));

  tabPtr.p->m_state = Table::DROPPED;
  for (int j= 0; j < 3; j++)
  {
    if (!tabPtr.p->m_hasOutstandingTriggerReq[j])
    {
      tabPtr.p->m_hasTriggerDefined[j] = 0;
      tabPtr.p->m_hasOutstandingTriggerReq[j] = 0;
      tabPtr.p->m_triggerIds[j] = ILLEGAL_TRIGGER_ID;
    }
    else
      tabPtr.p->m_hasTriggerDefined[j] = 1;
  }
  if (senderRef == 0)
  {
    DBUG_VOID_RETURN;
  }
  // dict coordinator sends info to API
  
  const Uint64 gci = get_current_gci(signal);
  SubTableData * data = (SubTableData*)signal->getDataPtrSend();
  data->gci_hi         = Uint32(gci >> 32);
  data->gci_lo         = Uint32(gci);
  data->tableId        = tableId;
  data->requestInfo    = 0;
  SubTableData::setOperation(data->requestInfo,NdbDictionary::Event::_TE_DROP);
  SubTableData::setReqNodeId(data->requestInfo, refToNode(senderRef));
  
  {
    LocalDLList<Subscriber> subbs(c_subscriberPool,tabPtr.p->c_subscribers);
    SubscriberPtr subbPtr;
    for(subbs.first(subbPtr);!subbPtr.isNull();subbs.next(subbPtr))
    {
      jam();
      /*
       * get subscription ptr for this subscriber
       */
      SubscriptionPtr subPtr;
      c_subscriptions.getPtr(subPtr, subbPtr.p->m_subPtrI);
      if(subPtr.p->m_subscriptionType != SubCreateReq::TableEvent) {
	jam();
	continue;
	//continue in for-loop if the table is not part of 
	//the subscription. Otherwise, send data to subscriber.
      }
      data->senderData= subbPtr.p->m_senderData;
      sendSignal(subbPtr.p->m_senderRef, GSN_SUB_TABLE_DATA, signal,
		 SubTableData::SignalLength, JBB);
      DBUG_PRINT("info",("sent to subscriber %d", subbPtr.i));
    }
  }
  DBUG_VOID_RETURN;
}

static Uint32 b_dti_buf[MAX_WORDS_META_FILE];

void
Suma::execALTER_TAB_REQ(Signal *signal)
{
  jamEntry();
  DBUG_ENTER("Suma::execALTER_TAB_REQ");
  ndbassert(signal->getNoOfSections() == 1);

  AlterTabReq * const req = (AlterTabReq*)signal->getDataPtr();
  Uint32 senderRef= req->senderRef;
  Uint32 tableId= req->tableId;
  Uint32 changeMask= req->changeMask;
  TablePtr tabPtr;
  if (!c_tables.find(tabPtr, tableId) ||
      tabPtr.p->m_state == Table::DROPPED ||
      tabPtr.p->m_state == Table::ALTERED)
  {
    DBUG_VOID_RETURN;
  }

  DBUG_PRINT("info",("alter table id: %d[i=%u]", tableId, tabPtr.i));
  Table::State old_state = tabPtr.p->m_state;
  tabPtr.p->m_state = Table::ALTERED;
  // triggers must be removed, waiting for sub stop req for that

  if (senderRef == 0)
  {
    tabPtr.p->m_state = old_state;
    DBUG_VOID_RETURN;
  }
  // dict coordinator sends info to API

  // Copy DICT_TAB_INFO to local buffer
  SegmentedSectionPtr tabInfoPtr;
  signal->getSection(tabInfoPtr, AlterTabReq::DICT_TAB_INFO);
#ifndef DBUG_OFF
  ndbout_c("DICT_TAB_INFO in SUMA,  tabInfoPtr.sz = %d", tabInfoPtr.sz);
  SimplePropertiesSectionReader reader(tabInfoPtr, getSectionSegmentPool());
  reader.printAll(ndbout);
#endif
  copy(b_dti_buf, tabInfoPtr);
  LinearSectionPtr ptr[3];
  ptr[0].p = b_dti_buf;
  ptr[0].sz = tabInfoPtr.sz;

  releaseSections(signal);

  const Uint64 gci = get_current_gci(signal);
  SubTableData * data = (SubTableData*)signal->getDataPtrSend();
  data->gci_hi         = Uint32(gci >> 32);
  data->gci_lo         = Uint32(gci);
  data->tableId        = tableId;
  data->requestInfo    = 0;
  SubTableData::setOperation(data->requestInfo, 
			     NdbDictionary::Event::_TE_ALTER);
  SubTableData::setReqNodeId(data->requestInfo, refToNode(senderRef));
  data->logType        = 0;
  data->changeMask     = changeMask;
  data->totalLen       = tabInfoPtr.sz;
  {
    LocalDLList<Subscriber> subbs(c_subscriberPool,tabPtr.p->c_subscribers);
    SubscriberPtr subbPtr;
    for(subbs.first(subbPtr);!subbPtr.isNull();subbs.next(subbPtr))
    {
      jam();
      /*
       * get subscription ptr for this subscriber
       */
      SubscriptionPtr subPtr;
      c_subscriptions.getPtr(subPtr, subbPtr.p->m_subPtrI);
      if(subPtr.p->m_subscriptionType != SubCreateReq::TableEvent) {
	jam();
	continue;
	//continue in for-loop if the table is not part of 
	//the subscription. Otherwise, send data to subscriber.
      }

      data->senderData= subbPtr.p->m_senderData;
      Callback c = { 0, 0 };
      sendFragmentedSignal(subbPtr.p->m_senderRef, GSN_SUB_TABLE_DATA, signal,
                           SubTableData::SignalLength, JBB, ptr, 1, c);
      DBUG_PRINT("info",("sent to subscriber %d", subbPtr.i));
    }
  }
  tabPtr.p->m_state = old_state;
  DBUG_VOID_RETURN;
}

void
Suma::execSUB_GCP_COMPLETE_ACK(Signal* signal)
{
  jamEntry();
  ndbassert(signal->getNoOfSections() == 0);

  SubGcpCompleteAck * const ack = (SubGcpCompleteAck*)signal->getDataPtr();
  Uint32 gci_hi = ack->rep.gci_hi;
  Uint32 gci_lo = ack->rep.gci_lo;
  Uint32 senderRef  = ack->rep.senderRef;
  if (unlikely(signal->getLength() < SubGcpCompleteAck::SignalLength))
  {
    jam();
    ndbassert(!ndb_check_micro_gcp(getNodeInfo(refToNode(senderRef)).m_version));
    gci_lo = 0;
  }

  Uint64 gci = gci_lo | (Uint64(gci_hi) << 32);
  m_max_seen_gci = (gci > m_max_seen_gci ? gci : m_max_seen_gci);

  if (refToBlock(senderRef) == SUMA) {
    jam();
    // Ack from other SUMA
    Uint32 nodeId= refToNode(senderRef);
    for(Uint32 i = 0; i<c_no_of_buckets; i++)
    {
      if(m_active_buckets.get(i) || 
	 (m_switchover_buckets.get(i) && (check_switchover(i, gci))) ||
	 (!m_switchover_buckets.get(i) && get_responsible_node(i) == nodeId))
      {
	release_gci(signal, i, gci);
      }
    }
    return;
  }

  // Ack from User and not an ack from other SUMA, redistribute in nodegroup
  
  Uint32 nodeId = refToNode(senderRef);
  
  jam();
  Ptr<Gcp_record> gcp;
  for(c_gcp_list.first(gcp); !gcp.isNull(); c_gcp_list.next(gcp))
  {
    if(gcp.p->m_gci == gci)
    {
      gcp.p->m_subscribers.clear(nodeId);
      if(!gcp.p->m_subscribers.isclear())
      {
	jam();
	return;
      }
      break;
    }
  }
  
  if(gcp.isNull())
  {
    ndbout_c("ACK wo/ gcp record (gci: %u/%u)", 
	     Uint32(gci >> 32), Uint32(gci));
  }
  else
  {
    c_gcp_list.release(gcp);
  }
  
  CRASH_INSERTION(13011);
  if(ERROR_INSERTED(13012))
  {
    CLEAR_ERROR_INSERT_VALUE;
    ndbout_c("Don't redistribute SUB_GCP_COMPLETE_ACK");
    return;
  }
  
  ack->rep.senderRef = reference();  
  NodeReceiverGroup rg(SUMA, c_nodes_in_nodegroup_mask);
  sendSignal(rg, GSN_SUB_GCP_COMPLETE_ACK, signal,
	     SubGcpCompleteAck::SignalLength, JBB);
}

/**************************************************************
 *
 * Removing subscription
 *
 */

void
Suma::execSUB_REMOVE_REQ(Signal* signal)
{
  jamEntry();
  DBUG_ENTER("Suma::execSUB_REMOVE_REQ");
  ndbassert(signal->getNoOfSections() == 0);

  CRASH_INSERTION(13021);

  const SubRemoveReq req = *(SubRemoveReq*)signal->getDataPtr();
  SubscriptionPtr subPtr;
  Subscription key;
  key.m_subscriptionId  = req.subscriptionId;
  key.m_subscriptionKey = req.subscriptionKey;

  DBUG_PRINT("enter",("key.m_subscriptionId: %u, key.m_subscriptionKey: %u",
		      key.m_subscriptionId, key.m_subscriptionKey));

  if(!c_subscriptions.find(subPtr, key))
  {
    jam();
    DBUG_PRINT("info",("Not found"));
    sendSubRemoveRef(signal, req, 1407);
    DBUG_VOID_RETURN;
  }
  if (subPtr.p->m_state == Subscription::LOCKED)
  {
    /**
     * we are currently setting up triggers etc. for this event
     */
    jam();
    sendSubRemoveRef(signal, req, 1413);
    DBUG_VOID_RETURN;
  }
  if (subPtr.p->m_state == Subscription::DROPPED)
  {
    /**
     * already dropped
     */
    jam();
    sendSubRemoveRef(signal, req, 1419);
    DBUG_VOID_RETURN;
  }

  ndbrequire(subPtr.p->m_state == Subscription::DEFINED);
  DBUG_PRINT("info",("n_subscribers: %u", subPtr.p->n_subscribers));

  if (subPtr.p->n_subscribers == 0)
  {
    // no subscribers on the subscription
    // remove it
    jam();
    completeSubRemove(subPtr);
  }
  else
  {
    // subscribers left on the subscription
    // mark it to be removed once all subscribers
    // are removed
    jam();
    subPtr.p->m_state = Subscription::DROPPED;
  }

  SubRemoveConf * const conf = (SubRemoveConf*)signal->getDataPtrSend();
  conf->senderRef            = reference();
  conf->senderData           = req.senderData;
  conf->subscriptionId       = req.subscriptionId;
  conf->subscriptionKey      = req.subscriptionKey;

  sendSignal(req.senderRef, GSN_SUB_REMOVE_CONF, signal,
	     SubRemoveConf::SignalLength, JBB);

  DBUG_VOID_RETURN;
}

void
Suma::completeSubRemove(SubscriptionPtr subPtr)
{
  DBUG_ENTER("Suma::completeSubRemove");
  //Uint32 subscriptionId  = subPtr.p->m_subscriptionId;
  //Uint32 subscriptionKey = subPtr.p->m_subscriptionKey;

  c_subscriptions.release(subPtr);
  DBUG_PRINT("info",("c_subscriptionPool  size: %d free: %d",
		     c_subscriptionPool.getSize(),
		     c_subscriptionPool.getNoOfFree()));

  /**
   * I was the last subscription to be remove so clear c_tables
   */
#if 0
  ndbout_c("c_subscriptionPool.getSize() %d c_subscriptionPool.getNoOfFree()%d",
	   c_subscriptionPool.getSize(),c_subscriptionPool.getNoOfFree());
#endif

  if(c_subscriptionPool.getSize() == c_subscriptionPool.getNoOfFree()) {
    jam();
#if 0
    ndbout_c("SUB_REMOVE_REQ:Clearing c_tables");
#endif
    int count= 0;
    KeyTable<Table>::Iterator it;
    for(c_tables.first(it); !it.isNull(); )
    {
      // ndbrequire(false);
      
      DBUG_PRINT("error",("trailing table id: %d[i=%d] n_subscribers: %d m_state: %d",
			  it.curr.p->m_tableId,
			  it.curr.p->m_ptrI,
			  it.curr.p->n_subscribers,
			  it.curr.p->m_state));

      LocalDLList<Subscriber> subbs(c_subscriberPool,it.curr.p->c_subscribers);
      SubscriberPtr subbPtr;
      for(subbs.first(subbPtr);!subbPtr.isNull();subbs.next(subbPtr))
      {
	DBUG_PRINT("error",("subscriber %d, m_subPtrI: %d", subbPtr.i, subbPtr.p->m_subPtrI));
      }

      it.curr.p->release(* this);
      TablePtr tabPtr = it.curr;
      c_tables.next(it);
      c_tables.remove(tabPtr);
      c_tablePool.release(tabPtr);
      DBUG_PRINT("info",("c_tablePool  size: %d free: %d",
			 c_tablePool.getSize(),
			 c_tablePool.getNoOfFree()));
      count++;
    }
    DBUG_ASSERT(count == 0);
  }
  DBUG_VOID_RETURN;
}

void
Suma::sendSubRemoveRef(Signal* signal, const SubRemoveReq& req,
				  Uint32 errCode)
{
  jam();
  DBUG_ENTER("Suma::sendSubRemoveRef");
  SubRemoveRef  * ref = (SubRemoveRef *)signal->getDataPtrSend();
  ref->senderRef  = reference();
  ref->senderData = req.senderData;
  ref->subscriptionId = req.subscriptionId;
  ref->subscriptionKey = req.subscriptionKey;
  ref->errorCode = errCode;
  releaseSections(signal);
  sendSignal(signal->getSendersBlockRef(), GSN_SUB_REMOVE_REF, 
	     signal, SubRemoveRef::SignalLength, JBB);
  DBUG_VOID_RETURN;
}

void
Suma::Table::release(Suma & suma){
  jam();

  LocalDataBuffer<15> fragBuf(suma.c_dataBufferPool, m_fragments);
  fragBuf.release();

  m_state = UNDEFINED;
#ifndef DBUG_OFF
  if (n_subscribers != 0)
    abort();
#endif
}

void
Suma::SyncRecord::release(){
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

void
Suma::execSUMA_START_ME_REQ(Signal* signal) {
  jamEntry();
  DBUG_ENTER("Suma::execSUMA_START_ME");
  ndbassert(signal->getNoOfSections() == 0);
  Restart.runSUMA_START_ME_REQ(signal, signal->getSendersBlockRef());
  DBUG_VOID_RETURN;
}

void 
Suma::execSUB_CREATE_REF(Signal* signal) {
  jamEntry();
  DBUG_ENTER("Suma::execSUB_CREATE_REF");
  ndbassert(signal->getNoOfSections() == 0);
  SubCreateRef *const ref= (SubCreateRef *)signal->getDataPtr();
  Uint32 error= ref->errorCode;
  if (error != 1415)
  {
    /*
     * This will happen if an api node connects during while other node
     * is restarting, and in this case the subscription will already
     * have been created.
     * ToDo: more complete handling of api nodes joining during
     * node restart
     */
    Uint32 senderRef = signal->getSendersBlockRef();
    BlockReference cntrRef = calcNdbCntrBlockRef(refToNode(senderRef));
    // for some reason we did not manage to create a subscription
    // on the starting node
    SystemError * const sysErr = (SystemError*)&signal->theData[0];
    sysErr->errorCode = SystemError::CopySubscriptionRef;
    sysErr->errorRef = reference();
    sysErr->data[0] = error;
    sysErr->data[1] = 0;
    sendSignal(cntrRef, GSN_SYSTEM_ERROR, signal,
               SystemError::SignalLength, JBB);
    Restart.resetRestart(signal);
    DBUG_VOID_RETURN;
  }
  // SubCreateConf has same signaldata as SubCreateRef
  Restart.runSUB_CREATE_CONF(signal);
  DBUG_VOID_RETURN;
}

void 
Suma::execSUB_CREATE_CONF(Signal* signal)
{
  jamEntry();
  DBUG_ENTER("Suma::execSUB_CREATE_CONF");
  ndbassert(signal->getNoOfSections() == 0);
  Restart.runSUB_CREATE_CONF(signal);
  DBUG_VOID_RETURN;
}

void 
Suma::execSUB_START_CONF(Signal* signal)
{
  jamEntry();
  DBUG_ENTER("Suma::execSUB_START_CONF");
  ndbassert(signal->getNoOfSections() == 0);
  Restart.runSUB_START_CONF(signal);
  DBUG_VOID_RETURN;
}

void
Suma::execSUB_START_REF(Signal* signal) {
  jamEntry();
  DBUG_ENTER("Suma::execSUB_START_REF");
  ndbassert(signal->getNoOfSections() == 0);
  SubStartRef *const ref= (SubStartRef *)signal->getDataPtr();
  Uint32 error= ref->errorCode;
  {
    Uint32 senderRef = signal->getSendersBlockRef();
    BlockReference cntrRef = calcNdbCntrBlockRef(refToNode(senderRef));
    // for some reason we did not manage to start a subscriber
    // on the starting node
    SystemError * const sysErr = (SystemError*)&signal->theData[0];
    sysErr->errorCode = SystemError::CopySubscriberRef;
    sysErr->errorRef = reference();
    sysErr->data[0] = error;
    sysErr->data[1] = 0;
    sendSignal(cntrRef, GSN_SYSTEM_ERROR, signal,
               SystemError::SignalLength, JBB);
    Restart.resetRestart(signal);
  }
  DBUG_VOID_RETURN;
}

Suma::Restart::Restart(Suma& s) : suma(s)
{
  nodeId = 0;
}

void
Suma::Restart::runSUMA_START_ME_REQ(Signal* signal, Uint32 sumaRef)
{
  jam();
  DBUG_ENTER("Suma::Restart::runSUMA_START_ME");

  if(nodeId != 0)
  {
    SumaStartMeRef* ref= (SumaStartMeRef*)signal->getDataPtrSend();
    ref->errorCode = SumaStartMeRef::Busy;
    suma.sendSignal(sumaRef, GSN_SUMA_START_ME_REF, signal,
		    SumaStartMeRef::SignalLength, JBB);
    return;
  }

  nodeId = refToNode(sumaRef);
  startNode(signal, sumaRef);

  DBUG_VOID_RETURN;
}

void
Suma::Restart::startNode(Signal* signal, Uint32 sumaRef)
{
  jam();
  DBUG_ENTER("Suma::Restart::startNode");
  
  // right now we can only handle restarting one node
  // at a time in a node group
  
  createSubscription(signal, sumaRef);
  DBUG_VOID_RETURN;
}

void 
Suma::Restart::createSubscription(Signal* signal, Uint32 sumaRef)
{
  jam();
  DBUG_ENTER("Suma::Restart::createSubscription");
  suma.c_subscriptions.first(c_subIt);
  nextSubscription(signal, sumaRef);
  DBUG_VOID_RETURN;
}

void 
Suma::Restart::nextSubscription(Signal* signal, Uint32 sumaRef)
{
  jam();
  DBUG_ENTER("Suma::Restart::nextSubscription");

  if (c_subIt.isNull())
  {
    jam();
    completeSubscription(signal, sumaRef);
    DBUG_VOID_RETURN;
  }
  SubscriptionPtr subPtr;
  subPtr.i = c_subIt.curr.i;
  subPtr.p = suma.c_subscriptions.getPtr(subPtr.i);

  suma.c_subscriptions.next(c_subIt);

  SubCreateReq * req = (SubCreateReq *)signal->getDataPtrSend();
      
  req->senderRef        = suma.reference();
  req->senderData       = subPtr.i;
  req->subscriptionId   = subPtr.p->m_subscriptionId;
  req->subscriptionKey  = subPtr.p->m_subscriptionKey;
  req->subscriptionType = subPtr.p->m_subscriptionType |
    SubCreateReq::RestartFlag;

  switch (subPtr.p->m_subscriptionType) {
  case SubCreateReq::TableEvent:
    jam();
    req->tableId = subPtr.p->m_tableId;
    req->state = subPtr.p->m_state;
    suma.sendSignal(sumaRef, GSN_SUB_CREATE_REQ, signal,
		    SubCreateReq::SignalLength2, JBB);
    DBUG_VOID_RETURN;
  case SubCreateReq::SingleTableScan:
    jam();
    nextSubscription(signal, sumaRef);
    DBUG_VOID_RETURN;
  case SubCreateReq::SelectiveTableSnapshot:
  case SubCreateReq::DatabaseSnapshot:
    ndbrequire(false);
  }
  ndbrequire(false);
}

void
Suma::Restart::runSUB_CREATE_CONF(Signal* signal)
{
  jam();
  DBUG_ENTER("Suma::Restart::runSUB_CREATE_CONF");

  const Uint32 senderRef = signal->senderBlockRef();
  Uint32 sumaRef = signal->getSendersBlockRef();

  SubCreateConf * const conf = (SubCreateConf *)signal->getDataPtr();

  SubscriptionPtr subPtr;
  suma.c_subscriptions.getPtr(subPtr,conf->senderData);

  switch(subPtr.p->m_subscriptionType) {
  case SubCreateReq::TableEvent:
    if (1)
    {
      jam();
      nextSubscription(signal, sumaRef);
    } else {
      jam();
      SubCreateReq * req = (SubCreateReq *)signal->getDataPtrSend();
      
      req->senderRef        = suma.reference();
      req->senderData       = subPtr.i;
      req->subscriptionId   = subPtr.p->m_subscriptionId;
      req->subscriptionKey  = subPtr.p->m_subscriptionKey;
      req->subscriptionType = subPtr.p->m_subscriptionType |
	SubCreateReq::RestartFlag |
	SubCreateReq::AddTableFlag;

      req->tableId = 0;

      suma.sendSignal(senderRef, GSN_SUB_CREATE_REQ, signal,
		      SubCreateReq::SignalLength, JBB);
    }
    DBUG_VOID_RETURN;
  case SubCreateReq::SingleTableScan:
  case SubCreateReq::SelectiveTableSnapshot:
  case SubCreateReq::DatabaseSnapshot:
    ndbrequire(false);
  }
  ndbrequire(false);
}

void 
Suma::Restart::completeSubscription(Signal* signal, Uint32 sumaRef)
{
  jam();
  DBUG_ENTER("Suma::Restart::completeSubscription");
  startSubscriber(signal, sumaRef);
  DBUG_VOID_RETURN;
}

void 
Suma::Restart::startSubscriber(Signal* signal, Uint32 sumaRef)
{
  jam();
  DBUG_ENTER("Suma::Restart::startSubscriber");
  suma.c_tables.first(c_tabIt);
  if (c_tabIt.isNull())
  {
    completeSubscriber(signal, sumaRef);
    DBUG_VOID_RETURN;
  }
  SubscriberPtr subbPtr;
  {
    LocalDLList<Subscriber>
      subbs(suma.c_subscriberPool,c_tabIt.curr.p->c_subscribers);
    subbs.first(subbPtr);
  }
  nextSubscriber(signal, sumaRef, subbPtr);
  DBUG_VOID_RETURN;
}

void 
Suma::Restart::nextSubscriber(Signal* signal, Uint32 sumaRef,
			      SubscriberPtr subbPtr)
{
  jam();
  DBUG_ENTER("Suma::Restart::nextSubscriber");
  while (subbPtr.isNull())
  {
    jam();
    DBUG_PRINT("info",("prev tableId %u",c_tabIt.curr.p->m_tableId));
    suma.c_tables.next(c_tabIt);
    if (c_tabIt.isNull())
    {
      completeSubscriber(signal, sumaRef);
      DBUG_VOID_RETURN;
    }
    DBUG_PRINT("info",("next tableId %u",c_tabIt.curr.p->m_tableId));

    LocalDLList<Subscriber>
      subbs(suma.c_subscriberPool,c_tabIt.curr.p->c_subscribers);
    subbs.first(subbPtr);
  }

  /*
   * get subscription ptr for this subscriber
   */

  SubscriptionPtr subPtr;
  suma.c_subscriptions.getPtr(subPtr, subbPtr.p->m_subPtrI);
  switch (subPtr.p->m_subscriptionType) {
  case SubCreateReq::TableEvent:
    jam();
    sendSubStartReq(subPtr, subbPtr, signal, sumaRef);
    DBUG_VOID_RETURN;
  case SubCreateReq::SelectiveTableSnapshot:
  case SubCreateReq::DatabaseSnapshot:
  case SubCreateReq::SingleTableScan:
    ndbrequire(false);
  }
  ndbrequire(false);
}

void
Suma::Restart::sendSubStartReq(SubscriptionPtr subPtr, SubscriberPtr subbPtr,
			       Signal* signal, Uint32 sumaRef)
{
  jam();
  DBUG_ENTER("Suma::Restart::sendSubStartReq");
  SubStartReq * req = (SubStartReq *)signal->getDataPtrSend();

  req->senderRef        = suma.reference();
  req->senderData       = subbPtr.i;
  req->subscriptionId   = subPtr.p->m_subscriptionId;
  req->subscriptionKey  = subPtr.p->m_subscriptionKey;
  req->part             = SubscriptionData::TableData;
  req->subscriberData   = subbPtr.p->m_senderData;
  req->subscriberRef    = subbPtr.p->m_senderRef;

  // restarting suma will not respond to this until startphase 5
  // since it is not until then data copying has been completed
  DBUG_PRINT("info",("Restarting subscriber: %u on key: [%u,%u] %u",
		     subbPtr.i,
		     subPtr.p->m_subscriptionId,
		     subPtr.p->m_subscriptionKey,
		     subPtr.p->m_tableId));

  suma.sendSignal(sumaRef, GSN_SUB_START_REQ,
		  signal, SubStartReq::SignalLength2, JBB);
  DBUG_VOID_RETURN;
}

void 
Suma::Restart::runSUB_START_CONF(Signal* signal)
{
  jam();
  DBUG_ENTER("Suma::Restart::runSUB_START_CONF");

  SubStartConf * const conf = (SubStartConf*)signal->getDataPtr();

  Subscription key;
  SubscriptionPtr subPtr;
  key.m_subscriptionId  = conf->subscriptionId;
  key.m_subscriptionKey = conf->subscriptionKey;
  ndbrequire(suma.c_subscriptions.find(subPtr, key));

  TablePtr tabPtr;
  ndbrequire(suma.c_tables.find(tabPtr, subPtr.p->m_tableId));

  SubscriberPtr subbPtr;
  {
    LocalDLList<Subscriber>
      subbs(suma.c_subscriberPool,tabPtr.p->c_subscribers);
    subbs.getPtr(subbPtr, conf->senderData);
    DBUG_PRINT("info",("Restarted subscriber: %u on key: [%u,%u] table: %u",
		       subbPtr.i,key.m_subscriptionId,key.m_subscriptionKey,
		       subPtr.p->m_tableId));
    subbs.next(subbPtr);
  }

  Uint32 sumaRef = signal->getSendersBlockRef();
  nextSubscriber(signal, sumaRef, subbPtr);

  DBUG_VOID_RETURN;
}

void 
Suma::Restart::completeSubscriber(Signal* signal, Uint32 sumaRef)
{
  DBUG_ENTER("Suma::Restart::completeSubscriber");
  completeRestartingNode(signal, sumaRef);
  DBUG_VOID_RETURN;
}

void
Suma::Restart::completeRestartingNode(Signal* signal, Uint32 sumaRef)
{
  jam();
  DBUG_ENTER("Suma::Restart::completeRestartingNode");
  //SumaStartMeConf *conf= (SumaStartMeConf*)signal->getDataPtrSend();
  suma.sendSignal(sumaRef, GSN_SUMA_START_ME_CONF, signal,
		  SumaStartMeConf::SignalLength, JBB);
  resetRestart(signal);
  DBUG_VOID_RETURN;
}

void
Suma::Restart::resetRestart(Signal* signal)
{
  jam();
  DBUG_ENTER("Suma::Restart::resetRestart");
  nodeId = 0;
  DBUG_VOID_RETURN;
}

// only run on restarting suma

void
Suma::execSUMA_HANDOVER_REQ(Signal* signal)
{
  jamEntry();
  DBUG_ENTER("Suma::execSUMA_HANDOVER_REQ");
  //  Uint32 sumaRef = signal->getSendersBlockRef();
  SumaHandoverReq const * req = (SumaHandoverReq *)signal->getDataPtr();

  Uint32 gci = req->gci;
  Uint32 nodeId = req->nodeId;
  Uint32 new_gci = (m_last_complete_gci >> 32) + MAX_CONCURRENT_GCP + 1;
  
  Uint32 start_gci = (gci > new_gci ? gci : new_gci);
  // mark all active buckets really belonging to restarting SUMA

  Bucket_mask tmp;
  for( Uint32 i = 0; i < c_no_of_buckets; i++) 
  {
    if(get_responsible_node(i) == nodeId)
    {
      if (m_active_buckets.get(i))
      {
	// I'm running this bucket but it should really be the restarted node
	tmp.set(i);
	m_active_buckets.clear(i);
	m_switchover_buckets.set(i);
	c_buckets[i].m_switchover_gci = (Uint64(start_gci) << 32) - 1;
	c_buckets[i].m_state |= Bucket::BUCKET_HANDOVER;
	c_buckets[i].m_switchover_node = nodeId;
	ndbout_c("prepare to handover bucket: %d", i);
      }
      else if(m_switchover_buckets.get(i))
      {
	ndbout_c("dont handover bucket: %d %d", i, nodeId);
      }
    }
  }
  
  SumaHandoverConf* conf= (SumaHandoverConf*)signal->getDataPtrSend();
  tmp.copyto(BUCKET_MASK_SIZE, conf->theBucketMask);
  conf->gci = start_gci;
  conf->nodeId = getOwnNodeId();
  sendSignal(calcSumaBlockRef(nodeId), GSN_SUMA_HANDOVER_CONF, signal,
	     SumaHandoverConf::SignalLength, JBB);
  
  DBUG_VOID_RETURN;
}

// only run on all but restarting suma
void
Suma::execSUMA_HANDOVER_REF(Signal* signal) 
{
  ndbrequire(false);
}

void
Suma::execSUMA_HANDOVER_CONF(Signal* signal) {
  jamEntry();
  DBUG_ENTER("Suma::execSUMA_HANDOVER_CONF");

  SumaHandoverConf const * conf = (SumaHandoverConf *)signal->getDataPtr();

  Uint32 gci = conf->gci;
  Uint32 nodeId = conf->nodeId;
  Bucket_mask tmp;
  tmp.assign(BUCKET_MASK_SIZE, conf->theBucketMask);
#ifdef HANDOVER_DEBUG
  ndbout_c("Suma::execSUMA_HANDOVER_CONF, gci = %u", gci);
#endif

  for( Uint32 i = 0; i < c_no_of_buckets; i++) 
  {
    if (tmp.get(i))
    {
      ndbrequire(get_responsible_node(i) == getOwnNodeId());
      // We should run this bucket, but _nodeId_ is
      c_buckets[i].m_switchover_gci = (Uint64(gci) << 32) - 1;
      c_buckets[i].m_state |= Bucket::BUCKET_STARTING;
    }
  }
  
  char buf[255];
  tmp.getText(buf);
  infoEvent("Suma: handover from node %d gci: %d buckets: %s (%d)",
	    nodeId, gci, buf, c_no_of_buckets);
  m_switchover_buckets.bitOR(tmp);
  c_startup.m_handover_nodes.clear(nodeId);
  DBUG_VOID_RETURN;
}

#ifdef NOT_USED
static
NdbOut&
operator<<(NdbOut & out, const Suma::Page_pos & pos)
{
  out << "[ Page_pos:"
      << " m_page_id: " << pos.m_page_id
      << " m_page_pos: " << pos.m_page_pos
      << " m_max_gci: " << pos.m_max_gci
      << " ]";
  return out;
}
#endif

Uint32*
Suma::get_buffer_ptr(Signal* signal, Uint32 buck, Uint64 gci, Uint32 sz)
{
  sz += 1; // len
  Bucket* bucket= c_buckets+buck;
  Page_pos pos= bucket->m_buffer_head;

  Buffer_page* page = 0;
  Uint32 *ptr = 0;
  
  if (likely(pos.m_page_id != RNIL))
  {
    page= (Buffer_page*)m_tup->c_page_pool.getPtr(pos.m_page_id);
    ptr= page->m_data + pos.m_page_pos;
  }

  const bool same_gci = (gci == pos.m_last_gci) && (!ERROR_INSERTED(13022));
  
  pos.m_page_pos += sz;
  pos.m_last_gci = gci;
  Uint64 max = pos.m_max_gci > gci ? pos.m_max_gci : gci;
  
  if(likely(same_gci && pos.m_page_pos <= Buffer_page::DATA_WORDS))
  {
    pos.m_max_gci = max;
    bucket->m_buffer_head = pos;
    * ptr++ = (0x8000 << 16) | sz; // Same gci
    return ptr;
  }
  else if(pos.m_page_pos + Buffer_page::GCI_SZ32 <= Buffer_page::DATA_WORDS)
  {
loop:
    pos.m_max_gci = max;
    pos.m_page_pos += Buffer_page::GCI_SZ32;
    bucket->m_buffer_head = pos;
    * ptr++ = (sz + Buffer_page::GCI_SZ32);
    * ptr++ = gci >> 32;
    * ptr++ = gci & 0xFFFFFFFF;
    return ptr;
  }
  else
  {
    /**
     * new page
     * 1) save header on last page
     * 2) seize new page
     */
    Uint32 next;
    if(unlikely((next= seize_page()) == RNIL))
    {
      /**
       * Out of buffer
       */
      out_of_buffer(signal);
      return 0;
    }

    if(likely(pos.m_page_id != RNIL))
    {
      page->m_max_gci_hi = pos.m_max_gci >> 32;
      page->m_max_gci_lo = pos.m_max_gci & 0xFFFFFFFF;
      page->m_words_used = pos.m_page_pos - sz;
      page->m_next_page= next;
      ndbassert(pos.m_max_gci != 0);
    }
    else
    {
      bucket->m_buffer_tail = next;
    }
    
    memset(&pos, 0, sizeof(pos));
    pos.m_page_id = next;
    pos.m_page_pos = sz;
    pos.m_last_gci = gci;
    
    page= (Buffer_page*)m_tup->c_page_pool.getPtr(pos.m_page_id);
    page->m_next_page= RNIL;
    ptr= page->m_data;
    goto loop; //
  }
}

void
Suma::out_of_buffer(Signal* signal)
{
  if(m_out_of_buffer_gci)
  {
    return;
  }
  
  m_out_of_buffer_gci = m_last_complete_gci - 1;
  infoEvent("Out of event buffer: nodefailure will cause event failures");

  out_of_buffer_release(signal, 0);
}

void
Suma::out_of_buffer_release(Signal* signal, Uint32 buck)
{
  Bucket* bucket= c_buckets+buck;
  Uint32 tail= bucket->m_buffer_tail;
  
  if(tail != RNIL)
  {
    Buffer_page* page= (Buffer_page*)m_tup->c_page_pool.getPtr(tail);
    bucket->m_buffer_tail = page->m_next_page;
    free_page(tail, page);
    signal->theData[0] = SumaContinueB::OUT_OF_BUFFER_RELEASE;
    signal->theData[1] = buck;
    sendSignal(SUMA_REF, GSN_CONTINUEB, signal, 2, JBB);
    return;
  }

  /**
   * Clear head
   */
  bucket->m_buffer_head.m_page_id = RNIL;
  bucket->m_buffer_head.m_page_pos = Buffer_page::DATA_WORDS + 1;
  
  buck++;
  if(buck != c_no_of_buckets)
  {
    signal->theData[0] = SumaContinueB::OUT_OF_BUFFER_RELEASE;
    signal->theData[1] = buck;
    sendSignal(SUMA_REF, GSN_CONTINUEB, signal, 2, JBB);
    return;
  }

  /**
   * Finished will all release
   *   prepare for inclusion
   */
  m_out_of_buffer_gci = m_max_seen_gci > m_last_complete_gci 
    ? m_max_seen_gci : m_last_complete_gci;
}

Uint32
Suma::seize_page()
{
  if(unlikely(m_out_of_buffer_gci))
  {
    return RNIL;
  }
loop:
  Ptr<Page_chunk> ptr;
  Uint32 ref= m_first_free_page;
  if(likely(ref != RNIL))
  {
    m_first_free_page = ((Buffer_page*)m_tup->c_page_pool.getPtr(ref))->m_next_page;
    Uint32 chunk = ((Buffer_page*)m_tup->c_page_pool.getPtr(ref))->m_page_chunk_ptr_i;
    c_page_chunk_pool.getPtr(ptr, chunk);
    ndbassert(ptr.p->m_free);
    ptr.p->m_free--;
    return ref;
  }

  if(!c_page_chunk_pool.seize(ptr))
    return RNIL;

  Uint32 count;
  m_tup->allocConsPages(16, count, ref);
  if (count == 0)
    return RNIL;

  ndbout_c("alloc_chunk(%d %d) - ", ref, count);

  m_first_free_page = ptr.p->m_page_id = ref;
  ptr.p->m_size = count;
  ptr.p->m_free = count;

  Buffer_page* page;
  LINT_INIT(page);
  for(Uint32 i = 0; i<count; i++)
  {
    page = (Buffer_page*)m_tup->c_page_pool.getPtr(ref);
    page->m_page_state= SUMA_SEQUENCE;
    page->m_page_chunk_ptr_i = ptr.i;
    page->m_next_page = ++ref;
  }
  page->m_next_page = RNIL;
  
  goto loop;
}

void
Suma::free_page(Uint32 page_id, Buffer_page* page)
{
  Ptr<Page_chunk> ptr;
  ndbrequire(page->m_page_state == SUMA_SEQUENCE);

  Uint32 chunk= page->m_page_chunk_ptr_i;

  c_page_chunk_pool.getPtr(ptr, chunk);  
  
  ptr.p->m_free ++;
  page->m_next_page = m_first_free_page;
  ndbrequire(ptr.p->m_free <= ptr.p->m_size);
  
  m_first_free_page = page_id;
}

void
Suma::release_gci(Signal* signal, Uint32 buck, Uint64 gci)
{
  Bucket* bucket= c_buckets+buck;
  Uint32 tail= bucket->m_buffer_tail;
  Page_pos head= bucket->m_buffer_head;
  Uint64 max_acked = bucket->m_max_acked_gci;

  const Uint32 mask = Bucket::BUCKET_TAKEOVER | Bucket::BUCKET_RESEND;
  if(unlikely(bucket->m_state & mask))
  {
    jam();
    ndbout_c("release_gci(%d, %llu) -> node failure -> abort", buck, gci);
    return;
  }
  
  bucket->m_max_acked_gci = (max_acked > gci ? max_acked : gci);
  if(unlikely(tail == RNIL))
  {
    return;
  }
  
  if(tail == head.m_page_id)
  {
    if(gci >= head.m_max_gci)
    {
      jam();
      head.m_page_pos = 0;
      head.m_max_gci = gci;
      head.m_last_gci = 0;
      bucket->m_buffer_head = head;
    }
    return;
  }
  else
  {
    jam();
    Buffer_page* page= (Buffer_page*)m_tup->c_page_pool.getPtr(tail);
    Uint64 max_gci = page->m_max_gci_lo | (Uint64(page->m_max_gci_hi) << 32);
    Uint32 next_page = page->m_next_page;

    ndbassert(max_gci != 0);
    
    if(gci >= max_gci)
    {
      jam();
      free_page(tail, page);
      
      bucket->m_buffer_tail = next_page;
      signal->theData[0] = SumaContinueB::RELEASE_GCI;
      signal->theData[1] = buck;
      signal->theData[2] = gci >> 32;
      signal->theData[3] = gci & 0xFFFFFFFF;
      sendSignal(SUMA_REF, GSN_CONTINUEB, signal, 4, JBB);
      return;
    }
    else
    {
      //ndbout_c("do nothing...");
    }
  }
}

static Uint32 g_cnt = 0;

void
Suma::start_resend(Signal* signal, Uint32 buck)
{
  printf("start_resend(%d, ", buck);
  
  if(m_out_of_buffer_gci)
  {
    progError(__LINE__, NDBD_EXIT_SYSTEM_ERROR, 
	      "Nodefailure while out of event buffer");
    return;
  }
  
  /**
   * Resend from m_max_acked_gci + 1 until max_gci + 1
   */
  Bucket* bucket= c_buckets + buck;
  Page_pos pos= bucket->m_buffer_head;

  if(pos.m_page_id == RNIL)
  {
    jam();
    m_active_buckets.set(buck);
    m_gcp_complete_rep_count ++;
    ndbout_c("empty bucket(RNIL) -> active max_acked: %u/%u max_gci: %u/%u",
	     Uint32(bucket->m_max_acked_gci >> 32),
	     Uint32(bucket->m_max_acked_gci),
	     Uint32(pos.m_max_gci >> 32),
	     Uint32(pos.m_max_gci));
    return;
  }

  Uint64 min= bucket->m_max_acked_gci + 1;
  Uint64 max = pos.m_max_gci;

  ndbrequire(max <= m_max_seen_gci);

  if(min > max)
  {
    ndbrequire(pos.m_page_pos <= 2);
    ndbrequire(pos.m_page_id == bucket->m_buffer_tail);
    m_active_buckets.set(buck);
    m_gcp_complete_rep_count ++;
    ndbout_c("empty bucket -> active");
    return;
  }

  g_cnt = 0;
  bucket->m_state |= (Bucket::BUCKET_TAKEOVER | Bucket::BUCKET_RESEND);
  bucket->m_switchover_node = get_responsible_node(buck);
  bucket->m_switchover_gci = max;

  m_switchover_buckets.set(buck);
  
  signal->theData[0] = SumaContinueB::RESEND_BUCKET;
  signal->theData[1] = buck;
  signal->theData[2] = min >> 32;
  signal->theData[3] = 0;
  signal->theData[4] = 0;
  signal->theData[5] = min & 0xFFFFFFFF;
  signal->theData[6] = 0;
  sendSignal(reference(), GSN_CONTINUEB, signal, 7, JBB);
  
  ndbout_c("min: %u/%u - max: %u/%u) page: %d", 
	   Uint32(min >> 32), Uint32(min), Uint32(max >> 32), Uint32(max), 
	   bucket->m_buffer_tail);
  ndbrequire(max >= min);
}

void
Suma::resend_bucket(Signal* signal, Uint32 buck, Uint64 min_gci,
		    Uint32 pos, Uint64 last_gci)
{
  Bucket* bucket= c_buckets+buck;
  Uint32 tail= bucket->m_buffer_tail;

  Buffer_page* page= (Buffer_page*)m_tup->c_page_pool.getPtr(tail);
  Uint64 max_gci = page->m_max_gci_lo | (Uint64(page->m_max_gci_hi) << 32);
  Uint32 next_page = page->m_next_page;
  Uint32 *ptr = page->m_data + pos;
  Uint32 *end = page->m_data + page->m_words_used;
  bool delay = false;

  ndbrequire(tail != RNIL);

  if(tail == bucket->m_buffer_head.m_page_id)
  {
    max_gci= bucket->m_buffer_head.m_max_gci;
    end= page->m_data + bucket->m_buffer_head.m_page_pos;
    next_page= RNIL;

    if(ptr == end)
    {
      delay = true;
      goto next;
    }
  }
  else if(pos == 0 && min_gci > max_gci)
  {
    free_page(tail, page);
    tail = bucket->m_buffer_tail = next_page;
    ndbout_c("pos==0 && min_gci(%u/%u) > max_gci(%u/%u) resend switching page to %d", 
	     Uint32(min_gci >> 32), Uint32(min_gci), 
	     Uint32(max_gci >> 32), Uint32(max_gci), tail);
    goto next;
  }
  
#if 0
  for(Uint32 i = 0; i<page->m_words_used; i++)
  {
    printf("%.8x ", page->m_data[i]);
    if(((i + 1) % 8) == 0)
      printf("\n");
  }
  printf("\n");
#endif

  while(ptr < end)
  {
    Uint32 *src = ptr;
    Uint32 tmp = * src++;
    Uint32 sz = tmp & 0xFFFF;

    ptr += sz;

    if(! (tmp & (0x8000 << 16)))
    {
      ndbrequire(sz >= Buffer_page::GCI_SZ32);
      sz -= Buffer_page::GCI_SZ32;
      Uint32 last_gci_hi = * src++;
      Uint32 last_gci_lo = * src++;
      last_gci = last_gci_lo | (Uint64(last_gci_hi) << 32);
    }
    else
    {
      ndbrequire(ptr - sz > page->m_data);
    }

    if(last_gci < min_gci)
    {
      continue;
    }

    ndbrequire(sz);
    sz --; // remove *len* part of sz
    
    if(sz == 0)
    {
      SubGcpCompleteRep * rep = (SubGcpCompleteRep*)signal->getDataPtrSend();
      rep->gci_hi = last_gci >> 32;
      rep->gci_lo = last_gci & 0xFFFFFFFF;
      rep->flags = 0;
      rep->senderRef  = reference();
      rep->gcp_complete_rep_count = 1;
  
      char buf[255];
      c_subscriber_nodes.getText(buf);
      ndbout_c("resending GCI: %u/%u rows: %d -> %s", 
	       Uint32(last_gci >> 32), Uint32(last_gci), g_cnt, buf);
      g_cnt = 0;
      
      NodeReceiverGroup rg(API_CLUSTERMGR, c_subscriber_nodes);
      sendSignal(rg, GSN_SUB_GCP_COMPLETE_REP, signal,
		 SubGcpCompleteRep::SignalLength, JBB);
    } 
    else
    {
      const uint buffer_header_sz = 4;
      g_cnt++;
      Uint32 table = * src++ ;
      Uint32 schemaVersion = * src++;
      Uint32 event = * src >> 16;
      Uint32 sz_1 = (* src ++) & 0xFFFF;
      Uint32 any_value = * src++;

      ndbassert(sz - buffer_header_sz >= sz_1);
      
      LinearSectionPtr ptr[3];
      const Uint32 nptr= reformat(signal, ptr, 
				  src, sz_1, 
				  src + sz_1, sz - buffer_header_sz - sz_1);
      Uint32 ptrLen= 0;
      for(Uint32 i =0; i < nptr; i++)
        ptrLen+= ptr[i].sz;

      /**
       * Signal to subscriber(s)
       */
      Ptr<Table> tabPtr;
      if (c_tables.find(tabPtr, table) && 
          table_version_major(tabPtr.p->m_schemaVersion) ==
          table_version_major(schemaVersion))
      {
	SubTableData * data = (SubTableData*)signal->getDataPtrSend();//trg;
	data->gci_hi         = last_gci >> 32;
	data->gci_lo         = last_gci & 0xFFFFFFFF;
	data->tableId        = table;
	data->requestInfo    = 0;
	SubTableData::setOperation(data->requestInfo, event);
	data->logType        = 0;
	data->anyValue       = any_value;
	data->totalLen       = ptrLen;
	
	{
	  LocalDLList<Subscriber> 
	    list(c_subscriberPool,tabPtr.p->c_subscribers);
	  SubscriberPtr subbPtr;
	  for(list.first(subbPtr); !subbPtr.isNull(); list.next(subbPtr))
	  {
	    DBUG_PRINT("info",("GSN_SUB_TABLE_DATA to node %d",
			       refToNode(subbPtr.p->m_senderRef)));
	    data->senderData = subbPtr.p->m_senderData;
	    sendSignal(subbPtr.p->m_senderRef, GSN_SUB_TABLE_DATA, signal,
		       SubTableData::SignalLength, JBB, ptr, nptr);
	  }
	}
      }
    }
    
    break;
  }
  
  if(ptr == end && (tail != bucket->m_buffer_head.m_page_id))
  {
    /**
     * release...
     */
    free_page(tail, page);
    tail = bucket->m_buffer_tail = next_page;
    pos = 0;
    last_gci = 0;
    ndbout_c("ptr == end -> resend switching page to %d", tail);
  }
  else
  {
    pos = (ptr - page->m_data);
  }
  
next:
  if(tail == RNIL)
  {
    bucket->m_state &= ~(Uint32)Bucket::BUCKET_RESEND;
    ndbassert(! (bucket->m_state & Bucket::BUCKET_TAKEOVER));
    ndbout_c("resend done...");
    return;
  }
  
  signal->theData[0] = SumaContinueB::RESEND_BUCKET;
  signal->theData[1] = buck;
  signal->theData[2] = min_gci >> 32;
  signal->theData[3] = pos;
  signal->theData[4] = last_gci >> 32;
  signal->theData[5] = min_gci & 0xFFFFFFFF;
  signal->theData[6] = last_gci & 0xFFFFFFFF;
  if(!delay)
    sendSignal(SUMA_REF, GSN_CONTINUEB, signal, 7, JBB);
  else
    sendSignalWithDelay(SUMA_REF, GSN_CONTINUEB, signal, 10, 7);
}

Uint64
Suma::get_current_gci(Signal* signal)
{
  signal->theData[0] = 0; // user ptr
  signal->theData[1] = 0; // Execute direct
  signal->theData[2] = 1; // Current
  EXECUTE_DIRECT(DBDIH, GSN_GETGCIREQ, signal, 3);

  jamEntry();
  Uint32 gci_hi = signal->theData[1];
  Uint32 gci_lo = signal->theData[2];

  Uint64 gci = gci_lo | (Uint64(gci_hi) << 32);
  return gci;
}

template void append(DataBuffer<11>&,SegmentedSectionPtr,SectionSegmentPool&);


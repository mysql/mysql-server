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

#include <my_global.h>
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
#include <signaldata/CreateTrigImpl.hpp>
#include <signaldata/DropTrigImpl.hpp>
#include <signaldata/FireTrigOrd.hpp>
#include <signaldata/TrigAttrInfo.hpp>
#include <signaldata/CheckNodeGroups.hpp>
#include <signaldata/CreateTab.hpp>
#include <signaldata/DropTab.hpp>
#include <signaldata/AlterTable.hpp>
#include <signaldata/AlterTab.hpp>
#include <signaldata/DihScanTab.hpp>
#include <signaldata/SystemError.hpp>
#include <signaldata/GCP.hpp>

#include <signaldata/DictLock.hpp>
#include <ndbapi/NdbDictionary.hpp>

#include <DebuggerNames.hpp>
#include <../dbtup/Dbtup.hpp>
#include <../dbdih/Dbdih.hpp>

#include <EventLogger.hpp>
extern EventLogger * g_eventLogger;

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
  Uint32 noTables, noAttrs, maxBufferedEpochs;
  ndb_mgm_get_int_parameter(p, CFG_DB_NO_TABLES,  
			    &noTables);
  ndb_mgm_get_int_parameter(p, CFG_DB_NO_ATTRIBUTES,  
			    &noAttrs);
  ndb_mgm_get_int_parameter(p, CFG_DB_MAX_BUFFERED_EPOCHS,
                            &maxBufferedEpochs);

  c_tablePool.setSize(noTables);
  c_tables.setSize(noTables);
  
  c_subscriptions.setSize(noTables);

  Uint32 cnt = 0;
  cnt = 0;
  ndb_mgm_get_int_parameter(p, CFG_DB_SUBSCRIPTIONS, &cnt);
  if (cnt == 0)
  {
    jam();
    cnt = noTables;
  }
  c_subscriptionPool.setSize(cnt);

  cnt *= 2;
  {
    Uint32 val = 0;
    ndb_mgm_get_int_parameter(p, CFG_DB_SUBSCRIBERS, &val);
    if (val)
    {
      jam();
      cnt =  val;
    }
  }
  c_subscriberPool.setSize(cnt);

  cnt = 0;
  ndb_mgm_get_int_parameter(p, CFG_DB_SUB_OPERATIONS, &cnt);
  if (cnt)
    c_subOpPool.setSize(cnt);
  else
    c_subOpPool.setSize(256);
  
  c_syncPool.setSize(2);
  c_dataBufferPool.setSize(noAttrs);

  c_maxBufferedEpochs = maxBufferedEpochs;

  // Calculate needed gcp pool as 10 records + the ones needed
  // during a possible api timeout
  Uint32 dbApiHbInterval, gcpInterval, microGcpInterval = 0;
  ndb_mgm_get_int_parameter(p, CFG_DB_API_HEARTBEAT_INTERVAL,
			    &dbApiHbInterval);
  ndb_mgm_get_int_parameter(p, CFG_DB_GCP_INTERVAL,
                            &gcpInterval);
  ndb_mgm_get_int_parameter(p, CFG_DB_MICRO_GCP_INTERVAL,
                            &microGcpInterval);

  if (microGcpInterval)
  {
    gcpInterval = microGcpInterval;
  }
  c_gcp_pool.setSize(10 + (4*dbApiHbInterval+gcpInterval-1)/gcpInterval);
  
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
  m_missing_data = false;

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
  m_startphase  = signal->theData[1];
  m_typeOfStart = signal->theData[7];

  DBUG_PRINT("info",("startphase = %u, typeOfStart = %u",
		     m_startphase, m_typeOfStart));

  if(m_startphase == 3)
  {
    jam();
    ndbrequire((m_tup = (Dbtup*)globalData.getBlock(DBTUP)) != 0);
  }

  if(m_startphase == 5)
  {
    jam();

    if (ERROR_INSERTED(13029)) /* Hold startphase 5 */
    {
      sendSignalWithDelay(SUMA_REF, GSN_STTOR, signal,
                          30, signal->getLength());
      DBUG_VOID_RETURN;
    }
    
    signal->theData[0] = reference();
    sendSignal(NDBCNTR_REF, GSN_READ_NODESREQ, signal, 1, JBB);
    DBUG_VOID_RETURN;
  }
  
  if(m_startphase == 7)
  {
    if (m_typeOfStart != NodeState::ST_NODE_RESTART &&
	m_typeOfStart != NodeState::ST_INITIAL_NODE_RESTART)
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
    
    if(m_typeOfStart == NodeState::ST_INITIAL_START &&
       c_masterNodeId == getOwnNodeId())
    {
      jam();
      createSequence(signal);
      DBUG_VOID_RETURN;
    }//if
    
    if (ERROR_INSERTED(13030))
    {
      ndbout_c("Dont start handover");
      DBUG_VOID_RETURN;
    }
  }//if
  
  if(m_startphase == 100)
  {
    /**
     * Allow API's to connect
     */
    sendSTTORRY(signal);
    DBUG_VOID_RETURN;
  }

  if(m_startphase == 101)
  {
    if (m_typeOfStart == NodeState::ST_NODE_RESTART ||
	m_typeOfStart == NodeState::ST_INITIAL_NODE_RESTART)
    {
      /**
       * Handover code here
       */
      c_startup.m_wait_handover= true;
      check_start_handover(signal);
      DBUG_VOID_RETURN;
    }
  }
  sendSTTORRY(signal);
  
  DBUG_VOID_RETURN;
}

#include <ndb_version.h>

void
Suma::send_dict_lock_req(Signal* signal)
{
  if (ndbd_suma_dictlock(getNodeInfo(c_masterNodeId).m_version))
  {
    jam();
    DictLockReq* req = (DictLockReq*)signal->getDataPtrSend();
    req->lockType = DictLockReq::SumaStartMe;
    req->userPtr = 0;
    req->userRef = reference();
    sendSignal(calcDictBlockRef(c_masterNodeId),
               GSN_DICT_LOCK_REQ, signal, DictLockReq::SignalLength, JBB);
  }
  else
  {
    jam();
    c_startup.m_restart_server_node_id = 0;
    send_start_me_req(signal);
  }
}

void
Suma::execDICT_LOCK_CONF(Signal* signal)
{
  jamEntry();
  c_startup.m_restart_server_node_id = 0;

  CRASH_INSERTION(13039);
  send_start_me_req(signal);
}

void
Suma::execDICT_LOCK_REF(Signal* signal)
{
  jamEntry();

  DictLockRef* ref = (DictLockRef*)signal->getDataPtr();

  ndbrequire(ref->errorCode == DictLockRef::TooManyRequests);
  signal->theData[0] = SumaContinueB::RETRY_DICT_LOCK;
  sendSignalWithDelay(reference(), GSN_CONTINUEB, signal, 300, 1);
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

  Uint32 error = ref->errorCode;
  if (error != SumaStartMeRef::Busy && error != SumaStartMeRef::NotStarted)
  {
    jam();
    // for some reason we did not manage to create a subscription
    // on the starting node
    SystemError * const sysErr = (SystemError*)&signal->theData[0];
    sysErr->errorCode = SystemError::CopySubscriptionRef;
    sysErr->errorRef = reference();
    sysErr->data[0] = error;
    sysErr->data[1] = 0;
    sendSignal(NDBCNTR_REF, GSN_SYSTEM_ERROR, signal,
               SystemError::SignalLength, JBB);
    return;
  }

  infoEvent("Suma: node %d refused %d", 
	    c_startup.m_restart_server_node_id, ref->errorCode);

  send_start_me_req(signal);
}

void
Suma::execSUMA_START_ME_CONF(Signal* signal)
{
  infoEvent("Suma: node %d has completed restoring me", 
	    c_startup.m_restart_server_node_id);
  sendSTTORRY(signal);  

  if (ndbd_suma_dictlock(getNodeInfo(c_masterNodeId).m_version))
  {
    jam();
    DictUnlockOrd* ord = (DictUnlockOrd*)signal->getDataPtrSend();
    ord->lockPtr = 0;
    ord->lockType = DictLockReq::SumaStartMe;
    ord->senderData = 0;
    ord->senderRef = reference();
    sendSignal(calcDictBlockRef(c_masterNodeId),
               GSN_DICT_UNLOCK_ORD, signal, DictUnlockOrd::SignalLength, JBB);
  }
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
        BaseString::snprintf(buf, sizeof(buf),
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
  
  getNodeGroupMembers(signal);
}

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
  sd->requestType = CheckNodeGroups::GetNodeGroupMembers;
  sd->nodeId = getOwnNodeId();
  sd->senderData = RNIL;
  sendSignal(DBDIH_REF, GSN_CHECKNODEGROUPSREQ, signal,
             CheckNodeGroups::SignalLength, JBB);
  DBUG_VOID_RETURN;
}

void
Suma::execCHECKNODEGROUPSCONF(Signal *signal)
{
  const CheckNodeGroups *sd = (const CheckNodeGroups *)signal->getDataPtrSend();
  DBUG_ENTER("Suma::execCHECKNODEGROUPSCONF");
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

  c_startup.m_restart_server_node_id = 0;    
  if (m_typeOfStart == NodeState::ST_NODE_RESTART ||
      m_typeOfStart == NodeState::ST_INITIAL_NODE_RESTART)
  {
    jam();
    
    send_dict_lock_req(signal);

    return;
  }

  c_startup.m_restart_server_node_id = 0;    
  sendSTTORRY(signal);

  DBUG_VOID_RETURN;
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
  case SumaContinueB::API_FAIL_GCI_LIST:
    api_fail_gci_list(signal, signal->theData[1]);
    return;
  case SumaContinueB::API_FAIL_SUBSCRIBER_LIST:
    api_fail_subscriber_list(signal,
                             signal->theData[1]);
    return;
  case SumaContinueB::API_FAIL_SUBSCRIPTION:
    api_fail_subscription(signal);
    return;
  case SumaContinueB::SUB_STOP_REQ:
    sub_stop_req(signal);
    return;
  case SumaContinueB::RETRY_DICT_LOCK:
    jam();
    send_dict_lock_req(signal);
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
  BlockReference retRef = signal->theData[1];

  c_connected_nodes.clear(failedApiNode);

  if (c_failedApiNodes.get(failedApiNode))
  {
    jam();
    goto CONF;
  }

  if (!c_subscriber_nodes.get(failedApiNode))
  {
    jam();
    goto CONF;
  }

  c_failedApiNodes.set(failedApiNode);
  c_subscriber_nodes.clear(failedApiNode);
  c_subscriber_per_node[failedApiNode] = 0;
  
  check_start_handover(signal);

  signal->theData[0] = SumaContinueB::API_FAIL_GCI_LIST;
  signal->theData[1] = failedApiNode;
  sendSignal(SUMA_REF, GSN_CONTINUEB, signal, 2, JBB);
  return;

CONF:
  signal->theData[0] = failedApiNode;
  signal->theData[1] = reference();
  sendSignal(retRef, GSN_API_FAILCONF, signal, 2, JBB);

  DBUG_VOID_RETURN;
}//execAPI_FAILREQ()

void
Suma::api_fail_gci_list(Signal* signal, Uint32 nodeId)
{
  jam();

  Ptr<Gcp_record> gcp;
  if (c_gcp_list.first(gcp))
  {
    jam();
    gcp.p->m_subscribers.bitAND(c_subscriber_nodes);

    if (gcp.p->m_subscribers.isclear())
    {
      jam();

      SubGcpCompleteAck* ack = (SubGcpCompleteAck*)signal->getDataPtrSend();
      ack->rep.gci_hi = Uint32(gcp.p->m_gci >> 32);
      ack->rep.gci_lo = Uint32(gcp.p->m_gci);
      ack->rep.senderRef = reference();
      NodeReceiverGroup rg(SUMA, c_nodes_in_nodegroup_mask);
      sendSignal(rg, GSN_SUB_GCP_COMPLETE_ACK, signal,
                 SubGcpCompleteAck::SignalLength, JBB);

      c_gcp_list.release(gcp);

      signal->theData[0] = SumaContinueB::API_FAIL_GCI_LIST;
      signal->theData[1] = nodeId;
      sendSignal(SUMA_REF, GSN_CONTINUEB, signal, 2, JBB);
      return;
    }
  }

  if (ERROR_INSERTED(13023))
  {
    CLEAR_ERROR_INSERT_VALUE;
  }

  signal->theData[0] = SumaContinueB::API_FAIL_SUBSCRIBER_LIST;
  signal->theData[1] = nodeId;
  signal->theData[2] = RNIL; // SubOpPtr
  signal->theData[3] = RNIL; // c_subscribers bucket
  signal->theData[4] = RNIL; // subscriptionId
  signal->theData[5] = RNIL; // SubscriptionKey

  Ptr<SubOpRecord> subOpPtr;
  if (c_subOpPool.seize(subOpPtr))
  {
    signal->theData[2] = subOpPtr.i;
    sendSignal(SUMA_REF, GSN_CONTINUEB, signal, 6, JBB);
  }
  else
  {
    sendSignal(SUMA_REF, GSN_CONTINUEB, signal, 3, JBB);
  }

  return;
}

void
Suma::api_fail_subscriber_list(Signal* signal, Uint32 nodeId)
{
  jam();
  Ptr<SubOpRecord> subOpPtr;
  subOpPtr.i = signal->theData[2];
  if (subOpPtr.i == RNIL)
  {
    if (c_subOpPool.seize(subOpPtr))
    {
      signal->theData[3] = RNIL;
    }
    else
    {
      jam();
      sendSignal(SUMA_REF, GSN_CONTINUEB, signal, 3, JBB);
      return;
    }
  }
  else
  {
    jam();
    c_subOpPool.getPtr(subOpPtr);
  }

  Uint32 bucket = signal->theData[3];
  Uint32 subscriptionId = signal->theData[4];
  Uint32 subscriptionKey = signal->theData[5];

  DLHashTable<Subscription>::Iterator iter;
  if (bucket == RNIL)
  {
    jam();
    c_subscriptions.first(iter);
  }
  else
  {
    jam();

    Subscription key;
    key.m_subscriptionId = subscriptionId;
    key.m_subscriptionKey = subscriptionKey;
    if (c_subscriptions.find(iter.curr, key) == false)
    {
      jam();
      /**
       * We restart from this bucket :-(
       */
      c_subscriptions.next(bucket, iter);
    }
    else
    {
      iter.bucket = bucket;
    }
  }

  if (iter.curr.isNull())
  {
    jam();
    signal->theData[0] = nodeId;
    signal->theData[1] = reference();
    sendSignal(QMGR_REF, GSN_API_FAILCONF, signal, 2, JBB);
    c_failedApiNodes.clear(nodeId);
    return;
  }

  subOpPtr.p->m_opType = SubOpRecord::R_API_FAIL_REQ;
  subOpPtr.p->m_subPtrI = iter.curr.i;
  subOpPtr.p->m_senderRef = nodeId;
  subOpPtr.p->m_senderData = iter.bucket;

  LocalDLFifoList<SubOpRecord> list(c_subOpPool, iter.curr.p->m_stop_req);
  bool empty = list.isEmpty();
  list.add(subOpPtr);

  if (empty)
  {
    signal->theData[0] = SumaContinueB::API_FAIL_SUBSCRIPTION;
    signal->theData[1] = subOpPtr.i;
    signal->theData[2] = RNIL;
    sendSignal(SUMA_REF, GSN_CONTINUEB, signal, 3, JBB);
  }
}

void
Suma::api_fail_subscription(Signal* signal)
{
  jam();
  Ptr<SubOpRecord> subOpPtr;
  c_subOpPool.getPtr(subOpPtr, signal->theData[1]);

  Uint32 nodeId = subOpPtr.p->m_senderRef;

  Ptr<Subscription> subPtr;
  c_subscriptionPool.getPtr(subPtr, subOpPtr.p->m_subPtrI);

  Ptr<Subscriber> ptr;
  {
    LocalDLList<Subscriber> list(c_subscriberPool, subPtr.p->m_subscribers);
    if (signal->theData[2] == RNIL)
    {
      jam();
      list.first(ptr);
    }
    else
    {
      jam();
      list.getPtr(ptr, signal->theData[2]);
    }

    for (Uint32 i = 0; i<32 && !ptr.isNull(); i++)
    {
      jam();
      if (refToNode(ptr.p->m_senderRef) == nodeId)
      {
        jam();

        Ptr<Subscriber> tmp = ptr;
        list.next(ptr);
        list.remove(tmp);
        
        /**
         * NOTE: remove before...so we done send UNSUBSCRIBE to self (yuck)
         */
        bool report = subPtr.p->m_options & Subscription::REPORT_SUBSCRIBE;

        send_sub_start_stop_event(signal, tmp, NdbDictionary::Event::_TE_STOP,
                                  report, list);
        
        c_subscriberPool.release(tmp);
      }
      else
      {
        jam();
        list.next(ptr);
      }
    }
  }

  if (!ptr.isNull())
  {
    jam();
    signal->theData[0] = SumaContinueB::API_FAIL_SUBSCRIPTION;
    signal->theData[1] = subOpPtr.i;
    signal->theData[2] = ptr.i;
    sendSignal(SUMA_REF, GSN_CONTINUEB, signal, 3, JBB);
    return;
  }

  // Start potential waiter(s)
  check_remove_queue(signal, subPtr, subOpPtr, true, false);
  check_release_subscription(signal, subPtr);

  // Continue iterating through subscriptions
  DLHashTable<Subscription>::Iterator iter;
  iter.bucket = subOpPtr.p->m_senderData;
  iter.curr = subPtr;

  if (c_subscriptions.next(iter))
  {
    signal->theData[0] = SumaContinueB::API_FAIL_SUBSCRIBER_LIST;
    signal->theData[1] = nodeId;
    signal->theData[2] = subOpPtr.i;
    signal->theData[3] = iter.bucket;
    signal->theData[4] = iter.curr.p->m_subscriptionId; // subscriptionId
    signal->theData[5] = iter.curr.p->m_subscriptionKey; // SubscriptionKey
    sendSignal(SUMA_REF, GSN_CONTINUEB, signal, 6, JBB);
    return;
  }

  c_subOpPool.release(subOpPtr);
  signal->theData[0] = nodeId;
  signal->theData[1] = reference();
  sendSignal(QMGR_REF, GSN_API_FAILCONF, signal, 2, JBB);
  c_failedApiNodes.clear(nodeId);
}

void
Suma::execNODE_FAILREP(Signal* signal){
  jamEntry();
  DBUG_ENTER("Suma::execNODE_FAILREP");
  ndbassert(signal->getNoOfSections() == 0);

  const NodeFailRep * rep = (NodeFailRep*)signal->getDataPtr();
  NdbNodeBitmask failed; failed.assign(NdbNodeBitmask::Size, rep->theNodes);
  
  if(c_restart.m_ref && failed.get(refToNode(c_restart.m_ref)))
  {
    jam();

    if (c_restart.m_waiting_on_self)
    {
      jam();
      c_restart.m_abort = 1;
    }
    else
    {
      jam();
      Ptr<Subscription> subPtr;
      c_subscriptionPool.getPtr(subPtr, c_restart.m_subPtrI);
      abort_start_me(signal, subPtr, false);
    }
  }

  if (ERROR_INSERTED(13032))
  {
    Uint32 node = c_subscriber_nodes.find(0);
    if (node != NodeBitmask::NotFound)
    {
      ndbout_c("Inserting API_FAILREQ node: %u", node);
      signal->theData[0] = node;
      sendSignal(QMGR_REF, GSN_API_FAILREQ, signal, 1, JBA);
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
  
  signal->theData[0] = nodeId;
  signal->theData[1] = reference();
  sendSignal(senderRef, GSN_INCL_NODECONF, signal, 2, JBB);
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

    infoEvent("Suma: c_subOpPool  size: %d free: %d",
	      c_subOpPool.getSize(),
	      c_subOpPool.getNoOfFree());

#if 0
    infoEvent("Suma: c_dataSubscribers count: %d",
	      count_subscribers(c_dataSubscribers));
    infoEvent("Suma: c_prepDataSubscribers count: %d",
	      count_subscribers(c_prepDataSubscribers));
#endif
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

  if (tCase == 8011)
  {
    jam();
    Uint32 bucket = signal->theData[1];
    KeyTable<Table>::Iterator it;
    if (signal->getLength() == 1)
    {
      jam();
      bucket = 0;
      infoEvent("-- Starting dump of subscribers --");
    }

    c_tables.next(bucket, it);
    const Uint32 RT_BREAK = 16;
    for(Uint32 i = 0; i<RT_BREAK || it.bucket == bucket; i++)
    {
      jam();
      if(it.curr.i == RNIL)
      {
        jam();
        infoEvent("-- Ending dump of subscribers --");        
        return;
      }

      infoEvent("Table %u ver %u",
                it.curr.p->m_tableId,
                it.curr.p->m_schemaVersion);

      Uint32 cnt = 0;
      Ptr<Subscription> subPtr;
      LocalDLList<Subscription> subList(c_subscriptionPool,
                                        it.curr.p->m_subscriptions);
      for(subList.first(subPtr); !subPtr.isNull(); subList.next(subPtr))
      {
        infoEvent(" Subcription %u", subPtr.i);
        {
          Ptr<Subscriber> ptr;
          LocalDLList<Subscriber> list(c_subscriberPool,
                                       subPtr.p->m_subscribers);
          for (list.first(ptr); !ptr.isNull(); list.next(ptr), i++)
          {
            jam();
            cnt++;
            infoEvent("  Subscriber [ %x %u %u ]",
                      ptr.p->m_senderRef,
                      ptr.p->m_senderData,
                      subPtr.i);
          }
        }

        {
          Ptr<SubOpRecord> ptr;
          LocalDLFifoList<SubOpRecord> list(c_subOpPool,
                                       subPtr.p->m_create_req);

          for (list.first(ptr); !ptr.isNull(); list.next(ptr), i++)
          {
            jam();
            infoEvent("  create [ %x %u ]",
                      ptr.p->m_senderRef,
                      ptr.p->m_senderData);
          }
        }

        {
          Ptr<SubOpRecord> ptr;
          LocalDLFifoList<SubOpRecord> list(c_subOpPool,
                                       subPtr.p->m_start_req);

          for (list.first(ptr); !ptr.isNull(); list.next(ptr), i++)
          {
            jam();
            infoEvent("  start [ %x %u ]",
                      ptr.p->m_senderRef,
                      ptr.p->m_senderData);
          }
        }

        {
          Ptr<SubOpRecord> ptr;
          LocalDLFifoList<SubOpRecord> list(c_subOpPool,
                                        subPtr.p->m_stop_req);

          for (list.first(ptr); !ptr.isNull(); list.next(ptr), i++)
          {
            jam();
            infoEvent("  stop [ %u %x %u ]",
                      ptr.p->m_opType,
                      ptr.p->m_senderRef,
                      ptr.p->m_senderData);
          }
        }
      }
      infoEvent("Table %u #subscribers %u", it.curr.p->m_tableId, cnt);
      c_tables.next(it);
    }

    signal->theData[0] = tCase;
    signal->theData[1] = it.bucket;
    sendSignalWithDelay(reference(), GSN_DUMP_STATE_ORD, signal, 100, 2);
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
  
  DBUG_VOID_RETURN;
}

/**********************************************************
 * Suma participant interface
 *
 * Creation of subscriptions
 */
void
Suma::execSUB_CREATE_REQ(Signal* signal)
{
  jamEntry();                            
  DBUG_ENTER("Suma::execSUB_CREATE_REQ");
  ndbassert(signal->getNoOfSections() == 0);
  CRASH_INSERTION(13003);

  const SubCreateReq req = *(SubCreateReq*)signal->getDataPtr();    
  
  const Uint32 senderRef  = req.senderRef;
  const Uint32 senderData = req.senderData;
  const Uint32 subId   = req.subscriptionId;
  const Uint32 subKey  = req.subscriptionKey;
  const Uint32 type    = req.subscriptionType & SubCreateReq::RemoveFlags;
  const Uint32 flags   = req.subscriptionType & SubCreateReq::GetFlags;
  const Uint32 reportAll = (flags & SubCreateReq::ReportAll) ?
    Subscription::REPORT_ALL : 0;
  const Uint32 reportSubscribe = (flags & SubCreateReq::ReportSubscribe) ?
    Subscription::REPORT_SUBSCRIBE : 0;
  const Uint32 tableId = req.tableId;
  const Uint32 schemaTransId = req.schemaTransId;

  bool subDropped = req.subscriptionType & SubCreateReq::NR_Sub_Dropped;

  /**
   * This 2 options are only allowed during NR
   */
  if (subDropped)
  {
    ndbrequire(refToNode(senderRef) == c_startup.m_restart_server_node_id);
  }

  Subscription key;
  key.m_subscriptionId  = subId;
  key.m_subscriptionKey = subKey;

  DBUG_PRINT("enter",("key.m_subscriptionId: %u, key.m_subscriptionKey: %u",
		      key.m_subscriptionId, key.m_subscriptionKey));

  SubscriptionPtr subPtr;

  bool found = c_subscriptions.find(subPtr, key);

  if (c_startup.m_restart_server_node_id == RNIL)
  {
    jam();

    /**
     * We havent started syncing yet
     */
    sendSubCreateRef(signal, senderRef, senderData,
                     SubCreateRef::NF_FakeErrorREF);
    return;
  }

  CRASH_INSERTION2(13040, c_startup.m_restart_server_node_id != RNIL);
  CRASH_INSERTION(13041);
  
  bool allowDup = true; //c_startup.m_restart_server_node_id;

  if (found && !allowDup)
  {
    jam();
    sendSubCreateRef(signal, senderRef, senderData,
                     SubCreateRef::SubscriptionAlreadyExist);
    return;
  }

  if (found == false)
  {
    jam();
    if(!c_subscriptions.seize(subPtr))
    {
      jam();
      sendSubCreateRef(signal, senderRef, senderData,
                       SubCreateRef::OutOfSubscriptionRecords);
      return;
    }

    new (subPtr.p) Subscription();
    subPtr.p->m_seq_no           = c_current_seq;
    subPtr.p->m_subscriptionId   = subId;
    subPtr.p->m_subscriptionKey  = subKey;
    subPtr.p->m_subscriptionType = type;
    subPtr.p->m_tableId          = tableId;
    subPtr.p->m_table_ptrI       = RNIL;
    subPtr.p->m_state            = Subscription::UNDEFINED;
    subPtr.p->m_trigger_state    =  Subscription::T_UNDEFINED;
    subPtr.p->m_triggers[0]      = ILLEGAL_TRIGGER_ID;
    subPtr.p->m_triggers[1]      = ILLEGAL_TRIGGER_ID;
    subPtr.p->m_triggers[2]      = ILLEGAL_TRIGGER_ID;
    subPtr.p->m_errorCode        = 0;
    subPtr.p->m_options          = reportSubscribe | reportAll;
    subPtr.p->m_schemaTransId    = schemaTransId;
  }

  Ptr<SubOpRecord> subOpPtr;
  LocalDLFifoList<SubOpRecord> subOpList(c_subOpPool, subPtr.p->m_create_req);
  if (subOpList.seize(subOpPtr) == false)
  {
    jam();
    if (found == false)
    {
      jam();
      c_subscriptions.release(subPtr);
    }
    sendSubCreateRef(signal, senderRef, senderData,
                     SubCreateRef::OutOfTableRecords);
    return;
  }

  subOpPtr.p->m_senderRef = senderRef;
  subOpPtr.p->m_senderData = senderData;

  if (subDropped)
  {
    jam();
    subPtr.p->m_options |= Subscription::MARKED_DROPPED;
  }

  TablePtr tabPtr;
  if (found)
  {
    jam();
    c_tablePool.getPtr(tabPtr, subPtr.p->m_table_ptrI);
  }
  else if (c_tables.find(tabPtr, tableId))
  {
    jam();
  }
  else
  {
    jam();
    if (c_tablePool.seize(tabPtr) == false)
    {
      jam();
      subOpList.release(subOpPtr);
      c_subscriptions.release(subPtr);
      sendSubCreateRef(signal, senderRef, senderData,
                       SubCreateRef::OutOfTableRecords);
      return;
    }

    new (tabPtr.p) Table;
    tabPtr.p->m_tableId= tableId;
    tabPtr.p->m_ptrI= tabPtr.i;
    tabPtr.p->m_error = 0;
    tabPtr.p->m_schemaVersion = RNIL;
    tabPtr.p->m_state = Table::UNDEFINED;
    tabPtr.p->m_schemaTransId = schemaTransId;
    c_tables.add(tabPtr);
  }

  if (found == false)
  {
    jam();
    c_subscriptions.add(subPtr);
    LocalDLList<Subscription> list(c_subscriptionPool,
                                   tabPtr.p->m_subscriptions);
    list.add(subPtr);
    subPtr.p->m_table_ptrI = tabPtr.i;
  }

  switch(tabPtr.p->m_state){
  case Table::DEFINED:{
    jam();
    // Send conf
    subOpList.release(subOpPtr);
    subPtr.p->m_state = Subscription::DEFINED;
    SubCreateConf * const conf = (SubCreateConf*)signal->getDataPtrSend();
    conf->senderRef  = reference();
    conf->senderData = senderData;
    sendSignal(senderRef, GSN_SUB_CREATE_CONF, signal,
               SubCreateConf::SignalLength, JBB);
    return;
  }
  case Table::UNDEFINED:{
    jam();
    tabPtr.p->m_state = Table::DEFINING;
    subPtr.p->m_state = Subscription::DEFINING;

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
      return;
    }

    GetTabInfoReq * req = (GetTabInfoReq *)signal->getDataPtrSend();
    req->senderRef = reference();
    req->senderData = tabPtr.i;
    req->requestType =
      GetTabInfoReq::RequestById | GetTabInfoReq::LongSignalConf;
    req->tableId = tableId;
    req->schemaTransId = schemaTransId;

    sendSignal(DBDICT_REF, GSN_GET_TABINFOREQ, signal,
               GetTabInfoReq::SignalLength, JBB);
    return;
  }
  case Table::DEFINING:
  {
    jam();
    /**
     * just wait for completion
     */
    subPtr.p->m_state = Subscription::DEFINING;
    return;
  }
  case Table::DROPPED:
  {
    subOpList.release(subOpPtr);

    {
      LocalDLList<Subscription> list(c_subscriptionPool,
                                     tabPtr.p->m_subscriptions);
      list.remove(subPtr);
    }
    c_subscriptions.release(subPtr);

    sendSubCreateRef(signal, senderRef, senderData,
                     SubCreateRef::TableDropped);
    return;
  }
  }

  ndbrequire(false);
}

void
Suma::sendSubCreateRef(Signal* signal, Uint32 retRef, Uint32 data,
                       Uint32 errCode)
{
  jam();
  SubCreateRef * ref = (SubCreateRef *)signal->getDataPtrSend();
  ref->errorCode  = errCode;
  ref->senderData = data;
  sendSignal(retRef, GSN_SUB_CREATE_REF, signal,
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

  CRASH_INSERTION(13004);

  SubSyncReq * const req = (SubSyncReq*)signal->getDataPtr();

  SubscriptionPtr subPtr;
  Subscription key; 
  key.m_subscriptionId = req->subscriptionId;
  key.m_subscriptionKey = req->subscriptionKey;

  SectionHandle handle(this, signal);
  if(!c_subscriptions.find(subPtr, key))
  {
    jam();
    releaseSections(handle);
    sendSubSyncRef(signal, 1407);
    return;
  }

  Ptr<SyncRecord> syncPtr;
  LocalDLList<SyncRecord> list(c_syncPool, subPtr.p->m_syncRecords);
  if(!list.seize(syncPtr))
  {
    jam();
    releaseSections(handle);
    sendSubSyncRef(signal, 1416);
    return;
  }
  
  new (syncPtr.p) Ptr<SyncRecord>;
  syncPtr.p->m_senderRef        = req->senderRef;
  syncPtr.p->m_senderData       = req->senderData;
  syncPtr.p->m_subscriptionPtrI = subPtr.i;
  syncPtr.p->ptrI               = syncPtr.i;
  syncPtr.p->m_error            = 0;
  syncPtr.p->m_requestInfo      = req->requestInfo;
  syncPtr.p->m_frag_cnt         = req->fragCount;
  syncPtr.p->m_tableId          = subPtr.p->m_tableId;

  {
    jam();
    if(handle.m_cnt > 0)
    {
      SegmentedSectionPtr ptr;
      handle.getSection(ptr, SubSyncReq::ATTRIBUTE_LIST);
      LocalDataBuffer<15> attrBuf(c_dataBufferPool, syncPtr.p->m_attributeList);
      append(attrBuf, ptr, getSectionSegmentPool());
      releaseSections(handle);
    }
  }

  /**
   * We need to gather fragment info
   */
  {
    jam();
    DihScanTabReq* req = (DihScanTabReq*)signal->getDataPtrSend();
    req->senderRef = reference();
    req->senderData = syncPtr.i;
    req->tableId = subPtr.p->m_tableId;
    req->schemaTransId = subPtr.p->m_schemaTransId;
    sendSignal(DBDIH_REF, GSN_DIH_SCAN_TAB_REQ, signal,
               DihScanTabReq::SignalLength, JBB);
  }
}

void
Suma::sendSubSyncRef(Signal* signal, Uint32 errCode){
  jam();
  SubSyncRef * ref= (SubSyncRef *)signal->getDataPtrSend();
  ref->errorCode = errCode;
  sendSignal(signal->getSendersBlockRef(), 
	     GSN_SUB_SYNC_REF, 
	     signal, 
	     SubSyncRef::SignalLength,
	     JBB);
  return;
}

void
Suma::execDIH_SCAN_TAB_REF(Signal* signal)
{
  jamEntry();
  DBUG_ENTER("Suma::execDI_FCOUNTREF");
  DihScanTabRef * ref = (DihScanTabRef*)signal->getDataPtr();
  switch ((DihScanTabRef::ErrorCode) ref->error)
  {
  case DihScanTabRef::ErroneousTableState:
    jam();
    if (ref->tableStatus == Dbdih::TabRecord::TS_CREATING)
    {
      const Uint32 tableId = ref->tableId;
      const Uint32 synPtrI = ref->senderData;
      const Uint32 schemaTransId = ref->schemaTransId;
      DihScanTabReq * req = (DihScanTabReq*)signal->getDataPtrSend();

      req->senderData = synPtrI;
      req->senderRef = reference();
      req->tableId = tableId;
      req->schemaTransId = schemaTransId;
      sendSignalWithDelay(DBDIH_REF, GSN_DIH_SCAN_TAB_REQ, signal,
                          DihScanTabReq::SignalLength,
                          DihScanTabReq::RetryInterval);
      DBUG_VOID_RETURN;
    }
    ndbrequire(false);
  default:
    ndbrequire(false);
  }

  DBUG_VOID_RETURN;
}

void
Suma::execDIH_SCAN_TAB_CONF(Signal* signal)
{
  jamEntry();
  DBUG_ENTER("Suma::execDI_FCOUNTCONF");
  ndbassert(signal->getNoOfSections() == 0);
  DihScanTabConf * conf = (DihScanTabConf*)signal->getDataPtr();
  const Uint32 tableId = conf->tableId;
  const Uint32 fragCount = conf->fragmentCount;
  const Uint32 scanCookie = conf->scanCookie;

  Ptr<SyncRecord> ptr;
  c_syncPool.getPtr(ptr, conf->senderData);

  LocalDataBuffer<15> fragBuf(c_dataBufferPool, ptr.p->m_fragments);
  ndbrequire(fragBuf.getSize() == 0);

  ndbassert(fragCount >= ptr.p->m_frag_cnt);
  if (ptr.p->m_frag_cnt == 0)
  {
    jam();
    ptr.p->m_frag_cnt = fragCount;
  }
  ptr.p->m_scan_cookie = scanCookie;

  DihScanGetNodesReq* req = (DihScanGetNodesReq*)signal->getDataPtrSend();
  req->senderRef = reference();
  req->senderData = ptr.i;
  req->tableId = tableId;
  req->fragId = 0;
  req->scanCookie = scanCookie;
  sendSignal(DBDIH_REF, GSN_DIH_SCAN_GET_NODES_REQ, signal,
             DihScanGetNodesReq::SignalLength, JBB);

  DBUG_VOID_RETURN;
}

void
Suma::execDIH_SCAN_GET_NODES_CONF(Signal* signal)
{
  jamEntry();
  DBUG_ENTER("Suma::execDIGETPRIMCONF");
  ndbassert(signal->getNoOfSections() == 0);

  DihScanGetNodesConf* conf = (DihScanGetNodesConf*)signal->getDataPtr();
  const Uint32 nodeCount = conf->count;
  const Uint32 tableId = conf->tableId;
  const Uint32 fragNo = conf->fragId;

  ndbrequire(nodeCount > 0 && nodeCount <= MAX_REPLICAS);

  Ptr<SyncRecord> ptr;
  c_syncPool.getPtr(ptr, conf->senderData);

  {
    LocalDataBuffer<15> fragBuf(c_dataBufferPool, ptr.p->m_fragments);

    /**
     * Add primary node for fragment to list
     */
    FragmentDescriptor fd;
    fd.m_fragDesc.m_nodeId = conf->nodes[0];
    fd.m_fragDesc.m_fragmentNo = fragNo;
    signal->theData[2] = fd.m_dummy;
    fragBuf.append(&signal->theData[2], 1);
  }

  const Uint32 nextFrag = fragNo + 1;
  if(nextFrag == ptr.p->m_frag_cnt)
  {
    jam();

    ptr.p->startScan(signal);
    return;
  }

  DihScanGetNodesReq* req = (DihScanGetNodesReq*)signal->getDataPtrSend();
  req->senderRef = reference();
  req->senderData = ptr.i;
  req->tableId = tableId;
  req->fragId = nextFrag;
  req->scanCookie = ptr.p->m_scan_cookie;
  sendSignal(DBDIH_REF, GSN_DIH_SCAN_GET_NODES_REQ, signal,
             DihScanGetNodesReq::SignalLength, JBB);

  DBUG_VOID_RETURN;
}

/**********************************************************
 * Dict interface
 */

/*************************************************************************
 *
 *
 */
void
Suma::execGET_TABINFOREF(Signal* signal){
  jamEntry();
  GetTabInfoRef* ref = (GetTabInfoRef*)signal->getDataPtr();
  Uint32 tableId = ref->tableId;
  Uint32 senderData = ref->senderData;
  Uint32 schemaTransId = ref->schemaTransId;
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
  case GetTabInfoRef::NoFetchByName:
    jam();
  case GetTabInfoRef::TableNameTooLong:
    jam();
    ndbrequire(false);
  }
  if (do_resend_request)
  {
    GetTabInfoReq * req = (GetTabInfoReq *)signal->getDataPtrSend();
    req->senderRef = reference();
    req->senderData = senderData;
    req->requestType =
      GetTabInfoReq::RequestById | GetTabInfoReq::LongSignalConf;
    req->tableId = tableId;
    req->schemaTransId = schemaTransId;
    sendSignalWithDelay(DBDICT_REF, GSN_GET_TABINFOREQ, signal,
                        30, GetTabInfoReq::SignalLength);
    return;
  }

  LocalDLList<Subscription> subList(c_subscriptionPool,
                                    tabPtr.p->m_subscriptions);
  Ptr<Subscription> subPtr;
  bool empty = subList.isEmpty();
  for(subList.first(subPtr); !subPtr.isNull();)
  {
    jam();
    Ptr<SubOpRecord> ptr;
    LocalDLFifoList<SubOpRecord> list(c_subOpPool, subPtr.p->m_create_req);
    for (list.first(ptr); !ptr.isNull(); )
    {
      jam();
      sendSubCreateRef(signal,
                       ptr.p->m_senderRef,
                       ptr.p->m_senderData,
                       SubCreateRef::TableDropped);

      Ptr<SubOpRecord> tmp0 = ptr;
      list.next(ptr);
      list.release(tmp0);
    }
    Ptr<Subscription> tmp1 = subPtr;
    subList.next(subPtr);
    subList.release(tmp1);
  }

  c_tables.release(tabPtr);
  ndbassert(!empty);
}

void
Suma::execGET_TABINFO_CONF(Signal* signal){
  jamEntry();

  CRASH_INSERTION(13006);

  if(!assembleFragments(signal)){
    return;
  }
  
  SectionHandle handle(this, signal);
  GetTabInfoConf* conf = (GetTabInfoConf*)signal->getDataPtr();
  TablePtr tabPtr;
  c_tablePool.getPtr(tabPtr, conf->senderData);
  SegmentedSectionPtr ptr;
  handle.getSection(ptr, GetTabInfoConf::DICT_TAB_INFO);
  ndbrequire(tabPtr.p->parseTable(ptr, *this));
  releaseSections(handle);

  tabPtr.p->m_state = Table::DEFINED;

  LocalDLList<Subscription> subList(c_subscriptionPool,
                                    tabPtr.p->m_subscriptions);
  Ptr<Subscription> subPtr;
  bool empty = subList.isEmpty();
  for(subList.first(subPtr); !subPtr.isNull(); subList.next(subPtr))
  {
    jam();
    subPtr.p->m_state = Subscription::DEFINED;

    Ptr<SubOpRecord> ptr;
    LocalDLFifoList<SubOpRecord> list(c_subOpPool, subPtr.p->m_create_req);
    for (list.first(ptr); !ptr.isNull();)
    {
      jam();
      SubCreateConf * const conf = (SubCreateConf*)signal->getDataPtrSend();
      conf->senderRef  = reference();
      conf->senderData = ptr.p->m_senderData;
      sendSignal(ptr.p->m_senderRef, GSN_SUB_CREATE_CONF, signal,
                 SubCreateConf::SignalLength, JBB);

      Ptr<SubOpRecord> tmp = ptr;
      list.next(ptr);
      list.release(tmp);
    }
  }

  ndbassert(!empty);
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

  jamBlock(&suma);
  suma.suma_ndbrequire(s == SimpleProperties::Break);

  /**
   * Initialize table object
   */
  m_noOfAttributes = tableDesc.NoOfAttributes;
  m_schemaVersion = tableDesc.TableVersion;
  
  DBUG_RETURN(true);
}

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
  DataBuffer<15>::DataBufferIterator fragIt;
  
  TablePtr tabPtr;
  suma.c_tablePool.getPtr(tabPtr, subPtr.p->m_table_ptrI);
  LocalDataBuffer<15> fragBuf(suma.c_dataBufferPool,  m_fragments);
    
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
  if (m_requestInfo & SubSyncReq::LM_Exclusive)
  {
    ScanFragReq::setLockMode(req->requestInfo, 1);
    ScanFragReq::setHoldLockFlag(req->requestInfo, 1);
    ScanFragReq::setKeyinfoFlag(req->requestInfo, 1);
  }

  if (m_requestInfo & SubSyncReq::Reorg)
  {
    ScanFragReq::setReorgFlag(req->requestInfo, ScanFragReq::REORG_MOVED);
  }

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
    req->senderData = senderData;
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
  Uint32 syncPtrI = conf->senderData;

  ndbrequire(c_subscriptions.find(subPtr, key));

  ScanFragNextReq * req = (ScanFragNextReq *)signal->getDataPtrSend();
  req->senderData = syncPtrI;
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

  SubscriptionPtr subPtr;
  suma.c_subscriptionPool.getPtr(subPtr, m_subscriptionPtrI);

  DihScanTabCompleteRep* rep = (DihScanTabCompleteRep*)signal->getDataPtr();
  rep->tableId = subPtr.p->m_tableId;
  rep->scanCookie = m_scan_cookie;
  suma.sendSignal(DBDIH_REF, GSN_DIH_SCAN_TAB_COMPLETE_REP, signal,
                  DihScanTabCompleteRep::SignalLength, JBB);

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
  LocalDLList<SyncRecord> list(suma.c_syncPool, subPtr.p->m_syncRecords);
  Ptr<SyncRecord> tmp;
  tmp.i = ptrI;
  tmp.p = this;
  list.release(tmp);
  
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
  //SubscriptionData::Part part = (SubscriptionData::Part)req->part;

  Subscription key; 
  key.m_subscriptionId        = req->subscriptionId;
  key.m_subscriptionKey       = req->subscriptionKey;

  SubscriptionPtr subPtr;

  if (c_startup.m_restart_server_node_id == RNIL)
  {
    jam();

    /**
     * We havent started syncing yet
     */
    sendSubStartRef(signal,
                    senderRef, senderData, SubStartRef::NF_FakeErrorREF);
    return;
  }

  CRASH_INSERTION2(13042, getNodeState().startLevel == NodeState::SL_STARTING);
  
  bool found = c_subscriptions.find(subPtr, key);
  if (!found)
  {
    jam();
    sendSubStartRef(signal,
                    senderRef, senderData, SubStartRef::NoSuchSubscription);
    return;
  }
  
  switch(subPtr.p->m_state){
  case Subscription::UNDEFINED:
    jam();
    ndbrequire(false);
  case Subscription::DEFINING:
    jam();
    sendSubStartRef(signal,
                    senderRef, senderData, SubStartRef::Defining);
    return;
  case Subscription::DEFINED:
    break;
  }

  if (subPtr.p->m_options & Subscription::MARKED_DROPPED)
  {
    jam();
    if (c_startup.m_restart_server_node_id == 0)
    {
      sendSubStartRef(signal,
                      senderRef, senderData, SubStartRef::Dropped);
      return;
    }
    else
    {
      /**
       * Allow SUB_START_REQ from peer node
       */
    }
  }

  if (subPtr.p->m_trigger_state == Subscription::T_ERROR)
  {
    jam();
    sendSubStartRef(signal,
                    senderRef, senderData, subPtr.p->m_errorCode);
    return;
  }
  
  SubscriberPtr subbPtr;
  if(!c_subscriberPool.seize(subbPtr))
  {
    jam();
    sendSubStartRef(signal,
                    senderRef, senderData, SubStartRef::OutOfSubscriberRecords);
    return;
  }

  Ptr<SubOpRecord> subOpPtr;
  if (!c_subOpPool.seize(subOpPtr))
  {
    jam();
    c_subscriberPool.release(subbPtr);
    sendSubStartRef(signal,
                    senderRef, senderData, SubStartRef::OutOfSubOpRecords);
    return;
  }

  if (! check_sub_start(subscriberRef))
  {
    jam();
    c_subscriberPool.release(subbPtr);
    c_subOpPool.release(subOpPtr);
    sendSubStartRef(signal,
                    senderRef, senderData, SubStartRef::NodeDied);
    return;
  }
  
  // setup subscriber record
  subbPtr.p->m_senderRef  = subscriberRef;
  subbPtr.p->m_senderData = subscriberData;

  subOpPtr.p->m_opType = SubOpRecord::R_SUB_START_REQ;
  subOpPtr.p->m_subPtrI = subPtr.i;
  subOpPtr.p->m_senderRef = senderRef;
  subOpPtr.p->m_senderData = senderData;
  subOpPtr.p->m_subscriberRef = subbPtr.i;

  {
    LocalDLFifoList<SubOpRecord> subOpList(c_subOpPool, subPtr.p->m_start_req);
    subOpList.add(subOpPtr);
  }

  /**
   * Check triggers
   */
  switch(subPtr.p->m_trigger_state){
  case Subscription::T_UNDEFINED:
    jam();
    /**
     * create triggers
     */
    create_triggers(signal, subPtr);
    break;
  case Subscription::T_CREATING:
    jam();
    /**
     * Triggers are already being created...wait for completion
     */
    return;
  case Subscription::T_DROPPING:
    jam();
    /**
     * Trigger(s) are being dropped...wait for completion
     *   (and recreate them when done)
     */
    break;
  case Subscription::T_DEFINED:{
    jam();
    report_sub_start_conf(signal, subPtr);
    return;
  }
  case Subscription::T_ERROR:
    jam();
    ndbrequire(false); // Checked above
    break;
  }
}

void
Suma::sendSubStartRef(Signal* signal, Uint32 dstref, Uint32 data, Uint32 err)
{
  jam();
  SubStartRef * ref = (SubStartRef *)signal->getDataPtrSend();
  ref->senderRef = reference();
  ref->senderData = data;
  ref->errorCode = err;
  sendSignal(dstref, GSN_SUB_START_REF, signal,
	     SubStartRef::SignalLength, JBB);
}

void
Suma::create_triggers(Signal* signal, SubscriptionPtr subPtr)
{
  jam();

  ndbrequire(subPtr.p->m_trigger_state == Subscription::T_UNDEFINED);
  subPtr.p->m_trigger_state = Subscription::T_CREATING;

  TablePtr tabPtr;
  c_tablePool.getPtr(tabPtr, subPtr.p->m_table_ptrI);

  AttributeMask attrMask;
  tabPtr.p->createAttributeMask(attrMask, *this);

  subPtr.p->m_outstanding_trigger = 3;
  for(Uint32 j = 0; j<3; j++)
  {
    Uint32 triggerId = (tabPtr.p->m_schemaVersion << 18) | (j << 16) | subPtr.i;
    ndbrequire(subPtr.p->m_triggers[j] == ILLEGAL_TRIGGER_ID);

    CreateTrigImplReq * const req =
      (CreateTrigImplReq*)signal->getDataPtrSend();
    req->senderRef = SUMA_REF;
    req->senderData = subPtr.i;
    req->requestType = 0;
    
    Uint32 ti = 0;
    TriggerInfo::setTriggerType(ti, TriggerType::SUBSCRIPTION_BEFORE);
    TriggerInfo::setTriggerActionTime(ti, TriggerActionTime::TA_DETACHED);
    TriggerInfo::setTriggerEvent(ti, (TriggerEvent::Value)j);
    TriggerInfo::setMonitorReplicas(ti, true);
    //TriggerInfo::setMonitorAllAttributes(ti, j == TriggerEvent::TE_DELETE);
    TriggerInfo::setMonitorAllAttributes(ti, true);
    TriggerInfo::setReportAllMonitoredAttributes(ti, 
       subPtr.p->m_options & Subscription::REPORT_ALL);
    req->triggerInfo = ti;
    
    req->receiverRef = SUMA_REF;
    req->triggerId = triggerId;
    req->tableId = subPtr.p->m_tableId;
    req->tableVersion = 0; // not used
    req->indexId = ~(Uint32)0;
    req->indexVersion = 0;
    req->attributeMask = attrMask;
    
    sendSignal(DBTUP_REF, GSN_CREATE_TRIG_IMPL_REQ, 
               signal, CreateTrigImplReq::SignalLength, JBB);
  }
}

void
Suma::execCREATE_TRIG_IMPL_CONF(Signal* signal)
{
  jamEntry();

  CreateTrigImplConf * conf = (CreateTrigImplConf*)signal->getDataPtr();
  const Uint32 triggerId = conf->triggerId;
  Uint32 type = (triggerId >> 16) & 0x3;
  Uint32 tableId = conf->tableId;

  TablePtr tabPtr;
  SubscriptionPtr subPtr;
  c_subscriptions.getPtr(subPtr, conf->senderData);
  c_tables.getPtr(tabPtr, subPtr.p->m_table_ptrI);

  ndbrequire(tabPtr.p->m_tableId == tableId);
  ndbrequire(subPtr.p->m_trigger_state == Subscription::T_CREATING);

  ndbrequire(type < 3);
  ndbrequire(subPtr.p->m_triggers[type] == ILLEGAL_TRIGGER_ID);
  subPtr.p->m_triggers[type] = triggerId;

  ndbrequire(subPtr.p->m_outstanding_trigger);
  subPtr.p->m_outstanding_trigger--;

  if (subPtr.p->m_outstanding_trigger)
  {
    jam();
    /**
     * Wait for more
     */
    return;
  }

  if (subPtr.p->m_errorCode == 0)
  {
    jam();
    subPtr.p->m_trigger_state = Subscription::T_DEFINED;
    report_sub_start_conf(signal, subPtr);
  }
  else
  {
    jam();
    subPtr.p->m_trigger_state = Subscription::T_ERROR;
    drop_triggers(signal, subPtr);
  }
}

void
Suma::execCREATE_TRIG_IMPL_REF(Signal* signal)
{
  jamEntry();

  CreateTrigImplRef * const ref = (CreateTrigImplRef*)signal->getDataPtr();
  const Uint32 triggerId = ref->triggerId;
  Uint32 type = (triggerId >> 16) & 0x3;
  Uint32 tableId = ref->tableId;

  TablePtr tabPtr;
  SubscriptionPtr subPtr;
  c_subscriptions.getPtr(subPtr, ref->senderData);
  c_tables.getPtr(tabPtr, subPtr.p->m_table_ptrI);

  ndbrequire(tabPtr.p->m_tableId == tableId);
  ndbrequire(subPtr.p->m_trigger_state == Subscription::T_CREATING);

  ndbrequire(type < 3);
  ndbrequire(subPtr.p->m_triggers[type] == ILLEGAL_TRIGGER_ID);

  subPtr.p->m_errorCode = ref->errorCode;

  ndbrequire(subPtr.p->m_outstanding_trigger);
  subPtr.p->m_outstanding_trigger--;

  if (subPtr.p->m_outstanding_trigger)
  {
    jam();
    /**
     * Wait for more
     */
    return;
  }

  for (Uint32 i = 0; i<3; i++)
  {
    jam();
    if (subPtr.p->m_triggers[i] == ILLEGAL_TRIGGER_ID)
    {
      jam();
      /**
       * Wait for more
       */
      return;
    }
  }

  subPtr.p->m_trigger_state = Subscription::T_ERROR;
  drop_triggers(signal, subPtr);
}

bool
Suma::check_sub_start(Uint32 subscriberRef)
{
  Uint32 nodeId = refToNode(subscriberRef);
  bool startme = c_startup.m_restart_server_node_id;
  bool handover = c_startup.m_wait_handover;
  bool connected = 
    c_failedApiNodes.get(nodeId) == false && 
    c_connected_nodes.get(nodeId);
  
  return (startme || handover || connected);
}

void
Suma::report_sub_start_conf(Signal* signal, Ptr<Subscription> subPtr)
{
  const Uint64 gci = get_current_gci(signal);
  {
    LocalDLList<Subscriber> list(c_subscriberPool,
                                 subPtr.p->m_subscribers);
    LocalDLFifoList<SubOpRecord> subOpList(c_subOpPool, subPtr.p->m_start_req);

    Ptr<Subscriber> ptr;
    Ptr<SubOpRecord> subOpPtr;
    for (subOpList.first(subOpPtr); !subOpPtr.isNull(); )
    {
      jam();

      Uint32 senderRef = subOpPtr.p->m_senderRef;
      Uint32 senderData = subOpPtr.p->m_senderData;
      c_subscriberPool.getPtr(ptr, subOpPtr.p->m_subscriberRef);

      if (check_sub_start(ptr.p->m_senderRef))
      {
        SubStartConf* conf = (SubStartConf*)signal->getDataPtrSend();
        conf->senderRef       = reference();
        conf->senderData      = senderData;
        conf->subscriptionId  = subPtr.p->m_subscriptionId;
        conf->subscriptionKey = subPtr.p->m_subscriptionKey;
        conf->firstGCI        = Uint32(gci >> 32);
        conf->part            = SubscriptionData::TableData;

        sendSignal(senderRef, GSN_SUB_START_CONF, signal,
                   SubStartConf::SignalLength, JBB);

        /**
         * Call before adding to list...
         *   cause method will (maybe) iterate thought list
         */
        bool report = subPtr.p->m_options & Subscription::REPORT_SUBSCRIBE;
        send_sub_start_stop_event(signal, ptr,NdbDictionary::Event::_TE_ACTIVE,
                                  report, list);
        
        list.add(ptr);
        c_subscriber_nodes.set(refToNode(ptr.p->m_senderRef));
        c_subscriber_per_node[refToNode(ptr.p->m_senderRef)]++;
      }
      else
      {
        jam();
        
        sendSubStartRef(signal,
                        senderRef, senderData, SubStartRef::NodeDied);

        c_subscriberPool.release(ptr);
      }
      
      Ptr<SubOpRecord> tmp = subOpPtr;
      subOpList.next(subOpPtr);
      subOpList.release(tmp);
    }
  }
  
  check_release_subscription(signal, subPtr);
}

void
Suma::report_sub_start_ref(Signal* signal,
                           Ptr<Subscription> subPtr,
                           Uint32 errCode)
{
  LocalDLList<Subscriber> list(c_subscriberPool,
                               subPtr.p->m_subscribers);
  LocalDLFifoList<SubOpRecord> subOpList(c_subOpPool, subPtr.p->m_start_req);

  Ptr<Subscriber> ptr;
  Ptr<SubOpRecord> subOpPtr;
  for (subOpList.first(subOpPtr); !subOpPtr.isNull(); )
  {
    jam();

    Uint32 senderRef = subOpPtr.p->m_senderRef;
    Uint32 senderData = subOpPtr.p->m_senderData;
    c_subscriberPool.getPtr(ptr, subOpPtr.p->m_subscriberRef);

    SubStartRef* ref = (SubStartRef*)signal->getDataPtrSend();
    ref->senderRef  = reference();
    ref->senderData = senderData;
    ref->errorCode  = errCode;

    sendSignal(senderRef, GSN_SUB_START_REF, signal,
               SubStartConf::SignalLength, JBB);


    Ptr<SubOpRecord> tmp = subOpPtr;
    subOpList.next(subOpPtr);
    subOpList.release(tmp);
    c_subscriberPool.release(ptr);
  }
}

void
Suma::drop_triggers(Signal* signal, SubscriptionPtr subPtr)
{
  jam();

  subPtr.p->m_outstanding_trigger = 0;

  Ptr<Table> tabPtr;
  c_tablePool.getPtr(tabPtr, subPtr.p->m_table_ptrI);
  if (tabPtr.p->m_state == Table::DROPPED)
  {
    jam();
    subPtr.p->m_triggers[0] = ILLEGAL_TRIGGER_ID;
    subPtr.p->m_triggers[1] = ILLEGAL_TRIGGER_ID;
    subPtr.p->m_triggers[2] = ILLEGAL_TRIGGER_ID;
  }
  else 
  {
    for(Uint32 j = 0; j<3; j++)
    {
      jam();
      Uint32 triggerId = subPtr.p->m_triggers[j];
      if (triggerId != ILLEGAL_TRIGGER_ID)
      {
        subPtr.p->m_outstanding_trigger++;
        
        DropTrigImplReq * const req =
          (DropTrigImplReq*)signal->getDataPtrSend();
        req->senderRef = SUMA_REF; // Sending to myself
        req->senderData = subPtr.i;
        req->requestType = 0;
        
        // TUP needs some triggerInfo to find right list
        Uint32 ti = 0;
        TriggerInfo::setTriggerType(ti, TriggerType::SUBSCRIPTION_BEFORE);
        TriggerInfo::setTriggerActionTime(ti, TriggerActionTime::TA_DETACHED);
        TriggerInfo::setTriggerEvent(ti, (TriggerEvent::Value)j);
        TriggerInfo::setMonitorReplicas(ti, true);
        //TriggerInfo::setMonitorAllAttributes(ti, j ==TriggerEvent::TE_DELETE);
        TriggerInfo::setMonitorAllAttributes(ti, true);
        TriggerInfo::setReportAllMonitoredAttributes(ti, 
                  subPtr.p->m_options & Subscription::REPORT_ALL);
        req->triggerInfo = ti;
        
        req->tableId = subPtr.p->m_tableId;
        req->tableVersion = 0; // not used
        req->indexId = RNIL;
        req->indexVersion = 0;
        req->triggerId = triggerId;
        
        sendSignal(DBTUP_REF, GSN_DROP_TRIG_IMPL_REQ,
                   signal, DropTrigImplReq::SignalLength, JBB);
      }
    }
  }
  
  if (subPtr.p->m_outstanding_trigger == 0)
  {
    jam();
    drop_triggers_complete(signal, subPtr);
  }
}

void
Suma::execDROP_TRIG_IMPL_REF(Signal* signal)
{
  jamEntry();
  DropTrigImplRef * const ref = (DropTrigImplRef*)signal->getDataPtr();
  Ptr<Table> tabPtr;
  Ptr<Subscription> subPtr;
  const Uint32 triggerId = ref->triggerId;
  const Uint32 type = (triggerId >> 16) & 0x3;

  c_subscriptionPool.getPtr(subPtr, ref->senderData);
  c_tables.getPtr(tabPtr, subPtr.p->m_table_ptrI);
  ndbrequire(tabPtr.p->m_tableId == ref->tableId);

  ndbrequire(type < 3);
  ndbrequire(subPtr.p->m_triggers[type] != ILLEGAL_TRIGGER_ID);
  subPtr.p->m_triggers[type] = ILLEGAL_TRIGGER_ID;

  ndbrequire(subPtr.p->m_outstanding_trigger);
  subPtr.p->m_outstanding_trigger--;

  if (subPtr.p->m_outstanding_trigger)
  {
    jam();
    /**
     * Wait for more
     */
    return;
  }

  drop_triggers_complete(signal, subPtr);
}

void
Suma::execDROP_TRIG_IMPL_CONF(Signal* signal)
{
  jamEntry();

  DropTrigImplConf * const conf = (DropTrigImplConf*)signal->getDataPtr();

  Ptr<Table> tabPtr;
  Ptr<Subscription> subPtr;
  const Uint32 triggerId = conf->triggerId;
  const Uint32 type = (triggerId >> 16) & 0x3;

  c_subscriptionPool.getPtr(subPtr, conf->senderData);
  c_tables.getPtr(tabPtr, subPtr.p->m_table_ptrI);
  ndbrequire(tabPtr.p->m_tableId == conf->tableId);

  ndbrequire(type < 3);
  ndbrequire(subPtr.p->m_triggers[type] != ILLEGAL_TRIGGER_ID);
  subPtr.p->m_triggers[type] = ILLEGAL_TRIGGER_ID;

  ndbrequire(subPtr.p->m_outstanding_trigger);
  subPtr.p->m_outstanding_trigger--;

  if (subPtr.p->m_outstanding_trigger)
  {
    jam();
    /**
     * Wait for more
     */
    return;
  }

  drop_triggers_complete(signal, subPtr);
}

void
Suma::drop_triggers_complete(Signal* signal, Ptr<Subscription> subPtr)
{
  switch(subPtr.p->m_trigger_state){
  case Subscription::T_UNDEFINED:
  case Subscription::T_CREATING:
  case Subscription::T_DEFINED:
    jam();
    ndbrequire(false);
    break;
  case Subscription::T_DROPPING:
    jam();
    /**
     */
    subPtr.p->m_trigger_state = Subscription::T_UNDEFINED;
    if (!subPtr.p->m_start_req.isEmpty())
    {
      jam();
      create_triggers(signal, subPtr);
      return;
    }
    break;
  case Subscription::T_ERROR:
    jam();
    Uint32 err = subPtr.p->m_errorCode;
    subPtr.p->m_trigger_state = Subscription::T_UNDEFINED;
    subPtr.p->m_errorCode = 0;
    report_sub_start_ref(signal, subPtr, err);
    break;
  }

  check_release_subscription(signal, subPtr);
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
  bool abortStart = (req->requestInfo & SubStopReq::RI_ABORT_START);

  if (c_startup.m_restart_server_node_id == RNIL)
  {
    jam();

    /**
     * We havent started syncing yet
     */
    sendSubStopRef(signal,
                   senderRef, senderData, SubStopRef::NF_FakeErrorREF);
    return;
  }

  bool found = c_subscriptions.find(subPtr, key);
  if (!found)
  {
    jam();
    sendSubStopRef(signal,
                   senderRef, senderData, SubStopRef::NoSuchSubscription);
    return;
  }
  
  switch(subPtr.p->m_state){
  case Subscription::UNDEFINED:
    jam();
    ndbrequire(false);
  case Subscription::DEFINING:
    jam();
    sendSubStopRef(signal,
                   senderRef, senderData, SubStopRef::Defining);
    return;
  case Subscription::DEFINED:
    jam();
    break;
  }

  Ptr<SubOpRecord> subOpPtr;
  LocalDLFifoList<SubOpRecord> list(c_subOpPool, subPtr.p->m_stop_req);
  bool empty = list.isEmpty();
  if (list.seize(subOpPtr) == false)
  {
    jam();
    sendSubStopRef(signal,
                   senderRef, senderData, SubStopRef::OutOfSubOpRecords);
    return;
  }

  if (abortStart)
  {
    jam();
    subOpPtr.p->m_opType = SubOpRecord::R_SUB_ABORT_START_REQ;
  }
  else
  {
    jam();
    subOpPtr.p->m_opType = SubOpRecord::R_SUB_STOP_REQ;
  }
  subOpPtr.p->m_subPtrI = subPtr.i;
  subOpPtr.p->m_senderRef = senderRef;
  subOpPtr.p->m_senderData = senderData;
  subOpPtr.p->m_subscriberRef = subscriberRef;
  subOpPtr.p->m_subscriberData = subscriberData;


  if (empty)
  {
    jam();
    signal->theData[0] = SumaContinueB::SUB_STOP_REQ;
    signal->theData[1] = subOpPtr.i;
    signal->theData[2] = RNIL;
    sendSignal(SUMA_REF, GSN_CONTINUEB, signal, 3, JBB);
  }
}

void
Suma::sub_stop_req(Signal* signal)
{
  jam();

  Ptr<SubOpRecord> subOpPtr;
  c_subOpPool.getPtr(subOpPtr, signal->theData[1]);

  Ptr<Subscription> subPtr;
  c_subscriptionPool.getPtr(subPtr, subOpPtr.p->m_subPtrI);

  Ptr<Subscriber> ptr;
  {
    LocalDLList<Subscriber> list(c_subscriberPool, subPtr.p->m_subscribers);
    if (signal->theData[2] == RNIL)
    {
      jam();
      list.first(ptr);
    }
    else
    {
      jam();
      list.getPtr(ptr, signal->theData[2]);
    }

    for (Uint32 i = 0; i<32 && !ptr.isNull(); i++, list.next(ptr))
    {
      if (ptr.p->m_senderRef == subOpPtr.p->m_subscriberRef &&
          ptr.p->m_senderData == subOpPtr.p->m_subscriberData)
      {
        jam();
        goto found;
      }
    }
  }

  if (ptr.isNull())
  {
    jam();
    sendSubStopRef(signal,
                   subOpPtr.p->m_senderRef,
                   subOpPtr.p->m_senderData,
                   SubStopRef::NoSuchSubscriber);
    check_remove_queue(signal, subPtr, subOpPtr, true, true);
    return;
  }

  signal->theData[0] = SumaContinueB::SUB_STOP_REQ;
  signal->theData[1] = subOpPtr.i;
  signal->theData[2] = ptr.i;
  sendSignal(SUMA_REF, GSN_CONTINUEB, signal, 3, JBB);
  return;

found:
  {
    LocalDLList<Subscriber> list(c_subscriberPool, subPtr.p->m_subscribers);
    list.remove(ptr);
    /**
     * NOTE: remove before...so we done send UNSUBSCRIBE to self (yuck)
     */
    bool report = subPtr.p->m_options & Subscription::REPORT_SUBSCRIBE;
    report_sub_stop_conf(signal, subOpPtr, ptr, report, list);
    c_subscriberPool.release(ptr);
  }
  check_remove_queue(signal, subPtr, subOpPtr, true, true);
  check_release_subscription(signal, subPtr);
}

void
Suma::check_remove_queue(Signal* signal,
                         Ptr<Subscription> subPtr,
                         Ptr<SubOpRecord> subOpPtr,
                         bool ishead,
                         bool dorelease)
{
  LocalDLFifoList<SubOpRecord> list(c_subOpPool, subPtr.p->m_stop_req);

  {
    Ptr<SubOpRecord> tmp;
    list.first(tmp);
    if (ishead)
    {
      jam();
      ndbrequire(tmp.i == subOpPtr.i);
    }
    else
    {
      jam();
      ishead = (tmp.i == subOpPtr.i);
    }
  }

  if (dorelease)
  {
    jam();
    list.release(subOpPtr);
  }
  else
  {
    jam();
    list.remove(subOpPtr);
  }

  if (ishead)
  {
    jam();
    if (list.first(subOpPtr) == false)
    {
      jam();
      c_restart.m_waiting_on_self = 1;
      return;
    }
    // Fall through
  }
  else
  {
    jam();
    return;
  }

  switch(subOpPtr.p->m_opType){
  case SubOpRecord::R_SUB_ABORT_START_REQ:
  case SubOpRecord::R_SUB_STOP_REQ:
    jam();
    signal->theData[0] = SumaContinueB::SUB_STOP_REQ;
    signal->theData[1] = subOpPtr.i;
    signal->theData[2] = RNIL;
    sendSignal(SUMA_REF, GSN_CONTINUEB, signal, 3, JBB);
    return;
  case SubOpRecord::R_API_FAIL_REQ:
    jam();
    signal->theData[0] = SumaContinueB::API_FAIL_SUBSCRIPTION;
    signal->theData[1] = subOpPtr.i;
    signal->theData[2] = RNIL;
    sendSignal(SUMA_REF, GSN_CONTINUEB, signal, 3, JBB);
    return;
  case SubOpRecord::R_START_ME_REQ:
    jam();
    sendSubCreateReq(signal, subPtr);
    return;
  }
}

void
Suma::report_sub_stop_conf(Signal* signal,
                           Ptr<SubOpRecord> subOpPtr,
                           Ptr<Subscriber> ptr,
                           bool report,
                           LocalDLList<Subscriber>& list)
{
  jam();
  CRASH_INSERTION(13020);
  
  Uint32 senderRef = subOpPtr.p->m_senderRef;
  Uint32 senderData = subOpPtr.p->m_senderData;
  bool abortStart = subOpPtr.p->m_opType == SubOpRecord::R_SUB_ABORT_START_REQ;
  
  // let subscriber know that subscrber is stopped
  if (!abortStart)
  {
    jam();
    send_sub_start_stop_event(signal, ptr, NdbDictionary::Event::_TE_STOP,
                              report, list);
  }
  
  SubStopConf * const conf = (SubStopConf*)signal->getDataPtrSend();
  conf->senderRef= reference();
  conf->senderData= senderData;
  sendSignal(senderRef, GSN_SUB_STOP_CONF, signal,
	     SubStopConf::SignalLength, JBB);

  Uint32 nodeId = refToNode(ptr.p->m_senderRef);
  if (c_subscriber_per_node[nodeId])
  {
    c_subscriber_per_node[nodeId]--;
    if (c_subscriber_per_node[nodeId] == 0)
    {
      jam();
      c_subscriber_nodes.clear(nodeId);
    }
  }
}

void
Suma::sendSubStopRef(Signal* signal,
                     Uint32 retref,
                     Uint32 data,
                     Uint32 errCode)
{
  jam();
  SubStopRef  * ref = (SubStopRef *)signal->getDataPtrSend();
  ref->senderRef = reference();
  ref->errorCode = errCode;
  ref->senderData = data;
  sendSignal(retref, GSN_SUB_STOP_REF, signal,  SubStopRef::SignalLength, JBB);
}

// report new started subscriber to all other subscribers
void
Suma::send_sub_start_stop_event(Signal *signal,
                                Ptr<Subscriber> ptr,
                                NdbDictionary::Event::_TableEvent event,
                                bool report,
                                LocalDLList<Subscriber>& list)
{
  const Uint64 gci = get_current_gci(signal);
  SubTableData * data  = (SubTableData*)signal->getDataPtrSend();
  Uint32 nodeId = refToNode(ptr.p->m_senderRef);

  NdbDictionary::Event::_TableEvent other;
  if (event == NdbDictionary::Event::_TE_STOP)
  {
    other = NdbDictionary::Event::_TE_UNSUBSCRIBE;
  }
  else if (event == NdbDictionary::Event::_TE_ACTIVE)
  {
    other = NdbDictionary::Event::_TE_SUBSCRIBE;
  }
  else
  {
    jamLine(event);
    ndbrequire(false);
  }
  
  data->gci_hi         = Uint32(gci >> 32);
  data->gci_lo         = Uint32(gci);
  data->tableId        = 0;
  data->requestInfo    = 0;
  SubTableData::setOperation(data->requestInfo, event);
  SubTableData::setNdbdNodeId(data->requestInfo, getOwnNodeId());
  SubTableData::setReqNodeId(data->requestInfo, nodeId);
  data->changeMask     = 0;
  data->totalLen       = 0;
  data->senderData     = ptr.p->m_senderData;
  sendSignal(ptr.p->m_senderRef, GSN_SUB_TABLE_DATA, signal,
             SubTableData::SignalLength, JBB);

  if (report == false)
  {
    return;
  }

  data->requestInfo    = 0;
  SubTableData::setOperation(data->requestInfo, other);
  SubTableData::setNdbdNodeId(data->requestInfo, getOwnNodeId());

  Ptr<Subscriber> tmp;
  for(list.first(tmp); !tmp.isNull(); list.next(tmp))
  {
    jam();
    SubTableData::setReqNodeId(data->requestInfo, nodeId);
    data->senderData = tmp.p->m_senderData;
    sendSignal(tmp.p->m_senderRef, GSN_SUB_TABLE_DATA, signal,
               SubTableData::SignalLength, JBB);
    
    ndbassert(tmp.i != ptr.i); // ptr should *NOT* be in list now
    if (other != NdbDictionary::Event::_TE_UNSUBSCRIBE)
    {
      jam();
      SubTableData::setReqNodeId(data->requestInfo, 
                                 refToNode(tmp.p->m_senderRef));
      
      data->senderData = ptr.p->m_senderData;
      sendSignal(ptr.p->m_senderRef, GSN_SUB_TABLE_DATA, signal,
                 SubTableData::SignalLength, JBB);
    }
  }
}

void
Suma::Table::createAttributeMask(AttributeMask& mask,
                                 Suma &suma)
{
  mask.clear();
  for(Uint32 i = 0; i<m_noOfAttributes; i++)
    mask.set(i);
}

void Suma::suma_ndbrequire(bool v) { ndbrequire(v); }


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
  f_trigBufferSize = sum;

  ndbrequire(src == end);

  if ((syncPtr.p->m_requestInfo & SubSyncReq::LM_Exclusive) == 0)
  {
    sendScanSubTableData(signal, syncPtr, 0);
  }

  DBUG_VOID_RETURN;
}

void
Suma::execKEYINFO20(Signal* signal)
{
  jamEntry();
  KeyInfo20* data = (KeyInfo20*)signal->getDataPtr();

  const Uint32 opPtrI = data->clientOpPtr;
  const Uint32 takeOver = data->scanInfo_Node;

  ndbrequire(f_bufferLock == opPtrI);

  Ptr<SyncRecord> syncPtr;
  c_syncPool.getPtr(syncPtr, (opPtrI >> 16));
  sendScanSubTableData(signal, syncPtr, takeOver);
}

void
Suma::sendScanSubTableData(Signal* signal,
                           Ptr<SyncRecord> syncPtr, Uint32 takeOver)
{
  const Uint32 attribs = syncPtr.p->m_currentNoOfAttributes;
  const Uint32 sum =  f_trigBufferSize;

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
  Uint32 ref = syncPtr.p->m_senderRef;
  sdata->tableId = syncPtr.p->m_tableId;
  sdata->senderData = syncPtr.p->m_senderData;
  sdata->requestInfo = 0;
  SubTableData::setOperation(sdata->requestInfo, 
			     NdbDictionary::Event::_TE_SCAN); // Scan
  sdata->gci_hi = 0; // Undefined
  sdata->gci_lo = 0;
  sdata->takeOver = takeOver;
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

  Ptr<Subscription> subPtr;
  c_subscriptionPool.getPtr(subPtr, trigId & 0xFFFF);

  ndbassert(gci > m_last_complete_gci);

  ndbrequire(f_bufferLock == trigId);
  /**
   * Reset f_bufferLock
   */
  f_bufferLock = 0;
  b_bufferLock = 0;
  
  Uint32 tableId = subPtr.p->m_tableId;
  Uint32 schemaVersion =
    c_tablePool.getPtr(subPtr.p->m_table_ptrI)->m_schemaVersion;
  
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
    SubTableData * data = (SubTableData*)signal->getDataPtrSend();//trg;
    data->gci_hi         = gci_hi;
    data->gci_lo         = gci_lo;
    data->tableId        = tableId;
    data->requestInfo    = 0;
    SubTableData::setOperation(data->requestInfo, event);
    data->flags          = 0;
    data->anyValue       = any_value;
    data->totalLen       = ptrLen;
    
    {
      LocalDLList<Subscriber> list(c_subscriberPool, subPtr.p->m_subscribers);
      SubscriberPtr subbPtr;
      for(list.first(subbPtr); !subbPtr.isNull(); list.next(subbPtr))
      {
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
      * dst++ = subPtr.i;
      * dst++ = schemaVersion;
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
Suma::checkMaxBufferedEpochs(Signal *signal)
{
  /*
   * Check if any subscribers are exceeding the MaxBufferedEpochs
   */
  Ptr<Gcp_record> gcp;
  jamEntry();
  if (c_gcp_list.isEmpty())
  {
    jam();
    return;
  }
  c_gcp_list.first(gcp);
  if (ERROR_INSERTED(13037))
  {
    jam();
    CLEAR_ERROR_INSERT_VALUE;
    ndbout_c("Simulating exceeding the MaxBufferedEpochs %u(%llu,%llu,%llu)",
            c_maxBufferedEpochs, m_max_seen_gci,
            m_last_complete_gci, gcp.p->m_gci);
  }
  else if (c_gcp_list.count() < c_maxBufferedEpochs)
  {
    return;
  }
  NodeBitmask subs = gcp.p->m_subscribers;
  jam();
  // Disconnect lagging subscribers waiting for oldest epoch
  ndbout_c("Found lagging epoch %llu", gcp.p->m_gci);
  for(Uint32 nodeId = 0; nodeId < MAX_NODES; nodeId++)
  {
    if (subs.get(nodeId))
    {
      jam();
      subs.clear(nodeId);
      // Disconnecting node
      signal->theData[0] = NDB_LE_SubscriptionStatus;
      signal->theData[1] = 1; // DISCONNECTED;
      signal->theData[2] = nodeId;
      signal->theData[3] = (Uint32) gcp.p->m_gci;
      signal->theData[4] = (Uint32) (gcp.p->m_gci >> 32);
      signal->theData[5] = (Uint32) c_gcp_list.count();
      signal->theData[6] = c_maxBufferedEpochs;
      sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 8, JBB);
      
      /**
       * Force API_FAILREQ
       */
      signal->theData[0] = nodeId;
      sendSignal(QMGR_REF, GSN_API_FAILREQ, signal, 1, JBA);
    }
  }
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
  Uint32 flags = (m_missing_data)
                 ? rep->flags | SubGcpCompleteRep::MISSING_DATA
                 : rep->flags;

  if (ERROR_INSERTED(13036))
  {
    jam();
    CLEAR_ERROR_INSERT_VALUE;
    ndbout_c("Simulating out of event buffer at node failure");
    flags |= SubGcpCompleteRep::MISSING_DATA;
  }

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
  checkMaxBufferedEpochs(signal);
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
    else
    {
      char buf[100];
      c_subscriber_nodes.getText(buf);
      g_eventLogger->error("c_gcp_list.seize() failed: gci: %llu nodes: %s",
                           gci, buf);
    }
  }
  
  /**
   * Add GCP COMPLETE REP to buffer
   */
  bool subscribers = !c_subscriber_nodes.isclear();
  for(Uint32 i = 0; i<c_no_of_buckets; i++)
  {
    if(m_active_buckets.get(i))
      continue;

    if (subscribers || (c_buckets[i].m_state & Bucket::BUCKET_RESEND))
    {
      //Uint32* dst;
      get_buffer_ptr(signal, i, gci, 0);
    }
  }

  if(m_out_of_buffer_gci && gci > m_out_of_buffer_gci)
  {
    infoEvent("Reenable event buffer");
    m_out_of_buffer_gci = 0;
    m_missing_data = false;
  }
}

void
Suma::execCREATE_TAB_CONF(Signal *signal)
{
  jamEntry();
  DBUG_ENTER("Suma::execCREATE_TAB_CONF");

  DBUG_VOID_RETURN;
}

void
Suma::execDROP_TAB_CONF(Signal *signal)
{
  jamEntry();
  ndbassert(signal->getNoOfSections() == 0);

  DropTabConf * const conf = (DropTabConf*)signal->getDataPtr();
  Uint32 senderRef= conf->senderRef;
  Uint32 tableId= conf->tableId;

  TablePtr tabPtr;
  if (!c_tables.find(tabPtr, tableId))
  {
    jam();
    return;
  }

  DBUG_PRINT("info",("drop table id: %d[i=%u]", tableId, tabPtr.i));

  tabPtr.p->m_state = Table::DROPPED;
  c_tables.remove(tabPtr);

  if (tabPtr.p->m_subscriptions.isEmpty())
  {
    jam();
    tabPtr.p->release(* this);
    c_tablePool.release(tabPtr);
    return;
  }

  if (senderRef == 0)
  {
    jam();
    return;
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
  
  Ptr<Subscription> subPtr;
  LocalDLList<Subscription> subList(c_subscriptionPool,
                                    tabPtr.p->m_subscriptions);

  for (subList.first(subPtr); !subPtr.isNull(); )
  {
    if(subPtr.p->m_subscriptionType != SubCreateReq::TableEvent)
    {
      jam();
      continue;
      //continue in for-loop if the table is not part of
      //the subscription. Otherwise, send data to subscriber.
    }

    Ptr<Subscriber> ptr;
    LocalDLList<Subscriber> list(c_subscriberPool, subPtr.p->m_subscribers);
    for(list.first(ptr); !ptr.isNull(); list.next(ptr))
    {
      jam();
      data->senderData= ptr.p->m_senderData;
      sendSignal(ptr.p->m_senderRef, GSN_SUB_TABLE_DATA, signal,
                 SubTableData::SignalLength, JBB);
    }
    
    Ptr<Subscription> tmp = subPtr;
    subList.next(subPtr);
  }
}

/**
 * This receives DICT_TAB_INFO in long signal section 1, and releases the data
 * after use.
 */
void
Suma::execALTER_TAB_REQ(Signal *signal)
{
  jamEntry();

  AlterTabReq * const req = (AlterTabReq*)signal->getDataPtr();
  Uint32 senderRef= req->senderRef;
  Uint32 tableId= req->tableId;
  Uint32 changeMask= req->changeMask;
  TablePtr tabPtr;

  // Copy DICT_TAB_INFO to local linear buffer
  SectionHandle handle(this, signal);
  SegmentedSectionPtr tabInfoPtr;
  handle.getSection(tabInfoPtr, 0);

  if (!c_tables.find(tabPtr, tableId))
  {
    jam();
    releaseSections(handle);
    return;
  }

  if (senderRef == 0)
  {
    jam();
    releaseSections(handle);
    return;
  }
  // dict coordinator sends info to API
  
#ifndef DBUG_OFF
  ndbout_c("DICT_TAB_INFO in SUMA,  tabInfoPtr.sz = %d", tabInfoPtr.sz);
  SimplePropertiesSectionReader reader(handle.m_ptr[0],
				       getSectionSegmentPool());
  reader.printAll(ndbout);
#endif
  copy(b_dti_buf, tabInfoPtr);
  releaseSections(handle);

  LinearSectionPtr lptr[3];
  lptr[0].p = b_dti_buf;
  lptr[0].sz = tabInfoPtr.sz;

  const Uint64 gci = get_current_gci(signal);
  SubTableData * data = (SubTableData*)signal->getDataPtrSend();
  data->gci_hi         = Uint32(gci >> 32);
  data->gci_lo         = Uint32(gci);
  data->tableId        = tableId;
  data->requestInfo    = 0;
  SubTableData::setOperation(data->requestInfo, 
			     NdbDictionary::Event::_TE_ALTER);
  SubTableData::setReqNodeId(data->requestInfo, refToNode(senderRef));
  data->flags          = 0;
  data->changeMask     = changeMask;
  data->totalLen       = tabInfoPtr.sz;
  Ptr<Subscription> subPtr;
  LocalDLList<Subscription> subList(c_subscriptionPool,
                                    tabPtr.p->m_subscriptions);

  for (subList.first(subPtr); !subPtr.isNull(); subList.next(subPtr))
  {
    if(subPtr.p->m_subscriptionType != SubCreateReq::TableEvent)
    {
      jam();
      continue;
      //continue in for-loop if the table is not part of
      //the subscription. Otherwise, send data to subscriber.
    }
  
    Ptr<Subscriber> ptr;
    LocalDLList<Subscriber> list(c_subscriberPool, subPtr.p->m_subscribers);
    for(list.first(ptr); !ptr.isNull(); list.next(ptr))
    {
      jam();
      data->senderData= ptr.p->m_senderData;
      Callback c = { 0, 0 };
      sendFragmentedSignal(ptr.p->m_senderRef, GSN_SUB_TABLE_DATA, signal,
                           SubTableData::SignalLength, JBB, lptr, 1, c);
    }
  }
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

  if (ERROR_INSERTED(13037))
  {
    jam();
    ndbout_c("Simulating exceeding the MaxBufferedEpochs, ignoring ack");
    return;
  }

  if (refToBlock(senderRef) == SUMA) 
  {
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
  if (ERROR_INSERTED(13023))
  {
    ndbout_c("Throwing SUB_GCP_COMPLETE_ACK gci: %u/%u from %u",
             Uint32(gci>>32), Uint32(gci), nodeId);
    return;
  }

  
  jam();
  Ptr<Gcp_record> gcp;
  for(c_gcp_list.first(gcp); !gcp.isNull(); c_gcp_list.next(gcp))
  {
    if(gcp.p->m_gci == gci)
    {
      gcp.p->m_subscribers.clear(nodeId);
      gcp.p->m_subscribers.bitAND(c_subscriber_nodes);
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
    g_eventLogger->warning("ACK wo/ gcp record (gci: %u/%u) ref: %.8x from: %.8x",
                           Uint32(gci >> 32), Uint32(gci),
                           senderRef, signal->getSendersBlockRef());
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

  CRASH_INSERTION(13021);

  const SubRemoveReq req = *(SubRemoveReq*)signal->getDataPtr();
  SubscriptionPtr subPtr;
  Subscription key;
  key.m_subscriptionId  = req.subscriptionId;
  key.m_subscriptionKey = req.subscriptionKey;

  if (c_startup.m_restart_server_node_id == RNIL)
  {
    jam();

    /**
     * We havent started syncing yet
     */
    sendSubRemoveRef(signal,  req, SubRemoveRef::NF_FakeErrorREF);
    return;
  }

  bool found = c_subscriptions.find(subPtr, key);

  if(!found)
  {
    jam();
    sendSubRemoveRef(signal, req, SubRemoveRef::NoSuchSubscription);
    return;
  }

  switch(subPtr.p->m_state){
  case Subscription::UNDEFINED:
    jam();
  case Subscription::DEFINING:
    jam();
    ndbrequire(false);
  case Subscription::DEFINED:
    if (subPtr.p->m_options & Subscription::MARKED_DROPPED)
    {
      /**
       * already dropped
       */
      jam();
      sendSubRemoveRef(signal, req, SubRemoveRef::AlreadyDropped);
      return;
    }
    break;
  }

  subPtr.p->m_options |= Subscription::MARKED_DROPPED;
  check_release_subscription(signal, subPtr);

  SubRemoveConf * const conf = (SubRemoveConf*)signal->getDataPtrSend();
  conf->senderRef            = reference();
  conf->senderData           = req.senderData;
  conf->subscriptionId       = req.subscriptionId;
  conf->subscriptionKey      = req.subscriptionKey;

  sendSignal(req.senderRef, GSN_SUB_REMOVE_CONF, signal,
             SubRemoveConf::SignalLength, JBB);
  return;
}

void
Suma::check_release_subscription(Signal* signal, Ptr<Subscription> subPtr)
{
  if (!subPtr.p->m_subscribers.isEmpty())
  {
    jam();
    return;
  }

  if (!subPtr.p->m_start_req.isEmpty())
  {
    jam();
    return;
  }

  if (!subPtr.p->m_stop_req.isEmpty())
  {
    jam();
    return;
  }

  switch(subPtr.p->m_trigger_state){
  case Subscription::T_UNDEFINED:
    jam();
    goto do_release;
  case Subscription::T_CREATING:
    jam();
    /**
     * Wait for completion
     */
    return;
  case Subscription::T_DEFINED:
    jam();
    subPtr.p->m_trigger_state = Subscription::T_DROPPING;
    drop_triggers(signal, subPtr);
    return;
  case Subscription::T_DROPPING:
    jam();
    /**
     * Wait for completion
     */
    return;
  case Subscription::T_ERROR:
    jam();
    /**
     * Wait for completion
     */
    return;
  }
  ndbrequire(false);

do_release:
  TablePtr tabPtr;
  c_tables.getPtr(tabPtr, subPtr.p->m_table_ptrI);

  if (tabPtr.p->m_state == Table::DROPPED)
  {
    jam();
    subPtr.p->m_options |= Subscription::MARKED_DROPPED;
  }

  if ((subPtr.p->m_options & Subscription::MARKED_DROPPED) == 0)
  {
    jam();
    return;
  }

  {
    LocalDLList<Subscription> list(c_subscriptionPool,
                                   tabPtr.p->m_subscriptions);
    list.remove(subPtr);
  }

  if (tabPtr.p->m_subscriptions.isEmpty())
  {
    jam();
    switch(tabPtr.p->m_state){
    case Table::UNDEFINED:
      ndbrequire(false);
    case Table::DEFINING:
      break;
    case Table::DEFINED:
      jam();
      c_tables.remove(tabPtr);
      // Fall through
    case Table::DROPPED:
      jam();
      tabPtr.p->release(* this);
      c_tablePool.release(tabPtr);
    };
  }
  
  c_subscriptions.release(subPtr);
}

void
Suma::sendSubRemoveRef(Signal* signal,
                       const SubRemoveReq& req,
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
  sendSignal(signal->getSendersBlockRef(), GSN_SUB_REMOVE_REF, 
	     signal, SubRemoveRef::SignalLength, JBB);
  DBUG_VOID_RETURN;
}

void
Suma::Table::release(Suma & suma){
  jamBlock(&suma);

  m_state = UNDEFINED;
}

void
Suma::SyncRecord::release(){
  jam();

  LocalDataBuffer<15> fragBuf(suma.c_dataBufferPool, m_fragments);
  fragBuf.release();

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

  Uint32 retref = signal->getSendersBlockRef();
  if (c_restart.m_ref)
  {
    jam();
    SumaStartMeRef* ref= (SumaStartMeRef*)signal->getDataPtrSend();
    ref->errorCode = SumaStartMeRef::Busy;
    sendSignal(retref, GSN_SUMA_START_ME_REF, signal,
               SumaStartMeRef::SignalLength, JBB);
    return;
  }

  if (getNodeState().getStarted() == false)
  {
    jam();
    SumaStartMeRef* ref= (SumaStartMeRef*)signal->getDataPtrSend();
    ref->errorCode = SumaStartMeRef::NotStarted;
    sendSignal(retref, GSN_SUMA_START_ME_REF, signal,
               SumaStartMeRef::SignalLength, JBB);
    return;
  }

  Ptr<SubOpRecord> subOpPtr;
  if (c_subOpPool.seize(subOpPtr) == false)
  {
    jam();
    SumaStartMeRef* ref= (SumaStartMeRef*)signal->getDataPtrSend();
    ref->errorCode = SumaStartMeRef::Busy;
    sendSignal(retref, GSN_SUMA_START_ME_REF, signal,
               SumaStartMeRef::SignalLength, JBB);
    return;
  }

  subOpPtr.p->m_opType = SubOpRecord::R_START_ME_REQ;

  c_restart.m_abort = 0;
  c_restart.m_waiting_on_self = 0;
  c_restart.m_ref = retref;
  c_restart.m_max_seq = c_current_seq;
  c_restart.m_subOpPtrI = subOpPtr.i;

  DLHashTable<Subscription>::Iterator it;
  if (c_subscriptions.first(it))
  {
    jam();

    /**
     * We only need to handle subscriptions with seq <= c_current_seq
     *   all subscriptions(s) created after this, will be handled by
     *   starting suma directly
     */
    c_current_seq++;
  }

  copySubscription(signal, it);
}

void
Suma::copySubscription(Signal* signal, DLHashTable<Subscription>::Iterator it)
{
  jam();

  Ptr<SubOpRecord> subOpPtr;
  c_subOpPool.getPtr(subOpPtr, c_restart.m_subOpPtrI);

  Ptr<Subscription> subPtr = it.curr;
  if (!subPtr.isNull())
  {
    jam();
    c_restart.m_subPtrI = subPtr.i;
    c_restart.m_bucket = it.bucket;

    LocalDLFifoList<SubOpRecord> list(c_subOpPool, subPtr.p->m_stop_req);
    bool empty = list.isEmpty();
    list.add(subOpPtr);

    if (!empty)
    {
      /**
       * Wait for lock
       */
      jam();
      c_restart.m_waiting_on_self = 1;
      return;
    }

    sendSubCreateReq(signal, subPtr);
  }
  else
  {
    jam();
    SumaStartMeConf* conf = (SumaStartMeConf*)signal->getDataPtrSend();
    conf->unused = 0;
    sendSignal(c_restart.m_ref, GSN_SUMA_START_ME_CONF, signal,
               SumaStartMeConf::SignalLength, JBB);

    c_subOpPool.release(subOpPtr);
    c_restart.m_ref = 0;
    return;
  }
}

void
Suma::sendSubCreateReq(Signal* signal, Ptr<Subscription> subPtr)
{
  jam();

  if (c_restart.m_abort)
  {
    jam();
    abort_start_me(signal, subPtr, true);
    return;
  }

  c_restart.m_waiting_on_self = 0;
  SubCreateReq * req = (SubCreateReq *)signal->getDataPtrSend();
  req->senderRef        = reference();
  req->senderData       = subPtr.i;
  req->subscriptionId   = subPtr.p->m_subscriptionId;
  req->subscriptionKey  = subPtr.p->m_subscriptionKey;
  req->subscriptionType = subPtr.p->m_subscriptionType;
  req->tableId          = subPtr.p->m_tableId;
  req->schemaTransId    = 0;

  if (subPtr.p->m_options & Subscription::REPORT_ALL)
  {
    req->subscriptionType |= SubCreateReq::ReportAll;
  }

  if (subPtr.p->m_options & Subscription::REPORT_SUBSCRIBE)
  {
    req->subscriptionType |= SubCreateReq::ReportSubscribe;
  }

  if (subPtr.p->m_options & Subscription::MARKED_DROPPED)
  {
    req->subscriptionType |= SubCreateReq::NR_Sub_Dropped;
    ndbout_c("copying dropped sub: %u", subPtr.i);
  }

  Ptr<Table> tabPtr;
  c_tablePool.getPtr(tabPtr, subPtr.p->m_table_ptrI);
  if (tabPtr.p->m_state != Table::DROPPED)
  {
    jam();
    c_restart.m_waiting_on_self = 0;
    if (!ndbd_suma_dictlock(getNodeInfo(refToNode(c_restart.m_ref)).m_version))
    {
      jam();
      /**
       * Downgrade
       *
       * In pre suma v2, SUB_CREATE_REQ::SignalLength is one greater
       *   but code checks length and set a default value...
       *   so we dont need to do anything...
       *   Thank you Ms. Fortuna
       */
    }

    sendSignal(c_restart.m_ref, GSN_SUB_CREATE_REQ, signal,
               SubCreateReq::SignalLength, JBB);
  }
  else
  {
    jam();
    ndbout_c("not copying sub %u with dropped table: %u/%u",
             subPtr.i,
             tabPtr.p->m_tableId, tabPtr.i);

    c_restart.m_waiting_on_self = 1;
    SubCreateConf * conf = (SubCreateConf *)signal->getDataPtrSend();
    conf->senderRef        = reference();
    conf->senderData       = subPtr.i;
    sendSignal(reference(), GSN_SUB_CREATE_CONF, signal,
               SubCreateConf::SignalLength, JBB);
  }
}

void 
Suma::execSUB_CREATE_REF(Signal* signal)
{
  jamEntry();

  SubCreateRef *const ref= (SubCreateRef *)signal->getDataPtr();
  Uint32 error= ref->errorCode;

  {
    SumaStartMeRef* ref= (SumaStartMeRef*)signal->getDataPtrSend();
    ref->errorCode = error;
    sendSignal(c_restart.m_ref, GSN_SUMA_START_ME_REF, signal,
               SumaStartMeRef::SignalLength, JBB);
  }

  Ptr<Subscription> subPtr;
  c_subscriptionPool.getPtr(subPtr, c_restart.m_subPtrI);
  abort_start_me(signal, subPtr, true);
}

void 
Suma::execSUB_CREATE_CONF(Signal* signal)
{
  jamEntry();

  /**
   * We have lock...start all subscriber(s)
   */
  Ptr<Subscription> subPtr;
  c_subscriptionPool.getPtr(subPtr, c_restart.m_subPtrI);

  c_restart.m_waiting_on_self = 0;

  /**
   * Check if we were aborted...
   *  this signal is sent to self in case of DROPPED subscription...
   */
  if (c_restart.m_abort)
  {
    jam();
    abort_start_me(signal, subPtr, true);
    return;
  }
  
  Ptr<Table> tabPtr;
  c_tablePool.getPtr(tabPtr, subPtr.p->m_table_ptrI);

  Ptr<Subscriber> ptr;
  if (tabPtr.p->m_state != Table::DROPPED)
  {
    jam();
    LocalDLList<Subscriber> list(c_subscriberPool, subPtr.p->m_subscribers);
    list.first(ptr);
  }
  else
  {
    jam();
    ptr.setNull();
    ndbout_c("not copying subscribers on sub: %u with dropped table %u/%u",
             subPtr.i, tabPtr.p->m_tableId, tabPtr.i);
  }

  copySubscriber(signal, subPtr, ptr);
}

void
Suma::copySubscriber(Signal* signal,
                     Ptr<Subscription> subPtr,
                     Ptr<Subscriber> ptr)
{
  if (!ptr.isNull())
  {
    jam();

    SubStartReq* req = (SubStartReq*)signal->getDataPtrSend();
    req->senderRef        = reference();
    req->senderData       = ptr.i;
    req->subscriptionId   = subPtr.p->m_subscriptionId;
    req->subscriptionKey  = subPtr.p->m_subscriptionKey;
    req->part             = SubscriptionData::TableData;
    req->subscriberData   = ptr.p->m_senderData;
    req->subscriberRef    = ptr.p->m_senderRef;

    sendSignal(c_restart.m_ref, GSN_SUB_START_REQ,
               signal, SubStartReq::SignalLength, JBB);
    return;
  }
  else
  {
    // remove lock from this subscription
    Ptr<SubOpRecord> subOpPtr;
    c_subOpPool.getPtr(subOpPtr, c_restart.m_subOpPtrI);
    check_remove_queue(signal, subPtr, subOpPtr, true, false);
    check_release_subscription(signal, subPtr);

    DLHashTable<Subscription>::Iterator it;
    it.curr = subPtr;
    it.bucket = c_restart.m_bucket;
    c_subscriptions.next(it);
    copySubscription(signal, it);
  }
}

void 
Suma::execSUB_START_CONF(Signal* signal)
{
  jamEntry();

  SubStartConf * const conf = (SubStartConf*)signal->getDataPtr();

  Ptr<Subscription> subPtr;
  c_subscriptionPool.getPtr(subPtr, c_restart.m_subPtrI);

  Ptr<Subscriber> ptr;
  c_subscriberPool.getPtr(ptr, conf->senderData);

  LocalDLList<Subscriber> list(c_subscriberPool, subPtr.p->m_subscribers);
  list.next(ptr);
  copySubscriber(signal, subPtr, ptr);
}

void
Suma::execSUB_START_REF(Signal* signal)
{
  jamEntry();

  SubStartRef * sig = (SubStartRef*)signal->getDataPtr();
  Uint32 errorCode = sig->errorCode;

  {
    SumaStartMeRef* ref= (SumaStartMeRef*)signal->getDataPtrSend();
    ref->errorCode = errorCode;
    sendSignal(c_restart.m_ref, GSN_SUMA_START_ME_REF, signal,
               SumaStartMeRef::SignalLength, JBB);
  }

  Ptr<Subscription> subPtr;
  c_subscriptionPool.getPtr(subPtr, c_restart.m_subPtrI);

  abort_start_me(signal, subPtr, true);
}

void
Suma::abort_start_me(Signal* signal, Ptr<Subscription> subPtr,
                     bool lockowner)
{
  Ptr<SubOpRecord> subOpPtr;
  c_subOpPool.getPtr(subOpPtr, c_restart.m_subOpPtrI);
  check_remove_queue(signal, subPtr, subOpPtr, lockowner, true);
  check_release_subscription(signal, subPtr);

  c_restart.m_ref = 0;
}

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

  c_alive_nodes.set(nodeId);
  
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
  m_missing_data = false;
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
  m_missing_data = false;
}

Uint32
Suma::seize_page()
{
  if (ERROR_INSERTED(13038))
  {
    jam();
    CLEAR_ERROR_INSERT_VALUE;
    ndbout_c("Simulating out of event buffer");
    m_out_of_buffer_gci = m_max_seen_gci;
  }
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
    ndbout_c("release_gci(%d, %llu) 0x%x-> node failure -> abort", 
             buck, gci, bucket->m_state);
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
      if (ERROR_INSERTED(13034))
      {
        jam();
        SET_ERROR_INSERT_VALUE(13035);
        return;
      }
      if (ERROR_INSERTED(13035))
      {
        CLEAR_ERROR_INSERT_VALUE;
        NodeReceiverGroup rg(CMVMI, c_nodes_in_nodegroup_mask);
        rg.m_nodes.clear(getOwnNodeId());
        signal->theData[0] = 9999;
        sendSignal(rg, GSN_NDB_TAMPER, signal, 1, JBA);
        return;
      }
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

  /**
   * Resend from m_max_acked_gci + 1 until max_gci + 1
   */
  Bucket* bucket= c_buckets + buck;
  Page_pos pos= bucket->m_buffer_head;

  if(m_out_of_buffer_gci)
  {
    Ptr<Gcp_record> gcp;
    c_gcp_list.last(gcp);
    signal->theData[0] = NDB_LE_SubscriptionStatus;
    signal->theData[1] = 2; // INCONSISTENT;
    signal->theData[2] = 0; // Not used
    signal->theData[3] = (Uint32) pos.m_max_gci;
    signal->theData[4] = (Uint32) (gcp.p->m_gci >> 32);
    sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 5, JBB);
    m_missing_data = true;
    return;
  }

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
  Uint64 max = m_max_seen_gci;

  ndbrequire(max <= m_max_seen_gci);

  if(min > max)
  {
    ndbrequire(pos.m_page_id == bucket->m_buffer_tail);
    m_active_buckets.set(buck);
    m_gcp_complete_rep_count ++;
    ndbout_c("empty bucket (%u/%u %u/%u) -> active", 
             Uint32(min >> 32), Uint32(min),
             Uint32(max >> 32), Uint32(max));
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
      rep->flags = (m_missing_data)
                   ? SubGcpCompleteRep::MISSING_DATA
                   : 0;
      rep->senderRef  = reference();
      rep->gcp_complete_rep_count = 1;

      if (ERROR_INSERTED(13036))
      {
        jam();
        CLEAR_ERROR_INSERT_VALUE;
        ndbout_c("Simulating out of event buffer at node failure");
        rep->flags |= SubGcpCompleteRep::MISSING_DATA;
      }
  
      char buf[255];
      c_subscriber_nodes.getText(buf);
      if (g_cnt)
      {      
        ndbout_c("resending GCI: %u/%u rows: %d -> %s", 
                 Uint32(last_gci >> 32), Uint32(last_gci), g_cnt, buf);
      }
      g_cnt = 0;
      
      NodeReceiverGroup rg(API_CLUSTERMGR, c_subscriber_nodes);
      sendSignal(rg, GSN_SUB_GCP_COMPLETE_REP, signal,
		 SubGcpCompleteRep::SignalLength, JBB);
    } 
    else
    {
      const uint buffer_header_sz = 4;
      g_cnt++;
      Uint32 subPtrI = * src++ ;
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
      Ptr<Subscription> subPtr;
      c_subscriptionPool.getPtr(subPtr, subPtrI);
      Ptr<Table> tabPtr;
      c_tablePool.getPtr(tabPtr, subPtr.p->m_table_ptrI);
      Uint32 table = subPtr.p->m_tableId;
      if (table_version_major(tabPtr.p->m_schemaVersion) ==
          table_version_major(schemaVersion))
      {
	SubTableData * data = (SubTableData*)signal->getDataPtrSend();//trg;
	data->gci_hi         = last_gci >> 32;
	data->gci_lo         = last_gci & 0xFFFFFFFF;
	data->tableId        = table;
	data->requestInfo    = 0;
	SubTableData::setOperation(data->requestInfo, event);
	data->flags          = 0;
	data->anyValue       = any_value;
	data->totalLen       = ptrLen;
	
	{
          LocalDLList<Subscriber> list(c_subscriberPool,
                                       subPtr.p->m_subscribers);
          SubscriberPtr subbPtr;
          for(list.first(subbPtr); !subbPtr.isNull(); list.next(subbPtr))
          {
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

void
Suma::execGCP_PREPARE(Signal *signal)
{
  jamEntry();
  const GCPPrepare *prep = (const GCPPrepare *)signal->getDataPtr();
  m_current_gci = prep->gci_lo | (Uint64(prep->gci_hi) << 32);
}

Uint64
Suma::get_current_gci(Signal*)
{
  return m_current_gci;
}

template void append(DataBuffer<11>&,SegmentedSectionPtr,SectionSegmentPool&);


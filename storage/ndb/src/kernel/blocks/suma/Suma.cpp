/*
   Copyright (c) 2003, 2015, Oracle and/or its affiliates. All rights reserved.

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
#include <signaldata/StopMe.hpp>

#include <signaldata/DictLock.hpp>
#include <ndbapi/NdbDictionary.hpp>

#include <DebuggerNames.hpp>
#include "../dbtup/Dbtup.hpp"
#include "../dbdih/Dbdih.hpp"

#include <signaldata/CreateNodegroup.hpp>
#include <signaldata/CreateNodegroupImpl.hpp>

#include <signaldata/DropNodegroup.hpp>
#include <signaldata/DropNodegroupImpl.hpp>

#include <signaldata/DbinfoScan.hpp>
#include <signaldata/TransIdAI.hpp>
#include <signaldata/DumpStateOrd.hpp>

#include <ndb_version.h>
#include <EventLogger.hpp>

#define JAM_FILE_ID 467

extern EventLogger * g_eventLogger;

//#define HANDOVER_DEBUG
//#define NODEFAIL_DEBUG
//#define NODEFAIL_DEBUG2
//#define DEBUG_SUMA_SEQUENCE
//#define EVENT_DEBUG
//#define EVENT_PH3_DEBUG
//#define EVENT_DEBUG2
#if 1
#undef DBUG_ENTER
#undef DBUG_PRINT
#undef DBUG_RETURN
#undef DBUG_VOID_RETURN

#if 0
#define DBUG_ENTER(a) {ndbout_c("%s:%d >%s", __FILE__, __LINE__, a);}
#define DBUG_PRINT(a,b) {ndbout << __FILE__ << ":" << __LINE__ << " " << a << ": "; ndbout_c b ;}
#define DBUG_RETURN(a) { ndbout_c("%s:%d <", __FILE__, __LINE__); return(a); }
#define DBUG_VOID_RETURN { ndbout_c("%s:%d <", __FILE__, __LINE__); return; }
#else
#define DBUG_ENTER(a)
#define DBUG_PRINT(a,b)
#define DBUG_RETURN(a) return a
#define DBUG_VOID_RETURN return
#endif

#endif

#define DBG_3R 0

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
  ndb_mgm_get_int_parameter(p, CFG_DICT_TABLE,
			    &noTables);
  ndb_mgm_get_int_parameter(p, CFG_DICT_ATTRIBUTE,
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

  // Trix: max 5 concurrent index stats ops with max 9 words bounds
  Uint32 noOfBoundWords = 5 * 9;

  // XXX multiplies number of words by 15 ???
  c_dataBufferPool.setSize(noAttrs + noOfBoundWords);

  c_maxBufferedEpochs = maxBufferedEpochs;
  infoEvent("Buffering maximum epochs %u", c_maxBufferedEpochs);

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
  Uint32 poolSize= MAX(c_maxBufferedEpochs,
		       10 + (4*dbApiHbInterval+gcpInterval-1)/gcpInterval);
  c_gcp_pool.setSize(poolSize);

  Uint32 maxBufferedEpochBytes, numPages, numPageChunks;
  ndb_mgm_get_int_parameter(p, CFG_DB_MAX_BUFFERED_EPOCH_BYTES,
			    &maxBufferedEpochBytes);
  numPages = (maxBufferedEpochBytes + Page_chunk::CHUNK_PAGE_SIZE - 1)
             / Page_chunk::CHUNK_PAGE_SIZE;
  numPageChunks = (numPages + Page_chunk::PAGES_PER_CHUNK - 1)
                  / Page_chunk::PAGES_PER_CHUNK;
  c_page_chunk_pool.setSize(numPageChunks);
  
  {
    SLList<SyncRecord> tmp(c_syncPool);
    Ptr<SyncRecord> ptr;
    while (tmp.seizeFirst(ptr))
      new (ptr.p) SyncRecord(* this, c_dataBufferPool);
    while (tmp.releaseFirst());
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
  c_startup.m_forced_disconnect_attempted = false;
  c_failedApiNodes.clear();
  c_startup.m_wait_handover_timeout_ms = 120000; /* Default for old MGMD */
  ndb_mgm_get_int_parameter(p, CFG_DB_AT_RESTART_SUBSCRIBER_CONNECT_TIMEOUT,
                            &c_startup.m_wait_handover_timeout_ms);

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

  if (m_startphase == 3)
  {
    jam();
    void* ptr = m_ctx.m_mm.get_memroot();
    c_page_pool.set((Buffer_page*)ptr, (Uint32)~0);
  }

  if (m_startphase == 5)
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
  
  if (m_startphase == 7)
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
          g_eventLogger->info("Activating bucket %u in SUMA", i);
	}
      }
    }
    
    if (!m_active_buckets.isclear())
    {
      NdbNodeBitmask tmp;
      Uint32 bucket = 0;
      while ((bucket = m_active_buckets.find(bucket)) != Bucket_mask::NotFound)
      {
	tmp.set(get_responsible_node(bucket, c_nodes_in_nodegroup_mask));
	bucket++;
      }
      
      ndbassert(tmp.get(getOwnNodeId()));
      m_gcp_complete_rep_count = m_active_buckets.count();
    }
    else
      m_gcp_complete_rep_count = 0; // I contribute 1 gcp complete rep
    
    if (m_typeOfStart == NodeState::ST_INITIAL_START &&
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
  
  if (m_startphase == 101)
  {
    if (m_typeOfStart == NodeState::ST_NODE_RESTART ||
	m_typeOfStart == NodeState::ST_INITIAL_NODE_RESTART)
    {
      jam();
      /**
       * Handover code here
       */
      c_startup.m_wait_handover= true;
      check_start_handover(signal);
      if (c_startup.m_wait_handover)
      {
        jam();
        /**
         * Handover is waiting for some Api connections,
         * We don't want to wait indefinitely
         */
        NdbTick_Invalidate(&c_startup.m_wait_handover_message_expire);
        if (c_startup.m_wait_handover_timeout_ms == 0)
        {
          jam();
          /* Unlimited wait */
          g_eventLogger->info("Suma: handover waiting until all subscribers connected");
          NdbTick_Invalidate(&c_startup.m_wait_handover_expire);
        }
        else
        {
          jam();
          /* Bounded wait */
          NDB_TICKS now = NdbTick_getCurrentTicks();
          g_eventLogger->info("Suma: handover waiting up to %ums for all subscribers to connect", c_startup.m_wait_handover_timeout_ms);
          c_startup.m_wait_handover_expire = NdbTick_AddMilliseconds(now, c_startup.m_wait_handover_timeout_ms);
        }
        check_wait_handover_timeout(signal);
      }
      DBUG_VOID_RETURN;
    }
  }
  sendSTTORRY(signal);
  DBUG_VOID_RETURN;
}

void
Suma::send_dict_lock_req(Signal* signal, Uint32 state)
{
  if (state == DictLockReq::SumaStartMe &&
      !ndbd_suma_dictlock_startme(getNodeInfo(c_masterNodeId).m_version))
  {
    jam();
    goto notsupported;
  }
  else if (state == DictLockReq::SumaHandOver &&
           !ndbd_suma_dictlock_handover(getNodeInfo(c_masterNodeId).m_version))
  {
    jam();
    goto notsupported;
  }

  {
    jam();
    DictLockReq* req = (DictLockReq*)signal->getDataPtrSend();
    req->lockType = state;
    req->userPtr = state;
    req->userRef = reference();
    sendSignal(calcDictBlockRef(c_masterNodeId),
               GSN_DICT_LOCK_REQ, signal, DictLockReq::SignalLength, JBB);
  }
  return;

notsupported:
  DictLockConf* conf = (DictLockConf*)signal->getDataPtrSend();
  conf->userPtr = state;
  execDICT_LOCK_CONF(signal);
}

void
Suma::execDICT_LOCK_CONF(Signal* signal)
{
  jamEntry();

  DictLockConf* conf = (DictLockConf*)signal->getDataPtr();
  Uint32 state = conf->userPtr;

  switch(state){
  case DictLockReq::SumaStartMe:
    jam();
    c_startup.m_restart_server_node_id = 0;
    CRASH_INSERTION(13039);
    send_start_me_req(signal);
    return;
  case DictLockReq::SumaHandOver:
    jam();
    send_handover_req(signal, SumaHandoverReq::RT_START_NODE);
    return;
  default:
    jam();
    jamLine(state);
    ndbrequire(false);
  }
}

void
Suma::execDICT_LOCK_REF(Signal* signal)
{
  jamEntry();

  DictLockRef* ref = (DictLockRef*)signal->getDataPtr();
  Uint32 state = ref->userPtr;

  ndbrequire(ref->errorCode == DictLockRef::TooManyRequests);
  signal->theData[0] = SumaContinueB::RETRY_DICT_LOCK;
  signal->theData[1] = state;
  sendSignalWithDelay(reference(), GSN_CONTINUEB, signal, 300, 2);
}

void
Suma::send_dict_unlock_ord(Signal* signal, Uint32 state)
{
  if (state == DictLockReq::SumaStartMe &&
      !ndbd_suma_dictlock_startme(getNodeInfo(c_masterNodeId).m_version))
  {
    jam();
    return;
  }
  else if (state == DictLockReq::SumaHandOver &&
           !ndbd_suma_dictlock_handover(getNodeInfo(c_masterNodeId).m_version))
  {
    jam();
    return;
  }

  jam();
  DictUnlockOrd* ord = (DictUnlockOrd*)signal->getDataPtrSend();
  ord->lockPtr = 0;
  ord->lockType = state;
  ord->senderData = state;
  ord->senderRef = reference();
  sendSignal(calcDictBlockRef(c_masterNodeId),
             GSN_DICT_UNLOCK_ORD, signal, DictUnlockOrd::SignalLength, JBB);
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
  send_dict_unlock_ord(signal, DictLockReq::SumaStartMe);
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

  if (DBG_3R)
  {
    for (Uint32 i = 0; i<MAX_NDB_NODES; i++)
    {
      if (c_alive_nodes.get(i))
        ndbout_c("%u c_alive_nodes.set(%u)", __LINE__, i);
    }
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

static
bool
valid_seq(Uint32 n, Uint32 r, Uint16 dst[])
{
  Uint16 tmp[MAX_REPLICAS];
  for (Uint32 i = 0; i<r; i++)
  {
    tmp[i] = n % r;
    for (Uint32 j = 0; j<i; j++)
      if (tmp[j] == tmp[i])
        return false;
    n /= r;
  }

  /**
   * reverse order for backward compatibility (with 2 replica)
   */
  for (Uint32 i = 0; i<r; i++)
    dst[i] = tmp[r-i-1];

  return true;
}

void
Suma::calculate_sub_data_stream(Uint16 bucket, Uint16 buckets, Uint16 replicas)
{
  ndbassert(bucket < NO_OF_BUCKETS);
  Bucket* ptr = c_buckets + bucket;

  // First responsible node, irrespective of it is up or not
  const Uint16 node = ptr->m_nodes[0];
  ndbassert(node >= 1);
  ndbassert(node <= MAX_SUB_DATA_STREAM_GROUPS);
  const Uint16 buckets_per_node = buckets/replicas;
  ndbassert(buckets_per_node <= MAX_SUB_DATA_STREAMS_PER_GROUP);
  const Uint16 sub_data_stream = (node << 8) | (bucket % buckets_per_node);

#ifdef VM_TRACE
  // Verify that this blocks sub data stream identifiers are unique.
  for (Uint32 i = 0; i < bucket; i++)
  {
    ndbassert(c_buckets[i].m_sub_data_stream != sub_data_stream);
  }
#endif

  ptr->m_sub_data_stream = sub_data_stream;
}

void
Suma::fix_nodegroup()
{
  Uint32 i, pos= 0;
  
  for (i = 0; i < MAX_NDB_NODES; i++)
  {
    if (c_nodes_in_nodegroup_mask.get(i))
    {
      c_nodesInGroup[pos++] = i;
    }
  }
  
  const Uint32 replicas= c_noNodesInGroup = pos;

  if (replicas)
  {
    Uint32 buckets= 1;
    for(i = 1; i <= replicas; i++)
      buckets *= i;

    Uint32 tot = 0;
    switch(replicas){
    case 1:
      tot = 1;
      break;
    case 2:
      tot = 4; // 2^2
      break;
    case 3:
      tot = 27; // 3^3
      break;
    case 4:
      tot = 256; // 4^4
      break;
      ndbrequire(false);
    }
    Uint32 cnt = 0;
    for (i = 0; i<tot; i++)
    {
      Bucket* ptr= c_buckets + cnt;
      if (valid_seq(i, replicas, ptr->m_nodes))
      {
        jam();
        if (DBG_3R) printf("bucket %u : ", cnt);
        for (Uint32 j = 0; j<replicas; j++)
        {
          ptr->m_nodes[j] = c_nodesInGroup[ptr->m_nodes[j]];
          if (DBG_3R) printf("%u ", ptr->m_nodes[j]);
        }
        if (DBG_3R) printf("\n");
        calculate_sub_data_stream(cnt, buckets, replicas);
        cnt++;
      }
    }
    ndbrequire(cnt == buckets);
    c_no_of_buckets= buckets;
  }
  else
  {
    jam();
    c_no_of_buckets = 0;
  }
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

  fix_nodegroup();

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
    
    send_dict_lock_req(signal, DictLockReq::SumaStartMe);

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

    if (c_no_of_buckets)
    {
      jam();
      send_dict_lock_req(signal, DictLockReq::SumaHandOver);
    }
    else
    {
      jam();
      sendSTTORRY(signal);
    }
  }
}

void
Suma::check_wait_handover_timeout(Signal* signal)
{
  jam();
  if (c_startup.m_wait_handover)
  {
    jam();
    /* Still waiting */

    /* Send CONTINUEB for next check... */
    signal->theData[0] = SumaContinueB::HANDOVER_WAIT_TIMEOUT;
    sendSignalWithDelay(reference(), GSN_CONTINUEB, signal, 1000, 1);

    /* Now check whether we should do something more */
    NDB_TICKS now = NdbTick_getCurrentTicks();
    if(NdbTick_IsValid(c_startup.m_wait_handover_expire))
    {
      jam();

      /* Wait is bounded... has it expired? */
      if (NdbTick_Compare(c_startup.m_wait_handover_expire, now) >= 0)
      {
        jam();

        /* Not expired, consider a log message, then wait some more */
        check_wait_handover_message(now);
        return;
      }

      /* Wait time has expired */
      NdbTick_Invalidate(&c_startup.m_wait_handover_expire);

      NodeBitmask subscribers_not_connected;
      subscribers_not_connected.assign(c_subscriber_nodes);
      subscribers_not_connected.bitANDC(c_connected_nodes);

      if(!subscribers_not_connected.isclear())
      {
        jam();
        if (!c_startup.m_forced_disconnect_attempted)
        {
          // Disconnect API nodes subscribing but not connected
          jam();
          Uint32 nodeId = 0;
          while((nodeId = subscribers_not_connected.find_next(nodeId + 1)) < MAX_NODES)
          {
            jam();
            // Disconnecting node
            signal->theData[0] = NDB_LE_SubscriptionStatus;
            signal->theData[1] = 3; // NOTCONNECTED;
            signal->theData[2] = nodeId;
            signal->theData[3] = (c_startup.m_wait_handover_timeout_ms + 999) / 1000;
            sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 4, JBB);

            // Same message to data node log file
            LogLevel ll;
            ll.setLogLevel(LogLevel::llError, 15);
            g_eventLogger->log(NDB_LE_SubscriptionStatus,
                               signal->theData,
                               signal->getLength(),
                               getOwnNodeId(),
                               &ll);

            /**
             * Force API_FAILREQ
             */
            if (ERROR_INSERTED(13048))
            {
              g_eventLogger->info("Skipping forced disconnect of %u",
                                  nodeId);
            }
            else
            {
              signal->theData[0] = nodeId;
              sendSignal(QMGR_REF, GSN_API_FAILREQ, signal, 1, JBB);
            }
          }

          /* Restart timing checks, but if we expire again
           * then we will shut down
           */
          c_startup.m_forced_disconnect_attempted = true;

          NDB_TICKS now = NdbTick_getCurrentTicks();
          c_startup.m_wait_handover_expire = NdbTick_AddMilliseconds(now, c_startup.m_wait_handover_timeout_ms);
        }
        else
        {
          jam();
          /* We already tried forcing a disconnect, and it failed
           * to get all subscribers connected.  Shutdown
           */
          g_eventLogger->critical("Failed to establish direct connection to all subscribers, shutting down.  (%s)",
                                  BaseString::getPrettyTextShort(subscribers_not_connected).c_str());
          CRASH_INSERTION(13048);
          progError(__LINE__,
                    NDBD_EXIT_GENERIC,
                    "Failed to establish direct connection to all subscribers");
        }
      }
      else
      {
        /* Why are we waiting if there are no disconnected subscribers? */
        g_eventLogger->critical("Subscriber nodes : %s", BaseString::getPrettyTextShort(c_subscriber_nodes).c_str());
        g_eventLogger->critical("Connected nodes  : %s", BaseString::getPrettyTextShort(c_connected_nodes).c_str());
        ndbrequire(false);
      }
    }
    else
    {
      /* Unbounded wait, display message */
      check_wait_handover_message(now);
    }
  }
}

void
Suma::check_wait_handover_message(NDB_TICKS now)
{
  jam();

  NodeBitmask subscribers_not_connected;
  subscribers_not_connected.assign(c_subscriber_nodes);
  subscribers_not_connected.bitANDC(c_connected_nodes);

  if (!NdbTick_IsValid(c_startup.m_wait_handover_message_expire) ||   // First time
      NdbTick_Compare(c_startup.m_wait_handover_message_expire, now) < 0)  // Time is up
  {
    jam();
    if (NdbTick_IsValid(c_startup.m_wait_handover_expire))
    {
      /* Bounded wait */
      ndbassert(NdbTick_Compare(c_startup.m_wait_handover_expire, now) >= 0);
      NdbDuration time_left = NdbTick_Elapsed(now, c_startup.m_wait_handover_expire);
      Uint64 milliseconds_left = time_left.milliSec();
      g_eventLogger->info("Start phase 101 waiting %us for absent subscribers to connect : %s",
                          (unsigned)((milliseconds_left + 999) / 1000),
                          BaseString::getPrettyTextShort(subscribers_not_connected).c_str());
      if (milliseconds_left > 0)
      { // Plan next message on next even 10s multiple before wait handover expire
        c_startup.m_wait_handover_message_expire = NdbTick_AddMilliseconds(now, (milliseconds_left - 1) % 10000 + 1);
      }
      else
      {
        c_startup.m_wait_handover_message_expire = now;
      }
    }
    else
    {
      /* Unbounded wait, show progress */
      g_eventLogger->info("Start phase 101 waiting for absent subscribers to connect : %s",
                          BaseString::getPrettyTextShort(subscribers_not_connected).c_str());
      c_startup.m_wait_handover_message_expire = NdbTick_AddMilliseconds(now, 10000);
    }
  }
}

void
Suma::send_handover_req(Signal* signal, Uint32 type)
{
  jam();
  c_startup.m_handover_nodes.assign(c_alive_nodes);
  c_startup.m_handover_nodes.bitAND(c_nodes_in_nodegroup_mask);
  c_startup.m_handover_nodes.clear(getOwnNodeId());
  Uint32 gci= Uint32(m_last_complete_gci >> 32) + 3;
  
  SumaHandoverReq* req= (SumaHandoverReq*)signal->getDataPtrSend();
  char buf[255];
  c_startup.m_handover_nodes.getText(buf);
  infoEvent("Suma: initiate handover for %s with nodes %s GCI: %u",
            (type == SumaHandoverReq::RT_START_NODE ? "startup" : "shutdown"),
            buf,
            gci);

  req->gci = gci;
  req->nodeId = getOwnNodeId();
  req->requestType = type;

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
  signal->theData[7] = 101;
  signal->theData[8] = 255; // No more start phases from missra
  sendSignal(NDBCNTR_REF, GSN_STTORRY, signal, 9, JBB);
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
    send_dict_lock_req(signal, signal->theData[1]);
    return;
  case SumaContinueB::HANDOVER_WAIT_TIMEOUT:
    jam();
    check_wait_handover_timeout(signal);
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
  ndbrequire(signal->theData[1] == QMGR_REF); // As callback hard-codes QMGR

  c_connected_nodes.clear(failedApiNode);

  if (c_failedApiNodes.get(failedApiNode))
  {
    jam();
    /* Being handled already, just conf */
    goto CONF;
  }

  if (!c_subscriber_nodes.get(failedApiNode))
  {
    jam();
    /* No Subscribers on that node, no SUMA 
     * specific work to do
     */
    goto BLOCK_CLEANUP;
  }

  c_failedApiNodes.set(failedApiNode);
  c_subscriber_nodes.clear(failedApiNode);
  c_subscriber_per_node[failedApiNode] = 0;
  c_failedApiNodesState[failedApiNode] = __LINE__;
  
  check_start_handover(signal);

  signal->theData[0] = SumaContinueB::API_FAIL_GCI_LIST;
  signal->theData[1] = failedApiNode;
  sendSignal(SUMA_REF, GSN_CONTINUEB, signal, 2, JBB);
  return;

BLOCK_CLEANUP:
  jam();
  api_fail_block_cleanup(signal, failedApiNode);
  DBUG_VOID_RETURN;

CONF:
  jam();
  signal->theData[0] = failedApiNode;
  signal->theData[1] = reference();
  sendSignal(QMGR_REF, GSN_API_FAILCONF, signal, 2, JBB);

  c_failedApiNodesState[failedApiNode] = 0;

  DBUG_VOID_RETURN;
}//execAPI_FAILREQ()

void
Suma::api_fail_block_cleanup_callback(Signal* signal,
                                      Uint32 failedNodeId,
                                      Uint32 elementsCleaned)
{
  jamEntry();

  /* Suma should not have any block level elements
   * to be cleaned (Fragmented send/receive structures etc.)
   * As it only uses Fragmented send/receive locally
   */
  ndbassert(elementsCleaned == 0);

  /* Node failure handling is complete */
  signal->theData[0] = failedNodeId;
  signal->theData[1] = reference();
  sendSignal(QMGR_REF, GSN_API_FAILCONF, signal, 2, JBB);
  c_failedApiNodes.clear(failedNodeId);
  c_failedApiNodesState[failedNodeId] = 0;
}

void
Suma::api_fail_block_cleanup(Signal* signal, Uint32 failedNode)
{
  jam();

  c_failedApiNodesState[failedNode] = __LINE__;

  Callback cb = {safe_cast(&Suma::api_fail_block_cleanup_callback),
                 failedNode};

  simBlockNodeFailure(signal, failedNode, cb);
}

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

      c_failedApiNodesState[nodeId] = __LINE__;
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
    c_failedApiNodesState[nodeId] = __LINE__;
    signal->theData[2] = subOpPtr.i;
    sendSignal(SUMA_REF, GSN_CONTINUEB, signal, 6, JBB);
  }
  else
  {
    c_failedApiNodesState[nodeId] = __LINE__;
    sendSignal(SUMA_REF, GSN_CONTINUEB, signal, 3, JBB);
  }

  return;
}

void
Suma::api_fail_subscriber_list(Signal* signal, Uint32 nodeId)
{
  jam();
  Ptr<SubOpRecord> subOpPtr;

  if (c_outstanding_drop_trig_req > 9)
  {
    jam();
    /**
     * Make sure not to overflow DbtupProxy with too many GSN_DROP_TRIG_IMPL_REQ
     *   9 is arbitrary number...
     */
    sendSignalWithDelay(reference(), GSN_CONTINUEB, signal, 100,
                        signal->getLength());
    return;
  }

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
      c_failedApiNodesState[nodeId] = __LINE__;
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
    c_failedApiNodesState[nodeId] = __LINE__;
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
      c_failedApiNodesState[nodeId] = __LINE__;
    }
    else
    {
      iter.bucket = bucket;
    }
  }

  if (iter.curr.isNull())
  {
    jam();
    api_fail_block_cleanup(signal, nodeId);
    c_failedApiNodesState[nodeId] = __LINE__;
    return;
  }

  subOpPtr.p->m_opType = SubOpRecord::R_API_FAIL_REQ;
  subOpPtr.p->m_subPtrI = iter.curr.i;
  subOpPtr.p->m_senderRef = nodeId;
  subOpPtr.p->m_senderData = iter.bucket;

  LocalDLFifoList<SubOpRecord> list(c_subOpPool, iter.curr.p->m_stop_req);
  bool empty = list.isEmpty();
  list.addLast(subOpPtr);

  if (empty)
  {
    jam();
    c_failedApiNodesState[nodeId] = __LINE__;
    signal->theData[0] = SumaContinueB::API_FAIL_SUBSCRIPTION;
    signal->theData[1] = subOpPtr.i;
    signal->theData[2] = RNIL;
    sendSignal(SUMA_REF, GSN_CONTINUEB, signal, 3, JBB);
  }
  else
  {
    jam();
    c_failedApiNodesState[nodeId] = __LINE__;
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
    c_failedApiNodesState[nodeId] = __LINE__;
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
    jam();
    c_failedApiNodesState[nodeId] = __LINE__;
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

  /* Now do block level cleanup */
  api_fail_block_cleanup(signal, nodeId);
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
        else if (state & Bucket::BUCKET_SHUTDOWN_TO)
        {
          jam();
          c_buckets[i].m_state &= ~Uint32(Bucket::BUCKET_SHUTDOWN_TO);
          m_switchover_buckets.clear(i);
          ndbrequire(get_responsible_node(i, tmp) == getOwnNodeId());
          start_resend(signal, i);
        }
      }
      else if(get_responsible_node(i, tmp) == getOwnNodeId())
      {
	start_resend(signal, i);
      }
    }
  }

  /* Block level cleanup */
  for(unsigned i = 1; i < MAX_NDB_NODES; i++) {
    jam();
    if(failed.get(i)) {
      jam();
      Uint32 elementsCleaned = simBlockNodeFailure(signal, i); // No callback
      ndbassert(elementsCleaned == 0); // As Suma has no remote fragmented signals
      (void) elementsCleaned; // Avoid compiler error
    }//if
  }//for
  
  c_alive_nodes.assign(tmp);
  
  DBUG_VOID_RETURN;
}

void
Suma::execINCL_NODEREQ(Signal* signal){
  jamEntry();
  
  const Uint32 senderRef = signal->theData[0];
  const Uint32 nodeId  = signal->theData[1];

  ndbrequire(!c_alive_nodes.get(nodeId));
  if (c_nodes_in_nodegroup_mask.get(nodeId))
  {
    /**
     *
     * XXX TODO: This should be removed
     *           But, other nodes are (incorrectly) reported as started
     *                even if they're not "started", but only INCL_NODEREQ'ed
     */
    c_alive_nodes.set(nodeId);

    /**
     *
     * Nodes in nodegroup will be "alive" when
     *   sending SUMA_HANDOVER_REQ
     */
  }
  else
  {
    jam();
    c_alive_nodes.set(nodeId);
  }
  
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
static
const char*
cstr(Suma::Subscription::State s)
{
  switch(s){
  case Suma::Subscription::UNDEFINED:
    return "undefined";
  case Suma::Subscription::DEFINED:
    return "defined";
  case Suma::Subscription::DEFINING:
    return "defining";
  }
  return "<unknown>";
}

static
const char*
cstr(Suma::Subscription::TriggerState s)
{
  switch(s){
  case Suma::Subscription::T_UNDEFINED:
    return "undefined";
  case Suma::Subscription::T_CREATING:
    return "creating";
  case Suma::Subscription::T_DEFINED:
    return "defined";
  case Suma::Subscription::T_DROPPING:
    return "dropping";
  case Suma::Subscription::T_ERROR:
    return "error";
  }
  return "<uknown>";
}

static
const char*
cstr(Suma::Subscription::Options s)
{
  static char buf[256];
  buf[0] = 0;
  strcat(buf, "[");
  if (s & Suma::Subscription::REPORT_ALL)
    strcat(buf, " reportall");
  if (s & Suma::Subscription::REPORT_SUBSCRIBE)
    strcat(buf, " reportsubscribe");
  if (s & Suma::Subscription::MARKED_DROPPED)
    strcat(buf, " dropped");
  if (s & Suma::Subscription::NO_REPORT_DDL)
    strcat(buf, " noreportddl");
  strcat(buf, " ]");
  return buf;
}

static
const char*
cstr(Suma::Table::State s)
{
  switch(s){
  case Suma::Table::UNDEFINED:
    return "undefined";
  case Suma::Table::DEFINING:
    return "defining";
  case Suma::Table::DEFINED:
    return "defined";
  case Suma::Table::DROPPED:
    return "dropped";
  }
  return "<unknown>";
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
      infoEvent("Bucket %d %d%d-%x switch gci: %llu max_acked_gci: %llu max_gci: %llu tail: %d head: %d",
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

  if (tCase == 8012)
  {
    jam();
    Uint32 bucket = signal->theData[1];
    KeyTable<Subscription>::Iterator it;
    if (signal->getLength() == 1)
    {
      jam();
      bucket = 0;
      infoEvent("-- Starting dump of subscribers --");
    }

    c_subscriptions.next(bucket, it);
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

      Ptr<Subscription> subPtr = it.curr;
      Ptr<Table> tabPtr;
      c_tablePool.getPtr(tabPtr, subPtr.p->m_table_ptrI);
      infoEvent("Subcription %u id: 0x%.8x key: 0x%.8x state: %s",
                subPtr.i,
                subPtr.p->m_subscriptionId,
                subPtr.p->m_subscriptionKey,
                cstr(subPtr.p->m_state));
      infoEvent("  trigger state: %s options: %s",
                cstr(subPtr.p->m_trigger_state),
                cstr((Suma::Subscription::Options)subPtr.p->m_options));
      infoEvent("  tablePtr: %u tableId: %u schemaVersion: 0x%.8x state: %s",
                tabPtr.i,
                subPtr.p->m_tableId,
                tabPtr.p->m_schemaVersion,
                cstr(tabPtr.p->m_state));
      {
        Ptr<Subscriber> ptr;
        LocalDLList<Subscriber> list(c_subscriberPool,
                                     subPtr.p->m_subscribers);
        for (list.first(ptr); !ptr.isNull(); list.next(ptr), i++)
        {
          jam();
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
      c_subscriptions.next(it);
    }

    signal->theData[0] = tCase;
    signal->theData[1] = it.bucket;
    sendSignalWithDelay(reference(), GSN_DUMP_STATE_ORD, signal, 100, 2);
    return;
  }

  if (tCase == 8013)
  {
    jam();
    Ptr<Gcp_record> gcp;
    infoEvent("-- Starting dump of pending subscribers --");
    infoEvent("Highest epoch %llu, oldest epoch %llu", m_max_seen_gci, m_last_complete_gci); 
    if (!c_gcp_list.isEmpty())
    {
      jam();
      c_gcp_list.first(gcp);
      infoEvent("Waiting for acknowledge of epoch %llu, buffering %u epochs", gcp.p->m_gci, c_gcp_list.count());
      NodeBitmask subs = gcp.p->m_subscribers;
      for(Uint32 nodeId = 0; nodeId < MAX_NODES; nodeId++)
      {
	if (subs.get(nodeId))
        {
	  jam();
	  infoEvent("Waiting for subscribing node %u", nodeId);
	}
      }
    }
    infoEvent("-- End dump of pending subscribers --");
  }

  if (tCase == DumpStateOrd::DihTcSumaNodeFailCompleted &&
      signal->getLength() == 2)
  {
    jam();
    Uint32 nodeId = signal->theData[1];
    if (nodeId < MAX_NODES)
    {
      warningEvent(" Suma %u %u line: %u", tCase, nodeId,
                   c_failedApiNodesState[nodeId]);
      warningEvent("   c_connected_nodes.get(): %u",
                   c_connected_nodes.get(nodeId));
      warningEvent("   c_failedApiNodes.get(): %u",
                   c_failedApiNodes.get(nodeId));
      warningEvent("   c_subscriber_nodes.get(): %u",
                   c_subscriber_nodes.get(nodeId));
      warningEvent(" c_subscriber_per_node[%u]: %u",
                   nodeId, c_subscriber_per_node[nodeId]);
    }
    else
    {
      warningEvent(" SUMA: dump-%u to unknown node: %u", tCase, nodeId);
    }
  }
}

void Suma::execDBINFO_SCANREQ(Signal *signal)
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
      { "Subscriber",
        c_subscriberPool.getUsed(),
        c_subscriberPool.getSize(),
        c_subscriberPool.getEntrySize(),
        c_subscriberPool.getUsedHi(),
        { CFG_DB_SUBSCRIBERS,
          CFG_DB_SUBSCRIPTIONS,
          CFG_DB_NO_TABLES,0 }},
      { "Table",
        c_tablePool.getUsed(),
        c_tablePool.getSize(),
        c_tablePool.getEntrySize(),
        c_tablePool.getUsedHi(),
        { CFG_DB_NO_TABLES,0,0,0 }},
      { "Subscription",
        c_subscriptionPool.getUsed(),
        c_subscriptionPool.getSize(),
        c_subscriptionPool.getEntrySize(),
        c_subscriptionPool.getUsedHi(),
        { CFG_DB_SUBSCRIPTIONS,
          CFG_DB_NO_TABLES,0,0 }},
      { "Sync",
        c_syncPool.getUsed(),
        c_syncPool.getSize(),
        c_syncPool.getEntrySize(),
        c_syncPool.getUsedHi(),
        { 0,0,0,0 }},
      { "Data Buffer",
        c_dataBufferPool.getUsed(),
        c_dataBufferPool.getSize(),
        c_dataBufferPool.getEntrySize(),
        c_dataBufferPool.getUsedHi(),
        { CFG_DB_NO_ATTRIBUTES,0,0,0 }},
      { "SubOp",
        c_subOpPool.getUsed(),
        c_subOpPool.getSize(),
        c_subOpPool.getEntrySize(),
        c_subOpPool.getUsedHi(),
        { CFG_DB_SUB_OPERATIONS,0,0,0 }},
      { "Page Chunk",
        c_page_chunk_pool.getUsed(),
        c_page_chunk_pool.getSize(),
        c_page_chunk_pool.getEntrySize(),
        c_page_chunk_pool.getUsedHi(),
        { 0,0,0,0 }},
      { "GCP",
        c_gcp_pool.getUsed(),
        c_gcp_pool.getSize(),
        c_gcp_pool.getEntrySize(),
        c_gcp_pool.getUsedHi(),
        { CFG_DB_API_HEARTBEAT_INTERVAL,
          CFG_DB_GCP_INTERVAL,0,0 }},
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
  if (err == UtilSequenceRef::TCError)
  {
    jam();
    err = ref->TCErrorCode;
  }
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
  const Uint32 noReportDDL = (flags & SubCreateReq::NoReportDDL) ?
    Subscription::NO_REPORT_DDL : 0;
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
                     SubCreateRef::NotStarted);
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
    subPtr.p->m_options          = reportSubscribe | reportAll | noReportDDL;
    subPtr.p->m_schemaTransId    = schemaTransId;
  }

  Ptr<SubOpRecord> subOpPtr;
  LocalDLFifoList<SubOpRecord> subOpList(c_subOpPool, subPtr.p->m_create_req);
  if ((ERROR_INSERTED(13044) && found == false) ||
      subOpList.seizeLast(subOpPtr) == false)
  {
    jam();
    if (found == false)
    {
      jam();
      if (ERROR_INSERTED(13044))
      {
        CLEAR_ERROR_INSERT_VALUE;
      }
      c_subscriptionPool.release(subPtr); // not yet in hash
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
    if (ERROR_INSERTED(13045) || c_tablePool.seize(tabPtr) == false)
    {
      jam();
      if (ERROR_INSERTED(13045))
      {
        CLEAR_ERROR_INSERT_VALUE;
      }

      subOpList.release(subOpPtr);
      c_subscriptionPool.release(subPtr); // not yet in hash
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
    list.addFirst(subPtr);
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
  if (!list.seizeFirst(syncPtr))
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
  syncPtr.p->m_frag_id          = req->fragId;
  syncPtr.p->m_tableId          = subPtr.p->m_tableId;
  syncPtr.p->m_sourceInstance   = RNIL;
  syncPtr.p->m_headersSection   = RNIL;
  syncPtr.p->m_dataSection      = RNIL;

  {
    jam();
    if(handle.m_cnt > 0)
    {
      SegmentedSectionPtr ptr;
      handle.getSection(ptr, SubSyncReq::ATTRIBUTE_LIST);
      LocalDataBuffer<15> attrBuf(c_dataBufferPool, syncPtr.p->m_attributeList);
      append(attrBuf, ptr, getSectionSegmentPool());
    }
    if (req->requestInfo & SubSyncReq::RangeScan)
    {
      jam();
      ndbrequire(handle.m_cnt > 1)
      SegmentedSectionPtr ptr;
      handle.getSection(ptr, SubSyncReq::TUX_BOUND_INFO);
      LocalDataBuffer<15> boundBuf(c_dataBufferPool, syncPtr.p->m_boundInfo);
      append(boundBuf, ptr, getSectionSegmentPool());
    }
    releaseSections(handle);
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
  req->tableId = tableId;
  req->scanCookie = scanCookie;
  req->fragCnt = 1;
  req->fragItem[0].senderData = ptr.i;
  req->fragItem[0].fragId = 0;

  sendSignal(DBDIH_REF, GSN_DIH_SCAN_GET_NODES_REQ, signal,
             DihScanGetNodesReq::FixedSignalLength
             + DihScanGetNodesReq::FragItem::Length,
             JBB);

  DBUG_VOID_RETURN;
}

void
Suma::execDIH_SCAN_GET_NODES_CONF(Signal* signal)
{
  jamEntry();
  DBUG_ENTER("Suma::execDIH_SCAN_GET_NODES_CONF");

  /**
   * Assume a short signal, with a single FragItem being returned
   * as we do only single fragment requests in
   * DIH_SCAN_GET_NODES_REQs sent from SUMA.
   */
  ndbassert(signal->getNoOfSections() == 0);
  ndbassert(signal->getLength() ==
            DihScanGetNodesConf::FixedSignalLength
            + DihScanGetNodesConf::FragItem::Length);

  DihScanGetNodesConf* conf = (DihScanGetNodesConf*)signal->getDataPtr();
  const Uint32 tableId = conf->tableId;
  const Uint32 fragNo = conf->fragItem[0].fragId;
  const Uint32 nodeCount = conf->fragItem[0].count;
  ndbrequire(nodeCount > 0 && nodeCount <= MAX_REPLICAS);

  Ptr<SyncRecord> ptr;
  c_syncPool.getPtr(ptr, conf->fragItem[0].senderData);

  {
    LocalDataBuffer<15> fragBuf(c_dataBufferPool, ptr.p->m_fragments);

    /**
     * Add primary node for fragment to list
     */
    FragmentDescriptor fd;
    fd.m_fragDesc.m_nodeId = conf->fragItem[0].nodes[0];
    fd.m_fragDesc.m_fragmentNo = fragNo;
    fd.m_fragDesc.m_lqhInstanceKey = conf->fragItem[0].instanceKey;
    if (ptr.p->m_frag_id == ZNIL)
    {
      signal->theData[2] = fd.m_dummy;
      fragBuf.append(&signal->theData[2], 1);
    }
    else if (ptr.p->m_frag_id == fragNo)
    {
      /*
       * Given fragment must have a replica on this node.
       */
      const Uint32 ownNodeId = getOwnNodeId();
      Uint32 i = 0;
      for (i = 0; i < nodeCount; i++)
        if (conf->fragItem[0].nodes[i] == ownNodeId)
          break;
      if (i == nodeCount)
      {
        sendSubSyncRef(signal, 1428);
        return;
      }
      fd.m_fragDesc.m_nodeId = ownNodeId;
      signal->theData[2] = fd.m_dummy;
      fragBuf.append(&signal->theData[2], 1);
    }
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
  req->tableId = tableId;
  req->scanCookie = ptr.p->m_scan_cookie;
  req->fragCnt = 1;
  req->fragItem[0].senderData = ptr.i;
  req->fragItem[0].fragId = nextFrag;

  sendSignal(DBDIH_REF, GSN_DIH_SCAN_GET_NODES_REQ, signal,
             DihScanGetNodesReq::FixedSignalLength
             + DihScanGetNodesReq::FragItem::Length,
             JBB);

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
  if (tabPtr.p->m_state == Table::DROPPED)
  {
    jam();
    do_resend_request = 0;
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
  get_tabinfo_ref_release(signal, tabPtr);
}

void
Suma::get_tabinfo_ref_release(Signal* signal, Ptr<Table> tabPtr)
{
  LocalDLList<Subscription> subList(c_subscriptionPool,
                                    tabPtr.p->m_subscriptions);
  Ptr<Subscription> subPtr;
  ndbassert(!subList.isEmpty());
  for(subList.first(subPtr); !subPtr.isNull();)
  {
    jam();
    Ptr<SubOpRecord> ptr;
    ndbassert(subPtr.p->m_start_req.isEmpty());
    ndbassert(subPtr.p->m_stop_req.isEmpty());
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
    c_subscriptions.remove(tmp1);
    subList.release(tmp1);
  }

  c_tables.release(tabPtr);
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

  if (tabPtr.p->m_state == Table::DROPPED)
  {
    jam();
    get_tabinfo_ref_release(signal, tabPtr);
    return;
  }

  tabPtr.p->m_state = Table::DEFINED;

  LocalDLList<Subscription> subList(c_subscriptionPool,
                                    tabPtr.p->m_subscriptions);
  Ptr<Subscription> subPtr;
  ndbassert(!subList.isEmpty());
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

  Uint32 instanceKey = fd.m_fragDesc.m_lqhInstanceKey;
  BlockReference lqhRef = numberToRef(DBLQH, instanceKey, suma.getOwnNodeId());
  
  ScanFragReq * req = (ScanFragReq *)signal->getDataPtrSend();
  const Uint32 parallelism = 16;
  //const Uint32 attrLen = 5 + attrBuf.getSize();

  req->senderData = ptrI;
  req->resultRef = suma.reference();
  req->tableId = tabPtr.p->m_tableId;
  req->requestInfo = 0;
  req->savePointId = 0;
  ScanFragReq::setLockMode(req->requestInfo, 0);
  ScanFragReq::setHoldLockFlag(req->requestInfo, 1);
  ScanFragReq::setKeyinfoFlag(req->requestInfo, 0);
  if (m_requestInfo & SubSyncReq::NoDisk)
  {
    ScanFragReq::setNoDiskFlag(req->requestInfo, 1);
  }
  
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

  if (m_requestInfo & SubSyncReq::TupOrder)
  {
    ScanFragReq::setTupScanFlag(req->requestInfo, 1);
  }

  if (m_requestInfo & SubSyncReq::LM_CommittedRead)
  {
    ScanFragReq::setReadCommittedFlag(req->requestInfo, 1);
  }

  if (m_requestInfo & SubSyncReq::RangeScan)
  {
    ScanFragReq::setRangeScanFlag(req->requestInfo, 1);
  }

  if (m_requestInfo & SubSyncReq::StatScan)
  {
    ScanFragReq::setStatScanFlag(req->requestInfo, 1);
  }

  req->fragmentNoKeyLen = fd.m_fragDesc.m_fragmentNo;
  req->schemaVersion = tabPtr.p->m_schemaVersion;
  req->transId1 = 0;
  req->transId2 = (SUMA << 20) + (suma.getOwnNodeId() << 8);
  req->clientOpPtr = (ptrI << 16);
  req->batch_size_rows= parallelism;

  req->batch_size_bytes= 0;

  Uint32 * attrInfo = signal->theData + 25;
  attrInfo[0] = attrBuf.getSize();
  attrInfo[1] = 0;
  attrInfo[2] = 0;
  attrInfo[3] = 0;
  attrInfo[4] = 0;
  
  Uint32 pos = 5;
  DataBuffer<15>::DataBufferIterator it;
  for(attrBuf.first(it); !it.curr.isNull(); attrBuf.next(it))
  {
    AttributeHeader::init(&attrInfo[pos++], * it.data, 0);
  }
  LinearSectionPtr ptr[3];
  Uint32 noOfSections;
  ptr[0].p = attrInfo;
  ptr[0].sz = pos;
  noOfSections = 1;
  if (m_requestInfo & SubSyncReq::RangeScan)
  {
    jam();
    Uint32 oldpos = pos; // after attrInfo
    LocalDataBuffer<15> boundBuf(suma.c_dataBufferPool, m_boundInfo);
    for (boundBuf.first(it); !it.curr.isNull(); boundBuf.next(it))
    {
      attrInfo[pos++] = *it.data;
    }
    ptr[1].p = &attrInfo[oldpos];
    ptr[1].sz = pos - oldpos;
    noOfSections = 2;
  }
  suma.sendSignal(lqhRef, GSN_SCAN_FRAGREQ, signal, 
		  ScanFragReq::SignalLength, JBB, ptr, noOfSections);
  
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
  
  if(completed != 2){ // 2==ZSCAN_FRAG_CLOSED
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

  Uint32 instanceKey;
  {
    Ptr<SyncRecord> syncPtr;
    c_syncPool.getPtr(syncPtr, syncPtrI);
    LocalDataBuffer<15> fragBuf(c_dataBufferPool, syncPtr.p->m_fragments);
    DataBuffer<15>::DataBufferIterator fragIt;
    bool ok = fragBuf.position(fragIt, syncPtr.p->m_currentFragment);
    ndbrequire(ok);
    FragmentDescriptor tmp;
    tmp.m_dummy = * fragIt.data;
    instanceKey = tmp.m_fragDesc.m_lqhInstanceKey;
  }
  BlockReference lqhRef = numberToRef(DBLQH, instanceKey, getOwnNodeId());

  ScanFragNextReq * req = (ScanFragNextReq *)signal->getDataPtrSend();
  req->senderData = syncPtrI;
  req->requestInfo = 0;
  req->transId1 = 0;
  req->transId2 = (SUMA << 20) + (getOwnNodeId() << 8);
  req->batch_size_rows = 16;
  req->batch_size_bytes = 0;
  sendSignal(lqhRef, GSN_SCAN_NEXTREQ, signal, 
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
  SubscriptionData::Part part = (SubscriptionData::Part)req->part;
  (void)part; // TODO validate part

  Subscription key; 
  key.m_subscriptionId        = req->subscriptionId;
  key.m_subscriptionKey       = req->subscriptionKey;

  SubscriptionPtr subPtr;

  CRASH_INSERTION2(13042, getNodeState().startLevel == NodeState::SL_STARTING);
  
  if (c_startup.m_restart_server_node_id == RNIL)
  {
    jam();

    /**
     * We havent started syncing yet
     */
    sendSubStartRef(signal,
                    senderRef, senderData, SubStartRef::NotStarted);
    return;
  }

  bool found = c_subscriptions.find(subPtr, key);
  if (!found)
  {
    jam();
    sendSubStartRef(signal,
                    senderRef, senderData, SubStartRef::NoSuchSubscription);
    return;
  }

  if (ERROR_INSERTED(13046))
  {
    jam();
    CLEAR_ERROR_INSERT_VALUE;
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

  switch(getNodeInfo(refToNode(subscriberRef)).m_type){
  case NodeInfo::DB:
  case NodeInfo::API:
  case NodeInfo::MGM:
    if (!ERROR_INSERTED_CLEAR(13047))
      break;
  default:
    /**
     * This can happen if we start...with a new config
     *   that has dropped a node...that has a subscription active
     *   (or maybe internal error ??)
     *
     * If this is a node-restart, it means that we will refuse to start
     * If not, this mean that substart will simply fail...
     */
    jam();
    sendSubStartRef(signal, senderRef, senderData,
                    SubStartRef::SubscriberNodeIdUndefined);
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
    subOpList.addLast(subOpPtr);
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

    LinearSectionPtr ptr[3];
    ptr[0].p = attrMask.rep.data;
    ptr[0].sz = attrMask.getSizeInWords();
    sendSignal(DBTUP_REF, GSN_CREATE_TRIG_IMPL_REQ, 
               signal, CreateTrigImplReq::SignalLength, JBB, ptr, 1);
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
        conf->bucketCount     = c_no_of_buckets;
        conf->nodegroup       = c_nodeGroup;
        sendSignal(senderRef, GSN_SUB_START_CONF, signal,
                   SubStartConf::SignalLength, JBB);

        /**
         * Call before adding to list...
         *   cause method will (maybe) iterate thought list
         */
        bool report = subPtr.p->m_options & Subscription::REPORT_SUBSCRIBE;
        send_sub_start_stop_event(signal, ptr,NdbDictionary::Event::_TE_ACTIVE,
                                  report, list);
        
        list.addFirst(ptr);
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
        req->receiverRef = SUMA_REF;

        c_outstanding_drop_trig_req++;
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

  ndbrequire(c_outstanding_drop_trig_req);
  c_outstanding_drop_trig_req--;

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

  ndbrequire(c_outstanding_drop_trig_req);
  c_outstanding_drop_trig_req--;

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
                   senderRef, senderData, SubStopRef::NotStarted);
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
  if (list.seizeLast(subOpPtr) == false)
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
  const Uint64 gci = m_max_seen_gci;
  conf->senderRef= reference();
  conf->senderData= senderData;
  conf->gci_hi= Uint32(gci>>32);
  conf->gci_lo= Uint32(gci);
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

#define NO_LOCK_VAL        0xffffffff
#define TRIGGER_LOCK_BASE  0x00000000

static Uint32 bufferLock = NO_LOCK_VAL;
static Uint32 f_buffer[SUMA_BUF_SZ];
static Uint32 f_trigBufferSize = 0;
static Uint32 b_buffer[SUMA_BUF_SZ];
static Uint32 b_trigBufferSize = 0;

static bool clearBufferLock()
{
  if (bufferLock == NO_LOCK_VAL)
    return false;
  
  bufferLock = NO_LOCK_VAL;
  
  return true;
}

static bool setBufferLock(Uint32 lockVal)
{
  if (bufferLock != NO_LOCK_VAL)
    return false;
  
  bufferLock = lockVal;
  return true;
}

static bool setTriggerBufferLock(Uint32 triggerId)
{
  return setBufferLock(triggerId | TRIGGER_LOCK_BASE);
}

static bool checkTriggerBufferLock(Uint32 triggerId)
{
  return (bufferLock == (TRIGGER_LOCK_BASE | triggerId));
}

void
Suma::execTRANSID_AI(Signal* signal)
{
  jamEntry();
  DBUG_ENTER("Suma::execTRANSID_AI");

  CRASH_INSERTION(13015);
  TransIdAI * const data = (TransIdAI*)signal->getDataPtr();
  const Uint32 opPtrI = data->connectPtr;
  Uint32 length = signal->length() - 3;

  if (signal->getNoOfSections())
  {
    /* Copy long data into linear signal buffer */
    SectionHandle handle(this, signal);
    SegmentedSectionPtr dataPtr;
    handle.getSection(dataPtr, 0);
    length = dataPtr.sz;
    copy(data->attrData, dataPtr);
    releaseSections(handle);
  }

  Ptr<SyncRecord> syncPtr;
  c_syncPool.getPtr(syncPtr, (opPtrI >> 16));
  
  Uint32 headersSection = RNIL;
  Uint32 dataSection = RNIL;
  const Uint32 * src = &data->attrData[0];
  const Uint32 * const end = &src[length];
  
  const Uint32 attribs = syncPtr.p->m_currentNoOfAttributes;
  for(Uint32 i = 0; i<attribs; i++){
    Uint32 tmp = * src++;
    Uint32 len = AttributeHeader::getDataSize(tmp);
    
    /**
     * Separate AttributeHeaders and data in separate
     * sections
     * 
     * Note that len == 0 is legitimate, and can result in 
     * dataSection == RNIL
     */
    if (! (appendToSection(headersSection, &tmp, 1) &&
           appendToSection(dataSection, src, len)))
    {
      ErrorReporter::handleError(NDBD_EXIT_OUT_OF_LONG_SIGNAL_MEMORY,
                                 "Out of LongMessageBuffer in SUMA scan",
                                 "");
    }
    src += len;
  } 

  ndbrequire(src == end);
  ndbrequire(syncPtr.p->m_sourceInstance == RNIL);
  ndbrequire(syncPtr.p->m_headersSection == RNIL);
  ndbrequire(syncPtr.p->m_dataSection == RNIL);
  syncPtr.p->m_sourceInstance = refToInstance(signal->getSendersBlockRef());
  syncPtr.p->m_headersSection = headersSection;
  syncPtr.p->m_dataSection = dataSection;
 

  if ((syncPtr.p->m_requestInfo & SubSyncReq::LM_Exclusive) == 0)
  {
    /* Send it now */
    sendScanSubTableData(signal, syncPtr, 0);
  }

  /* Wait for KEYINFO20 */
  DBUG_VOID_RETURN;
}

void
Suma::execKEYINFO20(Signal* signal)
{
  jamEntry();
  KeyInfo20* data = (KeyInfo20*)signal->getDataPtr();

  const Uint32 opPtrI = data->clientOpPtr;
  const Uint32 takeOver = data->scanInfo_Node;

  Ptr<SyncRecord> syncPtr;
  c_syncPool.getPtr(syncPtr, (opPtrI >> 16));

  ndbrequire(syncPtr.p->m_sourceInstance ==
             refToInstance(signal->getSendersBlockRef()));
  ndbrequire(syncPtr.p->m_headersSection != RNIL);
  ndbrequire(syncPtr.p->m_dataSection != RNIL);

  sendScanSubTableData(signal, syncPtr, takeOver);
}

void
Suma::sendScanSubTableData(Signal* signal,
                           Ptr<SyncRecord> syncPtr, Uint32 takeOver)
{
  if (unlikely(syncPtr.p->m_dataSection == RNIL))
  {
    jam();
    
    /* Zero length data section, but receivers expect 
     * to get something :(
     * import() currently supports empty sections
     */
    Ptr<SectionSegment> emptySection;
    Uint32 junk = 0;
    if (!import(emptySection, &junk, 0))
    {
      ErrorReporter::handleError(NDBD_EXIT_OUT_OF_LONG_SIGNAL_MEMORY,
                                 "Out of LongMessageBuffer in SUMA scan",
                                 "");
    }
    syncPtr.p->m_dataSection = emptySection.i;
  }

  ndbassert(syncPtr.p->m_headersSection != RNIL);
  ndbassert(syncPtr.p->m_dataSection != RNIL);

  /**
   * Send data to subscriber
   */
  SectionHandle sh(this);
  sh.m_ptr[0].i = syncPtr.p->m_headersSection;
  sh.m_ptr[1].i = syncPtr.p->m_dataSection;
  getSections(2, sh.m_ptr);
  sh.m_cnt = 2;

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
  ndbout_c("GSN_SUB_TABLE_DATA (scan) #attr: %d len: %d", 
           getSectionSz(syncPtr.p->m_headersSection),
           getSectionSz(syncPtr.p->m_dataSection));
#else
  sendSignal(ref,
	     GSN_SUB_TABLE_DATA,
	     signal, 
	     SubTableData::SignalLength, JBB,
	     &sh);
#endif
  
  /* Clear section references */
  syncPtr.p->m_sourceInstance = RNIL;
  syncPtr.p->m_headersSection = RNIL;
  syncPtr.p->m_dataSection = RNIL;  
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

    ndbrequire( checkTriggerBufferLock(trigId) );

    memcpy(b_buffer + b_trigBufferSize, trg->getData(), 4 * dataLen);
    b_trigBufferSize += dataLen;

    // printf("before values %u %u %u\n",trigId, dataLen,  b_trigBufferSize);
  } else {
    jam();

    if (setTriggerBufferLock(trigId))
    {
      /* Lock was not taken, we have it now */
      f_trigBufferSize = 0;
      b_trigBufferSize = 0;
    }
    else
    {
      /* Lock was taken, must be by us */
      ndbrequire( checkTriggerBufferLock(trigId) );
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
#ifdef NODEFAIL_DEBUG2
      theCounts[node]++;
      ndbout_c("Suma:responsible n=%u, D=%u, id = %u, count=%u",
               n,D, id, theCounts[node]);
#endif
      return node;
    }
  }
  
  return 0;
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
  const Uint32 send_mask = 
    Bucket::BUCKET_STARTING |
    Bucket::BUCKET_TAKEOVER |
    Bucket::BUCKET_SHUTDOWN_TO;

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

/**
 * Pass entire pages with SUMA-trigger-data from
 *   TUP to SUMA to avoid extensive LongSignalMessage buffer contention
 */
void
Suma::execFIRE_TRIG_ORD_L(Signal* signal)
{
  jamEntry();

  ndbassert(signal->getNoOfSections() == 0);
  Uint32 pageId = signal->theData[0];
  Uint32 len = signal->theData[1];

  if (pageId == RNIL && len == 0)
  {
    jam();
    /**
     * Out of memory
     */
    out_of_buffer(signal);
    return;
  }

  Uint32 * ptr = reinterpret_cast<Uint32*>(c_page_pool.getPtr(pageId));
  while (len)
  {
    Uint32 * save = ptr;
    Uint32 msglen  = * ptr++;
    Uint32 siglen  = * ptr++;
    Uint32 sec0len = * ptr++;
    Uint32 sec1len = * ptr++;
    Uint32 sec2len = * ptr++;

    /**
     * Copy value directly into local buffers
     */
    Uint32 trigId = ((FireTrigOrd*)ptr)->getTriggerId();
    ndbrequire( setTriggerBufferLock(trigId) );
    
    memcpy(signal->theData, ptr, 4 * siglen); // signal
    ptr += siglen;
    memcpy(f_buffer, ptr, 4*sec0len);
    ptr += sec0len;
    memcpy(b_buffer, ptr, 4*sec1len);
    ptr += sec1len;
    memcpy(f_buffer + sec0len, ptr, 4*sec2len);
    ptr += sec2len;

    f_trigBufferSize = sec0len + sec2len;
    b_trigBufferSize = sec1len;

    execFIRE_TRIG_ORD(signal);

    ndbrequire(ptr == save + msglen);
    ndbrequire(len >= msglen);
    len -= msglen;
  }

  m_ctx.m_mm.release_page(RT_DBTUP_PAGE, pageId);
}

void
Suma::execFIRE_TRIG_ORD(Signal* signal)
{
  jamEntry();
  DBUG_ENTER("Suma::execFIRE_TRIG_ORD");
  
  CRASH_INSERTION(13016);
  FireTrigOrd* const trg = (FireTrigOrd*)signal->getDataPtr();
  const Uint32 trigId    = trg->getTriggerId();
  const Uint32 hashValue = trg->getHashValue();
  const Uint32 gci_hi    = trg->getGCI();
  const Uint32 gci_lo    = trg->m_gci_lo;
  const Uint64 gci = gci_lo | (Uint64(gci_hi) << 32);
  const Uint32 event     = trg->getTriggerEvent();
  const Uint32 any_value = trg->getAnyValue();
  const Uint32 transId1  = trg->m_transId1;
  const Uint32 transId2  = trg->m_transId2;

  Ptr<Subscription> subPtr;
  c_subscriptionPool.getPtr(subPtr, trigId & 0xFFFF);

  ndbassert(gci > m_last_complete_gci);

  if (signal->getNoOfSections())
  {
    jam();
    ndbassert(isNdbMtLqh());
    SectionHandle handle(this, signal);

    ndbrequire( setTriggerBufferLock(trigId) );

    SegmentedSectionPtr ptr;
    handle.getSection(ptr, 0); // Keys
    Uint32 sz = ptr.sz;
    copy(f_buffer, ptr);

    handle.getSection(ptr, 2); // After values
    copy(f_buffer + sz, ptr);
    f_trigBufferSize = sz + ptr.sz;

    handle.getSection(ptr, 1); // Before values
    copy(b_buffer, ptr);
    b_trigBufferSize = ptr.sz;
    releaseSections(handle);
  }

  jam();
  ndbrequire( checkTriggerBufferLock(trigId) );
  /**
   * Reset bufferlock 
   * We will use the buffers until the end of 
   * signal processing, but not after
   */
  ndbrequire( clearBufferLock() );
  
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
				f_buffer, f_trigBufferSize,
                                b_buffer, b_trigBufferSize);
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
    data->transId1       = transId1;
    data->transId2       = transId2;
    
    {
      LocalDLList<Subscriber> list(c_subscriberPool, subPtr.p->m_subscribers);
      SubscriberPtr subbPtr;
      for(list.first(subbPtr); !subbPtr.isNull(); list.next(subbPtr))
      {
	data->senderData = subbPtr.p->m_senderData;
	sendSignal(subbPtr.p->m_senderRef, GSN_SUB_TABLE_DATA, signal,
		   SubTableData::SignalLengthWithTransId, JBB, ptr, nptr);
      }
    }
  }
  else 
  {
    const uint buffer_header_sz = 6;
    Uint32* dst;
    Uint32 sz = f_trigBufferSize + b_trigBufferSize + buffer_header_sz;
    if((dst = get_buffer_ptr(signal, bucket, gci, sz)))
    {
      * dst++ = subPtr.i;
      * dst++ = schemaVersion;
      * dst++ = (event << 16) | f_trigBufferSize;
      * dst++ = any_value;
      * dst++ = transId1;
      * dst++ = transId2;
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
  if (!subs.isclear())
  {
   char buf[100];
   subs.getText(buf);
   infoEvent("Disconnecting lagging nodes '%s', epoch %llu", buf, gcp.p->m_gci);
  }
  // Disconnect lagging subscribers waiting for oldest epoch
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

  if (isNdbMtLqh() && m_gcp_rep_cnt > 1)
  {

#define SSPP 0

    if (SSPP)
      printf("execSUB_GCP_COMPLETE_REP(%u/%u)", gci_hi, gci_lo);
    jam();
    Uint32 min = m_min_gcp_rep_counter_index;
    Uint32 sz = NDB_ARRAY_SIZE(m_gcp_rep_counter);
    for (Uint32 i = min; i != m_max_gcp_rep_counter_index; i = (i + 1) % sz)
    {
      jam();
      if (m_gcp_rep_counter[i].m_gci == gci)
      {
        jam();
        m_gcp_rep_counter[i].m_cnt ++;
        if (m_gcp_rep_counter[i].m_cnt == m_gcp_rep_cnt)
        {
          jam();
          /**
           * Release this entry...
           */
          if (i != min)
          {
            jam();
            m_gcp_rep_counter[i] = m_gcp_rep_counter[min];
          }
          m_min_gcp_rep_counter_index = (min + 1) % sz;
          if (SSPP)
            ndbout_c(" found - complete after: (min: %u max: %u)",
                     m_min_gcp_rep_counter_index,
                     m_max_gcp_rep_counter_index);
          goto found;
        }
        else
        {
          jam();
          if (SSPP)
            ndbout_c(" found - wait unchanged: (min: %u max: %u)",
                     m_min_gcp_rep_counter_index,
                     m_max_gcp_rep_counter_index);
          return; // Wait for more...
        }
      }
    }
    /**
     * Not found...
     */
    Uint32 next = (m_max_gcp_rep_counter_index + 1) % sz;
    ndbrequire(next != min); // ring buffer full
    m_gcp_rep_counter[m_max_gcp_rep_counter_index].m_gci = gci;
    m_gcp_rep_counter[m_max_gcp_rep_counter_index].m_cnt = 1;
    m_max_gcp_rep_counter_index = next;
    if (SSPP)
      ndbout_c(" new - after: (min: %u max: %u)",
               m_min_gcp_rep_counter_index,
               m_max_gcp_rep_counter_index);
    return;
  }
found:
  bool drop = false;
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
  Bucket_mask dropped_buckets;
  if(!m_switchover_buckets.isclear())
  {
    bool unlock = false;
    Uint32 i = m_switchover_buckets.find(0);
    for(; i != Bucket_mask::NotFound; i = m_switchover_buckets.find(i + 1))
    {
      if(gci > c_buckets[i].m_switchover_gci)
      {
	Uint32 state = c_buckets[i].m_state;
	m_switchover_buckets.clear(i);
	printf("%u/%u (%u/%u) switchover complete bucket %d state: %x\n", 
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
          jam();
	  m_active_buckets.set(i);
	  c_buckets[i].m_state &= ~(Uint32)Bucket::BUCKET_STARTING;
	  ndbout_c("starting");
	  m_gcp_complete_rep_count++;
          unlock = true;
	}
	else if(state & Bucket::BUCKET_TAKEOVER)
	{
	  /**
	   * NF case
	   */
          jam();
	  Bucket* bucket= c_buckets + i;
	  Page_pos pos= bucket->m_buffer_head;
	  ndbrequire(pos.m_max_gci < gci);

	  Buffer_page* page= c_page_pool.getPtr(pos.m_page_id);
	  ndbout_c("takeover %d", pos.m_page_id);
	  page->m_max_gci_hi = (Uint32)(pos.m_max_gci >> 32);
          page->m_max_gci_lo = (Uint32)(pos.m_max_gci & 0xFFFFFFFF);
          ndbassert(pos.m_max_gci != 0);
	  page->m_words_used = pos.m_page_pos;
	  page->m_next_page = RNIL;
	  memset(&bucket->m_buffer_head, 0, sizeof(bucket->m_buffer_head));
	  bucket->m_buffer_head.m_page_id = RNIL;
	  bucket->m_buffer_head.m_page_pos = Buffer_page::DATA_WORDS + 1;

	  m_active_buckets.set(i);
          m_gcp_complete_rep_count++;
	  c_buckets[i].m_state &= ~(Uint32)Bucket::BUCKET_TAKEOVER;
	}
	else if (state & Bucket::BUCKET_HANDOVER)
	{
	  /**
	   * NR, living node
	   */
          jam();
	  c_buckets[i].m_state &= ~(Uint32)Bucket::BUCKET_HANDOVER;
          m_gcp_complete_rep_count--;
	  ndbout_c("handover");
	}
        else if (state & Bucket::BUCKET_CREATED_MASK)
        {
          jam();
          Uint32 cnt = state >> 8;
          Uint32 mask = Uint32(Bucket::BUCKET_CREATED_MASK) | (cnt << 8);
	  c_buckets[i].m_state &= ~mask;
          flags |= SubGcpCompleteRep::ADD_CNT;
          flags |= (cnt << 16);
          ndbout_c("add %u %s", cnt, 
                   state & Bucket::BUCKET_CREATED_SELF ? "self" : "other");
          if (state & Bucket::BUCKET_CREATED_SELF &&
              get_responsible_node(i) == getOwnNodeId())
          {
            jam();
            m_active_buckets.set(i);
            m_gcp_complete_rep_count++;
          }
        }
        else if (state & Bucket::BUCKET_DROPPED_MASK)
        {
          jam();
          Uint32 cnt = state >> 8;
          Uint32 mask = Uint32(Bucket::BUCKET_DROPPED_MASK) | (cnt << 8);
	  c_buckets[i].m_state &= ~mask;
          flags |= SubGcpCompleteRep::SUB_CNT;
          flags |= (cnt << 16);
          ndbout_c("sub %u %s", cnt, 
                   state & Bucket::BUCKET_DROPPED_SELF ? "self" : "other");
          if (state & Bucket::BUCKET_DROPPED_SELF)
          {
            if (m_active_buckets.get(i))
            {
              m_active_buckets.clear(i);
              // Remember this bucket, it should be listed
              // in SUB_GCP_COMPLETE_REP signal
              dropped_buckets.set(i);
            }
            drop = true;
          }
        }
        else if (state & Bucket::BUCKET_SHUTDOWN)
        {
          jam();
          Uint32 nodeId = c_buckets[i].m_switchover_node;
          ndbrequire(nodeId == getOwnNodeId());
          m_active_buckets.clear(i);
          m_gcp_complete_rep_count--;
          ndbout_c("shutdown handover");
          c_buckets[i].m_state &= ~(Uint32)Bucket::BUCKET_SHUTDOWN;
        }
        else if (state & Bucket::BUCKET_SHUTDOWN_TO)
        {
          jam();
          Uint32 nodeId = c_buckets[i].m_switchover_node;
          NdbNodeBitmask nodegroup = c_nodes_in_nodegroup_mask;
          nodegroup.clear(nodeId);
          ndbrequire(get_responsible_node(i) == nodeId &&
                     get_responsible_node(i, nodegroup) == getOwnNodeId());
          m_active_buckets.set(i);
          m_gcp_complete_rep_count++;
          c_buckets[i].m_state &= ~(Uint32)Bucket::BUCKET_SHUTDOWN_TO;
          ndbout_c("shutdown handover takeover");
        }
      }
    }

    if (m_switchover_buckets.isclear())
    {
      jam();
      if(getNodeState().startLevel == NodeState::SL_STARTING && 
         c_startup.m_handover_nodes.isclear())
      {
        jam();
        sendSTTORRY(signal);
      }
      else if (getNodeState().startLevel >= NodeState::SL_STOPPING_1)
      {
        jam();
        ndbrequire(c_shutdown.m_wait_handover);
        StopMeConf * conf = CAST_PTR(StopMeConf, signal->getDataPtrSend());
        conf->senderData = c_shutdown.m_senderData;
        conf->senderRef = reference();
        sendSignal(c_shutdown.m_senderRef, GSN_STOP_ME_CONF, signal,
                   StopMeConf::SignalLength, JBB);
        c_shutdown.m_wait_handover = false;
        infoEvent("Suma: handover complete");
      }
    }

    if (unlock)
    {
      jam();
      send_dict_unlock_ord(signal, DictLockReq::SumaHandOver);
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

  /**
   * Append the identifiers of the data streams that this Suma has
   * completed for the gcp.
   * The subscribers can use that to identify duplicates or lack
   * of reception.
   */
  Uint32 siglen = SubGcpCompleteRep::SignalLength;

  Uint32 stream_count=0;
  for(Uint32 bucket = 0; bucket < NO_OF_BUCKETS; bucket ++)
  {
    if(m_active_buckets.get(bucket) ||
       dropped_buckets.get(bucket) ||
       (m_switchover_buckets.get(bucket) && (check_switchover(bucket, gci))))
    {
      Uint32 sub_data_stream = get_sub_data_stream(bucket);
      if ((stream_count & 1) == 0)
      {
        rep->sub_data_streams[stream_count/2] = sub_data_stream;
      }
      else
      {
        rep->sub_data_streams[stream_count/2] |= sub_data_stream << 16;
      }
      stream_count++;
    }
  }

  /**
   * If count match the number of buckets that should be reported
   * complete, send subscription data streams identifiers.
   * If this is not the case fallback on old signal without
   * the streams identifiers, but that should not happend!
   */
  if (stream_count == m_gcp_complete_rep_count)
  {
    rep->flags |= SubGcpCompleteRep::SUB_DATA_STREAMS_IN_SIGNAL;
    siglen += (stream_count + 1)/2;
  }
  else
  {
    g_eventLogger->error("Suma gcp complete rep count (%u) does "
                         "not match number of buckets that should "
                         "be reported complete (%u).",
                         m_gcp_complete_rep_count,
                         stream_count);
    ndbassert(false);
  }

  if(m_gcp_complete_rep_count && !c_subscriber_nodes.isclear())
  {
    CRASH_INSERTION(13033);

    NodeReceiverGroup rg(API_CLUSTERMGR, c_subscriber_nodes);
    sendSignal(rg, GSN_SUB_GCP_COMPLETE_REP, signal, siglen, JBB);
    
    Ptr<Gcp_record> gcp;
    if (c_gcp_list.seizeLast(gcp))
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
    jam();
    infoEvent("Reenable event buffer");
    m_out_of_buffer_gci = 0;
    m_missing_data = false;
  }

  if (unlikely(drop))
  {
    jam();
    m_gcp_complete_rep_count = 0;
    c_nodeGroup = RNIL;
    c_nodes_in_nodegroup_mask.clear();
    fix_nodegroup();
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
  const Table::State old_state = tabPtr.p->m_state;
  tabPtr.p->m_state = Table::DROPPED;
  c_tables.remove(tabPtr);

  if (senderRef != 0)
  {
    jam();

    // dict coordinator sends info to API

    const Uint64 gci = get_current_gci(signal);
    SubTableData * data = (SubTableData*)signal->getDataPtrSend();
    data->gci_hi         = Uint32(gci >> 32);
    data->gci_lo         = Uint32(gci);
    data->tableId        = tableId;
    data->requestInfo    = 0;
    SubTableData::setOperation(data->requestInfo,
                               NdbDictionary::Event::_TE_DROP);
    SubTableData::setReqNodeId(data->requestInfo, refToNode(senderRef));

    Ptr<Subscription> subPtr;
    LocalDLList<Subscription> subList(c_subscriptionPool,
                                      tabPtr.p->m_subscriptions);

    for (subList.first(subPtr); !subPtr.isNull(); subList.next(subPtr))
    {
      jam();
      if(subPtr.p->m_subscriptionType != SubCreateReq::TableEvent)
      {
        jam();
        continue;
        //continue in for-loop if the table is not part of
        //the subscription. Otherwise, send data to subscriber.
      }

      if (subPtr.p->m_options & Subscription::NO_REPORT_DDL)
      {
        jam();
        continue;
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
    }
  }

  if (old_state == Table::DEFINING)
  {
    jam();
    return;
  }

  if (tabPtr.p->m_subscriptions.isEmpty())
  {
    jam();
    tabPtr.p->release(* this);
    c_tablePool.release(tabPtr);
    return;
  }
  else
  {
    /**
     * check_release_subscription create a subList...
     *   weirdness below is to make sure that it's not created twice
     */
    Ptr<Subscription> subPtr;
    {
      LocalDLList<Subscription> subList(c_subscriptionPool,
                                        tabPtr.p->m_subscriptions);
      subList.first(subPtr);
    }
    while (!subPtr.isNull())
    {
      Ptr<Subscription> tmp = subPtr;
      {
        LocalDLList<Subscription> subList(c_subscriptionPool,
                                          tabPtr.p->m_subscriptions);
        subList.next(subPtr);
      }
      check_release_subscription(signal, tmp);
    }
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
  
    if (subPtr.p->m_options & Subscription::NO_REPORT_DDL)
    {
      jam();
      continue;
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
    sendSubRemoveRef(signal,  req, SubRemoveRef::NotStarted);
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
    ndbrequire(false);
  case Subscription::DEFINING:
    jam();
    sendSubRemoveRef(signal, req, SubRemoveRef::Defining);
    return;
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

  LocalDataBuffer<15> boundBuf(suma.c_dataBufferPool, m_boundInfo);
  boundBuf.release();  

  ndbassert(m_sourceInstance == RNIL);
  ndbassert(m_headersSection == RNIL);
  ndbassert(m_dataSection == RNIL);
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
    list.addLast(subOpPtr);

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

  if (subPtr.p->m_options & Subscription::NO_REPORT_DDL)
  {
    req->subscriptionType |= SubCreateReq::NoReportDDL;
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
    if (!ndbd_suma_dictlock_startme(getNodeInfo(refToNode(c_restart.m_ref)).m_version))
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
  const SumaHandoverReq * req = CAST_CONSTPTR(SumaHandoverReq,
                                              signal->getDataPtr());

  Uint32 gci = req->gci;
  Uint32 nodeId = req->nodeId;
  Uint32 new_gci = Uint32(m_last_complete_gci >> 32) + MAX_CONCURRENT_GCP + 1;
  Uint32 requestType = req->requestType;
  if (!ndbd_suma_stop_me(getNodeInfo(nodeId).m_version))
  {
    jam();
    requestType = SumaHandoverReq::RT_START_NODE;
  }
  
  Uint32 start_gci = (gci > new_gci ? gci : new_gci);
  // mark all active buckets really belonging to restarting SUMA

  Bucket_mask tmp;
  if (requestType == SumaHandoverReq::RT_START_NODE)
  {
    jam();
    c_alive_nodes.set(nodeId);
    if (DBG_3R)
      ndbout_c("%u c_alive_nodes.set(%u)", __LINE__, nodeId);

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
  }
  else if (requestType == SumaHandoverReq::RT_STOP_NODE)
  {
    jam();

    for( Uint32 i = 0; i < c_no_of_buckets; i++)
    {
      NdbNodeBitmask nodegroup = c_nodes_in_nodegroup_mask;
      nodegroup.clear(nodeId);
      if(get_responsible_node(i) == nodeId &&
         get_responsible_node(i, nodegroup) == getOwnNodeId())
      {
        // I'm will be running this bucket when nodeId shutdown
        jam();
        tmp.set(i);
        m_switchover_buckets.set(i);
        c_buckets[i].m_switchover_gci = (Uint64(start_gci) << 32) - 1;
        c_buckets[i].m_state |= Bucket::BUCKET_SHUTDOWN_TO;
        c_buckets[i].m_switchover_node = nodeId;
        ndbout_c("prepare to takeover bucket: %d", i);
      }
    }
  }
  else
  {
    jam();
    goto ref;
  }

  {
    SumaHandoverConf *conf= CAST_PTR(SumaHandoverConf,signal->getDataPtrSend());
    tmp.copyto(BUCKET_MASK_SIZE, conf->theBucketMask);
    conf->gci = start_gci;
    conf->nodeId = getOwnNodeId();
    conf->requestType = requestType;
    sendSignal(calcSumaBlockRef(nodeId), GSN_SUMA_HANDOVER_CONF, signal,
               SumaHandoverConf::SignalLength, JBB);
  }

  DBUG_VOID_RETURN;

ref:
  signal->theData[0] = 111;
  signal->theData[1] = getOwnNodeId();
  signal->theData[2] = nodeId;
  sendSignal(calcSumaBlockRef(nodeId), GSN_SUMA_HANDOVER_REF, signal, 3, JBB);
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

  const SumaHandoverConf * conf = CAST_CONSTPTR(SumaHandoverConf,
                                                signal->getDataPtr());

  CRASH_INSERTION(13043);

  Uint32 gci = conf->gci;
  Uint32 nodeId = conf->nodeId;
  Uint32 requestType = conf->requestType;
  Bucket_mask tmp;
  tmp.assign(BUCKET_MASK_SIZE, conf->theBucketMask);
#ifdef HANDOVER_DEBUG
  ndbout_c("Suma::execSUMA_HANDOVER_CONF, gci = %u", gci);
#endif

  if (!ndbd_suma_stop_me(getNodeInfo(nodeId).m_version))
  {
    jam();
    requestType = SumaHandoverReq::RT_START_NODE;
  }

  if (requestType == SumaHandoverReq::RT_START_NODE)
  {
    jam();
    for (Uint32 i = 0; i < c_no_of_buckets; i++)
    {
      if (tmp.get(i))
      {
        if (DBG_3R)
          ndbout_c("%u : %u %u", i, get_responsible_node(i), getOwnNodeId());
        ndbrequire(get_responsible_node(i) == getOwnNodeId());
        // We should run this bucket, but _nodeId_ is
        c_buckets[i].m_switchover_gci = (Uint64(gci) << 32) - 1;
        c_buckets[i].m_state |= Bucket::BUCKET_STARTING;
      }
    }

    char buf[255];
    tmp.getText(buf);
    infoEvent("Suma: handover from node %u gci: %u buckets: %s (%u)",
              nodeId, gci, buf, c_no_of_buckets);
    g_eventLogger->info("Suma: handover from node %u gci: %u buckets: %s (%u)",
                        nodeId, gci, buf, c_no_of_buckets);
    m_switchover_buckets.bitOR(tmp);
    c_startup.m_handover_nodes.clear(nodeId);
    DBUG_VOID_RETURN;
  }
  else if (requestType == SumaHandoverReq::RT_STOP_NODE)
  {
    jam();
    for (Uint32 i = 0; i < c_no_of_buckets; i++)
    {
      if (tmp.get(i))
      {
        ndbrequire(get_responsible_node(i) == getOwnNodeId());
        // We should run this bucket, but _nodeId_ is
        c_buckets[i].m_switchover_node = getOwnNodeId();
        c_buckets[i].m_switchover_gci = (Uint64(gci) << 32) - 1;
        c_buckets[i].m_state |= Bucket::BUCKET_SHUTDOWN;
      }
    }
  
    char buf[255];
    tmp.getText(buf);
    infoEvent("Suma: handover to node %u gci: %u buckets: %s (%u)",
              nodeId, gci, buf, c_no_of_buckets);
    g_eventLogger->info("Suma: handover to node %u gci: %u buckets: %s (%u)",
                        nodeId, gci, buf, c_no_of_buckets);
    m_switchover_buckets.bitOR(tmp);
    c_startup.m_handover_nodes.clear(nodeId);
    DBUG_VOID_RETURN;
  }
}

void
Suma::execSTOP_ME_REQ(Signal* signal)
{
  jam();
  StopMeReq req = * CAST_CONSTPTR(StopMeReq, signal->getDataPtr());

  ndbrequire(refToNode(req.senderRef) == getOwnNodeId());
  ndbrequire(c_shutdown.m_wait_handover == false);
  c_shutdown.m_wait_handover = true;
  NdbTick_Invalidate(&c_startup.m_wait_handover_expire);
  c_shutdown.m_senderRef = req.senderRef;
  c_shutdown.m_senderData = req.senderData;

  for (Uint32 i = c_nodes_in_nodegroup_mask.find(0);
       i != c_nodes_in_nodegroup_mask.NotFound ;
       i = c_nodes_in_nodegroup_mask.find(i + 1))
  {
    /**
     * Check that all SUMA nodes support graceful shutdown...
     *   and it's too late to stop it...
     * Shutdown instead...
     */
    if (!ndbd_suma_stop_me(getNodeInfo(i).m_version))
    {
      jam();
      char buf[255];
      BaseString::snprintf(buf, sizeof(buf),
			   "Not all versions support graceful shutdown (suma)."
			   " Shutdown directly instead");
      progError(__LINE__,
		NDBD_EXIT_GRACEFUL_SHUTDOWN_ERROR,
		buf);
      ndbrequire(false);
    }
  }
  send_handover_req(signal, SumaHandoverReq::RT_STOP_NODE);
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
    page= c_page_pool.getPtr(pos.m_page_id);
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
    * ptr++ = (Uint32)(gci >> 32);
    * ptr++ = (Uint32)(gci & 0xFFFFFFFF);
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
      page->m_max_gci_hi = (Uint32)(pos.m_max_gci >> 32);
      page->m_max_gci_lo = (Uint32)(pos.m_max_gci & 0xFFFFFFFF);
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
    
    page= c_page_pool.getPtr(pos.m_page_id);
    page->m_next_page= RNIL;
    ptr= page->m_data;
    goto loop; //
  }
}

void
Suma::out_of_buffer(Signal* signal)
{
  Ptr<Gcp_record> gcp;
  if(m_out_of_buffer_gci)
  {
    return;
  }
  
  m_out_of_buffer_gci = m_last_complete_gci - 1;
  infoEvent("Out of event buffer: nodefailure will cause event failures, consider increasing MaxBufferedEpochBytes");
  if (!c_gcp_list.isEmpty())
  {
    jam();
    c_gcp_list.first(gcp);
    infoEvent("Highest epoch %llu, oldest epoch %llu", m_max_seen_gci, m_last_complete_gci);
    NodeBitmask subs = gcp.p->m_subscribers;
    if (!subs.isclear())
    {
      char buf[100];
      subs.getText(buf);
      infoEvent("Pending nodes '%s', epoch %llu", buf, gcp.p->m_gci);
    }
  }
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
    Buffer_page* page= c_page_pool.getPtr(tail);
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
    m_first_free_page = (c_page_pool.getPtr(ref))->m_next_page;
    Uint32 chunk = (c_page_pool.getPtr(ref))->m_page_chunk_ptr_i;
    c_page_chunk_pool.getPtr(ptr, chunk);
    ndbassert(ptr.p->m_free);
    ptr.p->m_free--;
    return ref;
  }

  if(!c_page_chunk_pool.seize(ptr))
    return RNIL;

  Uint32 count = Page_chunk::PAGES_PER_CHUNK;
  m_ctx.m_mm.alloc_pages(RT_DBTUP_PAGE, &ref, &count, 1);
  if (count == 0)
    return RNIL;

  g_eventLogger->info("Allocate event buffering page chunk in SUMA, %u pages,"
                      " first page ref = %u",
                      count, ref);

  m_first_free_page = ptr.p->m_page_id = ref;
  ptr.p->m_size = count;
  ptr.p->m_free = count;

  Buffer_page* page;
  for(Uint32 i = 0; i<count; i++)
  {
    page = c_page_pool.getPtr(ref);
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
    ndbout_c("release_gci(%d, %u/%u) 0x%x-> node failure -> abort", 
             buck, Uint32(gci >> 32), Uint32(gci), bucket->m_state);
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
    Buffer_page* page= c_page_pool.getPtr(tail);
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
      signal->theData[2] = (Uint32)(gci >> 32);
      signal->theData[3] = (Uint32)(gci & 0xFFFFFFFF);
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
  signal->theData[2] = (Uint32)(min >> 32);
  signal->theData[3] = 0;
  signal->theData[4] = 0;
  signal->theData[5] = (Uint32)(min & 0xFFFFFFFF);
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

  Buffer_page* page= c_page_pool.getPtr(tail);
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
      Uint32 siglen = SubGcpCompleteRep::SignalLength;

      rep->gci_hi = (Uint32)(last_gci >> 32);
      rep->gci_lo = (Uint32)(last_gci & 0xFFFFFFFF);
      rep->flags = (m_missing_data)
                   ? SubGcpCompleteRep::MISSING_DATA
                   : 0;
      rep->senderRef  = reference();
      rep->gcp_complete_rep_count = 1;

      // Append the sub data stream id for the bucket
      rep->sub_data_streams[0] = get_sub_data_stream(buck);
      rep->flags |= SubGcpCompleteRep::SUB_DATA_STREAMS_IN_SIGNAL;
      siglen ++;

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
      sendSignal(rg, GSN_SUB_GCP_COMPLETE_REP, signal, siglen, JBB);
    } 
    else
    {
      const uint buffer_header_sz = 6;
      g_cnt++;
      Uint32 subPtrI = * src++ ;
      Uint32 schemaVersion = * src++;
      Uint32 event = * src >> 16;
      Uint32 sz_1 = (* src ++) & 0xFFFF;
      Uint32 any_value = * src++;
      Uint32 transId1 = * src++;
      Uint32 transId2 = * src++;

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
	data->gci_hi         = (Uint32)(last_gci >> 32);
	data->gci_lo         = (Uint32)(last_gci & 0xFFFFFFFF);
	data->tableId        = table;
	data->requestInfo    = 0;
	SubTableData::setOperation(data->requestInfo, event);
	data->flags          = 0;
	data->anyValue       = any_value;
	data->totalLen       = ptrLen;
        data->transId1       = transId1;
        data->transId2       = transId2;
	
	{
          LocalDLList<Subscriber> list(c_subscriberPool,
                                       subPtr.p->m_subscribers);
          SubscriberPtr subbPtr;
          for(list.first(subbPtr); !subbPtr.isNull(); list.next(subbPtr))
          {
            data->senderData = subbPtr.p->m_senderData;
            sendSignal(subbPtr.p->m_senderRef, GSN_SUB_TABLE_DATA, signal,
                       SubTableData::SignalLengthWithTransId, JBB, ptr, nptr);
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
    pos = Uint32(ptr - page->m_data);
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
  signal->theData[2] = (Uint32)(min_gci >> 32);
  signal->theData[3] = pos;
  signal->theData[4] = (Uint32)(last_gci >> 32);
  signal->theData[5] = (Uint32)(min_gci & 0xFFFFFFFF);
  signal->theData[6] = (Uint32)(last_gci & 0xFFFFFFFF);
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

void
Suma::execCREATE_NODEGROUP_IMPL_REQ(Signal* signal)
{
  CreateNodegroupImplReq reqCopy = *(CreateNodegroupImplReq*)
    signal->getDataPtr();
  CreateNodegroupImplReq *req = &reqCopy;

  Uint32 err = 0;
  Uint32 rt = req->requestType;

  NdbNodeBitmask tmp;
  for (Uint32 i = 0; i<NDB_ARRAY_SIZE(req->nodes) && req->nodes[i]; i++)
  {
    tmp.set(req->nodes[i]);
  }
  Uint32 cnt = tmp.count();
  Uint32 group = req->nodegroupId;

  switch(rt){
  case CreateNodegroupImplReq::RT_ABORT:
    jam();
    break;
  case CreateNodegroupImplReq::RT_PARSE:
    jam();
    break;
  case CreateNodegroupImplReq::RT_PREPARE:
    jam();
    break;
  case CreateNodegroupImplReq::RT_COMMIT:
    jam();
    break;
  case CreateNodegroupImplReq::RT_COMPLETE:
    jam();
    CRASH_INSERTION(13043);

    Uint64 gci = (Uint64(req->gci_hi) << 32) | req->gci_lo;
    ndbrequire(gci > m_last_complete_gci);

    Uint32 state = 0;
    if (c_nodeGroup != RNIL)
    {
      jam();
      NdbNodeBitmask check = tmp;
      check.bitAND(c_nodes_in_nodegroup_mask);
      ndbrequire(check.isclear());
      ndbrequire(c_nodeGroup != group);
      ndbrequire(cnt == c_nodes_in_nodegroup_mask.count());
      state = Bucket::BUCKET_CREATED_OTHER;
    }
    else if (tmp.get(getOwnNodeId()))
    {
      jam();
      c_nodeGroup = group;
      c_nodes_in_nodegroup_mask.assign(tmp);
      fix_nodegroup();
      state = Bucket::BUCKET_CREATED_SELF;
    }
    if (state != 0)
    {
      for (Uint32 i = 0; i<c_no_of_buckets; i++)
      {
        jam();
        m_switchover_buckets.set(i);
        c_buckets[i].m_switchover_gci = gci - 1; // start from gci
        c_buckets[i].m_state = state | (c_no_of_buckets << 8);
      }
    }
  }

  {
    CreateNodegroupImplConf* conf =
      (CreateNodegroupImplConf*)signal->getDataPtrSend();
    conf->senderRef = reference();
    conf->senderData = req->senderData;
    sendSignal(req->senderRef, GSN_CREATE_NODEGROUP_IMPL_CONF, signal,
               CreateNodegroupImplConf::SignalLength, JBB);
  }
  return;

//error:
  CreateNodegroupImplRef *ref =
    (CreateNodegroupImplRef*)signal->getDataPtrSend();
  ref->senderRef = reference();
  ref->senderData = req->senderData;
  ref->errorCode = err;
  sendSignal(req->senderRef, GSN_CREATE_NODEGROUP_IMPL_REF, signal,
             CreateNodegroupImplRef::SignalLength, JBB);
  return;
}

void
Suma::execDROP_NODEGROUP_IMPL_REQ(Signal* signal)
{
  DropNodegroupImplReq reqCopy = *(DropNodegroupImplReq*)
    signal->getDataPtr();
  DropNodegroupImplReq *req = &reqCopy;

  Uint32 err = 0;
  Uint32 rt = req->requestType;
  Uint32 group = req->nodegroupId;

  switch(rt){
  case DropNodegroupImplReq::RT_ABORT:
    jam();
    break;
  case DropNodegroupImplReq::RT_PARSE:
    jam();
    break;
  case DropNodegroupImplReq::RT_PREPARE:
    jam();
    break;
  case DropNodegroupImplReq::RT_COMMIT:
    jam();
    break;
  case DropNodegroupImplReq::RT_COMPLETE:
    jam();
    CRASH_INSERTION(13043);

    Uint64 gci = (Uint64(req->gci_hi) << 32) | req->gci_lo;
    ndbrequire(gci > m_last_complete_gci);

    Uint32 state;
    if (c_nodeGroup != group)
    {
      jam();
      state = Bucket::BUCKET_DROPPED_OTHER;
      break;
    }
    else
    {
      jam();
      state = Bucket::BUCKET_DROPPED_SELF;
    }

    for (Uint32 i = 0; i<c_no_of_buckets; i++)
    {
      jam();
      m_switchover_buckets.set(i);
      if (c_buckets[i].m_state != 0)
      {
        jamLine(c_buckets[i].m_state);
        ndbout_c("c_buckets[%u].m_state: %u", i, c_buckets[i].m_state);
      }
      ndbrequire(c_buckets[i].m_state == 0); // XXX todo
      c_buckets[i].m_switchover_gci = gci - 1; // start from gci
      c_buckets[i].m_state = state | (c_no_of_buckets << 8);
    }
    break;
  }
  
  {
    DropNodegroupImplConf* conf =
      (DropNodegroupImplConf*)signal->getDataPtrSend();
    conf->senderRef = reference();
    conf->senderData = req->senderData;
    sendSignal(req->senderRef, GSN_DROP_NODEGROUP_IMPL_CONF, signal,
               DropNodegroupImplConf::SignalLength, JBB);
  }
  return;

//error:
  DropNodegroupImplRef *ref =
    (DropNodegroupImplRef*)signal->getDataPtrSend();
  ref->senderRef = reference();
  ref->senderData = req->senderData;
  ref->errorCode = err;
  sendSignal(req->senderRef, GSN_DROP_NODEGROUP_IMPL_REF, signal,
             DropNodegroupImplRef::SignalLength, JBB);
  return;
}

template void append(DataBuffer<11>&,SegmentedSectionPtr,SectionSegmentPool&);


/*
   Copyright (c) 2003, 2011, Oracle and/or its affiliates. All rights reserved.

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
#include <ndb_limits.h>
#include <util/version.h>

#include "TransporterFacade.hpp"
#include <kernel/GlobalSignalNumbers.h>

#include "ClusterMgr.hpp"
#include <IPCConfig.hpp>
#include "NdbApiSignal.hpp"
#include <NdbSleep.h>
#include <NdbOut.hpp>
#include <NdbTick.h>


#include <signaldata/NodeFailRep.hpp>
#include <signaldata/NFCompleteRep.hpp>
#include <signaldata/ApiRegSignalData.hpp>
#include <signaldata/AlterTable.hpp>
#include <signaldata/SumaImpl.hpp>

#include <mgmapi.h>
#include <mgmapi_configuration.hpp>
#include <mgmapi_config_parameters.h>

int global_flag_skip_invalidate_cache = 0;
int global_flag_skip_waiting_for_clean_cache = 0;
//#define DEBUG_REG

// Just a C wrapper for threadMain
extern "C" 
void*
runClusterMgr_C(void * me)
{
  ((ClusterMgr*) me)->threadMain();

  return NULL;
}

ClusterMgr::ClusterMgr(TransporterFacade & _facade):
  theStop(0),
  theFacade(_facade),
  theArbitMgr(NULL),
  m_connect_count(0),
  m_max_api_reg_req_interval(~0),
  noOfAliveNodes(0),
  noOfConnectedNodes(0),
  minDbVersion(0),
  theClusterMgrThread(NULL),
  waitingForHB(false),
  m_cluster_state(CS_waiting_for_clean_cache)
{
  DBUG_ENTER("ClusterMgr::ClusterMgr");
  clusterMgrThreadMutex = NdbMutex_Create();
  waitForHBCond= NdbCondition_Create();
  m_auto_reconnect = -1;

  Uint32 ret = this->open(&theFacade, API_CLUSTERMGR);
  if (unlikely(ret == 0))
  {
    ndbout_c("Failed to register ClusterMgr! ret: %d", ret);
    abort();
  }
  DBUG_VOID_RETURN;
}

ClusterMgr::~ClusterMgr()
{
  DBUG_ENTER("ClusterMgr::~ClusterMgr");
  doStop();
  if (theArbitMgr != 0)
  {
    delete theArbitMgr;
    theArbitMgr = 0;
  }
  this->close(); // disconnect from TransporterFacade
  NdbCondition_Destroy(waitForHBCond);
  NdbMutex_Destroy(clusterMgrThreadMutex);
  DBUG_VOID_RETURN;
}

void
ClusterMgr::configure(Uint32 nodeId,
                      const ndb_mgm_configuration* config)
{
  ndb_mgm_configuration_iterator iter(* config, CFG_SECTION_NODE);
  for(iter.first(); iter.valid(); iter.next()){
    Uint32 nodeId = 0;
    if(iter.get(CFG_NODE_ID, &nodeId))
      continue;

    // Check array bounds + don't allow node 0 to be touched
    assert(nodeId > 0 && nodeId < MAX_NODES);
    trp_node& theNode = theNodes[nodeId];
    theNode.defined = true;

    unsigned type;
    if(iter.get(CFG_TYPE_OF_SECTION, &type))
      continue;

    switch(type){
    case NODE_TYPE_DB:
      theNode.m_info.m_type = NodeInfo::DB;
      break;
    case NODE_TYPE_API:
      theNode.m_info.m_type = NodeInfo::API;
      break;
    case NODE_TYPE_MGM:
      theNode.m_info.m_type = NodeInfo::MGM;
      break;
    default:
      type = type;
      break;
    }
  }

  /* Mark all non existing nodes as not defined */
  for(Uint32 i = 0; i<MAX_NODES; i++) {
    if (iter.first())
      continue;

    if (iter.find(CFG_NODE_ID, i))
      theNodes[i]= Node();
  }

#if 0
  print_nodes("init");
#endif

  // Configure arbitrator
  Uint32 rank = 0;
  iter.first();
  iter.find(CFG_NODE_ID, nodeId); // let not found in config mean rank=0
  iter.get(CFG_NODE_ARBIT_RANK, &rank);

  if (rank > 0)
  {
    // The arbitrator should be active
    if (!theArbitMgr)
      theArbitMgr = new ArbitMgr(* this);
    theArbitMgr->setRank(rank);

    Uint32 delay = 0;
    iter.get(CFG_NODE_ARBIT_DELAY, &delay);
    theArbitMgr->setDelay(delay);
  }
  else if (theArbitMgr)
  {
    // No arbitrator should be started
    theArbitMgr->doStop(NULL);
    delete theArbitMgr;
    theArbitMgr= NULL;
  }
}

void
ClusterMgr::startThread() {
  Guard g(clusterMgrThreadMutex);

  theStop = -1;
  theClusterMgrThread = NdbThread_Create(runClusterMgr_C,
                                         (void**)this,
                                         0, // default stack size
                                         "ndb_clustermgr",
                                         NDB_THREAD_PRIO_HIGH);
  Uint32 cnt = 0;
  while (theStop == -1 && cnt < 60)
  {
    NdbCondition_WaitTimeout(waitForHBCond, clusterMgrThreadMutex, 1000);
  }

  assert(theStop == 0);
}

void
ClusterMgr::doStop( ){
  DBUG_ENTER("ClusterMgr::doStop");
  {
    Guard g(clusterMgrThreadMutex);
    if(theStop == 1){
      DBUG_VOID_RETURN;
    }
  }

  void *status;
  theStop = 1;
  if (theClusterMgrThread) {
    NdbThread_WaitFor(theClusterMgrThread, &status);  
    NdbThread_Destroy(&theClusterMgrThread);
  }

  if (theArbitMgr != NULL)
  {
    theArbitMgr->doStop(NULL);
  }

  DBUG_VOID_RETURN;
}

void
ClusterMgr::forceHB()
{
  theFacade.lock_mutex();

  if(waitingForHB)
  {
    NdbCondition_WaitTimeout(waitForHBCond, theFacade.theMutexPtr, 1000);
    theFacade.unlock_mutex();
    return;
  }

  waitingForHB= true;

  NodeBitmask ndb_nodes;
  ndb_nodes.clear();
  waitForHBFromNodes.clear();
  for(Uint32 i = 1; i < MAX_NDB_NODES; i++)
  {
    const trp_node &node= getNodeInfo(i);
    if(!node.defined)
      continue;
    if(node.m_info.getType() == NodeInfo::DB)
    {
      ndb_nodes.set(i);
      waitForHBFromNodes.bitOR(node.m_state.m_connected_nodes);
    }
  }
  waitForHBFromNodes.bitAND(ndb_nodes);
  theFacade.unlock_mutex();

#ifdef DEBUG_REG
  char buf[128];
  ndbout << "Waiting for HB from " << waitForHBFromNodes.getText(buf) << endl;
#endif
  NdbApiSignal signal(numberToRef(API_CLUSTERMGR, theFacade.ownId()));

  signal.theVerId_signalNumber   = GSN_API_REGREQ;
  signal.theReceiversBlockNumber = QMGR;
  signal.theTrace                = 0;
  signal.theLength               = ApiRegReq::SignalLength;

  ApiRegReq * req = CAST_PTR(ApiRegReq, signal.getDataPtrSend());
  req->ref = numberToRef(API_CLUSTERMGR, theFacade.ownId());
  req->version = NDB_VERSION;
  req->mysql_version = NDB_MYSQL_VERSION_D;

  {
    lock();
    int nodeId= 0;
    for(int i=0;
        (int) NodeBitmask::NotFound != (nodeId= waitForHBFromNodes.find(i));
        i= nodeId+1)
    {
#ifdef DEBUG_REG
      ndbout << "FORCE HB to " << nodeId << endl;
#endif
      raw_sendSignal(&signal, nodeId);
    }
    unlock();
  }
  /* Wait for nodes to reply - if any heartbeats was sent */
  theFacade.lock_mutex();
  if (!waitForHBFromNodes.isclear())
    NdbCondition_WaitTimeout(waitForHBCond, theFacade.theMutexPtr, 1000);

  waitingForHB= false;
#ifdef DEBUG_REG
  ndbout << "Still waiting for HB from " << waitForHBFromNodes.getText(buf) << endl;
#endif
  theFacade.unlock_mutex();
}

void
ClusterMgr::startup()
{
  assert(theStop == -1);
  Uint32 nodeId = getOwnNodeId();
  Node & cm_node = theNodes[nodeId];
  trp_node & theNode = cm_node;
  assert(theNode.defined);

  lock();
  theFacade.doConnect(nodeId);
  unlock();

  for (Uint32 i = 0; i<3000; i++)
  {
    lock();
    theFacade.theTransporterRegistry->update_connections();
    unlock();
    if (theNode.is_connected())
      break;
    NdbSleep_MilliSleep(20);
  }

  assert(theNode.is_connected());
  Guard g(clusterMgrThreadMutex);
  theStop = 0;
  NdbCondition_Broadcast(waitForHBCond);
}

void
ClusterMgr::threadMain()
{
  startup();

  NdbApiSignal signal(numberToRef(API_CLUSTERMGR, theFacade.ownId()));
  
  signal.theVerId_signalNumber   = GSN_API_REGREQ;
  signal.theTrace                = 0;
  signal.theLength               = ApiRegReq::SignalLength;

  ApiRegReq * req = CAST_PTR(ApiRegReq, signal.getDataPtrSend());
  req->ref = numberToRef(API_CLUSTERMGR, theFacade.ownId());
  req->version = NDB_VERSION;
  req->mysql_version = NDB_MYSQL_VERSION_D;
  
  NdbApiSignal nodeFail_signal(numberToRef(API_CLUSTERMGR, getOwnNodeId()));
  nodeFail_signal.theVerId_signalNumber = GSN_NODE_FAILREP;
  nodeFail_signal.theReceiversBlockNumber = API_CLUSTERMGR;
  nodeFail_signal.theTrace  = 0;
  nodeFail_signal.theLength = NodeFailRep::SignalLengthLong;

  NDB_TICKS timeSlept = 100;
  NDB_TICKS now = NdbTick_CurrentMillisecond();

  while(!theStop)
  {
    /* Sleep at 100ms between each heartbeat check */
    NDB_TICKS before = now;
    for (Uint32 i = 0; i<10; i++)
    {
      NdbSleep_MilliSleep(10);
      {
        Guard g(clusterMgrThreadMutex);
        /**
         * Protect from ArbitMgr sending signals while we poll
         */
        start_poll();
        do_poll(0);
        complete_poll();
      }
    }
    now = NdbTick_CurrentMillisecond();
    timeSlept = (now - before);

    if (m_cluster_state == CS_waiting_for_clean_cache &&
        theFacade.m_globalDictCache)
    {
      if (!global_flag_skip_waiting_for_clean_cache)
      {
        theFacade.m_globalDictCache->lock();
        unsigned sz= theFacade.m_globalDictCache->get_size();
        theFacade.m_globalDictCache->unlock();
        if (sz)
          continue;
      }
      m_cluster_state = CS_waiting_for_first_connect;
    }


    NodeFailRep * nodeFailRep = CAST_PTR(NodeFailRep,
                                         nodeFail_signal.getDataPtrSend());
    nodeFailRep->noOfNodes = 0;
    NodeBitmask::clear(nodeFailRep->theNodes);

    trp_client::lock();
    for (int i = 1; i < MAX_NODES; i++){
      /**
       * Send register request (heartbeat) to all available nodes 
       * at specified timing intervals
       */
      const NodeId nodeId = i;
      // Check array bounds + don't allow node 0 to be touched
      assert(nodeId > 0 && nodeId < MAX_NODES);
      Node & cm_node = theNodes[nodeId];
      trp_node & theNode = cm_node;

      if (!theNode.defined)
	continue;

      if (theNode.is_connected() == false){
	theFacade.doConnect(nodeId);
	continue;
      }
      
      if (!theNode.compatible){
	continue;
      }
      
      if (nodeId == getOwnNodeId() && theNode.is_confirmed())
      {
        /**
         * Don't send HB to self more than once
         * (once needed to avoid weird special cases in e.g ConfigManager)
         */
        continue;
      }

      cm_node.hbCounter += (Uint32)timeSlept;
      if (cm_node.hbCounter >= m_max_api_reg_req_interval ||
          cm_node.hbCounter >= cm_node.hbFrequency)
      {
	/**
	 * It is now time to send a new Heartbeat
	 */
        if (cm_node.hbCounter >= cm_node.hbFrequency)
        {
          cm_node.hbMissed++;
          cm_node.hbCounter = 0;
	}

        if (theNode.m_info.m_type != NodeInfo::DB)
          signal.theReceiversBlockNumber = API_CLUSTERMGR;
        else
          signal.theReceiversBlockNumber = QMGR;

#ifdef DEBUG_REG
	ndbout_c("ClusterMgr: Sending API_REGREQ to node %d", (int)nodeId);
#endif
	raw_sendSignal(&signal, nodeId);
      }//if
      
      if (cm_node.hbMissed == 4 && cm_node.hbFrequency > 0)
      {
        nodeFailRep->noOfNodes++;
        NodeBitmask::set(nodeFailRep->theNodes, nodeId);
      }
    }

    if (nodeFailRep->noOfNodes)
    {
      raw_sendSignal(&nodeFail_signal, getOwnNodeId());
    }
    trp_client::unlock();
  }
}

void
ClusterMgr::trp_deliver_signal(const NdbApiSignal* sig,
                               const LinearSectionPtr ptr[3])
{
  const Uint32 gsn = sig->theVerId_signalNumber;
  const Uint32 * theData = sig->getDataPtr();

  switch (gsn){
  case GSN_API_REGREQ:
    execAPI_REGREQ(theData);
    break;

  case GSN_API_REGCONF:
     execAPI_REGCONF(sig, ptr);
    break;

  case GSN_API_REGREF:
    execAPI_REGREF(theData);
    break;

  case GSN_NODE_FAILREP:
    execNODE_FAILREP(sig, ptr);
    break;

  case GSN_NF_COMPLETEREP:
    execNF_COMPLETEREP(sig, ptr);
    break;
  case GSN_ARBIT_STARTREQ:
    if (theArbitMgr != NULL)
      theArbitMgr->doStart(theData);
    break;

  case GSN_ARBIT_CHOOSEREQ:
    if (theArbitMgr != NULL)
      theArbitMgr->doChoose(theData);
    break;

  case GSN_ARBIT_STOPORD:
    if(theArbitMgr != NULL)
      theArbitMgr->doStop(theData);
    break;

  case GSN_ALTER_TABLE_REP:
  {
    if (theFacade.m_globalDictCache == NULL)
      break;
    const AlterTableRep* rep = (const AlterTableRep*)theData;
    theFacade.m_globalDictCache->lock();
    theFacade.m_globalDictCache->
      alter_table_rep((const char*)ptr[0].p,
                      rep->tableId,
                      rep->tableVersion,
                      rep->changeType == AlterTableRep::CT_ALTERED);
    theFacade.m_globalDictCache->unlock();
    break;
  }
  case GSN_SUB_GCP_COMPLETE_REP:
  {
    /**
     * Report
     */
    theFacade.for_each(this, sig, ptr);

    /**
     * Reply
     */
    {
      BlockReference ownRef = numberToRef(API_CLUSTERMGR, theFacade.ownId());
      NdbApiSignal tSignal(* sig);
      Uint32* send= tSignal.getDataPtrSend();
      memcpy(send, theData, tSignal.getLength() << 2);
      CAST_PTR(SubGcpCompleteAck, send)->rep.senderRef = ownRef;
      Uint32 ref= sig->theSendersBlockRef;
      Uint32 aNodeId= refToNode(ref);
      tSignal.theReceiversBlockNumber= refToBlock(ref);
      tSignal.theVerId_signalNumber= GSN_SUB_GCP_COMPLETE_ACK;
      tSignal.theSendersBlockRef = API_CLUSTERMGR;
      safe_sendSignal(&tSignal, aNodeId);
    }
    break;
  }
  case GSN_TAKE_OVERTCCONF:
  {
    /**
     * Report
     */
    theFacade.for_each(this, sig, ptr);
    return;
  }
  case GSN_CONNECT_REP:
  {
    execCONNECT_REP(sig, ptr);
    return;
  }
  case GSN_DISCONNECT_REP:
  {
    execDISCONNECT_REP(sig, ptr);
    return;
  }
  default:
    break;

  }
  return;
}

ClusterMgr::Node::Node()
  : hbFrequency(0), hbCounter(0)
{
}

/**
 * recalcMinDbVersion
 *
 * This method is called whenever the 'minimum DB node
 * version' data for the connected DB nodes changes
 * It calculates the minimum version of all the connected
 * DB nodes.
 * This information is cached by Ndb object instances.
 * This information is useful when implementing API compatibility
 * with older DB nodes
 */
void
ClusterMgr::recalcMinDbVersion()
{
  Uint32 newMinDbVersion = ~ (Uint32) 0;
  
  for (Uint32 i = 0; i < MAX_NODES; i++)
  {
    trp_node& node = theNodes[i];

    if (node.is_connected() &&
        node.is_confirmed() &&
        node.m_info.getType() == NodeInfo::DB)
    {
      /* Include this node in the set of nodes used to
       * compute the lowest current DB node version
       */
      assert(node.m_info.m_version);

      if (node.minDbVersion < newMinDbVersion)
      {
        newMinDbVersion = node.minDbVersion;
      }
    }
  }

  /* Now update global min Db version if we have one.
   * Otherwise set it to 0
   */
  newMinDbVersion = (newMinDbVersion == ~ (Uint32) 0) ?
    0 :
    newMinDbVersion;

//#ifdef DEBUG_MINVER

#ifdef DEBUG_MINVER
  if (newMinDbVersion != minDbVersion)
  {
    ndbout << "Previous min Db node version was "
           << NdbVersion(minDbVersion)
           << " new min is "
           << NdbVersion(newMinDbVersion)
           << endl;
  }
  else
  {
    ndbout << "MinDbVersion recalculated, but is same : "
           << NdbVersion(minDbVersion)
           << endl;
  }
#endif

  minDbVersion = newMinDbVersion;
}

/******************************************************************************
 * API_REGREQ and friends
 ******************************************************************************/

void
ClusterMgr::execAPI_REGREQ(const Uint32 * theData){
  const ApiRegReq * const apiRegReq = (ApiRegReq *)&theData[0];
  const NodeId nodeId = refToNode(apiRegReq->ref);

#ifdef DEBUG_REG
  ndbout_c("ClusterMgr: Recd API_REGREQ from node %d", nodeId);
#endif

  assert(nodeId > 0 && nodeId < MAX_NODES);

  Node & cm_node = theNodes[nodeId];
  trp_node & node = cm_node;
  assert(node.defined == true);
  assert(node.is_connected() == true);

  if(node.m_info.m_version != apiRegReq->version){
    node.m_info.m_version = apiRegReq->version;
    node.m_info.m_mysql_version = apiRegReq->mysql_version;
    if (node.m_info.m_version < NDBD_SPLIT_VERSION)
      node.m_info.m_mysql_version = 0;

    if (getMajor(node.m_info.m_version) < getMajor(NDB_VERSION) ||
	getMinor(node.m_info.m_version) < getMinor(NDB_VERSION)) {
      node.compatible = false;
    } else {
      node.compatible = true;
    }
  }

  NdbApiSignal signal(numberToRef(API_CLUSTERMGR, theFacade.ownId()));
  signal.theVerId_signalNumber   = GSN_API_REGCONF;
  signal.theReceiversBlockNumber = API_CLUSTERMGR;
  signal.theTrace                = 0;
  signal.theLength               = ApiRegConf::SignalLength;
  
  ApiRegConf * const conf = CAST_PTR(ApiRegConf, signal.getDataPtrSend());
  conf->qmgrRef = numberToRef(API_CLUSTERMGR, theFacade.ownId());
  conf->version = NDB_VERSION;
  conf->mysql_version = NDB_MYSQL_VERSION_D;
  conf->apiHeartbeatFrequency = cm_node.hbFrequency;

  conf->minDbVersion= 0;
  conf->nodeState= node.m_state;

  node.set_confirmed(true);
  if (safe_sendSignal(&signal, nodeId) != 0)
    node.set_confirmed(false);
}

void
ClusterMgr::execAPI_REGCONF(const NdbApiSignal * signal,
                            const LinearSectionPtr ptr[])
{
  const ApiRegConf * apiRegConf = CAST_CONSTPTR(ApiRegConf,
                                                signal->getDataPtr());
  const NodeId nodeId = refToNode(apiRegConf->qmgrRef);
  
#ifdef DEBUG_REG
  ndbout_c("ClusterMgr: Recd API_REGCONF from node %d", nodeId);
#endif

  assert(nodeId > 0 && nodeId < MAX_NODES);
  
  Node & cm_node = theNodes[nodeId];
  trp_node & node = cm_node;
  assert(node.defined == true);
  assert(node.is_connected() == true);

  if(node.m_info.m_version != apiRegConf->version){
    node.m_info.m_version = apiRegConf->version;
    node.m_info.m_mysql_version = apiRegConf->mysql_version;
    if (node.m_info.m_version < NDBD_SPLIT_VERSION)
      node.m_info.m_mysql_version = 0;
        
    if(theNodes[theFacade.ownId()].m_info.m_type == NodeInfo::MGM)
      node.compatible = ndbCompatible_mgmt_ndb(NDB_VERSION,
					       node.m_info.m_version);
    else
      node.compatible = ndbCompatible_api_ndb(NDB_VERSION,
					      node.m_info.m_version);
  }

  node.set_confirmed(true);

  if (node.minDbVersion != apiRegConf->minDbVersion)
  {
    node.minDbVersion = apiRegConf->minDbVersion;
    recalcMinDbVersion();
  }

  if (node.m_info.m_version >= NDBD_255_NODES_VERSION)
  {
    node.m_state = apiRegConf->nodeState;
  }
  else
  {
    /**
     * from 2 to 8 words = 6 words diff, 6*4 = 24
     */
    memcpy(&node.m_state, &apiRegConf->nodeState, sizeof(node.m_state) - 24);
  }
  
  if (node.m_info.m_type == NodeInfo::DB)
  {
    /**
     * Only set DB nodes to "alive"
     */
    if (node.compatible && (node.m_state.startLevel == NodeState::SL_STARTED ||
                            node.m_state.getSingleUserMode()))
    {
      set_node_alive(node, true);
    }
    else
    {
      set_node_alive(node, false);
    }
  }

  cm_node.hbMissed = 0;
  cm_node.hbCounter = 0;
  cm_node.hbFrequency = (apiRegConf->apiHeartbeatFrequency * 10) - 50;

  // Distribute signal to all threads/blocks
  // TODO only if state changed...
  theFacade.for_each(this, signal, ptr);

  check_wait_for_hb(nodeId);
}

void
ClusterMgr::check_wait_for_hb(NodeId nodeId)
{
  if(waitingForHB)
  {
    waitForHBFromNodes.clear(nodeId);

    if(waitForHBFromNodes.isclear())
    {
      waitingForHB= false;
      NdbCondition_Broadcast(waitForHBCond);
    }
  }
  return;
}


void
ClusterMgr::execAPI_REGREF(const Uint32 * theData){
  
  ApiRegRef * ref = (ApiRegRef*)theData;
  
  const NodeId nodeId = refToNode(ref->ref);

  assert(nodeId > 0 && nodeId < MAX_NODES);

  Node & cm_node = theNodes[nodeId];
  trp_node & node = cm_node;

  assert(node.is_connected() == true);
  assert(node.defined == true);
  /* Only DB nodes will send API_REGREF */
  assert(node.m_info.getType() == NodeInfo::DB);

  node.compatible = false;
  set_node_alive(node, false);
  node.m_state = NodeState::SL_NOTHING;
  node.m_info.m_version = ref->version;

  switch(ref->errorCode){
  case ApiRegRef::WrongType:
    ndbout_c("Node %d reports that this node should be a NDB node", nodeId);
    abort();
  case ApiRegRef::UnsupportedVersion:
  default:
    break;
  }

  check_wait_for_hb(nodeId);
}

void
ClusterMgr::execNF_COMPLETEREP(const NdbApiSignal* signal,
                               const LinearSectionPtr ptr[3])
{
  const NFCompleteRep * nfComp = CAST_CONSTPTR(NFCompleteRep,
                                               signal->getDataPtr());
  const NodeId nodeId = nfComp->failedNodeId;
  assert(nodeId > 0 && nodeId < MAX_NODES);

  trp_node & node = theNodes[nodeId];
  if (node.nfCompleteRep == false)
  {
    node.nfCompleteRep = true;
    theFacade.for_each(this, signal, ptr);
  }
}

void
ClusterMgr::reportConnected(NodeId nodeId)
{
  DBUG_ENTER("ClusterMgr::reportConnected");
  DBUG_PRINT("info", ("nodeId: %u", nodeId));
  /**
   * Ensure that we are sending heartbeat every 100 ms
   * until we have got the first reply from NDB providing
   * us with the real time-out period to use.
   */
  assert(nodeId > 0 && nodeId < MAX_NODES);
  if (nodeId == getOwnNodeId())
  {
    noOfConnectedNodes--; // Don't count self...
  }

  noOfConnectedNodes++;

  Node & cm_node = theNodes[nodeId];
  trp_node & theNode = cm_node;

  cm_node.hbMissed = 0;
  cm_node.hbCounter = 0;
  cm_node.hbFrequency = 0;

  assert(theNode.is_connected() == false);

  /**
   * make sure the node itself is marked connected even
   * if first API_REGCONF has not arrived
   */
  theNode.set_connected(true);
  theNode.m_state.m_connected_nodes.set(nodeId);
  theNode.m_info.m_version = 0;
  theNode.compatible = true;
  theNode.nfCompleteRep = true;
  theNode.m_node_fail_rep = false;
  theNode.m_state.startLevel = NodeState::SL_NOTHING;
  theNode.minDbVersion = 0;
  
  /**
   * We know that we have clusterMgrThreadMutex and trp_client::mutex
   *   but we don't know if we are polling...and for_each can
   *   only be used by a poller...
   *
   * Send signal to self, so that we can do this when receiving a signal
   */
  NdbApiSignal signal(numberToRef(API_CLUSTERMGR, getOwnNodeId()));
  signal.theVerId_signalNumber = GSN_CONNECT_REP;
  signal.theReceiversBlockNumber = API_CLUSTERMGR;
  signal.theTrace  = 0;
  signal.theLength = 1;
  signal.getDataPtrSend()[0] = nodeId;
  raw_sendSignal(&signal, getOwnNodeId());
  DBUG_VOID_RETURN;
}

void
ClusterMgr::execCONNECT_REP(const NdbApiSignal* sig,
                            const LinearSectionPtr ptr[])
{
  theFacade.for_each(this, sig, 0);
}

void
ClusterMgr::set_node_dead(trp_node& theNode)
{
  set_node_alive(theNode, false);
  theNode.set_confirmed(false);
  theNode.m_state.m_connected_nodes.clear();
  theNode.m_state.startLevel = NodeState::SL_NOTHING;
  theNode.m_info.m_connectCount ++;
  theNode.nfCompleteRep = false;
}

void
ClusterMgr::reportDisconnected(NodeId nodeId)
{
  assert(nodeId > 0 && nodeId < MAX_NODES);
  assert(noOfConnectedNodes > 0);

  /**
   * We know that we have clusterMgrThreadMutex and trp_client::mutex
   *   but we don't know if we are polling...and for_each can
   *   only be used by a poller...
   *
   * Send signal to self, so that we can do this when receiving a signal
   */
  NdbApiSignal signal(numberToRef(API_CLUSTERMGR, getOwnNodeId()));
  signal.theVerId_signalNumber = GSN_DISCONNECT_REP;
  signal.theReceiversBlockNumber = API_CLUSTERMGR;
  signal.theTrace  = 0;
  signal.theLength = DisconnectRep::SignalLength;

  DisconnectRep * rep = CAST_PTR(DisconnectRep, signal.getDataPtrSend());
  rep->nodeId = nodeId;
  rep->err = 0;
  raw_sendSignal(&signal, getOwnNodeId());
}

void
ClusterMgr::execDISCONNECT_REP(const NdbApiSignal* sig,
                               const LinearSectionPtr ptr[])
{
  const DisconnectRep * rep = CAST_CONSTPTR(DisconnectRep, sig->getDataPtr());
  Uint32 nodeId = rep->nodeId;

  assert(nodeId > 0 && nodeId < MAX_NODES);
  Node & cm_node = theNodes[nodeId];
  trp_node & theNode = cm_node;

  bool node_failrep = theNode.m_node_fail_rep;
  set_node_dead(theNode);
  theNode.set_connected(false);

  noOfConnectedNodes--;
  if (noOfConnectedNodes == 0)
  {
    if (!global_flag_skip_invalidate_cache &&
        theFacade.m_globalDictCache)
    {
      theFacade.m_globalDictCache->lock();
      theFacade.m_globalDictCache->invalidate_all();
      theFacade.m_globalDictCache->unlock();
      m_connect_count ++;
      m_cluster_state = CS_waiting_for_clean_cache;
    }

    if (m_auto_reconnect == 0)
    {
      theStop = 2;
    }
  }

  if (node_failrep == false)
  {
    /**
     * Inform API
     */
    NdbApiSignal signal(numberToRef(API_CLUSTERMGR, getOwnNodeId()));
    signal.theVerId_signalNumber = GSN_NODE_FAILREP;
    signal.theReceiversBlockNumber = API_CLUSTERMGR;
    signal.theTrace  = 0;
    signal.theLength = NodeFailRep::SignalLengthLong;

    NodeFailRep * rep = CAST_PTR(NodeFailRep, signal.getDataPtrSend());
    rep->failNo = 0;
    rep->masterNodeId = 0;
    rep->noOfNodes = 1;
    NodeBitmask::clear(rep->theNodes);
    NodeBitmask::set(rep->theNodes, nodeId);
    execNODE_FAILREP(&signal, 0);
  }
}

void
ClusterMgr::execNODE_FAILREP(const NdbApiSignal* sig,
                             const LinearSectionPtr ptr[])
{
  const NodeFailRep * rep = CAST_CONSTPTR(NodeFailRep, sig->getDataPtr());

  NdbApiSignal signal(sig->theSendersBlockRef);
  signal.theVerId_signalNumber = GSN_NODE_FAILREP;
  signal.theReceiversBlockNumber = API_CLUSTERMGR;
  signal.theTrace  = 0;
  signal.theLength = NodeFailRep::SignalLengthLong;
  
  NodeFailRep * copy = CAST_PTR(NodeFailRep, signal.getDataPtrSend());
  copy->failNo = 0;
  copy->masterNodeId = 0;
  copy->noOfNodes = 0;
  NodeBitmask::clear(copy->theNodes);

  for (Uint32 i = NdbNodeBitmask::find_first(rep->theNodes);
       i != NdbNodeBitmask::NotFound;
       i = NdbNodeBitmask::find_next(rep->theNodes, i + 1))
  {
    Node & cm_node = theNodes[i];
    trp_node & theNode = cm_node;

    bool node_failrep = theNode.m_node_fail_rep;
    bool connected = theNode.is_connected();
    set_node_dead(theNode);

    if (node_failrep == false)
    {
      theNode.m_node_fail_rep = true;
      NodeBitmask::set(copy->theNodes, i);
      copy->noOfNodes++;
    }

    if (connected)
    {
      theFacade.doDisconnect(i);
    }
  }

  recalcMinDbVersion();
  if (copy->noOfNodes)
  {
    theFacade.for_each(this, &signal, 0); // report GSN_NODE_FAILREP
  }

  if (noOfAliveNodes == 0)
  {
    NdbApiSignal signal(numberToRef(API_CLUSTERMGR, getOwnNodeId()));
    signal.theVerId_signalNumber = GSN_NF_COMPLETEREP;
    signal.theReceiversBlockNumber = 0;
    signal.theTrace  = 0;
    signal.theLength = NFCompleteRep::SignalLength;

    NFCompleteRep * rep = CAST_PTR(NFCompleteRep, signal.getDataPtrSend());
    rep->blockNo =0;
    rep->nodeId = getOwnNodeId();
    rep->unused = 0;
    rep->from = __LINE__;

    for (Uint32 i = 1; i < MAX_NODES; i++)
    {
      trp_node& theNode = theNodes[i];
      if (theNode.defined && theNode.nfCompleteRep == false)
      {
        rep->failedNodeId = i;
        execNF_COMPLETEREP(&signal, 0);
      }
    }
  }
}

void
ClusterMgr::print_nodes(const char* where, NdbOut& out)
{
  out << where << " >>" << endl;
  for (NodeId n = 1; n < MAX_NODES ; n++)
  {
    const trp_node node = getNodeInfo(n);
    if (!node.defined)
      continue;
    out << "node: " << n << endl;
    out << " -";
    out << " connected: " << node.is_connected();
    out << ", compatible: " << node.compatible;
    out << ", nf_complete_rep: " << node.nfCompleteRep;
    out << ", alive: " << node.m_alive;
    out << ", confirmed: " << node.is_confirmed();
    out << endl;

    out << " - " << node.m_info << endl;
    out << " - " << node.m_state << endl;
  }
  out << "<<" << endl;
}


/******************************************************************************
 * Arbitrator
 ******************************************************************************/
ArbitMgr::ArbitMgr(ClusterMgr & c)
  : m_clusterMgr(c)
{
  DBUG_ENTER("ArbitMgr::ArbitMgr");

  theThreadMutex = NdbMutex_Create();
  theInputCond = NdbCondition_Create();
  theInputMutex = NdbMutex_Create();
  
  theRank = 0;
  theDelay = 0;
  theThread = 0;

  theInputTimeout = 0;
  theInputFull = false;
  memset(&theInputBuffer, 0, sizeof(theInputBuffer));
  theState = StateInit;

  memset(&theStartReq, 0, sizeof(theStartReq));
  memset(&theChooseReq1, 0, sizeof(theChooseReq1));
  memset(&theChooseReq2, 0, sizeof(theChooseReq2));
  memset(&theStopOrd, 0, sizeof(theStopOrd));

  DBUG_VOID_RETURN;
}

ArbitMgr::~ArbitMgr()
{
  DBUG_ENTER("ArbitMgr::~ArbitMgr");
  NdbMutex_Destroy(theThreadMutex);
  NdbCondition_Destroy(theInputCond);
  NdbMutex_Destroy(theInputMutex);
  DBUG_VOID_RETURN;
}

// Start arbitrator thread.  This is kernel request.
// First stop any previous thread since it is a left-over
// which was never used and which now has wrong ticket.
void
ArbitMgr::doStart(const Uint32* theData)
{
  ArbitSignal aSignal;
  NdbMutex_Lock(theThreadMutex);
  if (theThread != NULL) {
    aSignal.init(GSN_ARBIT_STOPORD, NULL);
    aSignal.data.code = StopRestart;
    sendSignalToThread(aSignal);
    void* value;
    NdbThread_WaitFor(theThread, &value);
    NdbThread_Destroy(&theThread);
    theState = StateInit;
    theInputFull = false;
  }
  aSignal.init(GSN_ARBIT_STARTREQ, theData);
  sendSignalToThread(aSignal);
  theThread = NdbThread_Create(
    runArbitMgr_C, (void**)this,
    0, // default stack size
    "ndb_arbitmgr",
    NDB_THREAD_PRIO_HIGH);
  NdbMutex_Unlock(theThreadMutex);
}

// The "choose me" signal from a candidate.
void
ArbitMgr::doChoose(const Uint32* theData)
{
  ArbitSignal aSignal;
  aSignal.init(GSN_ARBIT_CHOOSEREQ, theData);
  sendSignalToThread(aSignal);
}

// Stop arbitrator thread via stop signal from the kernel
// or when exiting API program.
void
ArbitMgr::doStop(const Uint32* theData)
{
  DBUG_ENTER("ArbitMgr::doStop");
  ArbitSignal aSignal;
  NdbMutex_Lock(theThreadMutex);
  if (theThread != NULL) {
    aSignal.init(GSN_ARBIT_STOPORD, theData);
    if (theData == 0) {
      aSignal.data.code = StopExit;
    } else {
      aSignal.data.code = StopRequest;
    }
    sendSignalToThread(aSignal);
    void* value;
    NdbThread_WaitFor(theThread, &value);
    NdbThread_Destroy(&theThread);
    theState = StateInit;
  }
  NdbMutex_Unlock(theThreadMutex);
  DBUG_VOID_RETURN;
}

// private methods

extern "C" 
void*
runArbitMgr_C(void* me)
{
  ((ArbitMgr*) me)->threadMain();
  return NULL;
}

void
ArbitMgr::sendSignalToThread(ArbitSignal& aSignal)
{
#ifdef DEBUG_ARBIT
  char buf[17] = "";
  ndbout << "arbit recv: ";
  ndbout << " gsn=" << aSignal.gsn;
  ndbout << " send=" << aSignal.data.sender;
  ndbout << " code=" << aSignal.data.code;
  ndbout << " node=" << aSignal.data.node;
  ndbout << " ticket=" << aSignal.data.ticket.getText(buf, sizeof(buf));
  ndbout << " mask=" << aSignal.data.mask.getText(buf, sizeof(buf));
  ndbout << endl;
#endif
  aSignal.setTimestamp();       // signal arrival time
  NdbMutex_Lock(theInputMutex);
  while (theInputFull) {
    NdbCondition_WaitTimeout(theInputCond, theInputMutex, 1000);
  }
  theInputBuffer = aSignal;
  theInputFull = true;
  NdbCondition_Signal(theInputCond);
  NdbMutex_Unlock(theInputMutex);
}

void
ArbitMgr::threadMain()
{
  ArbitSignal aSignal;
  aSignal = theInputBuffer;
  threadStart(aSignal);
  bool stop = false;
  while (! stop) {
    NdbMutex_Lock(theInputMutex);
    while (! theInputFull) {
      NdbCondition_WaitTimeout(theInputCond, theInputMutex, theInputTimeout);
      threadTimeout();
    }
    aSignal = theInputBuffer;
    theInputFull = false;
    NdbCondition_Signal(theInputCond);
    NdbMutex_Unlock(theInputMutex);
    switch (aSignal.gsn) {
    case GSN_ARBIT_CHOOSEREQ:
      threadChoose(aSignal);
      break;
    case GSN_ARBIT_STOPORD:
      stop = true;
      break;
    }
  }
  threadStop(aSignal);
}

// handle events in the thread

void
ArbitMgr::threadStart(ArbitSignal& aSignal)
{
  theStartReq = aSignal;
  sendStartConf(theStartReq, ArbitCode::ApiStart);
  theState = StateStarted;
  theInputTimeout = 1000;
}

void
ArbitMgr::threadChoose(ArbitSignal& aSignal)
{
  switch (theState) {
  case StateStarted:            // first REQ
    if (! theStartReq.data.match(aSignal.data)) {
      sendChooseRef(aSignal, ArbitCode::ErrTicket);
      break;
    }
    theChooseReq1 = aSignal;
    if (theDelay == 0) {
      sendChooseConf(aSignal, ArbitCode::WinChoose);
      theState = StateFinished;
      theInputTimeout = 1000;
      break;
    }
    theState = StateChoose1;
    theInputTimeout = 1;
    return;
  case StateChoose1:            // second REQ within Delay
    if (! theStartReq.data.match(aSignal.data)) {
      sendChooseRef(aSignal, ArbitCode::ErrTicket);
      break;
    }
    theChooseReq2 = aSignal;
    theState = StateChoose2;
    theInputTimeout = 1;
    return;
  case StateChoose2:            // too many REQs - refuse all
    if (! theStartReq.data.match(aSignal.data)) {
      sendChooseRef(aSignal, ArbitCode::ErrTicket);
      break;
    }
    sendChooseRef(theChooseReq1, ArbitCode::ErrToomany);
    sendChooseRef(theChooseReq2, ArbitCode::ErrToomany);
    sendChooseRef(aSignal, ArbitCode::ErrToomany);
    theState = StateFinished;
    theInputTimeout = 1000;
    return;
  default:
    sendChooseRef(aSignal, ArbitCode::ErrState);
    break;
  }
}

void
ArbitMgr::threadTimeout()
{
  switch (theState) {
  case StateStarted:
    break;
  case StateChoose1:
    if (theChooseReq1.getTimediff() < theDelay)
      break;
    sendChooseConf(theChooseReq1, ArbitCode::WinChoose);
    theState = StateFinished;
    theInputTimeout = 1000;
    break;
  case StateChoose2:
    sendChooseConf(theChooseReq1, ArbitCode::WinChoose);
    sendChooseConf(theChooseReq2, ArbitCode::LoseChoose);
    theState = StateFinished;
    theInputTimeout = 1000;
    break;
  default:
    break;
  }
}

void
ArbitMgr::threadStop(ArbitSignal& aSignal)
{
  switch (aSignal.data.code) {
  case StopExit:
    switch (theState) {
    case StateStarted:
      sendStopRep(theStartReq, 0);
      break;
    case StateChoose1:                  // just in time
      sendChooseConf(theChooseReq1, ArbitCode::WinChoose);
      break;
    case StateChoose2:
      sendChooseConf(theChooseReq1, ArbitCode::WinChoose);
      sendChooseConf(theChooseReq2, ArbitCode::LoseChoose);
      break;
    case StateInit:
    case StateFinished:
      //??
      break;
    }
    break;
  case StopRequest:
    break;
  case StopRestart:
    break;
  }
}

// output routines

void
ArbitMgr::sendStartConf(ArbitSignal& aSignal, Uint32 code)
{
  ArbitSignal copySignal = aSignal;
  copySignal.gsn = GSN_ARBIT_STARTCONF;
  copySignal.data.code = code;
  sendSignalToQmgr(copySignal);
}

void
ArbitMgr::sendChooseConf(ArbitSignal& aSignal, Uint32 code)
{
  ArbitSignal copySignal = aSignal;
  copySignal.gsn = GSN_ARBIT_CHOOSECONF;
  copySignal.data.code = code;
  sendSignalToQmgr(copySignal);
}

void
ArbitMgr::sendChooseRef(ArbitSignal& aSignal, Uint32 code)
{
  ArbitSignal copySignal = aSignal;
  copySignal.gsn = GSN_ARBIT_CHOOSEREF;
  copySignal.data.code = code;
  sendSignalToQmgr(copySignal);
}

void
ArbitMgr::sendStopRep(ArbitSignal& aSignal, Uint32 code)
{
  ArbitSignal copySignal = aSignal;
  copySignal.gsn = GSN_ARBIT_STOPREP;
  copySignal.data.code = code;
  sendSignalToQmgr(copySignal);
}

/**
 * Send signal to QMGR.  The input includes signal number and
 * signal data.  The signal data is normally a copy of a received
 * signal so it contains expected arbitrator node id and ticket.
 * The sender in signal data is the QMGR node id.
 */
void
ArbitMgr::sendSignalToQmgr(ArbitSignal& aSignal)
{
  NdbApiSignal signal(numberToRef(API_CLUSTERMGR, m_clusterMgr.getOwnNodeId()));

  signal.theVerId_signalNumber = aSignal.gsn;
  signal.theReceiversBlockNumber = QMGR;
  signal.theTrace  = 0;
  signal.theLength = ArbitSignalData::SignalLength;

  ArbitSignalData* sd = CAST_PTR(ArbitSignalData, signal.getDataPtrSend());

  sd->sender = numberToRef(API_CLUSTERMGR, m_clusterMgr.getOwnNodeId());
  sd->code = aSignal.data.code;
  sd->node = aSignal.data.node;
  sd->ticket = aSignal.data.ticket;
  sd->mask = aSignal.data.mask;

#ifdef DEBUG_ARBIT
  char buf[17] = "";
  ndbout << "arbit send: ";
  ndbout << " gsn=" << aSignal.gsn;
  ndbout << " recv=" << aSignal.data.sender;
  ndbout << " code=" << aSignal.data.code;
  ndbout << " node=" << aSignal.data.node;
  ndbout << " ticket=" << aSignal.data.ticket.getText(buf, sizeof(buf));
  ndbout << " mask=" << aSignal.data.mask.getText(buf, sizeof(buf));
  ndbout << endl;
#endif

  {
    m_clusterMgr.lock();
    m_clusterMgr.raw_sendSignal(&signal, aSignal.data.sender);
    m_clusterMgr.unlock();
  }
}


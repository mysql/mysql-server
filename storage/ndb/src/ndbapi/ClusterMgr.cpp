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
#include <ProcessInfo.hpp>
#include <OwnProcessInfo.hpp>

#include <signaldata/NodeFailRep.hpp>
#include <signaldata/NFCompleteRep.hpp>
#include <signaldata/ApiRegSignalData.hpp>
#include <signaldata/AlterTable.hpp>
#include <signaldata/SumaImpl.hpp>
#include <signaldata/ProcessInfoRep.hpp>

#include <mgmapi.h>
#include <mgmapi_configuration.hpp>
#include <mgmapi_config_parameters.h>

#if 0
#define DEBUG_FPRINTF(arglist) do { fprintf arglist ; } while (0)
#else
#define DEBUG_FPRINTF(a)
#endif

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
  m_sent_API_REGREQ_to_myself(false),
  theFacade(_facade),
  theArbitMgr(NULL),
  m_connect_count(0),
  m_max_api_reg_req_interval(~0),
  noOfAliveNodes(0),
  noOfConnectedNodes(0),
  noOfConnectedDBNodes(0),
  minDbVersion(0),
  theClusterMgrThread(NULL),
  m_process_info(NULL),
  m_cluster_state(CS_waiting_for_clean_cache),
  m_hbFrequency(0)
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
  assert(theStop == 1);
  if (theArbitMgr != 0)
  {
    delete theArbitMgr;
    theArbitMgr = 0;
  }
  NdbCondition_Destroy(waitForHBCond);
  NdbMutex_Destroy(clusterMgrThreadMutex);
  ProcessInfo::release(m_process_info);
  DBUG_VOID_RETURN;
}

/**
 * This method is called from start of cluster connection instance and
 * before we have started any socket services and thus it needs no
 * mutex protection since the ClusterMgr object isn't known by any other
 * thread at this point in time.
 */
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

  // Configure heartbeats.
  unsigned hbFrequency = 0;
  iter.get(CFG_MGMD_MGMD_HEARTBEAT_INTERVAL, &hbFrequency);
  m_hbFrequency = static_cast<Uint32>(hbFrequency);

  // Configure max backoff time for connection attempts to first
  // data node.
  Uint32 backoff_max_time = 0;
  iter.get(CFG_START_CONNECT_BACKOFF_MAX_TIME,
           &backoff_max_time);
  start_connect_backoff_max_time = backoff_max_time;

  // Configure max backoff time for connection attempts to data
  // nodes.
  backoff_max_time = 0;
  iter.get(CFG_CONNECT_BACKOFF_MAX_TIME, &backoff_max_time);
  connect_backoff_max_time = backoff_max_time;

  theFacade.get_registry()->set_connect_backoff_max_time_in_ms(
    start_connect_backoff_max_time);

  m_process_info = ProcessInfo::forNodeId(nodeId);
}

void
ClusterMgr::startThread()
{
  DBUG_ENTER("ClusterMgr::startThread");
  /**
   * We use the clusterMgrThreadMutex as a signalling object between this
   * thread and the main thread of the ClusterMgr.
   * The clusterMgrThreadMutex also protects the theStop-variable.
   */
  Guard g(clusterMgrThreadMutex);

  theStop = -1;
  theClusterMgrThread = NdbThread_Create(runClusterMgr_C,
                                         (void**)this,
                                         0, // default stack size
                                         "ndb_clustermgr",
                                         NDB_THREAD_PRIO_HIGH);
  if (theClusterMgrThread == NULL)
  {
    ndbout_c("ClusterMgr::startThread: Failed to create thread for cluster management.");
    assert(theClusterMgrThread != NULL);
    DBUG_VOID_RETURN;
  }

  Uint32 cnt = 0;
  while (theStop == -1 && cnt < 60)
  {
    NdbCondition_WaitTimeout(waitForHBCond, clusterMgrThreadMutex, 1000);
  }

  assert(theStop == 0);
  DBUG_VOID_RETURN;
}

void
ClusterMgr::doStop( ){
  DBUG_ENTER("ClusterMgr::doStop");
  {
    /* Ensure stop is only executed once */
    Guard g(clusterMgrThreadMutex);
    if(theStop == 1){
      DBUG_VOID_RETURN;
    }
    theStop = 1;
  }

  void *status;
  if (theClusterMgrThread) {
    NdbThread_WaitFor(theClusterMgrThread, &status);  
    NdbThread_Destroy(&theClusterMgrThread);
  }

  if (theArbitMgr != NULL)
  {
    theArbitMgr->doStop(NULL);
  }
  {
    /**
     * Need protection against concurrent execution of do_poll in main
     * thread. We cannot rely only on the trp_client lock since it is
     * not supposed to be locked when calling close (it is locked as
     * part of the close logic.
     */
    Guard g(clusterMgrThreadMutex);
    this->close(); // disconnect from TransporterFacade
  }

  DBUG_VOID_RETURN;
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
  flush_send_buffers();
  unlock();

  for (Uint32 i = 0; i<3000; i++)
  {
    theFacade.request_connection_check();
    prepare_poll();
    do_poll(0);
    complete_poll();

    if (theNode.is_connected())
      break;
    NdbSleep_MilliSleep(20);
  }

  assert(theNode.is_connected());
  Guard g(clusterMgrThreadMutex);
  /* Signalling to creating thread that we are done with thread startup */
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

  NDB_TICKS now = NdbTick_getCurrentTicks();

  while(!theStop)
  {
    /* Sleep 1/5 of minHeartBeatInterval between each check */
    const NDB_TICKS before = now;
    for (Uint32 i = 0; i<5; i++)
    {
      NdbSleep_MilliSleep(minHeartBeatInterval/5);
      {
        /**
         * prepare_poll does lock the trp_client and complete_poll
         * releases this lock. This means that this protects
         * against concurrent calls to send signals in ArbitMgr.
         * We do however need to protect also against concurrent
         * close in doStop, so to avoid this problem we need to
         * also lock clusterMgrThreadMutex before we start the
         * poll.
         */
        Guard g(clusterMgrThreadMutex);
        prepare_poll();
        do_poll(0);
        complete_poll();
      }
    }
    now = NdbTick_getCurrentTicks();
    const Uint32 timeSlept = (Uint32)NdbTick_Elapsed(before, now).milliSec();

    lock();
    if (m_cluster_state == CS_waiting_for_clean_cache &&
        theFacade.m_globalDictCache)
    {
      if (!global_flag_skip_waiting_for_clean_cache)
      {
        theFacade.m_globalDictCache->lock();
        unsigned sz= theFacade.m_globalDictCache->get_size();
        theFacade.m_globalDictCache->unlock();
        if (sz)
        {
          unlock();
          continue;
        }
      }
      m_cluster_state = CS_waiting_for_first_connect;
    }

    NodeFailRep * nodeFailRep = CAST_PTR(NodeFailRep,
                                         nodeFail_signal.getDataPtrSend());
    nodeFailRep->noOfNodes = 0;
    NodeBitmask::clear(nodeFailRep->theAllNodes);

    for (int i = 1; i < MAX_NODES; i++)
    {
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
      
      if (nodeId == getOwnNodeId())
      {
        /**
         * Don't send HB to self more than once
         * (once needed to avoid weird special cases in e.g ConfigManager)
         */
        if (m_sent_API_REGREQ_to_myself)
        {
          continue;
        }
      }

      cm_node.hbCounter += timeSlept;
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
        if (nodeId == getOwnNodeId())
        {
          /* Set flag to ensure we only send once to ourself */
          m_sent_API_REGREQ_to_myself = true;
        }
	raw_sendSignal(&signal, nodeId);
      }//if
      
      if (cm_node.hbMissed == 4 && cm_node.hbFrequency > 0)
      {
        nodeFailRep->noOfNodes++;
        NodeBitmask::set(nodeFailRep->theAllNodes, nodeId);
      }
    }
    flush_send_buffers();
    unlock();

    if (nodeFailRep->noOfNodes)
    {
      lock();
      raw_sendSignal(&nodeFail_signal, getOwnNodeId());
      flush_send_buffers();
      unlock();
    }
  }
}

/**
 * We're holding the trp_client lock while performing poll from
 * ClusterMgr. So we always execute all the execSIGNAL-methods in
 * ClusterMgr with protection other methods that use the trp_client
 * lock (reportDisconnect, reportConnect, is_cluster_completely_unavailable,
 * ArbitMgr (sendSignalToQmgr)).
 */
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

      // Send signal without delay, otherwise, Suma buffers may
      // overflow, resulting into the API node being disconnected.
      // SUB_GCP_COMPLETE_ACK will be sent per node per epoch, with
      // minimum interval of TimeBetweenEpochs.
      safe_sendSignal(&tSignal, aNodeId);

      /**
       * Note:
       * After fixing #Bug#22705935 'sendSignal() flush optimization isses',
       * we could likely just as well have used safe_noflush_sendSignal() above.
       * (and several other places)
       * That patch ensures that any buffered signals sent while 
       * delivering signals are flushed as soon as we have processed the 
       * chunk of signals to be delivered.
       */ 
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
  case GSN_CLOSE_COMREQ:
  {
    theFacade.perform_close_clnt(this);
    return;
  }
  case GSN_EXPAND_CLNT:
  {
    theFacade.expand_clnt();
    return;
  }
  default:
    break;

  }
  return;
}

ClusterMgr::Node::Node()
  : hbFrequency(0), hbCounter(0), processInfoSent(0)
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
 * Send PROCESSINFO_REP
 ******************************************************************************/
void
ClusterMgr::sendProcessInfoReport(NodeId nodeId)
{
  LinearSectionPtr ptr[3];
  LinearSectionPtr & pathSection = ptr[ProcessInfoRep::PathSectionNum];
  LinearSectionPtr & hostSection = ptr[ProcessInfoRep::HostSectionNum];
  BlockReference ownRef = numberToRef(API_CLUSTERMGR, theFacade.ownId());
  NdbApiSignal signal(ownRef);
  int nsections = 0;
  signal.theVerId_signalNumber = GSN_PROCESSINFO_REP;
  signal.theReceiversBlockNumber = QMGR;
  signal.theTrace  = 0;
  signal.theLength = ProcessInfoRep::SignalLength;

  ProcessInfoRep * report = CAST_PTR(ProcessInfoRep, signal.getDataPtrSend());
  m_process_info->buildProcessInfoReport(report);

  const char * uri_path = m_process_info->getUriPath();
  pathSection.p = (Uint32 *) uri_path;
  pathSection.sz = ProcessInfo::UriPathLengthInWords;
  if(uri_path[0])
  {
    nsections = 1;
  }

  const char * hostAddress = m_process_info->getHostAddress();
  if(hostAddress[0])
  {
    nsections = 2;
    hostSection.p = (Uint32 *) hostAddress;
    hostSection.sz = ProcessInfo::AddressStringLengthInWords;
  }
  safe_noflush_sendSignal(&signal, nodeId, ptr, nsections);
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

  /* 
     API nodes send API_REGREQ once to themselves. Other than that, there are
     no API-API heart beats.
  */
  assert(cm_node.m_info.m_type != NodeInfo::API ||
         (nodeId == getOwnNodeId() &&
          !cm_node.is_confirmed()));

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

  /*
    This is the frequency (in centiseonds) at which we want the other node
    to send API_REGREQ messages.
  */
  conf->apiHeartbeatFrequency = m_hbFrequency/10;

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
  /*
    By convention, conf->apiHeartbeatFrequency is in centiseconds rather than
    milliseconds. See also Qmgr::sendApiRegConf().
   */
  const Int64 freq = 
    (static_cast<Int64>(apiRegConf->apiHeartbeatFrequency) * 10) - 50;

  if (freq > UINT_MAX32)
  {
    // In case of overflow.
    assert(false);  /* Note this assert fails on some upgrades... */
    cm_node.hbFrequency = UINT_MAX32;
  }
  else if (freq < minHeartBeatInterval)
  {
    /** 
     * We use minHeartBeatInterval as a lower limit. This also prevents 
     * against underflow.
     */
    cm_node.hbFrequency = minHeartBeatInterval;
  }
  else
  {
    cm_node.hbFrequency = static_cast<Uint32>(freq);
  }

  // If responding nodes indicates that it is connected to other
  // nodes, that makes it probable that those nodes are alive and
  // available also for this node.
  for (int db_node_id = 1; db_node_id <= MAX_DATA_NODE_ID; db_node_id ++)
  {
    if (node.m_state.m_connected_nodes.get(db_node_id))
    {
      // Tell this nodes start clients thread that db_node_id
      // is up and probable connectable.
      theFacade.theTransporterRegistry->indicate_node_up(db_node_id);
    }
  }

  /* Send ProcessInfo Report to a newly connected DB node */
  if ( cm_node.m_info.m_type == NodeInfo::DB &&
       ndbd_supports_processinfo(cm_node.m_info.m_version) &&
       (! cm_node.processInfoSent) )
  {
    sendProcessInfoReport(nodeId);
    cm_node.processInfoSent = true;
  }

  // Distribute signal to all threads/blocks
  // TODO only if state changed...
  theFacade.for_each(this, signal, ptr);
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

/**
 * ::reportConnected() and ::reportDisconnected()
 *
 * Should be called from the client thread being the poll owner,
 * which could either be ClusterMgr itself, or another API client.
 *
 * As ClusterMgr maintains shared global data, updating
 * its connection state needs m_mutex being locked.
 * If ClusterMgr is the poll owner, it already owns that
 * lock, else it has to be locked now.
 */
void
ClusterMgr::reportConnected(NodeId nodeId)
{
  DBUG_ENTER("ClusterMgr::reportConnected");
  DBUG_PRINT("info", ("nodeId: %u", nodeId));
  assert(theFacade.is_poll_owner_thread());

  if (theFacade.m_poll_owner != this)
    lock();

  assert(nodeId > 0 && nodeId < MAX_NODES);
  if (nodeId != getOwnNodeId())
  {
    noOfConnectedNodes++;
  }

  Node & cm_node = theNodes[nodeId];
  trp_node & theNode = cm_node;

  if (theNode.m_info.m_type == NodeInfo::DB)
  {
    noOfConnectedDBNodes++;
    if (noOfConnectedDBNodes == 1)
    {
      // Data node connected, use ConnectBackoffMaxTime
      theFacade.get_registry()->set_connect_backoff_max_time_in_ms(connect_backoff_max_time);
    }
  }

  /**
   * Ensure that we are sending heartbeat every 100 ms
   * until we have got the first reply from NDB providing
   * us with the real time-out period to use.
   */
  cm_node.hbMissed = 0;
  cm_node.hbCounter = 0;
  cm_node.hbFrequency = 0;
  cm_node.processInfoSent = false;

  assert(theNode.is_connected() == false);

  /**
   * make sure the node itself is marked connected even
   * if first API_REGCONF has not arrived
   */
  DEBUG_FPRINTF((stderr, "(%u)theNode.set_connected(true) for node: %u\n",
                         getOwnNodeId(), nodeId));
  theNode.set_connected(true);
  theNode.m_state.m_connected_nodes.set(nodeId);
  theNode.m_info.m_version = 0;
  theNode.compatible = true;
  theNode.nfCompleteRep = true;
  theNode.m_node_fail_rep = false;
  theNode.m_state.startLevel = NodeState::SL_NOTHING;
  theNode.minDbVersion = 0;

  /**
   * End of protected ClusterMgr updates of shared global data.
   * Informing other API client does not need a global protection
   */ 
  if (theFacade.m_poll_owner != this)
    unlock();
  
  /**
   * We are called by the poll owner (asserted above), so we can
   * tell each API client about the CONNECT_REP ourself.
   */
  NdbApiSignal signal(numberToRef(API_CLUSTERMGR, getOwnNodeId()));
  signal.theVerId_signalNumber = GSN_CONNECT_REP;
  signal.theReceiversBlockNumber = API_CLUSTERMGR;
  signal.theTrace  = 0;
  signal.theLength = 1;
  signal.getDataPtrSend()[0] = nodeId;
  theFacade.for_each(this, &signal, NULL);
  DBUG_VOID_RETURN;
}

void
ClusterMgr::reportDisconnected(NodeId nodeId)
{
  assert(theFacade.is_poll_owner_thread());
  assert(nodeId > 0 && nodeId < MAX_NODES);

  if (theFacade.m_poll_owner != this)
    lock();

  Node & cm_node = theNodes[nodeId];
  trp_node & theNode = cm_node;

  const bool node_failrep = theNode.m_node_fail_rep;
  const bool node_connected = theNode.is_connected();
  set_node_dead(theNode);
  DEBUG_FPRINTF((stderr, "(%u)theNode.set_connected(false) for node: %u\n",
                         getOwnNodeId(), nodeId));
  theNode.set_connected(false);

  /**
   * Remaining processing should only be done if the node
   * actually completed connecting...
   */
  if (unlikely(!node_connected))
  {
    assert(node_connected);
    if (theFacade.m_poll_owner != this)
      unlock();
    return;
  }

  assert(noOfConnectedNodes > 0);

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

  if (theNode.m_info.m_type == NodeInfo::DB)
  {
    assert(noOfConnectedDBNodes > 0);
    noOfConnectedDBNodes--;
    if (noOfConnectedDBNodes == 0)
    {
      // No data nodes connected, use StartConnectBackoffMaxTime
      theFacade.get_registry()->set_connect_backoff_max_time_in_ms(start_connect_backoff_max_time);
    }
  }

  /**
   * End of protected ClusterMgr updates of shared global data.
   * Informing other API client does not need a global protection
   */
  if (theFacade.m_poll_owner != this)
    unlock();

  if (node_failrep == false)
  {
    /**
     * Inform API
     *
     * We are called by the poll owner (asserted above), so we can
     * tell each API client about the NODE_FAILREP ourself.
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
    NodeBitmask::clear(rep->theAllNodes);
    NodeBitmask::set(rep->theAllNodes, nodeId);
    execNODE_FAILREP(&signal, 0);
  }
}

void
ClusterMgr::execNODE_FAILREP(const NdbApiSignal* sig,
                             const LinearSectionPtr ptr[])
{
  const NodeFailRep * rep = CAST_CONSTPTR(NodeFailRep, sig->getDataPtr());
  NodeBitmask mask;
  if (sig->getLength() == NodeFailRep::SignalLengthLong)
  {
    mask.assign(NodeBitmask::Size, rep->theAllNodes);
  }
  else
  {
    mask.assign(NdbNodeBitmask::Size, rep->theNodes);
  }

  NdbApiSignal signal(sig->theSendersBlockRef);
  signal.theVerId_signalNumber = GSN_NODE_FAILREP;
  signal.theReceiversBlockNumber = API_CLUSTERMGR;
  signal.theTrace  = 0;
  signal.theLength = NodeFailRep::SignalLengthLong;

  NodeFailRep * copy = CAST_PTR(NodeFailRep, signal.getDataPtrSend());
  copy->failNo = 0;
  copy->masterNodeId = 0;
  copy->noOfNodes = 0;
  NodeBitmask::clear(copy->theAllNodes);

  for (Uint32 i = mask.find_first(); i != NodeBitmask::NotFound;
       i = mask.find_next(i + 1))
  {
    Node & cm_node = theNodes[i];
    trp_node & theNode = cm_node;

    bool node_failrep = theNode.m_node_fail_rep;
    bool connected = theNode.is_connected();
    set_node_dead(theNode);

    if (node_failrep == false)
    {
      theNode.m_node_fail_rep = true;
      NodeBitmask::set(copy->theAllNodes, i);
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
ClusterMgr::set_node_dead(trp_node& theNode)
{
  set_node_alive(theNode, false);
  theNode.set_confirmed(false);
  theNode.m_state.m_connected_nodes.clear();
  theNode.m_state.startLevel = NodeState::SL_NOTHING;
  theNode.m_info.m_connectCount ++;
  theNode.nfCompleteRep = false;
}

bool
ClusterMgr::is_cluster_completely_unavailable()
{
  bool ret_code = true;

  /**
   * This method (and several other 'node state getters') allow
   * reading of theNodes[] from multiple block threads while 
   * ClusterMgr concurrently updates them. Thus, a mutex should
   * have been expected here. See bug#20391191, and addendum patches
   * to bug#19524096, to understand what prevents us from locking (yet)
   */
  for (NodeId n = 1; n < MAX_NDB_NODES ; n++)
  {
    const trp_node& node = theNodes[n];
    if (!node.defined)
    {
      /**
       * Node isn't even part of configuration.
       */
      continue;
    }
    if (node.m_state.startLevel > NodeState::SL_STARTED)
    {
      /**
       * Node is stopping, so isn't available for any transactions,
       * so not available for us to use.
       */
      continue;
    }
    if (!node.compatible)
    {
      /**
       * The node isn't compatible with ours, so we can't use it
       */
      continue;
    }
    if (node.m_alive ||
        node.m_state.startLevel == NodeState::SL_STARTING ||
        node.m_state.startLevel == NodeState::SL_STARTED)
    {
      /**
       * We found a node that is either alive (less likely since we call this
       * method), or it is in state SL_STARTING which means that we were
       * allowed to connect, this means that we will very shortly be able to
       * use this connection. So this means that we know that the current
       * connection problem is a temporary issue and we can report a temporary
       * error instead of reporting 4009.
       *
       * We can deduce that the cluster isn't ready to be declared down
       * yet, we have a link to a starting node. We either very soon have
       * a working cluster, or we already have a working cluster but we
       * haven't yet the most up-to-date information about the cluster state.
       * So the cluster will soon be available again very likely, so
       * we can report a temporary error rather than an unknown error.
       */
      ret_code = false;
      break;
    }
  }
  return ret_code;
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

void
ClusterMgr::setProcessInfoUri(const char * scheme, const char * address_string,
                              int port, const char * path)
{
  Guard g(clusterMgrThreadMutex);

  m_process_info->setUriScheme(scheme);
  m_process_info->setHostAddress(address_string);
  m_process_info->setPort(port);
  m_process_info->setUriPath(path);

  /* Set flag to resend ProcessInfo Report */
  for(int i = 1; i < MAX_NODES ; i++)
  {
    Node & node = theNodes[i];
    if(node.is_connected()) node.processInfoSent = false;
  }
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
  DBUG_ENTER("ArbitMgr::doStart");
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
  if (theThread == NULL)
  {
    ndbout_c("ArbitMgr::doStart: Failed to create thread for arbitration.");
    assert(theThread != NULL);
  }
  NdbMutex_Unlock(theThreadMutex);
  DBUG_VOID_RETURN;
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
    m_clusterMgr.flush_send_buffers();
    m_clusterMgr.unlock();
  }
}

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
#include <my_pthread.h>
#include <ndb_limits.h>
#include <ndb_version.h>

#include "TransporterFacade.hpp"
#include "ClusterMgr.hpp"
#include <IPCConfig.hpp>
#include "NdbApiSignal.hpp"
#include "API.hpp"
#include <NdbSleep.h>
#include <NdbOut.hpp>
#include <NdbTick.h>


#include <signaldata/NodeFailRep.hpp>
#include <signaldata/NFCompleteRep.hpp>
#include <signaldata/ApiRegSignalData.hpp>

#include <mgmapi.h>
#include <mgmapi_configuration.hpp>
#include <mgmapi_config_parameters.h>

int global_flag_send_heartbeat_now= 0;

// Just a C wrapper for threadMain
extern "C" 
void*
runClusterMgr_C(void * me)
{
  ((ClusterMgr*) me)->threadMain();
  /** 
   * Sleep to allow another thread that is not exiting to take control 
   * of signals allocated by this thread
   *
   * see Ndb::~Ndb() in Ndbinit.cpp
   */  
#ifdef NDB_OSE
  NdbSleep_MilliSleep(50);
#endif
  return NULL;
}

extern "C" {
  void ndbSetOwnVersion();
}
ClusterMgr::ClusterMgr(TransporterFacade & _facade):
  theStop(0),
  theFacade(_facade)
{
  DBUG_ENTER("ClusterMgr::ClusterMgr");
  ndbSetOwnVersion();
  clusterMgrThreadMutex = NdbMutex_Create();
  noOfAliveNodes= 0;
  noOfConnectedNodes= 0;
  theClusterMgrThread= 0;
  DBUG_VOID_RETURN;
}

ClusterMgr::~ClusterMgr()
{
  DBUG_ENTER("ClusterMgr::~ClusterMgr");
  doStop();  
  NdbMutex_Destroy(clusterMgrThreadMutex);
  DBUG_VOID_RETURN;
}

void
ClusterMgr::init(ndb_mgm_configuration_iterator & iter){
  for(iter.first(); iter.valid(); iter.next()){
    Uint32 tmp = 0;
    if(iter.get(CFG_NODE_ID, &tmp))
      continue;

    theNodes[tmp].defined = true;
#if 0
    ndbout << "--------------------------------------" << endl;
    ndbout << "--------------------------------------" << endl;
    ndbout_c("ClusterMgr: Node %d defined as %s", tmp, config.getNodeType(tmp));
#endif

    unsigned type;
    if(iter.get(CFG_TYPE_OF_SECTION, &type))
      continue;

    switch(type){
    case NODE_TYPE_DB:
      theNodes[tmp].m_info.m_type = NodeInfo::DB;
      break;
    case NODE_TYPE_API:
      theNodes[tmp].m_info.m_type = NodeInfo::API;
      break;
    case NODE_TYPE_MGM:
      theNodes[tmp].m_info.m_type = NodeInfo::MGM;
      break;
    default:
      type = type;
#if 0
      ndbout_c("ClusterMgr: Unknown node type: %d", type);
#endif
    }
  }
}

void
ClusterMgr::startThread() {
  NdbMutex_Lock(clusterMgrThreadMutex);
  
  theStop = 0;
  
  theClusterMgrThread = NdbThread_Create(runClusterMgr_C,
                                         (void**)this,
                                         32768,
                                         "ndb_clustermgr",
                                         NDB_THREAD_PRIO_LOW);
  NdbMutex_Unlock(clusterMgrThreadMutex);
}

void
ClusterMgr::doStop( ){
  DBUG_ENTER("ClusterMgr::doStop");
  NdbMutex_Lock(clusterMgrThreadMutex);
  if(theStop){
    NdbMutex_Unlock(clusterMgrThreadMutex);
    DBUG_VOID_RETURN;
  }
  void *status;
  theStop = 1;
  if (theClusterMgrThread) {
    NdbThread_WaitFor(theClusterMgrThread, &status);  
    NdbThread_Destroy(&theClusterMgrThread);
  }
  NdbMutex_Unlock(clusterMgrThreadMutex);
  DBUG_VOID_RETURN;
}

void
ClusterMgr::threadMain( ){
  NdbApiSignal signal(numberToRef(API_CLUSTERMGR, theFacade.ownId()));
  
  signal.theVerId_signalNumber   = GSN_API_REGREQ;
  signal.theReceiversBlockNumber = QMGR;
  signal.theTrace                = 0;
  signal.theLength               = ApiRegReq::SignalLength;

  ApiRegReq * req = CAST_PTR(ApiRegReq, signal.getDataPtrSend());
  req->ref = numberToRef(API_CLUSTERMGR, theFacade.ownId());
  req->version = NDB_VERSION;

  
  Uint32 timeSlept = 100;
  Uint64 now = NdbTick_CurrentMillisecond();

  while(!theStop){
    /**
     * Start of Secure area for use of Transporter
     */
    int send_heartbeat_now= global_flag_send_heartbeat_now;
    global_flag_send_heartbeat_now= 0;

    theFacade.lock_mutex();
    for (int i = 1; i < MAX_NODES; i++){
      /**
       * Send register request (heartbeat) to all available nodes 
       * at specified timing intervals
       */
      const NodeId nodeId = i;
      Node & theNode = theNodes[nodeId];
      
      if (!theNode.defined)
	continue;

      if (theNode.connected == false){
	theFacade.doConnect(nodeId);
	continue;
      }
      
      if (!theNode.compatible){
	continue;
      }
      
      theNode.hbCounter += timeSlept;
      if (theNode.hbCounter >= theNode.hbFrequency ||
	  send_heartbeat_now) {
	/**
	 * It is now time to send a new Heartbeat
	 */
	if (theNode.hbCounter >= theNode.hbFrequency) {
	  theNode.m_info.m_heartbeat_cnt++;
	  theNode.hbCounter = 0;
	}

#if 0 
	ndbout_c("ClusterMgr: Sending API_REGREQ to node %d", (int)nodeId);
#endif
	theFacade.sendSignalUnCond(&signal, nodeId);
      }//if
      
      if (theNode.m_info.m_heartbeat_cnt == 4 && theNode.hbFrequency > 0){
	reportNodeFailed(i);
      }//if
    }
    
    /**
     * End of secure area. Let other threads in
     */
    theFacade.unlock_mutex();
    
    // Sleep for 100 ms between each Registration Heartbeat
    Uint64 before = now;
    NdbSleep_MilliSleep(100); 
    now = NdbTick_CurrentMillisecond();
    timeSlept = (now - before);
  }
}

#if 0
void
ClusterMgr::showState(NodeId nodeId){
  ndbout << "-- ClusterMgr - NodeId = " << nodeId << endl;
  ndbout << "theNodeList      = " << theNodeList[nodeId] << endl;
  ndbout << "theNodeState     = " << theNodeState[nodeId] << endl;
  ndbout << "theNodeCount     = " << theNodeCount[nodeId] << endl;
  ndbout << "theNodeStopDelay = " << theNodeStopDelay[nodeId] << endl;
  ndbout << "theNodeSendDelay = " << theNodeSendDelay[nodeId] << endl;
}
#endif

ClusterMgr::Node::Node()
  : m_state(NodeState::SL_NOTHING) { 
  compatible = nfCompleteRep = true;
  connected = defined = m_alive = false; 
  m_state.m_connected_nodes.clear();
}

/******************************************************************************
 * API_REGREQ and friends
 ******************************************************************************/

void
ClusterMgr::execAPI_REGREQ(const Uint32 * theData){
  const ApiRegReq * const apiRegReq = (ApiRegReq *)&theData[0];
  const NodeId nodeId = refToNode(apiRegReq->ref);

#if 0
  ndbout_c("ClusterMgr: Recd API_REGREQ from node %d", nodeId);
#endif

  assert(nodeId > 0 && nodeId < MAX_NODES);

  Node & node = theNodes[nodeId];
  assert(node.defined == true);
  assert(node.connected == true);

  if(node.m_info.m_version != apiRegReq->version){
    node.m_info.m_version = apiRegReq->version;

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
  conf->apiHeartbeatFrequency = node.hbFrequency;
  theFacade.sendSignalUnCond(&signal, nodeId);
}

int global_mgmt_server_check = 0; // set to one in mgmtsrvr main;

void
ClusterMgr::execAPI_REGCONF(const Uint32 * theData){
  const ApiRegConf * const apiRegConf = (ApiRegConf *)&theData[0];
  const NodeId nodeId = refToNode(apiRegConf->qmgrRef);
  
#if 0 
  ndbout_c("ClusterMgr: Recd API_REGCONF from node %d", nodeId);
#endif

  assert(nodeId > 0 && nodeId < MAX_NODES);
  
  Node & node = theNodes[nodeId];
  assert(node.defined == true);
  assert(node.connected == true);

  if(node.m_info.m_version != apiRegConf->version){
    node.m_info.m_version = apiRegConf->version;
    if (global_mgmt_server_check == 1)
      node.compatible = ndbCompatible_mgmt_ndb(NDB_VERSION,
					       node.m_info.m_version);
    else
      node.compatible = ndbCompatible_api_ndb(NDB_VERSION,
					      node.m_info.m_version);
  }

  node.m_state = apiRegConf->nodeState;
  if (node.compatible && (node.m_state.startLevel == NodeState::SL_STARTED  ||
			  node.m_state.startLevel == NodeState::SL_SINGLEUSER)){
    set_node_alive(node, true);
  } else {
    set_node_alive(node, false);
  }//if
  node.m_info.m_heartbeat_cnt = 0;
  node.hbCounter = 0;
  node.hbFrequency = (apiRegConf->apiHeartbeatFrequency * 10) - 50;
}

void
ClusterMgr::execAPI_REGREF(const Uint32 * theData){
  
  ApiRegRef * ref = (ApiRegRef*)theData;
  
  const NodeId nodeId = refToNode(ref->ref);
  
  assert(nodeId > 0 && nodeId < MAX_NODES);
  
  Node & node = theNodes[nodeId];
  assert(node.connected == true);
  assert(node.defined == true);

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
ClusterMgr::execNODE_FAILREP(const Uint32 * theData){
  NodeFailRep * const nodeFail = (NodeFailRep *)&theData[0];
  for(int i = 1; i<MAX_NODES; i++){
    if(NodeBitmask::get(nodeFail->theNodes, i)){
      reportNodeFailed(i);
    }
  }
}

void
ClusterMgr::execNF_COMPLETEREP(const Uint32 * theData){
  NFCompleteRep * const nfComp = (NFCompleteRep *)theData;

  const NodeId nodeId = nfComp->failedNodeId;
  assert(nodeId > 0 && nodeId < MAX_NODES);
  
  theFacade.ReportNodeFailureComplete(nodeId);
  theNodes[nodeId].nfCompleteRep = true;
}

void
ClusterMgr::reportConnected(NodeId nodeId){
  /**
   * Ensure that we are sending heartbeat every 100 ms
   * until we have got the first reply from NDB providing
   * us with the real time-out period to use.
   */
  assert(nodeId > 0 && nodeId < MAX_NODES);

  noOfConnectedNodes++;

  Node & theNode = theNodes[nodeId];
  theNode.connected = true;
  theNode.m_info.m_heartbeat_cnt = 0;
  theNode.hbCounter = 0;

  /**
   * make sure the node itself is marked connected even
   * if first API_REGCONF has not arrived
   */
  theNode.m_state.m_connected_nodes.set(nodeId);
  theNode.hbFrequency = 0;
  theNode.m_info.m_version = 0;
  theNode.compatible = true;
  theNode.nfCompleteRep = true;
  
  theFacade.ReportNodeAlive(nodeId);
}

void
ClusterMgr::reportDisconnected(NodeId nodeId){
  assert(nodeId > 0 && nodeId < MAX_NODES);
  assert(noOfConnectedNodes > 0);

  noOfConnectedNodes--;
  theNodes[nodeId].connected = false;

  theNodes[nodeId].m_state.m_connected_nodes.clear();

  reportNodeFailed(nodeId);
}

void
ClusterMgr::reportNodeFailed(NodeId nodeId){

  Node & theNode = theNodes[nodeId];
 
  set_node_alive(theNode, false);
  theNode.m_info.m_connectCount ++;
  
  if(theNode.connected)
  {
    theFacade.doDisconnect(nodeId);
  }
  const bool report = (theNode.m_state.startLevel != NodeState::SL_NOTHING);  
  theNode.m_state.startLevel = NodeState::SL_NOTHING;
  
  if(report)
  {
    theFacade.ReportNodeDead(nodeId);
  }
  
  theNode.nfCompleteRep = false;
  if(noOfAliveNodes == 0)
  {
    NFCompleteRep rep;
    for(Uint32 i = 1; i<MAX_NODES; i++){
      if(theNodes[i].defined && theNodes[i].nfCompleteRep == false){
	rep.failedNodeId = i;
	execNF_COMPLETEREP((Uint32*)&rep);
      }
    }
  }
}

/******************************************************************************
 * Arbitrator
 ******************************************************************************/
ArbitMgr::ArbitMgr(TransporterFacade & _fac)
  : theFacade(_fac)
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
  memset(&theInputFull, 0, sizeof(theInputFull));
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
    runArbitMgr_C, (void**)this, 32768, "ndb_arbitmgr",
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
  NdbApiSignal signal(numberToRef(API_CLUSTERMGR, theFacade.ownId()));

  signal.theVerId_signalNumber = aSignal.gsn;
  signal.theReceiversBlockNumber = QMGR;
  signal.theTrace  = 0;
  signal.theLength = ArbitSignalData::SignalLength;

  ArbitSignalData* sd = CAST_PTR(ArbitSignalData, signal.getDataPtrSend());

  sd->sender = numberToRef(API_CLUSTERMGR, theFacade.ownId());
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

  theFacade.lock_mutex();
  theFacade.sendSignalUnCond(&signal, aSignal.data.sender);
  theFacade.unlock_mutex();
}


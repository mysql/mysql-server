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

#define QMGR_C
#include "Qmgr.hpp"
#include <pc.hpp>
#include <NdbTick.h>
#include <signaldata/NodeRecoveryStatusRep.hpp>
#include <signaldata/EventReport.hpp>
#include <signaldata/StartOrd.hpp>
#include <signaldata/CloseComReqConf.hpp>
#include <signaldata/PrepFailReqRef.hpp>
#include <signaldata/NodeFailRep.hpp>
#include <signaldata/ReadNodesConf.hpp>
#include <signaldata/NFCompleteRep.hpp>
#include <signaldata/CheckNodeGroups.hpp>
#include <signaldata/ArbitSignalData.hpp>
#include <signaldata/ApiRegSignalData.hpp>
#include <signaldata/ApiVersion.hpp>
#include <signaldata/BlockCommitOrd.hpp>
#include <signaldata/FailRep.hpp>
#include <signaldata/DisconnectRep.hpp>
#include <signaldata/ApiBroadcast.hpp>
#include <signaldata/Upgrade.hpp>
#include <signaldata/EnableCom.hpp>
#include <signaldata/RouteOrd.hpp>
#include <signaldata/NodePing.hpp>
#include <signaldata/DihRestart.hpp>
#include <signaldata/DumpStateOrd.hpp>
#include <signaldata/IsolateOrd.hpp>
#include <signaldata/ProcessInfoRep.hpp>
#include <signaldata/LocalSysfile.hpp>
#include <ndb_version.h>
#include <OwnProcessInfo.hpp>
#include <NodeInfo.hpp>

#include <TransporterRegistry.hpp> // Get connect address

#include <EventLogger.hpp>
extern EventLogger * g_eventLogger;

//#define DEBUG_QMGR_START
#ifdef DEBUG_QMGR_START
#include <DebuggerNames.hpp>
#define DEBUG(x) ndbout << "QMGR " << __LINE__ << ": " << x << endl
#define DEBUG_START(gsn, node, msg) DEBUG(getSignalName(gsn) << " to: " << node << " - " << msg)
#define DEBUG_START2(gsn, rg, msg) { char nodes[255]; DEBUG(getSignalName(gsn) << " to: " << rg.m_nodes.getText(nodes) << " - " << msg); }
#define DEBUG_START3(signal, msg) DEBUG(getSignalName(signal->header.theVerId_signalNumber) << " from " << refToNode(signal->getSendersBlockRef()) << " - " << msg);
#else
#define DEBUG(x)
#define DEBUG_START(gsn, node, msg)
#define DEBUG_START2(gsn, rg, msg)
#define DEBUG_START3(signal, msg)
#endif

#define JAM_FILE_ID 360


/**
 * c_start.m_gsn = GSN_CM_REGREQ
 *   Possible for all nodes
 *   c_start.m_nodes contains all nodes in config
 *
 * c_start.m_gsn = GSN_CM_NODEINFOREQ;
 *   Set when receiving CM_REGCONF
 *   State possible for starting node only (not in cluster)
 *
 *   c_start.m_nodes contains all node in alive cluster that
 *                   that has not replied to GSN_CM_NODEINFOREQ
 *                   passed by president in GSN_CM_REGCONF
 *
 * c_start.m_gsn = GSN_CM_ADD
 *   Possible for president only
 *   Set when receiving and accepting CM_REGREQ (to include node)
 *
 *   c_start.m_nodes contains all nodes in alive cluster + starting node
 *                   that has not replied to GSN_CM_ADD
 *                   by sending GSN_CM_ACKADD
 *
 * c_start.m_gsn = GSN_CM_NODEINFOCONF
 *   Possible for non presidents only
 *     c_start.m_nodes contains a node that has been accepted by president
 *     but has not connected to us yet
 */

// Signal entries and statement blocks
/* 4  P R O G R A M        */
/*******************************/
/* CMHEART_BEAT               */
/*******************************/
void Qmgr::execCM_HEARTBEAT(Signal* signal) 
{
  NodeRecPtr hbNodePtr;
  jamEntry();
  hbNodePtr.i = signal->theData[0];
  ptrCheckGuard(hbNodePtr, MAX_NDB_NODES, nodeRec);
  set_hb_count(hbNodePtr.i) = 0;
  return;
}//Qmgr::execCM_HEARTBEAT()

/*******************************/
/* CM_NODEINFOREF             */
/*******************************/
void Qmgr::execCM_NODEINFOREF(Signal* signal) 
{
  jamEntry();
  systemErrorLab(signal, __LINE__);
  return;
}//Qmgr::execCM_NODEINFOREF()

/*******************************/
/* CONTINUEB                  */
/*******************************/
void Qmgr::execCONTINUEB(Signal* signal) 
{
  jamEntry();
  const Uint32 tcontinuebType = signal->theData[0];
  const Uint32 tdata0 = signal->theData[1];
  const Uint32 tdata1 = signal->theData[2];
  switch (tcontinuebType) {
  case ZREGREQ_TIMELIMIT:
    jam();
    if (c_start.m_startKey != tdata0 || c_start.m_startNode != tdata1) {
      jam();
      return;
    }//if
    regreqTimeLimitLab(signal);
    break;
  case ZREGREQ_MASTER_TIMELIMIT:
    jam();
    if (c_start.m_startKey != tdata0 || c_start.m_startNode != tdata1) {
      jam();
      return;
    }//if
    //regreqMasterTimeLimitLab(signal);
    failReportLab(signal, c_start.m_startNode, FailRep::ZSTART_IN_REGREQ, getOwnNodeId());
    return;
    break;
  case ZTIMER_HANDLING:
    jam();
    timerHandlingLab(signal);
    return;
    break;
  case ZARBIT_HANDLING:
    jam();
    runArbitThread(signal);
    return;
    break;
  case ZSTART_FAILURE_LIMIT:{
    if (cpresident != ZNIL)
    {
      jam();
      return;
    }
    const NDB_TICKS now = NdbTick_getCurrentTicks();
    const Uint64 elapsed = NdbTick_Elapsed(c_start_election_time,now).milliSec();
    if (c_restartFailureTimeout != Uint32(~0) &&
        elapsed > c_restartFailureTimeout)
    {
      jam();
      BaseString tmp;
      tmp.append("Shutting down node as total restart time exceeds "
		 " StartFailureTimeout as set in config file ");
      if(c_restartFailureTimeout == (Uint32) ~0)
	tmp.append(" 0 (inifinite)");
      else
	tmp.appfmt(" %d", c_restartFailureTimeout);
      
      progError(__LINE__, NDBD_EXIT_SYSTEM_ERROR, tmp.c_str());
    }
    signal->theData[0] = ZSTART_FAILURE_LIMIT;
    sendSignalWithDelay(reference(), GSN_CONTINUEB, signal, 3000, 1);
    return;
  }
  default:
    jam();
    // ZCOULD_NOT_OCCUR_ERROR;
    systemErrorLab(signal, __LINE__);
    return;
    break;
  }//switch
  return;
}//Qmgr::execCONTINUEB()


void Qmgr::execDEBUG_SIG(Signal* signal) 
{
  NodeRecPtr debugNodePtr;
  jamEntry();
  debugNodePtr.i = signal->theData[0];
  ptrCheckGuard(debugNodePtr, MAX_NODES, nodeRec);
  return;
}//Qmgr::execDEBUG_SIG()

/*******************************/
/* FAIL_REP                   */
/*******************************/
void Qmgr::execFAIL_REP(Signal* signal) 
{
  const FailRep * const failRep = (FailRep *)&signal->theData[0];
  const NodeId failNodeId = failRep->failNodeId;
  const FailRep::FailCause failCause = (FailRep::FailCause)failRep->failCause; 
  Uint32 failSource = failRep->getFailSourceNodeId(signal->length());
  if (!failSource)
  {
    /* Failure source not included, use sender of signal as 'source' */
    failSource = refToNode(signal->getSendersBlockRef());
  }

  jamEntry();
  failReportLab(signal, failNodeId, failCause, failSource);
  return;
}//Qmgr::execFAIL_REP()

/*******************************/
/* PRES_TOREQ                 */
/*******************************/
void Qmgr::execPRES_TOREQ(Signal* signal) 
{
  jamEntry();
  BlockReference Tblockref = signal->theData[0];
  signal->theData[0] = getOwnNodeId();
  signal->theData[1] = ccommitFailureNr;
  sendSignal(Tblockref, GSN_PRES_TOCONF, signal, 2, JBA);
  return;
}//Qmgr::execPRES_TOREQ()

void 
Qmgr::execREAD_CONFIG_REQ(Signal* signal)
{
  jamEntry();

  const ReadConfigReq * req = (ReadConfigReq*)signal->getDataPtr();

  Uint32 ref = req->senderRef;
  Uint32 senderData = req->senderData;

  const ndb_mgm_configuration_iterator * p = 
    m_ctx.m_config.getOwnConfigIterator();
  ndbrequire(p != 0);

  ReadConfigConf * conf = (ReadConfigConf*)signal->getDataPtrSend();
  conf->senderRef = reference();
  conf->senderData = senderData;
  sendSignal(ref, GSN_READ_CONFIG_CONF, signal, 
	     ReadConfigConf::SignalLength, JBB);
}

void
Qmgr::execSTART_ORD(Signal* signal)
{
  /**
   * Start timer handling 
   */
  const NDB_TICKS now = NdbTick_getCurrentTicks();
  signal->theData[0] = ZTIMER_HANDLING;
  signal->theData[1] = Uint32(now.getUint64() >> 32);
  signal->theData[2] = Uint32(now.getUint64());
  sendSignal(QMGR_REF, GSN_CONTINUEB, signal, 3, JBB);
}

/*
4.2  ADD NODE MODULE*/
/*##########################################################################*/
/*
4.2.1 STTOR     */
/**--------------------------------------------------------------------------
 * Start phase signal, must be handled by all blocks. 
 * QMGR is only interested in the first phase. 
 * During phase one we clear all registered applications. 
 *---------------------------------------------------------------------------*/
/*******************************/
/* STTOR                      */
/*******************************/
void Qmgr::execSTTOR(Signal* signal) 
{
  jamEntry();
  
  switch(signal->theData[1]){
  case 1:
    initData(signal);
    g_eventLogger->info("Starting QMGR phase 1");
    startphase1(signal);
    recompute_version_info(NodeInfo::DB);
    recompute_version_info(NodeInfo::API);
    recompute_version_info(NodeInfo::MGM);
    return;
  case 7:
    if (cpresident == getOwnNodeId())
    {
      switch(arbitRec.method){
      case ArbitRec::DISABLED:
        break;

      case ArbitRec::METHOD_EXTERNAL:
      case ArbitRec::METHOD_DEFAULT:
        /**
         * Start arbitration thread.  This could be done as soon as
         * we have all nodes (or a winning majority).
         */
        jam();
        handleArbitStart(signal);
        break;
      }
    }
    break;
  case 9:{
    /**
     * Enable communication to all API nodes by setting state
     *   to ZFAIL_CLOSING (which will make it auto-open in checkStartInterface)
     */
    c_allow_api_connect = 1;
    NodeRecPtr nodePtr;
    for (nodePtr.i = 1; nodePtr.i < MAX_NODES; nodePtr.i++)
    {
      jam();
      Uint32 type = getNodeInfo(nodePtr.i).m_type;
      if (type != NodeInfo::API)
        continue;

      ptrAss(nodePtr, nodeRec);
      if (nodePtr.p->phase == ZAPI_INACTIVE)
      {
        jam();
        set_hb_count(nodePtr.i) = 3;
        nodePtr.p->phase = ZFAIL_CLOSING;
        nodePtr.p->failState = NORMAL;
      }
    }
  }
  }
  
  sendSttorryLab(signal, false);
  return;
}//Qmgr::execSTTOR()

void Qmgr::sendSttorryLab(Signal* signal, bool first_phase)
{
  if (first_phase)
  {
    g_eventLogger->info("Include node protocol completed, phase 1 in QMGR"
                        " completed");
  }
/****************************<*/
/*< STTORRY                  <*/
/****************************<*/
  signal->theData[3] = 7;
  signal->theData[4] = 9;
  signal->theData[5] = 255;
  sendSignal(NDBCNTR_REF, GSN_STTORRY, signal, 6, JBB);
  return;
}//Qmgr::sendSttorryLab()

void Qmgr::startphase1(Signal* signal) 
{
  jamEntry();

  NodeRecPtr nodePtr;
  nodePtr.i = getOwnNodeId();
  ptrAss(nodePtr, nodeRec);
  nodePtr.p->phase = ZSTARTING;

  DihRestartReq * req = CAST_PTR(DihRestartReq, signal->getDataPtrSend());
  req->senderRef = reference();
  sendSignal(DBDIH_REF, GSN_DIH_RESTARTREQ, signal,
             DihRestartReq::SignalLength, JBB);
  return;
}

void
Qmgr::execDIH_RESTARTREF(Signal*signal)
{
  jamEntry();

  g_eventLogger->info("DIH reported initial start, now starting the"
                      " Node Inclusion Protocol");
  const DihRestartRef * ref = CAST_CONSTPTR(DihRestartRef,
                                            signal->getDataPtr());
  c_start.m_latest_gci = 0;
  c_start.m_no_nodegroup_nodes.assign(NdbNodeBitmask::Size,
                                      ref->no_nodegroup_mask);
  execCM_INFOCONF(signal);
}

void
Qmgr::execDIH_RESTARTCONF(Signal*signal)
{
  jamEntry();

  const DihRestartConf * conf = CAST_CONSTPTR(DihRestartConf,
                                              signal->getDataPtr());
  c_start.m_latest_gci = conf->latest_gci;
  c_start.m_no_nodegroup_nodes.assign(NdbNodeBitmask::Size,
                                      conf->no_nodegroup_mask);
  sendReadLocalSysfile(signal);
}

void
Qmgr::sendReadLocalSysfile(Signal *signal)
{
  ReadLocalSysfileReq *req = (ReadLocalSysfileReq*)signal->getDataPtrSend();
  req->userPointer = 0;
  req->userReference = reference();
  sendSignal(NDBCNTR_REF,
             GSN_READ_LOCAL_SYSFILE_REQ,
             signal,
             ReadLocalSysfileReq::SignalLength,
             JBB);
}

void
Qmgr::execREAD_LOCAL_SYSFILE_CONF(Signal *signal)
{
  ReadLocalSysfileConf *conf = (ReadLocalSysfileConf*)signal->getDataPtr();
  if (conf->nodeRestorableOnItsOwn ==
      ReadLocalSysfileReq::NODE_RESTORABLE_ON_ITS_OWN)
  {
    g_eventLogger->info("DIH reported normal start, now starting the"
                        " Node Inclusion Protocol");
  }
  else if (conf->nodeRestorableOnItsOwn ==
           ReadLocalSysfileReq::NODE_NOT_RESTORABLE_ON_ITS_OWN)
  {
    /**
     * We set gci = 1 and rely here on that gci here is simply used
     * as a tool to decide which nodes can be started up on their
     * own and which node to choose as master node. Only nodes
     * where m_latest_gci is set to a real GCI can be choosen as
     * master nodes.
     */
    g_eventLogger->info("Node not restorable on its own, now starting the"
                        " Node Inclusion Protocol");
    c_start.m_latest_gci = ZUNDEFINED_GCI_LIMIT;
  }
  else
  {
    g_eventLogger->info("Node requires initial start, now starting the"
                        " Node Inclusion Protocol");
    c_start.m_latest_gci = 0;
  }
  execCM_INFOCONF(signal);
}

void Qmgr::setHbDelay(UintR aHbDelay)
{
  const NDB_TICKS now = NdbTick_getCurrentTicks();
  hb_send_timer.setDelay(aHbDelay < 10 ? 10 : aHbDelay);
  hb_send_timer.reset(now);
  hb_check_timer.setDelay(aHbDelay < 10 ? 10 : aHbDelay);
  hb_check_timer.reset(now);
}

void Qmgr::setHbApiDelay(UintR aHbApiDelay)
{
  const NDB_TICKS now = NdbTick_getCurrentTicks();
  chbApiDelay = (aHbApiDelay < 100 ? 100 : aHbApiDelay);
  hb_api_timer.setDelay(chbApiDelay);
  hb_api_timer.reset(now);
}

void Qmgr::setArbitTimeout(UintR aArbitTimeout)
{
  arbitRec.timeout = (aArbitTimeout < 10 ? 10 : aArbitTimeout);
}

void Qmgr::setCCDelay(UintR aCCDelay)
{
  const NDB_TICKS now = NdbTick_getCurrentTicks();
  if (aCCDelay == 0)
  {
    /* Connectivity check disabled */
    m_connectivity_check.m_enabled = false;
    m_connectivity_check.m_timer.setDelay(0);
  }
  else
  {
    m_connectivity_check.m_enabled = true;
    m_connectivity_check.m_timer.setDelay(aCCDelay < 10 ? 10 : aCCDelay);
    m_connectivity_check.m_timer.reset(now);
  }
}

void Qmgr::execCONNECT_REP(Signal* signal)
{
  jamEntry();
  const Uint32 connectedNodeId = signal->theData[0];

  if (ERROR_INSERTED(931))
  {
    jam();
    ndbout_c("Discarding CONNECT_REP(%d)", connectedNodeId);
    infoEvent("Discarding CONNECT_REP(%d)", connectedNodeId);
    return;
  }

  if (ERROR_INSERTED(941) &&
      getNodeInfo(connectedNodeId).getType() == NodeInfo::API)
  {
    jam();
    CLEAR_ERROR_INSERT_VALUE;
    ndbout_c("Discarding one API CONNECT_REP(%d)", connectedNodeId);
    infoEvent("Discarding one API CONNECT_REP(%d)", connectedNodeId);
    return;
  }

  if (c_connectedNodes.get(connectedNodeId) == false)
  {
    jam();
    setNodeInfo(connectedNodeId).m_version = 0;
    setNodeInfo(connectedNodeId).m_mysql_version = 0;
  }

  c_connectedNodes.set(connectedNodeId);

  {
    NodeRecPtr connectedNodePtr;
    connectedNodePtr.i = connectedNodeId;
    ptrCheckGuard(connectedNodePtr, MAX_NODES, nodeRec);
    connectedNodePtr.p->m_secret = 0;
  }

  NodeRecPtr myNodePtr;
  myNodePtr.i = getOwnNodeId();
  ptrCheckGuard(myNodePtr, MAX_NODES, nodeRec);
  NodeInfo connectedNodeInfo = getNodeInfo(connectedNodeId);
  switch(myNodePtr.p->phase){
  case ZRUNNING:
    jam();
    if (connectedNodeInfo.getType() == NodeInfo::DB)
    {
      ndbrequire(!c_clusterNodes.get(connectedNodeId));
    }
    break;
  case ZSTARTING:
    jam();
    break;
  case ZPREPARE_FAIL:
  case ZFAIL_CLOSING:
    jam();
    return;
  case ZAPI_ACTIVATION_ONGOING:
    ndbabort();
  case ZAPI_ACTIVE:
    ndbabort();
  case ZAPI_INACTIVE:
    ndbabort();
  case ZINIT:
    ndbrequire(getNodeInfo(connectedNodeId).m_type == NodeInfo::MGM);
    break;
  default:
    ndbabort();
  }

  if (connectedNodeInfo.getType() != NodeInfo::DB)
  {
    jam();
    return;
  }

  switch(c_start.m_gsn){
  case GSN_CM_REGREQ:
    jam();
    sendCmRegReq(signal, connectedNodeId);

    /**
     * We're waiting for CM_REGCONF c_start.m_nodes contains all configured
     *   nodes
     */
    ndbrequire(myNodePtr.p->phase == ZSTARTING);
    ndbrequire(c_start.m_nodes.isWaitingFor(connectedNodeId));
    return;
  case GSN_CM_NODEINFOREQ:
    jam();
    
    if (c_start.m_nodes.isWaitingFor(connectedNodeId))
    {
      jam();
      ndbrequire(getOwnNodeId() != cpresident);
      ndbrequire(myNodePtr.p->phase == ZSTARTING);
      sendCmNodeInfoReq(signal, connectedNodeId, myNodePtr.p);
      return;
    }
    return;
  case GSN_CM_NODEINFOCONF:{
    jam();
    
    ndbrequire(getOwnNodeId() != cpresident);
    ndbrequire(myNodePtr.p->phase == ZRUNNING);
    if (c_start.m_nodes.isWaitingFor(connectedNodeId))
    {
      jam();
      c_start.m_nodes.clearWaitingFor(connectedNodeId);
      c_start.m_gsn = RNIL;
      
      NodeRecPtr addNodePtr;
      addNodePtr.i = connectedNodeId;
      ptrCheckGuard(addNodePtr, MAX_NDB_NODES, nodeRec);
      cmAddPrepare(signal, addNodePtr, myNodePtr.p);
      return;
    }
  }
  default:
    (void)1;
  }
  
  ndbrequire(!c_start.m_nodes.isWaitingFor(connectedNodeId));
  ndbrequire(!c_readnodes_nodes.get(connectedNodeId));
  c_readnodes_nodes.set(connectedNodeId);
  signal->theData[0] = reference();
  sendSignal(calcQmgrBlockRef(connectedNodeId), GSN_READ_NODESREQ, signal, 1, JBA);
  return;
}//Qmgr::execCONNECT_REP()

void
Qmgr::execREAD_NODESCONF(Signal* signal)
{
  jamEntry();
  check_readnodes_reply(signal, 
			refToNode(signal->getSendersBlockRef()),
			GSN_READ_NODESCONF);
}

void
Qmgr::execREAD_NODESREF(Signal* signal)
{
  jamEntry();
  check_readnodes_reply(signal, 
			refToNode(signal->getSendersBlockRef()),
			GSN_READ_NODESREF);
}

/**
 * Heartbeat Inclusion Protocol Handling
 * -------------------------------------
 * The protocol to include our node in the heartbeat protocol starts when
 * we call execCM_INFOCONF. We start by opening communication to all nodes
 * in the cluster. When we start this protocol we don't know anything about
 * which nodes are up and running and we don't which node is currently the
 * president of the heartbeat protocol.
 *
 * For us to be successful with being included in the heartbeat protocol we
 * need to be connected to all nodes currently in the heartbeat protocol. It
 * is important to remember that QMGR sees a node as alive if it is included
 * in the heartbeat protocol. Higher level notions of aliveness is handled
 * primarily by the DBDIH block, but also to some extent by NDBCNTR.
 * 
 * The protocol starts by the new node sending CM_REGREQ to all nodes it is
 * connected to. Only the president will respond to this message. We could
 * have a situation where there currently isn't a president choosen. In this
 * case an election is held whereby a new president is assigned. In the rest
 * of this comment we assume that a president already exists.
 *
 * So if we were connected to the president we will get a response to the
 * CM_REGREQ from the president with CM_REGCONF. The CM_REGCONF contains
 * the set of nodes currently included in the heartbeat protocol.
 *
 * The president will send in parallel to sending CM_REGCONF a CM_ADD(prepare)
 * message to all nodes included in the protocol.
 *
 * When receiving CM_REGCONF the new node will send CM_NODEINFOREQ with
 * information about version of the binary, number of LDM workers and
 * MySQL version of binary.
 *
 * The nodes already included in the heartbeat protocol will wait until it
 * receives both the CM_ADD(prepare) from the president and the
 * CM_NODEINFOREQ from the starting node. When it receives those two
 * messages it will send CM_ACKADD(prepare) to the president and
 * CM_NODEINFOCONF to the starting node with its own node information.
 *
 * When the president received CM_ACKADD(prepare) from all nodes included
 * in the heartbeat protocol then it sends CM_ADD(AddCommit) to all nodes
 * included in the heartbeat protocol.
 * 
 * When the nodes receives CM_ADD(AddCommit) from the president then
 * they will enable communication to the new node and immediately start
 * sending heartbeats to the new node. They will also include the new
 * node in their view of the nodes included in the heartbeat protocol.
 * Next they will send CM_ACKADD(AddCommit) back to the president.
 *
 * When the president has received CM_ACKADD(AddCommit) from all nodes
 * included in the heartbeat protocol then it sends CM_ADD(CommitNew)
 * to the starting node.
 *
 * This is also the point where we report the node as included in the
 * heartbeat protocol to DBDIH as from here the rest of the protocol is
 * only about informing the new node about the outcome of inclusion
 * protocol. When we receive the response to this message the new node
 * can already have proceeded a bit into its restart.
 *
 * The starting node after receiving CM_REGCONF waits for all nodes
 * included in the heartbeat protocol to send CM_NODEINFOCONF and
 * also for receiving the CM_ADD(CommitNew) from the president. When
 * all this have been received the new nodes adds itself and all nodes
 * it have been informed about into its view of the nodes included in
 * the heartbeat protocol and enables communication to all other
 * nodes included therein. Finally it sends CM_ACKADD(CommitNew) to
 * the president.
 *
 * When the president has received CM_ACKADD(CommitNew) from the starting
 * node the inclusion protocol is completed and the president is ready
 * to receive a new node into the cluster.
 *
 * It is the responsibility of the starting nodes to retry after a failed
 * node inclusion, they will do so with 3 seconds delay. This means that
 * at most one node per 3 seconds will normally be added to the cluster.
 * So this phase of adding nodes to the cluster can add up to a little bit
 * more than a minute of delay in a large cluster starting up.
 *
 * We try to depict the above in a graph here as well:
 *
 * New node           Nodes included in the heartbeat protocol        President
 * ----------------------------------------------------------------------------
 * ----CM_REGREQ--------------------->>
 * ----CM_REGREQ---------------------------------------------------------->
 *
 * <----------------CM_REGCONF---------------------------------------------
 *                                   <<------CM_ADD(Prepare)---------------
 *
 * -----CM_NODEINFOREQ--------------->>
 *
 * Nodes included in heartbeat protocol can receive CM_ADD(Prepare) and
 * CM_NODEINFOREQ in any order.
 *
 * <<---CM_NODEINFOCONF-------------- --------CM_ACKADD(Prepare)--------->>
 *
 *                                   <<-------CM_ADD(AddCommit)------------
 *
 * Here nodes enables communication to new node and starts sending heartbeats
 *
 *                                   ---------CM_ACKADD(AddCommit)------->>
 *
 * Here we report to DBDIH about new node included in heartbeat protocol
 * in master node.
 *
 * <----CM_ADD(CommitNew)--------------------------------------------------
 *
 * Here new node enables communication to new nodes and starts sending
 * heartbeat messages.
 *
 * -----CM_ACKADD(CommitNew)---------------------------------------------->
 *
 * Here the president can complete the inclusion protocol and is ready to
 * receive new nodes into the heartbeat protocol.
 */
/*******************************/
/* CM_INFOCONF                */
/*******************************/
void Qmgr::execCM_INFOCONF(Signal* signal) 
{
  /**
   * Open communcation to all DB nodes
   */
  signal->theData[0] = 0; // no answer
  signal->theData[1] = 0; // no id
  signal->theData[2] = NodeInfo::DB;
  sendSignal(TRPMAN_REF, GSN_OPEN_COMORD, signal, 3, JBB);

  cpresident = ZNIL;
  cpresidentAlive = ZFALSE;
  c_start_election_time = NdbTick_getCurrentTicks();
  
  signal->theData[0] = ZSTART_FAILURE_LIMIT;
  sendSignalWithDelay(reference(), GSN_CONTINUEB, signal, 3000, 1);
  
  cmInfoconf010Lab(signal);
  
  return;
}//Qmgr::execCM_INFOCONF()

Uint32 g_start_type = 0;
NdbNodeBitmask g_nowait_nodes; // Set by clo

void Qmgr::cmInfoconf010Lab(Signal* signal) 
{
  c_start.m_startKey = 0;
  c_start.m_startNode = getOwnNodeId();
  c_start.m_nodes.clearWaitingFor();
  c_start.m_gsn = GSN_CM_REGREQ;
  c_start.m_starting_nodes.clear();
  c_start.m_starting_nodes_w_log.clear();
  c_start.m_regReqReqSent = 0;
  c_start.m_regReqReqRecv = 0;
  c_start.m_skip_nodes = g_nowait_nodes;
  c_start.m_skip_nodes.bitAND(c_definedNodes);
  c_start.m_start_type = g_start_type;

  NodeRecPtr nodePtr;
  cnoOfNodes = 0;
  for (nodePtr.i = 1; nodePtr.i < MAX_NDB_NODES; nodePtr.i++) {
    jam();
    ptrAss(nodePtr, nodeRec);
    
    if(getNodeInfo(nodePtr.i).getType() != NodeInfo::DB)
      continue;

    c_start.m_nodes.setWaitingFor(nodePtr.i);    
    cnoOfNodes++;

    if(!c_connectedNodes.get(nodePtr.i))
      continue;
    
    sendCmRegReq(signal, nodePtr.i);
  }
  
  //----------------------------------------
  /* Wait for a while. When it returns    */
  /* we will check if we got any CM_REGREF*/
  /* or CM_REGREQ (lower nodeid than our  */
  /* own).                                */
  //----------------------------------------
  signal->theData[0] = ZREGREQ_TIMELIMIT;
  signal->theData[1] = c_start.m_startKey;
  signal->theData[2] = c_start.m_startNode;
  sendSignalWithDelay(QMGR_REF, GSN_CONTINUEB, signal, 3000, 3);

  creadyDistCom = ZTRUE;
  return;
}//Qmgr::cmInfoconf010Lab()

void
Qmgr::sendCmRegReq(Signal * signal, Uint32 nodeId){
  CmRegReq * req = (CmRegReq *)&signal->theData[0];
  req->blockRef = reference();
  req->nodeId = getOwnNodeId();
  req->version = NDB_VERSION;
  req->mysql_version = NDB_MYSQL_VERSION_D;
  req->latest_gci = c_start.m_latest_gci;
  req->start_type = c_start.m_start_type;
  c_start.m_skip_nodes.copyto(NdbNodeBitmask::Size, req->skip_nodes);
  const Uint32 ref = calcQmgrBlockRef(nodeId);
  sendSignal(ref, GSN_CM_REGREQ, signal, CmRegReq::SignalLength, JBB);
  DEBUG_START(GSN_CM_REGREQ, nodeId, "");
  
  c_start.m_regReqReqSent++;
}

/*
4.4.11 CM_REGREQ */
/**--------------------------------------------------------------------------
 * If this signal is received someone tries to get registrated. 
 * Only the president have the authority make decisions about new nodes, 
 * so only a president or a node that claims to be the president may send a 
 * reply to this signal. 
 * This signal can occur any time after that STTOR was received. 
 * CPRESIDENT:             Timelimit has expired and someone has 
 *                         decided to enter the president role
 * CPRESIDENT_CANDIDATE:
 *     Assigned when we receive a CM_REGREF, if we got more than one REF 
 *     then we always keep the lowest nodenumber. 
 *     We accept this nodeno as president when our timelimit expires
 * We should consider the following cases: 
 * 1- We are the president. If we are busy by adding new nodes to cluster, 
 *    then we have to refuse this node to be added. 
 *    The refused node will try in ZREFUSE_ADD_TIME seconds again. 
 *    If we are not busy then we confirm
 *
 * 2- We know the president, we dont bother us about this REQ. 
 *    The president has also got this REQ and will take care of it.
 *
 * 3- The president are not known. We have received CM_INIT, so we compare the
 *    senders node number to GETOWNNODEID().
 *    If we have a lower number than the sender then we will claim 
 *    that we are the president so we send him a refuse signal back. 
 *    We have to wait for the CONTINUEB signal before we can enter the 
 *    president role. If our GETOWNNODEID() if larger than sender node number, 
 *    we are not the president and just have to wait for the 
 *    reply signal (REF)  to our CM_REGREQ_2. 
 * 4- We havent received the CM_INIT signal so we don't know who we are. 
 *    Ignore the request.
 *--------------------------------------------------------------------------*/
/*******************************/
/* CM_REGREQ                  */
/*******************************/
static
int
check_start_type(Uint32 starting, Uint32 own)
{
  if (starting == (1 << NodeState::ST_INITIAL_START) &&
      ((own & (1 << NodeState::ST_INITIAL_START)) == 0))
  {
    return 1;
  }
  return 0;
}

void Qmgr::execCM_REGREQ(Signal* signal) 
{
  DEBUG_START3(signal, "");

  NodeRecPtr addNodePtr;
  jamEntry();

  CmRegReq * const cmRegReq = (CmRegReq *)&signal->theData[0];
  const BlockReference Tblockref = cmRegReq->blockRef;
  const Uint32 startingVersion = cmRegReq->version;
  Uint32 startingMysqlVersion = cmRegReq->mysql_version;
  addNodePtr.i = cmRegReq->nodeId;
  Uint32 gci = 1;
  Uint32 start_type = ~0;
  NdbNodeBitmask skip_nodes;

  if (!c_connectedNodes.get(cmRegReq->nodeId))
  {
    jam();

    /**
     * With ndbmtd, there is a race condition such that
     *   CM_REGREQ can arrive prior to CONNECT_REP
     *   since CONNECT_REP is sent from CMVMI
     *
     * In such cases, ignore the CM_REGREQ which is safe
     *   as it will anyway be resent by starting node
     */
    g_eventLogger->info("discarding CM_REGREQ from %u "
                        "as we're not yet connected (isNdbMt: %u)",
                        cmRegReq->nodeId,
                        (unsigned)isNdbMt());

    return;
  }

  if (signal->getLength() == CmRegReq::SignalLength)
  {
    jam();
    gci = cmRegReq->latest_gci;
    start_type = cmRegReq->start_type;
    skip_nodes.assign(NdbNodeBitmask::Size, cmRegReq->skip_nodes);
  }
  
  if (startingVersion < NDBD_SPLIT_VERSION)
  {
    startingMysqlVersion = 0;
  }
  
  if (creadyDistCom == ZFALSE) {
    jam();
    /* NOT READY FOR DISTRIBUTED COMMUNICATION.*/
    return;	     	      	             	      	             
  }//if
  
  if (!ndbCompatible_ndb_ndb(NDB_VERSION, startingVersion)) {
    jam();
    sendCmRegrefLab(signal, Tblockref, CmRegRef::ZINCOMPATIBLE_VERSION);
    return;
  }

  if (!ndb_check_micro_gcp(startingVersion))
  {
    jam();
    infoEvent("Connection from node %u refused as it's not micro GCP enabled",
              addNodePtr.i);
    sendCmRegrefLab(signal, Tblockref, CmRegRef::ZINCOMPATIBLE_VERSION);
    return;
  }

  if (!ndb_pnr(startingVersion))
  {
    jam();
    infoEvent("Connection from node %u refused as it's not does not support "
              "parallel node recovery",
              addNodePtr.i);
    sendCmRegrefLab(signal, Tblockref, CmRegRef::ZINCOMPATIBLE_VERSION);
    return;
  }

  if (!ndb_check_hb_order_version(startingVersion) &&
      m_hb_order_config_used)
  {
    jam();
    infoEvent("Connection from node %u refused as it does not support "
              "user-defined HeartbeatOrder",
              addNodePtr.i);
    sendCmRegrefLab(signal, Tblockref, CmRegRef::ZINCOMPATIBLE_VERSION);
    return;
  }

  if (m_connectivity_check.m_enabled &&
      !ndbd_connectivity_check(startingVersion))
  {
    jam();
    infoEvent("Connection from node %u refused as it does not support "
              "ConnectCheckIntervalDelay",
              addNodePtr.i);
    sendCmRegrefLab(signal, Tblockref, CmRegRef::ZINCOMPATIBLE_VERSION);
    return;
  }

  if (check_start_type(start_type, c_start.m_start_type))
  {
    jam();
    sendCmRegrefLab(signal, Tblockref, CmRegRef::ZINCOMPATIBLE_START_TYPE);
    return;
  }
  
  if (cpresident != getOwnNodeId())
  {
    jam();                      

    if (cpresident == ZNIL) 
    {
      /***
       * We don't know the president. 
       * If the node to be added has lower node id 
       * than it will be our president candidate. Set it as 
       * candidate.
       */
      jam(); 
      if (gci != ZUNDEFINED_GCI_LIMIT &&
          (gci > c_start.m_president_candidate_gci || 
	  (gci == c_start.m_president_candidate_gci && 
	   addNodePtr.i < c_start.m_president_candidate)))
      {
	jam();
	c_start.m_president_candidate = addNodePtr.i;
	c_start.m_president_candidate_gci = gci;
      }
      sendCmRegrefLab(signal, Tblockref, CmRegRef::ZELECTION);
      return;
    }                          
    
    /**
     * We are not the president.
     * We know the president.
     * President will answer.
     */
    sendCmRegrefLab(signal, Tblockref, CmRegRef::ZNOT_PRESIDENT);
    return;
  }//if

  if (c_start.m_startNode != 0)
  {
    jam();
    /**
     * President busy by adding another node
    */
    sendCmRegrefLab(signal, Tblockref, CmRegRef::ZBUSY_PRESIDENT);
    return;
  }//if
  
  if (ctoStatus == Q_ACTIVE) 
  {   
    jam();
    /**
     * Active taking over as president
     */
    sendCmRegrefLab(signal, Tblockref, CmRegRef::ZBUSY_TO_PRES);
    return;
  }//if

  if (getNodeInfo(addNodePtr.i).m_type != NodeInfo::DB) 
  {
    jam(); 
    /** 
     * The new node is not in config file
     */
    sendCmRegrefLab(signal, Tblockref, CmRegRef::ZNOT_IN_CFG);
    return;
  } 

  if (getNodeState().getSingleUserMode()) 
  {
    /** 
     * The cluster is in single user mode.
     * Data node is not allowed to get added in the cluster 
     * while in single user mode.
     */
    // handle rolling upgrade
    {
      unsigned int get_major = getMajor(startingVersion);
      unsigned int get_minor = getMinor(startingVersion);
      unsigned int get_build = getBuild(startingVersion);

      if (startingVersion < NDBD_QMGR_SINGLEUSER_VERSION_5) {
        jam();

        infoEvent("QMGR: detect upgrade: new node %u old version %u.%u.%u",
          (unsigned int)addNodePtr.i, get_major, get_minor, get_build);
        /** 
         * The new node is old version, send ZINCOMPATIBLE_VERSION instead
         * of ZSINGLE_USER_MODE.
         */
        sendCmRegrefLab(signal, Tblockref, CmRegRef::ZINCOMPATIBLE_VERSION);
      } else {
        jam();

        sendCmRegrefLab(signal, Tblockref, CmRegRef::ZSINGLE_USER_MODE);
      }//if
    }

    return;
  }//if

  ptrCheckGuard(addNodePtr, MAX_NDB_NODES, nodeRec);
  Phase phase = addNodePtr.p->phase;
  if (phase != ZINIT)
  {
    jam();
    DEBUG("phase = " << phase);
    sendCmRegrefLab(signal, Tblockref, CmRegRef::ZNOT_DEAD);
    return;
  }
  
  jam(); 
  /**
   * WE ARE PRESIDENT AND WE ARE NOT BUSY ADDING ANOTHER NODE. 
   * WE WILL TAKE CARE OF THE INCLUSION OF THIS NODE INTO THE CLUSTER.
   * WE NEED TO START TIME SUPERVISION OF THIS. SINCE WE CANNOT STOP 
   * TIMED SIGNAL IF THE INCLUSION IS INTERRUPTED WE IDENTIFY 
   * EACH INCLUSION WITH A UNIQUE IDENTITY. THIS IS CHECKED WHEN 
   * THE SIGNAL ARRIVES. IF IT HAS CHANGED THEN WE SIMPLY IGNORE 
   * THE TIMED SIGNAL. 
   */

  /**
   * Update start record
   */
  c_start.m_startKey++;
  c_start.m_startNode = addNodePtr.i;
  
  /**
   * Assign dynamic id
   */
  UintR TdynId = (++c_maxDynamicId) & 0xFFFF;
  TdynId |= (addNodePtr.p->hbOrder << 16);
  setNodeInfo(addNodePtr.i).m_version = startingVersion;
  setNodeInfo(addNodePtr.i).m_mysql_version = startingMysqlVersion;
  recompute_version_info(NodeInfo::DB, startingVersion);
  addNodePtr.p->ndynamicId = TdynId;
  
  /**
   * Reply with CM_REGCONF
   */
  CmRegConf * const cmRegConf = (CmRegConf *)&signal->theData[0];
  cmRegConf->presidentBlockRef = reference();
  cmRegConf->presidentNodeId   = getOwnNodeId();
  cmRegConf->presidentVersion  = getNodeInfo(getOwnNodeId()).m_version;
  cmRegConf->presidentMysqlVersion = getNodeInfo(getOwnNodeId()).m_mysql_version;
  cmRegConf->dynamicId         = TdynId;
  c_clusterNodes.copyto(NdbNodeBitmask::Size, cmRegConf->allNdbNodes);
  sendSignal(Tblockref, GSN_CM_REGCONF, signal, 
	     CmRegConf::SignalLength, JBA);
  DEBUG_START(GSN_CM_REGCONF, refToNode(Tblockref), "");

  /**
   * Send CmAdd to all nodes (including starting)
   */
  c_start.m_nodes = c_clusterNodes;
  c_start.m_nodes.setWaitingFor(addNodePtr.i);
  c_start.m_gsn = GSN_CM_ADD;

  NodeReceiverGroup rg(QMGR, c_start.m_nodes);
  CmAdd * const cmAdd = (CmAdd*)signal->getDataPtrSend();
  cmAdd->requestType = CmAdd::Prepare;
  cmAdd->startingNodeId = addNodePtr.i; 
  cmAdd->startingVersion = startingVersion;
  cmAdd->startingMysqlVersion = startingMysqlVersion;
  sendSignal(rg, GSN_CM_ADD, signal, CmAdd::SignalLength, JBA);
  DEBUG_START2(GSN_CM_ADD, rg, "Prepare");
  
  /**
   * Set timer
   */
  return;
  signal->theData[0] = ZREGREQ_MASTER_TIMELIMIT;
  signal->theData[1] = c_start.m_startKey;
  sendSignalWithDelay(QMGR_REF, GSN_CONTINUEB, signal, 30000, 2);

  return;
}//Qmgr::execCM_REGREQ()

void Qmgr::sendCmRegrefLab(Signal* signal, BlockReference TBRef, 
			   CmRegRef::ErrorCode Terror) 
{
  CmRegRef* ref = (CmRegRef*)signal->getDataPtrSend();
  ref->blockRef = reference();
  ref->nodeId = getOwnNodeId();
  ref->errorCode = Terror;
  ref->presidentCandidate = 
    (cpresident == ZNIL ? c_start.m_president_candidate : cpresident);
  ref->candidate_latest_gci = c_start.m_president_candidate_gci;
  ref->latest_gci = c_start.m_latest_gci;
  ref->start_type = c_start.m_start_type;
  c_start.m_skip_nodes.copyto(NdbNodeBitmask::Size, ref->skip_nodes);
  sendSignal(TBRef, GSN_CM_REGREF, signal, 
	     CmRegRef::SignalLength, JBB);
  DEBUG_START(GSN_CM_REGREF, refToNode(TBRef), "");
  return;
}//Qmgr::sendCmRegrefLab()

/*
4.4.11 CM_REGCONF */
/**--------------------------------------------------------------------------
 * President gives permission to a node which wants to join the cluster. 
 * The president will prepare the cluster that a new node will be added to 
 * cluster. When the new node has set up all connections to the cluster, 
 * the president will send commit to all clusternodes so the phase of the 
 * new node can be changed to ZRUNNING. 
 *--------------------------------------------------------------------------*/
/*******************************/
/* CM_REGCONF                 */
/*******************************/
void Qmgr::execCM_REGCONF(Signal* signal) 
{
  DEBUG_START3(signal, "");

  NodeRecPtr myNodePtr;
  NodeRecPtr nodePtr;
  jamEntry();

  const CmRegConf * const cmRegConf = (CmRegConf *)&signal->theData[0];

  if (!ndbCompatible_ndb_ndb(NDB_VERSION, cmRegConf->presidentVersion)) {
    jam();
    char buf[128];
    BaseString::snprintf(buf,sizeof(buf), 
			 "incompatible version own=0x%x other=0x%x, "
			 " shutting down", 
			 NDB_VERSION, cmRegConf->presidentVersion);
    progError(__LINE__, NDBD_EXIT_UNSUPPORTED_VERSION, buf);  
    return;
  }

  if (!ndb_check_hb_order_version(cmRegConf->presidentVersion) &&
      m_hb_order_config_used) {
    jam();
    char buf[128];
    BaseString::snprintf(buf,sizeof(buf), 
			 "incompatible version own=0x%x other=0x%x, "
			 "due to user-defined HeartbeatOrder, shutting down", 
			 NDB_VERSION, cmRegConf->presidentVersion);
    progError(__LINE__, NDBD_EXIT_UNSUPPORTED_VERSION, buf);  
    return;
  }

  if (m_connectivity_check.m_enabled &&
      !ndbd_connectivity_check(cmRegConf->presidentVersion))
  {
    jam();
    m_connectivity_check.m_enabled = false;
    ndbout_c("Disabling ConnectCheckIntervalDelay as president "
             " does not support it");
    infoEvent("Disabling ConnectCheckIntervalDelay as president "
              " does not support it");
  }

  myNodePtr.i = getOwnNodeId();
  ptrCheckGuard(myNodePtr, MAX_NDB_NODES, nodeRec);
  
  ndbrequire(c_start.m_gsn == GSN_CM_REGREQ);
  ndbrequire(myNodePtr.p->phase == ZSTARTING);
  
  cpdistref    = cmRegConf->presidentBlockRef;
  cpresident   = cmRegConf->presidentNodeId;
  UintR TdynamicId   = cmRegConf->dynamicId;
  c_maxDynamicId = TdynamicId & 0xFFFF;
  c_clusterNodes.assign(NdbNodeBitmask::Size, cmRegConf->allNdbNodes);

  myNodePtr.p->ndynamicId = TdynamicId;

  // set own MT config here or in REF, and others in CM_NODEINFOREQ/CONF
  setNodeInfo(getOwnNodeId()).m_lqh_workers = globalData.ndbMtLqhWorkers;
  
/*--------------------------------------------------------------*/
// Send this as an EVENT REPORT to inform about hearing about
// other NDB node proclaiming to be president.
/*--------------------------------------------------------------*/
  signal->theData[0] = NDB_LE_CM_REGCONF;
  signal->theData[1] = getOwnNodeId();
  signal->theData[2] = cpresident;
  signal->theData[3] = TdynamicId;
  sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 4, JBB);

  for (nodePtr.i = 1; nodePtr.i < MAX_NDB_NODES; nodePtr.i++) {
    if (c_clusterNodes.get(nodePtr.i)){
      jamLine(nodePtr.i);
      ptrAss(nodePtr, nodeRec);

      ndbrequire(nodePtr.p->phase == ZINIT);
      nodePtr.p->phase = ZRUNNING;

      if(c_connectedNodes.get(nodePtr.i)){
	jam();
	sendCmNodeInfoReq(signal, nodePtr.i, myNodePtr.p);
      }
    }
  }

  c_start.m_gsn = GSN_CM_NODEINFOREQ;
  c_start.m_nodes = c_clusterNodes;

  if (ERROR_INSERTED(937))
  {
    CLEAR_ERROR_INSERT_VALUE;
    signal->theData[0] = 9999;
    sendSignalWithDelay(CMVMI_REF, GSN_NDB_TAMPER, signal, 500, 1);
  }

  return;
}//Qmgr::execCM_REGCONF()

void
Qmgr::check_readnodes_reply(Signal* signal, Uint32 nodeId, Uint32 gsn)
{
  NodeRecPtr myNodePtr;
  myNodePtr.i = getOwnNodeId();
  ptrCheckGuard(myNodePtr, MAX_NDB_NODES, nodeRec);
  
  NodeRecPtr nodePtr;
  nodePtr.i = nodeId;
  ptrCheckGuard(nodePtr, MAX_NDB_NODES, nodeRec);

  ndbrequire(c_readnodes_nodes.get(nodeId));
  ReadNodesConf* conf = (ReadNodesConf*)signal->getDataPtr();
  if (gsn == GSN_READ_NODESREF)
  {
    jam();
retry:
    signal->theData[0] = reference();
    sendSignal(calcQmgrBlockRef(nodeId), GSN_READ_NODESREQ, signal, 1, JBA);
    return;
  }
  
  if (conf->masterNodeId == ZNIL)
  {
    jam();
    goto retry;
  }
  
  Uint32 president = conf->masterNodeId;
  if (president == cpresident)
  {
    jam();
    c_readnodes_nodes.clear(nodeId);
    return;
  }

  char buf[255];
  BaseString::snprintf(buf, sizeof(buf),
		       "check StartPartialTimeout, "
		       "node %d thinks %d is president, "
		       "I think president is: %d",
		       nodeId, president, cpresident);

  ndbout_c("%s", buf);
  CRASH_INSERTION(933);

  if (getNodeState().startLevel == NodeState::SL_STARTED)
  {
    jam();
    NdbNodeBitmask part;
    part.assign(NdbNodeBitmask::Size, conf->clusterNodes);
    FailRep* rep = (FailRep*)signal->getDataPtrSend();
    rep->failCause = FailRep::ZPARTITIONED_CLUSTER;
    rep->partitioned.president = cpresident;
    c_clusterNodes.copyto(NdbNodeBitmask::Size, rep->partitioned.partition);
    rep->partitioned.partitionFailSourceNodeId = getOwnNodeId();
    Uint32 ref = calcQmgrBlockRef(nodeId);
    Uint32 i = 0;
    /* Send source of event info if a node supports it */
    Uint32 length = FailRep::OrigSignalLength + FailRep::PartitionedExtraLength;    
    while((i = part.find(i + 1)) != NdbNodeBitmask::NotFound)
    {
      if (i == nodeId)
	continue;
      rep->failNodeId = i;
      bool sendSourceId = ndbd_fail_rep_source_node((getNodeInfo(i)).m_version);
      sendSignal(ref, GSN_FAIL_REP, signal, 
                 length + (sendSourceId ? FailRep::SourceExtraLength : 0), 
                 JBA);
    }
    rep->failNodeId = nodeId;
    bool sendSourceId = ndbd_fail_rep_source_node((getNodeInfo(nodeId)).m_version);
    
    sendSignal(ref, GSN_FAIL_REP, signal,
               length + (sendSourceId ? FailRep::SourceExtraLength : 0), 
               JBB);
    return;
  }
  
  CRASH_INSERTION(932);
  CRASH_INSERTION(938);

  progError(__LINE__, 
	    NDBD_EXIT_PARTITIONED_SHUTDOWN,
	    buf);
  
  ndbabort();
}

void
Qmgr::sendCmNodeInfoReq(Signal* signal, Uint32 nodeId, const NodeRec * self){
  CmNodeInfoReq * const req = (CmNodeInfoReq*)signal->getDataPtrSend();
  req->nodeId = getOwnNodeId();
  req->dynamicId = self->ndynamicId;
  req->version = getNodeInfo(getOwnNodeId()).m_version;
  req->mysql_version = getNodeInfo(getOwnNodeId()).m_mysql_version;
  req->lqh_workers = getNodeInfo(getOwnNodeId()).m_lqh_workers;
  const Uint32 ref = calcQmgrBlockRef(nodeId);
  sendSignal(ref,GSN_CM_NODEINFOREQ, signal, CmNodeInfoReq::SignalLength, JBB);
  DEBUG_START(GSN_CM_NODEINFOREQ, nodeId, "");
}

/*
4.4.11 CM_REGREF */
/**--------------------------------------------------------------------------
 * Only a president or a president candidate can refuse a node to get added to
 * the cluster.
 * Refuse reasons: 
 * ZBUSY         We know that the sender is the president and we have to 
 *               make a new CM_REGREQ.
 * ZNOT_IN_CFG   This node number is not specified in the configfile, 
 *               SYSTEM ERROR
 * ZELECTION     Sender is a president candidate, his timelimit 
 *               hasn't expired so maybe someone else will show up. 
 *               Update the CPRESIDENT_CANDIDATE, then wait for our 
 *               timelimit to expire. 
 *---------------------------------------------------------------------------*/
/*******************************/
/* CM_REGREF                  */
/*******************************/
static
const char *
get_start_type_string(Uint32 st)
{
  static char buf[256];
  
  if (st == 0)
  {
    return "<ANY>";
  }
  else
  {
    buf[0] = 0;
    for(Uint32 i = 0; i<NodeState::ST_ILLEGAL_TYPE; i++)
    {
      if (st & (1 << i))
      {
	if (buf[0])
	  strcat(buf, "/");
	switch(i){
	case NodeState::ST_INITIAL_START:
	  strcat(buf, "inital start");
	  break;
	case NodeState::ST_SYSTEM_RESTART:
	  strcat(buf, "system restart");
	  break;
	case NodeState::ST_NODE_RESTART:
	  strcat(buf, "node restart");
	  break;
	case NodeState::ST_INITIAL_NODE_RESTART:
	  strcat(buf, "initial node restart");
	  break;
	}
      }
    }
    return buf;
  }
}

void Qmgr::execCM_REGREF(Signal* signal) 
{
  jamEntry();

  CmRegRef* ref = (CmRegRef*)signal->getDataPtr();
  UintR TaddNodeno = ref->nodeId;
  UintR TrefuseReason = ref->errorCode;
  Uint32 candidate = ref->presidentCandidate;
  Uint32 node_gci = 1;
  Uint32 candidate_gci = 1;
  Uint32 start_type = ~0;
  NdbNodeBitmask skip_nodes;
  DEBUG_START3(signal, TrefuseReason);

  if (signal->getLength() == CmRegRef::SignalLength)
  {
    jam();
    node_gci = ref->latest_gci;
    candidate_gci = ref->candidate_latest_gci;
    start_type = ref->start_type;
    skip_nodes.assign(NdbNodeBitmask::Size, ref->skip_nodes);
  }
  
  c_start.m_regReqReqRecv++;

  // Ignore block reference in data[0]
  
  if(candidate != c_start.m_president_candidate)
  {
    jam();
    c_start.m_regReqReqRecv = ~0;
  }
  
  c_start.m_starting_nodes.set(TaddNodeno);
  if (node_gci > ZUNDEFINED_GCI_LIMIT)
  {
    jam();
    c_start.m_starting_nodes_w_log.set(TaddNodeno);
  }
  c_start.m_node_gci[TaddNodeno] = node_gci;

  skip_nodes.bitAND(c_definedNodes);
  c_start.m_skip_nodes.bitOR(skip_nodes);

  // set own MT config here or in CONF, and others in CM_NODEINFOREQ/CONF
  setNodeInfo(getOwnNodeId()).m_lqh_workers = globalData.ndbMtLqhWorkers;
  
  char buf[100];
  switch (TrefuseReason) {
  case CmRegRef::ZINCOMPATIBLE_VERSION:
    jam();
    progError(__LINE__, NDBD_EXIT_UNSUPPORTED_VERSION, 
              "incompatible version, "
              "connection refused by running ndb node");
  case CmRegRef::ZINCOMPATIBLE_START_TYPE:
    jam();
    BaseString::snprintf(buf, sizeof(buf),
			 "incompatible start type detected: node %d"
			 " reports %s(%d) my start type: %s(%d)",
			 TaddNodeno,
			 get_start_type_string(start_type), start_type,
			 get_start_type_string(c_start.m_start_type),
			 c_start.m_start_type);
    progError(__LINE__, NDBD_EXIT_SR_RESTARTCONFLICT, buf);
    break;
  case CmRegRef::ZBUSY:
  case CmRegRef::ZBUSY_TO_PRES:
  case CmRegRef::ZBUSY_PRESIDENT:
    jam();
    cpresidentAlive = ZTRUE;
    signal->theData[3] = 0;
    break;
  case CmRegRef::ZNOT_IN_CFG:
    jam();
    progError(__LINE__, NDBD_EXIT_NODE_NOT_IN_CONFIG);
    break;
  case CmRegRef::ZNOT_DEAD:
    jam();
    progError(__LINE__, NDBD_EXIT_NODE_NOT_DEAD);
    break;
  case CmRegRef::ZSINGLE_USER_MODE:
    jam();
    progError(__LINE__, NDBD_EXIT_SINGLE_USER_MODE);
    break;
  /**
   * For generic refuse error.
   * e.g. in online upgrade, we can use this error code instead
   * of the incompatible error code.
   */
  case CmRegRef::ZGENERIC:
    jam();
    progError(__LINE__, NDBD_EXIT_GENERIC);
    break;
  case CmRegRef::ZELECTION:
    jam();
    if (candidate_gci != ZUNDEFINED_GCI_LIMIT &&
        (candidate_gci > c_start.m_president_candidate_gci ||
	 (candidate_gci == c_start.m_president_candidate_gci &&
	 candidate < c_start.m_president_candidate)))
    {
      jam();
      //----------------------------------------
      /* We may already have a candidate      */
      /* choose the lowest nodeno             */
      //----------------------------------------
      signal->theData[3] = 2;
      c_start.m_president_candidate = candidate;
      c_start.m_president_candidate_gci = candidate_gci;
    } else {
      signal->theData[3] = 4;
    }//if
    break;
  case CmRegRef::ZNOT_PRESIDENT:
    jam();
    cpresidentAlive = ZTRUE;
    signal->theData[3] = 3;
    break;
  default:
    jam();
    signal->theData[3] = 5;
    /*empty*/;
    break;
  }//switch
/*--------------------------------------------------------------*/
// Send this as an EVENT REPORT to inform about hearing about
// other NDB node proclaiming not to be president.
/*--------------------------------------------------------------*/
  signal->theData[0] = NDB_LE_CM_REGREF;
  signal->theData[1] = getOwnNodeId();
  signal->theData[2] = TaddNodeno;
//-----------------------------------------
// signal->theData[3] filled in above
//-----------------------------------------
  sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 4, JBB);

  if(cpresidentAlive == ZTRUE)
  {
    jam();
    DEBUG("cpresidentAlive");
    return;
  }
  
  if(c_start.m_regReqReqSent != c_start.m_regReqReqRecv)
  {
    jam();
    DEBUG(c_start.m_regReqReqSent << " != " << c_start.m_regReqReqRecv);
    return;
  }
  
  if(c_start.m_president_candidate != getOwnNodeId())
  {
    jam();
    DEBUG("i'm not the candidate");
    return;
  }
  
  /**
   * All connected nodes has agreed
   */
  if(check_startup(signal))
  {
    jam();
    electionWon(signal);
  }
  
  return;
}//Qmgr::execCM_REGREF()

Uint32
Qmgr::check_startup(Signal* signal)
{
  const NDB_TICKS now  = NdbTick_getCurrentTicks();
  const Uint64 elapsed = NdbTick_Elapsed(c_start_election_time,now).milliSec();
  const Uint64 partitionedTimeout =
    c_restartPartitionedTimeout == Uint32(~0) ? Uint32(~0) :
     (c_restartPartialTimeout + c_restartPartitionedTimeout);

  const bool no_nodegroup_active =
    (c_restartNoNodegroupTimeout != ~Uint32(0)) &&
    (! c_start.m_no_nodegroup_nodes.isclear());

  /**
   * First see if we should wait more...
   */
  NdbNodeBitmask tmp;
  tmp.bitOR(c_start.m_skip_nodes);
  tmp.bitOR(c_start.m_starting_nodes);

  NdbNodeBitmask wait;
  wait.assign(c_definedNodes);
  wait.bitANDC(tmp);

  Uint32 retVal = 0;
  Uint32 incompleteng = MAX_NDB_NODES; // Illegal value
  NdbNodeBitmask report_mask;

  if ((c_start.m_latest_gci == 0) || 
      (c_start.m_start_type == (1 << NodeState::ST_INITIAL_START)))
  {
    if (tmp.equal(c_definedNodes))
    {
      jam();
      signal->theData[1] = 0x8000;
      report_mask.assign(c_definedNodes);
      report_mask.bitANDC(c_start.m_starting_nodes);
      retVal = 1;
      goto start_report;
    }
    else if (no_nodegroup_active)
    {
      if (elapsed < c_restartNoNodegroupTimeout)
      {
        signal->theData[1] = 6;
        signal->theData[2] = Uint32((c_restartNoNodegroupTimeout - elapsed + 500) / 1000);
        report_mask.assign(wait);
        retVal = 0;
        goto start_report;
      }
      tmp.bitOR(c_start.m_no_nodegroup_nodes);
      if (tmp.equal(c_definedNodes))
      {
        signal->theData[1] = 0x8000;
        report_mask.assign(c_definedNodes);
        report_mask.bitANDC(c_start.m_starting_nodes);
        retVal = 1;
        goto start_report;
      }
      else
      {
        jam();
        signal->theData[1] = 1;
        signal->theData[2] = ~0;
        report_mask.assign(wait);
        retVal = 0;
        goto start_report;
      }
    }
    else
    {
      jam();
      signal->theData[1] = 1;
      signal->theData[2] = ~0;
      report_mask.assign(wait);
      retVal = 0;
      goto start_report;
    }
  }

  if (c_restartNoNodegroupTimeout != Uint32(~0) &&
      elapsed >= c_restartNoNodegroupTimeout)
  {
    tmp.bitOR(c_start.m_no_nodegroup_nodes);
  }

  {
    const bool all = c_start.m_starting_nodes.equal(c_definedNodes);
    CheckNodeGroups* sd = (CheckNodeGroups*)&signal->theData[0];

    {
      /**
       * Check for missing node group directly
       */
      NdbNodeBitmask check;
      check.assign(c_definedNodes);
      check.bitANDC(c_start.m_starting_nodes);     // Keep not connected nodes
      check.bitOR(c_start.m_starting_nodes_w_log); //Add nodes with log
 
      sd->blockRef = reference();
      sd->requestType = CheckNodeGroups::Direct | CheckNodeGroups::ArbitCheck;
      sd->mask = check;
      EXECUTE_DIRECT(DBDIH, GSN_CHECKNODEGROUPSREQ, signal, 
                     CheckNodeGroups::SignalLength);

      if (sd->output == CheckNodeGroups::Lose)
      {
        jam();
        goto missing_nodegroup;
      }
    }
  
    sd->blockRef = reference();
    sd->requestType = CheckNodeGroups::Direct | CheckNodeGroups::ArbitCheck;
    sd->mask = c_start.m_starting_nodes;
    EXECUTE_DIRECT(DBDIH, GSN_CHECKNODEGROUPSREQ, signal, 
                   CheckNodeGroups::SignalLength);
  
    const Uint32 result = sd->output;
  
    sd->blockRef = reference();
    sd->requestType = CheckNodeGroups::Direct | CheckNodeGroups::ArbitCheck;
    sd->mask = c_start.m_starting_nodes_w_log;
    EXECUTE_DIRECT(DBDIH, GSN_CHECKNODEGROUPSREQ, signal, 
                   CheckNodeGroups::SignalLength);
  
    const Uint32 result_w_log = sd->output;

    if (tmp.equal(c_definedNodes))
    {
      /**
       * All nodes (wrt no-wait nodes) has connected...
       *   this means that we will now start or die
       */
      jam();    
      switch(result_w_log){
      case CheckNodeGroups::Lose:
      {
        jam();
        goto missing_nodegroup;
      }
      case CheckNodeGroups::Win:
        signal->theData[1] = all ? 0x8001 : 0x8002;
        report_mask.assign(c_definedNodes);
        report_mask.bitANDC(c_start.m_starting_nodes);
        retVal = 1;
        goto check_log;
      case CheckNodeGroups::Partitioning:
        ndbrequire(result != CheckNodeGroups::Lose);
        signal->theData[1] = 
          all ? 0x8001 : (result == CheckNodeGroups::Win ? 0x8002 : 0x8003);
        report_mask.assign(c_definedNodes);
        report_mask.bitANDC(c_start.m_starting_nodes);
        retVal = 1;
        goto check_log;
      }
    }

    if (c_restartPartialTimeout == Uint32(~0) ||
        elapsed < c_restartPartialTimeout)
    {
      jam();

      signal->theData[1] = c_restartPartialTimeout == (Uint32) ~0 ? 2 : 3;
      signal->theData[2] =
        c_restartPartialTimeout == Uint32(~0) ?
          Uint32(~0) :
          Uint32((c_restartPartialTimeout - elapsed + 500) / 1000);
      report_mask.assign(wait);
      retVal = 0;

      if (no_nodegroup_active && elapsed < c_restartNoNodegroupTimeout)
      {
        signal->theData[1] = 7;
        signal->theData[2] = Uint32((c_restartNoNodegroupTimeout - elapsed + 500) / 1000);
      }
      else if (no_nodegroup_active && elapsed >= c_restartNoNodegroupTimeout)
      {
        report_mask.bitANDC(c_start.m_no_nodegroup_nodes);
      }

      goto start_report;
    }
  
    /**
     * Start partial has passed...check for partitioning...
     */  
    switch(result_w_log){
    case CheckNodeGroups::Lose:
      jam();
      goto missing_nodegroup;
    case CheckNodeGroups::Partitioning:
      if (elapsed != Uint32(~0) &&
          elapsed < partitionedTimeout &&
          result != CheckNodeGroups::Win)
      {
        goto missinglog;
      }
      // Fall through...
    case CheckNodeGroups::Win:
      signal->theData[1] = 
        all ? 0x8001 : (result == CheckNodeGroups::Win ? 0x8002 : 0x8003);
      report_mask.assign(c_definedNodes);
      report_mask.bitANDC(c_start.m_starting_nodes);
      retVal = 2;
      goto check_log;
    }
  }
  ndbabort();

check_log:
  jam();
  {
    Uint32 save[4+4*NdbNodeBitmask::Size];
    memcpy(save, signal->theData, sizeof(save));

    DihRestartReq * req = CAST_PTR(DihRestartReq, signal->getDataPtrSend());
    req->senderRef = 0;
    c_start.m_starting_nodes.copyto(NdbNodeBitmask::Size, req->nodemask);
    memcpy(req->node_gcis, c_start.m_node_gci, 4*MAX_NDB_NODES);
    EXECUTE_DIRECT(DBDIH, GSN_DIH_RESTARTREQ, signal,
		   DihRestartReq::CheckLength);

    incompleteng = signal->theData[0];
    memcpy(signal->theData, save, sizeof(save));

    if (incompleteng != MAX_NDB_NODES)
    {
      jam();
      if (retVal == 1)
      {
	jam();
	goto incomplete_log;
      }
      else if (retVal == 2)
      {
	if (elapsed != Uint32(~0) && elapsed <= partitionedTimeout)
	{
	  jam();
	  goto missinglog;
	}
	else
	{
	  goto incomplete_log;
	}
      }
      ndbabort();
    }
  }
  goto start_report;

missinglog:
  signal->theData[1] = c_restartPartitionedTimeout == Uint32(~0) ? 4 : 5;
  signal->theData[2] = 
    partitionedTimeout == Uint32(~0) ?
      Uint32(~0) : Uint32((partitionedTimeout - elapsed + 500) / 1000);
  infoEvent("partitionedTimeout = %llu, elapsed = %llu", partitionedTimeout, elapsed);
  report_mask.assign(c_definedNodes);
  report_mask.bitANDC(c_start.m_starting_nodes);
  retVal = 0;
  goto start_report;
  
start_report:
  jam();
  {
    Uint32 sz = NdbNodeBitmask::Size;
    signal->theData[0] = NDB_LE_StartReport;
    signal->theData[3] = sz;
    Uint32* ptr = signal->theData+4;
    c_definedNodes.copyto(sz, ptr); ptr += sz;
    c_start.m_starting_nodes.copyto(sz, ptr); ptr += sz;
    c_start.m_skip_nodes.copyto(sz, ptr); ptr += sz;
    report_mask.copyto(sz, ptr); ptr+= sz;
    c_start.m_no_nodegroup_nodes.copyto(sz, ptr); ptr += sz;
    sendSignal(CMVMI_REF, GSN_EVENT_REP, signal,
	       4+5*NdbNodeBitmask::Size, JBB);
  }
  return retVal;
  
missing_nodegroup:
  jam();
  {
    char buf[100], mask1[100], mask2[100];
    c_start.m_starting_nodes.getText(mask1);
    tmp.assign(c_start.m_starting_nodes);
    tmp.bitANDC(c_start.m_starting_nodes_w_log);
    tmp.getText(mask2);
    BaseString::snprintf(buf, sizeof(buf),
			 "Unable to start missing node group! "
			 " starting: %s (missing working fs for: %s)",
			 mask1, mask2);
    CRASH_INSERTION(944);
    progError(__LINE__, NDBD_EXIT_INSUFFICENT_NODES, buf);
    return 0;                                     // Deadcode
  }

incomplete_log:
  jam();
  {
    char buf[100], mask1[100];
    c_start.m_starting_nodes.getText(mask1);
    BaseString::snprintf(buf, sizeof(buf),
			 "Incomplete log for node group: %d! "
			 " starting nodes: %s",
			 incompleteng, mask1);
    CRASH_INSERTION(944);
    progError(__LINE__, NDBD_EXIT_INSUFFICENT_NODES, buf);
    return 0;                                     // Deadcode
  }
}

void
Qmgr::electionWon(Signal* signal)
{
  NodeRecPtr myNodePtr;
  cpresident = getOwnNodeId(); /* This node becomes president. */
  myNodePtr.i = getOwnNodeId();
  ptrCheckGuard(myNodePtr, MAX_NDB_NODES, nodeRec);
  
  myNodePtr.p->phase = ZRUNNING;

  cpdistref = reference();
  cneighbourl = ZNIL;
  cneighbourh = ZNIL;
  myNodePtr.p->ndynamicId = 1 | (myNodePtr.p->hbOrder << 16);
  c_maxDynamicId = 1;
  c_clusterNodes.clear();
  c_clusterNodes.set(getOwnNodeId());
  
  cpresidentAlive = ZTRUE;
  NdbTick_Invalidate(&c_start_election_time);
  c_start.reset();

  signal->theData[0] = NDB_LE_CM_REGCONF;
  signal->theData[1] = getOwnNodeId();
  signal->theData[2] = cpresident;
  signal->theData[3] = myNodePtr.p->ndynamicId;
  sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 4, JBB);

  c_start.m_starting_nodes.clear(getOwnNodeId());
  if (c_start.m_starting_nodes.isclear())
  {
    jam();
    sendSttorryLab(signal, true);
  }
}

/*
4.4.11 CONTINUEB */
/*--------------------------------------------------------------------------*/
/*                                                                          */
/*--------------------------------------------------------------------------*/
/****************************>---------------------------------------------*/
/* CONTINUEB                 >        SENDER: Own block, Own node          */
/****************************>-------+INPUT : TCONTINUEB_TYPE              */
/*--------------------------------------------------------------*/
void Qmgr::regreqTimeLimitLab(Signal* signal) 
{
  if(cpresident == ZNIL)
  {
    if (c_start.m_president_candidate == ZNIL)
    {
      jam();
      c_start.m_president_candidate = getOwnNodeId();
    }
    
    cmInfoconf010Lab(signal);
  }
}//Qmgr::regreqTimelimitLab()

/**---------------------------------------------------------------------------
 * The new node will take care of giving information about own node and ask 
 * all other nodes for nodeinfo. The new node will use CM_NODEINFOREQ for 
 * that purpose. When the setup of connections to all running, the president 
 * will send a commit to all running nodes + the new node 
 * INPUT: NODE_PTR1, must be set as ZNIL if we don't enter CONNECT_NODES) 
 *                   from signal CM_NODEINFOCONF. 
 *---------------------------------------------------------------------------*/
/*******************************/
/* CM_NODEINFOCONF            */
/*******************************/
void Qmgr::execCM_NODEINFOCONF(Signal* signal) 
{
  DEBUG_START3(signal, "");

  jamEntry();

  CmNodeInfoConf * const conf = (CmNodeInfoConf*)signal->getDataPtr();

  const Uint32 nodeId = conf->nodeId;
  const Uint32 dynamicId = conf->dynamicId;
  const Uint32 version = conf->version;
  Uint32 mysql_version = conf->mysql_version;
  Uint32 lqh_workers = conf->lqh_workers;
  if (version < NDBD_SPLIT_VERSION)
  {
    jam();
    mysql_version = 0;
  }
  if (version < NDBD_MT_LQH_VERSION)
  {
    jam();
    lqh_workers = 0;
  }

  NodeRecPtr nodePtr;  
  nodePtr.i = getOwnNodeId();
  ptrAss(nodePtr, nodeRec);  
  ndbrequire(nodePtr.p->phase == ZSTARTING);
  ndbrequire(c_start.m_gsn == GSN_CM_NODEINFOREQ);
  c_start.m_nodes.clearWaitingFor(nodeId);

  /**
   * Update node info
   */
  NodeRecPtr replyNodePtr;
  replyNodePtr.i = nodeId;
  ptrCheckGuard(replyNodePtr, MAX_NDB_NODES, nodeRec);
  replyNodePtr.p->ndynamicId = dynamicId;
  replyNodePtr.p->blockRef = signal->getSendersBlockRef();
  setNodeInfo(replyNodePtr.i).m_version = version;
  setNodeInfo(replyNodePtr.i).m_mysql_version = mysql_version;
  setNodeInfo(replyNodePtr.i).m_lqh_workers = lqh_workers;

  recompute_version_info(NodeInfo::DB, version);
  
  if(!c_start.m_nodes.done()){
    jam();
    return;
  }

  /**********************************************<*/
  /* Send an ack. back to the president.          */
  /* CM_ACKADD                                    */
  /* The new node has been registered by all      */
  /* running nodes and has stored nodeinfo about  */
  /* all running nodes. The new node has to wait  */
  /* for CM_ADD (commit) from president to become */
  /* a running node in the cluster.               */
  /**********************************************<*/
  sendCmAckAdd(signal, getOwnNodeId(), CmAdd::Prepare);
  return;
}//Qmgr::execCM_NODEINFOCONF()

/**---------------------------------------------------------------------------
 * A new node sends nodeinfo about himself. The new node asks for 
 * corresponding nodeinfo back in the  CM_NODEINFOCONF.  
 *---------------------------------------------------------------------------*/
/*******************************/
/* CM_NODEINFOREQ             */
/*******************************/
void Qmgr::execCM_NODEINFOREQ(Signal* signal) 
{
  jamEntry();

  const Uint32 Tblockref = signal->getSendersBlockRef();

  NodeRecPtr nodePtr;  
  nodePtr.i = getOwnNodeId();
  ptrAss(nodePtr, nodeRec);  
  if(nodePtr.p->phase != ZRUNNING){
    jam();
    signal->theData[0] = reference();
    signal->theData[1] = getOwnNodeId();
    signal->theData[2] = ZNOT_RUNNING;
    sendSignal(Tblockref, GSN_CM_NODEINFOREF, signal, 3, JBB);
    return;
  }

  NodeRecPtr addNodePtr;
  CmNodeInfoReq * const req = (CmNodeInfoReq*)signal->getDataPtr();
  addNodePtr.i = req->nodeId;
  ptrCheckGuard(addNodePtr, MAX_NDB_NODES, nodeRec);
  addNodePtr.p->ndynamicId = req->dynamicId;
  addNodePtr.p->blockRef = signal->getSendersBlockRef();
  setNodeInfo(addNodePtr.i).m_version = req->version;

  Uint32 mysql_version = req->mysql_version;
  if (req->version < NDBD_SPLIT_VERSION)
    mysql_version = 0;
  setNodeInfo(addNodePtr.i).m_mysql_version = mysql_version;

  Uint32 lqh_workers = req->lqh_workers;
  if (req->version < NDBD_MT_LQH_VERSION)
    lqh_workers = 0;
  setNodeInfo(addNodePtr.i).m_lqh_workers = lqh_workers;

  c_maxDynamicId = req->dynamicId & 0xFFFF;

  cmAddPrepare(signal, addNodePtr, nodePtr.p);
}//Qmgr::execCM_NODEINFOREQ()

void
Qmgr::cmAddPrepare(Signal* signal, NodeRecPtr nodePtr, const NodeRec * self){
  jam();

  switch(nodePtr.p->phase){
  case ZINIT:
    jam();
    nodePtr.p->phase = ZSTARTING;
    return;
  case ZFAIL_CLOSING:
    jam();
    
#if 1
    warningEvent("Received request to incorporate node %u, "
		 "while error handling has not yet completed",
		 nodePtr.i);
    
    ndbrequire(getOwnNodeId() != cpresident);
    ndbrequire(signal->header.theVerId_signalNumber == GSN_CM_ADD);
    c_start.m_nodes.clearWaitingFor();
    c_start.m_nodes.setWaitingFor(nodePtr.i);
    c_start.m_gsn = GSN_CM_NODEINFOCONF;
#else
    warningEvent("Enabling communication to CM_ADD node %u state=%d", 
		 nodePtr.i,
		 nodePtr.p->phase);
    nodePtr.p->phase = ZSTARTING;
    nodePtr.p->failState = NORMAL;
    signal->theData[0] = 0;
    signal->theData[1] = nodePtr.i;
    sendSignal(TRPMAN_REF, GSN_OPEN_COMORD, signal, 2, JBB);
#endif
    return;
  case ZSTARTING:
    break;
  case ZRUNNING:
    ndbabort();
  case ZPREPARE_FAIL:
    ndbabort();
  case ZAPI_ACTIVATION_ONGOING:
    ndbabort();
  case ZAPI_ACTIVE:
    ndbabort();
  case ZAPI_INACTIVE:
    ndbabort();
  }
  
  sendCmAckAdd(signal, nodePtr.i, CmAdd::Prepare);
  sendApiVersionRep(signal, nodePtr);

  /* President have prepared us */
  CmNodeInfoConf * conf = (CmNodeInfoConf*)signal->getDataPtrSend();
  conf->nodeId = getOwnNodeId();
  conf->dynamicId = self->ndynamicId;
  conf->version = getNodeInfo(getOwnNodeId()).m_version;
  conf->mysql_version = getNodeInfo(getOwnNodeId()).m_mysql_version;
  conf->lqh_workers = getNodeInfo(getOwnNodeId()).m_lqh_workers;
  sendSignal(nodePtr.p->blockRef, GSN_CM_NODEINFOCONF, signal,
	     CmNodeInfoConf::SignalLength, JBB);
  DEBUG_START(GSN_CM_NODEINFOCONF, refToNode(nodePtr.p->blockRef), "");
}

void
Qmgr::sendApiVersionRep(Signal* signal, NodeRecPtr nodePtr)
{
  if (getNodeInfo(nodePtr.i).m_version >= NDBD_NODE_VERSION_REP)
  {
    jam();
    Uint32 ref = calcQmgrBlockRef(nodePtr.i);
    for(Uint32 i = 1; i<MAX_NODES; i++)
    {
      jam();
      Uint32 version = getNodeInfo(i).m_version;
      Uint32 type = getNodeInfo(i).m_type;
      if (type != NodeInfo::DB && version)
      {
	jam();
	signal->theData[0] = i;
	signal->theData[1] = version;
	sendSignal(ref, GSN_NODE_VERSION_REP, signal, 2, JBB);
      }
    }
  }
}

void
Qmgr::sendCmAckAdd(Signal * signal, Uint32 nodeId, CmAdd::RequestType type){
  
  CmAckAdd * cmAckAdd = (CmAckAdd*)signal->getDataPtrSend();
  cmAckAdd->requestType = type;
  cmAckAdd->startingNodeId = nodeId;
  cmAckAdd->senderNodeId = getOwnNodeId();
  sendSignal(cpdistref, GSN_CM_ACKADD, signal, CmAckAdd::SignalLength, JBA);
  DEBUG_START(GSN_CM_ACKADD, cpresident, "");

  switch(type){
  case CmAdd::Prepare:
    return;
  case CmAdd::AddCommit:
  case CmAdd::CommitNew:
    break;
  }

  signal->theData[0] = nodeId;
  EXECUTE_DIRECT(NDBCNTR, GSN_CM_ADD_REP, signal, 1);
  jamEntry();
}

/*
4.4.11 CM_ADD */
/**--------------------------------------------------------------------------
 * Prepare a running node to add a new node to the cluster. The running node 
 * will change phase of the new node fron ZINIT to ZWAITING. The running node 
 * will also mark that we have received a prepare. When the new node has sent 
 * us nodeinfo we can send an acknowledgement back to the president. When all 
 * running nodes has acknowledged the new node, the president will send a 
 * commit and we can change phase of the new node to ZRUNNING. The president 
 * will also send CM_ADD to himself.    
 *---------------------------------------------------------------------------*/
/*******************************/
/* CM_ADD                     */
/*******************************/
void Qmgr::execCM_ADD(Signal* signal) 
{
  NodeRecPtr addNodePtr;
  jamEntry();

  NodeRecPtr nodePtr;
  nodePtr.i = getOwnNodeId();
  ptrCheckGuard(nodePtr, MAX_NDB_NODES, nodeRec);

  CRASH_INSERTION(940);

  CmAdd * const cmAdd = (CmAdd*)signal->getDataPtr();
  const CmAdd::RequestType type = (CmAdd::RequestType)cmAdd->requestType;
  addNodePtr.i = cmAdd->startingNodeId;
  //const Uint32 startingVersion = cmAdd->startingVersion;
  ptrCheckGuard(addNodePtr, MAX_NDB_NODES, nodeRec);

  DEBUG_START3(signal, type);

  if(nodePtr.p->phase == ZSTARTING){
    jam();
    /**
     * We are joining...
     */
    ndbrequire(addNodePtr.i == nodePtr.i);
    switch(type){
    case CmAdd::Prepare:
      ndbrequire(c_start.m_gsn == GSN_CM_NODEINFOREQ);
      /**
       * Wait for CM_NODEINFO_CONF
       */
      return;
    case CmAdd::CommitNew:
      /**
       * Tata. we're in the cluster
       */
      joinedCluster(signal, addNodePtr);
      return;
    case CmAdd::AddCommit:
      ndbabort();
    }
  }

  switch (type) {
  case CmAdd::Prepare:
    cmAddPrepare(signal, addNodePtr, nodePtr.p);
    break;
  case CmAdd::AddCommit:{
    jam();
    ndbrequire(addNodePtr.p->phase == ZSTARTING);
    addNodePtr.p->phase = ZRUNNING;
    m_connectivity_check.reportNodeConnect(addNodePtr.i);
    set_hb_count(addNodePtr.i) = 0;
    c_clusterNodes.set(addNodePtr.i);
    findNeighbours(signal, __LINE__);

    /**
     * SEND A HEARTBEAT IMMEDIATELY TO DECREASE THE RISK THAT WE MISS EARLY
     * HEARTBEATS. 
     */
    sendHeartbeat(signal);
    hb_send_timer.reset(NdbTick_getCurrentTicks());

    /**
     *  ENABLE COMMUNICATION WITH ALL BLOCKS WITH THE NEWLY ADDED NODE
     */
    EnableComReq *enableComReq = (EnableComReq *)signal->getDataPtrSend();
    enableComReq->m_senderRef = reference();
    enableComReq->m_senderData = ENABLE_COM_CM_ADD_COMMIT;
    NodeBitmask::clear(enableComReq->m_nodeIds);
    NodeBitmask::set(enableComReq->m_nodeIds, addNodePtr.i);
    sendSignal(TRPMAN_REF, GSN_ENABLE_COMREQ, signal,
               EnableComReq::SignalLength, JBB);
    break;
  }
  case CmAdd::CommitNew:
    jam();
    ndbabort();
  }

}//Qmgr::execCM_ADD()

void
Qmgr::handleEnableComAddCommit(Signal *signal, Uint32 node)
{
  sendCmAckAdd(signal, node, CmAdd::AddCommit);
  if(getOwnNodeId() != cpresident){
    jam();
    c_start.reset();
  }
}

void
Qmgr::execENABLE_COMCONF(Signal *signal)
{
  const EnableComConf *enableComConf =
    (const EnableComConf *)signal->getDataPtr();
  Uint32 state = enableComConf->m_senderData;
  Uint32 node = NodeBitmask::find(enableComConf->m_nodeIds, 0);

  jamEntry();

  switch (state)
  {
    case ENABLE_COM_CM_ADD_COMMIT:
      jam();
      /* Only exactly one node possible here. */
      ndbrequire(node != NodeBitmask::NotFound);
      ndbrequire(NodeBitmask::find(enableComConf->m_nodeIds, node + 1) ==
                 NodeBitmask::NotFound);
      handleEnableComAddCommit(signal, node);
      break;

    case ENABLE_COM_CM_COMMIT_NEW:
      jam();
      handleEnableComCommitNew(signal);
      break;

    case ENABLE_COM_API_REGREQ:
      jam();
      /* Only exactly one node possible here. */
      ndbrequire(node != NodeBitmask::NotFound);
      ndbrequire(NodeBitmask::find(enableComConf->m_nodeIds, node + 1) ==
                 NodeBitmask::NotFound);
      handleEnableComApiRegreq(signal, node);
      break;

    default:
      jam();
      ndbabort();
  }
}

void
Qmgr::joinedCluster(Signal* signal, NodeRecPtr nodePtr){
  /**
   * WE HAVE BEEN INCLUDED IN THE CLUSTER WE CAN START BEING PART OF THE 
   * HEARTBEAT PROTOCOL AND WE WILL ALSO ENABLE COMMUNICATION WITH ALL 
   * NODES IN THE CLUSTER.
   */
  nodePtr.p->phase = ZRUNNING;
  set_hb_count(nodePtr.i) = 0;
  findNeighbours(signal, __LINE__);
  c_clusterNodes.set(nodePtr.i);
  c_start.reset();

  /**
   * SEND A HEARTBEAT IMMEDIATELY TO DECREASE THE RISK 
   * THAT WE MISS EARLY HEARTBEATS. 
   */
  sendHeartbeat(signal);
  hb_send_timer.reset(NdbTick_getCurrentTicks());

  /**
   * ENABLE COMMUNICATION WITH ALL BLOCKS IN THE CURRENT CLUSTER AND SET 
   * THE NODES IN THE CLUSTER TO BE RUNNING. 
   */
  EnableComReq *enableComReq = (EnableComReq *)signal->getDataPtrSend();
  enableComReq->m_senderRef = reference();
  enableComReq->m_senderData = ENABLE_COM_CM_COMMIT_NEW;
  NodeBitmask::clear(enableComReq->m_nodeIds);
  jam();
  for (nodePtr.i = 1; nodePtr.i < MAX_NDB_NODES; nodePtr.i++) {
    ptrAss(nodePtr, nodeRec);
    if ((nodePtr.p->phase == ZRUNNING) && (nodePtr.i != getOwnNodeId())) {
      /*-------------------------------------------------------------------*/
      // Enable full communication to all other nodes. Not really necessary 
      // to open communication to ourself.
      /*-------------------------------------------------------------------*/
      jamLine(nodePtr.i);
      NodeBitmask::set(enableComReq->m_nodeIds, nodePtr.i);
    }//if
  }//for

  if (!NodeBitmask::isclear(enableComReq->m_nodeIds))
  {
    jam();
    sendSignal(TRPMAN_REF, GSN_ENABLE_COMREQ, signal,
               EnableComReq::SignalLength, JBB);
  }
  else
  {
    handleEnableComCommitNew(signal);
  }
}

void
Qmgr::handleEnableComCommitNew(Signal *signal)
{
  sendSttorryLab(signal, true);
  
  sendCmAckAdd(signal, getOwnNodeId(), CmAdd::CommitNew);
}

/*  4.10.7 CM_ACKADD        - PRESIDENT IS RECEIVER -       */
/*---------------------------------------------------------------------------*/
/* Entry point for an ack add signal. 
 * The TTYPE defines if it is a prepare or a commit.                         */
/*---------------------------------------------------------------------------*/
void Qmgr::execCM_ACKADD(Signal* signal) 
{
  NodeRecPtr addNodePtr;
  NodeRecPtr senderNodePtr;
  jamEntry();

  CmAckAdd * const cmAckAdd = (CmAckAdd*)signal->getDataPtr();
  const CmAdd::RequestType type = (CmAdd::RequestType)cmAckAdd->requestType;
  addNodePtr.i = cmAckAdd->startingNodeId;
  senderNodePtr.i = cmAckAdd->senderNodeId;

  DEBUG_START3(signal, type);

  if (cpresident != getOwnNodeId()) {
    jam();
    /*-----------------------------------------------------------------------*/
    /* IF WE ARE NOT PRESIDENT THEN WE SHOULD NOT RECEIVE THIS MESSAGE.      */
    /*------------------------------------------------------------_----------*/
    warningEvent("Received CM_ACKADD from %d president=%d",
		 senderNodePtr.i, cpresident);
    return;
  }//if

  if (addNodePtr.i != c_start.m_startNode) {
    jam();
    /*----------------------------------------------------------------------*/
    /* THIS IS NOT THE STARTING NODE. WE ARE ACTIVE NOW WITH ANOTHER START. */
    /*----------------------------------------------------------------------*/
    warningEvent("Received CM_ACKADD from %d with startNode=%d != own %d",
		 senderNodePtr.i, addNodePtr.i, c_start.m_startNode);
    return;
  }//if
  
  ndbrequire(c_start.m_gsn == GSN_CM_ADD);
  c_start.m_nodes.clearWaitingFor(senderNodePtr.i);
  if(!c_start.m_nodes.done()){
    jam();
    return;
  }
  
  switch (type) {
  case CmAdd::Prepare:{
    jam();

    /*----------------------------------------------------------------------*/
    /* ALL RUNNING NODES HAVE PREPARED THE INCLUSION OF THIS NEW NODE.      */
    /*----------------------------------------------------------------------*/
    c_start.m_gsn = GSN_CM_ADD;
    c_start.m_nodes = c_clusterNodes;

    CmAdd * const cmAdd = (CmAdd*)signal->getDataPtrSend();
    cmAdd->requestType = CmAdd::AddCommit;
    cmAdd->startingNodeId = addNodePtr.i; 
    cmAdd->startingVersion = getNodeInfo(addNodePtr.i).m_version;
    cmAdd->startingMysqlVersion = getNodeInfo(addNodePtr.i).m_mysql_version;
    NodeReceiverGroup rg(QMGR, c_clusterNodes);
    sendSignal(rg, GSN_CM_ADD, signal, CmAdd::SignalLength, JBA);
    DEBUG_START2(GSN_CM_ADD, rg, "AddCommit");
    return;
  }
  case CmAdd::AddCommit:{
    jam();

    /****************************************/
    /* Send commit to the new node so he    */
    /* will change PHASE into ZRUNNING      */
    /****************************************/
    c_start.m_gsn = GSN_CM_ADD;
    c_start.m_nodes.clearWaitingFor();
    c_start.m_nodes.setWaitingFor(addNodePtr.i);

    CmAdd * const cmAdd = (CmAdd*)signal->getDataPtrSend();
    cmAdd->requestType = CmAdd::CommitNew;
    cmAdd->startingNodeId = addNodePtr.i; 
    cmAdd->startingVersion = getNodeInfo(addNodePtr.i).m_version;
    cmAdd->startingMysqlVersion = getNodeInfo(addNodePtr.i).m_mysql_version;
    sendSignal(calcQmgrBlockRef(addNodePtr.i), GSN_CM_ADD, signal, 
	       CmAdd::SignalLength, JBA);
    DEBUG_START(GSN_CM_ADD, addNodePtr.i, "CommitNew");
    /**
     * Report to DBDIH that a node have been added to the nodes included
     * in the heartbeat protocol.
     */
    InclNodeHBProtocolRep *rep = (InclNodeHBProtocolRep*)signal->getDataPtrSend();
    rep->nodeId = addNodePtr.i;
    EXECUTE_DIRECT(DBDIH, GSN_INCL_NODE_HB_PROTOCOL_REP, signal,
                   InclNodeHBProtocolRep::SignalLength);
    return;
  }
  case CmAdd::CommitNew:
    jam();
    /**
     * Tell arbitration about new node.
     */
    handleArbitNdbAdd(signal, addNodePtr.i);
    c_start.reset();

    if (c_start.m_starting_nodes.get(addNodePtr.i))
    {
      jam();
      c_start.m_starting_nodes.clear(addNodePtr.i);
      if (c_start.m_starting_nodes.isclear())
      {
	jam();
	sendSttorryLab(signal, true);
      }
    }
    return;
  }//switch
  ndbabort();
}//Qmgr::execCM_ACKADD()

/**-------------------------------------------------------------------------
 * WE HAVE BEEN INCLUDED INTO THE CLUSTER. IT IS NOW TIME TO CALCULATE WHICH 
 * ARE OUR LEFT AND RIGHT NEIGHBOURS FOR THE HEARTBEAT PROTOCOL. 
 *--------------------------------------------------------------------------*/
void Qmgr::findNeighbours(Signal* signal, Uint32 from) 
{
  UintR toldLeftNeighbour;
  UintR tfnLeftFound;
  UintR tfnMaxFound;
  UintR tfnMinFound;
  UintR tfnRightFound;
  NodeRecPtr fnNodePtr;
  NodeRecPtr fnOwnNodePtr;

  Uint32 toldRightNeighbour = cneighbourh;
  toldLeftNeighbour = cneighbourl;
  tfnLeftFound = 0;
  tfnMaxFound = 0;
  tfnMinFound = (UintR)-1;
  tfnRightFound = (UintR)-1;
  fnOwnNodePtr.i = getOwnNodeId();
  ptrCheckGuard(fnOwnNodePtr, MAX_NDB_NODES, nodeRec);
  for (fnNodePtr.i = 1; fnNodePtr.i < MAX_NDB_NODES; fnNodePtr.i++) {
    ptrAss(fnNodePtr, nodeRec);
    if (fnNodePtr.i != fnOwnNodePtr.i) {
      jamLine(fnNodePtr.i);
      if (fnNodePtr.p->phase == ZRUNNING) {
        if (tfnMinFound > fnNodePtr.p->ndynamicId) {
          jam();
          tfnMinFound = fnNodePtr.p->ndynamicId;
        }//if
        if (tfnMaxFound < fnNodePtr.p->ndynamicId) {
          jam();
          tfnMaxFound = fnNodePtr.p->ndynamicId;
        }//if
        if (fnOwnNodePtr.p->ndynamicId > fnNodePtr.p->ndynamicId) {
          jam();
          if (fnNodePtr.p->ndynamicId > tfnLeftFound) {
            jam();
            tfnLeftFound = fnNodePtr.p->ndynamicId;
          }//if
        } else {
          jam();
          if (fnNodePtr.p->ndynamicId < tfnRightFound) {
            jam();
            tfnRightFound = fnNodePtr.p->ndynamicId;
          }//if
        }//if
      }//if
    }//if
  }//for
  if (tfnLeftFound == 0) {
    if (tfnMinFound == (UintR)-1) {
      jam();
      cneighbourl = ZNIL;
    } else {
      jam();
      cneighbourl = translateDynamicIdToNodeId(signal, tfnMaxFound);
    }//if
  } else {
    jam();
    cneighbourl = translateDynamicIdToNodeId(signal, tfnLeftFound);
  }//if
  if (tfnRightFound == (UintR)-1) {
    if (tfnMaxFound == 0) {
      jam();
      cneighbourh = ZNIL;
    } else {
      jam();
      cneighbourh = translateDynamicIdToNodeId(signal, tfnMinFound);
    }//if
  } else {
    jam();
    cneighbourh = translateDynamicIdToNodeId(signal, tfnRightFound);
  }//if
  if (toldLeftNeighbour != cneighbourl) {
    jam();
    if (cneighbourl != ZNIL) {
      jam();
      /**-------------------------------------------------------------------*/
      /* WE ARE SUPERVISING A NEW LEFT NEIGHBOUR. WE START WITH ALARM COUNT 
       * EQUAL TO ZERO.
       *---------------------------------------------------------------------*/
      fnNodePtr.i = cneighbourl;
      ptrCheckGuard(fnNodePtr, MAX_NDB_NODES, nodeRec);
      set_hb_count(fnNodePtr.i) = 0;
    }//if
  }//if

  signal->theData[0] = NDB_LE_FIND_NEIGHBOURS;
  signal->theData[1] = getOwnNodeId();
  signal->theData[2] = cneighbourl;
  signal->theData[3] = cneighbourh;
  signal->theData[4] = fnOwnNodePtr.p->ndynamicId;
  UintR Tlen = 5;
  sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, Tlen, JBB);
  g_eventLogger->info("findNeighbours from: %u old (left: %u right: %u) new (%u %u)", 
                      from,
                      toldLeftNeighbour,
                      toldRightNeighbour,
                      cneighbourl,
                      cneighbourh);
}//Qmgr::findNeighbours()

/*
4.10.7 INIT_DATA        */
/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
void Qmgr::initData(Signal* signal) 
{
  // catch-all for missing initializations
  memset(&arbitRec, 0, sizeof(arbitRec));

  /**
   * Timeouts
   */
  const ndb_mgm_configuration_iterator * p = 
    m_ctx.m_config.getOwnConfigIterator();
  ndbrequire(p != 0);
  
  Uint32 hbDBDB = 1500;
  Uint32 arbitTimeout = 1000;
  Uint32 arbitMethod = ARBIT_METHOD_DEFAULT;
  Uint32 ccInterval = 0;
  c_restartPartialTimeout = 30000;
  c_restartPartitionedTimeout = Uint32(~0);
  c_restartFailureTimeout = Uint32(~0);
  c_restartNoNodegroupTimeout = 15000;
  ndb_mgm_get_int_parameter(p, CFG_DB_HEARTBEAT_INTERVAL, &hbDBDB);
  ndb_mgm_get_int_parameter(p, CFG_DB_ARBIT_TIMEOUT, &arbitTimeout);
  ndb_mgm_get_int_parameter(p, CFG_DB_ARBIT_METHOD, &arbitMethod);
  ndb_mgm_get_int_parameter(p, CFG_DB_START_PARTIAL_TIMEOUT, 
			    &c_restartPartialTimeout);
  ndb_mgm_get_int_parameter(p, CFG_DB_START_PARTITION_TIMEOUT,
			    &c_restartPartitionedTimeout);
  ndb_mgm_get_int_parameter(p, CFG_DB_START_NO_NODEGROUP_TIMEOUT,
			    &c_restartNoNodegroupTimeout);
  ndb_mgm_get_int_parameter(p, CFG_DB_START_FAILURE_TIMEOUT,
			    &c_restartFailureTimeout);
  ndb_mgm_get_int_parameter(p, CFG_DB_CONNECT_CHECK_DELAY,
                            &ccInterval);

  if(c_restartPartialTimeout == 0)
  {
    c_restartPartialTimeout = Uint32(~0);
  }

  if (c_restartPartitionedTimeout == 0)
  {
    c_restartPartitionedTimeout = Uint32(~0);
  }

  if (c_restartFailureTimeout == 0)
  {
    c_restartFailureTimeout = Uint32(~0);
  }

  if (c_restartNoNodegroupTimeout == 0)
  {
    c_restartNoNodegroupTimeout = Uint32(~0);
  }

  setHbDelay(hbDBDB);
  setCCDelay(ccInterval);
  setArbitTimeout(arbitTimeout);

  arbitRec.method = (ArbitRec::Method)arbitMethod;
  arbitRec.state = ARBIT_NULL;          // start state for all nodes
  arbitRec.apiMask[0].clear();          // prepare for ARBIT_CFG

  Uint32 sum = 0;
  ArbitSignalData* const sd = (ArbitSignalData*)&signal->theData[0];
  for (unsigned rank = 1; rank <= 2; rank++) {
    sd->sender = getOwnNodeId();
    sd->code = rank;
    sd->node = 0;
    sd->ticket.clear();
    sd->mask.clear();
    ndb_mgm_configuration_iterator * iter =
      m_ctx.m_config.getClusterConfigIterator();
    for (ndb_mgm_first(iter); ndb_mgm_valid(iter); ndb_mgm_next(iter)) {
      Uint32 tmp = 0;
      if (ndb_mgm_get_int_parameter(iter, CFG_NODE_ARBIT_RANK, &tmp) == 0 && 
	  tmp == rank){
	Uint32 nodeId = 0;
	ndbrequire(!ndb_mgm_get_int_parameter(iter, CFG_NODE_ID, &nodeId));
	sd->mask.set(nodeId);
      }
    }
    sum += sd->mask.count();
    execARBIT_CFG(signal);
  }

  if (arbitRec.method == ArbitRec::METHOD_DEFAULT &&
      sum == 0)
  {
    jam();
    infoEvent("Arbitration disabled, all API nodes have rank 0");
    arbitRec.method = ArbitRec::DISABLED;
  }

  setNodeInfo(getOwnNodeId()).m_mysql_version = NDB_MYSQL_VERSION_D;

  ndb_mgm_configuration_iterator * iter =
    m_ctx.m_config.getClusterConfigIterator();
  for (ndb_mgm_first(iter); ndb_mgm_valid(iter); ndb_mgm_next(iter))
  {
    jam();
    Uint32 nodeId = 0;
    if (ndb_mgm_get_int_parameter(iter, CFG_NODE_ID, &nodeId) == 0)
    {
      jam();
      if (nodeId < MAX_NDB_NODES && getNodeInfo(nodeId).m_type == NodeInfo::DB)
      {
        Uint32 hbOrder = 0;
        ndb_mgm_get_int_parameter(iter, CFG_DB_HB_ORDER, &hbOrder);

        NodeRecPtr nodePtr;
        nodePtr.i = nodeId;
        ptrCheckGuard(nodePtr, MAX_NDB_NODES, nodeRec);
        nodePtr.p->hbOrder = hbOrder;
      }
    }
  }
  int hb_order_error = check_hb_order_config();
  if (hb_order_error == -1)
  {
    char msg[] = "Illegal HeartbeatOrder config, "
                 "all nodes must have non-zero config value";
    progError(__LINE__, NDBD_EXIT_INVALID_CONFIG, msg);
    return;
  }
  if (hb_order_error == -2)
  {
    char msg[] = "Illegal HeartbeatOrder config, "
                 "the nodes must have distinct config values";
    progError(__LINE__, NDBD_EXIT_INVALID_CONFIG, msg);
    return;
  }
  ndbrequire(hb_order_error == 0);
}//Qmgr::initData()


/**---------------------------------------------------------------------------
 * HERE WE RECEIVE THE JOB TABLE SIGNAL EVERY 10 MILLISECONDS. 
 * WE WILL USE THIS TO CHECK IF IT IS TIME TO CHECK THE NEIGHBOUR NODE. 
 * WE WILL ALSO SEND A SIGNAL TO BLOCKS THAT NEED A TIME SIGNAL AND 
 * DO NOT WANT TO USE JOB TABLE SIGNALS.
 *---------------------------------------------------------------------------*/
void Qmgr::timerHandlingLab(Signal* signal) 
{
  const NDB_TICKS TcurrentTime = NdbTick_getCurrentTicks();
  NodeRecPtr myNodePtr;
  myNodePtr.i = getOwnNodeId();
  ptrCheckGuard(myNodePtr, MAX_NDB_NODES, nodeRec);

  const Uint32 sentHi = signal->theData[1];
  const Uint32 sentLo = signal->theData[2];
  const NDB_TICKS sent((Uint64(sentHi) << 32) | sentLo);
  
  if (NdbTick_Compare(sent,TcurrentTime) > 0)
  {
    jam();
    const Uint64 backwards = NdbTick_Elapsed(TcurrentTime,sent).milliSec();
    if (backwards > 0) //Ignore sub millisecond backticks
    {
      g_eventLogger->warning("timerHandlingLab, clock ticked backwards: %llu (ms)",
                              backwards);
    }
  }
  else
  {
    const Uint64 elapsed = NdbTick_Elapsed(sent,TcurrentTime).milliSec();
    if (elapsed >= 1000)
    {
      jam();
      g_eventLogger->warning("timerHandlingLab, expected 10ms sleep"
                             ", not scheduled for: %d (ms)", int(elapsed));
    }
    else if (elapsed >= 150)
    {
      g_eventLogger->info("timerHandlingLab, expected 10ms sleep"
                          ", not scheduled for: %d (ms)", int(elapsed));
    }
  }

  if (myNodePtr.p->phase == ZRUNNING) {
    jam();
    /**---------------------------------------------------------------------
     * WE ARE ONLY PART OF HEARTBEAT CLUSTER IF WE ARE UP AND RUNNING. 
     *---------------------------------------------------------------------*/
    if (hb_send_timer.check(TcurrentTime)) {
      jam();
      sendHeartbeat(signal);
      hb_send_timer.reset(TcurrentTime);
    }
    if (likely(! m_connectivity_check.m_active))
    {
      if (hb_check_timer.check(TcurrentTime)) {
        jam();
        checkHeartbeat(signal);
        hb_check_timer.reset(TcurrentTime);
      }
    }
    else
    {
      /* Connectivity check */
      if (m_connectivity_check.m_timer.check(TcurrentTime)) {
        jam();
        checkConnectivityTimeSignal(signal);
        m_connectivity_check.m_timer.reset(TcurrentTime);
      }
    }
  }
  
  if (interface_check_timer.check(TcurrentTime)) {
    jam();
    interface_check_timer.reset(TcurrentTime);
    checkStartInterface(signal, TcurrentTime);
  }

  if (hb_api_timer.check(TcurrentTime)) 
  {
    jam();
    hb_api_timer.reset(TcurrentTime);
    apiHbHandlingLab(signal, TcurrentTime);
  }

  //--------------------------------------------------
  // Resend this signal with 10 milliseconds delay.
  //--------------------------------------------------
  signal->theData[0] = ZTIMER_HANDLING;
  signal->theData[1] = Uint32(TcurrentTime.getUint64() >> 32);
  signal->theData[2] = Uint32(TcurrentTime.getUint64());
  sendSignalWithDelay(QMGR_REF, GSN_CONTINUEB, signal, 10, 3);
  return;
}//Qmgr::timerHandlingLab()

/*---------------------------------------------------------------------------*/
/*       THIS MODULE HANDLES THE SENDING AND RECEIVING OF HEARTBEATS.        */
/*---------------------------------------------------------------------------*/
void Qmgr::sendHeartbeat(Signal* signal) 
{
  NodeRecPtr localNodePtr;
  localNodePtr.i = cneighbourh;
  if (localNodePtr.i == ZNIL) {
    jam();
    /**---------------------------------------------------------------------
     * THERE ARE NO NEIGHBOURS. THIS IS POSSIBLE IF WE ARE THE ONLY NODE IN 
     * THE CLUSTER.IN THIS CASE WE DO NOT NEED TO SEND ANY HEARTBEAT SIGNALS.
     *-----------------------------------------------------------------------*/
    return;
  }//if

  if(ERROR_INSERTED(946))
  {
    sleep(180);
    return;
  }

  ptrCheckGuard(localNodePtr, MAX_NDB_NODES, nodeRec);
  signal->theData[0] = getOwnNodeId();

  sendSignal(localNodePtr.p->blockRef, GSN_CM_HEARTBEAT, signal, 1, JBA);
#ifdef VM_TRACE
  signal->theData[0] = NDB_LE_SentHeartbeat;
  signal->theData[1] = localNodePtr.i;
  sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 2, JBB);  
#endif
}//Qmgr::sendHeartbeat()

void Qmgr::checkHeartbeat(Signal* signal) 
{
  NodeRecPtr nodePtr;

  nodePtr.i = cneighbourl;
  if (nodePtr.i == ZNIL) {
    jam();
    /**---------------------------------------------------------------------
     * THERE ARE NO NEIGHBOURS. THIS IS POSSIBLE IF WE ARE THE ONLY NODE IN 
     * THE CLUSTER. IN THIS CASE WE DO NOT NEED TO CHECK ANY HEARTBEATS.
     *-----------------------------------------------------------------------*/
    return;
  }//if
  ptrCheckGuard(nodePtr, MAX_NDB_NODES, nodeRec);
  
  set_hb_count(nodePtr.i)++;
  ndbrequire(nodePtr.p->phase == ZRUNNING);
  ndbrequire(getNodeInfo(nodePtr.i).m_type == NodeInfo::DB);

  if (get_hb_count(nodePtr.i) > 2)
  {
    signal->theData[0] = NDB_LE_MissedHeartbeat;
    signal->theData[1] = nodePtr.i;
    signal->theData[2] = get_hb_count(nodePtr.i) - 1;
    sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 3, JBB);
  }

  if (get_hb_count(nodePtr.i) > 4)
  {
    jam();
    if (m_connectivity_check.getEnabled())
    {
      jam();
      /* Start connectivity check, indicating the cause */
      startConnectivityCheck(signal, FailRep::ZHEARTBEAT_FAILURE, nodePtr.i);
      return;
    }
    else
    {
      /**----------------------------------------------------------------------
       * OUR LEFT NEIGHBOUR HAVE KEPT QUIET FOR THREE CONSECUTIVE HEARTBEAT
       * PERIODS. THUS WE DECLARE HIM DOWN.
       *----------------------------------------------------------------------*/
      signal->theData[0] = NDB_LE_DeadDueToHeartbeat;
      signal->theData[1] = nodePtr.i;
      sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 2, JBB);

      failReportLab(signal, nodePtr.i, FailRep::ZHEARTBEAT_FAILURE, getOwnNodeId());
      return;
    }
  }//if
}//Qmgr::checkHeartbeat()

void Qmgr::apiHbHandlingLab(Signal* signal, NDB_TICKS now)
{
  NodeRecPtr TnodePtr;

  jam();
  for (TnodePtr.i = 1; TnodePtr.i < MAX_NODES; TnodePtr.i++) {
    const Uint32 nodeId = TnodePtr.i;
    ptrAss(TnodePtr, nodeRec);
    
    const NodeInfo::NodeType type = getNodeInfo(nodeId).getType();
    if(type == NodeInfo::DB)
      continue;
    
    if(type == NodeInfo::INVALID)
      continue;

    if (c_connectedNodes.get(nodeId))
    {
      jamLine(nodeId);
      set_hb_count(TnodePtr.i)++;

      if (get_hb_count(TnodePtr.i) > 2)
      {
	signal->theData[0] = NDB_LE_MissedHeartbeat;
	signal->theData[1] = nodeId;
	signal->theData[2] = get_hb_count(TnodePtr.i) - 1;
	sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 3, JBB);
      }

      if (get_hb_count(TnodePtr.i) > 4)
      {
        jam();
	/*------------------------------------------------------------------*/
	/* THE API NODE HAS NOT SENT ANY HEARTBEAT FOR THREE SECONDS.
	 * WE WILL DISCONNECT FROM IT NOW.
	 *------------------------------------------------------------------*/
	/*------------------------------------------------------------------*/
	/* We call node_failed to release all connections for this api node */
	/*------------------------------------------------------------------*/
	signal->theData[0] = NDB_LE_DeadDueToHeartbeat;
	signal->theData[1] = nodeId;
	sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 2, JBB);

        api_failed(signal, nodeId);
      }//if
    }//if
    else if (TnodePtr.p->phase == ZAPI_INACTIVE &&
             TnodePtr.p->m_secret != 0 &&
             NdbTick_Compare(now,TnodePtr.p->m_alloc_timeout) > 0)
    {
      jam();
      TnodePtr.p->m_secret = 0;
      warningEvent("Releasing node id allocation for node %u",
                   TnodePtr.i);
    }
  }//for
  return;
}//Qmgr::apiHbHandlingLab()

void Qmgr::checkStartInterface(Signal* signal, NDB_TICKS now) 
{
  NodeRecPtr nodePtr;
  /*------------------------------------------------------------------------*/
  // This method is called once per second. After a disconnect we wait at 
  // least three seconds before allowing new connects. We will also ensure 
  // that handling of the failure is completed before we allow new connections.
  /*------------------------------------------------------------------------*/
  jam();
  for (nodePtr.i = 1; nodePtr.i < MAX_NODES; nodePtr.i++) {
    ptrAss(nodePtr, nodeRec);
    Uint32 type = getNodeInfo(nodePtr.i).m_type;
    if (nodePtr.p->phase == ZFAIL_CLOSING) {
      jamLine(nodePtr.i);
      set_hb_count(nodePtr.i)++;
      if (c_connectedNodes.get(nodePtr.i)){
        jam();
	/*-------------------------------------------------------------------*/
	// We need to ensure that the connection is not restored until it has 
	// been disconnected for at least three seconds.
	/*-------------------------------------------------------------------*/
        set_hb_count(nodePtr.i) = 0;
      }//if
      if ((get_hb_count(nodePtr.i) > 3)
	  && (nodePtr.p->failState == NORMAL)) {
	/**------------------------------------------------------------------
	 * WE HAVE DISCONNECTED THREE SECONDS AGO. WE ARE NOW READY TO 
	 * CONNECT AGAIN AND ACCEPT NEW REGISTRATIONS FROM THIS NODE. 
	 * WE WILL NOT ALLOW CONNECTIONS OF API NODES UNTIL API FAIL HANDLING 
	 * IS COMPLETE.
	 *-------------------------------------------------------------------*/
        nodePtr.p->failState = NORMAL;
        nodePtr.p->m_secret = 0;
        switch(type){
        case NodeInfo::DB:
          jam();
          nodePtr.p->phase = ZINIT;
          break;
        case NodeInfo::MGM:
          jam();
          nodePtr.p->phase = ZAPI_INACTIVE;
          break;
        case NodeInfo::API:
          jam();
          if (c_allow_api_connect)
          {
            jam();
            nodePtr.p->phase = ZAPI_INACTIVE;
            break;
          }
          else
          {
            /**
             * Dont allow API node to connect before c_allow_api_connect
             */
            jam();
            set_hb_count(nodePtr.i) = 3;
            continue;
          }
        }

        set_hb_count(nodePtr.i) = 0;
        signal->theData[0] = 0;
        signal->theData[1] = nodePtr.i;
        sendSignal(TRPMAN_REF, GSN_OPEN_COMORD, signal, 2, JBB);
      }
      else
      {
        jam();
        if(((get_hb_count(nodePtr.i) + 1) % 30) == 0)
        {
          jam();
	  char buf[256];
          if (getNodeInfo(nodePtr.i).m_type == NodeInfo::DB)
          {
            jam();
            BaseString::snprintf(buf, sizeof(buf),
                                 "Failure handling of node %d has not completed"
                                 " in %d seconds - state = %d",
                                 nodePtr.i,
                                 get_hb_count(nodePtr.i),
                                 nodePtr.p->failState);
            warningEvent("%s", buf);

            /**
             * Also dump DIH nf-state
             */
            signal->theData[0] = DumpStateOrd::DihTcSumaNodeFailCompleted;
            signal->theData[1] = nodePtr.i;
            sendSignal(DBDIH_REF, GSN_DUMP_STATE_ORD, signal, 2, JBB);
          }
          else
          {
            jam();
            BaseString::snprintf(buf, sizeof(buf),
                                 "Failure handling of api %u has not completed"
                                 " in %d seconds - state = %d",
                                 nodePtr.i,
                                 get_hb_count(nodePtr.i),
                                 nodePtr.p->failState);
            warningEvent("%s", buf);
            if (nodePtr.p->failState == WAITING_FOR_API_FAILCONF)
            {
              jam();
              static_assert(NDB_ARRAY_SIZE(nodePtr.p->m_failconf_blocks) == 5, "");
              BaseString::snprintf(buf, sizeof(buf),
                                   "  Waiting for blocks: %u %u %u %u %u",
                                   nodePtr.p->m_failconf_blocks[0],
                                   nodePtr.p->m_failconf_blocks[1],
                                   nodePtr.p->m_failconf_blocks[2],
                                   nodePtr.p->m_failconf_blocks[3],
                                   nodePtr.p->m_failconf_blocks[4]);
              warningEvent("%s", buf);
            }
          }
	}
      }
    }
    else if (type == NodeInfo::DB && nodePtr.p->phase == ZINIT &&
             nodePtr.p->m_secret != 0 &&
             NdbTick_Compare(now,nodePtr.p->m_alloc_timeout) > 0)
    {
      jam();
      nodePtr.p->m_secret = 0;
      warningEvent("Releasing node id allocation for node %u",
                   nodePtr.i);
    }
  }//for
  return;
}//Qmgr::checkStartInterface()

/**-------------------------------------------------------------------------
 * This method is called when a DISCONNECT_REP signal arrived which means that
 * the API node is gone and we want to release resources in TC/DICT blocks.
 *---------------------------------------------------------------------------*/
void Qmgr::sendApiFailReq(Signal* signal, Uint16 failedNodeNo, bool sumaOnly) 
{
  jamEntry();
  signal->theData[0] = failedNodeNo;
  signal->theData[1] = QMGR_REF;

  /* We route the ApiFailReq signals via CMVMI
   * This is done to ensure that they are received after
   * any pending signals from the failed Api node when
   * running ndbmtd, as these signals would be enqueued from
   * the thread running CMVMI
   */
  Uint32 routedSignalSectionI = RNIL;
  ndbrequire(appendToSection(routedSignalSectionI,
                             &signal->theData[0],
                             2));
  SectionHandle handle(this, routedSignalSectionI);

  /* RouteOrd data */
  RouteOrd* routeOrd = (RouteOrd*) &signal->theData[0];
  routeOrd->srcRef = reference();
  routeOrd->gsn = GSN_API_FAILREQ;
  routeOrd->from = failedNodeNo;

  NodeRecPtr failedNodePtr;
  failedNodePtr.i = failedNodeNo;
  ptrCheckGuard(failedNodePtr, MAX_NODES, nodeRec);
  failedNodePtr.p->failState = WAITING_FOR_API_FAILCONF;


  /* Send ROUTE_ORD signals to CMVMI via JBA
   * CMVMI will then immediately send the API_FAILREQ
   * signals to the destination block(s) using JBB
   * These API_FAILREQ signals will be sent *after*
   * any JBB signals enqueued from the failed API
   * by the CMVMI thread.
   */
  if (!sumaOnly)
  {
    jam();
    add_failconf_block(failedNodePtr, DBTC);
    routeOrd->dstRef = DBTC_REF;
    sendSignalNoRelease(TRPMAN_REF, GSN_ROUTE_ORD, signal,
                        RouteOrd::SignalLength,
                        JBA, &handle);

    add_failconf_block(failedNodePtr, DBDICT);
    routeOrd->dstRef = DBDICT_REF;
    sendSignalNoRelease(TRPMAN_REF, GSN_ROUTE_ORD, signal,
                        RouteOrd::SignalLength,
                        JBA, &handle);

    add_failconf_block(failedNodePtr, DBSPJ);
    routeOrd->dstRef = DBSPJ_REF;
    sendSignalNoRelease(TRPMAN_REF, GSN_ROUTE_ORD, signal,
                        RouteOrd::SignalLength,
                        JBA, &handle);
  }

  /* Suma always notified */
  add_failconf_block(failedNodePtr, SUMA);
  routeOrd->dstRef = SUMA_REF;
  sendSignal(TRPMAN_REF, GSN_ROUTE_ORD, signal,
             RouteOrd::SignalLength,
             JBA, &handle);
}//Qmgr::sendApiFailReq()

void Qmgr::execAPI_FAILREQ(Signal* signal)
{
  jamEntry();
  NodeRecPtr failedNodePtr;
  failedNodePtr.i = signal->theData[0];
  // signal->theData[1] == QMGR_REF
  ptrCheckGuard(failedNodePtr, MAX_NODES, nodeRec);

  ndbrequire(getNodeInfo(failedNodePtr.i).getType() != NodeInfo::DB);

  api_failed(signal, signal->theData[0]);
}

void Qmgr::execAPI_FAILCONF(Signal* signal) 
{
  NodeRecPtr failedNodePtr;

  jamEntry();
  failedNodePtr.i = signal->theData[0];  
  ptrCheckGuard(failedNodePtr, MAX_NODES, nodeRec);

  Uint32 block = refToMain(signal->theData[1]);
  if (failedNodePtr.p->failState != WAITING_FOR_API_FAILCONF ||
      !remove_failconf_block(failedNodePtr, block))
  {
    jam();
    ndbout << "execAPI_FAILCONF from " << block
           << " failedNodePtr.p->failState = "
	   << (Uint32)(failedNodePtr.p->failState)
           << " blocks: ";
    for (Uint32 i = 0;i<NDB_ARRAY_SIZE(failedNodePtr.p->m_failconf_blocks);i++)
    {
      printf("%u ", failedNodePtr.p->m_failconf_blocks[i]);
    }
    ndbout << endl;
    systemErrorLab(signal, __LINE__);
  }//if

  if (is_empty_failconf_block(failedNodePtr))
  {
    jam();
    /**
     * When we set this state, connection will later be opened
     *   in checkStartInterface
     */
    failedNodePtr.p->failState = NORMAL;

    /**
     * Reset m_version only after all blocks has responded with API_FAILCONF
     *   so that no block risks reading 0 as node-version
     */
    setNodeInfo(failedNodePtr.i).m_version = 0;
    recompute_version_info(getNodeInfo(failedNodePtr.i).m_type);
  }
  return;
}//Qmgr::execAPI_FAILCONF()

void
Qmgr::add_failconf_block(NodeRecPtr nodePtr, Uint32 block)
{
  // Check that it does not already exists!!
  Uint32 pos = 0;
  for (; pos < NDB_ARRAY_SIZE(nodePtr.p->m_failconf_blocks); pos++)
  {
    jam();
    if (nodePtr.p->m_failconf_blocks[pos] == 0)
    {
      jam();
      break;
    }
    else if (nodePtr.p->m_failconf_blocks[pos] == block)
    {
      jam();
      break;
    }
  }

  ndbrequire(pos != NDB_ARRAY_SIZE(nodePtr.p->m_failconf_blocks));
  ndbassert(nodePtr.p->m_failconf_blocks[pos] != block);
  if (nodePtr.p->m_failconf_blocks[pos] == block)
  {
    jam();
    /**
     * Already in list!!
     */
#ifdef ERROR_INSERT
    ndbabort();
#endif
    return;
  }
  ndbrequire(nodePtr.p->m_failconf_blocks[pos] == 0);
  nodePtr.p->m_failconf_blocks[pos] = block;
}

bool
Qmgr::remove_failconf_block(NodeRecPtr nodePtr, Uint32 block)
{
  // Check that it does exists!!
  Uint32 pos = 0;
  for (; pos < NDB_ARRAY_SIZE(nodePtr.p->m_failconf_blocks); pos++)
  {
    jam();
    if (nodePtr.p->m_failconf_blocks[pos] == 0)
    {
      jam();
      break;
    }
    else if (nodePtr.p->m_failconf_blocks[pos] == block)
    {
      jam();
      break;
    }
  }

  if (pos == NDB_ARRAY_SIZE(nodePtr.p->m_failconf_blocks) ||
      nodePtr.p->m_failconf_blocks[pos] != block)
  {
    jam();
    /**
     * Not found!!
     */
    return false;
  }

  nodePtr.p->m_failconf_blocks[pos] = 0;
  for (pos++; pos < NDB_ARRAY_SIZE(nodePtr.p->m_failconf_blocks); pos++)
  {
    jam();
    nodePtr.p->m_failconf_blocks[pos - 1] = nodePtr.p->m_failconf_blocks[pos];
  }

  return true;
}

bool
Qmgr::is_empty_failconf_block(NodeRecPtr nodePtr) const
{
  return nodePtr.p->m_failconf_blocks[0] == 0;
}

void Qmgr::execNDB_FAILCONF(Signal* signal) 
{
  NodeRecPtr failedNodePtr;
  NodeRecPtr nodePtr;

  jamEntry();
  failedNodePtr.i = signal->theData[0];  

  if (ERROR_INSERTED(930))
  {
    CLEAR_ERROR_INSERT_VALUE;
    infoEvent("Discarding NDB_FAILCONF for %u", failedNodePtr.i);
    return;
  }
  
  ptrCheckGuard(failedNodePtr, MAX_NDB_NODES, nodeRec);
  if (failedNodePtr.p->failState == WAITING_FOR_NDB_FAILCONF)
  {
    g_eventLogger->info("Node %u has completed node fail handling",
                        failedNodePtr.i);
    failedNodePtr.p->failState = NORMAL;
  }
  else
  {
    jam();

    char buf[100];
    BaseString::snprintf(buf, 100, 
			 "Received NDB_FAILCONF for node %u with state: %d %d",
			 failedNodePtr.i,
			 failedNodePtr.p->phase,
			 failedNodePtr.p->failState);
    progError(__LINE__, 0, buf);
    systemErrorLab(signal, __LINE__);
  }//if

  if (cpresident == getOwnNodeId()) 
  {
    jam();
    
    CRASH_INSERTION(936);
  }

  /**
   * Reset node version only after all blocks has handled the failure
   *   so that no block risks reading 0 as node version
   */
  setNodeInfo(failedNodePtr.i).m_version = 0;
  recompute_version_info(NodeInfo::DB);

  /** 
   * Prepare a NFCompleteRep and send to all connected API's
   * They can then abort all transaction waiting for response from 
   * the failed node
   *
   * NOTE: This is sent from all nodes, as otherwise we would need
   *       take-over if cpresident dies befor sending this
   */
  NFCompleteRep * const nfComp = (NFCompleteRep *)&signal->theData[0];
  nfComp->blockNo = QMGR_REF;
  nfComp->nodeId = getOwnNodeId();
  nfComp->failedNodeId = failedNodePtr.i;
  
  jam();
  for (nodePtr.i = 1; nodePtr.i < MAX_NODES; nodePtr.i++) 
  {
    ptrAss(nodePtr, nodeRec);
    if (nodePtr.p->phase == ZAPI_ACTIVE){
      jamLine(nodePtr.i);
      sendSignal(nodePtr.p->blockRef, GSN_NF_COMPLETEREP, signal, 
                 NFCompleteRep::SignalLength, JBB);
    }//if
  }//for
  return;
}//Qmgr::execNDB_FAILCONF()

void
Qmgr::execNF_COMPLETEREP(Signal* signal)
{
  jamEntry();
  NFCompleteRep rep = *(NFCompleteRep*)signal->getDataPtr();
  if (rep.blockNo != DBTC)
  {
    jam();
    ndbassert(false);
    return;
  }

  /**
   * This is a simple way of having ndbapi to get
   * earlier information that transactions can be aborted
   */
  signal->theData[0] = rep.failedNodeId;
  NodeRecPtr nodePtr;
  for (nodePtr.i = 1; nodePtr.i < MAX_NODES; nodePtr.i++) 
  {
    ptrAss(nodePtr, nodeRec);
    if (nodePtr.p->phase == ZAPI_ACTIVE && 
        ndb_takeovertc(getNodeInfo(nodePtr.i).m_version))
    {
      jamLine(nodePtr.i);
      sendSignal(nodePtr.p->blockRef, GSN_TAKE_OVERTCCONF, signal, 
                 NFCompleteRep::SignalLength, JBB);
    }//if
  }//for
  return;
}

/*******************************/
/* DISCONNECT_REP             */
/*******************************/
const char *lookupConnectionError(Uint32 err);

void Qmgr::execDISCONNECT_REP(Signal* signal) 
{
  jamEntry();
  const DisconnectRep * const rep = (DisconnectRep *)&signal->theData[0];
  const Uint32 nodeId = rep->nodeId;
  const Uint32 err = rep->err;
  const NodeInfo nodeInfo = getNodeInfo(nodeId);
  c_connectedNodes.clear(nodeId);

  if (nodeInfo.getType() == NodeInfo::DB)
  {
    c_readnodes_nodes.clear(nodeId);

    if (ERROR_INSERTED(942))
    {
      g_eventLogger->info("DISCONNECT_REP received from data node %u - crash insertion",
                          nodeId);
      CRASH_INSERTION(942);
    }
  }
  
  NodeRecPtr nodePtr;
  nodePtr.i = getOwnNodeId();
  ptrCheckGuard(nodePtr, MAX_NODES, nodeRec);
  
  char buf[100];
  if (nodeInfo.getType() == NodeInfo::DB &&
      getNodeState().startLevel < NodeState::SL_STARTED)
  {
    jam();
    CRASH_INSERTION(932);
    CRASH_INSERTION(938);
    CRASH_INSERTION(944);
    CRASH_INSERTION(946);
    BaseString::snprintf(buf, 100, "Node %u disconnected", nodeId);    
    progError(__LINE__, NDBD_EXIT_SR_OTHERNODEFAILED, buf);
    ndbabort();
  }
  
  if (getNodeInfo(nodeId).getType() != NodeInfo::DB)
  {
    jam();
    api_failed(signal, nodeId);
    return;
  }

  switch(nodePtr.p->phase){
  case ZRUNNING:
    jam();
    break;
  case ZINIT:
    ndbabort();
  case ZSTARTING:
    progError(__LINE__, NDBD_EXIT_CONNECTION_SETUP_FAILED,
	      lookupConnectionError(err));
  case ZPREPARE_FAIL:
    ndbabort();
  case ZFAIL_CLOSING:
    ndbabort();
  case ZAPI_ACTIVATION_ONGOING:
    ndbabort();
  case ZAPI_ACTIVE:
    ndbabort();
  case ZAPI_INACTIVE:
  {
    BaseString::snprintf(buf, 100, "Node %u disconnected", nodeId);    
    progError(__LINE__, NDBD_EXIT_SR_OTHERNODEFAILED, buf);
  }
  }

  if (ERROR_INSERTED(939) && ERROR_INSERT_EXTRA == nodeId)
  {
    ndbout_c("Ignoring DISCONNECT_REP for node %u that was force disconnected",
             nodeId);
    CLEAR_ERROR_INSERT_VALUE;
    return;
  }

  node_failed(signal, nodeId);
}//DISCONNECT_REP

void Qmgr::node_failed(Signal* signal, Uint16 aFailedNode) 
{
  NodeRecPtr failedNodePtr;
  /**------------------------------------------------------------------------
   *   A COMMUNICATION LINK HAS BEEN DISCONNECTED. WE MUST TAKE SOME ACTION
   *   DUE TO THIS.
   *-----------------------------------------------------------------------*/
  failedNodePtr.i = aFailedNode;
  ptrCheckGuard(failedNodePtr, MAX_NODES, nodeRec);
  failedNodePtr.p->m_secret = 0; // Not yet Uint64(rand()) << 32 + rand();

  ndbrequire(getNodeInfo(failedNodePtr.i).getType() == NodeInfo::DB);
  
  /**---------------------------------------------------------------------
   *   THE OTHER NODE IS AN NDB NODE, WE HANDLE IT AS IF A HEARTBEAT 
   *   FAILURE WAS DISCOVERED.
   *---------------------------------------------------------------------*/
  switch(failedNodePtr.p->phase){
  case ZRUNNING:
    jam();
    failReportLab(signal, aFailedNode, FailRep::ZLINK_FAILURE, getOwnNodeId());
    return;
  case ZSTARTING:
    /**
     * bug#42422
     *   Force "real" failure handling
     */
    jam();
    failedNodePtr.p->phase = ZRUNNING;
    failReportLab(signal, aFailedNode, FailRep::ZLINK_FAILURE, getOwnNodeId());
    return;
  case ZFAIL_CLOSING:  // Close already in progress
    jam();
    return;
  case ZPREPARE_FAIL:  // PREP_FAIL already sent CLOSE_COMREQ
    jam();
    return;
  case ZINIT:
  {
    jam();
    /*---------------------------------------------------------------------*/
    // The other node is still not in the cluster but disconnected. 
    // We must restart communication in three seconds.
    /*---------------------------------------------------------------------*/
    failedNodePtr.p->failState = NORMAL;
    failedNodePtr.p->phase = ZFAIL_CLOSING;
    set_hb_count(failedNodePtr.i) = 0;

    CloseComReqConf * const closeCom = 
      (CloseComReqConf *)&signal->theData[0];

    closeCom->xxxBlockRef = reference();
    closeCom->requestType = CloseComReqConf::RT_NO_REPLY;
    closeCom->failNo      = 0;
    closeCom->noOfNodes   = 1;
    NodeBitmask::clear(closeCom->theNodes);
    NodeBitmask::set(closeCom->theNodes, failedNodePtr.i);
    sendSignal(TRPMAN_REF, GSN_CLOSE_COMREQ, signal,
               CloseComReqConf::SignalLength, JBB);
    return;
  }
  case ZAPI_ACTIVE:     // Unexpected states handled in ::api_failed()
    ndbabort();
  case ZAPI_INACTIVE:
    ndbabort();
  case ZAPI_ACTIVATION_ONGOING:
    ndbabort();
  default:
    ndbabort();  // Unhandled state
  }//switch

  return;
}

void
Qmgr::execUPGRADE_PROTOCOL_ORD(Signal* signal)
{
  const UpgradeProtocolOrd* ord = (UpgradeProtocolOrd*)signal->getDataPtr();
  switch(ord->type){
  case UpgradeProtocolOrd::UPO_ENABLE_MICRO_GCP:
    jam();
    m_micro_gcp_enabled = true;
    return;
  }
}

void
Qmgr::api_failed(Signal* signal, Uint32 nodeId)
{
  NodeRecPtr failedNodePtr;
  /**------------------------------------------------------------------------
   *   A COMMUNICATION LINK HAS BEEN DISCONNECTED. WE MUST TAKE SOME ACTION
   *   DUE TO THIS.
   *-----------------------------------------------------------------------*/
  failedNodePtr.i = nodeId;
  ptrCheckGuard(failedNodePtr, MAX_NODES, nodeRec);
  failedNodePtr.p->m_secret = 0; // Not yet Uint64(rand()) << 32 + rand();

  if (failedNodePtr.p->phase == ZFAIL_CLOSING)
  {
    /**
     * Failure handling already in progress
     */
    jam();
    return;
  }

  ndbrequire(failedNodePtr.p->failState == NORMAL);

  /* Send API_FAILREQ to peer QMGR blocks to allow them to disconnect
   * quickly
   * Local application blocks get API_FAILREQ once all pending signals
   * from the failed API have been processed.
   */
  signal->theData[0] = failedNodePtr.i;
  signal->theData[1] = QMGR_REF;
  NodeReceiverGroup rg(QMGR, c_clusterNodes);
  sendSignal(rg, GSN_API_FAILREQ, signal, 2, JBA);
  
  /* Now ask CMVMI to disconnect the node */
  FailState initialState = (failedNodePtr.p->phase == ZAPI_ACTIVE) ?
    WAITING_FOR_CLOSECOMCONF_ACTIVE : 
    WAITING_FOR_CLOSECOMCONF_NOTACTIVE;

  failedNodePtr.p->failState = initialState;
  failedNodePtr.p->phase = ZFAIL_CLOSING;
  set_hb_count(failedNodePtr.i) = 0;

  CloseComReqConf * const closeCom = (CloseComReqConf *)&signal->theData[0];
  closeCom->xxxBlockRef = reference();
  closeCom->requestType = CloseComReqConf::RT_API_FAILURE;
  closeCom->failNo      = 0;
  closeCom->noOfNodes   = 1;
  ProcessInfo * processInfo = getProcessInfo(nodeId);
  if(processInfo) processInfo->invalidate();
  NodeBitmask::clear(closeCom->theNodes);
  NodeBitmask::set(closeCom->theNodes, failedNodePtr.i);
  sendSignal(TRPMAN_REF, GSN_CLOSE_COMREQ, signal,
             CloseComReqConf::SignalLength, JBB);
} // api_failed

/**--------------------------------------------------------------------------
 * AN API NODE IS REGISTERING. IF FOR THE FIRST TIME WE WILL ENABLE 
 * COMMUNICATION WITH ALL NDB BLOCKS.
 *---------------------------------------------------------------------------*/
/*******************************/
/* API_REGREQ                 */
/*******************************/
void Qmgr::execAPI_REGREQ(Signal* signal) 
{
  jamEntry();
  
  ApiRegReq* req = (ApiRegReq*)signal->getDataPtr();
  const Uint32 version = req->version;
  const BlockReference ref = req->ref;
  
  Uint32 mysql_version = req->mysql_version;
  if (version < NDBD_SPLIT_VERSION)
    mysql_version = 0;

  NodeRecPtr apiNodePtr;
  apiNodePtr.i = refToNode(ref);
  ptrCheckGuard(apiNodePtr, MAX_NODES, nodeRec);

  if (apiNodePtr.p->phase == ZFAIL_CLOSING)
  {
    jam();
    /**
     * This node is pending CLOSE_COM_CONF
     *   ignore API_REGREQ
     */
    return;
  }

  if (!c_connectedNodes.get(apiNodePtr.i))
  {
    jam();
    /**
     * We have not yet heard execCONNECT_REP
     *   so ignore this until we do...
     */
    return;
  }

#if 0
  ndbout_c("Qmgr::execAPI_REGREQ: Recd API_REGREQ (NodeId=%d)", apiNodePtr.i);
#endif

  bool compatability_check;
  const char * extra = 0;
  NodeInfo::NodeType type= getNodeInfo(apiNodePtr.i).getType();
  switch(type){
  case NodeInfo::API:
    if (m_micro_gcp_enabled && !ndb_check_micro_gcp(version))
    {
      jam();
      compatability_check = false;
      extra = ": micro gcp enabled";
    }
    else
    {
      jam();
      compatability_check = ndbCompatible_ndb_api(NDB_VERSION, version);
    }
    break;
  case NodeInfo::MGM:
    compatability_check = ndbCompatible_ndb_mgmt(NDB_VERSION, version);
    break;
  case NodeInfo::DB:
  case NodeInfo::INVALID:
  default:
    sendApiRegRef(signal, ref, ApiRegRef::WrongType);
    infoEvent("Invalid connection attempt with type %d", type);
    return;
  }
  
  if (!compatability_check) {
    jam();
    char buf[NDB_VERSION_STRING_BUF_SZ];
    infoEvent("Connection attempt from %s id=%d with %s "
	      "incompatible with %s%s",
	      type == NodeInfo::API ? "api or mysqld" : "management server",
	      apiNodePtr.i,
	      ndbGetVersionString(version, mysql_version, 0,
                                  buf, 
                                  sizeof(buf)),
	      NDB_VERSION_STRING,
              extra ? extra : "");
    apiNodePtr.p->phase = ZAPI_INACTIVE;
    sendApiRegRef(signal, ref, ApiRegRef::UnsupportedVersion);
    return;
  }

  setNodeInfo(apiNodePtr.i).m_version = version;
  setNodeInfo(apiNodePtr.i).m_mysql_version = mysql_version;
  set_hb_count(apiNodePtr.i) = 0;

  NodeState state = getNodeState();
  if (apiNodePtr.p->phase == ZAPI_INACTIVE)
  {
    apiNodePtr.p->blockRef = ref;
    if ((state.startLevel == NodeState::SL_STARTED ||
         state.getSingleUserMode() ||
         (state.startLevel == NodeState::SL_STARTING &&
          state.starting.startPhase >= 8)))
    {
      jam();
      /**----------------------------------------------------------------------
       * THE API NODE IS REGISTERING. WE WILL ACCEPT IT BY CHANGING STATE AND
       * SENDING A CONFIRM. We set state to ZAPI_ACTIVATION_ONGOING to ensure
       * that we don't send unsolicited API_REGCONF or other things before we
       * actually fully enabled the node for communicating with the new API
       * node. It also avoids sending NODE_FAILREP, NF_COMPLETEREP and
       * TAKE_OVERTCCONF even before the API_REGCONF is sent. We will get a
       * fresh state of the nodes in API_REGCONF which is sufficient, no need
       * to update the API before the API got the initial state.
       *----------------------------------------------------------------------*/
      apiNodePtr.p->phase = ZAPI_ACTIVATION_ONGOING;
      EnableComReq *enableComReq = (EnableComReq *)signal->getDataPtrSend();
      enableComReq->m_senderRef = reference();
      enableComReq->m_senderData = ENABLE_COM_API_REGREQ;
      NodeBitmask::clear(enableComReq->m_nodeIds);
      NodeBitmask::set(enableComReq->m_nodeIds, apiNodePtr.i);
      sendSignal(TRPMAN_REF, GSN_ENABLE_COMREQ, signal,
                 EnableComReq::SignalLength, JBB);
      return;
    }
    /**
     * The node is in some kind of STOPPING state, so we send API_REGCONF even
     * though we've not enabled communication, if the API tries to send
     * anything to us anyways it will simply be ignored since only QMGR will
     * receive signals in this state. The API receives the node states, so it
     * should be able to discover what nodes that it is able to actually use.
     */
  }

  sendApiRegConf(signal, apiNodePtr.i);
}//Qmgr::execAPI_REGREQ()

void
Qmgr::handleEnableComApiRegreq(Signal *signal, Uint32 node)
{
  NodeRecPtr apiNodePtr;
  NodeInfo::NodeType type = getNodeInfo(node).getType();
  Uint32 version = getNodeInfo(node).m_version;
  recompute_version_info(type, version);

  signal->theData[0] = node;
  signal->theData[1] = version;
  NodeReceiverGroup rg(QMGR, c_clusterNodes);
  rg.m_nodes.clear(getOwnNodeId());
  sendVersionedDb(rg, GSN_NODE_VERSION_REP, signal, 2, JBB,
                  NDBD_NODE_VERSION_REP);

  signal->theData[0] = node;
  EXECUTE_DIRECT(NDBCNTR, GSN_API_START_REP, signal, 1);

  apiNodePtr.i = node;
  ptrCheckGuard(apiNodePtr, MAX_NODES, nodeRec);
  if (apiNodePtr.p->phase == ZAPI_ACTIVATION_ONGOING)
  {
    /**
     * Now we're about to send API_REGCONF to an API node, this means
     * that this node can immediately start communicating to TC, SUMA
     * and so forth. The state also indicates that the API is ready
     * to receive an unsolicited API_REGCONF when the node goes to
     * state SL_STARTED.
     */
    jam();
    apiNodePtr.p->phase = ZAPI_ACTIVE;
    sendApiRegConf(signal, node);
  }
  jam();
  /**
   * Node is no longer in state ZAPI_ACTIVATION_ONGOING, the node must
   * have failed, we can ignore sending API_REGCONF to a failed node.
   */
}

void
Qmgr::execNODE_STARTED_REP(Signal *signal)
{
  NodeRecPtr apiNodePtr;
  for (apiNodePtr.i = 1;
       apiNodePtr.i < MAX_NODES;
       apiNodePtr.i++)
  {
    ptrCheckGuard(apiNodePtr, MAX_NODES, nodeRec);
    NodeInfo::NodeType type = getNodeInfo(apiNodePtr.i).getType();
    if (type != NodeInfo::API)
    {
      /* Not an API node */
      continue;
    }
    if (!c_connectedNodes.get(apiNodePtr.i))
    {
      /* API not connected */
      continue;
    }
    if (apiNodePtr.p->phase != ZAPI_ACTIVE)
    {
      /**
       * The phase variable can be in three states for the API nodes, it can
       * be ZAPI_INACTIVE for an API node that hasn't connected, it can be
       * ZFAIL_CLOSING for an API node that recently failed and is performing
       * failure handling. It can be in the state ZAPI_ACTIVE which it enters
       * upon us receiving an API_REGREQ from the API. So at this point the
       * API is also able to receive an unsolicited API_REGCONF message.
       */
      continue;
    }
    /**
     * We will send an unsolicited API_REGCONF to the API node, this makes the
     * API node aware of our existence much faster (without it can wait up to
     * the lenght of a heartbeat DB-API period. For rolling restarts and other
     * similar actions this can easily cause the API to not have any usable
     * DB connections at all. This unsolicited response minimises this window
     * of unavailability to zero for all practical purposes.
     */
    sendApiRegConf(signal, apiNodePtr.i);
  }
}

void
Qmgr::sendApiRegConf(Signal *signal, Uint32 node)
{
  NodeRecPtr apiNodePtr;
  apiNodePtr.i = node;
  ptrCheckGuard(apiNodePtr, MAX_NODES, nodeRec);
  const BlockReference ref = apiNodePtr.p->blockRef;
  ndbassert(ref != 0);

  ApiRegConf * const apiRegConf = (ApiRegConf *)&signal->theData[0];
  apiRegConf->qmgrRef = reference();
  apiRegConf->apiHeartbeatFrequency = (chbApiDelay / 10);
  apiRegConf->version = NDB_VERSION;
  apiRegConf->mysql_version = NDB_MYSQL_VERSION_D;
  apiRegConf->nodeState = getNodeState();
  {
    NodeRecPtr nodePtr;
    nodePtr.i = getOwnNodeId();
    ptrCheckGuard(nodePtr, MAX_NDB_NODES, nodeRec);
    Uint32 dynamicId = nodePtr.p->ndynamicId;

    if(apiRegConf->nodeState.masterNodeId != getOwnNodeId()){
      jam();
      apiRegConf->nodeState.dynamicId = dynamicId;
    } else {
      apiRegConf->nodeState.dynamicId = (Uint32)(-(Int32)dynamicId);
    }
  }
  NodeVersionInfo info = getNodeVersionInfo();
  apiRegConf->minDbVersion = info.m_type[NodeInfo::DB].m_min_version;
  apiRegConf->nodeState.m_connected_nodes.assign(c_connectedNodes);
  sendSignal(ref, GSN_API_REGCONF, signal, ApiRegConf::SignalLength, JBB);
}

void
Qmgr::sendVersionedDb(NodeReceiverGroup rg,
		      GlobalSignalNumber gsn, 
		      Signal* signal, 
		      Uint32 length, 
		      JobBufferLevel jbuf,
		      Uint32 minversion)
{
  jam();
  NodeVersionInfo info = getNodeVersionInfo();
  if (info.m_type[NodeInfo::DB].m_min_version >= minversion)
  {
    jam();
    sendSignal(rg, gsn, signal, length, jbuf);
  }
  else
  {
    jam();
    Uint32 i = 0, cnt = 0;
    while((i = rg.m_nodes.find(i + 1)) != NodeBitmask::NotFound)
    {
      jam();
      if (getNodeInfo(i).m_version >= minversion)
      {
	jam();
	cnt++;
	sendSignal(numberToRef(rg.m_block, i), gsn, signal, length, jbuf);
      }
    }
    ndbassert((cnt == 0 && rg.m_nodes.count() == 0) ||
	      (cnt < rg.m_nodes.count()));
  }
}

void
Qmgr::execAPI_VERSION_REQ(Signal * signal) {
  jamEntry();
  ApiVersionReq * const req = (ApiVersionReq *)signal->getDataPtr();

  Uint32 senderRef = req->senderRef;
  Uint32 nodeId = req->nodeId;

  ApiVersionConf * conf = (ApiVersionConf *)req;
  if(getNodeInfo(nodeId).m_connected)
  {
    conf->version = getNodeInfo(nodeId).m_version;
    conf->mysql_version = getNodeInfo(nodeId).m_mysql_version;
    struct in_addr in= globalTransporterRegistry.get_connect_address(nodeId);
    conf->m_inet_addr= in.s_addr;
  }
  else
  {
    conf->version =  0;
    conf->mysql_version =  0;
    conf->m_inet_addr= 0;
  }
  conf->nodeId = nodeId;

  sendSignal(senderRef,
	     GSN_API_VERSION_CONF,
	     signal,
	     ApiVersionConf::SignalLength, JBB);
}

void
Qmgr::execNODE_VERSION_REP(Signal* signal)
{
  jamEntry();
  Uint32 nodeId = signal->theData[0];
  Uint32 version = signal->theData[1];

  if (nodeId < MAX_NODES)
  {
    jam();
    Uint32 type = getNodeInfo(nodeId).m_type;
    setNodeInfo(nodeId).m_version = version;
    recompute_version_info(type, version);
  }
}
 
void
Qmgr::recompute_version_info(Uint32 type, Uint32 version)
{
  NodeVersionInfo& info = setNodeVersionInfo();
  switch(type){
  case NodeInfo::DB:
  case NodeInfo::API:
  case NodeInfo::MGM:
    break;
  default:
    return;
  }
  
  if (info.m_type[type].m_min_version == 0 ||
      version < info.m_type[type].m_min_version)
    info.m_type[type].m_min_version = version;
  if (version > info.m_type[type].m_max_version)
    info.m_type[type].m_max_version = version;
}

void
Qmgr::recompute_version_info(Uint32 type)
{
  switch(type){
  case NodeInfo::DB:
  case NodeInfo::API:
  case NodeInfo::MGM:
    break;
  default:
    return;
  }
  
  Uint32 min = ~0, max = 0;
  Uint32 cnt = type == NodeInfo::DB ? MAX_NDB_NODES : MAX_NODES;
  for (Uint32 i = 1; i<cnt; i++)
  {
    if (getNodeInfo(i).m_type == type)
    {
      Uint32 version = getNodeInfo(i).m_version;
      
      if (version)
      {
	if (version < min)
	  min = version;
	if (version > max)
	  max = version;
      }
    }
  }
  
  NodeVersionInfo& info = setNodeVersionInfo();
  info.m_type[type].m_min_version = min == ~(Uint32)0 ? 0 : min;
  info.m_type[type].m_max_version = max;
}

#if 0
bool
Qmgr::checkAPIVersion(NodeId nodeId, 
		      Uint32 apiVersion, Uint32 ownVersion) const {
  bool ret=true;
  /**
   * First implementation...
   */
  if ((getMajor(apiVersion) < getMajor(ownVersion) ||
       getMinor(apiVersion) < getMinor(ownVersion)) &&
      apiVersion >= API_UPGRADE_VERSION) {
    jam();
    if ( getNodeInfo(nodeId).getType() !=  NodeInfo::MGM ) {
      jam();
      ret = false;
    } else {
      jam();
      /* we have a software upgrade situation, mgmtsrvr should be
       * the highest, let him decide what to do
       */
      ;
    }
  }
  return ret;
}
#endif

void
Qmgr::sendApiRegRef(Signal* signal, Uint32 Tref, ApiRegRef::ErrorCode err){
  ApiRegRef* ref = (ApiRegRef*)signal->getDataPtrSend();
  ref->ref = reference();
  ref->version = NDB_VERSION;
  ref->mysql_version = NDB_MYSQL_VERSION_D;
  ref->errorCode = err;
  sendSignal(Tref, GSN_API_REGREF, signal, ApiRegRef::SignalLength, JBB);
}

/**--------------------------------------------------------------------------
 * A NODE HAS BEEN DECLARED AS DOWN. WE WILL CLOSE THE COMMUNICATION TO THIS 
 * NODE IF NOT ALREADY DONE. IF WE ARE PRESIDENT OR BECOMES PRESIDENT BECAUSE 
 * OF A FAILED PRESIDENT THEN WE WILL TAKE FURTHER ACTION. 
 *---------------------------------------------------------------------------*/
void Qmgr::failReportLab(Signal* signal, Uint16 aFailedNode,
			 FailRep::FailCause aFailCause,
                         Uint16 sourceNode) 
{
  NodeRecPtr nodePtr;
  NodeRecPtr failedNodePtr;
  NodeRecPtr myNodePtr;

  failedNodePtr.i = aFailedNode;
  ptrCheckGuard(failedNodePtr, MAX_NDB_NODES, nodeRec);
  FailRep* rep = (FailRep*)signal->getDataPtr();

  if (check_multi_node_shutdown(signal))
  {
    jam();
    return;
  }

  if (isNodeConnectivitySuspect(sourceNode) &&
      // (! isNodeConnectivitySuspect(aFailedNode)) &&  // TODO : Required?
      ((aFailCause == FailRep::ZCONNECT_CHECK_FAILURE) ||
       (aFailCause == FailRep::ZLINK_FAILURE)))
  {
    jam();
    /* Connectivity related failure report from a node with suspect
     * connectivity, handle differently
     */
    ndbrequire(sourceNode != getOwnNodeId());

    handleFailFromSuspect(signal,
                          aFailCause,
                          aFailedNode,
                          sourceNode);
    return;
  }
  
  if (failedNodePtr.i == getOwnNodeId()) {
    jam();

    Uint32 code = NDBD_EXIT_NODE_DECLARED_DEAD;
    const char * msg = 0;
    char extra[100];
    switch(aFailCause){
    case FailRep::ZOWN_FAILURE: 
      msg = "Own failure"; 
      break;
    case FailRep::ZOTHER_NODE_WHEN_WE_START: 
    case FailRep::ZOTHERNODE_FAILED_DURING_START:
      msg = "Other node died during start"; 
      break;
    case FailRep::ZIN_PREP_FAIL_REQ:
      msg = "Prep fail";
      break;
    case FailRep::ZSTART_IN_REGREQ:
      msg = "Start timeout";
      break;
    case FailRep::ZHEARTBEAT_FAILURE:
      msg = "Heartbeat failure";
      break;
    case FailRep::ZLINK_FAILURE:
      msg = "Connection failure";
      break;
    case FailRep::ZPARTITIONED_CLUSTER:
    {
      code = NDBD_EXIT_PARTITIONED_SHUTDOWN;
      char buf1[100], buf2[100];
      c_clusterNodes.getText(buf1);
      if (((signal->getLength()== FailRep::OrigSignalLength + FailRep::PartitionedExtraLength) ||
           (signal->getLength()== FailRep::SignalLength + FailRep::PartitionedExtraLength)) &&
          signal->header.theVerId_signalNumber == GSN_FAIL_REP)
      {
	jam();
	NdbNodeBitmask part;
	part.assign(NdbNodeBitmask::Size, rep->partitioned.partition);
	part.getText(buf2);
	BaseString::snprintf(extra, sizeof(extra),
			     "Our cluster: %s other cluster: %s",
			     buf1, buf2);
      }
      else
      {
	jam();
	BaseString::snprintf(extra, sizeof(extra),
			     "Our cluster: %s", buf1);
      }
      msg = extra;
      break;
    }
    case FailRep::ZMULTI_NODE_SHUTDOWN:
      msg = "Multi node shutdown";
      break;
    case FailRep::ZCONNECT_CHECK_FAILURE:
      msg = "Connectivity check failure";
      break;
    case FailRep::ZFORCED_ISOLATION:
      msg = "Forced isolation";
      if (ERROR_INSERTED(942))
      {
        g_eventLogger->info("FAIL_REP FORCED_ISOLATION received from data node %u - ignoring.",
                            sourceNode);
        /* Let's wait for remote disconnection */
        return;
      }
      break;
    default:
      msg = "<UNKNOWN>";
    }
    
    CRASH_INSERTION(932);
    CRASH_INSERTION(938);

    char buf[255];
    BaseString::snprintf(buf, sizeof(buf), 
			 "We(%u) have been declared dead by %u (via %u) reason: %s(%u)",
			 getOwnNodeId(),
                         sourceNode,
			 refToNode(signal->getSendersBlockRef()),
			 msg ? msg : "<Unknown>",
			 aFailCause);
    
    progError(__LINE__, code, buf);
    return;
  }//if
  
  myNodePtr.i = getOwnNodeId();
  ptrCheckGuard(myNodePtr, MAX_NDB_NODES, nodeRec);
  if (myNodePtr.p->phase != ZRUNNING) {
    jam();
    systemErrorLab(signal, __LINE__);
    return;
  }//if

  if (getNodeState().startLevel < NodeState::SL_STARTED)
  {
    jam();
    CRASH_INSERTION(932);
    CRASH_INSERTION(938);
    char buf[100];
    switch(aFailCause)
    {
      case FailRep::ZHEARTBEAT_FAILURE:
        BaseString::snprintf(buf, 100 ,"Node %d heartbeat failure",
                             failedNodePtr.i);
        CRASH_INSERTION(947);
        break;
      default:
        BaseString::snprintf(buf, 100 , "Node %d failed",
                             failedNodePtr.i);
    }
    progError(__LINE__, NDBD_EXIT_SR_OTHERNODEFAILED, buf);
  }

  const NdbNodeBitmask TfailedNodes(cfailedNodes);
  failReport(signal, failedNodePtr.i, (UintR)ZTRUE, aFailCause, sourceNode);

  /**
   * If any node is starting now (c_start.startNode != 0)
   *   include it in nodes handled by sendPrepFailReq
   */
  if (c_start.m_startNode != 0)
  {
    jam();
    cfailedNodes.set(c_start.m_startNode);
  }

  if (cpresident == getOwnNodeId()) {
    jam();
    if (ctoStatus == Q_NOT_ACTIVE) {
      jam();
      /**--------------------------------------------------------------------
       * AS PRESIDENT WE ARE REQUIRED TO START THE EXCLUSION PROCESS SUCH THAT
       * THE APPLICATION SEE NODE FAILURES IN A CONSISTENT ORDER.
       * IF WE HAVE BECOME PRESIDENT NOW (CTO_STATUS = ACTIVE) THEN WE HAVE 
       * TO COMPLETE THE PREVIOUS COMMIT FAILED NODE PROCESS BEFORE STARTING 
       * A NEW.
       * CTO_STATUS = ACTIVE CAN ALSO MEAN THAT WE ARE PRESIDENT AND ARE 
       * CURRENTLY COMMITTING A SET OF NODE CRASHES. IN THIS CASE IT IS NOT 
       * ALLOWED TO START PREPARING NEW NODE CRASHES.
       *---------------------------------------------------------------------*/
      if (!cfailedNodes.equal(TfailedNodes)) {
        jam();
        cfailureNr = cfailureNr + 1;
        for (nodePtr.i = 1;
             nodePtr.i < MAX_NDB_NODES; nodePtr.i++) {
          ptrAss(nodePtr, nodeRec);
          if (nodePtr.p->phase == ZRUNNING) {
            jamLine(nodePtr.i);
            sendPrepFailReq(signal, nodePtr.i);
          }//if
        }//for
      }//if
    }//if
  }
  return;
}//Qmgr::failReportLab()

/**-------------------------------------------------------------------------
 * WE HAVE RECEIVED A PREPARE TO EXCLUDE A NUMBER OF NODES FROM THE CLUSTER.
 * WE WILL FIRST CHECK THAT WE HAVE NOT ANY MORE NODES THAT 
 * WE ALSO HAVE EXCLUDED
 *--------------------------------------------------------------------------*/
/*******************************/
/* PREP_FAILREQ               */
/*******************************/
void Qmgr::execPREP_FAILREQ(Signal* signal) 
{
  NodeRecPtr myNodePtr;
  PrepFailReqRef * const prepFail = (PrepFailReqRef *)&signal->theData[0];
  BlockReference Tblockref  = prepFail->xxxBlockRef;
  Uint16 TfailureNr = prepFail->failNo;

  jamEntry();
  
  // Clear 'c_start.m_startNode' if it failed.
  if (NdbNodeBitmask::get(prepFail->theNodes, c_start.m_startNode))
  {
    jam();
    c_start.reset();
  }
  if (c_start.m_gsn == GSN_CM_NODEINFOCONF)
  {
    Uint32 nodeId;
    jam();
    /**
     * This is a very unusual event we are looking for, but still required
     * to be handled. The starting node has connected to the president and
     * managed to start the node inclusion protocol. We received an indication
     * of this from the president. The starting node now however fails before
     * it connected to us, so we need to clear the indication of that we
     * received CM_ADD(Prepare) from president since this belonged to an
     * already cancelled node restart.
     */
    for (nodeId = 1; nodeId < MAX_NDB_NODES; nodeId++)
    {
      if (c_start.m_nodes.isWaitingFor(nodeId) &&
          NdbNodeBitmask::get(prepFail->theNodes, nodeId))
      {
        jamLine(nodeId);
        /* Found such a condition as described above, clear state */
        c_start.m_gsn = RNIL;
        c_start.m_nodes.clearWaitingFor();
        break;
      }
    }
  }
      
  
  if (check_multi_node_shutdown(signal))
  {
    jam();
    return;
  }

  if (ERROR_INSERTED(941) &&
      getOwnNodeId() == 4 &&
      NdbNodeBitmask::get(prepFail->theNodes, 2))
  {
    /* Insert ERROR_INSERT crash */
    CRASH_INSERTION(941);
  }

  cprepFailedNodes.assign(NdbNodeBitmask::Size, prepFail->theNodes);
  ndbassert(prepFail->noOfNodes == cprepFailedNodes.count());

  /**
   * Block commit until node failures has stabilized
   *
   * @See RT352
   */
  BlockCommitOrd* const block = (BlockCommitOrd *)&signal->theData[0];
  block->failNo = TfailureNr;
  EXECUTE_DIRECT(DBDIH, GSN_BLOCK_COMMIT_ORD, signal, 
		 BlockCommitOrd::SignalLength);

  myNodePtr.i = getOwnNodeId();
  ptrCheckGuard(myNodePtr, MAX_NDB_NODES, nodeRec);
  if (myNodePtr.p->phase != ZRUNNING) {
    jam();
    systemErrorLab(signal, __LINE__);
    return;
  }//if

  if (getNodeState().startLevel < NodeState::SL_STARTED)
  {
    jam();
    CRASH_INSERTION(932);
    CRASH_INSERTION(938);
    char buf[100];
    BaseString::snprintf(buf, 100, "Node failure during restart");
    progError(__LINE__, NDBD_EXIT_SR_OTHERNODEFAILED, buf);
  }

  for (unsigned nodeId = 1; nodeId < MAX_NDB_NODES; nodeId++)
  {
    if (cprepFailedNodes.get(nodeId))
    {
      jam();
      failReport(signal,
                 nodeId,
                 (UintR)ZFALSE,
                 FailRep::ZIN_PREP_FAIL_REQ,
                 0); /* Source node not required (or known) here */
    }//if
  }//for
  sendCloseComReq(signal, Tblockref, TfailureNr);
  ccommitFailedNodes.clear();
  cprepareFailureNr = TfailureNr;
  return;
}//Qmgr::execPREP_FAILREQ()


void Qmgr::handleApiCloseComConf(Signal* signal)
{
  jam();
  CloseComReqConf * const closeCom = (CloseComReqConf *)&signal->theData[0];

  /* Api failure special case */
  for(Uint32 nodeId = 0; nodeId < MAX_NODES; nodeId ++)
  {
    if (NodeBitmask::get(closeCom->theNodes, nodeId))
    {
      jam();
      /* Check that *only* 1 *API* node is included in
       * this CLOSE_COM_CONF
       */
      ndbrequire(getNodeInfo(nodeId).getType() != NodeInfo::DB);
      ndbrequire(closeCom->noOfNodes == 1);
      NodeBitmask::clear(closeCom->theNodes, nodeId);
      ndbrequire(NodeBitmask::isclear(closeCom->theNodes));
      
      /* Now that we know communication from the failed Api has
       * ceased, we can send the required API_FAILREQ signals
       * and continue API failure handling
       */
      NodeRecPtr failedNodePtr;
      failedNodePtr.i = nodeId;
      ptrCheckGuard(failedNodePtr, MAX_NODES, nodeRec);
      
      ndbrequire((failedNodePtr.p->failState == 
                  WAITING_FOR_CLOSECOMCONF_ACTIVE) ||
                 (failedNodePtr.p->failState ==
                  WAITING_FOR_CLOSECOMCONF_NOTACTIVE));
      
      if (failedNodePtr.p->failState == WAITING_FOR_CLOSECOMCONF_ACTIVE)
      {
        /**
         * Inform application blocks TC, DICT, SUMA etc.
         */
        jam();
        sendApiFailReq(signal, nodeId, false); // !sumaOnly
        if(arbitRec.node == nodeId)
        {
          arbitRec.code = ArbitCode::ApiFail;
          handleArbitApiFail(signal, nodeId);
        }
      }
      else
      {
        /**
         * Always inform SUMA
         */
        jam();
        sendApiFailReq(signal, nodeId, true); // sumaOnly
      }
      
      if (getNodeInfo(failedNodePtr.i).getType() == NodeInfo::MGM)
      {
        /**
         * Allow MGM do reconnect "directly"
         */
        jam();
        set_hb_count(failedNodePtr.i) = 3;
      }
      
      /* Handled the single API node failure */
      return;
    }
  }
  /* Never get here */
  ndbabort();
}

/**---------------------------------------------------------------------------
 * THE CRASHED NODES HAS BEEN EXCLUDED FROM COMMUNICATION. 
 * WE WILL CHECK WHETHER ANY MORE NODES HAVE FAILED DURING THE PREPARE PROCESS.
 * IF SO WE WILL REFUSE THE PREPARE PHASE AND EXPECT A NEW PREPARE MESSAGE 
 * WITH ALL FAILED NODES INCLUDED.
 *---------------------------------------------------------------------------*/
/*******************************/
/* CLOSE_COMCONF              */
/*******************************/
void Qmgr::execCLOSE_COMCONF(Signal* signal) 
{
  jamEntry();

  CloseComReqConf * const closeCom = (CloseComReqConf *)&signal->theData[0];

  Uint32 requestType = closeCom->requestType;

  if (requestType == CloseComReqConf::RT_API_FAILURE)
  {
    jam();
    if (ERROR_INSERTED(945))
    {
      if (arbitRec.code != ArbitCode::WinChoose)
      {
        // Delay API failure handling until arbitration in WinChoose
        sendSignalWithDelay(reference(),
                            GSN_CLOSE_COMCONF,
                            signal,
                            10,
                            signal->getLength());
        return;
      }
      CLEAR_ERROR_INSERT_VALUE;
    }
    handleApiCloseComConf(signal);
    return;
  }

  /* Normal node failure preparation path */
  ndbassert(requestType == CloseComReqConf::RT_NODE_FAILURE);
  BlockReference Tblockref  = closeCom->xxxBlockRef;
  Uint16 TfailureNr = closeCom->failNo;

  cprepFailedNodes.assign(NdbNodeBitmask::Size, closeCom->theNodes);
  ndbassert(closeCom->noOfNodes == cprepFailedNodes.count());

  UintR tprepFailConf = ZTRUE;

  /* Check whether the set of nodes which have had communications
   * closed is the same as the set of failed nodes.
   * If it is, we can confirm the PREP_FAIL phase for this set 
   * of nodes to the President.
   * If it is not, we Refuse the PREP_FAIL phase for this set
   * of nodes, the President will start a new PREP_FAIL phase
   * for the new set.
   */
  if (!cprepFailedNodes.contains(cfailedNodes)) {
    /* Failed node(s) is missing from the set, we will not
     * confirm this Prepare_Fail phase.
     * Store the node id in the array for later.
     */
    jam();
    tprepFailConf = ZFALSE;
    cprepFailedNodes.bitOR(cfailedNodes);
  }//if
  if (tprepFailConf == ZFALSE) {
    jam();
    /* Inform President that we cannot confirm the PREP_FAIL
     * phase as we are aware of at least one other node
     * failure
     */
    cfailedNodes = cprepFailedNodes;

    sendPrepFailReqRef(signal,
		       Tblockref,
		       GSN_PREP_FAILREF,
		       reference(),
		       cfailureNr,
		       cprepFailedNodes);
  } else {
    /* We have prepared the failure of the requested nodes
     * send confirmation to the president
     */
    jam();
    ccommitFailedNodes = cprepFailedNodes;

    signal->theData[0] = getOwnNodeId();
    signal->theData[1] = TfailureNr;
    sendSignal(Tblockref, GSN_PREP_FAILCONF, signal, 2, JBA);
  }//if
  return;
}//Qmgr::execCLOSE_COMCONF()

/*---------------------------------------------------------------------------*/
/* WE HAVE RECEIVED A CONFIRM OF THAT THIS NODE HAVE PREPARED THE FAILURE.   */
/*---------------------------------------------------------------------------*/
/*******************************/
/* PREP_FAILCONF              */
/*******************************/
void Qmgr::execPREP_FAILCONF(Signal* signal) 
{
  NodeRecPtr nodePtr;
  NodeRecPtr replyNodePtr;
  jamEntry();
  replyNodePtr.i = signal->theData[0];
  Uint16 TfailureNr = signal->theData[1];
  if (TfailureNr != cfailureNr) {
    jam();
    /**----------------------------------------------------------------------
     * WE HAVE ALREADY STARTING A NEW ATTEMPT TO EXCLUDE A NUMBER OF NODES. 
     *  IGNORE
     *----------------------------------------------------------------------*/
    return;
  }//if
  ptrCheckGuard(replyNodePtr, MAX_NDB_NODES, nodeRec);
  replyNodePtr.p->sendPrepFailReqStatus = Q_NOT_ACTIVE;
  for (nodePtr.i = 1; nodePtr.i < MAX_NDB_NODES; nodePtr.i++) {
    ptrAss(nodePtr, nodeRec);
    if (nodePtr.p->phase == ZRUNNING) {
      if (nodePtr.p->sendPrepFailReqStatus == Q_ACTIVE) {
        jamLine(nodePtr.i);
        return;
      }//if
    }//if
  }//for
  /**
   * Check node count and groups and invoke arbitrator if necessary.
   * Continues via sendCommitFailReq() if successful.
   */
  arbitRec.failureNr = cfailureNr;
  const NodeState & s = getNodeState();
  if(s.startLevel == NodeState::SL_STOPPING_3 && s.stopping.systemShutdown){
    jam();
    /**
     * We're performing a system shutdown, 
     * don't let artibtrator shut us down
     */
    return;
  }

  switch(arbitRec.method){
  case ArbitRec::DISABLED:
    jam();
    // No arbitration -> immediately commit the failed nodes
    sendCommitFailReq(signal);
    break;

  case ArbitRec::METHOD_EXTERNAL:
  case ArbitRec::METHOD_DEFAULT:
    jam();
    handleArbitCheck(signal);
    break;

  }
  return;
}//Qmgr::execPREP_FAILCONF()

void
Qmgr::sendCommitFailReq(Signal* signal)
{
  NodeRecPtr nodePtr;
  jam();
  if (arbitRec.failureNr != cfailureNr) {
    jam();
    /**----------------------------------------------------------------------
     * WE HAVE ALREADY STARTING A NEW ATTEMPT TO EXCLUDE A NUMBER OF NODES. 
     *  IGNORE
     *----------------------------------------------------------------------*/
    return;
  }//if
  /**-----------------------------------------------------------------------
   * WE HAVE SUCCESSFULLY PREPARED A SET OF NODE FAILURES. WE WILL NOW COMMIT 
   * THESE NODE FAILURES.
   *-------------------------------------------------------------------------*/
  for (nodePtr.i = 1; nodePtr.i < MAX_NDB_NODES; nodePtr.i++) {
    ptrAss(nodePtr, nodeRec);

#ifdef ERROR_INSERT    
    if (false && ERROR_INSERTED(935) && nodePtr.i == c_error_insert_extra)
    {
      ndbout_c("skipping node %d", c_error_insert_extra);
      CLEAR_ERROR_INSERT_VALUE;
      signal->theData[0] = 9999;
      sendSignalWithDelay(CMVMI_REF, GSN_NDB_TAMPER, signal, 1000, 1);
      continue;
    }
#endif

    if (nodePtr.p->phase == ZRUNNING) {
      jamLine(nodePtr.i);
      nodePtr.p->sendCommitFailReqStatus = Q_ACTIVE;
      signal->theData[0] = cpdistref;
      signal->theData[1] = cfailureNr;
      sendSignal(nodePtr.p->blockRef, GSN_COMMIT_FAILREQ, signal, 2, JBA);
    }//if
  }//for
  ctoStatus = Q_ACTIVE;
  cfailedNodes.clear();
  return;
}//sendCommitFailReq()

/*---------------------------------------------------------------------------*/
/* SOME NODE HAVE DISCOVERED A NODE FAILURE THAT WE HAVE NOT YET DISCOVERED. */
/* WE WILL START ANOTHER ROUND OF PREPARING A SET OF NODE FAILURES.          */
/*---------------------------------------------------------------------------*/
/*******************************/
/* PREP_FAILREF               */
/*******************************/
void Qmgr::execPREP_FAILREF(Signal* signal) 
{
  NodeRecPtr nodePtr;
  jamEntry();

  PrepFailReqRef * const prepFail = (PrepFailReqRef *)&signal->theData[0];

  Uint16 TfailureNr = prepFail->failNo;

  cprepFailedNodes.assign(NdbNodeBitmask::Size, prepFail->theNodes);
  ndbassert(prepFail->noOfNodes == cprepFailedNodes.count());

  if (TfailureNr != cfailureNr) {
    jam();
    /**---------------------------------------------------------------------
     * WE HAVE ALREADY STARTING A NEW ATTEMPT TO EXCLUDE A NUMBER OF NODES. 
     *  IGNORE
     *----------------------------------------------------------------------*/
    return;
  }//if

  cfailedNodes = cprepFailedNodes;

  cfailureNr = cfailureNr + 1;
  for (nodePtr.i = 1; nodePtr.i < MAX_NDB_NODES; nodePtr.i++) {
    ptrAss(nodePtr, nodeRec);
    if (nodePtr.p->phase == ZRUNNING) {
      jamLine(nodePtr.i);
      sendPrepFailReq(signal, nodePtr.i);
    }//if
  }//for
  return;
}//Qmgr::execPREP_FAILREF()

/*---------------------------------------------------------------------------*/
/*    THE PRESIDENT IS NOW COMMITTING THE PREVIOUSLY PREPARED NODE FAILURE.  */
/*---------------------------------------------------------------------------*/
/***********************/
/* COMMIT_FAILREQ     */
/***********************/
void Qmgr::execCOMMIT_FAILREQ(Signal* signal) 
{
  NodeRecPtr nodePtr;
  jamEntry();

  CRASH_INSERTION(935);

  BlockReference Tblockref = signal->theData[0];
  UintR TfailureNr = signal->theData[1];
  if (Tblockref != cpdistref) {
    jam();
    return;
  }//if

  /**
   * Block commit until node failures has stabilized
   *
   * @See RT352
   */
  UnblockCommitOrd* const unblock = (UnblockCommitOrd *)&signal->theData[0];
  unblock->failNo = TfailureNr;
  EXECUTE_DIRECT(DBDIH, GSN_UNBLOCK_COMMIT_ORD, signal, 
		 UnblockCommitOrd::SignalLength);
  
  if ((ccommitFailureNr != TfailureNr) &&
      (!ccommitFailedNodes.isclear()))
  {
    jam();
    /**-----------------------------------------------------------------------
     * WE ONLY DO THIS PART OF THE COMMIT HANDLING THE FIRST TIME WE HEAR THIS
     * SIGNAL. WE CAN HEAR IT SEVERAL TIMES IF THE PRESIDENTS KEEP FAILING.
     *-----------------------------------------------------------------------*/
    ccommitFailureNr = TfailureNr;
    NodeFailRep * const nodeFail = (NodeFailRep *)&signal->theData[0];
    
    nodeFail->failNo    = ccommitFailureNr;
    nodeFail->masterNodeId = cpresident;
    nodeFail->noOfNodes = ccommitFailedNodes.count();
    ccommitFailedNodes.copyto(NdbNodeBitmask::Size, nodeFail->theNodes);

    if (ERROR_INSERTED(936))
    {
      sendSignalWithDelay(NDBCNTR_REF, GSN_NODE_FAILREP, signal, 
                          200, NodeFailRep::SignalLength);
    }
    else
    {
      sendSignal(NDBCNTR_REF, GSN_NODE_FAILREP, signal, 
                 NodeFailRep::SignalLength, JBB);
    }

    /**--------------------------------------------------------------------
     * WE MUST PREPARE TO ACCEPT THE CRASHED NODE INTO THE CLUSTER AGAIN BY 
     * SETTING UP CONNECTIONS AGAIN AFTER THREE SECONDS OF DELAY.
     *--------------------------------------------------------------------*/
    for (nodePtr.i = 1; nodePtr.i < MAX_NDB_NODES; nodePtr.i++) {
      if (ccommitFailedNodes.get(nodePtr.i)) {
        jamLine(nodePtr.i);
        ptrCheckGuard(nodePtr, MAX_NDB_NODES, nodeRec);
        nodePtr.p->phase = ZFAIL_CLOSING;
        nodePtr.p->failState = WAITING_FOR_NDB_FAILCONF;
        set_hb_count(nodePtr.i) = 0;
        c_clusterNodes.clear(nodePtr.i);
      }//if
    }//for

    /*----------------------------------------------------------------------*/
    /*       WE INFORM THE API'S WE HAVE CONNECTED ABOUT THE FAILED NODES.  */
    /*----------------------------------------------------------------------*/
    for (nodePtr.i = 1; nodePtr.i < MAX_NODES; nodePtr.i++) {
      ptrAss(nodePtr, nodeRec);
      if (nodePtr.p->phase == ZAPI_ACTIVE) {
        jamLine(nodePtr.i);

	NodeFailRep * const nodeFail = (NodeFailRep *)&signal->theData[0];

	nodeFail->failNo    = ccommitFailureNr;
	nodeFail->noOfNodes = ccommitFailedNodes.count();
	ccommitFailedNodes.copyto(NdbNodeBitmask::Size, nodeFail->theNodes);

        sendSignal(nodePtr.p->blockRef, GSN_NODE_FAILREP, signal, 
		   NodeFailRep::SignalLength, JBB);
      }//if
    }//for

    /**
     * Remove committed nodes from failed/prepared
     */
    cfailedNodes.bitANDC(ccommitFailedNodes);
    cprepFailedNodes.bitANDC(ccommitFailedNodes);
    ccommitFailedNodes.clear();
  }//if
  /**-----------------------------------------------------------------------
   * WE WILL ALWAYS ACKNOWLEDGE THE COMMIT EVEN WHEN RECEIVING IT MULTIPLE 
   * TIMES SINCE IT WILL ALWAYS COME FROM A NEW PRESIDENT. 
   *------------------------------------------------------------------------*/
  signal->theData[0] = getOwnNodeId();
  sendSignal(Tblockref, GSN_COMMIT_FAILCONF, signal, 1, JBA);
  return;
}//Qmgr::execCOMMIT_FAILREQ()

/*--------------------------------------------------------------------------*/
/* WE HAVE RECEIVED A CONFIRM OF THAT THIS NODE HAVE COMMITTED THE FAILURES.*/
/*--------------------------------------------------------------------------*/
/*******************************/
/* COMMIT_FAILCONF            */
/*******************************/
void Qmgr::execCOMMIT_FAILCONF(Signal* signal) 
{
  NodeRecPtr nodePtr;
  NodeRecPtr replyNodePtr;
  jamEntry();
  replyNodePtr.i = signal->theData[0];

  ptrCheckGuard(replyNodePtr, MAX_NDB_NODES, nodeRec);
  replyNodePtr.p->sendCommitFailReqStatus = Q_NOT_ACTIVE;
  for (nodePtr.i = 1; nodePtr.i < MAX_NDB_NODES; nodePtr.i++) {
    ptrAss(nodePtr, nodeRec);
    if (nodePtr.p->phase == ZRUNNING) {
      if (nodePtr.p->sendCommitFailReqStatus == Q_ACTIVE) {
        jamLine(nodePtr.i);
        return;
      }//if
    }//if
  }//for
  /*-----------------------------------------------------------------------*/
  /*   WE HAVE SUCCESSFULLY COMMITTED A SET OF NODE FAILURES.              */
  /*-----------------------------------------------------------------------*/
  ctoStatus = Q_NOT_ACTIVE;
  if (!cfailedNodes.isclear()) {
    jam();
    /**----------------------------------------------------------------------
     *	A FAILURE OCCURRED IN THE MIDDLE OF THE COMMIT PROCESS. WE ARE NOW 
     *  READY TO START THE FAILED NODE PROCESS FOR THIS NODE.
     *----------------------------------------------------------------------*/
    cfailureNr = cfailureNr + 1;
    for (nodePtr.i = 1; nodePtr.i < MAX_NDB_NODES; nodePtr.i++) {
      ptrAss(nodePtr, nodeRec);
      if (nodePtr.p->phase == ZRUNNING) {
        jamLine(nodePtr.i);
        sendPrepFailReq(signal, nodePtr.i);
      }//if
    }//for
  }//if
  return;
}//Qmgr::execCOMMIT_FAILCONF()

/**--------------------------------------------------------------------------
 * IF THE PRESIDENT FAILS IN THE MIDDLE OF THE COMMIT OF A FAILED NODE THEN 
 * THE NEW PRESIDENT NEEDS TO QUERY THE COMMIT STATUS IN THE RUNNING NODES.
 *---------------------------------------------------------------------------*/
/*******************************/
/* PRES_TOCONF                */
/*******************************/
void Qmgr::execPRES_TOCONF(Signal* signal) 
{
  NodeRecPtr nodePtr;
  NodeRecPtr replyNodePtr;
  jamEntry();
  replyNodePtr.i = signal->theData[0];
  UintR TfailureNr = signal->theData[1];
  if (ctoFailureNr < TfailureNr) {
    jam();
    ctoFailureNr = TfailureNr;
  }//if
  ptrCheckGuard(replyNodePtr, MAX_NDB_NODES, nodeRec);
  replyNodePtr.p->sendPresToStatus = Q_NOT_ACTIVE;
  for (nodePtr.i = 1; nodePtr.i < MAX_NDB_NODES; nodePtr.i++) {
    ptrAss(nodePtr, nodeRec);
    if (nodePtr.p->sendPresToStatus == Q_ACTIVE) {
      jamLine(nodePtr.i);
      return;
    }//if
  }//for
  /*-------------------------------------------------------------------------*/
  /* WE ARE NOW READY TO DISCOVER WHETHER THE FAILURE WAS COMMITTED OR NOT.  */
  /*-------------------------------------------------------------------------*/
  if (ctoFailureNr > ccommitFailureNr) {
    jam();
    for (nodePtr.i = 1; nodePtr.i < MAX_NDB_NODES; nodePtr.i++) {
      ptrAss(nodePtr, nodeRec);
      if (nodePtr.p->phase == ZRUNNING) {
        jamLine(nodePtr.i);
        nodePtr.p->sendCommitFailReqStatus = Q_ACTIVE;
        signal->theData[0] = cpdistref;
        signal->theData[1] = ctoFailureNr;
        sendSignal(nodePtr.p->blockRef, GSN_COMMIT_FAILREQ, signal, 2, JBA);
      }//if
    }//for
    return;
  }//if
  /*-------------------------------------------------------------------------*/
  /*       WE ARE NOW READY TO START THE NEW NODE FAILURE PROCESS.           */
  /*-------------------------------------------------------------------------*/
  ctoStatus = Q_NOT_ACTIVE;
  cfailureNr = cfailureNr + 1;
  for (nodePtr.i = 1; nodePtr.i < MAX_NDB_NODES; nodePtr.i++) {
    ptrAss(nodePtr, nodeRec);
    if (nodePtr.p->phase == ZRUNNING) {
      jamLine(nodePtr.i);
      sendPrepFailReq(signal, nodePtr.i);
    }//if
  }//for
  return;
}//Qmgr::execPRES_TOCONF()

/*--------------------------------------------------------------------------*/
// Provide information about the configured NDB nodes in the system.
/*--------------------------------------------------------------------------*/
void Qmgr::execREAD_NODESREQ(Signal* signal)
{
  jamEntry();

  BlockReference TBref = signal->theData[0];

  ReadNodesConf * const readNodes = (ReadNodesConf *)&signal->theData[0];

  NodeRecPtr nodePtr;
  nodePtr.i = getOwnNodeId();
  ptrCheckGuard(nodePtr, MAX_NDB_NODES, nodeRec);

  NdbNodeBitmask tmp = c_definedNodes;
  tmp.bitANDC(c_clusterNodes);

  readNodes->noOfNodes = c_definedNodes.count();
  readNodes->masterNodeId = cpresident;
  readNodes->ndynamicId = nodePtr.p->ndynamicId;
  c_definedNodes.copyto(NdbNodeBitmask::Size, readNodes->definedNodes);
  c_clusterNodes.copyto(NdbNodeBitmask::Size, readNodes->clusterNodes);
  tmp.copyto(NdbNodeBitmask::Size, readNodes->inactiveNodes);
  NdbNodeBitmask::clear(readNodes->startingNodes);
  NdbNodeBitmask::clear(readNodes->startedNodes);

  sendSignal(TBref, GSN_READ_NODESCONF, signal, 
	     ReadNodesConf::SignalLength, JBB);
}//Qmgr::execREAD_NODESREQ()

void Qmgr::systemErrorBecauseOtherNodeFailed(Signal* signal, Uint32 line,
					     NodeId failedNodeId) {
  jam();

  // Broadcast that this node is failing to other nodes
  failReport(signal, getOwnNodeId(), (UintR)ZTRUE, FailRep::ZOWN_FAILURE, getOwnNodeId());

  char buf[100];
  BaseString::snprintf(buf, 100, 
	   "Node was shutdown during startup because node %d failed",
	   failedNodeId);

  progError(line, NDBD_EXIT_SR_OTHERNODEFAILED, buf);  
}


void Qmgr::systemErrorLab(Signal* signal, Uint32 line, const char * message) 
{
  jam();
  // Broadcast that this node is failing to other nodes
  failReport(signal, getOwnNodeId(), (UintR)ZTRUE, FailRep::ZOWN_FAILURE, getOwnNodeId());

  // If it's known why shutdown occured
  // an error message has been passed to this function
  progError(line, NDBD_EXIT_NDBREQUIRE, message);  
}//Qmgr::systemErrorLab()


/**---------------------------------------------------------------------------
 * A FAILURE HAVE BEEN DISCOVERED ON A NODE. WE NEED TO CLEAR A 
 * NUMBER OF VARIABLES.
 *---------------------------------------------------------------------------*/
void Qmgr::failReport(Signal* signal,
                      Uint16 aFailedNode,
                      UintR aSendFailRep,
                      FailRep::FailCause aFailCause,
                      Uint16 sourceNode) 
{
  UintR tfrMinDynamicId;
  NodeRecPtr failedNodePtr;
  NodeRecPtr nodePtr;
  NodeRecPtr presidentNodePtr;


  ndbassert((! aSendFailRep) || (sourceNode != 0));

  failedNodePtr.i = aFailedNode;
  ptrCheckGuard(failedNodePtr, MAX_NDB_NODES, nodeRec);
  if (failedNodePtr.p->phase == ZRUNNING) {
    jam();

#ifdef ERROR_INSERT
    if (ERROR_INSERTED(938))
    {
      nodeFailCount++;
      ndbout_c("QMGR : execFAIL_REP(Failed : %u Source : %u  Cause : %u) : "
               "%u nodes have failed", 
               aFailedNode, sourceNode, aFailCause, nodeFailCount);
      /* Count DB nodes */
      Uint32 nodeCount = 0;
      for (Uint32 i = 1; i < MAX_NDB_NODES; i++)
      {
        if (getNodeInfo(i).getType() == NODE_TYPE_DB)
          nodeCount++;
      }

      /* When > 25% of cluster has failed, resume communications */
      if (nodeFailCount > (nodeCount / 4))
      {
        ndbout_c("QMGR : execFAIL_REP > 25%% nodes failed, resuming comms");
        Signal save = *signal;
        signal->theData[0] = 9991;
        sendSignal(CMVMI_REF, GSN_DUMP_STATE_ORD, signal, 1, JBB);
        *signal = save;
        nodeFailCount = 0;
        SET_ERROR_INSERT_VALUE(932);
      }
    }
#endif

/* WE ALSO NEED TO ADD HERE SOME CODE THAT GETS OUR NEW NEIGHBOURS. */
    if (cpresident == getOwnNodeId()) {
      jam();
      if (failedNodePtr.p->sendCommitFailReqStatus == Q_ACTIVE) {
        jam();
        signal->theData[0] = failedNodePtr.i;
        sendSignal(QMGR_REF, GSN_COMMIT_FAILCONF, signal, 1, JBA);
      }//if
      if (failedNodePtr.p->sendPresToStatus == Q_ACTIVE) {
        jam();
        signal->theData[0] = failedNodePtr.i;
        signal->theData[1] = ccommitFailureNr;
        sendSignal(QMGR_REF, GSN_PRES_TOCONF, signal, 2, JBA);
      }//if
    }//if
    failedNodePtr.p->phase = ZPREPARE_FAIL;
    failedNodePtr.p->sendPrepFailReqStatus = Q_NOT_ACTIVE;
    failedNodePtr.p->sendCommitFailReqStatus = Q_NOT_ACTIVE;
    failedNodePtr.p->sendPresToStatus = Q_NOT_ACTIVE;
    set_hb_count(failedNodePtr.i) = 0;
    if (aSendFailRep == ZTRUE) {
      jam();
      if (failedNodePtr.i != getOwnNodeId()) {
        jam();
	FailRep * const failRep = (FailRep *)&signal->theData[0];
        failRep->failNodeId = failedNodePtr.i;
        failRep->failCause = aFailCause;
        failRep->failSourceNodeId = sourceNode;
        sendSignal(failedNodePtr.p->blockRef, GSN_FAIL_REP, signal, 
		   FailRep::SignalLength, JBA);
      }//if
      for (nodePtr.i = 1; nodePtr.i < MAX_NDB_NODES; nodePtr.i++) {
        ptrAss(nodePtr, nodeRec);
        if (nodePtr.p->phase == ZRUNNING) {
          jamLine(nodePtr.i);
	  FailRep * const failRep = (FailRep *)&signal->theData[0];
	  failRep->failNodeId = failedNodePtr.i;
	  failRep->failCause = aFailCause;
          failRep->failSourceNodeId = sourceNode;
          sendSignal(nodePtr.p->blockRef, GSN_FAIL_REP, signal, 
		     FailRep::SignalLength, JBA);
        }//if
      }//for
    }//if
    if (failedNodePtr.i == getOwnNodeId()) {
      jam();
      return;
    }//if

    if (unlikely(m_connectivity_check.reportNodeFailure(failedNodePtr.i)))
    {
      jam();
      connectivityCheckCompleted(signal);
    }

    failedNodePtr.p->ndynamicId = 0;
    findNeighbours(signal, __LINE__);
    if (failedNodePtr.i == cpresident) {
      jam();
      /**--------------------------------------------------------------------
       * IF PRESIDENT HAVE FAILED WE MUST CALCULATE THE NEW PRESIDENT BY 
       * FINDING THE NODE WITH THE MINIMUM DYNAMIC IDENTITY.
       *---------------------------------------------------------------------*/
      tfrMinDynamicId = (UintR)-1;
      for (nodePtr.i = 1; nodePtr.i < MAX_NDB_NODES; nodePtr.i++) {
        ptrAss(nodePtr, nodeRec);
        if (nodePtr.p->phase == ZRUNNING) {
          jamLine(nodePtr.i);
          if ((nodePtr.p->ndynamicId & 0xFFFF) < tfrMinDynamicId) {
            jam();
            tfrMinDynamicId = (nodePtr.p->ndynamicId & 0xFFFF);
            cpresident = nodePtr.i;
          }//if
        }//if
      }//for
      presidentNodePtr.i = cpresident;
      ptrCheckGuard(presidentNodePtr, MAX_NDB_NODES, nodeRec);
      cpdistref = presidentNodePtr.p->blockRef;
      if (cpresident == getOwnNodeId()) {
	CRASH_INSERTION(920);
        cfailureNr = cprepareFailureNr;
        ctoFailureNr = 0;
        ctoStatus = Q_ACTIVE;
	c_start.reset(); // Don't take over nodes being started
        if (!ccommitFailedNodes.isclear()) {
          jam();
	  /**-----------------------------------------------------------------
	   * IN THIS SITUATION WE ARE UNCERTAIN OF WHETHER THE NODE FAILURE 
	   * PROCESS WAS COMMITTED. WE NEED TO QUERY THE OTHER NODES ABOUT 
	   * THEIR STATUS.
	   *-----------------------------------------------------------------*/
          for (nodePtr.i = 1; nodePtr.i < MAX_NDB_NODES; 
	       nodePtr.i++) {
            jam();
            ptrAss(nodePtr, nodeRec);
            if (nodePtr.p->phase == ZRUNNING) {
              jam();
              nodePtr.p->sendPresToStatus = Q_ACTIVE;
              signal->theData[0] = cpdistref;
              signal->theData[1] = cprepareFailureNr;
              sendSignal(nodePtr.p->blockRef, GSN_PRES_TOREQ, 
			 signal, 1, JBA);
            }//if
          }//for
        } else {
          jam();
	  /*-----------------------------------------------------------------*/
	  // In this case it could be that a commit process is still ongoing. 
	  // If so we must conclude it as the new master.
	  /*-----------------------------------------------------------------*/
          for (nodePtr.i = 1; nodePtr.i < MAX_NDB_NODES; 
	       nodePtr.i++) {
            ptrAss(nodePtr, nodeRec);
            if (nodePtr.p->phase == ZRUNNING) {
              jamLine(nodePtr.i);
              nodePtr.p->sendCommitFailReqStatus = Q_ACTIVE;
              signal->theData[0] = cpdistref;
              signal->theData[1] = ccommitFailureNr;
              sendSignal(nodePtr.p->blockRef, GSN_COMMIT_FAILREQ, signal, 
			 2, JBA);
            }//if
          }//for
        }//if
      }//if
    }//if
    cfailedNodes.set(failedNodePtr.i);
  }//if
}//Qmgr::failReport()

/*---------------------------------------------------------------------------*/
/*       INPUT:  TTDI_DYN_ID                                                 */
/*       OUTPUT: TTDI_NODE_ID                                                */
/*---------------------------------------------------------------------------*/
Uint16 Qmgr::translateDynamicIdToNodeId(Signal* signal, UintR TdynamicId) 
{
  NodeRecPtr tdiNodePtr;
  Uint16 TtdiNodeId = ZNIL;

  for (tdiNodePtr.i = 1; tdiNodePtr.i < MAX_NDB_NODES; tdiNodePtr.i++) {
    jam();
    ptrAss(tdiNodePtr, nodeRec);
    if (tdiNodePtr.p->ndynamicId == TdynamicId) {
      jam();
      TtdiNodeId = tdiNodePtr.i;
      break;
    }//if
  }//for
  if (TtdiNodeId == ZNIL) {
    jam();
    systemErrorLab(signal, __LINE__);
  }//if
  return TtdiNodeId;
}//Qmgr::translateDynamicIdToNodeId()

/**--------------------------------------------------------------------------
 *       WHEN RECEIVING PREPARE FAILURE REQUEST WE WILL IMMEDIATELY CLOSE
 *       COMMUNICATION WITH ALL THOSE NODES.
 *--------------------------------------------------------------------------*/
void Qmgr::sendCloseComReq(Signal* signal, BlockReference TBRef, Uint16 aFailNo)
{
  CloseComReqConf * const closeCom = (CloseComReqConf *)&signal->theData[0];
  
  closeCom->xxxBlockRef = TBRef;
  closeCom->requestType = CloseComReqConf::RT_NODE_FAILURE;
  closeCom->failNo      = aFailNo;
  closeCom->noOfNodes   = cprepFailedNodes.count();
  /**
   * We are sending a signal where bitmap is of size NodeBitmask::size and we only
   * have a bitmask of NdbNodeBitmask::size, we clear all bits using NodeBitmask
   * before assigning the smaller bitmask to ensure we don't send any garbage.
   */
  NodeBitmask::clear(closeCom->theNodes);
  cprepFailedNodes.copyto(NdbNodeBitmask::Size, closeCom->theNodes);

  sendSignal(TRPMAN_REF, GSN_CLOSE_COMREQ, signal,
	     CloseComReqConf::SignalLength, JBB);

}//Qmgr::sendCloseComReq()

void 
Qmgr::sendPrepFailReqRef(Signal* signal, 
			 Uint32 dstBlockRef,
			 GlobalSignalNumber gsn,
			 Uint32 blockRef,
			 Uint32 failNo,
			 const NdbNodeBitmask& nodes)
{
  PrepFailReqRef * const prepFail = (PrepFailReqRef *)&signal->theData[0];
  prepFail->xxxBlockRef = blockRef;
  prepFail->failNo = failNo;
  prepFail->noOfNodes = nodes.count();
  nodes.copyto(NdbNodeBitmask::Size, prepFail->theNodes);

  sendSignal(dstBlockRef, gsn, signal, PrepFailReqRef::SignalLength, JBA);  
} 


/**--------------------------------------------------------------------------
 *       SEND PREPARE FAIL REQUEST FROM PRESIDENT.
 *---------------------------------------------------------------------------*/
void Qmgr::sendPrepFailReq(Signal* signal, Uint16 aNode) 
{
  NodeRecPtr sendNodePtr;
  sendNodePtr.i = aNode;
  ptrCheckGuard(sendNodePtr, MAX_NDB_NODES, nodeRec);
  sendNodePtr.p->sendPrepFailReqStatus = Q_ACTIVE;

  sendPrepFailReqRef(signal,
		     sendNodePtr.p->blockRef,
		     GSN_PREP_FAILREQ,
		     reference(),
		     cfailureNr,
		     cfailedNodes);
}//Qmgr::sendPrepFailReq()

/**
 * Arbitration module.  Rest of QMGR calls us only via
 * the "handle" routines.
 */

/**
 * Config signals are logically part of CM_INIT.
 */
void
Qmgr::execARBIT_CFG(Signal* signal)
{
  jamEntry();
  ArbitSignalData* sd = (ArbitSignalData*)&signal->theData[0];
  unsigned rank = sd->code;
  ndbrequire(1 <= rank && rank <= 2);
  arbitRec.apiMask[0].bitOR(sd->mask);
  arbitRec.apiMask[rank].assign(sd->mask);
}

/**
 * ContinueB delay (0=JBA 1=JBB)
 */
Uint32 Qmgr::getArbitDelay()
{
  switch (arbitRec.state) {
  case ARBIT_NULL:
    jam();
    break;
  case ARBIT_INIT:
    jam();
    return 100;
  case ARBIT_FIND:
    jam();
    return 100;
  case ARBIT_PREP1:
    jam();
    return 100;
  case ARBIT_PREP2:
    jam();
    return 100;
  case ARBIT_START:
    jam();
    return 100;
  case ARBIT_RUN:
    jam();
    return 1000;
  case ARBIT_CHOOSE:
    jam();
    return 10;
  case ARBIT_CRASH:             // if we could wait
    jam();
    return 100;
  }
  ndbabort();
  return (Uint32)-1;
}

/**
 * Time to wait for reply.  There is only 1 config parameter
 * (timeout for CHOOSE).  XXX The rest are guesses.
 */
Uint32 Qmgr::getArbitTimeout()
{
  switch (arbitRec.state) {
  case ARBIT_NULL:
    jam();
    break;
  case ARBIT_INIT:              // not used
    jam();
    // Fall through
  case ARBIT_FIND:
    jam();
    /* This timeout will be used only to print out a warning
     * when a suitable arbitrator is not found.
     */
    return 60000;
  case ARBIT_PREP1:
    jam();
    // Fall through
  case ARBIT_PREP2:
    jam();
    return 1000 + cnoOfNodes * Uint32(hb_send_timer.getDelay());
  case ARBIT_START:
    jam();
    return 1000 + arbitRec.timeout;
  case ARBIT_RUN:               // not used (yet)
    jam();
    return 1000;
  case ARBIT_CHOOSE:
    jam();
    return arbitRec.timeout;
  case ARBIT_CRASH:             // if we could wait
    jam();
    return 100;
  }
  ndbabort();
  return (Uint32)-1;
}

/**
 * Start arbitration thread when we are president and database
 * is opened for the first time.
 *
 * XXX  Do arbitration check just like on node failure.  Since
 * there is no arbitrator yet, must win on counts alone.
 */
void
Qmgr::handleArbitStart(Signal* signal)
{
  jam();
  ndbrequire(cpresident == getOwnNodeId());
  ndbrequire(arbitRec.state == ARBIT_NULL);
  arbitRec.state = ARBIT_INIT;
  arbitRec.newstate = true;
  startArbitThread(signal);
}

/**
 * Handle API node failure.  Called also by non-president nodes.
 * If we are president go back to INIT state, otherwise to NULL.
 * Start new thread to save time.
 */
void
Qmgr::handleArbitApiFail(Signal* signal, Uint16 nodeId)
{
  if (arbitRec.node != nodeId) {
    jam();
    return;
  }
  reportArbitEvent(signal, NDB_LE_ArbitState);
  arbitRec.node = 0;
  switch (arbitRec.state) {
  case ARBIT_NULL:              // should not happen
    jam();
    break;
  case ARBIT_INIT:
    jam();
    break;
  case ARBIT_FIND:
    jam();
    break;
  case ARBIT_PREP1:		// start from beginning
    jam();
    // Fall through
  case ARBIT_PREP2:
    jam();
    // Fall through
  case ARBIT_START:
    jam();
    // Fall through
  case ARBIT_RUN:
    if (cpresident == getOwnNodeId()) {
      jam();
      arbitRec.state = ARBIT_INIT;
      arbitRec.newstate = true;
      startArbitThread(signal);
    } else {
      jam();
      arbitRec.state = ARBIT_NULL;
    }
    break;
  case ARBIT_CHOOSE:		// XXX too late
    jam();
    break;
  case ARBIT_CRASH:
    jam();
    break;
  default:
    ndbabort();
  }
}

/**
 * Handle NDB node add.  Ignore if arbitration thread not yet
 * started.  If PREP is not ready, go back to INIT.  Otherwise
 * the new node gets arbitrator and ticket once we reach RUN state.
 * Start new thread to save time.
 */
void
Qmgr::handleArbitNdbAdd(Signal* signal, Uint16 nodeId)
{
  jam();
  ndbrequire(cpresident == getOwnNodeId());
  switch (arbitRec.state) {
  case ARBIT_NULL:              // before db opened
    jam();
    break;
  case ARBIT_INIT:		// start from beginning
    jam();
    // Fall through
  case ARBIT_FIND:
    jam();
    // Fall through
  case ARBIT_PREP1:
    jam();
    // Fall through
  case ARBIT_PREP2:
    jam();
    arbitRec.state = ARBIT_INIT;
    arbitRec.newstate = true;
    startArbitThread(signal);
    break;
  case ARBIT_START:		// process in RUN state
    jam();
    // Fall through
  case ARBIT_RUN:
    jam();
    arbitRec.newMask.set(nodeId);
    break;
  case ARBIT_CHOOSE:            // XXX too late
    jam();
    break;
  case ARBIT_CRASH:
    jam();
    break;
  default:
    ndbabort();
  }
}

/**
 * Check if current nodeset can survive.  The decision is
 * based on node count, node groups, and on external arbitrator
 * (if we have one).  Always starts a new thread because
 * 1) CHOOSE cannot wait 2) if we are new president we need
 * a thread 3) if we are old president it does no harm.
 *
 * The following logic governs if we will survive or not.
 * 1) If at least one node group is fully dead then we will not survive.
 * 2) If 1) is false AND at least one group is fully alive then we will
 *    survive.
 * 3) If 1) AND 2) is false AND a majority of the previously alive nodes are
 *    dead then we will not survive.
 * 4) If 1) AND 2) AND 3) is false AND a majority of the previously alive
 *    nodes are still alive, then we will survive.
 * 5) If 1) AND 2) AND 3) AND 4) is false then exactly half of the previously
 *    alive nodes are dead and the other half is alive. In this case we will
 *    ask the arbitrator whether we can continue or not. If no arbitrator is
 *    currently selected then we will fail. If an arbitrator exists then it
 *    will respond with either WIN in which case our part of the cluster will
 *    remain alive and LOSE in which case our part of the cluster will not
 *    survive.
 *
 * The number of previously alive nodes are the sum of the currently alive
 * nodes plus the number of nodes currently forming a node set that will
 * die. All other nodes was dead in a previous node fail transaction and are
 * not counted in the number of previously alive nodes.
 */
void
Qmgr::handleArbitCheck(Signal* signal)
{
  jam();
  Uint32 prev_alive_nodes = count_previously_alive_nodes();
  ndbrequire(cpresident == getOwnNodeId());
  NdbNodeBitmask survivorNodes;
  computeArbitNdbMask(survivorNodes);
  {
    jam();
    CheckNodeGroups* sd = (CheckNodeGroups*)&signal->theData[0];
    sd->blockRef = reference();
    sd->requestType = CheckNodeGroups::Direct | CheckNodeGroups::ArbitCheck;
    sd->mask = survivorNodes;
    EXECUTE_DIRECT(DBDIH, GSN_CHECKNODEGROUPSREQ, signal, 
		   CheckNodeGroups::SignalLength);
    jamEntry();
    if (ERROR_INSERTED(943))
    {
      ndbout << "Requiring arbitration, even if there is no" 
             << " possible split."<< endl;
      sd->output = CheckNodeGroups::Partitioning;
      arbitRec.state = ARBIT_RUN;
    }
    switch (sd->output) {
    case CheckNodeGroups::Win:
      jam();
      arbitRec.code = ArbitCode::WinGroups;
      break;
    case CheckNodeGroups::Lose:
      jam();
      arbitRec.code = ArbitCode::LoseGroups;
      break;
    case CheckNodeGroups::Partitioning:
      jam();
      arbitRec.code = ArbitCode::Partitioning;
      if (2 * survivorNodes.count() > prev_alive_nodes)
      {
        /**
         * We have lost nodes in all node groups so we are in a
         * potentially partitioned state. If we have the majority
         * of the nodes in this partition we will definitely
         * survive.
         */
        jam();
        arbitRec.code = ArbitCode::WinNodes;
      }
      else if (2 * survivorNodes.count() < prev_alive_nodes)
      {
        jam();
        /**
         * More than half of the live nodes failed and nodes from
         * all node groups failed, we are definitely in a losing
         * streak and we will be part of the failing side. Time
         * to crash.
         */
        arbitRec.code = ArbitCode::LoseNodes;
      }
      else
      {
        jam();
        /**
         * Half of the live nodes failed, we can be in a partitioned
         * state, use the arbitrator to decide what to do next.
         */
      }
      break;
    default:
      ndbabort();
    }
  }
  switch (arbitRec.code) {
  case ArbitCode::LoseNodes:
    jam();
    goto crashme;
  case ArbitCode::LoseGroups:
    jam();
    goto crashme;
  case ArbitCode::WinNodes:
    jam();
    // Fall through
  case ArbitCode::WinGroups:
    jam();
    if (arbitRec.state == ARBIT_RUN)
    {
      jam();
      break;
    }
    arbitRec.state = ARBIT_INIT;
    arbitRec.newstate = true;
    break;
  case ArbitCode::Partitioning:
    if (arbitRec.state == ARBIT_RUN)
    {
      jam();
      arbitRec.state = ARBIT_CHOOSE;
      arbitRec.newstate = true;
      break;
    }
    if (arbitRec.apiMask[0].count() != 0)
    {
      jam();
      arbitRec.code = ArbitCode::LoseNorun;
    }
    else
    {
      jam();
      arbitRec.code = ArbitCode::LoseNocfg;
    }
    goto crashme;
  default:
  crashme:
    jam();
    arbitRec.state = ARBIT_CRASH;
    arbitRec.newstate = true;
    break;
  }
  reportArbitEvent(signal, NDB_LE_ArbitResult);
  switch (arbitRec.state) {
  default:
    jam();
    arbitRec.newMask.bitAND(survivorNodes);   // delete failed nodes
    arbitRec.recvMask.bitAND(survivorNodes);
    sendCommitFailReq(signal);          // start commit of failed nodes
    break;
  case ARBIT_CHOOSE:
    jam();
    break;
  case ARBIT_CRASH:
    jam();
    break;
  }
  startArbitThread(signal);
}

/**
 * Start a new continueB thread.  The thread id is incremented
 * so that any old thread will exit.
 */
void
Qmgr::startArbitThread(Signal* signal)
{
  jam();
  ndbrequire(cpresident == getOwnNodeId());
  arbitRec.code = ArbitCode::ThreadStart;
  reportArbitEvent(signal, NDB_LE_ArbitState);
  signal->theData[1] = ++arbitRec.thread;
  runArbitThread(signal);
}

/**
 * Handle arbitration thread.  The initial thread normally ends
 * up in RUN state.  New thread can be started to save time.
 */
void
Qmgr::runArbitThread(Signal* signal)
{
#ifdef DEBUG_ARBIT
  char buf[256];
  NdbNodeBitmask ndbMask;
  computeArbitNdbMask(ndbMask);
  ndbout << "arbit thread:";
  ndbout << " state=" << arbitRec.state;
  ndbout << " newstate=" << arbitRec.newstate;
  ndbout << " thread=" << arbitRec.thread;
  ndbout << " node=" << arbitRec.node;
  arbitRec.ticket.getText(buf, sizeof(buf));
  ndbout << " ticket=" << buf;
  ndbMask.getText(buf);
  ndbout << " ndbmask=" << buf;
  ndbout << " sendcount=" << arbitRec.sendCount;
  ndbout << " recvcount=" << arbitRec.recvCount;
  arbitRec.recvMask.getText(buf);
  ndbout << " recvmask=" << buf;
  ndbout << " code=" << arbitRec.code;
  ndbout << endl;
#endif
  if (signal->theData[1] != arbitRec.thread) {
    jam();
    return;	        	// old thread dies
  }
  switch (arbitRec.state) {
  case ARBIT_INIT:		// main thread
    jam();
    stateArbitInit(signal);
    break;
  case ARBIT_FIND:
    jam();
    stateArbitFind(signal);
    break;
  case ARBIT_PREP1:
    jam();
    // Fall through
  case ARBIT_PREP2:
    jam();
    stateArbitPrep(signal);
    break;
  case ARBIT_START:
    jam();
    stateArbitStart(signal);
    break;
  case ARBIT_RUN:
    jam();
    stateArbitRun(signal);
    break;
  case ARBIT_CHOOSE:		// partitition thread
    jam();
    if (ERROR_INSERTED(945) && arbitRec.code == ArbitCode::WinChoose)
    {
      // Delay ARBIT_CHOOSE until NdbAPI node is disconnected
      break;
    }
    stateArbitChoose(signal);
    break;
  case ARBIT_CRASH:
    jam();
    stateArbitCrash(signal);
    break;
  default:
    ndbabort();
  }
  signal->theData[0] = ZARBIT_HANDLING;
  signal->theData[1] = arbitRec.thread;
  signal->theData[2] = arbitRec.state;		// just for signal log
  Uint32 delay = getArbitDelay();
  if (delay == 0) {
    jam();
    sendSignal(QMGR_REF, GSN_CONTINUEB, signal, 3, JBA);
  } else if (delay == 1) {
    jam();
    sendSignal(QMGR_REF, GSN_CONTINUEB, signal, 3, JBB);
  } else {
    jam();
    sendSignalWithDelay(QMGR_REF, GSN_CONTINUEB, signal, delay, 3);
  }//if
}

/**
 * Handle INIT state.  Generate next ticket.  Switch to FIND
 * state without delay.
 */
void
Qmgr::stateArbitInit(Signal* signal)
{
  if (arbitRec.newstate) {
    jam();
    CRASH_INSERTION((Uint32)910 + arbitRec.state);

    arbitRec.node = 0;
    arbitRec.ticket.update();
    arbitRec.newMask.clear();
    arbitRec.code = 0;
    arbitRec.newstate = false;
  }
  arbitRec.setTimestamp();  // Init arbitration timer 
  arbitRec.state = ARBIT_FIND;
  arbitRec.newstate = true;
  stateArbitFind(signal);
}

/**
 * Handle FIND state.  Find first arbitrator which is alive
 * and invoke PREP state without delay.  If none are found,
 * loop in FIND state.  This is forever if no arbitrators
 * are configured (not the normal case).
 *
 * XXX  Add adaptive behaviour to avoid getting stuck on API
 * nodes which are alive but do not respond or die too soon.
 */
void
Qmgr::stateArbitFind(Signal* signal)
{
  if (arbitRec.newstate) {
    jam();
    CRASH_INSERTION((Uint32)910 + arbitRec.state);

    arbitRec.code = 0;
    arbitRec.newstate = false;
  }

  switch (arbitRec.method){
  case ArbitRec::METHOD_EXTERNAL:
  {
    // Don't select any API node as arbitrator
    arbitRec.node = 0;
    arbitRec.state = ARBIT_PREP1;
    arbitRec.newstate = true;
    stateArbitPrep(signal);
    return;
    break;
  }

  case ArbitRec::METHOD_DEFAULT:
  {
    NodeRecPtr aPtr;
    // Select the best available API node as arbitrator
    for (unsigned rank = 1; rank <= 2; rank++) {
      jam();
      aPtr.i = 0;
      const unsigned stop = NodeBitmask::NotFound;
      while ((aPtr.i = arbitRec.apiMask[rank].find(aPtr.i + 1)) != stop) {
        jam();
        ptrAss(aPtr, nodeRec);
        if (aPtr.p->phase != ZAPI_ACTIVE)
          continue;
        ndbrequire(c_connectedNodes.get(aPtr.i));
        arbitRec.node = aPtr.i;
        arbitRec.state = ARBIT_PREP1;
        arbitRec.newstate = true;
        stateArbitPrep(signal);
        return;
      }
    }

    /* If the president cannot find a suitable arbitrator then
     * it will report this once a minute. Success in finding
     * an arbitrator will be notified when the arbitrator
     * accepts and acks the offer.
    */

    if (arbitRec.getTimediff() > getArbitTimeout()) {
      jam();
      g_eventLogger->warning("Could not find an arbitrator, cluster is not partition-safe");
      warningEvent("Could not find an arbitrator, cluster is not partition-safe");
      arbitRec.setTimestamp();
    }
    return;
    break;
  }

  default:
    ndbabort();
  }
}

/**
 * Handle PREP states.  First round nulls any existing tickets.
 * Second round sends new ticket.  When all confirms have been
 * received invoke START state immediately.
 */
void
Qmgr::stateArbitPrep(Signal* signal)
{
  if (arbitRec.newstate) {
    jam();
    CRASH_INSERTION((Uint32)910 + arbitRec.state);

    arbitRec.sendCount = 0;                     // send all at once
    computeArbitNdbMask(arbitRec.recvMask);     // to send and recv
    arbitRec.recvMask.clear(getOwnNodeId());
    arbitRec.code = 0;
    arbitRec.newstate = false;
  }
  if (! arbitRec.sendCount) {
    jam();
    NodeRecPtr aPtr;
    aPtr.i = 0;
    const unsigned stop = NodeBitmask::NotFound;
    while ((aPtr.i = arbitRec.recvMask.find(aPtr.i + 1)) != stop) {
      jam();
      ptrAss(aPtr, nodeRec);
      ArbitSignalData* sd = (ArbitSignalData*)&signal->theData[0];
      sd->sender = getOwnNodeId();
      if (arbitRec.state == ARBIT_PREP1) {
        jam();
        sd->code = ArbitCode::PrepPart1;
      } else {
        jam();
        sd->code = ArbitCode::PrepPart2;
      }
      sd->node = arbitRec.node;
      sd->ticket = arbitRec.ticket;
      sd->mask.clear();
      sendSignal(aPtr.p->blockRef, GSN_ARBIT_PREPREQ, signal,
        ArbitSignalData::SignalLength, JBB);
    }
    arbitRec.setTimestamp();			// send time
    arbitRec.sendCount = 1;
    return;
  }
  if (arbitRec.code != 0) {			// error
    jam();
    arbitRec.state = ARBIT_INIT;
    arbitRec.newstate = true;
    return;
  }
  if (arbitRec.recvMask.count() == 0) {		// recv all
    if (arbitRec.state == ARBIT_PREP1) {
      jam();
      arbitRec.state = ARBIT_PREP2;
      arbitRec.newstate = true;
    } else {
      jam();
      arbitRec.state = ARBIT_START;
      arbitRec.newstate = true;
      stateArbitStart(signal);
    }
    return;
  }
  if (arbitRec.getTimediff() > getArbitTimeout()) {
    jam();
    arbitRec.state = ARBIT_INIT;
    arbitRec.newstate = true;
    return;
  }
}

void
Qmgr::execARBIT_PREPREQ(Signal* signal)
{
  jamEntry();
  ArbitSignalData* sd = (ArbitSignalData*)&signal->theData[0];
  if (getOwnNodeId() == cpresident) {
    jam();
    return;		// wrong state
  }
  if (sd->sender != cpresident) {
    jam();
    return;		// wrong state
  }
  NodeRecPtr aPtr;
  aPtr.i = sd->sender;
  ptrAss(aPtr, nodeRec);
  switch (sd->code) {
  case ArbitCode::PrepPart1:    // zero them just to be sure
    jam();
    arbitRec.node = 0;
    arbitRec.ticket.clear();
    break;
  case ArbitCode::PrepPart2:    // non-president enters RUN state
    jam();
    // Fall through
  case ArbitCode::PrepAtrun:
    jam();
    arbitRec.node = sd->node;
    arbitRec.ticket = sd->ticket;
    arbitRec.code = sd->code;
    reportArbitEvent(signal, NDB_LE_ArbitState);
    arbitRec.state = ARBIT_RUN;
    arbitRec.newstate = true;

    // Non-president node logs.
    if (!c_connectedNodes.get(arbitRec.node))
    {
      char buf[20]; // needs 16 + 1 for '\0'
      arbitRec.ticket.getText(buf, sizeof(buf));
      g_eventLogger->warning("President %u proposed disconnected "
                             "node %u as arbitrator [ticket=%s]. "
                             "Cluster may be partially connected. "
                             "Connected nodes: %s",
                             cpresident, arbitRec.node, buf,
                             BaseString::getPrettyTextShort(c_connectedNodes).c_str());

      warningEvent("President %u proposed disconnected node %u "
                   "as arbitrator [ticket %s]",
                   cpresident, arbitRec.node, buf);
      warningEvent("Cluster may be partially connected. Connected nodes: ");

      // Split the connected-node list, since warningEvents are
      // limited to ~24 words / 96 chars
      BaseString tmp(BaseString::getPrettyTextShort(c_connectedNodes).c_str());
      Vector<BaseString> split;
      tmp.split(split, "", 92);
      for(unsigned i = 0; i < split.size(); ++i)
      {
        warningEvent("%s", split[i].c_str());
      }
    }

    if (sd->code == ArbitCode::PrepAtrun) {
      jam();
      return;
    }
    break;
  default:
    jam();
    ndbabort();
  }
  sd->sender = getOwnNodeId();
  sd->code = 0;
  sendSignal(aPtr.p->blockRef, GSN_ARBIT_PREPCONF, signal,
    ArbitSignalData::SignalLength, JBB);
}

void
Qmgr::execARBIT_PREPCONF(Signal* signal)
{
  jamEntry();
  ArbitSignalData* sd = (ArbitSignalData*)&signal->theData[0];
  if (! arbitRec.match(sd)) {
    jam();
    return;		// stray signal
  }
  if (arbitRec.state != ARBIT_PREP1 && arbitRec.state != ARBIT_PREP2) {
    jam();
    return;		// wrong state
  }
  if (! arbitRec.recvMask.get(sd->sender)) {
    jam();
    return;		// wrong state
  }
  arbitRec.recvMask.clear(sd->sender);
  if (arbitRec.code == 0 && sd->code != 0) {
    jam();
    arbitRec.code = sd->code;
  }//if
}

void
Qmgr::execARBIT_PREPREF(Signal* signal)
{
  jamEntry();
  ArbitSignalData* sd = (ArbitSignalData*)&signal->theData[0];
  if (sd->code == 0) {
    jam();
    sd->code = ArbitCode::ErrUnknown;
  }
  execARBIT_PREPCONF(signal);
}

/**
 * Handle START state.  On first call send start request to
 * the chosen arbitrator.  Then wait for a CONF.
 */
void
Qmgr::stateArbitStart(Signal* signal)
{
  if (arbitRec.newstate) {
    jam();
    CRASH_INSERTION((Uint32)910 + arbitRec.state);

    arbitRec.sendCount = 0;
    arbitRec.recvCount = 0;
    arbitRec.code = 0;
    arbitRec.newstate = false;
  }

  switch (arbitRec.method){
  case ArbitRec::METHOD_EXTERNAL:
    jam();
    ndbrequire(arbitRec.node == 0); // No arbitrator selected

    // Don't start arbitrator in API node => ARBIT_RUN
    arbitRec.state = ARBIT_RUN;
    arbitRec.newstate = true;
    return;
    break;

  case ArbitRec::METHOD_DEFAULT:
    if (! arbitRec.sendCount) {
      jam();
      BlockReference blockRef = calcApiClusterMgrBlockRef(arbitRec.node);
      ArbitSignalData* sd = (ArbitSignalData*)&signal->theData[0];
      sd->sender = getOwnNodeId();
      sd->code = 0;
      sd->node = arbitRec.node;
      sd->ticket = arbitRec.ticket;
      sd->mask.clear();
      sendSignal(blockRef, GSN_ARBIT_STARTREQ, signal,
                 ArbitSignalData::SignalLength, JBB);
      arbitRec.sendCount = 1;
      arbitRec.setTimestamp();		// send time
      return;
    }
    if (arbitRec.recvCount) {
      jam();
      reportArbitEvent(signal, NDB_LE_ArbitState);
      if (arbitRec.code == ArbitCode::ApiStart) {
        jam();
        arbitRec.state = ARBIT_RUN;
        arbitRec.newstate = true;
        return;
      }
      arbitRec.state = ARBIT_INIT;
      arbitRec.newstate = true;
      return;
    }
    if (arbitRec.getTimediff() > getArbitTimeout()) {
      jam();
      arbitRec.code = ArbitCode::ErrTimeout;
      reportArbitEvent(signal, NDB_LE_ArbitState);
      arbitRec.state = ARBIT_INIT;
      arbitRec.newstate = true;
      return;
    }
    break;

  default:
    ndbabort();
  }
}

void
Qmgr::execARBIT_STARTCONF(Signal* signal)
{
  jamEntry();
  ArbitSignalData* sd = (ArbitSignalData*)&signal->theData[0];
  if (! arbitRec.match(sd)) {
    jam();
    return;		// stray signal
  }
  if (arbitRec.state != ARBIT_START) {
    jam();
    return;		// wrong state
  }
  if (arbitRec.recvCount) {
    jam();
    return;		// wrong state
  }
  arbitRec.code = sd->code;
  arbitRec.recvCount = 1;
}

void
Qmgr::execARBIT_STARTREF(Signal* signal)
{
  jamEntry();
  ArbitSignalData* sd = (ArbitSignalData*)&signal->theData[0];
  if (sd->code == 0) {
    jam();
    sd->code = ArbitCode::ErrUnknown;
  }
  execARBIT_STARTCONF(signal);
}

/**
 * Handle RUN state.  Send ticket to any new nodes which have
 * appeared after PREP state.  We don't care about a CONF.
 */
void
Qmgr::stateArbitRun(Signal* signal)
{
  if (arbitRec.newstate) {
    jam();
    CRASH_INSERTION((Uint32)910 + arbitRec.state);

    arbitRec.code = 0;
    arbitRec.newstate = false;
  }
  NodeRecPtr aPtr;
  aPtr.i = 0;
  const unsigned stop = NodeBitmask::NotFound;
  while ((aPtr.i = arbitRec.newMask.find(aPtr.i + 1)) != stop) {
    jam();
    arbitRec.newMask.clear(aPtr.i);
    ptrAss(aPtr, nodeRec);
    ArbitSignalData* sd = (ArbitSignalData*)&signal->theData[0];
    sd->sender = getOwnNodeId();
    sd->code = ArbitCode::PrepAtrun;
    sd->node = arbitRec.node;
    sd->ticket = arbitRec.ticket;
    sd->mask.clear();
    sendSignal(aPtr.p->blockRef, GSN_ARBIT_PREPREQ, signal,
      ArbitSignalData::SignalLength, JBB);
  }
}

/**
 * Handle CHOOSE state.  Entered only from RUN state when
 * there is a possible network partitioning.  Send CHOOSE to
 * the arbitrator.  On win switch to INIT state because a new
 * ticket must be created.
 */
void
Qmgr::stateArbitChoose(Signal* signal)
{
  if (arbitRec.newstate) {
    jam();
    CRASH_INSERTION((Uint32)910 + arbitRec.state);

    arbitRec.sendCount = 0;
    arbitRec.recvCount = 0;
    arbitRec.code = 0;
    arbitRec.newstate = false;
  }

  switch(arbitRec.method){
  case ArbitRec::METHOD_EXTERNAL:
  {
    if (! arbitRec.sendCount) {
      jam();
      ndbrequire(arbitRec.node == 0); // No arbitrator selected
      // Don't send CHOOSE to anyone, just wait for timeout to expire
      arbitRec.sendCount = 1;
      arbitRec.setTimestamp();
      return;
    }

    if (arbitRec.getTimediff() > getArbitTimeout()) {
      jam();
      // Arbitration timeout has expired
      ndbrequire(arbitRec.node == 0); // No arbitrator selected

      NodeBitmask nodes;
      computeArbitNdbMask(nodes);
      arbitRec.code = ArbitCode::WinWaitExternal;
      reportArbitEvent(signal, NDB_LE_ArbitResult, nodes);

      sendCommitFailReq(signal);        // start commit of failed nodes
      arbitRec.state = ARBIT_INIT;
      arbitRec.newstate = true;
      return;
    }
    break;
  }

  case ArbitRec::METHOD_DEFAULT:
  {
    if (! arbitRec.sendCount) {
      jam();
      const BlockReference blockRef = calcApiClusterMgrBlockRef(arbitRec.node);
      ArbitSignalData* sd = (ArbitSignalData*)&signal->theData[0];
      sd->sender = getOwnNodeId();
      sd->code = 0;
      sd->node = arbitRec.node;
      sd->ticket = arbitRec.ticket;
      computeArbitNdbMask(sd->mask);
      if (ERROR_INSERTED(943))
      {
        ndbout << "Not sending GSN_ARBIT_CHOOSEREQ, thereby causing" 
               << " arbitration to time out."<< endl;
      }
      else
      {
        sendSignal(blockRef, GSN_ARBIT_CHOOSEREQ, signal,
                   ArbitSignalData::SignalLength, JBA);
      }
      arbitRec.sendCount = 1;
      arbitRec.setTimestamp();		// send time
      return;
    }

    if (arbitRec.recvCount) {
      jam();
      reportArbitEvent(signal, NDB_LE_ArbitResult);
      if (arbitRec.code == ArbitCode::WinChoose) {
        jam();
        sendCommitFailReq(signal);        // start commit of failed nodes
        arbitRec.state = ARBIT_INIT;
        arbitRec.newstate = true;
        return;
      }
      arbitRec.state = ARBIT_CRASH;
      arbitRec.newstate = true;
      stateArbitCrash(signal);		// do it at once
      return;
    }

    if (arbitRec.getTimediff() > getArbitTimeout()) {
      jam();
      // Arbitration timeout has expired
      arbitRec.code = ArbitCode::ErrTimeout;
      reportArbitEvent(signal, NDB_LE_ArbitState);
      arbitRec.state = ARBIT_CRASH;
      arbitRec.newstate = true;
      stateArbitCrash(signal);		// do it at once
      return;
    }
    break;
  }

  default:
    ndbabort();
  }
}

void
Qmgr::execARBIT_CHOOSECONF(Signal* signal)
{
  jamEntry();
  ArbitSignalData* sd = (ArbitSignalData*)&signal->theData[0];
  if (!arbitRec.match(sd)) {
    jam();
    return;		// stray signal
  }
  if (arbitRec.state != ARBIT_CHOOSE) {
    jam();
    return;		// wrong state
  }
  if (arbitRec.recvCount) {
    jam();
    return;		// wrong state
  }
  arbitRec.recvCount = 1;
  arbitRec.code = sd->code;
}

void
Qmgr::execARBIT_CHOOSEREF(Signal* signal)
{
  jamEntry();
  ArbitSignalData* sd = (ArbitSignalData*)&signal->theData[0];
  if (sd->code == 0) {
    jam();
    sd->code = ArbitCode::ErrUnknown;
  }
  execARBIT_CHOOSECONF(signal);
}

/**
 * Handle CRASH state.  We must crash immediately.
 * XXX tell other nodes in our party to crash too.
 */
void
Qmgr::stateArbitCrash(Signal* signal)
{
  jam();
  if (arbitRec.newstate) {
    jam();
    CRASH_INSERTION((Uint32)910 + arbitRec.state);
    arbitRec.setTimestamp();
    arbitRec.code = 0;
    arbitRec.newstate = false;
  }
#ifdef ndb_arbit_crash_wait_for_event_report_to_get_out
  if (! (arbitRec.getTimediff() > getArbitTimeout()))
    return;
#endif
  CRASH_INSERTION(932);
  CRASH_INSERTION(938);
  CRASH_INSERTION(943);
  CRASH_INSERTION(944);
  progError(__LINE__, NDBD_EXIT_ARBIT_SHUTDOWN,
            "Arbitrator decided to shutdown this node");
}

/**
 * Arbitrator may inform us that it will exit.  This lets us
 * start looking sooner for a new one.  Handle it like API node
 * failure.
 */
void
Qmgr::execARBIT_STOPREP(Signal* signal)
{
  jamEntry();
  ArbitSignalData* sd = (ArbitSignalData*)&signal->theData[0];
  if (! arbitRec.match(sd)) {
    jam();
    return;		// stray signal
  }
  arbitRec.code = ArbitCode::ApiExit;
  handleArbitApiFail(signal, arbitRec.node);
}

Uint32
Qmgr::count_previously_alive_nodes()
{
  Uint32 count = 0;
  NodeRecPtr aPtr;
  for (aPtr.i = 1; aPtr.i < MAX_NDB_NODES; aPtr.i++)
  {
    jam();
    ptrAss(aPtr, nodeRec);
    if (getNodeInfo(aPtr.i).getType() == NodeInfo::DB &&
        (aPtr.p->phase == ZRUNNING || aPtr.p->phase == ZPREPARE_FAIL))
    {
      jam();
      count++;
    }
  }
  return count;
}

void
Qmgr::computeArbitNdbMask(NodeBitmaskPOD& aMask)
{
  NodeRecPtr aPtr;
  aMask.clear();
  for (aPtr.i = 1; aPtr.i < MAX_NDB_NODES; aPtr.i++) {
    jam();
    ptrAss(aPtr, nodeRec);
    if (getNodeInfo(aPtr.i).getType() == NodeInfo::DB && aPtr.p->phase == ZRUNNING){
      jam();
      aMask.set(aPtr.i);
    }
  }
}

void
Qmgr::computeArbitNdbMask(NdbNodeBitmaskPOD& aMask)
{
  NodeRecPtr aPtr;
  aMask.clear();
  for (aPtr.i = 1; aPtr.i < MAX_NDB_NODES; aPtr.i++) {
    jam();
    ptrAss(aPtr, nodeRec);
    if (getNodeInfo(aPtr.i).getType() == NodeInfo::DB && aPtr.p->phase == ZRUNNING){
      jam();
      aMask.set(aPtr.i);
    }
  }
}

/**
 * Report arbitration event.  We use arbitration signal format
 * where sender (word 0) is event type.
 */
void
Qmgr::reportArbitEvent(Signal* signal, Ndb_logevent_type type,
                       const NodeBitmask mask)
{
  ArbitSignalData* sd = (ArbitSignalData*)&signal->theData[0];
  sd->sender = type;
  sd->code = arbitRec.code | (arbitRec.state << 16);
  sd->node = arbitRec.node;
  sd->ticket = arbitRec.ticket;
  sd->mask = mask;

  // Log to console/stdout
  LogLevel ll;
  ll.setLogLevel(LogLevel::llNodeRestart, 15);
  g_eventLogger->log(type, &signal->theData[0],
                     ArbitSignalData::SignalLength, 0, &ll);

  sendSignal(CMVMI_REF, GSN_EVENT_REP, signal,
    ArbitSignalData::SignalLength, JBB);
}

// end of arbitration module

void
Qmgr::execDUMP_STATE_ORD(Signal* signal)
{
  if (signal->theData[0] == 1)
  {
    unsigned max_nodes = MAX_NDB_NODES;
    if (signal->getLength() == 2)
    {
      max_nodes = signal->theData[1];
      if (max_nodes == 0 || max_nodes >= MAX_NODES)
      {
        max_nodes = MAX_NODES;
      }
      else
      {
        max_nodes++; // Include node id argument in loop
      }
    }
    infoEvent("creadyDistCom = %d, cpresident = %d\n",
	      creadyDistCom, cpresident);
    infoEvent("cpresidentAlive = %d, cpresidentCand = %d (gci: %d)\n",
              cpresidentAlive, 
	      c_start.m_president_candidate, 
	      c_start.m_president_candidate_gci);
    infoEvent("ctoStatus = %d\n", ctoStatus);
    for(Uint32 i = 1; i < max_nodes; i++){
      NodeRecPtr nodePtr;
      nodePtr.i = i;
      ptrCheckGuard(nodePtr, MAX_NODES, nodeRec);
      char buf[100];
      switch(nodePtr.p->phase){
      case ZINIT:
        sprintf(buf, "Node %d: ZINIT(%d)", i, nodePtr.p->phase);
        break;
      case ZSTARTING:
        sprintf(buf, "Node %d: ZSTARTING(%d)", i, nodePtr.p->phase);
        break;
      case ZRUNNING:
        sprintf(buf, "Node %d: ZRUNNING(%d)", i, nodePtr.p->phase);
        break;
      case ZPREPARE_FAIL:
        sprintf(buf, "Node %d: ZPREPARE_FAIL(%d)", i, nodePtr.p->phase);
        break;
      case ZFAIL_CLOSING:
        sprintf(buf, "Node %d: ZFAIL_CLOSING(%d)", i, nodePtr.p->phase);
        break;
      case ZAPI_INACTIVE:
        sprintf(buf, "Node %d: ZAPI_INACTIVE(%d)", i, nodePtr.p->phase);
        break;
      case ZAPI_ACTIVE:
        sprintf(buf, "Node %d: ZAPI_ACTIVE(%d)", i, nodePtr.p->phase);
        break;
      case ZAPI_ACTIVATION_ONGOING:
        sprintf(buf, "Node %d: ZAPI_ACTIVATION_ONGOING(%d)",
                i,
                nodePtr.p->phase);
        break;
      default:
        sprintf(buf, "Node %d: <UNKNOWN>(%d)", i, nodePtr.p->phase);
        break;
      }
      infoEvent("%s", buf);
    }
  }

#ifdef ERROR_INSERT
  if (signal->theData[0] == 935 && signal->getLength() == 2)
  {
    SET_ERROR_INSERT_VALUE(935);
    c_error_insert_extra = signal->theData[1];
  }
#endif

  if (signal->theData[0] == 900 && signal->getLength() == 2)
  {
    ndbout_c("disconnecting %u", signal->theData[1]);
    api_failed(signal, signal->theData[1]);
  }

  if (signal->theData[0] == 908)
  {
    int tag = signal->getLength() < 2 ? -1 : signal->theData[1];
    char buf[8192];
    // for easy grepping in *out.log ...
    strcpy(buf, "HB:");
    if (tag >= 0)
      sprintf(buf+strlen(buf), "%d:", tag);
    sprintf(buf+strlen(buf), " pres:%u", cpresident);
    sprintf(buf+strlen(buf), " own:%u", getOwnNodeId());
    NodeRecPtr myNodePtr;
    myNodePtr.i = getOwnNodeId();
    ptrCheckGuard(myNodePtr, MAX_NDB_NODES, nodeRec);
    sprintf(buf+strlen(buf), " dyn:%u-%u", myNodePtr.p->ndynamicId & 0xFFFF, myNodePtr.p->ndynamicId >> 16);
    sprintf(buf+strlen(buf), " mxdyn:%u", c_maxDynamicId);
    sprintf(buf+strlen(buf), " hb:%u->%u->%u", cneighbourl, getOwnNodeId(), cneighbourh);
    sprintf(buf+strlen(buf), " node:dyn-hi,cfg:");
    NodeRecPtr nodePtr;
    for (nodePtr.i = 1; nodePtr.i < MAX_NDB_NODES; nodePtr.i++)
    {
      ptrAss(nodePtr, nodeRec);
      Uint32 type = getNodeInfo(nodePtr.i).m_type;
      if (type == NodeInfo::DB)
      {
        sprintf(buf+strlen(buf), " %u:%u-%u,%u", nodePtr.i, nodePtr.p->ndynamicId & 0xFFFF, nodePtr.p->ndynamicId >> 16, nodePtr.p->hbOrder);
      }
    }
    ndbout << buf << endl;
  }

#ifdef ERROR_INSERT
  Uint32 dumpCode = signal->theData[0];
  if ((dumpCode == 9992) ||
      (dumpCode == 9993))
  {
    if (signal->getLength() == 2)
    {
      Uint32 nodeId = signal->theData[1];
      Uint32& newNodeId = signal->theData[1];
      Uint32 length = 2;
      assert(257 > MAX_NODES);
      if (nodeId > MAX_NODES)
      {
        const char* type = "None";
        switch (nodeId)
        {
        case 257:
        {
          /* Left (lower) neighbour */
          newNodeId = cneighbourl;
          type = "Left neighbour";
          break;
        }
        case 258:
        {
          /* Right (higher) neighbour */
          newNodeId = cneighbourh;
          type = "Right neighbour";
          break;
        }
        case 259:
        {
          /* President */
          newNodeId = cpresident;
          type = "President";
          break;
        }
        }
        ndbout_c("QMGR : Mapping request on node id %u to node id %u (%s)",
                 nodeId, newNodeId, type);
        if (newNodeId != nodeId)
        {
          sendSignal(CMVMI_REF, GSN_DUMP_STATE_ORD, signal, length, JBB);
        }
      }
    }
  }

  if (dumpCode == 9994)
  {
    ndbout_c("setCCDelay(%u)", signal->theData[1]);
    setCCDelay(signal->theData[1]);
    m_connectivity_check.m_enabled = true;
  }
#endif

  if (signal->theData[0] == 939 && signal->getLength() == 2)
  {
    jam();
    Uint32 nodeId = signal->theData[1];
    ndbout_c("Force close communication to %u", nodeId);
    SET_ERROR_INSERT_VALUE2(939, nodeId);
    CloseComReqConf * closeCom = CAST_PTR(CloseComReqConf,
                                          signal->getDataPtrSend());

    closeCom->xxxBlockRef = reference();
    closeCom->requestType = CloseComReqConf::RT_NO_REPLY;
    closeCom->failNo      = 0;
    closeCom->noOfNodes   = 1;
    NodeBitmask::clear(closeCom->theNodes);
    NodeBitmask::set(closeCom->theNodes, nodeId);
    sendSignal(TRPMAN_REF, GSN_CLOSE_COMREQ, signal,
               CloseComReqConf::SignalLength, JBB);
  }
}//Qmgr::execDUMP_STATE_ORD()

void
Qmgr::execAPI_BROADCAST_REP(Signal* signal)
{
  jamEntry();
  ApiBroadcastRep api= *(const ApiBroadcastRep*)signal->getDataPtr();

  SectionHandle handle(this, signal);
  Uint32 len = signal->getLength() - ApiBroadcastRep::SignalLength;
  memmove(signal->theData, signal->theData+ApiBroadcastRep::SignalLength, 
	  4*len);
  
  NodeBitmask mask;
  NodeRecPtr nodePtr;
  for (nodePtr.i = 1; nodePtr.i < MAX_NODES; nodePtr.i++) 
  {
    jam();
    ptrAss(nodePtr, nodeRec);
    if (nodePtr.p->phase == ZAPI_ACTIVE && 
	getNodeInfo(nodePtr.i).m_version >= api.minVersion)
    {
      jam();
      mask.set(nodePtr.i);
    }
  }
  
  if (mask.isclear())
  {
    jam();
    releaseSections(handle);
    return;
  }

  NodeReceiverGroup rg(API_CLUSTERMGR, mask);
  sendSignal(rg, api.gsn, signal, len, JBB,
	     &handle);
}

void
Qmgr::execNODE_FAILREP(Signal * signal)
{
  jamEntry();
  // make sure any distributed signals get acknowledged
  // destructive of the signal
  c_counterMgr.execNODE_FAILREP(signal);
}

void
Qmgr::execALLOC_NODEID_REQ(Signal * signal)
{
  jamEntry();
  AllocNodeIdReq req = *(AllocNodeIdReq*)signal->getDataPtr();
  Uint32 error = 0;

  NodeRecPtr nodePtr;
  nodePtr.i = req.nodeId;
  if ((nodePtr.i >= MAX_NODES) ||
      ((req.nodeType == NodeInfo::DB) &&
       (nodePtr.i >= MAX_NDB_NODES)))
  {
    /* Ignore messages about nodes not even within range */
    jam();
    return;
  }
  ptrAss(nodePtr, nodeRec);

  if (refToBlock(req.senderRef) != QMGR) // request from management server
  {
    /* master */

    if (getOwnNodeId() != cpresident)
    {
      jam();
      error = AllocNodeIdRef::NotMaster;
    }
    else if (!opAllocNodeIdReq.m_tracker.done())
    {
      jam();
      error = AllocNodeIdRef::Busy;
    }
    else if (c_connectedNodes.get(req.nodeId))
    {
      jam();
      error = AllocNodeIdRef::NodeConnected;
    }
    else if (nodePtr.p->m_secret != 0)
    {
      jam();
      error = AllocNodeIdRef::NodeReserved;
    }
    else if (req.nodeType != getNodeInfo(req.nodeId).m_type)
    {
      jam();
      error = AllocNodeIdRef::NodeTypeMismatch;
    }
    else if (req.nodeType == NodeInfo::API && c_allow_api_connect == 0)
    {
      jam();
      error = AllocNodeIdRef::NodeReserved;
    }

    if (error)
    {
      jam();
      AllocNodeIdRef * ref = (AllocNodeIdRef*)signal->getDataPtrSend();
      ref->senderRef = reference();
      ref->errorCode = error;
      ref->masterRef = numberToRef(QMGR, cpresident);
      ref->senderData = req.senderData;
      ref->nodeId = req.nodeId;
      sendSignal(req.senderRef, GSN_ALLOC_NODEID_REF, signal,
                 AllocNodeIdRef::SignalLength, JBB);
      return;
    }

    if (ERROR_INSERTED(934) && req.nodeId != getOwnNodeId())
    {
      CRASH_INSERTION(934);
    }

    /**
     * generate secret
     */
    const NDB_TICKS now = NdbTick_getCurrentTicks();
    const Uint32 secret_hi = Uint32(now.getUint64() >> 24);
    const Uint32 secret_lo = Uint32(now.getUint64() << 8) + getOwnNodeId();
    req.secret_hi = secret_hi;
    req.secret_lo = secret_lo;

    if (req.timeout > 60000)
      req.timeout = 60000;

    nodePtr.p->m_secret = (Uint64(secret_hi) << 32) + secret_lo;
    nodePtr.p->m_alloc_timeout = NdbTick_AddMilliseconds(now,req.timeout);

    opAllocNodeIdReq.m_req = req;
    opAllocNodeIdReq.m_error = 0;
    opAllocNodeIdReq.m_connectCount =
      getNodeInfo(refToNode(req.senderRef)).m_connectCount;

    jam();
    AllocNodeIdReq * req2 = (AllocNodeIdReq*)signal->getDataPtrSend();
    * req2 = req;
    req2->senderRef = reference();
    NodeReceiverGroup rg(QMGR, c_clusterNodes);
    RequestTracker & p = opAllocNodeIdReq.m_tracker;
    p.init<AllocNodeIdRef>(c_counterMgr, rg, GSN_ALLOC_NODEID_REF, 0);

    sendSignal(rg, GSN_ALLOC_NODEID_REQ, signal,
               AllocNodeIdReq::SignalLengthQMGR, JBB);
    return;
  }

  /* participant */
  if (c_connectedNodes.get(req.nodeId))
  {
    jam();
    error = AllocNodeIdRef::NodeConnected;
  }
  else if (req.nodeType != getNodeInfo(req.nodeId).m_type)
  {
    jam();
    error = AllocNodeIdRef::NodeTypeMismatch;
  }
  else if ((nodePtr.p->failState != NORMAL) ||
           ((req.nodeType == NodeInfo::DB) &&
            (cfailedNodes.get(nodePtr.i))))
  {
    /**
     * Either the node has committed its node failure in QMGR but not yet
     * completed the node internal node failure handling. Or the node
     * failure commit process is still ongoing in QMGR. We should not
     * allocate a node id in either case.
     */
    jam();
    error = AllocNodeIdRef::NodeFailureHandlingNotCompleted;
  }
  else if (req.nodeType == NodeInfo::API && nodePtr.p->phase != ZAPI_INACTIVE)
  {
    jam();
    if (cpresident != getOwnNodeId() && c_allow_api_connect == 0)
    {
      /**
       * Don't block during NR
       */
      jam();
    }
    else
    {
      jam();
      error = AllocNodeIdRef::NodeReserved;
    }
  }
#if 0
  /**
   * For now only make "time/secret" based reservation on master
   *   as we otherwise also need to clear it on failure + handle
   *   master failure
   */
  else if (nodePtr.p->m_secret != 0)
  {
    jam();
    error = AllocNodeIdRef::NodeReserved;
  }
#endif

  if (error)
  {
    jam();
    AllocNodeIdRef * ref = (AllocNodeIdRef*)signal->getDataPtrSend();
    ref->senderRef = reference();
    ref->errorCode = error;
    ref->senderData = req.senderData;
    ref->nodeId = req.nodeId;
    ref->masterRef = numberToRef(QMGR, cpresident);
    sendSignal(req.senderRef, GSN_ALLOC_NODEID_REF, signal,
               AllocNodeIdRef::SignalLength, JBB);
    return;
  }

  AllocNodeIdConf * conf = (AllocNodeIdConf*)signal->getDataPtrSend();
  conf->senderRef = reference();
  conf->secret_hi = req.secret_hi;
  conf->secret_lo = req.secret_lo;
  sendSignal(req.senderRef, GSN_ALLOC_NODEID_CONF, signal,
             AllocNodeIdConf::SignalLength, JBB);
}

void
Qmgr::execALLOC_NODEID_CONF(Signal * signal)
{
  /* master */

  jamEntry();
  const AllocNodeIdConf * conf = (AllocNodeIdConf*)signal->getDataPtr();
  opAllocNodeIdReq.m_tracker.reportConf(c_counterMgr,
                                        refToNode(conf->senderRef));

  if (signal->getLength() >= AllocNodeIdConf::SignalLength)
  {
    jam();
    if (opAllocNodeIdReq.m_req.secret_hi != conf->secret_hi ||
        opAllocNodeIdReq.m_req.secret_lo != conf->secret_lo)
    {
      jam();
      if (opAllocNodeIdReq.m_error == 0)
      {
        jam();
        opAllocNodeIdReq.m_error = AllocNodeIdRef::Undefined;
      }
    }
  }

  completeAllocNodeIdReq(signal);
}


void
Qmgr::execALLOC_NODEID_REF(Signal * signal)
{
  /* master */

  jamEntry();
  const AllocNodeIdRef * ref = (AllocNodeIdRef*)signal->getDataPtr();

  if (ref->errorCode == AllocNodeIdRef::NF_FakeErrorREF)
  {
    jam();
    if (ref->nodeId == refToNode(ref->senderRef))
    {
      /**
       * The node id we are trying to allocate has responded with a REF,
       * this was sent in response to a node failure, so we are most
       * likely not ready to allocate this node id yet. Report node
       * failure handling not ready yet.
       */
      jam();
      opAllocNodeIdReq.m_tracker.reportRef(c_counterMgr,
                                           refToNode(ref->senderRef));
      if (opAllocNodeIdReq.m_error == 0)
      {
        jam();
        opAllocNodeIdReq.m_error =
          AllocNodeIdRef::NodeFailureHandlingNotCompleted;
      }
    }
    else
    {
      jam();
      opAllocNodeIdReq.m_tracker.ignoreRef(c_counterMgr,
                                           refToNode(ref->senderRef));
    }
  }
  else
  {
    jam();
    opAllocNodeIdReq.m_tracker.reportRef(c_counterMgr,
                                         refToNode(ref->senderRef));
    if (opAllocNodeIdReq.m_error == 0)
    {
      jam();
      opAllocNodeIdReq.m_error = ref->errorCode;
    }
  }
  completeAllocNodeIdReq(signal);
}

void
Qmgr::completeAllocNodeIdReq(Signal *signal)
{
  /* master */

  if (!opAllocNodeIdReq.m_tracker.done())
  {
    jam();
    return;
  }

  if (opAllocNodeIdReq.m_connectCount !=
      getNodeInfo(refToNode(opAllocNodeIdReq.m_req.senderRef)).m_connectCount)
  {
    // management server not same version as the original requester
    jam();
    return;
  }

  if (opAllocNodeIdReq.m_tracker.hasRef())
  {
    jam();

    {
      /**
       * Clear reservation
       */
      NodeRecPtr nodePtr;
      nodePtr.i = opAllocNodeIdReq.m_req.nodeId;
      ptrAss(nodePtr, nodeRec);
      nodePtr.p->m_secret = 0;
    }

    AllocNodeIdRef * ref = (AllocNodeIdRef*)signal->getDataPtrSend();
    ref->senderRef = reference();
    ref->senderData = opAllocNodeIdReq.m_req.senderData;
    ref->nodeId = opAllocNodeIdReq.m_req.nodeId;
    ref->errorCode = opAllocNodeIdReq.m_error;
    ref->masterRef = numberToRef(QMGR, cpresident);
    ndbassert(AllocNodeIdRef::SignalLength == 5);
    sendSignal(opAllocNodeIdReq.m_req.senderRef, GSN_ALLOC_NODEID_REF, signal,
               AllocNodeIdRef::SignalLength, JBB);
    return;
  }

  jam();

  AllocNodeIdConf * conf = (AllocNodeIdConf*)signal->getDataPtrSend();
  conf->senderRef = reference();
  conf->senderData = opAllocNodeIdReq.m_req.senderData;
  conf->nodeId = opAllocNodeIdReq.m_req.nodeId;
  conf->secret_lo = opAllocNodeIdReq.m_req.secret_lo;
  conf->secret_hi = opAllocNodeIdReq.m_req.secret_hi;
  sendSignal(opAllocNodeIdReq.m_req.senderRef, GSN_ALLOC_NODEID_CONF, signal,
             AllocNodeIdConf::SignalLength, JBB);

  /**
   * We are the master and master DIH wants to keep track of node restart
   * state to be able to control LCP start and stop and also to be able
   * to easily report this state to the user when he asks for it.
   */
  AllocNodeIdRep *rep = (AllocNodeIdRep*)signal->getDataPtrSend();
  rep->nodeId = opAllocNodeIdReq.m_req.nodeId;
  EXECUTE_DIRECT(DBDIH, GSN_ALLOC_NODEID_REP, signal, 
		 AllocNodeIdRep::SignalLength);
}
	
void
Qmgr::execSTOP_REQ(Signal* signal)
{
  jamEntry();
  c_stopReq = * (StopReq*)signal->getDataPtr();

  if (c_stopReq.senderRef)
  {
    jam();
    ndbrequire(NdbNodeBitmask::get(c_stopReq.nodes, getOwnNodeId()));
    
    StopConf *conf = (StopConf*)signal->getDataPtrSend();
    conf->senderData = c_stopReq.senderData;
    conf->nodeState = getOwnNodeId();
    sendSignal(c_stopReq.senderRef, 
	       GSN_STOP_CONF, signal, StopConf::SignalLength, JBA);
  }
}

bool
Qmgr::check_multi_node_shutdown(Signal* signal)
{
  if (c_stopReq.senderRef && 
      NdbNodeBitmask::get(c_stopReq.nodes, getOwnNodeId()))
  {
    jam();
    if(StopReq::getPerformRestart(c_stopReq.requestInfo))
    {
      jam();
      StartOrd * startOrd = (StartOrd *)&signal->theData[0];
      startOrd->restartInfo = c_stopReq.requestInfo;
      sendSignal(CMVMI_REF, GSN_START_ORD, signal, 2, JBA);
    } else {
      sendSignal(CMVMI_REF, GSN_STOP_ORD, signal, 1, JBA);
    }
    return true;
  }
  return false;
}

int
Qmgr::check_hb_order_config()
{
  m_hb_order_config_used = false;
  Uint32 count = 0;
  Uint32 count_zero = 0;
  NodeRecPtr nodePtr;
  for (nodePtr.i = 1; nodePtr.i < MAX_NDB_NODES; nodePtr.i++)
  {
    ptrAss(nodePtr, nodeRec);
    const NodeInfo& nodeInfo = getNodeInfo(nodePtr.i);
    if (nodeInfo.m_type == NodeInfo::DB)
    {
      count++;
      if (nodePtr.p->hbOrder == 0)
        count_zero++;
    }
  }
  ndbrequire(count != 0); // must have node info
  if (count_zero == count)
  {
    jam();
    return 0; // no hbOrder defined
  }
  if (count_zero != 0)
  {
    jam();
    return -1; // error: not all zero or all nonzero
  }
  for (nodePtr.i = 1; nodePtr.i < MAX_NDB_NODES; nodePtr.i++)
  {
    ptrAss(nodePtr, nodeRec);
    const NodeInfo& nodeInfo = getNodeInfo(nodePtr.i);
    if (nodeInfo.m_type == NodeInfo::DB)
    {
      NodeRecPtr nodePtr2;
      for (nodePtr2.i = nodePtr.i + 1; nodePtr2.i < MAX_NDB_NODES; nodePtr2.i++)
      {
        ptrAss(nodePtr2, nodeRec);
        const NodeInfo& nodeInfo2 = getNodeInfo(nodePtr2.i);
        if (nodeInfo2.m_type == NodeInfo::DB)
        {
          if (nodePtr.i != nodePtr2.i &&
              nodePtr.p->hbOrder == nodePtr2.p->hbOrder)
          {
            jam();
            return -2; // error: duplicate nonzero value
          }
        }
      }
    }
  }
  m_hb_order_config_used = true;
  return 0;
}

static const Uint32 CC_SuspectTicks = 1;
static const Uint32 CC_FailedTicks = 2;

void
Qmgr::startConnectivityCheck(Signal* signal, Uint32 reason, Uint32 causingNode)
{
  jam();
  ndbrequire(m_connectivity_check.getEnabled());

  if (m_connectivity_check.m_active)
  {
    jam();
    /* Connectivity check underway already
     * do nothing
     */
    return;
  }


  m_connectivity_check.m_nodesPinged.clear();

  /* Send NODE_PINGREQ signal to all other running nodes, and
   * initialise connectivity check bitmasks.
   * Note that nodes may already be considered suspect due to
   * a previous connectivity check round.
   */
  Uint32 ownId = getOwnNodeId();
  NodePingReq* pingReq = CAST_PTR(NodePingReq, &signal->theData[0]);
  pingReq->senderData = ++m_connectivity_check.m_currentRound;
  pingReq->senderRef = reference();

  for (Uint32 i=1; i < MAX_NDB_NODES; i++)
  {
    if (i != ownId)
    {
      NodeRec& node = nodeRec[i];
      if (node.phase == ZRUNNING)
      {
        /* If connection was considered ok, treat as unknown,
         * If it was considered slow, continue to treat
         *   as slow
         */
        sendSignal(node.blockRef,
                   GSN_NODE_PING_REQ,
                   signal,
                   NodePingReq::SignalLength,
                   JBA);

        m_connectivity_check.m_nodesPinged.set(i);
      }
    }
  }

  /* Initialise result bitmasks */
  m_connectivity_check.m_nodesWaiting.assign(m_connectivity_check.m_nodesPinged);
  m_connectivity_check.m_nodesFailedDuring.clear();

  /* Ensure only live nodes are considered suspect */
  m_connectivity_check.m_nodesSuspect.bitAND(m_connectivity_check.m_nodesPinged);

  const char* reasonText = "Unknown";
  bool firstTime = true;

  switch(reason)
  {
  case FailRep::ZHEARTBEAT_FAILURE:
    reasonText = "Heartbeat failure";
    break;
  case FailRep::ZCONNECT_CHECK_FAILURE:
    reasonText = "Connectivity check request";
    break;
  default:
    firstTime = false;
    ndbrequire(m_connectivity_check.m_nodesSuspect.count() > 0);
    break;
  }

  if (!m_connectivity_check.m_nodesPinged.isclear())
  {
    jam();
    {
      char buff[100];
      m_connectivity_check.m_nodesPinged.getText(buff);
      if (firstTime)
      {
        g_eventLogger->info("QMGR : Starting connectivity check of %u other nodes (%s) due to %s from node %u.",
                            m_connectivity_check.m_nodesPinged.count(),
                            buff,
                            reasonText,
                            causingNode);
      }
      else
      {
        char buff2[100];
        m_connectivity_check.m_nodesSuspect.getText(buff2);
        g_eventLogger->info("QMGR : Restarting connectivity check of %u other nodes (%s) due to %u syspect nodes (%s)",
                            m_connectivity_check.m_nodesPinged.count(),
                            buff,
                            m_connectivity_check.m_nodesSuspect.count(),
                            buff2);
      }
    }

    /* Generate cluster log event */
    Uint32 bitmaskSz = NdbNodeBitmask::Size;
    signal->theData[0] = NDB_LE_ConnectCheckStarted;
    signal->theData[1] = m_connectivity_check.m_nodesPinged.count();
    signal->theData[2] = reason;
    signal->theData[3] = causingNode;
    signal->theData[4] = bitmaskSz;
    Uint32* sigPtr = &signal->theData[5];
    m_connectivity_check.m_nodesPinged.copyto(bitmaskSz, sigPtr); sigPtr+= bitmaskSz;
    m_connectivity_check.m_nodesSuspect.copyto(bitmaskSz, sigPtr);
    sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 5 + (2 * bitmaskSz), JBB);

    m_connectivity_check.m_active = true;
    m_connectivity_check.m_tick = 0;
    const NDB_TICKS now = NdbTick_getCurrentTicks();
    m_connectivity_check.m_timer.reset(now);
  }
  else
  {
    g_eventLogger->info("QMGR : Connectivity check requested due to %s (from %u) not started as no other running nodes.",
                        reasonText,
                        causingNode);
  }
}

void
Qmgr::execNODE_PINGREQ(Signal* signal)
{
  jamEntry();
  Uint32 ownId = getOwnNodeId();
  const NodePingReq* pingReq = CAST_CONSTPTR(NodePingReq, &signal->theData[0]);
  Uint32 sendersRef = signal->getSendersBlockRef();
  Uint32 sendersNodeId = refToNode(sendersRef);
  Uint32 senderData = pingReq->senderData;

  ndbrequire(sendersNodeId != ownId);

  /* We will start our own connectivity check if necessary
   * before responding with PING_CONF to the requestor.
   * This means that the sending node will receive our PING_REQ
   * before our PING_CONF, which should avoid them starting an
   * unnecessary extra connectivity check round in some cases.
   */
  if (likely(m_connectivity_check.getEnabled()))
  {
    jam();
    /* We have connectivity checking configured */
    if (! m_connectivity_check.m_active)
    {
      jam();

      {
        /* Don't start a new connectivity check if the requesting
         * node has failed from our point of view
         */
        NodeRecPtr nodePtr;
        nodePtr.i = sendersNodeId;
        ptrCheckGuard(nodePtr, MAX_NDB_NODES, nodeRec);
        if (unlikely(nodePtr.p->phase != ZRUNNING))
        {
          jam();

          g_eventLogger->warning("QMGR : Discarding NODE_PINGREQ from non-running node %u (%u)",
                                 sendersNodeId, nodePtr.p->phase);
          return;
        }
      }

      /* Start our own Connectivity Check now indicating reason and causing node */
      startConnectivityCheck(signal, FailRep::ZCONNECT_CHECK_FAILURE, sendersNodeId);
    }
  }
  else
  {
    jam();
    g_eventLogger->warning("QMGR : NODE_PINGREQ received from node %u, but connectivity "
                           "checking not configured on this node.  Ensure all "
                           "nodes have the same configuration for parameter "
                           "ConnectCheckIntervalMillis.",
                           sendersNodeId);
  }

  /* Now respond with NODE_PINGCONF */
  NodePingConf* pingConf = CAST_PTR(NodePingConf, &signal->theData[0]);

  pingConf->senderData = senderData;
  pingConf->senderRef = reference();

  sendSignal(sendersRef,
             GSN_NODE_PING_CONF,
             signal,
             NodePingConf::SignalLength,
             JBA);
}

void
Qmgr::ConnectCheckRec::reportNodeConnect(Uint32 nodeId)
{
  /* Clear any suspicion */
  m_nodesSuspect.clear(nodeId);
}

bool
Qmgr::ConnectCheckRec::reportNodeFailure(Uint32 nodeId)
{
  if (unlikely(m_active))
  {
    m_nodesFailedDuring.set(nodeId);

    if (m_nodesWaiting.get(nodeId))
    {
      /* We were waiting for a NODE_PING_CONF from this node,
       * remove it from the set
       */
      m_nodesWaiting.clear(nodeId);

      return m_nodesWaiting.isclear();
    }
  }
  return false;
}

void
Qmgr::execNODE_PINGCONF(Signal* signal)
{
  jamEntry();

  ndbrequire(m_connectivity_check.getEnabled());

  const NodePingConf* pingConf = CAST_CONSTPTR(NodePingConf, &signal->theData[0]);
  Uint32 sendersBlockRef = signal->getSendersBlockRef();
  Uint32 sendersNodeId = refToNode(sendersBlockRef);
  Uint32 roundNumber = pingConf->senderData;

  ndbrequire(sendersNodeId != getOwnNodeId());
  ndbrequire((m_connectivity_check.m_active)                                || /* Normal */
             (m_connectivity_check.m_nodesWaiting.get(sendersNodeId)          || /* We killed last round */
              m_connectivity_check.m_nodesFailedDuring.get(sendersNodeId)));     /* Someone killed */

  if (unlikely((! m_connectivity_check.m_active) ||
               (roundNumber != m_connectivity_check.m_currentRound)))
  {
    g_eventLogger->warning("QMGR : Received NODEPING_CONF from node %u for round %u, "
                           "but we are %sactive on round %u.  Discarding.",
                           sendersNodeId,
                           roundNumber,
                           ((m_connectivity_check.m_active)?"":"in"),
                           m_connectivity_check.m_currentRound);
    return;
  }

  if (ERROR_INSERTED(938))
  {
    ndbout_c("QMGR : execNODE_PING_CONF() from %u in tick %u",
             sendersNodeId, m_connectivity_check.m_tick);
  }

  /* Node must have been pinged, we must be waiting for the response,
   * or the node must have already failed
   */
  ndbrequire(m_connectivity_check.m_nodesPinged.get(sendersNodeId));
  ndbrequire(m_connectivity_check.m_nodesWaiting.get(sendersNodeId) ||
             m_connectivity_check.m_nodesFailedDuring.get(sendersNodeId));

  m_connectivity_check.m_nodesWaiting.clear(sendersNodeId);

  if (likely(m_connectivity_check.m_tick < CC_SuspectTicks))
  {
    jam();
    /* Node responded on time, clear any suspicion about it */
    m_connectivity_check.m_nodesSuspect.clear(sendersNodeId);
  }

  if (m_connectivity_check.m_nodesWaiting.isclear())
  {
    jam();
    /* Connectivity check round is now finished */
    connectivityCheckCompleted(signal);
  }
}

void
Qmgr::connectivityCheckCompleted(Signal* signal)
{
  jam();

  m_connectivity_check.m_active = false;

  /* Log the following :
   * Nodes checked
   * Nodes responded ok
   * Nodes responded late (now suspect)
   * Nodes failed to respond.
   * Nodes failed during
   */
  char pinged[100];
  char late[100];
  char silent[100];
  char failed[100];

  /* Any 'waiting' nodes have been killed
   * Surviving suspects do not include them.
   */
  NdbNodeBitmask survivingSuspects(m_connectivity_check.m_nodesSuspect);
  survivingSuspects.bitANDC(m_connectivity_check.m_nodesWaiting);

  /* Nodes that failed during the check are also excluded */
  survivingSuspects.bitANDC(m_connectivity_check.m_nodesFailedDuring);

  m_connectivity_check.m_nodesPinged.getText(pinged);
  survivingSuspects.getText(late);
  m_connectivity_check.m_nodesWaiting.getText(silent);
  m_connectivity_check.m_nodesFailedDuring.getText(failed);

  g_eventLogger->info("QMGR : Connectivity check completed, "
                      "%u other nodes checked (%s), "
                      "%u responded on time, "
                      "%u responded late (%s), "
                      "%u no response will be failed (%s), "
                      "%u failed during check (%s)\n",
                      m_connectivity_check.m_nodesPinged.count(),
                      pinged,
                      m_connectivity_check.m_nodesPinged.count() -
                      m_connectivity_check.m_nodesSuspect.count(),
                      survivingSuspects.count(),
                      late,
                      m_connectivity_check.m_nodesWaiting.count(),
                      silent,
                      m_connectivity_check.m_nodesFailedDuring.count(),
                      failed);

  /* Log in Cluster log */
  signal->theData[0] = NDB_LE_ConnectCheckCompleted;
  signal->theData[1] = m_connectivity_check.m_nodesPinged.count();
  signal->theData[2] = survivingSuspects.count();
  signal->theData[3] = m_connectivity_check.m_nodesWaiting.count() +
    m_connectivity_check.m_nodesFailedDuring.count();

  sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 4, JBB);

  if (survivingSuspects.count() > 0)
  {
    jam();
    /* Still suspect nodes, start another round */
    g_eventLogger->info("QMGR : Starting new connectivity check due to suspect nodes.");
    /* Restart connectivity check, no external reason or cause */
    startConnectivityCheck(signal, 0, 0);
  }
  else
  {
    jam();
    /* No suspect nodes, stop the protocol now */

    g_eventLogger->info("QMGR : All other nodes (%u) connectivity ok.",
                        m_connectivity_check.m_nodesPinged.count() -
                        (m_connectivity_check.m_nodesWaiting.count() +
                         m_connectivity_check.m_nodesFailedDuring.count()));

    /* Send a heartbeat to our right neighbour at this point as a gesture
     * of goodwill
     */
    sendHeartbeat(signal);
    hb_send_timer.reset(NdbTick_getCurrentTicks());
  };
}

void
Qmgr::checkConnectivityTimeSignal(Signal* signal)
{
  /* Executed periodically when a connectivity check is
   * underway.
   * After CC_SuspectTicks have elapsed, any nodes
   * which have not responded are considered
   * 'Suspect'.
   * After CC_FailedTicks have elapsed, any nodes
   * which have not responded are considered
   * to have failed, and failure handling
   * begins.
   */
  jam();

  /* Preconditions, otherwise we shouldn't have been called */
  ndbrequire(m_connectivity_check.getEnabled());
  ndbrequire(m_connectivity_check.m_active);
  ndbrequire(!m_connectivity_check.m_nodesWaiting.isclear());

  m_connectivity_check.m_tick++;

  switch (m_connectivity_check.m_tick)
  {
  case CC_SuspectTicks:
  {
    jam();
    /* Still waiting to hear from some nodes, they are now
     * suspect
     */
    m_connectivity_check.m_nodesSuspect.bitOR(m_connectivity_check.m_nodesWaiting);
    return;
  }
  case CC_FailedTicks:
  {
    jam();
    /* Still waiting to hear from some nodes, they will now
     * be failed
     */
    m_connectivity_check.m_active = false;
    Uint32 nodeId = 0;

    while ((nodeId = m_connectivity_check.m_nodesWaiting.find(nodeId))
           != BitmaskImpl::NotFound)
    {
      jam();
      /* Log failure reason */
      /* Todo : Connectivity Check specific failure log? */
      signal->theData[0] = NDB_LE_DeadDueToHeartbeat;
      signal->theData[1] = nodeId;

      sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 2, JBB);

      /* Fail the node */
      /* TODO : Consider real time break here */
      failReportLab(signal, nodeId, FailRep::ZCONNECT_CHECK_FAILURE, getOwnNodeId());
      nodeId++;
    }

    /* Now handle the end of the Connectivity Check */
    connectivityCheckCompleted(signal);
  }
  }
}

bool
Qmgr::isNodeConnectivitySuspect(Uint32 nodeId) const
{
  return m_connectivity_check.m_nodesSuspect.get(nodeId);
}

void
Qmgr::handleFailFromSuspect(Signal* signal,
                            Uint32 reason,
                            Uint16 aFailedNode,
                            Uint16 sourceNode)
{
  jam();

  const char* reasonText = "Unknown";

  /* We have received a failure report about some node X from
   * some other node that we consider to have suspect connectivity
   * which may have caused the report.
   *
   * We will 'invert' the sense of this, and handle it as
   * a failure report of the sender, with the same cause.
   */
  switch(reason)
  {
  case FailRep::ZCONNECT_CHECK_FAILURE:
    jam();
    /* Suspect says that connectivity check failed for another node.
     * As suspect has bad connectivity from our point of view, we
     * blame him.
     */
    reasonText = "ZCONNECT_CHECK_FAILURE";
    break;
  case FailRep::ZLINK_FAILURE:
    jam();
    /* Suspect says that link failed for another node.
     * As suspect has bad connectivity from our point of view, we
     * blame her.
     */
    reasonText = "ZLINK_FAILURE";
    break;
  default:
    ndbabort();
  }

  g_eventLogger->warning("QMGR : Received Connectivity failure notification about "
                         "%u from suspect node %u with reason %s.  "
                         "Mapping to failure of %u sourced by me.",
                         aFailedNode, sourceNode, reasonText, sourceNode);

  signal->theData[0] = NDB_LE_NodeFailRejected;
  signal->theData[1] = reason;
  signal->theData[2] = aFailedNode;
  signal->theData[3] = sourceNode;

  sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 4, JBB);

  failReportLab(signal, sourceNode, (FailRep::FailCause) reason, getOwnNodeId());
}

ProcessInfo *
Qmgr::getProcessInfo(Uint32 nodeId)
{
  ProcessInfo * storedProcessInfo = 0;
  Int16 index = processInfoNodeIndex[nodeId];
  if(index >= 0)
    storedProcessInfo = & receivedProcessInfo[index];
  else if(nodeId == getOwnNodeId())
    storedProcessInfo = getOwnProcessInfo(getOwnNodeId());
  return storedProcessInfo;
}

void
Qmgr::execDBINFO_SCANREQ(Signal *signal)
{
  DbinfoScanReq req= *(DbinfoScanReq*)signal->theData;
  Ndbinfo::Ratelimit rl;

  jamEntry();
  switch(req.tableId) {
  case Ndbinfo::MEMBERSHIP_TABLEID:
  {
    jam();
    Ndbinfo::Row row(signal, req);
    row.write_uint32(getOwnNodeId());
    row.write_uint32(getNodeState().nodeGroup);
    row.write_uint32(cneighbourl);
    row.write_uint32(cneighbourh);
    row.write_uint32(cpresident);

    // President successor
    Uint32 successor = 0;
    {
      NodeRecPtr nodePtr;
      UintR minDynamicId = (UintR)-1;
      for (nodePtr.i = 1; nodePtr.i < MAX_NDB_NODES; nodePtr.i++)
      {
        jam();
        ptrAss(nodePtr, nodeRec);
        if (nodePtr.p->phase == ZRUNNING)
        {
          if ((nodePtr.p->ndynamicId & 0xFFFF) < minDynamicId)
          {
            jam();
            if (cpresident !=  nodePtr.i)
            {
              minDynamicId = (nodePtr.p->ndynamicId & 0xFFFF);
              successor = nodePtr.i;
            }
          }
        }
      }
    }
    row.write_uint32(successor);

    NodeRecPtr myNodePtr;
    myNodePtr.i = getOwnNodeId();
    ptrCheckGuard(myNodePtr, MAX_NDB_NODES, nodeRec);
    row.write_uint32(myNodePtr.p->ndynamicId);

    row.write_uint32(arbitRec.node); // arbitrator

    char ticket[20]; // Need 16 characters + 1 for trailing '\0'
    arbitRec.ticket.getText(ticket, sizeof(ticket));
    row.write_string(ticket);

    row.write_uint32(arbitRec.state);

    // arbitrator connected
    row.write_uint32(c_connectedNodes.get(arbitRec.node));

    // Find potential (rank1 and rank2) arbitrators that are connected.
    NodeRecPtr aPtr;
    // buf_size: Node nr (max 3 chars) and ', '  + trailing '\0'
    const int buf_size = 5 * MAX_NODES + 1;
    char buf[buf_size];

    for (unsigned rank = 1; rank <= 2; rank++)
    {
      jam();
      aPtr.i = 0;
      const unsigned stop = NodeBitmask::NotFound;
      int buf_offset = 0;
      const char* delimiter = "";

      while ((aPtr.i = arbitRec.apiMask[rank].find(aPtr.i + 1)) != stop)
      {
        jam();
        ptrAss(aPtr, nodeRec);
        if (c_connectedNodes.get(aPtr.i))
        {
          buf_offset += BaseString::snprintf(buf + buf_offset,
                                             buf_size - buf_offset,
                                             "%s%u", delimiter, aPtr.i);
          delimiter = ", ";
        }
      }

      if (buf_offset == 0)
        row.write_string("-");
      else
        row.write_string(buf);
    }

    ndbinfo_send_row(signal, req, row, rl);
    break;
  }
  case Ndbinfo::PROCESSES_TABLEID:
  {
    jam();
    for(int i = 1 ; i <= max_api_node_id ; i++)
    {
      NodeInfo nodeInfo = getNodeInfo(i);
      if(nodeInfo.m_connected)
      {
        char version_buffer[NDB_VERSION_STRING_BUF_SZ];
        ndbGetVersionString(nodeInfo.m_version, nodeInfo.m_mysql_version,
                            0, version_buffer, NDB_VERSION_STRING_BUF_SZ);

        ProcessInfo *processInfo = getProcessInfo(i);
        if(processInfo && processInfo->isValid())
        {
          char uri_buffer[512];
          processInfo->getServiceUri(uri_buffer, sizeof(uri_buffer));
          Ndbinfo::Row row(signal, req);
          row.write_uint32(getOwnNodeId());                 // reporting_node_id
          row.write_uint32(i);                              // node_id
          row.write_uint32(nodeInfo.getType());             // node_type
          row.write_string(version_buffer);                 // node_version
          row.write_uint32(processInfo->getPid());          // process_id
          row.write_uint32(processInfo->getAngelPid());     // angel_process_id
          row.write_string(processInfo->getProcessName());  // process_name
          row.write_string(uri_buffer);                     // service_URI
          ndbinfo_send_row(signal, req, row, rl);
        }
        else if(nodeInfo.m_type != NodeInfo::DB)
        {
          /* MGM/API node is an older version or has not sent ProcessInfoRep */

          struct in_addr addr= globalTransporterRegistry.get_connect_address(i);
          char service_uri[32];
          strcpy(service_uri, "ndb://");
          Ndb_inet_ntop(AF_INET, & addr, service_uri + 6, 24);

          Ndbinfo::Row row(signal, req);
          row.write_uint32(getOwnNodeId());                 // reporting_node_id
          row.write_uint32(i);                              // node_id
          row.write_uint32(nodeInfo.getType());             // node_type
          row.write_string(version_buffer);                 // node_version
          row.write_uint32(0);                              // process_id
          row.write_uint32(0);                              // angel_process_id
          row.write_string("");                             // process_name
          row.write_string(service_uri);                    // service_URI
          ndbinfo_send_row(signal, req, row, rl);
        }
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
Qmgr::execPROCESSINFO_REP(Signal *signal)
{
  jamEntry();
  ProcessInfoRep * report = (ProcessInfoRep *) signal->theData;
  SectionHandle handle(this, signal);
  SegmentedSectionPtr pathSectionPtr, hostSectionPtr;

  ProcessInfo * processInfo = getProcessInfo(report->node_id);
  if(processInfo)
  {
    /* Set everything except the connection name and host address */
    processInfo->initializeFromProcessInfoRep(report);

    /* Set the URI path */
    if(handle.getSection(pathSectionPtr, ProcessInfoRep::PathSectionNum))
    {
      processInfo->setUriPath(pathSectionPtr.p->theData);
    }

    /* Set the host address */
    if(handle.getSection(hostSectionPtr, ProcessInfoRep::HostSectionNum))
    {
      processInfo->setHostAddress(hostSectionPtr.p->theData);
    }
    else
    {
      /* Use the address from the transporter registry.
         As implemented below we use setHostAddress() with struct in_addr
         to set an IPv4 address.  An alternate more abstract version
         of ProcessInfo::setHostAddress() is also available, which
         takes a struct sockaddr * and length.
      */
      struct in_addr addr=
        globalTransporterRegistry.get_connect_address(report->node_id);
      processInfo->setHostAddress(& addr);
    }
  }
  releaseSections(handle);
}

void
Qmgr::execISOLATE_ORD(Signal* signal)
{
  jamEntry();
  
  IsolateOrd* sig = (IsolateOrd*) signal->theData;
  
  ndbrequire(sig->senderRef != 0);
  NdbNodeBitmask victims;
  victims.assign(NdbNodeBitmask::Size, sig->nodesToIsolate);
  ndbrequire(!victims.isclear());

  switch (sig->isolateStep)
  {
  case IsolateOrd::IS_REQ:
  {
    jam();
    /* Initial request, broadcast immediately */

    /* Need to get the set of live nodes to broadcast to */
    NdbNodeBitmask hitmen(c_clusterNodes);

    unsigned nodeId = hitmen.find_first();
    do
    {
      jam();
      if (!ndbd_isolate_ord(getNodeInfo(nodeId).m_version))
      {
        jam();
        /* Node not able to handle ISOLATE_ORD, skip */
        hitmen.clear(nodeId);
      }

      nodeId = hitmen.find_next(nodeId + 1);
    } while (nodeId != BitmaskImpl::NotFound);

    ndbrequire(!hitmen.isclear()); /* At least me */

    NodeReceiverGroup rg(QMGR, hitmen);

    sig->isolateStep = IsolateOrd::IS_BROADCAST;
    sendSignal(rg, GSN_ISOLATE_ORD, signal, IsolateOrd::SignalLength, JBA);
    return;
  }
  case IsolateOrd::IS_BROADCAST:
  {
    jam();
    /* Received reqest, delay */
    sig->isolateStep = IsolateOrd::IS_DELAY;
    
    if (sig->delayMillis > 0)
    {
      /* Delay processing until delayMillis passes */
      jam();
      sendSignalWithDelay(reference(), 
                          GSN_ISOLATE_ORD, 
                          signal, 
                          sig->delayMillis, 
                          IsolateOrd::SignalLength);
      return;
    }
  }
  // Fall through
  case IsolateOrd::IS_DELAY:
  {
    jam();

    if (ERROR_INSERTED(942))
    {
      jam();
      g_eventLogger->info("QMGR discarding IsolateRequest");
      return;
    }

    /* Map to FAIL_REP signal(s) */
    Uint32 failSource = refToNode(sig->senderRef);

    unsigned nodeId = victims.find_first();
    do
    {
      jam();

      /* TODO : Consider checking node state and skipping if
       * failing already
       * Consider logging that action is being taken here
       */

      FailRep* failRep = (FailRep*)&signal->theData[0];
      failRep->failNodeId = nodeId;
      failRep->failCause = FailRep::ZFORCED_ISOLATION;
      failRep->failSourceNodeId = failSource;

      sendSignal(reference(), GSN_FAIL_REP, signal, 3, JBA);
      
      nodeId = victims.find_next(nodeId + 1);
    } while (nodeId != BitmaskImpl::NotFound);
    
    /* Fail rep signals are en-route... */
    
    return;
  }
  }

  ndbabort();
}

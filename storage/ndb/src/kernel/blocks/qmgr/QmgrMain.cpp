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


#define QMGR_C
#include "Qmgr.hpp"
#include <pc.hpp>
#include <NdbTick.h>
#include <signaldata/EventReport.hpp>
#include <signaldata/StartOrd.hpp>
#include <signaldata/CmInit.hpp>
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

#include <ndb_version.h>

#ifdef DEBUG_ARBIT
#include <NdbOut.hpp>
#endif

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
  setNodeInfo(hbNodePtr.i).m_heartbeat_cnt= 0;
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
    failReportLab(signal, c_start.m_startNode, FailRep::ZSTART_IN_REGREQ);
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
    Uint64 now = NdbTick_CurrentMillisecond();

    if (now > (c_start_election_time + c_restartFailureTimeout))
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
  
  jamEntry();
  failReportLab(signal, failNodeId, failCause);
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
  signal->theData[0] = ZTIMER_HANDLING;
  sendSignal(QMGR_REF, GSN_CONTINUEB, signal, 1, JBB);

  NodeRecPtr nodePtr;
  for (nodePtr.i = 1; nodePtr.i < MAX_NODES; nodePtr.i++) 
  {
    ptrAss(nodePtr, nodeRec);
    nodePtr.p->ndynamicId = 0;	
    if(getNodeInfo(nodePtr.i).m_type == NodeInfo::DB)
    {
      nodePtr.p->phase = ZINIT;
      c_definedNodes.set(nodePtr.i);
    } else {
      nodePtr.p->phase = ZAPI_INACTIVE;
    }
    
    setNodeInfo(nodePtr.i).m_heartbeat_cnt= 0;
    nodePtr.p->sendPrepFailReqStatus = Q_NOT_ACTIVE;
    nodePtr.p->sendCommitFailReqStatus = Q_NOT_ACTIVE;
    nodePtr.p->sendPresToStatus = Q_NOT_ACTIVE;
    nodePtr.p->failState = NORMAL;
    nodePtr.p->rcv[0] = 0;
    nodePtr.p->rcv[1] = 0;
  }//for
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
    startphase1(signal);
    recompute_version_info(NodeInfo::DB);
    recompute_version_info(NodeInfo::API);
    recompute_version_info(NodeInfo::MGM);
    return;
  case 7:
    cactivateApiCheck = 1;
    /**
     * Start arbitration thread.  This could be done as soon as
     * we have all nodes (or a winning majority).
     */
    if (cpresident == getOwnNodeId())
      handleArbitStart(signal);
    break;
  }
  
  sendSttorryLab(signal);
  return;
}//Qmgr::execSTTOR()

void Qmgr::sendSttorryLab(Signal* signal) 
{
/****************************<*/
/*< STTORRY                  <*/
/****************************<*/
  signal->theData[3] = 7;
  signal->theData[4] = 255;
  sendSignal(NDBCNTR_REF, GSN_STTORRY, signal, 5, JBB);
  return;
}//Qmgr::sendSttorryLab()

void Qmgr::startphase1(Signal* signal) 
{
  jamEntry();

  
  NodeRecPtr nodePtr;
  nodePtr.i = getOwnNodeId();
  ptrAss(nodePtr, nodeRec);
  nodePtr.p->phase = ZSTARTING;
  
  signal->theData[0] = reference();
  sendSignal(DBDIH_REF, GSN_DIH_RESTARTREQ, signal, 1, JBB);
  return;
}

void
Qmgr::execDIH_RESTARTREF(Signal*signal)
{
  jamEntry();

  c_start.m_latest_gci = 0;
  execCM_INFOCONF(signal);
}

void
Qmgr::execDIH_RESTARTCONF(Signal*signal)
{
  jamEntry();
  
  c_start.m_latest_gci = signal->theData[1];
  execCM_INFOCONF(signal);
}

void Qmgr::setHbDelay(UintR aHbDelay)
{
  hb_send_timer.setDelay(aHbDelay < 10 ? 10 : aHbDelay);
  hb_send_timer.reset();
  hb_check_timer.setDelay(aHbDelay < 10 ? 10 : aHbDelay);
  hb_check_timer.reset();
}

void Qmgr::setHbApiDelay(UintR aHbApiDelay)
{
  chbApiDelay = (aHbApiDelay < 100 ? 100 : aHbApiDelay);
  hb_api_timer.setDelay(chbApiDelay);
  hb_api_timer.reset();
}

void Qmgr::setArbitTimeout(UintR aArbitTimeout)
{
  arbitRec.timeout = (aArbitTimeout < 10 ? 10 : aArbitTimeout);
}

void Qmgr::execCONNECT_REP(Signal* signal)
{
  jamEntry();
  const Uint32 nodeId = signal->theData[0];

  if (ERROR_INSERTED(931))
  {
    jam();
    ndbout_c("Discarding CONNECT_REP(%d)", nodeId);
    infoEvent("Discarding CONNECT_REP(%d)", nodeId);
    return;
  }
  
  c_connectedNodes.set(nodeId);
  NodeRecPtr nodePtr;
  nodePtr.i = getOwnNodeId();
  ptrCheckGuard(nodePtr, MAX_NODES, nodeRec);
  switch(nodePtr.p->phase){
  case ZRUNNING:
    ndbrequire(!c_clusterNodes.get(nodeId));
  case ZSTARTING:
    jam();
    break;
  case ZPREPARE_FAIL:
  case ZFAIL_CLOSING:
    jam();
    return;
  case ZAPI_ACTIVE:
  case ZAPI_INACTIVE:
    return;
  case ZINIT:
    ndbrequire(getNodeInfo(nodeId).m_type == NodeInfo::MGM);
    break;
  default:
    ndbrequire(false);
  }

  if (getNodeInfo(nodeId).getType() != NodeInfo::DB)
  {
    jam();
    return;
  }

  switch(c_start.m_gsn){
  case GSN_CM_REGREQ:
    jam();
    sendCmRegReq(signal, nodeId);

    /**
     * We're waiting for CM_REGCONF c_start.m_nodes contains all configured
     *   nodes
     */
    ndbrequire(nodePtr.p->phase == ZSTARTING);
    ndbrequire(c_start.m_nodes.isWaitingFor(nodeId));
    return;
  case GSN_CM_NODEINFOREQ:
    jam();

    if (c_start.m_nodes.isWaitingFor(nodeId))
    {
      jam();
      ndbrequire(getOwnNodeId() != cpresident);
      ndbrequire(nodePtr.p->phase == ZSTARTING);
      sendCmNodeInfoReq(signal, nodeId, nodePtr.p);
      return;
    }
    return;
  case GSN_CM_NODEINFOCONF:{
    jam();
    
    ndbrequire(getOwnNodeId() != cpresident);
    ndbrequire(nodePtr.p->phase == ZRUNNING);
    if (c_start.m_nodes.isWaitingFor(nodeId))
    {
      jam();
      c_start.m_nodes.clearWaitingFor(nodeId);
      c_start.m_gsn = RNIL;
      
      NodeRecPtr addNodePtr;
      addNodePtr.i = nodeId;
      ptrCheckGuard(addNodePtr, MAX_NDB_NODES, nodeRec);
      cmAddPrepare(signal, addNodePtr, nodePtr.p);
      return;
    }
  }
  default:
    (void)1;
  }
  
  ndbrequire(!c_start.m_nodes.isWaitingFor(nodeId));
  ndbrequire(!c_readnodes_nodes.get(nodeId));
  c_readnodes_nodes.set(nodeId);
  signal->theData[0] = reference();
  sendSignal(calcQmgrBlockRef(nodeId), GSN_READ_NODESREQ, signal, 1, JBA);
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
  sendSignal(CMVMI_REF, GSN_OPEN_COMREQ, signal, 3, JBB);

  cpresident = ZNIL;
  cpresidentAlive = ZFALSE;
  c_start_election_time = NdbTick_CurrentMillisecond();
  
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
 * Only the president have the authority make decissions about new nodes, 
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
  addNodePtr.i = cmRegReq->nodeId;
  Uint32 gci = 1;
  Uint32 start_type = ~0;
  NdbNodeBitmask skip_nodes;

  if (signal->getLength() == CmRegReq::SignalLength)
  {
    jam();
    gci = cmRegReq->latest_gci;
    start_type = cmRegReq->start_type;
    skip_nodes.assign(NdbNodeBitmask::Size, cmRegReq->skip_nodes);
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
       * than our president cancidate. Set it as 
       * candidate
       */
      jam(); 
      if (gci > c_start.m_president_candidate_gci || 
	  (gci == c_start.m_president_candidate_gci && 
	   addNodePtr.i < c_start.m_president_candidate))
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
  UintR TdynId = ++c_maxDynamicId;
  setNodeInfo(addNodePtr.i).m_version = startingVersion;
  recompute_version_info(NodeInfo::DB, startingVersion);
  addNodePtr.p->ndynamicId = TdynId;
  
  /**
   * Reply with CM_REGCONF
   */
  CmRegConf * const cmRegConf = (CmRegConf *)&signal->theData[0];
  cmRegConf->presidentBlockRef = reference();
  cmRegConf->presidentNodeId   = getOwnNodeId();
  cmRegConf->presidentVersion  = getNodeInfo(getOwnNodeId()).m_version;
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
    systemErrorLab(signal, __LINE__, buf);
    return;
  }

  myNodePtr.i = getOwnNodeId();
  ptrCheckGuard(myNodePtr, MAX_NDB_NODES, nodeRec);
  
  ndbrequire(c_start.m_gsn == GSN_CM_REGREQ);
  ndbrequire(myNodePtr.p->phase == ZSTARTING);
  
  cpdistref    = cmRegConf->presidentBlockRef;
  cpresident   = cmRegConf->presidentNodeId;
  UintR TdynamicId   = cmRegConf->dynamicId;
  c_maxDynamicId = TdynamicId;
  c_clusterNodes.assign(NdbNodeBitmask::Size, cmRegConf->allNdbNodes);

  myNodePtr.p->ndynamicId = TdynamicId;
  
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
    jam();
    if (c_clusterNodes.get(nodePtr.i)){
      jam();
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

  ndbout_c(buf);
  CRASH_INSERTION(933);

  if (getNodeState().startLevel == NodeState::SL_STARTED)
  {
    jam();
    NdbNodeBitmask part;
    part.assign(NdbNodeBitmask::Size, conf->clusterNodes);
    FailRep* rep = (FailRep*)signal->getDataPtrSend();
    rep->failCause = FailRep::ZPARTITIONED_CLUSTER;
    rep->president = cpresident;
    c_clusterNodes.copyto(NdbNodeBitmask::Size, rep->partition);
    Uint32 ref = calcQmgrBlockRef(nodeId);
    Uint32 i = 0;
    while((i = part.find(i + 1)) != NdbNodeBitmask::NotFound)
    {
      if (i == nodeId)
	continue;
      rep->failNodeId = i;
      sendSignal(ref, GSN_FAIL_REP, signal, FailRep::SignalLength, JBA);
    }
    rep->failNodeId = nodeId;
    sendSignal(ref, GSN_FAIL_REP, signal, FailRep::SignalLength, JBB);
    return;
  }
  
  CRASH_INSERTION(932);
  
  progError(__LINE__, 
	    NDBD_EXIT_PARTITIONED_SHUTDOWN,
	    buf);
  
  ndbrequire(false);
}

void
Qmgr::sendCmNodeInfoReq(Signal* signal, Uint32 nodeId, const NodeRec * self){
  CmNodeInfoReq * const req = (CmNodeInfoReq*)signal->getDataPtrSend();
  req->nodeId = getOwnNodeId();
  req->dynamicId = self->ndynamicId;
  req->version = getNodeInfo(getOwnNodeId()).m_version;
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
  if (node_gci)
  {
    jam();
    c_start.m_starting_nodes_w_log.set(TaddNodeno);
  }
  c_start.m_node_gci[TaddNodeno] = node_gci;

  skip_nodes.bitAND(c_definedNodes);
  c_start.m_skip_nodes.bitOR(skip_nodes);
  
  char buf[100];
  switch (TrefuseReason) {
  case CmRegRef::ZINCOMPATIBLE_VERSION:
    jam();
    systemErrorLab(signal, __LINE__, 
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
    if (candidate_gci > c_start.m_president_candidate_gci ||
	(candidate_gci == c_start.m_president_candidate_gci &&
	 candidate < c_start.m_president_candidate))
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
  Uint64 now = NdbTick_CurrentMillisecond();
  Uint64 partial_timeout = c_start_election_time + c_restartPartialTimeout;
  Uint64 partitioned_timeout = partial_timeout + c_restartPartionedTimeout;

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
    if (!tmp.equal(c_definedNodes))
    {
      jam();
      signal->theData[1] = 1;
      signal->theData[2] = ~0;
      report_mask.assign(wait);
      retVal = 0;
      goto start_report;
    }
    else
    {
      jam();
      signal->theData[1] = 0x8000;
      report_mask.assign(c_definedNodes);
      report_mask.bitANDC(c_start.m_starting_nodes);
      retVal = 1;
      goto start_report;
    }
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
      check.bitANDC(c_start.m_starting_nodes);    // Not connected nodes
      check.bitOR(c_start.m_starting_nodes_w_log);
 
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

    if (now < partial_timeout)
    {
      jam();
      signal->theData[1] = c_restartPartialTimeout == (Uint32) ~0 ? 2 : 3;
      signal->theData[2] = Uint32((partial_timeout - now + 500) / 1000);
      report_mask.assign(wait);
      retVal = 0;
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
      if (now < partitioned_timeout && result != CheckNodeGroups::Win)
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
  ndbrequire(false);

check_log:
  jam();
  {
    Uint32 save[4+4*NdbNodeBitmask::Size];
    memcpy(save, signal->theData, sizeof(save));
    
    signal->theData[0] = 0;
    c_start.m_starting_nodes.copyto(NdbNodeBitmask::Size, signal->theData+1);
    memcpy(signal->theData+1+NdbNodeBitmask::Size, c_start.m_node_gci,
	   4*MAX_NDB_NODES);
    EXECUTE_DIRECT(DBDIH, GSN_DIH_RESTARTREQ, signal, 
		   1+NdbNodeBitmask::Size+MAX_NDB_NODES);
    
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
	if (now <= partitioned_timeout)
	{
	  jam();
	  goto missinglog;
	}
	else
	{
	  goto incomplete_log;
	}
      }
      ndbrequire(false);
    }
  }
  goto start_report;

missinglog:
  signal->theData[1] = c_restartPartionedTimeout == (Uint32) ~0 ? 4 : 5;
  signal->theData[2] = Uint32((partitioned_timeout - now + 500) / 1000);
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
    sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 
	       4+4*NdbNodeBitmask::Size, JBB);
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
			 " starting: %s (missing fs for: %s)",
			 mask1, mask2);
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
    progError(__LINE__, NDBD_EXIT_INSUFFICENT_NODES, buf);
    return 0;                                     // Deadcode
  }
}

void
Qmgr::electionWon(Signal* signal){
  NodeRecPtr myNodePtr;
  cpresident = getOwnNodeId(); /* This node becomes president. */
  myNodePtr.i = getOwnNodeId();
  ptrCheckGuard(myNodePtr, MAX_NDB_NODES, nodeRec);
  
  myNodePtr.p->phase = ZRUNNING;

  cpdistref = reference();
  cneighbourl = ZNIL;
  cneighbourh = ZNIL;
  myNodePtr.p->ndynamicId = 1;
  c_maxDynamicId = 1;
  c_clusterNodes.clear();
  c_clusterNodes.set(getOwnNodeId());
  
  cpresidentAlive = ZTRUE;
  c_start_election_time = ~0;
  c_start.reset();

  signal->theData[0] = NDB_LE_CM_REGCONF;
  signal->theData[1] = getOwnNodeId();
  signal->theData[2] = cpresident;
  signal->theData[3] = 1;
  sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 4, JBB);

  c_start.m_starting_nodes.clear(getOwnNodeId());
  if (c_start.m_starting_nodes.isclear())
  {
    jam();
    sendSttorryLab(signal);
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
  c_maxDynamicId = req->dynamicId;

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
    warningEvent("Recieved request to incorperate node %u, "
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
    sendSignal(CMVMI_REF, GSN_OPEN_COMREQ, signal, 2, JBA);
#endif
    return;
  case ZSTARTING:
    break;
  case ZRUNNING:
  case ZPREPARE_FAIL:
  case ZAPI_ACTIVE:
  case ZAPI_INACTIVE:
    ndbrequire(false);
  }
  
  sendCmAckAdd(signal, nodePtr.i, CmAdd::Prepare);
  sendApiVersionRep(signal, nodePtr);

  /* President have prepared us */
  CmNodeInfoConf * conf = (CmNodeInfoConf*)signal->getDataPtrSend();
  conf->nodeId = getOwnNodeId();
  conf->dynamicId = self->ndynamicId;
  conf->version = getNodeInfo(getOwnNodeId()).m_version;
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
      ndbrequire(false);
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
    setNodeInfo(addNodePtr.i).m_heartbeat_cnt= 0;
    c_clusterNodes.set(addNodePtr.i);
    findNeighbours(signal);

    /**
     * SEND A HEARTBEAT IMMEDIATELY TO DECREASE THE RISK THAT WE MISS EARLY
     * HEARTBEATS. 
     */
    sendHeartbeat(signal);

    /**
     *  ENABLE COMMUNICATION WITH ALL BLOCKS WITH THE NEWLY ADDED NODE
     */
    signal->theData[0] = addNodePtr.i;
    sendSignal(CMVMI_REF, GSN_ENABLE_COMORD, signal, 1, JBA);

    sendCmAckAdd(signal, addNodePtr.i, CmAdd::AddCommit);
    if(getOwnNodeId() != cpresident){
      jam();
      c_start.reset();
    }
    break;
  }
  case CmAdd::CommitNew:
    jam();
    ndbrequire(false);
  }

}//Qmgr::execCM_ADD()

void
Qmgr::joinedCluster(Signal* signal, NodeRecPtr nodePtr){
  /**
   * WE HAVE BEEN INCLUDED IN THE CLUSTER WE CAN START BEING PART OF THE 
   * HEARTBEAT PROTOCOL AND WE WILL ALSO ENABLE COMMUNICATION WITH ALL 
   * NODES IN THE CLUSTER.
   */
  nodePtr.p->phase = ZRUNNING;
  setNodeInfo(nodePtr.i).m_heartbeat_cnt= 0;
  findNeighbours(signal);
  c_clusterNodes.set(nodePtr.i);
  c_start.reset();

  /**
   * SEND A HEARTBEAT IMMEDIATELY TO DECREASE THE RISK 
   * THAT WE MISS EARLY HEARTBEATS. 
   */
  sendHeartbeat(signal);

  /**
   * ENABLE COMMUNICATION WITH ALL BLOCKS IN THE CURRENT CLUSTER AND SET 
   * THE NODES IN THE CLUSTER TO BE RUNNING. 
   */
  for (nodePtr.i = 1; nodePtr.i < MAX_NDB_NODES; nodePtr.i++) {
    jam();
    ptrAss(nodePtr, nodeRec);
    if ((nodePtr.p->phase == ZRUNNING) && (nodePtr.i != getOwnNodeId())) {
      /*-------------------------------------------------------------------*/
      // Enable full communication to all other nodes. Not really necessary 
      // to open communication to ourself.
      /*-------------------------------------------------------------------*/
      jam();
      signal->theData[0] = nodePtr.i;
      sendSignal(CMVMI_REF, GSN_ENABLE_COMORD, signal, 1, JBA);
    }//if
  }//for
  
  sendSttorryLab(signal);
  
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
    
    sendSignal(calcQmgrBlockRef(addNodePtr.i), GSN_CM_ADD, signal, 
	       CmAdd::SignalLength, JBA);
    DEBUG_START(GSN_CM_ADD, addNodePtr.i, "CommitNew");
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
	sendSttorryLab(signal);
      }
    }
    return;
  }//switch
  ndbrequire(false);
}//Qmgr::execCM_ACKADD()

/**-------------------------------------------------------------------------
 * WE HAVE BEEN INCLUDED INTO THE CLUSTER. IT IS NOW TIME TO CALCULATE WHICH 
 * ARE OUR LEFT AND RIGHT NEIGHBOURS FOR THE HEARTBEAT PROTOCOL. 
 *--------------------------------------------------------------------------*/
void Qmgr::findNeighbours(Signal* signal) 
{
  UintR toldLeftNeighbour;
  UintR tfnLeftFound;
  UintR tfnMaxFound;
  UintR tfnMinFound;
  UintR tfnRightFound;
  NodeRecPtr fnNodePtr;
  NodeRecPtr fnOwnNodePtr;

  toldLeftNeighbour = cneighbourl;
  tfnLeftFound = 0;
  tfnMaxFound = 0;
  tfnMinFound = (UintR)-1;
  tfnRightFound = (UintR)-1;
  fnOwnNodePtr.i = getOwnNodeId();
  ptrCheckGuard(fnOwnNodePtr, MAX_NDB_NODES, nodeRec);
  for (fnNodePtr.i = 1; fnNodePtr.i < MAX_NDB_NODES; fnNodePtr.i++) {
    jam();
    ptrAss(fnNodePtr, nodeRec);
    if (fnNodePtr.i != fnOwnNodePtr.i) {
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
      setNodeInfo(fnNodePtr.i).m_heartbeat_cnt= 0;
    }//if
  }//if

  signal->theData[0] = NDB_LE_FIND_NEIGHBOURS;
  signal->theData[1] = getOwnNodeId();
  signal->theData[2] = cneighbourl;
  signal->theData[3] = cneighbourh;
  signal->theData[4] = fnOwnNodePtr.p->ndynamicId;
  UintR Tlen = 5;
  for (fnNodePtr.i = 1; fnNodePtr.i < MAX_NDB_NODES; fnNodePtr.i++) {
    jam();
    ptrAss(fnNodePtr, nodeRec);
    if (fnNodePtr.i != fnOwnNodePtr.i) {
      if (fnNodePtr.p->phase == ZRUNNING) {
        jam();
        signal->theData[Tlen] = fnNodePtr.i;
        signal->theData[Tlen + 1] = fnNodePtr.p->ndynamicId;
        if (Tlen < 25) {
	  /*----------------------------------------------------------------*/
	  // This code can only report 11 nodes. 
	  // We need to update this when increasing the number of nodes
	  // supported.
	  /*-----------------------------------------------------------------*/
          Tlen += 2;
        }
      }//if
    }//if
  }//for
  sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, Tlen, JBB);
}//Qmgr::findNeighbours()

/*
4.10.7 INIT_DATA        */
/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
void Qmgr::initData(Signal* signal) 
{
  cfailureNr = 1;
  ccommitFailureNr = 1;
  cprepareFailureNr = 1;
  cnoFailedNodes = 0;
  cnoPrepFailedNodes = 0;
  creadyDistCom = ZFALSE;
  cpresident = ZNIL;
  c_start.m_president_candidate = ZNIL;
  c_start.m_president_candidate_gci = 0;
  cpdistref = 0;
  cneighbourh = ZNIL;
  cneighbourl = ZNIL;
  cdelayRegreq = ZDELAY_REGREQ;
  cactivateApiCheck = 0;
  ctoStatus = Q_NOT_ACTIVE;

  interface_check_timer.setDelay(1000);
  interface_check_timer.reset();
  clatestTransactionCheck = 0;

  cLqhTimeSignalCount = 0;

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
  c_restartPartialTimeout = 30000;
  c_restartPartionedTimeout = 60000;
  c_restartFailureTimeout = ~0;
  ndb_mgm_get_int_parameter(p, CFG_DB_HEARTBEAT_INTERVAL, &hbDBDB);
  ndb_mgm_get_int_parameter(p, CFG_DB_ARBIT_TIMEOUT, &arbitTimeout);
  ndb_mgm_get_int_parameter(p, CFG_DB_START_PARTIAL_TIMEOUT, 
			    &c_restartPartialTimeout);
  ndb_mgm_get_int_parameter(p, CFG_DB_START_PARTITION_TIMEOUT,
			    &c_restartPartionedTimeout);
  ndb_mgm_get_int_parameter(p, CFG_DB_START_FAILURE_TIMEOUT,
			    &c_restartFailureTimeout);
 
  if(c_restartPartialTimeout == 0)
  {
    c_restartPartialTimeout = ~0;
  }
  
  if (c_restartPartionedTimeout ==0)
  {
    c_restartPartionedTimeout = ~0;
  }
  
  if (c_restartFailureTimeout == 0)
  {
    c_restartFailureTimeout = ~0;
  }

  setHbDelay(hbDBDB);
  setArbitTimeout(arbitTimeout);
  
  arbitRec.state = ARBIT_NULL;          // start state for all nodes
  arbitRec.apiMask[0].clear();          // prepare for ARBIT_CFG

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
    
    execARBIT_CFG(signal);
  }
}//Qmgr::initData()


/**---------------------------------------------------------------------------
 * HERE WE RECEIVE THE JOB TABLE SIGNAL EVERY 10 MILLISECONDS. 
 * WE WILL USE THIS TO CHECK IF IT IS TIME TO CHECK THE NEIGHBOUR NODE. 
 * WE WILL ALSO SEND A SIGNAL TO BLOCKS THAT NEED A TIME SIGNAL AND 
 * DO NOT WANT TO USE JOB TABLE SIGNALS.
 *---------------------------------------------------------------------------*/
void Qmgr::timerHandlingLab(Signal* signal) 
{
  NDB_TICKS TcurrentTime = NdbTick_CurrentMillisecond();
  NodeRecPtr myNodePtr;
  myNodePtr.i = getOwnNodeId();
  ptrCheckGuard(myNodePtr, MAX_NDB_NODES, nodeRec);

  if (myNodePtr.p->phase == ZRUNNING) {
    jam();
    /**---------------------------------------------------------------------
     * WE ARE ONLY PART OF HEARTBEAT CLUSTER IF WE ARE UP AND RUNNING. 
     *---------------------------------------------------------------------*/
    if (hb_send_timer.check(TcurrentTime)) {
      jam();
      sendHeartbeat(signal);
      hb_send_timer.reset();
    }
    if (hb_check_timer.check(TcurrentTime)) {
      jam();
      checkHeartbeat(signal);
      hb_check_timer.reset();
    }
  }
  
  if (interface_check_timer.check(TcurrentTime)) {
    jam();
    interface_check_timer.reset();
    checkStartInterface(signal);
  }

  if (hb_api_timer.check(TcurrentTime)) 
  {
    jam();
    hb_api_timer.reset();
    apiHbHandlingLab(signal);
  }

  if (cactivateApiCheck != 0) {
    jam();
    if (clatestTransactionCheck == 0) {
      //-------------------------------------------------------------
      // Initialise the Transaction check timer.
      //-------------------------------------------------------------
      clatestTransactionCheck = TcurrentTime;
    }//if
    int counter = 0;
    while (TcurrentTime > ((NDB_TICKS)10 + clatestTransactionCheck)) {
      jam();
      clatestTransactionCheck += (NDB_TICKS)10;
      sendSignal(DBTC_REF, GSN_TIME_SIGNAL, signal, 1, JBB);
      cLqhTimeSignalCount++;
      if (cLqhTimeSignalCount >= 100) {
	cLqhTimeSignalCount = 0;
	sendSignal(DBLQH_REF, GSN_TIME_SIGNAL, signal, 1, JBB);          
      }//if
      counter++;
      if (counter > 1) {
	jam();
	break;
      } else {
	;
      }//if
    }//while
  }//if
  
  //--------------------------------------------------
  // Resend this signal with 10 milliseconds delay.
  //--------------------------------------------------
  signal->theData[0] = ZTIMER_HANDLING;
  sendSignalWithDelay(QMGR_REF, GSN_CONTINUEB, signal, 10, 1);
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
  
  setNodeInfo(nodePtr.i).m_heartbeat_cnt++;
  ndbrequire(nodePtr.p->phase == ZRUNNING);
  ndbrequire(getNodeInfo(nodePtr.i).m_type == NodeInfo::DB);

  if(getNodeInfo(nodePtr.i).m_heartbeat_cnt > 2){
    signal->theData[0] = NDB_LE_MissedHeartbeat;
    signal->theData[1] = nodePtr.i;
    signal->theData[2] = getNodeInfo(nodePtr.i).m_heartbeat_cnt - 1;
    sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 3, JBB);
  }

  if (getNodeInfo(nodePtr.i).m_heartbeat_cnt > 4) {
    jam();
    /**----------------------------------------------------------------------
     * OUR LEFT NEIGHBOUR HAVE KEPT QUIET FOR THREE CONSECUTIVE HEARTBEAT 
     * PERIODS. THUS WE DECLARE HIM DOWN.
     *----------------------------------------------------------------------*/
    signal->theData[0] = NDB_LE_DeadDueToHeartbeat;
    signal->theData[1] = nodePtr.i;
    sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 2, JBB);

    failReportLab(signal, nodePtr.i, FailRep::ZHEARTBEAT_FAILURE);
    return;
  }//if
}//Qmgr::checkHeartbeat()

void Qmgr::apiHbHandlingLab(Signal* signal) 
{
  NodeRecPtr TnodePtr;

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
      jam();
      setNodeInfo(TnodePtr.i).m_heartbeat_cnt++;
      
      if(getNodeInfo(TnodePtr.i).m_heartbeat_cnt > 2)
      {
	signal->theData[0] = NDB_LE_MissedHeartbeat;
	signal->theData[1] = nodeId;
	signal->theData[2] = getNodeInfo(TnodePtr.i).m_heartbeat_cnt - 1;
	sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 3, JBB);
      }
      
      if (getNodeInfo(TnodePtr.i).m_heartbeat_cnt > 4) 
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
  }//for
  return;
}//Qmgr::apiHbHandlingLab()

void Qmgr::checkStartInterface(Signal* signal) 
{
  NodeRecPtr nodePtr;
  /*------------------------------------------------------------------------*/
  // This method is called once per second. After a disconnect we wait at 
  // least three seconds before allowing new connects. We will also ensure 
  // that handling of the failure is completed before we allow new connections.
  /*------------------------------------------------------------------------*/
  for (nodePtr.i = 1; nodePtr.i < MAX_NODES; nodePtr.i++) {
    ptrAss(nodePtr, nodeRec);
    if (nodePtr.p->phase == ZFAIL_CLOSING) {
      jam();
      setNodeInfo(nodePtr.i).m_heartbeat_cnt++;
      if (c_connectedNodes.get(nodePtr.i)){
        jam();
	/*-------------------------------------------------------------------*/
	// We need to ensure that the connection is not restored until it has 
	// been disconnected for at least three seconds.
	/*-------------------------------------------------------------------*/
        setNodeInfo(nodePtr.i).m_heartbeat_cnt= 0;
      }//if
      if ((getNodeInfo(nodePtr.i).m_heartbeat_cnt > 3)
	  && (nodePtr.p->failState == NORMAL)) {
	/**------------------------------------------------------------------
	 * WE HAVE DISCONNECTED THREE SECONDS AGO. WE ARE NOW READY TO 
	 * CONNECT AGAIN AND ACCEPT NEW REGISTRATIONS FROM THIS NODE. 
	 * WE WILL NOT ALLOW CONNECTIONS OF API NODES UNTIL API FAIL HANDLING 
	 * IS COMPLETE.
	 *-------------------------------------------------------------------*/
        nodePtr.p->failState = NORMAL;
        if (getNodeInfo(nodePtr.i).m_type != NodeInfo::DB){
          jam();
          nodePtr.p->phase = ZAPI_INACTIVE;
        } else {
          jam();
          nodePtr.p->phase = ZINIT;
        }//if

        setNodeInfo(nodePtr.i).m_heartbeat_cnt= 0;
        signal->theData[0] = 0;
        signal->theData[1] = nodePtr.i;
        sendSignal(CMVMI_REF, GSN_OPEN_COMREQ, signal, 2, JBA);
      } else {
	if(((getNodeInfo(nodePtr.i).m_heartbeat_cnt + 1) % 60) == 0){
	  char buf[100];
	  BaseString::snprintf(buf, sizeof(buf), 
		   "Failure handling of node %d has not completed in %d min."
		   " - state = %d",
		   nodePtr.i, 
		   (getNodeInfo(nodePtr.i).m_heartbeat_cnt + 1)/60,
		   nodePtr.p->failState);
	  warningEvent(buf);
	}
      }
    }//if
  }//for
  return;
}//Qmgr::checkStartInterface()

/**-------------------------------------------------------------------------
 * This method is called when a DISCONNECT_REP signal arrived which means that
 * the API node is gone and we want to release resources in TC/DICT blocks.
 *---------------------------------------------------------------------------*/
void Qmgr::sendApiFailReq(Signal* signal, Uint16 failedNodeNo) 
{
  NodeRecPtr failedNodePtr;

  jamEntry();
  failedNodePtr.i = failedNodeNo;
  signal->theData[0] = failedNodePtr.i;
  signal->theData[1] = QMGR_REF; 

  ptrCheckGuard(failedNodePtr, MAX_NODES, nodeRec);
  
  ndbrequire(failedNodePtr.p->failState == NORMAL);
  
  failedNodePtr.p->failState = WAITING_FOR_FAILCONF1;
  NodeReceiverGroup rg(QMGR, c_clusterNodes);
  sendSignal(rg, GSN_API_FAILREQ, signal, 2, JBA);
  sendSignal(DBTC_REF, GSN_API_FAILREQ, signal, 2, JBA);
  sendSignal(DBDICT_REF, GSN_API_FAILREQ, signal, 2, JBA);
  sendSignal(SUMA_REF, GSN_API_FAILREQ, signal, 2, JBA);
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

  if (failedNodePtr.p->failState == WAITING_FOR_FAILCONF1){
    jam();

    failedNodePtr.p->rcv[0] = signal->theData[1];
    failedNodePtr.p->failState = WAITING_FOR_FAILCONF2;

  } else if (failedNodePtr.p->failState == WAITING_FOR_FAILCONF2) {
    failedNodePtr.p->rcv[1] = signal->theData[1];
    failedNodePtr.p->failState = NORMAL;

    if (failedNodePtr.p->rcv[0] == failedNodePtr.p->rcv[1]) {
      jam();
      systemErrorLab(signal, __LINE__);
    } else {
      jam();
      failedNodePtr.p->rcv[0] = 0;
      failedNodePtr.p->rcv[1] = 0;
    }//if
  } else {
    jam();
#ifdef VM_TRACE
    ndbout << "failedNodePtr.p->failState = "
	   << (Uint32)(failedNodePtr.p->failState) << endl;
#endif   
    systemErrorLab(signal, __LINE__);
  }//if
  return;
}//Qmgr::execAPI_FAILCONF()

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
  
  ptrCheckGuard(failedNodePtr, MAX_NODES, nodeRec);
  if (failedNodePtr.p->failState == WAITING_FOR_NDB_FAILCONF){
    failedNodePtr.p->failState = NORMAL;
  } else {
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
  if (cpresident == getOwnNodeId()) {
    jam();
    /** 
     * Prepare a NFCompleteRep and send to all connected API's
     * They can then abort all transaction waiting for response from 
     * the failed node
     */
    NFCompleteRep * const nfComp = (NFCompleteRep *)&signal->theData[0];
    nfComp->blockNo = QMGR_REF;
    nfComp->nodeId = getOwnNodeId();
    nfComp->failedNodeId = failedNodePtr.i;

    for (nodePtr.i = 1; nodePtr.i < MAX_NODES; nodePtr.i++) {
      jam();
      ptrAss(nodePtr, nodeRec);
      if (nodePtr.p->phase == ZAPI_ACTIVE){
        jam();
        sendSignal(nodePtr.p->blockRef, GSN_NF_COMPLETEREP, signal, 
                   NFCompleteRep::SignalLength, JBA);
      }//if
    }//for
  }
  return;
}//Qmgr::execNDB_FAILCONF()

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
  c_connectedNodes.clear(nodeId);
  c_readnodes_nodes.clear(nodeId);
  
  NodeRecPtr nodePtr;
  nodePtr.i = getOwnNodeId();
  ptrCheckGuard(nodePtr, MAX_NODES, nodeRec);

  char buf[100];
  if (getNodeInfo(nodeId).getType() == NodeInfo::DB &&
      getNodeState().startLevel < NodeState::SL_STARTED)
  {
    jam();
    CRASH_INSERTION(932);
    BaseString::snprintf(buf, 100, "Node %u disconnected", nodeId);    
    progError(__LINE__, NDBD_EXIT_SR_OTHERNODEFAILED, buf);
    ndbrequire(false);
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
    ndbrequire(false);
  case ZSTARTING:
    progError(__LINE__, NDBD_EXIT_CONNECTION_SETUP_FAILED,
	      lookupConnectionError(err));
    ndbrequire(false);
  case ZPREPARE_FAIL:
    ndbrequire(false);
  case ZFAIL_CLOSING:
    ndbrequire(false);
  case ZAPI_ACTIVE:
    ndbrequire(false);
  case ZAPI_INACTIVE:
  {
    BaseString::snprintf(buf, 100, "Node %u disconnected", nodeId);    
    progError(__LINE__, NDBD_EXIT_SR_OTHERNODEFAILED, buf);
    ndbrequire(false);
  }
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

  ndbrequire(getNodeInfo(failedNodePtr.i).getType() == NodeInfo::DB);
  
  /**---------------------------------------------------------------------
   *   THE OTHER NODE IS AN NDB NODE, WE HANDLE IT AS IF A HEARTBEAT 
   *   FAILURE WAS DISCOVERED.
   *---------------------------------------------------------------------*/
  switch(failedNodePtr.p->phase){
  case ZRUNNING:
    jam();
    failReportLab(signal, aFailedNode, FailRep::ZLINK_FAILURE);
    return;
  case ZFAIL_CLOSING:
    jam();
    return;
  case ZSTARTING:
    c_start.reset();
    // Fall-through
  default:
    jam();
    /*---------------------------------------------------------------------*/
    // The other node is still not in the cluster but disconnected. 
    // We must restart communication in three seconds.
    /*---------------------------------------------------------------------*/
    failedNodePtr.p->failState = NORMAL;
    failedNodePtr.p->phase = ZFAIL_CLOSING;
    setNodeInfo(failedNodePtr.i).m_heartbeat_cnt= 0;

    CloseComReqConf * const closeCom = 
      (CloseComReqConf *)&signal->theData[0];

    closeCom->xxxBlockRef = reference();
    closeCom->failNo      = 0;
    closeCom->noOfNodes   = 1;
    NodeBitmask::clear(closeCom->theNodes);
    NodeBitmask::set(closeCom->theNodes, failedNodePtr.i);
    sendSignal(CMVMI_REF, GSN_CLOSE_COMREQ, signal, 
               CloseComReqConf::SignalLength, JBA);
  }//if
  return;
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
  
  if (failedNodePtr.p->phase == ZFAIL_CLOSING)
  {
    /**
     * Failure handling already in progress
     */
    jam();
    return;
  }

  if (failedNodePtr.p->phase == ZAPI_ACTIVE)
  {
    jam();
    sendApiFailReq(signal, nodeId);
    arbitRec.code = ArbitCode::ApiFail;
    handleArbitApiFail(signal, nodeId);
  }
  else
  {
    /**
     * Always inform SUMA
     */
    jam();
    signal->theData[0] = nodeId;
    signal->theData[1] = QMGR_REF;
    sendSignal(SUMA_REF, GSN_API_FAILREQ, signal, 2, JBA);
    failedNodePtr.p->failState = NORMAL;
  }

  failedNodePtr.p->phase = ZFAIL_CLOSING;
  setNodeInfo(failedNodePtr.i).m_heartbeat_cnt= 0;
  setNodeInfo(failedNodePtr.i).m_version = 0;
  recompute_version_info(getNodeInfo(failedNodePtr.i).m_type);
  
  CloseComReqConf * const closeCom = (CloseComReqConf *)&signal->theData[0];
  closeCom->xxxBlockRef = reference();
  closeCom->failNo      = 0;
  closeCom->noOfNodes   = 1;
  NodeBitmask::clear(closeCom->theNodes);
  NodeBitmask::set(closeCom->theNodes, failedNodePtr.i);
  sendSignal(CMVMI_REF, GSN_CLOSE_COMREQ, signal, 
             CloseComReqConf::SignalLength, JBA);

  if (getNodeInfo(failedNodePtr.i).getType() == NodeInfo::MGM)
  {
    /**
     * Allow MGM do reconnect "directly"
     */
    jam();
    setNodeInfo(failedNodePtr.i).m_heartbeat_cnt = 3;
  }
}

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
  
  NodeRecPtr apiNodePtr;
  apiNodePtr.i = refToNode(ref);
  ptrCheckGuard(apiNodePtr, MAX_NODES, nodeRec);
  
#if 0
  ndbout_c("Qmgr::execAPI_REGREQ: Recd API_REGREQ (NodeId=%d)", apiNodePtr.i);
#endif

  bool compatability_check;
  NodeInfo::NodeType type= getNodeInfo(apiNodePtr.i).getType();
  switch(type){
  case NodeInfo::API:
    compatability_check = ndbCompatible_ndb_api(NDB_VERSION, version);
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
	      "incompatible with %s",
	      type == NodeInfo::API ? "api or mysqld" : "management server",
	      apiNodePtr.i,
	      ndbGetVersionString(version,"",buf,sizeof(buf)),
	      NDB_VERSION_STRING);
    apiNodePtr.p->phase = ZAPI_INACTIVE;
    sendApiRegRef(signal, ref, ApiRegRef::UnsupportedVersion);
    return;
  }

  setNodeInfo(apiNodePtr.i).m_version = version;
  setNodeInfo(apiNodePtr.i).m_heartbeat_cnt= 0;

  ApiRegConf * const apiRegConf = (ApiRegConf *)&signal->theData[0];
  apiRegConf->qmgrRef = reference();
  apiRegConf->apiHeartbeatFrequency = (chbApiDelay / 10);
  apiRegConf->version = NDB_VERSION;
  NodeState state= apiRegConf->nodeState = getNodeState();
  {
    NodeRecPtr nodePtr;
    nodePtr.i = getOwnNodeId();
    ptrCheckGuard(nodePtr, MAX_NDB_NODES, nodeRec);
    Uint32 dynamicId = nodePtr.p->ndynamicId;

    if(apiRegConf->nodeState.masterNodeId != getOwnNodeId()){
      jam();
      apiRegConf->nodeState.dynamicId = dynamicId;
    } else {
      apiRegConf->nodeState.dynamicId = -dynamicId;
    }
  }
  NodeVersionInfo info = getNodeVersionInfo();
  apiRegConf->minDbVersion = info.m_type[NodeInfo::DB].m_min_version;
  apiRegConf->nodeState.m_connected_nodes.assign(c_connectedNodes);
  sendSignal(ref, GSN_API_REGCONF, signal, ApiRegConf::SignalLength, JBB);

  if (apiNodePtr.p->phase == ZAPI_INACTIVE &&
      (state.startLevel == NodeState::SL_STARTED ||
       state.getSingleUserMode() ||
       (state.startLevel == NodeState::SL_STARTING && 
	state.starting.startPhase >= 100)))
  {       
    jam();
    /**----------------------------------------------------------------------
     * THE API NODE IS REGISTERING. WE WILL ACCEPT IT BY CHANGING STATE AND 
     * SENDING A CONFIRM. 
     *----------------------------------------------------------------------*/
    apiNodePtr.p->phase = ZAPI_ACTIVE;
    apiNodePtr.p->blockRef = ref;
    signal->theData[0] = apiNodePtr.i;
    sendSignal(CMVMI_REF, GSN_ENABLE_COMORD, signal, 1, JBA);
    
    recompute_version_info(type, version);
    
    signal->theData[0] = apiNodePtr.i;
    signal->theData[1] = version;
    NodeReceiverGroup rg(QMGR, c_clusterNodes);
    rg.m_nodes.clear(getOwnNodeId());
    sendVersionedDb(rg, GSN_NODE_VERSION_REP, signal, 2, JBB, 
		    NDBD_NODE_VERSION_REP);
    
    signal->theData[0] = apiNodePtr.i;
    EXECUTE_DIRECT(NDBCNTR, GSN_API_START_REP, signal, 1);
  }
  return;
}//Qmgr::execAPI_REGREQ()

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
    conf->version =  getNodeInfo(nodeId).m_version;
    struct in_addr in= globalTransporterRegistry.get_connect_address(nodeId);
    conf->inet_addr= in.s_addr;
  }
  else
  {
    conf->version =  0;
    conf->inet_addr= 0;
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
  ref->errorCode = err;
  sendSignal(Tref, GSN_API_REGREF, signal, ApiRegRef::SignalLength, JBB);
}

/**--------------------------------------------------------------------------
 * A NODE HAS BEEN DECLARED AS DOWN. WE WILL CLOSE THE COMMUNICATION TO THIS 
 * NODE IF NOT ALREADY DONE. IF WE ARE PRESIDENT OR BECOMES PRESIDENT BECAUSE 
 * OF A FAILED PRESIDENT THEN WE WILL TAKE FURTHER ACTION. 
 *---------------------------------------------------------------------------*/
void Qmgr::failReportLab(Signal* signal, Uint16 aFailedNode,
			 FailRep::FailCause aFailCause) 
{
  NodeRecPtr nodePtr;
  NodeRecPtr failedNodePtr;
  NodeRecPtr myNodePtr;
  UintR TnoFailedNodes;

  failedNodePtr.i = aFailedNode;
  ptrCheckGuard(failedNodePtr, MAX_NODES, nodeRec);
  FailRep* rep = (FailRep*)signal->getDataPtr();

  if (check_multi_node_shutdown(signal))
  {
    jam();
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
      msg = "Hearbeat failure";
      break;
    case FailRep::ZLINK_FAILURE:
      msg = "Connection failure";
      break;
    case FailRep::ZPARTITIONED_CLUSTER:
    {
      code = NDBD_EXIT_PARTITIONED_SHUTDOWN;
      char buf1[100], buf2[100];
      c_clusterNodes.getText(buf1);
      if (signal->getLength()== FailRep::SignalLength + FailRep::ExtraLength &&
	  signal->header.theVerId_signalNumber == GSN_FAIL_REP)
      {
	jam();
	NdbNodeBitmask part;
	part.assign(NdbNodeBitmask::Size, rep->partition);
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
    default:
      msg = "<UNKNOWN>";
    }
    
    CRASH_INSERTION(932);

    char buf[255];
    BaseString::snprintf(buf, sizeof(buf), 
			 "We(%u) have been declared dead by %u reason: %s(%u)",
			 getOwnNodeId(),
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
    char buf[100];
    BaseString::snprintf(buf, 100, "Node failure during restart");
    progError(__LINE__, NDBD_EXIT_SR_OTHERNODEFAILED, buf);
    ndbrequire(false);
  }

  TnoFailedNodes = cnoFailedNodes;
  failReport(signal, failedNodePtr.i, (UintR)ZTRUE, aFailCause);
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
      if (TnoFailedNodes != cnoFailedNodes) {
        jam();
        cfailureNr = cfailureNr + 1;
        for (nodePtr.i = 1;
             nodePtr.i < MAX_NDB_NODES; nodePtr.i++) {
          jam();
          ptrAss(nodePtr, nodeRec);
          if (nodePtr.p->phase == ZRUNNING) {
            jam();
            sendPrepFailReq(signal, nodePtr.i);
          }//if
        }//for
      }//if
    }//if
  }//if
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
  jamEntry();
  
  if (check_multi_node_shutdown(signal))
  {
    jam();
    return;
  }
  
  PrepFailReqRef * const prepFail = (PrepFailReqRef *)&signal->theData[0];

  BlockReference Tblockref  = prepFail->xxxBlockRef;
  Uint16 TfailureNr = prepFail->failNo;
  cnoPrepFailedNodes = prepFail->noOfNodes;
  UintR arrayIndex = 0;
  Uint32 Tindex;
  for (Tindex = 0; Tindex < MAX_NDB_NODES; Tindex++) {
    if (NodeBitmask::get(prepFail->theNodes, Tindex)){
      cprepFailedNodes[arrayIndex] = Tindex;
      arrayIndex++;
    }//if
  }//for
  UintR guard0;

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
    char buf[100];
    BaseString::snprintf(buf, 100, "Node failure during restart");
    progError(__LINE__, NDBD_EXIT_SR_OTHERNODEFAILED, buf);
    ndbrequire(false);
  }

  guard0 = cnoPrepFailedNodes - 1;
  arrGuard(guard0, MAX_NDB_NODES);
  for (Tindex = 0; Tindex <= guard0; Tindex++) {
    jam();
    failReport(signal,
               cprepFailedNodes[Tindex],
               (UintR)ZFALSE,
               FailRep::ZIN_PREP_FAIL_REQ);
  }//for
  sendCloseComReq(signal, Tblockref, TfailureNr);
  cnoCommitFailedNodes = 0;
  cprepareFailureNr = TfailureNr;
  return;
}//Qmgr::execPREP_FAILREQ()

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

  BlockReference Tblockref  = closeCom->xxxBlockRef;
  Uint16 TfailureNr = closeCom->failNo;

  cnoPrepFailedNodes = closeCom->noOfNodes;
  UintR arrayIndex = 0;
  UintR Tindex = 0;
  for(Tindex = 0; Tindex < MAX_NDB_NODES; Tindex++){
    if(NodeBitmask::get(closeCom->theNodes, Tindex)){
      cprepFailedNodes[arrayIndex] = Tindex;
      arrayIndex++;
    }
  }
  UintR tprepFailConf;
  UintR Tindex2;
  UintR guard0;
  UintR guard1;
  UintR Tfound;
  Uint16 TfailedNodeNo;

  tprepFailConf = ZTRUE;
  if (cnoFailedNodes > 0) {
    jam();
    guard0 = cnoFailedNodes - 1;
    arrGuard(guard0, MAX_NDB_NODES);
    for (Tindex = 0; Tindex <= guard0; Tindex++) {
      jam();
      TfailedNodeNo = cfailedNodes[Tindex];
      Tfound = ZFALSE;
      guard1 = cnoPrepFailedNodes - 1;
      arrGuard(guard1, MAX_NDB_NODES);
      for (Tindex2 = 0; Tindex2 <= guard1; Tindex2++) {
        jam();
        if (TfailedNodeNo == cprepFailedNodes[Tindex2]) {
          jam();
          Tfound = ZTRUE;
        }//if
      }//for
      if (Tfound == ZFALSE) {
        jam();
        tprepFailConf = ZFALSE;
        arrGuard(cnoPrepFailedNodes, MAX_NDB_NODES);
        cprepFailedNodes[cnoPrepFailedNodes] = TfailedNodeNo;
        cnoPrepFailedNodes = cnoPrepFailedNodes + 1;
      }//if
    }//for
  }//if
  if (tprepFailConf == ZFALSE) {
    jam();
    for (Tindex = 1; Tindex < MAX_NDB_NODES; Tindex++) {
      cfailedNodes[Tindex] = cprepFailedNodes[Tindex];
    }//for
    cnoFailedNodes = cnoPrepFailedNodes;
    sendPrepFailReqRef(signal,
		       Tblockref,
		       GSN_PREP_FAILREF,
		       reference(),
		       cfailureNr,
		       cnoPrepFailedNodes,
		       cprepFailedNodes);
  } else {
    jam();
    cnoCommitFailedNodes = cnoPrepFailedNodes;
    guard0 = cnoPrepFailedNodes - 1;
    arrGuard(guard0, MAX_NDB_NODES);
    for (Tindex = 0; Tindex <= guard0; Tindex++) {
      jam();
      arrGuard(Tindex, MAX_NDB_NODES);
      ccommitFailedNodes[Tindex] = cprepFailedNodes[Tindex];
    }//for
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
    jam();
    ptrAss(nodePtr, nodeRec);
    if (nodePtr.p->phase == ZRUNNING) {
      if (nodePtr.p->sendPrepFailReqStatus == Q_ACTIVE) {
        jam();
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
  handleArbitCheck(signal);
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
    jam();
    ptrAss(nodePtr, nodeRec);

#ifdef ERROR_INSERT    
    if (ERROR_INSERTED(935) && nodePtr.i == c_error_insert_extra)
    {
      ndbout_c("skipping node %d", c_error_insert_extra);
      CLEAR_ERROR_INSERT_VALUE;
      signal->theData[0] = 9999;
      sendSignalWithDelay(CMVMI_REF, GSN_NDB_TAMPER, signal, 1000, 1);
      continue;
    }
#endif

    if (nodePtr.p->phase == ZRUNNING) {
      jam();
      nodePtr.p->sendCommitFailReqStatus = Q_ACTIVE;
      signal->theData[0] = cpdistref;
      signal->theData[1] = cfailureNr;
      sendSignal(nodePtr.p->blockRef, GSN_COMMIT_FAILREQ, signal, 2, JBA);
    }//if
  }//for
  ctoStatus = Q_ACTIVE;
  cnoFailedNodes = 0;
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
  cnoPrepFailedNodes = prepFail->noOfNodes;

  UintR arrayIndex = 0;
  UintR Tindex = 0;
  for(Tindex = 0; Tindex < MAX_NDB_NODES; Tindex++) {
    jam();
    if(NodeBitmask::get(prepFail->theNodes, Tindex)){
      jam();
      cprepFailedNodes[arrayIndex] = Tindex;
      arrayIndex++;
    }//if
  }//for
  if (TfailureNr != cfailureNr) {
    jam();
    /**---------------------------------------------------------------------
     * WE HAVE ALREADY STARTING A NEW ATTEMPT TO EXCLUDE A NUMBER OF NODES. 
     *  IGNORE
     *----------------------------------------------------------------------*/
    return;
  }//if
  UintR guard0;
  UintR Ti;

  cnoFailedNodes = cnoPrepFailedNodes;
  guard0 = cnoPrepFailedNodes - 1;
  arrGuard(guard0, MAX_NDB_NODES);
  for (Ti = 0; Ti <= guard0; Ti++) {
    jam();
    cfailedNodes[Ti] = cprepFailedNodes[Ti];
  }//for
  cfailureNr = cfailureNr + 1;
  for (nodePtr.i = 1; nodePtr.i < MAX_NDB_NODES; nodePtr.i++) {
    jam();
    ptrAss(nodePtr, nodeRec);
    if (nodePtr.p->phase == ZRUNNING) {
      jam();
      sendPrepFailReq(signal, nodePtr.i);
    }//if
  }//for
  return;
}//Qmgr::execPREP_FAILREF()

static
Uint32
clear_nodes(Uint32 dstcnt, Uint16 dst[], Uint32 srccnt, const Uint16 src[])
{
  if (srccnt == 0)
    return dstcnt;
  
  Uint32 pos = 0;
  for (Uint32 i = 0; i<dstcnt; i++)
  {
    Uint32 node = dst[i];
    for (Uint32 j = 0; j<srccnt; j++)
    {
      if (node == dst[j])
      {
	node = RNIL;
	break;
      }
    }
    if (node != RNIL)
    {
      dst[pos++] = node;
    }
  }
  return pos;
}

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
  BlockReference Tblockref = signal->theData[0];
  UintR TfailureNr = signal->theData[1];
  if (Tblockref != cpdistref) {
    jam();
    return;
  }//if
  UintR guard0;
  UintR Tj;

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
      (cnoCommitFailedNodes > 0)) {
    jam();
    /**-----------------------------------------------------------------------
     * WE ONLY DO THIS PART OF THE COMMIT HANDLING THE FIRST TIME WE HEAR THIS
     * SIGNAL. WE CAN HEAR IT SEVERAL TIMES IF THE PRESIDENTS KEEP FAILING.
     *-----------------------------------------------------------------------*/
    ccommitFailureNr = TfailureNr;
    NodeFailRep * const nodeFail = (NodeFailRep *)&signal->theData[0];
    
    nodeFail->failNo    = ccommitFailureNr;
    nodeFail->noOfNodes = cnoCommitFailedNodes;
    nodeFail->masterNodeId = cpresident;
    NodeBitmask::clear(nodeFail->theNodes);
    for(unsigned i = 0; i < cnoCommitFailedNodes; i++) {
      jam();
      NodeBitmask::set(nodeFail->theNodes, ccommitFailedNodes[i]);
    }//if	
    sendSignal(NDBCNTR_REF, GSN_NODE_FAILREP, signal, 
	       NodeFailRep::SignalLength, JBB);

    guard0 = cnoCommitFailedNodes - 1;
    arrGuard(guard0, MAX_NDB_NODES);
    /**--------------------------------------------------------------------
     * WE MUST PREPARE TO ACCEPT THE CRASHED NODE INTO THE CLUSTER AGAIN BY 
     * SETTING UP CONNECTIONS AGAIN AFTER THREE SECONDS OF DELAY.
     *--------------------------------------------------------------------*/
    for (Tj = 0; Tj <= guard0; Tj++) {
      jam();
      nodePtr.i = ccommitFailedNodes[Tj];
      ptrCheckGuard(nodePtr, MAX_NDB_NODES, nodeRec);
      nodePtr.p->phase = ZFAIL_CLOSING;
      nodePtr.p->failState = WAITING_FOR_NDB_FAILCONF;
      setNodeInfo(nodePtr.i).m_heartbeat_cnt= 0;
      setNodeInfo(nodePtr.i).m_version = 0;
      c_clusterNodes.clear(nodePtr.i);
    }//for
    recompute_version_info(NodeInfo::DB);
    /*----------------------------------------------------------------------*/
    /*       WE INFORM THE API'S WE HAVE CONNECTED ABOUT THE FAILED NODES.  */
    /*----------------------------------------------------------------------*/
    for (nodePtr.i = 1; nodePtr.i < MAX_NODES; nodePtr.i++) {
      jam();
      ptrAss(nodePtr, nodeRec);
      if (nodePtr.p->phase == ZAPI_ACTIVE) {
        jam();

	NodeFailRep * const nodeFail = (NodeFailRep *)&signal->theData[0];

	nodeFail->failNo    = ccommitFailureNr;
	nodeFail->noOfNodes = cnoCommitFailedNodes;
	NodeBitmask::clear(nodeFail->theNodes);
	for(unsigned i = 0; i < cnoCommitFailedNodes; i++) {
          jam();
	  NodeBitmask::set(nodeFail->theNodes, ccommitFailedNodes[i]);
        }//for	
        sendSignal(nodePtr.p->blockRef, GSN_NODE_FAILREP, signal, 
		   NodeFailRep::SignalLength, JBB);
      }//if
    }//for

    /**
     * Remove committed nodes from failed/prepared
     */
    cnoFailedNodes = clear_nodes(cnoFailedNodes, 
				 cfailedNodes, 
				 cnoCommitFailedNodes, 
				 ccommitFailedNodes);
    cnoPrepFailedNodes = clear_nodes(cnoPrepFailedNodes, 
				     cprepFailedNodes,
				     cnoCommitFailedNodes,
				     ccommitFailedNodes);
    cnoCommitFailedNodes = 0;
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
    jam();
    ptrAss(nodePtr, nodeRec);
    if (nodePtr.p->phase == ZRUNNING) {
      if (nodePtr.p->sendCommitFailReqStatus == Q_ACTIVE) {
        jam();
        return;
      }//if
    }//if
  }//for
  /*-----------------------------------------------------------------------*/
  /*   WE HAVE SUCCESSFULLY COMMITTED A SET OF NODE FAILURES.              */
  /*-----------------------------------------------------------------------*/
  ctoStatus = Q_NOT_ACTIVE;
  if (cnoFailedNodes != 0) {
    jam();
    /**----------------------------------------------------------------------
     *	A FAILURE OCCURRED IN THE MIDDLE OF THE COMMIT PROCESS. WE ARE NOW 
     *  READY TO START THE FAILED NODE PROCESS FOR THIS NODE.
     *----------------------------------------------------------------------*/
    cfailureNr = cfailureNr + 1;
    for (nodePtr.i = 1; nodePtr.i < MAX_NDB_NODES; nodePtr.i++) {
      jam();
      ptrAss(nodePtr, nodeRec);
      if (nodePtr.p->phase == ZRUNNING) {
        jam();
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
    jam();
    ptrAss(nodePtr, nodeRec);
    if (nodePtr.p->sendPresToStatus == Q_ACTIVE) {
      jam();
      return;
    }//if
  }//for
  /*-------------------------------------------------------------------------*/
  /* WE ARE NOW READY TO DISCOVER WHETHER THE FAILURE WAS COMMITTED OR NOT.  */
  /*-------------------------------------------------------------------------*/
  if (ctoFailureNr > ccommitFailureNr) {
    jam();
    for (nodePtr.i = 1; nodePtr.i < MAX_NDB_NODES; nodePtr.i++) {
      jam();
      ptrAss(nodePtr, nodeRec);
      if (nodePtr.p->phase == ZRUNNING) {
        jam();
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
    jam();
    ptrAss(nodePtr, nodeRec);
    if (nodePtr.p->phase == ZRUNNING) {
      jam();
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
  failReport(signal, getOwnNodeId(), (UintR)ZTRUE, FailRep::ZOWN_FAILURE);

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
  failReport(signal, getOwnNodeId(), (UintR)ZTRUE, FailRep::ZOWN_FAILURE);

  // If it's known why shutdown occured
  // an error message has been passed to this function
  progError(line, NDBD_EXIT_NDBREQUIRE, message);  

  return;
}//Qmgr::systemErrorLab()


/**---------------------------------------------------------------------------
 * A FAILURE HAVE BEEN DISCOVERED ON A NODE. WE NEED TO CLEAR A 
 * NUMBER OF VARIABLES.
 *---------------------------------------------------------------------------*/
void Qmgr::failReport(Signal* signal,
                      Uint16 aFailedNode,
                      UintR aSendFailRep,
                      FailRep::FailCause aFailCause) 
{
  UintR tfrMinDynamicId;
  NodeRecPtr failedNodePtr;
  NodeRecPtr nodePtr;
  NodeRecPtr presidentNodePtr;


  failedNodePtr.i = aFailedNode;
  ptrCheckGuard(failedNodePtr, MAX_NDB_NODES, nodeRec);
  if (failedNodePtr.p->phase == ZRUNNING) {
    jam();
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
    setNodeInfo(failedNodePtr.i).m_heartbeat_cnt= 0;
    if (aSendFailRep == ZTRUE) {
      jam();
      if (failedNodePtr.i != getOwnNodeId()) {
        jam();
	FailRep * const failRep = (FailRep *)&signal->theData[0];
        failRep->failNodeId = failedNodePtr.i;
        failRep->failCause = aFailCause;
        sendSignal(failedNodePtr.p->blockRef, GSN_FAIL_REP, signal, 
		   FailRep::SignalLength, JBA);
      }//if
      for (nodePtr.i = 1; nodePtr.i < MAX_NDB_NODES; nodePtr.i++) {
        jam();
        ptrAss(nodePtr, nodeRec);
        if (nodePtr.p->phase == ZRUNNING) {
          jam();
	  FailRep * const failRep = (FailRep *)&signal->theData[0];
	  failRep->failNodeId = failedNodePtr.i;
	  failRep->failCause = aFailCause;
          sendSignal(nodePtr.p->blockRef, GSN_FAIL_REP, signal, 
		     FailRep::SignalLength, JBA);
        }//if
      }//for
    }//if
    if (failedNodePtr.i == getOwnNodeId()) {
      jam();
      return;
    }//if
    failedNodePtr.p->ndynamicId = 0;
    findNeighbours(signal);
    if (failedNodePtr.i == cpresident) {
      jam();
      /**--------------------------------------------------------------------
       * IF PRESIDENT HAVE FAILED WE MUST CALCULATE THE NEW PRESIDENT BY 
       * FINDING THE NODE WITH THE MINIMUM DYNAMIC IDENTITY.
       *---------------------------------------------------------------------*/
      tfrMinDynamicId = (UintR)-1;
      for (nodePtr.i = 1; nodePtr.i < MAX_NDB_NODES; nodePtr.i++) {
        jam();
        ptrAss(nodePtr, nodeRec);
        if (nodePtr.p->phase == ZRUNNING) {
          if (nodePtr.p->ndynamicId < tfrMinDynamicId) {
            jam();
            tfrMinDynamicId = nodePtr.p->ndynamicId;
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
        if (cnoCommitFailedNodes > 0) {
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
            jam();
            ptrAss(nodePtr, nodeRec);
            if (nodePtr.p->phase == ZRUNNING) {
              jam();
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
    arrGuard(cnoFailedNodes, MAX_NDB_NODES);
    cfailedNodes[cnoFailedNodes] = failedNodePtr.i;
    cnoFailedNodes = cnoFailedNodes + 1;
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
  closeCom->failNo      = aFailNo;
  closeCom->noOfNodes   = cnoPrepFailedNodes;
  
  NodeBitmask::clear(closeCom->theNodes);

  for(int i = 0; i < cnoPrepFailedNodes; i++) {
    const NodeId nodeId = cprepFailedNodes[i];
    jam();
    NodeBitmask::set(closeCom->theNodes, nodeId);
  }

  sendSignal(CMVMI_REF, GSN_CLOSE_COMREQ, signal, 
	     CloseComReqConf::SignalLength, JBA);

}//Qmgr::sendCloseComReq()

void 
Qmgr::sendPrepFailReqRef(Signal* signal, 
			 Uint32 dstBlockRef,
			 GlobalSignalNumber gsn,
			 Uint32 blockRef,
			 Uint32 failNo,
			 Uint32 noOfNodes,
			 const NodeId theNodes[]){

  PrepFailReqRef * const prepFail = (PrepFailReqRef *)&signal->theData[0];
  prepFail->xxxBlockRef = blockRef;
  prepFail->failNo = failNo;
  prepFail->noOfNodes = noOfNodes;

  NodeBitmask::clear(prepFail->theNodes);
  
  for(Uint32 i = 0; i<noOfNodes; i++){
    const NodeId nodeId = theNodes[i];
    NodeBitmask::set(prepFail->theNodes, nodeId);
  }

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
		     cnoFailedNodes,
		     cfailedNodes);
}//Qmgr::sendPrepFailReq()

/**
 * Arbitration module.  Rest of QMGR calls us only via
 * the "handle" routines.
 */

/**
 * Should < 1/2 nodes die unconditionally.  Affects only >= 3-way
 * replication.
 */
static const bool g_ndb_arbit_one_half_rule = false;

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
  arbitRec.apiMask[rank] = sd->mask;
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
  case ARBIT_FIND:
    jam();
  case ARBIT_PREP1:
    jam();
  case ARBIT_PREP2:
    jam();
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
  ndbrequire(false);
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
  case ARBIT_FIND:              // not used
    jam();
    return 1000;
  case ARBIT_PREP1:
    jam();
  case ARBIT_PREP2:
    jam();
    return 1000 + cnoOfNodes * hb_send_timer.getDelay();
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
  ndbrequire(false);
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
  case ARBIT_INIT:
    jam();
  case ARBIT_FIND:
    jam();
    break;
  case ARBIT_PREP1:		// start from beginning
    jam();
  case ARBIT_PREP2:
    jam();
  case ARBIT_START:
    jam();
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
  case ARBIT_CRASH:
    jam();
    break;
  default:
    ndbrequire(false);
    break;
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
  case ARBIT_FIND:
    jam();
  case ARBIT_PREP1:
    jam();
  case ARBIT_PREP2:
    jam();
    arbitRec.state = ARBIT_INIT;
    arbitRec.newstate = true;
    startArbitThread(signal);
    break;
  case ARBIT_START:		// process in RUN state
    jam();
  case ARBIT_RUN:
    jam();
    arbitRec.newMask.set(nodeId);
    break;
  case ARBIT_CHOOSE:            // XXX too late
    jam();
  case ARBIT_CRASH:
    jam();
    break;
  default:
    ndbrequire(false);
    break;
  }
}

/**
 * Check if current nodeset can survive.  The decision is
 * based on node count, node groups, and on external arbitrator
 * (if we have one).  Always starts a new thread because
 * 1) CHOOSE cannot wait 2) if we are new president we need
 * a thread 3) if we are old president it does no harm.
 */
void
Qmgr::handleArbitCheck(Signal* signal)
{
  jam();
  ndbrequire(cpresident == getOwnNodeId());
  NodeBitmask ndbMask;
  computeArbitNdbMask(ndbMask);
  if (g_ndb_arbit_one_half_rule &&
      2 * ndbMask.count() < cnoOfNodes) {
    jam();
    arbitRec.code = ArbitCode::LoseNodes;
  } else {
    jam();
    CheckNodeGroups* sd = (CheckNodeGroups*)&signal->theData[0];
    sd->blockRef = reference();
    sd->requestType = CheckNodeGroups::Direct | CheckNodeGroups::ArbitCheck;
    sd->mask = ndbMask;
    EXECUTE_DIRECT(DBDIH, GSN_CHECKNODEGROUPSREQ, signal, 
		   CheckNodeGroups::SignalLength);
    jamEntry();
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
      if (g_ndb_arbit_one_half_rule &&
          2 * ndbMask.count() > cnoOfNodes) {
        jam();
        arbitRec.code = ArbitCode::WinNodes;
      }
      break;
    default:
      ndbrequire(false);
      break;
    }
  }
  switch (arbitRec.code) {
  case ArbitCode::LoseNodes:
    jam();
  case ArbitCode::LoseGroups:
    jam();
    goto crashme;
  case ArbitCode::WinNodes:
    jam();
  case ArbitCode::WinGroups:
    jam();
    if (arbitRec.state == ARBIT_RUN) {
      jam();
      break;
    }
    arbitRec.state = ARBIT_INIT;
    arbitRec.newstate = true;
    break;
  case ArbitCode::Partitioning:
    if (arbitRec.state == ARBIT_RUN) {
      jam();
      arbitRec.state = ARBIT_CHOOSE;
      arbitRec.newstate = true;
      break;
    }
    if (arbitRec.apiMask[0].count() != 0) {
      jam();
      arbitRec.code = ArbitCode::LoseNorun;
    } else {
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
    arbitRec.newMask.bitAND(ndbMask);   // delete failed nodes
    arbitRec.recvMask.bitAND(ndbMask);
    sendCommitFailReq(signal);          // start commit of failed nodes
    break;
  case ARBIT_CHOOSE:
    jam();
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
  NodeBitmask ndbMask;
  computeArbitNdbMask(ndbMask);
  ndbout << "arbit thread:";
  ndbout << " state=" << arbitRec.state;
  ndbout << " newstate=" << arbitRec.newstate;
  ndbout << " thread=" << arbitRec.thread;
  ndbout << " node=" << arbitRec.node;
  ndbout << " ticket=" << arbitRec.ticket.getText();
  ndbout << " ndbmask=" << ndbMask.getText();
  ndbout << " sendcount=" << arbitRec.sendCount;
  ndbout << " recvcount=" << arbitRec.recvCount;
  ndbout << " recvmask=" << arbitRec.recvMask.getText();
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
    stateArbitChoose(signal);
    break;
  case ARBIT_CRASH:
    jam();
    stateArbitCrash(signal);
    break;
  default:
    ndbrequire(false);
    break;
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
  NodeRecPtr aPtr;
  for (unsigned rank = 1; rank <= 2; rank++) {
    jam();
    aPtr.i = 0;
    const unsigned stop = NodeBitmask::NotFound;
    while ((aPtr.i = arbitRec.apiMask[rank].find(aPtr.i + 1)) != stop) {
      jam();
      ptrAss(aPtr, nodeRec);
      if (aPtr.p->phase != ZAPI_ACTIVE)
	continue;
      arbitRec.node = aPtr.i;
      arbitRec.state = ARBIT_PREP1;
      arbitRec.newstate = true;
      stateArbitPrep(signal);
      return;
    }
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
  case ArbitCode::PrepAtrun:
    jam();
    arbitRec.node = sd->node;
    arbitRec.ticket = sd->ticket;
    arbitRec.code = sd->code;
    reportArbitEvent(signal, NDB_LE_ArbitState);
    arbitRec.state = ARBIT_RUN;
    arbitRec.newstate = true;
    if (sd->code == ArbitCode::PrepAtrun) {
      jam();
      return;
    }
    break;
  default:
    jam();
    ndbrequire(false);
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
  if (! arbitRec.sendCount) {
    jam();
    BlockReference blockRef = calcApiClusterMgrBlockRef(arbitRec.node);
    ArbitSignalData* sd = (ArbitSignalData*)&signal->theData[0];
    sd->sender = getOwnNodeId();
    sd->code = 0;
    sd->node = arbitRec.node;
    sd->ticket = arbitRec.ticket;
    computeArbitNdbMask(sd->mask);
    sendSignal(blockRef, GSN_ARBIT_CHOOSEREQ, signal,
      ArbitSignalData::SignalLength, JBA);
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
    arbitRec.code = ArbitCode::ErrTimeout;
    reportArbitEvent(signal, NDB_LE_ArbitState);
    arbitRec.state = ARBIT_CRASH;
    arbitRec.newstate = true;
    stateArbitCrash(signal);		// do it at once
    return;
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

void
Qmgr::computeArbitNdbMask(NodeBitmask& aMask)
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
Qmgr::reportArbitEvent(Signal* signal, Ndb_logevent_type type)
{
  ArbitSignalData* sd = (ArbitSignalData*)&signal->theData[0];
  sd->sender = type;
  sd->code = arbitRec.code | (arbitRec.state << 16);
  sd->node = arbitRec.node;
  sd->ticket = arbitRec.ticket;
  sd->mask.clear();
  sendSignal(CMVMI_REF, GSN_EVENT_REP, signal,
    ArbitSignalData::SignalLength, JBB);
}

// end of arbitration module

void
Qmgr::execDUMP_STATE_ORD(Signal* signal)
{
  switch (signal->theData[0]) {
  case 1:
    infoEvent("creadyDistCom = %d, cpresident = %d\n",
	      creadyDistCom, cpresident);
    infoEvent("cpresidentAlive = %d, cpresidentCand = %d (gci: %d)\n",
              cpresidentAlive, 
	      c_start.m_president_candidate, 
	      c_start.m_president_candidate_gci);
    infoEvent("ctoStatus = %d\n", ctoStatus);
    for(Uint32 i = 1; i<MAX_NDB_NODES; i++){
      NodeRecPtr nodePtr;
      nodePtr.i = i;
      ptrCheckGuard(nodePtr, MAX_NDB_NODES, nodeRec);
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
      default:
        sprintf(buf, "Node %d: <UNKNOWN>(%d)", i, nodePtr.p->phase);
        break;
      }
      infoEvent(buf);
    }
  }

#ifdef ERROR_INSERT
  if (signal->theData[0] == 935 && signal->getLength() == 2)
  {
    SET_ERROR_INSERT_VALUE(935);
    c_error_insert_extra = signal->theData[1];
  }
#endif
}//Qmgr::execDUMP_STATE_ORD()


void
Qmgr::execAPI_BROADCAST_REP(Signal* signal)
{
  jamEntry();
  ApiBroadcastRep api= *(const ApiBroadcastRep*)signal->getDataPtr();

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
      mask.set(nodePtr.i);
    }
  }
  
  NodeReceiverGroup rg(API_CLUSTERMGR, mask);
  sendSignal(rg, api.gsn, signal, len, JBB); // forward sections
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
  const AllocNodeIdReq * req = (AllocNodeIdReq*)signal->getDataPtr();
  Uint32 senderRef = req->senderRef;
  Uint32 nodeId = req->nodeId;
  Uint32 nodeType = req->nodeType;
  Uint32 error = 0;

  if (refToBlock(senderRef) != QMGR) // request from management server
  {
    /* master */

    if (getOwnNodeId() != cpresident)
      error = AllocNodeIdRef::NotMaster;
    else if (!opAllocNodeIdReq.m_tracker.done())
      error = AllocNodeIdRef::Busy;
    else if (c_connectedNodes.get(nodeId))
      error = AllocNodeIdRef::NodeConnected;

    if (error)
    {
      jam();
      AllocNodeIdRef * ref = (AllocNodeIdRef*)signal->getDataPtrSend();
      ref->senderRef = reference();
      ref->errorCode = error;
      ref->masterRef = numberToRef(QMGR, cpresident);
      sendSignal(senderRef, GSN_ALLOC_NODEID_REF, signal,
                 AllocNodeIdRef::SignalLength, JBB);
      return;
    }

    if (ERROR_INSERTED(934) && nodeId != getOwnNodeId())
    {
      CRASH_INSERTION(934);
    }
    
    opAllocNodeIdReq.m_req = *req;
    opAllocNodeIdReq.m_error = 0;
    opAllocNodeIdReq.m_connectCount = getNodeInfo(refToNode(senderRef)).m_connectCount;

    jam();
    AllocNodeIdReq * req = (AllocNodeIdReq*)signal->getDataPtrSend();
    req->senderRef = reference();
    NodeReceiverGroup rg(QMGR, c_clusterNodes);
    RequestTracker & p = opAllocNodeIdReq.m_tracker;
    p.init<AllocNodeIdRef>(c_counterMgr, rg, GSN_ALLOC_NODEID_REF, 0);

    sendSignal(rg, GSN_ALLOC_NODEID_REQ, signal,
               AllocNodeIdReq::SignalLength, JBB);
    return;
  }

  /* participant */

  if (c_connectedNodes.get(nodeId))
    error = AllocNodeIdRef::NodeConnected;
  else
  {
    NodeRecPtr nodePtr;
    nodePtr.i = nodeId;
    ptrAss(nodePtr, nodeRec);
    if (nodeType != getNodeInfo(nodeId).m_type)
      error = AllocNodeIdRef::NodeTypeMismatch;
    else if (nodePtr.p->failState != NORMAL)
      error = AllocNodeIdRef::NodeFailureHandlingNotCompleted;
  }

  if (error)
  {
    AllocNodeIdRef * ref = (AllocNodeIdRef*)signal->getDataPtrSend();
    ref->senderRef = reference();
    ref->errorCode = error;
    sendSignal(senderRef, GSN_ALLOC_NODEID_REF, signal,
               AllocNodeIdRef::SignalLength, JBB);
    return;
  }

  AllocNodeIdConf * conf = (AllocNodeIdConf*)signal->getDataPtrSend();
  conf->senderRef = reference();
  sendSignal(senderRef, GSN_ALLOC_NODEID_CONF, signal,
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
    opAllocNodeIdReq.m_tracker.ignoreRef(c_counterMgr,
                                         refToNode(ref->senderRef));    
  }
  else
  {
    opAllocNodeIdReq.m_tracker.reportRef(c_counterMgr,
                                         refToNode(ref->senderRef));
    if (opAllocNodeIdReq.m_error == 0)
      opAllocNodeIdReq.m_error = ref->errorCode;
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
  ndbassert(AllocNodeIdConf::SignalLength == 3);
  sendSignal(opAllocNodeIdReq.m_req.senderRef, GSN_ALLOC_NODEID_CONF, signal,
             AllocNodeIdConf::SignalLength, JBB);
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
      EXECUTE_DIRECT(CMVMI, GSN_START_ORD, signal, 2);
    } else {
      EXECUTE_DIRECT(CMVMI, GSN_STOP_ORD, signal, 1);
    }
    return true;
  }
  return false;
}

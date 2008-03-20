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

#define NDBCNTR_C
#include "Ndbcntr.hpp"

#include <ndb_limits.h>
#include <ndb_version.h>
#include <SimpleProperties.hpp>
#include <signaldata/DictTabInfo.hpp>
#include <signaldata/CreateTable.hpp>
#include <signaldata/ReadNodesConf.hpp>
#include <signaldata/NodeFailRep.hpp>
#include <signaldata/TcKeyReq.hpp>
#include <signaldata/TcKeyConf.hpp>
#include <signaldata/EventReport.hpp>
#include <signaldata/NodeStateSignalData.hpp>
#include <signaldata/StopPerm.hpp>
#include <signaldata/StopMe.hpp>
#include <signaldata/WaitGCP.hpp>
#include <signaldata/CheckNodeGroups.hpp>
#include <signaldata/StartOrd.hpp>
#include <signaldata/AbortAll.hpp>
#include <signaldata/SystemError.hpp>
#include <signaldata/NdbSttor.hpp>
#include <signaldata/CntrStart.hpp>
#include <signaldata/DumpStateOrd.hpp>

#include <signaldata/FsRemoveReq.hpp>
#include <signaldata/ReadConfig.hpp>

#include <signaldata/FailRep.hpp>

#include <AttributeHeader.hpp>
#include <Configuration.hpp>
#include <DebuggerNames.hpp>

#include <NdbOut.hpp>
#include <NdbTick.h>

// used during shutdown for reporting current startphase
// accessed from Emulator.cpp, NdbShutdown()
Uint32 g_currentStartPhase;

/**
 * ALL_BLOCKS Used during start phases and while changing node state
 *
 * NDBFS_REF Has to be before NDBCNTR_REF (due to "ndb -i" stuff)
 */
struct BlockInfo {
  BlockReference Ref; // BlockReference
  Uint32 NextSP;            // Next start phase
  Uint32 ErrorInsertStart;
  Uint32 ErrorInsertStop;
};

static BlockInfo ALL_BLOCKS[] = { 
  { NDBFS_REF,   0 ,  2000,  2999 },
  { DBTC_REF,    1 ,  8000,  8035 },
  { DBDIH_REF,   1 ,  7000,  7173 },
  { DBLQH_REF,   1 ,  5000,  5030 },
  { DBACC_REF,   1 ,  3000,  3999 },
  { DBTUP_REF,   1 ,  4000,  4007 },
  { DBDICT_REF,  1 ,  6000,  6003 },
  { NDBCNTR_REF, 0 ,  1000,  1999 },
  { CMVMI_REF,   1 ,  9000,  9999 }, // before QMGR
  { QMGR_REF,    1 ,     1,   999 },
  { TRIX_REF,    1 ,     0,     0 },
  { BACKUP_REF,  1 , 10000, 10999 },
  { DBUTIL_REF,  1 , 11000, 11999 },
  { SUMA_REF,    1 , 13000, 13999 },
  { DBTUX_REF,   1 , 12000, 12999 }
  ,{ TSMAN_REF,  1 ,     0,     0 }
  ,{ LGMAN_REF,  1 ,     0,     0 }
  ,{ PGMAN_REF,  1 ,     0,     0 }
  ,{ RESTORE_REF,1 ,     0,     0 }
};

static const Uint32 ALL_BLOCKS_SZ = sizeof(ALL_BLOCKS)/sizeof(BlockInfo);

static BlockReference readConfigOrder[ALL_BLOCKS_SZ] = {
  NDBFS_REF, // let it run first to make sure it can start the threads
  CMVMI_REF,
  DBTUP_REF,
  DBACC_REF,
  DBTC_REF,
  DBLQH_REF,
  DBTUX_REF,
  DBDICT_REF,
  DBDIH_REF,
  NDBCNTR_REF,
  QMGR_REF,
  TRIX_REF,
  BACKUP_REF,
  DBUTIL_REF,
  SUMA_REF,
  TSMAN_REF,
  LGMAN_REF,
  PGMAN_REF,
  RESTORE_REF
};

/*******************************/
/*  CONTINUEB                  */
/*******************************/
void Ndbcntr::execCONTINUEB(Signal* signal) 
{
  jamEntry();
  UintR Ttemp1 = signal->theData[0];
  switch (Ttemp1) {
  case ZSTARTUP:{
    if(getNodeState().startLevel == NodeState::SL_STARTED){
      jam();
      return;
    }
    
    if(cmasterNodeId == getOwnNodeId() && c_start.m_starting.isclear()){
      jam();
      trySystemRestart(signal);
      // Fall-through
    }
    
    Uint64 now = NdbTick_CurrentMillisecond();
    if(now > c_start.m_startFailureTimeout)
    {
      jam();
      Uint32 to_3= 0;
      const ndb_mgm_configuration_iterator * p = 
	m_ctx.m_config.getOwnConfigIterator();
      ndb_mgm_get_int_parameter(p, CFG_DB_START_FAILURE_TIMEOUT, &to_3);
      BaseString tmp;
      tmp.append("Shutting down node as total restart time exceeds "
		 " StartFailureTimeout as set in config file ");
      if(to_3 == 0)
	tmp.append(" 0 (inifinite)");
      else
	tmp.appfmt(" %d", to_3);
      
      progError(__LINE__, NDBD_EXIT_RESTART_TIMEOUT, tmp.c_str());
    }
    
    signal->theData[0] = ZSTARTUP;
    sendSignalWithDelay(reference(), GSN_CONTINUEB, signal, 1000, 1);
    break;
  }
  case ZSHUTDOWN:
    jam();
    c_stopRec.checkTimeout(signal);
    break;
  default:
    jam();
    systemErrorLab(signal, __LINE__);
    return;
    break;
  }//switch
}//Ndbcntr::execCONTINUEB()

void
Ndbcntr::execAPI_START_REP(Signal* signal)
{
  if(refToBlock(signal->getSendersBlockRef()) == QMGR)
  {
    for(Uint32 i = 0; i<ALL_BLOCKS_SZ; i++){
      sendSignal(ALL_BLOCKS[i].Ref, GSN_API_START_REP, signal, 1, JBB);
    }
  }
}
/*******************************/
/*  SYSTEM_ERROR               */
/*******************************/
void Ndbcntr::execSYSTEM_ERROR(Signal* signal) 
{
  const SystemError * const sysErr = (SystemError *)signal->getDataPtr();
  char buf[100];
  int killingNode = refToNode(sysErr->errorRef);
  Uint32 data1 = sysErr->data[0];
  
  jamEntry();
  switch (sysErr->errorCode){
  case SystemError::GCPStopDetected:
    BaseString::snprintf(buf, sizeof(buf), 
	     "Node %d killed this node because "
	     "GCP stop was detected",     
	     killingNode);
    break;

  case SystemError::CopyFragRefError:
    CRASH_INSERTION(1000);
    BaseString::snprintf(buf, sizeof(buf), 
			 "Killed by node %d as "
			 "copyfrag failed, error: %u",
			 killingNode, data1);
    break;

  case SystemError::StartFragRefError:
    BaseString::snprintf(buf, sizeof(buf), 
			 "Node %d killed this node because "
			 "it replied StartFragRef error code: %u.",
			 killingNode, data1);
    break;
    
  case SystemError::CopySubscriptionRef:
    BaseString::snprintf(buf, sizeof(buf), 
	     "Node %d killed this node because "
	     "it could not copy a subscription during node restart. "
	     "Copy subscription error code: %u.",
	     killingNode, data1);
    break;
  case SystemError::CopySubscriberRef:
    BaseString::snprintf(buf, sizeof(buf), 
	     "Node %d killed this node because "
	     "it could not start a subscriber during node restart. "
	     "Copy subscription error code: %u.",
	     killingNode, data1);
    break;
  default:
    BaseString::snprintf(buf, sizeof(buf), "System error %d, "
	     " this node was killed by node %d", 
	     sysErr->errorCode, killingNode);
    break;
  }

  progError(__LINE__, NDBD_EXIT_SYSTEM_ERROR, buf);
  return;
}//Ndbcntr::execSYSTEM_ERROR()

void 
Ndbcntr::execREAD_CONFIG_REQ(Signal* signal)
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

void Ndbcntr::execSTTOR(Signal* signal) 
{
  jamEntry();
  cstartPhase = signal->theData[1];

  cndbBlocksCount = 0;
  cinternalStartphase = cstartPhase - 1;

  switch (cstartPhase) {
  case 0:
    if(m_ctx.m_config.getInitialStart()){
      jam();
      c_fsRemoveCount = 0;
      clearFilesystem(signal);
      return;
    }
    sendSttorry(signal);
    break;
  case ZSTART_PHASE_1:
    jam();
    {
      Uint32 db_watchdog_interval = 0;
      const ndb_mgm_configuration_iterator * p = 
        m_ctx.m_config.getOwnConfigIterator();
      ndb_mgm_get_int_parameter(p, CFG_DB_WATCHDOG_INTERVAL, &db_watchdog_interval);
      ndbrequire(db_watchdog_interval);
      update_watch_dog_timer(db_watchdog_interval);
    }
    startPhase1Lab(signal);
    break;
  case ZSTART_PHASE_2:
    jam();
    startPhase2Lab(signal);
    break;
  case ZSTART_PHASE_3:
    jam();
    startPhase3Lab(signal);
    break;
  case ZSTART_PHASE_4:
    jam();
    startPhase4Lab(signal);
    break;
  case ZSTART_PHASE_5:
    jam();
    startPhase5Lab(signal);
    break;
  case 6:
    jam();
    getNodeGroup(signal);
    // Fall through
    break;
  case ZSTART_PHASE_8:
    jam();
    startPhase8Lab(signal);
    break;
  case ZSTART_PHASE_9:
    jam();
    startPhase9Lab(signal);
    break;
  default:
    jam();
    sendSttorry(signal);
    break;
  }//switch
}//Ndbcntr::execSTTOR()

void
Ndbcntr::getNodeGroup(Signal* signal){
  jam();
  CheckNodeGroups * sd = (CheckNodeGroups*)signal->getDataPtrSend();
  sd->requestType = CheckNodeGroups::Direct | CheckNodeGroups::GetNodeGroup;
  EXECUTE_DIRECT(DBDIH, GSN_CHECKNODEGROUPSREQ, signal, 
		 CheckNodeGroups::SignalLength);
  jamEntry();
  c_nodeGroup = sd->output;
  sendSttorry(signal);
}

/*******************************/
/*  NDB_STTORRY                */
/*******************************/
void Ndbcntr::execNDB_STTORRY(Signal* signal) 
{
  jamEntry();
  switch (cstartPhase) {
  case ZSTART_PHASE_2:
    jam();
    ph2GLab(signal);
    return;
    break;
  case ZSTART_PHASE_3:
    jam();
    ph3ALab(signal);
    return;
    break;
  case ZSTART_PHASE_4:
    jam();
    ph4BLab(signal);
    return;
    break;
  case ZSTART_PHASE_5:
    jam();
    ph5ALab(signal);
    return;
    break;
  case ZSTART_PHASE_6:
    jam();
    ph6ALab(signal);
    return;
    break;
  case ZSTART_PHASE_7:
    jam();
    ph6BLab(signal);
    return;
    break;
  case ZSTART_PHASE_8:
    jam();
    ph7ALab(signal);
    return;
    break;
  case ZSTART_PHASE_9:
    jam();
    ph8ALab(signal);
    return;
    break;
  default:
    jam();
    systemErrorLab(signal, __LINE__);
    return;
    break;
  }//switch
}//Ndbcntr::execNDB_STTORRY()

void Ndbcntr::startPhase1Lab(Signal* signal) 
{
  jamEntry();

  initData(signal);

  cdynamicNodeId = 0;

  NdbBlocksRecPtr ndbBlocksPtr;
  ndbBlocksPtr.i = 0;
  ptrAss(ndbBlocksPtr, ndbBlocksRec);
  ndbBlocksPtr.p->blockref = DBLQH_REF;
  ndbBlocksPtr.i = 1;
  ptrAss(ndbBlocksPtr, ndbBlocksRec);
  ndbBlocksPtr.p->blockref = DBDICT_REF;
  ndbBlocksPtr.i = 2;
  ptrAss(ndbBlocksPtr, ndbBlocksRec);
  ndbBlocksPtr.p->blockref = DBTUP_REF;
  ndbBlocksPtr.i = 3;
  ptrAss(ndbBlocksPtr, ndbBlocksRec);
  ndbBlocksPtr.p->blockref = DBACC_REF;
  ndbBlocksPtr.i = 4;
  ptrAss(ndbBlocksPtr, ndbBlocksRec);
  ndbBlocksPtr.p->blockref = DBTC_REF;
  ndbBlocksPtr.i = 5;
  ptrAss(ndbBlocksPtr, ndbBlocksRec);
  ndbBlocksPtr.p->blockref = DBDIH_REF;
  sendSttorry(signal);
  return;
}

void Ndbcntr::execREAD_NODESREF(Signal* signal) 
{
  jamEntry();
  systemErrorLab(signal, __LINE__);
  return;
}//Ndbcntr::execREAD_NODESREF()


/*******************************/
/*  NDB_STARTREF               */
/*******************************/
void Ndbcntr::execNDB_STARTREF(Signal* signal) 
{
  jamEntry();
  systemErrorLab(signal, __LINE__);
  return;
}//Ndbcntr::execNDB_STARTREF()

/*******************************/
/*  STTOR                      */
/*******************************/
void Ndbcntr::startPhase2Lab(Signal* signal) 
{
  c_start.m_lastGci = 0;
  c_start.m_lastGciNodeId = getOwnNodeId();
  
  signal->theData[0] = reference();
  sendSignal(DBDIH_REF, GSN_DIH_RESTARTREQ, signal, 1, JBB);
  return;
}//Ndbcntr::startPhase2Lab()

/*******************************/
/*  DIH_RESTARTCONF            */
/*******************************/
void Ndbcntr::execDIH_RESTARTCONF(Signal* signal) 
{
  jamEntry();
  //cmasterDihId = signal->theData[0];
  c_start.m_lastGci = signal->theData[1];
  ctypeOfStart = NodeState::ST_SYSTEM_RESTART;
  ph2ALab(signal);
  return;
}//Ndbcntr::execDIH_RESTARTCONF()

/*******************************/
/*  DIH_RESTARTREF             */
/*******************************/
void Ndbcntr::execDIH_RESTARTREF(Signal* signal) 
{
  jamEntry();
  ctypeOfStart = NodeState::ST_INITIAL_START;
  ph2ALab(signal);
  return;
}//Ndbcntr::execDIH_RESTARTREF()

void Ndbcntr::ph2ALab(Signal* signal) 
{
  /******************************/
  /* request configured nodes   */
  /* from QMGR                  */
  /*  READ_NODESREQ             */
  /******************************/
  signal->theData[0] = reference();
  sendSignal(QMGR_REF, GSN_READ_NODESREQ, signal, 1, JBB);
  return;
}//Ndbcntr::ph2ALab()

inline
Uint64
setTimeout(Uint64 time, Uint32 timeoutValue){
  if(timeoutValue == 0)
    return ~(Uint64)0;
  return time + timeoutValue;
}

/*******************************/
/*  READ_NODESCONF             */
/*******************************/
void Ndbcntr::execREAD_NODESCONF(Signal* signal) 
{
  jamEntry();
  const ReadNodesConf * readNodes = (ReadNodesConf *)&signal->theData[0];

  cmasterNodeId = readNodes->masterNodeId;
  cdynamicNodeId = readNodes->ndynamicId;

  /**
   * All defined nodes...
   */
  c_allDefinedNodes.assign(NdbNodeBitmask::Size, readNodes->allNodes);
  c_clusterNodes.assign(NdbNodeBitmask::Size, readNodes->clusterNodes);

  Uint32 to_1 = 30000;
  Uint32 to_2 = 0;
  Uint32 to_3 = 0;

  const ndb_mgm_configuration_iterator * p = 
    m_ctx.m_config.getOwnConfigIterator();
  
  ndbrequire(p != 0);
  ndb_mgm_get_int_parameter(p, CFG_DB_START_PARTIAL_TIMEOUT, &to_1);
  ndb_mgm_get_int_parameter(p, CFG_DB_START_PARTITION_TIMEOUT, &to_2);
  ndb_mgm_get_int_parameter(p, CFG_DB_START_FAILURE_TIMEOUT, &to_3);
  
  c_start.m_startTime = NdbTick_CurrentMillisecond();
  c_start.m_startPartialTimeout = setTimeout(c_start.m_startTime, to_1);
  c_start.m_startPartitionedTimeout = setTimeout(c_start.m_startTime, to_2);
  c_start.m_startFailureTimeout = setTimeout(c_start.m_startTime, to_3);
  
  UpgradeStartup::sendCmAppChg(* this, signal, 0); // ADD
  
  sendCntrStartReq(signal);

  signal->theData[0] = ZSTARTUP;
  sendSignalWithDelay(reference(), GSN_CONTINUEB, signal, 1000, 1);
  
  return;
}

void
Ndbcntr::execCM_ADD_REP(Signal* signal){
  jamEntry();
  c_clusterNodes.set(signal->theData[0]);
}

void
Ndbcntr::sendCntrStartReq(Signal * signal){
  jamEntry();

  CntrStartReq * req = (CntrStartReq*)signal->getDataPtrSend();
  req->startType = ctypeOfStart;
  req->lastGci = c_start.m_lastGci;
  req->nodeId = getOwnNodeId();
  sendSignal(calcNdbCntrBlockRef(cmasterNodeId), GSN_CNTR_START_REQ,
	     signal, CntrStartReq::SignalLength, JBB);
}

void
Ndbcntr::execCNTR_START_REF(Signal * signal){
  jamEntry();
  const CntrStartRef * ref = (CntrStartRef*)signal->getDataPtr();

  switch(ref->errorCode){
  case CntrStartRef::NotMaster:
    jam();
    cmasterNodeId = ref->masterNodeId;
    sendCntrStartReq(signal);
    return;
  case CntrStartRef::StopInProgress:
    jam();
    progError(__LINE__, NDBD_EXIT_RESTART_DURING_SHUTDOWN);
  }
  ndbrequire(false);
}

void
Ndbcntr::StartRecord::reset(){
  m_starting.clear();
  m_waiting.clear();
  m_withLog.clear();
  m_withoutLog.clear();
  m_lastGci = m_lastGciNodeId = 0;
  m_startPartialTimeout = ~0;
  m_startPartitionedTimeout = ~0;
  m_startFailureTimeout = ~0;
  
  m_logNodesCount = 0;
}

void
Ndbcntr::execCNTR_START_CONF(Signal * signal){
  jamEntry();
  const CntrStartConf * conf = (CntrStartConf*)signal->getDataPtr();

  cnoStartNodes = conf->noStartNodes;
  ctypeOfStart = (NodeState::StartType)conf->startType;
  c_start.m_lastGci = conf->startGci;
  cmasterNodeId = conf->masterNodeId;
  NdbNodeBitmask tmp; 
  tmp.assign(NdbNodeBitmask::Size, conf->startedNodes);
  c_startedNodes.bitOR(tmp);
  c_start.m_starting.assign(NdbNodeBitmask::Size, conf->startingNodes);
  ph2GLab(signal);

  UpgradeStartup::sendCmAppChg(* this, signal, 2); //START
}

/**
 * Tried with parallell nr, but it crashed in DIH
 * so I turned it off, as I don't want to debug DIH now...
 * Jonas 19/11-03
 *
 * After trying for 2 hours, I gave up.
 * DIH is not designed to support it, and
 * it requires quite of lot of changes to
 * make it work
 * Jonas 5/12-03
 */
#define PARALLELL_NR 0

#if PARALLELL_NR
const bool parallellNR = true;
#else
const bool parallellNR = false;
#endif

void
Ndbcntr::execCNTR_START_REP(Signal* signal){
  jamEntry();
  Uint32 nodeId = signal->theData[0];
  c_startedNodes.set(nodeId);
  c_start.m_starting.clear(nodeId);

  /**
   * Inform all interested blocks that node has started
   */
  for(Uint32 i = 0; i<ALL_BLOCKS_SZ; i++){
    sendSignal(ALL_BLOCKS[i].Ref, GSN_NODE_START_REP, signal, 1, JBB);
  }
  
  if(!c_start.m_starting.isclear()){
    jam();
    return;
  }
  
  if(cmasterNodeId != getOwnNodeId()){
    jam();
    c_start.reset();
    return;
  }

  if(c_start.m_waiting.isclear()){
    jam();
    c_start.reset();
    return;
  }

  startWaitingNodes(signal);
}

void
Ndbcntr::execCNTR_START_REQ(Signal * signal){
  jamEntry();
  const CntrStartReq * req = (CntrStartReq*)signal->getDataPtr();
  
  const Uint32 nodeId = req->nodeId;
  const Uint32 lastGci = req->lastGci;
  const NodeState::StartType st = (NodeState::StartType)req->startType;

  if(cmasterNodeId == 0){
    jam();
    // Has not completed READNODES yet
    sendSignalWithDelay(reference(), GSN_CNTR_START_REQ, signal, 100, 
			signal->getLength());
    return;
  }
  
  if(cmasterNodeId != getOwnNodeId()){
    jam();
    sendCntrStartRef(signal, nodeId, CntrStartRef::NotMaster);
    return;
  }
  
  const NodeState & nodeState = getNodeState();
  switch(nodeState.startLevel){
  case NodeState::SL_NOTHING:
  case NodeState::SL_CMVMI:
    jam();
    ndbrequire(false);
  case NodeState::SL_STARTING:
  case NodeState::SL_STARTED:
    jam();
    break;
    
  case NodeState::SL_STOPPING_1:
  case NodeState::SL_STOPPING_2:
  case NodeState::SL_STOPPING_3:
  case NodeState::SL_STOPPING_4:
    jam();
    sendCntrStartRef(signal, nodeId, CntrStartRef::StopInProgress);
    return;
  }

  /**
   * Am I starting (or started)
   */
  const bool starting = (nodeState.startLevel != NodeState::SL_STARTED);
  
  c_start.m_waiting.set(nodeId);
  switch(st){
  case NodeState::ST_INITIAL_START:
    jam();
    c_start.m_withoutLog.set(nodeId);
    break;
  case NodeState::ST_SYSTEM_RESTART:
    jam();
    c_start.m_withLog.set(nodeId);
    if(starting && lastGci > c_start.m_lastGci){
      jam();
      CntrStartRef * ref = (CntrStartRef*)signal->getDataPtrSend();
      ref->errorCode = CntrStartRef::NotMaster;
      ref->masterNodeId = nodeId;
      NodeReceiverGroup rg (NDBCNTR, c_start.m_waiting);
      sendSignal(rg, GSN_CNTR_START_REF, signal,
		 CntrStartRef::SignalLength, JBB);
      return;
    }
    if(starting){
      jam();
      Uint32 i = c_start.m_logNodesCount++;
      c_start.m_logNodes[i].m_nodeId = nodeId;
      c_start.m_logNodes[i].m_lastGci = req->lastGci;
    }
    break;
  case NodeState::ST_NODE_RESTART:
  case NodeState::ST_INITIAL_NODE_RESTART:
  case NodeState::ST_ILLEGAL_TYPE:
    ndbrequire(false);
  }

  const bool startInProgress = !c_start.m_starting.isclear();

  if((starting && startInProgress) || (startInProgress && !parallellNR)){
    jam();
    // We're already starting together with a bunch of nodes
    // Let this node wait...
    return;
  }
  
  if(starting){
    jam();
    trySystemRestart(signal);
  } else {
    jam();
    startWaitingNodes(signal);
  }
  return;
}

void
Ndbcntr::startWaitingNodes(Signal * signal){

#if ! PARALLELL_NR
  const Uint32 nodeId = c_start.m_waiting.find(0);
  const Uint32 Tref = calcNdbCntrBlockRef(nodeId);
  ndbrequire(nodeId != c_start.m_waiting.NotFound);

  NodeState::StartType nrType = NodeState::ST_NODE_RESTART;
  if(c_start.m_withoutLog.get(nodeId)){
    jam();
    nrType = NodeState::ST_INITIAL_NODE_RESTART;
  }
  
  /**
   * Let node perform restart
   */
  CntrStartConf * conf = (CntrStartConf*)signal->getDataPtrSend();
  conf->noStartNodes = 1;
  conf->startType = nrType;
  conf->startGci = ~0; // Not used
  conf->masterNodeId = getOwnNodeId();
  BitmaskImpl::clear(NdbNodeBitmask::Size, conf->startingNodes);
  BitmaskImpl::set(NdbNodeBitmask::Size, conf->startingNodes, nodeId);
  c_startedNodes.copyto(NdbNodeBitmask::Size, conf->startedNodes);
  sendSignal(Tref, GSN_CNTR_START_CONF, signal, 
	     CntrStartConf::SignalLength, JBB);

  c_start.m_waiting.clear(nodeId);
  c_start.m_withLog.clear(nodeId);
  c_start.m_withoutLog.clear(nodeId);
  c_start.m_starting.set(nodeId);
#else
  // Parallell nr
  
  c_start.m_starting = c_start.m_waiting;
  c_start.m_waiting.clear();
  
  CntrStartConf * conf = (CntrStartConf*)signal->getDataPtrSend();
  conf->noStartNodes = 1;
  conf->startGci = ~0; // Not used
  conf->masterNodeId = getOwnNodeId();
  c_start.m_starting.copyto(NdbNodeBitmask::Size, conf->startingNodes);
  c_startedNodes.copyto(NdbNodeBitmask::Size, conf->startedNodes);
  
  char buf[100];
  if(!c_start.m_withLog.isclear()){
    jam();
    ndbout_c("Starting nodes w/ log: %s", c_start.m_withLog.getText(buf));

    NodeReceiverGroup rg(NDBCNTR, c_start.m_withLog);
    conf->startType = NodeState::ST_NODE_RESTART;
    
    sendSignal(rg, GSN_CNTR_START_CONF, signal, 
	       CntrStartConf::SignalLength, JBB);
  }

  if(!c_start.m_withoutLog.isclear()){
    jam();
    ndbout_c("Starting nodes wo/ log: %s", c_start.m_withoutLog.getText(buf));
    NodeReceiverGroup rg(NDBCNTR, c_start.m_withoutLog);
    conf->startType = NodeState::ST_INITIAL_NODE_RESTART;
    
    sendSignal(rg, GSN_CNTR_START_CONF, signal, 
	       CntrStartConf::SignalLength, JBB);
  }

  c_start.m_waiting.clear();
  c_start.m_withLog.clear();
  c_start.m_withoutLog.clear();
#endif
}

void
Ndbcntr::sendCntrStartRef(Signal * signal, 
			  Uint32 nodeId, CntrStartRef::ErrorCode code){
  CntrStartRef * ref = (CntrStartRef*)signal->getDataPtrSend();
  ref->errorCode = code;
  ref->masterNodeId = cmasterNodeId;
  sendSignal(calcNdbCntrBlockRef(nodeId), GSN_CNTR_START_REF, signal,
	     CntrStartRef::SignalLength, JBB);
}

CheckNodeGroups::Output
Ndbcntr::checkNodeGroups(Signal* signal, const NdbNodeBitmask & mask){
  CheckNodeGroups* sd = (CheckNodeGroups*)&signal->theData[0];
  sd->blockRef = reference();
  sd->requestType = CheckNodeGroups::Direct | CheckNodeGroups::ArbitCheck;
  sd->mask = mask;
  EXECUTE_DIRECT(DBDIH, GSN_CHECKNODEGROUPSREQ, signal, 
		 CheckNodeGroups::SignalLength);
  jamEntry();
  return (CheckNodeGroups::Output)sd->output;
}

bool
Ndbcntr::trySystemRestart(Signal* signal){
  /**
   * System restart something
   */
  const bool allNodes = c_start.m_waiting.equal(c_allDefinedNodes);
  const bool allClusterNodes = c_start.m_waiting.equal(c_clusterNodes);

  if(!allClusterNodes){
    jam();
    return false;
  }
  
  NodeState::StartType srType = NodeState::ST_SYSTEM_RESTART;
  if(c_start.m_waiting.equal(c_start.m_withoutLog))
  {
    jam();
    srType = NodeState::ST_INITIAL_START;
    c_start.m_starting = c_start.m_withoutLog; // Used for starting...
    c_start.m_withoutLog.clear();
  } else {

    CheckNodeGroups::Output wLog = checkNodeGroups(signal, c_start.m_withLog);

    switch (wLog) {
    case CheckNodeGroups::Win:
      jam();
      break;
    case CheckNodeGroups::Lose:
      jam();
      // If we lose with all nodes, then we're in trouble
      ndbrequire(!allNodes);
      return false;
    case CheckNodeGroups::Partitioning:
      jam();
      bool allowPartition = (c_start.m_startPartitionedTimeout != (Uint64)~0);
      
      if(allNodes){
	if(allowPartition){
	  jam();
	  break;
	}
	ndbrequire(false); // All nodes -> partitioning, which is not allowed
      }
      
      break;
    }    
    
    // For now only with the "logged"-ones.
    // Let the others do node restart afterwards...
    c_start.m_starting = c_start.m_withLog;
    c_start.m_withLog.clear();
  }
      
  /**
   * Okidoki, we try to start
   */
  CntrStartConf * conf = (CntrStartConf*)signal->getDataPtr();
  conf->noStartNodes = c_start.m_starting.count();
  conf->startType = srType;
  conf->startGci = c_start.m_lastGci;
  conf->masterNodeId = c_start.m_lastGciNodeId;
  c_start.m_starting.copyto(NdbNodeBitmask::Size, conf->startingNodes);
  c_startedNodes.copyto(NdbNodeBitmask::Size, conf->startedNodes);
  
  ndbrequire(c_start.m_lastGciNodeId == getOwnNodeId());
  
  NodeReceiverGroup rg(NDBCNTR, c_start.m_starting);
  sendSignal(rg, GSN_CNTR_START_CONF, signal, CntrStartConf::SignalLength,JBB);
  
  c_start.m_waiting.bitANDC(c_start.m_starting);
  
  return true;
}

void Ndbcntr::ph2GLab(Signal* signal) 
{
  if (cndbBlocksCount < ZNO_NDB_BLOCKS) {
    jam();
    sendNdbSttor(signal);
    return;
  }//if
  sendSttorry(signal);
  return;
}//Ndbcntr::ph2GLab()

/*
4.4  START PHASE 3 */
/*###########################################################################*/
// SEND SIGNAL NDBSTTOR TO ALL BLOCKS, ACC, DICT, DIH, LQH, TC AND TUP
// WHEN ALL BLOCKS HAVE RETURNED THEIR NDB_STTORRY ALL BLOCK HAVE FINISHED
// THEIR LOCAL CONNECTIONs SUCESSFULLY
// AND THEN WE CAN SEND APPL_STARTREG TO INFORM QMGR THAT WE ARE READY TO
// SET UP DISTRIBUTED CONNECTIONS.
/*--------------------------------------------------------------*/
// THIS IS NDB START PHASE 3.
/*--------------------------------------------------------------*/
/*******************************/
/*  STTOR                      */
/*******************************/
void Ndbcntr::startPhase3Lab(Signal* signal) 
{
  ph3ALab(signal);
  return;
}//Ndbcntr::startPhase3Lab()

/*******************************/
/*  NDB_STTORRY                */
/*******************************/
void Ndbcntr::ph3ALab(Signal* signal) 
{
  if (cndbBlocksCount < ZNO_NDB_BLOCKS) {
    jam();
    sendNdbSttor(signal);
    return;
  }//if

  sendSttorry(signal);
  return;
}//Ndbcntr::ph3ALab()

/*
4.5  START PHASE 4      */
/*###########################################################################*/
// WAIT FOR ALL NODES IN CLUSTER TO CHANGE STATE INTO ZSTART ,
// APPL_CHANGEREP IS ALWAYS SENT WHEN SOMEONE HAVE
// CHANGED THEIR STATE. APPL_STARTCONF INDICATES THAT ALL NODES ARE IN START 
// STATE SEND NDB_STARTREQ TO DIH AND THEN WAIT FOR NDB_STARTCONF
/*---------------------------------------------------------------------------*/
/*******************************/
/*  STTOR                      */
/*******************************/
void Ndbcntr::startPhase4Lab(Signal* signal) 
{
  ph4ALab(signal);
}//Ndbcntr::startPhase4Lab()


void Ndbcntr::ph4ALab(Signal* signal) 
{
  ph4BLab(signal);
  return;
}//Ndbcntr::ph4ALab()

/*******************************/
/*  NDB_STTORRY                */
/*******************************/
void Ndbcntr::ph4BLab(Signal* signal) 
{
/*--------------------------------------*/
/* CASE: CSTART_PHASE = ZSTART_PHASE_4  */
/*--------------------------------------*/
  if (cndbBlocksCount < ZNO_NDB_BLOCKS) {
    jam();
    sendNdbSttor(signal);
    return;
  }//if
  if ((ctypeOfStart == NodeState::ST_NODE_RESTART) ||
      (ctypeOfStart == NodeState::ST_INITIAL_NODE_RESTART)) {
    jam();
    sendSttorry(signal);
    return;
  }//if
  waitpoint41Lab(signal);
  return;
}//Ndbcntr::ph4BLab()

void Ndbcntr::waitpoint41Lab(Signal* signal) 
{
  if (getOwnNodeId() == cmasterNodeId) {
    jam();
/*--------------------------------------*/
/* MASTER WAITS UNTIL ALL SLAVES HAS    */
/* SENT THE REPORTS                     */
/*--------------------------------------*/
    cnoWaitrep++;
    if (cnoWaitrep == cnoStartNodes) {
      jam();
      cnoWaitrep = 0;
/*---------------------------------------------------------------------------*/
// NDB_STARTREQ STARTS UP ALL SET UP OF DISTRIBUTION INFORMATION IN DIH AND
// DICT. AFTER SETTING UP THIS
// DATA IT USES THAT DATA TO SET UP WHICH FRAGMENTS THAT ARE TO START AND
// WHERE THEY ARE TO START. THEN
// IT SETS UP THE FRAGMENTS AND RECOVERS THEM BY:
//  1) READING A LOCAL CHECKPOINT FROM DISK.
//  2) EXECUTING THE UNDO LOG ON INDEX AND DATA.
//  3) EXECUTING THE FRAGMENT REDO LOG FROM ONE OR SEVERAL NODES TO
//     RESTORE THE RESTART CONFIGURATION OF DATA IN NDB CLUSTER.
/*---------------------------------------------------------------------------*/
      signal->theData[0] = reference();
      signal->theData[1] = ctypeOfStart;
      sendSignal(DBDIH_REF, GSN_NDB_STARTREQ, signal, 2, JBB);
    }//if
  } else {
    jam();
/*--------------------------------------*/
/* SLAVE NODES WILL PASS HERE ONCE AND  */
/* SEND A WAITPOINT REPORT TO MASTER.   */
/* SLAVES WONT DO ANYTHING UNTIL THEY   */
/* RECEIVE A WAIT REPORT FROM THE MASTER*/
/*--------------------------------------*/
    signal->theData[0] = getOwnNodeId();
    signal->theData[1] = ZWAITPOINT_4_1;
    sendSignal(calcNdbCntrBlockRef(cmasterNodeId), 
	       GSN_CNTR_WAITREP, signal, 2, JBB);
  }//if
  return;
}//Ndbcntr::waitpoint41Lab()

/*******************************/
/*  NDB_STARTCONF              */
/*******************************/
void Ndbcntr::execNDB_STARTCONF(Signal* signal) 
{
  jamEntry();

  NodeReceiverGroup rg(NDBCNTR, c_start.m_starting);
  signal->theData[0] = getOwnNodeId();
  signal->theData[1] = ZWAITPOINT_4_2;
  sendSignal(rg, GSN_CNTR_WAITREP, signal, 2, JBB);
  return;
}//Ndbcntr::execNDB_STARTCONF()

/*
4.6  START PHASE 5      */
/*###########################################################################*/
// SEND APPL_RUN TO THE QMGR IN THIS BLOCK
// SEND NDB_STTOR ALL BLOCKS ACC, DICT, DIH, LQH, TC AND TUP THEN WAIT FOR
// THEIR NDB_STTORRY
/*---------------------------------------------------------------------------*/
/*******************************/
/*  STTOR                      */
/*******************************/
void Ndbcntr::startPhase5Lab(Signal* signal) 
{
  ph5ALab(signal);
  return;
}//Ndbcntr::startPhase5Lab()

/*******************************/
/*  NDB_STTORRY                */
/*******************************/
/*---------------------------------------------------------------------------*/
// THIS IS NDB START PHASE 5.
/*---------------------------------------------------------------------------*/
// IN THIS START PHASE TUP INITIALISES DISK FILES FOR DISK STORAGE IF INITIAL
// START. DIH WILL START UP
// THE GLOBAL CHECKPOINT PROTOCOL AND WILL CONCLUDE ANY UNFINISHED TAKE OVERS 
// THAT STARTED BEFORE THE SYSTEM CRASH.
/*---------------------------------------------------------------------------*/
void Ndbcntr::ph5ALab(Signal* signal) 
{
  if (cndbBlocksCount < ZNO_NDB_BLOCKS) {
    jam();
    sendNdbSttor(signal);
    return;
  }//if

  cstartPhase = cstartPhase + 1;
  cinternalStartphase = cstartPhase - 1;
  if (getOwnNodeId() == cmasterNodeId) {
    switch(ctypeOfStart){
    case NodeState::ST_INITIAL_START:
      jam();
      /*--------------------------------------*/
      /* MASTER CNTR IS RESPONSIBLE FOR       */
      /* CREATING SYSTEM TABLES               */
      /*--------------------------------------*/
      createSystableLab(signal, 0);
      return;
    case NodeState::ST_SYSTEM_RESTART:
      jam();
      waitpoint52Lab(signal);
      return;
    case NodeState::ST_NODE_RESTART:
    case NodeState::ST_INITIAL_NODE_RESTART:
      jam();
      break;
    case NodeState::ST_ILLEGAL_TYPE:
      jam();
      break;
    }
    ndbrequire(false);
  }
  
  /**
   * Not master
   */
  NdbSttor * const req = (NdbSttor*)signal->getDataPtrSend();
  switch(ctypeOfStart){
  case NodeState::ST_NODE_RESTART:
  case NodeState::ST_INITIAL_NODE_RESTART:
    jam();
    /*----------------------------------------------------------------------*/
    // SEND NDB START PHASE 5 IN NODE RESTARTS TO COPY DATA TO THE NEWLY
    // STARTED NODE.
    /*----------------------------------------------------------------------*/
    req->senderRef = reference();
    req->nodeId = getOwnNodeId();
    req->internalStartPhase = cinternalStartphase;
    req->typeOfStart = ctypeOfStart;
    req->masterNodeId = cmasterNodeId;
    
    //#define TRACE_STTOR
#ifdef TRACE_STTOR
    ndbout_c("sending NDB_STTOR(%d) to DIH", cinternalStartphase);
#endif
    sendSignal(DBDIH_REF, GSN_NDB_STTOR, signal, 
	       NdbSttor::SignalLength, JBB);
    return;
  case NodeState::ST_INITIAL_START:
  case NodeState::ST_SYSTEM_RESTART:
    jam();
    /*--------------------------------------*/
    /* DURING SYSTEMRESTART AND INITALSTART:*/
    /* SLAVE NODES WILL PASS HERE ONCE AND  */
    /* SEND A WAITPOINT REPORT TO MASTER.   */
    /* SLAVES WONT DO ANYTHING UNTIL THEY   */
    /* RECEIVE A WAIT REPORT FROM THE MASTER*/
    /* WHEN THE MASTER HAS FINISHED HIS WORK*/
    /*--------------------------------------*/
    signal->theData[0] = getOwnNodeId();
    signal->theData[1] = ZWAITPOINT_5_2;
    sendSignal(calcNdbCntrBlockRef(cmasterNodeId), 
	       GSN_CNTR_WAITREP, signal, 2, JBB);
    return;
  default:
    ndbrequire(false);
  }
}//Ndbcntr::ph5ALab()

void Ndbcntr::waitpoint52Lab(Signal* signal) 
{
  cnoWaitrep = cnoWaitrep + 1;
/*---------------------------------------------------------------------------*/
// THIS WAITING POINT IS ONLY USED BY A MASTER NODE. WE WILL EXECUTE NDB START 
// PHASE 5 FOR DIH IN THE
// MASTER. THIS WILL START UP LOCAL CHECKPOINTS AND WILL ALSO CONCLUDE ANY
// UNFINISHED LOCAL CHECKPOINTS
// BEFORE THE SYSTEM CRASH. THIS WILL ENSURE THAT WE ALWAYS RESTART FROM A
// WELL KNOWN STATE.
/*---------------------------------------------------------------------------*/
/*--------------------------------------*/
/* MASTER WAITS UNTIL HE RECEIVED WAIT  */
/* REPORTS FROM ALL SLAVE CNTR          */
/*--------------------------------------*/
  if (cnoWaitrep == cnoStartNodes) {
    jam();
    cnoWaitrep = 0;

    NdbSttor * const req = (NdbSttor*)signal->getDataPtrSend();
    req->senderRef = reference();
    req->nodeId = getOwnNodeId();
    req->internalStartPhase = cinternalStartphase;
    req->typeOfStart = ctypeOfStart;
    req->masterNodeId = cmasterNodeId;
#ifdef TRACE_STTOR
    ndbout_c("sending NDB_STTOR(%d) to DIH", cinternalStartphase);
#endif
    sendSignal(DBDIH_REF, GSN_NDB_STTOR, signal, 
	       NdbSttor::SignalLength, JBB);
  }//if
  return;
}//Ndbcntr::waitpoint52Lab()

/*******************************/
/*  NDB_STTORRY                */
/*******************************/
void Ndbcntr::ph6ALab(Signal* signal) 
{
  if ((ctypeOfStart == NodeState::ST_NODE_RESTART) ||
      (ctypeOfStart == NodeState::ST_INITIAL_NODE_RESTART)) {
    jam();
    waitpoint51Lab(signal);
    return;
  }//if

  NodeReceiverGroup rg(NDBCNTR, c_start.m_starting);
  rg.m_nodes.clear(getOwnNodeId());
  signal->theData[0] = getOwnNodeId();
  signal->theData[1] = ZWAITPOINT_5_1;
  sendSignal(rg, GSN_CNTR_WAITREP, signal, 2, JBB);

  waitpoint51Lab(signal);
  return;
}//Ndbcntr::ph6ALab()

void Ndbcntr::waitpoint51Lab(Signal* signal) 
{
  cstartPhase = cstartPhase + 1;
/*---------------------------------------------------------------------------*/
// A FINAL STEP IS NOW TO SEND NDB_STTOR TO TC. THIS MAKES IT POSSIBLE TO 
// CONNECT TO TC FOR APPLICATIONS.
// THIS IS NDB START PHASE 6 WHICH IS FOR ALL BLOCKS IN ALL NODES.
/*---------------------------------------------------------------------------*/
  cinternalStartphase = cstartPhase - 1;
  cndbBlocksCount = 0;
  ph6BLab(signal);
  return;
}//Ndbcntr::waitpoint51Lab()

void Ndbcntr::ph6BLab(Signal* signal) 
{
  // c_missra.currentStartPhase - cstartPhase - cinternalStartphase =
  // 5 - 7 - 6
  if (cndbBlocksCount < ZNO_NDB_BLOCKS) {
    jam();
    sendNdbSttor(signal);
    return;
  }//if
  if ((ctypeOfStart == NodeState::ST_NODE_RESTART) ||
      (ctypeOfStart == NodeState::ST_INITIAL_NODE_RESTART)) {
    jam();
    sendSttorry(signal);
    return;
  }
  waitpoint61Lab(signal);
}

void Ndbcntr::waitpoint61Lab(Signal* signal)
{
  if (getOwnNodeId() == cmasterNodeId) {
    jam();
    cnoWaitrep6++;
    if (cnoWaitrep6 == cnoStartNodes) {
      jam();
      NodeReceiverGroup rg(NDBCNTR, c_start.m_starting);
      rg.m_nodes.clear(getOwnNodeId());
      signal->theData[0] = getOwnNodeId();
      signal->theData[1] = ZWAITPOINT_6_2;
      sendSignal(rg, GSN_CNTR_WAITREP, signal, 2, JBB);
      sendSttorry(signal);
    }
  } else {
    jam();
    signal->theData[0] = getOwnNodeId();
    signal->theData[1] = ZWAITPOINT_6_1;
    sendSignal(calcNdbCntrBlockRef(cmasterNodeId), GSN_CNTR_WAITREP, signal, 2, JBB);
  }
}

// Start phase 8 (internal 7)
void Ndbcntr::startPhase8Lab(Signal* signal)
{
  cinternalStartphase = cstartPhase - 1;
  cndbBlocksCount = 0;
  ph7ALab(signal);
}

void Ndbcntr::ph7ALab(Signal* signal)
{
  while (cndbBlocksCount < ZNO_NDB_BLOCKS) {
    jam();
    sendNdbSttor(signal);
    return;
  }
  if ((ctypeOfStart == NodeState::ST_NODE_RESTART) ||
      (ctypeOfStart == NodeState::ST_INITIAL_NODE_RESTART)) {
    jam();
    sendSttorry(signal);
    return;
  }
  waitpoint71Lab(signal);
}

void Ndbcntr::waitpoint71Lab(Signal* signal)
{
  if (getOwnNodeId() == cmasterNodeId) {
    jam();
    cnoWaitrep7++;
    if (cnoWaitrep7 == cnoStartNodes) {
      jam();
      NodeReceiverGroup rg(NDBCNTR, c_start.m_starting);
      rg.m_nodes.clear(getOwnNodeId());
      signal->theData[0] = getOwnNodeId();
      signal->theData[1] = ZWAITPOINT_7_2;
      sendSignal(rg, GSN_CNTR_WAITREP, signal, 2, JBB);
      sendSttorry(signal);
    }
  } else {
    jam();
    signal->theData[0] = getOwnNodeId();
    signal->theData[1] = ZWAITPOINT_7_1;
    sendSignal(calcNdbCntrBlockRef(cmasterNodeId), GSN_CNTR_WAITREP, signal, 2, JBB);
  }
}

// Start phase 9 (internal 8)
void Ndbcntr::startPhase9Lab(Signal* signal)
{
  cinternalStartphase = cstartPhase - 1;
  cndbBlocksCount = 0;
  ph8ALab(signal);
}

void Ndbcntr::ph8ALab(Signal* signal)
{
/*---------------------------------------------------------------------------*/
// NODES WHICH PERFORM A NODE RESTART NEEDS TO GET THE DYNAMIC ID'S
// OF THE OTHER NODES HERE.
/*---------------------------------------------------------------------------*/
  sendSttorry(signal);
  resetStartVariables(signal);
  return;
}//Ndbcntr::ph8BLab()

/*******************************/
/*  CNTR_WAITREP               */
/*******************************/
void Ndbcntr::execCNTR_WAITREP(Signal* signal) 
{
  Uint16 twaitPoint;

  jamEntry();
  twaitPoint = signal->theData[1];
  switch (twaitPoint) {
  case ZWAITPOINT_4_1:
    jam();
    waitpoint41Lab(signal);
    break;
  case ZWAITPOINT_4_2:
    jam();
    sendSttorry(signal);
    break;
  case ZWAITPOINT_5_1:
    jam();
    waitpoint51Lab(signal);
    break;
  case ZWAITPOINT_5_2:
    jam();
    waitpoint52Lab(signal);
    break;
  case ZWAITPOINT_6_1:
    jam();
    waitpoint61Lab(signal);
    break;
  case ZWAITPOINT_6_2:
    jam();
    sendSttorry(signal);
    break;
  case ZWAITPOINT_7_1:
    jam();
    waitpoint71Lab(signal);
    break;
  case ZWAITPOINT_7_2:
    jam();
    sendSttorry(signal);
    break;
  default:
    jam();
    systemErrorLab(signal, __LINE__);
    break;
  }//switch
}//Ndbcntr::execCNTR_WAITREP()

/*******************************/
/*  NODE_FAILREP               */
/*******************************/
void Ndbcntr::execNODE_FAILREP(Signal* signal) 
{
  jamEntry();

  if (ERROR_INSERTED(1001))
  {
    sendSignalWithDelay(reference(), GSN_NODE_FAILREP, signal, 100, 
                        signal->getLength());
    return;
  }
  
  const NodeFailRep * nodeFail = (NodeFailRep *)&signal->theData[0];
  NdbNodeBitmask allFailed; 
  allFailed.assign(NdbNodeBitmask::Size, nodeFail->theNodes);

  NdbNodeBitmask failedStarted = c_startedNodes;
  NdbNodeBitmask failedStarting = c_start.m_starting;
  NdbNodeBitmask failedWaiting = c_start.m_waiting;

  failedStarted.bitAND(allFailed);
  failedStarting.bitAND(allFailed);
  failedWaiting.bitAND(allFailed);
  
  const bool tMasterFailed = allFailed.get(cmasterNodeId);
  const bool tStarted = !failedStarted.isclear();
  const bool tStarting = !failedStarting.isclear();

  if(tMasterFailed){
    jam();
    /**
     * If master has failed choose qmgr president as master
     */
    cmasterNodeId = nodeFail->masterNodeId;
  }
  
  /**
   * Clear node bitmasks from failed nodes
   */
  c_start.m_starting.bitANDC(allFailed);
  c_start.m_waiting.bitANDC(allFailed);
  c_start.m_withLog.bitANDC(allFailed);
  c_start.m_withoutLog.bitANDC(allFailed);
  c_clusterNodes.bitANDC(allFailed);
  c_startedNodes.bitANDC(allFailed);

  const NodeState & st = getNodeState();
  if(st.startLevel == st.SL_STARTING){
    jam();

    const Uint32 phase = st.starting.startPhase;
    
    const bool tStartConf = (phase > 2) || (phase == 2 && cndbBlocksCount > 0);

    if(tMasterFailed){
      progError(__LINE__, NDBD_EXIT_SR_OTHERNODEFAILED,
		"Unhandled node failure during restart");
    }
    
    if(tStartConf && tStarting){
      // One of other starting nodes has crashed...
      progError(__LINE__, NDBD_EXIT_SR_OTHERNODEFAILED,
		"Unhandled node failure of starting node during restart");
    }

    if(tStartConf && tStarted){
      // One of other started nodes has crashed...      
      progError(__LINE__, NDBD_EXIT_SR_OTHERNODEFAILED,
		"Unhandled node failure of started node during restart");
    }
    
    Uint32 nodeId = 0;
    while(!allFailed.isclear()){
      nodeId = allFailed.find(nodeId + 1);
      allFailed.clear(nodeId);
      signal->theData[0] = nodeId;
      sendSignal(QMGR_REF, GSN_NDB_FAILCONF, signal, 1, JBB);
    }//for
    
    return;
  }
  
  ndbrequire(!allFailed.get(getOwnNodeId()));

  NodeFailRep * rep = (NodeFailRep *)&signal->theData[0];  
  rep->masterNodeId = cmasterNodeId;

  sendSignal(DBTC_REF, GSN_NODE_FAILREP, signal, 
	     NodeFailRep::SignalLength, JBB);
  
  sendSignal(DBLQH_REF, GSN_NODE_FAILREP, signal, 
	     NodeFailRep::SignalLength, JBB);
  
  sendSignal(DBDIH_REF, GSN_NODE_FAILREP, signal, 
	     NodeFailRep::SignalLength, JBB);
  
  sendSignal(DBDICT_REF, GSN_NODE_FAILREP, signal, 
	     NodeFailRep::SignalLength, JBB);
  
  sendSignal(BACKUP_REF, GSN_NODE_FAILREP, signal,
	     NodeFailRep::SignalLength, JBB);

  sendSignal(SUMA_REF, GSN_NODE_FAILREP, signal,
	     NodeFailRep::SignalLength, JBB);

  sendSignal(QMGR_REF, GSN_NODE_FAILREP, signal,
	     NodeFailRep::SignalLength, JBB);

  if (c_stopRec.stopReq.senderRef)
  {
    jam();
    switch(c_stopRec.m_state){
    case StopRecord::SR_WAIT_NODE_FAILURES:
    {
      jam();
      NdbNodeBitmask tmp;
      tmp.assign(NdbNodeBitmask::Size, c_stopRec.stopReq.nodes);
      tmp.bitANDC(allFailed);      
      tmp.copyto(NdbNodeBitmask::Size, c_stopRec.stopReq.nodes);
      
      if (tmp.isclear())
      {
	jam();
	if (c_stopRec.stopReq.senderRef != RNIL)
	{
	  jam();
	  StopConf * const stopConf = (StopConf *)&signal->theData[0];
	  stopConf->senderData = c_stopRec.stopReq.senderData;
	  stopConf->nodeState  = (Uint32) NodeState::SL_SINGLEUSER;
	  sendSignal(c_stopRec.stopReq.senderRef, GSN_STOP_CONF, signal, 
		     StopConf::SignalLength, JBB);
	}

	c_stopRec.stopReq.senderRef = 0;
	WaitGCPReq * req = (WaitGCPReq*)&signal->theData[0];
	req->senderRef = reference();
	req->senderData = StopRecord::SR_UNBLOCK_GCP_START_GCP;
	req->requestType = WaitGCPReq::UnblockStartGcp;
	sendSignal(DBDIH_REF, GSN_WAIT_GCP_REQ, signal, 
		   WaitGCPReq::SignalLength, JBA);
      }
      break;
    }
    case StopRecord::SR_QMGR_STOP_REQ:
    {
      NdbNodeBitmask tmp;
      tmp.assign(NdbNodeBitmask::Size, c_stopRec.stopReq.nodes);
      tmp.bitANDC(allFailed);      

      if (tmp.isclear())
      {
	Uint32 nodeId = allFailed.find(0);
	tmp.set(nodeId);

	StopConf* conf = (StopConf*)signal->getDataPtrSend();
	conf->senderData = c_stopRec.stopReq.senderData;
	conf->nodeId = nodeId;
	sendSignal(reference(), 
		   GSN_STOP_CONF, signal, StopConf::SignalLength, JBB);
      }

      tmp.copyto(NdbNodeBitmask::Size, c_stopRec.stopReq.nodes);
      
      break;
    }
    case StopRecord::SR_BLOCK_GCP_START_GCP:
    case StopRecord::SR_WAIT_COMPLETE_GCP:
    case StopRecord::SR_UNBLOCK_GCP_START_GCP:
    case StopRecord::SR_CLUSTER_SHUTDOWN:
      break;
    }
  }
  
  signal->theData[0] = NDB_LE_NODE_FAILREP;
  signal->theData[2] = 0;
  
  Uint32 nodeId = 0;
  while(!allFailed.isclear()){
    nodeId = allFailed.find(nodeId + 1);
    allFailed.clear(nodeId);
    signal->theData[1] = nodeId;
    sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 3, JBB);
  }//for

  return;
}//Ndbcntr::execNODE_FAILREP()

/*******************************/
/*  READ_NODESREQ              */
/*******************************/
void Ndbcntr::execREAD_NODESREQ(Signal* signal) 
{
  jamEntry();

  /*----------------------------------------------------------------------*/
  // ANY BLOCK MAY SEND A REQUEST ABOUT NDB NODES AND VERSIONS IN THE
  // SYSTEM. THIS REQUEST CAN ONLY BE HANDLED IN
  // ABSOLUTE STARTPHASE 3 OR LATER
  /*----------------------------------------------------------------------*/
  BlockReference TuserBlockref = signal->theData[0];
  ReadNodesConf * const readNodes = (ReadNodesConf *)&signal->theData[0];
  
  /**
   * Prepare inactiveNodes bitmask.
   * The concept as such is by the way pretty useless.
   * It makes parallell starts more or less impossible...
   */
  NdbNodeBitmask tmp1; 
  tmp1.bitOR(c_startedNodes);
  if(!getNodeState().getNodeRestartInProgress()){
    tmp1.bitOR(c_start.m_starting);
  } else {
    tmp1.set(getOwnNodeId());
  }

  NdbNodeBitmask tmp2;
  tmp2.bitOR(c_allDefinedNodes);
  tmp2.bitANDC(tmp1);
  /**
   * Fill in return signal
   */
  tmp2.copyto(NdbNodeBitmask::Size, readNodes->inactiveNodes);
  c_allDefinedNodes.copyto(NdbNodeBitmask::Size, readNodes->allNodes);
  c_clusterNodes.copyto(NdbNodeBitmask::Size, readNodes->clusterNodes);
  c_startedNodes.copyto(NdbNodeBitmask::Size, readNodes->startedNodes);
  c_start.m_starting.copyto(NdbNodeBitmask::Size, readNodes->startingNodes);

  readNodes->noOfNodes = c_allDefinedNodes.count();
  readNodes->masterNodeId = cmasterNodeId;
  readNodes->ndynamicId = cdynamicNodeId;
  if (cstartPhase > ZSTART_PHASE_2) {
    jam();
    sendSignal(TuserBlockref, GSN_READ_NODESCONF, signal, 
	       ReadNodesConf::SignalLength, JBB);
    
  } else {
    jam();
    signal->theData[0] = ZNOT_AVAILABLE;
    sendSignal(TuserBlockref, GSN_READ_NODESREF, signal, 1, JBB);
  }//if
}//Ndbcntr::execREAD_NODESREQ()

/*----------------------------------------------------------------------*/
// SENDS APPL_ERROR TO QMGR AND THEN SET A POINTER OUT OF BOUNDS
/*----------------------------------------------------------------------*/
void Ndbcntr::systemErrorLab(Signal* signal, int line) 
{
  progError(line, NDBD_EXIT_NDBREQUIRE); /* BUG INSERTION */
  return;
}//Ndbcntr::systemErrorLab()

/*###########################################################################*/
/* CNTR MASTER CREATES AND INITIALIZES A SYSTEMTABLE AT INITIALSTART         */
/*       |-2048| # 1 00000001    |                                           */
/*       |  :  |   :             |                                           */
/*       | -1  | # 1 00000001    |                                           */
/*       |  1  |   0             | tupleid sequence now created on first use */
/*       |  :  |   :             |                   v                       */
/*       | 2048|   0             |                   v                       */
/*---------------------------------------------------------------------------*/
void Ndbcntr::createSystableLab(Signal* signal, unsigned index)
{
  if (index >= g_sysTableCount) {
    ndbassert(index == g_sysTableCount);
    startInsertTransactions(signal);
    return;
  }
  const SysTable& table = *g_sysTableList[index];
  Uint32 propPage[256];
  LinearWriter w(propPage, 256);

  // XXX remove commented-out lines later

  w.first();
  w.add(DictTabInfo::TableName, table.name);
  w.add(DictTabInfo::TableLoggedFlag, table.tableLoggedFlag);
  //w.add(DictTabInfo::TableKValue, 6);
  //w.add(DictTabInfo::MinLoadFactor, 70);
  //w.add(DictTabInfo::MaxLoadFactor, 80);
  w.add(DictTabInfo::FragmentTypeVal, (Uint32)table.fragmentType);
  //w.add(DictTabInfo::NoOfKeyAttr, 1);
  w.add(DictTabInfo::NoOfAttributes, (Uint32)table.columnCount);
  //w.add(DictTabInfo::NoOfNullable, (Uint32)0);
  //w.add(DictTabInfo::NoOfVariable, (Uint32)0);
  //w.add(DictTabInfo::KeyLength, 1);
  w.add(DictTabInfo::TableTypeVal, (Uint32)table.tableType);
  w.add(DictTabInfo::SingleUserMode, (Uint32)NDB_SUM_READ_WRITE);

  for (unsigned i = 0; i < table.columnCount; i++) {
    const SysColumn& column = table.columnList[i];
    ndbassert(column.pos == i);
    w.add(DictTabInfo::AttributeName, column.name);
    w.add(DictTabInfo::AttributeId, (Uint32)i);
    w.add(DictTabInfo::AttributeKeyFlag, (Uint32)column.keyFlag);
    w.add(DictTabInfo::AttributeStorageType, 
	  (Uint32)NDB_STORAGETYPE_MEMORY);
    w.add(DictTabInfo::AttributeArrayType, 
	  (Uint32)NDB_ARRAYTYPE_FIXED);
    w.add(DictTabInfo::AttributeNullableFlag, (Uint32)column.nullable);
    w.add(DictTabInfo::AttributeExtType, (Uint32)column.type);
    w.add(DictTabInfo::AttributeExtLength, (Uint32)column.length);
    w.add(DictTabInfo::AttributeEnd, (Uint32)true);
  }
  w.add(DictTabInfo::TableEnd, (Uint32)true);
  
  Uint32 length = w.getWordsUsed();
  LinearSectionPtr ptr[3];
  ptr[0].p = &propPage[0];
  ptr[0].sz = length;

  CreateTableReq* const req = (CreateTableReq*)signal->getDataPtrSend();
  req->senderData = index;
  req->senderRef = reference();
  sendSignal(DBDICT_REF, GSN_CREATE_TABLE_REQ, signal,
	     CreateTableReq::SignalLength, JBB, ptr, 1);
  return;
}//Ndbcntr::createSystableLab()

void Ndbcntr::execCREATE_TABLE_REF(Signal* signal) 
{
  jamEntry();
  progError(__LINE__,NDBD_EXIT_NDBREQUIRE, "CREATE_TABLE_REF");
  return;
}//Ndbcntr::execDICTTABREF()

void Ndbcntr::execCREATE_TABLE_CONF(Signal* signal) 
{
  jamEntry();
  CreateTableConf * const conf = (CreateTableConf*)signal->getDataPtrSend();
  //csystabId = conf->tableId;
  ndbrequire(conf->senderData < g_sysTableCount);
  const SysTable& table = *g_sysTableList[conf->senderData];
  table.tableId = conf->tableId;
  createSystableLab(signal, conf->senderData + 1);
  //startInsertTransactions(signal);
  return;
}//Ndbcntr::execDICTTABCONF()

/*******************************/
/*  DICTRELEASECONF            */
/*******************************/
void Ndbcntr::startInsertTransactions(Signal* signal) 
{
  jamEntry();

  ckey = 1;
  ctransidPhase = ZTRUE;
  signal->theData[0] = 0;
  signal->theData[1] = reference();
  sendSignal(DBTC_REF, GSN_TCSEIZEREQ, signal, 2, JBB);
  return;
}//Ndbcntr::startInsertTransactions()

/*******************************/
/*  TCSEIZECONF                */
/*******************************/
void Ndbcntr::execTCSEIZECONF(Signal* signal) 
{
  jamEntry();
  ctcConnectionP = signal->theData[1];
  crSystab7Lab(signal);
  return;
}//Ndbcntr::execTCSEIZECONF()

const unsigned int RowsPerCommit = 16;
void Ndbcntr::crSystab7Lab(Signal* signal) 
{
  UintR tkey;
  UintR Tmp;
  
  TcKeyReq * const tcKeyReq = (TcKeyReq *)&signal->theData[0];
  
  UintR reqInfo_Start = 0;
  tcKeyReq->setOperationType(reqInfo_Start, ZINSERT); // Insert
  tcKeyReq->setKeyLength    (reqInfo_Start, 1);
  tcKeyReq->setAIInTcKeyReq (reqInfo_Start, 5);
  tcKeyReq->setAbortOption  (reqInfo_Start, TcKeyReq::AbortOnError);

/* KEY LENGTH = 1, ATTRINFO LENGTH IN TCKEYREQ = 5 */
  cresponses = 0;
  const UintR guard0 = ckey + (RowsPerCommit - 1);
  for (Tmp = ckey; Tmp <= guard0; Tmp++) {
    UintR reqInfo = reqInfo_Start;
    if (Tmp == ckey) { // First iteration, Set start flag
      jam();
      tcKeyReq->setStartFlag(reqInfo, 1);
    } //if
    if (Tmp == guard0) { // Last iteration, Set commit flag
      jam();
      tcKeyReq->setCommitFlag(reqInfo, 1);      
      tcKeyReq->setExecuteFlag(reqInfo, 1);
    } //if
    if (ctransidPhase == ZTRUE) {
      jam();
      tkey = 0;
      tkey = tkey - Tmp;
    } else {
      jam();
      tkey = Tmp;
    }//if

    tcKeyReq->apiConnectPtr      = ctcConnectionP;
    tcKeyReq->attrLen            = 5;
    tcKeyReq->tableId            = g_sysTable_SYSTAB_0.tableId;
    tcKeyReq->requestInfo        = reqInfo;
    tcKeyReq->tableSchemaVersion = ZSYSTAB_VERSION;
    tcKeyReq->transId1           = 0;
    tcKeyReq->transId2           = ckey;

//-------------------------------------------------------------
// There is no optional part in this TCKEYREQ. There is one
// key word and five ATTRINFO words.
//-------------------------------------------------------------
    Uint32* tKeyDataPtr          = &tcKeyReq->scanInfo;
    Uint32* tAIDataPtr           = &tKeyDataPtr[1];

    tKeyDataPtr[0]               = tkey;

    AttributeHeader::init(&tAIDataPtr[0], 0, 1 << 2);
    tAIDataPtr[1]                = tkey;
    AttributeHeader::init(&tAIDataPtr[2], 1, 2 << 2);
    tAIDataPtr[3]                = (tkey << 16);
    tAIDataPtr[4]                = 1;    
    sendSignal(DBTC_REF, GSN_TCKEYREQ, signal, 
	       TcKeyReq::StaticLength + 6, JBB);
  }//for
  ckey = ckey + RowsPerCommit;
  return;
}//Ndbcntr::crSystab7Lab()

/*******************************/
/*  TCKEYCONF09                */
/*******************************/
void Ndbcntr::execTCKEYCONF(Signal* signal) 
{
  const TcKeyConf * const keyConf = (TcKeyConf *)&signal->theData[0];
  
  jamEntry();
  cgciSystab = keyConf->gci_hi;
  UintR confInfo = keyConf->confInfo;
  
  if (TcKeyConf::getMarkerFlag(confInfo)){
    Uint32 transId1 = keyConf->transId1;
    Uint32 transId2 = keyConf->transId2;
    signal->theData[0] = transId1;
    signal->theData[1] = transId2;
    sendSignal(DBTC_REF, GSN_TC_COMMIT_ACK, signal, 2, JBB);    
  }//if
  
  cresponses = cresponses + TcKeyConf::getNoOfOperations(confInfo);
  if (TcKeyConf::getCommitFlag(confInfo)){
    jam();
    ndbrequire(cresponses == RowsPerCommit);

    crSystab8Lab(signal);
    return;
  }
  return;
}//Ndbcntr::tckeyConfLab()

void Ndbcntr::crSystab8Lab(Signal* signal) 
{
  if (ckey < ZSIZE_SYSTAB) {
    jam();
    crSystab7Lab(signal);
    return;
  } else if (ctransidPhase == ZTRUE) {
    jam();
    ckey = 1;
    ctransidPhase = ZFALSE;
    // skip 2nd loop - tupleid sequence now created on first use
  }//if
  signal->theData[0] = ctcConnectionP;
  signal->theData[1] = reference();
  signal->theData[2] = 0;
  sendSignal(DBTC_REF, GSN_TCRELEASEREQ, signal, 2, JBB);
  return;
}//Ndbcntr::crSystab8Lab()

/*******************************/
/*  TCRELEASECONF              */
/*******************************/
void Ndbcntr::execTCRELEASECONF(Signal* signal) 
{
  jamEntry();
  waitpoint52Lab(signal);
  return;
}//Ndbcntr::execTCRELEASECONF()

void Ndbcntr::crSystab9Lab(Signal* signal) 
{
  signal->theData[0] = 0; // user ptr
  signal->theData[1] = reference();
  signal->theData[2] = 0;
  sendSignalWithDelay(DBDIH_REF, GSN_GETGCIREQ, signal, 100, 3);
  return;
}//Ndbcntr::crSystab9Lab()

/*******************************/
/*  GETGCICONF                 */
/*******************************/
void Ndbcntr::execGETGCICONF(Signal* signal) 
{
  jamEntry();

#ifndef NO_GCP
  if (signal->theData[1] < cgciSystab) {
    jam();
/*--------------------------------------*/
/* MAKE SURE THAT THE SYSTABLE IS       */
/* NOW SAFE ON DISK                     */
/*--------------------------------------*/
    crSystab9Lab(signal);
    return;
  }//if
#endif
  waitpoint52Lab(signal);
  return;
}//Ndbcntr::execGETGCICONF()

void Ndbcntr::execTCKEYREF(Signal* signal) 
{
  jamEntry();
  systemErrorLab(signal, __LINE__);
  return;
}//Ndbcntr::execTCKEYREF()

void Ndbcntr::execTCROLLBACKREP(Signal* signal) 
{
  jamEntry();
  systemErrorLab(signal, __LINE__);
  return;
}//Ndbcntr::execTCROLLBACKREP()

void Ndbcntr::execTCRELEASEREF(Signal* signal) 
{
  jamEntry();
  systemErrorLab(signal, __LINE__);
  return;
}//Ndbcntr::execTCRELEASEREF()

void Ndbcntr::execTCSEIZEREF(Signal* signal) 
{
  jamEntry();
  systemErrorLab(signal, __LINE__);
  return;
}//Ndbcntr::execTCSEIZEREF()


/*---------------------------------------------------------------------------*/
/*INITIALIZE VARIABLES AND RECORDS                                           */
/*---------------------------------------------------------------------------*/
void Ndbcntr::initData(Signal* signal) 
{
  c_start.reset();
  cmasterNodeId = 0;
  cnoStartNodes = 0;
  cnoWaitrep = 0;
}//Ndbcntr::initData()


/*---------------------------------------------------------------------------*/
/*RESET VARIABLES USED DURING THE START                                      */
/*---------------------------------------------------------------------------*/
void Ndbcntr::resetStartVariables(Signal* signal) 
{
  cnoStartNodes = 0;
  cnoWaitrep6 = cnoWaitrep7 = 0;
}//Ndbcntr::resetStartVariables()


/*---------------------------------------------------------------------------*/
// SEND THE SIGNAL
// INPUT                  CNDB_BLOCKS_COUNT
/*---------------------------------------------------------------------------*/
void Ndbcntr::sendNdbSttor(Signal* signal) 
{
  NdbBlocksRecPtr ndbBlocksPtr;

  ndbBlocksPtr.i = cndbBlocksCount;
  ptrCheckGuard(ndbBlocksPtr, ZSIZE_NDB_BLOCKS_REC, ndbBlocksRec);

  NdbSttor * const req = (NdbSttor*)signal->getDataPtrSend();
  req->senderRef = reference();
  req->nodeId = getOwnNodeId();
  req->internalStartPhase = cinternalStartphase;
  req->typeOfStart = ctypeOfStart;
  req->masterNodeId = cmasterNodeId;
  
  for (int i = 0; i < 16; i++) {
    // Garbage
    req->config[i] = 0x88776655;
    //cfgBlockPtr.p->cfgData[i];
  }
  
  //#define MAX_STARTPHASE 2
#ifdef TRACE_STTOR
  ndbout_c("sending NDB_STTOR(%d) to %s",
	   cinternalStartphase, 
	   getBlockName( refToBlock(ndbBlocksPtr.p->blockref)));
#endif
  sendSignal(ndbBlocksPtr.p->blockref, GSN_NDB_STTOR, signal, 22, JBB);
  cndbBlocksCount++;
}//Ndbcntr::sendNdbSttor()

/*---------------------------------------------------------------------------*/
// JUST SEND THE SIGNAL
/*---------------------------------------------------------------------------*/
void Ndbcntr::sendSttorry(Signal* signal) 
{
  signal->theData[3] = ZSTART_PHASE_1;
  signal->theData[4] = ZSTART_PHASE_2;
  signal->theData[5] = ZSTART_PHASE_3;
  signal->theData[6] = ZSTART_PHASE_4;
  signal->theData[7] = ZSTART_PHASE_5;
  signal->theData[8] = ZSTART_PHASE_6;
  // skip simulated phase 7
  signal->theData[9] = ZSTART_PHASE_8;
  signal->theData[10] = ZSTART_PHASE_9;
  signal->theData[11] = ZSTART_PHASE_END;
  sendSignal(NDBCNTR_REF, GSN_STTORRY, signal, 12, JBB);
}//Ndbcntr::sendSttorry()

void
Ndbcntr::execDUMP_STATE_ORD(Signal* signal)
{
  DumpStateOrd * const & dumpState = (DumpStateOrd *)&signal->theData[0];
  Uint32 arg = dumpState->args[0];

  if(arg == 13){
    infoEvent("Cntr: cstartPhase = %d, cinternalStartphase = %d, block = %d", 
	      cstartPhase, cinternalStartphase, cndbBlocksCount);
    infoEvent("Cntr: cmasterNodeId = %d", cmasterNodeId);
  }

  if (arg == DumpStateOrd::NdbcntrTestStopOnError){
    if (m_ctx.m_config.stopOnError() == true)
      ((Configuration&)m_ctx.m_config).stopOnError(false);
    
    const BlockReference tblockref = calcNdbCntrBlockRef(getOwnNodeId());
      
    SystemError * const sysErr = (SystemError*)&signal->theData[0];
    sysErr->errorCode = SystemError::TestStopOnError;
    sysErr->errorRef = reference();
    sendSignal(tblockref, GSN_SYSTEM_ERROR, signal, 
	       SystemError::SignalLength, JBA);
  }

  if (arg == DumpStateOrd::NdbcntrStopNodes)
  {
    NdbNodeBitmask mask;
    for(Uint32 i = 1; i<signal->getLength(); i++)
      mask.set(signal->theData[i]);

    StopReq* req = (StopReq*)signal->getDataPtrSend();
    req->senderRef = RNIL;
    req->senderData = 123;
    req->requestInfo = 0;
    req->singleuser = 0;
    req->singleUserApi = 0;
    mask.copyto(NdbNodeBitmask::Size, req->nodes);
    StopReq::setPerformRestart(req->requestInfo, 1);
    StopReq::setNoStart(req->requestInfo, 1);
    StopReq::setStopNodes(req->requestInfo, 1);
    StopReq::setStopAbort(req->requestInfo, 1);
    
    sendSignal(reference(), GSN_STOP_REQ, signal,
	       StopReq::SignalLength, JBB);
    return;
  }

}//Ndbcntr::execDUMP_STATE_ORD()

void Ndbcntr::updateNodeState(Signal* signal, const NodeState& newState) const{
  NodeStateRep * const stateRep = (NodeStateRep *)&signal->theData[0];

  if (newState.startLevel == NodeState::SL_STARTED)
  {
    CRASH_INSERTION(1000);
  }

  stateRep->nodeState = newState;
  stateRep->nodeState.masterNodeId = cmasterNodeId;
  stateRep->nodeState.setNodeGroup(c_nodeGroup);
  
  for(Uint32 i = 0; i<ALL_BLOCKS_SZ; i++){
    sendSignal(ALL_BLOCKS[i].Ref, GSN_NODE_STATE_REP, signal,
	       NodeStateRep::SignalLength, JBB);
  }
}

void
Ndbcntr::execRESUME_REQ(Signal* signal){
  //ResumeReq * const req = (ResumeReq *)&signal->theData[0];
  //ResumeRef * const ref = (ResumeRef *)&signal->theData[0];
  
  jamEntry();

  signal->theData[0] = NDB_LE_SingleUser;
  signal->theData[1] = 2;
  sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 2, JBB);

  //Uint32 senderData = req->senderData;
  //BlockReference senderRef = req->senderRef;
  NodeState newState(NodeState::SL_STARTED);		  
  updateNodeState(signal, newState);
  c_stopRec.stopReq.senderRef=0;
}

void
Ndbcntr::execSTOP_REQ(Signal* signal){
  StopReq * const req = (StopReq *)&signal->theData[0];
  StopRef * const ref = (StopRef *)&signal->theData[0];
  Uint32 singleuser  = req->singleuser;
  jamEntry();
  Uint32 senderData = req->senderData;
  BlockReference senderRef = req->senderRef;
  bool abort = StopReq::getStopAbort(req->requestInfo);
  bool stopnodes = StopReq::getStopNodes(req->requestInfo);

  if(!singleuser && 
     (getNodeState().startLevel < NodeState::SL_STARTED || 
      (abort && !stopnodes)))
  {
    /**
     * Node is not started yet
     *
     * So stop it quickly
     */
    jam();
    const Uint32 reqInfo = req->requestInfo;
    if(StopReq::getPerformRestart(reqInfo)){
      jam();
      StartOrd * startOrd = (StartOrd *)&signal->theData[0];
      startOrd->restartInfo = reqInfo;
      sendSignal(CMVMI_REF, GSN_START_ORD, signal, 1, JBA);
    } else {
      jam();
      sendSignal(CMVMI_REF, GSN_STOP_ORD, signal, 1, JBA);
    }
    return;
  }

  if(c_stopRec.stopReq.senderRef != 0 ||
     (cmasterNodeId == getOwnNodeId() && !c_start.m_starting.isclear()))
  {
    /**
     * Requested a system shutdown
     */
    if(!singleuser && StopReq::getSystemStop(req->requestInfo)){
      jam();
      sendSignalWithDelay(reference(), GSN_STOP_REQ, signal, 100,
			  StopReq::SignalLength);
      return;
    }

    /**
     * Requested a node shutdown
     */
    if(c_stopRec.stopReq.senderRef &&
       StopReq::getSystemStop(c_stopRec.stopReq.requestInfo))
      ref->errorCode = StopRef::SystemShutdownInProgress;
    else
      ref->errorCode = StopRef::NodeShutdownInProgress;
    ref->senderData = senderData;
    ref->masterNodeId = cmasterNodeId;
    
    if (senderRef != RNIL)
      sendSignal(senderRef, GSN_STOP_REF, signal, StopRef::SignalLength, JBB);
    return;
  }

  if (stopnodes && !abort)
  {
    jam();
    ref->errorCode = StopRef::UnsupportedNodeShutdown;
    ref->senderData = senderData;
    ref->masterNodeId = cmasterNodeId;
    if (senderRef != RNIL)
      sendSignal(senderRef, GSN_STOP_REF, signal, StopRef::SignalLength, JBB);
    return;
  }

  if (stopnodes && cmasterNodeId != getOwnNodeId())
  {
    jam();
    ref->errorCode = StopRef::MultiNodeShutdownNotMaster;
    ref->senderData = senderData;
    ref->masterNodeId = cmasterNodeId;
    if (senderRef != RNIL)
      sendSignal(senderRef, GSN_STOP_REF, signal, StopRef::SignalLength, JBB);
    return;
  }
  
  c_stopRec.stopReq = * req;
  c_stopRec.stopInitiatedTime = NdbTick_CurrentMillisecond();
  
  if (stopnodes)
  {
    jam();

    if(!c_stopRec.checkNodeFail(signal))
    {
      jam();
      return;
    }

    char buf[100];
    NdbNodeBitmask mask;
    mask.assign(NdbNodeBitmask::Size, c_stopRec.stopReq.nodes);
    infoEvent("Initiating shutdown abort of %s", mask.getText(buf));
    ndbout_c("Initiating shutdown abort of %s", mask.getText(buf));    

    WaitGCPReq * req = (WaitGCPReq*)&signal->theData[0];
    req->senderRef = reference();
    req->senderData = StopRecord::SR_BLOCK_GCP_START_GCP;
    req->requestType = WaitGCPReq::BlockStartGcp;
    sendSignal(DBDIH_REF, GSN_WAIT_GCP_REQ, signal, 
	       WaitGCPReq::SignalLength, JBB);
    return;
  }
  else if(!singleuser) 
  {
    if(StopReq::getSystemStop(c_stopRec.stopReq.requestInfo)) 
    {
      jam();
      if(StopReq::getPerformRestart(c_stopRec.stopReq.requestInfo))
      {
	((Configuration&)m_ctx.m_config).stopOnError(false);
      }
    }
    if(!c_stopRec.checkNodeFail(signal))
    {
      jam();
      return;
    }
    signal->theData[0] = NDB_LE_NDBStopStarted;
    signal->theData[1] = StopReq::getSystemStop(c_stopRec.stopReq.requestInfo) ? 1 : 0;
    sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 2, JBB);
  }
  else
  {
    signal->theData[0] = NDB_LE_SingleUser;
    signal->theData[1] = 0;
    sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 2, JBB);
  }

  NodeState newState(NodeState::SL_STOPPING_1, 
		     StopReq::getSystemStop(c_stopRec.stopReq.requestInfo));
  
   if(singleuser) {
     newState.setSingleUser(true);
     newState.setSingleUserApi(c_stopRec.stopReq.singleUserApi);
   }
  updateNodeState(signal, newState);
  signal->theData[0] = ZSHUTDOWN;
  sendSignalWithDelay(reference(), GSN_CONTINUEB, signal, 100, 1);
}

void
Ndbcntr::StopRecord::checkTimeout(Signal* signal){
  jamEntry();

  if(!cntr.getNodeState().getSingleUserMode())
    if(!checkNodeFail(signal)){
      jam();
      return;
    }

  switch(cntr.getNodeState().startLevel){
  case NodeState::SL_STOPPING_1:
    checkApiTimeout(signal);
    break;
  case NodeState::SL_STOPPING_2:
    checkTcTimeout(signal);
    break;
  case NodeState::SL_STOPPING_3:
    checkLqhTimeout_1(signal);
    break;
  case NodeState::SL_STOPPING_4:
    checkLqhTimeout_2(signal);
    break;
  case NodeState::SL_SINGLEUSER:
    break;
  default:
    ndbrequire(false);
  }
}

bool
Ndbcntr::StopRecord::checkNodeFail(Signal* signal){
  jam();
  if(StopReq::getSystemStop(stopReq.requestInfo)){
    jam();
    return true;
  }

  /**
   * Check if I can survive me stopping
   */
  NdbNodeBitmask ndbMask; 
  ndbMask.assign(cntr.c_startedNodes);

  if (StopReq::getStopNodes(stopReq.requestInfo))
  {
    NdbNodeBitmask tmp;
    tmp.assign(NdbNodeBitmask::Size, stopReq.nodes);

    NdbNodeBitmask ndbStopNodes;
    ndbStopNodes.assign(NdbNodeBitmask::Size, stopReq.nodes);
    ndbStopNodes.bitAND(ndbMask);
    ndbStopNodes.copyto(NdbNodeBitmask::Size, stopReq.nodes);

    ndbMask.bitANDC(tmp);

    bool allNodesStopped = true;
    int i ;
    for( i = 0; i < (int) NdbNodeBitmask::Size; i++ ){
      if ( stopReq.nodes[i] != 0 ){
        allNodesStopped = false;
        break;
      }
    }
  
    if ( allNodesStopped ) {
      StopConf * const stopConf = (StopConf *)&signal->theData[0];
      stopConf->senderData = stopReq.senderData;
      stopConf->nodeState  = (Uint32) NodeState::SL_NOTHING;
      cntr.sendSignal(stopReq.senderRef, GSN_STOP_CONF, signal,
                       StopConf::SignalLength, JBB);
      stopReq.senderRef = 0;
      return false;
    }

  }
  else
  {
    ndbMask.clear(cntr.getOwnNodeId());
  }
  
  CheckNodeGroups* sd = (CheckNodeGroups*)&signal->theData[0];
  sd->blockRef = cntr.reference();
  sd->requestType = CheckNodeGroups::Direct | CheckNodeGroups::ArbitCheck;
  sd->mask = ndbMask;
  cntr.EXECUTE_DIRECT(DBDIH, GSN_CHECKNODEGROUPSREQ, signal, 
		      CheckNodeGroups::SignalLength);
  jamEntry();
  switch (sd->output) {
  case CheckNodeGroups::Win:
  case CheckNodeGroups::Partitioning:
    return true;
    break;
  }
  
  StopRef * const ref = (StopRef *)&signal->theData[0];    
  
  ref->senderData = stopReq.senderData;
  ref->errorCode = StopRef::NodeShutdownWouldCauseSystemCrash;
  ref->masterNodeId = cntr.cmasterNodeId;
  
  const BlockReference bref = stopReq.senderRef;
  if (bref != RNIL)
    cntr.sendSignal(bref, GSN_STOP_REF, signal, StopRef::SignalLength, JBB);
  
  stopReq.senderRef = 0;

  if (cntr.getNodeState().startLevel != NodeState::SL_SINGLEUSER)
  {
    NodeState newState(NodeState::SL_STARTED); 
    cntr.updateNodeState(signal, newState);
  }

  signal->theData[0] = NDB_LE_NDBStopAborted;
  cntr.sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 1, JBB);
  
  return false;
}

void
Ndbcntr::StopRecord::checkApiTimeout(Signal* signal){
  const Int32 timeout = stopReq.apiTimeout; 
  const NDB_TICKS alarm = stopInitiatedTime + (NDB_TICKS)timeout;
  const NDB_TICKS now = NdbTick_CurrentMillisecond();
  if((timeout >= 0 && now >= alarm)){
    // || checkWithApiInSomeMagicWay)
    jam();
    NodeState newState(NodeState::SL_STOPPING_2, 
		       StopReq::getSystemStop(stopReq.requestInfo));
    if(stopReq.singleuser) {
      newState.setSingleUser(true);
      newState.setSingleUserApi(stopReq.singleUserApi);
    }
    cntr.updateNodeState(signal, newState);

    stopInitiatedTime = now;
  }

  signal->theData[0] = ZSHUTDOWN;
  cntr.sendSignalWithDelay(cntr.reference(), GSN_CONTINUEB, signal, 100, 1);
}

void
Ndbcntr::StopRecord::checkTcTimeout(Signal* signal){
  const Int32 timeout = stopReq.transactionTimeout;
  const NDB_TICKS alarm = stopInitiatedTime + (NDB_TICKS)timeout;
  const NDB_TICKS now = NdbTick_CurrentMillisecond();
  if((timeout >= 0 && now >= alarm)){
    // || checkWithTcInSomeMagicWay)
    jam();
    if(stopReq.getSystemStop(stopReq.requestInfo)  || stopReq.singleuser){
      jam();
      if(stopReq.singleuser) 
      {
	jam();
	AbortAllReq * req = (AbortAllReq*)&signal->theData[0];
	req->senderRef = cntr.reference();
	req->senderData = 12;
	cntr.sendSignal(DBTC_REF, GSN_ABORT_ALL_REQ, signal, 
			AbortAllReq::SignalLength, JBB);
      } 
      else
      {
	WaitGCPReq * req = (WaitGCPReq*)&signal->theData[0];
	req->senderRef = cntr.reference();
	req->senderData = StopRecord::SR_CLUSTER_SHUTDOWN;
	req->requestType = WaitGCPReq::CompleteForceStart;
	cntr.sendSignal(DBDIH_REF, GSN_WAIT_GCP_REQ, signal, 
			WaitGCPReq::SignalLength, JBB);
      }
    } else {
      jam();
      StopPermReq * req = (StopPermReq*)&signal->theData[0];
      req->senderRef = cntr.reference();
      req->senderData = 12;
      cntr.sendSignal(DBDIH_REF, GSN_STOP_PERM_REQ, signal, 
		      StopPermReq::SignalLength, JBB);
    }
    return;
  } 
  signal->theData[0] = ZSHUTDOWN;
  cntr.sendSignalWithDelay(cntr.reference(), GSN_CONTINUEB, signal, 100, 1);
}

void Ndbcntr::execSTOP_PERM_REF(Signal* signal){
  //StopPermRef* const ref = (StopPermRef*)&signal->theData[0];

  jamEntry();

  signal->theData[0] = ZSHUTDOWN;
  sendSignalWithDelay(reference(), GSN_CONTINUEB, signal, 100, 1);
}

void Ndbcntr::execSTOP_PERM_CONF(Signal* signal){
  jamEntry();
  
  AbortAllReq * req = (AbortAllReq*)&signal->theData[0];
  req->senderRef = reference();
  req->senderData = 12;
  sendSignal(DBTC_REF, GSN_ABORT_ALL_REQ, signal, 
	     AbortAllReq::SignalLength, JBB);
}

void Ndbcntr::execABORT_ALL_CONF(Signal* signal){
  jamEntry();
  if(c_stopRec.stopReq.singleuser) {
    jam();

    NodeState newState(NodeState::SL_SINGLEUSER);    
    newState.setSingleUser(true);
    newState.setSingleUserApi(c_stopRec.stopReq.singleUserApi);
    updateNodeState(signal, newState);    
    c_stopRec.stopInitiatedTime = NdbTick_CurrentMillisecond();

    StopConf * const stopConf = (StopConf *)&signal->theData[0];
    stopConf->senderData = c_stopRec.stopReq.senderData;
    stopConf->nodeState  = (Uint32) NodeState::SL_SINGLEUSER;
    sendSignal(c_stopRec.stopReq.senderRef, GSN_STOP_CONF, signal, StopConf::SignalLength, JBB);

    c_stopRec.stopReq.senderRef = 0; // the command is done

    signal->theData[0] = NDB_LE_SingleUser;
    signal->theData[1] = 1;
    signal->theData[2] = c_stopRec.stopReq.singleUserApi;
    sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 3, JBB);
  }
  else 
    {
      jam();
      NodeState newState(NodeState::SL_STOPPING_3, 
			 StopReq::getSystemStop(c_stopRec.stopReq.requestInfo));
      updateNodeState(signal, newState);
  
      c_stopRec.stopInitiatedTime = NdbTick_CurrentMillisecond();
      
      signal->theData[0] = ZSHUTDOWN;
      sendSignalWithDelay(reference(), GSN_CONTINUEB, signal, 100, 1);
    }
}

void Ndbcntr::execABORT_ALL_REF(Signal* signal){
  jamEntry();

  StopRef * const stopRef = (StopRef *)&signal->theData[0];
  stopRef->senderData = c_stopRec.stopReq.senderData;
  stopRef->errorCode = StopRef::TransactionAbortFailed;
  stopRef->masterNodeId = cmasterNodeId;
  sendSignal(c_stopRec.stopReq.senderRef, GSN_STOP_REF, signal, StopRef::SignalLength, JBB);
}

void
Ndbcntr::StopRecord::checkLqhTimeout_1(Signal* signal){
  const Int32 timeout = stopReq.readOperationTimeout;
  const NDB_TICKS alarm = stopInitiatedTime + (NDB_TICKS)timeout;
  const NDB_TICKS now = NdbTick_CurrentMillisecond();
  
  if((timeout >= 0 && now >= alarm)){
    // || checkWithLqhInSomeMagicWay)
    jam();
    
    ChangeNodeStateReq * req = (ChangeNodeStateReq*)&signal->theData[0];

    NodeState newState(NodeState::SL_STOPPING_4, 
		       StopReq::getSystemStop(stopReq.requestInfo));
    req->nodeState = newState;
    req->senderRef = cntr.reference();
    req->senderData = 12;
    cntr.sendSignal(DBLQH_REF, GSN_CHANGE_NODE_STATE_REQ, signal, 2, JBB);
    return;
  }
  signal->theData[0] = ZSHUTDOWN;
  cntr.sendSignalWithDelay(cntr.reference(), GSN_CONTINUEB, signal, 100, 1);
}

void Ndbcntr::execCHANGE_NODE_STATE_CONF(Signal* signal){
  jamEntry();
  signal->theData[0] = reference();
  signal->theData[1] = 12;
  sendSignal(DBDIH_REF, GSN_STOP_ME_REQ, signal, 2, JBB);
}

void Ndbcntr::execSTOP_ME_REF(Signal* signal){
  jamEntry();
  ndbrequire(false);
}


void Ndbcntr::execSTOP_ME_CONF(Signal* signal){
  jamEntry();

  NodeState newState(NodeState::SL_STOPPING_4, 
		     StopReq::getSystemStop(c_stopRec.stopReq.requestInfo));
  updateNodeState(signal, newState);
  
  c_stopRec.stopInitiatedTime = NdbTick_CurrentMillisecond();
  signal->theData[0] = ZSHUTDOWN;
  sendSignalWithDelay(reference(), GSN_CONTINUEB, signal, 100, 1);
}

void
Ndbcntr::StopRecord::checkLqhTimeout_2(Signal* signal){
  const Int32 timeout = stopReq.operationTimeout; 
  const NDB_TICKS alarm = stopInitiatedTime + (NDB_TICKS)timeout;
  const NDB_TICKS now = NdbTick_CurrentMillisecond();

  if((timeout >= 0 && now >= alarm)){
    // || checkWithLqhInSomeMagicWay)
    jam();
    if(StopReq::getPerformRestart(stopReq.requestInfo)){
      jam();
      StartOrd * startOrd = (StartOrd *)&signal->theData[0];
      startOrd->restartInfo = stopReq.requestInfo;
      cntr.sendSignal(CMVMI_REF, GSN_START_ORD, signal, 2, JBA);
    } else {
      jam();
      cntr.sendSignal(CMVMI_REF, GSN_STOP_ORD, signal, 1, JBA);
    }
    return;
  }
  signal->theData[0] = ZSHUTDOWN;
  cntr.sendSignalWithDelay(cntr.reference(), GSN_CONTINUEB, signal, 100, 1);
}

void Ndbcntr::execWAIT_GCP_REF(Signal* signal){
  jamEntry();
  
  //WaitGCPRef* const ref = (WaitGCPRef*)&signal->theData[0];

  WaitGCPReq * req = (WaitGCPReq*)&signal->theData[0];
  req->senderRef = reference();
  req->senderData = StopRecord::SR_CLUSTER_SHUTDOWN;
  req->requestType = WaitGCPReq::CompleteForceStart;
  sendSignal(DBDIH_REF, GSN_WAIT_GCP_REQ, signal, 
	     WaitGCPReq::SignalLength, JBB);
}

void Ndbcntr::execWAIT_GCP_CONF(Signal* signal){
  jamEntry();

  WaitGCPConf* conf = (WaitGCPConf*)signal->getDataPtr();

  switch(conf->senderData){
  case StopRecord::SR_BLOCK_GCP_START_GCP:
  {
    jam();
    /**
     * 
     */
    if(!c_stopRec.checkNodeFail(signal))
    {
      jam();
      goto unblock;
    }
    
    WaitGCPReq * req = (WaitGCPReq*)&signal->theData[0];
    req->senderRef = reference();
    req->senderData = StopRecord::SR_WAIT_COMPLETE_GCP;
    req->requestType = WaitGCPReq::CompleteIfRunning;

    sendSignal(DBDIH_REF, GSN_WAIT_GCP_REQ, signal, 
	       WaitGCPReq::SignalLength, JBB);
    return;
  }
  case StopRecord::SR_UNBLOCK_GCP_START_GCP:
  {
    jam();
    return;
  }
  case StopRecord::SR_WAIT_COMPLETE_GCP:
  {
    jam();
    if(!c_stopRec.checkNodeFail(signal))
    {
      jam();
      goto unblock;
    }

    NdbNodeBitmask tmp;
    tmp.assign(NdbNodeBitmask::Size, c_stopRec.stopReq.nodes);
    c_stopRec.m_stop_req_counter = tmp;
    NodeReceiverGroup rg(QMGR, tmp);
    StopReq * stopReq = (StopReq *)&signal->theData[0];
    * stopReq = c_stopRec.stopReq;
    stopReq->senderRef = reference();
    sendSignal(rg, GSN_STOP_REQ, signal, StopReq::SignalLength, JBA);
    c_stopRec.m_state = StopRecord::SR_QMGR_STOP_REQ; 
    return;
  }
  case StopRecord::SR_CLUSTER_SHUTDOWN:
  {
    jam();
    break;
  }
  }
  
  {  
    ndbrequire(StopReq::getSystemStop(c_stopRec.stopReq.requestInfo));
    NodeState newState(NodeState::SL_STOPPING_3, true); 
    
    /**
     * Inform QMGR so that arbitrator won't kill us
     */
    NodeStateRep * rep = (NodeStateRep *)&signal->theData[0];
    rep->nodeState = newState;
    rep->nodeState.masterNodeId = cmasterNodeId;
    rep->nodeState.setNodeGroup(c_nodeGroup);
    EXECUTE_DIRECT(QMGR, GSN_NODE_STATE_REP, signal, 
		   NodeStateRep::SignalLength);
    
    if(StopReq::getPerformRestart(c_stopRec.stopReq.requestInfo)){
      jam();
      StartOrd * startOrd = (StartOrd *)&signal->theData[0];
      startOrd->restartInfo = c_stopRec.stopReq.requestInfo;
      sendSignalWithDelay(CMVMI_REF, GSN_START_ORD, signal, 500, 
			  StartOrd::SignalLength);
    } else {
      jam();
      sendSignalWithDelay(CMVMI_REF, GSN_STOP_ORD, signal, 500, 1);
    }
    return;
  }
  
unblock:
  WaitGCPReq * req = (WaitGCPReq*)&signal->theData[0];
  req->senderRef = reference();
  req->senderData = StopRecord::SR_UNBLOCK_GCP_START_GCP;
  req->requestType = WaitGCPReq::UnblockStartGcp;
  sendSignal(DBDIH_REF, GSN_WAIT_GCP_REQ, signal, 
	     WaitGCPReq::SignalLength, JBB);
}

void
Ndbcntr::execSTOP_CONF(Signal* signal)
{
  jamEntry();
  StopConf *conf = (StopConf*)signal->getDataPtr();
  ndbrequire(c_stopRec.m_state == StopRecord::SR_QMGR_STOP_REQ);
  c_stopRec.m_stop_req_counter.clearWaitingFor(conf->nodeId);
  if (c_stopRec.m_stop_req_counter.done())
  {
    char buf[100];
    NdbNodeBitmask mask;
    mask.assign(NdbNodeBitmask::Size, c_stopRec.stopReq.nodes);
    infoEvent("Stopping of %s", mask.getText(buf));
    ndbout_c("Stopping of %s", mask.getText(buf));    

    /**
     * Kill any node...
     */
    FailRep * const failRep = (FailRep *)&signal->theData[0];
    failRep->failCause = FailRep::ZMULTI_NODE_SHUTDOWN;
    NodeReceiverGroup rg(QMGR, c_clusterNodes);
    Uint32 nodeId = 0;
    while ((nodeId = NdbNodeBitmask::find(c_stopRec.stopReq.nodes, nodeId+1))
	   != NdbNodeBitmask::NotFound)
    {
      failRep->failNodeId = nodeId;
      sendSignal(rg, GSN_FAIL_REP, signal, FailRep::SignalLength, JBA);
    }
    c_stopRec.m_state = StopRecord::SR_WAIT_NODE_FAILURES;
    return;
  }
}

void Ndbcntr::execSTTORRY(Signal* signal){
  jamEntry();
  c_missra.execSTTORRY(signal);
}

void Ndbcntr::execREAD_CONFIG_CONF(Signal* signal){
  jamEntry();
  c_missra.execREAD_CONFIG_CONF(signal);
}

void Ndbcntr::execSTART_ORD(Signal* signal){
  jamEntry();
  c_missra.execSTART_ORD(signal);
}

#define CLEAR_DX 13
#define CLEAR_LCP 3

void
Ndbcntr::clearFilesystem(Signal* signal)
{
  const Uint32 lcp = c_fsRemoveCount >= CLEAR_DX;
  
  FsRemoveReq * req  = (FsRemoveReq *)signal->getDataPtrSend();
  req->userReference = reference();
  req->userPointer   = 0;
  req->directory     = 1;
  req->ownDirectory  = 1;

  if (lcp == 0)
  {
    FsOpenReq::setVersion(req->fileNumber, 3);
    FsOpenReq::setSuffix(req->fileNumber, FsOpenReq::S_CTL); // Can by any...
    FsOpenReq::v1_setDisk(req->fileNumber, c_fsRemoveCount);
  }
  else
  {
    FsOpenReq::setVersion(req->fileNumber, 5);
    FsOpenReq::setSuffix(req->fileNumber, FsOpenReq::S_DATA);
    FsOpenReq::v5_setLcpNo(req->fileNumber, c_fsRemoveCount - CLEAR_DX);
    FsOpenReq::v5_setTableId(req->fileNumber, 0);
    FsOpenReq::v5_setFragmentId(req->fileNumber, 0);
  }
  sendSignal(NDBFS_REF, GSN_FSREMOVEREQ, signal, 
             FsRemoveReq::SignalLength, JBA);
  c_fsRemoveCount++;
}

void
Ndbcntr::execFSREMOVECONF(Signal* signal){
  jamEntry();
  if(c_fsRemoveCount == CLEAR_DX + CLEAR_LCP){
    jam();
    sendSttorry(signal);
  } else {
    jam();
    ndbrequire(c_fsRemoveCount < CLEAR_DX + CLEAR_LCP);
    clearFilesystem(signal);
  }//if
}

void Ndbcntr::Missra::execSTART_ORD(Signal* signal){
  signal->theData[0] = NDB_LE_NDBStartStarted;
  signal->theData[1] = NDB_VERSION;
  signal->theData[2] = NDB_MYSQL_VERSION_D;
  cntr.sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 3, JBB);

  currentBlockIndex = 0;
  sendNextREAD_CONFIG_REQ(signal);
}

void Ndbcntr::Missra::sendNextREAD_CONFIG_REQ(Signal* signal){

  if(currentBlockIndex < ALL_BLOCKS_SZ){
    jam();

    ReadConfigReq * req = (ReadConfigReq*)signal->getDataPtrSend();    
    req->senderData = 0;
    req->senderRef = cntr.reference();
    req->noOfParameters = 0;
    
    const BlockReference ref = readConfigOrder[currentBlockIndex];

#if 0 
    ndbout_c("sending READ_CONFIG_REQ to %s(ref=%x index=%d)", 
	     getBlockName( refToBlock(ref)),
	     ref,
	     currentBlockIndex);
#endif
    
    cntr.sendSignal(ref, GSN_READ_CONFIG_REQ, signal, 
		    ReadConfigReq::SignalLength, JBB);
    return;
  }
  
  /**
   * Finished...
   */
  currentStartPhase = 0;
  for(Uint32 i = 0; i<ALL_BLOCKS_SZ; i++){
    if(ALL_BLOCKS[i].NextSP < currentStartPhase)
      currentStartPhase = ALL_BLOCKS[i].NextSP;
  }
  
  currentBlockIndex = 0;
  sendNextSTTOR(signal);
}

void Ndbcntr::Missra::execREAD_CONFIG_CONF(Signal* signal){
  const ReadConfigConf * conf = (ReadConfigConf*)signal->getDataPtr();

  const Uint32 ref = conf->senderRef;
  ndbrequire(refToBlock(readConfigOrder[currentBlockIndex])
	     == refToBlock(ref));

  currentBlockIndex++;
  sendNextREAD_CONFIG_REQ(signal);
}

void Ndbcntr::Missra::execSTTORRY(Signal* signal){
  const BlockReference ref = signal->senderBlockRef();
  ndbrequire(refToBlock(ref) == refToBlock(ALL_BLOCKS[currentBlockIndex].Ref));
  
  /**
   * Update next start phase
   */
  for (Uint32 i = 3; i < 25; i++){
    jam();
    if (signal->theData[i] > currentStartPhase){
      jam();
      ALL_BLOCKS[currentBlockIndex].NextSP = signal->theData[i];
      break;
    }
  }    
  
  currentBlockIndex++;
  sendNextSTTOR(signal);
}

void Ndbcntr::Missra::sendNextSTTOR(Signal* signal){

  for(; currentStartPhase < 255 ;
      currentStartPhase++, g_currentStartPhase = currentStartPhase){
    jam();
    
    const Uint32 start = currentBlockIndex;

    if (currentStartPhase == ZSTART_PHASE_6)
    {
      // Ndbd has passed the critical startphases.
      // Change error handler from "startup" state
      // to normal state.
      ErrorReporter::setErrorHandlerShutdownType();
    }

    for(; currentBlockIndex < ALL_BLOCKS_SZ; currentBlockIndex++){
      jam();
      if(ALL_BLOCKS[currentBlockIndex].NextSP == currentStartPhase){
	jam();
	signal->theData[0] = 0;
	signal->theData[1] = currentStartPhase;
	signal->theData[2] = 0;
	signal->theData[3] = 0;
	signal->theData[4] = 0;
	signal->theData[5] = 0;
	signal->theData[6] = 0;
	signal->theData[7] = cntr.ctypeOfStart;
	
	const BlockReference ref = ALL_BLOCKS[currentBlockIndex].Ref;

#ifdef MAX_STARTPHASE
	ndbrequire(currentStartPhase <= MAX_STARTPHASE);
#endif

#ifdef TRACE_STTOR
	ndbout_c("sending STTOR(%d) to %s(ref=%x index=%d)", 
		 currentStartPhase,
		 getBlockName( refToBlock(ref)),
		 ref,
		 currentBlockIndex);
#endif
	
	cntr.sendSignal(ref, GSN_STTOR, signal, 8, JBB);
	
	return;
      }
    }
    
    currentBlockIndex = 0;

    NodeState newState(NodeState::SL_STARTING, currentStartPhase, 
		       (NodeState::StartType)cntr.ctypeOfStart);
    cntr.updateNodeState(signal, newState);
    
    if(start != 0){
      /**
       * At least one wanted this start phase,  report it
       */
      jam();
      signal->theData[0] = NDB_LE_StartPhaseCompleted;
      signal->theData[1] = currentStartPhase;
      signal->theData[2] = cntr.ctypeOfStart;    
      cntr.sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 3, JBB);
    }
  }

  signal->theData[0] = NDB_LE_NDBStartCompleted;
  signal->theData[1] = NDB_VERSION;
  signal->theData[2] = NDB_MYSQL_VERSION_D;
  cntr.sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 3, JBB);
  
  NodeState newState(NodeState::SL_STARTED);
  cntr.updateNodeState(signal, newState);

  /**
   * Backward
   */
  UpgradeStartup::sendCmAppChg(cntr, signal, 3); //RUN

  NdbNodeBitmask nodes = cntr.c_clusterNodes;
  Uint32 node = 0;
  while((node = nodes.find(node+1)) != NdbNodeBitmask::NotFound){
    if(cntr.getNodeInfo(node).m_version < MAKE_VERSION(3,5,0)){
      nodes.clear(node);
    }
  }
  
  NodeReceiverGroup rg(NDBCNTR, nodes);
  signal->theData[0] = cntr.getOwnNodeId();
  cntr.sendSignal(rg, GSN_CNTR_START_REP, signal, 1, JBB);
}

/**
 * Backward compatible code
 */
void
UpgradeStartup::sendCmAppChg(Ndbcntr& cntr, Signal* signal, Uint32 startLevel){
  
  if(cntr.getNodeInfo(cntr.cmasterNodeId).m_version >= MAKE_VERSION(3,5,0)){
    jam();
    return;
  }

  /**
   * Old NDB running
   */
  
  signal->theData[0] = startLevel;
  signal->theData[1] = cntr.getOwnNodeId();
  signal->theData[2] = 3 | ('N' << 8);
  signal->theData[3] = 'D' | ('B' << 8);
  signal->theData[4] = 0;
  signal->theData[5] = 0;
  signal->theData[6] = 0;
  signal->theData[7] = 0;
  signal->theData[8] = 0;
  signal->theData[9] = 0;
  signal->theData[10] = 0;
  signal->theData[11] = 0;
  
  NdbNodeBitmask nodes = cntr.c_clusterNodes;
  nodes.clear(cntr.getOwnNodeId());
  Uint32 node = 0;
  while((node = nodes.find(node+1)) != NdbNodeBitmask::NotFound){
    if(cntr.getNodeInfo(node).m_version < MAKE_VERSION(3,5,0)){
      cntr.sendSignal(cntr.calcQmgrBlockRef(node),
		      GSN_CM_APPCHG, signal, 12, JBB);
    } else {
      cntr.c_startedNodes.set(node); // Fake started
    }
  }
}

void
UpgradeStartup::execCM_APPCHG(SimulatedBlock & block, Signal* signal){
  Uint32 state = signal->theData[0];
  Uint32 nodeId = signal->theData[1];
  if(block.number() == QMGR){
    Ndbcntr& cntr = * (Ndbcntr*)globalData.getBlock(CNTR);
    switch(state){
    case 0: // ZADD
      break;
    case 2: // ZSTART
      break;
    case 3: // ZRUN{
      cntr.c_startedNodes.set(nodeId);

      Uint32 recv = cntr.c_startedNodes.count();
      Uint32 cnt = cntr.c_clusterNodes.count();
      if(recv + 1 == cnt){ //+1 == own node
	/**
	 * Check master
	 */
	sendCntrMasterReq(cntr, signal, 0);
      }
      return;
    }
  }
  block.progError(__LINE__,NDBD_EXIT_NDBREQUIRE,
		  "UpgradeStartup::execCM_APPCHG");
}

void
UpgradeStartup::sendCntrMasterReq(Ndbcntr& cntr, Signal* signal, Uint32 n){
  Uint32 node = cntr.c_startedNodes.find(n);
  if(node != NdbNodeBitmask::NotFound && 
     (node == cntr.getOwnNodeId() || 
      cntr.getNodeInfo(node).m_version >= MAKE_VERSION(3,5,0))){
    node = cntr.c_startedNodes.find(node+1);
  }
  
  if(node == NdbNodeBitmask::NotFound){
    cntr.progError(__LINE__,NDBD_EXIT_NDBREQUIRE,
		   "UpgradeStartup::sendCntrMasterReq "
		   "NdbNodeBitmask::NotFound");
  }

  CntrMasterReq * const cntrMasterReq = (CntrMasterReq*)&signal->theData[0];
  cntr.c_clusterNodes.copyto(NdbNodeBitmask::Size, cntrMasterReq->theNodes);
  NdbNodeBitmask::clear(cntrMasterReq->theNodes, cntr.getOwnNodeId());
  cntrMasterReq->userBlockRef = 0;
  cntrMasterReq->userNodeId = cntr.getOwnNodeId();
  cntrMasterReq->typeOfStart = NodeState::ST_INITIAL_NODE_RESTART;
  cntrMasterReq->noRestartNodes = cntr.c_clusterNodes.count() - 1;
  cntr.sendSignal(cntr.calcNdbCntrBlockRef(node), GSN_CNTR_MASTERREQ,
		  signal, CntrMasterReq::SignalLength, JBB);
}

void
UpgradeStartup::execCNTR_MASTER_REPLY(SimulatedBlock & block, Signal* signal){
  Uint32 gsn = signal->header.theVerId_signalNumber;
  Uint32 node = refToNode(signal->getSendersBlockRef());
  if(block.number() == CNTR){
    Ndbcntr& cntr = (Ndbcntr&)block;
    switch(gsn){
    case GSN_CNTR_MASTERREF:
      sendCntrMasterReq(cntr, signal, node + 1);
      return;
      break;
    case GSN_CNTR_MASTERCONF:{
      CntrStartConf* conf = (CntrStartConf*)signal->getDataPtrSend();
      conf->startGci = 0;
      conf->masterNodeId = node;
      conf->noStartNodes = 1;
      conf->startType = NodeState::ST_INITIAL_NODE_RESTART;
      NdbNodeBitmask mask;
      mask.clear();
      mask.copyto(NdbNodeBitmask::Size, conf->startedNodes);
      mask.clear();
      mask.set(cntr.getOwnNodeId());
      mask.copyto(NdbNodeBitmask::Size, conf->startingNodes);
      cntr.execCNTR_START_CONF(signal);
      return;
    }
    }
  }
  block.progError(__LINE__,NDBD_EXIT_NDBREQUIRE,
		  "UpgradeStartup::execCNTR_MASTER_REPLY");
}

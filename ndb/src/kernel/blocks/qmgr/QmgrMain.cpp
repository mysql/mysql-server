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


#define QMGR_C
#include "Qmgr.hpp"
#include <pc.hpp>
#include <NdbTick.h>
#include <signaldata/EventReport.hpp>
#include <signaldata/SetVarReq.hpp>
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

#include <ndb_version.h>

#ifdef DEBUG_ARBIT
#include <NdbOut.hpp>
#endif

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
  hbNodePtr.p->alarmCount = 0;
  return;
}//Qmgr::execCM_HEARTBEAT()

/*******************************/
/* CM_NODEINFOREF             */
/*******************************/
void Qmgr::execCM_NODEINFOREF(Signal* signal) 
{
  jamEntry();
  systemErrorLab(signal);
  return;
}//Qmgr::execCM_NODEINFOREF()

/*******************************/
/* CONTINUEB                  */
/*******************************/
void Qmgr::execCONTINUEB(Signal* signal) 
{
  UintR tdata0;
  UintR tcontinuebType;

  jamEntry();
  tcontinuebType = signal->theData[0];
  tdata0 = signal->theData[1];
  switch (tcontinuebType) {
  case ZREGREQ_TIMELIMIT:
    jam();
    if (cstartNo == tdata0) {
      jam();
      regreqTimelimitLab(signal, signal->theData[2]);
      return;
    }
    break;
  case ZREGREQ_MASTER_TIMELIMIT:
    jam();
    if (cstartNo != tdata0) {
      jam();
      return;
    }//if
    if (cpresidentBusy != ZTRUE) {
      jam();
      return;
    }//if
    failReportLab(signal, cstartNode, FailRep::ZSTART_IN_REGREQ);
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
  default:
    jam();
    // ZCOULD_NOT_OCCUR_ERROR;
    systemErrorLab(signal);
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
  cstartseq = signal->theData[1];
  csignalkey = signal->theData[6];
  if (cstartseq == 1) {
    jam();
    initData(signal);
  }
  
  setNodeInfo(getOwnNodeId()).m_version = NDB_VERSION;

  sendSttorryLab(signal);
  return;
}//Qmgr::execSTTOR()

void Qmgr::sendSttorryLab(Signal* signal) 
{
/****************************<*/
/*< STTORRY                  <*/
/****************************<*/
  signal->theData[0] = csignalkey;
  signal->theData[1] = 3;
  signal->theData[2] = 2;
  signal->theData[3] = 2;
  signal->theData[4] = 255;
  sendSignal(NDBCNTR_REF, GSN_STTORRY, signal, 5, JBB);
  return;
}//Qmgr::sendSttorryLab()

/*
4.2.2 CM_INIT                   */
/**--------------------------------------------------------------------------
 * This signal is sent by the CLUSTERCTRL block. 
 * It initiates the QMGR and provides needed info about the 
 * cluster configuration (read from file). 
 *
 * The signal starts all QMGR functions. 
 * It is possible to register applications before this but the QMGR will
 * not be active before the registration face is complete. 
 *
 * The CM_INIT will result in a one CM_NODEINFOREQ for each ndb node.
 * We will also send a CONTINUEB to ourselves as a timelimit. 
 * If anyone sends a REF, CONF or a ( REQ with a lower NODENO than us ) during
 * this time, we are not the president . 
 *--------------------------------------------------------------------------*/
/*******************************/
/* CM_INIT                    */
/*******************************/
void Qmgr::execCM_INIT(Signal* signal) 
{
  jamEntry();

  CmInit * const cmInit = (CmInit *)&signal->theData[0];
  
  for(unsigned int i = 0; i<NdbNodeBitmask::Size; i++)
    cnodemask[i] = cmInit->allNdbNodes[i];
  
  cnoOfNodes = 0;
  setHbDelay(cmInit->heartbeatDbDb);
  setHbApiDelay(cmInit->heartbeatDbApi);
  setArbitTimeout(cmInit->arbitTimeout);
  arbitRec.state = ARBIT_NULL;          // start state for all nodes
  arbitRec.apiMask[0].clear();          // prepare for ARBIT_CFG
  
  NodeRecPtr nodePtr;
  for (nodePtr.i = 0; nodePtr.i < MAX_NDB_NODES; nodePtr.i++) {
    jam();
    ptrAss(nodePtr, nodeRec);
    if (NdbNodeBitmask::get(cnodemask, nodePtr.i)) {
      jam();
      
      nodePtr.p->blockRef = calcQmgrBlockRef(nodePtr.i);
      nodePtr.p->phase = ZINIT;     /* Not added to cluster        */
      cnoOfNodes = cnoOfNodes + 1;  /* Should never be changed after this loop. */
      ndbrequire(getNodeInfo(nodePtr.i).m_type == NodeInfo::DB);
    } else {
      jam();
      nodePtr.p->phase = ZBLOCKED;
    }//if
  }//for
  for (nodePtr.i = MAX_NDB_NODES; nodePtr.i < MAX_NODES; nodePtr.i++) {
    jam();
    ptrAss(nodePtr, nodeRec);
    nodePtr.p->phase = ZBLOCKED;
  }//for

  nodePtr.i = getOwnNodeId();
  ptrAss(nodePtr, nodeRec);
  nodePtr.p->phase = ZINIT;
  nodePtr.p->m_connected = true;

  /****************************<*/
  /*< CM_INFOREQ               <*/
  /****************************<*/
  signal->theData[0] = reference();
  signal->theData[1] = getOwnNodeId();
  sendSignal(CMVMI_REF, GSN_CM_INFOREQ, signal, 2, JBB);
  return;
}//Qmgr::execCM_INIT()

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
  NodeRecPtr connectNodePtr;
  connectNodePtr.i = signal->theData[0];
  ptrCheckGuard(connectNodePtr, MAX_NODES, nodeRec);
  connectNodePtr.p->m_connected = true;
  
  return;
}//Qmgr::execCONNECT_REP()

/*******************************/
/* CM_INFOCONF                */
/*******************************/
void Qmgr::execCM_INFOCONF(Signal* signal) 
{
  cpresident = ZNIL;
  cpresidentCandidate = getOwnNodeId();
  cpresidentAlive = ZFALSE;
  c_stopElectionTime = NdbTick_CurrentMillisecond();
  c_stopElectionTime += 30000; // 30s
  cmInfoconf010Lab(signal);
  
#if 0
  /*****************************************************/
  /* Allow the CLUSTER CONTROL to send STTORRY         */
  /* CM_RUN                                            */
  /* so we can receive APPL_REGREQ from applications.  */
  /*****************************************************/
  signal->theData[0] = 0;
  sendSignal(CMVMI_REF, GSN_CM_RUN, signal, 1, JBB);
#endif
  return;
}//Qmgr::execCM_INFOCONF()

void Qmgr::cmInfoconf010Lab(Signal* signal) 
{
  NodeRecPtr nodePtr;
  c_regReqReqSent = c_regReqReqRecv = 0;
  for (nodePtr.i = 1; nodePtr.i < MAX_NDB_NODES; nodePtr.i++) {
    jam();
    ptrAss(nodePtr, nodeRec);
    
    if(getNodeInfo(nodePtr.i).getType() != NodeInfo::DB)
      continue;

    if(!nodePtr.p->m_connected)
      continue;
    
    c_regReqReqSent++;
    CmRegReq * const cmRegReq = (CmRegReq *)&signal->theData[0];
    cmRegReq->blockRef = reference();
    cmRegReq->nodeId   = getOwnNodeId();
    cmRegReq->version  = NDB_VERSION;
    sendSignal(nodePtr.p->blockRef, GSN_CM_REGREQ, signal, 
	       CmRegReq::SignalLength, JBB);
  }
  cstartNo = cstartNo + 1;

  //----------------------------------------
  /* Wait for a while. When it returns    */
  /* we will check if we got any CM_REGREF*/
  /* or CM_REGREQ (lower nodeid than our  */
  /* own).                                */
  //----------------------------------------
  signal->theData[0] = ZREGREQ_TIMELIMIT;
  signal->theData[1] = cstartNo;
  signal->theData[2] = 0;
  sendSignalWithDelay(QMGR_REF, GSN_CONTINUEB, signal, 3 * cdelayRegreq, 3);
  cwaitContinuebPhase1 = ZTRUE;
  creadyDistCom = ZTRUE;
  return;
}//Qmgr::cmInfoconf010Lab()

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
void Qmgr::execCM_REGREQ(Signal* signal) 
{
  NodeRecPtr addNodePtr;
  jamEntry();

  CmRegReq * const cmRegReq = (CmRegReq *)&signal->theData[0];
  const BlockReference Tblockref = cmRegReq->blockRef;
  const Uint32 startingVersion = cmRegReq->version;
  addNodePtr.i = cmRegReq->nodeId;
  
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

  ptrCheckGuard(addNodePtr, MAX_NDB_NODES, nodeRec);

  if (cpresident != getOwnNodeId()){
    jam();                      
    if (cpresident == ZNIL) {
      /***
       * We don't know the president. 
       * If the node to be added has lower node id 
       * than our president cancidate. Set it as 
       * candidate
       */
      jam(); 
      if (addNodePtr.i < cpresidentCandidate) {
	jam();
	cpresidentCandidate = addNodePtr.i;
      }//if
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

  if (cpresidentBusy == ZTRUE) {
    jam();
    /**
    * President busy by adding another node
    */
    sendCmRegrefLab(signal, Tblockref, CmRegRef::ZBUSY_PRESIDENT);
    return;
  }//if
  
  if (cacceptRegreq == ZFALSE && 
      getNodeState().startLevel != NodeState::SL_STARTING) {        
    jam();
    /**
     * These checks are really confusing! 
     * The variables that is being checked are probably not
     * set in the correct places.
     */
    sendCmRegrefLab(signal, Tblockref, CmRegRef::ZBUSY);
    return;
  }//if

  if (ctoStatus == Q_ACTIVE) {   
    jam();
    /**
     * Active taking over as president
     */
    sendCmRegrefLab(signal, Tblockref, CmRegRef::ZBUSY_TO_PRES);
    return;
  }//if

  if (addNodePtr.p->phase == ZBLOCKED) {
    jam(); 
    /** 
     * The new node is not in config file
     */
    sendCmRegrefLab(signal, Tblockref, CmRegRef::ZNOT_IN_CFG);
    return;
  } 

  if (addNodePtr.p->phase != ZINIT) {
    jam();
    sendCmRegrefLab(signal, Tblockref, CmRegRef::ZNOT_DEAD);
    return;
  }//if
  
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
  cpresidentBusy = ZTRUE;

  /**
   * Indicates that we are busy with node start/restart and do 
   * not accept another start until this node is up and running 
   * (cpresidentBusy is released a little too early to use for this
   * purpose).
   */
  cacceptRegreq = ZFALSE;
  cstartNo = cstartNo + 1;
  cstartNode = addNodePtr.i;
  signal->theData[0] = ZREGREQ_MASTER_TIMELIMIT;
  signal->theData[1] = cstartNo;
  sendSignalWithDelay(QMGR_REF, GSN_CONTINUEB, signal, 30000, 2);
  UintR TdynId = getDynamicId(signal);	/* <- CDYNAMIC_ID */
  prepareAdd(signal, addNodePtr.i);
  setNodeInfo(addNodePtr.i).m_version = startingVersion;

  /**
   * Send "prepare for adding a new node" to all 
   * running nodes in cluster + the new node.
   * Give permission to the new node to join the
   * cluster
   */
  /*******************************/
  /*< CM_REGCONF                <*/
  /*******************************/
  
  CmRegConf * const cmRegConf = (CmRegConf *)&signal->theData[0];
  
  cmRegConf->presidentBlockRef = reference();
  cmRegConf->presidentNodeId   = getOwnNodeId();
  cmRegConf->presidentVersion  = getNodeInfo(getOwnNodeId()).m_version;
  cmRegConf->dynamicId         = TdynId;
  for(unsigned int i = 0; i<NdbNodeBitmask::Size; i++)
    cmRegConf->allNdbNodes[i] = cnodemask[i];
  
  sendSignal(Tblockref, GSN_CM_REGCONF, signal, 
	     CmRegConf::SignalLength, JBB);
  return;
}//Qmgr::execCM_REGREQ()

void Qmgr::sendCmRegrefLab(Signal* signal, BlockReference TBRef, 
			   CmRegRef::ErrorCode Terror) 
{
  CmRegRef* ref = (CmRegRef*)signal->getDataPtrSend();
  ref->blockRef = reference();
  ref->nodeId = getOwnNodeId();
  ref->errorCode = Terror;
  ref->presidentCandidate = cpresidentCandidate;
  sendSignal(TBRef, GSN_CM_REGREF, signal, 
	     CmRegRef::SignalLength, JBB);
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
  NodeRecPtr myNodePtr;
  NodeRecPtr nodePtr;
  NodeRecPtr presidentNodePtr;
  jamEntry();

  CmRegConf * const cmRegConf = (CmRegConf *)&signal->theData[0];
  cwaitContinuebPhase1 = ZFALSE;
  cwaitContinuebPhase2 = ZTRUE;

  if (!ndbCompatible_ndb_ndb(NDB_VERSION, cmRegConf->presidentVersion)) {
    jam();
    char buf[128];
    snprintf(buf,sizeof(buf),"incompatible version own=0x%x other=0x%x, shutting down", NDB_VERSION, cmRegConf->presidentVersion);
    systemErrorLab(signal, buf);
    return;
  }

  /**
   * Check if all necessary connections has been established
   */
  for (nodePtr.i = 1; nodePtr.i < MAX_NDB_NODES; nodePtr.i++) {
    jam();
    if (NodeBitmask::get(cmRegConf->allNdbNodes, nodePtr.i) == true){
      jam();
      ptrAss(nodePtr, nodeRec);
      if (!nodePtr.p->m_connected) {
        jam();
	
	/**
	 * Missing connection
	 */
#ifdef VM_TRACE
	ndbout_c("Resending CM_REGCONF, node %d is not connected", nodePtr.i);
	ndbout << " presidentBlockRef="<<cmRegConf->presidentBlockRef<<endl
	       << " presidentNodeId="<<cmRegConf->presidentNodeId<<endl
	       << " presidentVersion="<<cmRegConf->presidentVersion<<endl
	       << " dynamicId="<<cmRegConf->dynamicId<<endl;
#endif
	for(unsigned int i = 0; i<NdbNodeBitmask::Size; i++) {
	  jam();
#ifdef VM_TRACE
	  ndbout << " " << i << ": "
		 << hex << cmRegConf->allNdbNodes[i]<<endl;
#endif	
        }
	sendSignalWithDelay(reference(), GSN_CM_REGCONF, signal, 100,
			    signal->getLength());
	return;
      }
    }
  }
  
  cpdistref    = cmRegConf->presidentBlockRef;
  cpresident   = cmRegConf->presidentNodeId;
  UintR TdynamicId   = cmRegConf->dynamicId;
  for(unsigned int i = 0; i<NdbNodeBitmask::Size; i++)
    cnodemask[i] = cmRegConf->allNdbNodes[i];

/*--------------------------------------------------------------*/
// Send this as an EVENT REPORT to inform about hearing about
// other NDB node proclaiming to be president.
/*--------------------------------------------------------------*/
  signal->theData[0] = EventReport::CM_REGCONF;
  signal->theData[1] = getOwnNodeId();
  signal->theData[2] = cpresident;
  signal->theData[3] = TdynamicId;
  sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 4, JBB);

  myNodePtr.i = getOwnNodeId();
  ptrCheckGuard(myNodePtr, MAX_NDB_NODES, nodeRec);
  myNodePtr.p->ndynamicId = TdynamicId;
  presidentNodePtr.i = cpresident;
  ptrCheckGuard(presidentNodePtr, MAX_NDB_NODES, nodeRec);
  cpdistref = presidentNodePtr.p->blockRef;

  CmNodeInfoReq * const req = (CmNodeInfoReq*)signal->getDataPtrSend();
  req->nodeId = getOwnNodeId();
  req->dynamicId = myNodePtr.p->ndynamicId;
  req->version = getNodeInfo(getOwnNodeId()).m_version;

  for (nodePtr.i = 1; nodePtr.i < MAX_NDB_NODES; nodePtr.i++) {
    jam();
    if (NdbNodeBitmask::get(cnodemask, nodePtr.i) == true){
      jam();
      ptrAss(nodePtr, nodeRec);
      switch(nodePtr.p->phase){
      case ZINIT:  		/* All nodes start in phase INIT         */
	jam();
	break;
      case ZWAITING:  		/* Node is connecting to cluster         */
	jam();
	break;
      case ZRUNNING:  		/* Node is running in the cluster        */
	jam();
	break;
      case ZBLOCKED:  		/* Node is blocked from the cluster      */
	jam();
	break;
      case ZWAIT_PRESIDENT: 
	jam();
	break;
      case ZDEAD:
	jam();
	break;
      case ZAPI_ACTIVE: 		/* API IS RUNNING IN NODE                */
	jam();
	break;
      case ZFAIL_CLOSING:         /* API/NDB IS DISCONNECTING              */
	jam();
	break;
      case ZPREPARE_FAIL:         /* PREPARATION FOR FAILURE               */
	jam();
	break;
      case ZAPI_INACTIVE:        /* Inactive API */
	jam();
	break;
      default:
	jam();
	ndbout << "phase="<<nodePtr.p->phase<<endl;
	break;
      }
      ndbrequire(nodePtr.p->phase == ZINIT);
      ndbrequire(nodePtr.i != getOwnNodeId());
      nodePtr.p->phase = ZWAITING;
      
      sendSignal(nodePtr.p->blockRef, GSN_CM_NODEINFOREQ, 
		 signal, CmNodeInfoReq::SignalLength, JBB);
    }
  }
  return;
}//Qmgr::execCM_REGCONF()

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
void Qmgr::execCM_REGREF(Signal* signal) 
{
  jamEntry();
  c_regReqReqRecv++;

  // Ignore block reference in data[0]
  UintR TaddNodeno = signal->theData[1];
  UintR TrefuseReason = signal->theData[2];
  Uint32 candidate = signal->theData[3];

  if(candidate != cpresidentCandidate){
    jam();
    c_regReqReqRecv = c_regReqReqSent + 1;
  }

  switch (TrefuseReason) {
  case CmRegRef::ZINCOMPATIBLE_VERSION:
    jam();
    systemErrorLab(signal, "incompatible version, connection refused by running ndb node");
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
    progError(__LINE__, ERR_NODE_NOT_IN_CONFIG);
    break;
  case CmRegRef::ZNOT_DEAD:
    jam();
    if(TaddNodeno == getOwnNodeId() && cpresident == getOwnNodeId()){
      jam();
      cwaitContinuebPhase1 = ZFALSE;
      cwaitContinuebPhase2 = ZFALSE;
      return;
    }
    progError(__LINE__, ERR_NODE_NOT_DEAD);
    break;
  case CmRegRef::ZELECTION:
    jam();
    if (cwaitContinuebPhase1 == ZFALSE) {
      jam();
      signal->theData[3] = 1;
    } else if (cpresidentCandidate > TaddNodeno) {
      jam();
//----------------------------------------
/* We may already have a candidate      */
/* choose the lowest nodeno             */
//----------------------------------------
      signal->theData[3] = 2;
      cpresidentCandidate = TaddNodeno;
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
  signal->theData[0] = EventReport::CM_REGREF;
  signal->theData[1] = getOwnNodeId();
  signal->theData[2] = TaddNodeno;
//-----------------------------------------
// signal->theData[3] filled in above
//-----------------------------------------
  sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 4, JBB);

  if(cpresidentAlive == ZTRUE){
    jam();
    return;
  }
  
  if(c_regReqReqSent != c_regReqReqRecv){
    jam();
    return;
  }
  
  if(cpresidentCandidate != getOwnNodeId()){
    jam();
    return;
  }

  /**
   * All configured nodes has agreed
   */
  Uint64 now = NdbTick_CurrentMillisecond();
  if((c_regReqReqRecv == cnoOfNodes) || now > c_stopElectionTime){
    jam();
    
    electionWon();
#if 1
    signal->theData[0] = 0;
    sendSignal(CMVMI_REF, GSN_CM_RUN, signal, 1, JBB);
#endif
    /**
     * Start timer handling 
     */
    signal->theData[0] = ZTIMER_HANDLING;
    sendSignal(QMGR_REF, GSN_CONTINUEB, signal, 10, JBB);
  }
  
  return;
}//Qmgr::execCM_REGREF()

void
Qmgr::electionWon(){
  NodeRecPtr myNodePtr;
  cpresident = getOwnNodeId(); /* This node becomes president. */
  myNodePtr.i = getOwnNodeId();
  ptrCheckGuard(myNodePtr, MAX_NDB_NODES, nodeRec);
  
  myNodePtr.p->phase = ZRUNNING;
  cpdistref = reference();
  cclustersize = 1;
  cneighbourl = ZNIL;
  cneighbourh = ZNIL;
  myNodePtr.p->ndynamicId = 1;

  cpresidentAlive = ZTRUE;
  c_stopElectionTime = ~0;
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
void Qmgr::regreqTimelimitLab(Signal* signal, UintR callTime) 
{
  if (cwaitContinuebPhase1 == ZFALSE) {
    if (cwaitContinuebPhase2 == ZFALSE) {
      jam();
      return;
    } else {
      jam();
      if (callTime < 10) {
      /*-------------------------------------------------------------*/
      // We experienced a time-out of inclusion. Give it another few
      // seconds before crashing.
      /*-------------------------------------------------------------*/
        signal->theData[0] = ZREGREQ_TIMELIMIT;
        signal->theData[1] = cstartNo;
        signal->theData[2] = callTime + 1;
        sendSignalWithDelay(QMGR_REF, GSN_CONTINUEB, signal, 3000, 3);
        return;
      }//if
      /*-------------------------------------------------------------*/
      /*       WE HAVE COME HERE BECAUSE THE INCLUSION SUFFERED FROM */
      /*       TIME OUT. WE CRASH AND RESTART.                       */
      /*-------------------------------------------------------------*/
      systemErrorLab(signal);
      return;
    }//if
  } else {
    jam();
    cwaitContinuebPhase1 = ZFALSE;
  }//if
 
  cmInfoconf010Lab(signal);
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
  NodeRecPtr replyNodePtr;
  NodeRecPtr nodePtr;
  jamEntry();

  CmNodeInfoConf * const conf = (CmNodeInfoConf*)signal->getDataPtr();

  replyNodePtr.i = conf->nodeId;
  ptrCheckGuard(replyNodePtr, MAX_NDB_NODES, nodeRec);
  replyNodePtr.p->ndynamicId = conf->dynamicId;
  setNodeInfo(replyNodePtr.i).m_version = conf->version;
  replyNodePtr.p->phase = ZRUNNING;
  
  /**
   * A node in the cluster has replied nodeinfo about himself. 
   * He is already running in the cluster.
   */
  for (nodePtr.i = 1; nodePtr.i < MAX_NDB_NODES; nodePtr.i++) {
    jam();
    ptrAss(nodePtr, nodeRec);
    if (nodePtr.p->phase == ZWAITING) {
      if (nodePtr.i != getOwnNodeId()) {
        jam();
        return;
      }//if
    }//if
  }//for

  /**********************************************<*/
  /* Send an ack. back to the president.          */
  /* CM_ACKADD                                    */
  /* The new node has been registered by all      */
  /* running nodes and has stored nodeinfo about  */
  /* all running nodes. The new node has to wait  */
  /* for CM_ADD (commit) from president to become */
  /* a running node in the cluster.               */
  /**********************************************<*/
  CmAckAdd * const cmAckAdd = (CmAckAdd*)signal->getDataPtrSend();
  cmAckAdd->requestType = CmAdd::Prepare;
  cmAckAdd->senderNodeId = getOwnNodeId();
  cmAckAdd->startingNodeId = getOwnNodeId();
  sendSignal(cpdistref, GSN_CM_ACKADD, signal, CmAckAdd::SignalLength, JBA);
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
  NodeRecPtr addNodePtr;
  NodeRecPtr myNodePtr;
  jamEntry();

  CmNodeInfoReq * const req = (CmNodeInfoReq*)signal->getDataPtr();
  addNodePtr.i = req->nodeId;
  ptrCheckGuard(addNodePtr, MAX_NDB_NODES, nodeRec);
  addNodePtr.p->ndynamicId = req->dynamicId;
  setNodeInfo(addNodePtr.i).m_version = req->version;
  
  const BlockReference Tblockref = signal->getSendersBlockRef();

  myNodePtr.i = getOwnNodeId();
  ptrCheckGuard(myNodePtr, MAX_NDB_NODES, nodeRec);
  if (myNodePtr.p->phase == ZRUNNING) {
    if (addNodePtr.p->phase == ZWAITING) {
      jam();
      /* President have prepared us */
      /****************************<*/
      /*< CM_NODEINFOCONF          <*/
      /****************************<*/
      CmNodeInfoConf * const conf = (CmNodeInfoConf*)signal->getDataPtrSend();
      conf->nodeId = getOwnNodeId();
      conf->dynamicId = myNodePtr.p->ndynamicId;
      conf->version = getNodeInfo(getOwnNodeId()).m_version;
      sendSignal(Tblockref, GSN_CM_NODEINFOCONF, signal, 
		 CmNodeInfoConf::SignalLength, JBB);
      /****************************************/
      /* Send an ack. back to the president   */
      /* CM_ACKADD                            */
      /****************************************/
      CmAckAdd * const cmAckAdd = (CmAckAdd*)signal->getDataPtrSend();
      cmAckAdd->requestType = CmAdd::Prepare;
      cmAckAdd->senderNodeId = getOwnNodeId();
      cmAckAdd->startingNodeId = addNodePtr.i;
      sendSignal(cpdistref, GSN_CM_ACKADD, signal, CmAckAdd::SignalLength, JBA);
    } else {
      jam();
      addNodePtr.p->phase = ZWAIT_PRESIDENT;
    }//if
  } else {
    jam();
    /****************************<*/
    /*< CM_NODEINFOREF           <*/
    /****************************<*/
    signal->theData[0] = myNodePtr.p->blockRef;
    signal->theData[1] = myNodePtr.i;
    signal->theData[2] = ZNOT_RUNNING;
    sendSignal(Tblockref, GSN_CM_NODEINFOREF, signal, 3, JBB);
  }//if
  return;
}//Qmgr::execCM_NODEINFOREQ()

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
  NodeRecPtr nodePtr;
  NodeRecPtr myNodePtr;
  jamEntry();

  CmAdd * const cmAdd = (CmAdd*)signal->getDataPtr();
  const CmAdd::RequestType type = (CmAdd::RequestType)cmAdd->requestType;
  addNodePtr.i = cmAdd->startingNodeId;
  //const Uint32 startingVersion = cmAdd->startingVersion;
  ptrCheckGuard(addNodePtr, MAX_NDB_NODES, nodeRec);

  if(addNodePtr.p->phase == ZFAIL_CLOSING){
    jam();
#ifdef VM_TRACE
    ndbout_c("Enabling communication to CM_ADD node state=%d", 
	     addNodePtr.p->phase);
#endif
    addNodePtr.p->failState = NORMAL;
    signal->theData[0] = 0;
    signal->theData[1] = addNodePtr.i;
    sendSignal(CMVMI_REF, GSN_OPEN_COMREQ, signal, 2, JBA);
  }
  
  switch (type) {
  case CmAdd::Prepare:
    jam();
    if (addNodePtr.i != getOwnNodeId()) {
      jam();
      if (addNodePtr.p->phase == ZWAIT_PRESIDENT) {
        jam();
	/****************************<*/
	/*< CM_NODEINFOCONF          <*/
	/****************************<*/
        myNodePtr.i = getOwnNodeId();
        ptrCheckGuard(myNodePtr, MAX_NDB_NODES, nodeRec);

	CmNodeInfoConf * const conf = (CmNodeInfoConf*)signal->getDataPtrSend();
	conf->nodeId = getOwnNodeId();
	conf->dynamicId = myNodePtr.p->ndynamicId;
	conf->version = getNodeInfo(getOwnNodeId()).m_version;
	sendSignal(addNodePtr.p->blockRef, GSN_CM_NODEINFOCONF, signal, 
		   CmNodeInfoConf::SignalLength, JBB);
	/****************************<*/
	/* Send an ack. back to the president   */
	/*< CM_ACKADD                <*/
	/****************************<*/
	CmAckAdd * const cmAckAdd = (CmAckAdd*)signal->getDataPtrSend();
	cmAckAdd->requestType = CmAdd::Prepare;
	cmAckAdd->senderNodeId = getOwnNodeId();
	cmAckAdd->startingNodeId = addNodePtr.i;
        sendSignal(cpdistref, GSN_CM_ACKADD, signal, CmAckAdd::SignalLength, JBA);
      }//if
     // -----------------------------------------
     /* Wait for the new node's CM_NODEINFOREQ.*/
     // -----------------------------------------
      addNodePtr.p->phase = ZWAITING;
    }//if
    break;
  case CmAdd::AddCommit:{
    jam();
    addNodePtr.p->phase = ZRUNNING;
    addNodePtr.p->alarmCount = 0;
    findNeighbours(signal);
    /**-----------------------------------------------------------------------
     * SEND A HEARTBEAT IMMEDIATELY TO DECREASE THE RISK THAT WE MISS EARLY
     * HEARTBEATS. 
     *-----------------------------------------------------------------------*/
    sendHeartbeat(signal);
    /*-----------------------------------------------------------------------*/
    /*  ENABLE COMMUNICATION WITH ALL BLOCKS WITH THE NEWLY ADDED NODE.      */
    /*-----------------------------------------------------------------------*/
    signal->theData[0] = addNodePtr.i;
    sendSignal(CMVMI_REF, GSN_ENABLE_COMORD, signal, 1, JBA);
    /****************************<*/
    /*< CM_ACKADD                <*/
    /****************************<*/
    CmAckAdd * const cmAckAdd = (CmAckAdd*)signal->getDataPtrSend();
    cmAckAdd->requestType = CmAdd::AddCommit;
    cmAckAdd->senderNodeId = getOwnNodeId();
    cmAckAdd->startingNodeId = addNodePtr.i;
    sendSignal(cpdistref, GSN_CM_ACKADD, signal, CmAckAdd::SignalLength, JBA);
    break;
  }
  case CmAdd::CommitNew:{
    jam();
    /*-----------------------------------------------------------------------*/
    /* WE HAVE BEEN INCLUDED IN THE CLUSTER WE CAN START BEING PART OF THE 
     * HEARTBEAT PROTOCOL AND WE WILL ALSO ENABLE COMMUNICATION WITH ALL 
     * NODES IN THE CLUSTER.
     *-----------------------------------------------------------------------*/
    addNodePtr.p->phase = ZRUNNING;
    addNodePtr.p->alarmCount = 0;
    findNeighbours(signal);
    /**-----------------------------------------------------------------------
     * SEND A HEARTBEAT IMMEDIATELY TO DECREASE THE RISK THAT WE MISS EARLY
     * HEARTBEATS. 
     *-----------------------------------------------------------------------*/
    sendHeartbeat(signal);
    cwaitContinuebPhase2 = ZFALSE;
    /**-----------------------------------------------------------------------
     * ENABLE COMMUNICATION WITH ALL BLOCKS IN THE CURRENT CLUSTER AND SET 
     * THE NODES IN THE CLUSTER TO BE RUNNING. 
     *-----------------------------------------------------------------------*/
    for (nodePtr.i = 1; nodePtr.i < MAX_NDB_NODES; nodePtr.i++) {
      jam();
      ptrAss(nodePtr, nodeRec);
      if ((nodePtr.p->phase == ZRUNNING) &&
          (nodePtr.i != getOwnNodeId())) {
	/*-------------------------------------------------------------------*/
	// Enable full communication to all other nodes. Not really necessary 
	// to open communication to ourself.
	/*-------------------------------------------------------------------*/
        jam();
        signal->theData[0] = nodePtr.i;
        sendSignal(CMVMI_REF, GSN_ENABLE_COMORD, signal, 1, JBA);
      }//if
    }//for

    /****************************<*/
    /*< CM_ACKADD                <*/
    /****************************<*/
    CmAckAdd * const cmAckAdd = (CmAckAdd*)signal->getDataPtrSend();
    cmAckAdd->requestType = CmAdd::CommitNew;
    cmAckAdd->senderNodeId = getOwnNodeId();
    cmAckAdd->startingNodeId = addNodePtr.i;
    sendSignal(cpdistref, GSN_CM_ACKADD, signal, CmAckAdd::SignalLength, JBA);

#if 1
    /**********************************************<*/
    /* Allow the CLUSTER CONTROL to send STTORRY    */
    /* so we can receive CM_REG from applications.  */
    /**********************************************<*/
    signal->theData[0] = 0;
    sendSignal(CMVMI_REF, GSN_CM_RUN, signal, 1, JBB);
#endif
    
    /**
     * Start timer handling 
     */
    signal->theData[0] = ZTIMER_HANDLING;
    sendSignal(QMGR_REF, GSN_CONTINUEB, signal, 10, JBB);
  }
    break;
  default:
    jam();
    /*empty*/;
    break;
  }//switch
  return;
}//Qmgr::execCM_ADD()

/*  4.10.7 CM_ACKADD        - PRESIDENT IS RECEIVER -       */
/*---------------------------------------------------------------------------*/
/* Entry point for an ack add signal. 
 * The TTYPE defines if it is a prepare or a commit.                         */
/*---------------------------------------------------------------------------*/
void Qmgr::execCM_ACKADD(Signal* signal) 
{
  NodeRecPtr addNodePtr;
  NodeRecPtr nodePtr;
  NodeRecPtr senderNodePtr;
  jamEntry();

  CmAckAdd * const cmAckAdd = (CmAckAdd*)signal->getDataPtr();
  const CmAdd::RequestType type = (CmAdd::RequestType)cmAckAdd->requestType;
  addNodePtr.i = cmAckAdd->startingNodeId;
  senderNodePtr.i = cmAckAdd->senderNodeId;
  if (cpresident != getOwnNodeId()) {
    jam();
    /*-----------------------------------------------------------------------*/
    /* IF WE ARE NOT PRESIDENT THEN WE SHOULD NOT RECEIVE THIS MESSAGE.      */
    /*------------------------------------------------------------_----------*/
    return;
  }//if
  if (cpresidentBusy != ZTRUE) {
    jam();
    /**----------------------------------------------------------------------
     * WE ARE PRESIDENT BUT WE ARE NOT BUSY ADDING ANY NODE. THUS WE MUST 
     * HAVE STOPPED THIS ADDING OF THIS NODE. 
     *----------------------------------------------------------------------*/
    return;
  }//if
  if (addNodePtr.i != cstartNode) {
    jam();
    /*----------------------------------------------------------------------*/
    /* THIS IS NOT THE STARTING NODE. WE ARE ACTIVE NOW WITH ANOTHER START. */
    /*----------------------------------------------------------------------*/
    return;
  }//if
  switch (type) {
  case CmAdd::Prepare:{
    jam();
    ptrCheckGuard(senderNodePtr, MAX_NDB_NODES, nodeRec);
    senderNodePtr.p->sendCmAddPrepStatus = Q_NOT_ACTIVE;
    for (nodePtr.i = 1; nodePtr.i < MAX_NDB_NODES; nodePtr.i++) {
      jam();
      ptrAss(nodePtr, nodeRec);
      /* Check if all prepare are acknowledged*/
      if (nodePtr.p->sendCmAddPrepStatus == Q_ACTIVE) {
        jam();
        return;	/* Wait for more acknowledge's          */
      }//if
    }//for
    /*----------------------------------------------------------------------*/
    /* ALL RUNNING NODES HAVE PREPARED THE INCLUSION OF THIS NEW NODE.      */
    /*----------------------------------------------------------------------*/
    CmAdd * const cmAdd = (CmAdd*)signal->getDataPtrSend();
    cmAdd->requestType = CmAdd::AddCommit;
    cmAdd->startingNodeId = addNodePtr.i; 
    cmAdd->startingVersion = getNodeInfo(addNodePtr.i).m_version;
    for (nodePtr.i = 1; nodePtr.i < MAX_NDB_NODES; nodePtr.i++) {
      jam();
      ptrAss(nodePtr, nodeRec);
      if (nodePtr.p->phase == ZRUNNING) {
        jam();
        sendSignal(nodePtr.p->blockRef, GSN_CM_ADD, signal, 
		   CmAdd::SignalLength, JBA);
        nodePtr.p->sendCmAddCommitStatus = Q_ACTIVE;
      }//if
    }//for
    return;
    break;
  }
  case CmAdd::AddCommit:{
    jam();
    ptrCheckGuard(senderNodePtr, MAX_NDB_NODES, nodeRec);
    senderNodePtr.p->sendCmAddCommitStatus = Q_NOT_ACTIVE;
    for (nodePtr.i = 1; nodePtr.i < MAX_NDB_NODES; nodePtr.i++) {
      jam();
      ptrAss(nodePtr, nodeRec);
      /* Check to see if we need to wait for  */
      if (nodePtr.p->sendCmAddCommitStatus == Q_ACTIVE) {
        jam();
	/* any more ack. commit add.            */
        return;	/* Exit and continue waiting.           */
      }//if
    }//for
    /****************************************/
    /* Send commit to the new node so he    */
    /* will change PHASE into ZRUNNING      */
    /****************************************/
    CmAdd * const cmAdd = (CmAdd*)signal->getDataPtrSend();
    cmAdd->requestType = CmAdd::CommitNew;
    cmAdd->startingNodeId = addNodePtr.i; 
    cmAdd->startingVersion = getNodeInfo(addNodePtr.i).m_version;
    
    ptrCheckGuard(addNodePtr, MAX_NDB_NODES, nodeRec);
    sendSignal(addNodePtr.p->blockRef, GSN_CM_ADD, signal, 
	       CmAdd::SignalLength, JBA);
    break;
  }
  case CmAdd::CommitNew:
    jam();
    /*----------------------------------------------------------------------*/
    /* Increment the amount of nodes in the cluster in waiting mode.        */
    /* President now ready for more CM_REGREQ                               */
    /*----------------------------------------------------------------------*/
    cclustersize = cclustersize + 1; 
    /**
     * Tell arbitration about new node.
     */
    handleArbitNdbAdd(signal, addNodePtr.i);
    cpresidentBusy = ZFALSE;	
    break;
  default:
    jam();
    /*empty*/;
    break;
  }//switch
  return;
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
      fnNodePtr.p->alarmCount = 0;
    }//if
  }//if

  signal->theData[0] = EventReport::FIND_NEIGHBOURS;
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
  RegAppPtr localRegAppptr;

  for (localRegAppptr.i = 0;
       localRegAppptr.i < NO_REG_APP; localRegAppptr.i++) {
    ptrAss(localRegAppptr, regApp);
    localRegAppptr.p->version = 0;
    localRegAppptr.p->blockref = 0;
    memset(localRegAppptr.p->name, 0, sizeof(localRegAppptr.p->name));
    localRegAppptr.p->activity = ZREMOVE;
    localRegAppptr.p->noofapps = 0;
    localRegAppptr.p->noofpending = 0;
    localRegAppptr.p->m_runNodes.clear();
  }//for

  NodeRecPtr nodePtr;
  for (nodePtr.i = 1; nodePtr.i < MAX_NODES; nodePtr.i++) {
    ptrAss(nodePtr, nodeRec);
    nodePtr.p->ndynamicId = 0;	
    /* Subr NEXT_DYNAMIC_ID will use this to find   */
    /* a unique higher value than any of these      */

    /* Not in config file                           */
    nodePtr.p->phase = ZBLOCKED;
    nodePtr.p->alarmCount = 0;
    nodePtr.p->sendPrepFailReqStatus = Q_NOT_ACTIVE;
    nodePtr.p->sendCommitFailReqStatus = Q_NOT_ACTIVE;
    nodePtr.p->sendCmAddPrepStatus = Q_NOT_ACTIVE;
    nodePtr.p->sendCmAddCommitStatus = Q_NOT_ACTIVE;
    nodePtr.p->sendPresToStatus = Q_NOT_ACTIVE;
    nodePtr.p->m_connected = false;
    nodePtr.p->failState = NORMAL;
    nodePtr.p->rcv[0] = 0;
    nodePtr.p->rcv[1] = 0;
  }//for
  ccm_infoconfCounter = 0;
  cfailureNr = 1;
  ccommitFailureNr = 1;
  cprepareFailureNr = 1;
  cnoFailedNodes = 0;
  cnoPrepFailedNodes = 0;
  cwaitContinuebPhase1 = ZFALSE;
  cwaitContinuebPhase2 = ZFALSE;
  cstartNo = 0;
  cpresidentBusy = ZFALSE;
  cacceptRegreq = ZTRUE;
  creadyDistCom = ZFALSE;
  cpresident = ZNIL;
  cpresidentCandidate = ZNIL;
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
}//Qmgr::initData()

/*
4.10.7 PREPARE_ADD      */
/**--------------------------------------------------------------------------
 * President sends CM_ADD to prepare all running nodes to add a new node. 
 * Even the president node will get a CM_ADD (prepare). 
 * The new node will make REQs to all running nodes after it has received the 
 * CM_REGCONF. The president will just coordinate the adding of new nodes. 
 * The CM_ADD (prepare) is sent to the cluster before the CM_REGCONF signal 
 * to the new node. 
 *
 * At the same time we will store all running nodes in CNODEMASK, 
 * which will be sent to the new node 
 * Scan the NODE_REC for all running nodes and create a nodemask where 
 * each bit represents a node. 
 * --------------------------------------------------------------------------*/
void Qmgr::prepareAdd(Signal* signal, Uint16 anAddedNode) 
{
  NodeRecPtr nodePtr;
  NdbNodeBitmask::clear(cnodemask);
  
  CmAdd * const cmAdd = (CmAdd*)signal->getDataPtrSend();
  cmAdd->requestType = CmAdd::Prepare;
  cmAdd->startingNodeId = anAddedNode; 
  cmAdd->startingVersion = getNodeInfo(anAddedNode).m_version;
  for (nodePtr.i = 1; nodePtr.i < MAX_NDB_NODES; nodePtr.i++) {
    jam();
    ptrAss(nodePtr, nodeRec);
    if (nodePtr.p->phase == ZRUNNING) {
      jam();
      /* We found a node to prepare. */
      NdbNodeBitmask::set(cnodemask, nodePtr.i);
      sendSignal(nodePtr.p->blockRef, GSN_CM_ADD, signal, CmAdd::SignalLength, JBA);
      nodePtr.p->sendCmAddPrepStatus = Q_ACTIVE;
    }//if
  }//for

  NodeRecPtr addNodePtr;
  addNodePtr.i = anAddedNode;
  ptrCheckGuard(addNodePtr, MAX_NDB_NODES, nodeRec);
  /*****************************<
   * We send to the node to be added a CM_ADD as well.
   * We want him to send an ack when he has 
   * received all CM_NODEINFOCONF.
   */
  sendSignal(addNodePtr.p->blockRef, GSN_CM_ADD, signal, CmAdd::SignalLength, JBA);
  addNodePtr.p->sendCmAddPrepStatus = Q_ACTIVE;
}//Qmgr::prepareAdd()

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

  if (cactivateApiCheck != 0) {
    jam();
    if (hb_api_timer.check(TcurrentTime)) {
      jam();
      hb_api_timer.reset();
      apiHbHandlingLab(signal);
    }//if
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
  signal->theData[0] = EventReport::SentHeartbeat;
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
  
  nodePtr.p->alarmCount ++;
  ndbrequire(nodePtr.p->phase == ZRUNNING);
  ndbrequire(getNodeInfo(nodePtr.i).m_type == NodeInfo::DB);

  if(nodePtr.p->alarmCount > 2){
    signal->theData[0] = EventReport::MissedHeartbeat;
    signal->theData[1] = nodePtr.i;
    signal->theData[2] = nodePtr.p->alarmCount - 1;
    sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 3, JBB);
  }

  if (nodePtr.p->alarmCount > 4) {
    jam();
    /**----------------------------------------------------------------------
     * OUR LEFT NEIGHBOUR HAVE KEPT QUIET FOR THREE CONSECUTIVE HEARTBEAT 
     * PERIODS. THUS WE DECLARE HIM DOWN.
     *----------------------------------------------------------------------*/
    signal->theData[0] = EventReport::DeadDueToHeartbeat;
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
    ptrAss(TnodePtr, nodeRec);
    
    const NodeInfo::NodeType type = getNodeInfo(TnodePtr.i).getType();
    if(type == NodeInfo::DB)
      continue;
    
    if(type == NodeInfo::INVALID)
      continue;

    if (TnodePtr.p->m_connected && TnodePtr.p->phase != ZAPI_INACTIVE){
      jam();
      TnodePtr.p->alarmCount ++;

      if(TnodePtr.p->alarmCount > 2){
	signal->theData[0] = EventReport::MissedHeartbeat;
	signal->theData[1] = TnodePtr.i;
	signal->theData[2] = TnodePtr.p->alarmCount - 1;
	sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 3, JBB);
      }
      
      if (TnodePtr.p->alarmCount > 4) {
        jam();
	/*------------------------------------------------------------------*/
	/* THE API NODE HAS NOT SENT ANY HEARTBEAT FOR THREE SECONDS. 
	 * WE WILL DISCONNECT FROM IT NOW.
	 *------------------------------------------------------------------*/
	/*------------------------------------------------------------------*/
	/* We call node_failed to release all connections for this api node */
	/*------------------------------------------------------------------*/
	signal->theData[0] = EventReport::DeadDueToHeartbeat;
	signal->theData[1] = TnodePtr.i;
	sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 2, JBB);

        node_failed(signal, TnodePtr.i);
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
      nodePtr.p->alarmCount = nodePtr.p->alarmCount + 1;
      if (nodePtr.p->m_connected) {
        jam();
	/*-------------------------------------------------------------------*/
	// We need to ensure that the connection is not restored until it has 
	// been disconnected for at least three seconds.
	/*-------------------------------------------------------------------*/
        nodePtr.p->alarmCount = 0;
      }//if
      if ((nodePtr.p->alarmCount > 3) && (nodePtr.p->failState == NORMAL)) {
	/**------------------------------------------------------------------
	 * WE HAVE DISCONNECTED THREE SECONDS AGO. WE ARE NOW READY TO 
	 * CONNECT AGAIN AND ACCEPT NEW REGISTRATIONS FROM THIS NODE. 
	 * WE WILL NOT ALLOW CONNECTIONS OF API NODES UNTIL API FAIL HANDLING 
	 * IS COMPLETE.
	 *-------------------------------------------------------------------*/
        nodePtr.p->failState = NORMAL;
        if (getNodeInfo(nodePtr.i).m_type != NodeInfo::DB){
          jam();
          nodePtr.p->phase = ZBLOCKED;
        } else {
          jam();
          nodePtr.p->phase = ZINIT;
        }//if
        nodePtr.p->alarmCount = 0;
        signal->theData[0] = 0;
        signal->theData[1] = nodePtr.i;
        sendSignal(CMVMI_REF, GSN_OPEN_COMREQ, signal, 2, JBA);
      } else {
	if(((nodePtr.p->alarmCount + 1) % 60) == 0){
	  char buf[100];
	  snprintf(buf, sizeof(buf), 
		   "Failure handling of node %d has not completed in %d min."
		   " - state = %d",
		   nodePtr.i, 
		   (nodePtr.p->alarmCount + 1)/60,
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
  sendSignal(DBTC_REF, GSN_API_FAILREQ, signal, 2, JBA);
  sendSignal(DBDICT_REF, GSN_API_FAILREQ, signal, 2, JBA);
  sendSignal(SUMA_REF, GSN_API_FAILREQ, signal, 2, JBA);
  /**
   * GREP also need the information that an API node 
   * (actually a REP node) has failed.
   *
   * GREP does however NOT send a CONF on this signal, i.e.
   * the API_FAILREQ signal to GREP is like a REP signal 
   * (i.e. without any confirmation).
   */
  sendSignal(GREP_REF, GSN_API_FAILREQ, signal, 2, JBA);
  
  /**-------------------------------------------------------------------------
   * THE OTHER NODE WAS AN API NODE. THE COMMUNICATION LINK IS ALREADY 
   * BROKEN AND THUS NO ACTION IS NEEDED TO BREAK THE CONNECTION. 
   * WE ONLY NEED TO SET PARAMETERS TO ENABLE A NEW CONNECTION IN A FEW 
   * SECONDS. 
   *-------------------------------------------------------------------------*/
  failedNodePtr.p->alarmCount = 0;

  CloseComReqConf * const closeCom = (CloseComReqConf *)&signal->theData[0];

  closeCom->xxxBlockRef = reference();
  closeCom->failNo      = 0;
  closeCom->noOfNodes   = 1;
  NodeBitmask::clear(closeCom->theNodes);
  NodeBitmask::set(closeCom->theNodes, failedNodePtr.i);
  sendSignal(CMVMI_REF, GSN_CLOSE_COMREQ, signal, 
	     CloseComReqConf::SignalLength, JBA);
}//Qmgr::sendApiFailReq()

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
      systemErrorLab(signal);
    } else {
      jam();
      failedNodePtr.p->rcv[0] = 0;
      failedNodePtr.p->rcv[1] = 0;
    }//if
  } else {
    jam();
#ifdef VM_TRACE
    ndbout << "failedNodePtr.p->failState = " << failedNodePtr.p->failState
	   << endl;
#endif   
    systemErrorLab(signal);
  }//if
  return;
}//Qmgr::execAPI_FAILCONF()

void Qmgr::execNDB_FAILCONF(Signal* signal) 
{
  NodeRecPtr failedNodePtr;
  NodeRecPtr nodePtr;

  jamEntry();
  failedNodePtr.i = signal->theData[0];  
  ptrCheckGuard(failedNodePtr, MAX_NODES, nodeRec);
  if (failedNodePtr.p->failState == WAITING_FOR_NDB_FAILCONF){
    failedNodePtr.p->failState = NORMAL;
  } else {
    jam();
    systemErrorLab(signal);
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
      if ((nodePtr.p->phase == ZAPI_ACTIVE) && nodePtr.p->m_connected) {
        jam();
        sendSignal(nodePtr.p->blockRef, GSN_NF_COMPLETEREP, signal, 
                   NFCompleteRep::SignalLength, JBA);
      }//if
    }//for
  }//if
  return;
}//Qmgr::execNDB_FAILCONF()

/*******************************/
/* DISCONNECT_REP             */
/*******************************/
void Qmgr::execDISCONNECT_REP(Signal* signal) 
{
  const DisconnectRep * const rep = (DisconnectRep *)&signal->theData[0];
  NodeRecPtr failedNodePtr;

  jamEntry();
  failedNodePtr.i = rep->nodeId;
  ptrCheckGuard(failedNodePtr, MAX_NODES, nodeRec);
  failedNodePtr.p->m_connected = false;
  node_failed(signal, failedNodePtr.i);
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

  if (getNodeInfo(failedNodePtr.i).getType() == NodeInfo::DB){
    jam();
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
    default:
      jam();
      /*---------------------------------------------------------------------*/
      // The other node is still not in the cluster but disconnected. 
      // We must restart communication in three seconds.
      /*---------------------------------------------------------------------*/
      failedNodePtr.p->failState = NORMAL;
      failedNodePtr.p->phase = ZFAIL_CLOSING;
      failedNodePtr.p->alarmCount = 0;

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

  /**
   * API code
   */
  jam();
  if (failedNodePtr.p->phase != ZFAIL_CLOSING){
    jam();
    //--------------------------------------------------------------------------
    // The API was active and has now failed. We need to initiate API failure
    // handling. If the API had already failed then we can ignore this
    // discovery.
    //--------------------------------------------------------------------------
    failedNodePtr.p->phase = ZFAIL_CLOSING;
    
    sendApiFailReq(signal, aFailedNode);
    arbitRec.code = ArbitCode::ApiFail;
    handleArbitApiFail(signal, aFailedNode);
  }//if
  return;
}//Qmgr::node_failed()

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
  switch(getNodeInfo(apiNodePtr.i).getType()){
  case NodeInfo::DB:
  case NodeInfo::INVALID:
    sendApiRegRef(signal, ref, ApiRegRef::WrongType);
    return;
  case NodeInfo::API:
    compatability_check = ndbCompatible_ndb_api(NDB_VERSION, version);
    break;
  case NodeInfo::MGM:
    compatability_check = ndbCompatible_ndb_mgmt(NDB_VERSION, version);
    break;
  case NodeInfo::REP:
    compatability_check = ndbCompatible_ndb_api(NDB_VERSION, version);
    break;
  }

  if (!compatability_check) {
    jam();
    apiNodePtr.p->phase = ZAPI_INACTIVE;
    sendApiRegRef(signal, ref, ApiRegRef::UnsupportedVersion);
    return;
  }

  setNodeInfo(apiNodePtr.i).m_version = version;
   
  apiNodePtr.p->alarmCount = 0;

  ApiRegConf * const apiRegConf = (ApiRegConf *)&signal->theData[0];
  apiRegConf->qmgrRef = reference();
  apiRegConf->apiHeartbeatFrequency = (chbApiDelay / 10);
  apiRegConf->version = NDB_VERSION;


  //  if(apiNodePtr.i == getNodeState.single. && NodeState::SL_MAINTENANCE)
  // apiRegConf->nodeState = NodeState::SL_STARTED;
  //else
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
      apiRegConf->nodeState.dynamicId = -dynamicId;
    }
  }
  sendSignal(ref, GSN_API_REGCONF, signal, ApiRegConf::SignalLength, JBB);
  
  if ((getNodeState().startLevel == NodeState::SL_STARTED ||
       getNodeState().getSingleUserMode())
      && apiNodePtr.p->phase == ZBLOCKED) {
    jam();
    /**----------------------------------------------------------------------
     * THE API NODE IS REGISTERING. WE WILL ACCEPT IT BY CHANGING STATE AND 
     * SENDING A CONFIRM. 
     *----------------------------------------------------------------------*/
    apiNodePtr.p->phase = ZAPI_ACTIVE;
    apiNodePtr.p->blockRef = ref;
    signal->theData[0] = apiNodePtr.i;
    sendSignal(CMVMI_REF, GSN_ENABLE_COMORD, signal, 1, JBA);
  }
  return;
}//Qmgr::execAPI_REGREQ()


void 
Qmgr::execAPI_VERSION_REQ(Signal * signal) {
  jamEntry();
  ApiVersionReq * const req = (ApiVersionReq *)signal->getDataPtr();
  
  Uint32 senderRef = req->senderRef;
  Uint32 nodeId = req->nodeId;

  ApiVersionConf * conf = (ApiVersionConf *)req;
  if(getNodeInfo(nodeId).m_connected)
    conf->version =  getNodeInfo(nodeId).m_version;
  else
    conf->version =  0;
  conf->nodeId = nodeId;

  sendSignal(senderRef, 
	     GSN_API_VERSION_CONF,
	     signal,
	     ApiVersionConf::SignalLength, JBB);


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
  if (failedNodePtr.i == getOwnNodeId()) {
    jam();
    systemErrorLab(signal);
    return;
  }//if

  myNodePtr.i = getOwnNodeId();
  ptrCheckGuard(myNodePtr, MAX_NDB_NODES, nodeRec);
  if (myNodePtr.p->phase != ZRUNNING) {
    jam();
    systemErrorLab(signal);
    return;
  }//if
  TnoFailedNodes = cnoFailedNodes;
  failReport(signal, failedNodePtr.i, (UintR)ZTRUE, aFailCause);
  if (cpresident == getOwnNodeId()) {
    jam();
    if (cpresidentBusy == ZTRUE) {
      jam();
/**-------------------------------------------------------------------
* ALL STARTING NODES ARE CRASHED WHEN AN ALIVE NODE FAILS DURING ITS 
* START-UP. AS PRESIDENT OF THE CLUSTER IT IS OUR DUTY TO INFORM OTHERS
* ABOUT THIS.
*---------------------------------------------------------------------*/
      failReport(signal, cstartNode, (UintR)ZTRUE, FailRep::ZOTHER_NODE_WHEN_WE_START);
    }//if
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

  PrepFailReqRef * const prepFail = (PrepFailReqRef *)&signal->theData[0];

  BlockReference Tblockref  = prepFail->xxxBlockRef;
  Uint16 TfailureNr = prepFail->failNo;
  cnoPrepFailedNodes = prepFail->noOfNodes;
  UintR arrayIndex = 0;
  for (Uint32 Tindex = 0; Tindex < MAX_NDB_NODES; Tindex++) {
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
    systemErrorLab(signal);
    return;
  }//if

  guard0 = cnoPrepFailedNodes - 1;
  arrGuard(guard0, MAX_NDB_NODES);
  for (Uint32 Tindex = 0; Tindex <= guard0; Tindex++) {
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
  UintR Ti;
  UintR Tj;
  RegAppPtr localRegAppptr;

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
    for (localRegAppptr.i = 0;
         localRegAppptr.i < NO_REG_APP; localRegAppptr.i++) {
      jam();
      ptrAss(localRegAppptr, regApp);
      if (localRegAppptr.p->activity != ZREMOVE) {
	/*------------------------------------------------------------------*/
	// We need to remove the failed nodes from the set of running nodes 
	// in the registered application.
	//------------------------------------------------------------------*/
        for (Ti = 0; Ti < cnoCommitFailedNodes; Ti++) {
          jam();
          arrGuard(ccommitFailedNodes[Ti], MAX_NDB_NODES);
          localRegAppptr.p->m_runNodes.clear(ccommitFailedNodes[Ti]);
        }//for
	/*------------------------------------------------------------------*/
	// Send a signal to the registered application to inform him of the 
	// node failure(s).
	/*------------------------------------------------------------------*/
	NodeFailRep * const nodeFail = (NodeFailRep *)&signal->theData[0];

	nodeFail->failNo    = ccommitFailureNr;
	nodeFail->noOfNodes = cnoCommitFailedNodes;
	NodeBitmask::clear(nodeFail->theNodes);
	for(unsigned i = 0; i < cnoCommitFailedNodes; i++) {
          jam();
	  NodeBitmask::set(nodeFail->theNodes, ccommitFailedNodes[i]);
        }//if	
        sendSignal(localRegAppptr.p->blockref, GSN_NODE_FAILREP, signal, 
		   NodeFailRep::SignalLength, JBB);
      }//if
    }//for
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
      nodePtr.p->alarmCount = 0;
    }//for
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
    if (cpresident != getOwnNodeId()) {
      jam();
      cnoFailedNodes = cnoCommitFailedNodes - cnoFailedNodes;
      if (cnoFailedNodes > 0) {
        jam();
        guard0 = cnoFailedNodes - 1;
        arrGuard(guard0 + cnoCommitFailedNodes, MAX_NDB_NODES);
        for (Tj = 0; Tj <= guard0; Tj++) {
          jam();
          cfailedNodes[Tj] = cfailedNodes[Tj + cnoCommitFailedNodes];
        }//for
      }//if
    }//if
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
  NodeRecPtr nodePtr;
  UintR TnoOfNodes = 0;
  BlockReference TBref = signal->theData[0];

  ReadNodesConf * const readNodes = (ReadNodesConf *)&signal->theData[0];
  NodeBitmask::clear(readNodes->allNodes);

  for (nodePtr.i = 1; nodePtr.i < MAX_NDB_NODES; nodePtr.i++) {
    jam();
    ptrAss(nodePtr, nodeRec);
    if (getNodeInfo(nodePtr.i).getType() == NodeInfo::DB){
      jam();
      TnoOfNodes++;
      NodeBitmask::set(readNodes->allNodes, nodePtr.i);
    }//if
  }//for
  readNodes->noOfNodes = TnoOfNodes;
  sendSignal(TBref, GSN_READ_NODESCONF, signal, 
	     ReadNodesConf::SignalLength, JBB);
}//Qmgr::execREAD_NODESREQ()
/*--------------------------------------------------------------------------
 * Signal from an application requesting to be monitored in the cluster. 
 * APPL_REGREQ can be entered at any time during the life of the QMGR. 
 * It can be entered any number of times.
 * If QMGR is ZRUNNING a CM_APPCHG will be sent to all active nodes. 
 *---------------------------------------------------------------------------*/
void Qmgr::execAPPL_REGREQ(Signal* signal) 
{
  NodeRecPtr nodePtr;
  NodeRecPtr myNodePtr;
  RegAppPtr lRegApptr;
  char Tappname[16];
  jamEntry();
  BlockReference Tappref = signal->theData[0];
  Tappname[0] = signal->theData[1] >> 8;
  Tappname[1] = signal->theData[2];
  Tappname[2] = signal->theData[2] >> 8;
  Tappname[3] = signal->theData[3];
  Tappname[4] = signal->theData[3] >> 8;
  Tappname[5] = signal->theData[4];
  Tappname[6] = signal->theData[4] >> 8;
  Tappname[7] = signal->theData[5];
  Tappname[8] = signal->theData[5] >> 8;
  Tappname[9] = signal->theData[6];
  Tappname[10] = signal->theData[6] >> 8;
  Tappname[11] = signal->theData[7];
  Tappname[12] = signal->theData[7] >> 8;
  Tappname[13] = signal->theData[8];
  Tappname[14] = signal->theData[8] >> 8;
  Tappname[signal->theData[1] & 0xFF] = 0;
  UintR Tversion = signal->theData[10];
  Uint16 Tnodeno = refToNode(Tappref);
  if (Tnodeno == 0) {
    jam();
    /* Fix for all not distributed applications.                */
    Tnodeno = getOwnNodeId();
  }//if
  if (getOwnNodeId() == Tnodeno) {
    jam();
    /* Local application            */
    UintR Tfound = RNIL;
    for (lRegApptr.i = NO_REG_APP-1; (Uint16)~lRegApptr.i; lRegApptr.i--) {
      jam();
      ptrAss(lRegApptr, regApp);
      if (lRegApptr.p->activity == ZREMOVE) {
        Tfound = lRegApptr.i;
        break;
      }//if
    }//for
    if (Tfound != RNIL) {
      jam();
      /* If there was a slot available we     */
      /* register the application             */
      lRegApptr.i = Tfound;	
      ptrCheckGuard(lRegApptr, NO_REG_APP, regApp);
      lRegApptr.p->blockref = Tappref;
      strcpy(lRegApptr.p->name, Tappname);
      lRegApptr.p->version = Tversion;
      lRegApptr.p->activity = ZADD;
      myNodePtr.i = getOwnNodeId();
      ptrCheckGuard(myNodePtr, MAX_NDB_NODES, nodeRec);
      /****************************<*/
      /*< APPL_REGCONF             <*/
      /****************************<*/
      signal->theData[0] = lRegApptr.i;
      signal->theData[1] = cnoOfNodes;
      signal->theData[2] = cpresident;
      signal->theData[3] = myNodePtr.p->ndynamicId;
      sendSignal(lRegApptr.p->blockref, GSN_APPL_REGCONF, signal, 4, JBB);
      if (myNodePtr.p->phase == ZRUNNING) {
        jam();
	/* Check to see if any further action   */
        for (nodePtr.i = 1;
             nodePtr.i < MAX_NDB_NODES; nodePtr.i++) {
          jam();
          ptrAss(nodePtr, nodeRec);
	  /* is needed at this time               */
          if (nodePtr.p->phase == ZRUNNING) {
            jam();
            sendappchg(signal, lRegApptr.i, nodePtr.i);
          }//if
        }//for
      }//if
    } else {
      jam();
      /****************************<*/
      /*< APPL_REGREF              <*/
      /****************************<*/
      signal->theData[0] = ZERRTOOMANY;
      sendSignal(Tappref, GSN_APPL_REGREF, signal, 1, JBB);
    }//if
  } else {
    jam();
    /* TOO MANY REGISTERED APPLICATIONS     */
    systemErrorLab(signal);
  }//if
  return;
}//Qmgr::execAPPL_REGREQ()

/*
4.4.11 APPL_STARTREG */
/**--------------------------------------------------------------------------
 * Signal from an application indicating that it is ready to start running 
 * distributed. If the application is running alone or if all other 
 * applications of the same kind already have registered as STARTING then 
 * APPL_STARTCONF will be sent to the application as soon as phase four of 
 * STTOR is reached.
 *--------------------------------------------------------------------------*/
/*******************************/
/* APPL_STARTREG              */
/*******************************/
void Qmgr::execAPPL_STARTREG(Signal* signal) 
{
  RegAppPtr lRegApptr;
  NodeRecPtr myNodePtr;
  UintR TnodeId;
  jamEntry();
  lRegApptr.i = signal->theData[0];
  ptrCheckGuard(lRegApptr, NO_REG_APP, regApp);
  UintR Tcounter = signal->theData[1];

  lRegApptr.p->activity = ZSTART;
  /* Application is ready to start.       */

  /* Calculate how many apps we wait for  */
  lRegApptr.p->noofapps = (Tcounter - 1) - lRegApptr.p->noofpending;	
  /* send info to all other running nodes in the  */
  myNodePtr.i = getOwnNodeId();	
  ptrCheckGuard(myNodePtr, MAX_NDB_NODES, nodeRec);
  /* cluster indicating the status change of the  */
  if (myNodePtr.p->phase == ZRUNNING) {
    /* application.                                 */
    for (TnodeId = 1; TnodeId < MAX_NDB_NODES; TnodeId++) {
      jam();
      if (lRegApptr.p->m_runNodes.get(TnodeId)){
        jam();
        sendappchg(signal, lRegApptr.i, TnodeId);
      }//if
    }//for
  }//if
  /****************************<*/
  /*< APPL_STARTCONF           <*/
  /****************************<*/
  if (lRegApptr.p->noofapps == 0) {
    jam();
    sendSignal(lRegApptr.p->blockref, GSN_APPL_STARTCONF, signal, 1, JBB);
  }//if
  return;
}//Qmgr::execAPPL_STARTREG()

/*
  4.4.11 APPL_RUN */
/*--------------------------------------------------------------------------*/
/* Signal from an application announcing that it is running.                */
/*--------------------------------------------------------------------------*/
/*******************************/
/* APPL_RUN                   */
/*******************************/
void Qmgr::execAPPL_RUN(Signal* signal) 
{
  RegAppPtr lRegApptr;
  NodeRecPtr myNodePtr;
  UintR TnodeId;
  jamEntry();
  lRegApptr.i = signal->theData[0];
  ptrCheckGuard(lRegApptr, NO_REG_APP, regApp);
  lRegApptr.p->activity = ZRUN;
  /* Flag the application as running.             */
  myNodePtr.i = getOwnNodeId();
  ptrCheckGuard(myNodePtr, MAX_NDB_NODES, nodeRec);
  if (myNodePtr.p->phase == ZRUNNING) {
    /* If we are running send the appl. status      */
    for (TnodeId = 1; TnodeId < MAX_NDB_NODES; TnodeId++) {
      jam();
      /* change to all other running nodes.           */
      if (lRegApptr.p->m_runNodes.get(TnodeId)){
        jam();
        sendappchg(signal, lRegApptr.i, TnodeId);
      }//if
    }//for
  }//if
  /****************************<*/
  /*< CM_RUN                   <*/
  /****************************<*/
  /*---------------------------------------------------*/
  /* Inform the CLUSTER CONTROL of NDB started         */
  /* so we can connect to API nodes.                   */
  /*---------------------------------------------------*/
  signal->theData[0] = 1;
  sendSignal(CMVMI_REF, GSN_CM_RUN, signal, 1, JBB);
  cactivateApiCheck = 1;
  /**
   * Start arbitration thread.  This could be done as soon as
   * we have all nodes (or a winning majority).
   */
  if (cpresident == getOwnNodeId())
    handleArbitStart(signal);
  return;
}//Qmgr::execAPPL_RUN()


void Qmgr::systemErrorBecauseOtherNodeFailed(Signal* signal, 
					     NodeId failedNodeId) {
  jam();

  // Broadcast that this node is failing to other nodes
  failReport(signal, getOwnNodeId(), (UintR)ZTRUE, FailRep::ZOWN_FAILURE);

  char buf[100];
  snprintf(buf, 100, 
	   "Node was shutdown during startup because node %d failed",
	   failedNodeId);

  progError(__LINE__, ERR_SR_OTHERNODEFAILED, buf);  
}


void Qmgr::systemErrorLab(Signal* signal, const char * message) 
{
  jam();
  // Broadcast that this node is failing to other nodes
  failReport(signal, getOwnNodeId(), (UintR)ZTRUE, FailRep::ZOWN_FAILURE);

  // If it's known why shutdown occured
  // an error message has been passed to this function
  progError(__LINE__, 0, message);  

  return;
}//Qmgr::systemErrorLab()

/*
4.4.11 CM_APPCHG */
/*---------------------------------------------------------------------------*/
/*Signal between two QMGRs used to announce any changes of state for an appl.*/
/*---------------------------------------------------------------------------*/
/*******************************/
/* CM_APPCHG                  */
/*******************************/
void Qmgr::execCM_APPCHG(Signal* signal) 
{
  RegAppPtr lRegApptr;
  char Tappname[16];
  jamEntry();
  UintR Ttype = signal->theData[0];
  Uint16 Tnodeno = signal->theData[1];
  Tappname[0] = signal->theData[2] >> 8;
  Tappname[1] = signal->theData[3];
  Tappname[2] = signal->theData[3] >> 8;
  Tappname[3] = signal->theData[4];
  Tappname[4] = signal->theData[4] >> 8;
  Tappname[5] = signal->theData[5];
  Tappname[6] = signal->theData[5] >> 8;
  Tappname[7] = signal->theData[6];
  Tappname[8] = signal->theData[6] >> 8;
  Tappname[9] = signal->theData[7];
  Tappname[10] = signal->theData[7] >> 8;
  Tappname[11] = signal->theData[8];
  Tappname[12] = signal->theData[8] >> 8;
  Tappname[13] = signal->theData[9];
  Tappname[14] = signal->theData[9] >> 8;
  Tappname[signal->theData[2] & 0xFF] = 0;
  UintR Tversion = signal->theData[11];
  switch (Ttype) {
  case ZADD:
    jam();
    /* A new application has started on the sending node */
    for (lRegApptr.i = NO_REG_APP-1; (Uint16)~lRegApptr.i; lRegApptr.i--) { 
      jam(); 
      /* We are hosting this application      */
      ptrAss(lRegApptr, regApp);
      if (strcmp(lRegApptr.p->name, Tappname) == 0) {
        cmappAdd(signal, lRegApptr.i, Tnodeno, Ttype, Tversion);
      }//if
    }//for
    break;
    
  case ZSTART:
    jam(); 
    /* A registered application is ready to start on the sending node */
    for (lRegApptr.i = NO_REG_APP-1; (Uint16)~lRegApptr.i; lRegApptr.i--) {
      jam();
      ptrAss(lRegApptr, regApp);
      if (strcmp(lRegApptr.p->name, Tappname) == 0) {
        cmappStart(signal, lRegApptr.i, Tnodeno, Ttype, Tversion);
      }//if
    }//for
    break;

  case ZRUN:
    /* A registered application on the sending node has started to run      */
    jam();                                                              
    for (lRegApptr.i = NO_REG_APP-1; (Uint16)~lRegApptr.i; lRegApptr.i--) {
      jam();
      ptrAss(lRegApptr, regApp);
      if (strcmp(lRegApptr.p->name, Tappname) == 0) {
        arrGuard(Tnodeno, MAX_NDB_NODES);
        lRegApptr.p->m_runNodes.set(Tnodeno);
        applchangerep(signal, lRegApptr.i, Tnodeno, Ttype, Tversion);
      }//if
    }//for
    cacceptRegreq = ZTRUE; /* We can now start accepting new CM_REGREQ */
                           /* since the new node is running            */
    break;

  case ZREMOVE:
    /* A registered application has been deleted on the sending node */
    jam();                                                              
    for (lRegApptr.i = NO_REG_APP-1; (Uint16)~lRegApptr.i; lRegApptr.i--) { 
      jam();
      ptrAss(lRegApptr, regApp);
      if (strcmp(lRegApptr.p->name, Tappname) == 0) {
        applchangerep(signal, lRegApptr.i, Tnodeno, Ttype, Tversion);
      }//if
    }//for
    break;

  default:
    jam();
    /*empty*/;
    break;
  }//switch
  return;
}//Qmgr::execCM_APPCHG()

/**--------------------------------------------------------------------------
 * INPUT   REG_APPPTR
 *         TNODENO
 *--------------------------------------------------------------------------*/
void Qmgr::applchangerep(Signal* signal,
                         UintR aRegApp,
                         Uint16 aNode,
                         UintR aType,
                         UintR aVersion) 
{
  RegAppPtr lRegApptr;
  NodeRecPtr localNodePtr;
  lRegApptr.i = aRegApp;
  ptrCheckGuard(lRegApptr, NO_REG_APP, regApp);
  if (lRegApptr.p->blockref != 0) {
    jam();
    localNodePtr.i = aNode;
    ptrCheckGuard(localNodePtr, MAX_NDB_NODES, nodeRec);
    /****************************************/
    /* Send a report of changes on another  */
    /* node to the local application        */
    /****************************************/
    signal->theData[0] = aType;
    signal->theData[1] = aVersion;
    signal->theData[2] = localNodePtr.i;
    signal->theData[4] = localNodePtr.p->ndynamicId;
    sendSignal(lRegApptr.p->blockref, GSN_APPL_CHANGEREP, signal, 5, JBB);
  }//if
}//Qmgr::applchangerep()

/*
  4.10.7 CMAPP_ADD */
/**--------------------------------------------------------------------------
 * We only map applications of the same version. We have the same application 
 * and version locally.
 * INPUT   REG_APPPTR
 *         TNODENO       Sending node
 *         TVERSION      Version of application
 * OUTPUT  REG_APPPTR, TNODENO   ( not changed)
 *---------------------------------------------------------------------------*/
void Qmgr::cmappAdd(Signal* signal,
                    UintR aRegApp,
                    Uint16 aNode,
                    UintR aType,
                    UintR aVersion) 
{
  RegAppPtr lRegApptr;
  lRegApptr.i = aRegApp;
  ptrCheckGuard(lRegApptr, NO_REG_APP, regApp);
  if (lRegApptr.p->version == aVersion) {
    jam();
    arrGuard(aNode, MAX_NDB_NODES);
    if (lRegApptr.p->m_runNodes.get(aNode) == false){
      jam();
      /* Check if we already have added it.    */
      /*-------------------------------------------------------*/
      /* Since we only add remote applications, if we also are */
      /* hosting them we need to send a reply indicating that  */
      /* we also are hosting the application.                  */
      /*-------------------------------------------------------*/
      sendappchg(signal, lRegApptr.i, aNode);
      lRegApptr.p->m_runNodes.set(aNode);
      /*---------------------------------------*/
      /* Add the remote node to the the local  */
      /* nodes memberlist.                     */
      /* Inform the local application of the   */
      /* new application running remotely.     */
      /*---------------------------------------*/
      applchangerep(signal, lRegApptr.i, aNode, aType, aVersion);
    }//if
  }//if
}//Qmgr::cmappAdd()

/*
4.10.7 CMAPP_START */
/**--------------------------------------------------------------------------
 * Inform the local application of the change in node state on the remote node
 * INPUT   REG_APPPTR
 * OUTPUT  -
 *---------------------------------------------------------------------------*/
void Qmgr::cmappStart(Signal* signal,
                      UintR aRegApp,
                      Uint16 aNode,
                      UintR aType,
                      UintR aVersion) 
{
  RegAppPtr lRegApptr;
  lRegApptr.i = aRegApp;
  ptrCheckGuard(lRegApptr, NO_REG_APP, regApp);
  if (lRegApptr.p->version == aVersion) {
    applchangerep(signal, lRegApptr.i, aNode, aType, aVersion);
    if (lRegApptr.p->activity == ZSTART) {
      jam();
      //----------------------------------------
      /* If the local application is already  */
      /* in START face then we do some checks.*/
      //----------------------------------------
      if (lRegApptr.p->noofapps > 0) {
        jam();
	//----------------------------------------
	/* Check if we need to decrement the no */
	/* of apps.                             */
	/* This indicates how many startsignals */
	/* from apps remaining before we can    */
	/* send a APPL_STARTCONF.               */
	//----------------------------------------
        lRegApptr.p->noofapps--;
      }//if
      if (lRegApptr.p->noofapps == 0) {
        jam();
	//----------------------------------------
	/* All applications have registered as  */
	/* ready to start.                      */
	//----------------------------------------
	/****************************<*/
	/*< APPL_STARTCONF           <*/
	/****************************<*/
        sendSignal(lRegApptr.p->blockref, GSN_APPL_STARTCONF, signal, 1, JBB);
      }//if
    } else {
      jam();
      /**--------------------------------------------------------------------
       * Add the ready node to the nodes pending counter. 
       * This counter is used to see how many remote nodes that are waiting 
       * for this node to enter the start face.
       * It is used when the appl. sends a APPL_STARTREG signal.
       *---------------------------------------------------------------------*/
      if (lRegApptr.p->activity == ZADD) {
        jam();
        lRegApptr.p->noofpending++;
      }//if
    }//if
  }//if
}//Qmgr::cmappStart()

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
  if (((cpresidentBusy == ZTRUE) ||
       (cacceptRegreq == ZFALSE)) &&
       (cstartNode == aFailedNode)) {
    jam();
/*----------------------------------------------------------------------*/
// A node crashed keeping the president busy and that ensures that there 
// is no acceptance of regreq's which is not acceptable after its crash.
/*----------------------------------------------------------------------*/
    cpresidentBusy = ZFALSE;
    cacceptRegreq = ZTRUE;
  }//if
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
    failedNodePtr.p->sendCmAddPrepStatus = Q_NOT_ACTIVE;
    failedNodePtr.p->sendCmAddCommitStatus = Q_NOT_ACTIVE;
    failedNodePtr.p->sendPrepFailReqStatus = Q_NOT_ACTIVE;
    failedNodePtr.p->sendCommitFailReqStatus = Q_NOT_ACTIVE;
    failedNodePtr.p->sendPresToStatus = Q_NOT_ACTIVE;
    failedNodePtr.p->alarmCount = 0;
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
    systemErrorLab(signal);
  }//if
  return TtdiNodeId;
}//Qmgr::translateDynamicIdToNodeId()


/*
4.10.7 GET_DYNAMIC_ID   */
/**--------------------------------------------------------------------------
 * FIND THE CLOSEST HIGHER DYNAMIC ID AMONG THE RUNNING NODES. ADD ONE TO 
 * THAT VALUE AND WE HAVE CREATED A NEW, UNIQUE AND HIGHER DYNAMIC VALUE THAN 
 * ANYONE ELSE IN THE CLUSTER.THIS WAY WE DON'T HAVE TO KEEP TRACK OF VARIABLE
 * THAT HOLDS THE LAST USED DYNAMIC ID, ESPECIALLY WE DON'T NEED TO INFORM 
 * ANY VICE PRESIDENTS ABOUT THAT DYNAMIC VARIABLE.
 * INPUT -
 * RET   CDYNAMIC_ID  USED AS A TEMPORARY VARIABLE TO PASS THE VALUE TO THE 
 *                    CALLER OF THIS SUBROUTINE
 *---------------------------------------------------------------------------*/
UintR Qmgr::getDynamicId(Signal* signal) 
{
  NodeRecPtr nodePtr;
  UintR TdynamicId = 0;
  for (nodePtr.i = 1; nodePtr.i < MAX_NDB_NODES; nodePtr.i++) {
    jam();
    ptrAss(nodePtr, nodeRec);
    if (nodePtr.p->phase == ZRUNNING) {
      if (nodePtr.p->ndynamicId > TdynamicId) {
        jam();
        TdynamicId = nodePtr.p->ndynamicId;
      }//if
    }//if
  }//for
  TdynamicId++;
  return TdynamicId;
}//Qmgr::getDynamicId()

/*
4.10.7 SENDAPPCHG */
/*---------------------------------------------------------------------------*/
/* We only send changes to external nodes.                                   */
/* INPUT: TNODENO                                                            */
/*        REG_APPPTR                                                         */
/*---------------------------------------------------------------------------*/
void Qmgr::sendappchg(Signal* signal, UintR aRegApp, Uint16 aNode) 
{
  NodeRecPtr localNodePtr;
  RegAppPtr lRegApptr;
  if (aNode != getOwnNodeId()) {
    jam();
    localNodePtr.i = aNode;
    ptrCheckGuard(localNodePtr, MAX_NDB_NODES, nodeRec);
    lRegApptr.i = aRegApp;
    ptrCheckGuard(lRegApptr, NO_REG_APP, regApp);
    /****************************************/
    /* Signal any application changes to    */
    /* the receiving node                   */
    /****************************************/
    signal->theData[0] = lRegApptr.p->activity;
    signal->theData[1] = getOwnNodeId();
    signal->theData[2] = strlen(lRegApptr.p->name)|(lRegApptr.p->name[0] << 8);
    signal->theData[3] = lRegApptr.p->name[1] | (lRegApptr.p->name[2] << 8);
    signal->theData[4] = lRegApptr.p->name[3] | (lRegApptr.p->name[4] << 8);
    signal->theData[5] = lRegApptr.p->name[5] | (lRegApptr.p->name[6] << 8);
    signal->theData[6] = lRegApptr.p->name[7] | (lRegApptr.p->name[8] << 8);
    signal->theData[7] = lRegApptr.p->name[9] | (lRegApptr.p->name[10] << 8);
    signal->theData[8] = lRegApptr.p->name[11] | (lRegApptr.p->name[12] << 8);
    signal->theData[9] = lRegApptr.p->name[13] | (lRegApptr.p->name[14] << 8);
    signal->theData[10] = 0;
    signal->theData[11] = lRegApptr.p->version;
    sendSignal(localNodePtr.p->blockRef, GSN_CM_APPCHG, signal, 12, JBA);
  }//if
}//Qmgr::sendappchg()

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
  reportArbitEvent(signal, EventReport::ArbitState);
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
  if (2 * ndbMask.count() < cnoOfNodes) {
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
      break;
    default:
      ndbrequire(false);
      break;
    }
  }
  switch (arbitRec.code) {
  case ArbitCode::LoseNodes:
    jam();
    goto crashme;
  case ArbitCode::WinGroups:  
    jam();
    if (arbitRec.state == ARBIT_RUN) {
      jam();
      break;
    }
    arbitRec.state = ARBIT_INIT;
    arbitRec.newstate = true;
    break;
  case ArbitCode::LoseGroups:
    jam();
    goto crashme;
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
  reportArbitEvent(signal, EventReport::ArbitResult);
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
  reportArbitEvent(signal, EventReport::ArbitState);
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
    reportArbitEvent(signal, EventReport::ArbitState);
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
    reportArbitEvent(signal, EventReport::ArbitState);
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
    reportArbitEvent(signal, EventReport::ArbitState);
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
    reportArbitEvent(signal, EventReport::ArbitResult);
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
    reportArbitEvent(signal, EventReport::ArbitState);
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
 * Handle CRASH state.  We must crash immediately.  But it
 * would be nice to wait until event reports have been sent.
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
#if 0
  if (! (arbitRec.getTimediff() > getArbitTimeout()))
    return;
#endif
  progError(__LINE__, ERR_ARBIT_SHUTDOWN, "Arbitrator decided to shutdown this node");
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
Qmgr::reportArbitEvent(Signal* signal, EventReport::EventType type)
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
    infoEvent("creadyDistCom = %d, cpresident = %d, cpresidentBusy = %d\n",
             creadyDistCom, cpresident, cpresidentBusy);
    infoEvent("cacceptRegreq = %d, ccm_infoconfCounter = %d\n",
              cacceptRegreq, ccm_infoconfCounter);
    infoEvent("cstartNo = %d, cstartNode = %d, cwaitC..phase1 = %d\n",
               cstartNo, cstartNode, cwaitContinuebPhase1);
    infoEvent("cwaitC..phase2 = %d, cpresidentAlive = %d, cpresidentCand = %d\n"
              ,cwaitContinuebPhase2, cpresidentAlive, cpresidentCandidate);
    infoEvent("ctoStatus = %d\n", ctoStatus);
    for(Uint32 i = 1; i<MAX_NDB_NODES; i++){
      if(getNodeInfo(i).getType() == NodeInfo::DB){
	NodeRecPtr nodePtr;
	nodePtr.i = i;
	ptrCheckGuard(nodePtr, MAX_NDB_NODES, nodeRec);
	char buf[100];
	switch(nodePtr.p->phase){
	case ZINIT:
	  sprintf(buf, "Node %d: ZINIT(%d)", i, nodePtr.p->phase);
	  break;
	case ZBLOCKED:
	  sprintf(buf, "Node %d: ZBLOCKED(%d)", i, nodePtr.p->phase);
	  break;
	case ZWAITING:
	  sprintf(buf, "Node %d: ZWAITING(%d)", i, nodePtr.p->phase);
	  break;
	case ZWAIT_PRESIDENT:
	  sprintf(buf, "Node %d: ZWAIT_PRESIDENT(%d)", i, nodePtr.p->phase);
	  break;
	case ZRUNNING:
	  sprintf(buf, "Node %d: ZRUNNING(%d)", i, nodePtr.p->phase);
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
	case ZPREPARE_FAIL:
	  sprintf(buf, "Node %d: ZPREPARE_FAIL(%d)", i, nodePtr.p->phase);
	  break;
	default:
	  sprintf(buf, "Node %d: <UNKNOWN>(%d)", i, nodePtr.p->phase);
	  break;
	}
	infoEvent(buf);
      }
    }
  default:
    ;
  }//switch
}//Qmgr::execDUMP_STATE_ORD()

void Qmgr::execSET_VAR_REQ(Signal* signal) 
{
  SetVarReq* const setVarReq = (SetVarReq*)&signal->theData[0];
  ConfigParamId var = setVarReq->variable();
  UintR val = setVarReq->value();

  switch (var) {
  case HeartbeatIntervalDbDb:
    setHbDelay(val/10);
    sendSignal(CMVMI_REF, GSN_SET_VAR_CONF, signal, 1, JBB);
    break;

  case HeartbeatIntervalDbApi:
    setHbApiDelay(val/10);
    sendSignal(CMVMI_REF, GSN_SET_VAR_CONF, signal, 1, JBB);
    break;

  case ArbitTimeout:
    setArbitTimeout(val);
    sendSignal(CMVMI_REF, GSN_SET_VAR_CONF, signal, 1, JBB);
    break;

  default:
    sendSignal(CMVMI_REF, GSN_SET_VAR_REF, signal, 1, JBB);
  }// switch
}//execSET_VAR_REQ()

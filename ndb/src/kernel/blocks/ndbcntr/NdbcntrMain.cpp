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

#define NDBCNTR_C
#include "Ndbcntr.hpp"

#include <ndb_limits.h>
#include <ndb_version.h>
#include <SimpleProperties.hpp>
#include <signaldata/DictTabInfo.hpp>
#include <signaldata/CreateTable.hpp>
#include <signaldata/ReadNodesConf.hpp>
#include <signaldata/CntrMasterReq.hpp>
#include <signaldata/CntrMasterConf.hpp>
#include <signaldata/NodeFailRep.hpp>
#include <signaldata/TcKeyReq.hpp>
#include <signaldata/TcKeyConf.hpp>
#include <signaldata/EventReport.hpp>
#include <signaldata/SetVarReq.hpp>
#include <signaldata/NodeStateSignalData.hpp>
#include <signaldata/StopPerm.hpp>
#include <signaldata/StopMe.hpp>
#include <signaldata/WaitGCP.hpp>
#include <signaldata/CheckNodeGroups.hpp>
#include <signaldata/StartOrd.hpp>
#include <signaldata/AbortAll.hpp>
#include <signaldata/SystemError.hpp>
#include <signaldata/NdbSttor.hpp>
#include <signaldata/DumpStateOrd.hpp>

#include <signaldata/FsRemoveReq.hpp>

#include <AttributeHeader.hpp>
#include <Configuration.hpp>
#include <DebuggerNames.hpp>

#include <NdbOut.hpp>
#include <NdbTick.h>

#define ZSYSTEM_RUN 256

/**
 * ALL_BLOCKS Used during start phases and while changing node state
 *
 * NDBFS_REF Has to be before NDBCNTR_REF (due to "ndb -i" stuff)
 */
struct BlockInfo {
  BlockReference Ref; // BlockReference
  Uint32 NextSP;            // Next start phase
};

static BlockInfo ALL_BLOCKS[] = { 
  { DBTC_REF,    1 },
  { DBDIH_REF,   1 },
  { DBLQH_REF,   1 },
  { DBACC_REF,   1 },
  { DBTUP_REF,   1 },
  { DBDICT_REF,  1 },
  { NDBFS_REF,   0 },
  { NDBCNTR_REF, 0 },
  { QMGR_REF,    1 },
  { CMVMI_REF,   1 },
  { TRIX_REF,    1 },
  { BACKUP_REF,  1 },
  { DBUTIL_REF,  1 },
  { SUMA_REF,    1 },
  { GREP_REF,    1 },
  { DBTUX_REF,   1 }
};

static const Uint32 ALL_BLOCKS_SZ = sizeof(ALL_BLOCKS)/sizeof(BlockInfo);

/*******************************/
/*  CONTINUEB                  */
/*******************************/
void Ndbcntr::execCONTINUEB(Signal* signal) 
{
  jamEntry();
  UintR Ttemp1 = signal->theData[0];
  switch (Ttemp1) {
  case ZCONTINUEB_1:
    jam();
    if (cwaitContinuebFlag == ZFALSE) {
      jam();
/*******************************/
/* SIGNAL NOT WANTED ANYMORE   */
/*******************************/
      return;	
    } else {
      jam();
/*******************************/
/* START ALREADY IN PROGRESS   */
/*******************************/
      if (cstartProgressFlag == ZVOTING) {
        jam();
        systemErrorLab(signal);
        return;
      }//if
      if (ctypeOfStart == NodeState::ST_NODE_RESTART) {
        jam();
        systemErrorLab(signal);
        return;
      }//if
      ph2ELab(signal);
      return;
    }//if
    break;
  case ZSHUTDOWN:
    jam();
    c_stopRec.checkTimeout(signal);
    break;
  default:
    jam();
    systemErrorLab(signal);
    return;
    break;
  }//switch
}//Ndbcntr::execCONTINUEB()

/*******************************/
/*  SYSTEM_ERROR               */
/*******************************/
void Ndbcntr::execSYSTEM_ERROR(Signal* signal) 
{
  const SystemError * const sysErr = (SystemError *)signal->getDataPtr();
  char buf[100];
  int killingNode = refToNode(sysErr->errorRef);
  
  jamEntry();
  switch (sysErr->errorCode){
  case SystemError::StartInProgressError:    
    snprintf(buf, sizeof(buf), 
	     "Node %d killed this node because "
	     "master start in progress error",     
	     killingNode);
    break;

  case SystemError::GCPStopDetected:
    snprintf(buf, sizeof(buf), 
	     "Node %d killed this node because "
	     "GCP stop was detected",     
	     killingNode);
    break;

  case SystemError::ScanfragTimeout:
    snprintf(buf, sizeof(buf), 
	     "Node %d killed this node because "
	     "a fragment scan timed out and could not be stopped",     
	     killingNode);
    break;

  case SystemError::ScanfragStateError:
    snprintf(buf, sizeof(buf), 
	     "Node %d killed this node because "
	     "the state of a fragment scan was out of sync.",     
	     killingNode);
    break;

  case SystemError::CopyFragRefError:
    snprintf(buf, sizeof(buf), 
	     "Node %d killed this node because "
	     "it could not copy a fragment during node restart",     
	     killingNode);
    break;

  default:
    snprintf(buf, sizeof(buf), "System error %d, "
	     " this node was killed by node %d", 
	     sysErr->errorCode, killingNode);
    break;
  }

  progError(__LINE__, 
	    ERR_SYSTEM_ERROR,
	    buf);
  return;
}//Ndbcntr::execSYSTEM_ERROR()

/*---------------------------------------------------------------------------*/
/* The STTOR signal is on level C, we use CONTINUEB to get into level B      */
/*---------------------------------------------------------------------------*/
/**************************** >----------------------------------------------*/
/*  STTOR                     >  SENDER   : MISSRA                           */
/**************************** >------------------+ RECEIVER : NDBCNTR        */
                                                /* INPUT    : CSTART_PHASE   */
                                                /*            CSIGNAL_KEY    */
                                                /*---------------------------*/
/*******************************/
/*  STTOR                      */
/*******************************/
void Ndbcntr::execSTTOR(Signal* signal) 
{
  jamEntry();
  cstartPhase = signal->theData[1];
  csignalKey = signal->theData[6];

  NodeState newState(NodeState::SL_STARTING, cstartPhase, 
		     (NodeState::StartType)ctypeOfStart);
  updateNodeState(signal, newState);
  
  switch (cstartPhase) {
  case 0:
    if(theConfiguration.getInitialStart()){
      jam();
      c_fsRemoveCount = 0;
      clearFilesystem(signal);
      return;
    }
    sendSttorry(signal);
    break;
  case ZSTART_PHASE_1:
    jam();
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
    systemErrorLab(signal);
    return;
    break;
  }//switch
}//Ndbcntr::execNDB_STTORRY()

/*
4.2  START PHASE 1 */
/*###########################################################################*/
/*LOAD OUR BLOCK REFERENCE AND OUR NODE ID. LOAD NODE IDS OF ALL NODES IN    */
/* CLUSTER CALCULATE BLOCK REFERENCES OF ALL BLOCKS IN THIS NODE             */
/*---------------------------------------------------------------------------*/
/*******************************/
/*  STTOR                      */
/*******************************/
void Ndbcntr::startPhase1Lab(Signal* signal) 
{
  jamEntry();

  initData(signal);
  cownBlockref = calcNdbCntrBlockRef(0);
  cnoRunNodes = 0;
  cnoRegNodes = 0;

  NdbBlocksRecPtr ndbBlocksPtr;

  cdynamicNodeId = 0;
  cownBlockref   = calcNdbCntrBlockRef(getOwnNodeId());
  cqmgrBlockref  = calcQmgrBlockRef(getOwnNodeId());
  cdictBlockref  = calcDictBlockRef(getOwnNodeId());
  cdihBlockref   = calcDihBlockRef(getOwnNodeId());
  clqhBlockref   = calcLqhBlockRef(getOwnNodeId());
  ctcBlockref    = calcTcBlockRef(getOwnNodeId());
  ccmvmiBlockref = numberToRef(CMVMI, getOwnNodeId());

  ndbBlocksPtr.i = 0;
  ptrAss(ndbBlocksPtr, ndbBlocksRec);
  ndbBlocksPtr.p->blockref = clqhBlockref;
  ndbBlocksPtr.i = 1;
  ptrAss(ndbBlocksPtr, ndbBlocksRec);
  ndbBlocksPtr.p->blockref = cdictBlockref;
  ndbBlocksPtr.i = 2;
  ptrAss(ndbBlocksPtr, ndbBlocksRec);
  ndbBlocksPtr.p->blockref = calcTupBlockRef(getOwnNodeId());
  ndbBlocksPtr.i = 3;
  ptrAss(ndbBlocksPtr, ndbBlocksRec);
  ndbBlocksPtr.p->blockref = calcAccBlockRef(getOwnNodeId());
  ndbBlocksPtr.i = 4;
  ptrAss(ndbBlocksPtr, ndbBlocksRec);
  ndbBlocksPtr.p->blockref = ctcBlockref;
  ndbBlocksPtr.i = 5;
  ptrAss(ndbBlocksPtr, ndbBlocksRec);
  ndbBlocksPtr.p->blockref = cdihBlockref;
  sendSttorry(signal);
  return;
}

/*
4.3  START PHASE 2      */
/*###########################################################################*/
// SEND A REGISTATION REQUEST TO QMGR AND WAIT FOR REPLY APPL_REGCONF OR 
// APPL_REGREF COLLECT ALL OTHER NDB NODES
// AND THEIR STATES FIND OUT WHAT KIND OF START THIS NODE ARE GOING TO PERFORM 
// IF THIS IS A SYSTEM OR INITIAL
// RESTART THEN FIND OUT WHO IS THE MASTER IF THIS NODE BECOME THE CNTR MASTER
// THEN COLLECT CNTR_MASTERREQ FROM
// ALL OTHER REGISTRATED CNTR THE MASTER WILL SEND BACK A CNTR_MASTERCONF WITH 
// FINAL DECISSION ABOUT WHAT TYPE
// OF START AND WHICH NODES ARE APPROVED TO PARTICIPATE IN THE START IF THE 
// RECEIVER OF CNTR_MASTERREQ HAVE A
// BETTER CHOICE OF MASTER THEN SEND CNTR_MASTERREF. NEW NODES ARE ALWAYS 
// ALLOWED TO REGISTER, EVEN DURING
// RESTART BUT THEY WILL BE IGNORED UNTIL THE START HAVE FINISHED.
// SEND SIGNAL NDBSTTOR TO ALL BLOCKS, ACC, DICT, DIH, LQH, TC AND TUP
// SEND SIGNAL APPL_REGREQ TO QMGR IN THIS NODE AND WAIT FOR REPLY
// APPL_REGCONF OR APPL_REGREF                   */
/*--------------------------------------------------------------------------*/
/*******************************/
/*  READ_NODESREF              */
/*******************************/
void Ndbcntr::execREAD_NODESREF(Signal* signal) 
{
  jamEntry();
  systemErrorLab(signal);
  return;
}//Ndbcntr::execREAD_NODESREF()

/*******************************/
/*  APPL_REGREF                */
/*******************************/
void Ndbcntr::execAPPL_REGREF(Signal* signal) 
{
  jamEntry();
  systemErrorLab(signal);
  return;
}//Ndbcntr::execAPPL_REGREF()

/*******************************/
/*  CNTR_MASTERREF             */
/*******************************/
void Ndbcntr::execCNTR_MASTERREF(Signal* signal) 
{
  jamEntry();
  systemErrorLab(signal);
  return;
}//Ndbcntr::execCNTR_MASTERREF()

/*******************************/
/*  NDB_STARTREF               */
/*******************************/
void Ndbcntr::execNDB_STARTREF(Signal* signal) 
{
  jamEntry();
  systemErrorLab(signal);
  return;
}//Ndbcntr::execNDB_STARTREF()

/*******************************/
/*  STTOR                      */
/*******************************/
void Ndbcntr::startPhase2Lab(Signal* signal) 
{
  cinternalStartphase = cstartPhase - 1;
/*--------------------------------------*/
/* CASE: CSTART_PHASE = ZSTART_PHASE_2  */
/*--------------------------------------*/
  cndbBlocksCount = 0;
  cwaitContinuebFlag = ZFALSE;
/* NOT WAITING FOR SIGNAL CONTINUEB     */

  clastGci = 0;
  signal->theData[0] = cownBlockref;
  sendSignal(cdihBlockref, GSN_DIH_RESTARTREQ, signal, 1, JBB);
  return;
}//Ndbcntr::startPhase2Lab()

/*******************************/
/*  DIH_RESTARTCONF            */
/*******************************/
void Ndbcntr::execDIH_RESTARTCONF(Signal* signal) 
{
  jamEntry();
  cmasterDihId = signal->theData[0];
  clastGci = signal->theData[1];
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
  signal->theData[0] = cownBlockref;
  sendSignal(cqmgrBlockref, GSN_READ_NODESREQ, signal, 1, JBB);
  return;
}//Ndbcntr::ph2ALab()

/*******************************/
/*  READ_NODESCONF             */
/*******************************/
void Ndbcntr::execREAD_NODESCONF(Signal* signal) 
{
  jamEntry();
  ReadNodesConf * const readNodes = (ReadNodesConf *)&signal->theData[0];

  for (nodePtr.i = 1; nodePtr.i < MAX_NDB_NODES; nodePtr.i++) {
    jam();
    ptrAss(nodePtr, nodeRec);
    if(NodeBitmask::get(readNodes->allNodes, nodePtr.i)){
      jam();
      nodePtr.p->nodeDefined = ZTRUE;
    } else {
      jam();
      nodePtr.p->nodeDefined = ZFALSE;
    }//if
  }//for

  CfgBlockRecPtr cfgBlockPtr;
  
  cfgBlockPtr.i = 0;
  ptrAss(cfgBlockPtr, cfgBlockRec);
  signal->theData[0] = cownBlockref;
  signal->theData[1] = cfgBlockPtr.i;
  sendSignal(ccmvmiBlockref, GSN_CMVMI_CFGREQ, signal, 2, JBB);
  return;
}

/*******************************/
/*  CMVMI_CFGCONF              */
/*******************************/
void Ndbcntr::execCMVMI_CFGCONF(Signal* signal) 
{
  CfgBlockRecPtr cfgBlockPtr;
  jamEntry();

  CmvmiCfgConf * const cfgConf = (CmvmiCfgConf *)&signal->theData[0];

  cfgBlockPtr.i = cfgConf->startPhase;
  ptrCheckGuard(cfgBlockPtr, ZSIZE_CFG_BLOCK_REC, cfgBlockRec);
  for(unsigned int i = 0; i<CmvmiCfgConf::NO_OF_WORDS; i++)
    cfgBlockPtr.p->cfgData[i] = cfgConf->theData[i];

  if (cfgBlockPtr.i < 4) {
    jam();
    cfgBlockPtr.i = cfgBlockPtr.i + 1;
    signal->theData[0] = cownBlockref;
    signal->theData[1] = cfgBlockPtr.i;
    sendSignal(ccmvmiBlockref, GSN_CMVMI_CFGREQ, signal, 2, JBB);
    return;
  } 
  
  jam();

  cfgBlockPtr.i = 0;
  ptrAss(cfgBlockPtr, cfgBlockRec);
  
  cdelayStart = cfgBlockPtr.p->cfgData[0];
  
  signal->theData[0] = cownBlockref;
  signal->theData[1] = strlen(ZNAME_OF_APPL) | (ZNAME_OF_APPL[0] << 8);
  signal->theData[2] = ZNAME_OF_APPL[1] | (ZNAME_OF_APPL[2] << 8);
  signal->theData[9] = ZAPPL_SUBTYPE;
  signal->theData[10] = 0; //NDB_VERSION;
  sendSignal(cqmgrBlockref, GSN_APPL_REGREQ, signal, 11, JBB);
  return;	/* WAIT FOR APPL_REGCONF                */
}//Ndbcntr::execCMVMI_CFGCONF()

/*******************************/
/*  APPL_REGCONF               */
/*******************************/
void Ndbcntr::execAPPL_REGCONF(Signal* signal) 
{
  jamEntry();
  cqmgrConnectionP = signal->theData[0];
  cnoNdbNodes = signal->theData[1];
  if(ctypeOfStart == NodeState::ST_INITIAL_START){
    cmasterCandidateId = signal->theData[2];
  } else {
    cmasterCandidateId = ZNIL;
  }

  nodePtr.i = getOwnNodeId();
  ptrCheckGuard(nodePtr, MAX_NDB_NODES, nodeRec);
  
  /*----------------------------------------------------------------------*/
  /* CALCULATE HOW MANY NODES THAT WE NEED TO PERFORM A START. MAKE A     */
  /* DECISION ABOUT WAITING FOR MORE NODES OR TO CONTINUE AT ONCE         */
  /*----------------------------------------------------------------------*/
  nodePtr.p->state = ZADD;
  nodePtr.p->ndbVersion = 0; //NDB_VERSION;
  nodePtr.p->subType = ZAPPL_SUBTYPE;
  nodePtr.p->dynamicId = signal->theData[3];
  // Save dynamic nodeid in global variable
  cdynamicNodeId = nodePtr.p->dynamicId;
  cnoRegNodes = cnoRegNodes + 1;
  switch((NodeState::StartType)ctypeOfStart){
  case NodeState::ST_INITIAL_START:
    jam();
    cnoNeedNodes = cnoNdbNodes;
    break;
  case NodeState::ST_SYSTEM_RESTART:
   if (cnoNdbNodes == 2) {
     jam();
     /*--------------------------------------*/
     /* NEED > 50% OF ALL NODES.             */
     /* WE WILL SEND CONTINUEB WHEN THE WE   */
     /* RECEIVE THE FIRST APPL_CHANGEREP.    */
     /*--------------------------------------*/
     cnoNeedNodes = 1;	/* IF ONLY 2 NODES IN CLUSTER, 1 WILL DO*/
   } else {
     jam();
     cnoNeedNodes = (cnoNdbNodes >> 1) + 1;
   }//if
   break;
  case NodeState::ST_NODE_RESTART:
  case NodeState::ST_INITIAL_NODE_RESTART:
    break;
  default:
    ndbrequire(false);
  }//if
  
  /*--------------------------------------------------------------*/
  /*       WE CAN COME HERE ALSO IN A NODE RESTART IF THE         */
  /*       REGISTRATION OF A RUNNING NODE HAPPENS TO ARRIVE BEFORE*/
  /*       THE APPL_REGCONF SIGNAL.                               */
  /*       IN THAT CASE CNO_NEED_NODES = ZNIL IF NOT NODE_STATE   */
  /*       SIGNAL HAS RETURNED THE PROPER VALUE. IN BOTH CASES WE */
  /*       DO NOT NEED TO ASSIGN IT HERE.                         */
  /*--------------------------------------------------------------*/
  ph2CLab(signal);
  return;
}//Ndbcntr::execAPPL_REGCONF()

/*--------------------------------------------------------------*/
/* CHECK THAT WE GOT ALL NODES REGISTRATED AS WE NEED FOR THIS  */
/* KIND OF START. WE ALWAYS END UP HERE AFTER HANDLING OF       */
/* APPL_CHANGEREP AND NODE_STATESCONF                           */
/*--------------------------------------------------------------*/
void Ndbcntr::ph2CLab(Signal* signal) 
{
  NodeRecPtr ownNodePtr;
  ownNodePtr.i = getOwnNodeId();
  ptrCheckGuard(ownNodePtr, MAX_NDB_NODES, nodeRec);
  if (ownNodePtr.p->state != ZADD) {
    jam();
    return;
  }//if
  switch (ctypeOfStart) {
  case NodeState::ST_INITIAL_START:
    jam();
    if (cnoRegNodes == cnoNeedNodes) {
      jam();
      ph2ELab(signal);
/*******************************/
/* ALL NODES ADDED             */
/*******************************/
      return;
    }//if
    break;
  case NodeState::ST_SYSTEM_RESTART:
    ndbrequire(cnoRunNodes == 0);
    if (cnoRegNodes == cnoNdbNodes) {
      jam();
      /*******************************/
      /* ALL NODES ADDED             */
      /*******************************/
      ph2ELab(signal);
      return;
    }//if
    if (cwaitContinuebFlag == ZFALSE) {
      if (cnoRegNodes == cnoNeedNodes) {
	jam();
	/****************************************/
	/* ENOUGH NODES ADDED, WAIT CDELAY_START*/
	/****************************************/
	cwaitContinuebFlag = ZTRUE;
	/*******************************/
	/* A DELAY SIGNAL TO MYSELF    */
	/*******************************/
	signal->theData[0] = ZCONTINUEB_1;
	sendSignalWithDelay(cownBlockref, GSN_CONTINUEB,
			    signal, cdelayStart * 1000, 1);
	return;
      }//if
    }//if
    break;
  case NodeState::ST_NODE_RESTART:
  case NodeState::ST_INITIAL_NODE_RESTART:
    jam();
    if (cnoNeedNodes <= cnoRunNodes) {
      /*----------------------------------------------*/
      /* GOT ALL RUNNING NODES                        */
      /* " =< " :NODES MAY HAVE FINISHED A NODERESTART*/
      /* WHILE WE WERE WAITING FOR NODE_STATESCONF    */
      /*----------------------------------------------*/
      if (cnoRegNodes != (cnoRunNodes + 1)) {
        jam();
        systemErrorLab(signal);
        return;
      }//if
      getStartNodes(signal);
      cwaitContinuebFlag = ZFALSE;
      cstartProgressFlag = ZTRUE;
      /*--------------------------------------------------------------*/
      /* IF SOMEONE ELSE IS PERFORMING NODERESTART THEN WE GOT A REF  */
      /* AND WE HAVE TO MAKE A NEW NODE_STATESREQ                     */
      /*--------------------------------------------------------------*/
      sendCntrMasterreq(signal);
    }//if
    break;
  default:
    jam();
    systemErrorLab(signal);
    return;
    break;
  }//switch
  /*--------------------------------------------------------------*/
  /* WAIT FOR THE CONTINUEB SIGNAL                                */
  /* AND / OR MORE NODES TO REGISTER                              */
  /*--------------------------------------------------------------*/
  return;
}//Ndbcntr::ph2CLab()

/*******************************/
/*  CONTINUEB                  */
/*******************************/
/*--------------------------------------------------------------*/
/* WE COME HERE ONLY IN SYSTEM RESTARTS AND INITIAL START. FOR  */
/* INITIAL START WE HAVE ALREADY CALCULATED THE MASTER. FOR     */
/* SYSTEM RESTART WE NEED TO PERFORM A VOTING SCHEME TO AGREE   */
/* ON A COMMON MASTER. WE GET OUR VOTE FROM DIH AND THE RESTART */
/* INFORMATION IN DIH.                                          */
/*--------------------------------------------------------------*/
void Ndbcntr::ph2ELab(Signal* signal) 
{
  cwaitContinuebFlag = ZFALSE;
/*--------------------------------------*/
/* JMP TO THIS WHEN ENOUGH NO OF        */
/* NODES ADDED                          */
/*--------------------------------------*/
/*--------------------------------------*/
/* IGNORE CONTINUEB SIGNAL              */
/* CONTINUEB SIGNALS WILL EXIT AT       */
/* SIGNAL RECEPTION                     */
/*--------------------------------------*/
  if (cnoRegNodes >= cnoNeedNodes) {
    jam();
    getStartNodes(signal);
    if (ctypeOfStart == NodeState::ST_INITIAL_START) {
      if (cmasterCandidateId != getOwnNodeId()) {
        jam();
/*--------------------------------------*/
/* THIS NODE IS NOT THE MASTER          */
/* DON'T SEND ANY MORE CNTR_MASTERREQ   */
/* VOTE FOR MASTER                      */
/*--------------------------------------*/
        cstartProgressFlag = ZTRUE;
        sendCntrMasterreq(signal);
        resetStartVariables(signal);
      } else {
        jam();
        masterreq020Lab(signal);
      }//if
    } else if (ctypeOfStart == NodeState::ST_SYSTEM_RESTART) {
      jam();
/*--------------------------------------------------------------*/
/* WE START THE SELECTION OF MASTER PROCESS. IF WE HAVE NOT     */
/* COMPLETED THIS BEFORE THE TIME OUT WE WILL TRY A NEW RESTART.*/
/*--------------------------------------------------------------*/
      cwaitContinuebFlag = ZTRUE;
      cstartProgressFlag = ZVOTING;
      for (nodePtr.i = 1; nodePtr.i < MAX_NDB_NODES; nodePtr.i++) {
        jam();
        ptrAss(nodePtr, nodeRec);
        if (nodePtr.p->state == ZADD) {
          jam();
          signal->theData[0] = getOwnNodeId();
          signal->theData[1] = cmasterDihId;
          signal->theData[2] = clastGci;
          sendSignal(nodePtr.p->cntrBlockref, GSN_VOTE_MASTERORD,
                     signal, 3, JBB);
        }//if
      }//for
    } else {
      jam();
      systemErrorLab(signal);
    }//if
  } else {
    jam();
/*--------------------------------------------------------------*/
/* TOO FEW NODES TO START                                       */
/* WE HAVE WAITED FOR THE GIVEN TIME OUT AND NOT ENOUGH NODES   */
/* HAS REGISTERED. WE WILL CRASH AND RENEW THE ATTEMPT TO START */
/* THE SYSTEM.                                                  */
/*--------------------------------------------------------------*/
    systemErrorLab(signal);
  }//if
  return;
}//Ndbcntr::ph2ELab()

/*******************************/
/* MASTER NODE CONFIRMS REQ    */
/*  CNTR_MASTERCONF            */
/*******************************/
void Ndbcntr::execCNTR_MASTERCONF(Signal* signal) 
{
  jamEntry();

  CntrMasterConf * const cntrMasterConf = 
                                 (CntrMasterConf *)&signal->theData[0];

  cnoStartNodes = cntrMasterConf->noStartNodes;
  int index = 0;
  unsigned i;
  for (i = 1; i < MAX_NDB_NODES; i++) {
    jam();
    if (NodeBitmask::get(cntrMasterConf->theNodes, i)) {
      jam();
      cstartNodes[index] = i;
      index++;
    }//if
  }//for
  if (cnoStartNodes != index) {
    jam();
    systemErrorLab(signal);
  }//if
  ph2FLab(signal);
  return;
}//Ndbcntr::execCNTR_MASTERCONF()

void Ndbcntr::ph2FLab(Signal* signal) 
{
/*--------------------------------------------------------------*/
//The nodes have been selected and we now know which nodes are
// included in the system restart. We can reset wait for CONTINUEB
// flag to ensure system is not restarted when CONTINUEB after the
// delay.
/*--------------------------------------------------------------*/
  cmasterNodeId = cmasterCandidateId;
  cwaitContinuebFlag = ZFALSE;
  ph2GLab(signal);
  return;
}//Ndbcntr::ph2FLab()

/*--------------------------------------*/
/* RECEIVED CNTR_MASTERCONF             */
/*--------------------------------------*/
/*******************************/
/*  NDB_STTORRY                */
/*******************************/
/*---------------------------------------------------------------------------*/
// NOW WE CAN START NDB START PHASE 1. IN THIS PHASE ALL BLOCKS
// (EXCEPT DIH THAT INITIALISED WHEN
// RECEIVING DIH_RESTARTREQ) WILL INITIALISE THEIR DATA, COMMON VARIABLES,
// LINKED LISTS AND RECORD VARIABLES.
/*---------------------------------------------------------------------------*/
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
  cinternalStartphase = cstartPhase - 1;
/*--------------------------------------*/
/* CASE: CSTART_PHASE = ZSTART_PHASE_3  */
/*--------------------------------------*/
  cndbBlocksCount = 0;
  ph3ALab(signal);
  return;
}//Ndbcntr::startPhase3Lab()

/*******************************/
/*  NDB_STTORRY                */
/*******************************/
void Ndbcntr::ph3ALab(Signal* signal) 
{
  Uint16 tnoStartNodes;

  if (cndbBlocksCount < ZNO_NDB_BLOCKS) {
    jam();
    sendNdbSttor(signal);
    return;
  }//if
/*******************************/
/*< APPL_STARTREG             <*/
/*******************************/
  if (ctypeOfStart == NodeState::ST_NODE_RESTART) {
    jam();
    tnoStartNodes = 1;
  } else if (ctypeOfStart == NodeState::ST_INITIAL_NODE_RESTART) {
    jam();
    tnoStartNodes = 1;
  } else {
    jam();
    tnoStartNodes = cnoStartNodes;
  }//if
  signal->theData[0] = cqmgrConnectionP;
  signal->theData[1] = tnoStartNodes;
  sendSignal(cqmgrBlockref, GSN_APPL_STARTREG, signal, 2, JBB);
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
  cinternalStartphase = cstartPhase - 1;
/*--------------------------------------*/
/* CASE: CSTART_PHASE = ZSTART_PHASE_4  */
/*--------------------------------------*/
  cndbBlocksCount = 0;
  cnoWaitrep = 0;
  if (capplStartconfFlag != ZTRUE) {
    jam();
/*------------------------------------------------------*/
/* HAVE WE ALREADY RECEIVED APPL_STARTCONF              */
/*------------------------------------------------------*/
    return;
  }//if
  ph4ALab(signal);
  return;
}//Ndbcntr::startPhase4Lab()

/*******************************/
/*  APPL_STARTCONF             */
/*******************************/
void Ndbcntr::execAPPL_STARTCONF(Signal* signal) 
{
  jamEntry();
  if (cstartPhase == ZSTART_PHASE_4) {
    jam();
    ph4ALab(signal);
    return;
  } else {
    jam();
    capplStartconfFlag = ZTRUE;
//------------------------------------------------
/* FLAG WILL BE CHECKED WHEN WE RECEIVED STTOR  */
/* SIGNAL MAY BE RECEIVED IN STARTPHASE 3       */
//------------------------------------------------
    return;
  }//if
}//Ndbcntr::execAPPL_STARTCONF()

void Ndbcntr::ph4ALab(Signal* signal) 
{
  nodePtr.i = getOwnNodeId();
  ptrCheckGuard(nodePtr, MAX_NDB_NODES, nodeRec);
  nodePtr.p->state = ZSTART;
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
      signal->theData[0] = cownBlockref;
      signal->theData[1] = ctypeOfStart;
      sendSignal(cdihBlockref, GSN_NDB_STARTREQ, signal, 2, JBB);
    }//if
  } else {
    jam();
/*--------------------------------------*/
/* SLAVE NODES WILL PASS HERE ONCE AND  */
/* SEND A WAITPOINT REPORT TO MASTER.   */
/* SLAVES WONT DO ANYTHING UNTIL THEY   */
/* RECEIVE A WAIT REPORT FROM THE MASTER*/
/*--------------------------------------*/
    nodePtr.i = cmasterNodeId;
    ptrCheckGuard(nodePtr, MAX_NDB_NODES, nodeRec);
    signal->theData[0] = getOwnNodeId();
    signal->theData[1] = ZWAITPOINT_4_1;
    sendSignal(nodePtr.p->cntrBlockref, GSN_CNTR_WAITREP, signal, 2, JBB);
  }//if
  return;
}//Ndbcntr::waitpoint41Lab()

/*******************************/
/*  NDB_STARTCONF              */
/*******************************/
void Ndbcntr::execNDB_STARTCONF(Signal* signal) 
{
  jamEntry();
  UintR guard0;
  UintR Ttemp1;

  guard0 = cnoStartNodes - 1;
  arrGuard(guard0, MAX_NDB_NODES);
  for (Ttemp1 = 0; Ttemp1 <= guard0; Ttemp1++) {
    jam();
    if (cstartNodes[Ttemp1] != getOwnNodeId()) {
      jam();
      nodePtr.i = cstartNodes[Ttemp1];
      ptrCheckGuard(nodePtr, MAX_NDB_NODES, nodeRec);
      signal->theData[0] = getOwnNodeId();
      signal->theData[1] = ZWAITPOINT_4_2;
      sendSignal(nodePtr.p->cntrBlockref, GSN_CNTR_WAITREP, signal, 2, JBB);
    }//if
  }//for
  sendSttorry(signal);
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
  cinternalStartphase = cstartPhase - 1;
  cndbBlocksCount = 0;
  cnoWaitrep = 0;
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
      break;
    case NodeState::ST_ILLEGAL_TYPE:
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
    req->senderRef = cownBlockref;
    req->nodeId = getOwnNodeId();
    req->internalStartPhase = cinternalStartphase;
    req->typeOfStart = ctypeOfStart;
    req->masterNodeId = cmasterNodeId;
    
#ifdef TRACE_STTOR
    ndbout_c("sending NDB_STTOR(%d) to DIH", cinternalStartphase);
#endif
    sendSignal(cdihBlockref, GSN_NDB_STTOR, signal, 
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
    nodePtr.i = cmasterNodeId;
    ptrCheckGuard(nodePtr, MAX_NDB_NODES, nodeRec);
    signal->theData[0] = getOwnNodeId();
    signal->theData[1] = ZWAITPOINT_5_2;
    sendSignal(nodePtr.p->cntrBlockref, GSN_CNTR_WAITREP, signal, 2, JBB);
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
    NdbSttor * const req = (NdbSttor*)signal->getDataPtrSend();
    req->senderRef = cownBlockref;
    req->nodeId = getOwnNodeId();
    req->internalStartPhase = cinternalStartphase;
    req->typeOfStart = ctypeOfStart;
    req->masterNodeId = cmasterNodeId;
#ifdef TRACE_STTOR
    ndbout_c("sending NDB_STTOR(%d) to DIH", cinternalStartphase);
#endif
    sendSignal(cdihBlockref, GSN_NDB_STTOR, signal, 
	       NdbSttor::SignalLength, JBB);
  }//if
  return;
}//Ndbcntr::waitpoint52Lab()

/*******************************/
/*  NDB_STTORRY                */
/*******************************/
void Ndbcntr::ph6ALab(Signal* signal) 
{
  UintR guard0;
  UintR Ttemp1;

  if ((ctypeOfStart == NodeState::ST_NODE_RESTART) ||
      (ctypeOfStart == NodeState::ST_INITIAL_NODE_RESTART)) {
    jam();
    waitpoint51Lab(signal);
    return;
  }//if
  guard0 = cnoStartNodes - 1;
  arrGuard(guard0, MAX_NDB_NODES);
  for (Ttemp1 = 0; Ttemp1 <= guard0; Ttemp1++) {
    jam();
    if (cstartNodes[Ttemp1] != getOwnNodeId()) {
      jam();
      nodePtr.i = cstartNodes[Ttemp1];
      ptrCheckGuard(nodePtr, MAX_NDB_NODES, nodeRec);
      signal->theData[0] = getOwnNodeId();
      signal->theData[1] = ZWAITPOINT_5_1;
      sendSignal(nodePtr.p->cntrBlockref, GSN_CNTR_WAITREP, signal, 2, JBB);
    }//if
  }//for
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
      Uint32 guard0 = cnoStartNodes - 1;
      arrGuard(guard0, MAX_NDB_NODES);
      for (Uint32 Ttemp1 = 0; Ttemp1 <= guard0; Ttemp1++) {
        jam();
        if (cstartNodes[Ttemp1] != getOwnNodeId()) {
          jam();
          nodePtr.i = cstartNodes[Ttemp1];
          ptrCheckGuard(nodePtr, MAX_NDB_NODES, nodeRec);
          signal->theData[0] = getOwnNodeId();
          signal->theData[1] = ZWAITPOINT_6_2;
          sendSignal(nodePtr.p->cntrBlockref, GSN_CNTR_WAITREP, signal, 2, JBB);
        }
      }
      sendSttorry(signal);
    }
  } else {
    jam();
    nodePtr.i = cmasterNodeId;
    ptrCheckGuard(nodePtr, MAX_NDB_NODES, nodeRec);
    signal->theData[0] = getOwnNodeId();
    signal->theData[1] = ZWAITPOINT_6_1;
    sendSignal(nodePtr.p->cntrBlockref, GSN_CNTR_WAITREP, signal, 2, JBB);
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
      Uint32 guard0 = cnoStartNodes - 1;
      arrGuard(guard0, MAX_NDB_NODES);
      for (Uint32 Ttemp1 = 0; Ttemp1 <= guard0; Ttemp1++) {
        jam();
        if (cstartNodes[Ttemp1] != getOwnNodeId()) {
          jam();
          nodePtr.i = cstartNodes[Ttemp1];
          ptrCheckGuard(nodePtr, MAX_NDB_NODES, nodeRec);
          signal->theData[0] = getOwnNodeId();
          signal->theData[1] = ZWAITPOINT_7_2;
          sendSignal(nodePtr.p->cntrBlockref, GSN_CNTR_WAITREP, signal, 2, JBB);
        }
      }
      sendSttorry(signal);
    }
  } else {
    jam();
    nodePtr.i = cmasterNodeId;
    ptrCheckGuard(nodePtr, MAX_NDB_NODES, nodeRec);
    signal->theData[0] = getOwnNodeId();
    signal->theData[1] = ZWAITPOINT_7_1;
    sendSignal(nodePtr.p->cntrBlockref, GSN_CNTR_WAITREP, signal, 2, JBB);
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
  signal->theData[0] = cqmgrConnectionP;
  sendSignal(cqmgrBlockref, GSN_APPL_RUN, signal, 1, JBB);
  nodePtr.i = getOwnNodeId();
  ptrCheckGuard(nodePtr, MAX_NDB_NODES, nodeRec);
  nodePtr.p->state = ZRUN;
  cnoRunNodes = cnoRunNodes + 1;
  sendSttorry(signal);
  cstartProgressFlag = ZFALSE;
  ctypeOfStart = (NodeState::StartType)ZSYSTEM_RUN;
  resetStartVariables(signal);
  return;
}//Ndbcntr::ph8BLab()

/*
4.7  HANDLE GLOBAL EVENTS, NOT BOUNDED TO INITIALSTART OR SYSTEM RESTART */
/*#######################################################################*/
/*******************************/
/*  APPL_CHANGEREP             */
/*******************************/
void Ndbcntr::execAPPL_CHANGEREP(Signal* signal) 
{
  jamEntry();
  Uint16 TapplEvent = signal->theData[0];
  Uint16 TapplVersion = signal->theData[1];
  Uint16 TapplNodeId = signal->theData[2];
  Uint16 TapplSubType = signal->theData[3];

  nodePtr.i = TapplNodeId;
  ptrCheckGuard(nodePtr, MAX_NDB_NODES, nodeRec);
  nodePtr.p->subType = TapplSubType;
  nodePtr.p->ndbVersion = TapplVersion;
  nodePtr.p->dynamicId = signal->theData[4];

  switch (TapplEvent) {
  case ZADD:
/*----------------------------*/
/* ADD A NEW NDB NODE TO FILE */
/*----------------------------*/
    if (nodePtr.p->state == ZREMOVE) {
      jam();
      if (cnoRegNodes == cnoNdbNodes) {
        jam();
/*----------------------------------------------*/
/* DON'T ACCEPT MORE NODES THAN SYSFILE.CFG SPEC*/
/*----------------------------------------------*/
        systemErrorLab(signal);
        return;
      }//if
      nodePtr.p->state = ZADD;
      cnoRegNodes = cnoRegNodes + 1;
    } else {
      jam();
      systemErrorLab(signal);
      return;
    }//if
    if (cstartProgressFlag == ZFALSE) {
/*----------------------------------------------*/
/* FLAG = TRUE WHEN CNTR_MASTERREQ IS SENT      */
/*----------------------------------------------*/
      switch (ctypeOfStart) {
      case NodeState::ST_INITIAL_START:
      case NodeState::ST_SYSTEM_RESTART:
        jam();
        ph2CLab(signal);
/*----------------------------------------------*/
/* CHECK IF READY TO MAKE A CNTR_MASTERREQ      */
/*----------------------------------------------*/
        break;
      case NodeState::ST_NODE_RESTART:
      case NodeState::ST_INITIAL_NODE_RESTART:
        jam();
/*------------------------------------------------------------------------*/
/*       THIS SHOULD NEVER OCCUR SINCE WE HAVE ALREADY BEEN ALLOWED TO    */
/*       START OUR NODE. THE NEXT NODE CANNOT START UNTIL WE ARE FINISHED */
/*------------------------------------------------------------------------*/
        systemErrorLab(signal);
        break;
      case ZSYSTEM_RUN:
        jam();
        /*empty*/;
        break;
      default:
        jam();
/*------------------------------------------------------------------------*/
/*       NO PARTICULAR ACTION IS NEEDED. THE NODE WILL PERFORM A NODE     */
/*       RESTART BUT NO ACTION IS NEEDED AT THIS STAGE IN THE RESTART.    */
/*------------------------------------------------------------------------*/
        systemErrorLab(signal);
        break;
      }//switch
    } else {
      jam();
/*--------------------------------------------------------------------------*/
// WHEN A RESTART IS IN PROGRESS THERE IS A POSSIBILITY THAT A NODE
// REGISTER AND
// THINKS THAT HE WOULD BE THE MASTER (LOWER NODE ID) BUT THE OTHER NODE IS
// ALREADY RUNNING THE RESTART. THIS WILL BE DETECTED WHEN HE ATTEMPTS A
// CNTR_MASTERREQ AND RECEIVES A REFUSE SIGNAL IN RETURN. THIS WILL CAUSE HIM
// TO CRASH. IF HE ATTEMPTS TO JOIN AS A NON-MASTER HE WILL WAIT FOR THE MASTER.
// IN THIS CASE IT IS BETTER TO SHOT HIM DOWN. FOR SAFETY REASONS WE WILL ALWAYS
// SHOT HIM DOWN.
/*--------------------------------------------------------------------------*/
      const BlockReference tblockref = calcNdbCntrBlockRef(nodePtr.i);
      
      SystemError * const sysErr = (SystemError*)&signal->theData[0];
      sysErr->errorCode = SystemError::StartInProgressError;
      sysErr->errorRef = reference();
      sendSignal(tblockref, GSN_SYSTEM_ERROR, signal, SystemError::SignalLength, JBA);
    }//if
    break;
  case ZSTART:
    jam();
    if (nodePtr.p->state != ZADD) {
      jam();
      systemErrorLab(signal);
      return;
    }//if
    nodePtr.p->state = ZSTART;
    break;
  case ZRUN:
    if (nodePtr.p->state == ZREMOVE) {
      jam();
      cnoRegNodes = cnoRegNodes + 1;
    } else {
      jam();
      if (nodePtr.p->state != ZSTART) {
        jam();
/*----------------------------------------------*/
/* STATE ZADD OR ZRUN -> ZRUN NOT ALLOWED       */
/*----------------------------------------------*/
        systemErrorLab(signal);
        return;
      }//if
    }//if
    cnoRunNodes = cnoRunNodes + 1;
    nodePtr.p->state = ZRUN;
    switch (ctypeOfStart) {
    case NodeState::ST_INITIAL_START:
      jam();
      detectNoderestart(signal);
      if (ctypeOfStart == NodeState::ST_NODE_RESTART) {
        jam();
/*--------------------------------------------------------------------------*/
/* WE DISCOVERED THAT WE ARE TRYING TO PERFORM A INITIAL START WHEN THERE   */
/* ARE ALREADY RUNNING NODES. THIS MEANS THAT THE NODE HAS CLEANED THE      */
/* FILE SYSTEM AND CONTAINS NO DATA. THIS IS AN INITIAL NODE RESTART WHICH  */
/* IS NECESSARY TO START A NODE THAT HAS BEEN TAKEN OVER.                   */
/*--------------------------------------------------------------------------*/
        ctypeOfStart = NodeState::ST_INITIAL_NODE_RESTART;
      }//if
      break;
    case NodeState::ST_SYSTEM_RESTART:
      jam();
      detectNoderestart(signal);
/*----------------------------------------------*/
/* SHOULD THIS NODE PERFORM A NODE RESTART?     */
/* THEN CHANGE CTYPE_OF_START TO NodeState::ST_NODE_RESTART  */
/* AND SEND NODE_STATESREQ.                     */
/* WAIT FOR NODE_STATESCONF.                    */
/*----------------------------------------------*/
      break;
    case NodeState::ST_NODE_RESTART:
    case NodeState::ST_INITIAL_NODE_RESTART:
      jam();
/*----------------------------------------------*/
/* IF WE ARE WAITING FOR NODE_STATESCONF, THIS  */
/* JUMP WILL EXIT BECAUSE CNO_NEED_NODES = ZNIL */
/* UNTIL WE RECEIVE NODE_STATESCONF             */
/*----------------------------------------------*/
      ph2CLab(signal);
      break;
    case ZSYSTEM_RUN:
      jam();
      break;
    default:
      jam();
      systemErrorLab(signal);
      break;
    }//switch
    break;
  default:
    jam();
    systemErrorLab(signal);
    break;
  }//switch
  return;
}//Ndbcntr::execAPPL_CHANGEREP()

/*--------------------------------------------------------------------------*/
// A NODE HAS ADDED HAS VOTE ON WHICH MASTER IS TO BE CHOOSEN IN A SYSTEM 
// RESTART. WHEN ALL VOTES HAVE
// BEEN ADDED THEN WE ARE PREPARED TO CHOOSE MASTER AND CONTINUE WITH THE
// RESTART PROCESSING.
/*--------------------------------------------------------------------------*/

/*******************************/
/*  VOT_MASTERORD              */
/*******************************/
void Ndbcntr::execVOTE_MASTERORD(Signal* signal) 
{
  jamEntry();
  nodePtr.i = signal->theData[0];
  ptrCheckGuard(nodePtr, MAX_NDB_NODES, nodeRec);
  UintR TmasterCandidateId = signal->theData[1];
  UintR TlastGci = signal->theData[2];
  if (ctypeOfStart != NodeState::ST_SYSTEM_RESTART) {
    jam();
    progError(__LINE__,
	      ERR_SR_RESTARTCONFLICT,
	      "One ore more nodes probably requested an initial SR");
    return;
  }//if
  cmasterVoters = cmasterVoters + 1;
  if (cmasterVoters == 1) {
    jam();
    cmasterCurrentId = TmasterCandidateId;
    cmasterLastGci = TlastGci;
  } else {
    if (cmasterLastGci < TlastGci) {
      jam();
      cmasterCurrentId = TmasterCandidateId;
      cmasterLastGci = TlastGci;
    } else if (cmasterLastGci == TlastGci) {
      jam();
      if (cmasterCurrentId != TmasterCandidateId) {
        jam();
        systemErrorLab(signal);
        return;
      }//if
    }//if
  }//if
  if (cstartProgressFlag == ZVOTING) {
/*--------------------------------------------------------------------------*/
// UNLESS START PROGRESS FLAG IS SET TO VOTING WE HAVE NOT YET REACHED A
// STATE WHERE WE ARE READY TO
// PROCEED WITH THE SYSTEM RESTART. OUR OWN NOTE HAVE AT LEAST NOT BEEN
// CAST INTO THE BALLOT YET.
/*--------------------------------------------------------------------------*/
    if (cmasterVoters == cnoRegNodes) {
      cmasterCandidateId = cmasterCurrentId;
      if (cmasterCandidateId == getOwnNodeId()) {
        jam();
        masterreq020Lab(signal);
        return;
      } else {
        jam();
        cstartProgressFlag = ZTRUE;
        sendCntrMasterreq(signal);
        resetStartVariables(signal);
      }//if
    }//if
  }//if
  return;
}//Ndbcntr::execVOTE_MASTERORD()

/*******************************/
/*  CNTR_MASTERREQ             */
/*******************************/
void Ndbcntr::execCNTR_MASTERREQ(Signal* signal) 
{
  Uint16 ttypeOfStart;

  jamEntry();

  CntrMasterReq * const cntrMasterReq = 
                                 (CntrMasterReq *)&signal->theData[0];

//-----------------------------------------------
// cntrMasterReq->userBlockRef NOT USED
//-----------------------------------------------
  Uint16 TuserNodeId = cntrMasterReq->userNodeId;
  ttypeOfStart = cntrMasterReq->typeOfStart;
  Uint16 TnoRestartNodes = cntrMasterReq->noRestartNodes;
  int index = 0;
  unsigned i;
  for (i = 1; i < MAX_NDB_NODES; i++) {
    jam();
    if (NodeBitmask::get(cntrMasterReq->theNodes, i)) {
      jam();
      cstartNodes[index] = i;
      index++;
    }//if
  }//for
  if (TnoRestartNodes != index) {
    jam();
    systemErrorLab(signal);
  }//if
  switch (ttypeOfStart) {
  case NodeState::ST_INITIAL_START:
  case NodeState::ST_SYSTEM_RESTART:
    jam();
//--------------------------------
/* ELECTION OF MASTER AT        */
/* INITIAL OR SYSTEM RESTART    */
//--------------------------------
    masterreq010Lab(signal, TnoRestartNodes, TuserNodeId);
    break;
  case NodeState::ST_NODE_RESTART:
  case NodeState::ST_INITIAL_NODE_RESTART:
    jam();
    masterreq030Lab(signal, TnoRestartNodes, TuserNodeId);
    break;
  default:
    jam();
    systemErrorLab(signal);
    break;
  }//switch
}//Ndbcntr::execCNTR_MASTERREQ()

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
    systemErrorLab(signal);
    break;
  }//switch
}//Ndbcntr::execCNTR_WAITREP()

/*
4.7.4 MASTERREQ_010     ( CASE: INITIALSTART OR SYSTEMRESTART ) */
/*--------------------------------------------------------------------------*/
// ELECTION OF MASTER AND ELECTION OF PARTICIPANTS IN START. SENDER OF 
// CNTR_MASTERREQ THINKS THAT THIS NODE
// SHOULD BE THE MASTER. WE CAN'T MAKE A DECISION ABOUT WHO THE MASTER
// SHOULD BE UNTIL TIMELIMIT HAS EXPIRED AND
// THAT AT LEAST CNO_NEED_NODES ARE ZADD IN NODE_PTR_REC. IF THIS NODE IS
// MASTER THEN MAKE SURE THAT ALL NODES IN
// THE CLUSTER COMES TO AN AGREEMENT ABOUT A SUBSET OF NODES THAT SATISFIES
// THE NUMBER CNO_NEED_NODES. THERE IS
// A POSSIBILITY THAT THE RECEIVER OF CNTR_MASTERREQ DOESN'T HAS CHOOSEN
// A MASTER, THEN THE RECEIVER CAN'T
// EITHER CONFIRM OR REFUSE JUST STORE THE VOTES OF THE CLUSTERMEMBERS.
// IF THIS NODE BECOME AWARE OF THAT ANOTHER NODE IS MASTER THEN CHECK IF
// ANYONE HAS VOTED (SENT CNTR_MASTERREQ) */
// AND THEN SEND THEM CNTR_MASTERREF BACK.
/*--------------------------------------------------------------------------*/
void Ndbcntr::masterreq010Lab(Signal* signal,
                              Uint16 TnoRestartNodes,
                              Uint16 TuserNodeId) 
{
  UintR guard0;
  UintR Ttemp1;

  nodePtr.i = TuserNodeId;
  ptrCheckGuard(nodePtr, MAX_NDB_NODES, nodeRec);
  if (cstartProgressFlag == ZTRUE) {
    jam();
/*--------------------------------------*/
/* RESTART ALREADY IN PROGRESS          */
/*--------------------------------------*/
    if (ctypeOfStart == NodeState::ST_INITIAL_START) {
      jam();
      systemErrorLab(signal);
      return;
    }//if
    signal->theData[0] = cownBlockref;
    signal->theData[1] = getOwnNodeId();
    signal->theData[2] = ZSTART_IN_PROGRESS_ERROR;
    sendSignal(nodePtr.p->cntrBlockref, GSN_CNTR_MASTERREF, signal, 3, JBB);
    return;
  }//if
  cnoVoters = cnoVoters + 1;
  nodePtr.p->voter = ZTRUE;
  guard0 = TnoRestartNodes - 1;
  arrGuard(guard0, MAX_NDB_NODES);
  for (Ttemp1 = 0; Ttemp1 <= guard0; Ttemp1++) {
    jam();
    nodePtr.i = cstartNodes[Ttemp1];
    ptrCheckGuard(nodePtr, MAX_NDB_NODES, nodeRec);
    nodePtr.p->votes = nodePtr.p->votes + 1;
  }//for
  masterreq020Lab(signal);
  return;
}//Ndbcntr::masterreq010Lab()

/*----------------------------------------------------------------------*/
/* WHEN WE JUST WANT TO CHECK OUR VOTES IT IS POSSIBLE TO JUMP TO THIS  */
/* LABEL. IF WE HAVEN'T RECEIVED ANY VOTES SINCE OUR LASTCHECK WE WILL  */
/* JUST PERFORM AN EXIT                                                 */
/*----------------------------------------------------------------------*/
void Ndbcntr::masterreq020Lab(Signal* signal) 
{
  if (cmasterCandidateId == ZNIL) {
    jam();
/*--------------------------------------*/
/* MASTER UNKNOWN                       */
/*--------------------------------------*/
    return;
  } else if (cmasterCandidateId == getOwnNodeId()) {
    jam();
/*--------------------------------------*/
/* SATISFIED WHEN WE HAVE AS MANY VOTERS*/
/* AS RESTARTNODES - 1, DIFFERENT NODES?*/
/* <- CNO_START_NODES, ALL NODES AGREED */
/* ON THESE CNO_START_NODES             */
/*--------------------------------------*/
    if ((cnoStartNodes - 1) == cnoVoters) {
      chooseRestartNodes(signal);
      if (cnoStartNodes >= cnoNeedNodes) {
        jam();
        cstartProgressFlag = ZTRUE;	
/*--------------------------------------*/
/* DON'T SEND ANY MORE CNTR_MASTERREQ   */
/*--------------------------------------*/
        replyMasterconfToAll(signal);
/*--------------------------------------*/
/* SEND CONF TO ALL PASSED REQ          */
/* DON'T SEND ANYTHING TO REJECTED NODES*/
/* BLOCK THEM UNTIL SYSTEM IS RUNNING   */
/* CONTINUE RESTART                     */
/*--------------------------------------*/
        ph2FLab(signal);
      } else {
        jam();
        systemErrorLab(signal);
      }//if
    }//if
  } else {
    jam();
/*----------------------------------------------------------------------*/
/*       WE RECEIVED A REQUEST TO A MASTER WHILE NOT BEING MASTER. THIS */
/*       MUST BE AN ERROR INDICATION. WE CRASH.                         */
/*----------------------------------------------------------------------*/
    systemErrorLab(signal);
  }//if
  return;	/* WAIT FOR MORE CNTR_MASTERREQ         */
}//Ndbcntr::masterreq020Lab()

void Ndbcntr::masterreq030Lab(Signal* signal, 
                              Uint16 TnoRestartNodes,
                              Uint16 TuserNodeId) 
{
  UintR TretCode;
  if (cmasterNodeId == getOwnNodeId()) {
    jam();
    TretCode = checkNodelist(signal, TnoRestartNodes);
    nodePtr.i = TuserNodeId;
    ptrCheckGuard(nodePtr, MAX_NDB_NODES, nodeRec);
    if (TretCode == 1) {
      jam();
/*******************************************************<*/
/* CSTART_NODES IS OVERWRITTEN IN RECEIVING BLOCK,       */
/* SO WE MUST SEND CNTR_MASTERCONF TO  THE SAME          */
/* CSTART_NODES AS WE RECEIVED IN CNTR_MASTERREQ         */
/*******************************************************<*/

      CntrMasterConf * const cntrMasterConf = 
                           (CntrMasterConf *)&signal->theData[0];
      NodeBitmask::clear(cntrMasterConf->theNodes);
      for (int i = 0; i < TnoRestartNodes; i++){
        jam();
        UintR Tnode = cstartNodes[i];
        arrGuard(Tnode, MAX_NDB_NODES);
        NodeBitmask::set(cntrMasterConf->theNodes, Tnode);
      }//for
      cntrMasterConf->noStartNodes = TnoRestartNodes;
      sendSignal(nodePtr.p->cntrBlockref, GSN_CNTR_MASTERCONF,
                 signal, CntrMasterConf::SignalLength, JBB);
    } else {
      jam();
      signal->theData[0] = cownBlockref;
      signal->theData[1] = getOwnNodeId();
      signal->theData[2] = ZTOO_FEW_NODES;
      sendSignal(nodePtr.p->cntrBlockref, GSN_CNTR_MASTERREF, signal, 3, JBB);
    }//if
  } else {
    jam();
    nodePtr.i = TuserNodeId;
    ptrCheckGuard(nodePtr, MAX_NDB_NODES, nodeRec);
    signal->theData[0] = cownBlockref;
    signal->theData[1] = getOwnNodeId();
    signal->theData[2] = ZNOT_MASTER;
    sendSignal(nodePtr.p->cntrBlockref, GSN_CNTR_MASTERREF, signal, 3, JBB);
  }//if
  return;
}//Ndbcntr::masterreq030Lab()

/*******************************/
/*  NODE_FAILREP               */
/*******************************/
void Ndbcntr::execNODE_FAILREP(Signal* signal) 
{
  UintR TfailureNr;
  UintR TnoOfNodes;
  UintR TreadNodes[MAX_NDB_NODES];

  jamEntry();

  const NodeState & st = getNodeState();
  if(st.startLevel == st.SL_STARTING){
    if(!st.getNodeRestartInProgress()){
      progError(__LINE__,
		ERR_SR_OTHERNODEFAILED,
		"Unhandled node failure during system restart");
    }
  }
  
  {
    NodeFailRep * const nodeFail = (NodeFailRep *)&signal->theData[0];

    TfailureNr = nodeFail->failNo;
    TnoOfNodes = nodeFail->noOfNodes;
    unsigned index = 0;
    unsigned i;
    for (i = 0; i < MAX_NDB_NODES; i++) {
      jam();
      if (NodeBitmask::get(nodeFail->theNodes, i)) {
        jam();
        TreadNodes[index] = i;
        index++;
	ndbrequire(getOwnNodeId() != i);
      }//if
    }//for
    ndbrequire(TnoOfNodes == index);
  }
  
  for (Uint32 i = 0; i < TnoOfNodes; i++) {
    jam();
    nodePtr.i = TreadNodes[i];
    ptrCheckGuard(nodePtr, MAX_NDB_NODES, nodeRec);
    signal->theData[0] = EventReport::NODE_FAILREP;
    signal->theData[1] = nodePtr.i;
    signal->theData[2] = nodePtr.p->state;
    sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 3, JBB);
    if (nodePtr.p->state != ZREMOVE) {
      jam();
      deleteNode(signal);
    }//if
  }//for

/*******************************/
/*< NODE_FAILREP              <*/
/*******************************/
  NodeFailRep * const nodeFail = (NodeFailRep *)&signal->theData[0];
  
  nodeFail->failNo       = TfailureNr;
  nodeFail->masterNodeId = cmasterNodeId;

  nodeFail->noOfNodes = TnoOfNodes; 
  NodeBitmask::clear(nodeFail->theNodes);
  for (unsigned i = 0; i < TnoOfNodes; i++) {
    jam();
    NodeBitmask::set(nodeFail->theNodes, TreadNodes[i]);
  }//for
  
  sendSignal(ctcBlockref, GSN_NODE_FAILREP, signal, 
	     NodeFailRep::SignalLength, JBB);
  
  sendSignal(clqhBlockref, GSN_NODE_FAILREP, signal, 
	     NodeFailRep::SignalLength, JBB);
  
  sendSignal(cdihBlockref, GSN_NODE_FAILREP, signal, 
	     NodeFailRep::SignalLength, JBB);
  
  sendSignal(cdictBlockref, GSN_NODE_FAILREP, signal, 
	     NodeFailRep::SignalLength, JBB);
  
  sendSignal(BACKUP_REF, GSN_NODE_FAILREP, signal,
	     NodeFailRep::SignalLength, JBB);

  sendSignal(SUMA_REF, GSN_NODE_FAILREP, signal,
	     NodeFailRep::SignalLength, JBB);

  sendSignal(GREP_REF, GSN_NODE_FAILREP, signal,
	     NodeFailRep::SignalLength, JBB);
  return;
}//Ndbcntr::execNODE_FAILREP()

/*******************************/
/*  NODE_STATESCONF            */
/*******************************/
void Ndbcntr::execNODE_STATESCONF(Signal* signal) 
{
  jamEntry();
  cmasterCandidateId = signal->theData[0];
  cnoNeedNodes = signal->theData[1];
/*----------------------------------------------------------------------*/
// Now that we have knowledge of how many nodes are needed we will call
// ph2CLab to ensure that node restart continues if we already received
// all APPL_CHANGEREP signals.
/*----------------------------------------------------------------------*/
  ph2CLab(signal);
  return;
}//Ndbcntr::execNODE_STATESCONF()

/*******************************/
/*  NODE_STATESREF             */
/*******************************/
void Ndbcntr::execNODE_STATESREF(Signal* signal) 
{
  jamEntry();
  systemErrorLab(signal);
  return;
}//Ndbcntr::execNODE_STATESREF()

/*******************************/
/*  NODE_STATESREQ             */
/*******************************/
void Ndbcntr::execNODE_STATESREQ(Signal* signal) 
{
  UintR TnoNeedNodes = 0;
  NodeRecPtr TNodePtr;
  jamEntry();
  BlockReference TuserBlockref = signal->theData[0];
/*----------------------------------------------------------------------*/
// IF WE ARE RUNNING, WE WILL ANSWER THIS SIGNAL WITH THE AMOUNT OF NODES
// THAT ARE IN THE RUN STATE OR START STATE.
/*----------------------------------------------------------------------*/
  TNodePtr.i = getOwnNodeId();
  ptrCheckGuard(TNodePtr, MAX_NDB_NODES, nodeRec);
  if (TNodePtr.p->state == ZRUN) {
    jam();
    for (TNodePtr.i = 1; TNodePtr.i < MAX_NDB_NODES; TNodePtr.i++) {
      jam();
      ptrAss(TNodePtr, nodeRec);
      if ((TNodePtr.p->state == ZRUN) ||
           (TNodePtr.p->state == ZSTART)) {
        jam();
        TnoNeedNodes++;
      }//if
    }//for
    signal->theData[0] = cmasterNodeId;
    signal->theData[1] = TnoNeedNodes;
    sendSignal(TuserBlockref, GSN_NODE_STATESCONF, signal, 2, JBB);
  } else {
    jam();
    signal->theData[0] = ZERROR_NOT_RUNNING;
    sendSignal(TuserBlockref, GSN_NODE_STATESREF, signal, 1, JBB);
  }//if
  return;
}//Ndbcntr::execNODE_STATESREQ()

/*******************************/
/*  READ_NODESREQ              */
/*******************************/
void Ndbcntr::execREAD_NODESREQ(Signal* signal) 
{
  UintR TnoNodes = 0;
  NodeRecPtr TNodePtr;
  jamEntry();

  /*----------------------------------------------------------------------*/
  // ANY BLOCK MAY SEND A REQUEST ABOUT NDB NODES AND VERSIONS IN THE
  // SYSTEM. THIS REQUEST CAN ONLY BE HANDLED IN
  // ABSOLUTE STARTPHASE 3 OR LATER
  /*----------------------------------------------------------------------*/
  BlockReference TuserBlockref = signal->theData[0];
  ReadNodesConf * const readNodes = (ReadNodesConf *)&signal->theData[0];
  
  if (cstartPhase > ZSTART_PHASE_2) {
    ndbrequire(cstartProgressFlag == ZTRUE);
    
    NodeBitmask::clear(readNodes->allNodes);
    NodeBitmask::clear(readNodes->inactiveNodes);
    
    /**
     * Add started nodes
     */
    for (int i = 0; i < cnoStartNodes; i++){
      jam();
      TNodePtr.i = cstartNodes[i];
      ptrCheckGuard(TNodePtr, MAX_NDB_NODES, nodeRec);
      
      NodeBitmask::set(readNodes->allNodes, TNodePtr.i);
      readNodes->setVersionId(TNodePtr.i, TNodePtr.p->ndbVersion, 
			      readNodes->theVersionIds);
      TnoNodes++;
    }//for
    
    /**
     * Sometimes add myself
     */
    if ((ctypeOfStart == NodeState::ST_NODE_RESTART) ||
	(ctypeOfStart == NodeState::ST_INITIAL_NODE_RESTART)) {
      jam();

      NodeBitmask::set(readNodes->allNodes, getOwnNodeId());
      readNodes->setVersionId(getOwnNodeId(), NDB_VERSION,
			      readNodes->theVersionIds);
      TnoNodes++;
    }//if
    /**
     * Check all nodes which are defined but not already added
     */
    for (TNodePtr.i = 1; TNodePtr.i < MAX_NDB_NODES; TNodePtr.i++) {
      jam();
      ptrAss(TNodePtr, nodeRec);
      if ((TNodePtr.p->nodeDefined == ZTRUE) &&
	  (NodeBitmask::get(readNodes->allNodes, TNodePtr.i) == false)){
	jam();

	NodeBitmask::set(readNodes->allNodes, TNodePtr.i);
	NodeBitmask::set(readNodes->inactiveNodes, TNodePtr.i);
	readNodes->setVersionId(TNodePtr.i, NDB_VERSION,
				readNodes->theVersionIds);
	
	TnoNodes++;
      }//if 
    }//for
    
    readNodes->noOfNodes = TnoNodes;
    readNodes->masterNodeId = cmasterNodeId;
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
void Ndbcntr::systemErrorLab(Signal* signal) 
{
  progError(0, 0); /* BUG INSERTION */
  return;
}//Ndbcntr::systemErrorLab()

/*###########################################################################*/
/* CNTR MASTER CREATES AND INITIALIZES A SYSTEMTABLE AT INITIALSTART         */
/*       |-2048| # 1 00000001    |                                           */
/*       |  :  |   :             |                                           */
/*       | -1  | # 1 00000001    |                                           */
/*       |  0  |   0             |                                           */
/*       |  1  |   0             |                                           */
/*       |  :  |   :             |                                           */
/*       | 2047|   0             |                                           */
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
  //w.add(DictTabInfo::TableStorageVal, (Uint32)DictTabInfo::MainMemory);
  //w.add(DictTabInfo::NoOfKeyAttr, 1);
  w.add(DictTabInfo::NoOfAttributes, (Uint32)table.columnCount);
  //w.add(DictTabInfo::NoOfNullable, (Uint32)0);
  //w.add(DictTabInfo::NoOfVariable, (Uint32)0);
  //w.add(DictTabInfo::KeyLength, 1);
  w.add(DictTabInfo::TableTypeVal, (Uint32)table.tableType);

  for (unsigned i = 0; i < table.columnCount; i++) {
    const SysColumn& column = table.columnList[i];
    ndbassert(column.pos == i);
    w.add(DictTabInfo::AttributeName, column.name);
    w.add(DictTabInfo::AttributeId, (Uint32)column.pos);
    //w.add(DictTabInfo::AttributeType, DictTabInfo::UnSignedType);
    //w.add(DictTabInfo::AttributeSize, DictTabInfo::a32Bit);
    //w.add(DictTabInfo::AttributeArraySize, 1);
    w.add(DictTabInfo::AttributeKeyFlag, (Uint32)column.keyFlag);
    //w.add(DictTabInfo::AttributeStorage, (Uint32)DictTabInfo::MainMemory);
    w.add(DictTabInfo::AttributeNullableFlag, (Uint32)column.nullable);
    // ext type overrides
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
  progError(0,0);
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
  signal->theData[1] = cownBlockref;
  sendSignal(ctcBlockref, GSN_TCSEIZEREQ, signal, 2, JBB);
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
    tcKeyReq->transId2           = 0;

//-------------------------------------------------------------
// There is no optional part in this TCKEYREQ. There is one
// key word and five ATTRINFO words.
//-------------------------------------------------------------
    Uint32* tKeyDataPtr          = &tcKeyReq->scanInfo;
    Uint32* tAIDataPtr           = &tKeyDataPtr[1];

    tKeyDataPtr[0]               = tkey;

    AttributeHeader::init(&tAIDataPtr[0], 0, 1);
    tAIDataPtr[1]                = tkey;
    AttributeHeader::init(&tAIDataPtr[2], 1, 2);
    tAIDataPtr[3]                = (tkey << 16);
    tAIDataPtr[4]                = 1;    
    sendSignal(ctcBlockref, GSN_TCKEYREQ, signal, 
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
  cgciSystab = keyConf->gci;
  UintR confInfo = keyConf->confInfo;
  
  if (TcKeyConf::getMarkerFlag(confInfo)){
    Uint32 transId1 = keyConf->transId1;
    Uint32 transId2 = keyConf->transId2;
    signal->theData[0] = transId1;
    signal->theData[1] = transId2;
    sendSignal(ctcBlockref, GSN_TC_COMMIT_ACK, signal, 2, JBB);    
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
    crSystab7Lab(signal);
    return;
  }//if
  signal->theData[0] = ctcConnectionP;
  signal->theData[1] = cownBlockref;
  sendSignal(ctcBlockref, GSN_TCRELEASEREQ, signal, 2, JBB);
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
  signal->theData[1] = cownBlockref;
  sendSignalWithDelay(cdihBlockref, GSN_GETGCIREQ, signal, 100, 2);
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
  systemErrorLab(signal);
  return;
}//Ndbcntr::execTCKEYREF()

void Ndbcntr::execTCROLLBACKREP(Signal* signal) 
{
  jamEntry();
  systemErrorLab(signal);
  return;
}//Ndbcntr::execTCROLLBACKREP()

void Ndbcntr::execTCRELEASEREF(Signal* signal) 
{
  jamEntry();
  systemErrorLab(signal);
  return;
}//Ndbcntr::execTCRELEASEREF()

void Ndbcntr::execTCSEIZEREF(Signal* signal) 
{
  jamEntry();
  systemErrorLab(signal);
  return;
}//Ndbcntr::execTCSEIZEREF()

/*
4.10 SUBROUTINES        */
/*##########################################################################*/
/*
4.10.1 CHECK_NODELIST */
/*---------------------------------------------------------------------------*/
/*CHECK THAT ALL THE NEW NODE HAS DETECTED ALL RUNNING NODES                 */
/*INPUT: CSTART_NODES                                                        */
/*       TNO_RESTART_NODES                                                   */
/*       TUSER_NODE_ID                                                       */
/*RET:   CNODE_RESTART                                                       */
/*---------------------------------------------------------------------------*/
UintR Ndbcntr::checkNodelist(Signal* signal, Uint16 TnoRestartNodes) 
{
  UintR guard1;
  UintR Ttemp1;

  if (cnoRunNodes == TnoRestartNodes) {
    jam();
    guard1 = TnoRestartNodes - 1;
    arrGuard(guard1, MAX_NDB_NODES);
    for (Ttemp1 = 0; Ttemp1 <= guard1; Ttemp1++) {
      jam();
      nodePtr.i = cstartNodes[Ttemp1];
      ptrCheckGuard(nodePtr, MAX_NDB_NODES, nodeRec);
      if (nodePtr.p->state != ZRUN) {
        jam();
        return 0;
      }//if
    }//for
    return 1;
  }//if
  return 0;
}//Ndbcntr::checkNodelist()

/*---------------------------------------------------------------------------*/
// SELECT NODES THAT ARE IN THE STATE TO PERFORM A INITIALSTART OR 
// SYSTEMRESTART.
// THIS SUBROUTINE CAN ONLY BE INVOKED BY THE MASTER NODE.
// TO BE CHOOSEN A NODE NEED AS MANY VOTES AS THERE ARE VOTERS, AND OF
// COURSE THE NODE HAS TO BE KNOWN BY THE
// MASTER
// INPUT: NODE_REC
//        CNO_NEED_NODES
// RETURN:CNO_START_NODES
//       CSTART_NODES
/*---------------------------------------------------------------------------*/
void Ndbcntr::chooseRestartNodes(Signal* signal) 
{
  cnoStartNodes = 0;
  for (nodePtr.i = 1; nodePtr.i < MAX_NDB_NODES; nodePtr.i++) {
    ptrAss(nodePtr, nodeRec);
    if (nodePtr.p->votes == cnoVoters) {
      jam();
      if (nodePtr.p->state == ZADD) {
        jam();
        arrGuard(cnoStartNodes, MAX_NDB_NODES);
        cstartNodes[cnoStartNodes] = nodePtr.i;
        cnoStartNodes++;
      }//if
    } else {
      jam();
      if (nodePtr.p->votes > 0) {
        jam();
        systemErrorLab(signal);
        return;
      }//if
    }//if
  }//for
}//Ndbcntr::chooseRestartNodes()

/*
4.10.6 DELETE_NODE */
/*---------------------------------------------------------------------------*/
// INPUT:  NODE_PTR
/*---------------------------------------------------------------------------*/
void Ndbcntr::deleteNode(Signal* signal) 
{
  UintR tminDynamicId;

  if (nodePtr.p->state == ZRUN) {
    jam();
    cnoRunNodes = cnoRunNodes - 1;
  }//if
  nodePtr.p->state = ZREMOVE;
  nodePtr.p->votes = 0;
  nodePtr.p->voter = ZFALSE;
  cnoRegNodes--;
  if (nodePtr.i == cmasterNodeId) {
    jam();
    cmasterNodeId = ZNIL;
/*---------------------------------------------------------------------------*/
//       IF MASTER HAVE CRASHED WE NEED TO SELECT A NEW MASTER.
/*---------------------------------------------------------------------------*/
    for (nodePtr.i = 1; nodePtr.i < MAX_NDB_NODES; nodePtr.i++) {
      jam();
      ptrAss(nodePtr, nodeRec);
      if (nodePtr.p->state == ZRUN) {
        if (cmasterNodeId == ZNIL) {
          jam();
          cmasterNodeId = nodePtr.i;
          tminDynamicId = nodePtr.p->dynamicId;
        } else {
          jam();
          if (nodePtr.p->dynamicId < tminDynamicId) {
            jam();
            cmasterNodeId = nodePtr.i;
            tminDynamicId = nodePtr.p->dynamicId;
          }//if
        }//if
      }//if
    }//for
  }//if
}//Ndbcntr::deleteNode()

/*---------------------------------------------------------------------------*/
// A NEW NODE TRIES TO DETECT A NODE RESTART. NodeState::ST_NODE_RESTART IS A POSSIBLE
// STATE ONLY WHEN THE SYSTEM IS RUNNING.
// IF THE SYSTEM IS RUNNING THEN
// CTYPE_OF_START = NodeState::ST_SYSTEM_RESTART UNTIL THE FIRST NODE HAS REGISTERED.
// IF SYSTEM IS                           */
// RUNNING THE FIRST NODE TO REGISTER WILL BE ZRUN AND CTYPE_OF_START
// WILL BE CHANGED                           */
// TO NodeState::ST_NODE_RESTART AT PH_2C. WHEN A NodeState::ST_NODE_RESTART IS DETECTED THE NEW NODE
// HAS TO SEND                         */
// A CNTR_MASTERREQ TO THE MASTER
/*---------------------------------------------------------------------------*/
void Ndbcntr::detectNoderestart(Signal* signal) 
{
  NodeRecPtr ownNodePtr;
  ownNodePtr.i = getOwnNodeId();
  ptrCheckGuard(ownNodePtr, MAX_NDB_NODES, nodeRec);
  if (ownNodePtr.p->state != ZADD) {
    if (ownNodePtr.p->state != ZREMOVE) {
      jam();
      return;
    }//if
  }//if
  ctypeOfStart = NodeState::ST_NODE_RESTART;
/*----------------------------------------------*/
/* THIS NODE WILL PERFORM A NODE RESTART        */
/* REQUEST OF ALL NODES STATES IN SYSTEM        */
// The purpose of this signal is to ensure that
// the starting node knows when it has received
// all APPL_CHANGEREP signals and thus can continue
// to the next step of the node restart. Thus we
// need to know the amount of nodes that are in the
// RUN state and in the START state (more than one
// node can be copying data simultaneously in the
// cluster.
/*----------------------------------------------*/
  signal->theData[0] = cownBlockref;
  sendSignal(nodePtr.p->cntrBlockref, GSN_NODE_STATESREQ, signal, 1, JBB);
  cnoNeedNodes = ZNIL;	
/*---------------------------------*/
/* PREVENT TO SEND NODE_STATESREQ  */
/*---------------------------------------------------------------------------*/
/* WE NEED TO WATCH THE NODE RESTART WITH A TIME OUT TO NOT WAIT FOR EVER.   */
/*---------------------------------------------------------------------------*/
  cwaitContinuebFlag = ZTRUE;
  signal->theData[0] = ZCONTINUEB_1;
  sendSignalWithDelay(cownBlockref, GSN_CONTINUEB, signal, 3 * 1000, 1);
}//Ndbcntr::detectNoderestart()

/*---------------------------------------------------------------------------*/
// SCAN NODE_REC FOR APPROPRIATE NODES FOR A START.
// SYSTEMRESTART AND INITALSTART DEMANDS NODES OF STATE ZADD.
// NODERESTART DEMANDS NODE OF THE STATE ZRUN.
// INPUT:  CTYPE_OF_START, NODE_REC
// RETURN: CSTART_NODES(), CNO_START_NODES, CMASTER_CANDIDATE_ID
// (SYSTEMRESTART AND INITALSTART)
/*---------------------------------------------------------------------------*/
void Ndbcntr::getStartNodes(Signal* signal) 
{
  UintR Ttemp1;
  if ((ctypeOfStart == NodeState::ST_NODE_RESTART) ||
      (ctypeOfStart == NodeState::ST_INITIAL_NODE_RESTART)) {
    jam();
    Ttemp1 = ZRUN;
  } else {
    jam();
/*---------------------------------*/
/* SYSTEM RESTART AND INITIAL START*/
/*---------------------------------*/
    Ttemp1 = ZADD;
  }//if
  cnoStartNodes = 0;
  for (nodePtr.i = 1; nodePtr.i < MAX_NDB_NODES; nodePtr.i++) {
    jam();
    ptrAss(nodePtr, nodeRec);
    if (nodePtr.p->state == Ttemp1) {
      jam();
      cstartNodes[cnoStartNodes] = nodePtr.i;/*OVERWRITTEN AT CNTR_MASTERCONF*/
      cnoStartNodes++;
    }//if
  }//for
}//Ndbcntr::getStartNodes()

/*---------------------------------------------------------------------------*/
/*INITIALIZE VARIABLES AND RECORDS                                           */
/*---------------------------------------------------------------------------*/
void Ndbcntr::initData(Signal* signal) 
{
  cmasterNodeId = ZNIL;
  cmasterCandidateId = ZNIL;
  cmasterVoters = 0;
  cstartProgressFlag = ZFALSE;
  capplStartconfFlag = ZFALSE;
  cnoVoters = 0;
  cnoStartNodes = 0;
  for (nodePtr.i = 0; nodePtr.i < MAX_NDB_NODES; nodePtr.i++) {
    ptrAss(nodePtr, nodeRec);
    nodePtr.p->cntrBlockref = calcNdbCntrBlockRef(nodePtr.i);
    nodePtr.p->state = ZREMOVE;
    nodePtr.p->dynamicId = 0;
    nodePtr.p->votes = 0;	/* USED BY MASTER               */
    nodePtr.p->voter = ZFALSE;	/* USED BY MASTER               */
    nodePtr.p->masterReq = ZFALSE;	/* USED BY MASTER               */
  }//for
}//Ndbcntr::initData()

/*---------------------------------------------------------------------------*/
// THE MASTER NODE HAS CHOOSEN THE NODES WHO WERE QUALIFIED TO
// PARTICIPATE IN A INITIALSTART OR SYSTEMRESTART.
// THIS SUBROTINE SENDS A CNTR_MASTERCONF TO THESE NODES
/*---------------------------------------------------------------------------*/
void Ndbcntr::replyMasterconfToAll(Signal* signal) 
{
  if (cnoStartNodes > 1) {
    /**
     * Construct a MasterConf signal
     */
    
    CntrMasterConf * const cntrMasterConf = 
      (CntrMasterConf *)&signal->theData[0];
    NodeBitmask::clear(cntrMasterConf->theNodes);

    cntrMasterConf->noStartNodes = cnoStartNodes;

    for(int i = 0; i<cnoStartNodes; i++)
      NodeBitmask::set(cntrMasterConf->theNodes, cstartNodes[i]);

    /**
     * Then distribute it to everyone but myself
     */
    for(int i = 0; i<cnoStartNodes; i++){
      const NodeId nodeId = cstartNodes[i];
      if(nodeId != getOwnNodeId()){
        sendSignal(numberToRef(number(), nodeId),
		   GSN_CNTR_MASTERCONF,
                   signal, CntrMasterConf::SignalLength, JBB);
      }
    }
  }
}//Ndbcntr::replyMasterconfToAll()

/*---------------------------------------------------------------------------*/
/*RESET VARIABLES USED DURING THE START                                      */
/*---------------------------------------------------------------------------*/
void Ndbcntr::resetStartVariables(Signal* signal) 
{
  for (nodePtr.i = 1; nodePtr.i < MAX_NDB_NODES; nodePtr.i++) {
    ptrAss(nodePtr, nodeRec);
    nodePtr.p->votes = 0;
    nodePtr.p->voter = ZFALSE;
    nodePtr.p->masterReq = ZFALSE;
  }//for
  cnoVoters = 0;
  cnoStartNodes = 0;
  cnoWaitrep6 = cnoWaitrep7 = 0;
}//Ndbcntr::resetStartVariables()

/*---------------------------------------------------------------------------*/
// SENDER OF THIS SIGNAL HAS CHOOSEN A MASTER NODE AND SENDS A REQUEST
// TO THE MASTER_CANDIDATE AS AN VOTE FOR
// THE MASTER. THE SIGNAL ALSO INCLUDES VOTES FOR NODES WHICH SENDER
// THINKS SHOULD PARTICIPATE IN THE START.
// INPUT: CNO_START_NODES
//       CSTART_NODES
/*---------------------------------------------------------------------------*/
void Ndbcntr::sendCntrMasterreq(Signal* signal) 
{
  nodePtr.i = cmasterCandidateId;
  ptrCheckGuard(nodePtr, MAX_NDB_NODES, nodeRec);
/*--------------------------------------------------------------*/
/* O:INITIALSTART, 1:SYSTEMRESTART (ELECTION OF MASTER)         */
/* 2:NODE RESTART (SENDER NODE NOT INCLUDED IN CSTART_NODES)    */
/*--------------------------------------------------------------*/
  CntrMasterReq * const cntrMasterReq = (CntrMasterReq*)&signal->theData[0];
  NodeBitmask::clear(cntrMasterReq->theNodes);
  for (int i = 0; i < cnoStartNodes; i++){
    jam();
    UintR Tnode = cstartNodes[i];
    arrGuard(Tnode, MAX_NDB_NODES);
    NodeBitmask::set(cntrMasterReq->theNodes, Tnode);
  }//for
  cntrMasterReq->userBlockRef = cownBlockref;
  cntrMasterReq->userNodeId = getOwnNodeId();
  cntrMasterReq->typeOfStart = ctypeOfStart;
  cntrMasterReq->noRestartNodes = cnoStartNodes;
  sendSignal(nodePtr.p->cntrBlockref, GSN_CNTR_MASTERREQ,
             signal, CntrMasterReq::SignalLength, JBB);
}//Ndbcntr::sendCntrMasterreq()

/*---------------------------------------------------------------------------*/
// SEND THE SIGNAL
// INPUT                  CNDB_BLOCKS_COUNT
/*---------------------------------------------------------------------------*/
void Ndbcntr::sendNdbSttor(Signal* signal) 
{
  CfgBlockRecPtr cfgBlockPtr;
  NdbBlocksRecPtr ndbBlocksPtr;

  ndbBlocksPtr.i = cndbBlocksCount;
  ptrCheckGuard(ndbBlocksPtr, ZSIZE_NDB_BLOCKS_REC, ndbBlocksRec);
  cfgBlockPtr.i = cinternalStartphase;
  ptrCheckGuard(cfgBlockPtr, ZSIZE_CFG_BLOCK_REC, cfgBlockRec);
  NdbSttor * const req = (NdbSttor*)signal->getDataPtrSend();
  req->senderRef = cownBlockref;
  req->nodeId = getOwnNodeId();
  req->internalStartPhase = cinternalStartphase;
  req->typeOfStart = ctypeOfStart;
  req->masterNodeId = cmasterNodeId;
  
  for (int i = 0; i < 16; i++) {
    req->config[i] = cfgBlockPtr.p->cfgData[i];
  }
  
  //#define TRACE_STTOR
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
  signal->theData[0] = csignalKey;
  signal->theData[1] = 3;
  signal->theData[2] = 2;
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
  if(signal->theData[0] == 13){
    infoEvent("Cntr: cstartPhase = %d, cinternalStartphase = %d, block = %d", 
               cstartPhase, cinternalStartphase, cndbBlocksCount);
    infoEvent("Cntr: cmasterNodeId = %d, cmasterCandidateId = %d", 
               cmasterNodeId, cmasterCandidateId);
  }

  if (dumpState->args[0] == DumpStateOrd::NdbcntrTestStopOnError){
    if (theConfiguration.stopOnError() == true)
      ((Configuration&)theConfiguration).stopOnError(false);
    
    const BlockReference tblockref = calcNdbCntrBlockRef(getOwnNodeId());
      
    SystemError * const sysErr = (SystemError*)&signal->theData[0];
    sysErr->errorCode = SystemError::TestStopOnError;
    sysErr->errorRef = reference();
    sendSignal(tblockref, GSN_SYSTEM_ERROR, signal, 
	       SystemError::SignalLength, JBA);
  }


}//Ndbcntr::execDUMP_STATE_ORD()

void Ndbcntr::execSET_VAR_REQ(Signal* signal) {
  SetVarReq* const setVarReq = (SetVarReq*)&signal->theData[0];
  ConfigParamId var = setVarReq->variable();

  switch (var) {
  case TimeToWaitAlive:
    // Valid only during start so value not set.
    sendSignal(CMVMI_REF, GSN_SET_VAR_CONF, signal, 1, JBB);
    break;

  default:
    sendSignal(CMVMI_REF, GSN_SET_VAR_REF, signal, 1, JBB);
  }// switch
}//Ndbcntr::execSET_VAR_REQ()

void Ndbcntr::updateNodeState(Signal* signal, const NodeState& newState) const{
  NodeStateRep * const stateRep = (NodeStateRep *)&signal->theData[0];

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

  if(getNodeState().startLevel < NodeState::SL_STARTED || 
     abort && !singleuser){
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

  if(c_stopRec.stopReq.senderRef != 0 && !singleuser){
    jam();
    /**
     * Requested a system shutdown
     */
    if(StopReq::getSystemStop(req->requestInfo)){
      jam();
      sendSignalWithDelay(reference(), GSN_STOP_REQ, signal, 100,
			  StopReq::SignalLength);
      return;
    }

    /**
     * Requested a node shutdown
     */
    if(StopReq::getSystemStop(c_stopRec.stopReq.requestInfo))
      ref->errorCode = StopRef::SystemShutdownInProgress;
    else
      ref->errorCode = StopRef::NodeShutdownInProgress;
    ref->senderData = senderData;
    sendSignal(senderRef, GSN_STOP_REF, signal, StopRef::SignalLength, JBB);
    return;
  }
  
  c_stopRec.stopReq = * req;
  c_stopRec.stopInitiatedTime = NdbTick_CurrentMillisecond();
  
  if(StopReq::getSystemStop(c_stopRec.stopReq.requestInfo) && !singleuser) {
    jam();
    if(StopReq::getPerformRestart(c_stopRec.stopReq.requestInfo)){
      ((Configuration&)theConfiguration).stopOnError(false);
    }
  }
  if(!singleuser) {
    if(!c_stopRec.checkNodeFail(signal)){
      jam();
      return;
    }
  }
  NodeState newState(NodeState::SL_STOPPING_1, 
		     StopReq::getSystemStop(c_stopRec.stopReq.requestInfo));
  
   if(singleuser) {
     newState.setSingleUser(true);
     newState.setSingleUserApi(c_stopRec.stopReq.singleUserApi);
   }
  updateNodeState(signal, newState);
  signal->theData[0] = ZSHUTDOWN;
  sendSignalWithDelay(cownBlockref, GSN_CONTINUEB, signal, 100, 1);
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
  NodeBitmask ndbMask; ndbMask.clear();
  NodeRecPtr aPtr;
  for(aPtr.i = 1; aPtr.i < MAX_NDB_NODES; aPtr.i++){
    ptrAss(aPtr, cntr.nodeRec);
    if(aPtr.i != cntr.getOwnNodeId() && aPtr.p->state == ZRUN){
      ndbMask.set(aPtr.i);
    }
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
  
  const BlockReference bref = stopReq.senderRef;
  cntr.sendSignal(bref, GSN_STOP_REF, signal, StopRef::SignalLength, JBB);
  
  stopReq.senderRef = 0;

  NodeState newState(NodeState::SL_STARTED); 

  cntr.updateNodeState(signal, newState);
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
	  req->senderData = 12;
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
  ndbrequire(false);
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
  sendSignalWithDelay(cownBlockref, GSN_CONTINUEB, signal, 100, 1);
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
  req->senderData = 12;
  req->requestType = WaitGCPReq::CompleteForceStart;
  sendSignal(DBDIH_REF, GSN_WAIT_GCP_REQ, signal, 
	     WaitGCPReq::SignalLength, JBB);
}

void Ndbcntr::execWAIT_GCP_CONF(Signal* signal){
  jamEntry();

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

void Ndbcntr::execSTTORRY(Signal* signal){
  jamEntry();
  c_missra.execSTTORRY(signal);
}

void Ndbcntr::execSTART_ORD(Signal* signal){
  jamEntry();
  ndbrequire(NO_OF_BLOCKS == ALL_BLOCKS_SZ);
  c_missra.execSTART_ORD(signal);
}

void
Ndbcntr::clearFilesystem(Signal* signal){
  FsRemoveReq * req  = (FsRemoveReq *)signal->getDataPtrSend();
  req->userReference = reference();
  req->userPointer   = 0;
  req->directory     = 1;
  req->ownDirectory  = 1;
  FsOpenReq::setVersion(req->fileNumber, 3);
  FsOpenReq::setSuffix(req->fileNumber, FsOpenReq::S_CTL); // Can by any...
  FsOpenReq::v1_setDisk(req->fileNumber, c_fsRemoveCount);
  sendSignal(NDBFS_REF, GSN_FSREMOVEREQ, signal, 
             FsRemoveReq::SignalLength, JBA);
  c_fsRemoveCount++;
}

void
Ndbcntr::execFSREMOVEREF(Signal* signal){
  jamEntry();
  ndbrequire(0);
}

void
Ndbcntr::execFSREMOVECONF(Signal* signal){
  jamEntry();
  if(c_fsRemoveCount == 13){
    jam();
    sendSttorry(signal);
  } else {
    jam();
    ndbrequire(c_fsRemoveCount < 13);
    clearFilesystem(signal);
  }//if
}

void Ndbcntr::Missra::execSTART_ORD(Signal* signal){
  signal->theData[0] = EventReport::NDBStartStarted;
  signal->theData[1] = NDB_VERSION;
  cntr.sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 2, JBB);
  
  currentStartPhase = 0;
  for(Uint32 i = 0; i<NO_OF_BLOCKS; i++){
    if(ALL_BLOCKS[i].NextSP < currentStartPhase)
      currentStartPhase = ALL_BLOCKS[i].NextSP;
  }
  
  currentBlockIndex = 0;

  sendNextSTTOR(signal);
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

  for(; currentStartPhase < 255 ; currentStartPhase++){
    jam();
    
    const Uint32 start = currentBlockIndex;
    
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
	
	cntr.sendSignal(ref, GSN_STTOR, 
			signal, 8, JBB);
	return;
      }
    }
    
    currentBlockIndex = 0;

    if(start != 0){
      /**
       * At least one wanted this start phase,  report it
       */
      jam();
      signal->theData[0] = EventReport::StartPhaseCompleted;
      signal->theData[1] = currentStartPhase;
      signal->theData[2] = cntr.ctypeOfStart;    
      cntr.sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 3, JBB);
    }
  }
  
  signal->theData[0] = EventReport::NDBStartCompleted;
  signal->theData[1] = NDB_VERSION;
  cntr.sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 2, JBB);
  
  NodeState newState(NodeState::SL_STARTED);
  cntr.updateNodeState(signal, newState);
}

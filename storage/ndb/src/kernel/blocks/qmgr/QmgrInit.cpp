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

#define JAM_FILE_ID 361


#define DEBUG(x) { ndbout << "Qmgr::" << x << endl; }


void Qmgr::initData() 
{
  creadyDistCom = ZFALSE;

  // Records with constant sizes
  nodeRec = new NodeRec[MAX_NODES];
  for (Uint32 i = 0; i<MAX_NODES; i++)
  {
    nodeRec[i].m_secret = 0;
  }

  c_maxDynamicId = 0;
  c_clusterNodes.clear();
  c_stopReq.senderRef = 0;

  /**
   * Check sanity for NodeVersion
   */
  ndbrequire((Uint32)NodeInfo::DB == 0);
  ndbrequire((Uint32)NodeInfo::API == 1);
  ndbrequire((Uint32)NodeInfo::MGM == 2); 

  m_micro_gcp_enabled = false;
  m_hb_order_config_used = false;

  NodeRecPtr nodePtr;
  nodePtr.i = getOwnNodeId();
  ptrAss(nodePtr, nodeRec);
  nodePtr.p->blockRef = reference();
  ndbrequire(getNodeInfo(getOwnNodeId()).m_type == NodeInfo::DB);

  c_connectedNodes.set(getOwnNodeId());
  setNodeInfo(getOwnNodeId()).m_version = NDB_VERSION;


  /**
   * Timeouts
   */
  const ndb_mgm_configuration_iterator * p = 
    m_ctx.m_config.getOwnConfigIterator();
  ndbrequire(p != 0);
  
  Uint32 hbDBAPI = 1500;
  ndb_mgm_get_int_parameter(p, CFG_DB_API_HEARTBEAT_INTERVAL, &hbDBAPI);
  
  setHbApiDelay(hbDBAPI);

  const NDB_TICKS now = NdbTick_getCurrentTicks(); //OJA bug#17757895
  interface_check_timer.setDelay(1000);
  interface_check_timer.reset(now);

#ifdef ERROR_INSERT
  nodeFailCount = 0;
#endif

  cfailureNr = 1;
  ccommitFailureNr = 1;
  cprepareFailureNr = 1;
  cfailedNodes.clear();
  cprepFailedNodes.clear();
  ccommitFailedNodes.clear();
  creadyDistCom = ZFALSE;
  cpresident = ZNIL;
  c_start.m_president_candidate = ZNIL;
  c_start.m_president_candidate_gci = 0;
  cpdistref = 0;
  cneighbourh = ZNIL;
  cneighbourl = ZNIL;
  cdelayRegreq = ZDELAY_REGREQ;
  c_allow_api_connect = 0;
  ctoStatus = Q_NOT_ACTIVE;

  for (nodePtr.i = 1; nodePtr.i < MAX_NODES; nodePtr.i++)
  {
    ptrAss(nodePtr, nodeRec);
    nodePtr.p->ndynamicId = 0;
    nodePtr.p->hbOrder = 0;
    Uint32 cnt = 0;
    Uint32 type = getNodeInfo(nodePtr.i).m_type;
    switch(type){
    case NodeInfo::DB:
      jam();
      nodePtr.p->phase = ZINIT;
      c_definedNodes.set(nodePtr.i);
      break;
    case NodeInfo::API:
      jam();
      nodePtr.p->phase = ZAPI_INACTIVE;
      break;
    case NodeInfo::MGM:
      jam();
      /**
       * cmvmi allows ndb_mgmd to connect directly
       */
      nodePtr.p->phase = ZAPI_INACTIVE;
      break;
    default:
      jam();
      nodePtr.p->phase = ZAPI_INACTIVE;
    }

    set_hb_count(nodePtr.i) = cnt;
    nodePtr.p->sendPrepFailReqStatus = Q_NOT_ACTIVE;
    nodePtr.p->sendCommitFailReqStatus = Q_NOT_ACTIVE;
    nodePtr.p->sendPresToStatus = Q_NOT_ACTIVE;
    nodePtr.p->failState = NORMAL;
  }//for

  /* Received ProcessInfo are indirectly addressed:
     nodeId => fixed array lookup => dynamic array.
     The dynamic array contains enough entries for all
     configured MGM and API nodes.
  */
  int numOfApiAndMgmNodes = 0;
  for (int i = 1; i < MAX_NODES; i++)
  {
    Uint32 type = getNodeInfo(i).m_type;
    switch(type){
    case NodeInfo::API:
    case NodeInfo::MGM:
      processInfoNodeIndex[i] = numOfApiAndMgmNodes++;
      max_api_node_id = i;
      break;
    default:
      processInfoNodeIndex[i] = -1;
      break;
    }
  }
  receivedProcessInfo = new ProcessInfo[numOfApiAndMgmNodes];
}//Qmgr::initData()

void Qmgr::initRecords()
{
  // Records with dynamic sizes
}//Qmgr::initRecords()

Qmgr::Qmgr(Block_context& ctx)
  : SimulatedBlock(QMGR, ctx)
{
  BLOCK_CONSTRUCTOR(Qmgr);

  // Transit signals
  addRecSignal(GSN_DUMP_STATE_ORD, &Qmgr::execDUMP_STATE_ORD);
  addRecSignal(GSN_STOP_REQ, &Qmgr::execSTOP_REQ);
  addRecSignal(GSN_DEBUG_SIG, &Qmgr::execDEBUG_SIG);
  addRecSignal(GSN_CONTINUEB, &Qmgr::execCONTINUEB);
  addRecSignal(GSN_CM_HEARTBEAT, &Qmgr::execCM_HEARTBEAT);
  addRecSignal(GSN_CM_ADD, &Qmgr::execCM_ADD);
  addRecSignal(GSN_CM_ACKADD, &Qmgr::execCM_ACKADD);
  addRecSignal(GSN_CM_REGREQ, &Qmgr::execCM_REGREQ);
  addRecSignal(GSN_CM_REGCONF, &Qmgr::execCM_REGCONF);
  addRecSignal(GSN_CM_REGREF, &Qmgr::execCM_REGREF);
  addRecSignal(GSN_CM_NODEINFOREQ, &Qmgr::execCM_NODEINFOREQ);
  addRecSignal(GSN_CM_NODEINFOCONF, &Qmgr::execCM_NODEINFOCONF);
  addRecSignal(GSN_CM_NODEINFOREF, &Qmgr::execCM_NODEINFOREF);
  addRecSignal(GSN_PREP_FAILREQ, &Qmgr::execPREP_FAILREQ);
  addRecSignal(GSN_PREP_FAILCONF, &Qmgr::execPREP_FAILCONF);
  addRecSignal(GSN_PREP_FAILREF, &Qmgr::execPREP_FAILREF);
  addRecSignal(GSN_COMMIT_FAILREQ, &Qmgr::execCOMMIT_FAILREQ);
  addRecSignal(GSN_COMMIT_FAILCONF, &Qmgr::execCOMMIT_FAILCONF);
  addRecSignal(GSN_FAIL_REP, &Qmgr::execFAIL_REP);
  addRecSignal(GSN_PRES_TOREQ, &Qmgr::execPRES_TOREQ);
  addRecSignal(GSN_PRES_TOCONF, &Qmgr::execPRES_TOCONF);

  // Received signals
  addRecSignal(GSN_CONNECT_REP, &Qmgr::execCONNECT_REP);
  addRecSignal(GSN_NDB_FAILCONF, &Qmgr::execNDB_FAILCONF);
  addRecSignal(GSN_NF_COMPLETEREP, &Qmgr::execNF_COMPLETEREP);
  addRecSignal(GSN_READ_CONFIG_REQ, &Qmgr::execREAD_CONFIG_REQ);
  addRecSignal(GSN_STTOR, &Qmgr::execSTTOR);
  addRecSignal(GSN_CLOSE_COMCONF, &Qmgr::execCLOSE_COMCONF);
  addRecSignal(GSN_API_REGREQ, &Qmgr::execAPI_REGREQ);
  addRecSignal(GSN_API_VERSION_REQ, &Qmgr::execAPI_VERSION_REQ);
  addRecSignal(GSN_DISCONNECT_REP, &Qmgr::execDISCONNECT_REP);
  addRecSignal(GSN_API_FAILREQ, &Qmgr::execAPI_FAILREQ);
  addRecSignal(GSN_API_FAILCONF, &Qmgr::execAPI_FAILCONF);
  addRecSignal(GSN_READ_NODESREQ, &Qmgr::execREAD_NODESREQ);
  addRecSignal(GSN_API_BROADCAST_REP,  &Qmgr::execAPI_BROADCAST_REP);

  addRecSignal(GSN_NODE_FAILREP, &Qmgr::execNODE_FAILREP);
  addRecSignal(GSN_ALLOC_NODEID_REQ,  &Qmgr::execALLOC_NODEID_REQ);
  addRecSignal(GSN_ALLOC_NODEID_CONF,  &Qmgr::execALLOC_NODEID_CONF);
  addRecSignal(GSN_ALLOC_NODEID_REF,  &Qmgr::execALLOC_NODEID_REF);
  addRecSignal(GSN_ENABLE_COMCONF,  &Qmgr::execENABLE_COMCONF);
  addRecSignal(GSN_PROCESSINFO_REP, &Qmgr::execPROCESSINFO_REP);

  // Arbitration signals
  addRecSignal(GSN_ARBIT_PREPREQ, &Qmgr::execARBIT_PREPREQ);
  addRecSignal(GSN_ARBIT_PREPCONF, &Qmgr::execARBIT_PREPCONF);
  addRecSignal(GSN_ARBIT_PREPREF, &Qmgr::execARBIT_PREPREF);
  addRecSignal(GSN_ARBIT_STARTCONF, &Qmgr::execARBIT_STARTCONF);
  addRecSignal(GSN_ARBIT_STARTREF, &Qmgr::execARBIT_STARTREF);
  addRecSignal(GSN_ARBIT_CHOOSECONF, &Qmgr::execARBIT_CHOOSECONF);
  addRecSignal(GSN_ARBIT_CHOOSEREF, &Qmgr::execARBIT_CHOOSEREF);
  addRecSignal(GSN_ARBIT_STOPREP, &Qmgr::execARBIT_STOPREP);

  addRecSignal(GSN_READ_NODESREF, &Qmgr::execREAD_NODESREF);
  addRecSignal(GSN_READ_NODESCONF, &Qmgr::execREAD_NODESCONF);

  addRecSignal(GSN_DIH_RESTARTREF, &Qmgr::execDIH_RESTARTREF);
  addRecSignal(GSN_DIH_RESTARTCONF, &Qmgr::execDIH_RESTARTCONF);
  addRecSignal(GSN_NODE_VERSION_REP, &Qmgr::execNODE_VERSION_REP);
  addRecSignal(GSN_START_ORD, &Qmgr::execSTART_ORD);

  addRecSignal(GSN_UPGRADE_PROTOCOL_ORD, &Qmgr::execUPGRADE_PROTOCOL_ORD);
  
  // Connectivity check signals
  addRecSignal(GSN_NODE_PING_REQ, &Qmgr::execNODE_PINGREQ);
  addRecSignal(GSN_NODE_PING_CONF, &Qmgr::execNODE_PINGCONF);

  // Ndbinfo signal
  addRecSignal(GSN_DBINFO_SCANREQ, &Qmgr::execDBINFO_SCANREQ);

  // Message from NDBCNTR when our node is set to state STARTED
  addRecSignal(GSN_NODE_STARTED_REP, &Qmgr::execNODE_STARTED_REP);

  // Message from other blocks requesting node isolation
  addRecSignal(GSN_ISOLATE_ORD, &Qmgr::execISOLATE_ORD);

  addRecSignal(GSN_READ_LOCAL_SYSFILE_CONF,
               &Qmgr::execREAD_LOCAL_SYSFILE_CONF);

  addRecSignal(GSN_NODE_STATE_REP,
               &Qmgr::execNODE_STATE_REP, true); // Override

  initData();
}//Qmgr::Qmgr()

Qmgr::~Qmgr() 
{
  delete []nodeRec;
  delete []receivedProcessInfo;
}//Qmgr::~Qmgr()


BLOCK_FUNCTIONS(Qmgr)

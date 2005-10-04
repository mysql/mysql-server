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

#define DEBUG(x) { ndbout << "Qmgr::" << x << endl; }


void Qmgr::initData() 
{
  creadyDistCom = ZFALSE;

  // Records with constant sizes
  nodeRec = new NodeRec[MAX_NODES];

  cnoCommitFailedNodes = 0;
  c_maxDynamicId = 0;
  c_clusterNodes.clear();

  Uint32 hbDBAPI = 500;
  setHbApiDelay(hbDBAPI);

  c_connectedNodes.clear();
  c_connectedNodes.set(getOwnNodeId());
}//Qmgr::initData()

void Qmgr::initRecords() 
{
  // Records with dynamic sizes
}//Qmgr::initRecords()

Qmgr::Qmgr(const class Configuration & conf)
  : SimulatedBlock(QMGR, conf)
{
  BLOCK_CONSTRUCTOR(Qmgr);

  // Transit signals
  addRecSignal(GSN_DUMP_STATE_ORD, &Qmgr::execDUMP_STATE_ORD);
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
  addRecSignal(GSN_READ_CONFIG_REQ, &Qmgr::execREAD_CONFIG_REQ);
  addRecSignal(GSN_STTOR, &Qmgr::execSTTOR);
  addRecSignal(GSN_CLOSE_COMCONF, &Qmgr::execCLOSE_COMCONF);
  addRecSignal(GSN_API_REGREQ, &Qmgr::execAPI_REGREQ);
  addRecSignal(GSN_API_VERSION_REQ, &Qmgr::execAPI_VERSION_REQ);
  addRecSignal(GSN_DISCONNECT_REP, &Qmgr::execDISCONNECT_REP);
  addRecSignal(GSN_API_FAILCONF, &Qmgr::execAPI_FAILCONF);
  addRecSignal(GSN_READ_NODESREQ, &Qmgr::execREAD_NODESREQ);
  addRecSignal(GSN_SET_VAR_REQ,  &Qmgr::execSET_VAR_REQ);
  addRecSignal(GSN_API_BROADCAST_REP,  &Qmgr::execAPI_BROADCAST_REP);
  
  // Arbitration signals
  addRecSignal(GSN_ARBIT_PREPREQ, &Qmgr::execARBIT_PREPREQ);
  addRecSignal(GSN_ARBIT_PREPCONF, &Qmgr::execARBIT_PREPCONF);
  addRecSignal(GSN_ARBIT_PREPREF, &Qmgr::execARBIT_PREPREF);
  addRecSignal(GSN_ARBIT_STARTCONF, &Qmgr::execARBIT_STARTCONF);
  addRecSignal(GSN_ARBIT_STARTREF, &Qmgr::execARBIT_STARTREF);
  addRecSignal(GSN_ARBIT_CHOOSECONF, &Qmgr::execARBIT_CHOOSECONF);
  addRecSignal(GSN_ARBIT_CHOOSEREF, &Qmgr::execARBIT_CHOOSEREF);
  addRecSignal(GSN_ARBIT_STOPREP, &Qmgr::execARBIT_STOPREP);

  initData();
}//Qmgr::Qmgr()

Qmgr::~Qmgr() 
{
  delete []nodeRec;
}//Qmgr::~Qmgr()


BLOCK_FUNCTIONS(Qmgr)

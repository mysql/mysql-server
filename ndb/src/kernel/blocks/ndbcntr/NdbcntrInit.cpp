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

#define DEBUG(x) { ndbout << "Ndbcntr::" << x << endl; }


void Ndbcntr::initData() 
{

  // Records with constant sizes
  ndbBlocksRec = new NdbBlocksRec[ZSIZE_NDB_BLOCKS_REC];
}//Ndbcntr::initData()

void Ndbcntr::initRecords() 
{
  // Records with dynamic sizes
}//Ndbcntr::initRecords()

Ndbcntr::Ndbcntr(const class Configuration & conf):
  SimulatedBlock(NDBCNTR, conf),
  cnoWaitrep6(0),
  cnoWaitrep7(0),
  c_stopRec(* this),
  c_missra(* this)
{

  BLOCK_CONSTRUCTOR(Ndbcntr);

  // Transit signals
  addRecSignal(GSN_CONTINUEB, &Ndbcntr::execCONTINUEB);
  addRecSignal(GSN_READ_NODESCONF, &Ndbcntr::execREAD_NODESCONF);
  addRecSignal(GSN_READ_NODESREF, &Ndbcntr::execREAD_NODESREF);
  addRecSignal(GSN_CM_ADD_REP, &Ndbcntr::execCM_ADD_REP);
  addRecSignal(GSN_CNTR_START_REQ, &Ndbcntr::execCNTR_START_REQ);
  addRecSignal(GSN_CNTR_START_REF, &Ndbcntr::execCNTR_START_REF);
  addRecSignal(GSN_CNTR_START_CONF, &Ndbcntr::execCNTR_START_CONF);
  addRecSignal(GSN_CNTR_WAITREP, &Ndbcntr::execCNTR_WAITREP);
  addRecSignal(GSN_CNTR_START_REP, &Ndbcntr::execCNTR_START_REP);
  addRecSignal(GSN_NODE_FAILREP, &Ndbcntr::execNODE_FAILREP);
  addRecSignal(GSN_SYSTEM_ERROR , &Ndbcntr::execSYSTEM_ERROR);
  
  // Received signals
  addRecSignal(GSN_DUMP_STATE_ORD, &Ndbcntr::execDUMP_STATE_ORD);
  addRecSignal(GSN_STTOR, &Ndbcntr::execSTTOR);
  addRecSignal(GSN_TCSEIZECONF, &Ndbcntr::execTCSEIZECONF);
  addRecSignal(GSN_TCSEIZEREF, &Ndbcntr::execTCSEIZEREF);
  addRecSignal(GSN_TCRELEASECONF, &Ndbcntr::execTCRELEASECONF);
  addRecSignal(GSN_TCRELEASEREF, &Ndbcntr::execTCRELEASEREF);
  addRecSignal(GSN_TCKEYCONF, &Ndbcntr::execTCKEYCONF);
  addRecSignal(GSN_TCKEYREF, &Ndbcntr::execTCKEYREF);
  addRecSignal(GSN_TCROLLBACKREP, &Ndbcntr::execTCROLLBACKREP);
  addRecSignal(GSN_GETGCICONF, &Ndbcntr::execGETGCICONF);
  addRecSignal(GSN_DIH_RESTARTCONF, &Ndbcntr::execDIH_RESTARTCONF);
  addRecSignal(GSN_DIH_RESTARTREF, &Ndbcntr::execDIH_RESTARTREF);
  addRecSignal(GSN_CREATE_TABLE_REF, &Ndbcntr::execCREATE_TABLE_REF);
  addRecSignal(GSN_CREATE_TABLE_CONF, &Ndbcntr::execCREATE_TABLE_CONF);
  addRecSignal(GSN_NDB_STTORRY, &Ndbcntr::execNDB_STTORRY);
  addRecSignal(GSN_NDB_STARTCONF, &Ndbcntr::execNDB_STARTCONF);
  addRecSignal(GSN_READ_NODESREQ, &Ndbcntr::execREAD_NODESREQ);
  addRecSignal(GSN_NDB_STARTREF, &Ndbcntr::execNDB_STARTREF);
  addRecSignal(GSN_SET_VAR_REQ, &Ndbcntr::execSET_VAR_REQ);

  addRecSignal(GSN_STOP_PERM_REF, &Ndbcntr::execSTOP_PERM_REF);
  addRecSignal(GSN_STOP_PERM_CONF, &Ndbcntr::execSTOP_PERM_CONF);

  addRecSignal(GSN_STOP_ME_REF, &Ndbcntr::execSTOP_ME_REF);
  addRecSignal(GSN_STOP_ME_CONF, &Ndbcntr::execSTOP_ME_CONF);

  addRecSignal(GSN_STOP_REQ, &Ndbcntr::execSTOP_REQ);
  addRecSignal(GSN_RESUME_REQ, &Ndbcntr::execRESUME_REQ);

  addRecSignal(GSN_WAIT_GCP_REF, &Ndbcntr::execWAIT_GCP_REF);
  addRecSignal(GSN_WAIT_GCP_CONF, &Ndbcntr::execWAIT_GCP_CONF);
  addRecSignal(GSN_CHANGE_NODE_STATE_CONF, 
	       &Ndbcntr::execCHANGE_NODE_STATE_CONF);

  addRecSignal(GSN_ABORT_ALL_REF, &Ndbcntr::execABORT_ALL_REF);
  addRecSignal(GSN_ABORT_ALL_CONF, &Ndbcntr::execABORT_ALL_CONF);

  addRecSignal(GSN_START_ORD, &Ndbcntr::execSTART_ORD);
  addRecSignal(GSN_STTORRY, &Ndbcntr::execSTTORRY);
  addRecSignal(GSN_READ_CONFIG_CONF, &Ndbcntr::execREAD_CONFIG_CONF);

  addRecSignal(GSN_FSREMOVEREF, &Ndbcntr::execFSREMOVEREF);
  addRecSignal(GSN_FSREMOVECONF, &Ndbcntr::execFSREMOVECONF);
  
  initData();
  ctypeOfStart = NodeState::ST_ILLEGAL_TYPE;
  c_start.m_startTime = NdbTick_CurrentMillisecond();
}//Ndbcntr::Ndbcntr()

Ndbcntr::~Ndbcntr() 
{
  delete []ndbBlocksRec;

}//Ndbcntr::~Ndbcntr()

BLOCK_FUNCTIONS(Ndbcntr);

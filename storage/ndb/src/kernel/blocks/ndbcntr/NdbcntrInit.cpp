/*
   Copyright (c) 2003, 2013, Oracle and/or its affiliates. All rights reserved.

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



#define NDBCNTR_C
#include "Ndbcntr.hpp"
#include <ndb_limits.h>

#define JAM_FILE_ID 459


#define DEBUG(x) { ndbout << "Ndbcntr::" << x << endl; }


void Ndbcntr::initData() 
{
  c_start.reset();
  cmasterNodeId = 0;
  cnoStartNodes = 0;
  cnoWaitrep = 0;
  // Records with constant sizes
  ndbBlocksRec = new NdbBlocksRec[ZSIZE_NDB_BLOCKS_REC];
  // schema trans
  c_schemaTransId = 0;
  c_schemaTransKey = 0;
}//Ndbcntr::initData()

void Ndbcntr::initRecords() 
{
  // Records with dynamic sizes
}//Ndbcntr::initRecords()

Ndbcntr::Ndbcntr(Block_context& ctx):
  SimulatedBlock(NDBCNTR, ctx),
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
  addRecSignal(GSN_API_START_REP, &Ndbcntr::execAPI_START_REP, true);
  addRecSignal(GSN_NODE_FAILREP, &Ndbcntr::execNODE_FAILREP);
  addRecSignal(GSN_SYSTEM_ERROR , &Ndbcntr::execSYSTEM_ERROR);
  addRecSignal(GSN_START_PERMREP, &Ndbcntr::execSTART_PERMREP);
  
  // Received signals
  addRecSignal(GSN_DUMP_STATE_ORD, &Ndbcntr::execDUMP_STATE_ORD);
  addRecSignal(GSN_READ_CONFIG_REQ, &Ndbcntr::execREAD_CONFIG_REQ);
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
  addRecSignal(GSN_SCHEMA_TRANS_BEGIN_CONF, &Ndbcntr::execSCHEMA_TRANS_BEGIN_CONF);
  addRecSignal(GSN_SCHEMA_TRANS_BEGIN_REF, &Ndbcntr::execSCHEMA_TRANS_BEGIN_REF);
  addRecSignal(GSN_SCHEMA_TRANS_END_CONF, &Ndbcntr::execSCHEMA_TRANS_END_CONF);
  addRecSignal(GSN_SCHEMA_TRANS_END_REF, &Ndbcntr::execSCHEMA_TRANS_END_REF);
  addRecSignal(GSN_CREATE_TABLE_REF, &Ndbcntr::execCREATE_TABLE_REF);
  addRecSignal(GSN_CREATE_TABLE_CONF, &Ndbcntr::execCREATE_TABLE_CONF);
  addRecSignal(GSN_CREATE_HASH_MAP_REF, &Ndbcntr::execCREATE_HASH_MAP_REF);
  addRecSignal(GSN_CREATE_HASH_MAP_CONF, &Ndbcntr::execCREATE_HASH_MAP_CONF);
  addRecSignal(GSN_CREATE_FILEGROUP_REF, &Ndbcntr::execCREATE_FILEGROUP_REF);
  addRecSignal(GSN_CREATE_FILEGROUP_CONF, &Ndbcntr::execCREATE_FILEGROUP_CONF);
  addRecSignal(GSN_CREATE_FILE_REF, &Ndbcntr::execCREATE_FILE_REF);
  addRecSignal(GSN_CREATE_FILE_CONF, &Ndbcntr::execCREATE_FILE_CONF);
  addRecSignal(GSN_NDB_STTORRY, &Ndbcntr::execNDB_STTORRY);
  addRecSignal(GSN_NDB_STARTCONF, &Ndbcntr::execNDB_STARTCONF);
  addRecSignal(GSN_READ_NODESREQ, &Ndbcntr::execREAD_NODESREQ);
  addRecSignal(GSN_NDB_STARTREF, &Ndbcntr::execNDB_STARTREF);

  addRecSignal(GSN_STOP_PERM_REF, &Ndbcntr::execSTOP_PERM_REF);
  addRecSignal(GSN_STOP_PERM_CONF, &Ndbcntr::execSTOP_PERM_CONF);

  addRecSignal(GSN_STOP_ME_REF, &Ndbcntr::execSTOP_ME_REF);
  addRecSignal(GSN_STOP_ME_CONF, &Ndbcntr::execSTOP_ME_CONF);

  addRecSignal(GSN_STOP_REQ, &Ndbcntr::execSTOP_REQ);
  addRecSignal(GSN_STOP_CONF, &Ndbcntr::execSTOP_CONF);
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

  addRecSignal(GSN_FSREMOVECONF, &Ndbcntr::execFSREMOVECONF);

  addRecSignal(GSN_START_COPYREF, &Ndbcntr::execSTART_COPYREF);
  addRecSignal(GSN_START_COPYCONF, &Ndbcntr::execSTART_COPYCONF);

  addRecSignal(GSN_CREATE_NODEGROUP_IMPL_REQ, &Ndbcntr::execCREATE_NODEGROUP_IMPL_REQ);
  addRecSignal(GSN_DROP_NODEGROUP_IMPL_REQ, &Ndbcntr::execDROP_NODEGROUP_IMPL_REQ);
  
  initData();
  ctypeOfStart = NodeState::ST_ILLEGAL_TYPE;
  c_start.m_startTime = NdbTick_getCurrentTicks();
  m_cntr_start_conf = false;
}//Ndbcntr::Ndbcntr()

Ndbcntr::~Ndbcntr() 
{
  delete []ndbBlocksRec;

}//Ndbcntr::~Ndbcntr()

BLOCK_FUNCTIONS(Ndbcntr)

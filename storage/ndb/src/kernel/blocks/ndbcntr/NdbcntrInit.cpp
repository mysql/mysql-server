/*
   Copyright (c) 2003, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#define NDBCNTR_C
#include <ndb_limits.h>
#include "Ndbcntr.hpp"

#define JAM_FILE_ID 459

#define DEBUG(x) \
  { ndbout << "Ndbcntr::" << x << endl; }

void Ndbcntr::initData() {
  c_cntr_startedNodeSet.clear();
  c_startedNodeSet.clear();
  c_start.reset();
  cmasterNodeId = 0;
  cnoStartNodes = 0;
  cnoWaitrep = 0;
  // Records with constant sizes
  ndbBlocksRec = new NdbBlocksRec[ZSIZE_NDB_BLOCKS_REC];
  // schema trans
  c_schemaTransId = 0;
  c_schemaTransKey = 0;

  alloc_local_bat();
  init_secretsfile();
  init_local_sysfile();

  m_any_lcp_started = false;
  m_distributed_lcp_id = 0;
  m_outstanding_wait_lcp = 0;
  m_outstanding_wait_cut_redo_log_tail = 0;
  m_set_local_lcp_id_reqs = 0;
  m_received_wait_all = false;
  m_wait_cut_undo_log_tail = false;
  m_local_lcp_started = false;
  m_local_lcp_completed = false;
  m_full_local_lcp_started = false;
  m_first_distributed_lcp_started = false;
  m_distributed_lcp_started = false;
  m_copy_fragment_in_progress = false;
  m_max_gci_in_lcp = 0;
  m_max_keep_gci = 0;
  m_ready_to_cut_log_tail = false;
  /**
   * During initial start of Cluster we are executing an LCP before
   * we have started the GCP protocol. The first GCI is 2, so to
   * ensure that first LCP can complete we set m_max_completed_gci
   * to 2 from the start although it isn't really completed yet.
   */
  m_max_completed_gci = 2;
  m_initial_local_lcp_started = false;
  m_lcp_id = 0;
  m_local_lcp_id = 0;
  m_global_redo_alert_state = RedoStateRep::NO_REDO_ALERT;
  m_node_redo_alert_state = RedoStateRep::NO_REDO_ALERT;
  for (Uint32 i = 0; i < MAX_NDBMT_LQH_THREADS; i++)
    m_redo_alert_state[i] = RedoStateRep::NO_REDO_ALERT;
}  // Ndbcntr::initData()

void Ndbcntr::initRecords() {
  // Records with dynamic sizes
}  // Ndbcntr::initRecords()

Ndbcntr::Ndbcntr(Block_context &ctx)
    : SimulatedBlock(NDBCNTR, ctx),
      cnoWaitrep6(0),
      cnoWaitrep7(0),
      c_stopRec(*this),
      c_missra(*this) {
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
  addRecSignal(GSN_SYSTEM_ERROR, &Ndbcntr::execSYSTEM_ERROR);
  addRecSignal(GSN_START_PERMREP, &Ndbcntr::execSTART_PERMREP);

  // Received signals
  addRecSignal(GSN_DUMP_STATE_ORD, &Ndbcntr::execDUMP_STATE_ORD);
  addRecSignal(GSN_READ_CONFIG_REQ, &Ndbcntr::execREAD_CONFIG_REQ);
  addRecSignal(GSN_STTOR, &Ndbcntr::execSTTOR);
  addRecSignal(GSN_GETGCICONF, &Ndbcntr::execGETGCICONF);
  addRecSignal(GSN_DIH_RESTARTCONF, &Ndbcntr::execDIH_RESTARTCONF);
  addRecSignal(GSN_DIH_RESTARTREF, &Ndbcntr::execDIH_RESTARTREF);
  addRecSignal(GSN_SET_UP_MULTI_TRP_CONF, &Ndbcntr::execSET_UP_MULTI_TRP_CONF);
  addRecSignal(GSN_SCHEMA_TRANS_BEGIN_CONF,
               &Ndbcntr::execSCHEMA_TRANS_BEGIN_CONF);
  addRecSignal(GSN_SCHEMA_TRANS_BEGIN_REF,
               &Ndbcntr::execSCHEMA_TRANS_BEGIN_REF);
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

  addRecSignal(GSN_REDO_STATE_REP, &Ndbcntr::execREDO_STATE_REP);

  addRecSignal(GSN_ABORT_ALL_REF, &Ndbcntr::execABORT_ALL_REF);
  addRecSignal(GSN_ABORT_ALL_CONF, &Ndbcntr::execABORT_ALL_CONF);

  addRecSignal(GSN_START_ORD, &Ndbcntr::execSTART_ORD);
  addRecSignal(GSN_STTORRY, &Ndbcntr::execSTTORRY);
  addRecSignal(GSN_READ_CONFIG_CONF, &Ndbcntr::execREAD_CONFIG_CONF);

  addRecSignal(GSN_FSREMOVECONF, &Ndbcntr::execFSREMOVECONF);

  addRecSignal(GSN_START_COPYREF, &Ndbcntr::execSTART_COPYREF);
  addRecSignal(GSN_START_COPYCONF, &Ndbcntr::execSTART_COPYCONF);

  addRecSignal(GSN_WAIT_ALL_COMPLETE_LCP_REQ,
               &Ndbcntr::execWAIT_ALL_COMPLETE_LCP_REQ);
  addRecSignal(GSN_WAIT_COMPLETE_LCP_CONF,
               &Ndbcntr::execWAIT_COMPLETE_LCP_CONF);
  addRecSignal(GSN_START_LOCAL_LCP_ORD, &Ndbcntr::execSTART_LOCAL_LCP_ORD);
  addRecSignal(GSN_START_DISTRIBUTED_LCP_ORD,
               &Ndbcntr::execSTART_DISTRIBUTED_LCP_ORD);
  addRecSignal(GSN_COPY_FRAG_IN_PROGRESS_REP,
               &Ndbcntr::execCOPY_FRAG_IN_PROGRESS_REP);
  addRecSignal(GSN_COPY_FRAG_NOT_IN_PROGRESS_REP,
               &Ndbcntr::execCOPY_FRAG_NOT_IN_PROGRESS_REP);
  addRecSignal(GSN_LCP_ALL_COMPLETE_REQ, &Ndbcntr::execLCP_ALL_COMPLETE_REQ);
  addRecSignal(GSN_CUT_UNDO_LOG_TAIL_CONF,
               &Ndbcntr::execCUT_UNDO_LOG_TAIL_CONF);
  addRecSignal(GSN_CUT_REDO_LOG_TAIL_CONF,
               &Ndbcntr::execCUT_REDO_LOG_TAIL_CONF);
  addRecSignal(GSN_RESTORABLE_GCI_REP, &Ndbcntr::execRESTORABLE_GCI_REP);
  addRecSignal(GSN_UNDO_LOG_LEVEL_REP, &Ndbcntr::execUNDO_LOG_LEVEL_REP);
  addRecSignal(GSN_SET_LOCAL_LCP_ID_REQ, &Ndbcntr::execSET_LOCAL_LCP_ID_REQ);

  addRecSignal(GSN_CREATE_NODEGROUP_IMPL_REQ,
               &Ndbcntr::execCREATE_NODEGROUP_IMPL_REQ);
  addRecSignal(GSN_DROP_NODEGROUP_IMPL_REQ,
               &Ndbcntr::execDROP_NODEGROUP_IMPL_REQ);
  addRecSignal(GSN_READ_LOCAL_SYSFILE_REQ,
               &Ndbcntr::execREAD_LOCAL_SYSFILE_REQ);
  addRecSignal(GSN_READ_LOCAL_SYSFILE_CONF,
               &Ndbcntr::execREAD_LOCAL_SYSFILE_CONF);
  addRecSignal(GSN_WRITE_LOCAL_SYSFILE_REQ,
               &Ndbcntr::execWRITE_LOCAL_SYSFILE_REQ);
  addRecSignal(GSN_WRITE_LOCAL_SYSFILE_CONF,
               &Ndbcntr::execWRITE_LOCAL_SYSFILE_CONF);
  addRecSignal(GSN_FSOPENREF, &Ndbcntr::execFSOPENREF, true);
  addRecSignal(GSN_FSOPENCONF, &Ndbcntr::execFSOPENCONF);
  addRecSignal(GSN_FSREADREF, &Ndbcntr::execFSREADREF, true);
  addRecSignal(GSN_FSREADCONF, &Ndbcntr::execFSREADCONF);
  addRecSignal(GSN_FSWRITEREF, &Ndbcntr::execFSWRITEREF, true);
  addRecSignal(GSN_FSWRITECONF, &Ndbcntr::execFSWRITECONF);
  addRecSignal(GSN_FSCLOSEREF, &Ndbcntr::execFSCLOSEREF, true);
  addRecSignal(GSN_FSCLOSECONF, &Ndbcntr::execFSCLOSECONF);
  addRecSignal(GSN_FSAPPENDREF, &Ndbcntr::execFSAPPENDREF, true);
  addRecSignal(GSN_FSAPPENDCONF, &Ndbcntr::execFSAPPENDCONF);

  initData();
  ctypeOfStart = NodeState::ST_ILLEGAL_TYPE;
  c_start.m_startTime = NdbTick_getCurrentTicks();
  m_cntr_start_conf = false;
}  // Ndbcntr::Ndbcntr()

Ndbcntr::~Ndbcntr() { delete[] ndbBlocksRec; }  // Ndbcntr::~Ndbcntr()

BLOCK_FUNCTIONS(Ndbcntr)

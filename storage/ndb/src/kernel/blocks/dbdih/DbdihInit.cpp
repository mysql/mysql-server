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


#define DBDIH_C
#include "Dbdih.hpp"
#include <ndb_limits.h>

#define JAM_FILE_ID 355


#define DEBUG(x) { ndbout << "DIH::" << x << endl; }

void Dbdih::initData() 
{
  cpageFileSize = ZPAGEREC;

  // Records with constant sizes
  createReplicaRecord = (CreateReplicaRecord*)
    allocRecord("CreateReplicaRecord", sizeof(CreateReplicaRecord),
                 ZCREATE_REPLICA_FILE_SIZE);

  nodeGroupRecord = (NodeGroupRecord*)
    allocRecord("NodeGroupRecord",
                sizeof(NodeGroupRecord),
                MAX_NDB_NODE_GROUPS);

  nodeRecord = (NodeRecord*)
    allocRecord("NodeRecord", sizeof(NodeRecord), MAX_NDB_NODES);

  Uint32 i;
  for(i = 0; i<MAX_NDB_NODES; i++){
    new (&nodeRecord[i]) NodeRecord();
  }
  Uint32 max_takeover_threads = MAX(MAX_NDB_NODES,
                                    ZMAX_TAKE_OVER_THREADS);
  c_takeOverPool.setSize(max_takeover_threads);
  {
    Ptr<TakeOverRecord> ptr;
    while (c_masterActiveTakeOverList.seizeFirst(ptr))
    {
      new (ptr.p) TakeOverRecord;
    }
    while (c_masterActiveTakeOverList.first(ptr))
    {
      releaseTakeOver(ptr, true, true);
    }
  }
  
  waitGCPProxyPool.setSize(ZPROXY_FILE_SIZE);
  waitGCPMasterPool.setSize(ZPROXY_MASTER_FILE_SIZE);

  c_dictLockSlavePool.setSize(1); // assert single usage
  c_dictLockSlavePtrI_nodeRestart = RNIL;

  cgcpOrderBlocked = 0;
  c_lcpState.ctcCounter = 0;
  c_lcpState.m_lcp_trylock_timeout = 0;
  cwaitLcpSr       = false;
  c_blockCommit    = false;
  c_blockCommitNo  = 1;
  cntrlblockref    = RNIL;
  c_set_initial_start_flag = FALSE;
  c_sr_wait_to = false;
  c_2pass_inr = false;
  c_handled_master_take_over_copy_gci = 0;
  c_start_node_lcp_req_outstanding = false;

  c_lcpTabDefWritesControl.init(MAX_CONCURRENT_LCP_TAB_DEF_FLUSHES);
  for (Uint32 i = 0; i < MAX_NDB_NODES; i++)
  {
    m_node_redo_alert_state[i] = RedoStateRep::NO_REDO_ALERT;
  }
  m_global_redo_alert_state = RedoStateRep::NO_REDO_ALERT;
}//Dbdih::initData()

void Dbdih::initRecords()
{
  // Records with dynamic sizes

  connectRecord = (ConnectRecord*)allocRecord("ConnectRecord",
                                              sizeof(ConnectRecord), 
                                              cconnectFileSize);

  fileRecord = (FileRecord*)allocRecord("FileRecord",
                                        sizeof(FileRecord),
                                        cfileFileSize);

  fragmentstore = (Fragmentstore*)allocRecord("Fragmentstore",
                                              sizeof(Fragmentstore),
                                              cfragstoreFileSize);

  pageRecord = (PageRecord*)allocRecord("PageRecord",
                                  sizeof(PageRecord), 
                                  cpageFileSize);

  c_replicaRecordPool.setSize(creplicaFileSize);

  tabRecord = (TabRecord*)allocRecord("TabRecord",
                                              sizeof(TabRecord), 
                                              ctabFileSize);

  // Initialize BAT for interface to file system
  NewVARIABLE* bat = allocateBat(22);
  bat[1].WA = &pageRecord->word[0];
  bat[1].nrr = cpageFileSize;
  bat[1].ClusterSize = sizeof(PageRecord);
  bat[1].bits.q = 11;
  bat[1].bits.v = 5;
  bat[20].WA = &sysfileData[0];
  bat[20].nrr = 1;
  bat[20].ClusterSize = sizeof(sysfileData);
  bat[20].bits.q = 7;
  bat[20].bits.v = 5;
  bat[21].WA = &sysfileDataToFile[0];
  bat[21].nrr = 1;
  bat[21].ClusterSize = sizeof(sysfileDataToFile);
  bat[21].bits.q = 7;
  bat[21].bits.v = 5;
}//Dbdih::initRecords()

Dbdih::Dbdih(Block_context& ctx):
  SimulatedBlock(DBDIH, ctx),
  c_queued_lcp_frag_rep(c_replicaRecordPool),
  c_activeTakeOverList(c_takeOverPool),
  c_queued_for_start_takeover_list(c_takeOverPool),
  c_queued_for_commit_takeover_list(c_takeOverPool),
  c_active_copy_threads_list(c_takeOverPool),
  c_completed_copy_threads_list(c_takeOverPool),
  c_masterActiveTakeOverList(c_takeOverPool),
  c_waitGCPProxyList(waitGCPProxyPool),
  c_waitGCPMasterList(waitGCPMasterPool),
  c_waitEpochMasterList(waitGCPMasterPool)
{
  BLOCK_CONSTRUCTOR(Dbdih);

  c_mainTakeOverPtr.i = RNIL;
  c_mainTakeOverPtr.p = 0;
  c_activeThreadTakeOverPtr.i = RNIL;
  c_activeThreadTakeOverPtr.p = 0;

  /* Node Recovery Status Module signals */
  addRecSignal(GSN_ALLOC_NODEID_REP, &Dbdih::execALLOC_NODEID_REP);
  addRecSignal(GSN_INCL_NODE_HB_PROTOCOL_REP,
               &Dbdih::execINCL_NODE_HB_PROTOCOL_REP);
  addRecSignal(GSN_NDBCNTR_START_WAIT_REP,
               &Dbdih::execNDBCNTR_START_WAIT_REP);
  addRecSignal(GSN_NDBCNTR_STARTED_REP,
               &Dbdih::execNDBCNTR_STARTED_REP);
  addRecSignal(GSN_SUMA_HANDOVER_COMPLETE_REP,
               &Dbdih::execSUMA_HANDOVER_COMPLETE_REP);
  addRecSignal(GSN_END_TOREP, &Dbdih::execEND_TOREP);
  addRecSignal(GSN_LOCAL_RECOVERY_COMP_REP,
               &Dbdih::execLOCAL_RECOVERY_COMP_REP);
  addRecSignal(GSN_DBINFO_SCANREQ, &Dbdih::execDBINFO_SCANREQ);
  /* End Node Recovery Status Module signals */

  /* LCP Pause module */
  addRecSignal(GSN_PAUSE_LCP_REQ, &Dbdih::execPAUSE_LCP_REQ);
  addRecSignal(GSN_PAUSE_LCP_CONF, &Dbdih::execPAUSE_LCP_CONF);
  addRecSignal(GSN_FLUSH_LCP_REP_REQ, &Dbdih::execFLUSH_LCP_REP_REQ);
  addRecSignal(GSN_FLUSH_LCP_REP_CONF, &Dbdih::execFLUSH_LCP_REP_CONF);
  /* End LCP Pause module */

  addRecSignal(GSN_DUMP_STATE_ORD, &Dbdih::execDUMP_STATE_ORD);
  addRecSignal(GSN_NDB_TAMPER, &Dbdih::execNDB_TAMPER, true);
  addRecSignal(GSN_DEBUG_SIG, &Dbdih::execDEBUG_SIG);
  addRecSignal(GSN_MASTER_GCPREQ, &Dbdih::execMASTER_GCPREQ);
  addRecSignal(GSN_MASTER_GCPREF, &Dbdih::execMASTER_GCPREF);
  addRecSignal(GSN_MASTER_GCPCONF, &Dbdih::execMASTER_GCPCONF);
  addRecSignal(GSN_EMPTY_LCP_CONF, &Dbdih::execEMPTY_LCP_CONF);
  addRecSignal(GSN_EMPTY_LCP_REP, &Dbdih::execEMPTY_LCP_REP);

  addRecSignal(GSN_MASTER_LCPREQ, &Dbdih::execMASTER_LCPREQ);
  addRecSignal(GSN_MASTER_LCPREF, &Dbdih::execMASTER_LCPREF);
  addRecSignal(GSN_MASTER_LCPCONF, &Dbdih::execMASTER_LCPCONF);
  addRecSignal(GSN_NF_COMPLETEREP, &Dbdih::execNF_COMPLETEREP);
  addRecSignal(GSN_START_PERMREQ, &Dbdih::execSTART_PERMREQ);
  addRecSignal(GSN_START_PERMCONF, &Dbdih::execSTART_PERMCONF);
  addRecSignal(GSN_START_PERMREF, &Dbdih::execSTART_PERMREF);
  addRecSignal(GSN_INCL_NODEREQ, &Dbdih::execINCL_NODEREQ);
  addRecSignal(GSN_INCL_NODECONF, &Dbdih::execINCL_NODECONF);

  addRecSignal(GSN_START_TOREQ, &Dbdih::execSTART_TOREQ);
  addRecSignal(GSN_START_TOREF, &Dbdih::execSTART_TOREQ);
  addRecSignal(GSN_START_TOCONF, &Dbdih::execSTART_TOCONF);

  addRecSignal(GSN_UPDATE_TOREQ, &Dbdih::execUPDATE_TOREQ);
  addRecSignal(GSN_UPDATE_TOREF, &Dbdih::execUPDATE_TOREF);
  addRecSignal(GSN_UPDATE_TOCONF, &Dbdih::execUPDATE_TOCONF);

  addRecSignal(GSN_END_TOREQ, &Dbdih::execEND_TOREQ);
  addRecSignal(GSN_END_TOREF, &Dbdih::execEND_TOREF);
  addRecSignal(GSN_END_TOCONF, &Dbdih::execEND_TOCONF);

  addRecSignal(GSN_START_MEREQ, &Dbdih::execSTART_MEREQ);
  addRecSignal(GSN_START_MECONF, &Dbdih::execSTART_MECONF);
  addRecSignal(GSN_START_MEREF, &Dbdih::execSTART_MEREF);
  addRecSignal(GSN_START_COPYREQ, &Dbdih::execSTART_COPYREQ);
  addRecSignal(GSN_START_COPYCONF, &Dbdih::execSTART_COPYCONF);
  addRecSignal(GSN_START_COPYREF, &Dbdih::execSTART_COPYREF);
  addRecSignal(GSN_UPDATE_FRAG_STATEREQ,
                 &Dbdih::execUPDATE_FRAG_STATEREQ);
  addRecSignal(GSN_UPDATE_FRAG_STATECONF,
                 &Dbdih::execUPDATE_FRAG_STATECONF);
  addRecSignal(GSN_DIVERIFYREQ, &Dbdih::execDIVERIFYREQ);
  addRecSignal(GSN_GCP_SAVEREQ, &Dbdih::execGCP_SAVEREQ);
  addRecSignal(GSN_GCP_SAVEREF, &Dbdih::execGCP_SAVEREF);
  addRecSignal(GSN_GCP_SAVECONF, &Dbdih::execGCP_SAVECONF);
  addRecSignal(GSN_GCP_PREPARECONF, &Dbdih::execGCP_PREPARECONF);
  addRecSignal(GSN_GCP_PREPARE, &Dbdih::execGCP_PREPARE);
  addRecSignal(GSN_GCP_NODEFINISH, &Dbdih::execGCP_NODEFINISH);
  addRecSignal(GSN_GCP_COMMIT, &Dbdih::execGCP_COMMIT);
  addRecSignal(GSN_SUB_GCP_COMPLETE_REP, &Dbdih::execSUB_GCP_COMPLETE_REP);
  addRecSignal(GSN_SUB_GCP_COMPLETE_ACK, &Dbdih::execSUB_GCP_COMPLETE_ACK);
  addRecSignal(GSN_DIHNDBTAMPER, &Dbdih::execDIHNDBTAMPER);
  addRecSignal(GSN_CONTINUEB, &Dbdih::execCONTINUEB);
  addRecSignal(GSN_COPY_GCIREQ, &Dbdih::execCOPY_GCIREQ);
  addRecSignal(GSN_COPY_GCICONF, &Dbdih::execCOPY_GCICONF);
  addRecSignal(GSN_COPY_TABREQ, &Dbdih::execCOPY_TABREQ);
  addRecSignal(GSN_COPY_TABCONF, &Dbdih::execCOPY_TABCONF);
  addRecSignal(GSN_CHECK_LCP_IDLE_ORD, &Dbdih::execCHECK_LCP_IDLE_ORD);
  addRecSignal(GSN_TCGETOPSIZECONF, &Dbdih::execTCGETOPSIZECONF);
  addRecSignal(GSN_TC_CLOPSIZECONF, &Dbdih::execTC_CLOPSIZECONF);

  addRecSignal(GSN_LCP_COMPLETE_REP, &Dbdih::execLCP_COMPLETE_REP);
  addRecSignal(GSN_LCP_FRAG_REP, &Dbdih::execLCP_FRAG_REP);
  addRecSignal(GSN_START_LCP_REQ, &Dbdih::execSTART_LCP_REQ);
  addRecSignal(GSN_START_LCP_CONF, &Dbdih::execSTART_LCP_CONF);
  addRecSignal(GSN_START_NODE_LCP_CONF, &Dbdih::execSTART_NODE_LCP_CONF);
  
  addRecSignal(GSN_READ_CONFIG_REQ, &Dbdih::execREAD_CONFIG_REQ, true);
  addRecSignal(GSN_UNBLO_DICTCONF, &Dbdih::execUNBLO_DICTCONF);
  addRecSignal(GSN_COPY_ACTIVECONF, &Dbdih::execCOPY_ACTIVECONF);
  addRecSignal(GSN_TAB_COMMITREQ, &Dbdih::execTAB_COMMITREQ);
  addRecSignal(GSN_NODE_FAILREP, &Dbdih::execNODE_FAILREP);
  addRecSignal(GSN_COPY_FRAGCONF, &Dbdih::execCOPY_FRAGCONF);
  addRecSignal(GSN_COPY_FRAGREF, &Dbdih::execCOPY_FRAGREF);
  addRecSignal(GSN_DIADDTABREQ, &Dbdih::execDIADDTABREQ);
  addRecSignal(GSN_DIGETNODESREQ, &Dbdih::execDIGETNODESREQ);
  addRecSignal(GSN_STTOR, &Dbdih::execSTTOR);
  addRecSignal(GSN_DIH_SCAN_TAB_REQ, &Dbdih::execDIH_SCAN_TAB_REQ);
  addRecSignal(GSN_DIH_SCAN_TAB_COMPLETE_REP,
               &Dbdih::execDIH_SCAN_TAB_COMPLETE_REP);
  addRecSignal(GSN_GCP_TCFINISHED, &Dbdih::execGCP_TCFINISHED);
  addRecSignal(GSN_READ_NODESCONF, &Dbdih::execREAD_NODESCONF);
  addRecSignal(GSN_NDB_STTOR, &Dbdih::execNDB_STTOR);
  addRecSignal(GSN_DICTSTARTCONF, &Dbdih::execDICTSTARTCONF);
  addRecSignal(GSN_NDB_STARTREQ, &Dbdih::execNDB_STARTREQ);
  addRecSignal(GSN_GETGCIREQ, &Dbdih::execGETGCIREQ);
  addRecSignal(GSN_GET_LATEST_GCI_REQ, &Dbdih::execGET_LATEST_GCI_REQ);
  addRecSignal(GSN_SET_LATEST_LCP_ID, &Dbdih::execSET_LATEST_LCP_ID);
  addRecSignal(GSN_DIH_RESTARTREQ, &Dbdih::execDIH_RESTARTREQ);
  addRecSignal(GSN_START_RECCONF, &Dbdih::execSTART_RECCONF);
  addRecSignal(GSN_START_FRAGCONF, &Dbdih::execSTART_FRAGCONF);
  addRecSignal(GSN_ADD_FRAGCONF, &Dbdih::execADD_FRAGCONF);
  addRecSignal(GSN_ADD_FRAGREF, &Dbdih::execADD_FRAGREF);
  addRecSignal(GSN_DROP_FRAG_REF, &Dbdih::execDROP_FRAG_REF);
  addRecSignal(GSN_DROP_FRAG_CONF, &Dbdih::execDROP_FRAG_CONF);
  addRecSignal(GSN_FSOPENCONF, &Dbdih::execFSOPENCONF);
  addRecSignal(GSN_FSOPENREF, &Dbdih::execFSOPENREF, true);
  addRecSignal(GSN_FSCLOSECONF, &Dbdih::execFSCLOSECONF);
  addRecSignal(GSN_FSCLOSEREF, &Dbdih::execFSCLOSEREF, true);
  addRecSignal(GSN_FSREADCONF, &Dbdih::execFSREADCONF);
  addRecSignal(GSN_FSREADREF, &Dbdih::execFSREADREF, true);
  addRecSignal(GSN_FSWRITECONF, &Dbdih::execFSWRITECONF);
  addRecSignal(GSN_FSWRITEREF, &Dbdih::execFSWRITEREF, true);

  addRecSignal(GSN_START_INFOREQ, 
               &Dbdih::execSTART_INFOREQ);
  addRecSignal(GSN_START_INFOREF, 
               &Dbdih::execSTART_INFOREF);
  addRecSignal(GSN_START_INFOCONF, 
               &Dbdih::execSTART_INFOCONF);

  addRecSignal(GSN_CHECKNODEGROUPSREQ, &Dbdih::execCHECKNODEGROUPSREQ);

  addRecSignal(GSN_CHECK_NODE_RESTARTREQ,
               &Dbdih::execCHECK_NODE_RESTARTREQ);

  addRecSignal(GSN_BLOCK_COMMIT_ORD,
	       &Dbdih::execBLOCK_COMMIT_ORD);
  addRecSignal(GSN_UNBLOCK_COMMIT_ORD,
	       &Dbdih::execUNBLOCK_COMMIT_ORD);
  
  addRecSignal(GSN_DIH_SWITCH_REPLICA_REQ,
	       &Dbdih::execDIH_SWITCH_REPLICA_REQ);
  
  addRecSignal(GSN_DIH_SWITCH_REPLICA_REF,
	       &Dbdih::execDIH_SWITCH_REPLICA_REF);
  
  addRecSignal(GSN_DIH_SWITCH_REPLICA_CONF,
	       &Dbdih::execDIH_SWITCH_REPLICA_CONF);

  addRecSignal(GSN_STOP_PERM_REQ, &Dbdih::execSTOP_PERM_REQ);
  addRecSignal(GSN_STOP_PERM_REF, &Dbdih::execSTOP_PERM_REF);
  addRecSignal(GSN_STOP_PERM_CONF, &Dbdih::execSTOP_PERM_CONF);

  addRecSignal(GSN_STOP_ME_REQ, &Dbdih::execSTOP_ME_REQ);
  addRecSignal(GSN_STOP_ME_REF, &Dbdih::execSTOP_ME_REF);
  addRecSignal(GSN_STOP_ME_CONF, &Dbdih::execSTOP_ME_CONF);

  addRecSignal(GSN_WAIT_GCP_REQ, &Dbdih::execWAIT_GCP_REQ);
  addRecSignal(GSN_WAIT_GCP_REF, &Dbdih::execWAIT_GCP_REF);
  addRecSignal(GSN_WAIT_GCP_CONF, &Dbdih::execWAIT_GCP_CONF);

  addRecSignal(GSN_REDO_STATE_REP, &Dbdih::execREDO_STATE_REP);

  addRecSignal(GSN_PREP_DROP_TAB_REQ, &Dbdih::execPREP_DROP_TAB_REQ);
  addRecSignal(GSN_DROP_TAB_REQ, &Dbdih::execDROP_TAB_REQ);

  addRecSignal(GSN_ALTER_TAB_REQ, &Dbdih::execALTER_TAB_REQ);

  addRecSignal(GSN_CREATE_FRAGMENTATION_REQ, 
	       &Dbdih::execCREATE_FRAGMENTATION_REQ);

  addRecSignal(GSN_DICT_LOCK_CONF, &Dbdih::execDICT_LOCK_CONF);
  addRecSignal(GSN_DICT_LOCK_REF, &Dbdih::execDICT_LOCK_REF);
  addRecSignal(GSN_NODE_START_REP, &Dbdih::execNODE_START_REP, true);
  
  addRecSignal(GSN_START_FRAGREF,
	       &Dbdih::execSTART_FRAGREF);

  addRecSignal(GSN_PREPARE_COPY_FRAG_REF,
	       &Dbdih::execPREPARE_COPY_FRAG_REF);
  addRecSignal(GSN_PREPARE_COPY_FRAG_CONF,
	       &Dbdih::execPREPARE_COPY_FRAG_CONF);

  addRecSignal(GSN_UPGRADE_PROTOCOL_ORD,
	       &Dbdih::execUPGRADE_PROTOCOL_ORD);

  addRecSignal(GSN_CREATE_NODEGROUP_IMPL_REQ,
               &Dbdih::execCREATE_NODEGROUP_IMPL_REQ);

  addRecSignal(GSN_DROP_NODEGROUP_IMPL_REQ,
               &Dbdih::execDROP_NODEGROUP_IMPL_REQ);


  addRecSignal(GSN_DIH_GET_TABINFO_REQ,
               &Dbdih::execDIH_GET_TABINFO_REQ);
#if 0
  addRecSignal(GSN_DIH_GET_TABINFO_REF,
               &Dbdih::execDIH_GET_TABINFO_REF);
  addRecSignal(GSN_DIH_GET_TABINFO_CONF,
               &Dbdih::execDIH_GET_TABINFO_CONF);
#endif

  connectRecord = 0;
  fileRecord = 0;
  fragmentstore = 0;
  pageRecord = 0;
  tabRecord = 0;
  createReplicaRecord = 0;
  nodeGroupRecord = 0;
  nodeRecord = 0;
  c_nextNodeGroup = 0;
  memset(c_next_replica_node, 0, sizeof(c_next_replica_node));
  c_fragments_per_node_ = 0;
  memset(c_node_groups, 0, sizeof(c_node_groups));
  if (globalData.ndbMtTcThreads == 0)
  {
    c_diverify_queue_cnt = 1;
  }
  else
  {
    c_diverify_queue_cnt = globalData.ndbMtTcThreads;
  }
}//Dbdih::Dbdih()

Dbdih::~Dbdih()
{
  deallocRecord((void **)&connectRecord, "ConnectRecord",
                sizeof(ConnectRecord), 
                cconnectFileSize);
  
  deallocRecord((void **)&fileRecord, "FileRecord",
                sizeof(FileRecord),
                cfileFileSize);
  
  deallocRecord((void **)&fragmentstore, "Fragmentstore",
                sizeof(Fragmentstore),
                cfragstoreFileSize);

  deallocRecord((void **)&pageRecord, "PageRecord",
                sizeof(PageRecord), 
                cpageFileSize);
  
  deallocRecord((void **)&tabRecord, "TabRecord",
                sizeof(TabRecord), 
                ctabFileSize);

  // Records with constant sizes
  deallocRecord((void **)&createReplicaRecord, 
                "CreateReplicaRecord", sizeof(CreateReplicaRecord),
                ZCREATE_REPLICA_FILE_SIZE);
  
  deallocRecord((void **)&nodeGroupRecord, "NodeGroupRecord", 
                sizeof(NodeGroupRecord), MAX_NDB_NODE_GROUPS);
  
  deallocRecord((void **)&nodeRecord, "NodeRecord", 
                sizeof(NodeRecord), MAX_NDB_NODES);
}//Dbdih::~Dbdih()

BLOCK_FUNCTIONS(Dbdih)




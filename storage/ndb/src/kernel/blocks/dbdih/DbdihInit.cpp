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


#define DBDIH_C
#include "Dbdih.hpp"
#include <ndb_limits.h>

#define DEBUG(x) { ndbout << "DIH::" << x << endl; }

void Dbdih::initData() 
{
  cpageFileSize = ZPAGEREC;

  // Records with constant sizes
  createReplicaRecord = (CreateReplicaRecord*)
    allocRecord("CreateReplicaRecord", sizeof(CreateReplicaRecord),
                 ZCREATE_REPLICA_FILE_SIZE);

  nodeGroupRecord = (NodeGroupRecord*)
    allocRecord("NodeGroupRecord", sizeof(NodeGroupRecord), MAX_NDB_NODES);

  nodeRecord = (NodeRecord*)
    allocRecord("NodeRecord", sizeof(NodeRecord), MAX_NDB_NODES);

  Uint32 i;
  for(i = 0; i<MAX_NDB_NODES; i++){
    new (&nodeRecord[i]) NodeRecord();
  }
  
  takeOverRecord = (TakeOverRecord*)allocRecord("TakeOverRecord",
                                                sizeof(TakeOverRecord), 
                                                MAX_NDB_NODES);
  for(i = 0; i<MAX_NDB_NODES; i++)
    new (&takeOverRecord[i]) TakeOverRecord();

  for(i = 0; i<MAX_NDB_NODES; i++)
    new (&takeOverRecord[i]) TakeOverRecord();
  
  waitGCPProxyPool.setSize(ZPROXY_FILE_SIZE);
  waitGCPMasterPool.setSize(ZPROXY_MASTER_FILE_SIZE);

  c_dictLockSlavePool.setSize(1); // assert single usage
  c_dictLockSlavePtrI_nodeRestart = RNIL;

  cgcpOrderBlocked = 0;
  c_lcpState.ctcCounter = 0;
  cwaitLcpSr       = false;
  c_blockCommit    = false;
  c_blockCommitNo  = 1;
  cntrlblockref    = RNIL;
  c_set_initial_start_flag = FALSE;
}//Dbdih::initData()

void Dbdih::initRecords() 
{
  // Records with dynamic sizes
  apiConnectRecord = (ApiConnectRecord*)
    allocRecord("ApiConnectRecord", 
                sizeof(ApiConnectRecord),
                capiConnectFileSize);

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

  replicaRecord = (ReplicaRecord*)allocRecord("ReplicaRecord",
                                              sizeof(ReplicaRecord), 
                                              creplicaFileSize);

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
  c_waitGCPProxyList(waitGCPProxyPool),
  c_waitGCPMasterList(waitGCPMasterPool)
{
  BLOCK_CONSTRUCTOR(Dbdih);

  addRecSignal(GSN_DUMP_STATE_ORD, &Dbdih::execDUMP_STATE_ORD);
  addRecSignal(GSN_NDB_TAMPER, &Dbdih::execNDB_TAMPER, true);
  addRecSignal(GSN_DEBUG_SIG, &Dbdih::execDEBUG_SIG);
  addRecSignal(GSN_MASTER_GCPREQ, &Dbdih::execMASTER_GCPREQ);
  addRecSignal(GSN_MASTER_GCPREF, &Dbdih::execMASTER_GCPREF);
  addRecSignal(GSN_MASTER_GCPCONF, &Dbdih::execMASTER_GCPCONF);
  addRecSignal(GSN_EMPTY_LCP_CONF, &Dbdih::execEMPTY_LCP_CONF);
  addRecSignal(GSN_MASTER_LCPREQ, &Dbdih::execMASTER_LCPREQ);
  addRecSignal(GSN_MASTER_LCPREF, &Dbdih::execMASTER_LCPREF);
  addRecSignal(GSN_MASTER_LCPCONF, &Dbdih::execMASTER_LCPCONF);
  addRecSignal(GSN_NF_COMPLETEREP, &Dbdih::execNF_COMPLETEREP);
  addRecSignal(GSN_START_PERMREQ, &Dbdih::execSTART_PERMREQ);
  addRecSignal(GSN_START_PERMCONF, &Dbdih::execSTART_PERMCONF);
  addRecSignal(GSN_START_PERMREF, &Dbdih::execSTART_PERMREF);
  addRecSignal(GSN_INCL_NODEREQ, &Dbdih::execINCL_NODEREQ);
  addRecSignal(GSN_INCL_NODECONF, &Dbdih::execINCL_NODECONF);
  addRecSignal(GSN_END_TOREQ, &Dbdih::execEND_TOREQ);
  addRecSignal(GSN_END_TOCONF, &Dbdih::execEND_TOCONF);
  addRecSignal(GSN_START_TOREQ, &Dbdih::execSTART_TOREQ);
  addRecSignal(GSN_START_TOCONF, &Dbdih::execSTART_TOCONF);
  addRecSignal(GSN_START_MEREQ, &Dbdih::execSTART_MEREQ);
  addRecSignal(GSN_START_MECONF, &Dbdih::execSTART_MECONF);
  addRecSignal(GSN_START_MEREF, &Dbdih::execSTART_MEREF);
  addRecSignal(GSN_START_COPYREQ, &Dbdih::execSTART_COPYREQ);
  addRecSignal(GSN_START_COPYCONF, &Dbdih::execSTART_COPYCONF);
  addRecSignal(GSN_START_COPYREF, &Dbdih::execSTART_COPYREF);
  addRecSignal(GSN_CREATE_FRAGREQ, &Dbdih::execCREATE_FRAGREQ);
  addRecSignal(GSN_CREATE_FRAGCONF, &Dbdih::execCREATE_FRAGCONF);
  addRecSignal(GSN_DIVERIFYREQ, &Dbdih::execDIVERIFYREQ);
  addRecSignal(GSN_GCP_SAVECONF, &Dbdih::execGCP_SAVECONF);
  addRecSignal(GSN_GCP_PREPARECONF, &Dbdih::execGCP_PREPARECONF);
  addRecSignal(GSN_GCP_PREPARE, &Dbdih::execGCP_PREPARE);
  addRecSignal(GSN_GCP_NODEFINISH, &Dbdih::execGCP_NODEFINISH);
  addRecSignal(GSN_GCP_COMMIT, &Dbdih::execGCP_COMMIT);
  addRecSignal(GSN_DIHNDBTAMPER, &Dbdih::execDIHNDBTAMPER);
  addRecSignal(GSN_CONTINUEB, &Dbdih::execCONTINUEB);
  addRecSignal(GSN_COPY_GCIREQ, &Dbdih::execCOPY_GCIREQ);
  addRecSignal(GSN_COPY_GCICONF, &Dbdih::execCOPY_GCICONF);
  addRecSignal(GSN_COPY_TABREQ, &Dbdih::execCOPY_TABREQ);
  addRecSignal(GSN_COPY_TABCONF, &Dbdih::execCOPY_TABCONF);
  addRecSignal(GSN_TCGETOPSIZECONF, &Dbdih::execTCGETOPSIZECONF);
  addRecSignal(GSN_TC_CLOPSIZECONF, &Dbdih::execTC_CLOPSIZECONF);

  addRecSignal(GSN_LCP_COMPLETE_REP, &Dbdih::execLCP_COMPLETE_REP);
  addRecSignal(GSN_LCP_FRAG_REP, &Dbdih::execLCP_FRAG_REP);
  addRecSignal(GSN_START_LCP_REQ, &Dbdih::execSTART_LCP_REQ);
  addRecSignal(GSN_START_LCP_CONF, &Dbdih::execSTART_LCP_CONF);
  
  addRecSignal(GSN_READ_CONFIG_REQ, &Dbdih::execREAD_CONFIG_REQ, true);
  addRecSignal(GSN_UNBLO_DICTCONF, &Dbdih::execUNBLO_DICTCONF);
  addRecSignal(GSN_COPY_ACTIVECONF, &Dbdih::execCOPY_ACTIVECONF);
  addRecSignal(GSN_TAB_COMMITREQ, &Dbdih::execTAB_COMMITREQ);
  addRecSignal(GSN_NODE_FAILREP, &Dbdih::execNODE_FAILREP);
  addRecSignal(GSN_COPY_FRAGCONF, &Dbdih::execCOPY_FRAGCONF);
  addRecSignal(GSN_COPY_FRAGREF, &Dbdih::execCOPY_FRAGREF);
  addRecSignal(GSN_DIADDTABREQ, &Dbdih::execDIADDTABREQ);
  addRecSignal(GSN_DIGETNODESREQ, &Dbdih::execDIGETNODESREQ);
  addRecSignal(GSN_DIRELEASEREQ, &Dbdih::execDIRELEASEREQ);
  addRecSignal(GSN_DISEIZEREQ, &Dbdih::execDISEIZEREQ);
  addRecSignal(GSN_STTOR, &Dbdih::execSTTOR);
  addRecSignal(GSN_DI_FCOUNTREQ, &Dbdih::execDI_FCOUNTREQ);
  addRecSignal(GSN_DIGETPRIMREQ, &Dbdih::execDIGETPRIMREQ);
  addRecSignal(GSN_GCP_SAVEREF, &Dbdih::execGCP_SAVEREF);
  addRecSignal(GSN_GCP_TCFINISHED, &Dbdih::execGCP_TCFINISHED);
  addRecSignal(GSN_READ_NODESCONF, &Dbdih::execREAD_NODESCONF);
  addRecSignal(GSN_NDB_STTOR, &Dbdih::execNDB_STTOR);
  addRecSignal(GSN_DICTSTARTCONF, &Dbdih::execDICTSTARTCONF);
  addRecSignal(GSN_NDB_STARTREQ, &Dbdih::execNDB_STARTREQ);
  addRecSignal(GSN_GETGCIREQ, &Dbdih::execGETGCIREQ);
  addRecSignal(GSN_DIH_RESTARTREQ, &Dbdih::execDIH_RESTARTREQ);
  addRecSignal(GSN_START_RECCONF, &Dbdih::execSTART_RECCONF);
  addRecSignal(GSN_START_FRAGCONF, &Dbdih::execSTART_FRAGCONF);
  addRecSignal(GSN_ADD_FRAGCONF, &Dbdih::execADD_FRAGCONF);
  addRecSignal(GSN_ADD_FRAGREF, &Dbdih::execADD_FRAGREF);
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

  addRecSignal(GSN_UPDATE_TOREQ, &Dbdih::execUPDATE_TOREQ);
  addRecSignal(GSN_UPDATE_TOCONF, &Dbdih::execUPDATE_TOCONF);

  addRecSignal(GSN_PREP_DROP_TAB_REQ, &Dbdih::execPREP_DROP_TAB_REQ);
  addRecSignal(GSN_WAIT_DROP_TAB_REF, &Dbdih::execWAIT_DROP_TAB_REF);
  addRecSignal(GSN_WAIT_DROP_TAB_CONF, &Dbdih::execWAIT_DROP_TAB_CONF);
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
  
  apiConnectRecord = 0;
  connectRecord = 0;
  fileRecord = 0;
  fragmentstore = 0;
  pageRecord = 0;
  replicaRecord = 0;
  tabRecord = 0;
  takeOverRecord = 0;
  createReplicaRecord = 0;
  nodeGroupRecord = 0;
  nodeRecord = 0;
  c_nextNodeGroup = 0;
}//Dbdih::Dbdih()

Dbdih::~Dbdih() 
{
  deallocRecord((void **)&apiConnectRecord, "ApiConnectRecord", 
                sizeof(ApiConnectRecord),
                capiConnectFileSize);
  
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
  
  deallocRecord((void **)&replicaRecord, "ReplicaRecord",
                sizeof(ReplicaRecord), 
                creplicaFileSize);
  
  deallocRecord((void **)&tabRecord, "TabRecord",
                sizeof(TabRecord), 
                ctabFileSize);

  // Records with constant sizes
  deallocRecord((void **)&createReplicaRecord, 
                "CreateReplicaRecord", sizeof(CreateReplicaRecord),
                ZCREATE_REPLICA_FILE_SIZE);
  
  deallocRecord((void **)&nodeGroupRecord, "NodeGroupRecord", 
                sizeof(NodeGroupRecord), MAX_NDB_NODES);
  
  deallocRecord((void **)&nodeRecord, "NodeRecord", 
                sizeof(NodeRecord), MAX_NDB_NODES);

  deallocRecord((void **)&takeOverRecord, "TakeOverRecord",
                sizeof(TakeOverRecord), 
                MAX_NDB_NODES);

}//Dbdih::~Dbdih()

BLOCK_FUNCTIONS(Dbdih)




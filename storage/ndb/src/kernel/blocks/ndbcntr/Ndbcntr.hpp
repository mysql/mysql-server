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

#ifndef NDBCNTR_H
#define NDBCNTR_H

#include <ndb_limits.h>
#include <SimulatedBlock.hpp>
#include <pc.hpp>
#include <signaldata/CheckNodeGroups.hpp>
#include <signaldata/CntrStart.hpp>
#include <signaldata/DictTabInfo.hpp>
#include <signaldata/LocalSysfile.hpp>
#include <signaldata/RedoStateRep.hpp>
#include <signaldata/ResumeReq.hpp>
#include <signaldata/StopReq.hpp>

#include <NdbTick.h>
#include <NodeState.hpp>

#define JAM_FILE_ID 457

#ifdef NDBCNTR_C
/*
2.1 GLOBAL SYMBOLS
------------------
*/
/*
2.2 LOCAL SYMBOLS
-----------------
*/
#define ZNO_NDB_BLOCKS 6 /* ACC, DICT, DIH, LQH, TC, TUP         */

#define ZNOT_AVAILABLE 913

//------- OTHERS ---------------------------------------------
#define ZSTARTUP 1
#define ZSHUTDOWN 2
#define ZBLOCK_STTOR 3

#define ZSIZE_NDB_BLOCKS_REC 16 /* MAX BLOCKS IN NDB                    */
#define ZSTART_PHASE_1 1
#define ZSTART_PHASE_2 2
#define ZSTART_PHASE_3 3
#define ZSTART_PHASE_4 4
#define ZSTART_PHASE_5 5
#define ZSTART_PHASE_6 6
#define ZSTART_PHASE_7 7
#define ZSTART_PHASE_8 8
#define ZSTART_PHASE_9 9
#define ZSTART_PHASE_END 255
#endif

class Ndbcntr : public SimulatedBlock {
 public:
  // Records

  /* FSREADREQ FSWRITEREQ         */
  /**
   * 2.3 RECORDS AND FILESIZES
   * ------------------------------------------------------------
   */

  struct StartRecord {
    StartRecord() {}
    NDB_TICKS m_startTime;

    void reset();
    NdbNodeBitmask m_starting;
    NdbNodeBitmask m_waiting;
    // == (m_withLog | m_withoutLog | m_withLogNotRestorable | m_waitTO)
    NdbNodeBitmask m_withLog;
    NdbNodeBitmask m_withLogNotRestorable;
    NdbNodeBitmask m_withoutLog;
    NdbNodeBitmask m_waitTO;
    Uint32 m_lastGci;
    Uint32 m_lastGciNodeId;
    Uint32 m_lastLcpId;

    // Timeouts in ms since 'm_startTime'
    Uint64 m_startPartialTimeout;  // UNUSED!
    Uint64 m_startPartitionedTimeout;
    Uint64 m_startFailureTimeout;
    struct {
      Uint32 m_nodeId;
      Uint32 m_lastGci;
    } m_logNodes[MAX_NDB_NODES];
    Uint32 m_logNodesCount;

    Uint32 m_wait_sp[MAX_NDB_NODES];
  } c_start;

  struct LocalSysfile {
    LocalSysfile() {}
    Uint32 m_data[128];
    Uint32 m_file_pointer;
    Uint32 m_sender_data;
    Uint32 m_sender_ref;
    bool m_initial_read_done;
    bool m_last_write_done;
    bool m_initial_write_local_sysfile_ongoing;
    static constexpr Uint32 FILE_ID = 0;
    enum {
      NOT_USED = 0,
      OPEN_READ_FILE_0 = 1,
      OPEN_READ_FILE_1 = 2,
      READ_FILE_0 = 3,
      READ_FILE_1 = 4,
      CLOSE_READ_FILE = 5,
      CLOSE_READ_REF_0 = 6,
      CLOSE_READ_REF_1 = 7,
      OPEN_WRITE_FILE_0 = 8,
      OPEN_WRITE_FILE_1 = 9,
      WRITE_FILE_0 = 10,
      WRITE_FILE_1 = 11,
      CLOSE_WRITE_FILE_0 = 12,
      CLOSE_WRITE_FILE_1 = 13
    } m_state;
    Uint32 m_restorable_flag;
    Uint32 m_max_restorable_gci;
  } c_local_sysfile;

  struct SecretsFileOperationRecord {
    SecretsFileOperationRecord() {}
    Uint32 m_data[128];
    Uint32 m_file_pointer;
    Uint32 m_sender_data;
    Uint32 m_sender_ref;
    static constexpr Uint32 FILE_ID = 1;
    enum {
      NOT_USED = 0,
      OPEN_READ_FILE_0 = 1,
      READ_FILE_0 = 2,
      CLOSE_READ_FILE_0 = 3,
      OPEN_WRITE_FILE_0 = 4,
      WRITE_FILE_0 = 5,
      CLOSE_WRITE_FILE_0 = 6,
      CHECK_MISSING_0 = 7,
      WAITING = 8
    } m_state;
  } c_secretsfile;

  static constexpr Uint32 c_nodeMasterKeyLength = 32;
  bool c_encrypted_filesystem;

  struct NdbBlocksRec {
    BlockReference blockref;
  }; /* p2c: size = 2 bytes */

  typedef Ptr<NdbBlocksRec> NdbBlocksRecPtr;

  /**
   * Ndbcntr creates and initializes system tables on initial system start.
   * The tables are defined in static structs in NdbcntrSysTable.cpp.
   */
  struct SysColumn {
    unsigned pos;
    const char *name;
    // DictTabInfo
    DictTabInfo::ExtType type;
    Uint32 length;
    bool keyFlag;
    bool nullable;
  };
  struct SysTable {
    const char *name;
    unsigned columnCount;
    const SysColumn *columnList;
    // DictTabInfo
    DictTabInfo::TableType tableType;
    DictTabInfo::FragmentType fragmentType;
    bool tableLoggedFlag;
    // saved table id
    mutable Uint32 tableId;
    mutable Uint32 tableVersion;
  };
  struct SysIndex {
    const char *name;
    const SysTable *primaryTable;
    Uint32 columnCount;
    Uint32 columnList[4];
    // DictTabInfo
    DictTabInfo::TableType indexType;
    DictTabInfo::FragmentType fragmentType;
    bool indexLoggedFlag;
    // saved index table id
    mutable Uint32 indexId;
  };
  static const SysTable *g_sysTableList[];
  static const unsigned g_sysTableCount;
  // the system tables
  static const SysTable g_sysTable_SYSTAB_0;
  static SysTable g_sysTable_NDBEVENTS_0;
  // schema trans
  Uint32 c_schemaTransId;
  Uint32 c_schemaTransKey;
  // intersignal transient store of: hash_map, logfilegroup, tablesspace
  Uint32 c_objectId;
  Uint32 c_objectVersion;

 public:
  Ndbcntr(Block_context &);
  ~Ndbcntr() override;

 private:
  BLOCK_DEFINES(Ndbcntr);

  // Transit signals
  void execAPI_START_REP(Signal *);
  void execCONTINUEB(Signal *signal);
  void execREAD_NODESCONF(Signal *signal);
  void execREAD_NODESREF(Signal *signal);
  void execCM_ADD_REP(Signal *signal);
  void execCNTR_START_REQ(Signal *signal);
  void execCNTR_START_REF(Signal *signal);
  void execCNTR_START_CONF(Signal *signal);
  void execCNTR_START_REP(Signal *signal);
  void execCNTR_WAITREP(Signal *signal);
  void execNODE_FAILREP(Signal *signal);
  void execSYSTEM_ERROR(Signal *signal);

  void execSTART_PERMREP(Signal *);

  // Received signals
  void execDUMP_STATE_ORD(Signal *signal);
  void execREAD_CONFIG_REQ(Signal *signal);
  void execSTTOR(Signal *signal);
  void execGETGCICONF(Signal *signal);
  void execDIH_RESTARTCONF(Signal *signal);
  void execDIH_RESTARTREF(Signal *signal);
  void execSET_UP_MULTI_TRP_CONF(Signal *);
  void execSCHEMA_TRANS_BEGIN_CONF(Signal *signal);
  void execSCHEMA_TRANS_BEGIN_REF(Signal *signal);
  void execSCHEMA_TRANS_END_CONF(Signal *signal);
  void execSCHEMA_TRANS_END_REF(Signal *signal);
  void execCREATE_TABLE_REF(Signal *signal);
  void execCREATE_TABLE_CONF(Signal *signal);
  void execCREATE_HASH_MAP_REF(Signal *signal);
  void execCREATE_HASH_MAP_CONF(Signal *signal);
  void execCREATE_FILEGROUP_REF(Signal *signal);
  void execCREATE_FILEGROUP_CONF(Signal *signal);
  void execCREATE_FILE_REF(Signal *signal);
  void execCREATE_FILE_CONF(Signal *signal);
  void execNDB_STTORRY(Signal *signal);
  void execNDB_STARTCONF(Signal *signal);
  void execREAD_NODESREQ(Signal *signal);
  void execNDB_STARTREF(Signal *signal);

  void execSTOP_PERM_REF(Signal *signal);
  void execSTOP_PERM_CONF(Signal *signal);

  void execSTOP_ME_REF(Signal *signal);
  void execSTOP_ME_CONF(Signal *signal);

  void execWAIT_GCP_REF(Signal *signal);
  void execWAIT_GCP_CONF(Signal *signal);

  void execREDO_STATE_REP(Signal *signal);

  void execSTOP_REQ(Signal *signal);
  void execSTOP_CONF(Signal *signal);
  void execRESUME_REQ(Signal *signal);

  void execCHANGE_NODE_STATE_CONF(Signal *signal);

  void execABORT_ALL_REF(Signal *signal);
  void execABORT_ALL_CONF(Signal *signal);

  // Statement blocks
  void beginSchemaTransLab(Signal *signal);
  void endSchemaTransLab(Signal *signal);
  void sendCreateTabReq(Signal *signal, const char *buffer, Uint32 bufLen);
  void initData(Signal *signal);
  void resetStartVariables(Signal *signal);
  void sendCntrStartReq(Signal *signal);
  void sendCntrStartRef(Signal *, Uint32 nodeId, CntrStartRef::ErrorCode);
  void sendNdbSttor(Signal *signal);
  void sendSttorry(Signal *signal, Uint32 delayed = 0);

  bool trySystemRestart(Signal *signal);
  void startWaitingNodes(Signal *signal);
  CheckNodeGroups::Output checkNodeGroups(Signal *, const NdbNodeBitmask &);

  // Generated statement blocks
  [[noreturn]] void systemErrorLab(Signal *signal, int line);

  void createHashMap(Signal *, Uint32 index);
  void createSystableLab(Signal *signal, unsigned index);
  void createDDObjects(Signal *, unsigned index);

  void startPhase1Lab(Signal *signal);
  void startPhase2Lab(Signal *signal);
  void startPhase3Lab(Signal *signal);
  void startPhase4Lab(Signal *signal);
  void startPhase5Lab(Signal *signal);
  // jump 2 to resync phase counters
  void startPhase8Lab(Signal *signal);
  void startPhase9Lab(Signal *signal);
  void ph2ALab(Signal *signal);
  void ph2CLab(Signal *signal);
  void ph2ELab(Signal *signal);
  void ph2FLab(Signal *signal);
  void ph2GLab(Signal *signal);
  void ph3ALab(Signal *signal);
  void ph4ALab(Signal *signal);
  void ph4BLab(Signal *signal);
  void ph4CLab(Signal *signal);
  void ph5ALab(Signal *signal);
  void ph6ALab(Signal *signal);
  void ph6BLab(Signal *signal);
  void ph7ALab(Signal *signal);
  void ph8ALab(Signal *signal);

  void waitpoint41Lab(Signal *signal);
  void waitpoint51Lab(Signal *signal);
  void waitpoint52Lab(Signal *signal);
  void waitpoint61Lab(Signal *signal);
  void waitpoint71Lab(Signal *signal);
  void waitpoint42To(Signal *signal);

  /**
   * Wait before starting sp
   *   so that all nodes in cluster is waiting for >= sp
   */
  bool wait_sp(Signal *, Uint32 sp);
  void wait_sp_rep(Signal *);

  void execSTART_COPYREF(Signal *);
  void execSTART_COPYCONF(Signal *);

  void execCREATE_NODEGROUP_IMPL_REQ(Signal *);
  void execDROP_NODEGROUP_IMPL_REQ(Signal *);

  /* Local Sysfile stuff */
  void execREAD_LOCAL_SYSFILE_REQ(Signal *);
  void execWRITE_LOCAL_SYSFILE_REQ(Signal *);
  void execREAD_LOCAL_SYSFILE_CONF(Signal *);
  void execWRITE_LOCAL_SYSFILE_CONF(Signal *);
  void execFSOPENREF(Signal *);
  void execFSOPENCONF(Signal *);
  void execFSREADREF(Signal *);
  void execFSREADCONF(Signal *);
  void execFSWRITECONF(Signal *);
  void execFSWRITEREF(Signal *);
  void execFSAPPENDREF(Signal *);
  void execFSAPPENDCONF(Signal *);
  void execFSCLOSEREF(Signal *);
  void execFSCLOSECONF(Signal *);

  void sendReadLocalSysfile(Signal *signal);
  void sendWriteLocalSysfile_initial(Signal *signal);
  void update_withLog();

  void alloc_local_bat();

  void init_local_sysfile();
  void init_local_sysfile_vars();
  void open_local_sysfile(Signal *, Uint32, bool);
  void read_local_sysfile(Signal *);
  void read_local_sysfile_data(Signal *);
  void write_local_sysfile(Signal *);
  void handle_read_refuse(Signal *);
  void close_local_sysfile(Signal *);
  void sendReadLocalSysfileConf(Signal *, BlockReference, Uint32);
  void sendWriteLocalSysfileConf(Signal *);

  void updateNodeState(Signal *signal, const NodeState &newState) const;
  void getNodeGroup(Signal *signal);

  void send_node_started_rep(Signal *signal);

  /* Secrets file stuff */

  void init_secretsfile();
  void init_secretsfile_vars();
  void create_secrets_file(Signal *signal);
  void open_secretsfile(Signal *signal, Uint32 secretsfile_num,
                        bool open_for_read, bool check_file_exists);
  void write_secretsfile(Signal *signal);
  void read_secretsfile(Signal *signal);
  void read_secretsfile_data(Signal *signal);
  void close_secretsfile(Signal *signal);

  // Initialisation
  void initData();
  void initRecords();

  // Variables
  /**------------------------------------------------------------------------
   * CONTAIN INFO ABOUT ALL NODES IN CLUSTER. NODE_PTR ARE USED AS NODE NUMBER
   * IF THE STATE ARE ZDELETE THEN THE NODE DOESN'T EXIST. NODES ARE ALLOWED
   * TO REGISTER (ZADD) DURING RESTART.
   *------------------------------------------------------------------------*/
  NdbBlocksRec *ndbBlocksRec;

  /*
    2.4 COMMON STORED VARIABLES
  */
  UintR cnoWaitrep6;
  UintR cnoWaitrep7;
  UintR ctcReqInfo;

  Uint8 cstartPhase;
  Uint16 cinternalStartphase;

  bool m_cntr_start_conf;
  Uint16 cmasterNodeId;
  Uint16 cndbBlocksCount;
  Uint16 cnoStartNodes;
  UintR cnoWaitrep;
  NodeState::StartType ctypeOfStart;
  NodeState::StartType cdihStartType;
  Uint16 cdynamicNodeId;

  Uint32 c_fsRemoveCount;
  Uint32 c_nodeGroup;
  void clearFilesystem(Signal *signal);
  void execFSREMOVECONF(Signal *signal);

  NdbNodeBitmask c_allDefinedNodes;
  NdbNodeBitmask c_clusterNodes;  // All members of qmgr cluster
  /**
   * c_cntr_startedNodeSet contains the nodes that have been allowed
   * to start in CNTR_START_CONF. This is establised in phase 2 of the
   * start.
   *
   * c_startedNodeSet contains the nodes that have completed the
   * start and passed all start phases.
   */
  NdbNodeBitmask c_cntr_startedNodeSet;
  NdbNodeBitmask c_startedNodeSet;

 public:
  struct StopRecord {
   public:
    StopRecord(Ndbcntr &_cntr) : cntr(_cntr) { stopReq.senderRef = 0; }

    Ndbcntr &cntr;
    StopReq stopReq;              // Signal data
    NDB_TICKS stopInitiatedTime;  // When was the stop initiated

    bool checkNodeFail(Signal *signal);
    void checkTimeout(Signal *signal);
    void checkApiTimeout(Signal *signal);
    void checkTcTimeout(Signal *signal);
    void checkLqhTimeout_1(Signal *signal);
    void checkLqhTimeout_2(Signal *signal);

    BlockNumber number() const { return cntr.number(); }
    EmulatedJamBuffer *jamBuffer() const { return cntr.jamBuffer(); }
    [[noreturn]] void progError(int line, int cause, const char *extra,
                                const char *check) {
      cntr.progError(line, cause, extra, check);
    }

    enum StopNodesStep {
      SR_BLOCK_GCP_START_GCP = 0,
      SR_WAIT_COMPLETE_GCP = 1,
      SR_UNBLOCK_GCP_START_GCP = 2,
      SR_QMGR_STOP_REQ = 3,
      SR_WAIT_NODE_FAILURES = 4,
      SR_CLUSTER_SHUTDOWN = 12
    } m_state;
    SignalCounter m_stop_req_counter;
  };
  bool is_node_started(NodeId);
  bool is_node_starting(NodeId);

 private:
  bool is_nodegroup_starting(Signal *, NodeId);
  void get_node_group_mask(Signal *, NodeId, NdbNodeBitmask &);

  StopRecord c_stopRec;
  friend struct StopRecord;

  struct Missra {
    Missra(Ndbcntr &ref) : cntr(ref) {}

    Uint32 currentBlockIndex;
    Uint32 currentStartPhase;
    Uint32 nextStartPhase[NO_OF_BLOCKS];

    void execSTART_ORD(Signal *signal);
    void execSTTORRY(Signal *signal);
    void sendNextSTTOR(Signal *signal);
    void execREAD_CONFIG_CONF(Signal *signal);
    void sendNextREAD_CONFIG_REQ(Signal *signal);

    BlockNumber number() const { return cntr.number(); }
    EmulatedJamBuffer *jamBuffer() const { return cntr.jamBuffer(); }
    [[noreturn]] void progError(int line, int cause, const char *extra,
                                const char *check) {
      cntr.progError(line, cause, extra, check);
    }
    Ndbcntr &cntr;
  };

  Missra c_missra;
  friend struct Missra;

  void execSTTORRY(Signal *signal);
  void execSTART_ORD(Signal *signal);
  void execREAD_CONFIG_CONF(Signal *);

  void send_restorable_gci_rep_to_backup(Signal *, Uint32);

  bool m_received_wait_all;
  bool m_any_lcp_started;
  bool m_initial_local_lcp_started;
  bool m_local_lcp_started;
  bool m_local_lcp_completed;
  bool m_full_local_lcp_started;
  bool m_distributed_lcp_started;
  bool m_first_distributed_lcp_started;
  bool m_ready_to_cut_log_tail;
  bool m_wait_cut_undo_log_tail;
  bool m_copy_fragment_in_progress;
  Uint32 m_distributed_lcp_id;
  Uint32 m_set_local_lcp_id_reqs;
  Uint32 m_outstanding_wait_lcp;
  Uint32 m_outstanding_wait_cut_redo_log_tail;
  Uint32 m_max_gci_in_lcp;
  Uint32 m_max_keep_gci;
  Uint32 m_max_completed_gci;

  Uint32 m_lcp_id;
  Uint32 m_local_lcp_id;
  RedoStateRep::RedoAlertState m_global_redo_alert_state;
  RedoStateRep::RedoAlertState m_node_redo_alert_state;
  RedoStateRep::RedoAlertState m_redo_alert_state[MAX_NDBMT_LQH_THREADS];

  RedoStateRep::RedoAlertState get_node_redo_alert_state();
  Uint32 send_to_all_lqh(Signal *, Uint32 gsn, Uint32 sig_len);
  Uint32 send_to_all_backup(Signal *, Uint32 gsn, Uint32 sig_len);
  void send_cut_log_tail(Signal *);
  void check_cut_log_tail_completed(Signal *);
  bool is_ready_to_cut_log_tail();
  void sendWAIT_ALL_COMPLETE_LCP_CONF(Signal *);
  void sendLCP_ALL_COMPLETE_CONF(Signal *);
  void sendSTART_FULL_LOCAL_LCP_ORD(Signal *);
  void sendSTART_LOCAL_LCP_ORD(Signal *);
  void sendSET_LOCAL_LCP_ID_CONF(Signal *);
  void sendWriteLocalSysfile_startLcp(Signal *, Uint32);
  void write_local_sysfile_start_lcp_done(Signal *);
  const char *get_restorable_flag_string(Uint32);

  void execCOPY_FRAG_IN_PROGRESS_REP(Signal *);
  void execCOPY_FRAG_NOT_IN_PROGRESS_REP(Signal *);
  void execUNDO_LOG_LEVEL_REP(Signal *);
  void execSTART_LOCAL_LCP_ORD(Signal *);
  void execSET_LOCAL_LCP_ID_REQ(Signal *);
  void execWAIT_ALL_COMPLETE_LCP_REQ(Signal *);
  void execWAIT_COMPLETE_LCP_CONF(Signal *);

  void execSTART_DISTRIBUTED_LCP_ORD(Signal *);
  void execLCP_ALL_COMPLETE_REQ(Signal *);

  void execCUT_UNDO_LOG_TAIL_CONF(Signal *);
  void execCUT_REDO_LOG_TAIL_CONF(Signal *);

  void execRESTORABLE_GCI_REP(Signal *);
};

#undef JAM_FILE_ID

#endif

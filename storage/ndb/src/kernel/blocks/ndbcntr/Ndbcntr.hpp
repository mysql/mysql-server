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

#ifndef NDBCNTR_H
#define NDBCNTR_H


#include <pc.hpp>
#include <SimulatedBlock.hpp>
#include <ndb_limits.h>
#include <signaldata/StopReq.hpp>
#include <signaldata/ResumeReq.hpp>
#include <signaldata/DictTabInfo.hpp>
#include <signaldata/CntrStart.hpp>
#include <signaldata/CheckNodeGroups.hpp>

#include <signaldata/UpgradeStartup.hpp>

#include <NodeState.hpp>
#include <NdbTick.h>

#ifdef NDBCNTR_C
/*
2.1 GLOBAL SYMBOLS
------------------
*/
/*
2.2 LOCAL SYMBOLS
----------------- 
*/
#define ZNO_NDB_BLOCKS 6           /* ACC, DICT, DIH, LQH, TC, TUP         */

#define ZNOT_AVAILABLE 913

//------- OTHERS ---------------------------------------------
#define ZSTARTUP  1
#define ZSHUTDOWN 2

#define ZSIZE_NDB_BLOCKS_REC 16 /* MAX BLOCKS IN NDB                    */
#define ZSIZE_SYSTAB 2048
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
#define ZWAITPOINT_4_1 1
#define ZWAITPOINT_4_2 2
#define ZWAITPOINT_5_1 3
#define ZWAITPOINT_5_2 4
#define ZWAITPOINT_6_1 5
#define ZWAITPOINT_6_2 6
#define ZWAITPOINT_7_1 7
#define ZWAITPOINT_7_2 8
#define ZSYSTAB_VERSION 1
#endif

class Ndbcntr: public SimulatedBlock {
public:
// Records

/* FSREADREQ FSWRITEREQ         */
/**
 * 2.3 RECORDS AND FILESIZES
 * ------------------------------------------------------------
 */

  struct StartRecord {
    StartRecord() {}
    Uint64 m_startTime;
    
    void reset();
    NdbNodeBitmask m_starting;
    NdbNodeBitmask m_waiting; // == (m_withLog | m_withoutLog)
    NdbNodeBitmask m_withLog;
    NdbNodeBitmask m_withoutLog;
    Uint32 m_lastGci;
    Uint32 m_lastGciNodeId;

    Uint64 m_startPartialTimeout;
    Uint64 m_startPartitionedTimeout;
    Uint64 m_startFailureTimeout;
    struct {
      Uint32 m_nodeId;
      Uint32 m_lastGci;
    } m_logNodes[MAX_NDB_NODES];
    Uint32 m_logNodesCount;
  } c_start;
  
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
    const char* name;
    // DictTabInfo
    DictTabInfo::ExtType type;
    Uint32 length;
    bool keyFlag;
    bool nullable;
  };
  struct SysTable {
    const char* name;
    unsigned columnCount;
    const SysColumn* columnList;
    // DictTabInfo
    DictTabInfo::TableType tableType;
    DictTabInfo::FragmentType fragmentType;
    bool tableLoggedFlag;
    // saved table id
    mutable Uint32 tableId;
  };
  struct SysIndex {
    const char* name;
    const SysTable* primaryTable;
    Uint32 columnCount;
    Uint32 columnList[4];
    // DictTabInfo
    DictTabInfo::TableType indexType;
    DictTabInfo::FragmentType fragmentType;
    bool indexLoggedFlag;
    // saved index table id
    mutable Uint32 indexId;
  };
  static const SysTable* g_sysTableList[];
  static const unsigned g_sysTableCount;
  // the system tables
  static const SysTable g_sysTable_SYSTAB_0;
  static const SysTable g_sysTable_NDBEVENTS_0;

public:
  Ndbcntr(Block_context&);
  virtual ~Ndbcntr();

private:
  BLOCK_DEFINES(Ndbcntr);

  // Transit signals
  void execAPI_START_REP(Signal*);
  void execCONTINUEB(Signal* signal);
  void execREAD_NODESCONF(Signal* signal);
  void execREAD_NODESREF(Signal* signal);
  void execCM_ADD_REP(Signal* signal);
  void execCNTR_START_REQ(Signal* signal);
  void execCNTR_START_REF(Signal* signal);
  void execCNTR_START_CONF(Signal* signal);
  void execCNTR_START_REP(Signal* signal);
  void execCNTR_WAITREP(Signal* signal);
  void execNODE_FAILREP(Signal* signal);
  void execSYSTEM_ERROR(Signal* signal);

  // Received signals
  void execDUMP_STATE_ORD(Signal* signal);
  void execREAD_CONFIG_REQ(Signal* signal);
  void execSTTOR(Signal* signal);
  void execTCSEIZECONF(Signal* signal);
  void execTCSEIZEREF(Signal* signal);
  void execTCRELEASECONF(Signal* signal);
  void execTCRELEASEREF(Signal* signal);
  void execTCKEYCONF(Signal* signal);
  void execTCKEYREF(Signal* signal);
  void execTCROLLBACKREP(Signal* signal);
  void execGETGCICONF(Signal* signal);
  void execDIH_RESTARTCONF(Signal* signal);
  void execDIH_RESTARTREF(Signal* signal);
  void execCREATE_TABLE_REF(Signal* signal);
  void execCREATE_TABLE_CONF(Signal* signal);
  void execNDB_STTORRY(Signal* signal);
  void execNDB_STARTCONF(Signal* signal);
  void execREAD_NODESREQ(Signal* signal);
  void execNDB_STARTREF(Signal* signal);

  void execSTOP_PERM_REF(Signal* signal);
  void execSTOP_PERM_CONF(Signal* signal);

  void execSTOP_ME_REF(Signal* signal);
  void execSTOP_ME_CONF(Signal* signal);
  
  void execWAIT_GCP_REF(Signal* signal);
  void execWAIT_GCP_CONF(Signal* signal);

  void execSTOP_REQ(Signal* signal);
  void execSTOP_CONF(Signal* signal);
  void execRESUME_REQ(Signal* signal);

  void execCHANGE_NODE_STATE_CONF(Signal* signal);

  void execABORT_ALL_REF(Signal* signal);
  void execABORT_ALL_CONF(Signal* signal);

  // Statement blocks
  void sendCreateTabReq(Signal* signal, const char* buffer, Uint32 bufLen);
  void startInsertTransactions(Signal* signal);
  void initData(Signal* signal);
  void resetStartVariables(Signal* signal);
  void sendCntrStartReq(Signal* signal);
  void sendCntrStartRef(Signal*, Uint32 nodeId, CntrStartRef::ErrorCode);
  void sendNdbSttor(Signal* signal);
  void sendSttorry(Signal* signal);

  bool trySystemRestart(Signal* signal);
  void startWaitingNodes(Signal* signal);
  CheckNodeGroups::Output checkNodeGroups(Signal*, const NdbNodeBitmask &);
  
  // Generated statement blocks
  void systemErrorLab(Signal* signal, int line);

  void createSystableLab(Signal* signal, unsigned index);
  void crSystab7Lab(Signal* signal);
  void crSystab8Lab(Signal* signal);
  void crSystab9Lab(Signal* signal);

  void startPhase1Lab(Signal* signal);
  void startPhase2Lab(Signal* signal);
  void startPhase3Lab(Signal* signal);
  void startPhase4Lab(Signal* signal);
  void startPhase5Lab(Signal* signal);
  // jump 2 to resync phase counters
  void startPhase8Lab(Signal* signal);
  void startPhase9Lab(Signal* signal);
  void ph2ALab(Signal* signal);
  void ph2CLab(Signal* signal);
  void ph2ELab(Signal* signal);
  void ph2FLab(Signal* signal);
  void ph2GLab(Signal* signal);
  void ph3ALab(Signal* signal);
  void ph4ALab(Signal* signal);
  void ph4BLab(Signal* signal);
  void ph4CLab(Signal* signal);
  void ph5ALab(Signal* signal);
  void ph6ALab(Signal* signal);
  void ph6BLab(Signal* signal);
  void ph7ALab(Signal* signal);
  void ph8ALab(Signal* signal);


  void waitpoint41Lab(Signal* signal);
  void waitpoint51Lab(Signal* signal);
  void waitpoint52Lab(Signal* signal);
  void waitpoint61Lab(Signal* signal);
  void waitpoint71Lab(Signal* signal);

  void updateNodeState(Signal* signal, const NodeState & newState) const ;
  void getNodeGroup(Signal* signal);

  // Initialisation
  void initData();
  void initRecords();

  // Variables
  /**------------------------------------------------------------------------
   * CONTAIN INFO ABOUT ALL NODES IN CLUSTER. NODE_PTR ARE USED AS NODE NUMBER
   * IF THE STATE ARE ZDELETE THEN THE NODE DOESN'T EXIST. NODES ARE ALLOWED 
   * TO REGISTER (ZADD) DURING RESTART.
   *
   * WHEN THE SYSTEM IS RUNNING THE MASTER WILL CHECK IF ANY NODE HAS MADE 
   * A CNTR_MASTERREQ AND TAKE CARE OF THE REQUEST. 
   * TO CONFIRM THE REQ, THE MASTER DEMANDS THAT ALL RUNNING NODES HAS VOTED 
   * FOR THE NEW NODE. 
   * NODE_PTR:MASTER_REQ IS USED DURING RESTART TO LOG 
   * POSTPONED CNTR_MASTERREQ'S 
   *------------------------------------------------------------------------*/
  NdbBlocksRec *ndbBlocksRec;

  /*
    2.4 COMMON STORED VARIABLES
  */
  UintR cgciSystab;
  UintR ckey;
  //UintR csystabId;
  UintR cnoWaitrep6;
  UintR cnoWaitrep7;
  UintR ctcConnectionP;
  UintR ctcReqInfo;
  Uint8 ctransidPhase;
  Uint16 cresponses;

  Uint8 cstartPhase;
  Uint16 cinternalStartphase;

  Uint16 cmasterNodeId;
  Uint16 cndbBlocksCount;
  Uint16 cnoStartNodes;
  UintR cnoWaitrep;
  NodeState::StartType ctypeOfStart;
  Uint16 cdynamicNodeId;

  Uint32 c_fsRemoveCount;
  Uint32 c_nodeGroup;
  void clearFilesystem(Signal* signal);
  void execFSREMOVECONF(Signal* signal);

  NdbNodeBitmask c_allDefinedNodes;
  NdbNodeBitmask c_clusterNodes; // All members of qmgr cluster
  NdbNodeBitmask c_startedNodes; // All cntr started nodes
  
public:
  struct StopRecord {
  public:
    StopRecord(Ndbcntr & _cntr) : cntr(_cntr) {
      stopReq.senderRef = 0;
    }

    Ndbcntr & cntr;
    StopReq stopReq;          // Signal data
    NDB_TICKS stopInitiatedTime; // When was the stop initiated
    
    bool checkNodeFail(Signal* signal);
    void checkTimeout(Signal* signal);
    void checkApiTimeout(Signal* signal);
    void checkTcTimeout(Signal* signal);
    void checkLqhTimeout_1(Signal* signal);
    void checkLqhTimeout_2(Signal* signal);
    
    BlockNumber number() const { return cntr.number(); }
    void progError(int line, int cause, const char * extra) { 
      cntr.progError(line, cause, extra); 
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
private:
  StopRecord c_stopRec;
  friend struct StopRecord;

  struct Missra {
    Missra(Ndbcntr & ref) : cntr(ref) { }

    Uint32 currentBlockIndex;
    Uint32 currentStartPhase;
    Uint32 nextStartPhase[NO_OF_BLOCKS];

    void execSTART_ORD(Signal* signal);
    void execSTTORRY(Signal* signal);
    void sendNextSTTOR(Signal* signal);
    void execREAD_CONFIG_CONF(Signal* signal);
    void sendNextREAD_CONFIG_REQ(Signal* signal);
    
    BlockNumber number() const { return cntr.number(); }
    void progError(int line, int cause, const char * extra) { 
      cntr.progError(line, cause, extra); 
    }
    Ndbcntr & cntr;
  };

  Missra c_missra;
  friend struct Missra;

  void execSTTORRY(Signal* signal);
  void execSTART_ORD(Signal* signal);
  void execREAD_CONFIG_CONF(Signal*);

  friend struct UpgradeStartup;
};

#endif

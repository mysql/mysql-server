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

#ifndef NDBCNTR_H
#define NDBCNTR_H


#include <pc.hpp>
#include <SimulatedBlock.hpp>
#include <ndb_limits.h>
#include <signaldata/CmvmiCfgConf.hpp>
#include <signaldata/StopReq.hpp>
#include <signaldata/ResumeReq.hpp>
#include <signaldata/DictTabInfo.hpp>

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
#define ZDELAY_NODERESTART 5       /* MASTER REFUSED NODERESTART, WAIT SEC */
#define ZDELAY_START 25            /* WAIT SECONDS FOR OTHER NODES, RESTART*/
#define ZHB_INTERVAL 10            /* HEART BEAT INTERVAL TIME MS          */
#define ZNO_NDB_NODES 1
#define ZHB_TYPE 1

//------- ERROR CODES -----------------------------------------
#define ZERROR_ALREADY_EXISTS 901
#define ZERROR_DOESNT_EXIST 902
#define ZERROR_VALUE_OUT_OF_RANGE 903
#define ZERROR_NOT_STARTED 904
#define ZERROR_NODE_RESTART 905
#define ZERROR_NO_RESTART_NODES 906
#define ZERROR_STARTPHASE_VALUE 907
#define ZERROR_CNTR_MASTERREQ 908
#define ZERROR_CNTR_MASTERREF 909
#define ZERROR_CNTR_MASTERCONF 910
#define ZERROR_NOT_RUNNING 911
#define ZERROR_CNTR_WAITREP 912
#define ZNOT_AVAILABLE 913
#define ZERROR_DISK_FAILURE 914
#define ZERROR_TOO_MANY_NODES 915
#define ZERROR_TYPEOFSTART 916
#define ZERROR_CTYPE_OF_START 917
#define ZERROR_ZSTART 918
#define ZERROR_AT_SELECT_START 919
#define ZERROR_CNTR_CHANGEREP 920
#define ZERR_DETECT_NODERESTART_1 921
#define ZERR_DETECT_NODERESTART_2 922
#define ZERROR_CONTINUEB 923
#define ZERR_APPL_REGREF 924
#define ZERR_DICTADDATTRREF 925
#define ZERR_DICTPREPAREREF 926
#define ZERR_DICTRELEASEREF 927
#define ZERR_DICTTABREF 928
#define ZERR_DICTSEIZEREF 929
#define ZERR_NDB_STARTREF 930
#define ZERR_NODE_STATESREF 931
#define ZERR_READ_NODESREF 932
#define ZERR_TCKEYREF 933
#define ZERR_TCRELEASEREF 934
#define ZERR_TCSEIZEREF 935
#define ZNOTVALIDSTATE_1 936
#define ZNOTVALIDSTATE_2 937
#define ZNODE_FAILURE_DURING_RESTART 938
#define ZSTART_IN_PROGRESS_ERROR 939
#define ZCOULD_NOT_OCCUR_ERROR 940
#define ZTOO_MANY_NODES_ERROR 941
#define ZNODE_RESTART_ONGOING_ERROR 942
#define ZWE_ARE_DECLARED_DEAD_ERROR 943
#define ZTIME_OUT_ERROR 944
#define ZTYPE_OF_START_ERROR 945
#define ZMASTER_CONFLICT_ERROR 946
#define ZNODE_CONFLICT_ERROR 947

//------- OTHERS ---------------------------------------------
#define ZCONTINUEB_1 1
#define ZCONTINUEB_2 2
#define ZSHUTDOWN    3

#define ZAPPL_SUBTYPE 0
#define ZNAME_OF_APPL "NDB"
#define ZVOTING 2
#define ZSIZE_CFG_BLOCK_REC 8
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
/*
------- SIGNAL CONSTANTS -----------------------------------
*/
#define ZNOT_MASTER 0
/* REASON OF CNTR_MASTERREF     */
#define ZTOO_FEW_NODES 1
#define ZNEW_MASTER 0
#define ZDELETE_NODE 1
#define ZSYSTAB_EXIST 3
#define ZVOTE_NEW_NODE 4
#define ZVARIABLE_NO 1
#define ZAMOUNT_PAGES 8
#endif

class Ndbcntr: public SimulatedBlock {
public:
  // State values
  enum State {
    NOT_ACTIVE = 0,
    ACTIVE = 1
  };

// Records

/* FSREADREQ FSWRITEREQ         */
/**
 * 2.3 RECORDS AND FILESIZES
 * ------------------------------------------------------------
 */

/**
 * CFG_BLOCK_REC CONTAINS ALL CONFIG DATA SENT IN EACH NDB_STTOR 
 *               SOME OTHER CONFIG DATA IS STORED IN RECORD 0
 *
 * WHEN CFG_BLOCK_PTR = ZSTART_PHASE_X   ( CINTERNAL_STARTPHASE ) 
 *       WORD 0  DICT_1   
 *       WORD 1  DICT_2   
 *       WORD 2  DIH_1    
 *       WORD 3  DIH_2    
 *       WORD 4  LQH_1    
 *       WORD 5  LQH_2    
 *       WORD 6  TC_1     
 *       WORD 7  TC_2     
 *       WORD 8  TUP_1    
 *       WORD 9  TUP_2    
 *       WORD 10 ACC_1    
 *       WORD 11 ACC_2    
 *                        
 *       CFG_BLOCK_PTR = 0
 *       WORD 0  CDELAY_START
 *       WORD 1  CDELAY_NODERESTART
 *------------------------------------------------------------------------*/
  struct CfgBlockRec {
    UintR cfgData[CmvmiCfgConf::NO_OF_WORDS];
  }; /* p2c: size = 64 bytes */
  
  typedef Ptr<CfgBlockRec> CfgBlockRecPtr;

/*------------------------------------------------------------------------*/
// CONTAIN INFO ABOUT ALL NODES IN CLUSTER. NODE_PTR ARE USED AS NODE NUMBER.
// IF THE STATE ARE ZDELETE THEN THE NODE DOESN'T EXIST. NODES ARE ALLOWED
// TO REGISTER (ZADD) DURING RESTART.
// WHEN THE SYSTEM IS RUNNING THE MASTER WILL CHECK IF ANY NODE HAS MADE A 
// CNTR_MASTERREQ AND TAKE CARE OF THE
// REQUEST. TO CONFIRM THE REQ, THE MASTER DEMANDS THAT ALL RUNNING NODES HAS
// VOTED FOR THE NEW NODE.
// NODE_PTR:MASTER_REQ IS USED DURING RESTART TO LOG POSTPONED
// CNTR_MASTERREQ'S
/*------------------------------------------------------------------------*/
  struct NodeRec {
    UintR dynamicId;
    BlockReference cntrBlockref;
    Uint16 masterReq;
    Uint16 state;
    Uint16 ndbVersion;
    Uint16 subType;
    Uint8 votes;
    Uint8 voter;
    UintR nodeDefined;
  }; /* p2c: size = 16 bytes */

  typedef Ptr<NodeRec> NodeRecPtr;

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
  Ndbcntr(const class Configuration &);
  virtual ~Ndbcntr();

private:
  BLOCK_DEFINES(Ndbcntr);

  // Transit signals
  void execCONTINUEB(Signal* signal);
  void execREAD_NODESCONF(Signal* signal);
  void execREAD_NODESREF(Signal* signal);
  void execCNTR_MASTERREQ(Signal* signal);
  void execCNTR_MASTERCONF(Signal* signal);
  void execCNTR_MASTERREF(Signal* signal);
  void execCNTR_WAITREP(Signal* signal);
  void execNODE_STATESREQ(Signal* signal);
  void execNODE_STATESCONF(Signal* signal);
  void execNODE_STATESREF(Signal* signal);
  void execNODE_FAILREP(Signal* signal);
  void execSYSTEM_ERROR(Signal* signal);
  void execVOTE_MASTERORD(Signal* signal);

  // Received signals
  void execDUMP_STATE_ORD(Signal* signal);
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
  void execAPPL_REGCONF(Signal* signal);
  void execAPPL_REGREF(Signal* signal);
  void execAPPL_CHANGEREP(Signal* signal);
  void execAPPL_STARTCONF(Signal* signal);
  void execNDB_STARTREF(Signal* signal);
  void execCMVMI_CFGCONF(Signal* signal);
  void execSET_VAR_REQ(Signal* signal);

  void execSTOP_PERM_REF(Signal* signal);
  void execSTOP_PERM_CONF(Signal* signal);

  void execSTOP_ME_REF(Signal* signal);
  void execSTOP_ME_CONF(Signal* signal);
  
  void execWAIT_GCP_REF(Signal* signal);
  void execWAIT_GCP_CONF(Signal* signal);

  void execSTOP_REQ(Signal* signal);
  void execRESUME_REQ(Signal* signal);

  void execCHANGE_NODE_STATE_CONF(Signal* signal);

  void execABORT_ALL_REF(Signal* signal);
  void execABORT_ALL_CONF(Signal* signal);

  // Statement blocks
  void sendCreateTabReq(Signal* signal, const char* buffer, Uint32 bufLen);
  void startInsertTransactions(Signal* signal);
  UintR checkNodelist(Signal* signal, Uint16 TnoRestartNodes);
  void chooseRestartNodes(Signal* signal);
  void copyCfgVariables(Signal* signal);
  void deleteNode(Signal* signal);
  void detectNoderestart(Signal* signal);
  void getStartNodes(Signal* signal);
  void initData(Signal* signal);
  void replyMasterconfToAll(Signal* signal);
  void resetStartVariables(Signal* signal);
  void sendCntrMasterreq(Signal* signal);
  void sendNdbSttor(Signal* signal);
  void sendSttorry(Signal* signal);

  // Generated statement blocks
  void systemErrorLab(Signal* signal);

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

  void masterreq010Lab(Signal* signal,
                       Uint16 TnoRestartNodes,
                       Uint16 TuserNodeId);
  void masterreq020Lab(Signal* signal);
  void masterreq030Lab(Signal* signal,
                       Uint16 TnoRestartNodes,
                       Uint16 TuserNodeId);

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
  CfgBlockRec *cfgBlockRec;
  NodeRec *nodeRec;
  NdbBlocksRec *ndbBlocksRec;
  NodeRecPtr nodePtr;
/*
2.4 COMMON STORED VARIABLES
*/
  Uint16 cstartNodes[MAX_NDB_NODES];
  BlockReference ccmvmiBlockref;
  BlockReference cqmgrBlockref;
  BlockReference cdictBlockref;
  BlockReference cdihBlockref;
  BlockReference clqhBlockref;
  BlockReference cownBlockref;
  BlockReference ctcBlockref;
  UintR cnoNdbNodes;
  UintR capplStartconfFlag;
  UintR cgciSystab;
  UintR ckey;
  UintR cnoRegNodes;
  UintR cnoRunNodes;
  UintR cnoNeedNodes;
  UintR cnoWaitrep;
  UintR cnoWaitrep6;
  UintR cnoWaitrep7;
  //UintR csystabId;
  UintR ctcConnectionP;
  UintR ctcReqInfo;
  UintR clastGci;
  UintR cmasterLastGci;
  UintR cmasterCurrentId;
  Uint16 cmasterDihId;
  Uint16 cresponses;
  Uint16 cdelayStart;

  Uint16 cinternalStartphase;
  Uint16 cmasterNodeId;
  Uint16 cmasterCandidateId;
  Uint16 cndbBlocksCount;
  Uint16 cnoStartNodes;
  Uint16 cnoVoters;
  Uint16 cstartProgressFlag;
  Uint16 cqmgrConnectionP;
  Uint16 csignalKey;
  NodeState::StartType ctypeOfStart;
  Uint16 cmasterVoters;
  Uint16 cdynamicNodeId;
  Uint8 cwaitContinuebFlag;
  Uint8 cstartPhase;
  Uint8 ctransidPhase;

  Uint32 c_fsRemoveCount;
  Uint32 c_nodeGroup;
  void clearFilesystem(Signal* signal);
  void execFSREMOVEREF(Signal* signal);
  void execFSREMOVECONF(Signal* signal);
  
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
};

#endif

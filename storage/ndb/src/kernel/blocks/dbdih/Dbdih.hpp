/*
   Copyright (c) 2003, 2010, Oracle and/or its affiliates. All rights reserved.

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

#ifndef DBDIH_H
#define DBDIH_H

#include <ndb_limits.h>
#include <pc.hpp>
#include <SimulatedBlock.hpp>
#include "Sysfile.hpp"
#include <SignalCounter.hpp>

#include <signaldata/MasterLCP.hpp>
#include <signaldata/CopyGCIReq.hpp>
#include <blocks/mutexes.hpp>
#include <signaldata/LCP.hpp>
#include <NdbSeqLock.hpp>

#ifdef DBDIH_C

/*###################*/
/* FILE SYSTEM FLAGS */
/*###################*/
#define ZLIST_OF_PAIRS 0
#define ZLIST_OF_PAIRS_SYNCH 16
#define ZOPEN_READ_WRITE 2
#define ZCREATE_READ_WRITE 0x302
#define ZCLOSE_NO_DELETE 0
#define ZCLOSE_DELETE 1

/*###############*/
/* NODE STATES   */
/*###############*/
#define ZIDLE 0
#define ZACTIVE 1

/*#########*/
/* GENERAL */
/*#########*/
#define ZVAR_NO_WORD 1
#define ZVAR_NO_CRESTART_INFO 20
#define ZVAR_NO_CRESTART_INFO_TO_FILE 21
#define ZVALID 1
#define ZINVALID 2

/*###############*/
/*  ERROR CODES  */
/*###############*/
// ------------------------------------------
// Error Codes for Transactions (None sofar)
// ------------------------------------------
#define ZUNDEFINED_FRAGMENT_ERROR 311

// --------------------------------------
// Error Codes for Add Table
// --------------------------------------
#define ZREPLERROR1 306
#define ZREPLERROR2 307

// --------------------------------------
// Crash Codes
// --------------------------------------
#define ZCOULD_NOT_OCCUR_ERROR 300
#define ZNOT_MASTER_ERROR 301
#define ZWRONG_FAILURE_NUMBER_ERROR 302
#define ZWRONG_START_NODE_ERROR 303
#define ZNO_REPLICA_FOUND_ERROR 304

// --------------------------------------
// Codes from LQH
// --------------------------------------
#define ZNODE_FAILURE_ERROR 400


/*#########*/
/* PHASES  */
/*#########*/
#define ZNDB_SPH1 1
#define ZNDB_SPH2 2
#define ZNDB_SPH3 3
#define ZNDB_SPH4 4
#define ZNDB_SPH5 5
#define ZNDB_SPH6 6
#define ZNDB_SPH7 7
#define ZNDB_SPH8 8
/*#########*/
/* SIZES   */
/*#########*/
#define ZPAGEREC 100
#define ZCREATE_REPLICA_FILE_SIZE 4
#define ZPROXY_MASTER_FILE_SIZE 10
#define ZPROXY_FILE_SIZE 10
#endif

class Dbdih: public SimulatedBlock {
#ifdef ERROR_INSERT
  typedef void (Dbdih::* SendFunction)(Signal*, Uint32, Uint32);
#endif
public:

  // Records

  /*いいいいいいいいいいいいいいいいいいいいいいいいいいいいいいいいいいいい
   * THE API CONNECT RECORD IS THE SAME RECORD POINTER AS USED IN THE TC BLOCK
   *
   *  IT KEEPS TRACK OF ALL THE OPERATIONS CONNECTED TO THIS TRANSACTION.
   *  IT IS LINKED INTO A QUEUE IN CASE THE GLOBAL CHECKPOINT IS CURRENTLY 
   * ONGOING */
  struct ApiConnectRecord {
    Uint64 apiGci;
    Uint32 senderData;
  };
  typedef Ptr<ApiConnectRecord> ApiConnectRecordPtr;

  /*############## CONNECT_RECORD ##############*/
  /*いいいいいいいいいいいいいいいいいいいいいいいいいいいいいいいいいいいい*/
  /* THE CONNECT RECORD IS CREATED WHEN A TRANSACTION HAS TO START. IT KEEPS 
     ALL INTERMEDIATE INFORMATION NECESSARY FOR THE TRANSACTION FROM THE 
     DISTRIBUTED MANAGER. THE RECORD KEEPS INFORMATION ABOUT THE
     OPERATIONS THAT HAVE TO BE CARRIED OUT BY THE TRANSACTION AND
     ALSO THE TRAIL OF NODES FOR EACH OPERATION IN THE THE
     TRANSACTION. 
  */
  struct ConnectRecord {
    enum ConnectState {
      INUSE = 0,
      FREE = 1,
      STARTED = 2,
      ALTER_TABLE = 3,
      ALTER_TABLE_ABORT = 4, // "local" abort
      ALTER_TABLE_REVERT = 5,
      GET_TABINFO = 6
    };
    union {
      Uint32 nodes[MAX_REPLICAS];
      struct {
        Uint32 m_changeMask;
        Uint32 m_totalfragments;
        Uint32 m_org_totalfragments;
        Uint32 m_new_map_ptr_i;
      } m_alter;
      struct {
        Uint32 m_map_ptr_i;
      } m_create;
      struct {
        Uint32 m_requestInfo;
      } m_get_tabinfo;
    };
    ConnectState connectState;
    Uint32 nextPool;
    Uint32 table;
    Uint32 userpointer;
    BlockReference userblockref;
    Callback m_callback;
  };
  typedef Ptr<ConnectRecord> ConnectRecordPtr;

  /*いいいいいいいいいいいいいいいいいいいいいいいいいいいいいいいいいいいい*/
  /*       THESE RECORDS ARE USED WHEN CREATING REPLICAS DURING SYSTEM      */
  /*       RESTART. I NEED A COMPLEX DATA STRUCTURE DESCRIBING THE REPLICAS */
  /*       I WILL TRY TO CREATE FOR EACH FRAGMENT.                          */
  /*                                                                        */
  /*       I STORE A REFERENCE TO THE FOUR POSSIBLE CREATE REPLICA RECORDS  */
  /*       IN A COMMON STORED VARIABLE. I ALLOW A MAXIMUM OF 4 REPLICAS TO  */
  /*       BE RESTARTED PER FRAGMENT.                                       */
  /*いいいいいいいいいいいいいいいいいいいいいいいいいいいいいいいいいいいい*/
  struct CreateReplicaRecord {
    Uint32 logStartGci[MAX_LOG_EXEC];
    Uint32 logStopGci[MAX_LOG_EXEC];
    Uint16 logNodeId[MAX_LOG_EXEC];
    Uint32 createLcpId;

    Uint32 replicaRec;
    Uint16 dataNodeId;
    Uint16 lcpNo;
    Uint16 noLogNodes;
  };
  typedef Ptr<CreateReplicaRecord> CreateReplicaRecordPtr;

  /*いいいいいいいいいいいいいいいいいいいいいいいいいいいいいいいいいいいい*/
  /*       THIS RECORD CONTAINS A FILE DESCRIPTION. THERE ARE TWO           */
  /*       FILES PER TABLE TO RAISE SECURITY LEVEL AGAINST DISK CRASHES.    */ 
  /*いいいいいいいいいいいいいいいいいいいいいいいいいいいいいいいいいいいい*/
  struct FileRecord {
    enum FileStatus {
      CLOSED = 0,
      CRASHED = 1,
      OPEN = 2
    };
    enum FileType {
      TABLE_FILE = 0,
      GCP_FILE = 1
    };
    enum ReqStatus {
      IDLE = 0,
      CREATING_GCP = 1,
      OPENING_GCP = 2,
      OPENING_COPY_GCI = 3,
      WRITING_COPY_GCI = 4,
      CREATING_COPY_GCI = 5,
      OPENING_TABLE = 6,
      READING_GCP = 7,
      READING_TABLE = 8,
      WRITE_INIT_GCP = 9,
      TABLE_CREATE = 10,
      TABLE_WRITE = 11,
      TABLE_CLOSE = 12,
      CLOSING_GCP = 13,
      CLOSING_TABLE_CRASH = 14,
      CLOSING_TABLE_SR = 15,
      CLOSING_GCP_CRASH = 16,
      TABLE_OPEN_FOR_DELETE = 17,
      TABLE_CLOSE_DELETE = 18
    };
    Uint32 fileName[4];
    Uint32 fileRef;
    FileStatus fileStatus;
    FileType fileType;
    Uint32 nextFile;
    ReqStatus reqStatus;
    Uint32 tabRef;
  };
  typedef Ptr<FileRecord> FileRecordPtr;

  /*いいいいいいいいいいいいいいいいいいいいいいいいいいいいいいいいいいいい*/
  /* THIS RECORD KEEPS THE STORAGE AND DECISIONS INFORMATION OF A FRAGMENT  */
  /* AND ITS REPLICAS. IF FRAGMENT HAS MORE THAN ONE BACK UP                */
  /* REPLICA THEN A LIST OF MORE NODES IS ATTACHED TO THIS RECORD.          */
  /* EACH RECORD IN MORE LIST HAS INFORMATION ABOUT ONE BACKUP. THIS RECORD */
  /* ALSO HAVE THE STATUS OF THE FRAGMENT.                                  */
  /*いいいいいいいいいいいいいいいいいいいいいいいいいいいいいいいいいいいい*/
  /*                                                                        */
  /*       FRAGMENTSTORE RECORD ALIGNED TO BE 64 BYTES                      */
  /*いいいいいいいいいいいいいいいいいいいいいいいいいいいいいいいいいいいい*/
  struct Fragmentstore {
    Uint16 activeNodes[MAX_REPLICAS];
    Uint32 preferredPrimary;

    Uint32 oldStoredReplicas;    /* "DEAD" STORED REPLICAS */
    Uint32 storedReplicas;       /* "ALIVE" STORED REPLICAS */
    Uint32 nextFragmentChunk;
    
    Uint32 m_log_part_id;
    
    Uint8 distributionKey;
    Uint8 fragReplicas;
    Uint8 noOldStoredReplicas;  /* NUMBER OF "DEAD" STORED REPLICAS */
    Uint8 noStoredReplicas;     /* NUMBER OF "ALIVE" STORED REPLICAS*/
    Uint8 noLcpReplicas;        ///< No of replicas remaining to be LCP:ed
  };
  typedef Ptr<Fragmentstore> FragmentstorePtr;

  /*########### PAGE RECORD ############*/
  /*いいいいいいいいいいいいいいいいいいいいいいいいいいいいいいいいいい*/
  /*       THIS RECORD KEEPS INFORMATION ABOUT NODE GROUPS.             */
  /*いいいいいいいいいいいいいいいいいいいいいいいいいいいいいいいいいい*/
  struct NodeGroupRecord {
    Uint32 nodesInGroup[MAX_REPLICAS + 1];
    Uint32 nextReplicaNode;
    Uint32 nodeCount;
    Uint32 activeTakeOver; // Which node...
    Uint32 m_next_log_part;
    Uint32 nodegroupIndex;
    Uint32 m_ref_count;
  };
  typedef Ptr<NodeGroupRecord> NodeGroupRecordPtr;
  /*いいいいいいいいいいいいいいいいいいいいいいいいいいいいいいいいいい*/
  /*       THIS RECORD KEEPS INFORMATION ABOUT NODES.                   */
  /*いいいいいいいいいいいいいいいいいいいいいいいいいいいいいいいいいい*/
  /*       RECORD ALIGNED TO BE 64 BYTES.                               */
  /*いいいいいいいいいいいいいいいいいいいいいいいいいいいいいいいいいい*/
  enum NodefailHandlingStep {
    NF_REMOVE_NODE_FROM_TABLE = 1,
    NF_GCP_TAKE_OVER = 2,
    NF_LCP_TAKE_OVER = 4
  };
  
  struct NodeRecord {
    NodeRecord();
    
    enum NodeStatus {
      NOT_IN_CLUSTER = 0,
      ALIVE = 1,
      STARTING = 2,
      DIED_NOW = 3,
      DYING = 4,
      DEAD = 5
    };      

    struct FragmentCheckpointInfo {
      Uint32 tableId;
      Uint32 fragId;
      Uint32 replicaPtr;
    };
    
    Sysfile::ActiveStatus activeStatus;
    
    NodeStatus nodeStatus;
    bool useInTransactions;
    bool allowNodeStart;
    bool m_inclDihLcp;
    Uint8 copyCompleted; // 0 = NO :-), 1 = YES, 2 = yes, first WAITING

    FragmentCheckpointInfo startedChkpt[2];
    FragmentCheckpointInfo queuedChkpt[2];

    Bitmask<1> m_nodefailSteps;
    Uint32 activeTabptr;
    Uint32 nextNode;
    Uint32 nodeGroup;

    SignalCounter m_NF_COMPLETE_REP;
    
    Uint8 dbtcFailCompleted;
    Uint8 dblqhFailCompleted;
    Uint8 dbdihFailCompleted;
    Uint8 dbdictFailCompleted;
    Uint8 recNODE_FAILREP;
    
    Uint8 noOfQueuedChkpt;
    Uint8 noOfStartedChkpt;

    MasterLCPConf::State lcpStateAtTakeOver;
    Uint32 m_remove_node_from_table_lcp_id;
  };
  typedef Ptr<NodeRecord> NodeRecordPtr;
  /**********************************************************************/
  /* THIS RECORD KEEPS THE INFORMATION ABOUT A TABLE AND ITS FRAGMENTS  */
  /**********************************************************************/
  struct PageRecord {
    Uint32 word[2048];
    /* 8 KBYTE PAGE*/
    Uint32 nextfreepage;
  };
  typedef Ptr<PageRecord> PageRecordPtr;

  /************ REPLICA RECORD *************/
  /**********************************************************************/
  /* THIS RECORD KEEPS THE INFORMATION ABOUT A REPLICA OF A FRAGMENT    */
  /**********************************************************************/
  struct ReplicaRecord {
    /* -------------------------------------------------------------------- */
    /* THE GLOBAL CHECKPOINT IDENTITY WHEN THIS REPLICA WAS CREATED.        */
    /* THERE IS ONE INDEX PER REPLICA. A REPLICA INDEX IS CREATED WHEN ANODE*/
    /* CRASH OCCURS.                                                        */
    /* -------------------------------------------------------------------- */
    Uint32 createGci[8];
    /* -------------------------------------------------------------------- */
    /* THE LAST GLOBAL CHECKPOINT IDENTITY WHICH HAS BEEN SAVED ON DISK.    */
    /* THIS VARIABLE IS ONLY VALID FOR REPLICAS WHICH HAVE "DIED". A REPLICA*/
    /* "DIES" EITHER WHEN THE NODE CRASHES THAT KEPT THE REPLICA OR BY BEING*/
    /* STOPPED IN A CONTROLLED MANNER.                                      */
    /* THERE IS ONE INDEX PER REPLICA. A REPLICA INDEX IS CREATED WHEN ANODE*/
    /* CRASH OCCURS.                                                        */
    /* -------------------------------------------------------------------- */
    Uint32 replicaLastGci[8];
    /* -------------------------------------------------------------------- */
    /* THE LOCAL CHECKPOINT IDENTITY OF A LOCAL CHECKPOINT.                 */
    /* -------------------------------------------------------------------- */
    Uint32 lcpId[MAX_LCP_STORED];
    /* -------------------------------------------------------------------- */
    /* THIS VARIABLE KEEPS TRACK OF THE MAXIMUM GLOBAL CHECKPOINT COMPLETED */
    /* FOR EACH OF THE LOCAL CHECKPOINTS IN THIS FRAGMENT REPLICA.          */
    /* -------------------------------------------------------------------- */
    Uint32 maxGciCompleted[MAX_LCP_STORED];
    /* -------------------------------------------------------------------- */
    /* THIS VARIABLE KEEPS TRACK OF THE MINIMUM GLOBAL CHECKPOINT STARTEDFOR*/
    /* EACH OF THE LOCAL CHECKPOINTS IN THIS FRAGMENT REPLICA.              */
    /* -------------------------------------------------------------------- */
    Uint32 maxGciStarted[MAX_LCP_STORED];
    /* -------------------------------------------------------------------- */
    /* THE GLOBAL CHECKPOINT IDENTITY WHEN THE TABLE WAS CREATED.           */
    /* -------------------------------------------------------------------- */
    Uint32 initialGci;

    /* -------------------------------------------------------------------- */
    /* THE REFERENCE TO THE NEXT REPLICA. EITHER IT REFERS TO THE NEXT IN   */
    /* THE FREE LIST OR IT REFERS TO THE NEXT IN A LIST OF REPLICAS ON A    */
    /* FRAGMENT.                                                            */
    /* -------------------------------------------------------------------- */
    Uint32 nextReplica;

    /* -------------------------------------------------------------------- */
    /*       THE NODE ID WHERE THIS REPLICA IS STORED.                      */
    /* -------------------------------------------------------------------- */
    Uint16 procNode;

    /* -------------------------------------------------------------------- */
    /*    The last local checkpoint id started or queued on this replica.   */
    /* -------------------------------------------------------------------- */
    union {
      Uint32 lcpIdStarted;   // Started or queued
      Uint32 m_restorable_gci;
    };

    /* -------------------------------------------------------------------- */
    /* THIS VARIABLE SPECIFIES WHAT THE STATUS OF THE LOCAL CHECKPOINT IS.IT*/
    /* CAN EITHER BE VALID OR INVALID. AT CREATION OF A FRAGMENT REPLICA ALL*/
    /* LCP'S ARE INVALID. ALSO IF IF INDEX >= NO_LCP THEN THELOCALCHECKPOINT*/
    /* IS ALWAYS INVALID. IF THE LCP BEFORE THE NEXT_LCP HAS LCP_ID THAT    */
    /* DIFFERS FROM THE LATEST LCP_ID STARTED THEN THE NEXT_LCP IS ALSO     */
    /* INVALID */
    /* -------------------------------------------------------------------- */
    Uint8 lcpStatus[MAX_LCP_STORED];

    /* -------------------------------------------------------------------- */
    /*       THE NEXT LOCAL CHECKPOINT TO EXECUTE IN THIS FRAGMENT REPLICA. */
    /* -------------------------------------------------------------------- */
    Uint8 nextLcp;

    /* -------------------------------------------------------------------- */
    /*       THE NUMBER OF CRASHED REPLICAS IN THIS REPLICAS SO FAR.        */
    /* -------------------------------------------------------------------- */
    Uint8 noCrashedReplicas;

    /**
     * Is a LCP currently ongoing on fragment
     */
    Uint8 lcpOngoingFlag;
  };
  typedef Ptr<ReplicaRecord> ReplicaRecordPtr;

  /*************************************************************************
   * TAB_DESCRIPTOR IS A DESCRIPTOR OF THE LOCATION OF THE FRAGMENTS BELONGING
   * TO THE TABLE.THE INFORMATION ABOUT FRAGMENTS OF A TABLE ARE STORED IN 
   * CHUNKS OF FRAGMENTSTORE RECORDS.
   * THIS RECORD ALSO HAS THE NECESSARY INFORMATION TO LOCATE A FRAGMENT AND 
   * TO LOCATE A FRAGMENT AND TO TRANSLATE A KEY OF A TUPLE TO THE FRAGMENT IT
   * BELONGS
   */
  struct TabRecord
  {
    TabRecord() { }

    /**
     * rw-lock that protects multiple parallel DIGETNODES (readers) from
     *   updates to fragmenation changes (e.g CREATE_FRAGREQ)...
     *   search for DIH_TAB_WRITE_LOCK
     */
    NdbSeqLock m_lock;

    /**
     * State for copying table description into pages
     */
    enum CopyStatus {
      CS_IDLE,
      CS_SR_PHASE1_READ_PAGES,
      CS_SR_PHASE2_READ_TABLE,
      CS_SR_PHASE3_COPY_TABLE,
      CS_REMOVE_NODE,
      CS_LCP_READ_TABLE,
      CS_COPY_TAB_REQ,
      CS_COPY_NODE_STATE,
      CS_ADD_TABLE_MASTER,
      CS_ADD_TABLE_SLAVE,
      CS_INVALIDATE_NODE_LCP,
      CS_ALTER_TABLE,
      CS_COPY_TO_SAVE
      ,CS_GET_TABINFO
    };
    /**
     * State for copying pages to disk
     */
    enum UpdateState {
      US_IDLE,
      US_LOCAL_CHECKPOINT,
      US_REMOVE_NODE,
      US_COPY_TAB_REQ,
      US_ADD_TABLE_MASTER,
      US_ADD_TABLE_SLAVE,
      US_INVALIDATE_NODE_LCP,
      US_CALLBACK
    };
    enum TabLcpStatus {
      TLS_ACTIVE = 1,
      TLS_WRITING_TO_FILE = 2,
      TLS_COMPLETED = 3
    };
    enum TabStatus {
      TS_IDLE = 0,
      TS_ACTIVE = 1,
      TS_CREATING = 2,
      TS_DROPPING = 3
    };
    enum Method {
      LINEAR_HASH = 0,
      NOTDEFINED = 1,
      NORMAL_HASH = 2,
      USER_DEFINED = 3,
      HASH_MAP = 4
    };
    enum Storage {
      ST_NOLOGGING = 0,         // Table is not logged, but survives SR
      ST_NORMAL = 1,            // Normal table, logged and durable
      ST_TEMPORARY = 2          // Table is lost after SR, not logged
    };
    CopyStatus tabCopyStatus;
    UpdateState tabUpdateState;
    TabLcpStatus tabLcpStatus;
    TabStatus tabStatus;
    Method method;
    Storage tabStorage;

    Uint32 pageRef[32];
//-----------------------------------------------------------------------------
// Each entry in this array contains a reference to 16 fragment records in a
// row. Thus finding the correct record is very quick provided the fragment id.
//-----------------------------------------------------------------------------
    Uint32 startFid[MAX_NDB_NODES * MAX_FRAG_PER_NODE / NO_OF_FRAGS_PER_CHUNK];

    Uint32 tabFile[2];
    Uint32 connectrec;                                    
    union {
      Uint32 hashpointer;
      Uint32 m_new_map_ptr_i;
    };
    union {
      Uint32 mask;
      Uint32 m_map_ptr_i;
    };
    Uint32 noOfWords;
    Uint32 schemaVersion;
    Uint32 tabRemoveNode;
    Uint32 totalfragments;
    Uint32 noOfFragChunks;
    Uint32 m_scan_count[2];
    Uint32 m_scan_reorg_flag;
    Uint32 tabErrorCode;
    struct {
      Uint32 tabUserRef;
      Uint32 tabUserPtr;
    } m_dropTab;

    Uint8 kvalue;
    Uint8 noOfBackups;
    Uint8 noPages;
    Uint16 tableType;
    Uint16 primaryTableId;

    // set in local protocol during prepare until commit
    Uint32 schemaTransId;
  };
  typedef Ptr<TabRecord> TabRecordPtr;

  /***************************************************************************/
  /* THIS RECORD IS USED TO KEEP TRACK OF TAKE OVER AND STARTING A NODE.    */
  /* WE KEEP IT IN A RECORD TO ENABLE IT TO BE PARALLELISED IN THE FUTURE.  */
  /**************************************************************************/
  struct TakeOverRecord {
    
    /**
     * States possible on slave (starting node)
     */
    enum ToSlaveStatus {
      TO_SLAVE_IDLE = 0
      ,TO_START_FRAGMENTS = 1      // Finding LCP for each fragment
      ,TO_RUN_REDO = 2             // Waiting for local LQH to run REDO
      ,TO_START_TO = 3             // Waiting for master (START_TOREQ)
      ,TO_SELECTING_NEXT = 4       // Selecting next fragment to copy
      ,TO_PREPARE_COPY = 5         // Waiting for local LQH (PREPARE_COPYREQ)
      ,TO_UPDATE_BEFORE_STORED = 6 // Waiting on master (UPDATE_TOREQ)
      ,TO_CREATE_FRAG_STORED = 7   // Waiting for all (CREATE_FRAGREQ stored)
      ,TO_UPDATE_AFTER_STORED = 8  // Waiting for master (UPDATE_TOREQ)
      ,TO_COPY_FRAG = 9            // Waiting for copy node (COPY_FRAGREQ)
      ,TO_COPY_ACTIVE = 10         // Waiting for local LQH (COPY_ACTIVEREQ)
      ,TO_UPDATE_BEFORE_COMMIT = 11// Waiting for master (UPDATE_TOREQ)
      ,TO_CREATE_FRAG_COMMIT = 12  // Waiting for all (CREATE_FRAGREQ commit)
      ,TO_UPDATE_AFTER_COMMIT = 13 // Waiting for master (UPDATE_TOREQ)

      ,TO_START_LOGGING = 14        // Enabling logging on all fragments
      ,TO_SL_COPY_ACTIVE = 15       // Start logging: Copy active (local)
      ,TO_SL_CREATE_FRAG = 16       // Start logging: Create Frag (dist)
      ,TO_END_TO = 17               // Waiting for master (EBND_TOREQ)
    };

    /**
     * States possible on master
     */
    enum ToMasterStatus {
      TO_MASTER_IDLE = 0
      ,TO_MUTEX_BEFORE_STORED = 1  // Waiting for lock
      ,TO_MUTEX_BEFORE_LOCKED = 2  // Lock held
      ,TO_AFTER_STORED = 3         // No lock, but NGPtr reservation
      ,TO_MUTEX_BEFORE_COMMIT = 4  // Waiting for lock
      ,TO_MUTEX_BEFORE_SWITCH_REPLICA = 5 // Waiting for switch replica lock
      ,TO_MUTEX_AFTER_SWITCH_REPLICA = 6
      ,TO_WAIT_LCP = 7             // No locks, waiting for LCP
    };
    
    Uint32 m_flags;       // 
    Uint32 m_senderRef;   // Who requested START_COPYREQ
    Uint32 m_senderData;  // Data of sender
    
    Uint32 restorableGci; // Which GCI can be restore "locally" by node
    Uint32 startGci;
    Uint32 maxPage;
    Uint32 toCopyNode;
    Uint32 toCurrentFragid;
    Uint32 toCurrentReplica;
    Uint32 toCurrentTabref;
    Uint32 toFailedNode;
    Uint32 toStartingNode;
    Uint64 toStartTime;
    ToSlaveStatus toSlaveStatus;
    ToMasterStatus toMasterStatus;
   
    MutexHandle2<DIH_SWITCH_PRIMARY_MUTEX> m_switchPrimaryMutexHandle;
    MutexHandle2<DIH_FRAGMENT_INFO> m_fragmentInfoMutex;

    Uint32 nextList;
    union {
      Uint32 prevList;
      Uint32 nextPool;
    };
  };
  typedef Ptr<TakeOverRecord> TakeOverRecordPtr;

  virtual bool getParam(const char * param, Uint32 * retVal) { 
    if (param && strcmp(param, "ActiveMutexes") == 0)
    {
      if (retVal)
      {
        * retVal = 5 + MAX_NDB_NODES;
      }
      return true;
    }
    return false;
  }  
  
public:
  Dbdih(Block_context& ctx);
  virtual ~Dbdih();

  struct RWFragment {
    Uint32 pageIndex;
    Uint32 wordIndex;
    Uint32 fragId;
    TabRecordPtr rwfTabPtr;
    PageRecordPtr rwfPageptr;
    Uint32 totalfragments;
  };
  struct CopyTableNode {
    Uint32 pageIndex;
    Uint32 wordIndex;
    Uint32 noOfWords;
    TabRecordPtr ctnTabPtr;
    PageRecordPtr ctnPageptr;
  };
  
private:
  friend class SimulatedBlock;
  BLOCK_DEFINES(Dbdih);
  
  void execDUMP_STATE_ORD(Signal *);
  void execNDB_TAMPER(Signal *);
  void execDEBUG_SIG(Signal *);
  void execEMPTY_LCP_CONF(Signal *);
  void execEMPTY_LCP_REP(Signal*);
  void execMASTER_GCPREF(Signal *);
  void execMASTER_GCPREQ(Signal *);
  void execMASTER_GCPCONF(Signal *);
  void execMASTER_LCPREF(Signal *);
  void execMASTER_LCPREQ(Signal *);
  void execMASTER_LCPCONF(Signal *);
  void execNF_COMPLETEREP(Signal *);
  void execSTART_PERMREQ(Signal *);
  void execSTART_PERMCONF(Signal *);
  void execSTART_PERMREF(Signal *);
  void execINCL_NODEREQ(Signal *);
  void execINCL_NODECONF(Signal *);

  void execSTART_TOREQ(Signal *);
  void execSTART_TOREF(Signal *);
  void execSTART_TOCONF(Signal*);

  void execEND_TOREQ(Signal *);
  void execEND_TOREF(Signal *);
  void execEND_TOCONF(Signal*);

  void execUPDATE_TOREQ(Signal* signal);
  void execUPDATE_TOREF(Signal* signal);
  void execUPDATE_TOCONF(Signal* signal);

  void execSTART_MEREQ(Signal *);
  void execSTART_MECONF(Signal *);
  void execSTART_MEREF(Signal *);
  void execSTART_COPYREQ(Signal *);
  void execSTART_COPYCONF(Signal *);
  void execSTART_COPYREF(Signal *);
  void execCREATE_FRAGREQ(Signal *);
  void execCREATE_FRAGCONF(Signal *);
  void execDIVERIFYREQ(Signal *);
  void execGCP_SAVEREQ(Signal *);
  void execGCP_SAVECONF(Signal *);
  void execGCP_PREPARECONF(Signal *);
  void execGCP_PREPARE(Signal *);
  void execGCP_NODEFINISH(Signal *);
  void execGCP_COMMIT(Signal *);
  void execSUB_GCP_COMPLETE_REP(Signal *);
  void execSUB_GCP_COMPLETE_ACK(Signal *);
  void execDIHNDBTAMPER(Signal *);
  void execCONTINUEB(Signal *);
  void execCOPY_GCIREQ(Signal *);
  void execCOPY_GCICONF(Signal *);
  void execCOPY_TABREQ(Signal *);
  void execCOPY_TABCONF(Signal *);
  void execTCGETOPSIZECONF(Signal *);
  void execTC_CLOPSIZECONF(Signal *);

  void execDIH_GET_TABINFO_REQ(Signal*);

  int handle_invalid_lcp_no(const struct LcpFragRep*, ReplicaRecordPtr);
  void execLCP_FRAG_REP(Signal *);
  void execLCP_COMPLETE_REP(Signal *);
  void execSTART_LCP_REQ(Signal *);
  void execSTART_LCP_CONF(Signal *);
  MutexHandle2<DIH_START_LCP_MUTEX> c_startLcpMutexHandle;
  void startLcpMutex_locked(Signal* signal, Uint32, Uint32);
  void startLcpMutex_unlocked(Signal* signal, Uint32, Uint32);
  void lcpFragmentMutex_locked(Signal* signal, Uint32, Uint32);
  void master_lcp_fragmentMutex_locked(Signal* signal, Uint32, Uint32);

  MutexHandle2<DIH_SWITCH_PRIMARY_MUTEX> c_switchPrimaryMutexHandle;
  void switchPrimaryMutex_locked(Signal* signal, Uint32, Uint32);
  void switchPrimaryMutex_unlocked(Signal* signal, Uint32, Uint32);
  void check_force_lcp(Ptr<TakeOverRecord> takeOverPtr);

  void switch_primary_stop_node(Signal* signal, Uint32, Uint32);

  void updateToReq_fragmentMutex_locked(Signal*, Uint32, Uint32);

  MutexHandle2<DIH_FRAGMENT_INFO> c_fragmentInfoMutex_lcp;

  void execBLOCK_COMMIT_ORD(Signal *);
  void execUNBLOCK_COMMIT_ORD(Signal *);

  void execDIH_SWITCH_REPLICA_REQ(Signal *);
  void execDIH_SWITCH_REPLICA_REF(Signal *);
  void execDIH_SWITCH_REPLICA_CONF(Signal *);
  
  void execSTOP_PERM_REQ(Signal *);
  void execSTOP_PERM_REF(Signal *);
  void execSTOP_PERM_CONF(Signal *);

  void execSTOP_ME_REQ(Signal *);
  void execSTOP_ME_REF(Signal *);
  void execSTOP_ME_CONF(Signal *);

  void execREAD_CONFIG_REQ(Signal *);
  void execUNBLO_DICTCONF(Signal *);
  void execCOPY_ACTIVECONF(Signal *);
  void execTAB_COMMITREQ(Signal *);
  void execNODE_FAILREP(Signal *);
  void execCOPY_FRAGCONF(Signal *);
  void execCOPY_FRAGREF(Signal *);
  void execPREPARE_COPY_FRAG_REF(Signal*);
  void execPREPARE_COPY_FRAG_CONF(Signal*);
  void execDIADDTABREQ(Signal *);
  void execDIGETNODESREQ(Signal *);
  void execSTTOR(Signal *);
  void execDIH_SCAN_TAB_REQ(Signal *);
  void execDIH_SCAN_GET_NODES_REQ(Signal *);
  void execDIH_SCAN_TAB_COMPLETE_REP(Signal*);
  void execGCP_SAVEREF(Signal *);
  void execGCP_TCFINISHED(Signal *);
  void execGCP_TCFINISHED_sync_conf(Signal* signal, Uint32 cb, Uint32 err);
  void execREAD_NODESCONF(Signal *);
  void execNDB_STTOR(Signal *);
  void execDICTSTARTCONF(Signal *);
  void execNDB_STARTREQ(Signal *);
  void execGETGCIREQ(Signal *);
  void execDIH_RESTARTREQ(Signal *);
  void execSTART_RECCONF(Signal *);
  void execSTART_FRAGREF(Signal *);
  void execSTART_FRAGCONF(Signal *);
  void execADD_FRAGCONF(Signal *);
  void execADD_FRAGREF(Signal *);
  void execDROP_FRAG_REF(Signal *);
  void execDROP_FRAG_CONF(Signal *);
  void execFSOPENCONF(Signal *);
  void execFSOPENREF(Signal *);
  void execFSCLOSECONF(Signal *);
  void execFSCLOSEREF(Signal *);
  void execFSREADCONF(Signal *);
  void execFSREADREF(Signal *);
  void execFSWRITECONF(Signal *);
  void execFSWRITEREF(Signal *);
  void execCHECKNODEGROUPSREQ(Signal *);
  void execSTART_INFOREQ(Signal*);
  void execSTART_INFOREF(Signal*);
  void execSTART_INFOCONF(Signal*);
  void execWAIT_GCP_REQ(Signal* signal);
  void execWAIT_GCP_REF(Signal* signal);
  void execWAIT_GCP_CONF(Signal* signal);

  void execPREP_DROP_TAB_REQ(Signal* signal);
  void execDROP_TAB_REQ(Signal* signal);

  void execALTER_TAB_REQ(Signal* signal);

  void execCREATE_FRAGMENTATION_REQ(Signal*);
  
  void waitDropTabWritingToFile(Signal *, TabRecordPtr tabPtr);
  void checkDropTabComplete(Signal *, TabRecordPtr tabPtr);

  void execDICT_LOCK_CONF(Signal* signal);
  void execDICT_LOCK_REF(Signal* signal);

  void execUPGRADE_PROTOCOL_ORD(Signal* signal);

  void execCREATE_NODEGROUP_IMPL_REQ(Signal*);
  void execDROP_NODEGROUP_IMPL_REQ(Signal*);

  // Statement blocks
//------------------------------------
// Methods that send signals
//------------------------------------
  void nullRoutine(Signal *, Uint32 nodeId, Uint32);
  void sendCOPY_GCIREQ(Signal *, Uint32 nodeId, Uint32);
  void sendDIH_SWITCH_REPLICA_REQ(Signal *, Uint32 nodeId, Uint32);
  void sendEMPTY_LCP_REQ(Signal *, Uint32 nodeId, Uint32);
  void sendEND_TOREQ(Signal *, Uint32 nodeId, Uint32);
  void sendGCP_COMMIT(Signal *, Uint32 nodeId, Uint32);
  void sendGCP_PREPARE(Signal *, Uint32 nodeId, Uint32);
  void sendGCP_SAVEREQ(Signal *, Uint32 nodeId, Uint32);
  void sendSUB_GCP_COMPLETE_REP(Signal*, Uint32 nodeId, Uint32);
  void sendINCL_NODEREQ(Signal *, Uint32 nodeId, Uint32);
  void sendMASTER_GCPREQ(Signal *, Uint32 nodeId, Uint32);
  void sendMASTER_LCPREQ(Signal *, Uint32 nodeId, Uint32);
  void sendMASTER_LCPCONF(Signal * signal);
  void sendSTART_RECREQ(Signal *, Uint32 nodeId, Uint32);
  void sendSTART_INFOREQ(Signal *, Uint32 nodeId, Uint32);
  void sendSTART_TOREQ(Signal *, Uint32 nodeId, Uint32);
  void sendSTOP_ME_REQ(Signal *, Uint32 nodeId, Uint32);
  void sendTC_CLOPSIZEREQ(Signal *, Uint32 nodeId, Uint32);
  void sendTCGETOPSIZEREQ(Signal *, Uint32 nodeId, Uint32);
  void sendUPDATE_TOREQ(Signal *, Uint32 nodeId, Uint32);
  void sendSTART_LCP_REQ(Signal *, Uint32 nodeId, Uint32);

  void sendLCP_FRAG_ORD(Signal*, NodeRecord::FragmentCheckpointInfo info);
  void sendLastLCP_FRAG_ORD(Signal *);
  
  void sendCopyTable(Signal *, CopyTableNode* ctn,
                     BlockReference ref, Uint32 reqinfo);
  void sendCreateFragReq(Signal *,
                         Uint32 startGci,
                         Uint32 storedType,
                         Uint32 takeOverPtr);
  void sendDihfragreq(Signal *,
                      TabRecordPtr regTabPtr,
                      Uint32 fragId);

  void sendStartTo(Signal* signal, TakeOverRecordPtr);
  void sendUpdateTo(Signal* signal, TakeOverRecordPtr);

  void sendStartFragreq(Signal *,
                        TabRecordPtr regTabPtr,
                        Uint32 fragId);
  void sendAddFragreq(Signal *,
                      TabRecordPtr regTabPtr,
                      Uint32 fragId,
                      Uint32 lcpNo,
                      Uint32 param);

  void sendAddFragreq(Signal*, ConnectRecordPtr, TabRecordPtr, Uint32 fragId);
  void addTable_closeConf(Signal* signal, Uint32 tabPtrI);
  void resetReplicaSr(TabRecordPtr tabPtr);
  void resetReplicaLcp(ReplicaRecord * replicaP, Uint32 stopGci);
  void resetReplica(Ptr<ReplicaRecord>);

//------------------------------------
// Methods for LCP functionality
//------------------------------------
  void checkKeepGci(TabRecordPtr, Uint32, Fragmentstore*, Uint32);
  void checkLcpStart(Signal *, Uint32 lineNo);
  void checkStartMoreLcp(Signal *, Uint32 nodeId);
  bool reportLcpCompletion(const struct LcpFragRep *);
  void sendLCP_COMPLETE_REP(Signal *);

//------------------------------------
// Methods for Delete Table Files
//------------------------------------
  void startDeleteFile(Signal* signal, TabRecordPtr tabPtr);
  void openTableFileForDelete(Signal* signal, Uint32 fileIndex);
  void tableOpenLab(Signal* signal, FileRecordPtr regFilePtr);
  void tableDeleteLab(Signal* signal, FileRecordPtr regFilePtr);

//------------------------------------
// File Record specific methods
//------------------------------------
  void closeFile(Signal *, FileRecordPtr regFilePtr);
  void closeFileDelete(Signal *, FileRecordPtr regFilePtr);
  void createFileRw(Signal *, FileRecordPtr regFilePtr);
  void openFileRw(Signal *, FileRecordPtr regFilePtr);
  void openFileRo(Signal *, FileRecordPtr regFilePtr);
  void seizeFile(FileRecordPtr& regFilePtr);
  void releaseFile(Uint32 fileIndex);

//------------------------------------
// Methods called when completing file
// operation.
//------------------------------------
  void creatingGcpLab(Signal *, FileRecordPtr regFilePtr);
  void openingGcpLab(Signal *, FileRecordPtr regFilePtr);
  void openingTableLab(Signal *, FileRecordPtr regFilePtr);
  void tableCreateLab(Signal *, FileRecordPtr regFilePtr);
  void creatingGcpErrorLab(Signal *, FileRecordPtr regFilePtr);
  void openingCopyGciErrorLab(Signal *, FileRecordPtr regFilePtr);
  void creatingCopyGciErrorLab(Signal *, FileRecordPtr regFilePtr);
  void openingGcpErrorLab(Signal *, FileRecordPtr regFilePtr);
  void openingTableErrorLab(Signal *, FileRecordPtr regFilePtr);
  void tableCreateErrorLab(Signal *, FileRecordPtr regFilePtr);
  void closingGcpLab(Signal *, FileRecordPtr regFilePtr);
  void closingGcpCrashLab(Signal *, FileRecordPtr regFilePtr);
  void closingTableCrashLab(Signal *, FileRecordPtr regFilePtr);
  void closingTableSrLab(Signal *, FileRecordPtr regFilePtr);
  void tableCloseLab(Signal *, FileRecordPtr regFilePtr);
  void tableCloseErrorLab(FileRecordPtr regFilePtr);
  void readingGcpLab(Signal *, FileRecordPtr regFilePtr);
  void readingTableLab(Signal *, FileRecordPtr regFilePtr);
  void readingGcpErrorLab(Signal *, FileRecordPtr regFilePtr);
  void readingTableErrorLab(Signal *, FileRecordPtr regFilePtr);
  void writingCopyGciLab(Signal *, FileRecordPtr regFilePtr);
  void writeInitGcpLab(Signal *, FileRecordPtr regFilePtr);
  void tableWriteLab(Signal *, FileRecordPtr regFilePtr);
  void writeInitGcpErrorLab(Signal *, FileRecordPtr regFilePtr);


  void checkEscalation();
  void clearRestartInfoBits(Signal *);
  void invalidateLcpInfoAfterSr(Signal*);

  bool isMaster();
  bool isActiveMaster();

  void handleGcpStateInMaster(Signal *, NodeRecordPtr failedNodeptr);
  void initRestartInfo(Signal*);
  void initRestorableGciFiles();
  void makeNodeGroups(Uint32 nodeArray[]);
  void add_nodegroup(NodeGroupRecordPtr);
  void inc_ng_refcount(Uint32 ng);
  void dec_ng_refcount(Uint32 ng);

  void makePrnList(class ReadNodesConf * readNodes, Uint32 nodeArray[]);
  void nodeResetStart(Signal* signal);
  void releaseTabPages(Uint32 tableId);
  void replication(Uint32 noOfReplicas,
                   NodeGroupRecordPtr NGPtr,
                   FragmentstorePtr regFragptr);
  void sendDihRestartRef(Signal*);
  void selectMasterCandidateAndSend(Signal *);
  void setLcpActiveStatusEnd(Signal*);
  void setLcpActiveStatusStart(Signal *);
  void setNodeActiveStatus();
  void setNodeGroups();
  void setNodeInfo(Signal *);
  void setNodeLcpActiveStatus();
  void setNodeRestartInfoBits(Signal*);
  void startGcp(Signal *);
  void startGcpMonitor(Signal*);

  void readFragment(RWFragment* rf, FragmentstorePtr regFragptr);
  Uint32 readPageWord(RWFragment* rf);
  void readReplica(RWFragment* rf, ReplicaRecordPtr readReplicaPtr);
  void readReplicas(RWFragment* rf, FragmentstorePtr regFragptr);
  void readRestorableGci(Signal *, FileRecordPtr regFilePtr);
  void readTabfile(Signal *, TabRecord* tab, FileRecordPtr regFilePtr);
  void writeFragment(RWFragment* wf, FragmentstorePtr regFragptr);
  void writePageWord(RWFragment* wf, Uint32 dataWord);
  void writeReplicas(RWFragment* wf, Uint32 replicaStartIndex);
  void writeRestorableGci(Signal *, FileRecordPtr regFilePtr);
  void writeTabfile(Signal *, TabRecord* tab, FileRecordPtr regFilePtr);
  void copyTabReq_complete(Signal* signal, TabRecordPtr tabPtr);

  void gcpcommitreqLab(Signal *);
  void copyGciLab(Signal *, CopyGCIReq::CopyReason reason);
  void storeNewLcpIdLab(Signal *);
  void startLcpRoundLoopLab(Signal *, Uint32 startTableId, Uint32 startFragId);

  void nodeFailCompletedCheckLab(Signal*, NodeRecordPtr failedNodePtr);

  /**
   *
   */
  void setLocalNodefailHandling(Signal*, Uint32 failedNodeId,
				NodefailHandlingStep step);
  void checkLocalNodefailComplete(Signal*, Uint32 failedNodeId,
				  NodefailHandlingStep step);
  
  Callback m_sendSTTORRY;
  void sendSTTORRY(Signal*, Uint32 senderData = 0, Uint32 retVal = 0);
  void ndbsttorry10Lab(Signal *, Uint32 _line);
  void createMutexes(Signal* signal, Uint32 no);
  void createMutex_done(Signal* signal, Uint32 no, Uint32 retVal);
  void dumpGcpStop();
  void crashSystemAtGcpStop(Signal *, bool);
  void sendFirstDictfragsreq(Signal *, TabRecordPtr regTabPtr);
  void addtabrefuseLab(Signal *, ConnectRecordPtr regConnectPtr, Uint32 errorCode);
  void GCP_SAVEhandling(Signal *, Uint32 nodeId);
  void packTableIntoPagesLab(Signal *, Uint32 tableId);
  void readPagesIntoTableLab(Signal *, Uint32 tableId);
  void readPagesIntoFragLab(Signal *, RWFragment* rf);
  void readTabDescriptionLab(Signal *, Uint32 tableId);
  void copyTableLab(Signal *, Uint32 tableId);
  void breakCopyTableLab(Signal *,
                         TabRecordPtr regTabPtr,
                         Uint32 nodeId);
  void checkAddfragCompletedLab(Signal *,
                                TabRecordPtr regTabPtr,
                                Uint32 fragId);
  void completeRestartLab(Signal *);
  void readTableFromPagesLab(Signal *, TabRecordPtr regTabPtr);
  void srPhase2ReadTableLab(Signal *, TabRecordPtr regTabPtr);
  void checkTcCounterLab(Signal *);
  void calculateKeepGciLab(Signal *, Uint32 tableId, Uint32 fragId);
  void tableUpdateLab(Signal *, TabRecordPtr regTabPtr);
  void checkLcpCompletedLab(Signal *);
  void initLcpLab(Signal *, Uint32 masterRef, Uint32 tableId);
  void startGcpLab(Signal *, Uint32 aWaitTime);
  void checkGcpStopLab(Signal *);
  void MASTER_GCPhandling(Signal *, Uint32 failedNodeId);
  void MASTER_LCPhandling(Signal *, Uint32 failedNodeId);
  void rnfTableNotReadyLab(Signal *, TabRecordPtr regTabPtr, Uint32 removeNodeId);
  void startLcpTakeOverLab(Signal *, Uint32 failedNodeId);

  void startLcpMasterTakeOver(Signal *, Uint32 failedNodeId);
  void startGcpMasterTakeOver(Signal *, Uint32 failedNodeId);
  void checkGcpOutstanding(Signal*, Uint32 failedNodeId);

  void checkEmptyLcpComplete(Signal *);
  void lcpBlockedLab(Signal *, Uint32, Uint32);
  void breakCheckTabCompletedLab(Signal *, TabRecordPtr regTabptr);
  void readGciFileLab(Signal *);
  void openingCopyGciSkipInitLab(Signal *, FileRecordPtr regFilePtr);
  void startLcpRoundLab(Signal *);
  void gcpBlockedLab(Signal *);
  void initialStartCompletedLab(Signal *);
  void allNodesLcpCompletedLab(Signal *);
  void nodeRestartPh2Lab(Signal *);
  void nodeRestartPh2Lab2(Signal *);
  void initGciFilesLab(Signal *);
  void dictStartConfLab(Signal *);
  void nodeDictStartConfLab(Signal *);
  void ndbStartReqLab(Signal *, BlockReference ref);
  void nodeRestartStartRecConfLab(Signal *);
  void dihCopyCompletedLab(Signal *);
  void release_connect(ConnectRecordPtr ptr);
  void copyTableNode(Signal *,
                     CopyTableNode* ctn,
                     NodeRecordPtr regNodePtr);
  void startFragment(Signal *, Uint32 tableId, Uint32 fragId);
  bool checkLcpAllTablesDoneInLqh(Uint32 from);
  
  void lcpStateAtNodeFailureLab(Signal *, Uint32 nodeId);
  void copyNodeLab(Signal *, Uint32 tableId);
  void copyGciReqLab(Signal *);
  void allLab(Signal *,
              ConnectRecordPtr regConnectPtr,
              TabRecordPtr regTabPtr);
  void tableCopyNodeLab(Signal *, TabRecordPtr regTabPtr);

  void removeNodeFromTables(Signal *, Uint32 tableId, Uint32 nodeId);
  void removeNodeFromTable(Signal *, Uint32 tableId, TabRecordPtr tabPtr);
  void removeNodeFromTablesComplete(Signal* signal, Uint32 nodeId);
  
  void packFragIntoPagesLab(Signal *, RWFragment* wf);
  void startNextChkpt(Signal *);
  void failedNodeLcpHandling(Signal*, NodeRecordPtr failedNodePtr);
  void failedNodeSynchHandling(Signal *, NodeRecordPtr failedNodePtr);
  void checkCopyTab(Signal*, NodeRecordPtr failedNodePtr);

  void initCommonData();
  void initialiseRecordsLab(Signal *, Uint32 stepNo, Uint32, Uint32);

  void findReplica(ReplicaRecordPtr& regReplicaPtr,
                   Fragmentstore* fragPtrP, 
		   Uint32 nodeId,
		   bool oldStoredReplicas = false);
//------------------------------------
// Node failure handling methods
//------------------------------------
  void startRemoveFailedNode(Signal *, NodeRecordPtr failedNodePtr);
  void handleGcpTakeOver(Signal *, NodeRecordPtr failedNodePtr);
  void handleLcpTakeOver(Signal *, NodeRecordPtr failedNodePtr);
  void handleNewMaster(Signal *, NodeRecordPtr failedNodePtr);
  void handleTakeOver(Signal*, Ptr<TakeOverRecord>);
  void handleLcpMasterTakeOver(Signal *, Uint32 nodeId);

//------------------------------------
// Replica record specific methods
//------------------------------------
  Uint32 findLogInterval(ConstPtr<ReplicaRecord> regReplicaPtr, 
			 Uint32 startGci);
  void findMinGci(ReplicaRecordPtr fmgReplicaPtr,
                  Uint32& keeGci,
                  Uint32& oldestRestorableGci);
  bool findStartGci(ConstPtr<ReplicaRecord> fstReplicaPtr,
                    Uint32 tfstStopGci,
                    Uint32& tfstStartGci,
                    Uint32& tfstLcp);
  void newCrashedReplica(ReplicaRecordPtr ncrReplicaPtr);
  void packCrashedReplicas(ReplicaRecordPtr pcrReplicaPtr);
  void releaseReplicas(Uint32 * replicaPtr);
  void removeOldCrashedReplicas(Uint32, Uint32, ReplicaRecordPtr rocReplicaPtr);
  void removeTooNewCrashedReplicas(ReplicaRecordPtr rtnReplicaPtr, Uint32 lastCompletedGCI);
  void mergeCrashedReplicas(ReplicaRecordPtr pcrReplicaPtr);
  void seizeReplicaRec(ReplicaRecordPtr& replicaPtr);

//------------------------------------
// Methods operating on a fragment and
// its connected replicas and nodes.
//------------------------------------
  void allocStoredReplica(FragmentstorePtr regFragptr,
                          ReplicaRecordPtr& newReplicaPtr,
                          Uint32 nodeId);
  Uint32 extractNodeInfo(const Fragmentstore * fragPtr, Uint32 nodes[]);
  bool findBestLogNode(CreateReplicaRecord* createReplica,
                       FragmentstorePtr regFragptr,
                       Uint32 startGci,
                       Uint32 stopGci,
                       Uint32 logNode,
                       Uint32& fblStopGci);
  bool findLogNodes(CreateReplicaRecord* createReplica,
                    FragmentstorePtr regFragptr,
                    Uint32 startGci,
                    Uint32 stopGci);
  void initFragstore(FragmentstorePtr regFragptr);
  void insertBackup(FragmentstorePtr regFragptr, Uint32 nodeId);
  void insertfraginfo(FragmentstorePtr regFragptr,
                      Uint32 noOfBackups,
                      Uint32* nodeArray);
  void linkOldStoredReplica(FragmentstorePtr regFragptr,
                            ReplicaRecordPtr replicaPtr);
  void linkStoredReplica(FragmentstorePtr regFragptr,
                         ReplicaRecordPtr replicaPtr);
  void prepareReplicas(FragmentstorePtr regFragptr);
  void removeNodeFromStored(Uint32 nodeId,
                            FragmentstorePtr regFragptr,
                            ReplicaRecordPtr replicaPtr,
			    bool temporary);
  void removeOldStoredReplica(FragmentstorePtr regFragptr,
                              ReplicaRecordPtr replicaPtr);
  void removeStoredReplica(FragmentstorePtr regFragptr,
                           ReplicaRecordPtr replicaPtr);
  void searchStoredReplicas(FragmentstorePtr regFragptr);
  bool setup_create_replica(FragmentstorePtr, CreateReplicaRecord*,
			    ConstPtr<ReplicaRecord>);
  void updateNodeInfo(FragmentstorePtr regFragptr);

//------------------------------------
// Fragment allocation, deallocation and
// find methods
//------------------------------------
  void allocFragments(Uint32 noOfFragments, TabRecordPtr regTabPtr);
  void releaseFragments(TabRecordPtr regTabPtr);
  void getFragstore(TabRecord *, Uint32 fragNo, FragmentstorePtr & ptr);
  void initialiseFragstore();

  void wait_old_scan(Signal*);
  Uint32 add_fragments_to_table(Ptr<TabRecord>, const Uint16 buf[]);
  Uint32 add_fragment_to_table(Ptr<TabRecord>, Uint32, Ptr<Fragmentstore>&);

  void drop_fragments(Signal*, ConnectRecordPtr, Uint32 last);
  void release_fragment_from_table(Ptr<TabRecord>, Uint32 fragId);
  void send_alter_tab_ref(Signal*, Ptr<TabRecord>,Ptr<ConnectRecord>, Uint32);
  void send_alter_tab_conf(Signal*, Ptr<ConnectRecord>);
  void alter_table_writeTable_conf(Signal* signal, Uint32 ptrI, Uint32 err);
  void saveTableFile(Signal*, Ptr<ConnectRecord>, Ptr<TabRecord>,
                     TabRecord::CopyStatus, Callback&);

//------------------------------------
// Page Record specific methods
//------------------------------------
  void allocpage(PageRecordPtr& regPagePtr);
  void releasePage(Uint32 pageIndex);

//------------------------------------
// Table Record specific methods
//------------------------------------
  void initTable(TabRecordPtr regTabPtr);
  void initTableFile(TabRecordPtr regTabPtr);
  void releaseTable(TabRecordPtr tabPtr);
  bool findTakeOver(Ptr<TakeOverRecord> & ptr, Uint32 failedNodeId);
  void handleTakeOverMaster(Signal *, Uint32 takeOverPtr);
  void handleTakeOverNewMaster(Signal *, Uint32 takeOverPtr);

//------------------------------------
// TakeOver Record specific methods
//------------------------------------
  void releaseTakeOver(TakeOverRecordPtr);
  void abortTakeOver(Signal*, TakeOverRecordPtr);
  bool anyActiveTakeOver();
  void checkToCopy();
  void checkToCopyCompleted(Signal *);
  bool checkToInterrupted(TakeOverRecordPtr& regTakeOverptr);
  Uint32 getStartNode(Uint32 takeOverPtr);

//------------------------------------
// Methods for take over functionality
//------------------------------------
  void changeNodeGroups(Uint32 startNode, Uint32 nodeTakenOver);
  void endTakeOver(Uint32 takeOverPtr);
  
  void systemRestartTakeOverLab(Signal *);
  void startTakeOver(Signal *,
                     Uint32 startNode,
                     Uint32 toNode,
                     const struct StartCopyReq*);
  void startNextCopyFragment(Signal *, Uint32 takeOverPtr);
  void toCopyFragLab(Signal *, Uint32 takeOverPtr);
  void toStartCopyFrag(Signal *, TakeOverRecordPtr);
  void startHsAddFragConfLab(Signal *);
  void prepareSendCreateFragReq(Signal *, Uint32 takeOverPtr);
  void toCopyCompletedLab(Signal *, TakeOverRecordPtr regTakeOverptr);
  void takeOverCompleted(Uint32 aNodeId);

//------------------------------------
// Node Record specific methods
//------------------------------------
  void checkStartTakeOver(Signal *);
  void insertAlive(NodeRecordPtr newNodePtr);
  void insertDeadNode(NodeRecordPtr removeNodePtr);
  void removeAlive(NodeRecordPtr removeNodePtr);
  void removeDeadNode(NodeRecordPtr removeNodePtr);

  NodeRecord::NodeStatus getNodeStatus(Uint32 nodeId);
  void setNodeStatus(Uint32 nodeId, NodeRecord::NodeStatus);
  Sysfile::ActiveStatus getNodeActiveStatus(Uint32 nodeId);
  void setNodeActiveStatus(Uint32 nodeId, Sysfile::ActiveStatus newStatus);
  void setNodeLcpActiveStatus(Uint32 nodeId, bool newState);
  bool getNodeLcpActiveStatus(Uint32 nodeId);
  bool getAllowNodeStart(Uint32 nodeId);
  void setAllowNodeStart(Uint32 nodeId, bool newState);
  bool getNodeCopyCompleted(Uint32 nodeId);
  void setNodeCopyCompleted(Uint32 nodeId, bool newState);
  Uint32 getNodeGroup(Uint32 nodeId) const;
  bool checkNodeAlive(Uint32 nodeId);

  void nr_start_fragments(Signal*, TakeOverRecordPtr);
  void nr_start_fragment(Signal*, TakeOverRecordPtr, ReplicaRecordPtr);
  void nr_run_redo(Signal*, TakeOverRecordPtr);
  void nr_start_logging(Signal*, TakeOverRecordPtr);

  void getTabInfo(Signal*);
  void getTabInfo_send(Signal*, TabRecordPtr);
  void getTabInfo_sendComplete(Signal*, Uint32, Uint32);
  int getTabInfo_copyTableToSection(SegmentedSectionPtr & ptr, CopyTableNode);
  int getTabInfo_copySectionToPages(TabRecordPtr, SegmentedSectionPtr);

  // Initialisation
  void initData();
  void initRecords();

  // Variables to support record structures and their free lists

  Uint32 capiConnectFileSize;

  ConnectRecord *connectRecord;
  Uint32 cfirstconnect;
  Uint32 cconnectFileSize;

  CreateReplicaRecord *createReplicaRecord;
  Uint32 cnoOfCreateReplicas;

  FileRecord *fileRecord;
  Uint32 cfirstfreeFile;
  Uint32 cfileFileSize;

  Fragmentstore *fragmentstore;
  Uint32 cfirstfragstore;
  Uint32 cfragstoreFileSize;
  RSS_OP_SNAPSHOT(cremainingfrags);

  Uint32 c_nextNodeGroup;
  NodeGroupRecord *nodeGroupRecord;
  RSS_OP_SNAPSHOT(cnghash);

  NodeRecord *nodeRecord;

  PageRecord *pageRecord;
  Uint32 cfirstfreepage;
  Uint32 cpageFileSize;

  ReplicaRecord *replicaRecord;
  Uint32 cfirstfreeReplica;
  Uint32 cnoFreeReplicaRec;
  Uint32 creplicaFileSize;
  RSS_OP_SNAPSHOT(cnoFreeReplicaRec);

  TabRecord *tabRecord;
  Uint32 ctabFileSize;

  ArrayPool<TakeOverRecord> c_takeOverPool;
  DLList<TakeOverRecord> c_activeTakeOverList;

  /*
    2.4  C O M M O N    S T O R E D    V A R I A B L E S
    ----------------------------------------------------
  */
  struct DIVERIFY_queue
  {
    DIVERIFY_queue() {
      m_ref = 0;
      cfirstVerifyQueue = clastVerifyQueue = 0;
      apiConnectRecord = 0;
      m_empty_done = 1;
    }
    ApiConnectRecord *apiConnectRecord;
    Uint32 cfirstVerifyQueue;
    Uint32 clastVerifyQueue;
    Uint32 m_empty_done;
    Uint32 m_ref;
  };

  bool isEmpty(const DIVERIFY_queue&);
  void enqueue(DIVERIFY_queue&, Uint32 senderData, Uint64 gci);
  void dequeue(DIVERIFY_queue&, ApiConnectRecord &);
  void emptyverificbuffer(Signal *, Uint32 q, bool aContintueB);
  void emptyverificbuffer_check(Signal*, Uint32, Uint32);

  DIVERIFY_queue c_diverify_queue[MAX_NDBMT_LQH_THREADS];
  Uint32 c_diverify_queue_cnt;

  /*------------------------------------------------------------------------*/
  /*       THIS VARIABLE KEEPS THE REFERENCES TO FILE RECORDS THAT DESCRIBE */
  /*       THE TWO FILES THAT ARE USED TO STORE THE VARIABLE CRESTART_INFO  */
  /*       ON DISK.                                                         */
  /*------------------------------------------------------------------------*/
  Uint32 crestartInfoFile[2];

  bool cgckptflag;    /* A FLAG WHICH IS SET WHILE A NEW GLOBAL CHECK
                           POINT IS BEING CREATED. NO VERIFICATION IS ALLOWED
                           IF THE FLAG IS SET*/
  Uint32 cgcpOrderBlocked;

  /**
   * This structure describes
   *   the GCP Save protocol
   */
  struct GcpSave
  {
    Uint32 m_gci;
    Uint32 m_master_ref;
    enum State {
      GCP_SAVE_IDLE     = 0, // Idle
      GCP_SAVE_REQ      = 1, // REQ received
      GCP_SAVE_CONF     = 2, // REF/CONF sent
      GCP_SAVE_COPY_GCI = 3
    } m_state;

    struct {
      State m_state;
      Uint32 m_new_gci;
      Uint32 m_time_between_gcp;   /* Delay between global checkpoints */
      Uint64 m_start_time;
    } m_master;
  } m_gcp_save;

  /**
   * This structure describes the MicroGCP protocol
   */
  struct MicroGcp
  {
    MicroGcp() { }
    bool m_enabled;
    Uint32 m_master_ref;

    /**
     * rw-lock that protects multiple parallel DIVERIFY (readers) from
     *   updates to gcp-state (e.g GCP_PREPARE, GCP_COMMIT)
     */
    NdbSeqLock m_lock;
    Uint64 m_old_gci;
    Uint64 m_current_gci; // Currently active
    Uint64 m_new_gci;     // Currently being prepared...
    enum State {
      M_GCP_IDLE      = 0,
      M_GCP_PREPARE   = 1,
      M_GCP_COMMIT    = 2,
      M_GCP_COMMITTED = 3,
      M_GCP_COMPLETE  = 4
    } m_state;

    struct {
      State m_state;
      Uint32 m_time_between_gcp;
      Uint64 m_new_gci;
      Uint64 m_start_time;
    } m_master;
  } m_micro_gcp;

  struct GcpMonitor
  {
    struct
    {
      Uint32 m_gci;
      Uint32 m_counter;
      Uint32 m_max_lag;
    } m_gcp_save;

    struct
    {
      Uint64 m_gci;
      Uint32 m_counter;
      Uint32 m_max_lag;
    } m_micro_gcp;
  } m_gcp_monitor;

  /*------------------------------------------------------------------------*/
  /*       THIS VARIABLE KEEPS TRACK OF THE STATE OF THIS NODE AS MASTER.   */
  /*------------------------------------------------------------------------*/
  enum MasterState {
    MASTER_IDLE = 0,
    MASTER_ACTIVE = 1,
    MASTER_TAKE_OVER_GCP = 2
  };
  MasterState cmasterState;
  Uint16      cmasterTakeOverNode;
  /* NODE IS NOT MASTER            */
  /* NODE IS ACTIVE AS MASTER      */
  /* NODE IS TAKING OVER AS MASTER */

  struct CopyGCIMaster {
    CopyGCIMaster(){
      m_copyReason = CopyGCIReq::IDLE;
      for (Uint32 i = 0; i<WAIT_CNT; i++)
        m_waiting[i] = CopyGCIReq::IDLE;
    }
    /*------------------------------------------------------------------------*/
    /*       THIS STATE VARIABLE IS USED TO INDICATE IF COPYING OF RESTART    */
    /*       INFO WAS STARTED BY A LOCAL CHECKPOINT OR AS PART OF A SYSTEM    */
    /*       RESTART.                                                         */
    /*------------------------------------------------------------------------*/
    CopyGCIReq::CopyReason m_copyReason;
    
    /*------------------------------------------------------------------------*/
    /*       COPYING RESTART INFO CAN BE STARTED BY LOCAL CHECKPOINTS AND BY  */
    /*       GLOBAL CHECKPOINTS. WE CAN HOWEVER ONLY HANDLE TWO SUCH COPY AT  */
    /*       THE TIME. THUS WE HAVE TO KEEP WAIT INFORMATION IN THIS VARIABLE.*/
    /*------------------------------------------------------------------------*/
    STATIC_CONST( WAIT_CNT = 2 );
    CopyGCIReq::CopyReason m_waiting[WAIT_CNT];
  } c_copyGCIMaster;
  
  struct CopyGCISlave {
    CopyGCISlave(){ m_copyReason = CopyGCIReq::IDLE; m_expectedNextWord = 0;}
    /*------------------------------------------------------------------------*/
    /*       THIS STATE VARIABLE IS USED TO INDICATE IF COPYING OF RESTART    */
    /*       INFO WAS STARTED BY A LOCAL CHECKPOINT OR AS PART OF A SYSTEM    */
    /*       RESTART. THIS VARIABLE IS USED BY THE NODE THAT RECEIVES         */
    /*       COPY_GCI_REQ.                                                    */
    /*------------------------------------------------------------------------*/
    Uint32 m_senderData;
    BlockReference m_senderRef;
    CopyGCIReq::CopyReason m_copyReason;
    
    Uint32 m_expectedNextWord;
  } c_copyGCISlave;
  
  /*------------------------------------------------------------------------*/
  /*       THIS VARIABLE IS USED TO KEEP TRACK OF THE STATE OF LOCAL        */
  /*       CHECKPOINTS.                                                     */
  /*------------------------------------------------------------------------*/
public:
  enum LcpStatus {
    LCP_STATUS_IDLE        = 0,
    LCP_TCGET              = 1,  // Only master
    LCP_STATUS_ACTIVE      = 2,
    LCP_CALCULATE_KEEP_GCI = 4,  // Only master
    LCP_COPY_GCI           = 5,  
    LCP_INIT_TABLES        = 6,
    LCP_TC_CLOPSIZE        = 7,  // Only master
    LCP_START_LCP_ROUND    = 8,
    LCP_TAB_COMPLETED      = 9,
    LCP_TAB_SAVED          = 10
  };
private:
  
  struct LcpState {
    LcpState() {}
    LcpStatus lcpStatus;
    Uint32 lcpStatusUpdatedPlace;

    struct Save {
      LcpStatus m_status;
      Uint32 m_place;
    } m_saveState[10];

    void setLcpStatus(LcpStatus status, Uint32 line){
      for (Uint32 i = 9; i > 0; i--)
        m_saveState[i] = m_saveState[i-1];
      m_saveState[0].m_status = lcpStatus;
      m_saveState[0].m_place = lcpStatusUpdatedPlace;

      lcpStatus = status;
      lcpStatusUpdatedPlace = line;
    }

    Uint32 lcpStart;
    Uint32 lcpStopGcp; 
    Uint32 keepGci;      /* USED TO CALCULATE THE GCI TO KEEP AFTER A LCP  */
    Uint32 oldestRestorableGci;
    
    Uint64 m_start_time; // When last LCP was started
    Uint64 m_lcp_time;   // How long last LCP took
    Uint32 m_lcp_trylock_timeout;

    struct CurrentFragment {
      Uint32 tableId;
      Uint32 fragmentId;
    } currentFragment;

    Uint32 noOfLcpFragRepOutstanding;

    /*------------------------------------------------------------------------*/
    /*       USED TO ENSURE THAT LCP'S ARE EXECUTED WITH CERTAIN TIMEINTERVALS*/
    /*       EVEN WHEN SYSTEM IS NOT DOING ANYTHING.                          */
    /*------------------------------------------------------------------------*/
    Uint32 ctimer;
    Uint32 ctcCounter;
    Uint32 clcpDelay;            /* MAX. 2^(CLCP_DELAY - 2) SEC BETWEEN LCP'S */
    
    /*------------------------------------------------------------------------*/
    /*       THIS STATE IS USED TO TELL IF THE FIRST LCP AFTER START/RESTART  */
    /*       HAS BEEN RUN.  AFTER A NODE RESTART THE NODE DOES NOT ENTER      */
    /*       STARTED STATE BEFORE THIS IS DONE.                               */
    /*------------------------------------------------------------------------*/
    bool immediateLcpStart;  
    bool m_LCP_COMPLETE_REP_From_Master_Received;
    SignalCounter m_LCP_COMPLETE_REP_Counter_DIH;
    SignalCounter m_LCP_COMPLETE_REP_Counter_LQH;
    SignalCounter m_LAST_LCP_FRAG_ORD;
    NdbNodeBitmask m_participatingLQH;
    NdbNodeBitmask m_participatingDIH;
    
    Uint32 m_masterLcpDihRef;
    bool   m_MASTER_LCPREQ_Received;
    Uint32 m_MASTER_LCPREQ_FailedNodeId;

    Uint32 m_lastLCP_COMPLETE_REP_id;
    Uint32 m_lastLCP_COMPLETE_REP_ref;
  } c_lcpState;
  
  /*------------------------------------------------------------------------*/
  /*       THIS VARIABLE KEEPS TRACK OF HOW MANY TABLES ARE ACTIVATED WHEN  */
  /*       STARTING A LOCAL CHECKPOINT WE SHOULD AVOID STARTING A CHECKPOINT*/
  /*       WHEN NO TABLES ARE ACTIVATED.                                    */
  /*------------------------------------------------------------------------*/
  Uint32 cnoOfActiveTables;

  BlockReference cdictblockref;          /* DICTIONARY BLOCK REFERENCE */
  Uint32 cfailurenr;              /* EVERY TIME WHEN A NODE FAILURE IS REPORTED
                                    THIS NUMBER IS INCREMENTED. AT THE START OF
                                    THE SYSTEM THIS NUMBER MUST BE INITIATED TO
                                    ZERO */

  BlockReference clocallqhblockref;
  BlockReference clocaltcblockref;
  BlockReference cmasterdihref;
  Uint16 cownNodeId;
  BlockReference cndbStartReqBlockref;
  BlockReference cntrlblockref;
  Uint32 con_lineNodes;
  Uint32 creceivedfrag;
  Uint32 cremainingfrags;
  Uint32 cstarttype;
  Uint32 csystemnodes;
  Uint32 c_newest_restorable_gci;
  Uint32 c_set_initial_start_flag;
  Uint64 c_current_time; // Updated approx. every 10ms

public:
  enum LcpMasterTakeOverState {
    LMTOS_IDLE = 0,
    LMTOS_WAIT_EMPTY_LCP = 1,   // Currently doing empty LCP
    LMTOS_WAIT_LCP_FRAG_REP = 2,// Currently waiting for outst. LCP_FRAG_REP
    LMTOS_INITIAL = 3,
    LMTOS_ALL_IDLE = 4,
    LMTOS_ALL_ACTIVE = 5,
    LMTOS_LCP_CONCLUDING = 6,
    LMTOS_COPY_ONGOING = 7
  };
private:
  class MasterTakeOverState {
  public:
    MasterTakeOverState() {}
    void set(LcpMasterTakeOverState s, Uint32 line) { 
      state = s; updatePlace = line;
    }
    
    LcpMasterTakeOverState state;
    Uint32 updatePlace;

    Uint32 minTableId;
    Uint32 minFragId;
    Uint32 failedNodeId;
  } c_lcpMasterTakeOverState;
  
  Uint16 cmasterNodeId;

  struct NodeStartMasterRecord {
    NodeStartMasterRecord() {}
    Uint32 startNode;
    Uint32 wait;
    Uint32 failNr;
    bool activeState;
    bool blockLcp;
    Uint32 blockGcp; // 0, 1=ordered, 2=effective
    Uint32 startInfoErrorCode;
    Uint32 m_outstandingGsn;
    MutexHandle2<DIH_FRAGMENT_INFO> m_fragmentInfoMutex;
  };
  NodeStartMasterRecord c_nodeStartMaster;
  
  struct NodeStartSlaveRecord {
    NodeStartSlaveRecord() { nodeId = 0;}

    Uint32 nodeId;
  };
  NodeStartSlaveRecord c_nodeStartSlave;

  Uint32 cfirstAliveNode;
  Uint32 cfirstDeadNode;
  Uint32 cstartPhase;
  Uint32 cnoReplicas;

  bool cwaitLcpSr;
  /**
   * Available nodegroups (ids) (length == cnoOfNodeGroups)
   *   use to support nodegroups 2,4,6 (not just consequtive nodegroup ids)
   */
  Uint32 c_node_groups[MAX_NDB_NODES];
  Uint32 cnoOfNodeGroups;
  Uint32 crestartGci;      /* VALUE OF GCI WHEN SYSTEM RESTARTED OR STARTED */
  
  /**
   * Counter variables keeping track of the number of outstanding signals
   * for particular signals in various protocols.
   */
  SignalCounter c_COPY_GCIREQ_Counter;
  SignalCounter c_COPY_TABREQ_Counter;
  SignalCounter c_CREATE_FRAGREQ_Counter;
  SignalCounter c_DIH_SWITCH_REPLICA_REQ_Counter;
  SignalCounter c_EMPTY_LCP_REQ_Counter;
  SignalCounter c_GCP_COMMIT_Counter;
  SignalCounter c_GCP_PREPARE_Counter;
  SignalCounter c_GCP_SAVEREQ_Counter;
  SignalCounter c_SUB_GCP_COMPLETE_REP_Counter;
  SignalCounter c_INCL_NODEREQ_Counter;
  SignalCounter c_MASTER_GCPREQ_Counter;
  SignalCounter c_MASTER_LCPREQ_Counter;
  SignalCounter c_START_INFOREQ_Counter;
  SignalCounter c_START_RECREQ_Counter;
  SignalCounter c_STOP_ME_REQ_Counter;
  SignalCounter c_TC_CLOPSIZEREQ_Counter;
  SignalCounter c_TCGETOPSIZEREQ_Counter;
  SignalCounter c_START_LCP_REQ_Counter;

  bool   c_blockCommit;
  Uint32 c_blockCommitNo;

  bool getBlockCommit() const {
    return c_blockCommit || cgckptflag;
  }

  /**
   * SwitchReplicaRecord - Should only be used by master
   */
  struct SwitchReplicaRecord {
    SwitchReplicaRecord() {}
    void clear(){}

    Uint32 nodeId;
    Uint32 tableId;
    Uint32 fragNo;
  };
  SwitchReplicaRecord c_switchReplicas;
  
  struct StopPermProxyRecord {
    StopPermProxyRecord() { clientRef = 0; }
    
    Uint32 clientData;
    BlockReference clientRef;
    BlockReference masterRef;
  };
  
  struct StopPermMasterRecord {
    StopPermMasterRecord() { clientRef = 0;}
    
    Uint32 returnValue;
    
    Uint32 clientData;
    BlockReference clientRef;
  };
  
  StopPermProxyRecord c_stopPermProxy;
  StopPermMasterRecord c_stopPermMaster;
  
  void checkStopPermProxy(Signal*, NodeId failedNodeId);
  void checkStopPermMaster(Signal*, NodeRecordPtr failedNodePtr);
  
  void switchReplica(Signal*, 
		     Uint32 nodeId, 
		     Uint32 tableId, 
		     Uint32 fragNo);

  void switchReplicaReply(Signal*, NodeId nodeId);

  /**
   * Wait GCP (proxy)
   */
  struct WaitGCPProxyRecord {
    WaitGCPProxyRecord() { clientRef = 0;}
    
    Uint32 clientData;
    BlockReference clientRef;
    BlockReference masterRef;
    
    union { Uint32 nextPool; Uint32 nextList; };
    Uint32 prevList;
  };
  typedef Ptr<WaitGCPProxyRecord> WaitGCPProxyPtr;

  /**
   * Wait GCP (master)
   */
  struct WaitGCPMasterRecord {
    WaitGCPMasterRecord() { clientRef = 0;}
    Uint32 clientData;
    BlockReference clientRef;

    union { Uint32 nextPool; Uint32 nextList; };
    Uint32 prevList;
  };
  typedef Ptr<WaitGCPMasterRecord> WaitGCPMasterPtr;

  /**
   * Pool/list of WaitGCPProxyRecord record
   */
  ArrayPool<WaitGCPProxyRecord> waitGCPProxyPool;
  DLList<WaitGCPProxyRecord> c_waitGCPProxyList;

  /**
   * Pool/list of WaitGCPMasterRecord record
   */
  ArrayPool<WaitGCPMasterRecord> waitGCPMasterPool;
  typedef DLList<WaitGCPMasterRecord> WaitGCPList;
  WaitGCPList c_waitGCPMasterList;
  WaitGCPList c_waitEpochMasterList;

  void checkWaitGCPProxy(Signal*, NodeId failedNodeId);
  void checkWaitGCPMaster(Signal*, NodeId failedNodeId);
  void emptyWaitGCPMasterQueue(Signal*, Uint64, WaitGCPList&);
  
  /**
   * Stop me
   */
  struct StopMeRecord {
    StopMeRecord() { clientRef = 0;}

    BlockReference clientRef;
    Uint32 clientData;
  };
  StopMeRecord c_stopMe;

  void checkStopMe(Signal *, NodeRecordPtr failedNodePtr);
  
#define DIH_CDATA_SIZE 128
  /**
   * This variable must be atleast the size of Sysfile::SYSFILE_SIZE32
   */
  Uint32 cdata[DIH_CDATA_SIZE];       /* TEMPORARY ARRAY VARIABLE */

  /**
   * Sys file data
   */
  Uint32 sysfileData[DIH_CDATA_SIZE];
  Uint32 sysfileDataToFile[DIH_CDATA_SIZE];

  /**
   * When a node comes up without filesystem
   *   we have to clear all LCP for that node
   */
  void invalidateNodeLCP(Signal *, Uint32 nodeId, Uint32 tableId);
  void invalidateNodeLCP(Signal *, Uint32 nodeId, TabRecordPtr);

  /**
   * Reply from nodeId
   */
  void startInfoReply(Signal *, Uint32 nodeId);

  void dump_replica_info();
  void dump_replica_info(const Fragmentstore*);

  // DIH specifics for execNODE_START_REP (sendDictUnlockOrd)
  void execNODE_START_REP(Signal* signal);

  /*
   * Lock master DICT.  Only current use is by starting node
   * during NR.  A pool of slave records is convenient anyway.
   */
  struct DictLockSlaveRecord {
    Uint32 lockPtr;
    Uint32 lockType;
    bool locked;
    Callback callback;
    Uint32 nextPool;
  };

  typedef Ptr<DictLockSlaveRecord> DictLockSlavePtr;
  ArrayPool<DictLockSlaveRecord> c_dictLockSlavePool;

  // slave
  void sendDictLockReq(Signal* signal, Uint32 lockType, Callback c);
  void recvDictLockConf(Signal* signal);
  void sendDictUnlockOrd(Signal* signal, Uint32 lockSlavePtrI);

  // NR
  Uint32 c_dictLockSlavePtrI_nodeRestart; // userPtr for NR
  void recvDictLockConf_nodeRestart(Signal* signal, Uint32 data, Uint32 ret);

  Uint32 c_error_7181_ref;

#ifdef ERROR_INSERT
  void sendToRandomNodes(const char*, Signal*, SignalCounter*,
                         SendFunction,
                         Uint32 extra = RNIL,
                         Uint32 block = 0, Uint32 gsn = 0, Uint32 len = 0,
                         JobBufferLevel = JBB);
#endif

  bool check_enable_micro_gcp(Signal* signal, bool broadcast);

  bool c_sr_wait_to;
  NdbNodeBitmask m_sr_nodes;
  NdbNodeBitmask m_to_nodes;

  void startme_copygci_conf(Signal*);

  /**
   * Local LCP state
   *   This struct is more or less a copy of lcp-state
   *   Reason for duplicating it is that
   *   - not to mess with current code
   *   - this one is "distributed", i.e maintained by *all* nodes,
   *     not like c_lcpState which mixed master/slave state in a "unnatural"
   *     way
   */
  struct LocalLCPState
  {
    enum State {
      LS_INITIAL = 0,
      LS_RUNNING = 1,
      LS_COMPLETE = 2
    } m_state;
    
    StartLcpReq m_start_lcp_req;
    Uint32 m_keep_gci; // Min GCI is needed to restore LCP
    Uint32 m_stop_gci; // This GCI needs to be complete before LCP is restorable

    LocalLCPState() { reset();}
    
    void reset();
    void init(const StartLcpReq*);
    void lcp_frag_rep(const LcpFragRep*);
    void lcp_complete_rep(Uint32 gci);
    
    /**
     * @param gci - current GCI being made restorable (COPY_GCI)
     */
    bool check_cut_log_tail(Uint32 gci) const;
  } m_local_lcp_state;

  // MT LQH
  Uint32 c_fragments_per_node;
  Uint32 dihGetInstanceKey(FragmentstorePtr tFragPtr) {
    ndbrequire(!tFragPtr.isNull());
    Uint32 log_part_id = tFragPtr.p->m_log_part_id;
    Uint32 instanceKey = 1 + log_part_id % MAX_NDBMT_LQH_WORKERS;
    return instanceKey;
  }
  Uint32 dihGetInstanceKey(Uint32 tabId, Uint32 fragId);

  bool c_2pass_inr;
};

#if (DIH_CDATA_SIZE < _SYSFILE_SIZE32)
#error "cdata is to small compared to Sysfile size"
#endif

#endif


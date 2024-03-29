/*
   Copyright (c) 2003, 2023, Oracle and/or its affiliates.

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

#ifndef DBDIH_H
#define DBDIH_H

#include <ndb_limits.h>
#include <pc.hpp>
#include <SimulatedBlock.hpp>
#include "Sysfile.hpp"
#include <SignalCounter.hpp>

#include <signaldata/RedoStateRep.hpp>
#include <signaldata/MasterLCP.hpp>
#include <signaldata/CopyGCIReq.hpp>
#include <blocks/mutexes.hpp>
#include <signaldata/LCP.hpp>
#include <NdbSeqLock.hpp>
#include <CountingSemaphore.hpp>
#include <Mutex.hpp>

#define JAM_FILE_ID 356


#ifdef DBDIH_C

/*###############*/
/* NODE STATES   */
/*###############*/
#define ZIDLE 0
#define ZACTIVE 1

/*#########*/
/* GENERAL */
/*#########*/
#define ZVAR_NO_WORD 0
#define ZVAR_NO_CRESTART_INFO 1
#define ZVAR_NO_CRESTART_INFO_TO_FILE 2
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
// Other DIH error codes
// --------------------------------------
#define ZLONG_MESSAGE_ERROR 312

// --------------------------------------
// Crash Codes
// --------------------------------------
#define ZCOULD_NOT_OCCUR_ERROR 300
#define ZNOT_MASTER_ERROR 301
#define ZWRONG_FAILURE_NUMBER_ERROR 302
#define ZWRONG_START_NODE_ERROR 303
#define ZNO_REPLICA_FOUND_ERROR 304

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
/*
 * Pages are used for flushing table definitions during LCP,
 * and for other operations such as metadata changes etc
 * 
 */
#define MAX_CONCURRENT_LCP_TAB_DEF_FLUSHES 4
#define MAX_CONCURRENT_DIH_TAB_DEF_OPS (MAX_CONCURRENT_LCP_TAB_DEF_FLUSHES + 2)
#define ZPAGEREC (MAX_CONCURRENT_DIH_TAB_DEF_OPS * PACK_TABLE_PAGES)
#define ZCREATE_REPLICA_FILE_SIZE 4
#define ZPROXY_MASTER_FILE_SIZE (MAX_NDB_NODES + 1)

/*MaxConcurrent proxied WaitGcpReq.  Set to 10 as safety margin on 1.*/
#define ZPROXY_FILE_SIZE 10
#endif

/*
 * Pack table into pages.
 * See use of writePageWord() in
 * packTableIntoPagesLab() and helper
 * functions to determine the constants
 * below.
 */
#define MAX_CRASHED_REPLICAS 8
#define PACK_REPLICAS_WORDS (4 + 4 * MAX_LCP_STORED + 2 * MAX_CRASHED_REPLICAS)
#define PACK_FRAGMENT_WORDS (6 + 2 * MAX_REPLICAS * PACK_REPLICAS_WORDS)
#define PACK_TABLE_WORDS (10 + MAX_NDB_PARTITIONS * PACK_FRAGMENT_WORDS)
#define PACK_TABLE_PAGE_WORDS (2048 - 32)
#define PACK_TABLE_PAGES ((PACK_TABLE_WORDS + PACK_TABLE_PAGE_WORDS - 1) / PACK_TABLE_PAGE_WORDS)

#define MAX_QUEUED_FRAG_CHECKPOINTS_PER_NODE 32
#define MAX_STARTED_FRAG_CHECKPOINTS_PER_NODE 32

class Dbdih: public SimulatedBlock {
#ifdef ERROR_INSERT
  typedef void (Dbdih::* SendFunction)(Signal*, Uint32, Uint32);
#endif
public:

  // Records

  /*############## CONNECT_RECORD ##############*/
  /**
   * THE CONNECT RECORD IS CREATED WHEN A TRANSACTION HAS TO START. IT KEEPS
   * ALL INTERMEDIATE INFORMATION NECESSARY FOR THE TRANSACTION FROM THE
   * DISTRIBUTED MANAGER. THE RECORD KEEPS INFORMATION ABOUT THE
   * OPERATIONS THAT HAVE TO BE CARRIED OUT BY THE TRANSACTION AND
   * ALSO THE TRAIL OF NODES FOR EACH OPERATION IN THE THE
   * TRANSACTION.
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
        Uint32 m_partitionCount;
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

  /**
   *       THESE RECORDS ARE USED WHEN CREATING REPLICAS DURING SYSTEM
   *       RESTART. I NEED A COMPLEX DATA STRUCTURE DESCRIBING THE REPLICAS
   *       I WILL TRY TO CREATE FOR EACH FRAGMENT.
   *
   *       I STORE A REFERENCE TO THE FOUR POSSIBLE CREATE REPLICA RECORDS
   *       IN A COMMON STORED VARIABLE. I ALLOW A MAXIMUM OF 4 REPLICAS TO
   *       BE RESTARTED PER FRAGMENT.
   */
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

  /**
   *       THIS RECORD CONTAINS A FILE DESCRIPTION. THERE ARE TWO
   *       FILES PER TABLE TO RAISE SECURITY LEVEL AGAINST DISK CRASHES.
   */
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

  /**
   * THIS RECORD KEEPS THE STORAGE AND DECISIONS INFORMATION OF A FRAGMENT
   * AND ITS REPLICAS. IF FRAGMENT HAS MORE THAN ONE BACK UP
   * REPLICA THEN A LIST OF MORE NODES IS ATTACHED TO THIS RECORD.
   * EACH RECORD IN MORE LIST HAS INFORMATION ABOUT ONE BACKUP. THIS RECORD
   * ALSO HAVE THE STATUS OF THE FRAGMENT.
   */
  struct Fragmentstore {
    Uint16 activeNodes[MAX_REPLICAS];
    Uint32 preferredPrimary;

    Uint32 oldStoredReplicas;    /* "DEAD" STORED REPLICAS */
    Uint32 storedReplicas;       /* "ALIVE" STORED REPLICAS */
    Uint32 nextFragmentChunk;
    
    Uint32 m_log_part_id;

    /**
     * Used by Fully replicated tables to find the main fragment and to
     * find local fragments.
     */
    Uint32 fragId;
    Uint32 partition_id;
    Uint32 nextCopyFragment;
    
    Uint8 m_inc_used_log_parts;
    Uint8 distributionKey;
    Uint8 fragReplicas;
    Uint8 noOldStoredReplicas;  /* NUMBER OF "DEAD" STORED REPLICAS */
    Uint8 noStoredReplicas;     /* NUMBER OF "ALIVE" STORED REPLICAS*/
    Uint8 noLcpReplicas;        ///< No of replicas remaining to be LCP:ed
  };
  typedef Ptr<Fragmentstore> FragmentstorePtr;

  /*########### PAGE RECORD ############*/
  /**
   *       THIS RECORD KEEPS INFORMATION ABOUT NODE GROUPS.
   */
  struct NodeGroupRecord {
    Uint32 nodesInGroup[MAX_REPLICAS + 1];
    Uint32 nextReplicaNode;
    Uint32 nodeCount;
    Uint32 activeTakeOver; // Which node...
    Uint32 activeTakeOverCount;
    Uint32 m_next_log_part;
    Uint32 m_new_next_log_part;
    Uint32 nodegroupIndex;
    Uint32 m_ref_count;
    Uint32 m_used_log_parts[MAX_INSTANCE_KEYS];
  };
  typedef Ptr<NodeGroupRecord> NodeGroupRecordPtr;
  /**
   *       THIS RECORD KEEPS INFORMATION ABOUT NODES.
   *
   *       RECORD ALIGNED TO BE 64 BYTES.
   */
  enum NodefailHandlingStep {
    NF_REMOVE_NODE_FROM_TABLE = 1,
    NF_GCP_TAKE_OVER = 2,
    NF_LCP_TAKE_OVER = 4
  };
  
  /**
   * useInTransactions is used in DIGETNODES to assert that we give
   * DBTC a node view which is correct. To ensure we provide a view
   * which is correct we use an RCU mechanism when executing
   * DIGETNODES. It's not a crashing problem, but it ensures that
   * we avoid getting into unnecessary extra wait states at node
   * failures and also that we avoid unnecessary abortions.
   *
   * We update this view any time any node is changing the value of
   * useInTransactions and DBTC could be actively executing
   * transactions.
   */
  NdbSeqLock m_node_view_lock;

  struct NodeRecord
  {
    NodeRecord() { }
    /**
     * Removed the constructor method and replaced it with the method
     * initNodeRecord. The problem with the constructor method is that
     * in debug compiled code it will initialise the entire object to
     * zero. This didn't play well at all with the node recovery status
     * which is used from the start of the node until it dies, so it
     * should not be initialised when DIH finds it appropriate to
     * initialise it. One could also long-term separate the two functions
     * into two separate objects.
     */
    enum NodeStatus {
      NOT_IN_CLUSTER = 0,
      ALIVE = 1,
      STARTING = 2,
      DIED_NOW = 3,
      DYING = 4,
      DEAD = 5
    };      

    /**
     * The NodeRecoveryStatus variable and all the timers connected to this
     * status is used for two purposes. The first purpose is for a NDBINFO
     * table that the master node will use to be able to specify the times
     * a node restart has spent in the various node restart phases.
     *
     * This will help both the users and the developers to understand where
     * the node restart is spending time.
     *
     * In addition the timers are also used to estimate how much more time
     * the node will need before reaching the next wait for local checkpoint
     * (LCP). Starting LCPs with good timing is crucial to shorten the waits
     * for LCPs by the starting nodes. We want to wait with starting LCPs
     * to ensure that as many nodes as possible are handled in between
     * LCPs as possible. At the same time we cannot block LCP execution for
     * any extended period since it will jeopardize the future stability of
     * the cluster.
     */
    enum NodeRecoveryStatus
    {
      /* No valid state or node not defined in cluster */
      NOT_DEFINED_IN_CLUSTER = 0,

      /* There is state for no information about restarts. */
      NODE_NOT_RESTARTED_YET = 1,

      /* Node failure states are used in all nodes. */
      NODE_FAILED = 2,
      NODE_FAILURE_COMPLETED = 3,

      /* The first set of states are only used in master nodes. */
      ALLOCATED_NODE_ID = 4,
      INCLUDED_IN_HB_PROTOCOL = 5,
      NDBCNTR_START_WAIT = 6,
      NDBCNTR_STARTED = 7,
      START_PERMITTED = 8,
      WAIT_LCP_TO_COPY_DICT = 9,
      COPY_DICT_TO_STARTING_NODE = 10,
      INCLUDE_NODE_IN_LCP_AND_GCP = 11,
      LOCAL_RECOVERY_STARTED = 12,
      RESTORE_FRAG_COMPLETED = 13,
      UNDO_DD_COMPLETED = 14,
      EXECUTE_REDO_LOG_COMPLETED = 15,
      COPY_FRAGMENTS_STARTED = 16,
      WAIT_LCP_FOR_RESTART = 17,
      WAIT_SUMA_HANDOVER = 18,
      RESTART_COMPLETED = 19,

      /* There is a set of states used in non-master nodes as well. */
      NODE_GETTING_PERMIT = 20,
      NODE_GETTING_INCLUDED = 21,
      NODE_GETTING_SYNCHED = 22,
      NODE_IN_LCP_WAIT_STATE = 23,
      NODE_ACTIVE = 24
    };

    /**
     * We need to ensure that we don't pause the node when the master node
     * asks for it in case the node is already dead. We check this by
     * by verifying that the node is in the state NODE_GETTING_PERMIT in
     * in the non-master nodes. Since we do not yet maintain the
     * nodeRecoveryStatus in all restart situations we temporarily
     * put this into a separate variable that we maintain separately.
     * TODO: We should use nodeRecoveryStatus when we maintain this
     * state in all types of starts.
     */
    bool is_pausable;
    NodeRecoveryStatus nodeRecoveryStatus;
    NDB_TICKS nodeFailTime;
    NDB_TICKS nodeFailCompletedTime;
    NDB_TICKS allocatedNodeIdTime;
    NDB_TICKS includedInHBProtocolTime;
    NDB_TICKS ndbcntrStartWaitTime;
    NDB_TICKS ndbcntrStartedTime;
    NDB_TICKS startPermittedTime;
    NDB_TICKS waitLCPToCopyDictTime;
    NDB_TICKS copyDictToStartingNodeTime;
    NDB_TICKS includeNodeInLCPAndGCPTime;
    NDB_TICKS startDatabaseRecoveryTime;
    NDB_TICKS startUndoDDTime;
    NDB_TICKS startExecREDOLogTime;
    NDB_TICKS startBuildIndexTime;
    NDB_TICKS copyFragmentsStartedTime;
    NDB_TICKS waitLCPForRestartTime;
    NDB_TICKS waitSumaHandoverTime;
    NDB_TICKS restartCompletedTime;

    NDB_TICKS nodeGettingPermitTime;
    NDB_TICKS nodeGettingIncludedTime;
    NDB_TICKS nodeGettingSynchedTime;
    NDB_TICKS nodeInLCPWaitStateTime;
    NDB_TICKS nodeActiveTime;

    struct FragmentCheckpointInfo {
      Uint32 tableId;
      Uint32 fragId;
      Uint32 replicaPtr;
    };
    
    Sysfile::ActiveStatus activeStatus;

    bool useInTransactions;

    NodeStatus nodeStatus;
    bool allowNodeStart;
    bool m_inclDihLcp;
    Uint8 copyCompleted; // 0 = NO :-), 1 = YES, 2 = yes, first WAITING
   
    /**
     * Used by master as part of running LCPs to keep track of fragments
     * that have started checkpoints and fragments that have been queued
     * for LCP execution.
     */
    FragmentCheckpointInfo startedChkpt[MAX_STARTED_FRAG_CHECKPOINTS_PER_NODE];
    FragmentCheckpointInfo queuedChkpt[MAX_QUEUED_FRAG_CHECKPOINTS_PER_NODE];

    Bitmask<1> m_nodefailSteps;
    Uint32 activeTabptr;
    Uint32 nextNode;
    Uint32 nodeGroup;

    SignalCounter m_NF_COMPLETE_REP;
    
    Uint8 dbtcFailCompleted;
    Uint8 dblqhFailCompleted;
    Uint8 dbqlqhFailCompleted;
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
    Uint32 nextPool;

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

    /**
     * Information needed to put the LCP_FRAG_REP into a queue and avoid
     * sending the information onwards to all the other nodes in the
     * cluster. We use a doubly linked list to support removal from
     * queue due to drop table.
     *
     * By queueing in the local DIH we can make it appear as if the LCP
     * is paused from the point of view of all the DIH blocks in the cluster.
     *
     * In the DBLQH the LCP is continuing unabated as long as there are
     * fragments queued to execute LCPs on. The purpose of this pause support
     * is to be able to copy the meta data without having to wait for the
     * current LCP to be fully completed. Instead we can copy it while we are
     * pausing the LCP reporting. This gives a possibility to provide
     * new node with a snapshot of the metadata from the master node
     * without having to stop the progress with the LCP execution.
     */
    Uint32 nextList;
    Uint32 prevList;
    Uint32 repMaxGciStarted;
    Uint32 repMaxGciCompleted;
    Uint32 fragId;
    Uint32 tableId;
    /* lcpNo == nextLcp, checked at queueing */
    /* nodeId == procNode */

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
  typedef ArrayPool<ReplicaRecord> ReplicaRecord_pool;
  typedef DLFifoList<ReplicaRecord_pool> ReplicaRecord_fifo;

  ReplicaRecord_pool c_replicaRecordPool;
  ReplicaRecord_fifo c_queued_lcp_frag_rep;

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
    TabRecord() { m_flags = 0; }

    /**
     * State for copying table description into pages
     */
    enum CopyStatus {
      CS_IDLE = 0,
      CS_SR_PHASE1_READ_PAGES = 1,
      CS_SR_PHASE2_READ_TABLE = 2,
      CS_SR_PHASE3_COPY_TABLE = 3,
      CS_REMOVE_NODE = 4,
      CS_LCP_READ_TABLE = 5,
      CS_COPY_TAB_REQ = 6,
      CS_COPY_NODE_STATE = 7,
      CS_ADD_TABLE_MASTER = 8,
      CS_ADD_TABLE_SLAVE = 9,
      CS_INVALIDATE_NODE_LCP = 10,
      CS_ALTER_TABLE = 11,
      CS_COPY_TO_SAVE = 12
      ,CS_GET_TABINFO = 13
    };
    /**
     * State for copying pages to disk
     */
    enum UpdateState {
      US_IDLE = 0,
      US_LOCAL_CHECKPOINT = 1,
      US_LOCAL_CHECKPOINT_QUEUED = 2,
      US_REMOVE_NODE = 3,
      US_COPY_TAB_REQ = 4,
      US_ADD_TABLE_MASTER = 5,
      US_ADD_TABLE_SLAVE = 6,
      US_INVALIDATE_NODE_LCP = 7,
      US_CALLBACK = 8
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
    enum TableFlags
    {
      TF_FULLY_REPLICATED = 1
    };

    /**
     * rw-lock that protects multiple parallel DIGETNODES (readers) from
     *   updates to fragmenation changes (e.g UPDATE_FRAG_STATEREQ)...
     *   search for DIH_TAB_WRITE_LOCK
     */
    NdbSeqLock m_lock;

    /**
     * tabStatus, schemaTransId, m_map_ptr_i, totalfragments, noOfBackups
     * and m_scan_reorg_flag are read concurrently from many TC threads in
     * the execDIH_SCAN_TAB_REQ so we place these close to each other.
     */
    TabStatus tabStatus;
    Uint32 schemaTransId;
    Uint32 totalfragments;
    /**
     * partitionCount differs from totalfragments for fully replicated
     * tables.
     */
    Uint32 partitionCount;
    union {
      Uint32 mask;
      Uint32 m_map_ptr_i;
    };
    Uint32 m_scan_reorg_flag;
    Uint32 m_flags;
    Uint32 primaryTableId;

    Uint8 noOfBackups;
    Uint8 kvalue;

    Uint16 noPages;
    Uint16 tableType;

    Uint32 schemaVersion;
    union {
      Uint32 hashpointer;
      Uint32 m_new_map_ptr_i;
    };
    Method method;



//-----------------------------------------------------------------------------
// Each entry in this array contains a reference to 16 fragment records in a
// row. Thus finding the correct record is very quick provided the fragment id.
//-----------------------------------------------------------------------------
    Uint32 startFid[(MAX_NDB_PARTITIONS - 1) / NO_OF_FRAGS_PER_CHUNK + 1];

    CopyStatus tabCopyStatus;
    UpdateState tabUpdateState;
    TabLcpStatus tabLcpStatus;
    Storage tabStorage;

    Uint32 tabFile[2];
    Uint32 noOfWords;
    Uint32 tabRemoveNode;
    Uint32 noOfFragChunks;
    Uint32 tabActiveLcpFragments;

    struct {
      Uint32 tabUserRef;
      Uint32 tabUserPtr;
    } m_dropTab;
    Uint32 connectrec;

    // set in local protocol during prepare until commit
    /**
     * m_scan_count is heavily updated by all TC threads as they start and
     * stop scans. This is always updated when also grabbing the mutex,
     * so we place it close to the declaration of the mutex to avoid
     * contaminating too many CPU cache lines.
     */
    Uint32 m_scan_count[2];

    /**
     * This mutex protects the changes to m_scan_count to ensure that we
     * complete old scans relying on old meta data before removing the
     * metadata parts. It also protects the combination of tabStatus
     * schemaTransId checked for in execDIH_SCAN_TAB_REQ(...).
     *
     * Given that DIH_SCAN_TAB_REQ also reads totalfragments, partitionCount
     * m_map_ptr_i, noOfBackups, m_scan_reorg_flag we protect those variables
     * as well with this mutex. These variables are also protected by the
     * above NdbSeqLock to ensure that execDIGETNODESREQ can execute
     * concurrently from many TC threads simultaneously.
     *
     * DIH_SCAN_TAB_REQ and DIH_SCAN_TAB_COMPLETE_REP are called once per
     * scan at start and end. These will both grab a mutex on the table
     * object. This should support in the order of a few million scans
     * per table per data node. This should suffice. The need for a mutex
     * comes from the fact that we need to keep track of number of scans.
     * Thus we need to update from many different threads.
     *
     * DIGETNODESREQ is called once per primary key operation and once
     * per fragment scanned in a scan operation. This means that it can
     * be called many millions of times per second in a data node. Thus
     * a mutex per table is not sufficient. The data read in DIGETNODESREQ
     * is updated very seldom. So we use the RCU mechanism, we read
     * the value of the NdbSeqLock before reading the variables, we then
     * read the variables protected by this mechanism whereafter we verify
     * that the NdbSeqLock haven't changed it's value.
     *
     * It is noteworthy that using RCU requires reading the lock variable
     * before and after in both the successful case as well as in the
     * error case. We cannot deduce an error until we have verified that
     * we have read consistent data.
     *
     * So with this mechanism DIGETNODESREQ can scale to almost any number
     * of key operations and fragment scans per second with minor glitches
     * while still performing online schema changes.
     *
     * We put the mutex surrounded by variables that are not used in normal
     * operation to minimize the bad effects of CPU cache misses.
     */
    NdbMutex theMutex;

    Uint32 pageRef[PACK_TABLE_PAGES]; // TODO: makedynamic
  };
  typedef Ptr<TabRecord> TabRecordPtr;

  /***************************************************************************/
  /* THIS RECORD IS USED TO KEEP TRACK OF TAKE OVER AND STARTING A NODE.    */
  /* WE KEEP IT IN A RECORD TO ENABLE IT TO BE PARALLELISED IN THE FUTURE.  */
  /**************************************************************************/
  struct TakeOverRecord {

    TakeOverRecord() {}

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
      ,TO_UPDATE_FRAG_STATE_STORED = 7
                        // Waiting for all UPDATE_FRAG_STATEREQ stored
      ,TO_UPDATE_AFTER_STORED = 8  // Waiting for master (UPDATE_TOREQ)
      ,TO_COPY_FRAG = 9            // Waiting for copy node (COPY_FRAGREQ)
      ,TO_COPY_ACTIVE = 10         // Waiting for local LQH (COPY_ACTIVEREQ)
      ,TO_UPDATE_BEFORE_COMMIT = 11// Waiting for master (UPDATE_TOREQ)
      ,TO_UPDATE_FRAG_STATE_COMMIT = 12 
                            // Waiting for all (UPDATE_FRAG_STATEREQ commit)
      ,TO_UPDATE_AFTER_COMMIT = 13 // Waiting for master (UPDATE_TOREQ)

      ,TO_START_LOGGING = 14        // Enabling logging on all fragments
      ,TO_SL_COPY_ACTIVE = 15       // Start logging: Copy active (local)
      ,TO_SL_UPDATE_FRAG_STATE = 16 // Start logging: Create Frag (dist)
      ,TO_END_TO = 17               // Waiting for master (END_TOREQ)
      ,TO_QUEUED_UPDATE_BEFORE_STORED = 18 //Queued
      ,TO_QUEUED_UPDATE_BEFORE_COMMIT = 19  //Queued
      ,TO_QUEUED_SL_UPDATE_FRAG_STATE = 20  //Queued
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
    /**
     * For node restarts we use a number of parallel take over records
     * such that we can copy fragments from several LDM instances in
     * parallel. Each thread will take care of a subset of LDM
     * instances provided by knowing the number of instances and
     * our thread id. For each replica we will then check if
     * replica_instance_id % m_number_of_copy_threads == m_copy_thread_id.
     */
    Uint32 m_copy_thread_id;
    Uint32 m_number_of_copy_threads;
    Uint32 m_copy_threads_completed;

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
    NDB_TICKS toStartTime;
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
  typedef ArrayPool<TakeOverRecord> TakeOverRecord_pool;
  typedef DLList<TakeOverRecord_pool> TakeOverRecord_list;
  typedef SLFifoList<TakeOverRecord_pool> TakeOverRecord_fifo;


  bool getParam(const char * param, Uint32 * retVal) override { 
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
  ~Dbdih() override;

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

  /**
   * Methods used in Node Recovery Status module
   * -------------------------------------------
   */
  void execDBINFO_SCANREQ(Signal *);
  void execALLOC_NODEID_REP(Signal *);
  void execINCL_NODE_HB_PROTOCOL_REP(Signal *);
  void execNDBCNTR_START_WAIT_REP(Signal *);
  void execNDBCNTR_STARTED_REP(Signal *);
  void execSUMA_HANDOVER_COMPLETE_REP(Signal *);
  void execEND_TOREP(Signal *signal);
  void execLOCAL_RECOVERY_COMP_REP(Signal *signal);

  void sendEND_TOREP(Signal *signal, Uint32 startNodeId);
  bool check_stall_lcp_start(void);
  void check_node_not_restarted_yet(NodeRecordPtr nodePtr);
  void setNodeRecoveryStatus(Uint32 nodeId,
                             NodeRecord::NodeRecoveryStatus new_status);
  void setNodeRecoveryStatusInitial(NodeRecordPtr nodePtr);
  void initNodeRecoveryTimers(NodeRecordPtr nodePtr);
  void initNodeRecoveryStatus();
  void initNodeRecord(NodeRecordPtr);
  bool check_for_too_long_wait(Uint64 &lcp_max_wait_time,
                               Uint64 &lcp_stall_time,
                               NDB_TICKS now);
  void check_all_node_recovery_timers(void);
  bool check_node_recovery_timers(Uint32 nodeId);
  void calculate_time_remaining(Uint32 nodeId,
                                NDB_TICKS state_start_time,
                                NDB_TICKS now,
                                NodeRecord::NodeRecoveryStatus state,
                                Uint32 *node_waited_for,
                                Uint64 *time_since_state_start,
                                NodeRecord::NodeRecoveryStatus *max_status);
  void calculate_most_recent_node(Uint32 nodeId,
                          NDB_TICKS state_start_time,
                          NodeRecord::NodeRecoveryStatus state,
                          Uint32 *most_recent_node,
                          NDB_TICKS *most_recent_start_time,
                          NodeRecord::NodeRecoveryStatus *most_recent_state);
  const char* get_status_str(NodeRecord::NodeRecoveryStatus status);
  void fill_row_with_node_restart_status(NodeRecordPtr nodePtr,
                                         Ndbinfo::Row &row);
  void write_zero_columns(Ndbinfo::Row &row, Uint32 num_rows);
  void handle_before_master(NodeRecordPtr nodePtr, Ndbinfo::Row &row);
  /* End methods for Node Recovery Status module */

  void execDUMP_STATE_ORD(Signal *);
  void execNDB_TAMPER(Signal *);
  void execDEBUG_SIG(Signal *);
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
  void execUPDATE_FRAG_STATEREQ(Signal *);
  void execUPDATE_FRAG_STATECONF(Signal *);
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
  void execCHECK_LCP_IDLE_ORD(Signal *);

  void execDIH_GET_TABINFO_REQ(Signal*);
  void execSET_UP_MULTI_TRP_CONF(Signal*);

  /**
   * A number of functions used to find out if any node is currently is
   * restarting.
   */
  void execCHECK_NODE_RESTARTREQ(Signal*);
  void check_node_in_restart(Signal*, BlockReference, Uint32);
  void sendCHECK_NODE_RESTARTCONF(Signal*, BlockReference, Uint32);

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

  void switch_primary_stop_node(Signal* signal, Uint32, Uint32);

  MutexHandle2<DIH_SWITCH_PRIMARY_MUTEX> c_switchPrimaryMutexHandle;
  MutexHandle2<DIH_FRAGMENT_INFO> c_fragmentInfoMutex_lcp;

  /* LCP Pausing module start */
  void execFLUSH_LCP_REP_REQ(Signal*);
  void execFLUSH_LCP_REP_CONF(Signal*);
  void execPAUSE_LCP_REQ(Signal*);
  void execPAUSE_LCP_CONF(Signal*);

  void sendPAUSE_LCP_REQ(Signal*, bool pause);
  bool check_if_lcp_idle(void);
  void pause_lcp(Signal *signal,
                 Uint32 startNode,
                 BlockReference sender_ref);
  void unpause_lcp(Signal *signal,
                   Uint32 startNode,
                   BlockReference sender_ref,
                   PauseLcpReq::PauseAction pauseAction);
  void check_for_pause_action(Signal *signal,
                              StartLcpReq::PauseStart pauseStart);
  void end_pause(Signal *signal, PauseLcpReq::PauseAction pauseAction);
  void stop_pause(Signal *signal);
  void handle_node_failure_in_pause(Signal *signal);
  void dequeue_lcp_rep(Signal*);
  void start_copy_meta_data(Signal*);
  void start_lcp(Signal*);
  void start_lcp_before_mutex(Signal*);
  void queue_lcp_frag_rep(Signal *signal, LcpFragRep *lcpReport);
  void queue_lcp_complete_rep(Signal *signal, Uint32 lcpId);
  void init_lcp_pausing_module(void);
  bool check_pause_state_sanity(void);
  void check_pause_state_lcp_idle(void);

  /**
   * This is only true when an LCP is running and it is running with
   * support for PAUSE LCP (all DIH nodes support it). Actually this
   * is set when we have passed the START_LCP_REQ step. After this
   * step we release the fragment info mutex if we can use the pause
   * lcp protocol with all nodes.
   */
  bool c_lcp_runs_with_pause_support; /* Master state */

  /**
   * This is the state in the master that keeps track of where the master is 
   * in the PAUSE LCP process. We can follow two different tracks in the
   * state traversal.
   *
   * 1) When the starting node is included into the LCP as part of PAUSE LCP
   *    handling. This is the expected outcome after pausing. The LCP didn't
   *    complete while we were pausing. We need to be included into the LCP
   *    here to ensure that the LCP state in the starting node is kept up to
   *    date during the rest of the LCP.
   *
   * PAUSE_LCP_IDLE -> PAUSE_LCP_REQUESTED
   * PAUSE_LCP_REQUESTED -> PAUSE_START_LCP_INCLUSION
   * PAUSE_START_LCP_INCLUSION -> PAUSE_IN_LCP_COPY_META_DATA
   * PAUSE_IN_LCP_COPY_META_DATA -> PAUSE_COMPLETE_LCP_INCLUSION
   * PAUSE_COMPLETE_LCP_INCLUSION -> PAUSE_IN_LCP_UNPAUSE
   * PAUSE_IN_LCP_UNPAUSE -> PAUSE_LCP_IDLE
   *
   * 2) When the starting node isn't included into the LCP as part of PAUSE
   *    LCP handling. While we were pausing the LCP completed. Thus no need
   *    to include the new node into the LCP since no more updates of the
   *    LCP state will happen after the pause.
   *
   * PAUSE_LCP_IDLE -> PAUSE_LCP_REQUESTED
   * PAUSE_LCP_REQUESTED -> PAUSE_NOT_IN_LCP_COPY_META_DATA
   * PAUSE_NOT_IN_LCP_COPY_META_DATA -> PAUSE_NOT_IN_LCP_UNPAUSE
   * PAUSE_NOT_IN_LCP_UNPAUSE -> PAUSE_LCP_IDLE
   */
  enum PauseLCPState
  {
    PAUSE_LCP_IDLE = 0,
    PAUSE_LCP_REQUESTED = 1,
    /* States to handle inclusion in LCP. */
    PAUSE_START_LCP_INCLUSION = 2,
    PAUSE_IN_LCP_COPY_META_DATA = 3,
    PAUSE_COMPLETE_LCP_INCLUSION = 4,
    PAUSE_IN_LCP_UNPAUSE = 5,
    /* States to handle not included in LCP */
    PAUSE_NOT_IN_LCP_COPY_META_DATA = 6,
    PAUSE_NOT_IN_LCP_UNPAUSE = 7
  };
  PauseLCPState c_pause_lcp_master_state;

  /**
   * Bitmask of nodes that we're expecting a PAUSE_LCP_CONF response from.
   * This bitmask is cleared if the starting node dies (or for that matter
   * if any node dies since this will cause the starting node to also fail).
   * The PAUSE_LCP_REQ_Counter is only used in the master node.
   */
  SignalCounter c_PAUSE_LCP_REQ_Counter; /* Master state */

  /**
   * We need to keep track of the LQH nodes that participated in the PAUSE
   * LCP request to ensure that we unpause the same set of nodes in the
   * unpause request. If the LCP completes between as part of the pause
   * request phase, then the m_participatingLQH bitmap will be cleared and
   * we need this bitmap also to unpause the participants even if the
   * LCP has completed to ensure that the pause state is reset. This variable
   * is used to make sure that we retain this bitmap independent of what
   * happens with the LCP.
   */
  NdbNodeBitmask c_pause_participants;

  /**
   * This variable states which is the node starting up that requires a
   * pause of the LCP to copy the meta data during an ongoing LCP.
   * If the node fails this variable is set to RNIL to indicate we no
   * longer need to worry about signals handling this pause.
   *
   * This is also the state variable that says that pause lcp is ongoing
   * in this participant.
   */
  Uint32 c_pause_lcp_start_node;

  bool is_pause_for_this_node(Uint32 node)
  {
    return (node == c_pause_lcp_start_node);
  }

  /**
   * When is_lcp_paused is true then c_dequeue_lcp_rep_ongoing is false.
   * When is_lcp_paused is false then c_dequeue_lcp_rep_ongoing is true
   * until we have dequeued all queued requests. Requests will be
   * queued as long as either of them are true to ensure that we keep
   * the order of signals.
   */
  bool is_lcp_paused()
  {
    return (c_pause_lcp_start_node != RNIL);
  }
  bool c_dequeue_lcp_rep_ongoing;

  /**
   * Last LCP id we heard LCP_COMPLETE_REP from local LQH. We record this
   * to ensure we only get one LCP_COMPLETE_REP per LCP from our local
   * LQH.
   */
  Uint32 c_last_id_lcp_complete_rep;
  bool c_queued_lcp_complete_rep;

  /**
   * As soon as we have some LCP_FRAG_REP or LCP_COMPLETE_REP queued, this
   * variable gives us the lcp Id of the paused LCP.
   */
  Uint32 c_lcp_id_paused;

  /**
   * We set the LCP Id when receiving COPY_TABREQ to be used in the
   * updateLcpInfo routine.
   */
  Uint32 c_lcp_id_while_copy_meta_data; /* State in starting node */

  /**
   * A bitmap for outstanding FLUSH_LCP_REP_REQ messages to know
   * when all nodes have sent their reply. This bitmap is used in all nodes
   * that receive the PAUSE_LCP_REQ request.
   */
  SignalCounter c_FLUSH_LCP_REP_REQ_Counter;
  /* LCP Pausing module end   */

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
public:
  void execDIGETNODESREQ(Signal *);
  void execDIH_SCAN_TAB_REQ(Signal *);
  void execDIVERIFYREQ(Signal *);
private:
  void execSTTOR(Signal *);
  void execDIH_SCAN_TAB_COMPLETE_REP(Signal*);
  void execGCP_SAVEREF(Signal *);
  void execGCP_TCFINISHED(Signal *);
  void execGCP_TCFINISHED_sync_conf(Signal* signal, Uint32 cb, Uint32 err);
  void execREAD_NODESCONF(Signal *);
  void execNDB_STTOR(Signal *);
  void execDICTSTARTCONF(Signal *);
  void execNDB_STARTREQ(Signal *);
  void execGETGCIREQ(Signal *);
  void execGET_LATEST_GCI_REQ(Signal*);
  void execSET_LATEST_LCP_ID(Signal*);
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
  void execREDO_STATE_REP(Signal* signal);

  void execPREP_DROP_TAB_REQ(Signal* signal);
  void execDROP_TAB_REQ(Signal* signal);

  void execALTER_TAB_REQ(Signal* signal);

  void execCREATE_FRAGMENTATION_REQ(Signal*);
  bool verify_fragmentation(Uint16* fragments,
                            Uint32 partition_count,
                            Uint32 partition_balance,
                            Uint32 ldm_count) const;
  
  void waitDropTabWritingToFile(Signal *, TabRecordPtr tabPtr);
  void checkDropTabComplete(Signal *, TabRecordPtr tabPtr);

  void execDICT_LOCK_CONF(Signal* signal);
  void execDICT_LOCK_REF(Signal* signal);

  void execUPGRADE_PROTOCOL_ORD(Signal* signal);

  void execCREATE_NODEGROUP_IMPL_REQ(Signal*);
  void execDROP_NODEGROUP_IMPL_REQ(Signal*);

  void execSTART_NODE_LCP_CONF(Signal *signal);
  void handleStartLcpReq(Signal*, StartLcpReq*);
  StartLcpReq c_save_startLcpReq;
  bool c_start_node_lcp_req_outstanding;

  // Statement blocks
//------------------------------------
// Methods that send signals
//------------------------------------
  void nullRoutine(Signal *, Uint32 nodeId, Uint32);
  void sendCOPY_GCIREQ(Signal *, Uint32 nodeId, Uint32);
  void sendDIH_SWITCH_REPLICA_REQ(Signal *, Uint32 nodeId, Uint32);
  void sendEND_TOREQ(Signal *, Uint32 nodeId, Uint32);
  void sendGCP_COMMIT(Signal *, Uint32 nodeId, Uint32);
  void sendGCP_PREPARE(Signal *, Uint32 nodeId, Uint32);
  void sendGCP_SAVEREQ(Signal *, Uint32 nodeId, Uint32);
  void sendSUB_GCP_COMPLETE_REP(Signal*, Uint32 nodeId, Uint32);
  void sendINCL_NODEREQ(Signal *, Uint32 nodeId, Uint32);
  void sendMASTER_GCPREQ(Signal *, Uint32 nodeId, Uint32);
  void sendMASTER_LCPREQ(Signal *, Uint32 nodeId, Uint32);
  void sendMASTER_LCPCONF(Signal * signal, Uint32 fromLine);
  void sendSTART_RECREQ(Signal *, Uint32 nodeId, Uint32);
  void sendSTART_INFOREQ(Signal *, Uint32 nodeId, Uint32);
  void sendSTOP_ME_REQ(Signal *, Uint32 nodeId, Uint32);
  void sendTC_CLOPSIZEREQ(Signal *, Uint32 nodeId, Uint32);
  void sendTCGETOPSIZEREQ(Signal *, Uint32 nodeId, Uint32);
  void sendUPDATE_TOREQ(Signal *, Uint32 nodeId, Uint32);
  void sendSTART_LCP_REQ(Signal *, Uint32 nodeId, Uint32);

  void sendLCP_FRAG_ORD(Signal*, NodeRecord::FragmentCheckpointInfo info);
  void sendLastLCP_FRAG_ORD(Signal *);
  
  void sendCopyTable(Signal *, CopyTableNode* ctn,
                     BlockReference ref, Uint32 reqinfo);
  void sendDihfragreq(Signal *,
                      TabRecordPtr regTabPtr,
                      Uint32 fragId);

  void sendStartFragreq(Signal *,
                        TabRecordPtr regTabPtr,
                        Uint32 fragId);

  void sendAddFragreq(Signal*,
                      ConnectRecordPtr,
                      TabRecordPtr,
                      Uint32 fragId,
                      bool rcu_lock_held);
  void addTable_closeConf(Signal* signal, Uint32 tabPtrI);
  void resetReplicaSr(TabRecordPtr tabPtr);
  void resetReplicaLcp(ReplicaRecord * replicaP, Uint32 stopGci);
  void resetReplica(Ptr<ReplicaRecord>);

/**
 * Methods part of Transaction Handling module
 */
  void start_scan_on_table(TabRecordPtr, Signal*, Uint32, EmulatedJamBuffer*);
  void complete_scan_on_table(TabRecordPtr tabPtr, Uint32, EmulatedJamBuffer*);

  bool prepare_add_table(TabRecordPtr, ConnectRecordPtr, Signal*);
  void commit_new_table(TabRecordPtr);

  void make_node_usable(NodeRecord *nodePtr);
  void make_node_not_usable(NodeRecord *nodePtr);

  void start_add_fragments_in_new_table(TabRecordPtr,
                                        ConnectRecordPtr,
                                        const Uint16 buf[],
                                        const Uint32 bufLen,
                                        Signal *signal);
  void make_new_table_writeable(TabRecordPtr, ConnectRecordPtr, bool);
  void make_new_table_read_and_writeable(TabRecordPtr,
                                         ConnectRecordPtr,
                                         Signal*);
  bool make_old_table_non_writeable(TabRecordPtr, ConnectRecordPtr);
  void make_table_use_new_replica(TabRecordPtr,
                                  FragmentstorePtr fragPtr,
                                  ReplicaRecordPtr,
                                  Uint32 replicaType,
                                  Uint32 destNodeId);
  void make_table_use_new_node_order(TabRecordPtr,
                                     FragmentstorePtr,
                                     Uint32,
                                     Uint32*);
  void make_new_table_non_writeable(TabRecordPtr);
  void drop_fragments_from_new_table_view(TabRecordPtr, ConnectRecordPtr);

//------------------------------------
// Methods for LCP functionality
//------------------------------------
  void checkKeepGci(TabRecordPtr, Uint32, Fragmentstore*, Uint32);
  void checkLcpStart(Signal *, Uint32 lineNo, Uint32 delay);
  bool checkStartMoreLcp(Signal *, Uint32 nodeId, bool startNext);
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
  void readingGcpLab(Signal *, FileRecordPtr regFilePtr, Uint32 bytes_read);
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
  void send_COPY_GCIREQ_data_v1(Signal*, Uint32);
  void send_COPY_GCIREQ_data_v2(Signal*, Uint32, Uint32);
  void send_START_MECONF_data_v1(Signal*, Uint32);
  void send_START_MECONF_data_v2(Signal*, Uint32, Uint32);
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
  void readReplicas(RWFragment* rf,
                    TabRecord *regTabPtr,
                    FragmentstorePtr regFragptr);
  void updateLcpInfo(TabRecord *regTabPtr,
                     Fragmentstore *regFragPtr,
                     ReplicaRecord *regReplicaPtr);
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
  void checkLcpCompletedLab(Signal *, Uint32);
  void initLcpLab(Signal *, Uint32 masterRef, Uint32 tableId);
  void startGcpLab(Signal *);
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
  void allNodesLcpCompletedLab(Signal *);
  void nodeRestartPh2Lab(Signal *);
  void nodeRestartPh2Lab2(Signal *);
  void initGciFilesLab(Signal *);
  void dictStartConfLab(Signal *);
  void nodeDictStartConfLab(Signal *, Uint32 nodeId);
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
  void failedNodeLcpHandling(Signal*, NodeRecordPtr failedNodePtr, bool &);
  void failedNodeSynchHandling(Signal *, NodeRecordPtr failedNodePtr);
  void checkCopyTab(Signal*, NodeRecordPtr failedNodePtr);

  Uint32 compute_max_failure_time();
  void setGCPStopTimeouts(Signal*,
                          bool set_gcp_save_max_lag = true,
                          bool set_micro_gcp_max_lag = true);
  void sendINFO_GCP_STOP_TIMER(Signal*);
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
  bool findStartGci(Ptr<ReplicaRecord> fstReplicaPtr,
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
  void insertCopyFragmentList(TabRecord *tabPtr,
                              Fragmentstore *fragPtr,
                              Uint32 my_fragid);
  void allocStoredReplica(FragmentstorePtr regFragptr,
                          ReplicaRecordPtr& newReplicaPtr,
                          Uint32 nodeId,
                          Uint32 fragId,
                          Uint32 tableId);
  Uint32 extractNodeInfo(EmulatedJamBuffer *jambuf,
                         const Fragmentstore * fragPtr,
                         Uint32 nodes[],
                         bool crash_on_error = true);
  Uint32 findLocalFragment(const TabRecord *,
                           Ptr<Fragmentstore> & fragPtr,
                           EmulatedJamBuffer *jambuf);
  Uint32 findPartitionOrder(const TabRecord *tabPtrP,
                            FragmentstorePtr fragPtr);
  Uint32 findFirstNewFragment(const TabRecord *,
                              Ptr<Fragmentstore> & fragPtr,
                              Uint32 fragId,
                              EmulatedJamBuffer *jambuf);
  bool check_if_local_fragment(EmulatedJamBuffer *jambuf,
                               const Fragmentstore *fragPtr);
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
  void initFragstore(FragmentstorePtr regFragptr, Uint32 fragId);
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
			    Ptr<ReplicaRecord>);
  void updateNodeInfo(FragmentstorePtr regFragptr);

//------------------------------------
// Fragment allocation, deallocation and
// find methods
//------------------------------------
  void allocFragments(Uint32 noOfFragments, TabRecordPtr regTabPtr);
  void releaseFragments(TabRecordPtr regTabPtr);
  void getFragstore(const TabRecord *, Uint32 fragNo, FragmentstorePtr & ptr);
  void getFragstoreCanFail(const TabRecord *,
                           Uint32 fragNo,
                           FragmentstorePtr & ptr);
  void initialiseFragstore();

  void wait_old_scan(Signal*);
  Uint32 add_fragments_to_table(Ptr<TabRecord>,
                                const Uint16 buf[],
                                const Uint32 bufLen);
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

  void handleTakeOverMaster(Signal *, Uint32 takeOverPtr);
  void handleTakeOverNewMaster(Signal *, Uint32 takeOverPtr);

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

  void getTabInfo(Signal*);
  void getTabInfo_send(Signal*, TabRecordPtr);
  void getTabInfo_sendComplete(Signal*, Uint32, Uint32);
  int getTabInfo_copyTableToSection(SegmentedSectionPtr & ptr, CopyTableNode);
  int getTabInfo_copySectionToPages(TabRecordPtr, SegmentedSectionPtr);

  // Initialisation
  void initData();
  void initRecords();

  // Variables to support record structures and their free lists

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

  NodeGroupRecord *nodeGroupRecord;
  RSS_OP_SNAPSHOT(cnghash);

  Uint32 c_nextNodeGroup;
  Uint16 c_next_replica_node[MAX_NDB_NODE_GROUPS][NDBMT_MAX_WORKER_INSTANCES];

  /**
   * Temporary variables used by CREATE_FRAGMENTATION_REQ
   */
  Uint16
    tmp_next_replica_node[MAX_NDB_NODE_GROUPS][NDBMT_MAX_WORKER_INSTANCES];
  Uint8
    tmp_next_replica_node_set[MAX_NDB_NODE_GROUPS][NDBMT_MAX_WORKER_INSTANCES];
  Uint16 tmp_node_group_id[MAX_NDB_PARTITIONS];
  Uint16 tmp_fragments_per_ldm[MAX_NDB_NODES][NDBMT_MAX_WORKER_INSTANCES];
  Uint16 tmp_fragments_per_node[MAX_NDB_NODES];
  void init_next_replica_node(
    Uint16
     (*next_replica_node)[MAX_NDB_NODE_GROUPS][NDBMT_MAX_WORKER_INSTANCES],
     Uint32 noOfReplicas);

  NodeRecord *nodeRecord;

  PageRecord *pageRecord;
  Uint32 cfirstfreepage;
  Uint32 cpageFileSize;

  Uint32 cnoFreeReplicaRec;
  Uint32 creplicaFileSize;
  RSS_OP_SNAPSHOT(cnoFreeReplicaRec);

  TabRecord *tabRecord;
  Uint32 ctabFileSize;

  /**
   * Methods and variables used to control the node restart phase where a
   * node gets the data back from an alive node. This has two parts, one
   * part in the master node which controls that certain critical data is
   * only updated one at a time. The other part is in the starting node
   * where there is one thread for each parallel fragment copy process.
   *
   * There is also a set of signals used for the take over processes.
   *
   * START_FRAGREQ
   * Before performing the actual copy phase the starting node needs
   * information about all fragments to start. This signal is sent from the
   * from the starting nodes DBDIH to the starting nodes DBLQH and to the
   * actual instance that will handle the fragment replica.
   *
   * START_RECREQ/CONF:
   * This is sent from the starting node to all LDM instances to tell them
   * that they have now received all START_FRAGREQ, no more will be sent. After
   * receiving this signal the LDM instances can start reading the fragments
   * from disk and applying the REDO log to get them as up to date as possible
   * before we start the copy phase. One could also rebuild the ordered
   * indexes here.
   *
   * START_TOREQ/CONF/REF:
   * This is sent from the starting node to allocate a take over record in the
   * master node. This is sent once at the start of the take over processing.
   *
   * UPDATE_TOREQ/CONF/REF:
   * This is sent from a starting node to inform the master of a step forward
   * in the copy process. In some of those phases it means acquiring the global
   * cluster mutex on updating fragment state, in some phases it means
   * releasing the same mutex. Also the global switch primary replica mutex
   * can be acquired and released in certain phases.
   * 
   * This is sent once before UPDATE_FRAGSTATEREQ/CONF and once after for each
   * fragment replica that the starting node will take over.
   *
   * UPDATE_FRAGSTATEREQ/CONF/REF:
   * This signal is sent to all nodes from starting node informing them of a
   * new replica entering a certain fragment. After the CONF has been received
   * we're sure that all transactions will involve this new node when updating
   * this fragment. We have a distribution key that can be used to verify if a
   * particular transaction have included the node in its transaction.
   *
   * This is sent once per fragment replica the starting node is taking over.
   *
   * PREPARE_COPY_FRAGREQ/CONF/REF:
   * This is sent from starting node to the LDM instance in starting node
   * asking for the maxPage value. Once per fragment replica to take over.
   *
   * COPY_FRAGREQ/CONF/REF:
   * This is sent to the copying node with the maxPage value. This will start
   * a scan in the copying node and copying over all records that have a newer
   * GCI than the one already restored from an LCP (the maxPage is also
   * somehow involved in this decision).
   * This signal relates to copying one fragment and is done after updating the
   * fragment state to ensure that all future transactions will involve the
   * node as well. There is another fragment state update performed after this
   * copy is completed.
   *
   * Sent once per fragment replica the starting node is taking over.
   *
   * COPY_ACTIVEREQ/CONF/REF:
   * This tells the starting node that the fragment replica is now copied over
   * and is in an active state.
   *
   * Sent per fragment replica the starting node is taking over.
   *
   * END_TOREQ/CONF/REF:
   * This is sent from the starting node to the master node. The response can
   * take a long time since it involves waiting for the proper LCP to complete
   * to ensure that the node is fully recoverable even on its own without other
   * nodes to assist it. For this to happen the node requires a complete
   * LCP to happen which started after we completed the copying of all
   * fragments and where the new node was part of the LCP.
   *
   * This is sent only once at the end of the take over process.
   * Multiple nodes can be in the take over process at the same time.
   *
   * CONTINUEB:
   * This signal is used a lot to control execution in the local DIH block.
   * It is used to start parallel threads and to ensure that we don't
   * execute for too long without giving other threads a chance to execute
   * or other signals to the DIH block.
   *
   * Variable descriptions
   * ---------------------
   *
   * We have a pool of take over records used by the master for
   * handling parallel node recoveries. We also use the same pool
   * in starting nodes to keep one main take over record and then
   * one record for each parallel thread that we can copy from in
   * parallel.
   *
   * Then for each thread that takes over we keep one record.
   * These records are always in one list.
   *
   * All threads are scanning fragments to find a fragment replica that needs
   * take over. When they discover one they try to update the fragment replica
   * state on the master (start takeover), which requires that they
   * temporarily become the activeThread. If this succeeds they are placed in
   * the activeThread variable. If it isn't successful they are placed into the
   * c_queued_for_start_takeover_list. When the global fragment replica state
   * update is completed, the list is checked to see if a queued thread should
   * become the activeThread. Then COPY_FRAGREQ is sent and the thread is
   * placed on the c_active_copy_instance_list. When start take over phase is
   * completed one starts the next take over from the list and sends off
   * COPY_FRAGREQ whereafter it is placed in the c_active_copy_thread_list.
   *
   * When the copy phase is completed the take over record is removed
   * from the c_active_copy_thread_list and one tries to become
   * the active thread. If it isn't successful the take over record
   * is placed into the c_queued_for_end_takeover_list. When the
   * active thread is done it gets a new record from either the
   * c_queued_for_start_takeover_list or from
   * c_queued_for_commit_takeover_list. c_queued_for_commit_takeover_list has
   * higher priority. Finally when there is no more fragments to find
   * for a certain thread after ending the takeover of a fragment
   * the record is placed into the c_completed_copy_thread_list.
   * When all threads are placed into this list then all threads are
   * done with the copy phase.
   *
   * Finally we start up the phase where we activate the REDO log.
   * During this phase the records are placed into the
   * c_active_copy_thread_list. When a thread is completed with
   * this phase the take over record is released. When all threads
   * are completed we are done with this parallelisation phase and the
   * node copying phase is completed whereafter we can also release the
   * main take over record.
   *
   * c_takeOverPool:
   * This is the pool of records used by both master and starting
   * node.
   * 
   * c_mainTakeOverPtr:
   * This is the main record used by the starting node.
   *
   * c_queued_for_start_takeover_list:
   * A takeover thread is ready to copy a fragment, but has to wait until
   * another thread is ready with its master communication before
   * proceeding.
   *
   * c_queued_for_commit_takeover_list:
   * A takeover thread is ready to complete the copy of a fragment, it has to
   * wait a while since there is another thread currently communicating with
   * the master node.
   *
   * These two are queues, so we implement them as a Single Linked List,
   * FIFO queue, this means a SLFifoList.
   *
   * c_max_take_over_copy_threads:
   * The is the limit on the number of threads to use. Effectively the
   * parallelisation can never be higher than the number of LDM instances
   * that are used in the cluster.
   *
   * c_active_copy_threads_list:
   * Takeover threads are placed into this list while they are actively
   * copying a fragment at this point in time. We need to take things out
   * of this list in any order, so we need Double Linked List.
   *
   * c_activeTakeOverList:
   * While scanning fragments to find a fragment that our thread is
   * responsible for, we are placed into this list. This list handling
   * is on the starting node.
   * 
   * This list is also used on the master node to keep track of node
   * take overs.
   *
   * c_completed_copy_threads_list:
   * This is a list where an thread is placed after completing the first
   * phase of scanning for fragments to copy. Some threads will be done
   * with this very quickly if we have more threads scanning than we have
   * LDM instances in the cluster. After completing the second phase where
   * we change state of ongoing transactions we release the thread.
   *
   * c_activeThreadTakeOverPtr:
   * This is the pointer to the currently active thread using the master
   * node to update the fragment state.
   *
   */
#define ZTAKE_OVER_THREADS 16
#define ZMAX_TAKE_OVER_THREADS 64
  Uint32 c_max_takeover_copy_threads;

  TakeOverRecord_pool c_takeOverPool;
  TakeOverRecord_list c_activeTakeOverList;
  TakeOverRecord_fifo c_queued_for_start_takeover_list;
  TakeOverRecord_fifo c_queued_for_commit_takeover_list;
  TakeOverRecord_list c_active_copy_threads_list;
  TakeOverRecord_list c_completed_copy_threads_list;
  TakeOverRecordPtr c_mainTakeOverPtr;
  TakeOverRecordPtr c_activeThreadTakeOverPtr;

  /* List used in takeover handling in master part. */
  TakeOverRecord_list c_masterActiveTakeOverList;


//-----------------------------------------------------
// TakeOver Record specific methods, starting node part
//-----------------------------------------------------
  void startTakeOver(Signal *,
                     Uint32 startNode,
                     Uint32 toNode,
                     const struct StartCopyReq*);

  void startNextCopyFragment(Signal *, Uint32 takeOverPtr);
  void toCopyFragLab(Signal *, Uint32 takeOverPtr);
  void toStartCopyFrag(Signal *, TakeOverRecordPtr);
  void toCopyCompletedLab(Signal *, TakeOverRecordPtr regTakeOverptr);

  void nr_start_fragments(Signal*, TakeOverRecordPtr);
  void nr_start_fragment(Signal*, TakeOverRecordPtr, ReplicaRecordPtr);
  void nr_run_redo(Signal*, TakeOverRecordPtr);
  void nr_start_logging(Signal*, TakeOverRecordPtr);

  bool check_takeover_thread(TakeOverRecordPtr takeOverPtr,
                             FragmentstorePtr fragPtr,
                             Uint32 fragmentReplicaInstanceKey);
  void send_continueb_start_next_copy(Signal *signal,
                                      TakeOverRecordPtr takeOverPtr);
  void init_takeover_thread(TakeOverRecordPtr takeOverPtr,
                            TakeOverRecordPtr mainTakeOverPtr,
                            Uint32 number_of_threads,
                            Uint32 thread_id);
  void start_next_takeover_thread(Signal *signal);
  void start_thread_takeover_logging(Signal *signal);
  void send_continueb_nr_start_logging(Signal *signal,
                                       TakeOverRecordPtr takeOverPtr);
  bool thread_takeover_completed(Signal *signal,
                                 TakeOverRecordPtr takeOverPtr);
  bool thread_takeover_copy_completed(Signal *signal,
                                      TakeOverRecordPtr takeOverPtr);
  void release_take_over_threads(void);
  void check_take_over_completed_correctly(void);

  void sendStartTo(Signal* signal, TakeOverRecordPtr);
  void sendUpdateTo(Signal* signal, TakeOverRecordPtr);
  void sendUpdateFragStateReq(Signal *,
                              Uint32 startGci,
                              Uint32 storedType,
                              TakeOverRecordPtr takeOverPtr);

  void releaseTakeOver(TakeOverRecordPtr takeOverPtr,
                       bool from_master,
                       bool skip_check = false);

//-------------------------------------------------
// Methods for take over functionality, master part
//-------------------------------------------------
  void switchPrimaryMutex_locked(Signal* signal, Uint32, Uint32);
  void switchPrimaryMutex_unlocked(Signal* signal, Uint32, Uint32);
  void check_force_lcp(Ptr<TakeOverRecord> takeOverPtr);
  void abortTakeOver(Signal*, TakeOverRecordPtr);
  void updateToReq_fragmentMutex_locked(Signal*, Uint32, Uint32);
  bool findTakeOver(Ptr<TakeOverRecord> & ptr, Uint32 failedNodeId);
  void insertBackup(FragmentstorePtr regFragptr, Uint32 nodeId);

  /*
    2.4  C O M M O N    S T O R E D    V A R I A B L E S
    ----------------------------------------------------
  */
  bool c_performed_copy_phase;

  struct DIVERIFY_queue
  {
    DIVERIFY_queue() {
      m_ref = 0;
      cfirstVerifyQueue = clastVerifyQueue = 0;
      m_empty_done = 1;
    }
    Uint32 cfirstVerifyQueue;
    Uint32 clastVerifyQueue;
    Uint32 m_empty_done;
    Uint32 m_ref;
    char pad[NDB_CL_PADSZ(sizeof(void*) + 4 * sizeof(Uint32))];
  };

  bool isEmpty(const DIVERIFY_queue&);
  void enqueue(DIVERIFY_queue&);
  void dequeue(DIVERIFY_queue&);
  void emptyverificbuffer(Signal *, Uint32 q, bool aContintueB);
  void emptyverificbuffer_check(Signal*, Uint32, Uint32);

  DIVERIFY_queue c_diverify_queue[MAX_NDBMT_TC_THREADS];
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
      NDB_TICKS m_start_time;
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
    // To avoid double send of SUB_GCP_COMPLETE_REP to SUMA via DBLQH.
    Uint64 m_last_sent_gci;
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
      NDB_TICKS m_start_time;
    } m_master;
  } m_micro_gcp;

  struct GcpMonitor
  {
    struct
    {
      Uint32 m_gci;
      Uint32 m_elapsed_ms; //MilliSec since last GCP_SAVEed
      Uint32 m_max_lag_ms; //Max allowed lag(ms) before 'crashSystem'
      bool m_need_max_lag_recalc; // Whether max lag need to be recalculated
#ifdef ERROR_INSERT
      bool test_set_max_lag; // Testing
#endif
    } m_gcp_save;

    struct
    {
      Uint64 m_gci;
      Uint32 m_elapsed_ms; //MilliSec since last GCP_COMMITed
      Uint32 m_max_lag_ms; //Max allowed lag(ms) before 'crashSystem'
      bool m_need_max_lag_recalc; // Whether max lag need to be recalculated
#ifdef ERROR_INSERT
      bool test_set_max_lag; // Testing
#endif
    } m_micro_gcp;

    NDB_TICKS m_last_check; //Time GCP monitor last checked

#ifdef ERROR_INSERT
    Uint32 m_savedMaxCommitLag;  // Testing
#endif
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
    static constexpr Uint32 WAIT_CNT = 2;
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
    LCP_WAIT_MUTEX         = 3,  // Only master
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

    /**
     * State of stalling LCPs for node restarts
     */
    Uint32 lcpStallStart;  /* Has started stalling lcp start */
    NDB_TICKS lastLogTime; /* Last time we logged state of stall */
    NDB_TICKS m_start_lcp_check_time; /* Time of stalling started */
    Uint32 stall_node_waiting_for; /* The node we've logged about waiting for */

    Uint32 lcpStart;
    Uint32 lcpStopGcp; 
    Uint32 keepGci;      /* USED TO CALCULATE THE GCI TO KEEP AFTER A LCP  */
    Uint32 oldestRestorableGci;

    bool lcpManualStallStart; /* User requested stall of start (testing only) */
    
    NDB_TICKS m_start_time; // When last LCP was started
    Uint64    m_lcp_time;   // How long last LCP took
    Uint32    m_lcp_trylock_timeout;

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
    NdbNodeBitmask m_allReplicasQueuedLQH;
    
    Uint32 m_masterLcpDihRef;
    bool   m_MASTER_LCPREQ_Received;
    Uint32 m_MASTER_LCPREQ_FailedNodeId;

    Uint32 m_lastLCP_COMPLETE_REP_id;
    Uint32 m_lastLCP_COMPLETE_REP_ref;

    // Whether the 'lcp' is already completed under the
    // coordination of the failed master
    bool already_completed_lcp(Uint32 lcp, Uint32 current_master) const
    {
      const Uint32 last_completed_master_node =
        refToNode(m_lastLCP_COMPLETE_REP_ref);
      if (m_lastLCP_COMPLETE_REP_id == lcp &&
          last_completed_master_node != current_master &&
          last_completed_master_node == m_MASTER_LCPREQ_FailedNodeId)
      {
        return true;
      }
      return false;
    }

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
  Uint32 cMinTcFailNo;            /* Minimum TC handled failNo allowed to close GCP */

  BlockReference clocallqhblockref;
  BlockReference clocalqlqhblockref;
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
  NDB_TICKS c_current_time; // Updated approx. every 10ms

  /* Limit the number of concurrent table definition writes during LCP
   * This avoids exhausting the DIH page pool
   */
  CountingSemaphore c_lcpTabDefWritesControl;

public:
  enum LcpMasterTakeOverState {
    LMTOS_IDLE = 0,
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
   * After a node failure we want to increase the disk checkpoint speed until
   * we have completed the current ongoing node failure. We also increase the
   * checkpoint speed when we know that a node restart is ongoing.
   */
  bool c_increase_lcp_speed_after_nf;
  /**
   * Available nodegroups (ids) (length == cnoOfNodeGroups)
   *   use to support nodegroups 2,4,6 (not just consecutive nodegroup ids)
   */
  Uint32 c_node_groups[MAX_NDB_NODE_GROUPS];
  Uint32 cnoOfNodeGroups;
  Uint32 crestartGci;      /* VALUE OF GCI WHEN SYSTEM RESTARTED OR STARTED */
  
  /**
   * Counter variables keeping track of the number of outstanding signals
   * for particular signals in various protocols.
   */
  SignalCounter c_COPY_GCIREQ_Counter;
  SignalCounter c_COPY_TABREQ_Counter;
  SignalCounter c_UPDATE_FRAG_STATEREQ_Counter;
  SignalCounter c_DIH_SWITCH_REPLICA_REQ_Counter;
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
  typedef ArrayPool<WaitGCPProxyRecord> WaitGCPProxyRecord_pool;
  typedef DLList<WaitGCPProxyRecord_pool> WaitGCPProxyRecord_list;
  /**
   * Wait GCP (master)
   */
  struct WaitGCPMasterRecord {
    WaitGCPMasterRecord() { clientRef = 0;}
    Uint32 clientData;
    BlockReference clientRef;
    /**
     * GCI which must be completed before CONF sent
     * For WaitEpoch, it is not used, the next
     * completing epoch sends a CONF.
     */
    Uint32 waitGCI;

    /**
     * Special value indicating a request for shutdown sync
     */
    static const Uint32 ShutdownSyncGci = 0xffffffff;

    union { Uint32 nextPool; Uint32 nextList; };
    Uint32 prevList;
  };
  typedef Ptr<WaitGCPMasterRecord> WaitGCPMasterPtr;
  typedef ArrayPool<WaitGCPMasterRecord> WaitGCPMasterRecord_pool;

  /**
   * Pool/list of WaitGCPProxyRecord record
   */
  WaitGCPProxyRecord_pool waitGCPProxyPool;
  WaitGCPProxyRecord_list c_waitGCPProxyList;

  /**
   * Pool/list of WaitGCPMasterRecord record
   */
  WaitGCPMasterRecord_pool waitGCPMasterPool;
  typedef DLList<WaitGCPMasterRecord_pool> WaitGCPList;
  WaitGCPList c_waitGCPMasterList;
  WaitGCPList c_waitEpochMasterList;

  void checkWaitGCPProxy(Signal*, NodeId failedNodeId);
  void checkWaitGCPMaster(Signal*, NodeId failedNodeId);
  void checkShutdownSync();
  void emptyWaitGCPMasterQueue(Signal*, Uint64, WaitGCPList&);

  void getNodeBitmap(NdbNodeBitmask& map,
                     Uint32 listHead,
                     int (*versionFunction) (Uint32));

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
  
#define DIH_CDATA_SIZE _SYSFILE_FILE_SIZE
  /**
   * This variable must be at least the size of Sysfile::SYSFILE_SIZE32_v2
   */
  Uint32 cdata[DIH_CDATA_SIZE];       /* TEMPORARY ARRAY VARIABLE */

  /**
   * Sys file data
   */
  Sysfile sysfile;
  Uint32 sysfileDataToFile[DIH_CDATA_SIZE];

  /**
   * When a node comes up without filesystem
   *   we have to clear all LCP for that node
   */
  void handle_send_continueb_invalidate_node_lcp(Signal *signal);
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
  typedef ArrayPool<DictLockSlaveRecord> DictLockSlaveRecord_pool;
  DictLockSlaveRecord_pool c_dictLockSlavePool;

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
      LS_COMPLETE = 2,
      LS_RUNNING_MTO_TAB_SAVED = 3
    } m_state;
    
    StartLcpReq m_start_lcp_req;
    Uint32 m_keep_gci; // Min GCI is needed to restore LCP
    Uint32 m_stop_gci; // This GCI needs to be complete before LCP is restorable

    LocalLCPState() { reset();}
    
    void reset();
    void init(const StartLcpReq*);
    void init_master_take_over_idle_to_tab_saved();
    void lcp_frag_rep(const LcpFragRep*);
    void lcp_complete_rep(Uint32 gci);
    
    /**
     * @param gci - current GCI being made restorable (COPY_GCI)
     */
    bool check_cut_log_tail(Uint32 gci) const;
  } m_local_lcp_state;

  // MT LQH
  Uint32 c_fragments_per_node_;
  Uint32 getFragmentsPerNode();
  Uint32 getFragmentCount(Uint32 partitionBalance,
                          Uint32 numOfNodeGroups,
                          Uint32 numOfReplicas,
                          Uint32 numOfLDMs) const;
  /**
   * dihGetInstanceKey
   *
   * This method maps a fragment to a block instance key
   * This is the LDM instance which manages the fragment
   * on this node.
   * The range of an instance key is 1 to 
   * NDBMT_MAX_WORKER_INSTANCES inclusive.
   * 0 is the proxy block instance.
   */
  Uint32 dihGetInstanceKey(FragmentstorePtr tFragPtr)
  {
    ndbrequire(!tFragPtr.isNull());
    Uint32 log_part_id = tFragPtr.p->m_log_part_id;
    ndbrequire(log_part_id < MAX_INSTANCE_KEYS);
    return 1 + log_part_id;
  }
  Uint32 dihGetInstanceKey(Uint32 tabId, Uint32 fragId);
  Uint32 dihGetInstanceKeyCanFail(Uint32 tabId, Uint32 fragId);

  void log_setNoSend();
  /**
   * Get minimum version of nodes in alive-list
   */
  Uint32 getMinVersion() const;

  bool c_2pass_inr;

  /* Max LCP parallelism is node (version) specific */
  Uint8 getMaxStartedFragCheckpointsForNode(Uint32 nodeId) const;
  
  void isolateNodes(Signal* signal,
                    Uint32 delayMillis,
                    const NdbNodeBitmask& victims);

  NodeId c_handled_master_take_over_copy_gci;

  bool handle_master_take_over_copy_gci(Signal *signal,
                                        NodeId newMasterNodeId);

  RedoStateRep::RedoAlertState m_node_redo_alert_state[MAX_NDB_NODES];
  RedoStateRep::RedoAlertState m_global_redo_alert_state;
  RedoStateRep::RedoAlertState get_global_redo_alert_state();
  void sendREDO_STATE_REP_to_all(Signal*, Uint32 block, bool send_to_all);
  bool m_master_lcp_req_lcp_already_completed;

  void complete_restart_nr(Signal*);

  /* The highest data node id in the cluster. */
  Uint32 m_max_node_id;
  bool m_set_up_multi_trp_in_node_restart;
  bool m_use_classic_fragmentation;

  void find_min_used_log_part();
  bool select_ng(Uint32 fragNo,
                 Uint32 default_node_group,
                 NodeGroupRecordPtr & NGPtr,
                 Uint32 & err);
  void inc_ng(Uint32 fragNo,
              Uint32 noOfFragments,
              Uint32 partitionCount,
              Uint32 & default_node_group,
              Uint32 numNodeGroups);
  void add_nodes_to_fragment(Uint16 *fragments,
                             Uint32 & node_index,
                             Uint32 & count,
                             NodeGroupRecordPtr NGPtr,
                             Uint32 noOfReplicas);
  bool find_next_log_part(TabRecord *primTabPtrP, Uint32 & next_log_part);
  void getNodeGroupPtr(Uint32 nodeId, NodeGroupRecordPtr & NGPtr);
public:
  bool is_master() { return isMaster(); }
  
  NdbNodeBitmask c_shutdownReqNodes;
  void print_lcp_state();
};

#if (DIH_CDATA_SIZE < _SYSFILE_SIZE32_v2)
#error "cdata is to small compared to Sysfile size"
#endif


#undef JAM_FILE_ID

#endif


/*
   Copyright (c) 2003, 2018, Oracle and/or its affiliates. All rights reserved.

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

#ifndef DBLQH_H
#define DBLQH_H

#ifndef DBLQH_STATE_EXTRACT
#include <pc.hpp>
#include <ndb_limits.h>
#include <SimulatedBlock.hpp>
#include <SectionReader.hpp>
#include <IntrusiveList.hpp>
#include "ArrayPool.hpp"
#include <DLHashTable.hpp>

#include <NodeBitmask.hpp>
#include <signaldata/NodeRecoveryStatusRep.hpp>
#include <signaldata/LCP.hpp>
#include <signaldata/LqhTransConf.hpp>
#include <signaldata/CreateTab.hpp>
#include <signaldata/LqhFrag.hpp>
#include <signaldata/FsOpenReq.hpp>
#include <signaldata/DropTab.hpp>
#include <signaldata/CopyFrag.hpp>

// primary key is stored in TUP
#include "../dbtup/Dbtup.hpp"
#include "../dbacc/Dbacc.hpp"
#include "../dbtux/Dbtux.hpp"
#include "../backup/Backup.hpp"
#include "../restore.hpp"

class Dbacc;
class Dbtup;
class Dbtux;
class Lgman;
#endif // DBLQH_STATE_EXTRACT

#define JAM_FILE_ID 450

#ifdef DBLQH_C
// Constants
/* ------------------------------------------------------------------------- */
/*       CONSTANTS USED WHEN MASTER REQUESTS STATE OF COPY FRAGMENTS.        */
/* ------------------------------------------------------------------------- */
#define ZCOPY_CLOSING 0
#define ZCOPY_ONGOING 1
#define ZCOPY_ACTIVATION 2
/* ------------------------------------------------------------------------- */
/*       STATES FOR THE VARIABLE GCP_LOG_PART_STATE                          */
/* ------------------------------------------------------------------------- */
#define ZIDLE 0
#define ZWAIT_DISK 1
#define ZON_DISK 2
#define ZACTIVE 1
/* ------------------------------------------------------------------------- */
/*       STATES FOR THE VARIABLE CSR_PHASES_STARTED                          */
/* ------------------------------------------------------------------------- */
#define ZSR_NO_PHASE_STARTED 0
#define ZSR_PHASE1_COMPLETED 1
#define ZSR_PHASE2_COMPLETED 2
#define ZSR_BOTH_PHASES_STARTED 3
/* ------------------------------------------------------------------------- */
/*       THE NUMBER OF PAGES IN A MBYTE, THE TWO LOGARITHM OF THIS.          */
/*       THE NUMBER OF MBYTES IN A LOG FILE.                                 */
/*       THE MAX NUMBER OF PAGES READ/WRITTEN FROM/TO DISK DURING            */
/*       A WRITE OR READ.                                                    */
/* ------------------------------------------------------------------------- */
#define ZNOT_DIRTY 0
#define ZDIRTY 1
#define ZREAD_AHEAD_SIZE 8
/* ------------------------------------------------------------------------- */
/*       CONSTANTS OF THE LOG PAGES                                          */
/* ------------------------------------------------------------------------- */
#define ZPAGE_HEADER_SIZE 32
#define ZPAGE_SIZE 8192
#define ZPAGES_IN_MBYTE 32
#define ZTWOLOG_NO_PAGES_IN_MBYTE 5
#define ZTWOLOG_PAGE_SIZE 13
#define ZMAX_MM_BUFFER_SIZE 32     // Main memory window during log execution

#define ZMAX_PAGES_WRITTEN 8    // Max pages before writing to disk (=> config)
#define ZMIN_READ_BUFFER_SIZE 2       // Minimum number of pages to execute log
#define ZMIN_LOG_PAGES_OPERATION 10   // Minimum no of pages before stopping

#define ZPOS_CHECKSUM 0
#define ZPOS_LOG_LAP 1
#define ZPOS_MAX_GCI_COMPLETED 2
#define ZPOS_MAX_GCI_STARTED 3
#define ZNEXT_PAGE 4
#define ZPREV_PAGE 5
#define ZPOS_VERSION 6
#define ZPOS_NO_LOG_FILES 7
#define ZCURR_PAGE_INDEX 8
#define ZLAST_LOG_PREP_REF 10
#define ZPOS_DIRTY 11
/* A number of debug items written in the page header of all log files */
#define ZPOS_LOG_TIMER 12
#define ZPOS_PAGE_I 13
#define ZPOS_PLACE_WRITTEN_FROM 14
#define ZPOS_PAGE_NO 15
#define ZPOS_PAGE_FILE_NO 16
#define ZPOS_WORD_WRITTEN 17
#define ZPOS_IN_WRITING 18
#define ZPOS_PREV_PAGE_NO 19
#define ZPOS_IN_FREE_LIST 20

/* Specify number of log parts used to enable use of more LQH threads */
#define ZPOS_NO_LOG_PARTS 21

/* ------------------------------------------------------------------------- */
/*       CONSTANTS FOR THE VARIOUS REPLICA AND NODE TYPES.                   */
/* ------------------------------------------------------------------------- */
#define ZPRIMARY_NODE 0
#define ZBACKUP_NODE 1
#define ZSTANDBY_NODE 2
#define ZTC_NODE 3
#define ZLOG_NODE 3
/* ------------------------------------------------------------------------- */
/*       VARIOUS CONSTANTS USED AS FLAGS TO THE FILE MANAGER.                */
/* ------------------------------------------------------------------------- */
#define ZVAR_NO_LOG_PAGE_WORD 1
#define ZLIST_OF_PAIRS 0
#define ZLIST_OF_PAIRS_SYNCH 16
#define ZARRAY_OF_PAGES 1
#define ZLIST_OF_MEM_PAGES 2
#define ZLIST_OF_MEM_PAGES_SYNCH 18
#define ZCLOSE_NO_DELETE 0
#define ZCLOSE_DELETE 1
#define ZPAGE_ZERO 0
/* ------------------------------------------------------------------------- */
/*       THE FOLLOWING CONSTANTS ARE USED TO DESCRIBE THE TYPES OF           */
/*       LOG RECORDS, THE SIZE OF THE VARIOUS LOG RECORD TYPES AND           */
/*       THE POSITIONS WITHIN THOSE LOG RECORDS.                             */
/* ------------------------------------------------------------------------- */
/* ------------------------------------------------------------------------- */
/*       THESE CONSTANTS DESCRIBE THE SIZES OF VARIOUS TYPES OF LOG REORDS.  */
/*       NEXT_LOG_SIZE IS ACTUALLY ONE. THE REASON WE SET IT TO 2 IS TO      */
/*       SIMPLIFY THE CODE SINCE OTHERWISE HAVE TO USE A SPECIAL VERSION     */
/*       OF READ_LOGWORD WHEN READING LOG RECORD TYPE                        */
/*       SINCE NEXT MBYTE TYPE COULD BE THE VERY LAST WORD IN THE MBYTE.     */
/*       BY SETTING IT TO 2 WE ENSURE IT IS NEVER THE VERY LAST WORD         */
/*       IN THE MBYTE.                                                       */
/* ------------------------------------------------------------------------- */
#define ZFD_HEADER_SIZE 3
#define ZFD_MBYTE_SIZE 3
#define ZLOG_HEAD_SIZE 8
#define ZNEXT_LOG_SIZE 2
#define ZABORT_LOG_SIZE 3
#define ZCOMMIT_LOG_SIZE 9
#define ZCOMPLETED_GCI_LOG_SIZE 2
/* ------------------------------------------------------------------------- */
/*       THESE CONSTANTS DESCRIBE THE TYPE OF A LOG RECORD.                  */
/*       THIS IS THE FIRST WORD OF A LOG RECORD.                             */
/* ------------------------------------------------------------------------- */
#define ZNEW_PREP_OP_TYPE 0
#define ZPREP_OP_TYPE 1
#define ZCOMMIT_TYPE 2
#define ZABORT_TYPE 3
#define ZFD_TYPE 4
#define ZFRAG_SPLIT_TYPE 5
#define ZNEXT_LOG_RECORD_TYPE 6
#define ZNEXT_MBYTE_TYPE 7
#define ZCOMPLETED_GCI_TYPE 8
#define ZINVALID_COMMIT_TYPE 9
/* ------------------------------------------------------------------------- */
/*       THE POSITIONS OF LOGGED DATA IN A FILE DESCRIPTOR LOG RECORD HEADER.*/
/*       ALSO THE MAXIMUM NUMBER OF FILE DESCRIPTORS IN A LOG RECORD.        */
/* ------------------------------------------------------------------------- */
#define ZPOS_LOG_TYPE 0
#define ZPOS_NO_FD 1
#define ZPOS_FILE_NO 2
/* ------------------------------------------------------------------------- */
/*       THE POSITIONS WITHIN A PREPARE LOG RECORD AND A NEW PREPARE         */
/*       LOG RECORD.                                                         */
/* ------------------------------------------------------------------------- */
#define ZPOS_HASH_VALUE 2
#define ZPOS_SCHEMA_VERSION 3
#define ZPOS_TRANS_TICKET 4
#define ZPOS_OP_TYPE 5
#define ZPOS_NO_ATTRINFO 6
#define ZPOS_NO_KEYINFO 7
/* ------------------------------------------------------------------------- */
/*       THE POSITIONS WITHIN A COMMIT LOG RECORD.                           */
/* ------------------------------------------------------------------------- */
#define ZPOS_COMMIT_TRANSID1 1
#define ZPOS_COMMIT_TRANSID2 2
#define ZPOS_COMMIT_GCI 3
#define ZPOS_COMMIT_TABLE_REF 4
#define ZPOS_COMMIT_FRAGID 5
#define ZPOS_COMMIT_FILE_NO 6
#define ZPOS_COMMIT_START_PAGE_NO 7
#define ZPOS_COMMIT_START_PAGE_INDEX 8
#define ZPOS_COMMIT_STOP_PAGE_NO 9
/* ------------------------------------------------------------------------- */
/*       THE POSITIONS WITHIN A ABORT LOG RECORD.                            */
/* ------------------------------------------------------------------------- */
#define ZPOS_ABORT_TRANSID1 1
#define ZPOS_ABORT_TRANSID2 2
/* ------------------------------------------------------------------------- */
/*       THE POSITION WITHIN A COMPLETED GCI LOG RECORD.                     */
/* ------------------------------------------------------------------------- */
#define ZPOS_COMPLETED_GCI 1
/* ------------------------------------------------------------------------- */
/*       THE POSITIONS WITHIN A NEW PREPARE LOG RECORD.                      */
/* ------------------------------------------------------------------------- */
#define ZPOS_NEW_PREP_FILE_NO 8
#define ZPOS_NEW_PREP_PAGE_REF 9

#define ZLAST_WRITE_IN_FILE 1
#define ZENFORCE_WRITE 2
/* ------------------------------------------------------------------------- */
/*       CONSTANTS USED AS INPUT TO SUBROUTINE WRITE_LOG_PAGES AMONG OTHERS. */
/* ------------------------------------------------------------------------- */
#define ZNORMAL 0
#define ZINIT 1
/* ------------------------------------------------------------------------- */
/*       CONSTANTS USED BY CONTINUEB TO DEDUCE WHICH CONTINUE SIGNAL IS TO   */
/*       BE EXECUTED AS A RESULT OF THIS CONTINUEB SIGNAL.                   */
/* ------------------------------------------------------------------------- */
#define ZLOG_LQHKEYREQ 0
#define ZPACK_LQHKEYREQ 1
#define ZSEND_ATTRINFO 2
#define ZSR_GCI_LIMITS 3
#define ZSR_LOG_LIMITS 4
#define ZSEND_EXEC_CONF 5
#define ZEXEC_SR 6
#define ZSR_FOURTH_COMP 7
#define ZINIT_FOURTH 8
#define ZTIME_SUPERVISION 9
#define ZSR_PHASE3_START 10
#define ZLQH_TRANS_NEXT 11
#define ZLQH_RELEASE_AT_NODE_FAILURE 12
#define ZSCAN_TC_CONNECT 13
#define ZINITIALISE_RECORDS 14
#define ZINIT_GCP_REC 15
#define ZCHECK_LCP_STOP_BLOCKED 17
#define ZSCAN_MARKERS 18
#define ZOPERATION_EVENT_REP 19
#define ZDROP_TABLE_WAIT_USAGE 20
#define ZENABLE_EXPAND_CHECK 21
#define ZRETRY_TCKEYREF 22
#define ZWAIT_REORG_SUMA_FILTER_ENABLED 23
#define ZREBUILD_ORDERED_INDEXES 24
#define ZWAIT_READONLY 25
#define ZLCP_FRAG_WATCHDOG 26
#if defined ERROR_INSERT
#define ZDELAY_FS_OPEN 27
#endif
#define ZSTART_LOCAL_LCP 28
#define ZCHECK_SYSTEM_SCANS 29

/* ------------------------------------------------------------------------- */
/*        NODE STATE DURING SYSTEM RESTART, VARIABLES CNODES_SR_STATE        */
/*        AND CNODES_EXEC_SR_STATE.                                          */
/* ------------------------------------------------------------------------- */
#define ZSTART_SR 1
#define ZEXEC_SR_COMPLETED 2
/* ------------------------------------------------------------------------- */
/*       CONSTANTS USED BY NODE STATUS TO DEDUCE THE STATUS OF A NODE.       */
/* ------------------------------------------------------------------------- */
#define ZNODE_UP 0
#define ZNODE_DOWN 1
/* ------------------------------------------------------------------------- */
/*       START PHASES                                                        */
/* ------------------------------------------------------------------------- */
#define ZLAST_START_PHASE 255
#define ZSTART_PHASE1 1
#define ZSTART_PHASE2 2
#define ZSTART_PHASE3 3
#define ZSTART_PHASE4 4
#define ZSTART_PHASE6 6
/* ------------------------------------------------------------------------- */
/*       CONSTANTS USED BY SCAN AND COPY FRAGMENT PROCEDURES                 */
/* ------------------------------------------------------------------------- */
#define ZSTORED_PROC_SCAN 0
#define ZSTORED_PROC_COPY 2
#define ZDELETE_STORED_PROC_ID 3
#define ZWRITE_LOCK 1
#define ZSCAN_FRAG_CLOSED 2
#define ZNUM_RESERVED_TC_CONNECT_RECORDS 3
#define ZNUM_RESERVED_UTIL_CONNECT_RECORDS 100
/* ------------------------------------------------------------------------- */
/*       ERROR CODES ADDED IN VERSION 0.1 AND 0.2                            */
/* ------------------------------------------------------------------------- */
#define ZNOT_FOUND 1             // Not an error code, a return value
#define ZNO_FREE_LQH_CONNECTION 414
#define ZGET_DATAREC_ERROR 418
#define ZGET_ATTRINBUF_ERROR 419
#define ZNO_FREE_FRAGMENTREC 460 // Insert new fragment error code
#define ZTAB_FILE_SIZE 464       // Insert new fragment error code + Start kernel
#define ZNO_ADD_FRAGREC 465      // Insert new fragment error code
/* ------------------------------------------------------------------------- */
/*       ERROR CODES ADDED IN VERSION 0.3                                    */
/* ------------------------------------------------------------------------- */
#define ZTAIL_PROBLEM_IN_LOG_ERROR 410
#define ZGCI_TOO_LOW_ERROR 429        // GCP_SAVEREF error code
#define ZTAB_STATE_ERROR 474          // Insert new fragment error code
#define ZTOO_NEW_GCI_ERROR 479        // LCP Start error
/* ------------------------------------------------------------------------- */
/*       ERROR CODES ADDED IN VERSION 0.4                                    */
/* ------------------------------------------------------------------------- */

#define ZNO_FREE_FRAG_SCAN_REC_ERROR 490 // SCAN_FRAGREF error code
#define ZCOPY_NO_FRAGMENT_ERROR 491      // COPY_FRAGREF error code
#define ZTAKE_OVER_ERROR 499
#define ZTO_OP_STATE_ERROR 631           // Same as in Dbacc.hpp
#define ZCOPY_NODE_ERROR 1204
#define ZTOO_MANY_COPY_ACTIVE_ERROR 1208 // COPY_FRAG and COPY_ACTIVEREF code
#define ZCOPY_ACTIVE_ERROR 1210          // COPY_ACTIVEREF error code
#define ZNO_TC_CONNECT_ERROR 1217        // Simple Read + SCAN
#define ZTRANSPORTER_OVERLOADED_ERROR 1218
/* ------------------------------------------------------------------------- */
/*       ERROR CODES ADDED IN VERSION 1.X                                    */
/* ------------------------------------------------------------------------- */
//#define ZSCAN_BOOK_ACC_OP_ERROR 1219   // SCAN_FRAGREF error code
#define ZFILE_CHANGE_PROBLEM_IN_LOG_ERROR 1220
#define ZTEMPORARY_REDO_LOG_FAILURE 1221
#define ZNO_FREE_MARKER_RECORDS_ERROR 1222
#define ZNODE_SHUTDOWN_IN_PROGRESS 1223
#define ZTOO_MANY_FRAGMENTS 1224
#define ZTABLE_NOT_DEFINED 1225
#define ZDROP_TABLE_IN_PROGRESS 1226
#define ZINVALID_SCHEMA_VERSION 1227
#define ZTABLE_READ_ONLY 1233
#define ZREDO_IO_PROBLEM 1234

/* ------------------------------------------------------------------------- */
/*       ERROR CODES ADDED IN VERSION 2.X                                    */
/* ------------------------------------------------------------------------- */
#define ZNODE_FAILURE_ERROR 400
#define ZBAD_UNLOCK_STATE 416
#define ZBAD_OP_REF 417
/* ------------------------------------------------------------------------- */
/*       ERROR CODES FROM ACC                                                */
/* ------------------------------------------------------------------------- */
#define ZNO_TUPLE_FOUND 626
#define ZTUPLE_ALREADY_EXIST 630
/* ------------------------------------------------------------------------- */
/*       ERROR CODES FROM TUP                                                */
/* ------------------------------------------------------------------------- */
/** 
 * 899 would be returned by an interpreted program such as a scan filter. New
 * such programs should use 626 instead, but 899 will also be supported to 
 * remain backwards compatible. 899 is problematic since it is also used as
 * "Rowid already allocated" (cf. ndberror.c).
 */
#define ZUSER_SEARCH_CONDITION_FALSE_CODE 899
#endif

/** 
 * @class dblqh
 *
 * @section secIntro Introduction
 *
 * Dblqh is the coordinator of the LDM.  Dblqh is responsible for 
 * performing operations on tuples.  It does this job with help of 
 * Dbacc block (that manages the index structures) and Dbtup
 * (that manages the tuples).
 *
 * Dblqh also keeps track of the participants and acts as a coordinator of
 * 2-phase commits.  Logical redo logging is also handled by the Dblqh
 * block.
 *
 * @section secModules Modules
 *
 * The code is partitioned into the following modules:
 * - START / RESTART 
 *   - Start phase 1: Load our block reference and our processor id
 *   - Start phase 2: Initiate all records within the block
 *                    Connect LQH with ACC and TUP.
 *   - Start phase 4: Connect LQH with LQH.  Connect every LQH with 
 *                    every LQH in the database system.           
 *	              If initial start, then create the fragment log files.
 *	              If system restart or node restart, 
 *                    then open the fragment log files and   
 *	              find the end of the log files.
 * - ADD / DELETE FRAGMENT<br>
 *     Used by dictionary to create new fragments and delete old fragments.
 *  - EXECUTION<br>
 *    handles the reception of lqhkeyreq and all processing        
 *    of operations on behalf of this request. 
 *    This does also involve reception of various types of attrinfo 
 *    and keyinfo. 
 *    It also involves communication with ACC and TUP.
 *  - LOG<br>
 *    The log module handles the reading and writing of the log.
 *    It is also responsible for handling system restart. 
 *    It controls the system restart in TUP and ACC as well.
 *  - TRANSACTION<br>
 *    This module handles the commit and the complete phases.
 *  - MODULE TO HANDLE TC FAILURE<br>
 *  - SCAN<br>
 *    This module contains the code that handles a scan of a particular 
 *    fragment.
 *    It operates under the control of TC and orders ACC to 
 *    perform a scan of all tuples in the fragment.
 *    TUP performs the necessary search conditions
 *    to ensure that only valid tuples are returned to the application.
 *  - NODE RECOVERY<br>
 *    Used when a node has failed. 
 *    It performs a copy of a fragment to a new replica of the fragment. 
 *    It does also shut down all connections to the failed node.
 *  - LOCAL CHECKPOINT<br>
 *    Handles execution and control of LCPs
 *    It controls the LCPs in TUP and ACC. 
 *    It also interacts with DIH to control which GCPs are recoverable.
 *  - GLOBAL CHECKPOINT<br>
 *    Helps DIH in discovering when GCPs are recoverable. 
 *    It handles the request gcp_savereq that requests LQH to 
 *    save a particular GCP to disk and respond when completed.  
 *  - FILE HANDLING<br>
 *    With submodules: 
 *    - SIGNAL RECEPTION
 *    - NORMAL OPERATION
 *    - FILE CHANGE
 *    - INITIAL START
 *    - SYSTEM RESTART PHASE ONE
 *    - SYSTEM RESTART PHASE TWO,
 *    - SYSTEM RESTART PHASE THREE
 *    - SYSTEM RESTART PHASE FOUR
 *  - ERROR 
 *  - TEST 
 *  - LOG 
 */
class Dblqh 
#ifndef DBLQH_STATE_EXTRACT
  : public SimulatedBlock
#endif
{
  friend class DblqhProxy;
  friend class Backup;

public:
#ifndef DBLQH_STATE_EXTRACT
  enum LcpCloseState {
    LCP_IDLE = 0,
    LCP_RUNNING = 1,       // LCP is running
    LCP_CLOSE_STARTED = 2  // Completion(closing of files) has started
  };

  enum ExecUndoLogState {
    EULS_IDLE = 0,
    EULS_STARTED = 1,
    EULS_COMPLETED = 2
  };

  struct AddFragRecord {
    enum AddFragStatus {
      FREE = 0,
      ACC_ADDFRAG = 1,
      WAIT_TUP = 3,
      WAIT_TUX = 5,
      WAIT_ADD_ATTR = 6,
      TUP_ATTR_WAIT = 7,
      TUX_ATTR_WAIT = 9
    };
    AddFragStatus addfragStatus;
    UintR fragmentPtr;
    UintR nextAddfragrec;
    UintR accConnectptr;
    UintR tupConnectptr;
    UintR tuxConnectptr;

    CreateTabReq m_createTabReq;
    LqhFragReq m_lqhFragReq;
    LqhAddAttrReq m_addAttrReq;
    DropFragReq m_dropFragReq;
    DropTabReq m_dropTabReq;

    Uint16 addfragErrorCode;
    Uint16 attrSentToTup;
    Uint16 attrReceived;
    Uint16 totalAttrReceived;
    Uint16 fragCopyCreation;
    Uint16 defValNextPos;
    Uint32 defValSectionI;
  };
  typedef Ptr<AddFragRecord> AddFragRecordPtr;
  
  struct ScanRecord {
    ScanRecord() {}
    enum ScanState {
      SCAN_FREE = 0,
      WAIT_NEXT_SCAN_COPY = 1,
      WAIT_NEXT_SCAN = 2,
      WAIT_ACC_COPY = 3,
      WAIT_ACC_SCAN = 4,
      WAIT_SCAN_NEXTREQ = 5,
      WAIT_CLOSE_SCAN = 6,
      WAIT_CLOSE_COPY = 7,
      WAIT_TUPKEY_COPY = 8,
      WAIT_LQHKEY_COPY = 9,
      IN_QUEUE = 10,
      COPY_FRAG_HALTED = 11
    };
    enum ScanType {
      ST_IDLE = 0,
      SCAN = 1,
      COPY = 2
    };

    /* A single scan of each fragment can have MAX_PARALLEL_OP_PER_SCAN
     * read operations in progress at one time
     * We must store ACC ptrs for each read operation.  They are stored
     * in SegmentedSections linked in the array below.
     * The main oddity is that the first element of scan_acc_op_ptr is
     * an ACC ptr, but all others are refs to SectionSegments containing
     * ACC ptrs.
     */
    STATIC_CONST( MaxScanAccSegments= (
                 (MAX_PARALLEL_OP_PER_SCAN + SectionSegment::DataLength - 1) /
                 SectionSegment::DataLength) + 1);

    UintR scan_acc_op_ptr[ MaxScanAccSegments ];
    Uint32 scan_acc_index;
    Uint32 scan_acc_segments;
    UintR scanApiOpPtr;
    Local_key m_row_id;
    
    Uint32 m_max_batch_size_rows;
    Uint32 m_max_batch_size_bytes;

    Uint32 m_curr_batch_size_rows;
    Uint32 m_curr_batch_size_bytes;

    Uint32 m_exec_direct_batch_size_words;

    bool check_scan_batch_completed() const;
    
    UintR copyPtr;
    union {
      Uint32 nextPool;
      Uint32 nextList;
    };
    Uint32 prevList;
    Uint32 nextHash;
    Uint32 prevHash;
    bool equal(const ScanRecord & key) const {
      return scanNumber == key.scanNumber && fragPtrI == key.fragPtrI;
    }
    Uint32 hashValue() const {
      return fragPtrI ^ scanNumber;
    }
    
    UintR scanAccPtr;
    UintR scanAiLength;
    UintR scanErrorCounter;
    UintR scanSchemaVersion;
    Uint32 scanTcWaiting; // When the request came from TC, 0 is no request

    /**
     * This is _always_ main table, even in range scan
     *   in which case scanTcrec->fragmentptr is different
     */
    Uint32 scan_check_lcp_stop;
    Uint32 fragPtrI;
    UintR scanStoredProcId;
    ScanState scanState;
    UintR scanTcrec;
    ScanType scanType;
    BlockReference scanApiBlockref;
    NodeId scanNodeId;
    Uint16 scanReleaseCounter;
    Uint16 scanNumber;
    Uint16 scan_lastSeen;

    // scan source block, block object and function ACC TUX TUP
    BlockReference scanBlockref;
    SimulatedBlock* scanBlock;
    ExecFunction scanFunction_NEXT_SCANREQ;
 
    Uint8 scanCompletedStatus;
    Uint8 scanFlag;
    Uint8 scanLockHold;
    Uint8 scanLockMode;

    Uint8 readCommitted;
    Uint8 rangeScan;
    Uint8 descending;
    Uint8 tupScan;

    Uint8 lcpScan;
    Uint8 scanKeyinfoFlag;
    Uint8 m_last_row;
    Uint8 m_reserved;

    Uint8 statScan;
    Uint8 m_stop_batch;
    Uint8 scan_direct_count;
    Uint8 prioAFlag;
  };
  typedef Ptr<ScanRecord> ScanRecordPtr;
  typedef ArrayPool<ScanRecord> ScanRecord_pool;
  typedef DLCList<ScanRecord_pool> ScanRecord_list;
  typedef LocalDLCList<ScanRecord_pool> Local_ScanRecord_list;
  typedef DLCFifoList<ScanRecord_pool> ScanRecord_fifo;
  typedef LocalDLCFifoList<ScanRecord_pool> Local_ScanRecord_fifo;
  typedef DLHashTable<ScanRecord_pool> ScanRecord_hash;

/**
 * Constants for scan_direct_count
 * Mainly used to avoid overextending the stack and to some
 * extent keeping the scheduling rules.
 */
#define ZMAX_SCAN_DIRECT_COUNT 6

  struct Fragrecord {
    Fragrecord() {}

    enum ExecSrStatus {
      IDLE = 0,
      ACTIVE = 2
    };
    /**
     * Possible state transitions are:
     * - FREE -> DEFINED                 Fragment record is allocated
     * - DEFINED -> ACTIVE               Add fragment is completed and 
     *                                   fragment is ready to      
     *                                   receive operations.
     * - DEFINED -> ACTIVE_CREATION      Add fragment is completed and 
     *                                   fragment is ready to      
     *                                   receive operations in parallel 
     *                                   with a copy fragment     
     *                                   which is performed from the 
     *                                   primary replica             
     * - DEFINED -> CRASH_RECOVERING     A fragment is ready to be 
     *                                   recovered from a local        
     *                                   checkpoint on disk
     * - ACTIVE -> REMOVING              A fragment is removed from the node
     * - CRASH_RECOVERING -> ACTIVE      A fragment has been recovered and 
     *                                   are now ready for     
     *                                   operations again.
     * - CRASH_RECOVERING -> REMOVING    Fragment recovery failed or 
     *                                   was cancelled.              
     * - ACTIVE_CREATION -> ACTIVE       A fragment is now copied and now 
     *                                   is a normal fragment   
     * - ACTIVE_CREATION -> REMOVING     Copying of the fragment failed
     * - REMOVING -> FREE                Removing of the fragment is 
     *                                   completed and the fragment  
     *                                   is now free again.
     */
    enum FragStatus {
      FREE = 0,               ///< Fragment record is currently not in use
      FSACTIVE = 1,           ///< Fragment is defined and usable for operations
      DEFINED = 2,            ///< Fragment is defined but not yet usable by 
                              ///< operations
      ACTIVE_CREATION = 3,    ///< Fragment is defined and active but is under 
                              ///< creation by the primary LQH.
      CRASH_RECOVERING = 4,   ///< Fragment is recovering after a crash by 
                              ///< executing the fragment log and so forth. 
                              ///< Will need further breakdown.
      REMOVING = 5            ///< The fragment is currently removed. 
                              ///< Operations are not allowed. 
    };
    enum LogFlag {
      STATE_TRUE = 0,
      STATE_FALSE = 1
    };
    enum SrStatus {
      SS_IDLE = 0,
      SS_STARTED = 1,
      SS_COMPLETED = 2
    };
    enum LcpFlag {
      LCP_STATE_TRUE = 0,
      LCP_STATE_FALSE = 1
    };
    /**
     *        Last GCI for executing the fragment log in this phase.
     */
    UintR execSrLastGci[4];
    /**
     *       Start GCI for executing the fragment log in this phase.
     */
    UintR execSrStartGci[4];
    /**
     *       Requesting user pointer for executing the fragment log in
     *       this phase
     */
    UintR execSrUserptr[4];
    /**
     *       The LCP identifier of the LCP's. 
     *       =0 means that the LCP number has not been stored.
     *       The LCP identifier is supplied by DIH when starting the LCP.   
     */
    UintR lcpId[MAX_LCP_STORED];
    UintR maxGciInLcp;
    /**
     *       This variable contains the maximum global checkpoint 
     *       identifier that exists in a certain local checkpoint. 
     *       Maximum 4 local checkpoints is possible in this release.
     */
    UintR maxGciCompletedInLcp;
    UintR srLastGci[4];
    UintR srStartGci[4];
    /**
     *       The fragment pointers in ACC
     */
    UintR accFragptr;
    /**
     *       The EXEC_SR variables are used to keep track of which fragments  
     *       that are interested in being executed as part of executing the    
     *       fragment loop. 
     *       It is initialised for every phase of executing the 
     *       fragment log (the fragment log can be executed upto four times).  
     *                                                                         
     *       Each execution is capable of executing the log records on four    
     *       fragment replicas.                                                
     */
    /**
     *       Requesting block reference for executing the fragment log
     *       in this phase.
     */
    BlockReference execSrBlockref[4];
    /**
     *       This variable contains references to active scan and copy     
     *       fragment operations on the fragment. 
     *       A maximum of four concurrently active is allowed.
     */

    typedef Bitmask<8> ScanNumberMask; // Max 255 KeyInfo20::ScanNo
    ScanNumberMask m_scanNumberMask;
    ScanRecord_list::Head m_activeScans;
    ScanRecord_fifo::Head m_queuedScans;
    ScanRecord_fifo::Head m_queuedTupScans;
    ScanRecord_fifo::Head m_queuedAccScans;

    Uint16 srLqhLognode[4];
    /**
     *       The fragment pointers in TUP and TUX
     */
    UintR tupFragptr;
    UintR tuxFragptr;

    /**
     *       This variable keeps track of how many operations that are 
     *       active that have skipped writing the log but not yet committed 
     *       or aborted.  This is used during start of fragment.
     */
    UintR activeTcCounter;

    /**
     *       This status specifies whether this fragment is actively 
     *       engaged in executing the fragment log.
     */
    ExecSrStatus execSrStatus;

    /**
     *       The fragment id of this fragment.
     */
    UintR fragId;

    /**
     *       Status of fragment
     */
    FragStatus fragStatus;

    /**
     * 0 = undefined i.e fragStatus != ACTIVE_CREATION
     * 1 = yes
     * 2 = no
     */
    enum ActiveCreat {
      AC_NORMAL = 0,  // fragStatus != ACTIVE_CREATION
      AC_IGNORED = 1, // Operation that got ignored during NR
      AC_NR_COPY = 2  // Operation that got performed during NR
    };
    Uint8 m_copy_started_state; 

    /**
     *       This flag indicates whether logging is currently activated at 
     *       the fragment.  
     *       During a system restart it is temporarily shut off. 
     *       Some fragments have it permanently shut off. 
     */
    LogFlag logFlag;
    UintR masterPtr;
    /**
     *       This variable contains the maximum global checkpoint identifier 
     *       which was completed when the local checkpoint was started.
     */
    /**
     *       Reference to the next fragment record in a free list of fragment 
     *       records.              
     */
    union {
      Uint32 nextPool;
      Uint32 nextList;
    };
    Uint32 prevList;
    
    /**
     *       The newest GCI that has been committed on fragment             
     */
    UintR newestGci;
    Uint32 m_completed_gci;
    SrStatus srStatus;
    UintR srUserptr;
    /**
     *       The global checkpoint when table was created for this fragment.
     */
    UintR startGci;
    /**
     *       A reference to the table owning this fragment.
     */
    UintR tabRef;

    /**
     *       The block reference to ACC on the fragment makes it
     *       possible to have different ACC blocks for different
     *       fragments in the future.
     */
    BlockReference accBlockref;

    /**
     *       Ordered index block.
     */
    BlockReference tuxBlockref;
    /**
     *       The master block reference as sent in COPY_ACTIVEREQ.
     */
    BlockReference masterBlockref;
    /**
     *       These variables are used during system restart to recall
     *       from which node to execute the fragment log and which GCI's
     *       this node should start and stop from. Also to remember who
     *       to send the response to when system restart is completed.
     */
    BlockReference srBlockref;
    /**
     *       The block reference to TUP on the fragment makes it
     *       possible to have different TUP blocks for different
     *       fragments in the future.
     */
    BlockReference tupBlockref;
    /**
     *      This state indicates if the fragment will participate in a
     *      checkpoint.  
     *      Temporary tables with Fragrecord::logFlag permanently off
     *      will also have Fragrecord::lcpFlag off.
     */
    LcpFlag lcpFlag;
    /**
     *       Used to ensure that updates started with old
     *       configuration do not arrive here after the copy fragment
     *       has started. 
     *       If they are allowed to arrive after they
     *       could update a record that has already been replicated to
     *       the new node.  This type of arrival should be extremely
     *       rare but we must anyway ensure that no harm is done.
     */
    Uint16 copyNode;
    /**
     * Instance key for fast access.
     */
    Uint16 lqhInstanceKey;
    /**
     *       The number of fragment replicas that will execute the log
     *       records in this round of executing the fragment
     *       log.  Maximum four is possible.
     */
    Uint8 execSrNoReplicas;
    /**
     *       This variable contains what type of replica this fragment
     *       is.  Two types are possible:  
     *       - Primary/Backup replica = 0
     *       - Stand-by replica = 1 
     *
     *       It is not possible to distinguish between primary and
     *       backup on a fragment.  
     *       This can only be done per transaction. 
     *       DIH can change from primary to backup without informing
     *       the various replicas about this change.
     */
    Uint8 fragCopy;
    /**
     *       This is the last fragment distribution key that we have
     *       heard of.
     */
    Uint8 fragDistributionKey;
   /**
     *       How many local checkpoints does the fragment contain
     */
    Uint16 srChkpnr;
    Uint8  srNoLognodes;
    /**
     *       Table type.
     */
    Uint8 tableType;
    /**
     *       For ordered index fragment, i-value of corresponding
     *       fragment in primary table.
     */
    UintR tableFragptr;
    /**
     *       The GCI when the table was created
     */
    Uint32 createGci;

    /**
     * Log part
     */
    Uint32 m_log_part_ptr_i;
    /**
     * LCP_FRAG_ORD info for the c_queued_lcp_frag_ord queue.
     */
    enum LcpExecutionState
    {
      LCP_QUEUED = 0,
      LCP_EXECUTING = 1,
      LCP_EXECUTED = 2
    };

    /* 
       Usage counters. Except for m_queuedScanCount, these only count 'user' 
       operations, i.e. those directly initiated from the ndbapi, and not
       'internal' operations, such as those used for LCPs.
     */
    struct UsageStat
    {
      // Number of key read operations.
      Uint64 m_readKeyReqCount;

      // Number of inserts.
      Uint64 m_insKeyReqCount;

      // Number of updates.
      Uint64 m_updKeyReqCount;
      /*
        Number of write operations, meaning 'update' if key exists, and 'insert'
        otherwise.
      */
      Uint64 m_writeKeyReqCount;

      // Number of deletes
      Uint64 m_delKeyReqCount;
 
      /*
        Number of key operations refused by the LDM due to either:
        - no matching key for update/delete.
        - key exists already for insert.
        - operation rejected by interpreted program.
      */
      Uint64 m_keyRefCount;

      // Number of attrinfo words in key operations.
      Uint64 m_keyReqAttrWords;

      // Number of keyinfo words in key operations.
      Uint64 m_keyReqKeyWords;

      // Total size of interpeter programs for key operations.
      Uint64 m_keyProgramWords;

      // Number of interpreter instructions executed for key operations.
      Uint64 m_keyInstructionCount;

      // Number of words returned to client due to key operations.
      Uint64 m_keyReqWordsReturned;

      // Number of fragment scans requested.
      Uint64 m_scanFragReqCount;

      /*
        The number of rows examined during scans. Some of these may have been
        rejected by the interpreted program (i.e. a pushed condition), and 
        thus not been returned to the client.
      */
      Uint64 m_scanRowsExamined;

      // Number of scan rows returned to the client.
      Uint64 m_scanRowsReturned;

      // Number of words returned to client due to scans.
      Uint64 m_scanWordsReturned;

      // Total size of interpeter programs for scans.
      Uint64 m_scanProgramWords;

      // Total size of scan bounds (for ordered index scans).
      Uint64 m_scanBoundWords;

      // Number of interpreter instructions executed for scans.
      Uint64 m_scanInstructionCount;

      // Total number of scans queued (including those from internal clients.
      Uint64 m_queuedScanCount;
      
      // Set all counters to zero.
      void init()
      {
        memset(this, 0, sizeof *this);
      }
    };
    Uint32 lcp_frag_ord_lcp_no;
    Uint32 lcp_frag_ord_lcp_id;
    LcpExecutionState lcp_frag_ord_state;
    UsageStat m_useStat;
    Uint8 m_copy_complete_flag;
    /**
     * To keep track of which fragment have started the
     * current local LCP we have a value of 0 or 1. If
     * current local LCP is 0 the fragment will have 0
     * to indicate it has been started and 1 indicating
     * that it hasn't started yet.
     * The value is initialised to 0 and the value of the
     * first local LCP is 1.
     */
    Uint8 m_local_lcp_instance_started;
  };
  typedef Ptr<Fragrecord> FragrecordPtr;
  typedef ArrayPool<Fragrecord> Fragrecord_pool;
  typedef SLList<Fragrecord_pool> Fragrecord_list;
  typedef DLFifoList<Fragrecord_pool> Fragrecord_fifo;
  
  /* $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ */
  /* $$$$$$$                GLOBAL CHECKPOINT RECORD                  $$$$$$ */
  /* $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ */
  /**
   *       This record describes a global checkpoint that is
   *       completed.  It waits for all log records belonging to this
   *       global checkpoint to be saved on disk.
   */
  struct GcpRecord {
    /**
     *       The file number within each log part where the log was
     *       located when gcp_savereq was received. The last record 
     *       belonging to this global checkpoint is certainly before 
     *       this place in the log. We could come even closer but it 
     *       would cost performance and doesn't seem like a good 
     *       idea. This is simple and it works.
     */
    Uint16 gcpFilePtr[NDB_MAX_LOG_PARTS];
    /** 
     *       The page number within the file for each log part.
     */
    Uint16 gcpPageNo[NDB_MAX_LOG_PARTS];
    /**
     *       The word number within the last page that was written for
     *       each log part.
     */
    Uint16 gcpWordNo[NDB_MAX_LOG_PARTS];
    /**
     *       The identity of this global checkpoint.
     */
    UintR gcpId;
    /**
     *       The state of this global checkpoint, one for each log part.
     */
    Uint8 gcpLogPartState[NDB_MAX_LOG_PARTS];
    /**
     *       The sync state of this global checkpoint, one for each
     *       log part.
     */
    Uint8 gcpSyncReady[NDB_MAX_LOG_PARTS];
    /**
     *       User pointer of the sender of gcp_savereq (= master DIH).
     */
    UintR gcpUserptr;
    /**
     *       Block reference of the sender of gcp_savereq 
     *       (= master DIH).
     */
    BlockReference gcpBlockref;
  }; // Size 44 bytes
  typedef Ptr<GcpRecord> GcpRecordPtr;

  struct HostRecord {
    struct PackedWordsContainer lqh_pack[MAX_NDBMT_LQH_THREADS+1];
    struct PackedWordsContainer tc_pack[MAX_NDBMT_TC_THREADS+1];
    Uint8 inPackedList;
    Uint8 nodestatus;
  };
  typedef Ptr<HostRecord> HostRecordPtr;
  
  /* $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ */
  /* $$$$$$               LOCAL CHECKPOINT SUPPORT RECORD            $$$$$$$ */
  /* $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ */
  /**
   *      This record contains the information about an outstanding
   *      request to TUP or ACC. Used for both local checkpoints and
   *      system restart.
   */
  struct LcpLocRecord {
    enum LcpLocstate {
      IDLE = 0,
      WAIT_TUP_PREPLCP = 1,
      WAIT_LCPHOLDOP = 2,
      HOLDOP_READY = 3,
      ACC_WAIT_STARTED = 4,
      ACC_STARTED = 5,
      ACC_COMPLETED = 6,
      TUP_WAIT_STARTED = 7,
      TUP_STARTED = 8,
      TUP_COMPLETED = 9,
      SR_ACC_STARTED = 10,
      SR_TUP_STARTED = 11,
      SR_ACC_COMPLETED = 12,
      SR_TUP_COMPLETED = 13
    };
    LcpLocstate lcpLocstate;
    Uint32 lcpRef;
  }; // 28 bytes
  typedef Ptr<LcpLocRecord> LcpLocRecordPtr;

  /* $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ */
  /* $$$$$$$              LOCAL CHECKPOINT RECORD                    $$$$$$$ */
  /* $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ */
  /** 
   *       This record contains the information about a local
   *       checkpoint that is ongoing. This record is also used as a
   *       system restart record.
   */
  struct LcpRecord {
    LcpRecord() { m_EMPTY_LCP_REQ.clear(); }
    
    enum LcpState {
      LCP_IDLE = 0,
      LCP_COMPLETED = 1,
      LCP_PREPARING = 2,
      LCP_PREPARED = 3,
      LCP_CHECKPOINTING = 4
    };
 
    LcpState lcpPrepareState;
    LcpState lcpRunState;
    bool firstFragmentFlag;
    bool lastFragmentFlag;

    struct FragOrd {
      Uint32 fragPtrI;
      LcpFragOrd lcpFragOrd;
    };
    FragOrd currentPrepareFragment;
    FragOrd currentRunFragment;
    
    bool   reportEmpty;
    NdbNodeBitmask m_EMPTY_LCP_REQ;

    Uint32 m_outstanding;

    Uint64 m_no_of_records;
    Uint64 m_no_of_bytes;
  };
  typedef Ptr<LcpRecord> LcpRecordPtr;

  struct IOTracker
  {
    STATIC_CONST( SAMPLE_TIME = 128 );              // millis
    STATIC_CONST( SLIDING_WINDOW_LEN = 1024 );      // millis
    STATIC_CONST( SLIDING_WINDOW_HISTORY_LEN = 8 );

    void init(Uint32 partNo);
    Uint32 m_log_part_no;
    Uint32 m_current_time;

    /**
     * Keep sliding window of measurement
     */
    Uint32 m_save_pos; // current pos in array
    Uint32 m_save_written_bytes[SLIDING_WINDOW_HISTORY_LEN];
    Uint32 m_save_elapsed_millis[SLIDING_WINDOW_HISTORY_LEN];

    /**
     * Current sum of sliding window
     */
    Uint32 m_curr_elapsed_millis;
    Uint64 m_curr_written_bytes;

    /**
     * Currently outstanding bytes
     */
    Uint64 m_sum_outstanding_bytes;

    /**
     * How many times did we pass lag-threshold
     */
    Uint32 m_lag_cnt;

    /**
     * How many seconds of writes are we lagging
     */
    Uint32 m_lag_in_seconds;

    /**
     * bytes send during current sample
     */
    Uint64 m_sample_sent_bytes;

    /**
     * bytes completed during current sample
     */
    Uint64 m_sample_completed_bytes;

    /**
     * bytes completed since last report
     */
    Uint64 m_redo_written_bytes;

    int tick(Uint32 now, Uint32 maxlag, Uint32 maxlag_cnt);
    void send_io(Uint32 bytes);
    void complete_io(Uint32 bytes);
    Uint32 get_lag_cnt()
    {
      return m_lag_cnt;
    }
    Uint32 get_lag_in_seconds()
    {
      return m_lag_in_seconds;
    }
    Uint64 get_and_reset_redo_written_bytes()
    {
      Uint64 redo_written_bytes = m_redo_written_bytes;
      m_redo_written_bytes = 0;
      return redo_written_bytes;
    }
  };
  bool c_is_io_lag_reported;
  bool is_ldm_instance_io_lagging();
  Uint64 report_redo_written_bytes();

  /** 
   * RedoWorkStats
   *
   * Structure for tracking the work performed to recover
   * from redo
   */
  class RedoWorkStats
  {
  public:
    Uint64 m_pagesRead;
    
    Uint64 m_opsPrepared;
    Uint64 m_opsSkipped;
    Uint64 m_opsExecuted;
    Uint64 m_bytesExecuted;
    Uint32 m_gcisExecuted;

    RedoWorkStats()
      :m_pagesRead(0),
       m_opsSkipped(0),
       m_opsExecuted(0),
       m_bytesExecuted(0),
       m_gcisExecuted(0)
      {};
  };

  /**
   * LCPFragWatchdog
   *
   * Structure tracking state of LCP fragment watchdog.
   * This watchdog polls the state of the current LCP fragment
   * scan to ensure that forward progress is maintained at
   * a minimal rate.
   * It only continues running while this LQH instance 
   * thinks a fragment scan is ongoing
   */
  struct LCPFragWatchdog
  {
    STATIC_CONST( PollingPeriodMillis = 1000 ); /* 10s */
    Uint32 WarnElapsedWithNoProgressMillis; /* LCP Warn, milliseconds */
    Uint32 MaxElapsedWithNoProgressMillis;  /* LCP Fail, milliseconds */

    SimulatedBlock* block;
    
    /* Should the watchdog be running? */
    bool scan_running;
    
    /* Is there an active thread? */
    bool thread_active;
    
    /* LCP position and state info from Backup block */
    LcpStatusConf::LcpState lcpState;
    Uint32 tableId;
    Uint32 fragId;
    Uint64 completionStatus;
    Uint32 lcpScannedPages;

    /* Total elapsed milliseconds with no LCP progress observed */ 
    Uint32 elapsedNoProgressMillis; /* milliseconds */
    NDB_TICKS lastChecked;          /* Last time LCP progress checked */

    /* Reinitialise the watchdog */
    void reset();

    /* Handle an LCP Status report */
    void handleLcpStatusRep(LcpStatusConf::LcpState repLcpState,
                            Uint32 repTableId,
                            Uint32 repFragId,
                            Uint64 repCompletionStatus,
                            Uint32 repLcpScannedPages);
  };
  
  LCPFragWatchdog c_lcpFragWatchdog;
    
    
  /* $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ */
  /* $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ */
  /*                                                                          */
  /*       THE RECORDS THAT START BY LOG_ ARE A PART OF THE LOG MANAGER.      */
  /*       THESE RECORDS ARE USED TO HANDLE THE FRAGMENT LOG.                 */
  /*                                                                          */
  /* $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ */
  /* $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ */
  /* $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ */
  /* $$$$$$$                       LOG RECORD                         $$$$$$$ */
  /*                                                                          */
  /* $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ */
  /*       THIS RECORD IS ALIGNED TO BE 256 BYTES.                            */
  /* $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ */
  /**       
   *       This record describes the current state of a log. 
   *       A log consists of a number of log files. 
   *       These log files are described by the log file record.
   *
   *       There will be 4 sets of log files. 
   *       Different tables will use different log files dependent 
   *       on the table id. 
   *       This  ensures that more than one outstanding request can 
   *       be sent to the file system.
   *       The log file to use is found by performing a very simple hash
   *       function.
   */
  struct LogPartRecord {
    enum LogPartState {
      IDLE = 0,                       ///< Nothing happens at the moment
      ACTIVE = 1,                     ///< An operation is active logging 
      SR_FIRST_PHASE = 2,             ///< Finding the end of the log and 
                                      ///< the information about global 
                                      ///< checkpoints in the log is ongoing.
      SR_FIRST_PHASE_COMPLETED = 3,   ///< First phase completed
      SR_THIRD_PHASE_STARTED = 4,     ///< Executing fragment log is in 3rd ph
      SR_THIRD_PHASE_COMPLETED = 5,
      SR_FOURTH_PHASE_STARTED = 6,    ///< Finding the log tail and head 
                                      ///< is the fourth phase.
      SR_FOURTH_PHASE_COMPLETED = 7
    };
    enum WaitWriteGciLog {
      WWGL_TRUE = 0,
      WWGL_FALSE = 1
    };
    enum LogExecState {
      LES_IDLE = 0,
      LES_SEARCH_STOP = 1,
      LES_SEARCH_START = 2,
      LES_EXEC_LOG = 3,
      LES_EXEC_LOG_NEW_MBYTE = 4,
      LES_EXEC_LOG_NEW_FILE = 5,
      LES_EXEC_LOGREC_FROM_FILE = 6,
      LES_EXEC_LOG_COMPLETED = 7,
      LES_WAIT_READ_EXEC_SR_NEW_MBYTE = 8,
      LES_WAIT_READ_EXEC_SR = 9,
      LES_EXEC_LOG_INVALIDATE = 10
    };

    /**
     *       Is a CONTINUEB(ZLOG_LQHKEYREQ) signal sent and
     *       outstanding. We do not want several instances of this
     *       signal out in the air since that would create multiple
     *       writers of the list.
     */
    UintR LogLqhKeyReqSent;
    /**
     *       Contains the current log file where log records are
     *       written.  During system restart it is used to indicate the
     *       last log file.
     */
    UintR currentLogfile;
    /**
     *       The log file used to execute log records from far behind.
     */
    UintR execSrExecLogFile;
    /**
     *       The currently executing prepare record starts in this log
     *       page. This variable is used to enable that a log record is
     *       executed multiple times in execution of the log.
     */
    UintR execSrLogPage;
    /**
     *       This variable keeps track of the lfo record where the
     *       pages that were read from disk when an operations log
     *       record were not found in the main memory buffer for log
     *       pages.
     */
    UintR execSrLfoRec;
    /**
     *       The starting page number when reading log from far behind.
     */
    UintR execSrStartPageNo;
    /**
     *       The last page number when reading log from far behind.
     */
    UintR execSrStopPageNo;
    /**
     *       Contains a reference to the first log file, file number 0.
     */
    UintR firstLogfile;
    /** 
     *       This variable contains the oldest operation in this log
     *       part which have not been committed yet.
     */
    UintR firstLogTcrec;
    /**
     *       The first reference to a set of 8 pages. These are used
     *       during execution of the log to keep track of which pages
     *       are in memory and which are not.
     */
    UintR firstPageRef;
    /**
     *       This variable contains the global checkpoint record
     *       waiting for disk writes to complete.
     */
    UintR gcprec;
    /**
     *       The last reference to a set of 8 pages.  These are used
     *       during execution of the log to keep track of which pages
     *       are in memory and which are not.
     */
    UintR lastPageRef;

    struct OperationQueue
    {
      void init() { firstElement = lastElement = RNIL;}
      bool isEmpty() const { return firstElement == RNIL; }
      Uint32 firstElement;
      Uint32 lastElement;
    };

    /**
     * operations queued waiting on REDO to prepare
     */
    struct OperationQueue m_log_prepare_queue;

    /**
     * operations queued waiting on REDO to commit/abort
     */
    struct OperationQueue m_log_complete_queue;

    /**
     *       This variable contains the newest operation in this log
     *       part which have not been committed yet.
     */
    UintR lastLogTcrec;
    /**
     *       This variable indicates which was the last mbyte that was
     *       written before the system crashed.  Discovered during
     *       system restart.
     */
    UintR lastLogfile;
    /**
     *       This variable is used to keep track of the state during
     *       the third phase of the system restart, i.e. when
     *       LogPartRecord::logPartState == 
     *       LogPartRecord::SR_THIRD_PHASE_STARTED.
     */
    LogExecState logExecState;
    /**
     *       This variable contains the lap number of this log part.
     */
    UintR logLap;
    /**
     *       This variable contains the place to stop executing the log
     *       in this phase.
     */
    UintR logLastGci;
    /**
     *       This variable contains the place to start executing the
     *       log in this phase.
     */
    UintR logStartGci;
    /**
     *       The latest GCI completed in this log part.
     */
    UintR logPartNewestCompletedGCI;
    /**
     *       The current state of this log part.                               
     */
    LogPartState logPartState;

    /**
     * does current log-part have tail-problem (i.e 410)
     */
    enum {
      P_TAIL_PROBLEM        = 0x1,// 410
      P_REDO_IO_PROBLEM     = 0x2,// 1234
      P_FILE_CHANGE_PROBLEM = 0x4 // 1220
    };
    Uint32 m_log_problems;

    /**
     *       A timer that is set every time a log page is sent to disk.
     *       Ensures that log pages are not kept in main memory for
     *       more than a certain time.
     */
    UintR logPartTimer;
    /**
     *       The current timer which is set by the periodic signal
     *       received by LQH
     */
    UintR logTimer;
    /** 
     *       Contains the number of the log tail file and the mbyte
     *       reference within that file.  This information ensures that
     *       the tail is not overwritten when writing new log records.
     */
    UintR logTailFileNo;
    /**
     *       The TcConnectionrec used during execution of this log part.
     */
    UintR logTcConrec;
    /**
     *       The number of pages that currently resides in the main
     *       memory buffer.  It does not refer pages that are currently
     *       read from the log files.  Only to pages already read
     *       from the log file.
     */
    UintR mmBufferSize;
    /**
     *       Contains the current number of log files in this log part. 
     */
    UintR noLogFiles;
    /**
     *       This variable is used only during execution of a log
     *       record.  It keeps track of in which page record a log
     *       record was started.  It is used then to deduce which
     *       pages that are dirty after that the log records on the
     *       page have been executed.
     *
     *       It is also used to find out where to write the invalidate
     *       command when that is needed.
     */
    UintR prevLogpage;
    union {
      /**
       *       The number of files remaining to gather GCI information
       *       for during system restart.  Only used if number of files
       *       is larger than 60.
       */
      UintR srRemainingFiles;

      /**
       *       The index of the file which we should start loading redo
       *       meta information from after the 'FRONTPAGE' file has been
       *       closed.
       */
      UintR srLastFileIndex;
    };
    /**
     *       The log file where to start executing the log during
     *       system restart.
     */
    UintR startLogfile;
    /**
     *       The last log file in which to execute the log during system 
     *       restart.                    
     */
    UintR stopLogfile;
    /**
     *       This variable keeps track of when we want to write a complete 
     *       gci log record but have been blocked by an ongoing log operation.
     */
    WaitWriteGciLog waitWriteGciLog;
    /**
     *       The currently executing prepare record starts in this index 
     *       in the log page.            
     */
    Uint16 execSrLogPageIndex;
    /**
     *       Which of the four exec_sr's in the fragment is currently executing
     */
    Uint16 execSrExecuteIndex;
    /**
     *       The number of pages executed in the current mbyte. 
     */
    Uint16 execSrPagesExecuted;
    /**
     *       The number of pages read from disk that have arrived and are 
     *       currently awaiting execution of the log.
     */
    Uint16 execSrPagesRead;
    /**
     *       The number of pages read from disk and currently not arrived 
     *       to the block.             
     */
    Uint16 execSrPagesReading;
    /**
     *       This variable refers to the new header file where we will 
     *       start writing the log after a system restart have been completed.
     */
    Uint16 headFileNo;
    /**
     *       This variable refers to the page number within the header file.
     */
    Uint16 headPageNo;
    /**
     *       This variable refers to the index within the new header
     *       page.
     */
    Uint16 headPageIndex;
    /**
     *       This variables indicates which was the last mbyte in the last 
     *       logfile before a system crash. Discovered during system restart.
     */
    Uint16 lastMbyte;
    /**
     *       This variable is used only during execution of a log
     *       record. It keeps track of in which file page a log
     *       record was started.  It is used if it is needed to write a
     *       dirty page to disk during log execution (this happens when
     *       commit records are invalidated).
     */
    Uint16 prevFilepage;
    /**
     *       This is used to save where we were in the execution of log
     *       records when we find a commit record that needs to be
     *       executed.
     *
     *       This variable is also used to remember the index where the
     *       log type was in the log record. It is only used in this
     *       role when finding a commit record that needs to be
     *       invalidated.
     */
    Uint16 savePageIndex;
    Uint16 logTailMbyte;
    /**
     *       The mbyte within the starting log file where to start 
     *       executing the log.                
     */
    Uint16 startMbyte;
    /**
     *       The last mbyte in which to execute the log during system
     *       restart.
     */
    Uint16 stopMbyte;
   /**
     *       This variable refers to the file where invalidation is
     *       occuring during system/node restart.
     */
    Uint16 invalidateFileNo;
    /**
     *       This variable refers to the page where invalidation is
     *       occuring during system/node restart.
     */
    Uint16 invalidatePageNo;
    /**
     *       For MT LQH the log part (0-3).
     */
    Uint16 logPartNo;

    /**
     * Keep track of the first invalid log page found in our search. This
     * enables us to print information about irregular writes of log pages
     * at the end of the REDO log.
     */
    Uint16 endInvalidMByteSearch;
    Uint16 firstInvalidateFileNo;
    Uint16 firstInvalidatePageNo;
    bool firstInvalidatePageFound;
    /**
     * IO tracker...
     */
    struct IOTracker m_io_tracker;
    
    RedoWorkStats m_redoWorkStats;
  }; // Size 164 Bytes
  typedef Ptr<LogPartRecord> LogPartRecordPtr;
  
  /* $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ */
  /* $$$$$$$                      LOG FILE RECORD                     $$$$$$$ */
  /* $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ */
  /*       THIS RECORD IS ALIGNED TO BE 288 (256 + 32) BYTES.                 */
  /* $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ */
  /**
   *              This record contains information about a log file.          
   *              A log file contains log records from several tables and     
   *              fragments of a table. LQH can contain more than             
   *              one log file to ensure faster log processing.               
   *                                                                          
   *              The number of pages to write to disk at a time is           
   *              configurable.                                               
   */
  struct LogFileRecord {
    LogFileRecord() {}

    enum FileChangeState {
      NOT_ONGOING = 0,
      BOTH_WRITES_ONGOING = 1,
      LAST_WRITE_ONGOING = 2,
      FIRST_WRITE_ONGOING = 3,
      WRITE_PAGE_ZERO_ONGOING = 4,
      WAIT_FOR_OPEN_NEXT_FILE = 5,
      LAST_FILEWRITE_WAITS = 6,
      FIRST_FILEWRITE_WAITS = 7
    };  
    enum LogFileStatus {
      LFS_IDLE = 0,                     ///< Log file record not in use
      CLOSED = 1,                       ///< Log file closed
      OPENING_INIT = 2,
      OPEN_SR_FRONTPAGE = 3,            ///< Log file opened as part of system
                                        ///< restart.  Open file 0 to find  
                                        ///< the front page of the log part.
      OPEN_SR_LAST_FILE = 4,            ///< Open last log file that was written
                                        ///< before the system restart.  
      OPEN_SR_NEXT_FILE = 5,            ///< Open a log file which is 16 files 
                                        ///< backwards to find the next    
                                        ///< information about GCPs.
      OPEN_EXEC_SR_START = 6,           ///< Log file opened as part of 
                                        ///< executing 
                                        ///< log during system restart. 
      OPEN_EXEC_SR_NEW_MBYTE = 7,
      OPEN_SR_FOURTH_PHASE = 8,
      OPEN_SR_FOURTH_NEXT = 9,
      OPEN_SR_FOURTH_ZERO = 10,
      OPENING_WRITE_LOG = 11,           ///< Log file opened as part of writing 
                                        ///< log during normal operation. 
      OPEN_EXEC_LOG = 12,
      CLOSING_INIT = 13,
      CLOSING_SR = 14,                  ///< Log file closed as part of system 
                                        ///< restart.  Currently trying to  
                                        ///< find where to start executing the 
                                        ///< log
      CLOSING_EXEC_SR = 15,             ///< Log file closed as part of 
                                        ///< executing log during system restart
      CLOSING_EXEC_SR_COMPLETED = 16,
      CLOSING_WRITE_LOG = 17,           ///< Log file closed as part of writing 
                                        ///< log during normal operation. 
      CLOSING_EXEC_LOG = 18,
      OPEN_INIT = 19,
      OPEN = 20,                         ///< Log file open
      OPEN_SR_READ_INVALIDATE_PAGES = 21,
      CLOSE_SR_READ_INVALIDATE_PAGES = 22,
      OPEN_SR_WRITE_INVALIDATE_PAGES = 23,
      CLOSE_SR_WRITE_INVALIDATE_PAGES = 24,
      OPEN_SR_READ_INVALIDATE_SEARCH_FILES = 25,
      CLOSE_SR_READ_INVALIDATE_SEARCH_FILES = 26,
      CLOSE_SR_READ_INVALIDATE_SEARCH_LAST_FILE = 27
#ifndef NO_REDO_OPEN_FILE_CACHE
      ,OPEN_EXEC_LOG_CACHED = 28
      ,CLOSING_EXEC_LOG_CACHED = 29
#endif
      ,CLOSING_SR_FRONTPAGE = 30
    };
    
    /**
     *       When a new mbyte is started in the log we have to find out
     *       how far back in the log we still have prepared operations
     *       which have been neither committed or aborted.  This variable
     *       keeps track of this value for each of the mbytes in this
     *       log file.  This is used in writing down these values in the
     *       header of each log file.  That information is used during
     *       system restart to find the tail of the log.  
     */
    UintR *logLastPrepRef;
    /**
     *       The max global checkpoint completed before the mbyte in the
     *       log file was started.  One variable per mbyte.  
     */
    UintR *logMaxGciCompleted;
    /**
     *       The max global checkpoint started before the mbyte in the log
     *       file was started.  One variable per mbyte.
     */
    UintR *logMaxGciStarted;
    /**
     *       This variable contains the file name as needed by the file 
     *       system when opening the file.
     */
    UintR fileName[4];
    /**
     *       This variable has a reference to the log page which is 
     *       currently in use by the log.     
     */
    UintR currentLogpage;
    /**
     *       The number of the current mbyte in the log file.
     */
    UintR currentMbyte;
    /**
     *       This variable is used when changing files.  It is to find
     *       out when both the last write in the previous file and the
     *       first write in this file has been completed.  After these
     *       writes have completed the variable keeps track of when the
     *       write to page zero in file zero is completed.  
     */
    FileChangeState fileChangeState;
    /**
     *       The number of the file within this log part.
     */
    UintR fileNo;
    /**
     *       This variable shows where to read/write the next pages into
     *       the log.  Used when writing the log during normal operation
     *       and when reading the log during system restart.  It
     *       specifies the page position where each page is 8 kbyte.
     */
    UintR filePosition;
    /**
     *       This contains the file pointer needed by the file system
     *       when reading/writing/closing and synching.  
     */
    UintR fileRef;
    /**
     *       The head of the pages waiting for shipment to disk. 
     *       They are filled with log info. 
     */
    UintR firstFilledPage;
    /**
     *       A list of active read/write operations on the log file.
     *       Operations are always put in last and the first should
     *       always complete first.  
     */
    UintR firstLfo;
    UintR lastLfo;
    /**
     *       The tail of the pages waiting for shipment to disk. 
     *       They are filled with log info.
     */
    UintR lastFilledPage;
    /**
     *       This variable keeps track of the last written page in the
     *       file while writing page zero in file zero when changing log
     *       file.  
     */
    UintR lastPageWritten;
    /**
     *       This variable keeps track of the last written word in the
     *       last page written in the file while writing page zero in
     *       file zero when changing log file.  
     */
    UintR lastWordWritten;
    /**
     *       This variable contains the last word written in the last page.
     */
    LogFileStatus logFileStatus;
    /**
     *       A reference to page zero in this file. 
     *       This page is written before the file is closed.  
     */
    UintR logPageZero;
    /**
     *       This variable contains a reference to the record describing 
     *       this log part.   One of four records (0,1,2 or 3).
     */
    UintR logPartRec;
    /**
     *       Next free log file record or next log file in this log.
     */
    UintR nextLogFile;
    /**
     *       The previous log file.
     */
    UintR prevLogFile;
    /**
     *       The number of remaining words in this mbyte of the log file.
     */
    UintR remainingWordsInMbyte;
    /**
     *       The current file page within the current log file. This is
     *       a reference within the file and not a reference to a log
     *       page record.  It is used to deduce where log records are
     *       written.  Particularly completed gcp records and prepare log
     *       records.  
     */
    Uint16 currentFilepage;
    /**
     *       The number of pages in the list referenced by 
     *       LOG_PAGE_BUFFER.
     */
    Uint16 noLogpagesInBuffer;

#ifndef NO_REDO_OPEN_FILE_CACHE
    Uint32 nextList;
    Uint32 prevList;
#endif
  }; // Size 288 bytes
  typedef Ptr<LogFileRecord> LogFileRecordPtr;
  typedef ArrayPool<LogFileRecord> LogFileRecord_pool;
  typedef DLCFifoList<LogFileRecord_pool> LogFileRecord_fifo;

  /* $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ */
  /* $$$$$$$                      LOG OPERATION RECORD                $$$$$$$ */
  /* $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ */
  /**
   * This record contains a currently active file operation
   * that has started by the log module.
   */
  struct LogFileOperationRecord {
    enum LfoState {
      IDLE = 0,                         ///< Operation is not used at the moment
      INIT_WRITE_AT_END = 1,            ///< Write in file so that it grows to 
                                        ///< 16 Mbyte
      INIT_FIRST_PAGE = 2,              ///< Initialise the first page in a file
      WRITE_GCI_ZERO = 3,
      WRITE_INIT_MBYTE = 4,
      WRITE_DIRTY = 5,
      READ_SR_FRONTPAGE = 6,            ///< Read page zero in file zero during 
                                        ///< system restart               
      READ_SR_LAST_FILE = 7,            ///< Read page zero in last file open 
                                        ///< before system crash            
      READ_SR_NEXT_FILE = 8,            ///< Read 60 files backwards to find 
                                        ///< further information GCPs in page
                                        ///< zero
      READ_SR_LAST_MBYTE = 9,
      READ_EXEC_SR = 10,
      READ_EXEC_LOG = 11,
      READ_SR_FOURTH_PHASE = 12,
      READ_SR_FOURTH_ZERO = 13,
      FIRST_PAGE_WRITE_IN_LOGFILE = 14,
      LAST_WRITE_IN_FILE = 15,
      WRITE_PAGE_ZERO = 16,
      ACTIVE_WRITE_LOG = 17,             ///< A write operation during 
                                        ///< writing of log
      READ_SR_INVALIDATE_PAGES = 18,
      WRITE_SR_INVALIDATE_PAGES = 19,
      WRITE_SR_INVALIDATE_PAGES_UPDATE_PAGE0 = 20
      ,READ_SR_INVALIDATE_SEARCH_FILES = 21
    };
    /**
     * We have to remember the log pages read. 
     * Otherwise we cannot build the linked list after the pages have 
     * arrived to main memory.  
     */
    UintR logPageArray[16];
    /**
     * A list of the pages that are part of this active operation.
     */
    UintR firstLfoPage;
    /**
     * A timer to ensure that records are not lost.
     */
    UintR lfoTimer;
    /**
     * The word number of the last written word in the last during
     * a file write.  
     */
    UintR lfoWordWritten;
    /**
     * This variable contains the state of the log file operation.     
     */
    LfoState lfoState;
    /**
     * The log file that the file operation affects.
     */
    UintR logFileRec;
    /**
     * The log file operations on a file are kept in a linked list.
     */
    UintR nextLfo;
    /**
     * The page number of the first read/written page during a file 
     * read/write.                
     */
    Uint16 lfoPageNo;
    /**
     * The number of pages written or read during an operation to
     * the log file.                
     */
    Uint16 noPagesRw;
  }; // 92 bytes
  typedef Ptr<LogFileOperationRecord> LogFileOperationRecordPtr;
  
  /* $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ */
  /* $$$$$$$                      LOG PAGE RECORD                     $$$$$$$ */
  /* $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ */
  /**
   *    These are the 8 k pages used to store log records before storing
   *    them in the file system. 
   *    Since 64 kbyte is sent to disk at a time it is necessary to have   
   *    at least 4*64 kbytes of log pages. 
   *    To handle multiple outstanding requests we need some additional pages. 
   *    Thus we allocate 1 mbyte to ensure that we do not get problems with 
   *    insufficient number of pages.
   */
  struct LogPageRecord {
    /**
     * This variable contains the pages that are sent to disk. 
     *
     * All pages contain a header of 12 words:
     * - WORD 0:  CHECKSUM             Calculated before storing on disk and 
     *                                 checked when read from disk.
     * - WORD 1:  LAP                  How many wraparounds have the log 
     *                                 experienced since initial start of the
     *                                 system.
     * - WORD 2:  MAX_GCI_COMPLETED    Which is the maximum gci which have 
     *                                 completed before this page. This 
     *                                 gci will not be found in this    
     *                                 page and hereafter in the log.
     * - WORD 3:  MAX_GCI_STARTED      The maximum gci which have started 
     *                                 before this page.    
     * - WORD 4:  NEXT_PAGE            Pointer to the next page. 
     *                                 Only used in main memory      
     * - WORD 5:  PREVIOUS_PAGE        Pointer to the previous page. 
     *                                 Currently not used.       
     * - WORD 6:  VERSION              NDB version that wrote the page.
     * - WORD 7:  NO_LOG_FILES         Number of log files in this log part.
     * - WORD 8:  CURRENT PAGE INDEX   This keeps track of where we are in the 
     *                                 page.   
     *                                 This is only used when pages is in 
     *                                 memory.
     * - WORD 9:  OLD PREPARE FILE NO  This keeps track of the oldest prepare 
     *                                 operation still alive (not committed 
     *                                 or aborted) when this mbyte started.
     * - WORD 10: OLD PREPARE PAGE REF File page reference within this file 
     *                                 number.    
     *                                 Page no + Page index.
     *                                 If no prepare was alive then these 
     *                                 values points this mbyte.
     * - WORD 11: DIRTY FLAG            = 0 means not dirty and 
     *                                  = 1 means the page is dirty.    
     *                                 Is used when executing log when 
     *                                 a need to write invalid commit 
     *                                 records arise.
     *
     * The remaining 2036 words are used for log information, i.e.
     * log records.
     *
     * A log record on this page has the following layout:
     * - WORD 0: LOG RECORD TYPE                           
     *     The following types are supported:
     *     - PREPARE OPERATION       An operation not yet committed.
     *     - NEW PREPARE OPERATION   A prepared operation already 
     *                               logged is inserted 
     *                               into the log again so that the
     *                               log tail can be advanced. 
     *                               This can happen when a transaction is 
     *                               committed for a long time.     
     *     - ABORT TRANSACTION       A previously prepared transaction 
     *                               was aborted.  
     *     - COMMIT TRANSACTION      A previously prepared transaction 
     *                               was committed.
     *     - INVALID COMMIT          A previous commit record was 
     *                               invalidated by a   
     *                               subsequent system restart.
     *                               A log record must be invalidated
     *                               in a system restart if it belongs
     *                               to a global checkpoint id which
     *                               is not included in the system
     *                               restart.
     *                               Otherwise it will be included in
     *                               a subsequent system restart since
     *                               it will then most likely belong
     *                               to a global checkpoint id which
     *                               is part of that system
     *                               restart.  
     *                               This is not a correct behaviour
     *                               since this operation is lost in a
     *                               system restart and should not
     *                               reappear at a later system
     *                               restart.
     *     - COMPLETED GCI           A GCI has now been completed.
     *     - FRAGMENT SPLIT          A fragment has been split 
     *                               (not implemented yet)
     *     - FILE DESCRIPTOR         This is always the first log record 
     *                               in a file.
     *                               It is always placed on page 0 after 
     *                               the header.
     *                               It is written when the file is 
     *                               opened and when the file is closed.
     *     - NEXT LOG RECORD         This log record only records where 
     *                               the next log record starts.
     *     - NEXT MBYTE RECORD       This log record specifies that there 
     *                               are no more log records in this mbyte.
     *
     *
     * A FILE DESCRIPTOR log record continues as follows:
     * - WORD 1: NO_LOG_DESCRIPTORS  This defines the number of 
     *                               descriptors of log files that
     *                               will follow hereafter (max 32).
     *                               the log descriptor will describe
     *                               information about
     *                               max_gci_completed,
     *                               max_gci_started and log_lap at
     *                               every 1 mbyte of the log file
     *                               since a log file is 16 mbyte
     *                               always, i need 16 entries in the
     *                               array with max_gci_completed,
     *                               max_gci_started and log_lap. thus
     *                               32 entries per log file
     *                               descriptor (max 32*48 = 1536,
     *                               always fits in page 0).
     * - WORD 2: LAST LOG FILE       The number of the log file currently 
     *                               open.  This is only valid in file 0.
     * - WORD 3 - WORD 18:           MAX_GCI_COMPLETED for every 1 mbyte 
     *                               in this log file.
     * - WORD 19 - WORD 34:          MAX_GCI_STARTED for every 1 mbyte 
     *                               in this log file.
     *
     * Then it continues for NO_LOG_DESCRIPTORS until all subsequent 
     * log files (max 32) have been properly described.
     *
     *
     * A PREPARE OPERATION log record continues as follows:
     * - WORD 1: LOG RECORD SIZE
     * - WORD 2: HASH VALUE                   
     * - WORD 3: SCHEMA VERSION 
     * - WORD 4: OPERATION TYPE
     *            = 0 READ,
     *            = 1 UPDATE,
     *            = 2 INSERT,
     *            = 3 DELETE
     * - WORD 5: NUMBER OF WORDS IN ATTRINFO PART
     * - WORD 6: KEY LENGTH IN WORDS
     * - WORD 7 - (WORD 7 + KEY_LENGTH - 1)                 The tuple key
     * - (WORD 7 + KEY_LENGTH) - 
     *   (WORD 7 + KEY_LENGTH + ATTRINFO_LENGTH - 1)        The attrinfo
     *                                                                  
     * A log record can be spread in several pages in some cases.
     * The next log record always starts immediately after this log record.
     * A log record does however never traverse a 1 mbyte boundary. 
     * This is used to ensure that we can always come back if something 
     * strange occurs in the log file.
     * To ensure this we also have log records which only records 
     * the next log record.
     *
     *
     * A COMMIT TRANSACTION log record continues as follows:
     * - WORD 1: TRANSACTION ID PART 1
     * - WORD 2: TRANSACTION ID PART 2
     * - WORD 3: FRAGMENT ID OF THE OPERATION
     * - WORD 4: TABLE ID OF THE OPERATION
     * - WORD 5: THE FILE NUMBER OF THE PREPARE RECORD
     * - WORD 6: THE STARTING PAGE NUMBER OF THE PREPARE RECORD
     * - WORD 7: THE STARTING PAGE INDEX OF THE PREPARE RECORD
     * - WORD 8: THE STOP PAGE NUMBER OF THE PREPARE RECORD
     * - WORD 9: GLOBAL CHECKPOINT OF THE TRANSACTION
     *
     *
     * An ABORT TRANSACTION log record continues as follows:
     * - WORD 1: TRANSACTION ID PART 1
     * - WORD 2: TRANSACTION ID PART 2 
     *
     * 
     * A COMPLETED CGI log record continues as follows:
     * - WORD 1: THE COMPLETED GCI                                 
     *
     *
     * A NEXT LOG RECORD log record continues as follows:
     * - There is no more information needed. 
     *   The next log record will always refer to the start of the next page.
     *
     * A NEXT MBYTE RECORD log record continues as follows:
     * - There is no more information needed. 
     *   The next mbyte will always refer to the start of the next mbyte.
     */
    UintR logPageWord[8192]; // Size 32 kbytes
  };  
  typedef Ptr<LogPageRecord> LogPageRecordPtr;

  struct PageRefRecord {
    UintR pageRef[8];
    UintR prNext;
    UintR prPrev;
    Uint16 prFileNo;
    Uint16 prPageNo;
  }; // size 44 bytes
  typedef Ptr<PageRefRecord> PageRefRecordPtr;

  struct Tablerec {
    enum TableStatus {
      TABLE_DEFINED = 0,
      NOT_DEFINED = 1,
      ADD_TABLE_ONGOING = 2,
      PREP_DROP_TABLE_DONE = 3,
      DROP_TABLE_WAIT_USAGE = 4,
      DROP_TABLE_WAIT_DONE = 5,
      DROP_TABLE_ACC = 6,
      DROP_TABLE_TUP = 7,
      DROP_TABLE_TUX = 8
      ,TABLE_READ_ONLY = 9
    };
    
    UintR fragrec[MAX_FRAG_PER_LQH];
    Uint16 fragid[MAX_FRAG_PER_LQH];
    /**
     * Status of the table 
     */
    TableStatus tableStatus;
    /**
     * Table type and target table of index.
     */
    Uint16 tableType;
    Uint16 primaryTableId;
    Uint32 schemaVersion;
    Uint8 m_disk_table;
    bool  m_informed_backup_drop_tab;

    Uint32 usageCountR; // readers
    Uint32 usageCountW; // writers
  }; // Size 100 bytes
  typedef Ptr<Tablerec> TablerecPtr;
#endif // DBLQH_STATE_EXTRACT
  struct TcConnectionrec {
    enum LogWriteState {
      NOT_STARTED = 0,
      NOT_WRITTEN = 1,
      NOT_WRITTEN_WAIT = 2,
      WRITTEN = 3
    };
    enum AbortState {
      ABORT_IDLE = 0,
      ABORT_ACTIVE = 1,
      NEW_FROM_TC = 2,
      REQ_FROM_TC = 3,
      ABORT_FROM_TC = 4,
      ABORT_FROM_LQH = 5
    };
    enum TransactionState {
      IDLE = 0,

      /* -------------------------------------------------------------------- */
      // Transaction in progress states
      /* -------------------------------------------------------------------- */
      WAIT_ACC = 1,
      WAIT_TUPKEYINFO = 2,
      WAIT_ATTR = 3,
      WAIT_TUP = 4,
      LOG_QUEUED = 6,
      PREPARED = 7,
      LOG_COMMIT_WRITTEN_WAIT_SIGNAL = 8,
      LOG_COMMIT_QUEUED_WAIT_SIGNAL = 9,
      
      /* -------------------------------------------------------------------- */
      // Commit in progress states
      /* -------------------------------------------------------------------- */
      LOG_COMMIT_QUEUED = 11,
      COMMIT_QUEUED = 12,
      COMMITTED = 13,
      WAIT_TUP_COMMIT= 35,
      
      /* -------------------------------------------------------------------- */
      // Abort in progress states
      /* -------------------------------------------------------------------- */
      WAIT_ACC_ABORT = 14,
      ABORT_QUEUED = 15,
      WAIT_AI_AFTER_ABORT = 17,
      LOG_ABORT_QUEUED = 18,
      WAIT_TUP_TO_ABORT = 19,
      
      /* -------------------------------------------------------------------- */
      // Scan in progress states
      /* -------------------------------------------------------------------- */
      WAIT_SCAN_AI = 20,
      SCAN_STATE_USED = 21,
      SCAN_TUPKEY = 30,
      COPY_TUPKEY = 31,

      TC_NOT_CONNECTED = 32,
      PREPARED_RECEIVED_COMMIT = 33, // Temporary state in write commit log
      LOG_COMMIT_WRITTEN = 34        // Temporary state in write commit log
    };
    enum ConnectState {
      DISCONNECTED = 0,
      CONNECTED = 1,
      COPY_CONNECTED = 2,
      LOG_CONNECTED = 3
    };
#ifndef DBLQH_STATE_EXTRACT
    ConnectState connectState;
    UintR copyCountWords;
    Uint32 keyInfoIVal;
    Uint32 attrInfoIVal;
    UintR transid[2];
    AbortState abortState;
    UintR accConnectrec;
    UintR applOprec;
    UintR clientConnectrec;
    UintR tcTimer;
    UintR currReclenAi;
    UintR currTupAiLen;
    UintR fragmentid;
    UintR fragmentptr;
    UintR gci_hi;
    UintR gci_lo;
    UintR hashValue;
    
    UintR logStartFileNo;
    LogWriteState logWriteState;
    UintR nextHashRec;
    UintR nextLogTcrec;
    UintR nextTcLogQueue;
    UintR nextTcConnectrec;
    UintR prevHashRec;
    UintR prevLogTcrec;
    UintR prevTcLogQueue;
    UintR readlenAi;
    UintR reqRef;
    UintR reqinfo;
    UintR schemaVersion;
    UintR storedProcId;
    UintR simpleTcConnect;
    UintR tableref;
    UintR tcOprec;
    UintR hashIndex;
    Uint32 tcHashKeyHi;
    UintR tcScanInfo;
    UintR tcScanRec;
    UintR totReclenAi;
    UintR totSendlenAi;
    UintR tupConnectrec;
    UintR savePointId;
    TransactionState transactionState;
    BlockReference applRef;
    BlockReference clientBlockref;

    BlockReference reqBlockref;
    BlockReference tcBlockref;
    BlockReference tcAccBlockref;
    BlockReference tcTuxBlockref;
    BlockReference tcTupBlockref;
    Uint32 commitAckMarker;
    union {
      Uint32 m_scan_curr_range_no;
      UintR numFiredTriggers;
    };
    Uint32 m_corrFactorLo; // For result correlation for linked operations.
    Uint32 m_corrFactorHi;
    Uint64 lqhKeyReqId;
    Uint16 errorCode;
    Uint16 logStartPageIndex;
    Uint16 logStartPageNo;
    Uint16 logStopPageNo;
    Uint16 nextReplica;
    Uint16 primKeyLen;
    Uint16 save1;
    Uint16 nodeAfterNext[3];

    Uint8 activeCreat;
    Uint8 dirtyOp;
    Uint8 indTakeOver;
    Uint8 lastReplicaNo;
    Uint8 lockType;
    Uint8 nextSeqNoReplica;
    Uint8 opSimple;
    Uint8 opExec;
    Uint8 operation;
    Uint8 m_reorg;
    Uint8 reclenAiLqhkey;
    Uint8 replicaType;
    Uint8 seqNoReplica;
    Uint8 tcNodeFailrec;
    Uint8 m_disk_table;
    Uint8 m_use_rowid;
    Uint8 m_dealloc;
    Uint8 m_fire_trig_pass;
    enum op_flags {
      OP_ISLONGREQ              = 0x1,
      OP_SAVEATTRINFO           = 0x2,
      OP_SCANKEYINFOPOSSAVED    = 0x4,
      OP_DEFERRED_CONSTRAINTS   = 0x8,
      OP_NORMAL_PROTOCOL        = 0x10,
      OP_DISABLE_FK             = 0x20,
      OP_NO_TRIGGERS            = 0x40
    };
    Uint32 m_flags;
    Uint32 m_log_part_ptr_i;
    SectionReader::PosInfo scanKeyInfoPos;
    Local_key m_row_id;

    struct {
      Uint32 m_cnt;
      Uint32 m_page_id[2];
      Local_key m_disk_ref[2];
    } m_nr_delete;
    Uint32 accOpPtr; /* for scan lock take over */
#endif // DBLQH_STATE_EXTRACT
  }; /* p2c: size = 280 bytes */

#ifndef DBLQH_STATE_EXTRACT
  typedef Ptr<TcConnectionrec> TcConnectionrecPtr;

  struct TcNodeFailRecord {
    enum TcFailStatus {
      TC_STATE_TRUE = 0,
      TC_STATE_FALSE = 1,
      TC_STATE_BREAK = 2
    };
    UintR lastNewTcRef;
    UintR newTcRef;
    TcFailStatus tcFailStatus;
    UintR tcRecNow;
    BlockReference lastNewTcBlockref;
    BlockReference newTcBlockref;
    Uint32 lastTakeOverInstanceId;
    Uint32 takeOverInstanceId;
    Uint32 maxInstanceId;
    Uint16 oldNodeId;
  };
  typedef Ptr<TcNodeFailRecord> TcNodeFailRecordPtr;

  struct CommitLogRecord {
    Uint32 startPageNo;
    Uint32 startPageIndex;
    Uint32 stopPageNo;
    Uint32 fileNo;
  };
  //for statistic information about redo log initialization
  Uint32 totalLogFiles;
  Uint32 logFileInitDone;
  Uint32 totallogMBytes;
  Uint32 logMBytesInitDone;

  Uint32 m_startup_report_frequency;
  NDB_TICKS m_last_report_time;

  struct LocalSysfileStruct
  {
    LocalSysfileStruct() {}
    Uint32 m_node_restorable_on_its_own;
    Uint32 m_max_gci_restorable;
    Uint32 m_dihPtr;
    Uint32 m_dihRef;
    Uint32 m_save_gci;
  } c_local_sysfile;
  void send_read_local_sysfile(Signal*);
  void write_local_sysfile_restore_complete(Signal*);
  void write_local_sysfile_gcp_complete(Signal *signal, Uint32 gci);
  void write_local_sysfile_restart_complete(Signal*);
  void write_local_sysfile_restore_complete_done(Signal*);
  void write_local_sysfile_gcp_complete_done(Signal *signal);

  void write_local_sysfile_restart_complete_done(Signal*);

  void write_local_sysfile(Signal*, Uint32, Uint32);
  void sendLCP_FRAG_ORD(Signal*, Uint32 fragPtrI);

public:
  Dblqh(Block_context& ctx, Uint32 instanceNumber = 0);
  virtual ~Dblqh();

  void execTUPKEYCONF(Signal* signal);
  Uint32 get_scan_api_op_ptr(Uint32 scan_ptr_i);

  Uint32 get_is_scan_prioritised(Uint32 scan_ptr_i);
  Uint32 getCreateSchemaVersion(Uint32 tableId);
private:

  BLOCK_DEFINES(Dblqh);

  bool is_prioritised_scan(BlockReference resultRef)
  {
    /**
     * Scans that return data within the same thread to the
     * BACKUP and DBLQH block are always prioritised (LCP
     * scans, Backup scans and node recovery scans.
     */
    NodeId nodeId = refToNode(resultRef);
    Uint32 block = refToMain(resultRef);
    if (nodeId != getOwnNodeId())
      return false;
    if (block == BACKUP ||
        block == DBLQH)
      return true;
    return false;
  }

  void execPACKED_SIGNAL(Signal* signal);
  void execDEBUG_SIG(Signal* signal);
  void execATTRINFO(Signal* signal);
  void execKEYINFO(Signal* signal);
  void execLQHKEYREQ(Signal* signal);
  void execLQHKEYREF(Signal* signal);
  void execCOMMIT(Signal* signal);
  void execCOMPLETE(Signal* signal);
  void execLQHKEYCONF(Signal* signal);
  void execTESTSIG(Signal* signal);
  void execLQH_RESTART_OP(Signal* signal);
  void execCONTINUEB(Signal* signal);
  void execSTART_RECREQ(Signal* signal);
  void execSTART_RECCONF(Signal* signal);
  void execEXEC_FRAGREQ(Signal* signal);
  void execEXEC_FRAGCONF(Signal* signal);
  void execEXEC_FRAGREF(Signal* signal);
  void execSTART_EXEC_SR(Signal* signal);
  void execEXEC_SRREQ(Signal* signal);
  void execEXEC_SRCONF(Signal* signal);
  void execREAD_PSEUDO_REQ(Signal* signal);
  void execSIGNAL_DROPPED_REP(Signal* signal);

  void execDBINFO_SCANREQ(Signal* signal); 
  void execDUMP_STATE_ORD(Signal* signal);
  void execACC_ABORTCONF(Signal* signal);
  void execNODE_FAILREP(Signal* signal);
  void execCHECK_LCP_STOP(Signal* signal);
  void execSEND_PACKED(Signal* signal);
  void execTUP_ATTRINFO(Signal* signal);
  void execREAD_CONFIG_REQ(Signal* signal);

  void execCREATE_TAB_REQ(Signal* signal);
  void execCREATE_TAB_REF(Signal* signal);
  void execCREATE_TAB_CONF(Signal* signal);
  void execLQHADDATTREQ(Signal* signal);
  void execTUP_ADD_ATTCONF(Signal* signal);
  void execTUP_ADD_ATTRREF(Signal* signal);

  void execLQHFRAGREQ(Signal* signal);
  void execACCFRAGCONF(Signal* signal);
  void execACCFRAGREF(Signal* signal);
  void execTUPFRAGCONF(Signal* signal);
  void execTUPFRAGREF(Signal* signal);

  void execDROP_FRAG_REQ(Signal*);
  void execDROP_FRAG_REF(Signal*);
  void execDROP_FRAG_CONF(Signal*);

  void execTAB_COMMITREQ(Signal* signal);
  void execACCSEIZECONF(Signal* signal);
  void execACCSEIZEREF(Signal* signal);
  void execREAD_NODESCONF(Signal* signal);
  void execREAD_NODESREF(Signal* signal);
  void execSTTOR(Signal* signal);
  void execNDB_STTOR(Signal* signal);
  void execTUPSEIZECONF(Signal* signal);
  void execTUPSEIZEREF(Signal* signal);
  void execACCKEYCONF(Signal* signal);
  void execACCKEYREF(Signal* signal);
  void execTUPKEYREF(Signal* signal);
  void execABORT(Signal* signal);
  void execABORTREQ(Signal* signal);
  void execCOMMITREQ(Signal* signal);
  void execCOMPLETEREQ(Signal* signal);
  void execMEMCHECKREQ(Signal* signal);
  void execSCAN_FRAGREQ(Signal* signal);
  void execSCAN_NEXTREQ(Signal* signal);
  void execACC_SCANREF(Signal* signal, TcConnectionrecPtr);
  void execNEXT_SCANCONF(Signal* signal);
  void execNEXT_SCANREF(Signal* signal);
  void execACC_TO_REF(Signal* signal, TcConnectionrecPtr);
  void execCOPY_FRAGREQ(Signal* signal);
  void execCOPY_FRAGREF(Signal* signal);
  void execCOPY_FRAGCONF(Signal* signal);
  void execPREPARE_COPY_FRAG_REQ(Signal* signal);
  void execUPDATE_FRAG_DIST_KEY_ORD(Signal*);
  void execCOPY_ACTIVEREQ(Signal* signal);
  void execLQH_TRANSREQ(Signal* signal);
  void execTRANSID_AI(Signal* signal);
  void execINCL_NODEREQ(Signal* signal);

  void force_lcp(Signal* signal);
  void execLCP_FRAG_ORD(Signal* signal);
  void execEMPTY_LCP_REQ(Signal* signal);
  
  void execSTART_FRAGREQ(Signal* signal);
  void execSTART_RECREF(Signal* signal);

  void execGCP_SAVEREQ(Signal* signal);
  void execSUB_GCP_COMPLETE_REP(Signal* signal);
  void execFSOPENREF(Signal* signal);
  void execFSOPENCONF(Signal* signal);
  void execFSCLOSECONF(Signal* signal);
  void execFSWRITECONF(Signal* signal);
  void execFSWRITEREF(Signal* signal);
  void execFSREADCONF(Signal* signal);
  void execFSREADREF(Signal* signal);
  void execFSWRITEREQ(Signal*);
  void execTIME_SIGNAL(Signal* signal);
  void execFSSYNCCONF(Signal* signal);

  void execALTER_TAB_REQ(Signal* signal);
  void execALTER_TAB_CONF(Signal* signal);

  void execCREATE_TRIG_IMPL_CONF(Signal* signal);
  void execCREATE_TRIG_IMPL_REF(Signal* signal);
  void execCREATE_TRIG_IMPL_REQ(Signal* signal);

  void execDROP_TRIG_IMPL_CONF(Signal* signal);
  void execDROP_TRIG_IMPL_REF(Signal* signal);
  void execDROP_TRIG_IMPL_REQ(Signal* signal);

  void execPREP_DROP_TAB_REQ(Signal* signal);
  void execDROP_TAB_REQ(Signal* signal);
  void execDROP_TAB_REF(Signal*);
  void execDROP_TAB_CONF(Signal*);
  void dropTable_nextStep(Signal*, AddFragRecordPtr);

  void execTUP_DEALLOCREQ(Signal* signal);
  void execLQH_WRITELOG_REQ(Signal* signal);

  void execTUXFRAGCONF(Signal* signal);
  void execTUXFRAGREF(Signal* signal);
  void execTUX_ADD_ATTRCONF(Signal* signal);
  void execTUX_ADD_ATTRREF(Signal* signal);

  void execBUILD_INDX_IMPL_REF(Signal* signal);
  void execBUILD_INDX_IMPL_CONF(Signal* signal);

  void execFIRE_TRIG_REQ(Signal*);

  void execREAD_LOCAL_SYSFILE_CONF(Signal*);
  void execWRITE_LOCAL_SYSFILE_CONF(Signal*);

  void execSTART_NODE_LCP_REQ(Signal*);
  void execSTART_LOCAL_LCP_ORD(Signal*);
  void execSTART_FULL_LOCAL_LCP_ORD(Signal*);
  void execUNDO_LOG_LEVEL_REP(Signal*);
  void execHALT_COPY_FRAG_REQ(Signal*);
  void execHALT_COPY_FRAG_CONF(Signal*);
  void execHALT_COPY_FRAG_REF(Signal*);
  void execRESUME_COPY_FRAG_REQ(Signal*);
  void execRESUME_COPY_FRAG_CONF(Signal*);
  void execRESUME_COPY_FRAG_REF(Signal*);
  // Statement blocks

  void send_halt_copy_frag(Signal*);
  void send_resume_copy_frag(Signal*);
  void send_halt_copy_frag_conf(Signal*, bool);
  void send_resume_copy_frag_conf(Signal*);

  void sendLOCAL_RECOVERY_COMPLETE_REP(Signal *signal,
                LocalRecoveryCompleteRep::PhaseIds);
  void timer_handling(Signal *signal);
  void init_acc_ptr_list(ScanRecord*);
  bool seize_acc_ptr_list(ScanRecord*, Uint32, Uint32);
  void release_acc_ptr_list(ScanRecord*);
  Uint32 get_acc_ptr_from_scan_record(ScanRecord*, Uint32, bool);
  void set_acc_ptr_in_scan_record(ScanRecord*, Uint32, Uint32);
  void i_get_acc_ptr(ScanRecord*, Uint32*&, Uint32);
  
  void removeTable(Uint32 tableId);
  void sendLCP_COMPLETE_REP(Signal* signal, Uint32 lcpId);
  void sendEMPTY_LCP_CONF(Signal* signal, bool idle);
  void sendLCP_FRAGIDREQ(Signal* signal);
  void sendLCP_FRAG_REP(Signal * signal, const LcpRecord::FragOrd &,
                        const Fragrecord*) const;

  void updatePackedList(Signal* signal, HostRecord * ahostptr, Uint16 hostId);
  void LQHKEY_abort(Signal* signal, int errortype, TcConnectionrecPtr);
  void LQHKEY_error(Signal* signal, int errortype);
  void nextRecordCopy(Signal* signal, TcConnectionrecPtr);
  Uint32 calculateHash(Uint32 tableId, const Uint32* src);
  void checkLcpStopBlockedLab(Signal* signal);
  void sendCommittedTc(Signal* signal,
                       BlockReference atcBlockref,
                       const TcConnectionrec*);
  void sendCompletedTc(Signal* signal,
                       BlockReference atcBlockref,
                       const TcConnectionrec*);
  void sendLqhkeyconfTc(Signal* signal,
                        BlockReference atcBlockref,
                        TcConnectionrecPtr);
  void sendCommitLqh(Signal* signal,
                     BlockReference alqhBlockref,
                     const TcConnectionrec*);
  void sendCompleteLqh(Signal* signal,
                       BlockReference alqhBlockref,
                       const TcConnectionrec*);
  void sendPackedSignal(Signal* signal,
                        struct PackedWordsContainer * container);
  void cleanUp(Signal* signal, TcConnectionrecPtr);
  void sendAttrinfoLoop(Signal* signal);
  void sendAttrinfoSignal(Signal* signal);
  void sendLqhAttrinfoSignal(Signal* signal);
  Uint32 initScanrec(const class ScanFragReq *,
                     Uint32 aiLen,
                     TcConnectionrecPtr);
  void initScanTc(const class ScanFragReq *,
                  Uint32 transid1,
                  Uint32 transid2,
                  Uint32 fragId,
                  Uint32 nodeId,
                  Uint32 hashHi,
                  TcConnectionrecPtr);
  bool finishScanrec(Signal* signal,
                     ScanRecordPtr &restart,
                     TcConnectionrecPtr);
  void releaseScanrec(Signal* signal);
  void seizeScanrec(Signal* signal);
  Uint32 sendKeyinfo20(Signal* signal, ScanRecord *, TcConnectionrec *);
  void sendTCKEYREF(Signal*, Uint32 dst, Uint32 route, Uint32 cnt);
  void sendScanFragConf(Signal* signal,
                        Uint32 scanCompleted,
                        const TcConnectionrec*);

  void send_next_NEXT_SCANREQ(Signal* signal,
                              SimulatedBlock* block,
                              ExecFunction f,
                              ScanRecord * const scanPtr);

  void initCopyrec(Signal* signal);
  void initCopyTc(Signal* signal, Operation_t, TcConnectionrec*);
  void sendCopyActiveConf(Signal* signal,Uint32 tableId);
  void checkLcpCompleted(Signal* signal);
  void checkLcpHoldop(Signal* signal);
  bool checkLcpStarted(Signal* signal);
  void checkLcpTupprep(Signal* signal);
  void getNextFragForLcp(Signal* signal);
  void sendAccContOp(Signal* signal);
  void setLogTail(Signal* signal, Uint32 keepGci);
  Uint32 remainingLogSize(const LogFileRecordPtr &sltCurrLogFilePtr,
			  const LogPartRecordPtr &sltLogPartPtr);
  bool checkGcpCompleted(Signal* signal, Uint32 pageWritten, Uint32 wordWritten);
  void initFsopenconf(Signal* signal);
  void initFsrwconf(Signal* signal, bool write);
  void initLfo(Signal* signal);
  void initLogfile(Signal* signal, Uint32 fileNo);
  void initLogpage(Signal* signal);
  void openFileRw(Signal* signal, LogFileRecordPtr olfLogFilePtr, bool writeBuffer = true);
  void openLogfileInit(Signal* signal);
  void openNextLogfile(Signal* signal);
  void releaseLfo(Signal* signal);
  void releaseLfoPages(Signal* signal);
  void releaseLogpage(Signal* signal);
  void seizeLfo(Signal* signal);
  void seizeLogfile(Signal* signal);
  void seizeLogpage(Signal* signal);
  void writeFileDescriptor(Signal* signal);
  void writeFileHeaderOpen(Signal* signal, Uint32 type);
  void writeInitMbyte(Signal* signal);
  void writeSinglePage(Signal* signal, Uint32 pageNo,
                       Uint32 wordWritten, Uint32 place,
                       bool sync = true);
  void buildLinkedLogPageList(Signal* signal);
  void changeMbyte(Signal* signal);
  Uint32 checkIfExecLog(Signal* signal, TcConnectionrecPtr);
  void checkNewMbyte(Signal* signal, const TcConnectionrec*);
  void checkReadExecSr(Signal* signal);
  void checkScanTcCompleted(Signal* signal, TcConnectionrecPtr);
  void closeFile(Signal* signal, LogFileRecordPtr logFilePtr, Uint32 place);
  void completedLogPage(Signal* signal,
                        Uint32 clpType,
                        Uint32 place,
                        bool sync_flag = false);

  void commit_reorg(TablerecPtr tablePtr);
  void wait_reorg_suma_filter_enabled(Signal*);

  void deleteFragrec(Uint32 fragId);
  void deleteTransidHash(Signal* signal, TcConnectionrecPtr& tcConnectptr);
  void findLogfile(Signal* signal,
                   Uint32 fileNo,
                   LogPartRecordPtr flfLogPartPtr,
                   LogFileRecordPtr* parLogFilePtr);
  void findPageRef(Signal* signal, CommitLogRecord* commitLogRecord);
  int  findTransaction(UintR Transid1,
                       UintR Transid2,
                       UintR TcOprec,
                       UintR hi,
                       TcConnectionrecPtr& tcConnectptr);
  void getFirstInLogQueue(Signal* signal, Ptr<TcConnectionrec>&dst);
  void remove_from_prepare_log_queue(Signal *signal,
                                     TcConnectionrecPtr tcPtr);
  bool getFragmentrec(Signal* signal, Uint32 fragId);
  void initialiseAddfragrec(Signal* signal);
  void initialiseFragrec(Signal* signal);
  void initialiseGcprec(Signal* signal);
  void initialiseLcpRec(Signal* signal);
  void initialiseLfo(Signal* signal);
  void initialiseLogFile(Signal* signal);
  void initialiseLogPage(Signal* signal);
  void initialiseLogPart(Signal* signal);
  void initialisePageRef(Signal* signal);
  void initialiseScanrec(Signal* signal);
  void initialiseTabrec(Signal* signal);
  void initialiseTcrec(Signal* signal);
  void initialiseTcNodeFailRec(Signal* signal);
  void initFragrec(Signal* signal,
                   Uint32 tableId,
                   Uint32 fragId,
                   Uint32 copyType);
  void initFragrecSr(Signal* signal);
  void initGciInLogFileRec(Signal* signal, Uint32 noFdDesc);
  void initLogpart(Signal* signal);
  void initLogPointers(Signal* signal, TcConnectionrecPtr);
  void initReqinfoExecSr(Signal* signal, TcConnectionrecPtr);
  bool insertFragrec(Signal* signal, Uint32 fragId);
  void linkWaitLog(Signal*,
                   LogPartRecordPtr,
                   LogPartRecord::OperationQueue &,
                   TcConnectionrecPtr);
  void logNextStart(Signal* signal);
  void moveToPageRef(Signal* signal);
  void readAttrinfo(Signal* signal, TcConnectionrecPtr);
  void readCommitLog(Signal* signal,
                     CommitLogRecord* commitLogRecord,
                     TcConnectionrecPtr);
  void readExecLog(Signal* signal);
  void readExecSrNewMbyte(Signal* signal);
  void readExecSr(Signal* signal);
  void readKey(Signal* signal, TcConnectionrecPtr);
  void readLogData(Signal* signal, Uint32 noOfWords, Uint32& sectionIVal);
  void readLogHeader(Signal* signal, TcConnectionrecPtr);
  Uint32 readLogword(Signal* signal);
  Uint32 readLogwordExec(Signal* signal);
  void readSinglePage(Signal* signal, Uint32 pageNo);
  void releaseActiveCopy(Signal* signal);
  void releaseAddfragrec(Signal* signal);
  void releaseFragrec();
  void releaseOprec(Signal* signal, TcConnectionrec*);
  void releasePageRef(Signal* signal);
  void releaseMmPages(Signal* signal);
  void releasePrPages(Signal* signal);
  void releaseTcrec(Signal* signal, TcConnectionrecPtr);
  void releaseTcrecLog(Signal* signal, TcConnectionrecPtr);
  void removeLogTcrec(Signal* signal, TcConnectionrecPtr);
  void removePageRef(Signal* signal);
  Uint32 returnExecLog(Signal* signal, TcConnectionrecPtr);
  int saveAttrInfoInSection(const Uint32* dataPtr,
                            Uint32 len,
                            TcConnectionrec*);
  void seizeAddfragrec(Signal* signal);
  Uint32 seizeSingleSegment();
  Uint32 copyNextRange(Uint32 * dst, TcConnectionrec*);

  void seizeFragmentrec(Signal* signal);
  void seizePageRef(Signal* signal);
  void seizeTcrec(TcConnectionrecPtr& tcConnectptr);
  void sendAborted(Signal* signal, TcConnectionrecPtr);
  void sendLqhTransconf(Signal* signal,
                        LqhTransConf::OperationStatus,
                        TcConnectionrecPtr);
  void sendTupkey(Signal* signal, const TcConnectionrec*);
  void startExecSr(Signal* signal);
  void startNextExecSr(Signal* signal);
  void startTimeSupervision(Signal* signal);
  void stepAhead(Signal* signal, Uint32 stepAheadWords);
  void systemError(Signal* signal, int line);
  void writeAbortLog(Signal* signal, const TcConnectionrec*);
  void writeCommitLog(Signal* signal,
                      LogPartRecordPtr regLogPartPtr,
                      const TcConnectionrec*);
  void writeCompletedGciLog(Signal* signal);
  void writeDbgInfoPageHeader(LogPageRecordPtr logPagePtr, Uint32 place,
                              Uint32 pageNo, Uint32 wordWritten);
  void writeDirty(Signal* signal, Uint32 place);
  void writeKey(Signal* signal, const TcConnectionrec*);
  void writeLogHeader(Signal* signal, const TcConnectionrec*);
  void writeLogWord(Signal* signal, Uint32 data);
  void writeLogWords(Signal* signal, const Uint32* data, Uint32 len);
  void writeNextLog(Signal* signal);
  void errorReport(Signal* signal, int place);
  void warningReport(Signal* signal, int place);
  void invalidateLogAfterLastGCI(Signal *signal);
  Uint32 nextLogFilePtr(Uint32 logFilePtrI);
  void readFileInInvalidate(Signal *signal, int stepNext);
  void writeFileInInvalidate(Signal *signal, int stepPrev);
  bool invalidateCloseFile(Signal*, Ptr<LogPartRecord>, Ptr<LogFileRecord>,
                           LogFileRecord::LogFileStatus status);
  void exitFromInvalidate(Signal* signal);
  Uint32 calcPageCheckSum(LogPageRecordPtr logP);
  Uint32 handleLongTupKey(Signal* signal,
                          Uint32* dataPtr,
                          Uint32 len,
                          TcConnectionrec*);

  void rebuildOrderedIndexes(Signal* signal, Uint32 tableId);

  // Generated statement blocks
  void systemErrorLab(Signal* signal, int line);
  void initFourth(Signal* signal);
  void packLqhkeyreqLab(Signal* signal, TcConnectionrecPtr);
  void sendNdbSttorryLab(Signal* signal);
  void execSrCompletedLab(Signal* signal);
  void execLogRecord(Signal* signal);
  void srPhase3Comp(Signal* signal);
  void srLogLimits(Signal* signal);
  void srGciLimits(Signal* signal);
  void srPhase3Start(Signal* signal);
  void checkStartCompletedLab(Signal* signal);
  void continueAbortLab(Signal* signal, TcConnectionrecPtr);
  void abortContinueAfterBlockedLab(Signal* signal, TcConnectionrec*);
  void abortCommonLab(Signal* signal, TcConnectionrecPtr);
  void localCommitLab(Signal* signal, TcConnectionrecPtr);
  void abortErrorLab(Signal* signal, TcConnectionrecPtr);
  void continueAfterReceivingAllAiLab(Signal* signal, TcConnectionrecPtr);
  void continueACCKEYCONF(Signal* signal,
                          Uint32 localKey1,
                          Uint32 localKey2,
                          TcConnectionrecPtr);
  void abortStateHandlerLab(Signal* signal, TcConnectionrecPtr);
  void writeAttrinfoLab(Signal* signal, const TcConnectionrec*);
  void scanAttrinfoLab(Signal* signal,
                       Uint32* dataPtr,
                       Uint32 length,
                       TcConnectionrecPtr);
  void abort_scan(Signal* signal,
                  Uint32 scan_ptr_i,
                  Uint32 errcode,
                  TcConnectionrecPtr);
  void localAbortStateHandlerLab(Signal* signal, TcConnectionrecPtr);
  void logLqhkeyreqLab(Signal* signal, TcConnectionrecPtr);
  void logLqhkeyreqLab_problems(Signal* signal, TcConnectionrecPtr);
  void update_log_problem(Signal*, LogPartRecordPtr, Uint32 problem, bool);
  void lqhAttrinfoLab(Signal* signal,
                      Uint32* dataPtr,
                      Uint32 length,
                      TcConnectionrecPtr);
  void rwConcludedAiLab(Signal* signal, TcConnectionrecPtr);
  void aiStateErrorCheckLab(Signal* signal,
                            Uint32* dataPtr,
                            Uint32 length,
                            TcConnectionrecPtr);
  void takeOverErrorLab(Signal* signal, TcConnectionrecPtr);
  void endgettupkeyLab(Signal* signal, TcConnectionrecPtr);
  bool checkTransporterOverloaded(Signal* signal,
                                  const NodeBitmask& all,
                                  const class LqhKeyReq* req);
  void earlyKeyReqAbort(Signal* signal, 
                        const class LqhKeyReq * lqhKeyReq, 
                        bool isLongReq,
                        Uint32 errorCode,
                        TcConnectionrecPtr);
  void logLqhkeyrefLab(Signal* signal, TcConnectionrecPtr);
  void closeCopyLab(Signal* signal, TcConnectionrec*);
  void commitReplyLab(Signal* signal, TcConnectionrec*);
  void completeUnusualLab(Signal* signal, TcConnectionrecPtr);
  void completeTransNotLastLab(Signal* signal, TcConnectionrecPtr);
  void completedLab(Signal* signal, TcConnectionrecPtr);
  void copyCompletedLab(Signal* signal, TcConnectionrecPtr);
  void completeLcpRoundLab(Signal* signal, Uint32 lcpId);
  void continueAfterLogAbortWriteLab(Signal* signal, TcConnectionrecPtr);
  void sendAttrinfoLab(Signal* signal);
  void sendExecConf(Signal* signal);
  void execSr(Signal* signal);
  void srFourthComp(Signal* signal);
  void timeSup(Signal* signal);
  void closeCopyRequestLab(Signal* signal, TcConnectionrecPtr);
  void closeScanRequestLab(Signal* signal, TcConnectionrecPtr);
  void scanTcConnectLab(Signal* signal, Uint32 startTcCon, Uint32 fragId);
  void initGcpRecLab(Signal* signal);
  void prepareContinueAfterBlockedLab(Signal* signal, TcConnectionrecPtr);
  void commitContinueAfterBlockedLab(Signal* signal, TcConnectionrecPtr);
  void sendExecFragRefLab(Signal* signal);
  void fragrefLab(Signal* signal, Uint32 errorCode, const LqhFragReq* req);
  void abortAddFragOps(Signal* signal);
  void rwConcludedLab(Signal* signal, TcConnectionrecPtr);
  void sendsttorryLab(Signal* signal);
  void initialiseRecordsLab(Signal* signal, Uint32 data, Uint32, Uint32);
  void startphase2Lab(Signal* signal, Uint32 config);
  void startphase3Lab(Signal* signal);
  void startphase6Lab(Signal* signal);
  void moreconnectionsLab(Signal* signal, TcConnectionrecPtr);
  void scanReleaseLocksLab(Signal* signal, TcConnectionrec*);
  void closeScanLab(Signal* signal, TcConnectionrec*);
  void scanNextLoopLab(Signal* signal);
  void commitReqLab(Signal* signal,
                    Uint32 gci_hi,
                    Uint32 gci_lo,
                    TcConnectionrecPtr);
  void completeTransLastLab(Signal* signal, TcConnectionrecPtr);
  void tupScanCloseConfLab(Signal* signal, TcConnectionrecPtr);
  void tupCopyCloseConfLab(Signal* signal, TcConnectionrecPtr);
  void accScanCloseConfLab(Signal* signal, TcConnectionrecPtr);
  void accCopyCloseConfLab(Signal* signal, TcConnectionrecPtr);
  void nextScanConfScanLab(Signal* signal,
                           ScanRecord * const scanPtr,
                           Uint32 fragId,
                           Uint32 accOpPtr,
                           TcConnectionrecPtr);
  void nextScanConfCopyLab(Signal* signal, TcConnectionrecPtr);
  void continueScanNextReqLab(Signal* signal, TcConnectionrec*);
  bool keyinfoLab(const Uint32 * src, Uint32 len, TcConnectionrecPtr);
  void copySendTupkeyReqLab(Signal* signal);
  void storedProcConfScanLab(Signal* signal, TcConnectionrecPtr);
  void copyStateFinishedLab(Signal* signal);
  void lcpCompletedLab(Signal* signal);
  void lcpStartedLab(Signal* signal);
  void completed_fragment_checkpoint(Signal *signal,
                                     const LcpRecord::FragOrd & fragOrd);
  void prepare_next_fragment_checkpoint(Signal* signal);
  void perform_fragment_checkpoint(Signal *signal);
  void handleFirstFragment(Signal *signal);
  void startLcpRoundLab(Signal* signal);
  void startFragRefLab(Signal* signal);
  void move_start_gci_forward(Signal*, Uint32);
  void srCompletedLab(Signal* signal);
  void openFileInitLab(Signal* signal);
  void openSrFrontpageLab(Signal* signal);
  void openSrLastFileLab(Signal* signal);
  void openSrNextFileLab(Signal* signal);
  void openExecSrStartLab(Signal* signal);
  void openExecSrNewMbyteLab(Signal* signal);
  void openSrFourthPhaseLab(Signal* signal);
  void openSrFourthZeroSkipInitLab(Signal* signal);
  void openSrFourthZeroLab(Signal* signal);
  void openExecLogLab(Signal* signal);
  void checkInitCompletedLab(Signal* signal);
  void closingSrLab(Signal* signal);
  void closingSrFrontPage(Signal* signal);
  void closeExecSrLab(Signal* signal);
  void execLogComp(Signal* signal);
  void execLogComp_extra_files_closed(Signal* signal);
  void closeWriteLogLab(Signal* signal);
  void closeExecLogLab(Signal* signal);
  void writePageZeroLab(Signal* signal, Uint32 from);
  void lastWriteInFileLab(Signal* signal);
  void initWriteEndLab(Signal* signal);
  void initFirstPageLab(Signal* signal);
  void writeGciZeroLab(Signal* signal);
  void writeDirtyLab(Signal* signal);
  void writeInitMbyteLab(Signal* signal);
  void writeLogfileLab(Signal* signal);
  void firstPageWriteLab(Signal* signal);
  void readSrLastMbyteLab(Signal* signal);
  void readSrLastFileLab(Signal* signal);
  void readSrNextFileLab(Signal* signal);
  void readExecSrLab(Signal* signal);
  void readExecLogLab(Signal* signal);
  void readSrFourthPhaseLab(Signal* signal);
  void readSrFourthZeroLab(Signal* signal);
  void copyLqhKeyRefLab(Signal* signal, TcConnectionrecPtr);
  void restartOperationsLab(Signal* signal);
  void lqhTransNextLab(Signal* signal, TcNodeFailRecordPtr tcNodeFailPtr);
  void restartOperationsAfterStopLab(Signal* signal);
  void startphase1Lab(Signal* signal, Uint32 config, Uint32 nodeId);
  void tupkeyConfLab(Signal* signal,
                     TcConnectionrecPtr);
  void copyTupkeyRefLab(Signal* signal, TcConnectionrecPtr);
  void copyTupkeyConfLab(Signal* signal, TcConnectionrecPtr);
  void scanTupkeyConfLab(Signal* signal, TcConnectionrec*);
  void scanTupkeyRefLab(Signal* signal, TcConnectionrecPtr);
  void accScanConfScanLab(Signal* signal, TcConnectionrecPtr);
  void accScanConfCopyLab(Signal* signal);
  void scanLockReleasedLab(Signal* signal, TcConnectionrec *);
  void openSrFourthNextLab(Signal* signal);
  void closingInitLab(Signal* signal);
  void closeExecSrCompletedLab(Signal* signal);
  void readSrFrontpageLab(Signal* signal);
  
  void sendCreateTabReq(Signal*, AddFragRecordPtr);
  void sendAddAttrReq(Signal* signal);
  void sendAddFragReq(Signal* signal);
  void dropTab_wait_usage(Signal*);
  Uint32 get_table_state_error(Ptr<Tablerec> tabPtr) const;
  void wait_readonly(Signal*);
  int check_tabstate(Signal * signal,
                     const Tablerec * tablePtrP,
                     Uint32 op,
                     TcConnectionrecPtr);

  void remove_commit_marker(TcConnectionrec * const regTcPtr);
  // Initialisation
  void initData();
  void initRecords();
protected:
  virtual bool getParam(const char* name, Uint32* count);

public:
  void lcp_max_completed_gci(Uint32 & maxCompletedGci,
                             Uint32 max_gci_written,
                             Uint32 restorable_gci);
  void lcp_complete_scan(Uint32 & newestGci);
  Uint32 get_lcp_newest_gci(void);
  void get_lcp_frag_stats(Uint64 & row_count,
                          Uint64 & prev_row_count,
                          Uint64 & row_change_count,
                          Uint64 & memory_used_in_bytes,
                          Uint32 & max_page_cnt);
  Uint32 get_current_local_lcp_id(void);
  void get_redo_size(Uint64 &size_in_bytes);
  void get_redo_usage(Uint64 &used_in_bytes);

private:
  bool validate_filter(Signal*);
  bool match_and_print(Signal*, Ptr<TcConnectionrec>);
  void ndbinfo_write_op(Ndbinfo::Row&, TcConnectionrecPtr tcPtr);

  void define_backup(Signal*);
  void execDEFINE_BACKUP_REF(Signal*);
  void execDEFINE_BACKUP_CONF(Signal*);
  void execBACKUP_FRAGMENT_REF(Signal* signal);
  void execBACKUP_FRAGMENT_CONF(Signal* signal);
  void execLCP_START_REP(Signal *signal);
  void execLCP_PREPARE_REF(Signal* signal);
  void execLCP_PREPARE_CONF(Signal* signal);
  void execEND_LCPREF(Signal* signal);
  void execEND_LCPCONF(Signal* signal);
  void execINFORM_BACKUP_DROP_TAB_CONF(Signal *signal);

  Uint32 m_backup_ptr;
  bool m_node_restart_lcp_second_phase_started;
  bool m_node_restart_first_local_lcp_started;
  Uint32 m_first_activate_fragment_ptr_i;
  Uint32 m_second_activate_fragment_ptr_i;
  Uint32 m_curr_lcp_id;
  Uint32 m_curr_local_lcp_id;
  Uint32 m_next_local_lcp_id;
  Uint32 c_saveLcpId;
  Uint32 c_restart_localLcpId;
  Uint32 c_restart_lcpId;
  Uint32 c_restart_maxLcpId;
  Uint32 c_restart_maxLocalLcpId;

  void execWAIT_COMPLETE_LCP_REQ(Signal*);
  void execWAIT_ALL_COMPLETE_LCP_CONF(Signal*);

  bool handle_lcp_fragment_first_phase(Signal*);
  void activate_redo_log(Signal*, Uint32, Uint32);
  void start_lcp_second_phase(Signal*);
  void complete_local_lcp(Signal*);

  void send_restore_lcp(Signal * signal);
  void execRESTORE_LCP_REF(Signal* signal);
  void execRESTORE_LCP_CONF(Signal* signal);

  /**
   * For periodic redo log file initialization status reporting 
   * and explicit redo log file status reporting
   */
  /* Init at start of redo log file initialization, timers etc... */
  void initReportStatus(Signal* signal);
  /* Check timers for reporting at certain points */
  void checkReportStatus(Signal* signal);
  /* Send redo log file initialization status, invoked either periodically, or explicitly */
  void reportStatus(Signal* signal);
  /* redo log file initialization completed report*/
  void logfileInitCompleteReport(Signal* signal);
 
  void check_send_scan_hb_rep(Signal* signal, ScanRecord*, TcConnectionrec*);

  void unlockError(Signal* signal, Uint32 error, TcConnectionrecPtr);
  void handleUserUnlockRequest(Signal* signal, TcConnectionrecPtr);
  
  void execLCP_STATUS_CONF(Signal* signal);
  void execLCP_STATUS_REF(Signal* signal);

private:

  void startLcpFragWatchdog(Signal* signal);
  void stopLcpFragWatchdog();
  void invokeLcpFragWatchdogThread(Signal* signal);
  void checkLcpFragWatchdog(Signal* signal);
  const char* lcpStateString(LcpStatusConf::LcpState);
  
  Dbtup* c_tup;
  Dbtux* c_tux;
  Dbacc* c_acc;
  Backup* c_backup;
  Lgman* c_lgman;
  Restore* c_restore;

  /**
   * Read primary key from tup
   */
  Uint32 readPrimaryKeys(ScanRecord*, TcConnectionrec*, Uint32 * dst);

  /**
   * Read primary key from operation
   */
public:
  Uint32 readPrimaryKeys(Uint32 opPtrI, Uint32 * dst, bool xfrm);
private:

  void acckeyconf_tupkeyreq(Signal*, TcConnectionrec*, Fragrecord*,
                            Uint32, Uint32, Uint32);
  void acckeyconf_load_diskpage(Signal*,TcConnectionrecPtr,Fragrecord*,
                                Uint32, Uint32);

  void handle_nr_copy(Signal*, Ptr<TcConnectionrec>);
  void exec_acckeyreq(Signal*, Ptr<TcConnectionrec>);
  int compare_key(const TcConnectionrec*, const Uint32 * ptr, Uint32 len);
  void nr_copy_delete_row(Signal*, Ptr<TcConnectionrec>, Local_key*, Uint32);
  Uint32 getKeyInfoWordOrZero(const TcConnectionrec* regTcPtr, 
                              Uint32 offset);
public:
  struct Nr_op_info
  {
    Uint32 m_ptr_i;
    Uint32 m_tup_frag_ptr_i;
    Uint32 m_gci_hi;
    Uint32 m_gci_lo;
    Uint32 m_page_id;
    Local_key m_disk_ref;
    Local_key m_row_id;
  };
  void get_nr_op_info(Nr_op_info*, Uint32 page_id = RNIL);
  void nr_delete_complete(Signal*, Nr_op_info*);
  Uint64 m_update_size;
  Uint64 m_insert_size;
  Uint64 m_delete_size;
  void add_update_size(Uint64 average_row_size)
  {
    m_update_size += average_row_size;
  }
  void add_insert_size(Uint64 average_row_size)
  {
    m_insert_size += average_row_size;
  }
  void add_delete_size(Uint64 average_row_size)
  {
    m_delete_size += average_row_size;
  }
  
public:
  void acckeyconf_load_diskpage_callback(Signal*, Uint32, Uint32);
  
private:
  void next_scanconf_load_diskpage(Signal* signal, 
				   ScanRecord * const scanPtr,
				   Ptr<TcConnectionrec> regTcPtr,
				   Fragrecord* fragPtrP);
  
  void next_scanconf_tupkeyreq(Signal* signal,
                               ScanRecord * const scanPtr,
			       TcConnectionrec * regTcPtr,
			       Fragrecord* fragPtrP,
			       Uint32 disk_page);

public:  
  void next_scanconf_load_diskpage_callback(Signal* signal, Uint32, Uint32);

  void tupcommit_conf_callback(Signal* signal, Uint32 tcPtrI);
private:
  void tupcommit_conf(Signal* signal,
                      TcConnectionrecPtr,
                      Fragrecord *);

  void mark_end_of_lcp_restore(Signal* signal);
  void log_fragment_copied(Signal* signal);
  
// ----------------------------------------------------------------
// These are variables handling the records. For most records one
// pointer to the array of structs, one pointer-struct, a file size
// and a first free record variable. The pointer struct are temporary
// variables that are kept on the class object since there are often a
// great deal of those variables that exist simultaneously and
// thus no perfect solution of handling them is currently available.
// ----------------------------------------------------------------
/* ------------------------------------------------------------------------- */
/*       POSITIONS WITHIN THE ATTRINBUF AND THE MAX SIZE OF DATA WITHIN AN   */
/*       ATTRINBUF.                                                          */
/* ------------------------------------------------------------------------- */


#define ZADDFRAGREC_FILE_SIZE 1
  AddFragRecord *addFragRecord;
  AddFragRecordPtr addfragptr;
  UintR cfirstfreeAddfragrec;
  UintR caddfragrecFileSize;
  Uint32 c_active_add_frag_ptr_i;

// Configurable
  FragrecordPtr fragptr;
  Fragrecord_pool c_fragment_pool;
  RSS_AP_SNAPSHOT(c_fragment_pool);

#define ZGCPREC_FILE_SIZE 1
  GcpRecord *gcpRecord;
  GcpRecordPtr gcpPtr;
  UintR cgcprecFileSize;

// MAX_NDB_NODES is the size of this array
  HostRecord *hostRecord;
  UintR chostFileSize;

#define ZNO_CONCURRENT_LCP 1
  LcpRecord *lcpRecord;
  LcpRecordPtr lcpPtr;
  UintR cfirstfreeLcpLoc;
  UintR clcpFileSize;

  LogPartRecord *logPartRecord;
  LogPartRecordPtr logPartPtr;
  UintR clogPartFileSize;
  Uint32 clogFileSize; // In MBYTE
  /* Max entries for log file:mb meta info in file page zero */
  Uint32 cmaxLogFilesInPageZero; 
  /* Max valid entries for log file:mb meta info in file page zero 
   *  = cmaxLogFilesInPageZero - 1
   * as entry zero (for current file) is invalid.
   */
  Uint32 cmaxValidLogFilesInPageZero;

#if defined VM_TRACE || defined ERROR_INSERT
  Uint32 cmaxLogFilesInPageZero_DUMP;
#endif

#if defined ERROR_INSERT
  Uint32 delayOpenFilePtrI;
#endif

// Configurable
  LogFileRecord *logFileRecord;
  LogFileRecordPtr logFilePtr;
  UintR cfirstfreeLogFile;
  UintR clogFileFileSize;

#define ZLFO_MIN_FILE_SIZE 256
// RedoBuffer/32K minimum ZLFO_MIN_FILE_SIZE
  LogFileOperationRecord *logFileOperationRecord;
  LogFileOperationRecordPtr lfoPtr;
  UintR cfirstfreeLfo;
  UintR clfoFileSize;

  LogPageRecord *logPageRecord;
  LogPageRecordPtr logPagePtr;
  UintR cfirstfreeLogPage;
  UintR clogPageFileSize;
  Uint32 clogPageCount;

#define ZPAGE_REF_FILE_SIZE 20
  PageRefRecord *pageRefRecord;
  PageRefRecordPtr pageRefPtr;
  UintR cfirstfreePageRef;
  UintR cpageRefFileSize;

// Configurable
  ScanRecord_pool c_scanRecordPool;
  ScanRecordPtr scanptr;
  Uint32 cscanrecFileSize;
  ScanRecord_list m_reserved_scans; // LCP + NR

// Configurable
  Tablerec *tablerec;
  TablerecPtr tabptr;
  UintR ctabrecFileSize;

// Configurable
  TcConnectionrec *tcConnectionrec;
  UintR cfirstfreeTcConrec;
  UintR ctcConnectrecFileSize;
  Uint32 ctcNumFree;

// MAX_NDB_NODES is the size of this array
  TcNodeFailRecord *tcNodeFailRecord;
  UintR ctcNodeFailrecFileSize;

  Uint16 terrorCode;

  Uint32 c_firstInNodeGroup;

// ------------------------------------------------------------------------
// These variables are used to store block state which do not need arrays
// of struct's.
// ------------------------------------------------------------------------
  Uint32 c_lcpId;
  Uint32 cnoOfFragsCheckpointed;
  Uint32 c_last_force_lcp_time;
  Uint32 c_free_mb_force_lcp_limit; // Force lcp when less than this free mb
  Uint32 c_free_mb_tail_problem_limit; // Set TAIL_PROBLEM when less than this..

  Uint32 c_max_scan_direct_count;
/* ------------------------------------------------------------------------- */
// cmaxWordsAtNodeRec keeps track of how many words that currently are
// outstanding in a node recovery situation.
/* ------------------------------------------------------------------------- */
  UintR cmaxWordsAtNodeRec;
/* ------------------------------------------------------------------------- */
/*THIS STATE VARIABLE IS ZTRUE IF AN ADD NODE IS ONGOING. ADD NODE MEANS     */
/*THAT CONNECTIONS ARE SET-UP TO THE NEW NODE.                               */
/* ------------------------------------------------------------------------- */
  Uint8 caddNodeState;
/* ------------------------------------------------------------------------- */
/*THIS VARIABLE SPECIFIES WHICH TYPE OF RESTART THAT IS ONGOING              */
/* ------------------------------------------------------------------------- */
  Uint16 cstartType;
/* ------------------------------------------------------------------------- */
/*THIS VARIABLE INDICATES WHETHER AN INITIAL RESTART IS ONGOING OR NOT.      */
/* ------------------------------------------------------------------------- */
  Uint8 cinitialStartOngoing;
/* ------------------------------------------------------------------------- */
/*THIS VARIABLE KEEPS TRACK OF WHEN TUP AND ACC HAVE COMPLETED EXECUTING     */
/*THEIR UNDO LOG.                                                            */
/* ------------------------------------------------------------------------- */
  ExecUndoLogState csrExecUndoLogState;
/* ------------------------------------------------------------------------- */
/*THIS VARIABLE KEEPS TRACK OF WHEN TUP AND ACC HAVE CONFIRMED COMPLETION    */
/*OF A LOCAL CHECKPOINT ROUND.                                               */
/* ------------------------------------------------------------------------- */
  LcpCloseState clcpCompletedState;
/* ------------------------------------------------------------------------- */
/*DURING CONNECTION PROCESSES IN SYSTEM RESTART THESE VARIABLES KEEP TRACK   */
/*OF HOW MANY CONNECTIONS AND RELEASES THAT ARE TO BE PERFORMED.             */
/* ------------------------------------------------------------------------- */
/***************************************************************************>*/
/*THESE VARIABLES CONTAIN INFORMATION USED DURING SYSTEM RESTART.            */
/***************************************************************************>*/
/* ------------------------------------------------------------------------- */
/*THIS VARIABLE IS ZTRUE IF THE SIGNAL START_REC_REQ HAVE BEEN RECEIVED.     */
/*RECEPTION OF THIS SIGNAL INDICATES THAT ALL FRAGMENTS THAT THIS NODE       */
/*SHOULD START HAVE BEEN RECEIVED.                                           */
/* ------------------------------------------------------------------------- */
  enum { 
    SRR_INITIAL                = 0
    ,SRR_START_REC_REQ_ARRIVED = 1
    ,SRR_REDO_COMPLETE         = 2
    ,SRR_FIRST_LCP_DONE        = 3
  } cstartRecReq;
  Uint32 cstartRecReqData;
  
/* ------------------------------------------------------------------------- */
/*THIS VARIABLE KEEPS TRACK OF HOW MANY FRAGMENTS THAT PARTICIPATE IN        */
/*EXECUTING THE LOG. IF ZERO WE DON'T NEED TO EXECUTE THE LOG AT ALL.        */
/* ------------------------------------------------------------------------- */
  Uint32 cnoFragmentsExecSr;

  /**
   * This is no of sent GSN_EXEC_FRAGREQ during this log phase
   */
  Uint32 cnoOutstandingExecFragReq;

/* ------------------------------------------------------------------------- */
/*THIS VARIABLE KEEPS TRACK OF WHICH OF THE FIRST TWO RESTART PHASES THAT    */
/*HAVE COMPLETED.                                                            */
/* ------------------------------------------------------------------------- */
  Uint8 csrPhaseStarted;
/* ------------------------------------------------------------------------- */
/*NUMBER OF PHASES COMPLETED OF EXECUTING THE FRAGMENT LOG.                  */
/* ------------------------------------------------------------------------- */
  Uint8 csrPhasesCompleted;
/* ------------------------------------------------------------------------- */
/*THE BLOCK REFERENCE OF THE MASTER DIH DURING SYSTEM RESTART.               */
/* ------------------------------------------------------------------------- */
  BlockReference cmasterDihBlockref;
/* ------------------------------------------------------------------------- */
/*THIS VARIABLE IS THE HEAD OF A LINKED LIST OF FRAGMENTS WAITING TO BE      */
/*RESTORED FROM DISK.                                                        */
/* ------------------------------------------------------------------------- */
  Fragrecord_fifo c_lcp_waiting_fragments;  // StartFragReq'ed
  Fragrecord_fifo c_lcp_restoring_fragments; // Restoring as we speek
  Fragrecord_fifo c_lcp_complete_fragments;  // Restored
  Fragrecord_fifo c_queued_lcp_frag_ord;     //Queue for LCP_FRAG_ORDs
  
/* ------------------------------------------------------------------------- */
/*USED DURING SYSTEM RESTART, INDICATES THE OLDEST GCI THAT CAN BE RESTARTED */
/*FROM AFTER THIS SYSTEM RESTART. USED TO FIND THE LOG TAIL.                 */
/* ------------------------------------------------------------------------- */
  UintR crestartOldestGci;
/* ------------------------------------------------------------------------- */
/*USED DURING SYSTEM RESTART, INDICATES THE NEWEST GCI THAT CAN BE RESTARTED */
/*AFTER THIS SYSTEM RESTART. USED TO FIND THE LOG HEAD.                      */
/* ------------------------------------------------------------------------- */
  UintR crestartNewestGci;

  bool c_is_first_gcp_save_started;
/* ------------------------------------------------------------------------- */
/*THE NUMBER OF LOG FILES. SET AS A PARAMETER WHEN NDB IS STARTED.           */
/* ------------------------------------------------------------------------- */
  UintR cnoLogFiles;
/* ------------------------------------------------------------------------- */
/*THESE TWO VARIABLES CONTAIN THE NEWEST GCI RECEIVED IN THE BLOCK AND THE   */
/*NEWEST COMPLETED GCI IN THE BLOCK.                                         */
/* ------------------------------------------------------------------------- */
  UintR cnewestGci;
  UintR cnewestCompletedGci;
/* ------------------------------------------------------------------------- */
/*THIS VARIABLE ONLY PASSES INFORMATION FROM STTOR TO STTORRY = TEMPORARY    */
/* ------------------------------------------------------------------------- */
  Uint16 csignalKey;
/* ------------------------------------------------------------------------- */
/*THIS VARIABLE CONTAINS THE CURRENT START PHASE IN THE BLOCK. IS ZNIL IF    */
/*NO SYSTEM RESTART IS ONGOING.                                              */
/* ------------------------------------------------------------------------- */
  Uint16 cstartPhase;
/* ------------------------------------------------------------------------- */
/*THIS VARIABLE CONTAIN THE CURRENT GLOBAL CHECKPOINT RECORD. IT'S RNIL IF   */
/*NOT A GCP SAVE IS ONGOING.                                                 */
/* ------------------------------------------------------------------------- */
  UintR ccurrentGcprec;
/* ------------------------------------------------------------------------- */
/*THESE VARIABLES ARE USED TO KEEP TRACK OF ALL ACTIVE COPY FRAGMENTS IN LQH.*/
/* ------------------------------------------------------------------------- */
  Uint8 cnoActiveCopy;
  UintR cactiveCopy[4];
/* ------------------------------------------------------------------------- */
/* These variable is used to keep track of what time we have reported so far */
/* in the TIME_SIGNAL handling.                                              */
/* ------------------------------------------------------------------------- */
  NDB_TICKS c_latestTIME_SIGNAL;
  Uint64 c_elapsed_time_millis;

/* ------------------------------------------------------------------------- */
/*THESE VARIABLES CONTAIN THE BLOCK REFERENCES OF THE OTHER NDB BLOCKS.      */
/*ALSO THE BLOCK REFERENCE OF MY OWN BLOCK = LQH                             */
/* ------------------------------------------------------------------------- */
  BlockReference caccBlockref;
  BlockReference ctupBlockref;
  BlockReference ctuxBlockref;
  BlockReference cownref;
  Uint32 cTransactionDeadlockDetectionTimeout;
  UintR cLqhTimeOutCount;
  UintR cLqhTimeOutCheckCount;
  UintR cnoOfLogPages;
/* ------------------------------------------------------------------------- */
/*THIS VARIABLE CONTAINS MY OWN PROCESSOR ID.                                */
/* ------------------------------------------------------------------------- */
  NodeId cownNodeid;

/* ------------------------------------------------------------------------- */
/*THESE VARIABLES CONTAIN INFORMATION ABOUT THE OTHER NODES IN THE SYSTEM    */
/*THESE VARIABLES ARE MOSTLY USED AT SYSTEM RESTART AND ADD NODE TO SET-UP   */
/*AND RELEASE CONNECTIONS TO OTHER NODES IN THE CLUSTER.                     */
/* ------------------------------------------------------------------------- */
/* ------------------------------------------------------------------------- */
/*THIS ARRAY CONTAINS THE PROCESSOR ID'S OF THE NODES THAT ARE ALIVE.        */
/*CNO_OF_NODES SPECIFIES HOW MANY NODES THAT ARE CURRENTLY ALIVE.            */
/*CNODE_VERSION SPECIFIES THE NDB VERSION EXECUTING ON THE NODE.             */
/* ------------------------------------------------------------------------- */
  UintR cpackedListIndex;
  Uint16 cpackedList[MAX_NDB_NODES];
  UintR cnodeData[MAX_NDB_NODES];
  UintR cnodeStatus[MAX_NDB_NODES];
  UintR cnoOfNodes;

  NdbNodeBitmask m_sr_nodes;
  NdbNodeBitmask m_sr_exec_sr_req;
  NdbNodeBitmask m_sr_exec_sr_conf;

/* ------------------------------------------------------------------------- */
/* THIS VARIABLE CONTAINS THE DIRECTORY OF A HASH TABLE OF ALL ACTIVE        */
/* OPERATION IN THE BLOCK. IT IS USED TO BE ABLE TO QUICKLY ABORT AN         */
/* OPERATION WHERE THE CONNECTION WAS LOST DUE TO NODE FAILURES. IT IS       */
/* ACTUALLY USED FOR ALL ABORTS COMMANDED BY TC.                             */
/* ------------------------------------------------------------------------- */
  UintR preComputedRequestInfoMask;
  UintR ctransidHash[1024];
  
  Uint32 c_diskless;
  Uint32 c_o_direct;
  Uint32 c_o_direct_sync_flag;
  Uint32 m_use_om_init;
  Uint32 c_error_insert_table_id;

#ifndef NO_REDO_PAGE_CACHE
  /***********************************************************
   * MODULE: Redo Page Cache
   *
   *   When running redo, current codes scan log until finding a commit
   *     record (for an operation). The commit record contains a back-pointer
   *     to a prepare-record.
   *
   *   If the prepare record is inside the 512k window that is being read
   *     from redo-log, the access is quick.
   *
   *   But it's not, then the following sequence is performed
   *     [file-open]?[page-read][execute-log-record][file-close]?[release-page]
   *
   *   For big (or long running) transactions this becomes very inefficient
   *
   *   The RedoPageCache changes this so that the pages that are not released
   *     in sequence above, but rather put into a LRU (using RedoBuffer)
   */

  /**
   * This is a "dummy" struct that is used when
   *  putting LogPageRecord-entries into lists/hashes
   */
  struct RedoCacheLogPageRecord
  {
    RedoCacheLogPageRecord() {}
    /**
     * NOTE: These numbers must match page-header definition
     */
    Uint32 header0[15];
    Uint32 m_page_no;
    Uint32 m_file_no;
    Uint32 header1[5];
    Uint32 m_part_no;
    Uint32 nextList;
    Uint32 nextHash;
    Uint32 prevList;
    Uint32 prevHash;
    Uint32 rest[8192-27];

    inline bool equal(const RedoCacheLogPageRecord & p) const {
      return
        (p.m_part_no == m_part_no) &&
        (p.m_page_no == m_page_no) &&
        (p.m_file_no == m_file_no);
    }

    inline Uint32 hashValue() const {
      return (m_part_no << 24) + (m_file_no << 16) + m_page_no;
    }
  };
  typedef ArrayPool<RedoCacheLogPageRecord> RedoCacheLogPageRecord_pool;
  typedef DLHashTable<RedoCacheLogPageRecord_pool> RedoCacheLogPageRecord_hash;
  typedef DLCFifoList<RedoCacheLogPageRecord_pool> RedoCacheLogPageRecord_fifo;

  struct RedoPageCache
  {
    RedoPageCache() : m_hash(m_pool), m_lru(m_pool),
                      m_hits(0),m_multi_page(0), m_multi_miss(0) {}
    RedoCacheLogPageRecord_hash m_hash;
    RedoCacheLogPageRecord_fifo m_lru;
    RedoCacheLogPageRecord_pool m_pool;
    Uint32 m_hits;
    Uint32 m_multi_page;
    Uint32 m_multi_miss;
  } m_redo_page_cache;

  void evict(RedoPageCache&, Uint32 cnt);
  void do_evict(RedoPageCache&, Ptr<RedoCacheLogPageRecord>);
  void addCachePages(RedoPageCache&,
                     Uint32 partNo,
                     Uint32 startPageNo,
                     LogFileOperationRecord*);
  void release(RedoPageCache&);
#endif

#ifndef NO_REDO_OPEN_FILE_CACHE
  struct RedoOpenFileCache
  {
    RedoOpenFileCache() : m_lru(m_pool), m_hits(0), m_close_cnt(0) {}

    LogFileRecord_fifo m_lru;
    LogFileRecord_pool m_pool;
    Uint32 m_hits;
    Uint32 m_close_cnt;
  } m_redo_open_file_cache;

  void openFileRw_cache(Signal* signal, LogFileRecordPtr olfLogFilePtr);
  void closeFile_cache(Signal* signal, LogFileRecordPtr logFilePtr, Uint32);
  void release(Signal*, RedoOpenFileCache&);
#endif

public:
  void execINFO_GCP_STOP_TIMER(Signal*);
  Uint32 c_gcp_stop_timer;

  bool is_same_trans(Uint32 opId, Uint32 trid1, Uint32 trid2);
  void get_op_info(Uint32 opId, Uint32 *hash, Uint32* gci_hi, Uint32* gci_lo,
                   Uint32* transId1, Uint32* transId2);
  void accminupdate(Signal*, Uint32 opPtrI, const Local_key*);
  void accremoverow(Signal*, Uint32 opPtrI, const Local_key*);

  /**
   *
   */
  struct CommitAckMarker {
    CommitAckMarker() {}
    Uint32 transid1;
    Uint32 transid2;
    
    Uint32 apiRef;    // Api block ref
    Uint32 apiOprec;  // Connection Object in NDB API
    BlockReference tcRef;
    union { Uint32 nextPool; Uint32 nextHash; };
    Uint32 prevHash;
    Uint32 reference_count;
    bool in_hash;
    bool removed_by_fail_api;

    inline bool equal(const CommitAckMarker & p) const {
      return ((p.transid1 == transid1) && (p.transid2 == transid2));
    }
    
    inline Uint32 hashValue() const {
      return transid1;
    }
  };

  typedef Ptr<CommitAckMarker> CommitAckMarkerPtr;
  typedef ArrayPool<CommitAckMarker> CommitAckMarker_pool;
  typedef DLHashTable<CommitAckMarker_pool> CommitAckMarker_hash;

  CommitAckMarker_pool m_commitAckMarkerPool;
  CommitAckMarker_hash m_commitAckMarkerHash;
  typedef CommitAckMarker_hash::Iterator CommitAckMarkerIterator;
  void execREMOVE_MARKER_ORD(Signal* signal);
  void scanMarkers(Signal* signal, Uint32 tcNodeFail, Uint32 bucket, Uint32 i);
  bool check_tc_and_update_max_instance(BlockReference ref,
                                        TcNodeFailRecord *tcNodeFailPtr);

  void ndbdFailBlockCleanupCallback(Signal* signal, Uint32 failedNodeID, Uint32 ignoredRc);

  struct MonotonicCounters {
    MonotonicCounters() :
      operations(0) {}

    Uint64 operations;

    Uint32 build_event_rep(Signal* signal) const
    {
      /*
        Read saved value from CONTINUEB, subtract from
        counter and write to EVENT_REP
      */
      struct { const Uint64* ptr; Uint64 old; } vars[] = {
        { &operations, 0 }
      };
      const size_t num = sizeof(vars)/sizeof(vars[0]);

      signal->theData[0] = NDB_LE_OperationReportCounters;

      // Read old values from signal
      for (size_t i = 0; i < num ; i++)
      {
        vars[i].old =
          (signal->theData[1+(2*i)+1] |(Uint64(signal->theData[1+(2*i)])<< 32));
      }

      // Write difference back to signal
      for (size_t i = 0; i < num ; i++)
      {
        signal->theData[1 + i] = (Uint32)(*vars[i].ptr - vars[i].old);
      }
      return 1 + num;
    }

    Uint32 build_continueB(Signal* signal) const
    {
      /* Save current value of counters to CONTINUEB */
      const Uint64* vars[] = { &operations };
      const size_t num = sizeof(vars)/sizeof(vars[0]);

      for (size_t i = 0; i < num ; i++)
      {
        signal->theData[1+i*2] = Uint32(*vars[i] >> 32);
        signal->theData[1+i*2+1] = Uint32(*vars[i]);
      }
      return 1 + num * 2;
    }

  } c_Counters;

  Uint32 c_max_redo_lag;
  Uint32 c_max_redo_lag_counter;
  Uint64 cTotalLqhKeyReqCount;
  Uint32 c_max_parallel_scans_per_frag;

  Uint64 c_keyOverloads;
  
  /* All that apply */
  Uint64 c_keyOverloadsTcNode;
  Uint64 c_keyOverloadsReaderApi;
  Uint64 c_keyOverloadsPeerNode;
  Uint64 c_keyOverloadsSubscriber;
  
  Uint64 c_scanSlowDowns; 
  
  /**
     Startup logging:

     c_fragmentsStarted:
       Total number of fragments started as part of node restart
     c_fragmentsStartedWithCopy:
       Number of fragments started by complete copy where no useful LCP was
       accessible for the fragment.
     c_fragCopyFrag:
       The current fragment id copied
     c_fragCopyTable:
       The current table id copied
     c_fragCopyRowsIns:
       The number of rows inserted in current fragment
     c_fragCopyRowsDel:
       The number of rows deleted in current fragment
     c_fragBytesCopied:
       The number of bytes sent over the wire to copy the current fragment

     c_fragmentCopyStart:
       Time of start of copy fragment
     c_fragmentsCopied:
       Number of fragments copied
     c_totalCopyRowsIns:
       Total number of rows inserted as part of copy process
     c_totalCopyRowsDel:
       Total number of rows deleted as part of copy process
     c_totalBytesCopied:
       Total number of bytes sent over the wire as part of the copy process
  */
  Uint32 c_fragmentsStarted;
  Uint32 c_fragmentsStartedWithCopy;  /* Non trans -> 2PINR */

  Uint32 c_fragCopyFrag;
  Uint32 c_fragCopyTable;
  Uint64 c_fragCopyRowsIns;
  Uint64 c_fragCopyRowsDel;
  Uint64 c_fragBytesCopied;

  Uint64 c_fragmentCopyStart;
  Uint32 c_fragmentsCopied;
  Uint64 c_totalCopyRowsIns;
  Uint64 c_totalCopyRowsDel;
  Uint64 c_totalBytesCopied;

  bool is_first_instance();
  bool is_copy_frag_in_progress();
  bool is_scan_ok(ScanRecord*, Fragrecord::FragStatus);
  void set_min_keep_gci(Uint32 max_completed_gci);

  void sendRESTORABLE_GCI_REP(Signal*, Uint32 gci);
  void start_local_lcp(Signal*, Uint32 lcpId, Uint32 localLcpId);

  void execLCP_ALL_COMPLETE_CONF(Signal*);
  void execSET_LOCAL_LCP_ID_CONF(Signal*);
  void execCOPY_FRAG_NOT_IN_PROGRESS_REP(Signal*);
  void execCUT_REDO_LOG_TAIL_REQ(Signal*);

  /**
   * Variable keeping track of which GCI to keep in REDO log
   * after completing a LCP.
   */
  Uint32 c_max_keep_gci_in_lcp;
  Uint32 c_keep_gci_for_lcp;
  bool c_first_set_min_keep_gci;

  /**
   * Some code and variables to serialize access to NDBCNTR for
   * writes of the local sysfile.
   */
  bool c_start_phase_49_waiting;
  bool c_outstanding_write_local_sysfile;
  bool c_send_gcp_saveref_needed;
  void check_start_phase_49_waiting(Signal*);

  /**
   * Variable that keeps track of maximum GCI that was recorded in the
   * LCP. When this GCI is safe on disk the entire LCP is safe on disk.
   */
  Uint32 c_max_gci_in_lcp;

  /* Have we sent WAIT_COMPLETE_LCP_CONF yet */
  bool c_local_lcp_sent_wait_complete_conf;

  /* Have we sent WAIT_ALL_COMPLETE_LCP_REQ yet */
  bool c_local_lcp_sent_wait_all_complete_lcp_req;

  /**
   * Current ongoing local LCP id, == 0 means distributed LCP */
  Uint32 c_localLcpId;

  /* Counter for starting local LCP ordered by UNDO log overload */
  Uint32 c_current_local_lcp_table_id;

  /**
   * Set flag that indicates that first distributed LCP is started.
   * This means that we should distribute the signal
   * RESTORABLE_GCI_REP to the backup block even if first LCP isn't
   * done yet.
   */
  bool m_first_distributed_lcp_started;
  /**
   * 0/1 toggled for each local LCP executed to keep track of which
   * fragments have been started as part of this local LCP and which
   * haven't.
   */
  Uint8 c_current_local_lcp_instance;

  /* Variable set when local LCP starts and when it stops it is reset */
  bool c_local_lcp_started;

  /**
   * Variable set when local LCP is started due to UNDO log overload.
   */
  bool c_full_local_lcp_started;

  /* Is Copy Fragment process currently ongoing */
  bool c_copy_fragment_in_progress;

  void start_lcp_on_table(Signal*);
  void send_lastLCP_FRAG_ORD(Signal*);

  /**
   * Variables tracking state of Halt/Resume Copy Fragment process on
   * Client side (starting node). Also methods.
   * ------------------------------------------
   */

  /* Copy fragment process have been halted indicator */
  bool c_copy_frag_halted;

  /* Halt process is locked while waiting for response from live node */
  bool c_copy_frag_halt_process_locked;

  /* Is UNDO log currently overloaded */
  bool c_undo_log_overloaded;

  enum COPY_FRAG_HALT_STATE_TYPE
  {
    COPY_FRAG_HALT_STATE_IDLE = 0,
    COPY_FRAG_HALT_WAIT_FIRST_LQHKEYREQ = 1,
    PREPARE_COPY_FRAG_IS_HALTED = 2,
    WAIT_RESUME_COPY_FRAG_CONF = 3,
    WAIT_HALT_COPY_FRAG_CONF = 4,
    COPY_FRAG_IS_HALTED = 5
  };
  /* State of halt copy fragment process */
  COPY_FRAG_HALT_STATE_TYPE c_copy_frag_halt_state;

  /* Save of PREPARE_COPY_FRAGREQ signal */
  PrepareCopyFragReq c_prepare_copy_fragreq_save;

  void send_prepare_copy_frag_conf(Signal*,
                                   PrepareCopyFragReq&,
                                   Uint32,
                                   Uint32);
  /**
   * Variables tracking state of Halt/Resume Copy Fragment process on
   * Server side (live node).
   */
  Uint32 c_tc_connect_rec_copy_frag;
  bool c_copy_frag_live_node_halted;
  bool c_copy_frag_live_node_performing_halt;
  HaltCopyFragReq c_halt_copy_fragreq_save;

  inline bool getAllowRead() const {
    return getNodeState().startLevel < NodeState::SL_STOPPING_3;
  }

  ScanRecord_hash c_scanTakeOverHash;

  inline bool TRACE_OP_CHECK(const TcConnectionrec* regTcPtr);
#ifdef ERROR_INSERT
  void TRACE_OP_DUMP(const TcConnectionrec* regTcPtr, const char * pos);
#endif

#ifdef ERROR_INSERT
  Uint32 c_master_node_id;
#endif

  Uint32 get_node_status(Uint32 nodeId) const;
  bool check_ndb_versions() const;

  void suspendFile(Signal* signal, Uint32 filePtrI, Uint32 millis);
  void suspendFile(Signal* signal, Ptr<LogFileRecord> logFile, Uint32 millis);

  void send_runredo_event(Signal*, LogPartRecord *, Uint32 currgci);

  void sendFireTrigConfTc(Signal* signal, BlockReference ref, Uint32 Tdata[]);
  bool check_fire_trig_pass(Uint32 op, Uint32 pass);

  bool handleLCPSurfacing(Signal *signal);
  bool is_disk_columns_in_table(Uint32 tableId);
  void sendSTART_FRAGCONF(Signal*);
  void handle_check_system_scans(Signal*);
#define ZLCP_CHECK_INDEX 0
#define ZBACKUP_CHECK_INDEX 1
#define ZCOPY_FRAGREQ_CHECK_INDEX 2
  Uint32 c_check_scanptr_i[3];
  Uint32 c_check_scanptr_save_line[3];
  Uint32 c_check_scanptr_save_timer[3];

  AlterTabReq c_keep_alter_tab_req;
  Uint32 c_keep_alter_tab_req_len;
  Uint32 c_executing_redo_log;
  Uint32 c_num_fragments_created_since_restart;
  Uint32 c_fragments_in_lcp;
  bool c_wait_lcp_surfacing;
#endif
};

#ifndef DBLQH_STATE_EXTRACT

inline
bool
Dblqh::ScanRecord::check_scan_batch_completed() const
{
  Uint32 max_rows = m_max_batch_size_rows;
  Uint32 max_bytes = m_max_batch_size_bytes;

  return m_stop_batch ||
    (max_rows > 0 && (m_curr_batch_size_rows >= max_rows))  ||
    (max_bytes > 0 && (m_curr_batch_size_bytes >= max_bytes));
}

inline
void
Dblqh::i_get_acc_ptr(ScanRecord* scanP, Uint32* &acc_ptr, Uint32 index)
{
  /* Return ptr to place where acc ptr for operation with given
   * index is stored.
   * If index == 0, it's stored in the ScanRecord, otherwise it's 
   * stored in a segment linked from the ScanRecord.
   */
  if (index == 0) {
    acc_ptr= (Uint32*)&scanP->scan_acc_op_ptr[0];
  } else {
    
    Uint32 segmentIVal, segment, segmentOffset;
    SegmentedSectionPtr segPtr;

    segment= (index + SectionSegment::DataLength -1) / 
      SectionSegment::DataLength;
    segmentOffset= (index - 1) % SectionSegment::DataLength;
    jamDebug();
    ndbassert( segment < ScanRecord::MaxScanAccSegments );

    segmentIVal= scanP->scan_acc_op_ptr[ segment ];
    getSection(segPtr, segmentIVal);

    acc_ptr= &segPtr.p->theData[ segmentOffset ];
  }
}

inline
bool
Dblqh::is_same_trans(Uint32 opId, Uint32 trid1, Uint32 trid2)
{
  TcConnectionrecPtr regTcPtr;  
  regTcPtr.i= opId;
  ptrCheckGuard(regTcPtr, ctcConnectrecFileSize, tcConnectionrec);
  return ((regTcPtr.p->transid[0] == trid1) &&
          (regTcPtr.p->transid[1] == trid2));
}

inline
void
Dblqh::get_op_info(Uint32 opId, Uint32 *hash, Uint32* gci_hi, Uint32* gci_lo,
                   Uint32* transId1, Uint32* transId2)
{
  TcConnectionrecPtr regTcPtr;  
  regTcPtr.i= opId;
  ptrCheckGuard(regTcPtr, ctcConnectrecFileSize, tcConnectionrec);
  *hash = regTcPtr.p->hashValue;
  *gci_hi = regTcPtr.p->gci_hi;
  *gci_lo = regTcPtr.p->gci_lo;
  *transId1 = regTcPtr.p->transid[0];
  *transId2 = regTcPtr.p->transid[1];
}

inline
void
Dblqh::accminupdate(Signal* signal, Uint32 opId, const Local_key* key)
{
  TcConnectionrecPtr regTcPtr;  
  regTcPtr.i= opId;
  ptrCheckGuard(regTcPtr, ctcConnectrecFileSize, tcConnectionrec);
  signal->theData[0] = regTcPtr.p->accConnectrec;
  signal->theData[1] = key->m_page_no;
  signal->theData[2] = key->m_page_idx;
  c_acc->execACCMINUPDATE(signal);

  if (ERROR_INSERTED(5714))
  {
    FragrecordPtr regFragptr;
    regFragptr.i = regTcPtr.p->fragmentptr;
    c_fragment_pool.getPtr(regFragptr);
    if (regFragptr.p->m_copy_started_state == Fragrecord::AC_NR_COPY)
      ndbout << " LK: " << *key;
  }

  if (ERROR_INSERTED(5712) || ERROR_INSERTED(5713))
    ndbout << " LK: " << *key;
  regTcPtr.p->m_row_id = *key;
}

inline
void
Dblqh::accremoverow(Signal* signal, Uint32 opId, const Local_key* key)
{
  TcConnectionrecPtr regTcPtr;
  regTcPtr.i= opId;
  ptrCheckGuard(regTcPtr, ctcConnectrecFileSize, tcConnectionrec);
  c_acc->removerow(regTcPtr.p->accConnectrec, key);
}

inline
bool
Dblqh::TRACE_OP_CHECK(const TcConnectionrec* regTcPtr)
{
  if (ERROR_INSERTED(5714))
  {
    FragrecordPtr regFragptr;
    regFragptr.i = regTcPtr->fragmentptr;
    c_fragment_pool.getPtr(regFragptr);
    return regFragptr.p->m_copy_started_state == Fragrecord::AC_NR_COPY;
  }

  return (ERROR_INSERTED(5712) && 
	  (regTcPtr->operation == ZINSERT ||
	   regTcPtr->operation == ZDELETE)) ||
    ERROR_INSERTED(5713);
}

inline
bool Dblqh::is_scan_ok(ScanRecord* scanPtrP, Fragrecord::FragStatus fragstatus)
{
  if (fragstatus == Fragrecord::FSACTIVE ||
      (fragstatus == Fragrecord::ACTIVE_CREATION &&
       scanPtrP->lcpScan))
    return true;
  return false;
}
#endif

#undef JAM_FILE_ID

#endif

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

#ifndef DBLQH_H
#define DBLQH_H

#include <pc.hpp>
#include <ndb_limits.h>
#include <SimulatedBlock.hpp>
#include <DLList.hpp>
#include <DLFifoList.hpp>
#include <DLHashTable.hpp>

#include <NodeBitmask.hpp>
#include <signaldata/LCP.hpp>
#include <signaldata/LqhTransConf.hpp>
#include <signaldata/LqhFrag.hpp>

// primary key is stored in TUP
#include <../dbtup/Dbtup.hpp>

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
#define ZNO_MBYTES_IN_FILE 16
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
#define ZOPEN_READ 0
#define ZOPEN_WRITE 1
#define ZOPEN_READ_WRITE 2
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
#define ZFD_PART_SIZE 48
#define ZLOG_HEAD_SIZE 6
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
#define ZMAX_LOG_FILES_IN_PAGE_ZERO 40
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
#define ZRESTART_OPERATIONS_AFTER_STOP 16
#define ZCHECK_LCP_STOP_BLOCKED 17
#define ZSCAN_MARKERS 18
#define ZOPERATION_EVENT_REP 19
#define ZPREP_DROP_TABLE 20

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
//#define ZSCAN_NEXT 1
//#define ZSCAN_NEXT_COMMIT 2
//#define ZSCAN_NEXT_ABORT 12
#define ZCOPY_COMMIT 3
#define ZCOPY_REPEAT 4
#define ZCOPY_ABORT 5
#define ZCOPY_CLOSE 6
//#define ZSCAN_CLOSE 6
//#define ZEMPTY_FRAGMENT 0
#define ZWRITE_LOCK 1
#define ZSCAN_FRAG_CLOSED 2
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
#define ZCOPY_NODE_ERROR 1204
#define ZTOO_MANY_COPY_ACTIVE_ERROR 1208 // COPY_FRAG and COPY_ACTIVEREF code
#define ZCOPY_ACTIVE_ERROR 1210          // COPY_ACTIVEREF error code
#define ZNO_TC_CONNECT_ERROR 1217        // Simple Read + SCAN
/* ------------------------------------------------------------------------- */
/*       ERROR CODES ADDED IN VERSION 1.X                                    */
/* ------------------------------------------------------------------------- */
//#define ZSCAN_BOOK_ACC_OP_ERROR 1219   // SCAN_FRAGREF error code
#define ZFILE_CHANGE_PROBLEM_IN_LOG_ERROR 1220
#define ZTEMPORARY_REDO_LOG_FAILURE 1221
#define ZNO_FREE_MARKER_RECORDS_ERROR 1222
#define ZNODE_SHUTDOWN_IN_PROGESS 1223
#define ZTOO_MANY_FRAGMENTS 1224
#define ZTABLE_NOT_DEFINED 1225
#define ZDROP_TABLE_IN_PROGRESS 1226
#define ZINVALID_SCHEMA_VERSION 1227

/* ------------------------------------------------------------------------- */
/*       ERROR CODES ADDED IN VERSION 2.X                                    */
/* ------------------------------------------------------------------------- */
#define ZNODE_FAILURE_ERROR 400
/* ------------------------------------------------------------------------- */
/*       ERROR CODES FROM ACC                                                */
/* ------------------------------------------------------------------------- */
#define ZNO_TUPLE_FOUND 626
#define ZTUPLE_ALREADY_EXIST 630
/* ------------------------------------------------------------------------- */
/*       ERROR CODES FROM TUP                                                */
/* ------------------------------------------------------------------------- */
#define ZSEARCH_CONDITION_FALSE 899
#define ZUSER_ERROR_CODE_LIMIT 6000
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
class Dblqh: public SimulatedBlock {
public:
  enum LcpCloseState {
    LCP_IDLE = 0,
    LCP_RUNNING = 1,       // LCP is running
    LCP_CLOSE_STARTED = 2, // Completion(closing of files) has started
    ACC_LCP_CLOSE_COMPLETED = 3,
    TUP_LCP_CLOSE_COMPLETED = 4
  };

  enum ExecUndoLogState {
    EULS_IDLE = 0,
    EULS_STARTED = 1,
    EULS_COMPLETED = 2,
    EULS_ACC_COMPLETED = 3,
    EULS_TUP_COMPLETED = 4
  };

  struct AddFragRecord {
    enum AddFragStatus {
      FREE = 0,
      ACC_ADDFRAG = 1,
      WAIT_TWO_TUP = 2,
      WAIT_ONE_TUP = 3,
      WAIT_TWO_TUX = 4,
      WAIT_ONE_TUX = 5,
      WAIT_ADD_ATTR = 6,
      TUP_ATTR_WAIT1 = 7,
      TUP_ATTR_WAIT2 = 8,
      TUX_ATTR_WAIT1 = 9,
      TUX_ATTR_WAIT2 = 10
    };
    LqhAddAttrReq::Entry attributes[LqhAddAttrReq::MAX_ATTRIBUTES];
    UintR accConnectptr;
    AddFragStatus addfragStatus;
    UintR dictConnectptr;
    UintR fragmentPtr;
    UintR nextAddfragrec;
    UintR noOfAllocPages;
    UintR schemaVer;
    UintR tup1Connectptr;
    UintR tup2Connectptr;
    UintR tux1Connectptr;
    UintR tux2Connectptr;
    UintR checksumIndicator;
    UintR GCPIndicator;
    BlockReference dictBlockref;
    Uint32 m_senderAttrPtr;
    Uint16 addfragErrorCode;
    Uint16 attrSentToTup;
    Uint16 attrReceived;
    Uint16 addFragid;
    Uint16 fragid1;
    Uint16 fragid2;
    Uint16 noOfAttr;
    Uint16 noOfNull;
    Uint16 tabId;
    Uint16 totalAttrReceived;
    Uint16 fragCopyCreation;
    Uint16 noOfKeyAttr;
    Uint32 noOfNewAttr; // noOfCharsets in upper half
    Uint16 noOfAttributeGroups;
    Uint16 lh3DistrBits;
    Uint16 tableType;
    Uint16 primaryTableId;
  };// Size 108 bytes
  typedef Ptr<AddFragRecord> AddFragRecordPtr;
  
  /* $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ */
  /* $$$$$$$               ATTRIBUTE INFORMATION RECORD              $$$$$$$ */
  /* $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ */
  /**
   *       Can contain one (1) attrinfo signal. 
   *       One signal contains 24 attr. info words. 
   *       But 32 elements are used to make plex happy.  
   *       Some of the elements are used to the following things:
   *       - Data length in this record is stored in the
   *         element indexed by ZINBUF_DATA_LEN.  
   *       - Next attrinbuf is pointed out by the element 
   *         indexed by ZINBUF_NEXT.
   */
  struct Attrbuf {
    UintR attrbuf[32];
  }; // Size 128 bytes
  typedef Ptr<Attrbuf> AttrbufPtr;

  /* $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ */
  /* $$$$$$$                         DATA BUFFER                     $$$$$$$ */
  /* $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ */
  /**
   *       This buffer is used as a general data storage.                     
   */
  struct Databuf {
    UintR data[4];
    UintR nextDatabuf;
  }; // size 20 bytes
  typedef Ptr<Databuf> DatabufPtr;

  struct ScanRecord {
    enum ScanState {
      SCAN_FREE = 0,
      WAIT_STORED_PROC_COPY = 1,
      WAIT_STORED_PROC_SCAN = 2,
      WAIT_NEXT_SCAN_COPY = 3,
      WAIT_NEXT_SCAN = 4,
      WAIT_DELETE_STORED_PROC_ID_SCAN = 5,
      WAIT_DELETE_STORED_PROC_ID_COPY = 6,
      WAIT_ACC_COPY = 7,
      WAIT_ACC_SCAN = 8,
      WAIT_SCAN_NEXTREQ = 10,
      WAIT_CLOSE_SCAN = 12,
      WAIT_CLOSE_COPY = 13,
      WAIT_RELEASE_LOCK = 14,
      WAIT_TUPKEY_COPY = 15,
      WAIT_LQHKEY_COPY = 16,
      IN_QUEUE = 17
    };
    enum ScanType {
      ST_IDLE = 0,
      SCAN = 1,
      COPY = 2
    };

    UintR scan_acc_op_ptr[32];
    Uint32 scan_acc_index;
    Uint32 scan_acc_attr_recs;
    UintR scanApiOpPtr;
    UintR scanLocalref[2];
    
    Uint32 m_max_batch_size_rows;
    Uint32 m_max_batch_size_bytes;

    Uint32 m_curr_batch_size_rows;
    Uint32 m_curr_batch_size_bytes;

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
    UintR scanLocalFragid;
    UintR scanSchemaVersion;

    /**
     * This is _always_ main table, even in range scan
     *   in which case scanTcrec->fragmentptr is different
     */
    Uint32 fragPtrI;
    UintR scanStoredProcId;
    ScanState scanState;
    UintR scanTcrec;
    ScanType scanType;
    BlockReference scanApiBlockref;
    NodeId scanNodeId;
    Uint16 scanReleaseCounter;
    Uint16 scanNumber;

    // scan source block ACC TUX TUP
    BlockReference scanBlockref;
 
    Uint8 scanCompletedStatus;
    Uint8 scanFlag;
    Uint8 scanLockHold;
    Uint8 scanLockMode;
    Uint8 readCommitted;
    Uint8 rangeScan;
    Uint8 descending;
    Uint8 tupScan;
    Uint8 scanTcWaiting;
    Uint8 scanKeyinfoFlag;
    Uint8 m_last_row;
  }; // Size 272 bytes
  typedef Ptr<ScanRecord> ScanRecordPtr;

  struct Fragrecord {
    enum ExecSrStatus {
      IDLE = 0,
      ACTIVE_REMOVE_AFTER = 1,
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
     * - ACTIVE -> BLOCKED               A local checkpoint is to be 
     *                                   started.  No more operations 
     *                                   are allowed to be started until 
     *                                   the local checkpoint    
     *                                   has been started.
     * - ACTIVE -> REMOVING              A fragment is removed from the node
     * - BLOCKED -> ACTIVE               Operations are allowed again in 
     *                                   the fragment.           
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
      BLOCKED = 3,            ///< LQH is waiting for all active operations to 
                              ///< complete the current phase so that the 
                              ///< local checkpoint can be started.
      ACTIVE_CREATION = 4,    ///< Fragment is defined and active but is under 
                              ///< creation by the primary LQH.
      CRASH_RECOVERING = 5,   ///< Fragment is recovering after a crash by 
                              ///< executing the fragment log and so forth. 
                              ///< Will need further breakdown.
      REMOVING = 6            ///< The fragment is currently removed. 
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
    UintR accFragptr[2];
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
    typedef Bitmask<4> ScanNumberMask;
    ScanNumberMask m_scanNumberMask;
    DLList<ScanRecord>::Head m_activeScans;
    DLFifoList<ScanRecord>::Head m_queuedScans;

    Uint16 srLqhLognode[4];
    /**
     *       The fragment pointers in TUP and TUX
     */
    UintR tupFragptr[2];
    UintR tuxFragptr[2];
    /**
     *       This queue is where operations are put when blocked in ACC
     *       during start of a local chkp.
     */
    UintR accBlockedList;
    /**
     *       This is the queue where all operations that are active on the     
     *       fragment is put. 
     *       This is used to deduct when the fragment do 
     *       no longer contain any active operations. 
     *       This is needed when starting a local checkpoint.
     */
    UintR activeList;
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
     *       Indicates a local checkpoint is active and thus can generate
     *       UNDO log records.
     */
    UintR fragActiveStatus;
    /**
     *       Reference to current LCP record. 
     *       If no LCP is ongoing on the fragment then the value is RNIL.
     *       If LCP_REF /= RNIL then a local checkpoint is ongoing in the 
     *       fragment.
     *       LCP_STATE in LCP_RECORD specifies the state of the 
     *       local checkpoint.
     */
    UintR lcpRef;
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
    UintR nextFrag;
    /**
     *       The newest GCI that has been committed on fragment             
     */
    UintR newestGci;
    SrStatus srStatus;
    UintR srUserptr;
    /**
     *       The starting global checkpoint of this fragment.
     */
    UintR startGci;
    /**
     *       A reference to the table owning this fragment.
     */
    UintR tabRef;
    /**
     *       This is the queue to put operations that have been blocked 
     *       during start of a local chkp.
     */
    UintR firstWaitQueue;
    UintR lastWaitQueue;
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
     *       This variable ensures that only one copy fragment is
     *       active at a time on the fragment.
     */
    Uint8 copyFragState;
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
     *       The identity of the next local checkpoint this fragment
     *       should perform.
     */
    Uint8 nextLcp;
   /**
     *       How many local checkpoints does the fragment contain
     */
    Uint8 srChkpnr;
    Uint8 srNoLognodes;
    /**
     *       Table type.
     */
    Uint8 tableType;
    /**
     *       For ordered index fragment, i-value of corresponding
     *       fragment in primary table.
     */
    UintR tableFragptr;
  };
  typedef Ptr<Fragrecord> FragrecordPtr;
  
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
    Uint16 gcpFilePtr[4];
    /** 
     *       The page number within the file for each log part.
     */
    Uint16 gcpPageNo[4];
    /**
     *       The word number within the last page that was written for
     *       each log part.
     */
    Uint16 gcpWordNo[4];
    /**
     *       The identity of this global checkpoint.
     */
    UintR gcpId;
    /**
     *       The state of this global checkpoint, one for each log part.
     */
    Uint8 gcpLogPartState[4];
    /**
     *       The sync state of this global checkpoint, one for each
     *       log part.
     */
    Uint8 gcpSyncReady[4];
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
    bool inPackedList;
    UintR noOfPackedWordsLqh;
    UintR packedWordsLqh[30];
    UintR noOfPackedWordsTc;
    UintR packedWordsTc[29];
    BlockReference hostLqhBlockRef;
    BlockReference hostTcBlockRef;
  };// Size 128 bytes
  typedef Ptr<HostRecord> HostRecordPtr;
  
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
      LCP_COMPLETED = 2,
      LCP_WAIT_FRAGID = 3,
      LCP_WAIT_TUP_PREPLCP = 4,
      LCP_WAIT_HOLDOPS = 5,
      LCP_WAIT_ACTIVE_FINISH = 6,
      LCP_START_CHKP = 7,
      LCP_BLOCKED_COMP = 8,
      LCP_SR_WAIT_FRAGID = 9,
      LCP_SR_STARTED = 10,
      LCP_SR_COMPLETED = 11
    };
    Uint32 firstLcpLocAcc;
    Uint32 firstLcpLocTup;
    Uint32 lcpAccptr;
 
    LcpState lcpState;
    bool lastFragmentFlag;

    struct FragOrd {
      Uint32 fragPtrI;
      LcpFragOrd lcpFragOrd;
    };
    FragOrd currentFragment;
    
    bool   lcpQueued;
    FragOrd queuedFragment;
    
    bool   reportEmpty;
    NdbNodeBitmask m_EMPTY_LCP_REQ;
  }; // Size 76 bytes
  typedef Ptr<LcpRecord> LcpRecordPtr;
  
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
    enum WaitingBlock {
      ACC = 0,
      TUP = 1,
      NONE = 2
    };
    
    LcpLocstate lcpLocstate;
    UintR locFragid;
    UintR masterLcpRec;
    UintR nextLcpLoc;
    UintR tupRef;
    WaitingBlock waitingBlock;
    Uint32 accContCounter;
  }; // 28 bytes
  typedef Ptr<LcpLocRecord> LcpLocRecordPtr;
  
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
      SR_FOURTH_PHASE_COMPLETED = 7,
      FILE_CHANGE_PROBLEM = 8,        ///< For some reason the write to 
                                      ///< page zero in file zero have not   
                                      ///< finished after 15 mbyte of 
                                      ///< log data have been written
      TAIL_PROBLEM = 9                ///< Only 1 mbyte of log left. 
                                      ///< No operations allowed to enter the 
                                      ///< log. Only special log records 
                                      ///< are allowed
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
     *       The head of the operations queued for logging.
     */
    UintR firstLogQueue;
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
    /**
     *       The tail of the operations queued for logging.                   
     */
    UintR lastLogQueue;
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
    /**
     *       The number of files remaining to gather GCI information
     *       for during system restart.  Only used if number of files
     *       is larger than 60.
     */
    UintR srRemainingFiles;
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
    Uint8 logTailMbyte;
    /**
     *       The mbyte within the starting log file where to start 
     *       executing the log.                
     */
    Uint8 startMbyte;
    /**
     *       The last mbyte in which to execute the log during system
     *       restart.
     */
    Uint8 stopMbyte;
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
    enum FileChangeState {
      NOT_ONGOING = 0,
      BOTH_WRITES_ONGOING = 1,
      LAST_WRITE_ONGOING = 2,
      FIRST_WRITE_ONGOING = 3,
      WRITE_PAGE_ZERO_ONGOING = 4
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
      OPEN_SR_INVALIDATE_PAGES = 21,
      CLOSE_SR_INVALIDATE_PAGES = 22
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
    UintR logLastPrepRef[16];
    /**
     *       The max global checkpoint completed before the mbyte in the
     *       log file was started.  One variable per mbyte.  
     */
    UintR logMaxGciCompleted[16];
    /**
     *       The max global checkpoint started before the mbyte in the log
     *       file was started.  One variable per mbyte.
     */
    UintR logMaxGciStarted[16];
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
    UintR logFilePagesToDiskWithoutSynch;
    /**
     *       This variable keeps track of the number of pages written since
     *       last synch on this log file.
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
  }; // Size 288 bytes
  typedef Ptr<LogFileRecord> LogFileRecordPtr;
  
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
      WRITE_SR_INVALIDATE_PAGES = 19
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
      PREP_DROP_TABLE_ONGOING = 3,
      PREP_DROP_TABLE_DONE = 4
    };
    
    UintR fragrec[MAX_FRAG_PER_NODE];
    Uint16 fragid[MAX_FRAG_PER_NODE];
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

    Uint32 usageCount;
    NdbNodeBitmask waitingTC;
    NdbNodeBitmask waitingDIH;
  }; // Size 100 bytes
  typedef Ptr<Tablerec> TablerecPtr;

  struct TcConnectionrec {
    enum ListState {
      NOT_IN_LIST = 0,
      IN_ACTIVE_LIST = 1,
      ACC_BLOCK_LIST = 2,
      WAIT_QUEUE_LIST = 3
    };
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
      STOPPED = 5,
      LOG_QUEUED = 6,
      PREPARED = 7,
      LOG_COMMIT_WRITTEN_WAIT_SIGNAL = 8,
      LOG_COMMIT_QUEUED_WAIT_SIGNAL = 9,
      
      /* -------------------------------------------------------------------- */
      // Commit in progress states
      /* -------------------------------------------------------------------- */
      COMMIT_STOPPED = 10,
      LOG_COMMIT_QUEUED = 11,
      COMMIT_QUEUED = 12,
      COMMITTED = 13,
      
      /* -------------------------------------------------------------------- */
      // Abort in progress states
      /* -------------------------------------------------------------------- */
      WAIT_ACC_ABORT = 14,
      ABORT_QUEUED = 15,
      ABORT_STOPPED = 16,
      WAIT_AI_AFTER_ABORT = 17,
      LOG_ABORT_QUEUED = 18,
      WAIT_TUP_TO_ABORT = 19,
      
      /* -------------------------------------------------------------------- */
      // Scan in progress states
      /* -------------------------------------------------------------------- */
      WAIT_SCAN_AI = 20,
      SCAN_STATE_USED = 21,
      SCAN_FIRST_STOPPED = 22,
      SCAN_CHECK_STOPPED = 23,
      SCAN_STOPPED = 24,
      SCAN_RELEASE_STOPPED = 25,
      SCAN_CLOSE_STOPPED = 26,
      COPY_CLOSE_STOPPED = 27,
      COPY_FIRST_STOPPED = 28,
      COPY_STOPPED = 29,
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
    ConnectState connectState;
    UintR copyCountWords;    
    UintR firstAttrinfo[5];
    UintR tupkeyData[4];
    UintR transid[2];
    AbortState abortState;
    UintR accConnectrec;
    UintR applOprec;
    UintR clientConnectrec;
    UintR tcTimer;
    UintR currReclenAi;
    UintR currTupAiLen;
    UintR firstAttrinbuf;
    UintR firstTupkeybuf;
    UintR fragmentid;
    UintR fragmentptr;
    UintR gci;
    UintR hashValue;
    UintR lastTupkeybuf;
    UintR lastAttrinbuf;
    /**
     * Each operation (TcConnectrec) can be stored in max one out of many 
     * lists.
     * This variable keeps track of which list it is in.
     */
    ListState listState;
    
    UintR logStartFileNo;
    LogWriteState logWriteState;
    UintR nextHashRec;
    UintR nextLogTcrec;
    UintR nextTcLogQueue;
    UintR nextTc;
    UintR nextTcConnectrec;
    UintR prevHashRec;
    UintR prevLogTcrec;
    UintR prevTc;
    UintR readlenAi;
    UintR reqRef;
    UintR reqinfo;
    UintR schemaVersion;
    UintR storedProcId;
    UintR simpleTcConnect;
    UintR tableref;
    UintR tcOprec;
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
      UintR noFiredTriggers;
    };
    Uint16 errorCode;
    Uint16 logStartPageIndex;
    Uint16 logStartPageNo;
    Uint16 logStopPageNo;
    Uint16 nextReplica;
    Uint16 primKeyLen;
    Uint16 save1;
    Uint16 nodeAfterNext[3];

    Uint8 activeCreat;
    Uint8 apiVersionNo;
    Uint8 dirtyOp;
    Uint8 indTakeOver;
    Uint8 lastReplicaNo;
    Uint8 localFragptr;
    Uint8 lockType;
    Uint8 nextSeqNoReplica;
    Uint8 opSimple;
    Uint8 opExec;
    Uint8 operation;
    Uint8 reclenAiLqhkey;
    Uint8 m_offset_current_keybuf;
    Uint8 replicaType;
    Uint8 simpleRead;
    Uint8 seqNoReplica;
    Uint8 tcNodeFailrec;
  }; /* p2c: size = 280 bytes */
  
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
    Uint16 oldNodeId;
  }; // Size 28 bytes
  typedef Ptr<TcNodeFailRecord> TcNodeFailRecordPtr;

  struct CommitLogRecord {
    Uint32 startPageNo;
    Uint32 startPageIndex;
    Uint32 stopPageNo;
    Uint32 fileNo;
  };
  
public:
  Dblqh(const class Configuration &);
  virtual ~Dblqh();
  
private:
  BLOCK_DEFINES(Dblqh);

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
  void execREAD_PSUEDO_REQ(Signal* signal);
  
  void execDUMP_STATE_ORD(Signal* signal);
  void execACC_COM_BLOCK(Signal* signal);
  void execACC_COM_UNBLOCK(Signal* signal);
  void execTUP_COM_BLOCK(Signal* signal);
  void execTUP_COM_UNBLOCK(Signal* signal);
  void execACC_ABORTCONF(Signal* signal);
  void execNODE_FAILREP(Signal* signal);
  void execCHECK_LCP_STOP(Signal* signal);
  void execSEND_PACKED(Signal* signal);
  void execTUP_ATTRINFO(Signal* signal);
  void execREAD_CONFIG_REQ(Signal* signal);
  void execLQHFRAGREQ(Signal* signal);
  void execLQHADDATTREQ(Signal* signal);
  void execTUP_ADD_ATTCONF(Signal* signal);
  void execTUP_ADD_ATTRREF(Signal* signal);
  void execACCFRAGCONF(Signal* signal);
  void execACCFRAGREF(Signal* signal);
  void execTUPFRAGCONF(Signal* signal);
  void execTUPFRAGREF(Signal* signal);
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
  void execTUPKEYCONF(Signal* signal);
  void execTUPKEYREF(Signal* signal);
  void execABORT(Signal* signal);
  void execABORTREQ(Signal* signal);
  void execCOMMITREQ(Signal* signal);
  void execCOMPLETEREQ(Signal* signal);
  void execMEMCHECKREQ(Signal* signal);
  void execSCAN_FRAGREQ(Signal* signal);
  void execSCAN_NEXTREQ(Signal* signal);
  void execACC_SCANCONF(Signal* signal);
  void execACC_SCANREF(Signal* signal);
  void execNEXT_SCANCONF(Signal* signal);
  void execNEXT_SCANREF(Signal* signal);
  void execACC_TO_REF(Signal* signal);
  void execSTORED_PROCCONF(Signal* signal);
  void execSTORED_PROCREF(Signal* signal);
  void execCOPY_FRAGREQ(Signal* signal);
  void execCOPY_ACTIVEREQ(Signal* signal);
  void execCOPY_STATEREQ(Signal* signal);
  void execLQH_TRANSREQ(Signal* signal);
  void execTRANSID_AI(Signal* signal);
  void execINCL_NODEREQ(Signal* signal);
  void execACC_LCPCONF(Signal* signal);
  void execACC_LCPREF(Signal* signal);
  void execACC_LCPSTARTED(Signal* signal);
  void execACC_CONTOPCONF(Signal* signal);
  void execLCP_FRAGIDCONF(Signal* signal);
  void execLCP_FRAGIDREF(Signal* signal);
  void execLCP_HOLDOPCONF(Signal* signal);
  void execLCP_HOLDOPREF(Signal* signal);
  void execTUP_PREPLCPCONF(Signal* signal);
  void execTUP_PREPLCPREF(Signal* signal);
  void execTUP_LCPCONF(Signal* signal);
  void execTUP_LCPREF(Signal* signal);
  void execTUP_LCPSTARTED(Signal* signal);
  void execEND_LCPCONF(Signal* signal);

  void execLCP_FRAG_ORD(Signal* signal);
  void execEMPTY_LCP_REQ(Signal* signal);
  
  void execSTART_FRAGREQ(Signal* signal);
  void execSTART_RECREF(Signal* signal);
  void execSR_FRAGIDCONF(Signal* signal);
  void execSR_FRAGIDREF(Signal* signal);
  void execACC_SRCONF(Signal* signal);
  void execACC_SRREF(Signal* signal);
  void execTUP_SRCONF(Signal* signal);
  void execTUP_SRREF(Signal* signal);
  void execGCP_SAVEREQ(Signal* signal);
  void execFSOPENCONF(Signal* signal);
  void execFSCLOSECONF(Signal* signal);
  void execFSWRITECONF(Signal* signal);
  void execFSWRITEREF(Signal* signal);
  void execFSREADCONF(Signal* signal);
  void execFSREADREF(Signal* signal);
  void execSCAN_HBREP(Signal* signal);
  void execSET_VAR_REQ(Signal* signal);
  void execTIME_SIGNAL(Signal* signal);
  void execFSSYNCCONF(Signal* signal);

  void execALTER_TAB_REQ(Signal* signal);
  void execALTER_TAB_CONF(Signal* signal);

  void execCREATE_TRIG_CONF(Signal* signal);
  void execCREATE_TRIG_REF(Signal* signal);
  void execCREATE_TRIG_REQ(Signal* signal);

  void execDROP_TRIG_CONF(Signal* signal);
  void execDROP_TRIG_REF(Signal* signal);
  void execDROP_TRIG_REQ(Signal* signal);

  void execPREP_DROP_TAB_REQ(Signal* signal);
  void execWAIT_DROP_TAB_REQ(Signal* signal);
  void execDROP_TAB_REQ(Signal* signal);

  void execLQH_ALLOCREQ(Signal* signal);
  void execLQH_WRITELOG_REQ(Signal* signal);

  void execTUXFRAGCONF(Signal* signal);
  void execTUXFRAGREF(Signal* signal);
  void execTUX_ADD_ATTRCONF(Signal* signal);
  void execTUX_ADD_ATTRREF(Signal* signal);

  // Statement blocks

  void init_acc_ptr_list(ScanRecord*);
  bool seize_acc_ptr_list(ScanRecord*, Uint32);
  void release_acc_ptr_list(ScanRecord*);
  Uint32 get_acc_ptr_from_scan_record(ScanRecord*, Uint32, bool);
  void set_acc_ptr_in_scan_record(ScanRecord*, Uint32, Uint32);
  void i_get_acc_ptr(ScanRecord*, Uint32*&, Uint32);
  
  void removeTable(Uint32 tableId);
  void sendLCP_COMPLETE_REP(Signal* signal, Uint32 lcpId);
  void sendEMPTY_LCP_CONF(Signal* signal, bool idle);
  void sendLCP_FRAGIDREQ(Signal* signal);
  void sendLCP_FRAG_REP(Signal * signal, const LcpRecord::FragOrd &) const;

  void updatePackedList(Signal* signal, HostRecord * ahostptr, Uint16 hostId);
  void LQHKEY_abort(Signal* signal, int errortype);
  void LQHKEY_error(Signal* signal, int errortype);
  void nextRecordCopy(Signal* signal);
  Uint32 calculateHash(Uint32 tableId, const Uint32* src);
  void continueAfterCheckLcpStopBlocked(Signal* signal);
  void checkLcpStopBlockedLab(Signal* signal);
  void sendCommittedTc(Signal* signal, BlockReference atcBlockref);
  void sendCompletedTc(Signal* signal, BlockReference atcBlockref);
  void sendLqhkeyconfTc(Signal* signal, BlockReference atcBlockref);
  void sendCommitLqh(Signal* signal, BlockReference alqhBlockref);
  void sendCompleteLqh(Signal* signal, BlockReference alqhBlockref);
  void sendPackedSignalLqh(Signal* signal, HostRecord * ahostptr);
  void sendPackedSignalTc(Signal* signal, HostRecord * ahostptr);
  Uint32 handleLongTupKey(Signal* signal,
                          Uint32 lenSofar,
                          Uint32 primKeyLen,
                          Uint32* dataPtr);
  void cleanUp(Signal* signal);
  void sendAttrinfoLoop(Signal* signal);
  void sendAttrinfoSignal(Signal* signal);
  void sendLqhAttrinfoSignal(Signal* signal);
  void sendKeyinfoAcc(Signal* signal, Uint32 pos);
  Uint32 initScanrec(const class ScanFragReq *);
  void initScanTc(Signal* signal,
                  Uint32 transid1,
                  Uint32 transid2,
                  Uint32 fragId,
                  Uint32 nodeId);
  void finishScanrec(Signal* signal);
  void releaseScanrec(Signal* signal);
  void seizeScanrec(Signal* signal);
  Uint32 sendKeyinfo20(Signal* signal, ScanRecord *, TcConnectionrec *);
  void sendScanFragConf(Signal* signal, Uint32 scanCompleted);
  void initCopyrec(Signal* signal);
  void initCopyTc(Signal* signal);
  void sendCopyActiveConf(Signal* signal,Uint32 tableId);
  void checkLcpCompleted(Signal* signal);
  void checkLcpHoldop(Signal* signal);
  bool checkLcpStarted(Signal* signal);
  void checkLcpTupprep(Signal* signal);
  void getNextFragForLcp(Signal* signal);
  void initLcpLocAcc(Signal* signal, Uint32 fragId);
  void initLcpLocTup(Signal* signal, Uint32 fragId);
  void moveAccActiveFrag(Signal* signal);
  void moveActiveToAcc(Signal* signal);
  void releaseLocalLcps(Signal* signal);
  void seizeLcpLoc(Signal* signal);
  void sendAccContOp(Signal* signal);
  void sendStartLcp(Signal* signal);
  void setLogTail(Signal* signal, Uint32 keepGci);
  Uint32 remainingLogSize(const LogFileRecordPtr &sltCurrLogFilePtr,
			  const LogPartRecordPtr &sltLogPartPtr);
  void checkGcpCompleted(Signal* signal, Uint32 pageWritten, Uint32 wordWritten);
  void initFsopenconf(Signal* signal);
  void initFsrwconf(Signal* signal, bool write);
  void initLfo(Signal* signal);
  void initLogfile(Signal* signal, Uint32 fileNo);
  void initLogpage(Signal* signal);
  void openFileRw(Signal* signal, LogFileRecordPtr olfLogFilePtr);
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
                       Uint32 wordWritten, Uint32 place);
  void buildLinkedLogPageList(Signal* signal);
  void changeMbyte(Signal* signal);
  Uint32 checkIfExecLog(Signal* signal);
  void checkNewMbyte(Signal* signal);
  void checkReadExecSr(Signal* signal);
  void checkScanTcCompleted(Signal* signal);
  void checkSrCompleted(Signal* signal);
  void closeFile(Signal* signal, LogFileRecordPtr logFilePtr);
  void completedLogPage(Signal* signal, Uint32 clpType, Uint32 place);
  void deleteFragrec(Uint32 fragId);
  void deleteTransidHash(Signal* signal);
  void findLogfile(Signal* signal,
                   Uint32 fileNo,
                   LogPartRecordPtr flfLogPartPtr,
                   LogFileRecordPtr* parLogFilePtr);
  void findPageRef(Signal* signal, CommitLogRecord* commitLogRecord);
  int  findTransaction(UintR Transid1, UintR Transid2, UintR TcOprec);
  void getFirstInLogQueue(Signal* signal);
  bool getFragmentrec(Signal* signal, Uint32 fragId);
  void initialiseAddfragrec(Signal* signal);
  void initialiseAttrbuf(Signal* signal);
  void initialiseDatabuf(Signal* signal);
  void initialiseFragrec(Signal* signal);
  void initialiseGcprec(Signal* signal);
  void initialiseLcpRec(Signal* signal);
  void initialiseLcpLocrec(Signal* signal);
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
  void initLcpSr(Signal* signal,
                 Uint32 lcpNo,
                 Uint32 lcpId,
                 Uint32 tableId,
                 Uint32 fragId,
                 Uint32 fragPtr);
  void initLogpart(Signal* signal);
  void initLogPointers(Signal* signal);
  void initReqinfoExecSr(Signal* signal);
  bool insertFragrec(Signal* signal, Uint32 fragId);
  void linkActiveFrag(Signal* signal);
  void linkFragQueue(Signal* signal);
  void linkWaitLog(Signal* signal, LogPartRecordPtr regLogPartPtr);
  void logNextStart(Signal* signal);
  void moveToPageRef(Signal* signal);
  void readAttrinfo(Signal* signal);
  void readCommitLog(Signal* signal, CommitLogRecord* commitLogRecord);
  void readExecLog(Signal* signal);
  void readExecSrNewMbyte(Signal* signal);
  void readExecSr(Signal* signal);
  void readKey(Signal* signal);
  void readLogData(Signal* signal, Uint32 noOfWords, Uint32* dataPtr);
  void readLogHeader(Signal* signal);
  Uint32 readLogword(Signal* signal);
  Uint32 readLogwordExec(Signal* signal);
  void readSinglePage(Signal* signal, Uint32 pageNo);
  void releaseAccList(Signal* signal);
  void releaseActiveCopy(Signal* signal);
  void releaseActiveFrag(Signal* signal);
  void releaseActiveList(Signal* signal);
  void releaseAddfragrec(Signal* signal);
  void releaseFragrec();
  void releaseLcpLoc(Signal* signal);
  void releaseOprec(Signal* signal);
  void releasePageRef(Signal* signal);
  void releaseMmPages(Signal* signal);
  void releasePrPages(Signal* signal);
  void releaseTcrec(Signal* signal, TcConnectionrecPtr tcConnectptr);
  void releaseTcrecLog(Signal* signal, TcConnectionrecPtr tcConnectptr);
  void releaseWaitQueue(Signal* signal);
  void removeLogTcrec(Signal* signal);
  void removePageRef(Signal* signal);
  Uint32 returnExecLog(Signal* signal);
  int saveTupattrbuf(Signal* signal, Uint32* dataPtr, Uint32 length);
  void seizeAddfragrec(Signal* signal);
  void seizeAttrinbuf(Signal* signal);
  Uint32 seize_attrinbuf();
  Uint32 release_attrinbuf(Uint32);
  Uint32 copy_bounds(Uint32 * dst, TcConnectionrec*);

  void seizeFragmentrec(Signal* signal);
  void seizePageRef(Signal* signal);
  void seizeTcrec();
  void seizeTupkeybuf(Signal* signal);
  void sendAborted(Signal* signal);
  void sendLqhTransconf(Signal* signal, LqhTransConf::OperationStatus);
  void sendTupkey(Signal* signal);
  void startExecSr(Signal* signal);
  void startNextExecSr(Signal* signal);
  void startTimeSupervision(Signal* signal);
  void stepAhead(Signal* signal, Uint32 stepAheadWords);
  void systemError(Signal* signal);
  void writeAbortLog(Signal* signal);
  void writeCommitLog(Signal* signal, LogPartRecordPtr regLogPartPtr);
  void writeCompletedGciLog(Signal* signal);
  void writeDbgInfoPageHeader(LogPageRecordPtr logPagePtr, Uint32 place,
                              Uint32 pageNo, Uint32 wordWritten);
  void writeDirty(Signal* signal, Uint32 place);
  void writeKey(Signal* signal);
  void writeLogHeader(Signal* signal);
  void writeLogWord(Signal* signal, Uint32 data);
  void writeNextLog(Signal* signal);
  void errorReport(Signal* signal, int place);
  void warningReport(Signal* signal, int place);
  void invalidateLogAfterLastGCI(Signal *signal);
  void readFileInInvalidate(Signal *signal);
  void exitFromInvalidate(Signal* signal);
  Uint32 calcPageCheckSum(LogPageRecordPtr logP);

  // Generated statement blocks
  void systemErrorLab(Signal* signal);
  void initFourth(Signal* signal);
  void packLqhkeyreqLab(Signal* signal);
  void sendNdbSttorryLab(Signal* signal);
  void execSrCompletedLab(Signal* signal);
  void execLogRecord(Signal* signal);
  void srPhase3Comp(Signal* signal);
  void srLogLimits(Signal* signal);
  void srGciLimits(Signal* signal);
  void srPhase3Start(Signal* signal);
  void warningHandlerLab(Signal* signal);
  void checkStartCompletedLab(Signal* signal);
  void continueAbortLab(Signal* signal);
  void abortContinueAfterBlockedLab(Signal* signal, bool canBlock);
  void abortCommonLab(Signal* signal);
  void localCommitLab(Signal* signal);
  void abortErrorLab(Signal* signal);
  void continueAfterReceivingAllAiLab(Signal* signal);
  void abortStateHandlerLab(Signal* signal);
  void writeAttrinfoLab(Signal* signal);
  void scanAttrinfoLab(Signal* signal, Uint32* dataPtr, Uint32 length);
  void abort_scan(Signal* signal, Uint32 scan_ptr_i, Uint32 errcode);
  void localAbortStateHandlerLab(Signal* signal);
  void logLqhkeyreqLab(Signal* signal);
  void lqhAttrinfoLab(Signal* signal, Uint32* dataPtr, Uint32 length);
  void rwConcludedAiLab(Signal* signal);
  void aiStateErrorCheckLab(Signal* signal, Uint32* dataPtr, Uint32 length);
  void takeOverErrorLab(Signal* signal);
  void endgettupkeyLab(Signal* signal);
  void noFreeRecordLab(Signal* signal, 
		       const class LqhKeyReq * lqhKeyReq, 
		       Uint32 errorCode);
  void logLqhkeyrefLab(Signal* signal);
  void closeCopyLab(Signal* signal);
  void commitReplyLab(Signal* signal);
  void completeUnusualLab(Signal* signal);
  void completeTransNotLastLab(Signal* signal);
  void completedLab(Signal* signal);
  void copyCompletedLab(Signal* signal);
  void completeLcpRoundLab(Signal* signal);
  void continueAfterLogAbortWriteLab(Signal* signal);
  void sendAttrinfoLab(Signal* signal);
  void sendExecConf(Signal* signal);
  void execSr(Signal* signal);
  void srFourthComp(Signal* signal);
  void timeSup(Signal* signal);
  void closeCopyRequestLab(Signal* signal);
  void closeScanRequestLab(Signal* signal);
  void scanTcConnectLab(Signal* signal, Uint32 startTcCon, Uint32 fragId);
  void initGcpRecLab(Signal* signal);
  void prepareContinueAfterBlockedLab(Signal* signal);
  void commitContinueAfterBlockedLab(Signal* signal);
  void continueCopyAfterBlockedLab(Signal* signal);
  void continueFirstCopyAfterBlockedLab(Signal* signal);
  void continueFirstScanAfterBlockedLab(Signal* signal);
  void continueScanAfterBlockedLab(Signal* signal);
  void continueScanReleaseAfterBlockedLab(Signal* signal);
  void continueCloseScanAfterBlockedLab(Signal* signal);
  void continueCloseCopyAfterBlockedLab(Signal* signal);
  void sendExecFragRefLab(Signal* signal);
  void fragrefLab(Signal* signal, BlockReference retRef,
                  Uint32 retPtr, Uint32 errorCode);
  void abortAddFragOps(Signal* signal);
  void rwConcludedLab(Signal* signal);
  void sendsttorryLab(Signal* signal);
  void initialiseRecordsLab(Signal* signal, Uint32 data, Uint32, Uint32);
  void startphase2Lab(Signal* signal, Uint32 config);
  void startphase3Lab(Signal* signal);
  void startphase4Lab(Signal* signal);
  void startphase6Lab(Signal* signal);
  void moreconnectionsLab(Signal* signal);
  void scanReleaseLocksLab(Signal* signal);
  void closeScanLab(Signal* signal);
  void nextScanConfLoopLab(Signal* signal);
  void scanNextLoopLab(Signal* signal);
  void commitReqLab(Signal* signal, Uint32 gci);
  void completeTransLastLab(Signal* signal);
  void tupScanCloseConfLab(Signal* signal);
  void tupCopyCloseConfLab(Signal* signal);
  void accScanCloseConfLab(Signal* signal);
  void accCopyCloseConfLab(Signal* signal);
  void nextScanConfScanLab(Signal* signal);
  void nextScanConfCopyLab(Signal* signal);
  void continueScanNextReqLab(Signal* signal);
  void keyinfoLab(const Uint32 * src, const Uint32 * end);
  void copySendTupkeyReqLab(Signal* signal);
  void storedProcConfScanLab(Signal* signal);
  void storedProcConfCopyLab(Signal* signal);
  void copyStateFinishedLab(Signal* signal);
  void lcpCompletedLab(Signal* signal);
  void lcpStartedLab(Signal* signal);
  void contChkpNextFragLab(Signal* signal);
  void startLcpRoundLab(Signal* signal);
  void startFragRefLab(Signal* signal);
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
  void closeExecSrLab(Signal* signal);
  void execLogComp(Signal* signal);
  void closeWriteLogLab(Signal* signal);
  void closeExecLogLab(Signal* signal);
  void writePageZeroLab(Signal* signal);
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
  void copyLqhKeyRefLab(Signal* signal);
  void restartOperationsLab(Signal* signal);
  void lqhTransNextLab(Signal* signal);
  void restartOperationsAfterStopLab(Signal* signal);
  void sttorStartphase1Lab(Signal* signal);
  void startphase1Lab(Signal* signal, Uint32 config, Uint32 nodeId);
  void tupkeyConfLab(Signal* signal);
  void copyTupkeyConfLab(Signal* signal);
  void scanTupkeyConfLab(Signal* signal);
  void scanTupkeyRefLab(Signal* signal);
  void accScanConfScanLab(Signal* signal);
  void accScanConfCopyLab(Signal* signal);
  void scanLockReleasedLab(Signal* signal);
  void openSrFourthNextLab(Signal* signal);
  void closingInitLab(Signal* signal);
  void closeExecSrCompletedLab(Signal* signal);
  void readSrFrontpageLab(Signal* signal);
  
  void sendAddFragReq(Signal* signal);
  void sendAddAttrReq(Signal* signal);
  void checkDropTab(Signal*);
  Uint32 checkDropTabState(Tablerec::TableStatus, Uint32) const;
  
  // Initialisation
  void initData();
  void initRecords();

  Dbtup* c_tup;
  Uint32 readPrimaryKeys(ScanRecord*, TcConnectionrec*, Uint32 * dst);
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

#define ZATTRINBUF_FILE_SIZE 12288  // 1.5 MByte
#define ZINBUF_DATA_LEN 24            /* POSITION OF 'DATA LENGHT'-VARIABLE. */
#define ZINBUF_NEXT 25                /* POSITION OF 'NEXT'-VARIABLE.        */
  Attrbuf *attrbuf;
  AttrbufPtr attrinbufptr;
  UintR cfirstfreeAttrinbuf;
  UintR cattrinbufFileSize;
  Uint32 c_no_attrinbuf_recs;

#define ZDATABUF_FILE_SIZE 10000    // 200 kByte
  Databuf *databuf;
  DatabufPtr databufptr;
  UintR cfirstfreeDatabuf;
  UintR cdatabufFileSize;

// Configurable
  Fragrecord *fragrecord;
  FragrecordPtr fragptr;
  UintR cfirstfreeFragrec;
  UintR cfragrecFileSize;

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

#define ZLCP_LOCREC_FILE_SIZE 4
  LcpLocRecord *lcpLocRecord;
  LcpLocRecordPtr lcpLocptr;
  UintR clcpLocrecFileSize;

#define ZLOG_PART_FILE_SIZE 4
  LogPartRecord *logPartRecord;
  LogPartRecordPtr logPartPtr;
  UintR clogPartFileSize;

// Configurable
  LogFileRecord *logFileRecord;
  LogFileRecordPtr logFilePtr;
  UintR cfirstfreeLogFile;
  UintR clogFileFileSize;

#define ZLFO_FILE_SIZE 256            /* MAX 256 OUTSTANDING FILE OPERATIONS */
  LogFileOperationRecord *logFileOperationRecord;
  LogFileOperationRecordPtr lfoPtr;
  UintR cfirstfreeLfo;
  UintR clfoFileSize;

  LogPageRecord *logPageRecord;
  LogPageRecordPtr logPagePtr;
  UintR cfirstfreeLogPage;
  UintR clogPageFileSize;

#define ZPAGE_REF_FILE_SIZE 20
  PageRefRecord *pageRefRecord;
  PageRefRecordPtr pageRefPtr;
  UintR cfirstfreePageRef;
  UintR cpageRefFileSize;

#define ZSCANREC_FILE_SIZE 100
  ArrayPool<ScanRecord> c_scanRecordPool;
  ScanRecordPtr scanptr;
  UintR cscanNoFreeRec;
  Uint32 cscanrecFileSize;

// Configurable
  Tablerec *tablerec;
  TablerecPtr tabptr;
  UintR ctabrecFileSize;

// Configurable
  TcConnectionrec *tcConnectionrec;
  TcConnectionrecPtr tcConnectptr;
  UintR cfirstfreeTcConrec;
  UintR ctcConnectrecFileSize;

// MAX_NDB_NODES is the size of this array
  TcNodeFailRecord *tcNodeFailRecord;
  TcNodeFailRecordPtr tcNodeFailptr;
  UintR ctcNodeFailrecFileSize;

  Uint16 terrorCode;

  Uint32 c_firstInNodeGroup;

// ------------------------------------------------------------------------
// These variables are used to store block state which do not need arrays
// of struct's.
// ------------------------------------------------------------------------
  Uint32 c_lcpId;
  Uint32 cnoOfFragsCheckpointed;

/* ------------------------------------------------------------------------- */
// cmaxWordsAtNodeRec keeps track of how many words that currently are
// outstanding in a node recovery situation.
// cbookedAccOps keeps track of how many operation records that have been
// booked in ACC for the scan processes.
// cmaxAccOps contains the maximum number of operation records which can be
// allocated for scan purposes in ACC.
/* ------------------------------------------------------------------------- */
  UintR cmaxWordsAtNodeRec;
  UintR cbookedAccOps;
  UintR cmaxAccOps;
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
  Uint8 cstartRecReq;
/* ------------------------------------------------------------------------- */
/*THIS VARIABLE KEEPS TRACK OF HOW MANY FRAGMENTS THAT PARTICIPATE IN        */
/*EXECUTING THE LOG. IF ZERO WE DON'T NEED TO EXECUTE THE LOG AT ALL.        */
/* ------------------------------------------------------------------------- */
  UintR cnoFragmentsExecSr;
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
  UintR cfirstWaitFragSr;
/* ------------------------------------------------------------------------- */
/*THIS VARIABLE IS THE HEAD OF A LINKED LIST OF FRAGMENTS THAT HAVE BEEN     */
/*RESTORED FROM DISK THAT AWAITS EXECUTION OF THE FRAGMENT LOG.              */
/* ------------------------------------------------------------------------- */
  UintR cfirstCompletedFragSr;

  /**
   * List of fragment that the log execution is completed for
   */
  Uint32 c_redo_log_complete_frags;
  
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
/*THESE VARIABLES CONTAIN THE BLOCK REFERENCES OF THE OTHER NDB BLOCKS.      */
/*ALSO THE BLOCK REFERENCE OF MY OWN BLOCK = LQH                             */
/* ------------------------------------------------------------------------- */
  BlockReference caccBlockref;
  BlockReference ctupBlockref;
  BlockReference ctuxBlockref;
  BlockReference cownref;
  UintR cLqhTimeOutCount;
  UintR cLqhTimeOutCheckCount;
  UintR cnoOfLogPages;
  bool  caccCommitBlocked;
  bool  ctupCommitBlocked;
  bool  cCommitBlocked;
  UintR cCounterAccCommitBlocked;
  UintR cCounterTupCommitBlocked;
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
/* ------------------------------------------------------------------------- */
/*THIS VARIABLE INDICATES WHETHER A CERTAIN NODE HAS SENT ALL FRAGMENTS THAT */
/*NEED TO HAVE THE LOG EXECUTED.                                             */
/* ------------------------------------------------------------------------- */
  Uint8 cnodeSrState[MAX_NDB_NODES];
/* ------------------------------------------------------------------------- */
/*THIS VARIABLE INDICATES WHETHER A CERTAIN NODE HAVE EXECUTED THE LOG       */
/* ------------------------------------------------------------------------- */
  Uint8 cnodeExecSrState[MAX_NDB_NODES];
  UintR cnoOfNodes;

/* ------------------------------------------------------------------------- */
/* THIS VARIABLE CONTAINS THE DIRECTORY OF A HASH TABLE OF ALL ACTIVE        */
/* OPERATION IN THE BLOCK. IT IS USED TO BE ABLE TO QUICKLY ABORT AN         */
/* OPERATION WHERE THE CONNECTION WAS LOST DUE TO NODE FAILURES. IT IS       */
/* ACTUALLY USED FOR ALL ABORTS COMMANDED BY TC.                             */
/* ------------------------------------------------------------------------- */
  UintR preComputedRequestInfoMask;
  UintR ctransidHash[1024];
  
  Uint32 c_diskless;
  
public:
  /**
   *
   */
  struct CommitAckMarker {
    Uint32 transid1;
    Uint32 transid2;
    
    Uint32 apiRef;    // Api block ref
    Uint32 apiOprec;  // Connection Object in NDB API
    Uint32 tcNodeId;  
    union { Uint32 nextPool; Uint32 nextHash; };
    Uint32 prevHash;

    inline bool equal(const CommitAckMarker & p) const {
      return ((p.transid1 == transid1) && (p.transid2 == transid2));
    }
    
    inline Uint32 hashValue() const {
      return transid1;
    }
  };

  typedef Ptr<CommitAckMarker> CommitAckMarkerPtr;
  ArrayPool<CommitAckMarker>   m_commitAckMarkerPool;
  DLHashTable<CommitAckMarker> m_commitAckMarkerHash;
  typedef DLHashTable<CommitAckMarker>::Iterator CommitAckMarkerIterator;
  void execREMOVE_MARKER_ORD(Signal* signal);
  void scanMarkers(Signal* signal, Uint32 tcNodeFail, Uint32 bucket, Uint32 i);

  struct Counters {
    Uint32 operations;
    
    inline void clear(){
      operations = 0;
    }
  };

  Counters c_Counters;
  
  inline bool getAllowRead() const {
    return getNodeState().startLevel < NodeState::SL_STOPPING_3;
  }

  DLHashTable<ScanRecord> c_scanTakeOverHash;
};

inline
bool
Dblqh::ScanRecord::check_scan_batch_completed() const
{
  Uint32 max_rows = m_max_batch_size_rows;
  Uint32 max_bytes = m_max_batch_size_bytes;

  return (max_rows > 0 && (m_curr_batch_size_rows >= max_rows))  ||
    (max_bytes > 0 && (m_curr_batch_size_bytes >= max_bytes));
}

inline
void
Dblqh::i_get_acc_ptr(ScanRecord* scanP, Uint32* &acc_ptr, Uint32 index)
{
  if (index == 0) {
    acc_ptr= (Uint32*)&scanP->scan_acc_op_ptr[0];
  } else {
    Uint32 attr_buf_index, attr_buf_rec;
    
    AttrbufPtr regAttrPtr;
    jam();
    attr_buf_rec= (index + 31) / 32;
    attr_buf_index= (index - 1) & 31;
    regAttrPtr.i= scanP->scan_acc_op_ptr[attr_buf_rec];
    ptrCheckGuard(regAttrPtr, cattrinbufFileSize, attrbuf);
    acc_ptr= (Uint32*)&regAttrPtr.p->attrbuf[attr_buf_index];
  }
}

#endif

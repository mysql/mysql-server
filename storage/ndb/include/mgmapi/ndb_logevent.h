/*
   Copyright (c) 2005, 2016, Oracle and/or its affiliates. All rights reserved.

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

#ifndef NDB_LOGEVENT_H
#define NDB_LOGEVENT_H

/** @addtogroup MGM_C_API
 *  @{
 */

#include "mgmapi_config_parameters.h"

#ifdef __cplusplus
extern "C" {
#endif

  /**
   * Available log events grouped by @ref ndb_mgm_event_category
   */

  enum Ndb_logevent_type {

    NDB_LE_ILLEGAL_TYPE = -1,

    /** NDB_MGM_EVENT_CATEGORY_CONNECTION */
    NDB_LE_Connected = 0,
    /** NDB_MGM_EVENT_CATEGORY_CONNECTION */
    NDB_LE_Disconnected = 1,
    /** NDB_MGM_EVENT_CATEGORY_CONNECTION */
    NDB_LE_CommunicationClosed = 2,
    /** NDB_MGM_EVENT_CATEGORY_CONNECTION */
    NDB_LE_CommunicationOpened = 3,
    /** NDB_MGM_EVENT_CATEGORY_CONNECTION */
    NDB_LE_ConnectedApiVersion = 51,

    /** NDB_MGM_EVENT_CATEGORY_CHECKPOINT */
    NDB_LE_GlobalCheckpointStarted = 4,
    /** NDB_MGM_EVENT_CATEGORY_CHECKPOINT */
    NDB_LE_GlobalCheckpointCompleted = 5,
    /** NDB_MGM_EVENT_CATEGORY_CHECKPOINT */
    NDB_LE_LocalCheckpointStarted = 6,
    /** NDB_MGM_EVENT_CATEGORY_CHECKPOINT */
    NDB_LE_LocalCheckpointCompleted = 7,
    /** NDB_MGM_EVENT_CATEGORY_CHECKPOINT */
    NDB_LE_LCPStoppedInCalcKeepGci = 8,
    /** NDB_MGM_EVENT_CATEGORY_CHECKPOINT */
    NDB_LE_LCPFragmentCompleted = 9,

    /** NDB_MGM_EVENT_CATEGORY_STARTUP */
    NDB_LE_NDBStartStarted = 10,
    /** NDB_MGM_EVENT_CATEGORY_STARTUP */
    NDB_LE_NDBStartCompleted = 11,
    /** NDB_MGM_EVENT_CATEGORY_STARTUP */
    NDB_LE_STTORRYRecieved = 12,
    /** NDB_MGM_EVENT_CATEGORY_STARTUP */
    NDB_LE_StartPhaseCompleted = 13,
    /** NDB_MGM_EVENT_CATEGORY_STARTUP */
    NDB_LE_CM_REGCONF = 14,
    /** NDB_MGM_EVENT_CATEGORY_STARTUP */
    NDB_LE_CM_REGREF = 15,
    /** NDB_MGM_EVENT_CATEGORY_STARTUP */
    NDB_LE_FIND_NEIGHBOURS = 16,
    /** NDB_MGM_EVENT_CATEGORY_STARTUP */
    NDB_LE_NDBStopStarted = 17,
    /** NDB_MGM_EVENT_CATEGORY_STARTUP */
    NDB_LE_NDBStopCompleted = 53,
    /** NDB_MGM_EVENT_CATEGORY_STARTUP */
    NDB_LE_NDBStopForced = 59,
    /** NDB_MGM_EVENT_CATEGORY_STARTUP */
    NDB_LE_NDBStopAborted = 18,
    /** NDB_MGM_EVENT_CATEGORY_STARTUP */
    NDB_LE_LCPRestored = 86,
    /** NDB_MGM_EVENT_CATEGORY_STARTUP */
    NDB_LE_StartREDOLog = 19,
    /** NDB_MGM_EVENT_CATEGORY_STARTUP */
    NDB_LE_StartLog = 20,
    /** NDB_MGM_EVENT_CATEGORY_STARTUP */
    NDB_LE_UNDORecordsExecuted = 21,

    /** NDB_MGM_EVENT_CATEGORY_NODE_RESTART */
    NDB_LE_NR_CopyDict = 22,
    /** NDB_MGM_EVENT_CATEGORY_NODE_RESTART */
    NDB_LE_NR_CopyDistr = 23,
    /** NDB_MGM_EVENT_CATEGORY_NODE_RESTART */
    NDB_LE_NR_CopyFragsStarted = 24,
    /** NDB_MGM_EVENT_CATEGORY_NODE_RESTART */
    NDB_LE_NR_CopyFragDone = 25,
    /** NDB_MGM_EVENT_CATEGORY_NODE_RESTART */
    NDB_LE_NR_CopyFragsCompleted = 26,

    /* NODEFAIL */
    /** NDB_MGM_EVENT_CATEGORY_NODE_RESTART */
    NDB_LE_NodeFailCompleted = 27,
    /** NDB_MGM_EVENT_CATEGORY_NODE_RESTART */
    NDB_LE_NODE_FAILREP = 28,
    /** NDB_MGM_EVENT_CATEGORY_NODE_RESTART */
    NDB_LE_ArbitState = 29,
    /** NDB_MGM_EVENT_CATEGORY_NODE_RESTART */
    NDB_LE_ArbitResult = 30,
    /** NDB_MGM_EVENT_CATEGORY_NODE_RESTART */
    NDB_LE_GCP_TakeoverStarted = 31,
    /** NDB_MGM_EVENT_CATEGORY_NODE_RESTART */
    NDB_LE_GCP_TakeoverCompleted = 32,
    /** NDB_MGM_EVENT_CATEGORY_NODE_RESTART */
    NDB_LE_LCP_TakeoverStarted = 33,
    /** NDB_MGM_EVENT_CATEGORY_NODE_RESTART */
    NDB_LE_LCP_TakeoverCompleted = 34,
    /** NDB_MGM_EVENT_CATEGORY_NODE_RESTART */
    NDB_LE_ConnectCheckStarted = 82,
    /** NDB_MGM_EVENT_CATEGORY_NODE_RESTART */
    NDB_LE_ConnectCheckCompleted = 83,
    /** NDB_MGM_EVENT_CATEGORY_NODE_RESTART */
    NDB_LE_NodeFailRejected = 84,

    /** NDB_MGM_EVENT_CATEGORY_STATISTIC */
    NDB_LE_TransReportCounters = 35,
    /** NDB_MGM_EVENT_CATEGORY_STATISTIC */
    NDB_LE_OperationReportCounters = 36,
    /** NDB_MGM_EVENT_CATEGORY_STATISTIC */
    NDB_LE_TableCreated = 37,
    /** NDB_MGM_EVENT_CATEGORY_STATISTIC */
    NDB_LE_UndoLogBlocked = 38,
    /** NDB_MGM_EVENT_CATEGORY_STATISTIC */
    NDB_LE_JobStatistic = 39,
    /** NDB_MGM_EVENT_CATEGORY_STATISTIC */
    NDB_LE_SendBytesStatistic = 40,
    /** NDB_MGM_EVENT_CATEGORY_STATISTIC */
    NDB_LE_ReceiveBytesStatistic = 41,
    /** NDB_MGM_EVENT_CATEGORY_STATISTIC */
    NDB_LE_MemoryUsage = 50,
    /** NDB_MGM_EVENT_CATEGORY_STATISTIC */
    NDB_LE_ThreadConfigLoop = 68,

    /** NDB_MGM_EVENT_CATEGORY_ERROR */
    NDB_LE_TransporterError = 42,
    /** NDB_MGM_EVENT_CATEGORY_ERROR */
    NDB_LE_TransporterWarning = 43,
    /** NDB_MGM_EVENT_CATEGORY_ERROR */
    NDB_LE_MissedHeartbeat = 44,
    /** NDB_MGM_EVENT_CATEGORY_ERROR */
    NDB_LE_DeadDueToHeartbeat = 45,
    /** NDB_MGM_EVENT_CATEGORY_ERROR */
    NDB_LE_WarningEvent = 46,

    /** NDB_MGM_EVENT_CATEGORY_INFO */
    NDB_LE_SentHeartbeat = 47,
    /** NDB_MGM_EVENT_CATEGORY_INFO */
    NDB_LE_CreateLogBytes = 48,
    /** NDB_MGM_EVENT_CATEGORY_INFO */
    NDB_LE_InfoEvent = 49,

    /* 50 used */
    /* 51 used */

    /* SINGLE USER */
    NDB_LE_SingleUser = 52,
    /* 53 used */

    /** NDB_MGM_EVENT_CATEGORY_BACKUP */
    NDB_LE_BackupStarted = 54,
    /** NDB_MGM_EVENT_CATEGORY_BACKUP */
    NDB_LE_BackupFailedToStart = 55,
    /** NDB_MGM_EVENT_CATEGORY_BACKUP */
    NDB_LE_BackupStatus = 62,
    /** NDB_MGM_EVENT_CATEGORY_BACKUP */
    NDB_LE_BackupCompleted = 56,
    /** NDB_MGM_EVENT_CATEGORY_BACKUP */
    NDB_LE_BackupAborted = 57,
    /** NDB_MGM_EVENT_CATEGORY_BACKUP */
    NDB_LE_RestoreMetaData = 63,
    /** NDB_MGM_EVENT_CATEGORY_BACKUP */
    NDB_LE_RestoreData = 64,
    /** NDB_MGM_EVENT_CATEGORY_BACKUP */
    NDB_LE_RestoreLog = 65,
    /** NDB_MGM_EVENT_CATEGORY_BACKUP */
    NDB_LE_RestoreStarted = 66,
    /** NDB_MGM_EVENT_CATEGORY_BACKUP */
    NDB_LE_RestoreCompleted = 67,

    /** NDB_MGM_EVENT_CATEGORY_INFO */
    NDB_LE_EventBufferStatus = 58,

    /* 59 used */

    /** NDB_MGM_EVENT_CATEGORY_STARTUP */
    NDB_LE_StartReport = 60,

    /* 61 (used in upcoming patch) */
    /* 62-72 used */
    /** NDB_MGM_EVENT_SEVERITY_WARNING */
    NDB_LE_SubscriptionStatus = 69,

    NDB_LE_MTSignalStatistics = 70,

    /** NDB_MGM_EVENT_CATEGORY_FRAGLOGFILE */
    NDB_LE_LogFileInitStatus = 71,
    /** NDB_MGM_EVENT_CATEGORY_FRAGLOGFILE */
    NDB_LE_LogFileInitCompStatus = 72

    ,NDB_LE_RedoStatus = 73
    ,NDB_LE_CreateSchemaObject = 74
    ,NDB_LE_AlterSchemaObject = 75
    ,NDB_LE_DropSchemaObject = 76
    ,NDB_LE_StartReadLCP = 77
    ,NDB_LE_ReadLCPComplete = 78
    ,NDB_LE_RunRedo = 79
    ,NDB_LE_RebuildIndex = 80
    ,NDB_LE_SavedEvent = 81
    /* 82-84 used */

    /** NDB_MGM_EVENT_CATEGORY_INFO */
    /** NDB_LE_EventBufferStatus2 is an extension of
     * NDB_LE_EventBufferStatus with new fields added,
     * as well as the report text is improved.
     * Though it would work to add more fields to NDB_LE_EventBufferStatus
     * since the Data[] struct can accomodate 29 fields, it is cleaner
     * to introduce a new struct.
     * During an upgrade with incorrect order-
     * old mgmd with newer other components :
     * - extending NDB_LE_EventBufferStatus: new event consumer will
     *   get the error NDB_LEH_UNKNOWN_EVENT_VARIABLE when requesting
     *   new fields
     * - introducing NDB_LE_EventBufferStatus2: the event buffer status
     *   report will not be produced.
     */
    ,NDB_LE_EventBufferStatus2 = 85
  };

  /**
   *   Log event severities (used to filter the cluster log, 
   *   ndb_mgm_set_clusterlog_severity_filter(), and filter listening to events
   *   ndb_mgm_listen_event())
   */
  enum ndb_mgm_event_severity {
    NDB_MGM_ILLEGAL_EVENT_SEVERITY = -1,
    /*  Must be a nonnegative integer (used for array indexing) */
    /** Cluster log on */
    NDB_MGM_EVENT_SEVERITY_ON    = 0,
    /** Used in NDB Cluster developement */
    NDB_MGM_EVENT_SEVERITY_DEBUG = 1,
    /** Informational messages*/
    NDB_MGM_EVENT_SEVERITY_INFO = 2,
    /** Conditions that are not error condition, but might require handling.
     */
    NDB_MGM_EVENT_SEVERITY_WARNING = 3,
    /** Conditions that, while not fatal, should be corrected. */
    NDB_MGM_EVENT_SEVERITY_ERROR = 4,
    /** Critical conditions, like device errors or out of resources */
    NDB_MGM_EVENT_SEVERITY_CRITICAL = 5,
    /** A condition that should be corrected immediately,
     *  such as a corrupted system
     */
    NDB_MGM_EVENT_SEVERITY_ALERT = 6,
    /* must be next number, works as bound in loop */
    /** All severities */
    NDB_MGM_EVENT_SEVERITY_ALL = 7
  };

  /**
   *  Log event categories, used to set filter level on the log events using
   *  ndb_mgm_set_clusterlog_loglevel() and ndb_mgm_listen_event()
   */
  enum ndb_mgm_event_category {
    /**
     * Invalid log event category
     */
    NDB_MGM_ILLEGAL_EVENT_CATEGORY = -1,
    /**
     * Log events during all kinds of startups
     */
    NDB_MGM_EVENT_CATEGORY_STARTUP = CFG_LOGLEVEL_STARTUP,
    /**
     * Log events during shutdown
     */
    NDB_MGM_EVENT_CATEGORY_SHUTDOWN = CFG_LOGLEVEL_SHUTDOWN,
    /**
     * Statistics log events
     */
    NDB_MGM_EVENT_CATEGORY_STATISTIC = CFG_LOGLEVEL_STATISTICS,
    /**
     * Log events related to checkpoints
     */
    NDB_MGM_EVENT_CATEGORY_CHECKPOINT = CFG_LOGLEVEL_CHECKPOINT,
    /**
     * Log events during node restart
     */
    NDB_MGM_EVENT_CATEGORY_NODE_RESTART = CFG_LOGLEVEL_NODERESTART,
    /**
     * Log events related to connections between cluster nodes
     */
    NDB_MGM_EVENT_CATEGORY_CONNECTION = CFG_LOGLEVEL_CONNECTION,
    /**
     * Backup related log events
     */
    NDB_MGM_EVENT_CATEGORY_BACKUP = CFG_LOGLEVEL_BACKUP,
    /**
     * Congestion related log events
     */
    NDB_MGM_EVENT_CATEGORY_CONGESTION = CFG_LOGLEVEL_CONGESTION,
#ifndef DOXYGEN_SHOULD_SKIP_INTERNAL
    /**
     * Loglevel debug
     */
    NDB_MGM_EVENT_CATEGORY_DEBUG = CFG_LOGLEVEL_DEBUG,
#endif
    /**
     * Uncategorized log events (severity info)
     */
    NDB_MGM_EVENT_CATEGORY_INFO = CFG_LOGLEVEL_INFO,
    /**
     * Uncategorized log events (severity warning or higher)
     */
    NDB_MGM_EVENT_CATEGORY_ERROR = CFG_LOGLEVEL_ERROR,

    NDB_MGM_EVENT_CATEGORY_SCHEMA = CFG_LOGLEVEL_SCHEMA,

#ifndef DOXYGEN_SHOULD_SKIP_INTERNAL
    NDB_MGM_MIN_EVENT_CATEGORY = CFG_MIN_LOGLEVEL,
    NDB_MGM_MAX_EVENT_CATEGORY = CFG_MAX_LOGLEVEL
#endif
  };

  struct ndb_logevent_Connected {
    unsigned node;
  };
  
  struct ndb_logevent_Disconnected {
    unsigned node;
  };

  struct ndb_logevent_CommunicationClosed {
    unsigned node;
  };

  struct ndb_logevent_CommunicationOpened {
    unsigned node;
  };
  
  struct ndb_logevent_ConnectedApiVersion {
    unsigned node;
    unsigned version;
  };

  /* CHECKPOINT */
  struct ndb_logevent_GlobalCheckpointStarted {
    unsigned gci;
  };
  struct ndb_logevent_GlobalCheckpointCompleted {
    unsigned gci;
  };
  struct ndb_logevent_LocalCheckpointStarted {
    unsigned lci;
    unsigned keep_gci;
    unsigned restore_gci;
  };
  struct ndb_logevent_LocalCheckpointCompleted {
    unsigned lci;
  };
  struct ndb_logevent_LCPStoppedInCalcKeepGci {
    unsigned data;
  };
  struct ndb_logevent_LCPFragmentCompleted {
    unsigned node;
    unsigned table_id;
    unsigned fragment_id;
  };
  struct ndb_logevent_UndoLogBlocked {
    unsigned acc_count;
    unsigned tup_count;
  };

  /* STARTUP */
  struct ndb_logevent_NDBStartStarted {
    unsigned version;
  };
  struct ndb_logevent_NDBStartCompleted {
    unsigned version;
  };
  struct ndb_logevent_STTORRYRecieved {
    unsigned unused;
  };
  struct ndb_logevent_StartPhaseCompleted {
    unsigned phase;
    unsigned starttype;
  };
  struct ndb_logevent_CM_REGCONF {
    unsigned own_id;
    unsigned president_id;
    unsigned dynamic_id;
  };
  struct ndb_logevent_CM_REGREF {
    unsigned own_id;
    unsigned other_id;
    unsigned cause;
  };
  struct ndb_logevent_FIND_NEIGHBOURS {
    unsigned own_id;
    unsigned left_id;
    unsigned right_id;
    unsigned dynamic_id;
  };
  struct ndb_logevent_NDBStopStarted {
    unsigned stoptype;
  };
  struct ndb_logevent_NDBStopCompleted {
    unsigned action;
    unsigned signum;
  };
  struct ndb_logevent_NDBStopForced {
    unsigned action;
    unsigned signum;
    unsigned error;
    unsigned sphase;
    unsigned extra;
  };
  struct ndb_logevent_NDBStopAborted {
    unsigned _unused;
  };
  struct ndb_logevent_LCPRestored {
    unsigned restored_lcp_id;
  };
  struct ndb_logevent_StartREDOLog {
    unsigned node;
    unsigned keep_gci;
    unsigned completed_gci;
    unsigned restorable_gci;
  };
  struct ndb_logevent_StartLog {
    unsigned log_part;
    unsigned start_mb;
    unsigned stop_mb;
    unsigned gci;
  };
  struct ndb_logevent_UNDORecordsExecuted {
    unsigned block;
    unsigned data1;
    unsigned data2;
    unsigned data3;
    unsigned data4;
    unsigned data5;
    unsigned data6;
    unsigned data7;
    unsigned data8;
    unsigned data9;
    unsigned data10;
  };
  
  /* NODERESTART */
  struct ndb_logevent_NR_CopyDict {
    unsigned _unused;
  };
  struct ndb_logevent_NR_CopyDistr {
    unsigned _unused;
  };
  struct ndb_logevent_NR_CopyFragsStarted {
    unsigned dest_node;
  };
  struct ndb_logevent_NR_CopyFragDone {
    unsigned dest_node;
    unsigned table_id;
    unsigned fragment_id;
  };
  struct ndb_logevent_NR_CopyFragsCompleted {
    unsigned dest_node;
  };

  struct ndb_logevent_NodeFailCompleted {
    unsigned block; /* 0 = all */
    unsigned failed_node;
    unsigned completing_node; /* 0 = all */
  };
  struct ndb_logevent_NODE_FAILREP {
    unsigned failed_node;
    unsigned failure_state;
  };
  struct ndb_logevent_ArbitState {
    unsigned code;                /* code & state << 16 */
    unsigned arbit_node;
    unsigned ticket_0;
    unsigned ticket_1;
    /* TODO */
  };
  struct ndb_logevent_ArbitResult {
    unsigned code;                /* code & state << 16 */
    unsigned arbit_node;
    unsigned ticket_0;
    unsigned ticket_1;
    /* TODO */
  };
  struct ndb_logevent_GCP_TakeoverStarted {
    unsigned _unused;
  };
  struct ndb_logevent_GCP_TakeoverCompleted {
    unsigned _unused;
  };
  struct ndb_logevent_LCP_TakeoverStarted {
    unsigned _unused;
  };
  struct ndb_logevent_LCP_TakeoverCompleted {
    unsigned state;
  };
  struct ndb_logevent_ConnectCheckStarted {
    unsigned other_node_count;
    unsigned reason;
    unsigned causing_node;
  };
  struct ndb_logevent_ConnectCheckCompleted {
    unsigned nodes_checked;
    unsigned nodes_suspect;
    unsigned nodes_failed;
  };
  struct ndb_logevent_NodeFailRejected {
    unsigned reason;
    unsigned failed_node;
    unsigned source_node;
  };

  struct ndb_logevent_EventBufferStatus2 {
    unsigned usage;
    unsigned alloc;
    unsigned max;
    unsigned latest_consumed_epoch_l;
    unsigned latest_consumed_epoch_h;
    unsigned latest_buffered_epoch_l;
    unsigned latest_buffered_epoch_h;
    unsigned ndb_reference;
    unsigned report_reason;
  };

  /* STATISTIC */
  struct ndb_logevent_TransReportCounters {
    unsigned trans_count;
    unsigned commit_count;
    unsigned read_count;
    unsigned simple_read_count;
    unsigned write_count;
    unsigned attrinfo_count;
    unsigned conc_op_count;
    unsigned abort_count;
    unsigned scan_count;
    unsigned range_scan_count;
  };
  struct ndb_logevent_OperationReportCounters {
    unsigned ops;
  };
  struct ndb_logevent_TableCreated {
    unsigned table_id;
  };
  struct ndb_logevent_JobStatistic {
    unsigned mean_loop_count;
  };
  struct ndb_logevent_SendBytesStatistic {
    unsigned to_node;
    unsigned mean_sent_bytes;
  };
  struct ndb_logevent_ReceiveBytesStatistic {
    unsigned from_node;
    unsigned mean_received_bytes;
  };
  struct ndb_logevent_MemoryUsage {
    int      gth;
    /* union is for compatibility backward.
     * page_size_kb member variable should be removed in the future
     */
    union {
      unsigned page_size_kb;
      unsigned page_size_bytes;
    };
    unsigned pages_used;
    unsigned pages_total;
    unsigned block;
  };

  /* ERROR */
  struct ndb_logevent_TransporterError {
    unsigned to_node;
    unsigned code;
  };
  struct ndb_logevent_TransporterWarning {
    unsigned to_node;
    unsigned code;
  };
  struct ndb_logevent_MissedHeartbeat {
    unsigned node;
    unsigned count;
  };
  struct ndb_logevent_DeadDueToHeartbeat {
    unsigned node;
  };
  struct ndb_logevent_WarningEvent {
    /* TODO */
    unsigned _unused;
  };

  /* INFO */
  struct ndb_logevent_SentHeartbeat {
    unsigned node;
  };
  struct ndb_logevent_CreateLogBytes {
    unsigned node;
  };
  struct ndb_logevent_InfoEvent {
    /* TODO */
    unsigned _unused;
  };
  struct ndb_logevent_EventBufferStatus {
    unsigned usage;
    unsigned alloc;
    unsigned max;
    unsigned apply_gci_l;
    unsigned apply_gci_h;
    unsigned latest_gci_l;
    unsigned latest_gci_h;
  };

  /** Log event data for @ref NDB_LE_BackupStarted */
  struct ndb_logevent_BackupStarted {
    unsigned starting_node;
    unsigned backup_id;
  };
  /** Log event data @ref NDB_LE_BackupFailedToStart */
  struct ndb_logevent_BackupFailedToStart {
    unsigned starting_node;
    unsigned error;
  };
  /** Log event data @ref NDB_LE_BackupCompleted */
  struct ndb_logevent_BackupCompleted {
    unsigned starting_node;
    unsigned backup_id; 
    unsigned start_gci;
    unsigned stop_gci;
    unsigned n_records; 
    unsigned n_log_records;
    unsigned n_bytes;
    unsigned n_log_bytes;
    unsigned n_records_hi;
    unsigned n_log_records_hi;
    unsigned n_bytes_hi;
    unsigned n_log_bytes_hi;
  };
  /** Log event data @ref NDB_LE_BackupStatus */
  struct ndb_logevent_BackupStatus {
    unsigned starting_node;
    unsigned backup_id; 
    unsigned n_records_lo; 
    unsigned n_records_hi; 
    unsigned n_log_records_lo;
    unsigned n_log_records_hi;
    unsigned n_bytes_lo;
    unsigned n_bytes_hi;
    unsigned n_log_bytes_lo;
    unsigned n_log_bytes_hi;
  };

  /** Log event data @ref NDB_LE_BackupAborted */
  struct ndb_logevent_BackupAborted {
    unsigned starting_node;
    unsigned backup_id;
    unsigned error;
  };

  /** Log event data @ref NDB_LE_RestoreStarted */
  struct ndb_logevent_RestoreStarted {
    unsigned backup_id;
    unsigned node_id;
  };
  /** Log event data @ref NDB_LE_RestoreMetaData */
  struct ndb_logevent_RestoreMetaData {
    unsigned backup_id;
    unsigned node_id;
    unsigned n_tables;
    unsigned n_tablespaces;
    unsigned n_logfilegroups;
    unsigned n_datafiles;
    unsigned n_undofiles;
  };
  /** Log event data @ref NDB_LE_RestoreData */
  struct ndb_logevent_RestoreData {
    unsigned backup_id;
    unsigned node_id;
    unsigned n_records_lo;
    unsigned n_records_hi;
    unsigned n_bytes_lo;
    unsigned n_bytes_hi;
  };
  /** Log event data @ref NDB_LE_RestoreLog */
  struct ndb_logevent_RestoreLog {
    unsigned backup_id;
    unsigned node_id;
    unsigned n_records_lo;
    unsigned n_records_hi;
    unsigned n_bytes_lo;
    unsigned n_bytes_hi;
  };
  /** Log event data @ref NDB_LE_RestoreCompleted */
  struct ndb_logevent_RestoreCompleted {
    unsigned backup_id;
    unsigned node_id;
  };

  /** Log event data @ref NDB_LE_SingleUser */
  struct ndb_logevent_SingleUser {
    unsigned type;
    unsigned node_id;
  };
  /** Log even data @ref NDB_LE_StartReport */
  struct ndb_logevent_StartReport {
    unsigned report_type;
    unsigned remaining_time;
    unsigned bitmask_size;
    unsigned bitmask_data[1];
  };

  /** Log event data @ref NDB_LE_SubscriptionStatus */
  struct ndb_logevent_SubscriptionStatus {
    unsigned report_type;
    unsigned node_id;
  };
  
  /** Log event data @ref NDB_LE_RedoStatus */
  struct ndb_logevent_RedoStatus {
    unsigned log_part;
    unsigned head_file_no;
    unsigned head_mbyte;
    unsigned tail_file_no;
    unsigned tail_mbyte;
    unsigned total_hi;
    unsigned total_lo;
    unsigned free_hi;
    unsigned free_lo;
    unsigned no_logfiles;
    unsigned logfilesize;
  };

  /** Log event data @ref NDB_LE_LogFileInitStatus */
  struct ndb_logevent_LogFileInitStatus {
    unsigned node_id;
    unsigned total_files;
    unsigned file_done;
    unsigned total_mbytes;
    unsigned mbytes_done;
  };

  /** Log event data @ref NDB_LE_MTSignalStatistic */
  struct ndb_logevent_MTSignalStatistics {
    unsigned thr_no;
    unsigned prioa_count;
    unsigned prioa_size;
    unsigned priob_count;
    unsigned priob_size;
  };

  struct ndb_logevent_CreateSchemaObject {
    unsigned objectid;
    unsigned version;
    unsigned type;
    unsigned node; /* Node create object */
  };

  struct ndb_logevent_AlterSchemaObject {
    unsigned objectid;
    unsigned version;
    unsigned type;
    unsigned node; /* Node create object */
  };

  struct ndb_logevent_DropSchemaObject {
    unsigned objectid;
    unsigned version;
    unsigned type;
    unsigned node; /* Node create object */
  };

  struct ndb_logevent_StartReadLCP {
    unsigned tableid;
    unsigned fragmentid;
  };

  struct ndb_logevent_ReadLCPComplete {
    unsigned tableid;
    unsigned fragmentid;
    unsigned rows_hi;
    unsigned rows_lo;
  };

  struct ndb_logevent_RunRedo {
    unsigned logpart;
    unsigned phase;
    unsigned startgci;
    unsigned currgci;
    unsigned stopgci;
    unsigned startfile;
    unsigned startmb;
    unsigned currfile;
    unsigned currmb;
    unsigned stopfile;
    unsigned stopmb;
  };

  struct ndb_logevent_RebuildIndex {
    unsigned instance;
    unsigned indexid;
  };

  struct ndb_logevent_SavedEvent {
    unsigned len;
    unsigned seq;
    unsigned time;
    unsigned data[1];
  };

  /**
   * Structure to store and retrieve log event information.
   * @see @ref secSLogEvents
   */
  struct ndb_logevent {
    /** NdbLogEventHandle (to be used for comparing only)
     *  set in ndb_logevent_get_next()
     */
    void *handle;

    /** Which event */
    enum Ndb_logevent_type type;

    /** Time when log event was registred at the management server */
    unsigned time;

    /** Category of log event */
    enum ndb_mgm_event_category category;

    /** Severity of log event */
    enum ndb_mgm_event_severity severity;

    /** Level (0-15) of log event */
    unsigned level;

    /** Node ID of the node that reported the log event */
    unsigned source_nodeid;

    /** Union of log event specific data. Use @ref type to decide
     *  which struct to use
     */
    union {
      /* CONNECT */
      struct ndb_logevent_Connected Connected;
      struct ndb_logevent_Disconnected Disconnected;
      struct ndb_logevent_CommunicationClosed CommunicationClosed;
      struct ndb_logevent_CommunicationOpened CommunicationOpened;
      struct ndb_logevent_ConnectedApiVersion ConnectedApiVersion;

      /* CHECKPOINT */
      struct ndb_logevent_GlobalCheckpointStarted GlobalCheckpointStarted;
      struct ndb_logevent_GlobalCheckpointCompleted GlobalCheckpointCompleted;
      struct ndb_logevent_LocalCheckpointStarted LocalCheckpointStarted;
      struct ndb_logevent_LocalCheckpointCompleted LocalCheckpointCompleted;
      struct ndb_logevent_LCPStoppedInCalcKeepGci LCPStoppedInCalcKeepGci;
      struct ndb_logevent_LCPFragmentCompleted LCPFragmentCompleted;
      struct ndb_logevent_UndoLogBlocked UndoLogBlocked;

      /* STARTUP */
      struct ndb_logevent_NDBStartStarted NDBStartStarted;
      struct ndb_logevent_NDBStartCompleted NDBStartCompleted;
      struct ndb_logevent_STTORRYRecieved STTORRYRecieved;
      struct ndb_logevent_StartPhaseCompleted StartPhaseCompleted;
      struct ndb_logevent_CM_REGCONF CM_REGCONF;
      struct ndb_logevent_CM_REGREF CM_REGREF;
      struct ndb_logevent_FIND_NEIGHBOURS FIND_NEIGHBOURS;
      struct ndb_logevent_NDBStopStarted NDBStopStarted;
      struct ndb_logevent_NDBStopCompleted NDBStopCompleted;
      struct ndb_logevent_NDBStopForced NDBStopForced;
      struct ndb_logevent_NDBStopAborted NDBStopAborted;
      struct ndb_logevent_LCPRestored LCPRestored;
      struct ndb_logevent_StartREDOLog StartREDOLog;
      struct ndb_logevent_StartLog StartLog;
      struct ndb_logevent_UNDORecordsExecuted UNDORecordsExecuted;
      /* NODERESTART */
      struct ndb_logevent_NR_CopyDict NR_CopyDict;
      struct ndb_logevent_NR_CopyDistr NR_CopyDistr;
      struct ndb_logevent_NR_CopyFragsStarted NR_CopyFragsStarted;
      struct ndb_logevent_NR_CopyFragDone NR_CopyFragDone;
      struct ndb_logevent_NR_CopyFragsCompleted NR_CopyFragsCompleted;
      struct ndb_logevent_NodeFailCompleted NodeFailCompleted;
      struct ndb_logevent_NODE_FAILREP NODE_FAILREP;
      struct ndb_logevent_ArbitState ArbitState;
      struct ndb_logevent_ArbitResult ArbitResult;
      struct ndb_logevent_GCP_TakeoverStarted GCP_TakeoverStarted;
      struct ndb_logevent_GCP_TakeoverCompleted GCP_TakeoverCompleted;
      struct ndb_logevent_LCP_TakeoverStarted LCP_TakeoverStarted;
      struct ndb_logevent_LCP_TakeoverCompleted LCP_TakeoverCompleted;
      struct ndb_logevent_ConnectCheckStarted ConnectCheckStarted;
      struct ndb_logevent_ConnectCheckCompleted ConnectCheckCompleted;
      struct ndb_logevent_NodeFailRejected NodeFailRejected;

      /* STATISTIC */
      struct ndb_logevent_TransReportCounters TransReportCounters;
      struct ndb_logevent_OperationReportCounters OperationReportCounters;
      struct ndb_logevent_TableCreated TableCreated;
      struct ndb_logevent_JobStatistic JobStatistic;
      struct ndb_logevent_SendBytesStatistic SendBytesStatistic;
      struct ndb_logevent_ReceiveBytesStatistic ReceiveBytesStatistic;
      struct ndb_logevent_MemoryUsage MemoryUsage;

      /* ERROR */
      struct ndb_logevent_TransporterError TransporterError;
      struct ndb_logevent_TransporterWarning TransporterWarning;
      struct ndb_logevent_MissedHeartbeat MissedHeartbeat;
      struct ndb_logevent_DeadDueToHeartbeat DeadDueToHeartbeat;
      struct ndb_logevent_WarningEvent WarningEvent;

      /* INFO */
      struct ndb_logevent_SentHeartbeat SentHeartbeat;
      struct ndb_logevent_CreateLogBytes CreateLogBytes;
      struct ndb_logevent_InfoEvent InfoEvent;
      struct ndb_logevent_EventBufferStatus EventBufferStatus;
      struct ndb_logevent_SavedEvent SavedEvent;
      struct ndb_logevent_EventBufferStatus2 EventBufferStatus2;

      /** Log event data for @ref NDB_LE_BackupStarted */
      struct ndb_logevent_BackupStarted BackupStarted;
      /** Log event data @ref NDB_LE_BackupFailedToStart */
      struct ndb_logevent_BackupFailedToStart BackupFailedToStart;
      /** Log event data @ref NDB_LE_BackupCompleted */
      struct ndb_logevent_BackupCompleted BackupCompleted;
      /** Log event data @ref NDB_LE_BackupStatus */
      struct ndb_logevent_BackupStatus BackupStatus;
      /** Log event data @ref NDB_LE_BackupAborted */
      struct ndb_logevent_BackupAborted BackupAborted;
      /** Log event data @ref NDB_LE_RestoreStarted */
      struct ndb_logevent_RestoreStarted RestoreStarted;
      /** Log event data @ref NDB_LE_RestoreMetaData */
      struct ndb_logevent_RestoreMetaData RestoreMetaData;
      /** Log event data @ref NDB_LE_RestoreData */
      struct ndb_logevent_RestoreData RestoreData;
      /** Log event data @ref NDB_LE_RestoreLog */
      struct ndb_logevent_RestoreLog RestoreLog;
      /** Log event data @ref NDB_LE_RestoreCompleted */
      struct ndb_logevent_RestoreCompleted RestoreCompleted;
      /** Log event data @ref NDB_LE_LogFileInitStatus */
      struct ndb_logevent_LogFileInitStatus LogFileInitStatus;
      /** Log event data @ref NDB_LE_SingleUser */
      struct ndb_logevent_SingleUser SingleUser;
      /** Log event data @ref NDB_LE_MTSignalStatistic */
      struct ndb_logevent_MTSignalStatistics MTSignalStatistics;
      /** Log event data @ref NDB_LE_StartReport */
      struct ndb_logevent_StartReport StartReport;
      /** Log event data @ref NDB_LE_SubscriptionStatus */
      struct ndb_logevent_SubscriptionStatus SubscriptionStatus;
      /** Log event data @ref NDB_LE_RedoStatus */
      struct ndb_logevent_RedoStatus RedoStatus;

      struct ndb_logevent_CreateSchemaObject CreateSchemaObject;
      struct ndb_logevent_AlterSchemaObject AlterSchemaObject;
      struct ndb_logevent_DropSchemaObject DropSchemaObject;
      struct ndb_logevent_StartReadLCP StartReadLCP;
      struct ndb_logevent_ReadLCPComplete ReadLCPComplete;
      struct ndb_logevent_RunRedo RunRedo;
      struct ndb_logevent_RebuildIndex RebuildIndex;

      /** Raw data */
      unsigned Data[29];
#ifndef DOXYGEN_FIX
    };
#else
    } <union>;
#endif
  };

enum ndb_logevent_handle_error {
  NDB_LEH_NO_ERROR,
  NDB_LEH_READ_ERROR,
  NDB_LEH_MISSING_EVENT_SPECIFIER,
  NDB_LEH_UNKNOWN_EVENT_TYPE,
  NDB_LEH_UNKNOWN_EVENT_VARIABLE,
  NDB_LEH_INTERNAL_ERROR
};

enum ndb_logevent_event_buffer_status_report_reason{
  NO_REPORT,
  COMPLETELY_BUFFERING,
  PARTIALLY_DISCARDING,
  COMPLETELY_DISCARDING,
  PARTIALLY_BUFFERING,
  BUFFERED_EPOCHS_OVER_THRESHOLD,
  ENOUGH_FREE_EVENTBUFFER,
  LOW_FREE_EVENTBUFFER
};

#ifdef __cplusplus
}
#endif

/** @} */

#endif

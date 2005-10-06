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
    NDB_LE_BackupCompleted = 56,
    /** NDB_MGM_EVENT_CATEGORY_BACKUP */
    NDB_LE_BackupAborted = 57,

    /** NDB_MGM_EVENT_CATEGORY_INFO */
    NDB_LE_EventBufferStatus = 58

    /* 59 used */
    /* 60 unused */
    /* 61 unused */
    /* 62 unused */

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
#ifndef DOXYGEN_SHOULD_SKIP_INTERNAL
    NDB_MGM_MIN_EVENT_CATEGORY = CFG_MIN_LOGLEVEL,
    NDB_MGM_MAX_EVENT_CATEGORY = CFG_MAX_LOGLEVEL
#endif
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
      /** Log event specific data for for corresponding NDB_LE_ log event */
      struct {
	unsigned node;
      } Connected;

      /** Log event specific data for for corresponding NDB_LE_ log event */
      struct {
	unsigned node;
      } Disconnected;

      /** Log event specific data for for corresponding NDB_LE_ log event */
      struct {
	unsigned node;
      } CommunicationClosed;

      /** Log event specific data for for corresponding NDB_LE_ log event */
      struct {
	unsigned node;
      } CommunicationOpened;

      /** Log event specific data for for corresponding NDB_LE_ log event */
      struct {
	unsigned node;
	unsigned version;
      } ConnectedApiVersion;

      /* CHECKPOINT */
      /** Log event specific data for for corresponding NDB_LE_ log event */
      struct {
	unsigned gci;
      } GlobalCheckpointStarted;
      /** Log event specific data for for corresponding NDB_LE_ log event */
      struct {
	unsigned gci;
      } GlobalCheckpointCompleted;
      /** Log event specific data for for corresponding NDB_LE_ log event */
      struct {
	unsigned lci;
	unsigned keep_gci;
	unsigned restore_gci;
      } LocalCheckpointStarted;
      /** Log event specific data for for corresponding NDB_LE_ log event */
      struct {
	unsigned lci;
      } LocalCheckpointCompleted;
      /** Log event specific data for for corresponding NDB_LE_ log event */
      struct {
	unsigned data;
      } LCPStoppedInCalcKeepGci;
      /** Log event specific data for for corresponding NDB_LE_ log event */
      struct {
	unsigned node;
	unsigned table_id;
	unsigned fragment_id;
      } LCPFragmentCompleted;
      /** Log event specific data for for corresponding NDB_LE_ log event */
      struct {
	unsigned acc_count;
	unsigned tup_count;
      } UndoLogBlocked;

      /* STARTUP */
      /** Log event specific data for for corresponding NDB_LE_ log event */
      struct {
	unsigned version;
      } NDBStartStarted;
      /** Log event specific data for for corresponding NDB_LE_ log event */
      struct {
	unsigned version;
      } NDBStartCompleted;
      /** Log event specific data for for corresponding NDB_LE_ log event */
      struct {
      } STTORRYRecieved;
      /** Log event specific data for for corresponding NDB_LE_ log event */
      struct {
	unsigned phase;
	unsigned starttype;
      } StartPhaseCompleted;
      /** Log event specific data for for corresponding NDB_LE_ log event */
      struct {
	unsigned own_id;
	unsigned president_id;
	unsigned dynamic_id;
      } CM_REGCONF;
      /** Log event specific data for for corresponding NDB_LE_ log event */
      struct {
	unsigned own_id;
	unsigned other_id;
	unsigned cause;
      } CM_REGREF;
      /** Log event specific data for for corresponding NDB_LE_ log event */
      struct {
	unsigned own_id;
	unsigned left_id;
	unsigned right_id;
	unsigned dynamic_id;
      } FIND_NEIGHBOURS;
      /** Log event specific data for for corresponding NDB_LE_ log event */
      struct {
	unsigned stoptype;
      } NDBStopStarted;
      /** Log event specific data for for corresponding NDB_LE_ log event */
      struct {
	unsigned action;
	unsigned signum;
      } NDBStopCompleted;
      /** Log event specific data for for corresponding NDB_LE_ log event */
      struct {
	unsigned action;
	unsigned signum;
	unsigned error;
	unsigned sphase;
	unsigned extra;
      } NDBStopForced;
      /** Log event specific data for for corresponding NDB_LE_ log event */
      struct {
      } NDBStopAborted;
      /** Log event specific data for for corresponding NDB_LE_ log event */
      struct {
	unsigned node;
	unsigned keep_gci;
	unsigned completed_gci;
	unsigned restorable_gci;
      } StartREDOLog;
      /** Log event specific data for for corresponding NDB_LE_ log event */
      struct {
	unsigned log_part;
	unsigned start_mb;
	unsigned stop_mb;
	unsigned gci;
      } StartLog;
      /** Log event specific data for for corresponding NDB_LE_ log event */
      struct {
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
      } UNDORecordsExecuted;
  
      /* NODERESTART */
      /** Log event specific data for for corresponding NDB_LE_ log event */
      struct {
      } NR_CopyDict;
      /** Log event specific data for for corresponding NDB_LE_ log event */
      struct {
      } NR_CopyDistr;
      /** Log event specific data for for corresponding NDB_LE_ log event */
      struct {
	unsigned dest_node;
      } NR_CopyFragsStarted;
      /** Log event specific data for for corresponding NDB_LE_ log event */
      struct {
	unsigned dest_node;
	unsigned table_id;
	unsigned fragment_id;
      } NR_CopyFragDone;
      /** Log event specific data for for corresponding NDB_LE_ log event */
      struct {
	unsigned dest_node;
      } NR_CopyFragsCompleted;

      /** Log event specific data for for corresponding NDB_LE_ log event */
      struct {
	unsigned block; /* 0 = all */
	unsigned failed_node;
	unsigned completing_node; /* 0 = all */
      } NodeFailCompleted;
      /** Log event specific data for for corresponding NDB_LE_ log event */
      struct {
	unsigned failed_node;
	unsigned failure_state;
      } NODE_FAILREP;
      /** Log event specific data for for corresponding NDB_LE_ log event */
      struct {
	unsigned code;                /* code & state << 16 */
	unsigned arbit_node;
	unsigned ticket_0;
	unsigned ticket_1;
	/* TODO */
      } ArbitState;
      /** Log event specific data for for corresponding NDB_LE_ log event */
      struct {
	unsigned code;                /* code & state << 16 */
	unsigned arbit_node;
	unsigned ticket_0;
	unsigned ticket_1;
	/* TODO */
      } ArbitResult;
      /** Log event specific data for for corresponding NDB_LE_ log event */
      struct {
      } GCP_TakeoverStarted;
      /** Log event specific data for for corresponding NDB_LE_ log event */
      struct {
      } GCP_TakeoverCompleted;
      /** Log event specific data for for corresponding NDB_LE_ log event */
      struct {
      } LCP_TakeoverStarted;
      /** Log event specific data for for corresponding NDB_LE_ log event */
      struct {
	unsigned state;
      } LCP_TakeoverCompleted;

      /* STATISTIC */
      /** Log event specific data for for corresponding NDB_LE_ log event */
      struct {
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
      } TransReportCounters;
      /** Log event specific data for for corresponding NDB_LE_ log event */
      struct {
	unsigned ops;
      } OperationReportCounters;
      /** Log event specific data for for corresponding NDB_LE_ log event */
      struct {
	unsigned table_id;
      } TableCreated;
      /** Log event specific data for for corresponding NDB_LE_ log event */
      struct {
	unsigned mean_loop_count;
      } JobStatistic;
      /** Log event specific data for for corresponding NDB_LE_ log event */
      struct {
	unsigned to_node;
	unsigned mean_sent_bytes;
      } SendBytesStatistic;
      /** Log event specific data for for corresponding NDB_LE_ log event */
      struct {
	unsigned from_node;
	unsigned mean_received_bytes;
      } ReceiveBytesStatistic;
      /** Log event specific data for for corresponding NDB_LE_ log event */
      struct {
	int      gth;
	unsigned page_size_kb;
	unsigned pages_used;
	unsigned pages_total;
	unsigned block;
      } MemoryUsage;

      /* ERROR */
      /** Log event specific data for for corresponding NDB_LE_ log event */
      struct {
	unsigned to_node;
	unsigned code;
      } TransporterError;
      /** Log event specific data for for corresponding NDB_LE_ log event */
      struct {
	unsigned to_node;
	unsigned code;
      } TransporterWarning;
      /** Log event specific data for for corresponding NDB_LE_ log event */
      struct {
	unsigned node;
	unsigned count;
      } MissedHeartbeat;
      /** Log event specific data for for corresponding NDB_LE_ log event */
      struct {
	unsigned node;
      } DeadDueToHeartbeat;
      /** Log event specific data for for corresponding NDB_LE_ log event */
      struct {
	/* TODO */
      } WarningEvent;

      /* INFO */
      /** Log event specific data for for corresponding NDB_LE_ log event */
      struct {
	unsigned node;
      } SentHeartbeat;
      /** Log event specific data for for corresponding NDB_LE_ log event */
      struct {
	unsigned node;
      } CreateLogBytes;
      /** Log event specific data for for corresponding NDB_LE_ log event */
      struct {
	/* TODO */
      } InfoEvent;
      /** Log event specific data for for corresponding NDB_LE_ log event */
      struct {
	unsigned usage;
	unsigned alloc;
	unsigned max;
	unsigned apply_gci_l;
	unsigned apply_gci_h;
	unsigned latest_gci_l;
	unsigned latest_gci_h;
      } EventBufferStatus;

      /** Log event data for @ref NDB_LE_BackupStarted */
      struct {
	unsigned starting_node;
	unsigned backup_id;
      } BackupStarted;
      /** Log event data @ref NDB_LE_BackupFailedToStart */
      struct {
	unsigned starting_node;
	unsigned error;
      } BackupFailedToStart;
      /** Log event data @ref NDB_LE_BackupCompleted */
      struct {
	unsigned starting_node;
	unsigned backup_id; 
	unsigned start_gci;
	unsigned stop_gci;
	unsigned n_records; 
	unsigned n_log_records;
	unsigned n_bytes;
	unsigned n_log_bytes;
      } BackupCompleted;
      /** Log event data @ref NDB_LE_BackupAborted */
      struct {
	unsigned starting_node;
	unsigned backup_id;
	unsigned error;
      } BackupAborted;
      /** Log event data @ref NDB_LE_SingleUser */
      struct {
        unsigned type;
        unsigned node_id;
      } SingleUser;
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

#ifdef __cplusplus
}
#endif

/** @} */

#endif

/***********************************************************************

Copyright (c) 2010, 2024, Oracle and/or its affiliates.
Copyright (c) 2012, Facebook Inc.

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

***********************************************************************/

/** @file include/srv0mon.h
 Server monitor counter related defines

 Created 12/15/2009     Jimmy Yang
 *******************************************************/

#ifndef srv0mon_h
#define srv0mon_h

#include "univ.i"

#ifndef __STDC_LIMIT_MACROS
/* Required for FreeBSD so that INT64_MAX is defined. */
#define __STDC_LIMIT_MACROS
#endif /* __STDC_LIMIT_MACROS */

#include <stdint.h>

/** Possible status values for "mon_status" in "struct monitor_value" */
enum monitor_running_status {
  MONITOR_STARTED = 1, /*!< Monitor has been turned on */
  MONITOR_STOPPED = 2  /*!< Monitor has been turned off */
};

typedef enum monitor_running_status monitor_running_t;

/** Monitor counter value type */
typedef int64_t mon_type_t;

/** Two monitor structures are defined in this file. One is
"monitor_value_t" which contains dynamic counter values for each
counter. The other is "monitor_info_t", which contains
static information (counter name, desc etc.) for each counter.
In addition, an enum datatype "monitor_id_t" is also defined,
it identifies each monitor with an internally used symbol, whose
integer value indexes into above two structure for its dynamic
and static information.
Developer who intend to add new counters would require to
fill in counter information as described in "monitor_info_t" and
create the internal counter ID in "monitor_id_t". */

/** Structure containing the actual values of a monitor counter. */
struct monitor_value_t {
  std::chrono::system_clock::time_point
      mon_start_time; /*!< Start time of monitoring  */
  std::chrono::system_clock::time_point
      mon_stop_time; /*!< Stop time of monitoring */
  std::chrono::system_clock::time_point
      mon_reset_time;                /*!< Time counter was reset */
  std::atomic<mon_type_t> mon_value; /*!< Current counter Value */
  mon_type_t mon_max_value;          /*!< Current Max value */
  mon_type_t mon_min_value;          /*!< Current Min value */
  mon_type_t mon_value_reset;        /*!< value at last reset */
  mon_type_t mon_max_value_start;    /*!< Max value since start */
  mon_type_t mon_min_value_start;    /*!< Min value since start */
  mon_type_t mon_start_value;        /*!< Value at the start time */
  mon_type_t mon_last_value;         /*!< Last set of values */
  monitor_running_t mon_status;      /* whether monitor still running */
};

/** Following defines are possible values for "monitor_type" field in
"struct monitor_info" */
enum monitor_type_t {
  MONITOR_NONE = 0,            /*!< No monitoring */
  MONITOR_MODULE = 1,          /*!< This is a monitor module type,
                               not a counter */
  MONITOR_EXISTING = 2,        /*!< The monitor carries information from
                               an existing system status variable */
  MONITOR_NO_AVERAGE = 4,      /*!< Set this status if we don't want to
                               calculate the average value for the counter */
  MONITOR_DISPLAY_CURRENT = 8, /*!< Display current value of the
                               counter, rather than incremental value
                               over the period. Mostly for counters
                               displaying current resource usage */
  MONITOR_GROUP_MODULE = 16,   /*!< Monitor can be turned on/off
                               only as a module, but not individually */
  MONITOR_DEFAULT_ON = 32,     /*!< Monitor will be turned on by default at
                               server start up */
  MONITOR_SET_OWNER = 64,      /*!< Owner of "monitor set", a set of
                               monitor counters */
  MONITOR_SET_MEMBER = 128,    /*!< Being part of a "monitor set" */
  MONITOR_HIDDEN = 256         /*!< Do not display this monitor in the
                               metrics table */
};

/** Counter minimum value is initialized to be max value of
 mon_type_t (int64_t) */
#define MIN_RESERVED INT64_MAX
#define MAX_RESERVED (~MIN_RESERVED)

/** This enumeration defines internal monitor identifier used internally
to identify each particular counter. Its value indexes into two arrays,
one is the "innodb_counter_value" array which records actual monitor
counter values, the other is "innodb_counter_info" array which describes
each counter's basic information (name, desc etc.). A couple of
naming rules here:
1) If the monitor defines a module, it starts with MONITOR_MODULE
2) If the monitor uses existing counters from "status variable", its ID
name shall start with MONITOR_OVLD

Please refer to "innodb_counter_info" in srv/srv0mon.cc for detail
information for each monitor counter */

enum monitor_id_t {
  /* This is to identify the default value set by the metrics
  control global variables */
  MONITOR_DEFAULT_START = 0,

  /* Start of Metadata counter */
  MONITOR_MODULE_METADATA,
  MONITOR_TABLE_OPEN,
  MONITOR_TABLE_CLOSE,
  MONITOR_TABLE_REFERENCE,

  /* Lock manager related counters */
  MONITOR_MODULE_LOCK,
  MONITOR_DEADLOCK,
  MONITOR_DEADLOCK_FALSE_POSITIVES,
  MONITOR_DEADLOCK_ROUNDS,
  MONITOR_LOCK_THREADS_WAITING,
  MONITOR_TIMEOUT,
  MONITOR_LOCKREC_WAIT,
  MONITOR_TABLELOCK_WAIT,
  MONITOR_NUM_RECLOCK_REQ,
  MONITOR_RECLOCK_RELEASE_ATTEMPTS,
  MONITOR_RECLOCK_GRANT_ATTEMPTS,
  MONITOR_RECLOCK_CREATED,
  MONITOR_RECLOCK_REMOVED,
  MONITOR_NUM_RECLOCK,
  MONITOR_TABLELOCK_CREATED,
  MONITOR_TABLELOCK_REMOVED,
  MONITOR_NUM_TABLELOCK,
  MONITOR_OVLD_ROW_LOCK_CURRENT_WAIT,
  MONITOR_OVLD_LOCK_WAIT_TIME,
  MONITOR_OVLD_LOCK_MAX_WAIT_TIME,
  MONITOR_OVLD_ROW_LOCK_WAIT,
  MONITOR_OVLD_LOCK_AVG_WAIT_TIME,
  MONITOR_SCHEDULE_REFRESHES,

  /* Buffer and I/O related counters. */
  MONITOR_MODULE_BUFFER,
  MONITOR_OVLD_BUFFER_POOL_SIZE,
  MONITOR_OVLD_BUF_POOL_READS,
  MONITOR_OVLD_BUF_POOL_READ_REQUESTS,
  MONITOR_OVLD_BUF_POOL_WRITE_REQUEST,
  MONITOR_OVLD_BUF_POOL_WAIT_FREE,
  MONITOR_OVLD_BUF_POOL_READ_AHEAD,
  MONITOR_OVLD_BUF_POOL_READ_AHEAD_EVICTED,
  MONITOR_OVLD_BUF_POOL_PAGE_TOTAL,
  MONITOR_OVLD_BUF_POOL_PAGE_MISC,
  MONITOR_OVLD_BUF_POOL_PAGES_DATA,
  MONITOR_OVLD_BUF_POOL_BYTES_DATA,
  MONITOR_OVLD_BUF_POOL_PAGES_DIRTY,
  MONITOR_OVLD_BUF_POOL_BYTES_DIRTY,
  MONITOR_OVLD_BUF_POOL_PAGES_FREE,
  MONITOR_OVLD_PAGE_CREATED,
  MONITOR_OVLD_PAGES_WRITTEN,
  MONITOR_OVLD_PAGES_READ,
  MONITOR_OVLD_BYTE_READ,
  MONITOR_OVLD_BYTE_WRITTEN,
  MONITOR_FLUSH_BATCH_SCANNED,
  MONITOR_FLUSH_BATCH_SCANNED_NUM_CALL,
  MONITOR_FLUSH_BATCH_SCANNED_PER_CALL,
  MONITOR_FLUSH_BATCH_TOTAL_PAGE,
  MONITOR_FLUSH_BATCH_COUNT,
  MONITOR_FLUSH_BATCH_PAGES,
  MONITOR_FLUSH_NEIGHBOR_TOTAL_PAGE,
  MONITOR_FLUSH_NEIGHBOR_COUNT,
  MONITOR_FLUSH_NEIGHBOR_PAGES,
  MONITOR_FLUSH_N_TO_FLUSH_REQUESTED,
  MONITOR_FLUSH_N_TO_FLUSH_BY_DIRTY_PAGE,

  MONITOR_FLUSH_N_TO_FLUSH_BY_AGE,
  MONITOR_FLUSH_ADAPTIVE_AVG_TIME_SLOT,
  MONITOR_LRU_BATCH_FLUSH_AVG_TIME_SLOT,

  MONITOR_FLUSH_ADAPTIVE_AVG_TIME_THREAD,
  MONITOR_LRU_BATCH_FLUSH_AVG_TIME_THREAD,
  MONITOR_FLUSH_ADAPTIVE_AVG_TIME_EST,
  MONITOR_LRU_BATCH_FLUSH_AVG_TIME_EST,
  MONITOR_FLUSH_AVG_TIME,

  MONITOR_FLUSH_ADAPTIVE_AVG_PASS,
  MONITOR_LRU_BATCH_FLUSH_AVG_PASS,
  MONITOR_FLUSH_AVG_PASS,

  MONITOR_LRU_GET_FREE_LOOPS,
  MONITOR_LRU_GET_FREE_WAITS,

  MONITOR_FLUSH_AVG_PAGE_RATE,
  MONITOR_FLUSH_LSN_AVG_RATE,
  MONITOR_FLUSH_PCT_FOR_DIRTY,
  MONITOR_FLUSH_PCT_FOR_LSN,
  MONITOR_FLUSH_SYNC_WAITS,
  MONITOR_FLUSH_ADAPTIVE_TOTAL_PAGE,
  MONITOR_FLUSH_ADAPTIVE_COUNT,
  MONITOR_FLUSH_ADAPTIVE_PAGES,
  MONITOR_FLUSH_SYNC_TOTAL_PAGE,
  MONITOR_FLUSH_SYNC_COUNT,
  MONITOR_FLUSH_SYNC_PAGES,
  MONITOR_FLUSH_BACKGROUND_TOTAL_PAGE,
  MONITOR_FLUSH_BACKGROUND_COUNT,
  MONITOR_FLUSH_BACKGROUND_PAGES,
  MONITOR_LRU_BATCH_SCANNED,
  MONITOR_LRU_BATCH_SCANNED_NUM_CALL,
  MONITOR_LRU_BATCH_SCANNED_PER_CALL,
  MONITOR_LRU_BATCH_FLUSH_TOTAL_PAGE,
  MONITOR_LRU_BATCH_FLUSH_COUNT,
  MONITOR_LRU_BATCH_FLUSH_PAGES,
  MONITOR_LRU_BATCH_EVICT_TOTAL_PAGE,
  MONITOR_LRU_BATCH_EVICT_COUNT,
  MONITOR_LRU_BATCH_EVICT_PAGES,
  MONITOR_LRU_SINGLE_FLUSH_SCANNED,
  MONITOR_LRU_SINGLE_FLUSH_SCANNED_NUM_CALL,
  MONITOR_LRU_SINGLE_FLUSH_SCANNED_PER_CALL,
  MONITOR_LRU_SINGLE_FLUSH_FAILURE_COUNT,
  MONITOR_LRU_GET_FREE_SEARCH,
  MONITOR_LRU_SEARCH_SCANNED,
  MONITOR_LRU_SEARCH_SCANNED_NUM_CALL,
  MONITOR_LRU_SEARCH_SCANNED_PER_CALL,
  MONITOR_LRU_UNZIP_SEARCH_SCANNED,
  MONITOR_LRU_UNZIP_SEARCH_SCANNED_NUM_CALL,
  MONITOR_LRU_UNZIP_SEARCH_SCANNED_PER_CALL,

  /* Buffer Page I/O specific counters. */
  MONITOR_MODULE_BUF_PAGE,
  MONITOR_INDEX_LEAF_PAGE_READ,
  MONITOR_INDEX_NON_LEAF_PAGE_READ,
  MONITOR_INDEX_IBUF_LEAF_PAGE_READ,
  MONITOR_INDEX_IBUF_NON_LEAF_PAGE_READ,
  MONITOR_UNDO_LOG_PAGE_READ,
  MONITOR_INODE_PAGE_READ,
  MONITOR_IBUF_FREELIST_PAGE_READ,
  MONITOR_IBUF_BITMAP_PAGE_READ,
  MONITOR_SYSTEM_PAGE_READ,
  MONITOR_TRX_SYSTEM_PAGE_READ,
  MONITOR_FSP_HDR_PAGE_READ,
  MONITOR_XDES_PAGE_READ,
  MONITOR_BLOB_PAGE_READ,
  MONITOR_ZBLOB_PAGE_READ,
  MONITOR_ZBLOB2_PAGE_READ,
  MONITOR_RSEG_ARRAY_PAGE_READ,
  MONITOR_OTHER_PAGE_READ,
  MONITOR_INDEX_LEAF_PAGE_WRITTEN,
  MONITOR_INDEX_NON_LEAF_PAGE_WRITTEN,
  MONITOR_INDEX_IBUF_LEAF_PAGE_WRITTEN,
  MONITOR_INDEX_IBUF_NON_LEAF_PAGE_WRITTEN,
  MONITOR_UNDO_LOG_PAGE_WRITTEN,
  MONITOR_INODE_PAGE_WRITTEN,
  MONITOR_IBUF_FREELIST_PAGE_WRITTEN,
  MONITOR_IBUF_BITMAP_PAGE_WRITTEN,
  MONITOR_SYSTEM_PAGE_WRITTEN,
  MONITOR_TRX_SYSTEM_PAGE_WRITTEN,
  MONITOR_FSP_HDR_PAGE_WRITTEN,
  MONITOR_XDES_PAGE_WRITTEN,
  MONITOR_BLOB_PAGE_WRITTEN,
  MONITOR_ZBLOB_PAGE_WRITTEN,
  MONITOR_ZBLOB2_PAGE_WRITTEN,
  MONITOR_RSEG_ARRAY_PAGE_WRITTEN,
  MONITOR_OTHER_PAGE_WRITTEN,
  MONITOR_ON_LOG_NO_WAITS_PAGE_WRITTEN,
  MONITOR_ON_LOG_WAITS_PAGE_WRITTEN,
  MONITOR_ON_LOG_WAIT_LOOPS_PAGE_WRITTEN,

  /* OS level counters (I/O) */
  MONITOR_MODULE_OS,
  MONITOR_OVLD_OS_FILE_READ,
  MONITOR_OVLD_OS_FILE_WRITE,
  MONITOR_OVLD_OS_FSYNC,
  MONITOR_OS_PENDING_READS,
  MONITOR_OS_PENDING_WRITES,
  MONITOR_OVLD_OS_LOG_WRITTEN,
  MONITOR_OVLD_OS_LOG_FSYNC,
  MONITOR_OVLD_OS_LOG_PENDING_FSYNC,
  MONITOR_OVLD_OS_LOG_PENDING_WRITES,

  /* Transaction related counters */
  MONITOR_MODULE_TRX,
  MONITOR_TRX_RW_COMMIT,
  MONITOR_TRX_RO_COMMIT,
  MONITOR_TRX_NL_RO_COMMIT,
  MONITOR_TRX_COMMIT_UNDO,
  MONITOR_TRX_ROLLBACK,
  MONITOR_TRX_ROLLBACK_SAVEPOINT,
  MONITOR_TRX_ROLLBACK_ACTIVE,
  MONITOR_TRX_ACTIVE,
  MONITOR_TRX_ALLOCATIONS,
  MONITOR_TRX_ON_LOG_NO_WAITS,
  MONITOR_TRX_ON_LOG_WAITS,
  MONITOR_TRX_ON_LOG_WAIT_LOOPS,
  MONITOR_RSEG_HISTORY_LEN,
  MONITOR_NUM_UNDO_SLOT_USED,
  MONITOR_NUM_UNDO_SLOT_CACHED,
  MONITOR_RSEG_CUR_SIZE,

  /* Purge related counters */
  MONITOR_MODULE_PURGE,
  MONITOR_N_DEL_ROW_PURGE,
  MONITOR_N_UPD_EXIST_EXTERN,
  MONITOR_PURGE_INVOKED,
  MONITOR_PURGE_N_PAGE_HANDLED,
  MONITOR_DML_PURGE_DELAY,
  MONITOR_PURGE_STOP_COUNT,
  MONITOR_PURGE_RESUME_COUNT,
  MONITOR_PURGE_TRUNCATE_HISTORY_COUNT,
  MONITOR_PURGE_TRUNCATE_HISTORY_MICROSECOND,

  /* Undo tablespace truncation */
  MONITOR_UNDO_TRUNCATE,
  MONITOR_UNDO_TRUNCATE_COUNT,
  MONITOR_UNDO_TRUNCATE_START_LOGGING_COUNT,
  MONITOR_UNDO_TRUNCATE_DONE_LOGGING_COUNT,
  MONITOR_UNDO_TRUNCATE_MICROSECOND,

  /* Recovery related counters */
  MONITOR_MODULE_REDO_LOG,
  MONITOR_OVLD_LSN_FLUSHDISK,
  MONITOR_OVLD_LSN_CHECKPOINT,
  MONITOR_OVLD_LSN_CURRENT,
  MONITOR_OVLD_LSN_ARCHIVED,
  MONITOR_OVLD_LSN_CHECKPOINT_AGE,
  MONITOR_OVLD_LSN_BUF_DIRTY_PAGES_ADDED,
  MONITOR_OVLD_BUF_OLDEST_LSN_APPROX,
  MONITOR_OVLD_BUF_OLDEST_LSN_LWM,
  MONITOR_OVLD_MAX_AGE_ASYNC,
  MONITOR_OVLD_MAX_AGE_SYNC,
  MONITOR_OVLD_LOG_WAITS,
  MONITOR_OVLD_LOG_WRITE_REQUEST,
  MONITOR_OVLD_LOG_WRITES,

  MONITOR_LOG_FLUSH_TOTAL_TIME,
  MONITOR_LOG_FLUSH_MAX_TIME,
  MONITOR_LOG_FLUSH_AVG_TIME,
  MONITOR_LOG_FLUSH_LSN_AVG_RATE,

  MONITOR_LOG_FULL_BLOCK_WRITES,
  MONITOR_LOG_PARTIAL_BLOCK_WRITES,
  MONITOR_LOG_PADDED,
  MONITOR_LOG_NEXT_FILE,
  MONITOR_LOG_CHECKPOINTS,
  MONITOR_LOG_FREE_SPACE,
  MONITOR_LOG_CONCURRENCY_MARGIN,

  MONITOR_LOG_WRITER_NO_WAITS,
  MONITOR_LOG_WRITER_WAITS,
  MONITOR_LOG_WRITER_WAIT_LOOPS,
  MONITOR_LOG_WRITER_ON_FREE_SPACE_WAITS,
  MONITOR_LOG_WRITER_ON_ARCHIVER_WAITS,

  MONITOR_LOG_FLUSHER_NO_WAITS,
  MONITOR_LOG_FLUSHER_WAITS,
  MONITOR_LOG_FLUSHER_WAIT_LOOPS,

  MONITOR_LOG_WRITE_NOTIFIER_NO_WAITS,
  MONITOR_LOG_WRITE_NOTIFIER_WAITS,
  MONITOR_LOG_WRITE_NOTIFIER_WAIT_LOOPS,

  MONITOR_LOG_FLUSH_NOTIFIER_NO_WAITS,
  MONITOR_LOG_FLUSH_NOTIFIER_WAITS,
  MONITOR_LOG_FLUSH_NOTIFIER_WAIT_LOOPS,

  MONITOR_LOG_WRITE_TO_FILE_REQUESTS_INTERVAL,

  MONITOR_LOG_ON_WRITE_NO_WAITS,
  MONITOR_LOG_ON_WRITE_WAITS,
  MONITOR_LOG_ON_WRITE_WAIT_LOOPS,
  MONITOR_LOG_ON_FLUSH_NO_WAITS,
  MONITOR_LOG_ON_FLUSH_WAITS,
  MONITOR_LOG_ON_FLUSH_WAIT_LOOPS,
  MONITOR_LOG_ON_RECENT_WRITTEN_WAIT_LOOPS,
  MONITOR_LOG_ON_RECENT_CLOSED_WAIT_LOOPS,
  MONITOR_LOG_ON_BUFFER_SPACE_NO_WAITS,
  MONITOR_LOG_ON_BUFFER_SPACE_WAITS,
  MONITOR_LOG_ON_BUFFER_SPACE_WAIT_LOOPS,
  MONITOR_LOG_ON_FILE_SPACE_NO_WAITS,
  MONITOR_LOG_ON_FILE_SPACE_WAITS,
  MONITOR_LOG_ON_FILE_SPACE_WAIT_LOOPS,

  /* Page Manager related counters */
  MONITOR_MODULE_PAGE,
  MONITOR_PAGE_COMPRESS,
  MONITOR_PAGE_DECOMPRESS,
  MONITOR_PAD_INCREMENTS,
  MONITOR_PAD_DECREMENTS,

  /* Index related counters */
  MONITOR_MODULE_INDEX,
  MONITOR_INDEX_SPLIT,
  MONITOR_INDEX_MERGE_ATTEMPTS,
  MONITOR_INDEX_MERGE_SUCCESSFUL,
  MONITOR_INDEX_REORG_ATTEMPTS,
  MONITOR_INDEX_REORG_SUCCESSFUL,
  MONITOR_INDEX_DISCARD,

  /* Adaptive Hash Index related counters */
  MONITOR_MODULE_ADAPTIVE_HASH,
  MONITOR_OVLD_ADAPTIVE_HASH_SEARCH,
  MONITOR_OVLD_ADAPTIVE_HASH_SEARCH_BTREE,
  MONITOR_ADAPTIVE_HASH_PAGE_ADDED,
  MONITOR_ADAPTIVE_HASH_PAGE_REMOVED,
  MONITOR_ADAPTIVE_HASH_ROW_ADDED,
  MONITOR_ADAPTIVE_HASH_ROW_REMOVED,
  MONITOR_ADAPTIVE_HASH_ROW_REMOVE_NOT_FOUND,
  MONITOR_ADAPTIVE_HASH_ROW_UPDATED,

  /* Tablespace related counters */
  MONITOR_MODULE_FIL_SYSTEM,
  MONITOR_OVLD_N_FILE_OPENED,

  /* InnoDB Change Buffer related counters */
  MONITOR_MODULE_IBUF_SYSTEM,
  MONITOR_OVLD_IBUF_MERGE_INSERT,
  MONITOR_OVLD_IBUF_MERGE_DELETE,
  MONITOR_OVLD_IBUF_MERGE_PURGE,
  MONITOR_OVLD_IBUF_MERGE_DISCARD_INSERT,
  MONITOR_OVLD_IBUF_MERGE_DISCARD_DELETE,
  MONITOR_OVLD_IBUF_MERGE_DISCARD_PURGE,
  MONITOR_OVLD_IBUF_MERGES,
  MONITOR_OVLD_IBUF_SIZE,

  /* Counters for server operations */
  MONITOR_MODULE_SERVER,
  MONITOR_MASTER_THREAD_SLEEP,
  MONITOR_OVLD_SERVER_ACTIVITY,
  MONITOR_MASTER_ACTIVE_LOOPS,
  MONITOR_MASTER_IDLE_LOOPS,
  MONITOR_SRV_BACKGROUND_DROP_TABLE_MICROSECOND,
  MONITOR_SRV_IBUF_MERGE_MICROSECOND,
  MONITOR_SRV_MEM_VALIDATE_MICROSECOND,
  MONITOR_SRV_PURGE_MICROSECOND,
  MONITOR_SRV_DICT_LRU_MICROSECOND,
  MONITOR_SRV_DICT_LRU_EVICT_COUNT,
  MONITOR_OVLD_SRV_DBLWR_WRITES,
  MONITOR_OVLD_SRV_DBLWR_PAGES_WRITTEN,
  MONITOR_OVLD_SRV_PAGE_SIZE,
  MONITOR_OVLD_RWLOCK_S_SPIN_WAITS,
  MONITOR_OVLD_RWLOCK_X_SPIN_WAITS,
  MONITOR_OVLD_RWLOCK_SX_SPIN_WAITS,
  MONITOR_OVLD_RWLOCK_S_SPIN_ROUNDS,
  MONITOR_OVLD_RWLOCK_X_SPIN_ROUNDS,
  MONITOR_OVLD_RWLOCK_SX_SPIN_ROUNDS,
  MONITOR_OVLD_RWLOCK_S_OS_WAITS,
  MONITOR_OVLD_RWLOCK_X_OS_WAITS,
  MONITOR_OVLD_RWLOCK_SX_OS_WAITS,

  /* Data DML related counters */
  MONITOR_MODULE_DML_STATS,
  MONITOR_OLVD_ROW_READ,
  MONITOR_OLVD_ROW_INSERTED,
  MONITOR_OLVD_ROW_DELETED,
  MONITOR_OLVD_ROW_UPDTATED,
  MONITOR_OLVD_SYSTEM_ROW_READ,
  MONITOR_OLVD_SYSTEM_ROW_INSERTED,
  MONITOR_OLVD_SYSTEM_ROW_DELETED,
  MONITOR_OLVD_SYSTEM_ROW_UPDATED,

  /* Sampling related counters */
  MONITOR_MODULE_SAMPLING_STATS,
  MONITOR_SAMPLED_PAGES_READ,
  MONITOR_SAMPLED_PAGES_SKIPPED,

  /* Data DDL related counters */
  MONITOR_MODULE_DDL_STATS,
  MONITOR_BACKGROUND_DROP_TABLE,
  MONITOR_ONLINE_CREATE_INDEX,
  MONITOR_PENDING_ALTER_TABLE,
  MONITOR_ALTER_TABLE_SORT_FILES,
  MONITOR_ALTER_TABLE_LOG_FILES,

  MONITOR_MODULE_ICP,
  MONITOR_ICP_ATTEMPTS,
  MONITOR_ICP_NO_MATCH,
  MONITOR_ICP_OUT_OF_RANGE,
  MONITOR_ICP_MATCH,

  /* Mutex/RW-Lock related counters */
  MONITOR_MODULE_LATCHES,
  MONITOR_LATCHES,

  /* CPU usage information */
  MONITOR_MODULE_CPU,
  MONITOR_CPU_UTIME_ABS,
  MONITOR_CPU_STIME_ABS,
  MONITOR_CPU_UTIME_PCT,
  MONITOR_CPU_STIME_PCT,
  MONITOR_CPU_N,

  MONITOR_MODULE_PAGE_TRACK,
  MONITOR_PAGE_TRACK_RESETS,
  MONITOR_PAGE_TRACK_PARTIAL_BLOCK_WRITES,
  MONITOR_PAGE_TRACK_FULL_BLOCK_WRITES,
  MONITOR_PAGE_TRACK_CHECKPOINT_PARTIAL_FLUSH_REQUEST,

  MONITOR_MODULE_DBLWR,
  MONITOR_DBLWR_ASYNC_REQUESTS,
  MONITOR_DBLWR_SYNC_REQUESTS,
  MONITOR_DBLWR_FLUSH_REQUESTS,
  MONITOR_DBLWR_FLUSH_WAIT_EVENTS,

  /* This is used only for control system to turn
  on/off and reset all monitor counters */
  MONITOR_ALL_COUNTER,

  /* This must be the last member */
  NUM_MONITOR
};

/** This informs the monitor control system to turn
on/off and reset monitor counters through wild card match */
#define MONITOR_WILDCARD_MATCH (NUM_MONITOR + 1)

/** Cannot find monitor counter with a specified name */
#define MONITOR_NO_MATCH (NUM_MONITOR + 2)

/** struct monitor_info describes the basic/static information
about each monitor counter. */
struct monitor_info_t {
  const char *monitor_name;        /*!< Monitor name */
  const char *monitor_module;      /*!< Sub Module the monitor
                                   belongs to */
  const char *monitor_desc;        /*!< Brief desc of monitor counter */
  monitor_type_t monitor_type;     /*!< Type of Monitor Info */
  monitor_id_t monitor_related_id; /*!< Monitor ID of counter that
                                related to this monitor. This is
                                set when the monitor belongs to
                                a "monitor set" */
  monitor_id_t monitor_id;         /*!< Monitor ID as defined in enum
                                   monitor_id_t */
};

/** Following are the "set_option" values allowed for
srv_mon_process_existing_counter() and srv_mon_process_existing_counter()
functions. To turn on/off/reset the monitor counters. */
enum mon_option_t {
  MONITOR_TURN_ON = 1,     /*!< Turn on the counter */
  MONITOR_TURN_OFF,        /*!< Turn off the counter */
  MONITOR_RESET_VALUE,     /*!< Reset current values */
  MONITOR_RESET_ALL_VALUE, /*!< Reset all values */
  MONITOR_GET_VALUE        /*!< Option for
                           srv_mon_process_existing_counter()
                           function */
};

#ifndef UNIV_HOTBACKUP
/** Number of bit in a ulint datatype */
#define NUM_BITS_ULINT (sizeof(ulint) * CHAR_BIT)

/** This "monitor_set_tbl" is a bitmap records whether a particular monitor
counter has been turned on or off */
extern ulint
    monitor_set_tbl[(NUM_MONITOR + NUM_BITS_ULINT - 1) / NUM_BITS_ULINT];

/** Macros to turn on/off the control bit in monitor_set_tbl for a monitor
counter option. */
#define MONITOR_ON(monitor)                     \
  (monitor_set_tbl[monitor / NUM_BITS_ULINT] |= \
   ((ulint)1 << (monitor % NUM_BITS_ULINT)))

#define MONITOR_OFF(monitor)                    \
  (monitor_set_tbl[monitor / NUM_BITS_ULINT] &= \
   ~((ulint)1 << (monitor % NUM_BITS_ULINT)))

/** Check whether the requested monitor is turned on/off */
#define MONITOR_IS_ON(monitor)                 \
  (monitor_set_tbl[monitor / NUM_BITS_ULINT] & \
   ((ulint)1 << (monitor % NUM_BITS_ULINT)))

/** The actual monitor counter array that records each monitor counter
value */
extern monitor_value_t innodb_counter_value[NUM_MONITOR];

/** Following are macro defines for basic monitor counter manipulations.
Please note we do not provide any synchronization for these monitor
operations due to performance consideration. Most counters can
be placed under existing mutex protections in respective code
module. */

/** Macros to access various fields of a monitor counters */
#define MONITOR_FIELD(monitor, field) (innodb_counter_value[monitor].field)

#define MONITOR_VALUE(monitor) MONITOR_FIELD(monitor, mon_value)

#define MONITOR_MAX_VALUE(monitor) MONITOR_FIELD(monitor, mon_max_value)

#define MONITOR_MIN_VALUE(monitor) MONITOR_FIELD(monitor, mon_min_value)

#define MONITOR_VALUE_RESET(monitor) MONITOR_FIELD(monitor, mon_value_reset)

#define MONITOR_MAX_VALUE_START(monitor) \
  MONITOR_FIELD(monitor, mon_max_value_start)

#define MONITOR_MIN_VALUE_START(monitor) \
  MONITOR_FIELD(monitor, mon_min_value_start)

#define MONITOR_LAST_VALUE(monitor) MONITOR_FIELD(monitor, mon_last_value)

#define MONITOR_START_VALUE(monitor) MONITOR_FIELD(monitor, mon_start_value)

#define MONITOR_VALUE_SINCE_START(monitor) \
  (MONITOR_VALUE(monitor) + MONITOR_VALUE_RESET(monitor))

#define MONITOR_STATUS(monitor) MONITOR_FIELD(monitor, mon_status)

#define MONITOR_SET_START(monitor)             \
  do {                                         \
    MONITOR_STATUS(monitor) = MONITOR_STARTED; \
    MONITOR_FIELD((monitor), mon_start_time) = \
        std::chrono::system_clock::now();      \
  } while (0)

#define MONITOR_SET_OFF(monitor)               \
  do {                                         \
    MONITOR_STATUS(monitor) = MONITOR_STOPPED; \
    MONITOR_FIELD((monitor), mon_stop_time) =  \
        std::chrono::system_clock::now();      \
  } while (0)

#define MONITOR_INIT_ZERO_VALUE 0

/** Max and min values are initialized when we first turn on the monitor
counter, and set the MONITOR_STATUS. */
#define MONITOR_MAX_MIN_NOT_INIT(monitor)                   \
  (MONITOR_STATUS(monitor) == MONITOR_INIT_ZERO_VALUE &&    \
   MONITOR_MIN_VALUE(monitor) == MONITOR_INIT_ZERO_VALUE && \
   MONITOR_MAX_VALUE(monitor) == MONITOR_INIT_ZERO_VALUE)

#define MONITOR_INIT(monitor)                        \
  if (MONITOR_MAX_MIN_NOT_INIT(monitor)) {           \
    MONITOR_MIN_VALUE(monitor) = MIN_RESERVED;       \
    MONITOR_MIN_VALUE_START(monitor) = MIN_RESERVED; \
    MONITOR_MAX_VALUE(monitor) = MAX_RESERVED;       \
    MONITOR_MAX_VALUE_START(monitor) = MAX_RESERVED; \
  }

#ifdef UNIV_DEBUG_VALGRIND
#define MONITOR_CHECK_DEFINED(value)          \
  do {                                        \
    UNIV_MEM_ASSERT_RW(&value, sizeof value); \
  } while (0)
#else /* UNIV_DEBUG_VALGRIND */
#define MONITOR_CHECK_DEFINED(value) (void)0
#endif /* UNIV_DEBUG_VALGRIND */

/** Macros to increment/decrement the counters. The normal
monitor counter operation expects appropriate synchronization
already exists. No additional mutex is necessary when operating
on the counters */
#define MONITOR_INC(monitor) monitor_inc_value(monitor, 1)
#define MONITOR_DEC(monitor) monitor_dec(monitor)
#define MONITOR_INC_VALUE(monitor, value) monitor_inc_value(monitor, value)
#define MONITOR_DEC_VALUE(monitor, value) monitor_dec_value(monitor, value)

/* Increment/decrement counter without check the monitor on/off bit, which
could already be checked as a module group */
#define MONITOR_INC_NOCHECK(monitor) monitor_inc_value_nocheck(monitor, 1)
#define MONITOR_DEC_NOCHECK(monitor) monitor_dec_value_nocheck(monitor, 1)

/** Atomically increment a monitor counter.
Use MONITOR_INC if appropriate mutex protection exists.
@param monitor monitor to be incremented by 1 */
#define MONITOR_ATOMIC_INC(monitor) monitor_atomic_inc(monitor, 1)

#define MONITOR_ATOMIC_INC_VALUE(monitor, value) \
  monitor_atomic_inc(monitor, value)

/** Atomically decrement a monitor counter.
Use MONITOR_DEC if appropriate mutex protection exists.
@param monitor monitor to be decremented by 1 */
#define MONITOR_ATOMIC_DEC(monitor) monitor_atomic_dec(monitor)

inline void monitor_set_max_value(monitor_id_t monitor, mon_type_t value) {
  if (value > MONITOR_MAX_VALUE(monitor)) {
    MONITOR_MAX_VALUE(monitor) = value;
  }
}

inline void monitor_set_min_value(monitor_id_t monitor, mon_type_t value) {
  if (value < MONITOR_MIN_VALUE(monitor)) {
    MONITOR_MIN_VALUE(monitor) = value;
  }
}

inline void monitor_atomic_inc(monitor_id_t monitor, mon_type_t inc_value) {
  if (MONITOR_IS_ON(monitor)) {
    const mon_type_t value =
        MONITOR_VALUE(monitor).fetch_add(inc_value) + inc_value;
    /* Note: This is not 100% accurate because of the inherent race, we ignore
    it due to performance. */
    monitor_set_max_value(monitor, value);
  }
}

inline void monitor_atomic_dec(monitor_id_t monitor) {
  if (MONITOR_IS_ON(monitor)) {
    const mon_type_t value = --MONITOR_VALUE(monitor);
    /* Note: This is not 100% accurate because of the inherent race, we ignore
    it due to performance. */
    monitor_set_min_value(monitor, value);
  }
}

inline void monitor_inc_value_nocheck(monitor_id_t monitor, mon_type_t value,
                                      bool set_max = true) {
  /* We use std::memory_order_relaxed load() and store() as two separate steps,
  instead of single atomic fetch_add operation, because we want to leave it
  non-atomic as it was before changing mon_value to std::atomic.*/
  const auto new_value =
      MONITOR_VALUE(monitor).load(std::memory_order_relaxed) + value;
  MONITOR_VALUE(monitor).store(new_value, std::memory_order_relaxed);
  if (set_max) {
    monitor_set_max_value(monitor, new_value);
  }
}

inline void monitor_inc_value(monitor_id_t monitor, mon_type_t value) {
  MONITOR_CHECK_DEFINED(value);
  if (MONITOR_IS_ON(monitor)) {
    monitor_inc_value_nocheck(monitor, value);
  }
}

inline void monitor_dec_value_nocheck(monitor_id_t monitor, mon_type_t value) {
  /* We use std::memory_order_relaxed load() and store() as two separate steps,
  instead of single atomic fetch_sub operation, because we want to leave it
  non-atomic as it was before changing mon_value to std::atomic.*/
  const auto new_value =
      MONITOR_VALUE(monitor).load(std::memory_order_relaxed) - value;
  MONITOR_VALUE(monitor).store(new_value, std::memory_order_relaxed);
  monitor_set_min_value(monitor, new_value);
}

inline void monitor_dec_value(monitor_id_t monitor, mon_type_t value) {
  MONITOR_CHECK_DEFINED(value);
  if (MONITOR_IS_ON(monitor)) {
    ut_ad(MONITOR_VALUE(monitor) >= value);
    monitor_dec_value_nocheck(monitor, value);
  }
}

inline void monitor_dec(monitor_id_t monitor) {
  if (MONITOR_IS_ON(monitor)) {
    monitor_dec_value_nocheck(monitor, 1);
  }
}

/** Directly set a monitor counter's value */
#define MONITOR_SET(monitor, value) monitor_set(monitor, value, true, true)

/** Sets a value to the monitor counter
@param monitor monitor to update
@param value value to set
@param set_max says whether to update MONITOR_MAX_VALUE
@param set_min says whether to update MONITOR_MIN_VALUE */
inline void monitor_set(monitor_id_t monitor, mon_type_t value, bool set_max,
                        bool set_min) {
  MONITOR_CHECK_DEFINED(value);
  if (MONITOR_IS_ON(monitor)) {
    MONITOR_VALUE(monitor).store(value, std::memory_order_relaxed);
    if (set_max) {
      monitor_set_max_value(monitor, value);
    }
    if (set_min) {
      monitor_set_min_value(monitor, value);
    }
  }
}

/** Add time difference between now and input "value" to the monitor counter
@param monitor monitor to update for the time difference
@param value the start time value */
#define MONITOR_INC_TIME(monitor, value) monitor_inc_time(monitor, value)

inline void monitor_inc_time(monitor_id_t monitor,
                             std::chrono::steady_clock::time_point value) {
  MONITOR_CHECK_DEFINED(value);
  if (MONITOR_IS_ON(monitor)) {
    const mon_type_t new_value =
        MONITOR_VALUE(monitor).load(std::memory_order_relaxed) +
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - value)
            .count();
    MONITOR_VALUE(monitor).store(new_value, std::memory_order_relaxed);
  }
}

/** This macro updates 3 counters in one call. However, it only checks the
main/first monitor counter 'monitor', to see it is on or off to decide
whether to do the update.
@param monitor the main monitor counter to update. It accounts for
                        the accumulative value for the counter.
@param monitor_n_calls counter that counts number of times this macro is
                        called
@param monitor_per_call counter that records the current and max value of
                        each incremental value
@param value incremental value to record this time */
#define MONITOR_INC_VALUE_CUMULATIVE(monitor, monitor_n_calls,             \
                                     monitor_per_call, value)              \
  monitor_inc_value_cumulative(monitor, monitor_n_calls, monitor_per_call, \
                               value)

inline void monitor_inc_value_cumulative(monitor_id_t monitor,
                                         monitor_id_t monitor_n_calls,
                                         monitor_id_t monitor_per_call,
                                         mon_type_t value) {
  MONITOR_CHECK_DEFINED(value);
  if (MONITOR_IS_ON(monitor)) {
    monitor_inc_value_nocheck(monitor_n_calls, 1, false);
    monitor_set(monitor_per_call, value, true, false);
    monitor_inc_value_nocheck(monitor, value);
  }
}

/** Directly set a monitor counter's value, and if the value
is monotonically increasing, only max value needs to be updated */
#define MONITOR_SET_UPD_MAX_ONLY(monitor, value) \
  monitor_set(monitor, value, true, false)

/** Some values such as log sequence number are monotonically increasing
number, do not need to record max/min values */
#define MONITOR_SET_SIMPLE(monitor, value) \
  monitor_set(monitor, value, false, false)

/** Reset the monitor value and max/min value to zero. The reset
operation would only be conducted when the counter is turned off */
#define MONITOR_RESET_ALL(monitor) monitor_reset_all(monitor)

inline void monitor_reset_all(monitor_id_t monitor) {
  MONITOR_VALUE(monitor).store(MONITOR_INIT_ZERO_VALUE,
                               std::memory_order_relaxed);
  MONITOR_MAX_VALUE(monitor) = MAX_RESERVED;
  MONITOR_MIN_VALUE(monitor) = MIN_RESERVED;
  MONITOR_VALUE_RESET(monitor) = MONITOR_INIT_ZERO_VALUE;
  MONITOR_MAX_VALUE_START(monitor) = MAX_RESERVED;
  MONITOR_MIN_VALUE_START(monitor) = MIN_RESERVED;
  MONITOR_LAST_VALUE(monitor) = MONITOR_INIT_ZERO_VALUE;
  MONITOR_FIELD(monitor, mon_start_time) = {};
  MONITOR_FIELD(monitor, mon_stop_time) = {};
  MONITOR_FIELD(monitor, mon_reset_time) = {};
}

/** Following four macros defines necessary operations to fetch and
consolidate information from existing system status variables. */

/** Save the passed-in value to mon_start_value field of monitor
counters */
#define MONITOR_SAVE_START(monitor, value)                  \
  do {                                                      \
    MONITOR_CHECK_DEFINED(value);                           \
    (MONITOR_START_VALUE(monitor) =                         \
         (mon_type_t)(value)-MONITOR_VALUE_RESET(monitor)); \
  } while (0)

/** Save the passed-in value to mon_last_value field of monitor
counters */
#define MONITOR_SAVE_LAST(monitor) monitor_save_last(monitor)

inline void monitor_save_last(monitor_id_t monitor) {
  const auto value = MONITOR_VALUE(monitor).load(std::memory_order_relaxed);
  MONITOR_LAST_VALUE(monitor) = value;
  MONITOR_START_VALUE(monitor) += value;
}

/** Set monitor value to the difference of value and mon_start_value
compensated by mon_last_value if accumulated value is required. */
#define MONITOR_SET_DIFF(monitor, value)                                       \
  MONITOR_SET_UPD_MAX_ONLY(monitor, ((value)-MONITOR_VALUE_RESET(monitor) -    \
                                     MONITOR_FIELD(monitor, mon_start_value) + \
                                     MONITOR_FIELD(monitor, mon_last_value)))

/** Get monitor's monitor_info_t by its monitor id (index into the
 innodb_counter_info array
 @return Point to corresponding monitor_info_t, or NULL if no such
 monitor */
monitor_info_t *srv_mon_get_info(
    monitor_id_t monitor_id); /*!< id index into the
                              innodb_counter_info array */
/** Get monitor's name by its monitor id (index into the
 innodb_counter_info array
 @return corresponding monitor name, or NULL if no such
 monitor */
const char *srv_mon_get_name(
    monitor_id_t monitor_id); /*!< id index into the
                              innodb_counter_info array */

/** Turn on/off/reset monitor counters in a module. If module_value
 is NUM_MONITOR then turn on all monitor counters. */
void srv_mon_set_module_control(
    monitor_id_t module_id,   /*!< in: Module ID as in
                              monitor_counter_id. If it is
                              set to NUM_MONITOR, this means
                              we shall turn on all the counters */
    mon_option_t set_option); /*!< in: Turn on/off reset the
                              counter */
/** This function consolidates some existing server counters used
 by "system status variables". These existing system variables do not have
 mechanism to start/stop and reset the counters, so we simulate these
 controls by remembering the corresponding counter values when the
 corresponding monitors are turned on/off/reset, and do appropriate
 mathematics to deduct the actual value. */
void srv_mon_process_existing_counter(
    monitor_id_t monitor_id,  /*!< in: the monitor's ID as in
                              monitor_counter_id */
    mon_option_t set_option); /*!< in: Turn on/off reset the
                              counter */
/** This function is used to calculate the maximum counter value
 since the start of monitor counter
 @return max counter value since start. */
static inline mon_type_t srv_mon_calc_max_since_start(
    monitor_id_t monitor); /*!< in: monitor id */
/** This function is used to calculate the minimum counter value
 since the start of monitor counter
 @return min counter value since start. */
static inline mon_type_t srv_mon_calc_min_since_start(
    monitor_id_t monitor); /*!< in: monitor id*/
/** Reset a monitor, create a new base line with the current monitor
 value. This baseline is recorded by MONITOR_VALUE_RESET(monitor) */
void srv_mon_reset(monitor_id_t monitor); /*!< in: monitor id*/
/** This function resets all values of a monitor counter */
static inline void srv_mon_reset_all(
    monitor_id_t monitor); /*!< in: monitor id*/
/** Turn on monitor counters that are marked as default ON. */
void srv_mon_default_on(void);

#include "srv0mon.ic"
#else /* !UNIV_HOTBACKUP */
#define MONITOR_INC(x) ((void)0)
#define MONITOR_DEC(x) ((void)0)
#endif /* !UNIV_HOTBACKUP */

#define MONITOR_INC_WAIT_STATS_EX(monitor_prefix, monitor_sufix, wait_stats) \
  if ((wait_stats).wait_loops == 0) {                                        \
    MONITOR_INC(monitor_prefix##NO_WAITS##monitor_sufix);                    \
  } else {                                                                   \
    MONITOR_INC(monitor_prefix##WAITS##monitor_sufix);                       \
    MONITOR_INC_VALUE(monitor_prefix##WAIT_LOOPS##monitor_sufix,             \
                      (wait_stats).wait_loops);                              \
  }

#define MONITOR_INC_WAIT_STATS(monitor_prefix, wait_stats) \
  MONITOR_INC_WAIT_STATS_EX(monitor_prefix, , wait_stats);

#endif

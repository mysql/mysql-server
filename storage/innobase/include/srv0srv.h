/*****************************************************************************

Copyright (c) 1995, 2023, Oracle and/or its affiliates.
Copyright (c) 2008, 2009, Google Inc.
Copyright (c) 2009, Percona Inc.

Portions of this file contain modifications contributed and copyrighted by
Google, Inc. Those modifications are gratefully acknowledged and are described
briefly in the InnoDB documentation. The contributions by Google are
incorporated with their permission, and subject to the conditions contained in
the file COPYING.Google.

Portions of this file contain modifications contributed and copyrighted
by Percona Inc.. Those modifications are
gratefully acknowledged and are described briefly in the InnoDB
documentation. The contributions by Percona Inc. are incorporated with
their permission, and subject to the conditions contained in the file
COPYING.Percona.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is also distributed with certain software (including but not
limited to OpenSSL) that is licensed under separate terms, as designated in a
particular file or component or in included license documentation. The authors
of MySQL hereby grant you an additional permission to link the program and
your derivative works with the separately licensed software that they have
included with MySQL.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

/** @file include/srv0srv.h
 The server main program

 Created 10/10/1995 Heikki Tuuri
 *******************************************************/

#ifndef srv0srv_h
#define srv0srv_h

#include "buf0checksum.h"
#include "fil0fil.h"
#include "mysql/psi/mysql_stage.h"
#include "univ.i"

#ifndef UNIV_HOTBACKUP
#include "log0ddl.h"
#include "os0event.h"
#include "os0file.h"
#include "os0thread.h"
#include "que0types.h"
#include "srv0conc.h"
#include "trx0types.h"
#include "ut0counter.h"

#include <future>

/* Global counters used inside InnoDB. */
struct srv_stats_t {
  typedef ib_counter_t<ulint, 64> ulint_ctr_64_t;
  typedef ib_counter_t<lsn_t, 1, single_indexer_t> lsn_ctr_1_t;
  typedef ib_counter_t<ulint, 1, single_indexer_t> ulint_ctr_1_t;
  typedef ib_counter_t<lint, 1, single_indexer_t> lint_ctr_1_t;
  typedef ib_counter_t<int64_t, 1, single_indexer_t> int64_ctr_1_t;

  /** Count the amount of data written in total (in bytes) */
  ulint_ctr_1_t data_written;

  /** Number of the log write requests done */
  ulint_ctr_1_t log_write_requests;

  /** Number of physical writes to the log performed */
  ulint_ctr_1_t log_writes;

  /** Amount of data written to the log files in bytes */
  lsn_ctr_1_t os_log_written;

  /** Number of writes being done to the log files */
  lint_ctr_1_t os_log_pending_writes;

  /** We increase this counter, when we don't have enough
  space in the log buffer and have to flush it */
  ulint_ctr_1_t log_waits;

  /** Count the number of times the doublewrite buffer was flushed */
  ulint_ctr_1_t dblwr_writes;

  /** Store the number of pages that have been flushed to the
  doublewrite buffer */
  ulint_ctr_1_t dblwr_pages_written;

  /** Store the number of write requests issued */
  ulint_ctr_1_t buf_pool_write_requests;

  /** Store the number of times when we had to wait for a free page
  in the buffer pool. It happens when the buffer pool is full and we
  need to make a flush, in order to be able to read or create a page. */
  ulint_ctr_1_t buf_pool_wait_free;

  /** Count the number of pages that were written from buffer
  pool to the disk */
  ulint_ctr_1_t buf_pool_flushed;

  /** Number of buffer pool reads that led to the reading of
  a disk page */
  ulint_ctr_1_t buf_pool_reads;

  /** Number of data read in total (in bytes) */
  ulint_ctr_1_t data_read;

  /** Wait time of database locks */
  int64_ctr_1_t n_lock_wait_time;

  /** Number of database lock waits */
  ulint_ctr_1_t n_lock_wait_count;

  /** Number of threads currently waiting on database locks */
  lint_ctr_1_t n_lock_wait_current_count;

  /** Number of rows read. */
  ulint_ctr_64_t n_rows_read;

  /** Number of rows updated */
  ulint_ctr_64_t n_rows_updated;

  /** Number of rows deleted */
  ulint_ctr_64_t n_rows_deleted;

  /** Number of rows inserted */
  ulint_ctr_64_t n_rows_inserted;

  /** Number of system rows read. */
  ulint_ctr_64_t n_system_rows_read;

  /** Number of system rows updated */
  ulint_ctr_64_t n_system_rows_updated;

  /** Number of system rows deleted */
  ulint_ctr_64_t n_system_rows_deleted;

  /** Number of system rows inserted */
  ulint_ctr_64_t n_system_rows_inserted;

  /** Number of sampled pages read */
  ulint_ctr_64_t n_sampled_pages_read;

  /** Number of sampled pages skipped */
  ulint_ctr_64_t n_sampled_pages_skipped;
};

/** Structure which keeps shared future objects for InnoDB background
threads. One should use these objects to check if threads exited. */
struct Srv_threads {
  /** Monitor thread (prints info). */
  IB_thread m_monitor;

  /** Error monitor thread. */
  IB_thread m_error_monitor;

  /** Redo files governor thread. */
  IB_thread m_log_files_governor;

  /** Redo checkpointer thread. */
  IB_thread m_log_checkpointer;

  /** Redo writer thread. */
  IB_thread m_log_writer;

  /** Redo flusher thread. */
  IB_thread m_log_flusher;

  /** Redo write notifier thread. */
  IB_thread m_log_write_notifier;

  /** Redo flush notifier thread. */
  IB_thread m_log_flush_notifier;

  /** Redo log archiver (used by backup). */
  IB_thread m_backup_log_archiver;

  /** Buffer pool dump thread. */
  IB_thread m_buf_dump;

  /** Buffer pool resize thread. */
  IB_thread m_buf_resize;

  /** Dict stats background thread. */
  IB_thread m_dict_stats;

  /** Thread detecting lock wait timeouts. */
  IB_thread m_lock_wait_timeout;

  /** The master thread. */
  IB_thread m_master;

  /** The ts_alter_encrypt thread. */
  IB_thread m_ts_alter_encrypt;

  /** Thread doing rollbacks during recovery. */
  IB_thread m_trx_recovery_rollback;

  /** Thread writing recovered pages during recovery. */
  IB_thread m_recv_writer;

  /** Purge coordinator (also being a worker) */
  IB_thread m_purge_coordinator;

  /** Number of purge workers and size of array below. */
  size_t m_purge_workers_n;

  /** Purge workers. Note that the m_purge_workers[0] is the same shared
  state as m_purge_coordinator. */
  IB_thread *m_purge_workers;

  /** Page cleaner coordinator (also being a worker). */
  IB_thread m_page_cleaner_coordinator;

  /** Number of page cleaner workers and size of array below. */
  size_t m_page_cleaner_workers_n;

  /** Page cleaner workers. Note that m_page_cleaner_workers[0] is the
  same shared state as m_page_cleaner_coordinator. */
  IB_thread *m_page_cleaner_workers;

  /** Archiver's log archiver (used by Clone). */
  IB_thread m_log_archiver;

  /** Archiver's page archiver (used by Clone). */
  IB_thread m_page_archiver;

  /** Thread doing optimization for FTS index. */
  IB_thread m_fts_optimize;

  /** Thread for GTID persistence */
  IB_thread m_gtid_persister;

#ifdef UNIV_DEBUG
  /** Used in test scenario to delay threads' cleanup until the pre_dd_shutdown
  is ended and final plugin's shutdown is started (when plugin is DELETED).
  Note that you may only delay the shutdown for threads for which there is no
  waiting procedure used in the pre_dd_shutdown. */
  os_event_t m_shutdown_cleanup_dbg;
#endif /* UNIV_DEBUG */

  /** When the master thread notices that shutdown has started (by noticing
  srv_shutdown_state >= SRV_SHUTDOWN_PRE_DD_AND_SYSTEM_TRANSACTIONS), it exits
  its main loop. Then the master thread proceeds with actions related to tasks:
    - which it has been responsible for,
    - and which might depend on DD objects.
  After finishing them, the master thread sets this event.
  @remarks We use this event to wait in srv_pre_dd_shutdown before we enter
  next phase (SRV_SHUTDOWN_PURGE) in which master thread is not allowed to
  use system transactions or touch DD objects. */
  os_event_t m_master_ready_for_dd_shutdown;
};

/** Check if given thread is still active. */
bool srv_thread_is_active(const IB_thread &thread);

/** Check if given thread is cleaned-up and stopped. */
bool srv_thread_is_stopped(const IB_thread &thread);

/** Delay the thread after it discovered that the shutdown_state
is greater or equal to SRV_SHUTDOWN_CLEANUP, before it proceeds
with further clean up. This is used in the tests to see if such
a possible delay does not have impact on the clean shutdown.
@param[in]  wait_for_signal   wait until shutdown phase starts */
void srv_thread_delay_cleanup_if_needed(bool wait_for_signal);

struct Srv_cpu_usage {
  int n_cpu;
  double utime_abs;
  double stime_abs;
  double utime_pct;
  double stime_pct;
};

/** Structure with state of srv background threads. */
extern Srv_threads srv_threads;

/** Structure with cpu usage information. */
extern Srv_cpu_usage srv_cpu_usage;

extern Log_DDL *log_ddl;

#ifdef INNODB_DD_TABLE
extern bool srv_is_upgrade_mode;
extern bool srv_downgrade_logs;
extern bool srv_upgrade_old_undo_found;
#endif /* INNODB_DD_TABLE */

extern bool srv_downgrade_partition_files;

extern const char *srv_main_thread_op_info;

/* The monitor thread waits on this event. */
extern os_event_t srv_monitor_event;

/* The error monitor thread waits on this event. */
extern os_event_t srv_error_event;

/** The buffer pool dump/load thread waits on this event. */
extern os_event_t srv_buf_dump_event;

/** The buffer pool resize thread waits on this event. */
extern os_event_t srv_buf_resize_event;
#endif /* !UNIV_HOTBACKUP */

/** The buffer pool dump/load file name */
#define SRV_BUF_DUMP_FILENAME_DEFAULT "ib_buffer_pool"
extern char *srv_buf_dump_filename;

/** Boolean config knobs that tell InnoDB to dump the buffer pool at shutdown
and/or load it during startup. */
extern bool srv_buffer_pool_dump_at_shutdown;
extern bool srv_buffer_pool_load_at_startup;

/* Whether to disable file system cache if it is defined */
extern bool srv_disable_sort_file_cache;

/** Enable or disable writing of NULLs while extending a tablespace.
If this is false, then the server will just allocate the space without
actually initializing it with NULLs. If the variable is true, the
server will allocate and initialize the space by writing NULLs in it. */
extern bool tbsp_extend_and_initialize;

/* If the last data file is auto-extended, we add this many pages to it
at a time */
#define SRV_AUTO_EXTEND_INCREMENT (srv_sys_space.get_autoextend_increment())

#ifndef UNIV_HOTBACKUP
/** Mutex protecting page_zip_stat_per_index */
extern ib_mutex_t page_zip_stat_per_index_mutex;
/* Mutex for locking srv_monitor_file. Not created if srv_read_only_mode */
extern ib_mutex_t srv_monitor_file_mutex;
/* Temporary file for innodb monitor output */
extern FILE *srv_monitor_file;
/* Mutex for locking srv_misc_tmpfile. Only created if !srv_read_only_mode.
This mutex has a very low rank; threads reserving it should not
acquire any further latches or sleep before releasing this one. */
extern ib_mutex_t srv_misc_tmpfile_mutex;
/* Temporary file for miscellaneous diagnostic output */
extern FILE *srv_misc_tmpfile;
#endif /* !UNIV_HOTBACKUP */

/* Server parameters which are read from the initfile */

extern char *srv_data_home;

/* Number of threads used for initializing rollback segments */
extern uint32_t srv_rseg_init_threads;

/** Number of pages per doublewrite thread/segment */
extern ulong srv_dblwr_pages;

/** Set if InnoDB must operate in read-only mode. We don't do any
recovery and open all tables in RO mode instead of RW mode. We don't
sync the max trx id to disk either. */
extern bool srv_read_only_mode;
/** Set if InnoDB operates in read-only mode or innodb-force-recovery
is greater than SRV_FORCE_NO_TRX_UNDO. */
extern bool high_level_read_only;
/** store to its own file each table created by an user; data
dictionary tables are in the system tablespace 0 */
extern bool srv_file_per_table;
/** Sleep delay for threads waiting to enter InnoDB. In micro-seconds. */
extern ulong srv_thread_sleep_delay;
/** Maximum sleep delay (in micro-seconds), value of 0 disables it.*/
extern ulong srv_adaptive_max_sleep_delay;

/** Sort buffer size in index creation */
extern ulong srv_sort_buf_size;

/** Maximum modification log file size for online index creation */
extern unsigned long long srv_online_max_size;

/** Number of threads to use for parallel reads. */
extern ulong srv_parallel_read_threads;

/* If this flag is true, then we will use the native aio of the
OS (provided we compiled Innobase with it in), otherwise we will
use simulated aio we build below with threads.
Currently we support native aio on windows and linux */
extern bool srv_use_native_aio;
extern bool srv_numa_interleave;

/* The innodb_directories variable value. This a list of directories
deliminated by ';', i.e the FIL_PATH_SEPARATOR. */
extern char *srv_innodb_directories;

/** Server undo tablespaces directory, can be absolute path. */
extern char *srv_undo_dir;

/** Number of undo tablespaces to use. */
extern ulong srv_undo_tablespaces;

/** The number of rollback segments per tablespace */
extern ulong srv_rollback_segments;

/** Maximum size of undo tablespace. */
extern unsigned long long srv_max_undo_tablespace_size;

/** Rate at which UNDO records should be purged. */
extern ulong srv_purge_rseg_truncate_frequency;

/** Enable or Disable Truncate of UNDO tablespace. */
extern bool srv_undo_log_truncate;

/** Enable or disable Encrypt of UNDO tablespace. */
extern bool srv_undo_log_encrypt;

/** Maximum number of recently truncated undo tablespace IDs for
the same undo number. */
extern const size_t CONCURRENT_UNDO_TRUNCATE_LIMIT;

extern char *srv_log_group_home_dir;

/** Enable or Disable Encrypt of REDO tablespace. */
extern bool srv_redo_log_encrypt;

/* Maximum number of redo files of a cloned DB. */
constexpr size_t SRV_N_LOG_FILES_CLONE_MAX = 1000;

/** Value of innodb_log_files_in_group. This is deprecated. */
extern ulong srv_log_n_files;

/** Value of innodb_log_file_size. Expressed in bytes. This is deprecated. */
extern ulonglong srv_log_file_size;

/** Value of innodb_redo_log_capacity. Expressed in bytes. Might be set
during startup automatically when started in "dedicated server mode". */
extern ulonglong srv_redo_log_capacity;

/** Assumed value of innodb_redo_log_capacity's - value which is used.
Expressed in bytes. Might be set during startup automatically when
started in "dedicated server mode". Might also be set during startup
when old sysvar (innodb_log_file_size or innodb_log_files_in_group)
are configured and the new sysvar (innodb_redo_log_capacity) is not. */
extern ulonglong srv_redo_log_capacity_used;

#ifdef UNIV_DEBUG_DEDICATED
extern ulong srv_debug_system_mem_size;
#endif /* UNIV_DEBUG_DEDICATED */

/** Space for log buffer, expressed in bytes. Note, that log buffer
will use only the largest power of two, which is not greater than
the assigned space. */
extern ulong srv_log_buffer_size;

/** When log writer follows links in the log recent written buffer,
it stops when it has reached at least that many bytes to write,
limiting how many bytes can be written in single call. */
extern ulong srv_log_write_max_size;

/** Size of block, used for writing ahead to avoid read-on-write. */
extern ulong srv_log_write_ahead_size;

/** Number of events used for notifications about redo write. */
extern ulong srv_log_write_events;

/** Number of events used for notifications about redo flush. */
extern ulong srv_log_flush_events;

/** Number of slots in a small buffer, which is used to allow concurrent
writes to log buffer. The slots are addressed by LSN values modulo number
of the slots. */
extern ulong srv_log_recent_written_size;

/** Number of slots in a small buffer, which is used to break requirement
for total order of dirty pages, when they are added to flush lists.
The slots are addressed by LSN values modulo number of the slots. */
extern ulong srv_log_recent_closed_size;

/** Whether to activate/pause the log writer threads. */
extern bool srv_log_writer_threads;

/** Minimum absolute value of cpu time for which spin-delay is used. */
extern uint srv_log_spin_cpu_abs_lwm;

/** Maximum percentage of cpu time for which spin-delay is used. */
extern uint srv_log_spin_cpu_pct_hwm;

/** Number of spin iterations, when spinning and waiting for log buffer
written up to given LSN, before we fallback to loop with sleeps.
This is not used when user thread has to wait for log flushed to disk. */
extern ulong srv_log_wait_for_write_spin_delay;

/** Timeout used when waiting for redo write. */
std::chrono::microseconds get_srv_log_wait_for_write_timeout();

/** Number of spin iterations, when spinning and waiting for log flushed. */
extern ulong srv_log_wait_for_flush_spin_delay;

/** Maximum value of average log flush time for which spin-delay is used.
When flushing takes longer, user threads no longer spin when waiting for
flushed redo. Expressed in microseconds. */
extern ulong srv_log_wait_for_flush_spin_hwm;

/** Timeout used when waiting for redo flush. */
std::chrono::microseconds get_srv_log_wait_for_flush_timeout();

/** Number of spin iterations, for which log writer thread is waiting
for new data to write or flush without sleeping. */
extern ulong srv_log_writer_spin_delay;

/** Initial timeout used to wait on writer_event. */
std::chrono::microseconds get_srv_log_writer_timeout();

/** Period every which a periodical checkpoint is written by the log
checkpointer thread (unless periodical checkpoints are disabled,
which is a case during initial phase of startup). */
std::chrono::milliseconds get_srv_log_checkpoint_every();

/** Number of spin iterations, for which log flusher thread is waiting
for new data to flush, without sleeping. */
extern ulong srv_log_flusher_spin_delay;

/** Initial timeout used to wait on flusher_event. */
std::chrono::microseconds get_srv_log_flusher_timeout();

/** Number of spin iterations, for which log write notifier thread is waiting
for advanced writeed_to_disk_lsn without sleeping. */
extern ulong srv_log_write_notifier_spin_delay;

/** Initial timeout used to wait on write_notifier_event. */
std::chrono::microseconds get_srv_log_write_notifier_timeout();

/** Number of spin iterations, for which log flush notifier thread is waiting
for advanced flushed_to_disk_lsn without sleeping. */
extern ulong srv_log_flush_notifier_spin_delay;

/** Initial timeout used to wait on flush_notifier_event. */
std::chrono::microseconds get_srv_log_flush_notifier_timeout();

/** Whether to generate and require checksums on the redo log pages. */
extern bool srv_log_checksums;

#ifdef UNIV_DEBUG

/** If true then disable checkpointing. */
extern bool srv_checkpoint_disabled;

/* Used to inject a failure to find a free rollback segment. */
extern bool srv_inject_too_many_concurrent_trxs;

#endif /* UNIV_DEBUG */

extern ulong srv_flush_log_at_trx_commit;
extern ulong srv_log_write_ahead_size;
std::chrono::seconds get_srv_flush_log_at_timeout();
extern bool srv_adaptive_flushing;
extern bool srv_flush_sync;

/* If this flag is true, then we will load the indexes' (and tables') metadata
even if they are marked as "corrupted". Mostly it is for DBA to process
corrupted index and table */
extern bool srv_load_corrupted;

/** Dedicated server setting */
extern bool srv_dedicated_server;
/** Requested size in bytes */
extern ulint srv_buf_pool_size;
/** Minimum pool size in bytes */
extern const ulint srv_buf_pool_min_size;
/** Default pool size in bytes */
extern const ulint srv_buf_pool_def_size;
/** Maximum pool size in bytes */
extern const longlong srv_buf_pool_max_size;
/** Requested buffer pool chunk size. Each buffer pool instance consists
of one or more chunks. */
extern ulonglong srv_buf_pool_chunk_unit;
/** Minimum buffer pool chunk size. */
extern const ulonglong srv_buf_pool_chunk_unit_min;
/** The buffer pool chunk size must be a multiple of this number. */
extern const ulonglong srv_buf_pool_chunk_unit_blk_sz;
/** Maximum buffer pool chunk size. */
extern const ulonglong srv_buf_pool_chunk_unit_max;

/** Requested number of buffer pool instances */
extern ulong srv_buf_pool_instances;
/** Default number of buffer pool instances */
extern const ulong srv_buf_pool_instances_default;
/** Number of locks to protect buf_pool->page_hash */
extern ulong srv_n_page_hash_locks;
/** Whether to validate InnoDB tablespace paths on startup */
extern bool srv_validate_tablespace_paths;
/** Use fdatasync() instead of fsync(). */
extern bool srv_use_fdatasync;
/** Scan depth for LRU flush batch i.e.: number of blocks scanned*/
extern ulong srv_LRU_scan_depth;
/** Whether or not to flush neighbors of a block */
extern ulong srv_flush_neighbors;
/** Previously requested size. Accesses protected by memory barriers. */
extern ulint srv_buf_pool_old_size;
/** Current size as scaling factor for the other components */
extern ulint srv_buf_pool_base_size;
/** Current size in bytes */
extern long long srv_buf_pool_curr_size;
/** Dump this % of each buffer pool during BP dump */
extern ulong srv_buf_pool_dump_pct;
/** Lock table size in bytes */
extern ulint srv_lock_table_size;

extern bool srv_random_read_ahead;
extern ulong srv_read_ahead_threshold;
extern ulong srv_n_read_io_threads;
extern ulong srv_n_write_io_threads;

extern uint srv_change_buffer_max_size;

/** Default value of srv_idle_flush_pct */
extern const ulong srv_idle_flush_pct_default;
/** How much flush to be done in case of server is idle */
extern ulong srv_idle_flush_pct;

/* Number of IO operations per second the server can do */
extern ulong srv_io_capacity;

/* We use this dummy default value at startup for max_io_capacity.
The real value is set based on the value of io_capacity. */
constexpr uint32_t SRV_MAX_IO_CAPACITY_DUMMY_DEFAULT = ~0U;
constexpr uint32_t SRV_MAX_IO_CAPACITY_LIMIT = ~0U;
extern ulong srv_max_io_capacity;
/* Returns the number of IO operations that is X percent of the
capacity. PCT_IO(5) -> returns the number of IO operations that
is 5% of the max where max is srv_io_capacity.  */
static inline ulong PCT_IO(ulong p) {
  return (ulong)(srv_io_capacity * ((double)(p) / 100.0));
}

/** Maximum number of purge threads, including the purge coordinator */
constexpr uint32_t MAX_PURGE_THREADS = 32;

/* The "innodb_stats_method" setting, decides how InnoDB is going
to treat NULL value when collecting statistics. It is not defined
as enum type because the configure option takes unsigned integer type. */
extern ulong srv_innodb_stats_method;

/** Returns current value of the "innodb_open_files" configuration variable. */
long innobase_get_open_files_limit();
/** Sets new value of the "innodb_open_files" configuration variable to present
to users.
@param[in] new_limit new limit to be set. */
void innobase_set_open_files_limit(long new_limit);

extern ulong srv_fatal_semaphore_wait_threshold;
extern uint srv_flush_log_at_timeout;
extern bool srv_btr_search_enabled;
extern ulong srv_replication_delay;

extern ulong srv_log_wait_for_write_timeout;
extern ulong srv_log_wait_for_flush_timeout;
extern ulong srv_log_writer_timeout;
extern ulong srv_log_checkpoint_every;
extern ulong srv_log_flusher_timeout;
extern ulong srv_log_write_notifier_timeout;
extern ulong srv_log_flush_notifier_timeout;

extern uint buf_LRU_old_threshold;

extern ulong srv_n_page_cleaners;

extern double srv_max_dirty_pages_pct;
extern double srv_max_dirty_pages_pct_lwm;

extern ulong srv_adaptive_flushing_lwm;
extern ulong srv_flushing_avg_loops;

extern ulong srv_force_recovery;
#ifdef UNIV_DEBUG
extern ulong srv_force_recovery_crash;
#endif /* UNIV_DEBUG */

/** The value of the configuration parameter innodb_fast_shutdown,
controlling the InnoDB shutdown.

If innodb_fast_shutdown=0, InnoDB shutdown will purge all undo log
records (except XA PREPARE transactions) and complete the merge of the
entire change buffer, and then shut down the redo log.

If innodb_fast_shutdown=1, InnoDB shutdown will only flush the buffer
pool to data files, cleanly shutting down the redo log.

If innodb_fast_shutdown=2, shutdown will effectively 'crash' InnoDB
(but lose no committed transactions). */
extern ulong srv_fast_shutdown;
extern bool srv_innodb_status;

extern unsigned long long srv_stats_transient_sample_pages;
extern bool srv_stats_persistent;
extern unsigned long long srv_stats_persistent_sample_pages;
extern bool srv_stats_auto_recalc;
extern bool srv_stats_include_delete_marked;

extern ulong srv_checksum_algorithm;

extern double srv_max_buf_pool_modified_pct;
extern ulong srv_max_purge_lag;
extern ulong srv_max_purge_lag_delay;

std::chrono::milliseconds get_srv_replication_delay();
/*-------------------------------------------*/

extern bool srv_print_innodb_monitor;
/** In contrast to srv_print_innodb_monitor which is controlled by the user,
this variable is controlled by InnoDB itself: if some module of InnoDB decides
it would be good to print the monitoring information it increments this value,
and decrements it when it no longer needs it. */
extern std::atomic_uint32_t srv_innodb_needs_monitoring;
extern bool srv_print_innodb_lock_monitor;

extern ulong srv_n_spin_wait_rounds;
extern ulong srv_n_free_tickets_to_enter;
extern ulong srv_spin_wait_delay;
extern bool srv_priority_boost;

extern ulint srv_truncated_status_writes;

#if defined UNIV_DEBUG || defined UNIV_IBUF_DEBUG
extern bool srv_ibuf_disable_background_merge;
#endif /* UNIV_DEBUG || UNIV_IBUF_DEBUG */

#ifdef UNIV_DEBUG
extern bool srv_buf_pool_debug;
extern bool srv_sync_debug;
extern bool srv_purge_view_update_only_debug;

/** Value of MySQL global used to disable master thread. */
extern bool srv_master_thread_disabled_debug;
#endif /* UNIV_DEBUG */

std::chrono::seconds get_srv_fatal_semaphore_wait_threshold();
extern std::atomic<int> srv_fatal_semaphore_wait_extend;

extern ulint srv_dml_needed_delay;

#ifdef UNIV_HOTBACKUP
// MAHI: changed from 130 to 1 assuming the apply-log is single threaded
constexpr uint32_t SRV_MAX_N_IO_THREADS = 1;
#else  /* UNIV_HOTBACKUP */
constexpr uint32_t SRV_MAX_N_IO_THREADS = 130;
#endif /* UNIV_HOTBACKUP */

/* Array of English strings describing the current state of an
i/o handler thread */
extern const char *srv_io_thread_op_info[];
extern const char *srv_io_thread_function[];

/* the number of purge threads to use from the worker pool (currently 0 or 1) */
extern ulong srv_n_purge_threads;

/* the number of pages to purge in one batch */
extern ulong srv_purge_batch_size;

/* the number of sync wait arrays */
extern ulong srv_sync_array_size;

/* print all user-level transactions deadlocks to mysqld stderr */
extern bool srv_print_all_deadlocks;

/** Print all DDL logs to mysqld stderr */
extern bool srv_print_ddl_logs;

extern bool srv_cmp_per_index_enabled;

extern bool srv_redo_log;

/** Status variables to be passed to MySQL */
extern struct export_var_t export_vars;

#ifndef UNIV_HOTBACKUP
/** Global counters */
extern srv_stats_t srv_stats;

/* Keys to register InnoDB threads with performance schema */

#ifdef UNIV_PFS_THREAD
extern mysql_pfs_key_t log_archiver_thread_key;
extern mysql_pfs_key_t page_archiver_thread_key;
extern mysql_pfs_key_t buf_dump_thread_key;
extern mysql_pfs_key_t buf_resize_thread_key;
extern mysql_pfs_key_t clone_ddl_thread_key;
extern mysql_pfs_key_t clone_gtid_thread_key;
extern mysql_pfs_key_t ddl_thread_key;
extern mysql_pfs_key_t dict_stats_thread_key;
extern mysql_pfs_key_t fts_optimize_thread_key;
extern mysql_pfs_key_t fts_parallel_merge_thread_key;
extern mysql_pfs_key_t fts_parallel_tokenization_thread_key;
extern mysql_pfs_key_t io_ibuf_thread_key;
extern mysql_pfs_key_t io_read_thread_key;
extern mysql_pfs_key_t io_write_thread_key;
extern mysql_pfs_key_t log_writer_thread_key;
extern mysql_pfs_key_t log_files_governor_thread_key;
extern mysql_pfs_key_t log_checkpointer_thread_key;
extern mysql_pfs_key_t log_flusher_thread_key;
extern mysql_pfs_key_t log_write_notifier_thread_key;
extern mysql_pfs_key_t log_flush_notifier_thread_key;
extern mysql_pfs_key_t page_flush_coordinator_thread_key;
extern mysql_pfs_key_t page_flush_thread_key;
extern mysql_pfs_key_t recv_writer_thread_key;
extern mysql_pfs_key_t srv_error_monitor_thread_key;
extern mysql_pfs_key_t srv_lock_timeout_thread_key;
extern mysql_pfs_key_t srv_master_thread_key;
extern mysql_pfs_key_t srv_monitor_thread_key;
extern mysql_pfs_key_t srv_purge_thread_key;
extern mysql_pfs_key_t srv_worker_thread_key;
extern mysql_pfs_key_t trx_recovery_rollback_thread_key;
extern mysql_pfs_key_t srv_ts_alter_encrypt_thread_key;
extern mysql_pfs_key_t parallel_read_thread_key;
extern mysql_pfs_key_t parallel_rseg_init_thread_key;
#endif /* UNIV_PFS_THREAD */
#endif /* !UNIV_HOTBACKUP */

#ifdef HAVE_PSI_STAGE_INTERFACE
/** Performance schema stage event for monitoring ALTER TABLE progress
everything after flush log_make_latest_checkpoint(). */
extern PSI_stage_info srv_stage_alter_table_end;

/** Performance schema stage event for monitoring ALTER TABLE progress
log_make_latest_checkpoint(). */
extern PSI_stage_info srv_stage_alter_table_flush;

/** Performance schema stage event for monitoring ALTER TABLE progress
row_merge_insert_index_tuples(). */
extern PSI_stage_info srv_stage_alter_table_insert;

/** Performance schema stage event for monitoring ALTER TABLE progress
row_log_apply(). */
extern PSI_stage_info srv_stage_alter_table_log_index;

/** Performance schema stage event for monitoring ALTER TABLE progress
row_log_table_apply(). */
extern PSI_stage_info srv_stage_alter_table_log_table;

/** Performance schema stage event for monitoring ALTER TABLE progress
row_merge_sort(). */
extern PSI_stage_info srv_stage_alter_table_merge_sort;

/** Performance schema stage event for monitoring ALTER TABLE progress
row_merge_read_clustered_index(). */
extern PSI_stage_info srv_stage_alter_table_read_pk_internal_sort;

/** Performance schema stage event for monitoring ALTER TABLESPACE
ENCRYPTION progress. */
extern PSI_stage_info srv_stage_alter_tablespace_encryption;

/** Performance schema stage event for monitoring buffer pool load progress. */
extern PSI_stage_info srv_stage_buffer_pool_load;
#endif /* HAVE_PSI_STAGE_INTERFACE */

/** Performance schema stage event for monitoring clone file copy progress. */
extern PSI_stage_info srv_stage_clone_file_copy;

/** Performance schema stage event for monitoring clone redo copy progress. */
extern PSI_stage_info srv_stage_clone_redo_copy;

/** Performance schema stage event for monitoring clone page copy progress. */
extern PSI_stage_info srv_stage_clone_page_copy;

#ifndef _WIN32
/** Alternatives for the file flush option in Unix.
@see innodb_flush_method_names */
enum srv_unix_flush_t {
  SRV_UNIX_FSYNC = 0,  /*!< fsync, the default */
  SRV_UNIX_O_DSYNC,    /*!< open log files in O_SYNC mode */
  SRV_UNIX_LITTLESYNC, /*!< do not call os_file_flush()
                       when writing data files, but do flush
                       after writing to log files */
  SRV_UNIX_NOSYNC,     /*!< do not flush after writing */
  SRV_UNIX_O_DIRECT,   /*!< invoke os_file_set_nocache() on
                       data files. This implies using
                       non-buffered IO but still using fsync,
                       the reason for which is that some FS
                       do not flush meta-data when
                       unbuffered IO happens */
  SRV_UNIX_O_DIRECT_NO_FSYNC
  /*!< do not use fsync() when using
  direct IO i.e.: it can be set to avoid
  the fsync() call that we make when
  using SRV_UNIX_O_DIRECT. However, in
  this case user/DBA should be sure about
  the integrity of the meta-data */
};
extern enum srv_unix_flush_t srv_unix_file_flush_method;

inline bool srv_is_direct_io() {
  return (srv_unix_file_flush_method == SRV_UNIX_O_DIRECT ||
          srv_unix_file_flush_method == SRV_UNIX_O_DIRECT_NO_FSYNC);
}

#else
/** Alternatives for file i/o in Windows. @see innodb_flush_method_names. */
enum srv_win_flush_t {
  /** unbuffered I/O; this is the default */
  SRV_WIN_IO_UNBUFFERED = 0,
  /** buffered I/O */
  SRV_WIN_IO_NORMAL,
};
extern enum srv_win_flush_t srv_win_file_flush_method;

inline bool srv_is_direct_io() {
  return (srv_win_file_flush_method == SRV_WIN_IO_UNBUFFERED);
}

#endif /* _WIN32 */

/** Alternatives for srv_force_recovery. Non-zero values are intended
to help the user get a damaged database up so that he can dump intact
tables and rows with SELECT INTO OUTFILE. The database must not otherwise
be used with these options! A bigger number below means that all precautions
of lower numbers are included. */
enum {
  SRV_FORCE_IGNORE_CORRUPT = 1,   /*!< let the server run even if it
                                  detects a corrupt page */
  SRV_FORCE_NO_BACKGROUND = 2,    /*!< prevent the main thread from
                                  running: if a crash would occur
                                  in purge, this prevents it */
  SRV_FORCE_NO_TRX_UNDO = 3,      /*!< do not run trx rollback after
                                  recovery */
  SRV_FORCE_NO_IBUF_MERGE = 4,    /*!< prevent also ibuf operations:
                                  if they would cause a crash, better
                                  not do them */
  SRV_FORCE_NO_UNDO_LOG_SCAN = 5, /*!< do not look at undo logs when
                                  starting the database: InnoDB will
                                  treat even incomplete transactions
                                  as committed */
  SRV_FORCE_NO_LOG_REDO = 6       /*!< do not do the log roll-forward
                                  in connection with recovery */
};

/* Alternatives for srv_innodb_stats_method, which could be changed by
setting innodb_stats_method */
enum srv_stats_method_name_enum {
  SRV_STATS_NULLS_EQUAL,   /* All NULL values are treated as
                           equal. This is the default setting
                           for innodb_stats_method */
  SRV_STATS_NULLS_UNEQUAL, /* All NULL values are treated as
                           NOT equal. */
  SRV_STATS_NULLS_IGNORED  /* NULL values are ignored */
};

typedef enum srv_stats_method_name_enum srv_stats_method_name_t;

#ifdef UNIV_DEBUG
/** Force all user tables to use page compression. */
extern ulong srv_debug_compress;
#endif /* UNIV_DEBUG */

/** Types of threads existing in the system. */
enum srv_thread_type {
  SRV_NONE,   /*!< None */
  SRV_WORKER, /*!< threads serving parallelized
              queries and queries released from
              lock wait */
  SRV_PURGE,  /*!< Purge coordinator thread */
  SRV_MASTER  /*!< the master thread, (whose type
              number must be biggest) */
};

/** Boots Innobase server. */
void srv_boot(void);
/** Frees the data structures created in srv_init(). */
void srv_free(void);

/** Sets the info describing an i/o thread current state.
@param[in] i The 'segment' of the i/o thread
@param[in] str Constant char string describing the state */
void srv_set_io_thread_op_info(ulint i, const char *str);

/** Resets the info describing an i/o thread current state. */
void srv_reset_io_thread_op_info();
/** Tells the purge thread that there has been activity in the database
 and wakes up the purge thread if it is suspended (not sleeping).  Note
 that there is a small chance that the purge thread stays suspended
 (we do not protect our operation with the srv_sys_t:mutex, for
 performance reasons). */
void srv_wake_purge_thread_if_not_active(void);
/** Tells the Innobase server that there has been activity in the database
 and wakes up the master thread if it is suspended (not sleeping). Used
 in the MySQL interface. Note that there is a small chance that the master
 thread stays suspended (we do not protect our operation with the kernel
 mutex, for performance reasons). */
void srv_active_wake_master_thread_low(void);
static inline void srv_active_wake_master_thread() {
  if (!srv_read_only_mode) {
    srv_active_wake_master_thread_low();
  }
}
/** Wakes up the master thread if it is suspended or being suspended. */
void srv_wake_master_thread(void);
#ifndef UNIV_HOTBACKUP
/** Outputs to a file the output of the InnoDB Monitor.
@param[in]    file      output stream
@param[in]    nowait    whether to wait for the exclusive global lock_sys latch
@param[out]   trx_start file position of the start of the list of active
                        transactions
@param[out]   trx_end   file position of the end of the list of active
                        transactions
@return false if not all information printed due to failure to obtain necessary
        mutex */
bool srv_printf_innodb_monitor(FILE *file, bool nowait, ulint *trx_start,
                               ulint *trx_end);

/** Function to pass InnoDB status variables to MySQL */
void srv_export_innodb_status(void);
/** Get current server activity count. We don't hold srv_sys::mutex while
 reading this value as it is only used in heuristics.
 @return activity count. */
ulint srv_get_activity_count(void);
/** Check if there has been any activity.
 @return false if no change in activity counter. */
bool srv_check_activity(ulint old_activity_count); /*!< old activity count */
/** Increment the server activity counter. */
void srv_inc_activity_count(void);

/** Enqueues a task to server task queue and releases a worker thread, if there
is a suspended one. */
void srv_que_task_enqueue_low(que_thr_t *thr); /*!< in: query thread */

/** A thread which prints the info output by various InnoDB monitors. */
void srv_monitor_thread();

/** A thread which prints warnings about semaphore waits which have lasted
too long. These can be used to track bugs which cause hangs. */
void srv_error_monitor_thread();

/** The master thread controlling the server. */
void srv_master_thread();

/** Purge coordinator thread that schedules the purge tasks. */
void srv_purge_coordinator_thread();

/** Worker thread that reads tasks from the work queue and executes them. */
void srv_worker_thread();

/** Set encryption for UNDO tablespace with given space id.
@param[in] space_id     Undo tablespace id
@param[in] mtr          Mini-transaction
@return false for success, true otherwise */
bool set_undo_tablespace_encryption(space_id_t space_id, mtr_t *mtr);

/** Enable UNDO tablespaces encryption.
@return false for success, true otherwise. */
bool srv_enable_undo_encryption();

/** Enable REDO log encryption.
@return false for success, true otherwise. */
bool srv_enable_redo_encryption();

/** Get count of tasks in the queue.
 @return number of tasks in queue */
ulint srv_get_task_queue_length(void);

/** Releases threads of the type given from suspension in the thread table.
 NOTE! The server mutex has to be reserved by the caller!
 @return number of threads released: this may be less than n if not
 enough threads were suspended at the moment */
ulint srv_release_threads(enum srv_thread_type type, /*!< in: thread type */
                          ulint n); /*!< in: number of threads to release */

/** Check whether the master thread is active.
This is polled during the final phase of shutdown.
The first phase of server shutdown must have already been executed
(or the server must not have been fully started up).
@see srv_pre_dd_shutdown()
@retval true   if any thread is active
@retval false  if no thread is active */
bool srv_master_thread_is_active();

/** Wakeup the purge threads. */
void srv_purge_wakeup(void);

/** Check if the purge threads are active, both coordinator and worker threads
@return true if any thread is active, false if no thread is active */
bool srv_purge_threads_active();

/** Create an undo tablespace with an explicit file name
This is called during CREATE UNDO TABLESPACE.
@param[in]  space_name  tablespace name
@param[in]  file_name   file name
@param[in]  space_id    Tablespace ID
@return DB_SUCCESS or error code */
dberr_t srv_undo_tablespace_create(const char *space_name,
                                   const char *file_name, space_id_t space_id);

/** Initialize undo::spaces and trx_sys_undo_spaces,
called once during srv_start(). */
void undo_spaces_init();

/** Free the resources occupied by undo::spaces and trx_sys_undo_spaces,
called once during thread de-initialization. */
void undo_spaces_deinit();

/** Set redo log variable for performance schema global status.
@param[in]      enable  true => redo log enabled, false => redo log disabled */
void set_srv_redo_log(bool enable);

#ifdef UNIV_DEBUG
struct SYS_VAR;

/** Disables master thread. It's used by:
        SET GLOBAL innodb_master_thread_disabled_debug = 1 (0).
@param[in]      thd             thread handle
@param[in]      var             pointer to system variable
@param[out]     var_ptr         where the formal string goes
@param[in]      save            immediate result from check function */
void srv_master_thread_disabled_debug_update(THD *thd, SYS_VAR *var,
                                             void *var_ptr, const void *save);
#endif /* UNIV_DEBUG */
#endif /* !UNIV_HOTBACKUP */

/** Status variables to be passed to MySQL */
struct export_var_t {
  ulint innodb_data_pending_reads;  /*!< Pending reads */
  ulint innodb_data_pending_writes; /*!< Pending writes */
  ulint innodb_data_pending_fsyncs; /*!< Pending fsyncs */
  ulint innodb_data_fsyncs;         /*!< Number of fsyncs so far */
  ulint innodb_data_read;           /*!< Data bytes read */
  ulint innodb_data_writes;         /*!< I/O write requests */
  ulint innodb_data_written;        /*!< Data bytes written */
  ulint innodb_data_reads;          /*!< I/O read requests */
  char innodb_buffer_pool_dump_status[OS_FILE_MAX_PATH +
                                      128]; /*!< Buf pool dump status */
  char innodb_buffer_pool_load_status[OS_FILE_MAX_PATH +
                                      128];   /*!< Buf pool load status */
  char innodb_buffer_pool_resize_status[512]; /*!< Buf pool resize status */
  uint32_t
      innodb_buffer_pool_resize_status_code; /*!< Buf pool resize status code */
  uint32_t innodb_buffer_pool_resize_status_progress; /*!< Buf pool resize
                                                     status progess */
  ulint innodb_buffer_pool_pages_total;               /*!< Buffer pool size */
  ulint innodb_buffer_pool_pages_data;                /*!< Data pages */
  ulint innodb_buffer_pool_bytes_data;                /*!< File bytes used */
  ulint innodb_buffer_pool_pages_dirty;               /*!< Dirty data pages */
  ulint innodb_buffer_pool_bytes_dirty; /*!< File bytes modified */
  ulint innodb_buffer_pool_pages_misc;  /*!< Miscellaneous pages */
  ulint innodb_buffer_pool_pages_free;  /*!< Free pages */
#ifdef UNIV_DEBUG
  ulint innodb_buffer_pool_pages_latched;  /*!< Latched pages */
#endif                                     /* UNIV_DEBUG */
  ulint innodb_buffer_pool_read_requests;  /*!< buf_pool->stat.n_page_gets */
  ulint innodb_buffer_pool_reads;          /*!< srv_buf_pool_reads */
  ulint innodb_buffer_pool_wait_free;      /*!< srv_buf_pool_wait_free */
  ulint innodb_buffer_pool_pages_flushed;  /*!< srv_buf_pool_flushed */
  ulint innodb_buffer_pool_write_requests; /*!< srv_buf_pool_write_requests */
  ulint innodb_buffer_pool_read_ahead_rnd; /*!< srv_read_ahead_rnd */
  ulint innodb_buffer_pool_read_ahead;     /*!< srv_read_ahead */
  ulint innodb_buffer_pool_read_ahead_evicted; /*!< srv_read_ahead evicted*/
  ulint innodb_dblwr_pages_written;            /*!< srv_dblwr_pages_written */
  ulint innodb_dblwr_writes;                   /*!< srv_dblwr_writes */
  char innodb_redo_log_resize_status[512];     /*!< Redo log resize status */
  bool innodb_redo_log_read_only;              /*!< Is redo log read-only ? */
  ulonglong innodb_redo_log_uuid;              /*!< Redo log UUID */
  ulonglong innodb_redo_log_checkpoint_lsn;    /*!< Redo log checkpoint LSN */
  ulonglong innodb_redo_log_current_lsn;       /*!< Redo log current LSN */
  ulonglong
      innodb_redo_log_flushed_to_disk_lsn; /*!< Redo log flushed to disk LSN */
  ulonglong innodb_redo_log_logical_size;  /*!< Redo log logical size */
  ulonglong innodb_redo_log_physical_size; /*!< Redo log physical size */
  ulonglong innodb_redo_log_capacity_resized; /*!< Redo log capacity after
                                              the last finished redo resize */
  ulint innodb_log_waits;                     /*!< srv_log_waits */
  ulint innodb_log_write_requests;            /*!< srv_log_write_requests */
  ulint innodb_log_writes;                    /*!< srv_log_writes */
  lsn_t innodb_os_log_written;                /*!< srv_os_log_written */
  ulint innodb_os_log_fsyncs;                 /*!< log_total_flushes() */
  ulint innodb_os_log_pending_writes;         /*!< srv_os_log_pending_writes */
  ulint innodb_os_log_pending_fsyncs;         /*!< log_pending_flushes() */
  ulint innodb_page_size;                     /*!< UNIV_PAGE_SIZE */
  ulint innodb_pages_created;          /*!< buf_pool->stat.n_pages_created */
  ulint innodb_pages_read;             /*!< buf_pool->stat.n_pages_read */
  ulint innodb_pages_written;          /*!< buf_pool->stat.n_pages_written */
  bool innodb_redo_log_enabled;        /*!< srv_redo_log */
  ulint innodb_row_lock_waits;         /*!< srv_n_lock_wait_count */
  ulint innodb_row_lock_current_waits; /*!< srv_n_lock_wait_current_count */
  int64_t innodb_row_lock_time;        /*!< srv_n_lock_wait_time
                                       / 1000 */
  ulint innodb_row_lock_time_avg;      /*!< srv_n_lock_wait_time
                                       / 1000
                                       / srv_n_lock_wait_count */
  ulint innodb_row_lock_time_max;      /*!< srv_n_lock_max_wait_time
                                       / 1000 */
  ulint innodb_rows_read;              /*!< srv_n_rows_read */
  ulint innodb_rows_inserted;          /*!< srv_n_rows_inserted */
  ulint innodb_rows_updated;           /*!< srv_n_rows_updated */
  ulint innodb_rows_deleted;           /*!< srv_n_rows_deleted */
  ulint innodb_system_rows_read;       /*!< srv_n_system_rows_read */
  ulint innodb_system_rows_inserted;   /*!< srv_n_system_rows_inserted */
  ulint innodb_system_rows_updated;    /*!< srv_n_system_rows_updated */
  ulint innodb_system_rows_deleted;    /*!< srv_n_system_rows_deleted*/
  ulint innodb_sampled_pages_read;
  ulint innodb_sampled_pages_skipped;
  ulint innodb_num_open_files;            /*!< fil_n_files_open */
  ulint innodb_truncated_status_writes;   /*!< srv_truncated_status_writes */
  ulint innodb_undo_tablespaces_total;    /*!< total number of undo tablespaces
                                          innoDB is tracking. */
  ulint innodb_undo_tablespaces_implicit; /*!< number of undo tablespaces
                                          innoDB created implicitly. */
  ulint innodb_undo_tablespaces_explicit; /*!< number of undo tablespaces
                                          the dba created explicitly. */
  ulint innodb_undo_tablespaces_active;   /*!< number of active undo
                                          tablespaces */
#ifdef UNIV_DEBUG
  ulint innodb_purge_trx_id_age;      /*!< rw_max_trx_no - purged trx_no */
  ulint innodb_purge_view_trx_id_age; /*!< rw_max_trx_no
                                      - purged view's min trx_no */
  ulint innodb_ahi_drop_lookups;      /*!< number of adaptive hash
                                      index lookups when freeing
                                      file pages */
#endif                                /* UNIV_DEBUG */
};

#ifndef UNIV_HOTBACKUP
/** Thread slot in the thread table.  */
struct srv_slot_t {
  /** Thread type: user, utility etc. */
  srv_thread_type type;

  /** true if this slot is in use. */
  bool in_use;

  /** true if the thread is waiting for the event of this slot. */
  bool suspended;

  /** Time when the thread was suspended. Initialized by
  lock_wait_table_reserve_slot() for lock wait. */
  std::chrono::steady_clock::time_point suspend_time;

  /** Stores the current value of lock_wait_table_reservations, when
  lock_wait_table_reserve_slot is called.
  This can be used as a version number to avoid ABA problems.
  The difference lock_wait_table_reservations - reservation_no tells us how many
  other threads got suspended while our thr was sleeping.
  This can be used to determine if the wait was unfairly long, and it is time to
  boost trx->lock.schedule_weight.
  Protected by lock->wait_mutex. */
  uint64_t reservation_no;

  /** Wait time that if exceeded the thread will be timed out.
  Initialized by lock_wait_table_reserve_slot() for lock wait. */
  std::chrono::steady_clock::duration wait_timeout;

  /** Event used in suspending the thread when it has nothing to do. */
  os_event_t event;

  /** Suspended query thread (only used for user threads). */
  que_thr_t *thr;
};
#endif /* !UNIV_HOTBACKUP */

#endif

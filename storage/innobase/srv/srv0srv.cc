/*****************************************************************************

Copyright (c) 1995, 2018, Oracle and/or its affiliates. All Rights Reserved.
Copyright (c) 2008, 2009 Google Inc.
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
the terms of the GNU General Public License as published by the Free Software
Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA

*****************************************************************************/

/**************************************************//**
@file srv/srv0srv.cc
The database server main program

Created 10/8/1995 Heikki Tuuri
*******************************************************/

#include "my_global.h"
#include "my_thread.h"

#include "mysql/psi/mysql_stage.h"
#include "mysql/psi/psi.h"
#include "sql_thd_internal_api.h"

#include "ha_prototypes.h"

#include "btr0sea.h"
#include "buf0flu.h"
#include "buf0lru.h"
#include "dict0boot.h"
#include "dict0load.h"
#include "dict0stats_bg.h"
#include "fsp0sysspace.h"
#include "ibuf0ibuf.h"
#include "lock0lock.h"
#include "log0recv.h"
#include "mem0mem.h"
#include "os0proc.h"
#include "pars0pars.h"
#include "que0que.h"
#include "row0mysql.h"
#include "row0trunc.h"
#include "srv0mon.h"
#include "srv0srv.h"
#include "srv0start.h"
#include "sync0sync.h"
#include "trx0i_s.h"
#include "trx0purge.h"
#include "usr0sess.h"
#include "ut0crc32.h"
#include "ut0mem.h"

#ifndef UNIV_PFS_THREAD
#define create_thd(x,y,z,PFS_KEY)	create_thd(x,y,z,PFS_NOT_INSTRUMENTED.m_value)
#endif /* UNIV_PFS_THREAD */

/* The following is the maximum allowed duration of a lock wait. */
ulint	srv_fatal_semaphore_wait_threshold = 600;

/* How much data manipulation language (DML) statements need to be delayed,
in microseconds, in order to reduce the lagging of the purge thread. */
ulint	srv_dml_needed_delay = 0;

ibool	srv_monitor_active = FALSE;
ibool	srv_error_monitor_active = FALSE;

ibool	srv_buf_dump_thread_active = FALSE;

bool	srv_buf_resize_thread_active = false;

ibool	srv_dict_stats_thread_active = FALSE;

const char*	srv_main_thread_op_info = "";

/** Prefix used by MySQL to indicate pre-5.1 table name encoding */
const char		srv_mysql50_table_name_prefix[10] = "#mysql50#";

/* Server parameters which are read from the initfile */

/* The following three are dir paths which are catenated before file
names, where the file name itself may also contain a path */

char*	srv_data_home	= NULL;

/** Rollback files directory, can be absolute. */
char*	srv_undo_dir = NULL;

/** The number of tablespaces to use for rollback segments. */
ulong	srv_undo_tablespaces = 0;

/** The number of UNDO tablespaces that are open and ready to use. */
ulint	srv_undo_tablespaces_open = 0;

/** The number of UNDO tablespaces that are active (hosting some rollback
segment). It is quite possible that some of the tablespaces doesn't host
any of the rollback-segment based on configuration used. */
ulint	srv_undo_tablespaces_active = 0;

/* The number of rollback segments to use */
ulong	srv_rollback_segments = 1;

/* Used for the deprecated setting innodb_undo_logs. This will still get
put into srv_rollback_segments if it is set to a non-default value. */
ulong	srv_undo_logs = 0;
const char* deprecated_undo_logs =
	"The parameter innodb_undo_logs is deprecated"
	" and may be removed in future releases."
	" Please use innodb_rollback_segments instead."
	" See " REFMAN "innodb-undo-logs.html";


/** Rate at which UNDO records should be purged. */
ulong	srv_purge_rseg_truncate_frequency = 128;

/** Enable or Disable Truncate of UNDO tablespace.
Note: If enabled then UNDO tablespace will be selected for truncate.
While Server waits for undo-tablespace to truncate if user disables
it, truncate action is completed but no new tablespace is marked
for truncate (action is never aborted). */
my_bool	srv_undo_log_truncate = FALSE;

/** Maximum size of undo tablespace. */
unsigned long long	srv_max_undo_log_size;

/** UNDO logs that are not redo logged.
These logs reside in the temp tablespace.*/
const ulong		srv_tmp_undo_logs = 32;

/** Default undo tablespace size in UNIV_PAGEs count (10MB). */
const ulint SRV_UNDO_TABLESPACE_SIZE_IN_PAGES =
	((1024 * 1024) * 10) / UNIV_PAGE_SIZE_DEF;

/** Set if InnoDB must operate in read-only mode. We don't do any
recovery and open all tables in RO mode instead of RW mode. We don't
sync the max trx id to disk either. */
my_bool	srv_read_only_mode;
/** store to its own file each table created by an user; data
dictionary tables are in the system tablespace 0 */
my_bool	srv_file_per_table;
/** The file format to use on new *.ibd files. */
ulint	srv_file_format = 0;
/** Whether to check file format during startup.  A value of
UNIV_FORMAT_MAX + 1 means no checking ie. FALSE.  The default is to
set it to the highest format we support. */
ulint	srv_max_file_format_at_startup = UNIV_FORMAT_MAX;
/** Set if InnoDB operates in read-only mode or innodb-force-recovery
is greater than SRV_FORCE_NO_TRX_UNDO. */
my_bool	high_level_read_only;

#if UNIV_FORMAT_A
# error "UNIV_FORMAT_A must be 0!"
#endif

/** Place locks to records only i.e. do not use next-key locking except
on duplicate key checking and foreign key checking */
ibool	srv_locks_unsafe_for_binlog = FALSE;
/** Sort buffer size in index creation */
ulong	srv_sort_buf_size = 1048576;
/** Maximum modification log file size for online index creation */
unsigned long long	srv_online_max_size;

/* If this flag is TRUE, then we will use the native aio of the
OS (provided we compiled Innobase with it in), otherwise we will
use simulated aio we build below with threads.
Currently we support native aio on windows and linux */
my_bool	srv_use_native_aio = TRUE;

#ifdef UNIV_DEBUG
/** Force all user tables to use page compression. */
ulong	srv_debug_compress;
/** Used by SET GLOBAL innodb_master_thread_disabled_debug = X. */
my_bool	srv_master_thread_disabled_debug;
/** Event used to inform that master thread is disabled. */
static os_event_t	srv_master_thread_disabled_event;
/** Debug variable to find if any background threads are adding
to purge during slow shutdown. */
extern bool		trx_commit_disallowed;
#endif /* UNIV_DEBUG */

/*------------------------- LOG FILES ------------------------ */
char*	srv_log_group_home_dir	= NULL;

ulong	srv_n_log_files		= SRV_N_LOG_FILES_MAX;
/** At startup, this is the current redo log file size.
During startup, if this is different from srv_log_file_size_requested
(innodb_log_file_size), the redo log will be rebuilt and this size
will be initialized to srv_log_file_size_requested.
When upgrading from a previous redo log format, this will be set to 0,
and writing to the redo log is not allowed.

During startup, this is in bytes, and later converted to pages. */
ib_uint64_t	srv_log_file_size;
/** The value of the startup parameter innodb_log_file_size */
ib_uint64_t	srv_log_file_size_requested;
/* size in database pages */
ulint		srv_log_buffer_size = ULINT_MAX;
ulong		srv_flush_log_at_trx_commit = 1;
uint		srv_flush_log_at_timeout = 1;
ulong		srv_page_size = UNIV_PAGE_SIZE_DEF;
ulong		srv_page_size_shift = UNIV_PAGE_SIZE_SHIFT_DEF;
ulong		srv_log_write_ahead_size = 0;

page_size_t	univ_page_size(0, 0, false);

/* Try to flush dirty pages so as to avoid IO bursts at
the checkpoints. */
char	srv_adaptive_flushing	= TRUE;

/* Allow IO bursts at the checkpoints ignoring io_capacity setting. */
my_bool	srv_flush_sync		= TRUE;

/** Maximum number of times allowed to conditionally acquire
mutex before switching to blocking wait on the mutex */
#define MAX_MUTEX_NOWAIT	20

/** Check whether the number of failed nonblocking mutex
acquisition attempts exceeds maximum allowed value. If so,
srv_printf_innodb_monitor() will request mutex acquisition
with mutex_enter(), which will wait until it gets the mutex. */
#define MUTEX_NOWAIT(mutex_skipped)	((mutex_skipped) < MAX_MUTEX_NOWAIT)

/** Requested size in bytes */
ulint	srv_buf_pool_size	= ULINT_MAX;
/** Minimum pool size in bytes */
const ulint	srv_buf_pool_min_size	= 5 * 1024 * 1024;
/** Default pool size in bytes */
const ulint	srv_buf_pool_def_size	= 128 * 1024 * 1024;
/** Requested buffer pool chunk size. Each buffer pool instance consists
of one or more chunks. */
ulonglong	srv_buf_pool_chunk_unit;
/** Requested number of buffer pool instances */
ulong	srv_buf_pool_instances;
/** Default number of buffer pool instances */
const ulong	srv_buf_pool_instances_default = 0;
/** Number of locks to protect buf_pool->page_hash */
ulong	srv_n_page_hash_locks = 16;
/** Scan depth for LRU flush batch i.e.: number of blocks scanned*/
ulong	srv_LRU_scan_depth	= 1024;
/** Whether or not to flush neighbors of a block */
ulong	srv_flush_neighbors	= 1;
/** Previously requested size */
ulint	srv_buf_pool_old_size	= 0;
/** Current size as scaling factor for the other components */
ulint	srv_buf_pool_base_size	= 0;
/** Current size in bytes */
ulint	srv_buf_pool_curr_size	= 0;
/** Dump this % of each buffer pool during BP dump */
ulong	srv_buf_pool_dump_pct;
/** Lock table size in bytes */
ulint	srv_lock_table_size	= ULINT_MAX;

/* This parameter is deprecated. Use srv_n_io_[read|write]_threads
instead. */
ulint	srv_n_read_io_threads	= ULINT_MAX;
ulint	srv_n_write_io_threads	= ULINT_MAX;

/* Switch to enable random read ahead. */
my_bool	srv_random_read_ahead	= FALSE;
/* User settable value of the number of pages that must be present
in the buffer cache and accessed sequentially for InnoDB to trigger a
readahead request. */
ulong	srv_read_ahead_threshold	= 56;

/** Maximum on-disk size of change buffer in terms of percentage
of the buffer pool. */
uint	srv_change_buffer_max_size = CHANGE_BUFFER_DEFAULT_SIZE;

/* This parameter is used to throttle the number of insert buffers that are
merged in a batch. By increasing this parameter on a faster disk you can
possibly reduce the number of I/O operations performed to complete the
merge operation. The value of this parameter is used as is by the
background loop when the system is idle (low load), on a busy system
the parameter is scaled down by a factor of 4, this is to avoid putting
a heavier load on the I/O sub system. */

ulong	srv_insert_buffer_batch_size = 20;

char*	srv_file_flush_method_str = NULL;
#ifndef _WIN32
enum srv_unix_flush_t	srv_unix_file_flush_method = SRV_UNIX_FSYNC;
#else
enum srv_win_flush_t	srv_win_file_flush_method = SRV_WIN_IO_UNBUFFERED;
#endif /* _WIN32 */

ulint	srv_max_n_open_files	  = 300;

/* Number of IO operations per second the server can do */
ulong	srv_io_capacity         = 200;
ulong	srv_max_io_capacity     = 400;

/* The number of page cleaner threads to use.*/
ulong	srv_n_page_cleaners = 4;

/* The InnoDB main thread tries to keep the ratio of modified pages
in the buffer pool to all database pages in the buffer pool smaller than
the following number. But it is not guaranteed that the value stays below
that during a time of heavy update/insert activity. */

double	srv_max_buf_pool_modified_pct	= 75.0;
double	srv_max_dirty_pages_pct_lwm	= 0.0;

/* This is the percentage of log capacity at which adaptive flushing,
if enabled, will kick in. */
ulong	srv_adaptive_flushing_lwm	= 10;

/* Number of iterations over which adaptive flushing is averaged. */
ulong	srv_flushing_avg_loops		= 30;

/* The number of purge threads to use.*/
ulong	srv_n_purge_threads = 4;

/* the number of pages to purge in one batch */
ulong	srv_purge_batch_size = 20;

/* Internal setting for "innodb_stats_method". Decides how InnoDB treats
NULL value when collecting statistics. By default, it is set to
SRV_STATS_NULLS_EQUAL(0), ie. all NULL value are treated equal */
ulong srv_innodb_stats_method = SRV_STATS_NULLS_EQUAL;

srv_stats_t	srv_stats;

/* structure to pass status variables to MySQL */
export_var_t export_vars;

/** Normally 0. When nonzero, skip some phases of crash recovery,
starting from SRV_FORCE_IGNORE_CORRUPT, so that data can be recovered
by SELECT or mysqldump. When this is nonzero, we do not allow any user
modifications to the data. */
ulong	srv_force_recovery;
#ifndef DBUG_OFF
/** Inject a crash at different steps of the recovery process.
This is for testing and debugging only. */
ulong	srv_force_recovery_crash;
#endif /* !DBUG_OFF */

/** Print all user-level transactions deadlocks to mysqld stderr */

my_bool	srv_print_all_deadlocks = FALSE;

/** Enable INFORMATION_SCHEMA.innodb_cmp_per_index */
my_bool	srv_cmp_per_index_enabled = FALSE;

/* If the following is set to 1 then we do not run purge and insert buffer
merge to completion before shutdown. If it is set to 2, do not even flush the
buffer pool to data files at the shutdown: we effectively 'crash'
InnoDB (but lose no committed transactions). */
ulint	srv_fast_shutdown	= 0;

/* Generate a innodb_status.<pid> file */
ibool	srv_innodb_status	= FALSE;

/* When estimating number of different key values in an index, sample
this many index pages, there are 2 ways to calculate statistics:
* persistent stats that are calculated by ANALYZE TABLE and saved
  in the innodb database.
* quick transient stats, that are used if persistent stats for the given
  table/index are not found in the innodb database */
unsigned long long	srv_stats_transient_sample_pages = 8;
my_bool		srv_stats_persistent = TRUE;
my_bool		srv_stats_include_delete_marked = FALSE;
unsigned long long	srv_stats_persistent_sample_pages = 20;
my_bool		srv_stats_auto_recalc = TRUE;

ibool	srv_use_doublewrite_buf	= TRUE;

/** doublewrite buffer is 1MB is size i.e.: it can hold 128 16K pages.
The following parameter is the size of the buffer that is used for
batch flushing i.e.: LRU flushing and flush_list flushing. The rest
of the pages are used for single page flushing. */
ulong	srv_doublewrite_batch_size	= 120;

ulong	srv_replication_delay		= 0;

/*-------------------------------------------*/
ulong	srv_n_spin_wait_rounds	= 30;
ulong	srv_spin_wait_delay	= 6;
ibool	srv_priority_boost	= TRUE;

static ulint		srv_n_rows_inserted_old		= 0;
static ulint		srv_n_rows_updated_old		= 0;
static ulint		srv_n_rows_deleted_old		= 0;
static ulint		srv_n_rows_read_old		= 0;

ulint	srv_truncated_status_writes	= 0;
ulint	srv_available_undo_logs         = 0;

/* Set the following to 0 if you want InnoDB to write messages on
stderr on startup/shutdown. */
ibool	srv_print_verbose_log		= TRUE;
my_bool	srv_print_innodb_monitor	= FALSE;
my_bool	srv_print_innodb_lock_monitor	= FALSE;

/* Array of English strings describing the current state of an
i/o handler thread */

const char* srv_io_thread_op_info[SRV_MAX_N_IO_THREADS];
const char* srv_io_thread_function[SRV_MAX_N_IO_THREADS];

time_t	srv_last_monitor_time;

ib_mutex_t	srv_innodb_monitor_mutex;

/** Mutex protecting page_zip_stat_per_index */
ib_mutex_t	page_zip_stat_per_index_mutex;

/* Mutex for locking srv_monitor_file. Not created if srv_read_only_mode */
ib_mutex_t	srv_monitor_file_mutex;

/** Temporary file for innodb monitor output */
FILE*	srv_monitor_file;
/** Mutex for locking srv_dict_tmpfile. Not created if srv_read_only_mode.
This mutex has a very high rank; threads reserving it should not
be holding any InnoDB latches. */
ib_mutex_t	srv_dict_tmpfile_mutex;
/** Temporary file for output from the data dictionary */
FILE*	srv_dict_tmpfile;
/** Mutex for locking srv_misc_tmpfile. Not created if srv_read_only_mode.
This mutex has a very low rank; threads reserving it should not
acquire any further latches or sleep before releasing this one. */
ib_mutex_t	srv_misc_tmpfile_mutex;
/** Temporary file for miscellanous diagnostic output */
FILE*	srv_misc_tmpfile;

ulint	srv_main_thread_process_no	= 0;
ulint	srv_main_thread_id		= 0;

/* The following counts are used by the srv_master_thread. */

/** Iterations of the loop bounded by 'srv_active' label. */
static ulint		srv_main_active_loops		= 0;
/** Iterations of the loop bounded by the 'srv_idle' label. */
static ulint		srv_main_idle_loops		= 0;
/** Iterations of the loop bounded by the 'srv_shutdown' label. */
static ulint		srv_main_shutdown_loops		= 0;
/** Log writes involving flush. */
static ulint		srv_log_writes_and_flush	= 0;

/* This is only ever touched by the master thread. It records the
time when the last flush of log file has happened. The master
thread ensures that we flush the log files at least once per
second. */
static time_t	srv_last_log_flush_time;

/* Interval in seconds at which various tasks are performed by the
master thread when server is active. In order to balance the workload,
we should try to keep intervals such that they are not multiple of
each other. For example, if we have intervals for various tasks
defined as 5, 10, 15, 60 then all tasks will be performed when
current_time % 60 == 0 and no tasks will be performed when
current_time % 5 != 0. */

# define	SRV_MASTER_CHECKPOINT_INTERVAL		(7)
# define	SRV_MASTER_PURGE_INTERVAL		(10)
# define	SRV_MASTER_DICT_LRU_INTERVAL		(47)

/** Acquire the system_mutex. */
#define srv_sys_mutex_enter() do {			\
	mutex_enter(&srv_sys->mutex);			\
} while (0)

/** Test if the system mutex is owned. */
#define srv_sys_mutex_own() (mutex_own(&srv_sys->mutex)	\
			     && !srv_read_only_mode)

/** Release the system mutex. */
#define srv_sys_mutex_exit() do {			\
	mutex_exit(&srv_sys->mutex);			\
} while (0)

#define fetch_lock_wait_timeout(trx)			\
	((trx)->lock.allowed_to_wait			\
	 ? thd_lock_wait_timeout((trx)->mysql_thd)	\
	 : 0)

/*
	IMPLEMENTATION OF THE SERVER MAIN PROGRAM
	=========================================

There is the following analogue between this database
server and an operating system kernel:

DB concept			equivalent OS concept
----------			---------------------
transaction		--	process;

query thread		--	thread;

lock			--	semaphore;

kernel			--	kernel;

query thread execution:
(a) without lock mutex
reserved		--	process executing in user mode;
(b) with lock mutex reserved
			--	process executing in kernel mode;

The server has several backgroind threads all running at the same
priority as user threads. It periodically checks if here is anything
happening in the server which requires intervention of the master
thread. Such situations may be, for example, when flushing of dirty
blocks is needed in the buffer pool or old version of database rows
have to be cleaned away (purged). The user can configure a separate
dedicated purge thread(s) too, in which case the master thread does not
do any purging.

The threads which we call user threads serve the queries of the MySQL
server. They run at normal priority.

When there is no activity in the system, also the master thread
suspends itself to wait for an event making the server totally silent.

There is still one complication in our server design. If a
background utility thread obtains a resource (e.g., mutex) needed by a user
thread, and there is also some other user activity in the system,
the user thread may have to wait indefinitely long for the
resource, as the OS does not schedule a background thread if
there is some other runnable user thread. This problem is called
priority inversion in real-time programming.

One solution to the priority inversion problem would be to keep record
of which thread owns which resource and in the above case boost the
priority of the background thread so that it will be scheduled and it
can release the resource.  This solution is called priority inheritance
in real-time programming.  A drawback of this solution is that the overhead
of acquiring a mutex increases slightly, maybe 0.2 microseconds on a 100
MHz Pentium, because the thread has to call os_thread_get_curr_id.  This may
be compared to 0.5 microsecond overhead for a mutex lock-unlock pair. Note
that the thread cannot store the information in the resource , say mutex,
itself, because competing threads could wipe out the information if it is
stored before acquiring the mutex, and if it stored afterwards, the
information is outdated for the time of one machine instruction, at least.
(To be precise, the information could be stored to lock_word in mutex if
the machine supports atomic swap.)

The above solution with priority inheritance may become actual in the
future, currently we do not implement any priority twiddling solution.
Our general aim is to reduce the contention of all mutexes by making
them more fine grained.

The thread table contains information of the current status of each
thread existing in the system, and also the event semaphores used in
suspending the master thread and utility threads when they have nothing
to do.  The thread table can be seen as an analogue to the process table
in a traditional Unix implementation. */

/** The server system struct */
struct srv_sys_t{
	ib_mutex_t	tasks_mutex;		/*!< variable protecting the
						tasks queue */
	UT_LIST_BASE_NODE_T(que_thr_t)
			tasks;			/*!< task queue */

	ib_mutex_t	mutex;			/*!< variable protecting the
						fields below. */
	ulint		n_sys_threads;		/*!< size of the sys_threads
						array */

	srv_slot_t*	sys_threads;		/*!< server thread table */

	ulint		n_threads_active[SRV_MASTER + 1];
						/*!< number of threads active
						in a thread class */

	srv_stats_t::ulint_ctr_1_t
			activity_count;		/*!< For tracking server
						activity */
};

static srv_sys_t*	srv_sys	= NULL;

/** Event to signal the monitor thread. */
os_event_t	srv_monitor_event;

/** Event to signal the error thread */
os_event_t	srv_error_event;

/** Event to signal the buffer pool dump/load thread */
os_event_t	srv_buf_dump_event;

/** Event to signal the buffer pool resize thread */
os_event_t	srv_buf_resize_event;

/** The buffer pool dump/load file name */
char*	srv_buf_dump_filename;

/** Boolean config knobs that tell InnoDB to dump the buffer pool at shutdown
and/or load it during startup. */
char	srv_buffer_pool_dump_at_shutdown = TRUE;
char	srv_buffer_pool_load_at_startup = TRUE;

/** Slot index in the srv_sys->sys_threads array for the purge thread. */
static const ulint	SRV_PURGE_SLOT	= 1;

/** Slot index in the srv_sys->sys_threads array for the master thread. */
static const ulint	SRV_MASTER_SLOT = 0;

#ifdef HAVE_PSI_STAGE_INTERFACE
/** Performance schema stage event for monitoring ALTER TABLE progress
everything after flush log_make_checkpoint_at(). */
PSI_stage_info	srv_stage_alter_table_end
	= {0, "alter table (end)", PSI_FLAG_STAGE_PROGRESS};

/** Performance schema stage event for monitoring ALTER TABLE progress
log_make_checkpoint_at(). */
PSI_stage_info	srv_stage_alter_table_flush
	= {0, "alter table (flush)", PSI_FLAG_STAGE_PROGRESS};

/** Performance schema stage event for monitoring ALTER TABLE progress
row_merge_insert_index_tuples(). */
PSI_stage_info	srv_stage_alter_table_insert
	= {0, "alter table (insert)", PSI_FLAG_STAGE_PROGRESS};

/** Performance schema stage event for monitoring ALTER TABLE progress
row_log_apply(). */
PSI_stage_info	srv_stage_alter_table_log_index
	= {0, "alter table (log apply index)", PSI_FLAG_STAGE_PROGRESS};

/** Performance schema stage event for monitoring ALTER TABLE progress
row_log_table_apply(). */
PSI_stage_info	srv_stage_alter_table_log_table
	= {0, "alter table (log apply table)", PSI_FLAG_STAGE_PROGRESS};

/** Performance schema stage event for monitoring ALTER TABLE progress
row_merge_sort(). */
PSI_stage_info	srv_stage_alter_table_merge_sort
	= {0, "alter table (merge sort)", PSI_FLAG_STAGE_PROGRESS};

/** Performance schema stage event for monitoring ALTER TABLE progress
row_merge_read_clustered_index(). */
PSI_stage_info	srv_stage_alter_table_read_pk_internal_sort
	= {0, "alter table (read PK and internal sort)", PSI_FLAG_STAGE_PROGRESS};

/** Performance schema stage event for monitoring buffer pool load progress. */
PSI_stage_info	srv_stage_buffer_pool_load
	= {0, "buffer pool load", PSI_FLAG_STAGE_PROGRESS};
#endif /* HAVE_PSI_STAGE_INTERFACE */

/*********************************************************************//**
Prints counters for work done by srv_master_thread. */
static
void
srv_print_master_thread_info(
/*=========================*/
	FILE  *file)    /* in: output stream */
{
	fprintf(file,
		"srv_master_thread loops: "
		ULINTPF " srv_active, "
		ULINTPF " srv_shutdown, "
		ULINTPF " srv_idle\n",
		srv_main_active_loops,
		srv_main_shutdown_loops,
		srv_main_idle_loops);
	fprintf(file,
		"srv_master_thread log flush and writes: " ULINTPF "\n",
		srv_log_writes_and_flush);
}

/*********************************************************************//**
Sets the info describing an i/o thread current state. */
void
srv_set_io_thread_op_info(
/*======================*/
	ulint		i,	/*!< in: the 'segment' of the i/o thread */
	const char*	str)	/*!< in: constant char string describing the
				state */
{
	ut_a(i < SRV_MAX_N_IO_THREADS);

	srv_io_thread_op_info[i] = str;
}

/*********************************************************************//**
Resets the info describing an i/o thread current state. */
void
srv_reset_io_thread_op_info()
/*=========================*/
{
	for (ulint i = 0; i < UT_ARR_SIZE(srv_io_thread_op_info); ++i) {
		srv_io_thread_op_info[i] = "not started yet";
	}
}

#ifdef UNIV_DEBUG
/*********************************************************************//**
Validates the type of a thread table slot.
@return TRUE if ok */
static
ibool
srv_thread_type_validate(
/*=====================*/
	srv_thread_type	type)	/*!< in: thread type */
{
	switch (type) {
	case SRV_NONE:
		break;
	case SRV_WORKER:
	case SRV_PURGE:
	case SRV_MASTER:
		return(TRUE);
	}
	ut_error;
	return(FALSE);
}
#endif /* UNIV_DEBUG */

/*********************************************************************//**
Gets the type of a thread table slot.
@return thread type */
static
srv_thread_type
srv_slot_get_type(
/*==============*/
	const srv_slot_t*	slot)	/*!< in: thread slot */
{
	srv_thread_type	type = slot->type;
	ut_ad(srv_thread_type_validate(type));
	return(type);
}

/*********************************************************************//**
Reserves a slot in the thread table for the current thread.
@return reserved slot */
static
srv_slot_t*
srv_reserve_slot(
/*=============*/
	srv_thread_type	type)	/*!< in: type of the thread */
{
	srv_slot_t*	slot = 0;

	srv_sys_mutex_enter();

	ut_ad(srv_thread_type_validate(type));

	switch (type) {
	case SRV_MASTER:
		slot = &srv_sys->sys_threads[SRV_MASTER_SLOT];
		break;

	case SRV_PURGE:
		slot = &srv_sys->sys_threads[SRV_PURGE_SLOT];
		break;

	case SRV_WORKER:
		/* Find an empty slot, skip the master and purge slots. */
		for (slot = &srv_sys->sys_threads[2];
		     slot->in_use;
		     ++slot) {

			ut_a(slot < &srv_sys->sys_threads[
			     srv_sys->n_sys_threads]);
		}
		break;

	case SRV_NONE:
		ut_error;
	}

	ut_a(!slot->in_use);

	slot->in_use = TRUE;
	slot->suspended = FALSE;
	slot->type = type;

	ut_ad(srv_slot_get_type(slot) == type);

	++srv_sys->n_threads_active[type];

	srv_sys_mutex_exit();

	return(slot);
}

/*********************************************************************//**
Suspends the calling thread to wait for the event in its thread slot.
@return the current signal count of the event. */
static
int64_t
srv_suspend_thread_low(
/*===================*/
	srv_slot_t*	slot)	/*!< in/out: thread slot */
{

	ut_ad(!srv_read_only_mode);
	ut_ad(srv_sys_mutex_own());

	ut_ad(slot->in_use);

	srv_thread_type	type = srv_slot_get_type(slot);

	switch (type) {
	case SRV_NONE:
		ut_error;

	case SRV_MASTER:
		/* We have only one master thread and it
		should be the first entry always. */
		ut_a(srv_sys->n_threads_active[type] == 1);
		break;

	case SRV_PURGE:
		/* We have only one purge coordinator thread
		and it should be the second entry always. */
		ut_a(srv_sys->n_threads_active[type] == 1);
		break;

	case SRV_WORKER:
		ut_a(srv_n_purge_threads > 1);
		ut_a(srv_sys->n_threads_active[type] > 0);
		break;
	}

	ut_a(!slot->suspended);
	slot->suspended = TRUE;

	ut_a(srv_sys->n_threads_active[type] > 0);

	srv_sys->n_threads_active[type]--;

	return(os_event_reset(slot->event));
}

/*********************************************************************//**
Suspends the calling thread to wait for the event in its thread slot.
@return the current signal count of the event. */
static
int64_t
srv_suspend_thread(
/*===============*/
	srv_slot_t*	slot)	/*!< in/out: thread slot */
{
	srv_sys_mutex_enter();

	int64_t		sig_count = srv_suspend_thread_low(slot);

	srv_sys_mutex_exit();

	return(sig_count);
}

/*********************************************************************//**
Releases threads of the type given from suspension in the thread table.
NOTE! The server mutex has to be reserved by the caller!
@return number of threads released: this may be less than n if not
        enough threads were suspended at the moment. */
ulint
srv_release_threads(
/*================*/
	srv_thread_type	type,	/*!< in: thread type */
	ulint		n)	/*!< in: number of threads to release */
{
	ulint		i;
	ulint		count	= 0;

	ut_ad(srv_thread_type_validate(type));
	ut_ad(n > 0);

	srv_sys_mutex_enter();

	for (i = 0; i < srv_sys->n_sys_threads; i++) {
		srv_slot_t*	slot;

		slot = &srv_sys->sys_threads[i];

		if (slot->in_use
		    && srv_slot_get_type(slot) == type
		    && slot->suspended) {

			switch (type) {
			case SRV_NONE:
				ut_error;

			case SRV_MASTER:
				/* We have only one master thread and it
				should be the first entry always. */
				ut_a(n == 1);
				ut_a(i == SRV_MASTER_SLOT);
				ut_a(srv_sys->n_threads_active[type] == 0);
				break;

			case SRV_PURGE:
				/* We have only one purge coordinator thread
				and it should be the second entry always. */
				ut_a(n == 1);
				ut_a(i == SRV_PURGE_SLOT);
				ut_a(srv_n_purge_threads > 0);
				ut_a(srv_sys->n_threads_active[type] == 0);
				break;

			case SRV_WORKER:
				ut_a(srv_n_purge_threads > 1);
				ut_a(srv_sys->n_threads_active[type]
				     < srv_n_purge_threads - 1);
				break;
			}

			slot->suspended = FALSE;

			++srv_sys->n_threads_active[type];

			os_event_set(slot->event);

			if (++count == n) {
				break;
			}
		}
	}

	srv_sys_mutex_exit();

	return(count);
}

/*********************************************************************//**
Release a thread's slot. */
static
void
srv_free_slot(
/*==========*/
	srv_slot_t*	slot)	/*!< in/out: thread slot */
{
	srv_sys_mutex_enter();

	if (!slot->suspended) {
		/* Mark the thread as inactive. */
		srv_suspend_thread_low(slot);
	}

	/* Free the slot for reuse. */
	ut_ad(slot->in_use);
	slot->in_use = FALSE;

	srv_sys_mutex_exit();
}

/*********************************************************************//**
Initializes the server. */
void
srv_init(void)
/*==========*/
{
	ulint	n_sys_threads = 0;
	ulint	srv_sys_sz = sizeof(*srv_sys);

	mutex_create(LATCH_ID_SRV_INNODB_MONITOR, &srv_innodb_monitor_mutex);

	if (!srv_read_only_mode) {

		/* Number of purge threads + master thread */
		n_sys_threads = srv_n_purge_threads + 1;

		srv_sys_sz += n_sys_threads * sizeof(*srv_sys->sys_threads);
	}

	srv_sys = static_cast<srv_sys_t*>(ut_zalloc_nokey(srv_sys_sz));

	srv_sys->n_sys_threads = n_sys_threads;

	/* Even in read-only mode we flush pages related to intrinsic table
	and so mutex creation is needed. */
	{

		mutex_create(LATCH_ID_SRV_SYS, &srv_sys->mutex);

		mutex_create(LATCH_ID_SRV_SYS_TASKS, &srv_sys->tasks_mutex);

		srv_sys->sys_threads = (srv_slot_t*) &srv_sys[1];

		for (ulint i = 0; i < srv_sys->n_sys_threads; ++i) {
			srv_slot_t*	slot = &srv_sys->sys_threads[i];

			slot->event = os_event_create(0);

			ut_a(slot->event);
		}

		srv_error_event = os_event_create(0);

		srv_monitor_event = os_event_create(0);

		srv_buf_dump_event = os_event_create(0);

		buf_flush_event = os_event_create("buf_flush_event");

		UT_LIST_INIT(srv_sys->tasks, &que_thr_t::queue);
	}

	srv_buf_resize_event = os_event_create(0);

	ut_d(srv_master_thread_disabled_event = os_event_create(0));

	/* page_zip_stat_per_index_mutex is acquired from:
	1. page_zip_compress() (after SYNC_FSP)
	2. page_zip_decompress()
	3. i_s_cmp_per_index_fill_low() (where SYNC_DICT is acquired)
	4. innodb_cmp_per_index_update(), no other latches
	since we do not acquire any other latches while holding this mutex,
	it can have very low level. We pick SYNC_ANY_LATCH for it. */
	mutex_create(LATCH_ID_PAGE_ZIP_STAT_PER_INDEX,
		     &page_zip_stat_per_index_mutex);

	/* Create dummy indexes for infimum and supremum records */

	dict_ind_init();

	/* Initialize some INFORMATION SCHEMA internal structures */
	trx_i_s_cache_init(trx_i_s_cache);

	ut_crc32_init();

	dict_mem_init();
}

/*********************************************************************//**
Frees the data structures created in srv_init(). */
void
srv_free(void)
/*==========*/
{
	mutex_free(&srv_innodb_monitor_mutex);
	mutex_free(&page_zip_stat_per_index_mutex);

	{
		mutex_free(&srv_sys->mutex);
		mutex_free(&srv_sys->tasks_mutex);

		for (ulint i = 0; i < srv_sys->n_sys_threads; ++i) {
			srv_slot_t*	slot = &srv_sys->sys_threads[i];

			os_event_destroy(slot->event);
		}

		os_event_destroy(srv_error_event);
		os_event_destroy(srv_monitor_event);
		os_event_destroy(srv_buf_dump_event);
		os_event_destroy(buf_flush_event);
	}

	os_event_destroy(srv_buf_resize_event);

#ifdef UNIV_DEBUG
	os_event_destroy(srv_master_thread_disabled_event);
	srv_master_thread_disabled_event = NULL;
#endif /* UNIV_DEBUG */

	trx_i_s_cache_free(trx_i_s_cache);

	ut_free(srv_sys);

	srv_sys = 0;
}

/*********************************************************************//**
Initializes the synchronization primitives, memory system, and the thread
local storage. */
void
srv_general_init(void)
/*==================*/
{
	sync_check_init();
	/* Reset the system variables in the recovery module. */
	recv_sys_var_init();
	os_thread_init();
	trx_pool_init();
	que_init();
	row_mysql_init();
}

/*********************************************************************//**
Normalizes init parameter values to use units we use inside InnoDB. */
static
void
srv_normalize_init_values(void)
/*===========================*/
{
	srv_sys_space.normalize();

	srv_tmp_space.normalize();

	srv_log_file_size /= UNIV_PAGE_SIZE;

	srv_log_buffer_size /= UNIV_PAGE_SIZE;

	srv_lock_table_size = 5 * (srv_buf_pool_size / UNIV_PAGE_SIZE);
}

/*********************************************************************//**
Boots the InnoDB server. */
void
srv_boot(void)
/*==========*/
{
	/* Transform the init parameter values given by MySQL to
	use units we use inside InnoDB: */

	srv_normalize_init_values();

	/* Initialize synchronization primitives, memory management, and thread
	local storage */

	srv_general_init();

	/* Initialize this module */

	srv_init();
}

/******************************************************************//**
Refreshes the values used to calculate per-second averages. */
static
void
srv_refresh_innodb_monitor_stats(void)
/*==================================*/
{
	mutex_enter(&srv_innodb_monitor_mutex);

	srv_last_monitor_time = time(NULL);

	os_aio_refresh_stats();

	btr_cur_n_sea_old = btr_cur_n_sea;
	btr_cur_n_non_sea_old = btr_cur_n_non_sea;

	log_refresh_stats();

	buf_refresh_io_stats_all();

	srv_n_rows_inserted_old = srv_stats.n_rows_inserted;
	srv_n_rows_updated_old = srv_stats.n_rows_updated;
	srv_n_rows_deleted_old = srv_stats.n_rows_deleted;
	srv_n_rows_read_old = srv_stats.n_rows_read;

	mutex_exit(&srv_innodb_monitor_mutex);
}

/******************************************************************//**
Outputs to a file the output of the InnoDB Monitor.
@return FALSE if not all information printed
due to failure to obtain necessary mutex */
ibool
srv_printf_innodb_monitor(
/*======================*/
	FILE*	file,		/*!< in: output stream */
	ibool	nowait,		/*!< in: whether to wait for the
				lock_sys_t:: mutex */
	ulint*	trx_start_pos,	/*!< out: file position of the start of
				the list of active transactions */
	ulint*	trx_end)	/*!< out: file position of the end of
				the list of active transactions */
{
	double	time_elapsed;
	time_t	current_time;
	ulint	n_reserved;
	ibool	ret;

	mutex_enter(&srv_innodb_monitor_mutex);

	current_time = time(NULL);

	/* We add 0.001 seconds to time_elapsed to prevent division
	by zero if two users happen to call SHOW ENGINE INNODB STATUS at the
	same time */

	time_elapsed = difftime(current_time, srv_last_monitor_time)
		+ 0.001;

	srv_last_monitor_time = time(NULL);

	fputs("\n=====================================\n", file);

	ut_print_timestamp(file);
	fprintf(file,
		" INNODB MONITOR OUTPUT\n"
		"=====================================\n"
		"Per second averages calculated from the last %lu seconds\n",
		(ulong) time_elapsed);

	fputs("-----------------\n"
	      "BACKGROUND THREAD\n"
	      "-----------------\n", file);
	srv_print_master_thread_info(file);

	fputs("----------\n"
	      "SEMAPHORES\n"
	      "----------\n", file);

	sync_print(file);

	/* Conceptually, srv_innodb_monitor_mutex has a very high latching
	order level in sync0sync.h, while dict_foreign_err_mutex has a very
	low level 135. Therefore we can reserve the latter mutex here without
	a danger of a deadlock of threads. */

	mutex_enter(&dict_foreign_err_mutex);

	if (!srv_read_only_mode && ftell(dict_foreign_err_file) != 0L) {
		fputs("------------------------\n"
		      "LATEST FOREIGN KEY ERROR\n"
		      "------------------------\n", file);
		ut_copy_file(file, dict_foreign_err_file);
	}

	mutex_exit(&dict_foreign_err_mutex);

	/* Only if lock_print_info_summary proceeds correctly,
	before we call the lock_print_info_all_transactions
	to print all the lock information. IMPORTANT NOTE: This
	function acquires the lock mutex on success. */
	ret = lock_print_info_summary(file, nowait);

	if (ret) {
		if (trx_start_pos) {
			long	t = ftell(file);
			if (t < 0) {
				*trx_start_pos = ULINT_UNDEFINED;
			} else {
				*trx_start_pos = (ulint) t;
			}
		}

		/* NOTE: If we get here then we have the lock mutex. This
		function will release the lock mutex that we acquired when
		we called the lock_print_info_summary() function earlier. */

		lock_print_info_all_transactions(file);

		if (trx_end) {
			long	t = ftell(file);
			if (t < 0) {
				*trx_end = ULINT_UNDEFINED;
			} else {
				*trx_end = (ulint) t;
			}
		}
	}

	fputs("--------\n"
	      "FILE I/O\n"
	      "--------\n", file);
	os_aio_print(file);

	fputs("-------------------------------------\n"
	      "INSERT BUFFER AND ADAPTIVE HASH INDEX\n"
	      "-------------------------------------\n", file);
	ibuf_print(file);

	for (ulint i = 0; i < btr_ahi_parts; ++i) {
		rw_lock_s_lock(btr_search_latches[i]);
		ha_print_info(file, btr_search_sys->hash_tables[i]);
		rw_lock_s_unlock(btr_search_latches[i]);
	}

	fprintf(file,
		"%.2f hash searches/s, %.2f non-hash searches/s\n",
		(btr_cur_n_sea - btr_cur_n_sea_old)
		/ time_elapsed,
		(btr_cur_n_non_sea - btr_cur_n_non_sea_old)
		/ time_elapsed);
	btr_cur_n_sea_old = btr_cur_n_sea;
	btr_cur_n_non_sea_old = btr_cur_n_non_sea;

	fputs("---\n"
	      "LOG\n"
	      "---\n", file);
	log_print(file);

	fputs("----------------------\n"
	      "BUFFER POOL AND MEMORY\n"
	      "----------------------\n", file);
	fprintf(file,
		"Total large memory allocated " ULINTPF "\n"
		"Dictionary memory allocated " ULINTPF "\n",
		os_total_large_mem_allocated,
		dict_sys->size);

	buf_print_io(file);

	fputs("--------------\n"
	      "ROW OPERATIONS\n"
	      "--------------\n", file);
	fprintf(file,
		ULINTPF " queries inside InnoDB, "
		ULINTPF " queries in queue\n",
		srv_conc_get_active_threads(),
		srv_conc_get_waiting_threads());

	/* This is a dirty read, without holding trx_sys->mutex. */
	fprintf(file,
		ULINTPF " read views open inside InnoDB\n",
		trx_sys->mvcc->size());

	n_reserved = fil_space_get_n_reserved_extents(0);
	if (n_reserved > 0) {
		fprintf(file,
			ULINTPF " tablespace extents now reserved for"
			" B-tree split operations\n",
			n_reserved);
	}

	fprintf(file,
		"Process ID=" ULINTPF
		", Main thread ID=" ULINTPF
		", state: %s\n",
		srv_main_thread_process_no,
		srv_main_thread_id,
		srv_main_thread_op_info);
	fprintf(file,
		"Number of rows inserted " ULINTPF
		", updated " ULINTPF
		", deleted " ULINTPF
		", read " ULINTPF "\n",
		(ulint) srv_stats.n_rows_inserted,
		(ulint) srv_stats.n_rows_updated,
		(ulint) srv_stats.n_rows_deleted,
		(ulint) srv_stats.n_rows_read);
	fprintf(file,
		"%.2f inserts/s, %.2f updates/s,"
		" %.2f deletes/s, %.2f reads/s\n",
		((ulint) srv_stats.n_rows_inserted - srv_n_rows_inserted_old)
		/ time_elapsed,
		((ulint) srv_stats.n_rows_updated - srv_n_rows_updated_old)
		/ time_elapsed,
		((ulint) srv_stats.n_rows_deleted - srv_n_rows_deleted_old)
		/ time_elapsed,
		((ulint) srv_stats.n_rows_read - srv_n_rows_read_old)
		/ time_elapsed);

	srv_n_rows_inserted_old = srv_stats.n_rows_inserted;
	srv_n_rows_updated_old = srv_stats.n_rows_updated;
	srv_n_rows_deleted_old = srv_stats.n_rows_deleted;
	srv_n_rows_read_old = srv_stats.n_rows_read;

	fputs("----------------------------\n"
	      "END OF INNODB MONITOR OUTPUT\n"
	      "============================\n", file);
	mutex_exit(&srv_innodb_monitor_mutex);
	fflush(file);

	return(ret);
}

/******************************************************************//**
Function to pass InnoDB status variables to MySQL */
void
srv_export_innodb_status(void)
/*==========================*/
{
	buf_pool_stat_t		stat;
	buf_pools_list_size_t	buf_pools_list_size;
	ulint			LRU_len;
	ulint			free_len;
	ulint			flush_list_len;

	buf_get_total_stat(&stat);
	buf_get_total_list_len(&LRU_len, &free_len, &flush_list_len);
	buf_get_total_list_size_in_bytes(&buf_pools_list_size);

	mutex_enter(&srv_innodb_monitor_mutex);

	export_vars.innodb_data_pending_reads =
		os_n_pending_reads;

	export_vars.innodb_data_pending_writes =
		os_n_pending_writes;

	export_vars.innodb_data_pending_fsyncs =
		fil_n_pending_log_flushes
		+ fil_n_pending_tablespace_flushes;

	export_vars.innodb_data_fsyncs = os_n_fsyncs;

	export_vars.innodb_data_read = srv_stats.data_read;

	export_vars.innodb_data_reads = os_n_file_reads;

	export_vars.innodb_data_writes = os_n_file_writes;

	export_vars.innodb_data_written = srv_stats.data_written;

	export_vars.innodb_buffer_pool_read_requests = stat.n_page_gets;

	export_vars.innodb_buffer_pool_write_requests =
		srv_stats.buf_pool_write_requests;

	export_vars.innodb_buffer_pool_wait_free =
		srv_stats.buf_pool_wait_free;

	export_vars.innodb_buffer_pool_pages_flushed =
		srv_stats.buf_pool_flushed;

	export_vars.innodb_buffer_pool_reads = srv_stats.buf_pool_reads;

	export_vars.innodb_buffer_pool_read_ahead_rnd =
		stat.n_ra_pages_read_rnd;

	export_vars.innodb_buffer_pool_read_ahead =
		stat.n_ra_pages_read;

	export_vars.innodb_buffer_pool_read_ahead_evicted =
		stat.n_ra_pages_evicted;

	export_vars.innodb_buffer_pool_pages_data = LRU_len;

	export_vars.innodb_buffer_pool_bytes_data =
		buf_pools_list_size.LRU_bytes
		+ buf_pools_list_size.unzip_LRU_bytes;

	export_vars.innodb_buffer_pool_pages_dirty = flush_list_len;

	export_vars.innodb_buffer_pool_bytes_dirty =
		buf_pools_list_size.flush_list_bytes;

	export_vars.innodb_buffer_pool_pages_free = free_len;

#ifdef UNIV_DEBUG
	export_vars.innodb_buffer_pool_pages_latched =
		buf_get_latched_pages_number();
#endif /* UNIV_DEBUG */
	export_vars.innodb_buffer_pool_pages_total = buf_pool_get_n_pages();

	export_vars.innodb_buffer_pool_pages_misc =
		buf_pool_get_n_pages() - LRU_len - free_len;

	export_vars.innodb_page_size = UNIV_PAGE_SIZE;

	export_vars.innodb_log_waits = srv_stats.log_waits;

	export_vars.innodb_os_log_written = srv_stats.os_log_written;

	export_vars.innodb_os_log_fsyncs = fil_n_log_flushes;

	export_vars.innodb_os_log_pending_fsyncs = fil_n_pending_log_flushes;

	export_vars.innodb_os_log_pending_writes =
		srv_stats.os_log_pending_writes;

	export_vars.innodb_log_write_requests = srv_stats.log_write_requests;

	export_vars.innodb_log_writes = srv_stats.log_writes;

	export_vars.innodb_dblwr_pages_written =
		srv_stats.dblwr_pages_written;

	export_vars.innodb_dblwr_writes = srv_stats.dblwr_writes;

	export_vars.innodb_pages_created = stat.n_pages_created;

	export_vars.innodb_pages_read = stat.n_pages_read;

	export_vars.innodb_pages_written = stat.n_pages_written;

	export_vars.innodb_row_lock_waits = srv_stats.n_lock_wait_count;

	export_vars.innodb_row_lock_current_waits =
		srv_stats.n_lock_wait_current_count;

	export_vars.innodb_row_lock_time = srv_stats.n_lock_wait_time / 1000;

	if (srv_stats.n_lock_wait_count > 0) {

		export_vars.innodb_row_lock_time_avg = (ulint)
			(srv_stats.n_lock_wait_time
			 / 1000 / srv_stats.n_lock_wait_count);

	} else {
		export_vars.innodb_row_lock_time_avg = 0;
	}

	export_vars.innodb_row_lock_time_max =
		lock_sys->n_lock_max_wait_time / 1000;

	export_vars.innodb_rows_read = srv_stats.n_rows_read;

	export_vars.innodb_rows_inserted = srv_stats.n_rows_inserted;

	export_vars.innodb_rows_updated = srv_stats.n_rows_updated;

	export_vars.innodb_rows_deleted = srv_stats.n_rows_deleted;

	export_vars.innodb_num_open_files = fil_n_file_opened;

	export_vars.innodb_truncated_status_writes =
		srv_truncated_status_writes;

	export_vars.innodb_available_undo_logs = srv_available_undo_logs;

#ifdef UNIV_DEBUG
	rw_lock_s_lock(&purge_sys->latch);
	trx_id_t	up_limit_id;
	trx_id_t	done_trx_no	= purge_sys->done.trx_no;

	up_limit_id	= purge_sys->view_active
		? purge_sys->view.up_limit_id() : 0;

	rw_lock_s_unlock(&purge_sys->latch);

	mutex_enter(&trx_sys->mutex);
	trx_id_t	max_trx_id	= trx_sys->rw_max_trx_id;
	mutex_exit(&trx_sys->mutex);

	if (!done_trx_no || max_trx_id < done_trx_no - 1) {
		export_vars.innodb_purge_trx_id_age = 0;
	} else {
		export_vars.innodb_purge_trx_id_age =
			(ulint) (max_trx_id - done_trx_no + 1);
	}

	if (!up_limit_id
	    || max_trx_id < up_limit_id) {
		export_vars.innodb_purge_view_trx_id_age = 0;
	} else {
		export_vars.innodb_purge_view_trx_id_age =
			(ulint) (max_trx_id - up_limit_id);
	}
#endif /* UNIV_DEBUG */

	mutex_exit(&srv_innodb_monitor_mutex);
}

/*********************************************************************//**
A thread which prints the info output by various InnoDB monitors.
@return a dummy parameter */
extern "C"
os_thread_ret_t
DECLARE_THREAD(srv_monitor_thread)(
/*===============================*/
	void*	arg MY_ATTRIBUTE((unused)))
			/*!< in: a dummy parameter required by
			os_thread_create */
{
	int64_t		sig_count;
	double		time_elapsed;
	time_t		current_time;
	time_t		last_monitor_time;
	ulint		mutex_skipped;
	ibool		last_srv_print_monitor;

	ut_ad(!srv_read_only_mode);

#ifdef UNIV_DEBUG_THREAD_CREATION
	ib::info() << "Lock timeout thread starts, id "
		<< os_thread_pf(os_thread_get_curr_id());
#endif /* UNIV_DEBUG_THREAD_CREATION */

#ifdef UNIV_PFS_THREAD
	pfs_register_thread(srv_monitor_thread_key);
#endif /* UNIV_PFS_THREAD */
	srv_monitor_active = TRUE;

	UT_NOT_USED(arg);
	srv_last_monitor_time = last_monitor_time = ut_time();
	mutex_skipped = 0;
	last_srv_print_monitor = srv_print_innodb_monitor;
loop:
	/* Wake up every 5 seconds to see if we need to print
	monitor information or if signalled at shutdown. */

	sig_count = os_event_reset(srv_monitor_event);

	os_event_wait_time_low(srv_monitor_event, 5000000, sig_count);

	current_time = ut_time();

	time_elapsed = difftime(current_time, last_monitor_time);

	if (time_elapsed > 15) {
		last_monitor_time = ut_time();

		if (srv_print_innodb_monitor) {
			/* Reset mutex_skipped counter everytime
			srv_print_innodb_monitor changes. This is to
			ensure we will not be blocked by lock_sys->mutex
			for short duration information printing,
			such as requested by sync_array_print_long_waits() */
			if (!last_srv_print_monitor) {
				mutex_skipped = 0;
				last_srv_print_monitor = TRUE;
			}

			if (!srv_printf_innodb_monitor(stderr,
						MUTEX_NOWAIT(mutex_skipped),
						NULL, NULL)) {
				mutex_skipped++;
			} else {
				/* Reset the counter */
				mutex_skipped = 0;
			}
		} else {
			last_srv_print_monitor = FALSE;
		}


		/* We don't create the temp files or associated
		mutexes in read-only-mode */

		if (!srv_read_only_mode && srv_innodb_status) {
			mutex_enter(&srv_monitor_file_mutex);
			rewind(srv_monitor_file);
			if (!srv_printf_innodb_monitor(srv_monitor_file,
						MUTEX_NOWAIT(mutex_skipped),
						NULL, NULL)) {
				mutex_skipped++;
			} else {
				mutex_skipped = 0;
			}

			os_file_set_eof(srv_monitor_file);
			mutex_exit(&srv_monitor_file_mutex);
		}
	}

	if (srv_shutdown_state >= SRV_SHUTDOWN_CLEANUP) {
		goto exit_func;
	}

	if (srv_print_innodb_monitor || srv_print_innodb_lock_monitor) {
		goto loop;
	}

	goto loop;

exit_func:
	srv_monitor_active = FALSE;

	/* We count the number of threads in os_thread_exit(). A created
	thread should always use that to exit and not use return() to exit. */

	os_thread_exit();

	OS_THREAD_DUMMY_RETURN;
}

/*********************************************************************//**
A thread which prints warnings about semaphore waits which have lasted
too long. These can be used to track bugs which cause hangs.
@return a dummy parameter */
extern "C"
os_thread_ret_t
DECLARE_THREAD(srv_error_monitor_thread)(
/*=====================================*/
	void*	arg MY_ATTRIBUTE((unused)))
			/*!< in: a dummy parameter required by
			os_thread_create */
{
	/* number of successive fatal timeouts observed */
	ulint		fatal_cnt	= 0;
	lsn_t		old_lsn;
	lsn_t		new_lsn;
	int64_t		sig_count;
	/* longest waiting thread for a semaphore */
	os_thread_id_t	waiter		= os_thread_get_curr_id();
	os_thread_id_t	old_waiter	= waiter;
	/* the semaphore that is being waited for */
	const void*	sema		= NULL;
	const void*	old_sema	= NULL;

	ut_ad(!srv_read_only_mode);

	old_lsn = srv_start_lsn;

#ifdef UNIV_DEBUG_THREAD_CREATION
	ib::info() << "Error monitor thread starts, id "
		<< os_thread_pf(os_thread_get_curr_id());
#endif /* UNIV_DEBUG_THREAD_CREATION */

#ifdef UNIV_PFS_THREAD
	pfs_register_thread(srv_error_monitor_thread_key);
#endif /* UNIV_PFS_THREAD */
	srv_error_monitor_active = TRUE;

loop:
	/* Try to track a strange bug reported by Harald Fuchs and others,
	where the lsn seems to decrease at times */

	new_lsn = log_get_lsn();

	if (new_lsn < old_lsn) {
		ib::error() << "Old log sequence number " << old_lsn << " was"
			<< " greater than the new log sequence number "
			<< new_lsn << ". Please submit a bug report to"
			" http://bugs.mysql.com";
		ut_ad(0);
	}

	old_lsn = new_lsn;

	if (difftime(time(NULL), srv_last_monitor_time) > 60) {
		/* We referesh InnoDB Monitor values so that averages are
		printed from at most 60 last seconds */

		srv_refresh_innodb_monitor_stats();
	}

	/* Update the statistics collected for deciding LRU
	eviction policy. */
	buf_LRU_stat_update();

	/* In case mutex_exit is not a memory barrier, it is
	theoretically possible some threads are left waiting though
	the semaphore is already released. Wake up those threads: */

	sync_arr_wake_threads_if_sema_free();

	if (sync_array_print_long_waits(&waiter, &sema)
	    && sema == old_sema && os_thread_eq(waiter, old_waiter)) {
		fatal_cnt++;
		if (fatal_cnt > 10) {
			ib::fatal() << "Semaphore wait has lasted > "
				<< srv_fatal_semaphore_wait_threshold
				<< " seconds. We intentionally crash the"
				" server because it appears to be hung.";
		}
	} else {
		fatal_cnt = 0;
		old_waiter = waiter;
		old_sema = sema;
	}

	/* Flush stderr so that a database user gets the output
	to possible MySQL error file */

	fflush(stderr);

	sig_count = os_event_reset(srv_error_event);

	os_event_wait_time_low(srv_error_event, 1000000, sig_count);

	if (srv_shutdown_state < SRV_SHUTDOWN_CLEANUP) {

		goto loop;
	}

	srv_error_monitor_active = FALSE;

	/* We count the number of threads in os_thread_exit(). A created
	thread should always use that to exit and not use return() to exit. */

	os_thread_exit();

	OS_THREAD_DUMMY_RETURN;
}

/******************************************************************//**
Increment the server activity count. */
void
srv_inc_activity_count(void)
/*========================*/
{
	srv_sys->activity_count.inc();
}

/**********************************************************************//**
Check whether any background thread is active. If so return the thread
type.
@return SRV_NONE if all are suspended or have exited, thread
type if any are still active. */
srv_thread_type
srv_get_active_thread_type(void)
/*============================*/
{
	srv_thread_type ret = SRV_NONE;

	if (srv_read_only_mode) {
		return(SRV_NONE);
	}

	srv_sys_mutex_enter();

	for (ulint i = SRV_WORKER; i <= SRV_MASTER; ++i) {
		if (srv_sys->n_threads_active[i] != 0) {
			ret = static_cast<srv_thread_type>(i);
			break;
		}
	}

	srv_sys_mutex_exit();

	/* Check only on shutdown. */
	if (ret == SRV_NONE
	    && srv_shutdown_state != SRV_SHUTDOWN_NONE
	    && trx_purge_state() != PURGE_STATE_DISABLED
	    && trx_purge_state() != PURGE_STATE_EXIT) {

		ret = SRV_PURGE;
	}

	return(ret);
}

/**********************************************************************//**
Check whether any background thread are active. If so print which thread
is active. Send the threads wakeup signal.
@return name of thread that is active or NULL */
const char*
srv_any_background_threads_are_active(void)
/*=======================================*/
{
	const char*	thread_active = NULL;

	if (srv_read_only_mode) {
		if (srv_buf_resize_thread_active) {
			thread_active = "buf_resize_thread";
		}
		os_event_set(srv_buf_resize_event);
		return(thread_active);
	} else if (srv_error_monitor_active) {
		thread_active = "srv_error_monitor_thread";
	} else if (lock_sys->timeout_thread_active) {
		thread_active = "srv_lock_timeout thread";
	} else if (srv_monitor_active) {
		thread_active = "srv_monitor_thread";
	} else if (srv_buf_dump_thread_active) {
		thread_active = "buf_dump_thread";
	} else if (srv_buf_resize_thread_active) {
		thread_active = "buf_resize_thread";
	} else if (srv_dict_stats_thread_active) {
		thread_active = "dict_stats_thread";
	}

	os_event_set(srv_error_event);
	os_event_set(srv_monitor_event);
	os_event_set(srv_buf_dump_event);
	os_event_set(lock_sys->timeout_event);
	os_event_set(dict_stats_event);
	os_event_set(srv_buf_resize_event);

	return(thread_active);
}

/*******************************************************************//**
Tells the InnoDB server that there has been activity in the database
and wakes up the master thread if it is suspended (not sleeping). Used
in the MySQL interface. Note that there is a small chance that the master
thread stays suspended (we do not protect our operation with the
srv_sys_t->mutex, for performance reasons). */
void
srv_active_wake_master_thread_low()
/*===============================*/
{
	ut_ad(!srv_read_only_mode);
	ut_ad(!srv_sys_mutex_own());

	srv_inc_activity_count();

	if (srv_sys->n_threads_active[SRV_MASTER] == 0) {
		srv_slot_t*	slot;

		srv_sys_mutex_enter();

		slot = &srv_sys->sys_threads[SRV_MASTER_SLOT];

		/* Only if the master thread has been started. */

		if (slot->in_use) {
			ut_a(srv_slot_get_type(slot) == SRV_MASTER);

			if (slot->suspended) {

				slot->suspended = FALSE;

				++srv_sys->n_threads_active[SRV_MASTER];

				os_event_set(slot->event);
			}
		}

		srv_sys_mutex_exit();
	}
}

/*******************************************************************//**
Tells the purge thread that there has been activity in the database
and wakes up the purge thread if it is suspended (not sleeping).  Note
that there is a small chance that the purge thread stays suspended
(we do not protect our check with the srv_sys_t:mutex and the
purge_sys->latch, for performance reasons). */
void
srv_wake_purge_thread_if_not_active(void)
/*=====================================*/
{
	ut_ad(!srv_sys_mutex_own());

	if (purge_sys->state == PURGE_STATE_RUN
	    && srv_sys->n_threads_active[SRV_PURGE] == 0) {

		srv_release_threads(SRV_PURGE, 1);
	}
}

/*******************************************************************//**
Wakes up the master thread if it is suspended or being suspended. */
void
srv_wake_master_thread(void)
/*========================*/
{
	ut_ad(!srv_sys_mutex_own());

	srv_inc_activity_count();

	srv_release_threads(SRV_MASTER, 1);
}

/*******************************************************************//**
Get current server activity count. We don't hold srv_sys::mutex while
reading this value as it is only used in heuristics.
@return activity count. */
ulint
srv_get_activity_count(void)
/*========================*/
{
	return(srv_sys->activity_count);
}

/*******************************************************************//**
Check if there has been any activity.
@return FALSE if no change in activity counter. */
ibool
srv_check_activity(
/*===============*/
	ulint		old_activity_count)	/*!< in: old activity count */
{
	return(srv_sys->activity_count != old_activity_count);
}

/********************************************************************//**
The master thread is tasked to ensure that flush of log file happens
once every second in the background. This is to ensure that not more
than one second of trxs are lost in case of crash when
innodb_flush_logs_at_trx_commit != 1 */
static
void
srv_sync_log_buffer_in_background(void)
/*===================================*/
{
	time_t	current_time = time(NULL);

	srv_main_thread_op_info = "flushing log";
	if (difftime(current_time, srv_last_log_flush_time)
	    >= srv_flush_log_at_timeout) {
		log_buffer_sync_in_background(true);
		srv_last_log_flush_time = current_time;
		srv_log_writes_and_flush++;
	}
}

/********************************************************************//**
Make room in the table cache by evicting an unused table.
@return number of tables evicted. */
static
ulint
srv_master_evict_from_table_cache(
/*==============================*/
	ulint	pct_check)	/*!< in: max percent to check */
{
	ulint	n_tables_evicted = 0;

	rw_lock_x_lock(dict_operation_lock);

	dict_mutex_enter_for_mysql();

	n_tables_evicted = dict_make_room_in_cache(
		innobase_get_table_cache_size(), pct_check);

	dict_mutex_exit_for_mysql();

	rw_lock_x_unlock(dict_operation_lock);

	return(n_tables_evicted);
}

/*********************************************************************//**
This function prints progress message every 60 seconds during server
shutdown, for any activities that master thread is pending on. */
static
void
srv_shutdown_print_master_pending(
/*==============================*/
	ib_time_t*	last_print_time,	/*!< last time the function
						print the message */
	ulint		n_tables_to_drop,	/*!< number of tables to
						be dropped */
	ulint		n_bytes_merged)		/*!< number of change buffer
						just merged */
{
	ib_time_t	current_time;
	double		time_elapsed;

	current_time = ut_time();
	time_elapsed = ut_difftime(current_time, *last_print_time);

	if (time_elapsed > 60) {
		*last_print_time = ut_time();

		if (n_tables_to_drop) {
			ib::info() << "Waiting for " << n_tables_to_drop
				<< " table(s) to be dropped";
		}

		/* Check change buffer merge, we only wait for change buffer
		merge if it is a slow shutdown */
		if (!srv_fast_shutdown && n_bytes_merged) {
			ib::info() << "Waiting for change buffer merge to"
				" complete number of bytes of change buffer"
				" just merged: " << n_bytes_merged;
		}
	}
}

#ifdef UNIV_DEBUG
/** Waits in loop as long as master thread is disabled (debug) */
static
void
srv_master_do_disabled_loop(void)
{
	if (!srv_master_thread_disabled_debug) {
		/* We return here to avoid changing op_info. */
		return;
	}

	srv_main_thread_op_info = "disabled";

	while (srv_master_thread_disabled_debug) {
		os_event_set(srv_master_thread_disabled_event);
		if (srv_shutdown_state != SRV_SHUTDOWN_NONE) {
			break;
		}
		os_thread_sleep(100000);
	}

	srv_main_thread_op_info = "";
}

/** Disables master thread. It's used by:
	SET GLOBAL innodb_master_thread_disabled_debug = 1 (0).
@param[in]	thd		thread handle
@param[in]	var		pointer to system variable
@param[out]	var_ptr		where the formal string goes
@param[in]	save		immediate result from check function */
void
srv_master_thread_disabled_debug_update(
	THD*				thd,
	struct st_mysql_sys_var*	var,
	void*				var_ptr,
	const void*			save)
{
	/* This method is protected by mutex, as every SET GLOBAL .. */
	ut_ad(srv_master_thread_disabled_event != NULL);

	const bool disable = *static_cast<const my_bool*>(save);

	const int64_t sig_count = os_event_reset(
		srv_master_thread_disabled_event);

	srv_master_thread_disabled_debug = disable;

	if (disable) {
		os_event_wait_low(
			srv_master_thread_disabled_event, sig_count);
	}
}
#endif /* UNIV_DEBUG */

/*********************************************************************//**
Perform the tasks that the master thread is supposed to do when the
server is active. There are two types of tasks. The first category is
of such tasks which are performed at each inovcation of this function.
We assume that this function is called roughly every second when the
server is active. The second category is of such tasks which are
performed at some interval e.g.: purge, dict_LRU cleanup etc. */
static
void
srv_master_do_active_tasks(void)
/*============================*/
{
	ib_time_t	cur_time = ut_time();
	uintmax_t	counter_time = ut_time_us(NULL);

	/* First do the tasks that we are suppose to do at each
	invocation of this function. */

	++srv_main_active_loops;

	MONITOR_INC(MONITOR_MASTER_ACTIVE_LOOPS);

	/* ALTER TABLE in MySQL requires on Unix that the table handler
	can drop tables lazily after there no longer are SELECT
	queries to them. */
	srv_main_thread_op_info = "doing background drop tables";
	row_drop_tables_for_mysql_in_background();
	MONITOR_INC_TIME_IN_MICRO_SECS(
		MONITOR_SRV_BACKGROUND_DROP_TABLE_MICROSECOND, counter_time);

	ut_d(srv_master_do_disabled_loop());

	if (srv_shutdown_state > 0) {
		return;
	}

	/* make sure that there is enough reusable space in the redo
	log files */
	srv_main_thread_op_info = "checking free log space";
	log_free_check();

	/* Do an ibuf merge */
	srv_main_thread_op_info = "doing insert buffer merge";
	counter_time = ut_time_us(NULL);
	ibuf_merge_in_background(false);
	MONITOR_INC_TIME_IN_MICRO_SECS(
		MONITOR_SRV_IBUF_MERGE_MICROSECOND, counter_time);

	/* Flush logs if needed */
	srv_main_thread_op_info = "flushing log";
	srv_sync_log_buffer_in_background();
	MONITOR_INC_TIME_IN_MICRO_SECS(
		MONITOR_SRV_LOG_FLUSH_MICROSECOND, counter_time);

	/* Now see if various tasks that are performed at defined
	intervals need to be performed. */

	if (srv_shutdown_state > 0) {
		return;
	}

	if (srv_shutdown_state > 0) {
		return;
	}

	if (cur_time % SRV_MASTER_DICT_LRU_INTERVAL == 0) {
		srv_main_thread_op_info = "enforcing dict cache limit";
		ulint	n_evicted = srv_master_evict_from_table_cache(50);
		if (n_evicted != 0) {
			MONITOR_INC_VALUE(
				MONITOR_SRV_DICT_LRU_EVICT_COUNT, n_evicted);
		}
		MONITOR_INC_TIME_IN_MICRO_SECS(
			MONITOR_SRV_DICT_LRU_MICROSECOND, counter_time);
	}

	if (srv_shutdown_state > 0) {
		return;
	}

	/* Make a new checkpoint */
	if (cur_time % SRV_MASTER_CHECKPOINT_INTERVAL == 0) {
		srv_main_thread_op_info = "making checkpoint";
		log_checkpoint(TRUE, FALSE);
		MONITOR_INC_TIME_IN_MICRO_SECS(
			MONITOR_SRV_CHECKPOINT_MICROSECOND, counter_time);
	}
}

/*********************************************************************//**
Perform the tasks that the master thread is supposed to do whenever the
server is idle. We do check for the server state during this function
and if the server has entered the shutdown phase we may return from
the function without completing the required tasks.
Note that the server can move to active state when we are executing this
function but we don't check for that as we are suppose to perform more
or less same tasks when server is active. */
static
void
srv_master_do_idle_tasks(void)
/*==========================*/
{
	uintmax_t	counter_time;

	++srv_main_idle_loops;

	MONITOR_INC(MONITOR_MASTER_IDLE_LOOPS);


	/* ALTER TABLE in MySQL requires on Unix that the table handler
	can drop tables lazily after there no longer are SELECT
	queries to them. */
	counter_time = ut_time_us(NULL);
	srv_main_thread_op_info = "doing background drop tables";
	row_drop_tables_for_mysql_in_background();
	MONITOR_INC_TIME_IN_MICRO_SECS(
		MONITOR_SRV_BACKGROUND_DROP_TABLE_MICROSECOND,
			 counter_time);

	ut_d(srv_master_do_disabled_loop());

	if (srv_shutdown_state > 0) {
		return;
	}

	/* make sure that there is enough reusable space in the redo
	log files */
	srv_main_thread_op_info = "checking free log space";
	log_free_check();

	/* Do an ibuf merge */
	counter_time = ut_time_us(NULL);
	srv_main_thread_op_info = "doing insert buffer merge";
	ibuf_merge_in_background(true);
	MONITOR_INC_TIME_IN_MICRO_SECS(
		MONITOR_SRV_IBUF_MERGE_MICROSECOND, counter_time);

	if (srv_shutdown_state > 0) {
		return;
	}

	srv_main_thread_op_info = "enforcing dict cache limit";
	ulint	n_evicted = srv_master_evict_from_table_cache(100);
	if (n_evicted != 0) {
		MONITOR_INC_VALUE(
			MONITOR_SRV_DICT_LRU_EVICT_COUNT, n_evicted);
	}
	MONITOR_INC_TIME_IN_MICRO_SECS(
		MONITOR_SRV_DICT_LRU_MICROSECOND, counter_time);

	/* Flush logs if needed */
	srv_sync_log_buffer_in_background();
	MONITOR_INC_TIME_IN_MICRO_SECS(
		MONITOR_SRV_LOG_FLUSH_MICROSECOND, counter_time);

	if (srv_shutdown_state > 0) {
		return;
	}

	/* Make a new checkpoint */
	srv_main_thread_op_info = "making checkpoint";
	log_checkpoint(TRUE, FALSE);
	MONITOR_INC_TIME_IN_MICRO_SECS(MONITOR_SRV_CHECKPOINT_MICROSECOND,
				       counter_time);
}

/*********************************************************************//**
Perform the tasks during shutdown. The tasks that we do at shutdown
depend on srv_fast_shutdown:
2 => very fast shutdown => do no book keeping
1 => normal shutdown => clear drop table queue and make checkpoint
0 => slow shutdown => in addition to above do complete purge and ibuf
merge
@return TRUE if some work was done. FALSE otherwise */
static
ibool
srv_master_do_shutdown_tasks(
/*=========================*/
	ib_time_t*	last_print_time)/*!< last time the function
					print the message */
{
	ulint		n_bytes_merged = 0;
	ulint		n_tables_to_drop = 0;

	ut_ad(!srv_read_only_mode);

	++srv_main_shutdown_loops;

	ut_a(srv_shutdown_state > 0);

	/* In very fast shutdown none of the following is necessary */
	if (srv_fast_shutdown == 2) {
		return(FALSE);
	}

	/* ALTER TABLE in MySQL requires on Unix that the table handler
	can drop tables lazily after there no longer are SELECT
	queries to them. */
	srv_main_thread_op_info = "doing background drop tables";
	n_tables_to_drop = row_drop_tables_for_mysql_in_background();

	/* make sure that there is enough reusable space in the redo
	log files */
	srv_main_thread_op_info = "checking free log space";
	log_free_check();

	/* In case of normal shutdown we don't do ibuf merge or purge */
	if (srv_fast_shutdown == 1) {
		goto func_exit;
	}

	/* Do an ibuf merge */
	srv_main_thread_op_info = "doing insert buffer merge";
	n_bytes_merged = ibuf_merge_in_background(true);

	/* Flush logs if needed */
	srv_sync_log_buffer_in_background();

func_exit:
	/* Make a new checkpoint about once in 10 seconds */
	srv_main_thread_op_info = "making checkpoint";
	log_checkpoint(TRUE, FALSE);

	/* Print progress message every 60 seconds during shutdown */
	if (srv_shutdown_state > 0 && srv_print_verbose_log) {
		srv_shutdown_print_master_pending(
			last_print_time, n_tables_to_drop, n_bytes_merged);
	}

	return(n_bytes_merged || n_tables_to_drop);
}

/*********************************************************************//**
Puts master thread to sleep. At this point we are using polling to
service various activities. Master thread sleeps for one second before
checking the state of the server again */
static
void
srv_master_sleep(void)
/*==================*/
{
	srv_main_thread_op_info = "sleeping";
	os_thread_sleep(1000000);
	srv_main_thread_op_info = "";
}

/*********************************************************************//**
The master thread controlling the server.
@return a dummy parameter */
extern "C"
os_thread_ret_t
DECLARE_THREAD(srv_master_thread)(
/*==============================*/
	void*	arg MY_ATTRIBUTE((unused)))
			/*!< in: a dummy parameter required by
			os_thread_create */
{
	my_thread_init();
	DBUG_ENTER("srv_master_thread");

	srv_slot_t*	slot;
	ulint		old_activity_count = srv_get_activity_count();
	ib_time_t	last_print_time;

	ut_ad(!srv_read_only_mode);

#ifdef UNIV_DEBUG_THREAD_CREATION
	ib::info() << "Master thread starts, id "
		<< os_thread_pf(os_thread_get_curr_id());
#endif /* UNIV_DEBUG_THREAD_CREATION */

#ifdef UNIV_PFS_THREAD
	pfs_register_thread(srv_master_thread_key);
#endif /* UNIV_PFS_THREAD */

	srv_main_thread_process_no = os_proc_get_number();
	srv_main_thread_id = os_thread_pf(os_thread_get_curr_id());

	slot = srv_reserve_slot(SRV_MASTER);
	ut_a(slot == srv_sys->sys_threads);

	last_print_time = ut_time();
loop:
	if (srv_force_recovery >= SRV_FORCE_NO_BACKGROUND) {
		goto suspend_thread;
	}

	while (srv_shutdown_state == SRV_SHUTDOWN_NONE) {

		srv_master_sleep();

		MONITOR_INC(MONITOR_MASTER_THREAD_SLEEP);

		if (srv_check_activity(old_activity_count)) {
			old_activity_count = srv_get_activity_count();
			srv_master_do_active_tasks();
		} else {
			srv_master_do_idle_tasks();
		}
	}

	while (srv_shutdown_state != SRV_SHUTDOWN_EXIT_THREADS
	       && srv_master_do_shutdown_tasks(&last_print_time)) {

		/* Shouldn't loop here in case of very fast shutdown */
		ut_ad(srv_fast_shutdown < 2);
	}

suspend_thread:
	srv_main_thread_op_info = "suspending";

	srv_suspend_thread(slot);

	/* DO NOT CHANGE THIS STRING. innobase_start_or_create_for_mysql()
	waits for database activity to die down when converting < 4.1.x
	databases, and relies on this string being exactly as it is. InnoDB
	manual also mentions this string in several places. */
	srv_main_thread_op_info = "waiting for server activity";

	os_event_wait(slot->event);

	if (srv_shutdown_state != SRV_SHUTDOWN_EXIT_THREADS) {
		goto loop;
	}

	my_thread_end();
	os_thread_exit();
	DBUG_RETURN(0);
}

/**
Check if purge should stop.
@return true if it should shutdown. */
static
bool
srv_purge_should_exit(
	ulint		n_purged)	/*!< in: pages purged in last batch */
{
	switch (srv_shutdown_state) {
	case SRV_SHUTDOWN_NONE:
		/* Normal operation. */
		break;

	case SRV_SHUTDOWN_CLEANUP:
	case SRV_SHUTDOWN_EXIT_THREADS:
		/* Exit unless slow shutdown requested or all done. */
		return(srv_fast_shutdown != 0 || n_purged == 0);

	case SRV_SHUTDOWN_LAST_PHASE:
	case SRV_SHUTDOWN_FLUSH_PHASE:
		ut_error;
	}

	return(false);
}

/*********************************************************************//**
Fetch and execute a task from the work queue.
@return true if a task was executed */
static
bool
srv_task_execute(void)
/*==================*/
{
	que_thr_t*	thr = NULL;

	ut_ad(!srv_read_only_mode);
	ut_a(srv_force_recovery < SRV_FORCE_NO_BACKGROUND);

	mutex_enter(&srv_sys->tasks_mutex);

	if (UT_LIST_GET_LEN(srv_sys->tasks) > 0) {

		thr = UT_LIST_GET_FIRST(srv_sys->tasks);

		ut_a(que_node_get_type(thr->child) == QUE_NODE_PURGE);

		UT_LIST_REMOVE(srv_sys->tasks, thr);
	}

	mutex_exit(&srv_sys->tasks_mutex);

	if (thr != NULL) {

		que_run_threads(thr);

		os_atomic_inc_ulint(
			&purge_sys->pq_mutex, &purge_sys->n_completed, 1);
	}

	return(thr != NULL);
}

/*********************************************************************//**
Worker thread that reads tasks from the work queue and executes them.
@return a dummy parameter */
extern "C"
os_thread_ret_t
DECLARE_THREAD(srv_worker_thread)(
/*==============================*/
	void*	arg MY_ATTRIBUTE((unused)))	/*!< in: a dummy parameter
						required by os_thread_create */
{
	srv_slot_t*	slot;

	ut_ad(!srv_read_only_mode);
	ut_a(srv_force_recovery < SRV_FORCE_NO_BACKGROUND);
	my_thread_init();
	THD *thd= create_thd(false, true, true, srv_worker_thread_key.m_value);

#ifdef UNIV_DEBUG_THREAD_CREATION
	ib::info() << "Worker thread starting, id "
		<< os_thread_pf(os_thread_get_curr_id());
#endif /* UNIV_DEBUG_THREAD_CREATION */

	slot = srv_reserve_slot(SRV_WORKER);

	ut_a(srv_n_purge_threads > 1);

	srv_sys_mutex_enter();

	ut_a(srv_sys->n_threads_active[SRV_WORKER] < srv_n_purge_threads);

	srv_sys_mutex_exit();

	/* We need to ensure that the worker threads exit after the
	purge coordinator thread. Otherwise the purge coordinaor can
	end up waiting forever in trx_purge_wait_for_workers_to_complete() */

	do {
		srv_suspend_thread(slot);

		os_event_wait(slot->event);

		if (srv_task_execute()) {

			/* If there are tasks in the queue, wakeup
			the purge coordinator thread. */

			srv_wake_purge_thread_if_not_active();
		}

		/* Note: we are checking the state without holding the
		purge_sys->latch here. */
	} while (purge_sys->state != PURGE_STATE_EXIT);

	srv_free_slot(slot);

	rw_lock_x_lock(&purge_sys->latch);

	ut_a(!purge_sys->running);
	ut_a(purge_sys->state == PURGE_STATE_EXIT);
	ut_a(srv_shutdown_state > SRV_SHUTDOWN_NONE);

	rw_lock_x_unlock(&purge_sys->latch);

#ifdef UNIV_DEBUG_THREAD_CREATION
	ib::info() << "Purge worker thread exiting, id "
		<< os_thread_pf(os_thread_get_curr_id());
#endif /* UNIV_DEBUG_THREAD_CREATION */

	destroy_thd(thd);
        my_thread_end();
	/* We count the number of threads in os_thread_exit(). A created
	thread should always use that to exit and not use return() to exit. */
	os_thread_exit();

	OS_THREAD_DUMMY_RETURN;	/* Not reached, avoid compiler warning */
}

/*********************************************************************//**
Do the actual purge operation.
@return length of history list before the last purge batch. */
static
ulint
srv_do_purge(
/*=========*/
	ulint		n_threads,	/*!< in: number of threads to use */
	ulint*		n_total_purged)	/*!< in/out: total pages purged */
{
	ulint		n_pages_purged;

	static ulint	count = 0;
	static ulint	n_use_threads = 0;
	static ulint	rseg_history_len = 0;
	ulint		old_activity_count = srv_get_activity_count();

	ut_a(n_threads > 0);
	ut_ad(!srv_read_only_mode);

	/* Purge until there are no more records to purge and there is
	no change in configuration or server state. If the user has
	configured more than one purge thread then we treat that as a
	pool of threads and only use the extra threads if purge can't
	keep up with updates. */

	if (n_use_threads == 0) {
		n_use_threads = n_threads;
	}

	do {
		if (trx_sys->rseg_history_len > rseg_history_len
		    || (srv_max_purge_lag > 0
			&& rseg_history_len > srv_max_purge_lag)) {

			/* History length is now longer than what it was
			when we took the last snapshot. Use more threads. */

			if (n_use_threads < n_threads) {
				++n_use_threads;
			}

		} else if (srv_check_activity(old_activity_count)
			   && n_use_threads > 1) {

			/* History length same or smaller since last snapshot,
			use fewer threads. */

			--n_use_threads;

			old_activity_count = srv_get_activity_count();
		}

		/* Ensure that the purge threads are less than what
		was configured. */

		ut_a(n_use_threads > 0);
		ut_a(n_use_threads <= n_threads);

		/* Take a snapshot of the history list before purge. */
		if ((rseg_history_len = trx_sys->rseg_history_len) == 0) {
			break;
		}

		ulint	undo_trunc_freq =
			purge_sys->undo_trunc.get_rseg_truncate_frequency();

		ulint	rseg_truncate_frequency = ut_min(
			static_cast<ulint>(srv_purge_rseg_truncate_frequency),
			undo_trunc_freq);

		n_pages_purged = trx_purge(
			n_use_threads, srv_purge_batch_size,
			(++count % rseg_truncate_frequency) == 0);

		*n_total_purged += n_pages_purged;

	} while (!srv_purge_should_exit(n_pages_purged)
		 && n_pages_purged > 0
		 && purge_sys->state == PURGE_STATE_RUN);

	return(rseg_history_len);
}

/*********************************************************************//**
Suspend the purge coordinator thread. */
static
void
srv_purge_coordinator_suspend(
/*==========================*/
	srv_slot_t*	slot,			/*!< in/out: Purge coordinator
						thread slot */
	ulint		rseg_history_len)	/*!< in: history list length
						before last purge */
{
	ut_ad(!srv_read_only_mode);
	ut_a(slot->type == SRV_PURGE);

	bool		stop = false;

	/** Maximum wait time on the purge event, in micro-seconds. */
	static const ulint SRV_PURGE_MAX_TIMEOUT = 10000;

	int64_t		sig_count = srv_suspend_thread(slot);

	do {
		ulint		ret;

		rw_lock_x_lock(&purge_sys->latch);

		purge_sys->running = false;

		rw_lock_x_unlock(&purge_sys->latch);

		/* We don't wait right away on the the non-timed wait because
		we want to signal the thread that wants to suspend purge. */

		if (stop) {
			os_event_wait_low(slot->event, sig_count);
			ret = 0;
		} else if (rseg_history_len <= trx_sys->rseg_history_len) {
			ret = os_event_wait_time_low(
				slot->event, SRV_PURGE_MAX_TIMEOUT, sig_count);
		} else {
			/* We don't want to waste time waiting, if the
			history list increased by the time we got here,
			unless purge has been stopped. */
			ret = 0;
		}

		srv_sys_mutex_enter();

		/* The thread can be in state !suspended after the timeout
		but before this check if another thread sent a wakeup signal. */

		if (slot->suspended) {
			slot->suspended = FALSE;
			++srv_sys->n_threads_active[slot->type];
			ut_a(srv_sys->n_threads_active[slot->type] == 1);
		}

		srv_sys_mutex_exit();

		sig_count = srv_suspend_thread(slot);

		rw_lock_x_lock(&purge_sys->latch);

		stop = (srv_shutdown_state == SRV_SHUTDOWN_NONE
			&& purge_sys->state == PURGE_STATE_STOP);

		if (!stop) {
			ut_a(purge_sys->n_stop == 0);
			purge_sys->running = true;
		} else {
			ut_a(purge_sys->n_stop > 0);

			/* Signal that we are suspended. */
			os_event_set(purge_sys->event);
		}

		rw_lock_x_unlock(&purge_sys->latch);

		if (ret == OS_SYNC_TIME_EXCEEDED) {

			/* No new records added since wait started then simply
			wait for new records. The magic number 5000 is an
			approximation for the case where we have cached UNDO
			log records which prevent truncate of the UNDO
			segments. */

			if (rseg_history_len == trx_sys->rseg_history_len
			    && trx_sys->rseg_history_len < 5000) {

				stop = true;
			}
		}

	} while (stop);

	srv_sys_mutex_enter();

	if (slot->suspended) {
		slot->suspended = FALSE;
		++srv_sys->n_threads_active[slot->type];
		ut_a(srv_sys->n_threads_active[slot->type] == 1);
	}

	srv_sys_mutex_exit();
}

/*********************************************************************//**
Purge coordinator thread that schedules the purge tasks.
@return a dummy parameter */
extern "C"
os_thread_ret_t
DECLARE_THREAD(srv_purge_coordinator_thread)(
/*=========================================*/
	void*	arg MY_ATTRIBUTE((unused)))	/*!< in: a dummy parameter
						required by os_thread_create */
{
	my_thread_init();
	THD *thd= create_thd(false, true, true, srv_purge_thread_key.m_value);
	srv_slot_t*	slot;
	ulint           n_total_purged = ULINT_UNDEFINED;

	ut_ad(!srv_read_only_mode);
	ut_a(srv_n_purge_threads >= 1);
	ut_a(trx_purge_state() == PURGE_STATE_INIT);
	ut_a(srv_force_recovery < SRV_FORCE_NO_BACKGROUND);

	rw_lock_x_lock(&purge_sys->latch);

	purge_sys->running = true;
	purge_sys->state = PURGE_STATE_RUN;

	rw_lock_x_unlock(&purge_sys->latch);

#ifdef UNIV_DEBUG_THREAD_CREATION
	ib::info() << "Purge coordinator thread created, id "
		<< os_thread_pf(os_thread_get_curr_id());
#endif /* UNIV_DEBUG_THREAD_CREATION */

	slot = srv_reserve_slot(SRV_PURGE);

	ulint	rseg_history_len = trx_sys->rseg_history_len;

	do {
		/* If there are no records to purge or the last
		purge didn't purge any records then wait for activity. */

		if (srv_shutdown_state == SRV_SHUTDOWN_NONE
		    && (purge_sys->state == PURGE_STATE_STOP
			|| n_total_purged == 0)) {

			srv_purge_coordinator_suspend(slot, rseg_history_len);
		}

		if (srv_purge_should_exit(n_total_purged)) {
			ut_a(!slot->suspended);
			break;
		}

		n_total_purged = 0;

		rseg_history_len = srv_do_purge(
			srv_n_purge_threads, &n_total_purged);

	} while (!srv_purge_should_exit(n_total_purged));

	/* Ensure that we don't jump out of the loop unless the
	exit condition is satisfied. */

	ut_a(srv_purge_should_exit(n_total_purged));

	ulint	n_pages_purged = ULINT_MAX;

	/* Ensure that all records are purged if it is not a fast shutdown.
	This covers the case where a record can be added after we exit the
	loop above. */
	while (srv_fast_shutdown == 0 && n_pages_purged > 0) {
		n_pages_purged = trx_purge(1, srv_purge_batch_size, false);
	}

#ifdef UNIV_DEBUG
	if (srv_fast_shutdown == 0) {
		trx_commit_disallowed = true;
	}
#endif /* UNIV_DEBUG */

	/* This trx_purge is called to remove any undo records (added by
	background threads) after completion of the above loop. When
	srv_fast_shutdown != 0, a large batch size can cause significant
	delay in shutdown ,so reducing the batch size to magic number 20
	(which was default in 5.5), which we hope will be sufficient to
	remove all the undo records */
	const	uint temp_batch_size = 20;

	n_pages_purged = trx_purge(1, srv_purge_batch_size <= temp_batch_size
				      ? srv_purge_batch_size : temp_batch_size,
				   true);
	ut_a(n_pages_purged == 0 || srv_fast_shutdown != 0);

	/* The task queue should always be empty, independent of fast
	shutdown state. */
	ut_a(srv_get_task_queue_length() == 0);

	srv_free_slot(slot);

	/* Note that we are shutting down. */
	rw_lock_x_lock(&purge_sys->latch);

	purge_sys->state = PURGE_STATE_EXIT;

	/* If there are any pending undo-tablespace truncate then clear
	it off as we plan to shutdown the purge thread. */
	purge_sys->undo_trunc.clear();

	purge_sys->running = false;

	rw_lock_x_unlock(&purge_sys->latch);

#ifdef UNIV_DEBUG_THREAD_CREATION
	ib::info() << "Purge coordinator exiting, id "
		<< os_thread_pf(os_thread_get_curr_id());
#endif /* UNIV_DEBUG_THREAD_CREATION */

	/* Ensure that all the worker threads quit. */
	if (srv_n_purge_threads > 1) {
		srv_release_threads(SRV_WORKER, srv_n_purge_threads - 1);
	}

	destroy_thd(thd);
	my_thread_end();
	/* We count the number of threads in os_thread_exit(). A created
	thread should always use that to exit and not use return() to exit. */
	os_thread_exit();

	OS_THREAD_DUMMY_RETURN;	/* Not reached, avoid compiler warning */
}

/**********************************************************************//**
Enqueues a task to server task queue and releases a worker thread, if there
is a suspended one. */
void
srv_que_task_enqueue_low(
/*=====================*/
	que_thr_t*	thr)	/*!< in: query thread */
{
	ut_ad(!srv_read_only_mode);
	mutex_enter(&srv_sys->tasks_mutex);

	UT_LIST_ADD_LAST(srv_sys->tasks, thr);

	mutex_exit(&srv_sys->tasks_mutex);

	srv_release_threads(SRV_WORKER, 1);
}

/**********************************************************************//**
Get count of tasks in the queue.
@return number of tasks in queue */
ulint
srv_get_task_queue_length(void)
/*===========================*/
{
	ulint	n_tasks;

	ut_ad(!srv_read_only_mode);

	mutex_enter(&srv_sys->tasks_mutex);

	n_tasks = UT_LIST_GET_LEN(srv_sys->tasks);

	mutex_exit(&srv_sys->tasks_mutex);

	return(n_tasks);
}

/**********************************************************************//**
Wakeup the purge threads. */
void
srv_purge_wakeup(void)
/*==================*/
{
	ut_ad(!srv_read_only_mode);

	if (srv_force_recovery < SRV_FORCE_NO_BACKGROUND) {

		srv_release_threads(SRV_PURGE, 1);

		if (srv_n_purge_threads > 1) {
			ulint	n_workers = srv_n_purge_threads - 1;

			srv_release_threads(SRV_WORKER, n_workers);
		}
	}
}

/** Check if tablespace is being truncated.
(Ignore system-tablespace as we don't re-create the tablespace
and so some of the action that are suppressed by this function
for independent tablespace are not applicable to system-tablespace).
@param	space_id	space_id to check for truncate action
@return true		if being truncated, false if not being
			truncated or tablespace is system-tablespace. */
bool
srv_is_tablespace_truncated(ulint space_id)
{
	if (is_system_tablespace(space_id)) {
		return(false);
	}

	return(truncate_t::is_tablespace_truncated(space_id)
	       || undo::Truncate::is_tablespace_truncated(space_id));

}

/** Check if tablespace was truncated.
@param[in]	space	space object to check for truncate action
@return true if tablespace was truncated and we still have an active
MLOG_TRUNCATE REDO log record. */
bool
srv_was_tablespace_truncated(const fil_space_t* space)
{
	if (space == NULL) {
		ut_ad(0);
		return(false);
	}

	bool	has_shared_space = FSP_FLAGS_GET_SHARED(space->flags);

	if (is_system_tablespace(space->id) || has_shared_space) {
		return(false);
	}

	return(truncate_t::was_tablespace_truncated(space->id));
}

/** Call exit(3) */
void
srv_fatal_error()
{

	ib::error() << "Cannot continue operation.";

	fflush(stderr);

	ut_d(innodb_calling_exit = true);

	srv_shutdown_all_bg_threads();

	exit(3);
}

/** Check whether given space id is undo tablespace id
@param[in]	space_id	space id to check
@return true if it is undo tablespace else false. */
bool
srv_is_undo_tablespace(
	ulint	space_id)
{
	if (srv_undo_space_id_start == 0) {
		return(false);
	}

	return(space_id >= srv_undo_space_id_start
	       && space_id < (srv_undo_space_id_start
			      + srv_undo_tablespaces_open));
}

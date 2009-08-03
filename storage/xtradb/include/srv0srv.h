/*****************************************************************************

Copyright (c) 1995, 2009, Innobase Oy. All Rights Reserved.
Copyright (c) 2008, Google Inc.

Portions of this file contain modifications contributed and copyrighted by
Google, Inc. Those modifications are gratefully acknowledged and are described
briefly in the InnoDB documentation. The contributions by Google are
incorporated with their permission, and subject to the conditions contained in
the file COPYING.Google.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc., 59 Temple
Place, Suite 330, Boston, MA 02111-1307 USA

*****************************************************************************/

/******************************************************
The server main program

Created 10/10/1995 Heikki Tuuri
*******************************************************/

#ifndef srv0srv_h
#define srv0srv_h

#include "univ.i"
#include "sync0sync.h"
#include "os0sync.h"
#include "que0types.h"
#include "trx0types.h"

extern const char*	srv_main_thread_op_info;

/* Prefix used by MySQL to indicate pre-5.1 table name encoding */
extern const char	srv_mysql50_table_name_prefix[9];

/* When this event is set the lock timeout and InnoDB monitor
thread starts running */
extern os_event_t	srv_lock_timeout_thread_event;

/* If the last data file is auto-extended, we add this many pages to it
at a time */
#define SRV_AUTO_EXTEND_INCREMENT	\
	(srv_auto_extend_increment * ((1024 * 1024) / UNIV_PAGE_SIZE))

/* This is set to TRUE if the MySQL user has set it in MySQL */
extern ibool	srv_lower_case_table_names;

/* Mutex for locking srv_monitor_file */
extern mutex_t	srv_monitor_file_mutex;
/* Temporary file for innodb monitor output */
extern FILE*	srv_monitor_file;
/* Mutex for locking srv_dict_tmpfile.
This mutex has a very high rank; threads reserving it should not
be holding any InnoDB latches. */
extern mutex_t	srv_dict_tmpfile_mutex;
/* Temporary file for output from the data dictionary */
extern FILE*	srv_dict_tmpfile;
/* Mutex for locking srv_misc_tmpfile.
This mutex has a very low rank; threads reserving it should not
acquire any further latches or sleep before releasing this one. */
extern mutex_t	srv_misc_tmpfile_mutex;
/* Temporary file for miscellanous diagnostic output */
extern FILE*	srv_misc_tmpfile;

/* Server parameters which are read from the initfile */

extern char*	srv_data_home;
#ifdef UNIV_LOG_ARCHIVE
extern char*	srv_arch_dir;
#endif /* UNIV_LOG_ARCHIVE */

/* store to its own file each table created by an user; data
dictionary tables are in the system tablespace 0 */
extern my_bool	srv_file_per_table;
/* The file format to use on new *.ibd files. */
extern ulint	srv_file_format;
/* Whether to check file format during startup.*/
extern ulint	srv_check_file_format_at_startup;
/* Place locks to records only i.e. do not use next-key locking except
on duplicate key checking and foreign key checking */
extern ibool	srv_locks_unsafe_for_binlog;

extern ulint	srv_n_data_files;
extern char**	srv_data_file_names;
extern ulint*	srv_data_file_sizes;
extern ulint*	srv_data_file_is_raw_partition;

extern ibool	srv_extra_undoslots;

extern ibool	srv_auto_extend_last_data_file;
extern ulint	srv_last_file_size_max;
extern ulong	srv_auto_extend_increment;

extern ibool	srv_created_new_raw;

#define SRV_NEW_RAW	1
#define SRV_OLD_RAW	2

extern char**	srv_log_group_home_dirs;

extern ulint	srv_n_log_groups;
extern ulint	srv_n_log_files;
extern ulint	srv_log_file_size;
extern ulint	srv_log_buffer_size;
extern ulong	srv_flush_log_at_trx_commit;

extern ulint    srv_show_locks_held;
extern ulint    srv_show_verbose_locks;

/* The sort order table of the MySQL latin1_swedish_ci character set
collation */
extern const byte*	srv_latin1_ordering;
extern my_bool	srv_use_sys_malloc;
extern ulint	srv_buf_pool_size;	/* requested size in bytes */
extern ulint	srv_buf_pool_old_size;	/* previously requested size */
extern ulint	srv_buf_pool_curr_size;	/* current size in bytes */
extern ulint	srv_mem_pool_size;
extern ulint	srv_lock_table_size;

extern ibool	srv_thread_concurrency_timer_based;

extern ulint	srv_n_file_io_threads;
extern ulint	srv_n_read_io_threads;
extern ulint	srv_n_write_io_threads;

#ifdef UNIV_LOG_ARCHIVE
extern ibool	srv_log_archive_on;
extern ibool	srv_archive_recovery;
extern dulint	srv_archive_recovery_limit_lsn;
#endif /* UNIV_LOG_ARCHIVE */

extern char*	srv_file_flush_method_str;
extern ulint	srv_unix_file_flush_method;
extern ulint	srv_win_file_flush_method;

extern ulint	srv_max_n_open_files;

extern ulint	srv_max_dirty_pages_pct;

extern ulint	srv_force_recovery;
extern ulong	srv_thread_concurrency;
extern ulong	srv_commit_concurrency;

extern ulint	srv_max_n_threads;

extern lint	srv_conc_n_threads;

extern ulint	srv_fast_shutdown;	 /* If this is 1, do not do a
					 purge and index buffer merge.
					 If this 2, do not even flush the
					 buffer pool to data files at the
					 shutdown: we effectively 'crash'
					 InnoDB (but lose no committed
					 transactions). */
extern ibool	srv_innodb_status;

extern unsigned long long	srv_stats_sample_pages;
extern ulint	srv_stats_method;
#define SRV_STATS_METHOD_NULLS_EQUAL     0
#define SRV_STATS_METHOD_NULLS_NOT_EQUAL 1
#define SRV_STATS_METHOD_IGNORE_NULLS    2
extern ulint	srv_stats_auto_update;

extern ibool	srv_use_doublewrite_buf;
extern ibool	srv_use_checksums;

extern ibool	srv_set_thread_priorities;
extern int	srv_query_thread_priority;

extern ulong	srv_max_buf_pool_modified_pct;
extern ulong	srv_max_purge_lag;

extern ulong	srv_replication_delay;

extern ulint	srv_io_capacity;
extern long long	srv_ibuf_max_size;
extern ulint	srv_ibuf_active_contract;
extern ulint	srv_ibuf_accel_rate;
extern ulint	srv_flush_neighbor_pages;
extern ulint	srv_enable_unsafe_group_commit;
extern ulint	srv_read_ahead;
extern ulint	srv_adaptive_checkpoint;

extern ulint	srv_expand_import;

extern ulint	srv_extra_rsegments;
extern ulint	srv_dict_size_limit;
/*-------------------------------------------*/

extern ulint	srv_n_rows_inserted;
extern ulint	srv_n_rows_updated;
extern ulint	srv_n_rows_deleted;
extern ulint	srv_n_rows_read;

extern ibool	srv_print_innodb_monitor;
extern ibool	srv_print_innodb_lock_monitor;
extern ibool	srv_print_innodb_tablespace_monitor;
extern ibool	srv_print_verbose_log;
extern ibool	srv_print_innodb_table_monitor;

extern ibool	srv_lock_timeout_and_monitor_active;
extern ibool	srv_error_monitor_active;

extern ulong	srv_n_spin_wait_rounds;
extern ulong	srv_n_free_tickets_to_enter;
extern ulong	srv_thread_sleep_delay;
extern ulint	srv_spin_wait_delay;
extern ibool	srv_priority_boost;

extern	ulint	srv_mem_pool_size;
extern	ulint	srv_lock_table_size;

#ifdef UNIV_DEBUG
extern	ibool	srv_print_thread_releases;
extern	ibool	srv_print_lock_waits;
extern	ibool	srv_print_buf_io;
extern	ibool	srv_print_log_io;
extern	ibool	srv_print_latch_waits;
#else /* UNIV_DEBUG */
# define srv_print_thread_releases	FALSE
# define srv_print_lock_waits		FALSE
# define srv_print_buf_io		FALSE
# define srv_print_log_io		FALSE
# define srv_print_latch_waits		FALSE
#endif /* UNIV_DEBUG */

extern ulint	srv_activity_count;
extern ulint	srv_fatal_semaphore_wait_threshold;
extern ulint	srv_dml_needed_delay;

extern mutex_t*	kernel_mutex_temp;/* mutex protecting the server, trx structs,
				query threads, and lock table: we allocate
				it from dynamic memory to get it to the
				same DRAM page as other hotspot semaphores */
#define kernel_mutex (*kernel_mutex_temp)

#define SRV_MAX_N_IO_THREADS	100

/* Array of English strings describing the current state of an
i/o handler thread */
extern const char* srv_io_thread_op_info[];
extern const char* srv_io_thread_function[];

/* the number of the log write requests done */
extern ulint srv_log_write_requests;

/* the number of physical writes to the log performed */
extern ulint srv_log_writes;

/* amount of data written to the log files in bytes */
extern ulint srv_os_log_written;

/* amount of writes being done to the log files */
extern ulint srv_os_log_pending_writes;

/* we increase this counter, when there we don't have enough space in the
log buffer and have to flush it */
extern ulint srv_log_waits;

/* variable that counts amount of data read in total (in bytes) */
extern ulint srv_data_read;

/* here we count the amount of data written in total (in bytes) */
extern ulint srv_data_written;

/* this variable counts the amount of times, when the doublewrite buffer
was flushed */
extern ulint srv_dblwr_writes;

/* here we store the number of pages that have been flushed to the
doublewrite buffer */
extern ulint srv_dblwr_pages_written;

/* in this variable we store the number of write requests issued */
extern ulint srv_buf_pool_write_requests;

/* here we store the number of times when we had to wait for a free page
in the buffer pool. It happens when the buffer pool is full and we need
to make a flush, in order to be able to read or create a page. */
extern ulint srv_buf_pool_wait_free;

/* variable to count the number of pages that were written from the
buffer pool to disk */
extern ulint srv_buf_pool_flushed;

/* variable to count the number of buffer pool reads that led to the
reading of a disk page */
extern ulint srv_buf_pool_reads;

/* variable to count the number of sequential read-aheads were done */
extern ulint srv_read_ahead_seq;

/* variable to count the number of random read-aheads were done */
extern ulint srv_read_ahead_rnd;

/* In this structure we store status variables to be passed to MySQL */
typedef struct export_var_struct export_struc;

extern export_struc export_vars;

typedef struct srv_sys_struct	srv_sys_t;

/* The server system */
extern srv_sys_t*	srv_sys;

/* Alternatives for the file flush option in Unix; see the InnoDB manual
about what these mean */
#define SRV_UNIX_FSYNC		1	/* This is the default */
#define SRV_UNIX_O_DSYNC	2
#define SRV_UNIX_LITTLESYNC	3
#define SRV_UNIX_NOSYNC		4
#define SRV_UNIX_O_DIRECT	5

/* Alternatives for file i/o in Windows */
#define SRV_WIN_IO_NORMAL		1
#define SRV_WIN_IO_UNBUFFERED		2	/* This is the default */

/* Alternatives for srv_force_recovery. Non-zero values are intended
to help the user get a damaged database up so that he can dump intact
tables and rows with SELECT INTO OUTFILE. The database must not otherwise
be used with these options! A bigger number below means that all precautions
of lower numbers are included. */

#define SRV_FORCE_IGNORE_CORRUPT 1	/* let the server run even if it
					detects a corrupt page */
#define SRV_FORCE_NO_BACKGROUND	2	/* prevent the main thread from
					running: if a crash would occur
					in purge, this prevents it */
#define SRV_FORCE_NO_TRX_UNDO	3	/* do not run trx rollback after
					recovery */
#define SRV_FORCE_NO_IBUF_MERGE	4	/* prevent also ibuf operations:
					if they would cause a crash, better
					not do them */
#define	SRV_FORCE_NO_UNDO_LOG_SCAN 5	/* do not look at undo logs when
					starting the database: InnoDB will
					treat even incomplete transactions
					as committed */
#define SRV_FORCE_NO_LOG_REDO	6	/* do not do the log roll-forward
					in connection with recovery */

/** Types of threads existing in the system. */
enum srv_thread_type {
	SRV_COM = 1,	/**< threads serving communication and queries */
	SRV_CONSOLE,	/**< thread serving console */
	SRV_WORKER,	/**< threads serving parallelized queries and
			queries released from lock wait */
#if 0
	/* Utility threads */
	SRV_BUFFER,	/**< thread flushing dirty buffer blocks */
	SRV_RECOVERY,	/**< threads finishing a recovery */
	SRV_INSERT,	/**< thread flushing the insert buffer to disk */
#endif
	SRV_MASTER	/**< the master thread, (whose type number must
			be biggest) */
};

/*************************************************************************
Boots Innobase server. */
UNIV_INTERN
ulint
srv_boot(void);
/*==========*/
			/* out: DB_SUCCESS or error code */
/*************************************************************************
Initializes the server. */
UNIV_INTERN
void
srv_init(void);
/*==========*/
/*************************************************************************
Frees the OS fast mutex created in srv_boot(). */
UNIV_INTERN
void
srv_free(void);
/*==========*/
/*************************************************************************
Initializes the synchronization primitives, memory system, and the thread
local storage. */
UNIV_INTERN
void
srv_general_init(void);
/*==================*/
/*************************************************************************
Gets the number of threads in the system. */
UNIV_INTERN
ulint
srv_get_n_threads(void);
/*===================*/
/*************************************************************************
Returns the calling thread type. */

enum srv_thread_type
srv_get_thread_type(void);
/*=====================*/
			/* out: SRV_COM, ... */
/*************************************************************************
Sets the info describing an i/o thread current state. */
UNIV_INTERN
void
srv_set_io_thread_op_info(
/*======================*/
	ulint		i,	/* in: the 'segment' of the i/o thread */
	const char*	str);	/* in: constant char string describing the
				state */
/*************************************************************************
Releases threads of the type given from suspension in the thread table.
NOTE! The server mutex has to be reserved by the caller! */
UNIV_INTERN
ulint
srv_release_threads(
/*================*/
					/* out: number of threads
					released: this may be < n if
					not enough threads were
					suspended at the moment */
	enum srv_thread_type	type,	/* in: thread type */
	ulint			n);	/* in: number of threads to release */
/*************************************************************************
The master thread controlling the server. */
UNIV_INTERN
os_thread_ret_t
srv_master_thread(
/*==============*/
			/* out: a dummy parameter */
	void*	arg);	/* in: a dummy parameter required by
			os_thread_create */
/***********************************************************************
Tells the Innobase server that there has been activity in the database
and wakes up the master thread if it is suspended (not sleeping). Used
in the MySQL interface. Note that there is a small chance that the master
thread stays suspended (we do not protect our operation with the kernel
mutex, for performace reasons). */
UNIV_INTERN
void
srv_active_wake_master_thread(void);
/*===============================*/
/***********************************************************************
Wakes up the master thread if it is suspended or being suspended. */
UNIV_INTERN
void
srv_wake_master_thread(void);
/*========================*/
/*************************************************************************
Puts an OS thread to wait if there are too many concurrent threads
(>= srv_thread_concurrency) inside InnoDB. The threads wait in a FIFO queue. */
UNIV_INTERN
void
srv_conc_enter_innodb(
/*==================*/
	trx_t*	trx);	/* in: transaction object associated with the
			thread */
/*************************************************************************
This lets a thread enter InnoDB regardless of the number of threads inside
InnoDB. This must be called when a thread ends a lock wait. */
UNIV_INTERN
void
srv_conc_force_enter_innodb(
/*========================*/
	trx_t*	trx);	/* in: transaction object associated with the
			thread */
/*************************************************************************
This must be called when a thread exits InnoDB in a lock wait or at the
end of an SQL statement. */
UNIV_INTERN
void
srv_conc_force_exit_innodb(
/*=======================*/
	trx_t*	trx);	/* in: transaction object associated with the
			thread */
/*************************************************************************
This must be called when a thread exits InnoDB. */
UNIV_INTERN
void
srv_conc_exit_innodb(
/*=================*/
	trx_t*	trx);	/* in: transaction object associated with the
			thread */
/*******************************************************************
Puts a MySQL OS thread to wait for a lock to be released. If an error
occurs during the wait trx->error_state associated with thr is
!= DB_SUCCESS when we return. DB_LOCK_WAIT_TIMEOUT and DB_DEADLOCK
are possible errors. DB_DEADLOCK is returned if selective deadlock
resolution chose this transaction as a victim. */
UNIV_INTERN
void
srv_suspend_mysql_thread(
/*=====================*/
	que_thr_t*	thr);	/* in: query thread associated with the MySQL
				OS thread */
/************************************************************************
Releases a MySQL OS thread waiting for a lock to be released, if the
thread is already suspended. */
UNIV_INTERN
void
srv_release_mysql_thread_if_suspended(
/*==================================*/
	que_thr_t*	thr);	/* in: query thread associated with the
				MySQL OS thread	 */
/*************************************************************************
A thread which wakes up threads whose lock wait may have lasted too long.
This also prints the info output by various InnoDB monitors. */
UNIV_INTERN
os_thread_ret_t
srv_lock_timeout_and_monitor_thread(
/*================================*/
			/* out: a dummy parameter */
	void*	arg);	/* in: a dummy parameter required by
			os_thread_create */
/*************************************************************************
A thread which prints warnings about semaphore waits which have lasted
too long. These can be used to track bugs which cause hangs. */
UNIV_INTERN
os_thread_ret_t
srv_error_monitor_thread(
/*=====================*/
			/* out: a dummy parameter */
	void*	arg);	/* in: a dummy parameter required by
			os_thread_create */
/**********************************************************************
Outputs to a file the output of the InnoDB Monitor. */
UNIV_INTERN
void
srv_printf_innodb_monitor(
/*======================*/
	FILE*	file,		/* in: output stream */
	ulint*	trx_start,	/* out: file position of the start of
				the list of active transactions */
	ulint*	trx_end);	/* out: file position of the end of
				the list of active transactions */

/**********************************************************************
Function to pass InnoDB status variables to MySQL */
UNIV_INTERN
void
srv_export_innodb_status(void);
/*=====================*/

/* Thread slot in the thread table */
typedef struct srv_slot_struct	srv_slot_t;

/* Thread table is an array of slots */
typedef srv_slot_t	srv_table_t;

/* In this structure we store status variables to be passed to MySQL */
struct export_var_struct{
	ulint innodb_data_pending_reads;
	ulint innodb_data_pending_writes;
	ulint innodb_data_pending_fsyncs;
	ulint innodb_data_fsyncs;
	ulint innodb_data_read;
	ulint innodb_data_writes;
	ulint innodb_data_written;
	ulint innodb_data_reads;
	ulint innodb_dict_tables;
	ulint innodb_buffer_pool_pages_total;
	ulint innodb_buffer_pool_pages_data;
	ulint innodb_buffer_pool_pages_dirty;
	ulint innodb_buffer_pool_pages_misc;
	ulint innodb_buffer_pool_pages_free;
#ifdef UNIV_DEBUG
	ulint innodb_buffer_pool_pages_latched;
#endif /* UNIV_DEBUG */
	ulint innodb_buffer_pool_read_requests;
	ulint innodb_buffer_pool_reads;
	ulint innodb_buffer_pool_wait_free;
	ulint innodb_buffer_pool_pages_flushed;
	ulint innodb_buffer_pool_write_requests;
	ulint innodb_buffer_pool_read_ahead_seq;
	ulint innodb_buffer_pool_read_ahead_rnd;
	ulint innodb_dblwr_pages_written;
	ulint innodb_dblwr_writes;
	ibool innodb_have_atomic_builtins;
	ulint innodb_log_waits;
	ulint innodb_log_write_requests;
	ulint innodb_log_writes;
	ulint innodb_os_log_written;
	ulint innodb_os_log_fsyncs;
	ulint innodb_os_log_pending_writes;
	ulint innodb_os_log_pending_fsyncs;
	ulint innodb_page_size;
	ulint innodb_pages_created;
	ulint innodb_pages_read;
	ulint innodb_pages_written;
	ulint innodb_row_lock_waits;
	ulint innodb_row_lock_current_waits;
	ib_int64_t innodb_row_lock_time;
	ulint innodb_row_lock_time_avg;
	ulint innodb_row_lock_time_max;
	ulint innodb_rows_read;
	ulint innodb_rows_inserted;
	ulint innodb_rows_updated;
	ulint innodb_rows_deleted;
};

/* The server system struct */
struct srv_sys_struct{
	srv_table_t*	threads;	/* server thread table */
	UT_LIST_BASE_NODE_T(que_thr_t)
			tasks;		/* task queue */
	dict_index_t*	dummy_ind1;	/* dummy index for old-style
					supremum and infimum records */
	dict_index_t*	dummy_ind2;	/* dummy index for new-style
					supremum and infimum records */
};

extern ulint	srv_n_threads_active[];

#endif

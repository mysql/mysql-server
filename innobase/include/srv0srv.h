/******************************************************
The server main program

(c) 1995 Innobase Oy

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

/* Server parameters which are read from the initfile */

extern char*	srv_data_home;
#ifdef UNIV_LOG_ARCHIVE
extern char*	srv_arch_dir;
#endif /* UNIV_LOG_ARCHIVE */

extern ibool	srv_file_per_table;
extern ibool    srv_locks_unsafe_for_binlog;

extern ulint	srv_n_data_files;
extern char**	srv_data_file_names;
extern ulint*	srv_data_file_sizes;
extern ulint*   srv_data_file_is_raw_partition;

extern ibool	srv_auto_extend_last_data_file;
extern ulint	srv_last_file_size_max;
extern ulint	srv_auto_extend_increment;

extern ibool	srv_created_new_raw;

#define SRV_NEW_RAW    1
#define SRV_OLD_RAW    2

extern char**	srv_log_group_home_dirs;

extern ulint	srv_n_log_groups;
extern ulint	srv_n_log_files;
extern ulint	srv_log_file_size;
extern ulint	srv_log_buffer_size;
extern ulint	srv_flush_log_at_trx_commit;

extern byte	srv_latin1_ordering[256];/* The sort order table of the latin1
					character set */
extern ulint	srv_pool_size;
extern ulint	srv_awe_window_size;
extern ulint	srv_mem_pool_size;
extern ulint	srv_lock_table_size;

extern ulint	srv_n_file_io_threads;

#ifdef UNIV_LOG_ARCHIVE
extern ibool	srv_log_archive_on;
extern ibool	srv_archive_recovery;
extern dulint	srv_archive_recovery_limit_lsn;
#endif /* UNIV_LOG_ARCHIVE */

extern ulint	srv_lock_wait_timeout;

extern char*    srv_file_flush_method_str;
extern ulint    srv_unix_file_flush_method;
extern ulint   	srv_win_file_flush_method;

extern ulint	srv_max_n_open_files;

extern ulint	srv_max_dirty_pages_pct;

extern ulint	srv_force_recovery;
extern ulint	srv_thread_concurrency;

extern ulint	srv_max_n_threads;

extern lint	srv_conc_n_threads;

extern ibool	srv_fast_shutdown;
extern ibool	srv_very_fast_shutdown;  /* if this TRUE, do not flush the
					 buffer pool to data files at the
					 shutdown; we effectively 'crash'
					 InnoDB */
extern ibool	srv_innodb_status;

extern ibool	srv_use_doublewrite_buf;

extern ibool    srv_set_thread_priorities;
extern int      srv_query_thread_priority;

extern ibool	srv_use_awe;
extern ibool	srv_use_adaptive_hash_indexes;

extern ulint	srv_max_purge_lag;
/*-------------------------------------------*/

extern ulint	srv_n_rows_inserted;
extern ulint	srv_n_rows_updated;
extern ulint	srv_n_rows_deleted;
extern ulint	srv_n_rows_read;

extern ibool	srv_print_innodb_monitor;
extern ibool    srv_print_innodb_lock_monitor;
extern ibool    srv_print_innodb_tablespace_monitor;
extern ibool    srv_print_verbose_log;
extern ibool    srv_print_innodb_table_monitor;

extern ibool	srv_lock_timeout_and_monitor_active;
extern ibool	srv_error_monitor_active; 

extern ulint	srv_n_spin_wait_rounds;
extern ulint	srv_spin_wait_delay;
extern ibool	srv_priority_boost;
		
extern	ulint	srv_pool_size;
extern	ulint	srv_mem_pool_size;
extern	ulint	srv_lock_table_size;

extern	ulint	srv_sim_disk_wait_pct;
extern	ulint	srv_sim_disk_wait_len;
extern	ibool	srv_sim_disk_wait_by_yield;
extern	ibool	srv_sim_disk_wait_by_wait;

extern	ibool	srv_measure_contention;
extern	ibool	srv_measure_by_spin;
	
extern	ibool	srv_print_thread_releases;
extern	ibool	srv_print_lock_waits;
extern	ibool	srv_print_buf_io;
extern	ibool	srv_print_log_io;
extern	ibool	srv_print_parsed_sql;
extern	ibool	srv_print_latch_waits;

extern	ibool	srv_test_nocache;
extern	ibool	srv_test_cache_evict;

extern	ibool	srv_test_extra_mutexes;
extern	ibool	srv_test_sync;
extern	ulint	srv_test_n_threads;
extern	ulint	srv_test_n_loops;
extern	ulint	srv_test_n_free_rnds;
extern	ulint	srv_test_n_reserved_rnds;
extern	ulint	srv_test_n_mutexes;
extern	ulint	srv_test_array_size;

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

typedef struct srv_sys_struct	srv_sys_t;

/* The server system */
extern srv_sys_t*	srv_sys;

/* Alternatives for the file flush option in Unix; see the InnoDB manual about
what these mean */
#define SRV_UNIX_FDATASYNC   1	/* This is the default; it is currently mapped
				to a call of fsync() because fdatasync()
				seemed to corrupt files in Linux and Solaris */
#define SRV_UNIX_O_DSYNC     2
#define SRV_UNIX_LITTLESYNC  3
#define SRV_UNIX_NOSYNC      4
#define SRV_UNIX_O_DIRECT    5

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
#define SRV_FORCE_NO_BACKGROUND	2 	/* prevent the main thread from
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
					
/*************************************************************************
Boots Innobase server. */

ulint
srv_boot(void);
/*==========*/
			/* out: DB_SUCCESS or error code */
/*************************************************************************
Frees the OS fast mutex created in srv_boot(). */

void
srv_free(void);
/*==========*/
/*************************************************************************
Initializes the synchronization primitives, memory system, and the thread
local storage. */

void
srv_general_init(void);
/*==================*/
/*************************************************************************
Gets the number of threads in the system. */

ulint
srv_get_n_threads(void);
/*===================*/
/*************************************************************************
Returns the calling thread type. */

ulint
srv_get_thread_type(void);
/*=====================*/
			/* out: SRV_COM, ... */
/*************************************************************************
Sets the info describing an i/o thread current state. */

void
srv_set_io_thread_op_info(
/*======================*/
	ulint		i,	/* in: the 'segment' of the i/o thread */
	const char*	str);	/* in: constant char string describing the
				state */
/*************************************************************************
Releases threads of the type given from suspension in the thread table.
NOTE! The server mutex has to be reserved by the caller! */

ulint
srv_release_threads(
/*================*/
			/* out: number of threads released: this may be
			< n if not enough threads were suspended at the
			moment */
	ulint	type,	/* in: thread type */
	ulint	n);	/* in: number of threads to release */
/*************************************************************************
The master thread controlling the server. */

#ifndef __WIN__
void*
#else
ulint
#endif
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

void
srv_active_wake_master_thread(void);
/*===============================*/
/***********************************************************************
Wakes up the master thread if it is suspended or being suspended. */

void
srv_wake_master_thread(void);
/*========================*/
/*************************************************************************
Puts an OS thread to wait if there are too many concurrent threads
(>= srv_thread_concurrency) inside InnoDB. The threads wait in a FIFO queue. */

void
srv_conc_enter_innodb(
/*==================*/
	trx_t*	trx);	/* in: transaction object associated with the
			thread */
/*************************************************************************
This lets a thread enter InnoDB regardless of the number of threads inside
InnoDB. This must be called when a thread ends a lock wait. */

void
srv_conc_force_enter_innodb(
/*========================*/
	trx_t*	trx);	/* in: transaction object associated with the
			thread */
/*************************************************************************
This must be called when a thread exits InnoDB in a lock wait or at the
end of an SQL statement. */

void
srv_conc_force_exit_innodb(
/*=======================*/
	trx_t*	trx);	/* in: transaction object associated with the
			thread */
/*************************************************************************
This must be called when a thread exits InnoDB. */

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

void
srv_suspend_mysql_thread(
/*=====================*/
	que_thr_t*	thr);	/* in: query thread associated with the MySQL
				OS thread */
/************************************************************************
Releases a MySQL OS thread waiting for a lock to be released, if the
thread is already suspended. */

void
srv_release_mysql_thread_if_suspended(
/*==================================*/
	que_thr_t*	thr);	/* in: query thread associated with the
				MySQL OS thread  */
/*************************************************************************
A thread which wakes up threads whose lock wait may have lasted too long.
This also prints the info output by various InnoDB monitors. */

#ifndef __WIN__
void*
#else
ulint
#endif
srv_lock_timeout_and_monitor_thread(
/*================================*/
			/* out: a dummy parameter */
	void*	arg);	/* in: a dummy parameter required by
			os_thread_create */
/*************************************************************************
A thread which prints warnings about semaphore waits which have lasted
too long. These can be used to track bugs which cause hangs. */

#ifndef __WIN__
void*
#else
ulint
#endif
srv_error_monitor_thread(
/*=====================*/
			/* out: a dummy parameter */
	void*	arg);	/* in: a dummy parameter required by
			os_thread_create */
/**********************************************************************
Outputs to a file the output of the InnoDB Monitor. */

void
srv_printf_innodb_monitor(
/*======================*/
	FILE*	file);	/* in: output stream */


/* Types for the threads existing in the system. Threads of types 4 - 9
are called utility threads. Note that utility threads are mainly disk
bound, except that version threads 6 - 7 may also be CPU bound, if
cleaning versions from the buffer pool. */

#define	SRV_COM		1	/* threads serving communication and queries */
#define	SRV_CONSOLE	2	/* thread serving console */
#define	SRV_WORKER	3	/* threads serving parallelized queries and
				queries released from lock wait */
#define SRV_BUFFER	4	/* thread flushing dirty buffer blocks,
				not currently in use */
#define SRV_RECOVERY	5	/* threads finishing a recovery,
				not currently in use */
#define SRV_INSERT	6	/* thread flushing the insert buffer to disk,
				not currently in use */
#define SRV_MASTER	7      	/* the master thread, (whose type number must
				be biggest) */

/* Thread slot in the thread table */
typedef struct srv_slot_struct	srv_slot_t;

/* Thread table is an array of slots */
typedef srv_slot_t	srv_table_t;

/* The server system struct */
struct srv_sys_struct{
	os_event_t	operational;	/* created threads must wait for the
					server to become operational by
					waiting for this event */
	srv_table_t*	threads;	/* server thread table */
	UT_LIST_BASE_NODE_T(que_thr_t)
			tasks;		/* task queue */
};

extern ulint	srv_n_threads_active[];

#endif


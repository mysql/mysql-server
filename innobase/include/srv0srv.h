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
#include "com0com.h"
#include "que0types.h"

/* Server parameters which are read from the initfile */

extern char*	srv_data_home;
extern char*	srv_logs_home;
extern char*	srv_arch_dir;

extern ulint	srv_n_data_files;
extern char**	srv_data_file_names;
extern ulint*	srv_data_file_sizes;

extern char**	srv_log_group_home_dirs;

extern ulint	srv_n_log_groups;
extern ulint	srv_n_log_files;
extern ulint	srv_log_file_size;
extern ibool	srv_log_archive_on;
extern ulint	srv_log_buffer_size;
extern ibool	srv_flush_log_at_trx_commit;

extern ibool	srv_use_native_aio;		

extern ulint	srv_pool_size;
extern ulint	srv_mem_pool_size;
extern ulint	srv_lock_table_size;

extern ulint	srv_n_file_io_threads;

extern ibool	srv_archive_recovery;
extern dulint	srv_archive_recovery_limit_lsn;

extern ulint	srv_lock_wait_timeout;

extern ibool    srv_set_thread_priorities;
extern int      srv_query_thread_priority;

/*-------------------------------------------*/
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

extern mutex_t*	kernel_mutex_temp;/* mutex protecting the server, trx structs,
				query threads, and lock table: we allocate
				it from dynamic memory to get it to the
				same DRAM page as other hotspot semaphores */
#define kernel_mutex (*kernel_mutex_temp)
				
typedef struct srv_sys_struct	srv_sys_t;

/* The server system */
extern srv_sys_t*	srv_sys;

/*************************************************************************
Boots Innobase server. */

ulint
srv_boot(void);
/*==========*/
			/* out: DB_SUCCESS or error code */
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
/*************************************************************************
Reads a keyword and a value from a file. */

ulint
srv_read_init_val(
/*==============*/
				/* out: DB_SUCCESS or error code */
	FILE*	initfile,	/* in: file pointer */
	char*	keyword,	/* in: keyword before value(s), or NULL if
				no keyword read */
	char*	str_buf,	/* in/out: buffer for a string value to read,
				buffer size must be 10000 bytes, if NULL
				then not read */
	ulint*	num_val,	/* out:	numerical value to read, if NULL
				then not read */
	ibool	print_not_err);	/* in: if TRUE, then we will not print
				error messages to console */
/***********************************************************************
Tells the Innobase server that there has been activity in the database
and wakes up the master thread if it is suspended (not sleeping). Used
in the MySQL interface. Note that there is a small chance that the master
thread stays suspended (we do not protect our operation with the kernel
mutex, for performace reasons). */

void
srv_active_wake_master_thread(void);
/*===============================*/
/*******************************************************************
Puts a MySQL OS thread to wait for a lock to be released. */

ibool
srv_suspend_mysql_thread(
/*=====================*/
				/* out: TRUE if the lock wait timeout was
				exceeded */
	que_thr_t*	thr);	/* in: query thread associated with
				the MySQL OS thread */
/************************************************************************
Releases a MySQL OS thread waiting for a lock to be released, if the
thread is already suspended. */

void
srv_release_mysql_thread_if_suspended(
/*==================================*/
	que_thr_t*	thr);	/* in: query thread associated with the
				MySQL OS thread  */
/*************************************************************************
A thread which wakes up threads whose lock wait may have lasted too long. */

#ifndef __WIN__
void*
#else
ulint
#endif
srv_lock_timeout_monitor_thread(
/*============================*/
			/* out: a dummy parameter */
	void*	arg);	/* in: a dummy parameter required by
			os_thread_create */


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
	com_endpoint_t*	endpoint;	/* the communication endpoint of the
					server */

	srv_table_t*	threads;	/* server thread table */
	UT_LIST_BASE_NODE_T(que_thr_t)
			tasks;		/* task queue */
};

extern ulint	srv_n_threads_active[];

#endif

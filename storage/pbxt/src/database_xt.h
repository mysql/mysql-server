/* Copyright (c) 2005 PrimeBase Technologies GmbH
 *
 * PrimeBase XT
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 * 2005-01-15	Paul McCullagh
 *
 * H&G2JCtL
 */
#ifndef __xt_database_h__
#define __xt_database_h__

#include <time.h>

#include "thread_xt.h"
#include "hashtab_xt.h"
#include "table_xt.h"
#include "sortedlist_xt.h"
#include "xaction_xt.h"
#include "heap_xt.h"
#include "xactlog_xt.h"
#include "restart_xt.h"
#include "index_xt.h"

#ifdef DEBUG
//#define XT_USE_XACTION_DEBUG_SIZES
#endif

#ifdef XT_USE_XACTION_DEBUG_SIZES
#define XT_DB_TABLE_POOL_SIZE	2
#else
#define XT_DB_TABLE_POOL_SIZE	10		// The number of open tables maintained by the sweeper
#endif

/* Turn this switch on to enable spin lock based wait-for logic: */
#define XT_USE_SPINLOCK_WAIT_FOR

extern xtLogOffset		xt_db_log_file_threshold;
extern size_t			xt_db_log_buffer_size;
extern size_t			xt_db_transaction_buffer_size;
extern size_t			xt_db_checkpoint_frequency;
extern off_t			xt_db_data_log_threshold;
extern size_t			xt_db_data_file_grow_size;
extern size_t			xt_db_row_file_grow_size;
extern int				xt_db_garbage_threshold;
extern int				xt_db_log_file_count;
extern int				xt_db_auto_increment_mode;
extern int				xt_db_offline_log_function;
extern int				xt_db_sweeper_priority;
extern int				xt_db_flush_log_at_trx_commit;

extern XTSortedListPtr	xt_db_open_db_by_id;
extern XTHashTabPtr		xt_db_open_databases;
extern time_t			xt_db_approximate_time;

#define XT_OPEN_TABLE_POOL_HASH_SIZE	223

#define XT_SW_WORK_NORMAL				0
#define XT_SW_NO_MORE_XACT_SLOTS		1
#define XT_SW_DIRTY_RECORD_FOUND		2
#define XT_SW_TOO_FAR_BEHIND			3							/* The sweeper is getting too far behind, although it is working! */

typedef struct XTOpenTablePool {
	struct XTDatabase		*opt_db;
	xtTableID				opt_tab_id;								/* The table ID. */
	u_int					opt_total_open;							/* Total number of open tables. */
	xtBool					opt_locked;								/* This table is locked open tables are freed on return to pool. */
	u_int					opt_flushing;
	XTOpenTablePtr			opt_free_list;							/* A list of free, unused open tables. */
	struct XTOpenTablePool	*opt_next_hash;
} XTOpenTablePoolRec, *XTOpenTablePoolPtr;

typedef struct XTAllTablePools {
	xt_mutex_type			opt_lock;								/* This lock protects the open table pool. */
	xt_cond_type			opt_cond;								/* Used to wait for an exclusive lock on a table. */

	u_int					otp_total_free;							/* This is the total number of free open tables (not in use): */

	/* All free (unused tables) are on this list: */
	XTOpenTablePtr			otp_mr_used;
	XTOpenTablePtr			otp_lr_used;
	time_t					otp_free_time;							/* The free time of the LRU open table. */
	
	XTOpenTablePoolPtr		otp_hash[XT_OPEN_TABLE_POOL_HASH_SIZE];
} XTAllTablePoolsRec, *XTAllTablePoolsPtr;

typedef struct XTTablePath {
	u_int					tp_tab_count;							/* The number of tables using this path. */
	char					tp_path[XT_VAR_LENGTH];					/* The table path. */
} XTTablePathRec, *XTTablePathPtr;

#define XT_THREAD_BUSY		0
#define XT_THREAD_IDLE		1
#define XT_THREAD_INERR		2

#define XT_XA_HASH_TAB_SIZE	223

typedef struct XTDatabase : public XTHeap {
	char					*db_name;								/* The name of the database, last component of the path! */
	char					*db_main_path;
	xtDatabaseID			db_id;
	xtTableID				db_curr_tab_id;							/* The ID of the last table created. */
	XTHashTabPtr			db_tables;
	XTSortedListPtr			db_table_by_id;
	XTSortedListPtr			db_table_paths;							/* A list of table paths used by this database. */
	xtBool					db_multi_path;
	XTSortedListPtr			db_error_list;							/* A list of errors already reported. */

	/* The open table pool: */
	XTAllTablePoolsRec		db_ot_pool;

	/* Transaction related stuff: */
	XTSpinLockRec			db_xn_id_lock;							/* Lock for next transaction ID. */
	xtXactID				db_xn_curr_id;							/* The ID of the last transaction started. */
	xtXactID				db_xn_min_ram_id;						/* The lowest ID of the transactions in memory (RAM). */
	xtXactID				db_xn_to_clean_id;						/* The next transaction to be cleaned (>= db_xn_min_ram_id). */
	xtXactID				db_xn_min_run_id;						/* The lowest ID of all running transactions (not up-to-date! >= db_xn_to_clean_id) */
	xtWord4					db_xn_end_time;							/* The time of the transaction end. */
	XTXactSegRec			db_xn_idx[XT_XN_NO_OF_SEGMENTS];		/* Index of transactions in RAM. */
	xtWord1					*db_xn_data;							/* Start of the block allocated to contain transaction data. */
	xtWord1					*db_xn_data_end;						/* End of the transaction data block. */
	u_int					db_stat_sweep_waits;					/* STATISTICS: count the sweeper waits. */
	XTDatabaseLogRec		db_xlog;								/* The transaction log for this database. */
	XTXactRestartRec		db_restart;								/* Database recovery stuff. */
	xt_mutex_type			db_xn_xa_lock;
	XTXactPreparePtr		db_xn_xa_table[XT_XA_HASH_TAB_SIZE];
	XTSortedListPtr			db_xn_xa_list;							/* The "wait-for" list, of transactions waiting for other transactions. */

	XTSortedListPtr			db_xn_wait_for;							/* The "wait-for" list, of transactions waiting for other transactions. */
	u_int					db_xn_call_start;						/* Start of the post wait calls. */
	XTSpinLockRec			db_xn_wait_spinlock;
	//xt_mutex_type			db_xn_wait_lock;						/* The lock associated with the wait for list. */
	//xt_cond_type			db_xn_wait_cond;						/* This condition is signalled when a transaction quits. */
	//u_int					db_xn_wait_on_cond;						/* Number of threads waiting on the condition. */
	int						db_xn_wait_count;						/* Number of waiting transactions. */
	u_int					db_xn_total_writer_count;				/* The total number of writers. */
	int						db_xn_writer_count;						/* The number of writer threads. */
	int						db_xn_writer_wait_count;				/* The number of writer threads waiting. */
	int						db_xn_long_running_count;				/* The number of long running writer threads. */

	/* Sweeper stuff: */
	struct XTThread			*db_sw_thread;							/* The sweeper thread (cleans up transactions). */
	xt_mutex_type			db_sw_lock;								/* The lock associated with the sweeper. */
	xt_cond_type			db_sw_cond;								/* The sweeper wakeup condition. */
	u_int					db_sw_check_count;
	int						db_sw_idle;								/* BUSY/IDLE/INERR depending on the state of the sweeper. */
	int						db_sw_faster;							/* non-zero if the sweeper should work faster. */
	xtBool					db_sw_fast;								/* TRUE if the sweeper is working faster. */

	/* Writer stuff: */
	struct XTThread			*db_wr_thread;							/* The writer thread (write log data to the database). */
	int						db_wr_idle;								/* BUSY/IDLE/INERR depending on the state of the writer. */
	xtBool					db_wr_faster;							/* Set to TRUE if the writer should work faster. */
	xtBool					db_wr_fast;								/* TRUE if the writer is working faster. */
	u_int					db_wr_thread_waiting;					/* Count the number of threads waiting for the writer. */
	xtBool					db_wr_freeer_waiting;					/* TRUE if the freeer is wating for the writer. */
	xt_mutex_type			db_wr_lock;
	xt_cond_type			db_wr_cond;								/* Writer condition when idle (must bw woken by log flush! */
	xtLogID					db_wr_log_id;							/* Current write log ID. */
	xtLogOffset				db_wr_log_offset;						/* Current write log offset. */
	xtLogID					db_wr_flush_point_log_id;				/* This is the point to which the writer will write (log ID). */
	xtLogOffset				db_wr_flush_point_log_offset;			/* This is the point to which the writer will write (log offset). */

	/* Data log stuff: */
	XTDataLogCacheRec		db_datalogs;							/* The database data log stuff. */
	XTIndexLogPoolRec		db_indlogs;								/* Index logs used for consistent write. */

	/* Compactor stuff: */
	struct XTThread			*db_co_thread;							/* The compator thread (compacts data logs). */
	xt_mutex_type			db_co_ext_lock;							/* Required when extended data is moved, or removed. */
	xtBool					db_co_busy;								/* True of the compactor is busy compacting a data log. */
	xt_mutex_type			db_co_dlog_lock;						/* This is the lock required to flusht the compactors data log. */

	/* Checkpointer stuff: */
	struct XTThread			*db_cp_thread;							/* The checkpoint thread (flushes the database data). */
	xt_mutex_type			db_cp_lock;
	xt_cond_type			db_cp_cond;								/* Writer condition when idle (must bw woken by log flush! */
	XTCheckPointStateRec	db_cp_state;							/* The checkpoint state. */

	/* The "flusher" thread (used when pbxt_flush_log_at_trx_commit = 0 or 2) */
	struct XTThread			*db_fl_thread;							/* The flusher thread (flushes the transation log). */
	xt_mutex_type			db_fl_lock;
} XTDatabaseRec, *XTDatabaseHPtr;		/* Heap pointer */

#define XT_FOR_USER					0
#define XT_FOR_COMPACTOR			1
#define XT_FOR_SWEEPER				2
#define XT_FOR_WRITER				3
#define XT_FOR_CHECKPOINTER			4

void				xt_create_database(XTThreadPtr th, char *path);
XTDatabaseHPtr		xt_get_database(XTThreadPtr self, char *path, xtBool multi_path);
XTDatabaseHPtr		xt_get_database_by_id(XTThreadPtr self, xtDatabaseID db_id);
void				xt_drop_database(XTThreadPtr self, XTDatabaseHPtr db);
void				xt_check_database(XTThreadPtr self);

void				xt_add_pbxt_file(size_t size, char *path, const char *file);
void				xt_add_location_file(size_t size, char *path);
void				xt_add_pbxt_dir(size_t size, char *path);
void				xt_add_system_dir(size_t size, char *path);
void				xt_add_data_dir(size_t size, char *path);

void				xt_use_database(XTThreadPtr self, XTDatabaseHPtr db, int what_for);
void				xt_unuse_database(XTThreadPtr self, XTThreadPtr other_thr);
void				xt_open_database(XTThreadPtr self, char *path, xtBool multi_path);

void				xt_lock_installation(XTThreadPtr self, char *installation_path);
void				xt_unlock_installation(XTThreadPtr self, char *installation_path);
void				xt_crash_me(void);

void				xt_init_databases(XTThreadPtr self);
void				xt_stop_database_threads(XTThreadPtr self, xtBool sync);
void				xt_exit_databases(XTThreadPtr self);

void				xt_dump_database(XTThreadPtr self, XTDatabaseHPtr db);

void				xt_db_init_thread(XTThreadPtr self, XTThreadPtr new_thread);
void				xt_db_exit_thread(XTThreadPtr self);

void				xt_db_pool_init(XTThreadPtr self, struct XTDatabase *db);
void				xt_db_pool_exit(XTThreadPtr self, struct XTDatabase *db);
XTOpenTablePoolPtr	xt_db_lock_table_pool_by_name(XTThreadPtr self, XTDatabaseHPtr db, XTPathStrPtr name, xtBool no_load, xtBool flush_table, xtBool missing_ok, xtBool wait_for_open, XTTableHPtr *ret_tab);
void				xt_db_wait_for_open_tables(XTThreadPtr self, XTOpenTablePoolPtr table_pool);
void				xt_db_unlock_table_pool(struct XTThread *self, XTOpenTablePoolPtr table_pool);
XTOpenTablePtr		xt_db_open_pool_table(XTThreadPtr self, XTDatabaseHPtr db, xtTableID tab_id, int *result, xtBool i_am_background);
XTOpenTablePtr		xt_db_open_table_using_tab(XTTableHPtr tab, XTThreadPtr thread);
xtBool				xt_db_open_pool_table_ns(XTOpenTablePtr *ret_ot, XTDatabaseHPtr db, xtTableID tab_id);
void				xt_db_return_table_to_pool(XTThreadPtr self, XTOpenTablePtr ot);
void				xt_db_return_table_to_pool_ns(XTOpenTablePtr ot);
void				xt_db_free_unused_open_tables(XTThreadPtr self, XTDatabaseHPtr db);

#define XT_LONG_RUNNING_TIME	2

inline void xt_xlog_check_long_writer(XTThreadPtr thread)
{
	if (thread->st_xact_writer) {
		if (xt_db_approximate_time - thread->st_xact_write_time > XT_LONG_RUNNING_TIME) {
			if (!thread->st_xact_long_running) {
				thread->st_xact_long_running = TRUE;
				thread->st_database->db_xn_long_running_count++;
			}
		}
	}
}

extern XTDatabaseHPtr	pbxt_database;				// The global open database

#endif

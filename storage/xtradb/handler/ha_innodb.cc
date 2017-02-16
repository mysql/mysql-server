/*****************************************************************************

Copyright (c) 2000, 2015, Oracle and/or its affiliates. All Rights Reserved.
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

/* TODO list for the InnoDB handler in 5.0:
  - fix savepoint functions to use savepoint storage area
  - Find out what kind of problems the OS X case-insensitivity causes to
    table and database names; should we 'normalize' the names like we do
    in Windows?
*/

#ifdef USE_PRAGMA_IMPLEMENTATION
#pragma implementation				// gcc: Class implementation
#endif

#define MYSQL_SERVER 1

#include <sql_table.h>	// explain_filename, nz2, EXPLAIN_PARTITIONS_AS_COMMENT,
			// EXPLAIN_FILENAME_MAX_EXTRA_LENGTH

#include <sql_acl.h>	// PROCESS_ACL
#include <m_ctype.h>
#include <debug_sync.h> // DEBUG_SYNC
#include <mysys_err.h>
#include <mysql/plugin.h>
#include <innodb_priv.h>
#include <mysql/psi/psi.h>
#include <my_sys.h>
#include <my_check_opt.h>

#ifdef MYSQL_SERVER
#include <rpl_mi.h>
#include <slave.h>
#include <log_event.h> // rpl_get_position_info
#endif /* MYSQL_SERVER */

#ifdef _WIN32
#include <io.h>
#endif
/** @file ha_innodb.cc */

/* Include necessary InnoDB headers */
extern "C" {
#include "univ.i"
#include "buf0lru.h"
#include "btr0sea.h"
#include "os0file.h"
#include "os0thread.h"
#include "srv0start.h"
#include "srv0srv.h"
#include "trx0roll.h"
#include "trx0trx.h"
#include "trx0sys.h"
#include "mtr0mtr.h"
#include "row0ins.h"
#include "row0mysql.h"
#include "row0sel.h"
#include "row0upd.h"
#include "log0log.h"
#include "log0online.h"
#include "lock0lock.h"
#include "dict0crea.h"
#include "btr0cur.h"
#include "btr0btr.h"
#include "fsp0fsp.h"
#include "sync0sync.h"
#include "fil0fil.h"
#include "trx0xa.h"
#include "row0merge.h"
#include "dict0boot.h"
#include "ha_prototypes.h"
#include "ut0mem.h"
#include "ibuf0ibuf.h"

enum_tx_isolation thd_get_trx_isolation(const THD* thd);
}

#include "ha_innodb.h"
#include "i_s.h"

#ifdef MYSQL_SERVER
// Defined in trx0sys.c
extern char		trx_sys_mysql_master_log_name[];
extern ib_int64_t	trx_sys_mysql_master_log_pos;
extern char		trx_sys_mysql_relay_log_name[];
extern ib_int64_t	trx_sys_mysql_relay_log_pos;
#endif /* MYSQL_SERVER */

# ifndef MYSQL_PLUGIN_IMPORT
#  define MYSQL_PLUGIN_IMPORT /* nothing */
# endif /* MYSQL_PLUGIN_IMPORT */

/** to protect innobase_open_files */
static mysql_mutex_t innobase_share_mutex;
static ulong commit_threads = 0;
static mysql_cond_t commit_cond;
static mysql_mutex_t commit_cond_m;
static bool innodb_inited = 0;

#define INSIDE_HA_INNOBASE_CC

/* In the Windows plugin, the return value of current_thd is
undefined.  Map it to NULL. */

#define EQ_CURRENT_THD(thd) ((thd) == current_thd)


static struct handlerton* innodb_hton_ptr;

static const long AUTOINC_OLD_STYLE_LOCKING = 0;
static const long AUTOINC_NEW_STYLE_LOCKING = 1;
static const long AUTOINC_NO_LOCKING = 2;

static long innobase_mirrored_log_groups, innobase_log_files_in_group,
	innobase_log_buffer_size,
	innobase_additional_mem_pool_size, innobase_file_io_threads,
	innobase_force_recovery, innobase_open_files,
	innobase_autoinc_lock_mode;
static ulong innobase_commit_concurrency = 0;
static ulong innobase_read_io_threads;
static ulong innobase_write_io_threads;
static long innobase_buffer_pool_instances = 1;

static ulong innobase_page_size;
static ulong innobase_log_block_size;

static my_bool innobase_thread_concurrency_timer_based;
static long long innobase_buffer_pool_size, innobase_log_file_size;

/** Percentage of the buffer pool to reserve for 'old' blocks.
Connected to buf_LRU_old_ratio. */
static uint innobase_old_blocks_pct;

/* The default values for the following char* start-up parameters
are determined in innobase_init below: */

static char*	innobase_data_home_dir			= NULL;
static char*	innobase_data_file_path			= NULL;
static char*	innobase_log_group_home_dir		= NULL;
static char*	innobase_file_format_name		= NULL;
static char*	innobase_change_buffering		= NULL;
static char*	innobase_doublewrite_file		= NULL;

/* The highest file format being used in the database. The value can be
set by user, however, it will be adjusted to the newer file format if
a table of such format is created/opened. */
static char*	innobase_file_format_max		= NULL;

static char*	innobase_file_flush_method		= NULL;

/* Below we have boolean-valued start-up parameters, and their default
values */

static ulong	innobase_fast_shutdown			= 1;
static my_bool	innobase_file_format_check		= TRUE;
#ifdef UNIV_LOG_ARCHIVE
static my_bool	innobase_log_archive			= FALSE;
static char*	innobase_log_arch_dir			= NULL;
#endif /* UNIV_LOG_ARCHIVE */
static my_bool	innobase_use_atomic_writes		= FALSE;
static my_bool	innobase_use_fallocate			= TRUE;
static my_bool	innobase_use_doublewrite		= TRUE;
static my_bool	innobase_use_checksums			= TRUE;
static my_bool	innobase_fast_checksum			= FALSE;
static my_bool	innobase_recovery_stats			= TRUE;
static my_bool	innobase_locks_unsafe_for_binlog	= FALSE;
static my_bool	innobase_overwrite_relay_log_info	= FALSE;
static my_bool	innobase_rollback_on_timeout		= FALSE;
static my_bool	innobase_create_status_file		= FALSE;
static my_bool	innobase_stats_on_metadata		= TRUE;
static my_bool	innobase_large_prefix			= FALSE;
static my_bool	innobase_use_sys_stats_table		= FALSE;
#ifdef UNIV_DEBUG
static ulong    innobase_sys_stats_root_page		= 0;
#endif
static my_bool	innobase_buffer_pool_shm_checksum	= TRUE;
static uint	innobase_buffer_pool_shm_key		= 0;
static ulint	srv_lazy_drop_table			= 0;


static char*	internal_innobase_data_file_path	= NULL;

static char*	innodb_version_str = (char*) INNODB_VERSION_STR;

static my_bool	innobase_blocking_lru_restore		= FALSE;

/** Possible values for system variable "innodb_stats_method". The values
are defined the same as its corresponding MyISAM system variable
"myisam_stats_method"(see "myisam_stats_method_names"), for better usability */
static const char* innodb_stats_method_names[] = {
	"nulls_equal",
	"nulls_unequal",
	"nulls_ignored",
	NullS
};

/** Used to define an enumerate type of the system variable innodb_stats_method.
This is the same as "myisam_stats_method_typelib" */
static TYPELIB innodb_stats_method_typelib = {
	array_elements(innodb_stats_method_names) - 1,
	"innodb_stats_method_typelib",
	innodb_stats_method_names,
	NULL
};

/* The following counter is used to convey information to InnoDB
about server activity: in selects it is not sensible to call
srv_active_wake_master_thread after each fetch or search, we only do
it every INNOBASE_WAKE_INTERVAL'th step. */

#define INNOBASE_WAKE_INTERVAL	32
static ulong	innobase_active_counter	= 0;

static hash_table_t*	innobase_open_tables;

/** Allowed values of innodb_change_buffering */
static const char* innobase_change_buffering_values[IBUF_USE_COUNT] = {
	"none",		/* IBUF_USE_NONE */
	"inserts",	/* IBUF_USE_INSERT */
	"deletes",	/* IBUF_USE_DELETE_MARK */
	"changes",	/* IBUF_USE_INSERT_DELETE_MARK */
	"purges",	/* IBUF_USE_DELETE */
	"all"		/* IBUF_USE_ALL */
};

#ifdef HAVE_PSI_INTERFACE
/* Keys to register pthread mutexes/cond in the current file with
performance schema */
static mysql_pfs_key_t	innobase_share_mutex_key;
static mysql_pfs_key_t	commit_cond_mutex_key;
static mysql_pfs_key_t	commit_cond_key;

static PSI_mutex_info	all_pthread_mutexes[] = {
        {&commit_cond_mutex_key, "commit_cond_mutex", 0},
        {&innobase_share_mutex_key, "innobase_share_mutex", 0}
};

static PSI_cond_info	all_innodb_conds[] = {
	{&commit_cond_key, "commit_cond", 0}
};

# ifdef UNIV_PFS_MUTEX
/* all_innodb_mutexes array contains mutexes that are
performance schema instrumented if "UNIV_PFS_MUTEX"
is defined */
static PSI_mutex_info all_innodb_mutexes[] = {
	{&autoinc_mutex_key, "autoinc_mutex", 0},
	{&btr_search_enabled_mutex_key, "btr_search_enabled_mutex", 0},
#  ifndef PFS_SKIP_BUFFER_MUTEX_RWLOCK
	{&buffer_block_mutex_key, "buffer_block_mutex", 0},
#  endif /* !PFS_SKIP_BUFFER_MUTEX_RWLOCK */
	{&buf_pool_mutex_key, "buf_pool_mutex", 0},
	{&buf_pool_zip_mutex_key, "buf_pool_zip_mutex", 0},
	{&buf_pool_LRU_list_mutex_key, "buf_pool_LRU_list_mutex", 0},
	{&buf_pool_free_list_mutex_key, "buf_pool_free_list_mutex", 0},
	{&buf_pool_zip_free_mutex_key, "buf_pool_zip_free_mutex", 0},
	{&buf_pool_zip_hash_mutex_key, "buf_pool_zip_hash_mutex", 0},
	{&cache_last_read_mutex_key, "cache_last_read_mutex", 0},
	{&dict_foreign_err_mutex_key, "dict_foreign_err_mutex", 0},
	{&dict_sys_mutex_key, "dict_sys_mutex", 0},
	{&file_format_max_mutex_key, "file_format_max_mutex", 0},
	{&fil_system_mutex_key, "fil_system_mutex", 0},
	{&flush_list_mutex_key, "flush_list_mutex", 0},
	{&log_flush_order_mutex_key, "log_flush_order_mutex", 0},
	{&hash_table_mutex_key, "hash_table_mutex", 0},
	{&ibuf_bitmap_mutex_key, "ibuf_bitmap_mutex", 0},
	{&ibuf_mutex_key, "ibuf_mutex", 0},
	{&ibuf_pessimistic_insert_mutex_key,
		 "ibuf_pessimistic_insert_mutex", 0},
	{&kernel_mutex_key, "kernel_mutex", 0},
	{&log_bmp_sys_mutex_key, "log_bmp_sys_mutex", 0},
	{&log_sys_mutex_key, "log_sys_mutex", 0},
#  ifdef UNIV_MEM_DEBUG
	{&mem_hash_mutex_key, "mem_hash_mutex", 0},
#  endif /* UNIV_MEM_DEBUG */
	{&mem_pool_mutex_key, "mem_pool_mutex", 0},
	{&mutex_list_mutex_key, "mutex_list_mutex", 0},
	{&purge_sys_bh_mutex_key, "purge_sys_bh_mutex", 0},
	{&recv_sys_mutex_key, "recv_sys_mutex", 0},
	{&rseg_mutex_key, "rseg_mutex", 0},
#  ifdef UNIV_SYNC_DEBUG
	{&rw_lock_debug_mutex_key, "rw_lock_debug_mutex", 0},
#  endif /* UNIV_SYNC_DEBUG */
	{&rw_lock_list_mutex_key, "rw_lock_list_mutex", 0},
	{&rw_lock_mutex_key, "rw_lock_mutex", 0},
	{&srv_dict_tmpfile_mutex_key, "srv_dict_tmpfile_mutex", 0},
	{&srv_innodb_monitor_mutex_key, "srv_innodb_monitor_mutex", 0},
	{&srv_misc_tmpfile_mutex_key, "srv_misc_tmpfile_mutex", 0},
	{&srv_monitor_file_mutex_key, "srv_monitor_file_mutex", 0},
	{&syn_arr_mutex_key, "syn_arr_mutex", 0},
#  ifdef UNIV_SYNC_DEBUG
	{&sync_thread_mutex_key, "sync_thread_mutex", 0},
#  endif /* UNIV_SYNC_DEBUG */
	{&trx_doublewrite_mutex_key, "trx_doublewrite_mutex", 0},
	{&trx_undo_mutex_key, "trx_undo_mutex", 0}
};
# endif /* UNIV_PFS_MUTEX */

# ifdef UNIV_PFS_RWLOCK
/* all_innodb_rwlocks array contains rwlocks that are
performance schema instrumented if "UNIV_PFS_RWLOCK"
is defined */
static PSI_rwlock_info all_innodb_rwlocks[] = {
#  ifdef UNIV_LOG_ARCHIVE
	{&archive_lock_key, "archive_lock", 0},
#  endif /* UNIV_LOG_ARCHIVE */
	{&btr_search_latch_key, "btr_search_latch", 0},
	{&buf_pool_page_hash_key, "buf_pool_page_hash_latch", 0},
#  ifndef PFS_SKIP_BUFFER_MUTEX_RWLOCK
	{&buf_block_lock_key, "buf_block_lock", 0},
#  endif /* !PFS_SKIP_BUFFER_MUTEX_RWLOCK */
#  ifdef UNIV_SYNC_DEBUG
	{&buf_block_debug_latch_key, "buf_block_debug_latch", 0},
#  endif /* UNIV_SYNC_DEBUG */
	{&dict_operation_lock_key, "dict_operation_lock", 0},
	{&fil_space_latch_key, "fil_space_latch", 0},
	{&checkpoint_lock_key, "checkpoint_lock", 0},
	{&trx_i_s_cache_lock_key, "trx_i_s_cache_lock", 0},
	{&trx_purge_latch_key, "trx_purge_latch", 0},
	{&index_tree_rw_lock_key, "index_tree_rw_lock", 0},
	{&dict_table_stats_latch_key, "dict_table_stats", 0}
};
# endif /* UNIV_PFS_RWLOCK */

# ifdef UNIV_PFS_THREAD
/* all_innodb_threads array contains threads that are
performance schema instrumented if "UNIV_PFS_THREAD"
is defined */
static PSI_thread_info	all_innodb_threads[] = {
	{&trx_rollback_clean_thread_key, "trx_rollback_clean_thread", 0},
	{&io_handler_thread_key, "io_handler_thread", 0},
	{&srv_lock_timeout_thread_key, "srv_lock_timeout_thread", 0},
	{&srv_error_monitor_thread_key, "srv_error_monitor_thread", 0},
	{&srv_monitor_thread_key, "srv_monitor_thread", 0},
	{&srv_master_thread_key, "srv_master_thread", 0},
	{&srv_purge_thread_key, "srv_purge_thread", 0},
	{&srv_log_tracking_thread_key, "srv_redo_log_follow_thread", 0}
};
# endif /* UNIV_PFS_THREAD */

# ifdef UNIV_PFS_IO
/* all_innodb_files array contains the type of files that are
performance schema instrumented if "UNIV_PFS_IO" is defined */
static PSI_file_info	all_innodb_files[] = {
	{&innodb_file_data_key, "innodb_data_file", 0},
	{&innodb_file_log_key, "innodb_log_file", 0},
	{&innodb_file_temp_key, "innodb_temp_file", 0},
	{&innodb_file_bmp_key, "innodb_bmp_file", 0}
};
# endif /* UNIV_PFS_IO */
#endif /* HAVE_PSI_INTERFACE */

static INNOBASE_SHARE *get_share(const char *table_name);
static void free_share(INNOBASE_SHARE *share);
static int innobase_close_connection(handlerton *hton, THD* thd);
static void innobase_kill_query(handlerton *hton, THD* thd, enum thd_kill_levels level);
static void innobase_commit_ordered(handlerton *hton, THD* thd, bool all);
static int innobase_commit(handlerton *hton, THD* thd, bool all);
static int innobase_rollback(handlerton *hton, THD* thd, bool all);
static int innobase_rollback_to_savepoint(handlerton *hton, THD* thd,
           void *savepoint);
static int innobase_savepoint(handlerton *hton, THD* thd, void *savepoint);
static int innobase_release_savepoint(handlerton *hton, THD* thd,
           void *savepoint);
static handler *innobase_create_handler(handlerton *hton,
                                        TABLE_SHARE *table,
                                        MEM_ROOT *mem_root);
/* "GEN_CLUST_INDEX" is the name reserved for Innodb default
system primary index. */
static const char innobase_index_reserve_name[]= "GEN_CLUST_INDEX";

/** @brief Initialize the default value of innodb_commit_concurrency.

Once InnoDB is running, the innodb_commit_concurrency must not change
from zero to nonzero. (Bug #42101)

The initial default value is 0, and without this extra initialization,
SET GLOBAL innodb_commit_concurrency=DEFAULT would set the parameter
to 0, even if it was initially set to nonzero at the command line
or configuration file. */
static
void
innobase_commit_concurrency_init_default(void);
/*==========================================*/

/************************************************************//**
Validate the file format name and return its corresponding id.
@return	valid file format id */
static
uint
innobase_file_format_name_lookup(
/*=============================*/
	const char*	format_name);		/*!< in: pointer to file format
						name */
/************************************************************//**
Validate the file format check config parameters, as a side effect it
sets the srv_max_file_format_at_startup variable.
@return	the format_id if valid config value, otherwise, return -1 */
static
int
innobase_file_format_validate_and_set(
/*==================================*/
	const char*	format_max);		/*!< in: parameter value */
/****************************************************************//**
Return alter table flags supported in an InnoDB database. */
static
uint
innobase_alter_table_flags(
/*=======================*/
	uint	flags);
/************************************************************//**
Synchronously read and parse the redo log up to the last
checkpoint to write the changed page bitmap.
@return 0 to indicate success.  Current implementation cannot fail. */
static
my_bool
innobase_flush_changed_page_bitmaps() __attribute__((unused));
/*==================================*/
/************************************************************//**
Delete all the bitmap files for data less than the specified LSN.
If called with lsn == 0 (i.e. set by RESET request) or
IB_ULONGLONG_MAX, restart the bitmap file sequence, otherwise
continue it.
@return 0 to indicate success, 1 for failure. */
static
my_bool
innobase_purge_changed_page_bitmaps(
/*================================*/
	ulonglong lsn) __attribute__((unused));	/*!< in: LSN to purge files up to */


/*****************************************************************//**
Check whether this is a fake change transaction.
@return TRUE if a fake change transaction */
static
my_bool
innobase_is_fake_change(
/*====================*/
	handlerton	*hton,  /*!< in: InnoDB handlerton */
	THD*		thd) __attribute__((unused));	/*!< in: MySQL thread handle of the user for
				  whom the transaction is being committed */

/** Get the list of foreign keys referencing a specified table
table.
@param thd		The thread handle
@param path		Path to the table
@param f_key_list[out]	The list of foreign keys

@return error code or zero for success */
static
int
innobase_get_parent_fk_list(
	THD*			thd,
	const char*		path,
	List<FOREIGN_KEY_INFO>*	f_key_list) __attribute__((unused));

/******************************************************************//**
Maps a MySQL trx isolation level code to the InnoDB isolation level code
@return	InnoDB isolation level */
static inline
ulint
innobase_map_isolation_level(
/*=========================*/
	enum_tx_isolation	iso);	/*!< in: MySQL isolation level code */

static const char innobase_hton_name[]= "InnoDB";

/*************************************************************//**
Check for a valid value of innobase_commit_concurrency.
@return	0 for valid innodb_commit_concurrency */
static
int
innobase_commit_concurrency_validate(
/*=================================*/
	THD*				thd,	/*!< in: thread handle */
	struct st_mysql_sys_var*	var,	/*!< in: pointer to system
						variable */
	void*				save,	/*!< out: immediate result
						for update function */
	struct st_mysql_value*		value)	/*!< in: incoming string */
{
	long long	intbuf;
	ulong		commit_concurrency;

	DBUG_ENTER("innobase_commit_concurrency_validate");

	if (value->val_int(value, &intbuf)) {
		/* The value is NULL. That is invalid. */
		DBUG_RETURN(1);
	}

	*reinterpret_cast<ulong*>(save) = commit_concurrency
		= static_cast<ulong>(intbuf);

	/* Allow the value to be updated, as long as it remains zero
	or nonzero. */
	DBUG_RETURN(!(!commit_concurrency == !innobase_commit_concurrency));
}

static MYSQL_THDVAR_BOOL(support_xa, PLUGIN_VAR_OPCMDARG,
  "Enable InnoDB support for the XA two-phase commit",
  /* check_func */ NULL, /* update_func */ NULL,
  /* default */ TRUE);

static MYSQL_THDVAR_BOOL(table_locks, PLUGIN_VAR_OPCMDARG,
  "Enable InnoDB locking in LOCK TABLES",
  /* check_func */ NULL, /* update_func */ NULL,
  /* default */ TRUE);

static MYSQL_THDVAR_BOOL(strict_mode, PLUGIN_VAR_OPCMDARG,
  "Use strict mode when evaluating create options.",
  NULL, NULL, FALSE);

static MYSQL_THDVAR_ULONG(lock_wait_timeout, PLUGIN_VAR_RQCMDARG,
  "Timeout in seconds an InnoDB transaction may wait for a lock before being rolled back. Values above 100000000 disable the timeout.",
  NULL, NULL, 50, 1, 1024 * 1024 * 1024, 0);

static MYSQL_THDVAR_ULONG(flush_log_at_trx_commit, PLUGIN_VAR_OPCMDARG,
  "Set to 0 (write and flush once per second),"
  " 1 (write and flush at each commit)"
  " or 2 (write at commit, flush once per second).",
  NULL, NULL, 1, 0, 2, 0);

static MYSQL_THDVAR_BOOL(fake_changes, PLUGIN_VAR_OPCMDARG,
  "In the transaction after enabled, UPDATE, INSERT and DELETE only move the cursor to the records "
  "and do nothing other operations (no changes, no ibuf, no undo, no transaction log) in the transaction. "
  "This is to cause replication prefetch IO. ATTENTION: the transaction started after enabled is affected.",
  NULL, NULL, FALSE);

static MYSQL_THDVAR_ULONG(merge_sort_block_size, PLUGIN_VAR_RQCMDARG,
  "The block size used doing external merge-sort for secondary index creation.",
  NULL, NULL, 1UL << 20, 1UL << 20, 1UL << 30, 0);

static handler *innobase_create_handler(handlerton *hton,
                                        TABLE_SHARE *table,
                                        MEM_ROOT *mem_root)
{
  return new (mem_root) ha_innobase(hton, table);
}

/*******************************************************************//**
This function is used to prepare an X/Open XA distributed transaction.
@return	0 or error number */
static
int
innobase_xa_prepare(
/*================*/
        handlerton*	hton,	/*!< in: InnoDB handlerton */
	THD*		thd,	/*!< in: handle to the MySQL thread of
				the user whose XA transaction should
				be prepared */
	bool		all);	/*!< in: TRUE - commit transaction
				FALSE - the current SQL statement
				ended */
/*******************************************************************//**
This function is used to recover X/Open XA distributed transactions.
@return	number of prepared transactions stored in xid_list */
static
int
innobase_xa_recover(
/*================*/
	handlerton*	hton,	/*!< in: InnoDB handlerton */
	XID*		xid_list,/*!< in/out: prepared transactions */
	uint		len);	/*!< in: number of slots in xid_list */
/*******************************************************************//**
This function is used to commit one X/Open XA distributed transaction
which is in the prepared state
@return	0 or error number */
static
int
innobase_commit_by_xid(
/*===================*/
	handlerton* hton,
	XID*	xid);	/*!< in: X/Open XA transaction identification */
/*******************************************************************//**
This function is used to rollback one X/Open XA distributed transaction
which is in the prepared state
@return	0 or error number */
static
int
innobase_rollback_by_xid(
/*=====================*/
	handlerton*	hton,	/*!< in: InnoDB handlerton */
	XID*		xid);	/*!< in: X/Open XA transaction
				identification */
/*******************************************************************//**
Create a consistent view for a cursor based on current transaction
which is created if the corresponding MySQL thread still lacks one.
This consistent view is then used inside of MySQL when accessing records
using a cursor.
@return	pointer to cursor view or NULL */
static
void*
innobase_create_cursor_view(
/*========================*/
	handlerton*	hton,	/*!< in: innobase hton */
	THD*		thd);	/*!< in: user thread handle */
/*******************************************************************//**
Set the given consistent cursor view to a transaction which is created
if the corresponding MySQL thread still lacks one. If the given
consistent cursor view is NULL global read view of a transaction is
restored to a transaction read view. */
static
void
innobase_set_cursor_view(
/*=====================*/
	handlerton* hton,
	THD*	thd,	/*!< in: user thread handle */
	void*	curview);/*!< in: Consistent cursor view to be set */
/*******************************************************************//**
Close the given consistent cursor view of a transaction and restore
global read view to a transaction read view. Transaction is created if the
corresponding MySQL thread still lacks one. */
static
void
innobase_close_cursor_view(
/*=======================*/
	handlerton* hton,
	THD*	thd,	/*!< in: user thread handle */
	void*	curview);/*!< in: Consistent read view to be closed */
/*****************************************************************//**
Removes all tables in the named database inside InnoDB. */
static
void
innobase_drop_database(
/*===================*/
	handlerton* hton, /*!< in: handlerton of Innodb */
	char*	path);	/*!< in: database path; inside InnoDB the name
			of the last directory in the path is used as
			the database name: for example, in 'mysql/data/test'
			the database name is 'test' */
/*******************************************************************//**
Closes an InnoDB database. */
static
int
innobase_end(handlerton *hton, ha_panic_function type);

/*****************************************************************//**
Creates an InnoDB transaction struct for the thd if it does not yet have one.
Starts a new InnoDB transaction if a transaction is not yet started. And
assigns a new snapshot for a consistent read if the transaction does not yet
have one.
@return	0 */
static
int
innobase_start_trx_and_assign_read_view(
/*====================================*/
			/* out: 0 */
	handlerton* hton, /* in: Innodb handlerton */
	THD*	thd);	/* in: MySQL thread handle of the user for whom
			the transaction should be committed */
/****************************************************************//**
Flushes InnoDB logs to disk and makes a checkpoint. Really, a commit flushes
the logs, and the name of this function should be innobase_checkpoint.
@return	TRUE if error */
static
bool
innobase_flush_logs(
/*================*/
	handlerton*	hton);	/*!< in: InnoDB handlerton */

/************************************************************************//**
Implements the SHOW INNODB STATUS command. Sends the output of the InnoDB
Monitor to the client. */
static
bool
innodb_show_status(
/*===============*/
	handlerton*	hton,	/*!< in: the innodb handlerton */
	THD*	thd,	/*!< in: the MySQL query thread of the caller */
	stat_print_fn *stat_print);
static
bool innobase_show_status(handlerton *hton, THD* thd,
                          stat_print_fn* stat_print,
                          enum ha_stat_type stat_type);

/* Enable / disable checkpoints */
static int innobase_checkpoint_state(handlerton *hton, bool disable)
{
  if (disable)
    (void) log_disable_checkpoint();
  else
     log_enable_checkpoint();
  return 0;
}


/*****************************************************************//**
Commits a transaction in an InnoDB database. */
static
void
innobase_commit_low(
/*================*/
	trx_t*	trx);	/*!< in: transaction handle */

static SHOW_VAR innodb_status_variables[]= {
  {"adaptive_hash_cells",
  (char*) &export_vars.innodb_adaptive_hash_cells,	  SHOW_LONG},
  {"adaptive_hash_hash_searches",
  (char*) &export_vars.innodb_adaptive_hash_hash_searches, SHOW_LONG},
  {"adaptive_hash_heap_buffers",
  (char*) &export_vars.innodb_adaptive_hash_heap_buffers, SHOW_LONG},
  {"adaptive_hash_non_hash_searches",
  (char*) &export_vars.innodb_adaptive_hash_non_hash_searches, SHOW_LONG},
  {"background_log_sync",
  (char*) &export_vars.innodb_background_log_sync,	  SHOW_LONG},
  {"buffer_pool_bytes_data",
  (char*) &export_vars.innodb_buffer_pool_bytes_data,	  SHOW_LONG},
  {"buffer_pool_bytes_dirty",
  (char*) &export_vars.innodb_buffer_pool_bytes_dirty,	  SHOW_LONG},
  {"buffer_pool_pages_data",
  (char*) &export_vars.innodb_buffer_pool_pages_data,	  SHOW_LONG},
  {"buffer_pool_pages_dirty",
  (char*) &export_vars.innodb_buffer_pool_pages_dirty,	  SHOW_LONG},
  {"buffer_pool_pages_flushed",
  (char*) &export_vars.innodb_buffer_pool_pages_flushed,  SHOW_LONG},
  {"buffer_pool_pages_free",
  (char*) &export_vars.innodb_buffer_pool_pages_free,	  SHOW_LONG},
#ifdef UNIV_DEBUG
  {"buffer_pool_pages_latched",
  (char*) &export_vars.innodb_buffer_pool_pages_latched,  SHOW_LONG},
#endif /* UNIV_DEBUG */
  {"buffer_pool_pages_LRU_flushed",
  (char*) &export_vars.innodb_buffer_pool_pages_LRU_flushed,  SHOW_LONG},
  {"buffer_pool_pages_made_not_young",
  (char*) &export_vars.innodb_buffer_pool_pages_made_not_young, SHOW_LONG},
  {"buffer_pool_pages_made_young",
  (char*) &export_vars.innodb_buffer_pool_pages_made_young, SHOW_LONG},
  {"buffer_pool_pages_misc",
  (char*) &export_vars.innodb_buffer_pool_pages_misc,	  SHOW_LONG},
  {"buffer_pool_pages_old",
  (char*) &export_vars.innodb_buffer_pool_pages_old,	  SHOW_LONG},
  {"buffer_pool_pages_total",
  (char*) &export_vars.innodb_buffer_pool_pages_total,	  SHOW_LONG},
  {"buffer_pool_read_ahead",
  (char*) &export_vars.innodb_buffer_pool_read_ahead,	  SHOW_LONG},
  {"buffer_pool_read_ahead_evicted",
  (char*) &export_vars.innodb_buffer_pool_read_ahead_evicted, SHOW_LONG},
  {"buffer_pool_read_ahead_rnd",
  (char*) &export_vars.innodb_buffer_pool_read_ahead_rnd, SHOW_LONG},
  {"buffer_pool_read_requests",
  (char*) &export_vars.innodb_buffer_pool_read_requests,  SHOW_LONG},
  {"buffer_pool_reads",
  (char*) &export_vars.innodb_buffer_pool_reads,	  SHOW_LONG},
  {"buffer_pool_wait_free",
  (char*) &export_vars.innodb_buffer_pool_wait_free,	  SHOW_LONG},
  {"buffer_pool_write_requests",
  (char*) &export_vars.innodb_buffer_pool_write_requests, SHOW_LONG},
  {"checkpoint_age",
  (char*) &export_vars.innodb_checkpoint_age,		  SHOW_LONG},
  {"checkpoint_max_age",
  (char*) &export_vars.innodb_checkpoint_max_age,	  SHOW_LONG},
  {"checkpoint_target_age",
  (char*) &export_vars.innodb_checkpoint_target_age,	  SHOW_LONG},
  {"current_row_locks",
  (char*) &export_vars.innodb_current_row_locks,		  SHOW_LONG},
  {"data_fsyncs",
  (char*) &export_vars.innodb_data_fsyncs,		  SHOW_LONG},
  {"data_pending_fsyncs",
  (char*) &export_vars.innodb_data_pending_fsyncs,	  SHOW_LONG},
  {"data_pending_reads",
  (char*) &export_vars.innodb_data_pending_reads,	  SHOW_LONG},
  {"data_pending_writes",
  (char*) &export_vars.innodb_data_pending_writes,	  SHOW_LONG},
  {"data_read",
  (char*) &export_vars.innodb_data_read,		  SHOW_LONG},
  {"data_reads",
  (char*) &export_vars.innodb_data_reads,		  SHOW_LONG},
  {"data_writes",
  (char*) &export_vars.innodb_data_writes,		  SHOW_LONG},
  {"data_written",
  (char*) &export_vars.innodb_data_written,		  SHOW_LONG},
  {"dblwr_pages_written",
  (char*) &export_vars.innodb_dblwr_pages_written,	  SHOW_LONG},
  {"dblwr_writes",
  (char*) &export_vars.innodb_dblwr_writes,		  SHOW_LONG},
  {"deadlocks",
  (char*) &export_vars.innodb_deadlocks,		  SHOW_LONG},
  {"descriptors_memory",
  (char*) &export_vars.innodb_descriptors_memory,	  SHOW_LONG},
  {"dict_tables",
  (char*) &export_vars.innodb_dict_tables,		  SHOW_LONG},
  {"have_atomic_builtins",
  (char*) &export_vars.innodb_have_atomic_builtins,	  SHOW_BOOL},
  {"history_list_length",
  (char*) &export_vars.innodb_history_list_length,	  SHOW_LONG},
  {"ibuf_discarded_delete_marks",
  (char*) &export_vars.innodb_ibuf_discarded_delete_marks, SHOW_LONG},
  {"ibuf_discarded_deletes",
  (char*) &export_vars.innodb_ibuf_discarded_deletes,	  SHOW_LONG},
  {"ibuf_discarded_inserts",
  (char*) &export_vars.innodb_ibuf_discarded_inserts,	  SHOW_LONG},
  {"ibuf_free_list",
  (char*) &export_vars.innodb_ibuf_free_list,		  SHOW_LONG},
  {"ibuf_merged_delete_marks",
  (char*) &export_vars.innodb_ibuf_merged_delete_marks,	  SHOW_LONG},
  {"ibuf_merged_deletes",
  (char*) &export_vars.innodb_ibuf_merged_deletes,	  SHOW_LONG},
  {"ibuf_merged_inserts",
  (char*) &export_vars.innodb_ibuf_merged_inserts,	  SHOW_LONG},
  {"ibuf_merges",
  (char*) &export_vars.innodb_ibuf_merges,		  SHOW_LONG},
  {"ibuf_segment_size",
  (char*) &export_vars.innodb_ibuf_segment_size,	  SHOW_LONG},
  {"ibuf_size",
  (char*) &export_vars.innodb_ibuf_size,		  SHOW_LONG},
  {"log_waits",
  (char*) &export_vars.innodb_log_waits,		  SHOW_LONG},
  {"log_write_requests",
  (char*) &export_vars.innodb_log_write_requests,	  SHOW_LONG},
  {"log_writes",
  (char*) &export_vars.innodb_log_writes,		  SHOW_LONG},
  {"lsn_current",
  (char*) &export_vars.innodb_lsn_current,		  SHOW_LONGLONG},
  {"lsn_flushed",
  (char*) &export_vars.innodb_lsn_flushed,		  SHOW_LONGLONG},
  {"lsn_last_checkpoint",
  (char*) &export_vars.innodb_lsn_last_checkpoint,	  SHOW_LONGLONG},
  {"master_thread_1_second_loops",
  (char*) &export_vars.innodb_master_thread_1_second_loops, SHOW_LONG},
  {"master_thread_10_second_loops",
  (char*) &export_vars.innodb_master_thread_10_second_loops, SHOW_LONG},
  {"master_thread_background_loops",
  (char*) &export_vars.innodb_master_thread_background_loops, SHOW_LONG},
  {"master_thread_main_flush_loops",
  (char*) &export_vars.innodb_master_thread_main_flush_loops, SHOW_LONG},
  {"master_thread_sleeps",
  (char*) &export_vars.innodb_master_thread_sleeps,	  SHOW_LONG},
  {"max_trx_id",
  (char*) &export_vars.innodb_max_trx_id,		  SHOW_LONGLONG},
  {"mem_adaptive_hash",
  (char*) &export_vars.innodb_mem_adaptive_hash,	  SHOW_LONG},
  {"mem_dictionary",
  (char*) &export_vars.innodb_mem_dictionary,		  SHOW_LONG},
  {"mem_total",
  (char*) &export_vars.innodb_mem_total,		  SHOW_LONG},
  {"mutex_os_waits",
  (char*) &export_vars.innodb_mutex_os_waits,		  SHOW_LONGLONG},
  {"mutex_spin_rounds",
  (char*) &export_vars.innodb_mutex_spin_rounds,	  SHOW_LONGLONG},
  {"mutex_spin_waits",
  (char*) &export_vars.innodb_mutex_spin_waits,		  SHOW_LONGLONG},
  {"oldest_view_low_limit_trx_id",
  (char*) &export_vars.innodb_oldest_view_low_limit_trx_id, SHOW_LONGLONG},
  {"os_log_fsyncs",
  (char*) &export_vars.innodb_os_log_fsyncs,		  SHOW_LONG},
  {"os_log_pending_fsyncs",
  (char*) &export_vars.innodb_os_log_pending_fsyncs,	  SHOW_LONG},
  {"os_log_pending_writes",
  (char*) &export_vars.innodb_os_log_pending_writes,	  SHOW_LONG},
  {"os_log_written",
  (char*) &export_vars.innodb_os_log_written,		  SHOW_LONG},
  {"page_size",
  (char*) &export_vars.innodb_page_size,		  SHOW_LONG},
  {"pages_created",
  (char*) &export_vars.innodb_pages_created,		  SHOW_LONG},
  {"pages_read",
  (char*) &export_vars.innodb_pages_read,		  SHOW_LONG},
  {"pages_written",
  (char*) &export_vars.innodb_pages_written,		  SHOW_LONG},
  {"purge_trx_id",
  (char*) &export_vars.innodb_purge_trx_id,		  SHOW_LONGLONG},
#ifdef UNIV_DEBUG
  {"purge_trx_id_age",
  (char*) &export_vars.innodb_purge_trx_id_age,		  SHOW_LONG},
#endif /* UNIV_DEBUG */
  {"purge_undo_no",
  (char*) &export_vars.innodb_purge_undo_no,		  SHOW_LONGLONG},
#ifdef UNIV_DEBUG
  {"purge_view_trx_id_age",
  (char*) &export_vars.innodb_purge_view_trx_id_age,	  SHOW_LONG},
#endif /* UNIV_DEBUG */
  {"read_views_memory",
  (char*) &export_vars.innodb_read_views_memory,	  SHOW_LONG},
  {"row_lock_current_waits",
  (char*) &export_vars.innodb_row_lock_current_waits,	  SHOW_LONG},
  {"row_lock_time",
  (char*) &export_vars.innodb_row_lock_time,		  SHOW_LONGLONG},
  {"row_lock_time_avg",
  (char*) &export_vars.innodb_row_lock_time_avg,	  SHOW_LONG},
  {"row_lock_time_max",
  (char*) &export_vars.innodb_row_lock_time_max,	  SHOW_LONG},
  {"row_lock_waits",
  (char*) &export_vars.innodb_row_lock_waits,		  SHOW_LONG},
  {"rows_deleted",
  (char*) &export_vars.innodb_rows_deleted,		  SHOW_LONG},
  {"rows_inserted",
  (char*) &export_vars.innodb_rows_inserted,		  SHOW_LONG},
  {"rows_read",
  (char*) &export_vars.innodb_rows_read,		  SHOW_LONG},
  {"rows_updated",
  (char*) &export_vars.innodb_rows_updated,		  SHOW_LONG},
  {"s_lock_os_waits",
  (char*) &export_vars.innodb_s_lock_os_waits,		  SHOW_LONGLONG},
  {"s_lock_spin_rounds",
  (char*) &export_vars.innodb_s_lock_spin_rounds,	  SHOW_LONGLONG},
  {"s_lock_spin_waits",
  (char*) &export_vars.innodb_s_lock_spin_waits,	  SHOW_LONGLONG},
  {"truncated_status_writes",
  (char*) &export_vars.innodb_truncated_status_writes,	SHOW_LONG},
  {"x_lock_os_waits",
  (char*) &export_vars.innodb_x_lock_os_waits,		  SHOW_LONGLONG},
  {"x_lock_spin_rounds",
  (char*) &export_vars.innodb_x_lock_spin_rounds,	  SHOW_LONGLONG},
  {"x_lock_spin_waits",
  (char*) &export_vars.innodb_x_lock_spin_waits,	  SHOW_LONGLONG},
  {NullS, NullS, SHOW_LONG}
};

/* General functions */

/******************************************************************//**
Returns true if the thread is the replication thread on the slave
server. Used in srv_conc_enter_innodb() to determine if the thread
should be allowed to enter InnoDB - the replication thread is treated
differently than other threads. Also used in
srv_conc_force_exit_innodb().
@return	true if thd is the replication thread */
extern "C" UNIV_INTERN
ibool
thd_is_replication_slave_thread(
/*============================*/
	const void*	thd)	/*!< in: thread handle (THD*) */
{
	return((ibool) thd_slave_thread((THD*) thd));
}

/******************************************************************//**
Save some CPU by testing the value of srv_thread_concurrency in inline
functions. */
static inline
void
innodb_srv_conc_enter_innodb(
/*=========================*/
	trx_t*	trx)	/*!< in: transaction handle */
{
	if (UNIV_LIKELY(!srv_thread_concurrency)) {

		return;
	}

	srv_conc_enter_innodb(trx);
}

/******************************************************************//**
Save some CPU by testing the value of srv_thread_concurrency in inline
functions. */
static inline
void
innodb_srv_conc_exit_innodb(
/*========================*/
	trx_t*	trx)	/*!< in: transaction handle */
{
	if (UNIV_LIKELY(!trx->declared_to_be_inside_innodb)) {

		return;
	}

	srv_conc_exit_innodb(trx);
}

/******************************************************************//**
Force a thread to leave InnoDB even if it has spare tickets. */
static inline
void
innodb_srv_conc_force_exit_innodb(
/*==============================*/
	trx_t*	trx)	/*!< in: transaction handle */
{
#ifdef UNIV_SYNC_DEBUG
	ut_ad(!sync_thread_levels_nonempty_trx(trx->has_search_latch));
#endif /* UNIV_SYNC_DEBUG */

	if (trx->declared_to_be_inside_innodb) {

		srv_conc_force_exit_innodb(trx);
	}
}

/******************************************************************//**
Returns true if the transaction this thread is processing has edited
non-transactional tables. Used by the deadlock detector when deciding
which transaction to rollback in case of a deadlock - we try to avoid
rolling back transactions that have edited non-transactional tables.
@return	true if non-transactional tables have been edited */
extern "C" UNIV_INTERN
ibool
thd_has_edited_nontrans_tables(
/*===========================*/
	void*	thd)	/*!< in: thread handle (THD*) */
{
	return((ibool) thd_non_transactional_update((THD*) thd));
}

/******************************************************************//**
Returns true if the thread is executing a SELECT statement.
@return	true if thd is executing SELECT */
extern "C" UNIV_INTERN
ibool
thd_is_select(
/*==========*/
	const void*	thd)	/*!< in: thread handle (THD*) */
{
	return(thd_sql_command((const THD*) thd) == SQLCOM_SELECT);
}

/******************************************************************//**
Returns true if the thread supports XA,
global value of innodb_supports_xa if thd is NULL.
@return	true if thd has XA support */
extern "C" UNIV_INTERN
ibool
thd_supports_xa(
/*============*/
	void*	thd)	/*!< in: thread handle (THD*), or NULL to query
			the global innodb_supports_xa */
{
	return(THDVAR((THD*) thd, support_xa));
}

/******************************************************************//**
Check the status of fake changes mode (innodb_fake_changes)
@return	true	if fake change mode is enabled. */
extern "C" UNIV_INTERN
ibool
thd_fake_changes(
/*=============*/
	void*	thd)	/*!< in: thread handle, or NULL to query
			the global innodb_supports_xa */
{
	return(THDVAR((THD*) thd, fake_changes));
}

/******************************************************************//**
Returns the lock wait timeout for the current connection.
@return	the lock wait timeout, in seconds */
extern "C" UNIV_INTERN
ulong
thd_lock_wait_timeout(
/*==================*/
	void*	thd)	/*!< in: thread handle (THD*), or NULL to query
			the global innodb_lock_wait_timeout */
{
	/* According to <mysql/plugin.h>, passing thd == NULL
	returns the global value of the session variable. */
	return(THDVAR((THD*) thd, lock_wait_timeout));
}

/******************************************************************//**
Set the time waited for the lock for the current query. */
extern "C" UNIV_INTERN
void
thd_set_lock_wait_time(
/*===================*/
	void*	thd,	/*!< in: thread handle (THD*) */
	ulint	value)	/*!< in: time waited for the lock */
{
	if (thd) {
		thd_storage_lock_wait((THD*)thd, value);
	}
}

/******************************************************************//**
*/
extern "C" UNIV_INTERN
ulong
thd_flush_log_at_trx_commit(
/*================================*/
	void*	thd)
{
	return(THDVAR((THD*) thd, flush_log_at_trx_commit));
}

/******************************************************************//**
Returns the merge-sort block size used for the secondary index creation
for the current connection.
@return	the merge-sort block size, in bytes */
extern "C" UNIV_INTERN
ulong
thd_merge_sort_block_size(
/*================================*/
	void*	thd)	/*!< in: thread handle (THD*), or NULL to query
+			the global merge_sort_block_size */
{
	return(THDVAR((THD*) thd, merge_sort_block_size));
}

/********************************************************************//**
Obtain the InnoDB transaction of a MySQL thread.
@return	reference to transaction pointer */
static inline
trx_t*&
thd_to_trx(
/*=======*/
	THD*	thd)	/*!< in: MySQL thread */
{
	return(*(trx_t**) thd_ha_data(thd, innodb_hton_ptr));
}

my_bool
ha_innobase::is_fake_change_enabled(THD* thd)
{
	trx_t*	trx	= thd_to_trx(thd);
	return(trx && UNIV_UNLIKELY(trx->fake_changes));
}

/********************************************************************//**
Call this function when mysqld passes control to the client. That is to
avoid deadlocks on the adaptive hash S-latch possibly held by thd. For more
documentation, see handler.cc.
@return	0 */
static
int
innobase_release_temporary_latches(
/*===============================*/
	handlerton*	hton,	/*!< in: handlerton */
	THD*		thd)	/*!< in: MySQL thread */
{
	trx_t*	trx;

	DBUG_ASSERT(hton == innodb_hton_ptr);

	if (!innodb_inited) {

		return(0);
	}

	trx = thd_to_trx(thd);

	if (trx != NULL) {

		/* No-op in XtraDB */
		trx_search_latch_release_if_reserved(trx);
	}

	return(0);
}

/********************************************************************//**
Increments innobase_active_counter and every INNOBASE_WAKE_INTERVALth
time calls srv_active_wake_master_thread. This function should be used
when a single database operation may introduce a small need for
server utility activity, like checkpointing. */
static inline
void
innobase_active_small(void)
/*=======================*/
{
	innobase_active_counter++;

	if ((innobase_active_counter % INNOBASE_WAKE_INTERVAL) == 0) {
		srv_active_wake_master_thread();
	}
}

/********************************************************************//**
Converts an InnoDB error code to a MySQL error code and also tells to MySQL
about a possible transaction rollback inside InnoDB caused by a lock wait
timeout or a deadlock.
@return	MySQL error code */
extern "C" UNIV_INTERN
int
convert_error_code_to_mysql(
/*========================*/
	int	error,	/*!< in: InnoDB error code */
	ulint	flags,  /*!< in: InnoDB table flags, or 0 */
	THD*	thd)	/*!< in: user thread handle or NULL */
{
	switch (error) {
	case DB_SUCCESS:
		return(0);

	case DB_INTERRUPTED:
                return(HA_ERR_ABORTED_BY_USER);

	case DB_FOREIGN_EXCEED_MAX_CASCADE:
		push_warning_printf(thd, MYSQL_ERROR::WARN_LEVEL_WARN,
				    HA_ERR_ROW_IS_REFERENCED,
				    "InnoDB: Cannot delete/update "
				    "rows with cascading foreign key "
				    "constraints that exceed max "
				    "depth of %d. Please "
				    "drop extra constraints and try "
				    "again", DICT_FK_MAX_RECURSIVE_LOAD);

		/* fall through */

	case DB_ERROR:
	default:
		return(-1); /* unspecified error */

	case DB_DUPLICATE_KEY:
		/* Be cautious with returning this error, since
		mysql could re-enter the storage layer to get
		duplicated key info, the operation requires a
		valid table handle and/or transaction information,
		which might not always be available in the error
		handling stage. */
		return(HA_ERR_FOUND_DUPP_KEY);

	case DB_FOREIGN_DUPLICATE_KEY:
		return(HA_ERR_FOREIGN_DUPLICATE_KEY);

	case DB_MISSING_HISTORY:
		return(HA_ERR_TABLE_DEF_CHANGED);

	case DB_RECORD_NOT_FOUND:
		return(HA_ERR_NO_ACTIVE_RECORD);

        case DB_SEARCH_ABORTED_BY_USER:
                return(HA_ERR_ABORTED_BY_USER);

	case DB_DEADLOCK:
		/* Since we rolled back the whole transaction, we must
		tell it also to MySQL so that MySQL knows to empty the
		cached binlog for this transaction */

		if (thd) {
			thd_mark_transaction_to_rollback(thd, TRUE);
		}

		return(HA_ERR_LOCK_DEADLOCK);

	case DB_LOCK_WAIT_TIMEOUT:
		/* Starting from 5.0.13, we let MySQL just roll back the
		latest SQL statement in a lock wait timeout. Previously, we
		rolled back the whole transaction. */

		if (thd) {
			thd_mark_transaction_to_rollback(
				thd, (bool)row_rollback_on_timeout);
		}

		return(HA_ERR_LOCK_WAIT_TIMEOUT);

	case DB_NO_REFERENCED_ROW:
		return(HA_ERR_NO_REFERENCED_ROW);

	case DB_ROW_IS_REFERENCED:
		return(HA_ERR_ROW_IS_REFERENCED);

	case DB_CANNOT_ADD_CONSTRAINT:
	case DB_CHILD_NO_INDEX:
	case DB_PARENT_NO_INDEX:
		return(HA_ERR_CANNOT_ADD_FOREIGN);

	case DB_CANNOT_DROP_CONSTRAINT:

		return(HA_ERR_ROW_IS_REFERENCED); /* TODO: This is a bit
						misleading, a new MySQL error
						code should be introduced */

	case DB_CORRUPTION:
		return(HA_ERR_CRASHED);

	case DB_OUT_OF_FILE_SPACE:
		return(HA_ERR_RECORD_FILE_FULL);

	case DB_TABLE_IN_FK_CHECK:
		return(HA_ERR_TABLE_IN_FK_CHECK);

	case DB_TABLE_IS_BEING_USED:
		return(HA_ERR_WRONG_COMMAND);

	case DB_TABLE_NOT_FOUND:
		return(HA_ERR_NO_SUCH_TABLE);

	case DB_TOO_BIG_RECORD: {
		/* If prefix is true then a 768-byte prefix is stored
		locally for BLOB fields. Refer to dict_table_get_format() */
		bool prefix = ((flags & DICT_TF_FORMAT_MASK)
		 	       >> DICT_TF_FORMAT_SHIFT) < UNIV_FORMAT_B;
		my_printf_error(ER_TOO_BIG_ROWSIZE,
			"Row size too large (> %lu). Changing some columns "
			"to TEXT or BLOB %smay help. In current row "
			"format, BLOB prefix of %d bytes is stored inline.",
			MYF(0),
			page_get_free_space_of_empty(flags &
				DICT_TF_COMPACT) / 2,
			prefix ? "or using ROW_FORMAT=DYNAMIC "
			"or ROW_FORMAT=COMPRESSED ": "",
			prefix ? DICT_MAX_FIXED_COL_LEN : 0);
		return(HA_ERR_TO_BIG_ROW);
	}

	case DB_TOO_BIG_INDEX_COL:
		my_error(ER_INDEX_COLUMN_TOO_LONG, MYF(0),
			 DICT_MAX_FIELD_LEN_BY_FORMAT_FLAG(flags));
		return(HA_ERR_INDEX_COL_TOO_LONG);

	case DB_NO_SAVEPOINT:
		return(HA_ERR_NO_SAVEPOINT);

	case DB_LOCK_TABLE_FULL:
		/* Since we rolled back the whole transaction, we must
		tell it also to MySQL so that MySQL knows to empty the
		cached binlog for this transaction */

		if (thd) {
			thd_mark_transaction_to_rollback(thd, TRUE);
		}

		return(HA_ERR_LOCK_TABLE_FULL);

	case DB_PRIMARY_KEY_IS_NULL:
		return(ER_PRIMARY_CANT_HAVE_NULL);

	case DB_TOO_MANY_CONCURRENT_TRXS:
		/* New error code HA_ERR_TOO_MANY_CONCURRENT_TRXS is only
		available in 5.1.38 and later, but the plugin should still
		work with previous versions of MySQL. */
#ifdef HA_ERR_TOO_MANY_CONCURRENT_TRXS
		return(HA_ERR_TOO_MANY_CONCURRENT_TRXS);
#else /* HA_ERR_TOO_MANY_CONCURRENT_TRXS */
		return(HA_ERR_RECORD_FILE_FULL);
#endif /* HA_ERR_TOO_MANY_CONCURRENT_TRXS */
	case DB_UNSUPPORTED:
		return(HA_ERR_UNSUPPORTED);
	case DB_INDEX_CORRUPT:
		return(HA_ERR_INDEX_CORRUPT);
	case DB_UNDO_RECORD_TOO_BIG:
		return(HA_ERR_UNDO_REC_TOO_BIG);
	case DB_OUT_OF_MEMORY:
		return(HA_ERR_OUT_OF_MEM);
	case DB_IDENTIFIER_TOO_LONG:
		return(HA_ERR_INTERNAL_ERROR);
	}
}

/*************************************************************//**
Prints info of a THD object (== user session thread) to the given file. */
extern "C" UNIV_INTERN
void
innobase_mysql_print_thd(
/*=====================*/
	FILE*	f,		/*!< in: output stream */
	void*	thd,		/*!< in: pointer to a MySQL THD object */
	uint	max_query_len)	/*!< in: max query length to print, or 0 to
				   use the default max length */
{
	char	buffer[1024];

	fputs(thd_security_context((THD*) thd, buffer, sizeof buffer,
				   max_query_len), f);
	putc('\n', f);
}

/******************************************************************//**
Get the variable length bounds of the given character set. */
extern "C" UNIV_INTERN
void
innobase_get_cset_width(
/*====================*/
	ulint	cset,		/*!< in: MySQL charset-collation code */
	ulint*	mbminlen,	/*!< out: minimum length of a char (in bytes) */
	ulint*	mbmaxlen)	/*!< out: maximum length of a char (in bytes) */
{
	CHARSET_INFO*	cs;
	ut_ad(cset < 256);
	ut_ad(mbminlen);
	ut_ad(mbmaxlen);

	cs = all_charsets[cset];
	if (cs) {
		*mbminlen = cs->mbminlen;
		*mbmaxlen = cs->mbmaxlen;
		ut_ad(*mbminlen < DATA_MBMAX);
		ut_ad(*mbmaxlen < DATA_MBMAX);
	} else {
		THD*	thd = current_thd;

		if (thd && thd_sql_command(thd) == SQLCOM_DROP_TABLE) {

			/* Fix bug#46256: allow tables to be dropped if the
			collation is not found, but issue a warning. */
			if ((global_system_variables.log_warnings)
			    && (cset != 0)){

				sql_print_warning(
					"Unknown collation #%lu.", cset);
			}
		} else {

			ut_a(cset == 0);
		}

		*mbminlen = *mbmaxlen = 0;
	}
}

/******************************************************************//**
Converts an identifier to a table name. */
extern "C" UNIV_INTERN
void
innobase_convert_from_table_id(
/*===========================*/
	struct charset_info_st*	cs,	/*!< in: the 'from' character set */
	char*			to,	/*!< out: converted identifier */
	const char*		from,	/*!< in: identifier to convert */
	ulint			len)	/*!< in: length of 'to', in bytes */
{
	uint	errors;

	strconvert(cs, from, &my_charset_filename, to, (uint) len, &errors);
}

/**********************************************************************
Check if the length of the identifier exceeds the maximum allowed.
return true when length of identifier is too long. */
extern "C"
my_bool
innobase_check_identifier_length(
/*=============================*/
	const char*	id)	/* in: FK identifier to check excluding the
				database portion. */
{
	int		well_formed_error = 0;
	CHARSET_INFO	*cs = system_charset_info;
	DBUG_ENTER("innobase_check_identifier_length");

	uint res = cs->cset->well_formed_len(cs, id, id + strlen(id),
					     NAME_CHAR_LEN,
					     &well_formed_error);

	if (well_formed_error || res == NAME_CHAR_LEN) {
		my_error(ER_TOO_LONG_IDENT, MYF(0), id);
		DBUG_RETURN(true);
	}
	DBUG_RETURN(false);
}

/******************************************************************//**
Converts an identifier to UTF-8. */
extern "C" UNIV_INTERN
void
innobase_convert_from_id(
/*=====================*/
	struct charset_info_st*	cs,	/*!< in: the 'from' character set */
	char*			to,	/*!< out: converted identifier */
	const char*		from,	/*!< in: identifier to convert */
	ulint			len)	/*!< in: length of 'to', in bytes */
{
	uint	errors;

	strconvert(cs, from, system_charset_info, to, (uint) len, &errors);
}

/**********************************************************************
Converts an identifier from my_charset_filename to UTF-8 charset.
@return result string length, as returned by strconvert() */
extern "C"
uint
innobase_convert_to_system_charset(
/*===============================*/
	char*		to,	/* out: converted identifier */
	const char*	from,	/* in: identifier to convert */
	ulint		len,	/* in: length of 'to', in bytes */
	uint*		errors)	/* out: error return */
{
	CHARSET_INFO*	cs1 = &my_charset_filename;
	CHARSET_INFO*	cs2 = system_charset_info;

	return(strconvert(cs1, from, cs2, to, len, errors));
}

/******************************************************************//**
Compares NUL-terminated UTF-8 strings case insensitively.
@return	0 if a=b, <0 if a<b, >1 if a>b */
extern "C" UNIV_INTERN
int
innobase_strcasecmp(
/*================*/
	const char*	a,	/*!< in: first string to compare */
	const char*	b)	/*!< in: second string to compare */
{
	return(my_strcasecmp(system_charset_info, a, b));
}

/******************************************************************//**
Strip dir name from a full path name and return only the file name
@return file name or "null" if no file name */
extern "C" UNIV_INTERN
const char*
innobase_basename(
/*==============*/
	const char*	path_name)	/*!< in: full path name */
{
	const char*	name = base_name(path_name);

	return((name) ? name : "null");
}

/******************************************************************//**
Makes all characters in a NUL-terminated UTF-8 string lower case. */
extern "C" UNIV_INTERN
void
innobase_casedn_str(
/*================*/
	char*	a)	/*!< in/out: string to put in lower case */
{
	my_casedn_str(system_charset_info, a);
}

/**********************************************************************//**
Determines the connection character set.
@return	connection character set */
extern "C" UNIV_INTERN
struct charset_info_st*
innobase_get_charset(
/*=================*/
	void*	mysql_thd)	/*!< in: MySQL thread handle */
{
	return(thd_charset((THD*) mysql_thd));
}

/**********************************************************************//**
Determines the current SQL statement.
@return	SQL statement string */
extern "C" UNIV_INTERN
const char*
innobase_get_stmt(
/*==============*/
	void*	mysql_thd,	/*!< in: MySQL thread handle */
	size_t*	length)		/*!< out: length of the SQL statement */
{
	LEX_STRING* stmt;

	stmt = thd_query_string((THD*) mysql_thd);
	*length = stmt->length;
	return(stmt->str);
}

/**********************************************************************//**
Get the current setting of the lower_case_table_names global parameter from
mysqld.cc. We do a dirty read because for one there is no synchronization
object and secondly there is little harm in doing so even if we get a torn
read.
@return	value of lower_case_table_names */
extern "C" UNIV_INTERN
ulint
innobase_get_lower_case_table_names(void)
/*=====================================*/
{
	return(lower_case_table_names);
}

/*********************************************************************//**
Creates a temporary file.
@return	temporary file descriptor, or < 0 on error */
extern "C" UNIV_INTERN
int
innobase_mysql_tmpfile(void)
/*========================*/
{
	int	fd2 = -1;
	File	fd;

	DBUG_EXECUTE_IF(
		"innobase_tmpfile_creation_failure",
		return(-1);
	);

	fd = mysql_tmpfile("ib");

	if (fd >= 0) {
		/* Copy the file descriptor, so that the additional resources
		allocated by create_temp_file() can be freed by invoking
		my_close().

		Because the file descriptor returned by this function
		will be passed to fdopen(), it will be closed by invoking
		fclose(), which in turn will invoke close() instead of
		my_close(). */

#ifdef _WIN32
		/* Note that on Windows, the integer returned by mysql_tmpfile
		has no relation to C runtime file descriptor. Here, we need
		to call my_get_osfhandle to get the HANDLE and then convert it 
		to C runtime filedescriptor. */
		{
			HANDLE hFile = my_get_osfhandle(fd);
			HANDLE hDup;
			BOOL bOK = 
				DuplicateHandle(GetCurrentProcess(), hFile, GetCurrentProcess(),
								&hDup, 0, FALSE, DUPLICATE_SAME_ACCESS);
			if(bOK) {
				fd2 = _open_osfhandle((intptr_t)hDup,0);
			}
			else {
				my_osmaperr(GetLastError());
				fd2 = -1;
			}	
		}
#else
		fd2 = dup(fd);
#endif
		if (fd2 < 0) {
			DBUG_PRINT("error",("Got error %d on dup",fd2));
			my_errno=errno;
			my_error(EE_OUT_OF_FILERESOURCES,
				 MYF(ME_BELL+ME_WAITTANG),
				 "ib*", my_errno);
		}
		my_close(fd, MYF(MY_WME));
	}
	return(fd2);
}

/*********************************************************************//**
Wrapper around MySQL's copy_and_convert function.
@return	number of bytes copied to 'to' */
extern "C" UNIV_INTERN
ulint
innobase_convert_string(
/*====================*/
	void*		to,		/*!< out: converted string */
	ulint		to_length,	/*!< in: number of bytes reserved
					for the converted string */
	CHARSET_INFO*	to_cs,		/*!< in: character set to convert to */
	const void*	from,		/*!< in: string to convert */
	ulint		from_length,	/*!< in: number of bytes to convert */
	CHARSET_INFO*	from_cs,	/*!< in: character set to convert from */
	uint*		errors)		/*!< out: number of errors encountered
					during the conversion */
{
  return(copy_and_convert((char*)to, (uint32) to_length, to_cs,
                          (const char*)from, (uint32) from_length, from_cs,
                          errors));
}

/*******************************************************************//**
Formats the raw data in "data" (in InnoDB on-disk format) that is of
type DATA_(CHAR|VARCHAR|MYSQL|VARMYSQL) using "charset_coll" and writes
the result to "buf". The result is converted to "system_charset_info".
Not more than "buf_size" bytes are written to "buf".
The result is always NUL-terminated (provided buf_size > 0) and the
number of bytes that were written to "buf" is returned (including the
terminating NUL).
@return	number of bytes that were written */
extern "C" UNIV_INTERN
ulint
innobase_raw_format(
/*================*/
	const char*	data,		/*!< in: raw data */
	ulint		data_len,	/*!< in: raw data length
					in bytes */
	ulint		charset_coll,	/*!< in: charset collation */
	char*		buf,		/*!< out: output buffer */
	ulint		buf_size)	/*!< in: output buffer size
					in bytes */
{
	/* XXX we use a hard limit instead of allocating
	but_size bytes from the heap */
	CHARSET_INFO*	data_cs;
	char		buf_tmp[8192];
	ulint		buf_tmp_used;
	uint		num_errors;

	data_cs = all_charsets[charset_coll];

	buf_tmp_used = innobase_convert_string(buf_tmp, sizeof(buf_tmp),
					       system_charset_info,
					       data, data_len, data_cs,
					       &num_errors);

	return(ut_str_sql_format(buf_tmp, buf_tmp_used, buf, buf_size));
}

/*********************************************************************//**
Compute the next autoinc value.

For MySQL replication the autoincrement values can be partitioned among
the nodes. The offset is the start or origin of the autoincrement value
for a particular node. For n nodes the increment will be n and the offset
will be in the interval [1, n]. The formula tries to allocate the next
value for a particular node.

Note: This function is also called with increment set to the number of
values we want to reserve for multi-value inserts e.g.,

	INSERT INTO T VALUES(), (), ();

innobase_next_autoinc() will be called with increment set to 3 where
autoinc_lock_mode != TRADITIONAL because we want to reserve 3 values for
the multi-value INSERT above.
@return	the next value */
static
ulonglong
innobase_next_autoinc(
/*==================*/
	ulonglong	current,	/*!< in: Current value */
	ulonglong	need,		/*!< in: count of values needed */
	ulonglong	step,		/*!< in: AUTOINC increment step */
	ulonglong	offset,		/*!< in: AUTOINC offset */
	ulonglong	max_value)	/*!< in: max value for type */
{
	ulonglong	next_value;
	ulonglong	block = need * step;

	/* Should never be 0. */
	ut_a(need > 0);
	ut_a(block > 0);
	ut_a(max_value > 0);

        /*
          Allow auto_increment to go over max_value up to max ulonglong.
          This allows us to detect that all values are exhausted.
          If we don't do this, we will return max_value several times
          and get duplicate key errors instead of auto increment value
          out of range.
        */
        max_value= (~(ulonglong) 0);

	/* Current value should never be greater than the maximum. */
	ut_a(current <= max_value);

	/* According to MySQL documentation, if the offset is greater than
	the step then the offset is ignored. */
	if (offset > block) {
		offset = 0;
	}

	/* Check for overflow. Current can be > max_value if the value is
	in reality a negative value.The visual studio compilers converts
	large double values automatically into unsigned long long datatype
	maximum value */
	if (block >= max_value
	    || offset > max_value
	    || current >= max_value
	    || max_value - offset <= offset) {

		next_value = max_value;
	} else {
		ut_a(max_value > current);

		ulonglong	free = max_value - current;

		if (free < offset || free - offset <= block) {
			next_value = max_value;
		} else {
			next_value = 0;
		}
	}

	if (next_value == 0) {
		ulonglong	next;

		if (current >= offset) {
			next = (current - offset) / step;
		} else {
			next = 0;
			block -= step;
		}

		ut_a(max_value > next);
		next_value = next * step;
		/* Check for multiplication overflow. */
		ut_a(next_value >= next);
		ut_a(max_value > next_value);

		/* Check for overflow */
		if (max_value - next_value >= block) {

			next_value += block;

			if (max_value - next_value >= offset) {
				next_value += offset;
			} else {
				next_value = max_value;
			}
		} else {
			next_value = max_value;
		}
	}

	ut_a(next_value != 0);
	ut_a(next_value <= max_value);

	return(next_value);
}

/*********************************************************************//**
Initializes some fields in an InnoDB transaction object. */
static
void
innobase_trx_init(
/*==============*/
	THD*	thd,	/*!< in: user thread handle */
	trx_t*	trx)	/*!< in/out: InnoDB transaction handle */
{
	DBUG_ENTER("innobase_trx_init");
	DBUG_ASSERT(thd == trx->mysql_thd);

	trx->check_foreigns = !thd_test_options(
		thd, OPTION_NO_FOREIGN_KEY_CHECKS);

	trx->check_unique_secondary = !thd_test_options(
		thd, OPTION_RELAXED_UNIQUE_CHECKS);

	/* Transaction on start caches the fake_changes state and uses it for
	complete transaction lifetime.
	There are some APIs that doesn't need an active transaction object
	but transaction object are just use as a cache object/data carrier.
	Before using transaction object for such APIs refresh the state of
	fake_changes. */
	if (trx->state == TRX_NOT_STARTED) {
		trx->fake_changes = thd_fake_changes(thd);
	}

#ifdef EXTENDED_SLOWLOG
	if (thd_log_slow_verbosity(thd) & (1ULL << SLOG_V_INNODB)) {
		trx->take_stats = TRUE;
	} else {
		trx->take_stats = FALSE;
	}
#else
	trx->take_stats = FALSE;
#endif

	DBUG_VOID_RETURN;
}

/*********************************************************************//**
Allocates an InnoDB transaction for a MySQL handler object.
@return	InnoDB transaction handle */
extern "C" UNIV_INTERN
trx_t*
innobase_trx_allocate(
/*==================*/
	THD*	thd)	/*!< in: user thread handle */
{
	trx_t*	trx;

	DBUG_ENTER("innobase_trx_allocate");
	DBUG_ASSERT(thd != NULL);
	DBUG_ASSERT(EQ_CURRENT_THD(thd));

	trx = trx_allocate_for_mysql();

	trx->mysql_thd = thd;

	innobase_trx_init(thd, trx);

	DBUG_RETURN(trx);
}

/*********************************************************************//**
Gets the InnoDB transaction handle for a MySQL handler object, creates
an InnoDB transaction struct if the corresponding MySQL thread struct still
lacks one.
@return	InnoDB transaction handle */
static inline
trx_t*
check_trx_exists(
/*=============*/
	THD*	thd)	/*!< in: user thread handle */
{
	trx_t*&	trx = thd_to_trx(thd);

	if (trx == NULL) {
		trx = innobase_trx_allocate(thd);
		thd_set_ha_data(thd, innodb_hton_ptr, trx);
	} else if (UNIV_UNLIKELY(trx->magic_n != TRX_MAGIC_N)) {
		mem_analyze_corruption(trx);
		ut_error;
	}

	innobase_trx_init(thd, trx);

	return(trx);
}

/*************************************************************************
Gets current trx. */
extern "C"
trx_t*
innobase_get_trx()
{
	THD *thd=current_thd;
	if (likely(thd != 0)) {
		trx_t*& trx = thd_to_trx(thd);
		return(trx);
	} else {
		return(NULL);
	}
}

extern "C"
ibool
innobase_get_slow_log()
{
#ifdef EXTENDED_SLOWLOG
	return((ibool) thd_opt_slow_log());
#else
	return(FALSE);
#endif
}

/*********************************************************************//**
Note that a transaction has been registered with MySQL.
@return true if transaction is registered with MySQL 2PC coordinator */
static inline
bool
trx_is_registered_for_2pc(
/*=========================*/
	const trx_t*	trx)	/* in: transaction */
{
	return(trx->is_registered == 1);
}

/*********************************************************************//**
Note that innobase_commit_ordered() was run. */
static inline
void
trx_set_active_commit_ordered(
/*==============================*/
	trx_t*	trx)	/* in: transaction */
{
	ut_a(trx_is_registered_for_2pc(trx));
	trx->active_commit_ordered = 1;
}

/*********************************************************************//**
Note that a transaction has been registered with MySQL 2PC coordinator. */
static inline
void
trx_register_for_2pc(
/*==================*/
	trx_t*	trx)	/* in: transaction */
{
	trx->is_registered = 1;
	ut_ad(trx->active_commit_ordered == 0);
}

/*********************************************************************//**
Note that a transaction has been deregistered. */
static inline
void
trx_deregister_from_2pc(
/*====================*/
	trx_t*	trx)	/* in: transaction */
{
	trx->is_registered = 0;
	trx->active_commit_ordered = 0;
}

/*********************************************************************//**
Check whether a transaction has active_commit_ordered set */
static inline
bool
trx_is_active_commit_ordered(
/*=========================*/
	const trx_t*	trx)	/* in: transaction */
{
	return(trx->active_commit_ordered == 1);
}

/*********************************************************************//**
Check if transaction is started.
@reutrn true if transaction is in state started */
static
bool
trx_is_started(
/*===========*/
	trx_t*	trx)	/* in: transaction */
{
	return(trx->state != TRX_NOT_STARTED);
}

/*********************************************************************//**
Construct ha_innobase handler. */
UNIV_INTERN
ha_innobase::ha_innobase(handlerton *hton, TABLE_SHARE *table_arg)
  :handler(hton, table_arg),
  int_table_flags(HA_REC_NOT_IN_SEQ |
		  HA_NULL_IN_KEY | HA_CAN_VIRTUAL_COLUMNS |
		  HA_CAN_INDEX_BLOBS |
		  HA_CAN_SQL_HANDLER |
		  HA_PRIMARY_KEY_REQUIRED_FOR_POSITION |
		  HA_PRIMARY_KEY_IN_READ_INDEX |
		  HA_BINLOG_ROW_CAPABLE |
		  HA_CAN_GEOMETRY | HA_PARTIAL_COLUMN_READ |
		  HA_TABLE_SCAN_ON_INDEX),
  start_of_scan(0),
  num_write_row(0)
{}

/*********************************************************************//**
Destruct ha_innobase handler. */
UNIV_INTERN
ha_innobase::~ha_innobase()
{
}

/*********************************************************************//**
Updates the user_thd field in a handle and also allocates a new InnoDB
transaction handle if needed, and updates the transaction fields in the
prebuilt struct. */
UNIV_INTERN inline
void
ha_innobase::update_thd(
/*====================*/
	THD*	thd)	/*!< in: thd to use the handle */
{
	trx_t*		trx;

	trx = check_trx_exists(thd);

	if (prebuilt->trx != trx) {

		row_update_prebuilt_trx(prebuilt, trx);
	}

	user_thd = thd;
}

/*********************************************************************//**
Updates the user_thd field in a handle and also allocates a new InnoDB
transaction handle if needed, and updates the transaction fields in the
prebuilt struct. */
UNIV_INTERN
void
ha_innobase::update_thd()
/*=====================*/
{
	THD*	thd = ha_thd();
	ut_ad(EQ_CURRENT_THD(thd));
	update_thd(thd);
}

/*********************************************************************//**
Registers an InnoDB transaction with the MySQL 2PC coordinator, so that
the MySQL XA code knows to call the InnoDB prepare and commit, or rollback
for the transaction. This MUST be called for every transaction for which
the user may call commit or rollback. Calling this several times to register
the same transaction is allowed, too. This function also registers the
current SQL statement. */
static inline
void
innobase_register_trx(
/*==================*/
	handlerton*	hton,	/* in: Innobase handlerton */
	THD*		thd,	/* in: MySQL thd (connection) object */
	trx_t*		trx)	/* in: transaction to register */
{
	trans_register_ha(thd, FALSE, hton);

	if (!trx_is_registered_for_2pc(trx)
	    && thd_test_options(thd, OPTION_NOT_AUTOCOMMIT | OPTION_BEGIN)) {

		trans_register_ha(thd, TRUE, hton);
	}

	trx_register_for_2pc(trx);
}
  
/*   BACKGROUND INFO: HOW THE MYSQL QUERY CACHE WORKS WITH INNODB
     ------------------------------------------------------------

1) The use of the query cache for TBL is disabled when there is an
uncommitted change to TBL.

2) When a change to TBL commits, InnoDB stores the current value of
its global trx id counter, let us denote it by INV_TRX_ID, to the table object
in the InnoDB data dictionary, and does only allow such transactions whose
id <= INV_TRX_ID to use the query cache.

3) When InnoDB does an INSERT/DELETE/UPDATE to a table TBL, or an implicit
modification because an ON DELETE CASCADE, we invalidate the MySQL query cache
of TBL immediately.

How this is implemented inside InnoDB:

1) Since every modification always sets an IX type table lock on the InnoDB
table, it is easy to check if there can be uncommitted modifications for a
table: just check if there are locks in the lock list of the table.

2) When a transaction inside InnoDB commits, it reads the global trx id
counter and stores the value INV_TRX_ID to the tables on which it had a lock.

3) If there is an implicit table change from ON DELETE CASCADE or SET NULL,
InnoDB calls an invalidate method for the MySQL query cache for that table.

How this is implemented inside sql_cache.cc:

1) The query cache for an InnoDB table TBL is invalidated immediately at an
INSERT/UPDATE/DELETE, just like in the case of MyISAM. No need to delay
invalidation to the transaction commit.

2) To store or retrieve a value from the query cache of an InnoDB table TBL,
any query must first ask InnoDB's permission. We must pass the thd as a
parameter because InnoDB will look at the trx id, if any, associated with
that thd.

3) Use of the query cache for InnoDB tables is now allowed also when
AUTOCOMMIT==0 or we are inside BEGIN ... COMMIT. Thus transactions no longer
put restrictions on the use of the query cache.
*/

/******************************************************************//**
The MySQL query cache uses this to check from InnoDB if the query cache at
the moment is allowed to operate on an InnoDB table. The SQL query must
be a non-locking SELECT.

The query cache is allowed to operate on certain query only if this function
returns TRUE for all tables in the query.

If thd is not in the autocommit state, this function also starts a new
transaction for thd if there is no active trx yet, and assigns a consistent
read view to it if there is no read view yet.

Why a deadlock of threads is not possible: the query cache calls this function
at the start of a SELECT processing. Then the calling thread cannot be
holding any InnoDB semaphores. The calling thread is holding the
query cache mutex, and this function will reserver the InnoDB kernel mutex.
Thus, the 'rank' in sync0sync.h of the MySQL query cache mutex is above
the InnoDB kernel mutex.
@return TRUE if permitted, FALSE if not; note that the value FALSE
does not mean we should invalidate the query cache: invalidation is
called explicitly */
static
my_bool
innobase_query_caching_of_table_permitted(
/*======================================*/
	THD*	thd,		/*!< in: thd of the user who is trying to
				store a result to the query cache or
				retrieve it */
	char*	full_name,	/*!< in: concatenation of database name,
				the null character NUL, and the table
				name */
	uint	full_name_len,	/*!< in: length of the full name, i.e.
				len(dbname) + len(tablename) + 1 */
	ulonglong *unused)	/*!< unused for this engine */
{
	ibool	is_autocommit;
	trx_t*	trx;
	char	norm_name[1000];

	ut_a(full_name_len < 999);

	trx = check_trx_exists(thd);

	if (trx->isolation_level == TRX_ISO_SERIALIZABLE) {
		/* In the SERIALIZABLE mode we add LOCK IN SHARE MODE to every
		plain SELECT if AUTOCOMMIT is not on. */

		return((my_bool)FALSE);
	}

	if (trx->has_search_latch) {
		sql_print_error("The calling thread is holding the adaptive "
				"search, latch though calling "
				"innobase_query_caching_of_table_permitted.");

		mutex_enter(&kernel_mutex);
		trx_print(stderr, trx, 1024);
		mutex_exit(&kernel_mutex);
	}

	trx_search_latch_release_if_reserved(trx);
	innodb_srv_conc_force_exit_innodb(trx);

	if (!thd_test_options(thd, OPTION_NOT_AUTOCOMMIT | OPTION_BEGIN)) {

		is_autocommit = TRUE;
	} else {
		is_autocommit = FALSE;

	}

	if (is_autocommit && trx->n_mysql_tables_in_use == 0) {
		/* We are going to retrieve the query result from the query
		cache. This cannot be a store operation to the query cache
		because then MySQL would have locks on tables already.

		TODO: if the user has used LOCK TABLES to lock the table,
		then we open a transaction in the call of row_.. below.
		That trx can stay open until UNLOCK TABLES. The same problem
		exists even if we do not use the query cache. MySQL should be
		modified so that it ALWAYS calls some cleanup function when
		the processing of a query ends!

		We can imagine we instantaneously serialize this consistent
		read trx to the current trx id counter. If trx2 would have
		changed the tables of a query result stored in the cache, and
		trx2 would have already committed, making the result obsolete,
		then trx2 would have already invalidated the cache. Thus we
		can trust the result in the cache is ok for this query. */

		return((my_bool)TRUE);
	}

	/* Normalize the table name to InnoDB format */

	memcpy(norm_name, full_name, full_name_len);

	norm_name[strlen(norm_name)] = '/'; /* InnoDB uses '/' as the
					    separator between db and table */
	norm_name[full_name_len] = '\0';
#ifdef __WIN__
	innobase_casedn_str(norm_name);
#endif

	innobase_register_trx(innodb_hton_ptr, thd, trx);

	if (row_search_check_if_query_cache_permitted(trx, norm_name)) {

		/* printf("Query cache for %s permitted\n", norm_name); */

		return((my_bool)TRUE);
	}

	/* printf("Query cache for %s NOT permitted\n", norm_name); */

	return((my_bool)FALSE);
}

/*****************************************************************//**
Invalidates the MySQL query cache for the table. */
extern "C" UNIV_INTERN
void
innobase_invalidate_query_cache(
/*============================*/
	trx_t*		trx,		/*!< in: transaction which
					modifies the table */
	const char*	full_name,	/*!< in: concatenation of
					database name, null char NUL,
					table name, null char NUL;
					NOTE that in Windows this is
					always in LOWER CASE! */
	ulint		full_name_len)	/*!< in: full name length where
					also the null chars count */
{
	/* Note that the sync0sync.h rank of the query cache mutex is just
	above the InnoDB kernel mutex. The caller of this function must not
	have latches of a lower rank. */

	/* Argument TRUE below means we are using transactions */
#ifdef HAVE_QUERY_CACHE
	mysql_query_cache_invalidate4((THD*) trx->mysql_thd,
				      full_name,
				      (uint32) full_name_len,
				      TRUE);
#endif
}

/*****************************************************************//**
Convert an SQL identifier to the MySQL system_charset_info (UTF-8)
and quote it if needed.
@return	pointer to the end of buf */
static
char*
innobase_convert_identifier(
/*========================*/
	char*		buf,	/*!< out: buffer for converted identifier */
	ulint		buflen,	/*!< in: length of buf, in bytes */
	const char*	id,	/*!< in: identifier to convert */
	ulint		idlen,	/*!< in: length of id, in bytes */
	void*		thd,	/*!< in: MySQL connection thread, or NULL */
	ibool		file_id)/*!< in: TRUE=id is a table or database name;
				FALSE=id is an UTF-8 string */
{
	char nz[NAME_LEN + 1];
	char nz2[NAME_LEN + 1 + EXPLAIN_FILENAME_MAX_EXTRA_LENGTH];

	const char*	s	= id;
	int		q;

	if (file_id) {
		/* Decode the table name.  The MySQL function expects
		a NUL-terminated string.  The input and output strings
		buffers must not be shared. */

		if (UNIV_UNLIKELY(idlen > (sizeof nz) - 1)) {
			idlen = (sizeof nz) - 1;
		}

		memcpy(nz, id, idlen);
		nz[idlen] = 0;

		s = nz2;
		idlen = explain_filename((THD*) thd, nz, nz2, sizeof nz2,
					 EXPLAIN_PARTITIONS_AS_COMMENT);
		goto no_quote;
	}

	/* See if the identifier needs to be quoted. */
	if (UNIV_UNLIKELY(!thd)) {
		q = '"';
	} else {
		q = get_quote_char_for_identifier((THD*) thd, s, (int) idlen);
	}

	if (q == EOF) {
no_quote:
		if (UNIV_UNLIKELY(idlen > buflen)) {
			idlen = buflen;
		}
		memcpy(buf, s, idlen);
		return(buf + idlen);
	}

	/* Quote the identifier. */
	if (buflen < 2) {
		return(buf);
	}

	*buf++ = q;
	buflen--;

	for (; idlen; idlen--) {
		int	c = *s++;
		if (UNIV_UNLIKELY(c == q)) {
			if (UNIV_UNLIKELY(buflen < 3)) {
				break;
			}

			*buf++ = c;
			*buf++ = c;
			buflen -= 2;
		} else {
			if (UNIV_UNLIKELY(buflen < 2)) {
				break;
			}

			*buf++ = c;
			buflen--;
		}
	}

	*buf++ = q;
	return(buf);
}

/*****************************************************************//**
Convert a table or index name to the MySQL system_charset_info (UTF-8)
and quote it if needed.
@return	pointer to the end of buf */
extern "C" UNIV_INTERN
char*
innobase_convert_name(
/*==================*/
	char*		buf,	/*!< out: buffer for converted identifier */
	ulint		buflen,	/*!< in: length of buf, in bytes */
	const char*	id,	/*!< in: identifier to convert */
	ulint		idlen,	/*!< in: length of id, in bytes */
	void*		thd,	/*!< in: MySQL connection thread, or NULL */
	ibool		table_id)/*!< in: TRUE=id is a table or database name;
				FALSE=id is an index name */
{
	char*		s	= buf;
	const char*	bufend	= buf + buflen;

	if (table_id) {
		const char*	slash = (const char*) memchr(id, '/', idlen);
		if (!slash) {

			goto no_db_name;
		}

		/* Print the database name and table name separately. */
		s = innobase_convert_identifier(s, bufend - s, id, slash - id,
						thd, TRUE);
		if (UNIV_LIKELY(s < bufend)) {
			*s++ = '.';
			s = innobase_convert_identifier(s, bufend - s,
							slash + 1, idlen
							- (slash - id) - 1,
							thd, TRUE);
		}
	} else if (UNIV_UNLIKELY(*id == TEMP_INDEX_PREFIX)) {
		/* Temporary index name (smart ALTER TABLE) */
		const char temp_index_suffix[]= "--temporary--";

		s = innobase_convert_identifier(buf, buflen, id + 1, idlen - 1,
						thd, FALSE);
		if (s - buf + (sizeof temp_index_suffix - 1) < buflen) {
			memcpy(s, temp_index_suffix,
			       sizeof temp_index_suffix - 1);
			s += sizeof temp_index_suffix - 1;
		}
	} else {
no_db_name:
		s = innobase_convert_identifier(buf, buflen, id, idlen,
						thd, table_id);
	}

	return(s);

}

/*****************************************************************//**
A wrapper function of innobase_convert_name(), convert a table or
index name to the MySQL system_charset_info (UTF-8) and quote it if needed.
@return	pointer to the end of buf */
void
innobase_format_name(
/*==================*/
	char*		buf,	/*!< out: buffer for converted identifier */
	ulint		buflen,	/*!< in: length of buf, in bytes */
	const char*	name,	/*!< in: index or table name to format */
	ibool		is_index_name) /*!< in: index name */
{
	const char*     bufend;

	bufend = innobase_convert_name(buf, buflen, name, strlen(name),
				       NULL, !is_index_name);

	ut_ad((ulint) (bufend - buf) < buflen);

	buf[bufend - buf] = '\0';
}

/**********************************************************************//**
Determines if the currently running transaction has been interrupted.
@return	TRUE if interrupted */
extern "C" UNIV_INTERN
ibool
trx_is_interrupted(
/*===============*/
	trx_t*	trx)	/*!< in: transaction */
{
	return(trx && trx->mysql_thd && thd_kill_level((THD*) trx->mysql_thd));
}

/**********************************************************************//**
Determines if the currently running transaction is in strict mode.
@return	TRUE if strict */
extern "C" UNIV_INTERN
ibool
trx_is_strict(
/*==========*/
	trx_t*	trx)	/*!< in: transaction */
{
	return(trx && trx->mysql_thd
	       && THDVAR((THD*) trx->mysql_thd, strict_mode));
}

/**************************************************************//**
Resets some fields of a prebuilt struct. The template is used in fast
retrieval of just those column values MySQL needs in its processing. */
void
inline
ha_innobase::reset_template(void)
/*=============================*/
{
	ut_ad(prebuilt->magic_n == ROW_PREBUILT_ALLOCATED);
	ut_ad(prebuilt->magic_n2 == prebuilt->magic_n);

	prebuilt->keep_other_fields_on_keyread = 0;
	prebuilt->read_just_key = 0;
        /* Reset index condition pushdown state. */
        if (prebuilt->idx_cond) {
                prebuilt->idx_cond = NULL;
                prebuilt->idx_cond_n_cols = 0;
                /* Invalidate prebuilt->mysql_template
                in ha_innobase::write_row(). */
                prebuilt->template_type = ROW_MYSQL_NO_TEMPLATE;
        }
}

/*****************************************************************//**
Call this when you have opened a new table handle in HANDLER, before you
call index_read_idx() etc. Actually, we can let the cursor stay open even
over a transaction commit! Then you should call this before every operation,
fetch next etc. This function inits the necessary things even after a
transaction commit. */
UNIV_INTERN
void
ha_innobase::init_table_handle_for_HANDLER(void)
/*============================================*/
{
	/* If current thd does not yet have a trx struct, create one.
	If the current handle does not yet have a prebuilt struct, create
	one. Update the trx pointers in the prebuilt struct. Normally
	this operation is done in external_lock. */

	update_thd(ha_thd());

	/* Initialize the prebuilt struct much like it would be inited in
	external_lock */

	trx_search_latch_release_if_reserved(prebuilt->trx);
	innodb_srv_conc_force_exit_innodb(prebuilt->trx);

	/* If the transaction is not started yet, start it */

	trx_start_if_not_started(prebuilt->trx);

	/* Assign a read view if the transaction does not have it yet */

	trx_assign_read_view(prebuilt->trx);

	innobase_register_trx(ht, user_thd, prebuilt->trx);

	/* We did the necessary inits in this function, no need to repeat them
	in row_search_for_mysql */

	prebuilt->sql_stat_start = FALSE;

	/* We let HANDLER always to do the reads as consistent reads, even
	if the trx isolation level would have been specified as SERIALIZABLE */

	prebuilt->select_lock_type = LOCK_NONE;
	prebuilt->stored_select_lock_type = LOCK_NONE;

	/* Always fetch all columns in the index record */

	prebuilt->hint_need_to_fetch_extra_cols = ROW_RETRIEVE_ALL_COLS;

	/* We want always to fetch all columns in the whole row? Or do
	we???? */

	prebuilt->used_in_HANDLER = TRUE;
	reset_template();
}

#ifdef HAVE_REPLICATION
/* The last read master log coordinates in the slave info file */
static char	master_log_fname[TRX_SYS_MYSQL_MASTER_LOG_NAME_LEN] = "";
static int	master_log_pos;
/* The slave relay log coordinates in the slave info file after startup */
static char	original_relay_log_fname[TRX_SYS_MYSQL_MASTER_LOG_NAME_LEN] = "";
static int	original_relay_log_pos;
/* The master log coordinates in the slave info file after startup */
static char	original_master_log_fname[TRX_SYS_MYSQL_MASTER_LOG_NAME_LEN] = "";
static int	original_master_log_pos;
#endif

/*****************************************************************//**
Overwrites the MySQL relay log info file with the current master and relay log
coordinates from InnoDB.  Skips overwrite if the master log position did not
change from the last overwrite.  If the InnoDB master log position is equal
to position that was read from the info file on startup before any overwrites,
restores the original positions. */
static
void
innobase_do_overwrite_relay_log_info(void)
/*======================================*/
{
#ifdef HAVE_REPLICATION
	char	info_fname[FN_REFLEN];
	File	info_fd = -1;
	int	error	= 0;
	char	buff[FN_REFLEN*2+22*2+4];
	char	*relay_info_log_pos;
	size_t	buf_len;

	if (master_log_fname[0] == '\0') {
		fprintf(stderr,
			"InnoDB: something wrong with relay-log.info. "
			"InnoDB will not overwrite it.\n");
		return;
	}

	if (strcmp(master_log_fname, trx_sys_mysql_master_log_name) == 0
	    && master_log_pos == trx_sys_mysql_master_log_pos) {
		fprintf(stderr,
			"InnoDB: InnoDB and relay-log.info are synchronized. "
			"InnoDB will not overwrite it.\n");
		return;
	}

	/* If we overwrite the file back to the original master log position,
	restore the original relay log position too.  This is required because
	we might have rolled back a prepared transaction and restored the
	original master log position from the InnoDB trx sys header, but the
	corresponding relay log position points to an already-purged file. */
	if (strcmp(original_master_log_fname, trx_sys_mysql_master_log_name)
	    == 0
	    && (original_master_log_pos	== trx_sys_mysql_master_log_pos)) {

		strncpy(trx_sys_mysql_relay_log_name, original_relay_log_fname,
			TRX_SYS_MYSQL_MASTER_LOG_NAME_LEN);
		trx_sys_mysql_relay_log_pos = original_relay_log_pos;
	}

	fn_format(info_fname, relay_log_info_file, mysql_data_home, "",
		  MY_UNPACK_FILENAME | MY_RETURN_REAL_PATH);

	if (access(info_fname, F_OK)) {
		/* File does not exist */
		error = 1;
		goto skip_overwrite;
	}

	/* File exists */
	info_fd = my_open(info_fname, O_RDWR|O_BINARY, MYF(MY_WME));
	if (info_fd < 0) {
		error = 1;
		goto skip_overwrite;
	}

	relay_info_log_pos = strmov(buff, trx_sys_mysql_relay_log_name);
	*relay_info_log_pos ++= '\n';
	relay_info_log_pos = longlong2str(trx_sys_mysql_relay_log_pos,
					  relay_info_log_pos, 10);
	*relay_info_log_pos ++= '\n';
	relay_info_log_pos = strmov(relay_info_log_pos,
				    trx_sys_mysql_master_log_name);
	*relay_info_log_pos ++= '\n';
	relay_info_log_pos = longlong2str(trx_sys_mysql_master_log_pos,
					  relay_info_log_pos, 10);
	*relay_info_log_pos = '\n';

	buf_len = (relay_info_log_pos - buff) + 1;
	if (my_write(info_fd, (uchar *)buff, buf_len, MY_WME) != buf_len) {
		error = 1;
	} else if (my_sync(info_fd, MY_WME)) {
		error = 1;
	}

	if (info_fd >= 0) {
		my_close(info_fd, MYF(0));
	}

	strncpy(master_log_fname, trx_sys_mysql_relay_log_name,
		TRX_SYS_MYSQL_MASTER_LOG_NAME_LEN);
	master_log_pos = trx_sys_mysql_master_log_pos;

skip_overwrite:
	if (error) {
		fprintf(stderr,
			"InnoDB: ERROR: error occured during overwriting "
			"relay-log.info.\n");
	} else {
		fprintf(stderr,
			"InnoDB: relay-log.info was overwritten.\n");
	}
#endif
}


/*********************************************************************//**
Opens an InnoDB database.
@return	0 on success, error code on failure */
static
int
innobase_init(
/*==========*/
	void	*p)	/*!< in: InnoDB handlerton */
{
	static char	current_dir[3];		/*!< Set if using current lib */
	int		err;
	bool		ret;
	char		*default_path;
	uint		format_id;

	DBUG_ENTER("innobase_init");
        handlerton *innobase_hton= (handlerton *)p;
        innodb_hton_ptr = innobase_hton;

        innobase_hton->state = SHOW_OPTION_YES;
        innobase_hton->db_type= DB_TYPE_INNODB;
        innobase_hton->savepoint_offset=sizeof(trx_named_savept_t);
        innobase_hton->close_connection=innobase_close_connection;
        innobase_hton->savepoint_set=innobase_savepoint;
        innobase_hton->savepoint_rollback=innobase_rollback_to_savepoint;
        innobase_hton->savepoint_release=innobase_release_savepoint;
        innobase_hton->commit_ordered=innobase_commit_ordered;
        innobase_hton->commit=innobase_commit;
        innobase_hton->rollback=innobase_rollback;
        innobase_hton->prepare=innobase_xa_prepare;
        innobase_hton->recover=innobase_xa_recover;
        innobase_hton->commit_by_xid=innobase_commit_by_xid;
        innobase_hton->rollback_by_xid=innobase_rollback_by_xid;
        innobase_hton->checkpoint_state= innobase_checkpoint_state;
        innobase_hton->create_cursor_read_view=innobase_create_cursor_view;
        innobase_hton->set_cursor_read_view=innobase_set_cursor_view;
        innobase_hton->close_cursor_read_view=innobase_close_cursor_view;
        innobase_hton->create=innobase_create_handler;
        innobase_hton->drop_database=innobase_drop_database;
        innobase_hton->panic=innobase_end;
        innobase_hton->start_consistent_snapshot=innobase_start_trx_and_assign_read_view;
        innobase_hton->flush_logs=innobase_flush_logs;
        innobase_hton->show_status=innobase_show_status;
        innobase_hton->flags=HTON_EXTENDED_KEYS;
        innobase_hton->release_temporary_latches=innobase_release_temporary_latches;
	innobase_hton->alter_table_flags = innobase_alter_table_flags;
        innobase_hton->kill_query = innobase_kill_query;

	ut_a(DATA_MYSQL_TRUE_VARCHAR == (ulint)MYSQL_TYPE_VARCHAR);

#ifndef DBUG_OFF
	static const char	test_filename[] = "-@";
	char			test_tablename[sizeof test_filename
				+ sizeof srv_mysql50_table_name_prefix];
	if ((sizeof test_tablename) - 1
			!= filename_to_tablename(test_filename, test_tablename,
			sizeof test_tablename, true)
			|| strncmp(test_tablename,
			srv_mysql50_table_name_prefix,
			sizeof srv_mysql50_table_name_prefix)
			|| strcmp(test_tablename
			+ sizeof srv_mysql50_table_name_prefix,
			test_filename)) {
		sql_print_error("tablename encoding has been changed");
		goto error;
	}
#endif /* DBUG_OFF */

	srv_page_size = 0;
	srv_page_size_shift = 0;

	if (innobase_page_size != (1 << 14)) {
		uint n_shift;

		fprintf(stderr,
			"InnoDB: Warning: innodb_page_size has been changed from default value 16384. (###EXPERIMENTAL### operation)\n");
		for (n_shift = 12; n_shift <= UNIV_PAGE_SIZE_SHIFT_MAX; n_shift++) {
			if (innobase_page_size == ((ulong)1 << n_shift)) {
				srv_page_size_shift = n_shift;
				srv_page_size = (1 << srv_page_size_shift);
				fprintf(stderr,
					"InnoDB: The universal page size of the database is set to %lu.\n",
					srv_page_size);
				break;
			}
		}
	} else {
		srv_page_size_shift = 14;
		srv_page_size = (1 << srv_page_size_shift);
	}

	if (!srv_page_size_shift) {
		fprintf(stderr,
			"InnoDB: Error: %lu is not a valid value for innodb_page_size.\n"
			"InnoDB: Error: Valid values are 4096, 8192, and 16384 (default=16384).\n",
			innobase_page_size);
		goto error;
	}

	srv_log_block_size = 0;
	if (innobase_log_block_size != (1 << 9)) { /*!=512*/
		uint	n_shift;

		fprintf(stderr,
			"InnoDB: Warning: innodb_log_block_size has been changed from default value 512. (###EXPERIMENTAL### operation)\n");
		for (n_shift = 9; n_shift <= UNIV_PAGE_SIZE_SHIFT_MAX; n_shift++) {
			if (innobase_log_block_size == ((ulong)1 << n_shift)) {
				srv_log_block_size = (1 << n_shift);
				fprintf(stderr,
					"InnoDB: The log block size is set to %lu.\n",
					srv_log_block_size);
				break;
			}
		}
	} else {
		srv_log_block_size = 512;
	}
	ut_ad (srv_log_block_size >= OS_MIN_LOG_BLOCK_SIZE);

	if (!srv_log_block_size) {
		fprintf(stderr,
			"InnoDB: Error: %lu is not a valid value for innodb_log_block_size.\n"
			"InnoDB: Error: A valid value for innodb_log_block_size is\n"
			"InnoDB: Error: a power of 2 from 512 to 16384.\n",
			innobase_log_block_size);
		goto error;
	}

#ifndef MYSQL_SERVER
	innodb_overwrite_relay_log_info = FALSE;
#endif

#ifdef HAVE_REPLICATION
#ifdef MYSQL_SERVER
	/* read master log position from relay-log.info if exists */
	char info_fname[FN_REFLEN];
	char relay_log_fname[TRX_SYS_MYSQL_MASTER_LOG_NAME_LEN];
	int relay_log_pos;
	int info_fd;
	IO_CACHE info_file;

	info_fname[0] = '\0';

	if(innobase_overwrite_relay_log_info) {

	fprintf(stderr,
		"InnoDB: Warning: innodb_overwrite_relay_log_info is enabled."
		" Updates by other storage engines may not be synchronized.\n");

	bzero((char*) &info_file, sizeof(info_file));
	fn_format(info_fname, relay_log_info_file, mysql_data_home, "", 4+32);

	int error=0;

	if (!access(info_fname,F_OK)) {
		/* exist */
		if ((info_fd = my_open(info_fname, O_RDWR | O_BINARY,
				       MYF(MY_WME))) < 0) {
			error=1;
		} else if (init_io_cache(&info_file, info_fd, IO_SIZE*2,
					READ_CACHE, 0L, 0, MYF(MY_WME))) {
			error=1;
		}

		if (error) {
relay_info_error:
			if (info_fd >= 0)
				my_close(info_fd, MYF(0));
			master_log_fname[0] = '\0';
			goto skip_relay;
		}
	} else {
		master_log_fname[0] = '\0';
		goto skip_relay;
	}

	if (init_strvar_from_file(relay_log_fname, sizeof(relay_log_fname),
				  &info_file, "")
	    || /* dummy (it is relay-log) */ init_intvar_from_file(
		    &relay_log_pos, &info_file, BIN_LOG_HEADER_SIZE)) {
		end_io_cache(&info_file);
		error=1;
		goto relay_info_error;
	}

	fprintf(stderr,
		"InnoDB: relay-log.info is detected.\n"
		"InnoDB: relay log: position %u, file name %s\n",
		relay_log_pos, relay_log_fname);

	strncpy(trx_sys_mysql_relay_log_name, relay_log_fname,
		TRX_SYS_MYSQL_MASTER_LOG_NAME_LEN);
	trx_sys_mysql_relay_log_pos = (ib_int64_t) relay_log_pos;

	strncpy(original_relay_log_fname, relay_log_fname,
		TRX_SYS_MYSQL_MASTER_LOG_NAME_LEN);
	original_relay_log_pos = relay_log_pos;

	if (init_strvar_from_file(master_log_fname, sizeof(master_log_fname),
				  &info_file, "")
	    || init_intvar_from_file(&master_log_pos, &info_file, 0)) {
		end_io_cache(&info_file);
		error=1;
		goto relay_info_error;
	}

	fprintf(stderr,
		"InnoDB: master log: position %u, file name %s\n",
		master_log_pos, master_log_fname);

	strncpy(trx_sys_mysql_master_log_name, master_log_fname,
		TRX_SYS_MYSQL_MASTER_LOG_NAME_LEN);
	trx_sys_mysql_master_log_pos = (ib_int64_t) master_log_pos;

	strncpy(original_master_log_fname, master_log_fname,
		TRX_SYS_MYSQL_MASTER_LOG_NAME_LEN);
	original_master_log_pos = master_log_pos;

	end_io_cache(&info_file);
	if (info_fd >= 0)
		my_close(info_fd, MYF(0));
	}
skip_relay:
#endif /* MYSQL_SERVER */
#endif /* HAVE_REPLICATION */

	/* Check that values don't overflow on 32-bit systems. */
	if (sizeof(ulint) == 4) {
		if (innobase_buffer_pool_size > UINT_MAX32) {
			sql_print_error(
				"innobase_buffer_pool_size can't be over 4GB"
				" on 32-bit systems");

			goto error;
		}

		if (innobase_log_file_size > UINT_MAX32) {
			sql_print_error(
				"innobase_log_file_size can't be over 4GB"
				" on 32-bit systems");

			goto error;
		}
	}

	os_innodb_umask = (ulint)my_umask;

	/* First calculate the default path for innodb_data_home_dir etc.,
	in case the user has not given any value.

	Note that when using the embedded server, the datadirectory is not
	necessarily the current directory of this program. */

	if (mysqld_embedded) {
		default_path = mysql_real_data_home;
		fil_path_to_mysql_datadir = mysql_real_data_home;
	} else {
		/* It's better to use current lib, to keep paths short */
		current_dir[0] = FN_CURLIB;
		current_dir[1] = FN_LIBCHAR;
		current_dir[2] = 0;
		default_path = current_dir;
	}

	ut_a(default_path);

	/* Set InnoDB initialization parameters according to the values
	read from MySQL .cnf file */

	/*--------------- Data files -------------------------*/

	/* The default dir for data files is the datadir of MySQL */

	srv_data_home = (innobase_data_home_dir ? innobase_data_home_dir :
			 default_path);

	/* Set default InnoDB data file size to 10 MB and let it be
	auto-extending. Thus users can use InnoDB in >= 4.0 without having
	to specify any startup options. */

	if (!innobase_data_file_path) {
		innobase_data_file_path = (char*) "ibdata1:10M:autoextend";
	}

	/* Since InnoDB edits the argument in the next call, we make another
	copy of it: */

	internal_innobase_data_file_path = my_strdup(innobase_data_file_path,
						   MYF(MY_FAE));

	ret = (bool) srv_parse_data_file_paths_and_sizes(
		internal_innobase_data_file_path);
	if (ret == FALSE) {
		sql_print_error(
			"InnoDB: syntax error in innodb_data_file_path"
			" or size specified is less than 1 megabyte");
mem_free_and_error:
		srv_free_paths_and_sizes();
		my_free(internal_innobase_data_file_path);
		goto error;
	}

	srv_doublewrite_file = innobase_doublewrite_file;

	srv_use_sys_stats_table = (ibool) innobase_use_sys_stats_table;

#ifdef UNIV_DEBUG
	srv_sys_stats_root_page = innobase_sys_stats_root_page;
#endif

	/* -------------- Log files ---------------------------*/

	/* The default dir for log files is the datadir of MySQL */

	if (!innobase_log_group_home_dir) {
		innobase_log_group_home_dir = default_path;
	}

#ifdef UNIV_LOG_ARCHIVE
	/* Since innodb_log_arch_dir has no relevance under MySQL,
	starting from 4.0.6 we always set it the same as
	innodb_log_group_home_dir: */

	innobase_log_arch_dir = innobase_log_group_home_dir;

	srv_arch_dir = innobase_log_arch_dir;
#endif /* UNIG_LOG_ARCHIVE */

	ret = (bool)
		srv_parse_log_group_home_dirs(innobase_log_group_home_dir);

	if (ret == FALSE || innobase_mirrored_log_groups != 1) {
	  sql_print_error("syntax error in innodb_log_group_home_dir, or a "
			  "wrong number of mirrored log groups");

		goto mem_free_and_error;
	}

	/* Validate the file format by animal name */
	if (innobase_file_format_name != NULL) {

		format_id = innobase_file_format_name_lookup(
			innobase_file_format_name);

		if (format_id > DICT_TF_FORMAT_MAX) {

			sql_print_error("InnoDB: wrong innodb_file_format.");

			goto mem_free_and_error;
		}
	} else {
		/* Set it to the default file format id. Though this
		should never happen. */
		format_id = 0;
	}

	srv_file_format = format_id;

	/* Given the type of innobase_file_format_name we have little
	choice but to cast away the constness from the returned name.
	innobase_file_format_name is used in the MySQL set variable
	interface and so can't be const. */

	innobase_file_format_name =
		(char*) trx_sys_file_format_id_to_name(format_id);

	/* Check innobase_file_format_check variable */
	if (!innobase_file_format_check) {

		/* Set the value to disable checking. */
		srv_max_file_format_at_startup = DICT_TF_FORMAT_MAX + 1;

	} else {

		/* Set the value to the lowest supported format. */
		srv_max_file_format_at_startup = DICT_TF_FORMAT_MIN;
	}

	/* Did the user specify a format name that we support?
	As a side effect it will update the variable
	srv_max_file_format_at_startup */
	if (innobase_file_format_validate_and_set(
			innobase_file_format_max) < 0) {

		sql_print_error("InnoDB: invalid "
				"innodb_file_format_max value: "
				"should be any value up to %s or its "
				"equivalent numeric id",
				trx_sys_file_format_id_to_name(
					DICT_TF_FORMAT_MAX));

		goto mem_free_and_error;
	}

	if (innobase_change_buffering) {
		ulint	use;

		for (use = 0;
		     use < UT_ARR_SIZE(innobase_change_buffering_values);
		     use++) {
			if (!innobase_strcasecmp(
				    innobase_change_buffering,
				    innobase_change_buffering_values[use])) {
				ibuf_use = (ibuf_use_t) use;
				goto innobase_change_buffering_inited_ok;
			}
		}

		sql_print_error("InnoDB: invalid value "
				"innodb_change_buffering=%s",
				innobase_change_buffering);
		goto mem_free_and_error;
	}

innobase_change_buffering_inited_ok:
	ut_a((ulint) ibuf_use < UT_ARR_SIZE(innobase_change_buffering_values));
	innobase_change_buffering = (char*)
		innobase_change_buffering_values[ibuf_use];

	/* --------------------------------------------------*/

	srv_file_flush_method_str = innobase_file_flush_method;

	srv_n_log_groups = (ulint) innobase_mirrored_log_groups;
	srv_n_log_files = (ulint) innobase_log_files_in_group;
	srv_log_file_size = (ulint) innobase_log_file_size;

	srv_thread_concurrency_timer_based =
		(ibool) innobase_thread_concurrency_timer_based;

#ifdef UNIV_LOG_ARCHIVE
	srv_log_archive_on = (ulint) innobase_log_archive;
#endif /* UNIV_LOG_ARCHIVE */
	srv_log_buffer_size = (ulint) innobase_log_buffer_size;

	srv_buf_pool_size = (ulint) innobase_buffer_pool_size;
	srv_buf_pool_instances = (ulint) innobase_buffer_pool_instances;

	if (innobase_buffer_pool_shm_key) {
		fprintf(stderr,
			"InnoDB: Warning: innodb_buffer_pool_shm_key is deprecated function.\n"
			"InnoDB:          innodb_buffer_pool_shm_key was ignored.\n");
	}

	if (srv_lazy_drop_table) {
		fprintf(stderr,
			"InnoDB: Warning: "
			"innodb_lazy_drop_table is deprecated and ignored.\n");
	}

	srv_mem_pool_size = (ulint) innobase_additional_mem_pool_size;

	srv_n_file_io_threads = (ulint) innobase_file_io_threads;
	srv_n_read_io_threads = (ulint) innobase_read_io_threads;
	srv_n_write_io_threads = (ulint) innobase_write_io_threads;

	srv_read_ahead &= 3;
	srv_adaptive_flushing_method %= 3;
	srv_flush_neighbor_pages %= 3;

	srv_force_recovery = (ulint) innobase_force_recovery;

	srv_recovery_stats = (ibool) innobase_recovery_stats;

	srv_use_doublewrite_buf = (ibool) innobase_use_doublewrite;
	srv_use_checksums = (ibool) innobase_use_checksums;
	srv_fast_checksum = (ibool) innobase_fast_checksum;

	if (innobase_fast_checksum) {
		fprintf(stderr,
			"InnoDB: Warning: innodb_fast_checksum is DEPRECATED "
			"and *WILL* be removed in Percona Server 5.6. Please "
			"consult the Percona Server 5.6 documentation for "
			"help in upgrading.\n");
	}

	srv_blocking_lru_restore = (ibool) innobase_blocking_lru_restore;

#ifdef HAVE_LARGE_PAGES
        if ((os_use_large_pages = (ibool) my_use_large_pages))
		os_large_page_size = (ulint) opt_large_page_size;
#endif

	row_rollback_on_timeout = (ibool) innobase_rollback_on_timeout;

	srv_locks_unsafe_for_binlog = (ibool) innobase_locks_unsafe_for_binlog;

	srv_max_n_open_files = (ulint) innobase_open_files;
	srv_innodb_status = (ibool) innobase_create_status_file;

	srv_print_verbose_log = mysqld_embedded ? 0 : 1;

	/* Store the default charset-collation number of this MySQL
	installation */

	data_mysql_default_charset_coll = (ulint)default_charset_info->number;

	ut_a(DATA_MYSQL_LATIN1_SWEDISH_CHARSET_COLL ==
					my_charset_latin1.number);
	ut_a(DATA_MYSQL_BINARY_CHARSET_COLL == my_charset_bin.number);

	/* Store the latin1_swedish_ci character ordering table to InnoDB. For
	non-latin1_swedish_ci charsets we use the MySQL comparison functions,
	and consequently we do not need to know the ordering internally in
	InnoDB. */

	ut_a(0 == strcmp(my_charset_latin1.name, "latin1_swedish_ci"));
	srv_latin1_ordering = my_charset_latin1.sort_order;

	innobase_commit_concurrency_init_default();

#ifndef EXTENDED_FOR_KILLIDLE
	srv_kill_idle_transaction = 0;
#endif

#ifdef HAVE_POSIX_FALLOCATE
	srv_use_posix_fallocate = (ibool) innobase_use_fallocate;
#endif
	srv_use_atomic_writes = (ibool) innobase_use_atomic_writes;
	if (innobase_use_atomic_writes) {
		fprintf(stderr, "InnoDB: using atomic writes.\n");

		/* Force doublewrite buffer off, atomic writes replace it. */
		if (srv_use_doublewrite_buf) {
			fprintf(stderr,
				"InnoDB: Switching off doublewrite buffer "
				"because of atomic writes.\n");
			innobase_use_doublewrite = FALSE;
			srv_use_doublewrite_buf	= FALSE;
		}

		/* Force O_DIRECT on Unixes (on Windows writes are always
		unbuffered)*/
#ifndef _WIN32
		if(!innobase_file_flush_method ||
		   !strstr(innobase_file_flush_method, "O_DIRECT")) {
			innobase_file_flush_method =
				srv_file_flush_method_str = (char*)"O_DIRECT";
			fprintf(stderr,
				"InnoDB: using O_DIRECT due to atomic "
				"writes.\n");
		}
#endif
#ifdef HAVE_POSIX_FALLOCATE
		/* Due to a bug in directFS, using atomics needs
		posix_fallocate() to extend the file, because pwrite() past the
		end of the file won't work */
		srv_use_posix_fallocate = TRUE;
#endif
	}

#ifdef HAVE_PSI_INTERFACE
	/* Register keys with MySQL performance schema */
	if (PSI_server) {
		int	count;

                count = array_elements(all_pthread_mutexes);
                PSI_server->register_mutex("innodb",
                                           all_pthread_mutexes, count);

# ifdef UNIV_PFS_MUTEX
		count = array_elements(all_innodb_mutexes);
		PSI_server->register_mutex("innodb",
					   all_innodb_mutexes, count);
# endif /* UNIV_PFS_MUTEX */

# ifdef UNIV_PFS_RWLOCK
		count = array_elements(all_innodb_rwlocks);
		PSI_server->register_rwlock("innodb",
					    all_innodb_rwlocks, count);
# endif /* UNIV_PFS_MUTEX */

# ifdef UNIV_PFS_THREAD
		count = array_elements(all_innodb_threads);
		PSI_server->register_thread("innodb",
					    all_innodb_threads, count);
# endif /* UNIV_PFS_THREAD */

# ifdef UNIV_PFS_IO
		count = array_elements(all_innodb_files);
		PSI_server->register_file("innodb",
					  all_innodb_files, count);
# endif /* UNIV_PFS_IO */

		count = array_elements(all_innodb_conds);
		PSI_server->register_cond("innodb",
					  all_innodb_conds, count);
	}
#endif /* HAVE_PSI_INTERFACE */

	/* Since we in this module access directly the fields of a trx
	struct, and due to different headers and flags it might happen that
	mutex_t has a different size in this module and in InnoDB
	modules, we check at run time that the size is the same in
	these compilation modules. */

	err = innobase_start_or_create_for_mysql();

	if (err != DB_SUCCESS) {
		goto mem_free_and_error;
	}

	if(innobase_overwrite_relay_log_info) {
		innobase_do_overwrite_relay_log_info();
	}

	innobase_old_blocks_pct = buf_LRU_old_ratio_update(
		innobase_old_blocks_pct, TRUE);

	innobase_open_tables = hash_create(200);
	mysql_mutex_init(innobase_share_mutex_key,
			 &innobase_share_mutex,
			 MY_MUTEX_INIT_FAST);
	mysql_mutex_init(commit_cond_mutex_key,
			 &commit_cond_m, MY_MUTEX_INIT_FAST);
	mysql_cond_init(commit_cond_key, &commit_cond, NULL);
	innodb_inited= 1;
#ifdef MYSQL_DYNAMIC_PLUGIN
	if (innobase_hton != p) {
		innobase_hton = reinterpret_cast<handlerton*>(p);
		*innobase_hton = *innodb_hton_ptr;
	}
#endif /* MYSQL_DYNAMIC_PLUGIN */

	/* Get the current high water mark format. */
	innobase_file_format_max = (char*) trx_sys_file_format_max_get();

	DBUG_RETURN(FALSE);
error:
	DBUG_RETURN(TRUE);
}

/*******************************************************************//**
Closes an InnoDB database.
@return	TRUE if error */
static
int
innobase_end(
/*=========*/
	handlerton*		hton,	/*!< in/out: InnoDB handlerton */
	ha_panic_function	type __attribute__((unused)))
					/*!< in: ha_panic() parameter */
{
	int	err= 0;

	DBUG_ENTER("innobase_end");
	DBUG_ASSERT(hton == innodb_hton_ptr);

	if (innodb_inited) {

		THD *thd= current_thd;
		if (thd) { // may be UNINSTALL PLUGIN statement
		 	trx_t* trx = thd_to_trx(thd);
		 	if (trx) {
		 		trx_free_for_mysql(trx);
		 	}
		}

		srv_fast_shutdown = (ulint) innobase_fast_shutdown;
		innodb_inited = 0;
		hash_table_free(innobase_open_tables);
		innobase_open_tables = NULL;
		if (innobase_shutdown_for_mysql() != DB_SUCCESS) {
			err = 1;
		}
		srv_free_paths_and_sizes();
		my_free(internal_innobase_data_file_path);
		mysql_mutex_destroy(&innobase_share_mutex);
		mysql_mutex_destroy(&commit_cond_m);
		mysql_cond_destroy(&commit_cond);
	}

	DBUG_RETURN(err);
}

/****************************************************************//**
Flushes InnoDB logs to disk and makes a checkpoint. Really, a commit flushes
the logs, and the name of this function should be innobase_checkpoint.
@return	TRUE if error */
static
bool
innobase_flush_logs(
/*================*/
	handlerton*	hton)	/*!< in/out: InnoDB handlerton */
{
	bool	result = 0;

	DBUG_ENTER("innobase_flush_logs");
	DBUG_ASSERT(hton == innodb_hton_ptr);

	log_buffer_flush_to_disk();

	DBUG_RETURN(result);
}

/****************************************************************//**
Return alter table flags supported in an InnoDB database. */
static
uint
innobase_alter_table_flags(
/*=======================*/
	uint	flags)
{
	return(HA_INPLACE_ADD_INDEX_NO_READ_WRITE
		| HA_INPLACE_ADD_INDEX_NO_WRITE
		| HA_INPLACE_DROP_INDEX_NO_READ_WRITE
		| HA_INPLACE_ADD_UNIQUE_INDEX_NO_READ_WRITE
		| HA_INPLACE_ADD_UNIQUE_INDEX_NO_WRITE
		| HA_INPLACE_DROP_UNIQUE_INDEX_NO_READ_WRITE
		| HA_INPLACE_ADD_PK_INDEX_NO_READ_WRITE);
}

/************************************************************//**
Synchronously read and parse the redo log up to the last
checkpoint to write the changed page bitmap.
@return 0 to indicate success.  Current implementation cannot fail. */
static
my_bool
innobase_flush_changed_page_bitmaps()
/*=================================*/
{
	if (srv_track_changed_pages) {
		os_event_reset(srv_checkpoint_completed_event);
		log_online_follow_redo_log();
	}
	return FALSE;
}

/************************************************************//**
Delete all the bitmap files for data less than the specified LSN.
If called with lsn == IB_ULONGLONG_MAX (i.e. set by RESET request),
restart the bitmap file sequence, otherwise continue it.
@return 0 to indicate success, 1 for failure. */
static
my_bool
innobase_purge_changed_page_bitmaps(
/*================================*/
	ulonglong lsn)	/*!< in: LSN to purge files up to */
{
	return (my_bool)log_online_purge_changed_page_bitmaps(lsn);
}

/*****************************************************************//**
Check whether this is a fake change transaction.
@return TRUE if a fake change transaction */
static
my_bool
innobase_is_fake_change(
/*====================*/
	handlerton	*hton __attribute__((unused)),
				/*!< in: InnoDB handlerton */
	THD*		thd)	/*!< in: MySQL thread handle of the user for
				whom the transaction is being committed */
{
	trx_t*	trx = check_trx_exists(thd);
	return UNIV_UNLIKELY(trx->fake_changes);
}


/****************************************************************//**
Copy the current replication position from MySQL to a transaction. */
static
void
innobase_copy_repl_coords_to_trx(
/*=============================*/
	const THD*	thd,	/*!< in: thread handle */
	trx_t*		trx)	/*!< in/out: transaction */
{
        if (thd && thd_is_replication_slave_thread(thd)) {
        /* Update the replication position info inside InnoDB.
           In embedded server, does nothing. */
                const char *log_file_name, *group_relay_log_name;
                ulonglong log_pos, relay_log_pos;
                bool res = rpl_get_position_info(&log_file_name, &log_pos,
                                                 &group_relay_log_name,
                                                 &relay_log_pos);
                if (res) {
                        trx->mysql_master_log_file_name = log_file_name;
                        trx->mysql_master_log_pos = (ib_int64_t)log_pos;
                        trx->mysql_relay_log_file_name = group_relay_log_name;
                        trx->mysql_relay_log_pos = (ib_int64_t)relay_log_pos;
                }
        }
}

/*****************************************************************//**
Commits a transaction in an InnoDB database. */
static
void
innobase_commit_low(
/*================*/
	trx_t*	trx)	/*!< in: transaction handle */
{
	if (trx_is_started(trx)) {

		/* Save the current replication position for write to trx sys
		header for undo purposes, see the comment at corresponding call
		at innobase_xa_prepare(). */

		innobase_copy_repl_coords_to_trx((THD *) trx->mysql_thd, trx);

		trx_commit_for_mysql(trx);
	}
}

/*****************************************************************//**
Creates an InnoDB transaction struct for the thd if it does not yet have one.
Starts a new InnoDB transaction if a transaction is not yet started. And
assigns a new snapshot for a consistent read if the transaction does not yet
have one.
@return	0 */
static
int
innobase_start_trx_and_assign_read_view(
/*====================================*/
        handlerton *hton, /*!< in: Innodb handlerton */
	THD*	thd)	/*!< in: MySQL thread handle of the user for whom
			the transaction should be committed */
{
	trx_t*	trx;

	DBUG_ENTER("innobase_start_trx_and_assign_read_view");
	DBUG_ASSERT(hton == innodb_hton_ptr);

	/* Create a new trx struct for thd, if it does not yet have one */

	trx = check_trx_exists(thd);

	/* This is just to play safe: release a possible FIFO ticket and
	search latch. Since we will reserve the kernel mutex, we have to
	release the search system latch first to obey the latching order. */

	trx_search_latch_release_if_reserved(trx);
	innodb_srv_conc_force_exit_innodb(trx);

	/* If the transaction is not started yet, start it */

	trx_start_if_not_started(trx);

	/* Assign a read view if the transaction does not have it yet.
	Do this only if transaction is using REPEATABLE READ isolation
	level. */
	trx->isolation_level = innobase_map_isolation_level(
		thd_get_trx_isolation(thd));

	if (trx->isolation_level == TRX_ISO_REPEATABLE_READ) {
		trx_assign_read_view(trx);
	} else {
		push_warning_printf(thd, MYSQL_ERROR::WARN_LEVEL_WARN,
				    HA_ERR_UNSUPPORTED,
				    "InnoDB: WITH CONSISTENT SNAPSHOT "
				    "was ignored because this phrase "
				    "can only be used with "
				    "REPEATABLE READ isolation level.");
	}

	/* Set the MySQL flag to mark that there is an active transaction */

	innobase_register_trx(hton, current_thd, trx);

	DBUG_RETURN(0);
}

static
void
innobase_commit_ordered_2(
/*============*/
	trx_t*	trx, 	/*!< in: Innodb transaction */
	THD*	thd)	/*!< in: MySQL thread handle */
{
	ulonglong tmp_pos;
	DBUG_ENTER("innobase_commit_ordered");

	/* We need current binlog position for ibbackup to work.
	Note, the position is current because commit_ordered is guaranteed
	to be called in same sequenece as writing to binlog. */

retry:
	if (innobase_commit_concurrency > 0) {
		mysql_mutex_lock(&commit_cond_m);
		commit_threads++;

		if (commit_threads > innobase_commit_concurrency) {
			commit_threads--;
			mysql_cond_wait(&commit_cond,
					  &commit_cond_m);
			mysql_mutex_unlock(&commit_cond_m);
			goto retry;
		}
		else {
			mysql_mutex_unlock(&commit_cond_m);
		}
	}

	mysql_bin_log_commit_pos(thd, &tmp_pos, &(trx->mysql_log_file_name));
	trx->mysql_log_offset = (ib_int64_t) tmp_pos;

	/* Don't do write + flush right now. For group commit
	   to work we want to do the flush in the innobase_commit()
	   method, which runs without holding any locks. */
	trx->flush_log_later = TRUE;
	innobase_commit_low(trx);
	trx->flush_log_later = FALSE;

	if (innobase_commit_concurrency > 0) {
		mysql_mutex_lock(&commit_cond_m);
		commit_threads--;
		mysql_cond_signal(&commit_cond);
		mysql_mutex_unlock(&commit_cond_m);
	}

	DBUG_VOID_RETURN;
}

/*****************************************************************//**
Perform the first, fast part of InnoDB commit.

Doing it in this call ensures that we get the same commit order here
as in binlog and any other participating transactional storage engines.

Note that we want to do as little as really needed here, as we run
under a global mutex. The expensive fsync() is done later, in
innobase_commit(), without a lock so group commit can take place.

Note also that this method can be called from a different thread than
the one handling the rest of the transaction. */
static
void
innobase_commit_ordered(
/*============*/
	handlerton *hton, /*!< in: Innodb handlerton */
	THD*	thd,	/*!< in: MySQL thread handle of the user for whom
			the transaction should be committed */
	bool	all)	/*!< in:	TRUE - commit transaction
				FALSE - the current SQL statement ended */
{
	trx_t*		trx;
	DBUG_ENTER("innobase_commit_ordered");
	DBUG_ASSERT(hton == innodb_hton_ptr);

	trx = check_trx_exists(thd);

	/* Since we will reserve the kernel mutex, we must not be holding the
	search system latch, or we will disobey the latching order. But we
	already released it in innobase_xa_prepare() (if not before), so just
	have an assert here.*/
	ut_ad(!trx->has_search_latch);

        if (!trx_is_registered_for_2pc(trx) && trx_is_started(trx)) {
		/* We cannot throw error here; instead we will catch this error
		again in innobase_commit() and report it from there. */
		DBUG_VOID_RETURN;
	}

	/* commit_ordered is only called when committing the whole transaction
	(or an SQL statement when autocommit is on). */
	DBUG_ASSERT(all ||
		(!thd_test_options(thd, OPTION_NOT_AUTOCOMMIT | OPTION_BEGIN)));

	innobase_commit_ordered_2(trx, thd);

	trx_set_active_commit_ordered(trx);
	DBUG_VOID_RETURN;
}

/*****************************************************************//**
Commits a transaction in an InnoDB database or marks an SQL statement
ended.
@return	0 */
static
int
innobase_commit(
/*============*/
        handlerton *hton, /*!< in: Innodb handlerton */
	THD* 	thd,	/*!< in: MySQL thread handle of the user for whom
			the transaction should be committed */
	bool	all)	/*!< in:	TRUE - commit transaction
				FALSE - the current SQL statement ended */
{
	trx_t*		trx;

	DBUG_ENTER("innobase_commit");
	DBUG_ASSERT(hton == innodb_hton_ptr);
	DBUG_PRINT("trans", ("ending transaction"));

	trx = check_trx_exists(thd);

	/* Since we will reserve the kernel mutex, we have to release
	the search system latch first to obey the latching order. */

	/* No-op in XtraDB */
	trx_search_latch_release_if_reserved(trx);

	/* If fake-changes mode = ON then allow
	SELECT (they are read-only) and
	CREATE ... SELECT * from table (Well this doesn't open up DDL for InnoDB
	as ha_innobase::create will return appropriate error if fake-change = ON
	but if create is trying to use other SE and SELECT is executing on
	InnoDB table then we allow SELECT to proceed.
	Ideally, statement like this should be marked CREATE_SELECT like
	INSERT_SELECT but unfortunately it doesn't). */
	if (UNIV_UNLIKELY(trx->fake_changes
			  && (thd_sql_command(thd) != SQLCOM_SELECT
			      && thd_sql_command(thd) != SQLCOM_CREATE_TABLE)
			  && (all || (!thd_test_options(thd,
				OPTION_NOT_AUTOCOMMIT | OPTION_BEGIN))))) {

		/* rollback implicitly */
		innobase_rollback(hton, thd, all);

		/* because debug assertion code complains, if something left */
		thd->stmt_da->reset_diagnostics_area();

		DBUG_RETURN(HA_ERR_WRONG_COMMAND);
	}
	/* Transaction is deregistered only in a commit or a rollback. If
	it is deregistered we know there cannot be resources to be freed
	and we could return immediately.  For the time being, we play safe
	and do the cleanup though there should be nothing to clean up. */

	if (!trx_is_registered_for_2pc(trx) && trx_is_started(trx)) {

		sql_print_error("Transaction not registered for MySQL 2PC, "
				"but transaction is active");
	}

	if (all
	    || (!thd_test_options(thd, OPTION_NOT_AUTOCOMMIT | OPTION_BEGIN))) {

		DBUG_EXECUTE_IF("crash_innodb_before_commit",
				DBUG_SUICIDE(););

		/* Run the fast part of commit if we did not already. */
		if (!trx_is_active_commit_ordered(trx)) {
			innobase_commit_ordered_2(trx, thd);
		}

		/* We were instructed to commit the whole transaction, or
		this is an SQL statement end and autocommit is on */

		/* We did the first part already in innobase_commit_ordered(),
		Now finish by doing a write + flush of logs. */
		trx_commit_complete_for_mysql(trx);
                trx_deregister_from_2pc(trx);
	} else {
		/* We just mark the SQL statement ended and do not do a
		transaction commit */

		/* If we had reserved the auto-inc lock for some
		table in this SQL statement we release it now */

		row_unlock_table_autoinc_for_mysql(trx);

		/* Store the current undo_no of the transaction so that we
		know where to roll back if we have to roll back the next
		SQL statement */

		trx_mark_sql_stat_end(trx);
	}

	trx->n_autoinc_rows = 0; /* Reset the number AUTO-INC rows required */

	if (trx->declared_to_be_inside_innodb) {
		/* Release our possible ticket in the FIFO */

		srv_conc_force_exit_innodb(trx);
	}

	/* Tell the InnoDB server that there might be work for utility
	threads: */
	srv_active_wake_master_thread();

	DBUG_RETURN(0);
}

/*****************************************************************//**
Rolls back a transaction or the latest SQL statement.
@return	0 or error number */
static
int
innobase_rollback(
/*==============*/
        handlerton *hton, /*!< in: Innodb handlerton */ 
	THD*	thd,	/*!< in: handle to the MySQL thread of the user
			whose transaction should be rolled back */
	bool	all)	/*!< in:	TRUE - commit transaction
				FALSE - the current SQL statement ended */
{
	int	error = 0;
	trx_t*	trx;

	DBUG_ENTER("innobase_rollback");
	DBUG_ASSERT(hton == innodb_hton_ptr);
	DBUG_PRINT("trans", ("aborting transaction"));

	trx = check_trx_exists(thd);

	/* Release a possible FIFO ticket and search latch. Since we will
	reserve the kernel mutex, we have to release the search system latch
	first to obey the latching order. */

	trx_search_latch_release_if_reserved(trx);
	innodb_srv_conc_force_exit_innodb(trx);

	trx->n_autoinc_rows = 0; /* Reset the number AUTO-INC rows required */

	/* If we had reserved the auto-inc lock for some table (if
	we come here to roll back the latest SQL statement) we
	release it now before a possibly lengthy rollback */

	row_unlock_table_autoinc_for_mysql(trx);

	if (all
	    || !thd_test_options(thd, OPTION_NOT_AUTOCOMMIT | OPTION_BEGIN)) {

		error = trx_rollback_for_mysql(trx);
		trx_deregister_from_2pc(trx);
	} else {
		error = trx_rollback_last_sql_stat_for_mysql(trx);
	}

	DBUG_RETURN(convert_error_code_to_mysql(error, 0, NULL));
}

/*****************************************************************//**
Rolls back a transaction
@return	0 or error number */
static
int
innobase_rollback_trx(
/*==================*/
	trx_t*	trx)	/*!< in: transaction */
{
	int	error = 0;

	DBUG_ENTER("innobase_rollback_trx");
	DBUG_PRINT("trans", ("aborting transaction"));

	/* Release a possible FIFO ticket and search latch. Since we will
	reserve the kernel mutex, we have to release the search system latch
	first to obey the latching order. */

	trx_search_latch_release_if_reserved(trx);
	innodb_srv_conc_force_exit_innodb(trx);

	/* If we had reserved the auto-inc lock for some table (if
	we come here to roll back the latest SQL statement) we
	release it now before a possibly lengthy rollback */

	row_unlock_table_autoinc_for_mysql(trx);

	error = trx_rollback_for_mysql(trx);

	DBUG_RETURN(convert_error_code_to_mysql(error, 0, NULL));
}

/*****************************************************************//**
Rolls back a transaction to a savepoint.
@return 0 if success, HA_ERR_NO_SAVEPOINT if no savepoint with the
given name */
static
int
innobase_rollback_to_savepoint(
/*===========================*/
        handlerton *hton,       /*!< in: Innodb handlerton */ 
	THD*	thd,		/*!< in: handle to the MySQL thread of the user
				whose transaction should be rolled back */
	void*	savepoint)	/*!< in: savepoint data */
{
	ib_int64_t	mysql_binlog_cache_pos;
	int		error = 0;
	trx_t*		trx;
	char		name[64];

	DBUG_ENTER("innobase_rollback_to_savepoint");
	DBUG_ASSERT(hton == innodb_hton_ptr);

	trx = check_trx_exists(thd);

	/* Release a possible FIFO ticket and search latch. Since we will
	reserve the kernel mutex, we have to release the search system latch
	first to obey the latching order. */

	trx_search_latch_release_if_reserved(trx);
	innodb_srv_conc_force_exit_innodb(trx);

	/* TODO: use provided savepoint data area to store savepoint data */

	longlong2str((ulint)savepoint, name, 36);

	error = (int) trx_rollback_to_savepoint_for_mysql(trx, name,
						&mysql_binlog_cache_pos);
	DBUG_RETURN(convert_error_code_to_mysql(error, 0, NULL));
}

/*****************************************************************//**
Release transaction savepoint name.
@return 0 if success, HA_ERR_NO_SAVEPOINT if no savepoint with the
given name */
static
int
innobase_release_savepoint(
/*=======================*/
        handlerton*	hton,	/*!< in: handlerton for Innodb */
	THD*	thd,		/*!< in: handle to the MySQL thread of the user
				whose transaction should be rolled back */
	void*	savepoint)	/*!< in: savepoint data */
{
	int		error = 0;
	trx_t*		trx;
	char		name[64];

	DBUG_ENTER("innobase_release_savepoint");
	DBUG_ASSERT(hton == innodb_hton_ptr);

	trx = check_trx_exists(thd);

	/* TODO: use provided savepoint data area to store savepoint data */

	longlong2str((ulint)savepoint, name, 36);

	error = (int) trx_release_savepoint_for_mysql(trx, name);

	DBUG_RETURN(convert_error_code_to_mysql(error, 0, NULL));
}

/*****************************************************************//**
Sets a transaction savepoint.
@return	always 0, that is, always succeeds */
static
int
innobase_savepoint(
/*===============*/
	handlerton*	hton,   /*!< in: handle to the Innodb handlerton */
	THD*	thd,		/*!< in: handle to the MySQL thread */
	void*	savepoint)	/*!< in: savepoint data */
{
	int	error = 0;
	trx_t*	trx;

	DBUG_ENTER("innobase_savepoint");
	DBUG_ASSERT(hton == innodb_hton_ptr);

	/*
	  In the autocommit mode there is no sense to set a savepoint
	  (unless we are in sub-statement), so SQL layer ensures that
	  this method is never called in such situation.
	*/
#ifdef MYSQL_SERVER /* plugins cannot access thd->in_sub_stmt */
	DBUG_ASSERT(thd_test_options(thd, OPTION_NOT_AUTOCOMMIT | OPTION_BEGIN) ||
		thd->in_sub_stmt);
#endif /* MYSQL_SERVER */

	trx = check_trx_exists(thd);

	/* Release a possible FIFO ticket and search latch. Since we will
	reserve the kernel mutex, we have to release the search system latch
	first to obey the latching order. */

	trx_search_latch_release_if_reserved(trx);
	innodb_srv_conc_force_exit_innodb(trx);

	/* Cannot happen outside of transaction */
	DBUG_ASSERT(trx_is_registered_for_2pc(trx));

	/* TODO: use provided savepoint data area to store savepoint data */
	char name[64];
	longlong2str((ulint)savepoint,name,36);

	error = (int) trx_savepoint_for_mysql(trx, name, (ib_int64_t)0);

	DBUG_RETURN(convert_error_code_to_mysql(error, 0, NULL));
}

/*****************************************************************//**
Frees a possible InnoDB trx object associated with the current THD.
@return	0 or error number */
static
int
innobase_close_connection(
/*======================*/
        handlerton*	hton,	/*!< in:  innobase handlerton */
	THD*	thd)	/*!< in: handle to the MySQL thread of the user
			whose resources should be free'd */
{
	trx_t*	trx;

	DBUG_ENTER("innobase_close_connection");
	DBUG_ASSERT(hton == innodb_hton_ptr);
	trx = thd_to_trx(thd);

	ut_a(trx);

	if (!trx_is_registered_for_2pc(trx) && trx_is_started(trx)) {

		sql_print_error("Transaction not registered for MySQL 2PC, "
				"but transaction is active");
	}


	if (trx_is_started(trx) && global_system_variables.log_warnings) {

		sql_print_warning(
			"MySQL is closing a connection that has an active "
			"InnoDB transaction.  %llu row modifications will "
			"roll back.",
			(ullint) trx->undo_no);
	}

	innobase_rollback_trx(trx);

	trx_free_for_mysql(trx);

	DBUG_RETURN(0);
}

/*****************************************************************//**
Cancel any pending lock request associated with the current THD. */
static
void
innobase_kill_query(
/*======================*/
        handlerton*	hton,	    /*!< in: innobase handlerton */
	THD*	thd,	            /*!< in: MySQL thread being killed */
        enum thd_kill_levels level) /*!< in: kill level */
{
	trx_t*	trx;
	DBUG_ENTER("innobase_kill_query");
	DBUG_ASSERT(hton == innodb_hton_ptr);

	mutex_enter(&kernel_mutex);

	trx = thd_to_trx(thd);

	/* Cancel a pending lock request. */
	if (trx && trx->wait_lock) {
		lock_cancel_waiting_and_release(trx->wait_lock);
	}

	mutex_exit(&kernel_mutex);

	DBUG_VOID_RETURN;
}

/*************************************************************************//**
** InnoDB database tables
*****************************************************************************/

/****************************************************************//**
Get the record format from the data dictionary.
@return one of ROW_TYPE_REDUNDANT, ROW_TYPE_COMPACT,
ROW_TYPE_COMPRESSED, ROW_TYPE_DYNAMIC */
UNIV_INTERN
enum row_type
ha_innobase::get_row_type() const
/*=============================*/
{
	if (prebuilt && prebuilt->table) {
		const ulint	flags = prebuilt->table->flags;

		if (UNIV_UNLIKELY(!flags)) {
			return(ROW_TYPE_REDUNDANT);
		}

		ut_ad(flags & DICT_TF_COMPACT);

		switch (flags & DICT_TF_FORMAT_MASK) {
		case DICT_TF_FORMAT_51 << DICT_TF_FORMAT_SHIFT:
			return(ROW_TYPE_COMPACT);
		case DICT_TF_FORMAT_ZIP << DICT_TF_FORMAT_SHIFT:
			if (flags & DICT_TF_ZSSIZE_MASK) {
				return(ROW_TYPE_COMPRESSED);
			} else {
				return(ROW_TYPE_DYNAMIC);
			}
#if DICT_TF_FORMAT_ZIP != DICT_TF_FORMAT_MAX
# error "DICT_TF_FORMAT_ZIP != DICT_TF_FORMAT_MAX"
#endif
		}
	}
	ut_ad(0);
	return(ROW_TYPE_NOT_USED);
}



/****************************************************************//**
Get the table flags to use for the statement.
@return	table flags */
UNIV_INTERN
handler::Table_flags
ha_innobase::table_flags() const
/*============================*/
{
       /* Need to use tx_isolation here since table flags is (also)
          called before prebuilt is inited. */
        ulong const tx_isolation = thd_tx_isolation(ha_thd());
        if (tx_isolation <= ISO_READ_COMMITTED)
                return int_table_flags;
        return int_table_flags | HA_BINLOG_STMT_CAPABLE;
}

/****************************************************************//**
Gives the file extension of an InnoDB single-table tablespace. */
static const char* ha_innobase_exts[] = {
  ".ibd",
  NullS
};

/****************************************************************//**
Returns the table type (storage engine name).
@return	table type */
UNIV_INTERN
const char*
ha_innobase::table_type() const
/*===========================*/
{
	return(innobase_hton_name);
}

/****************************************************************//**
Returns the index type. */
UNIV_INTERN
const char*
ha_innobase::index_type(
/*====================*/
	uint)
				/*!< out: index type */
{
	return("BTREE");
}

/****************************************************************//**
Returns the table file name extension.
@return	file extension string */
UNIV_INTERN
const char**
ha_innobase::bas_ext() const
/*========================*/
{
	return(ha_innobase_exts);
}

/****************************************************************//**
Returns the operations supported for indexes.
@return	flags of supported operations */
UNIV_INTERN
ulong
ha_innobase::index_flags(
/*=====================*/
	uint index,
	uint part,
	bool all_parts)
const
{
       ulong extra_flag= 0;
       if (table && index == table->s->primary_key)
             extra_flag= HA_CLUSTERED_INDEX;
	return(HA_READ_NEXT | HA_READ_PREV | HA_READ_ORDER | extra_flag 
	       | HA_READ_RANGE | HA_KEYREAD_ONLY | HA_DO_INDEX_COND_PUSHDOWN);
}

/****************************************************************//**
Returns the maximum number of keys.
@return	MAX_KEY */
UNIV_INTERN
uint
ha_innobase::max_supported_keys() const
/*===================================*/
{
	return(MAX_KEY);
}

/****************************************************************//**
Returns the maximum key length.
@return	maximum supported key length, in bytes */
UNIV_INTERN
uint
ha_innobase::max_supported_key_length() const
/*=========================================*/
{
	/* An InnoDB page must store >= 2 keys; a secondary key record
	must also contain the primary key value: max key length is
	therefore set to slightly less than 1 / 4 of page size which
	is 16 kB; but currently MySQL does not work with keys whose
	size is > MAX_KEY_LENGTH */
	return(3500);
}

/****************************************************************//**
Returns the key map of keys that are usable for scanning.
@return	key_map_full */
UNIV_INTERN
const key_map*
ha_innobase::keys_to_use_for_scanning()
{
	return(&key_map_full);
}

/****************************************************************//**
Determines if table caching is supported.
@return	HA_CACHE_TBL_ASKTRANSACT */
UNIV_INTERN
uint8
ha_innobase::table_cache_type()
{
	return(HA_CACHE_TBL_ASKTRANSACT);
}

/****************************************************************//**
Determines if the primary key is clustered index.
@return	true */
UNIV_INTERN
bool
ha_innobase::primary_key_is_clustered()
{
	return(true);
}

/** Always normalize table name to lower case on Windows */
#ifdef __WIN__
#define normalize_table_name(norm_name, name)		\
	normalize_table_name_low(norm_name, name, TRUE)
#else
#define normalize_table_name(norm_name, name)           \
	normalize_table_name_low(norm_name, name, FALSE)
#endif /* __WIN__ */

/*****************************************************************//**
Normalizes a table name string. A normalized name consists of the
database name catenated to '/' and table name. An example:
test/mytable. On Windows normalization puts both the database name and the
table name always to lower case if "set_lower_case" is set to TRUE. */
extern "C" UNIV_INTERN
void
normalize_table_name_low(
/*=====================*/
	char*		norm_name,	/*!< out: normalized name as a
					null-terminated string */
	const char*	name,		/*!< in: table name string */
	ibool		set_lower_case) /*!< in: TRUE if we want to set
					name to lower case */
{
	char*	name_ptr;
	char*	db_ptr;
	ulint	db_len;
	char*	ptr;

	/* Scan name from the end */

	ptr = strend(name) - 1;

	/* seek to the last path separator */
	while (ptr >= name && *ptr != '\\' && *ptr != '/') {
		ptr--;
	}

	name_ptr = ptr + 1;

	/* skip any number of path separators */
	while (ptr >= name && (*ptr == '\\' || *ptr == '/')) {
		ptr--;
	}

	DBUG_ASSERT(ptr >= name);

	/* seek to the last but one path separator or one char before
	the beginning of name */
	db_len = 0;
	while (ptr >= name && *ptr != '\\' && *ptr != '/') {
		ptr--;
		db_len++;
	}

	db_ptr = ptr + 1;

	memcpy(norm_name, db_ptr, db_len);

	norm_name[db_len] = '/';

	memcpy(norm_name + db_len + 1, name_ptr, strlen(name_ptr) + 1);

	if (set_lower_case) {
		innobase_casedn_str(norm_name);
	}
}

#if !defined(DBUG_OFF)
/*********************************************************************
Test normalize_table_name_low(). */
static
void
test_normalize_table_name_low()
/*===========================*/
{
	char		norm_name[128];
	const char*	test_data[][2] = {
		/* input, expected result */
		{"./mysqltest/t1", "mysqltest/t1"},
		{"./test/#sql-842b_2", "test/#sql-842b_2"},
		{"./test/#sql-85a3_10", "test/#sql-85a3_10"},
		{"./test/#sql2-842b-2", "test/#sql2-842b-2"},
		{"./test/bug29807", "test/bug29807"},
		{"./test/foo", "test/foo"},
		{"./test/innodb_bug52663", "test/innodb_bug52663"},
		{"./test/t", "test/t"},
		{"./test/t1", "test/t1"},
		{"./test/t10", "test/t10"},
		{"/a/b/db/table", "db/table"},
		{"/a/b/db///////table", "db/table"},
		{"/a/b////db///////table", "db/table"},
		{"/var/tmp/mysqld.1/#sql842b_2_10", "mysqld.1/#sql842b_2_10"},
		{"db/table", "db/table"},
		{"ddd/t", "ddd/t"},
		{"d/ttt", "d/ttt"},
		{"d/t", "d/t"},
		{".\\mysqltest\\t1", "mysqltest/t1"},
		{".\\test\\#sql-842b_2", "test/#sql-842b_2"},
		{".\\test\\#sql-85a3_10", "test/#sql-85a3_10"},
		{".\\test\\#sql2-842b-2", "test/#sql2-842b-2"},
		{".\\test\\bug29807", "test/bug29807"},
		{".\\test\\foo", "test/foo"},
		{".\\test\\innodb_bug52663", "test/innodb_bug52663"},
		{".\\test\\t", "test/t"},
		{".\\test\\t1", "test/t1"},
		{".\\test\\t10", "test/t10"},
		{"C:\\a\\b\\db\\table", "db/table"},
		{"C:\\a\\b\\db\\\\\\\\\\\\\\table", "db/table"},
		{"C:\\a\\b\\\\\\\\db\\\\\\\\\\\\\\table", "db/table"},
		{"C:\\var\\tmp\\mysqld.1\\#sql842b_2_10", "mysqld.1/#sql842b_2_10"},
		{"db\\table", "db/table"},
		{"ddd\\t", "ddd/t"},
		{"d\\ttt", "d/ttt"},
		{"d\\t", "d/t"},
	};

	for (size_t i = 0; i < UT_ARR_SIZE(test_data); i++) {
		printf("test_normalize_table_name_low(): "
		       "testing \"%s\", expected \"%s\"... ",
		       test_data[i][0], test_data[i][1]);

		normalize_table_name_low(norm_name, test_data[i][0], FALSE);

		if (strcmp(norm_name, test_data[i][1]) == 0) {
			printf("ok\n");
		} else {
			printf("got \"%s\"\n", norm_name);
			ut_error;
		}
	}
}
#endif /* !DBUG_OFF */

/********************************************************************//**
Get the upper limit of the MySQL integral and floating-point type.
@return maximum allowed value for the field */
static
ulonglong
innobase_get_int_col_max_value(
/*===========================*/
	const Field*	field)	/*!< in: MySQL field */
{
	ulonglong	max_value = 0;

	switch(field->key_type()) {
	/* TINY */
	case HA_KEYTYPE_BINARY:
		max_value = 0xFFULL;
		break;
	case HA_KEYTYPE_INT8:
		max_value = 0x7FULL;
		break;
	/* SHORT */
	case HA_KEYTYPE_USHORT_INT:
		max_value = 0xFFFFULL;
		break;
	case HA_KEYTYPE_SHORT_INT:
		max_value = 0x7FFFULL;
		break;
	/* MEDIUM */
	case HA_KEYTYPE_UINT24:
		max_value = 0xFFFFFFULL;
		break;
	case HA_KEYTYPE_INT24:
		max_value = 0x7FFFFFULL;
		break;
	/* LONG */
	case HA_KEYTYPE_ULONG_INT:
		max_value = 0xFFFFFFFFULL;
		break;
	case HA_KEYTYPE_LONG_INT:
		max_value = 0x7FFFFFFFULL;
		break;
	/* BIG */
	case HA_KEYTYPE_ULONGLONG:
		max_value = 0xFFFFFFFFFFFFFFFFULL;
		break;
	case HA_KEYTYPE_LONGLONG:
		max_value = 0x7FFFFFFFFFFFFFFFULL;
		break;
	case HA_KEYTYPE_FLOAT:
		/* We use the maximum as per IEEE754-2008 standard, 2^24 */
		max_value = 0x1000000ULL;
		break;
	case HA_KEYTYPE_DOUBLE:
		/* We use the maximum as per IEEE754-2008 standard, 2^53 */
		max_value = 0x20000000000000ULL;
		break;
	default:
		ut_error;
	}

	return(max_value);
}

/*******************************************************************//**
This function checks whether the index column information
is consistent between KEY info from mysql and that from innodb index.
@return TRUE if all column types match. */
static
ibool
innobase_match_index_columns(
/*=========================*/
	const KEY*		key_info,	/*!< in: Index info
						from mysql */
	const dict_index_t*	index_info)	/*!< in: Index info
						from Innodb */
{
	const KEY_PART_INFO*	key_part;
	const KEY_PART_INFO*	key_end;
	const dict_field_t*	innodb_idx_fld;
	const dict_field_t*	innodb_idx_fld_end;

	DBUG_ENTER("innobase_match_index_columns");

	/* Check whether user defined index column count matches */
	if (key_info->key_parts != index_info->n_user_defined_cols) {
		DBUG_RETURN(FALSE);
	}

	key_part = key_info->key_part;
	key_end = key_part + key_info->key_parts;
	innodb_idx_fld = index_info->fields;
	innodb_idx_fld_end = index_info->fields + index_info->n_fields;

	/* Check each index column's datatype. We do not check
	column name because there exists case that index
	column name got modified in mysql but such change does not
	propagate to InnoDB.
	One hidden assumption here is that the index column sequences
	are matched up between those in mysql and Innodb. */
	for (; key_part != key_end; ++key_part) {
		ulint	col_type;
		ibool	is_unsigned;
		ulint	mtype = innodb_idx_fld->col->mtype;

		/* Need to translate to InnoDB column type before
		comparison. */
		col_type = get_innobase_type_from_mysql_type(&is_unsigned,
							     key_part->field);

		/* Ignore Innodb specific system columns. */
		while (mtype == DATA_SYS) {
			innodb_idx_fld++;

			if (innodb_idx_fld >= innodb_idx_fld_end) {
				DBUG_RETURN(FALSE);
			}
		}

		if (col_type != mtype) {
			/* Column Type mismatches */
			DBUG_RETURN(FALSE);
		}

		innodb_idx_fld++;
	}

	DBUG_RETURN(TRUE);
}

/*******************************************************************//**
This function builds a translation table in INNOBASE_SHARE
structure for fast index location with mysql array number from its
table->key_info structure. This also provides the necessary translation
between the key order in mysql key_info and Innodb ib_table->indexes if
they are not fully matched with each other.
Note we do not have any mutex protecting the translation table
building based on the assumption that there is no concurrent
index creation/drop and DMLs that requires index lookup. All table
handle will be closed before the index creation/drop.
@return TRUE if index translation table built successfully */
UNIV_INTERN
ibool
innobase_build_index_translation(
/*=============================*/
	const TABLE*		table,	  /*!< in: table in MySQL data
					  dictionary */
	dict_table_t*		ib_table, /*!< in: table in Innodb data
					  dictionary */
	INNOBASE_SHARE*		share)	  /*!< in/out: share structure
					  where index translation table
					  will be constructed in. */
{
	ulint		mysql_num_index;
	ulint		ib_num_index;
	dict_index_t**	index_mapping;
	ibool		ret = TRUE;

	DBUG_ENTER("innobase_build_index_translation");

	mutex_enter(&dict_sys->mutex);

	mysql_num_index = table->s->keys;
	ib_num_index = UT_LIST_GET_LEN(ib_table->indexes);

	index_mapping = share->idx_trans_tbl.index_mapping;

	/* If there exists inconsistency between MySQL and InnoDB dictionary
	(metadata) information, the number of index defined in MySQL
	could exceed that in InnoDB, do not build index translation
	table in such case */
	if (UNIV_UNLIKELY(ib_num_index < mysql_num_index)) {
		ret = FALSE;
		goto func_exit;
	}

	/* If index entry count is non-zero, nothing has
	changed since last update, directly return TRUE */
	if (share->idx_trans_tbl.index_count) {
		/* Index entry count should still match mysql_num_index */
		ut_a(share->idx_trans_tbl.index_count == mysql_num_index);
		goto func_exit;
	}

	/* The number of index increased, rebuild the mapping table */
	if (mysql_num_index > share->idx_trans_tbl.array_size) {
		index_mapping = (dict_index_t**) my_realloc(index_mapping,
							mysql_num_index *
							sizeof(*index_mapping),
							MYF(MY_ALLOW_ZERO_PTR));

		if (!index_mapping) {
			/* Report an error if index_mapping continues to be
			NULL and mysql_num_index is a non-zero value */
			sql_print_error("InnoDB: fail to allocate memory for "
					"index translation table. Number of "
					"Index:%lu, array size:%lu",
					mysql_num_index,
					share->idx_trans_tbl.array_size);
			ret = FALSE;
			goto func_exit;
		}

		share->idx_trans_tbl.array_size = mysql_num_index;
	}

	/* For each index in the mysql key_info array, fetch its
	corresponding InnoDB index pointer into index_mapping
	array. */
	for (ulint count = 0; count < mysql_num_index; count++) {

		/* Fetch index pointers into index_mapping according to mysql
		index sequence */
		index_mapping[count] = dict_table_get_index_on_name(
			ib_table, table->key_info[count].name);

		if (!index_mapping[count]) {
			sql_print_error("Cannot find index %s in InnoDB "
					"index dictionary.",
					table->key_info[count].name);
			ret = FALSE;
			goto func_exit;
		}

		/* Double check fetched index has the same
		column info as those in mysql key_info. */
		if (!innobase_match_index_columns(&table->key_info[count],
					          index_mapping[count])) {
			sql_print_error("Found index %s whose column info "
					"does not match that of MySQL.",
					table->key_info[count].name);
			ret = FALSE;
			goto func_exit;
		}
	}

	/* Successfully built the translation table */
	share->idx_trans_tbl.index_count = mysql_num_index;

func_exit:
	if (!ret) {
		/* Build translation table failed. */
		my_free(index_mapping);

		share->idx_trans_tbl.array_size = 0;
		share->idx_trans_tbl.index_count = 0;
		index_mapping = NULL;
	}

	share->idx_trans_tbl.index_mapping = index_mapping;

	mutex_exit(&dict_sys->mutex);

	DBUG_RETURN(ret);
}

/*******************************************************************//**
This function uses index translation table to quickly locate the
requested index structure.
Note we do not have mutex protection for the index translatoin table
access, it is based on the assumption that there is no concurrent
translation table rebuild (fter create/drop index) and DMLs that
require index lookup.
@return dict_index_t structure for requested index. NULL if
fail to locate the index structure. */
static
dict_index_t*
innobase_index_lookup(
/*==================*/
	INNOBASE_SHARE*	share,	/*!< in: share structure for index
				translation table. */
	uint		keynr)	/*!< in: index number for the requested
				index */
{
	if (!share->idx_trans_tbl.index_mapping
	    || keynr >= share->idx_trans_tbl.index_count) {
		return(NULL);
	}

	return(share->idx_trans_tbl.index_mapping[keynr]);
}

/************************************************************************
Set the autoinc column max value. This should only be called once from
ha_innobase::open(). Therefore there's no need for a covering lock. */
UNIV_INTERN
void
ha_innobase::innobase_initialize_autoinc()
/*======================================*/
{
	ulonglong	auto_inc;
	const Field*	field = table->found_next_number_field;

	if (field != NULL) {
		auto_inc = innobase_get_int_col_max_value(field);
	} else {
		/* We have no idea what's been passed in to us as the
		autoinc column. We set it to the 0, effectively disabling
		updates to the table. */
		auto_inc = 0;

		ut_print_timestamp(stderr);
		fprintf(stderr, "  InnoDB: Unable to determine the AUTOINC "
				"column name\n");
	}

	if (srv_force_recovery >= SRV_FORCE_NO_IBUF_MERGE) {
		/* If the recovery level is set so high that writes
		are disabled we force the AUTOINC counter to 0
		value effectively disabling writes to the table.
		Secondly, we avoid reading the table in case the read
		results in failure due to a corrupted table/index.

		We will not return an error to the client, so that the
		tables can be dumped with minimal hassle.  If an error
		were returned in this case, the first attempt to read
		the table would fail and subsequent SELECTs would succeed. */
		auto_inc = 0;
	} else if (field == NULL) {
		/* This is a far more serious error, best to avoid
		opening the table and return failure. */
		my_error(ER_AUTOINC_READ_FAILED, MYF(0));
	} else {
		dict_index_t*	index;
		const char*	col_name;
		ulonglong	read_auto_inc;
		ulint		err;

		update_thd(ha_thd());

		ut_a(prebuilt->trx == thd_to_trx(user_thd));

		col_name = field->field_name;
		index = innobase_get_index(table->s->next_number_index);

		/* Execute SELECT MAX(col_name) FROM TABLE; */
		err = row_search_max_autoinc(index, col_name, &read_auto_inc);

		switch (err) {
		case DB_SUCCESS: {
			ulonglong	col_max_value;

			col_max_value = innobase_get_int_col_max_value(field);

			/* At the this stage we do not know the increment
			nor the offset, so use a default increment of 1. */

			auto_inc = innobase_next_autoinc(
				read_auto_inc, 1, 1, 0, col_max_value);

			break;
		}
		case DB_RECORD_NOT_FOUND:
			ut_print_timestamp(stderr);
			fprintf(stderr, "  InnoDB: MySQL and InnoDB data "
				"dictionaries are out of sync.\n"
				"InnoDB: Unable to find the AUTOINC column "
				"%s in the InnoDB table %s.\n"
				"InnoDB: We set the next AUTOINC column "
				"value to 0,\n"
				"InnoDB: in effect disabling the AUTOINC "
				"next value generation.\n"
				"InnoDB: You can either set the next "
				"AUTOINC value explicitly using ALTER TABLE\n"
				"InnoDB: or fix the data dictionary by "
				"recreating the table.\n",
				col_name, index->table->name);

			/* This will disable the AUTOINC generation. */
			auto_inc = 0;

			/* We want the open to succeed, so that the user can
			take corrective action. ie. reads should succeed but
			updates should fail. */
			err = DB_SUCCESS;
			break;
		default:
			/* row_search_max_autoinc() should only return
			one of DB_SUCCESS or DB_RECORD_NOT_FOUND. */
			ut_error;
		}
	}

	dict_table_autoinc_initialize(prebuilt->table, auto_inc);
}

/*****************************************************************//**
Creates and opens a handle to a table which already exists in an InnoDB
database.
@return	1 if error, 0 if success */
UNIV_INTERN
int
ha_innobase::open(
/*==============*/
	const char*		name,		/*!< in: table name */
	int			mode,		/*!< in: not used */
	uint			test_if_locked)	/*!< in: not used */
{
	dict_table_t*		ib_table;
	char			norm_name[1000];
	THD*			thd;
	char*			is_part = NULL;
	ibool			par_case_name_set = FALSE;
	char			par_case_name[MAX_FULL_NAME_LEN + 1];
	dict_err_ignore_t	ignore_err = DICT_ERR_IGNORE_NONE;

	DBUG_ENTER("ha_innobase::open");

	UT_NOT_USED(mode);
	UT_NOT_USED(test_if_locked);

	thd = ha_thd();

	/* Under some cases MySQL seems to call this function while
	holding btr_search_latch. This breaks the latching order as
	we acquire dict_sys->mutex below and leads to a deadlock. */
	if (thd != NULL) {
		innobase_release_temporary_latches(ht, thd);
	}

	normalize_table_name(norm_name, name);

	user_thd = NULL;

	if (!(share=get_share(name))) {

		DBUG_RETURN(1);
	}

	if (UNIV_UNLIKELY(share->ib_table &&
			 share->ib_table->is_corrupt &&
			 srv_pass_corrupt_table <= 1)) {
		free_share(share);

		DBUG_RETURN(HA_ERR_CRASHED_ON_USAGE);
	}

	/* Will be allocated if it is needed in ::update_row() */
	upd_buf = NULL;
	upd_buf_size = 0;

	/* We look for pattern #P# to see if the table is partitioned
	MySQL table. */
#ifdef __WIN__
	is_part = strstr(norm_name, "#p#");
#else
	is_part = strstr(norm_name, "#P#");
#endif /* __WIN__ */

	/* Check whether FOREIGN_KEY_CHECKS is set to 0. If so, the table
	can be opened even if some FK indexes are missing. If not, the table
	can't be opened in the same situation */
	if (thd_test_options(thd, OPTION_NO_FOREIGN_KEY_CHECKS)) {
		ignore_err = DICT_ERR_IGNORE_FK_NOKEY;
	}

	/* Get pointer to a table object in InnoDB dictionary cache */
	ib_table = dict_table_get(norm_name, TRUE, ignore_err);

	if (UNIV_UNLIKELY(ib_table &&
			 ib_table->is_corrupt &&
			 srv_pass_corrupt_table <= 1)) {
		free_share(share);
		my_free(upd_buf);
		upd_buf = NULL;
		upd_buf_size = 0;

		DBUG_RETURN(HA_ERR_CRASHED_ON_USAGE);
	}

	share->ib_table = ib_table;

	if (NULL == ib_table) {
		if (is_part) {
			/* MySQL partition engine hard codes the file name
			separator as "#P#". The text case is fixed even if
			lower_case_table_names is set to 1 or 2. This is true
			for sub-partition names as well. InnoDB always
			normalises file names to lower case on Windows, this
			can potentially cause problems when copying/moving
			tables between platforms.

			1) If boot against an installation from Windows
			platform, then its partition table name could
			be all be in lower case in system tables. So we
			will need to check lower case name when load table.

			2) If  we boot an installation from other case
			sensitive platform in Windows, we might need to
			check the existence of table name without lowering
			case them in the system table. */
			if (innobase_get_lower_case_table_names() == 1) {

				if (!par_case_name_set) {
#ifndef __WIN__
					/* Check for the table using lower
					case name, including the partition
					separator "P" */
					memcpy(par_case_name, norm_name,
					       strlen(norm_name));
					par_case_name[strlen(norm_name)] = 0;
					innobase_casedn_str(par_case_name);
#else
					/* On Windows platfrom, check
					whether there exists table name in
					system table whose name is
					not being normalized to lower case */
					normalize_table_name_low(
						par_case_name, name, FALSE);
#endif
					par_case_name_set = TRUE;
				}

				ib_table = dict_table_get(
					par_case_name, TRUE, ignore_err);
			}

			if (ib_table) {
#ifndef __WIN__
				sql_print_warning("Partition table %s opened "
						  "after converting to lower "
						  "case. The table may have "
						  "been moved from a case "
						  "in-sensitive file system. "
						  "Please recreate table in "
						  "the current file system\n",
						  norm_name);
#else
				sql_print_warning("Partition table %s opened "
						  "after skipping the step to "
						  "lower case the table name. "
						  "The table may have been "
						  "moved from a case sensitive "
						  "file system. Please "
						  "recreate table in the "
						  "current file system\n",
						  norm_name);
#endif
				/* We allow use of table if it is found.
				this is consistent to current behavior
				to innodb_plugin */
				share->ib_table = ib_table;
				goto table_opened;
			}
		}

		if (is_part) {
			sql_print_error("Failed to open table %s.\n",
					norm_name);
		}

		sql_print_error("Cannot find or open table %s from\n"
				"the internal data dictionary of InnoDB "
				"though the .frm file for the\n"
				"table exists. Maybe you have deleted and "
				"recreated InnoDB data\n"
				"files but have forgotten to delete the "
				"corresponding .frm files\n"
				"of InnoDB tables, or you have moved .frm "
				"files to another database?\n"
				"or, the table contains indexes that this "
				"version of the engine\n"
				"doesn't support.\n"
				"See " REFMAN "innodb-troubleshooting.html\n"
				"how you can resolve the problem.\n",
				norm_name);
		free_share(share);
		my_errno = ENOENT;

		DBUG_RETURN(HA_ERR_NO_SUCH_TABLE);
	}

table_opened:

	if (ib_table->ibd_file_missing && !thd_tablespace_op(thd)) {
		sql_print_error("MySQL is trying to open a table handle but "
				"the .ibd file for\ntable %s does not exist.\n"
				"Have you deleted the .ibd file from the "
				"database directory under\nthe MySQL datadir, "
				"or have you used DISCARD TABLESPACE?\n"
				"See " REFMAN "innodb-troubleshooting.html\n"
				"how you can resolve the problem.\n",
				norm_name);
		free_share(share);
		my_errno = ENOENT;

		dict_table_decrement_handle_count(ib_table, FALSE);
		DBUG_RETURN(HA_ERR_NO_SUCH_TABLE);
	}

	prebuilt = row_create_prebuilt(ib_table, table->s->stored_rec_length);

	prebuilt->default_rec = table->s->default_values;
	ut_ad(prebuilt->default_rec);

	/* Looks like MySQL-3.23 sometimes has primary key number != 0 */

	primary_key = table->s->primary_key;
	key_used_on_scan = primary_key;

	if (!innobase_build_index_translation(table, ib_table, share)) {
		  sql_print_error("Build InnoDB index translation table for"
				  " Table %s failed", name);
	}

	/* Allocate a buffer for a 'row reference'. A row reference is
	a string of bytes of length ref_length which uniquely specifies
	a row in our table. Note that MySQL may also compare two row
	references for equality by doing a simple memcmp on the strings
	of length ref_length! */

	if (!row_table_got_default_clust_index(ib_table)) {

		prebuilt->clust_index_was_generated = FALSE;

		if (UNIV_UNLIKELY(primary_key >= MAX_KEY)) {
			sql_print_error("Table %s has a primary key in "
					"InnoDB data dictionary, but not "
					"in MySQL!", name);

			/* This mismatch could cause further problems
			if not attended, bring this to the user's attention
			by printing a warning in addition to log a message
			in the errorlog */
			push_warning_printf(thd, MYSQL_ERROR::WARN_LEVEL_WARN,
					    ER_NO_SUCH_INDEX,
					    "InnoDB: Table %s has a "
					    "primary key in InnoDB data "
					    "dictionary, but not in "
					    "MySQL!", name);

			/* If primary_key >= MAX_KEY, its (primary_key)
			value could be out of bound if continue to index
			into key_info[] array. Find InnoDB primary index,
			and assign its key_length to ref_length.
			In addition, since MySQL indexes are sorted starting
			with primary index, unique index etc., initialize
			ref_length to the first index key length in
			case we fail to find InnoDB cluster index.

			Please note, this will not resolve the primary
			index mismatch problem, other side effects are
			possible if users continue to use the table.
			However, we allow this table to be opened so
			that user can adopt necessary measures for the
			mismatch while still being accessible to the table
			date. */
			ref_length = table->key_info[0].key_length;

			/* Find correspoinding cluster index
			key length in MySQL's key_info[] array */
			for (ulint i = 0; i < table->s->keys; i++) {
				dict_index_t*	index;
				index = innobase_get_index(i);
				if (dict_index_is_clust(index)) {
					ref_length =
						 table->key_info[i].key_length;
				}
			}
		} else {
			/* MySQL allocates the buffer for ref.
			key_info->key_length includes space for all key
			columns + one byte for each column that may be
			NULL. ref_length must be as exact as possible to
			save space, because all row reference buffers are
			allocated based on ref_length. */

			ref_length = table->key_info[primary_key].key_length;
		}
	} else {
		if (primary_key != MAX_KEY) {
			sql_print_error(
				"Table %s has no primary key in InnoDB data "
				"dictionary, but has one in MySQL! If you "
				"created the table with a MySQL version < "
				"3.23.54 and did not define a primary key, "
				"but defined a unique key with all non-NULL "
				"columns, then MySQL internally treats that "
				"key as the primary key. You can fix this "
				"error by dump + DROP + CREATE + reimport "
				"of the table.", name);

			/* This mismatch could cause further problems
			if not attended, bring this to the user attention
			by printing a warning in addition to log a message
			in the errorlog */
			push_warning_printf(thd, MYSQL_ERROR::WARN_LEVEL_WARN,
					    ER_NO_SUCH_INDEX,
					    "InnoDB: Table %s has no "
					    "primary key in InnoDB data "
					    "dictionary, but has one in "
					    "MySQL!", name);
		}

		prebuilt->clust_index_was_generated = TRUE;

		ref_length = DATA_ROW_ID_LEN;

		/* If we automatically created the clustered index, then
		MySQL does not know about it, and MySQL must NOT be aware
		of the index used on scan, to make it avoid checking if we
		update the column of the index. That is why we assert below
		that key_used_on_scan is the undefined value MAX_KEY.
		The column is the row id in the automatical generation case,
		and it will never be updated anyway. */

		if (key_used_on_scan != MAX_KEY) {
			sql_print_warning(
				"Table %s key_used_on_scan is %lu even "
				"though there is no primary key inside "
				"InnoDB.", name, (ulong) key_used_on_scan);
		}
	}

	/* Index block size in InnoDB: used by MySQL in query optimization */
	stats.block_size = 16 * 1024;

	/* Init table lock structure */
	thr_lock_data_init(&share->lock,&lock,(void*) 0);

	if (prebuilt->table) {
		/* We update the highest file format in the system table
		space, if this table has higher file format setting. */

		trx_sys_file_format_max_upgrade(
			(const char**) &innobase_file_format_max,
			dict_table_get_format(prebuilt->table));
	}

	/* Only if the table has an AUTOINC column. */
	if (prebuilt->table != NULL && table->found_next_number_field != NULL) {
		dict_table_autoinc_lock(prebuilt->table);

		/* Since a table can already be "open" in InnoDB's internal
		data dictionary, we only init the autoinc counter once, the
		first time the table is loaded. We can safely reuse the
		autoinc value from a previous MySQL open. */
		if (dict_table_autoinc_read(prebuilt->table) == 0) {

			innobase_initialize_autoinc();
		}

		dict_table_autoinc_unlock(prebuilt->table);
	}

	info(HA_STATUS_NO_LOCK | HA_STATUS_VARIABLE | HA_STATUS_CONST);

	DBUG_RETURN(0);
}

UNIV_INTERN
handler*
ha_innobase::clone(
/*===============*/
	const char*	name,		/*!< in: table name */
	MEM_ROOT*	mem_root)	/*!< in: memory context */
{
	ha_innobase* new_handler;

	DBUG_ENTER("ha_innobase::clone");

	new_handler = static_cast<ha_innobase*>(handler::clone(name,
							       mem_root));
	if (new_handler) {
		new_handler->prebuilt->select_lock_type
			= prebuilt->select_lock_type;
	}

	DBUG_RETURN(new_handler);
}

UNIV_INTERN
uint
ha_innobase::max_supported_key_part_length() const
{
	/* A table format specific index column length check will be performed
	at ha_innobase::add_index() and row_create_index_for_mysql() */
	return(innobase_large_prefix
		? REC_VERSION_56_MAX_INDEX_COL_LEN
		: REC_ANTELOPE_MAX_INDEX_COL_LEN - 1);
}

/******************************************************************//**
Closes a handle to an InnoDB table.
@return	0 */
UNIV_INTERN
int
ha_innobase::close(void)
/*====================*/
{
	THD*	thd;

	DBUG_ENTER("ha_innobase::close");

	thd = ha_thd();
	if (thd != NULL) {
		innobase_release_temporary_latches(ht, thd);
	}

	row_prebuilt_free(prebuilt, FALSE);

	if (upd_buf != NULL) {
		ut_ad(upd_buf_size != 0);
		my_free(upd_buf);
		upd_buf = NULL;
		upd_buf_size = 0;
	}

	free_share(share);

	/* Tell InnoDB server that there might be work for
	utility threads: */

	srv_active_wake_master_thread();

	DBUG_RETURN(0);
}

/* The following accessor functions should really be inside MySQL code! */

/**************************************************************//**
Gets field offset for a field in a table.
@return	offset */
static inline
uint
get_field_offset(
/*=============*/
	const TABLE*	table,	/*!< in: MySQL table object */
	const Field*	field)	/*!< in: MySQL field object */
{
	return((uint) (field->ptr - table->record[0]));
}

/**************************************************************//**
Checks if a field in a record is SQL NULL. Uses the record format
information in table to track the null bit in record.
@return	1 if NULL, 0 otherwise */
static inline
uint
field_in_record_is_null(
/*====================*/
	TABLE*	table,	/*!< in: MySQL table object */
	Field*	field,	/*!< in: MySQL field object */
	char*	record)	/*!< in: a row in MySQL format */
{
	int	null_offset;

	if (!field->null_ptr) {

		return(0);
	}

	null_offset = (uint) ((char*) field->null_ptr
					- (char*) table->record[0]);

	if (record[null_offset] & field->null_bit) {

		return(1);
	}

	return(0);
}

/*************************************************************//**
InnoDB uses this function to compare two data fields for which the data type
is such that we must use MySQL code to compare them. NOTE that the prototype
of this function is in rem0cmp.c in InnoDB source code! If you change this
function, remember to update the prototype there!
@return	1, 0, -1, if a is greater, equal, less than b, respectively */
extern "C" UNIV_INTERN
int
innobase_mysql_cmp(
/*===============*/
	int		mysql_type,	/*!< in: MySQL type */
	uint		charset_number,	/*!< in: number of the charset */
	const unsigned char* a,		/*!< in: data field */
	unsigned int	a_length,	/*!< in: data field length,
					not UNIV_SQL_NULL */
	const unsigned char* b,		/*!< in: data field */
	unsigned int	b_length)	/*!< in: data field length,
					not UNIV_SQL_NULL */
{
	CHARSET_INFO*		charset;
	enum_field_types	mysql_tp;
	int			ret;

	DBUG_ASSERT(a_length != UNIV_SQL_NULL);
	DBUG_ASSERT(b_length != UNIV_SQL_NULL);

	mysql_tp = (enum_field_types) mysql_type;

	switch (mysql_tp) {

	case MYSQL_TYPE_BIT:
	case MYSQL_TYPE_STRING:
	case MYSQL_TYPE_VAR_STRING:
	case MYSQL_TYPE_TINY_BLOB:
	case MYSQL_TYPE_MEDIUM_BLOB:
	case MYSQL_TYPE_BLOB:
	case MYSQL_TYPE_LONG_BLOB:
	case MYSQL_TYPE_VARCHAR:
		/* Use the charset number to pick the right charset struct for
		the comparison. Since the MySQL function get_charset may be
		slow before Bar removes the mutex operation there, we first
		look at 2 common charsets directly. */

		if (charset_number == default_charset_info->number) {
			charset = default_charset_info;
		} else if (charset_number == my_charset_latin1.number) {
			charset = &my_charset_latin1;
		} else {
			charset = get_charset(charset_number, MYF(MY_WME));

			if (charset == NULL) {
			  sql_print_error("InnoDB needs charset %lu for doing "
					  "a comparison, but MySQL cannot "
					  "find that charset.",
					  (ulong) charset_number);
				ut_a(0);
			}
		}

		/* Starting from 4.1.3, we use strnncollsp() in comparisons of
		non-latin1_swedish_ci strings. NOTE that the collation order
		changes then: 'b\0\0...' is ordered BEFORE 'b  ...'. Users
		having indexes on such data need to rebuild their tables! */

		ret = charset->coll->strnncollsp(charset,
				  a, a_length,
						 b, b_length, 0);
		if (ret < 0) {
			return(-1);
		} else if (ret > 0) {
			return(1);
		} else {
			return(0);
		}
	default:
		ut_error;
	}

	return(0);
}

/**************************************************************//**
Converts a MySQL type to an InnoDB type. Note that this function returns
the 'mtype' of InnoDB. InnoDB differentiates between MySQL's old <= 4.1
VARCHAR and the new true VARCHAR in >= 5.0.3 by the 'prtype'.
@return	DATA_BINARY, DATA_VARCHAR, ... */
extern "C" UNIV_INTERN
ulint
get_innobase_type_from_mysql_type(
/*==============================*/
	ulint*		unsigned_flag,	/*!< out: DATA_UNSIGNED if an
					'unsigned type';
					at least ENUM and SET,
					and unsigned integer
					types are 'unsigned types' */
	const void*	f)		/*!< in: MySQL Field */
{
	const class Field* field = reinterpret_cast<const class Field*>(f);

	/* The following asserts try to check that the MySQL type code fits in
	8 bits: this is used in ibuf and also when DATA_NOT_NULL is ORed to
	the type */

	DBUG_ASSERT((ulint)MYSQL_TYPE_STRING < 256);
	DBUG_ASSERT((ulint)MYSQL_TYPE_VAR_STRING < 256);
	DBUG_ASSERT((ulint)MYSQL_TYPE_DOUBLE < 256);
	DBUG_ASSERT((ulint)MYSQL_TYPE_FLOAT < 256);
	DBUG_ASSERT((ulint)MYSQL_TYPE_DECIMAL < 256);

	if (field->flags & UNSIGNED_FLAG) {

		*unsigned_flag = DATA_UNSIGNED;
	} else {
		*unsigned_flag = 0;
	}

	if (field->real_type() == MYSQL_TYPE_ENUM
		|| field->real_type() == MYSQL_TYPE_SET) {

		/* MySQL has field->type() a string type for these, but the
		data is actually internally stored as an unsigned integer
		code! */

		*unsigned_flag = DATA_UNSIGNED; /* MySQL has its own unsigned
						flag set to zero, even though
						internally this is an unsigned
						integer type */
		return(DATA_INT);
	}

	switch (field->type()) {
		/* NOTE that we only allow string types in DATA_MYSQL and
		DATA_VARMYSQL */
	case MYSQL_TYPE_VAR_STRING: /* old <= 4.1 VARCHAR */
	case MYSQL_TYPE_VARCHAR:    /* new >= 5.0.3 true VARCHAR */
		if (field->binary()) {
			return(DATA_BINARY);
		} else if (strcmp(
				   field->charset()->name,
				   "latin1_swedish_ci") == 0) {
			return(DATA_VARCHAR);
		} else {
			return(DATA_VARMYSQL);
		}
	case MYSQL_TYPE_BIT:
	case MYSQL_TYPE_STRING: if (field->binary()) {

			return(DATA_FIXBINARY);
		} else if (strcmp(
				   field->charset()->name,
				   "latin1_swedish_ci") == 0) {
			return(DATA_CHAR);
		} else {
			return(DATA_MYSQL);
		}
	case MYSQL_TYPE_NEWDECIMAL:
		return(DATA_FIXBINARY);
	case MYSQL_TYPE_LONG:
	case MYSQL_TYPE_LONGLONG:
	case MYSQL_TYPE_TINY:
	case MYSQL_TYPE_SHORT:
	case MYSQL_TYPE_INT24:
	case MYSQL_TYPE_DATE:
	case MYSQL_TYPE_YEAR:
	case MYSQL_TYPE_NEWDATE:
           return(DATA_INT);

	case MYSQL_TYPE_TIME:
	case MYSQL_TYPE_DATETIME:
	case MYSQL_TYPE_TIMESTAMP:
          /*
            XtraDB should ideally just check field->keytype() and never
            field->type().  The following check is here to only
            change the new hires datetime/timestamp/time fields to
            use DATA_FIXBINARY.  We can't convert this function to
            just test for field->keytype() as then the check if a
            table is compatible will fail for old tables.
          */
           if (field->key_type() == HA_KEYTYPE_BINARY)
             return(DATA_FIXBINARY);
           return(DATA_INT);
	case MYSQL_TYPE_FLOAT:
		return(DATA_FLOAT);
	case MYSQL_TYPE_DOUBLE:
		return(DATA_DOUBLE);
	case MYSQL_TYPE_DECIMAL:
		return(DATA_DECIMAL);
	case MYSQL_TYPE_GEOMETRY:
	case MYSQL_TYPE_TINY_BLOB:
	case MYSQL_TYPE_MEDIUM_BLOB:
	case MYSQL_TYPE_BLOB:
	case MYSQL_TYPE_LONG_BLOB:
		return(DATA_BLOB);
	case MYSQL_TYPE_NULL:
		return(DATA_FIXBINARY);
	default:
		ut_error;
	}

	return(0);
}

/*******************************************************************//**
Writes an unsigned integer value < 64k to 2 bytes, in the little-endian
storage format. */
static inline
void
innobase_write_to_2_little_endian(
/*==============================*/
	byte*	buf,	/*!< in: where to store */
	ulint	val)	/*!< in: value to write, must be < 64k */
{
	ut_a(val < 256 * 256);

	buf[0] = (byte)(val & 0xFF);
	buf[1] = (byte)(val / 256);
}

/*******************************************************************//**
Reads an unsigned integer value < 64k from 2 bytes, in the little-endian
storage format.
@return	value */
static inline
uint
innobase_read_from_2_little_endian(
/*===============================*/
	const uchar*	buf)	/*!< in: from where to read */
{
	return (uint) ((ulint)(buf[0]) + 256 * ((ulint)(buf[1])));
}

/*******************************************************************//**
Stores a key value for a row to a buffer.
@return	key value length as stored in buff */
UNIV_INTERN
uint
ha_innobase::store_key_val_for_row(
/*===============================*/
	uint		keynr,	/*!< in: key number */
	char*		buff,	/*!< in/out: buffer for the key value (in MySQL
				format) */
	uint		buff_len,/*!< in: buffer length */
	const uchar*	record)/*!< in: row in MySQL format */
{
	KEY*		key_info	= table->key_info + keynr;
	KEY_PART_INFO*	key_part	= key_info->key_part;
	KEY_PART_INFO*	end		= key_part + key_info->key_parts;
	char*		buff_start	= buff;
	enum_field_types mysql_type;
	Field*		field;
	ibool		is_null;

	DBUG_ENTER("store_key_val_for_row");

	/* The format for storing a key field in MySQL is the following:

	1. If the column can be NULL, then in the first byte we put 1 if the
	field value is NULL, 0 otherwise.

	2. If the column is of a BLOB type (it must be a column prefix field
	in this case), then we put the length of the data in the field to the
	next 2 bytes, in the little-endian format. If the field is SQL NULL,
	then these 2 bytes are set to 0. Note that the length of data in the
	field is <= column prefix length.

	3. In a column prefix field, prefix_len next bytes are reserved for
	data. In a normal field the max field length next bytes are reserved
	for data. For a VARCHAR(n) the max field length is n. If the stored
	value is the SQL NULL then these data bytes are set to 0.

	4. We always use a 2 byte length for a true >= 5.0.3 VARCHAR. Note that
	in the MySQL row format, the length is stored in 1 or 2 bytes,
	depending on the maximum allowed length. But in the MySQL key value
	format, the length always takes 2 bytes.

	We have to zero-fill the buffer so that MySQL is able to use a
	simple memcmp to compare two key values to determine if they are
	equal. MySQL does this to compare contents of two 'ref' values. */

	bzero(buff, buff_len);

	for (; key_part != end; key_part++) {
		is_null = FALSE;

		if (key_part->null_bit) {
			if (record[key_part->null_offset]
						& key_part->null_bit) {
				*buff = 1;
				is_null = TRUE;
			} else {
				*buff = 0;
			}
			buff++;
		}

		field = key_part->field;
		mysql_type = field->type();

		if (mysql_type == MYSQL_TYPE_VARCHAR) {
						/* >= 5.0.3 true VARCHAR */
			ulint		lenlen;
			ulint		len;
			const byte*	data;
			ulint		key_len;
			ulint		true_len;
			CHARSET_INFO*	cs;
			int		error=0;

			key_len = key_part->length;

			if (is_null) {
				buff += key_len + 2;

				continue;
			}
			cs = field->charset();

			lenlen = (ulint)
				(((Field_varstring*)field)->length_bytes);

			data = row_mysql_read_true_varchar(&len,
				(byte*) (record
				+ (ulint)get_field_offset(table, field)),
				lenlen);

			true_len = len;

			/* For multi byte character sets we need to calculate
			the true length of the key */

			if (len > 0 && cs->mbmaxlen > 1) {
				true_len = (ulint) cs->cset->well_formed_len(cs,
						(const char *) data,
						(const char *) data + len,
                                                (uint) (key_len /
                                                        cs->mbmaxlen),
						&error);
			}

			/* In a column prefix index, we may need to truncate
			the stored value: */

			if (true_len > key_len) {
				true_len = key_len;
			}

			/* The length in a key value is always stored in 2
			bytes */

			row_mysql_store_true_var_len((byte*)buff, true_len, 2);
			buff += 2;

			memcpy(buff, data, true_len);

			/* Note that we always reserve the maximum possible
			length of the true VARCHAR in the key value, though
			only len first bytes after the 2 length bytes contain
			actual data. The rest of the space was reset to zero
			in the bzero() call above. */

			buff += key_len;

		} else if (mysql_type == MYSQL_TYPE_TINY_BLOB
			|| mysql_type == MYSQL_TYPE_MEDIUM_BLOB
			|| mysql_type == MYSQL_TYPE_BLOB
			|| mysql_type == MYSQL_TYPE_LONG_BLOB
			/* MYSQL_TYPE_GEOMETRY data is treated
			as BLOB data in innodb. */
			|| mysql_type == MYSQL_TYPE_GEOMETRY) {

			CHARSET_INFO*	cs;
			ulint		key_len;
			ulint		true_len;
			int		error=0;
			ulint		blob_len;
			const byte*	blob_data;

			ut_a(key_part->key_part_flag & HA_PART_KEY_SEG);

			key_len = key_part->length;

			if (is_null) {
				buff += key_len + 2;

				continue;
			}

			cs = field->charset();

			blob_data = row_mysql_read_blob_ref(&blob_len,
				(byte*) (record
				+ (ulint)get_field_offset(table, field)),
					(ulint) field->pack_length());

			true_len = blob_len;

			ut_a(get_field_offset(table, field)
				== key_part->offset);

			/* For multi byte character sets we need to calculate
			the true length of the key */

			if (blob_len > 0 && cs->mbmaxlen > 1) {
				true_len = (ulint) cs->cset->well_formed_len(cs,
						(const char *) blob_data,
						(const char *) blob_data
							+ blob_len,
                                                (uint) (key_len /
                                                        cs->mbmaxlen),
						&error);
			}

			/* All indexes on BLOB and TEXT are column prefix
			indexes, and we may need to truncate the data to be
			stored in the key value: */

			if (true_len > key_len) {
				true_len = key_len;
			}

			/* MySQL reserves 2 bytes for the length and the
			storage of the number is little-endian */

			innobase_write_to_2_little_endian(
					(byte*)buff, true_len);
			buff += 2;

			memcpy(buff, blob_data, true_len);

			/* Note that we always reserve the maximum possible
			length of the BLOB prefix in the key value. */

			buff += key_len;
		} else {
			/* Here we handle all other data types except the
			true VARCHAR, BLOB and TEXT. Note that the column
			value we store may be also in a column prefix
			index. */

			CHARSET_INFO*		cs;
			ulint			true_len;
			ulint			key_len;
			const uchar*		src_start;
			int			error=0;
			enum_field_types	real_type;

			key_len = key_part->length;

			if (is_null) {
				 buff += key_len;

				 continue;
			}

			src_start = record + key_part->offset;
			real_type = field->real_type();
			true_len = key_len;

			/* Character set for the field is defined only
			to fields whose type is string and real field
			type is not enum or set. For these fields check
			if character set is multi byte. */

			if (real_type != MYSQL_TYPE_ENUM
				&& real_type != MYSQL_TYPE_SET
				&& ( mysql_type == MYSQL_TYPE_VAR_STRING
					|| mysql_type == MYSQL_TYPE_STRING)) {

				cs = field->charset();

				/* For multi byte character sets we need to
				calculate the true length of the key */

				if (key_len > 0 && cs->mbmaxlen > 1) {

					true_len = (ulint)
						cs->cset->well_formed_len(cs,
							(const char *)src_start,
							(const char *)src_start
								+ key_len,
                                                        (uint) (key_len /
                                                                cs->mbmaxlen),
							&error);
				}
			}

			memcpy(buff, src_start, true_len);
			buff += true_len;

			/* Pad the unused space with spaces. */

			if (true_len < key_len) {
				ulint	pad_len = key_len - true_len;
				ut_a(!(pad_len % cs->mbminlen));

				cs->cset->fill(cs, buff, pad_len,
					       0x20 /* space */);
				buff += pad_len;
			}
		}
	}

	ut_a(buff <= buff_start + buff_len);

	DBUG_RETURN((uint)(buff - buff_start));
}

/**************************************************************//**
Determines if a field is needed in a prebuilt struct 'template'.
@return field to use, or NULL if the field is not needed */
static
const Field*
build_template_needs_field(
/*=======================*/
	ibool		index_contains,	/*!< in:
					dict_index_contains_col_or_prefix(
					index, i) */
	ibool		read_just_key,	/*!< in: TRUE when MySQL calls
					ha_innobase::extra with the
					argument HA_EXTRA_KEYREAD; it is enough
					to read just columns defined in
					the index (i.e., no read of the
					clustered index record necessary) */
	ibool		fetch_all_in_key,
					/*!< in: true=fetch all fields in
					the index */
	ibool		fetch_primary_key_cols,
					/*!< in: true=fetch the
					primary key columns */
	dict_index_t*	index,		/*!< in: InnoDB index to use */
	const TABLE*	table,		/*!< in: MySQL table object */
	ulint		i,		/*!< in: field index in InnoDB table */
	ulint		sql_idx)	/*!< in: field index in SQL table */
{
	const Field*	field	= table->field[sql_idx];

	ut_ad(index_contains == dict_index_contains_col_or_prefix(index, i));

	if (!index_contains) {
		if (read_just_key) {
			/* If this is a 'key read', we do not need
			columns that are not in the key */

			return(NULL);
		}
	} else if (fetch_all_in_key) {
		/* This field is needed in the query */

		return(field);
	}

	if (bitmap_is_set(table->read_set, sql_idx)
	    || bitmap_is_set(table->write_set, sql_idx)) {
		/* This field is needed in the query */

		return(field);
	}

	if (fetch_primary_key_cols
	    && dict_table_col_in_clustered_key(index->table, i)) {
		/* This field is needed in the query */

		return(field);
	}

	/* This field is not needed in the query, skip it */

	return(NULL);
}

/**************************************************************//**
Adds a field is to a prebuilt struct 'template'.
@return the field template */
static
mysql_row_templ_t*
build_template_field(
/*=================*/
	row_prebuilt_t*	prebuilt,	/*!< in/out: template */
	dict_index_t*	clust_index,	/*!< in: InnoDB clustered index */
	dict_index_t*	index,		/*!< in: InnoDB index to use */
	TABLE*		table,		/*!< in: MySQL table object */
	const Field*	field,		/*!< in: field in MySQL table */
	ulint		i)		/*!< in: field index in InnoDB table */
{
	mysql_row_templ_t*	templ;
	const dict_col_t*	col;

	//ut_ad(field == table->field[i]);
	ut_ad(clust_index->table == index->table);

	col = dict_table_get_nth_col(index->table, i);

	templ = prebuilt->mysql_template + prebuilt->n_template++;
	UNIV_MEM_INVALID(templ, sizeof *templ);
	templ->col_no = i;
	templ->clust_rec_field_no = dict_col_get_clust_pos(col, clust_index);
	ut_a(templ->clust_rec_field_no != ULINT_UNDEFINED);

	if (dict_index_is_clust(index)) {
		templ->rec_field_no = templ->clust_rec_field_no;
	} else {
		templ->rec_field_no = dict_index_get_nth_col_pos(index, i);
	}

	if (field->null_ptr) {
		templ->mysql_null_byte_offset =
			(ulint) ((char*) field->null_ptr
				 - (char*) table->record[0]);

		templ->mysql_null_bit_mask = (ulint) field->null_bit;
	} else {
		templ->mysql_null_bit_mask = 0;
	}

	templ->mysql_col_offset = (ulint) get_field_offset(table, field);

	templ->mysql_col_len = (ulint) field->pack_length();
	templ->type = col->mtype;
	templ->mysql_type = (ulint)field->type();

	if (templ->mysql_type == DATA_MYSQL_TRUE_VARCHAR) {
		templ->mysql_length_bytes = (ulint)
			(((Field_varstring*)field)->length_bytes);
	}

	templ->charset = dtype_get_charset_coll(col->prtype);
	templ->mbminlen = DATA_MBMINLEN(col->mbminmaxlen);
	templ->mbmaxlen = DATA_MBMAXLEN(col->mbminmaxlen);
	templ->is_unsigned = col->prtype & DATA_UNSIGNED;

	if (!dict_index_is_clust(index)
	    && templ->rec_field_no == ULINT_UNDEFINED) {
		prebuilt->need_to_access_clustered = TRUE;
	}

	if (prebuilt->mysql_prefix_len < templ->mysql_col_offset
	    + templ->mysql_col_len) {
		prebuilt->mysql_prefix_len = templ->mysql_col_offset
			+ templ->mysql_col_len;
	}

	if (templ->type == DATA_BLOB) {
		prebuilt->templ_contains_blob = TRUE;
	}

	return(templ);
}

/**************************************************************//**
Builds a 'template' to the prebuilt struct. The template is used in fast
retrieval of just those column values MySQL needs in its processing. */
UNIV_INTERN
void
ha_innobase::build_template(
/*========================*/
	bool		whole_row)	/*!< in: true=ROW_MYSQL_WHOLE_ROW,
					false=ROW_MYSQL_REC_FIELDS */
{
	dict_index_t*	index;
	dict_index_t*	clust_index;
	ulint		n_stored_fields;
	ibool		fetch_all_in_key	= FALSE;
	ibool		fetch_primary_key_cols	= FALSE;
	ulint		i, sql_idx;
       
	if (prebuilt->select_lock_type == LOCK_X) {
		/* We always retrieve the whole clustered index record if we
		use exclusive row level locks, for example, if the read is
		done in an UPDATE statement. */

		whole_row = true;
	} else if (!whole_row) {
		if (prebuilt->hint_need_to_fetch_extra_cols
			== ROW_RETRIEVE_ALL_COLS) {

			/* We know we must at least fetch all columns in the
			key, or all columns in the table */

			if (prebuilt->read_just_key) {
				/* MySQL has instructed us that it is enough
				to fetch the columns in the key; looks like
				MySQL can set this flag also when there is
				only a prefix of the column in the key: in
				that case we retrieve the whole column from
				the clustered index */

				fetch_all_in_key = TRUE;
			} else {
				whole_row = true;
			}
		} else if (prebuilt->hint_need_to_fetch_extra_cols
			== ROW_RETRIEVE_PRIMARY_KEY) {
			/* We must at least fetch all primary key cols. Note
			   that if the clustered index was internally generated
			   by InnoDB on the row id (no primary key was
			   defined), then row_search_for_mysql() will always
			   retrieve the row id to a special buffer in the
			   prebuilt struct. */

			fetch_primary_key_cols = TRUE;
		}
	}

	clust_index = dict_table_get_first_index(prebuilt->table);

	index = whole_row ? clust_index : prebuilt->index;

	prebuilt->need_to_access_clustered = (index == clust_index);

	/* Below we check column by column if we need to access
	the clustered index. */

	n_stored_fields= (ulint)table->s->stored_fields; /* number of stored columns */

	if (!prebuilt->mysql_template) {
		prebuilt->mysql_template = (mysql_row_templ_t*)
			mem_alloc(n_stored_fields * sizeof(mysql_row_templ_t));
	}

	prebuilt->template_type = whole_row
		? ROW_MYSQL_WHOLE_ROW : ROW_MYSQL_REC_FIELDS;
	prebuilt->null_bitmap_len = table->s->null_bytes;

	/* Prepare to build prebuilt->mysql_template[]. */
	prebuilt->templ_contains_blob = FALSE;
	prebuilt->mysql_prefix_len = 0;
	prebuilt->n_template = 0;
	prebuilt->idx_cond_n_cols = 0;

	/* Note that in InnoDB, i is the column number in the table.
	MySQL calls columns 'fields'. */

	if (active_index != MAX_KEY && active_index == pushed_idx_cond_keyno) {
		/* Push down an index condition or an end_range check. */
	  for (i = 0, sql_idx = 0; i < n_stored_fields; i++, sql_idx++) {

	                while (!table->field[sql_idx]->stored_in_db) {
			        sql_idx++;     
                        }
                       
			const ibool		index_contains
				= dict_index_contains_col_or_prefix(index, i);

			/* Test if an end_range or an index condition
			refers to the field. Note that "index" and
			"index_contains" may refer to the clustered index.
			Index condition pushdown is relative to prebuilt->index
			(the index that is being looked up first). */

			/* When join_read_always_key() invokes this
			code via handler::ha_index_init() and
			ha_innobase::index_init(), end_range is not
			yet initialized. Because of that, we must
			always check for index_contains, instead of
			the subset
			field->part_of_key.is_set(active_index)
			which would be acceptable if end_range==NULL. */
			if (index == prebuilt->index
				? index_contains
				: dict_index_contains_col_or_prefix(
					prebuilt->index, i)) {
				/* Needed in ICP */
				const Field*		field;
				mysql_row_templ_t*	templ;

				if (whole_row) {
					field = table->field[sql_idx];
				} else {
					field = build_template_needs_field(
						index_contains,
						prebuilt->read_just_key,
						fetch_all_in_key,
						fetch_primary_key_cols,
						index, table, i, sql_idx);
					if (!field) {
						continue;
					}
				}

				templ = build_template_field(
					prebuilt, clust_index, index,
					table, field, i);
				prebuilt->idx_cond_n_cols++;
				ut_ad(prebuilt->idx_cond_n_cols
				      == prebuilt->n_template);

				if (index == prebuilt->index) {
					templ->icp_rec_field_no
						= templ->rec_field_no;
				} else {
					templ->icp_rec_field_no
						= dict_index_get_nth_col_pos(
							prebuilt->index, i);
				}

				if (dict_index_is_clust(prebuilt->index)) {
					ut_ad(templ->icp_rec_field_no
					      != ULINT_UNDEFINED);
					/* If the primary key includes
					a column prefix, use it in
					index condition pushdown,
					because the condition is
					evaluated before fetching any
					off-page (externally stored)
					columns. */
					if (templ->icp_rec_field_no
					    < prebuilt->index->n_uniq) {
						/* This is a key column;
						all set. */
						continue;
					}
				} else if (templ->icp_rec_field_no
					   != ULINT_UNDEFINED) {
					continue;
				}

				/* This is a column prefix index.
				The column prefix can be used in
				an end_range comparison. */

				templ->icp_rec_field_no
					= dict_index_get_nth_col_or_prefix_pos(
						prebuilt->index, i, TRUE);
				ut_ad(templ->icp_rec_field_no
				      != ULINT_UNDEFINED);

				/* Index condition pushdown can be used on
				all columns of a secondary index, and on
				the PRIMARY KEY columns. */
				/* TODO: enable this assertion
				(but first ensure that end_range is
				valid here and use an accurate condition
				for end_range) 
				ut_ad(!dict_index_is_clust(prebuilt->index)
				      || templ->rec_field_no
				      < prebuilt->index->n_uniq);
				*/
			}
		}

		ut_ad(prebuilt->idx_cond_n_cols > 0);
		ut_ad(prebuilt->idx_cond_n_cols == prebuilt->n_template);

		/* Include the fields that are not needed in index condition
		pushdown. */
                for (i = 0, sql_idx = 0; i < n_stored_fields; i++, sql_idx++) {

		        while (!table->field[sql_idx]->stored_in_db) {
			        sql_idx++;     
                        }
                       
			const ibool		index_contains
				= dict_index_contains_col_or_prefix(index, i);

			if (index == prebuilt->index
				? !index_contains
				: !dict_index_contains_col_or_prefix(
					prebuilt->index, i)) {
				/* Not needed in ICP */
				const Field*	field;

				if (whole_row) {
					field = table->field[sql_idx];
				} else {
					field = build_template_needs_field(
						index_contains,
						prebuilt->read_just_key,
						fetch_all_in_key,
						fetch_primary_key_cols,
						index, table, i, sql_idx);
					if (!field) {
						continue;
					}
				}

				build_template_field(prebuilt,
						     clust_index, index,
						     table, field, i);
			}
		}

		prebuilt->idx_cond = this;
 	} else {
		/* No index condition pushdown */
		prebuilt->idx_cond = NULL;

                for (i = 0, sql_idx = 0; i < n_stored_fields; i++, sql_idx++) {
			const Field*	field;

	                while (!table->field[sql_idx]->stored_in_db) {
			        sql_idx++;     
                        }

			if (whole_row) {
				field = table->field[sql_idx];
			} else {
				field = build_template_needs_field(
					dict_index_contains_col_or_prefix(
						index, i),
					prebuilt->read_just_key,
					fetch_all_in_key,
					fetch_primary_key_cols,
					index, table, i, sql_idx);
				if (!field) {
					continue;
				}
			}

			build_template_field(prebuilt, clust_index, index,
					     table, field, i);
		}
        }

	if (index != clust_index && prebuilt->need_to_access_clustered) {
		/* Change rec_field_no's to correspond to the clustered index
		record */
		for (i = 0; i < prebuilt->n_template; i++) {
			mysql_row_templ_t*	templ
				= &prebuilt->mysql_template[i];
			templ->rec_field_no = templ->clust_rec_field_no;
		}
	}
}

/********************************************************************//**
This special handling is really to overcome the limitations of MySQL's
binlogging. We need to eliminate the non-determinism that will arise in
INSERT ... SELECT type of statements, since MySQL binlog only stores the
min value of the autoinc interval. Once that is fixed we can get rid of
the special lock handling.
@return	DB_SUCCESS if all OK else error code */
UNIV_INTERN
ulint
ha_innobase::innobase_lock_autoinc(void)
/*====================================*/
{
	ulint		error = DB_SUCCESS;

	switch (innobase_autoinc_lock_mode) {
	case AUTOINC_NO_LOCKING:
		/* Acquire only the AUTOINC mutex. */
		dict_table_autoinc_lock(prebuilt->table);
		break;

	case AUTOINC_NEW_STYLE_LOCKING:
		/* For simple (single/multi) row INSERTs/REPLACEs and RBR
		events, we fallback to the old style only if another
		transaction has already acquired the AUTOINC lock on
		behalf of a LOAD FILE or INSERT ... SELECT etc. type of
		statement. */
		if (thd_sql_command(user_thd) == SQLCOM_INSERT
		    || thd_sql_command(user_thd) == SQLCOM_REPLACE
		    || thd_sql_command(user_thd) == SQLCOM_END // RBR event
		) {
			dict_table_t*	table = prebuilt->table;

			/* Acquire the AUTOINC mutex. */
			dict_table_autoinc_lock(table);

			/* We need to check that another transaction isn't
			already holding the AUTOINC lock on the table. */
			if (table->n_waiting_or_granted_auto_inc_locks) {
				/* Release the mutex to avoid deadlocks and
				fall back to old style locking. */
				dict_table_autoinc_unlock(table);
			} else {
				/* Do not fall back to old style locking. */
				break;
			}
		}
		/* Fall through to old style locking. */

	case AUTOINC_OLD_STYLE_LOCKING:
		error = row_lock_table_autoinc_for_mysql(prebuilt);

		if (error == DB_SUCCESS) {

			/* Acquire the AUTOINC mutex. */
			dict_table_autoinc_lock(prebuilt->table);
		}
		break;

	default:
		ut_error;
	}

	return(ulong(error));
}

/********************************************************************//**
Reset the autoinc value in the table.
@return	DB_SUCCESS if all went well else error code */
UNIV_INTERN
ulint
ha_innobase::innobase_reset_autoinc(
/*================================*/
	ulonglong	autoinc)	/*!< in: value to store */
{
	ulint		error;

	error = innobase_lock_autoinc();

	if (error == DB_SUCCESS) {

		dict_table_autoinc_initialize(prebuilt->table, autoinc);

		dict_table_autoinc_unlock(prebuilt->table);
	}

	return(ulong(error));
}

/********************************************************************//**
Store the autoinc value in the table. The autoinc value is only set if
it's greater than the existing autoinc value in the table.
@return	DB_SUCCESS if all went well else error code */
UNIV_INTERN
ulint
ha_innobase::innobase_set_max_autoinc(
/*==================================*/
	ulonglong	auto_inc)	/*!< in: value to store */
{
	ulint		error;

	error = innobase_lock_autoinc();

	if (error == DB_SUCCESS) {

		dict_table_autoinc_update_if_greater(prebuilt->table, auto_inc);

		dict_table_autoinc_unlock(prebuilt->table);
	}

	return(ulong(error));
}

/********************************************************************//**
Stores a row in an InnoDB database, to the table specified in this
handle.
@return	error code */
UNIV_INTERN
int
ha_innobase::write_row(
/*===================*/
	uchar*	record)	/*!< in: a row in MySQL format */
{
	ulint		error = 0;
        int             error_result= 0;
	ibool		auto_inc_used= FALSE;
	ulint		sql_command;
	trx_t*		trx = thd_to_trx(user_thd);

	DBUG_ENTER("ha_innobase::write_row");

	if (prebuilt->trx != trx) {
		sql_print_error("The transaction object for the table handle is at "
			"%p, but for the current thread it is at %p",
			(const void*) prebuilt->trx, (const void*) trx);

		fputs("InnoDB: Dump of 200 bytes around prebuilt: ", stderr);
		ut_print_buf(stderr, ((const byte*)prebuilt) - 100, 200);
		fputs("\n"
			"InnoDB: Dump of 200 bytes around ha_data: ",
			stderr);
		ut_print_buf(stderr, ((const byte*) trx) - 100, 200);
		putc('\n', stderr);
		ut_error;
	}

	ha_statistic_increment(&SSV::ha_write_count);

	if (!share->ib_table || share->ib_table->is_corrupt) {
		DBUG_RETURN(HA_ERR_CRASHED);
	}

	if (table->timestamp_field_type & TIMESTAMP_AUTO_SET_ON_INSERT)
		table->timestamp_field->set_time();

	sql_command = thd_sql_command(user_thd);

	if ((sql_command == SQLCOM_ALTER_TABLE
	     || sql_command == SQLCOM_OPTIMIZE
	     || sql_command == SQLCOM_CREATE_INDEX
	     || sql_command == SQLCOM_DROP_INDEX)
	    && num_write_row >= 10000) {
		/* ALTER TABLE is COMMITted at every 10000 copied rows.
		The IX table lock for the original table has to be re-issued.
		As this method will be called on a temporary table where the
		contents of the original table is being copied to, it is
		a bit tricky to determine the source table.  The cursor
		position in the source table need not be adjusted after the
		intermediate COMMIT, since writes by other transactions are
		being blocked by a MySQL table lock TL_WRITE_ALLOW_READ. */

		dict_table_t*	src_table;
		enum lock_mode	mode;

		num_write_row = 0;

		/* Commit the transaction.  This will release the table
		locks, so they have to be acquired again. */

		/* Altering an InnoDB table */
		/* Get the source table. */
		src_table = lock_get_src_table(
				prebuilt->trx, prebuilt->table, &mode);
		if (!src_table) {
no_commit:
			/* Unknown situation: do not commit */
			/*
			ut_print_timestamp(stderr);
			fprintf(stderr,
				"  InnoDB: ALTER TABLE is holding lock"
				" on %lu tables!\n",
				prebuilt->trx->mysql_n_tables_locked);
			*/
			;
		} else if (src_table == prebuilt->table) {
			/* Source table is not in InnoDB format:
			no need to re-acquire locks on it. */

			/* Altering to InnoDB format */
			innobase_commit(ht, user_thd, 1);
			/* Note that this transaction is still active. */
			trx_register_for_2pc(prebuilt->trx);
			/* We will need an IX lock on the destination table. */
			prebuilt->sql_stat_start = TRUE;
		} else {
			/* Ensure that there are no other table locks than
			LOCK_IX and LOCK_AUTO_INC on the destination table. */

			if (!lock_is_table_exclusive(prebuilt->table,
							prebuilt->trx)) {
				goto no_commit;
			}

			/* Commit the transaction.  This will release the table
			locks, so they have to be acquired again. */
			innobase_commit(ht, user_thd, 1);
			/* Note that this transaction is still active. */
			trx_register_for_2pc(prebuilt->trx);
			/* Re-acquire the table lock on the source table. */
			row_lock_table_for_mysql(prebuilt, src_table, mode);
			/* We will need an IX lock on the destination table. */
			prebuilt->sql_stat_start = TRUE;
		}
	}

	num_write_row++;

	/* This is the case where the table has an auto-increment column */
	if (table->next_number_field && record == table->record[0]) {

		/* Reset the error code before calling
		innobase_get_auto_increment(). */
		prebuilt->autoinc_error = DB_SUCCESS;

		if ((error = update_auto_increment())) {
			/* We don't want to mask autoinc overflow errors. */

			/* Handle the case where the AUTOINC sub-system
			failed during initialization. */
			if (prebuilt->autoinc_error == DB_UNSUPPORTED) {
				error_result = ER_AUTOINC_READ_FAILED;
				/* Set the error message to report too. */
				my_error(ER_AUTOINC_READ_FAILED, MYF(0));
				goto func_exit;
			} else if (prebuilt->autoinc_error != DB_SUCCESS) {
				error = (int) prebuilt->autoinc_error;
				goto report_error;
			}

			/* MySQL errors are passed straight back. except for
                           HA_ERR_AUTO_INC_READ_FAILED. This can only happen
                           for values out of range.
                         */
			error_result = (int) error;
			goto func_exit;
		}

		auto_inc_used = TRUE;
	}

	if (prebuilt->mysql_template == NULL
	    || prebuilt->template_type != ROW_MYSQL_WHOLE_ROW) {

		/* Build the template used in converting quickly between
		the two database formats */

		build_template(true);
	}

	innodb_srv_conc_enter_innodb(prebuilt->trx);

	error = row_insert_for_mysql((byte*) record, prebuilt);

#ifdef EXTENDED_FOR_USERSTAT
	if (UNIV_LIKELY(error == DB_SUCCESS && !trx->fake_changes)) {
		rows_changed++;
	}
#endif

	/* Handle duplicate key errors */
	if (auto_inc_used) {
		ulint		err;
		ulonglong	auto_inc;
		ulonglong	col_max_value;

		/* Note the number of rows processed for this statement, used
		by get_auto_increment() to determine the number of AUTO-INC
		values to reserve. This is only useful for a mult-value INSERT
		and is a statement level counter.*/
		if (trx->n_autoinc_rows > 0) {
			--trx->n_autoinc_rows;
		}

		/* We need the upper limit of the col type to check for
		whether we update the table autoinc counter or not. */
		col_max_value = innobase_get_int_col_max_value(
			table->next_number_field);

		/* Get the value that MySQL attempted to store in the table.*/
		auto_inc = table->next_number_field->val_int();

		switch (error) {
		case DB_DUPLICATE_KEY:

			/* A REPLACE command and LOAD DATA INFILE REPLACE
			handle a duplicate key error themselves, but we
			must update the autoinc counter if we are performing
			those statements. */

			switch (sql_command) {
			case SQLCOM_LOAD:
				if (trx->duplicates) {

					goto set_max_autoinc;
				}
				break;

			case SQLCOM_REPLACE:
			case SQLCOM_INSERT_SELECT:
			case SQLCOM_REPLACE_SELECT:
				goto set_max_autoinc;

			default:
				break;
			}

			break;

		case DB_SUCCESS:
			/* If the actual value inserted is greater than
			the upper limit of the interval, then we try and
			update the table upper limit. Note: last_value
			will be 0 if get_auto_increment() was not called.*/

			if (auto_inc >= prebuilt->autoinc_last_value) {
set_max_autoinc:
				/* This should filter out the negative
				values set explicitly by the user. */
				if (auto_inc <= col_max_value) {
					ut_a(prebuilt->autoinc_increment > 0);

					ulonglong	offset;
					ulonglong	increment;

					offset = prebuilt->autoinc_offset;
					increment = prebuilt->autoinc_increment;

					auto_inc = innobase_next_autoinc(
						auto_inc,
						1, increment, offset,
						col_max_value);

					err = innobase_set_max_autoinc(
						auto_inc);

					if (err != DB_SUCCESS) {
						error = err;
					}
				}
			}
			break;
		}
	}

	innodb_srv_conc_exit_innodb(prebuilt->trx);

report_error:
	error_result = convert_error_code_to_mysql((int) error,
						   prebuilt->table->flags,
						   user_thd);

func_exit:
	innobase_active_small();

	if (!share->ib_table || share->ib_table->is_corrupt) {
		DBUG_RETURN(HA_ERR_CRASHED);
	}

	DBUG_RETURN(error_result);
}

/**********************************************************************//**
Checks which fields have changed in a row and stores information
of them to an update vector.
@return	error number or 0 */
static
int
calc_row_difference(
/*================*/
	upd_t*		uvect,		/*!< in/out: update vector */
	uchar*		old_row,	/*!< in: old row in MySQL format */
	uchar*		new_row,	/*!< in: new row in MySQL format */
	TABLE*		table,		/*!< in: table in MySQL data
					dictionary */
	uchar*		upd_buff,	/*!< in: buffer to use */
	ulint		buff_len,	/*!< in: buffer length */
	row_prebuilt_t*	prebuilt,	/*!< in: InnoDB prebuilt struct */
	THD*		thd)		/*!< in: user thread */
{
	uchar*		original_upd_buff = upd_buff;
	Field*		field;
	enum_field_types field_mysql_type;
	uint		n_fields;
	ulint		o_len;
	ulint		n_len;
	ulint		col_pack_len;
	const byte*	new_mysql_row_col;
	const byte*	o_ptr;
	const byte*	n_ptr;
	byte*		buf;
	upd_field_t*	ufield;
	ulint		col_type;
	ulint		n_changed = 0;
	dfield_t	dfield;
	dict_index_t*	clust_index;
	uint		sql_idx, innodb_idx= 0;

	n_fields = table->s->fields;
	clust_index = dict_table_get_first_index(prebuilt->table);

	/* We use upd_buff to convert changed fields */
	buf = (byte*) upd_buff;

	for (sql_idx = 0; sql_idx < n_fields; sql_idx++) {
		field = table->field[sql_idx];
		if (!field->stored_in_db)
		  continue;

		o_ptr = (const byte*) old_row + get_field_offset(table, field);
		n_ptr = (const byte*) new_row + get_field_offset(table, field);

		/* Use new_mysql_row_col and col_pack_len save the values */

		new_mysql_row_col = n_ptr;
		col_pack_len = field->pack_length();

		o_len = col_pack_len;
		n_len = col_pack_len;

		/* We use o_ptr and n_ptr to dig up the actual data for
		comparison. */

		field_mysql_type = field->type();

		col_type = prebuilt->table->cols[innodb_idx].mtype;

		switch (col_type) {

		case DATA_BLOB:
			o_ptr = row_mysql_read_blob_ref(&o_len, o_ptr, o_len);
			n_ptr = row_mysql_read_blob_ref(&n_len, n_ptr, n_len);

			break;

		case DATA_VARCHAR:
		case DATA_BINARY:
		case DATA_VARMYSQL:
			if (field_mysql_type == MYSQL_TYPE_VARCHAR) {
				/* This is a >= 5.0.3 type true VARCHAR where
				the real payload data length is stored in
				1 or 2 bytes */

				o_ptr = row_mysql_read_true_varchar(
					&o_len, o_ptr,
					(ulint)
					(((Field_varstring*)field)->length_bytes));

				n_ptr = row_mysql_read_true_varchar(
					&n_len, n_ptr,
					(ulint)
					(((Field_varstring*)field)->length_bytes));
			}

			break;
		default:
			;
		}

		if (field->null_ptr) {
			if (field_in_record_is_null(table, field,
							(char*) old_row)) {
				o_len = UNIV_SQL_NULL;
			}

			if (field_in_record_is_null(table, field,
							(char*) new_row)) {
				n_len = UNIV_SQL_NULL;
			}
		}

		if (o_len != n_len || (o_len != UNIV_SQL_NULL &&
					0 != memcmp(o_ptr, n_ptr, o_len))) {
			/* The field has changed */

			ufield = uvect->fields + n_changed;
			UNIV_MEM_INVALID(ufield, sizeof *ufield);

			/* Let us use a dummy dfield to make the conversion
			from the MySQL column format to the InnoDB format */

			if (n_len != UNIV_SQL_NULL) {
				dict_col_copy_type(prebuilt->table->cols + innodb_idx,
						   dfield_get_type(&dfield));

				buf = row_mysql_store_col_in_innobase_format(
					&dfield,
					(byte*)buf,
					TRUE,
					new_mysql_row_col,
					col_pack_len,
					dict_table_is_comp(prebuilt->table));
				dfield_copy(&ufield->new_val, &dfield);
			} else {
				dfield_set_null(&ufield->new_val);
			}

			ufield->exp = NULL;
			ufield->orig_len = 0;
			ufield->field_no = dict_col_get_clust_pos(
				&prebuilt->table->cols[innodb_idx], clust_index);
			n_changed++;
		}
                if (field->stored_in_db)
                  innodb_idx++;
	}

	uvect->n_fields = n_changed;
	uvect->info_bits = 0;

	ut_a(buf <= (byte*)original_upd_buff + buff_len);

	return(0);
}

/**********************************************************************//**
Updates a row given as a parameter to a new value. Note that we are given
whole rows, not just the fields which are updated: this incurs some
overhead for CPU when we check which fields are actually updated.
TODO: currently InnoDB does not prevent the 'Halloween problem':
in a searched update a single row can get updated several times
if its index columns are updated!
@return	error number or 0 */
UNIV_INTERN
int
ha_innobase::update_row(
/*====================*/
	const uchar*	old_row,	/*!< in: old row in MySQL format */
	uchar*		new_row)	/*!< in: new row in MySQL format */
{
	upd_t*		uvect;
	int		error = 0;
	trx_t*		trx = thd_to_trx(user_thd);

	DBUG_ENTER("ha_innobase::update_row");

	ut_a(prebuilt->trx == trx);

	if (upd_buf == NULL) {
		ut_ad(upd_buf_size == 0);

		/* Create a buffer for packing the fields of a record. Why
		table->stored_rec_length did not work here? Obviously, because char
		fields when packed actually became 1 byte longer, when we also
		stored the string length as the first byte. */

		upd_buf_size = table->s->stored_rec_length + table->s->max_key_length
			+ MAX_REF_PARTS * 3;
		upd_buf = (uchar*) my_malloc(upd_buf_size, MYF(MY_WME));
		if (upd_buf == NULL) {
			upd_buf_size = 0;
			DBUG_RETURN(HA_ERR_OUT_OF_MEM);
		}
	}

	ha_statistic_increment(&SSV::ha_update_count);

	if (!share->ib_table || share->ib_table->is_corrupt) {
		DBUG_RETURN(HA_ERR_CRASHED);
	}

	if (table->timestamp_field_type & TIMESTAMP_AUTO_SET_ON_UPDATE)
		table->timestamp_field->set_time();

	if (prebuilt->upd_node) {
		uvect = prebuilt->upd_node->update;
	} else {
		uvect = row_get_prebuilt_update_vector(prebuilt);
	}

	/* Build an update vector from the modified fields in the rows
	(uses upd_buf of the handle) */

	calc_row_difference(uvect, (uchar*) old_row, new_row, table,
			    upd_buf, upd_buf_size, prebuilt, user_thd);

	/* This is not a delete */
	prebuilt->upd_node->is_delete = FALSE;

	ut_a(prebuilt->template_type == ROW_MYSQL_WHOLE_ROW);

	innodb_srv_conc_enter_innodb(trx);

	error = row_update_for_mysql((byte*) old_row, prebuilt);

	/* We need to do some special AUTOINC handling for the following case:

	INSERT INTO t (c1,c2) VALUES(x,y) ON DUPLICATE KEY UPDATE ...

	We need to use the AUTOINC counter that was actually used by
	MySQL in the UPDATE statement, which can be different from the
	value used in the INSERT statement.*/

	if (error == DB_SUCCESS
	    && table->next_number_field
	    && new_row == table->record[0]
	    && thd_sql_command(user_thd) == SQLCOM_INSERT
	    && trx->duplicates)  {

		ulonglong	auto_inc;
		ulonglong	col_max_value;

		auto_inc = table->next_number_field->val_int();

		/* We need the upper limit of the col type to check for
		whether we update the table autoinc counter or not. */
		col_max_value = innobase_get_int_col_max_value(
			table->next_number_field);

		if (auto_inc <= col_max_value && auto_inc != 0) {

			ulonglong	offset;
			ulonglong	increment;

			offset = prebuilt->autoinc_offset;
			increment = prebuilt->autoinc_increment;

			auto_inc = innobase_next_autoinc(
				auto_inc, 1, increment, offset, col_max_value);

			error = innobase_set_max_autoinc(auto_inc);
		}
	}

#ifdef EXTENDED_FOR_USERSTAT
	if (UNIV_LIKELY(error == DB_SUCCESS && !trx->fake_changes)) {
		rows_changed++;
	}
#endif

	innodb_srv_conc_exit_innodb(trx);

	error = convert_error_code_to_mysql(error,
					    prebuilt->table->flags, user_thd);

	if (error == 0 /* success */
	    && uvect->n_fields == 0 /* no columns were updated */) {

		/* This is the same as success, but instructs
		MySQL that the row is not really updated and it
		should not increase the count of updated rows.
		This is fix for http://bugs.mysql.com/29157 */
		error = HA_ERR_RECORD_IS_THE_SAME;
	}

	/* Tell InnoDB server that there might be work for
	utility threads: */

	innobase_active_small();

	if (!share->ib_table || share->ib_table->is_corrupt) {
		DBUG_RETURN(HA_ERR_CRASHED);
	}

	DBUG_RETURN(error);
}

/**********************************************************************//**
Deletes a row given as the parameter.
@return	error number or 0 */
UNIV_INTERN
int
ha_innobase::delete_row(
/*====================*/
	const uchar*	record)	/*!< in: a row in MySQL format */
{
	int		error = 0;
	trx_t*		trx = thd_to_trx(user_thd);

	DBUG_ENTER("ha_innobase::delete_row");

	ut_a(prebuilt->trx == trx);

	ha_statistic_increment(&SSV::ha_delete_count);

	if (!share->ib_table || share->ib_table->is_corrupt) {
		DBUG_RETURN(HA_ERR_CRASHED);
	}

	if (!prebuilt->upd_node) {
		row_get_prebuilt_update_vector(prebuilt);
	}

	/* This is a delete */

	prebuilt->upd_node->is_delete = TRUE;

	innodb_srv_conc_enter_innodb(trx);

	error = row_update_for_mysql((byte*) record, prebuilt);

#ifdef EXTENDED_FOR_USERSTAT
	if (UNIV_LIKELY(error == DB_SUCCESS && !trx->fake_changes)) {
		rows_changed++;
	}
#endif

	innodb_srv_conc_exit_innodb(trx);

	error = convert_error_code_to_mysql(
		error, prebuilt->table->flags, user_thd);

	/* Tell the InnoDB server that there might be work for
	utility threads: */

	innobase_active_small();

	if (!share->ib_table || share->ib_table->is_corrupt) {
		DBUG_RETURN(HA_ERR_CRASHED);
	}

	DBUG_RETURN(error);
}

/**********************************************************************//**
Removes a new lock set on a row, if it was not read optimistically. This can
be called after a row has been read in the processing of an UPDATE or a DELETE
query, if the option innodb_locks_unsafe_for_binlog is set. */
UNIV_INTERN
void
ha_innobase::unlock_row(void)
/*=========================*/
{
	DBUG_ENTER("ha_innobase::unlock_row");

	ut_ad(prebuilt->trx->state == TRX_ACTIVE);

	/* Consistent read does not take any locks, thus there is
	nothing to unlock. */

	if (prebuilt->select_lock_type == LOCK_NONE) {
		DBUG_VOID_RETURN;
	}

	switch (prebuilt->row_read_type) {
	case ROW_READ_WITH_LOCKS:
		if (!srv_locks_unsafe_for_binlog
		    && prebuilt->trx->isolation_level
		    > TRX_ISO_READ_COMMITTED) {
			break;
		}
		/* fall through */
	case ROW_READ_TRY_SEMI_CONSISTENT:
		row_unlock_for_mysql(prebuilt, FALSE);
		break;
	case ROW_READ_DID_SEMI_CONSISTENT:
		prebuilt->row_read_type = ROW_READ_TRY_SEMI_CONSISTENT;
		break;
	}

	DBUG_VOID_RETURN;
}

/* See handler.h and row0mysql.h for docs on this function. */
UNIV_INTERN
bool
ha_innobase::was_semi_consistent_read(void)
/*=======================================*/
{
	return(prebuilt->row_read_type == ROW_READ_DID_SEMI_CONSISTENT);
}

/* See handler.h and row0mysql.h for docs on this function. */
UNIV_INTERN
void
ha_innobase::try_semi_consistent_read(bool yes)
/*===========================================*/
{
	ut_a(prebuilt->trx == thd_to_trx(ha_thd()));

	/* Row read type is set to semi consistent read if this was
	requested by the MySQL and either innodb_locks_unsafe_for_binlog
	option is used or this session is using READ COMMITTED isolation
	level. */

	if (yes
	    && (srv_locks_unsafe_for_binlog
		|| prebuilt->trx->isolation_level <= TRX_ISO_READ_COMMITTED)) {
		prebuilt->row_read_type = ROW_READ_TRY_SEMI_CONSISTENT;
	} else {
		prebuilt->row_read_type = ROW_READ_WITH_LOCKS;
	}
}

/******************************************************************//**
Initializes a handle to use an index.
@return	0 or error number */
UNIV_INTERN
int
ha_innobase::index_init(
/*====================*/
	uint	keynr,	/*!< in: key (index) number */
	bool sorted)	/*!< in: 1 if result MUST be sorted according to index */
{
	DBUG_ENTER("index_init");

	DBUG_RETURN(change_active_index(keynr));
}

/******************************************************************//**
Currently does nothing.
@return	0 */
UNIV_INTERN
int
ha_innobase::index_end(void)
/*========================*/
{
	int	error	= 0;
	DBUG_ENTER("index_end");
	active_index=MAX_KEY;
	in_range_check_pushed_down= FALSE;
	ds_mrr.dsmrr_close();
	DBUG_RETURN(error);
}

/*********************************************************************//**
Converts a search mode flag understood by MySQL to a flag understood
by InnoDB. */
static inline
ulint
convert_search_mode_to_innobase(
/*============================*/
	enum ha_rkey_function	find_flag)
{
	switch (find_flag) {
	case HA_READ_KEY_EXACT:
		/* this does not require the index to be UNIQUE */
		return(PAGE_CUR_GE);
	case HA_READ_KEY_OR_NEXT:
		return(PAGE_CUR_GE);
	case HA_READ_KEY_OR_PREV:
		return(PAGE_CUR_LE);
	case HA_READ_AFTER_KEY:	
		return(PAGE_CUR_G);
	case HA_READ_BEFORE_KEY:
		return(PAGE_CUR_L);
	case HA_READ_PREFIX:
		return(PAGE_CUR_GE);
	case HA_READ_PREFIX_LAST:
		return(PAGE_CUR_LE);
	case HA_READ_PREFIX_LAST_OR_PREV:
		return(PAGE_CUR_LE);
		/* In MySQL-4.0 HA_READ_PREFIX and HA_READ_PREFIX_LAST always
		pass a complete-field prefix of a key value as the search
		tuple. I.e., it is not allowed that the last field would
		just contain n first bytes of the full field value.
		MySQL uses a 'padding' trick to convert LIKE 'abc%'
		type queries so that it can use as a search tuple
		a complete-field-prefix of a key value. Thus, the InnoDB
		search mode PAGE_CUR_LE_OR_EXTENDS is never used.
		TODO: when/if MySQL starts to use also partial-field
		prefixes, we have to deal with stripping of spaces
		and comparison of non-latin1 char type fields in
		innobase_mysql_cmp() to get PAGE_CUR_LE_OR_EXTENDS to
		work correctly. */
	case HA_READ_MBR_CONTAIN:
	case HA_READ_MBR_INTERSECT:
	case HA_READ_MBR_WITHIN:
	case HA_READ_MBR_DISJOINT:
	case HA_READ_MBR_EQUAL:
		return(PAGE_CUR_UNSUPP);
	/* do not use "default:" in order to produce a gcc warning:
	enumeration value '...' not handled in switch
	(if -Wswitch or -Wall is used) */
	}

	my_error(ER_CHECK_NOT_IMPLEMENTED, MYF(0), "this functionality");

	return(PAGE_CUR_UNSUPP);
}

/*
   BACKGROUND INFO: HOW A SELECT SQL QUERY IS EXECUTED
   ---------------------------------------------------
The following does not cover all the details, but explains how we determine
the start of a new SQL statement, and what is associated with it.

For each table in the database the MySQL interpreter may have several
table handle instances in use, also in a single SQL query. For each table
handle instance there is an InnoDB  'prebuilt' struct which contains most
of the InnoDB data associated with this table handle instance.

  A) if the user has not explicitly set any MySQL table level locks:

  1) MySQL calls ::external_lock to set an 'intention' table level lock on
the table of the handle instance. There we set
prebuilt->sql_stat_start = TRUE. The flag sql_stat_start should be set
true if we are taking this table handle instance to use in a new SQL
statement issued by the user. We also increment trx->n_mysql_tables_in_use.

  2) If prebuilt->sql_stat_start == TRUE we 'pre-compile' the MySQL search
instructions to prebuilt->template of the table handle instance in
::index_read. The template is used to save CPU time in large joins.

  3) In row_search_for_mysql, if prebuilt->sql_stat_start is true, we
allocate a new consistent read view for the trx if it does not yet have one,
or in the case of a locking read, set an InnoDB 'intention' table level
lock on the table.

  4) We do the SELECT. MySQL may repeatedly call ::index_read for the
same table handle instance, if it is a join.

  5) When the SELECT ends, MySQL removes its intention table level locks
in ::external_lock. When trx->n_mysql_tables_in_use drops to zero,
 (a) we execute a COMMIT there if the autocommit is on,
 (b) we also release possible 'SQL statement level resources' InnoDB may
have for this SQL statement. The MySQL interpreter does NOT execute
autocommit for pure read transactions, though it should. That is why the
table handler in that case has to execute the COMMIT in ::external_lock.

  B) If the user has explicitly set MySQL table level locks, then MySQL
does NOT call ::external_lock at the start of the statement. To determine
when we are at the start of a new SQL statement we at the start of
::index_read also compare the query id to the latest query id where the
table handle instance was used. If it has changed, we know we are at the
start of a new SQL statement. Since the query id can theoretically
overwrap, we use this test only as a secondary way of determining the
start of a new SQL statement. */


/**********************************************************************//**
Positions an index cursor to the index specified in the handle. Fetches the
row if any.
@return	0, HA_ERR_KEY_NOT_FOUND, or error number */
UNIV_INTERN
int
ha_innobase::index_read(
/*====================*/
	uchar*		buf,		/*!< in/out: buffer for the returned
					row */
	const uchar*	key_ptr,	/*!< in: key value; if this is NULL
					we position the cursor at the
					start or end of index; this can
					also contain an InnoDB row id, in
					which case key_len is the InnoDB
					row id length; the key value can
					also be a prefix of a full key value,
					and the last column can be a prefix
					of a full column */
	uint			key_len,/*!< in: key value length */
	enum ha_rkey_function find_flag)/*!< in: search flags from my_base.h */
{
	ulint		mode;
	dict_index_t*	index;
	ulint		match_mode	= 0;
	int		error;
	ulint		ret;

	DBUG_ENTER("index_read");
	DEBUG_SYNC_C("ha_innobase_index_read_begin");

	ut_a(prebuilt->trx == thd_to_trx(user_thd));
	ut_ad(key_len != 0 || find_flag != HA_READ_KEY_EXACT);

	ha_statistic_increment(&SSV::ha_read_key_count);

	if (UNIV_UNLIKELY(!share->ib_table ||
			(share->ib_table &&
			 share->ib_table->is_corrupt &&
			 srv_pass_corrupt_table <= 1))) {
		DBUG_RETURN(HA_ERR_CRASHED);
	}

	index = prebuilt->index;

	if (UNIV_UNLIKELY(index == NULL) || dict_index_is_corrupted(index)) {
		prebuilt->index_usable = FALSE;
		DBUG_RETURN(HA_ERR_CRASHED);
	}
	if (UNIV_UNLIKELY(!prebuilt->index_usable)) {
		DBUG_RETURN(dict_index_is_corrupted(index)
			    ? HA_ERR_INDEX_CORRUPT
			    : HA_ERR_TABLE_DEF_CHANGED);
	}

	/* Note that if the index for which the search template is built is not
	necessarily prebuilt->index, but can also be the clustered index */

	if (prebuilt->sql_stat_start) {
		build_template(false);
	}

	if (key_ptr) {
		/* Convert the search key value to InnoDB format into
		prebuilt->search_tuple */

		row_sel_convert_mysql_key_to_innobase(
			prebuilt->search_tuple,
			srch_key_val1, sizeof(srch_key_val1),
			index,
			(byte*) key_ptr,
			(ulint) key_len,
			prebuilt->trx);
		DBUG_ASSERT(prebuilt->search_tuple->n_fields > 0);
	} else {
		/* We position the cursor to the last or the first entry
		in the index */

		dtuple_set_n_fields(prebuilt->search_tuple, 0);
	}

	mode = convert_search_mode_to_innobase(find_flag);

	match_mode = 0;

	if (find_flag == HA_READ_KEY_EXACT) {

		match_mode = ROW_SEL_EXACT;

	} else if (find_flag == HA_READ_PREFIX
		   || find_flag == HA_READ_PREFIX_LAST) {

		match_mode = ROW_SEL_EXACT_PREFIX;
	}

	last_match_mode = (uint) match_mode;

	if (mode != PAGE_CUR_UNSUPP) {

		innodb_srv_conc_enter_innodb(prebuilt->trx);

		ret = row_search_for_mysql((byte*) buf, mode, prebuilt,
					   match_mode, 0);

		innodb_srv_conc_exit_innodb(prebuilt->trx);
	} else {

		ret = DB_UNSUPPORTED;
	}

	if (UNIV_UNLIKELY(!share->ib_table ||
			(share->ib_table &&
			 share->ib_table->is_corrupt &&
			 srv_pass_corrupt_table <= 1))) {
		DBUG_RETURN(HA_ERR_CRASHED);
	}

	switch (ret) {
	case DB_SUCCESS:
		error = 0;
		table->status = 0;
#ifdef EXTENDED_FOR_USERSTAT
		rows_read++;
		if (active_index < MAX_KEY)
			index_rows_read[active_index]++;
#endif
		break;
	case DB_RECORD_NOT_FOUND:
		error = HA_ERR_KEY_NOT_FOUND;
		table->status = STATUS_NOT_FOUND;
		break;
	case DB_END_OF_INDEX:
		error = HA_ERR_KEY_NOT_FOUND;
		table->status = STATUS_NOT_FOUND;
		break;
	default:
		error = convert_error_code_to_mysql((int) ret,
						    prebuilt->table->flags,
						    user_thd);
		table->status = STATUS_NOT_FOUND;
		break;
	}

	DBUG_RETURN(error);
}

/*******************************************************************//**
The following functions works like index_read, but it find the last
row with the current key value or prefix.
@return	0, HA_ERR_KEY_NOT_FOUND, or an error code */
UNIV_INTERN
int
ha_innobase::index_read_last(
/*=========================*/
	uchar*		buf,	/*!< out: fetched row */
	const uchar*	key_ptr,/*!< in: key value, or a prefix of a full
				key value */
	uint		key_len)/*!< in: length of the key val or prefix
				in bytes */
{
	return(index_read(buf, key_ptr, key_len, HA_READ_PREFIX_LAST));
}

/********************************************************************//**
Get the index for a handle. Does not change active index.
@return	NULL or index instance. */
UNIV_INTERN
dict_index_t*
ha_innobase::innobase_get_index(
/*============================*/
	uint		keynr)	/*!< in: use this index; MAX_KEY means always
				clustered index, even if it was internally
				generated by InnoDB */
{
	KEY*		key = 0;
	dict_index_t*	index = 0;

	DBUG_ENTER("innobase_get_index");

	if (keynr != MAX_KEY && table->s->keys > 0) {
		key = table->key_info + keynr;

		index = innobase_index_lookup(share, keynr);

		if (index) {
			ut_a(ut_strcmp(index->name, key->name) == 0);
		} else {
			/* Can't find index with keynr in the translation
			table. Only print message if the index translation
			table exists */
			if (share->idx_trans_tbl.index_mapping) {
				sql_print_warning("InnoDB could not find "
						  "index %s key no %u for "
						  "table %s through its "
						  "index translation table",
						  key ? key->name : "NULL",
						  keynr,
						  prebuilt->table->name);
			}

			index = dict_table_get_index_on_name(prebuilt->table,
							     key->name);
		}
	} else {
		index = dict_table_get_first_index(prebuilt->table);
	}

	if (!index) {
		sql_print_error(
			"Innodb could not find key n:o %u with name %s "
			"from dict cache for table %s",
			keynr, key ? key->name : "NULL",
			prebuilt->table->name);
	}

	DBUG_RETURN(index);
}

/********************************************************************//**
Changes the active index of a handle.
@return	0 or error code */
UNIV_INTERN
int
ha_innobase::change_active_index(
/*=============================*/
	uint	keynr)	/*!< in: use this index; MAX_KEY means always clustered
			index, even if it was internally generated by
			InnoDB */
{
	DBUG_ENTER("change_active_index");

	if (UNIV_UNLIKELY(!share->ib_table ||
			(share->ib_table &&
			 share->ib_table->is_corrupt &&
			 srv_pass_corrupt_table <= 1))) {
		DBUG_RETURN(HA_ERR_CRASHED);
	}

	ut_ad(user_thd == ha_thd());
	ut_a(prebuilt->trx == thd_to_trx(user_thd));

	active_index = keynr;

	prebuilt->index = innobase_get_index(keynr);

	if (UNIV_UNLIKELY(!prebuilt->index)) {
		sql_print_warning("InnoDB: change_active_index(%u) failed",
				  keynr);
		prebuilt->index_usable = FALSE;
		DBUG_RETURN(1);
	}

	prebuilt->index_usable = row_merge_is_index_usable(prebuilt->trx,
							   prebuilt->index);

	if (UNIV_UNLIKELY(!prebuilt->index_usable)) {
		if (dict_index_is_corrupted(prebuilt->index)) {
			char	index_name[MAX_FULL_NAME_LEN + 1];
			char	table_name[MAX_FULL_NAME_LEN + 1];

			innobase_format_name(
				index_name, sizeof index_name,
				prebuilt->index->name, TRUE);

			innobase_format_name(
				table_name, sizeof table_name,
				prebuilt->index->table->name, FALSE);

			push_warning_printf(
				user_thd, MYSQL_ERROR::WARN_LEVEL_WARN,
				HA_ERR_INDEX_CORRUPT,
				"InnoDB: Index %s for table %s is"
				" marked as corrupted",
				index_name, table_name);
			DBUG_RETURN(HA_ERR_INDEX_CORRUPT);
		} else {
			push_warning_printf(
				user_thd, MYSQL_ERROR::WARN_LEVEL_WARN,
				HA_ERR_TABLE_DEF_CHANGED,
				"InnoDB: insufficient history for index %u",
				keynr);
		}

		/* The caller seems to ignore this.  Thus, we must check
		this again in row_search_for_mysql(). */
		DBUG_RETURN(convert_error_code_to_mysql(DB_MISSING_HISTORY,
                                                        0, NULL));
	}

	ut_a(prebuilt->search_tuple != 0);

	dtuple_set_n_fields(prebuilt->search_tuple, prebuilt->index->n_fields);

	dict_index_copy_types(prebuilt->search_tuple, prebuilt->index,
			      prebuilt->index->n_fields);

	/* MySQL changes the active index for a handle also during some
	queries, for example SELECT MAX(a), SUM(a) first retrieves the MAX()
	and then calculates the sum. Previously we played safe and used
	the flag ROW_MYSQL_WHOLE_ROW below, but that caused unnecessary
	copying. Starting from MySQL-4.1 we use a more efficient flag here. */

	build_template(false);

	DBUG_RETURN(0);
}

/**********************************************************************//**
Positions an index cursor to the index specified in keynr. Fetches the
row if any.
??? This is only used to read whole keys ???
@return	error number or 0 */
UNIV_INTERN
int
ha_innobase::index_read_idx(
/*========================*/
	uchar*		buf,		/*!< in/out: buffer for the returned
					row */
	uint		keynr,		/*!< in: use this index */
	const uchar*	key,		/*!< in: key value; if this is NULL
					we position the cursor at the
					start or end of index */
	uint		key_len,	/*!< in: key value length */
	enum ha_rkey_function find_flag)/*!< in: search flags from my_base.h */
{
	if (change_active_index(keynr)) {

		return(1);
	}

	return(index_read(buf, key, key_len, find_flag));
}

/***********************************************************************//**
Reads the next or previous row from a cursor, which must have previously been
positioned using index_read.
@return	0, HA_ERR_END_OF_FILE, or error number */
UNIV_INTERN
int
ha_innobase::general_fetch(
/*=======================*/
	uchar*	buf,		/*!< in/out: buffer for next row in MySQL
				format */
	uint	direction,	/*!< in: ROW_SEL_NEXT or ROW_SEL_PREV */
	uint	match_mode)	/*!< in: 0, ROW_SEL_EXACT, or
				ROW_SEL_EXACT_PREFIX */
{
	ulint		ret;
	int		error	= 0;

	DBUG_ENTER("general_fetch");

	/* If transaction is not startted do not continue, instead return a error code. */
	if(!(prebuilt->sql_stat_start || (prebuilt->trx && prebuilt->trx->state == 1))) {
		DBUG_RETURN(HA_ERR_END_OF_FILE);
	}

	if (UNIV_UNLIKELY(!share->ib_table ||
			(share->ib_table &&
			 share->ib_table->is_corrupt &&
			 srv_pass_corrupt_table <= 1))) {
		DBUG_RETURN(HA_ERR_CRASHED);
	}

	ut_a(prebuilt->trx == thd_to_trx(user_thd));

	innodb_srv_conc_enter_innodb(prebuilt->trx);

	ret = row_search_for_mysql(
		(byte*)buf, 0, prebuilt, match_mode, direction);

	innodb_srv_conc_exit_innodb(prebuilt->trx);

	if (UNIV_UNLIKELY(!share->ib_table ||
			(share->ib_table &&
			 share->ib_table->is_corrupt &&
			 srv_pass_corrupt_table <= 1))) {
		DBUG_RETURN(HA_ERR_CRASHED);
	}

	switch (ret) {
	case DB_SUCCESS:
		error = 0;
		table->status = 0;
#ifdef EXTENDED_FOR_USERSTAT
		rows_read++;
		if (active_index < MAX_KEY)
			index_rows_read[active_index]++;
#endif
		break;
	case DB_RECORD_NOT_FOUND:
		error = HA_ERR_END_OF_FILE;
		table->status = STATUS_NOT_FOUND;
		break;
	case DB_END_OF_INDEX:
		error = HA_ERR_END_OF_FILE;
		table->status = STATUS_NOT_FOUND;
		break;
	default:
		error = convert_error_code_to_mysql(
			(int) ret, prebuilt->table->flags, user_thd);
		table->status = STATUS_NOT_FOUND;
		break;
	}

	DBUG_RETURN(error);
}

/***********************************************************************//**
Reads the next row from a cursor, which must have previously been
positioned using index_read.
@return	0, HA_ERR_END_OF_FILE, or error number */
UNIV_INTERN
int
ha_innobase::index_next(
/*====================*/
	uchar*		buf)	/*!< in/out: buffer for next row in MySQL
				format */
{
	ha_statistic_increment(&SSV::ha_read_next_count);

	return(general_fetch(buf, ROW_SEL_NEXT, 0));
}

/*******************************************************************//**
Reads the next row matching to the key value given as the parameter.
@return	0, HA_ERR_END_OF_FILE, or error number */
UNIV_INTERN
int
ha_innobase::index_next_same(
/*=========================*/
	uchar*		buf,	/*!< in/out: buffer for the row */
	const uchar*	key,	/*!< in: key value */
	uint		keylen)	/*!< in: key value length */
{
	ha_statistic_increment(&SSV::ha_read_next_count);

	return(general_fetch(buf, ROW_SEL_NEXT, last_match_mode));
}

/***********************************************************************//**
Reads the previous row from a cursor, which must have previously been
positioned using index_read.
@return	0, HA_ERR_END_OF_FILE, or error number */
UNIV_INTERN
int
ha_innobase::index_prev(
/*====================*/
	uchar*	buf)	/*!< in/out: buffer for previous row in MySQL format */
{
	ha_statistic_increment(&SSV::ha_read_prev_count);

	return(general_fetch(buf, ROW_SEL_PREV, 0));
}

/********************************************************************//**
Positions a cursor on the first record in an index and reads the
corresponding row to buf.
@return	0, HA_ERR_END_OF_FILE, or error code */
UNIV_INTERN
int
ha_innobase::index_first(
/*=====================*/
	uchar*	buf)	/*!< in/out: buffer for the row */
{
	int	error;

	DBUG_ENTER("index_first");
	ha_statistic_increment(&SSV::ha_read_first_count);

	error = index_read(buf, NULL, 0, HA_READ_AFTER_KEY);

	/* MySQL does not seem to allow this to return HA_ERR_KEY_NOT_FOUND */

	if (error == HA_ERR_KEY_NOT_FOUND) {
		error = HA_ERR_END_OF_FILE;
	}

	DBUG_RETURN(error);
}

/********************************************************************//**
Positions a cursor on the last record in an index and reads the
corresponding row to buf.
@return	0, HA_ERR_END_OF_FILE, or error code */
UNIV_INTERN
int
ha_innobase::index_last(
/*====================*/
	uchar*	buf)	/*!< in/out: buffer for the row */
{
	int	error;

	DBUG_ENTER("index_last");
	ha_statistic_increment(&SSV::ha_read_last_count);

	error = index_read(buf, NULL, 0, HA_READ_BEFORE_KEY);

	/* MySQL does not seem to allow this to return HA_ERR_KEY_NOT_FOUND */

	if (error == HA_ERR_KEY_NOT_FOUND) {
		error = HA_ERR_END_OF_FILE;
	}

	DBUG_RETURN(error);
}

/****************************************************************//**
Initialize a table scan.
@return	0 or error number */
UNIV_INTERN
int
ha_innobase::rnd_init(
/*==================*/
	bool	scan)	/*!< in: TRUE if table/index scan FALSE otherwise */
{
	int	err;

	/* Store the active index value so that we can restore the original
	value after a scan */

	if (prebuilt->clust_index_was_generated) {
		err = change_active_index(MAX_KEY);
	} else {
		err = change_active_index(primary_key);
	}

	/* Don't use semi-consistent read in random row reads (by position).
	This means we must disable semi_consistent_read if scan is false */

	if (!scan) {
		try_semi_consistent_read(0);
	}

	start_of_scan = 1;

	return(err);
}

/*****************************************************************//**
Ends a table scan.
@return	0 or error number */
UNIV_INTERN
int
ha_innobase::rnd_end(void)
/*======================*/
{
	return(index_end());
}

/*****************************************************************//**
Reads the next row in a table scan (also used to read the FIRST row
in a table scan).
@return	0, HA_ERR_END_OF_FILE, or error number */
UNIV_INTERN
int
ha_innobase::rnd_next(
/*==================*/
	uchar*	buf)	/*!< in/out: returns the row in this buffer,
			in MySQL format */
{
	int	error;

	DBUG_ENTER("rnd_next");
	ha_statistic_increment(&SSV::ha_read_rnd_next_count);

	if (start_of_scan) {
		error = index_first(buf);

		if (error == HA_ERR_KEY_NOT_FOUND) {
			error = HA_ERR_END_OF_FILE;
		}

		start_of_scan = 0;
	} else {
		error = general_fetch(buf, ROW_SEL_NEXT, 0);
	}

	DBUG_RETURN(error);
}

/**********************************************************************//**
Fetches a row from the table based on a row reference.
@return	0, HA_ERR_KEY_NOT_FOUND, or error code */
UNIV_INTERN
int
ha_innobase::rnd_pos(
/*=================*/
	uchar*	buf,	/*!< in/out: buffer for the row */
	uchar*	pos)	/*!< in: primary key value of the row in the
			MySQL format, or the row id if the clustered
			index was internally generated by InnoDB; the
			length of data in pos has to be ref_length */
{
	int		error;
	uint		keynr	= active_index;
	DBUG_ENTER("rnd_pos");
	DBUG_DUMP("key", pos, ref_length);

	ha_statistic_increment(&SSV::ha_read_rnd_count);

	ut_a(prebuilt->trx == thd_to_trx(ha_thd()));

	if (prebuilt->clust_index_was_generated) {
		/* No primary key was defined for the table and we
		generated the clustered index from the row id: the
		row reference is the row id, not any key value
		that MySQL knows of */

		error = change_active_index(MAX_KEY);
	} else {
		error = change_active_index(primary_key);
	}

	if (error) {
		DBUG_PRINT("error", ("Got error: %d", error));
		DBUG_RETURN(error);
	}

	/* Note that we assume the length of the row reference is fixed
	for the table, and it is == ref_length */

	error = index_read(buf, pos, ref_length, HA_READ_KEY_EXACT);

	if (error) {
		DBUG_PRINT("error", ("Got error: %d", error));
	}

	change_active_index(keynr);

	DBUG_RETURN(error);
}

/*********************************************************************//**
Stores a reference to the current row to 'ref' field of the handle. Note
that in the case where we have generated the clustered index for the
table, the function parameter is illogical: we MUST ASSUME that 'record'
is the current 'position' of the handle, because if row ref is actually
the row id internally generated in InnoDB, then 'record' does not contain
it. We just guess that the row id must be for the record where the handle
was positioned the last time. */
UNIV_INTERN
void
ha_innobase::position(
/*==================*/
	const uchar*	record)	/*!< in: row in MySQL format */
{
	uint		len;

	ut_a(prebuilt->trx == thd_to_trx(ha_thd()));

	if (prebuilt->clust_index_was_generated) {
		/* No primary key was defined for the table and we
		generated the clustered index from row id: the
		row reference will be the row id, not any key value
		that MySQL knows of */

		len = DATA_ROW_ID_LEN;

		memcpy(ref, prebuilt->row_id, len);
	} else {
		len = store_key_val_for_row(primary_key, (char*)ref,
							 ref_length, record);
	}

	/* We assume that the 'ref' value len is always fixed for the same
	table. */

	if (len != ref_length) {
	  sql_print_error("Stored ref len is %lu, but table ref len is %lu",
			  (ulong) len, (ulong) ref_length);
	}
}

/* limit innodb monitor access to users with PROCESS privilege.
See http://bugs.mysql.com/32710 for expl. why we choose PROCESS. */
#define IS_MAGIC_TABLE_AND_USER_DENIED_ACCESS(table_name, thd) \
	(row_is_magic_monitor_table(table_name) \
	 && check_global_access(thd, PROCESS_ACL))

/*****************************************************************//**
Creates a table definition to an InnoDB database. */
static
int
create_table_def(
/*=============*/
	trx_t*		trx,		/*!< in: InnoDB transaction handle */
	TABLE*		form,		/*!< in: information on table
					columns and indexes */
	const char*	table_name,	/*!< in: table name */
	const char*	path_of_temp_table,/*!< in: if this is a table explicitly
					created by the user with the
					TEMPORARY keyword, then this
					parameter is the dir path where the
					table should be placed if we create
					an .ibd file for it (no .ibd extension
					in the path, though); otherwise this
					is NULL */
	ulint		flags)		/*!< in: table flags */
{
	Field*		field;
	dict_table_t*	table;
	ulint		n_cols;
	int		error;
	ulint		col_type;
	ulint		col_len;
	ulint		nulls_allowed;
	ulint		unsigned_type;
	ulint		binary_type;
	ulint		long_true_varchar;
	ulint		charset_no;
	ulint		i;

	DBUG_ENTER("create_table_def");
	DBUG_PRINT("enter", ("table_name: %s", table_name));

	ut_a(trx->mysql_thd != NULL);

	/* MySQL does the name length check. But we do additional check
	on the name length here */
	const size_t	table_name_len = strlen(table_name);
	if (table_name_len > MAX_FULL_NAME_LEN) {
		push_warning_printf(
			(THD*) trx->mysql_thd, MYSQL_ERROR::WARN_LEVEL_WARN,
			ER_TABLE_NAME,
			"InnoDB: Table Name or Database Name is too long");

		DBUG_RETURN(ER_TABLE_NAME);
	}

	if (table_name[table_name_len - 1] == '/') {
		push_warning_printf(
			(THD*) trx->mysql_thd, MYSQL_ERROR::WARN_LEVEL_WARN,
			ER_TABLE_NAME,
			"InnoDB: Table name is empty");

		DBUG_RETURN(ER_WRONG_TABLE_NAME);
	}

	n_cols = form->s->fields;

	/* We pass 0 as the space id, and determine at a lower level the space
	id where to store the table */

	table = dict_mem_table_create(table_name, 0, form->s->stored_fields, flags);

	if (path_of_temp_table) {
		table->dir_path_of_temp_table =
			mem_heap_strdup(table->heap, path_of_temp_table);
	}

	for (i = 0; i < n_cols; i++) {
		field = form->field[i];
		if (!field->stored_in_db)
		  continue;

		col_type = get_innobase_type_from_mysql_type(&unsigned_type,
							     field);

		if (!col_type) {
			push_warning_printf(
				(THD*) trx->mysql_thd,
				MYSQL_ERROR::WARN_LEVEL_WARN,
				ER_CANT_CREATE_TABLE,
				"Error creating table '%s' with "
				"column '%s'. Please check its "
				"column type and try to re-create "
				"the table with an appropriate "
				"column type.",
				table->name, (char*) field->field_name);
			goto err_col;
		}

		if (field->null_ptr) {
			nulls_allowed = 0;
		} else {
			nulls_allowed = DATA_NOT_NULL;
		}

		if (field->binary()) {
			binary_type = DATA_BINARY_TYPE;
		} else {
			binary_type = 0;
		}

		charset_no = 0;

		if (dtype_is_string_type(col_type)) {

			charset_no = (ulint)field->charset()->number;

			if (UNIV_UNLIKELY(charset_no >= 256)) {
				/* in data0type.h we assume that the
				number fits in one byte in prtype */
				push_warning_printf(
					(THD*) trx->mysql_thd,
					MYSQL_ERROR::WARN_LEVEL_WARN,
					ER_CANT_CREATE_TABLE,
					"In InnoDB, charset-collation codes"
					" must be below 256."
					" Unsupported code %lu.",
					(ulong) charset_no);
				DBUG_RETURN(ER_CANT_CREATE_TABLE);
			}
		}

		ut_a(field->type() < 256); /* we assume in dtype_form_prtype()
					   that this fits in one byte */
		col_len = field->pack_length();

		/* The MySQL pack length contains 1 or 2 bytes length field
		for a true VARCHAR. Let us subtract that, so that the InnoDB
		column length in the InnoDB data dictionary is the real
		maximum byte length of the actual data. */

		long_true_varchar = 0;

		if (field->type() == MYSQL_TYPE_VARCHAR) {
			col_len -= ((Field_varstring*)field)->length_bytes;

			if (((Field_varstring*)field)->length_bytes == 2) {
				long_true_varchar = DATA_LONG_TRUE_VARCHAR;
			}
		}

		/* First check whether the column to be added has a
		system reserved name. */
		if (dict_col_name_is_reserved(field->field_name)){
			my_error(ER_WRONG_COLUMN_NAME, MYF(0),
				 field->field_name);
err_col:
			dict_mem_table_free(table);
			trx_commit_for_mysql(trx);

			error = DB_ERROR;
			goto error_ret;
		}

		dict_mem_table_add_col(table, table->heap,
			(char*) field->field_name,
			col_type,
			dtype_form_prtype(
				(ulint)field->type()
				| nulls_allowed | unsigned_type
				| binary_type | long_true_varchar,
				charset_no),
			col_len);
	}

	error = row_create_table_for_mysql(table, trx);

	if (error == DB_DUPLICATE_KEY) {
		char buf[100];
		char* buf_end = innobase_convert_identifier(
			buf, sizeof buf - 1, table_name, strlen(table_name),
			trx->mysql_thd, TRUE);

		*buf_end = '\0';
		my_error(ER_TABLE_EXISTS_ERROR, MYF(0), buf);
	}

error_ret:
	error = convert_error_code_to_mysql(error, flags, NULL);

	DBUG_RETURN(error);
}

/*****************************************************************//**
Creates an index in an InnoDB database. */
static
int
create_index(
/*=========*/
	trx_t*		trx,		/*!< in: InnoDB transaction handle */
	TABLE*		form,		/*!< in: information on table
					columns and indexes */
	ulint		flags,		/*!< in: InnoDB table flags */
	const char*	table_name,	/*!< in: table name */
	uint		key_num)	/*!< in: index number */
{
	Field*		field;
	dict_index_t*	index;
	int		error;
	ulint		n_fields;
	KEY*		key;
	KEY_PART_INFO*	key_part;
	ulint		ind_type;
	ulint		col_type;
	ulint		prefix_len;
	ulint		is_unsigned;
	ulint		i;
	ulint		j;
	ulint*		field_lengths;

	DBUG_ENTER("create_index");

	key = form->key_info + key_num;

	n_fields = key->key_parts;

	/* Assert that "GEN_CLUST_INDEX" cannot be used as non-primary index */
	ut_a(innobase_strcasecmp(key->name, innobase_index_reserve_name) != 0);

	ind_type = 0;

	if (key_num == form->s->primary_key) {
		ind_type = ind_type | DICT_CLUSTERED;
	}

	if (key->flags & HA_NOSAME ) {
		ind_type = ind_type | DICT_UNIQUE;
	}

	/* We pass 0 as the space id, and determine at a lower level the space
	id where to store the table */

	index = dict_mem_index_create(table_name, key->name, 0,
				      ind_type, n_fields);

	field_lengths = (ulint*) my_malloc(sizeof(ulint) * n_fields,
		MYF(MY_FAE));

	for (i = 0; i < n_fields; i++) {
		key_part = key->key_part + i;

		/* (The flag HA_PART_KEY_SEG denotes in MySQL a column prefix
		field in an index: we only store a specified number of first
		bytes of the column to the index field.) The flag does not
		seem to be properly set by MySQL. Let us fall back on testing
		the length of the key part versus the column. */

		field = NULL;
		for (j = 0; j < form->s->fields; j++) {

			field = form->field[j];

			if (0 == innobase_strcasecmp(
					field->field_name,
					key_part->field->field_name)) {
				/* Found the corresponding column */

				break;
			}
		}

		ut_a(j < form->s->fields);

		col_type = get_innobase_type_from_mysql_type(
					&is_unsigned, key_part->field);

		if (DATA_BLOB == col_type
			|| (key_part->length < field->pack_length()
				&& field->type() != MYSQL_TYPE_VARCHAR)
			|| (field->type() == MYSQL_TYPE_VARCHAR
				&& key_part->length < field->pack_length()
				- ((Field_varstring*)field)->length_bytes)) {

			prefix_len = key_part->length;

			if (col_type == DATA_INT
				|| col_type == DATA_FLOAT
				|| col_type == DATA_DOUBLE
				|| col_type == DATA_DECIMAL) {
				sql_print_error(
					"MySQL is trying to create a column "
					"prefix index field, on an "
					"inappropriate data type. Table "
					"name %s, column name %s.",
					table_name,
					key_part->field->field_name);

				prefix_len = 0;
			}
		} else {
			prefix_len = 0;
		}

		field_lengths[i] = key_part->length;

		dict_mem_index_add_field(index,
			(char*) key_part->field->field_name, prefix_len);
	}

	/* Even though we've defined max_supported_key_part_length, we
	still do our own checking using field_lengths to be absolutely
	sure we don't create too long indexes. */
	error = row_create_index_for_mysql(index, trx, field_lengths);

	error = convert_error_code_to_mysql(error, flags, NULL);

	my_free(field_lengths);

	DBUG_RETURN(error);
}

/*****************************************************************//**
Creates an index to an InnoDB table when the user has defined no
primary index. */
static
int
create_clustered_index_when_no_primary(
/*===================================*/
	trx_t*		trx,		/*!< in: InnoDB transaction handle */
	ulint		flags,		/*!< in: InnoDB table flags */
	const char*	table_name)	/*!< in: table name */
{
	dict_index_t*	index;
	int		error;

	/* We pass 0 as the space id, and determine at a lower level the space
	id where to store the table */
	index = dict_mem_index_create(table_name,
				      innobase_index_reserve_name,
				      0, DICT_CLUSTERED, 0);

	error = row_create_index_for_mysql(index, trx, NULL);

	error = convert_error_code_to_mysql(error, flags, NULL);

	return(error);
}

/*****************************************************************//**
Return a display name for the row format
@return row format name */
UNIV_INTERN
const char*
get_row_format_name(
/*================*/
	enum row_type	row_format)		/*!< in: Row Format */
{
	switch (row_format) {
	case ROW_TYPE_COMPACT:
		return("COMPACT");
	case ROW_TYPE_COMPRESSED:
		return("COMPRESSED");
	case ROW_TYPE_DYNAMIC:
		return("DYNAMIC");
	case ROW_TYPE_REDUNDANT:
		return("REDUNDANT");
	case ROW_TYPE_DEFAULT:
		return("DEFAULT");
	case ROW_TYPE_FIXED:
		return("FIXED");
	case ROW_TYPE_PAGE:
	case ROW_TYPE_NOT_USED:
		break;
	}
	return("NOT USED");
}

/** If file-per-table is missing, issue warning and set ret false */
#define CHECK_ERROR_ROW_TYPE_NEEDS_FILE_PER_TABLE		\
	if (!srv_file_per_table) {				\
		push_warning_printf(				\
			thd, MYSQL_ERROR::WARN_LEVEL_WARN,	\
			ER_ILLEGAL_HA_CREATE_OPTION,		\
			"InnoDB: ROW_FORMAT=%s requires"	\
			" innodb_file_per_table.",		\
			get_row_format_name(row_format));	\
		ret = FALSE;					\
	}

/** If file-format is Antelope, issue warning and set ret false */
#define CHECK_ERROR_ROW_TYPE_NEEDS_GT_ANTELOPE			\
	if (srv_file_format < DICT_TF_FORMAT_ZIP) {		\
		push_warning_printf(				\
			thd, MYSQL_ERROR::WARN_LEVEL_WARN,	\
			ER_ILLEGAL_HA_CREATE_OPTION,		\
			"InnoDB: ROW_FORMAT=%s requires"	\
			" innodb_file_format > Antelope.",	\
			get_row_format_name(row_format));	\
		ret = FALSE;					\
	}


/*****************************************************************//**
Validates the create options. We may build on this function
in future. For now, it checks two specifiers:
KEY_BLOCK_SIZE and ROW_FORMAT
If innodb_strict_mode is not set then this function is a no-op
@return	TRUE if valid. */
static
ibool
create_options_are_valid(
/*=====================*/
	THD*		thd,		/*!< in: connection thread. */
	TABLE*		form,		/*!< in: information on table
					columns and indexes */
	HA_CREATE_INFO*	create_info)	/*!< in: create info. */
{
	ibool	kbs_specified	= FALSE;
	ibool	ret		= TRUE;
	enum row_type	row_format	= form->s->row_type;

	ut_ad(thd != NULL);

	/* If innodb_strict_mode is not set don't do any validation. */
	if (!(THDVAR(thd, strict_mode))) {
		return(TRUE);
	}

	ut_ad(form != NULL);
	ut_ad(create_info != NULL);

	/* First check if a non-zero KEY_BLOCK_SIZE was specified. */
	if (create_info->key_block_size) {
		kbs_specified = TRUE;
		switch (create_info->key_block_size) {
		case 1:
		case 2:
		case 4:
		case 8:
		case 16:
			/* Valid KEY_BLOCK_SIZE, check its dependencies. */
			if (!srv_file_per_table) {
				push_warning(
					thd, MYSQL_ERROR::WARN_LEVEL_WARN,
					ER_ILLEGAL_HA_CREATE_OPTION,
					"InnoDB: KEY_BLOCK_SIZE requires"
					" innodb_file_per_table.");
				ret = FALSE;
			}
			if (srv_file_format < DICT_TF_FORMAT_ZIP) {
				push_warning(
					thd, MYSQL_ERROR::WARN_LEVEL_WARN,
					ER_ILLEGAL_HA_CREATE_OPTION,
					"InnoDB: KEY_BLOCK_SIZE requires"
					" innodb_file_format > Antelope.");
					ret = FALSE;
			}
			break;
		default:
			push_warning_printf(
				thd, MYSQL_ERROR::WARN_LEVEL_WARN,
				ER_ILLEGAL_HA_CREATE_OPTION,
				"InnoDB: invalid KEY_BLOCK_SIZE = %lu."
				" Valid values are [1, 2, 4, 8, 16]",
				create_info->key_block_size);
			ret = FALSE;
			break;
		}
	}
	
	/* Check for a valid Innodb ROW_FORMAT specifier and
	other incompatibilities. */
	switch (row_format) {
	case ROW_TYPE_COMPRESSED:
		CHECK_ERROR_ROW_TYPE_NEEDS_FILE_PER_TABLE;
		CHECK_ERROR_ROW_TYPE_NEEDS_GT_ANTELOPE;
		break;
	case ROW_TYPE_DYNAMIC:
		CHECK_ERROR_ROW_TYPE_NEEDS_FILE_PER_TABLE;
		CHECK_ERROR_ROW_TYPE_NEEDS_GT_ANTELOPE;
		/* fall through since dynamic also shuns KBS */
	case ROW_TYPE_COMPACT:
	case ROW_TYPE_REDUNDANT:
		if (kbs_specified) {
			push_warning_printf(
				thd, MYSQL_ERROR::WARN_LEVEL_WARN,
				ER_ILLEGAL_HA_CREATE_OPTION,
				"InnoDB: cannot specify ROW_FORMAT = %s"
				" with KEY_BLOCK_SIZE.",
				get_row_format_name(row_format));
			ret = FALSE;
		}
		break;
	case ROW_TYPE_DEFAULT:
		break;
	case ROW_TYPE_FIXED:
	case ROW_TYPE_PAGE:
	case ROW_TYPE_NOT_USED:
		push_warning(
			thd, MYSQL_ERROR::WARN_LEVEL_WARN,
			ER_ILLEGAL_HA_CREATE_OPTION,		\
			"InnoDB: invalid ROW_FORMAT specifier.");
		ret = FALSE;
		break;
	}

	return(ret);
}

/*****************************************************************//**
Update create_info.  Used in SHOW CREATE TABLE et al. */
UNIV_INTERN
void
ha_innobase::update_create_info(
/*============================*/
	HA_CREATE_INFO* create_info)	/*!< in/out: create info */
{
  if (!(create_info->used_fields & HA_CREATE_USED_AUTO)) {
    ha_innobase::info(HA_STATUS_AUTO);
    create_info->auto_increment_value = stats.auto_increment_value;
  }
}

/*****************************************************************//**
Creates a new table to an InnoDB database.
@return	error number */
UNIV_INTERN
int
ha_innobase::create(
/*================*/
	const char*	name,		/*!< in: table name */
	TABLE*		form,		/*!< in: information on table
					columns and indexes */
	HA_CREATE_INFO*	create_info)	/*!< in: more information of the
					created table, contains also the
					create statement string */
{
	int		error;
	dict_table_t*	innobase_table;
	trx_t*		parent_trx;
	trx_t*		trx;
	int		primary_key_no;
	uint		i;
	char		name2[FN_REFLEN];
	char		norm_name[FN_REFLEN];
	THD*		thd = ha_thd();
	ib_int64_t	auto_inc_value;
	ulint		flags;
	/* Cache the value of innodb_file_format, in case it is
	modified by another thread while the table is being created. */
	const ulint	file_format = srv_file_format;
	const char*	stmt;
	size_t		stmt_len;
	enum row_type	row_format;

	DBUG_ENTER("ha_innobase::create");

	DBUG_ASSERT(thd != NULL);
	DBUG_ASSERT(create_info != NULL);

#ifdef __WIN__
	/* Names passed in from server are in two formats:
	1. <database_name>/<table_name>: for normal table creation
	2. full path: for temp table creation, or sym link

	When srv_file_per_table is on and mysqld_embedded is off,
	check for full path pattern, i.e.
	X:\dir\...,		X is a driver letter, or
	\\dir1\dir2\...,	UNC path
	returns error if it is in full path format, but not creating a temp.
	table. Currently InnoDB does not support symbolic link on Windows. */

	if (srv_file_per_table
	    && !mysqld_embedded
	    && !(create_info->options & HA_LEX_CREATE_TMP_TABLE)) {

		if ((name[1] == ':')
		    || (name[0] == '\\' && name[1] == '\\')) {
			sql_print_error("Cannot create table %s\n", name);
			DBUG_RETURN(HA_ERR_GENERIC);
		}
	}
#endif

	if (form->s->stored_fields > 1000) {
		/* The limit probably should be REC_MAX_N_FIELDS - 3 = 1020,
		but we play safe here */

		DBUG_RETURN(HA_ERR_TO_BIG_ROW);
	}

	ut_a(strlen(name) < sizeof(name2));

	strcpy(name2, name);

	normalize_table_name(norm_name, name2);

	/* Create the table definition in InnoDB */

	flags = 0;

	/* Validate create options if innodb_strict_mode is set. */
	if (!create_options_are_valid(thd, form, create_info)) {
		DBUG_RETURN(ER_ILLEGAL_HA_CREATE_OPTION);
	}

	if (create_info->key_block_size) {
		/* Determine the page_zip.ssize corresponding to the
		requested page size (key_block_size) in kilobytes. */

		ulint	ssize, ksize;
		ulint	key_block_size = create_info->key_block_size;

		/*  Set 'flags' to the correct key_block_size.
		It will be zero if key_block_size is an invalid number.*/
		for (ssize = ksize = 1; ssize <= DICT_TF_ZSSIZE_MAX;
		     ssize++, ksize <<= 1) {
			if (key_block_size == ksize) {
				flags = ssize << DICT_TF_ZSSIZE_SHIFT
					| DICT_TF_COMPACT
					| DICT_TF_FORMAT_ZIP
					  << DICT_TF_FORMAT_SHIFT;
				break;
			}
		}

		if (!srv_file_per_table) {
			push_warning(
				thd, MYSQL_ERROR::WARN_LEVEL_WARN,
				ER_ILLEGAL_HA_CREATE_OPTION,
				"InnoDB: KEY_BLOCK_SIZE requires"
				" innodb_file_per_table.");
			flags = 0;
		}

		if (file_format < DICT_TF_FORMAT_ZIP) {
			push_warning(
				thd, MYSQL_ERROR::WARN_LEVEL_WARN,
				ER_ILLEGAL_HA_CREATE_OPTION,
				"InnoDB: KEY_BLOCK_SIZE requires"
				" innodb_file_format > Antelope.");
			flags = 0;
		}

		if (!flags) {
			push_warning_printf(
				thd, MYSQL_ERROR::WARN_LEVEL_WARN,
				ER_ILLEGAL_HA_CREATE_OPTION,
				"InnoDB: ignoring KEY_BLOCK_SIZE=%lu.",
				create_info->key_block_size);
		}
	}

	row_format = form->s->row_type;

	if (flags) {
		/* if ROW_FORMAT is set to default,
		automatically change it to COMPRESSED.*/
		if (row_format == ROW_TYPE_DEFAULT) {
			row_format = ROW_TYPE_COMPRESSED;
		} else if (row_format != ROW_TYPE_COMPRESSED) {
			/* ROW_FORMAT other than COMPRESSED
			ignores KEY_BLOCK_SIZE.  It does not
			make sense to reject conflicting
			KEY_BLOCK_SIZE and ROW_FORMAT, because
			such combinations can be obtained
			with ALTER TABLE anyway. */
			push_warning_printf(
				thd, MYSQL_ERROR::WARN_LEVEL_WARN,
				ER_ILLEGAL_HA_CREATE_OPTION,
				"InnoDB: ignoring KEY_BLOCK_SIZE=%lu"
				" unless ROW_FORMAT=COMPRESSED.",
				create_info->key_block_size);
			flags = 0;
		}
	} else {
		/* flags == 0 means no KEY_BLOCK_SIZE.*/
		if (row_format == ROW_TYPE_COMPRESSED) {
			/* ROW_FORMAT=COMPRESSED without
			KEY_BLOCK_SIZE implies half the
			maximum KEY_BLOCK_SIZE. */
			flags = (DICT_TF_ZSSIZE_MAX - 1)
				<< DICT_TF_ZSSIZE_SHIFT
				| DICT_TF_COMPACT
				| DICT_TF_FORMAT_ZIP
				<< DICT_TF_FORMAT_SHIFT;
//#if DICT_TF_ZSSIZE_MAX < 1
//# error "DICT_TF_ZSSIZE_MAX < 1"
//#endif
		}
	}

	switch (row_format) {
	case ROW_TYPE_REDUNDANT:
		break;
	case ROW_TYPE_COMPRESSED:
	case ROW_TYPE_DYNAMIC:
		if (!srv_file_per_table) {
			push_warning_printf(
				thd, MYSQL_ERROR::WARN_LEVEL_WARN,
				ER_ILLEGAL_HA_CREATE_OPTION,
				"InnoDB: ROW_FORMAT=%s requires"
				" innodb_file_per_table.",
				get_row_format_name(row_format));
		} else if (file_format < DICT_TF_FORMAT_ZIP) {
			push_warning_printf(
				thd, MYSQL_ERROR::WARN_LEVEL_WARN,
				ER_ILLEGAL_HA_CREATE_OPTION,
				"InnoDB: ROW_FORMAT=%s requires"
				" innodb_file_format > Antelope.",
				get_row_format_name(row_format));
		} else {
			flags |= DICT_TF_COMPACT
			         | (DICT_TF_FORMAT_ZIP
			            << DICT_TF_FORMAT_SHIFT);
			break;
		}

		/* fall through */
	case ROW_TYPE_NOT_USED:
	case ROW_TYPE_FIXED:
	case ROW_TYPE_PAGE:
		push_warning(
			thd, MYSQL_ERROR::WARN_LEVEL_WARN,
			ER_ILLEGAL_HA_CREATE_OPTION,
			"InnoDB: assuming ROW_FORMAT=COMPACT.");
	case ROW_TYPE_DEFAULT:
	case ROW_TYPE_COMPACT:
		flags = DICT_TF_COMPACT;
		break;
	}

	/* Look for a primary key */

	primary_key_no= (form->s->primary_key != MAX_KEY ?
			 (int) form->s->primary_key :
			 -1);

	/* Our function innobase_get_mysql_key_number_for_index assumes
	the primary key is always number 0, if it exists */

	ut_a(primary_key_no == -1 || primary_key_no == 0);

	/* Check for name conflicts (with reserved name) for
	any user indices to be created. */
	if (innobase_index_name_is_reserved(thd, form->key_info,
					    form->s->keys)) {
		DBUG_RETURN(-1);
	}

	if (IS_MAGIC_TABLE_AND_USER_DENIED_ACCESS(norm_name, thd)) {
		DBUG_RETURN(HA_ERR_GENERIC);
	}

	if (create_info->options & HA_LEX_CREATE_TMP_TABLE) {
		flags |= DICT_TF2_TEMPORARY << DICT_TF2_SHIFT;
	}

	/* Get the transaction associated with the current thd, or create one
	if not yet created */

	parent_trx = check_trx_exists(thd);

	/* In case MySQL calls this in the middle of a SELECT query, release
	possible adaptive hash latch to avoid deadlocks of threads */

	trx_search_latch_release_if_reserved(parent_trx);

	trx = innobase_trx_allocate(thd);

	if (UNIV_UNLIKELY(trx->fake_changes)) {
		innobase_commit_low(trx);
		trx_free_for_mysql(trx);
		DBUG_RETURN(HA_ERR_WRONG_COMMAND);
	}

	/* Latch the InnoDB data dictionary exclusively so that no deadlocks
	or lock waits can happen in it during a table create operation.
	Drop table etc. do this latching in row0mysql.c. */

	row_mysql_lock_data_dictionary(trx);

	error = create_table_def(trx, form, norm_name,
		create_info->options & HA_LEX_CREATE_TMP_TABLE ? name2 : NULL,
		flags);

	if (error) {
		goto cleanup;
	}


	/* Create the keys */

	if (form->s->keys == 0 || primary_key_no == -1) {
		/* Create an index which is used as the clustered index;
		order the rows by their row id which is internally generated
		by InnoDB */

		error = create_clustered_index_when_no_primary(
			trx, flags, norm_name);
		if (error) {
			goto cleanup;
		}
	}

	if (primary_key_no != -1) {
		/* In InnoDB the clustered index must always be created
		first */
		if ((error = create_index(trx, form, flags, norm_name,
					  (uint) primary_key_no))) {
			goto cleanup;
		}
	}

	for (i = 0; i < form->s->keys; i++) {

		if (i != (uint) primary_key_no) {

			if ((error = create_index(trx, form, flags,
						  norm_name, i))) {
				goto cleanup;
			}
		}
	}

	stmt = innobase_get_stmt(thd, &stmt_len);

	if (stmt) {
		error = row_table_add_foreign_constraints(
			trx, stmt, stmt_len, norm_name,
			create_info->options & HA_LEX_CREATE_TMP_TABLE);

		switch (error) {

		case DB_PARENT_NO_INDEX:
			push_warning_printf(
				thd, MYSQL_ERROR::WARN_LEVEL_WARN,
				HA_ERR_CANNOT_ADD_FOREIGN,
				"Create table '%s' with foreign key constraint"
				" failed. There is no index in the referenced"
				" table where the referenced columns appear"
				" as the first columns.\n", norm_name);
			break;

		case DB_CHILD_NO_INDEX:
			push_warning_printf(
				thd, MYSQL_ERROR::WARN_LEVEL_WARN,
				HA_ERR_CANNOT_ADD_FOREIGN,
				"Create table '%s' with foreign key constraint"
				" failed. There is no index in the referencing"
				" table where referencing columns appear"
				" as the first columns.\n", norm_name);
			break;
                }

		error = convert_error_code_to_mysql(error, flags, NULL);

		if (error) {
			goto cleanup;
		}
	}

	innobase_commit_low(trx);

	row_mysql_unlock_data_dictionary(trx);

	/* Flush the log to reduce probability that the .frm files and
	the InnoDB data dictionary get out-of-sync if the user runs
	with innodb_flush_log_at_trx_commit = 0 */

	log_buffer_flush_to_disk();

	innobase_table = dict_table_get(norm_name, FALSE,
					DICT_ERR_IGNORE_NONE);

	DBUG_ASSERT(innobase_table != 0);

	if (innobase_table) {
		/* We update the highest file format in the system table
		space, if this table has higher file format setting. */

		trx_sys_file_format_max_upgrade(
			(const char**) &innobase_file_format_max,
			dict_table_get_format(innobase_table));
	}

	/* Note: We can't call update_thd() as prebuilt will not be
	setup at this stage and so we use thd. */

	/* We need to copy the AUTOINC value from the old table if
	this is an ALTER|OPTIMIZE TABLE or CREATE INDEX because CREATE INDEX
	does a table copy too. If query was one of :

		CREATE TABLE ...AUTO_INCREMENT = x; or
		ALTER TABLE...AUTO_INCREMENT = x;   or
		OPTIMIZE TABLE t; or
		CREATE INDEX x on t(...);

	Find out a table definition from the dictionary and get
	the current value of the auto increment field. Set a new
	value to the auto increment field if the value is greater
	than the maximum value in the column. */

	if (((create_info->used_fields & HA_CREATE_USED_AUTO)
	    || thd_sql_command(thd) == SQLCOM_ALTER_TABLE
	    || thd_sql_command(thd) == SQLCOM_OPTIMIZE
	    || thd_sql_command(thd) == SQLCOM_CREATE_INDEX)
	    && create_info->auto_increment_value > 0) {

		auto_inc_value = create_info->auto_increment_value;

		dict_table_autoinc_lock(innobase_table);
		dict_table_autoinc_initialize(innobase_table, auto_inc_value);
		dict_table_autoinc_unlock(innobase_table);
	}

	/* Tell the InnoDB server that there might be work for
	utility threads: */

	srv_active_wake_master_thread();

	trx_free_for_mysql(trx);

	DBUG_RETURN(0);

cleanup:
	innobase_commit_low(trx);

	row_mysql_unlock_data_dictionary(trx);

	trx_free_for_mysql(trx);

	DBUG_RETURN(error);
}

/*****************************************************************//**
Discards or imports an InnoDB tablespace.
@return	0 == success, -1 == error */
UNIV_INTERN
int
ha_innobase::discard_or_import_tablespace(
/*======================================*/
	my_bool discard)	/*!< in: TRUE if discard, else import */
{
	dict_table_t*	dict_table;
	trx_t*		trx;
	int		err;

	DBUG_ENTER("ha_innobase::discard_or_import_tablespace");

	ut_a(prebuilt->trx);
	ut_a(prebuilt->trx->magic_n == TRX_MAGIC_N);
	ut_a(prebuilt->trx == thd_to_trx(ha_thd()));

	dict_table = prebuilt->table;
	trx = prebuilt->trx;

	if (discard) {
		err = row_discard_tablespace_for_mysql(dict_table->name, trx);
	} else {
		err = row_import_tablespace_for_mysql(dict_table->name, trx);

		/* in expanded import mode re-initialize auto_increment again */
		if ((err == DB_SUCCESS) && srv_expand_import &&
		    (table->found_next_number_field != NULL)) {
			dict_table_autoinc_lock(dict_table);
			innobase_initialize_autoinc();
			dict_table_autoinc_unlock(dict_table);
		}
	}

	err = convert_error_code_to_mysql(err, dict_table->flags, NULL);

	DBUG_RETURN(err);
}

/*****************************************************************//**
Deletes all rows of an InnoDB table.
@return	error number */
UNIV_INTERN
int
ha_innobase::truncate(void)
/*==============================*/
{
	int		error;

	DBUG_ENTER("ha_innobase::truncate");

	/* Get the transaction associated with the current thd, or create one
	if not yet created, and update prebuilt->trx */

	update_thd(ha_thd());

	if (!share->ib_table || share->ib_table->is_corrupt) {
		DBUG_RETURN(HA_ERR_CRASHED);
	}

	if (UNIV_UNLIKELY(prebuilt->trx->fake_changes)) {
		DBUG_RETURN(HA_ERR_WRONG_COMMAND);
	}

	/* Truncate the table in InnoDB */

	error = row_truncate_table_for_mysql(prebuilt->table, prebuilt->trx);

	if (!share->ib_table || share->ib_table->is_corrupt) {
		DBUG_RETURN(HA_ERR_CRASHED);
	}

	error = convert_error_code_to_mysql(error, prebuilt->table->flags,
					    NULL);

	DBUG_RETURN(error);
}

/*****************************************************************//**
Drops a table from an InnoDB database. Before calling this function,
MySQL calls innobase_commit to commit the transaction of the current user.
Then the current user cannot have locks set on the table. Drop table
operation inside InnoDB will remove all locks any user has on the table
inside InnoDB.
@return	error number */
UNIV_INTERN
int
ha_innobase::delete_table(
/*======================*/
	const char*	name)	/*!< in: table name */
{
	ulint	name_len;
	int	error;
	trx_t*	parent_trx;
	trx_t*	trx;
	THD	*thd = ha_thd();
	char	norm_name[1000];

	DBUG_ENTER("ha_innobase::delete_table");

	DBUG_EXECUTE_IF(
		"test_normalize_table_name_low",
		test_normalize_table_name_low();
	);

	/* Strangely, MySQL passes the table name without the '.frm'
	extension, in contrast to ::create */
	normalize_table_name(norm_name, name);

	if (IS_MAGIC_TABLE_AND_USER_DENIED_ACCESS(norm_name, thd)) {
		DBUG_RETURN(HA_ERR_GENERIC);
	}

	/* Get the transaction associated with the current thd, or create one
	if not yet created */

	parent_trx = check_trx_exists(thd);

	/* In case MySQL calls this in the middle of a SELECT query, release
	possible adaptive hash latch to avoid deadlocks of threads */

	trx_search_latch_release_if_reserved(parent_trx);

	trx = innobase_trx_allocate(thd);

	if (UNIV_UNLIKELY(trx->fake_changes)) {
		innobase_commit_low(trx);
		trx_free_for_mysql(trx);
		DBUG_RETURN(HA_ERR_WRONG_COMMAND);
	}

	name_len = strlen(name);

	ut_a(name_len < 1000);

	/* Drop the table in InnoDB */

	error = row_drop_table_for_mysql(norm_name, trx,
					 thd_sql_command(thd)
					 == SQLCOM_DROP_DB,
					 FALSE);

	/* Flush the log to reduce probability that the .frm files and
	the InnoDB data dictionary get out-of-sync if the user runs
	with innodb_flush_log_at_trx_commit = 0 */

	log_buffer_flush_to_disk();

	/* Tell the InnoDB server that there might be work for
	utility threads: */

	srv_active_wake_master_thread();

	innobase_commit_low(trx);

	trx_free_for_mysql(trx);

	error = convert_error_code_to_mysql(error, 0, NULL);

	DBUG_RETURN(error);
}

/*****************************************************************//**
Removes all tables in the named database inside InnoDB. */
static
void
innobase_drop_database(
/*===================*/
	handlerton *hton, /*!< in: handlerton of Innodb */
	char*	path)	/*!< in: database path; inside InnoDB the name
			of the last directory in the path is used as
			the database name: for example, in 'mysql/data/test'
			the database name is 'test' */
{
	ulint	len		= 0;
	trx_t*	trx;
	char*	ptr;
	char*	namebuf;
	THD*	thd		= current_thd;

	/* Get the transaction associated with the current thd, or create one
	if not yet created */

	DBUG_ASSERT(hton == innodb_hton_ptr);

	/* In the Windows plugin, thd = current_thd is always NULL */
	if (thd) {
		trx_t*	parent_trx = check_trx_exists(thd);

		/* In case MySQL calls this in the middle of a SELECT
		query, release possible adaptive hash latch to avoid
		deadlocks of threads */

		trx_search_latch_release_if_reserved(parent_trx);
	}

	ptr = strend(path) - 2;

	while (ptr >= path && *ptr != '\\' && *ptr != '/') {
		ptr--;
		len++;
	}

	ptr++;
	namebuf = (char*) my_malloc((uint) len + 2, MYF(0));

	memcpy(namebuf, ptr, len);
	namebuf[len] = '/';
	namebuf[len + 1] = '\0';
#ifdef	__WIN__
	innobase_casedn_str(namebuf);
#endif
#if defined __WIN__ && !defined MYSQL_SERVER
	/* In the Windows plugin, thd = current_thd is always NULL */
	trx = trx_allocate_for_mysql();
	trx->mysql_thd = NULL;
#else
	trx = innobase_trx_allocate(thd);
#endif
	if (UNIV_UNLIKELY(trx->fake_changes)) {
		my_free(namebuf);
		innobase_commit_low(trx);
		trx_free_for_mysql(trx);
		return; /* ignore */
	}

	row_drop_database_for_mysql(namebuf, trx);
	my_free(namebuf);

	/* Flush the log to reduce probability that the .frm files and
	the InnoDB data dictionary get out-of-sync if the user runs
	with innodb_flush_log_at_trx_commit = 0 */

	log_buffer_flush_to_disk();

	/* Tell the InnoDB server that there might be work for
	utility threads: */

	srv_active_wake_master_thread();

	innobase_commit_low(trx);
	trx_free_for_mysql(trx);
}
/*********************************************************************//**
Renames an InnoDB table.
@return	0 or error code */
static
int
innobase_rename_table(
/*==================*/
	trx_t*		trx,	/*!< in: transaction */
	const char*	from,	/*!< in: old name of the table */
	const char*	to,	/*!< in: new name of the table */
	ibool		lock_and_commit)
				/*!< in: TRUE=lock data dictionary and commit */
{
	int	error;
	char*	norm_to;
	char*	norm_from;
	DBUG_ENTER("innobase_rename_table");

	// Magic number 64 arbitrary
	norm_to = (char*) my_malloc(strlen(to) + 64, MYF(0));
	norm_from = (char*) my_malloc(strlen(from) + 64, MYF(0));

	normalize_table_name(norm_to, to);
	normalize_table_name(norm_from, from);

	DEBUG_SYNC_C("innodb_rename_table_ready");

	/* Serialize data dictionary operations with dictionary mutex:
	no deadlocks can occur then in these operations.  Start the
	transaction first to avoid a possible deadlock in the server. */

	trx_start_if_not_started(trx);
	if (lock_and_commit) {
		row_mysql_lock_data_dictionary(trx);
	}

	/* Flag this transaction as a dictionary operation, so that
	the data dictionary will be locked in crash recovery. */
	trx_set_dict_operation(trx, TRX_DICT_OP_INDEX);

	error = row_rename_table_for_mysql(
		norm_from, norm_to, trx, lock_and_commit);

	if (lock_and_commit) {
		row_mysql_unlock_data_dictionary(trx);

		/* Flush the log to reduce probability that the .frm
		files and the InnoDB data dictionary get out-of-sync
		if the user runs with innodb_flush_log_at_trx_commit = 0 */

		log_buffer_flush_to_disk();
	}

	my_free(norm_to);
	my_free(norm_from);

	DBUG_RETURN(error);
}
/*********************************************************************//**
Renames an InnoDB table.
@return	0 or error code */
UNIV_INTERN
int
ha_innobase::rename_table(
/*======================*/
	const char*	from,	/*!< in: old name of the table */
	const char*	to)	/*!< in: new name of the table */
{
	trx_t*	trx;
	int	error;
	trx_t*	parent_trx;
	THD*	thd		= ha_thd();

	DBUG_ENTER("ha_innobase::rename_table");

	/* Get the transaction associated with the current thd, or create one
	if not yet created */

	parent_trx = check_trx_exists(thd);

	/* In case MySQL calls this in the middle of a SELECT query, release
	possible adaptive hash latch to avoid deadlocks of threads */

	trx_search_latch_release_if_reserved(parent_trx);

	trx = innobase_trx_allocate(thd);
	if (UNIV_UNLIKELY(trx->fake_changes)) {
		innobase_commit_low(trx);
		trx_free_for_mysql(trx);
		DBUG_RETURN(HA_ERR_WRONG_COMMAND);
	}

	error = innobase_rename_table(trx, from, to, TRUE);

	DEBUG_SYNC(thd, "after_innobase_rename_table");

	/* Tell the InnoDB server that there might be work for
	utility threads: */

	srv_active_wake_master_thread();

	innobase_commit_low(trx);
	trx_free_for_mysql(trx);

	/* Add a special case to handle the Duplicated Key error
	and return DB_ERROR instead.
	This is to avoid a possible SIGSEGV error from mysql error
	handling code. Currently, mysql handles the Duplicated Key
	error by re-entering the storage layer and getting dup key
	info by calling get_dup_key(). This operation requires a valid
	table handle ('row_prebuilt_t' structure) which could no
	longer be available in the error handling stage. The suggested
	solution is to report a 'table exists' error message (since
	the dup key error here is due to an existing table whose name
	is the one we are trying to rename to) and return the generic
	error code. */
	if (error == (int) DB_DUPLICATE_KEY) {
		my_error(ER_TABLE_EXISTS_ERROR, MYF(0), to);

		error = DB_ERROR;
	}

	error = convert_error_code_to_mysql(error, 0, NULL);

	DBUG_RETURN(error);
}

/*********************************************************************//**
Estimates the number of index records in a range.
@return	estimated number of rows */
UNIV_INTERN
ha_rows
ha_innobase::records_in_range(
/*==========================*/
	uint			keynr,		/*!< in: index number */
	key_range		*min_key,	/*!< in: start key value of the
						   range, may also be 0 */
	key_range		*max_key)	/*!< in: range end key val, may
						   also be 0 */
{
	KEY*		key;
	dict_index_t*	index;
	dtuple_t*	range_start;
	dtuple_t*	range_end;
	ib_int64_t	n_rows;
	ulint		mode1;
	ulint		mode2;
        uint key_parts;
	mem_heap_t*	heap;

	DBUG_ENTER("records_in_range");

	ut_a(prebuilt->trx == thd_to_trx(ha_thd()));

	prebuilt->trx->op_info = (char*)"estimating records in index range";

	/* In case MySQL calls this in the middle of a SELECT query, release
	possible adaptive hash latch to avoid deadlocks of threads */

	trx_search_latch_release_if_reserved(prebuilt->trx);

	active_index = keynr;

	key = table->key_info + active_index;

	index = innobase_get_index(keynr);

	/* There exists possibility of not being able to find requested
	index due to inconsistency between MySQL and InoDB dictionary info.
	Necessary message should have been printed in innobase_get_index() */
	if (prebuilt->table->ibd_file_missing) {
		n_rows = HA_POS_ERROR;
		goto func_exit;
	}
	if (UNIV_UNLIKELY(!index)) {
		n_rows = HA_POS_ERROR;
		goto func_exit;
	}
	if (dict_index_is_corrupted(index)) {
		n_rows = HA_ERR_INDEX_CORRUPT;
		goto func_exit;
	}
	if (UNIV_UNLIKELY(!row_merge_is_index_usable(prebuilt->trx, index))) {
		n_rows = HA_ERR_TABLE_DEF_CHANGED;
		goto func_exit;
	}

        key_parts= key->key_parts;
        if ((min_key && min_key->keypart_map>=(key_part_map) (1<<key_parts)) ||
            (max_key && max_key->keypart_map>=(key_part_map) (1<<key_parts)))
          key_parts= key->ext_key_parts;

	heap = mem_heap_create(2 * (key_parts * sizeof(dfield_t)
				    + sizeof(dtuple_t)));

	range_start= dtuple_create(heap, key_parts);
	dict_index_copy_types(range_start, index, key_parts);

	range_end= dtuple_create(heap, key_parts);
	dict_index_copy_types(range_end, index, key_parts);

	row_sel_convert_mysql_key_to_innobase(
				range_start,
				srch_key_val1, sizeof(srch_key_val1),
				index,
				(byte*) (min_key ? min_key->key :
					 (const uchar*) 0),
				(ulint) (min_key ? min_key->length : 0),
				prebuilt->trx);
	DBUG_ASSERT(min_key
		    ? range_start->n_fields > 0
		    : range_start->n_fields == 0);

	row_sel_convert_mysql_key_to_innobase(
				range_end,
				srch_key_val2, sizeof(srch_key_val2),
				index,
				(byte*) (max_key ? max_key->key :
					 (const uchar*) 0),
				(ulint) (max_key ? max_key->length : 0),
				prebuilt->trx);
	DBUG_ASSERT(max_key
		    ? range_end->n_fields > 0
		    : range_end->n_fields == 0);

	mode1 = convert_search_mode_to_innobase(min_key ? min_key->flag :
						HA_READ_KEY_EXACT);
	mode2 = convert_search_mode_to_innobase(max_key ? max_key->flag :
						HA_READ_KEY_EXACT);

	if (mode1 != PAGE_CUR_UNSUPP && mode2 != PAGE_CUR_UNSUPP) {

		n_rows = btr_estimate_n_rows_in_range(index, range_start,
						      mode1, range_end,
						      mode2);
	} else {

		n_rows = HA_POS_ERROR;
	}

	mem_heap_free(heap);

func_exit:

	prebuilt->trx->op_info = (char*)"";

	/* The MySQL optimizer seems to believe an estimate of 0 rows is
	always accurate and may return the result 'Empty set' based on that.
	The accuracy is not guaranteed, and even if it were, for a locking
	read we should anyway perform the search to set the next-key lock.
	Add 1 to the value to make sure MySQL does not make the assumption! */

	if (n_rows == 0) {
		n_rows = 1;
	}

	DBUG_RETURN((ha_rows) n_rows);
}

/*********************************************************************//**
Gives an UPPER BOUND to the number of rows in a table. This is used in
filesort.cc.
@return	upper bound of rows */
UNIV_INTERN
ha_rows
ha_innobase::estimate_rows_upper_bound(void)
/*======================================*/
{
	dict_index_t*	index;
	ulonglong	estimate;
	ulonglong	local_data_file_length;
	ulint		stat_n_leaf_pages;

	DBUG_ENTER("estimate_rows_upper_bound");

	/* We do not know if MySQL can call this function before calling
	external_lock(). To be safe, update the thd of the current table
	handle. */

	update_thd(ha_thd());

	prebuilt->trx->op_info = (char*)
				 "calculating upper bound for table rows";

	/* In case MySQL calls this in the middle of a SELECT query, release
	possible adaptive hash latch to avoid deadlocks of threads */

	trx_search_latch_release_if_reserved(prebuilt->trx);

	index = dict_table_get_first_index(prebuilt->table);

	stat_n_leaf_pages = index->stat_n_leaf_pages;

	ut_a(stat_n_leaf_pages > 0);

	local_data_file_length =
		((ulonglong) stat_n_leaf_pages) * UNIV_PAGE_SIZE;


	/* Calculate a minimum length for a clustered index record and from
	that an upper bound for the number of rows. Since we only calculate
	new statistics in row0mysql.c when a table has grown by a threshold
	factor, we must add a safety factor 2 in front of the formula below. */

	estimate = 2 * local_data_file_length /
					 dict_index_calc_min_rec_len(index);

        /* Set num_rows less than MERGEBUFF to simulate the case where we do
        not have enough space to merge the externally sorted file blocks. */
        DBUG_EXECUTE_IF("set_num_rows_lt_MERGEBUFF",
                        estimate = 2;
                        DBUG_SET("-d,set_num_rows_lt_MERGEBUFF");
                       );

	prebuilt->trx->op_info = (char*)"";

	DBUG_RETURN((ha_rows) estimate);
}

/*********************************************************************//**
How many seeks it will take to read through the table. This is to be
comparable to the number returned by records_in_range so that we can
decide if we should scan the table or use keys.
@return	estimated time measured in disk seeks */
UNIV_INTERN
double
ha_innobase::scan_time()
/*====================*/
{
	/* Since MySQL seems to favor table scans too much over index
	searches, we pretend that a sequential read takes the same time
	as a random disk read, that is, we do not divide the following
	by 10, which would be physically realistic. */

	return((double) (prebuilt->table->stat_clustered_index_size));
}

/******************************************************************//**
Calculate the time it takes to read a set of ranges through an index
This enables us to optimise reads for clustered indexes.
@return	estimated time measured in disk seeks */
UNIV_INTERN
double
ha_innobase::read_time(
/*===================*/
	uint	index,	/*!< in: key number */
	uint	ranges,	/*!< in: how many ranges */
	ha_rows rows)	/*!< in: estimated number of rows in the ranges */
{
	ha_rows total_rows;
	double	time_for_scan;

	if (index != table->s->primary_key) {
		/* Not clustered */
		return(handler::read_time(index, ranges, rows));
	}

	/* Assume that the read time is proportional to the scan time for all
	rows + at most one seek per range. */

	time_for_scan = scan_time();

	if ((total_rows = estimate_rows_upper_bound()) < rows) {

		return(time_for_scan);
	}

	return(ranges + (double) rows / (double) total_rows * time_for_scan);
}

/*********************************************************************//**
Calculates the key number used inside MySQL for an Innobase index. We will
first check the "index translation table" for a match of the index to get
the index number. If there does not exist an "index translation table",
or not able to find the index in the translation table, then we will fall back
to the traditional way of looping through dict_index_t list to find a
match. In this case, we have to take into account if we generated a
default clustered index for the table
@return the key number used inside MySQL */
static
unsigned int
innobase_get_mysql_key_number_for_index(
/*====================================*/
	INNOBASE_SHARE*		share,	/*!< in: share structure for index
					translation table. */
	const TABLE*		table,	/*!< in: table in MySQL data
					dictionary */
	dict_table_t*		ib_table,/*!< in: table in Innodb data
					dictionary */
        const dict_index_t*     index)	/*!< in: index */
{
	const dict_index_t*	ind;
	unsigned int		i;

        ut_a(index);

	/* If index does not belong to the table of share structure. Search
	index->table instead */
	if (index->table != ib_table) {
		i = 0;
		ind = dict_table_get_first_index(index->table);

		while (index != ind) {
			ind = dict_table_get_next_index(ind);
			i++;
		}

		if (row_table_got_default_clust_index(index->table)) {
			ut_a(i > 0);
			i--;
		}

		return(i);
	}

	/* If index translation table exists, we will first check
	the index through index translation table for a match. */
        if (share->idx_trans_tbl.index_mapping) {
		for (i = 0; i < share->idx_trans_tbl.index_count; i++) {
			if (share->idx_trans_tbl.index_mapping[i] == index) {
				return(i);
			}
		}

		/* If index_count in translation table is set to 0, it
		is possible we are in the process of rebuilding table,
		do not spit error in this case */
		if (share->idx_trans_tbl.index_count) {
			/* Print an error message if we cannot find the index
			** in the "index translation table". */
			sql_print_error("Cannot find index %s in InnoDB index "
					"translation table.", index->name);
		}
	}

	/* If we do not have an "index translation table", or not able
	to find the index in the translation table, we'll directly find
	matching index with information from mysql TABLE structure and
	InnoDB dict_index_t list */
	for (i = 0; i < table->s->keys; i++) {
		ind = dict_table_get_index_on_name(
			ib_table, table->key_info[i].name);

		if (index == ind) {
			return(i);
		}
        }

	ut_error;

        return(0);
}

/*********************************************************************//**
Calculate Record Per Key value. Need to exclude the NULL value if
innodb_stats_method is set to "nulls_ignored"
@return estimated record per key value */
static
ha_rows
innodb_rec_per_key(
/*===============*/
	dict_index_t*	index,		/*!< in: dict_index_t structure */
	ulint		i,		/*!< in: the column we are
					calculating rec per key */
	ha_rows		records)	/*!< in: estimated total records */
{
	ha_rows		rec_per_key;

	ut_ad(i < dict_index_get_n_unique(index));

	/* Note the stat_n_diff_key_vals[] stores the diff value with
	n-prefix indexing, so it is always stat_n_diff_key_vals[i + 1] */
	if (index->stat_n_diff_key_vals[i + 1] == 0) {

		rec_per_key = records;
	} else if (srv_innodb_stats_method == SRV_STATS_NULLS_IGNORED) {
		ib_int64_t	num_null;

		/* Number of rows with NULL value in this
		field */
		num_null = records - index->stat_n_non_null_key_vals[i];

		/* In theory, index->stat_n_non_null_key_vals[i]
		should always be less than the number of records.
		Since this is statistics value, the value could
		have slight discrepancy. But we will make sure
		the number of null values is not a negative number. */
		num_null = (num_null < 0) ? 0 : num_null;

		/* If the number of NULL values is the same as or
		large than that of the distinct values, we could
		consider that the table consists mostly of NULL value. 
		Set rec_per_key to 1. */
		if (index->stat_n_diff_key_vals[i + 1] <= num_null) {
			rec_per_key = 1;
		} else {
			/* Need to exclude rows with NULL values from
			rec_per_key calculation */
			rec_per_key = (ha_rows)(
				(records - num_null)
				/ (index->stat_n_diff_key_vals[i + 1]
				   - num_null));
		}
	} else {
		rec_per_key = (ha_rows)
			 (records / index->stat_n_diff_key_vals[i + 1]);
	}

	return(rec_per_key);
}

/*********************************************************************//**
Returns statistics information of the table to the MySQL interpreter,
in various fields of the handle object. */
UNIV_INTERN
int
ha_innobase::info_low(
/*==================*/
	uint	flag,			/*!< in: what information MySQL
					requests */
	bool	called_from_analyze)	/* in: TRUE if called from
					::analyze() */
{
	dict_table_t*	ib_table;
	dict_index_t*	index;
	ha_rows		rec_per_key;
	ib_int64_t	n_rows;
	char		path[FN_REFLEN];
	os_file_stat_t	stat_info;

	DBUG_ENTER("info");

	/* If we are forcing recovery at a high level, we will suppress
	statistics calculation on tables, because that may crash the
	server if an index is badly corrupted. */

	/* We do not know if MySQL can call this function before calling
	external_lock(). To be safe, update the thd of the current table
	handle. */

	update_thd(ha_thd());

	/* In case MySQL calls this in the middle of a SELECT query, release
	possible adaptive hash latch to avoid deadlocks of threads */

	prebuilt->trx->op_info = (char*)"returning various info to MySQL";

	trx_search_latch_release_if_reserved(prebuilt->trx);

	ib_table = prebuilt->table;

	if (flag & HA_STATUS_TIME) {
		if ((called_from_analyze || innobase_stats_on_metadata) &&
		     share->ib_table && !share->ib_table->is_corrupt) {
			/* In sql_show we call with this flag: update
			then statistics so that they are up-to-date */

			if (srv_use_sys_stats_table && !((ib_table->flags >> DICT_TF2_SHIFT) & DICT_TF2_TEMPORARY)
			    && called_from_analyze) {
				/* If the indexes on the table don't have enough rows in SYS_STATS system table, */
				/* they need to be created. */
				dict_index_t*	index;

				prebuilt->trx->op_info = "confirming rows of SYS_STATS to store statistics";

				ut_a(!trx_is_started(prebuilt->trx));

				for (index = dict_table_get_first_index(ib_table);
				     index != NULL;
				     index = dict_table_get_next_index(index)) {
					if (dict_is_older_statistics(index)) {
						row_delete_stats_for_mysql(index, prebuilt->trx);
						innobase_commit_low(prebuilt->trx);
					}
					row_insert_stats_for_mysql(index, prebuilt->trx);
					innobase_commit_low(prebuilt->trx);
				}

				ut_a(!trx_is_started(prebuilt->trx));
			}

			prebuilt->trx->op_info = "updating table statistics";

			DEBUG_SYNC_C("info_before_stats_update");

			dict_update_statistics(
				ib_table,
				FALSE, /* update even if initialized */
				called_from_analyze,
				FALSE /* update even if not changed too much */);

			prebuilt->trx->op_info = "returning various info to MySQL";
		}

	}

	if (flag & HA_STATUS_VARIABLE) {

		ulint	page_size;

		dict_table_stats_lock(ib_table, RW_S_LATCH);

		n_rows = ib_table->stat_n_rows;

		/* Because we do not protect stat_n_rows by any mutex in a
		delete, it is theoretically possible that the value can be
		smaller than zero! TODO: fix this race.

		The MySQL optimizer seems to assume in a left join that n_rows
		is an accurate estimate if it is zero. Of course, it is not,
		since we do not have any locks on the rows yet at this phase.
		Since SHOW TABLE STATUS seems to call this function with the
		HA_STATUS_TIME flag set, while the left join optimizer does not
		set that flag, we add one to a zero value if the flag is not
		set. That way SHOW TABLE STATUS will show the best estimate,
		while the optimizer never sees the table empty. */

		if (n_rows < 0) {
			n_rows = 0;
		}

		if (n_rows == 0 && !(flag & HA_STATUS_TIME)) {
			n_rows++;
		}

		/* Fix bug#40386: Not flushing query cache after truncate.
		n_rows can not be 0 unless the table is empty, set to 1
		instead. The original problem of bug#29507 is actually
		fixed in the server code. */
		if (thd_sql_command(user_thd) == SQLCOM_TRUNCATE) {

			n_rows = 1;

			/* We need to reset the prebuilt value too, otherwise
			checks for values greater than the last value written
			to the table will fail and the autoinc counter will
			not be updated. This will force write_row() into
			attempting an update of the table's AUTOINC counter. */

			prebuilt->autoinc_last_value = 0;
		}

		page_size = dict_table_zip_size(ib_table);
		if (page_size == 0) {
			page_size = UNIV_PAGE_SIZE;
		}

		stats.records = (ha_rows)n_rows;
		stats.deleted = 0;
		stats.data_file_length
			= ((ulonglong) ib_table->stat_clustered_index_size)
			* page_size;
		stats.index_file_length =
			((ulonglong) ib_table->stat_sum_of_other_index_sizes)
			* page_size;

		dict_table_stats_unlock(ib_table, RW_S_LATCH);

		/* Since fsp_get_available_space_in_free_extents() is
		acquiring latches inside InnoDB, we do not call it if we
		are asked by MySQL to avoid locking. Another reason to
		avoid the call is that it uses quite a lot of CPU.
		See Bug#38185. */
		if (flag & HA_STATUS_NO_LOCK || !srv_stats_update_need_lock
		    || !(flag & HA_STATUS_VARIABLE_EXTRA)) {
			/* We do not update delete_length if no
			locking is requested so the "old" value can
			remain. delete_length is initialized to 0 in
			the ha_statistics' constructor. Also we only
			need delete_length to be set when
			HA_STATUS_VARIABLE_EXTRA is set */
		} else if (UNIV_UNLIKELY
			   (srv_force_recovery >= SRV_FORCE_NO_IBUF_MERGE)) {
			/* Avoid accessing the tablespace if
			innodb_crash_recovery is set to a high value. */
			stats.delete_length = 0;
		} else {
			ullint	avail_space;

			avail_space = fsp_get_available_space_in_free_extents(
				ib_table->space);

			if (avail_space == ULLINT_UNDEFINED) {
				THD*	thd;

				thd = ha_thd();

				push_warning_printf(
					thd,
					MYSQL_ERROR::WARN_LEVEL_WARN,
					ER_CANT_GET_STAT,
					"InnoDB: Trying to get the free "
					"space for table %s but its "
					"tablespace has been discarded or "
					"the .ibd file is missing. Setting "
					"the free space to zero.",
					ib_table->name);

				stats.delete_length = 0;
			} else {
				stats.delete_length = avail_space * 1024;
			}
		}

		stats.check_time = 0;
		stats.mrr_length_per_rec= ref_length +  portable_sizeof_char_ptr;

		if (stats.records == 0) {
			stats.mean_rec_length = 0;
		} else {
			stats.mean_rec_length = (ulong) (stats.data_file_length / stats.records);
		}
	}

	if (flag & HA_STATUS_CONST) {
		ulong	i;
		/* Verify the number of index in InnoDB and MySQL
		matches up. If prebuilt->clust_index_was_generated
		holds, InnoDB defines GEN_CLUST_INDEX internally */
		ulint	num_innodb_index = UT_LIST_GET_LEN(ib_table->indexes)
					- prebuilt->clust_index_was_generated;

		if (table->s->keys != num_innodb_index) {
			sql_print_error("Table %s contains %lu "
					"indexes inside InnoDB, which "
					"is different from the number of "
					"indexes %u defined in the MySQL ",
					ib_table->name, num_innodb_index,
					table->s->keys);
		}

		dict_table_stats_lock(ib_table, RW_S_LATCH);

		for (i = 0; i < table->s->keys; i++) {
			ulong	j;
                        rec_per_key = 1;
			/* We could get index quickly through internal
			index mapping with the index translation table.
			The identity of index (match up index name with
			that of table->key_info[i]) is already verified in
			innobase_get_index().  */
			index = innobase_get_index(i);

			if (index == NULL) {
				sql_print_error("Table %s contains fewer "
						"indexes inside InnoDB than "
						"are defined in the MySQL "
						".frm file. Have you mixed up "
						".frm files from different "
						"installations? See "
						REFMAN
						"innodb-troubleshooting.html\n",
						ib_table->name);
				break;
			}

			for (j = 0; j < table->key_info[i].key_parts; j++) {

				if (j + 1 > index->n_uniq) {
					sql_print_error(
"Index %s of %s has %lu columns unique inside InnoDB, but MySQL is asking "
"statistics for %lu columns. Have you mixed up .frm files from different "
"installations? "
"See " REFMAN "innodb-troubleshooting.html\n",
							index->name,
							ib_table->name,
							(unsigned long)
							index->n_uniq, j + 1);
					break;
				}

				rec_per_key = innodb_rec_per_key(
					index, j, stats.records);

				/* Since MySQL seems to favor table scans
				too much over index searches, we pretend
				index selectivity is 2 times better than
				our estimate: */

				rec_per_key = rec_per_key / 2;

				if (rec_per_key == 0) {
					rec_per_key = 1;
				}

				table->key_info[i].rec_per_key[j]=
				  rec_per_key >= ~(ulong) 0 ? ~(ulong) 0 :
				  (ulong) rec_per_key;
			}

                        KEY *key_info= table->key_info+i; 
                        key_part_map ext_key_part_map=
                                             key_info->ext_key_part_map;                               

                        if (key_info->key_parts != key_info->ext_key_parts) {

                                KEY *pk_key_info= key_info+
                                                  table->s->primary_key;
                                uint k = key_info->key_parts;
                                ha_rows k_rec_per_key = rec_per_key;
                                uint pk_parts = pk_key_info->key_parts;
                          
		                index= innobase_get_index(
                                        table->s->primary_key);
                                
                                n_rows= ib_table->stat_n_rows;
    
                                for (j = 0; j < pk_parts; j++) {
 
				         if (ext_key_part_map & 1<<j) {

                                                rec_per_key =
						innodb_rec_per_key(index,
                                                        j, stats.records);
                               
				                if (rec_per_key == 0) {
					                rec_per_key = 1;
				                }
                                                else if (rec_per_key > 1) {
                                                        rec_per_key =
                                                          (ha_rows)
                                                          (k_rec_per_key *
						          (double)rec_per_key /
							   n_rows);
						}
                                                
				                key_info->rec_per_key[k++]=
				                rec_per_key >= ~(ulong) 0 ?
                                                ~(ulong) 0 :
                                                (ulong) rec_per_key;

					} 
				}
			}                                         
		}

		dict_table_stats_unlock(ib_table, RW_S_LATCH);

		my_snprintf(path, sizeof(path), "%s/%s%s",
			    mysql_data_home,
			    table->s->normalized_path.str,
			    reg_ext);

		unpack_filename(path,path);

		/* Note that we do not know the access time of the table,
		nor the CHECK TABLE time, nor the UPDATE or INSERT time. */

		if (os_file_get_status(path,&stat_info)) {
			stats.create_time = (ulong) stat_info.ctime;
		}
	}

	if (srv_force_recovery >= SRV_FORCE_NO_IBUF_MERGE) {

		goto func_exit;
	}

	if (flag & HA_STATUS_ERRKEY) {
		const dict_index_t*	err_index;

		ut_a(prebuilt->trx);
		ut_a(prebuilt->trx->magic_n == TRX_MAGIC_N);

		err_index = trx_get_error_info(prebuilt->trx);

		if (err_index) {
			errkey = innobase_get_mysql_key_number_for_index(
					share, table, ib_table, err_index);
		} else {
			errkey = (unsigned int) prebuilt->trx->error_key_num;
		}
	}

	if ((flag & HA_STATUS_AUTO) && table->found_next_number_field) {
		stats.auto_increment_value = innobase_peek_autoinc();
	}

func_exit:
	prebuilt->trx->op_info = (char*)"";

	DBUG_RETURN(0);
}

/*********************************************************************//**
Returns statistics information of the table to the MySQL interpreter,
in various fields of the handle object. */
UNIV_INTERN
int
ha_innobase::info(
/*==============*/
	uint	flag)	/*!< in: what information MySQL requests */
{
	return(info_low(flag, false /* not called from analyze */));
}

/**********************************************************************//**
Updates index cardinalities of the table, based on 8 random dives into
each index tree. This does NOT calculate exact statistics on the table.
@return	returns always 0 (success) */
UNIV_INTERN
int
ha_innobase::analyze(
/*=================*/
	THD*		thd,		/*!< in: connection thread handle */
	HA_CHECK_OPT*	check_opt)	/*!< in: currently ignored */
{
	if (!share->ib_table || share->ib_table->is_corrupt) {
		return(HA_ADMIN_CORRUPT);
	}

	/* Simply call ::info() with all the flags */
	info_low(HA_STATUS_TIME | HA_STATUS_CONST | HA_STATUS_VARIABLE,
		 true /* called from analyze */);

	if (!share->ib_table || share->ib_table->is_corrupt) {
		return(HA_ADMIN_CORRUPT);
	}

	return(0);
}

/**********************************************************************//**
This is mapped to "ALTER TABLE tablename ENGINE=InnoDB", which rebuilds
the table in MySQL. */
UNIV_INTERN
int
ha_innobase::optimize(
/*==================*/
	THD*		thd,		/*!< in: connection thread handle */
	HA_CHECK_OPT*	check_opt)	/*!< in: currently ignored */
{
	return(HA_ADMIN_TRY_ALTER);
}

/*******************************************************************//**
Tries to check that an InnoDB table is not corrupted. If corruption is
noticed, prints to stderr information about it. In case of corruption
may also assert a failure and crash the server.
@return	HA_ADMIN_CORRUPT or HA_ADMIN_OK */
UNIV_INTERN
int
ha_innobase::check(
/*===============*/
	THD*		thd,		/*!< in: user thread handle */
	HA_CHECK_OPT*	check_opt)	/*!< in: check options */
{
	dict_index_t*	index;
	ulint		n_rows;
	ulint		n_rows_in_table	= ULINT_UNDEFINED;
	ibool		is_ok		= TRUE;
	ulint		old_isolation_level;
	ibool		table_corrupted;

	DBUG_ENTER("ha_innobase::check");
	DBUG_ASSERT(thd == ha_thd());
	ut_a(prebuilt->trx);
	ut_a(prebuilt->trx->magic_n == TRX_MAGIC_N);
	ut_a(prebuilt->trx == thd_to_trx(thd));

	if (prebuilt->mysql_template == NULL) {
		/* Build the template; we will use a dummy template
		in index scans done in checking */

		build_template(true);
	}

	if (prebuilt->table->ibd_file_missing) {
		sql_print_error("InnoDB: Error:\n"
			"InnoDB: MySQL is trying to use a table handle"
			" but the .ibd file for\n"
			"InnoDB: table %s does not exist.\n"
			"InnoDB: Have you deleted the .ibd file"
			" from the database directory under\n"
			"InnoDB: the MySQL datadir, or have you"
			" used DISCARD TABLESPACE?\n"
			"InnoDB: Please refer to\n"
			"InnoDB: " REFMAN "innodb-troubleshooting.html\n"
			"InnoDB: how you can resolve the problem.\n",
			prebuilt->table->name);
		DBUG_RETURN(HA_ADMIN_CORRUPT);
	}

	if (prebuilt->table->corrupted) {
		char	index_name[MAX_FULL_NAME_LEN + 1];
		/* If some previous operation has marked the table as
		corrupted in memory, and has not propagated such to
		clustered index, we will do so here */
		index = dict_table_get_first_index(prebuilt->table);

		if (!dict_index_is_corrupted(index)) {
			row_mysql_lock_data_dictionary(prebuilt->trx);
			dict_set_corrupted(index);
			row_mysql_unlock_data_dictionary(prebuilt->trx);
		}

		innobase_format_name(index_name, sizeof index_name,
			index->name, TRUE);

		push_warning_printf(thd,
				    MYSQL_ERROR::WARN_LEVEL_WARN,
				    HA_ERR_INDEX_CORRUPT,
				    "InnoDB: Index %s is marked as"
				    " corrupted",
				    index_name);

		/* Now that the table is already marked as corrupted,
		there is no need to check any index of this table */
		prebuilt->trx->op_info = "";

		DBUG_RETURN(HA_ADMIN_CORRUPT);
	}

	prebuilt->trx->op_info = "checking table";

	old_isolation_level = prebuilt->trx->isolation_level;

	/* We must run the index record counts at an isolation level
	>= READ COMMITTED, because a dirty read can see a wrong number
	of records in some index; to play safe, we use always
	REPEATABLE READ here */

	prebuilt->trx->isolation_level = TRX_ISO_REPEATABLE_READ;

	/* Check whether the table is already marked as corrupted
	before running the check table */
	table_corrupted = prebuilt->table->corrupted;

	/* Reset table->corrupted bit so that check table can proceed to
	do additional check */
	prebuilt->table->corrupted = FALSE;

	for (index = dict_table_get_first_index(prebuilt->table);
	     index != NULL;
	     index = dict_table_get_next_index(index)) {
		char	index_name[MAX_FULL_NAME_LEN + 1];
#if 0
		fputs("Validating index ", stderr);
		ut_print_name(stderr, trx, FALSE, index->name);
		putc('\n', stderr);
#endif

		/* If this is an index being created, break */
		if (*index->name == TEMP_INDEX_PREFIX) {
			continue;
		}
		if (!(check_opt->flags & T_QUICK)) {
			/* Enlarge the fatal lock wait timeout during
			CHECK TABLE. */
			mutex_enter(&kernel_mutex);
			srv_fatal_semaphore_wait_threshold +=
				SRV_SEMAPHORE_WAIT_EXTENSION;
			mutex_exit(&kernel_mutex);

			ibool	valid = TRUE;
			valid = btr_validate_index(index, prebuilt->trx);

			/* Restore the fatal lock wait timeout after
			CHECK TABLE. */
			mutex_enter(&kernel_mutex);
			srv_fatal_semaphore_wait_threshold -=
				SRV_SEMAPHORE_WAIT_EXTENSION;
			mutex_exit(&kernel_mutex);

			if (!valid) {
				is_ok = FALSE;

				innobase_format_name(
					index_name, sizeof index_name,
					index->name, TRUE);
				push_warning_printf(thd,
					MYSQL_ERROR::WARN_LEVEL_WARN,
					ER_NOT_KEYFILE,
					"InnoDB: The B-tree of"
					" index %s is corrupted.",
					index_name);

				continue;
			}
		}

		/* Instead of invoking change_active_index(), set up
		a dummy template for non-locking reads, disabling
		access to the clustered index. */
		prebuilt->index = index;

		prebuilt->index_usable = row_merge_is_index_usable(
			prebuilt->trx, prebuilt->index);

		DBUG_EXECUTE_IF(
			"dict_set_index_corrupted",
			if (!dict_index_is_clust(index)) {
				prebuilt->index_usable = FALSE;
				row_mysql_lock_data_dictionary(prebuilt->trx);
				dict_set_corrupted(index);
				row_mysql_unlock_data_dictionary(prebuilt->trx);
			});

		if (UNIV_UNLIKELY(!prebuilt->index_usable)) {
			innobase_format_name(
				index_name, sizeof index_name,
				prebuilt->index->name, TRUE);

			if (dict_index_is_corrupted(prebuilt->index)) {
				push_warning_printf(
					user_thd, MYSQL_ERROR::WARN_LEVEL_WARN,
					HA_ERR_INDEX_CORRUPT,
					"InnoDB: Index %s is marked as"
					" corrupted",
					index_name);
				is_ok = FALSE;
			} else {
				push_warning_printf(
					thd, MYSQL_ERROR::WARN_LEVEL_WARN,
					HA_ERR_TABLE_DEF_CHANGED,
					"InnoDB: Insufficient history for"
					" index %s",
					index_name);
			}
			continue;
		}

		prebuilt->sql_stat_start = TRUE;
		prebuilt->template_type = ROW_MYSQL_DUMMY_TEMPLATE;
		prebuilt->n_template = 0;
		prebuilt->need_to_access_clustered = FALSE;

		dtuple_set_n_fields(prebuilt->search_tuple, 0);

		prebuilt->select_lock_type = LOCK_NONE;

		bool check_result
			= row_check_index_for_mysql(prebuilt, index, &n_rows);
		DBUG_EXECUTE_IF(
			"dict_set_index_corrupted",
			if (!(index->type & DICT_CLUSTERED)) {
				check_result = false;
			});
		if (!check_result) {
			innobase_format_name(
				index_name, sizeof index_name,
				index->name, TRUE);

			push_warning_printf(thd, MYSQL_ERROR::WARN_LEVEL_WARN,
					    ER_NOT_KEYFILE,
					    "InnoDB: The B-tree of"
					    " index %s is corrupted.",
					    index_name);
			is_ok = FALSE;
			row_mysql_lock_data_dictionary(prebuilt->trx);
			dict_set_corrupted(index);
			row_mysql_unlock_data_dictionary(prebuilt->trx);
		}

		if (thd_kill_level(user_thd)) {
			break;
		}

#if 0
		fprintf(stderr, "%lu entries in index %s\n", n_rows,
			index->name);
#endif

		if (index == dict_table_get_first_index(prebuilt->table)) {
			n_rows_in_table = n_rows;
		} else if (n_rows != n_rows_in_table) {
			push_warning_printf(thd, MYSQL_ERROR::WARN_LEVEL_WARN,
					    ER_NOT_KEYFILE,
					    "InnoDB: Index '%-.200s'"
					    " contains %lu entries,"
					    " should be %lu.",
					    index->name,
					    (ulong) n_rows,
					    (ulong) n_rows_in_table);
			is_ok = FALSE;
			row_mysql_lock_data_dictionary(prebuilt->trx);
			dict_set_corrupted(index);
			row_mysql_unlock_data_dictionary(prebuilt->trx);
		}
	}

	if (table_corrupted) {
		/* If some previous operation has marked the table as
		corrupted in memory, and has not propagated such to
		clustered index, we will do so here */
		index = dict_table_get_first_index(prebuilt->table);

		if (!dict_index_is_corrupted(index)) {
			mutex_enter(&dict_sys->mutex);
			dict_set_corrupted(index);
			mutex_exit(&dict_sys->mutex);
		}
		prebuilt->table->corrupted = TRUE;
	}

	/* Restore the original isolation level */
	prebuilt->trx->isolation_level = old_isolation_level;

#if defined UNIV_AHI_DEBUG || defined UNIV_DEBUG
	/* We validate the whole adaptive hash index for all tables
	at every CHECK TABLE only when QUICK flag is not present. */

	if (!(check_opt->flags & T_QUICK) && !btr_search_validate()) {
		push_warning(thd, MYSQL_ERROR::WARN_LEVEL_WARN,
			     ER_NOT_KEYFILE,
			     "InnoDB: The adaptive hash index is corrupted.");
		is_ok = FALSE;
	}
#endif /* defined UNIV_AHI_DEBUG || defined UNIV_DEBUG */
	prebuilt->trx->op_info = "";
	if (thd_kill_level(user_thd)) {
		my_error(ER_QUERY_INTERRUPTED, MYF(0));
	}

	if (!share->ib_table || share->ib_table->is_corrupt) {
		return(HA_ADMIN_CORRUPT);
	}

	DBUG_RETURN(is_ok ? HA_ADMIN_OK : HA_ADMIN_CORRUPT);
}

/*************************************************************//**
Adds information about free space in the InnoDB tablespace to a table comment
which is printed out when a user calls SHOW TABLE STATUS. Adds also info on
foreign keys.
@return	table comment + InnoDB free space + info on foreign keys */
UNIV_INTERN
char*
ha_innobase::update_table_comment(
/*==============================*/
	const char*	comment)/*!< in: table comment defined by user */
{
	uint	length = (uint) strlen(comment);
	char*	str;
	long	flen;

	/* We do not know if MySQL can call this function before calling
	external_lock(). To be safe, update the thd of the current table
	handle. */

	if (length > 64000 - 3) {
		return((char*)comment); /* string too long */
	}

	update_thd(ha_thd());

	prebuilt->trx->op_info = (char*)"returning table comment";

	/* In case MySQL calls this in the middle of a SELECT query, release
	possible adaptive hash latch to avoid deadlocks of threads */

	trx_search_latch_release_if_reserved(prebuilt->trx);
	str = NULL;

	/* output the data to a temporary file */

	mutex_enter(&srv_dict_tmpfile_mutex);
	rewind(srv_dict_tmpfile);

	fprintf(srv_dict_tmpfile, "InnoDB free: %llu kB",
		fsp_get_available_space_in_free_extents(
			prebuilt->table->space));

	dict_print_info_on_foreign_keys(FALSE, srv_dict_tmpfile,
				prebuilt->trx, prebuilt->table);
	flen = ftell(srv_dict_tmpfile);
	if (flen < 0) {
		flen = 0;
	} else if (length + flen + 3 > 64000) {
		flen = 64000 - 3 - length;
	}

	/* allocate buffer for the full string, and
	read the contents of the temporary file */

	str = (char*) my_malloc(length + flen + 3, MYF(0));

	if (str) {
		char* pos	= str + length;
		if (length) {
			memcpy(str, comment, length);
			*pos++ = ';';
			*pos++ = ' ';
		}
		rewind(srv_dict_tmpfile);
		flen = (uint) fread(pos, 1, flen, srv_dict_tmpfile);
		pos[flen] = 0;
	}

	mutex_exit(&srv_dict_tmpfile_mutex);

	prebuilt->trx->op_info = (char*)"";

	return(str ? str : (char*) comment);
}

/*******************************************************************//**
Gets the foreign key create info for a table stored in InnoDB.
@return own: character string in the form which can be inserted to the
CREATE TABLE statement, MUST be freed with
ha_innobase::free_foreign_key_create_info */
UNIV_INTERN
char*
ha_innobase::get_foreign_key_create_info(void)
/*==========================================*/
{
	char*	str	= 0;
	long	flen;

	ut_a(prebuilt != NULL);

	/* We do not know if MySQL can call this function before calling
	external_lock(). To be safe, update the thd of the current table
	handle. */

	update_thd(ha_thd());

	prebuilt->trx->op_info = (char*)"getting info on foreign keys";

	/* In case MySQL calls this in the middle of a SELECT query,
	release possible adaptive hash latch to avoid
	deadlocks of threads */

	trx_search_latch_release_if_reserved(prebuilt->trx);

	mutex_enter(&srv_dict_tmpfile_mutex);
	rewind(srv_dict_tmpfile);

	/* output the data to a temporary file */
	dict_print_info_on_foreign_keys(TRUE, srv_dict_tmpfile,
				prebuilt->trx, prebuilt->table);
	prebuilt->trx->op_info = (char*)"";

	flen = ftell(srv_dict_tmpfile);
	if (flen < 0) {
		flen = 0;
	}

	/* allocate buffer for the string, and
	read the contents of the temporary file */

	str = (char*) my_malloc(flen + 1, MYF(0));

	if (str) {
		rewind(srv_dict_tmpfile);
		flen = (uint) fread(str, 1, flen, srv_dict_tmpfile);
		str[flen] = 0;
	}

	mutex_exit(&srv_dict_tmpfile_mutex);

	return(str);
}


/***********************************************************************//**
Maps a InnoDB foreign key constraint to a equivalent MySQL foreign key info.
@return pointer to foreign key info */
static
FOREIGN_KEY_INFO*
get_foreign_key_info(
/*=================*/
	THD*			thd,		/*!< in: user thread handle */
	dict_foreign_t*		foreign)	/*!< in: foreign key constraint */
{
	FOREIGN_KEY_INFO	f_key_info;
	FOREIGN_KEY_INFO*	pf_key_info;
	uint			i = 0;
	ulint			len;
	char			tmp_buff[NAME_LEN+1];
	char			name_buff[NAME_LEN+1];
	const char*		ptr;
	LEX_STRING*		referenced_key_name;
	LEX_STRING*		name = NULL;

	ptr = dict_remove_db_name(foreign->id);
	f_key_info.foreign_id = thd_make_lex_string(thd, 0, ptr,
						    (uint) strlen(ptr), 1);

	/* Name format: database name, '/', table name, '\0' */

	/* Referenced (parent) database name */
	len = dict_get_db_name_len(foreign->referenced_table_name);
	ut_a(len < sizeof(tmp_buff));
	ut_memcpy(tmp_buff, foreign->referenced_table_name, len);
	tmp_buff[len] = 0;

	len = filename_to_tablename(tmp_buff, name_buff, sizeof(name_buff));
	f_key_info.referenced_db = thd_make_lex_string(thd, 0, name_buff, len, 1);

	/* Referenced (parent) table name */
	ptr = dict_remove_db_name(foreign->referenced_table_name);
	len = filename_to_tablename(ptr, name_buff, sizeof(name_buff));
	f_key_info.referenced_table = thd_make_lex_string(thd, 0, name_buff, len, 1);

	/* Dependent (child) database name */
	len = dict_get_db_name_len(foreign->foreign_table_name);
	ut_a(len < sizeof(tmp_buff));
	ut_memcpy(tmp_buff, foreign->foreign_table_name, len);
	tmp_buff[len] = 0;

	len = filename_to_tablename(tmp_buff, name_buff, sizeof(name_buff));
	f_key_info.foreign_db = thd_make_lex_string(thd, 0, name_buff, len, 1);

	/* Dependent (child) table name */
	ptr = dict_remove_db_name(foreign->foreign_table_name);
	len = filename_to_tablename(ptr, name_buff, sizeof(name_buff));
	f_key_info.foreign_table = thd_make_lex_string(thd, 0, name_buff, len, 1);

	do {
		ptr = foreign->foreign_col_names[i];
		name = thd_make_lex_string(thd, name, ptr,
					   (uint) strlen(ptr), 1);
		f_key_info.foreign_fields.push_back(name);
		ptr = foreign->referenced_col_names[i];
		name = thd_make_lex_string(thd, name, ptr,
					   (uint) strlen(ptr), 1);
		f_key_info.referenced_fields.push_back(name);
	} while (++i < foreign->n_fields);

	if (foreign->type & DICT_FOREIGN_ON_DELETE_CASCADE) {
		len = 7;
		ptr = "CASCADE";
	} else if (foreign->type & DICT_FOREIGN_ON_DELETE_SET_NULL) {
		len = 8;
		ptr = "SET NULL";
	} else if (foreign->type & DICT_FOREIGN_ON_DELETE_NO_ACTION) {
		len = 9;
		ptr = "NO ACTION";
	} else {
		len = 8;
		ptr = "RESTRICT";
	}

	f_key_info.delete_method = thd_make_lex_string(thd,
						       f_key_info.delete_method,
						       ptr, len, 1);

	if (foreign->type & DICT_FOREIGN_ON_UPDATE_CASCADE) {
		len = 7;
		ptr = "CASCADE";
	} else if (foreign->type & DICT_FOREIGN_ON_UPDATE_SET_NULL) {
		len = 8;
		ptr = "SET NULL";
	} else if (foreign->type & DICT_FOREIGN_ON_UPDATE_NO_ACTION) {
		len = 9;
		ptr = "NO ACTION";
	} else {
		len = 8;
		ptr = "RESTRICT";
	}

	f_key_info.update_method = thd_make_lex_string(thd,
						       f_key_info.update_method,
						       ptr, len, 1);

	if (foreign->referenced_index && foreign->referenced_index->name) {
		referenced_key_name = thd_make_lex_string(thd,
					f_key_info.referenced_key_name,
					foreign->referenced_index->name,
					 (uint) strlen(foreign->referenced_index->name),
					1);
	} else {
		referenced_key_name = NULL;
	}

	f_key_info.referenced_key_name = referenced_key_name;

	pf_key_info = (FOREIGN_KEY_INFO *) thd_memdup(thd, &f_key_info,
						      sizeof(FOREIGN_KEY_INFO));

	return(pf_key_info);
}

/** Get the list of foreign keys referencing a specified table
table.
@param thd		The thread handle
@param path		Path to the table
@param f_key_list[out]	The list of foreign keys */
static
void
fill_foreign_key_list(THD* thd,
		      const dict_table_t* table,
		      List<FOREIGN_KEY_INFO>* f_key_list)
{
	ut_ad(mutex_own(&dict_sys->mutex));

	for (dict_foreign_t* foreign
		     = UT_LIST_GET_FIRST(table->referenced_list);
	     foreign != NULL;
	     foreign = UT_LIST_GET_NEXT(referenced_list, foreign)) {

		FOREIGN_KEY_INFO* pf_key_info
			= get_foreign_key_info(thd, foreign);
		if (pf_key_info) {
			f_key_list->push_back(pf_key_info);
		}
	}
}

/** Get the list of foreign keys referencing a specified table
table.
@param thd		The thread handle
@param path		Path to the table
@param f_key_list[out]	The list of foreign keys

@return error code or zero for success */
static
int
innobase_get_parent_fk_list(
	THD*			thd,
	const char*		path,
	List<FOREIGN_KEY_INFO>*	f_key_list)
{
	ut_a(strlen(path) <= FN_REFLEN);
	char	norm_name[FN_REFLEN + 1];
	normalize_table_name(norm_name, path);

	trx_t*	parent_trx = check_trx_exists(thd);
	parent_trx->op_info = "getting list of referencing foreign keys";
	trx_search_latch_release_if_reserved(parent_trx);

	mutex_enter(&dict_sys->mutex);

	dict_table_t*	table
		= dict_table_get_low(norm_name,
				     static_cast<dict_err_ignore_t>(
					     DICT_ERR_IGNORE_INDEX_ROOT
					     | DICT_ERR_IGNORE_CORRUPT));
	if (!table) {
		mutex_exit(&dict_sys->mutex);
		return(HA_ERR_NO_SUCH_TABLE);
	}

	fill_foreign_key_list(thd, table, f_key_list);

	mutex_exit(&dict_sys->mutex);
	parent_trx->op_info = "";
	return(0);
}

/*******************************************************************//**
Gets the list of foreign keys in this table.
@return always 0, that is, always succeeds */
UNIV_INTERN
int
ha_innobase::get_foreign_key_list(
/*==============================*/
	THD*			thd,		/*!< in: user thread handle */
	List<FOREIGN_KEY_INFO>*	f_key_list)	/*!< out: foreign key list */
{
	FOREIGN_KEY_INFO*	pf_key_info;
	dict_foreign_t*		foreign;

	ut_a(prebuilt != NULL);
	update_thd(ha_thd());

	prebuilt->trx->op_info = "getting list of foreign keys";

	trx_search_latch_release_if_reserved(prebuilt->trx);

	mutex_enter(&(dict_sys->mutex));

	for (foreign = UT_LIST_GET_FIRST(prebuilt->table->foreign_list);
	     foreign != NULL;
	     foreign = UT_LIST_GET_NEXT(foreign_list, foreign)) {
		pf_key_info = get_foreign_key_info(thd, foreign);
		if (pf_key_info) {
			f_key_list->push_back(pf_key_info);
		}
	}

	mutex_exit(&(dict_sys->mutex));

	prebuilt->trx->op_info = "";

	return(0);
}

/*******************************************************************//**
Gets the set of foreign keys where this table is the referenced table.
@return always 0, that is, always succeeds */
UNIV_INTERN
int
ha_innobase::get_parent_foreign_key_list(
/*=====================================*/
	THD*			thd,		/*!< in: user thread handle */
	List<FOREIGN_KEY_INFO>*	f_key_list)	/*!< out: foreign key list */
{
	ut_a(prebuilt != NULL);
	update_thd(ha_thd());

	prebuilt->trx->op_info = "getting list of referencing foreign keys";

	trx_search_latch_release_if_reserved(prebuilt->trx);

	mutex_enter(&(dict_sys->mutex));
	fill_foreign_key_list(thd, prebuilt->table, f_key_list);
	mutex_exit(&(dict_sys->mutex));

	prebuilt->trx->op_info = "";

	return(0);
}

/*****************************************************************//**
Checks if ALTER TABLE may change the storage engine of the table.
Changing storage engines is not allowed for tables for which there
are foreign key constraints (parent or child tables).
@return	TRUE if can switch engines */
UNIV_INTERN
bool
ha_innobase::can_switch_engines(void)
/*=================================*/
{
	bool	can_switch;

	DBUG_ENTER("ha_innobase::can_switch_engines");

	ut_a(prebuilt->trx == thd_to_trx(ha_thd()));

	prebuilt->trx->op_info =
			"determining if there are foreign key constraints";
	row_mysql_lock_data_dictionary(prebuilt->trx);

	can_switch = !UT_LIST_GET_FIRST(prebuilt->table->referenced_list)
			&& !UT_LIST_GET_FIRST(prebuilt->table->foreign_list);

	row_mysql_unlock_data_dictionary(prebuilt->trx);
	prebuilt->trx->op_info = "";

	DBUG_RETURN(can_switch);
}

/*******************************************************************//**
Checks if a table is referenced by a foreign key. The MySQL manual states that
a REPLACE is either equivalent to an INSERT, or DELETE(s) + INSERT. Only a
delete is then allowed internally to resolve a duplicate key conflict in
REPLACE, not an update.
@return	> 0 if referenced by a FOREIGN KEY */
UNIV_INTERN
uint
ha_innobase::referenced_by_foreign_key(void)
/*========================================*/
{
	if (dict_table_is_referenced_by_foreign_key(prebuilt->table)) {

		return(1);
	}

	return(0);
}

/*******************************************************************//**
Frees the foreign key create info for a table stored in InnoDB, if it is
non-NULL. */
UNIV_INTERN
void
ha_innobase::free_foreign_key_create_info(
/*======================================*/
	char*	str)	/*!< in, own: create info string to free */
{
	if (str) {
		my_free(str);
	}
}

/*******************************************************************//**
Tells something additional to the handler about how to do things.
@return	0 or error number */
UNIV_INTERN
int
ha_innobase::extra(
/*===============*/
	enum ha_extra_function operation)
			   /*!< in: HA_EXTRA_FLUSH or some other flag */
{
	/* Warning: since it is not sure that MySQL calls external_lock
	before calling this function, the trx field in prebuilt can be
	obsolete! */

	switch (operation) {
		case HA_EXTRA_FLUSH:
			if (prebuilt->blob_heap) {
				row_mysql_prebuilt_free_blob_heap(prebuilt);
			}
			break;
		case HA_EXTRA_RESET_STATE:
			reset_template();
			thd_to_trx(ha_thd())->duplicates = 0;
			break;
		case HA_EXTRA_NO_KEYREAD:
			prebuilt->read_just_key = 0;
			break;
		case HA_EXTRA_KEYREAD:
			prebuilt->read_just_key = 1;
			break;
		case HA_EXTRA_KEYREAD_PRESERVE_FIELDS:
			prebuilt->keep_other_fields_on_keyread = 1;
			break;

			/* IMPORTANT: prebuilt->trx can be obsolete in
			this method, because it is not sure that MySQL
			calls external_lock before this method with the
			parameters below.  We must not invoke update_thd()
			either, because the calling threads may change.
			CAREFUL HERE, OR MEMORY CORRUPTION MAY OCCUR! */
		case HA_EXTRA_INSERT_WITH_UPDATE:
			thd_to_trx(ha_thd())->duplicates |= TRX_DUP_IGNORE;
			break;
		case HA_EXTRA_NO_IGNORE_DUP_KEY:
			thd_to_trx(ha_thd())->duplicates &= ~TRX_DUP_IGNORE;
			break;
		case HA_EXTRA_WRITE_CAN_REPLACE:
			thd_to_trx(ha_thd())->duplicates |= TRX_DUP_REPLACE;
			break;
		case HA_EXTRA_WRITE_CANNOT_REPLACE:
			thd_to_trx(ha_thd())->duplicates &= ~TRX_DUP_REPLACE;
			break;
		default:/* Do nothing */
			;
	}

	return(0);
}

UNIV_INTERN
int
ha_innobase::reset()
{
	if (prebuilt->blob_heap) {
		row_mysql_prebuilt_free_blob_heap(prebuilt);
	}

	reset_template();
	ds_mrr.dsmrr_close();

	/* TODO: This should really be reset in reset_template() but for now
	it's safer to do it explicitly here. */

	/* This is a statement level counter. */
	prebuilt->autoinc_last_value = 0;

	return(0);
}

/******************************************************************//**
MySQL calls this function at the start of each SQL statement inside LOCK
TABLES. Inside LOCK TABLES the ::external_lock method does not work to
mark SQL statement borders. Note also a special case: if a temporary table
is created inside LOCK TABLES, MySQL has not called external_lock() at all
on that table.
MySQL-5.0 also calls this before each statement in an execution of a stored
procedure. To make the execution more deterministic for binlogging, MySQL-5.0
locks all tables involved in a stored procedure with full explicit table
locks (thd_in_lock_tables(thd) holds in store_lock()) before executing the
procedure.
@return	0 or error code */
UNIV_INTERN
int
ha_innobase::start_stmt(
/*====================*/
	THD*		thd,	/*!< in: handle to the user thread */
	thr_lock_type	lock_type)
{
	trx_t*		trx;
	DBUG_ENTER("ha_innobase::start_stmt");

	update_thd(thd);

	trx = prebuilt->trx;

	/* Here we release the search latch and the InnoDB thread FIFO ticket
	if they were reserved. They should have been released already at the
	end of the previous statement, but because inside LOCK TABLES the
	lock count method does not work to mark the end of a SELECT statement,
	that may not be the case. We MUST release the search latch before an
	INSERT, for example. */

	trx_search_latch_release_if_reserved(trx);
	innodb_srv_conc_force_exit_innodb(trx);

	/* Reset the AUTOINC statement level counter for multi-row INSERTs. */
	trx->n_autoinc_rows = 0;

	prebuilt->sql_stat_start = TRUE;
	prebuilt->hint_need_to_fetch_extra_cols = 0;
	reset_template();

	if (dict_table_is_temporary(prebuilt->table)
	    && prebuilt->mysql_has_locked
	    && prebuilt->select_lock_type == LOCK_NONE) {
		ulint error;

		switch (thd_sql_command(thd)) {
		case SQLCOM_INSERT:
		case SQLCOM_UPDATE:
		case SQLCOM_DELETE:
			init_table_handle_for_HANDLER();
			prebuilt->select_lock_type = LOCK_X;
			error = row_lock_table_for_mysql(prebuilt, NULL, 1);

			if (error != DB_SUCCESS) {
				error = convert_error_code_to_mysql(
					(int) error, 0, thd);
				DBUG_RETURN((int) error);
			}
			break;
		}
	}

	if (!prebuilt->mysql_has_locked) {
		/* This handle is for a temporary table created inside
		this same LOCK TABLES; since MySQL does NOT call external_lock
		in this case, we must use x-row locks inside InnoDB to be
		prepared for an update of a row */

		prebuilt->select_lock_type = LOCK_X;

	} else if (trx->isolation_level != TRX_ISO_SERIALIZABLE
		   && thd_sql_command(thd) == SQLCOM_SELECT
		   && lock_type == TL_READ) {

		/* For other than temporary tables, we obtain
		no lock for consistent read (plain SELECT). */

		prebuilt->select_lock_type = LOCK_NONE;
	} else {
		/* Not a consistent read: restore the
		select_lock_type value. The value of
		stored_select_lock_type was decided in:
		1) ::store_lock(),
		2) ::external_lock(),
		3) ::init_table_handle_for_HANDLER(), and
		4) ::transactional_table_lock(). */

		prebuilt->select_lock_type = prebuilt->stored_select_lock_type;
	}

	*trx->detailed_error = 0;

	innobase_register_trx(ht, thd, trx);

	DBUG_RETURN(0);
}

/******************************************************************//**
Maps a MySQL trx isolation level code to the InnoDB isolation level code
@return	InnoDB isolation level */
static inline
ulint
innobase_map_isolation_level(
/*=========================*/
	enum_tx_isolation	iso)	/*!< in: MySQL isolation level code */
{
	switch(iso) {
		case ISO_REPEATABLE_READ: return(TRX_ISO_REPEATABLE_READ);
		case ISO_READ_COMMITTED: return(TRX_ISO_READ_COMMITTED);
		case ISO_SERIALIZABLE: return(TRX_ISO_SERIALIZABLE);
		case ISO_READ_UNCOMMITTED: return(TRX_ISO_READ_UNCOMMITTED);
		default: ut_a(0); return(0);
	}
}

/******************************************************************//**
As MySQL will execute an external lock for every new table it uses when it
starts to process an SQL statement (an exception is when MySQL calls
start_stmt for the handle) we can use this function to store the pointer to
the THD in the handle. We will also use this function to communicate
to InnoDB that a new SQL statement has started and that we must store a
savepoint to our transaction handle, so that we are able to roll back
the SQL statement in case of an error.
@return	0 */
UNIV_INTERN
int
ha_innobase::external_lock(
/*=======================*/
	THD*	thd,		/*!< in: handle to the user thread */
	int	lock_type)	/*!< in: lock type */
{
	trx_t*		trx;

	DBUG_ENTER("ha_innobase::external_lock");
	DBUG_PRINT("enter",("lock_type: %d", lock_type));

	update_thd(thd);

	/* Statement based binlogging does not work in isolation level
	READ UNCOMMITTED and READ COMMITTED since the necessary
	locks cannot be taken. In this case, we print an
	informative error message and return with an error.
	Note: decide_logging_format would give the same error message,
	except it cannot give the extra details. */
	if (lock_type == F_WRLCK
	    && !(table_flags() & HA_BINLOG_STMT_CAPABLE)
	    && thd_binlog_format(thd) == BINLOG_FORMAT_STMT
	    && thd_binlog_filter_ok(thd)
            && thd_sqlcom_can_generate_row_events(thd))
        {
		int skip = 0;
		/* used by test case */
		DBUG_EXECUTE_IF("no_innodb_binlog_errors", skip = 1;);
		if (!skip) {
			my_error(ER_BINLOG_STMT_MODE_AND_ROW_ENGINE, MYF(0),
			         " InnoDB is limited to row-logging when "
			         "transaction isolation level is "
			         "READ COMMITTED or READ UNCOMMITTED.");
			DBUG_RETURN(HA_ERR_LOGGING_IMPOSSIBLE);
		}
	}


	trx = prebuilt->trx;

	prebuilt->sql_stat_start = TRUE;
	prebuilt->hint_need_to_fetch_extra_cols = 0;

	reset_template();

	if (lock_type == F_WRLCK) {

		/* If this is a SELECT, then it is in UPDATE TABLE ...
		or SELECT ... FOR UPDATE */
		prebuilt->select_lock_type = LOCK_X;
		prebuilt->stored_select_lock_type = LOCK_X;
	}

	if (lock_type != F_UNLCK) {
		/* MySQL is setting a new table lock */

		*trx->detailed_error = 0;

		innobase_register_trx(ht, thd, trx);

		if (trx->isolation_level == TRX_ISO_SERIALIZABLE
		    && prebuilt->select_lock_type == LOCK_NONE
		    && thd_test_options(
			    thd, OPTION_NOT_AUTOCOMMIT | OPTION_BEGIN)) {

			/* To get serializable execution, we let InnoDB
			conceptually add 'LOCK IN SHARE MODE' to all SELECTs
			which otherwise would have been consistent reads. An
			exception is consistent reads in the AUTOCOMMIT=1 mode:
			we know that they are read-only transactions, and they
			can be serialized also if performed as consistent
			reads. */

			prebuilt->select_lock_type = LOCK_S;
			prebuilt->stored_select_lock_type = LOCK_S;
		}

		/* Starting from 4.1.9, no InnoDB table lock is taken in LOCK
		TABLES if AUTOCOMMIT=1. It does not make much sense to acquire
		an InnoDB table lock if it is released immediately at the end
		of LOCK TABLES, and InnoDB's table locks in that case cause
		VERY easily deadlocks.

		We do not set InnoDB table locks if user has not explicitly
		requested a table lock. Note that thd_in_lock_tables(thd)
		can hold in some cases, e.g., at the start of a stored
		procedure call (SQLCOM_CALL). */

		if (prebuilt->select_lock_type != LOCK_NONE) {

			if (thd_sql_command(thd) == SQLCOM_LOCK_TABLES
			    && THDVAR(thd, table_locks)
			    && thd_test_options(thd, OPTION_NOT_AUTOCOMMIT)
			    && thd_in_lock_tables(thd)) {

				ulint	error = row_lock_table_for_mysql(
					prebuilt, NULL, 0);

				if (error != DB_SUCCESS) {
					error = convert_error_code_to_mysql(
						(int) error, 0, thd);
					DBUG_RETURN((int) error);
				}
			}

			trx->mysql_n_tables_locked++;
		}

		trx->n_mysql_tables_in_use++;
		prebuilt->mysql_has_locked = TRUE;

		DBUG_RETURN(0);
	}

	/* MySQL is releasing a table lock */

	trx->n_mysql_tables_in_use--;
	prebuilt->mysql_has_locked = FALSE;

	/* Release a possible FIFO ticket and search latch. Since we
	may reserve the kernel mutex, we have to release the search
	system latch first to obey the latching order. */

	trx_search_latch_release_if_reserved(trx);
	innodb_srv_conc_force_exit_innodb(trx);

	/* If the MySQL lock count drops to zero we know that the current SQL
	statement has ended */

	if (trx->n_mysql_tables_in_use == 0) {
#ifdef EXTENDED_SLOWLOG
		if (UNIV_UNLIKELY(trx->take_stats)) {
			increment_thd_innodb_stats(thd,
						   (unsigned long long) trx->id,
						   trx->io_reads,
						   trx->io_read,
						   trx->io_reads_wait_timer,
						   trx->lock_que_wait_timer,
						   trx->innodb_que_wait_timer,
						   trx->distinct_page_access);

			trx->io_reads = 0;
			trx->io_read = 0;
			trx->io_reads_wait_timer = 0;
			trx->lock_que_wait_timer = 0;
			trx->innodb_que_wait_timer = 0;
			trx->distinct_page_access = 0;
			if (trx->distinct_page_access_hash)
				memset(trx->distinct_page_access_hash, 0,
				       DPAH_SIZE);
		}
#endif

		trx->mysql_n_tables_locked = 0;
		prebuilt->used_in_HANDLER = FALSE;

		if (!thd_test_options(
				thd, OPTION_NOT_AUTOCOMMIT | OPTION_BEGIN)) {

			if (trx_is_started(trx)) {
				innobase_commit(ht, thd, TRUE);
			}

		} else if (trx->isolation_level <= TRX_ISO_READ_COMMITTED
			   && trx->global_read_view) {

			/* At low transaction isolation levels we let
			each consistent read set its own snapshot */

			read_view_close_for_mysql(trx);
		}
	}

	DBUG_RETURN(0);
}

/******************************************************************//**
With this function MySQL request a transactional lock to a table when
user issued query LOCK TABLES..WHERE ENGINE = InnoDB.
@return	error code */
UNIV_INTERN
int
ha_innobase::transactional_table_lock(
/*==================================*/
	THD*	thd,		/*!< in: handle to the user thread */
	int	lock_type)	/*!< in: lock type */
{
	trx_t*		trx;

	DBUG_ENTER("ha_innobase::transactional_table_lock");
	DBUG_PRINT("enter",("lock_type: %d", lock_type));

	/* We do not know if MySQL can call this function before calling
	external_lock(). To be safe, update the thd of the current table
	handle. */

	update_thd(thd);

	if (!share->ib_table || share->ib_table->is_corrupt) {
		DBUG_RETURN(HA_ERR_CRASHED);
	}

	if (prebuilt->table->ibd_file_missing && !thd_tablespace_op(thd)) {
		ut_print_timestamp(stderr);
		fprintf(stderr,
			"  InnoDB: MySQL is trying to use a table handle"
			" but the .ibd file for\n"
			"InnoDB: table %s does not exist.\n"
			"InnoDB: Have you deleted the .ibd file"
			" from the database directory under\n"
			"InnoDB: the MySQL datadir?"
			"InnoDB: See " REFMAN
			"innodb-troubleshooting.html\n"
			"InnoDB: how you can resolve the problem.\n",
			prebuilt->table->name);
		DBUG_RETURN(HA_ERR_CRASHED);
	}

	trx = prebuilt->trx;

	prebuilt->sql_stat_start = TRUE;
	prebuilt->hint_need_to_fetch_extra_cols = 0;

	reset_template();

	if (lock_type == F_WRLCK) {
		prebuilt->select_lock_type = LOCK_X;
		prebuilt->stored_select_lock_type = LOCK_X;
	} else if (lock_type == F_RDLCK) {
		prebuilt->select_lock_type = LOCK_S;
		prebuilt->stored_select_lock_type = LOCK_S;
	} else {
		ut_print_timestamp(stderr);
		fprintf(stderr, "  InnoDB error:\n"
"MySQL is trying to set transactional table lock with corrupted lock type\n"
"to table %s, lock type %d does not exist.\n",
				prebuilt->table->name, lock_type);
		DBUG_RETURN(HA_ERR_CRASHED);
	}

	/* MySQL is setting a new transactional table lock */

	innobase_register_trx(ht, thd, trx);

	if (THDVAR(thd, table_locks) && thd_in_lock_tables(thd)) {
		ulint	error = DB_SUCCESS;

		error = row_lock_table_for_mysql(prebuilt, NULL, 0);

		if (error != DB_SUCCESS) {
			error = convert_error_code_to_mysql(
				(int) error, prebuilt->table->flags, thd);
			DBUG_RETURN((int) error);
		}

		if (thd_test_options(
			thd, OPTION_NOT_AUTOCOMMIT | OPTION_BEGIN)) {

			/* Store the current undo_no of the transaction
			so that we know where to roll back if we have
			to roll back the next SQL statement */

			trx_mark_sql_stat_end(trx);
		}
	}

	DBUG_RETURN(0);
}

/************************************************************************//**
Here we export InnoDB status variables to MySQL. */
static
void
innodb_export_status(void)
/*======================*/
{
	if (innodb_inited) {
		srv_export_innodb_status();
	}
}

/************************************************************************//**
Implements the SHOW INNODB STATUS command. Sends the output of the InnoDB
Monitor to the client. */
static
bool
innodb_show_status(
/*===============*/
	handlerton*	hton,	/*!< in: the innodb handlerton */
	THD*	thd,	/*!< in: the MySQL query thread of the caller */
	stat_print_fn *stat_print)
{
	trx_t*			trx;
	static const char	truncated_msg[] = "... truncated...\n";
	const long		MAX_STATUS_SIZE = 1048576;
	ulint			trx_list_start = ULINT_UNDEFINED;
	ulint			trx_list_end = ULINT_UNDEFINED;
	bool			ret_val;

	DBUG_ENTER("innodb_show_status");
	DBUG_ASSERT(hton == innodb_hton_ptr);

	trx = check_trx_exists(thd);

	trx_search_latch_release_if_reserved(trx);
	innodb_srv_conc_force_exit_innodb(trx);

	/* We let the InnoDB Monitor to output at most MAX_STATUS_SIZE
	bytes of text. */

	long	flen, usable_len;
	char*	str;

	mutex_enter(&srv_monitor_file_mutex);
	rewind(srv_monitor_file);
	srv_printf_innodb_monitor(srv_monitor_file, FALSE,
				  &trx_list_start, &trx_list_end);
	flen = ftell(srv_monitor_file);
	os_file_set_eof(srv_monitor_file);

	if (flen < 0) {
		flen = 0;
	}

	if (flen > MAX_STATUS_SIZE) {
		usable_len = MAX_STATUS_SIZE;
		srv_truncated_status_writes++;
	} else {
		usable_len = flen;
	}

	/* allocate buffer for the string, and
	read the contents of the temporary file */

	if (!(str = (char*) my_malloc(usable_len + 1, MYF(0)))) {
		mutex_exit(&srv_monitor_file_mutex);
		DBUG_RETURN(TRUE);
	}

	rewind(srv_monitor_file);
	if (flen < MAX_STATUS_SIZE) {
		/* Display the entire output. */
		flen = (long) fread(str, 1, flen, srv_monitor_file);
	} else if (trx_list_end < (ulint) flen
			&& trx_list_start < trx_list_end
			&& trx_list_start + (flen - trx_list_end)
			< MAX_STATUS_SIZE - sizeof truncated_msg - 1) {
		/* Omit the beginning of the list of active transactions. */
		long len = (long) fread(str, 1, trx_list_start, srv_monitor_file);
		memcpy(str + len, truncated_msg, sizeof truncated_msg - 1);
		len += sizeof truncated_msg - 1;
		usable_len = (MAX_STATUS_SIZE - 1) - len;
		fseek(srv_monitor_file, flen - usable_len, SEEK_SET);
		len += (long) fread(str + len, 1, usable_len, srv_monitor_file);
		flen = len;
	} else {
		/* Omit the end of the output. */
		flen = (long) fread(str, 1, MAX_STATUS_SIZE - 1, srv_monitor_file);
	}

	mutex_exit(&srv_monitor_file_mutex);

	ret_val= stat_print(thd, innobase_hton_name,
				(uint) strlen(innobase_hton_name),
				STRING_WITH_LEN(""), str, flen);

	my_free(str);

	DBUG_RETURN(ret_val);
}

/************************************************************************//**
Implements the SHOW MUTEX STATUS command.
@return TRUE on failure, FALSE on success. */
static
bool
innodb_mutex_show_status(
/*=====================*/
	handlerton*	hton,		/*!< in: the innodb handlerton */
	THD*		thd,		/*!< in: the MySQL query thread of the
					caller */
	stat_print_fn*	stat_print)	/*!< in: function for printing
					statistics */
{
	char buf1[IO_SIZE], buf2[IO_SIZE];
	mutex_t*	mutex;
	rw_lock_t*	lock;
	ulint		block_mutex_oswait_count = 0;
	ulint		block_lock_oswait_count = 0;
	mutex_t*	block_mutex = NULL;
	rw_lock_t*	block_lock = NULL;
#ifdef UNIV_DEBUG
	ulint	  rw_lock_count= 0;
	ulint	  rw_lock_count_spin_loop= 0;
	ulint	  rw_lock_count_spin_rounds= 0;
	ulint	  rw_lock_count_os_wait= 0;
	ulint	  rw_lock_count_os_yield= 0;
	ulonglong rw_lock_wait_time= 0;
#endif /* UNIV_DEBUG */
	uint	  hton_name_len= (uint) strlen(innobase_hton_name), buf1len, buf2len;
	DBUG_ENTER("innodb_mutex_show_status");
	DBUG_ASSERT(hton == innodb_hton_ptr);

	mutex_enter(&mutex_list_mutex);

	for (mutex = UT_LIST_GET_FIRST(mutex_list); mutex != NULL;
	     mutex = UT_LIST_GET_NEXT(list, mutex)) {
		if (mutex->count_os_wait == 0) {
			continue;
		}

		if (buf_pool_is_block_mutex(mutex)) {
			block_mutex = mutex;
			block_mutex_oswait_count += mutex->count_os_wait;
			continue;
		}
#ifdef UNIV_DEBUG
		if (mutex->mutex_type != 1) {
			if (mutex->count_using > 0) {
				buf1len= my_snprintf(buf1, sizeof(buf1),
					"%s:%s",
					mutex->cmutex_name,
					innobase_basename(mutex->cfile_name));
				buf2len= my_snprintf(buf2, sizeof(buf2),
					"count=%lu, spin_waits=%lu,"
					" spin_rounds=%lu, "
					"os_waits=%lu, os_yields=%lu,"
					" os_wait_times=%lu",
					mutex->count_using,
					mutex->count_spin_loop,
					mutex->count_spin_rounds,
					mutex->count_os_wait,
					mutex->count_os_yield,
					(ulong) (mutex->lspent_time/1000));

				if (stat_print(thd, innobase_hton_name,
						hton_name_len, buf1, buf1len,
						buf2, buf2len)) {
					mutex_exit(&mutex_list_mutex);
					DBUG_RETURN(1);
				}
			}
		} else {
			rw_lock_count += mutex->count_using;
			rw_lock_count_spin_loop += mutex->count_spin_loop;
			rw_lock_count_spin_rounds += mutex->count_spin_rounds;
			rw_lock_count_os_wait += mutex->count_os_wait;
			rw_lock_count_os_yield += mutex->count_os_yield;
			rw_lock_wait_time += mutex->lspent_time;
		}
#else /* UNIV_DEBUG */
		buf1len= (uint) my_snprintf(buf1, sizeof(buf1), "%s",
				     mutex->cmutex_name);
		buf2len= (uint) my_snprintf(buf2, sizeof(buf2), "os_waits=%lu",
				     (ulong) mutex->count_os_wait);

		if (stat_print(thd, innobase_hton_name,
			       hton_name_len, buf1, buf1len,
			       buf2, buf2len)) {
			mutex_exit(&mutex_list_mutex);
			DBUG_RETURN(1);
		}
#endif /* UNIV_DEBUG */
	}

	if (block_mutex) {
		buf1len = (uint) my_snprintf(buf1, sizeof buf1,
					     "combined %s",
					     block_mutex->cmutex_name);
		buf2len = (uint) my_snprintf(buf2, sizeof buf2,
					     "os_waits=%lu",
					     (ulong) block_mutex_oswait_count);

		if (stat_print(thd, innobase_hton_name,
			       hton_name_len, buf1, buf1len,
			       buf2, buf2len)) {
			mutex_exit(&mutex_list_mutex);
			DBUG_RETURN(1);
		}
	}

	mutex_exit(&mutex_list_mutex);

	mutex_enter(&rw_lock_list_mutex);

	for (lock = UT_LIST_GET_FIRST(rw_lock_list); lock != NULL;
	     lock = UT_LIST_GET_NEXT(list, lock)) {
		if (lock->count_os_wait == 0) {
			continue;
		}

		if (buf_pool_is_block_lock(lock)) {
			block_lock = lock;
			block_lock_oswait_count += lock->count_os_wait;
			continue;
		}

		buf1len = my_snprintf(buf1, sizeof buf1, "%s",
				     lock->lock_name);
		buf2len = my_snprintf(buf2, sizeof buf2, "os_waits=%lu",
				      (ulong) lock->count_os_wait);

		if (stat_print(thd, innobase_hton_name,
			       hton_name_len, buf1, buf1len,
			       buf2, buf2len)) {
			mutex_exit(&rw_lock_list_mutex);
			DBUG_RETURN(1);
		}
	}

	if (block_lock) {
		buf1len = (uint) my_snprintf(buf1, sizeof buf1,
					     "combined %s",
					     block_lock->lock_name);
		buf2len = (uint) my_snprintf(buf2, sizeof buf2,
					     "os_waits=%lu",
					     (ulong) block_lock_oswait_count);

		if (stat_print(thd, innobase_hton_name,
			       hton_name_len, buf1, buf1len,
			       buf2, buf2len)) {
			mutex_exit(&rw_lock_list_mutex);
			DBUG_RETURN(1);
		}
	}

	mutex_exit(&rw_lock_list_mutex);

#ifdef UNIV_DEBUG
	buf2len = my_snprintf(buf2, sizeof buf2,
			     "count=%lu, spin_waits=%lu, spin_rounds=%lu, "
			     "os_waits=%lu, os_yields=%lu, os_wait_times=%lu",
			      (ulong) rw_lock_count,
			      (ulong) rw_lock_count_spin_loop,
			      (ulong) rw_lock_count_spin_rounds,
			      (ulong) rw_lock_count_os_wait,
			      (ulong) rw_lock_count_os_yield,
			      (ulong) (rw_lock_wait_time / 1000));

	if (stat_print(thd, innobase_hton_name, hton_name_len,
			STRING_WITH_LEN("rw_lock_mutexes"), buf2, buf2len)) {
		DBUG_RETURN(1);
	}
#endif /* UNIV_DEBUG */

	DBUG_RETURN(FALSE);
}

static
bool innobase_show_status(handlerton *hton, THD* thd, 
                          stat_print_fn* stat_print,
                          enum ha_stat_type stat_type)
{
	DBUG_ASSERT(hton == innodb_hton_ptr);

	switch (stat_type) {
	case HA_ENGINE_STATUS:
		return innodb_show_status(hton, thd, stat_print);
	case HA_ENGINE_MUTEX:
		return innodb_mutex_show_status(hton, thd, stat_print);
	default:
		return(FALSE);
	}
}

/************************************************************************//**
 Handling the shared INNOBASE_SHARE structure that is needed to provide table
 locking.
****************************************************************************/

static INNOBASE_SHARE* get_share(const char* table_name)
{
	INNOBASE_SHARE *share;
	mysql_mutex_lock(&innobase_share_mutex);

	ulint	fold = ut_fold_string(table_name);

	HASH_SEARCH(table_name_hash, innobase_open_tables, fold,
		    INNOBASE_SHARE*, share,
		    ut_ad(share->use_count > 0),
		    !strcmp(share->table_name, table_name));

	if (!share) {

		uint length = (uint) strlen(table_name);

		/* TODO: invoke HASH_MIGRATE if innobase_open_tables
		grows too big */

		share = (INNOBASE_SHARE *) my_malloc(sizeof(*share)+length+1,
			MYF(MY_FAE | MY_ZEROFILL));

		share->table_name = (char*) memcpy(share + 1,
						   table_name, length + 1);

		HASH_INSERT(INNOBASE_SHARE, table_name_hash,
			    innobase_open_tables, fold, share);

		thr_lock_init(&share->lock);

		/* Index translation table initialization */
		share->idx_trans_tbl.index_mapping = NULL;
		share->idx_trans_tbl.index_count = 0;
		share->idx_trans_tbl.array_size = 0;
	}

	share->use_count++;
	mysql_mutex_unlock(&innobase_share_mutex);

	return(share);
}

static void free_share(INNOBASE_SHARE* share)
{
	mysql_mutex_lock(&innobase_share_mutex);

#ifdef UNIV_DEBUG
	INNOBASE_SHARE* share2;
	ulint	fold = ut_fold_string(share->table_name);

	HASH_SEARCH(table_name_hash, innobase_open_tables, fold,
		    INNOBASE_SHARE*, share2,
		    ut_ad(share->use_count > 0),
		    !strcmp(share->table_name, share2->table_name));

	ut_a(share2 == share);
#endif /* UNIV_DEBUG */

	if (!--share->use_count) {
		ulint	fold = ut_fold_string(share->table_name);

		HASH_DELETE(INNOBASE_SHARE, table_name_hash,
			    innobase_open_tables, fold, share);
		thr_lock_delete(&share->lock);

		/* Free any memory from index translation table */
		my_free(share->idx_trans_tbl.index_mapping);

		my_free(share);

		/* TODO: invoke HASH_MIGRATE if innobase_open_tables
		shrinks too much */
	}

	mysql_mutex_unlock(&innobase_share_mutex);
}

/*****************************************************************//**
Converts a MySQL table lock stored in the 'lock' field of the handle to
a proper type before storing pointer to the lock into an array of pointers.
MySQL also calls this if it wants to reset some table locks to a not-locked
state during the processing of an SQL query. An example is that during a
SELECT the read lock is released early on the 'const' tables where we only
fetch one row. MySQL does not call this when it releases all locks at the
end of an SQL statement.
@return	pointer to the next element in the 'to' array */
UNIV_INTERN
THR_LOCK_DATA**
ha_innobase::store_lock(
/*====================*/
	THD*			thd,		/*!< in: user thread handle */
	THR_LOCK_DATA**		to,		/*!< in: pointer to an array
						of pointers to lock structs;
						pointer to the 'lock' field
						of current handle is stored
						next to this array */
	enum thr_lock_type	lock_type)	/*!< in: lock type to store in
						'lock'; this may also be
						TL_IGNORE */
{
	trx_t*		trx;

	/* Note that trx in this function is NOT necessarily prebuilt->trx
	because we call update_thd() later, in ::external_lock()! Failure to
	understand this caused a serious memory corruption bug in 5.1.11. */

	trx = check_trx_exists(thd);

	/* NOTE: MySQL can call this function with lock 'type' TL_IGNORE!
	Be careful to ignore TL_IGNORE if we are going to do something with
	only 'real' locks! */

	/* If no MySQL table is in use, we need to set the isolation level
	of the transaction. */

	if (lock_type != TL_IGNORE
	    && trx->n_mysql_tables_in_use == 0) {
		trx->isolation_level = innobase_map_isolation_level(
			(enum_tx_isolation) thd_tx_isolation(thd));

		if (trx->isolation_level <= TRX_ISO_READ_COMMITTED
		    && trx->global_read_view) {

			/* At low transaction isolation levels we let
			each consistent read set its own snapshot */

			read_view_close_for_mysql(trx);
		}
	}

	DBUG_ASSERT(EQ_CURRENT_THD(thd));
	const bool in_lock_tables = thd_in_lock_tables(thd);
	const uint sql_command = thd_sql_command(thd);

	if (sql_command == SQLCOM_DROP_TABLE) {

		/* MySQL calls this function in DROP TABLE though this table
		handle may belong to another thd that is running a query. Let
		us in that case skip any changes to the prebuilt struct. */ 

	} else if ((lock_type == TL_READ && in_lock_tables)
		   || (lock_type == TL_READ_HIGH_PRIORITY && in_lock_tables)
		   || lock_type == TL_READ_WITH_SHARED_LOCKS
		   || lock_type == TL_READ_NO_INSERT
		   || (lock_type != TL_IGNORE
		       && sql_command != SQLCOM_SELECT)) {

		/* The OR cases above are in this order:
		1) MySQL is doing LOCK TABLES ... READ LOCAL, or we
		are processing a stored procedure or function, or
		2) (we do not know when TL_READ_HIGH_PRIORITY is used), or
		3) this is a SELECT ... IN SHARE MODE, or
		4) we are doing a complex SQL statement like
		INSERT INTO ... SELECT ... and the logical logging (MySQL
		binlog) requires the use of a locking read, or
		MySQL is doing LOCK TABLES ... READ.
		5) we let InnoDB do locking reads for all SQL statements that
		are not simple SELECTs; note that select_lock_type in this
		case may get strengthened in ::external_lock() to LOCK_X.
		Note that we MUST use a locking read in all data modifying
		SQL statements, because otherwise the execution would not be
		serializable, and also the results from the update could be
		unexpected if an obsolete consistent read view would be
		used. */

		ulint	isolation_level;

		isolation_level = trx->isolation_level;

		if ((srv_locks_unsafe_for_binlog
		     || isolation_level <= TRX_ISO_READ_COMMITTED)
		    && isolation_level != TRX_ISO_SERIALIZABLE
		    && (lock_type == TL_READ || lock_type == TL_READ_NO_INSERT)
		    && (sql_command == SQLCOM_INSERT_SELECT
			|| sql_command == SQLCOM_REPLACE_SELECT
			|| sql_command == SQLCOM_UPDATE
			|| sql_command == SQLCOM_CREATE_TABLE)) {

			/* If we either have innobase_locks_unsafe_for_binlog
			option set or this session is using READ COMMITTED
			isolation level and isolation level of the transaction
			is not set to serializable and MySQL is doing
			INSERT INTO...SELECT or REPLACE INTO...SELECT
			or UPDATE ... = (SELECT ...) or CREATE  ...
			SELECT... without FOR UPDATE or IN SHARE
			MODE in select, then we use consistent read
			for select. */

			prebuilt->select_lock_type = LOCK_NONE;
			prebuilt->stored_select_lock_type = LOCK_NONE;
		} else if (sql_command == SQLCOM_CHECKSUM) {
			/* Use consistent read for checksum table */

			prebuilt->select_lock_type = LOCK_NONE;
			prebuilt->stored_select_lock_type = LOCK_NONE;
		} else {
			prebuilt->select_lock_type = LOCK_S;
			prebuilt->stored_select_lock_type = LOCK_S;
		}

	} else if (lock_type != TL_IGNORE) {

		/* We set possible LOCK_X value in external_lock, not yet
		here even if this would be SELECT ... FOR UPDATE */

		prebuilt->select_lock_type = LOCK_NONE;
		prebuilt->stored_select_lock_type = LOCK_NONE;
	}

	if (lock_type != TL_IGNORE && lock.type == TL_UNLOCK) {

		/* Starting from 5.0.7, we weaken also the table locks
		set at the start of a MySQL stored procedure call, just like
		we weaken the locks set at the start of an SQL statement.
		MySQL does set in_lock_tables TRUE there, but in reality
		we do not need table locks to make the execution of a
		single transaction stored procedure call deterministic
		(if it does not use a consistent read). */

		if (lock_type == TL_READ
		    && sql_command == SQLCOM_LOCK_TABLES) {
			/* We come here if MySQL is processing LOCK TABLES
			... READ LOCAL. MyISAM under that table lock type
			reads the table as it was at the time the lock was
			granted (new inserts are allowed, but not seen by the
			reader). To get a similar effect on an InnoDB table,
			we must use LOCK TABLES ... READ. We convert the lock
			type here, so that for InnoDB, READ LOCAL is
			equivalent to READ. This will change the InnoDB
			behavior in mysqldump, so that dumps of InnoDB tables
			are consistent with dumps of MyISAM tables. */

			lock_type = TL_READ_NO_INSERT;
		}

		/* If we are not doing a LOCK TABLE, DISCARD/IMPORT
		TABLESPACE or TRUNCATE TABLE then allow multiple
		writers. Note that ALTER TABLE uses a TL_WRITE_ALLOW_READ
		< TL_WRITE_CONCURRENT_INSERT.

		We especially allow multiple writers if MySQL is at the
		start of a stored procedure call (SQLCOM_CALL) or a
		stored function call (MySQL does have in_lock_tables
		TRUE there). */

		if ((lock_type >= TL_WRITE_CONCURRENT_INSERT
		     && lock_type <= TL_WRITE)
		    && !(in_lock_tables
			 && sql_command == SQLCOM_LOCK_TABLES)
		    && !thd_tablespace_op(thd)
		    && sql_command != SQLCOM_TRUNCATE
		    && sql_command != SQLCOM_OPTIMIZE
		    && sql_command != SQLCOM_CREATE_TABLE) {

			lock_type = TL_WRITE_ALLOW_WRITE;
		}

		/* In queries of type INSERT INTO t1 SELECT ... FROM t2 ...
		MySQL would use the lock TL_READ_NO_INSERT on t2, and that
		would conflict with TL_WRITE_ALLOW_WRITE, blocking all inserts
		to t2. Convert the lock to a normal read lock to allow
		concurrent inserts to t2.

		We especially allow concurrent inserts if MySQL is at the
		start of a stored procedure call (SQLCOM_CALL)
		(MySQL does have thd_in_lock_tables() TRUE there). */

		if (lock_type == TL_READ_NO_INSERT
		    && sql_command != SQLCOM_LOCK_TABLES) {

			lock_type = TL_READ;
		}

		lock.type = lock_type;
	}

	*to++= &lock;

	return(to);
}

/*********************************************************************//**
Read the next autoinc value. Acquire the relevant locks before reading
the AUTOINC value. If SUCCESS then the table AUTOINC mutex will be locked
on return and all relevant locks acquired.
@return	DB_SUCCESS or error code */
UNIV_INTERN
ulint
ha_innobase::innobase_get_autoinc(
/*==============================*/
	ulonglong*	value)		/*!< out: autoinc value */
{
 	*value = 0;
 
	prebuilt->autoinc_error = innobase_lock_autoinc();

	if (prebuilt->autoinc_error == DB_SUCCESS) {

		/* Determine the first value of the interval */
		*value = dict_table_autoinc_read(prebuilt->table);

		/* It should have been initialized during open. */
		if (*value == 0) {
			prebuilt->autoinc_error = DB_UNSUPPORTED;
			dict_table_autoinc_unlock(prebuilt->table);
		}
	}

	return(prebuilt->autoinc_error);
}

/*******************************************************************//**
This function reads the global auto-inc counter. It doesn't use the
AUTOINC lock even if the lock mode is set to TRADITIONAL.
@return	the autoinc value */
UNIV_INTERN
ulonglong
ha_innobase::innobase_peek_autoinc(void)
/*====================================*/
{
	ulonglong	auto_inc;
	dict_table_t*	innodb_table;

	ut_a(prebuilt != NULL);
	ut_a(prebuilt->table != NULL);

	innodb_table = prebuilt->table;

	dict_table_autoinc_lock(innodb_table);

	auto_inc = dict_table_autoinc_read(innodb_table);

	if (auto_inc == 0) {
		ut_print_timestamp(stderr);
		fprintf(stderr, "  InnoDB: AUTOINC next value generation "
			"is disabled for '%s'\n", innodb_table->name);
	}

	dict_table_autoinc_unlock(innodb_table);

	return(auto_inc);
}

/*********************************************************************//**
This function initializes the auto-inc counter if it has not been
initialized yet. This function does not change the value of the auto-inc
counter if it already has been initialized. Returns the value of the
auto-inc counter in *first_value, and ULONGLONG_MAX in *nb_reserved_values (as
we have a table-level lock). offset, increment, nb_desired_values are ignored.
*first_value is set to -1 if error (deadlock or lock wait timeout) */
UNIV_INTERN
void
ha_innobase::get_auto_increment(
/*============================*/
        ulonglong	offset,              /*!< in: table autoinc offset */
        ulonglong	increment,           /*!< in: table autoinc increment */
        ulonglong	nb_desired_values,   /*!< in: number of values reqd */
        ulonglong	*first_value,        /*!< out: the autoinc value */
        ulonglong	*nb_reserved_values) /*!< out: count of reserved values */
{
	trx_t*		trx;
	ulint		error;
	ulonglong	autoinc = 0;

	/* Prepare prebuilt->trx in the table handle */
	update_thd(ha_thd());

	error = innobase_get_autoinc(&autoinc);

	if (error != DB_SUCCESS) {
		*first_value = (~(ulonglong) 0);
		return;
	}

	/* This is a hack, since nb_desired_values seems to be accurate only
	for the first call to get_auto_increment() for multi-row INSERT and
	meaningless for other statements e.g, LOAD etc. Subsequent calls to
	this method for the same statement results in different values which
	don't make sense. Therefore we store the value the first time we are
	called and count down from that as rows are written (see write_row()).
	*/

	trx = prebuilt->trx;

	/* Note: We can't rely on *first_value since some MySQL engines,
	in particular the partition engine, don't initialize it to 0 when
	invoking this method. So we are not sure if it's guaranteed to
	be 0 or not. */

	/* We need the upper limit of the col type to check for
	whether we update the table autoinc counter or not. */
	ulonglong	col_max_value = innobase_get_int_col_max_value(
		table->next_number_field);

	/* Called for the first time ? */
	if (trx->n_autoinc_rows == 0) {

		trx->n_autoinc_rows = (ulint) nb_desired_values;

		/* It's possible for nb_desired_values to be 0:
		e.g., INSERT INTO T1(C) SELECT C FROM T2; */
		if (nb_desired_values == 0) {

			trx->n_autoinc_rows = 1;
		}

		set_if_bigger(*first_value, autoinc);
	/* Not in the middle of a mult-row INSERT. */
	} else if (prebuilt->autoinc_last_value == 0) {
		set_if_bigger(*first_value, autoinc);
	}

        if (*first_value > col_max_value)
        {
          	/* Out of range number. Let handler::update_auto_increment()
                   take care of this */
                prebuilt->autoinc_last_value = 0;
                dict_table_autoinc_unlock(prebuilt->table);
                *nb_reserved_values = 0;
                return;
        }
	*nb_reserved_values = trx->n_autoinc_rows;

	/* With old style AUTOINC locking we only update the table's
	AUTOINC counter after attempting to insert the row. */
	if (innobase_autoinc_lock_mode != AUTOINC_OLD_STYLE_LOCKING) {
		ulonglong	current;
		ulonglong	next_value;

		current = *first_value;

		if (prebuilt->autoinc_increment != increment) {

			current = autoinc - prebuilt->autoinc_increment;

			current = innobase_next_autoinc(
				current, 1, increment, offset, col_max_value);

			dict_table_autoinc_initialize(prebuilt->table, current);

			*first_value = current;
		}

		/* Compute the last value in the interval */
		next_value = innobase_next_autoinc(
			current, *nb_reserved_values, increment, offset,
			col_max_value);

		prebuilt->autoinc_last_value = next_value;

		if (prebuilt->autoinc_last_value < *first_value) {
			*first_value = (~(ulonglong) 0);
		} else {
			/* Update the table autoinc variable */
			dict_table_autoinc_update_if_greater(
				prebuilt->table, prebuilt->autoinc_last_value);
		}
	} else {
		/* This will force write_row() into attempting an update
		of the table's AUTOINC counter. */
		prebuilt->autoinc_last_value = 0;
	}

	/* The increment to be used to increase the AUTOINC value, we use
	this in write_row() and update_row() to increase the autoinc counter
	for columns that are filled by the user. We need the offset and
	the increment. */
	prebuilt->autoinc_offset = offset;
	prebuilt->autoinc_increment = increment;

	dict_table_autoinc_unlock(prebuilt->table);
}

/*******************************************************************//**
Reset the auto-increment counter to the given value, i.e. the next row
inserted will get the given value. This is called e.g. after TRUNCATE
is emulated by doing a 'DELETE FROM t'. HA_ERR_WRONG_COMMAND is
returned by storage engines that don't support this operation.
@return	0 or error code */
UNIV_INTERN
int
ha_innobase::reset_auto_increment(
/*==============================*/
	ulonglong	value)		/*!< in: new value for table autoinc */
{
	DBUG_ENTER("ha_innobase::reset_auto_increment");

	int	error;

	update_thd(ha_thd());

	error = row_lock_table_autoinc_for_mysql(prebuilt);

	if (error != DB_SUCCESS) {
		error = convert_error_code_to_mysql(error,
						    prebuilt->table->flags,
						    user_thd);

		DBUG_RETURN(error);
	}

	/* The next value can never be 0. */
	if (value == 0) {
		value = 1;
	}

	innobase_reset_autoinc(value);

	DBUG_RETURN(0);
}

/* See comment in handler.cc */
UNIV_INTERN
bool
ha_innobase::get_error_message(int error, String *buf)
{
	trx_t*	trx = check_trx_exists(ha_thd());

	buf->copy(trx->detailed_error, (uint) strlen(trx->detailed_error),
		system_charset_info);

	return(FALSE);
}

/*******************************************************************//**
Compares two 'refs'. A 'ref' is the (internal) primary key value of the row.
If there is no explicitly declared non-null unique key or a primary key, then
InnoDB internally uses the row id as the primary key.
@return	< 0 if ref1 < ref2, 0 if equal, else > 0 */
UNIV_INTERN
int
ha_innobase::cmp_ref(
/*=================*/
	const uchar*	ref1,	/*!< in: an (internal) primary key value in the
				MySQL key value format */
	const uchar*	ref2)	/*!< in: an (internal) primary key value in the
				MySQL key value format */
{
	enum_field_types mysql_type;
	Field*		field;
	KEY_PART_INFO*	key_part;
	KEY_PART_INFO*	key_part_end;
	uint		len1;
	uint		len2;
	int		result;

	if (prebuilt->clust_index_was_generated) {
		/* The 'ref' is an InnoDB row id */

		return(memcmp(ref1, ref2, DATA_ROW_ID_LEN));
	}

	/* Do a type-aware comparison of primary key fields. PK fields
	are always NOT NULL, so no checks for NULL are performed. */

	key_part = table->key_info[table->s->primary_key].key_part;

	key_part_end = key_part
			+ table->key_info[table->s->primary_key].key_parts;

	for (; key_part != key_part_end; ++key_part) {
		field = key_part->field;
		mysql_type = field->type();

		if (mysql_type == MYSQL_TYPE_TINY_BLOB
			|| mysql_type == MYSQL_TYPE_MEDIUM_BLOB
			|| mysql_type == MYSQL_TYPE_BLOB
			|| mysql_type == MYSQL_TYPE_LONG_BLOB) {

			/* In the MySQL key value format, a column prefix of
			a BLOB is preceded by a 2-byte length field */

			len1 = innobase_read_from_2_little_endian(ref1);
			len2 = innobase_read_from_2_little_endian(ref2);

			result = ((Field_blob*)field)->cmp(ref1 + 2, len1,
							   ref2 + 2, len2);
		} else {
			result = field->key_cmp(ref1, ref2);
		}

		if (result) {

			return(result);
		}

		ref1 += key_part->store_length;
		ref2 += key_part->store_length;
	}

	return(0);
}

/*******************************************************************//**
Ask InnoDB if a query to a table can be cached.
@return	TRUE if query caching of the table is permitted */
UNIV_INTERN
my_bool
ha_innobase::register_query_cache_table(
/*====================================*/
	THD*		thd,		/*!< in: user thread handle */
	char*		table_key,	/*!< in: concatenation of database name,
					the null character NUL,
					and the table name */
	uint		key_length,	/*!< in: length of the full name, i.e.
					len(dbname) + len(tablename) + 1 */
	qc_engine_callback*
			call_back,	/*!< out: pointer to function for
					checking if query caching
					is permitted */
	ulonglong	*engine_data)	/*!< in/out: data to call_back */
{
	*call_back = innobase_query_caching_of_table_permitted;
	*engine_data = 0;
	return(innobase_query_caching_of_table_permitted(thd, table_key,
							 key_length,
							 engine_data));
}

UNIV_INTERN
char*
ha_innobase::get_mysql_bin_log_name()
{
	return(trx_sys_mysql_bin_log_name);
}

UNIV_INTERN
ulonglong
ha_innobase::get_mysql_bin_log_pos()
{
	/* trx... is ib_int64_t, which is a typedef for a 64-bit integer
	(__int64 or longlong) so it's ok to cast it to ulonglong. */

	return(trx_sys_mysql_bin_log_pos);
}

/******************************************************************//**
This function is used to find the storage length in bytes of the first n
characters for prefix indexes using a multibyte character set. The function
finds charset information and returns length of prefix_len characters in the
index field in bytes.
@return	number of bytes occupied by the first n characters */
extern "C" UNIV_INTERN
ulint
innobase_get_at_most_n_mbchars(
/*===========================*/
	ulint charset_id,	/*!< in: character set id */
	ulint prefix_len,	/*!< in: prefix length in bytes of the index
				(this has to be divided by mbmaxlen to get the
				number of CHARACTERS n in the prefix) */
	ulint data_len,		/*!< in: length of the string in bytes */
	const char* str)	/*!< in: character string */
{
	ulint char_length;	/*!< character length in bytes */
	ulint n_chars;		/*!< number of characters in prefix */
	CHARSET_INFO* charset;	/*!< charset used in the field */

	charset = get_charset((uint) charset_id, MYF(MY_WME));

	ut_ad(charset);
	ut_ad(charset->mbmaxlen);

	/* Calculate how many characters at most the prefix index contains */

	n_chars = prefix_len / charset->mbmaxlen;

	/* If the charset is multi-byte, then we must find the length of the
	first at most n chars in the string. If the string contains less
	characters than n, then we return the length to the end of the last
	character. */

	if (charset->mbmaxlen > 1) {
		/* my_charpos() returns the byte length of the first n_chars
		characters, or a value bigger than the length of str, if
		there were not enough full characters in str.

		Why does the code below work:
		Suppose that we are looking for n UTF-8 characters.

		1) If the string is long enough, then the prefix contains at
		least n complete UTF-8 characters + maybe some extra
		characters + an incomplete UTF-8 character. No problem in
		this case. The function returns the pointer to the
		end of the nth character.

		2) If the string is not long enough, then the string contains
		the complete value of a column, that is, only complete UTF-8
		characters, and we can store in the column prefix index the
		whole string. */

		char_length = my_charpos(charset, str,
						str + data_len, (int) n_chars);
		if (char_length > data_len) {
			char_length = data_len;
		}
	} else {
		if (data_len < prefix_len) {
			char_length = data_len;
		} else {
			char_length = prefix_len;
		}
	}

	return(char_length);
}

/*******************************************************************//**
This function is used to prepare an X/Open XA distributed transaction.
@return	0 or error number */
static
int
innobase_xa_prepare(
/*================*/
        handlerton*	hton,	/*!< in: InnoDB handlerton */
	THD*		thd,	/*!< in: handle to the MySQL thread of
				the user whose XA transaction should
				be prepared */
	bool		all)	/*!< in: TRUE - commit transaction
				FALSE - the current SQL statement
				ended */
{
	int error = 0;
	trx_t* trx = check_trx_exists(thd);

	DBUG_ASSERT(hton == innodb_hton_ptr);

	/* we use support_xa value as it was seen at transaction start
	time, not the current session variable value. Any possible changes
	to the session variable take effect only in the next transaction */
	if (!trx->support_xa) {

		return(0);
	}

	if (UNIV_UNLIKELY(trx->fake_changes)) {

		if (all || (!thd_test_options(thd, OPTION_NOT_AUTOCOMMIT
					      | OPTION_BEGIN))) {

			thd->stmt_da->reset_diagnostics_area();
			return(HA_ERR_WRONG_COMMAND);
		}
		return(0);
	}

	thd_get_xid(thd, (MYSQL_XID*) &trx->xid);

	/* Release a possible FIFO ticket and search latch. Since we will
	reserve the kernel mutex, we have to release the search system latch
	first to obey the latching order. */

	trx_search_latch_release_if_reserved(trx);
	innodb_srv_conc_force_exit_innodb(trx);

	if (!trx_is_registered_for_2pc(trx) && trx_is_started(trx)) {

		sql_print_error("Transaction not registered for MySQL 2PC, "
				"but transaction is active");
	}

	if (all
	    || (!thd_test_options(thd, OPTION_NOT_AUTOCOMMIT | OPTION_BEGIN))) {

		/* We were instructed to prepare the whole transaction, or
		this is an SQL statement end and autocommit is on */

		ut_ad(trx_is_registered_for_2pc(trx));

		/* Update the replication position info in current trx.  This
		is different from the binlog position update that happens
		during XA COMMIT.  In contrast to that, the slave position is
		an actual part of the changes made by this transaction and thus
		must be updated in the XA PREPARE stage.  Since the trx sys
		header page changes are not undo-logged, again store this
		position in a different field in the XA COMMIT stage, so that
		it might be used in case of rollbacks. */

		/* Since currently there might be only one slave SQL thread, we
		don't need to take any precautions (e.g. prepare_commit_mutex)
		to ensure position ordering.  Revisit this in 5.6 which has
		both the multi-threaded replication to cause us problems and
		the group commit to solve them.  */

		innobase_copy_repl_coords_to_trx(thd, trx);

		error = (int) trx_prepare_for_mysql(trx);

		DBUG_EXECUTE_IF("crash_innodb_after_prepare",
				DBUG_SUICIDE(););
	} else {
		/* We just mark the SQL statement ended and do not do a
		transaction prepare */

		/* If we had reserved the auto-inc lock for some
		table in this SQL statement we release it now */

		row_unlock_table_autoinc_for_mysql(trx);

		/* Store the current undo_no of the transaction so that we
		know where to roll back if we have to roll back the next
		SQL statement */

		trx_mark_sql_stat_end(trx);
	}

	/* Tell the InnoDB server that there might be work for utility
	threads: */

	srv_active_wake_master_thread();

	return(error);
}

/*******************************************************************//**
This function is used to recover X/Open XA distributed transactions.
@return	number of prepared transactions stored in xid_list */
static
int
innobase_xa_recover(
/*================*/
	handlerton*	hton,	/*!< in: InnoDB handlerton */
	XID*		xid_list,/*!< in/out: prepared transactions */
	uint		len)	/*!< in: number of slots in xid_list */
{
	DBUG_ASSERT(hton == innodb_hton_ptr);

	if (len == 0 || xid_list == NULL) {

		return(0);
	}

	return(trx_recover_for_mysql(xid_list, len));
}

/*******************************************************************//**
This function is used to commit one X/Open XA distributed transaction
which is in the prepared state
@return	0 or error number */
static
int
innobase_commit_by_xid(
/*===================*/
	handlerton*	hton,
	XID*		xid)	/*!< in: X/Open XA transaction identification */
{
	trx_t*	trx;

	DBUG_ASSERT(hton == innodb_hton_ptr);

	trx = trx_get_trx_by_xid(xid);

	if (trx) {
		innobase_commit_low(trx);
		trx_free_for_background(trx);
		return(XA_OK);
	} else {
		return(XAER_NOTA);
	}
}

/*******************************************************************//**
This function is used to rollback one X/Open XA distributed transaction
which is in the prepared state
@return	0 or error number */
static
int
innobase_rollback_by_xid(
/*=====================*/
	handlerton*	hton,	/*!< in: InnoDB handlerton */
	XID*		xid)	/*!< in: X/Open XA transaction
				identification */
{
	trx_t*	trx;

	DBUG_ASSERT(hton == innodb_hton_ptr);

	trx = trx_get_trx_by_xid(xid);

	if (trx) {
		int	ret = innobase_rollback_trx(trx);
		trx_free_for_background(trx);

		if (innobase_overwrite_relay_log_info) {

			/* On rollback of a prepared transaction revert the
			current slave positions to the ones recorded by the
			last COMMITTed transaction.  This has an effect of
			undoing the position change caused by the transaction
			being rolled back.  Assumes single-threaded slave SQL
			thread.  If the server has non-master write traffic
			with XA rollbacks, this will cause additional spurious
			slave info log overwrites, which should be harmless. */

			trx_sys_print_committed_mysql_master_log_pos();
			innobase_do_overwrite_relay_log_info();
		}

		return(ret);
	} else {
		return(XAER_NOTA);
	}
}

/*******************************************************************//**
Create a consistent view for a cursor based on current transaction
which is created if the corresponding MySQL thread still lacks one.
This consistent view is then used inside of MySQL when accessing records
using a cursor.
@return	pointer to cursor view or NULL */
static
void*
innobase_create_cursor_view(
/*========================*/
        handlerton *hton, /*!< in: innobase hton */
	THD* thd)	  /*!< in: user thread handle */
{
	DBUG_ASSERT(hton == innodb_hton_ptr);

	return(read_cursor_view_create_for_mysql(check_trx_exists(thd)));
}

/*******************************************************************//**
Close the given consistent cursor view of a transaction and restore
global read view to a transaction read view. Transaction is created if the
corresponding MySQL thread still lacks one. */
static
void
innobase_close_cursor_view(
/*=======================*/
        handlerton *hton,
	THD*	thd,	/*!< in: user thread handle */
	void*	curview)/*!< in: Consistent read view to be closed */
{
	DBUG_ASSERT(hton == innodb_hton_ptr);

	read_cursor_view_close_for_mysql(check_trx_exists(thd),
					 (cursor_view_t*) curview);
}

/*******************************************************************//**
Set the given consistent cursor view to a transaction which is created
if the corresponding MySQL thread still lacks one. If the given
consistent cursor view is NULL global read view of a transaction is
restored to a transaction read view. */
static
void
innobase_set_cursor_view(
/*=====================*/
        handlerton *hton,
	THD*	thd,	/*!< in: user thread handle */
	void*	curview)/*!< in: Consistent cursor view to be set */
{
	DBUG_ASSERT(hton == innodb_hton_ptr);

	read_cursor_set_for_mysql(check_trx_exists(thd),
				  (cursor_view_t*) curview);
}

/*******************************************************************//**
If col_name is not NULL, check whether the named column is being
renamed in the table. If col_name is not provided, check
whether any one of columns in the table is being renamed.
@return true if the column is being renamed */
static
bool
check_column_being_renamed(
/*=======================*/
	const TABLE*	table,		/*!< in: MySQL table */
	const char*	col_name)	/*!< in: name of the column */
{
	uint		k;
	Field*		field;

	for (k = 0; k < table->s->fields; k++) {
		field = table->field[k];

		if (field->flags & FIELD_IS_RENAMED) {

			/* If col_name is not provided, return
			if the field is marked as being renamed. */
			if (!col_name) {
				return(true);
			}

			/* If col_name is provided, return only
			if names match */
			if (innobase_strcasecmp(field->field_name,
						col_name) == 0) {
				return(true);
			}
		}
	}

	return(false);
}

/*******************************************************************//**
Check whether any of the given columns is being renamed in the table.
@return true if any of col_names is being renamed in table */
static
bool
column_is_being_renamed(
/*====================*/
	TABLE*		table,		/*!< in: MySQL table */
	uint		n_cols,		/*!< in: number of columns */
	const char**	col_names)	/*!< in: names of the columns */
{
	uint		j;

	for (j = 0; j < n_cols; j++) {
		if (check_column_being_renamed(table, col_names[j])) {
			return(true);
		}
	}

	return(false);
}

/***********************************************************************
Check whether a column in table "table" is being renamed and if this column
is part of a foreign key, either part of another table, referencing this
table or part of this table, referencing another table. */
static
bool
foreign_key_column_is_being_renamed(
/*================================*/
					/* out: true if a column that
					participates in a foreign key definition
					is being renamed */
	row_prebuilt_t*	prebuilt,	/* in: InnoDB prebuilt struct */
	TABLE*		table)		/* in: MySQL table */
{
	dict_foreign_t*	foreign;

	/* check whether there are foreign keys at all */
	if (UT_LIST_GET_LEN(prebuilt->table->foreign_list) == 0
	    && UT_LIST_GET_LEN(prebuilt->table->referenced_list) == 0) {
		/* no foreign keys involved with prebuilt->table */

		return(false);
	}

	row_mysql_lock_data_dictionary(prebuilt->trx);

	/* Check whether any column in the foreign key constraints which refer
	to this table is being renamed. */
	for (foreign = UT_LIST_GET_FIRST(prebuilt->table->referenced_list);
	     foreign != NULL;
	     foreign = UT_LIST_GET_NEXT(referenced_list, foreign)) {

		if (column_is_being_renamed(table, foreign->n_fields,
					    foreign->referenced_col_names)) {

			row_mysql_unlock_data_dictionary(prebuilt->trx);
			return(true);
		}
	}

	/* Check whether any column in the foreign key constraints in the
	table is being renamed. */
	for (foreign = UT_LIST_GET_FIRST(prebuilt->table->foreign_list);
	     foreign != NULL;
	     foreign = UT_LIST_GET_NEXT(foreign_list, foreign)) {

		if (column_is_being_renamed(table, foreign->n_fields,
					    foreign->foreign_col_names)) {

			row_mysql_unlock_data_dictionary(prebuilt->trx);
			return(true);
		}
	}

	row_mysql_unlock_data_dictionary(prebuilt->trx);

	return(false);
}

UNIV_INTERN
bool
ha_innobase::check_if_incompatible_data(
	HA_CREATE_INFO*	info,
	uint		table_changes)
{
	enum row_type row_type, info_row_type;
	DBUG_ENTER("ha_innobase::check_if_incompatible_data");

	if (table_changes != IS_EQUAL_YES) {

		DBUG_PRINT("info", ("table_changes != IS_EQUAL_YES "
				    "-> COMPATIBLE_DATA_NO"));
		DBUG_RETURN(COMPATIBLE_DATA_NO);
	}

	/* Check that auto_increment value was not changed */
	if ((info->used_fields & HA_CREATE_USED_AUTO) &&
		info->auto_increment_value != 0) {

		DBUG_PRINT("info", ("auto_increment_value changed -> "
				    "COMPATIBLE_DATA_NO"));
		DBUG_RETURN(COMPATIBLE_DATA_NO);
	}

	/* For column rename operation, MySQL does not supply enough
	information (new column name etc.) for InnoDB to make appropriate
	system metadata change. To avoid system metadata inconsistency,
	currently we can just request a table rebuild/copy by returning
	COMPATIBLE_DATA_NO */
	if (check_column_being_renamed(table, NULL)) {
        	DBUG_RETURN(COMPATIBLE_DATA_NO);
	}

	/* Check if a column participating in a foreign key is being renamed.
	There is no mechanism for updating InnoDB foreign key definitions. */
	if (foreign_key_column_is_being_renamed(prebuilt, table)) {

		DBUG_RETURN(COMPATIBLE_DATA_NO);
	}

	/* Check that row format didn't change */
	row_type = get_row_type();
	info_row_type = info->row_type;
	/* Default is compact. */
	if (info_row_type == ROW_TYPE_DEFAULT)
		info_row_type = ROW_TYPE_COMPACT;
	if ((info->used_fields & HA_CREATE_USED_ROW_FORMAT) &&
	    row_type != ((info->row_type == ROW_TYPE_DEFAULT)
				? ROW_TYPE_COMPACT : info->row_type)) {

		DBUG_PRINT("info", ("get_row_type()=%d != info->row_type=%d -> "
				    "COMPATIBLE_DATA_NO",
				    row_type, info->row_type));
		DBUG_RETURN(COMPATIBLE_DATA_NO);
	}

	/* Specifying KEY_BLOCK_SIZE requests a rebuild of the table. */
	if (info->used_fields & HA_CREATE_USED_KEY_BLOCK_SIZE) {
		DBUG_PRINT("info", ("HA_CREATE_USED_KEY_BLOCK_SIZE -> "
				    "COMPATIBLE_DATA_NO"));
		DBUG_RETURN(COMPATIBLE_DATA_NO);
	}

	DBUG_PRINT("info", (" -> COMPATIBLE_DATA_YES"));
	DBUG_RETURN(COMPATIBLE_DATA_YES);
}

/************************************************************//**
Validate the file format name and return its corresponding id.
@return	valid file format id */
static
uint
innobase_file_format_name_lookup(
/*=============================*/
	const char*	format_name)	/*!< in: pointer to file format name */
{
	char*	endp;
	uint	format_id;

	ut_a(format_name != NULL);

	/* The format name can contain the format id itself instead of
	the name and we check for that. */
	format_id = (uint) strtoul(format_name, &endp, 10);

	/* Check for valid parse. */
	if (*endp == '\0' && *format_name != '\0') {

		if (format_id <= DICT_TF_FORMAT_MAX) {

			return(format_id);
		}
	} else {

		for (format_id = 0; format_id <= DICT_TF_FORMAT_MAX;
		     format_id++) {
			const char*	name;

			name = trx_sys_file_format_id_to_name(format_id);

			if (!innobase_strcasecmp(format_name, name)) {

				return(format_id);
			}
		}
	}

	return(DICT_TF_FORMAT_MAX + 1);
}

/************************************************************//**
Validate the file format check config parameters, as a side effect it
sets the srv_max_file_format_at_startup variable.
@return the format_id if valid config value, otherwise, return -1 */
static
int
innobase_file_format_validate_and_set(
/*==================================*/
	const char*	format_max)	/*!< in: parameter value */
{
	uint		format_id;

	format_id = innobase_file_format_name_lookup(format_max);

	if (format_id < DICT_TF_FORMAT_MAX + 1) {
		srv_max_file_format_at_startup = format_id;

		return((int) format_id);
	} else {
		return(-1);
	}
}

/*************************************************************//**
Check if it is a valid file format. This function is registered as
a callback with MySQL.
@return	0 for valid file format */
static
int
innodb_file_format_name_validate(
/*=============================*/
	THD*				thd,	/*!< in: thread handle */
	struct st_mysql_sys_var*	var,	/*!< in: pointer to system
						variable */
	void*				save,	/*!< out: immediate result
						for update function */
	struct st_mysql_value*		value)	/*!< in: incoming string */
{
	const char*	file_format_input;
	char		buff[STRING_BUFFER_USUAL_SIZE];
	int		len = sizeof(buff);

	ut_a(save != NULL);
	ut_a(value != NULL);

	file_format_input = value->val_str(value, buff, &len);

	if (file_format_input != NULL) {
		uint	format_id;

		format_id = innobase_file_format_name_lookup(
			file_format_input);

		if (format_id <= DICT_TF_FORMAT_MAX) {

			/* Save a pointer to the name in the
			'file_format_name_map' constant array. */
			*static_cast<const char**>(save) =
			    trx_sys_file_format_id_to_name(format_id);

			return(0);
		}
	}

	*static_cast<const char**>(save) = NULL;
	return(1);
}

/****************************************************************//**
Update the system variable innodb_file_format using the "saved"
value. This function is registered as a callback with MySQL. */
static
void
innodb_file_format_name_update(
/*===========================*/
	THD*				thd,		/*!< in: thread handle */
	struct st_mysql_sys_var*	var,		/*!< in: pointer to
							system variable */
	void*				var_ptr,	/*!< out: where the
							formal string goes */
	const void*			save)		/*!< in: immediate result
							from check function */
{
	const char* format_name;

	ut_a(var_ptr != NULL);
	ut_a(save != NULL);

	format_name = *static_cast<const char*const*>(save);

	if (format_name) {
		uint	format_id;

		format_id = innobase_file_format_name_lookup(format_name);

		if (format_id <= DICT_TF_FORMAT_MAX) {
			srv_file_format = format_id;
		}
	}

	*static_cast<const char**>(var_ptr)
		= trx_sys_file_format_id_to_name(srv_file_format);
}
/*************************************************************//**
Check if valid argument to innodb_file_format_max. This function
is registered as a callback with MySQL.
@return	0 for valid file format */
static
int
innodb_file_format_max_validate(
/*============================*/
	THD*				thd,	/*!< in: thread handle */
	struct st_mysql_sys_var*	var,	/*!< in: pointer to system
						variable */
	void*				save,	/*!< out: immediate result
						for update function */
	struct st_mysql_value*		value)	/*!< in: incoming string */
{
	const char*	file_format_input;
	char		buff[STRING_BUFFER_USUAL_SIZE];
	int		len = sizeof(buff);
	int		format_id;

	ut_a(save != NULL);
	ut_a(value != NULL);

	file_format_input = value->val_str(value, buff, &len);

	if (file_format_input != NULL) {

		format_id = innobase_file_format_validate_and_set(
			file_format_input);

		if (format_id >= 0) {
			/* Save a pointer to the name in the
			'file_format_name_map' constant array. */
			*static_cast<const char**>(save) =
			    trx_sys_file_format_id_to_name(
						(uint)format_id);

			return(0);

		} else {
			push_warning_printf(thd,
			  MYSQL_ERROR::WARN_LEVEL_WARN,
			  ER_WRONG_ARGUMENTS,
			  "InnoDB: invalid innodb_file_format_max "
			  "value; can be any format up to %s "
			  "or equivalent id of %d",
			  trx_sys_file_format_id_to_name(DICT_TF_FORMAT_MAX),
			  DICT_TF_FORMAT_MAX);
		}
	}

	*static_cast<const char**>(save) = NULL;
	return(1);
}

/****************************************************************//**
Update the system variable innodb_file_format_max using the "saved"
value. This function is registered as a callback with MySQL. */
static
void
innodb_file_format_max_update(
/*==========================*/
	THD*				thd,		/*!< in: thread handle */
	struct st_mysql_sys_var*	var,		/*!< in: pointer to
							system variable */
	void*				var_ptr,	/*!< out: where the
							formal string goes */
	const void*			save)		/*!< in: immediate result
							from check function */
{
	const char*	format_name_in;
	const char**	format_name_out;
	uint		format_id;

	ut_a(save != NULL);
	ut_a(var_ptr != NULL);

	format_name_in = *static_cast<const char*const*>(save);

	if (!format_name_in) {

		return;
	}

	format_id = innobase_file_format_name_lookup(format_name_in);

	if (format_id > DICT_TF_FORMAT_MAX) {
		/* DEFAULT is "on", which is invalid at runtime. */
		push_warning_printf(thd, MYSQL_ERROR::WARN_LEVEL_WARN,
				    ER_WRONG_ARGUMENTS,
				    "Ignoring SET innodb_file_format=%s",
				    format_name_in);
		return;
	}

	format_name_out = static_cast<const char**>(var_ptr);

	/* Update the max format id in the system tablespace. */
	if (trx_sys_file_format_max_set(format_id, format_name_out)) {
		ut_print_timestamp(stderr);
		fprintf(stderr,
			" [Info] InnoDB: the file format in the system "
			"tablespace is now set to %s.\n", *format_name_out);
	}
}

/****************************************************************//**
Update the system variable innodb_adaptive_hash_index using the "saved"
value. This function is registered as a callback with MySQL. */
static
void
innodb_adaptive_hash_index_update(
/*==============================*/
	THD*				thd,		/*!< in: thread handle */
	struct st_mysql_sys_var*	var,		/*!< in: pointer to
							system variable */
	void*				var_ptr,	/*!< out: where the
							formal string goes */
	const void*			save)		/*!< in: immediate result
							from check function */
{
	if (*(my_bool*) save) {
		btr_search_enable();
	} else {
		btr_search_disable();
	}
}

/****************************************************************//**
Update the system variable innodb_old_blocks_pct using the "saved"
value. This function is registered as a callback with MySQL. */
static
void
innodb_old_blocks_pct_update(
/*=========================*/
	THD*				thd,	/*!< in: thread handle */
	struct st_mysql_sys_var*	var,	/*!< in: pointer to
						system variable */
	void*				var_ptr,/*!< out: where the
						formal string goes */
	const void*			save)	/*!< in: immediate result
						from check function */
{
	innobase_old_blocks_pct = buf_LRU_old_ratio_update(
		*static_cast<const uint*>(save), TRUE);
}

/*************************************************************//**
Find the corresponding ibuf_use_t value that indexes into
innobase_change_buffering_values[] array for the input
change buffering option name.
@return	corresponding IBUF_USE_* value for the input variable
name, or IBUF_USE_COUNT if not able to find a match */
static
ibuf_use_t
innodb_find_change_buffering_value(
/*===============================*/
	const char*	input_name)	/*!< in: input change buffering
					option name */
{
	ulint	use;

	for (use = 0; use < UT_ARR_SIZE(innobase_change_buffering_values);
	     use++) {
		/* found a match */
		if (!innobase_strcasecmp(
			input_name, innobase_change_buffering_values[use])) {
			return((ibuf_use_t)use);
		}
	}

	/* Did not find any match */
	return(IBUF_USE_COUNT);
}

/*************************************************************//**
Check if it is a valid value of innodb_change_buffering. This function is
registered as a callback with MySQL.
@return	0 for valid innodb_change_buffering */
static
int
innodb_change_buffering_validate(
/*=============================*/
	THD*				thd,	/*!< in: thread handle */
	struct st_mysql_sys_var*	var,	/*!< in: pointer to system
						variable */
	void*				save,	/*!< out: immediate result
						for update function */
	struct st_mysql_value*		value)	/*!< in: incoming string */
{
	const char*	change_buffering_input;
	char		buff[STRING_BUFFER_USUAL_SIZE];
	int		len = sizeof(buff);

	ut_a(save != NULL);
	ut_a(value != NULL);

	change_buffering_input = value->val_str(value, buff, &len);

	if (change_buffering_input != NULL) {
		ibuf_use_t	use;

		use = innodb_find_change_buffering_value(
			change_buffering_input);

		if (use != IBUF_USE_COUNT) {
			/* Find a matching change_buffering option value. */
			*static_cast<const char**>(save) =
				innobase_change_buffering_values[use];

			return(0);
		}
	}

	/* No corresponding change buffering option for user supplied
	"change_buffering_input" */
	return(1);
}

/****************************************************************//**
Update the system variable innodb_change_buffering using the "saved"
value. This function is registered as a callback with MySQL. */
static
void
innodb_change_buffering_update(
/*===========================*/
	THD*				thd,	/*!< in: thread handle */
	struct st_mysql_sys_var*	var,	/*!< in: pointer to
						system variable */
	void*				var_ptr,/*!< out: where the
						formal string goes */
	const void*			save)	/*!< in: immediate result
						from check function */
{
	ibuf_use_t	use;

	ut_a(var_ptr != NULL);
	ut_a(save != NULL);

	use = innodb_find_change_buffering_value(
		*static_cast<const char*const*>(save));

	ut_a(use < IBUF_USE_COUNT);

	ibuf_use = use;
	*static_cast<const char**>(var_ptr) =
		 *static_cast<const char*const*>(save);
}

#ifdef UNIV_DEBUG
/*************************************************************//**
Check if it is a valid value of innodb_track_changed_pages.
Changed pages tracking is not working correctly without initialization
procedure on server startup. The function allows to temporary
disable tracking, but only if the feature was enabled on startup.
This function is registered as a callback with MySQL.
@return	0 for valid innodb_track_changed_pages */
static
int
innodb_track_changed_pages_validate(
	THD*				thd,	/*!< in: thread handle */
	struct st_mysql_sys_var*	var,	/*!< in: pointer to system
						variable */
	void*				save,	/*!< out: immediate result
						for update function */
	struct st_mysql_value*		value)	/*!< in: incoming bool */
{
	long long	intbuf = 0;

	if (value->val_int(value, &intbuf)) {
		/* The value is NULL. That is invalid. */
		return 1;
	}

	if (srv_redo_log_thread_started) {
		*reinterpret_cast<ulong*>(save)
			= static_cast<ulong>(intbuf);
		return 0;
	}

	if (intbuf == srv_track_changed_pages)
		return 0;

	return 1;
}
#endif

#ifndef DBUG_OFF
static char* srv_buffer_pool_evict;

/****************************************************************//**
Called on SET GLOBAL innodb_buffer_pool_evict=...
Handles some values specially, to evict pages from the buffer pool.
SET GLOBAL innodb_buffer_pool_evict='uncompressed'
evicts all uncompressed page frames of compressed tablespaces. */
static
void
innodb_buffer_pool_evict_update(
/*============================*/
	THD*			thd,	/*!< in: thread handle */
	struct st_mysql_sys_var*var,	/*!< in: pointer to system variable */
	void*			var_ptr,/*!< out: ignored */
	const void*		save)	/*!< in: immediate result
					from check function */
{
	if (const char* op = *static_cast<const char*const*>(save)) {
		if (!strcmp(op, "uncompressed")) {
			/* Evict all uncompressed pages of compressed
			tables from the buffer pool. Keep the compressed
			pages in the buffer pool. */

			for (ulint i = 0; i < srv_buf_pool_instances; i++) {
				buf_pool_t*	buf_pool = &buf_pool_ptr[i];

				//buf_pool_mutex_enter(buf_pool);
				ut_ad(!mutex_own(&buf_pool->LRU_list_mutex));
				mutex_enter(&buf_pool->LRU_list_mutex);

				for (buf_block_t* block = UT_LIST_GET_LAST(
					     buf_pool->unzip_LRU);
				     block != NULL; ) {

					buf_block_t*	prev_block
						= UT_LIST_GET_PREV(unzip_LRU,
								   block);
					ut_ad(buf_block_get_state(block)
					      == BUF_BLOCK_FILE_PAGE);
					ut_ad(block->in_unzip_LRU_list);
					ut_ad(block->page.in_LRU_list);

					mutex_enter(&block->mutex);
					ut_ad(mutex_own(&buf_pool->LRU_list_mutex));
					buf_LRU_free_block(&block->page,
						           FALSE, TRUE);
					mutex_exit(&block->mutex);
					block = prev_block;
				}

				ut_ad(mutex_own(&buf_pool->LRU_list_mutex));
				mutex_exit(&buf_pool->LRU_list_mutex);
				//buf_pool_mutex_exit(buf_pool);
			}
		}
	}
}
#endif /* !DBUG_OFF */

static int show_innodb_vars(THD *thd, SHOW_VAR *var, char *buff)
{
  innodb_export_status();
  var->type= SHOW_ARRAY;
  var->value= (char *) &innodb_status_variables;
  return 0;
}

/*********************************************************************//**
This function checks each index name for a table against reserved
system default primary index name 'GEN_CLUST_INDEX'. If a name
matches, this function pushes an warning message to the client,
and returns true.
@return true if the index name matches the reserved name */
extern "C" UNIV_INTERN
bool
innobase_index_name_is_reserved(
/*============================*/
	THD*		thd,		/*!< in/out: MySQL connection */
	const KEY*	key_info,	/*!< in: Indexes to be created */
	ulint		num_of_keys)	/*!< in: Number of indexes to
					be created. */
{
	const KEY*	key;
	uint		key_num;	/* index number */

	for (key_num = 0; key_num < num_of_keys; key_num++) {
		key = &key_info[key_num];

		if (innobase_strcasecmp(key->name,
					innobase_index_reserve_name) == 0) {
			/* Push warning to mysql */
			push_warning_printf(thd,
					    MYSQL_ERROR::WARN_LEVEL_WARN,
					    ER_WRONG_NAME_FOR_INDEX,
					    "Cannot Create Index with name "
					    "'%s'. The name is reserved "
					    "for the system default primary "
					    "index.",
					    innobase_index_reserve_name);

			my_error(ER_WRONG_NAME_FOR_INDEX, MYF(0),
				 innobase_index_reserve_name);

			return(true);
		}
	}

	return(false);
}

/***********************************************************************
functions for kill session of idle transaction */
extern "C"
ibool
innobase_thd_is_idle(
/*=================*/
	const void*	thd)	/*!< in: thread handle (THD*) */
{
#ifdef EXTENDED_FOR_KILLIDLE
	return(thd_command((const THD*) thd) == COM_SLEEP);
#else
	return(FALSE);
#endif
}

extern "C"
ib_int64_t
innobase_thd_get_start_time(
/*========================*/
	const void*	thd)	/*!< in: thread handle (THD*) */
{
#ifdef EXTENDED_FOR_KILLIDLE
	return((ib_int64_t)thd_start_time((const THD*) thd));
#else
	return(0); /*dummy value*/
#endif
}

extern "C"
void
innobase_thd_kill(
/*==============*/
	ulong	thd_id)
{
#ifdef EXTENDED_FOR_KILLIDLE
	thd_kill(thd_id);
#else
	return;
#endif
}

extern "C"
ulong
innobase_thd_get_thread_id(
/*=======================*/
	const void*	thd)
{
	return(thd_get_thread_id((const THD*) thd));
}

#ifdef UNIV_DEBUG
static my_bool	innodb_log_checkpoint_now = TRUE;

/****************************************************************//**
Force innodb to checkpoint. */
static
void
checkpoint_now_set(
/*===============*/
	THD*				thd	/*!< in: thread handle */
	__attribute__((unused)),
	struct st_mysql_sys_var*	var	/*!< in: pointer to system
						variable */
	__attribute__((unused)),
	void*				var_ptr	/*!< out: where the formal
						string goes */
	__attribute__((unused)),
	const void*			save)	/*!< in: immediate result from
						check function */
{
	if (*(my_bool*) save) {
		while (log_sys->last_checkpoint_lsn < log_sys->lsn) {
			log_make_checkpoint_at(IB_ULONGLONG_MAX, TRUE);
			fil_flush_file_spaces(FIL_LOG);
		}
		fil_write_flushed_lsn_to_data_files(log_sys->lsn, 0);
		fil_flush_file_spaces(FIL_TABLESPACE);
	}
}

static my_bool	innodb_track_redo_log_now = TRUE;

/****************************************************************//**
Force log tracker to track the log synchronously.  */
static
void
track_redo_log_now_set(
/*===================*/
	THD*				thd	/*!< in: thread handle */
	__attribute__((unused)),
	struct st_mysql_sys_var*	var	/*!< in: pointer to system
						variable */
	__attribute__((unused)),
	void*				var_ptr	/*!< out: where the formal
						string goes */
	__attribute__((unused)),
	const void*			save)	/*!< in: immediate result from
						check function */
{
	if (*(my_bool*) save && srv_track_changed_pages) {

		log_online_follow_redo_log();
	}
}


#endif /* UNIV_DEBUG */

static SHOW_VAR innodb_status_variables_export[]= {
  {"Innodb",                   (char*) &show_innodb_vars, SHOW_FUNC},
  {NullS, NullS, SHOW_LONG}
};

static struct st_mysql_storage_engine innobase_storage_engine=
{ MYSQL_HANDLERTON_INTERFACE_VERSION };

/* plugin options */
static MYSQL_SYSVAR_BOOL(checksums, innobase_use_checksums,
  PLUGIN_VAR_NOCMDARG | PLUGIN_VAR_READONLY,
  "Enable InnoDB checksums validation (enabled by default). "
  "Disable with --skip-innodb-checksums.",
  NULL, NULL, TRUE);

static MYSQL_SYSVAR_BOOL(fast_checksum, innobase_fast_checksum,
  PLUGIN_VAR_NOCMDARG | PLUGIN_VAR_READONLY,
  "DEPRECATED. #### WARNING #### : This feature is DEPRECATED and WILL "
  "be removed in Percona Server 5.6. "
  "Change the algorithm of checksum for the whole of datapage to 4-bytes word based. "
  "The original checksum is checked after the new one. It may be slow for reading page"
  " which has orginal checksum. Overwrite the page or recreate the InnoDB database, "
  "if you want the entire benefit for performance at once. "
  "#### Attention: The checksum is not compatible for normal or disabled version! ####",
  NULL, NULL, FALSE);

static MYSQL_SYSVAR_ULONG(page_size, innobase_page_size,
  PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_READONLY,
  "###EXPERIMENTAL###: The universal page size of the database. Changing for created database is not supported. Use on your own risk!",
  NULL, NULL, (1 << 14), (1 << 12), (1 << UNIV_PAGE_SIZE_SHIFT_MAX), 0);

static MYSQL_SYSVAR_ULONG(log_block_size, innobase_log_block_size,
  PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_READONLY,
  "###EXPERIMENTAL###: The log block size of the transaction log file. Changing for created log file is not supported. Use on your own risk!",
  NULL, NULL, (1 << 9)/*512*/, OS_MIN_LOG_BLOCK_SIZE,
  (1 << UNIV_PAGE_SIZE_SHIFT_MAX), 0);

static MYSQL_SYSVAR_STR(data_home_dir, innobase_data_home_dir,
  PLUGIN_VAR_READONLY,
  "The common part for InnoDB table spaces.",
  NULL, NULL, NULL);

static MYSQL_SYSVAR_BOOL(recovery_stats, innobase_recovery_stats,
  PLUGIN_VAR_NOCMDARG | PLUGIN_VAR_READONLY,
  "Output statistics of recovery process after it.",
  NULL, NULL, FALSE);

static MYSQL_SYSVAR_BOOL(recovery_update_relay_log, innobase_overwrite_relay_log_info,
  PLUGIN_VAR_NOCMDARG | PLUGIN_VAR_READONLY,
  "During InnoDB crash recovery on slave overwrite relay-log.info "
  "to align master log file position if information in InnoDB and relay-log.info is different.",
  NULL, NULL, FALSE);

static MYSQL_SYSVAR_BOOL(doublewrite, innobase_use_doublewrite,
  PLUGIN_VAR_NOCMDARG | PLUGIN_VAR_READONLY,
  "Enable InnoDB doublewrite buffer (enabled by default). "
  "Disable with --skip-innodb-doublewrite.",
  NULL, NULL, TRUE);

static MYSQL_SYSVAR_BOOL(use_atomic_writes, innobase_use_atomic_writes,
  PLUGIN_VAR_NOCMDARG | PLUGIN_VAR_READONLY,
  "Prevent partial page writes, via atomic writes (beta). "
  "The option is used to prevent partial writes in case of a crash/poweroff, "
  "as faster alternative to doublewrite buffer. "
  "Currently this option works only "
  "on Linux only with FusionIO device, and directFS filesystem.",
  NULL, NULL, FALSE);

static MYSQL_SYSVAR_BOOL(use_fallocate, innobase_use_fallocate,
  PLUGIN_VAR_NOCMDARG | PLUGIN_VAR_READONLY,
  "Preallocate files fast, using operating system functionality. On POSIX systems, posix_fallocate system call is used.",
  NULL, NULL, FALSE);

static MYSQL_SYSVAR_ULONG(io_capacity, srv_io_capacity,
  PLUGIN_VAR_RQCMDARG,
  "Number of IOPs the server can do. Tunes the background IO rate",
  NULL, NULL, 200, 100, ~0UL, 0);

static MYSQL_SYSVAR_ULONG(purge_batch_size, srv_purge_batch_size,
  PLUGIN_VAR_OPCMDARG,
  "Number of UNDO log pages to purge in one batch from the history list.",
  NULL, NULL,
  20,			/* Default setting */
  1,			/* Minimum value */
  5000, 0);		/* Maximum value */

static MYSQL_SYSVAR_ULONG(rollback_segments, srv_rollback_segments,
  PLUGIN_VAR_OPCMDARG,
  "Number of UNDO logs to use.",
  NULL, NULL,
  128,			/* Default setting */
  1,			/* Minimum value */
  TRX_SYS_N_RSEGS, 0);	/* Maximum value */

static MYSQL_SYSVAR_ULONG(purge_threads, srv_n_purge_threads,
  PLUGIN_VAR_OPCMDARG | PLUGIN_VAR_READONLY,
  "Purge threads can be either 0 or 1.",
  NULL, NULL,
  1,			/* Default setting */
  0,			/* Minimum value */
  1, 0);		/* Maximum value */

static MYSQL_SYSVAR_ULONG(fast_shutdown, innobase_fast_shutdown,
  PLUGIN_VAR_OPCMDARG,
  "Speeds up the shutdown process of the InnoDB storage engine. Possible "
  "values are 0, 1 (faster) or 2 (fastest - crash-like).",
  NULL, NULL, 1, 0, 2, 0);

static MYSQL_SYSVAR_BOOL(file_per_table, srv_file_per_table,
  PLUGIN_VAR_NOCMDARG,
  "Stores each InnoDB table to an .ibd file in the database dir.",
  NULL, NULL, FALSE);

static MYSQL_SYSVAR_STR(file_format, innobase_file_format_name,
  PLUGIN_VAR_RQCMDARG,
  "File format to use for new tables in .ibd files.",
  innodb_file_format_name_validate,
  innodb_file_format_name_update, "Antelope");

/* "innobase_file_format_check" decides whether we would continue
booting the server if the file format stamped on the system
table space exceeds the maximum file format supported
by the server. Can be set during server startup at command
line or configure file, and a read only variable after
server startup */
static MYSQL_SYSVAR_BOOL(file_format_check, innobase_file_format_check,
  PLUGIN_VAR_NOCMDARG | PLUGIN_VAR_READONLY,
  "Whether to perform system file format check.",
  NULL, NULL, TRUE);

/* If a new file format is introduced, the file format
name needs to be updated accordingly. Please refer to
file_format_name_map[] defined in trx0sys.c for the next
file format name. */
static MYSQL_SYSVAR_STR(file_format_max, innobase_file_format_max,
  PLUGIN_VAR_OPCMDARG,
  "The highest file format in the tablespace.",
  innodb_file_format_max_validate,
  innodb_file_format_max_update, "Antelope");

/* Changed to the THDVAR */
//static MYSQL_SYSVAR_ULONG(flush_log_at_trx_commit, srv_flush_log_at_trx_commit,
//  PLUGIN_VAR_OPCMDARG,
//  "Set to 0 (write and flush once per second),"
//  " 1 (write and flush at each commit)"
//  " or 2 (write at commit, flush once per second).",
//  NULL, NULL, 1, 0, 2, 0);

static MYSQL_SYSVAR_BOOL(use_global_flush_log_at_trx_commit, srv_use_global_flush_log_at_trx_commit,
  PLUGIN_VAR_NOCMDARG,
  "Use global innodb_flush_log_at_trx_commit value. (default: ON).",
  NULL, NULL, TRUE);

static MYSQL_SYSVAR_STR(flush_method, innobase_file_flush_method,
  PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_READONLY,
  "With which method to flush data.", NULL, NULL, NULL);

static MYSQL_SYSVAR_BOOL(large_prefix, innobase_large_prefix,
  PLUGIN_VAR_NOCMDARG,
  "Support large index prefix length of REC_VERSION_56_MAX_INDEX_COL_LEN (3072) bytes.",
  NULL, NULL, FALSE);

static MYSQL_SYSVAR_BOOL(force_load_corrupted, srv_load_corrupted,
  PLUGIN_VAR_NOCMDARG | PLUGIN_VAR_READONLY,
  "Force InnoDB to load metadata of corrupted table.",
  NULL, NULL, FALSE);

static MYSQL_SYSVAR_BOOL(locks_unsafe_for_binlog, innobase_locks_unsafe_for_binlog,
  PLUGIN_VAR_NOCMDARG | PLUGIN_VAR_READONLY,
  "Force InnoDB to not use next-key locking, to use only row-level locking.",
  NULL, NULL, FALSE);

static MYSQL_SYSVAR_ULONG(show_verbose_locks, srv_show_verbose_locks,
  PLUGIN_VAR_OPCMDARG,
  "Whether to show records locked in SHOW INNODB STATUS.",
  NULL, NULL, 0, 0, 1, 0);

static MYSQL_SYSVAR_ULONG(show_locks_held, srv_show_locks_held,
  PLUGIN_VAR_RQCMDARG,
  "Number of locks held to print for each InnoDB transaction in SHOW INNODB STATUS.",
  NULL, NULL, 10, 0, 1000, 0);

#ifdef UNIV_LOG_ARCHIVE
static MYSQL_SYSVAR_STR(log_arch_dir, innobase_log_arch_dir,
  PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_READONLY,
  "Where full logs should be archived.", NULL, NULL, NULL);

static MYSQL_SYSVAR_BOOL(log_archive, innobase_log_archive,
  PLUGIN_VAR_OPCMDARG | PLUGIN_VAR_READONLY,
  "Set to 1 if you want to have logs archived.", NULL, NULL, FALSE);
#endif /* UNIV_LOG_ARCHIVE */

static MYSQL_SYSVAR_STR(log_group_home_dir, innobase_log_group_home_dir,
  PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_READONLY,
  "Path to InnoDB log files.", NULL, NULL, NULL);

static MYSQL_SYSVAR_ULONG(max_dirty_pages_pct, srv_max_buf_pool_modified_pct,
  PLUGIN_VAR_RQCMDARG,
  "Percentage of dirty pages allowed in bufferpool.",
  NULL, NULL, 75, 0, 99, 0);

static MYSQL_SYSVAR_BOOL(adaptive_flushing, srv_adaptive_flushing,
  PLUGIN_VAR_NOCMDARG,
  "Attempt flushing dirty pages to avoid IO bursts at checkpoints.",
  NULL, NULL, TRUE);

static MYSQL_SYSVAR_ULONG(max_purge_lag, srv_max_purge_lag,
  PLUGIN_VAR_RQCMDARG,
  "Desired maximum length of the purge queue (0 = no limit)",
  NULL, NULL, 0, 0, ~0UL, 0);

static MYSQL_SYSVAR_BOOL(rollback_on_timeout, innobase_rollback_on_timeout,
  PLUGIN_VAR_OPCMDARG | PLUGIN_VAR_READONLY,
  "Roll back the complete transaction on lock wait timeout, for 4.x compatibility (disabled by default)",
  NULL, NULL, FALSE);

static MYSQL_SYSVAR_BOOL(status_file, innobase_create_status_file,
  PLUGIN_VAR_OPCMDARG | PLUGIN_VAR_NOSYSVAR,
  "Enable SHOW INNODB STATUS output in the innodb_status.<pid> file",
  NULL, NULL, FALSE);

static MYSQL_SYSVAR_BOOL(stats_on_metadata, innobase_stats_on_metadata,
  PLUGIN_VAR_OPCMDARG,
  "Enable statistics gathering for metadata commands such as SHOW TABLE STATUS (on by default)",
  NULL, NULL, TRUE);

static MYSQL_SYSVAR_ULONGLONG(stats_sample_pages, srv_stats_sample_pages,
  PLUGIN_VAR_RQCMDARG,
  "The number of index pages to sample when calculating statistics (default 8)",
  NULL, NULL, 8, 1, ~0ULL, 0);

static MYSQL_SYSVAR_ULONGLONG(stats_modified_counter, srv_stats_modified_counter,
  PLUGIN_VAR_RQCMDARG,
  "The number of rows modified before we calculate new statistics (default 0 = current limits)",
  NULL, NULL, 0, 0, ~0ULL, 0);

static MYSQL_SYSVAR_BOOL(stats_traditional, srv_stats_sample_traditional,
  PLUGIN_VAR_RQCMDARG,
  "Enable traditional statistic calculation based on number of configured pages (default true)",
  NULL, NULL, TRUE);

static MYSQL_SYSVAR_ULINT(stats_auto_update, srv_stats_auto_update,
  PLUGIN_VAR_RQCMDARG,
  "Enable/Disable InnoDB's auto update statistics of indexes. "
  "(except for ANALYZE TABLE command) 0:disable 1:enable",
  NULL, NULL, 1, 0, 1, 0);

static MYSQL_SYSVAR_ULINT(stats_update_need_lock, srv_stats_update_need_lock,
  PLUGIN_VAR_RQCMDARG,
  "Enable/Disable InnoDB's update statistics which needs to lock dictionary. "
  "e.g. Data_free.",
  NULL, NULL, 1, 0, 1, 0);

static MYSQL_SYSVAR_BOOL(use_sys_stats_table, innobase_use_sys_stats_table,
  PLUGIN_VAR_NOCMDARG | PLUGIN_VAR_READONLY,
  "Enable to use SYS_STATS system table to store statistics statically, "
  "And avoids to calculate statistics at every first open of the tables. "
  "This option may make the opportunities of update statistics less. "
  "So you should use ANALYZE TABLE command intentionally.",
  NULL, NULL, FALSE);

#ifdef UNIV_DEBUG
static MYSQL_SYSVAR_ULONG(persistent_stats_root_page,
  innobase_sys_stats_root_page, PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_READONLY,
  "Override the SYS_STATS root page id, 0 = no override (for testing only)",
  NULL, NULL, 0, 0, ULONG_MAX, 0);
#endif

static MYSQL_SYSVAR_BOOL(adaptive_hash_index, btr_search_enabled,
  PLUGIN_VAR_OPCMDARG,
  "Enable InnoDB adaptive hash index (enabled by default).  "
  "Disable with --skip-innodb-adaptive-hash-index.",
  NULL, innodb_adaptive_hash_index_update, TRUE);

/* btr_search_index_num is constrained to machine word size for historical
reasons. This limitation can be easily removed later. */
static MYSQL_SYSVAR_ULINT(adaptive_hash_index_partitions, btr_search_index_num,
  PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_READONLY,
  "Number of InnoDB adaptive hash index partitions (default 1: disable partitioning)",
  NULL, NULL, 1, 1, sizeof(ulint) * 8, 0);

static MYSQL_SYSVAR_ULONG(replication_delay, srv_replication_delay,
  PLUGIN_VAR_RQCMDARG,
  "Replication thread delay (ms) on the slave server if "
  "innodb_thread_concurrency is reached (0 by default)",
  NULL, NULL, 0, 0, ~0UL, 0);

static MYSQL_SYSVAR_LONG(additional_mem_pool_size, innobase_additional_mem_pool_size,
  PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_READONLY,
  "Size of a memory pool InnoDB uses to store data dictionary information and other internal data structures.",
  NULL, NULL, 8*1024*1024L, 512*1024L, LONG_MAX, 1024);

static MYSQL_SYSVAR_ULONG(autoextend_increment, srv_auto_extend_increment,
  PLUGIN_VAR_RQCMDARG,
  "Data file autoextend increment in megabytes",
  NULL, NULL, 8L, 1L, 1000L, 0);

#ifndef DBUG_OFF
static MYSQL_SYSVAR_STR(buffer_pool_evict, srv_buffer_pool_evict,
  PLUGIN_VAR_RQCMDARG,
  "Evict pages from the InnoDB buffer pool.",
  NULL, innodb_buffer_pool_evict_update, "");
#endif /* !DBUG_OFF */

static MYSQL_SYSVAR_LONGLONG(buffer_pool_size, innobase_buffer_pool_size,
  PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_READONLY,
  "The size of the memory buffer InnoDB uses to cache data and indexes of its tables.",
  NULL, NULL, 128*1024*1024L, 5*1024*1024L, LONGLONG_MAX, 1024*1024L);

static MYSQL_SYSVAR_BOOL(buffer_pool_populate, srv_buf_pool_populate,
  PLUGIN_VAR_NOCMDARG | PLUGIN_VAR_READONLY,
  "Preallocate (pre-fault) the page frames required for the mapping "
  "established by the buffer pool memory region. Disabled by default.",
  NULL, NULL, FALSE);

static MYSQL_SYSVAR_LONG(buffer_pool_instances, innobase_buffer_pool_instances,
  PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_READONLY,
  "Number of buffer pool instances, set to higher value on high-end machines to increase scalability",
  NULL, NULL, 1L, 1L, MAX_BUFFER_POOLS, 1L);

static MYSQL_SYSVAR_UINT(buffer_pool_shm_key, innobase_buffer_pool_shm_key,
  PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_READONLY,
  "[Deprecated option] no effect",
  NULL, NULL, 0, 0, INT_MAX32, 0);

static MYSQL_SYSVAR_BOOL(buffer_pool_shm_checksum, innobase_buffer_pool_shm_checksum,
  PLUGIN_VAR_NOCMDARG | PLUGIN_VAR_READONLY,
  "[Deprecated option] no effect",
  NULL, NULL, TRUE);

static MYSQL_SYSVAR_ULONG(commit_concurrency, innobase_commit_concurrency,
  PLUGIN_VAR_RQCMDARG,
  "Helps in performance tuning in heavily concurrent environments.",
  innobase_commit_concurrency_validate, NULL, 0, 0, 1000, 0);

static MYSQL_SYSVAR_ULONG(concurrency_tickets, srv_n_free_tickets_to_enter,
  PLUGIN_VAR_RQCMDARG,
  "Number of times a thread is allowed to enter InnoDB within the same SQL query after it has once got the ticket",
  NULL, NULL, 500L, 1L, ~0UL, 0);

#ifdef EXTENDED_FOR_KILLIDLE
#define kill_idle_help_text "If non-zero value, the idle session with transaction which is idle over the value in seconds is killed by InnoDB."
#else
#define kill_idle_help_text "No effect for this build."
#endif
static MYSQL_SYSVAR_LONGLONG(kill_idle_transaction, srv_kill_idle_transaction,
  PLUGIN_VAR_RQCMDARG, kill_idle_help_text, NULL, NULL, 0, 0, LONG_MAX, 0);

static MYSQL_SYSVAR_LONG(file_io_threads, innobase_file_io_threads,
  PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_READONLY | PLUGIN_VAR_NOSYSVAR,
  "Number of file I/O threads in InnoDB.",
  NULL, NULL, 4, 4, 64, 0);

static MYSQL_SYSVAR_ULONG(read_io_threads, innobase_read_io_threads,
  PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_READONLY,
  "Number of background read I/O threads in InnoDB.",
  NULL, NULL, 4, 1, 64, 0);

static MYSQL_SYSVAR_ULONG(write_io_threads, innobase_write_io_threads,
  PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_READONLY,
  "Number of background write I/O threads in InnoDB.",
  NULL, NULL, 4, 1, 64, 0);

static MYSQL_SYSVAR_LONG(force_recovery, innobase_force_recovery,
  PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_READONLY,
  "Helps to save your data in case the disk image of the database becomes corrupt.",
  NULL, NULL, 0, 0, 6, 0);

static MYSQL_SYSVAR_LONG(log_buffer_size, innobase_log_buffer_size,
  PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_READONLY,
  "The size of the buffer which InnoDB uses to write log to the log files on disk.",
  NULL, NULL, 8*1024*1024L, 256*1024L, LONG_MAX, 1024);

static MYSQL_SYSVAR_LONGLONG(log_file_size, innobase_log_file_size,
  PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_READONLY,
  "Size of each log file in a log group.",
  NULL, NULL, 5*1024*1024L, 1*1024*1024L, LONGLONG_MAX, 1024*1024L);

static MYSQL_SYSVAR_LONG(log_files_in_group, innobase_log_files_in_group,
  PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_READONLY,
  "Number of log files in the log group. InnoDB writes to the files in a circular fashion. Value 3 is recommended here.",
  NULL, NULL, 2, 2, 100, 0);

static MYSQL_SYSVAR_LONG(mirrored_log_groups, innobase_mirrored_log_groups,
  PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_READONLY,
  "Number of identical copies of log groups we keep for the database. Currently this should be set to 1.",
  NULL, NULL, 1, 1, 10, 0);

static MYSQL_SYSVAR_UINT(old_blocks_pct, innobase_old_blocks_pct,
  PLUGIN_VAR_RQCMDARG,
  "Percentage of the buffer pool to reserve for 'old' blocks.",
  NULL, innodb_old_blocks_pct_update, 100 * 3 / 8, 5, 95, 0);

static MYSQL_SYSVAR_UINT(old_blocks_time, buf_LRU_old_threshold_ms,
  PLUGIN_VAR_RQCMDARG,
  "Move blocks to the 'new' end of the buffer pool if the first access"
  " was at least this many milliseconds ago."
  " The timeout is disabled if 0 (the default).",
  NULL, NULL, 0, 0, UINT_MAX32, 0);

static MYSQL_SYSVAR_LONG(open_files, innobase_open_files,
  PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_READONLY,
  "How many files at the maximum InnoDB keeps open at the same time.",
  NULL, NULL, 300L, 10L, LONG_MAX, 0);

static MYSQL_SYSVAR_ULONG(sync_spin_loops, srv_n_spin_wait_rounds,
  PLUGIN_VAR_RQCMDARG,
  "Count of spin-loop rounds in InnoDB mutexes (30 by default)",
  NULL, NULL, 30L, 0L, ~0UL, 0);

static MYSQL_SYSVAR_ULONG(spin_wait_delay, srv_spin_wait_delay,
  PLUGIN_VAR_OPCMDARG,
  "Maximum delay between polling for a spin lock (6 by default)",
  NULL, NULL, 6L, 0L, ~0UL, 0);

static MYSQL_SYSVAR_BOOL(thread_concurrency_timer_based,
  innobase_thread_concurrency_timer_based,
  PLUGIN_VAR_NOCMDARG | PLUGIN_VAR_READONLY,
  "Use InnoDB timer based concurrency throttling. ",
  NULL, NULL, FALSE);

static MYSQL_SYSVAR_ULONG(thread_concurrency, srv_thread_concurrency,
  PLUGIN_VAR_RQCMDARG,
  "Helps in performance tuning in heavily concurrent environments. Sets the maximum number of threads allowed inside InnoDB. Value 0 will disable the thread throttling.",
  NULL, NULL, 0, 0, 1000, 0);

static MYSQL_SYSVAR_ULONG(thread_sleep_delay, srv_thread_sleep_delay,
  PLUGIN_VAR_RQCMDARG,
  "Time of innodb thread sleeping before joining InnoDB queue (usec). Value 0 disable a sleep",
  NULL, NULL, 10000L, 0L, 1000000L, 0);

static MYSQL_SYSVAR_STR(data_file_path, innobase_data_file_path,
  PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_READONLY,
  "Path to individual files and their sizes.",
  NULL, NULL, NULL);

static MYSQL_SYSVAR_STR(doublewrite_file, innobase_doublewrite_file,
  PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_READONLY,
  "Path to special datafile for doublewrite buffer. (default is "": not used) ### ONLY FOR EXPERTS!!! ###",
  NULL, NULL, NULL);

static MYSQL_SYSVAR_LONG(autoinc_lock_mode, innobase_autoinc_lock_mode,
  PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_READONLY,
  "The AUTOINC lock modes supported by InnoDB:               "
  "0 => Old style AUTOINC locking (for backward"
  " compatibility)                                           "
  "1 => New style AUTOINC locking                            "
  "2 => No AUTOINC locking (unsafe for SBR)",
  NULL, NULL,
  AUTOINC_NEW_STYLE_LOCKING,	/* Default setting */
  AUTOINC_OLD_STYLE_LOCKING,	/* Minimum value */
  AUTOINC_NO_LOCKING, 0);	/* Maximum value */

static MYSQL_SYSVAR_STR(version, innodb_version_str,
  PLUGIN_VAR_NOCMDOPT | PLUGIN_VAR_READONLY,
  "Percona-InnoDB-plugin version", NULL, NULL, INNODB_VERSION_STR);

static MYSQL_SYSVAR_BOOL(use_sys_malloc, srv_use_sys_malloc,
  PLUGIN_VAR_NOCMDARG | PLUGIN_VAR_READONLY,
  "Use OS memory allocator instead of InnoDB's internal memory allocator",
  NULL, NULL, TRUE);

static MYSQL_SYSVAR_BOOL(use_native_aio, srv_use_native_aio,
  PLUGIN_VAR_NOCMDARG | PLUGIN_VAR_READONLY,
  "Use native AIO if supported on this platform.",
  NULL, NULL, TRUE);

static MYSQL_SYSVAR_STR(change_buffering, innobase_change_buffering,
  PLUGIN_VAR_RQCMDARG,
  "Buffer changes to reduce random access: "
  "OFF, ON, inserting, deleting, changing, or purging.",
  innodb_change_buffering_validate,
  innodb_change_buffering_update, "all");

static MYSQL_SYSVAR_ENUM(stats_method, srv_innodb_stats_method,
   PLUGIN_VAR_RQCMDARG,
  "Specifies how InnoDB index statistics collection code should "
  "treat NULLs. Possible values are NULLS_EQUAL (default), "
  "NULLS_UNEQUAL and NULLS_IGNORED",
   NULL, NULL, SRV_STATS_NULLS_EQUAL, &innodb_stats_method_typelib);

#ifdef UNIV_DEBUG
/* Make this variable dynamic for debug builds to
provide a testcase sync facility */
#define track_changed_pages_flags PLUGIN_VAR_NOCMDARG
#define track_changed_pages_check innodb_track_changed_pages_validate
#else
#define track_changed_pages_flags PLUGIN_VAR_NOCMDARG | PLUGIN_VAR_READONLY
#define track_changed_pages_check NULL
#endif
static MYSQL_SYSVAR_BOOL(track_changed_pages, srv_track_changed_pages,
  track_changed_pages_flags,
  "Track the redo log for changed pages and output a changed page bitmap",
  track_changed_pages_check,
  NULL, FALSE);

static MYSQL_SYSVAR_ULONGLONG(max_bitmap_file_size, srv_max_bitmap_file_size,
    PLUGIN_VAR_RQCMDARG,
    "The maximum size of changed page bitmap files",
    NULL, NULL, 100*1024*1024ULL, 4096ULL, ULONGLONG_MAX, 0);

static MYSQL_SYSVAR_ULONGLONG(max_changed_pages, srv_max_changed_pages,
  PLUGIN_VAR_RQCMDARG,
  "The maximum number of rows for "
  "INFORMATION_SCHEMA.INNODB_CHANGED_PAGES table, "
  "0 - unlimited",
  NULL, NULL, 1000000, 0, ~0ULL, 0);

#if defined UNIV_DEBUG || defined UNIV_IBUF_DEBUG
static MYSQL_SYSVAR_UINT(change_buffering_debug, ibuf_debug,
  PLUGIN_VAR_RQCMDARG,
  "Debug flags for InnoDB change buffering (0=none, 2=crash at merge)",
  NULL, NULL, 0, 0, 2, 0);
#endif /* UNIV_DEBUG || UNIV_IBUF_DEBUG */

static MYSQL_SYSVAR_BOOL(random_read_ahead, srv_random_read_ahead,
  PLUGIN_VAR_NOCMDARG,
  "Whether to use read ahead for random access within an extent.",
  NULL, NULL, FALSE);

static MYSQL_SYSVAR_ULONG(read_ahead_threshold, srv_read_ahead_threshold,
  PLUGIN_VAR_RQCMDARG,
  "Number of pages that must be accessed sequentially for InnoDB to "
  "trigger a readahead.",
  NULL, NULL, 56, 0, 64, 0);

#ifdef UNIV_DEBUG
static MYSQL_SYSVAR_UINT(trx_rseg_n_slots_debug, trx_rseg_n_slots_debug,
  PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_NOCMDOPT,
  "Debug flags for InnoDB to limit TRX_RSEG_N_SLOTS for trx_rsegf_undo_find_free()",
  NULL, NULL, 0, 0, 1024, 0);

static MYSQL_SYSVAR_UINT(limit_optimistic_insert_debug,
  btr_cur_limit_optimistic_insert_debug, PLUGIN_VAR_RQCMDARG,
  "Artificially limit the number of records per B-tree page (0=unlimited).",
  NULL, NULL, 0, 0, UINT_MAX32, 0);

static MYSQL_SYSVAR_BOOL(trx_purge_view_update_only_debug,
  srv_purge_view_update_only_debug, PLUGIN_VAR_NOCMDOPT,
  "Pause actual purging any delete-marked records, but merely update the purge view. "
  "It is to create artificially the situation the purge view have been updated "
  "but the each purges were not done yet.",
  NULL, NULL, FALSE);
#endif /* UNIV_DEBUG */

static MYSQL_SYSVAR_LONGLONG(ibuf_max_size, srv_ibuf_max_size,
  PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_READONLY,
  "The maximum size of the insert buffer. (in bytes)",
  NULL, NULL, LONGLONG_MAX, 0, LONGLONG_MAX, 0);

static MYSQL_SYSVAR_ULONG(ibuf_active_contract, srv_ibuf_active_contract,
  PLUGIN_VAR_RQCMDARG,
  "Enable/Disable active_contract of insert buffer. 0:disable 1:enable",
  NULL, NULL, 1, 0, 1, 0);

static MYSQL_SYSVAR_ULONG(ibuf_accel_rate, srv_ibuf_accel_rate,
  PLUGIN_VAR_RQCMDARG,
  "Tunes amount of insert buffer processing of background, in addition to innodb_io_capacity. (in percentage)",
  NULL, NULL, 100, 100, 999999999, 0);

static MYSQL_SYSVAR_ULINT(checkpoint_age_target, srv_checkpoint_age_target,
  PLUGIN_VAR_RQCMDARG,
  "Control soft limit of checkpoint age. (0 : not control)",
  NULL, NULL, 0, 0, ~0UL, 0);

static MYSQL_SYSVAR_UINT(simulate_comp_failures, srv_simulate_comp_failures,
  PLUGIN_VAR_NOCMDARG,
  "Simulate compression failures.",
  NULL, NULL, 0, 0, 99, 0);

static
void
innodb_flush_neighbor_pages_update(
  THD* thd,
  struct st_mysql_sys_var* var,
  void* var_ptr,
  const void* save)
{
  *(long *)var_ptr = (*(long *)save) % 3;
}

const char *flush_neighbor_pages_names[]=
{
  "none", /* 0 */
  "area",
  "cont", /* 2 */
  /* For compatibility with the older patch */
  "0", /* "none" + 3 */
  "1", /* "area" + 3 */
  "2", /* "cont" + 3 */
  NullS
};

TYPELIB flush_neighbor_pages_typelib=
{
  array_elements(flush_neighbor_pages_names) - 1,
  "flush_neighbor_pages_typelib",
  flush_neighbor_pages_names,
  NULL
};

static MYSQL_SYSVAR_ENUM(flush_neighbor_pages, srv_flush_neighbor_pages,
  PLUGIN_VAR_RQCMDARG, "Neighbor page flushing behaviour: none: do not flush, "
                       "[area]: flush selected pages one-by-one, "
                       "cont: flush a contiguous block of pages", NULL,
  innodb_flush_neighbor_pages_update, 1, &flush_neighbor_pages_typelib);

static
void
innodb_read_ahead_update(
  THD* thd,
  struct st_mysql_sys_var*     var,
  void*        var_ptr,
  const void*  save)
{
  *(long *)var_ptr= (*(long *)save) & 3;
}
const char *read_ahead_names[]=
{
  "none", /* 0 */
  "random",
  "linear",
  "both", /* 3 */
  /* For compatibility of the older patch */
  "0", /* 4 ("none" + 4) */
  "1",
  "2",
  "3", /* 7 ("both" + 4) */
  NullS
};
TYPELIB read_ahead_typelib=
{
  array_elements(read_ahead_names) - 1, "read_ahead_typelib",
  read_ahead_names, NULL
};
static MYSQL_SYSVAR_ENUM(read_ahead, srv_read_ahead,
  PLUGIN_VAR_RQCMDARG,
  "Control read ahead activity (none, random, [linear], both). [from 1.0.5: random read ahead is ignored]",
  NULL, innodb_read_ahead_update, 2, &read_ahead_typelib);

static
void
innodb_adaptive_flushing_method_update(
  THD* thd,
  struct st_mysql_sys_var*     var,
  void*        var_ptr,
  const void*  save)
{
  *(long *)var_ptr= (*(long *)save) % 3;
}
const char *adaptive_flushing_method_names[]=
{
  "native", /* 0 */
  "estimate", /* 1 */
  "keep_average", /* 2 */
  /* For compatibility of the older patch */
  "0", /* 3 ("none" + 3) */
  "1", /* 4 ("estimate" + 3) */
  "2", /* 5 ("keep_average" + 3) */
  NullS
};
TYPELIB adaptive_flushing_method_typelib=
{
  array_elements(adaptive_flushing_method_names) - 1, "adaptive_flushing_method_typelib",
  adaptive_flushing_method_names, NULL
};
static MYSQL_SYSVAR_ENUM(adaptive_flushing_method, srv_adaptive_flushing_method,
  PLUGIN_VAR_RQCMDARG,
  "Choose method of innodb_adaptive_flushing. (native, [estimate], keep_average)",
  NULL, innodb_adaptive_flushing_method_update, 1, &adaptive_flushing_method_typelib);

static MYSQL_SYSVAR_ULONG(import_table_from_xtrabackup, srv_expand_import,
  PLUGIN_VAR_RQCMDARG,
  "Enable/Disable converting automatically *.ibd files when import tablespace.",
  NULL, NULL, 0, 0, 1, 0);

static MYSQL_SYSVAR_ULINT(dict_size_limit, srv_dict_size_limit,
  PLUGIN_VAR_RQCMDARG,
  "Limit the allocated memory for dictionary cache. (0: unlimited)",
  NULL, NULL, 0, 0, LONG_MAX, 0);

static MYSQL_SYSVAR_UINT(buffer_pool_restore_at_startup, srv_auto_lru_dump,
  PLUGIN_VAR_RQCMDARG,
  "Time in seconds between automatic buffer pool dumps. "
  "0 (the default) disables automatic dumps.",
  NULL, NULL, 0, 0, UINT_MAX32, 0);

static MYSQL_SYSVAR_BOOL(blocking_buffer_pool_restore,
  innobase_blocking_lru_restore,
  PLUGIN_VAR_OPCMDARG | PLUGIN_VAR_READONLY,
  "Block XtraDB startup process until buffer pool is full restored from a "
  "dump file (if present). Disabled by default.",
  NULL, NULL, FALSE);

const char *corrupt_table_action_names[]=
{
  "assert", /* 0 */
  "warn", /* 1 */
  "salvage", /* 2 */
  NullS
};
TYPELIB corrupt_table_action_typelib=
{
  array_elements(corrupt_table_action_names) - 1, "corrupt_table_action_typelib",
  corrupt_table_action_names, NULL
};
static	MYSQL_SYSVAR_ENUM(corrupt_table_action, srv_pass_corrupt_table,
  PLUGIN_VAR_RQCMDARG,
  "Warn corruptions of user tables as 'corrupt table' instead of not crashing itself, "
  "when used with file_per_table. "
  "All file io for the datafile after detected as corrupt are disabled, "
  "except for the deletion. Possible options are 'assert', 'warn' & 'salvage'",
  NULL, NULL, 0, &corrupt_table_action_typelib);

static MYSQL_SYSVAR_ULINT(lazy_drop_table, srv_lazy_drop_table,
  PLUGIN_VAR_RQCMDARG,
  "[Deprecated option] no effect",
  NULL, NULL, 0, 0, 1, 0);

#ifdef UNIV_DEBUG

static MYSQL_SYSVAR_BOOL(log_checkpoint_now, innodb_log_checkpoint_now,
  PLUGIN_VAR_OPCMDARG,
  "Force checkpoint now",
  NULL, checkpoint_now_set, FALSE);

static MYSQL_SYSVAR_BOOL(track_redo_log_now,
  innodb_track_redo_log_now,
  PLUGIN_VAR_OPCMDARG,
  "Force log tracker to catch up with checkpoint now",
  NULL, track_redo_log_now_set, FALSE);

#endif /* UNIV_DEBUG */

static MYSQL_SYSVAR_BOOL(locking_fake_changes, srv_fake_changes_locks,
  PLUGIN_VAR_NOCMDARG,
  "###EXPERIMENTAL### if enabled, transactions will get S row locks instead "
  "of X locks for fake changes.  If disabled, fake change transactions will "
  "not take any locks at all.",
  NULL, NULL, TRUE);

static MYSQL_SYSVAR_BOOL(print_all_deadlocks, srv_print_all_deadlocks,
  PLUGIN_VAR_OPCMDARG,
  "Print all deadlocks to MySQL error log (off by default)",
  NULL, NULL, FALSE);

static MYSQL_SYSVAR_BOOL(use_stacktrace, srv_use_stacktrace,
  PLUGIN_VAR_NOCMDARG | PLUGIN_VAR_READONLY,
  "Print stacktrace on long semaphore wait (off by default supported only on linux)",
  NULL, NULL, FALSE);

static struct st_mysql_sys_var* innobase_system_variables[]= {
  MYSQL_SYSVAR(page_size),
  MYSQL_SYSVAR(log_block_size),
  MYSQL_SYSVAR(additional_mem_pool_size),
  MYSQL_SYSVAR(autoextend_increment),
#ifndef DBUG_OFF
  MYSQL_SYSVAR(buffer_pool_evict),
#endif /* !DBUG_OFF */
  MYSQL_SYSVAR(buffer_pool_size),
  MYSQL_SYSVAR(buffer_pool_populate),
  MYSQL_SYSVAR(buffer_pool_instances),
  MYSQL_SYSVAR(buffer_pool_shm_key),
  MYSQL_SYSVAR(buffer_pool_shm_checksum),
  MYSQL_SYSVAR(checksums),
  MYSQL_SYSVAR(fast_checksum),
  MYSQL_SYSVAR(commit_concurrency),
  MYSQL_SYSVAR(concurrency_tickets),
  MYSQL_SYSVAR(kill_idle_transaction),
  MYSQL_SYSVAR(data_file_path),
  MYSQL_SYSVAR(doublewrite_file),
  MYSQL_SYSVAR(data_home_dir),
  MYSQL_SYSVAR(doublewrite),
  MYSQL_SYSVAR(use_atomic_writes),
  MYSQL_SYSVAR(use_fallocate),
  MYSQL_SYSVAR(recovery_stats),
  MYSQL_SYSVAR(fast_shutdown),
  MYSQL_SYSVAR(file_io_threads),
  MYSQL_SYSVAR(read_io_threads),
  MYSQL_SYSVAR(write_io_threads),
  MYSQL_SYSVAR(file_per_table),
  MYSQL_SYSVAR(file_format),
  MYSQL_SYSVAR(file_format_check),
  MYSQL_SYSVAR(file_format_max),
  MYSQL_SYSVAR(flush_log_at_trx_commit),
  MYSQL_SYSVAR(use_global_flush_log_at_trx_commit),
  MYSQL_SYSVAR(flush_method),
  MYSQL_SYSVAR(force_recovery),
  MYSQL_SYSVAR(large_prefix),
  MYSQL_SYSVAR(force_load_corrupted),
  MYSQL_SYSVAR(locks_unsafe_for_binlog),
  MYSQL_SYSVAR(lock_wait_timeout),
#ifdef UNIV_LOG_ARCHIVE
  MYSQL_SYSVAR(log_arch_dir),
  MYSQL_SYSVAR(log_archive),
#endif /* UNIV_LOG_ARCHIVE */
  MYSQL_SYSVAR(log_buffer_size),
  MYSQL_SYSVAR(log_file_size),
  MYSQL_SYSVAR(log_files_in_group),
  MYSQL_SYSVAR(log_group_home_dir),
  MYSQL_SYSVAR(max_dirty_pages_pct),
  MYSQL_SYSVAR(adaptive_flushing),
  MYSQL_SYSVAR(max_purge_lag),
  MYSQL_SYSVAR(mirrored_log_groups),
  MYSQL_SYSVAR(old_blocks_pct),
  MYSQL_SYSVAR(old_blocks_time),
  MYSQL_SYSVAR(open_files),
  MYSQL_SYSVAR(recovery_update_relay_log),
  MYSQL_SYSVAR(rollback_on_timeout),
  MYSQL_SYSVAR(stats_on_metadata),
  MYSQL_SYSVAR(stats_auto_update),
  MYSQL_SYSVAR(stats_update_need_lock),
  MYSQL_SYSVAR(use_sys_stats_table),
#ifdef UNIV_DEBUG
  MYSQL_SYSVAR(persistent_stats_root_page),
#endif
  MYSQL_SYSVAR(stats_sample_pages),
  MYSQL_SYSVAR(stats_modified_counter),
  MYSQL_SYSVAR(stats_traditional),
  MYSQL_SYSVAR(adaptive_hash_index),
  MYSQL_SYSVAR(adaptive_hash_index_partitions),
  MYSQL_SYSVAR(stats_method),
  MYSQL_SYSVAR(replication_delay),
  MYSQL_SYSVAR(status_file),
  MYSQL_SYSVAR(strict_mode),
  MYSQL_SYSVAR(support_xa),
  MYSQL_SYSVAR(sync_spin_loops),
  MYSQL_SYSVAR(spin_wait_delay),
  MYSQL_SYSVAR(table_locks),
  MYSQL_SYSVAR(thread_concurrency),
  MYSQL_SYSVAR(thread_concurrency_timer_based),
  MYSQL_SYSVAR(thread_sleep_delay),
  MYSQL_SYSVAR(autoinc_lock_mode),
  MYSQL_SYSVAR(show_verbose_locks),
  MYSQL_SYSVAR(show_locks_held),
  MYSQL_SYSVAR(version),
  MYSQL_SYSVAR(ibuf_max_size),
  MYSQL_SYSVAR(ibuf_active_contract),
  MYSQL_SYSVAR(ibuf_accel_rate),
  MYSQL_SYSVAR(checkpoint_age_target),
  MYSQL_SYSVAR(flush_neighbor_pages),
  MYSQL_SYSVAR(read_ahead),
  MYSQL_SYSVAR(adaptive_flushing_method),
  MYSQL_SYSVAR(import_table_from_xtrabackup),
  MYSQL_SYSVAR(dict_size_limit),
  MYSQL_SYSVAR(use_sys_malloc),
  MYSQL_SYSVAR(use_native_aio),
  MYSQL_SYSVAR(change_buffering),
  MYSQL_SYSVAR(track_changed_pages),
  MYSQL_SYSVAR(max_bitmap_file_size),
  MYSQL_SYSVAR(max_changed_pages),
#if defined UNIV_DEBUG || defined UNIV_IBUF_DEBUG
  MYSQL_SYSVAR(change_buffering_debug),
#endif /* UNIV_DEBUG || UNIV_IBUF_DEBUG */
  MYSQL_SYSVAR(random_read_ahead),
  MYSQL_SYSVAR(read_ahead_threshold),
  MYSQL_SYSVAR(io_capacity),
  MYSQL_SYSVAR(buffer_pool_restore_at_startup),
  MYSQL_SYSVAR(blocking_buffer_pool_restore),
  MYSQL_SYSVAR(purge_threads),
  MYSQL_SYSVAR(purge_batch_size),
  MYSQL_SYSVAR(rollback_segments),
#ifdef UNIV_DEBUG
  MYSQL_SYSVAR(trx_rseg_n_slots_debug),
  MYSQL_SYSVAR(limit_optimistic_insert_debug),
  MYSQL_SYSVAR(trx_purge_view_update_only_debug),
#endif /* UNIV_DEBUG */
  MYSQL_SYSVAR(corrupt_table_action),
  MYSQL_SYSVAR(lazy_drop_table),
  MYSQL_SYSVAR(fake_changes),
  MYSQL_SYSVAR(locking_fake_changes),
  MYSQL_SYSVAR(merge_sort_block_size),
  MYSQL_SYSVAR(print_all_deadlocks),
  MYSQL_SYSVAR(use_stacktrace),
#ifdef UNIV_DEBUG
  MYSQL_SYSVAR(log_checkpoint_now),
  MYSQL_SYSVAR(track_redo_log_now),
#endif /* UNIV_DEBUG */
  MYSQL_SYSVAR(simulate_comp_failures),
  NULL
};

maria_declare_plugin(xtradb)
{ /* InnoDB */
  MYSQL_STORAGE_ENGINE_PLUGIN,
  &innobase_storage_engine,
  innobase_hton_name,
  plugin_author,
  "Percona-XtraDB, Supports transactions, row-level locking, and foreign keys",
  PLUGIN_LICENSE_GPL,
  innobase_init, /* Plugin Init */
  NULL, /* Plugin Deinit */
  INNODB_VERSION_SHORT,
  innodb_status_variables_export,/* status variables             */
  innobase_system_variables, /* system variables */
  INNODB_VERSION_STR,         /* string version */
  MariaDB_PLUGIN_MATURITY_STABLE /* maturity */
},
i_s_innodb_rseg,
i_s_innodb_undo_logs,
i_s_innodb_trx,
i_s_innodb_locks,
i_s_innodb_lock_waits,
i_s_innodb_cmp,
i_s_innodb_cmp_reset,
i_s_innodb_cmpmem,
i_s_innodb_cmpmem_reset,
i_s_innodb_sys_tables,
i_s_innodb_sys_tablestats,
i_s_innodb_sys_indexes,
i_s_innodb_sys_columns,
i_s_innodb_sys_fields,
i_s_innodb_sys_foreign,
i_s_innodb_sys_foreign_cols,
i_s_innodb_sys_stats,
i_s_innodb_table_stats,
i_s_innodb_index_stats,
i_s_innodb_buffer_pool_pages,
i_s_innodb_buffer_pool_pages_index,
i_s_innodb_buffer_pool_pages_blob,
i_s_innodb_admin_command,
i_s_innodb_changed_pages,
i_s_innodb_buffer_page,
i_s_innodb_buffer_page_lru,
i_s_innodb_buffer_stats
maria_declare_plugin_end;

/** @brief Initialize the default value of innodb_commit_concurrency.

Once InnoDB is running, the innodb_commit_concurrency must not change
from zero to nonzero. (Bug #42101)

The initial default value is 0, and without this extra initialization,
SET GLOBAL innodb_commit_concurrency=DEFAULT would set the parameter
to 0, even if it was initially set to nonzero at the command line
or configuration file. */
static
void
innobase_commit_concurrency_init_default(void)
/*==========================================*/
{
	MYSQL_SYSVAR_NAME(commit_concurrency).def_val
		= innobase_commit_concurrency;
}

#ifdef UNIV_COMPILE_TEST_FUNCS

typedef struct innobase_convert_name_test_struct {
	char*		buf;
	ulint		buflen;
	const char*	id;
	ulint		idlen;
	void*		thd;
	ibool		file_id;

	const char*	expected;
} innobase_convert_name_test_t;

void
test_innobase_convert_name()
{
	char	buf[1024];
	ulint	i;

	innobase_convert_name_test_t test_input[] = {
		{buf, sizeof(buf), "abcd", 4, NULL, TRUE, "\"abcd\""},
		{buf, 7, "abcd", 4, NULL, TRUE, "\"abcd\""},
		{buf, 6, "abcd", 4, NULL, TRUE, "\"abcd\""},
		{buf, 5, "abcd", 4, NULL, TRUE, "\"abc\""},
		{buf, 4, "abcd", 4, NULL, TRUE, "\"ab\""},

		{buf, sizeof(buf), "ab@0060cd", 9, NULL, TRUE, "\"ab`cd\""},
		{buf, 9, "ab@0060cd", 9, NULL, TRUE, "\"ab`cd\""},
		{buf, 8, "ab@0060cd", 9, NULL, TRUE, "\"ab`cd\""},
		{buf, 7, "ab@0060cd", 9, NULL, TRUE, "\"ab`cd\""},
		{buf, 6, "ab@0060cd", 9, NULL, TRUE, "\"ab`c\""},
		{buf, 5, "ab@0060cd", 9, NULL, TRUE, "\"ab`\""},
		{buf, 4, "ab@0060cd", 9, NULL, TRUE, "\"ab\""},

		{buf, sizeof(buf), "ab\"cd", 5, NULL, TRUE,
			"\"#mysql50#ab\"\"cd\""},
		{buf, 17, "ab\"cd", 5, NULL, TRUE,
			"\"#mysql50#ab\"\"cd\""},
		{buf, 16, "ab\"cd", 5, NULL, TRUE,
			"\"#mysql50#ab\"\"c\""},
		{buf, 15, "ab\"cd", 5, NULL, TRUE,
			"\"#mysql50#ab\"\"\""},
		{buf, 14, "ab\"cd", 5, NULL, TRUE,
			"\"#mysql50#ab\""},
		{buf, 13, "ab\"cd", 5, NULL, TRUE,
			"\"#mysql50#ab\""},
		{buf, 12, "ab\"cd", 5, NULL, TRUE,
			"\"#mysql50#a\""},
		{buf, 11, "ab\"cd", 5, NULL, TRUE,
			"\"#mysql50#\""},
		{buf, 10, "ab\"cd", 5, NULL, TRUE,
			"\"#mysql50\""},

		{buf, sizeof(buf), "ab/cd", 5, NULL, TRUE, "\"ab\".\"cd\""},
		{buf, 9, "ab/cd", 5, NULL, TRUE, "\"ab\".\"cd\""},
		{buf, 8, "ab/cd", 5, NULL, TRUE, "\"ab\".\"c\""},
		{buf, 7, "ab/cd", 5, NULL, TRUE, "\"ab\".\"\""},
		{buf, 6, "ab/cd", 5, NULL, TRUE, "\"ab\"."},
		{buf, 5, "ab/cd", 5, NULL, TRUE, "\"ab\"."},
		{buf, 4, "ab/cd", 5, NULL, TRUE, "\"ab\""},
		{buf, 3, "ab/cd", 5, NULL, TRUE, "\"a\""},
		{buf, 2, "ab/cd", 5, NULL, TRUE, "\"\""},
		/* XXX probably "" is a better result in this case
		{buf, 1, "ab/cd", 5, NULL, TRUE, "."},
		*/
		{buf, 0, "ab/cd", 5, NULL, TRUE, ""},
	};

	for (i = 0; i < sizeof(test_input) / sizeof(test_input[0]); i++) {

		char*	end;
		ibool	ok = TRUE;
		size_t	res_len;

		fprintf(stderr, "TESTING %lu, %s, %lu, %s\n",
			test_input[i].buflen,
			test_input[i].id,
			test_input[i].idlen,
			test_input[i].expected);

		end = innobase_convert_name(
			test_input[i].buf,
			test_input[i].buflen,
			test_input[i].id,
			test_input[i].idlen,
			test_input[i].thd,
			test_input[i].file_id);

		res_len = (size_t) (end - test_input[i].buf);

		if (res_len != strlen(test_input[i].expected)) {

			fprintf(stderr, "unexpected len of the result: %u, "
				"expected: %u\n", (unsigned) res_len,
				(unsigned) strlen(test_input[i].expected));
			ok = FALSE;
		}

		if (memcmp(test_input[i].buf,
			   test_input[i].expected,
			   strlen(test_input[i].expected)) != 0
		    || !ok) {

			fprintf(stderr, "unexpected result: %.*s, "
				"expected: %s\n", (int) res_len,
				test_input[i].buf,
				test_input[i].expected);
			ok = FALSE;
		}

		if (ok) {
			fprintf(stderr, "OK: res: %.*s\n\n", (int) res_len,
				buf);
		} else {
			fprintf(stderr, "FAILED\n\n");
			return;
		}
	}
}
#endif /* UNIV_COMPILE_TEST_FUNCS */

/**********************************************************************
Converts an identifier from my_charset_filename to UTF-8 charset.
@return result string length, as returned by strconvert() */
extern "C"
uint
innobase_convert_to_filename_charset(
/*=================================*/
	char*		to,	/* out: converted identifier */
	const char*	from,	/* in: identifier to convert */
	ulint		len)	/* in: length of 'to', in bytes */
{
	uint		errors;
	CHARSET_INFO*	cs_to = &my_charset_filename;
	CHARSET_INFO*	cs_from = system_charset_info;

	return(strconvert(cs_from, from, cs_to, to, len, &errors));
}


/**********************************************************************
Issue a warning that the row is too big. */
extern "C" UNIV_INTERN
void
ib_warn_row_too_big(const dict_table_t*	table)
{
	/* If prefix is true then a 768-byte prefix is stored
	locally for BLOB fields. Refer to dict_table_get_format() */
	const bool	prefix = ((table->flags & DICT_TF_FORMAT_MASK)
				  >> DICT_TF_FORMAT_SHIFT) < UNIV_FORMAT_B;

	const ulint	free_space = page_get_free_space_of_empty(
		table->flags & DICT_TF_COMPACT) / 2;

	THD*	thd = current_thd;

	if (thd == NULL) {
		return;
	}

	push_warning_printf(
		thd, MYSQL_ERROR::WARN_LEVEL_WARN, HA_ERR_TO_BIG_ROW,
		"Row size too large (> %lu). Changing some columns to TEXT"
		" or BLOB %smay help. In current row format, BLOB prefix of"
		" %d bytes is stored inline.", free_space
		, prefix ? "or using ROW_FORMAT=DYNAMIC or"
		" ROW_FORMAT=COMPRESSED ": ""
		, prefix ? DICT_MAX_FIXED_COL_LEN : 0);
}


/****************************************************************************
 * DS-MRR implementation 
 ***************************************************************************/

/**
 * Multi Range Read interface, DS-MRR calls
 */

int ha_innobase::multi_range_read_init(RANGE_SEQ_IF *seq, void *seq_init_param,
                                       uint n_ranges, uint mode, 
                                       HANDLER_BUFFER *buf)
{
  return ds_mrr.dsmrr_init(this, seq, seq_init_param, n_ranges, mode, buf);
}

int ha_innobase::multi_range_read_next(range_id_t *range_info)
{
  return ds_mrr.dsmrr_next(range_info);
}

ha_rows ha_innobase::multi_range_read_info_const(uint keyno, RANGE_SEQ_IF *seq,
                                                 void *seq_init_param,  
                                                 uint n_ranges, uint *bufsz,
                                                 uint *flags, 
                                                 COST_VECT *cost)
{
  /* See comments in ha_myisam::multi_range_read_info_const */
  ds_mrr.init(this, table);

  if (prebuilt->select_lock_type != LOCK_NONE)
    *flags |= HA_MRR_USE_DEFAULT_IMPL;

  ha_rows res= ds_mrr.dsmrr_info_const(keyno, seq, seq_init_param, n_ranges,
                                       bufsz, flags, cost);
  return res;
}

ha_rows ha_innobase::multi_range_read_info(uint keyno, uint n_ranges, uint keys,
                                           uint key_parts, uint *bufsz, 
                                           uint *flags, COST_VECT *cost)
{
  ds_mrr.init(this, table);
  ha_rows res= ds_mrr.dsmrr_info(keyno, n_ranges, keys, key_parts, bufsz, 
                                 flags, cost);
  return res;
}

int ha_innobase::multi_range_read_explain_info(uint mrr_mode, char *str, size_t size)
{
  return ds_mrr.dsmrr_explain_info(mrr_mode, str, size);
}

/* 
  A helper function used only in index_cond_func_innodb
*/

bool ha_innobase::is_thd_killed()
{ 
  return thd_kill_level(user_thd);
}

/**
 * Index Condition Pushdown interface implementation
 */

/** Attempt to push down an index condition.
* @param[in] keyno	MySQL key number
* @param[in] idx_cond	Index condition to be checked
* @return idx_cond if pushed; NULL if not pushed
*/
UNIV_INTERN
class Item*
ha_innobase::idx_cond_push(
	uint		keyno,
	class Item*	idx_cond)
{
	DBUG_ENTER("ha_innobase::idx_cond_push");
	DBUG_ASSERT(keyno != MAX_KEY);
	DBUG_ASSERT(idx_cond != NULL);

	pushed_idx_cond = idx_cond;
	pushed_idx_cond_keyno = keyno;
	in_range_check_pushed_down = TRUE;
	/* Table handler will check the entire condition */
	DBUG_RETURN(NULL);
}

/********************************************************************//**
Helper function to push warnings from InnoDB internals to SQL-layer. */
extern "C" UNIV_INTERN
void
ib_push_warning(
	trx_t*		trx,	/*!< in: trx */
	ulint		error,	/*!< in: error code to push as warning */
	const char	*format,/*!< in: warning message */
	...)
{
	va_list args;
	THD *thd = (THD *)trx->mysql_thd;
	char *buf;
#define MAX_BUF_SIZE 4*1024

	va_start(args, format);
	buf = (char *)my_malloc(MAX_BUF_SIZE, MYF(MY_WME));
	vsprintf(buf,format, args);

	push_warning_printf(thd, MYSQL_ERROR::WARN_LEVEL_WARN,
		convert_error_code_to_mysql(error, 0, thd),
		buf);
	my_free(buf);
	va_end(args);
}

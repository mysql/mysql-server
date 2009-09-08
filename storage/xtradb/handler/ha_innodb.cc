/*****************************************************************************

Copyright (c) 2000, 2009, MySQL AB & Innobase Oy. All Rights Reserved.
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

/* TODO list for the InnoDB handler in 5.0:
  - Remove the flag trx->active_trans and look at trx->conc_state
  - fix savepoint functions to use savepoint storage area
  - Find out what kind of problems the OS X case-insensitivity causes to
    table and database names; should we 'normalize' the names like we do
    in Windows?
*/

#ifdef USE_PRAGMA_IMPLEMENTATION
#pragma implementation				// gcc: Class implementation
#endif

#define MYSQL_SERVER

#include <mysql_priv.h>
#ifdef MYSQL_SERVER
#include <log_event.h>
#endif /* MYSQL_SERVER */

#include <m_ctype.h>
#include <mysys_err.h>
#include <mysql/plugin.h>

/* Include necessary InnoDB headers */
extern "C" {
#include "../storage/xtradb/include/univ.i"
#include "../storage/xtradb/include/btr0sea.h"
#include "../storage/xtradb/include/os0file.h"
#include "../storage/xtradb/include/os0thread.h"
#include "../storage/xtradb/include/srv0start.h"
#include "../storage/xtradb/include/srv0srv.h"
#include "../storage/xtradb/include/trx0roll.h"
#include "../storage/xtradb/include/trx0trx.h"
#include "../storage/xtradb/include/trx0sys.h"
#include "../storage/xtradb/include/mtr0mtr.h"
#include "../storage/xtradb/include/row0ins.h"
#include "../storage/xtradb/include/row0mysql.h"
#include "../storage/xtradb/include/row0sel.h"
#include "../storage/xtradb/include/row0upd.h"
#include "../storage/xtradb/include/log0log.h"
#include "../storage/xtradb/include/lock0lock.h"
#include "../storage/xtradb/include/dict0crea.h"
#include "../storage/xtradb/include/btr0cur.h"
#include "../storage/xtradb/include/btr0btr.h"
#include "../storage/xtradb/include/fsp0fsp.h"
#include "../storage/xtradb/include/sync0sync.h"
#include "../storage/xtradb/include/fil0fil.h"
#include "../storage/xtradb/include/trx0xa.h"
#include "../storage/xtradb/include/row0merge.h"
#include "../storage/xtradb/include/thr0loc.h"
#include "../storage/xtradb/include/dict0boot.h"
#include "../storage/xtradb/include/ha_prototypes.h"
#include "../storage/xtradb/include/ut0mem.h"
#include "../storage/xtradb/include/ibuf0ibuf.h"
}

#include "ha_innodb.h"
#include "i_s.h"
#include "handler0vars.h"

#ifdef MYSQL_SERVER
// Defined in trx0sys.c
extern char		trx_sys_mysql_master_log_name[];
extern ib_int64_t	trx_sys_mysql_master_log_pos;
extern char		trx_sys_mysql_relay_log_name[];
extern ib_int64_t	trx_sys_mysql_relay_log_pos;
#endif /* MYSQL_SERVER */

#ifndef MYSQL_SERVER
/* This is needed because of Bug #3596.  Let us hope that pthread_mutex_t
is defined the same in both builds: the MySQL server and the InnoDB plugin. */
extern pthread_mutex_t LOCK_thread_count;

#if MYSQL_VERSION_ID < 50124
/* this is defined in mysql_priv.h inside #ifdef MYSQL_SERVER
but we need it here */
bool check_global_access(THD *thd, ulong want_access);
#endif /* MYSQL_VERSION_ID < 50124 */
#endif /* MYSQL_SERVER */

/** to protect innobase_open_files */
static pthread_mutex_t innobase_share_mutex;
/** to force correct commit order in binlog */
static pthread_mutex_t prepare_commit_mutex;
static ulong commit_threads = 0;
static pthread_mutex_t commit_threads_m;
static pthread_cond_t commit_cond;
static pthread_mutex_t commit_cond_m;
static bool innodb_inited = 0;

#define INSIDE_HA_INNOBASE_CC

/* In the Windows plugin, the return value of current_thd is
undefined.  Map it to NULL. */
#if defined MYSQL_DYNAMIC_PLUGIN && defined __WIN__
# undef current_thd
# define current_thd NULL
# define EQ_CURRENT_THD(thd) TRUE
#else /* MYSQL_DYNAMIC_PLUGIN && __WIN__ */
# define EQ_CURRENT_THD(thd) ((thd) == current_thd)
#endif /* MYSQL_DYNAMIC_PLUGIN && __WIN__ */

#ifdef MYSQL_DYNAMIC_PLUGIN
/* These must be weak global variables in the dynamic plugin. */
struct handlerton* innodb_hton_ptr;
#ifdef __WIN__
struct st_mysql_plugin*	builtin_innobase_plugin_ptr;
#else
int builtin_innobase_plugin;
#endif /* __WIN__ */
/********************************************************************
Copy InnoDB system variables from the static InnoDB to the dynamic
plugin. */
static
bool
innodb_plugin_init(void);
/*====================*/
		/* out: TRUE if the dynamic InnoDB plugin should start */
#else /* MYSQL_DYNAMIC_PLUGIN */
/* This must be a global variable in the statically linked InnoDB. */
struct handlerton* innodb_hton_ptr = NULL;
#endif /* MYSQL_DYNAMIC_PLUGIN */

static const long AUTOINC_OLD_STYLE_LOCKING = 0;
static const long AUTOINC_NEW_STYLE_LOCKING = 1;
static const long AUTOINC_NO_LOCKING = 2;

static long innobase_mirrored_log_groups, innobase_log_files_in_group,
	innobase_log_buffer_size,
	innobase_additional_mem_pool_size, innobase_file_io_threads,
	innobase_force_recovery, innobase_open_files,
	innobase_autoinc_lock_mode;
static unsigned long innobase_commit_concurrency = 0;

static unsigned long innobase_read_io_threads, innobase_write_io_threads;
static my_bool innobase_thread_concurrency_timer_based;
static long long innobase_buffer_pool_size, innobase_log_file_size;

/* The default values for the following char* start-up parameters
are determined in innobase_init below: */

static char*	innobase_data_home_dir			= NULL;
static char*	innobase_data_file_path			= NULL;
static char*	innobase_log_group_home_dir		= NULL;
static char*	innobase_file_format_name		= NULL;
static char*	innobase_change_buffering		= NULL;

/* Note: This variable can be set to on/off and any of the supported
file formats in the configuration file, but can only be set to any
of the supported file formats during runtime. */
static char*	innobase_file_format_check		= NULL;

/* The following has a misleading name: starting from 4.0.5, this also
affects Windows: */
static char*	innobase_unix_file_flush_method		= NULL;

/* Below we have boolean-valued start-up parameters, and their default
values */

static ulong	innobase_fast_shutdown			= 1;
#ifdef UNIV_LOG_ARCHIVE
static my_bool	innobase_log_archive			= FALSE;
static char*	innobase_log_arch_dir			= NULL;
#endif /* UNIV_LOG_ARCHIVE */
static my_bool	innobase_use_doublewrite		= TRUE;
static my_bool	innobase_use_checksums			= TRUE;
static my_bool	innobase_extra_undoslots		= FALSE;
static my_bool	innobase_fast_recovery			= FALSE;
static my_bool	innobase_locks_unsafe_for_binlog	= FALSE;
static my_bool	innobase_overwrite_relay_log_info	= FALSE;
static my_bool	innobase_rollback_on_timeout		= FALSE;
static my_bool	innobase_create_status_file		= FALSE;
static my_bool	innobase_stats_on_metadata		= TRUE;

static char*	internal_innobase_data_file_path	= NULL;

static char*	innodb_version_str = (char*) INNODB_VERSION_STR;

/* The following counter is used to convey information to InnoDB
about server activity: in selects it is not sensible to call
srv_active_wake_master_thread after each fetch or search, we only do
it every INNOBASE_WAKE_INTERVAL'th step. */

#define INNOBASE_WAKE_INTERVAL	32
static ulong	innobase_active_counter	= 0;

static hash_table_t*	innobase_open_tables;

#ifdef __NETWARE__	/* some special cleanup for NetWare */
bool nw_panic = FALSE;
#endif

/** Allowed values of innodb_change_buffering */
static const char* innobase_change_buffering_values[IBUF_USE_COUNT] = {
	"none",		/* IBUF_USE_NONE */
	"inserts"	/* IBUF_USE_INSERT */
};

static INNOBASE_SHARE *get_share(const char *table_name);
static void free_share(INNOBASE_SHARE *share);
static int innobase_close_connection(handlerton *hton, THD* thd);
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

/****************************************************************
Validate the file format name and return its corresponding id. */
static
uint
innobase_file_format_name_lookup(
/*=============================*/
						/* out: valid file format id */
	const char*	format_name);		/* in: pointer to file format
						name */
/****************************************************************
Validate the file format check config parameters, as a side effect it
sets the srv_check_file_format_at_startup variable. */
static
bool
innobase_file_format_check_on_off(
/*==============================*/
						/* out: true if one of 
						"on" or "off" */
	const char*	format_check);		/* in: parameter value */
/****************************************************************
Validate the file format check config parameters, as a side effect it
sets the srv_check_file_format_at_startup variable. */
static
bool
innobase_file_format_check_validate(
/*================================*/
						/* out: true if valid
						config value */
	const char*	format_check);		/* in: parameter value */
/********************************************************************
Return alter table flags supported in an InnoDB database. */
static
uint
innobase_alter_table_flags(
/*=======================*/
	uint	flags);

static const char innobase_hton_name[]= "InnoDB";

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

/*****************************************************************
Check for a valid value of innobase_commit_concurrency. */
static
int
innobase_commit_concurrency_validate(
/*=================================*/
						/* out: 0 for valid
						innodb_commit_concurrency */
	THD*				thd,	/* in: thread handle */
	struct st_mysql_sys_var*	var,	/* in: pointer to system
						variable */
	void*				save,	/* out: immediate result
						for update function */
	struct st_mysql_value*		value)	/* in: incoming string */
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


static handler *innobase_create_handler(handlerton *hton,
                                        TABLE_SHARE *table, 
                                        MEM_ROOT *mem_root)
{
  return new (mem_root) ha_innobase(hton, table);
}

/***********************************************************************
This function is used to prepare X/Open XA distributed transaction   */
static
int
innobase_xa_prepare(
/*================*/
			/* out: 0 or error number */
	handlerton* hton,
	THD*	thd,	/* in: handle to the MySQL thread of the user
			whose XA transaction should be prepared */
	bool	all);	/* in: TRUE - commit transaction
			FALSE - the current SQL statement ended */
/***********************************************************************
This function is used to recover X/Open XA distributed transactions   */
static
int
innobase_xa_recover(
/*================*/
				/* out: number of prepared transactions
				stored in xid_list */
	handlerton* hton,
	XID*	xid_list,	/* in/out: prepared transactions */
	uint	len);		/* in: number of slots in xid_list */
/***********************************************************************
This function is used to commit one X/Open XA distributed transaction
which is in the prepared state */
static
int
innobase_commit_by_xid(
/*===================*/
			/* out: 0 or error number */
	handlerton* hton,
	XID*	xid);	/* in: X/Open XA transaction identification */
/***********************************************************************
This function is used to rollback one X/Open XA distributed transaction
which is in the prepared state */
static
int
innobase_rollback_by_xid(
/*=====================*/
			/* out: 0 or error number */
	handlerton* hton,
	XID	*xid);	/* in: X/Open XA transaction identification */
/***********************************************************************
Create a consistent view for a cursor based on current transaction
which is created if the corresponding MySQL thread still lacks one.
This consistent view is then used inside of MySQL when accessing records
using a cursor. */
static
void*
innobase_create_cursor_view(
/*========================*/
				/* out: pointer to cursor view or NULL */
	handlerton*	hton,	/* in: innobase hton */
	THD*		thd);	/* in: user thread handle */
/***********************************************************************
Set the given consistent cursor view to a transaction which is created
if the corresponding MySQL thread still lacks one. If the given
consistent cursor view is NULL global read view of a transaction is
restored to a transaction read view. */
static
void
innobase_set_cursor_view(
/*=====================*/
	handlerton* hton,
	THD*	thd,	/* in: user thread handle */
	void*	curview);/* in: Consistent cursor view to be set */
/***********************************************************************
Close the given consistent cursor view of a transaction and restore
global read view to a transaction read view. Transaction is created if the
corresponding MySQL thread still lacks one. */
static
void
innobase_close_cursor_view(
/*=======================*/
	handlerton* hton,
	THD*	thd,	/* in: user thread handle */
	void*	curview);/* in: Consistent read view to be closed */
/*********************************************************************
Removes all tables in the named database inside InnoDB. */
static
void
innobase_drop_database(
/*===================*/
			/* out: error number */
	handlerton* hton, /* in: handlerton of Innodb */
	char*	path);	/* in: database path; inside InnoDB the name
			of the last directory in the path is used as
			the database name: for example, in 'mysql/data/test'
			the database name is 'test' */
/***********************************************************************
Closes an InnoDB database. */
static
int
innobase_end(handlerton *hton, ha_panic_function type);

/*********************************************************************
Creates an InnoDB transaction struct for the thd if it does not yet have one.
Starts a new InnoDB transaction if a transaction is not yet started. And
assigns a new snapshot for a consistent read if the transaction does not yet
have one. */
static
int
innobase_start_trx_and_assign_read_view(
/*====================================*/
			/* out: 0 */
	handlerton* hton, /* in: Innodb handlerton */ 
	THD*	thd);	/* in: MySQL thread handle of the user for whom
			the transaction should be committed */
/********************************************************************
Flushes InnoDB logs to disk and makes a checkpoint. Really, a commit flushes
the logs, and the name of this function should be innobase_checkpoint. */
static
bool
innobase_flush_logs(
/*================*/
				/* out: TRUE if error */
	handlerton*	hton);	/* in: InnoDB handlerton */

/****************************************************************************
Implements the SHOW INNODB STATUS command. Sends the output of the InnoDB
Monitor to the client. */
static
bool
innodb_show_status(
/*===============*/
	handlerton*	hton,	/* in: the innodb handlerton */
	THD*	thd,	/* in: the MySQL query thread of the caller */
	stat_print_fn *stat_print);
static
bool innobase_show_status(handlerton *hton, THD* thd, 
                          stat_print_fn* stat_print,
                          enum ha_stat_type stat_type);

/*********************************************************************
Commits a transaction in an InnoDB database. */
static
void
innobase_commit_low(
/*================*/
	trx_t*	trx);	/* in: transaction handle */

static SHOW_VAR innodb_status_variables[]= {
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
  {"buffer_pool_pages_misc",
  (char*) &export_vars.innodb_buffer_pool_pages_misc,	  SHOW_LONG},
  {"buffer_pool_pages_total",
  (char*) &export_vars.innodb_buffer_pool_pages_total,	  SHOW_LONG},
  {"buffer_pool_read_ahead_rnd",
  (char*) &export_vars.innodb_buffer_pool_read_ahead_rnd, SHOW_LONG},
  {"buffer_pool_read_ahead_seq",
  (char*) &export_vars.innodb_buffer_pool_read_ahead_seq, SHOW_LONG},
  {"buffer_pool_read_requests",
  (char*) &export_vars.innodb_buffer_pool_read_requests,  SHOW_LONG},
  {"buffer_pool_reads",
  (char*) &export_vars.innodb_buffer_pool_reads,	  SHOW_LONG},
  {"buffer_pool_wait_free",
  (char*) &export_vars.innodb_buffer_pool_wait_free,	  SHOW_LONG},
  {"buffer_pool_write_requests",
  (char*) &export_vars.innodb_buffer_pool_write_requests, SHOW_LONG},
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
  {"dict_tables",
  (char*) &export_vars.innodb_dict_tables,		  SHOW_LONG},
  {"have_atomic_builtins",
  (char*) &export_vars.innodb_have_atomic_builtins,	  SHOW_BOOL},
  {"log_waits",
  (char*) &export_vars.innodb_log_waits,		  SHOW_LONG},
  {"log_write_requests",
  (char*) &export_vars.innodb_log_write_requests,	  SHOW_LONG},
  {"log_writes",
  (char*) &export_vars.innodb_log_writes,		  SHOW_LONG},
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
  {NullS, NullS, SHOW_LONG}
};

/* General functions */

/**********************************************************************
Returns true if the thread is the replication thread on the slave
server. Used in srv_conc_enter_innodb() to determine if the thread
should be allowed to enter InnoDB - the replication thread is treated
differently than other threads. Also used in
srv_conc_force_exit_innodb(). */
extern "C" UNIV_INTERN
ibool
thd_is_replication_slave_thread(
/*============================*/
			/* out: true if thd is the replication thread */
	void*	thd)	/* in: thread handle (THD*) */
{
	return((ibool) thd_slave_thread((THD*) thd));
}

/**********************************************************************
Save some CPU by testing the value of srv_thread_concurrency in inline
functions. */
static inline
void
innodb_srv_conc_enter_innodb(
/*=========================*/
	trx_t*	trx)	/* in: transaction handle */
{
	if (UNIV_LIKELY(!srv_thread_concurrency)) {

		return;
	}

	srv_conc_enter_innodb(trx);
}

/**********************************************************************
Save some CPU by testing the value of srv_thread_concurrency in inline
functions. */
static inline
void
innodb_srv_conc_exit_innodb(
/*========================*/
	trx_t*	trx)	/* in: transaction handle */
{
	if (UNIV_LIKELY(!trx->declared_to_be_inside_innodb)) {

		return;
	}

	srv_conc_exit_innodb(trx);
}

/**********************************************************************
Releases possible search latch and InnoDB thread FIFO ticket. These should
be released at each SQL statement end, and also when mysqld passes the
control to the client. It does no harm to release these also in the middle
of an SQL statement. */
static inline
void
innobase_release_stat_resources(
/*============================*/
	trx_t*	trx)	/* in: transaction object */
{
	if (trx->has_search_latch) {
		trx_search_latch_release_if_reserved(trx);
	}

	if (trx->declared_to_be_inside_innodb) {
		/* Release our possible ticket in the FIFO */

		srv_conc_force_exit_innodb(trx);
	}
}

/**********************************************************************
Returns true if the transaction this thread is processing has edited
non-transactional tables. Used by the deadlock detector when deciding
which transaction to rollback in case of a deadlock - we try to avoid
rolling back transactions that have edited non-transactional tables. */
extern "C" UNIV_INTERN
ibool
thd_has_edited_nontrans_tables(
/*===========================*/
			/* out: true if non-transactional tables have
			been edited */
	void*	thd)	/* in: thread handle (THD*) */
{
	return((ibool) thd_non_transactional_update((THD*) thd));
}

/**********************************************************************
Returns true if the thread is executing a SELECT statement. */
extern "C" UNIV_INTERN
ibool
thd_is_select(
/*==========*/
				/* out: true if thd is executing SELECT */
	const void*	thd)	/* in: thread handle (THD*) */
{
	return(thd_sql_command((const THD*) thd) == SQLCOM_SELECT);
}

/**********************************************************************
Returns true if the thread supports XA,
global value of innodb_supports_xa if thd is NULL. */
extern "C" UNIV_INTERN
ibool
thd_supports_xa(
/*============*/
			/* out: true if thd has XA support */
	void*	thd)	/* in: thread handle (THD*), or NULL to query
			the global innodb_supports_xa */
{
	return(THDVAR((THD*) thd, support_xa));
}

/**********************************************************************
Returns the lock wait timeout for the current connection. */
extern "C" UNIV_INTERN
ulong
thd_lock_wait_timeout(
/*==================*/
			/* out: the lock wait timeout, in seconds */
	void*	thd)	/* in: thread handle (THD*), or NULL to query
			the global innodb_lock_wait_timeout */
{
	/* According to <mysql/plugin.h>, passing thd == NULL
	returns the global value of the session variable. */
	return(THDVAR((THD*) thd, lock_wait_timeout));
}

/************************************************************************
Obtain the InnoDB transaction of a MySQL thread. */
static inline
trx_t*&
thd_to_trx(
/*=======*/
			/* out: reference to transaction pointer */
	THD*	thd)	/* in: MySQL thread */
{
	return(*(trx_t**) thd_ha_data(thd, innodb_hton_ptr));
}

/************************************************************************
Call this function when mysqld passes control to the client. That is to
avoid deadlocks on the adaptive hash S-latch possibly held by thd. For more
documentation, see handler.cc. */
static
int
innobase_release_temporary_latches(
/*===============================*/
				/* out: 0 */
	handlerton*	hton,	/* in: handlerton */
	THD*		thd)	/* in: MySQL thread */
{
	trx_t*	trx;

	DBUG_ASSERT(hton == innodb_hton_ptr);

	if (!innodb_inited) {

		return(0);
	}

	trx = thd_to_trx(thd);

	if (trx) {
		innobase_release_stat_resources(trx);
	}
	return(0);
}

/************************************************************************
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

/************************************************************************
Converts an InnoDB error code to a MySQL error code and also tells to MySQL
about a possible transaction rollback inside InnoDB caused by a lock wait
timeout or a deadlock. */
extern "C" UNIV_INTERN
int
convert_error_code_to_mysql(
/*========================*/
			/* out: MySQL error code */
	int	error,	/* in: InnoDB error code */
	ulint	flags,	/* in: InnoDB table flags, or 0 */
	THD*	thd)	/* in: user thread handle or NULL */
{
	switch (error) {
	case DB_SUCCESS:
		return(0);

	case DB_ERROR:
	default:
		return(-1); /* unspecified error */

	case DB_DUPLICATE_KEY:
		return(HA_ERR_FOUND_DUPP_KEY);

	case DB_FOREIGN_DUPLICATE_KEY:
		return(HA_ERR_FOREIGN_DUPLICATE_KEY);

	case DB_RECORD_NOT_FOUND:
		return(HA_ERR_NO_ACTIVE_RECORD);

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
		return(HA_ERR_CANNOT_ADD_FOREIGN);

	case DB_CANNOT_DROP_CONSTRAINT:

		return(HA_ERR_ROW_IS_REFERENCED); /* TODO: This is a bit
						misleading, a new MySQL error
						code should be introduced */

	case DB_COL_APPEARS_TWICE_IN_INDEX:
	case DB_CORRUPTION:
		return(HA_ERR_CRASHED);

	case DB_OUT_OF_FILE_SPACE:
		return(HA_ERR_RECORD_FILE_FULL);

	case DB_TABLE_IS_BEING_USED:
		return(HA_ERR_WRONG_COMMAND);

	case DB_TABLE_NOT_FOUND:
		return(HA_ERR_NO_SUCH_TABLE);

	case DB_TOO_BIG_RECORD:
		my_error(ER_TOO_BIG_ROWSIZE, MYF(0),
			 page_get_free_space_of_empty(flags
						      & DICT_TF_COMPACT) / 2);
		return(HA_ERR_TO_BIG_ROW);

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
		/* Once MySQL add the appropriate code to errmsg.txt then
		we can get rid of this #ifdef. NOTE: The code checked by
		the #ifdef is the suggested name for the error condition
		and the actual error code name could very well be different.
		This will require some monitoring, ie. the status
		of this request on our part.*/
#ifdef ER_TOO_MANY_CONCURRENT_TRXS
		return(ER_TOO_MANY_CONCURRENT_TRXS);
#else
		return(HA_ERR_RECORD_FILE_FULL);
#endif
	case DB_UNSUPPORTED:
		return(HA_ERR_UNSUPPORTED);
	}
}

/*****************************************************************
If you want to print a thd that is not associated with the current thread,
you must call this function before reserving the InnoDB kernel_mutex, to
protect MySQL from setting thd->query NULL. If you print a thd of the current
thread, we know that MySQL cannot modify thd->query, and it is not necessary
to call this. Call innobase_mysql_end_print_arbitrary_thd() after you release
the kernel_mutex. */
extern "C" UNIV_INTERN
void
innobase_mysql_prepare_print_arbitrary_thd(void)
/*============================================*/
{
	ut_ad(!mutex_own(&kernel_mutex));
	VOID(pthread_mutex_lock(&LOCK_thread_count));
}

/*****************************************************************
Releases the mutex reserved by innobase_mysql_prepare_print_arbitrary_thd().
In the InnoDB latching order, the mutex sits right above the
kernel_mutex.  In debug builds, we assert that the kernel_mutex is
released before this function is invoked. */
extern "C" UNIV_INTERN
void
innobase_mysql_end_print_arbitrary_thd(void)
/*========================================*/
{
	ut_ad(!mutex_own(&kernel_mutex));
	VOID(pthread_mutex_unlock(&LOCK_thread_count));
}

/*****************************************************************
Prints info of a THD object (== user session thread) to the given file. */
extern "C" UNIV_INTERN
void
innobase_mysql_print_thd(
/*=====================*/
	FILE*	f,		/* in: output stream */
	void*	thd,		/* in: pointer to a MySQL THD object */
	uint	max_query_len)	/* in: max query length to print, or 0 to
				   use the default max length */
{
	char	buffer[1024];

	fputs(thd_security_context((THD*) thd, buffer, sizeof buffer,
				   max_query_len), f);
	putc('\n', f);
}

/**********************************************************************
Get the variable length bounds of the given character set. */
extern "C" UNIV_INTERN
void
innobase_get_cset_width(
/*====================*/
	ulint	cset,		/* in: MySQL charset-collation code */
	ulint*	mbminlen,	/* out: minimum length of a char (in bytes) */
	ulint*	mbmaxlen)	/* out: maximum length of a char (in bytes) */
{
	CHARSET_INFO*	cs;
	ut_ad(cset < 256);
	ut_ad(mbminlen);
	ut_ad(mbmaxlen);

	cs = all_charsets[cset];
	if (cs) {
		*mbminlen = cs->mbminlen;
		*mbmaxlen = cs->mbmaxlen;
	} else {
		ut_a(cset == 0);
		*mbminlen = *mbmaxlen = 0;
	}
}

/**********************************************************************
Converts an identifier to a table name. */
extern "C" UNIV_INTERN
void
innobase_convert_from_table_id(
/*===========================*/
	struct charset_info_st*	cs,	/* in: the 'from' character set */
	char*			to,	/* out: converted identifier */
	const char*		from,	/* in: identifier to convert */
	ulint			len)	/* in: length of 'to', in bytes */
{
	uint	errors;

	strconvert(cs, from, &my_charset_filename, to, (uint) len, &errors);
}

/**********************************************************************
Converts an identifier to UTF-8. */
extern "C" UNIV_INTERN
void
innobase_convert_from_id(
/*=====================*/
	struct charset_info_st*	cs,	/* in: the 'from' character set */
	char*			to,	/* out: converted identifier */
	const char*		from,	/* in: identifier to convert */
	ulint			len)	/* in: length of 'to', in bytes */
{
	uint	errors;

	strconvert(cs, from, system_charset_info, to, (uint) len, &errors);
}

/**********************************************************************
Compares NUL-terminated UTF-8 strings case insensitively. */
extern "C" UNIV_INTERN
int
innobase_strcasecmp(
/*================*/
				/* out: 0 if a=b, <0 if a<b, >1 if a>b */
	const char*	a,	/* in: first string to compare */
	const char*	b)	/* in: second string to compare */
{
	return(my_strcasecmp(system_charset_info, a, b));
}

/**********************************************************************
Makes all characters in a NUL-terminated UTF-8 string lower case. */
extern "C" UNIV_INTERN
void
innobase_casedn_str(
/*================*/
	char*	a)	/* in/out: string to put in lower case */
{
	my_casedn_str(system_charset_info, a);
}

/**************************************************************************
Determines the connection character set. */
extern "C" UNIV_INTERN
struct charset_info_st*
innobase_get_charset(
/*=================*/
				/* out: connection character set */
	void*	mysql_thd)	/* in: MySQL thread handle */
{
	return(thd_charset((THD*) mysql_thd));
}

#if defined (__WIN__) && defined (MYSQL_DYNAMIC_PLUGIN)
/***********************************************************************
Map an OS error to an errno value. The OS error number is stored in
_doserrno and the mapped value is stored in errno) */
extern "C"
void __cdecl
_dosmaperr(
	unsigned long);	/* in: OS error value */

/*************************************************************************
Creates a temporary file. */
extern "C" UNIV_INTERN
int
innobase_mysql_tmpfile(void)
/*========================*/
			/* out: temporary file descriptor, or < 0 on error */
{
	int	fd;				/* handle of opened file */
	HANDLE	osfh;				/* OS handle of opened file */
	char*	tmpdir;				/* point to the directory
						where to create file */
	TCHAR	path_buf[MAX_PATH - 14];	/* buffer for tmp file path.
						The length cannot be longer
						than MAX_PATH - 14, or
						GetTempFileName will fail. */
	char	filename[MAX_PATH];		/* name of the tmpfile */
	DWORD	fileaccess = GENERIC_READ	/* OS file access */
			     | GENERIC_WRITE
			     | DELETE;
	DWORD	fileshare = FILE_SHARE_READ	/* OS file sharing mode */
			    | FILE_SHARE_WRITE
			    | FILE_SHARE_DELETE;
	DWORD	filecreate = CREATE_ALWAYS;	/* OS method of open/create */
	DWORD	fileattrib =			/* OS file attribute flags */
			     FILE_ATTRIBUTE_NORMAL
			     | FILE_FLAG_DELETE_ON_CLOSE
			     | FILE_ATTRIBUTE_TEMPORARY
			     | FILE_FLAG_SEQUENTIAL_SCAN;

	DBUG_ENTER("innobase_mysql_tmpfile");

	tmpdir = my_tmpdir(&mysql_tmpdir_list);

	/* The tmpdir parameter can not be NULL for GetTempFileName. */
	if (!tmpdir) {
		uint	ret;

		/* Use GetTempPath to determine path for temporary files. */
		ret = GetTempPath(sizeof(path_buf), path_buf);
		if (ret > sizeof(path_buf) || (ret == 0)) {

			_dosmaperr(GetLastError());	/* map error */
			DBUG_RETURN(-1);
		}

		tmpdir = path_buf;
	}

	/* Use GetTempFileName to generate a unique filename. */
	if (!GetTempFileName(tmpdir, "ib", 0, filename)) {

		_dosmaperr(GetLastError());	/* map error */
		DBUG_RETURN(-1);
	}

	DBUG_PRINT("info", ("filename: %s", filename));

	/* Open/Create the file. */
	osfh = CreateFile(filename, fileaccess, fileshare, NULL,
			  filecreate, fileattrib, NULL);
	if (osfh == INVALID_HANDLE_VALUE) {

		/* open/create file failed! */
		_dosmaperr(GetLastError());	/* map error */
		DBUG_RETURN(-1);
	}

	do {
		/* Associates a CRT file descriptor with the OS file handle. */
		fd = _open_osfhandle((intptr_t) osfh, 0);
	} while (fd == -1 && errno == EINTR);

	if (fd == -1) {
		/* Open failed, close the file handle. */

		_dosmaperr(GetLastError());	/* map error */
		CloseHandle(osfh);		/* no need to check if
						CloseHandle fails */
	}

	DBUG_RETURN(fd);
}
#else
/*************************************************************************
Creates a temporary file. */
extern "C" UNIV_INTERN
int
innobase_mysql_tmpfile(void)
/*========================*/
			/* out: temporary file descriptor, or < 0 on error */
{
	int	fd2 = -1;
	File	fd = mysql_tmpfile("ib");
	if (fd >= 0) {
		/* Copy the file descriptor, so that the additional resources
		allocated by create_temp_file() can be freed by invoking
		my_close().

		Because the file descriptor returned by this function
		will be passed to fdopen(), it will be closed by invoking
		fclose(), which in turn will invoke close() instead of
		my_close(). */
		fd2 = dup(fd);
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
#endif /* defined (__WIN__) && defined (MYSQL_DYNAMIC_PLUGIN) */

/*************************************************************************
Wrapper around MySQL's copy_and_convert function, see it for
documentation. */
extern "C" UNIV_INTERN
ulint
innobase_convert_string(
/*====================*/
	void*		to,
	ulint		to_length,
	CHARSET_INFO*	to_cs,
	const void*	from,
	ulint		from_length,
	CHARSET_INFO*	from_cs,
	uint*		errors)
{
  return(copy_and_convert((char*)to, (uint32) to_length, to_cs,
                          (const char*)from, (uint32) from_length, from_cs,
                          errors));
}

/***********************************************************************
Formats the raw data in "data" (in InnoDB on-disk format) that is of
type DATA_(CHAR|VARCHAR|MYSQL|VARMYSQL) using "charset_coll" and writes
the result to "buf". The result is converted to "system_charset_info".
Not more than "buf_size" bytes are written to "buf".
The result is always '\0'-terminated (provided buf_size > 0) and the
number of bytes that were written to "buf" is returned (including the
terminating '\0'). */
extern "C" UNIV_INTERN
ulint
innobase_raw_format(
/*================*/
					/* out: number of bytes
					that were written */
	const char*	data,		/* in: raw data */
	ulint		data_len,	/* in: raw data length
					in bytes */
	ulint		charset_coll,	/* in: charset collation */
	char*		buf,		/* out: output buffer */
	ulint		buf_size)	/* in: output buffer size
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

/*************************************************************************
Compute the next autoinc value.

For MySQL replication the autoincrement values can be partitioned among
the nodes. The offset is the start or origin of the autoincrement value
for a particular node. For n nodes the increment will be n and the offset
will be in the interval [1, n]. The formula tries to allocate the next
value for a particular node.

Note: This function is also called with increment set to the number of
values we want to reserve for multi-value inserts e.g.,

	INSERT INTO T VALUES(), (), ();

innobase_next_autoinc() will be called with increment set to
n * 3 where autoinc_lock_mode != TRADITIONAL because we want
to reserve 3 values for the multi-value INSERT above. */
static
ulonglong
innobase_next_autoinc(
/*==================*/
					/* out: the next value */
	ulonglong	current,	/* in: Current value */
	ulonglong	increment,	/* in: increment current by */
	ulonglong	offset,		/* in: AUTOINC offset */
	ulonglong	max_value)	/* in: max value for type */
{
	ulonglong	next_value;

	/* Should never be 0. */
	ut_a(increment > 0);

	/* According to MySQL documentation, if the offset is greater than
	the increment then the offset is ignored. */
	if (offset > increment) {
		offset = 0;
	}

	if (max_value <= current) {
		next_value = max_value;
	} else if (offset <= 1) {
		/* Offset 0 and 1 are the same, because there must be at
		least one node in the system. */
		if (max_value - current <= increment) {
			next_value = max_value;
		} else {
			next_value = current + increment;
		}
	} else if (max_value > current) {
		if (current > offset) {
			next_value = ((current - offset) / increment) + 1;
		} else {
			next_value = ((offset - current) / increment) + 1;
		}

		ut_a(increment > 0);
		ut_a(next_value > 0);

		/* Check for multiplication overflow. */
		if (increment > (max_value / next_value)) {

			next_value = max_value;
		} else {
			next_value *= increment;

			ut_a(max_value >= next_value);

			/* Check for overflow. */
			if (max_value - next_value <= offset) {
				next_value = max_value;
			} else {
				next_value += offset;
			}
		}
	} else {
		next_value = max_value;
	}

	ut_a(next_value <= max_value);

	return(next_value);
}

/*************************************************************************
Initializes some fields in an InnoDB transaction object. */
static
void
innobase_trx_init(
/*==============*/
	THD*	thd,	/* in: user thread handle */
	trx_t*	trx)	/* in/out: InnoDB transaction handle */
{
	DBUG_ENTER("innobase_trx_init");
	DBUG_ASSERT(EQ_CURRENT_THD(thd));
	DBUG_ASSERT(thd == trx->mysql_thd);

	trx->check_foreigns = !thd_test_options(
		thd, OPTION_NO_FOREIGN_KEY_CHECKS);

	trx->check_unique_secondary = !thd_test_options(
		thd, OPTION_RELAXED_UNIQUE_CHECKS);

	DBUG_VOID_RETURN;
}

/*************************************************************************
Allocates an InnoDB transaction for a MySQL handler object. */
extern "C" UNIV_INTERN
trx_t*
innobase_trx_allocate(
/*==================*/
			/* out: InnoDB transaction handle */
	THD*	thd)	/* in: user thread handle */
{
	trx_t*	trx;

	DBUG_ENTER("innobase_trx_allocate");
	DBUG_ASSERT(thd != NULL);
	DBUG_ASSERT(EQ_CURRENT_THD(thd));

	trx = trx_allocate_for_mysql();

	trx->mysql_thd = thd;
	trx->mysql_query_str = thd_query(thd);

	innobase_trx_init(thd, trx);

	DBUG_RETURN(trx);
}

/*************************************************************************
Gets the InnoDB transaction handle for a MySQL handler object, creates
an InnoDB transaction struct if the corresponding MySQL thread struct still
lacks one. */
static
trx_t*
check_trx_exists(
/*=============*/
			/* out: InnoDB transaction handle */
	THD*	thd)	/* in: user thread handle */
{
	trx_t*&	trx = thd_to_trx(thd);

	ut_ad(EQ_CURRENT_THD(thd));

	if (trx == NULL) {
		trx = innobase_trx_allocate(thd);
	} else if (UNIV_UNLIKELY(trx->magic_n != TRX_MAGIC_N)) {
		mem_analyze_corruption(trx);
		ut_error;
	}

	innobase_trx_init(thd, trx);

	return(trx);
}


/*************************************************************************
Construct ha_innobase handler. */
UNIV_INTERN
ha_innobase::ha_innobase(handlerton *hton, TABLE_SHARE *table_arg)
  :handler(hton, table_arg),
  int_table_flags(HA_REC_NOT_IN_SEQ |
		  HA_NULL_IN_KEY |
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

/*************************************************************************
Destruct ha_innobase handler. */
UNIV_INTERN
ha_innobase::~ha_innobase()
{
}

/*************************************************************************
Updates the user_thd field in a handle and also allocates a new InnoDB
transaction handle if needed, and updates the transaction fields in the
prebuilt struct. */
UNIV_INTERN inline
void
ha_innobase::update_thd(
/*====================*/
	THD*	thd)	/* in: thd to use the handle */
{
	trx_t*		trx;

	trx = check_trx_exists(thd);

	if (prebuilt->trx != trx) {

		row_update_prebuilt_trx(prebuilt, trx);
	}

	user_thd = thd;
}

/*************************************************************************
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

/*************************************************************************
Registers that InnoDB takes part in an SQL statement, so that MySQL knows to
roll back the statement if the statement results in an error. This MUST be
called for every SQL statement that may be rolled back by MySQL. Calling this
several times to register the same statement is allowed, too. */
static inline
void
innobase_register_stmt(
/*===================*/
        handlerton*	hton,	/* in: Innobase hton */
	THD*	thd)	/* in: MySQL thd (connection) object */
{
	DBUG_ASSERT(hton == innodb_hton_ptr);
	/* Register the statement */
	trans_register_ha(thd, FALSE, hton);
}

/*************************************************************************
Registers an InnoDB transaction in MySQL, so that the MySQL XA code knows
to call the InnoDB prepare and commit, or rollback for the transaction. This
MUST be called for every transaction for which the user may call commit or
rollback. Calling this several times to register the same transaction is
allowed, too.
This function also registers the current SQL statement. */
static inline
void
innobase_register_trx_and_stmt(
/*===========================*/
        handlerton *hton, /* in: Innobase handlerton */
	THD*	thd)	/* in: MySQL thd (connection) object */
{
	/* NOTE that actually innobase_register_stmt() registers also
	the transaction in the AUTOCOMMIT=1 mode. */

	innobase_register_stmt(hton, thd);

	if (thd_test_options(thd, OPTION_NOT_AUTOCOMMIT | OPTION_BEGIN)) {

		/* No autocommit mode, register for a transaction */
		trans_register_ha(thd, TRUE, hton);
	}
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

/**********************************************************************
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
the InnoDB kernel mutex. */
static
my_bool
innobase_query_caching_of_table_permitted(
/*======================================*/
				/* out: TRUE if permitted, FALSE if not;
				note that the value FALSE does not mean
				we should invalidate the query cache:
				invalidation is called explicitly */
	THD*	thd,		/* in: thd of the user who is trying to
				store a result to the query cache or
				retrieve it */
	char*	full_name,	/* in: concatenation of database name,
				the null character '\0', and the table
				name */
	uint	full_name_len,	/* in: length of the full name, i.e.
				len(dbname) + len(tablename) + 1 */
	ulonglong *unused)	/* unused for this engine */
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

	innobase_release_stat_resources(trx);

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
	/* The call of row_search_.. will start a new transaction if it is
	not yet started */

	if (trx->active_trans == 0) {

		innobase_register_trx_and_stmt(innodb_hton_ptr, thd);
		trx->active_trans = 1;
	}

	if (row_search_check_if_query_cache_permitted(trx, norm_name)) {

		/* printf("Query cache for %s permitted\n", norm_name); */

		return((my_bool)TRUE);
	}

	/* printf("Query cache for %s NOT permitted\n", norm_name); */

	return((my_bool)FALSE);
}

/*********************************************************************
Invalidates the MySQL query cache for the table.
NOTE that the exact prototype of this function has to be in
/xtradb/row/row0ins.c! */
extern "C" UNIV_INTERN
void
innobase_invalidate_query_cache(
/*============================*/
	trx_t*	trx,		/* in: transaction which modifies the table */
	char*	full_name,	/* in: concatenation of database name, null
				char '\0', table name, null char'\0';
				NOTE that in Windows this is always
				in LOWER CASE! */
	ulint	full_name_len)	/* in: full name length where also the null
				chars count */
{
	/* Note that the sync0sync.h rank of the query cache mutex is just
	above the InnoDB kernel mutex. The caller of this function must not
	have latches of a lower rank. */

	/* Argument TRUE below means we are using transactions */
#ifdef HAVE_QUERY_CACHE
	mysql_query_cache_invalidate4((THD*) trx->mysql_thd,
				      (const char*) full_name,
				      (uint32) full_name_len,
				      TRUE);
#endif
}

/*********************************************************************
Convert an SQL identifier to the MySQL system_charset_info (UTF-8)
and quote it if needed. */
static
char*
innobase_convert_identifier(
/*========================*/
				/* out: pointer to the end of buf */
	char*		buf,	/* out: buffer for converted identifier */
	ulint		buflen,	/* in: length of buf, in bytes */
	const char*	id,	/* in: identifier to convert */
	ulint		idlen,	/* in: length of id, in bytes */
	void*		thd,	/* in: MySQL connection thread, or NULL */
	ibool		file_id)/* in: TRUE=id is a table or database name;
				FALSE=id is an UTF-8 string */
{
	char nz[NAME_LEN + 1];
	char nz2[NAME_LEN + 1 + sizeof srv_mysql50_table_name_prefix];

	const char*	s	= id;
	int		q;

	if (file_id) {
		/* Decode the table name.  The filename_to_tablename()
		function expects a NUL-terminated string.  The input and
		output strings buffers must not be shared. */

		if (UNIV_UNLIKELY(idlen > (sizeof nz) - 1)) {
			idlen = (sizeof nz) - 1;
		}

		memcpy(nz, id, idlen);
		nz[idlen] = 0;

		s = nz2;
		idlen = filename_to_tablename(nz, nz2, sizeof nz2);
	}

	/* See if the identifier needs to be quoted. */
	if (UNIV_UNLIKELY(!thd)) {
		q = '"';
	} else {
		q = get_quote_char_for_identifier((THD*) thd, s, (int) idlen);
	}

	if (q == EOF) {
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

/*********************************************************************
Convert a table or index name to the MySQL system_charset_info (UTF-8)
and quote it if needed. */
extern "C" UNIV_INTERN
char*
innobase_convert_name(
/*==================*/
				/* out: pointer to the end of buf */
	char*		buf,	/* out: buffer for converted identifier */
	ulint		buflen,	/* in: length of buf, in bytes */
	const char*	id,	/* in: identifier to convert */
	ulint		idlen,	/* in: length of id, in bytes */
	void*		thd,	/* in: MySQL connection thread, or NULL */
	ibool		table_id)/* in: TRUE=id is a table or database name;
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

/**************************************************************************
Determines if the currently running transaction has been interrupted. */
extern "C" UNIV_INTERN
ibool
trx_is_interrupted(
/*===============*/
			/* out: TRUE if interrupted */
	trx_t*	trx)	/* in: transaction */
{
	return(trx && trx->mysql_thd && thd_killed((THD*) trx->mysql_thd));
}

/******************************************************************
Resets some fields of a prebuilt struct. The template is used in fast
retrieval of just those column values MySQL needs in its processing. */
static
void
reset_template(
/*===========*/
	row_prebuilt_t*	prebuilt)	/* in/out: prebuilt struct */
{
	prebuilt->keep_other_fields_on_keyread = 0;
	prebuilt->read_just_key = 0;
}

/*********************************************************************
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

	innobase_release_stat_resources(prebuilt->trx);

	/* If the transaction is not started yet, start it */

	trx_start_if_not_started(prebuilt->trx);

	/* Assign a read view if the transaction does not have it yet */

	trx_assign_read_view(prebuilt->trx);

	/* Set the MySQL flag to mark that there is an active transaction */

	if (prebuilt->trx->active_trans == 0) {

		innobase_register_trx_and_stmt(ht, user_thd);

		prebuilt->trx->active_trans = 1;
	}

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
	reset_template(prebuilt);
}

/*************************************************************************
Opens an InnoDB database. */
static
int
innobase_init(
/*==========*/
			/* out: 0 on success, error code on failure */
	void	*p)	/* in: InnoDB handlerton */
{
	static char	current_dir[3];		/* Set if using current lib */
	int		err;
	bool		ret;
	char		*default_path;
	uint		format_id;

	DBUG_ENTER("innobase_init");
        handlerton *innobase_hton= (handlerton *)p;

#ifdef MYSQL_DYNAMIC_PLUGIN
	if (!innodb_plugin_init()) {
		sql_print_error("InnoDB plugin init failed.");
		DBUG_RETURN(-1);
	}

	if (innodb_hton_ptr) {
		/* Patch the statically linked handlerton and variables */
		innobase_hton = innodb_hton_ptr;
	}
#endif /* MYSQL_DYNAMIC_PLUGIN */

        innodb_hton_ptr = innobase_hton;

        innobase_hton->state = SHOW_OPTION_YES;
        innobase_hton->db_type= DB_TYPE_INNODB;
        innobase_hton->savepoint_offset=sizeof(trx_named_savept_t);
        innobase_hton->close_connection=innobase_close_connection;
        innobase_hton->savepoint_set=innobase_savepoint;
        innobase_hton->savepoint_rollback=innobase_rollback_to_savepoint;
        innobase_hton->savepoint_release=innobase_release_savepoint;
        innobase_hton->commit=innobase_commit;
        innobase_hton->rollback=innobase_rollback;
        innobase_hton->prepare=innobase_xa_prepare;
        innobase_hton->recover=innobase_xa_recover;
        innobase_hton->commit_by_xid=innobase_commit_by_xid;
        innobase_hton->rollback_by_xid=innobase_rollback_by_xid;
        innobase_hton->create_cursor_read_view=innobase_create_cursor_view;
        innobase_hton->set_cursor_read_view=innobase_set_cursor_view;
        innobase_hton->close_cursor_read_view=innobase_close_cursor_view;
        innobase_hton->create=innobase_create_handler;
        innobase_hton->drop_database=innobase_drop_database;
        innobase_hton->panic=innobase_end;
        innobase_hton->start_consistent_snapshot=innobase_start_trx_and_assign_read_view;
        innobase_hton->flush_logs=innobase_flush_logs;
        innobase_hton->show_status=innobase_show_status;
        innobase_hton->flags=HTON_NO_FLAGS;
        innobase_hton->release_temporary_latches=innobase_release_temporary_latches;
	innobase_hton->alter_table_flags = innobase_alter_table_flags;

	ut_a(DATA_MYSQL_TRUE_VARCHAR == (ulint)MYSQL_TYPE_VARCHAR);

#ifdef UNIV_DEBUG
	static const char	test_filename[] = "-@";
	char			test_tablename[sizeof test_filename
				+ sizeof srv_mysql50_table_name_prefix];
	if ((sizeof test_tablename) - 1
			!= filename_to_tablename(test_filename, test_tablename,
			sizeof test_tablename)
			|| strncmp(test_tablename,
			srv_mysql50_table_name_prefix,
			sizeof srv_mysql50_table_name_prefix)
			|| strcmp(test_tablename
			+ sizeof srv_mysql50_table_name_prefix,
			test_filename)) {
		sql_print_error("tablename encoding has been changed");
		goto error;
	}
#endif /* UNIV_DEBUG */

#ifndef MYSQL_SERVER
	innodb_overwrite_relay_log_info = FALSE;
#endif

#ifdef HAVE_REPLICATION
#ifdef MYSQL_SERVER
	/* read master log position from relay-log.info if exists */
	char fname[FN_REFLEN+128];
	int pos;
	int info_fd;
	IO_CACHE info_file;

	fname[0] = '\0';

	if(innobase_overwrite_relay_log_info) {

	fprintf(stderr,
		"InnoDB: Warning: innodb_overwrite_relay_log_info is enabled."
		" Updates in other storage engines may have problem with consistency.\n");

	bzero((char*) &info_file, sizeof(info_file));
	fn_format(fname, relay_log_info_file, mysql_data_home, "", 4+32);

	int error=0;

	if (!access(fname,F_OK)) {
		/* exist */
		if ((info_fd = my_open(fname, O_RDWR|O_BINARY, MYF(MY_WME))) < 0) {
			error=1;
		} else if (init_io_cache(&info_file, info_fd, IO_SIZE*2,
					READ_CACHE, 0L, 0, MYF(MY_WME))) {
			error=1;
		}

		if (error) {
relay_info_error:
			if (info_fd >= 0)
				my_close(info_fd, MYF(0));
			fname[0] = '\0';
			goto skip_relay;
		}
	} else {
		fname[0] = '\0';
		goto skip_relay;
	}

	if (init_strvar_from_file(fname, sizeof(fname), &info_file, "") || /* dummy (it is relay-log) */
	    init_intvar_from_file(&pos, &info_file, BIN_LOG_HEADER_SIZE)) { 
		end_io_cache(&info_file);
		error=1;
		goto relay_info_error;
	}

	fprintf(stderr,
		"InnoDB: relay-log.info is detected.\n"
		"InnoDB: relay log: position %u, file name %s\n",
		pos, fname);

	strncpy(trx_sys_mysql_relay_log_name, fname, TRX_SYS_MYSQL_MASTER_LOG_NAME_LEN);
	trx_sys_mysql_relay_log_pos = (ib_int64_t) pos;

	if (init_strvar_from_file(fname, sizeof(fname), &info_file, "") ||
	    init_intvar_from_file(&pos, &info_file, 0)) {
		end_io_cache(&info_file);
		error=1;
		goto relay_info_error;
	}

	fprintf(stderr,
		"InnoDB: master log: position %u, file name %s\n",
		pos, fname);

	strncpy(trx_sys_mysql_master_log_name, fname, TRX_SYS_MYSQL_MASTER_LOG_NAME_LEN);
	trx_sys_mysql_master_log_pos = (ib_int64_t) pos;

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

	if (specialflag & SPECIAL_NO_PRIOR) {
		srv_set_thread_priorities = FALSE;
	} else {
		srv_set_thread_priorities = TRUE;
		srv_query_thread_priority = QUERY_PRIOR;
	}

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
			"InnoDB: syntax error in innodb_data_file_path");
mem_free_and_error:
		srv_free_paths_and_sizes();
		my_free(internal_innobase_data_file_path,
						MYF(MY_ALLOW_ZERO_PTR));
		goto error;
	}

	srv_extra_undoslots = (ibool) innobase_extra_undoslots;

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

	/* Process innobase_file_format_check variable */
	ut_a(innobase_file_format_check != NULL);

	/* As a side effect it will set srv_check_file_format_at_startup
	on valid input. First we check for "on"/"off". */
	if (!innobase_file_format_check_on_off(innobase_file_format_check)) {

		/* Did the user specify a format name that we support ?
		As a side effect it will update the variable
		srv_check_file_format_at_startup */
		if (!innobase_file_format_check_validate(
			innobase_file_format_check)) {

			sql_print_error("InnoDB: invalid "
					"innodb_file_format_check value: "
					"should be either 'on' or 'off' or "
					"any value up to %s or its "
					"equivalent numeric id",
					trx_sys_file_format_id_to_name(
						DICT_TF_FORMAT_MAX));

			goto mem_free_and_error;
		}
	}

	ut_a((ulint) ibuf_use < UT_ARR_SIZE(innobase_change_buffering_values));
	innobase_change_buffering = (char*)
		innobase_change_buffering_values[ibuf_use];

	/* --------------------------------------------------*/

	srv_file_flush_method_str = innobase_unix_file_flush_method;

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

	srv_mem_pool_size = (ulint) innobase_additional_mem_pool_size;

	srv_n_file_io_threads = (ulint) innobase_file_io_threads;
	srv_n_read_io_threads = (ulint) innobase_read_io_threads;
	srv_n_write_io_threads = (ulint) innobase_write_io_threads;

	srv_read_ahead &= 3;
	srv_adaptive_checkpoint %= 3;

	srv_force_recovery = (ulint) innobase_force_recovery;

	srv_fast_recovery = (ibool) innobase_fast_recovery;

	srv_use_doublewrite_buf = (ibool) innobase_use_doublewrite;
	srv_use_checksums = (ibool) innobase_use_checksums;

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

	/* Since we in this module access directly the fields of a trx
	struct, and due to different headers and flags it might happen that
	mutex_t has a different size in this module and in InnoDB
	modules, we check at run time that the size is the same in
	these compilation modules. */

	err = innobase_start_or_create_for_mysql();

	if (err != DB_SUCCESS) {
		goto mem_free_and_error;
	}

#ifdef HAVE_REPLICATION
#ifdef MYSQL_SERVER
	if(innobase_overwrite_relay_log_info) {
	/* If InnoDB progressed from relay-log.info, overwrite it */
	if (fname[0] == '\0') {
		fprintf(stderr,
			"InnoDB: something wrong with relay-info.log. InnoDB will not overwrite it.\n");
	} else if (0 != strcmp(fname, trx_sys_mysql_master_log_name)
		   || pos != trx_sys_mysql_master_log_pos) {
		/* Overwrite relay-log.info */
		bzero((char*) &info_file, sizeof(info_file));
		fn_format(fname, relay_log_info_file, mysql_data_home, "", 4+32);

		int error = 0;

		if (!access(fname,F_OK)) {
			/* exist */
			if ((info_fd = my_open(fname, O_RDWR|O_BINARY, MYF(MY_WME))) < 0) {
				error = 1;
			} else if (init_io_cache(&info_file, info_fd, IO_SIZE*2,
						WRITE_CACHE, 0L, 0, MYF(MY_WME))) {
				error = 1;
			}

			if (error) {
				if (info_fd >= 0)
					my_close(info_fd, MYF(0));
				goto skip_overwrite;
			}
		} else {
			error = 1;
			goto skip_overwrite;
		}

		char buff[FN_REFLEN*2+22*2+4], *pos;

		my_b_seek(&info_file, 0L);
		pos=strmov(buff, trx_sys_mysql_relay_log_name);
		*pos++='\n';
		pos=longlong2str(trx_sys_mysql_relay_log_pos, pos, 10);
		*pos++='\n';
		pos=strmov(pos, trx_sys_mysql_master_log_name);
		*pos++='\n';
		pos=longlong2str(trx_sys_mysql_master_log_pos, pos, 10);
		*pos='\n';

		if (my_b_write(&info_file, (uchar*) buff, (size_t) (pos-buff)+1))
			error = 1;
		if (flush_io_cache(&info_file))
			error = 1;

		end_io_cache(&info_file);
		if (info_fd >= 0)
			my_close(info_fd, MYF(0));
skip_overwrite:
		if (error) {
			fprintf(stderr,
				"InnoDB: ERROR: error occured during overwriting relay-log.info.\n");
		} else {
			fprintf(stderr,
				"InnoDB: relay-log.info was overwritten.\n");
		}
	} else {
		fprintf(stderr,
			"InnoDB: InnoDB and relay-log.info are synchronized. InnoDB will not overwrite it.\n");
	}
	}
#endif /* MYSQL_SERVER */
#endif /* HAVE_REPLICATION */

	innobase_open_tables = hash_create(200);
	pthread_mutex_init(&innobase_share_mutex, MY_MUTEX_INIT_FAST);
	pthread_mutex_init(&prepare_commit_mutex, MY_MUTEX_INIT_FAST);
	pthread_mutex_init(&commit_threads_m, MY_MUTEX_INIT_FAST);
	pthread_mutex_init(&commit_cond_m, MY_MUTEX_INIT_FAST);
	pthread_cond_init(&commit_cond, NULL);
	innodb_inited= 1;
#ifdef MYSQL_DYNAMIC_PLUGIN
	if (innobase_hton != p) {
		innobase_hton = reinterpret_cast<handlerton*>(p);
		*innobase_hton = *innodb_hton_ptr;
	}
#endif /* MYSQL_DYNAMIC_PLUGIN */

	/* Get the current high water mark format. */
	innobase_file_format_check = (char*) trx_sys_file_format_max_get();

	DBUG_RETURN(FALSE);
error:
	DBUG_RETURN(TRUE);
}

/***********************************************************************
Closes an InnoDB database. */
static
int
innobase_end(handlerton *hton, ha_panic_function type)
/*==============*/
				/* out: TRUE if error */
{
	int	err= 0;

	DBUG_ENTER("innobase_end");
	DBUG_ASSERT(hton == innodb_hton_ptr);

#ifdef __NETWARE__	/* some special cleanup for NetWare */
	if (nw_panic) {
		set_panic_flag_for_netware();
	}
#endif
	if (innodb_inited) {

		srv_fast_shutdown = (ulint) innobase_fast_shutdown;
		innodb_inited = 0;
		hash_table_free(innobase_open_tables);
		innobase_open_tables = NULL;
		if (innobase_shutdown_for_mysql() != DB_SUCCESS) {
			err = 1;
		}
		srv_free_paths_and_sizes();
		my_free(internal_innobase_data_file_path,
						MYF(MY_ALLOW_ZERO_PTR));
		pthread_mutex_destroy(&innobase_share_mutex);
		pthread_mutex_destroy(&prepare_commit_mutex);
		pthread_mutex_destroy(&commit_threads_m);
		pthread_mutex_destroy(&commit_cond_m);
		pthread_cond_destroy(&commit_cond);
	}

	DBUG_RETURN(err);
}

/********************************************************************
Flushes InnoDB logs to disk and makes a checkpoint. Really, a commit flushes
the logs, and the name of this function should be innobase_checkpoint. */
static
bool
innobase_flush_logs(handlerton *hton)
/*=====================*/
				/* out: TRUE if error */
{
	bool	result = 0;

	DBUG_ENTER("innobase_flush_logs");
	DBUG_ASSERT(hton == innodb_hton_ptr);

	log_buffer_flush_to_disk();

	DBUG_RETURN(result);
}

/********************************************************************
Return alter table flags supported in an InnoDB database. */
static
uint
innobase_alter_table_flags(
/*=======================*/
	uint	flags)
{
	return(HA_ONLINE_ADD_INDEX_NO_WRITES
		| HA_ONLINE_DROP_INDEX_NO_WRITES
		| HA_ONLINE_ADD_UNIQUE_INDEX_NO_WRITES
		| HA_ONLINE_DROP_UNIQUE_INDEX_NO_WRITES
		| HA_ONLINE_ADD_PK_INDEX_NO_WRITES);
}

/*********************************************************************
Commits a transaction in an InnoDB database. */
static
void
innobase_commit_low(
/*================*/
	trx_t*	trx)	/* in: transaction handle */
{
	if (trx->conc_state == TRX_NOT_STARTED) {

		return;
	}

#ifdef HAVE_REPLICATION
#ifdef MYSQL_SERVER
	THD *thd=current_thd;

	if (thd && thd->slave_thread) {
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
#endif /* MYSQL_SERVER */
#endif /* HAVE_REPLICATION */

	trx_commit_for_mysql(trx);
}

/*********************************************************************
Creates an InnoDB transaction struct for the thd if it does not yet have one.
Starts a new InnoDB transaction if a transaction is not yet started. And
assigns a new snapshot for a consistent read if the transaction does not yet
have one. */
static
int
innobase_start_trx_and_assign_read_view(
/*====================================*/
			/* out: 0 */
        handlerton *hton, /* in: Innodb handlerton */ 
	THD*	thd)	/* in: MySQL thread handle of the user for whom
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

	innobase_release_stat_resources(trx);

	/* If the transaction is not started yet, start it */

	trx_start_if_not_started(trx);

	/* Assign a read view if the transaction does not have it yet */

	trx_assign_read_view(trx);

	/* Set the MySQL flag to mark that there is an active transaction */

	if (trx->active_trans == 0) {
		innobase_register_trx_and_stmt(hton, thd);
		trx->active_trans = 1;
	}

	DBUG_RETURN(0);
}

/*********************************************************************
Commits a transaction in an InnoDB database or marks an SQL statement
ended. */
static
int
innobase_commit(
/*============*/
			/* out: 0 */
        handlerton *hton, /* in: Innodb handlerton */ 
	THD* 	thd,	/* in: MySQL thread handle of the user for whom
			the transaction should be committed */
	bool	all)	/* in:	TRUE - commit transaction
				FALSE - the current SQL statement ended */
{
	trx_t*		trx;

	DBUG_ENTER("innobase_commit");
	DBUG_ASSERT(hton == innodb_hton_ptr);
	DBUG_PRINT("trans", ("ending transaction"));

	trx = check_trx_exists(thd);

	/* Since we will reserve the kernel mutex, we have to release
	the search system latch first to obey the latching order. */

	if (trx->has_search_latch) {
		trx_search_latch_release_if_reserved(trx);
	}

	/* The flag trx->active_trans is set to 1 in

	1. ::external_lock(),
	2. ::start_stmt(),
	3. innobase_query_caching_of_table_permitted(),
	4. innobase_savepoint(),
	5. ::init_table_handle_for_HANDLER(),
	6. innobase_start_trx_and_assign_read_view(),
	7. ::transactional_table_lock()

	and it is only set to 0 in a commit or a rollback. If it is 0 we know
	there cannot be resources to be freed and we could return immediately.
	For the time being, we play safe and do the cleanup though there should
	be nothing to clean up. */

	if (trx->active_trans == 0
		&& trx->conc_state != TRX_NOT_STARTED) {

		sql_print_error("trx->active_trans == 0, but"
			" trx->conc_state != TRX_NOT_STARTED");
	}
	if (all
		|| (!thd_test_options(thd, OPTION_NOT_AUTOCOMMIT | OPTION_BEGIN))) {

		/* We were instructed to commit the whole transaction, or
		this is an SQL statement end and autocommit is on */

		/* We need current binlog position for ibbackup to work.
		Note, the position is current because of
		prepare_commit_mutex */
retry:
		if (innobase_commit_concurrency > 0) {
			pthread_mutex_lock(&commit_cond_m);
			commit_threads++;

			if (commit_threads > innobase_commit_concurrency) {
				commit_threads--;
				pthread_cond_wait(&commit_cond,
					&commit_cond_m);
				pthread_mutex_unlock(&commit_cond_m);
				goto retry;
			}
			else {
				pthread_mutex_unlock(&commit_cond_m);
			}
		}

		trx->mysql_log_file_name = mysql_bin_log_file_name();
		trx->mysql_log_offset = (ib_int64_t) mysql_bin_log_file_pos();

		innobase_commit_low(trx);

		if (innobase_commit_concurrency > 0) {
			pthread_mutex_lock(&commit_cond_m);
			commit_threads--;
			pthread_cond_signal(&commit_cond);
			pthread_mutex_unlock(&commit_cond_m);
		}

		if (trx->active_trans == 2) {

			pthread_mutex_unlock(&prepare_commit_mutex);
		}

		trx->active_trans = 0;

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

/*********************************************************************
Rolls back a transaction or the latest SQL statement. */
static
int
innobase_rollback(
/*==============*/
			/* out: 0 or error number */
        handlerton *hton, /* in: Innodb handlerton */ 
	THD*	thd,	/* in: handle to the MySQL thread of the user
			whose transaction should be rolled back */
	bool	all)	/* in:	TRUE - commit transaction
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

	innobase_release_stat_resources(trx);

	/* If we had reserved the auto-inc lock for some table (if
	we come here to roll back the latest SQL statement) we
	release it now before a possibly lengthy rollback */

	row_unlock_table_autoinc_for_mysql(trx);

	if (all
		|| !thd_test_options(thd, OPTION_NOT_AUTOCOMMIT | OPTION_BEGIN)) {

		error = trx_rollback_for_mysql(trx);
		trx->active_trans = 0;
	} else {
		error = trx_rollback_last_sql_stat_for_mysql(trx);
	}

	DBUG_RETURN(convert_error_code_to_mysql(error, 0, NULL));
}

/*********************************************************************
Rolls back a transaction */
static
int
innobase_rollback_trx(
/*==================*/
			/* out: 0 or error number */
	trx_t*	trx)	/*  in: transaction */
{
	int	error = 0;

	DBUG_ENTER("innobase_rollback_trx");
	DBUG_PRINT("trans", ("aborting transaction"));

	/* Release a possible FIFO ticket and search latch. Since we will
	reserve the kernel mutex, we have to release the search system latch
	first to obey the latching order. */

	innobase_release_stat_resources(trx);

	/* If we had reserved the auto-inc lock for some table (if
	we come here to roll back the latest SQL statement) we
	release it now before a possibly lengthy rollback */

	row_unlock_table_autoinc_for_mysql(trx);

	error = trx_rollback_for_mysql(trx);

	DBUG_RETURN(convert_error_code_to_mysql(error, 0, NULL));
}

/*********************************************************************
Rolls back a transaction to a savepoint. */
static
int
innobase_rollback_to_savepoint(
/*===========================*/
				/* out: 0 if success, HA_ERR_NO_SAVEPOINT if
				no savepoint with the given name */
        handlerton *hton,       /* in: Innodb handlerton */ 
	THD*	thd,		/* in: handle to the MySQL thread of the user
				whose transaction should be rolled back */
	void*	savepoint)	/* in: savepoint data */
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

	innobase_release_stat_resources(trx);

	/* TODO: use provided savepoint data area to store savepoint data */

	longlong2str((ulint)savepoint, name, 36);

	error = (int) trx_rollback_to_savepoint_for_mysql(trx, name,
						&mysql_binlog_cache_pos);
	DBUG_RETURN(convert_error_code_to_mysql(error, 0, NULL));
}

/*********************************************************************
Release transaction savepoint name. */
static
int
innobase_release_savepoint(
/*=======================*/
				/* out: 0 if success, HA_ERR_NO_SAVEPOINT if
				no savepoint with the given name */
        handlerton*	hton,	/* in: handlerton for Innodb */
	THD*	thd,		/* in: handle to the MySQL thread of the user
				whose transaction should be rolled back */
	void*	savepoint)	/* in: savepoint data */
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

/*********************************************************************
Sets a transaction savepoint. */
static
int
innobase_savepoint(
/*===============*/
				/* out: always 0, that is, always succeeds */
	handlerton*	hton,   /* in: handle to the Innodb handlerton */
	THD*	thd,		/* in: handle to the MySQL thread */
	void*	savepoint)	/* in: savepoint data */
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

	innobase_release_stat_resources(trx);

	/* cannot happen outside of transaction */
	DBUG_ASSERT(trx->active_trans);

	/* TODO: use provided savepoint data area to store savepoint data */
	char name[64];
	longlong2str((ulint)savepoint,name,36);

	error = (int) trx_savepoint_for_mysql(trx, name, (ib_int64_t)0);

	DBUG_RETURN(convert_error_code_to_mysql(error, 0, NULL));
}

/*********************************************************************
Frees a possible InnoDB trx object associated with the current THD. */
static
int
innobase_close_connection(
/*======================*/
			/* out: 0 or error number */
        handlerton*	hton,	/* in:  innobase handlerton */
	THD*	thd)	/* in: handle to the MySQL thread of the user
			whose resources should be free'd */
{
	trx_t*	trx;

	DBUG_ENTER("innobase_close_connection");
	DBUG_ASSERT(hton == innodb_hton_ptr);
	trx = thd_to_trx(thd);

	ut_a(trx);

	if (trx->active_trans == 0
		&& trx->conc_state != TRX_NOT_STARTED) {

		sql_print_error("trx->active_trans == 0, but"
			" trx->conc_state != TRX_NOT_STARTED");
	}


	if (trx->conc_state != TRX_NOT_STARTED &&
		global_system_variables.log_warnings) {
		sql_print_warning(
			"MySQL is closing a connection that has an active "
			"InnoDB transaction.  %lu row modifications will "
			"roll back.",
			(ulong) trx->undo_no.low);
	}

	innobase_rollback_trx(trx);

	thr_local_free(trx->mysql_thread_id);
	trx_free_for_mysql(trx);

	DBUG_RETURN(0);
}


/*****************************************************************************
** InnoDB database tables
*****************************************************************************/

/********************************************************************
Get the record format from the data dictionary. */
UNIV_INTERN
enum row_type
ha_innobase::get_row_type() const
/*=============================*/
			/* out: one of
			ROW_TYPE_REDUNDANT,
			ROW_TYPE_COMPACT,
			ROW_TYPE_COMPRESSED,
			ROW_TYPE_DYNAMIC */
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



/********************************************************************
Get the table flags to use for the statement. */
UNIV_INTERN
handler::Table_flags
ha_innobase::table_flags() const
{
       /* Need to use tx_isolation here since table flags is (also)
          called before prebuilt is inited. */
        ulong const tx_isolation = thd_tx_isolation(ha_thd());
        if (tx_isolation <= ISO_READ_COMMITTED)
                return int_table_flags;
        return int_table_flags | HA_BINLOG_STMT_CAPABLE;
}

/********************************************************************
Gives the file extension of an InnoDB single-table tablespace. */
static const char* ha_innobase_exts[] = {
  ".ibd",
  NullS
};

UNIV_INTERN
const char*
ha_innobase::table_type() const
/*===========================*/
				/* out: table type */
{
	return(innobase_hton_name);
}

UNIV_INTERN
const char*
ha_innobase::index_type(uint)
/*=========================*/
				/* out: index type */
{
	return("BTREE");
}

UNIV_INTERN
const char**
ha_innobase::bas_ext() const
/*========================*/
				/* out: file extension string */
{
	return(ha_innobase_exts);
}

UNIV_INTERN
ulong
ha_innobase::index_flags(uint, uint, bool) const
{
	return(HA_READ_NEXT | HA_READ_PREV | HA_READ_ORDER
	       | HA_READ_RANGE | HA_KEYREAD_ONLY);
}

UNIV_INTERN
uint
ha_innobase::max_supported_keys() const
{
	return(MAX_KEY);
}

UNIV_INTERN
uint
ha_innobase::max_supported_key_length() const
{
	/* An InnoDB page must store >= 2 keys; a secondary key record
	must also contain the primary key value: max key length is
	therefore set to slightly less than 1 / 4 of page size which
	is 16 kB; but currently MySQL does not work with keys whose
	size is > MAX_KEY_LENGTH */
	return(3500);
}

UNIV_INTERN
const key_map*
ha_innobase::keys_to_use_for_scanning()
{
	return(&key_map_full);
}

UNIV_INTERN
uint8
ha_innobase::table_cache_type()
{
	return(HA_CACHE_TBL_ASKTRANSACT);
}

UNIV_INTERN
bool
ha_innobase::primary_key_is_clustered()
{
	return(true);
}

/*********************************************************************
Normalizes a table name string. A normalized name consists of the
database name catenated to '/' and table name. An example:
test/mytable. On Windows normalization puts both the database name and the
table name always to lower case. */
static
void
normalize_table_name(
/*=================*/
	char*		norm_name,	/* out: normalized name as a
					null-terminated string */
	const char*	name)		/* in: table name string */
{
	char*	name_ptr;
	char*	db_ptr;
	char*	ptr;

	/* Scan name from the end */

	ptr = strend(name)-1;

	while (ptr >= name && *ptr != '\\' && *ptr != '/') {
		ptr--;
	}

	name_ptr = ptr + 1;

	DBUG_ASSERT(ptr > name);

	ptr--;

	while (ptr >= name && *ptr != '\\' && *ptr != '/') {
		ptr--;
	}

	db_ptr = ptr + 1;

	memcpy(norm_name, db_ptr, strlen(name) + 1 - (db_ptr - name));

	norm_name[name_ptr - db_ptr - 1] = '/';

#ifdef __WIN__
	innobase_casedn_str(norm_name);
#endif
}

/************************************************************************
Set the autoinc column max value. This should only be called once from
ha_innobase::open(). Therefore there's no need for a covering lock. */
UNIV_INTERN
ulint
ha_innobase::innobase_initialize_autoinc()
/*======================================*/
{
	dict_index_t*	index;
	ulonglong	auto_inc;
	const char*	col_name;
	ulint		error = DB_SUCCESS;
	dict_table_t*	innodb_table = prebuilt->table;

	col_name = table->found_next_number_field->field_name;
	index = innobase_get_index(table->s->next_number_index);

	/* Execute SELECT MAX(col_name) FROM TABLE; */
	error = row_search_max_autoinc(index, col_name, &auto_inc);

	if (error == DB_SUCCESS) {

		/* At the this stage we dont' know the increment
		or the offset, so use default inrement of 1. */
		++auto_inc;

		dict_table_autoinc_initialize(innodb_table, auto_inc);

	} else {
		ut_print_timestamp(stderr);
		fprintf(stderr, "  InnoDB: Error: (%lu) Couldn't read "
			"the MAX(%s) autoinc value from the "
			"index (%s).\n", error, col_name, index->name);
	}

	return(error);
}

/*********************************************************************
Creates and opens a handle to a table which already exists in an InnoDB
database. */
UNIV_INTERN
int
ha_innobase::open(
/*==============*/
					/* out: 1 if error, 0 if success */
	const char*	name,		/* in: table name */
	int		mode,		/* in: not used */
	uint		test_if_locked)	/* in: not used */
{
	dict_table_t*	ib_table;
	char		norm_name[1000];
	THD*		thd;
	ulint		retries = 0;
	char*		is_part = NULL;

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

	/* Create buffers for packing the fields of a record. Why
	table->reclength did not work here? Obviously, because char
	fields when packed actually became 1 byte longer, when we also
	stored the string length as the first byte. */

	upd_and_key_val_buff_len =
				table->s->reclength + table->s->max_key_length
							+ MAX_REF_PARTS * 3;
	if (!(uchar*) my_multi_malloc(MYF(MY_WME),
			&upd_buff, upd_and_key_val_buff_len,
			&key_val_buff, upd_and_key_val_buff_len,
			NullS)) {
		free_share(share);

		DBUG_RETURN(1);
	}

	/* We look for pattern #P# to see if the table is partitioned
	MySQL table. The retry logic for partitioned tables is a
	workaround for http://bugs.mysql.com/bug.php?id=33349. Look
	at support issue https://support.mysql.com/view.php?id=21080
	for more details. */
	is_part = strstr(norm_name, "#P#");
retry:
	/* Get pointer to a table object in InnoDB dictionary cache */
	ib_table = dict_table_get(norm_name, TRUE);
	
	if (NULL == ib_table) {
		if (is_part && retries < 10) {
			++retries;
			os_thread_sleep(100000);
			goto retry;
		}

		if (is_part) {
			sql_print_error("Failed to open table %s after "
					"%lu attemtps.\n", norm_name,
					retries);
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
				"See http://dev.mysql.com/doc/refman/5.1/en/innodb-troubleshooting.html\n"
				"how you can resolve the problem.\n",
				norm_name);
		free_share(share);
		my_free(upd_buff, MYF(0));
		my_errno = ENOENT;

		DBUG_RETURN(HA_ERR_NO_SUCH_TABLE);
	}

	if (ib_table->ibd_file_missing && !thd_tablespace_op(thd)) {
		sql_print_error("MySQL is trying to open a table handle but "
				"the .ibd file for\ntable %s does not exist.\n"
				"Have you deleted the .ibd file from the "
				"database directory under\nthe MySQL datadir, "
				"or have you used DISCARD TABLESPACE?\n"
				"See http://dev.mysql.com/doc/refman/5.1/en/innodb-troubleshooting.html\n"
				"how you can resolve the problem.\n",
				norm_name);
		free_share(share);
		my_free(upd_buff, MYF(0));
		my_errno = ENOENT;

		dict_table_decrement_handle_count(ib_table, FALSE);
		DBUG_RETURN(HA_ERR_NO_SUCH_TABLE);
	}

	prebuilt = row_create_prebuilt(ib_table);

	prebuilt->mysql_row_len = table->s->reclength;
	prebuilt->default_rec = table->s->default_values;
	ut_ad(prebuilt->default_rec);

	/* Looks like MySQL-3.23 sometimes has primary key number != 0 */

	primary_key = table->s->primary_key;
	key_used_on_scan = primary_key;

	/* Allocate a buffer for a 'row reference'. A row reference is
	a string of bytes of length ref_length which uniquely specifies
	a row in our table. Note that MySQL may also compare two row
	references for equality by doing a simple memcmp on the strings
	of length ref_length! */

	if (!row_table_got_default_clust_index(ib_table)) {
		if (primary_key >= MAX_KEY) {
		  sql_print_error("Table %s has a primary key in InnoDB data "
				  "dictionary, but not in MySQL!", name);
		}

		prebuilt->clust_index_was_generated = FALSE;

		/* MySQL allocates the buffer for ref. key_info->key_length
		includes space for all key columns + one byte for each column
		that may be NULL. ref_length must be as exact as possible to
		save space, because all row reference buffers are allocated
		based on ref_length. */

		ref_length = table->key_info[primary_key].key_length;
	} else {
		if (primary_key != MAX_KEY) {
		  sql_print_error("Table %s has no primary key in InnoDB data "
				  "dictionary, but has one in MySQL! If you "
				  "created the table with a MySQL version < "
				  "3.23.54 and did not define a primary key, "
				  "but defined a unique key with all non-NULL "
				  "columns, then MySQL internally treats that "
				  "key as the primary key. You can fix this "
				  "error by dump + DROP + CREATE + reimport "
				  "of the table.", name);
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
			(const char**) &innobase_file_format_check,
			dict_table_get_format(prebuilt->table));
	}

	info(HA_STATUS_NO_LOCK | HA_STATUS_VARIABLE | HA_STATUS_CONST);

	/* Only if the table has an AUTOINC column. */
	if (prebuilt->table != NULL && table->found_next_number_field != NULL) {
		ulint	error;

		dict_table_autoinc_lock(prebuilt->table);

		/* Since a table can already be "open" in InnoDB's internal
		data dictionary, we only init the autoinc counter once, the
		first time the table is loaded. We can safely reuse the
		autoinc value from a previous MySQL open. */
		if (dict_table_autoinc_read(prebuilt->table) == 0) {

			error = innobase_initialize_autoinc();
			/* Should always succeed! */
			ut_a(error == DB_SUCCESS);
		}

		dict_table_autoinc_unlock(prebuilt->table);
	}

	DBUG_RETURN(0);
}

UNIV_INTERN
uint
ha_innobase::max_supported_key_part_length() const
{
	return(DICT_MAX_INDEX_COL_LEN - 1);
}

/**********************************************************************
Closes a handle to an InnoDB table. */
UNIV_INTERN
int
ha_innobase::close(void)
/*====================*/
				/* out: 0 */
{
	THD*	thd;

	DBUG_ENTER("ha_innobase::close");

	thd = ha_thd();
	if (thd != NULL) {
		innobase_release_temporary_latches(ht, thd);
	}

	row_prebuilt_free(prebuilt, FALSE);

	my_free(upd_buff, MYF(0));
	free_share(share);

	/* Tell InnoDB server that there might be work for
	utility threads: */

	srv_active_wake_master_thread();

	DBUG_RETURN(0);
}

/* The following accessor functions should really be inside MySQL code! */

/******************************************************************
Gets field offset for a field in a table. */
static inline
uint
get_field_offset(
/*=============*/
			/* out: offset */
	TABLE*	table,	/* in: MySQL table object */
	Field*	field)	/* in: MySQL field object */
{
	return((uint) (field->ptr - table->record[0]));
}

/******************************************************************
Checks if a field in a record is SQL NULL. Uses the record format
information in table to track the null bit in record. */
static inline
uint
field_in_record_is_null(
/*====================*/
			/* out: 1 if NULL, 0 otherwise */
	TABLE*	table,	/* in: MySQL table object */
	Field*	field,	/* in: MySQL field object */
	char*	record)	/* in: a row in MySQL format */
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

/******************************************************************
Sets a field in a record to SQL NULL. Uses the record format
information in table to track the null bit in record. */
static inline
void
set_field_in_record_to_null(
/*========================*/
	TABLE*	table,	/* in: MySQL table object */
	Field*	field,	/* in: MySQL field object */
	char*	record)	/* in: a row in MySQL format */
{
	int	null_offset;

	null_offset = (uint) ((char*) field->null_ptr
					- (char*) table->record[0]);

	record[null_offset] = record[null_offset] | field->null_bit;
}

/*****************************************************************
InnoDB uses this function to compare two data fields for which the data type
is such that we must use MySQL code to compare them. NOTE that the prototype
of this function is in rem0cmp.c in InnoDB source code! If you change this
function, remember to update the prototype there! */
extern "C" UNIV_INTERN
int
innobase_mysql_cmp(
/*===============*/
					/* out: 1, 0, -1, if a is greater,
					equal, less than b, respectively */
	int		mysql_type,	/* in: MySQL type */
	uint		charset_number,	/* in: number of the charset */
	const unsigned char* a,		/* in: data field */
	unsigned int	a_length,	/* in: data field length,
					not UNIV_SQL_NULL */
	const unsigned char* b,		/* in: data field */
	unsigned int	b_length)	/* in: data field length,
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

/******************************************************************
Converts a MySQL type to an InnoDB type. Note that this function returns
the 'mtype' of InnoDB. InnoDB differentiates between MySQL's old <= 4.1
VARCHAR and the new true VARCHAR in >= 5.0.3 by the 'prtype'. */
extern "C" UNIV_INTERN
ulint
get_innobase_type_from_mysql_type(
/*==============================*/
					/* out: DATA_BINARY,
					DATA_VARCHAR, ... */
	ulint*		unsigned_flag,	/* out: DATA_UNSIGNED if an
					'unsigned type';
					at least ENUM and SET,
					and unsigned integer
					types are 'unsigned types' */
	const void*	f)		/* in: MySQL Field */
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
	case MYSQL_TYPE_DATETIME:
	case MYSQL_TYPE_YEAR:
	case MYSQL_TYPE_NEWDATE:
	case MYSQL_TYPE_TIME:
	case MYSQL_TYPE_TIMESTAMP:
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
	default:
		ut_error;
	}

	return(0);
}

/***********************************************************************
Writes an unsigned integer value < 64k to 2 bytes, in the little-endian
storage format. */
static inline
void
innobase_write_to_2_little_endian(
/*==============================*/
	byte*	buf,	/* in: where to store */
	ulint	val)	/* in: value to write, must be < 64k */
{
	ut_a(val < 256 * 256);

	buf[0] = (byte)(val & 0xFF);
	buf[1] = (byte)(val / 256);
}

/***********************************************************************
Reads an unsigned integer value < 64k from 2 bytes, in the little-endian
storage format. */
static inline
uint
innobase_read_from_2_little_endian(
/*===============================*/
				/* out: value */
	const uchar*	buf)	/* in: from where to read */
{
	return (uint) ((ulint)(buf[0]) + 256 * ((ulint)(buf[1])));
}

/***********************************************************************
Stores a key value for a row to a buffer. */
UNIV_INTERN
uint
ha_innobase::store_key_val_for_row(
/*===============================*/
				/* out: key value length as stored in buff */
	uint		keynr,	/* in: key number */
	char*		buff,	/* in/out: buffer for the key value (in MySQL
				format) */
	uint		buff_len,/* in: buffer length */
	const uchar*	record)/* in: row in MySQL format */
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
			|| mysql_type == MYSQL_TYPE_LONG_BLOB) {

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

			/* Pad the unused space with spaces. Note that no
			padding is ever needed for UCS-2 because in MySQL,
			all UCS2 characters are 2 bytes, as MySQL does not
			support surrogate pairs, which are needed to represent
			characters in the range U+10000 to U+10FFFF. */

			if (true_len < key_len) {
				ulint pad_len = key_len - true_len;
				memset(buff, ' ', pad_len);
				buff += pad_len;
			}
		}
	}

	ut_a(buff <= buff_start + buff_len);

	DBUG_RETURN((uint)(buff - buff_start));
}

/******************************************************************
Builds a 'template' to the prebuilt struct. The template is used in fast
retrieval of just those column values MySQL needs in its processing. */
static
void
build_template(
/*===========*/
	row_prebuilt_t*	prebuilt,	/* in/out: prebuilt struct */
	THD*		thd,		/* in: current user thread, used
					only if templ_type is
					ROW_MYSQL_REC_FIELDS */
	TABLE*		table,		/* in: MySQL table */
	uint		templ_type)	/* in: ROW_MYSQL_WHOLE_ROW or
					ROW_MYSQL_REC_FIELDS */
{
	dict_index_t*	index;
	dict_index_t*	clust_index;
	mysql_row_templ_t* templ;
	Field*		field;
	ulint		n_fields;
	ulint		n_requested_fields	= 0;
	ibool		fetch_all_in_key	= FALSE;
	ibool		fetch_primary_key_cols	= FALSE;
	ulint		i;
	/* byte offset of the end of last requested column */
	ulint		mysql_prefix_len	= 0;

	if (prebuilt->select_lock_type == LOCK_X) {
		/* We always retrieve the whole clustered index record if we
		use exclusive row level locks, for example, if the read is
		done in an UPDATE statement. */

		templ_type = ROW_MYSQL_WHOLE_ROW;
	}

	if (templ_type == ROW_MYSQL_REC_FIELDS) {
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
				templ_type = ROW_MYSQL_WHOLE_ROW;
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

	if (templ_type == ROW_MYSQL_REC_FIELDS) {
		index = prebuilt->index;
	} else {
		index = clust_index;
	}

	if (index == clust_index) {
		prebuilt->need_to_access_clustered = TRUE;
	} else {
		prebuilt->need_to_access_clustered = FALSE;
		/* Below we check column by column if we need to access
		the clustered index */
	}

	n_fields = (ulint)table->s->fields; /* number of columns */

	if (!prebuilt->mysql_template) {
		prebuilt->mysql_template = (mysql_row_templ_t*)
			mem_alloc(n_fields * sizeof(mysql_row_templ_t));
	}

	prebuilt->template_type = templ_type;
	prebuilt->null_bitmap_len = table->s->null_bytes;

	prebuilt->templ_contains_blob = FALSE;

	/* Note that in InnoDB, i is the column number. MySQL calls columns
	'fields'. */
	for (i = 0; i < n_fields; i++) {
		templ = prebuilt->mysql_template + n_requested_fields;
		field = table->field[i];

		if (UNIV_LIKELY(templ_type == ROW_MYSQL_REC_FIELDS)) {
			/* Decide which columns we should fetch
			and which we can skip. */
			register const ibool	index_contains_field =
				dict_index_contains_col_or_prefix(index, i);

			if (!index_contains_field && prebuilt->read_just_key) {
				/* If this is a 'key read', we do not need
				columns that are not in the key */

				goto skip_field;
			}

			if (index_contains_field && fetch_all_in_key) {
				/* This field is needed in the query */

				goto include_field;
			}

			if (bitmap_is_set(table->read_set, i) ||
			    bitmap_is_set(table->write_set, i)) {
				/* This field is needed in the query */

				goto include_field;
			}

			if (fetch_primary_key_cols
				&& dict_table_col_in_clustered_key(
					index->table, i)) {
				/* This field is needed in the query */

				goto include_field;
			}

			/* This field is not needed in the query, skip it */

			goto skip_field;
		}
include_field:
		n_requested_fields++;

		templ->col_no = i;

		if (index == clust_index) {
			templ->rec_field_no = dict_col_get_clust_pos(
				&index->table->cols[i], index);
		} else {
			templ->rec_field_no = dict_index_get_nth_col_pos(
								index, i);
		}

		if (templ->rec_field_no == ULINT_UNDEFINED) {
			prebuilt->need_to_access_clustered = TRUE;
		}

		if (field->null_ptr) {
			templ->mysql_null_byte_offset =
				(ulint) ((char*) field->null_ptr
					- (char*) table->record[0]);

			templ->mysql_null_bit_mask = (ulint) field->null_bit;
		} else {
			templ->mysql_null_bit_mask = 0;
		}

		templ->mysql_col_offset = (ulint)
					get_field_offset(table, field);

		templ->mysql_col_len = (ulint) field->pack_length();
		if (mysql_prefix_len < templ->mysql_col_offset
				+ templ->mysql_col_len) {
			mysql_prefix_len = templ->mysql_col_offset
				+ templ->mysql_col_len;
		}
		templ->type = index->table->cols[i].mtype;
		templ->mysql_type = (ulint)field->type();

		if (templ->mysql_type == DATA_MYSQL_TRUE_VARCHAR) {
			templ->mysql_length_bytes = (ulint)
				(((Field_varstring*)field)->length_bytes);
		}

		templ->charset = dtype_get_charset_coll(
			index->table->cols[i].prtype);
		templ->mbminlen = index->table->cols[i].mbminlen;
		templ->mbmaxlen = index->table->cols[i].mbmaxlen;
		templ->is_unsigned = index->table->cols[i].prtype
							& DATA_UNSIGNED;
		if (templ->type == DATA_BLOB) {
			prebuilt->templ_contains_blob = TRUE;
		}
skip_field:
		;
	}

	prebuilt->n_template = n_requested_fields;
	prebuilt->mysql_prefix_len = mysql_prefix_len;

	if (index != clust_index && prebuilt->need_to_access_clustered) {
		/* Change rec_field_no's to correspond to the clustered index
		record */
		for (i = 0; i < n_requested_fields; i++) {
			templ = prebuilt->mysql_template + i;

			templ->rec_field_no = dict_col_get_clust_pos(
				&index->table->cols[templ->col_no],
				clust_index);
		}
	}
}

/************************************************************************
Get the upper limit of the MySQL integral and floating-point type. */
UNIV_INTERN
ulonglong
ha_innobase::innobase_get_int_col_max_value(
/*========================================*/
	const Field*	field)
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

/************************************************************************
This special handling is really to overcome the limitations of MySQL's
binlogging. We need to eliminate the non-determinism that will arise in
INSERT ... SELECT type of statements, since MySQL binlog only stores the
min value of the autoinc interval. Once that is fixed we can get rid of
the special lock handling.*/
UNIV_INTERN
ulint
ha_innobase::innobase_lock_autoinc(void)
/*====================================*/
					/* out: DB_SUCCESS if all OK else
					error code */
{
	ulint		error = DB_SUCCESS;

	switch (innobase_autoinc_lock_mode) {
	case AUTOINC_NO_LOCKING:
		/* Acquire only the AUTOINC mutex. */
		dict_table_autoinc_lock(prebuilt->table);
		break;

	case AUTOINC_NEW_STYLE_LOCKING:
		/* For simple (single/multi) row INSERTs, we fallback to the
		old style only if another transaction has already acquired
		the AUTOINC lock on behalf of a LOAD FILE or INSERT ... SELECT
		etc. type of statement. */
		if (thd_sql_command(user_thd) == SQLCOM_INSERT
		    || thd_sql_command(user_thd) == SQLCOM_REPLACE) {
			dict_table_t*	table = prebuilt->table;

			/* Acquire the AUTOINC mutex. */
			dict_table_autoinc_lock(table);

			/* We need to check that another transaction isn't
			already holding the AUTOINC lock on the table. */
			if (table->n_waiting_or_granted_auto_inc_locks) {
				/* Release the mutex to avoid deadlocks. */
				dict_table_autoinc_unlock(table);
			} else {
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

/************************************************************************
Reset the autoinc value in the table.*/
UNIV_INTERN
ulint
ha_innobase::innobase_reset_autoinc(
/*================================*/
					/* out: DB_SUCCESS if all went well
					else error code */
	ulonglong	autoinc)	/* in: value to store */
{
	ulint		error;

	error = innobase_lock_autoinc();

	if (error == DB_SUCCESS) {

		dict_table_autoinc_initialize(prebuilt->table, autoinc);

		dict_table_autoinc_unlock(prebuilt->table);
	}

	return(ulong(error));
}

/************************************************************************
Store the autoinc value in the table. The autoinc value is only set if
it's greater than the existing autoinc value in the table.*/
UNIV_INTERN
ulint
ha_innobase::innobase_set_max_autoinc(
/*==================================*/
					/* out: DB_SUCCES if all went well
					else error code */
	ulonglong	auto_inc)	/* in: value to store */
{
	ulint		error;

	error = innobase_lock_autoinc();

	if (error == DB_SUCCESS) {

		dict_table_autoinc_update_if_greater(prebuilt->table, auto_inc);

		dict_table_autoinc_unlock(prebuilt->table);
	}

	return(ulong(error));
}

/************************************************************************
Stores a row in an InnoDB database, to the table specified in this
handle. */
UNIV_INTERN
int
ha_innobase::write_row(
/*===================*/
			/* out: error code */
	uchar*	record)	/* in: a row in MySQL format */
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
			prebuilt->trx->active_trans = 1;
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
			prebuilt->trx->active_trans = 1;
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
			if (prebuilt->autoinc_error != DB_SUCCESS) {
				error = (int) prebuilt->autoinc_error;

				goto report_error;
			}

			/* MySQL errors are passed straight back. */
			error_result = (int) error;
			goto func_exit;
		}

		auto_inc_used = TRUE;
	}

	if (prebuilt->mysql_template == NULL
	    || prebuilt->template_type != ROW_MYSQL_WHOLE_ROW) {

		/* Build the template used in converting quickly between
		the two database formats */

		build_template(prebuilt, NULL, table, ROW_MYSQL_WHOLE_ROW);
	}

	innodb_srv_conc_enter_innodb(prebuilt->trx);

	error = row_insert_for_mysql((byte*) record, prebuilt);

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
				if ((trx->duplicates
				    & (TRX_DUP_IGNORE | TRX_DUP_REPLACE))) {

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

			if (auto_inc <= col_max_value
			    && auto_inc >= prebuilt->autoinc_last_value) {
set_max_autoinc:
				ut_a(prebuilt->autoinc_increment > 0);

				ulonglong	need;
				ulonglong	offset;

				offset = prebuilt->autoinc_offset;
				need = prebuilt->autoinc_increment;

				auto_inc = innobase_next_autoinc(
					auto_inc, need, offset, col_max_value);

				err = innobase_set_max_autoinc(auto_inc);

				if (err != DB_SUCCESS) {
					error = err;
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

	DBUG_RETURN(error_result);
}

/**************************************************************************
Checks which fields have changed in a row and stores information
of them to an update vector. */
static
int
calc_row_difference(
/*================*/
					/* out: error number or 0 */
	upd_t*		uvect,		/* in/out: update vector */
	uchar*		old_row,	/* in: old row in MySQL format */
	uchar*		new_row,	/* in: new row in MySQL format */
	struct st_table* table,		/* in: table in MySQL data
					dictionary */
	uchar*		upd_buff,	/* in: buffer to use */
	ulint		buff_len,	/* in: buffer length */
	row_prebuilt_t*	prebuilt,	/* in: InnoDB prebuilt struct */
	THD*		thd)		/* in: user thread */
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
	uint		i;

	n_fields = table->s->fields;
	clust_index = dict_table_get_first_index(prebuilt->table);

	/* We use upd_buff to convert changed fields */
	buf = (byte*) upd_buff;

	for (i = 0; i < n_fields; i++) {
		field = table->field[i];

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

		col_type = prebuilt->table->cols[i].mtype;

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

			/* Let us use a dummy dfield to make the conversion
			from the MySQL column format to the InnoDB format */

			dict_col_copy_type(prebuilt->table->cols + i,
					   dfield_get_type(&dfield));

			if (n_len != UNIV_SQL_NULL) {
				buf = row_mysql_store_col_in_innobase_format(
					&dfield,
					(byte*)buf,
					TRUE,
					new_mysql_row_col,
					col_pack_len,
					dict_table_is_comp(prebuilt->table));
				dfield_copy_data(&ufield->new_val, &dfield);
			} else {
				dfield_set_null(&ufield->new_val);
			}

			ufield->exp = NULL;
			ufield->orig_len = 0;
			ufield->field_no = dict_col_get_clust_pos(
				&prebuilt->table->cols[i], clust_index);
			n_changed++;
		}
	}

	uvect->n_fields = n_changed;
	uvect->info_bits = 0;

	ut_a(buf <= (byte*)original_upd_buff + buff_len);

	return(0);
}

/**************************************************************************
Updates a row given as a parameter to a new value. Note that we are given
whole rows, not just the fields which are updated: this incurs some
overhead for CPU when we check which fields are actually updated.
TODO: currently InnoDB does not prevent the 'Halloween problem':
in a searched update a single row can get updated several times
if its index columns are updated! */
UNIV_INTERN
int
ha_innobase::update_row(
/*====================*/
					/* out: error number or 0 */
	const uchar*	old_row,	/* in: old row in MySQL format */
	uchar*		new_row)	/* in: new row in MySQL format */
{
	upd_t*		uvect;
	int		error = 0;
	trx_t*		trx = thd_to_trx(user_thd);

	DBUG_ENTER("ha_innobase::update_row");

	ut_a(prebuilt->trx == trx);

	ha_statistic_increment(&SSV::ha_update_count);

	if (table->timestamp_field_type & TIMESTAMP_AUTO_SET_ON_UPDATE)
		table->timestamp_field->set_time();

	if (prebuilt->upd_node) {
		uvect = prebuilt->upd_node->update;
	} else {
		uvect = row_get_prebuilt_update_vector(prebuilt);
	}

	/* Build an update vector from the modified fields in the rows
	(uses upd_buff of the handle) */

	calc_row_difference(uvect, (uchar*) old_row, new_row, table,
			upd_buff, (ulint)upd_and_key_val_buff_len,
			prebuilt, user_thd);

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
	    && (trx->duplicates & (TRX_DUP_IGNORE | TRX_DUP_REPLACE))
		== TRX_DUP_IGNORE)  {

		ulonglong	auto_inc;
		ulonglong	col_max_value;

		auto_inc = table->next_number_field->val_int();

		/* We need the upper limit of the col type to check for
		whether we update the table autoinc counter or not. */
		col_max_value = innobase_get_int_col_max_value(
			table->next_number_field);

		if (auto_inc <= col_max_value && auto_inc != 0) {

			ulonglong	need;
			ulonglong	offset;

			offset = prebuilt->autoinc_offset;
			need = prebuilt->autoinc_increment;

			auto_inc = innobase_next_autoinc(
				auto_inc, need, offset, col_max_value);

			error = innobase_set_max_autoinc(auto_inc);
		}
	}

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

	DBUG_RETURN(error);
}

/**************************************************************************
Deletes a row given as the parameter. */
UNIV_INTERN
int
ha_innobase::delete_row(
/*====================*/
				/* out: error number or 0 */
	const uchar*	record)	/* in: a row in MySQL format */
{
	int		error = 0;
	trx_t*		trx = thd_to_trx(user_thd);

	DBUG_ENTER("ha_innobase::delete_row");

	ut_a(prebuilt->trx == trx);

	ha_statistic_increment(&SSV::ha_delete_count);

	if (!prebuilt->upd_node) {
		row_get_prebuilt_update_vector(prebuilt);
	}

	/* This is a delete */

	prebuilt->upd_node->is_delete = TRUE;

	innodb_srv_conc_enter_innodb(trx);

	error = row_update_for_mysql((byte*) record, prebuilt);

	innodb_srv_conc_exit_innodb(trx);

	error = convert_error_code_to_mysql(
		error, prebuilt->table->flags, user_thd);

	/* Tell the InnoDB server that there might be work for
	utility threads: */

	innobase_active_small();

	DBUG_RETURN(error);
}

/**************************************************************************
Removes a new lock set on a row, if it was not read optimistically. This can
be called after a row has been read in the processing of an UPDATE or a DELETE
query, if the option innodb_locks_unsafe_for_binlog is set. */
UNIV_INTERN
void
ha_innobase::unlock_row(void)
/*=========================*/
{
	DBUG_ENTER("ha_innobase::unlock_row");

	/* Consistent read does not take any locks, thus there is
	nothing to unlock. */

	if (prebuilt->select_lock_type == LOCK_NONE) {
		DBUG_VOID_RETURN;
	}

	switch (prebuilt->row_read_type) {
	case ROW_READ_WITH_LOCKS:
		if (!srv_locks_unsafe_for_binlog
		    && prebuilt->trx->isolation_level
		    != TRX_ISO_READ_COMMITTED) {
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
		|| prebuilt->trx->isolation_level == TRX_ISO_READ_COMMITTED)) {
		prebuilt->row_read_type = ROW_READ_TRY_SEMI_CONSISTENT;
	} else {
		prebuilt->row_read_type = ROW_READ_WITH_LOCKS;
	}
}

#ifdef ROW_MERGE_IS_INDEX_USABLE
/**********************************************************************
Check if an index can be used by the optimizer. */
UNIV_INTERN
bool
ha_innobase::is_index_available(
/*============================*/
					/* out: true if available else false*/
	uint		keynr)		/* in: index number to check */
{
	DBUG_ENTER("ha_innobase::is_index_available");

	if (table && keynr != MAX_KEY && table->s->keys > 0) {
		const dict_index_t*	index;
		const KEY*		key = table->key_info + keynr;

		ut_ad(user_thd == ha_thd());
		ut_a(prebuilt->trx == thd_to_trx(user_thd));

		index = dict_table_get_index_on_name(
			prebuilt->table, key->name);

		if (!row_merge_is_index_usable(prebuilt->trx, index)) {

			DBUG_RETURN(false);
		}
	}

	DBUG_RETURN(true);
}
#endif /* ROW_MERGE_IS_INDEX_USABLE */

/**********************************************************************
Initializes a handle to use an index. */
UNIV_INTERN
int
ha_innobase::index_init(
/*====================*/
			/* out: 0 or error number */
	uint	keynr,	/* in: key (index) number */
	bool sorted)	/* in: 1 if result MUST be sorted according to index */
{
	DBUG_ENTER("index_init");

	DBUG_RETURN(change_active_index(keynr));
}

/**********************************************************************
Currently does nothing. */
UNIV_INTERN
int
ha_innobase::index_end(void)
/*========================*/
{
	int	error	= 0;
	DBUG_ENTER("index_end");
	active_index=MAX_KEY;
	DBUG_RETURN(error);
}

/*************************************************************************
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


/**************************************************************************
Positions an index cursor to the index specified in the handle. Fetches the
row if any. */
UNIV_INTERN
int
ha_innobase::index_read(
/*====================*/
					/* out: 0, HA_ERR_KEY_NOT_FOUND,
					or error number */
	uchar*		buf,		/* in/out: buffer for the returned
					row */
	const uchar*	key_ptr,	/* in: key value; if this is NULL
					we position the cursor at the
					start or end of index; this can
					also contain an InnoDB row id, in
					which case key_len is the InnoDB
					row id length; the key value can
					also be a prefix of a full key value,
					and the last column can be a prefix
					of a full column */
	uint			key_len,/* in: key value length */
	enum ha_rkey_function find_flag)/* in: search flags from my_base.h */
{
	ulint		mode;
	dict_index_t*	index;
	ulint		match_mode	= 0;
	int		error;
	ulint		ret;

	DBUG_ENTER("index_read");

	ut_a(prebuilt->trx == thd_to_trx(user_thd));

	ha_statistic_increment(&SSV::ha_read_key_count);

	index = prebuilt->index;

	/* Note that if the index for which the search template is built is not
	necessarily prebuilt->index, but can also be the clustered index */

	if (prebuilt->sql_stat_start) {
		build_template(prebuilt, user_thd, table, ROW_MYSQL_REC_FIELDS);
	}

	if (key_ptr) {
		/* Convert the search key value to InnoDB format into
		prebuilt->search_tuple */

		row_sel_convert_mysql_key_to_innobase(
			prebuilt->search_tuple,
			(byte*) key_val_buff,
			(ulint)upd_and_key_val_buff_len,
			index,
			(byte*) key_ptr,
			(ulint) key_len,
			prebuilt->trx);
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

	switch (ret) {
	case DB_SUCCESS:
		error = 0;
		table->status = 0;
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

/***********************************************************************
The following functions works like index_read, but it find the last
row with the current key value or prefix. */
UNIV_INTERN
int
ha_innobase::index_read_last(
/*=========================*/
				/* out: 0, HA_ERR_KEY_NOT_FOUND, or an
				error code */
	uchar*		buf,	/* out: fetched row */
	const uchar*	key_ptr,/* in: key value, or a prefix of a full
				key value */
	uint		key_len)/* in: length of the key val or prefix
				in bytes */
{
	return(index_read(buf, key_ptr, key_len, HA_READ_PREFIX_LAST));
}

/************************************************************************
Get the index for a handle. Does not change active index.*/
UNIV_INTERN
dict_index_t*
ha_innobase::innobase_get_index(
/*============================*/
				/* out: NULL or index instance. */
	uint		keynr)	/* in: use this index; MAX_KEY means always
				clustered index, even if it was internally
				generated by InnoDB */
{
	KEY*		key = 0;
	dict_index_t*	index = 0;

	DBUG_ENTER("innobase_get_index");
	ha_statistic_increment(&SSV::ha_read_key_count);

	ut_ad(user_thd == ha_thd());
	ut_a(prebuilt->trx == thd_to_trx(user_thd));

	if (keynr != MAX_KEY && table->s->keys > 0) {
		key = table->key_info + keynr;

		index = dict_table_get_index_on_name(prebuilt->table,
						     key->name);
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

/************************************************************************
Changes the active index of a handle. */
UNIV_INTERN
int
ha_innobase::change_active_index(
/*=============================*/
			/* out: 0 or error code */
	uint	keynr)	/* in: use this index; MAX_KEY means always clustered
			index, even if it was internally generated by
			InnoDB */
{
	DBUG_ENTER("change_active_index");

	ut_ad(user_thd == ha_thd());
	ut_a(prebuilt->trx == thd_to_trx(user_thd));

	active_index = keynr;

	prebuilt->index = innobase_get_index(keynr);

	if (UNIV_UNLIKELY(!prebuilt->index)) {
		sql_print_warning("InnoDB: change_active_index(%u) failed",
				  keynr);
		DBUG_RETURN(1);
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

	build_template(prebuilt, user_thd, table, ROW_MYSQL_REC_FIELDS);

	DBUG_RETURN(0);
}

/**************************************************************************
Positions an index cursor to the index specified in keynr. Fetches the
row if any. */
/* ??? This is only used to read whole keys ??? */
UNIV_INTERN
int
ha_innobase::index_read_idx(
/*========================*/
					/* out: error number or 0 */
	uchar*		buf,		/* in/out: buffer for the returned
					row */
	uint		keynr,		/* in: use this index */
	const uchar*	key,		/* in: key value; if this is NULL
					we position the cursor at the
					start or end of index */
	uint		key_len,	/* in: key value length */
	enum ha_rkey_function find_flag)/* in: search flags from my_base.h */
{
	if (change_active_index(keynr)) {

		return(1);
	}

	return(index_read(buf, key, key_len, find_flag));
}

/***************************************************************************
Reads the next or previous row from a cursor, which must have previously been
positioned using index_read. */
UNIV_INTERN
int
ha_innobase::general_fetch(
/*=======================*/
				/* out: 0, HA_ERR_END_OF_FILE, or error
				number */
	uchar*	buf,		/* in/out: buffer for next row in MySQL
				format */
	uint	direction,	/* in: ROW_SEL_NEXT or ROW_SEL_PREV */
	uint	match_mode)	/* in: 0, ROW_SEL_EXACT, or
				ROW_SEL_EXACT_PREFIX */
{
	ulint		ret;
	int		error	= 0;

	DBUG_ENTER("general_fetch");

	ut_a(prebuilt->trx == thd_to_trx(user_thd));

	innodb_srv_conc_enter_innodb(prebuilt->trx);

	ret = row_search_for_mysql(
		(byte*)buf, 0, prebuilt, match_mode, direction);

	innodb_srv_conc_exit_innodb(prebuilt->trx);

	switch (ret) {
	case DB_SUCCESS:
		error = 0;
		table->status = 0;
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

/***************************************************************************
Reads the next row from a cursor, which must have previously been
positioned using index_read. */
UNIV_INTERN
int
ha_innobase::index_next(
/*====================*/
				/* out: 0, HA_ERR_END_OF_FILE, or error
				number */
	uchar*		buf)	/* in/out: buffer for next row in MySQL
				format */
{
	ha_statistic_increment(&SSV::ha_read_next_count);

	return(general_fetch(buf, ROW_SEL_NEXT, 0));
}

/***********************************************************************
Reads the next row matching to the key value given as the parameter. */
UNIV_INTERN
int
ha_innobase::index_next_same(
/*=========================*/
				/* out: 0, HA_ERR_END_OF_FILE, or error
				number */
	uchar*		buf,	/* in/out: buffer for the row */
	const uchar*	key,	/* in: key value */
	uint		keylen)	/* in: key value length */
{
	ha_statistic_increment(&SSV::ha_read_next_count);

	return(general_fetch(buf, ROW_SEL_NEXT, last_match_mode));
}

/***************************************************************************
Reads the previous row from a cursor, which must have previously been
positioned using index_read. */
UNIV_INTERN
int
ha_innobase::index_prev(
/*====================*/
			/* out: 0, HA_ERR_END_OF_FILE, or error number */
	uchar*	buf)	/* in/out: buffer for previous row in MySQL format */
{
	ha_statistic_increment(&SSV::ha_read_prev_count);

	return(general_fetch(buf, ROW_SEL_PREV, 0));
}

/************************************************************************
Positions a cursor on the first record in an index and reads the
corresponding row to buf. */
UNIV_INTERN
int
ha_innobase::index_first(
/*=====================*/
			/* out: 0, HA_ERR_END_OF_FILE, or error code */
	uchar*	buf)	/* in/out: buffer for the row */
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

/************************************************************************
Positions a cursor on the last record in an index and reads the
corresponding row to buf. */
UNIV_INTERN
int
ha_innobase::index_last(
/*====================*/
			/* out: 0, HA_ERR_END_OF_FILE, or error code */
	uchar*	buf)	/* in/out: buffer for the row */
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

/********************************************************************
Initialize a table scan. */
UNIV_INTERN
int
ha_innobase::rnd_init(
/*==================*/
			/* out: 0 or error number */
	bool	scan)	/* in: TRUE if table/index scan FALSE otherwise */
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

/*********************************************************************
Ends a table scan. */
UNIV_INTERN
int
ha_innobase::rnd_end(void)
/*======================*/
				/* out: 0 or error number */
{
	return(index_end());
}

/*********************************************************************
Reads the next row in a table scan (also used to read the FIRST row
in a table scan). */
UNIV_INTERN
int
ha_innobase::rnd_next(
/*==================*/
			/* out: 0, HA_ERR_END_OF_FILE, or error number */
	uchar*	buf)	/* in/out: returns the row in this buffer,
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

/**************************************************************************
Fetches a row from the table based on a row reference. */
UNIV_INTERN
int
ha_innobase::rnd_pos(
/*=================*/
			/* out: 0, HA_ERR_KEY_NOT_FOUND, or error code */
	uchar*	buf,	/* in/out: buffer for the row */
	uchar*	pos)	/* in: primary key value of the row in the
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

/*************************************************************************
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
	const uchar*	record)	/* in: row in MySQL format */
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

/*********************************************************************
Creates a table definition to an InnoDB database. */
static
int
create_table_def(
/*=============*/
	trx_t*		trx,		/* in: InnoDB transaction handle */
	TABLE*		form,		/* in: information on table
					columns and indexes */
	const char*	table_name,	/* in: table name */
	const char*	path_of_temp_table,/* in: if this is a table explicitly
					created by the user with the
					TEMPORARY keyword, then this
					parameter is the dir path where the
					table should be placed if we create
					an .ibd file for it (no .ibd extension
					in the path, though); otherwise this
					is NULL */
	ulint		flags)		/* in: table flags */
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
	if (IS_MAGIC_TABLE_AND_USER_DENIED_ACCESS(table_name,
						  (THD*) trx->mysql_thd)) {
		DBUG_RETURN(HA_ERR_GENERIC);
	}

	n_cols = form->s->fields;

	/* We pass 0 as the space id, and determine at a lower level the space
	id where to store the table */

	table = dict_mem_table_create(table_name, 0, n_cols, flags);

	if (path_of_temp_table) {
		table->dir_path_of_temp_table =
			mem_heap_strdup(table->heap, path_of_temp_table);
	}

	for (i = 0; i < n_cols; i++) {
		field = form->field[i];

		col_type = get_innobase_type_from_mysql_type(&unsigned_type,
									field);
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
					MYSQL_ERROR::WARN_LEVEL_ERROR,
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

	error = convert_error_code_to_mysql(error, flags, NULL);

	DBUG_RETURN(error);
}

/*********************************************************************
Creates an index in an InnoDB database. */
static
int
create_index(
/*=========*/
	trx_t*		trx,		/* in: InnoDB transaction handle */
	TABLE*		form,		/* in: information on table
					columns and indexes */
	ulint		flags,		/* in: InnoDB table flags */
	const char*	table_name,	/* in: table name */
	uint		key_num)	/* in: index number */
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

	my_free(field_lengths, MYF(0));

	DBUG_RETURN(error);
}

/*********************************************************************
Creates an index to an InnoDB table when the user has defined no
primary index. */
static
int
create_clustered_index_when_no_primary(
/*===================================*/
	trx_t*		trx,		/* in: InnoDB transaction handle */
	ulint		flags,		/* in: InnoDB table flags */
	const char*	table_name)	/* in: table name */
{
	dict_index_t*	index;
	int		error;

	/* We pass 0 as the space id, and determine at a lower level the space
	id where to store the table */

	index = dict_mem_index_create(table_name, "GEN_CLUST_INDEX",
				      0, DICT_CLUSTERED, 0);

	error = row_create_index_for_mysql(index, trx, NULL);

	error = convert_error_code_to_mysql(error, flags, NULL);

	return(error);
}

/*********************************************************************
Validates the create options. We may build on this function
in future. For now, it checks two specifiers:
KEY_BLOCK_SIZE and ROW_FORMAT
If innodb_strict_mode is not set then this function is a no-op */
static
ibool
create_options_are_valid(
/*=====================*/
					/* out: TRUE if valid. */
	THD*		thd,		/* in: connection thread. */
	TABLE*		form,		/* in: information on table
					columns and indexes */
	HA_CREATE_INFO*	create_info)	/* in: create info. */
{
	ibool 	kbs_specified	= FALSE;
	ibool	ret		= TRUE;


	ut_ad(thd != NULL);

	/* If innodb_strict_mode is not set don't do any validation. */
	if (!(THDVAR(thd, strict_mode))) {
		return(TRUE);
	}

	ut_ad(form != NULL);
	ut_ad(create_info != NULL);

	/* First check if KEY_BLOCK_SIZE was specified. */
	if (create_info->key_block_size
	    || (create_info->used_fields & HA_CREATE_USED_KEY_BLOCK_SIZE)) {

		kbs_specified = TRUE;
		switch (create_info->key_block_size) {
		case 1:
		case 2:
		case 4:
		case 8:
		case 16:
			/* Valid value. */
			break;
		default:
			push_warning_printf(thd, MYSQL_ERROR::WARN_LEVEL_ERROR,
					    ER_ILLEGAL_HA_CREATE_OPTION,
					    "InnoDB: invalid"
					    " KEY_BLOCK_SIZE = %lu."
					    " Valid values are"
					    " [1, 2, 4, 8, 16]",
					    create_info->key_block_size);
			ret = FALSE;
		}
	}
	
	/* If KEY_BLOCK_SIZE was specified, check for its
	dependencies. */
	if (kbs_specified && !srv_file_per_table) {
		push_warning(thd, MYSQL_ERROR::WARN_LEVEL_ERROR,
			     ER_ILLEGAL_HA_CREATE_OPTION,
			     "InnoDB: KEY_BLOCK_SIZE"
			     " requires innodb_file_per_table.");
		ret = FALSE;
	}

	if (kbs_specified && srv_file_format < DICT_TF_FORMAT_ZIP) {
		push_warning(thd, MYSQL_ERROR::WARN_LEVEL_ERROR,
			     ER_ILLEGAL_HA_CREATE_OPTION,
			     "InnoDB: KEY_BLOCK_SIZE"
			     " requires innodb_file_format >"
			     " Antelope.");
		ret = FALSE;
	}

	/* Now check for ROW_FORMAT specifier. */
	if (create_info->used_fields & HA_CREATE_USED_ROW_FORMAT) {
		switch (form->s->row_type) {
			const char* row_format_name;
		case ROW_TYPE_COMPRESSED:
		case ROW_TYPE_DYNAMIC:
			row_format_name
				= form->s->row_type == ROW_TYPE_COMPRESSED
				? "COMPRESSED"
				: "DYNAMIC";

			/* These two ROW_FORMATs require
			srv_file_per_table and srv_file_format */
			if (!srv_file_per_table) {
				push_warning_printf(
					thd,
					MYSQL_ERROR::WARN_LEVEL_ERROR,
					ER_ILLEGAL_HA_CREATE_OPTION,
					"InnoDB: ROW_FORMAT=%s"
					" requires innodb_file_per_table.",
					row_format_name);
					ret = FALSE;

			}

			if (srv_file_format < DICT_TF_FORMAT_ZIP) {
				push_warning_printf(
					thd,
					MYSQL_ERROR::WARN_LEVEL_ERROR,
					ER_ILLEGAL_HA_CREATE_OPTION,
					"InnoDB: ROW_FORMAT=%s"
					" requires innodb_file_format >"
					" Antelope.",
					row_format_name);
					ret = FALSE;
			}

			/* Cannot specify KEY_BLOCK_SIZE with
			ROW_FORMAT = DYNAMIC.
			However, we do allow COMPRESSED to be
			specified with KEY_BLOCK_SIZE. */
			if (kbs_specified
			    && form->s->row_type == ROW_TYPE_DYNAMIC) {
				push_warning_printf(
					thd,
					MYSQL_ERROR::WARN_LEVEL_ERROR,
					ER_ILLEGAL_HA_CREATE_OPTION,
					"InnoDB: cannot specify"
					" ROW_FORMAT = DYNAMIC with"
					" KEY_BLOCK_SIZE.");
					ret = FALSE;
			}

			break;

		case ROW_TYPE_REDUNDANT:
		case ROW_TYPE_COMPACT:
		case ROW_TYPE_DEFAULT:
			/* Default is COMPACT. */
			row_format_name
				= form->s->row_type == ROW_TYPE_REDUNDANT
				? "REDUNDANT"
				: "COMPACT";

			/* Cannot specify KEY_BLOCK_SIZE with these
			format specifiers. */
			if (kbs_specified) {
				push_warning_printf(
					thd,
					MYSQL_ERROR::WARN_LEVEL_ERROR,
					ER_ILLEGAL_HA_CREATE_OPTION,
					"InnoDB: cannot specify"
					" ROW_FORMAT = %s with"
					" KEY_BLOCK_SIZE.",
					row_format_name);
					ret = FALSE;
			}

			break;

		default:
			push_warning(thd,
				     MYSQL_ERROR::WARN_LEVEL_ERROR,
				     ER_ILLEGAL_HA_CREATE_OPTION,
				     "InnoDB: invalid ROW_FORMAT specifier.");
			ret = FALSE;

		}
	}

	return(ret);
}

/*********************************************************************
Update create_info.  Used in SHOW CREATE TABLE et al. */
UNIV_INTERN
void
ha_innobase::update_create_info(
/*============================*/
	HA_CREATE_INFO* create_info)	/* in/out: create info */
{
  if (!(create_info->used_fields & HA_CREATE_USED_AUTO)) {
    ha_innobase::info(HA_STATUS_AUTO);
    create_info->auto_increment_value = stats.auto_increment_value;
  }
}

/*********************************************************************
Creates a new table to an InnoDB database. */
UNIV_INTERN
int
ha_innobase::create(
/*================*/
					/* out: error number */
	const char*	name,		/* in: table name */
	TABLE*		form,		/* in: information on table
					columns and indexes */
	HA_CREATE_INFO*	create_info)	/* in: more information of the
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

	DBUG_ENTER("ha_innobase::create");

	DBUG_ASSERT(thd != NULL);
	DBUG_ASSERT(create_info != NULL);

#ifdef __WIN__
	/* Names passed in from server are in two formats:
	1. <database_name>/<table_name>: for normal table creation
	2. full path: for temp table creation, or sym link

	When srv_file_per_table is on, check for full path pattern, i.e.
	X:\dir\...,		X is a driver letter, or
	\\dir1\dir2\...,	UNC path
	returns error if it is in full path format, but not creating a temp.
	table. Currently InnoDB does not support symbolic link on Windows. */

	if (srv_file_per_table
	    && (!create_info->options & HA_LEX_CREATE_TMP_TABLE)) {

		if ((name[1] == ':')
		    || (name[0] == '\\' && name[1] == '\\')) {
			sql_print_error("Cannot create table %s\n", name);
			DBUG_RETURN(HA_ERR_GENERIC);
		}
	}
#endif

	if (form->s->fields > 1000) {
		/* The limit probably should be REC_MAX_N_FIELDS - 3 = 1020,
		but we play safe here */

		DBUG_RETURN(HA_ERR_TO_BIG_ROW);
	}

	/* Get the transaction associated with the current thd, or create one
	if not yet created */

	parent_trx = check_trx_exists(thd);

	/* In case MySQL calls this in the middle of a SELECT query, release
	possible adaptive hash latch to avoid deadlocks of threads */

	trx_search_latch_release_if_reserved(parent_trx);

	trx = innobase_trx_allocate(thd);

	if (lower_case_table_names) {
		srv_lower_case_table_names = TRUE;
	} else {
		srv_lower_case_table_names = FALSE;
	}

	strcpy(name2, name);

	normalize_table_name(norm_name, name2);

	/* Latch the InnoDB data dictionary exclusively so that no deadlocks
	or lock waits can happen in it during a table create operation.
	Drop table etc. do this latching in row0mysql.c. */

	row_mysql_lock_data_dictionary(trx);

	/* Create the table definition in InnoDB */

	flags = 0;

	/* Validate create options if innodb_strict_mode is set. */
	if (!create_options_are_valid(thd, form, create_info)) {
		error = ER_ILLEGAL_HA_CREATE_OPTION;
		goto cleanup;
	}

	if (create_info->key_block_size
	    || (create_info->used_fields & HA_CREATE_USED_KEY_BLOCK_SIZE)) {
		/* Determine the page_zip.ssize corresponding to the
		requested page size (key_block_size) in kilobytes. */

		ulint	ssize, ksize;
		ulint	key_block_size = create_info->key_block_size;

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
			push_warning(thd, MYSQL_ERROR::WARN_LEVEL_WARN,
				     ER_ILLEGAL_HA_CREATE_OPTION,
				     "InnoDB: KEY_BLOCK_SIZE"
				     " requires innodb_file_per_table.");
			flags = 0;
		}

		if (file_format < DICT_TF_FORMAT_ZIP) {
			push_warning(thd, MYSQL_ERROR::WARN_LEVEL_WARN,
				     ER_ILLEGAL_HA_CREATE_OPTION,
				     "InnoDB: KEY_BLOCK_SIZE"
				     " requires innodb_file_format >"
				     " Antelope.");
			flags = 0;
		}

		if (!flags) {
			push_warning_printf(thd, MYSQL_ERROR::WARN_LEVEL_WARN,
					    ER_ILLEGAL_HA_CREATE_OPTION,
					    "InnoDB: ignoring"
					    " KEY_BLOCK_SIZE=%lu.",
					    create_info->key_block_size);
		}
	}

	if (create_info->used_fields & HA_CREATE_USED_ROW_FORMAT) {
		if (flags) {
			/* KEY_BLOCK_SIZE was specified. */
			if (form->s->row_type != ROW_TYPE_COMPRESSED) {
				/* ROW_FORMAT other than COMPRESSED
				ignores KEY_BLOCK_SIZE.  It does not
				make sense to reject conflicting
				KEY_BLOCK_SIZE and ROW_FORMAT, because
				such combinations can be obtained
				with ALTER TABLE anyway. */
				push_warning_printf(
					thd,
					MYSQL_ERROR::WARN_LEVEL_WARN,
					ER_ILLEGAL_HA_CREATE_OPTION,
					"InnoDB: ignoring KEY_BLOCK_SIZE=%lu"
					" unless ROW_FORMAT=COMPRESSED.",
					create_info->key_block_size);
				flags = 0;
			}
		} else {
			/* No KEY_BLOCK_SIZE */
			if (form->s->row_type == ROW_TYPE_COMPRESSED) {
				/* ROW_FORMAT=COMPRESSED without
				KEY_BLOCK_SIZE implies half the
				maximum KEY_BLOCK_SIZE. */
				flags = (DICT_TF_ZSSIZE_MAX - 1)
					<< DICT_TF_ZSSIZE_SHIFT
					| DICT_TF_COMPACT
					| DICT_TF_FORMAT_ZIP
					<< DICT_TF_FORMAT_SHIFT;
#if DICT_TF_ZSSIZE_MAX < 1
# error "DICT_TF_ZSSIZE_MAX < 1"
#endif
			}
		}

		switch (form->s->row_type) {
			const char* row_format_name;
		case ROW_TYPE_REDUNDANT:
			break;
		case ROW_TYPE_COMPRESSED:
		case ROW_TYPE_DYNAMIC:
			row_format_name
				= form->s->row_type == ROW_TYPE_COMPRESSED
				? "COMPRESSED"
				: "DYNAMIC";

			if (!srv_file_per_table) {
				push_warning_printf(
					thd,
					MYSQL_ERROR::WARN_LEVEL_WARN,
					ER_ILLEGAL_HA_CREATE_OPTION,
					"InnoDB: ROW_FORMAT=%s"
					" requires innodb_file_per_table.",
					row_format_name);
			} else if (file_format < DICT_TF_FORMAT_ZIP) {
				push_warning_printf(
					thd,
					MYSQL_ERROR::WARN_LEVEL_WARN,
					ER_ILLEGAL_HA_CREATE_OPTION,
					"InnoDB: ROW_FORMAT=%s"
					" requires innodb_file_format >"
					" Antelope.",
					row_format_name);
			} else {
				flags |= DICT_TF_COMPACT
					| (DICT_TF_FORMAT_ZIP
					   << DICT_TF_FORMAT_SHIFT);
				break;
			}

			/* fall through */
		case ROW_TYPE_NOT_USED:
		case ROW_TYPE_FIXED:
		default:
			push_warning(thd,
				     MYSQL_ERROR::WARN_LEVEL_WARN,
				     ER_ILLEGAL_HA_CREATE_OPTION,
				     "InnoDB: assuming ROW_FORMAT=COMPACT.");
		case ROW_TYPE_DEFAULT:
		case ROW_TYPE_COMPACT:
			flags = DICT_TF_COMPACT;
			break;
		}
	} else if (!flags) {
		/* No KEY_BLOCK_SIZE or ROW_FORMAT specified:
		use ROW_FORMAT=COMPACT by default. */
		flags = DICT_TF_COMPACT;
	}

	error = create_table_def(trx, form, norm_name,
		create_info->options & HA_LEX_CREATE_TMP_TABLE ? name2 : NULL,
		flags);

	if (error) {
		goto cleanup;
	}

	/* Look for a primary key */

	primary_key_no= (form->s->primary_key != MAX_KEY ?
			 (int) form->s->primary_key :
			 -1);

	/* Our function row_get_mysql_key_number_for_index assumes
	the primary key is always number 0, if it exists */

	DBUG_ASSERT(primary_key_no == -1 || primary_key_no == 0);

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

			if ((error = create_index(trx, form, flags, norm_name,
						  i))) {
				goto cleanup;
			}
		}
	}

	if (*trx->mysql_query_str) {
		error = row_table_add_foreign_constraints(trx,
			*trx->mysql_query_str, norm_name,
			create_info->options & HA_LEX_CREATE_TMP_TABLE);

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

	innobase_table = dict_table_get(norm_name, FALSE);

	DBUG_ASSERT(innobase_table != 0);

	if (innobase_table) {
		/* We update the highest file format in the system table
		space, if this table has higher file format setting. */

		trx_sys_file_format_max_upgrade(
			(const char**) &innobase_file_format_check,
			dict_table_get_format(innobase_table));
	}

	/* Note: We can't call update_thd() as prebuilt will not be
	setup at this stage and so we use thd. */

	/* We need to copy the AUTOINC value from the old table if
	this is an ALTER TABLE. */

	if (((create_info->used_fields & HA_CREATE_USED_AUTO)
	    || thd_sql_command(thd) == SQLCOM_ALTER_TABLE)
	    && create_info->auto_increment_value != 0) {

		/* Query was ALTER TABLE...AUTO_INCREMENT = x; or
		CREATE TABLE ...AUTO_INCREMENT = x; Find out a table
		definition from the dictionary and get the current value
		of the auto increment field. Set a new value to the
		auto increment field if the value is greater than the
		maximum value in the column. */

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

/*********************************************************************
Discards or imports an InnoDB tablespace. */
UNIV_INTERN
int
ha_innobase::discard_or_import_tablespace(
/*======================================*/
				/* out: 0 == success, -1 == error */
	my_bool discard)	/* in: TRUE if discard, else import */
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
	}

	err = convert_error_code_to_mysql(err, dict_table->flags, NULL);

	DBUG_RETURN(err);
}

/*********************************************************************
Deletes all rows of an InnoDB table. */
UNIV_INTERN
int
ha_innobase::delete_all_rows(void)
/*==============================*/
				/* out: error number */
{
	int		error;

	DBUG_ENTER("ha_innobase::delete_all_rows");

	/* Get the transaction associated with the current thd, or create one
	if not yet created, and update prebuilt->trx */

	update_thd(ha_thd());

	if (thd_sql_command(user_thd) != SQLCOM_TRUNCATE) {
	fallback:
		/* We only handle TRUNCATE TABLE t as a special case.
		DELETE FROM t will have to use ha_innobase::delete_row(),
		because DELETE is transactional while TRUNCATE is not. */
		DBUG_RETURN(my_errno=HA_ERR_WRONG_COMMAND);
	}

	/* Truncate the table in InnoDB */

	error = row_truncate_table_for_mysql(prebuilt->table, prebuilt->trx);
	if (error == DB_ERROR) {
		/* Cannot truncate; resort to ha_innobase::delete_row() */
		goto fallback;
	}

	error = convert_error_code_to_mysql(error, prebuilt->table->flags,
					    NULL);

	DBUG_RETURN(error);
}

/*********************************************************************
Drops a table from an InnoDB database. Before calling this function,
MySQL calls innobase_commit to commit the transaction of the current user.
Then the current user cannot have locks set on the table. Drop table
operation inside InnoDB will remove all locks any user has on the table
inside InnoDB. */
UNIV_INTERN
int
ha_innobase::delete_table(
/*======================*/
				/* out: error number */
	const char*	name)	/* in: table name */
{
	ulint	name_len;
	int	error;
	trx_t*	parent_trx;
	trx_t*	trx;
	THD	*thd = ha_thd();
	char	norm_name[1000];

	DBUG_ENTER("ha_innobase::delete_table");

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

	if (lower_case_table_names) {
		srv_lower_case_table_names = TRUE;
	} else {
		srv_lower_case_table_names = FALSE;
	}

	name_len = strlen(name);

	ut_a(name_len < 1000);

	/* Drop the table in InnoDB */

	error = row_drop_table_for_mysql(norm_name, trx,
					 thd_sql_command(thd)
					 == SQLCOM_DROP_DB);

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

/*********************************************************************
Removes all tables in the named database inside InnoDB. */
static
void
innobase_drop_database(
/*===================*/
			/* out: error number */
        handlerton *hton, /* in: handlerton of Innodb */
	char*	path)	/* in: database path; inside InnoDB the name
			of the last directory in the path is used as
			the database name: for example, in 'mysql/data/test'
			the database name is 'test' */
{
	ulint	len		= 0;
	trx_t*	trx;
	char*	ptr;
	int	error;
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
	trx->mysql_query_str = NULL;
#else
	trx = innobase_trx_allocate(thd);
#endif
	error = row_drop_database_for_mysql(namebuf, trx);
	my_free(namebuf, MYF(0));

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
/*************************************************************************
Renames an InnoDB table. */
static
int
innobase_rename_table(
/*==================*/
				/* out: 0 or error code */
	trx_t*		trx,	/* in: transaction */
	const char*	from,	/* in: old name of the table */
	const char*	to,	/* in: new name of the table */
	ibool		lock_and_commit)
				/* in: TRUE=lock data dictionary and commit */
{
	int	error;
	char*	norm_to;
	char*	norm_from;
	DBUG_ENTER("innobase_rename_table");

	if (lower_case_table_names) {
		srv_lower_case_table_names = TRUE;
	} else {
		srv_lower_case_table_names = FALSE;
	}

	// Magic number 64 arbitrary
	norm_to = (char*) my_malloc(strlen(to) + 64, MYF(0));
	norm_from = (char*) my_malloc(strlen(from) + 64, MYF(0));

	normalize_table_name(norm_to, to);
	normalize_table_name(norm_from, from);

	/* Serialize data dictionary operations with dictionary mutex:
	no deadlocks can occur then in these operations */

	if (lock_and_commit) {
		row_mysql_lock_data_dictionary(trx);
	}

	error = row_rename_table_for_mysql(
		norm_from, norm_to, trx, lock_and_commit);

	if (error != DB_SUCCESS) {
		FILE* ef = dict_foreign_err_file;

		DBUG_PRINT("info", ("rename failed: %d", error));
		fputs("InnoDB: Renaming table ", ef);
		ut_print_name(ef, trx, TRUE, norm_from);
		fputs(" to ", ef);
		ut_print_name(ef, trx, TRUE, norm_to);
		fputs(" failed!\n", ef);
	}

	if (lock_and_commit) {
		row_mysql_unlock_data_dictionary(trx);

		/* Flush the log to reduce probability that the .frm
		files and the InnoDB data dictionary get out-of-sync
		if the user runs with innodb_flush_log_at_trx_commit = 0 */

		log_buffer_flush_to_disk();
	}

	my_free(norm_to, MYF(0));
	my_free(norm_from, MYF(0));

	DBUG_RETURN(error);
}
/*************************************************************************
Renames an InnoDB table. */
UNIV_INTERN
int
ha_innobase::rename_table(
/*======================*/
				/* out: 0 or error code */
	const char*	from,	/* in: old name of the table */
	const char*	to)	/* in: new name of the table */
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

	error = innobase_rename_table(trx, from, to, TRUE);

	/* Tell the InnoDB server that there might be work for
	utility threads: */

	srv_active_wake_master_thread();

	innobase_commit_low(trx);
	trx_free_for_mysql(trx);

	error = convert_error_code_to_mysql(error, 0, NULL);

	DBUG_RETURN(error);
}

/*************************************************************************
Estimates the number of index records in a range. */
UNIV_INTERN
ha_rows
ha_innobase::records_in_range(
/*==========================*/
						/* out: estimated number of
						rows */
	uint			keynr,		/* in: index number */
	key_range		*min_key,	/* in: start key value of the
						   range, may also be 0 */
	key_range		*max_key)	/* in: range end key val, may
						   also be 0 */
{
	KEY*		key;
	dict_index_t*	index;
	uchar*		key_val_buff2	= (uchar*) my_malloc(
						  table->s->reclength
					+ table->s->max_key_length + 100,
								MYF(MY_FAE));
	ulint		buff2_len = table->s->reclength
					+ table->s->max_key_length + 100;
	dtuple_t*	range_start;
	dtuple_t*	range_end;
	ib_int64_t	n_rows;
	ulint		mode1;
	ulint		mode2;
	mem_heap_t*	heap;

	DBUG_ENTER("records_in_range");

	ut_a(prebuilt->trx == thd_to_trx(ha_thd()));

	prebuilt->trx->op_info = (char*)"estimating records in index range";

	/* In case MySQL calls this in the middle of a SELECT query, release
	possible adaptive hash latch to avoid deadlocks of threads */

	trx_search_latch_release_if_reserved(prebuilt->trx);

	active_index = keynr;

	key = table->key_info + active_index;

	index = dict_table_get_index_on_name(prebuilt->table, key->name);

	/* MySQL knows about this index and so we must be able to find it.*/
	ut_a(index);

	heap = mem_heap_create(2 * (key->key_parts * sizeof(dfield_t)
				    + sizeof(dtuple_t)));

	range_start = dtuple_create(heap, key->key_parts);
	dict_index_copy_types(range_start, index, key->key_parts);

	range_end = dtuple_create(heap, key->key_parts);
	dict_index_copy_types(range_end, index, key->key_parts);

	row_sel_convert_mysql_key_to_innobase(
				range_start, (byte*) key_val_buff,
				(ulint)upd_and_key_val_buff_len,
				index,
				(byte*) (min_key ? min_key->key :
					 (const uchar*) 0),
				(ulint) (min_key ? min_key->length : 0),
				prebuilt->trx);

	row_sel_convert_mysql_key_to_innobase(
				range_end, (byte*) key_val_buff2,
				buff2_len, index,
				(byte*) (max_key ? max_key->key :
					 (const uchar*) 0),
				(ulint) (max_key ? max_key->length : 0),
				prebuilt->trx);

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

	my_free(key_val_buff2, MYF(0));

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

/*************************************************************************
Gives an UPPER BOUND to the number of rows in a table. This is used in
filesort.cc. */
UNIV_INTERN
ha_rows
ha_innobase::estimate_rows_upper_bound(void)
/*======================================*/
			/* out: upper bound of rows */
{
	dict_index_t*	index;
	ulonglong	estimate;
	ulonglong	local_data_file_length;

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

	ut_a(index->stat_n_leaf_pages > 0);

	local_data_file_length =
		((ulonglong) index->stat_n_leaf_pages) * UNIV_PAGE_SIZE;


	/* Calculate a minimum length for a clustered index record and from
	that an upper bound for the number of rows. Since we only calculate
	new statistics in row0mysql.c when a table has grown by a threshold
	factor, we must add a safety factor 2 in front of the formula below. */

	estimate = 2 * local_data_file_length /
					 dict_index_calc_min_rec_len(index);

	prebuilt->trx->op_info = (char*)"";

	DBUG_RETURN((ha_rows) estimate);
}

/*************************************************************************
How many seeks it will take to read through the table. This is to be
comparable to the number returned by records_in_range so that we can
decide if we should scan the table or use keys. */
UNIV_INTERN
double
ha_innobase::scan_time()
/*====================*/
			/* out: estimated time measured in disk seeks */
{
	/* Since MySQL seems to favor table scans too much over index
	searches, we pretend that a sequential read takes the same time
	as a random disk read, that is, we do not divide the following
	by 10, which would be physically realistic. */

	return((double) (prebuilt->table->stat_clustered_index_size));
}

/**********************************************************************
Calculate the time it takes to read a set of ranges through an index
This enables us to optimise reads for clustered indexes. */
UNIV_INTERN
double
ha_innobase::read_time(
/*===================*/
			/* out: estimated time measured in disk seeks */
	uint	index,	/* in: key number */
	uint	ranges,	/* in: how many ranges */
	ha_rows rows)	/* in: estimated number of rows in the ranges */
{
	ha_rows total_rows;
	double	time_for_scan;

	if (index != table->s->primary_key) {
		/* Not clustered */
		return(handler::read_time(index, ranges, rows));
	}

	if (rows <= 2) {

		return((double) rows);
	}

	/* Assume that the read time is proportional to the scan time for all
	rows + at most one seek per range. */

	time_for_scan = scan_time();

	if ((total_rows = estimate_rows_upper_bound()) < rows) {

		return(time_for_scan);
	}

	return(ranges + (double) rows / (double) total_rows * time_for_scan);
}

/*************************************************************************
Returns statistics information of the table to the MySQL interpreter,
in various fields of the handle object. */
UNIV_INTERN
int
ha_innobase::info(
/*==============*/
	uint flag)	/* in: what information MySQL requests */
{
	dict_table_t*	ib_table;
	dict_index_t*	index;
	ha_rows		rec_per_key;
	ib_int64_t	n_rows;
	ulong		j;
	ulong		i;
	char		path[FN_REFLEN];
	os_file_stat_t	stat_info;

	DBUG_ENTER("info");

	/* If we are forcing recovery at a high level, we will suppress
	statistics calculation on tables, because that may crash the
	server if an index is badly corrupted. */

	if (srv_force_recovery >= SRV_FORCE_NO_IBUF_MERGE) {

		/* We return success (0) instead of HA_ERR_CRASHED,
		because we want MySQL to process this query and not
		stop, like it would do if it received the error code
		HA_ERR_CRASHED. */

		DBUG_RETURN(0);
	}

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
		if (innobase_stats_on_metadata
		    && (thd_sql_command(user_thd) == SQLCOM_ANALYZE
			|| srv_stats_auto_update)) {
			/* In sql_show we call with this flag: update
			then statistics so that they are up-to-date */

			prebuilt->trx->op_info = "updating table statistics";

			dict_update_statistics(ib_table);

			prebuilt->trx->op_info = "returning various info to MySQL";
		}

		my_snprintf(path, sizeof(path), "%s/%s%s",
				mysql_data_home, ib_table->name, reg_ext);

		unpack_filename(path,path);

		/* Note that we do not know the access time of the table,
		nor the CHECK TABLE time, nor the UPDATE or INSERT time. */

		if (os_file_get_status(path,&stat_info)) {
			stats.create_time = stat_info.ctime;
		}
	}

	if (flag & HA_STATUS_VARIABLE) {
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

		stats.records = (ha_rows)n_rows;
		stats.deleted = 0;
		stats.data_file_length = ((ulonglong)
				ib_table->stat_clustered_index_size)
					* UNIV_PAGE_SIZE;
		stats.index_file_length = ((ulonglong)
				ib_table->stat_sum_of_other_index_sizes)
					* UNIV_PAGE_SIZE;

		/* Since fsp_get_available_space_in_free_extents() is
		acquiring latches inside InnoDB, we do not call it if we
		are asked by MySQL to avoid locking. Another reason to
		avoid the call is that it uses quite a lot of CPU.
		See Bug#38185.
		We do not update delete_length if no locking is requested
		so the "old" value can remain. delete_length is initialized
		to 0 in the ha_statistics' constructor. */
		if (!(flag & HA_STATUS_NO_LOCK)) {

			/* lock the data dictionary to avoid races with
			ibd_file_missing and tablespace_discarded */
			row_mysql_lock_data_dictionary(prebuilt->trx);

			/* ib_table->space must be an existent tablespace */
			if (!ib_table->ibd_file_missing
			    && !ib_table->tablespace_discarded) {

				stats.delete_length =
					fsp_get_available_space_in_free_extents(
						ib_table->space) * 1024;
			} else {

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
			}

			row_mysql_unlock_data_dictionary(prebuilt->trx);
		}

		stats.check_time = 0;

		if (stats.records == 0) {
			stats.mean_rec_length = 0;
		} else {
			stats.mean_rec_length = (ulong) (stats.data_file_length / stats.records);
		}
	}

	if (flag & HA_STATUS_CONST) {
		index = dict_table_get_first_index(ib_table);

		if (prebuilt->clust_index_was_generated) {
			index = dict_table_get_next_index(index);
		}

		for (i = 0; i < table->s->keys; i++) {
			if (index == NULL) {
				sql_print_error("Table %s contains fewer "
						"indexes inside InnoDB than "
						"are defined in the MySQL "
						".frm file. Have you mixed up "
						".frm files from different "
						"installations? See "
"http://dev.mysql.com/doc/refman/5.1/en/innodb-troubleshooting.html\n",

						ib_table->name);
				break;
			}

			for (j = 0; j < table->key_info[i].key_parts; j++) {

				if (j + 1 > index->n_uniq) {
					sql_print_error(
"Index %s of %s has %lu columns unique inside InnoDB, but MySQL is asking "
"statistics for %lu columns. Have you mixed up .frm files from different "
"installations? "
"See http://dev.mysql.com/doc/refman/5.1/en/innodb-troubleshooting.html\n",
							index->name,
							ib_table->name,
							(unsigned long)
							index->n_uniq, j + 1);
					break;
				}

				if (index->stat_n_diff_key_vals[j + 1] == 0) {

					rec_per_key = stats.records;
				} else {
					rec_per_key = (ha_rows)(stats.records /
					 index->stat_n_diff_key_vals[j + 1]);
				}

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

			index = dict_table_get_next_index(index);
		}
	}

	if (flag & HA_STATUS_ERRKEY) {
		const dict_index_t*	err_index;

		ut_a(prebuilt->trx);
		ut_a(prebuilt->trx->magic_n == TRX_MAGIC_N);

		err_index = trx_get_error_info(prebuilt->trx);

		if (err_index) {
			errkey = (unsigned int)
				row_get_mysql_key_number_for_index(err_index);
		} else {
			errkey = (unsigned int) prebuilt->trx->error_key_num;
		}
	}

	if ((flag & HA_STATUS_AUTO) && table->found_next_number_field) {
		stats.auto_increment_value = innobase_peek_autoinc();
	}

	prebuilt->trx->op_info = (char*)"";

	DBUG_RETURN(0);
}

/**************************************************************************
Updates index cardinalities of the table, based on 8 random dives into
each index tree. This does NOT calculate exact statistics on the table. */
UNIV_INTERN
int
ha_innobase::analyze(
/*=================*/
					/* out: returns always 0 (success) */
	THD*		thd,		/* in: connection thread handle */
	HA_CHECK_OPT*	check_opt)	/* in: currently ignored */
{
	/* Simply call ::info() with all the flags */
	info(HA_STATUS_TIME | HA_STATUS_CONST | HA_STATUS_VARIABLE);

	return(0);
}

/**************************************************************************
This is mapped to "ALTER TABLE tablename ENGINE=InnoDB", which rebuilds
the table in MySQL. */
UNIV_INTERN
int
ha_innobase::optimize(
/*==================*/
	THD*		thd,		/* in: connection thread handle */
	HA_CHECK_OPT*	check_opt)	/* in: currently ignored */
{
	return(HA_ADMIN_TRY_ALTER);
}

/***********************************************************************
Tries to check that an InnoDB table is not corrupted. If corruption is
noticed, prints to stderr information about it. In case of corruption
may also assert a failure and crash the server. */
UNIV_INTERN
int
ha_innobase::check(
/*===============*/
					/* out: HA_ADMIN_CORRUPT or
					HA_ADMIN_OK */
	THD*		thd,		/* in: user thread handle */
	HA_CHECK_OPT*	check_opt)	/* in: check options, currently
					ignored */
{
	ulint		ret;

	DBUG_ASSERT(thd == ha_thd());
	ut_a(prebuilt->trx);
	ut_a(prebuilt->trx->magic_n == TRX_MAGIC_N);
	ut_a(prebuilt->trx == thd_to_trx(thd));

	if (prebuilt->mysql_template == NULL) {
		/* Build the template; we will use a dummy template
		in index scans done in checking */

		build_template(prebuilt, NULL, table, ROW_MYSQL_WHOLE_ROW);
	}

	ret = row_check_table_for_mysql(prebuilt);

	if (ret == DB_SUCCESS) {
		return(HA_ADMIN_OK);
	}

	return(HA_ADMIN_CORRUPT);
}

/*****************************************************************
Adds information about free space in the InnoDB tablespace to a table comment
which is printed out when a user calls SHOW TABLE STATUS. Adds also info on
foreign keys. */
UNIV_INTERN
char*
ha_innobase::update_table_comment(
/*==============================*/
				/* out: table comment + InnoDB free space +
				info on foreign keys */
	const char*	comment)/* in: table comment defined by user */
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

/***********************************************************************
Gets the foreign key create info for a table stored in InnoDB. */
UNIV_INTERN
char*
ha_innobase::get_foreign_key_create_info(void)
/*==========================================*/
			/* out, own: character string in the form which
			can be inserted to the CREATE TABLE statement,
			MUST be freed with ::free_foreign_key_create_info */
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
	} else if (flen > 64000 - 1) {
		flen = 64000 - 1;
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


UNIV_INTERN
int
ha_innobase::get_foreign_key_list(THD *thd, List<FOREIGN_KEY_INFO> *f_key_list)
{
  dict_foreign_t* foreign;

  DBUG_ENTER("get_foreign_key_list");
  ut_a(prebuilt != NULL);
  update_thd(ha_thd());
  prebuilt->trx->op_info = (char*)"getting list of foreign keys";
  trx_search_latch_release_if_reserved(prebuilt->trx);
  mutex_enter(&(dict_sys->mutex));
  foreign = UT_LIST_GET_FIRST(prebuilt->table->foreign_list);

  while (foreign != NULL) {
	  uint i;
	  FOREIGN_KEY_INFO f_key_info;
	  LEX_STRING *name= 0;
          uint ulen;
          char uname[NAME_LEN+1];           /* Unencoded name */
          char db_name[NAME_LEN+1];
	  const char *tmp_buff;

	  tmp_buff= foreign->id;
	  i= 0;
	  while (tmp_buff[i] != '/')
		  i++;
	  tmp_buff+= i + 1;
	  f_key_info.forein_id = thd_make_lex_string(thd, 0,
		  tmp_buff, (uint) strlen(tmp_buff), 1);
	  tmp_buff= foreign->referenced_table_name;

          /* Database name */
	  i= 0;
	  while (tmp_buff[i] != '/')
          {
            db_name[i]= tmp_buff[i];
            i++;
          }
          db_name[i]= 0;
          ulen= filename_to_tablename(db_name, uname, sizeof(uname));
	  f_key_info.referenced_db = thd_make_lex_string(thd, 0,
		  uname, ulen, 1);

          /* Table name */
	  tmp_buff+= i + 1;
          ulen= filename_to_tablename(tmp_buff, uname, sizeof(uname));
	  f_key_info.referenced_table = thd_make_lex_string(thd, 0,
		  uname, ulen, 1);

	  for (i= 0;;) {
		  tmp_buff= foreign->foreign_col_names[i];
		  name = thd_make_lex_string(thd, name,
			  tmp_buff, (uint) strlen(tmp_buff), 1);
		  f_key_info.foreign_fields.push_back(name);
		  tmp_buff= foreign->referenced_col_names[i];
		  name = thd_make_lex_string(thd, name,
			tmp_buff, (uint) strlen(tmp_buff), 1);
		  f_key_info.referenced_fields.push_back(name);
		  if (++i >= foreign->n_fields)
			  break;
	  }

          ulong length;
          if (foreign->type & DICT_FOREIGN_ON_DELETE_CASCADE)
          {
            length=7;
            tmp_buff= "CASCADE";
          }
          else if (foreign->type & DICT_FOREIGN_ON_DELETE_SET_NULL)
          {
            length=8;
            tmp_buff= "SET NULL";
          }
          else if (foreign->type & DICT_FOREIGN_ON_DELETE_NO_ACTION)
          {
            length=9;
            tmp_buff= "NO ACTION";
          }
          else
          {
            length=8;
            tmp_buff= "RESTRICT";
          }
	  f_key_info.delete_method = thd_make_lex_string(
		  thd, f_key_info.delete_method, tmp_buff, length, 1);


          if (foreign->type & DICT_FOREIGN_ON_UPDATE_CASCADE)
          {
            length=7;
            tmp_buff= "CASCADE";
          }
          else if (foreign->type & DICT_FOREIGN_ON_UPDATE_SET_NULL)
          {
            length=8;
            tmp_buff= "SET NULL";
          }
          else if (foreign->type & DICT_FOREIGN_ON_UPDATE_NO_ACTION)
          {
            length=9;
            tmp_buff= "NO ACTION";
          }
          else
          {
            length=8;
            tmp_buff= "RESTRICT";
          }
	  f_key_info.update_method = thd_make_lex_string(
		  thd, f_key_info.update_method, tmp_buff, length, 1);
          if (foreign->referenced_index &&
              foreign->referenced_index->name)
          {
	    f_key_info.referenced_key_name = thd_make_lex_string(
		    thd, f_key_info.referenced_key_name,
		    foreign->referenced_index->name,
		    (uint) strlen(foreign->referenced_index->name), 1);
          }
          else
            f_key_info.referenced_key_name= 0;

	  FOREIGN_KEY_INFO *pf_key_info = (FOREIGN_KEY_INFO *)
		  thd_memdup(thd, &f_key_info, sizeof(FOREIGN_KEY_INFO));
	  f_key_list->push_back(pf_key_info);
	  foreign = UT_LIST_GET_NEXT(foreign_list, foreign);
  }
  mutex_exit(&(dict_sys->mutex));
  prebuilt->trx->op_info = (char*)"";

  DBUG_RETURN(0);
}

/*********************************************************************
Checks if ALTER TABLE may change the storage engine of the table.
Changing storage engines is not allowed for tables for which there
are foreign key constraints (parent or child tables). */
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

/***********************************************************************
Checks if a table is referenced by a foreign key. The MySQL manual states that
a REPLACE is either equivalent to an INSERT, or DELETE(s) + INSERT. Only a
delete is then allowed internally to resolve a duplicate key conflict in
REPLACE, not an update. */
UNIV_INTERN
uint
ha_innobase::referenced_by_foreign_key(void)
/*========================================*/
			/* out: > 0 if referenced by a FOREIGN KEY */
{
	if (dict_table_is_referenced_by_foreign_key(prebuilt->table)) {

		return(1);
	}

	return(0);
}

/***********************************************************************
Frees the foreign key create info for a table stored in InnoDB, if it is
non-NULL. */
UNIV_INTERN
void
ha_innobase::free_foreign_key_create_info(
/*======================================*/
	char*	str)	/* in, own: create info string to free	*/
{
	if (str) {
		my_free(str, MYF(0));
	}
}

/***********************************************************************
Tells something additional to the handler about how to do things. */
UNIV_INTERN
int
ha_innobase::extra(
/*===============*/
			   /* out: 0 or error number */
	enum ha_extra_function operation)
			   /* in: HA_EXTRA_FLUSH or some other flag */
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
			reset_template(prebuilt);
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
		case HA_EXTRA_IGNORE_DUP_KEY:
			thd_to_trx(ha_thd())->duplicates |= TRX_DUP_IGNORE;
			break;
		case HA_EXTRA_WRITE_CAN_REPLACE:
			thd_to_trx(ha_thd())->duplicates |= TRX_DUP_REPLACE;
			break;
		case HA_EXTRA_WRITE_CANNOT_REPLACE:
			thd_to_trx(ha_thd())->duplicates &= ~TRX_DUP_REPLACE;
			break;
		case HA_EXTRA_NO_IGNORE_DUP_KEY:
			thd_to_trx(ha_thd())->duplicates &=
				~(TRX_DUP_IGNORE | TRX_DUP_REPLACE);
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

	reset_template(prebuilt);

	/* TODO: This should really be reset in reset_template() but for now
	it's safer to do it explicitly here. */

	/* This is a statement level counter. */
	prebuilt->autoinc_last_value = 0;

	return(0);
}

/**********************************************************************
MySQL calls this function at the start of each SQL statement inside LOCK
TABLES. Inside LOCK TABLES the ::external_lock method does not work to
mark SQL statement borders. Note also a special case: if a temporary table
is created inside LOCK TABLES, MySQL has not called external_lock() at all
on that table.
MySQL-5.0 also calls this before each statement in an execution of a stored
procedure. To make the execution more deterministic for binlogging, MySQL-5.0
locks all tables involved in a stored procedure with full explicit table
locks (thd_in_lock_tables(thd) holds in store_lock()) before executing the
procedure. */
UNIV_INTERN
int
ha_innobase::start_stmt(
/*====================*/
				/* out: 0 or error code */
	THD*		thd,	/* in: handle to the user thread */
	thr_lock_type	lock_type)
{
	trx_t*		trx;

	update_thd(thd);

	trx = prebuilt->trx;

	/* Here we release the search latch and the InnoDB thread FIFO ticket
	if they were reserved. They should have been released already at the
	end of the previous statement, but because inside LOCK TABLES the
	lock count method does not work to mark the end of a SELECT statement,
	that may not be the case. We MUST release the search latch before an
	INSERT, for example. */

	innobase_release_stat_resources(trx);

	/* Reset the AUTOINC statement level counter for multi-row INSERTs. */
	trx->n_autoinc_rows = 0;

	prebuilt->sql_stat_start = TRUE;
	prebuilt->hint_need_to_fetch_extra_cols = 0;
	reset_template(prebuilt);

	if (!prebuilt->mysql_has_locked) {
		/* This handle is for a temporary table created inside
		this same LOCK TABLES; since MySQL does NOT call external_lock
		in this case, we must use x-row locks inside InnoDB to be
		prepared for an update of a row */

		prebuilt->select_lock_type = LOCK_X;
	} else {
		if (trx->isolation_level != TRX_ISO_SERIALIZABLE
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

			prebuilt->select_lock_type =
				prebuilt->stored_select_lock_type;
		}
	}

	trx->detailed_error[0] = '\0';

	/* Set the MySQL flag to mark that there is an active transaction */
	if (trx->active_trans == 0) {

		innobase_register_trx_and_stmt(ht, thd);
		trx->active_trans = 1;
	} else {
		innobase_register_stmt(ht, thd);
	}

	return(0);
}

/**********************************************************************
Maps a MySQL trx isolation level code to the InnoDB isolation level code */
static inline
ulint
innobase_map_isolation_level(
/*=========================*/
					/* out: InnoDB isolation level */
	enum_tx_isolation	iso)	/* in: MySQL isolation level code */
{
	switch(iso) {
		case ISO_REPEATABLE_READ: return(TRX_ISO_REPEATABLE_READ);
		case ISO_READ_COMMITTED: return(TRX_ISO_READ_COMMITTED);
		case ISO_SERIALIZABLE: return(TRX_ISO_SERIALIZABLE);
		case ISO_READ_UNCOMMITTED: return(TRX_ISO_READ_UNCOMMITTED);
		default: ut_a(0); return(0);
	}
}

/**********************************************************************
As MySQL will execute an external lock for every new table it uses when it
starts to process an SQL statement (an exception is when MySQL calls
start_stmt for the handle) we can use this function to store the pointer to
the THD in the handle. We will also use this function to communicate
to InnoDB that a new SQL statement has started and that we must store a
savepoint to our transaction handle, so that we are able to roll back
the SQL statement in case of an error. */
UNIV_INTERN
int
ha_innobase::external_lock(
/*=======================*/
				/* out: 0 */
	THD*	thd,		/* in: handle to the user thread */
	int	lock_type)	/* in: lock type */
{
	trx_t*		trx;

	DBUG_ENTER("ha_innobase::external_lock");
	DBUG_PRINT("enter",("lock_type: %d", lock_type));

	update_thd(thd);

	/* Statement based binlogging does not work in isolation level
	READ UNCOMMITTED and READ COMMITTED since the necessary
	locks cannot be taken. In this case, we print an
	informative error message and return with an error. */
	if (lock_type == F_WRLCK)
	{
		ulong const binlog_format= thd_binlog_format(thd);
		ulong const tx_isolation = thd_tx_isolation(ha_thd());
		if (tx_isolation <= ISO_READ_COMMITTED &&
		    binlog_format == BINLOG_FORMAT_STMT)
		{
			char buf[256];
			my_snprintf(buf, sizeof(buf),
				    "Transaction level '%s' in"
				    " InnoDB is not safe for binlog mode '%s'",
				    tx_isolation_names[tx_isolation],
				    binlog_format_names[binlog_format]);
			my_error(ER_BINLOG_LOGGING_IMPOSSIBLE, MYF(0), buf);
			DBUG_RETURN(HA_ERR_LOGGING_IMPOSSIBLE);
		}
	}


	trx = prebuilt->trx;

	prebuilt->sql_stat_start = TRUE;
	prebuilt->hint_need_to_fetch_extra_cols = 0;

	reset_template(prebuilt);

	if (lock_type == F_WRLCK) {

		/* If this is a SELECT, then it is in UPDATE TABLE ...
		or SELECT ... FOR UPDATE */
		prebuilt->select_lock_type = LOCK_X;
		prebuilt->stored_select_lock_type = LOCK_X;
	}

	if (lock_type != F_UNLCK) {
		/* MySQL is setting a new table lock */

		trx->detailed_error[0] = '\0';

		/* Set the MySQL flag to mark that there is an active
		transaction */
		if (trx->active_trans == 0) {

			innobase_register_trx_and_stmt(ht, thd);
			trx->active_trans = 1;
		} else if (trx->n_mysql_tables_in_use == 0) {
			innobase_register_stmt(ht, thd);
		}

		if (trx->isolation_level == TRX_ISO_SERIALIZABLE
			&& prebuilt->select_lock_type == LOCK_NONE
			&& thd_test_options(thd,
				OPTION_NOT_AUTOCOMMIT | OPTION_BEGIN)) {

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

	innobase_release_stat_resources(trx);

	/* If the MySQL lock count drops to zero we know that the current SQL
	statement has ended */

	if (trx->n_mysql_tables_in_use == 0) {

		trx->mysql_n_tables_locked = 0;
		prebuilt->used_in_HANDLER = FALSE;

		if (!thd_test_options(thd, OPTION_NOT_AUTOCOMMIT | OPTION_BEGIN)) {
			if (trx->active_trans != 0) {
				innobase_commit(ht, thd, TRUE);
			}
		} else {
			if (trx->isolation_level <= TRX_ISO_READ_COMMITTED
						&& trx->global_read_view) {

				/* At low transaction isolation levels we let
				each consistent read set its own snapshot */

				read_view_close_for_mysql(trx);
			}
		}
	}

	DBUG_RETURN(0);
}

/**********************************************************************
With this function MySQL request a transactional lock to a table when
user issued query LOCK TABLES..WHERE ENGINE = InnoDB. */
UNIV_INTERN
int
ha_innobase::transactional_table_lock(
/*==================================*/
				/* out: error code */
	THD*	thd,		/* in: handle to the user thread */
	int	lock_type)	/* in: lock type */
{
	trx_t*		trx;

	DBUG_ENTER("ha_innobase::transactional_table_lock");
	DBUG_PRINT("enter",("lock_type: %d", lock_type));

	/* We do not know if MySQL can call this function before calling
	external_lock(). To be safe, update the thd of the current table
	handle. */

	update_thd(thd);

	if (prebuilt->table->ibd_file_missing && !thd_tablespace_op(thd)) {
		ut_print_timestamp(stderr);
		fprintf(stderr,
			"  InnoDB: MySQL is trying to use a table handle"
			" but the .ibd file for\n"
			"InnoDB: table %s does not exist.\n"
			"InnoDB: Have you deleted the .ibd file"
			" from the database directory under\n"
			"InnoDB: the MySQL datadir?"
			"InnoDB: See"
			" http://dev.mysql.com/doc/refman/5.1/en/innodb-troubleshooting.html\n"
			"InnoDB: how you can resolve the problem.\n",
			prebuilt->table->name);
		DBUG_RETURN(HA_ERR_CRASHED);
	}

	trx = prebuilt->trx;

	prebuilt->sql_stat_start = TRUE;
	prebuilt->hint_need_to_fetch_extra_cols = 0;

	reset_template(prebuilt);

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

	/* Set the MySQL flag to mark that there is an active transaction */
	if (trx->active_trans == 0) {

		innobase_register_trx_and_stmt(ht, thd);
		trx->active_trans = 1;
	}

	if (THDVAR(thd, table_locks) && thd_in_lock_tables(thd)) {
		ulint	error = DB_SUCCESS;

		error = row_lock_table_for_mysql(prebuilt, NULL, 0);

		if (error != DB_SUCCESS) {
			error = convert_error_code_to_mysql(
				(int) error, prebuilt->table->flags, thd);
			DBUG_RETURN((int) error);
		}

		if (thd_test_options(thd, OPTION_NOT_AUTOCOMMIT | OPTION_BEGIN)) {

			/* Store the current undo_no of the transaction
			so that we know where to roll back if we have
			to roll back the next SQL statement */

			trx_mark_sql_stat_end(trx);
		}
	}

	DBUG_RETURN(0);
}

/****************************************************************************
Here we export InnoDB status variables to MySQL.  */
static
int
innodb_export_status()
/*==================*/
{
	if (innodb_inited) {
		srv_export_innodb_status();
	}

	return(0);
}

/****************************************************************************
Implements the SHOW INNODB STATUS command. Sends the output of the InnoDB
Monitor to the client. */
static
bool
innodb_show_status(
/*===============*/
	handlerton*	hton,	/* in: the innodb handlerton */
	THD*	thd,	/* in: the MySQL query thread of the caller */
	stat_print_fn *stat_print)
{
	trx_t*			trx;
	static const char	truncated_msg[] = "... truncated...\n";
	const long		MAX_STATUS_SIZE = 64000;
	ulint			trx_list_start = ULINT_UNDEFINED;
	ulint			trx_list_end = ULINT_UNDEFINED;

	DBUG_ENTER("innodb_show_status");
	DBUG_ASSERT(hton == innodb_hton_ptr);

	trx = check_trx_exists(thd);

	innobase_release_stat_resources(trx);

	/* We let the InnoDB Monitor to output at most MAX_STATUS_SIZE
	bytes of text. */

	long	flen, usable_len;
	char*	str;

	mutex_enter(&srv_monitor_file_mutex);
	rewind(srv_monitor_file);
	srv_printf_innodb_monitor(srv_monitor_file,
				&trx_list_start, &trx_list_end);
	flen = ftell(srv_monitor_file);
	os_file_set_eof(srv_monitor_file);

	if (flen < 0) {
		flen = 0;
	}

	if (flen > MAX_STATUS_SIZE) {
		usable_len = MAX_STATUS_SIZE;
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

	bool result = FALSE;

	if (stat_print(thd, innobase_hton_name, (uint) strlen(innobase_hton_name),
			STRING_WITH_LEN(""), str, flen)) {
		result= TRUE;
	}
	my_free(str, MYF(0));

	DBUG_RETURN(FALSE);
}

/****************************************************************************
Implements the SHOW MUTEX STATUS command. . */
static
bool
innodb_mutex_show_status(
/*=====================*/
	handlerton*	hton,	/* in: the innodb handlerton */
	THD*		thd,		/* in: the MySQL query thread of the
					caller */
	stat_print_fn*	stat_print)
{
	char buf1[IO_SIZE], buf2[IO_SIZE];
	mutex_t*	mutex;
	rw_lock_t*	lock;
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

	mutex = UT_LIST_GET_FIRST(mutex_list);

	while (mutex != NULL) {
#ifdef UNIV_DEBUG
		if (mutex->mutex_type != 1) {
			if (mutex->count_using > 0) {
				buf1len= my_snprintf(buf1, sizeof(buf1),
					"%s:%s",
					mutex->cmutex_name, mutex->cfile_name);
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
		}
		else {
			rw_lock_count += mutex->count_using;
			rw_lock_count_spin_loop += mutex->count_spin_loop;
			rw_lock_count_spin_rounds += mutex->count_spin_rounds;
			rw_lock_count_os_wait += mutex->count_os_wait;
			rw_lock_count_os_yield += mutex->count_os_yield;
			rw_lock_wait_time += mutex->lspent_time;
		}
#else /* UNIV_DEBUG */
		buf1len= (uint) my_snprintf(buf1, sizeof(buf1), "%s:%lu",
				     mutex->cfile_name, (ulong) mutex->cline);
		buf2len= (uint) my_snprintf(buf2, sizeof(buf2), "os_waits=%lu",
				     mutex->count_os_wait);

		if (stat_print(thd, innobase_hton_name,
			       hton_name_len, buf1, buf1len,
			       buf2, buf2len)) {
			mutex_exit(&mutex_list_mutex);
			DBUG_RETURN(1);
		}
#endif /* UNIV_DEBUG */

		mutex = UT_LIST_GET_NEXT(list, mutex);
	}

	mutex_exit(&mutex_list_mutex);

	mutex_enter(&rw_lock_list_mutex);

	lock = UT_LIST_GET_FIRST(rw_lock_list);

	while (lock != NULL) {
		if (lock->count_os_wait) {
			buf1len= my_snprintf(buf1, sizeof(buf1), "%s:%lu",
                                    lock->cfile_name, (ulong) lock->cline);
			buf2len= my_snprintf(buf2, sizeof(buf2),
                                    "os_waits=%lu", lock->count_os_wait);

			if (stat_print(thd, innobase_hton_name,
				       hton_name_len, buf1, buf1len,
				       buf2, buf2len)) {
				mutex_exit(&rw_lock_list_mutex);
				DBUG_RETURN(1);
			}
		}
		lock = UT_LIST_GET_NEXT(list, lock);
	}

	mutex_exit(&rw_lock_list_mutex);

#ifdef UNIV_DEBUG
	buf2len= my_snprintf(buf2, sizeof(buf2),
		"count=%lu, spin_waits=%lu, spin_rounds=%lu, "
		"os_waits=%lu, os_yields=%lu, os_wait_times=%lu",
		rw_lock_count, rw_lock_count_spin_loop,
		rw_lock_count_spin_rounds,
		rw_lock_count_os_wait, rw_lock_count_os_yield,
		(ulong) (rw_lock_wait_time/1000));

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

/****************************************************************************
 Handling the shared INNOBASE_SHARE structure that is needed to provide table
 locking.
****************************************************************************/

static INNOBASE_SHARE* get_share(const char* table_name)
{
	INNOBASE_SHARE *share;
	pthread_mutex_lock(&innobase_share_mutex);

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
		pthread_mutex_init(&share->mutex,MY_MUTEX_INIT_FAST);
	}

	share->use_count++;
	pthread_mutex_unlock(&innobase_share_mutex);

	return(share);
}

static void free_share(INNOBASE_SHARE* share)
{
	pthread_mutex_lock(&innobase_share_mutex);

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
		pthread_mutex_destroy(&share->mutex);
		my_free(share, MYF(0));

		/* TODO: invoke HASH_MIGRATE if innobase_open_tables
		shrinks too much */
	}

	pthread_mutex_unlock(&innobase_share_mutex);
}

/*********************************************************************
Converts a MySQL table lock stored in the 'lock' field of the handle to
a proper type before storing pointer to the lock into an array of pointers.
MySQL also calls this if it wants to reset some table locks to a not-locked
state during the processing of an SQL query. An example is that during a
SELECT the read lock is released early on the 'const' tables where we only
fetch one row. MySQL does not call this when it releases all locks at the
end of an SQL statement. */
UNIV_INTERN
THR_LOCK_DATA**
ha_innobase::store_lock(
/*====================*/
						/* out: pointer to the next
						element in the 'to' array */
	THD*			thd,		/* in: user thread handle */
	THR_LOCK_DATA**		to,		/* in: pointer to an array
						of pointers to lock structs;
						pointer to the 'lock' field
						of current handle is stored
						next to this array */
	enum thr_lock_type	lock_type)	/* in: lock type to store in
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
		     || isolation_level == TRX_ISO_READ_COMMITTED)
		    && isolation_level != TRX_ISO_SERIALIZABLE
		    && (lock_type == TL_READ || lock_type == TL_READ_NO_INSERT)
		    && (sql_command == SQLCOM_INSERT_SELECT
			|| sql_command == SQLCOM_UPDATE
			|| sql_command == SQLCOM_CREATE_TABLE)) {

			/* If we either have innobase_locks_unsafe_for_binlog
			option set or this session is using READ COMMITTED
			isolation level and isolation level of the transaction
			is not set to serializable and MySQL is doing
			INSERT INTO...SELECT or UPDATE ... = (SELECT ...) or
			CREATE  ... SELECT... without FOR UPDATE or
			IN SHARE MODE in select, then we use consistent
			read for select. */

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

/*******************************************************************************
Read the next autoinc value. Acquire the relevant locks before reading
the AUTOINC value. If SUCCESS then the table AUTOINC mutex will be locked
on return and all relevant locks acquired. */
UNIV_INTERN
ulint
ha_innobase::innobase_get_autoinc(
/*==============================*/
					/* out: DB_SUCCESS or error code */
	ulonglong*	value)		/* out: autoinc value */
{
 	*value = 0;
 
	prebuilt->autoinc_error = innobase_lock_autoinc();

	if (prebuilt->autoinc_error == DB_SUCCESS) {

		/* Determine the first value of the interval */
		*value = dict_table_autoinc_read(prebuilt->table);

		/* It should have been initialized during open. */
		ut_a(*value != 0);
	}

	return(prebuilt->autoinc_error);
}

/***********************************************************************
This function reads the global auto-inc counter. It doesn't use the 
AUTOINC lock even if the lock mode is set to TRADITIONAL. */
UNIV_INTERN
ulonglong
ha_innobase::innobase_peek_autoinc(void)
/*====================================*/
					/* out: the autoinc value */
{
	ulonglong	auto_inc;
	dict_table_t*	innodb_table;

	ut_a(prebuilt != NULL);
	ut_a(prebuilt->table != NULL);

	innodb_table = prebuilt->table;

	dict_table_autoinc_lock(innodb_table);

	auto_inc = dict_table_autoinc_read(innodb_table);

	ut_a(auto_inc > 0);

	dict_table_autoinc_unlock(innodb_table);
 
	return(auto_inc);
}
  
/*******************************************************************************
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
        ulonglong	offset,              /* in: */
        ulonglong	increment,           /* in: table autoinc increment */
        ulonglong	nb_desired_values,   /* in: number of values reqd */
        ulonglong	*first_value,        /* out: the autoinc value */
        ulonglong	*nb_reserved_values) /* out: count of reserved values */
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

	*nb_reserved_values = trx->n_autoinc_rows;

	/* With old style AUTOINC locking we only update the table's
	AUTOINC counter after attempting to insert the row. */
	if (innobase_autoinc_lock_mode != AUTOINC_OLD_STYLE_LOCKING) {
		ulonglong	need;
		ulonglong	next_value;
		ulonglong	col_max_value;

		/* We need the upper limit of the col type to check for
		whether we update the table autoinc counter or not. */
		col_max_value = innobase_get_int_col_max_value(
			table->next_number_field);

		need = *nb_reserved_values * increment;

		/* Compute the last value in the interval */
		next_value = innobase_next_autoinc(
			*first_value, need, offset, col_max_value);

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

/* See comment in handler.h */
UNIV_INTERN
int
ha_innobase::reset_auto_increment(
/*==============================*/
	ulonglong	value)		/* in: new value for table autoinc */
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

/***********************************************************************
Compares two 'refs'. A 'ref' is the (internal) primary key value of the row.
If there is no explicitly declared non-null unique key or a primary key, then
InnoDB internally uses the row id as the primary key. */
UNIV_INTERN
int
ha_innobase::cmp_ref(
/*=================*/
				/* out: < 0 if ref1 < ref2, 0 if equal, else
				> 0 */
	const uchar*	ref1,	/* in: an (internal) primary key value in the
				MySQL key value format */
	const uchar*	ref2)	/* in: an (internal) primary key value in the
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

			ref1 += 2;
			ref2 += 2;
			result = ((Field_blob*)field)->cmp( ref1, len1,
                                                            ref2, len2);
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

/***********************************************************************
Ask InnoDB if a query to a table can be cached. */
UNIV_INTERN
my_bool
ha_innobase::register_query_cache_table(
/*====================================*/
					/* out: TRUE if query caching
					of the table is permitted */
	THD*		thd,		/* in: user thread handle */
	char*		table_key,	/* in: concatenation of database name,
					the null character '\0',
					and the table name */
	uint		key_length,	/* in: length of the full name, i.e.
					len(dbname) + len(tablename) + 1 */
	qc_engine_callback*
			call_back,	/* out: pointer to function for
					checking if query caching
					is permitted */
	ulonglong	*engine_data)	/* in/out: data to call_back */
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

/**********************************************************************
This function is used to find the storage length in bytes of the first n
characters for prefix indexes using a multibyte character set. The function
finds charset information and returns length of prefix_len characters in the
index field in bytes.

NOTE: the prototype of this function is copied to data0type.c! If you change
this function, you MUST change also data0type.c! */
extern "C" UNIV_INTERN
ulint
innobase_get_at_most_n_mbchars(
/*===========================*/
				/* out: number of bytes occupied by the first
				n characters */
	ulint charset_id,	/* in: character set id */
	ulint prefix_len,	/* in: prefix length in bytes of the index
				(this has to be divided by mbmaxlen to get the
				number of CHARACTERS n in the prefix) */
	ulint data_len,		/* in: length of the string in bytes */
	const char* str)	/* in: character string */
{
	ulint char_length;	/* character length in bytes */
	ulint n_chars;		/* number of characters in prefix */
	CHARSET_INFO* charset;	/* charset used in the field */

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

/***********************************************************************
This function is used to prepare X/Open XA distributed transaction   */
static
int
innobase_xa_prepare(
/*================*/
			/* out: 0 or error number */
        handlerton *hton,
	THD*	thd,	/* in: handle to the MySQL thread of the user
			whose XA transaction should be prepared */
	bool	all)	/* in: TRUE - commit transaction
			FALSE - the current SQL statement ended */
{
	int error = 0;
	trx_t* trx = check_trx_exists(thd);

	DBUG_ASSERT(hton == innodb_hton_ptr);

	if (thd_sql_command(thd) != SQLCOM_XA_PREPARE &&
	    (all || !thd_test_options(thd, OPTION_NOT_AUTOCOMMIT | OPTION_BEGIN)))
	{
		if (srv_enable_unsafe_group_commit && !THDVAR(thd, support_xa)) {
			/* choose group commit rather than binlog order */
			return(0);
		}

		/* For ibbackup to work the order of transactions in binlog
		and InnoDB must be the same. Consider the situation

		  thread1> prepare; write to binlog; ...
			  <context switch>
		  thread2> prepare; write to binlog; commit
		  thread1>			     ... commit

		To ensure this will not happen we're taking the mutex on
		prepare, and releasing it on commit.

		Note: only do it for normal commits, done via ha_commit_trans.
		If 2pc protocol is executed by external transaction
		coordinator, it will be just a regular MySQL client
		executing XA PREPARE and XA COMMIT commands.
		In this case we cannot know how many minutes or hours
		will be between XA PREPARE and XA COMMIT, and we don't want
		to block for undefined period of time.
		*/
		pthread_mutex_lock(&prepare_commit_mutex);
		trx->active_trans = 2;
	}

	/* we use support_xa value as it was seen at transaction start
	time, not the current session variable value. Any possible changes
	to the session variable take effect only in the next transaction */
	if (!trx->support_xa) {

		return(0);
	}

	thd_get_xid(thd, (MYSQL_XID*) &trx->xid);

	/* Release a possible FIFO ticket and search latch. Since we will
	reserve the kernel mutex, we have to release the search system latch
	first to obey the latching order. */

	innobase_release_stat_resources(trx);

	if (trx->active_trans == 0 && trx->conc_state != TRX_NOT_STARTED) {

	  sql_print_error("trx->active_trans == 0, but trx->conc_state != "
			  "TRX_NOT_STARTED");
	}

	if (all
		|| (!thd_test_options(thd, OPTION_NOT_AUTOCOMMIT | OPTION_BEGIN))) {

		/* We were instructed to prepare the whole transaction, or
		this is an SQL statement end and autocommit is on */

		ut_ad(trx->active_trans);

		error = (int) trx_prepare_for_mysql(trx);
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

/***********************************************************************
This function is used to recover X/Open XA distributed transactions   */
static
int
innobase_xa_recover(
/*================*/
				/* out: number of prepared transactions
				stored in xid_list */
        handlerton *hton,
	XID*	xid_list,	/* in/out: prepared transactions */
	uint	len)		/* in: number of slots in xid_list */
{
	DBUG_ASSERT(hton == innodb_hton_ptr);

	if (len == 0 || xid_list == NULL) {

		return(0);
	}

	return(trx_recover_for_mysql(xid_list, len));
}

/***********************************************************************
This function is used to commit one X/Open XA distributed transaction
which is in the prepared state */
static
int
innobase_commit_by_xid(
/*===================*/
			/* out: 0 or error number */
        handlerton *hton,
	XID*	xid)	/* in: X/Open XA transaction identification */
{
	trx_t*	trx;

	DBUG_ASSERT(hton == innodb_hton_ptr);

	trx = trx_get_trx_by_xid(xid);

	if (trx) {
		innobase_commit_low(trx);

		return(XA_OK);
	} else {
		return(XAER_NOTA);
	}
}

/***********************************************************************
This function is used to rollback one X/Open XA distributed transaction
which is in the prepared state */
static
int
innobase_rollback_by_xid(
/*=====================*/
			/* out: 0 or error number */
        handlerton *hton,
	XID	*xid)	/* in: X/Open XA transaction identification */
{
	trx_t*	trx;

	DBUG_ASSERT(hton == innodb_hton_ptr);

	trx = trx_get_trx_by_xid(xid);

	if (trx) {
		return(innobase_rollback_trx(trx));
	} else {
		return(XAER_NOTA);
	}
}

/***********************************************************************
Create a consistent view for a cursor based on current transaction
which is created if the corresponding MySQL thread still lacks one.
This consistent view is then used inside of MySQL when accessing records
using a cursor. */
static
void*
innobase_create_cursor_view(
/*========================*/
                          /* out: pointer to cursor view or NULL */
        handlerton *hton, /* in: innobase hton */
	THD* thd)	  /* in: user thread handle */
{
	DBUG_ASSERT(hton == innodb_hton_ptr);

	return(read_cursor_view_create_for_mysql(check_trx_exists(thd)));
}

/***********************************************************************
Close the given consistent cursor view of a transaction and restore
global read view to a transaction read view. Transaction is created if the
corresponding MySQL thread still lacks one. */
static
void
innobase_close_cursor_view(
/*=======================*/
        handlerton *hton,
	THD*	thd,	/* in: user thread handle */
	void*	curview)/* in: Consistent read view to be closed */
{
	DBUG_ASSERT(hton == innodb_hton_ptr);

	read_cursor_view_close_for_mysql(check_trx_exists(thd),
					 (cursor_view_t*) curview);
}

/***********************************************************************
Set the given consistent cursor view to a transaction which is created
if the corresponding MySQL thread still lacks one. If the given
consistent cursor view is NULL global read view of a transaction is
restored to a transaction read view. */
static
void
innobase_set_cursor_view(
/*=====================*/
        handlerton *hton,
	THD*	thd,	/* in: user thread handle */
	void*	curview)/* in: Consistent cursor view to be set */
{
	DBUG_ASSERT(hton == innodb_hton_ptr);

	read_cursor_set_for_mysql(check_trx_exists(thd),
				  (cursor_view_t*) curview);
}


/***********************************************************************
Check whether any of the given columns is being renamed in the table. */
static
bool
column_is_being_renamed(
/*====================*/
					/* out: true if any of col_names is
					being renamed in table */
	TABLE*		table,		/* in: MySQL table */
	uint		n_cols,		/* in: number of columns */
	const char**	col_names)	/* in: names of the columns */
{
	uint		j;
	uint		k;
	Field*		field;
	const char*	col_name;

	for (j = 0; j < n_cols; j++) {
		col_name = col_names[j];
		for (k = 0; k < table->s->fields; k++) {
			field = table->field[k];
			if ((field->flags & FIELD_IS_RENAMED)
			    && innobase_strcasecmp(field->field_name,
						   col_name) == 0) {
				return(true);
			}
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

	/* Check if a column participating in a foreign key is being renamed.
	There is no mechanism for updating InnoDB foreign key definitions. */
	if (foreign_key_column_is_being_renamed(prebuilt, table)) {

		return(COMPATIBLE_DATA_NO);
	}

	/* Check that row format didn't change */
	row_type = get_row_type();
	info_row_type = info->row_type;
	/* Default is compact. */
	if (info_row_type == ROW_TYPE_DEFAULT)
		info_row_type = ROW_TYPE_COMPACT;
	if ((info->used_fields & HA_CREATE_USED_ROW_FORMAT) &&
	    get_row_type() != ((info->row_type == ROW_TYPE_DEFAULT)
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

/****************************************************************
Validate the file format name and return its corresponding id. */
static
uint
innobase_file_format_name_lookup(
/*=============================*/
					/* out: valid file format id*/
	const char*	format_name)	/* in: pointer to file format name */
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

/****************************************************************
Validate the file format check value, is it one of "on" or "off",
as a side effect it sets the srv_check_file_format_at_startup variable. */
static
bool
innobase_file_format_check_on_off(
/*==============================*/
					/* out: true if config value one
					of "on" or  "off" */
	const char*	format_check)	/* in: parameter value */
{
	bool		ret = true;

	if (!innobase_strcasecmp(format_check, "off")) {

		/* Set the value to disable checking. */
		srv_check_file_format_at_startup = DICT_TF_FORMAT_MAX + 1;

	} else if (!innobase_strcasecmp(format_check, "on")) {

		/* Set the value to the lowest supported format. */
		srv_check_file_format_at_startup = DICT_TF_FORMAT_51;
	} else {
		ret = FALSE;
	}

	return(ret);
}

/****************************************************************
Validate the file format check config parameters, as a side effect it
sets the srv_check_file_format_at_startup variable. */
static
bool
innobase_file_format_check_validate(
/*================================*/
					/* out: true if valid config value */
	const char*	format_check)	/* in: parameter value */
{
	uint		format_id;
	bool		ret = true;

	format_id = innobase_file_format_name_lookup(format_check);

	if (format_id < DICT_TF_FORMAT_MAX + 1) {
		srv_check_file_format_at_startup = format_id;
	} else {
		ret = false;
	}

	return(ret);
}

/*****************************************************************
Check if it is a valid file format. This function is registered as
a callback with MySQL. */
static
int
innodb_file_format_name_validate(
/*=============================*/
						/* out: 0 for valid file
						format */
	THD*				thd,	/* in: thread handle */
	struct st_mysql_sys_var*	var,	/* in: pointer to system
						variable */
	void*				save,	/* out: immediate result
						for update function */
	struct st_mysql_value*		value)	/* in: incoming string */
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

			*(uint*) save = format_id;
			return(0);
		}
	}

	return(1);
}

/********************************************************************
Update the system variable innodb_file_format using the "saved"
value. This function is registered as a callback with MySQL. */
static
void
innodb_file_format_name_update(
/*===========================*/
	THD*				thd,		/* in: thread handle */
	struct st_mysql_sys_var*	var,		/* in: pointer to
							system variable */
	void*				var_ptr,	/* out: where the
							formal string goes */
	const void*			save)		/* in: immediate result
							from check function */
{
	ut_a(var_ptr != NULL);
	ut_a(save != NULL);
	ut_a((*(const uint*) save) <= DICT_TF_FORMAT_MAX);

	srv_file_format = *(const uint*) save;

	*(const char**) var_ptr
		= trx_sys_file_format_id_to_name(srv_file_format);
}

/*****************************************************************
Check if valid argument to innodb_file_format_check. This
function is registered as a callback with MySQL. */
static
int
innodb_file_format_check_validate(
/*==============================*/
						/* out: 0 for valid file
						format */
	THD*				thd,	/* in: thread handle */
	struct st_mysql_sys_var*	var,	/* in: pointer to system
						variable */
	void*				save,	/* out: immediate result
						for update function */
	struct st_mysql_value*		value)	/* in: incoming string */
{
	const char*	file_format_input;
	char		buff[STRING_BUFFER_USUAL_SIZE];
	int		len = sizeof(buff);

	ut_a(save != NULL);
	ut_a(value != NULL);

	file_format_input = value->val_str(value, buff, &len);

	if (file_format_input != NULL) {

		/* Check if user set on/off, we want to print a suitable
		message if they did so. */

		if (innobase_file_format_check_on_off(file_format_input)) {
			sql_print_warning(
				"InnoDB: invalid innodb_file_format_check "
				"value; on/off can only be set at startup or "
				"in the configuration file");
		} else if (innobase_file_format_check_validate(
				file_format_input)) {

			uint	format_id;

			format_id = innobase_file_format_name_lookup(
				file_format_input);

			ut_a(format_id <= DICT_TF_FORMAT_MAX);

			*(uint*) save = format_id;

			return(0);

		} else {
			sql_print_warning(
				"InnoDB: invalid innodb_file_format_check "
				"value; can be any format up to %s "
				"or its equivalent numeric id",
				trx_sys_file_format_id_to_name(
					DICT_TF_FORMAT_MAX));
		}
	}

	return(1);
}

/********************************************************************
Update the system variable innodb_file_format_check using the "saved"
value. This function is registered as a callback with MySQL. */
static
void
innodb_file_format_check_update(
/*============================*/
	THD*				thd,		/* in: thread handle */
	struct st_mysql_sys_var*	var,		/* in: pointer to
							system variable */
	void*				var_ptr,	/* out: where the
							formal string goes */
	const void*			save)		/* in: immediate result
							from check function */
{
	uint	format_id;

	ut_a(save != NULL);
	ut_a(var_ptr != NULL);

	format_id = *(const uint*) save;

	/* Update the max format id in the system tablespace. */
	if (trx_sys_file_format_max_set(format_id, (const char**) var_ptr)) {
		ut_print_timestamp(stderr);
		fprintf(stderr,
			" [Info] InnoDB: the file format in the system "
			"tablespace is now set to %s.\n", *(char**) var_ptr);
	}
}

/********************************************************************
Update the system variable innodb_adaptive_hash_index using the "saved"
value. This function is registered as a callback with MySQL. */
static
void
innodb_adaptive_hash_index_update(
/*==============================*/
	THD*				thd,		/* in: thread handle */
	struct st_mysql_sys_var*	var,		/* in: pointer to
							system variable */
	void*				var_ptr,	/* out: where the
							formal string goes */
	const void*			save)		/* in: immediate result
							from check function */
{
	if (*(my_bool*) save) {
		btr_search_enable();
	} else {
		btr_search_disable();
	}
}

/*****************************************************************
Check if it is a valid value of innodb_change_buffering.  This function is
registered as a callback with MySQL. */
static
int
innodb_change_buffering_validate(
/*=============================*/
						/* out: 0 for valid
						innodb_change_buffering */
	THD*				thd,	/* in: thread handle */
	struct st_mysql_sys_var*	var,	/* in: pointer to system
						variable */
	void*				save,	/* out: immediate result
						for update function */
	struct st_mysql_value*		value)	/* in: incoming string */
{
	const char*	change_buffering_input;
	char		buff[STRING_BUFFER_USUAL_SIZE];
	int		len = sizeof(buff);

	ut_a(save != NULL);
	ut_a(value != NULL);

	change_buffering_input = value->val_str(value, buff, &len);

	if (change_buffering_input != NULL) {
		ulint	use;

		for (use = 0; use < UT_ARR_SIZE(innobase_change_buffering_values);
		     use++) {
			if (!innobase_strcasecmp(
				    change_buffering_input,
				    innobase_change_buffering_values[use])) {
				*(ibuf_use_t*) save = (ibuf_use_t) use;
				return(0);
			}
		}
	}

	return(1);
}

/********************************************************************
Update the system variable innodb_change_buffering using the "saved"
value. This function is registered as a callback with MySQL. */
static
void
innodb_change_buffering_update(
/*===========================*/
	THD*				thd,		/* in: thread handle */
	struct st_mysql_sys_var*	var,		/* in: pointer to
							system variable */
	void*				var_ptr,	/* out: where the
							formal string goes */
	const void*			save)		/* in: immediate result
							from check function */
{
	ut_a(var_ptr != NULL);
	ut_a(save != NULL);
	ut_a((*(ibuf_use_t*) save) < IBUF_USE_COUNT);

	ibuf_use = *(const ibuf_use_t*) save;

	*(const char**) var_ptr = innobase_change_buffering_values[ibuf_use];
}

static int show_innodb_vars(THD *thd, SHOW_VAR *var, char *buff)
{
  innodb_export_status();
  var->type= SHOW_ARRAY;
  var->value= (char *) &innodb_status_variables;
  return 0;
}

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

static MYSQL_SYSVAR_STR(data_home_dir, innobase_data_home_dir,
  PLUGIN_VAR_READONLY,
  "The common part for InnoDB table spaces.",
  NULL, NULL, NULL);

static MYSQL_SYSVAR_BOOL(extra_undoslots, innobase_extra_undoslots,
  PLUGIN_VAR_NOCMDARG | PLUGIN_VAR_READONLY,
  "Enable to use about 4000 undo slots instead of default 1024. "
  "#### Attention: Once you enable this parameter, "
  "don't use the datafile for normal mysqld or ibbackup! ####",
  NULL, NULL, FALSE);

static MYSQL_SYSVAR_BOOL(fast_recovery, innobase_fast_recovery,
  PLUGIN_VAR_NOCMDARG | PLUGIN_VAR_READONLY,
  "Enable to use speed hack of recovery avoiding flush list sorting.",
  NULL, NULL, FALSE);

static MYSQL_SYSVAR_BOOL(overwrite_relay_log_info, innobase_overwrite_relay_log_info,
  PLUGIN_VAR_NOCMDARG | PLUGIN_VAR_READONLY,
  "During InnoDB crash recovery on slave overwrite relay-log.info "
  "to align master log file position if information in InnoDB and relay-log.info is different.",
  NULL, NULL, FALSE);

static MYSQL_SYSVAR_BOOL(doublewrite, innobase_use_doublewrite,
  PLUGIN_VAR_NOCMDARG | PLUGIN_VAR_READONLY,
  "Enable InnoDB doublewrite buffer (enabled by default). "
  "Disable with --skip-innodb-doublewrite.",
  NULL, NULL, TRUE);

static MYSQL_SYSVAR_ULONG(fast_shutdown, innobase_fast_shutdown,
  PLUGIN_VAR_OPCMDARG,
  "Speeds up the shutdown process of the InnoDB storage engine. Possible "
  "values are 0, 1 (faster)"
  /*
    NetWare can't close unclosed files, can't automatically kill remaining
    threads, etc, so on this OS we disable the crash-like InnoDB shutdown.
  */
  IF_NETWARE("", " or 2 (fastest - crash-like)")
  ".",
  NULL, NULL, 1, 0, IF_NETWARE(1,2), 0);

static MYSQL_SYSVAR_BOOL(file_per_table, srv_file_per_table,
  PLUGIN_VAR_NOCMDARG,
  "Stores each InnoDB table to an .ibd file in the database dir.",
  NULL, NULL, FALSE);

static MYSQL_SYSVAR_STR(file_format, innobase_file_format_name,
  PLUGIN_VAR_RQCMDARG,
  "File format to use for new tables in .ibd files.",
  innodb_file_format_name_validate,
  innodb_file_format_name_update, "Antelope");

static MYSQL_SYSVAR_STR(file_format_check, innobase_file_format_check,
  PLUGIN_VAR_OPCMDARG,
  "The highest file format in the tablespace.",
  innodb_file_format_check_validate,
  innodb_file_format_check_update,
  "on");

static MYSQL_SYSVAR_ULONG(flush_log_at_trx_commit, srv_flush_log_at_trx_commit,
  PLUGIN_VAR_OPCMDARG,
  "Set to 0 (write and flush once per second),"
  " 1 (write and flush at each commit)"
  " or 2 (write at commit, flush once per second).",
  NULL, NULL, 1, 0, 2, 0);

static MYSQL_SYSVAR_STR(flush_method, innobase_unix_file_flush_method,
  PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_READONLY,
  "With which method to flush data.", NULL, NULL, NULL);

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
  NULL, NULL, 90, 0, 100, 0);

static MYSQL_SYSVAR_ULONG(max_purge_lag, srv_max_purge_lag,
  PLUGIN_VAR_RQCMDARG,
  "Desired maximum length of the purge queue (0 = no limit)",
  NULL, NULL, 0, 0, ~0L, 0);

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

const char *innobase_stats_method_names[]=
{
  "nulls_equal",
  "nulls_unequal",
  "nulls_ignored",
  NullS
};
TYPELIB innobase_stats_method_typelib=
{
  array_elements(innobase_stats_method_names) - 1, "innobase_stats_method_typelib",
  innobase_stats_method_names, NULL
};
static MYSQL_SYSVAR_ENUM(stats_method, srv_stats_method,
  PLUGIN_VAR_RQCMDARG,
  "Specifies how InnoDB index statistics collection code should threat NULLs. "
  "Possible values of name are same to for 'myisam_stats_method'. "
  "This is startup parameter.",
  NULL, NULL, 0, &innobase_stats_method_typelib);

static MYSQL_SYSVAR_ULONG(stats_auto_update, srv_stats_auto_update,
  PLUGIN_VAR_RQCMDARG,
  "Enable/Disable InnoDB's auto update statistics of indexes. "
  "(except for ANALYZE TABLE command) 0:disable 1:enable",
  NULL, NULL, 1, 0, 1, 0);

static MYSQL_SYSVAR_BOOL(adaptive_hash_index, btr_search_enabled,
  PLUGIN_VAR_OPCMDARG,
  "Enable InnoDB adaptive hash index (enabled by default).  "
  "Disable with --skip-innodb-adaptive-hash-index.",
  NULL, innodb_adaptive_hash_index_update, TRUE);

static MYSQL_SYSVAR_ULONG(replication_delay, srv_replication_delay,
  PLUGIN_VAR_RQCMDARG,
  "Replication thread delay (ms) on the slave server if "
  "innodb_thread_concurrency is reached (0 by default)",
  NULL, NULL, 0, 0, ~0UL, 0);

static MYSQL_SYSVAR_LONG(additional_mem_pool_size, innobase_additional_mem_pool_size,
  PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_READONLY,
  "Size of a memory pool InnoDB uses to store data dictionary information and other internal data structures.",
  NULL, NULL, 1*1024*1024L, 512*1024L, LONG_MAX, 1024);

static MYSQL_SYSVAR_ULONG(autoextend_increment, srv_auto_extend_increment,
  PLUGIN_VAR_RQCMDARG,
  "Data file autoextend increment in megabytes",
  NULL, NULL, 8L, 1L, 1000L, 0);

static MYSQL_SYSVAR_LONGLONG(buffer_pool_size, innobase_buffer_pool_size,
  PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_READONLY,
  "The size of the memory buffer InnoDB uses to cache data and indexes of its tables.",
  NULL, NULL, 8*1024*1024L, 5*1024*1024L, LONGLONG_MAX, 1024*1024L);

static MYSQL_SYSVAR_ULONG(commit_concurrency, innobase_commit_concurrency,
  PLUGIN_VAR_RQCMDARG,
  "Helps in performance tuning in heavily concurrent environments.",
  innobase_commit_concurrency_validate, NULL, 0, 0, 1000, 0);

static MYSQL_SYSVAR_ULONG(concurrency_tickets, srv_n_free_tickets_to_enter,
  PLUGIN_VAR_RQCMDARG,
  "Number of times a thread is allowed to enter InnoDB within the same SQL query after it has once got the ticket",
  NULL, NULL, 500L, 1L, ~0L, 0);

static MYSQL_SYSVAR_LONG(file_io_threads, innobase_file_io_threads,
  PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_READONLY,
  "Number of file I/O threads in InnoDB.",
  NULL, NULL, 4, 4, 64, 0);

static MYSQL_SYSVAR_LONG(force_recovery, innobase_force_recovery,
  PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_READONLY,
  "Helps to save your data in case the disk image of the database becomes corrupt.",
  NULL, NULL, 0, 0, 6, 0);

static MYSQL_SYSVAR_LONG(log_buffer_size, innobase_log_buffer_size,
  PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_READONLY,
  "The size of the buffer which InnoDB uses to write log to the log files on disk.",
  NULL, NULL, 1024*1024L, 256*1024L, LONG_MAX, 1024);

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

static MYSQL_SYSVAR_LONG(open_files, innobase_open_files,
  PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_READONLY,
  "How many files at the maximum InnoDB keeps open at the same time.",
  NULL, NULL, 300L, 10L, LONG_MAX, 0);

static MYSQL_SYSVAR_ULONG(sync_spin_loops, srv_n_spin_wait_rounds,
  PLUGIN_VAR_RQCMDARG,
  "Count of spin-loop rounds in InnoDB mutexes",
  NULL, NULL, 20L, 0L, ~0L, 0);

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
  NULL, NULL, 10000L, 0L, ~0L, 0);

static MYSQL_SYSVAR_STR(data_file_path, innobase_data_file_path,
  PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_READONLY,
  "Path to individual files and their sizes.",
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

static MYSQL_SYSVAR_STR(change_buffering, innobase_change_buffering,
  PLUGIN_VAR_RQCMDARG,
  "Buffer changes to reduce random access: "
  "OFF, ON, inserting, deleting, changing, or purging.",
  innodb_change_buffering_validate,
  innodb_change_buffering_update, NULL);

static MYSQL_SYSVAR_ULONG(io_capacity, srv_io_capacity,
  PLUGIN_VAR_RQCMDARG,
  "Number of IO operations per second the server can do. Tunes background IO rate.",
  NULL, NULL, 200, 100, 999999999, 0);

static MYSQL_SYSVAR_LONGLONG(ibuf_max_size, srv_ibuf_max_size,
  PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_READONLY,
  "The maximum size of the insert buffer. (in bytes)",
  NULL, NULL, LONGLONG_MAX, 0, LONGLONG_MAX, 0);

static MYSQL_SYSVAR_ULONG(ibuf_active_contract, srv_ibuf_active_contract,
  PLUGIN_VAR_RQCMDARG,
  "Enable/Disable active_contract of insert buffer. 0:disable 1:enable",
  NULL, NULL, 0, 0, 1, 0);

static MYSQL_SYSVAR_ULONG(ibuf_accel_rate, srv_ibuf_accel_rate,
  PLUGIN_VAR_RQCMDARG,
  "Tunes amount of insert buffer processing of background, in addition to innodb_io_capacity. (in percentage)",
  NULL, NULL, 100, 100, 999999999, 0);

static MYSQL_SYSVAR_ULONG(flush_neighbor_pages, srv_flush_neighbor_pages,
  PLUGIN_VAR_RQCMDARG,
  "Enable/Disable flushing also neighbor pages. 0:disable 1:enable",
  NULL, NULL, 1, 0, 1, 0);

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
  "Control read ahead activity. (none, random, linear, [both])",
  NULL, innodb_read_ahead_update, 3, &read_ahead_typelib);

static
void
innodb_adaptive_checkpoint_update(
  THD* thd,
  struct st_mysql_sys_var*     var,
  void*        var_ptr,
  const void*  save)
{
  *(long *)var_ptr= (*(long *)save) % 3;
}
const char *adaptive_checkpoint_names[]=
{
  "none", /* 0 */
  "reflex", /* 1 */
  "estimate", /* 2 */
  /* For compatibility of the older patch */
  "0", /* 3 ("none" + 3) */
  "1", /* 4 ("reflex" + 3) */
  "2", /* 5 ("estimate" + 3) */
  NullS
};
TYPELIB adaptive_checkpoint_typelib=
{
  array_elements(adaptive_checkpoint_names) - 1, "adaptive_checkpoint_typelib",
  adaptive_checkpoint_names, NULL
};
static MYSQL_SYSVAR_ENUM(adaptive_checkpoint, srv_adaptive_checkpoint,
  PLUGIN_VAR_RQCMDARG,
  "Enable/Disable flushing along modified age. ([none], reflex, estimate)",
  NULL, innodb_adaptive_checkpoint_update, 0, &adaptive_checkpoint_typelib);

static MYSQL_SYSVAR_ULONG(enable_unsafe_group_commit, srv_enable_unsafe_group_commit,
  PLUGIN_VAR_RQCMDARG,
  "Enable/Disable unsafe group commit when support_xa=OFF and use with binlog or other XA storage engine.",
  NULL, NULL, 0, 0, 1, 0);

static MYSQL_SYSVAR_ULONG(read_io_threads, innobase_read_io_threads,
  PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_READONLY,
  "Number of background read I/O threads in InnoDB.",
  NULL, NULL, 8, 1, 64, 0);

static MYSQL_SYSVAR_ULONG(write_io_threads, innobase_write_io_threads,
  PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_READONLY,
  "Number of background write I/O threads in InnoDB.",
  NULL, NULL, 8, 1, 64, 0);

static MYSQL_SYSVAR_ULONG(expand_import, srv_expand_import,
  PLUGIN_VAR_RQCMDARG,
  "Enable/Disable converting automatically *.ibd files when import tablespace.",
  NULL, NULL, 0, 0, 1, 0);

static MYSQL_SYSVAR_ULONG(extra_rsegments, srv_extra_rsegments,
  PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_READONLY,
  "Number of extra user rollback segments when create new database.",
  NULL, NULL, 0, 0, 127, 0);

static MYSQL_SYSVAR_ULONG(dict_size_limit, srv_dict_size_limit,
  PLUGIN_VAR_RQCMDARG,
  "Limit the allocated memory for dictionary cache. (0: unlimited)",
  NULL, NULL, 0, 0, LONG_MAX, 0);

static struct st_mysql_sys_var* innobase_system_variables[]= {
  MYSQL_SYSVAR(additional_mem_pool_size),
  MYSQL_SYSVAR(autoextend_increment),
  MYSQL_SYSVAR(buffer_pool_size),
  MYSQL_SYSVAR(checksums),
  MYSQL_SYSVAR(commit_concurrency),
  MYSQL_SYSVAR(concurrency_tickets),
  MYSQL_SYSVAR(data_file_path),
  MYSQL_SYSVAR(data_home_dir),
  MYSQL_SYSVAR(doublewrite),
  MYSQL_SYSVAR(extra_undoslots),
  MYSQL_SYSVAR(fast_recovery),
  MYSQL_SYSVAR(fast_shutdown),
  MYSQL_SYSVAR(file_io_threads),
  MYSQL_SYSVAR(file_per_table),
  MYSQL_SYSVAR(file_format),
  MYSQL_SYSVAR(file_format_check),
  MYSQL_SYSVAR(flush_log_at_trx_commit),
  MYSQL_SYSVAR(flush_method),
  MYSQL_SYSVAR(force_recovery),
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
  MYSQL_SYSVAR(max_purge_lag),
  MYSQL_SYSVAR(mirrored_log_groups),
  MYSQL_SYSVAR(open_files),
  MYSQL_SYSVAR(overwrite_relay_log_info),
  MYSQL_SYSVAR(rollback_on_timeout),
  MYSQL_SYSVAR(stats_on_metadata),
  MYSQL_SYSVAR(stats_method),
  MYSQL_SYSVAR(stats_auto_update),
  MYSQL_SYSVAR(stats_sample_pages),
  MYSQL_SYSVAR(adaptive_hash_index),
  MYSQL_SYSVAR(replication_delay),
  MYSQL_SYSVAR(status_file),
  MYSQL_SYSVAR(strict_mode),
  MYSQL_SYSVAR(support_xa),
  MYSQL_SYSVAR(sync_spin_loops),
  MYSQL_SYSVAR(table_locks),
  MYSQL_SYSVAR(thread_concurrency),
  MYSQL_SYSVAR(thread_concurrency_timer_based),
  MYSQL_SYSVAR(thread_sleep_delay),
  MYSQL_SYSVAR(autoinc_lock_mode),
  MYSQL_SYSVAR(show_verbose_locks),
  MYSQL_SYSVAR(show_locks_held),
  MYSQL_SYSVAR(version),
  MYSQL_SYSVAR(io_capacity),
  MYSQL_SYSVAR(ibuf_max_size),
  MYSQL_SYSVAR(ibuf_active_contract),
  MYSQL_SYSVAR(ibuf_accel_rate),
  MYSQL_SYSVAR(flush_neighbor_pages),
  MYSQL_SYSVAR(read_ahead),
  MYSQL_SYSVAR(adaptive_checkpoint),
  MYSQL_SYSVAR(enable_unsafe_group_commit),
  MYSQL_SYSVAR(read_io_threads),
  MYSQL_SYSVAR(write_io_threads),
  MYSQL_SYSVAR(expand_import),
  MYSQL_SYSVAR(extra_rsegments),
  MYSQL_SYSVAR(dict_size_limit),
  MYSQL_SYSVAR(use_sys_malloc),
  MYSQL_SYSVAR(change_buffering),
  NULL
};

#ifdef MYSQL_DYNAMIC_PLUGIN
struct st_mysql_sys_var
{
	MYSQL_PLUGIN_VAR_HEADER;
	void* value;
};

struct param_mapping
{
	const char*	server;		/* Parameter name in the server. */
	const char*	plugin;		/* Paramater name in the plugin. */
};

/********************************************************************
Match the parameters from the static and dynamic versions. */
static
bool
innobase_match_parameter(
/*=====================*/
					/* out: true if names match */
	const char*	from_server,	/* in: variable name from server */
	const char*	from_plugin)	/* in: variable name from plugin */
{
	static const param_mapping param_map[] = {
		{"use_adaptive_hash_indexes", "adaptive_hash_index"}
	};

	if (strcmp(from_server, from_plugin) == 0) {
		return(true);
	}

	const param_mapping*	param = param_map;
	int	n_elems = sizeof(param_map) / sizeof(param_map[0]);

	for (int i = 0; i < n_elems; ++i, ++param) {

		if (strcmp(param->server, from_server) == 0
		    && strcmp(param->plugin, from_plugin) == 0) {

			return(true);
		}
	}

	return(false);
}

/********************************************************************
Copy InnoDB system variables from the static InnoDB to the dynamic
plugin. */
static
bool
innodb_plugin_init(void)
/*====================*/
		/* out: TRUE if the dynamic InnoDB plugin should start */
{
#if !MYSQL_STORAGE_ENGINE_PLUGIN
#error "MYSQL_STORAGE_ENGINE_PLUGIN must be nonzero."
#endif

	/* Copy the system variables. */

	struct st_mysql_plugin*		builtin;
	struct st_mysql_sys_var**	sta; /* static parameters */
	struct st_mysql_sys_var**	dyn; /* dynamic parameters */

#ifdef __WIN__
	if (!builtin_innobase_plugin_ptr) {

		return(true);
	}

	builtin = builtin_innobase_plugin_ptr;
#else
	switch (builtin_innobase_plugin) {
	case 0:
		return(true);
	case MYSQL_STORAGE_ENGINE_PLUGIN:
		break;
	default:
		return(false);
	}

	builtin = (struct st_mysql_plugin*) &builtin_innobase_plugin;
#endif

	for (sta = builtin->system_vars; *sta != NULL; sta++) {

		for (dyn = innobase_system_variables; *dyn != NULL; dyn++) {

			/* do not copy session variables */
			if (((*sta)->flags | (*dyn)->flags)
			    & PLUGIN_VAR_THDLOCAL) {
				continue;
			}

			if (innobase_match_parameter((*sta)->name,
						     (*dyn)->name)) {

				/* found the corresponding parameter */

				/* check if the flags are the same,
				ignoring differences in the READONLY or
				NOSYSVAR flags;
				e.g. we are not copying string variable to
				an integer one, but we do not care if it is
				readonly in the static and not in the
				dynamic */
				if (((*sta)->flags ^ (*dyn)->flags)
				    & ~(PLUGIN_VAR_READONLY
					| PLUGIN_VAR_NOSYSVAR)) {

					fprintf(stderr,
						"InnoDB: %s in static InnoDB "
						"(flags=0x%x) differs from "
						"%s in dynamic InnoDB "
						"(flags=0x%x)\n",
						(*sta)->name, (*sta)->flags,
						(*dyn)->name, (*dyn)->flags);

					/* we could break; here leaving this
					parameter uncopied */
					return(false);
				}

				/* assign the value of the static parameter
				to the dynamic one, according to their type */

#define COPY_VAR(label, type)					\
	case label:						\
		*(type*)(*dyn)->value = *(type*)(*sta)->value;	\
		break;

				switch ((*sta)->flags
					& ~(PLUGIN_VAR_MASK
					    | PLUGIN_VAR_UNSIGNED)) {

				COPY_VAR(PLUGIN_VAR_BOOL, char);
				COPY_VAR(PLUGIN_VAR_INT, int);
				COPY_VAR(PLUGIN_VAR_LONG, long);
				COPY_VAR(PLUGIN_VAR_LONGLONG, long long);
				COPY_VAR(PLUGIN_VAR_STR, char*);

				default:
					fprintf(stderr,
						"InnoDB: unknown flags "
						"0x%x for %s\n",
						(*sta)->flags, (*sta)->name);
				}

				/* Make the static InnoDB variable point to
				the dynamic one */
				(*sta)->value = (*dyn)->value;

				break;
			}
		}
	}

	return(true);
}
#endif /* MYSQL_DYNAMIC_PLUGIN */

mysql_declare_plugin(innobase)
{
  MYSQL_STORAGE_ENGINE_PLUGIN,
  &innobase_storage_engine,
  innobase_hton_name,
  "Innobase Oy",
  "Supports transactions, row-level locking, and foreign keys",
  PLUGIN_LICENSE_GPL,
  innobase_init, /* Plugin Init */
  NULL, /* Plugin Deinit */
  INNODB_VERSION_SHORT,
  innodb_status_variables_export,/* status variables             */
  innobase_system_variables, /* system variables */
  NULL /* reserved */
},
i_s_innodb_rseg,
i_s_innodb_buffer_pool_pages,
i_s_innodb_buffer_pool_pages_index,
i_s_innodb_buffer_pool_pages_blob,
i_s_innodb_trx,
i_s_innodb_locks,
i_s_innodb_lock_waits,
i_s_innodb_cmp,
i_s_innodb_cmp_reset,
i_s_innodb_cmpmem,
i_s_innodb_cmpmem_reset,
i_s_innodb_table_stats,
i_s_innodb_index_stats,
i_s_innodb_patches
mysql_declare_plugin_end;

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

/* Copyright (C) 2000 MySQL AB & Innobase Oy

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

/* This file defines the InnoDB handler: the interface between MySQL and
InnoDB */
  
/* TODO list for the InnoDB handler:
  - Ask Monty if strings of different languages can exist in the same
    database. Answer: in near future yes, but not yet.
*/
   
#ifdef __GNUC__
#pragma implementation				// gcc: Class implementation
#endif

#include "mysql_priv.h"
#include "slave.h"
#ifdef HAVE_INNOBASE_DB
#include <m_ctype.h>
#include <assert.h>
#include <hash.h>
#include <myisampack.h>

#define MAX_ULONG_BIT ((ulong) 1 << (sizeof(ulong)*8-1))

#include "ha_innobase.h"

/* We must declare this here because we undef SAFE_MUTEX below */
pthread_mutex_t innobase_mutex;

/* Store MySQL definition of 'byte': in Linux it is char while InnoDB
uses unsigned char */
typedef byte	mysql_byte;

#ifdef SAFE_MUTEX
#undef pthread_mutex_t
#endif

#define INSIDE_HA_INNOBASE_CC

/* Include necessary InnoDB headers */
extern "C" {
#include "../innobase/include/univ.i"
#include "../innobase/include/os0file.h"
#include "../innobase/include/srv0start.h"
#include "../innobase/include/srv0srv.h"
#include "../innobase/include/trx0roll.h"
#include "../innobase/include/trx0trx.h"
#include "../innobase/include/trx0sys.h"
#include "../innobase/include/row0ins.h"
#include "../innobase/include/row0mysql.h"
#include "../innobase/include/row0sel.h"
#include "../innobase/include/row0upd.h"
#include "../innobase/include/log0log.h"
#include "../innobase/include/lock0lock.h"
#include "../innobase/include/dict0crea.h"
#include "../innobase/include/btr0cur.h"
#include "../innobase/include/btr0btr.h"
#include "../innobase/include/fsp0fsp.h"
}

#define HA_INNOBASE_ROWS_IN_TABLE 10000 /* to get optimization right */
#define HA_INNOBASE_RANGE_COUNT	  100

bool 	innodb_skip 		= 0;
uint 	innobase_init_flags 	= 0;
ulong 	innobase_cache_size 	= 0;

/* The default values for the following, type long, start-up parameters
are declared in mysqld.cc: */

long innobase_mirrored_log_groups, innobase_log_files_in_group,
     innobase_log_file_size, innobase_log_buffer_size,
     innobase_buffer_pool_size, innobase_additional_mem_pool_size,
     innobase_file_io_threads, innobase_lock_wait_timeout,
     innobase_thread_concurrency, innobase_force_recovery;

/* The default values for the following char* start-up parameters
are determined in innobase_init below: */

/* innobase_data_file_path=ibdata:15,idata2:1,... */

char*	innobase_data_file_path			= NULL;
char*	innobase_data_home_dir			= NULL;
char*	innobase_log_group_home_dir		= NULL;
char*	innobase_log_arch_dir			= NULL;
char*	innobase_unix_file_flush_method		= NULL;

/* Below we have boolean-valued start-up parameters, and their default
values */

my_bool innobase_flush_log_at_trx_commit	= FALSE;
my_bool innobase_log_archive			= FALSE;
my_bool	innobase_use_native_aio			= FALSE;
my_bool	innobase_fast_shutdown			= TRUE;

/* The following counter is used to convey information to InnoDB
about server activity: in selects it is not sensible to call
srv_active_wake_master_thread after each fetch or search, we only do
it every INNOBASE_WAKE_INTERVAL'th step. */

#define INNOBASE_WAKE_INTERVAL	32
ulong	innobase_active_counter	= 0;

char*	innobase_home 	= NULL;

char    innodb_dummy_stmt_trx_handle = 'D';

static HASH 	innobase_open_tables;

static mysql_byte* innobase_get_key(INNOBASE_SHARE *share,uint *length,
			      my_bool not_used __attribute__((unused)));
static INNOBASE_SHARE *get_share(const char *table_name);
static void free_share(INNOBASE_SHARE *share);
static void innobase_print_error(const char* db_errpfx, char* buffer);

/* General functions */

/**********************************************************************
Releases possible search latch and InnoDB thread FIFO ticket. These should
be released at each SQL statement end. It does no harm to release these
also in the middle of an SQL statement. */
static
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

/************************************************************************
Increments innobase_active_counter and every INNOBASE_WAKE_INTERVALth
time calls srv_active_wake_master_thread. This function should be used
when a single database operation may introduce a small need for
server utility activity, like checkpointing. */
inline
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
Converts an InnoDB error code to a MySQL error code. */
static
int
convert_error_code_to_mysql(
/*========================*/
			/* out: MySQL error code */
	int	error,	/* in: InnoDB error code */
	THD*	thd)	/* in: user thread handle or NULL */
{
	if (error == DB_SUCCESS) {

		return(0);

  	} else if (error == (int) DB_DUPLICATE_KEY) {

    		return(HA_ERR_FOUND_DUPP_KEY);

 	} else if (error == (int) DB_RECORD_NOT_FOUND) {

    		return(HA_ERR_NO_ACTIVE_RECORD);

 	} else if (error == (int) DB_ERROR) {

    		return(HA_ERR_NO_ACTIVE_RECORD);

 	} else if (error == (int) DB_DEADLOCK) {
 		/* Since we roll back the whole transaction, we must
 		tell it also to MySQL so that MySQL knows to empty the
 		cached binlog for this transaction */

 		if (thd) {
 			ha_rollback(thd);
 		}

    		return(HA_ERR_LOCK_DEADLOCK);

 	} else if (error == (int) DB_LOCK_WAIT_TIMEOUT) {

 		/* Since we roll back the whole transaction, we must
 		tell it also to MySQL so that MySQL knows to empty the
 		cached binlog for this transaction */


 		if (thd) {
 			ha_rollback(thd);
 		}

    		return(HA_ERR_LOCK_WAIT_TIMEOUT);

 	} else if (error == (int) DB_NO_REFERENCED_ROW) {

    		return(HA_ERR_NO_REFERENCED_ROW);

 	} else if (error == (int) DB_ROW_IS_REFERENCED) {

    		return(HA_ERR_ROW_IS_REFERENCED);

 	} else if (error == (int) DB_CANNOT_ADD_CONSTRAINT) {

    		return(HA_ERR_CANNOT_ADD_FOREIGN);

 	} else if (error == (int) DB_OUT_OF_FILE_SPACE) {

    		return(HA_ERR_RECORD_FILE_FULL);

 	} else if (error == (int) DB_TABLE_IS_BEING_USED) {

    		return(HA_ERR_WRONG_COMMAND);

 	} else if (error == (int) DB_TABLE_NOT_FOUND) {

    		return(HA_ERR_KEY_NOT_FOUND);

  	} else if (error == (int) DB_TOO_BIG_RECORD) {

    		return(HA_ERR_TO_BIG_ROW);
    	} else {
    		return(-1);			// Unknown error
    	}
}

extern "C" {
/*****************************************************************
Prints info of a THD object (== user session thread) to the
standard output. NOTE that mysql/innobase/trx/trx0trx.c must contain
the prototype for this function! */

void
innobase_mysql_print_thd(
/*=====================*/
	char* buf,	/* in/out: buffer where to print, must be at least
			300 bytes */
        void* input_thd)/* in: pointer to a MySQL THD object */
{
  	THD*  thd;

  	thd = (THD*) input_thd;

        buf += sprintf(buf, "MySQL thread id %lu, query id %lu",
		       thd->thread_id, thd->query_id);
        if (thd->host) {
	  buf += sprintf(buf, " %.30s", thd->host);
        }

        if (thd->ip) {
	  buf += sprintf(buf, " %.20s", thd->ip);
        }

        if (thd->user) {
	  buf += sprintf(buf, " %.20s", thd->user);
        }

        if (thd->proc_info) {
	  buf += sprintf(buf, " %.50s", thd->proc_info);
        }

        if (thd->query) {
	  buf += sprintf(buf, "\n%.150s", thd->query);
        }

        buf += sprintf(buf, "\n");

#ifdef notdefined
	/* July 30, 2002
	Revert Monty's changes because they seem to make control
	characters sometimes appear in the output */

	/*  We can't use value of sprintf() as this is not portable */
  	buf+= my_sprintf(buf,
			 (buf, "MySQL thread id %lu",
			  thd->thread_id));
    	if (thd->host)
	{
	  *buf++=' ';
	  buf=strnmov(buf, thd->host, 30);
  	}

  	if (thd->ip)
	{
	  *buf++=' ';
	  buf=strnmov(buf, thd->ip, 20);
  	}

  	if (thd->user)
	{
	  *buf++=' ';
	  buf=strnmov(buf, thd->user, 20);
  	}

  	if (thd->proc_info)
	{
	  *buf++=' ';
	  buf=strnmov(buf, thd->proc_info, 50);
  	}

  	if (thd->query)
	{
	  *buf++='\n';
	  buf=strnmov(buf, thd->query, 150);
  	}  
	*buf='\n';
#endif
}
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
	trx_t*	trx;

	ut_a(thd == current_thd);

	trx = (trx_t*) thd->transaction.all.innobase_tid;

	if (trx == NULL) {
	        ut_a(thd != NULL);
		trx = trx_allocate_for_mysql();

		trx->mysql_thd = thd;

		thd->transaction.all.innobase_tid = trx;

		/* The execution of a single SQL statement is denoted by
		a 'transaction' handle which is a dummy pointer: InnoDB
		remembers internally where the latest SQL statement
		started, and if error handling requires rolling back the
		latest statement, InnoDB does a rollback to a savepoint. */

		thd->transaction.stmt.innobase_tid =
		                  (void*)&innodb_dummy_stmt_trx_handle;
	} else {
		if (trx->magic_n != TRX_MAGIC_N) {
			mem_analyze_corruption((byte*)trx);

			ut_a(0);
		}
	}

	if (thd->options & OPTION_NO_FOREIGN_KEY_CHECKS) {
		trx->check_foreigns = FALSE;
	} else {
		trx->check_foreigns = TRUE;
	}

	if (thd->options & OPTION_RELAXED_UNIQUE_CHECKS) {
		trx->check_unique_secondary = FALSE;
	} else {
		trx->check_unique_secondary = TRUE;
	}

	return(trx);
}

/*************************************************************************
Updates the user_thd field in a handle and also allocates a new InnoDB
transaction handle if needed, and updates the transaction fields in the
prebuilt struct. */
inline
int
ha_innobase::update_thd(
/*====================*/
			/* out: 0 or error code */
	THD*	thd)	/* in: thd to use the handle */
{
	row_prebuilt_t*	prebuilt = (row_prebuilt_t*) innobase_prebuilt;
	trx_t*		trx;
	
	trx = check_trx_exists(thd);

	if (prebuilt->trx != trx) {

		row_update_prebuilt_trx(prebuilt, trx);
	}

	user_thd = thd;

	return(0);
}

/*************************************************************************
Opens an InnoDB database. */

bool
innobase_init(void)
/*===============*/
			/* out: TRUE if error */
{
	static char 	current_dir[3];
	int		err;
	bool		ret;
	
  	DBUG_ENTER("innobase_init");

	os_innodb_umask = (ulint)my_umask;

	/* Use current_dir if no paths are set */
	current_dir[0] = FN_CURLIB;
	current_dir[1] = FN_LIBCHAR;
	current_dir[2] = 0;

	if (specialflag & SPECIAL_NO_PRIOR) {
	        srv_set_thread_priorities = FALSE;
	} else {
	        srv_set_thread_priorities = TRUE;
	        srv_query_thread_priority = QUERY_PRIOR;
	}

	/* Set InnoDB initialization parameters according to the values
	read from MySQL .cnf file */

	if (!innobase_data_file_path) {
		fprintf(stderr,
       "Cannot initialize InnoDB as 'innodb_data_file_path' is not set.\n"
       "If you do not want to use transactional InnoDB tables, add a line\n"
       "skip-innodb\n"
       "to the [mysqld] section of init parameters in your my.cnf\n"
       "or my.ini. If you want to use InnoDB tables, add to the [mysqld]\n"
       "section, for example,\n"
       "innodb_data_file_path = ibdata1:10M:autoextend\n"
       "But to get good performance you should adjust for your hardware\n"
       "the InnoDB startup options listed in section 2 at\n"
       "http://www.innodb.com/ibman.html\n");

	  	innodb_skip=1;
	  	DBUG_RETURN(FALSE);	/* Continue without InnoDB */
	}

	srv_data_home = (innobase_data_home_dir ? innobase_data_home_dir :
			 current_dir);
	srv_arch_dir =  (innobase_log_arch_dir ? innobase_log_arch_dir :
			 current_dir);

	ret = (bool)
		srv_parse_data_file_paths_and_sizes(innobase_data_file_path,
				&srv_data_file_names,
				&srv_data_file_sizes,
				&srv_data_file_is_raw_partition,
				&srv_n_data_files,
				&srv_auto_extend_last_data_file,
				&srv_last_file_size_max);
	if (ret == FALSE) {
		fprintf(stderr,
			"InnoDB: syntax error in innodb_data_file_path\n");
	  	DBUG_RETURN(TRUE);
	}

	if (!innobase_log_group_home_dir)
		innobase_log_group_home_dir = current_dir;

	ret = (bool)
		srv_parse_log_group_home_dirs(innobase_log_group_home_dir,
						&srv_log_group_home_dirs);

	if (ret == FALSE || innobase_mirrored_log_groups != 1) {
		fprintf(stderr,
		"InnoDB: syntax error in innodb_log_group_home_dir\n"
		"InnoDB: or a wrong number of mirrored log groups\n");

		DBUG_RETURN(TRUE);
	}

	srv_unix_file_flush_method_str = (innobase_unix_file_flush_method ?
				      innobase_unix_file_flush_method :
				      (char*)"fdatasync");

	srv_n_log_groups = (ulint) innobase_mirrored_log_groups;
	srv_n_log_files = (ulint) innobase_log_files_in_group;
	srv_log_file_size = (ulint) innobase_log_file_size;

	srv_log_archive_on = (ulint) innobase_log_archive;
	srv_log_buffer_size = (ulint) innobase_log_buffer_size;
	srv_flush_log_at_trx_commit = (ulint) innobase_flush_log_at_trx_commit;

	srv_use_native_aio = 0;

	srv_pool_size = (ulint) innobase_buffer_pool_size;

	srv_mem_pool_size = (ulint) innobase_additional_mem_pool_size;

	srv_n_file_io_threads = (ulint) innobase_file_io_threads;

	srv_lock_wait_timeout = (ulint) innobase_lock_wait_timeout;
	srv_thread_concurrency = (ulint) innobase_thread_concurrency;
	srv_force_recovery = (ulint) innobase_force_recovery;

	srv_fast_shutdown = (ibool) innobase_fast_shutdown;

	if (strcmp(default_charset_info->name, "latin1") == 0) {
		/* Store the character ordering table to InnoDB.
		For non-latin1 charsets we use the MySQL comparison
		functions, and consequently we do not need to know
		the ordering internally in InnoDB. */
		
		memcpy(srv_latin1_ordering,
				default_charset_info->sort_order, 256);
	}

	err = innobase_start_or_create_for_mysql();

	if (err != DB_SUCCESS) {

		DBUG_RETURN(1);
	}

	(void) hash_init(&innobase_open_tables,32,0,0,
			 		(hash_get_key) innobase_get_key,0,0);
	pthread_mutex_init(&innobase_mutex,MY_MUTEX_INIT_FAST);

	/* If this is a replication slave and we needed to do a crash recovery,
	set the master binlog position to what InnoDB internally knew about
	how far we got transactions durable inside InnoDB. There is a
	problem here: if the user used also MyISAM tables, InnoDB might not
	know the right position for them.

	THIS DOES NOT WORK CURRENTLY because replication seems to initialize
	glob_mi also after innobase_init. */
	
/*	if (trx_sys_mysql_master_log_pos != -1) {
		ut_memcpy(glob_mi.log_file_name, trx_sys_mysql_master_log_name,
				1 + ut_strlen(trx_sys_mysql_master_log_name));
		glob_mi.pos = trx_sys_mysql_master_log_pos;
	}
*/
  	DBUG_RETURN(0);
}

/***********************************************************************
Closes an InnoDB database. */

bool
innobase_end(void)
/*==============*/
				/* out: TRUE if error */
{
	int	err;

	DBUG_ENTER("innobase_end");

	err = innobase_shutdown_for_mysql();
	hash_free(&innobase_open_tables);

	if (err != DB_SUCCESS) {

	  DBUG_RETURN(1);
	}

  	DBUG_RETURN(0);
}

/********************************************************************
Flushes InnoDB logs to disk and makes a checkpoint. Really, a commit
flushes logs, and the name of this function should be innobase_checkpoint. */

bool
innobase_flush_logs(void)
/*=====================*/
				/* out: TRUE if error */
{
  	bool 	result = 0;

  	DBUG_ENTER("innobase_flush_logs");

	log_flush_up_to(ut_dulint_max, LOG_WAIT_ONE_GROUP);

  	DBUG_RETURN(result);
}

/*************************************************************************
Gets the free space in an InnoDB database: returned in units of kB. */

uint
innobase_get_free_space(void)
/*=========================*/
			/* out: free space in kB */
{
	return((uint) fsp_get_available_space_in_free_extents(0));
}

/*********************************************************************
Commits a transaction in an InnoDB database. */

void
innobase_commit_low(
/*================*/
	trx_t*	trx)	/* in: transaction handle */
{
	if (current_thd->slave_thread) {

		/* Update the replication position info inside InnoDB */
	
		trx->mysql_master_log_file_name = glob_mi.log_file_name;
		trx->mysql_master_log_pos = (ib_longlong)
					(glob_mi.pos + glob_mi.event_len
					+ glob_mi.pending);
	}

	trx_commit_for_mysql(trx);
}

/*********************************************************************
Commits a transaction in an InnoDB database. */

int
innobase_commit(
/*============*/
			/* out: 0 or error number */
	THD*	thd,	/* in: MySQL thread handle of the user for whom
			the transaction should be committed */
	void*	trx_handle)/* in: InnoDB trx handle or NULL: NULL means
			that the current SQL statement ended, and we should
			mark the start of a new statement with a savepoint */
{
	int	error	= 0;
	trx_t*	trx;

  	DBUG_ENTER("innobase_commit");
  	DBUG_PRINT("trans", ("ending transaction"));

	trx = check_trx_exists(thd);

        if (trx->auto_inc_lock) {
		  	
		/* If we had reserved the auto-inc lock for
		some table in this SQL statement, we release it now */
		  	
		srv_conc_enter_innodb(trx);
		row_unlock_table_autoinc_for_mysql(trx);
		srv_conc_exit_innodb(trx);
	}

	if (trx_handle != (void*)&innodb_dummy_stmt_trx_handle) {
		innobase_commit_low(trx);
	}

	/* Release possible statement level resources */
	innobase_release_stat_resources(trx);
	
	trx_mark_sql_stat_end(trx);

#ifndef DBUG_OFF
	if (error) {
    		DBUG_PRINT("error", ("error: %d", error));
    	}
#endif
	/* Tell InnoDB server that there might be work for
	utility threads: */

	srv_active_wake_master_thread();

	DBUG_RETURN(error);
}

/*********************************************************************
This is called when MySQL writes the binlog entry for the current
transaction. Writes to the InnoDB tablespace info which tells where the
MySQL binlog entry for the current transaction ended. Also commits the
transaction inside InnoDB. */

int
innobase_report_binlog_offset_and_commit(
/*=====================================*/
                                /* out: 0 or error code */
        THD*    thd,            /* in: user thread */
        void*   trx_handle,     /* in: InnoDB trx handle */
        char*   log_file_name,  /* in: latest binlog file name */
        my_off_t end_offset)    /* in: the offset in the binlog file
                                   up to which we wrote */
{
	trx_t*	trx;

	trx = (trx_t*)trx_handle;

	ut_a(trx != NULL);

	trx->mysql_log_file_name = log_file_name;  	
	trx->mysql_log_offset = (ib_longlong)end_offset;
	
  	return(innobase_commit(thd, trx_handle));
}

/*********************************************************************
Rolls back a transaction in an InnoDB database. */

int
innobase_rollback(
/*==============*/
			/* out: 0 or error number */
	THD*	thd,	/* in: handle to the MySQL thread of the user
			whose transaction should be rolled back */
	void*	trx_handle)/* in: InnoDB trx handle or a dummy stmt handle */
{
	int	error = 0;
	trx_t*	trx;

	DBUG_ENTER("innobase_rollback");
	DBUG_PRINT("trans", ("aborting transaction"));

	trx = check_trx_exists(thd);

        if (trx->auto_inc_lock) {
		  	
		/* If we had reserved the auto-inc lock for
		some table in this SQL statement, we release it now */
		  	
		srv_conc_enter_innodb(trx);
		row_unlock_table_autoinc_for_mysql(trx);
		srv_conc_exit_innodb(trx);
	}

	srv_conc_enter_innodb(trx);

	if (trx_handle != (void*)&innodb_dummy_stmt_trx_handle) {
		error = trx_rollback_for_mysql(trx);
	} else {
		error = trx_rollback_last_sql_stat_for_mysql(trx);
	}

	srv_conc_exit_innodb(trx);

	/* Release possible statement level resources */
	innobase_release_stat_resources(trx);

	trx_mark_sql_stat_end(trx);

	DBUG_RETURN(convert_error_code_to_mysql(error, NULL));
}

/*********************************************************************
Frees a possible InnoDB trx object associated with the current
THD. */

int
innobase_close_connection(
/*======================*/
			/* out: 0 or error number */
	THD*	thd)	/* in: handle to the MySQL thread of the user
			whose transaction should be rolled back */
{
	if (NULL != thd->transaction.all.innobase_tid) {

	        trx_rollback_for_mysql((trx_t*)
				(thd->transaction.all.innobase_tid));
		trx_free_for_mysql((trx_t*)
				(thd->transaction.all.innobase_tid));
		thd->transaction.all.innobase_tid = NULL;
	}

	return(0);
}

/**********************************************************************
Prints an error message. */
static
void
innobase_print_error(
/*=================*/
	const char*	db_errpfx,	/* in: error prefix text */
	char*		buffer)		/* in: error text */
{
  	sql_print_error("%s:  %s", db_errpfx, buffer);
}


/*****************************************************************************
** InnoDB database tables
*****************************************************************************/

/********************************************************************
This function is not relevant since we store the tables and indexes
into our own tablespace, not as files, whose extension this function would
give. */

const char**
ha_innobase::bas_ext() const
/*========================*/
				/* out: file extension strings, currently not
				used */
{
	static const char* ext[] = {".InnoDB", NullS};

	return(ext);
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
	/* Put to lower case */

	ptr = norm_name;

	while (*ptr != '\0') {
	        *ptr = tolower(*ptr);
	        ptr++;
	}
#endif
}

/*********************************************************************
Creates and opens a handle to a table which already exists in an InnoDB
database. */

int
ha_innobase::open(
/*==============*/
					/* out: 1 if error, 0 if success */
	const char*	name,		/* in: table name */
	int 		mode,		/* in: not used */
	uint 		test_if_locked)	/* in: not used */
{
	dict_table_t*	ib_table;
  	int 		error	= 0;
  	uint		buff_len;
  	char		norm_name[1000];

	DBUG_ENTER("ha_innobase::open");

	UT_NOT_USED(mode);
	UT_NOT_USED(test_if_locked);

	normalize_table_name(norm_name, name);

	user_thd = NULL;

	last_query_id = (ulong)-1;

	if (!(share=get_share(name)))
	  DBUG_RETURN(1);

	/* Create buffers for packing the fields of a record. Why
	table->reclength did not work here? Obviously, because char
	fields when packed actually became 1 byte longer, when we also
	stored the string length as the first byte. */

	buff_len = table->reclength + table->max_key_length
							+ MAX_REF_PARTS * 3;
	if (!(mysql_byte*) my_multi_malloc(MYF(MY_WME),
				     &upd_buff, buff_len,
				     &key_val_buff, buff_len,
				     NullS)) {
	  	free_share(share);
	  	DBUG_RETURN(1);
  	}

	/* Get pointer to a table object in InnoDB dictionary cache */

	ib_table = dict_table_get_and_increment_handle_count(
				      		     norm_name, NULL);
 	if (NULL == ib_table) {

	  	fprintf(stderr,
"InnoDB: Error: cannot find table %s from the internal data dictionary\n"
"InnoDB: of InnoDB though the .frm file for the table exists. Maybe you\n"
"InnoDB: have deleted and recreated InnoDB data files but have forgotten\n"
"InnoDB: to delete the corresponding .frm files of InnoDB tables, or you\n"
"InnoDB: have moved .frm files to another database?\n",
		  norm_name);

	        free_share(share);
    		my_free((char*) upd_buff, MYF(0));
    		my_errno = ENOENT;
    		DBUG_RETURN(1);
  	}

	innobase_prebuilt = row_create_prebuilt(ib_table);

	((row_prebuilt_t*)innobase_prebuilt)->mysql_row_len = table->reclength;

  	primary_key = MAX_KEY;

	/* Allocate a buffer for a 'row reference'. A row reference is
	a string of bytes of length ref_length which uniquely specifies
        a row in our table. Note that MySQL may also compare two row
        references for equality by doing a simple memcmp on the strings
        of length ref_length! */

  	if (!row_table_got_default_clust_index(ib_table)) {

		((row_prebuilt_t*)innobase_prebuilt)
				->clust_index_was_generated = FALSE;

		primary_key = 0;
		key_used_on_scan = 0;

 		/* MySQL allocates the buffer for ref. key_info->key_length
                includes space for all key columns + one byte for each column
		that may be NULL. ref_length must be as exact as possible to
		save space, because all row reference buffers are allocated
		based on ref_length. */

  		ref_length = table->key_info->key_length;
	} else {
		((row_prebuilt_t*)innobase_prebuilt)
				->clust_index_was_generated = TRUE;

  		ref_length = DATA_ROW_ID_LEN;
				
		/* If we automatically created the clustered index, then
		MySQL does not know about it, and MySQL must NOT be aware
		of the index used on scan, to make it avoid checking if we
		update the column of the index. That is why we assert below
                that key_used_on_scan is the undefined value MAX_KEY.
                The column is the row id in the automatical generation case,
		and it will never be updated anyway. */

		DBUG_ASSERT(key_used_on_scan == MAX_KEY);
	}

	auto_inc_counter_for_this_stat = 0;

	block_size = 16 * 1024;	/* Index block size in InnoDB: used by MySQL
				in query optimization */

	/* Init table lock structure */
	thr_lock_data_init(&share->lock,&lock,(void*) 0);

  	info(HA_STATUS_NO_LOCK | HA_STATUS_VARIABLE | HA_STATUS_CONST);

  	DBUG_RETURN(0);
}

/*********************************************************************
Does nothing. */

void
ha_innobase::initialize(void)
/*=========================*/
{
}

/**********************************************************************
Closes a handle to an InnoDB table. */

int
ha_innobase::close(void)
/*====================*/
				/* out: error number */
{
  	DBUG_ENTER("ha_innobase::close");

	row_prebuilt_free((row_prebuilt_t*) innobase_prebuilt);

    	my_free((char*) upd_buff, MYF(0));
        free_share(share);

	/* Tell InnoDB server that there might be work for
	utility threads: */

	srv_active_wake_master_thread();

  	DBUG_RETURN(0);
}

/* The following accessor functions should really be inside MySQL code! */

/******************************************************************
Gets field offset for a field in a table. */
inline
uint
get_field_offset(
/*=============*/
			/* out: offset */
	TABLE*	table,	/* in: MySQL table object */
	Field*	field)	/* in: MySQL field object */
{
	return((uint) (field->ptr - (char*) table->record[0]));
}

/******************************************************************
Checks if a field in a record is SQL NULL. Uses the record format
information in table to track the null bit in record. */
inline
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
inline
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

/******************************************************************
Resets SQL NULL bits in a record to zero. */
inline
void
reset_null_bits(
/*============*/
	TABLE*	table,	/* in: MySQL table object */
	char*	record)	/* in: a row in MySQL format */
{
	bzero(record, table->null_bytes);
}

extern "C" {
/*****************************************************************
InnoDB uses this function is to compare two data fields for which the
data type is such that we must use MySQL code to compare them. NOTE that the
prototype of this function is in rem0cmp.c in InnoDB source code!
If you change this function, remember to update the prototype there! */

int
innobase_mysql_cmp(
/*===============*/
					/* out: 1, 0, -1, if a is greater,
					equal, less than b, respectively */
	int		mysql_type,	/* in: MySQL type */
	unsigned char*	a,		/* in: data field */
	unsigned int	a_length,	/* in: data field length,
					not UNIV_SQL_NULL */
	unsigned char*	b,		/* in: data field */
	unsigned int	b_length)	/* in: data field length,
					not UNIV_SQL_NULL */
{
	enum_field_types	mysql_tp;
	int                     ret;

	DBUG_ASSERT(a_length != UNIV_SQL_NULL);
	DBUG_ASSERT(b_length != UNIV_SQL_NULL);

	mysql_tp = (enum_field_types) mysql_type;

	switch (mysql_tp) {

	case FIELD_TYPE_STRING:
	case FIELD_TYPE_VAR_STRING:
  		ret = my_sortncmp((const char*) a, a_length,
				  (const char*) b, b_length);
		if (ret < 0) {
		  return(-1);
		} else if (ret > 0) {
		  return(1);
		} else {
		  return(0);
	        }
	default:
		assert(0);
	}

	return(0);
}
}

/******************************************************************
Converts a MySQL type to an InnoDB type. */
inline
ulint
get_innobase_type_from_mysql_type(
/*==============================*/
			/* out: DATA_BINARY, DATA_VARCHAR, ... */
	Field*	field)	/* in: MySQL field */
{
	/* The following asserts check that MySQL type code fits in
	8 bits: this is used in ibuf and also when DATA_NOT_NULL is
	ORed to the type */

	DBUG_ASSERT((ulint)FIELD_TYPE_STRING < 256);
	DBUG_ASSERT((ulint)FIELD_TYPE_VAR_STRING < 256);
	DBUG_ASSERT((ulint)FIELD_TYPE_DOUBLE < 256);
	DBUG_ASSERT((ulint)FIELD_TYPE_FLOAT < 256);
	DBUG_ASSERT((ulint)FIELD_TYPE_DECIMAL < 256);

	switch (field->type()) {
		case FIELD_TYPE_VAR_STRING: if (field->flags & BINARY_FLAG) {

						return(DATA_BINARY);
					} else if (strcmp(
						   default_charset_info->name,
							"latin1") == 0) {
						return(DATA_VARCHAR);
					} else {
						return(DATA_VARMYSQL);
					}
		case FIELD_TYPE_STRING: if (field->flags & BINARY_FLAG) {

						return(DATA_FIXBINARY);
					} else if (strcmp(
						   default_charset_info->name,
							"latin1") == 0) {
						return(DATA_CHAR);
					} else {
						return(DATA_MYSQL);
					}
		case FIELD_TYPE_LONG:
		case FIELD_TYPE_LONGLONG:
		case FIELD_TYPE_TINY:
		case FIELD_TYPE_SHORT:
		case FIELD_TYPE_INT24:
		case FIELD_TYPE_DATE:
		case FIELD_TYPE_DATETIME:
		case FIELD_TYPE_YEAR:
		case FIELD_TYPE_NEWDATE:
		case FIELD_TYPE_ENUM:
		case FIELD_TYPE_SET:
		case FIELD_TYPE_TIME:
		case FIELD_TYPE_TIMESTAMP:
					return(DATA_INT);
		case FIELD_TYPE_FLOAT:
					return(DATA_FLOAT);
		case FIELD_TYPE_DOUBLE:
					return(DATA_DOUBLE);
		case FIELD_TYPE_DECIMAL:
					return(DATA_DECIMAL);
		case FIELD_TYPE_TINY_BLOB:
		case FIELD_TYPE_MEDIUM_BLOB:
		case FIELD_TYPE_BLOB:
		case FIELD_TYPE_LONG_BLOB:
					return(DATA_BLOB);
		default:
					assert(0);
	}

	return(0);
}

/***********************************************************************
Stores a key value for a row to a buffer. This must currently only be used
to store a row reference to the 'ref' buffer of this table handle! */

uint
ha_innobase::store_key_val_for_row(
/*===============================*/
				/* out: key value length as stored in buff */
	uint 		keynr,	/* in: key number */
	char*		buff,	/* in/out: buffer for the key value (in MySQL
				format); currently this MUST be the 'ref'
				buffer! */
	const mysql_byte* record)/* in: row in MySQL format */
{
	KEY*		key_info 	= table->key_info + keynr;
  	KEY_PART_INFO*	key_part	= key_info->key_part;
  	KEY_PART_INFO*	end		= key_part + key_info->key_parts;
	char*		buff_start	= buff;

  	DBUG_ENTER("store_key_val_for_row");

  	for (; key_part != end; key_part++) {

    		if (key_part->null_bit) {
      			/* Store 0 if the key part is a NULL part */

      			if (record[key_part->null_offset]
						& key_part->null_bit) {
				*buff++ = 1;
				continue;
      			}

      			*buff++ = 0;
    		}

		memcpy(buff, record + key_part->offset, key_part->length);
		buff += key_part->length;
  	}

	/* We have to zero-fill the 'ref' buffer so that MySQL is able to
        use a simple memcmp to compare two key values to determine if they are
	equal */

	bzero(buff, (ref_length - (uint) (buff - buff_start)));

	DBUG_RETURN(ref_length);
}

/******************************************************************
Builds a template to the prebuilt struct. */
static
void
build_template(
/*===========*/
	row_prebuilt_t*	prebuilt,	/* in: prebuilt struct */
	THD*		thd,		/* in: current user thread, used
					only if templ_type is
					ROW_MYSQL_REC_FIELDS */
	TABLE*		table,		/* in: MySQL table */
	ulint		templ_type)	/* in: ROW_MYSQL_WHOLE_ROW or
					ROW_MYSQL_REC_FIELDS */
{
	dict_index_t*	index;
	dict_index_t*	clust_index;
	mysql_row_templ_t* templ;
	Field*		field;
	ulint		n_fields;
	ulint		n_requested_fields	= 0;
	ibool		fetch_all_in_key	= FALSE;
	ulint		i;

	clust_index = dict_table_get_first_index_noninline(prebuilt->table);

	if (!prebuilt->hint_no_need_to_fetch_extra_cols) {
		/* We have a hint that we should at least fetch all
		columns in the key, or all columns in the table */

		if (prebuilt->read_just_key) {
			/* MySQL has instructed us that it is enough to
			fetch the columns in the key */
			
			fetch_all_in_key = TRUE;
		} else {
			/* We are building a temporary table: fetch all
 			columns; the reason is that MySQL may use the
			clustered index key to store rows, but the mechanism
			we use below to detect required columns does not
			reveal that. Actually, it might be enough to
			fetch only all in the key also in this case! */
		
			templ_type = ROW_MYSQL_WHOLE_ROW;
		}
	}

	if (prebuilt->select_lock_type == LOCK_X) {
		/* We always retrieve the whole clustered index record if we
		use exclusive row level locks, for example, if the read is
		done in an UPDATE statement. */

	        templ_type = ROW_MYSQL_WHOLE_ROW;
	}

	if (templ_type == ROW_MYSQL_REC_FIELDS) {
		/* In versions < 3.23.50 we always retrieved the clustered
		index record if prebuilt->select_lock_type == LOCK_S,
		but there is really not need for that, and in some cases
		performance could be seriously degraded because the MySQL
		optimizer did not know about our convention! */

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

	n_fields = (ulint)table->fields;

	if (!prebuilt->mysql_template) {
		prebuilt->mysql_template = (mysql_row_templ_t*)
						mem_alloc_noninline(
					n_fields * sizeof(mysql_row_templ_t));
	}

	prebuilt->template_type = templ_type;
	prebuilt->null_bitmap_len = table->null_bytes;

	prebuilt->templ_contains_blob = FALSE;

	for (i = 0; i < n_fields; i++) {
		templ = prebuilt->mysql_template + n_requested_fields;
		field = table->field[i];

		if (templ_type == ROW_MYSQL_REC_FIELDS
			&& !(fetch_all_in_key &&
				ULINT_UNDEFINED != dict_index_get_nth_col_pos(
								index, i))
			&& thd->query_id != field->query_id
			&& thd->query_id != (field->query_id ^ MAX_ULONG_BIT)
			&& thd->query_id !=
				(field->query_id ^ (MAX_ULONG_BIT >> 1))) {

			/* This field is not needed in the query, skip it */

			goto skip_field;
		}

		n_requested_fields++;

		templ->col_no = i;

		if (index == clust_index) {
			templ->rec_field_no = (index->table->cols + i)
								->clust_pos;
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
		templ->type = get_innobase_type_from_mysql_type(field);
		templ->is_unsigned = (ulint) (field->flags & UNSIGNED_FLAG);

		if (templ->type == DATA_BLOB) {
			prebuilt->templ_contains_blob = TRUE;
		}
skip_field:
		;
	}

	prebuilt->n_template = n_requested_fields;

	if (prebuilt->need_to_access_clustered) {
		/* Change rec_field_no's to correspond to the clustered index
		record */
		for (i = 0; i < n_requested_fields; i++) {
			templ = prebuilt->mysql_template + i;

			templ->rec_field_no =
			    (index->table->cols + templ->col_no)->clust_pos;
		}
	}
}

/************************************************************************
Stores a row in an InnoDB database, to the table specified in this
handle. */

int
ha_innobase::write_row(
/*===================*/
				/* out: error code */
	mysql_byte* 	record)	/* in: a row in MySQL format */
{
	row_prebuilt_t* prebuilt = (row_prebuilt_t*)innobase_prebuilt;
  	int 		error;
	longlong	auto_inc;
	longlong	dummy;
	
  	DBUG_ENTER("ha_innobase::write_row");

	ut_a(prebuilt->trx ==
		(trx_t*) current_thd->transaction.all.innobase_tid);

  	statistic_increment(ha_write_count, &LOCK_status);

  	if (table->time_stamp) {
    		update_timestamp(record + table->time_stamp - 1);
    	}

	if (last_query_id != user_thd->query_id) {
	        prebuilt->sql_stat_start = TRUE;
                last_query_id = user_thd->query_id;

		innobase_release_stat_resources(prebuilt->trx);
	}

  	if (table->next_number_field && record == table->record[0]) {
		/* This is the case where the table has an
		auto-increment column */

		/* Initialize the auto-inc counter if it has not been
		initialized yet */

		if (0 == dict_table_autoinc_peek(prebuilt->table)) {

			/* This call initializes the counter */
		        error = innobase_read_and_init_auto_inc(&dummy);

			if (error) {
				/* Deadlock or lock wait timeout */

				goto func_exit;
			}

			/* We have to set sql_stat_start to TRUE because
			the above call probably has called a select, and
			has reset that flag; row_insert_for_mysql has to
			know to set the IX intention lock on the table,
			something it only does at the start of each
			statement */

			prebuilt->sql_stat_start = TRUE;
		}

	        /* Fetch the value the user possibly has set in the
	        autoincrement field */
	        
	        auto_inc = table->next_number_field->val_int();

		/* In replication and also otherwise the auto-inc column 
		can be set with SET INSERT_ID. Then we must look at
		user_thd->next_insert_id. If it is nonzero and the user
		has not supplied a value, we must use it, and use values
		incremented by 1 in all subsequent inserts within the
		same SQL statement! */

		if (auto_inc == 0 && user_thd->next_insert_id != 0) {
		        auto_inc = user_thd->next_insert_id;
		        auto_inc_counter_for_this_stat = auto_inc;
		}

		if (auto_inc == 0 && auto_inc_counter_for_this_stat) {
			/* The user set the auto-inc counter for
			this SQL statement with SET INSERT_ID. We must
			assign sequential values from the counter. */

			auto_inc_counter_for_this_stat++;

			auto_inc = auto_inc_counter_for_this_stat;

			/* We give MySQL a new value to place in the
			auto-inc column */
			user_thd->next_insert_id = auto_inc;
		}
		
		if (auto_inc != 0) {
			/* This call will calculate the max of the current
			value and the value supplied by the user and
			update the counter accordingly */

			/* We have to use the transactional lock mechanism
			on the auto-inc counter of the table to ensure
			that replication and roll-forward of the binlog
			exactly imitates also the given auto-inc values.
			The lock is released at each SQL statement's
			end. */

			srv_conc_enter_innodb(prebuilt->trx);
			error = row_lock_table_autoinc_for_mysql(prebuilt);
			srv_conc_exit_innodb(prebuilt->trx);

			if (error != DB_SUCCESS) {
			
				error = convert_error_code_to_mysql(error,
								user_thd);
				goto func_exit;
			}	
			
			dict_table_autoinc_update(prebuilt->table, auto_inc);
		} else {
			srv_conc_enter_innodb(prebuilt->trx);

			if (!prebuilt->trx->auto_inc_lock) {

				error = row_lock_table_autoinc_for_mysql(
								prebuilt);
				if (error != DB_SUCCESS) {
					srv_conc_exit_innodb(prebuilt->trx);
			
					error = convert_error_code_to_mysql(
							error, user_thd);
					goto func_exit;
				}
			}	

			auto_inc = dict_table_autoinc_get(prebuilt->table);
			srv_conc_exit_innodb(prebuilt->trx);

			/* We can give the new value for MySQL to place in
			the field */

			user_thd->next_insert_id = auto_inc;
		}

		/* This call of a handler.cc function places
		user_thd->next_insert_id to the column value, if the column
		value was not set by the user */

    		update_auto_increment();
	}

	if (prebuilt->mysql_template == NULL
			|| prebuilt->template_type != ROW_MYSQL_WHOLE_ROW) {
		/* Build the template used in converting quickly between
		the two database formats */

		build_template(prebuilt, NULL, table, ROW_MYSQL_WHOLE_ROW);
	}

	if (user_thd->lex.sql_command == SQLCOM_INSERT
	    && user_thd->lex.duplicates == DUP_IGNORE) {
	        prebuilt->trx->ignore_duplicates_in_insert = TRUE;
        } else {
	        prebuilt->trx->ignore_duplicates_in_insert = FALSE;
	}

	srv_conc_enter_innodb(prebuilt->trx);

	error = row_insert_for_mysql((byte*) record, prebuilt);

	srv_conc_exit_innodb(prebuilt->trx);

	prebuilt->trx->ignore_duplicates_in_insert = FALSE;

	error = convert_error_code_to_mysql(error, user_thd);

	/* Tell InnoDB server that there might be work for
	utility threads: */
func_exit:
	innobase_active_small();

  	DBUG_RETURN(error);
}

/******************************************************************
Converts field data for storage in an InnoDB update vector. */
inline
mysql_byte*
innobase_convert_and_store_changed_col(
/*===================================*/
				/* out: pointer to the end of the converted
				data in the buffer */
	upd_field_t*	ufield,	/* in/out: field in the update vector */
	mysql_byte*	buf,	/* in: buffer we can use in conversion */
	mysql_byte*	data,	/* in: column data to store */
	ulint		len,	/* in: data len */
	ulint		col_type,/* in: data type in InnoDB type numbers */
	ulint		is_unsigned)/* in: != 0 if an unsigned integer type */
{
	uint	i;

	if (len == UNIV_SQL_NULL) {
		data = NULL;
	} else if (col_type == DATA_VARCHAR || col_type == DATA_BINARY
		   || col_type == DATA_VARMYSQL) {
	        /* Remove trailing spaces */
        	while (len > 0 && data[len - 1] == ' ') {
	                len--;
	        }

	} else if (col_type == DATA_INT) {
		/* Store integer data in InnoDB in a big-endian
		format, sign bit negated, if signed */

		for (i = 0; i < len; i++) {
			buf[len - 1 - i] = data[i];
		}

		if (!is_unsigned) {
			buf[0] = buf[0] ^ 128;
		}

		data = buf;

		buf += len;
	}

	ufield->new_val.data = data;
	ufield->new_val.len = len;

	return(buf);
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
	mysql_byte* 	old_row,	/* in: old row in MySQL format */
	mysql_byte* 	new_row,	/* in: new row in MySQL format */
	struct st_table* table,		/* in: table in MySQL data dictionary */
	mysql_byte*	upd_buff,	/* in: buffer to use */
	row_prebuilt_t*	prebuilt,	/* in: InnoDB prebuilt struct */
	THD*		thd)		/* in: user thread */
{
	Field*		field;
	uint		n_fields;
	ulint		o_len;
	ulint		n_len;
	byte*	        o_ptr;
        byte*	        n_ptr;
        byte*	        buf;
	upd_field_t*	ufield;
	ulint		col_type;
	ulint		is_unsigned;
	ulint		n_changed = 0;
	uint		i;

	n_fields = table->fields;

	/* We use upd_buff to convert changed fields */
	buf = (byte*) upd_buff;

	for (i = 0; i < n_fields; i++) {
		field = table->field[i];

		/* if (thd->query_id != field->query_id) { */
			/* TODO: check that these fields cannot have
			changed! */

		/*	goto skip_field;
		}*/

		o_ptr = (byte*) old_row + get_field_offset(table, field);
		n_ptr = (byte*) new_row + get_field_offset(table, field);
		o_len = field->pack_length();
		n_len = field->pack_length();

		col_type = get_innobase_type_from_mysql_type(field);
		is_unsigned = (ulint) (field->flags & UNSIGNED_FLAG);

		switch (col_type) {

		case DATA_BLOB:
			o_ptr = row_mysql_read_blob_ref(&o_len, o_ptr, o_len);
			n_ptr = row_mysql_read_blob_ref(&n_len, n_ptr, n_len);
			break;
		case DATA_VARCHAR:
		case DATA_BINARY:
		case DATA_VARMYSQL:
			o_ptr = row_mysql_read_var_ref_noninline(&o_len, o_ptr);
			n_ptr = row_mysql_read_var_ref_noninline(&n_len, n_ptr);
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

			buf = (byte*)
                          innobase_convert_and_store_changed_col(ufield,
					  (mysql_byte*)buf,
					  (mysql_byte*)n_ptr, n_len, col_type,
						is_unsigned);
			ufield->exp = NULL;
			ufield->field_no =
					(prebuilt->table->cols + i)->clust_pos;
			n_changed++;
		}
		;
	}

	uvect->n_fields = n_changed;
	uvect->info_bits = 0;

	return(0);
}

/**************************************************************************
Updates a row given as a parameter to a new value. Note that we are given
whole rows, not just the fields which are updated: this incurs some
overhead for CPU when we check which fields are actually updated.
TODO: currently InnoDB does not prevent the 'Halloween problem':
in a searched update a single row can get updated several times
if its index columns are updated! */

int
ha_innobase::update_row(
/*====================*/
					/* out: error number or 0 */
	const mysql_byte* 	old_row,/* in: old row in MySQL format */
	mysql_byte* 		new_row)/* in: new row in MySQL format */
{
	row_prebuilt_t*	prebuilt = (row_prebuilt_t*) innobase_prebuilt;
	upd_t*		uvect;
	int		error = 0;

	DBUG_ENTER("ha_innobase::update_row");

	ut_a(prebuilt->trx ==
		(trx_t*) current_thd->transaction.all.innobase_tid);

        if (table->time_stamp) {
                update_timestamp(new_row + table->time_stamp - 1);
	}

	if (last_query_id != user_thd->query_id) {
	        prebuilt->sql_stat_start = TRUE;
                last_query_id = user_thd->query_id;

		innobase_release_stat_resources(prebuilt->trx);
	}

	if (prebuilt->upd_node) {
		uvect = prebuilt->upd_node->update;
	} else {
		uvect = row_get_prebuilt_update_vector(prebuilt);
	}

	/* Build an update vector from the modified fields in the rows
	(uses upd_buff of the handle) */

	calc_row_difference(uvect, (mysql_byte*) old_row, new_row, table,
						upd_buff, prebuilt, user_thd);
	/* This is not a delete */
	prebuilt->upd_node->is_delete = FALSE;

	assert(prebuilt->template_type == ROW_MYSQL_WHOLE_ROW);

	srv_conc_enter_innodb(prebuilt->trx);

	error = row_update_for_mysql((byte*) old_row, prebuilt);

	srv_conc_exit_innodb(prebuilt->trx);

	error = convert_error_code_to_mysql(error, user_thd);

	/* Tell InnoDB server that there might be work for
	utility threads: */

	innobase_active_small();

	DBUG_RETURN(error);
}

/**************************************************************************
Deletes a row given as the parameter. */

int
ha_innobase::delete_row(
/*====================*/
					/* out: error number or 0 */
	const mysql_byte* record)	/* in: a row in MySQL format */
{
	row_prebuilt_t*	prebuilt = (row_prebuilt_t*) innobase_prebuilt;
	int		error = 0;

	DBUG_ENTER("ha_innobase::delete_row");

	ut_a(prebuilt->trx ==
		(trx_t*) current_thd->transaction.all.innobase_tid);

	if (last_query_id != user_thd->query_id) {
	        prebuilt->sql_stat_start = TRUE;
                last_query_id = user_thd->query_id;

		innobase_release_stat_resources(prebuilt->trx);
	}

	if (!prebuilt->upd_node) {
		row_get_prebuilt_update_vector(prebuilt);
	}

	/* This is a delete */

	prebuilt->upd_node->is_delete = TRUE;

	srv_conc_enter_innodb(prebuilt->trx);

	error = row_update_for_mysql((byte*) record, prebuilt);

	srv_conc_exit_innodb(prebuilt->trx);

	error = convert_error_code_to_mysql(error, user_thd);

	/* Tell the InnoDB server that there might be work for
	utility threads: */

	innobase_active_small();

	DBUG_RETURN(error);
}

/**********************************************************************
Initializes a handle to use an index. */

int
ha_innobase::index_init(
/*====================*/
			/* out: 0 or error number */
	uint 	keynr)	/* in: key (index) number */
{
	int 	error	= 0;
  	DBUG_ENTER("index_init");

	error = change_active_index(keynr);

  	DBUG_RETURN(error);
}

/**********************************************************************
Currently does nothing. */

int
ha_innobase::index_end(void)
/*========================*/
{
	int 	error	= 0;
  	DBUG_ENTER("index_end");

  	DBUG_RETURN(error);
}

/*************************************************************************
Converts a search mode flag understood by MySQL to a flag understood
by InnoDB. */
inline
ulint
convert_search_mode_to_innobase(
/*============================*/
	enum ha_rkey_function	find_flag)
{
	switch (find_flag) {
  		case HA_READ_KEY_EXACT:		return(PAGE_CUR_GE);
  			/* the above does not require the index to be UNIQUE */
  		case HA_READ_KEY_OR_NEXT:	return(PAGE_CUR_GE);
		case HA_READ_KEY_OR_PREV:	return(PAGE_CUR_LE);
		case HA_READ_AFTER_KEY:		return(PAGE_CUR_G);
		case HA_READ_BEFORE_KEY:	return(PAGE_CUR_L);
		case HA_READ_PREFIX:		return(PAGE_CUR_GE);
		case HA_READ_PREFIX_LAST:	return(PAGE_CUR_LE);
			/* the above PREFIX flags mean that the last
			field in the key value may just be a prefix
			of the complete fixed length field */
		default:			assert(0);
	}

	return(0);
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

int
ha_innobase::index_read(
/*====================*/
					/* out: 0, HA_ERR_KEY_NOT_FOUND,
					or error number */
	mysql_byte*		buf,	/* in/out: buffer for the returned
					row */
	const mysql_byte* 	key_ptr,/* in: key value; if this is NULL
					we position the cursor at the
					start or end of index; this can
					also contain an InnoDB row id, in
					which case key_len is the InnoDB
					row id length */
	uint			key_len,/* in: key value length */
	enum ha_rkey_function find_flag)/* in: search flags from my_base.h */
{
	row_prebuilt_t* prebuilt	= (row_prebuilt_t*) innobase_prebuilt;
	ulint		mode;
	dict_index_t*	index;
	ulint		match_mode 	= 0;
	int 		error;
	ulint		ret;

  	DBUG_ENTER("index_read");

	ut_a(prebuilt->trx ==
		(trx_t*) current_thd->transaction.all.innobase_tid);

  	statistic_increment(ha_read_key_count, &LOCK_status);

	if (last_query_id != user_thd->query_id) {
	        prebuilt->sql_stat_start = TRUE;
                last_query_id = user_thd->query_id;

		innobase_release_stat_resources(prebuilt->trx);
	}

	index = prebuilt->index;

	/* Note that if the index for which the search template is built is not
        necessarily prebuilt->index, but can also be the clustered index */

	if (prebuilt->sql_stat_start) {
		build_template(prebuilt, user_thd, table,
							ROW_MYSQL_REC_FIELDS);
	}

	if (key_ptr) {
	        /* Convert the search key value to InnoDB format into
		prebuilt->search_tuple */

		row_sel_convert_mysql_key_to_innobase(prebuilt->search_tuple,
							(byte*) key_val_buff,
							index,
							(byte*) key_ptr,
							(ulint) key_len);
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

	last_match_mode = match_mode;

	srv_conc_enter_innodb(prebuilt->trx);

	ret = row_search_for_mysql((byte*) buf, mode, prebuilt, match_mode, 0);

	srv_conc_exit_innodb(prebuilt->trx);

	if (ret == DB_SUCCESS) {
		error = 0;
		table->status = 0;

	} else if (ret == DB_RECORD_NOT_FOUND) {
		error = HA_ERR_KEY_NOT_FOUND;
		table->status = STATUS_NOT_FOUND;

	} else if (ret == DB_END_OF_INDEX) {
		error = HA_ERR_KEY_NOT_FOUND;
		table->status = STATUS_NOT_FOUND;
	} else {
		error = convert_error_code_to_mysql(ret, user_thd);
		table->status = STATUS_NOT_FOUND;
	}

	DBUG_RETURN(error);
}

/************************************************************************
Changes the active index of a handle. */

int
ha_innobase::change_active_index(
/*=============================*/
			/* out: 0 or error code */
	uint 	keynr)	/* in: use this index; MAX_KEY means always clustered
			index, even if it was internally generated by
			InnoDB */
{
	row_prebuilt_t* prebuilt	= (row_prebuilt_t*) innobase_prebuilt;
	KEY*		key=0;

  	statistic_increment(ha_read_key_count, &LOCK_status);

  	DBUG_ENTER("change_active_index");

	active_index = keynr;

	if (keynr != MAX_KEY && table->keys > 0) {
		key = table->key_info + active_index;

		prebuilt->index = dict_table_get_index_noninline(
						prebuilt->table, key->name);
	} else {
		prebuilt->index = dict_table_get_first_index_noninline(
							prebuilt->table);
	}

	if (!prebuilt->index) {
		fprintf(stderr,
	"InnoDB: Could not find key n:o %u with name %s from dict cache\n"
	"InnoDB: for table %s\n", keynr, key ? key->name : "NULL", prebuilt->table->name);

		DBUG_RETURN(1);
	}
	
	assert(prebuilt->search_tuple != 0);

	dtuple_set_n_fields(prebuilt->search_tuple, prebuilt->index->n_fields);

 	dict_index_copy_types(prebuilt->search_tuple, prebuilt->index,
						prebuilt->index->n_fields);

	/* Maybe MySQL changes the active index for a handle also
	during some queries, we do not know: then it is safest to build
	the template such that all columns will be fetched */

	build_template(prebuilt, user_thd, table, ROW_MYSQL_WHOLE_ROW);

	DBUG_RETURN(0);
}

/**************************************************************************
Positions an index cursor to the index specified in keynr. Fetches the
row if any. */
/* ??? This is only used to read whole keys ??? */

int
ha_innobase::index_read_idx(
/*========================*/
					/* out: error number or 0 */
	mysql_byte*	buf,		/* in/out: buffer for the returned
					row */
	uint 		keynr,		/* in: use this index */
	const mysql_byte* key,		/* in: key value; if this is NULL
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

int
ha_innobase::general_fetch(
/*=======================*/
				/* out: 0, HA_ERR_END_OF_FILE, or error
				number */
	mysql_byte* 	buf,	/* in/out: buffer for next row in MySQL
				format */
	uint 	direction,	/* in: ROW_SEL_NEXT or ROW_SEL_PREV */
	uint	match_mode)	/* in: 0, ROW_SEL_EXACT, or
				ROW_SEL_EXACT_PREFIX */
{
	row_prebuilt_t*	prebuilt = (row_prebuilt_t*) innobase_prebuilt;
	ulint		ret;
	int		error	= 0;

	DBUG_ENTER("general_fetch");

	ut_a(prebuilt->trx ==
		(trx_t*) current_thd->transaction.all.innobase_tid);

	srv_conc_enter_innodb(prebuilt->trx);
	
	ret = row_search_for_mysql((byte*)buf, 0, prebuilt, match_mode,
								direction);
	srv_conc_exit_innodb(prebuilt->trx);

	if (ret == DB_SUCCESS) {
		error = 0;
		table->status = 0;

	} else if (ret == DB_RECORD_NOT_FOUND) {
		error = HA_ERR_END_OF_FILE;
		table->status = STATUS_NOT_FOUND;

	} else if (ret == DB_END_OF_INDEX) {
		error = HA_ERR_END_OF_FILE;
		table->status = STATUS_NOT_FOUND;
	} else {
		error = convert_error_code_to_mysql(ret, user_thd);
		table->status = STATUS_NOT_FOUND;
	}

	DBUG_RETURN(error);
}

/***************************************************************************
Reads the next row from a cursor, which must have previously been
positioned using index_read. */

int
ha_innobase::index_next(
/*====================*/
				/* out: 0, HA_ERR_END_OF_FILE, or error
				number */
	mysql_byte* 	buf)	/* in/out: buffer for next row in MySQL
				format */
{
  	statistic_increment(ha_read_next_count, &LOCK_status);

	return(general_fetch(buf, ROW_SEL_NEXT, 0));
}

/***********************************************************************
Reads the next row matching to the key value given as the parameter. */

int
ha_innobase::index_next_same(
/*=========================*/
				/* out: 0, HA_ERR_END_OF_FILE, or error
				number */
	mysql_byte* 	buf,	/* in/out: buffer for the row */
	const mysql_byte* key,	/* in: key value */
	uint 		keylen)	/* in: key value length */
{
  	statistic_increment(ha_read_next_count, &LOCK_status);

	return(general_fetch(buf, ROW_SEL_NEXT, last_match_mode));
}

/***************************************************************************
Reads the previous row from a cursor, which must have previously been
positioned using index_read. */

int
ha_innobase::index_prev(
/*====================*/
				/* out: 0, HA_ERR_END_OF_FILE, or error
				number */
	mysql_byte* 	buf)	/* in/out: buffer for previous row in MySQL
				format */
{
	return(general_fetch(buf, ROW_SEL_PREV, 0));
}

/************************************************************************
Positions a cursor on the first record in an index and reads the
corresponding row to buf. */

int
ha_innobase::index_first(
/*=====================*/
				/* out: 0, HA_ERR_END_OF_FILE,
				or error code */
	mysql_byte*	buf)	/* in/out: buffer for the row */
{
	int	error;

  	DBUG_ENTER("index_first");
  	statistic_increment(ha_read_first_count, &LOCK_status);

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

int
ha_innobase::index_last(
/*====================*/
				/* out: 0, HA_ERR_END_OF_FILE, or error code */
	mysql_byte*	buf)	/* in/out: buffer for the row */
{
	int	error;

  	DBUG_ENTER("index_first");
  	statistic_increment(ha_read_last_count, &LOCK_status);

  	error = index_read(buf, NULL, 0, HA_READ_BEFORE_KEY);

        /* MySQL does not seem to allow this to return HA_ERR_KEY_NOT_FOUND */

  	if (error == HA_ERR_KEY_NOT_FOUND) {
  		error = HA_ERR_END_OF_FILE;
  	}

  	DBUG_RETURN(error);
}

/********************************************************************
Initialize a table scan. */

int
ha_innobase::rnd_init(
/*==================*/
			/* out: 0 or error number */
	bool	scan)	/* in: ???????? */
{
	int	err;
	
	row_prebuilt_t*	prebuilt = (row_prebuilt_t*) innobase_prebuilt;

	if (prebuilt->clust_index_was_generated) {
		err = change_active_index(MAX_KEY);
	} else {
		err = change_active_index(primary_key);
	}

  	start_of_scan = 1;

 	return(err);
}

/*********************************************************************
Ends a table scan ???????????????? */

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

int
ha_innobase::rnd_next(
/*==================*/
			/* out: 0, HA_ERR_END_OF_FILE, or error number */
	mysql_byte* buf)/* in/out: returns the row in this buffer,
			in MySQL format */
{
	int	error;

  	DBUG_ENTER("rnd_next");
  	statistic_increment(ha_read_rnd_next_count, &LOCK_status);

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

int
ha_innobase::rnd_pos(
/*=================*/
				/* out: 0, HA_ERR_KEY_NOT_FOUND,
				or error code */
	mysql_byte* 	buf,	/* in/out: buffer for the row */
	mysql_byte*	pos)	/* in: primary key value of the row in the
				MySQL format, or the row id if the clustered
				index was internally generated by InnoDB;
				the length of data in pos has to be
				ref_length */
{
	row_prebuilt_t*	prebuilt = (row_prebuilt_t*) innobase_prebuilt;
	int		error;
	uint		keynr	= active_index;

	DBUG_ENTER("rnd_pos");
	statistic_increment(ha_read_rnd_count, &LOCK_status);

	ut_a(prebuilt->trx ==
		(trx_t*) current_thd->transaction.all.innobase_tid);

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
		DBUG_RETURN(error);
	}
	
	/* Note that we assume the length of the row reference is fixed
        for the table, and it is == ref_length */

	error = index_read(buf, pos, ref_length, HA_READ_KEY_EXACT);

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

void
ha_innobase::position(
/*==================*/
	const mysql_byte*	record)	/* in: row in MySQL format */
{
	row_prebuilt_t*	prebuilt = (row_prebuilt_t*) innobase_prebuilt;
	uint		len;

	ut_a(prebuilt->trx ==
		(trx_t*) current_thd->transaction.all.innobase_tid);

	if (prebuilt->clust_index_was_generated) {
		/* No primary key was defined for the table and we
		generated the clustered index from row id: the
		row reference will be the row id, not any key value
		that MySQL knows of */

		len = DATA_ROW_ID_LEN;

		memcpy(ref, prebuilt->row_id, len);
	} else {
		len = store_key_val_for_row(primary_key, (char*) ref, record);
	}

	/* Since we do not store len to the buffer 'ref', we must assume
	that len is always fixed for this table. The following assertion
	checks this. */

	ut_a(len == ref_length);
}

/*********************************************************************
Creates a table definition to an InnoDB database. */
static
int
create_table_def(
/*=============*/
	trx_t*		trx,		/* in: InnoDB transaction handle */
	TABLE*		form,		/* in: information on table
					columns and indexes */
	const char*	table_name)	/* in: table name */
{
	Field*		field;
	dict_table_t*	table;
	ulint		n_cols;
  	int 		error;
  	ulint		col_type;
  	ulint		nulls_allowed;
	ulint		unsigned_type;
  	ulint		i;

  	DBUG_ENTER("create_table_def");
  	DBUG_PRINT("enter", ("table_name: %s", table_name));

	n_cols = form->fields;

	/* The '0' below specifies that everything is currently
	created in tablespace 0 */

	table = dict_mem_table_create((char*) table_name, 0, n_cols);

	for (i = 0; i < n_cols; i++) {
		field = form->field[i];

		col_type = get_innobase_type_from_mysql_type(field);
		if (field->null_ptr) {
			nulls_allowed = 0;
		} else {
			nulls_allowed = DATA_NOT_NULL;
		}

		if (field->flags & UNSIGNED_FLAG) {
			unsigned_type = DATA_UNSIGNED;
		} else {
			unsigned_type = 0;
		}

		dict_mem_table_add_col(table, (char*) field->field_name,
					col_type, (ulint)field->type()
					| nulls_allowed | unsigned_type,
					field->pack_length(), 0);
	}

	error = row_create_table_for_mysql(table, trx);

	error = convert_error_code_to_mysql(error, NULL);

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
	const char*	table_name,	/* in: table name */
	uint		key_num)	/* in: index number */
{
	dict_index_t*	index;
  	int 		error;
	ulint		n_fields;
	KEY*		key;
	KEY_PART_INFO*	key_part;
	ulint		ind_type;
  	ulint		i;

  	DBUG_ENTER("create_index");

	key = form->key_info + key_num;

    	n_fields = key->key_parts;

    	ind_type = 0;

    	if (key_num == form->primary_key)
	{
		ind_type = ind_type | DICT_CLUSTERED;
	}

	if (key->flags & HA_NOSAME ) {
		ind_type = ind_type | DICT_UNIQUE;
	}

	/* The '0' below specifies that everything in InnoDB is currently
	created in tablespace 0 */

	index = dict_mem_index_create((char*) table_name, key->name, 0,
						ind_type, n_fields);
	for (i = 0; i < n_fields; i++) {
		key_part = key->key_part + i;

		/* We assume all fields should be sorted in ascending
		order, hence the '0': */
		dict_mem_index_add_field(index,
				(char*) key_part->field->field_name, 0);
	}

	error = row_create_index_for_mysql(index, trx);

	error = convert_error_code_to_mysql(error, NULL);

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
	const char*	table_name)	/* in: table name */
{
	dict_index_t*	index;
  	int 		error;

	/* The first '0' below specifies that everything in InnoDB is
	currently created in file space 0 */

	index = dict_mem_index_create((char*) table_name,
				      (char*) "GEN_CLUST_INDEX",
				      0, DICT_CLUSTERED, 0);
	error = row_create_index_for_mysql(index, trx);

	error = convert_error_code_to_mysql(error, NULL);

	return(error);
}

/*********************************************************************
Creates a new table to an InnoDB database. */

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
	trx_t*		trx;
	int		primary_key_no;
	uint		i;
	char		name2[FN_REFLEN];
	char		norm_name[FN_REFLEN];
	THD		*thd= current_thd;

  	DBUG_ENTER("ha_innobase::create");

	DBUG_ASSERT(thd != NULL);

	trx = trx_allocate_for_mysql();

	if (thd->options & OPTION_NO_FOREIGN_KEY_CHECKS) {
		trx->check_foreigns = FALSE;
	}

	if (thd->options & OPTION_RELAXED_UNIQUE_CHECKS) {
		trx->check_unique_secondary = FALSE;
	}

	fn_format(name2, name, "", "",2);	// Remove the .frm extension

	normalize_table_name(norm_name, name2);

	/* Latch the InnoDB data dictionary exclusive so that no deadlocks
	or lock waits can happen in it during a table create operation.
	(Drop table etc. do this latching in row0mysql.c.) */

	row_mysql_lock_data_dictionary();

	/* Create the table definition in InnoDB */

  	error = create_table_def(trx, form, norm_name);
  	
  	if (error) {
		innobase_commit_low(trx);

		row_mysql_unlock_data_dictionary();

  		trx_free_for_mysql(trx);

 		DBUG_RETURN(error);
 	}

	/* Look for a primary key */

	primary_key_no= (table->primary_key != MAX_KEY ?
			 (int) table->primary_key : 
			 -1);

	/* Our function row_get_mysql_key_number_for_index assumes
	the primary key is always number 0, if it exists */

	assert(primary_key_no == -1 || primary_key_no == 0);

	/* Create the keys */

	if (form->keys == 0 || primary_key_no == -1) {
		/* Create an index which is used as the clustered index;
		order the rows by their row id which is internally generated
		by InnoDB */

		error = create_clustered_index_when_no_primary(trx,
							norm_name);
  		if (error) {
			innobase_commit_low(trx);

			row_mysql_unlock_data_dictionary();

			trx_free_for_mysql(trx);

			DBUG_RETURN(error);
      		}
	}

	if (primary_key_no != -1) {
		/* In InnoDB the clustered index must always be created
		first */
	    	if ((error = create_index(trx, form, norm_name,
					  (uint) primary_key_no))) {
			innobase_commit_low(trx);

			row_mysql_unlock_data_dictionary();

  			trx_free_for_mysql(trx);

			DBUG_RETURN(error);
      		}
      	}

	for (i = 0; i < form->keys; i++) {

		if (i != (uint) primary_key_no) {

    			if ((error = create_index(trx, form, norm_name, i))) {

				innobase_commit_low(trx);

				row_mysql_unlock_data_dictionary();

				trx_free_for_mysql(trx);

				DBUG_RETURN(error);
      			}
      		}
  	}

	error = row_table_add_foreign_constraints(trx,
				create_info->create_statement, norm_name);

	error = convert_error_code_to_mysql(error, NULL);

	if (error) {
		innobase_commit_low(trx);

		row_mysql_unlock_data_dictionary();

  		trx_free_for_mysql(trx);

		DBUG_RETURN(error);
	}

  	innobase_commit_low(trx);

	row_mysql_unlock_data_dictionary();

	/* Flush the log to reduce probability that the .frm files and
	the InnoDB data dictionary get out-of-sync if the user runs
	with innodb_flush_log_at_trx_commit = 0 */
	
	log_flush_up_to(ut_dulint_max, LOG_WAIT_ONE_GROUP);

	innobase_table = dict_table_get(norm_name, NULL);

	assert(innobase_table != 0);

	/* Tell the InnoDB server that there might be work for
	utility threads: */

	srv_active_wake_master_thread();

  	trx_free_for_mysql(trx);

	DBUG_RETURN(0);
}

/*********************************************************************
Drops a table from an InnoDB database. Before calling this function,
MySQL calls innobase_commit to commit the transaction of the current user.
Then the current user cannot have locks set on the table. Drop table
operation inside InnoDB will remove all locks any user has on the table
inside InnoDB. */

int
ha_innobase::delete_table(
/*======================*/
				/* out: error number */
	const char*	name)	/* in: table name */
{
	ulint	name_len;
	int	error;
	trx_t*	trx;
	char	norm_name[1000];

  	DBUG_ENTER("ha_innobase::delete_table");

	trx = trx_allocate_for_mysql();

	name_len = strlen(name);

	assert(name_len < 1000);

	/* Strangely, MySQL passes the table name without the '.frm'
	extension, in contrast to ::create */

	normalize_table_name(norm_name, name);

  	/* Drop the table in InnoDB */

  	error = row_drop_table_for_mysql(norm_name, trx, FALSE);

	/* Flush the log to reduce probability that the .frm files and
	the InnoDB data dictionary get out-of-sync if the user runs
	with innodb_flush_log_at_trx_commit = 0 */
	
	log_flush_up_to(ut_dulint_max, LOG_WAIT_ONE_GROUP);

	/* Tell the InnoDB server that there might be work for
	utility threads: */

	srv_active_wake_master_thread();

  	innobase_commit_low(trx);

  	trx_free_for_mysql(trx);

	error = convert_error_code_to_mysql(error, NULL);

	DBUG_RETURN(error);
}

/*********************************************************************
Removes all tables in the named database inside InnoDB. */

int
innobase_drop_database(
/*===================*/
			/* out: error number */
	char*	path)	/* in: database path; inside InnoDB the name
			of the last directory in the path is used as
			the database name: for example, in 'mysql/data/test'
			the database name is 'test' */
{
	ulint	len		= 0;
	trx_t*	trx;
	char*	ptr;
	int	error;
	char	namebuf[10000];
	
	ptr = strend(path) - 2;
	
	while (ptr >= path && *ptr != '\\' && *ptr != '/') {
		ptr--;
		len++;
	}

	ptr++;

	memcpy(namebuf, ptr, len);
	namebuf[len] = '/';
	namebuf[len + 1] = '\0';
	
#ifdef __WIN__
	casedn_str(namebuf);
#endif
	trx = trx_allocate_for_mysql();

  	error = row_drop_database_for_mysql(namebuf, trx);

	/* Flush the log to reduce probability that the .frm files and
	the InnoDB data dictionary get out-of-sync if the user runs
	with innodb_flush_log_at_trx_commit = 0 */
	
	log_flush_up_to(ut_dulint_max, LOG_WAIT_ONE_GROUP);

	/* Tell the InnoDB server that there might be work for
	utility threads: */

	srv_active_wake_master_thread();

  	innobase_commit_low(trx);
  	trx_free_for_mysql(trx);

	error = convert_error_code_to_mysql(error, NULL);

	return(error);
}

/*************************************************************************
Renames an InnoDB table. */

int
ha_innobase::rename_table(
/*======================*/
				/* out: 0 or error code */
	const char*	from,	/* in: old name of the table */
	const char*	to)	/* in: new name of the table */
{
	ulint	name_len1;
	ulint	name_len2;
	int	error;
	trx_t*	trx;
	char	norm_from[1000];
	char	norm_to[1000];

  	DBUG_ENTER("ha_innobase::rename_table");

	trx = trx_allocate_for_mysql();

	name_len1 = strlen(from);
	name_len2 = strlen(to);

	assert(name_len1 < 1000);
	assert(name_len2 < 1000);

	normalize_table_name(norm_from, from);
	normalize_table_name(norm_to, to);

  	/* Rename the table in InnoDB */

  	error = row_rename_table_for_mysql(norm_from, norm_to, trx);

	/* Flush the log to reduce probability that the .frm files and
	the InnoDB data dictionary get out-of-sync if the user runs
	with innodb_flush_log_at_trx_commit = 0 */
	
	log_flush_up_to(ut_dulint_max, LOG_WAIT_ONE_GROUP);

	/* Tell the InnoDB server that there might be work for
	utility threads: */

	srv_active_wake_master_thread();

  	innobase_commit_low(trx);
  	trx_free_for_mysql(trx);

	error = convert_error_code_to_mysql(error, NULL);

	DBUG_RETURN(error);
}

/*************************************************************************
Estimates the number of index records in a range. */

ha_rows
ha_innobase::records_in_range(
/*==========================*/
						/* out: estimated number of rows,
						currently 32-bit int or uint */
	int 			keynr,		/* in: index number */
	const mysql_byte*	start_key,	/* in: start key value of the
						range, may also be empty */
	uint 			start_key_len,	/* in: start key val len, may
						also be 0 */
	enum ha_rkey_function 	start_search_flag,/* in: start search condition
						e.g., 'greater than' */
	const mysql_byte*	end_key,	/* in: range end key val, may
						also be empty */
	uint 			end_key_len,	/* in: range end key val len,
						may also be 0 */
	enum ha_rkey_function 	end_search_flag)/* in: range end search cond */
{
	row_prebuilt_t* prebuilt	= (row_prebuilt_t*) innobase_prebuilt;
	KEY*		key;
	dict_index_t*	index;
	mysql_byte*	key_val_buff2 	= (mysql_byte*) my_malloc(
						  table->reclength
      						+ table->max_key_length + 100,
								MYF(MY_WME));
	dtuple_t*	range_start;
	dtuple_t*	range_end;
	ulint		n_rows;
	ulint		mode1;
	ulint		mode2;
	void*           heap1;
	void*           heap2;

   	DBUG_ENTER("records_in_range");

	/* Warning: since it is not sure that MySQL calls external_lock
	before calling this function, the trx field in prebuilt can be
	obsolete! */
   	
	active_index = keynr;

	key = table->key_info + active_index;

	index = dict_table_get_index_noninline(prebuilt->table, key->name);

	range_start = dtuple_create_for_mysql(&heap1, key->key_parts);
 	dict_index_copy_types(range_start, index, key->key_parts);

	range_end = dtuple_create_for_mysql(&heap2, key->key_parts);
 	dict_index_copy_types(range_end, index, key->key_parts);

	row_sel_convert_mysql_key_to_innobase(
				range_start, (byte*) key_val_buff, index,
				(byte*) start_key,
				(ulint) start_key_len);

	row_sel_convert_mysql_key_to_innobase(
				range_end, (byte*) key_val_buff2, index,
				(byte*) end_key,
				(ulint) end_key_len);

	mode1 = convert_search_mode_to_innobase(start_search_flag);
	mode2 = convert_search_mode_to_innobase(end_search_flag);

	n_rows = btr_estimate_n_rows_in_range(index, range_start,
						mode1, range_end, mode2);
	dtuple_free_for_mysql(heap1);
	dtuple_free_for_mysql(heap2);

    	my_free((char*) key_val_buff2, MYF(0));

	DBUG_RETURN((ha_rows) n_rows);
}

/*************************************************************************
Gives an UPPER BOUND to the number of rows in a table. This is used in
filesort.cc and the upper bound must hold. TODO: Since the number of
rows in a table may change after this function is called, we still may
get a 'Sort aborted' error in filesort.cc of MySQL. The ultimate fix is to
improve the algorithm of filesort.cc. */

ha_rows
ha_innobase::estimate_number_of_rows(void)
/*======================================*/
			/* out: upper bound of rows */
{
	row_prebuilt_t* prebuilt	= (row_prebuilt_t*) innobase_prebuilt;
	dict_index_t*	index;
	ulonglong	estimate;
	ulonglong	data_file_length;
	
	/* Warning: since it is not sure that MySQL calls external_lock
	before calling this function, the trx field in prebuilt can be
	obsolete! */

 	DBUG_ENTER("info");

	index = dict_table_get_first_index_noninline(prebuilt->table);
 	
	data_file_length = ((ulonglong) index->stat_n_leaf_pages)
    							* UNIV_PAGE_SIZE;

	/* Calculate a minimum length for a clustered index record and from
	that an upper bound for the number of rows. Since we only calculate
	new statistics in row0mysql.c when a tablehas grown
        by a threshold factor, we must add a safety factor 2 in front
	of the formula below. */

	estimate = 2 * data_file_length / dict_index_calc_min_rec_len(index);
	
	DBUG_RETURN((ha_rows) estimate);
}

/*************************************************************************
How many seeks it will take to read through the table. This is to be
comparable to the number returned by records_in_range so that we can
decide if we should scan the table or use keys. */

double
ha_innobase::scan_time()
/*====================*/
			/* out: estimated time measured in disk seeks */
{
	row_prebuilt_t* prebuilt	= (row_prebuilt_t*) innobase_prebuilt;

	/* Since MySQL seems to favor table scans too much over index
	searches, we pretend that a sequential read takes the same time
	as a random disk read, that is, we do not divide the following
	by 10, which would be physically realistic. */
	
	return((double) (prebuilt->table->stat_clustered_index_size));
}

/*************************************************************************
Returns statistics information of the table to the MySQL interpreter,
in various fields of the handle object. */

void
ha_innobase::info(
/*==============*/
	uint flag)	/* in: what information MySQL requests */
{
	row_prebuilt_t* prebuilt	= (row_prebuilt_t*) innobase_prebuilt;
	dict_table_t*	ib_table;
	dict_index_t*	index;
	ulong		rec_per_key;
	ulong		j;
	ulong		i;

 	DBUG_ENTER("info");

	/* Warning: since it is not sure that MySQL calls external_lock
	before calling this function, the trx field in prebuilt can be
	obsolete! */
   	   	
 	ib_table = prebuilt->table;

 	if (flag & HA_STATUS_TIME) {
 		/* In sql_show we call with this flag: update then statistics
 		so that they are up-to-date */

 		dict_update_statistics(ib_table);
 	}

	if (flag & HA_STATUS_VARIABLE) {
    		records = (ha_rows)ib_table->stat_n_rows;
    		deleted = 0;
    		data_file_length = ((ulonglong)
				ib_table->stat_clustered_index_size)
    					* UNIV_PAGE_SIZE;
    		index_file_length = ((ulonglong)
				ib_table->stat_sum_of_other_index_sizes)
    					* UNIV_PAGE_SIZE;
    		delete_length = 0;
    		check_time = 0;

    		if (records == 0) {
    			mean_rec_length = 0;
    		} else {
    			mean_rec_length = (ulong) (data_file_length / records);
    		}
    	}

	if (flag & HA_STATUS_CONST) {
		index = dict_table_get_first_index_noninline(ib_table);

		if (prebuilt->clust_index_was_generated) {
			index = dict_table_get_next_index_noninline(index);
		}

		for (i = 0; i < table->keys; i++) {
			for (j = 0; j < table->key_info[i].key_parts; j++) {

				if (index->stat_n_diff_key_vals[j + 1] == 0) {

					rec_per_key = records;
				} else {
					rec_per_key = (ulong)(records /
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
			
				table->key_info[i].rec_per_key[j]
								= rec_per_key;
			}
			
			index = dict_table_get_next_index_noninline(index);
		}
	}

	/* The trx struct in InnoDB contains a pthread mutex embedded:
	in the debug version of MySQL that it replaced by a 'safe mutex'
	which is of a different size. We have to use a function to access
	trx fields. Otherwise trx->error_info will be a random
	pointer and cause a seg fault. */

  	if (flag & HA_STATUS_ERRKEY) {
		ut_a(prebuilt->trx && prebuilt->trx->magic_n == TRX_MAGIC_N);

		errkey = (unsigned int) row_get_mysql_key_number_for_index(
				       (dict_index_t*)
				       trx_get_error_info(prebuilt->trx));
  	}

  	DBUG_VOID_RETURN;
}

/***********************************************************************
Tries to check that an InnoDB table is not corrupted. If corruption is
noticed, prints to stderr information about it. In case of corruption
may also assert a failure and crash the server. */

int
ha_innobase::check(
/*===============*/
					/* out: HA_ADMIN_CORRUPT or
					HA_ADMIN_OK */
	THD* 		thd,		/* in: user thread handle */
	HA_CHECK_OPT* 	check_opt)	/* in: check options, currently
					ignored */
{
	row_prebuilt_t* prebuilt	= (row_prebuilt_t*) innobase_prebuilt;
	ulint		ret;
	   	
	ut_a(prebuilt->trx && prebuilt->trx->magic_n == TRX_MAGIC_N);
	ut_a(prebuilt->trx ==
		(trx_t*) current_thd->transaction.all.innobase_tid);

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

char*
ha_innobase::update_table_comment(
/*==============================*/
				/* out: table comment + InnoDB free space +
				info on foreign keys */
        const char*	comment)/* in: table comment defined by user */
{
	row_prebuilt_t* prebuilt = (row_prebuilt_t*)innobase_prebuilt;
  	uint 		length 	= strlen(comment);
  	char*		str 	= my_malloc(length + 550, MYF(0));
  	char*		pos;

	/* Warning: since it is not sure that MySQL calls external_lock
	before calling this function, the trx field in prebuilt can be
	obsolete! */
   	
	if (!str) {
    		return((char*)comment);
	}

	pos = str;
  	if (length) {
    		pos=strmov(str, comment);
    		*pos++=';';
    		*pos++=' ';
  	}

  	pos += my_sprintf(pos,
			  (pos,"InnoDB free: %lu kB",
			   (ulong) innobase_get_free_space()));

	/* We assume 450 - length bytes of space to print info */

	if (length < 450) {
  		dict_print_info_on_foreign_keys(FALSE, pos, 450 - length,
							prebuilt->table);
	}
  
  	return(str);
}

/***********************************************************************
Gets the foreign key create info for a table stored in InnoDB. */

char*
ha_innobase::get_foreign_key_create_info(void)
/*==========================================*/
			/* out, own: character string in the form which
			can be inserted to the CREATE TABLE statement,
			MUST be freed with ::free_foreign_key_create_info */
{
	row_prebuilt_t* prebuilt = (row_prebuilt_t*)innobase_prebuilt;
	char*	str;
	
	if (prebuilt == NULL) {
		fprintf(stderr,
"InnoDB: Error: cannot get create info for foreign keys\n");

		return(NULL);
	}

	str = (char*)ut_malloc(10000);

	str[0] = '\0';
	
  	dict_print_info_on_foreign_keys(TRUE, str, 9000, prebuilt->table);

  	return(str);
}			

/***********************************************************************
Frees the foreign key create info for a table stored in InnoDB, if it is
non-NULL. */

void
ha_innobase::free_foreign_key_create_info(
/*======================================*/
	char*	str)	/* in, own: create info string to free  */
{
	if (str) {
		ut_free(str);
	}
}

/***********************************************************************
Tells something additional to the handler about how to do things. */

int
ha_innobase::extra(
/*===============*/
			   /* out: 0 or error number */
	enum ha_extra_function operation)
                           /* in: HA_EXTRA_DONT_USE_CURSOR_TO_UPDATE */
{
	row_prebuilt_t*	prebuilt = (row_prebuilt_t*) innobase_prebuilt;

	/* Warning: since it is not sure that MySQL calls external_lock
	before calling this function, the trx field in prebuilt can be
	obsolete! */

	switch (operation) {
 		case HA_EXTRA_RESET:
  		case HA_EXTRA_RESET_STATE:
	        	prebuilt->read_just_key = 0;
	        	break;
		case HA_EXTRA_NO_KEYREAD:
    			prebuilt->read_just_key = 0;
    			break;
	        case HA_EXTRA_DONT_USE_CURSOR_TO_UPDATE:
			prebuilt->hint_no_need_to_fetch_extra_cols = FALSE;
			break;
	        case HA_EXTRA_KEYREAD:
	        	prebuilt->read_just_key = 1;
	        	break;
		default:/* Do nothing */
			;
	}

	return(0);
}

/**********************************************************************
????????????? */

int
ha_innobase::reset(void)
/*====================*/
{
  	return(0);
}

/**********************************************************************
As MySQL will execute an external lock for every new table it uses when it
starts to process an SQL statement, we can use this function to store the
pointer to the THD in the handle. We will also use this function to communicate
to InnoDB that a new SQL statement has started and that we must store a
savepoint to our transaction handle, so that we are able to roll back
the SQL statement in case of an error. */

int
ha_innobase::external_lock(
/*=======================*/
	THD*	thd,		/* in: handle to the user thread */
	int 	lock_type)	/* in: lock type */
{
	row_prebuilt_t* prebuilt = (row_prebuilt_t*) innobase_prebuilt;
	int 		error = 0;
	trx_t*		trx;

  	DBUG_ENTER("ha_innobase::external_lock");

	update_thd(thd);

	trx = prebuilt->trx;

	prebuilt->sql_stat_start = TRUE;
	prebuilt->hint_no_need_to_fetch_extra_cols = TRUE;

	prebuilt->read_just_key = 0;

	if (lock_type == F_WRLCK) {

		/* If this is a SELECT, then it is in UPDATE TABLE ...
		or SELECT ... FOR UPDATE */
		prebuilt->select_lock_type = LOCK_X;
	}

	if (lock_type != F_UNLCK) {
		if (trx->n_mysql_tables_in_use == 0) {
			trx_mark_sql_stat_end(trx);
		}

		thd->transaction.all.innodb_active_trans = 1;
		trx->n_mysql_tables_in_use++;

		if (thd->tx_isolation == ISO_SERIALIZABLE
		    && prebuilt->select_lock_type == LOCK_NONE) {

		    	/* To get serializable execution we let InnoDB
		    	conceptually add 'LOCK IN SHARE MODE' to all SELECTs
			which otherwise would have been consistent reads */

			prebuilt->select_lock_type = LOCK_S;
		}

		if (prebuilt->select_lock_type != LOCK_NONE) {

		  	trx->mysql_n_tables_locked++;
		}
	} else {
		trx->n_mysql_tables_in_use--;
		auto_inc_counter_for_this_stat = 0;

		if (trx->n_mysql_tables_in_use == 0) {

		  	trx->mysql_n_tables_locked = 0;

			/* Here we release the search latch, auto_inc_lock,
			and InnoDB thread FIFO ticket if they were reserved. */

			innobase_release_stat_resources(trx);

		  	if (!(thd->options
				 & (OPTION_NOT_AUTO_COMMIT | OPTION_BEGIN))) {

		    		innobase_commit(thd, trx);
				thd->transaction.all.innodb_active_trans=0;
		  	}
		}
	}

	DBUG_RETURN(error);
}

/****************************************************************************
Implements the SHOW INNODB STATUS command. Send the output of the InnoDB
Monitor to the client. */

int
innodb_show_status(
/*===============*/
	THD*	thd)	/* in: the MySQL query thread of the caller */
{
	String* 	packet 	= &thd->packet;
	char*		buf;

  	DBUG_ENTER("innodb_show_status");

	/* We let the InnoDB Monitor to output at most 100 kB of text */
	buf = (char*)ut_malloc(100 * 1024);
	
	srv_sprintf_innodb_monitor(buf, 100 * 1024);
	
	List<Item> field_list;

	field_list.push_back(new Item_empty_string("Status", strlen(buf)));

	if(send_fields(thd, field_list, 1)) {
	  	DBUG_RETURN(-1);
	}

  	packet->length(0);
  
  	net_store_data(packet, buf);
  
  	if (my_net_write(&thd->net, (char*)thd->packet.ptr(),
						packet->length())) {
		ut_free(buf);
	
    		DBUG_RETURN(-1);
    	}

	ut_free(buf);

  	send_eof(&thd->net);

  	DBUG_RETURN(0);
}

/****************************************************************************
 Handling the shared INNOBASE_SHARE structure that is needed to provide table
 locking.
****************************************************************************/

static mysql_byte* innobase_get_key(INNOBASE_SHARE *share,uint *length,
			      my_bool not_used __attribute__((unused)))
{
  *length=share->table_name_length;
  return (mysql_byte*) share->table_name;
}

static INNOBASE_SHARE *get_share(const char *table_name)
{
  INNOBASE_SHARE *share;
  pthread_mutex_lock(&innobase_mutex);
  uint length=(uint) strlen(table_name);
  if (!(share=(INNOBASE_SHARE*) hash_search(&innobase_open_tables,
					(mysql_byte*) table_name,
					    length)))
  {
    if ((share=(INNOBASE_SHARE *) my_malloc(sizeof(*share)+length+1,
				       MYF(MY_WME | MY_ZEROFILL))))
    {
      share->table_name_length=length;
      share->table_name=(char*) (share+1);
      strmov(share->table_name,table_name);
      if (hash_insert(&innobase_open_tables, (mysql_byte*) share))
      {
	pthread_mutex_unlock(&innobase_mutex);
	my_free((gptr) share,0);
	return 0;
      }
      thr_lock_init(&share->lock);
      pthread_mutex_init(&share->mutex,MY_MUTEX_INIT_FAST);
    }
  }
  share->use_count++;
  pthread_mutex_unlock(&innobase_mutex);
  return share;
}

static void free_share(INNOBASE_SHARE *share)
{
  pthread_mutex_lock(&innobase_mutex);
  if (!--share->use_count)
  {
    hash_delete(&innobase_open_tables, (mysql_byte*) share);
    thr_lock_delete(&share->lock);
    pthread_mutex_destroy(&share->mutex);
    my_free((gptr) share, MYF(0));
  }
  pthread_mutex_unlock(&innobase_mutex);
}

/*********************************************************************
Stores a MySQL lock into a 'lock' field in a handle. */

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
	enum thr_lock_type 	lock_type)	/* in: lock type to store in
						'lock' */
{
	row_prebuilt_t* prebuilt	= (row_prebuilt_t*) innobase_prebuilt;

	if (lock_type == TL_READ_WITH_SHARED_LOCKS ||
	    lock_type == TL_READ_NO_INSERT) {
		/* This is a SELECT ... IN SHARE MODE, or
		we are doing a complex SQL statement like
		INSERT INTO ... SELECT ... and the logical logging (MySQL
		binlog) requires the use of a locking read */

		prebuilt->select_lock_type = LOCK_S;
	} else {
		/* We set possible LOCK_X value in external_lock, not yet
		here even if this would be SELECT ... FOR UPDATE */

		prebuilt->select_lock_type = LOCK_NONE;
	}

	if (lock_type != TL_IGNORE && lock.type == TL_UNLOCK) {

    		/* If we are not doing a LOCK TABLE, then allow multiple
		writers */

    		if ((lock_type >= TL_WRITE_CONCURRENT_INSERT &&
	 	    lock_type <= TL_WRITE) && !thd->in_lock_tables) {

      			lock_type = TL_WRITE_ALLOW_WRITE;
      		}

 		lock.type=lock_type;
  	}

  	*to++= &lock;

	return(to);
}

/***********************************************************************
This function initializes the auto-inc counter if it has not been
initialized yet. This function does not change the value of the auto-inc
counter if it already has been initialized. In parameter ret returns
the value of the auto-inc counter. */

int
ha_innobase::innobase_read_and_init_auto_inc(
/*=========================================*/
				/* out: 0 or error code: deadlock or
				lock wait timeout */
	longlong*	ret)	/* out: auto-inc value */
{
  	row_prebuilt_t* prebuilt	= (row_prebuilt_t*) innobase_prebuilt;
    	longlong        auto_inc;
  	int     	error;

  	ut_a(prebuilt);
	ut_a(prebuilt->trx ==
		(trx_t*) current_thd->transaction.all.innobase_tid);
	ut_a(prebuilt->table);
	
	auto_inc = dict_table_autoinc_read(prebuilt->table);

	if (auto_inc != 0) {
		/* Already initialized */
		*ret = auto_inc;
	
		return(0);
	}

	srv_conc_enter_innodb(prebuilt->trx);
	error = row_lock_table_autoinc_for_mysql(prebuilt);
	srv_conc_exit_innodb(prebuilt->trx);

	if (error != DB_SUCCESS) {
		error = convert_error_code_to_mysql(error, user_thd);

		goto func_exit;
	}	

	/* Check again if someone has initialized the counter meanwhile */
	auto_inc = dict_table_autoinc_read(prebuilt->table);

	if (auto_inc != 0) {
		*ret = auto_inc;
	
		return(0);
	}

  	(void) extra(HA_EXTRA_KEYREAD);
  	index_init(table->next_number_index);

	/* We use an exclusive lock when we read the max key value from the
  	auto-increment column index. This is because then build_template will
  	advise InnoDB to fetch all columns. In SHOW TABLE STATUS the query
  	id of the auto-increment column is not changed, and previously InnoDB
  	did not fetch it, causing SHOW TABLE STATUS to show wrong values
  	for the autoinc column. */

  	prebuilt->select_lock_type = LOCK_X;

  	/* Play safe and also give in another way the hint to fetch
  	all columns in the key: */
  	
	prebuilt->hint_no_need_to_fetch_extra_cols = FALSE;

	prebuilt->trx->mysql_n_tables_locked += 1;
  
	error = index_last(table->record[1]);

  	if (error) {
		if (error == HA_ERR_END_OF_FILE) {
			/* The table was empty, initialize to 1 */
			auto_inc = 1;

			error = 0;
		} else {
			/* Deadlock or a lock wait timeout */
  			auto_inc = -1;

  			goto func_exit;
  		}
  	} else {
		/* Initialize to max(col) + 1 */
    		auto_inc = (longlong) table->next_number_field->
                        	val_int_offset(table->rec_buff_length) + 1;
  	}

	dict_table_autoinc_initialize(prebuilt->table, auto_inc);

func_exit:
  	(void) extra(HA_EXTRA_NO_KEYREAD);

	index_end();

	*ret = auto_inc;

  	return(error);
}

/***********************************************************************
This function initializes the auto-inc counter if it has not been
initialized yet. This function does not change the value of the auto-inc
counter if it already has been initialized. Returns the value of the
auto-inc counter. */

longlong
ha_innobase::get_auto_increment()
/*=============================*/
                         /* out: auto-increment column value, -1 if error
                         (deadlock or lock wait timeout) */
{
  	longlong        nr;
  	int     	error;
	
	error = innobase_read_and_init_auto_inc(&nr);

	if (error) {

		return(-1);
	}

	return(nr);
}

#endif /* HAVE_INNOBASE_DB */

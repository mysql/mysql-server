/* Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB
   & Innobase Oy

   - This file is modified from ha_berkeley.cpp of the MySQL distribution -
   
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

/* This file defines the Innobase handler: the interface between MySQL and
Innobase */

/* TODO list for the Innobase handler:
  - How to check for deadlocks if Innobase tables are used alongside
    other MySQL table types? Should MySQL communicate the locking
    information also to Innobase of any object it locks, or should
    we use a timeout to detect a deadlock? Solution: there is no problem,
    because MySQL requires that all table-level locks are reserved
    at transaction startup (conservative locking). Deadlocks cannot
    occur because of these locks.
  - Innobase cmp function should call MySQL cmp for most datatypes?
    Except currently for binary strings and 32-bit integers?
    Solution: MySQL has conversion functions which currently convert
    any datatype to a binary string which can be compared as binary
    strings, except for character strings where we must identify
    lower case with upper case.
  - MySQL parser should know SELECT FOR UPDATE and SELECT WITH SHARED LOCKS
    for Innobase interface. We probably will make the non-locking
    consistent read the default in Innobase like in Oracle.
  - Does function next_same require matching of the whole last field,
    or it is enough that the common prefix of the last field matches?
    Answer: it is enough that the common prefix matches.
  - Is the 'ref' field in handle pre-allocated to be big enough? Primary key
    values can be very long! Answer: we can reallocate it to be long enough.
  - DELETE FROM TABLE must not drop the table like it does now, because
    consistent read will not work then! Answer: there is probably a flag
    in MySQL which we can use to prevent dropping of a table in this case.
-------Oct 24, 2000
  - Update trx pointers in 'prebuilt' when the transaction object of
    the handle changes. Answer: in 'external_lock' we always set the pointers
    to point to the trx of the current user. Note that if a user has
    disconnected, then another thd at exactly the same machine address may
    be created: just comparing the thd pointers does not really tell if it
    actually is the same user using the handle!
  - ANSI SQL specifies that if an SQL statement fails because of
    an error (like duplicate key, division by zero), the whole statement
    must be rolled back. Currently an error like this only rolls
    back a single insert of a row, or a single row update.
-------Oct 25, 2000
  - There are some autonomous threads within Innobase, like purge (= gc),
    ibuf merge, and recovery threads, which may have to open tables.
    Then they need type information for the table columns from MySQL.
    Could they call 'openfrm' in MySQL? Then they should be properly
    initialized pthreads, I presume.
-------Oct 30, 2000
  - Dropping of table in Innobase fails if there is a lock set on it:
    Innobase then gives an error number to MySQL but MySQL seems to drop
    the table from its own data dictionary anyway, causing incoherence
    between the two databases.
-------Oct 31, 2000
  - In sql_table.cpp in quick_rm_table, the path has to be 'unpacked'
    also after the latter sprintf to change / to \ in the path name.
  - Innobase currently includes the path to a table name: the path should
    actually be dropped off, because we may move a whole database to a new
    directory.
-------Nov 1, 2000
  - Ask from Monty what error codes 'record not found' and 'end of table'
    exactly mean and when read and fetch operations should return them.
-------Nov 2, 2000
  - Find out why in 'test-ATIS' in 'bench' directory, the client does
    not seem to receive rows sent by the server: maybe Innobase does not
    handle 'decimal' type correctly. Answer: Innobase did not return
    'record not found' and 'end of table' with right meanings.
-------Nov 3, 2000
  - 'pack' adds field length in front of string type fields: fields of
    different length are not correctly alphabetically ordered.
  - 'pack' removes leading (or was it, trailing) spaces from string type
    fields: maybe it would be better to store them as they are, if the
    field is not declared as varchar.
  - MySQL 'read_last_with_key' does not allow Innobase to return
    HA_ERR_KEY_NOT_FOUND, even when we try to read from an empty
    table.
-------Nov 4, 2000
  - MySQL should know when row id is added as uniquefier to a table for
    update and delete to work.
  - Innobase does not really support MySQL varchar type yet.
-------Nov 16, 2000
  - We use memcpy to store float and double types to Innobase: this
    makes database files not portable between big-endian and little-endian
    machines.
-------Nov 17, 2000
  - We added call of innobase_close_connection to THD::~THD in sql_class.cpp.
-------Nov 21, 2000
  - In mysql_delete, in sql_delete.cpp, we must be able to prevent
    MySQL from using generate_table to do a delete: consistent read does
    not allow this. Currently, MySQL uses generate_table in DELETE FROM ...
    if autocommit is on.
-------Nov 24, 2000
  - Make the SELECT in an update a locking read.
  - Add a deadlock error message to MySQL.
  - Add 'cannot drop locked table' error message to MySQL.
-------Nov 26, 2000
  - Find out why MySQL sometimes prints error message about read locks and
    write locks associated with a handle.
  - Find out why MySQL at shutdown prints error message 'Error on delete of
    ......pid (Errcode : 2).
-------Nov 30, 2000
  - MySQL calls innobase_end (shutdown) before it knows that all handles
    have been closed. It declares MySQL shutdown complete before Innobase
    shutdown is complete.
*/

#ifdef __GNUC__
#pragma implementation				// gcc: Class implementation
#endif

#include "mysql_priv.h"
#ifdef HAVE_INNOBASE_DB
#include <m_ctype.h>
#include <assert.h>
#include <hash.h>
#include <myisampack.h>


/* Include necessary Innobase headers */
extern "C" {
#include <univmysql.i>
#include <srv0start.h>
#include <srv0srv.h>
#include <trx0roll.h>
#include <trx0trx.h>
#include <row0ins.h>
#include <row0mysql.h>
#include <row0sel.h>
#include <row0upd.h>
#include <log0log.h>
#include <dict0crea.h>
#include <btr0cur.h>
#include <btr0btr.h>
}
#include "ha_innobase.h"


#define HA_INNOBASE_ROWS_IN_TABLE 10000 /* to get optimization right */
#define HA_INNOBASE_RANGE_COUNT	  100

const char* 	ha_innobase_ext		= ".ib";

bool 		innobase_skip 		= 0;
uint 		innobase_init_flags 	= 0;
ulong 		innobase_cache_size 	= 0;

long innobase_mirrored_log_groups, innobase_mirrored_log_groups,
     innobase_log_file_size, innobase_log_buffer_size,
     innobase_buffer_pool_size, innobase_additional_mem_pool_size,
     innobase_file_io_threads;

char *innobase_data_home_dir, *innobase_data_file_path;
char *innobase_log_group_home_dir, *innobase_log_arch_dir;
bool innobase_flush_log_at_trx_commit,innobase_log_archive;

/* innobase_data_file_path=ibdata:15,idata2:1,... */

/* The following counter is used to convey information to Innobase
about server activity: in selects it is not sensible to call
srv_active_wake_master_thread after each fetch or search, we only do
it every INNOBASE_WAKE_INTERVAL'th step. */

ulong	innobase_select_counter	= 0;
#define INNOBASE_WAKE_INTERVAL	32


char*	innobase_home 	= NULL;

pthread_mutex_t innb_mutex;

static HASH 	innb_open_tables;

static byte* innobase_get_key(INNOBASE_SHARE *share,uint *length,
			      my_bool not_used __attribute__((unused)));
static INNOBASE_SHARE *get_share(const char *table_name);
static void free_share(INNOBASE_SHARE *share);
static void innobase_print_error(const char* db_errpfx, char* buffer);

/* General functions */

/************************************************************************
Converts an Innobase error code to a MySQL error code. */
static
int
convert_error_code_to_mysql(
/*========================*/
			/* out: MySQL error code */
	int	error)	/* in: Innobase error code */
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

    		return(HA_ERR_UNSUPPORTED);

 	} else if (error == (int) DB_OUT_OF_FILE_SPACE) {

    		return(HA_ERR_RECORD_FILE_FULL);

 	} else if (error == (int) DB_TABLE_IS_BEING_USED) {

    		return(HA_ERR_WRONG_COMMAND);

 	} else if (error == (int) DB_TABLE_NOT_FOUND) {

    		return(HA_ERR_KEY_NOT_FOUND);

  	} else if (error == (int) DB_TOO_BIG_RECORD) {

    		return(HA_ERR_TO_BIG_ROW);

    	} else {
    		assert(0);

    		return(0);
    	}
}

/*************************************************************************
Gets the Innobase transaction handle for a MySQL handler object, creates
an Innobase transaction struct if the corresponding MySQL thread struct still
lacks one. */
inline
trx_t*
check_trx_exists(
/*=============*/
			/* out: Innobase transaction handle */
	THD*	thd)	/* in: user thread handle */
{
	trx_t*	trx;

	assert(thd != NULL);

	trx = (trx_t*) thd->transaction.innobase_trx_handle;

	if (trx == NULL) {
		trx = trx_allocate_for_mysql();
		
		thd->transaction.innobase_trx_handle = trx;
	}

	return(trx);
}

/*************************************************************************
Updates the user_thd field in a handle and also allocates a new Innobase
transaction handle if needed, and updates the transaction fields in the
prebuilt struct. */

int
ha_innobase::update_thd(
/*====================*/
			/* out: 0 or error code */
	THD*	thd)	/* in: thd to use the handle */
{
	trx_t*	trx;

	trx = check_trx_exists(thd);

	if (innobase_prebuilt != NULL) {

		row_update_prebuilt_trx((row_prebuilt_t*) innobase_prebuilt,
									trx);
	}

	user_thd = thd;
	
	return(0);
}

/*************************************************************************
Opens an Innobase database. */

bool
innobase_init(void)
/*===============*/
			/* out: TRUE if error */ 
{
	int	err;
	
  	DBUG_ENTER("innobase_init");

  	if (!innobase_home) {
    		innobase_home = mysql_real_data_home;

    		printf("Innobase home is %s\n", innobase_home);
    	}

	err = innobase_start_or_create_for_mysql(innobase_home);

	if (err != DB_SUCCESS) {

		return(1);
	}
	(void) hash_init(&innobase_open_tables,32,0,0,
			 (hash_get_key) innobase_get_key,0,0);
	pthread_mutex_init(&innobase_mutex,NULL);
  	return(0);
}

/***********************************************************************
Closes an Innobase database. */

bool
innobase_end(void)
/*==============*/
				/* out: TRUE if error */
{
	int	err;

	DBUG_ENTER("innobase_end");

	err = innobase_shutdown_for_mysql();

	if (err != DB_SUCCESS) {

		return(1);
	}
	
  	return(0);
}

/********************************************************************
Flushes Innobase logs to disk and makes a checkpoint. Really, a commit
flushes logs, and the name of this function should be innobase_checkpoint. */

bool
innobase_flush_logs(void)
/*=====================*/
				/* out: TRUE if error */
{
  	bool 	result = 0;

  	DBUG_ENTER("innobase_flush_logs");

	log_make_checkpoint_at(ut_dulint_max, TRUE);
	
  	DBUG_RETURN(result);
}

/*********************************************************************
Commits a transaction in an Innobase database. */

int
innobase_commit(
/*============*/
			/* out: 0 or error number */
	THD*	thd)	/* in: MySQL thread handle of the user for whom
			the transaction should be committed */
{
	int	error	= 0;

  	DBUG_ENTER("innobase_commit");
  	DBUG_PRINT("trans", ("ending transaction"));

	check_trx_exists(thd);

	trx_commit_for_mysql((trx_t*)(thd->transaction.innobase_trx_handle));

#ifndef DBUG_OFF
	if (error) {
    		DBUG_PRINT("error", ("error: %d", error));
    	}
#endif
	/* Tell Innobase server that there might be work for
	utility threads: */

	srv_active_wake_master_thread();

	DBUG_RETURN(error);
}

/*********************************************************************
Rolls back a transaction in an Innobase database. */

int
innobase_rollback(
/*==============*/
			/* out: 0 or error number */
	THD*	thd)	/* in: handle to the MySQL thread of the user
			whose transaction should be rolled back */
{
	int	error = 0;

	DBUG_ENTER("innobase_rollback");
	DBUG_PRINT("trans", ("aborting transaction"));

	check_trx_exists(thd);

	error = trx_rollback_for_mysql((trx_t*)
				(thd->transaction.innobase_trx_handle));

	DBUG_RETURN(convert_error_code_to_mysql(error));
}

/*********************************************************************
Frees a possible Innobase trx object associated with the current
THD. */

int
innobase_close_connection(
/*======================*/
			/* out: 0 or error number */
	THD*	thd)	/* in: handle to the MySQL thread of the user
			whose transaction should be rolled back */
{
	if (NULL != thd->transaction.innobase_trx_handle) {
	
		trx_free_for_mysql((trx_t*)
				(thd->transaction.innobase_trx_handle));
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
** Innobase database tables
*****************************************************************************/

/********************************************************************
??????????????? */

const char**
ha_innobase::bas_ext() const
/*========================*/
				/* out: ?????????? */
{
	static const char* ext[] = {ha_innobase_ext, NullS};
	return(ext);
}

/*********************************************************************
Creates and opens a handle to a table which already exists in an Innnobase
database. */

int
ha_innobase::open(
/*==============*/
					/* out: 1 if error, 0 if success */
	const char*	name,		/* in: table name */
	int 		mode,		/* in: not used */
	int 		test_if_locked)	/* in: not used */
{
  	int 	error	= 0;
  	uint	buff_len;

	DBUG_ENTER("ha_innobase::open");

	UT_NOT_USED(mode);
	UT_NOT_USED(test_if_locked);

	user_thd = NULL;

	if (!(share=get_share(name)))
	  DBUG_RETURN(1);

	/* Create buffers for packing the fields of a record;
	Why table->reclength did not work here?
	obviously, because char fields when packed actually became
	1 byte longer, when we also stored the string length as
	the first byte. */

	buff_len = table->reclength + table->max_key_length
						+ MAX_REF_PARTS * 2;
	
	if (!(byte*) my_multi_malloc(MYF(MY_WME),
				     &rec_buff, buff_len,
				     &upd_buff, buff_len,
				     &key_val_buff, buff_len,
				     NullS))
	{
	  free_share(share);		
	  DBUG_RETURN(1);
  	}

  	/* MySQL allocates the buffer for ref */
  	
  	ref_length = buff_len;

	/* Get pointer to a table object in Innobase dictionary cache */

 	if (NULL == (innobase_table_handle
				= dict_table_get((char*)name, NULL))) {
		
	        free_share(share);
    		my_free((char*) rec_buff, MYF(0));
    		my_errno = ENOENT;
    		DBUG_RETURN(1);
  	}

  	info(HA_STATUS_NO_LOCK | HA_STATUS_VARIABLE | HA_STATUS_CONST);

  	/* Allocate a persistent cursor struct */
  	
	innobase_prebuilt = row_create_prebuilt((dict_table_t*)
							innobase_table_handle);
  	primary_key = 0;

  	if (!row_table_got_default_clust_index((dict_table_t*)
						innobase_table_handle)) {
		/* If we automatically created the clustered index,
		then MySQL does not know about it and it must not be aware
		of the index used on scan, to avoid checking if we update
		the column of the index. The column is the row id in
		the automatical case, and it will not be updated. */
		
		key_used_on_scan = primary_key;
	} else {
		assert(key_used_on_scan == MAX_KEY);
	}
	  									
 	/* Init table lock structure */
	thr_lock_data_init(&share->lock,&lock,(void*) 0);
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
Closes a handle to an Innobase table. */

int
ha_innobase::close(void)
/*====================*/
				/* out: error number */
{
  	DBUG_ENTER("ha_innobase::close");

	row_prebuilt_free((row_prebuilt_t*) innobase_prebuilt);

    	my_free((char*) rec_buff, MYF(0));
        free_share(share);

	/* Tell Innobase server that there might be work for
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
This function is used to compare two data fields for which the data type
is such that we must use MySQL code to compare them. */

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
	float	f_1;
	float	f_2;
	double	d_1;
	double	d_2;
	int	swap_flag		= 1;
	enum_field_types	mysql_tp;

	assert(a_length != UNIV_SQL_NULL);	
	assert(b_length != UNIV_SQL_NULL);

	mysql_tp = (enum_field_types) mysql_type;

	switch (mysql_tp) {

	case FIELD_TYPE_STRING:
	case FIELD_TYPE_VAR_STRING:
  		return(my_sortncmp((const char*) a, a_length,
						(const char*) b, b_length));
	case FIELD_TYPE_FLOAT:
		memcpy(&f_1, a, sizeof(float));
		memcpy(&f_2, b, sizeof(float));

      		if (f_1 > f_2) {
      			return(1);
      		} else if (f_2 > f_1) {
      			return(-1);
      		}

      		return(0);

    	case FIELD_TYPE_DOUBLE:
		memcpy(&d_1, a, sizeof(double));
		memcpy(&d_2, b, sizeof(double));
		
      		if (d_1 > d_2) {
      			return(1);
      		} else if (d_2 > d_1) {
      			return(-1);
      		}

      		return(0);
	
    	case FIELD_TYPE_DECIMAL:
		/* Remove preceding spaces */
		for (; a_length && *a == ' '; a++, a_length--);
		for (; b_length && *b == ' '; b++, b_length--);

      		if (*a == '-') {
			if (*b != '-') {
	  			return(-1);
	  		}

			a++; b++;
			a_length--;
			b_length--;

			swap_flag = -1;
			
      		} else if (*b == '-') {

			return(1);
		}
		
      		while (a_length > 0 && (*a == '+' || *a == '0')) {
			a++; a_length--;
      		}

      		while (b_length > 0 && (*b == '+' || *b == '0')) {
			b++; b_length--;
      		}
      		
      		if (a_length != b_length) {
			if (a_length < b_length) {
				return(-swap_flag);
			}

			return(swap_flag);
		}

		while (a_length > 0 && *a == *b) {

			a++; b++; a_length--;
		}
			
		if (a_length == 0) {

			return(0);
		}

		if (*a > *b) {
			return(swap_flag);
		}

		return(-swap_flag);
	default:
		assert(0);
	}

	return(0);
}
}

/******************************************************************
Decides of MySQL types whether Innobase can internally compare them
using its own comparison functions, or whether Innobase must call MySQL
cmp function to compare them. */
inline
ulint
innobase_cmp_type(
/*==============*/
			/* out: DATA_BINARY, DATA_VARCHAR, or DATA_MYSQL */
	Field*	field)	/* in: MySQL field */
{
	/* The following asserts check that MySQL type code fits in
	one byte: this is used in ibuf */

	assert((ulint)FIELD_TYPE_STRING < 256);
	assert((ulint)FIELD_TYPE_VAR_STRING < 256);
	assert((ulint)FIELD_TYPE_DOUBLE < 256);
	assert((ulint)FIELD_TYPE_FLOAT < 256);
	assert((ulint)FIELD_TYPE_DECIMAL < 256);

	switch (field->type()) {
		case FIELD_TYPE_VAR_STRING:
		case FIELD_TYPE_STRING: if (field->flags & BINARY_FLAG) {

						return(DATA_BINARY);
					} else if (strcmp(
						   default_charset_info->name,
							"latin1") == 0) {
						return(DATA_VARCHAR);
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
					return(DATA_BINARY);
		case FIELD_TYPE_FLOAT:
		case FIELD_TYPE_DOUBLE:
		case FIELD_TYPE_DECIMAL:
					return(DATA_MYSQL);
		default:
					assert(0);
	}

	return(0);
}
					
/******************************************************************
Packs a non-SQL-NULL field data for storage in Innobase. Usually this
'packing' is just memcpy, but for an integer it is also converted
to a big-endian, sign bit negated form. */
inline
byte*
pack_for_ib(
/*========*/
			/* out: pointer to the end of the packed data
			in the buffer */
	byte*	buf,	/* in/out: pointer to buffer where to pack */
	Field*	field,	/* in: MySQL field object */
	byte*	data)	/* in: data to pack */
{
	uint	len;
	uint	i;

	switch (field->type()) {
		case FIELD_TYPE_LONG:
		case FIELD_TYPE_TINY:
		case FIELD_TYPE_SHORT:
		case FIELD_TYPE_INT24:
		case FIELD_TYPE_LONGLONG:
				len = field->pack_length(); break;
		case FIELD_TYPE_VAR_STRING:
				len = field->field_length;
		
				/* Scan away trailing spaces */
				for (i = 0; i < len; i++) {
					if (data[len - i - 1] != ' ') {
						break;
					}
				}

				memcpy(buf, data, len - i);
				return(buf + len - i);
		case FIELD_TYPE_STRING:
				/* We store character strings with no
				conversion */
				len = field->field_length;
				memcpy(buf, data, len);
				return(buf + len);
		case FIELD_TYPE_DOUBLE:
				memcpy(buf, data, sizeof(double));

				return(buf + sizeof(double));
		case FIELD_TYPE_FLOAT:
				memcpy(buf, data, sizeof(float));

				return(buf + sizeof(float));
		default:
				return((byte*) field->pack((char*) buf,
							(const char*) data));
	}

	/* Store integer data in Innobase in a big-endian format, sign
	bit negated */
	
	for (i = 0; i < len; i++) {
		buf[len - 1 - i] = data[i];
	}

	buf[0] = buf[0] ^ 128;

	return(buf + len);
}

/******************************************************************
Packs a non-SQL-NULL field data in a key value for storage in Innobase.
TODO: find out what is the difference between keypack and pack. */
inline
byte*
keypack_for_ib(
/*===========*/
			/* out: pointer to the end of the packed data
			in the buffer */
	byte*	buf,	/* in/out: buffer where to pack */
	Field*	field,	/* in: field object */
	byte*	data,	/* in: data to pack */
	uint	len)	/* in: length of the data to pack */
{
	switch (field->type()) {
		case FIELD_TYPE_LONG:
		case FIELD_TYPE_TINY:
		case FIELD_TYPE_SHORT:
		case FIELD_TYPE_INT24:
		case FIELD_TYPE_LONGLONG:
		case FIELD_TYPE_STRING:
		case FIELD_TYPE_VAR_STRING:
		case FIELD_TYPE_DOUBLE:
		case FIELD_TYPE_FLOAT:
			return(pack_for_ib(buf, field, data));
		default:
			return((byte*) field->keypack((char*) buf,
						(const char*) data, len));
	}
}

/******************************************************************
Unpacks a non-SQL-NULL field data stored in Innobase. */
inline
void
unpack_for_ib(
/*==========*/
	byte*	dest,	/* in/out: buffer where to unpack */
	Field*	field,	/* in: field object */
	byte*	ptr,	/* in: data to unpack */
	uint	data_len)/* in: length of the data */
{
	uint	len;
	uint	i;

	switch (field->type()) {
		case FIELD_TYPE_LONG:
		case FIELD_TYPE_TINY:
		case FIELD_TYPE_SHORT:
		case FIELD_TYPE_INT24:
		case FIELD_TYPE_LONGLONG:
				len = field->pack_length(); break;
		case FIELD_TYPE_VAR_STRING:
				len = field->field_length;
				/* Pad trailing characters with spaces */
				for (i = data_len; i < len; i++) {
					dest[i] = ' ';
				}
				memcpy(dest, ptr, data_len);

				return;
		case FIELD_TYPE_STRING:
				/* We store character strings with no
				conversion */
				len = field->field_length;
				memcpy(dest, ptr, len);

				return;
		case FIELD_TYPE_DOUBLE:
				memcpy(dest, ptr, sizeof(double));

				return;
		case FIELD_TYPE_FLOAT:
				memcpy(dest, ptr, sizeof(float));

				return;
		default:
				field->unpack((char*) dest, (const char*) ptr);

				return;
	}

	/* Get integer data from Innobase in a little-endian format, sign
	bit restored to normal */

	for (i = 0; i < len; i++) {
		dest[len - 1 - i] = ptr[i];
	}

	dest[len - 1] = dest[len - 1] ^ 128;
}

/***********************************************************************
Stores a key value for a row to a buffer. */

uint
ha_innobase::store_key_val_for_row(
/*===============================*/
				/* out: key value length as stored in buff */
	uint 		keynr,	/* in: key number */
	char*		buff,	/* in/out: buffer for the key value (in MySQL
				format) */
	const byte*	record)	/* in: row in MySQL format */
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
				*buff++ =0;
				continue;
      			}

      			*buff++ = 1;
    		}
    	
		memcpy(buff, record + key_part->offset, key_part->length);
		buff += key_part->length;
  	}

	DBUG_RETURN(buff - buff_start);
}

/******************************************************************
Convert a row in MySQL format to a row in Innobase format. Uses rec_buff
of the handle. */

void
ha_innobase::convert_row_to_innobase(
/*=================================*/
	dtuple_t*	row,	/* in/out: row in Innobase format */
	char*		record)	/* in: row in MySQL format */
{
	Field*		field;
	dfield_t*	dfield;
	uint		n_fields;
	ulint		len;
	byte*		old_ptr;
	byte*		ptr;
	uint		i;

	n_fields = table->fields;

	ptr = rec_buff;
	
	for (i = 0; i < n_fields; i++) {
		field = table->field[i];
		dfield = dtuple_get_nth_field_noninline(row, i);

		old_ptr = ptr;

		if (field->null_ptr && field_in_record_is_null(table,
							field, record)) {
			len = UNIV_SQL_NULL;
		} else {
			ptr = pack_for_ib(ptr, field, (byte*) record
						+ get_field_offset(table,
								field));
			len = ptr - old_ptr;
		}

		dfield_set_data_noninline(dfield, old_ptr, len);
	}
}

/******************************************************************
Convert a row in Innobase format to a row in MySQL format. */

void
ha_innobase::convert_row_to_mysql(
/*==============================*/
	char*		record,	/* in/out: row in MySQL format */
	dtuple_t*	row)	/* in: row in Innobase format */
{
	Field*		field;
	dfield_t*	dfield;
	byte*		ptr;
	uint		n_fields;
	uint		len;
	uint		i;
	
	reset_null_bits(table, record);

	n_fields = table->fields;
	
	for (i = 0; i < n_fields; i++) {
		field = table->field[i];
		dfield = dtuple_get_nth_field_noninline(row, i);

		len = dfield_get_len_noninline(dfield);
		
		if (len != UNIV_SQL_NULL) {

			ptr = (byte*) dfield_get_data_noninline(dfield);

			unpack_for_ib((byte*)
				      record + get_field_offset(table, field),
							field, ptr, len);	
		} else {
			set_field_in_record_to_null(table, field, record);
		}
	} 
}

/********************************************************************
Converts a key value stored in MySQL format to an Innobase dtuple.
The last field of the key value may be just a prefix of a fixed length
field: hence the parameter key_len. */

dtuple_t*
ha_innobase::convert_key_to_innobase(
/*=================================*/
	dtuple_t*	tuple,	/* in/out: an Innobase dtuple which
				must contain enough fields to be
				able to store the key value */
	byte*		buf,	/* in/out: buffer where to store converted
				field data */
	dict_index_t*	index,	/* in: Innobase index handle */
	KEY*		key,	/* in: MySQL key definition */
	byte*		key_ptr,/* in: MySQL key value */
	int		key_len)/* in: MySQL key value length */
{
	KEY_PART_INFO*	key_part = key->key_part;
  	KEY_PART_INFO*	end	 = key_part + key->key_parts;
	uint		offset;
	dfield_t*	dfield;
	byte*		old_buf;
	ulint		n_fields = 0;
  	
 	DBUG_ENTER("convert_key_to_innobase");

	/* Permit us to access any field in the tuple (ULINT_MAX): */
						
	dtuple_set_n_fields(tuple, ULINT_MAX);

	dfield = dtuple_get_nth_field_noninline(tuple, 0);
 	
  	for (; key_part != end && key_len > 0; key_part++) {
		n_fields++;
  		
		offset = 0;
    		
    		if (key_part->null_bit) {
    			offset = 1;
			if (*key_ptr != 0) {
      				dfield_set_data_noninline(dfield, NULL,
							UNIV_SQL_NULL);
      				goto next_part;
      			}

      			/* Is there a bug in ha_berkeley.cpp here? There
      			key_ptr is advanced one byte here. ????????????  */
      		}

		old_buf = buf;
      		buf = keypack_for_ib(buf, key_part->field, key_ptr + offset,
							key_part->length);
		
		dfield_set_data_noninline(dfield, old_buf,
						(ulint) (buf - old_buf));
	next_part:
    		key_ptr += key_part->store_length;
		key_len -= key_part->store_length;

		if (key_len < 0) {
			/* The last field in key was not a complete
			field but a prefix of it */

			assert(dfield_get_len_noninline(dfield)
							!= UNIV_SQL_NULL);
			assert((int)(buf - old_buf) + key_len >= 0);
			
			dfield_set_data_noninline(dfield, old_buf,
					(ulint) ((buf - old_buf) + key_len));
		}

		dfield++;
  	}

 	dict_index_copy_types(tuple, index, n_fields);

 	/* We set the length of tuple to n_fields: we assume that
	the memory area allocated for it is big enough (usually
	bigger than n_fields). */
 	
 	dtuple_set_n_fields(tuple, n_fields);

	DBUG_RETURN(tuple);
}

/************************************************************************
Stores a row in an Innobase database, to the table specified in this
handle. */

int
ha_innobase::write_row(
/*===================*/
			/* out: error code */
	byte* 	record)	/* in: a row in MySQL format */
{
	trx_t*		trx;
	dtuple_t*	row;
  	int 		error;
  	
  	DBUG_ENTER("write_row");

  	statistic_increment(ha_write_count, &LOCK_status);

  	if (table->time_stamp) {
    		update_timestamp(record + table->time_stamp - 1);
    	}
  	if (table->next_number_field && record == table->record[0]) {
    		update_auto_increment();
    	}
  	
	assert(user_thd->transaction.innobase_trx_handle);
	trx = check_trx_exists(user_thd);

	/* Convert the MySQL row into an Innobase dtuple format */

	row = row_get_prebuilt_insert_row(
				(row_prebuilt_t*) innobase_prebuilt,
				(dict_table_t*) innobase_table_handle, trx);

	convert_row_to_innobase(row, (char*) record);

	error = row_insert_for_mysql((row_prebuilt_t*)innobase_prebuilt, trx);

	error = convert_error_code_to_mysql(error);
	
	/* Tell Innobase server that there might be work for
	utility threads: */

	srv_active_wake_master_thread();

  	DBUG_RETURN(error);
}

/**************************************************************************
Checks which fields have changed in a row and stores information
of them to an update vector. */

int
ha_innobase::calc_row_difference(
/*=============================*/
				/* out: error number or 0 */
	upd_t*	uvect,		/* in/out: update vector */
	byte* 	old_row,	/* in: old row in MySQL format */
	byte* 	new_row)	/* in: new row in MySQL format */
{
	row_prebuilt_t*	prebuilt = (row_prebuilt_t*) innobase_prebuilt;
	Field*		field;
	uint		n_fields;
	ulint		o_len;
	ulint		n_len;
	byte*		o_ptr;
	byte*		n_ptr;
	byte*		old_ptr;
	byte*		ptr;
	uint		i;
	upd_field_t*	ufield;
	ulint		n_changed = 0;

	n_fields = table->fields;

	/* We use upd_buff to pack changed fields */
	ptr = upd_buff;
	
	for (i = 0; i < n_fields; i++) {
		field = table->field[i];

		o_ptr = old_row + get_field_offset(table, field);
		n_ptr = new_row + get_field_offset(table, field);
		o_len = field->pack_length();
		n_len = field->pack_length();

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

			if (n_len != UNIV_SQL_NULL) {
				old_ptr = ptr;
    				ptr = pack_for_ib(ptr, field, n_ptr);
				n_len = ptr - old_ptr;
			}

			ufield = uvect->fields + n_changed;

			dfield_set_data_noninline(&(ufield->new_val), old_ptr,
									n_len);
			ufield->exp = NULL;
			ufield->field_no = dict_table_get_nth_col_pos(
						(dict_table_t*)
						innobase_table_handle, i);
			n_changed++;
		}
	}

	uvect->n_fields = n_changed;
	uvect->info_bits = 0;

	return(0);
}

/**************************************************************************
Updates a row given as a parameter to a new value. Note that we are given
whole rows, not just the fields which are updated: this incurs some
overhead for CPU when we check which fields are actually updated.
TODO: currently Innobase does not prevent the 'Halloween problem':
in a searched update a single row can get updated several times
if its index columns are updated! */
 
int
ha_innobase::update_row(
/*====================*/
					/* out: error number or 0 */
	const byte* 	old_row,	/* in: old row in MySQL format */
	byte* 		new_row)	/* in: new row in MySQL format */
{
	row_prebuilt_t*	prebuilt = (row_prebuilt_t*) innobase_prebuilt;
	trx_t*		trx;
	upd_t*		uvect;
	int		error = 0;

	DBUG_ENTER("update_row");
	
	assert(user_thd->transaction.innobase_trx_handle);
	trx = check_trx_exists(user_thd);

	uvect = row_get_prebuilt_update_vector(
				prebuilt,
				(dict_table_t*) innobase_table_handle, trx);

	/* Build old row in the Innobase format (uses rec_buff of the
	handle) */

	convert_row_to_innobase(prebuilt->row_tuple, (char*) old_row);	
				
	/* Build an update vector from the modified fields in the rows
	(uses upd_buff of the handle) */

	calc_row_difference(uvect, (byte*) old_row, new_row);

	/* This is not a delete */
	prebuilt->upd_node->is_delete = FALSE;

	error = row_update_for_mysql((row_prebuilt_t*) innobase_prebuilt,
				(dict_table_t*) innobase_table_handle, trx);

	error = convert_error_code_to_mysql(error);

	/* Tell Innobase server that there might be work for
	utility threads: */

	srv_active_wake_master_thread();

	DBUG_RETURN(error);
}

/**************************************************************************
Deletes a row given as the parameter. */

int
ha_innobase::delete_row(
/*====================*/
				/* out: error number or 0 */
	const byte* record)	/* in: a row in MySQL format */
{
	row_prebuilt_t*	prebuilt = (row_prebuilt_t*) innobase_prebuilt;
	trx_t*		trx;
	upd_t*		uvect;
	int		error = 0;

	DBUG_ENTER("update_row");

	assert(user_thd->transaction.innobase_trx_handle);
	trx = check_trx_exists(user_thd);

	uvect = row_get_prebuilt_update_vector(
				prebuilt,
				(dict_table_t*) innobase_table_handle, trx);

	/* Build old row in the Innobase format (uses rec_buff of the
	handle) */

	convert_row_to_innobase(prebuilt->row_tuple, (char*) record);	
		
	/* This is a delete */
	
	prebuilt->upd_node->is_delete = TRUE;

	error = row_update_for_mysql((row_prebuilt_t*) innobase_prebuilt,
				(dict_table_t*) innobase_table_handle, trx);

	error = convert_error_code_to_mysql(error);

	/* Tell the Innobase server that there might be work for
	utility threads: */

	srv_active_wake_master_thread();

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

	change_active_index(keynr);

  	DBUG_RETURN(error);
}

/**********************************************************************
?????????????????????????????????? */

int
ha_innobase::index_end(void)
/*========================*/
{
	int 	error	= 0;
  	DBUG_ENTER("index_end");

	/* Tell Innobase server that there might be work for utility
	threads: */

	srv_active_wake_master_thread();

  	DBUG_RETURN(error);
}

/*************************************************************************
Converts a search mode flag understood by MySQL to a flag understood
by Innobase. */
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
		
/**************************************************************************
Positions an index cursor to the index specified in the handle. Fetches the
row if any. */

int
ha_innobase::index_read(
/*====================*/
					/* out: 0, HA_ERR_KEY_NOT_FOUND,
					or error number */
	byte*		buf,		/* in/out: buffer for the returned
					row */
	const byte* 	key_ptr,	/* in: key value; if this is NULL
					we position the cursor at the
					start or end of index */
	uint		key_len,	/* in: key value length */
	enum ha_rkey_function find_flag)/* in: search flags from my_base.h */
{
	row_prebuilt_t* prebuilt	= (row_prebuilt_t*) innobase_prebuilt;
	ulint		mode;
	dict_index_t*	index;
	btr_pcur_t*	pcur;
	KEY*		key;
	ulint		match_mode 	= 0;
	int 		error;
	ulint		ret;
	trx_t*		trx;
	mtr_t		mtr;

  	DBUG_ENTER("index_read");
  	statistic_increment(ha_read_key_count, &LOCK_status);

	/* TODO: currently we assume all reads perform consistent read! */
	/* prebuilt->consistent_read = TRUE; */

	assert(user_thd->transaction.innobase_trx_handle);
	trx = check_trx_exists(user_thd);
	
  	pcur = prebuilt->pcur;

	key = table->key_info + active_index;

	index = prebuilt->index;

	if (key_ptr) {
		convert_key_to_innobase(prebuilt->search_tuple, key_val_buff,
				index, key, (unsigned char*) key_ptr,
				(int) key_len);
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

	/* Start an Innobase mini-transaction, which carries the
	latch information of the read operation */

	mtr_start_noninline(&mtr);

	ret = row_search_for_mysql(prebuilt->row_tuple,
				mode, prebuilt, match_mode,
				trx, &mtr, 0); 

	if (ret == DB_SUCCESS) {
		convert_row_to_mysql((char*) buf, prebuilt->row_tuple);
		error = 0;
		table->status = 0;

	} else if (ret == DB_RECORD_NOT_FOUND) {
		error = HA_ERR_KEY_NOT_FOUND;
		table->status = STATUS_NOT_FOUND;

	} else if (ret == DB_END_OF_INDEX) {
		error = HA_ERR_KEY_NOT_FOUND;
		table->status = STATUS_NOT_FOUND;
	} else {
		error = convert_error_code_to_mysql(ret);
		table->status = STATUS_NOT_FOUND;
	}

	mtr_commit(&mtr);
	
	innobase_select_counter++;

	if (innobase_select_counter % INNOBASE_WAKE_INTERVAL == 0) {
		srv_active_wake_master_thread();
	}

	DBUG_RETURN(error);
}

/************************************************************************
Changes the active index of a handle. */

int
ha_innobase::change_active_index(
/*=============================*/
				/* out: 0 or error code */
	uint 	keynr)		/* in: use this index */
{
	row_prebuilt_t* prebuilt	= (row_prebuilt_t*) innobase_prebuilt;
	KEY*		key;

  	statistic_increment(ha_read_key_count, &LOCK_status);

  	DBUG_ENTER("index_read_idx");
  	
	active_index = keynr;

	if (table->keys > 0) {
		key = table->key_info + active_index;
	
		prebuilt->index = dict_table_get_index_noninline(
				(dict_table_t*) innobase_table_handle,
							key->name);
	} else {
		assert(keynr == 0);
		prebuilt->index = dict_table_get_first_index_noninline(
					(dict_table_t*) innobase_table_handle);
	}

	assert(prebuilt->index);
	
	return(0);
}

/**************************************************************************
Positions an index cursor to the index specified in keynr. Fetches the
row if any. */
/* ??? This is only used to read whole keys ??? */

int
ha_innobase::index_read_idx(
/*========================*/
					/* out: error number or 0 */
	byte*		buf,		/* in/out: buffer for the returned
					row */
	uint 		keynr,		/* in: use this index */
	const byte* 	key,		/* in: key value; if this is NULL
					we position the cursor at the
					start or end of index */
	uint		key_len,	/* in: key value length */
	enum ha_rkey_function find_flag)/* in: search flags from my_base.h */
{
	change_active_index(keynr);

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
	byte* 	buf,		/* in/out: buffer for next row in MySQL
				format */
	uint 	direction,	/* in: ROW_SEL_NEXT or ROW_SEL_PREV */
	uint	match_mode)	/* in: 0, ROW_SEL_EXACT, or
				ROW_SEL_EXACT_PREFIX */
{
	row_prebuilt_t*	prebuilt = (row_prebuilt_t*) innobase_prebuilt;
	ulint		ret;
	trx_t*		trx;
	int		error	= 0;
	mtr_t		mtr;
	
	DBUG_ENTER("general_fetch");
  	statistic_increment(ha_read_next_count, &LOCK_status);

	trx = check_trx_exists(user_thd);
	
	mtr_start_noninline(&mtr);

	ret = row_search_for_mysql(prebuilt->row_tuple, 0, prebuilt,
					match_mode, trx, &mtr, direction);
	if (ret == DB_SUCCESS) {
		convert_row_to_mysql((char*) buf, prebuilt->row_tuple);
		error = 0;
		table->status = 0;

	} else if (ret == DB_RECORD_NOT_FOUND) {
		error = HA_ERR_END_OF_FILE;
		table->status = STATUS_NOT_FOUND;

	} else if (ret == DB_END_OF_INDEX) {
		error = HA_ERR_END_OF_FILE;
		table->status = STATUS_NOT_FOUND;
	} else {
		error = convert_error_code_to_mysql(ret);
		table->status = STATUS_NOT_FOUND;
	}

	mtr_commit(&mtr);	
	
	innobase_select_counter++;

	if (innobase_select_counter % INNOBASE_WAKE_INTERVAL == 0) {
		srv_active_wake_master_thread();
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
	byte* 	buf)		/* in/out: buffer for next row in MySQL
				format */
{
	return(general_fetch(buf, ROW_SEL_NEXT, 0));
}

/***********************************************************************
Reads the next row matching to the key value given as the parameter. */

int
ha_innobase::index_next_same(
/*=========================*/
				/* out: 0, HA_ERR_END_OF_FILE, or error
				number */
	byte* 		buf,	/* in/out: buffer for the row */
	const byte*	key,	/* in: key value */
	uint 		keylen)	/* in: key value length */
{
	assert(last_match_mode != 0);

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
	byte* 	buf)		/* in/out: buffer for previous row in MySQL
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
			/* out: 0, HA_ERR_KEY_NOT_FOUND, or error code */
	byte*	buf)	/* in/out: buffer for the row */
{
	int	error;

  	DBUG_ENTER("index_first");
  	statistic_increment(ha_read_first_count, &LOCK_status);

  	error = index_read(buf, NULL, 0, HA_READ_AFTER_KEY);

  	DBUG_RETURN(error);
}

/************************************************************************
Positions a cursor on the last record in an index and reads the
corresponding row to buf. */

int
ha_innobase::index_last(
/*====================*/
			/* out: 0, HA_ERR_END_OF_FILE, or error code */
	byte*	buf)	/* in/out: buffer for the row */
{
	int	error;

  	DBUG_ENTER("index_first");
  	statistic_increment(ha_read_first_count, &LOCK_status);

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
	row_prebuilt_t* prebuilt = (row_prebuilt_t*) innobase_prebuilt;

	change_active_index(primary_key);

  	prebuilt->start_of_scan = TRUE;

 	return(0);
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
	byte* buf)	/* in/out: returns the row in this buffer,
			in MySQL format */
{
	row_prebuilt_t* prebuilt = (row_prebuilt_t*) innobase_prebuilt;
	int		error;

  	DBUG_ENTER("rnd_next");
  	statistic_increment(ha_read_rnd_next_count, &LOCK_status);

  	if (prebuilt->start_of_scan) {
		error = index_first(buf);
		if (error == HA_ERR_KEY_NOT_FOUND) {
			error = HA_ERR_END_OF_FILE;
		}
		prebuilt->start_of_scan = FALSE;
	} else {
		error = index_next(buf);
	}
  	
  	DBUG_RETURN(error);
}

/**************************************************************************
Fetches a row from the table based on a reference. TODO: currently we use
'ref_stored_len' of the handle as the key length. This may change. */
 
int
ha_innobase::rnd_pos(
/*=================*/
			/* out: 0, HA_ERR_KEY_NOT_FOUND, or error code */
	byte* 	buf,	/* in/out: buffer for the row */
	byte*	pos)	/* in: primary key value in MySQL format */
{
	int error;

	DBUG_ENTER("rnd_pos");
	statistic_increment(ha_read_rnd_count, &LOCK_status);

	assert(table->keys > 0);

	/* The following assert states that the cursor is positioned
	to the primary index in this function: this cannot be used to
	position the cursor to a secondary index! */
	
	assert(active_index == primary_key);

	error = index_read(buf, pos, ref_stored_len, HA_READ_KEY_EXACT);

  	DBUG_RETURN(error);
}

/*************************************************************************
Stores a reference to a given row to 'ref' field of the handle. */

void
ha_innobase::position(
/*==================*/
	const byte*	record)	/* in: row in MySQL format */
{
	uint	len;
	
	assert(table->keys > 0);

	len = store_key_val_for_row(primary_key, (char*) ref, record);

	assert(len <= ref_length);

	ref_stored_len = len;
}

/*************************************************************************
Returns various information to MySQL interpreter, in various fields
of the handle object. */

void
ha_innobase::info(
/*==============*/
	uint flag)	/* in: what information MySQL requests */
{
 	DBUG_ENTER("info");

 	if (flag & HA_STATUS_VARIABLE) {
    		records = HA_INNOBASE_ROWS_IN_TABLE; // Just to get optimisations right
    		deleted = 0;

  	} else if (flag & HA_STATUS_ERRKEY) {

		errkey = -1;	/* TODO: get the key number from Innobase */
  	}

  	DBUG_VOID_RETURN;
}

int ha_innobase::extra(enum ha_extra_function operation)
{
  return 0;
}

int ha_innobase::reset(void)
{
  return 0;
}

/**********************************************************************
As MySQL will execute an external lock for every new table it uses
we can use this to store the pointer to the THD in the handle. We use this
also in explicit locking of tables by request of the user. */  

int
ha_innobase::external_lock(
/*=======================*/
	THD*	thd,		/* in: handle to the user thread */
	int 	lock_type)	/* in: lock type */
{
	int 	error = 0;

  	DBUG_ENTER("ha_innobase::external_lock");

	update_thd(thd);
  
	DBUG_RETURN(error);
}  

/* Currently, the following does nothing in Innobase: */ 
THR_LOCK_DATA **ha_innobase::store_lock(THD *thd, THR_LOCK_DATA **to,
					enum thr_lock_type lock_type)
{
	/*??????????????????*/

  	*to++= &lock;
  	
	return(to);
}

/*********************************************************************
Creates a table definition to an Innobase database. */
static
int
create_table_def(
/*=============*/
	trx_t*		trx,		/* in: Innobase transaction handle */
	TABLE*		form,		/* in: information on table
					columns and indexes */
	const char*	table_name)	/* in: table name */
{
	Field*		field;
	dict_table_t*	table;
	ulint		n_cols;
  	int 		error;
  	ulint		col_type;
  	ulint		i;

  	DBUG_ENTER("create_table_def");
  	DBUG_PRINT("enter", ("table_name: %s", table_name));

	n_cols = form->fields;

	/* The '0' below specifies that everything is currently
	created in tablespace 0 */

	table = dict_mem_table_create((char*) table_name, 0, n_cols);
	
	for (i = 0; i < n_cols; i++) {
		field = form->field[i];

		col_type = innobase_cmp_type(field);

		dict_mem_table_add_col(table, (char*) field->field_name,
					col_type, (ulint)field->type(),
					field->pack_length(), 0);
	}

	error = row_create_table_for_mysql(table, trx);

	error = convert_error_code_to_mysql(error);

	DBUG_RETURN(error);
}

/*********************************************************************
Creates an index in an Innobase database. */
static
int
create_sub_table(
/*=============*/
	trx_t*		trx,		/* in: Innobase transaction handle */
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

  	DBUG_ENTER("create_sub_table");
  	
	key = form->key_info + key_num;

    	n_fields = key->key_parts;
    	
    	ind_type = 0;

	if (key_num == 0) {
		/* We assume that the clustered index is always
		created first: */
		ind_type = ind_type | DICT_CLUSTERED;
	}
    	
	if (key->flags & HA_NOSAME ) {
		ind_type = ind_type | DICT_UNIQUE;
	}

	/* The '0' below specifies that everything in Innobase is currently
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

	error = convert_error_code_to_mysql(error);

	DBUG_RETURN(error);
}

/*********************************************************************
Creates an index to an Innobase table when the user has defined no
index. */
static
int
create_index_when_no_index(
/*=======================*/
	trx_t*		trx,		/* in: Innobase transaction handle */
	const char*	table_name)	/* in: table name */
{
	dict_index_t*	index;
  	int 		error;  	

  	DBUG_ENTER("create_index_when_no_index");
  	    	
	/* The first '0' below specifies that everything in Innobase is
	currently created in file space 0 */

	index = dict_mem_index_create((char*) table_name, "GEN_CLUST_INDEX",
						0, DICT_CLUSTERED, 0);
	error = row_create_index_for_mysql(index, trx);

	error = convert_error_code_to_mysql(error);

	DBUG_RETURN(error);
}

/*********************************************************************
Creates a new table to an Innobase database. */

int
ha_innobase::create(
/*================*/
					/* out: error number */
	const char*	name,		/* in: table name */
	TABLE*		form,		/* in: information on table
					columns and indexes */
	HA_CREATE_INFO*	create_info)	/* in: ??????? */
{
	int		error;
	dict_table_t*	innobase_table;
	uint		name_len;
	trx_t*		trx;
	char		name2[1000];
  	
  	DBUG_ENTER("ha_innobase::create");

	trx = trx_allocate_for_mysql();

	name_len = strlen(name);

	assert(name_len < 1000);
	assert(name_len > 4);

	memcpy(name2, name, name_len);

	/* Erase the .frm end from table name: */

	name2[name_len - 4] = '\0';
	
  	/* Create the table definition in Innobase */

  	if (error = create_table_def(trx, form, name2)) {

		trx_commit_for_mysql(trx);

  		trx_free_for_mysql(trx);

 		DBUG_RETURN(error);
 	}

	/* Create the keys */

	if (form->keys == 0) {
		/* Create a single index which is used as the clustered
		index; order the rows by their row id generated internally
		by Innobase */

		error = create_index_when_no_index(trx, name2);
    		
  		if (error) {
			trx_commit_for_mysql(trx);
  		
			trx_free_for_mysql(trx);

			DBUG_RETURN(error);
      		}
	} else {
		for (uint i = 0; i < form->keys; i++) {

    			if (error = create_sub_table(trx, form, name2, i)) {
    		
				trx_commit_for_mysql(trx);

  				trx_free_for_mysql(trx);

				DBUG_RETURN(error);
      			}
  		}
  	}

  	trx_commit_for_mysql(trx);

	innobase_table = dict_table_get((char*)name2, NULL);

	assert(innobase_table);
	
	/* Tell the Innobase server that there might be work for
	utility threads: */

	srv_active_wake_master_thread();

  	trx_free_for_mysql(trx);

	DBUG_RETURN(0);
}

/*********************************************************************
Drops a table from an Innobase database. No one is allowed to have
locks on the table, not even the calling user when the table is
dropped. */

int
ha_innobase::delete_table(
/*======================*/
					/* out: error number */
	const char*	name)		/* in: table name */
{
	ulint	name_len;
	int	error;
	trx_t*	trx;
  	
  	DBUG_ENTER("ha_innobase::delete_table");

	trx = trx_allocate_for_mysql();

	name_len = strlen(name);

	assert(name_len < 1000);
	assert(name_len > 4);

	/* Strangely, MySQL passes the table name without the '.frm'
	extension, in contrast to ::create */
	
  	/* Drop the table in Innobase */

  	error = row_drop_table_for_mysql((char*) name, trx, FALSE);
	
	/* Tell the Innobase server that there might be work for
	utility threads: */

	srv_active_wake_master_thread();

  	trx_free_for_mysql(trx);

	error = convert_error_code_to_mysql(error);

	DBUG_RETURN(error);
}

/*************************************************************************
Renames an Innobase table. */

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
  	
  	DBUG_ENTER("ha_innobase::rename_table");

	trx = trx_allocate_for_mysql();

	name_len1 = strlen(from);
	name_len2 = strlen(to);

	assert(name_len1 < 1000);
	assert(name_len1 > 4);
	assert(name_len2 < 1000);
	assert(name_len2 > 4);

	/* TODO: what name extensions MySQL passes here? */
	
  	/* Rename the table in Innobase */

  	error = row_rename_table_for_mysql((char*) from, (char*) to, trx);
	
	/* Tell the Innobase server that there might be work for
	utility threads: */

	srv_active_wake_master_thread();

  	trx_free_for_mysql(trx);

	error = convert_error_code_to_mysql(error);

	DBUG_RETURN(error);
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
	dict_table_t*	table = (dict_table_t*) innobase_table_handle;

	/* In the following formula we assume that scanning 5 pages
	takes the same time as a disk seek: */
	
	return((double) (btr_get_size(
			   dict_table_get_first_index_noninline(table),
			   BTR_N_LEAF_PAGES) / 5));
}

/*************************************************************************
Estimates the number of index records in a range. */

ha_rows
ha_innobase::records_in_range(
/*==========================*/
						/* out: estimated number of rows,
						currently 32-bit int or uint */
	int 			keynr,		/* in: index number */
	const byte*		start_key,	/* in: start key value of the
						range, may also be empty */
	uint 			start_key_len,	/* in: start key val len, may
						also be 0 */
	enum ha_rkey_function 	start_search_flag,/* in: start search condition
						e.g., 'greater than' */
	const byte*		end_key,	/* in: range end key val, may
						also be empty */
	uint 			end_key_len,	/* in: range end key val len,
						may also be 0 */
	enum ha_rkey_function 	end_search_flag)/* in: range end search cond */
{
	row_prebuilt_t* prebuilt	= (row_prebuilt_t*) innobase_prebuilt;
	KEY*		key;
	dict_index_t*	index;
	byte*		key_val_buff2 	= (byte*) my_malloc(table->reclength,
								MYF(MY_WME));
	dtuple_t*	range_end;
	mem_heap_t*	heap;
	ulint		n_rows;
	ulint		mode1;
	ulint		mode2;
								
   	DBUG_ENTER("records_in_range");
  	
	active_index = keynr;

	key = table->key_info + active_index;
	
	index = dict_table_get_index_noninline(
				(dict_table_t*) innobase_table_handle,
							key->name);
	
	/* In converting the first key value we make use of the buffers
	in our handle: */

	convert_key_to_innobase(prebuilt->search_tuple, key_val_buff, index,
				key, (byte*) start_key, (int) start_key_len);
	
	/* For the second key value we have to use allocated buffers: */

	heap = mem_heap_create(100);
	
	range_end = dtuple_create(heap, key->key_parts);

	convert_key_to_innobase(range_end, key_val_buff2, index,
				key, (byte*) end_key, (int) end_key_len);
	
	mode1 = convert_search_mode_to_innobase(start_search_flag);
	mode2 = convert_search_mode_to_innobase(end_search_flag);

	n_rows = btr_estimate_n_rows_in_range(index, prebuilt->search_tuple,
						mode1, range_end, mode2);
	mem_heap_free(heap);
    	my_free((char*) key_val_buff2, MYF(0));

	DBUG_RETURN((ha_rows) n_rows);
}

/****************************************************************************
 Handling the shared INNOBASE_SHARE structure that is needed to provide table
 locking.
****************************************************************************/

static byte* innobase_get_key(INNOBASE_SHARE *share,uint *length,
			      my_bool not_used __attribute__((unused)))
{
  *length=share->table_name_length;
  return (byte*) share->table_name;
}

static INNOBASE_SHARE *get_share(const char *table_name)
{
  INNOBASE_SHARE *share;
  pthread_mutex_lock(&innobase_mutex);
  uint length=(uint) strlen(table_name);
  if (!(share=(INNOBASE_SHARE*) hash_search(&innobase_open_tables, table_name,
					    length)))
  {
    if ((share=(INNOBASE_SHARE *) my_malloc(sizeof(*share)+length+1,
				       MYF(MY_WME | MY_ZEROFILL))))
    {
      share->table_name_length=length;
      share->table_name=(char*) (share+1);
      strmov(share->table_name,table_name);
      if (hash_insert(&innobase_open_tables, (char*) share))
      {
	pthread_mutex_unlock(&innobase_mutex);
	my_free((gptr) share,0);
	return 0;
      }
      thr_lock_init(&share->lock);
      pthread_mutex_init(&share->mutex,NULL);
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
    hash_delete(&innobase_open_tables, (gptr) share);
    thr_lock_delete(&share->lock);
    pthread_mutex_destroy(&share->mutex);
    my_free((gptr) share, MYF(0));
  }
  pthread_mutex_unlock(&innobase_mutex);
}
#endif /* HAVE_INNOBASE_DB */

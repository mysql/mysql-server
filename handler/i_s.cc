/*****************************************************************************

Copyright (c) 2007, 2009, Innobase Oy. All Rights Reserved.

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

/**************************************************//**
@file handler/i_s.cc
InnoDB INFORMATION SCHEMA tables interface to MySQL.

Created July 18, 2007 Vasil Dimov
*******************************************************/
#ifndef MYSQL_SERVER
#define MYSQL_SERVER /* For Item_* classes */
#include <item.h>
/* Prevent influence of this definition to other headers */
#undef MYSQL_SERVER
#else
#include <mysql_priv.h>
#endif //MYSQL_SERVER

#include <mysqld_error.h>
#include <sql_acl.h>                            // PROCESS_ACL

#include <m_ctype.h>
#include <hash.h>
#include <myisampack.h>
#include <mysys_err.h>
#include <my_sys.h>
#include "i_s.h"
#include <sql_plugin.h>
#include <mysql/innodb_priv.h>

extern "C" {
#include "btr0pcur.h"	/* for file sys_tables related info. */
#include "btr0types.h"
#include "buf0buddy.h" /* for i_s_cmpmem */
#include "buf0buf.h" /* for buf_pool and PAGE_ZIP_MIN_SIZE */
#include "dict0load.h"	/* for file sys_tables related info. */
#include "dict0mem.h"
#include "dict0types.h"
#include "ha_prototypes.h" /* for innobase_convert_name() */
#include "srv0srv.h" /* for srv_track_changed_pages */
#include "srv0start.h" /* for srv_was_started */
#include "trx0i_s.h"
#include "trx0trx.h" /* for TRX_QUE_STATE_STR_MAX_LEN */
#include "trx0rseg.h" /* for trx_rseg_struct */
#include "trx0undo.h" /* for trx_undo_struct */
#include "trx0sys.h" /* for trx_sys */
#include "dict0dict.h" /* for dict_sys */
#include "buf0lru.h" /* for XTRA_LRU_[DUMP/RESTORE] */
#include "btr0btr.h" /* for btr_page_get_index_id */
#include "log0online.h"
#include "btr0btr.h"
#include "page0zip.h"
#include "log0log.h"
}

/** structure associates a name string with a file page type and/or buffer
page state. */
struct buffer_page_desc_str_struct{
	const char*	type_str;	/*!< String explain the page
					type/state */
	ulint		type_value;	/*!< Page type or page state */
};

typedef struct buffer_page_desc_str_struct	buf_page_desc_str_t;

/** Any states greater than FIL_PAGE_TYPE_LAST would be treated as unknown. */
#define	I_S_PAGE_TYPE_UNKNOWN		(FIL_PAGE_TYPE_LAST + 1)

/** We also define I_S_PAGE_TYPE_INDEX as the Index Page's position
in i_s_page_type[] array */
#define I_S_PAGE_TYPE_INDEX		1

/** Name string for File Page Types */
static buf_page_desc_str_t	i_s_page_type[] = {
	{"ALLOCATED", FIL_PAGE_TYPE_ALLOCATED},
	{"INDEX", FIL_PAGE_INDEX},
	{"UNDO_LOG", FIL_PAGE_UNDO_LOG},
	{"INODE", FIL_PAGE_INODE},
	{"IBUF_FREE_LIST", FIL_PAGE_IBUF_FREE_LIST},
	{"IBUF_BITMAP", FIL_PAGE_IBUF_BITMAP},
	{"SYSTEM", FIL_PAGE_TYPE_SYS},
	{"TRX_SYSTEM", FIL_PAGE_TYPE_TRX_SYS},
	{"FILE_SPACE_HEADER", FIL_PAGE_TYPE_FSP_HDR},
	{"EXTENT_DESCRIPTOR", FIL_PAGE_TYPE_XDES},
	{"BLOB", FIL_PAGE_TYPE_BLOB},
	{"COMPRESSED_BLOB", FIL_PAGE_TYPE_ZBLOB},
	{"COMPRESSED_BLOB2", FIL_PAGE_TYPE_ZBLOB2},
	{"UNKNOWN", I_S_PAGE_TYPE_UNKNOWN}
};

/* Check if we can hold all page type in a 4 bit value */
#if I_S_PAGE_TYPE_UNKNOWN > 1<<4
# error "i_s_page_type[] is too large"
#endif

/** This structure defines information we will fetch from pages
currently cached in the buffer pool. It will be used to populate
table INFORMATION_SCHEMA.INNODB_BUFFER_PAGE */
struct buffer_page_info_struct{
	ulint		block_id;	/*!< Buffer Pool block ID */
	unsigned	space_id:32;	/*!< Tablespace ID */
	unsigned	page_num:32;	/*!< Page number/offset */
	unsigned	access_time:32;	/*!< Time of first access */
	unsigned	pool_id:MAX_BUFFER_POOLS_BITS;
					/*!< Buffer Pool ID. Must be less than
					MAX_BUFFER_POOLS */
	unsigned	flush_type:2;	/*!< Flush type */
	unsigned	io_fix:2;	/*!< type of pending I/O operation */
	unsigned	fix_count:19;	/*!< Count of how manyfold this block
					is bufferfixed */
	unsigned	hashed:1;	/*!< Whether hash index has been
					built on this page */
	unsigned	is_old:1;	/*!< TRUE if the block is in the old
					blocks in buf_pool->LRU_old */
	unsigned	freed_page_clock:31; /*!< the value of
					buf_pool->freed_page_clock */
	unsigned	zip_ssize:PAGE_ZIP_SSIZE_BITS;
					/*!< Compressed page size */
	unsigned	page_state:BUF_PAGE_STATE_BITS; /*!< Page state */
	unsigned	page_type:4;	/*!< Page type */
	unsigned	num_recs;
					/*!< Number of records on Page */
	unsigned	data_size;
					/*!< Sum of the sizes of the records */
	lsn_t		newest_mod;	/*!< Log sequence number of
					the youngest modification */
	lsn_t		oldest_mod;	/*!< Log sequence number of
					the oldest modification */
	index_id_t	index_id;	/*!< Index ID if a index page */
};

typedef struct buffer_page_info_struct	buf_page_info_t;

/** maximum number of buffer page info we would cache. */
#define MAX_BUF_INFO_CACHED		10000


#define OK(expr)		\
	if ((expr) != 0) {	\
		DBUG_RETURN(1);	\
	}

#define RETURN_IF_INNODB_NOT_STARTED(plugin_name)			\
do {									\
	if (!srv_was_started) {						\
		push_warning_printf(thd, MYSQL_ERROR::WARN_LEVEL_WARN,	\
				    ER_CANT_FIND_SYSTEM_REC,		\
				    "InnoDB: SELECTing from "		\
				    "INFORMATION_SCHEMA.%s but "	\
				    "the InnoDB storage engine "	\
				    "is not installed", plugin_name);	\
		DBUG_RETURN(0);						\
	}								\
} while (0)

#if !defined __STRICT_ANSI__ && defined __GNUC__ && (__GNUC__) > 2 && !defined __INTEL_COMPILER
#define STRUCT_FLD(name, value)	name: value
#else
#define STRUCT_FLD(name, value)	value
#endif

/* Don't use a static const variable here, as some C++ compilers (notably
HPUX aCC: HP ANSI C++ B3910B A.03.65) can't handle it. */
#define END_OF_ST_FIELD_INFO \
	{STRUCT_FLD(field_name,		NULL), \
	 STRUCT_FLD(field_length,	0), \
	 STRUCT_FLD(field_type,		MYSQL_TYPE_NULL), \
	 STRUCT_FLD(value,		0), \
	 STRUCT_FLD(field_flags,	0), \
	 STRUCT_FLD(old_name,		""), \
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)}

/*
Use the following types mapping:

C type	ST_FIELD_INFO::field_type
---------------------------------
long			MYSQL_TYPE_LONGLONG
(field_length=MY_INT64_NUM_DECIMAL_DIGITS)

long unsigned		MYSQL_TYPE_LONGLONG
(field_length=MY_INT64_NUM_DECIMAL_DIGITS, field_flags=MY_I_S_UNSIGNED)

char*			MYSQL_TYPE_STRING
(field_length=n)

float			MYSQL_TYPE_FLOAT
(field_length=0 is ignored)

void*			MYSQL_TYPE_LONGLONG
(field_length=MY_INT64_NUM_DECIMAL_DIGITS, field_flags=MY_I_S_UNSIGNED)

boolean (if else)	MYSQL_TYPE_LONG
(field_length=1)

time_t			MYSQL_TYPE_DATETIME
(field_length=0 ignored)
---------------------------------
*/

/*******************************************************************//**
Common function to fill any of the dynamic tables:
INFORMATION_SCHEMA.innodb_trx
INFORMATION_SCHEMA.innodb_locks
INFORMATION_SCHEMA.innodb_lock_waits
@return	0 on success */
static
int
trx_i_s_common_fill_table(
/*======================*/
	THD*		thd,	/*!< in: thread */
	TABLE_LIST*	tables,	/*!< in/out: tables to fill */
	COND*		cond);	/*!< in: condition (not used) */

/*******************************************************************//**
Unbind a dynamic INFORMATION_SCHEMA table.
@return	0 on success */
static
int
i_s_common_deinit(
/*==============*/
	void*	p);	/*!< in/out: table schema object */
/*******************************************************************//**
Auxiliary function to store time_t value in MYSQL_TYPE_DATETIME
field.
@return	0 on success */
static
int
field_store_time_t(
/*===============*/
	Field*	field,	/*!< in/out: target field for storage */
	time_t	time)	/*!< in: value to store */
{
	MYSQL_TIME	my_time;
	struct tm	tm_time;

#if 0
	/* use this if you are sure that `variables' and `time_zone'
	are always initialized */
	thd->variables.time_zone->gmt_sec_to_TIME(
		&my_time, (my_time_t) time);
#else
	localtime_r(&time, &tm_time);
	localtime_to_TIME(&my_time, &tm_time);
	my_time.time_type = MYSQL_TIMESTAMP_DATETIME;
#endif

	return(field->store_time(&my_time, MYSQL_TIMESTAMP_DATETIME));
}

/*******************************************************************//**
Auxiliary function to store char* value in MYSQL_TYPE_STRING field.
@return	0 on success */
static
int
field_store_string(
/*===============*/
	Field*		field,	/*!< in/out: target field for storage */
	const char*	str)	/*!< in: NUL-terminated utf-8 string,
				or NULL */
{
	int	ret;

	if (str != NULL) {

		ret = field->store(str, strlen(str),
				   system_charset_info);
		field->set_notnull();
	} else {

		ret = 0; /* success */
		field->set_null();
	}

	return(ret);
}

/*******************************************************************//**
Auxiliary function to store ulint value in MYSQL_TYPE_LONGLONG field.
If the value is ULINT_UNDEFINED then the field it set to NULL.
@return	0 on success */
static
int
field_store_ulint(
/*==============*/
	Field*	field,	/*!< in/out: target field for storage */
	ulint	n)	/*!< in: value to store */
{
	int	ret;

	if (n != ULINT_UNDEFINED) {

		ret = field->store(n);
		field->set_notnull();
	} else {

		ret = 0; /* success */
		field->set_null();
	}

	return(ret);
}

/* Fields of the dynamic table INFORMATION_SCHEMA.innodb_trx */
static ST_FIELD_INFO	innodb_trx_fields_info[] =
{
#define IDX_TRX_ID		0
	{STRUCT_FLD(field_name,		"trx_id"),
	 STRUCT_FLD(field_length,	TRX_ID_MAX_LEN + 1),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_STRING),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	0),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define IDX_TRX_STATE		1
	{STRUCT_FLD(field_name,		"trx_state"),
	 STRUCT_FLD(field_length,	TRX_QUE_STATE_STR_MAX_LEN + 1),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_STRING),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	0),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define IDX_TRX_STARTED		2
	{STRUCT_FLD(field_name,		"trx_started"),
	 STRUCT_FLD(field_length,	0),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_DATETIME),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	0),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define IDX_TRX_REQUESTED_LOCK_ID	3
	{STRUCT_FLD(field_name,		"trx_requested_lock_id"),
	 STRUCT_FLD(field_length,	TRX_I_S_LOCK_ID_MAX_LEN + 1),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_STRING),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_MAYBE_NULL),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define IDX_TRX_WAIT_STARTED	4
	{STRUCT_FLD(field_name,		"trx_wait_started"),
	 STRUCT_FLD(field_length,	0),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_DATETIME),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_MAYBE_NULL),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define IDX_TRX_WEIGHT		5
	{STRUCT_FLD(field_name,		"trx_weight"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define IDX_TRX_MYSQL_THREAD_ID	6
	{STRUCT_FLD(field_name,		"trx_mysql_thread_id"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define IDX_TRX_QUERY		7
	{STRUCT_FLD(field_name,		"trx_query"),
	 STRUCT_FLD(field_length,	TRX_I_S_TRX_QUERY_MAX_LEN),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_STRING),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_MAYBE_NULL),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define IDX_TRX_OPERATION_STATE	8
	{STRUCT_FLD(field_name,		"trx_operation_state"),
	 STRUCT_FLD(field_length,	TRX_I_S_TRX_OP_STATE_MAX_LEN),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_STRING),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_MAYBE_NULL),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define IDX_TRX_TABLES_IN_USE	9
	{STRUCT_FLD(field_name,		"trx_tables_in_use"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define IDX_TRX_TABLES_LOCKED	10
	{STRUCT_FLD(field_name,		"trx_tables_locked"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define IDX_TRX_LOCK_STRUCTS	11
	{STRUCT_FLD(field_name,		"trx_lock_structs"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define IDX_TRX_LOCK_MEMORY_BYTES	12
	{STRUCT_FLD(field_name,		"trx_lock_memory_bytes"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define IDX_TRX_ROWS_LOCKED	13
	{STRUCT_FLD(field_name,		"trx_rows_locked"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define IDX_TRX_ROWS_MODIFIED		14
	{STRUCT_FLD(field_name,		"trx_rows_modified"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define IDX_TRX_CONNCURRENCY_TICKETS	15
	{STRUCT_FLD(field_name,		"trx_concurrency_tickets"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define IDX_TRX_ISOLATION_LEVEL	16
	{STRUCT_FLD(field_name,		"trx_isolation_level"),
	 STRUCT_FLD(field_length,	TRX_I_S_TRX_ISOLATION_LEVEL_MAX_LEN),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_STRING),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	0),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define IDX_TRX_UNIQUE_CHECKS	17
	{STRUCT_FLD(field_name,		"trx_unique_checks"),
	 STRUCT_FLD(field_length,	1),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONG),
	 STRUCT_FLD(value,		1),
	 STRUCT_FLD(field_flags,	0),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define IDX_TRX_FOREIGN_KEY_CHECKS	18
	{STRUCT_FLD(field_name,		"trx_foreign_key_checks"),
	 STRUCT_FLD(field_length,	1),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONG),
	 STRUCT_FLD(value,		1),
	 STRUCT_FLD(field_flags,	0),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define IDX_TRX_LAST_FOREIGN_KEY_ERROR	19
	{STRUCT_FLD(field_name,		"trx_last_foreign_key_error"),
	 STRUCT_FLD(field_length,	TRX_I_S_TRX_FK_ERROR_MAX_LEN),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_STRING),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_MAYBE_NULL),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define IDX_TRX_ADAPTIVE_HASH_LATCHED	20
	{STRUCT_FLD(field_name,		"trx_adaptive_hash_latched"),
	 STRUCT_FLD(field_length,	1),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	0),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define IDX_TRX_ADAPTIVE_HASH_TIMEOUT	21
	{STRUCT_FLD(field_name,		"trx_adaptive_hash_timeout"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	END_OF_ST_FIELD_INFO
};

/*******************************************************************//**
Read data from cache buffer and fill the INFORMATION_SCHEMA.innodb_trx
table with it.
@return	0 on success */
static
int
fill_innodb_trx_from_cache(
/*=======================*/
	trx_i_s_cache_t*	cache,	/*!< in: cache to read from */
	THD*			thd,	/*!< in: used to call
					schema_table_store_record() */
	TABLE*			table)	/*!< in/out: fill this table */
{
	Field**	fields;
	ulint	rows_num;
	char	lock_id[TRX_I_S_LOCK_ID_MAX_LEN + 1];
	ulint	i;

	DBUG_ENTER("fill_innodb_trx_from_cache");

	fields = table->field;

	rows_num = trx_i_s_cache_get_rows_used(cache,
					       I_S_INNODB_TRX);

	for (i = 0; i < rows_num; i++) {

		i_s_trx_row_t*	row;
		char		trx_id[TRX_ID_MAX_LEN + 1];

		row = (i_s_trx_row_t*)
			trx_i_s_cache_get_nth_row(
				cache, I_S_INNODB_TRX, i);

		/* trx_id */
		ut_snprintf(trx_id, sizeof(trx_id), TRX_ID_FMT, row->trx_id);
		OK(field_store_string(fields[IDX_TRX_ID], trx_id));

		/* trx_state */
		OK(field_store_string(fields[IDX_TRX_STATE],
				      row->trx_state));

		/* trx_started */
		OK(field_store_time_t(fields[IDX_TRX_STARTED],
				      (time_t) row->trx_started));

		/* trx_requested_lock_id */
		/* trx_wait_started */
		if (row->trx_wait_started != 0) {

			OK(field_store_string(
				   fields[IDX_TRX_REQUESTED_LOCK_ID],
				   trx_i_s_create_lock_id(
					   row->requested_lock_row,
					   lock_id, sizeof(lock_id))));
			/* field_store_string() sets it no notnull */

			OK(field_store_time_t(
				   fields[IDX_TRX_WAIT_STARTED],
				   (time_t) row->trx_wait_started));
			fields[IDX_TRX_WAIT_STARTED]->set_notnull();
		} else {

			fields[IDX_TRX_REQUESTED_LOCK_ID]->set_null();
			fields[IDX_TRX_WAIT_STARTED]->set_null();
		}

		/* trx_weight */
		OK(fields[IDX_TRX_WEIGHT]->store((longlong) row->trx_weight,
						 true));

		/* trx_mysql_thread_id */
		OK(fields[IDX_TRX_MYSQL_THREAD_ID]->store(
			   row->trx_mysql_thread_id));

		/* trx_query */
		if (row->trx_query) {
			/* store will do appropriate character set
			conversion check */
			fields[IDX_TRX_QUERY]->store(
				row->trx_query, strlen(row->trx_query),
				row->trx_query_cs);
			fields[IDX_TRX_QUERY]->set_notnull();
		} else {
			fields[IDX_TRX_QUERY]->set_null();
		}

		/* trx_operation_state */
		OK(field_store_string(fields[IDX_TRX_OPERATION_STATE],
				      row->trx_operation_state));

		/* trx_tables_in_use */
		OK(fields[IDX_TRX_TABLES_IN_USE]->store(
			   (longlong) row->trx_tables_in_use, true));

		/* trx_tables_locked */
		OK(fields[IDX_TRX_TABLES_LOCKED]->store(
			   (longlong) row->trx_tables_locked, true));

		/* trx_lock_structs */
		OK(fields[IDX_TRX_LOCK_STRUCTS]->store(
			   (longlong) row->trx_lock_structs, true));

		/* trx_lock_memory_bytes */
		OK(fields[IDX_TRX_LOCK_MEMORY_BYTES]->store(
			   (longlong) row->trx_lock_memory_bytes, true));

		/* trx_rows_locked */
		OK(fields[IDX_TRX_ROWS_LOCKED]->store(
			   (longlong) row->trx_rows_locked, true));

		/* trx_rows_modified */
		OK(fields[IDX_TRX_ROWS_MODIFIED]->store(
			   (longlong) row->trx_rows_modified, true));

		/* trx_concurrency_tickets */
		OK(fields[IDX_TRX_CONNCURRENCY_TICKETS]->store(
			   (longlong) row->trx_concurrency_tickets, true));

		/* trx_isolation_level */
		OK(field_store_string(fields[IDX_TRX_ISOLATION_LEVEL],
				      row->trx_isolation_level));

		/* trx_unique_checks */
		OK(fields[IDX_TRX_UNIQUE_CHECKS]->store(
			   row->trx_unique_checks));

		/* trx_foreign_key_checks */
		OK(fields[IDX_TRX_FOREIGN_KEY_CHECKS]->store(
			   row->trx_foreign_key_checks));

		/* trx_last_foreign_key_error */
		OK(field_store_string(fields[IDX_TRX_LAST_FOREIGN_KEY_ERROR],
				      row->trx_foreign_key_error));

		/* trx_adaptive_hash_latched */
		OK(fields[IDX_TRX_ADAPTIVE_HASH_LATCHED]->store(
			   row->trx_has_search_latch));

		/* trx_adaptive_hash_timeout */
		OK(fields[IDX_TRX_ADAPTIVE_HASH_TIMEOUT]->store(
			   (longlong) row->trx_search_latch_timeout, true));

		OK(schema_table_store_record(thd, table));
	}

	DBUG_RETURN(0);
}

/*******************************************************************//**
Bind the dynamic table INFORMATION_SCHEMA.innodb_trx
@return	0 on success */
static
int
innodb_trx_init(
/*============*/
	void*	p)	/*!< in/out: table schema object */
{
	ST_SCHEMA_TABLE*	schema;

	DBUG_ENTER("innodb_trx_init");

	schema = (ST_SCHEMA_TABLE*) p;

	schema->fields_info = innodb_trx_fields_info;
	schema->fill_table = trx_i_s_common_fill_table;

	DBUG_RETURN(0);
}

static struct st_mysql_information_schema	i_s_info =
{
	MYSQL_INFORMATION_SCHEMA_INTERFACE_VERSION
};

UNIV_INTERN struct st_mysql_plugin	i_s_innodb_trx =
{
	/* the plugin type (a MYSQL_XXX_PLUGIN value) */
	/* int */
	STRUCT_FLD(type, MYSQL_INFORMATION_SCHEMA_PLUGIN),

	/* pointer to type-specific plugin descriptor */
	/* void* */
	STRUCT_FLD(info, &i_s_info),

	/* plugin name */
	/* const char* */
	STRUCT_FLD(name, "INNODB_TRX"),

	/* plugin author (for SHOW PLUGINS) */
	/* const char* */
	STRUCT_FLD(author, plugin_author),

	/* general descriptive text (for SHOW PLUGINS) */
	/* const char* */
	STRUCT_FLD(descr, "InnoDB transactions"),

	/* the plugin license (PLUGIN_LICENSE_XXX) */
	/* int */
	STRUCT_FLD(license, PLUGIN_LICENSE_GPL),

	/* the function to invoke when plugin is loaded */
	/* int (*)(void*); */
	STRUCT_FLD(init, innodb_trx_init),

	/* the function to invoke when plugin is unloaded */
	/* int (*)(void*); */
	STRUCT_FLD(deinit, i_s_common_deinit),

	/* plugin version (for SHOW PLUGINS) */
	/* unsigned int */
	STRUCT_FLD(version, INNODB_VERSION_SHORT),

	/* struct st_mysql_show_var* */
	STRUCT_FLD(status_vars, NULL),

	/* struct st_mysql_sys_var** */
	STRUCT_FLD(system_vars, NULL),

	/* reserved for dependency checking */
	/* void* */
	STRUCT_FLD(__reserved1, NULL),

	/* Plugin flags */
	/* unsigned long */
	STRUCT_FLD(flags, 0UL),
};

/* Fields of the dynamic table INFORMATION_SCHEMA.innodb_locks */
static ST_FIELD_INFO	innodb_locks_fields_info[] =
{
#define IDX_LOCK_ID		0
	{STRUCT_FLD(field_name,		"lock_id"),
	 STRUCT_FLD(field_length,	TRX_I_S_LOCK_ID_MAX_LEN + 1),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_STRING),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	0),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define IDX_LOCK_TRX_ID		1
	{STRUCT_FLD(field_name,		"lock_trx_id"),
	 STRUCT_FLD(field_length,	TRX_ID_MAX_LEN + 1),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_STRING),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	0),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define IDX_LOCK_MODE		2
	{STRUCT_FLD(field_name,		"lock_mode"),
	 /* S[,GAP] X[,GAP] IS[,GAP] IX[,GAP] AUTO_INC UNKNOWN */
	 STRUCT_FLD(field_length,	32),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_STRING),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	0),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define IDX_LOCK_TYPE		3
	{STRUCT_FLD(field_name,		"lock_type"),
	 STRUCT_FLD(field_length,	32 /* RECORD|TABLE|UNKNOWN */),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_STRING),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	0),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define IDX_LOCK_TABLE		4
	{STRUCT_FLD(field_name,		"lock_table"),
	 STRUCT_FLD(field_length,	1024),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_STRING),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	0),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define IDX_LOCK_INDEX		5
	{STRUCT_FLD(field_name,		"lock_index"),
	 STRUCT_FLD(field_length,	1024),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_STRING),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_MAYBE_NULL),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define IDX_LOCK_SPACE		6
	{STRUCT_FLD(field_name,		"lock_space"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED | MY_I_S_MAYBE_NULL),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define IDX_LOCK_PAGE		7
	{STRUCT_FLD(field_name,		"lock_page"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED | MY_I_S_MAYBE_NULL),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define IDX_LOCK_REC		8
	{STRUCT_FLD(field_name,		"lock_rec"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED | MY_I_S_MAYBE_NULL),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define IDX_LOCK_DATA		9
	{STRUCT_FLD(field_name,		"lock_data"),
	 STRUCT_FLD(field_length,	TRX_I_S_LOCK_DATA_MAX_LEN),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_STRING),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_MAYBE_NULL),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	END_OF_ST_FIELD_INFO
};

/*******************************************************************//**
Read data from cache buffer and fill the INFORMATION_SCHEMA.innodb_locks
table with it.
@return	0 on success */
static
int
fill_innodb_locks_from_cache(
/*=========================*/
	trx_i_s_cache_t*	cache,	/*!< in: cache to read from */
	THD*			thd,	/*!< in: MySQL client connection */
	TABLE*			table)	/*!< in/out: fill this table */
{
	Field**	fields;
	ulint	rows_num;
	char	lock_id[TRX_I_S_LOCK_ID_MAX_LEN + 1];
	ulint	i;

	DBUG_ENTER("fill_innodb_locks_from_cache");

	fields = table->field;

	rows_num = trx_i_s_cache_get_rows_used(cache,
					       I_S_INNODB_LOCKS);

	for (i = 0; i < rows_num; i++) {

		i_s_locks_row_t*	row;
		char			buf[MAX_FULL_NAME_LEN + 1];
		const char*		bufend;

		char			lock_trx_id[TRX_ID_MAX_LEN + 1];

		row = (i_s_locks_row_t*)
			trx_i_s_cache_get_nth_row(
				cache, I_S_INNODB_LOCKS, i);

		/* lock_id */
		trx_i_s_create_lock_id(row, lock_id, sizeof(lock_id));
		OK(field_store_string(fields[IDX_LOCK_ID],
				      lock_id));

		/* lock_trx_id */
		ut_snprintf(lock_trx_id, sizeof(lock_trx_id),
			    TRX_ID_FMT, row->lock_trx_id);
		OK(field_store_string(fields[IDX_LOCK_TRX_ID], lock_trx_id));

		/* lock_mode */
		OK(field_store_string(fields[IDX_LOCK_MODE],
				      row->lock_mode));

		/* lock_type */
		OK(field_store_string(fields[IDX_LOCK_TYPE],
				      row->lock_type));

		/* lock_table */
		bufend = innobase_convert_name(buf, sizeof(buf),
					       row->lock_table,
					       strlen(row->lock_table),
					       thd, TRUE);
		OK(fields[IDX_LOCK_TABLE]->store(buf, bufend - buf,
						 system_charset_info));

		/* lock_index */
		if (row->lock_index != NULL) {

			bufend = innobase_convert_name(buf, sizeof(buf),
						       row->lock_index,
						       strlen(row->lock_index),
						       thd, FALSE);
			OK(fields[IDX_LOCK_INDEX]->store(buf, bufend - buf,
							 system_charset_info));
			fields[IDX_LOCK_INDEX]->set_notnull();
		} else {

			fields[IDX_LOCK_INDEX]->set_null();
		}

		/* lock_space */
		OK(field_store_ulint(fields[IDX_LOCK_SPACE],
				     row->lock_space));

		/* lock_page */
		OK(field_store_ulint(fields[IDX_LOCK_PAGE],
				     row->lock_page));

		/* lock_rec */
		OK(field_store_ulint(fields[IDX_LOCK_REC],
				     row->lock_rec));

		/* lock_data */
		OK(field_store_string(fields[IDX_LOCK_DATA],
				      row->lock_data));

		OK(schema_table_store_record(thd, table));
	}

	DBUG_RETURN(0);
}

/*******************************************************************//**
Bind the dynamic table INFORMATION_SCHEMA.innodb_locks
@return	0 on success */
static
int
innodb_locks_init(
/*==============*/
	void*	p)	/*!< in/out: table schema object */
{
	ST_SCHEMA_TABLE*	schema;

	DBUG_ENTER("innodb_locks_init");

	schema = (ST_SCHEMA_TABLE*) p;

	schema->fields_info = innodb_locks_fields_info;
	schema->fill_table = trx_i_s_common_fill_table;

	DBUG_RETURN(0);
}

UNIV_INTERN struct st_mysql_plugin	i_s_innodb_locks =
{
	/* the plugin type (a MYSQL_XXX_PLUGIN value) */
	/* int */
	STRUCT_FLD(type, MYSQL_INFORMATION_SCHEMA_PLUGIN),

	/* pointer to type-specific plugin descriptor */
	/* void* */
	STRUCT_FLD(info, &i_s_info),

	/* plugin name */
	/* const char* */
	STRUCT_FLD(name, "INNODB_LOCKS"),

	/* plugin author (for SHOW PLUGINS) */
	/* const char* */
	STRUCT_FLD(author, plugin_author),

	/* general descriptive text (for SHOW PLUGINS) */
	/* const char* */
	STRUCT_FLD(descr, "InnoDB conflicting locks"),

	/* the plugin license (PLUGIN_LICENSE_XXX) */
	/* int */
	STRUCT_FLD(license, PLUGIN_LICENSE_GPL),

	/* the function to invoke when plugin is loaded */
	/* int (*)(void*); */
	STRUCT_FLD(init, innodb_locks_init),

	/* the function to invoke when plugin is unloaded */
	/* int (*)(void*); */
	STRUCT_FLD(deinit, i_s_common_deinit),

	/* plugin version (for SHOW PLUGINS) */
	/* unsigned int */
	STRUCT_FLD(version, INNODB_VERSION_SHORT),

	/* struct st_mysql_show_var* */
	STRUCT_FLD(status_vars, NULL),

	/* struct st_mysql_sys_var** */
	STRUCT_FLD(system_vars, NULL),

	/* reserved for dependency checking */
	/* void* */
	STRUCT_FLD(__reserved1, NULL),

	/* Plugin flags */
	/* unsigned long */
	STRUCT_FLD(flags, 0UL),
};

/* Fields of the dynamic table INFORMATION_SCHEMA.innodb_lock_waits */
static ST_FIELD_INFO	innodb_lock_waits_fields_info[] =
{
#define IDX_REQUESTING_TRX_ID	0
	{STRUCT_FLD(field_name,		"requesting_trx_id"),
	 STRUCT_FLD(field_length,	TRX_ID_MAX_LEN + 1),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_STRING),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	0),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define IDX_REQUESTED_LOCK_ID	1
	{STRUCT_FLD(field_name,		"requested_lock_id"),
	 STRUCT_FLD(field_length,	TRX_I_S_LOCK_ID_MAX_LEN + 1),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_STRING),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	0),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define IDX_BLOCKING_TRX_ID	2
	{STRUCT_FLD(field_name,		"blocking_trx_id"),
	 STRUCT_FLD(field_length,	TRX_ID_MAX_LEN + 1),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_STRING),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	0),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define IDX_BLOCKING_LOCK_ID	3
	{STRUCT_FLD(field_name,		"blocking_lock_id"),
	 STRUCT_FLD(field_length,	TRX_I_S_LOCK_ID_MAX_LEN + 1),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_STRING),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	0),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	END_OF_ST_FIELD_INFO
};

/*******************************************************************//**
Read data from cache buffer and fill the
INFORMATION_SCHEMA.innodb_lock_waits table with it.
@return	0 on success */
static
int
fill_innodb_lock_waits_from_cache(
/*==============================*/
	trx_i_s_cache_t*	cache,	/*!< in: cache to read from */
	THD*			thd,	/*!< in: used to call
					schema_table_store_record() */
	TABLE*			table)	/*!< in/out: fill this table */
{
	Field**	fields;
	ulint	rows_num;
	char	requested_lock_id[TRX_I_S_LOCK_ID_MAX_LEN + 1];
	char	blocking_lock_id[TRX_I_S_LOCK_ID_MAX_LEN + 1];
	ulint	i;

	DBUG_ENTER("fill_innodb_lock_waits_from_cache");

	fields = table->field;

	rows_num = trx_i_s_cache_get_rows_used(cache,
					       I_S_INNODB_LOCK_WAITS);

	for (i = 0; i < rows_num; i++) {

		i_s_lock_waits_row_t*	row;

		char	requesting_trx_id[TRX_ID_MAX_LEN + 1];
		char	blocking_trx_id[TRX_ID_MAX_LEN + 1];

		row = (i_s_lock_waits_row_t*)
			trx_i_s_cache_get_nth_row(
				cache, I_S_INNODB_LOCK_WAITS, i);

		/* requesting_trx_id */
		ut_snprintf(requesting_trx_id, sizeof(requesting_trx_id),
			    TRX_ID_FMT, row->requested_lock_row->lock_trx_id);
		OK(field_store_string(fields[IDX_REQUESTING_TRX_ID],
				      requesting_trx_id));

		/* requested_lock_id */
		OK(field_store_string(
			   fields[IDX_REQUESTED_LOCK_ID],
			   trx_i_s_create_lock_id(
				   row->requested_lock_row,
				   requested_lock_id,
				   sizeof(requested_lock_id))));

		/* blocking_trx_id */
		ut_snprintf(blocking_trx_id, sizeof(blocking_trx_id),
			    TRX_ID_FMT, row->blocking_lock_row->lock_trx_id);
		OK(field_store_string(fields[IDX_BLOCKING_TRX_ID],
				      blocking_trx_id));

		/* blocking_lock_id */
		OK(field_store_string(
			   fields[IDX_BLOCKING_LOCK_ID],
			   trx_i_s_create_lock_id(
				   row->blocking_lock_row,
				   blocking_lock_id,
				   sizeof(blocking_lock_id))));

		OK(schema_table_store_record(thd, table));
	}

	DBUG_RETURN(0);
}

/*******************************************************************//**
Bind the dynamic table INFORMATION_SCHEMA.innodb_lock_waits
@return	0 on success */
static
int
innodb_lock_waits_init(
/*===================*/
	void*	p)	/*!< in/out: table schema object */
{
	ST_SCHEMA_TABLE*	schema;

	DBUG_ENTER("innodb_lock_waits_init");

	schema = (ST_SCHEMA_TABLE*) p;

	schema->fields_info = innodb_lock_waits_fields_info;
	schema->fill_table = trx_i_s_common_fill_table;

	DBUG_RETURN(0);
}

UNIV_INTERN struct st_mysql_plugin	i_s_innodb_lock_waits =
{
	/* the plugin type (a MYSQL_XXX_PLUGIN value) */
	/* int */
	STRUCT_FLD(type, MYSQL_INFORMATION_SCHEMA_PLUGIN),

	/* pointer to type-specific plugin descriptor */
	/* void* */
	STRUCT_FLD(info, &i_s_info),

	/* plugin name */
	/* const char* */
	STRUCT_FLD(name, "INNODB_LOCK_WAITS"),

	/* plugin author (for SHOW PLUGINS) */
	/* const char* */
	STRUCT_FLD(author, plugin_author),

	/* general descriptive text (for SHOW PLUGINS) */
	/* const char* */
	STRUCT_FLD(descr, "InnoDB which lock is blocking which"),

	/* the plugin license (PLUGIN_LICENSE_XXX) */
	/* int */
	STRUCT_FLD(license, PLUGIN_LICENSE_GPL),

	/* the function to invoke when plugin is loaded */
	/* int (*)(void*); */
	STRUCT_FLD(init, innodb_lock_waits_init),

	/* the function to invoke when plugin is unloaded */
	/* int (*)(void*); */
	STRUCT_FLD(deinit, i_s_common_deinit),

	/* plugin version (for SHOW PLUGINS) */
	/* unsigned int */
	STRUCT_FLD(version, INNODB_VERSION_SHORT),

	/* struct st_mysql_show_var* */
	STRUCT_FLD(status_vars, NULL),

	/* struct st_mysql_sys_var** */
	STRUCT_FLD(system_vars, NULL),

	/* reserved for dependency checking */
	/* void* */
	STRUCT_FLD(__reserved1, NULL),

	/* Plugin flags */
	/* unsigned long */
	STRUCT_FLD(flags, 0UL),
};

/*******************************************************************//**
Common function to fill any of the dynamic tables:
INFORMATION_SCHEMA.innodb_trx
INFORMATION_SCHEMA.innodb_locks
INFORMATION_SCHEMA.innodb_lock_waits
@return	0 on success */
static
int
trx_i_s_common_fill_table(
/*======================*/
	THD*		thd,	/*!< in: thread */
	TABLE_LIST*	tables,	/*!< in/out: tables to fill */
	COND*		cond)	/*!< in: condition (not used) */
{
	const char*		table_name;
	int			ret;
	trx_i_s_cache_t*	cache;

	DBUG_ENTER("trx_i_s_common_fill_table");

	/* deny access to non-superusers */
	if (check_global_access(thd, PROCESS_ACL)) {

		DBUG_RETURN(0);
	}

	/* minimize the number of places where global variables are
	referenced */
	cache = trx_i_s_cache;

	/* which table we have to fill? */
	table_name = tables->schema_table_name;
	/* or table_name = tables->schema_table->table_name; */

	RETURN_IF_INNODB_NOT_STARTED(table_name);

	/* update the cache */
	trx_i_s_cache_start_write(cache);
	trx_i_s_possibly_fetch_data_into_cache(cache);
	trx_i_s_cache_end_write(cache);

	if (trx_i_s_cache_is_truncated(cache)) {

		/* XXX show warning to user if possible */
		fprintf(stderr, "Warning: data in %s truncated due to "
			"memory limit of %d bytes\n", table_name,
			TRX_I_S_MEM_LIMIT);
	}

	ret = 0;

	trx_i_s_cache_start_read(cache);

	if (innobase_strcasecmp(table_name, "innodb_trx") == 0) {

		if (fill_innodb_trx_from_cache(
			cache, thd, tables->table) != 0) {

			ret = 1;
		}

	} else if (innobase_strcasecmp(table_name, "innodb_locks") == 0) {

		if (fill_innodb_locks_from_cache(
			cache, thd, tables->table) != 0) {

			ret = 1;
		}

	} else if (innobase_strcasecmp(table_name, "innodb_lock_waits") == 0) {

		if (fill_innodb_lock_waits_from_cache(
			cache, thd, tables->table) != 0) {

			ret = 1;
		}

	} else {

		/* huh! what happened!? */
		fprintf(stderr,
			"InnoDB: trx_i_s_common_fill_table() was "
			"called to fill unknown table: %s.\n"
			"This function only knows how to fill "
			"innodb_trx, innodb_locks and "
			"innodb_lock_waits tables.\n", table_name);

		ret = 1;
	}

	trx_i_s_cache_end_read(cache);

#if 0
	DBUG_RETURN(ret);
#else
	/* if this function returns something else than 0 then a
	deadlock occurs between the mysqld server and mysql client,
	see http://bugs.mysql.com/29900 ; when that bug is resolved
	we can enable the DBUG_RETURN(ret) above */
	ret++;  // silence a gcc46 warning
	DBUG_RETURN(0);
#endif
}

/* Fields of the dynamic table information_schema.innodb_cmp. */
static ST_FIELD_INFO	i_s_cmp_fields_info[] =
{
	{STRUCT_FLD(field_name,		"page_size"),
	 STRUCT_FLD(field_length,	5),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	0),
	 STRUCT_FLD(old_name,		"Compressed Page Size"),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	{STRUCT_FLD(field_name,		"compress_ops"),
	 STRUCT_FLD(field_length,	MY_INT32_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	0),
	 STRUCT_FLD(old_name,		"Total Number of Compressions"),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	{STRUCT_FLD(field_name,		"compress_ops_ok"),
	 STRUCT_FLD(field_length,	MY_INT32_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	0),
	 STRUCT_FLD(old_name,		"Total Number of"
					" Successful Compressions"),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	{STRUCT_FLD(field_name,		"compress_time"),
	 STRUCT_FLD(field_length,	MY_INT32_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	0),
	 STRUCT_FLD(old_name,		"Total Duration of Compressions,"
		    " in Seconds"),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	{STRUCT_FLD(field_name,		"uncompress_ops"),
	 STRUCT_FLD(field_length,	MY_INT32_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	0),
	 STRUCT_FLD(old_name,		"Total Number of Decompressions"),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	{STRUCT_FLD(field_name,		"uncompress_time"),
	 STRUCT_FLD(field_length,	MY_INT32_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	0),
	 STRUCT_FLD(old_name,		"Total Duration of Decompressions,"
		    " in Seconds"),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	END_OF_ST_FIELD_INFO
};


/*******************************************************************//**
Fill the dynamic table information_schema.innodb_cmp or
innodb_cmp_reset.
@return	0 on success, 1 on failure */
static
int
i_s_cmp_fill_low(
/*=============*/
	THD*		thd,	/*!< in: thread */
	TABLE_LIST*	tables,	/*!< in/out: tables to fill */
	COND*		cond,	/*!< in: condition (ignored) */
	ibool		reset)	/*!< in: TRUE=reset cumulated counts */
{
	TABLE*	table	= (TABLE *) tables->table;
	int	status	= 0;

	DBUG_ENTER("i_s_cmp_fill_low");

	/* deny access to non-superusers */
	if (check_global_access(thd, PROCESS_ACL)) {

		DBUG_RETURN(0);
	}

	RETURN_IF_INNODB_NOT_STARTED(tables->schema_table_name);

	for (uint i = 0; i < PAGE_ZIP_NUM_SSIZE - 1; i++) {
		page_zip_stat_t*	zip_stat = &page_zip_stat[i];

		table->field[0]->store(PAGE_ZIP_MIN_SIZE << i);

		/* The cumulated counts are not protected by any
		mutex.  Thus, some operation in page0zip.c could
		increment a counter between the time we read it and
		clear it.  We could introduce mutex protection, but it
		could cause a measureable performance hit in
		page0zip.c. */
		table->field[1]->store(zip_stat->compressed);
		table->field[2]->store(zip_stat->compressed_ok);
		table->field[3]->store(
			(ulong) (zip_stat->compressed_usec / 1000000));
		table->field[4]->store(zip_stat->decompressed);
		table->field[5]->store(
			(ulong) (zip_stat->decompressed_usec / 1000000));

		if (reset) {
			memset(zip_stat, 0, sizeof *zip_stat);
		}

		if (schema_table_store_record(thd, table)) {
			status = 1;
			break;
		}
	}

	DBUG_RETURN(status);
}

/*******************************************************************//**
Fill the dynamic table information_schema.innodb_cmp.
@return	0 on success, 1 on failure */
static
int
i_s_cmp_fill(
/*=========*/
	THD*		thd,	/*!< in: thread */
	TABLE_LIST*	tables,	/*!< in/out: tables to fill */
	COND*		cond)	/*!< in: condition (ignored) */
{
	return(i_s_cmp_fill_low(thd, tables, cond, FALSE));
}

/*******************************************************************//**
Fill the dynamic table information_schema.innodb_cmp_reset.
@return	0 on success, 1 on failure */
static
int
i_s_cmp_reset_fill(
/*===============*/
	THD*		thd,	/*!< in: thread */
	TABLE_LIST*	tables,	/*!< in/out: tables to fill */
	COND*		cond)	/*!< in: condition (ignored) */
{
	return(i_s_cmp_fill_low(thd, tables, cond, TRUE));
}

/*******************************************************************//**
Bind the dynamic table information_schema.innodb_cmp.
@return	0 on success */
static
int
i_s_cmp_init(
/*=========*/
	void*	p)	/*!< in/out: table schema object */
{
	DBUG_ENTER("i_s_cmp_init");
	ST_SCHEMA_TABLE* schema = (ST_SCHEMA_TABLE*) p;

	schema->fields_info = i_s_cmp_fields_info;
	schema->fill_table = i_s_cmp_fill;

	DBUG_RETURN(0);
}

/*******************************************************************//**
Bind the dynamic table information_schema.innodb_cmp_reset.
@return	0 on success */
static
int
i_s_cmp_reset_init(
/*===============*/
	void*	p)	/*!< in/out: table schema object */
{
	DBUG_ENTER("i_s_cmp_reset_init");
	ST_SCHEMA_TABLE* schema = (ST_SCHEMA_TABLE*) p;

	schema->fields_info = i_s_cmp_fields_info;
	schema->fill_table = i_s_cmp_reset_fill;

	DBUG_RETURN(0);
}

UNIV_INTERN struct st_mysql_plugin	i_s_innodb_cmp =
{
	/* the plugin type (a MYSQL_XXX_PLUGIN value) */
	/* int */
	STRUCT_FLD(type, MYSQL_INFORMATION_SCHEMA_PLUGIN),

	/* pointer to type-specific plugin descriptor */
	/* void* */
	STRUCT_FLD(info, &i_s_info),

	/* plugin name */
	/* const char* */
	STRUCT_FLD(name, "INNODB_CMP"),

	/* plugin author (for SHOW PLUGINS) */
	/* const char* */
	STRUCT_FLD(author, plugin_author),

	/* general descriptive text (for SHOW PLUGINS) */
	/* const char* */
	STRUCT_FLD(descr, "Statistics for the InnoDB compression"),

	/* the plugin license (PLUGIN_LICENSE_XXX) */
	/* int */
	STRUCT_FLD(license, PLUGIN_LICENSE_GPL),

	/* the function to invoke when plugin is loaded */
	/* int (*)(void*); */
	STRUCT_FLD(init, i_s_cmp_init),

	/* the function to invoke when plugin is unloaded */
	/* int (*)(void*); */
	STRUCT_FLD(deinit, i_s_common_deinit),

	/* plugin version (for SHOW PLUGINS) */
	/* unsigned int */
	STRUCT_FLD(version, INNODB_VERSION_SHORT),

	/* struct st_mysql_show_var* */
	STRUCT_FLD(status_vars, NULL),

	/* struct st_mysql_sys_var** */
	STRUCT_FLD(system_vars, NULL),

	/* reserved for dependency checking */
	/* void* */
	STRUCT_FLD(__reserved1, NULL),

	/* Plugin flags */
	/* unsigned long */
	STRUCT_FLD(flags, 0UL),
};

UNIV_INTERN struct st_mysql_plugin	i_s_innodb_cmp_reset =
{
	/* the plugin type (a MYSQL_XXX_PLUGIN value) */
	/* int */
	STRUCT_FLD(type, MYSQL_INFORMATION_SCHEMA_PLUGIN),

	/* pointer to type-specific plugin descriptor */
	/* void* */
	STRUCT_FLD(info, &i_s_info),

	/* plugin name */
	/* const char* */
	STRUCT_FLD(name, "INNODB_CMP_RESET"),

	/* plugin author (for SHOW PLUGINS) */
	/* const char* */
	STRUCT_FLD(author, plugin_author),

	/* general descriptive text (for SHOW PLUGINS) */
	/* const char* */
	STRUCT_FLD(descr, "Statistics for the InnoDB compression;"
		   " reset cumulated counts"),

	/* the plugin license (PLUGIN_LICENSE_XXX) */
	/* int */
	STRUCT_FLD(license, PLUGIN_LICENSE_GPL),

	/* the function to invoke when plugin is loaded */
	/* int (*)(void*); */
	STRUCT_FLD(init, i_s_cmp_reset_init),

	/* the function to invoke when plugin is unloaded */
	/* int (*)(void*); */
	STRUCT_FLD(deinit, i_s_common_deinit),

	/* plugin version (for SHOW PLUGINS) */
	/* unsigned int */
	STRUCT_FLD(version, INNODB_VERSION_SHORT),

	/* struct st_mysql_show_var* */
	STRUCT_FLD(status_vars, NULL),

	/* struct st_mysql_sys_var** */
	STRUCT_FLD(system_vars, NULL),

	/* reserved for dependency checking */
	/* void* */
	STRUCT_FLD(__reserved1, NULL),

	/* Plugin flags */
	/* unsigned long */
	STRUCT_FLD(flags, 0UL),
};

/* Fields of the dynamic table information_schema.innodb_cmpmem. */
static ST_FIELD_INFO	i_s_cmpmem_fields_info[] =
{
	{STRUCT_FLD(field_name,		"page_size"),
	 STRUCT_FLD(field_length,	5),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	0),
	 STRUCT_FLD(old_name,		"Buddy Block Size"),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	{STRUCT_FLD(field_name,		"buffer_pool_instance"),
	STRUCT_FLD(field_length,	MY_INT32_NUM_DECIMAL_DIGITS),
	STRUCT_FLD(field_type,		MYSQL_TYPE_LONG),
	STRUCT_FLD(value,		0),
	STRUCT_FLD(field_flags,		0),
	STRUCT_FLD(old_name,		"Buffer Pool Id"),
	STRUCT_FLD(open_method,		SKIP_OPEN_TABLE)},

	{STRUCT_FLD(field_name,		"pages_used"),
	 STRUCT_FLD(field_length,	MY_INT32_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	0),
	 STRUCT_FLD(old_name,		"Currently in Use"),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	{STRUCT_FLD(field_name,		"pages_free"),
	 STRUCT_FLD(field_length,	MY_INT32_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	0),
	 STRUCT_FLD(old_name,		"Currently Available"),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	{STRUCT_FLD(field_name,		"relocation_ops"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	0),
	 STRUCT_FLD(old_name,		"Total Number of Relocations"),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	{STRUCT_FLD(field_name,		"relocation_time"),
	 STRUCT_FLD(field_length,	MY_INT32_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	0),
	 STRUCT_FLD(old_name,		"Total Duration of Relocations,"
		    			" in Seconds"),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	END_OF_ST_FIELD_INFO
};

/*******************************************************************//**
Fill the dynamic table information_schema.innodb_cmpmem or
innodb_cmpmem_reset.
@return	0 on success, 1 on failure */
static
int
i_s_cmpmem_fill_low(
/*================*/
	THD*		thd,	/*!< in: thread */
	TABLE_LIST*	tables,	/*!< in/out: tables to fill */
	COND*		cond,	/*!< in: condition (ignored) */
	ibool		reset)	/*!< in: TRUE=reset cumulated counts */
{
	int		status = 0;
	TABLE*	table	= (TABLE *) tables->table;

	DBUG_ENTER("i_s_cmpmem_fill_low");

	/* deny access to non-superusers */
	if (check_global_access(thd, PROCESS_ACL)) {

		DBUG_RETURN(0);
	}

	RETURN_IF_INNODB_NOT_STARTED(tables->schema_table_name);

	for (ulint i = 0; i < srv_buf_pool_instances; i++) {
		buf_pool_t*	buf_pool;

		status	= 0;

		buf_pool = buf_pool_from_array(i);

		mutex_enter(&buf_pool->zip_free_mutex);

		for (uint x = 0; x <= BUF_BUDDY_SIZES; x++) {
			buf_buddy_stat_t*	buddy_stat;

			buddy_stat = &buf_pool->buddy_stat[x];

			table->field[0]->store(BUF_BUDDY_LOW << x);
			table->field[1]->store(i);
			table->field[2]->store(buddy_stat->used);
			table->field[3]->store(UNIV_LIKELY(x < BUF_BUDDY_SIZES)
				? UT_LIST_GET_LEN(buf_pool->zip_free[x])
				: 0);
			table->field[4]->store((longlong)
			buddy_stat->relocated, true);
			table->field[5]->store(
				(ulong) (buddy_stat->relocated_usec / 1000000));

			if (reset) {
				/* This is protected by buf_pool->mutex. */
				buddy_stat->relocated = 0;
				buddy_stat->relocated_usec = 0;
			}

			if (schema_table_store_record(thd, table)) {
				status = 1;
				break;
			}
		}

		mutex_exit(&buf_pool->zip_free_mutex);

		if (status) {
			break;
		}
	}

	DBUG_RETURN(status);
}

/*******************************************************************//**
Fill the dynamic table information_schema.innodb_cmpmem.
@return	0 on success, 1 on failure */
static
int
i_s_cmpmem_fill(
/*============*/
	THD*		thd,	/*!< in: thread */
	TABLE_LIST*	tables,	/*!< in/out: tables to fill */
	COND*		cond)	/*!< in: condition (ignored) */
{
	return(i_s_cmpmem_fill_low(thd, tables, cond, FALSE));
}

/*******************************************************************//**
Fill the dynamic table information_schema.innodb_cmpmem_reset.
@return	0 on success, 1 on failure */
static
int
i_s_cmpmem_reset_fill(
/*==================*/
	THD*		thd,	/*!< in: thread */
	TABLE_LIST*	tables,	/*!< in/out: tables to fill */
	COND*		cond)	/*!< in: condition (ignored) */
{
	return(i_s_cmpmem_fill_low(thd, tables, cond, TRUE));
}

/*******************************************************************//**
Bind the dynamic table information_schema.innodb_cmpmem.
@return	0 on success */
static
int
i_s_cmpmem_init(
/*============*/
	void*	p)	/*!< in/out: table schema object */
{
	DBUG_ENTER("i_s_cmpmem_init");
	ST_SCHEMA_TABLE* schema = (ST_SCHEMA_TABLE*) p;

	schema->fields_info = i_s_cmpmem_fields_info;
	schema->fill_table = i_s_cmpmem_fill;

	DBUG_RETURN(0);
}

/*******************************************************************//**
Bind the dynamic table information_schema.innodb_cmpmem_reset.
@return	0 on success */
static
int
i_s_cmpmem_reset_init(
/*==================*/
	void*	p)	/*!< in/out: table schema object */
{
	DBUG_ENTER("i_s_cmpmem_reset_init");
	ST_SCHEMA_TABLE* schema = (ST_SCHEMA_TABLE*) p;

	schema->fields_info = i_s_cmpmem_fields_info;
	schema->fill_table = i_s_cmpmem_reset_fill;

	DBUG_RETURN(0);
}

UNIV_INTERN struct st_mysql_plugin	i_s_innodb_cmpmem =
{
	/* the plugin type (a MYSQL_XXX_PLUGIN value) */
	/* int */
	STRUCT_FLD(type, MYSQL_INFORMATION_SCHEMA_PLUGIN),

	/* pointer to type-specific plugin descriptor */
	/* void* */
	STRUCT_FLD(info, &i_s_info),

	/* plugin name */
	/* const char* */
	STRUCT_FLD(name, "INNODB_CMPMEM"),

	/* plugin author (for SHOW PLUGINS) */
	/* const char* */
	STRUCT_FLD(author, plugin_author),

	/* general descriptive text (for SHOW PLUGINS) */
	/* const char* */
	STRUCT_FLD(descr, "Statistics for the InnoDB compressed buffer pool"),

	/* the plugin license (PLUGIN_LICENSE_XXX) */
	/* int */
	STRUCT_FLD(license, PLUGIN_LICENSE_GPL),

	/* the function to invoke when plugin is loaded */
	/* int (*)(void*); */
	STRUCT_FLD(init, i_s_cmpmem_init),

	/* the function to invoke when plugin is unloaded */
	/* int (*)(void*); */
	STRUCT_FLD(deinit, i_s_common_deinit),

	/* plugin version (for SHOW PLUGINS) */
	/* unsigned int */
	STRUCT_FLD(version, INNODB_VERSION_SHORT),

	/* struct st_mysql_show_var* */
	STRUCT_FLD(status_vars, NULL),

	/* struct st_mysql_sys_var** */
	STRUCT_FLD(system_vars, NULL),

	/* reserved for dependency checking */
	/* void* */
	STRUCT_FLD(__reserved1, NULL),

	/* Plugin flags */
	/* unsigned long */
	STRUCT_FLD(flags, 0UL),
};

UNIV_INTERN struct st_mysql_plugin	i_s_innodb_cmpmem_reset =
{
	/* the plugin type (a MYSQL_XXX_PLUGIN value) */
	/* int */
	STRUCT_FLD(type, MYSQL_INFORMATION_SCHEMA_PLUGIN),

	/* pointer to type-specific plugin descriptor */
	/* void* */
	STRUCT_FLD(info, &i_s_info),

	/* plugin name */
	/* const char* */
	STRUCT_FLD(name, "INNODB_CMPMEM_RESET"),

	/* plugin author (for SHOW PLUGINS) */
	/* const char* */
	STRUCT_FLD(author, plugin_author),

	/* general descriptive text (for SHOW PLUGINS) */
	/* const char* */
	STRUCT_FLD(descr, "Statistics for the InnoDB compressed buffer pool;"
		   " reset cumulated counts"),

	/* the plugin license (PLUGIN_LICENSE_XXX) */
	/* int */
	STRUCT_FLD(license, PLUGIN_LICENSE_GPL),

	/* the function to invoke when plugin is loaded */
	/* int (*)(void*); */
	STRUCT_FLD(init, i_s_cmpmem_reset_init),

	/* the function to invoke when plugin is unloaded */
	/* int (*)(void*); */
	STRUCT_FLD(deinit, i_s_common_deinit),

	/* plugin version (for SHOW PLUGINS) */
	/* unsigned int */
	STRUCT_FLD(version, INNODB_VERSION_SHORT),

	/* struct st_mysql_show_var* */
	STRUCT_FLD(status_vars, NULL),

	/* struct st_mysql_sys_var** */
	STRUCT_FLD(system_vars, NULL),

	/* reserved for dependency checking */
	/* void* */
	STRUCT_FLD(__reserved1, NULL),

	/* Plugin flags */
	/* unsigned long */
	STRUCT_FLD(flags, 0UL),
};

/* Fields of the dynamic table INNODB_BUFFER_POOL_STATS. */
static ST_FIELD_INFO	i_s_innodb_buffer_stats_fields_info[] =
{
#define IDX_BUF_STATS_POOL_ID		0
	{STRUCT_FLD(field_name,		"POOL_ID"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define IDX_BUF_STATS_POOL_SIZE		1
	{STRUCT_FLD(field_name,		"POOL_SIZE"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define IDX_BUF_STATS_FREE_BUFFERS	2
	{STRUCT_FLD(field_name,		"FREE_BUFFERS"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define IDX_BUF_STATS_LRU_LEN		3
	{STRUCT_FLD(field_name,		"DATABASE_PAGES"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define IDX_BUF_STATS_OLD_LRU_LEN	4
	{STRUCT_FLD(field_name,		"OLD_DATABASE_PAGES"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define IDX_BUF_STATS_FLUSH_LIST_LEN	5
	{STRUCT_FLD(field_name,		"MODIFIED_DATABASE_PAGES"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define IDX_BUF_STATS_PENDING_ZIP	6
	{STRUCT_FLD(field_name,		"PENDING_DECOMPRESS"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define IDX_BUF_STATS_PENDING_READ	7
	{STRUCT_FLD(field_name,		"PENDING_READS"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define IDX_BUF_STATS_FLUSH_LRU		8
	{STRUCT_FLD(field_name,		"PENDING_FLUSH_LRU"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define IDX_BUF_STATS_FLUSH_LIST	9
	{STRUCT_FLD(field_name,		"PENDING_FLUSH_LIST"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define IDX_BUF_STATS_PAGE_YOUNG	10
	{STRUCT_FLD(field_name,		"PAGES_MADE_YOUNG"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define IDX_BUF_STATS_PAGE_NOT_YOUNG	11
	{STRUCT_FLD(field_name,		"PAGES_NOT_MADE_YOUNG"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define	IDX_BUF_STATS_PAGE_YOUNG_RATE	12
	{STRUCT_FLD(field_name,		"PAGES_MADE_YOUNG_RATE"),
	 STRUCT_FLD(field_length,	MAX_FLOAT_STR_LENGTH),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_FLOAT),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	0),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define	IDX_BUF_STATS_PAGE_NOT_YOUNG_RATE 13
	{STRUCT_FLD(field_name,		"PAGES_MADE_NOT_YOUNG_RATE"),
	 STRUCT_FLD(field_length,	MAX_FLOAT_STR_LENGTH),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_FLOAT),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	0),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define IDX_BUF_STATS_PAGE_READ		14
	{STRUCT_FLD(field_name,		"NUMBER_PAGES_READ"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define IDX_BUF_STATS_PAGE_CREATED	15
	{STRUCT_FLD(field_name,		"NUMBER_PAGES_CREATED"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define IDX_BUF_STATS_PAGE_WRITTEN	16
	{STRUCT_FLD(field_name,		"NUMBER_PAGES_WRITTEN"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define	IDX_BUF_STATS_PAGE_READ_RATE	17
	{STRUCT_FLD(field_name,		"PAGES_READ_RATE"),
	 STRUCT_FLD(field_length,	MAX_FLOAT_STR_LENGTH),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_FLOAT),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	0),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define	IDX_BUF_STATS_PAGE_CREATE_RATE	18
	{STRUCT_FLD(field_name,		"PAGES_CREATE_RATE"),
	 STRUCT_FLD(field_length,	MAX_FLOAT_STR_LENGTH),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_FLOAT),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	0),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define	IDX_BUF_STATS_PAGE_WRITTEN_RATE	19
	{STRUCT_FLD(field_name,		"PAGES_WRITTEN_RATE"),
	 STRUCT_FLD(field_length,	MAX_FLOAT_STR_LENGTH),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_FLOAT),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	0),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define IDX_BUF_STATS_GET		20
	{STRUCT_FLD(field_name,		"NUMBER_PAGES_GET"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define IDX_BUF_STATS_HIT_RATE		21
	{STRUCT_FLD(field_name,		"HIT_RATE"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define IDX_BUF_STATS_MADE_YOUNG_PCT	22
	{STRUCT_FLD(field_name,		"YOUNG_MAKE_PER_THOUSAND_GETS"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define IDX_BUF_STATS_NOT_MADE_YOUNG_PCT 23
	{STRUCT_FLD(field_name,		"NOT_YOUNG_MAKE_PER_THOUSAND_GETS"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define IDX_BUF_STATS_READ_AHREAD	24
	{STRUCT_FLD(field_name,		"NUMBER_PAGES_READ_AHEAD"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define IDX_BUF_STATS_READ_AHEAD_EVICTED 25
	{STRUCT_FLD(field_name,		"NUMBER_READ_AHEAD_EVICTED"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define	IDX_BUF_STATS_READ_AHEAD_RATE	26
	{STRUCT_FLD(field_name,		"READ_AHEAD_RATE"),
	 STRUCT_FLD(field_length,	MAX_FLOAT_STR_LENGTH),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_FLOAT),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	0),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define	IDX_BUF_STATS_READ_AHEAD_EVICT_RATE 27
	{STRUCT_FLD(field_name,		"READ_AHEAD_EVICTED_RATE"),
	 STRUCT_FLD(field_length,	MAX_FLOAT_STR_LENGTH),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_FLOAT),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	0),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define IDX_BUF_STATS_LRU_IO_SUM	28
	{STRUCT_FLD(field_name,		"LRU_IO_TOTAL"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define IDX_BUF_STATS_LRU_IO_CUR	29
	{STRUCT_FLD(field_name,		"LRU_IO_CURRENT"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define IDX_BUF_STATS_UNZIP_SUM		30
	{STRUCT_FLD(field_name,		"UNCOMPRESS_TOTAL"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define IDX_BUF_STATS_UNZIP_CUR		31
	{STRUCT_FLD(field_name,		"UNCOMPRESS_CURRENT"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	END_OF_ST_FIELD_INFO
};

/*******************************************************************//**
Fill Information Schema table INNODB_BUFFER_POOL_STATS for a particular
buffer pool
@return	0 on success, 1 on failure */
static
int
i_s_innodb_stats_fill(
/*==================*/
	THD*			thd,		/*!< in: thread */
	TABLE_LIST*		tables,		/*!< in/out: tables to fill */
	const buf_pool_info_t*	info)		/*!< in: buffer pool
						information */
{
	TABLE*			table;
	Field**			fields;

	DBUG_ENTER("i_s_innodb_stats_fill");

	table = tables->table;

	fields = table->field;

	OK(fields[IDX_BUF_STATS_POOL_ID]->store(info->pool_unique_id));

	OK(fields[IDX_BUF_STATS_POOL_SIZE]->store(info->pool_size));

	OK(fields[IDX_BUF_STATS_LRU_LEN]->store(info->lru_len));

	OK(fields[IDX_BUF_STATS_OLD_LRU_LEN]->store(info->old_lru_len));

	OK(fields[IDX_BUF_STATS_FREE_BUFFERS]->store(info->free_list_len));

	OK(fields[IDX_BUF_STATS_FLUSH_LIST_LEN]->store(
		info->flush_list_len));

	OK(fields[IDX_BUF_STATS_PENDING_ZIP]->store(info->n_pend_unzip));

	OK(fields[IDX_BUF_STATS_PENDING_READ]->store(info->n_pend_reads));

	OK(fields[IDX_BUF_STATS_FLUSH_LRU]->store(info->n_pending_flush_lru));

	OK(fields[IDX_BUF_STATS_FLUSH_LIST]->store(info->n_pending_flush_list));

	OK(fields[IDX_BUF_STATS_PAGE_YOUNG]->store(info->n_pages_made_young));

	OK(fields[IDX_BUF_STATS_PAGE_NOT_YOUNG]->store(
		info->n_pages_not_made_young));

	OK(fields[IDX_BUF_STATS_PAGE_YOUNG_RATE]->store(
		info->page_made_young_rate));

	OK(fields[IDX_BUF_STATS_PAGE_NOT_YOUNG_RATE]->store(
		info->page_not_made_young_rate));

	OK(fields[IDX_BUF_STATS_PAGE_READ]->store(info->n_pages_read));

	OK(fields[IDX_BUF_STATS_PAGE_CREATED]->store(info->n_pages_created));

	OK(fields[IDX_BUF_STATS_PAGE_WRITTEN]->store(info->n_pages_written));

	OK(fields[IDX_BUF_STATS_GET]->store(info->n_page_gets));

	OK(fields[IDX_BUF_STATS_PAGE_READ_RATE]->store(info->pages_read_rate));

	OK(fields[IDX_BUF_STATS_PAGE_CREATE_RATE]->store(info->pages_created_rate));

	OK(fields[IDX_BUF_STATS_PAGE_WRITTEN_RATE]->store(info->pages_written_rate));

	if (info->n_page_get_delta) {
		OK(fields[IDX_BUF_STATS_HIT_RATE]->store(
			1000 - (1000 * info->page_read_delta
				/ info->n_page_get_delta)));

		OK(fields[IDX_BUF_STATS_MADE_YOUNG_PCT]->store(
			1000 * info->young_making_delta
			/ info->n_page_get_delta));

		OK(fields[IDX_BUF_STATS_NOT_MADE_YOUNG_PCT]->store(
			1000 * info->not_young_making_delta
			/ info->n_page_get_delta));
	} else {
		OK(fields[IDX_BUF_STATS_HIT_RATE]->store(0));
		OK(fields[IDX_BUF_STATS_MADE_YOUNG_PCT]->store(0));
		OK(fields[IDX_BUF_STATS_NOT_MADE_YOUNG_PCT]->store(0));
	}

	OK(fields[IDX_BUF_STATS_READ_AHREAD]->store(info->n_ra_pages_read));

	OK(fields[IDX_BUF_STATS_READ_AHEAD_EVICTED]->store(
		info->n_ra_pages_evicted));

	OK(fields[IDX_BUF_STATS_READ_AHEAD_RATE]->store(
		info->pages_readahead_rate));

	OK(fields[IDX_BUF_STATS_READ_AHEAD_EVICT_RATE]->store(
		info->pages_evicted_rate));

	OK(fields[IDX_BUF_STATS_LRU_IO_SUM]->store(info->io_sum));

	OK(fields[IDX_BUF_STATS_LRU_IO_CUR]->store(info->io_cur));

	OK(fields[IDX_BUF_STATS_UNZIP_SUM]->store(info->unzip_sum));

	OK(fields[IDX_BUF_STATS_UNZIP_CUR]->store( info->unzip_cur));

	DBUG_RETURN(schema_table_store_record(thd, table));
}

/*******************************************************************//**
This is the function that loops through each buffer pool and fetch buffer
pool stats to information schema  table: I_S_INNODB_BUFFER_POOL_STATS
@return	0 on success, 1 on failure */
static
int
i_s_innodb_buffer_stats_fill_table(
/*===============================*/
	THD*		thd,		/*!< in: thread */
	TABLE_LIST*	tables,		/*!< in/out: tables to fill */
	Item*		)		/*!< in: condition (ignored) */
{
	int			status	= 0;
	buf_pool_info_t*	pool_info;

	DBUG_ENTER("i_s_innodb_buffer_fill_general");

	/* Only allow the PROCESS privilege holder to access the stats */
	if (check_global_access(thd, PROCESS_ACL)) {
		DBUG_RETURN(0);
	}

	pool_info = (buf_pool_info_t*) mem_zalloc(
		srv_buf_pool_instances *  sizeof *pool_info);

	/* Walk through each buffer pool */
	for (ulint i = 0; i < srv_buf_pool_instances; i++) {
		buf_pool_t*		buf_pool;

		buf_pool = buf_pool_from_array(i);

		/* Fetch individual buffer pool info */
		buf_stats_get_pool_info(buf_pool, i, pool_info);

		status = i_s_innodb_stats_fill(thd, tables, &pool_info[i]);

		/* If something goes wrong, break and return */
		if (status) {
			break;
		}
	}

	mem_free(pool_info);

	DBUG_RETURN(status);
}

/*******************************************************************//**
Bind the dynamic table INFORMATION_SCHEMA.INNODB_BUFFER_POOL_STATS.
@return	0 on success, 1 on failure */
static
int
i_s_innodb_buffer_pool_stats_init(
/*==============================*/
	void*	p)	/*!< in/out: table schema object */
{
	ST_SCHEMA_TABLE*	schema;

	DBUG_ENTER("i_s_innodb_buffer_pool_stats_init");

	schema = reinterpret_cast<ST_SCHEMA_TABLE*>(p);

	schema->fields_info = i_s_innodb_buffer_stats_fields_info;
	schema->fill_table = i_s_innodb_buffer_stats_fill_table;

	DBUG_RETURN(0);
}

UNIV_INTERN struct st_mysql_plugin	i_s_innodb_buffer_stats =
{
	/* the plugin type (a MYSQL_XXX_PLUGIN value) */
	/* int */
	STRUCT_FLD(type, MYSQL_INFORMATION_SCHEMA_PLUGIN),

	/* pointer to type-specific plugin descriptor */
	/* void* */
	STRUCT_FLD(info, &i_s_info),

	/* plugin name */
	/* const char* */
	STRUCT_FLD(name, "INNODB_BUFFER_POOL_STATS"),

	/* plugin author (for SHOW PLUGINS) */
	/* const char* */
	STRUCT_FLD(author, plugin_author),

	/* general descriptive text (for SHOW PLUGINS) */
	/* const char* */
	STRUCT_FLD(descr, "InnoDB Buffer Pool Statistics Information "),

	/* the plugin license (PLUGIN_LICENSE_XXX) */
	/* int */
	STRUCT_FLD(license, PLUGIN_LICENSE_GPL),

	/* the function to invoke when plugin is loaded */
	/* int (*)(void*); */
	STRUCT_FLD(init, i_s_innodb_buffer_pool_stats_init),

	/* the function to invoke when plugin is unloaded */
	/* int (*)(void*); */
	STRUCT_FLD(deinit, i_s_common_deinit),

	/* plugin version (for SHOW PLUGINS) */
	/* unsigned int */
	STRUCT_FLD(version, INNODB_VERSION_SHORT),

	/* struct st_mysql_show_var* */
	STRUCT_FLD(status_vars, NULL),

	/* struct st_mysql_sys_var** */
	STRUCT_FLD(system_vars, NULL),

	/* reserved for dependency checking */
	/* void* */
	STRUCT_FLD(__reserved1, NULL),

	/* Plugin flags */
	/* unsigned long */
	STRUCT_FLD(flags, 0UL),
};

/* Fields of the dynamic table INNODB_BUFFER_POOL_PAGE. */
static ST_FIELD_INFO	i_s_innodb_buffer_page_fields_info[] =
{
#define IDX_BUFFER_POOL_ID		0
	{STRUCT_FLD(field_name,		"POOL_ID"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define IDX_BUFFER_BLOCK_ID		1
	{STRUCT_FLD(field_name,		"BLOCK_ID"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define IDX_BUFFER_PAGE_SPACE		2
	{STRUCT_FLD(field_name,		"SPACE"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define IDX_BUFFER_PAGE_NUM		3
	{STRUCT_FLD(field_name,		"PAGE_NUMBER"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define IDX_BUFFER_PAGE_TYPE		4
	{STRUCT_FLD(field_name,		"PAGE_TYPE"),
	 STRUCT_FLD(field_length,	64),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_STRING),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_MAYBE_NULL),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define IDX_BUFFER_PAGE_FLUSH_TYPE	5
	{STRUCT_FLD(field_name,		"FLUSH_TYPE"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define IDX_BUFFER_PAGE_FIX_COUNT	6
	{STRUCT_FLD(field_name,		"FIX_COUNT"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define IDX_BUFFER_PAGE_HASHED		7
	{STRUCT_FLD(field_name,		"IS_HASHED"),
	 STRUCT_FLD(field_length,	3),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_STRING),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_MAYBE_NULL),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define IDX_BUFFER_PAGE_NEWEST_MOD	8
	{STRUCT_FLD(field_name,		"NEWEST_MODIFICATION"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define IDX_BUFFER_PAGE_OLDEST_MOD	9
	{STRUCT_FLD(field_name,		"OLDEST_MODIFICATION"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define IDX_BUFFER_PAGE_ACCESS_TIME	10
	{STRUCT_FLD(field_name,		"ACCESS_TIME"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define IDX_BUFFER_PAGE_TABLE_NAME	11
	{STRUCT_FLD(field_name,		"TABLE_NAME"),
	 STRUCT_FLD(field_length,	1024),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_STRING),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_MAYBE_NULL),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define IDX_BUFFER_PAGE_INDEX_NAME	12
	{STRUCT_FLD(field_name,		"INDEX_NAME"),
	 STRUCT_FLD(field_length,	1024),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_STRING),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_MAYBE_NULL),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define IDX_BUFFER_PAGE_NUM_RECS	13
	{STRUCT_FLD(field_name,		"NUMBER_RECORDS"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define IDX_BUFFER_PAGE_DATA_SIZE	14
	{STRUCT_FLD(field_name,		"DATA_SIZE"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define IDX_BUFFER_PAGE_ZIP_SIZE	15
	{STRUCT_FLD(field_name,		"COMPRESSED_SIZE"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define IDX_BUFFER_PAGE_STATE		16
	{STRUCT_FLD(field_name,		"PAGE_STATE"),
	 STRUCT_FLD(field_length,	64),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_STRING),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_MAYBE_NULL),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define IDX_BUFFER_PAGE_IO_FIX		17
	{STRUCT_FLD(field_name,		"IO_FIX"),
	 STRUCT_FLD(field_length,	64),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_STRING),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_MAYBE_NULL),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define IDX_BUFFER_PAGE_IS_OLD		18
	{STRUCT_FLD(field_name,		"IS_OLD"),
	 STRUCT_FLD(field_length,	3),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_STRING),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_MAYBE_NULL),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define IDX_BUFFER_PAGE_FREE_CLOCK	19
	{STRUCT_FLD(field_name,		"FREE_PAGE_CLOCK"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	END_OF_ST_FIELD_INFO
};

/*******************************************************************//**
Fill Information Schema table INNODB_BUFFER_PAGE with information
cached in the buf_page_info_t array
@return	0 on success, 1 on failure */
static
int
i_s_innodb_buffer_page_fill(
/*========================*/
	THD*			thd,		/*!< in: thread */
	TABLE_LIST*		tables,		/*!< in/out: tables to fill */
	const buf_page_info_t*	info_array,	/*!< in: array cached page
						info */
	ulint			num_page,	/*!< in: number of page info
						 cached */
	mem_heap_t*		heap)		/*!< in: temp heap memory */
{
	TABLE*			table;
	Field**			fields;

	DBUG_ENTER("i_s_innodb_buffer_page_fill");

	table = tables->table;

	fields = table->field;

	/* Iterate through the cached array and fill the I_S table rows */
	for (ulint i = 0; i < num_page; i++) {
		const buf_page_info_t*	page_info;
		const char*		table_name;
		const char*		index_name;
		const char*		state_str;
		enum buf_page_state	state;

		page_info = info_array + i;

		table_name = NULL;
		index_name = NULL;
		state_str = NULL;

		OK(fields[IDX_BUFFER_POOL_ID]->store(page_info->pool_id));

		OK(fields[IDX_BUFFER_BLOCK_ID]->store(page_info->block_id));

		OK(fields[IDX_BUFFER_PAGE_SPACE]->store(page_info->space_id));

		OK(fields[IDX_BUFFER_PAGE_NUM]->store(page_info->page_num));

		OK(field_store_string(
			fields[IDX_BUFFER_PAGE_TYPE],
			i_s_page_type[page_info->page_type].type_str));

		OK(fields[IDX_BUFFER_PAGE_FLUSH_TYPE]->store(
			page_info->flush_type));

		OK(fields[IDX_BUFFER_PAGE_FIX_COUNT]->store(
			page_info->fix_count));

		if (page_info->hashed) {
			OK(field_store_string(
				fields[IDX_BUFFER_PAGE_HASHED], "YES"));
		} else {
			OK(field_store_string(
				fields[IDX_BUFFER_PAGE_HASHED], "NO"));
		}

		OK(fields[IDX_BUFFER_PAGE_NEWEST_MOD]->store(
			(longlong) page_info->newest_mod, true));

		OK(fields[IDX_BUFFER_PAGE_OLDEST_MOD]->store(
			(longlong) page_info->oldest_mod, true));

		OK(fields[IDX_BUFFER_PAGE_ACCESS_TIME]->store(
			page_info->access_time));

		/* If this is an index page, fetch the index name
		and table name */
		if (page_info->page_type == I_S_PAGE_TYPE_INDEX) {
			const dict_index_t*	index;

			mutex_enter(&dict_sys->mutex);
			index = dict_index_get_if_in_cache_low(
				page_info->index_id);

			/* Copy the index/table name under mutex. We
			do not want to hold the InnoDB mutex while
			filling the IS table */
			if (index) {
				const char*	name_ptr = index->name;

				if (name_ptr[0] == TEMP_INDEX_PREFIX) {
					name_ptr++;
				}

				index_name = mem_heap_strdup(heap, name_ptr);

				table_name = mem_heap_strdup(heap,
							     index->table_name);

			}

			mutex_exit(&dict_sys->mutex);
		}

		OK(field_store_string(
			fields[IDX_BUFFER_PAGE_TABLE_NAME], table_name));

		OK(field_store_string(
			fields[IDX_BUFFER_PAGE_INDEX_NAME], index_name));

		OK(fields[IDX_BUFFER_PAGE_NUM_RECS]->store(
			page_info->num_recs));

		OK(fields[IDX_BUFFER_PAGE_DATA_SIZE]->store(
			page_info->data_size));

		OK(fields[IDX_BUFFER_PAGE_ZIP_SIZE]->store(
			page_info->zip_ssize
			? (PAGE_ZIP_MIN_SIZE >> 1) << page_info->zip_ssize
			: 0));

#if BUF_PAGE_STATE_BITS > 3
# error "BUF_PAGE_STATE_BITS > 3, please ensure that all 1<<BUF_PAGE_STATE_BITS values are checked for"
#endif
		state = static_cast<enum buf_page_state>(page_info->page_state);

		switch (state) {
		/* First three states are for compression pages and
		are not states we would get as we scan pages through
		buffer blocks */
		case BUF_BLOCK_ZIP_FREE:
		case BUF_BLOCK_ZIP_PAGE:
		case BUF_BLOCK_ZIP_DIRTY:
			state_str = NULL;
			break;
		case BUF_BLOCK_NOT_USED:
			state_str = "NOT_USED";
			break;
		case BUF_BLOCK_READY_FOR_USE:
			state_str = "READY_FOR_USE";
			break;
		case BUF_BLOCK_FILE_PAGE:
			state_str = "FILE_PAGE";
			break;
		case BUF_BLOCK_MEMORY:
			state_str = "MEMORY";
			break;
		case BUF_BLOCK_REMOVE_HASH:
			state_str = "REMOVE_HASH";
			break;
		};

		OK(field_store_string(fields[IDX_BUFFER_PAGE_STATE],
				      state_str));

		switch (page_info->io_fix) {
		case BUF_IO_NONE:
			OK(field_store_string(fields[IDX_BUFFER_PAGE_IO_FIX],
					      "IO_NONE"));
			break;
		case BUF_IO_READ:
			OK(field_store_string(fields[IDX_BUFFER_PAGE_IO_FIX],
					      "IO_READ"));
			break;
		case BUF_IO_WRITE:
			OK(field_store_string(fields[IDX_BUFFER_PAGE_IO_FIX],
					      "IO_WRITE"));
			break;
		case BUF_IO_PIN:
			OK(field_store_string(fields[IDX_BUFFER_PAGE_IO_FIX],
					      "IO_PIN"));
			break;
		}

		OK(field_store_string(fields[IDX_BUFFER_PAGE_IS_OLD],
				      (page_info->is_old) ? "YES" : "NO"));

		OK(fields[IDX_BUFFER_PAGE_FREE_CLOCK]->store(
			page_info->freed_page_clock));

		if (schema_table_store_record(thd, table)) {
			DBUG_RETURN(1);
		}
	}

	DBUG_RETURN(0);
}

/*******************************************************************//**
Set appropriate page type to a buf_page_info_t structure */
static
void
i_s_innodb_set_page_type(
/*=====================*/
	buf_page_info_t*page_info,	/*!< in/out: structure to fill with
					scanned info */
	ulint		page_type,	/*!< in: page type */
	const byte*	frame)		/*!< in: buffer frame */
{
	if (page_type == FIL_PAGE_INDEX) {
		const page_t*	page = (const page_t*) frame;

		/* FIL_PAGE_INDEX is a bit special, its value
		is defined as 17855, so we cannot use FIL_PAGE_INDEX
		to index into i_s_page_type[] array, its array index
		in the i_s_page_type[] array is I_S_PAGE_TYPE_INDEX
		(1) */
		page_info->page_type = I_S_PAGE_TYPE_INDEX;

		page_info->index_id = btr_page_get_index_id(page);

		page_info->data_size = (ulint)(page_header_get_field(
			page, PAGE_HEAP_TOP) - (page_is_comp(page)
						? PAGE_NEW_SUPREMUM_END
						: PAGE_OLD_SUPREMUM_END)
			- page_header_get_field(page, PAGE_GARBAGE));

		page_info->num_recs = page_get_n_recs(page);
	} else if (page_type >= I_S_PAGE_TYPE_UNKNOWN) {
		/* Encountered an unknown page type */
		page_info->page_type = I_S_PAGE_TYPE_UNKNOWN;
	} else {
		/* Make sure we get the right index into the
		i_s_page_type[] array */
		ut_a(page_type == i_s_page_type[page_type].type_value);

		page_info->page_type = page_type;
	}

	if (page_info->page_type == FIL_PAGE_TYPE_ZBLOB
	    || page_info->page_type == FIL_PAGE_TYPE_ZBLOB2) {
		page_info->page_num = mach_read_from_4(
			frame + FIL_PAGE_OFFSET);
		page_info->space_id = mach_read_from_4(
			frame + FIL_PAGE_ARCH_LOG_NO_OR_SPACE_ID);
	}
}
/*******************************************************************//**
Scans pages in the buffer cache, and collect their general information
into the buf_page_info_t array which is zero-filled. So any fields
that are not initialized in the function will default to 0 */
static
void
i_s_innodb_buffer_page_get_info(
/*============================*/
	const buf_page_t*bpage,		/*!< in: buffer pool page to scan */
	ulint		pool_id,	/*!< in: buffer pool id */
	ulint		pos,		/*!< in: buffer block position in
					buffer pool or in the LRU list */
	buf_page_info_t*page_info)	/*!< in: zero filled info structure;
					out: structure filled with scanned
					info */
{
	ut_ad(pool_id < MAX_BUFFER_POOLS);

	page_info->pool_id = pool_id;

	page_info->block_id = pos;

	page_info->page_state = buf_page_get_state(bpage);

	/* Only fetch information for buffers that map to a tablespace,
	that is, buffer page with state BUF_BLOCK_ZIP_PAGE,
	BUF_BLOCK_ZIP_DIRTY or BUF_BLOCK_FILE_PAGE */
	if (buf_page_in_file(bpage)) {
		const byte*	frame;
		ulint		page_type;

		page_info->space_id = buf_page_get_space(bpage);

		page_info->page_num = buf_page_get_page_no(bpage);

		page_info->flush_type = bpage->flush_type;

		page_info->fix_count = bpage->buf_fix_count;

		page_info->newest_mod = bpage->newest_modification;

		page_info->oldest_mod = bpage->oldest_modification;

		page_info->access_time = bpage->access_time;

		page_info->zip_ssize = bpage->zip.ssize;

		page_info->io_fix = bpage->io_fix;

		page_info->is_old = bpage->old;

		page_info->freed_page_clock = bpage->freed_page_clock;

		if (page_info->page_state == BUF_BLOCK_FILE_PAGE) {
			const buf_block_t*block;

			block = reinterpret_cast<const buf_block_t*>(bpage);
			frame = block->frame;
			page_info->hashed = (block->index != NULL);
		} else {
			ut_ad(page_info->zip_ssize);
			frame = bpage->zip.data;
		}

		page_type = fil_page_get_type(frame);

		i_s_innodb_set_page_type(page_info, page_type, frame);
	} else {
		page_info->page_type = I_S_PAGE_TYPE_UNKNOWN;
	}
}

/*******************************************************************//**
This is the function that goes through each block of the buffer pool
and fetch information to information schema tables: INNODB_BUFFER_PAGE.
@return	0 on success, 1 on failure */
static
int
i_s_innodb_fill_buffer_pool(
/*========================*/
	THD*			thd,		/*!< in: thread */
	TABLE_LIST*		tables,		/*!< in/out: tables to fill */
	buf_pool_t*		buf_pool,	/*!< in: buffer pool to scan */
	const ulint		pool_id)	/*!< in: buffer pool id */
{
	int			status	= 0;
	mem_heap_t*		heap;

	DBUG_ENTER("i_s_innodb_fill_buffer_pool");

	heap = mem_heap_create(10000);

	/* Go through each chunk of buffer pool. Currently, we only
	have one single chunk for each buffer pool */
	for (ulint n = 0; n < buf_pool->n_chunks; n++) {
		const buf_block_t*	block;
		ulint			n_blocks;
		buf_page_info_t*	info_buffer;
		ulint			num_page;
		ulint			mem_size;
		ulint			chunk_size;
		ulint			num_to_process = 0;
		ulint			block_id = 0;
		mutex_t*		block_mutex;

		/* Get buffer block of the nth chunk */
		block = buf_get_nth_chunk_block(buf_pool, n, &chunk_size);
		num_page = 0;

		while (chunk_size > 0) {
			/* we cache maximum MAX_BUF_INFO_CACHED number of
			buffer page info */
			num_to_process = ut_min(chunk_size,
						MAX_BUF_INFO_CACHED);

			mem_size = num_to_process * sizeof(buf_page_info_t);

			/* For each chunk, we'll pre-allocate information
			structures to cache the page information read from
			the buffer pool. Doing so before obtain any mutex */
			info_buffer = (buf_page_info_t*) mem_heap_zalloc(
				heap, mem_size);

			/* GO through each block in the chunk */
			for (n_blocks = num_to_process; n_blocks--; block++) {
				block_mutex = buf_page_get_mutex_enter(&block->page);
				i_s_innodb_buffer_page_get_info(
					&block->page, pool_id, block_id,
					info_buffer + num_page);
				mutex_exit(block_mutex);
				block_id++;
				num_page++;
			}


			/* Fill in information schema table with information
			just collected from the buffer chunk scan */
			status = i_s_innodb_buffer_page_fill(
				thd, tables, info_buffer,
				num_page, heap);

			/* If something goes wrong, break and return */
			if (status) {
				break;
			}

			mem_heap_empty(heap);
			chunk_size -= num_to_process;
			num_page = 0;
		}
	}

	mem_heap_free(heap);

	DBUG_RETURN(status);
}

/*******************************************************************//**
Fill page information for pages in InnoDB buffer pool to the
dynamic table INFORMATION_SCHEMA.INNODB_BUFFER_PAGE
@return	0 on success, 1 on failure */
static
int
i_s_innodb_buffer_page_fill_table(
/*==============================*/
	THD*		thd,		/*!< in: thread */
	TABLE_LIST*	tables,		/*!< in/out: tables to fill */
	Item*		)		/*!< in: condition (ignored) */
{
	int	status	= 0;

	DBUG_ENTER("i_s_innodb_buffer_page_fill_table");

	/* deny access to user without PROCESS privilege */
	if (check_global_access(thd, PROCESS_ACL)) {
		DBUG_RETURN(0);
	}

	/* Walk through each buffer pool */
	for (ulint i = 0; i < srv_buf_pool_instances; i++) {
		buf_pool_t*	buf_pool;

		buf_pool = buf_pool_from_array(i);

		/* Fetch information from pages in this buffer pool,
		and fill the corresponding I_S table */
		status = i_s_innodb_fill_buffer_pool(thd, tables, buf_pool, i);

		/* If something wrong, break and return */
		if (status) {
			break;
		}
	}

	DBUG_RETURN(status);
}

/*******************************************************************//**
Bind the dynamic table INFORMATION_SCHEMA.INNODB_BUFFER_PAGE.
@return	0 on success, 1 on failure */
static
int
i_s_innodb_buffer_page_init(
/*========================*/
	void*	p)	/*!< in/out: table schema object */
{
	ST_SCHEMA_TABLE*	schema;

	DBUG_ENTER("i_s_innodb_buffer_page_init");

	schema = reinterpret_cast<ST_SCHEMA_TABLE*>(p);

	schema->fields_info = i_s_innodb_buffer_page_fields_info;
	schema->fill_table = i_s_innodb_buffer_page_fill_table;

	DBUG_RETURN(0);
}

UNIV_INTERN struct st_mysql_plugin	i_s_innodb_buffer_page =
{
	/* the plugin type (a MYSQL_XXX_PLUGIN value) */
	/* int */
	STRUCT_FLD(type, MYSQL_INFORMATION_SCHEMA_PLUGIN),

	/* pointer to type-specific plugin descriptor */
	/* void* */
	STRUCT_FLD(info, &i_s_info),

	/* plugin name */
	/* const char* */
	STRUCT_FLD(name, "INNODB_BUFFER_PAGE"),

	/* plugin author (for SHOW PLUGINS) */
	/* const char* */
	STRUCT_FLD(author, plugin_author),

	/* general descriptive text (for SHOW PLUGINS) */
	/* const char* */
	STRUCT_FLD(descr, "InnoDB Buffer Page Information"),

	/* the plugin license (PLUGIN_LICENSE_XXX) */
	/* int */
	STRUCT_FLD(license, PLUGIN_LICENSE_GPL),

	/* the function to invoke when plugin is loaded */
	/* int (*)(void*); */
	STRUCT_FLD(init, i_s_innodb_buffer_page_init),

	/* the function to invoke when plugin is unloaded */
	/* int (*)(void*); */
	STRUCT_FLD(deinit, i_s_common_deinit),

	/* plugin version (for SHOW PLUGINS) */
	/* unsigned int */
	STRUCT_FLD(version, INNODB_VERSION_SHORT),

	/* struct st_mysql_show_var* */
	STRUCT_FLD(status_vars, NULL),

	/* struct st_mysql_sys_var** */
	STRUCT_FLD(system_vars, NULL),

	/* reserved for dependency checking */
	/* void* */
	STRUCT_FLD(__reserved1, NULL),

	/* Plugin flags */
	/* unsigned long */
	STRUCT_FLD(flags, 0UL),
};

static ST_FIELD_INFO	i_s_innodb_buf_page_lru_fields_info[] =
{
#define IDX_BUF_LRU_POOL_ID		0
	{STRUCT_FLD(field_name,		"POOL_ID"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define IDX_BUF_LRU_POS			1
	{STRUCT_FLD(field_name,		"LRU_POSITION"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define IDX_BUF_LRU_PAGE_SPACE		2
	{STRUCT_FLD(field_name,		"SPACE"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define IDX_BUF_LRU_PAGE_NUM		3
	{STRUCT_FLD(field_name,		"PAGE_NUMBER"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define IDX_BUF_LRU_PAGE_TYPE		4
	{STRUCT_FLD(field_name,		"PAGE_TYPE"),
	 STRUCT_FLD(field_length,	64),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_STRING),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_MAYBE_NULL),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define IDX_BUF_LRU_PAGE_FLUSH_TYPE	5
	{STRUCT_FLD(field_name,		"FLUSH_TYPE"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define IDX_BUF_LRU_PAGE_FIX_COUNT	6
	{STRUCT_FLD(field_name,		"FIX_COUNT"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define IDX_BUF_LRU_PAGE_HASHED		7
	{STRUCT_FLD(field_name,		"IS_HASHED"),
	 STRUCT_FLD(field_length,	3),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_STRING),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_MAYBE_NULL),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define IDX_BUF_LRU_PAGE_NEWEST_MOD	8
	{STRUCT_FLD(field_name,		"NEWEST_MODIFICATION"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define IDX_BUF_LRU_PAGE_OLDEST_MOD	9
	{STRUCT_FLD(field_name,		"OLDEST_MODIFICATION"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define IDX_BUF_LRU_PAGE_ACCESS_TIME	10
	{STRUCT_FLD(field_name,		"ACCESS_TIME"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define IDX_BUF_LRU_PAGE_TABLE_NAME	11
	{STRUCT_FLD(field_name,		"TABLE_NAME"),
	 STRUCT_FLD(field_length,	1024),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_STRING),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_MAYBE_NULL),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define IDX_BUF_LRU_PAGE_INDEX_NAME	12
	{STRUCT_FLD(field_name,		"INDEX_NAME"),
	 STRUCT_FLD(field_length,	1024),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_STRING),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_MAYBE_NULL),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define IDX_BUF_LRU_PAGE_NUM_RECS	13
	{STRUCT_FLD(field_name,		"NUMBER_RECORDS"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define IDX_BUF_LRU_PAGE_DATA_SIZE	14
	{STRUCT_FLD(field_name,		"DATA_SIZE"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define IDX_BUF_LRU_PAGE_ZIP_SIZE	15
	{STRUCT_FLD(field_name,		"COMPRESSED_SIZE"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define IDX_BUF_LRU_PAGE_STATE		16
	{STRUCT_FLD(field_name,		"COMPRESSED"),
	 STRUCT_FLD(field_length,	3),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_STRING),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_MAYBE_NULL),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define IDX_BUF_LRU_PAGE_IO_FIX		17
	{STRUCT_FLD(field_name,		"IO_FIX"),
	 STRUCT_FLD(field_length,	64),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_STRING),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_MAYBE_NULL),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define IDX_BUF_LRU_PAGE_IS_OLD		18
	{STRUCT_FLD(field_name,		"IS_OLD"),
	 STRUCT_FLD(field_length,	3),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_STRING),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_MAYBE_NULL),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define IDX_BUF_LRU_PAGE_FREE_CLOCK	19
	{STRUCT_FLD(field_name,		"FREE_PAGE_CLOCK"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	END_OF_ST_FIELD_INFO
};

/*******************************************************************//**
Fill Information Schema table INNODB_BUFFER_PAGE_LRU with information
cached in the buf_page_info_t array
@return	0 on success, 1 on failure */
static
int
i_s_innodb_buf_page_lru_fill(
/*=========================*/
	THD*			thd,		/*!< in: thread */
	TABLE_LIST*		tables,		/*!< in/out: tables to fill */
	const buf_page_info_t*	info_array,	/*!< in: array cached page
						info */
	ulint			num_page)	/*!< in: number of page info
						 cached */
{
	TABLE*			table;
	Field**			fields;
	mem_heap_t*		heap;

	DBUG_ENTER("i_s_innodb_buf_page_lru_fill");

	table = tables->table;

	fields = table->field;

	heap = mem_heap_create(1000);

	/* Iterate through the cached array and fill the I_S table rows */
	for (ulint i = 0; i < num_page; i++) {
		const buf_page_info_t*	page_info;
		const char*		table_name;
		const char*		index_name;
		const char*		state_str;
		enum buf_page_state	state;

		table_name = NULL;
		index_name = NULL;
		state_str = NULL;

		page_info = info_array + i;

		OK(fields[IDX_BUF_LRU_POOL_ID]->store(page_info->pool_id));

		OK(fields[IDX_BUF_LRU_POS]->store(page_info->block_id));

		OK(fields[IDX_BUF_LRU_PAGE_SPACE]->store(page_info->space_id));

		OK(fields[IDX_BUF_LRU_PAGE_NUM]->store(page_info->page_num));

		OK(field_store_string(
			fields[IDX_BUF_LRU_PAGE_TYPE],
			i_s_page_type[page_info->page_type].type_str));

		OK(fields[IDX_BUF_LRU_PAGE_FLUSH_TYPE]->store(
			page_info->flush_type));

		OK(fields[IDX_BUF_LRU_PAGE_FIX_COUNT]->store(
			page_info->fix_count));

		if (page_info->hashed) {
			OK(field_store_string(
				fields[IDX_BUF_LRU_PAGE_HASHED], "YES"));
		} else {
			OK(field_store_string(
				fields[IDX_BUF_LRU_PAGE_HASHED], "NO"));
		}

		OK(fields[IDX_BUF_LRU_PAGE_NEWEST_MOD]->store(
			page_info->newest_mod, true));

		OK(fields[IDX_BUF_LRU_PAGE_OLDEST_MOD]->store(
			page_info->oldest_mod, true));

		OK(fields[IDX_BUF_LRU_PAGE_ACCESS_TIME]->store(
			page_info->access_time));

		/* If this is an index page, fetch the index name
		and table name */
		if (page_info->page_type == I_S_PAGE_TYPE_INDEX) {
			const dict_index_t*	index;

			mutex_enter(&dict_sys->mutex);
			index = dict_index_get_if_in_cache_low(
				page_info->index_id);

			/* Copy the index/table name under mutex. We
			do not want to hold the InnoDB mutex while
			filling the IS table */
			if (index) {
				const char*	name_ptr = index->name;

				if (name_ptr[0] == TEMP_INDEX_PREFIX) {
					name_ptr++;
				}

				index_name = mem_heap_strdup(heap, name_ptr);

				table_name = mem_heap_strdup(heap,
							     index->table_name);
			}

			mutex_exit(&dict_sys->mutex);
		}

		OK(field_store_string(
			fields[IDX_BUF_LRU_PAGE_TABLE_NAME], table_name));

		OK(field_store_string(
			fields[IDX_BUF_LRU_PAGE_INDEX_NAME], index_name));
		OK(fields[IDX_BUF_LRU_PAGE_NUM_RECS]->store(
			page_info->num_recs));

		OK(fields[IDX_BUF_LRU_PAGE_DATA_SIZE]->store(
			page_info->data_size));

		OK(fields[IDX_BUF_LRU_PAGE_ZIP_SIZE]->store(
			page_info->zip_ssize ?
				 512 << page_info->zip_ssize : 0));

		state = static_cast<enum buf_page_state>(page_info->page_state);

		switch (state) {
		/* Compressed page */
		case BUF_BLOCK_ZIP_PAGE:
		case BUF_BLOCK_ZIP_DIRTY:
			state_str = "YES";
			break;
		/* Uncompressed page */
		case BUF_BLOCK_FILE_PAGE:
			state_str = "NO";
			break;
		/* We should not see following states */
		case BUF_BLOCK_ZIP_FREE:
		case BUF_BLOCK_READY_FOR_USE:
		case BUF_BLOCK_NOT_USED:
		case BUF_BLOCK_MEMORY:
		case BUF_BLOCK_REMOVE_HASH:
			state_str = NULL;
			break;
		};

		OK(field_store_string(fields[IDX_BUF_LRU_PAGE_STATE],
				      state_str));

		switch (page_info->io_fix) {
		case BUF_IO_NONE:
			OK(field_store_string(fields[IDX_BUF_LRU_PAGE_IO_FIX],
					      "IO_NONE"));
			break;
		case BUF_IO_READ:
			OK(field_store_string(fields[IDX_BUF_LRU_PAGE_IO_FIX],
					      "IO_READ"));
			break;
		case BUF_IO_WRITE:
			OK(field_store_string(fields[IDX_BUF_LRU_PAGE_IO_FIX],
					      "IO_WRITE"));
			break;
		}

		OK(field_store_string(fields[IDX_BUF_LRU_PAGE_IS_OLD],
				      (page_info->is_old) ? "YES" : "NO"));

		OK(fields[IDX_BUF_LRU_PAGE_FREE_CLOCK]->store(
			page_info->freed_page_clock));

		if (schema_table_store_record(thd, table)) {
			mem_heap_free(heap);
			DBUG_RETURN(1);
		}

		mem_heap_empty(heap);
	}

	mem_heap_free(heap);

	DBUG_RETURN(0);
}

/*******************************************************************//**
This is the function that goes through buffer pool's LRU list
and fetch information to INFORMATION_SCHEMA.INNODB_BUFFER_PAGE_LRU.
@return	0 on success, 1 on failure */
static
int
i_s_innodb_fill_buffer_lru(
/*=======================*/
	THD*			thd,		/*!< in: thread */
	TABLE_LIST*		tables,		/*!< in/out: tables to fill */
	buf_pool_t*		buf_pool,	/*!< in: buffer pool to scan */
	const ulint		pool_id)	/*!< in: buffer pool id */
{
	int			status = 0;
	buf_page_info_t*	info_buffer;
	ulint			lru_pos = 0;
	const buf_page_t*	bpage;
	ulint			lru_len;
	mutex_t*		block_mutex;

	DBUG_ENTER("i_s_innodb_fill_buffer_lru");

	RETURN_IF_INNODB_NOT_STARTED(tables->schema_table_name);

	/* Obtain buf_pool mutex before allocate info_buffer, since
	UT_LIST_GET_LEN(buf_pool->LRU) could change */
	mutex_enter(&buf_pool->LRU_list_mutex);

	lru_len = UT_LIST_GET_LEN(buf_pool->LRU);

	/* Print error message if malloc fail */
	info_buffer = (buf_page_info_t*) my_malloc(
		lru_len * sizeof *info_buffer, MYF(MY_WME));

	if (!info_buffer) {
		status = 1;
		goto exit;
	}

	memset(info_buffer, 0, lru_len * sizeof *info_buffer);

	/* Walk through Pool's LRU list and print the buffer page
	information */
	bpage = UT_LIST_GET_LAST(buf_pool->LRU);

	while (bpage != NULL) {
		block_mutex = buf_page_get_mutex_enter(bpage);
		/* Use the same function that collect buffer info for
		INNODB_BUFFER_PAGE to get buffer page info */
		i_s_innodb_buffer_page_get_info(bpage, pool_id, lru_pos,
						(info_buffer + lru_pos));

		bpage = UT_LIST_GET_PREV(LRU, bpage);
		mutex_exit(block_mutex);

		lru_pos++;
	}

	ut_ad(lru_pos == lru_len);
	ut_ad(lru_pos == UT_LIST_GET_LEN(buf_pool->LRU));

exit:
	mutex_exit(&buf_pool->LRU_list_mutex);

	if (info_buffer) {
		status = i_s_innodb_buf_page_lru_fill(
			thd, tables, info_buffer, lru_len);

		my_free(info_buffer);
	}

	DBUG_RETURN(status);
}

/*******************************************************************//**
Fill page information for pages in InnoDB buffer pool to the
dynamic table INFORMATION_SCHEMA.INNODB_BUFFER_PAGE_LRU
@return	0 on success, 1 on failure */
static
int
i_s_innodb_buf_page_lru_fill_table(
/*===============================*/
	THD*		thd,		/*!< in: thread */
	TABLE_LIST*	tables,		/*!< in/out: tables to fill */
	Item*		)		/*!< in: condition (ignored) */
{
	int	status	= 0;

	DBUG_ENTER("i_s_innodb_buf_page_lru_fill_table");

	/* deny access to any users that do not hold PROCESS_ACL */
	if (check_global_access(thd, PROCESS_ACL)) {
		DBUG_RETURN(0);
	}

	/* Walk through each buffer pool */
	for (ulint i = 0; i < srv_buf_pool_instances; i++) {
		buf_pool_t*	buf_pool;

		buf_pool = buf_pool_from_array(i);

		/* Fetch information from pages in this buffer pool's LRU list,
		and fill the corresponding I_S table */
		status = i_s_innodb_fill_buffer_lru(thd, tables, buf_pool, i);

		/* If something wrong, break and return */
		if (status) {
			break;
		}
	}

	DBUG_RETURN(status);
}

/*******************************************************************//**
Bind the dynamic table INFORMATION_SCHEMA.INNODB_BUFFER_PAGE_LRU.
@return	0 on success, 1 on failure */
static
int
i_s_innodb_buffer_page_lru_init(
/*============================*/
	void*	p)	/*!< in/out: table schema object */
{
	ST_SCHEMA_TABLE*	schema;

	DBUG_ENTER("i_s_innodb_buffer_page_lru_init");

	schema = reinterpret_cast<ST_SCHEMA_TABLE*>(p);

	schema->fields_info = i_s_innodb_buf_page_lru_fields_info;
	schema->fill_table = i_s_innodb_buf_page_lru_fill_table;

	DBUG_RETURN(0);
}

UNIV_INTERN struct st_mysql_plugin	i_s_innodb_buffer_page_lru =
{
	/* the plugin type (a MYSQL_XXX_PLUGIN value) */
	/* int */
	STRUCT_FLD(type, MYSQL_INFORMATION_SCHEMA_PLUGIN),

	/* pointer to type-specific plugin descriptor */
	/* void* */
	STRUCT_FLD(info, &i_s_info),

	/* plugin name */
	/* const char* */
	STRUCT_FLD(name, "INNODB_BUFFER_PAGE_LRU"),

	/* plugin author (for SHOW PLUGINS) */
	/* const char* */
	STRUCT_FLD(author, plugin_author),

	/* general descriptive text (for SHOW PLUGINS) */
	/* const char* */
	STRUCT_FLD(descr, "InnoDB Buffer Page in LRU"),

	/* the plugin license (PLUGIN_LICENSE_XXX) */
	/* int */
	STRUCT_FLD(license, PLUGIN_LICENSE_GPL),

	/* the function to invoke when plugin is loaded */
	/* int (*)(void*); */
	STRUCT_FLD(init, i_s_innodb_buffer_page_lru_init),

	/* the function to invoke when plugin is unloaded */
	/* int (*)(void*); */
	STRUCT_FLD(deinit, i_s_common_deinit),

	/* plugin version (for SHOW PLUGINS) */
	/* unsigned int */
	STRUCT_FLD(version, INNODB_VERSION_SHORT),

	/* struct st_mysql_show_var* */
	STRUCT_FLD(status_vars, NULL),

	/* struct st_mysql_sys_var** */
	STRUCT_FLD(system_vars, NULL),

	/* reserved for dependency checking */
	/* void* */
	STRUCT_FLD(__reserved1, NULL),

	/* Plugin flags */
	/* unsigned long */
	STRUCT_FLD(flags, 0UL),
};

/*******************************************************************//**
Unbind a dynamic INFORMATION_SCHEMA table.
@return	0 on success */
static
int
i_s_common_deinit(
/*==============*/
	void*	p)	/*!< in/out: table schema object */
{
	DBUG_ENTER("i_s_common_deinit");

	/* Do nothing */

	DBUG_RETURN(0);
}

/* Fields of the dynamic table INFORMATION_SCHEMA.SYS_TABLES */
static ST_FIELD_INFO    innodb_sys_tables_fields_info[] =
{
#define SYS_TABLE_ID		0
	{STRUCT_FLD(field_name,		"TABLE_ID"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define SYS_TABLE_SCHEMA	1
	{STRUCT_FLD(field_name,		"SCHEMA"),
	 STRUCT_FLD(field_length,	NAME_LEN + 1),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_STRING),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	0),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define SYS_TABLE_NAME		2
	{STRUCT_FLD(field_name,		"NAME"),
	 STRUCT_FLD(field_length,	NAME_LEN + 1),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_STRING),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	0),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define SYS_TABLE_FLAG		3
	{STRUCT_FLD(field_name,		"FLAG"),
	 STRUCT_FLD(field_length,	MY_INT32_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	0),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define SYS_TABLE_NUM_COLUMN	4
	{STRUCT_FLD(field_name,		"N_COLS"),
	 STRUCT_FLD(field_length,	MY_INT32_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	0),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define SYS_TABLE_SPACE		5
	{STRUCT_FLD(field_name,		"SPACE"),
	 STRUCT_FLD(field_length,	MY_INT32_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	0),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	END_OF_ST_FIELD_INFO
};

/**********************************************************************//**
Populate information_schema.innodb_sys_tables table with information
from SYS_TABLES.
@return	0 on success */
static
int
i_s_dict_fill_sys_tables(
/*=====================*/
	THD*		thd,		/*!< in: thread */
	dict_table_t*	table,		/*!< in: table */
	TABLE*		table_to_fill)  /*!< in/out: fill this table */
{
	Field**		fields;
	char		buf[NAME_LEN * 2 + 2];
	char*		ptr;

	DBUG_ENTER("i_s_dict_fill_sys_tables");

	fields = table_to_fill->field;

	OK(fields[SYS_TABLE_ID]->store(longlong(table->id), TRUE));

	strncpy(buf, table->name, NAME_LEN * 2 + 2);
	ptr = strchr(buf, '/');
	if (ptr) {
		*ptr = '\0';
		++ptr;

		OK(field_store_string(fields[SYS_TABLE_SCHEMA], buf));
		OK(field_store_string(fields[SYS_TABLE_NAME], ptr));
	} else {
		fields[SYS_TABLE_SCHEMA]->set_null();
		OK(field_store_string(fields[SYS_TABLE_NAME], buf));
	}

	OK(fields[SYS_TABLE_FLAG]->store(table->flags));

	OK(fields[SYS_TABLE_NUM_COLUMN]->store(table->n_cols));

	OK(fields[SYS_TABLE_SPACE]->store(table->space));

	OK(schema_table_store_record(thd, table_to_fill));

	DBUG_RETURN(0);
}
/*******************************************************************//**
Function to go through each record in SYS_TABLES table, and fill the
information_schema.innodb_sys_tables table with related table information
@return 0 on success */
static
int
i_s_sys_tables_fill_table(
/*======================*/
	THD*		thd,    /*!< in: thread */
	TABLE_LIST*	tables, /*!< in/out: tables to fill */
	COND*		cond)   /*!< in: condition (not used) */
{
        btr_pcur_t	pcur;
	const rec_t*	rec;
	mem_heap_t*	heap;
	mtr_t		mtr;

	DBUG_ENTER("i_s_sys_tables_fill_table");

	RETURN_IF_INNODB_NOT_STARTED(tables->schema_table_name);

	/* deny access to non-superusers */
	if (check_global_access(thd, PROCESS_ACL)) {

                DBUG_RETURN(0);
	}

        heap = mem_heap_create(1000);
        mutex_enter(&(dict_sys->mutex));
        mtr_start(&mtr);

	rec = dict_startscan_system(&pcur, &mtr, SYS_TABLES);

	while (rec) {
		const char*	err_msg;
		dict_table_t*	table_rec;

		/* Create and populate a dict_table_t structure with
		information from SYS_TABLES row */
		err_msg = dict_process_sys_tables_rec(
			heap, rec, &table_rec, DICT_TABLE_LOAD_FROM_RECORD);

		mtr_commit(&mtr);
		mutex_exit(&dict_sys->mutex);

		if (!err_msg) {
			i_s_dict_fill_sys_tables(thd, table_rec, tables->table);
		} else {
			push_warning_printf(thd, MYSQL_ERROR::WARN_LEVEL_WARN,
					    ER_CANT_FIND_SYSTEM_REC,
					    err_msg);
		}

		/* Since dict_process_sys_tables_rec() is called with
		DICT_TABLE_LOAD_FROM_RECORD, the table_rec is created in
		dict_process_sys_tables_rec(), we will need to free it */
		if (table_rec) {
			dict_mem_table_free(table_rec);
		}

		mem_heap_empty(heap);

		/* Get the next record */
		mutex_enter(&dict_sys->mutex);
		mtr_start(&mtr);
		rec = dict_getnext_system(&pcur, &mtr);
	}

	mtr_commit(&mtr);
	mutex_exit(&dict_sys->mutex);
	mem_heap_free(heap);

	DBUG_RETURN(0);
}

/*******************************************************************//**
Bind the dynamic table INFORMATION_SCHEMA.innodb_sys_tables
@return 0 on success */
static
int
innodb_sys_tables_init(
/*===================*/
        void*   p)      /*!< in/out: table schema object */
{
        ST_SCHEMA_TABLE*        schema;

        DBUG_ENTER("innodb_sys_tables_init");

        schema = (ST_SCHEMA_TABLE*) p;

        schema->fields_info = innodb_sys_tables_fields_info;
        schema->fill_table = i_s_sys_tables_fill_table;

        DBUG_RETURN(0);
}

UNIV_INTERN struct st_mysql_plugin	i_s_innodb_sys_tables =
{
	/* the plugin type (a MYSQL_XXX_PLUGIN value) */
	/* int */
	STRUCT_FLD(type, MYSQL_INFORMATION_SCHEMA_PLUGIN),

	/* pointer to type-specific plugin descriptor */
	/* void* */
	STRUCT_FLD(info, &i_s_info),

	/* plugin name */
	/* const char* */
	STRUCT_FLD(name, "INNODB_SYS_TABLES"),

	/* plugin author (for SHOW PLUGINS) */
	/* const char* */
	STRUCT_FLD(author, "Percona"),

	/* general descriptive text (for SHOW PLUGINS) */
	/* const char* */
	STRUCT_FLD(descr, "InnoDB SYS_TABLES"),

	/* the plugin license (PLUGIN_LICENSE_XXX) */
	/* int */
	STRUCT_FLD(license, PLUGIN_LICENSE_GPL),

	/* the function to invoke when plugin is loaded */
	/* int (*)(void*); */
	STRUCT_FLD(init, innodb_sys_tables_init),

	/* the function to invoke when plugin is unloaded */
	/* int (*)(void*); */
	STRUCT_FLD(deinit, i_s_common_deinit),

	/* plugin version (for SHOW PLUGINS) */
	/* unsigned int */
	STRUCT_FLD(version, INNODB_VERSION_SHORT),

	/* struct st_mysql_show_var* */
	STRUCT_FLD(status_vars, NULL),

	/* struct st_mysql_sys_var** */
	STRUCT_FLD(system_vars, NULL),

	/* reserved for dependency checking */
	/* void* */
	STRUCT_FLD(__reserved1, NULL),

	/* flags for plugin */
	/* unsigned long */
	STRUCT_FLD(flags, 0UL)
};

/* Fields of the dynamic table INFORMATION_SCHEMA.SYS_TABLESTATS */
static ST_FIELD_INFO    innodb_sys_tablestats_fields_info[] =
{
#define SYS_TABLESTATS_ID		0
	{STRUCT_FLD(field_name,		"TABLE_ID"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define SYS_TABLESTATS_SCHEMA		1
	{STRUCT_FLD(field_name,		"SCHEMA"),
	 STRUCT_FLD(field_length,	NAME_LEN + 1),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_STRING),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	0),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define SYS_TABLESTATS_NAME		2
	{STRUCT_FLD(field_name,		"NAME"),
	 STRUCT_FLD(field_length,	NAME_LEN + 1),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_STRING),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	0),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define SYS_TABLESTATS_INIT		3
	{STRUCT_FLD(field_name,		"STATS_INITIALIZED"),
	 STRUCT_FLD(field_length,	NAME_LEN + 1),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_STRING),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	0),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define SYS_TABLESTATS_NROW		4
	{STRUCT_FLD(field_name,		"NUM_ROWS"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define SYS_TABLESTATS_CLUST_SIZE	5
	{STRUCT_FLD(field_name,		"CLUST_INDEX_SIZE"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define SYS_TABLESTATS_INDEX_SIZE	6
	{STRUCT_FLD(field_name,		"OTHER_INDEX_SIZE"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define SYS_TABLESTATS_MODIFIED		7
	{STRUCT_FLD(field_name,		"MODIFIED_COUNTER"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define SYS_TABLESTATS_AUTONINC		8
	{STRUCT_FLD(field_name,		"AUTOINC"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define SYS_TABLESTATS_MYSQL_OPEN_HANDLE	9
	{STRUCT_FLD(field_name,		"MYSQL_HANDLES_OPENED"),
	 STRUCT_FLD(field_length,	MY_INT32_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	0),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	END_OF_ST_FIELD_INFO
};

/**********************************************************************//**
Populate information_schema.innodb_sys_tablestats table with information
from SYS_TABLES.
@return	0 on success */
static
int
i_s_dict_fill_sys_tablestats(
/*=========================*/
	THD*		thd,		/*!< in: thread */
	dict_table_t*	table,		/*!< in: table */
	TABLE*		table_to_fill)  /*!< in/out: fill this table */
{
	Field**		fields;
	char		buf[NAME_LEN * 2 + 2];
	char*		ptr;

	DBUG_ENTER("i_s_dict_fill_sys_tablestats");

	fields = table_to_fill->field;

	OK(fields[SYS_TABLESTATS_ID]->store(longlong(table->id), TRUE));

	strncpy(buf, table->name, NAME_LEN * 2 + 2);
	ptr = strchr(buf, '/');
	if (ptr) {
		*ptr = '\0';
		++ptr;

		OK(field_store_string(fields[SYS_TABLESTATS_SCHEMA], buf));
		OK(field_store_string(fields[SYS_TABLESTATS_NAME], ptr));
	} else {
		fields[SYS_TABLESTATS_SCHEMA]->set_null();
		OK(field_store_string(fields[SYS_TABLESTATS_NAME], buf));
	}

	if (table->stat_initialized) {
		OK(field_store_string(fields[SYS_TABLESTATS_INIT],
				      "Initialized"));
	} else {
		OK(field_store_string(fields[SYS_TABLESTATS_INIT],
				      "Uninitialized"));
	}

	OK(fields[SYS_TABLESTATS_NROW]->store(table->stat_n_rows, TRUE));

	OK(fields[SYS_TABLESTATS_CLUST_SIZE]->store(
		table->stat_clustered_index_size));

	OK(fields[SYS_TABLESTATS_INDEX_SIZE]->store(
		table->stat_sum_of_other_index_sizes));

	OK(fields[SYS_TABLESTATS_MODIFIED]->store(
		table->stat_modified_counter));

	OK(fields[SYS_TABLESTATS_AUTONINC]->store(table->autoinc, TRUE));

	OK(fields[SYS_TABLESTATS_MYSQL_OPEN_HANDLE]->store(
		table->n_mysql_handles_opened));

	OK(schema_table_store_record(thd, table_to_fill));

	DBUG_RETURN(0);
}
/*******************************************************************//**
Function to go through each record in SYS_TABLES table, and fill the
information_schema.innodb_sys_tablestats table with table statistics
related information
@return 0 on success */
static
int
i_s_sys_tables_fill_table_stats(
/*============================*/
	THD*		thd,    /*!< in: thread */
	TABLE_LIST*	tables, /*!< in/out: tables to fill */
	COND*		cond)   /*!< in: condition (not used) */
{
        btr_pcur_t	pcur;
	const rec_t*	rec;
	mem_heap_t*	heap;
	mtr_t		mtr;

	DBUG_ENTER("i_s_sys_tables_fill_table_stats");

	RETURN_IF_INNODB_NOT_STARTED(tables->schema_table_name);

	/* deny access to non-superusers */
	if (check_global_access(thd, PROCESS_ACL)) {

                DBUG_RETURN(0);
	}

        heap = mem_heap_create(1000);
        mutex_enter(&dict_sys->mutex);
        mtr_start(&mtr);

	rec = dict_startscan_system(&pcur, &mtr, SYS_TABLES);

	while (rec) {
		const char*	err_msg;
		dict_table_t*	table_rec;

		/* Fetch the dict_table_t structure corresponding to
		this SYS_TABLES record */
		err_msg = dict_process_sys_tables_rec(
			heap, rec, &table_rec, DICT_TABLE_LOAD_FROM_CACHE);

		mtr_commit(&mtr);
		mutex_exit(&dict_sys->mutex);

		if (!err_msg) {
			i_s_dict_fill_sys_tablestats(thd, table_rec,
						     tables->table);
		} else {
			push_warning_printf(thd, MYSQL_ERROR::WARN_LEVEL_WARN,
					    ER_CANT_FIND_SYSTEM_REC,
					    err_msg);
		}

		mem_heap_empty(heap);

		/* Get the next record */
		mutex_enter(&dict_sys->mutex);
		mtr_start(&mtr);
		rec = dict_getnext_system(&pcur, &mtr);
	}

	mtr_commit(&mtr);
	mutex_exit(&dict_sys->mutex);
	mem_heap_free(heap);

	DBUG_RETURN(0);
}

/*******************************************************************//**
Bind the dynamic table INFORMATION_SCHEMA.innodb_sys_tablestats
@return 0 on success */
static
int
innodb_sys_tablestats_init(
/*=======================*/
        void*   p)      /*!< in/out: table schema object */
{
        ST_SCHEMA_TABLE*        schema;

        DBUG_ENTER("innodb_sys_tablestats_init");

        schema = (ST_SCHEMA_TABLE*) p;

        schema->fields_info = innodb_sys_tablestats_fields_info;
        schema->fill_table = i_s_sys_tables_fill_table_stats;

        DBUG_RETURN(0);
}

UNIV_INTERN struct st_mysql_plugin	i_s_innodb_sys_tablestats =
{
	/* the plugin type (a MYSQL_XXX_PLUGIN value) */
	/* int */
	STRUCT_FLD(type, MYSQL_INFORMATION_SCHEMA_PLUGIN),

	/* pointer to type-specific plugin descriptor */
	/* void* */
	STRUCT_FLD(info, &i_s_info),

	/* plugin name */
	/* const char* */
	STRUCT_FLD(name, "INNODB_SYS_TABLESTATS"),

	/* plugin author (for SHOW PLUGINS) */
	/* const char* */
	STRUCT_FLD(author, "Percona"),

	/* general descriptive text (for SHOW PLUGINS) */
	/* const char* */
	STRUCT_FLD(descr, "InnoDB SYS_TABLESTATS"),

	/* the plugin license (PLUGIN_LICENSE_XXX) */
	/* int */
	STRUCT_FLD(license, PLUGIN_LICENSE_GPL),

	/* the function to invoke when plugin is loaded */
	/* int (*)(void*); */
	STRUCT_FLD(init, innodb_sys_tablestats_init),

	/* the function to invoke when plugin is unloaded */
	/* int (*)(void*); */
	STRUCT_FLD(deinit, i_s_common_deinit),

	/* plugin version (for SHOW PLUGINS) */
	/* unsigned int */
	STRUCT_FLD(version, INNODB_VERSION_SHORT),

	/* struct st_mysql_show_var* */
	STRUCT_FLD(status_vars, NULL),

	/* struct st_mysql_sys_var** */
	STRUCT_FLD(system_vars, NULL),

	/* reserved for dependency checking */
	/* void* */
	STRUCT_FLD(__reserved1, NULL),

	/* flags for plugin */
	/* unsigned long */
	STRUCT_FLD(flags, 0UL)
};

/* Fields of the dynamic table INFORMATION_SCHEMA.SYS_INDEXES */
static ST_FIELD_INFO    innodb_sysindex_fields_info[] =
{
#define SYS_INDEX_ID		0
	{STRUCT_FLD(field_name,		"INDEX_ID"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define SYS_INDEX_NAME		1
	{STRUCT_FLD(field_name,		"NAME"),
	 STRUCT_FLD(field_length,	NAME_LEN + 1),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_STRING),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	0),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define SYS_INDEX_TABLE_ID	2
	{STRUCT_FLD(field_name,		"TABLE_ID"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define SYS_INDEX_TYPE		3
	{STRUCT_FLD(field_name,		"TYPE"),
	 STRUCT_FLD(field_length,	MY_INT32_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	0),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define SYS_INDEX_NUM_FIELDS	4
	{STRUCT_FLD(field_name,		"N_FIELDS"),
	 STRUCT_FLD(field_length,	MY_INT32_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	0),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define SYS_INDEX_PAGE_NO	5
	{STRUCT_FLD(field_name,		"PAGE_NO"),
	 STRUCT_FLD(field_length,	MY_INT32_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	0),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define SYS_INDEX_SPACE		6
	{STRUCT_FLD(field_name,		"SPACE"),
	 STRUCT_FLD(field_length,	MY_INT32_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	0),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	END_OF_ST_FIELD_INFO
};

/**********************************************************************//**
Function to populate the information_schema.innodb_sys_indexes table with
collected index information
@return 0 on success */
static
int
i_s_dict_fill_sys_indexes(
/*======================*/
	THD*		thd,		/*!< in: thread */
	table_id_t	table_id,	/*!< in: table id */
	dict_index_t*	index,		/*!< in: populated dict_index_t
					struct with index info */
	TABLE*		table_to_fill)  /*!< in/out: fill this table */
{
	Field**		fields;

	DBUG_ENTER("i_s_dict_fill_sys_indexes");

	fields = table_to_fill->field;

	OK(fields[SYS_INDEX_ID]->store(longlong(index->id), TRUE));

	OK(field_store_string(fields[SYS_INDEX_NAME], index->name));

	OK(fields[SYS_INDEX_TABLE_ID]->store(longlong(table_id), TRUE));

	OK(fields[SYS_INDEX_TYPE]->store(index->type));

	OK(fields[SYS_INDEX_NUM_FIELDS]->store(index->n_fields));

	OK(fields[SYS_INDEX_PAGE_NO]->store(index->page));

	OK(fields[SYS_INDEX_SPACE]->store(index->space));

	OK(schema_table_store_record(thd, table_to_fill));

	DBUG_RETURN(0);
}
/*******************************************************************//**
Function to go through each record in SYS_INDEXES table, and fill the
information_schema.innodb_sys_indexes table with related index information
@return 0 on success */
static
int
i_s_sys_indexes_fill_table(
/*=======================*/
	THD*		thd,    /*!< in: thread */
	TABLE_LIST*	tables, /*!< in/out: tables to fill */
	COND*		cond)   /*!< in: condition (not used) */
{
        btr_pcur_t		pcur;
	const rec_t*		rec;
	mem_heap_t*		heap;
	mtr_t			mtr;

	DBUG_ENTER("i_s_sys_indexes_fill_table");

	RETURN_IF_INNODB_NOT_STARTED(tables->schema_table_name);

	/* deny access to non-superusers */
	if (check_global_access(thd, PROCESS_ACL)) {

                DBUG_RETURN(0);
	}

        heap = mem_heap_create(1000);
        mutex_enter(&dict_sys->mutex);
        mtr_start(&mtr);

	/* Start scan the SYS_INDEXES table */
	rec = dict_startscan_system(&pcur, &mtr, SYS_INDEXES);

	/* Process each record in the table */
	while (rec) {
		const char*	err_msg;;
		table_id_t	table_id;
		dict_index_t	index_rec;

		/* Populate a dict_index_t structure with information from
		a SYS_INDEXES row */
		err_msg = dict_process_sys_indexes_rec(heap, rec, &index_rec,
						       &table_id);

		mtr_commit(&mtr);
		mutex_exit(&dict_sys->mutex);

		if (!err_msg) {
			i_s_dict_fill_sys_indexes(thd, table_id, &index_rec,
						 tables->table);
		} else {
			push_warning_printf(thd, MYSQL_ERROR::WARN_LEVEL_WARN,
					    ER_CANT_FIND_SYSTEM_REC,
					    err_msg);
		}

		mem_heap_empty(heap);

		/* Get the next record */
		mutex_enter(&dict_sys->mutex);
		mtr_start(&mtr);
		rec = dict_getnext_system(&pcur, &mtr);
	}

	mtr_commit(&mtr);
	mutex_exit(&dict_sys->mutex);
	mem_heap_free(heap);

	DBUG_RETURN(0);
}
/*******************************************************************//**
Bind the dynamic table INFORMATION_SCHEMA.innodb_sys_indexes
@return 0 on success */
static
int
innodb_sys_indexes_init(
/*====================*/
        void*   p)      /*!< in/out: table schema object */
{
        ST_SCHEMA_TABLE*        schema;

        DBUG_ENTER("innodb_sys_index_init");

        schema = (ST_SCHEMA_TABLE*) p;

        schema->fields_info = innodb_sysindex_fields_info;
        schema->fill_table = i_s_sys_indexes_fill_table;

        DBUG_RETURN(0);
}

UNIV_INTERN struct st_mysql_plugin	i_s_innodb_sys_indexes =
{
	/* the plugin type (a MYSQL_XXX_PLUGIN value) */
	/* int */
	STRUCT_FLD(type, MYSQL_INFORMATION_SCHEMA_PLUGIN),

	/* pointer to type-specific plugin descriptor */
	/* void* */
	STRUCT_FLD(info, &i_s_info),

	/* plugin name */
	/* const char* */
	STRUCT_FLD(name, "INNODB_SYS_INDEXES"),

	/* plugin author (for SHOW PLUGINS) */
	/* const char* */
	STRUCT_FLD(author, "Percona"),

	/* general descriptive text (for SHOW PLUGINS) */
	/* const char* */
	STRUCT_FLD(descr, "InnoDB SYS_INDEXES"),

	/* the plugin license (PLUGIN_LICENSE_XXX) */
	/* int */
	STRUCT_FLD(license, PLUGIN_LICENSE_GPL),

	/* the function to invoke when plugin is loaded */
	/* int (*)(void*); */
	STRUCT_FLD(init, innodb_sys_indexes_init),

	/* the function to invoke when plugin is unloaded */
	/* int (*)(void*); */
	STRUCT_FLD(deinit, i_s_common_deinit),

	/* plugin version (for SHOW PLUGINS) */
	/* unsigned int */
	STRUCT_FLD(version, INNODB_VERSION_SHORT),

	/* struct st_mysql_show_var* */
	STRUCT_FLD(status_vars, NULL),

	/* struct st_mysql_sys_var** */
	STRUCT_FLD(system_vars, NULL),

	/* reserved for dependency checking */
	/* void* */
	STRUCT_FLD(__reserved1, NULL),

	/* flags for plugin */
	/* unsigned long */
	STRUCT_FLD(flags, 0UL)
};

/* Fields of the dynamic table INFORMATION_SCHEMA.SYS_COLUMNS */
static ST_FIELD_INFO    innodb_sys_columns_fields_info[] =
{
#define SYS_COLUMN_TABLE_ID		0
	{STRUCT_FLD(field_name,		"TABLE_ID"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define SYS_COLUMN_NAME		1
	{STRUCT_FLD(field_name,		"NAME"),
	 STRUCT_FLD(field_length,	NAME_LEN + 1),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_STRING),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	0),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define SYS_COLUMN_POSITION	2
	{STRUCT_FLD(field_name,		"POS"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define SYS_COLUMN_MTYPE		3
	{STRUCT_FLD(field_name,		"MTYPE"),
	 STRUCT_FLD(field_length,	MY_INT32_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	0),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define SYS_COLUMN__PRTYPE	4
	{STRUCT_FLD(field_name,		"PRTYPE"),
	 STRUCT_FLD(field_length,	MY_INT32_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	0),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define SYS_COLUMN_COLUMN_LEN	5
	{STRUCT_FLD(field_name,		"LEN"),
	 STRUCT_FLD(field_length,	MY_INT32_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	0),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	END_OF_ST_FIELD_INFO
};

/**********************************************************************//**
Function to populate the information_schema.innodb_sys_columns with
related column information
@return 0 on success */
static
int
i_s_dict_fill_sys_columns(
/*======================*/
	THD*		thd,		/*!< in: thread */
	table_id_t	table_id,	/*!< in: table ID */
	const char*	col_name,	/*!< in: column name */
	dict_col_t*	column,		/*!< in: dict_col_t struct holding
					more column information */
	TABLE*		table_to_fill)  /*!< in/out: fill this table */
{
	Field**		fields;

	DBUG_ENTER("i_s_dict_fill_sys_columns");

	fields = table_to_fill->field;

	OK(fields[SYS_COLUMN_TABLE_ID]->store(longlong(table_id), TRUE));

	OK(field_store_string(fields[SYS_COLUMN_NAME], col_name));

	OK(fields[SYS_COLUMN_POSITION]->store(column->ind));

	OK(fields[SYS_COLUMN_MTYPE]->store(column->mtype));

	OK(fields[SYS_COLUMN__PRTYPE]->store(column->prtype));

	OK(fields[SYS_COLUMN_COLUMN_LEN]->store(column->len));

	OK(schema_table_store_record(thd, table_to_fill));

	DBUG_RETURN(0);
}
/*******************************************************************//**
Function to fill information_schema.innodb_sys_columns with information
collected by scanning SYS_COLUMNS table.
@return 0 on success */
static
int
i_s_sys_columns_fill_table(
/*=======================*/
	THD*		thd,    /*!< in: thread */
	TABLE_LIST*	tables, /*!< in/out: tables to fill */
	COND*		cond)   /*!< in: condition (not used) */
{
        btr_pcur_t	pcur;
	const rec_t*	rec;
	const char*	col_name;
	mem_heap_t*	heap;
	mtr_t		mtr;

	DBUG_ENTER("i_s_sys_columns_fill_table");

	RETURN_IF_INNODB_NOT_STARTED(tables->schema_table_name);

	/* deny access to non-superusers */
	if (check_global_access(thd, PROCESS_ACL)) {

                DBUG_RETURN(0);
	}

        heap = mem_heap_create(1000);
        mutex_enter(&dict_sys->mutex);
        mtr_start(&mtr);

	rec = dict_startscan_system(&pcur, &mtr, SYS_COLUMNS);

	while (rec) {
		const char*	err_msg;
		dict_col_t	column_rec;
		table_id_t	table_id;

		/* populate a dict_col_t structure with information from
		a SYS_COLUMNS row */
		err_msg = dict_process_sys_columns_rec(heap, rec, &column_rec,
						       &table_id, &col_name);

		mtr_commit(&mtr);
		mutex_exit(&dict_sys->mutex);

		if (!err_msg) {
			i_s_dict_fill_sys_columns(thd, table_id, col_name,
						 &column_rec,
						 tables->table);
		} else {
			push_warning_printf(thd, MYSQL_ERROR::WARN_LEVEL_WARN,
					    ER_CANT_FIND_SYSTEM_REC,
					    err_msg);
		}

		mem_heap_empty(heap);

		/* Get the next record */
		mutex_enter(&dict_sys->mutex);
		mtr_start(&mtr);
		rec = dict_getnext_system(&pcur, &mtr);
	}

	mtr_commit(&mtr);
	mutex_exit(&dict_sys->mutex);
	mem_heap_free(heap);

	DBUG_RETURN(0);
}
/*******************************************************************//**
Bind the dynamic table INFORMATION_SCHEMA.innodb_sys_columns
@return 0 on success */
static
int
innodb_sys_columns_init(
/*====================*/
        void*   p)      /*!< in/out: table schema object */
{
        ST_SCHEMA_TABLE*        schema;

        DBUG_ENTER("innodb_sys_columns_init");

        schema = (ST_SCHEMA_TABLE*) p;

        schema->fields_info = innodb_sys_columns_fields_info;
        schema->fill_table = i_s_sys_columns_fill_table;

        DBUG_RETURN(0);
}

UNIV_INTERN struct st_mysql_plugin	i_s_innodb_sys_columns =
{
	/* the plugin type (a MYSQL_XXX_PLUGIN value) */
	/* int */
	STRUCT_FLD(type, MYSQL_INFORMATION_SCHEMA_PLUGIN),

	/* pointer to type-specific plugin descriptor */
	/* void* */
	STRUCT_FLD(info, &i_s_info),

	/* plugin name */
	/* const char* */
	STRUCT_FLD(name, "INNODB_SYS_COLUMNS"),

	/* plugin author (for SHOW PLUGINS) */
	/* const char* */
	STRUCT_FLD(author, "Percona"),

	/* general descriptive text (for SHOW PLUGINS) */
	/* const char* */
	STRUCT_FLD(descr, "InnoDB SYS_COLUMNS"),

	/* the plugin license (PLUGIN_LICENSE_XXX) */
	/* int */
	STRUCT_FLD(license, PLUGIN_LICENSE_GPL),

	/* the function to invoke when plugin is loaded */
	/* int (*)(void*); */
	STRUCT_FLD(init, innodb_sys_columns_init),

	/* the function to invoke when plugin is unloaded */
	/* int (*)(void*); */
	STRUCT_FLD(deinit, i_s_common_deinit),

	/* plugin version (for SHOW PLUGINS) */
	/* unsigned int */
	STRUCT_FLD(version, INNODB_VERSION_SHORT),

	/* struct st_mysql_show_var* */
	STRUCT_FLD(status_vars, NULL),

	/* struct st_mysql_sys_var** */
	STRUCT_FLD(system_vars, NULL),

	/* reserved for dependency checking */
	/* void* */
	STRUCT_FLD(__reserved1, NULL),

	/* flags for plugin */
	/* unsigned long */
	STRUCT_FLD(flags, 0UL)
};
/* Fields of the dynamic table INFORMATION_SCHEMA.innodb_sys_fields */
static ST_FIELD_INFO    innodb_sys_fields_fields_info[] =
{
#define SYS_FIELD_INDEX_ID	0
	{STRUCT_FLD(field_name,		"INDEX_ID"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define SYS_FIELD_NAME		1
	{STRUCT_FLD(field_name,		"NAME"),
	 STRUCT_FLD(field_length,	NAME_LEN + 1),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_STRING),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	0),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define SYS_FIELD_POS		2
	{STRUCT_FLD(field_name,		"POS"),
	 STRUCT_FLD(field_length,	MY_INT32_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	END_OF_ST_FIELD_INFO
};

/**********************************************************************//**
Function to fill information_schema.innodb_sys_fields with information
collected by scanning SYS_FIELDS table.
@return 0 on success */
static
int
i_s_dict_fill_sys_fields(
/*=====================*/
	THD*		thd,		/*!< in: thread */
	index_id_t	index_id,	/*!< in: index id for the field */
	dict_field_t*	field,		/*!< in: table */
	ulint		pos,		/*!< in: Field position */
	TABLE*		table_to_fill)  /*!< in/out: fill this table */
{
	Field**		fields;

	DBUG_ENTER("i_s_dict_fill_sys_fields");

	fields = table_to_fill->field;

	OK(fields[SYS_FIELD_INDEX_ID]->store(longlong(index_id), TRUE));

	OK(field_store_string(fields[SYS_FIELD_NAME], field->name));

	OK(fields[SYS_FIELD_POS]->store(pos));

	OK(schema_table_store_record(thd, table_to_fill));

	DBUG_RETURN(0);
}
/*******************************************************************//**
Function to go through each record in SYS_FIELDS table, and fill the
information_schema.innodb_sys_fields table with related index field
information
@return 0 on success */
static
int
i_s_sys_fields_fill_table(
/*======================*/
	THD*		thd,    /*!< in: thread */
	TABLE_LIST*	tables, /*!< in/out: tables to fill */
	COND*		cond)   /*!< in: condition (not used) */
{
        btr_pcur_t	pcur;
	const rec_t*	rec;
	mem_heap_t*	heap;
	index_id_t	last_id;
	mtr_t		mtr;

	DBUG_ENTER("i_s_sys_fields_fill_table");

	RETURN_IF_INNODB_NOT_STARTED(tables->schema_table_name);

	/* deny access to non-superusers */
	if (check_global_access(thd, PROCESS_ACL)) {

                DBUG_RETURN(0);
	}

        heap = mem_heap_create(1000);
        mutex_enter(&dict_sys->mutex);
        mtr_start(&mtr);

	/* will save last index id so that we know whether we move to
	the next index. This is used to calculate prefix length */
	last_id = 0;

	rec = dict_startscan_system(&pcur, &mtr, SYS_FIELDS);

	while (rec) {
		ulint		pos;
		const char*	err_msg;
		index_id_t	index_id;
		dict_field_t	field_rec;

		/* Populate a dict_field_t structure with information from
		a SYS_FIELDS row */
		err_msg = dict_process_sys_fields_rec(heap, rec, &field_rec,
						      &pos, &index_id, last_id);

		mtr_commit(&mtr);
		mutex_exit(&dict_sys->mutex);

		if (!err_msg) {
			i_s_dict_fill_sys_fields(thd, index_id, &field_rec,
						 pos, tables->table);
			last_id = index_id;
		} else {
			push_warning_printf(thd, MYSQL_ERROR::WARN_LEVEL_WARN,
					    ER_CANT_FIND_SYSTEM_REC,
					    err_msg);
		}

		mem_heap_empty(heap);

		/* Get the next record */
		mutex_enter(&dict_sys->mutex);
		mtr_start(&mtr);
		rec = dict_getnext_system(&pcur, &mtr);
	}

	mtr_commit(&mtr);
	mutex_exit(&dict_sys->mutex);
	mem_heap_free(heap);

	DBUG_RETURN(0);
}
/*******************************************************************//**
Bind the dynamic table INFORMATION_SCHEMA.innodb_sys_fields
@return 0 on success */
static
int
innodb_sys_fields_init(
/*===================*/
        void*   p)      /*!< in/out: table schema object */
{
        ST_SCHEMA_TABLE*        schema;

        DBUG_ENTER("innodb_sys_field_init");

        schema = (ST_SCHEMA_TABLE*) p;

        schema->fields_info = innodb_sys_fields_fields_info;
        schema->fill_table = i_s_sys_fields_fill_table;

        DBUG_RETURN(0);
}

UNIV_INTERN struct st_mysql_plugin	i_s_innodb_sys_fields =
{
	/* the plugin type (a MYSQL_XXX_PLUGIN value) */
	/* int */
	STRUCT_FLD(type, MYSQL_INFORMATION_SCHEMA_PLUGIN),

	/* pointer to type-specific plugin descriptor */
	/* void* */
	STRUCT_FLD(info, &i_s_info),

	/* plugin name */
	/* const char* */
	STRUCT_FLD(name, "INNODB_SYS_FIELDS"),

	/* plugin author (for SHOW PLUGINS) */
	/* const char* */
	STRUCT_FLD(author, "Percona"),

	/* general descriptive text (for SHOW PLUGINS) */
	/* const char* */
	STRUCT_FLD(descr, "InnoDB SYS_FIELDS"),

	/* the plugin license (PLUGIN_LICENSE_XXX) */
	/* int */
	STRUCT_FLD(license, PLUGIN_LICENSE_GPL),

	/* the function to invoke when plugin is loaded */
	/* int (*)(void*); */
	STRUCT_FLD(init, innodb_sys_fields_init),

	/* the function to invoke when plugin is unloaded */
	/* int (*)(void*); */
	STRUCT_FLD(deinit, i_s_common_deinit),

	/* plugin version (for SHOW PLUGINS) */
	/* unsigned int */
	STRUCT_FLD(version, INNODB_VERSION_SHORT),

	/* struct st_mysql_show_var* */
	STRUCT_FLD(status_vars, NULL),

	/* struct st_mysql_sys_var** */
	STRUCT_FLD(system_vars, NULL),

	/* reserved for dependency checking */
	/* void* */
	STRUCT_FLD(__reserved1, NULL),

	/* flags for plugin */
	/* unsigned long */
	STRUCT_FLD(flags, 0UL)
};

/* Fields of the dynamic table INFORMATION_SCHEMA.innodb_sys_foreign */
static ST_FIELD_INFO    innodb_sys_foreign_fields_info[] =
{
#define SYS_FOREIGN_ID		0
	{STRUCT_FLD(field_name,		"ID"),
	 STRUCT_FLD(field_length,	NAME_LEN + 1),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_STRING),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	0),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define SYS_FOREIGN_FOR_NAME	1
	{STRUCT_FLD(field_name,		"FOR_NAME"),
	 STRUCT_FLD(field_length,	NAME_LEN + 1),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_STRING),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	0),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define SYS_FOREIGN_REF_NAME	2
	{STRUCT_FLD(field_name,		"REF_NAME"),
	 STRUCT_FLD(field_length,	NAME_LEN + 1),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_STRING),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	0),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define SYS_FOREIGN_NUM_COL	3
	{STRUCT_FLD(field_name,		"N_COLS"),
	 STRUCT_FLD(field_length,	MY_INT32_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define SYS_FOREIGN_TYPE	4
	{STRUCT_FLD(field_name,		"TYPE"),
	 STRUCT_FLD(field_length,	MY_INT32_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	END_OF_ST_FIELD_INFO
};

/**********************************************************************//**
Function to fill information_schema.innodb_sys_foreign with information
collected by scanning SYS_FOREIGN table.
@return 0 on success */
static
int
i_s_dict_fill_sys_foreign(
/*======================*/
	THD*		thd,		/*!< in: thread */
	dict_foreign_t*	foreign,	/*!< in: table */
	TABLE*		table_to_fill)  /*!< in/out: fill this table */
{
	Field**		fields;

	DBUG_ENTER("i_s_dict_fill_sys_foreign");

	fields = table_to_fill->field;

	OK(field_store_string(fields[SYS_FOREIGN_ID], foreign->id));

	OK(field_store_string(fields[SYS_FOREIGN_FOR_NAME],
			      foreign->foreign_table_name));

	OK(field_store_string(fields[SYS_FOREIGN_REF_NAME],
			      foreign->referenced_table_name));

	OK(fields[SYS_FOREIGN_NUM_COL]->store(foreign->n_fields));

	OK(fields[SYS_FOREIGN_TYPE]->store(foreign->type));

	OK(schema_table_store_record(thd, table_to_fill));

	DBUG_RETURN(0);
}
/*******************************************************************//**
Function to populate INFORMATION_SCHEMA.innodb_sys_foreign table. Loop
through each record in SYS_FOREIGN, and extract the foreign key
information.
@return 0 on success */
static
int
i_s_sys_foreign_fill_table(
/*=======================*/
	THD*		thd,    /*!< in: thread */
	TABLE_LIST*	tables, /*!< in/out: tables to fill */
	COND*		cond)   /*!< in: condition (not used) */
{
        btr_pcur_t	pcur;
	const rec_t*	rec;
	mem_heap_t*	heap;
	mtr_t		mtr;

	DBUG_ENTER("i_s_sys_foreign_fill_table");

	RETURN_IF_INNODB_NOT_STARTED(tables->schema_table_name);

	/* deny access to non-superusers */
	if (check_global_access(thd, PROCESS_ACL)) {

                DBUG_RETURN(0);
	}

        heap = mem_heap_create(1000);
        mutex_enter(&dict_sys->mutex);
        mtr_start(&mtr);

	rec = dict_startscan_system(&pcur, &mtr, SYS_FOREIGN);

	while (rec) {
		const char*	err_msg;
		dict_foreign_t	foreign_rec;

		/* Populate a dict_foreign_t structure with information from
		a SYS_FOREIGN row */
		err_msg = dict_process_sys_foreign_rec(heap, rec, &foreign_rec);

		mtr_commit(&mtr);
		mutex_exit(&dict_sys->mutex);

		if (!err_msg) {
			i_s_dict_fill_sys_foreign(thd, &foreign_rec,
						 tables->table);
		} else {
			push_warning_printf(thd, MYSQL_ERROR::WARN_LEVEL_WARN,
					    ER_CANT_FIND_SYSTEM_REC,
					    err_msg);
		}

		mem_heap_empty(heap);

		/* Get the next record */
		mtr_start(&mtr);
		mutex_enter(&dict_sys->mutex);
		rec = dict_getnext_system(&pcur, &mtr);
	}

	mtr_commit(&mtr);
	mutex_exit(&dict_sys->mutex);
	mem_heap_free(heap);

	DBUG_RETURN(0);
}
/*******************************************************************//**
Bind the dynamic table INFORMATION_SCHEMA.innodb_sys_foreign
@return 0 on success */
static
int
innodb_sys_foreign_init(
/*====================*/
        void*   p)      /*!< in/out: table schema object */
{
        ST_SCHEMA_TABLE*        schema;

        DBUG_ENTER("innodb_sys_foreign_init");

        schema = (ST_SCHEMA_TABLE*) p;

        schema->fields_info = innodb_sys_foreign_fields_info;
        schema->fill_table = i_s_sys_foreign_fill_table;

        DBUG_RETURN(0);
}

UNIV_INTERN struct st_mysql_plugin	i_s_innodb_sys_foreign =
{
	/* the plugin type (a MYSQL_XXX_PLUGIN value) */
	/* int */
	STRUCT_FLD(type, MYSQL_INFORMATION_SCHEMA_PLUGIN),

	/* pointer to type-specific plugin descriptor */
	/* void* */
	STRUCT_FLD(info, &i_s_info),

	/* plugin name */
	/* const char* */
	STRUCT_FLD(name, "INNODB_SYS_FOREIGN"),

	/* plugin author (for SHOW PLUGINS) */
	/* const char* */
	STRUCT_FLD(author, "Percona"),

	/* general descriptive text (for SHOW PLUGINS) */
	/* const char* */
	STRUCT_FLD(descr, "InnoDB SYS_FOREIGN"),

	/* the plugin license (PLUGIN_LICENSE_XXX) */
	/* int */
	STRUCT_FLD(license, PLUGIN_LICENSE_GPL),

	/* the function to invoke when plugin is loaded */
	/* int (*)(void*); */
	STRUCT_FLD(init, innodb_sys_foreign_init),

	/* the function to invoke when plugin is unloaded */
	/* int (*)(void*); */
	STRUCT_FLD(deinit, i_s_common_deinit),

	/* plugin version (for SHOW PLUGINS) */
	/* unsigned int */
	STRUCT_FLD(version, INNODB_VERSION_SHORT),

	/* struct st_mysql_show_var* */
	STRUCT_FLD(status_vars, NULL),

	/* struct st_mysql_sys_var** */
	STRUCT_FLD(system_vars, NULL),

	/* reserved for dependency checking */
	/* void* */
	STRUCT_FLD(__reserved1, NULL),

	/* flags for plugin */
	/* unsigned long */
	STRUCT_FLD(flags, 0UL)
};
/* Fields of the dynamic table INFORMATION_SCHEMA.innodb_sys_foreign_cols */
static ST_FIELD_INFO    innodb_sys_foreign_cols_fields_info[] =
{
#define SYS_FOREIGN_COL_ID		0
	{STRUCT_FLD(field_name,		"ID"),
	 STRUCT_FLD(field_length,	NAME_LEN + 1),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_STRING),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	0),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define SYS_FOREIGN_COL_FOR_NAME	1
	{STRUCT_FLD(field_name,		"FOR_COL_NAME"),
	 STRUCT_FLD(field_length,	NAME_LEN + 1),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_STRING),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	0),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define SYS_FOREIGN_COL_REF_NAME	2
	{STRUCT_FLD(field_name,		"REF_COL_NAME"),
	 STRUCT_FLD(field_length,	NAME_LEN + 1),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_STRING),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	0),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define SYS_FOREIGN_COL_POS		3
	{STRUCT_FLD(field_name,		"POS"),
	 STRUCT_FLD(field_length,	MY_INT32_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	END_OF_ST_FIELD_INFO
};

/**********************************************************************//**
Function to fill information_schema.innodb_sys_foreign_cols with information
collected by scanning SYS_FOREIGN_COLS table.
@return 0 on success */
static
int
i_s_dict_fill_sys_foreign_cols(
/*==========================*/
	THD*		thd,		/*!< in: thread */
	const char*	name,		/*!< in: foreign key constraint name */
	const char*	for_col_name,	/*!< in: referencing column name*/
	const char*	ref_col_name,	/*!< in: referenced column
					name */
	ulint		pos,		/*!< in: column position */
	TABLE*		table_to_fill)  /*!< in/out: fill this table */
{
	Field**		fields;

	DBUG_ENTER("i_s_dict_fill_sys_foreign_cols");

	fields = table_to_fill->field;

	OK(field_store_string(fields[SYS_FOREIGN_COL_ID], name));

	OK(field_store_string(fields[SYS_FOREIGN_COL_FOR_NAME], for_col_name));

	OK(field_store_string(fields[SYS_FOREIGN_COL_REF_NAME], ref_col_name));

	OK(fields[SYS_FOREIGN_COL_POS]->store(pos));

	OK(schema_table_store_record(thd, table_to_fill));

	DBUG_RETURN(0);
}
/*******************************************************************//**
Function to populate INFORMATION_SCHEMA.innodb_sys_foreign_cols table. Loop
through each record in SYS_FOREIGN_COLS, and extract the foreign key column
information and fill the INFORMATION_SCHEMA.innodb_sys_foreign_cols table.
@return 0 on success */
static
int
i_s_sys_foreign_cols_fill_table(
/*============================*/
	THD*		thd,    /*!< in: thread */
	TABLE_LIST*	tables, /*!< in/out: tables to fill */
	COND*		cond)   /*!< in: condition (not used) */
{
        btr_pcur_t	pcur;
	const rec_t*	rec;
	mem_heap_t*	heap;
	mtr_t		mtr;

	DBUG_ENTER("i_s_sys_foreign_cols_fill_table");

	RETURN_IF_INNODB_NOT_STARTED(tables->schema_table_name);

	/* deny access to non-superusers */
	if (check_global_access(thd, PROCESS_ACL)) {
                DBUG_RETURN(0);
	}

        heap = mem_heap_create(1000);
        mutex_enter(&dict_sys->mutex);
        mtr_start(&mtr);

	rec = dict_startscan_system(&pcur, &mtr, SYS_FOREIGN_COLS);

	while (rec) {
		const char*	err_msg;
		const char*	name;
		const char*	for_col_name;
		const char*	ref_col_name;
		ulint		pos;

		/* Extract necessary information from a SYS_FOREIGN_COLS row */
		err_msg = dict_process_sys_foreign_col_rec(
			heap, rec, &name, &for_col_name, &ref_col_name, &pos);

		mtr_commit(&mtr);
		mutex_exit(&dict_sys->mutex);

		if (!err_msg) {
			i_s_dict_fill_sys_foreign_cols(
				thd, name, for_col_name, ref_col_name, pos,
				tables->table);
		} else {
			push_warning_printf(thd, MYSQL_ERROR::WARN_LEVEL_WARN,
					    ER_CANT_FIND_SYSTEM_REC,
					    err_msg);
		}

		mem_heap_empty(heap);

		/* Get the next record */
		mutex_enter(&dict_sys->mutex);
		mtr_start(&mtr);
		rec = dict_getnext_system(&pcur, &mtr);
	}

	mtr_commit(&mtr);
	mutex_exit(&dict_sys->mutex);
	mem_heap_free(heap);

	DBUG_RETURN(0);
}
/*******************************************************************//**
Bind the dynamic table INFORMATION_SCHEMA.innodb_sys_foreign_cols
@return 0 on success */
static
int
innodb_sys_foreign_cols_init(
/*========================*/
        void*   p)      /*!< in/out: table schema object */
{
        ST_SCHEMA_TABLE*        schema;

        DBUG_ENTER("innodb_sys_foreign_cols_init");

        schema = (ST_SCHEMA_TABLE*) p;

        schema->fields_info = innodb_sys_foreign_cols_fields_info;
        schema->fill_table = i_s_sys_foreign_cols_fill_table;

        DBUG_RETURN(0);
}

UNIV_INTERN struct st_mysql_plugin	i_s_innodb_sys_foreign_cols =
{
	/* the plugin type (a MYSQL_XXX_PLUGIN value) */
	/* int */
	STRUCT_FLD(type, MYSQL_INFORMATION_SCHEMA_PLUGIN),

	/* pointer to type-specific plugin descriptor */
	/* void* */
	STRUCT_FLD(info, &i_s_info),

	/* plugin name */
	/* const char* */
	STRUCT_FLD(name, "INNODB_SYS_FOREIGN_COLS"),

	/* plugin author (for SHOW PLUGINS) */
	/* const char* */
	STRUCT_FLD(author, "Percona"),

	/* general descriptive text (for SHOW PLUGINS) */
	/* const char* */
	STRUCT_FLD(descr, "InnoDB SYS_FOREIGN_COLS"),

	/* the plugin license (PLUGIN_LICENSE_XXX) */
	/* int */
	STRUCT_FLD(license, PLUGIN_LICENSE_GPL),

	/* the function to invoke when plugin is loaded */
	/* int (*)(void*); */
	STRUCT_FLD(init, innodb_sys_foreign_cols_init),

	/* the function to invoke when plugin is unloaded */
	/* int (*)(void*); */
	STRUCT_FLD(deinit, i_s_common_deinit),

	/* plugin version (for SHOW PLUGINS) */
	/* unsigned int */
	STRUCT_FLD(version, INNODB_VERSION_SHORT),

	/* struct st_mysql_show_var* */
	STRUCT_FLD(status_vars, NULL),

	/* struct st_mysql_sys_var** */
	STRUCT_FLD(system_vars, NULL),

	/* reserved for dependency checking */
	/* void* */
	STRUCT_FLD(__reserved1, NULL),

	/* flags for plugin */
	/* unsigned long */
	STRUCT_FLD(flags, 0UL)
};

/* Fields of the dynamic table INFORMATION_SCHEMA.innodb_sys_stats */
static ST_FIELD_INFO	innodb_sys_stats_fields_info[] =
{
#define SYS_STATS_INDEX_ID	0
	{STRUCT_FLD(field_name,		"INDEX_ID"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define SYS_STATS_KEY_COLS	1
	{STRUCT_FLD(field_name,		"KEY_COLS"),
	 STRUCT_FLD(field_length,	MY_INT32_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define SYS_STATS_DIFF_VALS	2
	{STRUCT_FLD(field_name,		"DIFF_VALS"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define SYS_STATS_NON_NULL_VALS	3
	{STRUCT_FLD(field_name,		"NON_NULL_VALS"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED | MY_I_S_MAYBE_NULL),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	END_OF_ST_FIELD_INFO
};
/**********************************************************************//**
Function to fill information_schema.innodb_sys_stats
@return 0 on success */
static
int
i_s_dict_fill_sys_stats(
/*====================*/
	THD*		thd,		/*!< in: thread */
	index_id_t	index_id,	/*!< in: INDEX_ID */
	ulint		key_cols,	/*!< in: KEY_COLS */
	ib_uint64_t	diff_vals,	/*!< in: DIFF_VALS */
	ib_uint64_t	non_null_vals,	/*!< in: NON_NULL_VALS */
	TABLE*		table_to_fill)  /*!< in/out: fill this table */
{
	Field**		fields;

	DBUG_ENTER("i_s_dict_fill_sys_stats");

	fields = table_to_fill->field;

	OK(fields[SYS_STATS_INDEX_ID]->store(longlong(index_id), TRUE));

	OK(fields[SYS_STATS_KEY_COLS]->store(key_cols));

	OK(fields[SYS_STATS_DIFF_VALS]->store(longlong(diff_vals), TRUE));

	if (non_null_vals == ((ib_uint64_t)(-1))) {
		fields[SYS_STATS_NON_NULL_VALS]->set_null();
	} else {
		OK(fields[SYS_STATS_NON_NULL_VALS]->store(longlong(non_null_vals), TRUE));
		fields[SYS_STATS_NON_NULL_VALS]->set_notnull();
	}

	OK(schema_table_store_record(thd, table_to_fill));

	DBUG_RETURN(0);
}
/*******************************************************************//**
Function to populate INFORMATION_SCHEMA.innodb_sys_stats table.
@return 0 on success */
static
int
i_s_sys_stats_fill_table(
/*=====================*/
	THD*		thd,    /*!< in: thread */
	TABLE_LIST*	tables, /*!< in/out: tables to fill */
	COND*		cond)   /*!< in: condition (not used) */
{
        btr_pcur_t	pcur;
	const rec_t*	rec;
	mem_heap_t*	heap;
	mtr_t		mtr;

	DBUG_ENTER("i_s_sys_stats_fill_table");

	RETURN_IF_INNODB_NOT_STARTED(tables->schema_table_name);

	/* deny access to non-superusers */
	if (check_global_access(thd, PROCESS_ACL)) {
                DBUG_RETURN(0);
	}

        heap = mem_heap_create(1000);
        mutex_enter(&dict_sys->mutex);
        mtr_start(&mtr);

	rec = dict_startscan_system(&pcur, &mtr, SYS_STATS);

	while (rec) {
		const char*	err_msg;
		index_id_t	index_id;
		ulint		key_cols;
		ib_uint64_t	diff_vals;
		ib_uint64_t	non_null_vals;

		/* Extract necessary information from a SYS_FOREIGN_COLS row */
		err_msg = dict_process_sys_stats_rec(
			heap, rec, &index_id, &key_cols, &diff_vals, &non_null_vals);

		mtr_commit(&mtr);
		mutex_exit(&dict_sys->mutex);

		if (!err_msg) {
			i_s_dict_fill_sys_stats(
				thd, index_id, key_cols, diff_vals, non_null_vals,
				tables->table);
		} else {
			push_warning_printf(thd, MYSQL_ERROR::WARN_LEVEL_WARN,
					    ER_CANT_FIND_SYSTEM_REC,
					    err_msg);
		}

		mem_heap_empty(heap);

		/* Get the next record */
		mutex_enter(&dict_sys->mutex);
		mtr_start(&mtr);
		rec = dict_getnext_system(&pcur, &mtr);
	}

	mtr_commit(&mtr);
	mutex_exit(&dict_sys->mutex);
	mem_heap_free(heap);

	DBUG_RETURN(0);
}
/*******************************************************************//**
Bind the dynamic table INFORMATION_SCHEMA.innodb_sys_stats
@return 0 on success */
static
int
innodb_sys_stats_init(
/*========================*/
        void*   p)      /*!< in/out: table schema object */
{
        ST_SCHEMA_TABLE*        schema;

        DBUG_ENTER("innodb_sys_stats_init");

        schema = (ST_SCHEMA_TABLE*) p;

        schema->fields_info = innodb_sys_stats_fields_info;
        schema->fill_table = i_s_sys_stats_fill_table;

        DBUG_RETURN(0);
}

UNIV_INTERN struct st_mysql_plugin	i_s_innodb_sys_stats =
{
	/* the plugin type (a MYSQL_XXX_PLUGIN value) */
	/* int */
	STRUCT_FLD(type, MYSQL_INFORMATION_SCHEMA_PLUGIN),

	/* pointer to type-specific plugin descriptor */
	/* void* */
	STRUCT_FLD(info, &i_s_info),

	/* plugin name */
	/* const char* */
	STRUCT_FLD(name, "INNODB_SYS_STATS"),

	/* plugin author (for SHOW PLUGINS) */
	/* const char* */
	STRUCT_FLD(author, "Percona"),

	/* general descriptive text (for SHOW PLUGINS) */
	/* const char* */
	STRUCT_FLD(descr, "XtraDB SYS_STATS table"),

	/* the plugin license (PLUGIN_LICENSE_XXX) */
	/* int */
	STRUCT_FLD(license, PLUGIN_LICENSE_GPL),

	/* the function to invoke when plugin is loaded */
	/* int (*)(void*); */
	STRUCT_FLD(init, innodb_sys_stats_init),

	/* the function to invoke when plugin is unloaded */
	/* int (*)(void*); */
	STRUCT_FLD(deinit, i_s_common_deinit),

	/* plugin version (for SHOW PLUGINS) */
	/* unsigned int */
	STRUCT_FLD(version, INNODB_VERSION_SHORT),

	/* struct st_mysql_show_var* */
	STRUCT_FLD(status_vars, NULL),

	/* struct st_mysql_sys_var** */
	STRUCT_FLD(system_vars, NULL),

	/* reserved for dependency checking */
	/* void* */
	STRUCT_FLD(__reserved1, NULL),

	/* flags for plugin */
	/* unsigned long */
	STRUCT_FLD(flags, 0UL)
};

/***********************************************************************
*/
static ST_FIELD_INFO	i_s_innodb_rseg_fields_info[] =
{
	{STRUCT_FLD(field_name,		"rseg_id"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	{STRUCT_FLD(field_name,		"space_id"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	{STRUCT_FLD(field_name,		"zip_size"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	{STRUCT_FLD(field_name,		"page_no"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	{STRUCT_FLD(field_name,		"max_size"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	{STRUCT_FLD(field_name,		"curr_size"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	END_OF_ST_FIELD_INFO
};

static
int
i_s_innodb_rseg_fill(
/*=================*/
	THD*		thd,	/* in: thread */
	TABLE_LIST*	tables,	/* in/out: tables to fill */
	COND*		cond)	/* in: condition (ignored) */
{
	TABLE*	table	= (TABLE *) tables->table;
	int	status	= 0;
	trx_rseg_t*	rseg;

	DBUG_ENTER("i_s_innodb_rseg_fill");

	/* deny access to non-superusers */
	if (check_global_access(thd, PROCESS_ACL)) {

		DBUG_RETURN(0);
	}

	RETURN_IF_INNODB_NOT_STARTED(tables->schema_table_name);

	rseg = UT_LIST_GET_FIRST(trx_sys->rseg_list);

	while (rseg) {
		table->field[0]->store(rseg->id);
		table->field[1]->store(rseg->space);
		table->field[2]->store(rseg->zip_size);
		table->field[3]->store(rseg->page_no);
		table->field[4]->store(rseg->max_size);
		table->field[5]->store(rseg->curr_size);

		if (schema_table_store_record(thd, table)) {
			status = 1;
			break;
		}

		rseg = UT_LIST_GET_NEXT(rseg_list, rseg);
	}

	DBUG_RETURN(status);
}

static
int
i_s_innodb_rseg_init(
/*=================*/
			/* out: 0 on success */
	void*	p)	/* in/out: table schema object */
{
	DBUG_ENTER("i_s_innodb_rseg_init");
	ST_SCHEMA_TABLE* schema = (ST_SCHEMA_TABLE*) p;

	schema->fields_info = i_s_innodb_rseg_fields_info;
	schema->fill_table = i_s_innodb_rseg_fill;

	DBUG_RETURN(0);
}

UNIV_INTERN struct st_mysql_plugin	i_s_innodb_rseg =
{
	/* the plugin type (a MYSQL_XXX_PLUGIN value) */
	/* int */
	STRUCT_FLD(type, MYSQL_INFORMATION_SCHEMA_PLUGIN),

	/* pointer to type-specific plugin descriptor */
	/* void* */
	STRUCT_FLD(info, &i_s_info),

	/* plugin name */
	/* const char* */
	STRUCT_FLD(name, "INNODB_RSEG"),

	/* plugin author (for SHOW PLUGINS) */
	/* const char* */
	STRUCT_FLD(author, "Percona"),

	/* general descriptive text (for SHOW PLUGINS) */
	/* const char* */
	STRUCT_FLD(descr, "InnoDB rollback segment information"),

	/* the plugin license (PLUGIN_LICENSE_XXX) */
	/* int */
	STRUCT_FLD(license, PLUGIN_LICENSE_GPL),

	/* the function to invoke when plugin is loaded */
	/* int (*)(void*); */
	STRUCT_FLD(init, i_s_innodb_rseg_init),

	/* the function to invoke when plugin is unloaded */
	/* int (*)(void*); */
	STRUCT_FLD(deinit, i_s_common_deinit),

	/* plugin version (for SHOW PLUGINS) */
	/* unsigned int */
	STRUCT_FLD(version, 0x0100 /* 1.0 */),

	/* struct st_mysql_show_var* */
	STRUCT_FLD(status_vars, NULL),

	/* struct st_mysql_sys_var** */
	STRUCT_FLD(system_vars, NULL),

	/* reserved for dependency checking */
	/* void* */
	STRUCT_FLD(__reserved1, NULL),

	/* Plugin flags */
	/* unsigned long */
	STRUCT_FLD(flags, 0UL),
};

/***********************************************************************
*/
static ST_FIELD_INFO	i_s_innodb_table_stats_info[] =
{
	{STRUCT_FLD(field_name,		"table_schema"),
	 STRUCT_FLD(field_length,	NAME_LEN),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_STRING),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	0),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	{STRUCT_FLD(field_name,		"table_name"),
	 STRUCT_FLD(field_length,	NAME_LEN),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_STRING),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	0),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	{STRUCT_FLD(field_name,		"rows"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	{STRUCT_FLD(field_name,		"clust_size"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	{STRUCT_FLD(field_name,		"other_size"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	{STRUCT_FLD(field_name,		"modified"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	END_OF_ST_FIELD_INFO
};

static ST_FIELD_INFO	i_s_innodb_index_stats_info[] =
{
	{STRUCT_FLD(field_name,		"table_schema"),
	 STRUCT_FLD(field_length,	NAME_LEN),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_STRING),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	0),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	{STRUCT_FLD(field_name,		"table_name"),
	 STRUCT_FLD(field_length,	NAME_LEN),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_STRING),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	0),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	{STRUCT_FLD(field_name,		"index_name"),
	 STRUCT_FLD(field_length,	NAME_LEN),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_STRING),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	0),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	{STRUCT_FLD(field_name,		"fields"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	{STRUCT_FLD(field_name,		"rows_per_key"),
	 STRUCT_FLD(field_length,	256),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_STRING),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	0),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	{STRUCT_FLD(field_name,		"index_total_pages"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	{STRUCT_FLD(field_name,		"index_leaf_pages"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	END_OF_ST_FIELD_INFO
};

static
int
i_s_innodb_table_stats_fill(
/*========================*/
	THD*		thd,
	TABLE_LIST*	tables,
	COND*		cond)
{
	TABLE*	i_s_table	= (TABLE *) tables->table;
	int	status	= 0;
	dict_table_t*	table;

	DBUG_ENTER("i_s_innodb_table_stats_fill");

	RETURN_IF_INNODB_NOT_STARTED(tables->schema_table_name);

	/* deny access to non-superusers */
	if (check_global_access(thd, PROCESS_ACL)) {
		DBUG_RETURN(0);
	}

	mutex_enter(&(dict_sys->mutex));

	table = UT_LIST_GET_FIRST(dict_sys->table_LRU);

	while (table) {
		char	buf[NAME_LEN * 2 + 2];
		char*	ptr;

		if (table->stat_clustered_index_size == 0) {
			table = UT_LIST_GET_NEXT(table_LRU, table);
			continue;
		}

		buf[NAME_LEN * 2 + 1] = 0;
		strncpy(buf, table->name, NAME_LEN * 2 + 1);
		ptr = strchr(buf, '/');
		if (ptr) {
			*ptr = '\0';
			++ptr;
		} else {
			ptr = buf;
		}

		field_store_string(i_s_table->field[0], buf);
		field_store_string(i_s_table->field[1], ptr);
		i_s_table->field[2]->store(table->stat_n_rows);
		i_s_table->field[3]->store(table->stat_clustered_index_size);
		i_s_table->field[4]->store(table->stat_sum_of_other_index_sizes);
		i_s_table->field[5]->store(table->stat_modified_counter);

		if (schema_table_store_record(thd, i_s_table)) {
			status = 1;
			break;
		}

		table = UT_LIST_GET_NEXT(table_LRU, table);
	}

	mutex_exit(&(dict_sys->mutex));

	DBUG_RETURN(status);
}

static
int
i_s_innodb_index_stats_fill(
/*========================*/
	THD*		thd,
	TABLE_LIST*	tables,
	COND*		cond)
{
	TABLE*	i_s_table	= (TABLE *) tables->table;
	int	status	= 0;
	dict_table_t*	table;
	dict_index_t*	index;

	DBUG_ENTER("i_s_innodb_index_stats_fill");

	RETURN_IF_INNODB_NOT_STARTED(tables->schema_table_name);

	/* deny access to non-superusers */
	if (check_global_access(thd, PROCESS_ACL)) {
		DBUG_RETURN(0);
	}

	mutex_enter(&(dict_sys->mutex));

	table = UT_LIST_GET_FIRST(dict_sys->table_LRU);

	while (table) {
		if (table->stat_clustered_index_size == 0) {
			table = UT_LIST_GET_NEXT(table_LRU, table);
			continue;
		}

		ib_int64_t	n_rows = table->stat_n_rows;

		if (n_rows < 0) {
			n_rows = 0;
		}

		index = dict_table_get_first_index(table);

		while (index) {
			char	buff[256+1];
			char	row_per_keys[256+1];
			char	buf[NAME_LEN * 2 + 2];
			char*	ptr;
			ulint	i;

			buf[NAME_LEN * 2 + 1] = 0;
			strncpy(buf, table->name, NAME_LEN * 2 + 1);
			ptr = strchr(buf, '/');
			if (ptr) {
				*ptr = '\0';
				++ptr;
			} else {
				ptr = buf;
			}

			field_store_string(i_s_table->field[0], buf);
			field_store_string(i_s_table->field[1], ptr);
			field_store_string(i_s_table->field[2], index->name);
			i_s_table->field[3]->store(index->n_uniq);

			row_per_keys[0] = '\0';

			/* It is remained optimistic operation still for now */
			//dict_index_stat_mutex_enter(index);
			if (index->stat_n_diff_key_vals) {
				for (i = 1; i <= index->n_uniq; i++) {
					ib_int64_t	rec_per_key;
					if (index->stat_n_diff_key_vals[i]) {
						rec_per_key = n_rows / index->stat_n_diff_key_vals[i];
					} else {
						rec_per_key = n_rows;
					}
					ut_snprintf(buff, 256, (i == index->n_uniq)?"%llu":"%llu, ",
						 rec_per_key);
					strncat(row_per_keys, buff, 256 - strlen(row_per_keys));
				}
			}
			//dict_index_stat_mutex_exit(index);

			field_store_string(i_s_table->field[4], row_per_keys);

			i_s_table->field[5]->store(index->stat_index_size);
			i_s_table->field[6]->store(index->stat_n_leaf_pages);

			if (schema_table_store_record(thd, i_s_table)) {
				status = 1;
				break;
			}

			index = dict_table_get_next_index(index);
		}

		if (status == 1) {
			break;
		}

		table = UT_LIST_GET_NEXT(table_LRU, table);
	}

	mutex_exit(&(dict_sys->mutex));

	DBUG_RETURN(status);
}

static
int
i_s_innodb_table_stats_init(
/*========================*/
	void*   p)
{
	DBUG_ENTER("i_s_innodb_table_stats_init");
	ST_SCHEMA_TABLE* schema = (ST_SCHEMA_TABLE*) p;

	schema->fields_info = i_s_innodb_table_stats_info;
	schema->fill_table = i_s_innodb_table_stats_fill;

	DBUG_RETURN(0);
}

static
int
i_s_innodb_index_stats_init(
/*========================*/
	void*	p)
{
	DBUG_ENTER("i_s_innodb_index_stats_init");
	ST_SCHEMA_TABLE* schema = (ST_SCHEMA_TABLE*) p;

	schema->fields_info = i_s_innodb_index_stats_info;
	schema->fill_table = i_s_innodb_index_stats_fill;

	DBUG_RETURN(0);
}

UNIV_INTERN struct st_mysql_plugin	i_s_innodb_table_stats =
{
	STRUCT_FLD(type, MYSQL_INFORMATION_SCHEMA_PLUGIN),
	STRUCT_FLD(info, &i_s_info),
	STRUCT_FLD(name, "INNODB_TABLE_STATS"),
	STRUCT_FLD(author, "Percona"),
	STRUCT_FLD(descr, "InnoDB table statistics in memory"),
	STRUCT_FLD(license, PLUGIN_LICENSE_GPL),
	STRUCT_FLD(init, i_s_innodb_table_stats_init),
	STRUCT_FLD(deinit, i_s_common_deinit),
	STRUCT_FLD(version, 0x0100 /* 1.0 */),
	STRUCT_FLD(status_vars, NULL),
	STRUCT_FLD(system_vars, NULL),
	STRUCT_FLD(__reserved1, NULL),
	STRUCT_FLD(flags, 0UL)
};

UNIV_INTERN struct st_mysql_plugin	i_s_innodb_index_stats =
{
	STRUCT_FLD(type, MYSQL_INFORMATION_SCHEMA_PLUGIN),
	STRUCT_FLD(info, &i_s_info),
	STRUCT_FLD(name, "INNODB_INDEX_STATS"),
	STRUCT_FLD(author, "Percona"),
	STRUCT_FLD(descr, "InnoDB index statistics in memory"),
	STRUCT_FLD(license, PLUGIN_LICENSE_GPL),
	STRUCT_FLD(init, i_s_innodb_index_stats_init),
	STRUCT_FLD(deinit, i_s_common_deinit),
	STRUCT_FLD(version, 0x0100 /* 1.0 */),
	STRUCT_FLD(status_vars, NULL),
	STRUCT_FLD(system_vars, NULL),
	STRUCT_FLD(__reserved1, NULL),
	STRUCT_FLD(flags, 0UL)
};

/***********************************************************************
*/
static ST_FIELD_INFO	i_s_innodb_admin_command_info[] =
{
	{STRUCT_FLD(field_name,		"result_message"),
	 STRUCT_FLD(field_length,	1024),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_STRING),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	0),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	END_OF_ST_FIELD_INFO
};

#ifndef INNODB_COMPATIBILITY_HOOKS
#error InnoDB needs MySQL to be built with #define INNODB_COMPATIBILITY_HOOKS
#endif

extern "C" {
char **thd_query(MYSQL_THD thd);
}

static
int
i_s_innodb_admin_command_fill(
/*==========================*/
	THD*		thd,
	TABLE_LIST*	tables,
	COND*		cond)
{
	TABLE*	i_s_table	= (TABLE *) tables->table;
	char**	query_str;
	char*	ptr;
	char	quote	= '\0';
	const char*	command_head = "XTRA_";

	DBUG_ENTER("i_s_innodb_admin_command_fill");

	RETURN_IF_INNODB_NOT_STARTED(tables->schema_table_name);

	/* deny access to non-superusers */
	if (check_global_access(thd, PROCESS_ACL)) {
		DBUG_RETURN(0);
	}

	if(thd_sql_command(thd) != SQLCOM_SELECT) {
		field_store_string(i_s_table->field[0],
			"SELECT command is only accepted.");
		goto end_func;
	}

	query_str = thd_query(thd);
	ptr = *query_str;
	
	for (; *ptr; ptr++) {
		if (*ptr == quote) {
			quote = '\0';
		} else if (quote) {
		} else if (*ptr == '`' || *ptr == '"') {
			quote = *ptr;
		} else {
			long	i;
			for (i = 0; command_head[i]; i++) {
				if (toupper((int)(unsigned char)(ptr[i]))
				    != toupper((int)(unsigned char)
				      (command_head[i]))) {
					goto nomatch;
				}
			}
			break;
nomatch:
			;
		}
	}

	if (!*ptr) {
		field_store_string(i_s_table->field[0],
			"No XTRA_* command in the SQL statement."
			" Please add /*!XTRA_xxxx*/ to the SQL.");
		goto end_func;
	}

	if (!strncasecmp("XTRA_HELLO", ptr, 10)) {
		/* This is example command XTRA_HELLO */

		ut_print_timestamp(stderr);
		fprintf(stderr, " InnoDB: administration command test for XtraDB"
				" 'XTRA_HELLO' was detected.\n");

		field_store_string(i_s_table->field[0],
			"Hello!");
		goto end_func;
	}
	else if (!strncasecmp("XTRA_LRU_DUMP", ptr, 13)) {
		ut_print_timestamp(stderr);
		fprintf(stderr, " InnoDB: Administrative command 'XTRA_LRU_DUMP'"
				" was detected.\n");

		if (buf_LRU_file_dump()) {
			field_store_string(i_s_table->field[0],
				"XTRA_LRU_DUMP was succeeded.");
		} else {
			field_store_string(i_s_table->field[0],
				"XTRA_LRU_DUMP was failed.");
		}

		goto end_func;
	}
	else if (!strncasecmp("XTRA_LRU_RESTORE", ptr, 16)) {
		ut_print_timestamp(stderr);
		fprintf(stderr, " InnoDB: Administrative command 'XTRA_LRU_RESTORE'"
				" was detected.\n");

		if (buf_LRU_file_restore()) {
			field_store_string(i_s_table->field[0],
				"XTRA_LRU_RESTORE was succeeded.");
		} else {
			field_store_string(i_s_table->field[0],
				"XTRA_LRU_RESTORE was failed.");
		}

		goto end_func;
	}

	field_store_string(i_s_table->field[0],
		"Undefined XTRA_* command.");
	goto end_func;

end_func:
	if (schema_table_store_record(thd, i_s_table)) {
		DBUG_RETURN(1);
	} else {
		DBUG_RETURN(0);
	}
}

static
int
i_s_innodb_admin_command_init(
/*==========================*/
	void*	p)
{
	DBUG_ENTER("i_s_innodb_admin_command_init");
	ST_SCHEMA_TABLE* schema = (ST_SCHEMA_TABLE*) p;

	schema->fields_info = i_s_innodb_admin_command_info;
	schema->fill_table = i_s_innodb_admin_command_fill;

	DBUG_RETURN(0);
}

UNIV_INTERN struct st_mysql_plugin	i_s_innodb_admin_command =
{
	STRUCT_FLD(type, MYSQL_INFORMATION_SCHEMA_PLUGIN),
	STRUCT_FLD(info, &i_s_info),
	STRUCT_FLD(name, "XTRADB_ADMIN_COMMAND"),
	STRUCT_FLD(author, "Percona"),
	STRUCT_FLD(descr, "XtraDB specific command acceptor"),
	STRUCT_FLD(license, PLUGIN_LICENSE_GPL),
	STRUCT_FLD(init, i_s_innodb_admin_command_init),
	STRUCT_FLD(deinit, i_s_common_deinit),
	STRUCT_FLD(version, 0x0100 /* 1.0 */),
	STRUCT_FLD(status_vars, NULL),
	STRUCT_FLD(system_vars, NULL),
	STRUCT_FLD(__reserved1, NULL),
	STRUCT_FLD(flags, 0UL)
};

/***********************************************************************
*/
static ST_FIELD_INFO	i_s_innodb_buffer_pool_pages_fields_info[] =
{
	{STRUCT_FLD(field_name,		"page_type"),
	 STRUCT_FLD(field_length,	64),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_STRING),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_MAYBE_NULL),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	{STRUCT_FLD(field_name,		"space_id"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	{STRUCT_FLD(field_name,		"page_no"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	{STRUCT_FLD(field_name,		"lru_position"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	{STRUCT_FLD(field_name,		"fix_count"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	{STRUCT_FLD(field_name,		"flush_type"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	END_OF_ST_FIELD_INFO
};

static ST_FIELD_INFO	i_s_innodb_buffer_pool_pages_index_fields_info[] =
{
	{STRUCT_FLD(field_name,		"index_id"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	{STRUCT_FLD(field_name,		"space_id"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	{STRUCT_FLD(field_name,		"page_no"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	{STRUCT_FLD(field_name,		"n_recs"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	{STRUCT_FLD(field_name,		"data_size"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	{STRUCT_FLD(field_name,		"hashed"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	{STRUCT_FLD(field_name,		"access_time"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	{STRUCT_FLD(field_name,		"modified"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	{STRUCT_FLD(field_name,		"dirty"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	{STRUCT_FLD(field_name,		"old"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	{STRUCT_FLD(field_name,		"lru_position"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	{STRUCT_FLD(field_name,		"fix_count"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	{STRUCT_FLD(field_name,		"flush_type"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	END_OF_ST_FIELD_INFO
};

static ST_FIELD_INFO	i_s_innodb_buffer_pool_pages_blob_fields_info[] =
{
	{STRUCT_FLD(field_name,		"space_id"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	{STRUCT_FLD(field_name,		"page_no"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	{STRUCT_FLD(field_name,		"compressed"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	{STRUCT_FLD(field_name,		"part_len"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	{STRUCT_FLD(field_name,		"next_page_no"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	{STRUCT_FLD(field_name,		"lru_position"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	{STRUCT_FLD(field_name,		"fix_count"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	{STRUCT_FLD(field_name,		"flush_type"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	END_OF_ST_FIELD_INFO
};

/***********************************************************************
Fill the dynamic table information_schema.innodb_buffer_pool_pages. */
static
int
i_s_innodb_buffer_pool_pages_fill(
/*================*/
				/* out: 0 on success, 1 on failure */
	THD*		thd,	/* in: thread */
	TABLE_LIST*	tables,	/* in/out: tables to fill */
	COND*		cond)	/* in: condition (ignored) */
{
	TABLE*	table	= (TABLE *) tables->table;
	int	status	= 0;
	ulint	i;

	DBUG_ENTER("i_s_innodb_buffer_pool_pages_fill");

	/* deny access to non-superusers */
	if (check_global_access(thd, PROCESS_ACL)) {

		DBUG_RETURN(0);
	}

	RETURN_IF_INNODB_NOT_STARTED(tables->schema_table_name);

	for (i = 0; i < srv_buf_pool_instances; i++) {
		ulint		n_block;
		buf_pool_t*	buf_pool;

		buf_pool = buf_pool_from_array(i);

		buf_pool_mutex_enter(buf_pool);

		for (n_block = 0; n_block < buf_pool->curr_size; n_block++) {
			buf_block_t*	block = buf_page_from_array(buf_pool, n_block);
			const buf_frame_t*	frame = block->frame;

			char page_type[64];

			switch(fil_page_get_type(frame))
			{
				case FIL_PAGE_INDEX:
					strcpy(page_type, "index");
					break;
				case FIL_PAGE_UNDO_LOG:
					strcpy(page_type, "undo_log");
					break;
				case FIL_PAGE_INODE:
					strcpy(page_type, "inode");
					break;
				case FIL_PAGE_IBUF_FREE_LIST:
					strcpy(page_type, "ibuf_free_list");
					break;
				case FIL_PAGE_TYPE_ALLOCATED:
					strcpy(page_type, "allocated");
					break;
				case FIL_PAGE_IBUF_BITMAP:
					strcpy(page_type, "bitmap");
					break;
				case FIL_PAGE_TYPE_SYS:
					strcpy(page_type, "sys");
					break;
				case FIL_PAGE_TYPE_TRX_SYS:
					strcpy(page_type, "trx_sys");
					break;
				case FIL_PAGE_TYPE_FSP_HDR:
					strcpy(page_type, "fsp_hdr");
					break;
				case FIL_PAGE_TYPE_XDES:
					strcpy(page_type, "xdes");
					break;
				case FIL_PAGE_TYPE_BLOB:
					strcpy(page_type, "blob");
					break;
				case FIL_PAGE_TYPE_ZBLOB:
					strcpy(page_type, "zblob");
					break;
				case FIL_PAGE_TYPE_ZBLOB2:
					strcpy(page_type, "zblob2");
					break;
				default:
					sprintf(page_type, "unknown (type=%li)", fil_page_get_type(frame));
			}

			field_store_string(table->field[0], page_type);
			table->field[1]->store(block->page.space);
			table->field[2]->store(block->page.offset);
			table->field[3]->store(0);
			table->field[4]->store(block->page.buf_fix_count);
			table->field[5]->store(block->page.flush_type);

			if (schema_table_store_record(thd, table)) {
				status = 1;
				break;
			}

		}      

		buf_pool_mutex_exit(buf_pool);
	}

	DBUG_RETURN(status);
}

/***********************************************************************
Fill the dynamic table information_schema.innodb_buffer_pool_pages_index. */
static
int
i_s_innodb_buffer_pool_pages_index_fill(
/*================*/
				/* out: 0 on success, 1 on failure */
	THD*		thd,	/* in: thread */
	TABLE_LIST*	tables,	/* in/out: tables to fill */
	COND*		cond)	/* in: condition (ignored) */
{
	TABLE*	table	= (TABLE *) tables->table;
	int	status	= 0;
	ulint	i;
	index_id_t	index_id;

	DBUG_ENTER("i_s_innodb_buffer_pool_pages_index_fill");

	/* deny access to non-superusers */
	if (check_global_access(thd, PROCESS_ACL)) {

		DBUG_RETURN(0);
	}

	RETURN_IF_INNODB_NOT_STARTED(tables->schema_table_name);

	for (i = 0; i < srv_buf_pool_instances; i++) {
		ulint		n_block;
		buf_pool_t*	buf_pool;

		buf_pool = buf_pool_from_array(i);

		buf_pool_mutex_enter(buf_pool);
	
		for (n_block = 0; n_block < buf_pool->curr_size; n_block++) {
			buf_block_t*	block = buf_page_from_array(buf_pool, n_block);
			const buf_frame_t* frame = block->frame;

			if (fil_page_get_type(frame) == FIL_PAGE_INDEX) {
				index_id = btr_page_get_index_id(frame);
				table->field[0]->store(index_id);
				table->field[1]->store(block->page.space);
				table->field[2]->store(block->page.offset);
				table->field[3]->store(page_get_n_recs(frame));
				table->field[4]->store(page_get_data_size(frame));
				table->field[5]->store(block->index != NULL);
				table->field[6]->store(block->page.access_time);
				table->field[7]->store(block->page.newest_modification != 0);
				table->field[8]->store(block->page.oldest_modification != 0);
				table->field[9]->store(block->page.old);
				table->field[10]->store(0);
				table->field[11]->store(block->page.buf_fix_count);
				table->field[12]->store(block->page.flush_type);

				if (schema_table_store_record(thd, table)) {
					status = 1;
					break;
				}
			}      
		}

		buf_pool_mutex_exit(buf_pool);
	}

	DBUG_RETURN(status);
}

/***********************************************************************
Fill the dynamic table information_schema.innodb_buffer_pool_pages_index. */
static
int
i_s_innodb_buffer_pool_pages_blob_fill(
/*================*/
				/* out: 0 on success, 1 on failure */
	THD*		thd,	/* in: thread */
	TABLE_LIST*	tables,	/* in/out: tables to fill */
	COND*		cond)	/* in: condition (ignored) */
{
	TABLE*	table	= (TABLE *) tables->table;
	int	status	= 0;
	ulint	i;

	ulint		part_len;
	ulint		next_page_no;

	DBUG_ENTER("i_s_innodb_buffer_pool_pages_blob_fill");

	/* deny access to non-superusers */
	if (check_global_access(thd, PROCESS_ACL)) {

		DBUG_RETURN(0);
	}

	RETURN_IF_INNODB_NOT_STARTED(tables->schema_table_name);

	for (i = 0; i < srv_buf_pool_instances; i++) {
		ulint		n_block;
		buf_pool_t*	buf_pool;

		buf_pool = buf_pool_from_array(i);

		buf_pool_mutex_enter(buf_pool);
	
		for (n_block = 0; n_block < buf_pool->curr_size; n_block++) {
			buf_block_t*	block = buf_page_from_array(buf_pool, n_block);
			page_zip_des_t*	block_page_zip = buf_block_get_page_zip(block);
			const buf_frame_t* frame = block->frame;

			if (fil_page_get_type(frame) == FIL_PAGE_TYPE_BLOB) {

				if (UNIV_LIKELY_NULL(block_page_zip)) {
					part_len = 0; /* hmm, can't figure it out */

					next_page_no = mach_read_from_4(
							buf_block_get_frame(block)
							+ FIL_PAGE_NEXT);        
				} else {
					part_len = mach_read_from_4(
							buf_block_get_frame(block)
							+ FIL_PAGE_DATA
							+ 0 /*BTR_BLOB_HDR_PART_LEN*/);

					next_page_no = mach_read_from_4(
							buf_block_get_frame(block)
							+ FIL_PAGE_DATA
							+ 4 /*BTR_BLOB_HDR_NEXT_PAGE_NO*/);
				}

				table->field[0]->store(block->page.space);
				table->field[1]->store(block->page.offset);
				table->field[2]->store(block_page_zip != NULL);
				table->field[3]->store(part_len);

				if(next_page_no == FIL_NULL)
				{
					table->field[4]->store(0);
				} else {
					table->field[4]->store(block->page.offset);
				}

				table->field[5]->store(0);
				table->field[6]->store(block->page.buf_fix_count);
				table->field[7]->store(block->page.flush_type);

				if (schema_table_store_record(thd, table)) {
					status = 1;
					break;
				}

			}
		}      

		buf_pool_mutex_exit(buf_pool);
	}

	DBUG_RETURN(status);
}

/***********************************************************************
Bind the dynamic table information_schema.innodb_buffer_pool_pages. */
static
int
i_s_innodb_buffer_pool_pages_init(
/*=========*/
			/* out: 0 on success */
	void*	p)	/* in/out: table schema object */
{
	DBUG_ENTER("i_s_innodb_buffer_pool_pages_init");
	ST_SCHEMA_TABLE* schema = (ST_SCHEMA_TABLE*) p;

	schema->fields_info = i_s_innodb_buffer_pool_pages_fields_info;
	schema->fill_table = i_s_innodb_buffer_pool_pages_fill;

	DBUG_RETURN(0);
}

/***********************************************************************
Bind the dynamic table information_schema.innodb_buffer_pool_pages. */
static
int
i_s_innodb_buffer_pool_pages_index_init(
/*=========*/
			/* out: 0 on success */
	void*	p)	/* in/out: table schema object */
{
	DBUG_ENTER("i_s_innodb_buffer_pool_pages_index_init");
	ST_SCHEMA_TABLE* schema = (ST_SCHEMA_TABLE*) p;

	schema->fields_info = i_s_innodb_buffer_pool_pages_index_fields_info;
	schema->fill_table = i_s_innodb_buffer_pool_pages_index_fill;

	DBUG_RETURN(0);
}

/***********************************************************************
Bind the dynamic table information_schema.innodb_buffer_pool_pages. */
static
int
i_s_innodb_buffer_pool_pages_blob_init(
/*=========*/
			/* out: 0 on success */
	void*	p)	/* in/out: table schema object */
{
	DBUG_ENTER("i_s_innodb_buffer_pool_pages_blob_init");
	ST_SCHEMA_TABLE* schema = (ST_SCHEMA_TABLE*) p;

	schema->fields_info = i_s_innodb_buffer_pool_pages_blob_fields_info;
	schema->fill_table = i_s_innodb_buffer_pool_pages_blob_fill;

	DBUG_RETURN(0);
}


UNIV_INTERN struct st_mysql_plugin	i_s_innodb_buffer_pool_pages =
{
	/* the plugin type (a MYSQL_XXX_PLUGIN value) */
	/* int */
	STRUCT_FLD(type, MYSQL_INFORMATION_SCHEMA_PLUGIN),

	/* pointer to type-specific plugin descriptor */
	/* void* */
	STRUCT_FLD(info, &i_s_info),

	/* plugin name */
	/* const char* */
	STRUCT_FLD(name, "INNODB_BUFFER_POOL_PAGES"),

	/* plugin author (for SHOW PLUGINS) */
	/* const char* */
	STRUCT_FLD(author, "Percona"),

	/* general descriptive text (for SHOW PLUGINS) */
	/* const char* */
	STRUCT_FLD(descr, "InnoDB buffer pool pages"),

	/* the plugin license (PLUGIN_LICENSE_XXX) */
	/* int */
	STRUCT_FLD(license, PLUGIN_LICENSE_GPL),

	/* the function to invoke when plugin is loaded */
	/* int (*)(void*); */
	STRUCT_FLD(init, i_s_innodb_buffer_pool_pages_init),

	/* the function to invoke when plugin is unloaded */
	/* int (*)(void*); */
	STRUCT_FLD(deinit, i_s_common_deinit),

	/* plugin version (for SHOW PLUGINS) */
	/* unsigned int */
	STRUCT_FLD(version, 0x0100 /* 1.0 */),

	/* struct st_mysql_show_var* */
	STRUCT_FLD(status_vars, NULL),

	/* struct st_mysql_sys_var** */
	STRUCT_FLD(system_vars, NULL),

	/* reserved for dependency checking */
	/* void* */
	STRUCT_FLD(__reserved1, NULL),

	/* flags for plugin */
	/* unsigned long */
	STRUCT_FLD(flags, 0UL)
};

UNIV_INTERN struct st_mysql_plugin	i_s_innodb_buffer_pool_pages_index =
{
	/* the plugin type (a MYSQL_XXX_PLUGIN value) */
	/* int */
	STRUCT_FLD(type, MYSQL_INFORMATION_SCHEMA_PLUGIN),

	/* pointer to type-specific plugin descriptor */
	/* void* */
	STRUCT_FLD(info, &i_s_info),

	/* plugin name */
	/* const char* */
	STRUCT_FLD(name, "INNODB_BUFFER_POOL_PAGES_INDEX"),

	/* plugin author (for SHOW PLUGINS) */
	/* const char* */
	STRUCT_FLD(author, "Percona"),

	/* general descriptive text (for SHOW PLUGINS) */
	/* const char* */
	STRUCT_FLD(descr, "InnoDB buffer pool index pages"),

	/* the plugin license (PLUGIN_LICENSE_XXX) */
	/* int */
	STRUCT_FLD(license, PLUGIN_LICENSE_GPL),

	/* the function to invoke when plugin is loaded */
	/* int (*)(void*); */
	STRUCT_FLD(init, i_s_innodb_buffer_pool_pages_index_init),

	/* the function to invoke when plugin is unloaded */
	/* int (*)(void*); */
	STRUCT_FLD(deinit, i_s_common_deinit),

	/* plugin version (for SHOW PLUGINS) */
	/* unsigned int */
	STRUCT_FLD(version, 0x0100 /* 1.0 */),

	/* struct st_mysql_show_var* */
	STRUCT_FLD(status_vars, NULL),

	/* struct st_mysql_sys_var** */
	STRUCT_FLD(system_vars, NULL),

	/* reserved for dependency checking */
	/* void* */
	STRUCT_FLD(__reserved1, NULL),

	/* flags for plugin */
	/* unsigned long */
	STRUCT_FLD(flags, 0UL)
};

UNIV_INTERN struct st_mysql_plugin	i_s_innodb_buffer_pool_pages_blob =
{
	/* the plugin type (a MYSQL_XXX_PLUGIN value) */
	/* int */
	STRUCT_FLD(type, MYSQL_INFORMATION_SCHEMA_PLUGIN),

	/* pointer to type-specific plugin descriptor */
	/* void* */
	STRUCT_FLD(info, &i_s_info),

	/* plugin name */
	/* const char* */
	STRUCT_FLD(name, "INNODB_BUFFER_POOL_PAGES_BLOB"),

	/* plugin author (for SHOW PLUGINS) */
	/* const char* */
	STRUCT_FLD(author, "Percona"),

	/* general descriptive text (for SHOW PLUGINS) */
	/* const char* */
	STRUCT_FLD(descr, "InnoDB buffer pool blob pages"),

	/* the plugin license (PLUGIN_LICENSE_XXX) */
	/* int */
	STRUCT_FLD(license, PLUGIN_LICENSE_GPL),

	/* the function to invoke when plugin is loaded */
	/* int (*)(void*); */
	STRUCT_FLD(init, i_s_innodb_buffer_pool_pages_blob_init),

	/* the function to invoke when plugin is unloaded */
	/* int (*)(void*); */
	STRUCT_FLD(deinit, i_s_common_deinit),

	/* plugin version (for SHOW PLUGINS) */
	/* unsigned int */
	STRUCT_FLD(version, 0x0100 /* 1.0 */),

	/* struct st_mysql_show_var* */
	STRUCT_FLD(status_vars, NULL),

	/* struct st_mysql_sys_var** */
	STRUCT_FLD(system_vars, NULL),

	/* reserved for dependency checking */
	/* void* */
	STRUCT_FLD(__reserved1, NULL),

	/* flags for plugin */
	/* unsigned long */
	STRUCT_FLD(flags, 0UL)
};


static ST_FIELD_INFO	i_s_innodb_undo_logs_fields_info[] =
{
#define IDX_USEG_TRX_ID 0
	{STRUCT_FLD(field_name,		"trx_id"),
	 STRUCT_FLD(field_length,	TRX_ID_MAX_LEN + 1),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_STRING),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	0),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define IDX_USEG_RSEG_ID 1
	{STRUCT_FLD(field_name,		"rseg_id"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define IDX_USEG_USEG_ID 2
	{STRUCT_FLD(field_name,		"useg_id"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define IDX_USEG_TYPE 3
#define USEG_TYPE_MAX_LEN 256
	{STRUCT_FLD(field_name,		"type"),
	 STRUCT_FLD(field_length,	USEG_TYPE_MAX_LEN),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_STRING),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	0),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},
	 
 #define IDX_USEG_STATE 4
 #define USEG_STATE_MAX_LEN 256
	{STRUCT_FLD(field_name,		"state"),
	 STRUCT_FLD(field_length,	USEG_STATE_MAX_LEN),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_STRING),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	0),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},
	 
#define IDX_USEG_SIZE 5
	{STRUCT_FLD(field_name,		"size"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	END_OF_ST_FIELD_INFO
};
static
int
i_s_innodb_undo_logs_fill_store(
/*=================*/
	THD*		thd,	/* in: thread */
	TABLE*		table,	/* in/out: table to fill */
	trx_undo_t*	useg)	/* in: useg to fill from */
{
	char		trx_id[TRX_ID_MAX_LEN + 1];

	DBUG_ENTER("i_s_innodb_undo_logs_fill_store");

	switch (useg->type) {
	case TRX_UNDO_INSERT:
		OK(field_store_string(table->field[IDX_USEG_TYPE], "INSERT"));		
		break;
	case TRX_UNDO_UPDATE:
		OK(field_store_string(table->field[IDX_USEG_TYPE], "UPDATE"));		
		break;
	default:
		OK(field_store_string(table->field[IDX_USEG_TYPE], "UNKNOWN"));		
		break;
	}

	ut_snprintf(trx_id, sizeof(trx_id), TRX_ID_FMT, useg->trx_id);

	switch (useg->state) {
	case TRX_UNDO_ACTIVE:
		OK(field_store_string(table->field[IDX_USEG_TRX_ID], trx_id));
		OK(field_store_string(table->field[IDX_USEG_STATE], "ACTIVE"));		
		break;
	case TRX_UNDO_CACHED:
		OK(field_store_string(table->field[IDX_USEG_TRX_ID], NULL));
		OK(field_store_string(table->field[IDX_USEG_STATE], "CACHED"));		
		break;
	case TRX_UNDO_TO_FREE:
		OK(field_store_string(table->field[IDX_USEG_TRX_ID], NULL));
		OK(field_store_string(table->field[IDX_USEG_STATE], "TO_FREE"));		
		break;
	case TRX_UNDO_TO_PURGE:
		OK(field_store_string(table->field[IDX_USEG_TRX_ID], NULL));
		OK(field_store_string(table->field[IDX_USEG_STATE], "TO_PURGE"));		
		break;
	case TRX_UNDO_PREPARED:
		OK(field_store_string(table->field[IDX_USEG_TRX_ID], trx_id));
		OK(field_store_string(table->field[IDX_USEG_STATE], "PREPARED"));		
		break;
	default:
		OK(field_store_string(table->field[IDX_USEG_TRX_ID], trx_id));
		OK(field_store_string(table->field[IDX_USEG_STATE], "UNKNOWN"));		
		break;
	}

	table->field[IDX_USEG_RSEG_ID]->store(useg->rseg->id);
	table->field[IDX_USEG_USEG_ID]->store(useg->id);
	table->field[IDX_USEG_SIZE]->store(useg->size);
	if (schema_table_store_record(thd, table)) {
		DBUG_RETURN(1);
	}
	DBUG_RETURN(0);
}
static
int
i_s_innodb_undo_logs_fill(
/*=================*/
	THD*		thd,	/* in: thread */
	TABLE_LIST*	tables,	/* in/out: tables to fill */
	COND*		cond)	/* in: condition (ignored) */
{
	TABLE*	table	= (TABLE *) tables->table;
	int	status	= 0;
	trx_rseg_t*	rseg;
	trx_undo_t*	useg;

	DBUG_ENTER("i_s_innodb_undo_logs_fill");

	/* deny access to non-superusers */
	if (check_global_access(thd, PROCESS_ACL)) {
		DBUG_RETURN(0);
	}

	RETURN_IF_INNODB_NOT_STARTED(tables->schema_table_name);

	rseg = UT_LIST_GET_FIRST(trx_sys->rseg_list);
	while (rseg && status == 0) {
		mutex_enter(&(rseg->mutex));
		useg = UT_LIST_GET_FIRST(rseg->update_undo_list);
		while (useg && status == 0) {
			status = i_s_innodb_undo_logs_fill_store(thd, table, useg);
			useg = UT_LIST_GET_NEXT(undo_list, useg);
		}

		useg = UT_LIST_GET_FIRST(rseg->update_undo_cached);
		while (useg && status == 0) {
			status = i_s_innodb_undo_logs_fill_store(thd, table, useg);
			useg = UT_LIST_GET_NEXT(undo_list, useg);
		}

		useg = UT_LIST_GET_FIRST(rseg->insert_undo_list);
		while (useg && status == 0) {
			status = i_s_innodb_undo_logs_fill_store(thd, table, useg);
			useg = UT_LIST_GET_NEXT(undo_list, useg);
		}

		useg = UT_LIST_GET_FIRST(rseg->insert_undo_cached);
		while (useg && status == 0) {
			status = i_s_innodb_undo_logs_fill_store(thd, table, useg);
			useg = UT_LIST_GET_NEXT(undo_list, useg);
		}
		mutex_exit(&(rseg->mutex));
		rseg = UT_LIST_GET_NEXT(rseg_list, rseg);
	}

	DBUG_RETURN(status);
}

static
int
i_s_innodb_undo_logs_init(
/*=================*/
			/* out: 0 on success */
	void*	p)	/* in/out: table schema object */
{
	DBUG_ENTER("i_s_innodb_undo_logs_init");
	ST_SCHEMA_TABLE* schema = (ST_SCHEMA_TABLE*) p;

	schema->fields_info = i_s_innodb_undo_logs_fields_info;
	schema->fill_table = i_s_innodb_undo_logs_fill;

	DBUG_RETURN(0);
}

UNIV_INTERN struct st_mysql_plugin	i_s_innodb_undo_logs =
{
	/* the plugin type (a MYSQL_XXX_PLUGIN value) */
	/* int */
	STRUCT_FLD(type, MYSQL_INFORMATION_SCHEMA_PLUGIN),

	/* pointer to type-specific plugin descriptor */
	/* void* */
	STRUCT_FLD(info, &i_s_info),

	/* plugin name */
	/* const char* */
	STRUCT_FLD(name, "INNODB_UNDO_LOGS"),

	/* plugin author (for SHOW PLUGINS) */
	/* const char* */
	STRUCT_FLD(author, "Percona"),

	/* general descriptive text (for SHOW PLUGINS) */
	/* const char* */
	STRUCT_FLD(descr, "InnoDB rollback undo segment information"),

	/* the plugin license (PLUGIN_LICENSE_XXX) */
	/* int */
	STRUCT_FLD(license, PLUGIN_LICENSE_GPL),

	/* the function to invoke when plugin is loaded */
	/* int (*)(void*); */
	STRUCT_FLD(init, i_s_innodb_undo_logs_init),

	/* the function to invoke when plugin is unloaded */
	/* int (*)(void*); */	STRUCT_FLD(deinit, i_s_common_deinit),

	/* plugin version (for SHOW PLUGINS) */
	STRUCT_FLD(version, 0x0100 /* 1.0 */),

	/* struct st_mysql_show_var* */
	STRUCT_FLD(status_vars, NULL),

	/* struct st_mysql_sys_var** */
	STRUCT_FLD(system_vars, NULL),

	/* reserved for dependency checking */
	/* void* */
	STRUCT_FLD(__reserved1, NULL),

	/* Plugin flags */
	/* unsigned long */
	STRUCT_FLD(flags, 0UL),
};

static ST_FIELD_INFO	i_s_innodb_changed_pages_info[] =
{
	{STRUCT_FLD(field_name,		"space_id"),
	 STRUCT_FLD(field_length,	MY_INT32_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	{STRUCT_FLD(field_name,		"page_id"),
	 STRUCT_FLD(field_length,	MY_INT32_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	{STRUCT_FLD(field_name,		"start_lsn"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	{STRUCT_FLD(field_name,		"end_lsn"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	END_OF_ST_FIELD_INFO
};

/***********************************************************************
  This function parses condition and gets upper bounds for start and end LSN's
  if condition corresponds to certain pattern.

  We can't know right position to avoid scanning bitmap files from the beginning
  to the lower bound. But we can stop scanning bitmap files if we reach upper bound.

  It's expected the most used queries will be like the following:

  SELECT * FROM INNODB_CHANGED_PAGES WHERE START_LSN > num1 AND start_lsn < num2;

  That's why the pattern is:

  pattern:  comp | and_comp;
  comp:     lsn <  int_num | lsn <= int_num | int_num > lsn  | int_num >= lsn;
  lsn:	    start_lsn | end_lsn;
  and_comp: some_expression AND some_expression  | some_expression AND and_comp;
  some_expression: comp	| any_other_expression;

  Suppose the condition is start_lsn < 100, this means we have to read all
  blocks with start_lsn < 100. Which is equivalent to reading all the blocks
  with end_lsn <= 99, or just end_lsn < 100. That's why it's enough to find
  maximum lsn value, doesn't matter if this is start or end lsn and compare
  it with "start_lsn" field.

  Example:

  SELECT * FROM INNODB_CHANGED_PAGES
    WHERE
    start_lsn > 10  AND
    end_lsn <= 1111 AND
    555 > end_lsn   AND
    page_id = 100;

  max_lsn will be set to 555.
*/
static
void
limit_lsn_range_from_condition(
/*===========================*/
	TABLE*		table,   /*!<in: table */
	COND*		cond,    /*!<in: condition */
	ib_uint64_t*	max_lsn) /*!<in/out: maximum LSN
				 (must be initialized with maximum
				 available value) */
{
	if (cond->type() != Item::COND_ITEM &&
	    cond->type() != Item::FUNC_ITEM)
		return;

	switch (((Item_func*) cond)->functype())
	{
		case Item_func::COND_AND_FUNC:
		{
			List_iterator<Item>	li(*((Item_cond*) cond)->
						   argument_list());
			Item			*item;
			while ((item= li++))
				limit_lsn_range_from_condition(table,
							       item,
							       max_lsn);
			break;
		}
		case Item_func::LT_FUNC:
		case Item_func::LE_FUNC:
		case Item_func::GT_FUNC:
		case Item_func::GE_FUNC:
		{
			Item			*left;
			Item			*right;
			Item_field		*item_field;
			ib_uint64_t		tmp_result;

			/*
			   a <= b equals to b >= a that's why we just exchange
			   "left" and "right" in the case of ">" or ">="
			   function
			*/
			if (((Item_func*) cond)->functype() ==
				Item_func::LT_FUNC ||
			    ((Item_func*) cond)->functype() ==
				Item_func::LE_FUNC)
			{
				left = ((Item_func*) cond)->arguments()[0];
				right = ((Item_func*) cond)->arguments()[1];
			} else {
				left = ((Item_func*) cond)->arguments()[1];
				right = ((Item_func*) cond)->arguments()[0];
			}

			if (!left || !right)
				return;
			if (left->type() != Item::FIELD_ITEM)
				return;
			if (right->type() != Item::INT_ITEM)
				return;

			item_field = (Item_field*)left;

			if (/* START_LSN */
			    table->field[2] != item_field->field &&
			    /* END_LSN */
			    table->field[3] != item_field->field)
			{
				return;
			}

			/* Check if the current field belongs to our table */
			if (table != item_field->field->table)
				return;

			tmp_result = right->val_int();
			if (tmp_result < *max_lsn)
				*max_lsn = tmp_result;

			break;
		}
		default:;
	}

}

/***********************************************************************
Fill the dynamic table information_schema.innodb_changed_pages.
@return 0 on success, 1 on failure */
static
int
i_s_innodb_changed_pages_fill(
/*==========================*/
	THD*		thd,	/*!<in: thread */
	TABLE_LIST*	tables,	/*!<in/out: tables to fill */
	COND*		cond)	/*!<in: condition */
{
	TABLE*			table = (TABLE *) tables->table;
	log_bitmap_iterator_t	i;
	ib_uint64_t		output_rows_num = 0UL;
	ib_uint64_t		max_lsn = ~0ULL;

	if (!srv_track_changed_pages)
		return 0;

	if (!log_online_bitmap_iterator_init(&i))
		return 1;

	if (cond)
		limit_lsn_range_from_condition(table, cond, &max_lsn);

	while(log_online_bitmap_iterator_next(&i) &&
	      (!srv_changed_pages_limit ||
	       output_rows_num < srv_changed_pages_limit) &&
	      /*
	        There is no need to compare both start LSN and end LSN fields
	        with maximum value. It's enough to compare only start LSN.
	        Example:

	                              max_lsn = 100
	        \\\\\\\\\\\\\\\\\\\\\\\\\|\\\\\\\\        - Query 1
	        I------I I-------I I-------------I I----I
		//////////////////       |                - Query 2
	           1        2             3          4

	        Query 1:
	        SELECT * FROM INNODB_CHANGED_PAGES WHERE start_lsn < 100
	        will select 1,2,3 bitmaps
	        Query 2:
	        SELECT * FROM INNODB_CHANGED_PAGES WHERE end_lsn < 100
	        will select 1,2 bitmaps

	        The condition start_lsn <= 100 will be false after reading
	        1,2,3 bitmaps which suits for both cases.
	      */
	      LOG_BITMAP_ITERATOR_START_LSN(i) <= max_lsn)
	{
		if (!LOG_BITMAP_ITERATOR_PAGE_CHANGED(i))
			continue;

		/* SPACE_ID */
		table->field[0]->store(
				       LOG_BITMAP_ITERATOR_SPACE_ID(i));
		/* PAGE_ID */
		table->field[1]->store(
				       LOG_BITMAP_ITERATOR_PAGE_NUM(i));
		/* START_LSN */
		table->field[2]->store(
				       LOG_BITMAP_ITERATOR_START_LSN(i));
		/* END_LSN */
		table->field[3]->store(
				       LOG_BITMAP_ITERATOR_END_LSN(i));

		/*
		  I_S tables are in-memory tables. If bitmap file is big enough
		  a lot of memory can be used to store the table. But the size
		  of used memory can be diminished if we store only data which
		  corresponds to some conditions (in WHERE sql clause). Here
		  conditions are checked for the field values stored above.

		  Conditions are checked twice. The first is here (during table
		  generation) and the second during query execution. Maybe it
		  makes sense to use some flag in THD object to avoid double
		  checking.
		*/
		if (cond && !cond->val_int())
			continue;

		if (schema_table_store_record(thd, table))
		{
			log_online_bitmap_iterator_release(&i);
			return 1;
		}

		++output_rows_num;
	}

	log_online_bitmap_iterator_release(&i);
	return 0;
}

static
int
i_s_innodb_changed_pages_init(
/*==========================*/
	void*	p)
{
	DBUG_ENTER("i_s_innodb_changed_pages_init");
	ST_SCHEMA_TABLE* schema = (ST_SCHEMA_TABLE*) p;

	schema->fields_info = i_s_innodb_changed_pages_info;
	schema->fill_table = i_s_innodb_changed_pages_fill;

	DBUG_RETURN(0);
}

UNIV_INTERN struct st_mysql_plugin   i_s_innodb_changed_pages =
{
	STRUCT_FLD(type, MYSQL_INFORMATION_SCHEMA_PLUGIN),
	STRUCT_FLD(info, &i_s_info),
	STRUCT_FLD(name, "INNODB_CHANGED_PAGES"),
	STRUCT_FLD(author, "Percona"),
	STRUCT_FLD(descr, "InnoDB CHANGED_PAGES table"),
	STRUCT_FLD(license, PLUGIN_LICENSE_GPL),
	STRUCT_FLD(init, i_s_innodb_changed_pages_init),
	STRUCT_FLD(deinit, i_s_common_deinit),
	STRUCT_FLD(version, 0x0100 /* 1.0 */),
	STRUCT_FLD(status_vars, NULL),
	STRUCT_FLD(system_vars, NULL),
	STRUCT_FLD(__reserved1, NULL),
	STRUCT_FLD(flags, 0UL),
};

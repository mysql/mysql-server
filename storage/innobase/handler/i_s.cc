/*****************************************************************************

Copyright (c) 2007, 2017, Oracle and/or its affiliates. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is also distributed with certain software (including but not
limited to OpenSSL) that is licensed under separate terms, as designated in a
particular file or component or in included license documentation. The authors
of MySQL hereby grant you an additional permission to link the program and
your derivative works with the separately licensed software that they have
included with MySQL.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

/**************************************************//**
@file handler/i_s.cc
InnoDB INFORMATION SCHEMA tables interface to MySQL.

Created July 18, 2007 Vasil Dimov
*******************************************************/

#include "storage/innobase/handler/i_s.h"

#include <field.h>
#include <sql_acl.h>
#include <sql_show.h>
#include <sql_time.h>
#include <sys/types.h>
#include <time.h>

#include "auth_acls.h"
#include "btr0btr.h"
#include "btr0pcur.h"
#include "btr0types.h"
#include "buf0buddy.h"
#include "buf0buf.h"
#include "buf0stats.h"
#include "dict0crea.h"
#include "dict0dd.h"
#include "dict0dict.h"
#include "dict0load.h"
#include "dict0mem.h"
#include "dict0types.h"
#include "fsp0sysspace.h"
#include "fts0opt.h"
#include "fts0priv.h"
#include "fts0types.h"
#include "fut0fut.h"
#include "ha_prototypes.h"
#include "ibuf0ibuf.h"
#include "my_dbug.h"
#include "my_inttypes.h"
#include "page0zip.h"
#include "pars0pars.h"
#include "srv0mon.h"
#include "srv0start.h"
#include "trx0i_s.h"
#include "trx0trx.h"
#include "ut0new.h"

/** structure associates a name string with a file page type and/or buffer
page state. */
struct buf_page_desc_t{
	const char*	type_str;	/*!< String explain the page
					type/state */
	ulint		type_value;	/*!< Page type or page state */
};

/** We also define I_S_PAGE_TYPE_INDEX as the Index Page's position
in i_s_page_type[] array */
constexpr size_t I_S_PAGE_TYPE_INDEX = 1;

/** Any unassigned FIL_PAGE_TYPE will be treated as unknown. */
constexpr auto I_S_PAGE_TYPE_UNKNOWN = FIL_PAGE_TYPE_UNKNOWN;

/** R-tree index page */
constexpr auto I_S_PAGE_TYPE_RTREE = (FIL_PAGE_TYPE_LAST + 1);

/** Change buffer B-tree page */
constexpr auto I_S_PAGE_TYPE_IBUF = (FIL_PAGE_TYPE_LAST + 2);

/** SDI B-tree page */
constexpr auto I_S_PAGE_TYPE_SDI = (FIL_PAGE_TYPE_LAST + 3);

constexpr auto I_S_PAGE_TYPE_LAST = I_S_PAGE_TYPE_SDI;

constexpr auto I_S_PAGE_TYPE_BITS = 6;

/* Check if we can hold all page types */
static_assert(
        I_S_PAGE_TYPE_LAST < (1 << I_S_PAGE_TYPE_BITS),
        "i_s_page_type[] is too large");

/** Name string for File Page Types */
static buf_page_desc_t	i_s_page_type[] = {
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
	{"UNKNOWN", I_S_PAGE_TYPE_UNKNOWN},
	{"PAGE_IO_COMPRESSED", FIL_PAGE_COMPRESSED},
	{"PAGE_IO_ENCRYPTED", FIL_PAGE_ENCRYPTED},
	{"PAGE_IO_COMPRESSED_ENCRYPTED", FIL_PAGE_COMPRESSED_AND_ENCRYPTED},
	{"ENCRYPTED_RTREE", FIL_PAGE_ENCRYPTED_RTREE},
	{"SDI_BLOB", FIL_PAGE_SDI_BLOB},
	{"SDI_COMPRESSED_BLOB", FIL_PAGE_SDI_ZBLOB},
	{"FIL_PAGE_TYPE_UNUSED", FIL_PAGE_TYPE_UNUSED},
	{"RSEG_ARRAY", FIL_PAGE_TYPE_RSEG_ARRAY},
	{"LOB_INDEX", FIL_PAGE_TYPE_LOB_INDEX},
	{"LOB_DATA", FIL_PAGE_TYPE_LOB_DATA},
	{"LOB_FIRST", FIL_PAGE_TYPE_LOB_FIRST},
	{"ZLOB_FIRST", FIL_PAGE_TYPE_ZLOB_FIRST},
	{"ZLOB_DATA", FIL_PAGE_TYPE_ZLOB_DATA},
	{"ZLOB_INDEX", FIL_PAGE_TYPE_ZLOB_INDEX},
	{"ZLOB_FRAG", FIL_PAGE_TYPE_ZLOB_FRAG},
	{"ZLOB_FRAG_ENTRY", FIL_PAGE_TYPE_ZLOB_FRAG_ENTRY},
	{"RTREE_INDEX", I_S_PAGE_TYPE_RTREE},
	{"IBUF_INDEX", I_S_PAGE_TYPE_IBUF},
	{"SDI_INDEX", I_S_PAGE_TYPE_SDI}
};

/** This structure defines information we will fetch from pages
currently cached in the buffer pool. It will be used to populate
table INFORMATION_SCHEMA.INNODB_BUFFER_PAGE */
struct buf_page_info_t{
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
	unsigned	page_type:I_S_PAGE_TYPE_BITS;	/*!< Page type */
	unsigned	num_recs:UNIV_PAGE_SIZE_SHIFT_MAX-2;
					/*!< Number of records on Page */
	unsigned	data_size:UNIV_PAGE_SIZE_SHIFT_MAX;
					/*!< Sum of the sizes of the records */
	lsn_t		newest_mod;	/*!< Log sequence number of
					the youngest modification */
	lsn_t		oldest_mod;	/*!< Log sequence number of
					the oldest modification */
	space_index_t	index_id;	/*!< Index ID if a index page */
};

/** Maximum number of buffer page info we would cache. */
const ulint	MAX_BUF_INFO_CACHED = 10000;

#define OK(expr)		\
	if ((expr) != 0) {	\
		DBUG_RETURN(1);	\
	}

#if !defined __STRICT_ANSI__ && defined __GNUC__ && !defined __clang__
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
@return 0 on success */
static
int
trx_i_s_common_fill_table(
/*======================*/
	THD*		thd,	/*!< in: thread */
	TABLE_LIST*	tables,	/*!< in/out: tables to fill */
	Item*		);	/*!< in: condition (not used) */

/*******************************************************************//**
Unbind a dynamic INFORMATION_SCHEMA table.
@return 0 on success */
static
int
i_s_common_deinit(
/*==============*/
	void*	p);	/*!< in/out: table schema object */
/*******************************************************************//**
Auxiliary function to store time_t value in MYSQL_TYPE_DATETIME
field.
@return 0 on success */
static
int
field_store_time_t(
/*===============*/
	Field*	field,	/*!< in/out: target field for storage */
	time_t	time)	/*!< in: value to store */
{
	MYSQL_TIME	my_time;
	struct tm	tm_time;

	if (time) {
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
	} else {
		memset(&my_time, 0, sizeof(my_time));
	}

	return(field->store_time(&my_time, MYSQL_TIMESTAMP_DATETIME));
}

/*******************************************************************//**
Auxiliary function to store char* value in MYSQL_TYPE_STRING field.
@return 0 on success */
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

		ret = field->store(str, static_cast<uint>(strlen(str)),
				   system_charset_info);
		field->set_notnull();
	} else {

		ret = 0; /* success */
		field->set_null();
	}

	return(ret);
}

/*******************************************************************//**
Store the name of an index in a MYSQL_TYPE_VARCHAR field.
Handles the names of incomplete secondary indexes.
@return 0 on success */
static
int
field_store_index_name(
/*===================*/
	Field*		field,		/*!< in/out: target field for
					storage */
	const char*	index_name)	/*!< in: NUL-terminated utf-8
					index name, possibly starting with
					TEMP_INDEX_PREFIX */
{
	int	ret;

	ut_ad(index_name != NULL);
	ut_ad(field->real_type() == MYSQL_TYPE_VARCHAR);

	/* Since TEMP_INDEX_PREFIX is not a valid UTF8, we need to convert
	it to something else. */
	if (*index_name == *TEMP_INDEX_PREFIX_STR) {
		char	buf[NAME_LEN + 1];
		buf[0] = '?';
		memcpy(buf + 1, index_name + 1, strlen(index_name));
		ret = field->store(
			buf, static_cast<uint>(strlen(buf)),
			system_charset_info);
	} else {
		ret = field->store(
			index_name, static_cast<uint>(strlen(index_name)),
			system_charset_info);
	}

	field->set_notnull();

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

#define IDX_TRX_READ_ONLY		22
	{STRUCT_FLD(field_name,		"trx_is_read_only"),
	 STRUCT_FLD(field_length,	1),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	0),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define IDX_TRX_AUTOCOMMIT_NON_LOCKING	23
	{STRUCT_FLD(field_name,		"trx_autocommit_non_locking"),
	 STRUCT_FLD(field_length,	1),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	0),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	END_OF_ST_FIELD_INFO
};

/*******************************************************************//**
Read data from cache buffer and fill the INFORMATION_SCHEMA.innodb_trx
table with it.
@return 0 on success */
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
		snprintf(trx_id, sizeof(trx_id), TRX_ID_FMT, row->trx_id);
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
		OK(fields[IDX_TRX_WEIGHT]->store(row->trx_weight, true));

		/* trx_mysql_thread_id */
		OK(fields[IDX_TRX_MYSQL_THREAD_ID]->store(
			   row->trx_mysql_thread_id, true));

		/* trx_query */
		if (row->trx_query) {
			/* store will do appropriate character set
			conversion check */
			fields[IDX_TRX_QUERY]->store(
				row->trx_query,
				static_cast<uint>(strlen(row->trx_query)),
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
			   row->trx_tables_in_use, true));

		/* trx_tables_locked */
		OK(fields[IDX_TRX_TABLES_LOCKED]->store(
			   row->trx_tables_locked, true));

		/* trx_lock_structs */
		OK(fields[IDX_TRX_LOCK_STRUCTS]->store(
			   row->trx_lock_structs, true));

		/* trx_lock_memory_bytes */
		OK(fields[IDX_TRX_LOCK_MEMORY_BYTES]->store(
			   row->trx_lock_memory_bytes, true));

		/* trx_rows_locked */
		OK(fields[IDX_TRX_ROWS_LOCKED]->store(
			   row->trx_rows_locked, true));

		/* trx_rows_modified */
		OK(fields[IDX_TRX_ROWS_MODIFIED]->store(
			   row->trx_rows_modified, true));

		/* trx_concurrency_tickets */
		OK(fields[IDX_TRX_CONNCURRENCY_TICKETS]->store(
			   row->trx_concurrency_tickets, true));

		/* trx_isolation_level */
		OK(field_store_string(fields[IDX_TRX_ISOLATION_LEVEL],
				      row->trx_isolation_level));

		/* trx_unique_checks */
		OK(fields[IDX_TRX_UNIQUE_CHECKS]->store(
			   row->trx_unique_checks, true));

		/* trx_foreign_key_checks */
		OK(fields[IDX_TRX_FOREIGN_KEY_CHECKS]->store(
			   row->trx_foreign_key_checks, true));

		/* trx_last_foreign_key_error */
		OK(field_store_string(fields[IDX_TRX_LAST_FOREIGN_KEY_ERROR],
				      row->trx_foreign_key_error));

		/* trx_adaptive_hash_latched */
		OK(fields[IDX_TRX_ADAPTIVE_HASH_LATCHED]->store(
			   row->trx_has_search_latch, true));

		/* trx_is_read_only*/
		OK(fields[IDX_TRX_READ_ONLY]->store(
			   row->trx_is_read_only, true));

		/* trx_is_autocommit_non_locking */
		OK(fields[IDX_TRX_AUTOCOMMIT_NON_LOCKING]->store(
			   (longlong) row->trx_is_autocommit_non_locking,
			   true));

		OK(schema_table_store_record(thd, table));
	}

	DBUG_RETURN(0);
}

/*******************************************************************//**
Bind the dynamic table INFORMATION_SCHEMA.innodb_trx
@return 0 on success */
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

struct st_mysql_plugin	i_s_innodb_trx =
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

	/* the function to invoke when plugin is un installed */
	/* int (*)(void*); */
	NULL,

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
@return 0 on success */
static
int
trx_i_s_common_fill_table(
/*======================*/
	THD*		thd,	/*!< in: thread */
	TABLE_LIST*	tables,	/*!< in/out: tables to fill */
	Item*		)	/*!< in: condition (not used) */
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

	/* update the cache */
	trx_i_s_cache_start_write(cache);
	trx_i_s_possibly_fetch_data_into_cache(cache);
	trx_i_s_cache_end_write(cache);

	if (trx_i_s_cache_is_truncated(cache)) {

		ib::warn() << "Data in " << table_name << " truncated due to"
			" memory limit of " << TRX_I_S_MEM_LIMIT << " bytes";
	}

	ret = 0;

	trx_i_s_cache_start_read(cache);

	if (innobase_strcasecmp(table_name, "innodb_trx") == 0) {

		if (fill_innodb_trx_from_cache(
			cache, thd, tables->table) != 0) {

			ret = 1;
		}

	} else {
		ib::error() << "trx_i_s_common_fill_table() was"
			" called to fill unknown table: " << table_name << "."
			" This function only knows how to fill"
			" innodb_trx, innodb_locks and"
			" innodb_lock_waits tables.";

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
@return 0 on success, 1 on failure */
static
int
i_s_cmp_fill_low(
/*=============*/
	THD*		thd,	/*!< in: thread */
	TABLE_LIST*	tables,	/*!< in/out: tables to fill */
	Item*		,	/*!< in: condition (ignored) */
	ibool		reset)	/*!< in: TRUE=reset cumulated counts */
{
	TABLE*	table	= (TABLE*) tables->table;
	int	status	= 0;

	DBUG_ENTER("i_s_cmp_fill_low");

	/* deny access to non-superusers */
	if (check_global_access(thd, PROCESS_ACL)) {

		DBUG_RETURN(0);
	}

	for (uint i = 0; i < PAGE_ZIP_SSIZE_MAX; i++) {
		page_zip_stat_t*	zip_stat = &page_zip_stat[i];

		table->field[0]->store(UNIV_ZIP_SIZE_MIN << i);

		/* The cumulated counts are not protected by any
		mutex.  Thus, some operation in page0zip.cc could
		increment a counter between the time we read it and
		clear it.  We could introduce mutex protection, but it
		could cause a measureable performance hit in
		page0zip.cc. */
		table->field[1]->store(zip_stat->compressed, true);
		table->field[2]->store(zip_stat->compressed_ok, true);
		table->field[3]->store(zip_stat->compressed_usec / 1000000, true);
		table->field[4]->store(zip_stat->decompressed, true);
		table->field[5]->store(zip_stat->decompressed_usec / 1000000, true);

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
@return 0 on success, 1 on failure */
static
int
i_s_cmp_fill(
/*=========*/
	THD*		thd,	/*!< in: thread */
	TABLE_LIST*	tables,	/*!< in/out: tables to fill */
	Item*		cond)	/*!< in: condition (ignored) */
{
	return(i_s_cmp_fill_low(thd, tables, cond, FALSE));
}

/*******************************************************************//**
Fill the dynamic table information_schema.innodb_cmp_reset.
@return 0 on success, 1 on failure */
static
int
i_s_cmp_reset_fill(
/*===============*/
	THD*		thd,	/*!< in: thread */
	TABLE_LIST*	tables,	/*!< in/out: tables to fill */
	Item*		cond)	/*!< in: condition (ignored) */
{
	return(i_s_cmp_fill_low(thd, tables, cond, TRUE));
}

/*******************************************************************//**
Bind the dynamic table information_schema.innodb_cmp.
@return 0 on success */
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
@return 0 on success */
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

struct st_mysql_plugin	i_s_innodb_cmp =
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

	/* the function to invoke when plugin is un installed */
	/* int (*)(void*); */
	NULL,

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

struct st_mysql_plugin	i_s_innodb_cmp_reset =
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

	/* the function to invoke when plugin is un installed */
	/* int (*)(void*); */
	NULL,

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

/* Fields of the dynamic tables
information_schema.innodb_cmp_per_index and
information_schema.innodb_cmp_per_index_reset. */
static ST_FIELD_INFO	i_s_cmp_per_index_fields_info[] =
{
#define IDX_DATABASE_NAME	0
	{STRUCT_FLD(field_name,		"database_name"),
	 STRUCT_FLD(field_length,	192),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_STRING),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	0),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define IDX_TABLE_NAME		1
	{STRUCT_FLD(field_name,		"table_name"),
	 STRUCT_FLD(field_length,	192),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_STRING),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	0),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define IDX_INDEX_NAME		2
	{STRUCT_FLD(field_name,		"index_name"),
	 STRUCT_FLD(field_length,	192),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_STRING),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	0),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define IDX_COMPRESS_OPS	3
	{STRUCT_FLD(field_name,		"compress_ops"),
	 STRUCT_FLD(field_length,	MY_INT32_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	0),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define IDX_COMPRESS_OPS_OK	4
	{STRUCT_FLD(field_name,		"compress_ops_ok"),
	 STRUCT_FLD(field_length,	MY_INT32_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	0),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define IDX_COMPRESS_TIME	5
	{STRUCT_FLD(field_name,		"compress_time"),
	 STRUCT_FLD(field_length,	MY_INT32_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	0),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define IDX_UNCOMPRESS_OPS	6
	{STRUCT_FLD(field_name,		"uncompress_ops"),
	 STRUCT_FLD(field_length,	MY_INT32_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	0),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define IDX_UNCOMPRESS_TIME	7
	{STRUCT_FLD(field_name,		"uncompress_time"),
	 STRUCT_FLD(field_length,	MY_INT32_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	0),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	END_OF_ST_FIELD_INFO
};

/*******************************************************************//**
Fill the dynamic table
information_schema.innodb_cmp_per_index or
information_schema.innodb_cmp_per_index_reset.
@return 0 on success, 1 on failure */
static
int
i_s_cmp_per_index_fill_low(
/*=======================*/
	THD*		thd,	/*!< in: thread */
	TABLE_LIST*	tables,	/*!< in/out: tables to fill */
	Item*		,	/*!< in: condition (ignored) */
	ibool		reset)	/*!< in: TRUE=reset cumulated counts */
{
	TABLE*	table = tables->table;
	Field**	fields = table->field;
	int	status = 0;
	int	error;

	DBUG_ENTER("i_s_cmp_per_index_fill_low");

	/* deny access to non-superusers */
	if (check_global_access(thd, PROCESS_ACL)) {

		DBUG_RETURN(0);
	}

	/* Create a snapshot of the stats so we do not bump into lock
	order violations with dict_sys->mutex below. */
	mutex_enter(&page_zip_stat_per_index_mutex);
	page_zip_stat_per_index_t		snap (page_zip_stat_per_index);
	mutex_exit(&page_zip_stat_per_index_mutex);

	mutex_enter(&dict_sys->mutex);

	page_zip_stat_per_index_t::iterator	iter;
	ulint					i;

	for (iter = snap.begin(), i = 0; iter != snap.end(); iter++, i++) {

		char			name[NAME_LEN];
		const dict_index_t*	index = dict_index_find(iter->first);

		if (index != NULL) {
			if (dict_index_is_sdi(index)) {
				continue;
			}
			char	db_utf8[MAX_DB_UTF8_LEN];
			char	table_utf8[MAX_TABLE_UTF8_LEN];

			dict_fs2utf8(index->table_name,
				     db_utf8, sizeof(db_utf8),
				     table_utf8, sizeof(table_utf8));

			field_store_string(fields[IDX_DATABASE_NAME], db_utf8);
			field_store_string(fields[IDX_TABLE_NAME], table_utf8);
			field_store_index_name(fields[IDX_INDEX_NAME],
					       index->name);
		} else {
			/* index not found */
			snprintf(name, sizeof(name),
				    "index_id:" IB_ID_FMT,
				    iter->first.m_index_id);
			field_store_string(fields[IDX_DATABASE_NAME],
					   "unknown");
			field_store_string(fields[IDX_TABLE_NAME],
					   "unknown");
			field_store_string(fields[IDX_INDEX_NAME],
					   name);
		}

		fields[IDX_COMPRESS_OPS]->store(
			   iter->second.compressed, true);

		fields[IDX_COMPRESS_OPS_OK]->store(
			   iter->second.compressed_ok, true);

		fields[IDX_COMPRESS_TIME]->store(
			   iter->second.compressed_usec / 1000000, true);

		fields[IDX_UNCOMPRESS_OPS]->store(
			   iter->second.decompressed, true);

		fields[IDX_UNCOMPRESS_TIME]->store(
			   iter->second.decompressed_usec / 1000000, true);

		if ((error = schema_table_store_record2(thd, table, false))) {
			mutex_exit(&dict_sys->mutex);
			if (convert_heap_table_to_ondisk(thd, table, error) != 0) {
				status = 1;
				goto err;
			}
			mutex_enter(&dict_sys->mutex);
		}

		/* Release and reacquire the dict mutex to allow other
		threads to proceed. This could eventually result in the
		contents of INFORMATION_SCHEMA.innodb_cmp_per_index being
		inconsistent, but it is an acceptable compromise. */
		if (i % 1000 == 0) {
			mutex_exit(&dict_sys->mutex);
			mutex_enter(&dict_sys->mutex);
		}
	}

	mutex_exit(&dict_sys->mutex);
err:

	if (reset) {
		page_zip_reset_stat_per_index();
	}

	DBUG_RETURN(status);
}

/*******************************************************************//**
Fill the dynamic table information_schema.innodb_cmp_per_index.
@return 0 on success, 1 on failure */
static
int
i_s_cmp_per_index_fill(
/*===================*/
	THD*		thd,	/*!< in: thread */
	TABLE_LIST*	tables,	/*!< in/out: tables to fill */
	Item*		cond)	/*!< in: condition (ignored) */
{
	return(i_s_cmp_per_index_fill_low(thd, tables, cond, FALSE));
}

/*******************************************************************//**
Fill the dynamic table information_schema.innodb_cmp_per_index_reset.
@return 0 on success, 1 on failure */
static
int
i_s_cmp_per_index_reset_fill(
/*=========================*/
	THD*		thd,	/*!< in: thread */
	TABLE_LIST*	tables,	/*!< in/out: tables to fill */
	Item*		cond)	/*!< in: condition (ignored) */
{
	return(i_s_cmp_per_index_fill_low(thd, tables, cond, TRUE));
}

/*******************************************************************//**
Bind the dynamic table information_schema.innodb_cmp_per_index.
@return 0 on success */
static
int
i_s_cmp_per_index_init(
/*===================*/
	void*	p)	/*!< in/out: table schema object */
{
	DBUG_ENTER("i_s_cmp_init");
	ST_SCHEMA_TABLE* schema = (ST_SCHEMA_TABLE*) p;

	schema->fields_info = i_s_cmp_per_index_fields_info;
	schema->fill_table = i_s_cmp_per_index_fill;

	DBUG_RETURN(0);
}

/*******************************************************************//**
Bind the dynamic table information_schema.innodb_cmp_per_index_reset.
@return 0 on success */
static
int
i_s_cmp_per_index_reset_init(
/*=========================*/
	void*	p)	/*!< in/out: table schema object */
{
	DBUG_ENTER("i_s_cmp_reset_init");
	ST_SCHEMA_TABLE* schema = (ST_SCHEMA_TABLE*) p;

	schema->fields_info = i_s_cmp_per_index_fields_info;
	schema->fill_table = i_s_cmp_per_index_reset_fill;

	DBUG_RETURN(0);
}

struct st_mysql_plugin	i_s_innodb_cmp_per_index =
{
	/* the plugin type (a MYSQL_XXX_PLUGIN value) */
	/* int */
	STRUCT_FLD(type, MYSQL_INFORMATION_SCHEMA_PLUGIN),

	/* pointer to type-specific plugin descriptor */
	/* void* */
	STRUCT_FLD(info, &i_s_info),

	/* plugin name */
	/* const char* */
	STRUCT_FLD(name, "INNODB_CMP_PER_INDEX"),

	/* plugin author (for SHOW PLUGINS) */
	/* const char* */
	STRUCT_FLD(author, plugin_author),

	/* general descriptive text (for SHOW PLUGINS) */
	/* const char* */
	STRUCT_FLD(descr, "Statistics for the InnoDB compression (per index)"),

	/* the plugin license (PLUGIN_LICENSE_XXX) */
	/* int */
	STRUCT_FLD(license, PLUGIN_LICENSE_GPL),

	/* the function to invoke when plugin is loaded */
	/* int (*)(void*); */
	STRUCT_FLD(init, i_s_cmp_per_index_init),

	/* the function to invoke when plugin is un installed */
	/* int (*)(void*); */
	NULL,

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

struct st_mysql_plugin	i_s_innodb_cmp_per_index_reset =
{
	/* the plugin type (a MYSQL_XXX_PLUGIN value) */
	/* int */
	STRUCT_FLD(type, MYSQL_INFORMATION_SCHEMA_PLUGIN),

	/* pointer to type-specific plugin descriptor */
	/* void* */
	STRUCT_FLD(info, &i_s_info),

	/* plugin name */
	/* const char* */
	STRUCT_FLD(name, "INNODB_CMP_PER_INDEX_RESET"),

	/* plugin author (for SHOW PLUGINS) */
	/* const char* */
	STRUCT_FLD(author, plugin_author),

	/* general descriptive text (for SHOW PLUGINS) */
	/* const char* */
	STRUCT_FLD(descr, "Statistics for the InnoDB compression (per index);"
		   " reset cumulated counts"),

	/* the plugin license (PLUGIN_LICENSE_XXX) */
	/* int */
	STRUCT_FLD(license, PLUGIN_LICENSE_GPL),

	/* the function to invoke when plugin is loaded */
	/* int (*)(void*); */
	STRUCT_FLD(init, i_s_cmp_per_index_reset_init),

	/* the function to invoke when plugin is un installed */
	/* int (*)(void*); */
	NULL,

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

/** Fill the dynamic table information_schema.innodb_cmpmem or
innodb_cmpmem_reset.
@param[in]	thd	thread
@param[in,out]	tables	tables to fill
@param[in]	item	condition (ignored)
@param[in]	reset	TRUE=reset cumulated counts
@return 0 on success, 1 on failure */
static
int
i_s_cmpmem_fill_low(
	THD*		thd,
	TABLE_LIST*	tables,
	Item*		item,
	ibool		reset)
{
	int		status = 0;
	TABLE*	table	= (TABLE*) tables->table;

	DBUG_ENTER("i_s_cmpmem_fill_low");

	/* deny access to non-superusers */
	if (check_global_access(thd, PROCESS_ACL)) {

		DBUG_RETURN(0);
	}

	for (ulint i = 0; i < srv_buf_pool_instances; i++) {
		buf_pool_t*		buf_pool;
		ulint			zip_free_len_local[BUF_BUDDY_SIZES_MAX + 1];
		buf_buddy_stat_t	buddy_stat_local[BUF_BUDDY_SIZES_MAX + 1];

		status	= 0;

		buf_pool = buf_pool_from_array(i);

		mutex_enter(&buf_pool->zip_free_mutex);

		/* Save buddy stats for buffer pool in local variables. */
		for (uint x = 0; x <= BUF_BUDDY_SIZES; x++) {

			zip_free_len_local[x] = (x < BUF_BUDDY_SIZES) ?
				UT_LIST_GET_LEN(buf_pool->zip_free[x]) : 0;

			os_rmb;
			buddy_stat_local[x] = buf_pool->buddy_stat[x];

			if (reset) {
				/* This is protected by buf_pool->mutex. */
				buf_pool->buddy_stat[x].relocated = 0;
				buf_pool->buddy_stat[x].relocated_usec = 0;
			}
		}

		mutex_exit(&buf_pool->zip_free_mutex);

		for (uint x = 0; x <= BUF_BUDDY_SIZES; x++) {
			buf_buddy_stat_t*	buddy_stat;

			buddy_stat = &buddy_stat_local[x];

			table->field[0]->store(BUF_BUDDY_LOW << x);
			table->field[1]->store(i, true);
			table->field[2]->store(buddy_stat->used, true);
			table->field[3]->store(zip_free_len_local[x], true);
			table->field[4]->store(buddy_stat->relocated, true);
			table->field[5]->store(
				buddy_stat->relocated_usec / 1000000, true);

			if (schema_table_store_record(thd, table)) {
				status = 1;
				break;
			}
		}

		if (status) {
			break;
		}
	}

	DBUG_RETURN(status);
}

/*******************************************************************//**
Fill the dynamic table information_schema.innodb_cmpmem.
@return 0 on success, 1 on failure */
static
int
i_s_cmpmem_fill(
/*============*/
	THD*		thd,	/*!< in: thread */
	TABLE_LIST*	tables,	/*!< in/out: tables to fill */
	Item*		cond)	/*!< in: condition (ignored) */
{
	return(i_s_cmpmem_fill_low(thd, tables, cond, FALSE));
}

/*******************************************************************//**
Fill the dynamic table information_schema.innodb_cmpmem_reset.
@return 0 on success, 1 on failure */
static
int
i_s_cmpmem_reset_fill(
/*==================*/
	THD*		thd,	/*!< in: thread */
	TABLE_LIST*	tables,	/*!< in/out: tables to fill */
	Item*		cond)	/*!< in: condition (ignored) */
{
	return(i_s_cmpmem_fill_low(thd, tables, cond, TRUE));
}

/*******************************************************************//**
Bind the dynamic table information_schema.innodb_cmpmem.
@return 0 on success */
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
@return 0 on success */
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

struct st_mysql_plugin	i_s_innodb_cmpmem =
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

	/* the function to invoke when plugin is un installed */
	/* int (*)(void*); */
	NULL,

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

struct st_mysql_plugin	i_s_innodb_cmpmem_reset =
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

	/* the function to invoke when plugin is un installed */
	/* int (*)(void*); */
	NULL,

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

/* Fields of the dynamic table INFORMATION_SCHEMA.innodb_metrics */
static ST_FIELD_INFO	innodb_metrics_fields_info[] =
{
#define	METRIC_NAME		0
	{STRUCT_FLD(field_name,		"NAME"),
	 STRUCT_FLD(field_length,	NAME_LEN + 1),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_STRING),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	0),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define	METRIC_SUBSYS		1
	{STRUCT_FLD(field_name,		"SUBSYSTEM"),
	 STRUCT_FLD(field_length,	NAME_LEN + 1),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_STRING),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	0),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define	METRIC_VALUE_START	2
	{STRUCT_FLD(field_name,		"COUNT"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	0),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define	METRIC_MAX_VALUE_START	3
	{STRUCT_FLD(field_name,		"MAX_COUNT"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_MAYBE_NULL),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define	METRIC_MIN_VALUE_START	4
	{STRUCT_FLD(field_name,		"MIN_COUNT"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_MAYBE_NULL),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define	METRIC_AVG_VALUE_START	5
	{STRUCT_FLD(field_name,		"AVG_COUNT"),
	 STRUCT_FLD(field_length,	MAX_FLOAT_STR_LENGTH),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_FLOAT),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_MAYBE_NULL),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define	METRIC_VALUE_RESET	6
	{STRUCT_FLD(field_name,		"COUNT_RESET"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	0),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define	METRIC_MAX_VALUE_RESET	7
	{STRUCT_FLD(field_name,		"MAX_COUNT_RESET"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_MAYBE_NULL),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define	METRIC_MIN_VALUE_RESET	8
	{STRUCT_FLD(field_name,		"MIN_COUNT_RESET"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_MAYBE_NULL),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define	METRIC_AVG_VALUE_RESET	9
	{STRUCT_FLD(field_name,		"AVG_COUNT_RESET"),
	 STRUCT_FLD(field_length,	MAX_FLOAT_STR_LENGTH),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_FLOAT),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_MAYBE_NULL),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define	METRIC_START_TIME	10
	{STRUCT_FLD(field_name,		"TIME_ENABLED"),
	 STRUCT_FLD(field_length,	0),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_DATETIME),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_MAYBE_NULL),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define	METRIC_STOP_TIME	11
	{STRUCT_FLD(field_name,		"TIME_DISABLED"),
	 STRUCT_FLD(field_length,	0),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_DATETIME),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_MAYBE_NULL),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define	METRIC_TIME_ELAPSED	12
	{STRUCT_FLD(field_name,		"TIME_ELAPSED"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_MAYBE_NULL),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define	METRIC_RESET_TIME	13
	{STRUCT_FLD(field_name,		"TIME_RESET"),
	 STRUCT_FLD(field_length,	0),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_DATETIME),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_MAYBE_NULL),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define	METRIC_STATUS		14
	{STRUCT_FLD(field_name,		"STATUS"),
	 STRUCT_FLD(field_length,	NAME_LEN + 1),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_STRING),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	0),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define	METRIC_TYPE		15
	{STRUCT_FLD(field_name,		"TYPE"),
	 STRUCT_FLD(field_length,	NAME_LEN + 1),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_STRING),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	0),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define	METRIC_DESC		16
	{STRUCT_FLD(field_name,		"COMMENT"),
	 STRUCT_FLD(field_length,	NAME_LEN + 1),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_STRING),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	0),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	END_OF_ST_FIELD_INFO
};

/**********************************************************************//**
Fill the information schema metrics table.
@return 0 on success */
static
int
i_s_metrics_fill(
/*=============*/
	THD*		thd,		/*!< in: thread */
	TABLE*		table_to_fill)	/*!< in/out: fill this table */
{
	int		count;
	Field**		fields;
	double		time_diff = 0;
	monitor_info_t*	monitor_info;
	mon_type_t	min_val;
	mon_type_t	max_val;

	DBUG_ENTER("i_s_metrics_fill");
	fields = table_to_fill->field;

	for (count = 0; count < NUM_MONITOR; count++) {
		monitor_info = srv_mon_get_info((monitor_id_t) count);

		/* A good place to sanity check the Monitor ID */
		ut_a(count == monitor_info->monitor_id);

		/* If the item refers to a Module, nothing to fill,
		continue. */
		if ((monitor_info->monitor_type & MONITOR_MODULE)
		    || (monitor_info->monitor_type & MONITOR_HIDDEN)) {
			continue;
		}

		/* If this is an existing "status variable", and
		its corresponding counter is still on, we need
		to calculate the result from its corresponding
		counter. */
		if (monitor_info->monitor_type & MONITOR_EXISTING
		    && MONITOR_IS_ON(count)) {
			srv_mon_process_existing_counter((monitor_id_t) count,
							 MONITOR_GET_VALUE);
		}

		/* Fill in counter's basic information */
		OK(field_store_string(fields[METRIC_NAME],
				      monitor_info->monitor_name));

		OK(field_store_string(fields[METRIC_SUBSYS],
				      monitor_info->monitor_module));

		OK(field_store_string(fields[METRIC_DESC],
				      monitor_info->monitor_desc));

		/* Fill in counter values */
		OK(fields[METRIC_VALUE_RESET]->store(
			MONITOR_VALUE(count), FALSE));

		OK(fields[METRIC_VALUE_START]->store(
			MONITOR_VALUE_SINCE_START(count), FALSE));

		/* If the max value is MAX_RESERVED, counter max
		value has not been updated. Set the column value
		to NULL. */
		if (MONITOR_MAX_VALUE(count) == MAX_RESERVED
		    || MONITOR_MAX_MIN_NOT_INIT(count)) {
			fields[METRIC_MAX_VALUE_RESET]->set_null();
		} else {
			OK(fields[METRIC_MAX_VALUE_RESET]->store(
				MONITOR_MAX_VALUE(count), FALSE));
			fields[METRIC_MAX_VALUE_RESET]->set_notnull();
		}

		/* If the min value is MAX_RESERVED, counter min
		value has not been updated. Set the column value
		to NULL. */
		if (MONITOR_MIN_VALUE(count) == MIN_RESERVED
		    || MONITOR_MAX_MIN_NOT_INIT(count)) {
			fields[METRIC_MIN_VALUE_RESET]->set_null();
		} else {
			OK(fields[METRIC_MIN_VALUE_RESET]->store(
				MONITOR_MIN_VALUE(count), FALSE));
			fields[METRIC_MIN_VALUE_RESET]->set_notnull();
		}

		/* Calculate the max value since counter started */
		max_val = srv_mon_calc_max_since_start((monitor_id_t) count);

		if (max_val == MAX_RESERVED
		    || MONITOR_MAX_MIN_NOT_INIT(count)) {
			fields[METRIC_MAX_VALUE_START]->set_null();
		} else {
			OK(fields[METRIC_MAX_VALUE_START]->store(
				max_val, FALSE));
			fields[METRIC_MAX_VALUE_START]->set_notnull();
		}

		/* Calculate the min value since counter started */
		min_val = srv_mon_calc_min_since_start((monitor_id_t) count);

		if (min_val == MIN_RESERVED
		    || MONITOR_MAX_MIN_NOT_INIT(count)) {
			fields[METRIC_MIN_VALUE_START]->set_null();
		} else {
			OK(fields[METRIC_MIN_VALUE_START]->store(
				min_val, FALSE));

			fields[METRIC_MIN_VALUE_START]->set_notnull();
		}

		/* If monitor has been enabled (no matter it is disabled
		or not now), fill METRIC_START_TIME and METRIC_TIME_ELAPSED
		field */
		if (MONITOR_FIELD(count, mon_start_time)) {
			OK(field_store_time_t(fields[METRIC_START_TIME],
				(time_t)MONITOR_FIELD(count, mon_start_time)));
			fields[METRIC_START_TIME]->set_notnull();

			/* If monitor is enabled, the TIME_ELAPSED is the
			time difference between current and time when monitor
			is enabled. Otherwise, it is the time difference
			between time when monitor is enabled and time
			when it is disabled */
			if (MONITOR_IS_ON(count)) {
				time_diff = difftime(time(NULL),
					MONITOR_FIELD(count, mon_start_time));
			} else {
				time_diff =  difftime(
					MONITOR_FIELD(count, mon_stop_time),
					MONITOR_FIELD(count, mon_start_time));
			}

			OK(fields[METRIC_TIME_ELAPSED]->store(
				time_diff));
			fields[METRIC_TIME_ELAPSED]->set_notnull();
		} else {
			fields[METRIC_START_TIME]->set_null();
			fields[METRIC_TIME_ELAPSED]->set_null();
			time_diff = 0;
		}

		/* Unless MONITOR__NO_AVERAGE is marked, we will need
		to calculate the average value. If this is a monitor set
		owner marked by MONITOR_SET_OWNER, divide
		the value by another counter (number of calls) designated
		by monitor_info->monitor_related_id.
		Otherwise average the counter value by the time between the
		time that the counter is enabled and time it is disabled
		or time it is sampled. */
		if (!(monitor_info->monitor_type & MONITOR_NO_AVERAGE)
		    && (monitor_info->monitor_type & MONITOR_SET_OWNER)
		    && monitor_info->monitor_related_id) {
			mon_type_t	value_start
				 = MONITOR_VALUE_SINCE_START(
					monitor_info->monitor_related_id);

			if (value_start) {
				OK(fields[METRIC_AVG_VALUE_START]->store(
					MONITOR_VALUE_SINCE_START(count)
					/ value_start, FALSE));

				fields[METRIC_AVG_VALUE_START]->set_notnull();
			} else {
				fields[METRIC_AVG_VALUE_START]->set_null();
			}

			if (MONITOR_VALUE(monitor_info->monitor_related_id)) {
				OK(fields[METRIC_AVG_VALUE_RESET]->store(
					MONITOR_VALUE(count)
					/ MONITOR_VALUE(
					monitor_info->monitor_related_id),
					FALSE));
			} else {
				fields[METRIC_AVG_VALUE_RESET]->set_null();
			}
		} else if (!(monitor_info->monitor_type & MONITOR_NO_AVERAGE)
			   && !(monitor_info->monitor_type
				& MONITOR_DISPLAY_CURRENT)) {
			if (time_diff) {
				OK(fields[METRIC_AVG_VALUE_START]->store(
					(double) MONITOR_VALUE_SINCE_START(
						count) / time_diff));
				fields[METRIC_AVG_VALUE_START]->set_notnull();
			} else {
				fields[METRIC_AVG_VALUE_START]->set_null();
			}

			if (MONITOR_FIELD(count, mon_reset_time)) {
				/* calculate the time difference since last
				reset */
				if (MONITOR_IS_ON(count)) {
					time_diff = difftime(
						time(NULL), MONITOR_FIELD(
							count, mon_reset_time));
				} else {
					time_diff =  difftime(
					MONITOR_FIELD(count, mon_stop_time),
					MONITOR_FIELD(count, mon_reset_time));
				}
			} else {
				time_diff = 0;
			}

			if (time_diff) {
				OK(fields[METRIC_AVG_VALUE_RESET]->store(
					static_cast<double>(
						MONITOR_VALUE(count) / time_diff)));
				fields[METRIC_AVG_VALUE_RESET]->set_notnull();
			} else {
				fields[METRIC_AVG_VALUE_RESET]->set_null();
			}
		} else {
			fields[METRIC_AVG_VALUE_START]->set_null();
			fields[METRIC_AVG_VALUE_RESET]->set_null();
		}


		if (MONITOR_IS_ON(count)) {
			/* If monitor is on, the stop time will set to NULL */
			fields[METRIC_STOP_TIME]->set_null();

			/* Display latest Monitor Reset Time only if Monitor
			counter is on. */
			if (MONITOR_FIELD(count, mon_reset_time)) {
				OK(field_store_time_t(
					fields[METRIC_RESET_TIME],
					(time_t)MONITOR_FIELD(
						count, mon_reset_time)));
				fields[METRIC_RESET_TIME]->set_notnull();
			} else {
				fields[METRIC_RESET_TIME]->set_null();
			}

			/* Display the monitor status as "enabled" */
			OK(field_store_string(fields[METRIC_STATUS],
					      "enabled"));
		} else {
			if (MONITOR_FIELD(count, mon_stop_time)) {
				OK(field_store_time_t(fields[METRIC_STOP_TIME],
				(time_t)MONITOR_FIELD(count, mon_stop_time)));
				fields[METRIC_STOP_TIME]->set_notnull();
			} else {
				fields[METRIC_STOP_TIME]->set_null();
			}

			fields[METRIC_RESET_TIME]->set_null();

			OK(field_store_string(fields[METRIC_STATUS],
					      "disabled"));
		}

		if (monitor_info->monitor_type & MONITOR_DISPLAY_CURRENT) {
			OK(field_store_string(fields[METRIC_TYPE],
					      "value"));
		} else if (monitor_info->monitor_type & MONITOR_EXISTING) {
			OK(field_store_string(fields[METRIC_TYPE],
					      "status_counter"));
		} else if (monitor_info->monitor_type & MONITOR_SET_OWNER) {
			OK(field_store_string(fields[METRIC_TYPE],
					      "set_owner"));
		} else if ( monitor_info->monitor_type & MONITOR_SET_MEMBER) {
			OK(field_store_string(fields[METRIC_TYPE],
					      "set_member"));
		} else {
			OK(field_store_string(fields[METRIC_TYPE],
					      "counter"));
		}

		OK(schema_table_store_record(thd, table_to_fill));
	}

	DBUG_RETURN(0);
}

/*******************************************************************//**
Function to fill information schema metrics tables.
@return 0 on success */
static
int
i_s_metrics_fill_table(
/*===================*/
	THD*		thd,	/*!< in: thread */
	TABLE_LIST*	tables,	/*!< in/out: tables to fill */
	Item*		)	/*!< in: condition (not used) */
{
	DBUG_ENTER("i_s_metrics_fill_table");

	/* deny access to non-superusers */
	if (check_global_access(thd, PROCESS_ACL)) {
		DBUG_RETURN(0);
	}

	i_s_metrics_fill(thd, tables->table);

	DBUG_RETURN(0);
}
/*******************************************************************//**
Bind the dynamic table INFORMATION_SCHEMA.innodb_metrics
@return 0 on success */
static
int
innodb_metrics_init(
/*================*/
	void*	p)	/*!< in/out: table schema object */
{
	ST_SCHEMA_TABLE*	schema;

	DBUG_ENTER("innodb_metrics_init");

	schema = (ST_SCHEMA_TABLE*) p;

	schema->fields_info = innodb_metrics_fields_info;
	schema->fill_table = i_s_metrics_fill_table;

	DBUG_RETURN(0);
}

struct st_mysql_plugin	i_s_innodb_metrics =
{
	/* the plugin type (a MYSQL_XXX_PLUGIN value) */
	/* int */
	STRUCT_FLD(type, MYSQL_INFORMATION_SCHEMA_PLUGIN),

	/* pointer to type-specific plugin descriptor */
	/* void* */
	STRUCT_FLD(info, &i_s_info),

	/* plugin name */
	/* const char* */
	STRUCT_FLD(name, "INNODB_METRICS"),

	/* plugin author (for SHOW PLUGINS) */
	/* const char* */
	STRUCT_FLD(author, plugin_author),

	/* general descriptive text (for SHOW PLUGINS) */
	/* const char* */
	STRUCT_FLD(descr, "InnoDB Metrics Info"),

	/* the plugin license (PLUGIN_LICENSE_XXX) */
	/* int */
	STRUCT_FLD(license, PLUGIN_LICENSE_GPL),

	/* the function to invoke when plugin is loaded */
	/* int (*)(void*); */
	STRUCT_FLD(init, innodb_metrics_init),

	/* the function to invoke when plugin is un installed */
	/* int (*)(void*); */
	NULL,

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
/* Fields of the dynamic table INFORMATION_SCHEMA.innodb_ft_default_stopword */
static ST_FIELD_INFO	i_s_stopword_fields_info[] =
{
#define STOPWORD_VALUE	0
	{STRUCT_FLD(field_name,		"value"),
	 STRUCT_FLD(field_length,	TRX_ID_MAX_LEN + 1),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_STRING),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	0),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	END_OF_ST_FIELD_INFO
};

/*******************************************************************//**
Fill the dynamic table information_schema.innodb_ft_default_stopword.
@return 0 on success, 1 on failure */
static
int
i_s_stopword_fill(
/*==============*/
	THD*		thd,	/*!< in: thread */
	TABLE_LIST*	tables,	/*!< in/out: tables to fill */
	Item*		)	/*!< in: condition (not used) */
{
	Field**	fields;
	ulint	i = 0;
	TABLE*	table = (TABLE*) tables->table;

	DBUG_ENTER("i_s_stopword_fill");

	fields = table->field;

	/* Fill with server default stopword list in array
	fts_default_stopword */
	while (fts_default_stopword[i]) {
		OK(field_store_string(fields[STOPWORD_VALUE],
				      fts_default_stopword[i]));

		OK(schema_table_store_record(thd, table));
		i++;
	}

	DBUG_RETURN(0);
}

/*******************************************************************//**
Bind the dynamic table information_schema.innodb_ft_default_stopword.
@return 0 on success */
static
int
i_s_stopword_init(
/*==============*/
	void*	p)	/*!< in/out: table schema object */
{
	DBUG_ENTER("i_s_stopword_init");
	ST_SCHEMA_TABLE* schema = (ST_SCHEMA_TABLE*) p;

	schema->fields_info = i_s_stopword_fields_info;
	schema->fill_table = i_s_stopword_fill;

	DBUG_RETURN(0);
}

struct st_mysql_plugin	i_s_innodb_ft_default_stopword =
{
	/* the plugin type (a MYSQL_XXX_PLUGIN value) */
	/* int */
	STRUCT_FLD(type, MYSQL_INFORMATION_SCHEMA_PLUGIN),

	/* pointer to type-specific plugin descriptor */
	/* void* */
	STRUCT_FLD(info, &i_s_info),

	/* plugin name */
	/* const char* */
	STRUCT_FLD(name, "INNODB_FT_DEFAULT_STOPWORD"),

	/* plugin author (for SHOW PLUGINS) */
	/* const char* */
	STRUCT_FLD(author, plugin_author),

	/* general descriptive text (for SHOW PLUGINS) */
	/* const char* */
	STRUCT_FLD(descr, "Default stopword list for InnDB Full Text Search"),

	/* the plugin license (PLUGIN_LICENSE_XXX) */
	/* int */
	STRUCT_FLD(license, PLUGIN_LICENSE_GPL),

	/* the function to invoke when plugin is loaded */
	/* int (*)(void*); */
	STRUCT_FLD(init, i_s_stopword_init),

	/* the function to invoke when plugin is un installed */
	/* int (*)(void*); */
	NULL,

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

/* Fields of the dynamic table INFORMATION_SCHEMA.INNODB_FT_DELETED
INFORMATION_SCHEMA.INNODB_FT_BEING_DELETED */
static ST_FIELD_INFO	i_s_fts_doc_fields_info[] =
{
#define	I_S_FTS_DOC_ID			0
	{STRUCT_FLD(field_name,		"DOC_ID"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	END_OF_ST_FIELD_INFO
};

/*******************************************************************//**
Fill the dynamic table INFORMATION_SCHEMA.INNODB_FT_DELETED or
INFORMATION_SCHEMA.INNODB_FT_BEING_DELETED
@return 0 on success, 1 on failure */
static
int
i_s_fts_deleted_generic_fill(
/*=========================*/
	THD*		thd,		/*!< in: thread */
	TABLE_LIST*	tables,		/*!< in/out: tables to fill */
	ibool		being_deleted)	/*!< in: BEING_DELTED table */
{
	Field**			fields;
	TABLE*			table = (TABLE*) tables->table;
	trx_t*			trx;
	fts_table_t		fts_table;
	fts_doc_ids_t*		deleted;
	dict_table_t*		user_table;
	MDL_ticket*		mdl = nullptr;
	char			local_name[MAX_FULL_NAME_LEN];

	DBUG_ENTER("i_s_fts_deleted_generic_fill");

	/* deny access to non-superusers */
	if (check_global_access(thd, PROCESS_ACL)) {
		DBUG_RETURN(0);
	}

	/* Prevent DDL to drop fts aux tables. */
	rw_lock_s_lock(dict_operation_lock);

	if (!fts_internal_tbl_name) {
		rw_lock_s_unlock(dict_operation_lock);
		DBUG_RETURN(0);
	}

	ut_strcpy(local_name, fts_internal_tbl_name);

	user_table = dd_table_open_on_name(
		thd, &mdl, local_name, false, DICT_ERR_IGNORE_NONE);

	if (!user_table) {
		rw_lock_s_unlock(dict_operation_lock);

		DBUG_RETURN(0);
	} else if (!dict_table_has_fts_index(user_table)) {
		dd_table_close(user_table, thd, &mdl, false);

		rw_lock_s_unlock(dict_operation_lock);

		DBUG_RETURN(0);
	}

	deleted = fts_doc_ids_create();

	trx = trx_allocate_for_background();
	trx->op_info = "Select for FTS DELETE TABLE";

	FTS_INIT_FTS_TABLE(&fts_table,
			   (being_deleted) ? FTS_SUFFIX_BEING_DELETED
			   : FTS_SUFFIX_DELETED,
			   FTS_COMMON_TABLE, user_table);

	fts_table_fetch_doc_ids(trx, &fts_table, deleted);

	fields = table->field;

	for (ulint j = 0; j < ib_vector_size(deleted->doc_ids); ++j) {
		doc_id_t	doc_id;

		doc_id = *(doc_id_t*) ib_vector_get_const(deleted->doc_ids, j);

		OK(fields[I_S_FTS_DOC_ID]->store(doc_id, true));

		OK(schema_table_store_record(thd, table));
	}

	trx_free_for_background(trx);

	fts_doc_ids_free(deleted);

	dd_table_close(user_table, thd, &mdl, false);

	rw_lock_s_unlock(dict_operation_lock);

	DBUG_RETURN(0);
}

/*******************************************************************//**
Fill the dynamic table INFORMATION_SCHEMA.INNODB_FT_DELETED
@return 0 on success, 1 on failure */
static
int
i_s_fts_deleted_fill(
/*=================*/
	THD*		thd,	/*!< in: thread */
	TABLE_LIST*	tables,	/*!< in/out: tables to fill */
	Item*		)	/*!< in: condition (ignored) */
{
	DBUG_ENTER("i_s_fts_deleted_fill");

	DBUG_RETURN(i_s_fts_deleted_generic_fill(thd, tables, FALSE));
}

/*******************************************************************//**
Bind the dynamic table INFORMATION_SCHEMA.INNODB_FT_DELETED
@return 0 on success */
static
int
i_s_fts_deleted_init(
/*=================*/
	void*	p)	/*!< in/out: table schema object */
{
	DBUG_ENTER("i_s_fts_deleted_init");
	ST_SCHEMA_TABLE* schema = (ST_SCHEMA_TABLE*) p;

	schema->fields_info = i_s_fts_doc_fields_info;
	schema->fill_table = i_s_fts_deleted_fill;

	DBUG_RETURN(0);
}

struct st_mysql_plugin	i_s_innodb_ft_deleted =
{
	/* the plugin type (a MYSQL_XXX_PLUGIN value) */
	/* int */
	STRUCT_FLD(type, MYSQL_INFORMATION_SCHEMA_PLUGIN),

	/* pointer to type-specific plugin descriptor */
	/* void* */
	STRUCT_FLD(info, &i_s_info),

	/* plugin name */
	/* const char* */
	STRUCT_FLD(name, "INNODB_FT_DELETED"),

	/* plugin author (for SHOW PLUGINS) */
	/* const char* */
	STRUCT_FLD(author, plugin_author),

	/* general descriptive text (for SHOW PLUGINS) */
	/* const char* */
	STRUCT_FLD(descr, "INNODB AUXILIARY FTS DELETED TABLE"),

	/* the plugin license (PLUGIN_LICENSE_XXX) */
	/* int */
	STRUCT_FLD(license, PLUGIN_LICENSE_GPL),

	/* the function to invoke when plugin is loaded */
	/* int (*)(void*); */
	STRUCT_FLD(init, i_s_fts_deleted_init),

	/* the function to invoke when plugin is un installed */
	/* int (*)(void*); */
	NULL,

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
Fill the dynamic table INFORMATION_SCHEMA.INNODB_FT_BEING_DELETED
@return 0 on success, 1 on failure */
static
int
i_s_fts_being_deleted_fill(
/*=======================*/
	THD*		thd,	/*!< in: thread */
	TABLE_LIST*	tables,	/*!< in/out: tables to fill */
	Item*		)	/*!< in: condition (ignored) */
{
	DBUG_ENTER("i_s_fts_being_deleted_fill");

	DBUG_RETURN(i_s_fts_deleted_generic_fill(thd, tables, TRUE));
}

/*******************************************************************//**
Bind the dynamic table INFORMATION_SCHEMA.INNODB_FT_BEING_DELETED
@return 0 on success */
static
int
i_s_fts_being_deleted_init(
/*=======================*/
	void*	p)	/*!< in/out: table schema object */
{
	DBUG_ENTER("i_s_fts_deleted_init");
	ST_SCHEMA_TABLE* schema = (ST_SCHEMA_TABLE*) p;

	schema->fields_info = i_s_fts_doc_fields_info;
	schema->fill_table = i_s_fts_being_deleted_fill;

	DBUG_RETURN(0);
}

struct st_mysql_plugin	i_s_innodb_ft_being_deleted =
{
	/* the plugin type (a MYSQL_XXX_PLUGIN value) */
	/* int */
	STRUCT_FLD(type, MYSQL_INFORMATION_SCHEMA_PLUGIN),

	/* pointer to type-specific plugin descriptor */
	/* void* */
	STRUCT_FLD(info, &i_s_info),

	/* plugin name */
	/* const char* */
	STRUCT_FLD(name, "INNODB_FT_BEING_DELETED"),

	/* plugin author (for SHOW PLUGINS) */
	/* const char* */
	STRUCT_FLD(author, plugin_author),

	/* general descriptive text (for SHOW PLUGINS) */
	/* const char* */
	STRUCT_FLD(descr, "INNODB AUXILIARY FTS BEING DELETED TABLE"),

	/* the plugin license (PLUGIN_LICENSE_XXX) */
	/* int */
	STRUCT_FLD(license, PLUGIN_LICENSE_GPL),

	/* the function to invoke when plugin is loaded */
	/* int (*)(void*); */
	STRUCT_FLD(init, i_s_fts_being_deleted_init),

	/* the function to invoke when plugin is un installed */
	/* int (*)(void*); */
	NULL,

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

/* Fields of the dynamic table INFORMATION_SCHEMA.INNODB_FT_INDEX_CACHED and
INFORMATION_SCHEMA.INNODB_FT_INDEX_TABLE */
static ST_FIELD_INFO	i_s_fts_index_fields_info[] =
{
#define	I_S_FTS_WORD			0
	{STRUCT_FLD(field_name,		"WORD"),
	 STRUCT_FLD(field_length,	FTS_MAX_WORD_LEN + 1),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_STRING),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	0),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define	I_S_FTS_FIRST_DOC_ID		1
	{STRUCT_FLD(field_name,		"FIRST_DOC_ID"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define	I_S_FTS_LAST_DOC_ID		2
	{STRUCT_FLD(field_name,		"LAST_DOC_ID"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define	I_S_FTS_DOC_COUNT		3
	{STRUCT_FLD(field_name,		"DOC_COUNT"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define	I_S_FTS_ILIST_DOC_ID		4
	{STRUCT_FLD(field_name,		"DOC_ID"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define	I_S_FTS_ILIST_DOC_POS		5
	{STRUCT_FLD(field_name,		"POSITION"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	END_OF_ST_FIELD_INFO
};

/*******************************************************************//**
Go through the Doc Node and its ilist, fill the dynamic table
INFORMATION_SCHEMA.INNODB_FT_INDEX_CACHED for one FTS index on the table.
@return 0 on success, 1 on failure */
static
int
i_s_fts_index_cache_fill_one_index(
/*===============================*/
	fts_index_cache_t*	index_cache,	/*!< in: FTS index cache */
	THD*			thd,		/*!< in: thread */
	TABLE_LIST*		tables)		/*!< in/out: tables to fill */
{
	TABLE*			table = (TABLE*) tables->table;
	Field**			fields;
	CHARSET_INFO*		index_charset;
	const ib_rbt_node_t*	rbt_node;
	fts_string_t		conv_str;
	uint			dummy_errors;
	char*			word_str;

	DBUG_ENTER("i_s_fts_index_cache_fill_one_index");

	fields = table->field;

	index_charset = index_cache->charset;
	conv_str.f_len = system_charset_info->mbmaxlen
		* FTS_MAX_WORD_LEN_IN_CHAR;
	conv_str.f_str = static_cast<byte*>(ut_malloc_nokey(conv_str.f_len));
	conv_str.f_n_char = 0;

	/* Go through each word in the index cache */
	for (rbt_node = rbt_first(index_cache->words);
	     rbt_node;
	     rbt_node = rbt_next(index_cache->words, rbt_node)) {
		fts_tokenizer_word_t* word;

		word = rbt_value(fts_tokenizer_word_t, rbt_node);

		/* Convert word from index charset to system_charset_info */
		if (index_charset->cset != system_charset_info->cset) {
			conv_str.f_n_char = my_convert(
				reinterpret_cast<char*>(conv_str.f_str),
				static_cast<uint32>(conv_str.f_len),
				system_charset_info,
				reinterpret_cast<char*>(word->text.f_str),
				static_cast<uint32>(word->text.f_len),
				index_charset, &dummy_errors);
			ut_ad(conv_str.f_n_char <= conv_str.f_len);
			conv_str.f_str[conv_str.f_n_char] = 0;
			word_str = reinterpret_cast<char*>(conv_str.f_str);
		} else {
			word_str = reinterpret_cast<char*>(word->text.f_str);
		}

		/* Decrypt the ilist, and display Dod ID and word position */
		for (ulint i = 0; i < ib_vector_size(word->nodes); i++) {
			fts_node_t*	node;
			byte*		ptr;
			ulint		decoded = 0;
			doc_id_t	doc_id = 0;

			node = static_cast<fts_node_t*> (ib_vector_get(
				word->nodes, i));

			ptr = node->ilist;

			while (decoded < node->ilist_size) {
				ulint	pos = fts_decode_vlc(&ptr);

				doc_id += pos;

				/* Get position info */
				while (*ptr) {
					pos = fts_decode_vlc(&ptr);

					OK(field_store_string(
						   fields[I_S_FTS_WORD],
						   word_str));

					OK(fields[I_S_FTS_FIRST_DOC_ID]->store(
						   node->first_doc_id,
						   true));

					OK(fields[I_S_FTS_LAST_DOC_ID]->store(
						   node->last_doc_id,
						   true));

					OK(fields[I_S_FTS_DOC_COUNT]->store(
						   node->doc_count, true));

					OK(fields[I_S_FTS_ILIST_DOC_ID]->store(
						   doc_id, true));

					OK(fields[I_S_FTS_ILIST_DOC_POS]->store(
						   pos, true));

					OK(schema_table_store_record(
						   thd, table));
				}

				++ptr;

				decoded = ptr - (byte*) node->ilist;
			}
		}
	}

	ut_free(conv_str.f_str);

	DBUG_RETURN(0);
}
/*******************************************************************//**
Fill the dynamic table INFORMATION_SCHEMA.INNODB_FT_INDEX_CACHED
@return 0 on success, 1 on failure */
static
int
i_s_fts_index_cache_fill(
/*=====================*/
	THD*		thd,	/*!< in: thread */
	TABLE_LIST*	tables,	/*!< in/out: tables to fill */
	Item*		)	/*!< in: condition (ignored) */
{
	dict_table_t*		user_table;
	fts_cache_t*		cache;
	MDL_ticket*		mdl = nullptr;
	char			local_name[MAX_FULL_NAME_LEN];

	DBUG_ENTER("i_s_fts_index_cache_fill");

	/* deny access to non-superusers */
	if (check_global_access(thd, PROCESS_ACL)) {
		DBUG_RETURN(0);
	}

	if (!fts_internal_tbl_name) {
		DBUG_RETURN(0);
	}

	ut_strcpy(local_name, fts_internal_tbl_name);

	user_table = dd_table_open_on_name(
		thd, &mdl, local_name, false, DICT_ERR_IGNORE_NONE);

	if (!user_table) {
		DBUG_RETURN(0);
	}

	if (user_table->fts == NULL || user_table->fts->cache == NULL) {
		dd_table_close(user_table, thd, &mdl, false);

		DBUG_RETURN(0);
	}

	cache = user_table->fts->cache;

	ut_a(cache);

	for (ulint i = 0; i < ib_vector_size(cache->indexes); i++) {
		fts_index_cache_t*      index_cache;

		index_cache = static_cast<fts_index_cache_t*> (
			ib_vector_get(cache->indexes, i));

		i_s_fts_index_cache_fill_one_index(index_cache, thd, tables);
	}

	dd_table_close(user_table, thd, &mdl, false);

	DBUG_RETURN(0);
}

/*******************************************************************//**
Bind the dynamic table INFORMATION_SCHEMA.INNODB_FT_INDEX_CACHE
@return 0 on success */
static
int
i_s_fts_index_cache_init(
/*=====================*/
	void*	p)	/*!< in/out: table schema object */
{
	DBUG_ENTER("i_s_fts_index_cache_init");
	ST_SCHEMA_TABLE* schema = (ST_SCHEMA_TABLE*) p;

	schema->fields_info = i_s_fts_index_fields_info;
	schema->fill_table = i_s_fts_index_cache_fill;

	DBUG_RETURN(0);
}

struct st_mysql_plugin	i_s_innodb_ft_index_cache =
{
	/* the plugin type (a MYSQL_XXX_PLUGIN value) */
	/* int */
	STRUCT_FLD(type, MYSQL_INFORMATION_SCHEMA_PLUGIN),

	/* pointer to type-specific plugin descriptor */
	/* void* */
	STRUCT_FLD(info, &i_s_info),

	/* plugin name */
	/* const char* */
	STRUCT_FLD(name, "INNODB_FT_INDEX_CACHE"),

	/* plugin author (for SHOW PLUGINS) */
	/* const char* */
	STRUCT_FLD(author, plugin_author),

	/* general descriptive text (for SHOW PLUGINS) */
	/* const char* */
	STRUCT_FLD(descr, "INNODB AUXILIARY FTS INDEX CACHED"),

	/* the plugin license (PLUGIN_LICENSE_XXX) */
	/* int */
	STRUCT_FLD(license, PLUGIN_LICENSE_GPL),

	/* the function to invoke when plugin is loaded */
	/* int (*)(void*); */
	STRUCT_FLD(init, i_s_fts_index_cache_init),

	/* the function to invoke when plugin is un installed */
	/* int (*)(void*); */
	NULL,

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
Go through a FTS index auxiliary table, fetch its rows and fill
FTS word cache structure.
@return DB_SUCCESS on success, otherwise error code */
static
dberr_t
i_s_fts_index_table_fill_selected(
/*==============================*/
	dict_index_t*		index,		/*!< in: FTS index */
	ib_vector_t*		words,		/*!< in/out: vector to hold
						fetched words */
	ulint			selected,	/*!< in: selected FTS index */
	fts_string_t*		word)		/*!< in: word to select */
{
	pars_info_t*		info;
	fts_table_t		fts_table;
	trx_t*			trx;
	que_t*			graph;
	dberr_t			error;
	fts_fetch_t		fetch;
	char			table_name[MAX_FULL_NAME_LEN];

	info = pars_info_create();

	fetch.read_arg = words;
	fetch.read_record = fts_optimize_index_fetch_node;
	fetch.total_memory = 0;

	DBUG_EXECUTE_IF("fts_instrument_result_cache_limit",
		fts_result_cache_limit = 8192;
	);

	trx = trx_allocate_for_background();

	trx->op_info = "fetching FTS index nodes";

	pars_info_bind_function(info, "my_func", fetch.read_record, &fetch);
	pars_info_bind_varchar_literal(info, "word", word->f_str, word->f_len);

	FTS_INIT_INDEX_TABLE(&fts_table, fts_get_suffix(selected),
			     FTS_INDEX_TABLE, index);
	fts_get_table_name(&fts_table, table_name);
	pars_info_bind_id(info, true, "table_name", table_name);

	graph = fts_parse_sql(
		&fts_table, info,
		"DECLARE FUNCTION my_func;\n"
		"DECLARE CURSOR c IS"
		" SELECT word, doc_count, first_doc_id, last_doc_id,"
		" ilist\n"
		" FROM $table_name WHERE word >= :word;\n"
		"BEGIN\n"
		"\n"
		"OPEN c;\n"
		"WHILE 1 = 1 LOOP\n"
		"  FETCH c INTO my_func();\n"
		"  IF c % NOTFOUND THEN\n"
		"    EXIT;\n"
		"  END IF;\n"
		"END LOOP;\n"
		"CLOSE c;");

	for (;;) {
		error = fts_eval_sql(trx, graph);

		if (error == DB_SUCCESS) {
			fts_sql_commit(trx);

			break;
		} else {
			fts_sql_rollback(trx);

			if (error == DB_LOCK_WAIT_TIMEOUT) {
				ib::warn() << "Lock wait timeout reading"
					" FTS index. Retrying!";

				trx->error_state = DB_SUCCESS;
			} else {
				ib::error() << "Error occurred while reading"
					" FTS index: " << ut_strerr(error);
				break;
			}
		}
	}

	que_graph_free(graph);

	trx_free_for_background(trx);

	if (fetch.total_memory >= fts_result_cache_limit) {
		error = DB_FTS_EXCEED_RESULT_CACHE_LIMIT;
	}

	return(error);
}

/*******************************************************************//**
Free words. */
static
void
i_s_fts_index_table_free_one_fetch(
/*===============================*/
	ib_vector_t*		words)		/*!< in: words fetched */
{
	for (ulint i = 0; i < ib_vector_size(words); i++) {
		fts_word_t*	word;

		word = static_cast<fts_word_t*>(ib_vector_get(words, i));

		for (ulint j = 0; j < ib_vector_size(word->nodes); j++) {
			fts_node_t*     node;

			node = static_cast<fts_node_t*> (ib_vector_get(
				word->nodes, j));
			ut_free(node->ilist);
		}

		fts_word_free(word);
	}

	ib_vector_reset(words);
}

/*******************************************************************//**
Go through words, fill INFORMATION_SCHEMA.INNODB_FT_INDEX_TABLE.
@return	0 on success, 1 on failure */
static
int
i_s_fts_index_table_fill_one_fetch(
/*===============================*/
	CHARSET_INFO*		index_charset,	/*!< in: FTS index charset */
	THD*			thd,		/*!< in: thread */
	TABLE_LIST*		tables,		/*!< in/out: tables to fill */
	ib_vector_t*		words,		/*!< in: words fetched */
	fts_string_t*		conv_str,	/*!< in: string for conversion*/
	bool			has_more)	/*!< in: has more to fetch */
{
	TABLE*			table = (TABLE*) tables->table;
	Field**			fields;
	uint			dummy_errors;
	char*			word_str;
	ulint			words_size;
	int			ret = 0;

	DBUG_ENTER("i_s_fts_index_table_fill_one_fetch");

	fields = table->field;

	words_size = ib_vector_size(words);
	if (has_more) {
		/* the last word is not fetched completely. */
		ut_ad(words_size > 1);
		words_size -= 1;
	}

	/* Go through each word in the index cache */
	for (ulint i = 0; i < words_size; i++) {
		fts_word_t*	word;

		word = static_cast<fts_word_t*>(ib_vector_get(words, i));

		word->text.f_str[word->text.f_len] = 0;

		/* Convert word from index charset to system_charset_info */
		if (index_charset->cset != system_charset_info->cset) {
			conv_str->f_n_char = my_convert(
				reinterpret_cast<char*>(conv_str->f_str),
				static_cast<uint32>(conv_str->f_len),
				system_charset_info,
				reinterpret_cast<char*>(word->text.f_str),
				static_cast<uint32>(word->text.f_len),
				index_charset, &dummy_errors);
			ut_ad(conv_str->f_n_char <= conv_str->f_len);
			conv_str->f_str[conv_str->f_n_char] = 0;
			word_str = reinterpret_cast<char*>(conv_str->f_str);
		} else {
			word_str = reinterpret_cast<char*>(word->text.f_str);
		}

		/* Decrypt the ilist, and display Dod ID and word position */
		for (ulint i = 0; i < ib_vector_size(word->nodes); i++) {
			fts_node_t*	node;
			byte*		ptr;
			ulint		decoded = 0;
			doc_id_t	doc_id = 0;

			node = static_cast<fts_node_t*> (ib_vector_get(
				word->nodes, i));

			ptr = node->ilist;

			while (decoded < node->ilist_size) {
				ulint	pos = fts_decode_vlc(&ptr);

				doc_id += pos;

				/* Get position info */
				while (*ptr) {
					pos = fts_decode_vlc(&ptr);

					OK(field_store_string(
						   fields[I_S_FTS_WORD],
						   word_str));

					OK(fields[I_S_FTS_FIRST_DOC_ID]->store(
						   node->first_doc_id, true));

					OK(fields[I_S_FTS_LAST_DOC_ID]->store(
						   node->last_doc_id, true));

					OK(fields[I_S_FTS_DOC_COUNT]->store(
						   node->doc_count, true));

					OK(fields[I_S_FTS_ILIST_DOC_ID]->store(
						   doc_id, true));

					OK(fields[I_S_FTS_ILIST_DOC_POS]->store(
						   pos, true));

					OK(schema_table_store_record(
						   thd, table));
				}

				++ptr;

				decoded = ptr - (byte*) node->ilist;
			}
		}
	}

	i_s_fts_index_table_free_one_fetch(words);

	DBUG_RETURN(ret);
}

/*******************************************************************//**
Go through a FTS index and its auxiliary tables, fetch rows in each table
and fill INFORMATION_SCHEMA.INNODB_FT_INDEX_TABLE.
@return 0 on success, 1 on failure */
static
int
i_s_fts_index_table_fill_one_index(
/*===============================*/
	dict_index_t*		index,		/*!< in: FTS index */
	THD*			thd,		/*!< in: thread */
	TABLE_LIST*		tables)		/*!< in/out: tables to fill */
{
	ib_vector_t*		words;
	mem_heap_t*		heap;
	CHARSET_INFO*		index_charset;
	fts_string_t		conv_str;
	dberr_t			error;
	int			ret = 0;

	DBUG_ENTER("i_s_fts_index_table_fill_one_index");
	DBUG_ASSERT(!dict_index_is_online_ddl(index));

	heap = mem_heap_create(1024);

	words = ib_vector_create(ib_heap_allocator_create(heap),
				 sizeof(fts_word_t), 256);

	index_charset = fts_index_get_charset(index);
	conv_str.f_len = system_charset_info->mbmaxlen
		* FTS_MAX_WORD_LEN_IN_CHAR;
	conv_str.f_str = static_cast<byte*>(ut_malloc_nokey(conv_str.f_len));
	conv_str.f_n_char = 0;

	/* Iterate through each auxiliary table as described in
	fts_index_selector */
	for (ulint selected = 0; selected < FTS_NUM_AUX_INDEX; selected++) {
		fts_string_t	word;
		bool		has_more = false;

		word.f_str = NULL;
		word.f_len = 0;
		word.f_n_char = 0;

		do {
			/* Fetch from index */
			error = i_s_fts_index_table_fill_selected(
				index, words, selected, &word);

			if (error == DB_SUCCESS) {
				has_more = false;
			} else if (error == DB_FTS_EXCEED_RESULT_CACHE_LIMIT) {
				has_more = true;
			} else {
				i_s_fts_index_table_free_one_fetch(words);
				ret = 1;
				goto func_exit;
			}

			if (has_more) {
				fts_word_t*	last_word;

				/* Prepare start point for next fetch */
				last_word = static_cast<fts_word_t*>(ib_vector_last(words));
				ut_ad(last_word != NULL);
				fts_string_dup(&word, &last_word->text, heap);
			}

			/* Fill into tables */
			ret = i_s_fts_index_table_fill_one_fetch(
				index_charset, thd, tables, words, &conv_str, has_more);

			if (ret != 0) {
				i_s_fts_index_table_free_one_fetch(words);
				goto func_exit;
			}
		} while (has_more);
	}

func_exit:
	ut_free(conv_str.f_str);
	mem_heap_free(heap);

	DBUG_RETURN(ret);
}
/*******************************************************************//**
Fill the dynamic table INFORMATION_SCHEMA.INNODB_FT_INDEX_TABLE
@return 0 on success, 1 on failure */
static
int
i_s_fts_index_table_fill(
/*=====================*/
	THD*		thd,	/*!< in: thread */
	TABLE_LIST*	tables,	/*!< in/out: tables to fill */
	Item*		)	/*!< in: condition (ignored) */
{
	dict_table_t*		user_table;
	dict_index_t*		index;
	MDL_ticket*		mdl = nullptr;
	char			local_name[MAX_FULL_NAME_LEN];

	DBUG_ENTER("i_s_fts_index_table_fill");

	/* deny access to non-superusers */
	if (check_global_access(thd, PROCESS_ACL)) {
		DBUG_RETURN(0);
	}

	/* Prevent DDL to drop fts aux tables. */
	rw_lock_s_lock(dict_operation_lock);

	if (!fts_internal_tbl_name) {
		rw_lock_s_unlock(dict_operation_lock);
		DBUG_RETURN(0);
	}

	ut_strcpy(local_name, fts_internal_tbl_name);

	user_table = dd_table_open_on_name(
		thd, &mdl, local_name, false, DICT_ERR_IGNORE_NONE);

	if (!user_table) {
		rw_lock_s_unlock(dict_operation_lock);

		DBUG_RETURN(0);
	}

	for (index = user_table->first_index();
	     index; index = index->next()) {
		if (index->type & DICT_FTS) {
			i_s_fts_index_table_fill_one_index(index, thd, tables);
		}
	}

	dd_table_close(user_table, thd, &mdl, false);

	rw_lock_s_unlock(dict_operation_lock);

	DBUG_RETURN(0);
}

/*******************************************************************//**
Bind the dynamic table INFORMATION_SCHEMA.INNODB_FT_INDEX_TABLE
@return 0 on success */
static
int
i_s_fts_index_table_init(
/*=====================*/
	void*	p)	/*!< in/out: table schema object */
{
	DBUG_ENTER("i_s_fts_index_table_init");
	ST_SCHEMA_TABLE* schema = (ST_SCHEMA_TABLE*) p;

	schema->fields_info = i_s_fts_index_fields_info;
	schema->fill_table = i_s_fts_index_table_fill;

	DBUG_RETURN(0);
}

struct st_mysql_plugin	i_s_innodb_ft_index_table =
{
	/* the plugin type (a MYSQL_XXX_PLUGIN value) */
	/* int */
	STRUCT_FLD(type, MYSQL_INFORMATION_SCHEMA_PLUGIN),

	/* pointer to type-specific plugin descriptor */
	/* void* */
	STRUCT_FLD(info, &i_s_info),

	/* plugin name */
	/* const char* */
	STRUCT_FLD(name, "INNODB_FT_INDEX_TABLE"),

	/* plugin author (for SHOW PLUGINS) */
	/* const char* */
	STRUCT_FLD(author, plugin_author),

	/* general descriptive text (for SHOW PLUGINS) */
	/* const char* */
	STRUCT_FLD(descr, "INNODB AUXILIARY FTS INDEX TABLE"),

	/* the plugin license (PLUGIN_LICENSE_XXX) */
	/* int */
	STRUCT_FLD(license, PLUGIN_LICENSE_GPL),

	/* the function to invoke when plugin is loaded */
	/* int (*)(void*); */
	STRUCT_FLD(init, i_s_fts_index_table_init),

	/* the function to invoke when plugin is un installed */
	/* int (*)(void*); */
	NULL,

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

/* Fields of the dynamic table INFORMATION_SCHEMA.INNODB_FT_CONFIG */
static ST_FIELD_INFO	i_s_fts_config_fields_info[] =
{
#define	FTS_CONFIG_KEY			0
	{STRUCT_FLD(field_name,		"KEY"),
	 STRUCT_FLD(field_length,	NAME_LEN + 1),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_STRING),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	0),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define	FTS_CONFIG_VALUE		1
	{STRUCT_FLD(field_name,		"VALUE"),
	 STRUCT_FLD(field_length,	NAME_LEN + 1),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_STRING),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	0),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	END_OF_ST_FIELD_INFO
};

static const char* fts_config_key[] = {
	FTS_OPTIMIZE_LIMIT_IN_SECS,
	FTS_SYNCED_DOC_ID,
	FTS_STOPWORD_TABLE_NAME,
	FTS_USE_STOPWORD,
	NULL
};

/*******************************************************************//**
Fill the dynamic table INFORMATION_SCHEMA.INNODB_FT_CONFIG
@return 0 on success, 1 on failure */
static
int
i_s_fts_config_fill(
/*================*/
	THD*		thd,		/*!< in: thread */
	TABLE_LIST*	tables,		/*!< in/out: tables to fill */
	Item*		)	/*!< in: condition (ignored) */
{
	Field**			fields;
	TABLE*			table = (TABLE*) tables->table;
	trx_t*			trx;
	fts_table_t		fts_table;
	dict_table_t*		user_table;
	ulint			i = 0;
	dict_index_t*		index = NULL;
	unsigned char		str[FTS_MAX_CONFIG_VALUE_LEN + 1];
	MDL_ticket*		mdl = nullptr;
	char			local_name[MAX_FULL_NAME_LEN];

	DBUG_ENTER("i_s_fts_config_fill");

	/* deny access to non-superusers */
	if (check_global_access(thd, PROCESS_ACL)) {
		DBUG_RETURN(0);
	}

	if (!fts_internal_tbl_name) {
		DBUG_RETURN(0);
	}

	ut_strcpy(local_name, fts_internal_tbl_name);

	DEBUG_SYNC_C("i_s_fts_config_fille_check");

	fields = table->field;

	if (innobase_strcasecmp(local_name, "default") == 0) {
		DBUG_RETURN(0);
	}

	/* Prevent DDL to drop fts aux tables. */
	rw_lock_s_lock(dict_operation_lock);

	user_table = dd_table_open_on_name(
		thd, &mdl, local_name, false, DICT_ERR_IGNORE_NONE);

	if (!user_table) {
		rw_lock_s_unlock(dict_operation_lock);

		DBUG_RETURN(0);
	} else if (!dict_table_has_fts_index(user_table)) {
		dd_table_close(user_table, thd, &mdl, false);

		rw_lock_s_unlock(dict_operation_lock);

		DBUG_RETURN(0);
	}

	trx = trx_allocate_for_background();
	trx->op_info = "Select for FTS CONFIG TABLE";

	FTS_INIT_FTS_TABLE(&fts_table, FTS_SUFFIX_CONFIG,
			   FTS_COMMON_TABLE, user_table);

	if (!ib_vector_is_empty(user_table->fts->indexes)) {
		index = (dict_index_t*) ib_vector_getp_const(
				user_table->fts->indexes, 0);
		DBUG_ASSERT(!dict_index_is_online_ddl(index));
	}

	while (fts_config_key[i]) {
		fts_string_t	value;
		char*		key_name;
		ulint		allocated = FALSE;

		value.f_len = FTS_MAX_CONFIG_VALUE_LEN;

		value.f_str = str;

		if (index
		    && strcmp(fts_config_key[i], FTS_TOTAL_WORD_COUNT) == 0) {
			key_name = fts_config_create_index_param_name(
				fts_config_key[i], index);
			allocated = TRUE;
		} else {
			key_name = (char*) fts_config_key[i];
		}

		fts_config_get_value(trx, &fts_table, key_name, &value);

		if (allocated) {
			ut_free(key_name);
		}

		OK(field_store_string(
			fields[FTS_CONFIG_KEY], fts_config_key[i]));

		OK(field_store_string(
			fields[FTS_CONFIG_VALUE], (const char*) value.f_str));

		OK(schema_table_store_record(thd, table));

		i++;
	}

	fts_sql_commit(trx);

	trx_free_for_background(trx);

	dd_table_close(user_table, thd, &mdl, false);

	rw_lock_s_unlock(dict_operation_lock);

	DBUG_RETURN(0);
}

/*******************************************************************//**
Bind the dynamic table INFORMATION_SCHEMA.INNODB_FT_CONFIG
@return 0 on success */
static
int
i_s_fts_config_init(
/*=================*/
	void*	p)	/*!< in/out: table schema object */
{
	DBUG_ENTER("i_s_fts_config_init");
	ST_SCHEMA_TABLE* schema = (ST_SCHEMA_TABLE*) p;

	schema->fields_info = i_s_fts_config_fields_info;
	schema->fill_table = i_s_fts_config_fill;

	DBUG_RETURN(0);
}

struct st_mysql_plugin	i_s_innodb_ft_config =
{
	/* the plugin type (a MYSQL_XXX_PLUGIN value) */
	/* int */
	STRUCT_FLD(type, MYSQL_INFORMATION_SCHEMA_PLUGIN),

	/* pointer to type-specific plugin descriptor */
	/* void* */
	STRUCT_FLD(info, &i_s_info),

	/* plugin name */
	/* const char* */
	STRUCT_FLD(name, "INNODB_FT_CONFIG"),

	/* plugin author (for SHOW PLUGINS) */
	/* const char* */
	STRUCT_FLD(author, plugin_author),

	/* general descriptive text (for SHOW PLUGINS) */
	/* const char* */
	STRUCT_FLD(descr, "INNODB AUXILIARY FTS CONFIG TABLE"),

	/* the plugin license (PLUGIN_LICENSE_XXX) */
	/* int */
	STRUCT_FLD(license, PLUGIN_LICENSE_GPL),

	/* the function to invoke when plugin is loaded */
	/* int (*)(void*); */
	STRUCT_FLD(init, i_s_fts_config_init),

	/* the function to invoke when plugin is un installed */
	/* int (*)(void*); */
	NULL,

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

/* Fields of the dynamic table INNODB_TEMP_TABLE_INFO. */
static ST_FIELD_INFO	i_s_innodb_temp_table_info_fields_info[] =
{
#define IDX_TEMP_TABLE_ID		0
	{STRUCT_FLD(field_name,		"TABLE_ID"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define IDX_TEMP_TABLE_NAME		1
	{STRUCT_FLD(field_name,		"NAME"),
	 STRUCT_FLD(field_length,	NAME_CHAR_LEN),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_STRING),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_MAYBE_NULL),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define IDX_TEMP_TABLE_N_COLS		2
	{STRUCT_FLD(field_name,		"N_COLS"),
	 STRUCT_FLD(field_length,	MY_INT32_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define IDX_TEMP_TABLE_SPACE_ID		3
	{STRUCT_FLD(field_name,		"SPACE"),
	 STRUCT_FLD(field_length,	MY_INT32_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},
	END_OF_ST_FIELD_INFO
};

struct temp_table_info_t{
	table_id_t	m_table_id;
	char		m_table_name[NAME_LEN + 1];
	unsigned	m_n_cols;
	unsigned	m_space_id;
};

typedef std::vector<temp_table_info_t, ut_allocator<temp_table_info_t> >
	temp_table_info_cache_t;

/*******************************************************************//**
Fill Information Schema table INNODB_TEMP_TABLE_INFO for a particular
temp-table
@return 0 on success, 1 on failure */
static
int
i_s_innodb_temp_table_info_fill(
/*=============================*/
	THD*				thd,		/*!< in: thread */
	TABLE_LIST*			tables,		/*!< in/out: tables
							to fill */
	const temp_table_info_t*	info)		/*!< in: temp-table
							information */
{
	TABLE*			table;
	Field**			fields;

	DBUG_ENTER("i_s_innodb_temp_table_info_fill");

	table = tables->table;

	fields = table->field;

	OK(fields[IDX_TEMP_TABLE_ID]->store(info->m_table_id, true));

	OK(field_store_string(
		   fields[IDX_TEMP_TABLE_NAME], info->m_table_name));

	OK(fields[IDX_TEMP_TABLE_N_COLS]->store(info->m_n_cols));

	OK(fields[IDX_TEMP_TABLE_SPACE_ID]->store(info->m_space_id));

	DBUG_RETURN(schema_table_store_record(thd, table));
}

/** Populate current table information to cache
@param[in]	table	table
@param[in,out]	cache	populate data in this cache */
static
void
innodb_temp_table_populate_cache(
	const dict_table_t*	table,
	temp_table_info_t*	cache)
{
	cache->m_table_id = table->id;

	char	db_utf8[MAX_DB_UTF8_LEN];
	char	table_utf8[MAX_TABLE_UTF8_LEN];

	dict_fs2utf8(table->name.m_name,
		     db_utf8, sizeof(db_utf8),
		     table_utf8, sizeof(table_utf8));
	strcpy(cache->m_table_name, table_utf8);

	cache->m_n_cols = table->n_cols;

	cache->m_space_id = table->space;
}

/*******************************************************************//**
This function will iterate over all available table and will fill
stats for temp-tables to INNODB_TEMP_TABLE_INFO.
@return 0 on success, 1 on failure */
static
int
i_s_innodb_temp_table_info_fill_table(
/*===================================*/
	THD*		thd,		/*!< in: thread */
	TABLE_LIST*	tables,		/*!< in/out: tables to fill */
	Item*		)		/*!< in: condition (ignored) */
{
	int			status	= 0;
	dict_table_t*		table	= NULL;

	DBUG_ENTER("i_s_innodb_temp_table_info_fill_table");

	/* Only allow the PROCESS privilege holder to access the stats */
	if (check_global_access(thd, PROCESS_ACL)) {
		DBUG_RETURN(0);
	}

	/* First populate all temp-table info by acquiring dict_sys->mutex.
	Note: Scan is being done on NON-LRU list which mainly has system
	table entries and temp-table entries. This means 2 things: list
	is smaller so processing would be faster and most of the data
	is relevant */
	temp_table_info_cache_t all_temp_info_cache;
	all_temp_info_cache.reserve(UT_LIST_GET_LEN(dict_sys->table_non_LRU));

	mutex_enter(&dict_sys->mutex);
	for (table = UT_LIST_GET_FIRST(dict_sys->table_non_LRU);
	     table != NULL;
	     table = UT_LIST_GET_NEXT(table_LRU, table)) {

		if (!table->is_temporary()) {
			continue;
		}

		temp_table_info_t current_temp_table_info;

		innodb_temp_table_populate_cache(
			table, &current_temp_table_info);

		all_temp_info_cache.push_back(current_temp_table_info);
	}
	mutex_exit(&dict_sys->mutex);

	/* Now populate the info to MySQL table */
	temp_table_info_cache_t::const_iterator end = all_temp_info_cache.end();
	for (temp_table_info_cache_t::const_iterator it
		= all_temp_info_cache.begin();
	     it != end;
	     it++) {
		status = i_s_innodb_temp_table_info_fill(thd, tables, &(*it));
		if (status) {
			break;
		}
	}

	DBUG_RETURN(status);
}

/*******************************************************************//**
Bind the dynamic table INFORMATION_SCHEMA.INNODB_TEMP_TABLE_INFO.
@return 0 on success, 1 on failure */
static
int
i_s_innodb_temp_table_info_init(
/*=============================*/
	void*	p)	/*!< in/out: table schema object */
{
	ST_SCHEMA_TABLE*	schema;

	DBUG_ENTER("i_s_innodb_temp_table_info_init");

	schema = reinterpret_cast<ST_SCHEMA_TABLE*>(p);

	schema->fields_info = i_s_innodb_temp_table_info_fields_info;
	schema->fill_table = i_s_innodb_temp_table_info_fill_table;

	DBUG_RETURN(0);
}

struct st_mysql_plugin	i_s_innodb_temp_table_info =
{
	/* the plugin type (a MYSQL_XXX_PLUGIN value) */
	/* int */
	STRUCT_FLD(type, MYSQL_INFORMATION_SCHEMA_PLUGIN),

	/* pointer to type-specific plugin descriptor */
	/* void* */
	STRUCT_FLD(info, &i_s_info),

	/* plugin name */
	/* const char* */
	STRUCT_FLD(name, "INNODB_TEMP_TABLE_INFO"),

	/* plugin author (for SHOW PLUGINS) */
	/* const char* */
	STRUCT_FLD(author, plugin_author),

	/* general descriptive text (for SHOW PLUGINS) */
	/* const char* */
	STRUCT_FLD(descr, "InnoDB Temp Table Stats"),

	/* the plugin license (PLUGIN_LICENSE_XXX) */
	/* int */
	STRUCT_FLD(license, PLUGIN_LICENSE_GPL),

	/* the function to invoke when plugin is loaded */
	/* int (*)(void*); */
	STRUCT_FLD(init, i_s_innodb_temp_table_info_init),

	/* the function to invoke when plugin is un installed */
	/* int (*)(void*); */
	NULL,

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
@return 0 on success, 1 on failure */
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

	OK(fields[IDX_BUF_STATS_POOL_ID]->store(
		   info->pool_unique_id, true));

	OK(fields[IDX_BUF_STATS_POOL_SIZE]->store(
		   info->pool_size, true));

	OK(fields[IDX_BUF_STATS_LRU_LEN]->store(
		   info->lru_len, true));

	OK(fields[IDX_BUF_STATS_OLD_LRU_LEN]->store(
		   info->old_lru_len, true));

	OK(fields[IDX_BUF_STATS_FREE_BUFFERS]->store(
		   info->free_list_len, true));

	OK(fields[IDX_BUF_STATS_FLUSH_LIST_LEN]->store(
		   info->flush_list_len, true));

	OK(fields[IDX_BUF_STATS_PENDING_ZIP]->store(
		   info->n_pend_unzip, true));

	OK(fields[IDX_BUF_STATS_PENDING_READ]->store(
		   info->n_pend_reads, true));

	OK(fields[IDX_BUF_STATS_FLUSH_LRU]->store(
		   info->n_pending_flush_lru, true));

	OK(fields[IDX_BUF_STATS_FLUSH_LIST]->store(
		   info->n_pending_flush_list, true));

	OK(fields[IDX_BUF_STATS_PAGE_YOUNG]->store(
		   info->n_pages_made_young, true));

	OK(fields[IDX_BUF_STATS_PAGE_NOT_YOUNG]->store(
		   info->n_pages_not_made_young, true));

	OK(fields[IDX_BUF_STATS_PAGE_YOUNG_RATE]->store(
		   info->page_made_young_rate));

	OK(fields[IDX_BUF_STATS_PAGE_NOT_YOUNG_RATE]->store(
		   info->page_not_made_young_rate));

	OK(fields[IDX_BUF_STATS_PAGE_READ]->store(
		   info->n_pages_read, true));

	OK(fields[IDX_BUF_STATS_PAGE_CREATED]->store(
		   info->n_pages_created, true));

	OK(fields[IDX_BUF_STATS_PAGE_WRITTEN]->store(
		   info->n_pages_written, true));

	OK(fields[IDX_BUF_STATS_GET]->store(
		   info->n_page_gets, true));

	OK(fields[IDX_BUF_STATS_PAGE_READ_RATE]->store(
		   info->pages_read_rate));

	OK(fields[IDX_BUF_STATS_PAGE_CREATE_RATE]->store(
		   info->pages_created_rate));

	OK(fields[IDX_BUF_STATS_PAGE_WRITTEN_RATE]->store(
		   info->pages_written_rate));

	if (info->n_page_get_delta) {
		OK(fields[IDX_BUF_STATS_HIT_RATE]->store(
			   1000 - (1000 * info->page_read_delta
				   / info->n_page_get_delta), true));

		OK(fields[IDX_BUF_STATS_MADE_YOUNG_PCT]->store(
			   1000 * info->young_making_delta
			   / info->n_page_get_delta, true));

		OK(fields[IDX_BUF_STATS_NOT_MADE_YOUNG_PCT]->store(
			   1000 * info->not_young_making_delta
			   / info->n_page_get_delta, true));
	} else {
		OK(fields[IDX_BUF_STATS_HIT_RATE]->store(0, true));
		OK(fields[IDX_BUF_STATS_MADE_YOUNG_PCT]->store(0, true));
		OK(fields[IDX_BUF_STATS_NOT_MADE_YOUNG_PCT]->store(0, true));
	}

	OK(fields[IDX_BUF_STATS_READ_AHREAD]->store(
		   info->n_ra_pages_read, true));

	OK(fields[IDX_BUF_STATS_READ_AHEAD_EVICTED]->store(
		   info->n_ra_pages_evicted, true));

	OK(fields[IDX_BUF_STATS_READ_AHEAD_RATE]->store(
		   info->pages_readahead_rate));

	OK(fields[IDX_BUF_STATS_READ_AHEAD_EVICT_RATE]->store(
		   info->pages_evicted_rate));

	OK(fields[IDX_BUF_STATS_LRU_IO_SUM]->store(
		   info->io_sum, true));

	OK(fields[IDX_BUF_STATS_LRU_IO_CUR]->store(
		   info->io_cur, true));

	OK(fields[IDX_BUF_STATS_UNZIP_SUM]->store(
		   info->unzip_sum, true));

	OK(fields[IDX_BUF_STATS_UNZIP_CUR]->store(
		   info->unzip_cur, true));

	DBUG_RETURN(schema_table_store_record(thd, table));
}

/*******************************************************************//**
This is the function that loops through each buffer pool and fetch buffer
pool stats to information schema  table: I_S_INNODB_BUFFER_POOL_STATS
@return 0 on success, 1 on failure */
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

	pool_info = (buf_pool_info_t*) ut_zalloc_nokey(
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

	ut_free(pool_info);

	DBUG_RETURN(status);
}

/*******************************************************************//**
Bind the dynamic table INFORMATION_SCHEMA.INNODB_BUFFER_POOL_STATS.
@return 0 on success, 1 on failure */
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

struct st_mysql_plugin	i_s_innodb_buffer_stats =
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

	/* the function to invoke when plugin is un installed */
	/* int (*)(void*); */
	NULL,

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
@return 0 on success, 1 on failure */
static
int
i_s_innodb_buffer_page_fill(
/*========================*/
	THD*			thd,		/*!< in: thread */
	TABLE_LIST*		tables,		/*!< in/out: tables to fill */
	const buf_page_info_t*	info_array,	/*!< in: array cached page
						info */
	ulint			num_page)	/*!< in: number of page info
						cached */
{
	TABLE*			table;
	Field**			fields;

	DBUG_ENTER("i_s_innodb_buffer_page_fill");

	table = tables->table;

	fields = table->field;

	/* Iterate through the cached array and fill the I_S table rows */
	for (ulint i = 0; i < num_page; i++) {
		const buf_page_info_t*	page_info;
		char			table_name[MAX_FULL_NAME_LEN + 1];
		const char*		table_name_end = NULL;
		const char*		state_str;
		enum buf_page_state	state;

		page_info = info_array + i;

		state_str = NULL;

		OK(fields[IDX_BUFFER_POOL_ID]->store(
			   page_info->pool_id, true));

		OK(fields[IDX_BUFFER_BLOCK_ID]->store(
			   page_info->block_id, true));

		OK(fields[IDX_BUFFER_PAGE_SPACE]->store(
			   page_info->space_id, true));

		OK(fields[IDX_BUFFER_PAGE_NUM]->store(
			   page_info->page_num, true));

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
			   page_info->newest_mod, true));

		OK(fields[IDX_BUFFER_PAGE_OLDEST_MOD]->store(
			   page_info->oldest_mod, true));

		OK(fields[IDX_BUFFER_PAGE_ACCESS_TIME]->store(
			   page_info->access_time));

		fields[IDX_BUFFER_PAGE_TABLE_NAME]->set_null();

		fields[IDX_BUFFER_PAGE_INDEX_NAME]->set_null();

		/* If this is an index page, fetch the index name
		and table name */
		switch (page_info->page_type) {
			const dict_index_t*	index;

		case I_S_PAGE_TYPE_INDEX:
		case I_S_PAGE_TYPE_RTREE:
		case I_S_PAGE_TYPE_SDI:
			{
				index_id_t	id(page_info->space_id,
						   page_info->index_id);

				mutex_enter(&dict_sys->mutex);
				index = dict_index_find(id);
			}

			if (index) {

				table_name_end = innobase_convert_name(
					table_name, sizeof(table_name),
					index->table_name,
					strlen(index->table_name),
					thd);

				OK(fields[IDX_BUFFER_PAGE_TABLE_NAME]->store(
					table_name,
					static_cast<size_t>(table_name_end - table_name),
					system_charset_info));
				fields[IDX_BUFFER_PAGE_TABLE_NAME]->set_notnull();

				OK(field_store_index_name(
					fields[IDX_BUFFER_PAGE_INDEX_NAME],
					index->name));
			}

			mutex_exit(&dict_sys->mutex);
		}

		OK(fields[IDX_BUFFER_PAGE_NUM_RECS]->store(
			   page_info->num_recs, true));

		OK(fields[IDX_BUFFER_PAGE_DATA_SIZE]->store(
			   page_info->data_size, true));

		OK(fields[IDX_BUFFER_PAGE_ZIP_SIZE]->store(
			   page_info->zip_ssize
			   ? (UNIV_ZIP_SIZE_MIN >> 1) << page_info->zip_ssize
			   : 0, true));

#if BUF_PAGE_STATE_BITS > 3
# error "BUF_PAGE_STATE_BITS > 3, please ensure that all 1<<BUF_PAGE_STATE_BITS values are checked for"
#endif
		state = static_cast<enum buf_page_state>(page_info->page_state);

		switch (state) {
		/* First three states are for compression pages and
		are not states we would get as we scan pages through
		buffer blocks */
		case BUF_BLOCK_POOL_WATCH:
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
			   page_info->freed_page_clock, true));

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
	if (fil_page_type_is_index(page_type)) {
		const page_t*	page = (const page_t*) frame;

		page_info->index_id = btr_page_get_index_id(page);

		/* FIL_PAGE_INDEX and FIL_PAGE_RTREE are a bit special,
		their values are defined as 17855 and 17854, so we cannot
		use them to index into i_s_page_type[] array, its array index
		in the i_s_page_type[] array is I_S_PAGE_TYPE_INDEX
		(1) for index pages or I_S_PAGE_TYPE_IBUF for
		change buffer index pages */
		if (page_info->index_id
		    == static_cast<space_index_t>(
			    DICT_IBUF_ID_MIN + IBUF_SPACE_ID)) {
			page_info->page_type = I_S_PAGE_TYPE_IBUF;
		} else if (page_type == FIL_PAGE_RTREE) {
			page_info->page_type = I_S_PAGE_TYPE_RTREE;
		} else if (page_type == FIL_PAGE_SDI) {
			page_info->page_type = I_S_PAGE_TYPE_SDI;
		} else {
			page_info->page_type = I_S_PAGE_TYPE_INDEX;
		}

		page_info->data_size = (ulint)(page_header_get_field(
			page, PAGE_HEAP_TOP) - (page_is_comp(page)
						? PAGE_NEW_SUPREMUM_END
						: PAGE_OLD_SUPREMUM_END)
			- page_header_get_field(page, PAGE_GARBAGE));

		page_info->num_recs = page_get_n_recs(page);
	} else if (page_type > FIL_PAGE_TYPE_LAST) {
		/* Encountered an unknown page type */
		page_info->page_type = I_S_PAGE_TYPE_UNKNOWN;
	} else {
		/* Make sure we get the right index into the
		i_s_page_type[] array */
		ut_a(page_type == i_s_page_type[page_type].type_value);

		page_info->page_type = page_type;
	}

	switch (page_info->page_type) {
	case FIL_PAGE_TYPE_ZBLOB:
	case FIL_PAGE_TYPE_ZBLOB2:
	case FIL_PAGE_SDI_ZBLOB:
	case FIL_PAGE_TYPE_LOB_INDEX:
	case FIL_PAGE_TYPE_LOB_DATA:
	case FIL_PAGE_TYPE_LOB_FIRST:
	case FIL_PAGE_TYPE_ZLOB_FIRST:
	case FIL_PAGE_TYPE_ZLOB_DATA:
	case FIL_PAGE_TYPE_ZLOB_INDEX:
	case FIL_PAGE_TYPE_ZLOB_FRAG:
	case FIL_PAGE_TYPE_ZLOB_FRAG_ENTRY:
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
	BPageMutex*	mutex = buf_page_get_mutex(bpage);

	ut_ad(pool_id < MAX_BUFFER_POOLS);

	page_info->pool_id = pool_id;

	page_info->block_id = pos;

	mutex_enter(mutex);

	page_info->page_state = buf_page_get_state(bpage);

	/* Only fetch information for buffers that map to a tablespace,
	that is, buffer page with state BUF_BLOCK_ZIP_PAGE,
	BUF_BLOCK_ZIP_DIRTY or BUF_BLOCK_FILE_PAGE */
	if (buf_page_in_file(bpage)) {
		const byte*	frame;
		ulint		page_type;

		page_info->space_id = bpage->id.space();

		page_info->page_num = bpage->id.page_no();

		page_info->flush_type = bpage->flush_type;

		page_info->fix_count = bpage->buf_fix_count;

		page_info->newest_mod = bpage->newest_modification;

		page_info->oldest_mod = bpage->oldest_modification;

		page_info->access_time = bpage->access_time;

		page_info->zip_ssize = bpage->zip.ssize;

		page_info->io_fix = bpage->io_fix;

		page_info->is_old = bpage->old;

		page_info->freed_page_clock = bpage->freed_page_clock;

		switch (buf_page_get_io_fix(bpage)) {
		case BUF_IO_NONE:
		case BUF_IO_WRITE:
		case BUF_IO_PIN:
			break;
		case BUF_IO_READ:
			page_info->page_type = I_S_PAGE_TYPE_UNKNOWN;
			mutex_exit(mutex);
			return;
		}

		if (page_info->page_state == BUF_BLOCK_FILE_PAGE) {
			const buf_block_t*block;

			block = reinterpret_cast<const buf_block_t*>(bpage);
			frame = block->frame;
			/* Note: this may be a false positive, that
			is, block->index will not always be set to
			NULL when the last adaptive hash index
			reference is dropped. */
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

	mutex_exit(mutex);
}

/*******************************************************************//**
This is the function that goes through each block of the buffer pool
and fetch information to information schema tables: INNODB_BUFFER_PAGE.
@return 0 on success, 1 on failure */
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
	for (ulint n = 0;
	     n < ut_min(buf_pool->n_chunks, buf_pool->n_chunks_new); n++) {
		const buf_block_t*	block;
		ulint			n_blocks;
		buf_page_info_t*	info_buffer;
		ulint			num_page;
		ulint			mem_size;
		ulint			chunk_size;
		ulint			num_to_process = 0;
		ulint			block_id = 0;

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
			the buffer pool */
			info_buffer = (buf_page_info_t*) mem_heap_zalloc(
				heap, mem_size);

			/* GO through each block in the chunk */
			for (n_blocks = num_to_process; n_blocks--; block++) {
				i_s_innodb_buffer_page_get_info(
					&block->page, pool_id, block_id,
					info_buffer + num_page);
				block_id++;
				num_page++;
			}

			/* Fill in information schema table with information
			just collected from the buffer chunk scan */
			status = i_s_innodb_buffer_page_fill(
				thd, tables, info_buffer,
				num_page);

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
@return 0 on success, 1 on failure */
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
@return 0 on success, 1 on failure */
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

struct st_mysql_plugin	i_s_innodb_buffer_page =
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

	/* the function to invoke when plugin is un installed */
	/* int (*)(void*); */
	NULL,

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
@return 0 on success, 1 on failure */
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
		char			table_name[MAX_FULL_NAME_LEN + 1];
		const char*		table_name_end = NULL;
		const char*		state_str;
		enum buf_page_state	state;

		state_str = NULL;

		page_info = info_array + i;

		OK(fields[IDX_BUF_LRU_POOL_ID]->store(
			   page_info->pool_id, true));

		OK(fields[IDX_BUF_LRU_POS]->store(
			   page_info->block_id, true));

		OK(fields[IDX_BUF_LRU_PAGE_SPACE]->store(
			   page_info->space_id, true));

		OK(fields[IDX_BUF_LRU_PAGE_NUM]->store(
			   page_info->page_num, true));

		OK(field_store_string(
			   fields[IDX_BUF_LRU_PAGE_TYPE],
			   i_s_page_type[page_info->page_type].type_str));

		OK(fields[IDX_BUF_LRU_PAGE_FLUSH_TYPE]->store(
			   page_info->flush_type, true));

		OK(fields[IDX_BUF_LRU_PAGE_FIX_COUNT]->store(
			   page_info->fix_count, true));

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
			   page_info->access_time, true));

		fields[IDX_BUF_LRU_PAGE_TABLE_NAME]->set_null();

		fields[IDX_BUF_LRU_PAGE_INDEX_NAME]->set_null();

		/* If this is an index page, fetch the index name
		and table name */
		if (page_info->page_type == I_S_PAGE_TYPE_INDEX) {
			index_id_t		id(page_info->space_id,
						   page_info->index_id);
			const dict_index_t*	index;

			mutex_enter(&dict_sys->mutex);
			index = dict_index_find(id);

			if (index) {

				table_name_end = innobase_convert_name(
					table_name, sizeof(table_name),
					index->table_name,
					strlen(index->table_name),
					thd);

				OK(fields[IDX_BUF_LRU_PAGE_TABLE_NAME]->store(
					table_name,
					static_cast<size_t>(table_name_end - table_name),
					system_charset_info));
				fields[IDX_BUF_LRU_PAGE_TABLE_NAME]->set_notnull();

				OK(field_store_index_name(
					fields[IDX_BUF_LRU_PAGE_INDEX_NAME],
					index->name));
			}

			mutex_exit(&dict_sys->mutex);
		}

		OK(fields[IDX_BUF_LRU_PAGE_NUM_RECS]->store(
			   page_info->num_recs, true));

		OK(fields[IDX_BUF_LRU_PAGE_DATA_SIZE]->store(
			   page_info->data_size, true));

		OK(fields[IDX_BUF_LRU_PAGE_ZIP_SIZE]->store(
			   page_info->zip_ssize ?
			   512 << page_info->zip_ssize : 0, true));

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
		case BUF_BLOCK_POOL_WATCH:
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
			   page_info->freed_page_clock, true));

		if (schema_table_store_record(thd, table)) {
			mem_heap_free(heap);
			DBUG_RETURN(1);
		}

		mem_heap_empty(heap);
	}

	mem_heap_free(heap);

	DBUG_RETURN(0);
}

/** This is the function that goes through buffer pool's LRU list
and fetch information to INFORMATION_SCHEMA.INNODB_BUFFER_PAGE_LRU.
@param[in]	thd		thread
@param[in,out]	tables		tables to fill
@param[in]	buf_pool	buffer pool to scan
@param[in]	pool_id		buffer pool id
@return 0 on success, 1 on failure */
static
int
i_s_innodb_fill_buffer_lru(
	THD*			thd,
	TABLE_LIST*		tables,
	buf_pool_t*		buf_pool,
	const ulint		pool_id)
{
	int			status = 0;
	buf_page_info_t*	info_buffer;
	ulint			lru_pos = 0;
	const buf_page_t*	bpage;
	ulint			lru_len;

	DBUG_ENTER("i_s_innodb_fill_buffer_lru");

	/* Obtain buf_pool->LRU_list_mutex before allocate info_buffer, since
	UT_LIST_GET_LEN(buf_pool->LRU) could change */
	mutex_enter(&buf_pool->LRU_list_mutex);

	lru_len = UT_LIST_GET_LEN(buf_pool->LRU);

	/* Print error message if malloc fail */
	info_buffer = (buf_page_info_t*) my_malloc(PSI_INSTRUMENT_ME,
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
		/* Use the same function that collect buffer info for
		INNODB_BUFFER_PAGE to get buffer page info */
		i_s_innodb_buffer_page_get_info(bpage, pool_id, lru_pos,
						(info_buffer + lru_pos));

		bpage = UT_LIST_GET_PREV(LRU, bpage);

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
@return 0 on success, 1 on failure */
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
@return 0 on success, 1 on failure */
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

struct st_mysql_plugin	i_s_innodb_buffer_page_lru =
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

	/* the function to invoke when plugin is un installed */
	/* int (*)(void*); */
	NULL,

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
@return 0 on success */
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

/**  INNODB_TABLES  ***************************************************/
/* Fields of the dynamic table INFORMATION_SCHEMA.INNODB_TABLES */
static ST_FIELD_INFO	innodb_tables_fields_info[] =
{
#define INNODB_TABLES_ID			0
	{STRUCT_FLD(field_name,		"TABLE_ID"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define INNODB_TABLES_NAME			1
	{STRUCT_FLD(field_name,		"NAME"),
	 STRUCT_FLD(field_length,	MAX_FULL_NAME_LEN + 1),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_STRING),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	0),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define INNODB_TABLES_FLAG			2
	{STRUCT_FLD(field_name,		"FLAG"),
	 STRUCT_FLD(field_length,	MY_INT32_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	0),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define INNODB_TABLES_NUM_COLUMN		3
	{STRUCT_FLD(field_name,		"N_COLS"),
	 STRUCT_FLD(field_length,	MY_INT32_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	0),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define INNODB_TABLES_SPACE		4
	{STRUCT_FLD(field_name,		"SPACE"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	0),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define INNODB_TABLES_ROW_FORMAT		5
	{STRUCT_FLD(field_name,		"ROW_FORMAT"),
	 STRUCT_FLD(field_length,	12),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_STRING),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_MAYBE_NULL),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define INNODB_TABLES_ZIP_PAGE_SIZE	6
	{STRUCT_FLD(field_name,		"ZIP_PAGE_SIZE"),
	 STRUCT_FLD(field_length,	MY_INT32_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define INNODB_TABLES_SPACE_TYPE		7
	{STRUCT_FLD(field_name,		"SPACE_TYPE"),
	 STRUCT_FLD(field_length,	10),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_STRING),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_MAYBE_NULL),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	END_OF_ST_FIELD_INFO
};

/** Populate information_schema.innodb_tables table with information
from INNODB_TABLES.
@param[in]	thd		thread
@param[in]	table		table obj
@param[in,out]	table_to_fill	fill this table
@return 0 on success */
static
int
i_s_dict_fill_innodb_tables(
	THD*		thd,
	dict_table_t*	table,
	TABLE*		table_to_fill)
{
	Field**			fields;
	ulint			compact = DICT_TF_GET_COMPACT(table->flags);
	ulint			atomic_blobs = DICT_TF_HAS_ATOMIC_BLOBS(
								table->flags);
	const page_size_t&	page_size = dict_tf_get_page_size(table->flags);
	const char*		row_format;
	const char*		space_type;

	if (!compact) {
		row_format = "Redundant";
	} else if (!atomic_blobs) {
		row_format = "Compact";
	} else if (DICT_TF_GET_ZIP_SSIZE(table->flags)) {
		row_format = "Compressed";
	} else {
		row_format = "Dynamic";
	}

	if (fsp_is_system_or_temp_tablespace(table->space)) {
		space_type = "System";
	} else if (DICT_TF_HAS_SHARED_SPACE(table->flags)) {
		space_type = "General";
	} else {
		space_type = "Single";
	}

	DBUG_ENTER("i_s_dict_fill_innodb_tables");

	fields = table_to_fill->field;

	OK(fields[INNODB_TABLES_ID]->store(longlong(table->id), TRUE));

	OK(field_store_string(fields[INNODB_TABLES_NAME], table->name.m_name));

	OK(fields[INNODB_TABLES_FLAG]->store(table->flags));

	OK(fields[INNODB_TABLES_NUM_COLUMN]->store(table->n_cols));

	OK(fields[INNODB_TABLES_SPACE]->store(table->space));

	OK(field_store_string(fields[INNODB_TABLES_ROW_FORMAT], row_format));

	OK(fields[INNODB_TABLES_ZIP_PAGE_SIZE]->store(page_size.is_compressed()
						   ? page_size.physical()
						   : 0, true));

	OK(field_store_string(fields[INNODB_TABLES_SPACE_TYPE], space_type));

	OK(schema_table_store_record(thd, table_to_fill));

	DBUG_RETURN(0);
}

/** Function to go through each record in INNODB_TABLES table, and fill the
information_schema.innodb_tables table with related table information
@param[in]	thd		thread
@param[in,out]	tables		tables to fill
@return 0 on success */
static
int
i_s_innodb_tables_fill_table(
	THD*		thd,
	TABLE_LIST*	tables,
	Item*		)
{
	btr_pcur_t	pcur;
	const rec_t*	rec;
	mem_heap_t*	heap;
	mtr_t		mtr;
	MDL_ticket*	mdl = nullptr;
	dict_table_t*	dd_tables;

	DBUG_ENTER("i_s_innodb_tables_fill_table");

	/* deny access to user without PROCESS_ACL privilege */
	if (check_global_access(thd, PROCESS_ACL)) {
		DBUG_RETURN(0);
	}

	heap = mem_heap_create(1000);
	mutex_enter(&dict_sys->mutex);
	mtr_start(&mtr);

	rec = dd_startscan_system(thd, &mdl, &pcur, &mtr,
				  dd_tables_name.c_str(), &dd_tables);

	while (rec) {
		dict_table_t*	table_rec;
		MDL_ticket*	mdl_on_tab = nullptr;

		/* Fetch the dict_table_t structure corresponding to
		this INNODB_TABLES record */
		dd_process_dd_tables_rec_and_mtr_commit(
			heap, rec, &table_rec, dd_tables, &mdl_on_tab, &mtr);

		mutex_exit(&dict_sys->mutex);
		if (table_rec != NULL) {
			i_s_dict_fill_innodb_tables(thd, table_rec, tables->table);
		}

		mem_heap_empty(heap);

		/* Get the next record */
		mutex_enter(&dict_sys->mutex);

		if (table_rec != NULL) {
			dd_table_close(table_rec, thd, &mdl_on_tab, true);
		}

		mtr_start(&mtr);
		rec = dd_getnext_system_rec(&pcur, &mtr);
	}

	mtr_commit(&mtr);
	dd_table_close(dd_tables, thd, &mdl, true);

	/* Scan mysql.partitions */
	mem_heap_empty(heap);
	mtr_start(&mtr);

	rec = dd_startscan_system(thd, &mdl, &pcur, &mtr,
				  dd_partitions_name.c_str(), &dd_tables);

	while (rec) {
		dict_table_t*	table_rec;
		MDL_ticket*	mdl_on_tab = nullptr;

		/* Fetch the dict_table_t structure corresponding to
		this INNODB_TABLES record */
		dd_process_dd_partitions_rec_and_mtr_commit(
			heap, rec, &table_rec, dd_tables, &mdl_on_tab, &mtr);

		mutex_exit(&dict_sys->mutex);
		if (table_rec != NULL) {
			i_s_dict_fill_innodb_tables(thd, table_rec, tables->table);
		}

		mem_heap_empty(heap);

		/* Get the next record */
		mutex_enter(&dict_sys->mutex);

		if (table_rec != NULL) {
			dd_table_close(table_rec, thd, &mdl_on_tab, true);
		}

		mtr_start(&mtr);
		rec = dd_getnext_system_rec(&pcur, &mtr);
	}

	mtr_commit(&mtr);
	dd_table_close(dd_tables, thd, &mdl, true);
	mutex_exit(&dict_sys->mutex);

	mem_heap_free(heap);

	DBUG_RETURN(0);
}

/** Bind the dynamic table INFORMATION_SCHEMA.innodb_tables
@param[in,out]	p	table schema object
@return 0 on success */
static
int
innodb_tables_init(
	void*	p)
{
	ST_SCHEMA_TABLE*	schema;

	DBUG_ENTER("innodb_tables_init");

	schema = (ST_SCHEMA_TABLE*) p;

	schema->fields_info = innodb_tables_fields_info;
	schema->fill_table = i_s_innodb_tables_fill_table;

	DBUG_RETURN(0);
}

struct st_mysql_plugin	i_s_innodb_tables =
{
	/* the plugin type (a MYSQL_XXX_PLUGIN value) */
	/* int */
	STRUCT_FLD(type, MYSQL_INFORMATION_SCHEMA_PLUGIN),

	/* pointer to type-specific plugin descriptor */
	/* void* */
	STRUCT_FLD(info, &i_s_info),

	/* plugin name */
	/* const char* */
	STRUCT_FLD(name, "INNODB_TABLES"),

	/* plugin author (for SHOW PLUGINS) */
	/* const char* */
	STRUCT_FLD(author, plugin_author),

	/* general descriptive text (for SHOW PLUGINS) */
	/* const char* */
	STRUCT_FLD(descr, "InnoDB INNODB_TABLES"),

	/* the plugin license (PLUGIN_LICENSE_XXX) */
	/* int */
	STRUCT_FLD(license, PLUGIN_LICENSE_GPL),

	/* the function to invoke when plugin is loaded */
	/* int (*)(void*); */
	STRUCT_FLD(init, innodb_tables_init),

	/* the function to invoke when plugin is un installed */
	/* int (*)(void*); */
	NULL,

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

/**  INNODB_TABLESTATS  ***********************************************/
/* Fields of the dynamic table INFORMATION_SCHEMA.INNODB_TABLESTATS */
static ST_FIELD_INFO	innodb_tablestats_fields_info[] =
{
#define INNODB_TABLESTATS_ID		0
	{STRUCT_FLD(field_name,		"TABLE_ID"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define INNODB_TABLESTATS_NAME		1
	{STRUCT_FLD(field_name,		"NAME"),
	 STRUCT_FLD(field_length,	NAME_LEN + 1),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_STRING),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	0),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define INNODB_TABLESTATS_INIT		2
	{STRUCT_FLD(field_name,		"STATS_INITIALIZED"),
	 STRUCT_FLD(field_length,	NAME_LEN + 1),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_STRING),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	0),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define INNODB_TABLESTATS_NROW		3
	{STRUCT_FLD(field_name,		"NUM_ROWS"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define INNODB_TABLESTATS_CLUST_SIZE	4
	{STRUCT_FLD(field_name,		"CLUST_INDEX_SIZE"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define INNODB_TABLESTATS_INDEX_SIZE	5
	{STRUCT_FLD(field_name,		"OTHER_INDEX_SIZE"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define INNODB_TABLESTATS_MODIFIED		6
	{STRUCT_FLD(field_name,		"MODIFIED_COUNTER"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define INNODB_TABLESTATS_AUTONINC		7
	{STRUCT_FLD(field_name,		"AUTOINC"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define INNODB_TABLESTATS_TABLE_REF_COUNT	8
	{STRUCT_FLD(field_name,		"REF_COUNT"),
	 STRUCT_FLD(field_length,	MY_INT32_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	0),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	END_OF_ST_FIELD_INFO
};

/** Populate information_schema.innodb_tablestats table with information
from INNODB_TABLES.
@param[in]	thd		thread ID
@param[in,out]	table		table
@param[in]	ref_count	table reference count
@param[in,out]	table_to_fill	fill this table
@return 0 on success */
static
int
i_s_dict_fill_innodb_tablestats(
	THD*		thd,
	dict_table_t*	table,
	ulint		ref_count,
	TABLE*		table_to_fill)
{
	Field**		fields;

	DBUG_ENTER("i_s_dict_fill_innodb_tablestats");

	fields = table_to_fill->field;

	OK(fields[INNODB_TABLESTATS_ID]->store(longlong(table->id), TRUE));

	OK(field_store_string(fields[INNODB_TABLESTATS_NAME],
			      table->name.m_name));

	dict_table_stats_lock(table, RW_S_LATCH);

	if (table->stat_initialized) {
		OK(field_store_string(fields[INNODB_TABLESTATS_INIT],
				      "Initialized"));

		OK(fields[INNODB_TABLESTATS_NROW]->store(table->stat_n_rows,
						      true));

		OK(fields[INNODB_TABLESTATS_CLUST_SIZE]->store(
			   table->stat_clustered_index_size, true));

		OK(fields[INNODB_TABLESTATS_INDEX_SIZE]->store(
			   table->stat_sum_of_other_index_sizes, true));

		OK(fields[INNODB_TABLESTATS_MODIFIED]->store(
			   table->stat_modified_counter, true));
	} else {
		OK(field_store_string(fields[INNODB_TABLESTATS_INIT],
				      "Uninitialized"));

		OK(fields[INNODB_TABLESTATS_NROW]->store(0, true));

		OK(fields[INNODB_TABLESTATS_CLUST_SIZE]->store(0, true));

		OK(fields[INNODB_TABLESTATS_INDEX_SIZE]->store(0, true));

		OK(fields[INNODB_TABLESTATS_MODIFIED]->store(0, true));
	}

	dict_table_stats_unlock(table, RW_S_LATCH);

	OK(fields[INNODB_TABLESTATS_AUTONINC]->store(table->autoinc, true));

	OK(fields[INNODB_TABLESTATS_TABLE_REF_COUNT]->store(ref_count, true));

	OK(schema_table_store_record(thd, table_to_fill));

	DBUG_RETURN(0);
}

/** Function to go through each record in INNODB_TABLES table, and fill the
information_schema.innodb_tablestats table with table statistics
related information
@param[in]	thd		thread
@param[in,out]	tables		tables to fill
@return 0 on success */
static
int
i_s_innodb_tables_fill_table_stats(
	THD*		thd,
	TABLE_LIST*	tables,
	Item*		)
{
	btr_pcur_t	pcur;
	const rec_t*	rec;
	mem_heap_t*	heap;
	mtr_t		mtr;
	MDL_ticket*	mdl = nullptr;
	dict_table_t*	dd_tables;

	DBUG_ENTER("i_s_innodb_tables_fill_table_stats");

	/* deny access to user without PROCESS_ACL privilege */
	if (check_global_access(thd, PROCESS_ACL)) {
		DBUG_RETURN(0);
	}

	heap = mem_heap_create(1000);

	/* Prevent DDL to drop tables. */
	mutex_enter(&dict_sys->mutex);
	mtr_start(&mtr);
	rec = dd_startscan_system(thd, &mdl, &pcur, &mtr, dd_tables_name.c_str(),
				  &dd_tables);

	while (rec) {
		dict_table_t*	table_rec;
		MDL_ticket*	mdl_on_tab = nullptr;
		ulint		ref_count = 0;
#ifdef UNIV_DEBUG
		bool		dd_closed;

		dd_closed = false;
#endif

		/* Fetch the dict_table_t structure corresponding to
		this INNODB_TABLES record */
		dd_process_dd_tables_rec_and_mtr_commit(
			heap, rec, &table_rec, dd_tables, &mdl_on_tab, &mtr);
		if (table_rec != NULL) {
			ref_count = table_rec->get_ref_count();
		}

		mutex_exit(&dict_sys->mutex);

		if (table_rec != NULL) {
			i_s_dict_fill_innodb_tablestats(thd, table_rec, ref_count,
						     tables->table);
		}

		mem_heap_empty(heap);

		/* Get the next record */
		mutex_enter(&dict_sys->mutex);

		if (table_rec != NULL
#ifdef UNIV_DEBUG
		    && !dd_closed
#endif
		    ) {
			dd_table_close(table_rec, thd, &mdl_on_tab, true);
		}

		mtr_start(&mtr);
		rec = dd_getnext_system_rec(&pcur, &mtr);
	}

	mtr_commit(&mtr);
	dd_table_close(dd_tables, thd, &mdl, true);
	mutex_exit(&dict_sys->mutex);
	mem_heap_free(heap);

	DBUG_RETURN(0);
}

/** Bind the dynamic table INFORMATION_SCHEMA.innodb_tablestats
@param[in,out]	p	table schema object
@return 0 on success */
static
int
innodb_tablestats_init(
	void*	p)
{
	ST_SCHEMA_TABLE*	schema;

	DBUG_ENTER("innodb_tablestats_init");

	schema = (ST_SCHEMA_TABLE*) p;

	schema->fields_info = innodb_tablestats_fields_info;
	schema->fill_table = i_s_innodb_tables_fill_table_stats;

	DBUG_RETURN(0);
}

struct st_mysql_plugin	i_s_innodb_tablestats =
{
	/* the plugin type (a MYSQL_XXX_PLUGIN value) */
	/* int */
	STRUCT_FLD(type, MYSQL_INFORMATION_SCHEMA_PLUGIN),

	/* pointer to type-specific plugin descriptor */
	/* void* */
	STRUCT_FLD(info, &i_s_info),

	/* plugin name */
	/* const char* */
	STRUCT_FLD(name, "INNODB_TABLESTATS"),

	/* plugin author (for SHOW PLUGINS) */
	/* const char* */
	STRUCT_FLD(author, plugin_author),

	/* general descriptive text (for SHOW PLUGINS) */
	/* const char* */
	STRUCT_FLD(descr, "InnoDB INNODB_TABLESTATS"),

	/* the plugin license (PLUGIN_LICENSE_XXX) */
	/* int */
	STRUCT_FLD(license, PLUGIN_LICENSE_GPL),

	/* the function to invoke when plugin is loaded */
	/* int (*)(void*); */
	STRUCT_FLD(init, innodb_tablestats_init),

	/* the function to invoke when plugin is un installed */
	/* int (*)(void*); */
	NULL,

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

/**  INNODB_INDEXES  **************************************************/
/* Fields of the dynamic table INFORMATION_SCHEMA.INNODB_INDEXES */
static ST_FIELD_INFO	innodb_sysindex_fields_info[] =
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

#define SYS_INDEX_MERGE_THRESHOLD 7
	{STRUCT_FLD(field_name,		"MERGE_THRESHOLD"),
	 STRUCT_FLD(field_length,	MY_INT32_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	0),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	END_OF_ST_FIELD_INFO
};

/** Function to populate the information_schema.innodb_indexes table with
collected index information
@param[in]	thd		thread
@param[in]	index		dict_index_t obj
@param[in,out]	table_to_fill	fill this table
@return 0 on success */
static
int
i_s_dict_fill_innodb_indexes(
	THD*			thd,
	const dict_index_t*	index,
	TABLE*			table_to_fill)
{
	Field**		fields;

	DBUG_ENTER("i_s_dict_fill_innodb_indexes");

	fields = table_to_fill->field;

	OK(field_store_index_name(fields[SYS_INDEX_NAME], index->name));

	OK(fields[SYS_INDEX_ID]->store(longlong(index->id), true));

	OK(fields[SYS_INDEX_TABLE_ID]->store(longlong(index->table->id), true));

	OK(fields[SYS_INDEX_TYPE]->store(index->type));

	OK(fields[SYS_INDEX_NUM_FIELDS]->store(index->n_fields));

	/* FIL_NULL is ULINT32_UNDEFINED */
	if (index->page == FIL_NULL) {
		OK(fields[SYS_INDEX_PAGE_NO]->store(-1));
	} else {
		OK(fields[SYS_INDEX_PAGE_NO]->store(index->page));
	}

	OK(fields[SYS_INDEX_SPACE]->store(index->space));

	OK(fields[SYS_INDEX_MERGE_THRESHOLD]->store(index->merge_threshold));

	OK(schema_table_store_record(thd, table_to_fill));

	DBUG_RETURN(0);
}

/** Function to go through each record in INNODB_INDEXES table, and fill the
information_schema.innodb_indexes table with related index information
@param[in]	thd		thread
@param[in,out]	tables		tables to fill
@return 0 on success */
static
int
i_s_innodb_indexes_fill_table(
	THD*		thd,
	TABLE_LIST*	tables,
	Item*		)
{
	btr_pcur_t	pcur;
	const rec_t*	rec;
	mem_heap_t*	heap;
	mtr_t		mtr;
	MDL_ticket*	mdl = nullptr;
	dict_table_t*	dd_indexes;
	bool		ret;

	DBUG_ENTER("i_s_innodb_indexes_fill_table");

	/* deny access to user without PROCESS_ACL privilege */
	if (check_global_access(thd, PROCESS_ACL)) {
		DBUG_RETURN(0);
	}

	heap = mem_heap_create(1000);
	mutex_enter(&dict_sys->mutex);
	mtr_start(&mtr);

	/* Start scan the mysql.indexes */
	rec = dd_startscan_system(thd, &mdl, &pcur, &mtr,
				  dd_indexes_name.c_str(), &dd_indexes);

	/* Process each record in the table */
	while (rec) {
		const dict_index_t*	index_rec;
		MDL_ticket*		mdl_on_tab = nullptr;
		dict_table_t*		parent = nullptr;
		MDL_ticket*		mdl_on_parent = nullptr;

		/* Populate a dict_index_t structure with information from
		a INNODB_INDEXES row */
		ret = dd_process_dd_indexes_rec(heap, rec, &index_rec, &mdl_on_tab,
						&parent, &mdl_on_parent,
						dd_indexes, &mtr);

		mutex_exit(&dict_sys->mutex);

		if (ret) {
			i_s_dict_fill_innodb_indexes(thd, index_rec, tables->table);
		}

		mem_heap_empty(heap);

		/* Get the next record */
		mutex_enter(&dict_sys->mutex);

		if (index_rec != NULL) {
			dd_table_close(index_rec->table, thd, &mdl_on_tab, true);

			/* Close parent table if it's a fts aux table. */
			if (index_rec->table->is_fts_aux() && parent) {
				dd_table_close(parent, thd, &mdl_on_parent, true);
			}
		}

		mtr_start(&mtr);
		rec = dd_getnext_system_rec(&pcur, &mtr);
	}

	mtr_commit(&mtr);
	dd_table_close(dd_indexes, thd, &mdl, true);
	mutex_exit(&dict_sys->mutex);
	mem_heap_free(heap);

	DBUG_RETURN(0);
}

/** Bind the dynamic table INFORMATION_SCHEMA.innodb_indexes
@param[in,out]	p	table schema object
@return 0 on success */
static
int
innodb_indexes_init(
	void*	p)
{
	ST_SCHEMA_TABLE*	schema;

	DBUG_ENTER("innodb_indexes_init");

	schema = (ST_SCHEMA_TABLE*) p;

	schema->fields_info = innodb_sysindex_fields_info;
	schema->fill_table = i_s_innodb_indexes_fill_table;

	DBUG_RETURN(0);
}

struct st_mysql_plugin	i_s_innodb_indexes =
{
	/* the plugin type (a MYSQL_XXX_PLUGIN value) */
	/* int */
	STRUCT_FLD(type, MYSQL_INFORMATION_SCHEMA_PLUGIN),

	/* pointer to type-specific plugin descriptor */
	/* void* */
	STRUCT_FLD(info, &i_s_info),

	/* plugin name */
	/* const char* */
	STRUCT_FLD(name, "INNODB_INDEXES"),

	/* plugin author (for SHOW PLUGINS) */
	/* const char* */
	STRUCT_FLD(author, plugin_author),

	/* general descriptive text (for SHOW PLUGINS) */
	/* const char* */
	STRUCT_FLD(descr, "InnoDB INNODB_INDEXES"),

	/* the plugin license (PLUGIN_LICENSE_XXX) */
	/* int */
	STRUCT_FLD(license, PLUGIN_LICENSE_GPL),

	/* the function to invoke when plugin is loaded */
	/* int (*)(void*); */
	STRUCT_FLD(init, innodb_indexes_init),

	/* the function to invoke when plugin is un installed */
	/* int (*)(void*); */
	NULL,

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

/**  INNODB_COLUMNS  **************************************************/
/* Fields of the dynamic table INFORMATION_SCHEMA.INNODB_COLUMNS */
static ST_FIELD_INFO	innodb_columns_fields_info[] =
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

/** Function to populate the information_schema.innodb_columns with
related column information
@param[in]	thd		thread
@param[in]	table_id	table id
@param[in]	col_name	column name
@param[in]	column		dict_col_t obj
@param[in]	nth_v_col	virtual column, its sequence number
@param[in,out]	table_to_fill	fill this table
@return 0 on success */
static
int
i_s_dict_fill_innodb_columns(
	THD*		thd,
	table_id_t	table_id,
	const char*	col_name,
	dict_col_t*	column,
	ulint		nth_v_col,
	TABLE*		table_to_fill)
{
	Field**		fields;

	DBUG_ENTER("i_s_dict_fill_innodb_columns");

	fields = table_to_fill->field;

	OK(fields[SYS_COLUMN_TABLE_ID]->store((longlong)table_id, true));

	OK(field_store_string(fields[SYS_COLUMN_NAME], col_name));

	if (column->is_virtual()) {
		ulint	pos = dict_create_v_col_pos(nth_v_col, column->ind);
		OK(fields[SYS_COLUMN_POSITION]->store(pos, true));
	} else {
		OK(fields[SYS_COLUMN_POSITION]->store(column->ind, true));
	}

	OK(fields[SYS_COLUMN_MTYPE]->store(column->mtype));

	OK(fields[SYS_COLUMN__PRTYPE]->store(column->prtype));

	OK(fields[SYS_COLUMN_COLUMN_LEN]->store(column->len));

	OK(schema_table_store_record(thd, table_to_fill));

	DBUG_RETURN(0);
}

/** Function to fill information_schema.innodb_columns with information
collected by scanning INNODB_COLUMNS table.
@param[in]	thd		thread
@param[in,out]	tables		tables to fill
@return 0 on success */
static
int
i_s_innodb_columns_fill_table(
	THD*		thd,
	TABLE_LIST*	tables,
	Item*		)
{
	btr_pcur_t	pcur;
	const rec_t*	rec;
	char*		col_name;
	mem_heap_t*	heap;
	mtr_t		mtr;
	MDL_ticket*	mdl = nullptr;
	dict_table_t*	dd_columns;
	bool		ret;

	DBUG_ENTER("i_s_innodb_columns_fill_table");

	/* deny access to user without PROCESS_ACL privilege */
	if (check_global_access(thd, PROCESS_ACL)) {
		DBUG_RETURN(0);
	}

	heap = mem_heap_create(1000);
	mutex_enter(&dict_sys->mutex);
	mtr_start(&mtr);

	/* Start scan the mysql.columns */
	rec = dd_startscan_system(thd, &mdl, &pcur, &mtr,
				  dd_columns_name.c_str(), &dd_columns);

	while (rec) {
		dict_col_t	column_rec;
		table_id_t	table_id;
		ulint		nth_v_col;

		/* populate a dict_col_t structure with information from
		a row */
		ret = dd_process_dd_columns_rec(
			heap, rec, &column_rec, &table_id,
			&col_name, &nth_v_col, dd_columns, &mtr);

		mutex_exit(&dict_sys->mutex);

		if (ret) {
			i_s_dict_fill_innodb_columns(thd, table_id, col_name,
						 &column_rec, nth_v_col,
						 tables->table);
		}

		mem_heap_empty(heap);

		/* Get the next record */
		mutex_enter(&dict_sys->mutex);
		mtr_start(&mtr);
		rec = dd_getnext_system_rec(&pcur, &mtr);
	}

	mtr_commit(&mtr);
	dd_table_close(dd_columns, thd, &mdl, true);
	mutex_exit(&dict_sys->mutex);
	mem_heap_free(heap);

	DBUG_RETURN(0);
}
/** Bind the dynamic table INFORMATION_SCHEMA.innodb_columns
@param[in,out]	p	table schema object
@return 0 on success */
static
int
innodb_columns_init(
	void*	p)
{
	ST_SCHEMA_TABLE*	schema;

	DBUG_ENTER("innodb_columns_init");

	schema = (ST_SCHEMA_TABLE*) p;

	schema->fields_info = innodb_columns_fields_info;
	schema->fill_table = i_s_innodb_columns_fill_table;

	DBUG_RETURN(0);
}

struct st_mysql_plugin	i_s_innodb_columns =
{
	/* the plugin type (a MYSQL_XXX_PLUGIN value) */
	/* int */
	STRUCT_FLD(type, MYSQL_INFORMATION_SCHEMA_PLUGIN),

	/* pointer to type-specific plugin descriptor */
	/* void* */
	STRUCT_FLD(info, &i_s_info),

	/* plugin name */
	/* const char* */
	STRUCT_FLD(name, "INNODB_COLUMNS"),

	/* plugin author (for SHOW PLUGINS) */
	/* const char* */
	STRUCT_FLD(author, plugin_author),

	/* general descriptive text (for SHOW PLUGINS) */
	/* const char* */
	STRUCT_FLD(descr, "InnoDB INNODB_COLUMNS"),

	/* the plugin license (PLUGIN_LICENSE_XXX) */
	/* int */
	STRUCT_FLD(license, PLUGIN_LICENSE_GPL),

	/* the function to invoke when plugin is loaded */
	/* int (*)(void*); */
	STRUCT_FLD(init, innodb_columns_init),

	/* the function to invoke when plugin is un installed */
	/* int (*)(void*); */
	NULL,

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

/**  INNODB_VIRTUAL **************************************************/
/** Fields of the dynamic table INFORMATION_SCHEMA.INNODB_VIRTUAL */
static ST_FIELD_INFO	innodb_virtual_fields_info[] =
{
#define INNODB_VIRTUAL_TABLE_ID		0
	{STRUCT_FLD(field_name,		"TABLE_ID"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define INNODB_VIRTUAL_POS			1
	{STRUCT_FLD(field_name,		"POS"),
	 STRUCT_FLD(field_length,	MY_INT32_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define INNODB_VIRTUAL_BASE_POS		2
	{STRUCT_FLD(field_name,		"BASE_POS"),
	 STRUCT_FLD(field_length,	MY_INT32_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	END_OF_ST_FIELD_INFO
};

/** Function to populate the information_schema.innodb_virtual with
related information
param[in]	thd		thread
param[in]	table_id	table ID
param[in]	pos		virtual column position
param[in]	base_pos	base column position
param[in,out]	table_to_fill	fill this table
@return 0 on success */
static
int
i_s_dict_fill_innodb_virtual(
	THD*		thd,
	table_id_t	table_id,
	ulint		pos,
	ulint		base_pos,
	TABLE*		table_to_fill)
{
	Field**		fields;

	DBUG_ENTER("i_s_dict_fill_innodb_virtual");

	fields = table_to_fill->field;

	OK(fields[INNODB_VIRTUAL_TABLE_ID]->store(table_id, true));

	OK(fields[INNODB_VIRTUAL_POS]->store(pos, true));

	OK(fields[INNODB_VIRTUAL_BASE_POS]->store(base_pos, true));

	OK(schema_table_store_record(thd, table_to_fill));

	DBUG_RETURN(0);
}

/** Function to fill information_schema.innodb_virtual with information
collected by scanning INNODB_VIRTUAL table.
param[in]	thd		thread
param[in,out]	tables		tables to fill
param[in]	item		condition (not used)
@return 0 on success */
static
int
i_s_innodb_virtual_fill_table(
	THD*		thd,
	TABLE_LIST*	tables,
	Item*		)
{
	btr_pcur_t	pcur;
	const rec_t*	rec;
	mem_heap_t*	heap;
	mtr_t		mtr;
	MDL_ticket*	mdl = nullptr;
	dict_table_t*	dd_columns;
	bool		ret;

	DBUG_ENTER("i_s_innodb_columns_fill_table");

	/* deny access to user without PROCESS_ACL privilege */
	if (check_global_access(thd, PROCESS_ACL)) {
		DBUG_RETURN(0);
	}

	heap = mem_heap_create(1000);
	mutex_enter(&dict_sys->mutex);
	mtr_start(&mtr);

	/* Start scan the mysql.columns */
	rec = dd_startscan_system(thd, &mdl, &pcur, &mtr,
				  dd_columns_name.c_str(), &dd_columns);

	while (rec) {
		table_id_t	table_id;
		ulint*		pos;
		ulint*		base_pos;
		ulint		n_row;

		/* populate a dict_col_t structure with information from
		a row */
		ret = dd_process_dd_virtual_columns_rec(
			heap, rec, &table_id, &pos,
			&base_pos, &n_row, dd_columns, &mtr);

		mutex_exit(&dict_sys->mutex);

		if (ret) {
			for (ulint i = 0; i < n_row; i++) {
				i_s_dict_fill_innodb_virtual(thd, table_id,
							     *(pos++),
							     *(base_pos++),
							     tables->table);
			}
		}

		mem_heap_empty(heap);

		/* Get the next record */
		mutex_enter(&dict_sys->mutex);
		mtr_start(&mtr);
		rec = dd_getnext_system_rec(&pcur, &mtr);
	}

	mtr_commit(&mtr);
	dd_table_close(dd_columns, thd, &mdl, true);
	mutex_exit(&dict_sys->mutex);
	mem_heap_free(heap);

	DBUG_RETURN(0);
}

/** Bind the dynamic table INFORMATION_SCHEMA.innodb_virtual
param[in,out]	p	table schema object
@return 0 on success */
static
int
innodb_virtual_init(
	void*	p)
{
	ST_SCHEMA_TABLE*	schema;

	DBUG_ENTER("innodb_virtual_init");

	schema = (ST_SCHEMA_TABLE*) p;

	schema->fields_info = innodb_virtual_fields_info;
	schema->fill_table = i_s_innodb_virtual_fill_table;

	DBUG_RETURN(0);
}

struct st_mysql_plugin	i_s_innodb_virtual =
{
	/* the plugin type (a MYSQL_XXX_PLUGIN value) */
	/* int */
	STRUCT_FLD(type, MYSQL_INFORMATION_SCHEMA_PLUGIN),

	/* pointer to type-specific plugin descriptor */
	/* void* */
	STRUCT_FLD(info, &i_s_info),

	/* plugin name */
	/* const char* */
	STRUCT_FLD(name, "INNODB_VIRTUAL"),

	/* plugin author (for SHOW PLUGINS) */
	/* const char* */
	STRUCT_FLD(author, plugin_author),

	/* general descriptive text (for SHOW PLUGINS) */
	/* const char* */
	STRUCT_FLD(descr, "InnoDB INNODB_VIRTUAL"),

	/* the plugin license (PLUGIN_LICENSE_XXX) */
	/* int */
	STRUCT_FLD(license, PLUGIN_LICENSE_GPL),

	/* the function to invoke when plugin is loaded */
	/* int (*)(void*); */
	STRUCT_FLD(init, innodb_virtual_init),

	/* the function to invoke when plugin is un installed */
	/* int (*)(void*); */
	NULL,

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

/**  INNODB_TABLESPACES    ********************************************/
/* Fields of the dynamic table INFORMATION_SCHEMA.INNODB_TABLESPACES */
static ST_FIELD_INFO	innodb_tablespaces_fields_info[] =
{
#define INNODB_TABLESPACES_SPACE		0
	{STRUCT_FLD(field_name,		"SPACE"),
	 STRUCT_FLD(field_length,	MY_INT32_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define INNODB_TABLESPACES_NAME		1
	{STRUCT_FLD(field_name,		"NAME"),
	 STRUCT_FLD(field_length,	MAX_FULL_NAME_LEN + 1),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_STRING),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	0),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define INNODB_TABLESPACES_FLAGS		2
	{STRUCT_FLD(field_name,		"FLAG"),
	 STRUCT_FLD(field_length,	MY_INT32_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define INNODB_TABLESPACES_ROW_FORMAT	3
	{STRUCT_FLD(field_name,		"ROW_FORMAT"),
	 STRUCT_FLD(field_length,	22),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_STRING),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_MAYBE_NULL),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define INNODB_TABLESPACES_PAGE_SIZE	4
	{STRUCT_FLD(field_name,		"PAGE_SIZE"),
	 STRUCT_FLD(field_length,	MY_INT32_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define INNODB_TABLESPACES_ZIP_PAGE_SIZE	5
	{STRUCT_FLD(field_name,		"ZIP_PAGE_SIZE"),
	 STRUCT_FLD(field_length,	MY_INT32_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define INNODB_TABLESPACES_SPACE_TYPE	6
	{STRUCT_FLD(field_name,		"SPACE_TYPE"),
	 STRUCT_FLD(field_length,	10),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_STRING),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_MAYBE_NULL),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define INNODB_TABLESPACES_FS_BLOCK_SIZE	7
	{STRUCT_FLD(field_name,		"FS_BLOCK_SIZE"),
	 STRUCT_FLD(field_length,	MY_INT32_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define INNODB_TABLESPACES_FILE_SIZE	8
	{STRUCT_FLD(field_name,		"FILE_SIZE"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define INNODB_TABLESPACES_ALLOC_SIZE	9
	{STRUCT_FLD(field_name,		"ALLOCATED_SIZE"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define INNODB_TABLESPACES_SERVER_VESION 10
	{STRUCT_FLD(field_name,		"SERVER_VERSION"),
	 STRUCT_FLD(field_length,	10),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_STRING),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_MAYBE_NULL),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define INNODB_TABLESPACES_SPACE_VESION 11
	{STRUCT_FLD(field_name,		"SPACE_VERSION"),
	 STRUCT_FLD(field_length,	MY_INT32_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	END_OF_ST_FIELD_INFO

};

/** Function to fill INFORMATION_SCHEMA.INNODB_TABLESPACES with information
collected by scanning INNODB_TABLESPACESS table.
@param[in]	thd		thread
@param[in]	space		space ID
@param[in]	name		tablespace name
@param[in]	flags		tablespace flags
@param[in]	server_version	server version
@param[in]	space_version	tablespace version
@param[in,out]	table_to_fill	fill this table
@return 0 on success */
static
int
i_s_dict_fill_innodb_tablespaces(
	THD*		thd,
	space_id_t	space,
	const char*	name,
	ulint		flags,
	uint32		server_version,
	uint32		space_version,
	TABLE*		table_to_fill)
{

	Field**		fields;
	ulint		atomic_blobs = FSP_FLAGS_HAS_ATOMIC_BLOBS(flags);
	bool		is_compressed = FSP_FLAGS_GET_ZIP_SSIZE(flags);
	const char*	row_format;
	const page_size_t	page_size(flags);
	const char*	space_type;
	ulint		major_version = server_version / 10000;
	ulint		minor_version = (server_version
					 - (major_version *10000)) /100;
	ulint		patch_version = server_version - (major_version * 10000)
					- (minor_version * 100);
	char            version_str[NAME_LEN];

	DBUG_ENTER("i_s_dict_fill_innodb_tablespaces");

	snprintf(version_str, NAME_LEN, "%ld.%ld.%ld", major_version,
		 minor_version, patch_version);

	if (fsp_is_system_or_temp_tablespace(space)) {
		row_format = "Compact or Redundant";
	} else if (fsp_is_shared_tablespace(flags) && !is_compressed) {
		row_format = "Any";
	} else if (is_compressed) {
		row_format = "Compressed";
	} else if (atomic_blobs) {
		row_format = "Dynamic";
	} else {
		row_format = "Compact or Redundant";
	}

	if (fsp_is_system_or_temp_tablespace(space)) {
		space_type = "System";
	} else if (fsp_is_shared_tablespace(flags)) {
		space_type = "General";
	} else  {
		space_type = "Single";
	}

	fields = table_to_fill->field;

	OK(fields[INNODB_TABLESPACES_SPACE]->store(space, true));

	OK(field_store_string(fields[INNODB_TABLESPACES_NAME], name));

	OK(fields[INNODB_TABLESPACES_FLAGS]->store(flags, true));

	OK(field_store_string(fields[INNODB_TABLESPACES_ROW_FORMAT],
			      row_format));

	OK(fields[INNODB_TABLESPACES_PAGE_SIZE]->store(
			univ_page_size.physical(), true));

	OK(fields[INNODB_TABLESPACES_ZIP_PAGE_SIZE]->store(
				page_size.is_compressed()
				? page_size.physical()
				: 0, true));

	OK(field_store_string(fields[INNODB_TABLESPACES_SPACE_TYPE],
			      space_type));

	OK(field_store_string(fields[INNODB_TABLESPACES_SERVER_VESION],
			      version_str));

	OK(fields[INNODB_TABLESPACES_SPACE_VESION]->store(space_version, true));

	char*	filepath = NULL;
	if (FSP_FLAGS_HAS_DATA_DIR(flags)
	    || FSP_FLAGS_GET_SHARED(flags)) {
		mutex_enter(&dict_sys->mutex);
		filepath = fil_space_get_first_path(space);
		mutex_exit(&dict_sys->mutex);
	}

	if (filepath == NULL) {
		if (strstr(name, dict_sys_t::s_file_per_table_name) != 0) {
			mutex_enter(&dict_sys->mutex);
			filepath = fil_space_get_first_path(space);
			mutex_exit(&dict_sys->mutex);
		} else {
			filepath = Fil_path::make_ibd_from_table_name(name);
		}
	}

	os_file_stat_t	stat;
	os_file_size_t	file;

	memset(&file, 0xff, sizeof(file));
	memset(&stat, 0x0, sizeof(stat));

	if (filepath != NULL) {

		file = os_file_get_size(filepath);

		/* Get the file system (or Volume) block size. */
		dberr_t	err = os_file_get_status(filepath, &stat, false, false);

		switch(err) {
		case DB_FAIL:
			ib::warn()
				<< "File '" << filepath << "', failed to get "
				<< "stats";
			break;

		case DB_SUCCESS:
		case DB_NOT_FOUND:
			break;

		default:
			ib::error()
				<< "File '" << filepath << "' "
				<< ut_strerr(err);
			break;
		}

		ut_free(filepath);
	}

	if (file.m_total_size == static_cast<os_offset_t>(~0)) {
		stat.block_size = 0;
		file.m_total_size = 0;
		file.m_alloc_size = 0;
	}

	OK(fields[INNODB_TABLESPACES_FS_BLOCK_SIZE]->store(stat.block_size, true));

	OK(fields[INNODB_TABLESPACES_FILE_SIZE]->store(file.m_total_size, true));

	OK(fields[INNODB_TABLESPACES_ALLOC_SIZE]->store(file.m_alloc_size, true));

	OK(schema_table_store_record(thd, table_to_fill));

	DBUG_RETURN(0);
}

/** Function to populate INFORMATION_SCHEMA.INNODB_TABLESPACES table.
Loop through each record in INNODB_TABLESPACES, and extract the column
information and fill the INFORMATION_SCHEMA.INNODB_TABLESPACES table.
@param[in]	thd		thread
@param[in,out]	tables		tables to fill
@return 0 on success */
static
int
i_s_innodb_tablespaces_fill_table(
	THD*		thd,
	TABLE_LIST*	tables,
	Item*		)
{
	btr_pcur_t	pcur;
	const rec_t*	rec;
	mem_heap_t*	heap;
	mtr_t		mtr;
	dict_table_t*	dd_spaces;
	MDL_ticket*	mdl = nullptr;
	bool		ret;

	DBUG_ENTER("i_s_innodb_tablespaces_fill_table");

	/* deny access to user without PROCESS_ACL privilege */
	if (check_global_access(thd, PROCESS_ACL)) {
		DBUG_RETURN(0);
	}

	heap = mem_heap_create(1000);
	mutex_enter(&dict_sys->mutex);
	mtr_start(&mtr);

	for (rec = dd_startscan_system(thd, &mdl, &pcur, &mtr,
				       dd_tablespaces_name.c_str(), &dd_spaces);
	     rec != NULL;
	     rec = dd_getnext_system_rec(&pcur, &mtr)) {

		space_id_t	space;
		char*		name;
		uint		flags;
		uint32		server_version;
		uint32		space_version;

		/* Extract necessary information from a INNODB_TABLESPACES
		row */
		ret = dd_process_dd_tablespaces_rec(
			heap, rec, &space, &name, &flags, &server_version,
			&space_version, dd_spaces);

		mtr_commit(&mtr);
		mutex_exit(&dict_sys->mutex);

		if (ret && space != 0) {
			i_s_dict_fill_innodb_tablespaces(
				thd, space, name, flags, server_version,
				space_version, tables->table);
		}

		mem_heap_empty(heap);

		/* Get the next record */
		mutex_enter(&dict_sys->mutex);
		mtr_start(&mtr);
	}

	mtr_commit(&mtr);
	dd_table_close(dd_spaces, thd, &mdl, true);
	mutex_exit(&dict_sys->mutex);
	mem_heap_free(heap);

	DBUG_RETURN(0);
}
/** Bind the dynamic table INFORMATION_SCHEMA.INNODB_TABLESPACES
@param[in,out]	p	table schema object
@return 0 on success */
static
int
innodb_tablespaces_init(
	void*	p)
{
	ST_SCHEMA_TABLE*	schema;

	DBUG_ENTER("innodb_tablespaces_init");

	schema = (ST_SCHEMA_TABLE*) p;

	schema->fields_info = innodb_tablespaces_fields_info;
	schema->fill_table = i_s_innodb_tablespaces_fill_table;

	DBUG_RETURN(0);
}

struct st_mysql_plugin	i_s_innodb_tablespaces =
{
	/* the plugin type (a MYSQL_XXX_PLUGIN value) */
	/* int */
	STRUCT_FLD(type, MYSQL_INFORMATION_SCHEMA_PLUGIN),

	/* pointer to type-specific plugin descriptor */
	/* void* */
	STRUCT_FLD(info, &i_s_info),

	/* plugin name */
	/* const char* */
	STRUCT_FLD(name, "INNODB_TABLESPACES"),

	/* plugin author (for SHOW PLUGINS) */
	/* const char* */
	STRUCT_FLD(author, plugin_author),

	/* general descriptive text (for SHOW PLUGINS) */
	/* const char* */
	STRUCT_FLD(descr, "InnoDB INNODB_TABLESPACES"),

	/* the plugin license (PLUGIN_LICENSE_XXX) */
	/* int */
	STRUCT_FLD(license, PLUGIN_LICENSE_GPL),

	/* the function to invoke when plugin is loaded */
	/* int (*)(void*); */
	STRUCT_FLD(init, innodb_tablespaces_init),

	/* the function to invoke when plugin is un installed */
	/* int (*)(void*); */
	NULL,

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

/** INFORMATION_SCHEMA.INNODB_CACHED_INDEXES */

/* Fields of the dynamic table INFORMATION_SCHEMA.INNODB_CACHED_INDEXES */
static ST_FIELD_INFO	innodb_cached_indexes_fields_info[] =
{
#define CACHED_INDEXES_SPACE_ID		0
	{STRUCT_FLD(field_name,		"SPACE_ID"),
	 STRUCT_FLD(field_length,	MY_INT32_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define CACHED_INDEXES_INDEX_ID		1
	{STRUCT_FLD(field_name,		"INDEX_ID"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

#define CACHED_INDEXES_N_CACHED_PAGES	2
	{STRUCT_FLD(field_name,		"N_CACHED_PAGES"),
	 STRUCT_FLD(field_length,	MY_INT64_NUM_DECIMAL_DIGITS),
	 STRUCT_FLD(field_type,		MYSQL_TYPE_LONGLONG),
	 STRUCT_FLD(value,		0),
	 STRUCT_FLD(field_flags,	MY_I_S_UNSIGNED),
	 STRUCT_FLD(old_name,		""),
	 STRUCT_FLD(open_method,	SKIP_OPEN_TABLE)},

	END_OF_ST_FIELD_INFO
};

/** Populate INFORMATION_SCHEMA.INNODB_CACHED_INDEXES.
@param[in]	thd		user thread
@param[in]	space_id	space id
@param[in]	index_id	index id
@param[in,out]	table_to_fill	fill this table
@return 0 on success */
static
int
i_s_fill_innodb_cached_indexes_row(
	THD*		thd,
	space_id_t	space_id,
	ulint		index_id,
	TABLE*		table_to_fill)
{
	DBUG_ENTER("i_s_fill_innodb_cached_indexes_row");

	const index_id_t	idx_id(space_id, index_id);
	const uint64_t		n = buf_stat_per_index->get(idx_id);

	if (n == 0) {
		DBUG_RETURN(0);
	}

	Field**	fields = table_to_fill->field;

	OK(fields[CACHED_INDEXES_SPACE_ID]->store(space_id, true));

	OK(fields[CACHED_INDEXES_INDEX_ID]->store(index_id, true));

	OK(fields[CACHED_INDEXES_N_CACHED_PAGES]->store(n, true));

	OK(schema_table_store_record(thd, table_to_fill));

	DBUG_RETURN(0);
}

/** Go through each record in INNODB_INDEXES, and fill
INFORMATION_SCHEMA.INNODB_CACHED_INDEXES.
@param[in]	thd	thread
@param[in,out]	tables	tables to fill
@return 0 on success */
static
int
i_s_innodb_cached_indexes_fill_table(
	THD*		thd,
	TABLE_LIST*	tables,
	Item*		/* not used */)
{
	MDL_ticket*		mdl = nullptr;
	dict_table_t*		dd_indexes;
	space_id_t		space_id;
	space_index_t		index_id;

	DBUG_ENTER("i_s_innodb_cached_indexes_fill_table");

	/* deny access to user without PROCESS_ACL privilege */
	if (check_global_access(thd, PROCESS_ACL)) {
		DBUG_RETURN(0);
	}

	mem_heap_t*	heap = mem_heap_create(1000);

	mutex_enter(&dict_sys->mutex);

	mtr_t		mtr;

	mtr_start(&mtr);

	/* Start the scan of INNODB_INDEXES. */
	btr_pcur_t	pcur;
	const rec_t* rec = dd_startscan_system(thd, &mdl, &pcur, &mtr,
					       dd_indexes_name.c_str(),
					       &dd_indexes);

	/* Process each record in the table. */
	while (rec != NULL) {
		/* Populate a dict_index_t structure with an information
		from a INNODB_INDEXES row. */
		bool ret = dd_process_dd_indexes_rec_simple(
			heap, rec, &index_id, &space_id, dd_indexes);

		mtr_commit(&mtr);

		mutex_exit(&dict_sys->mutex);

		if (ret) {
			i_s_fill_innodb_cached_indexes_row(
				thd, space_id, index_id, tables->table);
		}

		mem_heap_empty(heap);

		/* Get the next record. */
		mutex_enter(&dict_sys->mutex);

		mtr_start(&mtr);

		rec = dd_getnext_system_rec(&pcur, &mtr);
	}

	mtr_commit(&mtr);

	dd_table_close(dd_indexes, thd, &mdl, true);

	mutex_exit(&dict_sys->mutex);

	mem_heap_free(heap);

	DBUG_RETURN(0);
}

/** Bind the dynamic table INFORMATION_SCHEMA.INNODB_CACHED_INDEXES.
@param[in,out]	p	table schema object
@return 0 on success */
static
int
innodb_cached_indexes_init(
	void*	p)
{
	ST_SCHEMA_TABLE*	schema;

	DBUG_ENTER("innodb_cached_indexes_init");

	schema = static_cast<ST_SCHEMA_TABLE*>(p);

	schema->fields_info = innodb_cached_indexes_fields_info;
	schema->fill_table = i_s_innodb_cached_indexes_fill_table;

	DBUG_RETURN(0);
}

struct st_mysql_plugin	i_s_innodb_cached_indexes =
{
	/* the plugin type (a MYSQL_XXX_PLUGIN value) */
	/* int */
	STRUCT_FLD(type, MYSQL_INFORMATION_SCHEMA_PLUGIN),

	/* pointer to type-specific plugin descriptor */
	/* void* */
	STRUCT_FLD(info, &i_s_info),

	/* plugin name */
	/* const char* */
	STRUCT_FLD(name, "INNODB_CACHED_INDEXES"),

	/* plugin author (for SHOW PLUGINS) */
	/* const char* */
	STRUCT_FLD(author, plugin_author),

	/* general descriptive text (for SHOW PLUGINS) */
	/* const char* */
	STRUCT_FLD(descr, "InnoDB cached indexes"),

	/* the plugin license (PLUGIN_LICENSE_XXX) */
	/* int */
	STRUCT_FLD(license, PLUGIN_LICENSE_GPL),

	/* the function to invoke when plugin is loaded */
	/* int (*)(void*); */
	STRUCT_FLD(init, innodb_cached_indexes_init),

	/* the function to invoke when plugin is un installed */
	/* int (*)(void*); */
	NULL,

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

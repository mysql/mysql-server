/*****************************************************************************

Copyright (c) 2000, 2013, Oracle and/or its affiliates. All Rights Reserved.

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

/**************************************************//**
@file row/row0mysql.cc
Interface between Innobase row operations and MySQL.
Contains also create table and other data dictionary operations.

Created 9/17/2000 Heikki Tuuri
*******************************************************/

#include "row0mysql.h"

#ifdef UNIV_NONINL
#include "row0mysql.ic"
#endif

#include <debug_sync.h>
#include <my_dbug.h>

#include "row0ins.h"
#include "row0merge.h"
#include "row0sel.h"
#include "row0upd.h"
#include "row0row.h"
#include "que0que.h"
#include "pars0pars.h"
#include "dict0dict.h"
#include "dict0crea.h"
#include "dict0load.h"
#include "dict0boot.h"
#include "dict0stats.h"
#include "dict0stats_bg.h"
#include "trx0roll.h"
#include "trx0purge.h"
#include "trx0rec.h"
#include "trx0undo.h"
#include "lock0lock.h"
#include "rem0cmp.h"
#include "log0log.h"
#include "btr0sea.h"
#include "fil0fil.h"
#include "ibuf0ibuf.h"
#include "fts0fts.h"
#include "fts0types.h"
#include "srv0start.h"
#include "row0import.h"
#include "m_string.h"
#include "my_sys.h"
#include "ha_prototypes.h"

/** Provide optional 4.x backwards compatibility for 5.0 and above */
UNIV_INTERN ibool	row_rollback_on_timeout	= FALSE;

/** Chain node of the list of tables to drop in the background. */
struct row_mysql_drop_t{
	char*				table_name;	/*!< table name */
	UT_LIST_NODE_T(row_mysql_drop_t)row_mysql_drop_list;
							/*!< list chain node */
};

#ifdef UNIV_PFS_MUTEX
/* Key to register drop list mutex with performance schema */
UNIV_INTERN mysql_pfs_key_t	row_drop_list_mutex_key;
#endif /* UNIV_PFS_MUTEX */

/** @brief List of tables we should drop in background.

ALTER TABLE in MySQL requires that the table handler can drop the
table in background when there are no queries to it any
more.  Protected by row_drop_list_mutex. */
static UT_LIST_BASE_NODE_T(row_mysql_drop_t)	row_mysql_drop_list;

/** Mutex protecting the background table drop list. */
static ib_mutex_t row_drop_list_mutex;

/** Flag: has row_mysql_drop_list been initialized? */
static ibool	row_mysql_drop_list_inited	= FALSE;

/** Magic table names for invoking various monitor threads */
/* @{ */
static const char S_innodb_monitor[] = "innodb_monitor";
static const char S_innodb_lock_monitor[] = "innodb_lock_monitor";
static const char S_innodb_tablespace_monitor[] = "innodb_tablespace_monitor";
static const char S_innodb_table_monitor[] = "innodb_table_monitor";
#ifdef UNIV_MEM_DEBUG
static const char S_innodb_mem_validate[] = "innodb_mem_validate";
#endif /* UNIV_MEM_DEBUG */
/* @} */

/** Evaluates to true if str1 equals str2_onstack, used for comparing
the magic table names.
@param str1		in: string to compare
@param str1_len 	in: length of str1, in bytes, including terminating NUL
@param str2_onstack	in: char[] array containing a NUL terminated string
@return			TRUE if str1 equals str2_onstack */
#define STR_EQ(str1, str1_len, str2_onstack) \
	((str1_len) == sizeof(str2_onstack) \
	 && memcmp(str1, str2_onstack, sizeof(str2_onstack)) == 0)

/*******************************************************************//**
Determine if the given name is a name reserved for MySQL system tables.
@return	TRUE if name is a MySQL system table name */
static
ibool
row_mysql_is_system_table(
/*======================*/
	const char*	name)
{
	if (strncmp(name, "mysql/", 6) != 0) {

		return(FALSE);
	}

	return(0 == strcmp(name + 6, "host")
	       || 0 == strcmp(name + 6, "user")
	       || 0 == strcmp(name + 6, "db"));
}

/*********************************************************************//**
If a table is not yet in the drop list, adds the table to the list of tables
which the master thread drops in background. We need this on Unix because in
ALTER TABLE MySQL may call drop table even if the table has running queries on
it. Also, if there are running foreign key checks on the table, we drop the
table lazily.
@return	TRUE if the table was not yet in the drop list, and was added there */
static
ibool
row_add_table_to_background_drop_list(
/*==================================*/
	const char*	name);	/*!< in: table name */

/*******************************************************************//**
Delays an INSERT, DELETE or UPDATE operation if the purge is lagging. */
static
void
row_mysql_delay_if_needed(void)
/*===========================*/
{
	if (srv_dml_needed_delay) {
		os_thread_sleep(srv_dml_needed_delay);
	}
}

/*******************************************************************//**
Frees the blob heap in prebuilt when no longer needed. */
UNIV_INTERN
void
row_mysql_prebuilt_free_blob_heap(
/*==============================*/
	row_prebuilt_t*	prebuilt)	/*!< in: prebuilt struct of a
					ha_innobase:: table handle */
{
	mem_heap_free(prebuilt->blob_heap);
	prebuilt->blob_heap = NULL;
}

/*******************************************************************//**
Stores a >= 5.0.3 format true VARCHAR length to dest, in the MySQL row
format.
@return pointer to the data, we skip the 1 or 2 bytes at the start
that are used to store the len */
UNIV_INTERN
byte*
row_mysql_store_true_var_len(
/*=========================*/
	byte*	dest,	/*!< in: where to store */
	ulint	len,	/*!< in: length, must fit in two bytes */
	ulint	lenlen)	/*!< in: storage length of len: either 1 or 2 bytes */
{
	if (lenlen == 2) {
		ut_a(len < 256 * 256);

		mach_write_to_2_little_endian(dest, len);

		return(dest + 2);
	}

	ut_a(lenlen == 1);
	ut_a(len < 256);

	mach_write_to_1(dest, len);

	return(dest + 1);
}

/*******************************************************************//**
Reads a >= 5.0.3 format true VARCHAR length, in the MySQL row format, and
returns a pointer to the data.
@return pointer to the data, we skip the 1 or 2 bytes at the start
that are used to store the len */
UNIV_INTERN
const byte*
row_mysql_read_true_varchar(
/*========================*/
	ulint*		len,	/*!< out: variable-length field length */
	const byte*	field,	/*!< in: field in the MySQL format */
	ulint		lenlen)	/*!< in: storage length of len: either 1
				or 2 bytes */
{
	if (lenlen == 2) {
		*len = mach_read_from_2_little_endian(field);

		return(field + 2);
	}

	ut_a(lenlen == 1);

	*len = mach_read_from_1(field);

	return(field + 1);
}

/*******************************************************************//**
Stores a reference to a BLOB in the MySQL format. */
UNIV_INTERN
void
row_mysql_store_blob_ref(
/*=====================*/
	byte*		dest,	/*!< in: where to store */
	ulint		col_len,/*!< in: dest buffer size: determines into
				how many bytes the BLOB length is stored,
				the space for the length may vary from 1
				to 4 bytes */
	const void*	data,	/*!< in: BLOB data; if the value to store
				is SQL NULL this should be NULL pointer */
	ulint		len)	/*!< in: BLOB length; if the value to store
				is SQL NULL this should be 0; remember
				also to set the NULL bit in the MySQL record
				header! */
{
	/* MySQL might assume the field is set to zero except the length and
	the pointer fields */

	memset(dest, '\0', col_len);

	/* In dest there are 1 - 4 bytes reserved for the BLOB length,
	and after that 8 bytes reserved for the pointer to the data.
	In 32-bit architectures we only use the first 4 bytes of the pointer
	slot. */

	ut_a(col_len - 8 > 1 || len < 256);
	ut_a(col_len - 8 > 2 || len < 256 * 256);
	ut_a(col_len - 8 > 3 || len < 256 * 256 * 256);

	mach_write_to_n_little_endian(dest, col_len - 8, len);

	memcpy(dest + col_len - 8, &data, sizeof data);
}

/*******************************************************************//**
Reads a reference to a BLOB in the MySQL format.
@return	pointer to BLOB data */
UNIV_INTERN
const byte*
row_mysql_read_blob_ref(
/*====================*/
	ulint*		len,		/*!< out: BLOB length */
	const byte*	ref,		/*!< in: BLOB reference in the
					MySQL format */
	ulint		col_len)	/*!< in: BLOB reference length
					(not BLOB length) */
{
	byte*	data;

	*len = mach_read_from_n_little_endian(ref, col_len - 8);

	memcpy(&data, ref + col_len - 8, sizeof data);

	return(data);
}

/**************************************************************//**
Pad a column with spaces. */
UNIV_INTERN
void
row_mysql_pad_col(
/*==============*/
	ulint	mbminlen,	/*!< in: minimum size of a character,
				in bytes */
	byte*	pad,		/*!< out: padded buffer */
	ulint	len)		/*!< in: number of bytes to pad */
{
	const byte*	pad_end;

	switch (UNIV_EXPECT(mbminlen, 1)) {
	default:
		ut_error;
	case 1:
		/* space=0x20 */
		memset(pad, 0x20, len);
		break;
	case 2:
		/* space=0x0020 */
		pad_end = pad + len;
		ut_a(!(len % 2));
		while (pad < pad_end) {
			*pad++ = 0x00;
			*pad++ = 0x20;
		};
		break;
	case 4:
		/* space=0x00000020 */
		pad_end = pad + len;
		ut_a(!(len % 4));
		while (pad < pad_end) {
			*pad++ = 0x00;
			*pad++ = 0x00;
			*pad++ = 0x00;
			*pad++ = 0x20;
		}
		break;
	}
}

/**************************************************************//**
Stores a non-SQL-NULL field given in the MySQL format in the InnoDB format.
The counterpart of this function is row_sel_field_store_in_mysql_format() in
row0sel.cc.
@return	up to which byte we used buf in the conversion */
UNIV_INTERN
byte*
row_mysql_store_col_in_innobase_format(
/*===================================*/
	dfield_t*	dfield,		/*!< in/out: dfield where dtype
					information must be already set when
					this function is called! */
	byte*		buf,		/*!< in/out: buffer for a converted
					integer value; this must be at least
					col_len long then! NOTE that dfield
					may also get a pointer to 'buf',
					therefore do not discard this as long
					as dfield is used! */
	ibool		row_format_col,	/*!< TRUE if the mysql_data is from
					a MySQL row, FALSE if from a MySQL
					key value;
					in MySQL, a true VARCHAR storage
					format differs in a row and in a
					key value: in a key value the length
					is always stored in 2 bytes! */
	const byte*	mysql_data,	/*!< in: MySQL column value, not
					SQL NULL; NOTE that dfield may also
					get a pointer to mysql_data,
					therefore do not discard this as long
					as dfield is used! */
	ulint		col_len,	/*!< in: MySQL column length; NOTE that
					this is the storage length of the
					column in the MySQL format row, not
					necessarily the length of the actual
					payload data; if the column is a true
					VARCHAR then this is irrelevant */
	ulint		comp)		/*!< in: nonzero=compact format */
{
	const byte*	ptr	= mysql_data;
	const dtype_t*	dtype;
	ulint		type;
	ulint		lenlen;

	dtype = dfield_get_type(dfield);

	type = dtype->mtype;

	if (type == DATA_INT) {
		/* Store integer data in Innobase in a big-endian format,
		sign bit negated if the data is a signed integer. In MySQL,
		integers are stored in a little-endian format. */

		byte*	p = buf + col_len;

		for (;;) {
			p--;
			*p = *mysql_data;
			if (p == buf) {
				break;
			}
			mysql_data++;
		}

		if (!(dtype->prtype & DATA_UNSIGNED)) {

			*buf ^= 128;
		}

		ptr = buf;
		buf += col_len;
	} else if ((type == DATA_VARCHAR
		    || type == DATA_VARMYSQL
		    || type == DATA_BINARY)) {

		if (dtype_get_mysql_type(dtype) == DATA_MYSQL_TRUE_VARCHAR) {
			/* The length of the actual data is stored to 1 or 2
			bytes at the start of the field */

			if (row_format_col) {
				if (dtype->prtype & DATA_LONG_TRUE_VARCHAR) {
					lenlen = 2;
				} else {
					lenlen = 1;
				}
			} else {
				/* In a MySQL key value, lenlen is always 2 */
				lenlen = 2;
			}

			ptr = row_mysql_read_true_varchar(&col_len, mysql_data,
							  lenlen);
		} else {
			/* Remove trailing spaces from old style VARCHAR
			columns. */

			/* Handle Unicode strings differently. */
			ulint	mbminlen	= dtype_get_mbminlen(dtype);

			ptr = mysql_data;

			switch (mbminlen) {
			default:
				ut_error;
			case 4:
				/* space=0x00000020 */
				/* Trim "half-chars", just in case. */
				col_len &= ~3;

				while (col_len >= 4
				       && ptr[col_len - 4] == 0x00
				       && ptr[col_len - 3] == 0x00
				       && ptr[col_len - 2] == 0x00
				       && ptr[col_len - 1] == 0x20) {
					col_len -= 4;
				}
				break;
			case 2:
				/* space=0x0020 */
				/* Trim "half-chars", just in case. */
				col_len &= ~1;

				while (col_len >= 2 && ptr[col_len - 2] == 0x00
				       && ptr[col_len - 1] == 0x20) {
					col_len -= 2;
				}
				break;
			case 1:
				/* space=0x20 */
				while (col_len > 0
				       && ptr[col_len - 1] == 0x20) {
					col_len--;
				}
			}
		}
	} else if (comp && type == DATA_MYSQL
		   && dtype_get_mbminlen(dtype) == 1
		   && dtype_get_mbmaxlen(dtype) > 1) {
		/* In some cases we strip trailing spaces from UTF-8 and other
		multibyte charsets, from FIXED-length CHAR columns, to save
		space. UTF-8 would otherwise normally use 3 * the string length
		bytes to store an ASCII string! */

		/* We assume that this CHAR field is encoded in a
		variable-length character set where spaces have
		1:1 correspondence to 0x20 bytes, such as UTF-8.

		Consider a CHAR(n) field, a field of n characters.
		It will contain between n * mbminlen and n * mbmaxlen bytes.
		We will try to truncate it to n bytes by stripping
		space padding.	If the field contains single-byte
		characters only, it will be truncated to n characters.
		Consider a CHAR(5) field containing the string ".a   "
		where "." denotes a 3-byte character represented by
		the bytes "$%&".  After our stripping, the string will
		be stored as "$%&a " (5 bytes).	 The string ".abc "
		will be stored as "$%&abc" (6 bytes).

		The space padding will be restored in row0sel.cc, function
		row_sel_field_store_in_mysql_format(). */

		ulint		n_chars;

		ut_a(!(dtype_get_len(dtype) % dtype_get_mbmaxlen(dtype)));

		n_chars = dtype_get_len(dtype) / dtype_get_mbmaxlen(dtype);

		/* Strip space padding. */
		while (col_len > n_chars && ptr[col_len - 1] == 0x20) {
			col_len--;
		}
	} else if (type == DATA_BLOB && row_format_col) {

		ptr = row_mysql_read_blob_ref(&col_len, mysql_data, col_len);
	}

	dfield_set_data(dfield, ptr, col_len);

	return(buf);
}

/**************************************************************//**
Convert a row in the MySQL format to a row in the Innobase format. Note that
the function to convert a MySQL format key value to an InnoDB dtuple is
row_sel_convert_mysql_key_to_innobase() in row0sel.cc. */
static
void
row_mysql_convert_row_to_innobase(
/*==============================*/
	dtuple_t*	row,		/*!< in/out: Innobase row where the
					field type information is already
					copied there! */
	row_prebuilt_t*	prebuilt,	/*!< in: prebuilt struct where template
					must be of type ROW_MYSQL_WHOLE_ROW */
	byte*		mysql_rec)	/*!< in: row in the MySQL format;
					NOTE: do not discard as long as
					row is used, as row may contain
					pointers to this record! */
{
	const mysql_row_templ_t*templ;
	dfield_t*		dfield;
	ulint			i;

	ut_ad(prebuilt->template_type == ROW_MYSQL_WHOLE_ROW);
	ut_ad(prebuilt->mysql_template);

	for (i = 0; i < prebuilt->n_template; i++) {

		templ = prebuilt->mysql_template + i;
		dfield = dtuple_get_nth_field(row, i);

		if (templ->mysql_null_bit_mask != 0) {
			/* Column may be SQL NULL */

			if (mysql_rec[templ->mysql_null_byte_offset]
			    & (byte) (templ->mysql_null_bit_mask)) {

				/* It is SQL NULL */

				dfield_set_null(dfield);

				goto next_column;
			}
		}

		row_mysql_store_col_in_innobase_format(
			dfield,
			prebuilt->ins_upd_rec_buff + templ->mysql_col_offset,
			TRUE, /* MySQL row format data */
			mysql_rec + templ->mysql_col_offset,
			templ->mysql_col_len,
			dict_table_is_comp(prebuilt->table));
next_column:
		;
	}

	/* If there is a FTS doc id column and it is not user supplied (
	generated by server) then assign it a new doc id. */
	if (prebuilt->table->fts) {

		ut_a(prebuilt->table->fts->doc_col != ULINT_UNDEFINED);

		fts_create_doc_id(prebuilt->table, row, prebuilt->heap);
	}
}

/****************************************************************//**
Handles user errors and lock waits detected by the database engine.
@return true if it was a lock wait and we should continue running the
query thread and in that case the thr is ALREADY in the running state. */
UNIV_INTERN
bool
row_mysql_handle_errors(
/*====================*/
	dberr_t*	new_err,/*!< out: possible new error encountered in
				lock wait, or if no new error, the value
				of trx->error_state at the entry of this
				function */
	trx_t*		trx,	/*!< in: transaction */
	que_thr_t*	thr,	/*!< in: query thread, or NULL */
	trx_savept_t*	savept)	/*!< in: savepoint, or NULL */
{
	dberr_t	err;

handle_new_error:
	err = trx->error_state;

	ut_a(err != DB_SUCCESS);

	trx->error_state = DB_SUCCESS;

	switch (err) {
	case DB_LOCK_WAIT_TIMEOUT:
		if (row_rollback_on_timeout) {
			trx_rollback_to_savepoint(trx, NULL);
			break;
		}
		/* fall through */
	case DB_DUPLICATE_KEY:
	case DB_FOREIGN_DUPLICATE_KEY:
	case DB_TOO_BIG_RECORD:
	case DB_UNDO_RECORD_TOO_BIG:
	case DB_ROW_IS_REFERENCED:
	case DB_NO_REFERENCED_ROW:
	case DB_CANNOT_ADD_CONSTRAINT:
	case DB_TOO_MANY_CONCURRENT_TRXS:
	case DB_OUT_OF_FILE_SPACE:
	case DB_READ_ONLY:
	case DB_FTS_INVALID_DOCID:
	case DB_INTERRUPTED:
	case DB_DICT_CHANGED:
		if (savept) {
			/* Roll back the latest, possibly incomplete insertion
			or update */

			trx_rollback_to_savepoint(trx, savept);
		}
		/* MySQL will roll back the latest SQL statement */
		break;
	case DB_LOCK_WAIT:
		lock_wait_suspend_thread(thr);

		if (trx->error_state != DB_SUCCESS) {
			que_thr_stop_for_mysql(thr);

			goto handle_new_error;
		}

		*new_err = err;

		return(true);

	case DB_DEADLOCK:
	case DB_LOCK_TABLE_FULL:
		/* Roll back the whole transaction; this resolution was added
		to version 3.23.43 */

		trx_rollback_to_savepoint(trx, NULL);
		break;

	case DB_MUST_GET_MORE_FILE_SPACE:
		fputs("InnoDB: The database cannot continue"
		      " operation because of\n"
		      "InnoDB: lack of space. You must add"
		      " a new data file to\n"
		      "InnoDB: my.cnf and restart the database.\n", stderr);

		ut_ad(0);
		exit(1);

	case DB_CORRUPTION:
		fputs("InnoDB: We detected index corruption"
		      " in an InnoDB type table.\n"
		      "InnoDB: You have to dump + drop + reimport"
		      " the table or, in\n"
		      "InnoDB: a case of widespread corruption,"
		      " dump all InnoDB\n"
		      "InnoDB: tables and recreate the"
		      " whole InnoDB tablespace.\n"
		      "InnoDB: If the mysqld server crashes"
		      " after the startup or when\n"
		      "InnoDB: you dump the tables, look at\n"
		      "InnoDB: " REFMAN "forcing-innodb-recovery.html"
		      " for help.\n", stderr);
		break;
	case DB_FOREIGN_EXCEED_MAX_CASCADE:
		fprintf(stderr, "InnoDB: Cannot delete/update rows with"
			" cascading foreign key constraints that exceed max"
			" depth of %lu\n"
			"Please drop excessive foreign constraints"
			" and try again\n", (ulong) DICT_FK_MAX_RECURSIVE_LOAD);
		break;
	default:
		fprintf(stderr, "InnoDB: unknown error code %lu\n",
			(ulong) err);
		ut_error;
	}

	if (trx->error_state != DB_SUCCESS) {
		*new_err = trx->error_state;
	} else {
		*new_err = err;
	}

	trx->error_state = DB_SUCCESS;

	return(false);
}

/********************************************************************//**
Create a prebuilt struct for a MySQL table handle.
@return	own: a prebuilt struct */
UNIV_INTERN
row_prebuilt_t*
row_create_prebuilt(
/*================*/
	dict_table_t*	table,		/*!< in: Innobase table handle */
	ulint		mysql_row_len)	/*!< in: length in bytes of a row in
					the MySQL format */
{
	row_prebuilt_t*	prebuilt;
	mem_heap_t*	heap;
	dict_index_t*	clust_index;
	dtuple_t*	ref;
	ulint		ref_len;
	ulint		search_tuple_n_fields;

	search_tuple_n_fields = 2 * dict_table_get_n_cols(table);

	clust_index = dict_table_get_first_index(table);

	/* Make sure that search_tuple is long enough for clustered index */
	ut_a(2 * dict_table_get_n_cols(table) >= clust_index->n_fields);

	ref_len = dict_index_get_n_unique(clust_index);

#define PREBUILT_HEAP_INITIAL_SIZE	\
	( \
	sizeof(*prebuilt) \
	/* allocd in this function */ \
	+ DTUPLE_EST_ALLOC(search_tuple_n_fields) \
	+ DTUPLE_EST_ALLOC(ref_len) \
	/* allocd in row_prebuild_sel_graph() */ \
	+ sizeof(sel_node_t) \
	+ sizeof(que_fork_t) \
	+ sizeof(que_thr_t) \
	/* allocd in row_get_prebuilt_update_vector() */ \
	+ sizeof(upd_node_t) \
	+ sizeof(upd_t) \
	+ sizeof(upd_field_t) \
	  * dict_table_get_n_cols(table) \
	+ sizeof(que_fork_t) \
	+ sizeof(que_thr_t) \
	/* allocd in row_get_prebuilt_insert_row() */ \
	+ sizeof(ins_node_t) \
	/* mysql_row_len could be huge and we are not \
	sure if this prebuilt instance is going to be \
	used in inserts */ \
	+ (mysql_row_len < 256 ? mysql_row_len : 0) \
	+ DTUPLE_EST_ALLOC(dict_table_get_n_cols(table)) \
	+ sizeof(que_fork_t) \
	+ sizeof(que_thr_t) \
	)

	/* We allocate enough space for the objects that are likely to
	be created later in order to minimize the number of malloc()
	calls */
	heap = mem_heap_create(PREBUILT_HEAP_INITIAL_SIZE);

	prebuilt = static_cast<row_prebuilt_t*>(
		mem_heap_zalloc(heap, sizeof(*prebuilt)));

	prebuilt->magic_n = ROW_PREBUILT_ALLOCATED;
	prebuilt->magic_n2 = ROW_PREBUILT_ALLOCATED;

	prebuilt->table = table;

	prebuilt->sql_stat_start = TRUE;
	prebuilt->heap = heap;

	btr_pcur_reset(&prebuilt->pcur);
	btr_pcur_reset(&prebuilt->clust_pcur);

	prebuilt->select_lock_type = LOCK_NONE;
	prebuilt->stored_select_lock_type = LOCK_NONE_UNSET;

	prebuilt->search_tuple = dtuple_create(heap, search_tuple_n_fields);

	ref = dtuple_create(heap, ref_len);

	dict_index_copy_types(ref, clust_index, ref_len);

	prebuilt->clust_ref = ref;

	prebuilt->autoinc_error = DB_SUCCESS;
	prebuilt->autoinc_offset = 0;

	/* Default to 1, we will set the actual value later in
	ha_innobase::get_auto_increment(). */
	prebuilt->autoinc_increment = 1;

	prebuilt->autoinc_last_value = 0;

	/* During UPDATE and DELETE we need the doc id. */
	prebuilt->fts_doc_id = 0;

	prebuilt->mysql_row_len = mysql_row_len;

	return(prebuilt);
}

/********************************************************************//**
Free a prebuilt struct for a MySQL table handle. */
UNIV_INTERN
void
row_prebuilt_free(
/*==============*/
	row_prebuilt_t*	prebuilt,	/*!< in, own: prebuilt struct */
	ibool		dict_locked)	/*!< in: TRUE=data dictionary locked */
{
	ulint	i;

	if (UNIV_UNLIKELY
	    (prebuilt->magic_n != ROW_PREBUILT_ALLOCATED
	     || prebuilt->magic_n2 != ROW_PREBUILT_ALLOCATED)) {

		fprintf(stderr,
			"InnoDB: Error: trying to free a corrupt\n"
			"InnoDB: table handle. Magic n %lu,"
			" magic n2 %lu, table name ",
			(ulong) prebuilt->magic_n,
			(ulong) prebuilt->magic_n2);
		ut_print_name(stderr, NULL, TRUE, prebuilt->table->name);
		putc('\n', stderr);

		mem_analyze_corruption(prebuilt);

		ut_error;
	}

	prebuilt->magic_n = ROW_PREBUILT_FREED;
	prebuilt->magic_n2 = ROW_PREBUILT_FREED;

	btr_pcur_reset(&prebuilt->pcur);
	btr_pcur_reset(&prebuilt->clust_pcur);

	if (prebuilt->mysql_template) {
		mem_free(prebuilt->mysql_template);
	}

	if (prebuilt->ins_graph) {
		que_graph_free_recursive(prebuilt->ins_graph);
	}

	if (prebuilt->sel_graph) {
		que_graph_free_recursive(prebuilt->sel_graph);
	}

	if (prebuilt->upd_graph) {
		que_graph_free_recursive(prebuilt->upd_graph);
	}

	if (prebuilt->blob_heap) {
		mem_heap_free(prebuilt->blob_heap);
	}

	if (prebuilt->old_vers_heap) {
		mem_heap_free(prebuilt->old_vers_heap);
	}

	if (prebuilt->fetch_cache[0] != NULL) {
		byte*	base = prebuilt->fetch_cache[0] - 4;
		byte*	ptr = base;

		for (i = 0; i < MYSQL_FETCH_CACHE_SIZE; i++) {
			byte*	row;
			ulint	magic1;
			ulint	magic2;

			magic1 = mach_read_from_4(ptr);
			ptr += 4;

			row = ptr;
			ptr += prebuilt->mysql_row_len;

			magic2 = mach_read_from_4(ptr);
			ptr += 4;

			if (ROW_PREBUILT_FETCH_MAGIC_N != magic1
			    || row != prebuilt->fetch_cache[i]
			    || ROW_PREBUILT_FETCH_MAGIC_N != magic2) {

				fputs("InnoDB: Error: trying to free"
					" a corrupt fetch buffer.\n", stderr);

				mem_analyze_corruption(base);
				ut_error;
			}
		}

		mem_free(base);
	}

	dict_table_close(prebuilt->table, dict_locked, TRUE);

	mem_heap_free(prebuilt->heap);
}

/*********************************************************************//**
Updates the transaction pointers in query graphs stored in the prebuilt
struct. */
UNIV_INTERN
void
row_update_prebuilt_trx(
/*====================*/
	row_prebuilt_t*	prebuilt,	/*!< in/out: prebuilt struct
					in MySQL handle */
	trx_t*		trx)		/*!< in: transaction handle */
{
	if (trx->magic_n != TRX_MAGIC_N) {
		fprintf(stderr,
			"InnoDB: Error: trying to use a corrupt\n"
			"InnoDB: trx handle. Magic n %lu\n",
			(ulong) trx->magic_n);

		mem_analyze_corruption(trx);

		ut_error;
	}

	if (prebuilt->magic_n != ROW_PREBUILT_ALLOCATED) {
		fprintf(stderr,
			"InnoDB: Error: trying to use a corrupt\n"
			"InnoDB: table handle. Magic n %lu, table name ",
			(ulong) prebuilt->magic_n);
		ut_print_name(stderr, trx, TRUE, prebuilt->table->name);
		putc('\n', stderr);

		mem_analyze_corruption(prebuilt);

		ut_error;
	}

	prebuilt->trx = trx;

	if (prebuilt->ins_graph) {
		prebuilt->ins_graph->trx = trx;
	}

	if (prebuilt->upd_graph) {
		prebuilt->upd_graph->trx = trx;
	}

	if (prebuilt->sel_graph) {
		prebuilt->sel_graph->trx = trx;
	}
}

/*********************************************************************//**
Gets pointer to a prebuilt dtuple used in insertions. If the insert graph
has not yet been built in the prebuilt struct, then this function first
builds it.
@return	prebuilt dtuple; the column type information is also set in it */
static
dtuple_t*
row_get_prebuilt_insert_row(
/*========================*/
	row_prebuilt_t*	prebuilt)	/*!< in: prebuilt struct in MySQL
					handle */
{
	dict_table_t*		table	= prebuilt->table;

	ut_ad(prebuilt && table && prebuilt->trx);

	if (prebuilt->ins_node != 0) {

		/* Check if indexes have been dropped or added and we
		may need to rebuild the row insert template. */

		if (prebuilt->trx_id == table->def_trx_id
		    && UT_LIST_GET_LEN(prebuilt->ins_node->entry_list)
		    == UT_LIST_GET_LEN(table->indexes)) {

			return(prebuilt->ins_node->row);
		}

		ut_ad(prebuilt->trx_id < table->def_trx_id);

		que_graph_free_recursive(prebuilt->ins_graph);

		prebuilt->ins_graph = 0;
	}

	/* Create an insert node and query graph to the prebuilt struct */

	ins_node_t*		node;

	node = ins_node_create(INS_DIRECT, table, prebuilt->heap);

	prebuilt->ins_node = node;

	if (prebuilt->ins_upd_rec_buff == 0) {
		prebuilt->ins_upd_rec_buff = static_cast<byte*>(
			mem_heap_alloc(
				prebuilt->heap,
				prebuilt->mysql_row_len));
	}

	dtuple_t*	row;

	row = dtuple_create(prebuilt->heap, dict_table_get_n_cols(table));

	dict_table_copy_types(row, table);

	ins_node_set_new_row(node, row);

	prebuilt->ins_graph = static_cast<que_fork_t*>(
		que_node_get_parent(
			pars_complete_graph_for_exec(
				node,
				prebuilt->trx, prebuilt->heap)));

	prebuilt->ins_graph->state = QUE_FORK_ACTIVE;

	prebuilt->trx_id = table->def_trx_id;

	return(prebuilt->ins_node->row);
}

/*********************************************************************//**
Updates the table modification counter and calculates new estimates
for table and index statistics if necessary. */
UNIV_INLINE
void
row_update_statistics_if_needed(
/*============================*/
	dict_table_t*	table)	/*!< in: table */
{
	ib_uint64_t	counter;
	ib_uint64_t	n_rows;

	if (!table->stat_initialized) {
		DBUG_EXECUTE_IF(
			"test_upd_stats_if_needed_not_inited",
			fprintf(stderr, "test_upd_stats_if_needed_not_inited "
				"was executed\n");
		);
		return;
	}

	counter = table->stat_modified_counter++;
	n_rows = dict_table_get_n_rows(table);

	if (dict_stats_is_persistent_enabled(table)) {
		if (counter > n_rows / 10 /* 10% */
		    && dict_stats_auto_recalc_is_enabled(table)) {

			dict_stats_recalc_pool_add(table);
			table->stat_modified_counter = 0;
		}
		return;
	}

	/* Calculate new statistics if 1 / 16 of table has been modified
	since the last time a statistics batch was run.
	We calculate statistics at most every 16th round, since we may have
	a counter table which is very small and updated very often. */

	if (counter > 16 + n_rows / 16 /* 6.25% */) {

		ut_ad(!mutex_own(&dict_sys->mutex));
		/* this will reset table->stat_modified_counter to 0 */
		dict_stats_update(table, DICT_STATS_RECALC_TRANSIENT);
	}
}

/*********************************************************************//**
Sets an AUTO_INC type lock on the table mentioned in prebuilt. The
AUTO_INC lock gives exclusive access to the auto-inc counter of the
table. The lock is reserved only for the duration of an SQL statement.
It is not compatible with another AUTO_INC or exclusive lock on the
table.
@return	error code or DB_SUCCESS */
UNIV_INTERN
dberr_t
row_lock_table_autoinc_for_mysql(
/*=============================*/
	row_prebuilt_t*	prebuilt)	/*!< in: prebuilt struct in the MySQL
					table handle */
{
	trx_t*			trx	= prebuilt->trx;
	ins_node_t*		node	= prebuilt->ins_node;
	const dict_table_t*	table	= prebuilt->table;
	que_thr_t*		thr;
	dberr_t			err;
	ibool			was_lock_wait;

	ut_ad(trx);

	/* If we already hold an AUTOINC lock on the table then do nothing.
        Note: We peek at the value of the current owner without acquiring
	the lock mutex. **/
	if (trx == table->autoinc_trx) {

		return(DB_SUCCESS);
	}

	trx->op_info = "setting auto-inc lock";

	row_get_prebuilt_insert_row(prebuilt);
	node = prebuilt->ins_node;

	/* We use the insert query graph as the dummy graph needed
	in the lock module call */

	thr = que_fork_get_first_thr(prebuilt->ins_graph);

	que_thr_move_to_run_state_for_mysql(thr, trx);

run_again:
	thr->run_node = node;
	thr->prev_node = node;

	/* It may be that the current session has not yet started
	its transaction, or it has been committed: */

	trx_start_if_not_started_xa(trx);

	err = lock_table(0, prebuilt->table, LOCK_AUTO_INC, thr);

	trx->error_state = err;

	if (err != DB_SUCCESS) {
		que_thr_stop_for_mysql(thr);

		was_lock_wait = row_mysql_handle_errors(&err, trx, thr, NULL);

		if (was_lock_wait) {
			goto run_again;
		}

		trx->op_info = "";

		return(err);
	}

	que_thr_stop_for_mysql_no_error(thr, trx);

	trx->op_info = "";

	return(err);
}

/*********************************************************************//**
Sets a table lock on the table mentioned in prebuilt.
@return	error code or DB_SUCCESS */
UNIV_INTERN
dberr_t
row_lock_table_for_mysql(
/*=====================*/
	row_prebuilt_t*	prebuilt,	/*!< in: prebuilt struct in the MySQL
					table handle */
	dict_table_t*	table,		/*!< in: table to lock, or NULL
					if prebuilt->table should be
					locked as
					prebuilt->select_lock_type */
	ulint		mode)		/*!< in: lock mode of table
					(ignored if table==NULL) */
{
	trx_t*		trx		= prebuilt->trx;
	que_thr_t*	thr;
	dberr_t		err;
	ibool		was_lock_wait;

	ut_ad(trx);

	trx->op_info = "setting table lock";

	if (prebuilt->sel_graph == NULL) {
		/* Build a dummy select query graph */
		row_prebuild_sel_graph(prebuilt);
	}

	/* We use the select query graph as the dummy graph needed
	in the lock module call */

	thr = que_fork_get_first_thr(prebuilt->sel_graph);

	que_thr_move_to_run_state_for_mysql(thr, trx);

run_again:
	thr->run_node = thr;
	thr->prev_node = thr->common.parent;

	/* It may be that the current session has not yet started
	its transaction, or it has been committed: */

	trx_start_if_not_started_xa(trx);

	if (table) {
		err = lock_table(
			0, table,
			static_cast<enum lock_mode>(mode), thr);
	} else {
		err = lock_table(
			0, prebuilt->table,
			static_cast<enum lock_mode>(
				prebuilt->select_lock_type),
			thr);
	}

	trx->error_state = err;

	if (err != DB_SUCCESS) {
		que_thr_stop_for_mysql(thr);

		was_lock_wait = row_mysql_handle_errors(&err, trx, thr, NULL);

		if (was_lock_wait) {
			goto run_again;
		}

		trx->op_info = "";

		return(err);
	}

	que_thr_stop_for_mysql_no_error(thr, trx);

	trx->op_info = "";

	return(err);
}

/*********************************************************************//**
Does an insert for MySQL.
@return	error code or DB_SUCCESS */
UNIV_INTERN
dberr_t
row_insert_for_mysql(
/*=================*/
	byte*		mysql_rec,	/*!< in: row in the MySQL format */
	row_prebuilt_t*	prebuilt)	/*!< in: prebuilt struct in MySQL
					handle */
{
	trx_savept_t	savept;
	que_thr_t*	thr;
	dberr_t		err;
	ibool		was_lock_wait;
	trx_t*		trx		= prebuilt->trx;
	ins_node_t*	node		= prebuilt->ins_node;
	dict_table_t*	table		= prebuilt->table;

	ut_ad(trx);

	if (dict_table_is_discarded(prebuilt->table)) {
		ib_logf(IB_LOG_LEVEL_ERROR,
			"The table %s doesn't have a corresponding "
			"tablespace, it was discarded.",
			prebuilt->table->name);

		return(DB_TABLESPACE_DELETED);

	} else if (prebuilt->table->ibd_file_missing) {

		ib_logf(IB_LOG_LEVEL_ERROR,
			".ibd file is missing for table %s",
			prebuilt->table->name);

		return(DB_TABLESPACE_NOT_FOUND);

	} else if (prebuilt->magic_n != ROW_PREBUILT_ALLOCATED) {
		fprintf(stderr,
			"InnoDB: Error: trying to free a corrupt\n"
			"InnoDB: table handle. Magic n %lu, table name ",
			(ulong) prebuilt->magic_n);
		ut_print_name(stderr, trx, TRUE, prebuilt->table->name);
		putc('\n', stderr);

		mem_analyze_corruption(prebuilt);

		ut_error;
	} else if (srv_created_new_raw || srv_force_recovery) {
		fputs("InnoDB: A new raw disk partition was initialized or\n"
		      "InnoDB: innodb_force_recovery is on: we do not allow\n"
		      "InnoDB: database modifications by the user. Shut down\n"
		      "InnoDB: mysqld and edit my.cnf so that"
		      " newraw is replaced\n"
		      "InnoDB: with raw, and innodb_force_... is removed.\n",
		      stderr);

		return(DB_ERROR);
	}

	trx->op_info = "inserting";

	row_mysql_delay_if_needed();

	trx_start_if_not_started_xa(trx);

	row_get_prebuilt_insert_row(prebuilt);
	node = prebuilt->ins_node;

	row_mysql_convert_row_to_innobase(node->row, prebuilt, mysql_rec);

	savept = trx_savept_take(trx);

	thr = que_fork_get_first_thr(prebuilt->ins_graph);

	if (prebuilt->sql_stat_start) {
		node->state = INS_NODE_SET_IX_LOCK;
		prebuilt->sql_stat_start = FALSE;
	} else {
		node->state = INS_NODE_ALLOC_ROW_ID;
	}

	que_thr_move_to_run_state_for_mysql(thr, trx);

run_again:
	thr->run_node = node;
	thr->prev_node = node;

	row_ins_step(thr);

	err = trx->error_state;

	if (err != DB_SUCCESS) {
error_exit:
		que_thr_stop_for_mysql(thr);

		/* FIXME: What's this ? */
		thr->lock_state = QUE_THR_LOCK_ROW;

		was_lock_wait = row_mysql_handle_errors(
			&err, trx, thr, &savept);

		thr->lock_state = QUE_THR_LOCK_NOLOCK;

		if (was_lock_wait) {
			ut_ad(node->state == INS_NODE_INSERT_ENTRIES
			      || node->state == INS_NODE_ALLOC_ROW_ID);
			goto run_again;
		}

		trx->op_info = "";

		return(err);
	}

	if (dict_table_has_fts_index(table)) {
		doc_id_t        doc_id;

		/* Extract the doc id from the hidden FTS column */
		doc_id = fts_get_doc_id_from_row(table, node->row);

		if (doc_id <= 0) {
			fprintf(stderr,
				"InnoDB: FTS Doc ID must be large than 0 \n");
			err = DB_FTS_INVALID_DOCID;
			trx->error_state = DB_FTS_INVALID_DOCID;
			goto error_exit;
		}

		if (!DICT_TF2_FLAG_IS_SET(table, DICT_TF2_FTS_HAS_DOC_ID)) {
			doc_id_t	next_doc_id
				= table->fts->cache->next_doc_id;

			if (doc_id < next_doc_id) {
				fprintf(stderr,
					"InnoDB: FTS Doc ID must be large than"
					" "UINT64PF" for table",
					next_doc_id - 1);
				ut_print_name(stderr, trx, TRUE, table->name);
				putc('\n', stderr);

				err = DB_FTS_INVALID_DOCID;
				trx->error_state = DB_FTS_INVALID_DOCID;
				goto error_exit;
			}

			/* Difference between Doc IDs are restricted within
			4 bytes integer. See fts_get_encoded_len() */

			if (doc_id - next_doc_id >= FTS_DOC_ID_MAX_STEP) {
				fprintf(stderr,
					"InnoDB: Doc ID "UINT64PF" is too"
					" big. Its difference with largest"
					" used Doc ID "UINT64PF" cannot"
					" exceed or equal to %d\n",
					doc_id, next_doc_id - 1,
					FTS_DOC_ID_MAX_STEP);
				err = DB_FTS_INVALID_DOCID;
				trx->error_state = DB_FTS_INVALID_DOCID;
				goto error_exit;
			}
		}

		/* Pass NULL for the columns affected, since an INSERT affects
		all FTS indexes. */
		fts_trx_add_op(trx, table, doc_id, FTS_INSERT, NULL);
	}

	que_thr_stop_for_mysql_no_error(thr, trx);

	srv_stats.n_rows_inserted.add((size_t)trx->id, 1);

	/* Not protected by dict_table_stats_lock() for performance
	reasons, we would rather get garbage in stat_n_rows (which is
	just an estimate anyway) than protecting the following code
	with a latch. */
	dict_table_n_rows_inc(table);

	row_update_statistics_if_needed(table);
	trx->op_info = "";

	return(err);
}

/*********************************************************************//**
Builds a dummy query graph used in selects. */
UNIV_INTERN
void
row_prebuild_sel_graph(
/*===================*/
	row_prebuilt_t*	prebuilt)	/*!< in: prebuilt struct in MySQL
					handle */
{
	sel_node_t*	node;

	ut_ad(prebuilt && prebuilt->trx);

	if (prebuilt->sel_graph == NULL) {

		node = sel_node_create(prebuilt->heap);

		prebuilt->sel_graph = static_cast<que_fork_t*>(
			que_node_get_parent(
				pars_complete_graph_for_exec(
					static_cast<sel_node_t*>(node),
					prebuilt->trx, prebuilt->heap)));

		prebuilt->sel_graph->state = QUE_FORK_ACTIVE;
	}
}

/*********************************************************************//**
Creates an query graph node of 'update' type to be used in the MySQL
interface.
@return	own: update node */
UNIV_INTERN
upd_node_t*
row_create_update_node_for_mysql(
/*=============================*/
	dict_table_t*	table,	/*!< in: table to update */
	mem_heap_t*	heap)	/*!< in: mem heap from which allocated */
{
	upd_node_t*	node;

	node = upd_node_create(heap);

	node->in_mysql_interface = TRUE;
	node->is_delete = FALSE;
	node->searched_update = FALSE;
	node->select = NULL;
	node->pcur = btr_pcur_create_for_mysql();
	node->table = table;

	node->update = upd_create(dict_table_get_n_cols(table), heap);

	node->update_n_fields = dict_table_get_n_cols(table);

	UT_LIST_INIT(node->columns);
	node->has_clust_rec_x_lock = TRUE;
	node->cmpl_info = 0;

	node->table_sym = NULL;
	node->col_assign_list = NULL;

	return(node);
}

/*********************************************************************//**
Gets pointer to a prebuilt update vector used in updates. If the update
graph has not yet been built in the prebuilt struct, then this function
first builds it.
@return	prebuilt update vector */
UNIV_INTERN
upd_t*
row_get_prebuilt_update_vector(
/*===========================*/
	row_prebuilt_t*	prebuilt)	/*!< in: prebuilt struct in MySQL
					handle */
{
	dict_table_t*	table	= prebuilt->table;
	upd_node_t*	node;

	ut_ad(prebuilt && table && prebuilt->trx);

	if (prebuilt->upd_node == NULL) {

		/* Not called before for this handle: create an update node
		and query graph to the prebuilt struct */

		node = row_create_update_node_for_mysql(table, prebuilt->heap);

		prebuilt->upd_node = node;

		prebuilt->upd_graph = static_cast<que_fork_t*>(
			que_node_get_parent(
				pars_complete_graph_for_exec(
					static_cast<upd_node_t*>(node),
					prebuilt->trx, prebuilt->heap)));

		prebuilt->upd_graph->state = QUE_FORK_ACTIVE;
	}

	return(prebuilt->upd_node->update);
}

/********************************************************************
Handle an update of a column that has an FTS index. */
static
void
row_fts_do_update(
/*==============*/
	trx_t*		trx,		/* in: transaction */
	dict_table_t*	table,		/* in: Table with FTS index */
	doc_id_t	old_doc_id,	/* in: old document id */
	doc_id_t	new_doc_id)	/* in: new document id */
{
	if (trx->fts_next_doc_id) {
		fts_trx_add_op(trx, table, old_doc_id, FTS_DELETE, NULL);
		fts_trx_add_op(trx, table, new_doc_id, FTS_INSERT, NULL);
	}
}

/************************************************************************
Handles FTS matters for an update or a delete.
NOTE: should not be called if the table does not have an FTS index. .*/
static
dberr_t
row_fts_update_or_delete(
/*=====================*/
	row_prebuilt_t*	prebuilt)	/* in: prebuilt struct in MySQL
					handle */
{
	trx_t*		trx = prebuilt->trx;
	dict_table_t*	table = prebuilt->table;
	upd_node_t*	node = prebuilt->upd_node;
	doc_id_t	old_doc_id = prebuilt->fts_doc_id;

	ut_a(dict_table_has_fts_index(prebuilt->table));

	/* Deletes are simple; get them out of the way first. */
	if (node->is_delete) {
		/* A delete affects all FTS indexes, so we pass NULL */
		fts_trx_add_op(trx, table, old_doc_id, FTS_DELETE, NULL);
	} else {
		doc_id_t	new_doc_id;

		new_doc_id = fts_read_doc_id((byte*) &trx->fts_next_doc_id);

		if (new_doc_id == 0) {
			fprintf(stderr, " InnoDB FTS: Doc ID cannot be 0 \n");
			return(DB_FTS_INVALID_DOCID);
		}

		row_fts_do_update(trx, table, old_doc_id, new_doc_id);
	}

	return(DB_SUCCESS);
}

/*********************************************************************//**
Initialize the Doc ID system for FK table with FTS index */
static
void
init_fts_doc_id_for_ref(
/*====================*/
	dict_table_t*	table,		/*!< in: table */
	ulint*		depth)		/*!< in: recusive call depth */
{
	dict_foreign_t* foreign;

	foreign = UT_LIST_GET_FIRST(table->referenced_list);

	table->fk_max_recusive_level = 0;

	(*depth)++;

	/* Limit on tables involved in cascading delete/update */
	if (*depth > FK_MAX_CASCADE_DEL) {
		return;
	}

	/* Loop through this table's referenced list and also
	recursively traverse each table's foreign table list */
	while (foreign && foreign->foreign_table) {
		if (foreign->foreign_table->fts) {
			fts_init_doc_id(foreign->foreign_table);
		}

		if (UT_LIST_GET_LEN(foreign->foreign_table->referenced_list)
		    > 0 && foreign->foreign_table != table) {
			init_fts_doc_id_for_ref(foreign->foreign_table, depth);
		}

		foreign = UT_LIST_GET_NEXT(referenced_list, foreign);
	}
}

/*********************************************************************//**
Does an update or delete of a row for MySQL.
@return	error code or DB_SUCCESS */
UNIV_INTERN
dberr_t
row_update_for_mysql(
/*=================*/
	byte*		mysql_rec,	/*!< in: the row to be updated, in
					the MySQL format */
	row_prebuilt_t*	prebuilt)	/*!< in: prebuilt struct in MySQL
					handle */
{
	trx_savept_t	savept;
	dberr_t		err;
	que_thr_t*	thr;
	ibool		was_lock_wait;
	dict_index_t*	clust_index;
	/*	ulint		ref_len; */
	upd_node_t*	node;
	dict_table_t*	table		= prebuilt->table;
	trx_t*		trx		= prebuilt->trx;
	ulint		fk_depth	= 0;

	ut_ad(prebuilt && trx);
	UT_NOT_USED(mysql_rec);

	if (prebuilt->table->ibd_file_missing) {
		ut_print_timestamp(stderr);
		fprintf(stderr, "  InnoDB: Error:\n"
			"InnoDB: MySQL is trying to use a table handle"
			" but the .ibd file for\n"
			"InnoDB: table %s does not exist.\n"
			"InnoDB: Have you deleted the .ibd file"
			" from the database directory under\n"
			"InnoDB: the MySQL datadir, or have you"
			" used DISCARD TABLESPACE?\n"
			"InnoDB: Look from\n"
			"InnoDB: " REFMAN "innodb-troubleshooting.html\n"
			"InnoDB: how you can resolve the problem.\n",
			prebuilt->table->name);
		return(DB_ERROR);
	}

	if (UNIV_UNLIKELY(prebuilt->magic_n != ROW_PREBUILT_ALLOCATED)) {
		fprintf(stderr,
			"InnoDB: Error: trying to free a corrupt\n"
			"InnoDB: table handle. Magic n %lu, table name ",
			(ulong) prebuilt->magic_n);
		ut_print_name(stderr, trx, TRUE, prebuilt->table->name);
		putc('\n', stderr);

		mem_analyze_corruption(prebuilt);

		ut_error;
	}

	if (UNIV_UNLIKELY(srv_created_new_raw || srv_force_recovery)) {
		fputs("InnoDB: A new raw disk partition was initialized or\n"
		      "InnoDB: innodb_force_recovery is on: we do not allow\n"
		      "InnoDB: database modifications by the user. Shut down\n"
		      "InnoDB: mysqld and edit my.cnf so that newraw"
		      " is replaced\n"
		      "InnoDB: with raw, and innodb_force_... is removed.\n",
		      stderr);

		return(DB_ERROR);
	}

	DEBUG_SYNC_C("innodb_row_update_for_mysql_begin");

	trx->op_info = "updating or deleting";

	row_mysql_delay_if_needed();

	trx_start_if_not_started_xa(trx);

	if (dict_table_is_referenced_by_foreign_key(table)) {
		/* Share lock the data dictionary to prevent any
		table dictionary (for foreign constraint) change.
		This is similar to row_ins_check_foreign_constraint
		check protect by the dictionary lock as well.
		In the future, this can be removed once the Foreign
		key MDL is implemented */
		row_mysql_freeze_data_dictionary(trx);
		init_fts_doc_id_for_ref(table, &fk_depth);
		row_mysql_unfreeze_data_dictionary(trx);
	}

	node = prebuilt->upd_node;

	clust_index = dict_table_get_first_index(table);

	if (prebuilt->pcur.btr_cur.index == clust_index) {
		btr_pcur_copy_stored_position(node->pcur, &prebuilt->pcur);
	} else {
		btr_pcur_copy_stored_position(node->pcur,
					      &prebuilt->clust_pcur);
	}

	ut_a(node->pcur->rel_pos == BTR_PCUR_ON);

	/* MySQL seems to call rnd_pos before updating each row it
	has cached: we can get the correct cursor position from
	prebuilt->pcur; NOTE that we cannot build the row reference
	from mysql_rec if the clustered index was automatically
	generated for the table: MySQL does not know anything about
	the row id used as the clustered index key */

	savept = trx_savept_take(trx);

	thr = que_fork_get_first_thr(prebuilt->upd_graph);

	node->state = UPD_NODE_UPDATE_CLUSTERED;

	ut_ad(!prebuilt->sql_stat_start);

	que_thr_move_to_run_state_for_mysql(thr, trx);

run_again:
	thr->run_node = node;
	thr->prev_node = node;
	thr->fk_cascade_depth = 0;

	row_upd_step(thr);

	err = trx->error_state;

	/* Reset fk_cascade_depth back to 0 */
	thr->fk_cascade_depth = 0;

	if (err != DB_SUCCESS) {
		que_thr_stop_for_mysql(thr);

		if (err == DB_RECORD_NOT_FOUND) {
			trx->error_state = DB_SUCCESS;
			trx->op_info = "";

			return(err);
		}

		thr->lock_state= QUE_THR_LOCK_ROW;

		DEBUG_SYNC(trx->mysql_thd, "row_update_for_mysql_error");

		was_lock_wait = row_mysql_handle_errors(&err, trx, thr,
							&savept);
		thr->lock_state= QUE_THR_LOCK_NOLOCK;

		if (was_lock_wait) {
			goto run_again;
		}

		trx->op_info = "";

		return(err);
	}

	que_thr_stop_for_mysql_no_error(thr, trx);

	if (dict_table_has_fts_index(table)
	    && trx->fts_next_doc_id != UINT64_UNDEFINED) {
		err = row_fts_update_or_delete(prebuilt);
		if (err != DB_SUCCESS) {
			trx->op_info = "";
			return(err);
		}
	}

	if (node->is_delete) {
		/* Not protected by dict_table_stats_lock() for performance
		reasons, we would rather get garbage in stat_n_rows (which is
		just an estimate anyway) than protecting the following code
		with a latch. */
		dict_table_n_rows_dec(prebuilt->table);

		srv_stats.n_rows_deleted.add((size_t)trx->id, 1);
	} else {
		srv_stats.n_rows_updated.add((size_t)trx->id, 1);
	}

	/* We update table statistics only if it is a DELETE or UPDATE
	that changes indexed columns, UPDATEs that change only non-indexed
	columns would not affect statistics. */
	if (node->is_delete || !(node->cmpl_info & UPD_NODE_NO_ORD_CHANGE)) {
		row_update_statistics_if_needed(prebuilt->table);
	}

	trx->op_info = "";

	return(err);
}

/*********************************************************************//**
This can only be used when srv_locks_unsafe_for_binlog is TRUE or this
session is using a READ COMMITTED or READ UNCOMMITTED isolation level.
Before calling this function row_search_for_mysql() must have
initialized prebuilt->new_rec_locks to store the information which new
record locks really were set. This function removes a newly set
clustered index record lock under prebuilt->pcur or
prebuilt->clust_pcur.  Thus, this implements a 'mini-rollback' that
releases the latest clustered index record lock we set.
@return error code or DB_SUCCESS */
UNIV_INTERN
void
row_unlock_for_mysql(
/*=================*/
	row_prebuilt_t*	prebuilt,	/*!< in/out: prebuilt struct in MySQL
					handle */
	ibool		has_latches_on_recs)/*!< in: TRUE if called so
					that we have the latches on
					the records under pcur and
					clust_pcur, and we do not need
					to reposition the cursors. */
{
	btr_pcur_t*	pcur		= &prebuilt->pcur;
	btr_pcur_t*	clust_pcur	= &prebuilt->clust_pcur;
	trx_t*		trx		= prebuilt->trx;

	ut_ad(prebuilt && trx);

	if (UNIV_UNLIKELY
	    (!srv_locks_unsafe_for_binlog
	     && trx->isolation_level > TRX_ISO_READ_COMMITTED)) {

		fprintf(stderr,
			"InnoDB: Error: calling row_unlock_for_mysql though\n"
			"InnoDB: innodb_locks_unsafe_for_binlog is FALSE and\n"
			"InnoDB: this session is not using"
			" READ COMMITTED isolation level.\n");
		return;
	}

	trx->op_info = "unlock_row";

	if (prebuilt->new_rec_locks >= 1) {

		const rec_t*	rec;
		dict_index_t*	index;
		trx_id_t	rec_trx_id;
		mtr_t		mtr;

		mtr_start(&mtr);

		/* Restore the cursor position and find the record */

		if (!has_latches_on_recs) {
			btr_pcur_restore_position(BTR_SEARCH_LEAF, pcur, &mtr);
		}

		rec = btr_pcur_get_rec(pcur);
		index = btr_pcur_get_btr_cur(pcur)->index;

		if (prebuilt->new_rec_locks >= 2) {
			/* Restore the cursor position and find the record
			in the clustered index. */

			if (!has_latches_on_recs) {
				btr_pcur_restore_position(BTR_SEARCH_LEAF,
							  clust_pcur, &mtr);
			}

			rec = btr_pcur_get_rec(clust_pcur);
			index = btr_pcur_get_btr_cur(clust_pcur)->index;
		}

		if (!dict_index_is_clust(index)) {
			/* This is not a clustered index record.  We
			do not know how to unlock the record. */
			goto no_unlock;
		}

		/* If the record has been modified by this
		transaction, do not unlock it. */

		if (index->trx_id_offset) {
			rec_trx_id = trx_read_trx_id(rec
						     + index->trx_id_offset);
		} else {
			mem_heap_t*	heap			= NULL;
			ulint	offsets_[REC_OFFS_NORMAL_SIZE];
			ulint*	offsets				= offsets_;

			rec_offs_init(offsets_);
			offsets = rec_get_offsets(rec, index, offsets,
						  ULINT_UNDEFINED, &heap);

			rec_trx_id = row_get_rec_trx_id(rec, index, offsets);

			if (UNIV_LIKELY_NULL(heap)) {
				mem_heap_free(heap);
			}
		}

		if (rec_trx_id != trx->id) {
			/* We did not update the record: unlock it */

			rec = btr_pcur_get_rec(pcur);

			lock_rec_unlock(
				trx,
				btr_pcur_get_block(pcur),
				rec,
				static_cast<enum lock_mode>(
					prebuilt->select_lock_type));

			if (prebuilt->new_rec_locks >= 2) {
				rec = btr_pcur_get_rec(clust_pcur);

				lock_rec_unlock(
					trx,
					btr_pcur_get_block(clust_pcur),
					rec,
					static_cast<enum lock_mode>(
						prebuilt->select_lock_type));
			}
		}
no_unlock:
		mtr_commit(&mtr);
	}

	trx->op_info = "";
}

/**********************************************************************//**
Does a cascaded delete or set null in a foreign key operation.
@return	error code or DB_SUCCESS */
UNIV_INTERN
dberr_t
row_update_cascade_for_mysql(
/*=========================*/
	que_thr_t*	thr,	/*!< in: query thread */
	upd_node_t*	node,	/*!< in: update node used in the cascade
				or set null operation */
	dict_table_t*	table)	/*!< in: table where we do the operation */
{
	dberr_t	err;
	trx_t*	trx;

	trx = thr_get_trx(thr);

	/* Increment fk_cascade_depth to record the recursive call depth on
	a single update/delete that affects multiple tables chained
	together with foreign key relations. */
	thr->fk_cascade_depth++;

	if (thr->fk_cascade_depth > FK_MAX_CASCADE_DEL) {
		return(DB_FOREIGN_EXCEED_MAX_CASCADE);
	}
run_again:
	thr->run_node = node;
	thr->prev_node = node;

	DEBUG_SYNC_C("foreign_constraint_update_cascade");

	row_upd_step(thr);

	/* The recursive call for cascading update/delete happens
	in above row_upd_step(), reset the counter once we come
	out of the recursive call, so it does not accumulate for
	different row deletes */
	thr->fk_cascade_depth = 0;

	err = trx->error_state;

	/* Note that the cascade node is a subnode of another InnoDB
	query graph node. We do a normal lock wait in this node, but
	all errors are handled by the parent node. */

	if (err == DB_LOCK_WAIT) {
		/* Handle lock wait here */

		que_thr_stop_for_mysql(thr);

		lock_wait_suspend_thread(thr);

		/* Note that a lock wait may also end in a lock wait timeout,
		or this transaction is picked as a victim in selective
		deadlock resolution */

		if (trx->error_state != DB_SUCCESS) {

			return(trx->error_state);
		}

		/* Retry operation after a normal lock wait */

		goto run_again;
	}

	if (err != DB_SUCCESS) {

		return(err);
	}

	if (node->is_delete) {
		/* Not protected by dict_table_stats_lock() for performance
		reasons, we would rather get garbage in stat_n_rows (which is
		just an estimate anyway) than protecting the following code
		with a latch. */
		dict_table_n_rows_dec(table);

		srv_stats.n_rows_deleted.add((size_t)trx->id, 1);
	} else {
		srv_stats.n_rows_updated.add((size_t)trx->id, 1);
	}

	row_update_statistics_if_needed(table);

	return(err);
}

/*********************************************************************//**
Checks if a table is such that we automatically created a clustered
index on it (on row id).
@return	TRUE if the clustered index was generated automatically */
UNIV_INTERN
ibool
row_table_got_default_clust_index(
/*==============================*/
	const dict_table_t*	table)	/*!< in: table */
{
	const dict_index_t*	clust_index;

	clust_index = dict_table_get_first_index(table);

	return(dict_index_get_nth_col(clust_index, 0)->mtype == DATA_SYS);
}

/*********************************************************************//**
Locks the data dictionary in shared mode from modifications, for performing
foreign key check, rollback, or other operation invisible to MySQL. */
UNIV_INTERN
void
row_mysql_freeze_data_dictionary_func(
/*==================================*/
	trx_t*		trx,	/*!< in/out: transaction */
	const char*	file,	/*!< in: file name */
	ulint		line)	/*!< in: line number */
{
	ut_a(trx->dict_operation_lock_mode == 0);

	rw_lock_s_lock_inline(&dict_operation_lock, 0, file, line);

	trx->dict_operation_lock_mode = RW_S_LATCH;
}

/*********************************************************************//**
Unlocks the data dictionary shared lock. */
UNIV_INTERN
void
row_mysql_unfreeze_data_dictionary(
/*===============================*/
	trx_t*	trx)	/*!< in/out: transaction */
{
	ut_ad(lock_trx_has_sys_table_locks(trx) == NULL);

	ut_a(trx->dict_operation_lock_mode == RW_S_LATCH);

	rw_lock_s_unlock(&dict_operation_lock);

	trx->dict_operation_lock_mode = 0;
}

/*********************************************************************//**
Locks the data dictionary exclusively for performing a table create or other
data dictionary modification operation. */
UNIV_INTERN
void
row_mysql_lock_data_dictionary_func(
/*================================*/
	trx_t*		trx,	/*!< in/out: transaction */
	const char*	file,	/*!< in: file name */
	ulint		line)	/*!< in: line number */
{
	ut_a(trx->dict_operation_lock_mode == 0
	     || trx->dict_operation_lock_mode == RW_X_LATCH);

	/* Serialize data dictionary operations with dictionary mutex:
	no deadlocks or lock waits can occur then in these operations */

	rw_lock_x_lock_inline(&dict_operation_lock, 0, file, line);
	trx->dict_operation_lock_mode = RW_X_LATCH;

	mutex_enter(&(dict_sys->mutex));
}

/*********************************************************************//**
Unlocks the data dictionary exclusive lock. */
UNIV_INTERN
void
row_mysql_unlock_data_dictionary(
/*=============================*/
	trx_t*	trx)	/*!< in/out: transaction */
{
	ut_ad(lock_trx_has_sys_table_locks(trx) == NULL);

	ut_a(trx->dict_operation_lock_mode == RW_X_LATCH);

	/* Serialize data dictionary operations with dictionary mutex:
	no deadlocks can occur then in these operations */

	mutex_exit(&(dict_sys->mutex));
	rw_lock_x_unlock(&dict_operation_lock);

	trx->dict_operation_lock_mode = 0;
}

/*********************************************************************//**
Creates a table for MySQL. If the name of the table ends in
one of "innodb_monitor", "innodb_lock_monitor", "innodb_tablespace_monitor",
"innodb_table_monitor", then this will also start the printing of monitor
output by the master thread. If the table name ends in "innodb_mem_validate",
InnoDB will try to invoke mem_validate(). On failure the transaction will
be rolled back and the 'table' object will be freed.
@return	error code or DB_SUCCESS */
UNIV_INTERN
dberr_t
row_create_table_for_mysql(
/*=======================*/
	dict_table_t*	table,	/*!< in, own: table definition
				(will be freed, or on DB_SUCCESS
				added to the data dictionary cache) */
	trx_t*		trx,	/*!< in/out: transaction */
	bool		commit)	/*!< in: if true, commit the transaction */
{
	tab_node_t*	node;
	mem_heap_t*	heap;
	que_thr_t*	thr;
	const char*	table_name;
	ulint		table_name_len;
	dberr_t		err;

#ifdef UNIV_SYNC_DEBUG
	ut_ad(rw_lock_own(&dict_operation_lock, RW_LOCK_EX));
#endif /* UNIV_SYNC_DEBUG */
	ut_ad(mutex_own(&(dict_sys->mutex)));
	ut_ad(trx->dict_operation_lock_mode == RW_X_LATCH);

	DBUG_EXECUTE_IF(
		"ib_create_table_fail_at_start_of_row_create_table_for_mysql",
		goto err_exit;
	);

	if (srv_created_new_raw) {
		fputs("InnoDB: A new raw disk partition was initialized:\n"
		      "InnoDB: we do not allow database modifications"
		      " by the user.\n"
		      "InnoDB: Shut down mysqld and edit my.cnf so that newraw"
		      " is replaced with raw.\n", stderr);
err_exit:
		dict_mem_table_free(table);

		if (commit) {
			trx_commit_for_mysql(trx);
		}

		return(DB_ERROR);
	}

	trx->op_info = "creating table";

	if (row_mysql_is_system_table(table->name)) {

		fprintf(stderr,
			"InnoDB: Error: trying to create a MySQL system"
			" table %s of type InnoDB.\n"
			"InnoDB: MySQL system tables must be"
			" of the MyISAM type!\n",
			table->name);
		goto err_exit;
	}

	trx_start_if_not_started_xa(trx);

	/* The table name is prefixed with the database name and a '/'.
	Certain table names starting with 'innodb_' have their special
	meaning regardless of the database name.  Thus, we need to
	ignore the database name prefix in the comparisons. */
	table_name = dict_remove_db_name(table->name);
	table_name_len = strlen(table_name) + 1;

	if (STR_EQ(table_name, table_name_len, S_innodb_monitor)) {

		/* Table equals "innodb_monitor":
		start monitor prints */

		srv_print_innodb_monitor = TRUE;

		/* The lock timeout monitor thread also takes care
		of InnoDB monitor prints */

		os_event_set(lock_sys->timeout_event);
	} else if (STR_EQ(table_name, table_name_len,
			  S_innodb_lock_monitor)) {

		srv_print_innodb_monitor = TRUE;
		srv_print_innodb_lock_monitor = TRUE;
		os_event_set(lock_sys->timeout_event);
	} else if (STR_EQ(table_name, table_name_len,
			  S_innodb_tablespace_monitor)) {

		srv_print_innodb_tablespace_monitor = TRUE;
		os_event_set(lock_sys->timeout_event);
	} else if (STR_EQ(table_name, table_name_len,
			  S_innodb_table_monitor)) {

		srv_print_innodb_table_monitor = TRUE;
		os_event_set(lock_sys->timeout_event);
#ifdef UNIV_MEM_DEBUG
	} else if (STR_EQ(table_name, table_name_len,
			  S_innodb_mem_validate)) {
		/* We define here a debugging feature intended for
		developers */

		fputs("Validating InnoDB memory:\n"
		      "to use this feature you must compile InnoDB with\n"
		      "UNIV_MEM_DEBUG defined in univ.i and"
		      " the server must be\n"
		      "quiet because allocation from a mem heap"
		      " is not protected\n"
		      "by any semaphore.\n", stderr);
		ut_a(mem_validate());
		fputs("Memory validated\n", stderr);
#endif /* UNIV_MEM_DEBUG */
	}

	heap = mem_heap_create(512);

	switch (trx_get_dict_operation(trx)) {
	case TRX_DICT_OP_NONE:
		trx_set_dict_operation(trx, TRX_DICT_OP_TABLE);
	case TRX_DICT_OP_TABLE:
		break;
	case TRX_DICT_OP_INDEX:
		/* If the transaction was previously flagged as
		TRX_DICT_OP_INDEX, we should be creating auxiliary
		tables for full-text indexes. */
		ut_ad(strstr(table->name, "/FTS_") != NULL);
	}

	node = tab_create_graph_create(table, heap, commit);

	thr = pars_complete_graph_for_exec(node, trx, heap);

	ut_a(thr == que_fork_start_command(
			static_cast<que_fork_t*>(que_node_get_parent(thr))));

	que_run_threads(thr);

	err = trx->error_state;

	if (table->space != TRX_SYS_SPACE) {
		ut_a(DICT_TF2_FLAG_IS_SET(table, DICT_TF2_USE_TABLESPACE));

		/* Update SYS_TABLESPACES and SYS_DATAFILES if a new
		tablespace was created. */
		if (err == DB_SUCCESS) {
			char*	path;
			path = fil_space_get_first_path(table->space);

			err = dict_create_add_tablespace_to_dictionary(
				table->space, table->name,
				fil_space_get_flags(table->space),
				path, trx, commit);

			mem_free(path);
		}

		if (err != DB_SUCCESS) {
			/* We must delete the link file. */
			fil_delete_link_file(table->name);
		}
	}

	switch (err) {
	case DB_SUCCESS:
		break;
	case DB_OUT_OF_FILE_SPACE:
		trx->error_state = DB_SUCCESS;
		trx_rollback_to_savepoint(trx, NULL);

		ut_print_timestamp(stderr);
		fputs("  InnoDB: Warning: cannot create table ",
		      stderr);
		ut_print_name(stderr, trx, TRUE, table->name);
		fputs(" because tablespace full\n", stderr);

		if (dict_table_open_on_name(table->name, TRUE, FALSE,
					    DICT_ERR_IGNORE_NONE)) {

			/* Make things easy for the drop table code. */

			if (table->can_be_evicted) {
				dict_table_move_from_lru_to_non_lru(table);
			}

			dict_table_close(table, TRUE, FALSE);

			row_drop_table_for_mysql(table->name, trx, FALSE);

			if (commit) {
				trx_commit_for_mysql(trx);
			}
		} else {
			dict_mem_table_free(table);
		}

		break;

	case DB_TOO_MANY_CONCURRENT_TRXS:
		/* We already have .ibd file here. it should be deleted. */

		if (table->space
		    && fil_delete_tablespace(
			    table->space,
			    BUF_REMOVE_FLUSH_NO_WRITE)
		    != DB_SUCCESS) {

			ut_print_timestamp(stderr);
			fprintf(stderr,
				"  InnoDB: Error: not able to"
				" delete tablespace %lu of table ",
				(ulong) table->space);
			ut_print_name(stderr, trx, TRUE, table->name);
			fputs("!\n", stderr);
		}
		/* fall through */

	case DB_DUPLICATE_KEY:
	case DB_TABLESPACE_EXISTS:
	default:
		trx->error_state = DB_SUCCESS;
		trx_rollback_to_savepoint(trx, NULL);
		dict_mem_table_free(table);
		break;
	}

	que_graph_free((que_t*) que_node_get_parent(thr));

	trx->op_info = "";

	return(err);
}

/*********************************************************************//**
Does an index creation operation for MySQL. TODO: currently failure
to create an index results in dropping the whole table! This is no problem
currently as all indexes must be created at the same time as the table.
@return	error number or DB_SUCCESS */
UNIV_INTERN
dberr_t
row_create_index_for_mysql(
/*=======================*/
	dict_index_t*	index,		/*!< in, own: index definition
					(will be freed) */
	trx_t*		trx,		/*!< in: transaction handle */
	const ulint*	field_lengths)	/*!< in: if not NULL, must contain
					dict_index_get_n_fields(index)
					actual field lengths for the
					index columns, which are
					then checked for not being too
					large. */
{
	ind_node_t*	node;
	mem_heap_t*	heap;
	que_thr_t*	thr;
	dberr_t		err;
	ulint		i;
	ulint		len;
	char*		table_name;
	char*		index_name;
	dict_table_t*	table;
	ibool		is_fts;

#ifdef UNIV_SYNC_DEBUG
	ut_ad(rw_lock_own(&dict_operation_lock, RW_LOCK_EX));
#endif /* UNIV_SYNC_DEBUG */
	ut_ad(mutex_own(&(dict_sys->mutex)));

	trx->op_info = "creating index";

	/* Copy the table name because we may want to drop the
	table later, after the index object is freed (inside
	que_run_threads()) and thus index->table_name is not available. */
	table_name = mem_strdup(index->table_name);
	index_name = mem_strdup(index->name);

	is_fts = (index->type == DICT_FTS);

	table = dict_table_open_on_name(table_name, TRUE, TRUE,
					DICT_ERR_IGNORE_NONE);

	trx_start_if_not_started_xa(trx);

	for (i = 0; i < index->n_def; i++) {
		/* Check that prefix_len and actual length
		< DICT_MAX_INDEX_COL_LEN */

		len = dict_index_get_nth_field(index, i)->prefix_len;

		if (field_lengths && field_lengths[i]) {
			len = ut_max(len, field_lengths[i]);
		}

		DBUG_EXECUTE_IF(
			"ib_create_table_fail_at_create_index",
			len = DICT_MAX_FIELD_LEN_BY_FORMAT(table) + 1;
		);

		/* Column or prefix length exceeds maximum column length */
		if (len > (ulint) DICT_MAX_FIELD_LEN_BY_FORMAT(table)) {
			err = DB_TOO_BIG_INDEX_COL;

			dict_mem_index_free(index);
			goto error_handling;
		}
	}

	heap = mem_heap_create(512);

	trx_set_dict_operation(trx, TRX_DICT_OP_TABLE);

	/* Note that the space id where we store the index is inherited from
	the table in dict_build_index_def_step() in dict0crea.cc. */

	node = ind_create_graph_create(index, heap, true);

	thr = pars_complete_graph_for_exec(node, trx, heap);

	ut_a(thr == que_fork_start_command(
			static_cast<que_fork_t*>(que_node_get_parent(thr))));

	que_run_threads(thr);

	err = trx->error_state;

	que_graph_free((que_t*) que_node_get_parent(thr));

	/* Create the index specific FTS auxiliary tables. */
	if (err == DB_SUCCESS && is_fts) {
		dict_index_t*	idx;

		idx = dict_table_get_index_on_name(table, index_name);

		ut_ad(idx);
		err = fts_create_index_tables(trx, idx);
	}

error_handling:
	dict_table_close(table, TRUE, FALSE);

	if (err != DB_SUCCESS) {
		/* We have special error handling here */

		trx->error_state = DB_SUCCESS;

		trx_rollback_to_savepoint(trx, NULL);

		row_drop_table_for_mysql(table_name, trx, FALSE);

		trx_commit_for_mysql(trx);

		trx->error_state = DB_SUCCESS;
	}

	trx->op_info = "";

	mem_free(table_name);
	mem_free(index_name);

	return(err);
}

/*********************************************************************//**
Scans a table create SQL string and adds to the data dictionary
the foreign key constraints declared in the string. This function
should be called after the indexes for a table have been created.
Each foreign key constraint must be accompanied with indexes in
both participating tables. The indexes are allowed to contain more
fields than mentioned in the constraint. Check also that foreign key
constraints which reference this table are ok.
@return	error code or DB_SUCCESS */
UNIV_INTERN
dberr_t
row_table_add_foreign_constraints(
/*==============================*/
	trx_t*		trx,		/*!< in: transaction */
	const char*	sql_string,	/*!< in: table create statement where
					foreign keys are declared like:
				FOREIGN KEY (a, b) REFERENCES table2(c, d),
					table2 can be written also with the
					database name before it: test.table2 */
	size_t		sql_length,	/*!< in: length of sql_string */
	const char*	name,		/*!< in: table full name in the
					normalized form
					database_name/table_name */
	ibool		reject_fks)	/*!< in: if TRUE, fail with error
					code DB_CANNOT_ADD_CONSTRAINT if
					any foreign keys are found. */
{
	dberr_t	err;

	ut_ad(mutex_own(&(dict_sys->mutex)));
#ifdef UNIV_SYNC_DEBUG
	ut_ad(rw_lock_own(&dict_operation_lock, RW_LOCK_EX));
#endif /* UNIV_SYNC_DEBUG */
	ut_a(sql_string);

	trx->op_info = "adding foreign keys";

	trx_start_if_not_started_xa(trx);

	trx_set_dict_operation(trx, TRX_DICT_OP_TABLE);

	err = dict_create_foreign_constraints(trx, sql_string, sql_length,
					      name, reject_fks);

	DBUG_EXECUTE_IF("ib_table_add_foreign_fail",
			err = DB_DUPLICATE_KEY;);

	DEBUG_SYNC_C("table_add_foreign_constraints");

	if (err == DB_SUCCESS) {
		/* Check that also referencing constraints are ok */
		err = dict_load_foreigns(name, NULL, false, true,
					 DICT_ERR_IGNORE_NONE);
	}

	if (err != DB_SUCCESS) {
		/* We have special error handling here */

		trx->error_state = DB_SUCCESS;

		trx_rollback_to_savepoint(trx, NULL);

		row_drop_table_for_mysql(name, trx, FALSE);

		trx_commit_for_mysql(trx);

		trx->error_state = DB_SUCCESS;
	}

	return(err);
}

/*********************************************************************//**
Drops a table for MySQL as a background operation. MySQL relies on Unix
in ALTER TABLE to the fact that the table handler does not remove the
table before all handles to it has been removed. Furhermore, the MySQL's
call to drop table must be non-blocking. Therefore we do the drop table
as a background operation, which is taken care of by the master thread
in srv0srv.cc.
@return	error code or DB_SUCCESS */
static
dberr_t
row_drop_table_for_mysql_in_background(
/*===================================*/
	const char*	name)	/*!< in: table name */
{
	dberr_t	error;
	trx_t*	trx;

	trx = trx_allocate_for_background();

	/* If the original transaction was dropping a table referenced by
	foreign keys, we must set the following to be able to drop the
	table: */

	trx->check_foreigns = FALSE;

	/*	fputs("InnoDB: Error: Dropping table ", stderr);
	ut_print_name(stderr, trx, TRUE, name);
	fputs(" in background drop list\n", stderr); */

	/* Try to drop the table in InnoDB */

	error = row_drop_table_for_mysql(name, trx, FALSE);

	/* Flush the log to reduce probability that the .frm files and
	the InnoDB data dictionary get out-of-sync if the user runs
	with innodb_flush_log_at_trx_commit = 0 */

	log_buffer_flush_to_disk();

	trx_commit_for_mysql(trx);

	trx_free_for_background(trx);

	return(error);
}

/*********************************************************************//**
The master thread in srv0srv.cc calls this regularly to drop tables which
we must drop in background after queries to them have ended. Such lazy
dropping of tables is needed in ALTER TABLE on Unix.
@return	how many tables dropped + remaining tables in list */
UNIV_INTERN
ulint
row_drop_tables_for_mysql_in_background(void)
/*=========================================*/
{
	row_mysql_drop_t*	drop;
	dict_table_t*		table;
	ulint			n_tables;
	ulint			n_tables_dropped = 0;
loop:
	mutex_enter(&row_drop_list_mutex);

	ut_a(row_mysql_drop_list_inited);

	drop = UT_LIST_GET_FIRST(row_mysql_drop_list);

	n_tables = UT_LIST_GET_LEN(row_mysql_drop_list);

	mutex_exit(&row_drop_list_mutex);

	if (drop == NULL) {
		/* All tables dropped */

		return(n_tables + n_tables_dropped);
	}

	table = dict_table_open_on_name(drop->table_name, FALSE, FALSE,
					DICT_ERR_IGNORE_NONE);

	if (table == NULL) {
		/* If for some reason the table has already been dropped
		through some other mechanism, do not try to drop it */

		goto already_dropped;
	}

	ut_a(!table->can_be_evicted);

	dict_table_close(table, FALSE, FALSE);

	if (DB_SUCCESS != row_drop_table_for_mysql_in_background(
		    drop->table_name)) {
		/* If the DROP fails for some table, we return, and let the
		main thread retry later */

		return(n_tables + n_tables_dropped);
	}

	n_tables_dropped++;

already_dropped:
	mutex_enter(&row_drop_list_mutex);

	UT_LIST_REMOVE(row_mysql_drop_list, row_mysql_drop_list, drop);

	MONITOR_DEC(MONITOR_BACKGROUND_DROP_TABLE);

	ut_print_timestamp(stderr);
	fputs("  InnoDB: Dropped table ", stderr);
	ut_print_name(stderr, NULL, TRUE, drop->table_name);
	fputs(" in background drop queue.\n", stderr);

	mem_free(drop->table_name);

	mem_free(drop);

	mutex_exit(&row_drop_list_mutex);

	goto loop;
}

/*********************************************************************//**
Get the background drop list length. NOTE: the caller must own the
drop list mutex!
@return	how many tables in list */
UNIV_INTERN
ulint
row_get_background_drop_list_len_low(void)
/*======================================*/
{
	ulint	len;

	mutex_enter(&row_drop_list_mutex);

	ut_a(row_mysql_drop_list_inited);

	len = UT_LIST_GET_LEN(row_mysql_drop_list);

	mutex_exit(&row_drop_list_mutex);

	return(len);
}

/*********************************************************************//**
If a table is not yet in the drop list, adds the table to the list of tables
which the master thread drops in background. We need this on Unix because in
ALTER TABLE MySQL may call drop table even if the table has running queries on
it. Also, if there are running foreign key checks on the table, we drop the
table lazily.
@return	TRUE if the table was not yet in the drop list, and was added there */
static
ibool
row_add_table_to_background_drop_list(
/*==================================*/
	const char*	name)	/*!< in: table name */
{
	row_mysql_drop_t*	drop;

	mutex_enter(&row_drop_list_mutex);

	ut_a(row_mysql_drop_list_inited);

	/* Look if the table already is in the drop list */
	for (drop = UT_LIST_GET_FIRST(row_mysql_drop_list);
	     drop != NULL;
	     drop = UT_LIST_GET_NEXT(row_mysql_drop_list, drop)) {

		if (strcmp(drop->table_name, name) == 0) {
			/* Already in the list */

			mutex_exit(&row_drop_list_mutex);

			return(FALSE);
		}
	}

	drop = static_cast<row_mysql_drop_t*>(
		mem_alloc(sizeof(row_mysql_drop_t)));

	drop->table_name = mem_strdup(name);

	UT_LIST_ADD_LAST(row_mysql_drop_list, row_mysql_drop_list, drop);

	MONITOR_INC(MONITOR_BACKGROUND_DROP_TABLE);

	/*	fputs("InnoDB: Adding table ", stderr);
	ut_print_name(stderr, trx, TRUE, drop->table_name);
	fputs(" to background drop list\n", stderr); */

	mutex_exit(&row_drop_list_mutex);

	return(TRUE);
}

/*********************************************************************//**
Reassigns the table identifier of a table.
@return	error code or DB_SUCCESS */
UNIV_INTERN
dberr_t
row_mysql_table_id_reassign(
/*========================*/
	dict_table_t*	table,	/*!< in/out: table */
	trx_t*		trx,	/*!< in/out: transaction */
	table_id_t*	new_id)	/*!< out: new table id */
{
	dberr_t		err;
	pars_info_t*	info	= pars_info_create();

	dict_hdr_get_new_id(new_id, NULL, NULL);

	/* Remove all locks except the table-level S and X locks. */
	lock_remove_all_on_table(table, FALSE);

	pars_info_add_ull_literal(info, "old_id", table->id);
	pars_info_add_ull_literal(info, "new_id", *new_id);

	err = que_eval_sql(
		info,
		"PROCEDURE RENUMBER_TABLE_PROC () IS\n"
		"BEGIN\n"
		"UPDATE SYS_TABLES SET ID = :new_id\n"
		" WHERE ID = :old_id;\n"
		"UPDATE SYS_COLUMNS SET TABLE_ID = :new_id\n"
		" WHERE TABLE_ID = :old_id;\n"
		"UPDATE SYS_INDEXES SET TABLE_ID = :new_id\n"
		" WHERE TABLE_ID = :old_id;\n"
		"END;\n", FALSE, trx);

	return(err);
}

/*********************************************************************//**
Setup the pre-requisites for DISCARD TABLESPACE. It will start the transaction,
acquire the data dictionary lock in X mode and open the table.
@return table instance or 0 if not found. */
static
dict_table_t*
row_discard_tablespace_begin(
/*=========================*/
	const char*	name,	/*!< in: table name */
	trx_t*		trx)	/*!< in: transaction handle */
{
	trx->op_info = "discarding tablespace";

	trx_set_dict_operation(trx, TRX_DICT_OP_TABLE);

	trx_start_if_not_started_xa(trx);

	/* Serialize data dictionary operations with dictionary mutex:
	this is to avoid deadlocks during data dictionary operations */

	row_mysql_lock_data_dictionary(trx);

	dict_table_t*	table;

	table = dict_table_open_on_name(
		name, TRUE, FALSE, DICT_ERR_IGNORE_NONE);

	if (table) {
		dict_stats_wait_bg_to_stop_using_table(table, trx);
		ut_a(table->space != TRX_SYS_SPACE);
		ut_a(table->n_foreign_key_checks_running == 0);
	}

	return(table);
}

/*********************************************************************//**
Do the foreign key constraint checks.
@return DB_SUCCESS or error code. */
static
dberr_t
row_discard_tablespace_foreign_key_checks(
/*======================================*/
	const trx_t*		trx,	/*!< in: transaction handle */
	const dict_table_t*	table)	/*!< in: table to be discarded */
{
	const dict_foreign_t*	foreign;

	/* Check if the table is referenced by foreign key constraints from
	some other table (not the table itself) */

	for (foreign = UT_LIST_GET_FIRST(table->referenced_list);
	     foreign && foreign->foreign_table == table;
	     foreign = UT_LIST_GET_NEXT(referenced_list, foreign)) {

	}

	if (!srv_read_only_mode && foreign && trx->check_foreigns) {

		FILE*	ef	= dict_foreign_err_file;

		/* We only allow discarding a referenced table if
		FOREIGN_KEY_CHECKS is set to 0 */

		mutex_enter(&dict_foreign_err_mutex);

		rewind(ef);

		ut_print_timestamp(ef);

		fputs("  Cannot DISCARD table ", ef);
		ut_print_name(stderr, trx, TRUE, table->name);
		fputs("\n"
		      "because it is referenced by ", ef);
		ut_print_name(stderr, trx, TRUE, foreign->foreign_table_name);
		putc('\n', ef);

		mutex_exit(&dict_foreign_err_mutex);

		return(DB_CANNOT_DROP_CONSTRAINT);
	}

	return(DB_SUCCESS);
}

/*********************************************************************//**
Cleanup after the DISCARD TABLESPACE operation.
@return error code. */
static
dberr_t
row_discard_tablespace_end(
/*=======================*/
	trx_t*		trx,	/*!< in/out: transaction handle */
	dict_table_t*	table,	/*!< in/out: table to be discarded */
	dberr_t		err)	/*!< in: error code */
{
	if (table != 0) {
		dict_table_close(table, TRUE, FALSE);
	}

	DBUG_EXECUTE_IF("ib_discard_before_commit_crash",
			log_make_checkpoint_at(LSN_MAX, TRUE);
			DBUG_SUICIDE(););

	trx_commit_for_mysql(trx);

	DBUG_EXECUTE_IF("ib_discard_after_commit_crash",
			log_make_checkpoint_at(LSN_MAX, TRUE);
			DBUG_SUICIDE(););

	row_mysql_unlock_data_dictionary(trx);

	trx->op_info = "";

	return(err);
}

/*********************************************************************//**
Do the DISCARD TABLESPACE operation.
@return DB_SUCCESS or error code. */
static
dberr_t
row_discard_tablespace(
/*===================*/
	trx_t*		trx,	/*!< in/out: transaction handle */
	dict_table_t*	table)	/*!< in/out: table to be discarded */
{
	dberr_t		err;

	/* How do we prevent crashes caused by ongoing operations on
	the table? Old operations could try to access non-existent
	pages. MySQL will block all DML on the table using MDL and a
	DISCARD will not start unless all existing operations on the
	table to be discarded are completed.

	1) Acquire the data dictionary latch in X mode. To prevent any
	internal operations that MySQL is not aware off and also for
	the internal SQL parser.

	2) Purge and rollback: we assign a new table id for the
	table. Since purge and rollback look for the table based on
	the table id, they see the table as 'dropped' and discard
	their operations.

	3) Insert buffer: we remove all entries for the tablespace in
	the insert buffer tree.

	4) FOREIGN KEY operations: if table->n_foreign_key_checks_running > 0,
	we do not allow the discard. */

	/* Play safe and remove all insert buffer entries, though we should
	have removed them already when DISCARD TABLESPACE was called */

	ibuf_delete_for_discarded_space(table->space);

	table_id_t	new_id;

	/* Set the TABLESPACE DISCARD flag in the table definition on disk. */

	err = row_import_update_discarded_flag(trx, table->id, true, true);

	if (err != DB_SUCCESS) {
		return(err);
	}

	/* Update the index root pages in the system tables, on disk */

	err = row_import_update_index_root(trx, table, true, true);

	if (err != DB_SUCCESS) {
		return(err);
	}

	/* Drop all the FTS auxiliary tables. */
	if (dict_table_has_fts_index(table)
	    || DICT_TF2_FLAG_IS_SET(table, DICT_TF2_FTS_HAS_DOC_ID)) {

		fts_drop_tables(trx, table);
	}

	/* Assign a new space ID to the table definition so that purge
	can ignore the changes. Update the system table on disk. */

	err = row_mysql_table_id_reassign(table, trx, &new_id);

	if (err != DB_SUCCESS) {
		return(err);
	}

	/* Discard the physical file that is used for the tablespace. */

	err = fil_discard_tablespace(table->space);

	switch(err) {
	case DB_SUCCESS:
	case DB_IO_ERROR:
	case DB_TABLESPACE_NOT_FOUND:
		/* All persistent operations successful, update the
		data dictionary memory cache. */

		table->ibd_file_missing = TRUE;

		table->flags2 |= DICT_TF2_DISCARDED;

		dict_table_change_id_in_cache(table, new_id);

		/* Reset the root page numbers. */

		for (dict_index_t* index = UT_LIST_GET_FIRST(table->indexes);
		     index != 0;
		     index = UT_LIST_GET_NEXT(indexes, index)) {

			index->page = FIL_NULL;
			index->space = FIL_NULL;
		}

		/* If the tablespace did not already exist or we couldn't
		write to it, we treat that as a successful DISCARD. It is
		unusable anyway. */

		err = DB_SUCCESS;
		break;

	default:
		/* We need to rollback the disk changes, something failed. */

		trx->error_state = DB_SUCCESS;

		trx_rollback_to_savepoint(trx, NULL);

		trx->error_state = DB_SUCCESS;
	}

	return(err);
}

/*********************************************************************//**
Discards the tablespace of a table which stored in an .ibd file. Discarding
means that this function renames the .ibd file and assigns a new table id for
the table. Also the flag table->ibd_file_missing is set to TRUE.
@return	error code or DB_SUCCESS */
UNIV_INTERN
dberr_t
row_discard_tablespace_for_mysql(
/*=============================*/
	const char*	name,	/*!< in: table name */
	trx_t*		trx)	/*!< in: transaction handle */
{
	dberr_t		err;
	dict_table_t*	table;

	/* Open the table and start the transaction if not started. */

	table = row_discard_tablespace_begin(name, trx);

	if (table == 0) {
		err = DB_TABLE_NOT_FOUND;
	} else if (table->space == TRX_SYS_SPACE) {
		char	table_name[MAX_FULL_NAME_LEN + 1];

		innobase_format_name(
			table_name, sizeof(table_name), table->name, FALSE);

		ib_senderrf(trx->mysql_thd, IB_LOG_LEVEL_ERROR,
			    ER_TABLE_IN_SYSTEM_TABLESPACE, table_name);

		err = DB_ERROR;

	} else if (table->n_foreign_key_checks_running > 0) {
		char	table_name[MAX_FULL_NAME_LEN + 1];

		innobase_format_name(
			table_name, sizeof(table_name), table->name, FALSE);

		ib_senderrf(trx->mysql_thd, IB_LOG_LEVEL_ERROR,
			    ER_DISCARD_FK_CHECKS_RUNNING, table_name);

		err = DB_ERROR;

	} else {
		/* Do foreign key constraint checks. */

		err = row_discard_tablespace_foreign_key_checks(trx, table);

		if (err == DB_SUCCESS) {
			err = row_discard_tablespace(trx, table);
		}
	}

	return(row_discard_tablespace_end(trx, table, err));
}

/*********************************************************************//**
Sets an exclusive lock on a table.
@return	error code or DB_SUCCESS */
UNIV_INTERN
dberr_t
row_mysql_lock_table(
/*=================*/
	trx_t*		trx,		/*!< in/out: transaction */
	dict_table_t*	table,		/*!< in: table to lock */
	enum lock_mode	mode,		/*!< in: LOCK_X or LOCK_S */
	const char*	op_info)	/*!< in: string for trx->op_info */
{
	mem_heap_t*	heap;
	que_thr_t*	thr;
	dberr_t		err;
	sel_node_t*	node;

	ut_ad(trx);
	ut_ad(mode == LOCK_X || mode == LOCK_S);

	heap = mem_heap_create(512);

	trx->op_info = op_info;

	node = sel_node_create(heap);
	thr = pars_complete_graph_for_exec(node, trx, heap);
	thr->graph->state = QUE_FORK_ACTIVE;

	/* We use the select query graph as the dummy graph needed
	in the lock module call */

	thr = que_fork_get_first_thr(
		static_cast<que_fork_t*>(que_node_get_parent(thr)));

	que_thr_move_to_run_state_for_mysql(thr, trx);

run_again:
	thr->run_node = thr;
	thr->prev_node = thr->common.parent;

	err = lock_table(0, table, mode, thr);

	trx->error_state = err;

	if (err == DB_SUCCESS) {
		que_thr_stop_for_mysql_no_error(thr, trx);
	} else {
		que_thr_stop_for_mysql(thr);

		if (err != DB_QUE_THR_SUSPENDED) {
			ibool	was_lock_wait;

			was_lock_wait = row_mysql_handle_errors(
				&err, trx, thr, NULL);

			if (was_lock_wait) {
				goto run_again;
			}
		} else {
			que_thr_t*	run_thr;
			que_node_t*	parent;

			parent = que_node_get_parent(thr);

			run_thr = que_fork_start_command(
				static_cast<que_fork_t*>(parent));

			ut_a(run_thr == thr);

			/* There was a lock wait but the thread was not
			in a ready to run or running state. */
			trx->error_state = DB_LOCK_WAIT;

			goto run_again;
		}
	}

	que_graph_free(thr->graph);
	trx->op_info = "";

	return(err);
}

/*********************************************************************//**
Truncates a table for MySQL.
@return	error code or DB_SUCCESS */
UNIV_INTERN
dberr_t
row_truncate_table_for_mysql(
/*=========================*/
	dict_table_t*	table,	/*!< in: table handle */
	trx_t*		trx)	/*!< in: transaction handle */
{
	dict_foreign_t*	foreign;
	dberr_t		err;
	mem_heap_t*	heap;
	byte*		buf;
	dtuple_t*	tuple;
	dfield_t*	dfield;
	dict_index_t*	sys_index;
	btr_pcur_t	pcur;
	mtr_t		mtr;
	table_id_t	new_id;
	ulint		recreate_space = 0;
	pars_info_t*	info = NULL;
	ibool		has_internal_doc_id;
	ulint		old_space = table->space;

	/* How do we prevent crashes caused by ongoing operations on
	the table? Old operations could try to access non-existent
	pages.

	1) SQL queries, INSERT, SELECT, ...: we must get an exclusive
	InnoDB table lock on the table before we can do TRUNCATE
	TABLE. Then there are no running queries on the table.

	2) Purge and rollback: we assign a new table id for the
	table. Since purge and rollback look for the table based on
	the table id, they see the table as 'dropped' and discard
	their operations.

	3) Insert buffer: TRUNCATE TABLE is analogous to DROP TABLE,
	so we do not have to remove insert buffer records, as the
	insert buffer works at a low level. If a freed page is later
	reallocated, the allocator will remove the ibuf entries for
	it.

	When we truncate *.ibd files by recreating them (analogous to
	DISCARD TABLESPACE), we remove all entries for the table in the
	insert buffer tree.  This is not strictly necessary, because
	in 6) we will assign a new tablespace identifier, but we can
	free up some space in the system tablespace.

	4) Linear readahead and random readahead: we use the same
	method as in 3) to discard ongoing operations. (This is only
	relevant for TRUNCATE TABLE by DISCARD TABLESPACE.)

	5) FOREIGN KEY operations: if
	table->n_foreign_key_checks_running > 0, we do not allow the
	TRUNCATE. We also reserve the data dictionary latch.

	6) Crash recovery: To prevent the application of pre-truncation
	redo log records on the truncated tablespace, we will assign
	a new tablespace identifier to the truncated tablespace. */

	ut_ad(table);

	if (srv_created_new_raw) {
		fputs("InnoDB: A new raw disk partition was initialized:\n"
		      "InnoDB: we do not allow database modifications"
		      " by the user.\n"
		      "InnoDB: Shut down mysqld and edit my.cnf so that newraw"
		      " is replaced with raw.\n", stderr);

		return(DB_ERROR);
	}

	if (dict_table_is_discarded(table)) {
		return(DB_TABLESPACE_DELETED);
	} else if (table->ibd_file_missing) {
		return(DB_TABLESPACE_NOT_FOUND);
	}

	trx_start_for_ddl(trx, TRX_DICT_OP_TABLE);

	trx->op_info = "truncating table";

	/* Serialize data dictionary operations with dictionary mutex:
	no deadlocks can occur then in these operations */

	ut_a(trx->dict_operation_lock_mode == 0);
	/* Prevent foreign key checks etc. while we are truncating the
	table */

	row_mysql_lock_data_dictionary(trx);

	ut_ad(mutex_own(&(dict_sys->mutex)));
#ifdef UNIV_SYNC_DEBUG
	ut_ad(rw_lock_own(&dict_operation_lock, RW_LOCK_EX));
#endif /* UNIV_SYNC_DEBUG */

	dict_stats_wait_bg_to_stop_using_table(table, trx);

	/* Check if the table is referenced by foreign key constraints from
	some other table (not the table itself) */

	for (foreign = UT_LIST_GET_FIRST(table->referenced_list);
	     foreign != 0 && foreign->foreign_table == table;
	     foreign = UT_LIST_GET_NEXT(referenced_list, foreign)) {

		/* Do nothing. */
	}

	if (!srv_read_only_mode
	    && foreign
	    && trx->check_foreigns) {

		FILE*	ef	= dict_foreign_err_file;

		/* We only allow truncating a referenced table if
		FOREIGN_KEY_CHECKS is set to 0 */

		mutex_enter(&dict_foreign_err_mutex);
		rewind(ef);
		ut_print_timestamp(ef);

		fputs("  Cannot truncate table ", ef);
		ut_print_name(ef, trx, TRUE, table->name);
		fputs(" by DROP+CREATE\n"
		      "InnoDB: because it is referenced by ", ef);
		ut_print_name(ef, trx, TRUE, foreign->foreign_table_name);
		putc('\n', ef);
		mutex_exit(&dict_foreign_err_mutex);

		err = DB_ERROR;
		goto funct_exit;
	}

	/* TODO: could we replace the counter n_foreign_key_checks_running
	with lock checks on the table? Acquire here an exclusive lock on the
	table, and rewrite lock0lock.cc and the lock wait in srv0srv.cc so that
	they can cope with the table having been truncated here? Foreign key
	checks take an IS or IX lock on the table. */

	if (table->n_foreign_key_checks_running > 0) {
		ut_print_timestamp(stderr);
		fputs("  InnoDB: Cannot truncate table ", stderr);
		ut_print_name(stderr, trx, TRUE, table->name);
		fputs(" by DROP+CREATE\n"
		      "InnoDB: because there is a foreign key check"
		      " running on it.\n",
		      stderr);
		err = DB_ERROR;

		goto funct_exit;
	}

	/* Remove all locks except the table-level X lock. */

	lock_remove_all_on_table(table, FALSE);

	/* Ensure that the table will be dropped by
	trx_rollback_active() in case of a crash. */

	trx->table_id = table->id;
	trx_set_dict_operation(trx, TRX_DICT_OP_TABLE);

	/* Assign an undo segment for the transaction, so that the
	transaction will be recovered after a crash. */

	mutex_enter(&trx->undo_mutex);

	err = trx_undo_assign_undo(trx, TRX_UNDO_UPDATE);

	mutex_exit(&trx->undo_mutex);

	if (err != DB_SUCCESS) {

		goto funct_exit;
	}

	if (table->space && !table->dir_path_of_temp_table) {
		/* Discard and create the single-table tablespace. */
		ulint	space	= table->space;
		ulint	flags	= fil_space_get_flags(space);

		ut_a(!DICT_TF2_FLAG_IS_SET(table, DICT_TF2_TEMPORARY));

		dict_get_and_save_data_dir_path(table, true);

		if (flags != ULINT_UNDEFINED
		    && fil_discard_tablespace(space) == DB_SUCCESS) {

			dict_index_t*	index;

			dict_hdr_get_new_id(NULL, NULL, &space);

			/* Lock all index trees for this table. We must
			do so after dict_hdr_get_new_id() to preserve
			the latch order */
			dict_table_x_lock_indexes(table);

			if (space == ULINT_UNDEFINED
			    || fil_create_new_single_table_tablespace(
				    space, table->name,
				    table->data_dir_path,
				    flags, table->flags2,
				    FIL_IBD_FILE_INITIAL_SIZE)
			    != DB_SUCCESS) {
				dict_table_x_unlock_indexes(table);

				ib_logf(IB_LOG_LEVEL_ERROR,
					"TRUNCATE TABLE %s failed to "
					"create a new tablespace",
					table->name);

				table->ibd_file_missing = 1;
				err = DB_ERROR;
				goto funct_exit;
			}

			recreate_space = space;

			/* Replace the space_id in the data dictionary cache.
			The persisent data dictionary (SYS_TABLES.SPACE
			and SYS_INDEXES.SPACE) are updated later in this
			function. */
			table->space = space;
			index = dict_table_get_first_index(table);
			do {
				index->space = space;
				index = dict_table_get_next_index(index);
			} while (index);

			mtr_start(&mtr);
			fsp_header_init(space,
					FIL_IBD_FILE_INITIAL_SIZE, &mtr);
			mtr_commit(&mtr);
		}
	} else {
		/* Lock all index trees for this table, as we will
		truncate the table/index and possibly change their metadata.
		All DML/DDL are blocked by table level lock, with
		a few exceptions such as queries into information schema
		about the table, MySQL could try to access index stats
		for this kind of query, we need to use index locks to
		sync up */
		dict_table_x_lock_indexes(table);
	}

	/* scan SYS_INDEXES for all indexes of the table */
	heap = mem_heap_create(800);

	tuple = dtuple_create(heap, 1);
	dfield = dtuple_get_nth_field(tuple, 0);

	buf = static_cast<byte*>(mem_heap_alloc(heap, 8));
	mach_write_to_8(buf, table->id);

	dfield_set_data(dfield, buf, 8);
	sys_index = dict_table_get_first_index(dict_sys->sys_indexes);
	dict_index_copy_types(tuple, sys_index, 1);

	mtr_start(&mtr);
	btr_pcur_open_on_user_rec(sys_index, tuple, PAGE_CUR_GE,
				  BTR_MODIFY_LEAF, &pcur, &mtr);
	for (;;) {
		rec_t*		rec;
		const byte*	field;
		ulint		len;
		ulint		root_page_no;

		if (!btr_pcur_is_on_user_rec(&pcur)) {
			/* The end of SYS_INDEXES has been reached. */
			break;
		}

		rec = btr_pcur_get_rec(&pcur);

		field = rec_get_nth_field_old(
			rec, DICT_FLD__SYS_INDEXES__TABLE_ID, &len);
		ut_ad(len == 8);

		if (memcmp(buf, field, len) != 0) {
			/* End of indexes for the table (TABLE_ID mismatch). */
			break;
		}

		if (rec_get_deleted_flag(rec, FALSE)) {
			/* The index has been dropped. */
			goto next_rec;
		}

		/* This call may commit and restart mtr
		and reposition pcur. */
		root_page_no = dict_truncate_index_tree(table, recreate_space,
							&pcur, &mtr);

		rec = btr_pcur_get_rec(&pcur);

		if (root_page_no != FIL_NULL) {
			page_rec_write_field(
				rec, DICT_FLD__SYS_INDEXES__PAGE_NO,
				root_page_no, &mtr);
			/* We will need to commit and restart the
			mini-transaction in order to avoid deadlocks.
			The dict_truncate_index_tree() call has allocated
			a page in this mini-transaction, and the rest of
			this loop could latch another index page. */
			mtr_commit(&mtr);
			mtr_start(&mtr);
			btr_pcur_restore_position(BTR_MODIFY_LEAF,
						  &pcur, &mtr);
		}

next_rec:
		btr_pcur_move_to_next_user_rec(&pcur, &mtr);
	}

	btr_pcur_close(&pcur);
	mtr_commit(&mtr);

	mem_heap_free(heap);
	/* Done with index truncation, release index tree locks,
	subsequent work relates to table level metadata change */
	dict_table_x_unlock_indexes(table);

	dict_hdr_get_new_id(&new_id, NULL, NULL);

	/* Create new FTS auxiliary tables with the new_id, and
	drop the old index later, only if everything runs successful. */
	has_internal_doc_id = dict_table_has_fts_index(table)
			      || DICT_TF2_FLAG_IS_SET(
				table, DICT_TF2_FTS_HAS_DOC_ID);
	if (has_internal_doc_id) {
		dict_table_t	fts_table;
		ulint		i;

		fts_table.name = table->name;
		fts_table.id = new_id;

		err = fts_create_common_tables(
			trx, &fts_table, table->name, TRUE);

		for (i = 0;
		     i < ib_vector_size(table->fts->indexes)
		     && err == DB_SUCCESS;
		     i++) {

			dict_index_t*	fts_index;

			fts_index = static_cast<dict_index_t*>(
				ib_vector_getp(table->fts->indexes, i));

			err = fts_create_index_tables_low(
				trx, fts_index, table->name, new_id);
		}

		if (err != DB_SUCCESS) {
			trx->error_state = DB_SUCCESS;
			trx_rollback_to_savepoint(trx, NULL);
			trx->error_state = DB_SUCCESS;
			ut_print_timestamp(stderr);
			fputs("  InnoDB: Unable to truncate FTS index for"
			      " table", stderr);
			ut_print_name(stderr, trx, TRUE, table->name);
			fputs("\n", stderr);

			goto funct_exit;
		} else {
			ut_ad(trx->state != TRX_STATE_NOT_STARTED);
		}
	}

	info = pars_info_create();

	pars_info_add_int4_literal(info, "new_space", (lint) table->space);
	pars_info_add_ull_literal(info, "old_id", table->id);
	pars_info_add_ull_literal(info, "new_id", new_id);

	err = que_eval_sql(info,
			   "PROCEDURE RENUMBER_TABLE_ID_PROC () IS\n"
			   "BEGIN\n"
			   "UPDATE SYS_TABLES"
			   " SET ID = :new_id, SPACE = :new_space\n"
			   " WHERE ID = :old_id;\n"
			   "UPDATE SYS_COLUMNS SET TABLE_ID = :new_id\n"
			   " WHERE TABLE_ID = :old_id;\n"
			   "UPDATE SYS_INDEXES"
			   " SET TABLE_ID = :new_id, SPACE = :new_space\n"
			   " WHERE TABLE_ID = :old_id;\n"
			   "END;\n"
			   , FALSE, trx);

	if (err == DB_SUCCESS && old_space != table->space) {
		info = pars_info_create();

		pars_info_add_int4_literal(info, "old_space", (lint) old_space);

		pars_info_add_int4_literal(
			info, "new_space", (lint) table->space);

		err = que_eval_sql(info,
				   "PROCEDURE RENUMBER_TABLESPACE_PROC () IS\n"
				   "BEGIN\n"
				   "UPDATE SYS_TABLESPACES"
				   " SET SPACE = :new_space\n"
				   " WHERE SPACE = :old_space;\n"
				   "UPDATE SYS_DATAFILES"
				   " SET SPACE = :new_space"
				   " WHERE SPACE = :old_space;\n"
				   "END;\n"
				   , FALSE, trx);
	}
	DBUG_EXECUTE_IF("ib_ddl_crash_before_fts_truncate", err = DB_ERROR;);

	if (err != DB_SUCCESS) {
		trx->error_state = DB_SUCCESS;
		trx_rollback_to_savepoint(trx, NULL);
		trx->error_state = DB_SUCCESS;

		/* Update system table failed.  Table in memory metadata
		could be in an inconsistent state, mark the in-memory
		table->corrupted to be true. In the long run, this should
		be fixed by atomic truncate table */
		table->corrupted = true;

		ut_print_timestamp(stderr);
		fputs("  InnoDB: Unable to assign a new identifier to table ",
		      stderr);
		ut_print_name(stderr, trx, TRUE, table->name);
		fputs("\n"
		      "InnoDB: after truncating it.  Background processes"
		      " may corrupt the table!\n", stderr);

		/* Failed to update the table id, so drop the new
		FTS auxiliary tables */
		if (has_internal_doc_id) {
			ut_ad(trx->state == TRX_STATE_NOT_STARTED);

			table_id_t	id = table->id;

			table->id = new_id;

			fts_drop_tables(trx, table);

			table->id = id;

			ut_ad(trx->state != TRX_STATE_NOT_STARTED);
		}

		err = DB_ERROR;
	} else {
		/* Drop the old FTS index */
		if (has_internal_doc_id) {
			ut_ad(trx->state != TRX_STATE_NOT_STARTED);
			fts_drop_tables(trx, table);
			ut_ad(trx->state != TRX_STATE_NOT_STARTED);
		}

		DBUG_EXECUTE_IF("ib_truncate_crash_after_fts_drop",
				DBUG_SUICIDE(););

		dict_table_change_id_in_cache(table, new_id);

		/* Reset the Doc ID in cache to 0 */
		if (has_internal_doc_id && table->fts->cache) {
			table->fts->fts_status |= TABLE_DICT_LOCKED;
			fts_update_next_doc_id(trx, table, NULL, 0);
			fts_cache_clear(table->fts->cache);
			fts_cache_init(table->fts->cache);
			table->fts->fts_status &= ~TABLE_DICT_LOCKED;
		}
	}

	/* Reset auto-increment. */
	dict_table_autoinc_lock(table);
	dict_table_autoinc_initialize(table, 1);
	dict_table_autoinc_unlock(table);

	trx_commit_for_mysql(trx);

funct_exit:

	row_mysql_unlock_data_dictionary(trx);

	dict_stats_update(table, DICT_STATS_EMPTY_TABLE);

	trx->op_info = "";

	srv_wake_master_thread();

	return(err);
}

/*********************************************************************//**
Drops a table for MySQL.  If the name of the dropped table ends in
one of "innodb_monitor", "innodb_lock_monitor", "innodb_tablespace_monitor",
"innodb_table_monitor", then this will also stop the printing of monitor
output by the master thread.  If the data dictionary was not already locked
by the transaction, the transaction will be committed.  Otherwise, the
data dictionary will remain locked.
@return	error code or DB_SUCCESS */
UNIV_INTERN
dberr_t
row_drop_table_for_mysql(
/*=====================*/
	const char*	name,	/*!< in: table name */
	trx_t*		trx,	/*!< in: transaction handle */
	bool		drop_db,/*!< in: true=dropping whole database */
	bool		nonatomic)
				/*!< in: whether it is permitted
				to release and reacquire dict_operation_lock */
{
	dberr_t		err;
	dict_foreign_t*	foreign;
	dict_table_t*	table;
	ibool		print_msg;
	ulint		space_id;
	char*		filepath = NULL;
	const char*	tablename_minus_db;
	char*		tablename =  NULL;
	bool		ibd_file_missing;
	ulint		namelen;
	bool		locked_dictionary	= false;
	pars_info_t*	info			= NULL;
	mem_heap_t*	heap			= NULL;

	ut_a(name != NULL);

	if (srv_created_new_raw) {
		fputs("InnoDB: A new raw disk partition was initialized:\n"
		      "InnoDB: we do not allow database modifications"
		      " by the user.\n"
		      "InnoDB: Shut down mysqld and edit my.cnf so that newraw"
		      " is replaced with raw.\n", stderr);

		return(DB_ERROR);
	}

	/* The table name is prefixed with the database name and a '/'.
	Certain table names starting with 'innodb_' have their special
	meaning regardless of the database name.  Thus, we need to
	ignore the database name prefix in the comparisons. */
	tablename_minus_db = strchr(name, '/');

	if (tablename_minus_db) {
		tablename_minus_db++;
	} else {
		/* Ancillary FTS tables don't have '/' characters. */
		tablename_minus_db = name;
	}

	namelen = strlen(tablename_minus_db) + 1;

	if (namelen == sizeof S_innodb_monitor
	    && !memcmp(tablename_minus_db, S_innodb_monitor,
		       sizeof S_innodb_monitor)) {

		/* Table name equals "innodb_monitor":
		stop monitor prints */

		srv_print_innodb_monitor = FALSE;
		srv_print_innodb_lock_monitor = FALSE;
	} else if (namelen == sizeof S_innodb_lock_monitor
		   && !memcmp(tablename_minus_db, S_innodb_lock_monitor,
			      sizeof S_innodb_lock_monitor)) {
		srv_print_innodb_monitor = FALSE;
		srv_print_innodb_lock_monitor = FALSE;
	} else if (namelen == sizeof S_innodb_tablespace_monitor
		   && !memcmp(tablename_minus_db, S_innodb_tablespace_monitor,
			      sizeof S_innodb_tablespace_monitor)) {

		srv_print_innodb_tablespace_monitor = FALSE;
	} else if (namelen == sizeof S_innodb_table_monitor
		   && !memcmp(tablename_minus_db, S_innodb_table_monitor,
			      sizeof S_innodb_table_monitor)) {

		srv_print_innodb_table_monitor = FALSE;
	}

	/* Serialize data dictionary operations with dictionary mutex:
	no deadlocks can occur then in these operations */

	trx->op_info = "dropping table";

	/* This function is called recursively via fts_drop_tables(). */
	if (trx->state == TRX_STATE_NOT_STARTED) {
		trx_start_for_ddl(trx, TRX_DICT_OP_TABLE);
	}

	if (trx->dict_operation_lock_mode != RW_X_LATCH) {
		/* Prevent foreign key checks etc. while we are dropping the
		table */

		row_mysql_lock_data_dictionary(trx);

		locked_dictionary = true;
		nonatomic = true;
	}

	ut_ad(mutex_own(&(dict_sys->mutex)));
#ifdef UNIV_SYNC_DEBUG
	ut_ad(rw_lock_own(&dict_operation_lock, RW_LOCK_EX));
#endif /* UNIV_SYNC_DEBUG */

	table = dict_table_open_on_name(
		name, TRUE, FALSE,
		static_cast<dict_err_ignore_t>(
			DICT_ERR_IGNORE_INDEX_ROOT | DICT_ERR_IGNORE_CORRUPT));

	if (!table) {
		err = DB_TABLE_NOT_FOUND;
		ut_print_timestamp(stderr);

		fputs("  InnoDB: Error: table ", stderr);
		ut_print_name(stderr, trx, TRUE, name);
		fputs(" does not exist in the InnoDB internal\n"
		      "InnoDB: data dictionary though MySQL is"
		      " trying to drop it.\n"
		      "InnoDB: Have you copied the .frm file"
		      " of the table to the\n"
		      "InnoDB: MySQL database directory"
		      " from another database?\n"
		      "InnoDB: You can look for further help from\n"
		      "InnoDB: " REFMAN "innodb-troubleshooting.html\n",
		      stderr);
		goto funct_exit;
	}

	/* Turn on this drop bit before we could release the dictionary
	latch */
	table->to_be_dropped = true;

	if (nonatomic) {
		/* This trx did not acquire any locks on dictionary
		table records yet. Thus it is safe to release and
		reacquire the data dictionary latches. */
		if (table->fts) {
			ut_ad(!table->fts->add_wq);
			ut_ad(lock_trx_has_sys_table_locks(trx) == 0);

			row_mysql_unlock_data_dictionary(trx);
			fts_optimize_remove_table(table);
			row_mysql_lock_data_dictionary(trx);
		}

		/* Do not bother to deal with persistent stats for temp
		tables since we know temp tables do not use persistent
		stats. */
		if (!dict_table_is_temporary(table)) {
			dict_stats_wait_bg_to_stop_using_table(
				table, trx);
		}
	}

	/* make sure background stats thread is not running on the table */
	ut_ad(!(table->stats_bg_flag & BG_STAT_IN_PROGRESS));

	/* Delete the link file if used. */
	if (DICT_TF_HAS_DATA_DIR(table->flags)) {
		fil_delete_link_file(name);
	}

	if (!dict_table_is_temporary(table)) {

		dict_stats_recalc_pool_del(table);

		/* Remove stats for this table and all of its indexes from the
		persistent storage if it exists and if there are stats for this
		table in there. This function creates its own trx and commits
		it. */
		char	errstr[1024];
		err = dict_stats_drop_table(name, errstr, sizeof(errstr));

		if (err != DB_SUCCESS) {
			ib_logf(IB_LOG_LEVEL_WARN, "%s", errstr);
		}
	}

	/* Move the table the the non-LRU list so that it isn't
	considered for eviction. */

	if (table->can_be_evicted) {
		dict_table_move_from_lru_to_non_lru(table);
	}

	dict_table_close(table, TRUE, FALSE);

	/* Check if the table is referenced by foreign key constraints from
	some other table (not the table itself) */

	foreign = UT_LIST_GET_FIRST(table->referenced_list);

	while (foreign && foreign->foreign_table == table) {
check_next_foreign:
		foreign = UT_LIST_GET_NEXT(referenced_list, foreign);
	}

	if (!srv_read_only_mode
	    && foreign
	    && trx->check_foreigns
	    && !(drop_db && dict_tables_have_same_db(
			 name, foreign->foreign_table_name_lookup))) {
		FILE*	ef	= dict_foreign_err_file;

		/* We only allow dropping a referenced table if
		FOREIGN_KEY_CHECKS is set to 0 */

		err = DB_CANNOT_DROP_CONSTRAINT;

		mutex_enter(&dict_foreign_err_mutex);
		rewind(ef);
		ut_print_timestamp(ef);

		fputs("  Cannot drop table ", ef);
		ut_print_name(ef, trx, TRUE, name);
		fputs("\n"
		      "because it is referenced by ", ef);
		ut_print_name(ef, trx, TRUE, foreign->foreign_table_name);
		putc('\n', ef);
		mutex_exit(&dict_foreign_err_mutex);

		goto funct_exit;
	}

	if (foreign && trx->check_foreigns) {
		goto check_next_foreign;
	}

	/* TODO: could we replace the counter n_foreign_key_checks_running
	with lock checks on the table? Acquire here an exclusive lock on the
	table, and rewrite lock0lock.cc and the lock wait in srv0srv.cc so that
	they can cope with the table having been dropped here? Foreign key
	checks take an IS or IX lock on the table. */

	if (table->n_foreign_key_checks_running > 0) {

		const char*	save_tablename = table->name;
		ibool		added;

		added = row_add_table_to_background_drop_list(save_tablename);

		if (added) {
			ut_print_timestamp(stderr);
			fputs("  InnoDB: You are trying to drop table ",
			      stderr);
			ut_print_name(stderr, trx, TRUE, save_tablename);
			fputs("\n"
			      "InnoDB: though there is a"
			      " foreign key check running on it.\n"
			      "InnoDB: Adding the table to"
			      " the background drop queue.\n",
			      stderr);

			/* We return DB_SUCCESS to MySQL though the drop will
			happen lazily later */

			err = DB_SUCCESS;
		} else {
			/* The table is already in the background drop list */
			err = DB_ERROR;
		}

		goto funct_exit;
	}

	/* Remove all locks that are on the table or its records, if there
	are no refernces to the table but it has record locks, we release
	the record locks unconditionally. One use case is:

		CREATE TABLE t2 (PRIMARY KEY (a)) SELECT * FROM t1;

	If after the user transaction has done the SELECT and there is a
	problem in completing the CREATE TABLE operation, MySQL will drop
	the table. InnoDB will create a new background transaction to do the
	actual drop, the trx instance that is passed to this function. To
	preserve existing behaviour we remove the locks but ideally we
	shouldn't have to. There should never be record locks on a table
	that is going to be dropped. */

	if (table->n_ref_count == 0) {
		lock_remove_all_on_table(table, TRUE);
		ut_a(table->n_rec_locks == 0);
	} else if (table->n_ref_count > 0 || table->n_rec_locks > 0) {
		ibool	added;

		added = row_add_table_to_background_drop_list(table->name);

		if (added) {
			ut_print_timestamp(stderr);
			fputs("  InnoDB: Warning: MySQL is"
			      " trying to drop table ", stderr);
			ut_print_name(stderr, trx, TRUE, table->name);
			fputs("\n"
			      "InnoDB: though there are still"
			      " open handles to it.\n"
			      "InnoDB: Adding the table to the"
			      " background drop queue.\n",
			      stderr);

			/* We return DB_SUCCESS to MySQL though the drop will
			happen lazily later */
			err = DB_SUCCESS;
		} else {
			/* The table is already in the background drop list */
			err = DB_ERROR;
		}

		goto funct_exit;
	}

	/* The "to_be_dropped" marks table that is to be dropped, but
	has not been dropped, instead, was put in the background drop
	list due to being used by concurrent DML operations. Clear it
	here since there are no longer any concurrent activities on it,
	and it is free to be dropped */
	table->to_be_dropped = false;

	/* If we get this far then the table to be dropped must not have
	any table or record locks on it. */

	ut_a(!lock_table_has_locks(table));

	switch (trx_get_dict_operation(trx)) {
	case TRX_DICT_OP_NONE:
		trx_set_dict_operation(trx, TRX_DICT_OP_TABLE);
		trx->table_id = table->id;
	case TRX_DICT_OP_TABLE:
		break;
	case TRX_DICT_OP_INDEX:
		/* If the transaction was previously flagged as
		TRX_DICT_OP_INDEX, we should be dropping auxiliary
		tables for full-text indexes. */
		ut_ad(strstr(table->name, "/FTS_") != NULL);
	}

	/* Mark all indexes unavailable in the data dictionary cache
	before starting to drop the table. */

	unsigned*	page_no;
	unsigned*	page_nos;
	heap = mem_heap_create(
		200 + UT_LIST_GET_LEN(table->indexes) * sizeof *page_nos);
	tablename = mem_heap_strdup(heap, name);

	page_no = page_nos = static_cast<unsigned*>(
		mem_heap_alloc(
			heap,
			UT_LIST_GET_LEN(table->indexes) * sizeof *page_no));

	for (dict_index_t* index = dict_table_get_first_index(table);
	     index != NULL;
	     index = dict_table_get_next_index(index)) {
		rw_lock_x_lock(dict_index_get_lock(index));
		/* Save the page numbers so that we can restore them
		if the operation fails. */
		*page_no++ = index->page;
		/* Mark the index unusable. */
		index->page = FIL_NULL;
		rw_lock_x_unlock(dict_index_get_lock(index));
	}

	/* We use the private SQL parser of Innobase to generate the
	query graphs needed in deleting the dictionary data from system
	tables in Innobase. Deleting a row from SYS_INDEXES table also
	frees the file segments of the B-tree associated with the index. */

	info = pars_info_create();

	pars_info_add_str_literal(info, "table_name", name);

	err = que_eval_sql(info,
			   "PROCEDURE DROP_TABLE_PROC () IS\n"
			   "sys_foreign_id CHAR;\n"
			   "table_id CHAR;\n"
			   "index_id CHAR;\n"
			   "foreign_id CHAR;\n"
			   "space_id INT;\n"
			   "found INT;\n"

			   "DECLARE CURSOR cur_fk IS\n"
			   "SELECT ID FROM SYS_FOREIGN\n"
			   "WHERE FOR_NAME = :table_name\n"
			   "AND TO_BINARY(FOR_NAME)\n"
			   "  = TO_BINARY(:table_name)\n"
			   "LOCK IN SHARE MODE;\n"

			   "DECLARE CURSOR cur_idx IS\n"
			   "SELECT ID FROM SYS_INDEXES\n"
			   "WHERE TABLE_ID = table_id\n"
			   "LOCK IN SHARE MODE;\n"

			   "BEGIN\n"
			   "SELECT ID INTO table_id\n"
			   "FROM SYS_TABLES\n"
			   "WHERE NAME = :table_name\n"
			   "LOCK IN SHARE MODE;\n"
			   "IF (SQL % NOTFOUND) THEN\n"
			   "       RETURN;\n"
			   "END IF;\n"
			   "SELECT SPACE INTO space_id\n"
			   "FROM SYS_TABLES\n"
			   "WHERE NAME = :table_name;\n"
			   "IF (SQL % NOTFOUND) THEN\n"
			   "       RETURN;\n"
			   "END IF;\n"
			   "found := 1;\n"
			   "SELECT ID INTO sys_foreign_id\n"
			   "FROM SYS_TABLES\n"
			   "WHERE NAME = 'SYS_FOREIGN'\n"
			   "LOCK IN SHARE MODE;\n"
			   "IF (SQL % NOTFOUND) THEN\n"
			   "       found := 0;\n"
			   "END IF;\n"
			   "IF (:table_name = 'SYS_FOREIGN') THEN\n"
			   "       found := 0;\n"
			   "END IF;\n"
			   "IF (:table_name = 'SYS_FOREIGN_COLS') THEN\n"
			   "       found := 0;\n"
			   "END IF;\n"
			   "OPEN cur_fk;\n"
			   "WHILE found = 1 LOOP\n"
			   "       FETCH cur_fk INTO foreign_id;\n"
			   "       IF (SQL % NOTFOUND) THEN\n"
			   "               found := 0;\n"
			   "       ELSE\n"
			   "               DELETE FROM SYS_FOREIGN_COLS\n"
			   "               WHERE ID = foreign_id;\n"
			   "               DELETE FROM SYS_FOREIGN\n"
			   "               WHERE ID = foreign_id;\n"
			   "       END IF;\n"
			   "END LOOP;\n"
			   "CLOSE cur_fk;\n"
			   "found := 1;\n"
			   "OPEN cur_idx;\n"
			   "WHILE found = 1 LOOP\n"
			   "       FETCH cur_idx INTO index_id;\n"
			   "       IF (SQL % NOTFOUND) THEN\n"
			   "               found := 0;\n"
			   "       ELSE\n"
			   "               DELETE FROM SYS_FIELDS\n"
			   "               WHERE INDEX_ID = index_id;\n"
			   "               DELETE FROM SYS_INDEXES\n"
			   "               WHERE ID = index_id\n"
			   "               AND TABLE_ID = table_id;\n"
			   "       END IF;\n"
			   "END LOOP;\n"
			   "CLOSE cur_idx;\n"
			   "DELETE FROM SYS_TABLESPACES\n"
			   "WHERE SPACE = space_id;\n"
			   "DELETE FROM SYS_DATAFILES\n"
			   "WHERE SPACE = space_id;\n"
			   "DELETE FROM SYS_COLUMNS\n"
			   "WHERE TABLE_ID = table_id;\n"
			   "DELETE FROM SYS_TABLES\n"
			   "WHERE NAME = :table_name;\n"
			   "END;\n"
			   , FALSE, trx);

	switch (err) {
		ibool	is_temp;

	case DB_SUCCESS:
		/* Clone the name, in case it has been allocated
		from table->heap, which will be freed by
		dict_table_remove_from_cache(table) below. */
		space_id = table->space;
		ibd_file_missing = table->ibd_file_missing;

		is_temp = DICT_TF2_FLAG_IS_SET(table, DICT_TF2_TEMPORARY);

		/* If there is a temp path then the temp flag is set.
		However, during recovery, we might have a temp flag but
		not know the temp path */
		ut_a(table->dir_path_of_temp_table == NULL || is_temp);
		if (dict_table_is_discarded(table)
		    || table->ibd_file_missing) {
			/* Do not attempt to drop known-to-be-missing
			tablespaces. */
			space_id = 0;
		}

		/* We do not allow temporary tables with a remote path. */
		ut_a(!(is_temp && DICT_TF_HAS_DATA_DIR(table->flags)));

		if (space_id && DICT_TF_HAS_DATA_DIR(table->flags)) {
			dict_get_and_save_data_dir_path(table, true);
			ut_a(table->data_dir_path);

			filepath = os_file_make_remote_pathname(
				table->data_dir_path, table->name, "ibd");
		} else if (table->dir_path_of_temp_table) {
			filepath = fil_make_ibd_name(
				table->dir_path_of_temp_table, true);
		} else {
			filepath = fil_make_ibd_name(tablename, false);
		}

		if (dict_table_has_fts_index(table)
		    || DICT_TF2_FLAG_IS_SET(table, DICT_TF2_FTS_HAS_DOC_ID)) {
			ut_ad(table->n_ref_count == 0);
			ut_ad(trx->state != TRX_STATE_NOT_STARTED);
			err = fts_drop_tables(trx, table);

			if (err != DB_SUCCESS) {
				ut_print_timestamp(stderr);
				fprintf(stderr," InnoDB: Error: (%s) not "
					"able to remove ancillary FTS tables "
					"for table ", ut_strerr(err));
				ut_print_name(stderr, trx, TRUE, tablename);
				fputs("\n", stderr);

				goto funct_exit;
			}
		}

		/* The table->fts flag can be set on the table for which
		the cluster index is being rebuilt. Such table might not have
		DICT_TF2_FTS flag set. So keep this out of above
		dict_table_has_fts_index condition */
		if (table->fts) {
			/* Need to set TABLE_DICT_LOCKED bit, since
			fts_que_graph_free_check_lock would try to acquire
			dict mutex lock */
			table->fts->fts_status |= TABLE_DICT_LOCKED;

			fts_free(table);
		}

		dict_table_remove_from_cache(table);

		if (dict_load_table(tablename, TRUE,
				    DICT_ERR_IGNORE_NONE) != NULL) {
			ut_print_timestamp(stderr);
			fputs("  InnoDB: Error: not able to remove table ",
			      stderr);
			ut_print_name(stderr, trx, TRUE, tablename);
			fputs(" from the dictionary cache!\n", stderr);
			err = DB_ERROR;
		}

		/* Do not drop possible .ibd tablespace if something went
		wrong: we do not want to delete valuable data of the user */

		/* Don't spam the log if we can't find the tablespace of
		a temp table or if the tablesace has been discarded. */
		print_msg = !(is_temp || ibd_file_missing);

		if (err == DB_SUCCESS && space_id > TRX_SYS_SPACE) {
			if (!is_temp
			    && !fil_space_for_table_exists_in_mem(
					space_id, tablename, FALSE,
					print_msg, false, NULL, 0)) {
				/* This might happen if we are dropping a
				discarded tablespace */
				err = DB_SUCCESS;

				if (print_msg) {
					char msg_tablename[MAX_FULL_NAME_LEN + 1];

					innobase_format_name(
						msg_tablename, sizeof(tablename),
						tablename, FALSE);

					ib_logf(IB_LOG_LEVEL_INFO,
						"Removed the table %s from "
						"InnoDB's data dictionary",
						msg_tablename);
				}

				/* Force a delete of any discarded
				or temporary files. */

				fil_delete_file(filepath);

			} else if (fil_delete_tablespace(
					space_id,
					BUF_REMOVE_FLUSH_NO_WRITE)
				   != DB_SUCCESS) {
				fprintf(stderr,
					"InnoDB: We removed now the InnoDB"
					" internal data dictionary entry\n"
					"InnoDB: of table ");
				ut_print_name(stderr, trx, TRUE, tablename);
				fprintf(stderr, ".\n");

				ut_print_timestamp(stderr);
				fprintf(stderr,
					"  InnoDB: Error: not able to"
					" delete tablespace %lu of table ",
					(ulong) space_id);
				ut_print_name(stderr, trx, TRUE, tablename);
				fputs("!\n", stderr);
				err = DB_ERROR;
			}
		}

		break;

	case DB_OUT_OF_FILE_SPACE:
		err = DB_MUST_GET_MORE_FILE_SPACE;

		row_mysql_handle_errors(&err, trx, NULL, NULL);

		/* raise error */
		ut_error;
		break;

	case DB_TOO_MANY_CONCURRENT_TRXS:
		/* Cannot even find a free slot for the
		the undo log. We can directly exit here
		and return the DB_TOO_MANY_CONCURRENT_TRXS
		error. */

	default:
		/* This is some error we do not expect. Print
		the error number and rollback transaction */
		ut_print_timestamp(stderr);

		fprintf(stderr, "InnoDB: unknown error code %lu"
			" while dropping table:", (ulong) err);
		ut_print_name(stderr, trx, TRUE, tablename);
		fprintf(stderr, ".\n");

		trx->error_state = DB_SUCCESS;
		trx_rollback_to_savepoint(trx, NULL);
		trx->error_state = DB_SUCCESS;

		/* Mark all indexes available in the data dictionary
		cache again. */

		page_no = page_nos;

		for (dict_index_t* index = dict_table_get_first_index(table);
		     index != NULL;
		     index = dict_table_get_next_index(index)) {
			rw_lock_x_lock(dict_index_get_lock(index));
			ut_a(index->page == FIL_NULL);
			index->page = *page_no++;
			rw_lock_x_unlock(dict_index_get_lock(index));
		}
	}

funct_exit:
	if (heap) {
		mem_heap_free(heap);
	}
	if (filepath) {
		mem_free(filepath);
	}

	if (locked_dictionary) {
		trx_commit_for_mysql(trx);

		row_mysql_unlock_data_dictionary(trx);
	}

	trx->op_info = "";

	srv_wake_master_thread();

	return(err);
}

/*********************************************************************//**
Drop all temporary tables during crash recovery. */
UNIV_INTERN
void
row_mysql_drop_temp_tables(void)
/*============================*/
{
	trx_t*		trx;
	btr_pcur_t	pcur;
	mtr_t		mtr;
	mem_heap_t*	heap;

	trx = trx_allocate_for_background();
	trx->op_info = "dropping temporary tables";
	row_mysql_lock_data_dictionary(trx);

	heap = mem_heap_create(200);

	mtr_start(&mtr);

	btr_pcur_open_at_index_side(
		true,
		dict_table_get_first_index(dict_sys->sys_tables),
		BTR_SEARCH_LEAF, &pcur, true, 0, &mtr);

	for (;;) {
		const rec_t*	rec;
		const byte*	field;
		ulint		len;
		const char*	table_name;
		dict_table_t*	table;

		btr_pcur_move_to_next_user_rec(&pcur, &mtr);

		if (!btr_pcur_is_on_user_rec(&pcur)) {
			break;
		}

		/* The high order bit of N_COLS is set unless
		ROW_FORMAT=REDUNDANT. */
		rec = btr_pcur_get_rec(&pcur);
		field = rec_get_nth_field_old(
			rec, DICT_FLD__SYS_TABLES__NAME, &len);
		field = rec_get_nth_field_old(
			rec, DICT_FLD__SYS_TABLES__N_COLS, &len);
		if (len != 4
		    || !(mach_read_from_4(field) & DICT_N_COLS_COMPACT)) {
			continue;
		}

		/* Older versions of InnoDB, which only supported tables
		in ROW_FORMAT=REDUNDANT could write garbage to
		SYS_TABLES.MIX_LEN, where we now store the is_temp flag.
		Above, we assumed is_temp=0 if ROW_FORMAT=REDUNDANT. */
		field = rec_get_nth_field_old(
			rec, DICT_FLD__SYS_TABLES__MIX_LEN, &len);
		if (len != 4
		    || !(mach_read_from_4(field) & DICT_TF2_TEMPORARY)) {
			continue;
		}

		/* This is a temporary table. */
		field = rec_get_nth_field_old(
			rec, DICT_FLD__SYS_TABLES__NAME, &len);
		if (len == UNIV_SQL_NULL || len == 0) {
			/* Corrupted SYS_TABLES.NAME */
			continue;
		}

		table_name = mem_heap_strdupl(heap, (const char*) field, len);

		btr_pcur_store_position(&pcur, &mtr);
		btr_pcur_commit_specify_mtr(&pcur, &mtr);

		table = dict_load_table(table_name, TRUE, DICT_ERR_IGNORE_NONE);

		if (table) {
			row_drop_table_for_mysql(table_name, trx, FALSE);
			trx_commit_for_mysql(trx);
		}

		mtr_start(&mtr);
		btr_pcur_restore_position(BTR_SEARCH_LEAF,
					  &pcur, &mtr);
	}

	btr_pcur_close(&pcur);
	mtr_commit(&mtr);
	mem_heap_free(heap);
	row_mysql_unlock_data_dictionary(trx);
	trx_free_for_background(trx);
}

/*******************************************************************//**
Drop all foreign keys in a database, see Bug#18942.
Called at the end of row_drop_database_for_mysql().
@return	error code or DB_SUCCESS */
static __attribute__((nonnull, warn_unused_result))
dberr_t
drop_all_foreign_keys_in_db(
/*========================*/
	const char*	name,	/*!< in: database name which ends to '/' */
	trx_t*		trx)	/*!< in: transaction handle */
{
	pars_info_t*	pinfo;
	dberr_t		err;

	ut_a(name[strlen(name) - 1] == '/');

	pinfo = pars_info_create();

	pars_info_add_str_literal(pinfo, "dbname", name);

/** true if for_name is not prefixed with dbname */
#define TABLE_NOT_IN_THIS_DB \
"SUBSTR(for_name, 0, LENGTH(:dbname)) <> :dbname"

	err = que_eval_sql(pinfo,
			   "PROCEDURE DROP_ALL_FOREIGN_KEYS_PROC () IS\n"
			   "foreign_id CHAR;\n"
			   "for_name CHAR;\n"
			   "found INT;\n"
			   "DECLARE CURSOR cur IS\n"
			   "SELECT ID, FOR_NAME FROM SYS_FOREIGN\n"
			   "WHERE FOR_NAME >= :dbname\n"
			   "LOCK IN SHARE MODE\n"
			   "ORDER BY FOR_NAME;\n"
			   "BEGIN\n"
			   "found := 1;\n"
			   "OPEN cur;\n"
			   "WHILE found = 1 LOOP\n"
			   "        FETCH cur INTO foreign_id, for_name;\n"
			   "        IF (SQL % NOTFOUND) THEN\n"
			   "                found := 0;\n"
			   "        ELSIF (" TABLE_NOT_IN_THIS_DB ") THEN\n"
			   "                found := 0;\n"
			   "        ELSIF (1=1) THEN\n"
			   "                DELETE FROM SYS_FOREIGN_COLS\n"
			   "                WHERE ID = foreign_id;\n"
			   "                DELETE FROM SYS_FOREIGN\n"
			   "                WHERE ID = foreign_id;\n"
			   "        END IF;\n"
			   "END LOOP;\n"
			   "CLOSE cur;\n"
			   "COMMIT WORK;\n"
			   "END;\n",
			   FALSE, /* do not reserve dict mutex,
				  we are already holding it */
			   trx);

	return(err);
}

/*********************************************************************//**
Drops a database for MySQL.
@return	error code or DB_SUCCESS */
UNIV_INTERN
dberr_t
row_drop_database_for_mysql(
/*========================*/
	const char*	name,	/*!< in: database name which ends to '/' */
	trx_t*		trx)	/*!< in: transaction handle */
{
	dict_table_t*	table;
	char*		table_name;
	dberr_t		err	= DB_SUCCESS;
	ulint		namelen	= strlen(name);

	ut_a(name != NULL);
	ut_a(name[namelen - 1] == '/');

	trx->op_info = "dropping database";

	trx_set_dict_operation(trx, TRX_DICT_OP_TABLE);

	trx_start_if_not_started_xa(trx);
loop:
	row_mysql_lock_data_dictionary(trx);

	while ((table_name = dict_get_first_table_name_in_db(name))) {
		ut_a(memcmp(table_name, name, namelen) == 0);

		table = dict_table_open_on_name(
			table_name, TRUE, FALSE, static_cast<dict_err_ignore_t>(
				DICT_ERR_IGNORE_INDEX_ROOT
				| DICT_ERR_IGNORE_CORRUPT));

		if (!table) {
			ib_logf(IB_LOG_LEVEL_ERROR,
				"Cannot load table %s from InnoDB internal "
				"data dictionary during drop database",
				table_name);
			mem_free(table_name);
			err = DB_TABLE_NOT_FOUND;
			break;

		}

		if (!row_is_mysql_tmp_table_name(table->name)) {
			/* There could be orphan temp tables left from
			interrupted alter table. Leave them, and handle
			the rest.*/
			if (table->can_be_evicted) {
				ib_logf(IB_LOG_LEVEL_WARN,
					"Orphan table encountered during "
					"DROP DATABASE. This is possible if "
					"'%s.frm' was lost.", table->name);
			}

			if (table->ibd_file_missing) {
				ib_logf(IB_LOG_LEVEL_WARN,
					"Missing %s.ibd file for table %s.",
					table->name, table->name);
			}
		}

		dict_table_close(table, TRUE, FALSE);

		/* The dict_table_t object must not be accessed before
		dict_table_open() or after dict_table_close(). But this is OK
		if we are holding, the dict_sys->mutex. */
		ut_ad(mutex_own(&dict_sys->mutex));

		/* Wait until MySQL does not have any queries running on
		the table */

		if (table->n_ref_count > 0) {
			row_mysql_unlock_data_dictionary(trx);

			ut_print_timestamp(stderr);
			fputs("  InnoDB: Warning: MySQL is trying to"
			      " drop database ", stderr);
			ut_print_name(stderr, trx, TRUE, name);
			fputs("\n"
			      "InnoDB: though there are still"
			      " open handles to table ", stderr);
			ut_print_name(stderr, trx, TRUE, table_name);
			fputs(".\n", stderr);

			os_thread_sleep(1000000);

			mem_free(table_name);

			goto loop;
		}

		err = row_drop_table_for_mysql(table_name, trx, TRUE);
		trx_commit_for_mysql(trx);

		if (err != DB_SUCCESS) {
			fputs("InnoDB: DROP DATABASE ", stderr);
			ut_print_name(stderr, trx, TRUE, name);
			fprintf(stderr, " failed with error (%s) for table ",
				ut_strerr(err));
			ut_print_name(stderr, trx, TRUE, table_name);
			putc('\n', stderr);
			mem_free(table_name);
			break;
		}

		mem_free(table_name);
	}

	if (err == DB_SUCCESS) {
		/* after dropping all tables try to drop all leftover
		foreign keys in case orphaned ones exist */
		err = drop_all_foreign_keys_in_db(name, trx);

		if (err != DB_SUCCESS) {
			fputs("InnoDB: DROP DATABASE ", stderr);
			ut_print_name(stderr, trx, TRUE, name);
			fprintf(stderr, " failed with error %d while "
				"dropping all foreign keys", err);
		}
	}

	trx_commit_for_mysql(trx);

	row_mysql_unlock_data_dictionary(trx);

	trx->op_info = "";

	return(err);
}

/*********************************************************************//**
Checks if a table name contains the string "/#sql" which denotes temporary
tables in MySQL.
@return	true if temporary table */
UNIV_INTERN __attribute__((warn_unused_result))
bool
row_is_mysql_tmp_table_name(
/*========================*/
	const char*	name)	/*!< in: table name in the form
				'database/tablename' */
{
	return(strstr(name, "/#sql") != NULL);
	/* return(strstr(name, "/@0023sql") != NULL); */
}

/****************************************************************//**
Delete a single constraint.
@return	error code or DB_SUCCESS */
static __attribute__((nonnull, warn_unused_result))
dberr_t
row_delete_constraint_low(
/*======================*/
	const char*	id,		/*!< in: constraint id */
	trx_t*		trx)		/*!< in: transaction handle */
{
	pars_info_t*	info = pars_info_create();

	pars_info_add_str_literal(info, "id", id);

	return(que_eval_sql(info,
			    "PROCEDURE DELETE_CONSTRAINT () IS\n"
			    "BEGIN\n"
			    "DELETE FROM SYS_FOREIGN_COLS WHERE ID = :id;\n"
			    "DELETE FROM SYS_FOREIGN WHERE ID = :id;\n"
			    "END;\n"
			    , FALSE, trx));
}

/****************************************************************//**
Delete a single constraint.
@return	error code or DB_SUCCESS */
static __attribute__((nonnull, warn_unused_result))
dberr_t
row_delete_constraint(
/*==================*/
	const char*	id,		/*!< in: constraint id */
	const char*	database_name,	/*!< in: database name, with the
					trailing '/' */
	mem_heap_t*	heap,		/*!< in: memory heap */
	trx_t*		trx)		/*!< in: transaction handle */
{
	dberr_t	err;

	/* New format constraints have ids <databasename>/<constraintname>. */
	err = row_delete_constraint_low(
		mem_heap_strcat(heap, database_name, id), trx);

	if ((err == DB_SUCCESS) && !strchr(id, '/')) {
		/* Old format < 4.0.18 constraints have constraint ids
		NUMBER_NUMBER. We only try deleting them if the
		constraint name does not contain a '/' character, otherwise
		deleting a new format constraint named 'foo/bar' from
		database 'baz' would remove constraint 'bar' from database
		'foo', if it existed. */

		err = row_delete_constraint_low(id, trx);
	}

	return(err);
}

/*********************************************************************//**
Renames a table for MySQL.
@return	error code or DB_SUCCESS */
UNIV_INTERN
dberr_t
row_rename_table_for_mysql(
/*=======================*/
	const char*	old_name,	/*!< in: old table name */
	const char*	new_name,	/*!< in: new table name */
	trx_t*		trx,		/*!< in/out: transaction */
	bool		commit)		/*!< in: whether to commit trx */
{
	dict_table_t*	table			= NULL;
	ibool		dict_locked		= FALSE;
	dberr_t		err			= DB_ERROR;
	mem_heap_t*	heap			= NULL;
	const char**	constraints_to_drop	= NULL;
	ulint		n_constraints_to_drop	= 0;
	ibool		old_is_tmp, new_is_tmp;
	pars_info_t*	info			= NULL;
	int		retry;

	ut_a(old_name != NULL);
	ut_a(new_name != NULL);
	ut_ad(trx->state == TRX_STATE_ACTIVE);

	if (srv_created_new_raw || srv_force_recovery) {
		fputs("InnoDB: A new raw disk partition was initialized or\n"
		      "InnoDB: innodb_force_recovery is on: we do not allow\n"
		      "InnoDB: database modifications by the user. Shut down\n"
		      "InnoDB: mysqld and edit my.cnf so that newraw"
		      " is replaced\n"
		      "InnoDB: with raw, and innodb_force_... is removed.\n",
		      stderr);

		goto funct_exit;
	} else if (row_mysql_is_system_table(new_name)) {

		fprintf(stderr,
			"InnoDB: Error: trying to create a MySQL"
			" system table %s of type InnoDB.\n"
			"InnoDB: MySQL system tables must be"
			" of the MyISAM type!\n",
			new_name);

		goto funct_exit;
	}

	trx->op_info = "renaming table";

	old_is_tmp = row_is_mysql_tmp_table_name(old_name);
	new_is_tmp = row_is_mysql_tmp_table_name(new_name);

	dict_locked = trx->dict_operation_lock_mode == RW_X_LATCH;

	table = dict_table_open_on_name(old_name, dict_locked, FALSE,
					DICT_ERR_IGNORE_NONE);

	if (!table) {
		err = DB_TABLE_NOT_FOUND;
		ut_print_timestamp(stderr);

		fputs("  InnoDB: Error: table ", stderr);
		ut_print_name(stderr, trx, TRUE, old_name);
		fputs(" does not exist in the InnoDB internal\n"
		      "InnoDB: data dictionary though MySQL is"
		      " trying to rename the table.\n"
		      "InnoDB: Have you copied the .frm file"
		      " of the table to the\n"
		      "InnoDB: MySQL database directory"
		      " from another database?\n"
		      "InnoDB: You can look for further help from\n"
		      "InnoDB: " REFMAN "innodb-troubleshooting.html\n",
		      stderr);
		goto funct_exit;

	} else if (table->ibd_file_missing
		   && !dict_table_is_discarded(table)) {

		err = DB_TABLE_NOT_FOUND;

		ib_logf(IB_LOG_LEVEL_ERROR,
			"Table %s does not have an .ibd file in the database "
			"directory. See " REFMAN "innodb-troubleshooting.html",
			old_name);

		goto funct_exit;

	} else if (new_is_tmp) {
		/* MySQL is doing an ALTER TABLE command and it renames the
		original table to a temporary table name. We want to preserve
		the original foreign key constraint definitions despite the
		name change. An exception is those constraints for which
		the ALTER TABLE contained DROP FOREIGN KEY <foreign key id>.*/

		heap = mem_heap_create(100);

		err = dict_foreign_parse_drop_constraints(
			heap, trx, table, &n_constraints_to_drop,
			&constraints_to_drop);

		if (err != DB_SUCCESS) {
			goto funct_exit;
		}
	}

	/* Is a foreign key check running on this table? */
	for (retry = 0; retry < 100
	     && table->n_foreign_key_checks_running > 0; ++retry) {
		row_mysql_unlock_data_dictionary(trx);
		os_thread_yield();
		row_mysql_lock_data_dictionary(trx);
	}

	if (table->n_foreign_key_checks_running > 0) {
		ut_print_timestamp(stderr);
		fputs(" InnoDB: Error: in ALTER TABLE ", stderr);
		ut_print_name(stderr, trx, TRUE, old_name);
		fprintf(stderr, "\n"
			"InnoDB: a FOREIGN KEY check is running.\n"
			"InnoDB: Cannot rename table.\n");
		err = DB_TABLE_IN_FK_CHECK;
		goto funct_exit;
	}

	/* We use the private SQL parser of Innobase to generate the query
	graphs needed in updating the dictionary data from system tables. */

	info = pars_info_create();

	pars_info_add_str_literal(info, "new_table_name", new_name);
	pars_info_add_str_literal(info, "old_table_name", old_name);

	err = que_eval_sql(info,
			   "PROCEDURE RENAME_TABLE () IS\n"
			   "BEGIN\n"
			   "UPDATE SYS_TABLES"
			   " SET NAME = :new_table_name\n"
			   " WHERE NAME = :old_table_name;\n"
			   "END;\n"
			   , FALSE, trx);

	/* SYS_TABLESPACES and SYS_DATAFILES track non-system tablespaces
	which have space IDs > 0. */
	if (err == DB_SUCCESS
	    && table->space != TRX_SYS_SPACE
	    && !table->ibd_file_missing) {
		/* Make a new pathname to update SYS_DATAFILES. */
		char*	new_path = row_make_new_pathname(table, new_name);

		info = pars_info_create();

		pars_info_add_str_literal(info, "new_table_name", new_name);
		pars_info_add_str_literal(info, "new_path_name", new_path);
		pars_info_add_int4_literal(info, "space_id", table->space);

		err = que_eval_sql(info,
				   "PROCEDURE RENAME_SPACE () IS\n"
				   "BEGIN\n"
				   "UPDATE SYS_TABLESPACES"
				   " SET NAME = :new_table_name\n"
				   " WHERE SPACE = :space_id;\n"
				   "UPDATE SYS_DATAFILES"
				   " SET PATH = :new_path_name\n"
				   " WHERE SPACE = :space_id;\n"
				   "END;\n"
				   , FALSE, trx);

		mem_free(new_path);
	}
	if (err != DB_SUCCESS) {
		goto end;
	}

	if (!new_is_tmp) {
		/* Rename all constraints. */
		char	new_table_name[MAX_TABLE_NAME_LEN] = "";
		char	old_table_utf8[MAX_TABLE_NAME_LEN] = "";
		uint	errors = 0;

		strncpy(old_table_utf8, old_name, MAX_TABLE_NAME_LEN);
		innobase_convert_to_system_charset(
			strchr(old_table_utf8, '/') + 1,
			strchr(old_name, '/') +1,
			MAX_TABLE_NAME_LEN, &errors);

		if (errors) {
			/* Table name could not be converted from charset
			my_charset_filename to UTF-8. This means that the
			table name is already in UTF-8 (#mysql#50). */
			strncpy(old_table_utf8, old_name, MAX_TABLE_NAME_LEN);
		}

		info = pars_info_create();

		pars_info_add_str_literal(info, "new_table_name", new_name);
		pars_info_add_str_literal(info, "old_table_name", old_name);
		pars_info_add_str_literal(info, "old_table_name_utf8",
					  old_table_utf8);

		strncpy(new_table_name, new_name, MAX_TABLE_NAME_LEN);
		innobase_convert_to_system_charset(
			strchr(new_table_name, '/') + 1,
			strchr(new_name, '/') +1,
			MAX_TABLE_NAME_LEN, &errors);

		if (errors) {
			/* Table name could not be converted from charset
			my_charset_filename to UTF-8. This means that the
			table name is already in UTF-8 (#mysql#50). */
			strncpy(new_table_name, new_name, MAX_TABLE_NAME_LEN);
		}

		pars_info_add_str_literal(info, "new_table_utf8", new_table_name);

		err = que_eval_sql(
			info,
			"PROCEDURE RENAME_CONSTRAINT_IDS () IS\n"
			"gen_constr_prefix CHAR;\n"
			"new_db_name CHAR;\n"
			"foreign_id CHAR;\n"
			"new_foreign_id CHAR;\n"
			"old_db_name_len INT;\n"
			"old_t_name_len INT;\n"
			"new_db_name_len INT;\n"
			"id_len INT;\n"
			"offset INT;\n"
			"found INT;\n"
			"BEGIN\n"
			"found := 1;\n"
			"old_db_name_len := INSTR(:old_table_name, '/')-1;\n"
			"new_db_name_len := INSTR(:new_table_name, '/')-1;\n"
			"new_db_name := SUBSTR(:new_table_name, 0,\n"
			"                      new_db_name_len);\n"
			"old_t_name_len := LENGTH(:old_table_name);\n"
			"gen_constr_prefix := CONCAT(:old_table_name_utf8,\n"
			"			     '_ibfk_');\n"
			"WHILE found = 1 LOOP\n"
			"       SELECT ID INTO foreign_id\n"
			"        FROM SYS_FOREIGN\n"
			"        WHERE FOR_NAME = :old_table_name\n"
			"         AND TO_BINARY(FOR_NAME)\n"
			"           = TO_BINARY(:old_table_name)\n"
			"         LOCK IN SHARE MODE;\n"
			"       IF (SQL % NOTFOUND) THEN\n"
			"        found := 0;\n"
			"       ELSE\n"
			"        UPDATE SYS_FOREIGN\n"
			"        SET FOR_NAME = :new_table_name\n"
			"         WHERE ID = foreign_id;\n"
			"        id_len := LENGTH(foreign_id);\n"
			"        IF (INSTR(foreign_id, '/') > 0) THEN\n"
			"               IF (INSTR(foreign_id,\n"
			"                         gen_constr_prefix) > 0)\n"
			"               THEN\n"
                        "                offset := INSTR(foreign_id, '_ibfk_') - 1;\n"
			"                new_foreign_id :=\n"
			"                CONCAT(:new_table_utf8,\n"
			"                SUBSTR(foreign_id, offset,\n"
			"                       id_len - offset));\n"
			"               ELSE\n"
			"                new_foreign_id :=\n"
			"                CONCAT(new_db_name,\n"
			"                SUBSTR(foreign_id,\n"
			"                       old_db_name_len,\n"
			"                       id_len - old_db_name_len));\n"
			"               END IF;\n"
			"               UPDATE SYS_FOREIGN\n"
			"                SET ID = new_foreign_id\n"
			"                WHERE ID = foreign_id;\n"
			"               UPDATE SYS_FOREIGN_COLS\n"
			"                SET ID = new_foreign_id\n"
			"                WHERE ID = foreign_id;\n"
			"        END IF;\n"
			"       END IF;\n"
			"END LOOP;\n"
			"UPDATE SYS_FOREIGN SET REF_NAME = :new_table_name\n"
			"WHERE REF_NAME = :old_table_name\n"
			"  AND TO_BINARY(REF_NAME)\n"
			"    = TO_BINARY(:old_table_name);\n"
			"END;\n"
			, FALSE, trx);

	} else if (n_constraints_to_drop > 0) {
		/* Drop some constraints of tmp tables. */

		ulint	db_name_len = dict_get_db_name_len(old_name) + 1;
		char*	db_name = mem_heap_strdupl(heap, old_name,
						   db_name_len);
		ulint	i;

		for (i = 0; i < n_constraints_to_drop; i++) {
			err = row_delete_constraint(constraints_to_drop[i],
						    db_name, heap, trx);

			if (err != DB_SUCCESS) {
				break;
			}
		}
	}

	if (dict_table_has_fts_index(table)
	    && !dict_tables_have_same_db(old_name, new_name)) {
		err = fts_rename_aux_tables(table, new_name, trx);

		if (err != DB_SUCCESS && (table->space != 0)) {
			char*	orig_name = table->name;

			/* If rename fails and table has its own tablespace,
			we need to call fts_rename_aux_tables again to
			revert the ibd file rename, which is not under the
			control of trx. Also notice the parent table name
			in cache is not changed yet. */
			table->name = const_cast<char*>(new_name);
			fts_rename_aux_tables(table, old_name, trx);
			table->name = orig_name;
		}
	}

end:
	if (err != DB_SUCCESS) {
		if (err == DB_DUPLICATE_KEY) {
			ut_print_timestamp(stderr);
			fputs("  InnoDB: Error; possible reasons:\n"
			      "InnoDB: 1) Table rename would cause"
			      " two FOREIGN KEY constraints\n"
			      "InnoDB: to have the same internal name"
			      " in case-insensitive comparison.\n"
			      "InnoDB: 2) table ", stderr);
			ut_print_name(stderr, trx, TRUE, new_name);
			fputs(" exists in the InnoDB internal data\n"
			      "InnoDB: dictionary though MySQL is"
			      " trying to rename table ", stderr);
			ut_print_name(stderr, trx, TRUE, old_name);
			fputs(" to it.\n"
			      "InnoDB: Have you deleted the .frm file"
			      " and not used DROP TABLE?\n"
			      "InnoDB: You can look for further help from\n"
			      "InnoDB: " REFMAN "innodb-troubleshooting.html\n"
			      "InnoDB: If table ", stderr);
			ut_print_name(stderr, trx, TRUE, new_name);
			fputs(" is a temporary table #sql..., then"
			      " it can be that\n"
			      "InnoDB: there are still queries running"
			      " on the table, and it will be\n"
			      "InnoDB: dropped automatically when"
			      " the queries end.\n"
			      "InnoDB: You can drop the orphaned table"
			      " inside InnoDB by\n"
			      "InnoDB: creating an InnoDB table with"
			      " the same name in another\n"
			      "InnoDB: database and copying the .frm file"
			      " to the current database.\n"
			      "InnoDB: Then MySQL thinks the table exists,"
			      " and DROP TABLE will\n"
			      "InnoDB: succeed.\n", stderr);
		}
		trx->error_state = DB_SUCCESS;
		trx_rollback_to_savepoint(trx, NULL);
		trx->error_state = DB_SUCCESS;
	} else {
		/* The following call will also rename the .ibd data file if
		the table is stored in a single-table tablespace */

		err = dict_table_rename_in_cache(
			table, new_name, !new_is_tmp);
		if (err != DB_SUCCESS) {
			trx->error_state = DB_SUCCESS;
			trx_rollback_to_savepoint(trx, NULL);
			trx->error_state = DB_SUCCESS;
			goto funct_exit;
		}

		/* We only want to switch off some of the type checking in
		an ALTER, not in a RENAME. */

		err = dict_load_foreigns(
			new_name, NULL,
			false, !old_is_tmp || trx->check_foreigns,
			DICT_ERR_IGNORE_NONE);

		if (err != DB_SUCCESS) {
			ut_print_timestamp(stderr);

			if (old_is_tmp) {
				fputs("  InnoDB: Error: in ALTER TABLE ",
				      stderr);
				ut_print_name(stderr, trx, TRUE, new_name);
				fputs("\n"
				      "InnoDB: has or is referenced"
				      " in foreign key constraints\n"
				      "InnoDB: which are not compatible"
				      " with the new table definition.\n",
				      stderr);
			} else {
				fputs("  InnoDB: Error: in RENAME TABLE"
				      " table ",
				      stderr);
				ut_print_name(stderr, trx, TRUE, new_name);
				fputs("\n"
				      "InnoDB: is referenced in"
				      " foreign key constraints\n"
				      "InnoDB: which are not compatible"
				      " with the new table definition.\n",
				      stderr);
			}

			ut_a(DB_SUCCESS == dict_table_rename_in_cache(
				table, old_name, FALSE));
			trx->error_state = DB_SUCCESS;
			trx_rollback_to_savepoint(trx, NULL);
			trx->error_state = DB_SUCCESS;
		}
	}

funct_exit:
	if (table != NULL) {
		dict_table_close(table, dict_locked, FALSE);
	}

	if (commit) {
		trx_commit_for_mysql(trx);
	}

	if (UNIV_LIKELY_NULL(heap)) {
		mem_heap_free(heap);
	}

	trx->op_info = "";

	return(err);
}

/*********************************************************************//**
Checks that the index contains entries in an ascending order, unique
constraint is not broken, and calculates the number of index entries
in the read view of the current transaction.
@return	true if ok */
UNIV_INTERN
bool
row_check_index_for_mysql(
/*======================*/
	row_prebuilt_t*		prebuilt,	/*!< in: prebuilt struct
						in MySQL handle */
	const dict_index_t*	index,		/*!< in: index */
	ulint*			n_rows)		/*!< out: number of entries
						seen in the consistent read */
{
	dtuple_t*	prev_entry	= NULL;
	ulint		matched_fields;
	ulint		matched_bytes;
	byte*		buf;
	ulint		ret;
	rec_t*		rec;
	bool		is_ok		= true;
	int		cmp;
	ibool		contains_null;
	ulint		i;
	ulint		cnt;
	mem_heap_t*	heap		= NULL;
	ulint		n_ext;
	ulint		offsets_[REC_OFFS_NORMAL_SIZE];
	ulint*		offsets;
	rec_offs_init(offsets_);

	*n_rows = 0;

	if (dict_index_is_clust(index)) {
		/* The clustered index of a table is always available.
		During online ALTER TABLE that rebuilds the table, the
		clustered index in the old table will have
		index->online_log pointing to the new table. All
		indexes of the old table will remain valid and the new
		table will be unaccessible to MySQL until the
		completion of the ALTER TABLE. */
	} else if (dict_index_is_online_ddl(index)
		   || (index->type & DICT_FTS)) {
		/* Full Text index are implemented by auxiliary tables,
		not the B-tree. We also skip secondary indexes that are
		being created online. */
		return(true);
	}

	buf = static_cast<byte*>(mem_alloc(UNIV_PAGE_SIZE));
	heap = mem_heap_create(100);

	cnt = 1000;

	ret = row_search_for_mysql(buf, PAGE_CUR_G, prebuilt, 0, 0);
loop:
	/* Check thd->killed every 1,000 scanned rows */
	if (--cnt == 0) {
		if (trx_is_interrupted(prebuilt->trx)) {
			goto func_exit;
		}
		cnt = 1000;
	}

	switch (ret) {
	case DB_SUCCESS:
		break;
	default:
		ut_print_timestamp(stderr);
		fputs("  InnoDB: Warning: CHECK TABLE on ", stderr);
		dict_index_name_print(stderr, prebuilt->trx, index);
		fprintf(stderr, " returned %lu\n", ret);
		/* fall through (this error is ignored by CHECK TABLE) */
	case DB_END_OF_INDEX:
func_exit:
		mem_free(buf);
		mem_heap_free(heap);

		return(is_ok);
	}

	*n_rows = *n_rows + 1;

	/* row_search... returns the index record in buf, record origin offset
	within buf stored in the first 4 bytes, because we have built a dummy
	template */

	rec = buf + mach_read_from_4(buf);

	offsets = rec_get_offsets(rec, index, offsets_,
				  ULINT_UNDEFINED, &heap);

	if (prev_entry != NULL) {
		matched_fields = 0;
		matched_bytes = 0;

		cmp = cmp_dtuple_rec_with_match(prev_entry, rec, offsets,
						&matched_fields,
						&matched_bytes);
		contains_null = FALSE;

		/* In a unique secondary index we allow equal key values if
		they contain SQL NULLs */

		for (i = 0;
		     i < dict_index_get_n_ordering_defined_by_user(index);
		     i++) {
			if (UNIV_SQL_NULL == dfield_get_len(
				    dtuple_get_nth_field(prev_entry, i))) {

				contains_null = TRUE;
				break;
			}
		}

		if (cmp > 0) {
			fputs("InnoDB: index records in a wrong order in ",
			      stderr);
not_ok:
			dict_index_name_print(stderr,
					      prebuilt->trx, index);
			fputs("\n"
			      "InnoDB: prev record ", stderr);
			dtuple_print(stderr, prev_entry);
			fputs("\n"
			      "InnoDB: record ", stderr);
			rec_print_new(stderr, rec, offsets);
			putc('\n', stderr);
			is_ok = false;
		} else if (dict_index_is_unique(index)
			   && !contains_null
			   && matched_fields
			   >= dict_index_get_n_ordering_defined_by_user(
				   index)) {

			fputs("InnoDB: duplicate key in ", stderr);
			goto not_ok;
		}
	}

	{
		mem_heap_t*	tmp_heap = NULL;

		/* Empty the heap on each round.  But preserve offsets[]
		for the row_rec_to_index_entry() call, by copying them
		into a separate memory heap when needed. */
		if (UNIV_UNLIKELY(offsets != offsets_)) {
			ulint	size = rec_offs_get_n_alloc(offsets)
				* sizeof *offsets;

			tmp_heap = mem_heap_create(size);

			offsets = static_cast<ulint*>(
				mem_heap_dup(tmp_heap, offsets, size));
		}

		mem_heap_empty(heap);

		prev_entry = row_rec_to_index_entry(
			rec, index, offsets, &n_ext, heap);

		if (UNIV_LIKELY_NULL(tmp_heap)) {
			mem_heap_free(tmp_heap);
		}
	}

	ret = row_search_for_mysql(buf, PAGE_CUR_G, prebuilt, 0, ROW_SEL_NEXT);

	goto loop;
}

/*********************************************************************//**
Determines if a table is a magic monitor table.
@return	true if monitor table */
UNIV_INTERN
bool
row_is_magic_monitor_table(
/*=======================*/
	const char*	table_name)	/*!< in: name of the table, in the
					form database/table_name */
{
	const char*	name; /* table_name without database/ */
	ulint		len;

	name = dict_remove_db_name(table_name);
	len = strlen(name) + 1;

	return(STR_EQ(name, len, S_innodb_monitor)
	       || STR_EQ(name, len, S_innodb_lock_monitor)
	       || STR_EQ(name, len, S_innodb_tablespace_monitor)
	       || STR_EQ(name, len, S_innodb_table_monitor)
#ifdef UNIV_MEM_DEBUG
	       || STR_EQ(name, len, S_innodb_mem_validate)
#endif /* UNIV_MEM_DEBUG */
	       );
}

/*********************************************************************//**
Initialize this module */
UNIV_INTERN
void
row_mysql_init(void)
/*================*/
{
	mutex_create(
		row_drop_list_mutex_key,
		&row_drop_list_mutex, SYNC_NO_ORDER_CHECK);

	UT_LIST_INIT(row_mysql_drop_list);

	row_mysql_drop_list_inited = TRUE;
}

/*********************************************************************//**
Close this module */
UNIV_INTERN
void
row_mysql_close(void)
/*================*/
{
	ut_a(UT_LIST_GET_LEN(row_mysql_drop_list) == 0);

	mutex_free(&row_drop_list_mutex);

	row_mysql_drop_list_inited = FALSE;
}

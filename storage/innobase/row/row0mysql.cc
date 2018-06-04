/*****************************************************************************

Copyright (c) 2000, 2018, Oracle and/or its affiliates. All Rights Reserved.

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

#include "ha_prototypes.h"
#include <debug_sync.h>
#include <gstream.h>
#include <spatial.h>
#include <log.h>
#include <mysys_err.h>
#include <sql_error.h>
#include <vector>

#include "row0mysql.h"

#ifdef UNIV_NONINL
#include "row0mysql.ic"
#endif

#include "btr0sea.h"
#include "dict0boot.h"
#include "dict0crea.h"
#include <sql_const.h>
#include "dict0dict.h"
#include "dict0load.h"
#include "dict0stats.h"
#include "dict0stats_bg.h"
#include "fil0fil.h"
#include "fsp0file.h"
#include "fsp0sysspace.h"
#include "fts0fts.h"
#include "fts0types.h"
#include "ibuf0ibuf.h"
#include "lock0lock.h"
#include "log0log.h"
#include "pars0pars.h"
#include "que0que.h"
#include "rem0cmp.h"
#include "row0import.h"
#include "row0ins.h"
#include "row0merge.h"
#include "row0row.h"
#include "row0sel.h"
#include "row0upd.h"
#include "srv0srv.h"
#include "trx0purge.h"
#include "trx0rec.h"
#include "trx0roll.h"
#include "trx0undo.h"
#include "row0ext.h"
#include "ut0new.h"

#include <algorithm>
#include <deque>
#include <vector>

const char* MODIFICATIONS_NOT_ALLOWED_MSG_FORCE_RECOVERY =
	"innodb_force_recovery is on. We do not allow database modifications"
	" by the user. Shut down mysqld and edit my.cnf to set"
	" innodb_force_recovery=0";

/** Provide optional 4.x backwards compatibility for 5.0 and above */
ibool	row_rollback_on_timeout	= FALSE;

/** Chain node of the list of tables to drop in the background. */
struct row_mysql_drop_t{
	char*				table_name;	/*!< table name */
	UT_LIST_NODE_T(row_mysql_drop_t)row_mysql_drop_list;
							/*!< list chain node */
};

/** @brief List of tables we should drop in background.

ALTER TABLE in MySQL requires that the table handler can drop the
table in background when there are no queries to it any
more.  Protected by row_drop_list_mutex. */
static UT_LIST_BASE_NODE_T(row_mysql_drop_t)	row_mysql_drop_list;

/** Mutex protecting the background table drop list. */
static ib_mutex_t row_drop_list_mutex;

/** Flag: has row_mysql_drop_list been initialized? */
static ibool	row_mysql_drop_list_inited	= FALSE;

extern ib_mutex_t	master_key_id_mutex;

/*******************************************************************//**
Determine if the given name is a name reserved for MySQL system tables.
@return TRUE if name is a MySQL system table name */
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
@return TRUE if the table was not yet in the drop list, and was added there */
static
ibool
row_add_table_to_background_drop_list(
/*==================================*/
	const char*	name);	/*!< in: table name */

#ifdef UNIV_DEBUG
/** Wait for the background drop list to become empty. */
void
row_wait_for_background_drop_list_empty()
{
	bool	empty = false;
	while (!empty) {
		mutex_enter(&row_drop_list_mutex);
		empty = (UT_LIST_GET_LEN(row_mysql_drop_list) == 0);
		mutex_exit(&row_drop_list_mutex);
		os_thread_sleep(100000);
	}
}
#endif /* UNIV_DEBUG */

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
void
row_mysql_prebuilt_free_blob_heap(
/*==============================*/
	row_prebuilt_t*	prebuilt)	/*!< in: prebuilt struct of a
					ha_innobase:: table handle */
{
	DBUG_ENTER("row_mysql_prebuilt_free_blob_heap");

	DBUG_PRINT("row_mysql_prebuilt_free_blob_heap",
		   ("blob_heap freeing: %p", prebuilt->blob_heap));

	mem_heap_free(prebuilt->blob_heap);
	prebuilt->blob_heap = NULL;
	DBUG_VOID_RETURN;
}

/*******************************************************************//**
Stores a >= 5.0.3 format true VARCHAR length to dest, in the MySQL row
format.
@return pointer to the data, we skip the 1 or 2 bytes at the start
that are used to store the len */
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
@return pointer to BLOB data */
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

/*******************************************************************//**
Converting InnoDB geometry data format to MySQL data format. */
void
row_mysql_store_geometry(
/*=====================*/
	byte*		dest,		/*!< in/out: where to store */
	ulint		dest_len,	/*!< in: dest buffer size: determines
					into how many bytes the GEOMETRY length
					is stored, the space for the length
					may vary from 1 to 4 bytes */
	const byte*	src,		/*!< in: GEOMETRY data; if the value to
					store is SQL NULL this should be NULL
					pointer */
	ulint		src_len)	/*!< in: GEOMETRY length; if the value
					to store is SQL NULL this should be 0;
					remember also to set the NULL bit in
					the MySQL record header! */
{
	/* MySQL might assume the field is set to zero except the length and
	the pointer fields */
	UNIV_MEM_ASSERT_RW(src, src_len);
	UNIV_MEM_ASSERT_W(dest, dest_len);
	UNIV_MEM_INVALID(dest, dest_len);

	memset(dest, '\0', dest_len);

	/* In dest there are 1 - 4 bytes reserved for the BLOB length,
	and after that 8 bytes reserved for the pointer to the data.
	In 32-bit architectures we only use the first 4 bytes of the pointer
	slot. */

	ut_ad(dest_len - 8 > 1 || src_len < 1<<8);
	ut_ad(dest_len - 8 > 2 || src_len < 1<<16);
	ut_ad(dest_len - 8 > 3 || src_len < 1<<24);

	mach_write_to_n_little_endian(dest, dest_len - 8, src_len);

	memcpy(dest + dest_len - 8, &src, sizeof src);

	DBUG_EXECUTE_IF("row_print_geometry_data",
	{
		String  res;
		Geometry_buffer buffer;
		String  wkt;

		/** Show the meaning of geometry data. */
		Geometry* g = Geometry::construct(
			&buffer, (const char*)src, (uint32) src_len);

		if (g)
		{
			if (g->as_wkt(&wkt) == 0)
			{
				ib::info() << "Write geometry data to"
					" MySQL WKT format: "
					<< wkt.c_ptr_safe() << ".";
			}
		}
	});
}

/*******************************************************************//**
Read geometry data in the MySQL format.
@return pointer to geometry data */
const byte*
row_mysql_read_geometry(
/*====================*/
	ulint*		len,		/*!< out: data length */
	const byte*	ref,		/*!< in: geometry data in the
					MySQL format */
	ulint		col_len)	/*!< in: MySQL format length */
{
	byte*		data;

	*len = mach_read_from_n_little_endian(ref, col_len - 8);

	memcpy(&data, ref + col_len - 8, sizeof data);

	DBUG_EXECUTE_IF("row_print_geometry_data",
	{
		String  res;
		Geometry_buffer buffer;
		String  wkt;

		/** Show the meaning of geometry data. */
		Geometry* g = Geometry::construct(
			&buffer, (const char*) data, (uint32) *len);

		if (g)
		{
			if (g->as_wkt(&wkt) == 0)
			{
				ib::info() << "Read geometry data in"
					" MySQL's WKT format: "
					<< wkt.c_ptr_safe() << ".";
			}
		}
	});

	return(data);
}

/**************************************************************//**
Pad a column with spaces. */
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
@return up to which byte we used buf in the conversion */
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
		Consider a CHAR(5) field containing the string
		".a   " where "." denotes a 3-byte character represented
		by the bytes "$%&". After our stripping, the string will
		be stored as "$%&a " (5 bytes). The string
		".abc " will be stored as "$%&abc" (6 bytes).

		The space padding will be restored in row0sel.cc, function
		row_sel_field_store_in_mysql_format(). */

		ulint		n_chars;

		ut_a(!(dtype_get_len(dtype) % dtype_get_mbmaxlen(dtype)));

		n_chars = dtype_get_len(dtype) / dtype_get_mbmaxlen(dtype);

		/* Strip space padding. */
		while (col_len > n_chars && ptr[col_len - 1] == 0x20) {
			col_len--;
		}
	} else if (!row_format_col) {
		/* if mysql data is from a MySQL key value
		since the length is always stored in 2 bytes,
		we need do nothing here. */
	} else if (type == DATA_BLOB) {

		ptr = row_mysql_read_blob_ref(&col_len, mysql_data, col_len);
	} else if (DATA_GEOMETRY_MTYPE(type)) {
		/* We use blob to store geometry data except DATA_POINT
		internally, but in MySQL Layer the datatype is always blob. */
		ptr = row_mysql_read_geometry(&col_len, mysql_data, col_len);
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
	const byte*	mysql_rec,	/*!< in: row in the MySQL format;
					NOTE: do not discard as long as
					row is used, as row may contain
					pointers to this record! */
	mem_heap_t**	blob_heap)	/*!< in: FIX_ME, remove this after
					server fixes its issue */
{
	const mysql_row_templ_t*templ;
	dfield_t*		dfield;
	ulint			i;
	ulint			n_col = 0;
	ulint			n_v_col = 0;

	ut_ad(prebuilt->template_type == ROW_MYSQL_WHOLE_ROW);
	ut_ad(prebuilt->mysql_template);

	for (i = 0; i < prebuilt->n_template; i++) {

		templ = prebuilt->mysql_template + i;

		if (templ->is_virtual) {
			ut_ad(n_v_col < dtuple_get_n_v_fields(row));
			dfield = dtuple_get_nth_v_field(row, n_v_col);
			n_v_col++;
		} else {
			dfield = dtuple_get_nth_field(row, n_col);
			n_col++;
		}

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

		/* server has issue regarding handling BLOB virtual fields,
		and we need to duplicate it with our own memory here */
		if (templ->is_virtual
		    && DATA_LARGE_MTYPE(dfield_get_type(dfield)->mtype)) {
			if (*blob_heap == NULL) {
				*blob_heap = mem_heap_create(dfield->len);
			}
			dfield_dup(dfield, *blob_heap);
		}
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
	case DB_CANT_CREATE_GEOMETRY_OBJECT:
	case DB_COMPUTE_VALUE_FAILED:
		DBUG_EXECUTE_IF("row_mysql_crash_if_error", {
					log_buffer_flush_to_disk();
					DBUG_SUICIDE(); });
		if (savept) {
			/* Roll back the latest, possibly incomplete insertion
			or update */

			trx_rollback_to_savepoint(trx, savept);
		}
		/* MySQL will roll back the latest SQL statement */
		break;
	case DB_LOCK_WAIT:

		trx_kill_blocking(trx);

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
		ib::fatal() << "The database cannot continue operation because"
			" of lack of space. You must add a new data file"
			" to my.cnf and restart the database.";
		break;

	case DB_CORRUPTION:
		ib::error() << "We detected index corruption in an InnoDB type"
			" table. You have to dump + drop + reimport the"
			" table or, in a case of widespread corruption,"
			" dump all InnoDB tables and recreate the whole"
			" tablespace. If the mysqld server crashes after"
			" the startup or when you dump the tables. "
			<< FORCE_RECOVERY_MSG;
		break;
	case DB_FOREIGN_EXCEED_MAX_CASCADE:
		ib::error() << "Cannot delete/update rows with cascading"
			" foreign key constraints that exceed max depth of "
			<< FK_MAX_CASCADE_DEL << ". Please drop excessive"
			" foreign constraints and try again";
		break;
	default:
		ib::fatal() << "Unknown error code " << err << ": "
			<< ut_strerr(err);
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
@return own: a prebuilt struct */
row_prebuilt_t*
row_create_prebuilt(
/*================*/
	dict_table_t*	table,		/*!< in: Innobase table handle */
	ulint		mysql_row_len)	/*!< in: length in bytes of a row in
					the MySQL format */
{
	DBUG_ENTER("row_create_prebuilt");

	row_prebuilt_t*	prebuilt;
	mem_heap_t*	heap;
	dict_index_t*	clust_index;
	dict_index_t*	temp_index;
	dtuple_t*	ref;
	ulint		ref_len;
	uint		srch_key_len = 0;
	ulint		search_tuple_n_fields;

	search_tuple_n_fields = 2 * (dict_table_get_n_cols(table)
				     + dict_table_get_n_v_cols(table));

	clust_index = dict_table_get_first_index(table);

	/* Make sure that search_tuple is long enough for clustered index */
	ut_a(2 * dict_table_get_n_cols(table) >= clust_index->n_fields);

	ref_len = dict_index_get_n_unique(clust_index);


        /* Maximum size of the buffer needed for conversion of INTs from
	little endian format to big endian format in an index. An index
	can have maximum 16 columns (MAX_REF_PARTS) in it. Therfore
	Max size for PK: 16 * 8 bytes (BIGINT's size) = 128 bytes
	Max size Secondary index: 16 * 8 bytes + PK = 256 bytes. */
#define MAX_SRCH_KEY_VAL_BUFFER         2* (8 * MAX_REF_PARTS)

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
	+ DTUPLE_EST_ALLOC(dict_table_get_n_cols(table) \
			   + dict_table_get_n_v_cols(table)) \
	+ sizeof(que_fork_t) \
	+ sizeof(que_thr_t) \
	+ sizeof(*prebuilt->pcur) \
	+ sizeof(*prebuilt->clust_pcur) \
	)

	/* Calculate size of key buffer used to store search key in
	InnoDB format. MySQL stores INTs in little endian format and
	InnoDB stores INTs in big endian format with the sign bit
	flipped. All other field types are stored/compared the same
	in MySQL and InnoDB, so we must create a buffer containing
	the INT key parts in InnoDB format.We need two such buffers
	since both start and end keys are used in records_in_range(). */

	for (temp_index = dict_table_get_first_index(table); temp_index;
	     temp_index = dict_table_get_next_index(temp_index)) {
		DBUG_EXECUTE_IF("innodb_srch_key_buffer_max_value",
			ut_a(temp_index->n_user_defined_cols
						== MAX_REF_PARTS););
		uint temp_len = 0;
		for (uint i = 0; i < temp_index->n_uniq; i++) {
			ulint type = temp_index->fields[i].col->mtype;
			if (type == DATA_INT) {
				temp_len +=
					temp_index->fields[i].fixed_len;
			}
		}
		srch_key_len = std::max(srch_key_len,temp_len);
	}

	ut_a(srch_key_len <= MAX_SRCH_KEY_VAL_BUFFER);

	DBUG_EXECUTE_IF("innodb_srch_key_buffer_max_value",
		ut_a(srch_key_len == MAX_SRCH_KEY_VAL_BUFFER););

	/* We allocate enough space for the objects that are likely to
	be created later in order to minimize the number of malloc()
	calls */
	heap = mem_heap_create(PREBUILT_HEAP_INITIAL_SIZE + 2 * srch_key_len);

	prebuilt = static_cast<row_prebuilt_t*>(
		mem_heap_zalloc(heap, sizeof(*prebuilt)));

	prebuilt->magic_n = ROW_PREBUILT_ALLOCATED;
	prebuilt->magic_n2 = ROW_PREBUILT_ALLOCATED;

	prebuilt->table = table;

	prebuilt->sql_stat_start = TRUE;
	prebuilt->heap = heap;

	prebuilt->srch_key_val_len = srch_key_len;
	if (prebuilt->srch_key_val_len) {
		prebuilt->srch_key_val1 = static_cast<byte*>(
			mem_heap_alloc(prebuilt->heap,
				       2 * prebuilt->srch_key_val_len));
		prebuilt->srch_key_val2 = prebuilt->srch_key_val1 +
						prebuilt->srch_key_val_len;
	} else {
		prebuilt->srch_key_val1 = NULL;
		prebuilt->srch_key_val2 = NULL;
	}

	prebuilt->pcur = static_cast<btr_pcur_t*>(
				mem_heap_zalloc(prebuilt->heap,
					       sizeof(btr_pcur_t)));
	prebuilt->clust_pcur = static_cast<btr_pcur_t*>(
					mem_heap_zalloc(prebuilt->heap,
						       sizeof(btr_pcur_t)));
	btr_pcur_reset(prebuilt->pcur);
	btr_pcur_reset(prebuilt->clust_pcur);

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

	prebuilt->ins_sel_stmt = false;
	prebuilt->session = NULL;

	prebuilt->fts_doc_id_in_read_set = 0;
	prebuilt->blob_heap = NULL;

	prebuilt->m_no_prefetch = false;
	prebuilt->m_read_virtual_key = false;

	DBUG_RETURN(prebuilt);
}

/********************************************************************//**
Free a prebuilt struct for a MySQL table handle. */
void
row_prebuilt_free(
/*==============*/
	row_prebuilt_t*	prebuilt,	/*!< in, own: prebuilt struct */
	ibool		dict_locked)	/*!< in: TRUE=data dictionary locked */
{
	DBUG_ENTER("row_prebuilt_free");

	ut_a(prebuilt->magic_n == ROW_PREBUILT_ALLOCATED);
	ut_a(prebuilt->magic_n2 == ROW_PREBUILT_ALLOCATED);

	prebuilt->magic_n = ROW_PREBUILT_FREED;
	prebuilt->magic_n2 = ROW_PREBUILT_FREED;

	btr_pcur_reset(prebuilt->pcur);
	btr_pcur_reset(prebuilt->clust_pcur);

	ut_free(prebuilt->mysql_template);

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
		row_mysql_prebuilt_free_blob_heap(prebuilt);
	}

	if (prebuilt->old_vers_heap) {
		mem_heap_free(prebuilt->old_vers_heap);
	}

	if (prebuilt->fetch_cache[0] != NULL) {
		byte*	base = prebuilt->fetch_cache[0] - 4;
		byte*	ptr = base;

		for (ulint i = 0; i < MYSQL_FETCH_CACHE_SIZE; i++) {
			ulint	magic1 = mach_read_from_4(ptr);
			ut_a(magic1 == ROW_PREBUILT_FETCH_MAGIC_N);
			ptr += 4;

			byte*	row = ptr;
			ut_a(row == prebuilt->fetch_cache[i]);
			ptr += prebuilt->mysql_row_len;

			ulint	magic2 = mach_read_from_4(ptr);
			ut_a(magic2 == ROW_PREBUILT_FETCH_MAGIC_N);
			ptr += 4;
		}

		ut_free(base);
	}

	if (prebuilt->rtr_info) {
		rtr_clean_rtr_info(prebuilt->rtr_info, true);
	}
	if (prebuilt->table) {
		dict_table_close(prebuilt->table, dict_locked, TRUE);
	}

	mem_heap_free(prebuilt->heap);

	DBUG_VOID_RETURN;
}

/*********************************************************************//**
Updates the transaction pointers in query graphs stored in the prebuilt
struct. */
void
row_update_prebuilt_trx(
/*====================*/
	row_prebuilt_t*	prebuilt,	/*!< in/out: prebuilt struct
					in MySQL handle */
	trx_t*		trx)		/*!< in: transaction handle */
{
	ut_a(trx->magic_n == TRX_MAGIC_N);
	ut_a(prebuilt->magic_n == ROW_PREBUILT_ALLOCATED);
	ut_a(prebuilt->magic_n2 == ROW_PREBUILT_ALLOCATED);

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
@return prebuilt dtuple; the column type information is also set in it */
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

	row = dtuple_create_with_vcol(
			prebuilt->heap, dict_table_get_n_cols(table),
			dict_table_get_n_v_cols(table));

	dict_table_copy_types(row, table);

	ins_node_set_new_row(node, row);

	prebuilt->ins_graph = static_cast<que_fork_t*>(
		que_node_get_parent(
			pars_complete_graph_for_exec(
				node,
				prebuilt->trx, prebuilt->heap, prebuilt)));

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
			fprintf(stderr, "test_upd_stats_if_needed_not_inited"
				" was executed\n");
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
@return error code or DB_SUCCESS */
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

	/* If we already hold an AUTOINC lock on the table then do nothing.
	Note: We peek at the value of the current owner without acquiring
	the lock mutex. */
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

	trx_start_if_not_started_xa(trx, true);

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
@return error code or DB_SUCCESS */
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

	trx_start_if_not_started_xa(trx, false);

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

/** Perform explicit rollback in absence of UNDO logs.
@param[in]	index	apply rollback action on this index
@param[in]	entry	entry to remove/rollback.
@param[in,out]	thr	thread handler.
@param[in,out]	mtr	mini transaction.
@return error code or DB_SUCCESS */
static
dberr_t
row_explicit_rollback(
	dict_index_t*		index,
	const dtuple_t*		entry,
	que_thr_t*		thr,
	mtr_t*			mtr)
{
	btr_cur_t	cursor;
	ulint		flags;
	ulint		offsets_[REC_OFFS_NORMAL_SIZE];
	ulint*		offsets;
	mem_heap_t*	heap = NULL;
	dberr_t		err;

	rec_offs_init(offsets_);
	flags = BTR_NO_LOCKING_FLAG | BTR_NO_UNDO_LOG_FLAG;

	btr_cur_search_to_nth_level_with_no_latch(
		index, 0, entry, PAGE_CUR_LE,
		&cursor, __FILE__, __LINE__, mtr);

	offsets = rec_get_offsets(
		btr_cur_get_rec(&cursor), index, offsets_,
		ULINT_UNDEFINED, &heap);

	if (dict_index_is_clust(index)) {
		err = btr_cur_del_mark_set_clust_rec(
			flags, btr_cur_get_block(&cursor),
			btr_cur_get_rec(&cursor), index,
			offsets, thr, entry, mtr);
	} else {
		err = btr_cur_del_mark_set_sec_rec(
			flags, &cursor, TRUE, thr, mtr);
	}
	ut_ad(err == DB_SUCCESS);

	/* Void call just to set mtr modification flag
	to true failing which block is not scheduled for flush*/
	byte* log_ptr = mlog_open(mtr, 0);
	ut_ad(log_ptr == NULL);
	if (log_ptr != NULL) {
		/* To keep complier happy. */
		mlog_close(mtr, log_ptr);
	}

	if (heap != NULL) {
		mem_heap_free(heap);
	}

	return(err);
}

/** Convert a row in the MySQL format to a row in the Innobase format.
This is specialized function used for intrinsic table with reduce branching.
@param[in,out]	row		row where field values are copied.
@param[in]	prebuilt	prebuilt handler
@param[in]	mysql_rec	row in mysql format. */
static
void
row_mysql_to_innobase(
	dtuple_t*		row,
	row_prebuilt_t*		prebuilt,
	const byte*		mysql_rec)
{
	ut_ad(dict_table_is_intrinsic(prebuilt->table));

	const byte*		ptr = mysql_rec;

	for (ulint i = 0; i < prebuilt->n_template; i++) {
		const mysql_row_templ_t*	templ;
		dfield_t*			dfield;

		templ = prebuilt->mysql_template + i;
		dfield = dtuple_get_nth_field(row, i);

		/* Check if column has null value. */
		if (templ->mysql_null_bit_mask != 0) {
			if (mysql_rec[templ->mysql_null_byte_offset]
			    & (byte) (templ->mysql_null_bit_mask)) {
				dfield_set_null(dfield);
				continue;
			}
		}

		/* Extract the column value. */
		ptr = mysql_rec + templ->mysql_col_offset;
		const dtype_t*	dtype = dfield_get_type(dfield);
		ulint		col_len = templ->mysql_col_len;

		ut_ad(dtype->mtype == DATA_INT
		      || dtype->mtype == DATA_CHAR
		      || dtype->mtype == DATA_MYSQL
		      || dtype->mtype == DATA_VARCHAR
		      || dtype->mtype == DATA_VARMYSQL
		      || dtype->mtype == DATA_BINARY
		      || dtype->mtype == DATA_FIXBINARY
		      || dtype->mtype == DATA_FLOAT
		      || dtype->mtype == DATA_DOUBLE
		      || dtype->mtype == DATA_DECIMAL
		      || dtype->mtype == DATA_BLOB
		      || dtype->mtype == DATA_GEOMETRY
		      || dtype->mtype == DATA_POINT
		      || dtype->mtype == DATA_VAR_POINT);

#ifdef UNIV_DEBUG
		if (dtype_get_mysql_type(dtype) == DATA_MYSQL_TRUE_VARCHAR) {
			ut_ad(templ->mysql_length_bytes > 0);
		}
#endif /* UNIV_DEBUG */

		/* For now varchar field this has to be always 0 so
		memcpy of 0 bytes shouldn't affect the original col_len. */
		if (dtype->mtype == DATA_INT) {
			/* Convert and Store in big-endian. */
			byte*	buf = prebuilt->ins_upd_rec_buff
				+ templ->mysql_col_offset;
			byte*	copy_to = buf + col_len;
			for (;;) {
				copy_to--;
				*copy_to = *ptr;
				if (copy_to == buf) {
					break;
				}
				ptr++;
			}

			if (!(dtype->prtype & DATA_UNSIGNED)) {
				*buf ^= 128;
			}

			ptr = buf;
			buf += col_len;
		} else if (dtype_get_mysql_type(dtype) ==
				DATA_MYSQL_TRUE_VARCHAR) {

			ut_ad(dtype->mtype == DATA_VARCHAR
			      || dtype->mtype == DATA_VARMYSQL
			      || dtype->mtype == DATA_BINARY);

			col_len = 0;
			row_mysql_read_true_varchar(
				&col_len, ptr, templ->mysql_length_bytes);
			ptr += templ->mysql_length_bytes;
		} else if (dtype->mtype == DATA_BLOB) {
			ptr = row_mysql_read_blob_ref(&col_len, ptr, col_len);
		} else if (DATA_GEOMETRY_MTYPE(dtype->mtype)) {
			/* Point, Var-Point, Geometry */
			ptr = row_mysql_read_geometry(&col_len, ptr, col_len);
		}

		dfield_set_data(dfield, ptr, col_len);
	}
}

/** Does an insert for MySQL using cursor interface.
Cursor interface is low level interface that directly interacts at
Storage Level by-passing all the locking and transaction semantics.
For InnoDB case, this will also by-pass hidden column generation.
@param[in]	mysql_rec	row in the MySQL format
@param[in,out]	prebuilt	prebuilt struct in MySQL handle
@return error code or DB_SUCCESS */
static
dberr_t
row_insert_for_mysql_using_cursor(
	const byte*		mysql_rec,
	row_prebuilt_t*		prebuilt)
{
	dberr_t		err	= DB_SUCCESS;
	ins_node_t*	node	= NULL;
	que_thr_t*	thr	= NULL;
	mtr_t		mtr;

	/* Step-1: Get the reference of row to insert. */
	row_get_prebuilt_insert_row(prebuilt);
	node = prebuilt->ins_node;
	thr = que_fork_get_first_thr(prebuilt->ins_graph);

	/* Step-2: Convert row from MySQL row format to InnoDB row format. */
	row_mysql_to_innobase(node->row, prebuilt, mysql_rec);

	/* Step-3: Append row-id index is not unique. */
	dict_index_t*	clust_index = dict_table_get_first_index(node->table);

	if (!dict_index_is_unique(clust_index)) {
		dict_sys_write_row_id(
			node->row_id_buf,
			dict_table_get_next_table_sess_row_id(node->table));
	}

	trx_write_trx_id(node->trx_id_buf,
			 dict_table_get_next_table_sess_trx_id(node->table));

	/* Step-4: Iterate over all the indexes and insert entries. */
	dict_index_t*	inserted_upto = NULL;
	node->entry = UT_LIST_GET_FIRST(node->entry_list);
	for (dict_index_t* index = UT_LIST_GET_FIRST(node->table->indexes);
	     index != NULL;
	     index = UT_LIST_GET_NEXT(indexes, index),
	     node->entry = UT_LIST_GET_NEXT(tuple_list, node->entry)) {

		node->index = index;
		err = row_ins_index_entry_set_vals(
			node->index, node->entry, node->row);
		if (err != DB_SUCCESS) {
			break;
		}

		if (dict_index_is_clust(index)) {
			err = row_ins_clust_index_entry(
				node->index, node->entry, thr, 0, false);
		} else {
			err = row_ins_sec_index_entry(
				node->index, node->entry, thr, false);
		}

		if (err == DB_SUCCESS) {
			inserted_upto = index;
		} else {
			break;
		}
	}

	/* Step-5: If error is encountered while inserting entries to any
	of the index then entries inserted to previous indexes are removed
	explicity. Automatic rollback is not in action as UNDO logs are
	turned-off. */
	if (err != DB_SUCCESS) {

		node->entry = UT_LIST_GET_FIRST(node->entry_list);

		mtr_start(&mtr);
		dict_disable_redo_if_temporary(node->table, &mtr);

		for (dict_index_t* index =
			UT_LIST_GET_FIRST(node->table->indexes);
		     inserted_upto != NULL;
		     index = UT_LIST_GET_NEXT(indexes, index),
		     node->entry = UT_LIST_GET_NEXT(tuple_list, node->entry)) {

			row_explicit_rollback(index, node->entry, thr, &mtr);

			if (index == inserted_upto) {
				break;
			}
		}

		mtr_commit(&mtr);
	} else {
		/* Not protected by dict_table_stats_lock() for performance
		reasons, we would rather get garbage in stat_n_rows (which is
		just an estimate anyway) than protecting the following code
		, with a latch. */
		dict_table_n_rows_inc(node->table);

		srv_stats.n_rows_inserted.inc();
	}

	thr_get_trx(thr)->error_state = DB_SUCCESS;
	return(err);
}

/** Does an insert for MySQL using INSERT graph. This function will run/execute
INSERT graph.
@param[in]	mysql_rec	row in the MySQL format
@param[in,out]	prebuilt	prebuilt struct in MySQL handle
@return error code or DB_SUCCESS */
static
dberr_t
row_insert_for_mysql_using_ins_graph(
	const byte*	mysql_rec,
	row_prebuilt_t*	prebuilt)
{
	trx_savept_t	savept;
	que_thr_t*	thr;
	dberr_t		err;
	ibool		was_lock_wait;
	trx_t*		trx		= prebuilt->trx;
	ins_node_t*	node		= prebuilt->ins_node;
	dict_table_t*	table		= prebuilt->table;

	/* FIX_ME: This blob heap is used to compensate an issue in server
	for virtual column blob handling */
	mem_heap_t*	blob_heap = NULL;

	ut_ad(trx);
	ut_a(prebuilt->magic_n == ROW_PREBUILT_ALLOCATED);
	ut_a(prebuilt->magic_n2 == ROW_PREBUILT_ALLOCATED);

	if (dict_table_is_discarded(prebuilt->table)) {

		ib::error() << "The table " << prebuilt->table->name
			<< " doesn't have a corresponding tablespace, it was"
			" discarded.";

		return(DB_TABLESPACE_DELETED);

	} else if (prebuilt->table->ibd_file_missing) {

		ib::error() << ".ibd file is missing for table "
			<< prebuilt->table->name;

		return(DB_TABLESPACE_NOT_FOUND);

	} else if (srv_force_recovery) {

		ib::error() << MODIFICATIONS_NOT_ALLOWED_MSG_FORCE_RECOVERY;

		return(DB_READ_ONLY);
	}

	DBUG_EXECUTE_IF("mark_table_corrupted", {
		/* Mark the table corrupted for the clustered index */
		dict_index_t*	index = dict_table_get_first_index(table);
		ut_ad(dict_index_is_clust(index));
		dict_set_corrupted(index, trx, "INSERT TABLE"); });

	if (dict_table_is_corrupted(table)) {

		ib::error() << "Table " << table->name << " is corrupt.";
		return(DB_TABLE_CORRUPT);
	}

	trx->op_info = "inserting";

	row_mysql_delay_if_needed();

	trx_start_if_not_started_xa(trx, true);

	row_get_prebuilt_insert_row(prebuilt);
	node = prebuilt->ins_node;

	row_mysql_convert_row_to_innobase(node->row, prebuilt, mysql_rec,
					  &blob_heap);

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

	DEBUG_SYNC_C("ib_after_row_insert_step");

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

		node->duplicate = NULL;
		trx->op_info = "";

		if (blob_heap != NULL) {
			mem_heap_free(blob_heap);
		}

		return(err);
	}

	node->duplicate = NULL;

	if (dict_table_has_fts_index(table)) {
		doc_id_t	doc_id;

		/* Extract the doc id from the hidden FTS column */
		doc_id = fts_get_doc_id_from_row(table, node->row);

		if (doc_id <= 0) {
			ib::error() << "FTS Doc ID must be large than 0";
			err = DB_FTS_INVALID_DOCID;
			trx->error_state = DB_FTS_INVALID_DOCID;
			goto error_exit;
		}

		if (!DICT_TF2_FLAG_IS_SET(table, DICT_TF2_FTS_HAS_DOC_ID)) {
			doc_id_t	next_doc_id
				= table->fts->cache->next_doc_id;

			if (doc_id < next_doc_id) {

				ib::error() << "FTS Doc ID must be large than "
					<< next_doc_id - 1 << " for table "
					<< table->name;

				err = DB_FTS_INVALID_DOCID;
				trx->error_state = DB_FTS_INVALID_DOCID;
				goto error_exit;
			}

			/* Difference between Doc IDs are restricted within
			4 bytes integer. See fts_get_encoded_len(). Consecutive
			doc_ids difference should not exceed
			FTS_DOC_ID_MAX_STEP value. */

			if (doc_id - next_doc_id >= FTS_DOC_ID_MAX_STEP) {
				 ib::error() << "Doc ID " << doc_id
					<< " is too big. Its difference with"
					" largest used Doc ID "
					<< next_doc_id - 1 << " cannot"
					" exceed or equal to "
					<< FTS_DOC_ID_MAX_STEP;
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

	srv_stats.n_rows_inserted.inc();

	/* Not protected by dict_table_stats_lock() for performance
	reasons, we would rather get garbage in stat_n_rows (which is
	just an estimate anyway) than protecting the following code
	with a latch. */
	dict_table_n_rows_inc(table);

	row_update_statistics_if_needed(table);
	trx->op_info = "";

	if (blob_heap != NULL) {
		mem_heap_free(blob_heap);
	}

	return(err);
}

/** Does an insert for MySQL.
@param[in]	mysql_rec	row in the MySQL format
@param[in,out]	prebuilt	prebuilt struct in MySQL handle
@return error code or DB_SUCCESS*/
dberr_t
row_insert_for_mysql(
	const byte*		mysql_rec,
	row_prebuilt_t*		prebuilt)
{
	/* For intrinsic tables there a lot of restrictions that can be
	relaxed including locking of table, transaction handling, etc.
	Use direct cursor interface for inserting to intrinsic tables. */
	if (dict_table_is_intrinsic(prebuilt->table)) {
		return(row_insert_for_mysql_using_cursor(mysql_rec, prebuilt));
	} else {
		return(row_insert_for_mysql_using_ins_graph(
			mysql_rec, prebuilt));
	}
}

/*********************************************************************//**
Builds a dummy query graph used in selects. */
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
					prebuilt->trx, prebuilt->heap,
					prebuilt)));

		prebuilt->sel_graph->state = QUE_FORK_ACTIVE;
	}
}

/*********************************************************************//**
Creates an query graph node of 'update' type to be used in the MySQL
interface.
@return own: update node */
upd_node_t*
row_create_update_node_for_mysql(
/*=============================*/
	dict_table_t*	table,	/*!< in: table to update */
	mem_heap_t*	heap)	/*!< in: mem heap from which allocated */
{
	upd_node_t*	node;

	DBUG_ENTER("row_create_update_node_for_mysql");

	node = upd_node_create(heap);

	node->in_mysql_interface = TRUE;
	node->is_delete = FALSE;
	node->searched_update = FALSE;
	node->select = NULL;
	node->pcur = btr_pcur_create_for_mysql();

	DBUG_PRINT("info", ("node: %p, pcur: %p", node, node->pcur));

	node->table = table;

	node->update = upd_create(dict_table_get_n_cols(table)
				  + dict_table_get_n_v_cols(table), heap);

	node->update_n_fields = dict_table_get_n_cols(table);

	UT_LIST_INIT(node->columns, &sym_node_t::col_var_list);

	node->has_clust_rec_x_lock = TRUE;
	node->cmpl_info = 0;

	node->table_sym = NULL;
	node->col_assign_list = NULL;

	DBUG_RETURN(node);
}

/*********************************************************************//**
Gets pointer to a prebuilt update vector used in updates. If the update
graph has not yet been built in the prebuilt struct, then this function
first builds it.
@return prebuilt update vector */
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
					prebuilt->trx, prebuilt->heap,
					prebuilt)));

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
	if(trx->fts_next_doc_id) {
		fts_trx_add_op(trx, table, old_doc_id, FTS_DELETE, NULL);
		if(new_doc_id != FTS_NULL_DOC_ID)
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

	DBUG_ENTER("row_fts_update_or_delete");

	ut_a(dict_table_has_fts_index(prebuilt->table));

	/* Deletes are simple; get them out of the way first. */
	if (node->is_delete) {
		/* A delete affects all FTS indexes, so we pass NULL */
		fts_trx_add_op(trx, table, old_doc_id, FTS_DELETE, NULL);
	} else {
		doc_id_t	new_doc_id;
		new_doc_id = fts_read_doc_id((byte*) &trx->fts_next_doc_id);

		if (new_doc_id == 0) {
			ib::error() << "InnoDB FTS: Doc ID cannot be 0";
			return(DB_FTS_INVALID_DOCID);
		}
		row_fts_do_update(trx, table, old_doc_id, new_doc_id);
	}

	DBUG_RETURN(DB_SUCCESS);
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

	table->fk_max_recusive_level = 0;

	(*depth)++;

	/* Limit on tables involved in cascading delete/update */
	if (*depth > FK_MAX_CASCADE_DEL) {
		return;
	}

	/* Loop through this table's referenced list and also
	recursively traverse each table's foreign table list */
	for (dict_foreign_set::iterator it = table->referenced_set.begin();
	     it != table->referenced_set.end();
	     ++it) {

		foreign = *it;

		ut_ad(foreign->foreign_table != NULL);

		if (foreign->foreign_table->fts != NULL) {
			fts_init_doc_id(foreign->foreign_table);
		}

		if (!foreign->foreign_table->referenced_set.empty()
		    && foreign->foreign_table != table) {
			init_fts_doc_id_for_ref(
				foreign->foreign_table, depth);
		}
	}
}

/* A functor for decrementing counters. */
class ib_dec_counter {
public:
	ib_dec_counter() {}

	void operator() (upd_node_t* node) {
		ut_ad(node->table->n_foreign_key_checks_running > 0);
		os_atomic_decrement_ulint(
			&node->table->n_foreign_key_checks_running, 1);
	}
};


typedef	std::vector<btr_pcur_t, ut_allocator<btr_pcur_t> >	cursors_t;

/** Delete row from table (corresponding entries from all the indexes).
Function will maintain cursor to the entries to invoke explicity rollback
just incase update action following delete fails.

@param[in]	node		update node carrying information to delete.
@param[out]	delete_entries	vector of cursor to deleted entries.
@param[in]	restore_delete	if true, then restore DELETE records by
				unmarking delete.
@return error code or DB_SUCCESS */
static
dberr_t
row_delete_for_mysql_using_cursor(
	const upd_node_t*	node,
	cursors_t&		delete_entries,
	bool			restore_delete)
{
	mtr_t		mtr;
	dict_table_t*	table = node->table;
	mem_heap_t*	heap = mem_heap_create(1000);
	dberr_t		err = DB_SUCCESS;
	dtuple_t*	entry;

	mtr_start(&mtr);
	dict_disable_redo_if_temporary(table, &mtr);

	for (dict_index_t* index = UT_LIST_GET_FIRST(table->indexes);
	     index != NULL && err == DB_SUCCESS && !restore_delete;
	     index = UT_LIST_GET_NEXT(indexes, index)) {

		entry = row_build_index_entry(
			node->row, node->ext, index, heap);

		btr_pcur_t	pcur;

		btr_pcur_open(index, entry, PAGE_CUR_LE,
			      BTR_MODIFY_LEAF, &pcur, &mtr);

#ifdef UNIV_DEBUG
		ulint           offsets_[REC_OFFS_NORMAL_SIZE];
		ulint*          offsets         = offsets_;
		rec_offs_init(offsets_);

		offsets = rec_get_offsets(
			btr_cur_get_rec(btr_pcur_get_btr_cur(&pcur)),
			index, offsets, ULINT_UNDEFINED, &heap);

		ut_ad(!cmp_dtuple_rec(
			entry, btr_cur_get_rec(btr_pcur_get_btr_cur(&pcur)),
			offsets));
#endif /* UNIV_DEBUG */

		ut_ad(!rec_get_deleted_flag(
			btr_cur_get_rec(btr_pcur_get_btr_cur(&pcur)),
			dict_table_is_comp(index->table)));

		ut_ad(btr_pcur_get_block(&pcur)->made_dirty_with_no_latch);

		if (page_rec_is_infimum(btr_pcur_get_rec(&pcur))
		    || page_rec_is_supremum(btr_pcur_get_rec(&pcur))) {
			err = DB_ERROR;
		} else {
			btr_cur_t* btr_cur = btr_pcur_get_btr_cur(&pcur);

			btr_rec_set_deleted_flag(
				btr_cur_get_rec(btr_cur),
				buf_block_get_page_zip(
					btr_cur_get_block(btr_cur)),
				TRUE);

			/* Void call just to set mtr modification flag
			to true failing which block is not scheduled for flush*/
			byte* log_ptr = mlog_open(&mtr, 0);
			ut_ad(log_ptr == NULL);
			if (log_ptr != NULL) {
				/* To keep complier happy. */
				mlog_close(&mtr, log_ptr);
			}

			btr_pcur_store_position(&pcur, &mtr);

			delete_entries.push_back(pcur);
		}
	}

	if (err != DB_SUCCESS || restore_delete) {

		/* Rollback half-way delete action that might have been
		applied to few of the indexes. */
		cursors_t::iterator	end = delete_entries.end();
		for (cursors_t::iterator it = delete_entries.begin();
		     it != end;
		     ++it) {

			ibool success = btr_pcur_restore_position(
				BTR_MODIFY_LEAF, &(*it), &mtr);

			if (!success) {
				ut_a(success);
			} else {
				btr_cur_t* btr_cur = btr_pcur_get_btr_cur(
					&(*it));

				ut_ad(btr_cur_get_block(
					btr_cur)->made_dirty_with_no_latch);

				btr_rec_set_deleted_flag(
					btr_cur_get_rec(btr_cur),
					buf_block_get_page_zip(
						btr_cur_get_block(btr_cur)),
					FALSE);

				/* Void call just to set mtr modification flag
				to true failing which block is not scheduled for
				flush. */
				byte* log_ptr = mlog_open(&mtr, 0);
				ut_ad(log_ptr == NULL);
				if (log_ptr != NULL) {
					/* To keep complier happy. */
					mlog_close(&mtr, log_ptr);
				}
			}
		}
	}

	mtr_commit(&mtr);

	mem_heap_free(heap);

	return(err);
}

/** Does an update of a row for MySQL by inserting new entry with update values.
@param[in]	node		update node carrying information to delete.
@param[out]	delete_entries	vector of cursor to deleted entries.
@param[in]	thr		thread handler
@return error code or DB_SUCCESS */
static
dberr_t
row_update_for_mysql_using_cursor(
	const upd_node_t*	node,
	cursors_t&		delete_entries,
	que_thr_t*		thr)
{
	dberr_t		err = DB_SUCCESS;
	dict_table_t*	table = node->table;
	mem_heap_t*	heap = mem_heap_create(1000);
	dtuple_t*	entry;
	dfield_t*	trx_id_field;

	/* Step-1: Update row-id column if table doesn't have unique index. */
	if (!dict_index_is_unique(dict_table_get_first_index(table))) {
		/* Update the row_id column. */
		dfield_t*	row_id_field;

		row_id_field = dtuple_get_nth_field(
			node->upd_row, dict_table_get_n_cols(table) - 2);

		dict_sys_write_row_id(
			static_cast<byte*>(row_id_field->data),
			dict_table_get_next_table_sess_row_id(node->table));
	}

	/* Step-2: Update the trx_id column. */
	trx_id_field = dtuple_get_nth_field(
		node->upd_row, dict_table_get_n_cols(table) - 1);
	trx_write_trx_id(static_cast<byte*>(trx_id_field->data),
			 dict_table_get_next_table_sess_trx_id(node->table));


	/* Step-3: Check if UPDATE can lead to DUPLICATE key violation.
	If yes, then avoid executing it and return error. Only after ensuring
	that UPDATE is safe execute it as we can't rollback. */
	for (dict_index_t* index = UT_LIST_GET_FIRST(table->indexes);
	     index != NULL && err == DB_SUCCESS;
	     index = UT_LIST_GET_NEXT(indexes, index)) {

		entry = row_build_index_entry(
			node->upd_row, node->upd_ext, index, heap);

		if (dict_index_is_clust(index)) {
			if (!dict_index_is_auto_gen_clust(index)) {
				err = row_ins_clust_index_entry(
					index, entry, thr,
					node->upd_ext
					? node->upd_ext->n_ext : 0,
					true);
			}
		} else {
			err = row_ins_sec_index_entry(index, entry, thr, true);
		}
	}

	if (err != DB_SUCCESS) {
		/* This suggest update can't be executed safely.
		Avoid executing update. Rollback DELETE action. */
		row_delete_for_mysql_using_cursor(node, delete_entries, true);
	}

	/* Step-4: It is now safe to execute update if there is no error */
	for (dict_index_t* index = UT_LIST_GET_FIRST(table->indexes);
	     index != NULL && err == DB_SUCCESS;
	     index = UT_LIST_GET_NEXT(indexes, index)) {

		entry = row_build_index_entry(
			node->upd_row, node->upd_ext, index, heap);

		if (dict_index_is_clust(index)) {
			err = row_ins_clust_index_entry(
				index, entry, thr,
				node->upd_ext ? node->upd_ext->n_ext : 0,
				false);
			/* Commit the open mtr as we are processing UPDATE. */
			if (index->last_ins_cur) {
				index->last_ins_cur->release();
			}
		} else {
			err = row_ins_sec_index_entry(index, entry, thr, false);
		}

		/* Too big record is valid error and suggestion is to use
		bigger page-size or different format. */
		ut_ad(err == DB_SUCCESS
		      || err == DB_TOO_BIG_RECORD
		      || err == DB_OUT_OF_FILE_SPACE);

		if (err == DB_TOO_BIG_RECORD) {
			row_delete_for_mysql_using_cursor(
				node, delete_entries, true);
		}
	}

	if (heap != NULL) {
		mem_heap_free(heap);
	}
	return(err);
}

/** Does an update or delete of a row for MySQL.
@param[in]	mysql_rec	row in the MySQL format
@param[in,out]	prebuilt	prebuilt struct in MySQL handle
@return error code or DB_SUCCESS */
static
dberr_t
row_del_upd_for_mysql_using_cursor(
	const byte*		mysql_rec,
	row_prebuilt_t*		prebuilt)
{
	dberr_t			err = DB_SUCCESS;
	upd_node_t*		node;
	cursors_t		delete_entries;
	dict_index_t*		clust_index;
	que_thr_t*		thr = NULL;

	/* Step-0: If there is cached insert position commit it before
	starting delete/update action as this can result in btree structure
	to change. */
	thr = que_fork_get_first_thr(prebuilt->upd_graph);
	clust_index = dict_table_get_first_index(prebuilt->table);
	if (clust_index->last_ins_cur) {
		clust_index->last_ins_cur->release();
	}

	/* Step-1: Select the appropriate cursor that will help build
	the original row and updated row. */
	node = prebuilt->upd_node;
	if (prebuilt->pcur->btr_cur.index == clust_index) {
		btr_pcur_copy_stored_position(node->pcur, prebuilt->pcur);
	} else {
		btr_pcur_copy_stored_position(node->pcur,
					      prebuilt->clust_pcur);
	}

	ut_ad(dict_table_is_intrinsic(prebuilt->table));
	ut_ad(!prebuilt->table->n_v_cols);

	/* Internal table is created by optimiser. So there
	should not be any virtual columns. */
	row_upd_store_row(node, NULL, NULL);

	/* Step-2: Execute DELETE operation. */
	err = row_delete_for_mysql_using_cursor(node, delete_entries, false);

	/* Step-3: If only DELETE operation then exit immediately. */
	if (node->is_delete) {
		if (err == DB_SUCCESS) {
			dict_table_n_rows_dec(prebuilt->table);
			srv_stats.n_rows_deleted.inc();
		}
	}

	if (err == DB_SUCCESS && !node->is_delete) {
		/* Step-4: Complete UPDATE operation by inserting new row with
		updated data. */
		err = row_update_for_mysql_using_cursor(
			node, delete_entries, thr);

		if (err == DB_SUCCESS) {
			srv_stats.n_rows_updated.inc();
		}
	}

	thr_get_trx(thr)->error_state = DB_SUCCESS;
	cursors_t::iterator	end = delete_entries.end();
	for (cursors_t::iterator it = delete_entries.begin(); it != end; ++it) {
		btr_pcur_close(&(*it));
	}

	return(err);
}

/** Does an update or delete of a row for MySQL.
@param[in]	mysql_rec	row in the MySQL format
@param[in,out]	prebuilt	prebuilt struct in MySQL handle
@return error code or DB_SUCCESS */
static
dberr_t
row_update_for_mysql_using_upd_graph(
	const byte*	mysql_rec,
	row_prebuilt_t*	prebuilt)
{
	trx_savept_t	savept;
	dberr_t		err;
	que_thr_t*	thr;
	ibool		was_lock_wait;
	dict_index_t*	clust_index;
	upd_node_t*	node;
	dict_table_t*	table		= prebuilt->table;
	trx_t*		trx		= prebuilt->trx;
	ulint		fk_depth	= 0;
	bool		got_s_lock	= false;

	DBUG_ENTER("row_update_for_mysql_using_upd_graph");

	ut_ad(trx);
	ut_a(prebuilt->magic_n == ROW_PREBUILT_ALLOCATED);
	ut_a(prebuilt->magic_n2 == ROW_PREBUILT_ALLOCATED);
	UT_NOT_USED(mysql_rec);

	if (prebuilt->table->ibd_file_missing) {
		ib::error() << "MySQL is trying to use a table handle but the"
			" .ibd file for table " << prebuilt->table->name
			<< " does not exist. Have you deleted"
			" the .ibd file from the database directory under"
			" the MySQL datadir, or have you used DISCARD"
			" TABLESPACE? " << TROUBLESHOOTING_MSG;
		DBUG_RETURN(DB_ERROR);
	}

	if(srv_force_recovery) {
		ib::error() << MODIFICATIONS_NOT_ALLOWED_MSG_FORCE_RECOVERY;
		DBUG_RETURN(DB_READ_ONLY);
	}

	DEBUG_SYNC_C("innodb_row_update_for_mysql_begin");

	trx->op_info = "updating or deleting";

	row_mysql_delay_if_needed();

	init_fts_doc_id_for_ref(table, &fk_depth);

	trx_start_if_not_started_xa(trx, true);

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

	if (prebuilt->pcur->btr_cur.index == clust_index) {
		btr_pcur_copy_stored_position(node->pcur, prebuilt->pcur);
	} else {
		btr_pcur_copy_stored_position(node->pcur,
					      prebuilt->clust_pcur);
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

	if (err != DB_SUCCESS) {

		que_thr_stop_for_mysql(thr);

		if (err == DB_RECORD_NOT_FOUND) {
			trx->error_state = DB_SUCCESS;
			trx->op_info = "";
			goto error;
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
		goto error;
	}


	que_thr_stop_for_mysql_no_error(thr, trx);

	if (dict_table_has_fts_index(table)
	    && trx->fts_next_doc_id != UINT64_UNDEFINED) {
		err = row_fts_update_or_delete(prebuilt);
		ut_ad(err == DB_SUCCESS);
		if (err != DB_SUCCESS) {
			goto error;
		}
	}

	/* Completed cascading operations (if any) */
	if (got_s_lock) {
		row_mysql_unfreeze_data_dictionary(trx);
	}

	if (node->is_delete) {
		/* Not protected by dict_table_stats_lock() for performance
		reasons, we would rather get garbage in stat_n_rows (which is
		just an estimate anyway) than protecting the following code
		with a latch. */
		dict_table_n_rows_dec(prebuilt->table);

		srv_stats.n_rows_deleted.inc();
	} else {
		srv_stats.n_rows_updated.inc();
	}

	/* We update table statistics only if it is a DELETE or UPDATE
	that changes indexed columns, UPDATEs that change only non-indexed
	columns would not affect statistics. */
	if (node->is_delete || !(node->cmpl_info & UPD_NODE_NO_ORD_CHANGE)) {
		row_update_statistics_if_needed(prebuilt->table);
	}

	trx->op_info = "";

	DBUG_RETURN(err);

error:
	if (got_s_lock) {
		row_mysql_unfreeze_data_dictionary(trx);
	}
	DBUG_RETURN(err);
}

/** Does an update or delete of a row for MySQL.
@param[in]	mysql_rec	row in the MySQL format
@param[in,out]	prebuilt	prebuilt struct in MySQL handle
@return error code or DB_SUCCESS */
dberr_t
row_update_for_mysql(
	const byte*		mysql_rec,
	row_prebuilt_t*		prebuilt)
{
	if (dict_table_is_intrinsic(prebuilt->table)) {
		return(row_del_upd_for_mysql_using_cursor(mysql_rec, prebuilt));
	} else {
		ut_a(prebuilt->template_type == ROW_MYSQL_WHOLE_ROW);
		return(row_update_for_mysql_using_upd_graph(
			mysql_rec, prebuilt));
	}
}

/** Delete all rows for the given table by freeing/truncating indexes.
@param[in,out]	table	table handler
@return error code or DB_SUCCESS */
dberr_t
row_delete_all_rows(
	dict_table_t*	table)
{
	dberr_t		err = DB_SUCCESS;
	dict_index_t*	index;


	index = dict_table_get_first_index(table);
	/* Step-0: If there is cached insert position along with mtr
	commit it before starting delete/update action. */
	if (index->last_ins_cur) {
		index->last_ins_cur->release();
	}

	/* Step-1: Now truncate all the indexes and re-create them.
	Note: This is ddl action even though delete all rows is
	DML action. Any error during this action is ir-reversible. */
	for (index = UT_LIST_GET_FIRST(table->indexes);
	     index != NULL && err == DB_SUCCESS;
	     index = UT_LIST_GET_NEXT(indexes, index)) {

		err = dict_truncate_index_tree_in_mem(index);
		// TODO: what happen if get an error
		ut_ad(err == DB_SUCCESS);
	}

	return(err);
}

/** This can only be used when srv_locks_unsafe_for_binlog is TRUE or this
session is using a READ COMMITTED or READ UNCOMMITTED isolation level.
Before calling this function row_search_for_mysql() must have
initialized prebuilt->new_rec_locks to store the information which new
record locks really were set. This function removes a newly set
clustered index record lock under prebuilt->pcur or
prebuilt->clust_pcur.  Thus, this implements a 'mini-rollback' that
releases the latest clustered index record lock we set.
@param[in,out]	prebuilt		prebuilt struct in MySQL handle
@param[in]	has_latches_on_recs	TRUE if called so that we have the
					latches on the records under pcur
					and clust_pcur, and we do not need
					to reposition the cursors. */
void
row_unlock_for_mysql(
	row_prebuilt_t*	prebuilt,
	ibool		has_latches_on_recs)
{
	btr_pcur_t*	pcur		= prebuilt->pcur;
	btr_pcur_t*	clust_pcur	= prebuilt->clust_pcur;
	trx_t*		trx		= prebuilt->trx;

	ut_ad(prebuilt != NULL);
	ut_ad(trx != NULL);

	if (UNIV_UNLIKELY
	    (!srv_locks_unsafe_for_binlog
	     && trx->isolation_level > TRX_ISO_READ_COMMITTED)) {

		ib::error() << "Calling row_unlock_for_mysql though"
			" innodb_locks_unsafe_for_binlog is FALSE and this"
			" session is not using READ COMMITTED isolation"
			" level.";
		return;
	}
	if (dict_index_is_spatial(prebuilt->index)) {
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
@return error code or DB_SUCCESS */
dberr_t
row_update_cascade_for_mysql(
/*=========================*/
        que_thr_t*      thr,    /*!< in: query thread */
        upd_node_t*     node,   /*!< in: update node used in the cascade
                                or set null operation */
        dict_table_t*   table)  /*!< in: table where we do the operation */
{
        dberr_t err;
        trx_t*  trx;

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
	TABLE *temp = thr->prebuilt->m_mysql_table;
	thr->prebuilt->m_mysql_table = NULL ;
        row_upd_step(thr);
	thr->prebuilt->m_mysql_table = temp;
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
@return TRUE if the clustered index was generated automatically */
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
void
row_mysql_freeze_data_dictionary_func(
/*==================================*/
	trx_t*		trx,	/*!< in/out: transaction */
	const char*	file,	/*!< in: file name */
	ulint		line)	/*!< in: line number */
{
	ut_a(trx->dict_operation_lock_mode == 0);

	rw_lock_s_lock_inline(dict_operation_lock, 0, file, line);

	trx->dict_operation_lock_mode = RW_S_LATCH;
}

/*********************************************************************//**
Unlocks the data dictionary shared lock. */
void
row_mysql_unfreeze_data_dictionary(
/*===============================*/
	trx_t*	trx)	/*!< in/out: transaction */
{
	ut_ad(lock_trx_has_sys_table_locks(trx) == NULL);

	ut_a(trx->dict_operation_lock_mode == RW_S_LATCH);

	rw_lock_s_unlock(dict_operation_lock);

	trx->dict_operation_lock_mode = 0;
}

/*********************************************************************//**
Locks the data dictionary exclusively for performing a table create or other
data dictionary modification operation. */
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

	rw_lock_x_lock_inline(dict_operation_lock, 0, file, line);
	trx->dict_operation_lock_mode = RW_X_LATCH;

	mutex_enter(&dict_sys->mutex);
}

/*********************************************************************//**
Unlocks the data dictionary exclusive lock. */
void
row_mysql_unlock_data_dictionary(
/*=============================*/
	trx_t*	trx)	/*!< in/out: transaction */
{
	ut_ad(lock_trx_has_sys_table_locks(trx) == NULL);

	ut_a(trx->dict_operation_lock_mode == RW_X_LATCH);

	/* Serialize data dictionary operations with dictionary mutex:
	no deadlocks can occur then in these operations */

	mutex_exit(&dict_sys->mutex);
	rw_lock_x_unlock(dict_operation_lock);

	trx->dict_operation_lock_mode = 0;
}

/*********************************************************************//**
Creates a table for MySQL. On failure the transaction will be rolled back
and the 'table' object will be freed.
@return error code or DB_SUCCESS */
dberr_t
row_create_table_for_mysql(
/*=======================*/
	dict_table_t*	table,	/*!< in, own: table definition
				(will be freed, or on DB_SUCCESS
				added to the data dictionary cache) */
	const char*	compression,
				/*!< in: compression algorithm to use,
				can be NULL */
	trx_t*		trx,	/*!< in/out: transaction */
	bool		commit)	/*!< in: if true, commit the transaction */
{
	tab_node_t*	node;
	mem_heap_t*	heap;
	que_thr_t*	thr;
	dberr_t		err;

	ut_ad(rw_lock_own(dict_operation_lock, RW_LOCK_X));
	ut_ad(mutex_own(&dict_sys->mutex));
	ut_ad(trx->dict_operation_lock_mode == RW_X_LATCH);

	DBUG_EXECUTE_IF(
		"ib_create_table_fail_at_start_of_row_create_table_for_mysql",
		goto err_exit;
	);

	trx->op_info = "creating table";

	if (row_mysql_is_system_table(table->name.m_name)) {

		ib::error() << "Trying to create a MySQL system table "
			<< table->name << " of type InnoDB. MySQL system"
			" tables must be of the MyISAM type!";
#ifndef DBUG_OFF
err_exit:
#endif /* !DBUG_OFF */
		dict_mem_table_free(table);

		if (commit) {
			trx_commit_for_mysql(trx);
		}

		trx->op_info = "";

		return(DB_ERROR);
	}

	trx_start_if_not_started_xa(trx, true);

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
		ut_ad(strstr(table->name.m_name, "/FTS_") != NULL);
	}

	node = tab_create_graph_create(table, heap);

	thr = pars_complete_graph_for_exec(node, trx, heap, NULL);

	ut_a(thr == que_fork_start_command(
			static_cast<que_fork_t*>(que_node_get_parent(thr))));

	que_run_threads(thr);

	err = trx->error_state;

	/* Update SYS_TABLESPACES and SYS_DATAFILES if a new file-per-table
	tablespace was created. */
	if (err == DB_SUCCESS && dict_table_is_file_per_table(table)) {

		ut_ad(dict_table_is_file_per_table(table));

		char*	path;
		path = fil_space_get_first_path(table->space);

		err = dict_replace_tablespace_in_dictionary(
			table->space, table->name.m_name,
			fil_space_get_flags(table->space),
			path, trx, commit);

			ut_free(path);

		if (err != DB_SUCCESS) {

			/* We must delete the link file. */
			RemoteDatafile::delete_link_file(table->name.m_name);

		} else if (compression != NULL && compression[0] != '\0') {

			ut_ad(!dict_table_in_shared_tablespace(table));

			ut_ad(Compression::validate(compression) == DB_SUCCESS);

			err = fil_set_compression(table, compression);

			switch (err) {
			case DB_SUCCESS:
				break;
			case DB_NOT_FOUND:
			case DB_UNSUPPORTED:
			case DB_IO_NO_PUNCH_HOLE_FS:
				/* Return these errors */
				break;
			case DB_IO_NO_PUNCH_HOLE_TABLESPACE:
				/* Page Compression will not be used. */
				err = DB_SUCCESS;
				break;
			default:
				ut_error;
			}

			/* We can check for file system punch hole support
                        only after creating the tablespace. On Windows
			we can query that information but not on Linux. */
			ut_ad(err == DB_SUCCESS
				|| err == DB_IO_NO_PUNCH_HOLE_FS);

			/* In non-strict mode we ignore dodgy compression
			settings. */
		}
	}

	switch (err) {
	case DB_SUCCESS:
	case DB_IO_NO_PUNCH_HOLE_FS:
		break;
	case DB_OUT_OF_FILE_SPACE:
		trx->error_state = DB_SUCCESS;
		trx_rollback_to_savepoint(trx, NULL);

		ib::warn() << "Cannot create table "
			<< table->name
			<< " because tablespace full";

		if (dict_table_open_on_name(table->name.m_name, TRUE, FALSE,
					    DICT_ERR_IGNORE_NONE)) {

			dict_table_close_and_drop(trx, table);

			if (commit) {
				trx_commit_for_mysql(trx);
			}
		} else {
			dict_mem_table_free(table);
		}

		break;

	case DB_UNSUPPORTED:
	case DB_TOO_MANY_CONCURRENT_TRXS:
		/* We already have .ibd file here. it should be deleted. */

		if (dict_table_is_file_per_table(table)
		    && fil_delete_tablespace(
			    table->space,
			    BUF_REMOVE_FLUSH_NO_WRITE)
		    != DB_SUCCESS) {

			ib::error() << "Not able to delete tablespace "
				<< table->space << " of table "
				<< table->name << "!";
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
@return error number or DB_SUCCESS */
dberr_t
row_create_index_for_mysql(
/*=======================*/
	dict_index_t*	index,		/*!< in, own: index definition
					(will be freed) */
	trx_t*		trx,		/*!< in: transaction handle */
	const ulint*	field_lengths,	/*!< in: if not NULL, must contain
					dict_index_get_n_fields(index)
					actual field lengths for the
					index columns, which are
					then checked for not being too
					large. */
	dict_table_t*	handler)	/*!< in/out: table handler. */
{
	ind_node_t*	node;
	mem_heap_t*	heap;
	que_thr_t*	thr;
	dberr_t		err;
	ulint		i;
	ulint		len;
	char*		table_name;
	char*		index_name;
	dict_table_t*	table = NULL;
	ibool		is_fts;

	trx->op_info = "creating index";

	/* Copy the table name because we may want to drop the
	table later, after the index object is freed (inside
	que_run_threads()) and thus index->table_name is not available. */
	table_name = mem_strdup(index->table_name);
	index_name = mem_strdup(index->name);

	is_fts = (index->type == DICT_FTS);

	if (handler != NULL && dict_table_is_intrinsic(handler)) {
		table = handler;
	}

	if (table == NULL) {

		ut_ad(rw_lock_own(dict_operation_lock, RW_LOCK_X));
		ut_ad(mutex_own(&dict_sys->mutex));

		table = dict_table_open_on_name(table_name, TRUE, TRUE,
						DICT_ERR_IGNORE_NONE);

	} else {
		table->acquire();
		ut_ad(dict_table_is_intrinsic(table));
	}

	if (!dict_table_is_temporary(table)) {
		trx_start_if_not_started_xa(trx, true);
	}

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

	trx_set_dict_operation(trx, TRX_DICT_OP_TABLE);

	/* For temp-table we avoid insertion into SYSTEM TABLES to
	maintain performance and so we have separate path that directly
	just updates dictonary cache. */
	if (!dict_table_is_temporary(table)) {
		/* Note that the space id where we store the index is
		inherited from the table in dict_build_index_def_step()
		in dict0crea.cc. */

		heap = mem_heap_create(512);

		node = ind_create_graph_create(index, heap, NULL);

		thr = pars_complete_graph_for_exec(node, trx, heap, NULL);

		ut_a(thr == que_fork_start_command(
				static_cast<que_fork_t*>(
					que_node_get_parent(thr))));

		que_run_threads(thr);

		err = trx->error_state;

		que_graph_free((que_t*) que_node_get_parent(thr));
	} else {
		dict_build_index_def(table, index, trx);

		index_id_t index_id = index->id;

		/* add index to dictionary cache and also free index object.
		We allow instrinsic table to violate the size limits because
		they are used by optimizer for all record formats. */
		err = dict_index_add_to_cache(
			table, index, FIL_NULL,
			!dict_table_is_intrinsic(table)
			&& trx_is_strict(trx));

		if (err != DB_SUCCESS) {
			goto error_handling;
		}

		/* as above function has freed index object re-load it
		now from dictionary cache using index_id */
		if (!dict_table_is_intrinsic(table)) {
			index = dict_index_get_if_in_cache_low(index_id);
		} else {
			index = dict_table_find_index_on_id(table, index_id);

			/* trx_id field is used for tracking which transaction
			created the index. For intrinsic table this is
			ir-relevant and so re-use it for tracking consistent
			view while processing SELECT as part of UPDATE. */
			index->trx_id = ULINT_UNDEFINED;
		}
		ut_a(index != NULL);
		index->table = table;

		err = dict_create_index_tree_in_mem(index, trx);

		if (err != DB_SUCCESS && !dict_table_is_intrinsic(table)) {
			dict_index_remove_from_cache(table, index);
		}
	}

	/* Create the index specific FTS auxiliary tables. */
	if (err == DB_SUCCESS && is_fts) {
		dict_index_t*	idx;

		idx = dict_table_get_index_on_name(table, index_name);

		ut_ad(idx);
		err = fts_create_index_tables_low(
			trx, idx, table->name.m_name, table->id);
	}

error_handling:
	dict_table_close(table, TRUE, FALSE);

	if (err != DB_SUCCESS) {
		/* We have special error handling here */

		trx->error_state = DB_SUCCESS;

		if (trx_is_started(trx)) {

			trx_rollback_to_savepoint(trx, NULL);
		}

		row_drop_table_for_mysql(table_name, trx, FALSE, true, handler);

		if (trx_is_started(trx)) {

			trx_commit_for_mysql(trx);
		}

		trx->error_state = DB_SUCCESS;
	}

	trx->op_info = "";

	ut_free(table_name);
	ut_free(index_name);

	return(err);
}

/*********************************************************************//**
Scans a table create SQL string and adds to the data dictionary
the foreign key constraints declared in the string. This function
should be called after the indexes for a table have been created.
Each foreign key constraint must be accompanied with indexes in
bot participating tables. The indexes are allowed to contain more
fields than mentioned in the constraint.

@param[in]	trx		transaction
@param[in]	sql_string	table create statement where
				foreign keys are declared like:
				FOREIGN KEY (a, b) REFERENCES table2(c, d),
				table2 can be written also with the database
				name before it: test.table2; the default
				database id the database of parameter name
@param[in]	sql_length	length of sql_string
@param[in]	name		table full name in normalized form
@param[in]	reject_fks	if TRUE, fail with error code
				DB_CANNOT_ADD_CONSTRAINT if any
				foreign keys are found.
@return error code or DB_SUCCESS */
dberr_t
row_table_add_foreign_constraints(
	trx_t*			trx,
	const char*		sql_string,
	size_t			sql_length,
	const char*		name,
	ibool			reject_fks)
{
	dberr_t	err;

	DBUG_ENTER("row_table_add_foreign_constraints");

	ut_ad(mutex_own(&dict_sys->mutex));
	ut_ad(rw_lock_own(dict_operation_lock, RW_LOCK_X));
	ut_a(sql_string);

	trx->op_info = "adding foreign keys";

	trx_start_if_not_started_xa(trx, true);

	trx_set_dict_operation(trx, TRX_DICT_OP_TABLE);

	err = dict_create_foreign_constraints(
		trx, sql_string, sql_length, name, reject_fks);

	DBUG_EXECUTE_IF("ib_table_add_foreign_fail",
			err = DB_DUPLICATE_KEY;);

	DEBUG_SYNC_C("table_add_foreign_constraints");

	/* Check like this shouldn't be done for table that doesn't
	have foreign keys but code still continues to run with void action.
	Disable it for intrinsic table at-least */
	if (err == DB_SUCCESS) {
		/* Check that also referencing constraints are ok */
		dict_names_t	fk_tables;
		err = dict_load_foreigns(name, NULL, false, true,
					 DICT_ERR_IGNORE_NONE, fk_tables);

		while (err == DB_SUCCESS && !fk_tables.empty()) {
			dict_load_table(fk_tables.front(), true,
					DICT_ERR_IGNORE_NONE);
			fk_tables.pop_front();
		}
	}

	if (err != DB_SUCCESS) {
		/* We have special error handling here */

		trx->error_state = DB_SUCCESS;

		if (trx_is_started(trx)) {

			trx_rollback_to_savepoint(trx, NULL);
		}

		row_drop_table_for_mysql(name, trx, FALSE, true);

		if (trx_is_started(trx)) {

			trx_commit_for_mysql(trx);
		}

		trx->error_state = DB_SUCCESS;
	}

	DBUG_RETURN(err);
}

/*********************************************************************//**
Drops a table for MySQL as a background operation. MySQL relies on Unix
in ALTER TABLE to the fact that the table handler does not remove the
table before all handles to it has been removed. Furhermore, the MySQL's
call to drop table must be non-blocking. Therefore we do the drop table
as a background operation, which is taken care of by the master thread
in srv0srv.cc.
@return error code or DB_SUCCESS */
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

	trx->check_foreigns = false;

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
@return how many tables dropped + remaining tables in list */
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

	DBUG_EXECUTE_IF("row_drop_tables_in_background_sleep",
		os_thread_sleep(5000000);
	);

	table = dict_table_open_on_name(drop->table_name, FALSE, FALSE,
					DICT_ERR_IGNORE_NONE);

	if (table == NULL) {
		/* If for some reason the table has already been dropped
		through some other mechanism, do not try to drop it */

		goto already_dropped;
	}

	if (!table->to_be_dropped) {
		/* There is a scenario: the old table is dropped
		just after it's added into drop list, and new
		table with the same name is created, then we try
		to drop the new table in background. */
		dict_table_close(table, FALSE, FALSE);

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

	UT_LIST_REMOVE(row_mysql_drop_list, drop);

	MONITOR_DEC(MONITOR_BACKGROUND_DROP_TABLE);

	ib::info() << "Dropped table "
		<< ut_get_name(NULL, drop->table_name)
		<< " in background drop queue.",

	ut_free(drop->table_name);

	ut_free(drop);

	mutex_exit(&row_drop_list_mutex);

	goto loop;
}

/*********************************************************************//**
Get the background drop list length. NOTE: the caller must own the
drop list mutex!
@return how many tables in list */
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
@return TRUE if the table was not yet in the drop list, and was added there */
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
		ut_malloc_nokey(sizeof(row_mysql_drop_t)));

	drop->table_name = mem_strdup(name);

	UT_LIST_ADD_LAST(row_mysql_drop_list, drop);

	MONITOR_INC(MONITOR_BACKGROUND_DROP_TABLE);

	mutex_exit(&row_drop_list_mutex);

	return(TRUE);
}

/** Reassigns the table identifier of a table.
@param[in,out]	table	table
@param[in,out]	trx	transaction
@param[out]	new_id	new table id
@return error code or DB_SUCCESS */
dberr_t
row_mysql_table_id_reassign(
	dict_table_t*	table,
	trx_t*		trx,
	table_id_t*	new_id)
{
	dberr_t		err;
	pars_info_t*	info	= pars_info_create();

	dict_hdr_get_new_id(new_id, NULL, NULL, table, false);

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
		"UPDATE SYS_VIRTUAL SET TABLE_ID = :new_id\n"
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

	trx_start_if_not_started_xa(trx, true);

	/* Serialize data dictionary operations with dictionary mutex:
	this is to avoid deadlocks during data dictionary operations */

	row_mysql_lock_data_dictionary(trx);

	dict_table_t*	table;

	table = dict_table_open_on_name(
		name, TRUE, FALSE, DICT_ERR_IGNORE_NONE);

	if (table) {
		dict_stats_wait_bg_to_stop_using_table(table, trx);
		ut_a(!is_system_tablespace(table->space));
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

	if (srv_read_only_mode || !trx->check_foreigns) {
		return(DB_SUCCESS);
	}

	/* Check if the table is referenced by foreign key constraints from
	some other table (not the table itself) */
	dict_foreign_set::iterator	it
		= std::find_if(table->referenced_set.begin(),
			       table->referenced_set.end(),
			       dict_foreign_different_tables());

	if (it == table->referenced_set.end()) {
		return(DB_SUCCESS);
	}

	const dict_foreign_t*	foreign	= *it;
	FILE*			ef	= dict_foreign_err_file;

	ut_ad(foreign->foreign_table != table);
	ut_ad(foreign->referenced_table == table);

	/* We only allow discarding a referenced table if
	FOREIGN_KEY_CHECKS is set to 0 */

	mutex_enter(&dict_foreign_err_mutex);

	rewind(ef);

	ut_print_timestamp(ef);

	fputs("  Cannot DISCARD table ", ef);
	ut_print_name(ef, trx, table->name.m_name);
	fputs("\n"
	      "because it is referenced by ", ef);
	ut_print_name(ef, trx, foreign->foreign_table_name);
	putc('\n', ef);

	mutex_exit(&dict_foreign_err_mutex);

	return(DB_CANNOT_DROP_CONSTRAINT);
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

	/* Set the TABLESPACE DISCARD flag in the table definition
	on disk. */
	err = row_import_update_discarded_flag(
		trx, table->id, true, true);

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

	/* For encrypted table, before we discard the tablespace,
	we need save the encryption information into table, otherwise,
	this information will be lost in fil_discard_tablespace along
	with fil_space_free(). */
	if (dict_table_is_encrypted(table)) {
		ut_ad(table->encryption_key == NULL
		      && table->encryption_iv == NULL);

		table->encryption_key =
			static_cast<byte*>(mem_heap_alloc(table->heap,
							  ENCRYPTION_KEY_LEN));

		table->encryption_iv =
			static_cast<byte*>(mem_heap_alloc(table->heap,
							  ENCRYPTION_KEY_LEN));

		fil_space_t*	space = fil_space_get(table->space);
		ut_ad(FSP_FLAGS_GET_ENCRYPTION(space->flags));

		memcpy(table->encryption_key,
		       space->encryption_key,
		       ENCRYPTION_KEY_LEN);
		memcpy(table->encryption_iv,
		       space->encryption_iv,
		       ENCRYPTION_KEY_LEN);
	}

	/* Discard the physical file that is used for the tablespace. */

	err = fil_discard_tablespace(table->space);

	switch (err) {
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
@return error code or DB_SUCCESS */
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
	} else if (dict_table_is_temporary(table)) {

		ib_senderrf(trx->mysql_thd, IB_LOG_LEVEL_ERROR,
			    ER_CANNOT_DISCARD_TEMPORARY_TABLE);

		err = DB_ERROR;

	} else if (table->space == srv_sys_space.space_id()) {
		char	table_name[MAX_FULL_NAME_LEN + 1];

		innobase_format_name(
			table_name, sizeof(table_name),
			table->name.m_name);

		ib_senderrf(trx->mysql_thd, IB_LOG_LEVEL_ERROR,
			    ER_TABLE_IN_SYSTEM_TABLESPACE, table_name);

		err = DB_ERROR;

	} else if (table->n_foreign_key_checks_running > 0) {
		char	table_name[MAX_FULL_NAME_LEN + 1];

		innobase_format_name(
			table_name, sizeof(table_name),
			table->name.m_name);

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
@return error code or DB_SUCCESS */
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
	thr = pars_complete_graph_for_exec(node, trx, heap, NULL);
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

/** Drop ancillary FTS tables as part of dropping a table.
@param[in,out]	table		Table cache entry
@param[in,out]	trx		Transaction handle
@return error code or DB_SUCCESS */
UNIV_INLINE
dberr_t
row_drop_ancillary_fts_tables(
	dict_table_t*	table,
	trx_t*		trx)
{
	/* Drop ancillary FTS tables */
	if (dict_table_has_fts_index(table)
	    || DICT_TF2_FLAG_IS_SET(table, DICT_TF2_FTS_HAS_DOC_ID)) {

		ut_ad(table->get_ref_count() == 0);
		ut_ad(trx_is_started(trx));

		dberr_t err = fts_drop_tables(trx, table);

		if (err != DB_SUCCESS) {
			ib::error() << " Unable to remove ancillary FTS"
				" tables for table "
				<< table->name << " : " << ut_strerr(err);

			return(err);
		}
	}

	/* The table->fts flag can be set on the table for which
	the cluster index is being rebuilt. Such table might not have
	DICT_TF2_FTS flag set. So keep this out of above
	dict_table_has_fts_index condition */
	if (table->fts != NULL) {
		/* Need to set TABLE_DICT_LOCKED bit, since
		fts_que_graph_free_check_lock would try to acquire
		dict mutex lock */
		table->fts->fts_status |= TABLE_DICT_LOCKED;

		fts_free(table);
	}

	return(DB_SUCCESS);
}

/** Drop a table from the memory cache as part of dropping a table.
@param[in]	tablename	A copy of table->name. Used when table == null
@param[in,out]	table		Table cache entry
@param[in,out]	trx		Transaction handle
@return error code or DB_SUCCESS */
UNIV_INLINE
dberr_t
row_drop_table_from_cache(
	const char*	tablename,
	dict_table_t*	table,
	trx_t*		trx)
{
	dberr_t	err = DB_SUCCESS;
	bool	is_temp = dict_table_is_temporary(table);

	/* Remove the pointer to this table object from the list
	of modified tables by the transaction because the object
	is going to be destroyed below. */
	trx->mod_tables.erase(table);

	if (!dict_table_is_intrinsic(table)) {
		dict_table_remove_from_cache(table);
	} else {
		for (dict_index_t* index = UT_LIST_GET_FIRST(table->indexes);
		     index != NULL;
		     index = UT_LIST_GET_FIRST(table->indexes)) {

			rw_lock_free(&index->lock);

			UT_LIST_REMOVE(table->indexes, index);

			dict_mem_index_free(index);
		}

		dict_mem_table_free(table);
		table = NULL;
	}

	if (!is_temp
	    && dict_load_table(tablename, true,
			       DICT_ERR_IGNORE_NONE) != NULL) {
		ib::error() << "Not able to remove table "
			<< ut_get_name(trx, tablename)
			<< " from the dictionary cache!";
		err = DB_ERROR;
	}

	return(err);
}

/** Drop a single-table tablespace as part of dropping or renaming a table.
This deletes the fil_space_t if found and the file on disk.
@param[in]	space_id	Tablespace ID
@param[in]	tablename	Table name, same as the tablespace name
@param[in]	filepath	File path of tablespace to delete
@param[in]	is_temp		Is this a temporary table/tablespace
@param[in]	is_encrypted	Is this an encrypted table/tablespace
@param[in]	trx		Transaction handle
@return error code or DB_SUCCESS */
UNIV_INLINE
dberr_t
row_drop_single_table_tablespace(
	ulint		space_id,
	const char*	tablename,
	const char*	filepath,
	bool		is_temp,
	bool		is_encrypted,
	trx_t*		trx)
{
	dberr_t	err = DB_SUCCESS;

	/* This might be a temporary single-table tablespace if the table
	is compressed and temporary. If so, don't spam the log when we
	delete one of these or if we can't find the tablespace. */
	bool	print_msg = !is_temp && !is_encrypted;

	/* If the tablespace is not in the cache, just delete the file. */
	if (!fil_space_for_table_exists_in_mem(
		    space_id, tablename, print_msg, false, NULL, 0)) {

		/* Force a delete of any discarded or temporary files. */
		fil_delete_file(filepath);

		if (print_msg) {
			ib::info() << "Removed datafile " << filepath
				<< " for table " << tablename;
		}

	} else if (fil_delete_tablespace(space_id, BUF_REMOVE_FLUSH_NO_WRITE)
		   != DB_SUCCESS) {

		ib::error() << "We removed the InnoDB internal data"
			" dictionary entry of table " << tablename
			<< " but we are not able to delete the tablespace "
			<< space_id << " file " << filepath << "!";

		err = DB_ERROR;
	}

	return(err);
}

/** Drop a table for MySQL.
If the data dictionary was not already locked by the transaction,
the transaction will be committed.  Otherwise, the data dictionary
will remain locked.
@param[in]	name		Table name
@param[in]	trx		Transaction handle
@param[in]	drop_db		true=dropping whole database
@param[in]	nonatomic	Whether it is permitted to release
and reacquire dict_operation_lock
@param[in,out]	handler		Table handler
@return error code or DB_SUCCESS */
dberr_t
row_drop_table_for_mysql(
	const char*	name,
	trx_t*		trx,
	bool		drop_db,
	bool		nonatomic,
	dict_table_t*	handler)
{
	dberr_t		err;
	dict_foreign_t*	foreign;
	dict_table_t*	table			= NULL;
	char*		filepath		= NULL;
	char*		tablename		= NULL;
	bool		locked_dictionary	= false;
	pars_info_t*	info			= NULL;
	mem_heap_t*	heap			= NULL;
	bool		is_intrinsic_temp_table	= false;

	DBUG_ENTER("row_drop_table_for_mysql");
	DBUG_PRINT("row_drop_table_for_mysql", ("table: '%s'", name));

	ut_a(name != NULL);

	/* Serialize data dictionary operations with dictionary mutex:
	no deadlocks can occur then in these operations */

	trx->op_info = "dropping table";

	if (handler != NULL && dict_table_is_intrinsic(handler)) {
		table = handler;
		is_intrinsic_temp_table = true;
	}

	if (table == NULL) {

		if (trx->dict_operation_lock_mode != RW_X_LATCH) {
			/* Prevent foreign key checks etc. while we are
			dropping the table */

			row_mysql_lock_data_dictionary(trx);

			locked_dictionary = true;
			nonatomic = true;
		}

		ut_ad(mutex_own(&dict_sys->mutex));
		ut_ad(rw_lock_own(dict_operation_lock, RW_LOCK_X));

		table = dict_table_open_on_name(
			name, TRUE, FALSE,
			static_cast<dict_err_ignore_t>(
				DICT_ERR_IGNORE_INDEX_ROOT
				| DICT_ERR_IGNORE_CORRUPT));
	} else {
		table->acquire();
		ut_ad(dict_table_is_intrinsic(table));
	}

	if (!table) {
		err = DB_TABLE_NOT_FOUND;
		goto funct_exit;
	}

	/* This function is called recursively via fts_drop_tables(). */
	if (!trx_is_started(trx)) {

		if (!dict_table_is_temporary(table)) {
			trx_start_for_ddl(trx, TRX_DICT_OP_TABLE);
		} else {
			trx_set_dict_operation(trx, TRX_DICT_OP_TABLE);
		}
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

			for (;;) {
				bool retry = false;
				if (dict_fts_index_syncing(table)) {
					retry = true;
				}
				if (!retry) {
			        break;
				}
				DICT_BG_YIELD(trx);
			}

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
		RemoteDatafile::delete_link_file(name);
	}

	if (!dict_table_is_temporary(table)) {

		dict_stats_recalc_pool_del(table);

		/* Remove stats for this table and all of its indexes from the
		persistent storage if it exists and if there are stats for this
		table in there. This function creates its own trx and commits
		it. */
		if (dict_stats_is_persistent_enabled(table)) {
			char	errstr[1024];
			err = dict_stats_drop_table(name, errstr, sizeof(errstr));
			if (err != DB_SUCCESS) {
				ib::warn() << errstr;
			}
		}
	}

	if (!dict_table_is_intrinsic(table)) {
		dict_table_prevent_eviction(table);
	}

	dict_table_close(table, TRUE, FALSE);

	/* Check if the table is referenced by foreign key constraints from
	some other table (not the table itself) */

	if (!srv_read_only_mode && trx->check_foreigns) {

		for (dict_foreign_set::iterator it
			= table->referenced_set.begin();
		     it != table->referenced_set.end();
		     ++it) {

			foreign = *it;

			const bool	ref_ok = drop_db
				&& dict_tables_have_same_db(
					name,
					foreign->foreign_table_name_lookup);

			if (foreign->foreign_table != table && !ref_ok) {

				FILE*	ef	= dict_foreign_err_file;

				/* We only allow dropping a referenced table
				if FOREIGN_KEY_CHECKS is set to 0 */

				err = DB_CANNOT_DROP_CONSTRAINT;

				mutex_enter(&dict_foreign_err_mutex);
				rewind(ef);
				ut_print_timestamp(ef);

				fputs("  Cannot drop table ", ef);
				ut_print_name(ef, trx, name);
				fputs("\n"
				      "because it is referenced by ", ef);
				ut_print_name(ef, trx,
					      foreign->foreign_table_name);
				putc('\n', ef);
				mutex_exit(&dict_foreign_err_mutex);

				goto funct_exit;
			}
		}
	}


	DBUG_EXECUTE_IF("row_drop_table_add_to_background",
		row_add_table_to_background_drop_list(table->name.m_name);
		err = DB_SUCCESS;
		goto funct_exit;
	);

	/* TODO: could we replace the counter n_foreign_key_checks_running
	with lock checks on the table? Acquire here an exclusive lock on the
	table, and rewrite lock0lock.cc and the lock wait in srv0srv.cc so that
	they can cope with the table having been dropped here? Foreign key
	checks take an IS or IX lock on the table. */

	if (table->n_foreign_key_checks_running > 0) {

		const char*	save_tablename = table->name.m_name;
		ibool		added;

		added = row_add_table_to_background_drop_list(save_tablename);

		if (added) {
			ib::info() << "You are trying to drop table "
				<< table->name
				<< " though there is a foreign key check"
				" running on it. Adding the table to the"
				" background drop queue.";

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
	are no references to the table but it has record locks, we release
	the record locks unconditionally. One use case is:

		CREATE TABLE t2 (PRIMARY KEY (a)) SELECT * FROM t1;

	If after the user transaction has done the SELECT and there is a
	problem in completing the CREATE TABLE operation, MySQL will drop
	the table. InnoDB will create a new background transaction to do the
	actual drop, the trx instance that is passed to this function. To
	preserve existing behaviour we remove the locks but ideally we
	shouldn't have to. There should never be record locks on a table
	that is going to be dropped. */

	if (table->get_ref_count() == 0) {
		/* We don't take lock on intrinsic table so nothing to remove.*/
		if (!dict_table_is_intrinsic(table)) {
			lock_remove_all_on_table(table, TRUE);
		}
		ut_a(table->n_rec_locks == 0);
	} else if (table->get_ref_count() > 0 || table->n_rec_locks > 0) {
		ibool	added;

		ut_ad(!dict_table_is_intrinsic(table));

		added = row_add_table_to_background_drop_list(
			table->name.m_name);

		if (added) {
			ib::info() << "MySQL is trying to drop table "
				<< table->name
				<< " though there are still open handles to"
				" it. Adding the table to the background drop"
				" queue.";

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

	ut_a(dict_table_is_intrinsic(table) || !lock_table_has_locks(table));

	switch (trx_get_dict_operation(trx)) {
	case TRX_DICT_OP_NONE:
		trx_set_dict_operation(trx, TRX_DICT_OP_TABLE);
		trx->table_id = table->id;
	case TRX_DICT_OP_TABLE:
		break;
	case TRX_DICT_OP_INDEX:
		/* If the transaction was previously flagged as
		TRX_DICT_OP_INDEX, we should be dropping auxiliary
		tables for full-text indexes or temp tables. */
		ut_ad(strstr(table->name.m_name, "/FTS_") != NULL
		      || strstr(table->name.m_name, TEMP_FILE_PREFIX_INNODB)
		      != NULL);
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

	/* As we don't insert entries to SYSTEM TABLES for temp-tables
	we need to avoid running removal of these entries. */
	if (!dict_table_is_temporary(table)) {
		/* We use the private SQL parser of Innobase to generate the
		query graphs needed in deleting the dictionary data from system
		tables in Innobase. Deleting a row from SYS_INDEXES table also
		frees the file segments of the B-tree associated with the
		index. */

		info = pars_info_create();

		pars_info_add_str_literal(info, "table_name", name);

		std::basic_string<char, std::char_traits<char>,
				  ut_allocator<char> > sql;
		sql.reserve(2000);

		sql =	"PROCEDURE DROP_TABLE_PROC () IS\n"
			"sys_foreign_id CHAR;\n"
			"table_id CHAR;\n"
			"index_id CHAR;\n"
			"foreign_id CHAR;\n"
			"space_id INT;\n"
			"found INT;\n";

		sql +=	"DECLARE CURSOR cur_fk IS\n"
			"SELECT ID FROM SYS_FOREIGN\n"
			"WHERE FOR_NAME = :table_name\n"
			"AND TO_BINARY(FOR_NAME)\n"
			"  = TO_BINARY(:table_name)\n"
			"LOCK IN SHARE MODE;\n";

		sql +=	"DECLARE CURSOR cur_idx IS\n"
			"SELECT ID FROM SYS_INDEXES\n"
			"WHERE TABLE_ID = table_id\n"
			"LOCK IN SHARE MODE;\n";

		sql +=	"BEGIN\n";

		sql +=	"SELECT ID INTO table_id\n"
			"FROM SYS_TABLES\n"
			"WHERE NAME = :table_name\n"
			"LOCK IN SHARE MODE;\n"
			"IF (SQL % NOTFOUND) THEN\n"
			"       RETURN;\n"
			"END IF;\n";

		sql +=	"SELECT SPACE INTO space_id\n"
			"FROM SYS_TABLES\n"
			"WHERE NAME = :table_name;\n"
			"IF (SQL % NOTFOUND) THEN\n"
			"       RETURN;\n"
			"END IF;\n";

		sql +=	"found := 1;\n"
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
			"IF (:table_name = 'SYS_FOREIGN_COLS') \n"
			"THEN\n"
			"       found := 0;\n"
			"END IF;\n";

		sql +=	"OPEN cur_fk;\n"
			"WHILE found = 1 LOOP\n"
			"       FETCH cur_fk INTO foreign_id;\n"
			"       IF (SQL % NOTFOUND) THEN\n"
			"               found := 0;\n"
			"       ELSE\n"
			"               DELETE FROM \n"
			"		   SYS_FOREIGN_COLS\n"
			"               WHERE ID = foreign_id;\n"
			"               DELETE FROM SYS_FOREIGN\n"
			"               WHERE ID = foreign_id;\n"
			"       END IF;\n"
			"END LOOP;\n"
			"CLOSE cur_fk;\n";

		sql +=	"found := 1;\n"
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
			"CLOSE cur_idx;\n";

		sql +=	"DELETE FROM SYS_COLUMNS\n"
			"WHERE TABLE_ID = table_id;\n"
			"DELETE FROM SYS_TABLES\n"
			"WHERE NAME = :table_name;\n";

		if (dict_table_is_file_per_table(table)) {
			sql += "DELETE FROM SYS_TABLESPACES\n"
				"WHERE SPACE = space_id;\n"
				"DELETE FROM SYS_DATAFILES\n"
				"WHERE SPACE = space_id;\n";
		}

		sql +=	"DELETE FROM SYS_VIRTUAL\n"
			"WHERE TABLE_ID = table_id;\n";

		sql += "END;\n";

		err = que_eval_sql(info, sql.c_str(), FALSE, trx);
	} else {
		page_no = page_nos;
		for (dict_index_t* index = dict_table_get_first_index(table);
		     index != NULL;
		     index = dict_table_get_next_index(index)) {
			/* remove the index object associated. */
			dict_drop_index_tree_in_mem(index, *page_no++);
		}
		err = DB_SUCCESS;
	}

	switch (err) {
		ulint	space_id;
		bool	is_temp;
		bool	is_encrypted;
		bool	ibd_file_missing;
		bool	is_discarded;
		bool	shared_tablespace;

	case DB_SUCCESS:
		space_id = table->space;
		ibd_file_missing = table->ibd_file_missing;
		is_discarded = dict_table_is_discarded(table);
		is_temp = dict_table_is_temporary(table);
		is_encrypted = dict_table_is_encrypted(table);
		shared_tablespace = DICT_TF_HAS_SHARED_SPACE(table->flags);

		/* If there is a temp path then the temp flag is set.
		However, during recovery, we might have a temp flag but
		not know the temp path */
		ut_a(table->dir_path_of_temp_table == NULL || is_temp);

		/* We do not allow temporary tables with a remote path. */
		ut_a(!(is_temp && DICT_TF_HAS_DATA_DIR(table->flags)));

		/* Make sure the data_dir_path is set if needed. */
		dict_get_and_save_data_dir_path(table, true);

		err = row_drop_ancillary_fts_tables(table, trx);
		if (err != DB_SUCCESS) {
			break;
		}

		/* Determine the tablespace filename before we drop
		dict_table_t.  Free this memory before returning. */
		if (DICT_TF_HAS_DATA_DIR(table->flags)) {
			ut_a(table->data_dir_path);

			filepath = fil_make_filepath(
				table->data_dir_path,
				table->name.m_name, IBD, true);
		} else if (table->dir_path_of_temp_table) {
			filepath = fil_make_filepath(
				table->dir_path_of_temp_table,
				NULL, IBD, false);
		} else if (!shared_tablespace) {
			filepath = fil_make_filepath(
				NULL, table->name.m_name, IBD, false);
		}

		/* Free the dict_table_t object. */
		err = row_drop_table_from_cache(tablename, table, trx);
		if (err != DB_SUCCESS) {
			break;
		}

		/* Do not attempt to drop known-to-be-missing tablespaces,
		nor system or shared general tablespaces. */
		if (is_discarded || ibd_file_missing || shared_tablespace
		    || is_system_tablespace(space_id)) {
			/* For encrypted table, if ibd file can not be decrypt,
			we also set ibd_file_missing. We still need to try to
			remove the ibd file for this. */
			if (is_discarded || !is_encrypted
			    || !ibd_file_missing) {
				break;
			}
		}

		if (is_encrypted) {
			/* Require the mutex to block key rotation. */
			mutex_enter(&master_key_id_mutex);
		}
		/* We can now drop the single-table tablespace. */
		err = row_drop_single_table_tablespace(
			space_id, tablename, filepath,
			is_temp, is_encrypted, trx);

		if (is_encrypted) {
			mutex_exit(&master_key_id_mutex);
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
		the error number and rollback the transaction */
		ib::error() << "Unknown error code " << err << " while"
			" dropping table: "
			<< ut_get_name(trx, tablename) << ".";

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

	if (err != DB_SUCCESS && table != NULL) {
		/* Drop table has failed with error but as drop table is not
		transaction safe we should mark the table as corrupted to avoid
		unwarranted follow-up action on this table that can result
		in more serious issues. */

		table->corrupted = true;
		for (dict_index_t* index = UT_LIST_GET_FIRST(table->indexes);
		     index != NULL;
		     index = UT_LIST_GET_NEXT(indexes, index)) {
			dict_set_corrupted(index, trx, "DROP TABLE");
		}
	}

funct_exit:
	if (heap) {
		mem_heap_free(heap);
	}

	ut_free(filepath);

	if (locked_dictionary) {

		if (trx_is_started(trx)) {

			trx_commit_for_mysql(trx);
		}

		row_mysql_unlock_data_dictionary(trx);
	}

	trx->op_info = "";

	/* No need to immediately invoke master thread as there is no work
	generated by intrinsic table operation that needs master thread
	attention. */
	if (!is_intrinsic_temp_table) {
		srv_wake_master_thread();
	}

	DBUG_RETURN(err);
}

/*********************************************************************//**
Drop all temporary tables during crash recovery. */
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

		table = dict_load_table(table_name, true,
					DICT_ERR_IGNORE_NONE);

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
@return error code or DB_SUCCESS */
static MY_ATTRIBUTE((warn_unused_result))
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

/** Drop a database for MySQL.
@param[in]	name	database name which ends at '/'
@param[in]	trx	transaction handle
@param[out]	found	number of dropped tables/partitions
@return error code or DB_SUCCESS */
dberr_t
row_drop_database_for_mysql(
	const char*	name,
	trx_t*		trx,
	ulint*		found)
{
	dict_table_t*	table;
	char*		table_name;
	dberr_t		err	= DB_SUCCESS;
	ulint		namelen	= strlen(name);
	bool		is_partition = false;

	ut_ad(found != NULL);

	DBUG_ENTER("row_drop_database_for_mysql");

	DBUG_PRINT("row_drop_database_for_mysql", ("db: '%s'", name));

	ut_a(name != NULL);
	/* Assert DB name or partition name. */
	if (name[namelen - 1] == '#') {
		ut_ad(name[namelen - 2] != '/');
		is_partition = true;
		trx->op_info = "dropping partitions";
	} else {
		ut_a(name[namelen - 1] == '/');
		trx->op_info = "dropping database";
	}

	*found = 0;

	trx_set_dict_operation(trx, TRX_DICT_OP_TABLE);

	trx_start_if_not_started_xa(trx, true);

loop:
	row_mysql_lock_data_dictionary(trx);

	while ((table_name = dict_get_first_table_name_in_db(name))) {
		/* Drop parent table if it is a fts aux table, to
		avoid accessing dropped fts aux tables in information
		scheam when parent table still exists.
		Note: Drop parent table will drop fts aux tables. */
		char*	parent_table_name;
		parent_table_name = fts_get_parent_table_name(
				table_name, strlen(table_name));

		if (parent_table_name != NULL) {
			ut_free(table_name);
			table_name = parent_table_name;
		}

		ut_a(memcmp(table_name, name, namelen) == 0);

		table = dict_table_open_on_name(
			table_name, TRUE, FALSE, static_cast<dict_err_ignore_t>(
				DICT_ERR_IGNORE_INDEX_ROOT
				| DICT_ERR_IGNORE_CORRUPT));

		if (!table) {
			ib::error() << "Cannot load table " << table_name
				<< " from InnoDB internal data dictionary"
				" during drop database";
			ut_free(table_name);
			err = DB_TABLE_NOT_FOUND;
			break;

		}

		if (!row_is_mysql_tmp_table_name(table->name.m_name)) {
			/* There could be orphan temp tables left from
			interrupted alter table. Leave them, and handle
			the rest.*/
			if (table->can_be_evicted
			    && (name[namelen - 1] != '#')) {
				ib::warn() << "Orphan table encountered during"
					" DROP DATABASE. This is possible if '"
					<< table->name << ".frm' was lost.";
			}

			if (table->ibd_file_missing) {
				ib::warn() << "Missing .ibd file for table "
					<< table->name << ".";
			}
		}

		dict_table_close(table, TRUE, FALSE);

		/* The dict_table_t object must not be accessed before
		dict_table_open() or after dict_table_close(). But this is OK
		if we are holding, the dict_sys->mutex. */
		ut_ad(mutex_own(&dict_sys->mutex));

		/* Disable statistics on the found table. */
		if (!dict_stats_stop_bg(table)) {
			row_mysql_unlock_data_dictionary(trx);

			os_thread_sleep(250000);

			ut_free(table_name);

			goto loop;
		}

		/* Wait until MySQL does not have any queries running on
		the table */

		if (table->get_ref_count() > 0) {
			row_mysql_unlock_data_dictionary(trx);

			ib::warn() << "MySQL is trying to drop database "
				<< ut_get_name(trx, name) << " though"
				" there are still open handles to table "
				<< table->name << ".";

			os_thread_sleep(1000000);

			ut_free(table_name);

			goto loop;
		}

		err = row_drop_table_for_mysql(table_name, trx, TRUE);
		trx_commit_for_mysql(trx);

		if (err != DB_SUCCESS) {
			ib::error() << "DROP DATABASE "
				<< ut_get_name(trx, name) << " failed"
				" with error (" << ut_strerr(err) << ") for"
				" table " << ut_get_name(trx, table_name);
			ut_free(table_name);
			break;
		}

		ut_free(table_name);
		(*found)++;
	}

	/* Partitioning does not yet support foreign keys. */
	if (err == DB_SUCCESS && !is_partition) {
		/* after dropping all tables try to drop all leftover
		foreign keys in case orphaned ones exist */
		err = drop_all_foreign_keys_in_db(name, trx);

		if (err != DB_SUCCESS) {
			const std::string&	db = ut_get_name(trx, name);
			ib::error() << "DROP DATABASE " << db << " failed with"
				" error " << err << " while dropping all"
				" foreign keys";
		}
	}

	trx_commit_for_mysql(trx);

	row_mysql_unlock_data_dictionary(trx);

	trx->op_info = "";

	DBUG_RETURN(err);
}

/*********************************************************************//**
Checks if a table name contains the string "/#sql" which denotes temporary
tables in MySQL.
@return true if temporary table */
MY_ATTRIBUTE((warn_unused_result))
bool
row_is_mysql_tmp_table_name(
/*========================*/
	const char*	name)	/*!< in: table name in the form
				'database/tablename' */
{
	return(strstr(name, "/" TEMP_FILE_PREFIX) != NULL);
	/* return(strstr(name, "/@0023sql") != NULL); */
}

/****************************************************************//**
Delete a single constraint.
@return error code or DB_SUCCESS */
static MY_ATTRIBUTE((nonnull, warn_unused_result))
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
@return error code or DB_SUCCESS */
static MY_ATTRIBUTE((nonnull, warn_unused_result))
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
@return error code or DB_SUCCESS */
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
	bool		aux_fts_rename		= false;

	ut_a(old_name != NULL);
	ut_a(new_name != NULL);
	ut_ad(trx->state == TRX_STATE_ACTIVE);

	if (srv_force_recovery) {
		ib::info() << MODIFICATIONS_NOT_ALLOWED_MSG_FORCE_RECOVERY;
		err = DB_READ_ONLY;
		goto funct_exit;

	} else if (row_mysql_is_system_table(new_name)) {

		ib::error() << "Trying to create a MySQL system table "
			<< new_name << " of type InnoDB. MySQL system tables"
			" must be of the MyISAM type!";
		goto funct_exit;
	}

	/* Check the table identifier length here. It is possible that when we
	are renaming a temporary table back to original name (after alter)
	the table identifier length can exceed the maximum file name limit */

	if (strlen(strchr(new_name,'/') + 1) > FN_LEN ) {
		my_error(ER_PATH_LENGTH, MYF(0),
			 strchr(new_name,'/')+1);
		err = DB_IDENTIFIER_TOO_LONG;
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
		goto funct_exit;

	} else if (table->ibd_file_missing
		   && !dict_table_is_discarded(table)) {

		err = DB_TABLE_NOT_FOUND;

		ib::error() << "Table " << old_name << " does not have an .ibd"
			" file in the database directory. "
			<< TROUBLESHOOTING_MSG;

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
		ib::error() << "In ALTER TABLE "
			<< ut_get_name(trx, old_name)
			<< " a FOREIGN KEY check is running. Cannot rename"
			" table.";
		err = DB_TABLE_IN_FK_CHECK;
		goto funct_exit;
	}

	/* We use the private SQL parser of Innobase to generate the query
	graphs needed in updating the dictionary data from system tables. */

	info = pars_info_create();

	pars_info_add_str_literal(info, "new_table_name", new_name);
	pars_info_add_str_literal(info, "old_table_name", old_name);

	DEBUG_SYNC_C("rename_table");
	err = que_eval_sql(info,
			   "PROCEDURE RENAME_TABLE () IS\n"
			   "BEGIN\n"
			   "UPDATE SYS_TABLES"
			   " SET NAME = :new_table_name\n"
			   " WHERE NAME = :old_table_name;\n"
			   "END;\n"
			   , FALSE, trx);

	/* SYS_TABLESPACES and SYS_DATAFILES need to be updated if
	the table is in a single-table tablespace. */
	if (err == DB_SUCCESS
	    && dict_table_is_file_per_table(table)
	    && !table->ibd_file_missing) {
		/* Make a new pathname to update SYS_DATAFILES. */
		char*	new_path = row_make_new_pathname(table, new_name);
		char*	old_path = fil_space_get_first_path(table->space);

		/* If old path and new path are the same means tablename
		has not changed and only the database name holding the table
		has changed so we need to make the complete filepath again. */
		if (!dict_tables_have_same_db(old_name, new_name)) {
			ut_free(new_path);
			new_path = fil_make_filepath(NULL, new_name, IBD, false);
		}

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

		ut_free(old_path);
		ut_free(new_path);
	}
	if (err != DB_SUCCESS) {
		goto end;
	}

	if (!new_is_tmp) {
		/* Rename all constraints. */
		char	new_table_name[MAX_TABLE_NAME_LEN + 1] = "";
		char	old_table_utf8[MAX_TABLE_NAME_LEN + 1] = "";
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

	if ((dict_table_has_fts_index(table)
		|| DICT_TF2_FLAG_IS_SET(table, DICT_TF2_FTS_HAS_DOC_ID))
		&& !dict_tables_have_same_db(old_name, new_name)) {
		err = fts_rename_aux_tables(table, new_name, trx);
		if (err != DB_TABLE_NOT_FOUND) {
			aux_fts_rename = true;
		}
	}

end:
	if (err != DB_SUCCESS) {
		if (err == DB_DUPLICATE_KEY) {
			ib::error() << "Possible reasons:";
			ib::error() << "(1) Table rename would cause two"
				" FOREIGN KEY constraints to have the same"
				" internal name in case-insensitive"
				" comparison.";
			ib::error() << "(2) Table "
				<< ut_get_name(trx, new_name)
				<< " exists in the InnoDB internal data"
				" dictionary though MySQL is trying to rename"
				" table " << ut_get_name(trx, old_name)
				<< " to it. Have you deleted the .frm file and"
				" not used DROP TABLE?";
			ib::info() << TROUBLESHOOTING_MSG;
			ib::error() << "If table "
				<< ut_get_name(trx, new_name)
				<< " is a temporary table #sql..., then"
				" it can be that there are still queries"
				" running on the table, and it will be dropped"
				" automatically when the queries end. You can"
				" drop the orphaned table inside InnoDB by"
				" creating an InnoDB table with the same name"
				" in another database and copying the .frm file"
				" to the current database. Then MySQL thinks"
				" the table exists, and DROP TABLE will"
				" succeed.";
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

		/* In case of copy alter, template db_name and
		table_name should be renamed only for newly
		created table. */
		if (table->vc_templ != NULL && !new_is_tmp) {
			innobase_rename_vc_templ(table);
		}

		/* We only want to switch off some of the type checking in
		an ALTER TABLE...ALGORITHM=COPY, not in a RENAME. */
		dict_names_t	fk_tables;

		err = dict_load_foreigns(
			new_name, NULL,
			false, !old_is_tmp || trx->check_foreigns,
			DICT_ERR_IGNORE_NONE, fk_tables);

		if (err != DB_SUCCESS) {

			if (old_is_tmp) {
				ib::error() << "In ALTER TABLE "
					<< ut_get_name(trx, new_name)
					<< " has or is referenced in foreign"
					" key constraints which are not"
					" compatible with the new table"
					" definition.";
			} else {
				ib::error() << "In RENAME TABLE table "
					<< ut_get_name(trx, new_name)
					<< " is referenced in foreign key"
					" constraints which are not compatible"
					" with the new table definition.";
			}

			ut_a(DB_SUCCESS == dict_table_rename_in_cache(
				table, old_name, FALSE));
			trx->error_state = DB_SUCCESS;
			trx_rollback_to_savepoint(trx, NULL);
			trx->error_state = DB_SUCCESS;
		}

		/* Check whether virtual column or stored column affects
		the foreign key constraint of the table. */
		if (dict_foreigns_has_s_base_col(
				table->foreign_set, table)) {
			err = DB_NO_FK_ON_S_BASE_COL;
			ut_a(DB_SUCCESS == dict_table_rename_in_cache(
				table, old_name, FALSE));
			trx->error_state = DB_SUCCESS;
			trx_rollback_to_savepoint(trx, NULL);
			trx->error_state = DB_SUCCESS;
			goto funct_exit;
		}

		/* Fill the virtual column set in foreign when
		the table undergoes copy alter operation. */
		dict_mem_table_free_foreign_vcol_set(table);
		dict_mem_table_fill_foreign_vcol_set(table);

		while (!fk_tables.empty()) {
			dict_load_table(fk_tables.front(), true,
					DICT_ERR_IGNORE_NONE);
			fk_tables.pop_front();
		}
	}

funct_exit:
	if (aux_fts_rename && err != DB_SUCCESS
	    && table != NULL && (table->space != 0)) {

		char*	orig_name = table->name.m_name;
		trx_t*	trx_bg = trx_allocate_for_background();

		/* If the first fts_rename fails, the trx would
		be rolled back and committed, we can't use it any more,
		so we have to start a new background trx here. */
		ut_a(trx_state_eq(trx_bg, TRX_STATE_NOT_STARTED));
		trx_bg->op_info = "Revert the failing rename "
				  "for fts aux tables";
		trx_bg->dict_operation_lock_mode = RW_X_LATCH;
		trx_start_for_ddl(trx_bg, TRX_DICT_OP_TABLE);

		/* If rename fails and table has its own tablespace,
		we need to call fts_rename_aux_tables again to
		revert the ibd file rename, which is not under the
		control of trx. Also notice the parent table name
		in cache is not changed yet. If the reverting fails,
		the ibd data may be left in the new database, which
		can be fixed only manually. */
		table->name.m_name = const_cast<char*>(new_name);
		fts_rename_aux_tables(table, old_name, trx_bg);
		table->name.m_name = orig_name;

		trx_bg->dict_operation_lock_mode = 0;
		trx_commit_for_mysql(trx_bg);
		trx_free_for_background(trx_bg);
	}

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

/** Renames a partitioned table for MySQL.
@parama[in]	thd		Connection thread handle
@param[in]	old_name	Old table name.
@param[in]	new_name	New table name.
@param[in,out]	trx		Transaction.
@return error code or DB_SUCCESS */
dberr_t
row_rename_partitions_for_mysql(
	THD*		thd,
	const char*	old_name,
	const char*	new_name,
	trx_t*		trx)
{
	char		from_name[FN_REFLEN];
	char		to_name[FN_REFLEN];
	ulint		from_len = strlen(old_name);
	ulint		to_len = strlen(new_name);
	char*		table_name;
	dberr_t		error = DB_TABLE_NOT_FOUND;

	ut_a(from_len < (FN_REFLEN - 4));
	ut_a(to_len < (FN_REFLEN - 4));
	memcpy(from_name, old_name, from_len);
	from_name[from_len] = '#';
	from_name[from_len + 1] = 0;
	typedef std::vector<std::pair<std::string, std::string> > partition_names;
	partition_names store_name;
	partition_names::iterator it;

	while ((table_name = dict_get_first_table_name_in_db(from_name))) {
		ut_a(memcmp(table_name, from_name, from_len) == 0);
		/* Must match #[Pp]#<partition_name> */
		if (strlen(table_name) <= (from_len + 3)
		    || table_name[from_len] != '#'
		    || table_name[from_len + 2] != '#'
		    || (table_name[from_len + 1] != 'P'
			&& table_name[from_len + 1] != 'p')) {

			ut_ad(0);
			ut_free(table_name);
			continue;
		}
		memcpy(to_name, new_name, to_len);
		memcpy(to_name + to_len, table_name + from_len,
			strlen(table_name) - from_len + 1);
		error = row_rename_table_for_mysql(table_name, to_name,
						trx, false);
		if (error == DB_SUCCESS) {
			std::pair<std::string, std::string> pair_names;
			pair_names.first = table_name;
			pair_names.second = to_name;
			store_name.push_back(pair_names);
		} else {
			store_name.clear();
			/* Rollback and return. */
			trx_rollback_for_mysql(trx);
			ut_free(table_name);
			return(error);
		}
		ut_free(table_name);
	}
	trx_commit_for_mysql(trx);

	char    errstr[512];
	for (it = store_name.begin(); it != store_name.end(); ++it) {
		error = dict_stats_rename_table(
			   true, it->first.c_str(), it->second.c_str(),
			   errstr, sizeof(errstr));

		if (error != DB_SUCCESS) {
			ib::error() << errstr;
			push_warning(thd, Sql_condition::SL_WARNING,
				     ER_LOCK_WAIT_TIMEOUT, errstr);
			break;
		}
	}

	store_name.clear();
	return(error);
}

/*********************************************************************//**
Scans an index for either COUNT(*) or CHECK TABLE.
If CHECK TABLE; Checks that the index contains entries in an ascending order,
unique constraint is not broken, and calculates the number of index entries
in the read view of the current transaction.
@return DB_SUCCESS or other error */
dberr_t
row_scan_index_for_mysql(
/*=====================*/
	row_prebuilt_t*		prebuilt,	/*!< in: prebuilt struct
						in MySQL handle */
	const dict_index_t*	index,		/*!< in: index */
#ifdef WL6742
	/* Removing WL6742 as part of Bug 23046302 */

	bool			check_keys,	/*!< in: true=check for mis-
						ordered or duplicate records,
						false=count the rows only */
#endif
	ulint*			n_rows)		/*!< out: number of entries
						seen in the consistent read */
{
	dtuple_t*	prev_entry	= NULL;
	ulint		matched_fields;
	byte*		buf;
	dberr_t		ret;
	rec_t*		rec;
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

	/* Don't support RTree Leaf level scan */
	ut_ad(!dict_index_is_spatial(index));

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
		return(DB_SUCCESS);
	}

	ulint bufsize = ut_max(UNIV_PAGE_SIZE, prebuilt->mysql_row_len);
	buf = static_cast<byte*>(ut_malloc_nokey(bufsize));
	heap = mem_heap_create(100);

	cnt = 1000;

	ret = row_search_for_mysql(buf, PAGE_CUR_G, prebuilt, 0, 0);
loop:
	/* Check thd->killed every 1,000 scanned rows */
	if (--cnt == 0) {
		if (trx_is_interrupted(prebuilt->trx)) {
			ret = DB_INTERRUPTED;
			goto func_exit;
		}
		cnt = 1000;
	}

	switch (ret) {
	case DB_SUCCESS:
		break;
	case DB_DEADLOCK:
	case DB_LOCK_TABLE_FULL:
	case DB_LOCK_WAIT_TIMEOUT:
	case DB_INTERRUPTED:
		goto func_exit;
	default:
	{
		const char* doing = "CHECK TABLE";
		ib::warn() << doing << " on index " << index->name << " of"
			" table " << index->table->name << " returned " << ret;
	}
	/* fall through (this error is ignored by CHECK TABLE) */
	case DB_END_OF_INDEX:
		ret = DB_SUCCESS;
func_exit:
		ut_free(buf);
		mem_heap_free(heap);

		return(ret);
	}

	*n_rows = *n_rows + 1;

#ifdef WL6742
	/*Removing WL6742 as part of Bug 23046302 */
	if (!check_keys) {
		goto next_rec;
	}
#endif
	/* else this code is doing handler::check() for CHECK TABLE */

	/* row_search... returns the index record in buf, record origin offset
	within buf stored in the first 4 bytes, because we have built a dummy
	template */

	rec = buf + mach_read_from_4(buf);

	offsets = rec_get_offsets(rec, index, offsets_,
				  ULINT_UNDEFINED, &heap);

	if (prev_entry != NULL) {
		matched_fields = 0;

		cmp = cmp_dtuple_rec_with_match(prev_entry, rec, offsets,
						&matched_fields);
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

		const char* msg;

		if (cmp > 0) {
			ret = DB_INDEX_CORRUPT;
			msg = "index records in a wrong order in ";
not_ok:
			ib::error()
				<< msg << index->name
				<< " of table " << index->table->name
				<< ": " << *prev_entry << ", "
				<< rec_offsets_print(rec, offsets);
			/* Continue reading */
		} else if (dict_index_is_unique(index)
			   && !contains_null
			   && matched_fields
			   >= dict_index_get_n_ordering_defined_by_user(
				   index)) {
			ret = DB_DUPLICATE_KEY;
			msg = "duplicate key in ";
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
#ifdef WL6742
/* Removed WL6742 as part of Bug 23046302 */
next_rec:
#endif
	ret = row_search_for_mysql(
		buf, PAGE_CUR_G, prebuilt, 0, ROW_SEL_NEXT);

	goto loop;
}

/*********************************************************************//**
Initialize this module */
void
row_mysql_init(void)
/*================*/
{
	mutex_create(LATCH_ID_ROW_DROP_LIST, &row_drop_list_mutex);

	UT_LIST_INIT(
		row_mysql_drop_list,
		&row_mysql_drop_t::row_mysql_drop_list);

	row_mysql_drop_list_inited = TRUE;
}

/*********************************************************************//**
Close this module */
void
row_mysql_close(void)
/*================*/
{
	ut_a(UT_LIST_GET_LEN(row_mysql_drop_list) == 0);

	mutex_free(&row_drop_list_mutex);

	row_mysql_drop_list_inited = FALSE;
}

/*****************************************************************************

Copyright (c) 2012, Oracle and/or its affiliates. All Rights Reserved.

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
@file row/row0quiesce.cc
Quiesce a tablespace.

Created 2012-02-08 by Sunny Bains.
*******************************************************/

#include "row0quiesce.h"
#include "row0mysql.h"

#ifdef UNIV_NONINL
#include "row0quiesce.ic"
#endif

#include "ibuf0ibuf.h"
#include "srv0start.h"
#include "trx0purge.h"

/*********************************************************************//**
Write the meta data (index user fields) config file.
@return DB_SUCCESS or error code. */
static	__attribute__((nonnull, warn_unused_result))
db_err
row_quiesce_write_index_fields(
/*===========================*/
	const dict_index_t*	index,	/*!< in: write the meta data for
					this index */
	FILE*			file,	/*!< in: file to write to */
	void*			thd)	/*!< in/out: session */
{
	byte			row[sizeof(ib_uint32_t) * 2];

	for (ulint i = 0; i < index->n_fields; ++i) {
		byte*			ptr = row;
		const dict_field_t*	field = &index->fields[i];

		mach_write_to_4(ptr, field->prefix_len);
		ptr += sizeof(ib_uint32_t);

		mach_write_to_4(ptr, field->fixed_len);

		if (fwrite(row, 1, sizeof(row), file) != sizeof(row)) {

			ib_pushf(thd, IB_LOG_LEVEL_ERROR,
				 ER_IO_WRITE_ERROR,
				 "I/O error (%lu), while writing index fields.",
				 (ulint) errno);

			return(DB_IO_ERROR);
		}

		/* Include the NUL byte in the length. */
		ib_uint32_t	len = strlen(field->name) + 1;
		ut_a(len > 1);

		mach_write_to_4(row, len);

		if (fwrite(row, 1,  sizeof(len), file) != sizeof(len)
		    || fwrite(field->name, 1, len, file) != len) {

			ib_pushf(thd, IB_LOG_LEVEL_ERROR,
				 ER_IO_WRITE_ERROR,
				 "I/O error (%lu), while writing index column "
				 "name.", (ulint) errno);

			return(DB_IO_ERROR);
		}
	}

	return(DB_SUCCESS);
}

/*********************************************************************//**
Write the meta data config file index information.
@return DB_SUCCESS or error code. */
static	__attribute__((nonnull, warn_unused_result))
db_err
row_quiesce_write_indexes(
/*======================*/
	const dict_table_t*	table,	/*!< in: write the meta data for
					this table */
	FILE*			file,	/*!< in: file to write to */
	void*			thd)	/*!< in/out: session */
{
	{
		byte		row[sizeof(ib_uint32_t)];

		/* Write the number of indexes in the table. */
		mach_write_to_4(row, UT_LIST_GET_LEN(table->indexes));

		if (fwrite(row, 1,  sizeof(row), file) != sizeof(row)) {
			ib_pushf(thd, IB_LOG_LEVEL_ERROR,
				 ER_IO_WRITE_ERROR,
				 "I/O error (%lu), while writing index count.",
				 (ulint) errno);

			return(DB_IO_ERROR);
		}
	}

	db_err			err = DB_SUCCESS;

	/* Write the index meta data. */
	for (const dict_index_t* index = UT_LIST_GET_FIRST(table->indexes);
	     index != 0 && err == DB_SUCCESS;
	     index = UT_LIST_GET_NEXT(indexes, index)) {

		byte*		ptr;
		byte		row[sizeof(ib_uint32_t) * 8];

		ptr = row;

		mach_write_to_4(ptr, index->space);
		ptr += sizeof(ib_uint32_t);

		mach_write_to_4(ptr, index->page);
		ptr += sizeof(ib_uint32_t);

		mach_write_to_4(ptr, index->type);
		ptr += sizeof(ib_uint32_t);

		mach_write_to_4(ptr, index->trx_id_offset);
		ptr += sizeof(ib_uint32_t);

		mach_write_to_4(ptr, index->n_user_defined_cols);
		ptr += sizeof(ib_uint32_t);

		mach_write_to_4(ptr, index->n_uniq);
		ptr += sizeof(ib_uint32_t);

		mach_write_to_4(ptr, index->n_nullable);
		ptr += sizeof(ib_uint32_t);

		mach_write_to_4(ptr, index->n_fields);

		if (fwrite(row, 1, sizeof(row), file) != sizeof(row)) {

			ib_pushf(thd, IB_LOG_LEVEL_ERROR,
				 ER_IO_WRITE_ERROR,
				 "I/O error (%lu), while writing index "
				 "meta-data", (ulint) errno);

			return(DB_IO_ERROR);
		}

		/* Write the length of the index name.
		NUL byte is included in the length. */
		ib_uint32_t	len = strlen(index->name) + 1;
		ut_a(len > 1);

		mach_write_to_4(row, len);

		if (fwrite(row, 1, sizeof(len), file) != sizeof(len)
		    || fwrite(index->name, 1, len, file) != len) {

			ib_pushf(thd, IB_LOG_LEVEL_ERROR,
				 ER_IO_WRITE_ERROR,
				 "I/O error (%lu), while writing index "
				 "name", (ulint) errno);

			return(DB_IO_ERROR);
		}

		err = row_quiesce_write_index_fields(index, file, thd);
	}

	return(err);
}

/*********************************************************************//**
Write the meta data (table columns) config file. Serialise the contents of
dict_col_t structure, along with the column name. All fields are serialized
as ib_uint32_t.
@return DB_SUCCESS or error code. */
static	__attribute__((nonnull, warn_unused_result))
db_err
row_quiesce_write_table(
/*====================*/
	const dict_table_t*	table,	/*!< in: write the meta data for
					this table */
	FILE*			file,	/*!< in: file to write to */
	void*			thd)	/*!< in/out: session */
{
	dict_col_t*		col;
	byte			row[sizeof(ib_uint32_t) * 7];

	col = table->cols;

	for (ulint i = 0; i < table->n_cols; ++i, ++col) {
		byte*		ptr = row;

		mach_write_to_4(ptr, col->prtype);
		ptr += sizeof(ib_uint32_t);

		mach_write_to_4(ptr, col->mtype);
		ptr += sizeof(ib_uint32_t);

		mach_write_to_4(ptr, col->len);
		ptr += sizeof(ib_uint32_t);

		mach_write_to_4(ptr, col->mbminmaxlen);
		ptr += sizeof(ib_uint32_t);

		mach_write_to_4(ptr, col->ind);
		ptr += sizeof(ib_uint32_t);

		mach_write_to_4(ptr, col->ord_part);
		ptr += sizeof(ib_uint32_t);

		mach_write_to_4(ptr, col->max_prefix);

		if (fwrite(row, 1,  sizeof(row), file) != sizeof(row)) {
			ib_pushf(thd, IB_LOG_LEVEL_ERROR,
				 ER_IO_WRITE_ERROR,
				 "I/O error (%lu), while writing table column "
				 "data.", (ulint) errno);

			return(DB_IO_ERROR);
		}

		/* Write out the column name as [len, byte array]. The len
		includes the NUL byte. */
		ib_uint32_t	len;
		const char*	col_name;

		col_name = dict_table_get_col_name(table, dict_col_get_no(col));

		/* Include the NUL byte in the length. */
		len = strlen(col_name) + 1;
		ut_a(len > 1);

		mach_write_to_4(row, len);

		if (fwrite(row, 1,  sizeof(len), file) != sizeof(len)
		    || fwrite(col_name, 1, len, file) != len) {

			ib_pushf(thd, IB_LOG_LEVEL_ERROR,
				 ER_IO_WRITE_ERROR,
				 "I/O error (%lu), while writing column name",
				 (ulint) errno);

			return(DB_IO_ERROR);
		}
	}

	return(DB_SUCCESS);
}

/*********************************************************************//**
Write the meta data config file header.
@return DB_SUCCESS or error code. */
static	__attribute__((nonnull, warn_unused_result))
db_err
row_quiesce_write_header(
/*=====================*/
	const dict_table_t*	table,	/*!< in: write the meta data for
					this table */
	FILE*			file,	/*!< in: file to write to */
	void*			thd)	/*!< in/out: session */
{
	byte			value[sizeof(ib_uint32_t)];

	/* Write the meta-data version number. */
	mach_write_to_4(value, IB_EXPORT_CFG_VERSION_V1);

	if (fwrite(&value, 1,  sizeof(value), file) != sizeof(value)) {
		ib_pushf(thd, IB_LOG_LEVEL_ERROR, ER_IO_WRITE_ERROR,
			"I/O error (%lu), while writing meta-data "
			"version number.", (ulint) errno);

		return(DB_IO_ERROR);
	}

	/* Write the server hostname. */
	ib_uint32_t		len;
	const char*		hostname = server_get_hostname();

	/* Play it safe and check for NULL. */
	if (hostname == 0) {
		static const char	NullHostname[] = "Hostname unknown";

		ib_logf(IB_LOG_LEVEL_WARN,
			"Unable to determine server hostname.");

		hostname = NullHostname;
	}

	/* The server hostname includes the NUL byte. */
	len = strlen(hostname) + 1;
	mach_write_to_4(value, len);

	if (fwrite(&value, 1,  sizeof(value), file) != sizeof(value)
	    || fwrite(hostname, 1,  len, file) != len) {

		ib_pushf(thd, IB_LOG_LEVEL_ERROR, ER_IO_WRITE_ERROR,
			"I/O error (%lu), while writing hostname.",
			(ulint) errno);

		return(DB_IO_ERROR);
	}

	/* The table name includes the NUL byte. */
	ut_a(table->name != 0);
	len = strlen(table->name) + 1;

	/* Write the table name. */
	mach_write_to_4(value, len);

	if (fwrite(&value, 1,  sizeof(value), file) != sizeof(value)
	    || fwrite(table->name, 1,  len, file) != len) {

		ib_pushf(thd, IB_LOG_LEVEL_ERROR, ER_IO_WRITE_ERROR,
			"I/O error (%lu), while writing table name.",
			(ulint) errno);

		return(DB_IO_ERROR);
	}

	byte			row[sizeof(ib_uint32_t) * 3];
	byte*			ptr = row;

	/* Write the system page size. */
	mach_write_to_4(ptr, UNIV_PAGE_SIZE);
	ptr += sizeof(ib_uint32_t);

	/* Write the table->flags. */
	mach_write_to_4(ptr, table->flags);
	ptr += sizeof(ib_uint32_t);

	/* Write the number of columns in the table. */
	mach_write_to_4(ptr, table->n_cols);

	if (fwrite(row, 1,  sizeof(row), file) != sizeof(row)) {
		ib_pushf(thd, IB_LOG_LEVEL_ERROR, ER_IO_WRITE_ERROR,
			"I/O error (%lu), while writing table meta-data",
			(ulint) errno);

		return(DB_IO_ERROR);
	}

	return(DB_SUCCESS);
}

/*********************************************************************//**
Write the table meta data after quiesce.
@return DB_SUCCESS or error code */
static	__attribute__((nonnull, warn_unused_result))
db_err
row_quiesce_write_cfg(
/*==================*/
	const dict_table_t*	table,	/*!< in: write the meta data for
					this table */
	void*			thd)	/*!< in/out: session */
{
	db_err			err;
	char			name[OS_FILE_MAX_PATH];

	srv_get_meta_data_filename(table, name, sizeof(name));

	ib_logf(IB_LOG_LEVEL_INFO, "Writing table metadata to '%s'", name);

	FILE*	file = fopen(name, "w+");

	if (file == NULL) {
		ib_pushf(thd, IB_LOG_LEVEL_ERROR, ER_CANT_CREATE_FILE,
			 "Can't create %s : errno %lu", name, (ulong) errno);

		err = DB_IO_ERROR;
	} else {
		err = row_quiesce_write_header(table, file, thd);

		if (err == DB_SUCCESS) {
			err = row_quiesce_write_table(table, file, thd);
		}

		if (err == DB_SUCCESS) {
			err = row_quiesce_write_indexes(table, file, thd);
		}

		fclose(file);
	}

	return(err);
}

/*********************************************************************//**
Quiesce the tablespace that the table resides in. */
UNIV_INTERN
void
row_quiesce_table_start(
/*====================*/
	dict_table_t*	table,		/*!< in: quiesce this table */
	trx_t*		trx)		/*!< in/out: transaction/session */
{
	ut_a(trx->mysql_thd != 0);
	ut_a(srv_n_purge_threads > 0);

	char		table_name[MAX_FULL_NAME_LEN + 1];

	ut_a(trx->mysql_thd != 0);

	innobase_format_name(
		table_name, sizeof(table_name), table->name, FALSE);

	ib_logf(IB_LOG_LEVEL_INFO,
		"Sync to disk of %s started.", table_name);

	trx_purge_stop();

	ut_a(table->id > 0);

	ibuf_contract_in_background(table->id, TRUE);

	if (!trx_is_interrupted(trx)) {
		buf_LRU_flush_or_remove_pages(
			table->space, BUF_REMOVE_FLUSH_WRITE, trx);

		if (trx_is_interrupted(trx)) {

			ib_logf(IB_LOG_LEVEL_WARN, "Quiesce aborted!");

		} else if (row_quiesce_write_cfg(table, trx->mysql_thd)
			   != DB_SUCCESS) {

			ib_logf(IB_LOG_LEVEL_ERROR,
				"There was an error writing to the "
				"meta data file");
		} else {
			ib_logf(IB_LOG_LEVEL_INFO,
				"Table %s flushed to disk", table_name);
		}
	} else {
		ib_logf(IB_LOG_LEVEL_WARN, "Quiesce aborted!");
	}

	db_err	err = row_quiesce_set_state(table, QUIESCE_COMPLETE, trx);
	ut_a(err == DB_SUCCESS);
}

/*********************************************************************//**
Cleanup after table quiesce. */
UNIV_INTERN
void
row_quiesce_table_complete(
/*=======================*/
	dict_table_t*	table,		/*!< in: quiesce this table */
	trx_t*		trx)		/*!< in/out: transaction/session */
{
	ulint		count = 0;
	char		table_name[MAX_FULL_NAME_LEN + 1];

	ut_a(trx->mysql_thd != 0);

	innobase_format_name(
		table_name, sizeof(table_name), table->name, FALSE);

	/* We need to wait for the operation to complete if the
	transaction has been killed. */

	while (table->quiesce != QUIESCE_COMPLETE) {

		/* Print a warning after every minute. */
		if (!(count % 60)) {
			ib_logf(IB_LOG_LEVEL_WARN,
				"Waiting for quiesce of '%s' to complete",
				table_name);
		}

		/* Sleep for a second. */
		os_thread_sleep(10000000);

		++count;
	}

	/* Remove the .cfg file now that the user has resumed
	normal operations. Otherwise it will cause problems when
	the user tries to drop the database (remove directory). */
	char		cfg_name[OS_FILE_MAX_PATH];

	srv_get_meta_data_filename(table, cfg_name, sizeof(cfg_name));

	os_file_delete_if_exists(cfg_name);

	ib_logf(IB_LOG_LEVEL_INFO,
		"Deleting the meta-data file '%s'", cfg_name);

	trx_purge_run();

	db_err	err = row_quiesce_set_state(table, QUIESCE_NONE, trx);
	ut_a(err == DB_SUCCESS);
}

/*********************************************************************//**
Set a table's quiesce state.
@return DB_SUCCESS or error code. */
UNIV_INTERN
db_err
row_quiesce_set_state(
/*==================*/
	dict_table_t*	table,		/*!< in: quiesce this table */
	ib_quiesce_t	state,		/*!< in: quiesce state to set */
	trx_t*		trx)		/*!< in/out: transaction */
{
	ut_a(srv_n_purge_threads > 0);

	if (table->space == TRX_SYS_SPACE) {

		char	table_name[MAX_FULL_NAME_LEN + 1];

		innobase_format_name(
			table_name, sizeof(table_name), table->name, FALSE);

		ib_pushf(trx->mysql_thd,
			 IB_LOG_LEVEL_WARN,
			 ER_TABLE_IN_SYSTEM_TABLESPACE,
			"Can't quiesce table '%s', it is in the system "
			"tablespace", table->name);

		return(DB_UNSUPPORTED);
	}

	row_mysql_lock_data_dictionary(trx);

	dict_table_x_lock_indexes(table);

	switch (state) {
	case QUIESCE_START:
		ut_a(table->quiesce == QUIESCE_NONE);
		break;

	case QUIESCE_COMPLETE:
		ut_a(table->quiesce == QUIESCE_START);
		break;

	case QUIESCE_NONE:
		ut_a(table->quiesce == QUIESCE_COMPLETE);
		break;
	}

	table->quiesce = state;

	dict_table_x_unlock_indexes(table);

	row_mysql_unlock_data_dictionary(trx);

	return(DB_SUCCESS);
}


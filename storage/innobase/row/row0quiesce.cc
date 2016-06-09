/*****************************************************************************

Copyright (c) 2012, 2016, Oracle and/or its affiliates. All Rights Reserved.

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

#include "ha_prototypes.h"

#include "row0quiesce.h"
#ifdef UNIV_NONINL
#include "row0quiesce.ic"
#endif

#include "row0mysql.h"
#include "ibuf0ibuf.h"
#include "srv0start.h"
#include "trx0purge.h"
#include "fsp0sysspace.h"

#include <my_aes.h>

/*********************************************************************//**
Write the meta data (index user fields) config file.
@return DB_SUCCESS or error code. */
static	MY_ATTRIBUTE((nonnull, warn_unused_result))
dberr_t
row_quiesce_write_index_fields(
/*===========================*/
	const dict_index_t*	index,	/*!< in: write the meta data for
					this index */
	FILE*			file,	/*!< in: file to write to */
	THD*			thd)	/*!< in/out: session */
{
	byte			row[sizeof(ib_uint32_t) * 2];

	for (ulint i = 0; i < index->n_fields; ++i) {
		byte*			ptr = row;
		const dict_field_t*	field = &index->fields[i];

		mach_write_to_4(ptr, field->prefix_len);
		ptr += sizeof(ib_uint32_t);

		mach_write_to_4(ptr, field->fixed_len);

		DBUG_EXECUTE_IF("ib_export_io_write_failure_9",
				close(fileno(file)););

		if (fwrite(row, 1, sizeof(row), file) != sizeof(row)) {

			ib_senderrf(
				thd, IB_LOG_LEVEL_WARN, ER_IO_WRITE_ERROR,
				errno, strerror(errno),
				"while writing index fields.");

			return(DB_IO_ERROR);
		}

		/* Include the NUL byte in the length. */
		ib_uint32_t	len = static_cast<ib_uint32_t>(strlen(field->name) + 1);
		ut_a(len > 1);

		mach_write_to_4(row, len);

		DBUG_EXECUTE_IF("ib_export_io_write_failure_10",
				close(fileno(file)););

		if (fwrite(row, 1,  sizeof(len), file) != sizeof(len)
		    || fwrite(field->name, 1, len, file) != len) {

			ib_senderrf(
				thd, IB_LOG_LEVEL_WARN, ER_IO_WRITE_ERROR,
				errno, strerror(errno),
				"while writing index column.");

			return(DB_IO_ERROR);
		}
	}

	return(DB_SUCCESS);
}

/*********************************************************************//**
Write the meta data config file index information.
@return DB_SUCCESS or error code. */
static	MY_ATTRIBUTE((nonnull, warn_unused_result))
dberr_t
row_quiesce_write_indexes(
/*======================*/
	const dict_table_t*	table,	/*!< in: write the meta data for
					this table */
	FILE*			file,	/*!< in: file to write to */
	THD*			thd)	/*!< in/out: session */
{
	{
		byte		row[sizeof(ib_uint32_t)];

		/* Write the number of indexes in the table. */
		mach_write_to_4(row, UT_LIST_GET_LEN(table->indexes));

		DBUG_EXECUTE_IF("ib_export_io_write_failure_11",
				close(fileno(file)););

		if (fwrite(row, 1,  sizeof(row), file) != sizeof(row)) {
			ib_senderrf(
				thd, IB_LOG_LEVEL_WARN, ER_IO_WRITE_ERROR,
				errno, strerror(errno),
				"while writing index count.");

			return(DB_IO_ERROR);
		}
	}

	dberr_t			err = DB_SUCCESS;

	/* Write the index meta data. */
	for (const dict_index_t* index = UT_LIST_GET_FIRST(table->indexes);
	     index != 0 && err == DB_SUCCESS;
	     index = UT_LIST_GET_NEXT(indexes, index)) {

		byte*		ptr;
		byte		row[sizeof(index_id_t)
				    + sizeof(ib_uint32_t) * 8];

		ptr = row;

		ut_ad(sizeof(index_id_t) == 8);
		mach_write_to_8(ptr, index->id);
		ptr += sizeof(index_id_t);

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

		DBUG_EXECUTE_IF("ib_export_io_write_failure_12",
				close(fileno(file)););

		if (fwrite(row, 1, sizeof(row), file) != sizeof(row)) {

			ib_senderrf(
				thd, IB_LOG_LEVEL_WARN, ER_IO_WRITE_ERROR,
				errno, strerror(errno),
				"while writing index meta-data.");

			return(DB_IO_ERROR);
		}

		/* Write the length of the index name.
		NUL byte is included in the length. */
		ib_uint32_t	len = static_cast<ib_uint32_t>(strlen(index->name) + 1);
		ut_a(len > 1);

		mach_write_to_4(row, len);

		DBUG_EXECUTE_IF("ib_export_io_write_failure_1",
				close(fileno(file)););

		if (fwrite(row, 1, sizeof(len), file) != sizeof(len)
		    || fwrite(index->name, 1, len, file) != len) {

			ib_senderrf(
				thd, IB_LOG_LEVEL_WARN, ER_IO_WRITE_ERROR,
				errno, strerror(errno),
				"while writing index name.");

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
static	MY_ATTRIBUTE((nonnull, warn_unused_result))
dberr_t
row_quiesce_write_table(
/*====================*/
	const dict_table_t*	table,	/*!< in: write the meta data for
					this table */
	FILE*			file,	/*!< in: file to write to */
	THD*			thd)	/*!< in/out: session */
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

		DBUG_EXECUTE_IF("ib_export_io_write_failure_2",
				close(fileno(file)););

		if (fwrite(row, 1,  sizeof(row), file) != sizeof(row)) {
			ib_senderrf(
				thd, IB_LOG_LEVEL_WARN, ER_IO_WRITE_ERROR,
				errno, strerror(errno),
				"while writing table column data.");

			return(DB_IO_ERROR);
		}

		/* Write out the column name as [len, byte array]. The len
		includes the NUL byte. */
		ib_uint32_t	len;
		const char*	col_name;

		col_name = dict_table_get_col_name(table, dict_col_get_no(col));

		/* Include the NUL byte in the length. */
		len = static_cast<ib_uint32_t>(strlen(col_name) + 1);
		ut_a(len > 1);

		mach_write_to_4(row, len);

		DBUG_EXECUTE_IF("ib_export_io_write_failure_3",
				close(fileno(file)););

		if (fwrite(row, 1,  sizeof(len), file) != sizeof(len)
		    || fwrite(col_name, 1, len, file) != len) {

			ib_senderrf(
				thd, IB_LOG_LEVEL_WARN, ER_IO_WRITE_ERROR,
				errno, strerror(errno),
				"while writing column name.");

			return(DB_IO_ERROR);
		}
	}

	return(DB_SUCCESS);
}

/*********************************************************************//**
Write the meta data config file header.
@return DB_SUCCESS or error code. */
static	MY_ATTRIBUTE((nonnull, warn_unused_result))
dberr_t
row_quiesce_write_header(
/*=====================*/
	const dict_table_t*	table,	/*!< in: write the meta data for
					this table */
	FILE*			file,	/*!< in: file to write to */
	THD*			thd)	/*!< in/out: session */
{
	byte			value[sizeof(ib_uint32_t)];

	/* Write the meta-data version number. */
	mach_write_to_4(value, IB_EXPORT_CFG_VERSION_V1);

	DBUG_EXECUTE_IF("ib_export_io_write_failure_4", close(fileno(file)););

	if (fwrite(&value, 1,  sizeof(value), file) != sizeof(value)) {
		ib_senderrf(
			thd, IB_LOG_LEVEL_WARN, ER_IO_WRITE_ERROR,
			errno, strerror(errno),
			"while writing meta-data version number.");

		return(DB_IO_ERROR);
	}

	/* Write the server hostname. */
	ib_uint32_t		len;
	const char*		hostname = server_get_hostname();

	/* Play it safe and check for NULL. */
	if (hostname == 0) {
		static const char	NullHostname[] = "Hostname unknown";

		ib::warn() << "Unable to determine server hostname.";

		hostname = NullHostname;
	}

	/* The server hostname includes the NUL byte. */
	len = static_cast<ib_uint32_t>(strlen(hostname) + 1);
	mach_write_to_4(value, len);

	DBUG_EXECUTE_IF("ib_export_io_write_failure_5", close(fileno(file)););

	if (fwrite(&value, 1,  sizeof(value), file) != sizeof(value)
	    || fwrite(hostname, 1,  len, file) != len) {

		ib_senderrf(
			thd, IB_LOG_LEVEL_WARN, ER_IO_WRITE_ERROR,
			errno, strerror(errno),
			"while writing hostname.");

		return(DB_IO_ERROR);
	}

	/* The table name includes the NUL byte. */
	ut_a(table->name.m_name != NULL);
	len = static_cast<ib_uint32_t>(strlen(table->name.m_name) + 1);

	/* Write the table name. */
	mach_write_to_4(value, len);

	DBUG_EXECUTE_IF("ib_export_io_write_failure_6", close(fileno(file)););

	if (fwrite(&value, 1,  sizeof(value), file) != sizeof(value)
	    || fwrite(table->name.m_name, 1,  len, file) != len) {

		ib_senderrf(
			thd, IB_LOG_LEVEL_WARN, ER_IO_WRITE_ERROR,
			errno, strerror(errno),
			"while writing table name.");

		return(DB_IO_ERROR);
	}

	byte		row[sizeof(ib_uint32_t) * 3];

	/* Write the next autoinc value. */
	mach_write_to_8(row, table->autoinc);

	DBUG_EXECUTE_IF("ib_export_io_write_failure_7", close(fileno(file)););

	if (fwrite(row, 1,  sizeof(ib_uint64_t), file) != sizeof(ib_uint64_t)) {
		ib_senderrf(
			thd, IB_LOG_LEVEL_WARN, ER_IO_WRITE_ERROR,
			errno, strerror(errno),
			"while writing table autoinc value.");

		return(DB_IO_ERROR);
	}

	byte*		ptr = row;

	/* Write the system page size. */
	mach_write_to_4(ptr, UNIV_PAGE_SIZE);
	ptr += sizeof(ib_uint32_t);

	/* Write the table->flags. */
	mach_write_to_4(ptr, table->flags);
	ptr += sizeof(ib_uint32_t);

	/* Write the number of columns in the table. */
	mach_write_to_4(ptr, table->n_cols);

	DBUG_EXECUTE_IF("ib_export_io_write_failure_8", close(fileno(file)););

	if (fwrite(row, 1,  sizeof(row), file) != sizeof(row)) {
		ib_senderrf(
			thd, IB_LOG_LEVEL_WARN, ER_IO_WRITE_ERROR,
			errno, strerror(errno),
			"while writing table meta-data.");

		return(DB_IO_ERROR);
	}

	return(DB_SUCCESS);
}

/*********************************************************************//**
Write the table meta data after quiesce.
@return DB_SUCCESS or error code */
static	MY_ATTRIBUTE((nonnull, warn_unused_result))
dberr_t
row_quiesce_write_cfg(
/*==================*/
	dict_table_t*	table,	/*!< in: write the meta data for
					this table */
	THD*			thd)	/*!< in/out: session */
{
	dberr_t			err;
	char			name[OS_FILE_MAX_PATH];

	srv_get_meta_data_filename(table, name, sizeof(name));

	ib::info() << "Writing table metadata to '" << name << "'";

	FILE*	file = fopen(name, "w+b");

	if (file == NULL) {
		ib_errf(thd, IB_LOG_LEVEL_WARN, ER_CANT_CREATE_FILE,
			 name, errno, strerror(errno));

		err = DB_IO_ERROR;
	} else {
		err = row_quiesce_write_header(table, file, thd);

		if (err == DB_SUCCESS) {
			err = row_quiesce_write_table(table, file, thd);
		}

		if (err == DB_SUCCESS) {
			err = row_quiesce_write_indexes(table, file, thd);
		}

		if (fflush(file) != 0) {

			char	msg[BUFSIZ];

			ut_snprintf(msg, sizeof(msg), "%s flush() failed",
				    name);

			ib_senderrf(
				thd, IB_LOG_LEVEL_WARN, ER_IO_WRITE_ERROR,
				errno, strerror(errno), msg);
		}

		if (fclose(file) != 0) {
			char	msg[BUFSIZ];

			ut_snprintf(msg, sizeof(msg), "%s flose() failed",
				    name);

			ib_senderrf(
				thd, IB_LOG_LEVEL_WARN, ER_IO_WRITE_ERROR,
				errno, strerror(errno), msg);
		}
	}

	return(err);
}

/** Write the transfer key to CFP file.
@param[in]	table		write the data for this table
@param[in]	file		file to write to
@param[in]	thd		session
@return DB_SUCCESS or error code. */
static	MY_ATTRIBUTE((nonnull, warn_unused_result))
dberr_t
row_quiesce_write_transfer_key(
	const dict_table_t*	table,
	FILE*			file,
	THD*			thd)
{
	byte		key_size[sizeof(ib_uint32_t)];
	byte		row[ENCRYPTION_KEY_LEN * 3];
	byte*		ptr = row;
	byte*		transfer_key = ptr;
	lint		elen;

	ut_ad(table->encryption_key != NULL
	      && table->encryption_iv != NULL);

	/* Write the encryption key size. */
	mach_write_to_4(key_size, ENCRYPTION_KEY_LEN);

	if (fwrite(&key_size, 1,  sizeof(key_size), file)
		!= sizeof(key_size)) {
		ib_senderrf(
			thd, IB_LOG_LEVEL_WARN, ER_IO_WRITE_ERROR,
			errno, strerror(errno),
			"while writing key size.");

		return(DB_IO_ERROR);
	}

	/* Generate and write the transfer key. */
	Encryption::random_value(transfer_key);
	if (fwrite(transfer_key, 1, ENCRYPTION_KEY_LEN, file)
		!= ENCRYPTION_KEY_LEN) {
		ib_senderrf(
			thd, IB_LOG_LEVEL_WARN, ER_IO_WRITE_ERROR,
			errno, strerror(errno),
			"while writing transfer key.");

		return(DB_IO_ERROR);
	}

	ptr += ENCRYPTION_KEY_LEN;

	/* Encrypt tablespace key. */
	elen = my_aes_encrypt(
		reinterpret_cast<unsigned char*>(table->encryption_key),
		ENCRYPTION_KEY_LEN,
		ptr,
		reinterpret_cast<unsigned char*>(transfer_key),
		ENCRYPTION_KEY_LEN,
		my_aes_256_ecb,
		NULL, false);

	if (elen == MY_AES_BAD_DATA) {
		ib_senderrf(
			thd, IB_LOG_LEVEL_WARN, ER_IO_WRITE_ERROR,
			errno, strerror(errno),
			"while encrypt tablespace key.");
		return(DB_ERROR);
	}

	/* Write encrypted tablespace key */
	if (fwrite(ptr, 1, ENCRYPTION_KEY_LEN, file)
		!= ENCRYPTION_KEY_LEN) {
		ib_senderrf(
			thd, IB_LOG_LEVEL_WARN, ER_IO_WRITE_ERROR,
			errno, strerror(errno),
			"while writing encrypted tablespace key.");

		return(DB_IO_ERROR);
	}
	ptr += ENCRYPTION_KEY_LEN;

	/* Encrypt tablespace iv. */
	elen = my_aes_encrypt(
		reinterpret_cast<unsigned char*>(table->encryption_iv),
		ENCRYPTION_KEY_LEN,
		ptr,
		reinterpret_cast<unsigned char*>(transfer_key),
		ENCRYPTION_KEY_LEN,
		my_aes_256_ecb,
		NULL, false);

	if (elen == MY_AES_BAD_DATA) {
		ib_senderrf(
			thd, IB_LOG_LEVEL_WARN, ER_IO_WRITE_ERROR,
			errno, strerror(errno),
			"while encrypt tablespace iv.");
		return(DB_ERROR);
	}

	/* Write encrypted tablespace iv */
	if (fwrite(ptr, 1, ENCRYPTION_KEY_LEN, file)
		!= ENCRYPTION_KEY_LEN) {
		ib_senderrf(
			thd, IB_LOG_LEVEL_WARN, ER_IO_WRITE_ERROR,
			errno, strerror(errno),
			"while writing encrypted tablespace iv.");

		return(DB_IO_ERROR);
	}

	return(DB_SUCCESS);
}

/** Write the encryption data after quiesce.
@param[in]	table		write the data for this table
@param[in]	thd		session
@return DB_SUCCESS or error code */
static	MY_ATTRIBUTE((nonnull, warn_unused_result))
dberr_t
row_quiesce_write_cfp(
	dict_table_t*	table,
	THD*		thd)
{
	dberr_t			err;
	char			name[OS_FILE_MAX_PATH];

	/* If table is not encrypted, return. */
	if (!dict_table_is_encrypted(table)) {
		return(DB_SUCCESS);
	}

	/* Get the encryption key and iv from space */
	/* For encrypted table, before we discard the tablespace,
	we need save the encryption information into table, otherwise,
	this information will be lost in fil_discard_tablespace along
	with fil_space_free(). */
	if (table->encryption_key == NULL) {
		table->encryption_key =
			static_cast<byte*>(mem_heap_alloc(table->heap,
							  ENCRYPTION_KEY_LEN));

		table->encryption_iv =
			static_cast<byte*>(mem_heap_alloc(table->heap,
							  ENCRYPTION_KEY_LEN));

		fil_space_t*	space = fil_space_get(table->space);
		ut_ad(space != NULL && FSP_FLAGS_GET_ENCRYPTION(space->flags));

		memcpy(table->encryption_key,
		       space->encryption_key,
		       ENCRYPTION_KEY_LEN);
		memcpy(table->encryption_iv,
		       space->encryption_iv,
		       ENCRYPTION_KEY_LEN);
	}

	srv_get_encryption_data_filename(table, name, sizeof(name));

	ib::info() << "Writing table encryption data to '" << name << "'";

	FILE*	file = fopen(name, "w+b");

	if (file == NULL) {
		ib_errf(thd, IB_LOG_LEVEL_WARN, ER_CANT_CREATE_FILE,
			 name, errno, strerror(errno));

		err = DB_IO_ERROR;
	} else {
		err = row_quiesce_write_transfer_key(table, file, thd);

		if (fflush(file) != 0) {

			char	msg[BUFSIZ];

			ut_snprintf(msg, sizeof(msg), "%s flush() failed",
				    name);

			ib_senderrf(
				thd, IB_LOG_LEVEL_WARN, ER_IO_WRITE_ERROR,
				errno, strerror(errno), msg);

			err = DB_IO_ERROR;
		}

		if (fclose(file) != 0) {
			char	msg[BUFSIZ];

			ut_snprintf(msg, sizeof(msg), "%s flose() failed",
				    name);

			ib_senderrf(
				thd, IB_LOG_LEVEL_WARN, ER_IO_WRITE_ERROR,
				errno, strerror(errno), msg);
			err = DB_IO_ERROR;
		}
	}

	/* Clean the encryption information */
	table->encryption_key = NULL;
	table->encryption_iv = NULL;

	return(err);
}

/*********************************************************************//**
Check whether a table has an FTS index defined on it.
@return true if an FTS index exists on the table */
static
bool
row_quiesce_table_has_fts_index(
/*============================*/
	const dict_table_t*	table)	/*!< in: quiesce this table */
{
	bool			exists = false;

	dict_mutex_enter_for_mysql();

	for (const dict_index_t* index = UT_LIST_GET_FIRST(table->indexes);
	     index != 0;
	     index = UT_LIST_GET_NEXT(indexes, index)) {

		if (index->type & DICT_FTS) {
			exists = true;
			break;
		}
	}

	dict_mutex_exit_for_mysql();

	return(exists);
}

/*********************************************************************//**
Quiesce the tablespace that the table resides in. */
void
row_quiesce_table_start(
/*====================*/
	dict_table_t*	table,		/*!< in: quiesce this table */
	trx_t*		trx)		/*!< in/out: transaction/session */
{
	ut_a(trx->mysql_thd != 0);
	ut_a(srv_n_purge_threads > 0);
	ut_ad(!srv_read_only_mode);

	ut_a(trx->mysql_thd != 0);

	ut_ad(fil_space_get(table->space) != NULL);

	ib::info() << "Sync to disk of " << table->name << " started.";

	if (trx_purge_state() != PURGE_STATE_DISABLED) {
		trx_purge_stop();
	}

	for (ulint count = 0;
	     ibuf_merge_space(table->space) != 0
	     && !trx_is_interrupted(trx);
	     ++count) {
		if (!(count % 20)) {
			ib::info() << "Merging change buffer entries for "
				<< table->name;
		}
	}

	if (!trx_is_interrupted(trx)) {
		extern	ib_mutex_t	master_key_id_mutex;

		if (dict_table_is_encrypted(table)) {
			/* Require the mutex to block key rotation. */
			mutex_enter(&master_key_id_mutex);
		}

		buf_LRU_flush_or_remove_pages(
			table->space, BUF_REMOVE_FLUSH_WRITE, trx);

		if (dict_table_is_encrypted(table)) {
			mutex_exit(&master_key_id_mutex);
		}

		if (trx_is_interrupted(trx)) {

			ib::warn() << "Quiesce aborted!";

		} else if (row_quiesce_write_cfg(table, trx->mysql_thd)
			   != DB_SUCCESS) {

			ib::warn() << "There was an error writing to the"
				" meta data file";
		} else if (row_quiesce_write_cfp(table, trx->mysql_thd)
			   != DB_SUCCESS) {
			ib::warn() << "There was an error writing to the"
				" encryption info file";
		} else {
			ib::info() << "Table " << table->name
				<< " flushed to disk";
		}
	} else {
		ib::warn() << "Quiesce aborted!";
	}

	dberr_t	err = row_quiesce_set_state(table, QUIESCE_COMPLETE, trx);
	ut_a(err == DB_SUCCESS);
}

/*********************************************************************//**
Cleanup after table quiesce. */
void
row_quiesce_table_complete(
/*=======================*/
	dict_table_t*	table,		/*!< in: quiesce this table */
	trx_t*		trx)		/*!< in/out: transaction/session */
{
	ulint		count = 0;

	ut_a(trx->mysql_thd != 0);

	/* We need to wait for the operation to complete if the
	transaction has been killed. */

	while (table->quiesce != QUIESCE_COMPLETE) {

		/* Print a warning after every minute. */
		if (!(count % 60)) {
			ib::warn() << "Waiting for quiesce of " << table->name
				<< " to complete";
		}

		/* Sleep for a second. */
		os_thread_sleep(1000000);

		++count;
	}

	/* Remove the .cfg file now that the user has resumed
	normal operations. Otherwise it will cause problems when
	the user tries to drop the database (remove directory). */
	char		cfg_name[OS_FILE_MAX_PATH];

	srv_get_meta_data_filename(table, cfg_name, sizeof(cfg_name));

	os_file_delete_if_exists(innodb_data_file_key, cfg_name, NULL);

	ib::info() << "Deleting the meta-data file '" << cfg_name << "'";

	if (dict_table_is_encrypted(table)) {
		char		cfp_name[OS_FILE_MAX_PATH];

		srv_get_encryption_data_filename(table,
						 cfp_name,
						 sizeof(cfp_name));

		os_file_delete_if_exists(innodb_data_file_key, cfp_name, NULL);

		ib::info() << "Deleting the meta-data file '"
			<< cfp_name << "'";
	}

	if (trx_purge_state() != PURGE_STATE_DISABLED) {
		trx_purge_run();
	}

	dberr_t	err = row_quiesce_set_state(table, QUIESCE_NONE, trx);
	ut_a(err == DB_SUCCESS);
}

/*********************************************************************//**
Set a table's quiesce state.
@return DB_SUCCESS or error code. */
dberr_t
row_quiesce_set_state(
/*==================*/
	dict_table_t*	table,		/*!< in: quiesce this table */
	ib_quiesce_t	state,		/*!< in: quiesce state to set */
	trx_t*		trx)		/*!< in/out: transaction */
{
	ut_a(srv_n_purge_threads > 0);

	if (srv_read_only_mode) {

		ib_senderrf(trx->mysql_thd,
			    IB_LOG_LEVEL_WARN, ER_READ_ONLY_MODE);

		return(DB_UNSUPPORTED);

	} else if (dict_table_is_temporary(table)) {

		ib_senderrf(trx->mysql_thd, IB_LOG_LEVEL_WARN,
			    ER_CANNOT_DISCARD_TEMPORARY_TABLE);

		return(DB_UNSUPPORTED);
	} else if (table->space == srv_sys_space.space_id()) {

		char	table_name[MAX_FULL_NAME_LEN + 1];

		innobase_format_name(
			table_name, sizeof(table_name),
			table->name.m_name);

		ib_senderrf(trx->mysql_thd, IB_LOG_LEVEL_WARN,
			    ER_TABLE_IN_SYSTEM_TABLESPACE, table_name);

		return(DB_UNSUPPORTED);

	} else if (DICT_TF_HAS_SHARED_SPACE(table->flags)) {
		std::ostringstream err_msg;
		err_msg << "FLUSH TABLES FOR EXPORT on table " << table->name
			<< " in a general tablespace.";
		ib_senderrf(trx->mysql_thd, IB_LOG_LEVEL_WARN,
			    ER_NOT_SUPPORTED_YET, err_msg.str().c_str());

		return(DB_UNSUPPORTED);
	} else if (row_quiesce_table_has_fts_index(table)) {

		ib_senderrf(trx->mysql_thd, IB_LOG_LEVEL_WARN,
			    ER_NOT_SUPPORTED_YET,
			    "FLUSH TABLES on tables that have an FTS index."
			    " FTS auxiliary tables will not be flushed.");

	} else if (DICT_TF2_FLAG_IS_SET(table, DICT_TF2_FTS_HAS_DOC_ID)) {
		/* If this flag is set then the table may not have any active
		FTS indexes but it will still have the auxiliary tables. */

		ib_senderrf(trx->mysql_thd, IB_LOG_LEVEL_WARN,
			    ER_NOT_SUPPORTED_YET,
			    "FLUSH TABLES on a table that had an FTS index,"
			    " created on a hidden column, the"
			    " auxiliary tables haven't been dropped as yet."
			    " FTS auxiliary tables will not be flushed.");
	}

	row_mysql_lock_data_dictionary(trx);

	dict_table_x_lock_indexes(table);

	switch (state) {
	case QUIESCE_START:
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


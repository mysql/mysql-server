/*****************************************************************************

Copyright (c) 2017, Oracle and/or its affiliates. All Rights Reserved.

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
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA

*****************************************************************************/

/**************************************************//**
@file log/log0ddl.h
DDL log

Created 12/1/2016 Shaohua Wang
*******************************************************/

#ifndef log0ddl_h
#define log0ddl_h

#include "univ.i"
#include "dict0types.h"
#include "que0types.h"
#include "trx0types.h"

extern thread_local bool thread_local_ddl_log_replay;

class LogDDL
{
public:
	/** Constructor */
	LogDDL();

	/** Deconstructor */
	~LogDDL()
	{}

	/** Write DDL log for freeing B-tree
	@param[in,out]	trx		transaction
	@param[in]	index		dict index
	@param[in]	is_drop		flag whether dropping index
	@return	DB_SUCCESS or error */
	dberr_t writeFreeTreeLog(
		trx_t*			trx,
		const dict_index_t*	index,
		bool			is_drop_table);

	/** Write DDL log for deleting tablespace file
	@param[in,out]	trx		transaction
	@param[in]	table		dict table
	@param[in]	space_id	tablespace id
	@param[in]	file_path	file path
	@param[in]	is_drop		flag whether dropping tablespace
	@param[in]	dick_locked	true if dict_sys mutex is held
	@return	DB_SUCCESS or error */
	dberr_t writeDeleteSpaceLog(
		trx_t*			trx,
		const dict_table_t*	table,
		space_id_t		space_id,
		const char*		file_path,
		bool			is_drop,
		bool			dict_locked);

	/** Write a RENAME log record
	@param[in]	id		log id
	@param[in]	space_id	tablespace id
	@param[in]	old_file_path	file path after rename
	@param[in]	new_file_path	file path before rename
	@return DB_SUCCESS or error */
	dberr_t writeRenameSpaceLog(
		trx_t*			trx,
		space_id_t		space_id,
		const char*		old_file_path,
		const char*		new_file_path);

	/** Write a DROP log to indicate the entry in innodb_table_metadata
	should be removed for specified table
	@param[in,out]	trx		transaction
	@param[in]	table_id	table ID
	@return DB_SUCCESS or error */
	dberr_t writeDropLog(
		trx_t*			trx,
		const table_id_t	table_id);

	/** Write a RENAME TABLE log record
	@param[in]	trx		transaction
	@param[in]	table		dict table
	@param[in]	old_name	table name after rename
	@param[in]	new_name	table name before rename
	@return DB_SUCCESS or error */
	dberr_t writeRenameTableLog(
		trx_t*		trx,
		dict_table_t*	table,
		const char*	old_name,
		const char*	new_name);

	/** Write a REMOVE cache log record
	@param[in]	trx		transaction
	@param[in]	table		dict table
	@return DB_SUCCESS or error */
	dberr_t writeRemoveCacheLog(
		trx_t*		trx,
		dict_table_t*	table);

	/** Replay and clean DDL logs after DDL transaction
	commints or rollbacks.
	@param[in]	thd	mysql thread
	@return	DB_SUCCESS or error */
	dberr_t postDDL(THD*	thd);

	/** Recover in server startup.
	Scan innodb_ddl_log table, and replay all log entries.
	Note: redo log should be applied, and dict transactions
	should be recovered before calling this function.
	@return	DB_SUCCESS or error */
	dberr_t recover();

	/** Scan and replay all log records
	@param[in]	trx		transaction instance
	@return DB_SUCCESS or error */
	dberr_t	printAll();

private:

	/** DDL log record */
	struct	Record {
		/** Log id */
		ulint		id;

		/** Log type */
		ulint		type;

		/** Thread id */
		ulint		thread_id;

		/** Tablespace id */
		space_id_t	space_id;

		/** Index root page no */
		page_no_t	page_no;

		/** Index id */
		ulint		index_id;

		/** Table id */
		table_id_t	table_id;

		/** Tablespace file path for DELETE,
		Old tablespace file path for RENAME */
		char*		old_file_path;

		/** New tablespace file name for RENAME */
		char*		new_file_path;
	};

	/** Read and replay DDL log record
	@param[in]	row	DDL log row
	@param[in]	arg	argument passed down
	@return TRUE on success, FALSE on failure */
	static ibool readAndReplay(
		void*		row,
		void*		arg);

	/** Read and print DDL log record
	@param[in]	row	DDL log row
	@param[in]	arg	argument passed down
	@return TRUE on success, FALSE on failure */
	static ibool printRecord(
		void*		row,
		void*		arg);

	/** Read DDL log record
	@param[in]	exp	query node expression
	@param[in,out]	record	DDL log record
	@param[in,out]	heap	mem heap
	@return DB_SUCCESS or error */
	static dberr_t read(
		que_node_t*	exp,
		Record&		record,
		mem_heap_t*	heap);

	/** Replay DDL log record
	@param[in,out]	record	DDL log record
	return DB_SUCCESS or error */
	static dberr_t replay(
//		trx_t*		trx,
		Record&		record);

	/** Insert a FREE log record
	@param[in,out]	trx		transaction
	@param[in]	index		dict index
	@param[in]	id		log id
	@param[in]	thread_id	thread id
	@return DB_SUCCESS or error */
	dberr_t	insertFreeTreeLog(
		trx_t*			trx,
		const dict_index_t*	index,
		ib_uint64_t		id,
		ulint			thread_id);

	/** Replay FREE log(free B-tree if exist)
	@param[in]	space_id	tablespace id
	@param[in]	page_no		root page no
	@param[in]	index_id	index id */
	static void replayFreeLog(
		space_id_t	space_id,
		page_no_t	page_no,
		ulint		index_id);

	/** Insert a DELETE log record
	@param[in]	id		log id
	@param[in]	thread_id	thread id
	@param[in]	space_id	tablespace id
	@param[in]	file_path	file path
	@param[in]	dict_locked	true if dict_sys mutex is held
	@return DB_SUCCESS or error */
	dberr_t insertDeleteSpaceLog(
		trx_t*			trx,
		ib_uint64_t		id,
		ulint			thread_id,
		space_id_t		space_id,
		const char*		file_path,
		bool			dict_locked);

	/** Replay DELETE log(delete file if exist)
	@param[in]	space_id	tablespace id
	@param[in]	file_path	file path */
	static void replayDeleteLog(
		space_id_t	space_id,
		const char*	file_path);

	/** Insert a RENAME log record
	@param[in]	id		log id
	@param[in]	thread_id	thread id
	@param[in]	space_id	tablespace id
	@param[in]	old_file_path	file path after rename
	@param[in]	new_file_path	file path before rename
	@return DB_SUCCESS or error */
	dberr_t insertRenameLog(
		ib_uint64_t		id,
		ulint			thread_id,
		space_id_t		space_id,
		const char*		old_file_path,
		const char*		new_file_path);

	/** Relay RENAME log
	@param[in]	space_id	tablespace id
	@param[in]	old_file_path	old file path
	@param[in]	new_file_path	new file path */
	static void replayRenameLog(
		space_id_t		space_id,
		const char*		old_file_path,
		const char*		new_file_path);

	/** Insert a DROP log record
	@parampin,out]	trx		transaction
	@param[in]	id		log id
	@param[in]	thread_id	thread id
	@param[in]	table_id	table id
	@return DB_SUCCESS or error */
	static dberr_t insertDropLog(
		trx_t*			trx,
		ib_uint64_t		id,
		ulint			thread_id,
		const table_id_t	table_id);

	/** Replay DROP log
	@param[in]	table_id	table id */
	static void replayDropLog(
		const table_id_t	table_id);

	/** Insert a RENAME TABLE log record
	@param[in]	id		log id
	@param[in]	thread_id	thread id
	@param[in]	table_id	table id
	@param[in]	old_name	table name after rename
	@param[in]	new_name	table name before rename
	@return DB_SUCCESS or error */
	static dberr_t insertRenameTableLog(
		ib_uint64_t		id,
		ulint			thread_id,
		table_id_t		table_id,
		const char*		old_name,
		const char*		new_name);

	/** Relay RENAME TABLE log
	@param[in]	table_id	table id
	@param[in]	old_name	old name
	@param[in]	new_name	new name */
	static void replayRenameTableLog(
		table_id_t	table_id,
		const char*	old_name,
		const char*	new_name);

	/** Insert a REMOVE cache log record
	@param[in]	id		log id
	@param[in]	thread_id	thread id
	@param[in]	table_id	table id
	@param[in]	table_name	table name
	@return DB_SUCCESS or error */
	static dberr_t insertRemoveCacheLog(
		ib_uint64_t		id,
		ulint			thread_id,
		table_id_t		table_id,
		const char*		table_name);

	/** Relay remove cache log
	@param[in]	table_id	table id
	@param[in]	table_name	table name */
	static void replayRemoveLog(
		table_id_t	table_id,
		const char*	table_name);

	/** Delete log record by id
	@param[in]	trx	transaction instance
	@param[in]	id	log id
	@return DB_SUCCESS or error */
	dberr_t	deleteById(
		trx_t*		trx,
		ib_uint64_t	id);

	/** Delete log records by thread id
	@param[in]	trx		transaction instance
	@param[in]	thread_id	thread id
	@return DB_SUCCESS or error */
	dberr_t deleteByThreadId(
		trx_t*		trx,
		ulint		thread_id);

	/** Delete all log records
	@param[in]	trx	transaction instance
	@return DB_SUCCESS or error */
	dberr_t deleteAll(
		trx_t*		trx);

	/** Scan and replay log records by thread id
	@param[in]	trx		transaction instance
	@param[in]	thread_id	thread id
	@return DB_SUCCESS or error */
	dberr_t	scanByThreadId(
		trx_t*		trx,
		ulint		thread_id);

	/** Scan and replay all log records
	@param[in]	trx		transaction instance
	@return DB_SUCCESS or error */
	dberr_t	scanAll(
		trx_t*		trx);

	enum LogType {

			/** Drop an index tree */
			FREE_LOG = 1,

			/** Delete a file */
			DELETE_LOG,

			/** Rename a file */
			RENAME_LOG,

			/** Drop the entry in innodb_table_metadata */
			DROP_LOG,

			/** Rename table in dict cache. */
			RENAME_TABLE_LOG,

			/** Remove a table from dict cache */
			REMOVE_LOG
	};

	/** Get next autoinc counter by increasing 1 for innodb_ddl_log
	@return	new next counter */
	inline ib_uint64_t getNextId();

	/** Check if we need to skip ddl log for a table.
	@param[in]	table	dict table
	@param[in]	thd	mysql thread
	@return true if should skip, otherwise false */
	inline bool shouldSkip(
		const dict_table_t*	table,
		THD*			thd);

	/** Whether in recover(replay) ddl log in startup. */
	static bool in_recovery;
};

#endif /* log0ddl_h */

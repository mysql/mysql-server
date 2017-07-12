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
@file include/log0ddl.h
DDL log

Created 12/1/2016 Shaohua Wang
*******************************************************/

#ifndef log0ddl_h
#define log0ddl_h

#include "que0types.h"

extern thread_local bool thread_local_ddl_log_replay;

/** DDL log record */
class DDL_Record {

public:
	/** Constructor. */
	DDL_Record();

	/** Destructor. */
	~DDL_Record();

	/** Get the id of the DDL log record.
	@return id of the record. */
	ulint get_id() const;

	/** Set the id for the DDL log record.
	@param[in]	id	id of the record. */
	void set_id(ulint id);

	/** Get the type of operation to perform
	for the DDL log record.
	@return type of the record. */
	ulint get_type() const;

	/** Set the type for the DDL log record.
	@param[in]	record_type	set the record type.*/
	void set_type(ulint record_type);

	/** Get the thread id for the DDL log record.
	@return thread id of the DDL log record. */
	ulint get_thread_id() const;

	/** Set the thread id for the DDL log record.
	@param[in]	thr_id	thread id. */
	void set_thread_id(ulint thr_id);

	/** Get the space_id present in the DDL log record.
	@return space_id in the DDL log record. */
	space_id_t get_space_id() const;

	/** Set the space id for the DDL log record.
	@param[in]	space	space id. */
	void set_space_id(space_id_t space);

	/** Get the page no present in the DDL log record.
	@return page_no */
	page_no_t get_page_no() const;

	/** Set the page number for the DDL log record.
	@param[in]	page_no	page number. */
	void set_page_no(page_no_t page_no);

	/** Get the index id present in the DDL log record.
	@return index id. */
	ulint get_index_id() const;

	/** Set the index id for the DDL log record.
	@param[in]	ind_id	index id. */
	void set_index_id(ulint ind_id);

	/** Get the table id present in the DDL log record.
	@return table id from the record. */
	table_id_t get_table_id() const;

	/** Set the table if for the DDL log record.
	@param[in]	table_id	table id. */
	void set_table_id(table_id_t table_id);

	/** Get the old file path/name present in the DDL log record.
	@return old file path/name. */
	const char* get_old_file_path() const;

	/** Set the old file path from the name for the DDL log record.
	@param[in]	name	old file name. */
	void set_old_file_path(const char* name);

	/** Copy the data and set it in old file path
	@param[in]	data	data to be set
	@param[in]	len	length of the data. */
	void set_old_file_path(const byte* data, ulint len);

	/** Get the new file path/name present in the DDL log record.
	@return new file path/name. */
	const char* get_new_file_path() const;

	/** Set the new file path/name for the DDL log record.
	@param[in]	name	name to be set. */
	void set_new_file_path(const char* name);

	/** Copy the data and set it in new file path.
	@param[in]	data	data to be set
	@param[in]	len	length of the data. */
	void set_new_file_path(const byte* data, ulint len);

	/** Set the given field of the innodb_ddl_log record from
	given data.
	@param[in]	data	data to be set
	@param[in]	offset	column of the ddl record
	@param[in]	len	length of the data. */
	void set_field(const byte* data, ulint offset, ulint len);

	/** Column number of mysql.innodb_ddl_log.id. */
	static constexpr unsigned	ID_COL_NO = 0;
	/** Column length of mysql.innodb_ddl_log.id. */
	static constexpr unsigned	ID_COL_LEN = 8;
	/** Column number of mysql.innodb_ddl_log.thread_id. */
	static constexpr unsigned	THREAD_ID_COL_NO = 1;
	/** Column length of mysql.innodb_ddl_log.thread_id. */
	static constexpr unsigned	THREAD_ID_COL_LEN = 8;
	/** Column number of mysql.innodb_ddl_log.type. */
	static constexpr unsigned	TYPE_COL_NO = 2;
	/** Column length of mysql.innodb_ddl_log.type. */
	static constexpr unsigned	TYPE_COL_LEN = 4;
	/** Column number of mysql.innodb_ddl_log.space_id. */
	static constexpr unsigned	SPACE_ID_COL_NO = 3;
	/** Column length of mysql.innodb_ddl_log.space_id. */
	static constexpr unsigned	SPACE_ID_COL_LEN = 4;
	/** Column number of mysql.innodb_ddl_log.page_no. */
	static constexpr unsigned	PAGE_NO_COL_NO = 4;
	/** Column length of mysql.innodb_ddl_log.page_no. */
	static constexpr unsigned	PAGE_NO_COL_LEN = 4;
	/** Column number of mysql.innodb_ddl_log.index_id. */
	static constexpr unsigned	INDEX_ID_COL_NO = 5;
	/** Column length of mysql.innodb_ddl_log.index_id. */
	static constexpr unsigned	INDEX_ID_COL_LEN = 8;
	/** Column number of mysql.innodb_ddl_log.table_id. */
	static constexpr unsigned	TABLE_ID_COL_NO = 6;
	/** Column length of mysql.innodb_ddl_log.table_id. */
	static constexpr unsigned	TABLE_ID_COL_LEN = 8;
	/** Column number of mysql.innodb_ddl_log.old_file_path. */
	static constexpr unsigned	OLD_FILE_PATH_COL_NO = 7;
	/** Column number of mysql.innodb_ddl_log.new_file_path. */
	static constexpr unsigned	NEW_FILE_PATH_COL_NO = 8;
private:
	/** Fetch the value from given offset.
	@param[in]	data	value to be retrieved from data
	@param[in]	offset	offset of the column
	@return value of the given offset. */
	ulint fetch_value(const byte* data, ulint offset);

	/** Log id */
	ulint		m_id;

	/** Log type */
	ulint		m_type;

	/** Thread id */
	ulint		m_thread_id;

	/** Tablespace id */
	space_id_t	m_space_id;

	/** Index root page */
	page_no_t	m_page_no;

	/** Index id */
	ulint		m_index_id;

	/** Table id */
	table_id_t	m_table_id;

	/** Tablespace file path for DELETE,
	Old tablespace file path for RENAME */
	char*		m_old_file_path;

	/** New tablespace file name for RENAME */
	char*		m_new_file_path;

	/** memory heap object used for storing file name. */
	mem_heap_t*	m_heap;
};

using DDL_Records = std::vector<DDL_Record*>;

using DDL_Record_Ids = std::vector<ulint>;

class DDL_Log_Table
{
public:
	/** Constructor. */
	DDL_Log_Table();

	/** Constructor and it initalizes transaction
	and query thread. */
	DDL_Log_Table(trx_t* trx);

	/** Destructor. */
	~DDL_Log_Table();

	/** Get the list of ddl records ids.
	@return list of ddl record ids. */
	DDL_Record_Ids& get_ids();

	/** Get the list of ddl records.
	@return list of ddl records. */
	DDL_Records& get_records();

	/** Insert the DDL log record in the innodb_ddl_log table.
	@param[in]	record	Record to be inserted.
	@return DB_SUCCESS or error. */
	dberr_t insert(DDL_Record& record);

	/** The function does the following
	1) Scan the given id record and replay the given id record.
	2) Scan the given thread id and store the record id in the
	list.
	@param[in]	id	thread id for scanByThreadID (or)
				id for scanById (or)
				ULINT_UNDEFINED for scanAll operation
	@param[in]	type	type of the operation
	@return DB_SUCCESS or error. */
	dberr_t search(ulint id, ulint type);

	/** Scan the table from right to left and replay all the record.
	@return DB_SUCCESS or error. */
	dberr_t search();

	/** Scan the table for the ddl_record_ids and replay the operation.
	@param[in]	type	SEARCH THREAD ID operation.
	@return DB_SUCCESS or error. */
	dberr_t search(ulint type);

	/** Delete all the innodb_ddl_log records from the table.
	@param[in]	id	thread id/id of the record
	@param[in]	type	type of the operation
	@return DB_SUCCESS or error. */
	dberr_t remove(ulint id, ulint type);

	/** Delete all innodb_ddl_log records in the given list.
	@param[in]	ddl_record_ids	list of ddl record id
	@param[in]	type		Operation to be perform
	@return DB_SUCCESS or error. */
	dberr_t remove(DDL_Record_Ids& ddl_record_ids, ulint type);

	/** Search the table using record id operation. */
	static constexpr unsigned	SEARCH_ID_OP = 0;
	/** Search the table using thread id operation. */
	static constexpr unsigned	SEARCH_THREAD_ID_OP = 1;
	/** Remove the records using the record id list operation. */
	static constexpr unsigned	DELETE_LIST_OP = 2;
	/** Remove the record using the record id operation. */
	static constexpr unsigned	DELETE_ID_OP = 3;

	/** Stop the query thread. */
	void stop_query_thread();

private:

	/** Set the query thread using graph. */
	void start_query_thread();

	/** Create tuple for the innodb_ddl_log table.
	It is used for insert operation.
	@param[in]	record	DDL log record. */
	void create_tuple(const DDL_Record& record);

	/** Create tuple for the given index.
	It can be used for delete or search operation.
	@param[in]	id	Thread id/ id of the record
	@param[in]	index	Clustered index or secondary index. */
	void create_tuple(ulint id, const dict_index_t* index);

	/** Convert the innodb_ddl_log clustered index record
	to ddl_record structure format.
	@param[in]	clust_rec	clustered index record
	@param[in]	clust_offsets	clustered index record offset
	@param[in,out]	ddl_record	structure to fill the data from
					innodb_ddl_log record. */
	void
	convert_to_ddl_record(
		rec_t*		clust_rec,
		ulint*		clust_offsets,
		DDL_Record&	ddl_record);

	/** Parse the secondary index record and get
	the id in the list.
	@param[in]	rec	secondary index rec
	@param[in]	offsets	offsets for secondary index.
	@return id of the record. */
	ulint
	fetch_id_from_sec_rec_index(
		rec_t*		rec,
		ulint*		offsets);

	/** Get the index of the table based on the given operation.
	@param[in]	type	Operation type
	@return clustered index or secondary index. */
	dict_index_t* get_index(ulint type);

private:
	/** innodb_ddl_log table. */
	dict_table_t*		m_table;

	/** Tuple used for insert, search, delete operation. */
	dtuple_t*		m_tuple;

	/** Transaction used for insert, delete operation. */
	trx_t*			m_trx;

	/** Dummy query thread. */
	que_thr_t*		m_thr;

	/** List of ddl record ids and it is used for
	scan_and_deleteAll, scan_and_deleteByThread operation. */
	DDL_Record_Ids		m_ddl_record_ids;

	/** List of ddl records and it can be used for
	replay operation. */
	DDL_Records		m_ddl_records;

	/** Heap to store the m_tuple, m_thr and all
	operation on mysql.innodb_ddl_log table. */
	mem_heap_t*		m_heap;
};

class Log_DDL
{
public:
	/** Constructor */
	Log_DDL();

	/** Deconstructor */
	~Log_DDL()
	{}

	/** Write DDL log for freeing B-tree
	@param[in,out]	trx		transaction
	@param[in]	index		dict index
	@param[in]	is_drop		flag whether dropping index
	@return	DB_SUCCESS or error */
	dberr_t write_free_tree_log(
		trx_t*			trx,
		const dict_index_t*	index,
		bool			is_drop_table);

	/** Write DDL log for deleting tablespace file
	@param[in,out]	trx		transaction
	@param[in]	table		dict table
	@param[in]	space_id	tablespace id
	@param[in]	file_path	file path
	@param[in]	is_drop		flag whether dropping tablespace
	@param[in]	dict_locked	true if dict_sys mutex is held
	@return	DB_SUCCESS or error */
	dberr_t write_delete_space_log(
		trx_t*			trx,
		const dict_table_t*	table,
		space_id_t		space_id,
		const char*		file_path,
		bool			is_drop,
		bool			dict_locked);

	/** Write a RENAME log record
	@param[in]	trx		transaction
	@param[in]	space_id	tablespace id
	@param[in]	old_file_path	file path after rename
	@param[in]	new_file_path	file path before rename
	@return DB_SUCCESS or error */
	dberr_t write_rename_space_log(
		trx_t*			trx,
		space_id_t		space_id,
		const char*		old_file_path,
		const char*		new_file_path);

	/** Write a DROP log to indicate the entry in innodb_table_metadata
	should be removed for specified table
	@param[in,out]	trx		transaction
	@param[in]	table_id	table ID
	@return DB_SUCCESS or error */
	dberr_t write_drop_log(
		trx_t*			trx,
		const table_id_t	table_id);

	/** Write a RENAME TABLE log record
	@param[in]	trx		transaction
	@param[in]	table		dict table
	@param[in]	old_name	table name after rename
	@param[in]	new_name	table name before rename
	@return DB_SUCCESS or error */
	dberr_t write_rename_table_log(
		trx_t*		trx,
		dict_table_t*	table,
		const char*	old_name,
		const char*	new_name);

	/** Write a REMOVE cache log record
	@param[in]	trx		transaction
	@param[in]	table		dict table
	@return DB_SUCCESS or error */
	dberr_t write_remove_cache_log(
		trx_t*		trx,
		dict_table_t*	table);

	/** Replay DDL log record
	@param[in,out]	record	DDL log record
	return DB_SUCCESS or error */
	static dberr_t replay(
		DDL_Record&	record);

	/** Replay and clean DDL logs after DDL transaction
	commints or rollbacks.
	@param[in]	thd	mysql thread
	@return	DB_SUCCESS or error */
	dberr_t post_DDL(THD*	thd);

	/** Recover in server startup.
	Scan innodb_ddl_log table, and replay all log entries.
	Note: redo log should be applied, and dict transactions
	should be recovered before calling this function.
	@return	DB_SUCCESS or error */
	dberr_t recover();

	/** Is it in ddl recovery in server startup.
	@return	true if it's in ddl recover */
	static bool is_in_recovery() {
		return in_recovery;
	}

private:

	/** Insert a FREE log record
	@param[in,out]	trx		transaction
	@param[in]	index		dict index
	@param[in]	id		log id
	@param[in]	thread_id	thread id
	@return DB_SUCCESS or error */
	dberr_t	insert_free_tree_log(
		trx_t*			trx,
		const dict_index_t*	index,
		ib_uint64_t		id,
		ulint			thread_id);

	/** Replay FREE log(free B-tree if exist)
	@param[in]	space_id	tablespace id
	@param[in]	page_no		root page no
	@param[in]	index_id	index id */
	static void replay_free_log(
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
	dberr_t insert_delete_space_log(
		trx_t*			trx,
		ib_uint64_t		id,
		ulint			thread_id,
		space_id_t		space_id,
		const char*		file_path,
		bool			dict_locked);

	/** Replay DELETE log(delete file if exist)
	@param[in]	space_id	tablespace id
	@param[in]	file_path	file path */
	static void replay_delete_log(
		space_id_t	space_id,
		const char*	file_path);

	/** Insert a RENAME log record
	@param[in]	id		log id
	@param[in]	thread_id	thread id
	@param[in]	space_id	tablespace id
	@param[in]	old_file_path	file path after rename
	@param[in]	new_file_path	file path before rename
	@return DB_SUCCESS or error */
	dberr_t insert_rename_log(
		ib_uint64_t		id,
		ulint			thread_id,
		space_id_t		space_id,
		const char*		old_file_path,
		const char*		new_file_path);

	/** Relay RENAME log
	@param[in]	space_id	tablespace id
	@param[in]	old_file_path	old file path
	@param[in]	new_file_path	new file path */
	static void replay_rename_log(
		space_id_t		space_id,
		const char*		old_file_path,
		const char*		new_file_path);

	/** Insert a DROP log record
	@param[in,out]	trx		transaction
	@param[in]	id		log id
	@param[in]	thread_id	thread id
	@param[in]	table_id	table id
	@return DB_SUCCESS or error */
	static dberr_t insert_drop_log(
		trx_t*			trx,
		ib_uint64_t		id,
		ulint			thread_id,
		const table_id_t	table_id);

	/** Replay DROP log
	@param[in]	table_id	table id */
	static void replay_drop_log(
		const table_id_t	table_id);

	/** Insert a RENAME TABLE log record
	@param[in]	id		log id
	@param[in]	thread_id	thread id
	@param[in]	table_id	table id
	@param[in]	old_name	table name after rename
	@param[in]	new_name	table name before rename
	@return DB_SUCCESS or error */
	static dberr_t insert_rename_table_log(
		ib_uint64_t		id,
		ulint			thread_id,
		table_id_t		table_id,
		const char*		old_name,
		const char*		new_name);

	/** Relay RENAME TABLE log
	@param[in]	table_id	table id
	@param[in]	old_name	old name
	@param[in]	new_name	new name */
	static void replay_rename_table_log(
		table_id_t	table_id,
		const char*	old_name,
		const char*	new_name);

	/** Insert a REMOVE cache log record
	@param[in]	id		log id
	@param[in]	thread_id	thread id
	@param[in]	table_id	table id
	@param[in]	table_name	table name
	@return DB_SUCCESS or error */
	static dberr_t insert_remove_cache_log(
		ib_uint64_t		id,
		ulint			thread_id,
		table_id_t		table_id,
		const char*		table_name);

	/** Relay remove cache log
	@param[in]	table_id	table id
	@param[in]	table_name	table name */
	static void replay_remove_log(
		table_id_t	table_id,
		const char*	table_name);

	/** Delete log record by id
	@param[in]	trx	transaction instance
	@param[in]	id	log id
	@return DB_SUCCESS or error */
	dberr_t	delete_by_id(
		trx_t*		trx,
		ib_uint64_t	id);

	/** Scan and replay log records by thread id
	@param[in]	trx		transaction instance
	@param[in]	thread_id	thread id
	@param[out]	ids_list	list of ddl records ids
	@return DB_SUCCESS or error */
	dberr_t	replay_by_thread_id(
		trx_t*			trx,
		ulint			thread_id,
		DDL_Record_Ids&		ids_list);

	/** Delete the log records present in the list.
	@param[in]	trx		transaction instance
	@param[in]	ids_list	list of ddl record ids
	@return DB_SUCCESS or error. */
	dberr_t delete_by_ids(
		trx_t*			trx,
		DDL_Record_Ids&		ids_list);

	/** Scan and replay all log records
	@param[out]	ids_list	list of ddl records ids
	@return DB_SUCCESS or error */
	dberr_t	replay_all(
		DDL_Record_Ids&		ids_list);

	enum Log_Type {
		/** Drop an index tree */
		FREE_TREE_LOG = 1,

		/** Delete a file */
		DELETE_SPACE_LOG,

		/** Rename a file */
		RENAME_LOG,

		/** Drop the entry in innodb_table_metadata */
		DROP_LOG,

		/** Rename table in dict cache. */
		RENAME_TABLE_LOG,

		/** Remove a table from dict cache */
		REMOVE_CACHE_LOG
	};

	/** Get next autoinc counter by increasing 1 for innodb_ddl_log
	@return	new next counter */
	inline ib_uint64_t next_id();

	/** Check if we need to skip ddl log for a table.
	@param[in]	table	dict table
	@param[in]	thd	mysql thread
	@return true if should skip, otherwise false */
	inline bool skip(
		const dict_table_t*	table,
		THD*			thd);

	/** Whether in recover(replay) ddl log in startup. */
	static bool in_recovery;
};

#endif /* log0ddl_h */

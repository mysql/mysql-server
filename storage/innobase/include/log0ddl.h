/*****************************************************************************

Copyright (c) 2017, 2018, Oracle and/or its affiliates. All Rights Reserved.

Portions of this file contain modifications contributed and copyrighted by
Google, Inc. Those modifications are gratefully acknowledged and are described
briefly in the InnoDB documentation. The contributions by Google are
incorporated with their permission, and subject to the conditions contained in
the file COPYING.Google.

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

/** @file include/log0ddl.h
 DDL log

 Created 12/1/2016 Shaohua Wang
 *******************************************************/

#ifndef log0ddl_h
#define log0ddl_h

/** DDL log types defined as uint32_t because it costs 4 bytes in
mysql.innodb_ddl_log. */
enum class Log_Type : uint32_t {
  /** Smallest log type */
  SMALLEST_LOG = 1,

  /** Drop an index tree */
  FREE_TREE_LOG = 1,

  /** Delete a file */
  DELETE_SPACE_LOG,

  /** Rename a file */
  RENAME_SPACE_LOG,

  /** Drop the entry in innodb_table_metadata */
  DROP_LOG,

  /** Rename table in dict cache. */
  RENAME_TABLE_LOG,

  /** Remove a table from dict cache */
  REMOVE_CACHE_LOG,

  /** Biggest log type */
  BIGGEST_LOG = REMOVE_CACHE_LOG
};

/** DDL log record */
class DDL_Record {
 public:
  /** Constructor. */
  DDL_Record();

  /** Destructor. */
  ~DDL_Record();

  /** Get the id of the DDL log record.
  @return id of the record. */
  ulint get_id() const { return (m_id); }

  /** Set the id for the DDL log record.
  @param[in]	id	id of the record. */
  void set_id(ulint id) { m_id = id; }

  /** Get the type of operation to perform
  for the DDL log record.
  @return type of the record. */
  Log_Type get_type() const { return (m_type); }

  /** Set the type for the DDL log record.
  @param[in]	type	set the record type.*/
  void set_type(Log_Type type) { m_type = type; }

  /** Get the thread id for the DDL log record.
  @return thread id of the DDL log record. */
  ulint get_thread_id() const { return (m_thread_id); }

  /** Set the thread id for the DDL log record.
  @param[in]	thread_id	thread id. */
  void set_thread_id(ulint thread_id) { m_thread_id = thread_id; }

  /** Get the space_id present in the DDL log record.
  @return space_id in the DDL log record. */
  space_id_t get_space_id() const { return (m_space_id); }

  /** Set the space id for the DDL log record.
  @param[in]	space	space id. */
  void set_space_id(space_id_t space) { m_space_id = space; }

  /** Get the page no present in the DDL log record.
  @return page_no */
  page_no_t get_page_no() const { return (m_page_no); }

  /** Set the page number for the DDL log record.
  @param[in]	page_no	page number. */
  void set_page_no(page_no_t page_no) { m_page_no = page_no; }

  /** Get the index id present in the DDL log record.
  @return index id. */
  ulint get_index_id() const { return (m_index_id); }

  /** Set the index id for the DDL log record.
  @param[in]	index_id	index id. */
  void set_index_id(ulint index_id) { m_index_id = index_id; }

  /** Get the table id present in the DDL log record.
  @return table id from the record. */
  table_id_t get_table_id() const { return (m_table_id); }

  /** Set the table if for the DDL log record.
  @param[in]	table_id	table id. */
  void set_table_id(table_id_t table_id) { m_table_id = table_id; }

  /** Get the old file path/name present in the DDL log record.
  @return old file path/name. */
  const char *get_old_file_path() const { return (m_old_file_path); }

  /** Set the old file path from the name for the DDL log record.
  @param[in]	name	old file name. */
  void set_old_file_path(const char *name);

  /** Copy the data and set it in old file path
  @param[in]	data	data to be set
  @param[in]	len	length of the data. */
  void set_old_file_path(const byte *data, ulint len);

  /** Get the new file path/name present in the DDL log record.
  @return new file path/name. */
  const char *get_new_file_path() const { return (m_new_file_path); }

  /** Set the new file path/name for the DDL log record.
  @param[in]	name	name to be set. */
  void set_new_file_path(const char *name);

  /** Copy the data and set it in new file path.
  @param[in]	data	data to be set
  @param[in]	len	length of the data. */
  void set_new_file_path(const byte *data, ulint len);

  /** Print the DDL record to specified output stream
  @param[in,out]	out	output stream
  @return output stream */
  std::ostream &print(std::ostream &out) const;

 private:
  /** Log id */
  ulint m_id;

  /** Log type */
  Log_Type m_type;

  /** Thread id */
  ulint m_thread_id;

  /** Tablespace id */
  space_id_t m_space_id;

  /** Index root page */
  page_no_t m_page_no;

  /** Index id */
  ulint m_index_id;

  /** Table id */
  table_id_t m_table_id;

  /** Tablespace file path for DELETE, Old tablespace file path
  for RENAME */
  char *m_old_file_path;

  /** New tablespace file name for RENAME */
  char *m_new_file_path;

  /** memory heap object used for storing file name. */
  mem_heap_t *m_heap;
};

/** Forward declaration */
class THD;
struct que_thr_t;
struct dtuple_t;

/** Array of DDL records */
using DDL_Records = std::vector<DDL_Record *>;

/** Wrapper of mysql.innodb_ddl_log table. Accessing to this table doesn't
require row lock because thread could only access/modify its own ddl records. */
class DDL_Log_Table {
 public:
  /** Constructor. */
  DDL_Log_Table();

  /** Constructor and it initalizes transaction and query thread.
  Once trx is passed in, make sure destructor is called before the
  trx commits.
  @param[in,out]	trx	Transaction */
  explicit DDL_Log_Table(trx_t *trx);

  /** Destructor. */
  ~DDL_Log_Table();

  /** Insert the DDL log record into the innodb_ddl_log table.
  This is thread safe.
  @param[in]	record	Record to be inserted.
  @return DB_SUCCESS or error. */
  dberr_t insert(const DDL_Record &record);

  /** Search for all records of specified thread_id. The records
  are kept in reverse order.
  This is thread safe. Because different threads have different thread
  ids, there should not be any conflict with update.
  @param[in]	thread_id	thread id to search
  @param[out]	records		DDL_Records of the specified thread id
  @return DB_SUCCESS or error. */
  dberr_t search(ulint thread_id, DDL_Records &records);

  /** Do a reverse scan on the table to fetch all the record.
  This is only called during recovery
  @param[out]	records	DDL_Records of the whole table
  @return DB_SUCCESS or error. */
  dberr_t search_all(DDL_Records &records);

  /** Delete the innodb_ddl_log record of specified ID.
  This is thread safe. One thread will only remove its ddl record.
  @param[in]	id	ID of the DDL_Record
  @return DB_SUCCESS or error. */
  dberr_t remove(ulint id);

  /** Delete specified DDL_Records from innodb_ddl_log.
  This is thread safe. Different threads have their own ddl records
  to delete. And this could be called during recovery.
  @param[in]	records		DDL_Record(s) to be deleted
  @return DB_SUCCESS or error. */
  dberr_t remove(const DDL_Records &records);

 private:
  /** Set the query thread using graph. */
  void start_query_thread();

  /** Stop the query thread. */
  void stop_query_thread();

  /** Create tuple for the innodb_ddl_log table.
  It is used for insert operation.
  @param[in]	record	DDL log record. */
  void create_tuple(const DDL_Record &record);

  /** Create tuple for the given index. Used for search by id
  (and following delete)
  @param[in]	id	Thread id/ id of the record
  @param[in]	index	Clustered index or secondary index. */
  void create_tuple(ulint id, const dict_index_t *index);

  /** Convert the innodb_ddl_log index record to DDL_Record.
  @param[in]	is_clustered	true if this is clustered index record,
                                  otherwise the secondary index record
  @param[in]	rec		index record
  @param[in]	offsets		index record offset
  @param[in,out]	record		to store the innodb_ddl_log record. */
  void convert_to_ddl_record(bool is_clustered, rec_t *rec,
                             const ulint *offsets, DDL_Record &record);

  /** Parse the index record and get 'ID'.
  @param[in]	index	index where the record resides
  @param[in]	rec	index rec
  @param[in]	offsets	offsets of the index.
  @return id of the record. */
  ulint parse_id(const dict_index_t *index, rec_t *rec, const ulint *offsets);

  /** Set the given field of the innodb_ddl_log record from given data.
  @param[in]	data	data to be set
  @param[in]	offset	column of the ddl record
  @param[in]	len	length of the data
  @param[in,out]	record	DDL_Record to set */
  void set_field(const byte *data, ulint offset, ulint len, DDL_Record &record);

  /** Fetch the value from given offset.
  @param[in]	data	value to be retrieved from data
  @param[in]	offset	offset of the column
  @return value of the given offset. */
  ulint fetch_value(const byte *data, ulint offset);

  /** Seach specified index by specified ID
  @param[in]	id	ID to search
  @param[in]	index	index to search
  @param[in,out]	records	DDL_Record(s) got by the search
  @return DB_SUCCESS or error */
  dberr_t search_by_id(ulint id, dict_index_t *index, DDL_Records &records);

 private:
  /** Column number of mysql.innodb_ddl_log.id. */
  static constexpr unsigned s_id_col_no = 0;

  /** Column length of mysql.innodb_ddl_log.id. */
  static constexpr unsigned s_id_col_len = 8;

  /** Column number of mysql.innodb_ddl_log.thread_id. */
  static constexpr unsigned s_thread_id_col_no = 1;

  /** Column length of mysql.innodb_ddl_log.thread_id. */
  static constexpr unsigned s_thread_id_col_len = 8;

  /** Column number of mysql.innodb_ddl_log.type. */
  static constexpr unsigned s_type_col_no = 2;

  /** Column length of mysql.innodb_ddl_log.type. */
  static constexpr unsigned s_type_col_len = 4;

  /** Column number of mysql.innodb_ddl_log.space_id. */
  static constexpr unsigned s_space_id_col_no = 3;

  /** Column length of mysql.innodb_ddl_log.space_id. */
  static constexpr unsigned s_space_id_col_len = 4;

  /** Column number of mysql.innodb_ddl_log.page_no. */
  static constexpr unsigned s_page_no_col_no = 4;

  /** Column length of mysql.innodb_ddl_log.page_no. */
  static constexpr unsigned s_page_no_col_len = 4;

  /** Column number of mysql.innodb_ddl_log.index_id. */
  static constexpr unsigned s_index_id_col_no = 5;

  /** Column length of mysql.innodb_ddl_log.index_id. */
  static constexpr unsigned s_index_id_col_len = 8;

  /** Column number of mysql.innodb_ddl_log.table_id. */
  static constexpr unsigned s_table_id_col_no = 6;

  /** Column length of mysql.innodb_ddl_log.table_id. */
  static constexpr unsigned s_table_id_col_len = 8;

  /** Column number of mysql.innodb_ddl_log.old_file_path. */
  static constexpr unsigned s_old_file_path_col_no = 7;

  /** Column number of mysql.innodb_ddl_log.new_file_path. */
  static constexpr unsigned s_new_file_path_col_no = 8;

  /** innodb_ddl_log table. */
  dict_table_t *m_table;

  /** Tuple used for insert, search, delete operation. */
  dtuple_t *m_tuple;

  /** Transaction used for insert, delete operation. */
  trx_t *m_trx;

  /** Dummy query thread. */
  que_thr_t *m_thr;

  /** Heap to store the m_tuple, m_thr and all
  operation on mysql.innodb_ddl_log table. */
  mem_heap_t *m_heap;
};

/** Class to write and replay ddl logs */
class Log_DDL {
 public:
  /** Constructor */
  Log_DDL();

  /** Deconstructor */
  ~Log_DDL() {}

  /** Write DDL log for freeing B-tree
  @param[in,out]	trx		transaction
  @param[in]	index		dict index
  @param[in]	is_drop_table	true if this is drop table
  @return	DB_SUCCESS or error */
  dberr_t write_free_tree_log(trx_t *trx, const dict_index_t *index,
                              bool is_drop_table);

  /** Write DDL log for deleting tablespace file
  @param[in,out]	trx		transaction
  @param[in]	table		dict table
  @param[in]	space_id	tablespace id
  @param[in]	file_path	file path
  @param[in]	is_drop		flag whether dropping tablespace
  @param[in]	dict_locked	true if dict_sys mutex is held
  @return	DB_SUCCESS or error */
  dberr_t write_delete_space_log(trx_t *trx, const dict_table_t *table,
                                 space_id_t space_id, const char *file_path,
                                 bool is_drop, bool dict_locked);

  /** Write a RENAME log record
  @param[in]	space_id	tablespace id
  @param[in]	old_file_path	file path after rename
  @param[in]	new_file_path	file path before rename
  @return DB_SUCCESS or error */
  dberr_t write_rename_space_log(space_id_t space_id, const char *old_file_path,
                                 const char *new_file_path);

  /** Write a DROP log to indicate the entry in innodb_table_metadata
  should be removed for specified table
  @param[in,out]	trx		transaction
  @param[in]	table_id	table ID
  @return DB_SUCCESS or error */
  dberr_t write_drop_log(trx_t *trx, const table_id_t table_id);

  /** Write a RENAME table log record
  @param[in]	table		dict table
  @param[in]	old_name	table name after rename
  @param[in]	new_name	table name before rename
  @return DB_SUCCESS or error */
  dberr_t write_rename_table_log(dict_table_t *table, const char *old_name,
                                 const char *new_name);

  /** Write a REMOVE cache log record
  @param[in,out]	trx		transaction
  @param[in]	table		dict table
  @return DB_SUCCESS or error */
  dberr_t write_remove_cache_log(trx_t *trx, dict_table_t *table);

  /** Replay DDL log record
  @param[in,out]	record	DDL log record
  return DB_SUCCESS or error */
  dberr_t replay(DDL_Record &record);

  /** Replay and clean DDL logs after DDL transaction
  commints or rollbacks.
  @param[in]	thd	mysql thread
  @return	DB_SUCCESS or error */
  dberr_t post_ddl(THD *thd);

  /** Recover in server startup.
  Scan innodb_ddl_log table, and replay all log entries.
  Note: redo log should be applied, and DD transactions
  should be recovered before calling this function.
  @return	DB_SUCCESS or error */
  dberr_t recover();

  /** Is it in ddl recovery in server startup.
  @return	true if it's in ddl recover */
  static bool is_in_recovery() { return (s_in_recovery); }

 private:
  /** Insert a FREE log record
  @param[in,out]	trx		transaction
  @param[in]	index		dict index
  @param[in]	id		log id
  @param[in]	thread_id	thread id
  @return DB_SUCCESS or error */
  dberr_t insert_free_tree_log(trx_t *trx, const dict_index_t *index,
                               uint64_t id, ulint thread_id);

  /** Replay FREE log(free B-tree if exist)
  @param[in]	space_id	tablespace id
  @param[in]	page_no		root page no
  @param[in]	index_id	index id */
  void replay_free_tree_log(space_id_t space_id, page_no_t page_no,
                            ulint index_id);

  /** Insert a DELETE log record
  @param[in,out]	trx		transaction
  @param[in]	id		log id
  @param[in]	thread_id	thread id
  @param[in]	space_id	tablespace id
  @param[in]	file_path	file path
  @param[in]	dict_locked	true if dict_sys mutex is held
  @return DB_SUCCESS or error */
  dberr_t insert_delete_space_log(trx_t *trx, uint64_t id, ulint thread_id,
                                  space_id_t space_id, const char *file_path,
                                  bool dict_locked);

  /** Replay DELETE log(delete file if exist)
  @param[in]	space_id	tablespace id
  @param[in]	file_path	file path */
  void replay_delete_space_log(space_id_t space_id, const char *file_path);

  /** Insert a RENAME log record
  @param[in]	id		log id
  @param[in]	thread_id	thread id
  @param[in]	space_id	tablespace id
  @param[in]	old_file_path	file path after rename
  @param[in]	new_file_path	file path before rename
  @return DB_SUCCESS or error */
  dberr_t insert_rename_space_log(uint64_t id, ulint thread_id,
                                  space_id_t space_id,
                                  const char *old_file_path,
                                  const char *new_file_path);

  /** Relay RENAME log
  @param[in]	space_id	tablespace id
  @param[in]	old_file_path	old file path
  @param[in]	new_file_path	new file path */
  void replay_rename_space_log(space_id_t space_id, const char *old_file_path,
                               const char *new_file_path);

  /** Insert a DROP log record
  @param[in,out]	trx		transaction
  @param[in]	id		log id
  @param[in]	thread_id	thread id
  @param[in]	table_id	table id
  @return DB_SUCCESS or error */
  dberr_t insert_drop_log(trx_t *trx, uint64_t id, ulint thread_id,
                          const table_id_t table_id);

  /** Replay DROP log
  @param[in]	table_id	table id */
  void replay_drop_log(const table_id_t table_id);

  /** Insert a RENAME TABLE log record
  @param[in]	id		log id
  @param[in]	thread_id	thread id
  @param[in]	table_id	table id
  @param[in]	old_name	table name after rename
  @param[in]	new_name	table name before rename
  @return DB_SUCCESS or error */
  dberr_t insert_rename_table_log(uint64_t id, ulint thread_id,
                                  table_id_t table_id, const char *old_name,
                                  const char *new_name);

  /** Relay RENAME TABLE log
  @param[in]	table_id	table id
  @param[in]	old_name	old name
  @param[in]	new_name	new name */
  void replay_rename_table_log(table_id_t table_id, const char *old_name,
                               const char *new_name);

  /** Insert a REMOVE cache log record
  @param[in]	id		log id
  @param[in]	thread_id	thread id
  @param[in]	table_id	table id
  @param[in]	table_name	table name
  @return DB_SUCCESS or error */
  dberr_t insert_remove_cache_log(uint64_t id, ulint thread_id,
                                  table_id_t table_id, const char *table_name);

  /** Relay remove cache log
  @param[in]	table_id	table id
  @param[in]	table_name	table name */
  void replay_remove_cache_log(table_id_t table_id, const char *table_name);

  /** Delete log record by id
  @param[in]	trx		transaction instance
  @param[in]	id		log id
  @param[in]	dict_locked	true if dict_sys mutex is held,
                                  otherwise false
  @return DB_SUCCESS or error */
  dberr_t delete_by_id(trx_t *trx, uint64_t id, bool dict_locked);

  /** Scan, replay and delete log records by thread id
  @param[in]	thread_id	thread id
  @return DB_SUCCESS or error */
  dberr_t replay_by_thread_id(ulint thread_id);

  /** Delete the log records present in the list.
  @param[in]	records		DDL_Records where the IDs are got
  @return DB_SUCCESS or error. */
  dberr_t delete_by_ids(DDL_Records &records);

  /** Scan, replay and delete all log records
  @return DB_SUCCESS or error */
  dberr_t replay_all();

  /** Get next autoinc counter by increasing 1 for innodb_ddl_log
  @return	new next counter */
  inline uint64_t next_id();

  /** Check if we need to skip ddl log for a table.
  @param[in]	table	dict table
  @param[in]	thd	mysql thread
  @return true if should skip, otherwise false */
  inline bool skip(const dict_table_t *table, THD *thd);

 private:
  /** Whether in recover(replay) ddl log in startup. */
  static bool s_in_recovery;
};

/** Object to handle Log_DDL */
extern Log_DDL *log_ddl;

/** Close the DDL log system */
inline void ddl_log_close() { UT_DELETE(log_ddl); }

#ifdef UNIV_DEBUG
struct SYS_VAR;

/** Used by SET GLOBAL innodb_ddl_log_crash_counter_reset_debug = 1; */
extern bool innodb_ddl_log_crash_reset_debug;

/** Reset all crash injection counters. It's used by:
        SET GLOBAL innodb_ddl_log_crash_reset_debug = 1 (0).
@param[in]	thd	thread handle
@param[in]	var	pointer to system variable
@param[in]	var_ptr	where the formal string goes
@param[in]	save	immediate result from check function */
void ddl_log_crash_reset(THD *thd, SYS_VAR *var, void *var_ptr,
                         const void *save);
#endif /* UNIV_DEBUG */

#endif /* log0ddl_h */

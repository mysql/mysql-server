/*****************************************************************************

Copyright (c) 2000, 2017, Oracle and/or its affiliates. All Rights Reserved.

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

#ifndef ha_innodb_h
#define ha_innodb_h

/* The InnoDB handler: the interface between MySQL and InnoDB. */

#include <sys/types.h>

#include "handler.h"
#include "my_compiler.h"
#include "my_dbug.h"
#include "my_inttypes.h"
#include "trx0trx.h"

/** "GEN_CLUST_INDEX" is the name reserved for InnoDB default
system clustered index when there is no primary key. */
extern const char innobase_index_reserve_name[];

/* Structure defines translation table between mysql index and InnoDB
index structures */
struct innodb_idx_translate_t {

	ulint		index_count;	/*!< number of valid index entries
					in the index_mapping array */

	ulint		array_size;	/*!< array size of index_mapping */

	dict_index_t**	index_mapping;	/*!< index pointer array directly
					maps to index in InnoDB from MySQL
					array index */
};

/** InnoDB table share */
typedef struct st_innobase_share {
	const char*	table_name;	/*!< InnoDB table name */
	uint		use_count;	/*!< reference count,
					incremented in get_share()
					and decremented in
					free_share() */
	void*		table_name_hash;
					/*!< hash table chain node */
	innodb_idx_translate_t
			idx_trans_tbl;	/*!< index translation table between
					MySQL and InnoDB */
} INNOBASE_SHARE;

/** Prebuilt structures in an InnoDB table handle used within MySQL */
struct row_prebuilt_t;

namespace dd { namespace cache { class Dictionary_client; } }

/** The class defining a handle to an InnoDB table */
class ha_innobase: public handler
{
public:
	ha_innobase(handlerton* hton, TABLE_SHARE* table_arg);
	~ha_innobase();

	row_type get_real_row_type(const HA_CREATE_INFO *create_info) const;

	const char* table_type() const;

	enum ha_key_alg get_default_index_algorithm() const
	{ return HA_KEY_ALG_BTREE; }

	/** Check if SE supports specific key algorithm. */
	bool is_index_algorithm_supported(enum ha_key_alg key_alg) const
	{
		/* This method is never used for FULLTEXT or SPATIAL keys.
		We rely on handler::ha_table_flags() to check if such keys
		are supported. */
		DBUG_ASSERT(key_alg != HA_KEY_ALG_FULLTEXT
			    && key_alg != HA_KEY_ALG_RTREE);
		return key_alg == HA_KEY_ALG_BTREE;
	}

	Table_flags table_flags() const;

	ulong index_flags(uint idx, uint part, bool all_parts) const;

	uint max_supported_keys() const;

	uint max_supported_key_length() const;

	uint max_supported_key_part_length() const;

	int open(
		const char *name,
		int,
		uint open_flags,
		const dd::Table *table_def);

	/** Opens dictionary table object using table name. For partition, we need to
	try alternative lower/upper case names to support moving data files across
	platforms.
	@param[in]	table_name	name of the table/partition
	@param[in]	norm_name	normalized name of the table/partition
	@param[in]	is_partition	if this is a partition of a table
	@param[in]	ignore_err	error to ignore for loading dictionary object
	@return dictionary table object or NULL if not found */
	static dict_table_t* open_dict_table(
	const char*		table_name,
	const char*		norm_name,
	bool			is_partition,
	dict_err_ignore_t	ignore_err);

	handler* clone(const char *name, MEM_ROOT *mem_root);

	int close(void);

	double scan_time();

	double read_time(uint index, uint ranges, ha_rows rows);

	longlong get_memory_buffer_size() const;

	int write_row(uchar * buf);

	int update_row(const uchar * old_data, uchar * new_data);

	int delete_row(const uchar * buf);

	/** Delete all rows from the table.
	@retval HA_ERR_WRONG_COMMAND if the table is transactional
	@retval 0 on success */
	int delete_all_rows();

	bool was_semi_consistent_read();

	void try_semi_consistent_read(bool yes);

	void unlock_row();

	int index_init(uint index, bool sorted);

	int index_end();

	int index_read(
		uchar*			buf,
		const uchar*		key,
		uint			key_len,
		ha_rkey_function	find_flag);

	int index_read_last(uchar * buf, const uchar * key, uint key_len);

	int index_next(uchar * buf);

	int index_next_same(uchar * buf, const uchar *key, uint keylen);

	int index_prev(uchar * buf);

	int index_first(uchar * buf);

	int index_last(uchar * buf);

	int rnd_init(bool scan);

	int rnd_end();

	int rnd_next(uchar *buf);

	int rnd_pos(uchar * buf, uchar *pos);

	int ft_init();

	void ft_end();

	FT_INFO* ft_init_ext(uint flags, uint inx, String* key);

	FT_INFO* ft_init_ext_with_hints(
		uint			inx,
		String*			key,
		Ft_hints*		hints);

	int ft_read(uchar* buf);

	void position(const uchar *record);

	int info(uint);

	int enable_indexes(uint mode);

	int disable_indexes(uint mode);

	int analyze(THD* thd,HA_CHECK_OPT* check_opt);

	int optimize(THD* thd,HA_CHECK_OPT* check_opt);

	int discard_or_import_tablespace(
		bool		discard,
		dd::Table*	table_def);

	int extra(ha_extra_function operation);

	int reset();

	int external_lock(THD *thd, int lock_type);

	/** MySQL calls this function at the start of each SQL statement
	inside LOCK TABLES. Inside LOCK TABLES the "::external_lock" method
	does not work to mark SQL statement borders. Note also a special case:
	if a temporary table is created inside LOCK TABLES, MySQL has not
	called external_lock() at all on that table.
	MySQL-5.0 also calls this before each statement in an execution of a
	stored procedure. To make the execution more deterministic for
	binlogging, MySQL-5.0 locks all tables involved in a stored procedure
	with full explicit table locks (thd_in_lock_tables(thd) holds in
	store_lock()) before executing the procedure.
	@param[in]	thd		handle to the user thread
	@param[in]	lock_type	lock type
	@return 0 or error code */
	int start_stmt(THD *thd, thr_lock_type lock_type);

	void position(uchar *record);

	virtual int records(ha_rows* num_rows);

	ha_rows records_in_range(
		uint			inx,
		key_range*		min_key,
		key_range*		max_key);

	ha_rows estimate_rows_upper_bound();

	void update_create_info(HA_CREATE_INFO* create_info);

	/** Get storage-engine private data for a data dictionary table.
	@param[in,out]	dd_table	data dictionary table definition
	@param		reset		reset counters
	@retval		true		an error occurred
	@retval		false		success */
	bool get_se_private_data(
		dd::Table*	dd_table,
		bool		reset);

	/** Add hidden columns and indexes to an InnoDB table definition.
	@param[in,out]	dd_table	data dictionary cache object
	@return	error number
	@retval	0 on success */
	int get_extra_columns_and_keys(
		const HA_CREATE_INFO*,
		const List<Create_field>*,
		const KEY*,
		uint,
		dd::Table*	dd_table);

	/** Set Engine specific data to dd::Table object for upgrade.
	@param[in,out]  thd		thread handle
	@param[in]	db_name		database name
	@param[in]	table_name	table name
	@param[in,out]	dd_table	data dictionary cache object
	@return 0 on success, non-zero on failure */
	bool upgrade_table(
		THD*			thd,
		const char*		db_name,
		const char*		table_name,
		dd::Table*		dd_table);

	/** Create an InnoDB table.
	@param[in]	name		table name in filename-safe encoding
	@param[in]	form		table structure
	@param[in]	create_info	more information
	@param[in,out]	table_def	dd::Table describing table to be created.
	Can be adjusted by SE, the changes will be saved into data-dictionary at
	statement commit time.
	@return error number
	@retval 0 on success */
	int create(
		const char*		name,
		TABLE*			form,
		HA_CREATE_INFO*		create_info,
		dd::Table*		table_def);

	/** Drop a table.
	@param[in]	name		table name
	@param[in]	table_def	dd::Table describing table to
	be dropped
	@return	error number
	@retval 0 on success */
	int delete_table(
		const char*		name,
		const dd::Table*	table_def);
protected:
	/** Drop a table.
	@param[in]	name		table name
	@param[in]	table_def	dd::Table describing table to
	be dropped
	@param[in]	sqlcom	type of operation that the DROP is part of
	@return	error number
	@retval 0 on success */
	int delete_table(
		const char*		name,
		const dd::Table*	table_def,
		enum enum_sql_command	sqlcom);
public:
	/** DROP and CREATE an InnoDB table.
	@param[in,out]	table_def	dd::Table describing table to be
	truncated. Can be adjusted by SE, the changes will be saved into
	the data-dictionary at statement commit time.
	@return	error number
	@retval 0 on success */
	int truncate(dd::Table *table_def);

	int rename_table(const char* from, const char* to,
		const dd::Table *from_table,
		dd::Table *to_table);

	int check(THD* thd, HA_CHECK_OPT* check_opt);

	char* get_foreign_key_create_info();

	int get_foreign_key_list(THD *thd, List<FOREIGN_KEY_INFO> *f_key_list);

	int get_parent_foreign_key_list(
		THD*			thd,
		List<FOREIGN_KEY_INFO>*	f_key_list);

	int get_cascade_foreign_key_table_list(
		THD*				thd,
		List<st_handler_tablename>*	fk_table_list);

	bool can_switch_engines();

	uint referenced_by_foreign_key();

	void free_foreign_key_create_info(char* str);

	uint lock_count(void) const;

	THR_LOCK_DATA** store_lock(
		THD*			thd,
		THR_LOCK_DATA**		to,
		thr_lock_type		lock_type);

	void init_table_handle_for_HANDLER();

	virtual void get_auto_increment(
		ulonglong		offset,
		ulonglong		increment,
		ulonglong		nb_desired_values,
		ulonglong*		first_value,
		ulonglong*		nb_reserved_values);

	virtual bool get_error_message(int error, String *buf);

	virtual bool get_foreign_dup_key(char*, uint, char*, uint);

	bool primary_key_is_clustered() const;

	int cmp_ref(const uchar* ref1, const uchar* ref2) const;

	/** On-line ALTER TABLE interface @see handler0alter.cc @{ */

	/** Check if InnoDB supports a particular alter table in-place
	@param altered_table TABLE object for new version of table.
	@param ha_alter_info Structure describing changes to be done
	by ALTER TABLE and holding data used during in-place alter.

	@retval HA_ALTER_INPLACE_NOT_SUPPORTED Not supported
	@retval HA_ALTER_INPLACE_NO_LOCK Supported
	@retval HA_ALTER_INPLACE_SHARED_LOCK_AFTER_PREPARE
		Supported, but requires lock during main phase and
		exclusive lock during prepare phase.
	@retval HA_ALTER_INPLACE_NO_LOCK_AFTER_PREPARE
		Supported, prepare phase requires exclusive lock.  */
	enum_alter_inplace_result check_if_supported_inplace_alter(
		TABLE*			altered_table,
		Alter_inplace_info*	ha_alter_info);

	/** Allows InnoDB to update internal structures with concurrent
	writes blocked (provided that check_if_supported_inplace_alter()
	did not return HA_ALTER_INPLACE_NO_LOCK).
	This will be invoked before inplace_alter_table().

	@param[in]	altered_table	TABLE object for new version of table.
	@param[in,out]	ha_alter_info	Structure describing changes to be done
	by ALTER TABLE and holding data used during in-place alter.
	@param[in]	old_dd_tab	dd::Table object describing old version
	of the table.
	@param[in,out]	new_dd_tab	dd::Table object for the new version of the
	table. Can be adjusted by this call. Changes to the table
	definition will be persisted in the data-dictionary at statement
	commit time.

	@retval true Failure
	@retval false Success
	*/
	bool prepare_inplace_alter_table(
		TABLE*			altered_table,
		Alter_inplace_info*	ha_alter_info,
		const dd::Table*	old_dd_tab,
		dd::Table*		new_dd_tab);

	/** Alter the table structure in-place with operations
	specified using HA_ALTER_FLAGS and Alter_inplace_information.
	The level of concurrency allowed during this operation depends
	on the return value from check_if_supported_inplace_alter().

	@param[in]	altered_table	TABLE object for new version of table.
	@param[in,out]	ha_alter_info	Structure describing changes to be done
	by ALTER TABLE and holding data used during in-place alter.
	@param[in]	 old_dd_tab	dd::Table object describing old version
	of the table.
	@param[in,out]	 new_dd_tab	dd::Table object for the new version of the
	table. Can be adjusted by this call. Changes to the table
	definition will be persisted in the data-dictionary at statement
	commit time.

	@retval true Failure
	@retval false Success
	*/
	bool inplace_alter_table(
		TABLE*			altered_table,
		Alter_inplace_info*	ha_alter_info,
		const dd::Table*	old_dd_tab,
		dd::Table*		new_dd_tab);

	/** Commit or rollback the changes made during
	prepare_inplace_alter_table() and inplace_alter_table() inside
	the storage engine. Note that the allowed level of concurrency
	during this operation will be the same as for
	inplace_alter_table() and thus might be higher than during
	prepare_inplace_alter_table(). (E.g concurrent writes were
	blocked during prepare, but might not be during commit).

	@param[in]	altered_table	TABLE object for new version of table.
	@param[in,out]	ha_alter_info	Structure describing changes to be done
	by ALTER TABLE and holding data used during in-place alter.
	@param commit true => Commit, false => Rollback.
	@param old_dd_tab dd::Table object describing old version
	of the table.
	@param new_dd_tab dd::Table object for the new version of the
	table. Can be adjusted by this call. Changes to the table
	definition will be persisted in the data-dictionary at statement
	commit time.
	@retval true Failure
	@retval false Success
	*/
	bool commit_inplace_alter_table(
		TABLE*			altered_table,
		Alter_inplace_info*	ha_alter_info,
		bool			commit,
		const dd::Table*	old_dd_tab,
		dd::Table*		new_dd_tab);
	/** @} */

	bool check_if_incompatible_data(
		HA_CREATE_INFO*		info,
		uint			table_changes);

private:

	/** @name Multi Range Read interface @{ */

	/** Initialize multi range read @see DsMrr_impl::dsmrr_init
	@param seq
	@param seq_init_param
	@param n_ranges
	@param mode
	@param buf */
	int multi_range_read_init(
		RANGE_SEQ_IF*		seq,
		void*			seq_init_param,
		uint			n_ranges,
		uint			mode,
		HANDLER_BUFFER*		buf);

	/** Process next multi range read @see DsMrr_impl::dsmrr_next
	@param range_info */
	int multi_range_read_next(char** range_info);

	/** Initialize multi range read and get information.
	@see ha_myisam::multi_range_read_info_const
	@see DsMrr_impl::dsmrr_info_const
	@param keyno
	@param seq
	@param seq_init_param
	@param n_ranges
	@param bufsz
	@param flags
	@param cost */
	ha_rows multi_range_read_info_const(
		uint			keyno,
		RANGE_SEQ_IF*		seq,
		void*			seq_init_param,
		uint			n_ranges,
		uint*			bufsz,
		uint*			flags,
		Cost_estimate*		cost);

	/** Initialize multi range read and get information.
	@see DsMrr_impl::dsmrr_info
	@param keyno
	@param n_ranges
	@param keys
	@param bufsz
	@param flags
	@param cost */
	ha_rows multi_range_read_info(
		uint			keyno,
		uint			n_ranges,
		uint			keys,
		uint*			bufsz,
		uint*			flags,
		Cost_estimate*		cost);

	/** Attempt to push down an index condition.
	@param[in] keyno MySQL key number
	@param[in] idx_cond Index condition to be checked
	@return idx_cond if pushed; NULL if not pushed */
	Item* idx_cond_push(uint keyno, Item* idx_cond);
	/* @} */

private:
	void update_thd();

	int change_active_index(uint keynr);

	dberr_t innobase_lock_autoinc();

	dberr_t innobase_set_max_autoinc(ulonglong auto_inc);

	dberr_t innobase_get_autoinc(ulonglong* value);

	void innobase_initialize_autoinc();

	/** Resets a query execution 'template'.
	@see build_template() */
	void reset_template();

	/** Write Row Interface optimized for Intrinsic table. */
	int intrinsic_table_write_row(uchar* record);

	/** Find out if a Record_buffer is wanted by this handler, and what is
	the maximum buffer size the handler wants.

	@param[out] max_rows gets set to the maximum number of records to
		    allocate space for in the buffer
	@retval true   if the handler wants a buffer
	@retval false  if the handler does not want a buffer */
	virtual bool is_record_buffer_wanted(ha_rows* const max_rows) const;

protected:
	void update_thd(THD* thd);

	int general_fetch(uchar* buf, uint direction, uint match_mode);

	virtual dict_index_t* innobase_get_index(uint keynr);

	/** Builds a 'template' to the prebuilt struct.

	The template is used in fast retrieval of just those column
	values MySQL needs in its processing.
	@param whole_row true if access is needed to a whole row,
	false if accessing individual fields is enough */
	void build_template(bool whole_row);

	/** Returns statistics information of the table to the MySQL
	interpreter, in various fields of the handle object.
	@param[in]	flag		what information is requested
	@param[in]	is_analyze	True if called from "::analyze()"
	@return HA_ERR_* error code or 0 */
	virtual int info_low(uint flag, bool is_analyze);

	/**
	MySQL calls this method at the end of each statement. This method
	exists for readability only, called from reset(). The name reset()
	doesn't give any clue that it is called at the end of a statement. */
	int end_stmt();

	/** Rename tablespace file name for truncate
	@param[in]	name	table name
	@return 0 on success, error code on failure */
	int truncate_rename_tablespace(
		const char*	name);

	/** Implementation of prepare_inplace_alter_table()
	@tparam		Table		dd::Table or dd::Partition
	@param[in]	altered_table	TABLE object for new version of table.
	@param[in,out]	ha_alter_info	Structure describing changes to be done
					by ALTER TABLE and holding data used
					during in-place alter.
	@param[in]	old_dd_tab	dd::Table object representing old
					version of the table
	@param[in,out]	new_dd_tab	dd::Table object representing new
					version of the table
	@retval	true Failure
	@retval	false Success */
	template<typename Table>
	bool
	prepare_inplace_alter_table_impl(
		TABLE*			altered_table,
		Alter_inplace_info*	ha_alter_info,
		const Table*		old_dd_tab,
		Table*			new_dd_tab);

	/** Implementation of inplace_alter_table()
	@tparam		Table		dd::Table or dd::Partition
	@param[in]	altered_table	TABLE object for new version of table.
	@param[in,out]	ha_alter_info	Structure describing changes to be done
					by ALTER TABLE and holding data used
					during in-place alter.
	@param[in]	old_dd_tab	dd::Table object representing old
					version of the table
	@param[in,out]	new_dd_tab	dd::Table object representing new
					version of the table
	@retval	true Failure
	@retval	false Success */
	template<typename Table>
	bool
	inplace_alter_table_impl(
		TABLE*			altered_table,
		Alter_inplace_info*	ha_alter_info,
		const Table*		old_dd_tab,
		Table*			new_dd_tab);

	/** Implementation of commit_inplace_alter_table()
	@tparam		Table		dd::Table or dd::Partition
	@param[in]	altered_table	TABLE object for new version of table.
	@param[in,out]	ha_alter_info	Structure describing changes to be done
					by ALTER TABLE and holding data used
					during in-place alter.
	@param[in]	commit		True to commit or false to rollback.
	@param[in]	old_dd_tab      Table object describing old version
					of the table.
	@param[in,out]	new_dd_tab	Table object for the new version of the
					table. Can be adjusted by this call.
					Changes to the table definition
					will be persisted in the data-dictionary
					at statement version of it.
	@retval	true Failure
	@retval	false Success */
	template<typename Table>
	bool
	commit_inplace_alter_table_impl(
		TABLE*			altered_table,
		Alter_inplace_info*	ha_alter_info,
		bool			commit,
		const Table*		old_dd_tab,
		Table*			new_dd_tab);

	/** The multi range read session object */
	DsMrr_impl		m_ds_mrr;

	/** Save CPU time with prebuilt/cached data structures */
	row_prebuilt_t*		m_prebuilt;

	/** Thread handle of the user currently using the handler;
	this is set in external_lock function */
	THD*			m_user_thd;

	/** information for MySQL table locking */
	INNOBASE_SHARE*		m_share;

	/** buffer used in updates */
	uchar*			m_upd_buf;

	/** the size of upd_buf in bytes */
	ulint			m_upd_buf_size;

	/** Flags that specificy the handler instance (table) capability. */
	Table_flags		m_int_table_flags;

	/** this is set to 1 when we are starting a table scan but have
	not yet fetched any row, else false */
	bool			m_start_of_scan;

	/*!< match mode of the latest search: ROW_SEL_EXACT,
	ROW_SEL_EXACT_PREFIX, or undefined */
	uint			m_last_match_mode;

	/** this field is used to remember the original select_lock_type that
	was decided in ha_innodb.cc,":: store_lock()", "::external_lock()",
	etc. */
	ulint			m_stored_select_lock_type;

	/** If mysql has locked with external_lock() */
	bool			m_mysql_has_locked;
};

struct trx_t;

extern const struct _ft_vft ft_vft_result;

/** Structure Returned by ha_innobase::ft_init_ext() */
typedef struct new_ft_info
{
	struct _ft_vft		*please;
	struct _ft_vft_ext	*could_you;
	row_prebuilt_t*		ft_prebuilt;
	fts_result_t*		ft_result;
} NEW_FT_INFO;

/** Allocates an InnoDB transaction for a MySQL handler object for DML.
@param[in]	hton	Innobase handlerton.
@param[in]	thd	MySQL thd (connection) object.
@param[in]	trx	transaction to register. */
void
innobase_register_trx(
	handlerton*	hton,
	THD*		thd,
	trx_t*		trx);

/**
Allocates an InnoDB transaction for a MySQL handler object.
@return InnoDB transaction handle */
trx_t*
innobase_trx_allocate(
	MYSQL_THD	thd);	/*!< in: user thread handle */

/** Match index columns between MySQL and InnoDB.
This function checks whether the index column information
is consistent between KEY info from mysql and that from innodb index.
@param[in]	key_info	Index info from mysql
@param[in]	index_info	Index info from InnoDB
@return true if all column types match. */
bool
innobase_match_index_columns(
	const KEY*		key_info,
	const dict_index_t*	index_info);

/*********************************************************************//**
This function checks each index name for a table against reserved
system default primary index name 'GEN_CLUST_INDEX'. If a name
matches, this function pushes an warning message to the client,
and returns true.
@return true if the index name matches the reserved name */
bool
innobase_index_name_is_reserved(
	THD*			thd,		/*!< in/out: MySQL connection */
	const KEY*		key_info,	/*!< in: Indexes to be
						created */
	ulint			num_of_keys)	/*!< in: Number of indexes to
						be created. */
	MY_ATTRIBUTE((warn_unused_result));

/** Check if the explicit tablespace targeted is file_per_table.
@param[in]	create_info	Metadata for the table to create.
@return true if the table is intended to use a file_per_table tablespace. */
UNIV_INLINE
bool
tablespace_is_file_per_table(
	const HA_CREATE_INFO*	create_info)
{
	return(create_info->tablespace != NULL
	       && (0 == strcmp(create_info->tablespace,
			       dict_sys_t::s_file_per_table_name)));
}

/** Check if table will be explicitly put in an existing shared general
or system tablespace.
@param[in]	create_info	Metadata for the table to create.
@return true if the table will use a shared general or system tablespace. */
UNIV_INLINE
bool
tablespace_is_shared_space(
const HA_CREATE_INFO*	create_info)
{
	return(create_info->tablespace != NULL
		&& create_info->tablespace[0] != '\0'
		&& (0 != strcmp(create_info->tablespace,
		dict_sys_t::s_file_per_table_name)));
}

/** Check if table will be explicitly put in a general tablespace.
@param[in]	create_info	Metadata for the table to create.
@return true if the table will use a general tablespace. */
UNIV_INLINE
bool
tablespace_is_general_space(
const HA_CREATE_INFO*	create_info)
{
	return(create_info->tablespace != NULL
		&& create_info->tablespace[0] != '\0'
		&& (0 != strcmp(create_info->tablespace,
				dict_sys_t::s_file_per_table_name))
		&& (0 != strcmp(create_info->tablespace,
				dict_sys_t::s_temp_space_name))
		&& (0 != strcmp(create_info->tablespace,
				dict_sys_t::s_sys_space_name)));
}

/** Parse hint for table and its indexes, and update the information
in dictionary.
@param[in]	thd		Connection thread
@param[in,out]	table		Target table
@param[in]	table_share	Table definition */
void
innobase_parse_hint_from_comment(
	THD*			thd,
	dict_table_t*		table,
	const TABLE_SHARE*	table_share);

/** Obtain the InnoDB transaction of a MySQL thread.
@param[in,out]	thd	MySQL thread handler.
@return	reference to transaction pointer */
trx_t*& thd_to_trx(THD* thd);

/** Class for handling create table information. */
class create_table_info_t
{
public:
	/** Constructor.
	Used in two ways:
	- all but file_per_table is used, when creating the table.
	- all but name/path is used, when validating options and using flags. */
	create_table_info_t(
		THD*		thd,
		TABLE*		form,
		HA_CREATE_INFO*	create_info,
		char*		table_name,
		char*		remote_path,
		char*		tablespace,
		bool		file_per_table,
		bool		skip_strict,
		ulint		old_flags,
		ulint		old_flags2)
	:m_thd(thd),
	m_trx(thd_to_trx(thd)),
	m_form(form),
	m_create_info(create_info),
	m_table_name(table_name),
	m_remote_path(remote_path),
	m_tablespace(tablespace),
	m_innodb_file_per_table(file_per_table),
	m_flags(old_flags),
	m_flags2(old_flags2),
	m_skip_strict(skip_strict)
	{}

	/** Initialize the object. */
	int initialize();

	/** Set m_tablespace_type. */
	void set_tablespace_type(bool table_being_altered_is_file_per_table);

	/** Create the internal innodb table.
	@param[in]	dd_table	dd::Table or nullptr for intrinsic table
	@return 0 or error number */
	int create_table(const dd::Table*	dd_table);

	/** Update the internal data dictionary. */
	int create_table_update_dict();

	/** Update the global data dictionary.
	@param[in]	dd_table	table object
	@return	0		On success
	@retval	error number	On failure*/
	template<typename Table>
	int create_table_update_global_dd(Table* dd_table);

	/** Validates the create options. Checks that the options
	KEY_BLOCK_SIZE, ROW_FORMAT, DATA DIRECTORY, TEMPORARY & TABLESPACE
	are compatible with each other and other settings.
	These CREATE OPTIONS are not validated here unless innodb_strict_mode
	is on. With strict mode, this function will report each problem it
	finds using a custom message with error code
	ER_ILLEGAL_HA_CREATE_OPTION, not its built-in message.
	@return NULL if valid, string name of bad option if not. */
	const char* create_options_are_invalid();

	/** Validate DATA DIRECTORY option. */
	bool create_option_data_directory_is_valid();
	/** Validate TABLESPACE option. */
	bool create_option_tablespace_is_valid();

	/** Validate COMPRESSION option. */
	bool create_option_compression_is_valid();

	/** Prepare to create a table. */
	int prepare_create_table(const char* name);

	/** Determines InnoDB table flags.
	If strict_mode=OFF, this will adjust the flags to what should be assumed.
	@retval true if successful, false if error */
	bool innobase_table_flags();

	/** Set flags and append '/' to remote path if necessary. */
	void set_remote_path_flags();

	/** Get table flags. */
	ulint flags() const
	{ return(m_flags); }

	/** Get table flags2. */
	ulint flags2() const
	{ return(m_flags2); }

	/** Reset table flags. */
	void flags_reset()
	{ m_flags = 0; }

	/** Reset table flags2. */
	void flags2_reset()
	{ m_flags2 = 0; }

	/** whether to skip strict check. */
	bool skip_strict() const
	{ return(m_skip_strict); }

	/** Return table name. */
	const char* table_name() const
	{ return(m_table_name); }

	THD* thd() const
	{ return(m_thd); }

	inline bool is_intrinsic_temp_table() const
	{
		/* DICT_TF2_INTRINSIC implies DICT_TF2_TEMPORARY */
		ut_ad(!(m_flags2 & DICT_TF2_INTRINSIC)
		      || (m_flags2 & DICT_TF2_TEMPORARY));
		return((m_flags2 & DICT_TF2_INTRINSIC) != 0);
	}

	/** @return true only if table is temporary and not intrinsic */
	inline bool is_temp_table() const
	{
		return(((m_flags2 & DICT_TF2_TEMPORARY) != 0)
		       && ((m_flags & DICT_TF2_INTRINSIC) == 0));
	}

	/** Prevent the created table to be evicted from cache, also all
	auxiliary tables.
	Call this if the DD would be updated after dict_sys mutex is released,
	since all opening table functions require metadata updated to DD.
	@return	True	The eviction of base table is changed,
			so detach should handle it
	@return	False	Already not evicted base table */
	bool prevent_eviction();

	/** Detach the just created table and its auxiliary tables.
	@param[in]	force		True if caller wants this table to be
					not evictable and ignore 'prevented'
	@param[in]	prevented	True if the base table was prevented
					to be evicted by prevent_eviction()
	@param[in]	dict_locked	True if dict_sys mutex is held */
	void detach(bool force, bool prevented, bool dict_locked);

	/** Normalizes a table name string.
	A normalized name consists of the database name catenated to '/' and
	table name. An example: test/mytable. On Windows normalization puts
	both the database name and the table name always to lower case if
	"set_lower_case" is set to true.
	@param[in,out]	norm_name	Buffer to return the normalized name in.
	@param[in]	name		Table name string.
	@param[in]	set_lower_case	True if we want to set name to lower
					case. */
	static void normalize_table_name_low(
		char*		norm_name,
		const char*	name,
		ibool		set_lower_case);

private:
	/** Parses the table name into normal name and either temp path or
	remote path if needed.*/
	int
	parse_table_name(
		const char*	name);

	/** Create the internal innodb table definition.
	@param[in]	dd_table	dd::Table or nullptr for intrinsic table
	@return ER_* level error */
	int create_table_def(const dd::Table*	dd_table);

	/** Initialize the autoinc of this table if necessary, which should
	be called before we flush logs, so autoinc counter can be persisted. */
	void initialize_autoinc();

	/** Connection thread handle. */
	THD*		m_thd;

	/** InnoDB transaction handle. */
	trx_t*		m_trx;

	/** Information on table columns and indexes. */
	const TABLE*	m_form;

	/** Create options. */
	HA_CREATE_INFO*	m_create_info;

	/** Table name */
	char*		m_table_name;
	/** Remote path (DATA DIRECTORY) or zero length-string */
	char*		m_remote_path;
	/** Tablespace name or zero length-string. */
	char*		m_tablespace;

	/** Local copy of srv_file_per_table. */
	bool		m_innodb_file_per_table;

	/** Allow file_per_table for this table either because:
	1) the setting innodb_file_per_table=on,
	2) it was explicitly requested by tablespace=innodb_file_per_table.
	3) the table being altered is currently file_per_table */
	bool		m_allow_file_per_table;

	/** After all considerations, this shows whether we will actually
	create a table and tablespace using file-per-table. */
	bool		m_use_file_per_table;

	/** Using DATA DIRECTORY */
	bool		m_use_data_dir;

	/** Using a Shared General Tablespace */
	bool		m_use_shared_space;

	/** Table flags */
	ulint		m_flags;

	/** Table flags2 */
	ulint		m_flags2;

	/** Skip strict check */
	bool		m_skip_strict;
};

/** Class of basic DDL implementation, for CREATE/DROP/RENAME TABLE */
class innobase_basic_ddl
{
public:
	/** Create an InnoDB table.
	@tparam		Table		dd::Table or dd::Partition
	@param[in,out]	thd		THD object
	@param[in]	name		Table name, format: "db/table_name"
	@param[in]	form		Table format; columns and index
					information
	@param[in]	create_info	Create info(including create statement
					string)
	@param[in,out]	dd_tab		dd::Table describing table to be created
	@param[in]	file_per_table	whether to create a tablespace too
	@param[in]	evictable	whether the caller wants the
					dict_table_t to be kept in memory
	@param[in]	skip_strict	whether to skip strict check for create
					option
	@param[in]	old_flags	old Table flags
	@param[in]	old_flags2	old Table flags2
	@return	error number
	@retval	0 on success */
	template<typename Table>
	static int create_impl(
		THD*		thd,
		const char*	name,
		TABLE*		form,
		HA_CREATE_INFO*	create_info,
		Table*		dd_tab,
		bool		file_per_table,
		bool		evictable,
		bool		skip_strict,
		ulint		old_flags,
		ulint		old_flags2);

	/** Drop an InnoDB table.
	@tparam		Table		dd::Table or dd::Partition
	@param[in,out]	thd		THD object
	@param[in]	name		table name
	@param[in]	dd_tab		dd::Table describing table to be dropped
	@param[in]	sqlcom		type of operation that the DROP
					is part of
	@return	error number
	@retval	0 on success */
	template<typename Table>
	static int delete_impl(
		THD*			thd,
		const char*		name,
		const Table*		dd_tab,
		enum enum_sql_command	sqlcom);

	/** Renames an InnoDB table.
	@tparam		Table		dd::Table or dd::Partition
	@param[in,out]	thd		THD object
	@param[in]	from		old name of the table
	@param[in]	to		new name of the table
	@param[in]	from_table	dd::Table or dd::Partition of the table
					with old name
	@param[in]	to_table	dd::Table or dd::Partition of the table
					with new name
	@return	error number
	@retval	0 on success */
	template<typename Table>
	static int rename_impl(
		THD*			thd,
		const char*		from,
		const char*		to,
		const Table*		from_table,
		const Table*		to_table);
};

/**
Initialize the table FTS stopword list
@return TRUE if success */
ibool
innobase_fts_load_stopword(
/*=======================*/
	dict_table_t*	table,		/*!< in: Table has the FTS */
	trx_t*		trx,		/*!< in: transaction */
	THD*		thd)		/*!< in: current thread */
	MY_ATTRIBUTE((warn_unused_result));

/** Some defines for innobase_fts_check_doc_id_index() return value */
enum fts_doc_id_index_enum {
	FTS_INCORRECT_DOC_ID_INDEX,
	FTS_EXIST_DOC_ID_INDEX,
	FTS_NOT_EXIST_DOC_ID_INDEX
};

/**
Check whether the table has a unique index with FTS_DOC_ID_INDEX_NAME
on the Doc ID column.
@return the status of the FTS_DOC_ID index */
fts_doc_id_index_enum
innobase_fts_check_doc_id_index(
	const dict_table_t*	table,		/*!< in: table definition */
	const TABLE*		altered_table,	/*!< in: MySQL table
						that is being altered */
	ulint*			fts_doc_col_no)	/*!< out: The column number for
						Doc ID */
	MY_ATTRIBUTE((warn_unused_result));

/**
Check whether the table has a unique index with FTS_DOC_ID_INDEX_NAME
on the Doc ID column in MySQL create index definition.
@return FTS_EXIST_DOC_ID_INDEX if there exists the FTS_DOC_ID index,
FTS_INCORRECT_DOC_ID_INDEX if the FTS_DOC_ID index is of wrong format */
fts_doc_id_index_enum
innobase_fts_check_doc_id_index_in_def(
	ulint		n_key,		/*!< in: Number of keys */
	const KEY*	key_info)	/*!< in: Key definitions */
	MY_ATTRIBUTE((warn_unused_result));

/**
Copy table flags from MySQL's TABLE_SHARE into an InnoDB table object.
Those flags are stored in .frm file and end up in the MySQL table object,
but are frequently used inside InnoDB so we keep their copies into the
InnoDB table object. */
void
innobase_copy_frm_flags_from_table_share(
	dict_table_t*		innodb_table,	/*!< in/out: InnoDB table */
	const TABLE_SHARE*	table_share);	/*!< in: table share */

/** Set up base columns for virtual column
@param[in]	table	the InnoDB table
@param[in]	field	MySQL field
@param[in,out]	v_col	virtual column to be set up */
void
innodb_base_col_setup(
	dict_table_t*	table,
	const Field*	field,
	dict_v_col_t*	v_col);

/** Set up base columns for stored column
@param[in]	table	InnoDB table
@param[in]	field	MySQL field
@param[in,out]	s_col	stored column */
void
innodb_base_col_setup_for_stored(
	const dict_table_t*	table,
	const Field*		field,
	dict_s_col_t*		s_col);

/** whether this ia stored column */
#define innobase_is_s_fld(field) ((field)->gcol_info && (field)->stored_in_db)

/** whether this is a computed virtual column */
#define innobase_is_v_fld(field) ((field)->gcol_info && !(field)->stored_in_db)

/** Always normalize table name to lower case on Windows */
#ifdef _WIN32
#define normalize_table_name(norm_name, name)           \
	create_table_info_t::normalize_table_name_low(norm_name, name, TRUE)
#else
#define normalize_table_name(norm_name, name)           \
	create_table_info_t::normalize_table_name_low(norm_name, name, FALSE)
#endif /* _WIN32 */

/** Note that a transaction has been registered with MySQL.
@param[in]	trx	Transaction.
@return true if transaction is registered with MySQL 2PC coordinator */
inline
bool
trx_is_registered_for_2pc(
	const trx_t*	trx)
{
	return(trx->is_registered == 1);
}

/** Converts an InnoDB error code to a MySQL error code.
Also tells to MySQL about a possible transaction rollback inside InnoDB caused
by a lock wait timeout or a deadlock.
@param[in]	error	InnoDB error code.
@param[in]	flags	InnoDB table flags or 0.
@param[in]	thd	MySQL thread or NULL.
@return MySQL error code */
int
convert_error_code_to_mysql(
	dberr_t	error,
	ulint	flags,
	THD*	thd);

/** Converts a search mode flag understood by MySQL to a flag understood
by InnoDB.
@param[in]	find_flag	MySQL search mode flag.
@return	InnoDB search mode flag. */
page_cur_mode_t
convert_search_mode_to_innobase(
	enum ha_rkey_function	find_flag);

extern bool	innobase_stats_on_metadata;

/** Calculate Record Per Key value.
Need to exclude the NULL value if innodb_stats_method is set to "nulls_ignored"
@param[in]	index	InnoDB index.
@param[in]	i	The column we are calculating rec per key.
@param[in]	records	Estimated total records.
@return estimated record per key value */
rec_per_key_t
innodb_rec_per_key(
	const dict_index_t*	index,
	ulint		i,
	ha_rows		records);

/** Build template for the virtual columns and their base columns
@param[in]	table		MySQL TABLE
@param[in]	ib_table	InnoDB dict_table_t
@param[in,out]	s_templ		InnoDB template structure
@param[in]	add_v		new virtual columns added along with
				add index call
@param[in]	locked		true if innobase_share_mutex is held
@param[in]	share_tbl_name	original MySQL table name */
void
innobase_build_v_templ(
	const TABLE*		table,
	const dict_table_t*	ib_table,
	dict_vcol_templ_t*	s_templ,
	const dict_add_v_col_t*	add_v,
	bool			locked,
	const char*		share_tbl_name);

/** callback used by MySQL server layer to initialized
the table virtual columns' template
@param[in]	table		MySQL TABLE
@param[in,out]	ib_table	InnoDB dict_table_t */
void
innobase_build_v_templ_callback(
	const TABLE*	table,
	void*		ib_table);

/** Callback function definition, used by MySQL server layer to initialized
the table virtual columns' template */
typedef void (*my_gcolumn_templatecallback_t)(const TABLE*, void*);

#endif /* ha_innodb_h */

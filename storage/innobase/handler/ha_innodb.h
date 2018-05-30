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

/* The InnoDB handler: the interface between MySQL and InnoDB. */

/** "GEN_CLUST_INDEX" is the name reserved for InnoDB default
system clustered index when there is no primary key. */
extern const char innobase_index_reserve_name[];

/* "innodb_file_per_table" tablespace name  is reserved by InnoDB in order
to explicitly create a file_per_table tablespace for the table. */
extern const char reserved_file_per_table_space_name[];

/* "innodb_system" tablespace name is reserved by InnoDB for the
system tablespace which uses space_id 0 and stores extra types of
system pages like UNDO and doublewrite. */
extern const char reserved_system_space_name[];

/* "innodb_temporary" tablespace name is reserved by InnoDB for the
predefined shared temporary tablespace. */
extern const char reserved_temporary_space_name[];

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

/** The class defining a handle to an InnoDB table */
class ha_innobase: public handler
{
public:
	ha_innobase(handlerton* hton, TABLE_SHARE* table_arg);
	~ha_innobase();

	/** Get the row type from the storage engine.  If this method returns
	ROW_TYPE_NOT_USED, the information in HA_CREATE_INFO should be used. */
	row_type get_row_type() const;

	const char* table_type() const;

	const char* index_type(uint key_number);

	const char** bas_ext() const;

	Table_flags table_flags() const;

	ulong index_flags(uint idx, uint part, bool all_parts) const;

	uint max_supported_keys() const;

	uint max_supported_key_length() const;

	uint max_supported_key_part_length(HA_CREATE_INFO *create_info) const;

	const key_map* keys_to_use_for_scanning();

	int open(const char *name, int mode, uint test_if_locked);

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

	int discard_or_import_tablespace(my_bool discard);

	int extra(ha_extra_function operation);

	int reset();

	int external_lock(THD *thd, int lock_type);

	int start_stmt(THD *thd, thr_lock_type lock_type);

	void position(uchar *record);

#ifdef WL6742
	/* Removing WL6742 as part of Bug #23046302 */
	virtual int records(ha_rows* num_rows);
#endif
	ha_rows records_in_range(
		uint			inx,
		key_range*		min_key,
		key_range*		max_key);

	ha_rows estimate_rows_upper_bound();

	void update_create_info(HA_CREATE_INFO* create_info);

	int create(
		const char*		name,
		TABLE*			form,
		HA_CREATE_INFO*		create_info);

	int truncate();

	int delete_table(const char *name);

	int rename_table(const char* from, const char* to);

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

	uint8 table_cache_type();

	/**
	Ask handler about permission to cache table during query registration
	*/
	my_bool register_query_cache_table(
		THD*			thd,
		char*			table_key,
		size_t			key_length,
		qc_engine_callback*	call_back,
		ulonglong*		engine_data);

	bool primary_key_is_clustered() const;

	int cmp_ref(const uchar* ref1, const uchar* ref2);

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

	@param altered_table TABLE object for new version of table.
	@param ha_alter_info Structure describing changes to be done
	by ALTER TABLE and holding data used during in-place alter.

	@retval true Failure
	@retval false Success
	*/
	bool prepare_inplace_alter_table(
		TABLE*			altered_table,
		Alter_inplace_info*	ha_alter_info);

	/** Alter the table structure in-place with operations
	specified using HA_ALTER_FLAGS and Alter_inplace_information.
	The level of concurrency allowed during this operation depends
	on the return value from check_if_supported_inplace_alter().

	@param altered_table TABLE object for new version of table.
	@param ha_alter_info Structure describing changes to be done
	by ALTER TABLE and holding data used during in-place alter.

	@retval true Failure
	@retval false Success
	*/
	bool inplace_alter_table(
		TABLE*			altered_table,
		Alter_inplace_info*	ha_alter_info);

	/** Commit or rollback the changes made during
	prepare_inplace_alter_table() and inplace_alter_table() inside
	the storage engine. Note that the allowed level of concurrency
	during this operation will be the same as for
	inplace_alter_table() and thus might be higher than during
	prepare_inplace_alter_table(). (E.g concurrent writes were
	blocked during prepare, but might not be during commit).
	@param altered_table TABLE object for new version of table.
	@param ha_alter_info Structure describing changes to be done
	by ALTER TABLE and holding data used during in-place alter.
	@param commit true => Commit, false => Rollback.
	@retval true Failure
	@retval false Success
	*/
	bool commit_inplace_alter_table(
		TABLE*			altered_table,
		Alter_inplace_info*	ha_alter_info,
		bool			commit);
	/** @} */

	bool check_if_incompatible_data(
		HA_CREATE_INFO*		info,
		uint			table_changes);

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
	@param seq
	@param seq_init_param
	@param n_ranges
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

	ulonglong innobase_peek_autoinc();

	dberr_t innobase_set_max_autoinc(ulonglong auto_inc);

	dberr_t innobase_get_autoinc(ulonglong* value);

	void innobase_initialize_autoinc();

	/** Resets a query execution 'template'.
	@see build_template() */
	void reset_template();

	/** Write Row Interface optimized for Intrinsic table. */
	int intrinsic_table_write_row(uchar* record);

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

	virtual int info_low(uint, bool);

	/**
	MySQL calls this method at the end of each statement. This method
	exists for readability only, called from reset(). The name reset()
	doesn't give any clue that it is called at the end of a statement. */
	int end_stmt();


	/** The multi range read session object */
	DsMrr_impl		m_ds_mrr;

	/** Save CPU time with prebuilt/cached data structures */
	row_prebuilt_t*		m_prebuilt;

	/** prebuilt pointer for the right prebuilt. For native
	partitioning, points to the current partition prebuilt. */
	row_prebuilt_t**	m_prebuilt_ptr;

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

	/** Index into the server's primkary keye meta-data table->key_info{} */
	uint			m_primary_key;

	/** this is set to 1 when we are starting a table scan but have
	not yet fetched any row, else false */
	bool			m_start_of_scan;

	/*!< match mode of the latest search: ROW_SEL_EXACT,
	ROW_SEL_EXACT_PREFIX, or undefined */
	uint			m_last_match_mode;

	/** number of write_row() calls */
	uint			m_num_write_row;

        /** If mysql has locked with external_lock() */
        bool                    m_mysql_has_locked;
};


/* Some accessor functions which the InnoDB plugin needs, but which
can not be added to mysql/plugin.h as part of the public interface;
the definitions are bracketed with #ifdef INNODB_COMPATIBILITY_HOOKS */

#ifndef INNODB_COMPATIBILITY_HOOKS
#error InnoDB needs MySQL to be built with #define INNODB_COMPATIBILITY_HOOKS
#endif

LEX_CSTRING thd_query_unsafe(MYSQL_THD thd);
size_t thd_query_safe(MYSQL_THD thd, char *buf, size_t buflen);

extern "C" {

CHARSET_INFO *thd_charset(MYSQL_THD thd);

/** Check if a user thread is a replication slave thread
@param thd user thread
@retval 0 the user thread is not a replication slave thread
@retval 1 the user thread is a replication slave thread */
int thd_slave_thread(const MYSQL_THD thd);

/** Check if a user thread is running a non-transactional update
@param thd user thread
@retval 0 the user thread is not running a non-transactional update
@retval 1 the user thread is running a non-transactional update */
int thd_non_transactional_update(const MYSQL_THD thd);

/** Get the user thread's binary logging format
@param thd user thread
@return Value to be used as index into the binlog_format_names array */
int thd_binlog_format(const MYSQL_THD thd);

/** Check if binary logging is filtered for thread's current db.
@param thd Thread handle
@retval 1 the query is not filtered, 0 otherwise. */
bool thd_binlog_filter_ok(const MYSQL_THD thd);

/** Check if the query may generate row changes which may end up in the binary.
@param thd Thread handle
@retval 1 the query may generate row changes, 0 otherwise.
*/
bool thd_sqlcom_can_generate_row_events(const MYSQL_THD thd);

/** Gets information on the durability property requested by a thread.
@param thd Thread handle
@return a durability property. */
durability_properties thd_get_durability_property(const MYSQL_THD thd);

/** Get the auto_increment_offset auto_increment_increment.
@param thd Thread object
@param off auto_increment_offset
@param inc auto_increment_increment */
void thd_get_autoinc(const MYSQL_THD thd, ulong* off, ulong* inc);

/** Is strict sql_mode set.
@param thd Thread object
@return True if sql_mode has strict mode (all or trans), false otherwise. */
bool thd_is_strict_mode(const MYSQL_THD thd);

/** Get the partition_info working copy.
@param	thd	Thread object.
@return	NULL or pointer to partition_info working copy. */
partition_info*
thd_get_work_part_info(
	THD*	thd);
} /* extern "C" */

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

extern const char reserved_file_per_table_space_name[];

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
			       reserved_file_per_table_space_name)));
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
		reserved_file_per_table_space_name)));
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
				reserved_file_per_table_space_name))
		&& (0 != strcmp(create_info->tablespace,
				reserved_temporary_space_name))
		&& (0 != strcmp(create_info->tablespace,
				reserved_system_space_name)));
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
		char*		temp_path,
		char*		remote_path,
		char*		tablespace)
	:m_thd(thd),
	m_form(form),
	m_create_info(create_info),
	m_table_name(table_name),
	m_temp_path(temp_path),
	m_remote_path(remote_path),
	m_tablespace(tablespace),
	m_innodb_file_per_table(srv_file_per_table)
	{}

	/** Initialize the object. */
	int initialize();

	/** Set m_tablespace_type. */
	void set_tablespace_type(bool table_being_altered_is_file_per_table);

	/** Create the internal innodb table. */
	int create_table();

	/** Update the internal data dictionary. */
	int create_table_update_dict();

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

	void allocate_trx();

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

	/** Get trx. */
	trx_t* trx() const
	{ return(m_trx); }

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
		char*           norm_name,
		const char*     name,
		ibool           set_lower_case);

private:
	/** Parses the table name into normal name and either temp path or
	remote path if needed.*/
	int
	parse_table_name(
		const char*	name);

	/** Create the internal innodb table definition. */
	int create_table_def();

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
	/** If this is a table explicitly created by the user with the
	TEMPORARY keyword, then this parameter is the dir path where the
	table should be placed if we create an .ibd file for it
	(no .ibd extension in the path, though).
	Otherwise this is a zero length-string */
	char*		m_temp_path;

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
};

/**
Retrieve the FTS Relevance Ranking result for doc with doc_id
of prebuilt->fts_doc_id
@return the relevance ranking value */
float
innobase_fts_retrieve_ranking(
	FT_INFO*	fts_hdl);	/*!< in: FTS handler */

/**
Find and Retrieve the FTS Relevance Ranking result for doc with doc_id
of prebuilt->fts_doc_id
@return the relevance ranking value */
float
innobase_fts_find_ranking(
	FT_INFO*	fts_hdl,	/*!< in: FTS handler */
	uchar*		record,		/*!< in: Unused */
	uint		len);		/*!< in: Unused */

/**
Free the memory for the FTS handler */
void
innobase_fts_close_ranking(
	FT_INFO*	fts_hdl);	/*!< in: FTS handler */

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
@return version of the extended FTS API */
uint
innobase_fts_get_version();

/**
@return Which part of the extended FTS API is supported */
ulonglong
innobase_fts_flags();

/**
Find and Retrieve the FTS doc_id for the current result row
@return the document ID */
ulonglong
innobase_fts_retrieve_docid(
	FT_INFO_EXT*	fts_hdl);	/*!< in: FTS handler */

/**
Find and retrieve the size of the current result
@return number of matching rows */
ulonglong
innobase_fts_count_matches(
	FT_INFO_EXT*	fts_hdl);	/*!< in: FTS handler */

/**
Copy table flags from MySQL's HA_CREATE_INFO into an InnoDB table object.
Those flags are stored in .frm file and end up in the MySQL table object,
but are frequently used inside InnoDB so we keep their copies into the
InnoDB table object. */
void
innobase_copy_frm_flags_from_create_info(
	dict_table_t*		innodb_table,	/*!< in/out: InnoDB table */
	const HA_CREATE_INFO*	create_info);	/*!< in: create info */

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

/** Release temporary latches.
Call this function when mysqld passes control to the client. That is to
avoid deadlocks on the adaptive hash S-latch possibly held by thd. For more
documentation, see handler.cc.
@param[in]	hton	Handlerton.
@param[in]	thd	MySQL thread.
@return 0 */
int
innobase_release_temporary_latches(
	handlerton*	hton,
	THD*		thd);

/** Always normalize table name to lower case on Windows */
#ifdef _WIN32
#define normalize_table_name(norm_name, name)           \
	create_table_info_t::normalize_table_name_low(norm_name, name, TRUE)
#else
#define normalize_table_name(norm_name, name)           \
	create_table_info_t::normalize_table_name_low(norm_name, name, FALSE)
#endif /* _WIN32 */

/** Obtain the InnoDB transaction of a MySQL thread.
@param[in,out]	thd	MySQL thread handler.
@return reference to transaction pointer */
trx_t*& thd_to_trx(THD*	thd);

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

/** Commits a transaction in an InnoDB database.
@param[in]	trx	Transaction handle. */
void
innobase_commit_low(
	trx_t*	trx);

extern my_bool	innobase_stats_on_metadata;

/** Calculate Record Per Key value.
Need to exclude the NULL value if innodb_stats_method is set to "nulls_ignored"
@param[in]	index	InnoDB index.
@param[in]	i	The column we are calculating rec per key.
@param[in]	records	Estimated total records.
@return estimated record per key value */
rec_per_key_t
innodb_rec_per_key(
	dict_index_t*	index,
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


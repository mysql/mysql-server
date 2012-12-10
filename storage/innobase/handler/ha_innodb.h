/*****************************************************************************

Copyright (c) 2000, 2012, Oracle and/or its affiliates. All Rights Reserved.

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

/*
  This file is based on ha_berkeley.h of MySQL distribution

  This file defines the Innodb handler: the interface between MySQL and
  Innodb
*/

#include "dict0stats.h"

/* Structure defines translation table between mysql index and innodb
index structures */
struct innodb_idx_translate_t {
	ulint		index_count;	/*!< number of valid index entries
					in the index_mapping array */
	ulint		array_size;	/*!< array size of index_mapping */
	dict_index_t**	index_mapping;	/*!< index pointer array directly
					maps to index in Innodb from MySQL
					array index */
};


/** InnoDB table share */
typedef struct st_innobase_share {
	THR_LOCK		lock;		/*!< MySQL lock protecting
						this structure */
	const char*		table_name;	/*!< InnoDB table name */
	uint			use_count;	/*!< reference count,
						incremented in get_share()
						and decremented in
						free_share() */
	void*			table_name_hash;/*!< hash table chain node */
	innodb_idx_translate_t	idx_trans_tbl;	/*!< index translation
						table between MySQL and
						Innodb */
} INNOBASE_SHARE;


/** Prebuilt structures in an InnoDB table handle used within MySQL */
struct row_prebuilt_t;

/** The class defining a handle to an Innodb table */
class ha_innobase: public handler
{
	row_prebuilt_t*	prebuilt;	/*!< prebuilt struct in InnoDB, used
					to save CPU time with prebuilt data
					structures*/
	THD*		user_thd;	/*!< the thread handle of the user
					currently using the handle; this is
					set in external_lock function */
	THR_LOCK_DATA	lock;
	INNOBASE_SHARE*	share;		/*!< information for MySQL
					table locking */

	uchar*		upd_buf;	/*!< buffer used in updates */
	ulint		upd_buf_size;	/*!< the size of upd_buf in bytes */
	uchar		srch_key_val1[MAX_KEY_LENGTH + MAX_REF_PARTS*2];
	uchar		srch_key_val2[MAX_KEY_LENGTH + MAX_REF_PARTS*2];
					/*!< buffers used in converting
					search key values from MySQL format
					to InnoDB format. For each column
					2 bytes are used to store length,
					hence MAX_REF_PARTS*2. */
	Table_flags	int_table_flags;
	uint		primary_key;
	ulong		start_of_scan;	/*!< this is set to 1 when we are
					starting a table scan but have not
					yet fetched any row, else 0 */
	uint		last_match_mode;/* match mode of the latest search:
					ROW_SEL_EXACT, ROW_SEL_EXACT_PREFIX,
					or undefined */
	uint		num_write_row;	/*!< number of write_row() calls */

	uint store_key_val_for_row(uint keynr, char* buff, uint buff_len,
                                   const uchar* record);
	inline void update_thd(THD* thd);
	void update_thd();
	int change_active_index(uint keynr);
	int general_fetch(uchar* buf, uint direction, uint match_mode);
	dberr_t innobase_lock_autoinc();
	ulonglong innobase_peek_autoinc();
	dberr_t innobase_set_max_autoinc(ulonglong auto_inc);
	dberr_t innobase_reset_autoinc(ulonglong auto_inc);
	dberr_t innobase_get_autoinc(ulonglong* value);
	void innobase_initialize_autoinc();
	dict_index_t* innobase_get_index(uint keynr);

	/* Init values for the class: */
 public:
	ha_innobase(handlerton *hton, TABLE_SHARE *table_arg);
	~ha_innobase();
	/*
	  Get the row type from the storage engine.  If this method returns
	  ROW_TYPE_NOT_USED, the information in HA_CREATE_INFO should be used.
	*/
	enum row_type get_row_type() const;

	const char* table_type() const;
	const char* index_type(uint key_number);
	const char** bas_ext() const;
	Table_flags table_flags() const;
	ulong index_flags(uint idx, uint part, bool all_parts) const;
	uint max_supported_keys() const;
	uint max_supported_key_length() const;
	uint max_supported_key_part_length() const;
	const key_map* keys_to_use_for_scanning();

	int open(const char *name, int mode, uint test_if_locked);
	handler* clone(const char *name, MEM_ROOT *mem_root);
	int close(void);
	double scan_time();
	double read_time(uint index, uint ranges, ha_rows rows);
	longlong get_memory_buffer_size() const;

	int write_row(uchar * buf);
	int update_row(const uchar * old_data, uchar * new_data);
	int delete_row(const uchar * buf);
	bool was_semi_consistent_read();
	void try_semi_consistent_read(bool yes);
	void unlock_row();

	int index_init(uint index, bool sorted);
	int index_end();
	int index_read(uchar * buf, const uchar * key,
		uint key_len, enum ha_rkey_function find_flag);
	int index_read_idx(uchar * buf, uint index, const uchar * key,
			   uint key_len, enum ha_rkey_function find_flag);
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
	FT_INFO *ft_init_ext(uint flags, uint inx, String* key);
	int ft_read(uchar* buf);

	void position(const uchar *record);
	int info(uint);
	int analyze(THD* thd,HA_CHECK_OPT* check_opt);
	int optimize(THD* thd,HA_CHECK_OPT* check_opt);
	int discard_or_import_tablespace(my_bool discard);
	int extra(enum ha_extra_function operation);
	int reset();
	int external_lock(THD *thd, int lock_type);
	int transactional_table_lock(THD *thd, int lock_type);
	int start_stmt(THD *thd, thr_lock_type lock_type);
	void position(uchar *record);
	ha_rows records_in_range(uint inx, key_range *min_key, key_range
								*max_key);
	ha_rows estimate_rows_upper_bound();

	void update_create_info(HA_CREATE_INFO* create_info);
	int parse_table_name(const char*name,
			     HA_CREATE_INFO* create_info,
			     ulint flags,
			     ulint flags2,
			     char* norm_name,
			     char* temp_path,
			     char* remote_path);
	int create(const char *name, register TABLE *form,
					HA_CREATE_INFO *create_info);
	int truncate();
	int delete_table(const char *name);
	int rename_table(const char* from, const char* to);
	int check(THD* thd, HA_CHECK_OPT* check_opt);
	char* update_table_comment(const char* comment);
	char* get_foreign_key_create_info();
	int get_foreign_key_list(THD *thd, List<FOREIGN_KEY_INFO> *f_key_list);
	int get_parent_foreign_key_list(THD *thd,
					List<FOREIGN_KEY_INFO> *f_key_list);
	bool can_switch_engines();
	uint referenced_by_foreign_key();
	void free_foreign_key_create_info(char* str);
	THR_LOCK_DATA **store_lock(THD *thd, THR_LOCK_DATA **to,
					enum thr_lock_type lock_type);
	void init_table_handle_for_HANDLER();
        virtual void get_auto_increment(ulonglong offset, ulonglong increment,
                                        ulonglong nb_desired_values,
                                        ulonglong *first_value,
                                        ulonglong *nb_reserved_values);
	int reset_auto_increment(ulonglong value);

	virtual bool get_error_message(int error, String *buf);
	virtual bool get_foreign_dup_key(char*, uint, char*, uint);
	uint8 table_cache_type();
	/*
	  ask handler about permission to cache table during query registration
	*/
	my_bool register_query_cache_table(THD *thd, char *table_key,
					   uint key_length,
					   qc_engine_callback *call_back,
					   ulonglong *engine_data);
	static const char *get_mysql_bin_log_name();
	static ulonglong get_mysql_bin_log_pos();
	bool primary_key_is_clustered();
	int cmp_ref(const uchar *ref1, const uchar *ref2);
	/** On-line ALTER TABLE interface @see handler0alter.cc @{ */

	/** Check if InnoDB supports a particular alter table in-place
	@param altered_table	TABLE object for new version of table.
	@param ha_alter_info	Structure describing changes to be done
	by ALTER TABLE and holding data used during in-place alter.

	@retval HA_ALTER_INPLACE_NOT_SUPPORTED	Not supported
	@retval HA_ALTER_INPLACE_NO_LOCK	Supported
	@retval HA_ALTER_INPLACE_SHARED_LOCK_AFTER_PREPARE
						Supported, but requires lock
						during main phase and exclusive
						lock during prepare phase.
	@retval HA_ALTER_INPLACE_NO_LOCK_AFTER_PREPARE
						Supported, prepare phase
						requires exclusive lock.
	*/
	enum_alter_inplace_result check_if_supported_inplace_alter(
		TABLE*			altered_table,
		Alter_inplace_info*	ha_alter_info);
	/** Allows InnoDB to update internal structures with concurrent
	writes blocked (provided that check_if_supported_inplace_alter()
	did not return HA_ALTER_INPLACE_NO_LOCK).
	This will be invoked before inplace_alter_table().

	@param altered_table	TABLE object for new version of table.
	@param ha_alter_info	Structure describing changes to be done
	by ALTER TABLE and holding data used during in-place alter.

	@retval true		Failure
	@retval false		Success
	*/
	bool prepare_inplace_alter_table(
		TABLE*			altered_table,
		Alter_inplace_info*	ha_alter_info);

	/** Alter the table structure in-place with operations
	specified using HA_ALTER_FLAGS and Alter_inplace_information.
	The level of concurrency allowed during this operation depends
	on the return value from check_if_supported_inplace_alter().

	@param altered_table	TABLE object for new version of table.
	@param ha_alter_info	Structure describing changes to be done
	by ALTER TABLE and holding data used during in-place alter.

	@retval true		Failure
	@retval false		Success
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
	@param altered_table	TABLE object for new version of table.
	@param ha_alter_info	Structure describing changes to be done
	by ALTER TABLE and holding data used during in-place alter.
	@param commit		true => Commit, false => Rollback.
	@retval true		Failure
	@retval false		Success
	*/
	bool commit_inplace_alter_table(
		TABLE*			altered_table,
		Alter_inplace_info*	ha_alter_info,
		bool			commit);
	/** @} */
	bool check_if_incompatible_data(HA_CREATE_INFO *info,
					uint table_changes);
private:
	/** Builds a 'template' to the prebuilt struct.

	The template is used in fast retrieval of just those column
	values MySQL needs in its processing.
	@param whole_row true if access is needed to a whole row,
	false if accessing individual fields is enough */
	void build_template(bool whole_row);
	/** Resets a query execution 'template'.
	@see build_template() */
	inline void reset_template();

	int info_low(uint, bool);

public:
	/** @name Multi Range Read interface @{ */
	/** Initialize multi range read @see DsMrr_impl::dsmrr_init
	* @param seq
	* @param seq_init_param
	* @param n_ranges
	* @param mode
	* @param buf
	*/
	int multi_range_read_init(RANGE_SEQ_IF* seq,
				  void* seq_init_param,
				  uint n_ranges, uint mode,
				  HANDLER_BUFFER* buf);
	/** Process next multi range read @see DsMrr_impl::dsmrr_next
	* @param range_info
	*/
	int multi_range_read_next(char** range_info);
	/** Initialize multi range read and get information.
	* @see ha_myisam::multi_range_read_info_const
	* @see DsMrr_impl::dsmrr_info_const
	* @param keyno
	* @param seq
	* @param seq_init_param
	* @param n_ranges
	* @param bufsz
	* @param flags
	* @param cost
	*/
	ha_rows multi_range_read_info_const(uint keyno, RANGE_SEQ_IF* seq,
					   void* seq_init_param,
					   uint n_ranges, uint* bufsz,
					   uint* flags, Cost_estimate* cost);
	/** Initialize multi range read and get information.
	* @see DsMrr_impl::dsmrr_info
	* @param keyno
	* @param seq
	* @param seq_init_param
	* @param n_ranges
	* @param bufsz
	* @param flags
	* @param cost
	*/
	ha_rows multi_range_read_info(uint keyno, uint n_ranges, uint keys,
				      uint* bufsz, uint* flags,
				      Cost_estimate* cost);

	/** Attempt to push down an index condition.
	* @param[in] keyno	MySQL key number
	* @param[in] idx_cond	Index condition to be checked
	* @return idx_cond if pushed; NULL if not pushed
	*/
	class Item* idx_cond_push(uint keyno, class Item* idx_cond);

private:
	/** The multi range read session object */
	DsMrr_impl ds_mrr;
	/* @} */
};

/* Some accessor functions which the InnoDB plugin needs, but which
can not be added to mysql/plugin.h as part of the public interface;
the definitions are bracketed with #ifdef INNODB_COMPATIBILITY_HOOKS */

#ifndef INNODB_COMPATIBILITY_HOOKS
#error InnoDB needs MySQL to be built with #define INNODB_COMPATIBILITY_HOOKS
#endif

LEX_STRING* thd_query_string(MYSQL_THD thd);

extern "C" {

struct charset_info_st *thd_charset(MYSQL_THD thd);

/**
  Check if a user thread is a replication slave thread
  @param thd  user thread
  @retval 0 the user thread is not a replication slave thread
  @retval 1 the user thread is a replication slave thread
*/
int thd_slave_thread(const MYSQL_THD thd);

/**
  Check if a user thread is running a non-transactional update
  @param thd  user thread
  @retval 0 the user thread is not running a non-transactional update
  @retval 1 the user thread is running a non-transactional update
*/
int thd_non_transactional_update(const MYSQL_THD thd);

/**
  Get the user thread's binary logging format
  @param thd  user thread
  @return Value to be used as index into the binlog_format_names array
*/
int thd_binlog_format(const MYSQL_THD thd);

/**
  Mark transaction to rollback and mark error as fatal to a sub-statement.
  @param  thd   Thread handle
  @param  all   TRUE <=> rollback main transaction.
*/
void thd_mark_transaction_to_rollback(MYSQL_THD thd, bool all);

/**
  Check if binary logging is filtered for thread's current db.
  @param  thd   Thread handle
  @retval 1 the query is not filtered, 0 otherwise.
*/
bool thd_binlog_filter_ok(const MYSQL_THD thd);

/**
  Check if the query may generate row changes which
  may end up in the binary.
  @param  thd   Thread handle
  @return 1 the query may generate row changes, 0 otherwise.
*/
bool thd_sqlcom_can_generate_row_events(const MYSQL_THD thd);

/**
  Gets information on the durability property requested by
  a thread.
  @param  thd   Thread handle
  @return a durability property.
*/
enum durability_properties thd_get_durability_property(const MYSQL_THD thd);

/** Get the auto_increment_offset auto_increment_increment.
@param thd	Thread object
@param off	auto_increment_offset
@param inc	auto_increment_increment */
void thd_get_autoinc(const MYSQL_THD thd, ulong* off, ulong* inc)
__attribute__((nonnull));
} /* extern "C" */

struct trx_t;

extern const struct _ft_vft ft_vft_result;

/* Structure Returned by ha_innobase::ft_init_ext() */
typedef struct new_ft_info
{
	struct _ft_vft		*please;
	struct _ft_vft_ext	*could_you;
	row_prebuilt_t*		ft_prebuilt;
	fts_result_t*		ft_result;
} NEW_FT_INFO;

/*********************************************************************//**
Allocates an InnoDB transaction for a MySQL handler object.
@return	InnoDB transaction handle */
trx_t*
innobase_trx_allocate(
/*==================*/
	MYSQL_THD	thd);	/*!< in: user thread handle */

/*********************************************************************//**
This function checks each index name for a table against reserved
system default primary index name 'GEN_CLUST_INDEX'. If a name
matches, this function pushes an warning message to the client,
and returns true.
@return true if the index name matches the reserved name */
UNIV_INTERN
bool
innobase_index_name_is_reserved(
/*============================*/
	THD*		thd,		/*!< in/out: MySQL connection */
	const KEY*	key_info,	/*!< in: Indexes to be created */
	ulint		num_of_keys)	/*!< in: Number of indexes to
					be created. */
	__attribute__((nonnull, warn_unused_result));

/*****************************************************************//**
Determines InnoDB table flags.
@retval true if successful, false if error */
UNIV_INTERN
bool
innobase_table_flags(
/*=================*/
	const TABLE*		form,		/*!< in: table */
	const HA_CREATE_INFO*	create_info,	/*!< in: information
						on table columns and indexes */
	THD*			thd,		/*!< in: connection */
	bool			use_tablespace,	/*!< in: whether to create
						outside system tablespace */
	ulint*			flags,		/*!< out: DICT_TF flags */
	ulint*			flags2)		/*!< out: DICT_TF2 flags */
	__attribute__((nonnull, warn_unused_result));

/*****************************************************************//**
Validates the create options. We may build on this function
in future. For now, it checks two specifiers:
KEY_BLOCK_SIZE and ROW_FORMAT
If innodb_strict_mode is not set then this function is a no-op
@return	NULL if valid, string if not. */
UNIV_INTERN
const char*
create_options_are_invalid(
/*=======================*/
	THD*		thd,		/*!< in: connection thread. */
	TABLE*		form,		/*!< in: information on table
					columns and indexes */
	HA_CREATE_INFO*	create_info,	/*!< in: create info. */
	bool		use_tablespace)	/*!< in: srv_file_per_table */
	__attribute__((nonnull, warn_unused_result));

/*********************************************************************//**
Retrieve the FTS Relevance Ranking result for doc with doc_id
of prebuilt->fts_doc_id
@return the relevance ranking value */
UNIV_INTERN
float
innobase_fts_retrieve_ranking(
/*==========================*/
	FT_INFO*	fts_hdl);	/*!< in: FTS handler */

/*********************************************************************//**
Find and Retrieve the FTS Relevance Ranking result for doc with doc_id
of prebuilt->fts_doc_id
@return the relevance ranking value */
UNIV_INTERN
float
innobase_fts_find_ranking(
/*======================*/
	FT_INFO*	fts_hdl,	/*!< in: FTS handler */
	uchar*		record,		/*!< in: Unused */
	uint		len);		/*!< in: Unused */
/*********************************************************************//**
Free the memory for the FTS handler */
UNIV_INTERN
void
innobase_fts_close_ranking(
/*=======================*/
	FT_INFO*	fts_hdl)	/*!< in: FTS handler */
	__attribute__((nonnull));
/*****************************************************************//**
Initialize the table FTS stopword list
@return TRUE if success */
UNIV_INTERN
ibool
innobase_fts_load_stopword(
/*=======================*/
	dict_table_t*	table,		/*!< in: Table has the FTS */
	trx_t*		trx,		/*!< in: transaction */
	THD*		thd)		/*!< in: current thread */
	__attribute__((nonnull(1,3), warn_unused_result));

/** Some defines for innobase_fts_check_doc_id_index() return value */
enum fts_doc_id_index_enum {
	FTS_INCORRECT_DOC_ID_INDEX,
	FTS_EXIST_DOC_ID_INDEX,
	FTS_NOT_EXIST_DOC_ID_INDEX
};

/*******************************************************************//**
Check whether the table has a unique index with FTS_DOC_ID_INDEX_NAME
on the Doc ID column.
@return the status of the FTS_DOC_ID index */
UNIV_INTERN
enum fts_doc_id_index_enum
innobase_fts_check_doc_id_index(
/*============================*/
	const dict_table_t*	table,		/*!< in: table definition */
	const TABLE*		altered_table,	/*!< in: MySQL table
						that is being altered */
	ulint*			fts_doc_col_no)	/*!< out: The column number for
						Doc ID */
	__attribute__((warn_unused_result));

/*******************************************************************//**
Check whether the table has a unique index with FTS_DOC_ID_INDEX_NAME
on the Doc ID column in MySQL create index definition.
@return FTS_EXIST_DOC_ID_INDEX if there exists the FTS_DOC_ID index,
FTS_INCORRECT_DOC_ID_INDEX if the FTS_DOC_ID index is of wrong format */
UNIV_INTERN
enum fts_doc_id_index_enum
innobase_fts_check_doc_id_index_in_def(
/*===================================*/
	ulint		n_key,		/*!< in: Number of keys */
	const KEY*	key_info)	/*!< in: Key definitions */
	__attribute__((nonnull, warn_unused_result));

/***********************************************************************
@return version of the extended FTS API */
uint
innobase_fts_get_version();

/***********************************************************************
@return Which part of the extended FTS API is supported */
ulonglong
innobase_fts_flags();

/***********************************************************************
Find and Retrieve the FTS doc_id for the current result row
@return the document ID */
ulonglong
innobase_fts_retrieve_docid(
/*============================*/
	FT_INFO_EXT*	fts_hdl);	/*!< in: FTS handler */

/***********************************************************************
Find and retrieve the size of the current result
@return number of matching rows */
ulonglong
innobase_fts_count_matches(
/*============================*/
	FT_INFO_EXT*	fts_hdl);	/*!< in: FTS handler */

/** "GEN_CLUST_INDEX" is the name reserved for InnoDB default
system clustered index when there is no primary key. */
extern const char innobase_index_reserve_name[];

/*********************************************************************//**
Copy table flags from MySQL's HA_CREATE_INFO into an InnoDB table object.
Those flags are stored in .frm file and end up in the MySQL table object,
but are frequently used inside InnoDB so we keep their copies into the
InnoDB table object. */
UNIV_INTERN
void
innobase_copy_frm_flags_from_create_info(
/*=====================================*/
	dict_table_t*	innodb_table,		/*!< in/out: InnoDB table */
	HA_CREATE_INFO*	create_info);		/*!< in: create info */

/*********************************************************************//**
Copy table flags from MySQL's TABLE_SHARE into an InnoDB table object.
Those flags are stored in .frm file and end up in the MySQL table object,
but are frequently used inside InnoDB so we keep their copies into the
InnoDB table object. */
UNIV_INTERN
void
innobase_copy_frm_flags_from_table_share(
/*=====================================*/
	dict_table_t*	innodb_table,		/*!< in/out: InnoDB table */
	TABLE_SHARE*	table_share);		/*!< in: table share */
/*********************************************************************//**
Check if table is non-compressed temporary table.
@return true if non-compressed temporary table. */
UNIV_INTERN
bool
innobase_table_is_noncompressed_temporary(
/*======================================*/
	HA_CREATE_INFO* create_info,
	TABLE*		form);

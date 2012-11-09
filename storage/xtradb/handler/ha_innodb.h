/*****************************************************************************

Copyright (c) 2000, 2010, MySQL AB & Innobase Oy. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc., 59 Temple
Place, Suite 330, Boston, MA 02111-1307 USA

*****************************************************************************/

/*
  This file is based on ha_berkeley.h of MySQL distribution

  This file defines the Innodb handler: the interface between MySQL and
  Innodb
*/

#ifdef USE_PRAGMA_INTERFACE
#pragma interface			/* gcc class implementation */
#endif

/* Structure defines translation table between mysql index and innodb
index structures */
typedef struct innodb_idx_translate_struct {
	ulint		index_count;	/*!< number of valid index entries
					in the index_mapping array */
	ulint		array_size;	/*!< array size of index_mapping */
	dict_index_t**	index_mapping;	/*!< index pointer array directly
					maps to index in Innodb from MySQL
					array index */
} innodb_idx_translate_t;


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
	dict_table_t*		ib_table;
} INNOBASE_SHARE;


/** InnoDB B-tree index */
struct dict_index_struct;
/** Prebuilt structures in an Innobase table handle used within MySQL */
struct row_prebuilt_struct;

/** InnoDB B-tree index */
typedef struct dict_index_struct dict_index_t;
/** Prebuilt structures in an Innobase table handle used within MySQL */
typedef struct row_prebuilt_struct row_prebuilt_t;

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

	uchar*		upd_buff;	/*!< buffer used in updates */
	uchar*		key_val_buff;	/*!< buffer used in converting
					search key values from MySQL format
					to Innodb format */
	ulong		upd_and_key_val_buff_len;
					/* the length of each of the previous
					two buffers */
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
	ulint innobase_lock_autoinc();
	ulonglong innobase_peek_autoinc();
	ulint innobase_set_max_autoinc(ulonglong auto_inc);
	ulint innobase_reset_autoinc(ulonglong auto_inc);
	ulint innobase_get_autoinc(ulonglong* value);
	ulint innobase_update_autoinc(ulonglong	auto_inc);
	void innobase_initialize_autoinc();
	dict_index_t* innobase_get_index(uint keynr);
	int info_low(uint flag, bool called_from_analyze);

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
	my_bool is_fake_change_enabled(THD *thd);
	bool is_corrupt() const;

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
	int create(const char *name, register TABLE *form,
					HA_CREATE_INFO *create_info);
	int delete_all_rows();
	int delete_table(const char *name);
	int rename_table(const char* from, const char* to);
	int check(THD* thd, HA_CHECK_OPT* check_opt);
	char* update_table_comment(const char* comment);
	char* get_foreign_key_create_info();
	int get_foreign_key_list(THD *thd, List<FOREIGN_KEY_INFO> *f_key_list);
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

	uint8 table_cache_type();
	/*
	  ask handler about permission to cache table during query registration
	*/
	my_bool register_query_cache_table(THD *thd, char *table_key,
					   uint key_length,
					   qc_engine_callback *call_back,
					   ulonglong *engine_data);
	static char *get_mysql_bin_log_name();
	static ulonglong get_mysql_bin_log_pos();
	bool primary_key_is_clustered();
	int cmp_ref(const uchar *ref1, const uchar *ref2);
	/** Fast index creation (smart ALTER TABLE) @see handler0alter.cc @{ */
	int add_index(TABLE *table_arg, KEY *key_info, uint num_of_keys);
	int prepare_drop_index(TABLE *table_arg, uint *key_num,
			       uint num_of_keys);
	int final_drop_index(TABLE *table_arg);
	/** @} */
	bool check_if_incompatible_data(HA_CREATE_INFO *info,
					uint table_changes);
	bool check_if_supported_virtual_columns(void) { return TRUE; }

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
                                  HANDLER_BUFFER *buf);
	/** Process next multi range read @see DsMrr_impl::dsmrr_next
	* @param range_info
	*/
        int multi_range_read_next(range_id_t *range_info);
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
        ha_rows multi_range_read_info_const(uint keyno, RANGE_SEQ_IF *seq,
                                            void *seq_init_param, 
                                            uint n_ranges, uint *bufsz,
                                            uint *flags, COST_VECT *cost);
	/** Initialize multi range read and get information.
	* @see DsMrr_impl::dsmrr_info
	* @param keyno
        * @param n_ranges
        * @param keys
        * @param key_parts
	* @param bufsz
	* @param flags
	* @param cost
	*/
        ha_rows multi_range_read_info(uint keyno, uint n_ranges, uint keys,
                                      uint key_parts, uint *bufsz, 
                                      uint *flags, COST_VECT *cost);
        int multi_range_read_explain_info(uint mrr_mode,
                                          char *str, size_t size);

	/** Attempt to push down an index condition.
	* @param[in] keyno	MySQL key number
	* @param[in] idx_cond	Index condition to be checked
	* @return idx_cond if pushed; NULL if not pushed
	*/
	class Item* idx_cond_push(uint keyno, class Item* idx_cond);

        /* An helper function for index_cond_func_innodb: */
        bool is_thd_killed();

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

extern "C" {
struct charset_info_st *thd_charset(MYSQL_THD thd);
#if MYSQL_VERSION_ID >= 50142
LEX_STRING *thd_query_string(MYSQL_THD thd);
#else
char **thd_query(MYSQL_THD thd);
#endif

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

#if MYSQL_VERSION_ID > 50140
/**
  Check if binary logging is filtered for thread's current db.
  @param  thd   Thread handle
  @retval 1 the query is not filtered, 0 otherwise.
*/
bool thd_binlog_filter_ok(const MYSQL_THD thd);
#endif /* MYSQL_VERSION_ID > 50140 */
}

/** Get the file name and position of the MySQL binlog corresponding to the
 * current commit.
 */
extern void mysql_bin_log_commit_pos(THD *thd, ulonglong *out_pos, const char **out_file);

typedef struct trx_struct trx_t;
/********************************************************************//**
@file handler/ha_innodb.h
Converts an InnoDB error code to a MySQL error code and also tells to MySQL
about a possible transaction rollback inside InnoDB caused by a lock wait
timeout or a deadlock.
@return	MySQL error code */
extern "C"
int
convert_error_code_to_mysql(
/*========================*/
	int		error,	/*!< in: InnoDB error code */
	ulint		flags,	/*!< in: InnoDB table flags, or 0 */
	MYSQL_THD	thd);	/*!< in: user thread handle or NULL */

/*********************************************************************//**
Allocates an InnoDB transaction for a MySQL handler object.
@return	InnoDB transaction handle */
extern "C"
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
extern "C"
bool
innobase_index_name_is_reserved(
/*============================*/
	THD*		thd,		/*!< in/out: MySQL connection */
	const KEY*	key_info,	/*!< in: Indexes to be created */
	ulint		num_of_keys);	/*!< in: Number of indexes to
					be created. */


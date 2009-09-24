/* Copyright (C) 2000-2005 MySQL AB && Innobase Oy

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307	 USA */

/*
  This file is based on ha_berkeley.h of MySQL distribution

  This file defines the Innodb handler: the interface between MySQL and
  Innodb
*/

#ifdef USE_PRAGMA_INTERFACE
#pragma interface			/* gcc class implementation */
#endif

typedef struct st_innobase_share {
  THR_LOCK lock;
  pthread_mutex_t mutex;
  char *table_name;
  uint table_name_length,use_count;
} INNOBASE_SHARE;


struct dict_index_struct;
struct row_prebuilt_struct;

typedef struct dict_index_struct dict_index_t;
typedef struct row_prebuilt_struct row_prebuilt_t;

/* The class defining a handle to an Innodb table */
class ha_innobase: public handler
{
	row_prebuilt_t*	prebuilt;	/* prebuilt struct in InnoDB, used
					to save CPU time with prebuilt data
					structures*/
	THD*		user_thd;	/* the thread handle of the user
					currently using the handle; this is
					set in external_lock function */
	THR_LOCK_DATA	lock;
	INNOBASE_SHARE	*share;

	uchar*		upd_buff;	/* buffer used in updates */
	uchar*		key_val_buff;	/* buffer used in converting
					search key values from MySQL format
					to Innodb format */
	ulong		upd_and_key_val_buff_len;
					/* the length of each of the previous
					two buffers */
	Table_flags	int_table_flags;
	uint		primary_key;
	ulong		start_of_scan;	/* this is set to 1 when we are
					starting a table scan but have not
					yet fetched any row, else 0 */
	uint		last_match_mode;/* match mode of the latest search:
					ROW_SEL_EXACT, ROW_SEL_EXACT_PREFIX,
					or undefined */
	uint		num_write_row;	/* number of write_row() calls */

	uint store_key_val_for_row(uint keynr, char* buff, uint buff_len,
                                   const uchar* record);
	int update_thd(THD* thd);
	int change_active_index(uint keynr);
	int general_fetch(uchar* buf, uint direction, uint match_mode);
	ulong innobase_lock_autoinc();
	ulonglong innobase_peek_autoinc();
	ulong innobase_set_max_autoinc(ulonglong auto_inc);
	ulong innobase_reset_autoinc(ulonglong auto_inc);
	ulong innobase_get_autoinc(ulonglong* value);
	ulong innobase_update_autoinc(ulonglong	auto_inc);
	ulong innobase_initialize_autoinc();
	dict_index_t* innobase_get_index(uint keynr);
 	ulonglong innobase_get_int_col_max_value(const Field* field);

	/* Init values for the class: */
 public:
	ha_innobase(handlerton *hton, TABLE_SHARE *table_arg);
	~ha_innobase() {}
	/*
	  Get the row type from the storage engine.  If this method returns
	  ROW_TYPE_NOT_USED, the information in HA_CREATE_INFO should be used.
	*/
	enum row_type get_row_type() const;

	const char* table_type() const { return("InnoDB");}
	const char *index_type(uint key_number) { return "BTREE"; }
	const char** bas_ext() const;
	Table_flags table_flags() const;
	ulong index_flags(uint idx, uint part, bool all_parts) const
	{
	  return (HA_READ_NEXT |
		  HA_READ_PREV |
		  HA_READ_ORDER |
		  HA_READ_RANGE |
		  HA_KEYREAD_ONLY);
	}
	uint max_supported_keys()	   const { return MAX_KEY; }
				/* An InnoDB page must store >= 2 keys;
				a secondary key record must also contain the
				primary key value:
				max key length is therefore set to slightly
				less than 1 / 4 of page size which is 16 kB;
				but currently MySQL does not work with keys
				whose size is > MAX_KEY_LENGTH */
	uint max_supported_key_length() const { return 3500; }
	uint max_supported_key_part_length() const;
	const key_map *keys_to_use_for_scanning() { return &key_map_full; }

	int open(const char *name, int mode, uint test_if_locked);
	int close(void);
	double scan_time();
	double read_time(uint index, uint ranges, ha_rows rows);

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

	uint8 table_cache_type() { return HA_CACHE_TBL_ASKTRANSACT; }
	/*
	  ask handler about permission to cache table during query registration
	*/
	my_bool register_query_cache_table(THD *thd, char *table_key,
					   uint key_length,
					   qc_engine_callback *call_back,
					   ulonglong *engine_data);
	static char *get_mysql_bin_log_name();
	static ulonglong get_mysql_bin_log_pos();
	bool primary_key_is_clustered() { return true; }
	int cmp_ref(const uchar *ref1, const uchar *ref2);
	bool check_if_incompatible_data(HA_CREATE_INFO *info,
					uint table_changes);
};

/* Some accessor functions which the InnoDB plugin needs, but which
can not be added to mysql/plugin.h as part of the public interface;
the definitions are bracketed with #ifdef INNODB_COMPATIBILITY_HOOKS */

#ifndef INNODB_COMPATIBILITY_HOOKS
#error InnoDB needs MySQL to be built with #define INNODB_COMPATIBILITY_HOOKS
#endif

extern "C" {
struct charset_info_st *thd_charset(MYSQL_THD thd);
char **thd_query(MYSQL_THD thd);

/** Get the file name of the MySQL binlog.
 * @return the name of the binlog file
 */
const char* mysql_bin_log_file_name(void);

/** Get the current position of the MySQL binlog.
 * @return byte offset from the beginning of the binlog
 */
ulonglong mysql_bin_log_file_pos(void);

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
}

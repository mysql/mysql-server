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


struct row_prebuilt_struct;
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
	ulong		int_table_flags;
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
	int innobase_read_and_init_auto_inc(longlong* ret);

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
	ulonglong table_flags() const { return int_table_flags; }
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

extern long innobase_mirrored_log_groups, innobase_log_files_in_group;
extern long long innobase_buffer_pool_size, innobase_log_file_size;
extern long innobase_log_buffer_size;
extern long innobase_additional_mem_pool_size;
extern long innobase_buffer_pool_awe_mem_mb;
extern long innobase_file_io_threads, innobase_lock_wait_timeout;
extern long innobase_force_recovery;
extern long innobase_open_files;
extern char *innobase_data_home_dir, *innobase_data_file_path;
extern char *innobase_log_group_home_dir, *innobase_log_arch_dir;
extern char *innobase_unix_file_flush_method;
extern "C" {
extern ulong srv_max_buf_pool_modified_pct;
extern ulong srv_max_purge_lag;
extern ulong srv_auto_extend_increment;
extern ulong srv_n_spin_wait_rounds;
extern ulong srv_n_free_tickets_to_enter;
extern ulong srv_thread_sleep_delay;
extern ulong srv_thread_concurrency;
extern ulong srv_commit_concurrency;
extern ulong srv_flush_log_at_trx_commit;
}

/*
  don't delete it - it may be re-enabled later
  as an optimization for the most common case InnoDB+binlog
*/
#if 0
int innobase_report_binlog_offset_and_commit(
	THD*	thd,
	void*	trx_handle,
	char*	log_file_name,
	my_off_t end_offset);
int innobase_commit_complete(void* trx_handle);
void innobase_store_binlog_offset_and_flush_log(char *binlog_name,longlong offset);
#endif

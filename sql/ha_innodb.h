/* Copyright (C) 2000 MySQL AB && Innobase Oy

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

/*
  This file is based on ha_berkeley.h of MySQL distribution

  This file defines the Innodb handler: the interface between MySQL and
  Innodb
*/

#ifdef __GNUC__
#pragma interface			/* gcc class implementation */
#endif

typedef struct st_innobase_share {
  THR_LOCK lock;
  pthread_mutex_t mutex;
  char *table_name;
  uint table_name_length,use_count;
} INNOBASE_SHARE;


/* The class defining a handle to an Innodb table */
class ha_innobase: public handler
{
	void*	innobase_prebuilt;	/* (row_prebuilt_t*) prebuilt
					struct in Innodb, used to save
					CPU */
	THD*		user_thd;	/* the thread handle of the user
					currently using the handle; this is
					set in external_lock function */
	ulong           last_query_id;  /* the latest query id where the
					handle was used */
  	THR_LOCK_DATA 	lock;
	INNOBASE_SHARE  *share;

  	gptr 		alloc_ptr;
  	byte*		upd_buff;	/* buffer used in updates */
  	byte*		key_val_buff;	/* buffer used in converting
  					search key values from MySQL format
  					to Innodb format */
	ulong		upd_and_key_val_buff_len;
					/* the length of each of the previous
					two buffers */
  	ulong 		int_table_flags;
  	uint 		primary_key;
	uint		last_dup_key;
	ulong		start_of_scan;	/* this is set to 1 when we are
					starting a table scan but have not
					yet fetched any row, else 0 */
	uint		last_match_mode;/* match mode of the latest search:
					ROW_SEL_EXACT, ROW_SEL_EXACT_PREFIX,
					or undefined */
	longlong	auto_inc_counter_for_this_stat;
	ulong max_supported_row_length(const byte *buf);

	uint store_key_val_for_row(uint keynr, char* buff, uint buff_len,
					       const byte* record);
	int update_thd(THD* thd);
	int change_active_index(uint keynr);
	int general_fetch(byte* buf, uint direction, uint match_mode);
	int innobase_read_and_init_auto_inc(longlong* ret);

	/* Init values for the class: */
 public:
  	ha_innobase(TABLE *table): handler(table),
	  int_table_flags(HA_REC_NOT_IN_SEQ |
			  HA_NULL_IN_KEY | HA_FAST_KEY_READ |
			  HA_CAN_INDEX_BLOBS |
			  HA_CAN_SQL_HANDLER |
			  HA_NOT_EXACT_COUNT |
			  HA_PRIMARY_KEY_IN_READ_INDEX |
			  HA_TABLE_SCAN_ON_INDEX),
	  last_dup_key((uint) -1),
	  start_of_scan(0)
  	{
  	}
  	~ha_innobase() {}

  	const char* table_type() const { return("InnoDB");}
	const char *index_type(uint key_number) { return "BTREE"; }
  	const char** bas_ext() const;
 	ulong table_flags() const { return int_table_flags; }
	ulong index_flags(uint idx, uint part, bool all_parts) const
	{
	  return (HA_READ_NEXT | HA_READ_PREV | HA_READ_ORDER | HA_READ_RANGE |
                  HA_KEYREAD_ONLY);
	}
  	uint max_supported_keys()          const { return MAX_KEY; }
				/* An InnoDB page must store >= 2 keys;
				a secondary key record must also contain the
				primary key value:
				max key length is therefore set to slightly
				less than 1 / 4 of page size which is 16 kB;
				but currently MySQL does not work with keys
				whose size is > MAX_KEY_LENGTH */
  	uint max_supported_key_length() const { return 3500; }
  	uint max_supported_key_part_length() const { return 3500; }
	const key_map *keys_to_use_for_scanning() { return &key_map_full; }
  	bool has_transactions()  { return 1;}

  	int open(const char *name, int mode, uint test_if_locked);
  	int close(void);
  	double scan_time();
	double read_time(uint index, uint ranges, ha_rows rows);

  	int write_row(byte * buf);
  	int update_row(const byte * old_data, byte * new_data);
  	int delete_row(const byte * buf);

  	int index_init(uint index);
  	int index_end();
  	int index_read(byte * buf, const byte * key,
		       uint key_len, enum ha_rkey_function find_flag);
  	int index_read_idx(byte * buf, uint index, const byte * key,
			   uint key_len, enum ha_rkey_function find_flag);
	int index_read_last(byte * buf, const byte * key, uint key_len);
  	int index_next(byte * buf);
  	int index_next_same(byte * buf, const byte *key, uint keylen);
  	int index_prev(byte * buf);
  	int index_first(byte * buf);
  	int index_last(byte * buf);

  	int rnd_init(bool scan);
  	int rnd_end();
  	int rnd_next(byte *buf);
  	int rnd_pos(byte * buf, byte *pos);

  	void position(const byte *record);
  	void info(uint);
        int analyze(THD* thd,HA_CHECK_OPT* check_opt);
        int optimize(THD* thd,HA_CHECK_OPT* check_opt);
	int discard_or_import_tablespace(my_bool discard);
  	int extra(enum ha_extra_function operation);
  	int external_lock(THD *thd, int lock_type);
	int start_stmt(THD *thd);

  	void position(byte *record);
  	ha_rows records_in_range(uint inx, key_range *min_key, key_range *max_key);
	ha_rows estimate_rows_upper_bound();

  	int create(const char *name, register TABLE *form,
					HA_CREATE_INFO *create_info);
  	int delete_table(const char *name);
	int rename_table(const char* from, const char* to);
	int check(THD* thd, HA_CHECK_OPT* check_opt);
        char* update_table_comment(const char* comment);
	char* get_foreign_key_create_info();
  	uint referenced_by_foreign_key();
	void free_foreign_key_create_info(char* str);	
  	THR_LOCK_DATA **store_lock(THD *thd, THR_LOCK_DATA **to,
			     		enum thr_lock_type lock_type);
	void init_table_handle_for_HANDLER(); 
	longlong get_auto_increment();
        uint8 table_cache_type() { return HA_CACHE_TBL_ASKTRANSACT; }

        static char      *get_mysql_bin_log_name();
        static ulonglong get_mysql_bin_log_pos();
};

extern uint innobase_init_flags, innobase_lock_type;
extern uint innobase_flush_log_at_trx_commit;
extern ulong innobase_cache_size;
extern char *innobase_home, *innobase_tmpdir, *innobase_logdir;
extern long innobase_lock_scan_time;
extern long innobase_mirrored_log_groups, innobase_log_files_in_group;
extern long innobase_log_file_size, innobase_log_buffer_size;
extern long innobase_buffer_pool_size, innobase_additional_mem_pool_size;
extern long innobase_buffer_pool_awe_mem_mb;
extern long innobase_file_io_threads, innobase_lock_wait_timeout;
extern long innobase_force_recovery, innobase_thread_concurrency;
extern long innobase_open_files;
extern char *innobase_data_home_dir, *innobase_data_file_path;
extern char *innobase_log_group_home_dir, *innobase_log_arch_dir;
extern char *innobase_unix_file_flush_method;
/* The following variables have to be my_bool for SHOW VARIABLES to work */
extern my_bool innobase_log_archive,
               innobase_use_native_aio, innobase_fast_shutdown,
	       innobase_file_per_table, innobase_locks_unsafe_for_binlog,
               innobase_create_status_file;
extern "C" {
extern ulong srv_max_buf_pool_modified_pct;
extern ulong srv_auto_extend_increment;
}

extern TYPELIB innobase_lock_typelib;

bool innobase_init(void);
bool innobase_end(void);
bool innobase_flush_logs(void);
uint innobase_get_free_space(void);

int innobase_commit(THD *thd, void* trx_handle);
int innobase_report_binlog_offset_and_commit(
        THD*    thd,
	void*	trx_handle,
        char*   log_file_name,
        my_off_t end_offset);
int innobase_commit_complete(
        void*   trx_handle);
int innobase_rollback(THD *thd, void* trx_handle);
int innobase_rollback_to_savepoint(
	THD*	thd,
	char*	savepoint_name,
	my_off_t* binlog_cache_pos);
int innobase_savepoint(
	THD*	thd,
	char*	savepoint_name,
	my_off_t binlog_cache_pos);
int innobase_close_connection(THD *thd);
int innobase_drop_database(char *path);
int innodb_show_status(THD* thd);

my_bool innobase_query_caching_of_table_permitted(THD* thd, char* full_name,
						uint full_name_len);
void innobase_release_temporary_latches(void* innobase_tid);

void innobase_store_binlog_offset_and_flush_log(char *binlog_name,longlong offset);

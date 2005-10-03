/* Copyright (C) 2000-2005 MySQL AB && Innobase Oy

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

#ifdef USE_PRAGMA_INTERFACE
#pragma interface			/* gcc class implementation */
#endif

typedef struct st_innobase_share {
  THR_LOCK lock;
  pthread_mutex_t mutex;
  char *table_name;
  uint table_name_length,use_count;
} INNOBASE_SHARE;


my_bool innobase_query_caching_of_table_permitted(THD* thd, char* full_name,
                                                  uint full_name_len,
                                                  ulonglong *unused);

/* The class defining a handle to an Innodb table */
class ha_innobase: public handler
{
	void*		innobase_prebuilt;/* (row_prebuilt_t*) prebuilt
					struct in InnoDB, used to save
					CPU time with prebuilt data
					structures*/
	THD*		user_thd;	/* the thread handle of the user
					currently using the handle; this is
					set in external_lock function */
	query_id_t      last_query_id;  /* the latest query id where the
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
	uint		num_write_row;	/* number of write_row() calls */
	ulong max_supported_row_length(const byte *buf);

	uint store_key_val_for_row(uint keynr, char* buff, uint buff_len,
					       const byte* record);
	int update_thd(THD* thd);
	int change_active_index(uint keynr);
	int general_fetch(byte* buf, uint direction, uint match_mode);
	int innobase_read_and_init_auto_inc(longlong* ret);

	/* Init values for the class: */
 public:
  	ha_innobase(TABLE *table_arg);
  	~ha_innobase() {}
	/*
	  Get the row type from the storage engine.  If this method returns
	  ROW_TYPE_NOT_USED, the information in HA_CREATE_INFO should be used.
	*/
	enum row_type get_row_type() const;

  	const char* table_type() const { return("InnoDB");}
	const char *index_type(uint key_number) { return "BTREE"; }
  	const char** bas_ext() const;
 	ulong table_flags() const { return int_table_flags; }
	ulong index_flags(uint idx, uint part, bool all_parts) const
	{
	  return (HA_READ_NEXT |
		  HA_READ_PREV |
		  HA_READ_ORDER |
		  HA_READ_RANGE |
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
	void unlock_row();

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
	int transactional_table_lock(THD *thd, int lock_type);
	int start_stmt(THD *thd);

  	void position(byte *record);
  	ha_rows records_in_range(uint inx, key_range *min_key, key_range
								*max_key);
	ha_rows estimate_rows_upper_bound();

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
	ulonglong get_auto_increment();
	int reset_auto_increment(ulonglong value);

	virtual bool get_error_message(int error, String *buf);
	
        uint8 table_cache_type() { return HA_CACHE_TBL_ASKTRANSACT; }
        /*
          ask handler about permission to cache table during query registration
        */
        my_bool register_query_cache_table(THD *thd, char *table_key,
					   uint key_length,
					   qc_engine_callback *call_back,
					   ulonglong *engine_data)
        {
          *call_back= innobase_query_caching_of_table_permitted;
          *engine_data= 0;
          return innobase_query_caching_of_table_permitted(thd, table_key,
                                                           key_length,
                                                           engine_data);
        }
        static char *get_mysql_bin_log_name();
        static ulonglong get_mysql_bin_log_pos();
        bool primary_key_is_clustered() { return true; }
        int cmp_ref(const byte *ref1, const byte *ref2);
};

extern struct show_var_st innodb_status_variables[];
extern uint innobase_init_flags, innobase_lock_type;
extern uint innobase_flush_log_at_trx_commit;
extern ulong innobase_cache_size, innobase_fast_shutdown;
extern ulong innobase_large_page_size;
extern char *innobase_home, *innobase_tmpdir, *innobase_logdir;
extern long innobase_lock_scan_time;
extern long innobase_mirrored_log_groups, innobase_log_files_in_group;
extern long innobase_log_file_size, innobase_log_buffer_size;
extern long innobase_buffer_pool_size, innobase_additional_mem_pool_size;
extern long innobase_buffer_pool_awe_mem_mb;
extern long innobase_file_io_threads, innobase_lock_wait_timeout;
extern long innobase_force_recovery;
extern long innobase_open_files;
extern char *innobase_data_home_dir, *innobase_data_file_path;
extern char *innobase_log_group_home_dir, *innobase_log_arch_dir;
extern char *innobase_unix_file_flush_method;
/* The following variables have to be my_bool for SHOW VARIABLES to work */
extern my_bool innobase_log_archive,
               innobase_use_doublewrite,
               innobase_use_checksums,
               innobase_use_large_pages,
               innobase_use_native_aio,
	       innobase_file_per_table, innobase_locks_unsafe_for_binlog,
               innobase_create_status_file;
extern my_bool innobase_very_fast_shutdown; /* set this to 1 just before
					    calling innobase_end() if you want
					    InnoDB to shut down without
					    flushing the buffer pool: this
					    is equivalent to a 'crash' */
extern "C" {
extern ulong srv_max_buf_pool_modified_pct;
extern ulong srv_max_purge_lag;
extern ulong srv_auto_extend_increment;
extern ulong srv_n_spin_wait_rounds;
extern ulong srv_n_free_tickets_to_enter;
extern ulong srv_thread_sleep_delay;
extern ulong srv_thread_concurrency;
extern ulong srv_commit_concurrency;
}

extern TYPELIB innobase_lock_typelib;

bool innobase_init(void);
bool innobase_end(void);
bool innobase_flush_logs(void);
uint innobase_get_free_space(void);

/*
  don't delete it - it may be re-enabled later
  as an optimization for the most common case InnoDB+binlog
*/
#if 0
int innobase_report_binlog_offset_and_commit(
        THD*    thd,
	void*	trx_handle,
        char*   log_file_name,
        my_off_t end_offset);
int innobase_commit_complete(void* trx_handle);
void innobase_store_binlog_offset_and_flush_log(char *binlog_name,longlong offset);
#endif

int innobase_drop_database(char *path);
bool innodb_show_status(THD* thd);
bool innodb_mutex_show_status(THD* thd);
void innodb_export_status(void);

void innobase_release_temporary_latches(THD *thd);

void innobase_store_binlog_offset_and_flush_log(char *binlog_name,longlong offset);

int innobase_start_trx_and_assign_read_view(THD* thd);

/***********************************************************************
This function is used to prepare X/Open XA distributed transaction   */

int innobase_xa_prepare(
/*====================*/
			/* out: 0 or error number */
	THD*	thd,	/* in: handle to the MySQL thread of the user
			whose XA transaction should be prepared */
	bool	all);	/* in: TRUE - commit transaction
			FALSE - the current SQL statement ended */

/***********************************************************************
This function is used to recover X/Open XA distributed transactions   */

int innobase_xa_recover(
/*====================*/
				/* out: number of prepared transactions 
				stored in xid_list */
	XID*    xid_list, 	/* in/out: prepared transactions */
	uint	len);		/* in: number of slots in xid_list */

/***********************************************************************
This function is used to commit one X/Open XA distributed transaction
which is in the prepared state */

int innobase_commit_by_xid(
/*=======================*/
			/* out: 0 or error number */
	XID*	xid);	/* in : X/Open XA Transaction Identification */

/***********************************************************************
This function is used to rollback one X/Open XA distributed transaction
which is in the prepared state */

int innobase_rollback_by_xid(
			/* out: 0 or error number */
	XID	*xid);	/* in : X/Open XA Transaction Identification */


int innobase_xa_end(THD *thd);


int innobase_repl_report_sent_binlog(THD *thd, char *log_file_name,
                               my_off_t end_offset);

/***********************************************************************
Create a consistent view for a cursor based on current transaction
which is created if the corresponding MySQL thread still lacks one.
This consistent view is then used inside of MySQL when accessing records 
using a cursor. */

void*
innobase_create_cursor_view(void);
/*=============================*/
				/* out: Pointer to cursor view or NULL */

/***********************************************************************
Close the given consistent cursor view of a transaction and restore
global read view to a transaction read view. Transaction is created if the 
corresponding MySQL thread still lacks one. */

void
innobase_close_cursor_view(
/*=======================*/
	void*	curview);	/* in: Consistent read view to be closed */

/***********************************************************************
Set the given consistent cursor view to a transaction which is created 
if the corresponding MySQL thread still lacks one. If the given 
consistent cursor view is NULL global read view of a transaction is
restored to a transaction read view. */

void
innobase_set_cursor_view(
/*=====================*/
	void*	curview);	/* in: Consistent read view to be set */

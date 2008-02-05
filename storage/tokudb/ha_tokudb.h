#ifdef USE_PRAGMA_INTERFACE
#pragma interface			/* gcc class implementation */
#endif

#include <db.h>

typedef struct st_tokudb_share {
  char *table_name;
  uint table_name_length, use_count;
  pthread_mutex_t mutex;
  THR_LOCK lock;

  ulonglong auto_ident;
  ha_rows rows, org_rows;
  ulong *rec_per_key;
  DB *status_block, *file, **key_file;
  u_int32_t *key_type;
  uint status, version;
  uint ref_length;
  bool fixed_length_primary_key, fixed_length_row;

} TOKUDB_SHARE;

class ha_tokudb: public handler
{
  THR_LOCK_DATA lock;      ///< MySQL lock
  TOKUDB_SHARE *share;    ///< Shared lock info

  DBT last_key, current_row;
  void *alloc_ptr;
  uchar *rec_buff;
  uchar *key_buff, *key_buff2, *primary_key_buff;
  DB *file, **key_file;
  DB_TXN *transaction;
  u_int32_t *key_type;
  DBC *cursor;
  ulong int_table_flags;
  ulong alloced_rec_buff_length;
  ulong changed_rows;
  uint primary_key, last_dup_key, hidden_primary_key, version;
  bool key_read, using_ignore;
  bool fix_rec_buff_for_blob(ulong length);
#define TDB_HIDDEN_PRIMARY_KEY_LENGTH 5
  uchar current_ident[TDB_HIDDEN_PRIMARY_KEY_LENGTH];

  ulong max_row_length(const uchar *buf);
  int pack_row(DBT *row, const uchar *record, bool new_row);
  void unpack_row(uchar *record, DBT *row);
  void unpack_key(uchar *record, DBT *key, uint index);
  DBT *create_key(DBT *key, uint keynr, uchar *buff, const uchar *record, 
		  int key_length = MAX_KEY_LENGTH);
  DBT *pack_key(DBT *key, uint keynr, uchar *buff, const uchar *key_ptr, 
		uint key_length);
  int remove_key(DB_TXN *trans, uint keynr, const uchar *record, DBT *prim_key);
  int remove_keys(DB_TXN *trans, const uchar *record, DBT *new_record, 
		  DBT *prim_key, key_map *keys);
  int restore_keys(DB_TXN *trans, key_map *changed_keys, uint primary_key, 
		   const uchar *old_row, DBT *old_key, 
		   const uchar *new_row, DBT *new_key);
  int key_cmp(uint keynr, const uchar *old_row, const uchar *new_row);
  int update_primary_key(DB_TXN *trans, bool primary_key_changed, 
			 const uchar *old_row, DBT *old_key, 
			 const uchar *new_row, DBT *prim_key, 
			 bool local_using_ignore);
  int read_row(int error, uchar *buf, uint keynr, DBT *row, DBT *key, bool);
  DBT *get_pos(DBT *to, uchar *pos);

public:
  ha_tokudb(handlerton *hton, TABLE_SHARE *table_arg);
  ~ha_tokudb() { }
  const char *table_type() const { return "TOKUDB"; }
  const char *index_type(uint inx) { return "BTREE"; }
  const char **bas_ext() const;
  ulonglong table_flags(void) const { return int_table_flags; }
  ulong index_flags(uint inx, uint part, bool all_parts) const;

  uint max_supported_keys() const { return MAX_KEY-1; }
  uint extra_rec_buf_length() const { return TDB_HIDDEN_PRIMARY_KEY_LENGTH; }
  ha_rows estimate_rows_upper_bound();
  uint max_supported_key_length() const { return UINT_MAX32; }
  uint max_supported_key_part_length() const { return UINT_MAX32; }
  const key_map *keys_to_use_for_scanning() { return &key_map_full; }

  double scan_time();

  int open(const char *name, int mode, uint test_if_locked);
  int close(void);
  int create(const char *name, TABLE *form, 
             HA_CREATE_INFO *create_info);
  int delete_table(const char *name);
  int rename_table(const char *from, const char *to);

  int analyze(THD *thd, HA_CHECK_OPT *check_opt);
  int optimize(THD *thd, HA_CHECK_OPT *check_opt);
  int check(THD *thd, HA_CHECK_OPT *check_opt);

  int write_row(uchar *buf);
  int update_row(const uchar *old_data, uchar *new_data);
  int delete_row(const uchar *buf);

  int index_init(uint index, bool sorted);
  int index_end();
  int index_read(uchar *buf, const uchar *key, 
		 uint key_len, enum ha_rkey_function find_flag);
  int index_read_idx(uchar *buf, uint index, const uchar *key, 
		     uint key_len, enum ha_rkey_function find_flag);
  int index_read_last(uchar *buf, const uchar *key, uint key_len);
  int index_next(uchar *buf);
  int index_next_same(uchar *buf, const uchar *key, uint keylen);
  int index_prev(uchar *buf);
  int index_first(uchar *buf);
  int index_last(uchar *buf);

  int rnd_init(bool scan);
  int rnd_end();
  int rnd_next(uchar *buf);
  int rnd_pos(uchar *buf, uchar *pos);

  void position(const uchar *record);
  int info(uint);
  int extra(enum ha_extra_function operation);
  int reset(void);
  int external_lock(THD *thd, int lock_type);
  int start_stmt(THD *thd, thr_lock_type lock_type);

  ha_rows records_in_range(uint inx, key_range *min_key, key_range *max_key);

  THR_LOCK_DATA **store_lock(THD *thd, THR_LOCK_DATA **to, 
                             enum thr_lock_type lock_type);     

  void get_status();
  inline void get_auto_primary_key(uchar *to)
  {
    pthread_mutex_lock(&share->mutex);
    share->auto_ident++;
    int5store(to, share->auto_ident);
    pthread_mutex_unlock(&share->mutex);
  }
  virtual void get_auto_increment(ulonglong offset, ulonglong increment, 
                                  ulonglong nb_desired_values, 
                                  ulonglong *first_value, 
                                  ulonglong *nb_reserved_values);
  void print_error(int error, myf errflag);
  uint8 table_cache_type() { return HA_CACHE_TBL_TRANSACT; }
  bool primary_key_is_clustered() { return true; }
  int cmp_ref(const uchar *ref1, const uchar *ref2);
  bool check_if_incompatible_data(HA_CREATE_INFO *info, uint table_changes);

 private:
  int __close(int mutex_is_locked);
};

#ifdef UNDEF

extern const u_int32_t tdb_DB_TXN_NOSYNC;
extern const u_int32_t tdb_DB_RECOVER;
extern const u_int32_t tdb_DB_PRIVATE;
extern const u_int32_t tdb_DB_DIRECT_DB;
extern const u_int32_t tdb_DB_DIRECT_LOG;
extern bool tokudb_shared_data;
extern u_int32_t tokudb_init_flags, tokudb_env_flags, tokudb_lock_type, 
                 tokudb_lock_types[];
extern ulong tokudb_max_lock, tokudb_log_buffer_size;
extern ulonglong tokudb_cache_size;
extern ulong tokudb_region_size, tokudb_cache_parts;
extern char *tokudb_home, *tokudb_tmpdir, *tokudb_logdir;
extern long tokudb_lock_scan_time;
extern TYPELIB tokudb_lock_typelib;

#endif

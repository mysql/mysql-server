/* Copyright (C) 2000,2004 MySQL AB & MySQL Finland AB & TCX DataKonsult AB

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


/* Definitions for parameters to do with handler-routines */

#ifdef __GNUC__
#pragma interface			/* gcc class implementation */
#endif

#include <ft_global.h>
#include <keycache.h>

#ifndef NO_HASH
#define NO_HASH				/* Not yet implemented */
#endif

#if defined(HAVE_BERKELEY_DB) || defined(HAVE_INNOBASE_DB) || \
    defined(HAVE_NDBCLUSTER_DB)
#define USING_TRANSACTIONS
#endif

// the following is for checking tables

#define HA_ADMIN_ALREADY_DONE	  1
#define HA_ADMIN_OK               0
#define HA_ADMIN_NOT_IMPLEMENTED -1
#define HA_ADMIN_FAILED		 -2
#define HA_ADMIN_CORRUPT         -3
#define HA_ADMIN_INTERNAL_ERROR  -4
#define HA_ADMIN_INVALID         -5
#define HA_ADMIN_REJECT          -6
#define HA_ADMIN_TRY_ALTER       -7

/* Bits in table_flags() to show what database can do */
#define HA_READ_RND_SAME       (1 << 0) /* can switch index during the scan
                                           with ::rnd_same() - not used yet.
                                           see mi_rsame/heap_rsame/myrg_rsame */
#define HA_TABLE_SCAN_ON_INDEX (1 << 2) /* No separate data/index file */
#define HA_REC_NOT_IN_SEQ      (1 << 3) /* ha_info don't return recnumber;
                                           It returns a position to ha_r_rnd */
#define HA_CAN_GEOMETRY        (1 << 4)
#define HA_FAST_KEY_READ       (1 << 5) /* no need for a record cache in filesort */
#define HA_NULL_IN_KEY         (1 << 7) /* One can have keys with NULL */
#define HA_DUPP_POS            (1 << 8) /* ha_position() gives dup row */
#define HA_NO_BLOBS            (1 << 9) /* Doesn't support blobs */
#define HA_CAN_INDEX_BLOBS     (1 << 10)
#define HA_AUTO_PART_KEY       (1 << 11) /* auto-increment in multi-part key */
#define HA_REQUIRE_PRIMARY_KEY (1 << 12) /* .. and can't create a hidden one */
#define HA_NOT_EXACT_COUNT     (1 << 13)
#define HA_CAN_INSERT_DELAYED  (1 << 14) /* only handlers with table-level locks
                                            need no special code to support
                                            INSERT DELAYED */
#define HA_PRIMARY_KEY_IN_READ_INDEX (1 << 15)
#define HA_NOT_DELETE_WITH_CACHE (1 << 18)
#define HA_NO_PREFIX_CHAR_KEYS (1 << 20)
#define HA_CAN_FULLTEXT        (1 << 21)
#define HA_CAN_SQL_HANDLER     (1 << 22)
#define HA_NO_AUTO_INCREMENT   (1 << 23)
#define HA_HAS_CHECKSUM        (1 << 24)
/* Table data are stored in separate files (for lower_case_table_names) */
#define HA_FILE_BASED	       (1 << 26)


/* bits in index_flags(index_number) for what you can do with index */
#define HA_READ_NEXT            1       /* TODO really use this flag */
#define HA_READ_PREV            2       /* supports ::index_prev */
#define HA_READ_ORDER           4       /* index_next/prev follow sort order */
#define HA_READ_RANGE           8       /* can find all records in a range */
#define HA_ONLY_WHOLE_INDEX	16	/* Can't use part key searches */
#define HA_KEYREAD_ONLY         64	/* Support HA_EXTRA_KEYREAD */

/* operations for disable/enable indexes */
#define HA_KEY_SWITCH_NONUNIQ      0
#define HA_KEY_SWITCH_ALL          1
#define HA_KEY_SWITCH_NONUNIQ_SAVE 2
#define HA_KEY_SWITCH_ALL_SAVE     3


/*
  Bits in index_ddl_flags(KEY *wanted_index)
  for what ddl you can do with index
  If none is set, the wanted type of index is not supported
  by the handler at all. See WorkLog 1563.
*/
#define HA_DDL_SUPPORT   1 /* Supported by handler */
#define HA_DDL_WITH_LOCK 2 /* Can create/drop with locked table */
#define HA_DDL_ONLINE    4 /* Can create/drop without lock */

/*
  Parameters for open() (in register form->filestat)
  HA_GET_INFO does an implicit HA_ABORT_IF_LOCKED
*/

#define HA_OPEN_KEYFILE		1
#define HA_OPEN_RNDFILE		2
#define HA_GET_INDEX		4
#define HA_GET_INFO		8	/* do a ha_info() after open */
#define HA_READ_ONLY		16	/* File opened as readonly */
/* Try readonly if can't open with read and write */
#define HA_TRY_READ_ONLY	32
#define HA_WAIT_IF_LOCKED	64	/* Wait if locked on open */
#define HA_ABORT_IF_LOCKED	128	/* skip if locked on open.*/
#define HA_BLOCK_LOCK		256	/* unlock when reading some records */
#define HA_OPEN_TEMPORARY	512

	/* Errors on write which is recoverable  (Key exist) */
#define HA_WRITE_SKIP 121		/* Duplicate key on write */
#define HA_READ_CHECK 123		/* Update with is recoverable */
#define HA_CANT_DO_THAT 131		/* Databasehandler can't do it */

	/* Some key definitions */
#define HA_KEY_NULL_LENGTH	1
#define HA_KEY_BLOB_LENGTH	2

#define HA_LEX_CREATE_TMP_TABLE	1
#define HA_LEX_CREATE_IF_NOT_EXISTS 2
#define HA_OPTION_NO_CHECKSUM	(1L << 17)
#define HA_OPTION_NO_DELAY_KEY_WRITE (1L << 18)
#define HA_MAX_REC_LENGTH	65535

/* Table caching type */
#define HA_CACHE_TBL_NONTRANSACT 0
#define HA_CACHE_TBL_NOCACHE     1
#define HA_CACHE_TBL_ASKTRANSACT 2
#define HA_CACHE_TBL_TRANSACT    4


enum db_type 
{ 
  DB_TYPE_UNKNOWN=0,DB_TYPE_DIAB_ISAM=1,
  DB_TYPE_HASH,DB_TYPE_MISAM,DB_TYPE_PISAM,
  DB_TYPE_RMS_ISAM, DB_TYPE_HEAP, DB_TYPE_ISAM,
  DB_TYPE_MRG_ISAM, DB_TYPE_MYISAM, DB_TYPE_MRG_MYISAM,
  DB_TYPE_BERKELEY_DB, DB_TYPE_INNODB, 
  DB_TYPE_GEMINI, DB_TYPE_NDBCLUSTER,
  DB_TYPE_EXAMPLE_DB, DB_TYPE_ARCHIVE_DB, DB_TYPE_CSV_DB,
	       
  DB_TYPE_DEFAULT // Must be last
};

struct show_table_type_st {
  const char *type;
  SHOW_COMP_OPTION *value;
  const char *comment;
  enum db_type db_type;
};

enum row_type { ROW_TYPE_NOT_USED=-1, ROW_TYPE_DEFAULT, ROW_TYPE_FIXED,
		ROW_TYPE_DYNAMIC, ROW_TYPE_COMPRESSED};

/* struct to hold information about the table that should be created */

/* Bits in used_fields */
#define HA_CREATE_USED_AUTO		1
#define HA_CREATE_USED_RAID		2
#define HA_CREATE_USED_UNION		4
#define HA_CREATE_USED_INSERT_METHOD	8
#define HA_CREATE_USED_MIN_ROWS		16
#define HA_CREATE_USED_MAX_ROWS		32
#define HA_CREATE_USED_AVG_ROW_LENGTH	64
#define HA_CREATE_USED_PACK_KEYS	128
#define HA_CREATE_USED_CHARSET		256
#define HA_CREATE_USED_DEFAULT_CHARSET	512

typedef struct st_thd_trans {
  void *bdb_tid;
  void *innobase_tid;
  bool innodb_active_trans;
  void *ndb_tid;
} THD_TRANS;

enum enum_tx_isolation { ISO_READ_UNCOMMITTED, ISO_READ_COMMITTED,
			 ISO_REPEATABLE_READ, ISO_SERIALIZABLE};

typedef struct st_ha_create_information
{
  CHARSET_INFO *table_charset, *default_table_charset;
  const char *comment,*password;
  const char *data_file_name, *index_file_name;
  const char *alias;
  ulonglong max_rows,min_rows;
  ulonglong auto_increment_value;
  ulong table_options;
  ulong avg_row_length;
  ulong raid_chunksize;
  ulong used_fields;
  SQL_LIST merge_list;
  enum db_type db_type;
  enum row_type row_type;
  uint options;				/* OR of HA_CREATE_ options */
  uint raid_type,raid_chunks;
  uint merge_insert_method;
  bool table_existed;			/* 1 in create if table existed */
} HA_CREATE_INFO;


/* The handler for a table type.  Will be included in the TABLE structure */

struct st_table;
typedef struct st_table TABLE;

typedef struct st_ha_check_opt
{
  ulong sort_buffer_size;
  uint flags;       /* isam layer flags (e.g. for myisamchk) */
  uint sql_flags;   /* sql layer flags - for something myisamchk cannot do */
  KEY_CACHE *key_cache;	/* new key cache when changing key cache */
  void init();
} HA_CHECK_OPT;


class handler :public Sql_alloc
{
 protected:
  struct st_table *table;		/* The table definition */

  virtual int index_init(uint idx) { active_index=idx; return 0; }
  virtual int index_end() { active_index=MAX_KEY; return 0; }
  /*
    rnd_init() can be called two times without rnd_end() in between
    (it only makes sense if scan=1).
    then the second call should prepare for the new table scan (e.g
    if rnd_init allocates the cursor, second call should position it
    to the start of the table, no need to deallocate and allocate it again
  */
  virtual int rnd_init(bool scan) =0;
  virtual int rnd_end() { return 0; }

public:
  byte *ref;				/* Pointer to current row */
  byte *dupp_ref;			/* Pointer to dupp row */
  ulonglong data_file_length;		/* Length off data file */
  ulonglong max_data_file_length;	/* Length off data file */
  ulonglong index_file_length;
  ulonglong max_index_file_length;
  ulonglong delete_length;		/* Free bytes */
  ulonglong auto_increment_value;
  ha_rows records;			/* Records in table */
  ha_rows deleted;			/* Deleted records */
  ulong raid_chunksize;
  ulong mean_rec_length;		/* physical reclength */
  time_t create_time;			/* When table was created */
  time_t check_time;
  time_t update_time;

  /* The following are for read_range() */
  key_range save_end_range, *end_range;
  KEY_PART_INFO *range_key_part;
  int key_compare_result_on_equal;
  bool eq_range;

  uint errkey;				/* Last dup key */
  uint sortkey, key_used_on_scan;
  uint active_index;
  /* Length of ref (1-8 or the clustered key length) */
  uint ref_length;
  uint block_size;			/* index block size */
  uint raid_type,raid_chunks;
  FT_INFO *ft_handler;
  enum {NONE=0, INDEX, RND} inited;
  bool  auto_increment_column_changed;
  bool implicit_emptied;                /* Can be !=0 only if HEAP */


  handler(TABLE *table_arg) :table(table_arg),
    ref(0), data_file_length(0), max_data_file_length(0), index_file_length(0),
    delete_length(0), auto_increment_value(0),
    records(0), deleted(0), mean_rec_length(0),
    create_time(0), check_time(0), update_time(0),
    key_used_on_scan(MAX_KEY), active_index(MAX_KEY),
    ref_length(sizeof(my_off_t)), block_size(0),
    raid_type(0), ft_handler(0), inited(NONE), implicit_emptied(0)
    {}
  virtual ~handler(void) { /* TODO: DBUG_ASSERT(inited == NONE); */ }
  int ha_open(const char *name, int mode, int test_if_locked);
  void update_timestamp(byte *record);
  void update_auto_increment();
  virtual void print_error(int error, myf errflag);
  virtual bool get_error_message(int error, String *buf);
  uint get_dup_key(int error);
  void change_table_ptr(TABLE *table_arg) { table=table_arg; }
  virtual double scan_time()
    { return ulonglong2double(data_file_length) / IO_SIZE + 2; }
  virtual double read_time(uint index, uint ranges, ha_rows rows)
 { return rows2double(ranges+rows); }
  virtual const key_map *keys_to_use_for_scanning() { return &key_map_empty; }
  virtual bool has_transactions(){ return 0;}
  virtual uint extra_rec_buf_length() { return 0; }
  virtual ha_rows estimate_number_of_rows() { return records+EXTRA_RECORDS; }

  virtual const char *index_type(uint key_number) { DBUG_ASSERT(0); return "";}

  int ha_index_init(uint idx)
  {
    DBUG_ASSERT(inited==NONE);
    inited=INDEX;
    return index_init(idx);
  }
  int ha_index_end()
  {
    DBUG_ASSERT(inited==INDEX);
    inited=NONE;
    return index_end();
  }
  int ha_rnd_init(bool scan)
  {
    DBUG_ASSERT(inited==NONE || (inited==RND && scan));
    inited=RND;
    return rnd_init(scan);
  }
  int ha_rnd_end()
  {
    DBUG_ASSERT(inited==RND);
    inited=NONE;
    return rnd_end();
  }
  /* this is neseccary in many places, e.g. in HANDLER command */
  int ha_index_or_rnd_end()
  {
    return inited == INDEX ? ha_index_end() : inited == RND ? ha_rnd_end() : 0;
  }
  uint get_index(void) const { return active_index; }
  virtual int open(const char *name, int mode, uint test_if_locked)=0;
  virtual int close(void)=0;
  virtual int write_row(byte * buf) { return  HA_ERR_WRONG_COMMAND; }
  virtual int update_row(const byte * old_data, byte * new_data)
   { return  HA_ERR_WRONG_COMMAND; }
  virtual int delete_row(const byte * buf)
   { return  HA_ERR_WRONG_COMMAND; }
  virtual int index_read(byte * buf, const byte * key,
			 uint key_len, enum ha_rkey_function find_flag)
   { return  HA_ERR_WRONG_COMMAND; }
  virtual int index_read_idx(byte * buf, uint index, const byte * key,
			     uint key_len, enum ha_rkey_function find_flag);
  virtual int index_next(byte * buf)
   { return  HA_ERR_WRONG_COMMAND; }
  virtual int index_prev(byte * buf)
   { return  HA_ERR_WRONG_COMMAND; }
  virtual int index_first(byte * buf)
   { return  HA_ERR_WRONG_COMMAND; }
  virtual int index_last(byte * buf)
   { return  HA_ERR_WRONG_COMMAND; }
  virtual int index_next_same(byte *buf, const byte *key, uint keylen);
  virtual int index_read_last(byte * buf, const byte * key, uint key_len)
   { return (my_errno=HA_ERR_WRONG_COMMAND); }
  virtual int read_range_first(const key_range *start_key,
                               const key_range *end_key,
                               bool eq_range, bool sorted);
  virtual int read_range_next();
  int compare_key(key_range *range);
  virtual int ft_init() { return HA_ERR_WRONG_COMMAND; }
  virtual FT_INFO *ft_init_ext(uint flags,uint inx,const byte *key,
                               uint keylen)
    { return NULL; }
  virtual int ft_read(byte *buf) { return HA_ERR_WRONG_COMMAND; }
  virtual int rnd_next(byte *buf)=0;
  virtual int rnd_pos(byte * buf, byte *pos)=0;
  virtual int read_first_row(byte *buf, uint primary_key);
  /*
    The following function is only needed for tables that may be temporary
    tables during joins
  */
  virtual int restart_rnd_next(byte *buf, byte *pos)
    { return HA_ERR_WRONG_COMMAND; }
  virtual int rnd_same(byte *buf, uint inx)
    { return HA_ERR_WRONG_COMMAND; }
  virtual ha_rows records_in_range(uint inx, key_range *min_key,
                                   key_range *max_key)
    { return (ha_rows) 10; }
  virtual void position(const byte *record)=0;
  virtual void info(uint)=0;
  virtual int extra(enum ha_extra_function operation)
  { return 0; }
  virtual int extra_opt(enum ha_extra_function operation, ulong cache_size)
  { return extra(operation); }
  virtual int reset() { return extra(HA_EXTRA_RESET); }
  virtual int external_lock(THD *thd, int lock_type)=0;
  virtual void unlock_row() {}
  virtual int start_stmt(THD *thd) {return 0;}
  /*
    This is called to delete all rows in a table
    If the handler don't support this, then this function will
    return HA_ERR_WRONG_COMMAND and MySQL will delete the rows one
    by one.
  */
  virtual int delete_all_rows()
  { return (my_errno=HA_ERR_WRONG_COMMAND); }
  virtual longlong get_auto_increment();
  virtual void update_create_info(HA_CREATE_INFO *create_info) {}

  /* admin commands - called from mysql_admin_table */
  virtual int check(THD* thd, HA_CHECK_OPT* check_opt)
  { return HA_ADMIN_NOT_IMPLEMENTED; }
  virtual int backup(THD* thd, HA_CHECK_OPT* check_opt)
  { return HA_ADMIN_NOT_IMPLEMENTED; }
  /*
    restore assumes .frm file must exist, and that generate_table() has been
    called; It will just copy the data file and run repair.
  */
  virtual int restore(THD* thd, HA_CHECK_OPT* check_opt)
  { return HA_ADMIN_NOT_IMPLEMENTED; }
  virtual int repair(THD* thd, HA_CHECK_OPT* check_opt)
  { return HA_ADMIN_NOT_IMPLEMENTED; }
  virtual int optimize(THD* thd, HA_CHECK_OPT* check_opt)
  { return HA_ADMIN_NOT_IMPLEMENTED; }
  virtual int analyze(THD* thd, HA_CHECK_OPT* check_opt)
  { return HA_ADMIN_NOT_IMPLEMENTED; }
  virtual int assign_to_keycache(THD* thd, HA_CHECK_OPT* check_opt)
  { return HA_ADMIN_NOT_IMPLEMENTED; }
  virtual int preload_keys(THD* thd, HA_CHECK_OPT* check_opt)
  { return HA_ADMIN_NOT_IMPLEMENTED; }
  /* end of the list of admin commands */

  virtual bool check_and_repair(THD *thd) { return HA_ERR_WRONG_COMMAND; }
  virtual int dump(THD* thd, int fd = -1) { return HA_ERR_WRONG_COMMAND; }
  virtual int disable_indexes(uint mode) { return HA_ERR_WRONG_COMMAND; }
  virtual int enable_indexes(uint mode) { return HA_ERR_WRONG_COMMAND; }
  virtual int indexes_are_disabled(void) {return 0;}
  virtual void start_bulk_insert(ha_rows rows) {}
  virtual int end_bulk_insert() {return 0; }
  virtual int discard_or_import_tablespace(my_bool discard)
  {return HA_ERR_WRONG_COMMAND;}
  virtual int net_read_dump(NET* net) { return HA_ERR_WRONG_COMMAND; }
  virtual char *update_table_comment(const char * comment)
  { return (char*) comment;}
  virtual void append_create_info(String *packet) {}
  virtual char* get_foreign_key_create_info()
  { return(NULL);}  /* gets foreign key create string from InnoDB */
  /* used in REPLACE; is > 0 if table is referred by a FOREIGN KEY */
  virtual uint referenced_by_foreign_key() { return 0;}
  virtual void init_table_handle_for_HANDLER()
  { return; }       /* prepare InnoDB for HANDLER */
  virtual void free_foreign_key_create_info(char* str) {}
  /* The following can be called without an open handler */
  virtual const char *table_type() const =0;
  virtual const char **bas_ext() const =0;
  virtual ulong table_flags(void) const =0;
  virtual ulong index_flags(uint idx, uint part, bool all_parts) const =0;
  virtual ulong index_ddl_flags(KEY *wanted_index) const
  { return (HA_DDL_SUPPORT); }
  virtual int add_index(TABLE *table_arg, KEY *key_info, uint num_of_keys)
  { return (HA_ERR_WRONG_COMMAND); }
  virtual int drop_index(TABLE *table_arg, uint *key_num, uint num_of_keys)
  { return (HA_ERR_WRONG_COMMAND); }

  uint max_record_length() const
  { return min(HA_MAX_REC_LENGTH, max_supported_record_length()); }
  uint max_keys() const
  { return min(MAX_KEY, max_supported_keys()); }
  uint max_key_parts() const
  { return min(MAX_REF_PARTS, max_supported_key_parts()); }
  uint max_key_length() const
  { return min(MAX_KEY_LENGTH, max_supported_key_length()); }
  uint max_key_part_length() const
  { return min(MAX_KEY_LENGTH, max_supported_key_part_length()); }

  virtual uint max_supported_record_length() const { return HA_MAX_REC_LENGTH; }
  virtual uint max_supported_keys() const { return 0; }
  virtual uint max_supported_key_parts() const { return MAX_REF_PARTS; }
  virtual uint max_supported_key_length() const { return MAX_KEY_LENGTH; }
  virtual uint max_supported_key_part_length() const { return 255; }
  virtual uint min_record_length(uint options) const { return 1; }

  virtual bool low_byte_first() const { return 1; }
  virtual uint checksum() const { return 0; }
  virtual bool is_crashed() const  { return 0; }
  virtual bool auto_repair() const { return 0; }

  /*
    default rename_table() and delete_table() rename/delete files with a
    given name and extensions from bas_ext()
  */
  virtual int rename_table(const char *from, const char *to);
  virtual int delete_table(const char *name);
  
  virtual int create(const char *name, TABLE *form, HA_CREATE_INFO *info)=0;

  /* lock_count() can be more than one if the table is a MERGE */
  virtual uint lock_count(void) const { return 1; }
  virtual THR_LOCK_DATA **store_lock(THD *thd,
				     THR_LOCK_DATA **to,
				     enum thr_lock_type lock_type)=0;

  /* Type of table for caching query */
  virtual uint8 table_cache_type() { return HA_CACHE_TBL_NONTRANSACT; }
  /*
    Is query with this table cachable (have sense only for ASKTRANSACT
    tables)
  */
};

	/* Some extern variables used with handlers */

extern struct show_table_type_st sys_table_types[];
extern const char *ha_row_type[];
extern TYPELIB tx_isolation_typelib;

	/* Wrapper functions */
#define ha_commit_stmt(thd) (ha_commit_trans((thd), &((thd)->transaction.stmt)))
#define ha_rollback_stmt(thd) (ha_rollback_trans((thd), &((thd)->transaction.stmt)))
#define ha_commit(thd) (ha_commit_trans((thd), &((thd)->transaction.all)))
#define ha_rollback(thd) (ha_rollback_trans((thd), &((thd)->transaction.all)))

#define ha_supports_generate(T) (T != DB_TYPE_INNODB && \
                                 T != DB_TYPE_BERKELEY_DB && \
                                 T != DB_TYPE_NDBCLUSTER)

bool ha_caching_allowed(THD* thd, char* table_key,
                        uint key_length, uint8 cache_type);
enum db_type ha_resolve_by_name(const char *name, uint namelen);
const char *ha_get_storage_engine(enum db_type db_type);
handler *get_new_handler(TABLE *table, enum db_type db_type);
my_off_t ha_get_ptr(byte *ptr, uint pack_length);
void ha_store_ptr(byte *buff, uint pack_length, my_off_t pos);
int ha_init(void);
int ha_panic(enum ha_panic_function flag);
void ha_close_connection(THD* thd);
enum db_type ha_checktype(enum db_type database_type);
int ha_create_table(const char *name, HA_CREATE_INFO *create_info,
		    bool update_create_info);
int ha_delete_table(enum db_type db_type, const char *path);
void ha_drop_database(char* path);
int ha_init_key_cache(const char *name, KEY_CACHE *key_cache);
int ha_resize_key_cache(KEY_CACHE *key_cache);
int ha_change_key_cache_param(KEY_CACHE *key_cache);
int ha_end_key_cache(KEY_CACHE *key_cache);
int ha_start_stmt(THD *thd);
int ha_report_binlog_offset_and_commit(THD *thd, char *log_file_name,
				       my_off_t end_offset);
int ha_commit_complete(THD *thd);
int ha_release_temporary_latches(THD *thd);
int ha_commit_trans(THD *thd, THD_TRANS *trans);
int ha_rollback_trans(THD *thd, THD_TRANS *trans);
int ha_rollback_to_savepoint(THD *thd, char *savepoint_name);
int ha_savepoint(THD *thd, char *savepoint_name);
int ha_autocommit_or_rollback(THD *thd, int error);
void ha_set_spin_retries(uint retries);
bool ha_flush_logs(void);
int ha_enable_transaction(THD *thd, bool on);
int ha_change_key_cache(KEY_CACHE *old_key_cache,
			KEY_CACHE *new_key_cache);
int ha_discover(const char* dbname, const char* name,
		const void** frmblob, uint* frmlen);

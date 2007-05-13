/* Copyright (C) 2000-2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

/*
  This file defines the NDB Cluster handler: the interface between MySQL and
  NDB Cluster
*/

/* The class defining a handle to an NDB Cluster table */

#ifdef USE_PRAGMA_INTERFACE
#pragma interface                       /* gcc class implementation */
#endif

/* Blob tables and events are internal to NDB and must never be accessed */
#define IS_NDB_BLOB_PREFIX(A) is_prefix(A, "NDB$BLOB")

#include <NdbApi.hpp>
#include <ndbapi_limits.h>

#define NDB_HIDDEN_PRIMARY_KEY_LENGTH 8

class Ndb;             // Forward declaration
class NdbOperation;    // Forward declaration
class NdbTransaction;  // Forward declaration
class NdbRecAttr;      // Forward declaration
class NdbScanOperation; 
class NdbIndexScanOperation; 
class NdbBlob;
class NdbIndexStat;
class NdbEventOperation;
class ha_ndbcluster_cond;

// connectstring to cluster if given by mysqld
extern const char *ndbcluster_connectstring;
extern ulong ndb_cache_check_time;
#ifdef HAVE_NDB_BINLOG
extern ulong ndb_report_thresh_binlog_epoch_slip;
extern ulong ndb_report_thresh_binlog_mem_usage;
#endif

typedef enum ndb_index_type {
  UNDEFINED_INDEX = 0,
  PRIMARY_KEY_INDEX = 1,
  PRIMARY_KEY_ORDERED_INDEX = 2,
  UNIQUE_INDEX = 3,
  UNIQUE_ORDERED_INDEX = 4,
  ORDERED_INDEX = 5
} NDB_INDEX_TYPE;

typedef enum ndb_index_status {
  UNDEFINED = 0,
  ACTIVE = 1,
  TO_BE_DROPPED = 2
} NDB_INDEX_STATUS;

typedef struct ndb_index_data {
  NDB_INDEX_TYPE type;
  NDB_INDEX_STATUS status;  
  const NdbDictionary::Index *index;
  const NdbDictionary::Index *unique_index;
  unsigned char *unique_index_attrid_map;
  bool null_in_unique_index;
  // In this version stats are not shared between threads
  NdbIndexStat* index_stat;
  uint index_stat_cache_entries;
  // Simple counter mechanism to decide when to connect to db
  uint index_stat_update_freq;
  uint index_stat_query_count;
  /*
    In mysqld, keys and rows are stored differently (using KEY_PART_INFO for
    keys and Field for rows).
    So we need to use different NdbRecord for an index for passing values
    from a key and from a row.
  */
  NdbRecord *ndb_record_key;
  NdbRecord *ndb_record_row;
  NdbRecord *ndb_unique_record_key;
  NdbRecord *ndb_unique_record_row;
} NDB_INDEX_DATA;

typedef union { const NdbRecAttr *rec; NdbBlob *blob; void *ptr; } NdbValue;

int get_ndb_blobs_value(TABLE* table, NdbValue* value_array,
                        byte*& buffer, uint& buffer_size,
                        my_ptrdiff_t ptrdiff);

typedef enum {
  NSS_INITIAL= 0,
  NSS_DROPPED,
  NSS_ALTERED 
} NDB_SHARE_STATE;

typedef struct st_ndbcluster_share {
  NDB_SHARE_STATE state;
  MEM_ROOT mem_root;
  THR_LOCK lock;
  pthread_mutex_t mutex;
  char *key;
  uint key_length;
  THD *util_lock;
  uint use_count;
  uint commit_count_lock;
  ulonglong commit_count;
  char *db;
  char *table_name;
  Ndb::TupleIdRange tuple_id_range;
#ifdef HAVE_NDB_BINLOG
  uint32 connect_count;
  uint32 flags;
  NdbEventOperation *op;
  NdbEventOperation *op_old; // for rename table
  char *old_names; // for rename table
  TABLE_SHARE *table_share;
  TABLE *table;
  byte *record[2]; // pointer to allocated records for receiving data
  NdbValue *ndb_value[2];
  MY_BITMAP *subscriber_bitmap;
#endif
} NDB_SHARE;

inline
NDB_SHARE_STATE
get_ndb_share_state(NDB_SHARE *share)
{
  NDB_SHARE_STATE state;
  pthread_mutex_lock(&share->mutex);
  state= share->state;
  pthread_mutex_unlock(&share->mutex);
  return state;
}

inline
void
set_ndb_share_state(NDB_SHARE *share, NDB_SHARE_STATE state)
{
  pthread_mutex_lock(&share->mutex);
  share->state= state;
  pthread_mutex_unlock(&share->mutex);
}

struct Ndb_tuple_id_range_guard {
  Ndb_tuple_id_range_guard(NDB_SHARE* _share) :
    share(_share),
    range(share->tuple_id_range) {
    pthread_mutex_lock(&share->mutex);
  }
  ~Ndb_tuple_id_range_guard() {
    pthread_mutex_unlock(&share->mutex);
  }
  NDB_SHARE* share;
  Ndb::TupleIdRange& range;
};

#ifdef HAVE_NDB_BINLOG
/* NDB_SHARE.flags */
#define NSF_HIDDEN_PK 1 /* table has hidden primary key */
#define NSF_BLOB_FLAG 2 /* table has blob attributes */
#define NSF_NO_BINLOG 4 /* table should not be binlogged */
#endif

typedef enum ndb_query_state_bits {
  NDB_QUERY_NORMAL = 0,
  NDB_QUERY_MULTI_READ_RANGE = 1
} NDB_QUERY_STATE_BITS;

/*
  Place holder for ha_ndbcluster thread specific data
*/

enum THD_NDB_OPTIONS
{
  TNO_NO_LOG_SCHEMA_OP= 1 << 0
};

enum THD_NDB_TRANS_OPTIONS
{
  TNTO_INJECTED_APPLY_STATUS= 1 << 0
};

struct Ndb_local_table_statistics {
  int no_uncommitted_rows_count;
  ulong last_count;
  ha_rows records;
};

typedef struct st_thd_ndb_share {
  const void *key;
  struct Ndb_local_table_statistics stat;
} THD_NDB_SHARE;

class Thd_ndb 
{
 public:
  Thd_ndb();
  ~Thd_ndb();

  void init_open_tables();
  THD_NDB_SHARE *get_open_table(THD *thd, const void *key);

  Ndb_cluster_connection *connection;
  Ndb *ndb;
  ulong count;
  uint lock_count;
  NdbTransaction *all;
  NdbTransaction *stmt;
  bool m_error;
  bool m_slow_path;
  uint32 options;
  uint32 trans_options;
  List<NDB_SHARE> changed_tables;
  uint query_state;
  HASH open_tables;
  /*
    This is a memroot used to buffer rows for batched execution.
    It is reset after every execute().
  */
  MEM_ROOT m_batch_mem_root;
  /*
    Estimated pending batched execution bytes, once this is > BATCH_FLUSH_SIZE
    we execute() to flush the rows buffered in m_batch_mem_root.
  */
  uint m_unsent_bytes;
};

class ha_ndbcluster: public handler
{
 public:
  ha_ndbcluster(handlerton *hton, TABLE_SHARE *table);
  ~ha_ndbcluster();

  int ha_initialise();
  void column_bitmaps_signal();
  int open(const char *name, int mode, uint test_if_locked);
  int close(void);

  int write_row(byte *buf);
  int update_row(const byte *old_data, byte *new_data);
  int delete_row(const byte *buf);
  int index_init(uint index, bool sorted);
  int index_end();
  int index_read(byte *buf, const byte *key, uint key_len, 
                 enum ha_rkey_function find_flag);
  int index_next(byte *buf);
  int index_prev(byte *buf);
  int index_first(byte *buf);
  int index_last(byte *buf);
  int index_read_last(byte * buf, const byte * key, uint key_len);
  int rnd_init(bool scan);
  int rnd_end();
  int rnd_next(byte *buf);
  int rnd_pos(byte *buf, byte *pos);
  void position(const byte *record);
  int read_range_first(const key_range *start_key,
                       const key_range *end_key,
                       bool eq_range, bool sorted);
  int read_range_first_to_buf(const key_range *start_key,
                              const key_range *end_key,
                              bool eq_range, bool sorted,
                              byte* buf);
  int read_range_next();
  int alter_tablespace(st_alter_tablespace *info);

  /**
   * Multi range stuff
   */
  int read_multi_range_first(KEY_MULTI_RANGE **found_range_p,
                             KEY_MULTI_RANGE*ranges, uint range_count,
                             bool sorted, HANDLER_BUFFER *buffer);
  int read_multi_range_next(KEY_MULTI_RANGE **found_range_p);
  bool null_value_index_search(KEY_MULTI_RANGE *ranges,
			       KEY_MULTI_RANGE *end_range,
			       HANDLER_BUFFER *buffer);

  bool get_error_message(int error, String *buf);
  ha_rows records();
  ha_rows estimate_rows_upper_bound()
    { return HA_POS_ERROR; }
  int info(uint);
  void get_dynamic_partition_info(PARTITION_INFO *stat_info, uint part_id);
  int extra(enum ha_extra_function operation);
  int extra_opt(enum ha_extra_function operation, ulong cache_size);
  int reset();
  int external_lock(THD *thd, int lock_type);
  void unlock_row();
  int start_stmt(THD *thd, thr_lock_type lock_type);
  void print_error(int error, myf errflag);
  const char * table_type() const;
  const char ** bas_ext() const;
  ulonglong table_flags(void) const;
  void prepare_for_alter();
  int add_index(TABLE *table_arg, KEY *key_info, uint num_of_keys);
  int prepare_drop_index(TABLE *table_arg, uint *key_num, uint num_of_keys);
  int final_drop_index(TABLE *table_arg);
  void set_part_info(partition_info *part_info);
  ulong index_flags(uint idx, uint part, bool all_parts) const;
  uint max_supported_record_length() const;
  uint max_supported_keys() const;
  uint max_supported_key_parts() const;
  uint max_supported_key_length() const;
  uint max_supported_key_part_length() const;

  int rename_table(const char *from, const char *to);
  int delete_table(const char *name);
  int create(const char *name, TABLE *form, HA_CREATE_INFO *info);
  int create_handler_files(const char *file, const char *old_name,
                           int action_flag, HA_CREATE_INFO *info);
  int get_default_no_partitions(HA_CREATE_INFO *info);
  bool get_no_parts(const char *name, uint *no_parts);
  void set_auto_partitions(partition_info *part_info);
  virtual bool is_fatal_error(int error, uint flags)
  {
    if (!handler::is_fatal_error(error, flags) ||
        error == HA_ERR_NO_PARTITION_FOUND)
      return FALSE;
    return TRUE;
  }

  THR_LOCK_DATA **store_lock(THD *thd,
                             THR_LOCK_DATA **to,
                             enum thr_lock_type lock_type);

  bool low_byte_first() const;

  const char* index_type(uint key_number);

  double scan_time();
  ha_rows records_in_range(uint inx, key_range *min_key, key_range *max_key);
  void start_bulk_insert(ha_rows rows);
  int end_bulk_insert();

  static Thd_ndb* seize_thd_ndb();
  static void release_thd_ndb(Thd_ndb* thd_ndb);
 
static void set_dbname(const char *pathname, char *dbname);
static void set_tabname(const char *pathname, char *tabname);

  /*
    Condition pushdown
  */

 /*
   Push condition down to the table handler.
   SYNOPSIS
     cond_push()
     cond   Condition to be pushed. The condition tree must not be
     modified by the by the caller.
   RETURN
     The 'remainder' condition that caller must use to filter out records.
     NULL means the handler will not return rows that do not match the
     passed condition.
   NOTES
   The pushed conditions form a stack (from which one can remove the
   last pushed condition using cond_pop).
   The table handler filters out rows using (pushed_cond1 AND pushed_cond2 
   AND ... AND pushed_condN)
   or less restrictive condition, depending on handler's capabilities.
   
   handler->reset() call empties the condition stack.
   Calls to rnd_init/rnd_end, index_init/index_end etc do not affect the  
   condition stack.
   The current implementation supports arbitrary AND/OR nested conditions
   with comparisons between columns and constants (including constant
   expressions and function calls) and the following comparison operators:
   =, !=, >, >=, <, <=, like, "not like", "is null", and "is not null". 
   Negated conditions are supported by NOT which generate NAND/NOR groups.
 */ 
  const COND *cond_push(const COND *cond);
 /*
   Pop the top condition from the condition stack of the handler instance.
   SYNOPSIS
     cond_pop()
     Pops the top if condition stack, if stack is not empty
 */
  void cond_pop();

  uint8 table_cache_type();

  /*
   * Internal to ha_ndbcluster, used by C functions
   */
  int ndb_err(NdbTransaction*);

  my_bool register_query_cache_table(THD *thd, char *table_key,
                                     uint key_length,
                                     qc_engine_callback *engine_callback,
                                     ulonglong *engine_data);

  bool check_if_incompatible_data(HA_CREATE_INFO *info,
				  uint table_changes);

private:
  friend int ndbcluster_drop_database_impl(const char *path);
  friend int ndb_handle_schema_change(THD *thd, 
                                      Ndb *ndb, NdbEventOperation *pOp,
                                      NDB_SHARE *share);

  static int delete_table(ha_ndbcluster *h, Ndb *ndb,
			  const char *path,
			  const char *db,
			  const char *table_name);
  int create_ndb_index(const char *name, KEY *key_info, bool unique);
  int create_ordered_index(const char *name, KEY *key_info);
  int create_unique_index(const char *name, KEY *key_info);
  int create_index(const char *name, KEY *key_info, 
                   NDB_INDEX_TYPE idx_type, uint idx_no);
// Index list management
  int create_indexes(Ndb *ndb, TABLE *tab);
  int open_indexes(Ndb *ndb, TABLE *tab, bool ignore_error);
  void renumber_indexes(Ndb *ndb, TABLE *tab);
  int drop_indexes(Ndb *ndb, TABLE *tab);
  int add_index_handle(THD *thd, NdbDictionary::Dictionary *dict,
                       KEY *key_info, const char *index_name, uint index_no);
  int add_table_ndb_record(NdbDictionary::Dictionary *dict);
  int add_hidden_pk_ndb_record(NdbDictionary::Dictionary *dict);
  int add_index_ndb_record(NdbDictionary::Dictionary *dict,
                           KEY *key_info, uint index_no);
  int get_metadata(const char* path);
  void release_metadata(THD *thd, Ndb *ndb);
  NDB_INDEX_TYPE get_index_type(uint idx_no) const;
  NDB_INDEX_TYPE get_index_type_from_table(uint index_no) const;
  NDB_INDEX_TYPE get_index_type_from_key(uint index_no, KEY *key_info, 
                                         bool primary) const;
  bool has_null_in_unique_index(uint idx_no) const;
  bool check_index_fields_not_null(KEY *key_info);

  uint set_up_partition_info(partition_info *part_info,
                             TABLE *table,
                             void *tab);
  char* get_tablespace_name(THD *thd, char *name, uint name_len);
  int set_range_data(void *tab, partition_info* part_info);
  int set_list_data(void *tab, partition_info* part_info);
  int complemented_read(const byte *old_data, byte *new_data,
                        uint32 old_part_id);
  int pk_read(const byte *key, uint key_len, byte *buf, uint32 part_id);
  int ordered_index_scan(const key_range *start_key,
                         const key_range *end_key,
                         bool sorted, bool descending, byte* buf,
                         part_id_range *part_spec);
  int unique_index_read(const byte *key, uint key_len, 
                        byte *buf);
  int unique_index_scan(const KEY* key_info, 
			const byte *key, 
			uint key_len,
			byte *buf);
  int full_table_scan(byte * buf);
  int flush_bulk_insert();
  int ndb_write_row(byte *record, bool primary_key_update, bool batched_update);
  int ndb_delete_row(const byte *record, bool primary_key_update);

  bool check_all_operations_for_error(NdbTransaction *trans,
                                      const NdbOperation *first,
                                      const NdbOperation *last,
                                      uint errcode);
  int peek_indexed_rows(const byte *record, bool check_pk);
  int scan_handle_lock_tuple(NdbScanOperation *scanOp, NdbTransaction *trans);
  int fetch_next(NdbScanOperation* op);
  int next_result(byte *buf); 
  int close_scan();
  void unpack_record(byte *buf);
  void unpack_record_ndbrecord(byte *dst_row, const byte *src_row);
  int get_ndb_lock_type(enum thr_lock_type type,
                        const MY_BITMAP *column_bitmap);

  void set_dbname(const char *pathname);
  void set_tabname(const char *pathname);

  uint offset_hidden_key() { return table->s->reclength; }
  uint offset_user_partition_function() {
    return table->s->reclength +
      (table_share->primary_key == MAX_KEY ?
           NDB_HIDDEN_PRIMARY_KEY_LENGTH : 0);
  }
  uint offset_user_partition_fragment() {
    return table->s->reclength +
      (table_share->primary_key == MAX_KEY ?
           NDB_HIDDEN_PRIMARY_KEY_LENGTH+4 : 4);
  }
  uint field_number_hidden_key() { return table->s->fields; }
  uint field_number_user_partition_function() {
    return table->s->fields +
      (table_share->primary_key == MAX_KEY ? 1 : 0);
  }

  void set_hidden_key(char *row, Uint64 auto_value);
  Uint64 get_hidden_key(const char *row);
  void request_hidden_key(uchar *mask);
  void set_partition_function_value(char *row, uint32 func_value);
  uint32 get_partition_fragment(const char *row);
  void request_partition_function_value(uchar *mask);
  char *batch_copy_row_to_buffer(Thd_ndb *thd_ndb, const byte *record,
                                 bool & batch_full);
  char *batch_copy_key_to_buffer(Thd_ndb *thd_ndb, const byte *key,
                                 uint key_len,
                                 uint op_batch_size, bool & batch_full);
  char *copy_row_to_buffer(Thd_ndb *thd_ndb, const byte *record);
  char *get_row_buffer();
  void clear_extended_column_set(uchar *mask);
  uchar *copy_column_set(MY_BITMAP *bitmap);

  int get_blob_values(NdbOperation *ndb_op, byte *dst_record,
                      const MY_BITMAP *bitmap);
  int set_blob_values(NdbOperation *ndb_op, my_ptrdiff_t row_offset,
                      const MY_BITMAP *bitmap, uint *set_count);
  friend int g_get_ndb_blobs_value(NdbBlob *ndb_blob, void *arg);
  void eventSetAnyValue(const THD *thd, NdbOperation *op);

  NdbOperation *pk_unique_index_read_key(uint idx, const byte *key, byte *buf,
                                         NdbOperation::LockMode lm);
  int read_multi_range_fetch_next();
  
  int set_bounds(NdbIndexScanOperation*, uint inx, bool rir,
                 const key_range *keys[2], uint= 0);
  int primary_key_cmp(const byte * old_row, const byte * new_row);
  void print_results();

  virtual void get_auto_increment(ulonglong offset, ulonglong increment,
                                  ulonglong nb_desired_values,
                                  ulonglong *first_value,
                                  ulonglong *nb_reserved_values);
  bool uses_blob_value(const MY_BITMAP *bitmap);

  char *update_table_comment(const char * comment);

  int write_ndb_file(const char *name);

  int check_ndb_connection(THD* thd= current_thd);

  void set_rec_per_key();
  int records_update();
  void no_uncommitted_rows_execute_failure();
  void no_uncommitted_rows_update(int);
  void no_uncommitted_rows_reset(THD *);

  void release_completed_operations(NdbTransaction*, bool);

  void reset_state_at_execute();
  friend int execute_commit(ha_ndbcluster*, NdbTransaction*);
  friend int execute_no_commit_ignore_no_key(ha_ndbcluster*, NdbTransaction*);
  friend int execute_no_commit(ha_ndbcluster*, NdbTransaction*, bool);
  friend int execute_no_commit_ie(ha_ndbcluster*, NdbTransaction*, bool);

  NdbTransaction *m_active_trans;
  NdbScanOperation *m_active_cursor;
  const NdbDictionary::Table *m_table;
  /*
    Normal NdbRecord for accessing rows, with all fields including hidden
    fields (hidden primary key, user-defined partitioning function value).
  */
  NdbRecord *m_ndb_record;
  /* As m_ndb_record, but adding the FRAGMENT pseudo-column at end of row. */
  NdbRecord *m_ndb_record_fragment;
  /* NdbRecord for accessing tuple by hidden Uint64 primary key. */
  NdbRecord *m_ndb_hidden_key_record;

  /*
    Special NdbRecord for ndb_get_table_statistics(), reading lots of
    pseudo-columns.
  */
  NdbRecord *m_ndb_statistics_record;
  /* Bitmap used for NdbRecord operation column mask. */
  MY_BITMAP m_bitmap;
  my_bitmap_map m_bitmap_buf[(NDB_MAX_ATTRIBUTES_IN_TABLE +
                              8*sizeof(my_bitmap_map) - 1) /
                             (8*sizeof(my_bitmap_map))]; // Buffer for m_bitmap
  /* Bitmap with bit set for all primary key columns. */
  MY_BITMAP m_pk_bitmap;
  my_bitmap_map m_pk_bitmap_buf[(NDB_MAX_ATTRIBUTES_IN_TABLE +
                                 8*sizeof(my_bitmap_map) - 1) /
                                (8*sizeof(my_bitmap_map))]; // Buffer for m_pk_bitmap
  struct Ndb_local_table_statistics *m_table_info;
  char m_dbname[FN_HEADLEN];
  //char m_schemaname[FN_HEADLEN];
  char m_tabname[FN_HEADLEN];
  ulonglong m_table_flags;
  THR_LOCK_DATA m_lock;
  bool m_lock_tuple;
  NDB_SHARE *m_share;
  NDB_INDEX_DATA  m_index[MAX_KEY];
  THD_NDB_SHARE *m_thd_ndb_share;
  /*
    Pointer to row returned from scan nextResult().
  */
  const char *m_next_row;
  /* For read_multi_range scans, the get_range_no() of current row. */
  int m_current_range_no;
  /*
    A buffer of rows for when we cannot pass the mysqld record pointer directly
    to the NDB API, either because the mysqld buffer is too small (eg. hidden
    primary key), or because it will not remain valid until execute() (eg.
    bulk insert).
  */
  char *m_row_buffer;
  uint m_row_buffer_size;
  char *m_row_buffer_current;
  /* Extra bytes needed in row for hidden fields. */
  uint m_extra_reclength;
  // NdbRecAttr has no reference to blob
  NdbValue m_value[NDB_MAX_ATTRIBUTES_IN_TABLE];
  Uint64 m_ref;
  partition_info *m_part_info;
  uint32 m_part_id;
  bool m_use_partition_function;
  bool m_sorted;
  bool m_use_write;
  bool m_ignore_dup_key;
  bool m_has_unique_index;
  bool m_ignore_no_key;
  ha_rows m_rows_to_insert; // TODO: merge it with handler::estimation_rows_to_insert?
  ha_rows m_rows_inserted;
  ha_rows m_rows_changed;
  bool m_delete_cannot_batch;
  bool m_update_cannot_batch;
  ha_rows m_ops_pending;
  uint m_bytes_per_write;
  bool m_skip_auto_increment;
  bool m_blobs_pending;
  bool m_slow_path;

  /* State for setActiveHook() callback for reading blob data. */
  uint m_blob_counter;
  uint m_blob_expected_count;
  byte *m_blob_destination_record;
  Uint64 m_blob_total_size;
  
  // memory for blobs in one tuple
  char *m_blobs_buffer;
  uint32 m_blobs_buffer_size;
  uint m_dupkey;
  // set from thread variables at external lock
  bool m_ha_not_exact_count;
  bool m_force_send;
  ha_rows m_autoincrement_prefetch;
  bool m_transaction_on;

  ha_ndbcluster_cond *m_cond;
  bool m_disable_multi_read;
  byte *m_multi_range_result_ptr;
  KEY_MULTI_RANGE *m_multi_ranges;
  /*
    Points 1 past the end of last multi range operation currently being
    executed, to support splitting large multi range reands into manageable
    pieces.
  */
  KEY_MULTI_RANGE *m_multi_range_defined_end;
  const NdbOperation *m_current_multi_operation;
  Ndb *get_ndb();
};

extern SHOW_VAR ndb_status_variables[];

int ndbcluster_discover(THD* thd, const char* dbname, const char* name,
                        const void** frmblob, uint* frmlen);
int ndbcluster_find_files(THD *thd,const char *db,const char *path,
                          const char *wild, bool dir, List<char> *files);
int ndbcluster_table_exists_in_engine(THD* thd,
                                      const char *db, const char *name);
void ndbcluster_print_error(int error, const NdbOperation *error_op);

static const char ndbcluster_hton_name[]= "ndbcluster";
static const int ndbcluster_hton_name_length=sizeof(ndbcluster_hton_name)-1;
extern int ndbcluster_terminating;
extern int ndb_util_thread_running;
extern pthread_cond_t COND_ndb_util_ready;

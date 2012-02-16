#ifndef HA_NDBCLUSTER_INCLUDED
#define HA_NDBCLUSTER_INCLUDED

/* Copyright (c) 2000, 2010, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

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

#ifdef HAVE_PSI_INTERFACE
extern PSI_file_key key_file_ndb;
#endif /* HAVE_PSI_INTERFACE */


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

#include "sql_partition.h"                      /* part_id_range */

// connectstring to cluster if given by mysqld
extern const char *ndbcluster_connectstring;

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
} NDB_INDEX_DATA;

typedef enum ndb_write_op {
  NDB_INSERT = 0,
  NDB_UPDATE = 1,
  NDB_PK_UPDATE = 2
} NDB_WRITE_OP;

typedef union { const NdbRecAttr *rec; NdbBlob *blob; void *ptr; } NdbValue;

int get_ndb_blobs_value(TABLE* table, NdbValue* value_array,
                        uchar*& buffer, uint& buffer_size,
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
  mysql_mutex_t mutex;
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
  uchar *record[2]; // pointer to allocated records for receiving data
  NdbValue *ndb_value[2];
  MY_BITMAP *subscriber_bitmap;
#endif
} NDB_SHARE;

inline
NDB_SHARE_STATE
get_ndb_share_state(NDB_SHARE *share)
{
  NDB_SHARE_STATE state;
  mysql_mutex_lock(&share->mutex);
  state= share->state;
  mysql_mutex_unlock(&share->mutex);
  return state;
}

inline
void
set_ndb_share_state(NDB_SHARE *share, NDB_SHARE_STATE state)
{
  mysql_mutex_lock(&share->mutex);
  share->state= state;
  mysql_mutex_unlock(&share->mutex);
}

struct Ndb_tuple_id_range_guard {
  Ndb_tuple_id_range_guard(NDB_SHARE* _share) :
    share(_share),
    range(share->tuple_id_range) {
    mysql_mutex_lock(&share->mutex);
  }
  ~Ndb_tuple_id_range_guard() {
    mysql_mutex_unlock(&share->mutex);
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
  ,TNTO_NO_LOGGING=           1 << 1
};

struct Ndb_local_table_statistics {
  int no_uncommitted_rows_count;
  ulong last_count;
  ha_rows records;
};

class Thd_ndb 
{
 public:
  Thd_ndb();
  ~Thd_ndb();

  void init_open_tables();

  Ndb *ndb;
  ulong count;
  uint lock_count;
  uint start_stmt_count;
  NdbTransaction *trans;
  bool m_error;
  bool m_slow_path;
  int m_error_code;
  uint32 m_query_id; /* query id whn m_error_code was set */
  uint32 options;
  uint32 trans_options;
  List<NDB_SHARE> changed_tables;
  uint query_state;
  HASH open_tables;
};

class ha_ndbcluster: public handler
{
 public:
  ha_ndbcluster(handlerton *hton, TABLE_SHARE *table);
  ~ha_ndbcluster();

  int ha_initialise();
  int open(const char *name, int mode, uint test_if_locked);
  int close(void);

  int write_row(uchar *buf);
  int update_row(const uchar *old_data, uchar *new_data);
  int delete_row(const uchar *buf);
  int index_init(uint index, bool sorted);
  int index_end();
  int index_read(uchar *buf, const uchar *key, uint key_len, 
                 enum ha_rkey_function find_flag);
  int index_next(uchar *buf);
  int index_prev(uchar *buf);
  int index_first(uchar *buf);
  int index_last(uchar *buf);
  int index_read_last(uchar * buf, const uchar * key, uint key_len);
  int rnd_init(bool scan);
  int rnd_end();
  int rnd_next(uchar *buf);
  int rnd_pos(uchar *buf, uchar *pos);
  void position(const uchar *record);
  int read_range_first(const key_range *start_key,
                       const key_range *end_key,
                       bool eq_range, bool sorted);
  int read_range_first_to_buf(const key_range *start_key,
                              const key_range *end_key,
                              bool eq_range, bool sorted,
                              uchar* buf);
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
  void get_dynamic_partition_info(PARTITION_STATS *stat_info, uint part_id);
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
  int loc_read_multi_range_next(KEY_MULTI_RANGE **found_range_p);
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
  int complemented_read(const uchar *old_data, uchar *new_data,
                        uint32 old_part_id);
  int pk_read(const uchar *key, uint key_len, uchar *buf, uint32 part_id);
  int ordered_index_scan(const key_range *start_key,
                         const key_range *end_key,
                         bool sorted, bool descending, uchar* buf,
                         part_id_range *part_spec);
  int unique_index_read(const uchar *key, uint key_len, 
                        uchar *buf);
  int unique_index_scan(const KEY* key_info, 
			const uchar *key, 
			uint key_len,
			uchar *buf);
  int full_table_scan(uchar * buf);

  bool check_all_operations_for_error(NdbTransaction *trans,
                                      const NdbOperation *first,
                                      const NdbOperation *last,
                                      uint errcode);
  int peek_indexed_rows(const uchar *record, NDB_WRITE_OP write_op);
  int fetch_next(NdbScanOperation* op);
  int set_auto_inc(Field *field);
  int next_result(uchar *buf); 
  int define_read_attrs(uchar* buf, NdbOperation* op);
  int filtered_scan(const uchar *key, uint key_len, 
                    uchar *buf,
                    enum ha_rkey_function find_flag);
  int close_scan();
  void unpack_record(uchar *buf);
  int get_ndb_lock_type(enum thr_lock_type type);

  void set_dbname(const char *pathname);
  void set_tabname(const char *pathname);

  bool set_hidden_key(NdbOperation*,
                      uint fieldnr, const uchar* field_ptr);
  int set_ndb_key(NdbOperation*, Field *field,
                  uint fieldnr, const uchar* field_ptr);
  int set_ndb_value(NdbOperation*, Field *field, uint fieldnr,
		    int row_offset= 0, bool *set_blob_value= 0);
  int get_ndb_value(NdbOperation*, Field *field, uint fieldnr, uchar*);
  int get_ndb_partition_id(NdbOperation *);
  friend int g_get_ndb_blobs_value(NdbBlob *ndb_blob, void *arg);
  int set_primary_key(NdbOperation *op, const uchar *key);
  int set_primary_key_from_record(NdbOperation *op, const uchar *record);
  bool check_index_fields_in_write_set(uint keyno);
  int set_index_key_from_record(NdbOperation *op, const uchar *record,
                                uint keyno);
  int set_bounds(NdbIndexScanOperation*, uint inx, bool rir,
                 const key_range *keys[2], uint= 0);
  int key_cmp(uint keynr, const uchar * old_row, const uchar * new_row);
  int set_index_key(NdbOperation *, const KEY *key_info, const uchar *key_ptr);
  void print_results();

  virtual void get_auto_increment(ulonglong offset, ulonglong increment,
                                  ulonglong nb_desired_values,
                                  ulonglong *first_value,
                                  ulonglong *nb_reserved_values);
  bool uses_blob_value();

  char *update_table_comment(const char * comment);

  int write_ndb_file(const char *name);

  int check_ndb_connection(THD* thd= current_thd);

  void set_rec_per_key();
  int records_update();
  void no_uncommitted_rows_execute_failure();
  void no_uncommitted_rows_update(int);
  void no_uncommitted_rows_reset(THD *);

  void release_completed_operations(NdbTransaction*, bool);

  friend int execute_commit(ha_ndbcluster*, NdbTransaction*);
  friend int execute_no_commit_ignore_no_key(ha_ndbcluster*, NdbTransaction*);
  friend int execute_no_commit(ha_ndbcluster*, NdbTransaction*, bool);
  friend int execute_no_commit_ie(ha_ndbcluster*, NdbTransaction*, bool);

  void transaction_checks(THD *thd);
  int start_statement(THD *thd, Thd_ndb *thd_ndb, Ndb* ndb);
  int init_handler_for_statement(THD *thd, Thd_ndb *thd_ndb);

  NdbTransaction *m_active_trans;
  NdbScanOperation *m_active_cursor;
  const NdbDictionary::Table *m_table;
  struct Ndb_local_table_statistics *m_table_info;
  struct Ndb_local_table_statistics m_table_info_instance;
  char m_dbname[FN_HEADLEN];
  //char m_schemaname[FN_HEADLEN];
  char m_tabname[FN_HEADLEN];
  ulonglong m_table_flags;
  THR_LOCK_DATA m_lock;
  bool m_lock_tuple;
  NDB_SHARE *m_share;
  NDB_INDEX_DATA  m_index[MAX_KEY];
  // NdbRecAttr has no reference to blob
  NdbValue m_value[NDB_MAX_ATTRIBUTES_IN_TABLE];
  uchar m_ref[NDB_HIDDEN_PRIMARY_KEY_LENGTH];
  partition_info *m_part_info;
  uint32 m_part_id;
  uchar *m_rec0;
  Field **m_part_field_array;
  bool m_use_partition_function;
  bool m_sorted;
  bool m_use_write;
  bool m_ignore_dup_key;
  bool m_has_unique_index;
  bool m_primary_key_update;
  bool m_write_op;
  bool m_ignore_no_key;
  ha_rows m_rows_to_insert; // TODO: merge it with handler::estimation_rows_to_insert?
  ha_rows m_rows_inserted;
  ha_rows m_bulk_insert_rows;
  ha_rows m_rows_changed;
  bool m_bulk_insert_not_flushed;
  bool m_delete_cannot_batch;
  bool m_update_cannot_batch;
  ha_rows m_ops_pending;
  bool m_skip_auto_increment;
  bool m_blobs_pending;
  bool m_slow_path;
  my_ptrdiff_t m_blobs_offset;
  // memory for blobs in one tuple
  uchar *m_blobs_buffer;
  uint32 m_blobs_buffer_size;
  uint m_dupkey;
  // set from thread variables at external lock
  bool m_ha_not_exact_count;
  bool m_force_send;
  ha_rows m_autoincrement_prefetch;
  bool m_transaction_on;

  ha_ndbcluster_cond *m_cond;
  bool m_disable_multi_read;
  uchar *m_multi_range_result_ptr;
  KEY_MULTI_RANGE *m_multi_ranges;
  KEY_MULTI_RANGE *m_multi_range_defined;
  const NdbOperation *m_current_multi_operation;
  NdbIndexScanOperation *m_multi_cursor;
  uchar *m_multi_range_cursor_result_ptr;
  int setup_recattr(const NdbRecAttr*);
  Ndb *get_ndb();
};

extern SHOW_VAR ndb_status_variables[];

int ndbcluster_discover(THD* thd, const char* dbname, const char* name,
                        const void** frmblob, uint* frmlen);
int ndbcluster_find_files(THD *thd,const char *db,const char *path,
                          const char *wild, bool dir, List<LEX_STRING> *files);
int ndbcluster_table_exists_in_engine(THD* thd,
                                      const char *db, const char *name);
void ndbcluster_print_error(int error, const NdbOperation *error_op);

static const char ndbcluster_hton_name[]= "ndbcluster";
static const int ndbcluster_hton_name_length=sizeof(ndbcluster_hton_name)-1;
extern int ndbcluster_terminating;
extern int ndb_util_thread_running;
extern mysql_cond_t COND_ndb_util_ready;

#endif /* HA_NDBCLUSTER_INCLUDED */

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

#include <ndbapi_limits.h>

#define NDB_HIDDEN_PRIMARY_KEY_LENGTH 8
/* Forward declarations */
class Ndb;             
class NdbOperation;    
class NdbTransaction;  
class NdbRecAttr;      
class NdbScanOperation; 
class NdbIndexScanOperation; 
class NdbBlob;
class ha_ndbcluster_cond;

// connectstring to cluster if given by mysqld
extern const char *ndbcluster_connectstring;
extern ulong ndb_cache_check_time;

typedef enum ndb_index_type {
  UNDEFINED_INDEX = 0,
  PRIMARY_KEY_INDEX = 1,
  PRIMARY_KEY_ORDERED_INDEX = 2,
  UNIQUE_INDEX = 3,
  UNIQUE_ORDERED_INDEX = 4,
  ORDERED_INDEX = 5
} NDB_INDEX_TYPE;

typedef struct ndb_index_data {
  NDB_INDEX_TYPE type;
  void *index;
  void *unique_index;
  unsigned char *unique_index_attrid_map;
  bool null_in_unique_index;
} NDB_INDEX_DATA;

typedef struct st_ndbcluster_share {
  THR_LOCK lock;
  pthread_mutex_t mutex;
  char *table_name;
  uint table_name_length,use_count;
  uint commit_count_lock;
  ulonglong commit_count;
} NDB_SHARE;

typedef enum ndb_query_state_bits {
  NDB_QUERY_NORMAL = 0,
  NDB_QUERY_MULTI_READ_RANGE = 1
} NDB_QUERY_STATE_BITS;

/*
  Place holder for ha_ndbcluster thread specific data
*/

class Thd_ndb 
{
 public:
  Thd_ndb();
  ~Thd_ndb();
  Ndb *ndb;
  ulong count;
  uint lock_count;
  NdbTransaction *all;
  NdbTransaction *stmt;
  int error;
  List<NDB_SHARE> changed_tables;
  uint query_state;
};

class ha_ndbcluster: public handler
{
 public:
  ha_ndbcluster(TABLE *table);
  ~ha_ndbcluster();

  int open(const char *name, int mode, uint test_if_locked);
  int close(void);

  int write_row(byte *buf);
  int update_row(const byte *old_data, byte *new_data);
  int delete_row(const byte *buf);
  int index_init(uint index);
  int index_end();
  int index_read(byte *buf, const byte *key, uint key_len, 
                 enum ha_rkey_function find_flag);
  int index_read_idx(byte *buf, uint index, const byte *key, uint key_len, 
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
  int info(uint);
  int extra(enum ha_extra_function operation);
  int extra_opt(enum ha_extra_function operation, ulong cache_size);
  int reset();
  int external_lock(THD *thd, int lock_type);
  void unlock_row();
  int start_stmt(THD *thd, thr_lock_type lock_type);
  const char * table_type() const;
  const char ** bas_ext() const;
  ulong table_flags(void) const;
  ulong index_flags(uint idx, uint part, bool all_parts) const;
  uint max_supported_record_length() const;
  uint max_supported_keys() const;
  uint max_supported_key_parts() const;
  uint max_supported_key_length() const;
  uint max_supported_key_part_length() const;

  int rename_table(const char *from, const char *to);
  int delete_table(const char *name);
  int create(const char *name, TABLE *form, HA_CREATE_INFO *info);
  THR_LOCK_DATA **store_lock(THD *thd,
                             THR_LOCK_DATA **to,
                             enum thr_lock_type lock_type);

  bool low_byte_first() const;
  bool has_transactions();
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
   
   handler->extra(HA_EXTRA_RESET) call empties the condition stack.
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
private:
  int alter_table_name(const char *to);
  int drop_table();
  int create_index(const char *name, KEY *key_info, bool unique);
  int create_ordered_index(const char *name, KEY *key_info);
  int create_unique_index(const char *name, KEY *key_info);
  int initialize_autoincrement(const void *table);
  enum ILBP {ILBP_CREATE = 0, ILBP_OPEN = 1}; // Index List Build Phase
  int build_index_list(Ndb *ndb, TABLE *tab, enum ILBP phase);
  int get_metadata(const char* path);
  void release_metadata();
  NDB_INDEX_TYPE get_index_type(uint idx_no) const;
  NDB_INDEX_TYPE get_index_type_from_table(uint index_no) const;
  bool has_null_in_unique_index(uint idx_no) const;
  bool check_index_fields_not_null(uint index_no);

  int pk_read(const byte *key, uint key_len, byte *buf);
  int complemented_pk_read(const byte *old_data, byte *new_data);
  bool check_all_operations_for_error(NdbTransaction *trans,
                                      const NdbOperation *first,
                                      const NdbOperation *last,
                                      uint errcode);
  int peek_indexed_rows(const byte *record, bool check_pk);
  int unique_index_read(const byte *key, uint key_len, 
                        byte *buf);
  int ordered_index_scan(const key_range *start_key,
                         const key_range *end_key,
                         bool sorted, bool descending, byte* buf);
  int unique_index_scan(const KEY* key_info, 
			const byte *key, 
			uint key_len,
			byte *buf);

  int full_table_scan(byte * buf);
  int fetch_next(NdbScanOperation* op);
  int next_result(byte *buf); 
  int define_read_attrs(byte* buf, NdbOperation* op);
  int filtered_scan(const byte *key, uint key_len, 
                    byte *buf,
                    enum ha_rkey_function find_flag);
  int close_scan();
  void unpack_record(byte *buf);
  int get_ndb_lock_type(enum thr_lock_type type);

  void set_dbname(const char *pathname);
  void set_tabname(const char *pathname);

  bool set_hidden_key(NdbOperation*,
                      uint fieldnr, const byte* field_ptr);
  int set_ndb_key(NdbOperation*, Field *field,
                  uint fieldnr, const byte* field_ptr);
  int set_ndb_value(NdbOperation*, Field *field, uint fieldnr, bool *set_blob_value= 0);
  int get_ndb_value(NdbOperation*, Field *field, uint fieldnr, byte*);
  friend int g_get_ndb_blobs_value(NdbBlob *ndb_blob, void *arg);
  int get_ndb_blobs_value(NdbBlob *last_ndb_blob, my_ptrdiff_t ptrdiff);
  int set_primary_key(NdbOperation *op, const byte *key);
  int set_primary_key_from_record(NdbOperation *op, const byte *record);
  int set_index_key_from_record(NdbOperation *op, const byte *record,
                                uint keyno);
  int set_bounds(NdbIndexScanOperation*, const key_range *keys[2], uint= 0);
  int key_cmp(uint keynr, const byte * old_row, const byte * new_row);
  int set_index_key(NdbOperation *, const KEY *key_info, const byte *key_ptr);
  void print_results();

  ulonglong get_auto_increment();
  void invalidate_dictionary_cache(bool global);

bool uses_blob_value(bool all_fields);

  char *update_table_comment(const char * comment);

  int write_ndb_file();

  int check_ndb_connection(THD* thd= current_thd);

  void set_rec_per_key();
  int records_update();
  void no_uncommitted_rows_execute_failure();
  void no_uncommitted_rows_update(int);
  void no_uncommitted_rows_init(THD *);
  void no_uncommitted_rows_reset(THD *);

  friend int execute_commit(ha_ndbcluster*, NdbTransaction*);
  friend int execute_no_commit(ha_ndbcluster*, NdbTransaction*, bool);
  friend int execute_no_commit_ie(ha_ndbcluster*, NdbTransaction*, bool);

  NdbTransaction *m_active_trans;
  NdbScanOperation *m_active_cursor;
  void *m_table;
  int m_table_version;
  void *m_table_info;
  char m_dbname[FN_HEADLEN];
  //char m_schemaname[FN_HEADLEN];
  char m_tabname[FN_HEADLEN];
  ulong m_table_flags;
  THR_LOCK_DATA m_lock;
  bool m_lock_tuple;
  NDB_SHARE *m_share;
  NDB_INDEX_DATA  m_index[MAX_KEY];
  // NdbRecAttr has no reference to blob
  typedef union { const NdbRecAttr *rec; NdbBlob *blob; void *ptr; } NdbValue;
  NdbValue m_value[NDB_MAX_ATTRIBUTES_IN_TABLE];
  byte m_ref[NDB_HIDDEN_PRIMARY_KEY_LENGTH];
  bool m_use_write;
  bool m_ignore_dup_key;
  bool m_has_unique_index;
  bool m_primary_key_update;
  bool m_retrieve_all_fields;
  bool m_retrieve_primary_key;
  ha_rows m_rows_to_insert;
  ha_rows m_rows_inserted;
  ha_rows m_bulk_insert_rows;
  ha_rows m_rows_changed;
  bool m_bulk_insert_not_flushed;
  bool m_delete_cannot_batch;
  bool m_update_cannot_batch;
  ha_rows m_ops_pending;
  bool m_skip_auto_increment;
  bool m_blobs_pending;
  my_ptrdiff_t m_blobs_offset;
  // memory for blobs in one tuple
  char *m_blobs_buffer;
  uint32 m_blobs_buffer_size;
  uint m_dupkey;
  // set from thread variables at external lock
  bool m_ha_not_exact_count;
  bool m_force_send;
  ha_rows m_autoincrement_prefetch;
  bool m_transaction_on;
  void release_completed_operations(NdbTransaction*, bool);

  ha_ndbcluster_cond *m_cond;
  bool m_disable_multi_read;
  byte *m_multi_range_result_ptr;
  KEY_MULTI_RANGE *m_multi_ranges;
  KEY_MULTI_RANGE *m_multi_range_defined;
  const NdbOperation *m_current_multi_operation;
  NdbIndexScanOperation *m_multi_cursor;
  byte *m_multi_range_cursor_result_ptr;
  int setup_recattr(const NdbRecAttr*);
  Ndb *get_ndb();
};

extern struct show_var_st ndb_status_variables[];

bool ndbcluster_init(void);
bool ndbcluster_end(void);

int ndbcluster_discover(THD* thd, const char* dbname, const char* name,
                        const void** frmblob, uint* frmlen);
int ndbcluster_find_files(THD *thd,const char *db,const char *path,
                          const char *wild, bool dir, List<char> *files);
int ndbcluster_table_exists_in_engine(THD* thd,
                                      const char *db, const char *name);
int ndbcluster_drop_database(const char* path);

void ndbcluster_print_error(int error, const NdbOperation *error_op);

int ndbcluster_show_status(THD*);

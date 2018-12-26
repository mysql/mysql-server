/*
   Copyright (c) 2000, 2018, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

/*
  This file defines the NDB Cluster handler: the interface between MySQL and
  NDB Cluster
*/


/* DDL names have to fit in system table ndb_schema */
#define NDB_MAX_DDL_NAME_BYTESIZE 63
#define NDB_MAX_DDL_NAME_BYTESIZE_STR "63"

/* Blob tables and events are internal to NDB and must never be accessed */
#define IS_NDB_BLOB_PREFIX(A) is_prefix(A, "NDB$BLOB")

#include <ndbapi/NdbApi.hpp>
#include <ndbapi/ndbapi_limits.h>
#include <kernel/ndb_limits.h>
#include "ndb_conflict.h"
#include "partitioning/partition_handler.h"

#define NDB_IGNORE_VALUE(x) (void)x

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
class NdbQuery;
class NdbQueryOperation;
class NdbQueryOperationTypeWrapper;
class NdbQueryParamValue;
class ndb_pushed_join;

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
  /*
    In mysqld, keys and rows are stored differently (using KEY_PART_INFO for
    keys and Field for rows).
    So we need to use different NdbRecord for an index for passing values
    from a key and from a row.
  */
  NdbRecord *ndb_record_key;
  NdbRecord *ndb_unique_record_key;
  NdbRecord *ndb_unique_record_row;
} NDB_INDEX_DATA;

// Wrapper class for list to hold NDBFKs
class Ndb_fk_list :public List<NdbDictionary::ForeignKey>
{
public:
  ~Ndb_fk_list()
  {
    delete_elements();
  }
};

typedef enum ndb_write_op {
  NDB_INSERT = 0,
  NDB_UPDATE = 1,
  NDB_PK_UPDATE = 2
} NDB_WRITE_OP;

#include "ndb_ndbapi_util.h"
#include "ndb_share.h"

struct Ndb_local_table_statistics {
  int no_uncommitted_rows_count;
  ulong last_count;
  ha_rows records;
};

#include "ndb_thd_ndb.h"

struct st_ndb_status {
  st_ndb_status() { memset(this, 0, sizeof(struct st_ndb_status)); }
  long cluster_node_id;
  const char * connected_host;
  long connected_port;
  long number_of_replicas;
  long number_of_data_nodes;
  long number_of_ready_data_nodes;
  long connect_count;
  long execute_count;
  long scan_count;
  long pruned_scan_count;
  long schema_locks_count;
  long sorted_scan_count;
  long pushed_queries_defined;
  long pushed_queries_dropped;
  long pushed_queries_executed;
  long pushed_reads;
  long long last_commit_epoch_server;
  long long last_commit_epoch_session;
  long transaction_no_hint_count[MAX_NDB_NODES];
  long transaction_hint_count[MAX_NDB_NODES];
  long long api_client_stats[Ndb::NumClientStatistics];
};

int ndbcluster_commit(handlerton *hton, THD *thd, bool all);


class ha_ndbcluster: public handler, public Partition_handler
{
  friend class ndb_pushed_builder_ctx;

 public:
  ha_ndbcluster(handlerton *hton, TABLE_SHARE *table);
  ~ha_ndbcluster();

  int open(const char *name, int mode, uint test_if_locked);
  int close(void);
  void local_close(THD *thd, bool release_metadata);

  int optimize(THD* thd, HA_CHECK_OPT* check_opt);
  int analyze(THD* thd, HA_CHECK_OPT* check_opt);
  int analyze_index(THD* thd);

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
  virtual int cmp_ref(const uchar * ref1, const uchar * ref2);
  int read_range_first(const key_range *start_key,
                       const key_range *end_key,
                       bool eq_range, bool sorted);
  int read_range_first_to_buf(const key_range *start_key,
                              const key_range *end_key,
                              bool eq_range, bool sorted,
                              uchar* buf);
  int read_range_next();

  /**
   * Multi Range Read interface
   */
  int multi_range_read_init(RANGE_SEQ_IF *seq, void *seq_init_param,
                            uint n_ranges, uint mode, HANDLER_BUFFER *buf);
  int multi_range_read_next(char **range_info);
  ha_rows multi_range_read_info_const(uint keyno, RANGE_SEQ_IF *seq,
                                      void *seq_init_param, 
                                      uint n_ranges, uint *bufsz,
                                      uint *flags, Cost_estimate *cost);
  ha_rows multi_range_read_info(uint keyno, uint n_ranges, uint keys,
                                uint *bufsz, uint *flags, Cost_estimate *cost);
private:
  bool choose_mrr_impl(uint keyno, uint n_ranges, ha_rows n_rows,
                       uint *bufsz, uint *flags,
                       Cost_estimate *cost);

private:
  uint first_running_range;
  uint first_range_in_batch;
  uint first_unstarted_range;
  /* TRUE <=> need range association */
  bool mrr_need_range_assoc;

  int multi_range_start_retrievals(uint first_range);

public:
  bool get_error_message(int error, String *buf);
  virtual int records(ha_rows *num_rows);
  ha_rows estimate_rows_upper_bound()
    { return HA_POS_ERROR; }
  int info(uint);
  uint32 calculate_key_hash_value(Field **field_array);
  bool start_read_removal(void);
  ha_rows end_read_removal(void);
  int extra(enum ha_extra_function operation);
  int extra_opt(enum ha_extra_function operation, ulong cache_size);
  int reset();
  int external_lock(THD *thd, int lock_type);
  void unlock_row();
  int start_stmt(THD *thd, thr_lock_type lock_type);
  void update_create_info(HA_CREATE_INFO *create_info);
  void print_error(int error, myf errflag);
  const char * table_type() const;
  const char ** bas_ext() const;
  ulonglong table_flags(void) const;
  ulong index_flags(uint idx, uint part, bool all_parts) const;
  virtual const key_map *keys_to_use_for_scanning() { return &btree_keys; }
  bool primary_key_is_clustered() const;
  uint max_supported_record_length() const;
  uint max_supported_keys() const;
  uint max_supported_key_parts() const;
  uint max_supported_key_length() const;
  uint max_supported_key_part_length(HA_CREATE_INFO *create_info) const;

  virtual bool is_fk_defined_on_table_or_index(uint index);
  int get_child_or_parent_fk_list(THD *thd,
                                  List<FOREIGN_KEY_INFO>*f_key_list,
                                  bool is_child, bool is_parent);
  virtual int get_foreign_key_list(THD *thd,
                                   List<FOREIGN_KEY_INFO>*f_key_list);
  virtual int get_parent_foreign_key_list(THD *thd,
                                          List<FOREIGN_KEY_INFO>*f_key_list);
  virtual uint referenced_by_foreign_key();
  uint is_child_or_parent_of_fk();
  virtual bool can_switch_engines();
  virtual char* get_foreign_key_create_info();
  virtual void free_foreign_key_create_info(char* str);

  int rename_table(const char *from, const char *to);
  int delete_table(const char *name);
  int create(const char *name, TABLE *form, HA_CREATE_INFO *info);
  virtual bool is_ignorable_error(int error)
  {
    if (handler::is_ignorable_error(error) ||
        error == HA_ERR_NO_PARTITION_FOUND)
      return true;
    return false;
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

  bool start_bulk_update();
  int bulk_update_row(const uchar *old_data, uchar *new_data,
                      uint *dup_key_found);
  int exec_bulk_update(uint *dup_key_found);
  void end_bulk_update();
  int ndb_update_row(const uchar *old_data, uchar *new_data,
                     int is_bulk_update);

static void set_dbname(const char *pathname, char *dbname);
static void set_tabname(const char *pathname, char *tabname);

  /*
    static member function as it needs to access private
    NdbTransaction methods
  */
  static void release_completed_operations(NdbTransaction*);

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
  const Item *cond_push(const Item *cond);
 /*
   Pop the top condition from the condition stack of the handler instance.
   SYNOPSIS
     cond_pop()
     Pops the top if condition stack, if stack is not empty
 */
  void cond_pop();

  bool maybe_pushable_join(const char*& reason) const;
  int assign_pushed_join(const ndb_pushed_join* pushed_join);

  uint number_of_pushed_joins() const;
  const TABLE* root_of_pushed_join() const;
  const TABLE* parent_of_pushed_join() const;

  int index_read_pushed(uchar *buf, const uchar *key,
                        key_part_map keypart_map);

  int index_next_pushed(uchar * buf);

  uint8 table_cache_type();

  /*
   * Internal to ha_ndbcluster, used by C functions
   */
  int ndb_err(NdbTransaction*);

  my_bool register_query_cache_table(THD *thd, char *table_key,
                                     size_t key_length,
                                     qc_engine_callback *engine_callback,
                                     ulonglong *engine_data);
enum_alter_inplace_result
  check_if_supported_inplace_alter(TABLE *altered_table,
                                   Alter_inplace_info *ha_alter_info);

bool prepare_inplace_alter_table(TABLE *altered_table,
                                    Alter_inplace_info *ha_alter_info);

bool inplace_alter_table(TABLE *altered_table,
                            Alter_inplace_info *ha_alter_info);
  
bool commit_inplace_alter_table(TABLE *altered_table,
                                   Alter_inplace_info *ha_alter_info,
                                   bool commit);

void notify_table_changed();

private:
  void prepare_for_alter();
  /*
  int add_index(TABLE *table_arg, KEY *key_info, uint num_of_keys,
		handler_add_index **add);
  */
  int prepare_drop_index(TABLE *table_arg, uint *key_num, uint num_of_keys);
  int final_drop_index(TABLE *table_arg);
  
  bool abort_inplace_alter_table(TABLE *altered_table,
                                 Alter_inplace_info *ha_alter_info);
#ifdef HAVE_NDB_BINLOG
  int prepare_conflict_detection(enum_conflicting_op_type op_type,
                                 const NdbRecord* key_rec,
                                 const NdbRecord* data_rec,
                                 const uchar* old_data,
                                 const uchar* new_data,
                                 const MY_BITMAP *write_set,
                                 NdbTransaction* trans,
                                 NdbInterpretedCode* code,
                                 NdbOperation::OperationOptions* options,
                                 bool& conflict_handled,
                                 bool& avoid_ndbapi_write);
#endif
  void setup_key_ref_for_ndb_record(const NdbRecord **key_rec,
                                    const uchar **key_row,
                                    const uchar *record,
                                    bool use_active_index);
  friend int ndbcluster_drop_database_impl(THD *thd, const char *path);

  void check_read_before_write_removal();
  static int drop_table_impl(THD *thd, ha_ndbcluster *h, Ndb *ndb,
                             const char *path,
                             const char *db,
                             const char *table_name);

  int add_index_impl(THD *thd, TABLE *table_arg,
                     KEY *key_info, uint num_of_keys);
  int create_ndb_index(THD *thd, const char *name, KEY *key_info,
                       bool unique) const;
  int create_ordered_index(THD *thd, const char *name, KEY *key_info) const;
  int create_unique_index(THD *thd, const char *name, KEY *key_info) const;
  int create_index(THD *thd, const char *name, KEY *key_info, 
                   NDB_INDEX_TYPE idx_type, uint idx_no) const;
// Index list management
  int create_indexes(THD *thd, Ndb *ndb, TABLE *tab) const;
  int open_indexes(THD *thd, Ndb *ndb, TABLE *tab, bool ignore_error);
  void renumber_indexes(Ndb *ndb, TABLE *tab);
  int drop_indexes(Ndb *ndb, TABLE *tab);
  int add_index_handle(THD *thd, NdbDictionary::Dictionary *dict,
                       KEY *key_info, const char *key_name, uint index_no);
  int add_table_ndb_record(NdbDictionary::Dictionary *dict);
  int add_hidden_pk_ndb_record(NdbDictionary::Dictionary *dict);
  int add_index_ndb_record(NdbDictionary::Dictionary *dict,
                           KEY *key_info, uint index_no);
  int get_fk_data(THD *thd, Ndb *ndb);
  void release_fk_data(THD *thd);
  int create_fks(THD *thd, Ndb *ndb);
  int copy_fk_for_offline_alter(THD * thd, Ndb*, NdbDictionary::Table* _dsttab);
  int drop_fk_for_online_alter(THD*, Ndb*, NdbDictionary::Dictionary*,
                               const NdbDictionary::Table*);
  static int get_fk_data_for_truncate(NdbDictionary::Dictionary*,
                                      const NdbDictionary::Table*,
                                      Ndb_fk_list&);
  static int recreate_fk_for_truncate(THD*, Ndb*, const char*,
                                      Ndb_fk_list&);
  static bool drop_table_and_related(THD*, Ndb*, NdbDictionary::Dictionary*,
                                     const NdbDictionary::Table*,
                                     int drop_flags, bool skip_related);
  int check_default_values(const NdbDictionary::Table* ndbtab);
  int get_metadata(THD *thd, const char* path);
  void release_metadata(THD *thd, Ndb *ndb);
  NDB_INDEX_TYPE get_index_type(uint idx_no) const;
  NDB_INDEX_TYPE get_index_type_from_table(uint index_no) const;
  NDB_INDEX_TYPE get_index_type_from_key(uint index_no, KEY *key_info, 
                                         bool primary) const;
  bool has_null_in_unique_index(uint idx_no) const;
  bool check_index_fields_not_null(KEY *key_info) const;

  bool check_if_pushable(int type, //NdbQueryOperationDef::Type,
                         uint idx= MAX_KEY) const;
  bool check_is_pushed() const;
  int create_pushed_join(const NdbQueryParamValue* keyFieldParams=NULL,
                         uint paramCnt= 0);

  int ndb_pk_update_row(THD *thd, 
                        const uchar *old_data, uchar *new_data);
  int pk_read(const uchar *key, uint key_len, uchar *buf, uint32 *part_id);
  int ordered_index_scan(const key_range *start_key,
                         const key_range *end_key,
                         bool sorted, bool descending, uchar* buf,
                         part_id_range *part_spec);
  int unique_index_read(const uchar *key, uint key_len, 
                        uchar *buf);
  int full_table_scan(const KEY* key_info, 
                      const key_range *start_key,
                      const key_range *end_key,
                      uchar *buf);
  int flush_bulk_insert(bool allow_batch= FALSE);
  int ndb_write_row(uchar *record, bool primary_key_update,
                    bool batched_update);

  bool start_bulk_delete();
  int end_bulk_delete();
  int ndb_delete_row(const uchar *record, bool primary_key_update);

  int ndb_optimize_table(THD* thd, uint delay);

  int alter_frm(const char *file, class NDB_ALTER_DATA *alter_data);

  bool check_all_operations_for_error(NdbTransaction *trans,
                                      const NdbOperation *first,
                                      const NdbOperation *last,
                                      uint errcode);
  int peek_indexed_rows(const uchar *record, NDB_WRITE_OP write_op);
  int scan_handle_lock_tuple(NdbScanOperation *scanOp, NdbTransaction *trans);
  int fetch_next(NdbScanOperation* op);
  int fetch_next_pushed();
  int set_auto_inc(THD *thd, Field *field);
  int set_auto_inc_val(THD *thd, Uint64 value);
  int next_result(uchar *buf); 
  int close_scan();
  void unpack_record(uchar *dst_row, const uchar *src_row);

  void set_dbname(const char *pathname);
  void set_tabname(const char *pathname);

  const NdbDictionary::Column *get_hidden_key_column() {
    return m_table->getColumn(table_share->fields);
  }
  const NdbDictionary::Column *get_partition_id_column() {
    Uint32 index= table_share->fields + (table_share->primary_key == MAX_KEY);
    return m_table->getColumn(index);
  }

  uchar *get_buffer(Thd_ndb *thd_ndb, uint size);
  uchar *copy_row_to_buffer(Thd_ndb *thd_ndb, const uchar *record);

  int get_blob_values(const NdbOperation *ndb_op, uchar *dst_record,
                      const MY_BITMAP *bitmap);
  int set_blob_values(const NdbOperation *ndb_op, my_ptrdiff_t row_offset,
                      const MY_BITMAP *bitmap, uint *set_count, bool batch);
  friend int g_get_ndb_blobs_value(NdbBlob *ndb_blob, void *arg);
  void release_blobs_buffer();
  Uint32 setup_get_hidden_fields(NdbOperation::GetValueSpec gets[2]);
  void get_hidden_fields_keyop(NdbOperation::OperationOptions *options,
                               NdbOperation::GetValueSpec gets[2]);
  void get_hidden_fields_scan(NdbScanOperation::ScanOptions *options,
                              NdbOperation::GetValueSpec gets[2]);
  void get_read_set(bool use_cursor, uint idx);

  void eventSetAnyValue(THD *thd,
                        NdbOperation::OperationOptions *options) const;
  bool check_index_fields_in_write_set(uint keyno);

  int log_exclusive_read(const NdbRecord *key_rec,
                         const uchar *key,
                         uchar *buf,
                         Uint32 *ppartition_id);
  int scan_log_exclusive_read(NdbScanOperation*,
                              NdbTransaction*);
  const NdbOperation *pk_unique_index_read_key(uint idx, 
                                               const uchar *key, uchar *buf,
                                               NdbOperation::LockMode lm,
                                               Uint32 *ppartition_id);
  int pk_unique_index_read_key_pushed(uint idx, 
                                      const uchar *key,
                                      Uint32 *ppartition_id);

  int read_multi_range_fetch_next();
  
  int primary_key_cmp(const uchar * old_row, const uchar * new_row);
  void print_results();

  virtual void get_auto_increment(ulonglong offset, ulonglong increment,
                                  ulonglong nb_desired_values,
                                  ulonglong *first_value,
                                  ulonglong *nb_reserved_values);
  bool uses_blob_value(const MY_BITMAP *bitmap) const;

  int write_ndb_file(const char *name) const;

  int check_ndb_connection(THD* thd) const;

  void set_rec_per_key();
  void no_uncommitted_rows_execute_failure();
  void no_uncommitted_rows_update(int);

  /* Ordered index statistics v4 */
  int ndb_index_stat_query(uint inx,
                           const key_range *min_key,
                           const key_range *max_key,
                           NdbIndexStat::Stat& stat,
                           int from);
  int ndb_index_stat_get_rir(uint inx,
                             key_range *min_key,
                             key_range *max_key,
                             ha_rows *rows_out);
  int ndb_index_stat_set_rpk(uint inx);
  int ndb_index_stat_analyze(Ndb *ndb,
                             uint *inx_list,
                             uint inx_count);

  NdbTransaction *start_transaction_part_id(uint32 part_id, int &error);
  inline NdbTransaction *get_transaction_part_id(uint32 part_id, int &error)
  {
    if (m_thd_ndb->trans)
      return m_thd_ndb->trans;
    return start_transaction_part_id(part_id, error);
  }

  NdbTransaction *start_transaction(int &error);
  inline NdbTransaction *get_transaction(int &error)
  {
    if (m_thd_ndb->trans)
      return m_thd_ndb->trans;
    return start_transaction(error);
  }

  NdbTransaction *start_transaction_row(const NdbRecord *ndb_record,
                                        const uchar *record,
                                        int &error);
  NdbTransaction *start_transaction_key(uint index,
                                        const uchar *key_data,
                                        int &error);

  friend int check_completed_operations_pre_commit(Thd_ndb*,
                                                   NdbTransaction*,
                                                   const NdbOperation*,
                                                   uint *ignore_count);
  friend int ndbcluster_commit(handlerton *hton, THD *thd, bool all);
  int start_statement(THD *thd, Thd_ndb *thd_ndb, uint table_count);
  int init_handler_for_statement(THD *thd);
  /*
    Implementing Partition_handler API.
  */
  Partition_handler *get_partition_handler()
  { return static_cast<Partition_handler*>(this); }
  uint alter_flags(uint flags) const;
  void get_dynamic_partition_info(ha_statistics *stat_info,
                                  ha_checksum *check_sum,
                                  uint part_id);
  int get_default_num_partitions(HA_CREATE_INFO *info);
  bool get_num_parts(const char *name, uint *num_parts);
  void set_auto_partitions(partition_info *part_info);
  void set_part_info(partition_info *part_info, bool early);
  /* End of Partition_handler API */

  Thd_ndb *m_thd_ndb;
  NdbScanOperation *m_active_cursor;
  const NdbDictionary::Table *m_table;
  /*
    Normal NdbRecord for accessing rows, with all fields including hidden
    fields (hidden primary key, user-defined partitioning function value).
  */
  NdbRecord *m_ndb_record;
  /* NdbRecord for accessing tuple by hidden Uint64 primary key. */
  NdbRecord *m_ndb_hidden_key_record;

  /* Bitmap used for NdbRecord operation column mask. */
  MY_BITMAP m_bitmap;
  my_bitmap_map m_bitmap_buf[(NDB_MAX_ATTRIBUTES_IN_TABLE +
                              8*sizeof(my_bitmap_map) - 1) /
                             (8*sizeof(my_bitmap_map))]; // Buffer for m_bitmap
  /* Bitmap with bit set for all primary key columns. */
  MY_BITMAP *m_pk_bitmap_p;
  my_bitmap_map m_pk_bitmap_buf[(NDB_MAX_ATTRIBUTES_IN_TABLE +
                                 8*sizeof(my_bitmap_map) - 1) /
                                (8*sizeof(my_bitmap_map))]; // Buffer for m_pk_bitmap
  struct Ndb_local_table_statistics *m_table_info;
  struct Ndb_local_table_statistics m_table_info_instance;
  char m_dbname[FN_HEADLEN];
  //char m_schemaname[FN_HEADLEN];
  char m_tabname[FN_HEADLEN];
  THR_LOCK_DATA m_lock;
  bool m_lock_tuple;
  NDB_SHARE *m_share;
  NDB_INDEX_DATA  m_index[MAX_KEY];
  key_map btree_keys;
  static const size_t fk_root_block_size= 1024;
  MEM_ROOT m_fk_mem_root;
  struct Ndb_fk_data *m_fk_data;

  /*
    Pointer to row returned from scan nextResult().
  */
  union
  {
    const char *_m_next_row;
    const uchar *m_next_row;
  };
  /* For read_multi_range scans, the get_range_no() of current row. */
  int m_current_range_no;
  /* For multi range read, return from last mrr_funcs.next() call. */
  int m_range_res;
  MY_BITMAP **m_key_fields;
  // NdbRecAttr has no reference to blob
  NdbValue m_value[NDB_MAX_ATTRIBUTES_IN_TABLE];
  Uint64 m_ref;
  partition_info *m_part_info;
  uint32 m_part_id;
  bool m_user_defined_partitioning;
  bool m_use_partition_pruning;
  bool m_sorted;
  bool m_use_write;
  bool m_ignore_dup_key;
  bool m_has_unique_index;
  bool m_ignore_no_key;
  bool m_read_before_write_removal_possible;
  bool m_read_before_write_removal_used;
  ha_rows m_rows_updated;
  ha_rows m_rows_deleted;
  ha_rows m_rows_to_insert; // TODO: merge it with handler::estimation_rows_to_insert?
  ha_rows m_rows_inserted;
  ha_rows m_rows_changed;
  bool m_delete_cannot_batch;
  bool m_update_cannot_batch;
  uint m_bytes_per_write;
  bool m_skip_auto_increment;
  bool m_blobs_pending;
  bool m_slow_path;
  bool m_is_bulk_delete;

  /* State for setActiveHook() callback for reading blob data. */
  uint m_blob_counter;
  uint m_blob_expected_count_per_row;
  uchar *m_blob_destination_record;
  Uint64 m_blobs_row_total_size; /* Bytes needed for all blobs in current row */
  
  // memory for blobs in one tuple
  uchar *m_blobs_buffer;
  Uint64 m_blobs_buffer_size;
  uint m_dupkey;
  // set from thread variables at external lock
  ha_rows m_autoincrement_prefetch;

  // Joins pushed to NDB.
  const ndb_pushed_join
       *m_pushed_join_member;            // Pushed join def. I am member of
  int m_pushed_join_operation;           // Op. id. in above pushed join
  static const int PUSHED_ROOT= 0;       // Op. id. if I'm root

  bool m_disable_pushed_join;            // Pushed execution allowed?
  NdbQuery* m_active_query;              // Pushed query instance executing
  NdbQueryOperation* m_pushed_operation; // Pushed operation instance

  ha_ndbcluster_cond *m_cond;
  bool m_disable_multi_read;
  uchar *m_multi_range_result_ptr;
  NdbIndexScanOperation *m_multi_cursor;
  Ndb *get_ndb(THD *thd) const;

  int update_stats(THD *thd, bool do_read_stat,
                   uint part_id= ~(uint)0);
  int add_handler_to_open_tables(THD*, Thd_ndb*, ha_ndbcluster* handler);
  int rename_table_impl(THD* thd, Ndb* ndb,
                        const NdbDictionary::Table* orig_tab,
                        const char* from, const char* to,
                        const char* old_dbname, const char* old_tabname,
                        const char* new_dbname, const char* new_tabname,
                        bool real_rename,
                        const char* real_rename_db,
                        const char* real_rename_name,
                        bool real_rename_log_on_participant,
                        bool drop_events,
                        bool create_events,
                        bool commit_alter);
};

static const char ndbcluster_hton_name[]= "ndbcluster";
static const int ndbcluster_hton_name_length=sizeof(ndbcluster_hton_name)-1;
extern int ndbcluster_terminating;

#include "ndb_util_thread.h"
extern Ndb_util_thread ndb_util_thread;

int ndb_to_mysql_error(const NdbError *ndberr);

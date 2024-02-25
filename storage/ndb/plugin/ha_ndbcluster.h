/*
   Copyright (c) 2000, 2023, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef SQL_HA_NDBCLUSTER_INCLUDED
#define SQL_HA_NDBCLUSTER_INCLUDED

/*
  This file defines the NDB Cluster handler: the interface between MySQL and
  NDB Cluster
*/

#include <array>

#include "sql/partitioning/partition_handler.h"
#include "storage/ndb/include/kernel/ndb_limits.h"
#include "storage/ndb/include/ndbapi/NdbApi.hpp"
#include "storage/ndb/include/ndbapi/ndbapi_limits.h"
#include "storage/ndb/plugin/ha_ndbcluster_cond.h"
#include "storage/ndb/plugin/ndb_bitmap.h"
#include "storage/ndb/plugin/ndb_blobs_buffer.h"
#include "storage/ndb/plugin/ndb_conflict.h"
#include "storage/ndb/plugin/ndb_ndbapi_util.h"
#include "storage/ndb/plugin/ndb_share.h"
#include "storage/ndb/plugin/ndb_table_map.h"
#include "storage/ndb/plugin/ndb_thd_ndb.h"

class Ndb;
class NdbOperation;
class NdbTransaction;
class NdbRecAttr;
class NdbScanOperation;
class NdbIndexScanOperation;
class NdbBlob;
class NdbIndexStat;
class ha_ndbcluster_cond;
class NdbQuery;
class NdbQueryOperation;
class NdbQueryOperationTypeWrapper;
class NdbQueryParamValue;
class ndb_pushed_join;

enum NDB_INDEX_TYPE {
  UNDEFINED_INDEX = 0,
  PRIMARY_KEY_INDEX = 1,
  PRIMARY_KEY_ORDERED_INDEX = 2,
  UNIQUE_INDEX = 3,
  UNIQUE_ORDERED_INDEX = 4,
  ORDERED_INDEX = 5
};

struct NDB_INDEX_DATA {
  NDB_INDEX_TYPE type{UNDEFINED_INDEX};
  const NdbDictionary::Index *index{nullptr};
  const NdbDictionary::Index *unique_index{nullptr};

 private:
  // Map from MySQL key to NDB column order, this is necessary when the order of
  // the keys used by MySQL does not match the column order in NDB. The map
  // is only created if necessary, otherwise the default sequential column order
  // is used. The below table have both its primary key and unique key
  // specified in a different order than the table:
  //   CREATE TABLE t1 (
  //     a int, b int, c int, d int, e int,
  //     PRIMARY KEY(d,b,c),
  //     UNIQUE_KEY(e,d,c)
  //   ) engine = ndb;
  class Attrid_map {
    std::vector<unsigned char> m_ids;
    // Verify that vector's type is large enough to store "index of NDB column"
    // (currently 32 columns supported by NDB and 16 by MySQL)
    static_assert(std::numeric_limits<decltype(m_ids)::value_type>::max() >
                  NDB_MAX_NO_OF_ATTRIBUTES_IN_KEY);

   public:
    Attrid_map(const KEY *key_info, const NdbDictionary::Table *table);
    Attrid_map(const KEY *key_info, const NdbDictionary::Index *index);

    void fill_column_map(uint column_map[]) const;
  };
  const Attrid_map *attrid_map{nullptr};

 public:
  // Create Attrid_map for primary key, if required
  void create_attrid_map(const KEY *key_info,
                         const NdbDictionary::Table *table);
  // Create Attrid_map for unique key, if required
  void create_attrid_map(const KEY *key_info,
                         const NdbDictionary::Index *index);
  // Delete the Attrid_map
  void delete_attrid_map();
  // Fill column_map for given KEY
  void fill_column_map(const KEY *key_info, uint column_map[]) const;

  bool null_in_unique_index{false};
  // The keys and rows passed from MySQL Server are in different formats
  // depending on whether it's a key (using KEY_PART_INFO) or row (using Field),
  // thus different NdbRecord's need to be setup for each format.
  NdbRecord *ndb_record_key{nullptr};
  NdbRecord *ndb_unique_record_key{nullptr};
  NdbRecord *ndb_unique_record_row{nullptr};
};

int ndbcluster_commit(handlerton *, THD *thd, bool all);

class ha_ndbcluster : public handler, public Partition_handler {
  friend class ha_ndbcluster_cond;
  friend class ndb_pushed_builder_ctx;

 public:
  ha_ndbcluster(handlerton *hton, TABLE_SHARE *table);
  ~ha_ndbcluster() override;

  std::string explain_extra() const override;

  int open(const char *path, int mode, uint test_if_locked,
           const dd::Table *table_def) override;

  int close(void) override;

  int optimize(THD *thd, HA_CHECK_OPT *) override;

 private:
  int analyze_index();

 public:
  int analyze(THD *thd, HA_CHECK_OPT *) override;
  int write_row(uchar *buf) override;
  int update_row(const uchar *old_data, uchar *new_data) override;
  int delete_row(const uchar *buf) override;
  int index_init(uint index, bool sorted) override;
  int index_end() override;
  int index_read(uchar *buf, const uchar *key, uint key_len,
                 enum ha_rkey_function find_flag) override;
  int index_next(uchar *buf) override;
  int index_prev(uchar *buf) override;
  int index_first(uchar *buf) override;
  int index_last(uchar *buf) override;
  int index_next_same(uchar *buf, const uchar *key, uint keylen) override;
  int index_read_last(uchar *buf, const uchar *key, uint key_len) override;
  int rnd_init(bool scan) override;
  int rnd_end() override;
  int rnd_next(uchar *buf) override;
  int rnd_pos(uchar *buf, uchar *pos) override;
  void position(const uchar *record) override;
  int cmp_ref(const uchar *ref1, const uchar *ref2) const override;

 private:
  int read_range_first_to_buf(const key_range *start_key,
                              const key_range *end_key, bool eq_range,
                              bool sorted, uchar *buf);

 public:
  int read_range_first(const key_range *start_key, const key_range *end_key,
                       bool eq_range, bool sorted) override;
  int read_range_next() override;

  /**
   * Multi Range Read interface
   */
  int multi_range_read_init(RANGE_SEQ_IF *seq, void *seq_init_param,
                            uint n_ranges, uint mode,
                            HANDLER_BUFFER *buf) override;
  int multi_range_read_next(char **range_info) override;
  ha_rows multi_range_read_info_const(uint keyno, RANGE_SEQ_IF *seq,
                                      void *seq_init_param, uint n_ranges,
                                      uint *bufsz, uint *flags,
                                      Cost_estimate *cost) override;
  ha_rows multi_range_read_info(uint keyno, uint n_ranges, uint keys,
                                uint *bufsz, uint *flags,
                                Cost_estimate *cost) override;

  void append_create_info(String *packet) override;

  /* Get partition row type
  @param[in] table   partition table
  @param[in] part_id Id of partition for which row type to be retrieved
  @return Partition row type. */
  enum row_type get_partition_row_type(const dd::Table *table_def,
                                       uint part_id) override;

 private:
  bool choose_mrr_impl(uint keyno, uint n_ranges, ha_rows n_rows, uint *bufsz,
                       uint *flags, Cost_estimate *);

 private:
  uint first_running_range;
  uint first_range_in_batch;
  uint first_unstarted_range;

  int multi_range_start_retrievals(uint first_range);

 public:
  bool get_error_message(int error, String *buf) override;
  int records(ha_rows *num_rows) override;
  ha_rows estimate_rows_upper_bound() override { return HA_POS_ERROR; }
  int info(uint) override;
  uint32 calculate_key_hash_value(Field **field_array) override;
  bool start_read_removal(void) override;
  ha_rows end_read_removal(void) override;
  int extra(enum ha_extra_function operation) override;
  int reset() override;
  int external_lock(THD *thd, int lock_type) override;
  void unlock_row() override;
  int start_stmt(THD *thd, thr_lock_type) override;
  void update_create_info(HA_CREATE_INFO *create_info) override;

 private:
  int get_old_table_comment_items(THD *thd, bool *comment_items_shown,
                                  char *comment_str, unsigned comment_len);
  void update_comment_info(THD *thd, HA_CREATE_INFO *create_info,
                           const NdbDictionary::Table *tab);

 public:
  void print_error(int error, myf errflag) override;
  const char *table_type() const override;
  ulonglong table_flags(void) const override;
  ulong index_flags(uint idx, uint part, bool all_parts) const override;
  bool primary_key_is_clustered() const override;
  uint max_supported_keys() const override;
  uint max_supported_key_parts() const override;
  uint max_supported_key_length() const override;
  uint max_supported_key_part_length(
      HA_CREATE_INFO *create_info) const override;

  int rename_table(const char *from, const char *to,
                   const dd::Table *from_table_def,
                   dd::Table *to_table_def) override;
  int delete_table(const char *path, const dd::Table *table_def) override;
  bool upgrade_table(THD *thd, const char *db_name, const char *table_name,
                     dd::Table *dd_table) override;

  row_type get_real_row_type(const HA_CREATE_INFO *create_info) const override {
    DBUG_TRACE;
    // ROW_RORMAT=FIXED -> using FIXED
    if (create_info->row_type == ROW_TYPE_FIXED) return ROW_TYPE_FIXED;

    // All other values uses DYNAMIC
    return ROW_TYPE_DYNAMIC;
  }
  int create(const char *path, TABLE *table, HA_CREATE_INFO *info,
             dd::Table *table_def) override;
  int truncate(dd::Table *table_def) override;
  bool is_ignorable_error(int error) override {
    if (handler::is_ignorable_error(error) ||
        error == HA_ERR_NO_PARTITION_FOUND)
      return true;
    return false;
  }

  THR_LOCK_DATA **store_lock(THD *thd, THR_LOCK_DATA **to,
                             enum thr_lock_type lock_type) override;

  bool low_byte_first() const override;

  enum ha_key_alg get_default_index_algorithm() const override {
    /* NDB uses hash indexes only when explicitly requested. */
    return HA_KEY_ALG_BTREE;
  }
  bool is_index_algorithm_supported(enum ha_key_alg key_alg) const override {
    return key_alg == HA_KEY_ALG_BTREE || key_alg == HA_KEY_ALG_HASH;
  }

  double scan_time() override;

  double read_time(uint index, uint ranges, ha_rows rows) override;
  double page_read_cost(uint index, double rows) override;
  double worst_seek_times(double reads) override;

  ha_rows records_in_range(uint inx, key_range *min_key,
                           key_range *max_key) override;
  void start_bulk_insert(ha_rows rows) override;
  int end_bulk_insert() override;

  bool start_bulk_update() override;
  int bulk_update_row(const uchar *old_data, uchar *new_data,
                      uint *dup_key_found) override;
  int exec_bulk_update(uint *dup_key_found) override;
  void end_bulk_update() override;

 private:
  int ndb_update_row(const uchar *old_data, uchar *new_data,
                     int is_bulk_update);

 public:
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
    The table handler filters out rows using (pushed_cond1 AND pushed_cond2
    AND ... AND pushed_condN)
    or less restrictive condition, depending on handler's capabilities.

    handler->reset() call discard any pushed conditions.
    Calls to rnd_init/rnd_end, index_init/index_end etc do not affect
    any condition being pushed.
    The current implementation supports arbitrary AND/OR nested conditions
    with comparisons between columns and constants (including constant
    expressions and function calls) and the following comparison operators:
    =, !=, >, >=, <, <=, like, "not like", "is null", and "is not null".
    Negated conditions are supported by NOT which generate NAND/NOR groups.
  */
  const Item *cond_push(const Item *cond) override;

 public:
  /**
   * Generate the ScanFilters code for the condition(s) previously
   * accepted for cond_push'ing.
   * If code generation failed, the handler will evaluate the
   * condition for every row returned from NDB.
   */
  void generate_scan_filter(NdbInterpretedCode *code,
                            NdbScanOperation::ScanOptions *options);

  /**
   * Generate a ScanFilter using both the pushed condition AND
   * add equality predicates matching the 'key' supplied as
   * arguments.
   * @return 1 if generation of the key part failed.
   */
  int generate_scan_filter_with_key(NdbInterpretedCode *code,
                                    NdbScanOperation::ScanOptions *options,
                                    const KEY *key_info,
                                    const key_range *start_key,
                                    const key_range *end_key);
  /**
   * NDB support join- and condition pushdown, so we return
   * the NDB-handlerton to signal that
   * handlerton::push_to_engine() need to be called.
   */
  const handlerton *hton_supporting_engine_pushdown() override { return ht; }

  friend int ndbcluster_push_to_engine(THD *thd, AccessPath *, JOIN *);
  friend void accept_pushed_conditions(const TABLE *table, AccessPath *filter);

 private:
  bool maybe_pushable_join(const char *&reason) const;

 public:
  uint number_of_pushed_joins() const override;
  const TABLE *member_of_pushed_join() const override;
  const TABLE *parent_of_pushed_join() const override;
  table_map tables_in_pushed_join() const override;

  int index_read_pushed(uchar *buf, const uchar *key,
                        key_part_map keypart_map) override;

  int index_next_pushed(uchar *buf) override;

  /*
   * Internal to ha_ndbcluster, used by C functions
   */
  int ndb_err(NdbTransaction *);

  enum_alter_inplace_result check_if_supported_inplace_alter(
      TABLE *altered_table, Alter_inplace_info *ha_alter_info) override;

 private:
  static bool inplace_parse_comment(
      NdbDictionary::Table *new_tab, const NdbDictionary::Table *old_tab,
      HA_CREATE_INFO *create_info, THD *thd, Ndb *ndb,
      const char **unsupported_reason, bool &max_rows_changed,
      bool *partition_balance_in_comment = nullptr);

 public:
  bool prepare_inplace_alter_table(TABLE *altered_table,
                                   Alter_inplace_info *ha_alter_info,
                                   const dd::Table *old_table_def,
                                   dd::Table *new_table_def) override;

  bool inplace_alter_table(TABLE *altered_table,
                           Alter_inplace_info *ha_alter_info,
                           const dd::Table *old_table_def,
                           dd::Table *new_table_def) override;

  bool commit_inplace_alter_table(TABLE *altered_table,
                                  Alter_inplace_info *ha_alter_info,
                                  bool commit, const dd::Table *old_table_def,
                                  dd::Table *new_table_def) override;

  void notify_table_changed(Alter_inplace_info *alter_info) override;

 private:
  bool open_table_set_key_fields();
  void release_key_fields();
  void release_ndb_share();
  NDB_SHARE *open_share_before_schema_sync(THD *thd, const char *dbname,
                                           const char *tabname) const;
  void prepare_inplace__drop_index(uint index_num);

  enum_alter_inplace_result supported_inplace_field_change(Alter_inplace_info *,
                                                           Field *, Field *,
                                                           bool, bool) const;
  enum_alter_inplace_result supported_inplace_ndb_column_change(
      uint, TABLE *, Alter_inplace_info *, bool, bool) const;
  enum_alter_inplace_result supported_inplace_column_change(
      NdbDictionary::Dictionary *dict, TABLE *, uint, Field *,
      Alter_inplace_info *) const;
  enum_alter_inplace_result check_inplace_alter_supported(
      TABLE *altered_table, Alter_inplace_info *ha_alter_info);

  bool abort_inplace_alter_table(TABLE *altered_table,
                                 Alter_inplace_info *ha_alter_info);
  int prepare_conflict_detection(
      enum_conflicting_op_type op_type, const NdbRecord *key_rec,
      const NdbRecord *data_rec, const uchar *old_data, const uchar *new_data,
      const MY_BITMAP *write_set, NdbTransaction *trans,
      NdbInterpretedCode *code, NdbOperation::OperationOptions *options,
      bool &conflict_handled, bool &avoid_ndbapi_write);
  void setup_key_ref_for_ndb_record(const NdbRecord **key_rec,
                                    const uchar **key_row, const uchar *record,
                                    bool use_active_index);

  void check_read_before_write_removal();

  int prepare_inplace__add_index(THD *thd, KEY *key_info,
                                 uint num_of_keys) const;
  int create_index_in_NDB(THD *thd, const char *name, const KEY *key_info,
                          const NdbDictionary::Table *ndbtab,
                          bool unique) const;
  int create_index(THD *thd, const char *name, const KEY *key_info,
                   NDB_INDEX_TYPE idx_type,
                   const NdbDictionary::Table *ndbtab) const;
  // Index list management
  int create_indexes(THD *thd, TABLE *tab,
                     const NdbDictionary::Table *ndbtab) const;
  int open_indexes(NdbDictionary::Dictionary *dict);
  void release_indexes(NdbDictionary::Dictionary *dict, bool invalidate);
  int inplace__drop_index(NdbDictionary::Dictionary *dict, uint index_num);
  int open_index(NdbDictionary::Dictionary *dict, const KEY *key_info,
                 const char *key_name, uint index_no);
  int add_table_ndb_record(NdbDictionary::Dictionary *dict);
  int add_hidden_pk_ndb_record(NdbDictionary::Dictionary *dict);
  int open_index_ndb_record(NdbDictionary::Dictionary *dict,
                            const KEY *key_info, uint index_no);
  static int create_fks(THD *thd, Ndb *ndb, const char *dbname,
                        const char *tabname);
  static int copy_fk_for_offline_alter(THD *thd, Ndb *ndb, const char *dbname,
                                       const char *tabname);
  static int inplace__drop_fks(THD *thd, Ndb *ndb, const char *dbname,
                               const char *tabname);
  static int recreate_fk_for_truncate(THD *thd, Ndb *ndb, const char *dbname,
                                      const char *tabname,
                                      std::vector<NdbDictionary::ForeignKey> *);
  bool has_fk_dependency(NdbDictionary::Dictionary *dict,
                         const NdbDictionary::Column *) const;
#ifndef NDEBUG
  bool check_default_values() const;
#endif
  int get_metadata(Ndb *ndb, const char *dbname, const char *tabname,
                   const dd::Table *table_def);
  void release_metadata(NdbDictionary::Dictionary *dict,
                        bool invalidate_objects);
  NDB_INDEX_TYPE get_index_type(uint idx_no) const;
  NDB_INDEX_TYPE get_declared_index_type(uint index_num) const;
  bool has_null_in_unique_index(uint idx_no) const;

  bool check_if_pushable(int type,  // NdbQueryOperationDef::Type,
                         uint idx = MAX_KEY) const;
  bool check_is_pushed() const;
  int create_pushed_join(const NdbQueryParamValue *keyFieldParams = nullptr,
                         uint paramCnt = 0);

  int ndb_pk_update_row(const uchar *old_data, uchar *new_data);
  int pk_read(const uchar *key, uchar *buf, uint32 *part_id);
  int ordered_index_scan(const key_range *start_key, const key_range *end_key,
                         bool sorted, bool descending, uchar *buf,
                         part_id_range *part_spec);
  int unique_index_read(const uchar *key, uchar *buf);
  int full_table_scan(const KEY *key_info, const key_range *start_key,
                      const key_range *end_key, uchar *buf);
  int flush_bulk_insert(bool allow_batch = false);
  int ndb_write_row(uchar *record, bool primary_key_update,
                    bool batched_update);

  bool start_bulk_delete() override;
  int end_bulk_delete() override;
  int ndb_delete_row(const uchar *record, bool primary_key_update);

  int ndb_optimize_table(THD *thd, uint delay) const;

  bool peek_index_rows_check_index_fields_in_write_set(
      const KEY *key_info) const;
  bool peek_index_rows_check_ops(NdbTransaction *trans,
                                 const NdbOperation *first,
                                 const NdbOperation *last);

  enum NDB_WRITE_OP { NDB_INSERT = 0, NDB_UPDATE = 1, NDB_PK_UPDATE = 2 };

  int peek_indexed_rows(const uchar *record, NDB_WRITE_OP write_op);
  int scan_handle_lock_tuple(NdbScanOperation *scanOp, NdbTransaction *trans);
  int fetch_next(NdbScanOperation *op);
  int fetch_next_pushed();
  int set_auto_inc(Ndb *ndb, Field *field);
  int set_auto_inc_val(Ndb *ndb, Uint64 value) const;
  int next_result(uchar *buf);
  int close_scan();
  int unpack_record(uchar *dst_row, const uchar *src_row);
  int unpack_record_and_set_generated_fields(uchar *dst_row,
                                             const uchar *src_row);

  const NdbDictionary::Column *get_hidden_key_column() {
    return m_table->getColumn(m_table_map->get_hidden_key_column());
  }
  const NdbDictionary::Column *get_partition_id_column() {
    return m_table->getColumn(m_table_map->get_partition_id_column());
  }

  static int get_ndb_blobs_value_hook(NdbBlob *ndb_blob, void *arg);

  int get_blob_values(const NdbOperation *ndb_op, uchar *dst_record,
                      const MY_BITMAP *bitmap);
  int set_blob_values(const NdbOperation *ndb_op, ptrdiff_t row_offset,
                      const MY_BITMAP *bitmap, uint *set_count,
                      bool batch) const;
  void release_blobs_buffer();
  Uint32 setup_get_hidden_fields(NdbOperation::GetValueSpec gets[2]);
  void get_hidden_fields_keyop(NdbOperation::OperationOptions *options,
                               NdbOperation::GetValueSpec gets[2]);
  void get_hidden_fields_scan(NdbScanOperation::ScanOptions *options,
                              NdbOperation::GetValueSpec gets[2]);
  void get_read_set(bool use_cursor, uint idx);

  int log_exclusive_read(const NdbRecord *key_rec, const uchar *key, uchar *buf,
                         Uint32 *ppartition_id) const;
  int scan_log_exclusive_read(NdbScanOperation *, NdbTransaction *) const;
  const NdbOperation *pk_unique_index_read_key(uint idx, const uchar *key,
                                               uchar *buf,
                                               NdbOperation::LockMode lm,
                                               Uint32 *ppartition_id);
  int pk_unique_index_read_key_pushed(uint idx, const uchar *key);

  int read_multi_range_fetch_next();

  int primary_key_cmp(const uchar *old_row, const uchar *new_row);

  void get_auto_increment(ulonglong offset, ulonglong increment,
                          ulonglong number_of_desired_values,
                          ulonglong *first_value,
                          ulonglong *nb_reserved_values) override;
  bool uses_blob_value(const MY_BITMAP *bitmap) const;

  int check_ndb_connection(THD *thd) const;

  void set_rec_per_key(THD *thd);

  /* Ordered index statistics v4 */
  int ndb_index_stat_query(uint inx, const key_range *min_key,
                           const key_range *max_key, NdbIndexStat::Stat &stat,
                           int from);
  int ndb_index_stat_get_rir(uint inx, key_range *min_key, key_range *max_key,
                             ha_rows *rows_out);
  int ndb_index_stat_set_rpk(uint inx);
  int ndb_index_stat_analyze(uint *inx_list, uint inx_count);

  NdbTransaction *start_transaction_part_id(uint32 part_id, int &error);
  inline NdbTransaction *get_transaction_part_id(uint32 part_id, int &error) {
    if (m_thd_ndb->trans) return m_thd_ndb->trans;
    return start_transaction_part_id(part_id, error);
  }

  NdbTransaction *start_transaction(int &error);
  inline NdbTransaction *get_transaction(int &error) {
    if (m_thd_ndb->trans) return m_thd_ndb->trans;
    return start_transaction(error);
  }

  NdbTransaction *start_transaction_row(const NdbRecord *ndb_record,
                                        const uchar *record, int &error);
  NdbTransaction *start_transaction_key(uint index_num, const uchar *key_data,
                                        int &error);

  friend int ndbcluster_commit(handlerton *, THD *thd, bool all);

  int start_statement(THD *thd, Thd_ndb *thd_ndb, uint table_count);
  /*
    Implementing Partition_handler API.
  */
  Partition_handler *get_partition_handler() override {
    return static_cast<Partition_handler *>(this);
  }
  uint alter_flags(uint flags) const override;
  void get_dynamic_partition_info(ha_statistics *stat_info,
                                  ha_checksum *checksum, uint part_id) override;
  int get_default_num_partitions(HA_CREATE_INFO *info) override;
  bool get_num_parts(const char *name, uint *num_parts) override;
  void set_auto_partitions(partition_info *part_info) override;
  void set_part_info(partition_info *part_info, bool early) override;
  /* End of Partition_handler API */

  Thd_ndb *m_thd_ndb;
  NdbScanOperation *m_active_cursor;
  // NDB table definition
  const NdbDictionary::Table *m_table{nullptr};
  // Mapping from MySQL table to NDB table
  Ndb_table_map *m_table_map{nullptr};
  /*
    Normal NdbRecord for accessing rows, with all fields including hidden
    fields (hidden primary key, user-defined partitioning function value).
  */
  NdbRecord *m_ndb_record;
  /* NdbRecord for accessing tuple by hidden Uint64 primary key. */
  NdbRecord *m_ndb_hidden_key_record;

  /* Bitmap used for NdbRecord operation column mask. */
  MY_BITMAP m_bitmap;
  Ndb_bitmap_buf<NDB_MAX_ATTRIBUTES_IN_TABLE> m_bitmap_buf;

  // Pointer to bitmap for the primary key columns (the actual bitmap is in
  // m_key_fields array that have one bitmap for each index of the table)
  MY_BITMAP *m_pk_bitmap_p;
  // Since all NDB table have primary key, the bitmap buffer is preallocated
  Ndb_bitmap_buf<NDB_MAX_ATTRIBUTES_IN_TABLE> m_pk_bitmap_buf;

  // Pointer to table stats for transaction
  Thd_ndb::Trans_tables::Stats *m_trans_table_stats{nullptr};

  THR_LOCK_DATA m_lock;
  bool m_lock_tuple;
  NDB_SHARE *m_share{nullptr};

  std::array<NDB_INDEX_DATA, MAX_INDEXES> m_index;
  // Cached metadata variable, indicating if the open table have any unique
  // indexes. Used as a quick optimization to avoid looping the list of indexes.
  bool m_has_unique_index{false};

  /*
    Pointer to row returned from scan nextResult().
  */
  union {
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
  bool m_ignore_no_key;
  bool m_read_before_write_removal_possible;
  bool m_read_before_write_removal_used;
  ha_rows m_rows_updated;
  ha_rows m_rows_deleted;
  ha_rows m_rows_to_insert;  // TODO: merge it with
                             // handler::estimation_rows_to_insert?
  bool m_delete_cannot_batch;
  bool m_update_cannot_batch;
  // Approximate number of bytes that need to be sent to NDB when updating a row
  // of this table, used for determining when batch should be flushed.
  uint m_bytes_per_write;
  bool m_skip_auto_increment;
  bool m_is_bulk_delete;

  class Copying_alter {
    Uint64 m_saved_commit_count;

   public:
    // Save the commit count for source table during copying ALTER,
    // returns 0 on success, handler error otherwise
    int save_commit_count(Thd_ndb *thd_ndb, const NdbDictionary::Table *ndbtab);
    // Check commit count for source table during copying ALTER,
    // returns 0 on success, handler error otherwise
    int check_saved_commit_count(Thd_ndb *thd_ndb,
                                 const NdbDictionary::Table *ndbtab) const;
  } copying_alter;

  /* State for setActiveHook() callback for reading blob data. */
  uint m_blob_counter;
  uint m_blob_expected_count_per_row;
  uchar *m_blob_destination_record;
  Uint64 m_blobs_row_total_size; /* Bytes needed for all blobs in current row */

  Ndb_blobs_buffer m_blobs_buffer;

  uint m_dupkey;
  // set from thread variables at external lock
  ha_rows m_autoincrement_prefetch;

  // Joins pushed to NDB.
  const ndb_pushed_join
      *m_pushed_join_member;         // Pushed join def. I am member of
  int m_pushed_join_operation;       // Op. id. in above pushed join
  static const int PUSHED_ROOT = 0;  // Op. id. if I'm root

  bool m_disable_pushed_join;             // Pushed execution allowed?
  NdbQuery *m_active_query;               // Pushed query instance executing
  NdbQueryOperation *m_pushed_operation;  // Pushed operation instance

  /* In case we failed to push a 'pushed_cond', the handler will evaluate it */
  ha_ndbcluster_cond m_cond;
  bool m_disable_multi_read;
  uchar *m_multi_range_result_ptr;
  NdbIndexScanOperation *m_multi_cursor;

  int update_stats(THD *thd, bool do_read_stat);
};

int ndb_to_mysql_error(const NdbError *ndberr);

#endif

/*****************************************************************************

Copyright (c) 2000, 2023, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is also distributed with certain software (including but not
limited to OpenSSL) that is licensed under separate terms, as designated in a
particular file or component or in included license documentation. The authors
of MySQL hereby grant you an additional permission to link the program and
your derivative works with the separately licensed software that they have
included with MySQL.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

#ifndef ha_innodb_h
#define ha_innodb_h

/* The InnoDB handler: the interface between MySQL and InnoDB. */

#include <assert.h>
#include <sys/types.h>
#include "create_field.h"
#include "field.h"
#include "handler.h"
#include "mysql/components/services/clone_protocol_service.h"

#include "row0pread-adapter.h"
#include "row0pread-histogram.h"
#include "trx0trx.h"

/** "GEN_CLUST_INDEX" is the name reserved for InnoDB default
system clustered index when there is no primary key. */
extern const char innobase_index_reserve_name[];

/** Clone protocol service. */
extern SERVICE_TYPE(clone_protocol) * clone_protocol_svc;

/* Structure defines translation table between mysql index and InnoDB
index structures */
struct innodb_idx_translate_t {
  ulint index_count; /*!< number of valid index entries
                     in the index_mapping array */

  ulint array_size; /*!< array size of index_mapping */

  dict_index_t **index_mapping; /*!< index pointer array directly
                                maps to index in InnoDB from MySQL
                                array index */
};

/** InnoDB table share */
struct INNOBASE_SHARE {
  const char *table_name; /*!< InnoDB table name */
  uint use_count;         /*!< reference count,
                          incremented in get_share()
                          and decremented in
                          free_share() */
  void *table_name_hash;
  /*!< hash table chain node */
  innodb_idx_translate_t idx_trans_tbl; /*!< index translation table between
                                        MySQL and InnoDB */
};

/** Prebuilt structures in an InnoDB table handle used within MySQL */
struct row_prebuilt_t;

namespace dd {
namespace cache {
class Dictionary_client;
}
}  // namespace dd

/** The class defining a handle to an InnoDB table */
class ha_innobase : public handler {
 public:
  ha_innobase(handlerton *hton, TABLE_SHARE *table_arg);
  ~ha_innobase() override = default;

  row_type get_real_row_type(const HA_CREATE_INFO *create_info) const override;

  const char *table_type() const override;

  enum ha_key_alg get_default_index_algorithm() const override {
    return HA_KEY_ALG_BTREE;
  }

  /** Check if SE supports specific key algorithm. */
  bool is_index_algorithm_supported(enum ha_key_alg key_alg) const override {
    /* This method is never used for FULLTEXT or SPATIAL keys.
    We rely on handler::ha_table_flags() to check if such keys
    are supported. */
    assert(key_alg != HA_KEY_ALG_FULLTEXT && key_alg != HA_KEY_ALG_RTREE);
    return key_alg == HA_KEY_ALG_BTREE;
  }

  Table_flags table_flags() const override;

  ulong index_flags(uint idx, uint part, bool all_parts) const override;

  uint max_supported_keys() const override;

  uint max_supported_key_length() const override;

  uint max_supported_key_part_length(
      HA_CREATE_INFO *create_info) const override;

  int open(const char *name, int, uint open_flags,
           const dd::Table *table_def) override;

  handler *clone(const char *name, MEM_ROOT *mem_root) override;

  int close(void) override;

  double scan_time() override;

  double read_time(uint index, uint ranges, ha_rows rows) override;

  longlong get_memory_buffer_size() const override;

  int write_row(uchar *buf) override;

  int update_row(const uchar *old_data, uchar *new_data) override;

  int delete_row(const uchar *buf) override;

  /** Delete all rows from the table.
  @retval HA_ERR_WRONG_COMMAND if the table is transactional
  @retval 0 on success */
  int delete_all_rows() override;

  bool was_semi_consistent_read() override;

  void try_semi_consistent_read(bool yes) override;

  void unlock_row() override;

  int index_init(uint index, bool sorted) override;

  int index_end() override;

  int index_read(uchar *buf, const uchar *key, uint key_len,
                 ha_rkey_function find_flag) override;

  int index_read_last(uchar *buf, const uchar *key, uint key_len) override;

  int index_next(uchar *buf) override;

  int index_next_same(uchar *buf, const uchar *key, uint keylen) override;

  int index_prev(uchar *buf) override;

  int index_first(uchar *buf) override;

  int index_last(uchar *buf) override;

  int read_range_first(const key_range *start_key, const key_range *end_key,
                       bool eq_range_arg, bool sorted) override;

  int read_range_next() override;

  int rnd_init(bool scan) override;

  int rnd_end() override;

  int rnd_next(uchar *buf) override;

  int rnd_pos(uchar *buf, uchar *pos) override;

  int ft_init() override;

  void ft_end();

  FT_INFO *ft_init_ext(uint flags, uint inx, String *key) override;

  FT_INFO *ft_init_ext_with_hints(uint inx, String *key,
                                  Ft_hints *hints) override;

  int ft_read(uchar *buf) override;

  void position(const uchar *record) override;

  int info(uint) override;

  int enable_indexes(uint mode) override;

  int disable_indexes(uint mode) override;

  int analyze(THD *thd, HA_CHECK_OPT *check_opt) override;

  int optimize(THD *thd, HA_CHECK_OPT *check_opt) override;

  int discard_or_import_tablespace(bool discard, dd::Table *table_def) override;

  int extra(ha_extra_function operation) override;

  int reset() override;

  int external_lock(THD *thd, int lock_type) override;

  /** Initialize sampling.
  @param[out] scan_ctx  A scan context created by this method that has to be
  used in sample_next
  @param[in]  sampling_percentage percentage of records that need to be sampled
  @param[in]  sampling_seed       random seed that the random generator will use
  @param[in]  sampling_method     sampling method to be used; currently only
  SYSTEM sampling is supported
  @param[in]  tablesample         true if the sampling is for tablesample
  @return 0 for success, else one of the HA_xxx values in case of error. */
  int sample_init(void *&scan_ctx, double sampling_percentage,
                  int sampling_seed, enum_sampling_method sampling_method,
                  const bool tablesample) override;

  /** Get the next record for sampling.
  @param[in]  scan_ctx  Scan context of the sampling
  @param[in]  buf       buffer to place the read record
  @return 0 for success, else one of the HA_xxx values in case of error. */
  int sample_next(void *scan_ctx, uchar *buf) override;

  /** End sampling.
  @param[in] scan_ctx  Scan context of the sampling
  @return 0 for success, else one of the HA_xxx values in case of error. */
  int sample_end(void *scan_ctx) override;

  /** MySQL calls this function at the start of each SQL statement
  inside LOCK TABLES. Inside LOCK TABLES the "::external_lock" method
  does not work to mark SQL statement borders. Note also a special case:
  if a temporary table is created inside LOCK TABLES, MySQL has not
  called external_lock() at all on that table.
  MySQL-5.0 also calls this before each statement in an execution of a
  stored procedure. To make the execution more deterministic for
  binlogging, MySQL-5.0 locks all tables involved in a stored procedure
  with full explicit table locks (thd_in_lock_tables(thd) holds in
  store_lock()) before executing the procedure.
  @param[in]    thd             handle to the user thread
  @param[in]    lock_type       lock type
  @return 0 or error code */
  int start_stmt(THD *thd, thr_lock_type lock_type) override;

  void position(uchar *record);

  int records(ha_rows *num_rows) override;

  int records_from_index(ha_rows *num_rows, uint) override {
    /* Force use of cluster index until we implement sec index parallel scan. */
    return ha_innobase::records(num_rows);
  }

  ha_rows records_in_range(uint inx, key_range *min_key,
                           key_range *max_key) override;

  ha_rows estimate_rows_upper_bound() override;

  void update_create_info(HA_CREATE_INFO *create_info) override;

  /** Get storage-engine private data for a data dictionary table.
  @param[in,out]        dd_table        data dictionary table definition
  @param                reset           reset counters
  @retval               true            an error occurred
  @retval               false           success */
  bool get_se_private_data(dd::Table *dd_table, bool reset) override;

  /** Add hidden columns and indexes to an InnoDB table definition.
  @param[in,out]        dd_table        data dictionary cache object
  @return       error number
  @retval       0 on success */
  int get_extra_columns_and_keys(const HA_CREATE_INFO *,
                                 const List<Create_field> *, const KEY *, uint,
                                 dd::Table *dd_table) override;

  /** Set Engine specific data to dd::Table object for upgrade.
  @param[in,out]  thd           thread handle
  @param[in]    db_name         database name
  @param[in]    table_name      table name
  @param[in,out]        dd_table        data dictionary cache object
  @return 0 on success, non-zero on failure */
  bool upgrade_table(THD *thd, const char *db_name, const char *table_name,
                     dd::Table *dd_table) override;

  /** Create an InnoDB table.
  @param[in]    name            table name in filename-safe encoding
  @param[in]    form            table structure
  @param[in]    create_info     more information on the table
  @param[in,out]        table_def       dd::Table describing table to be
  created. Can be adjusted by SE, the changes will be saved into data-dictionary
  at statement commit time.
  @return error number
  @retval 0 on success */
  int create(const char *name, TABLE *form, HA_CREATE_INFO *create_info,
             dd::Table *table_def) override;

  /** Drop a table.
  @param[in]    name            table name
  @param[in]    table_def       dd::Table describing table to
  be dropped
  @return       error number
  @retval 0 on success */
  int delete_table(const char *name, const dd::Table *table_def) override;

 protected:
  /** Drop a table.
  @param[in]    name            table name
  @param[in]    table_def       dd::Table describing table to
  be dropped
  @param[in]    sqlcom  type of operation that the DROP is part of
  @return       error number
  @retval 0 on success */
  int delete_table(const char *name, const dd::Table *table_def,
                   enum enum_sql_command sqlcom);

 public:
  int rename_table(const char *from, const char *to,
                   const dd::Table *from_table, dd::Table *to_table) override;

  int check(THD *thd, HA_CHECK_OPT *check_opt) override;

  uint lock_count(void) const override;

  THR_LOCK_DATA **store_lock(THD *thd, THR_LOCK_DATA **to,
                             thr_lock_type lock_type) override;

  void init_table_handle_for_HANDLER() override;

  void get_auto_increment(ulonglong offset, ulonglong increment,
                          ulonglong nb_desired_values, ulonglong *first_value,
                          ulonglong *nb_reserved_values) override;

  /** Do cleanup for auto increment calculation. */
  void release_auto_increment() override;

  bool get_error_message(int error, String *buf) override;

  bool get_foreign_dup_key(char *, uint, char *, uint) override;

  bool primary_key_is_clustered() const override;

  int cmp_ref(const uchar *ref1, const uchar *ref2) const override;

  /** @defgroup ALTER_TABLE_INTERFACE On-line ALTER TABLE interface
  @see handler0alter.cc
  @{ */

  /** Check if InnoDB supports a particular alter table in-place
  @param altered_table TABLE object for new version of table.
  @param ha_alter_info Structure describing changes to be done
  by ALTER TABLE and holding data used during in-place alter.

  @retval HA_ALTER_INPLACE_NOT_SUPPORTED Not supported
  @retval HA_ALTER_INPLACE_NO_LOCK Supported
  @retval HA_ALTER_INPLACE_SHARED_LOCK_AFTER_PREPARE Supported, but requires
  lock during main phase and exclusive lock during prepare phase.
  @retval HA_ALTER_INPLACE_NO_LOCK_AFTER_PREPARE Supported, prepare phase
  requires exclusive lock (any transactions that have accessed the table
  must commit or roll back first, and no transactions can access the table
  while prepare_inplace_alter_table() is executing)
  */
  enum_alter_inplace_result check_if_supported_inplace_alter(
      TABLE *altered_table, Alter_inplace_info *ha_alter_info) override;

  /** Allows InnoDB to update internal structures with concurrent
  writes blocked (provided that check_if_supported_inplace_alter()
  did not return HA_ALTER_INPLACE_NO_LOCK).
  This will be invoked before inplace_alter_table().
  @param[in]    altered_table   TABLE object for new version of table.
  @param[in,out]        ha_alter_info   Structure describing changes to be done
  by ALTER TABLE and holding data used during in-place alter.
  @param[in]    old_dd_tab      dd::Table object describing old version
  of the table.
  @param[in,out]        new_dd_tab      dd::Table object for the new version of
  the table. Can be adjusted by this call. Changes to the table definition will
  be persisted in the data-dictionary at statement commit time.
  @retval true Failure
  @retval false Success
  */
  bool prepare_inplace_alter_table(TABLE *altered_table,
                                   Alter_inplace_info *ha_alter_info,
                                   const dd::Table *old_dd_tab,
                                   dd::Table *new_dd_tab) override;

  /** Alter the table structure in-place with operations
  specified using HA_ALTER_FLAGS and Alter_inplace_information.
  The level of concurrency allowed during this operation depends
  on the return value from check_if_supported_inplace_alter().
  @param[in]    altered_table   TABLE object for new version of table.
  @param[in,out]        ha_alter_info   Structure describing changes to be done
  by ALTER TABLE and holding data used during in-place alter.
  @param[in]     old_dd_tab     dd::Table object describing old version
  of the table.
  @param[in,out]         new_dd_tab     dd::Table object for the new version of
  the table. Can be adjusted by this call. Changes to the table definition will
  be persisted in the data-dictionary at statement commit time.
  @retval true Failure
  @retval false Success
  */
  bool inplace_alter_table(TABLE *altered_table,
                           Alter_inplace_info *ha_alter_info,
                           const dd::Table *old_dd_tab,
                           dd::Table *new_dd_tab) override;

  /** Commit or rollback the changes made during
  prepare_inplace_alter_table() and inplace_alter_table() inside
  the storage engine. Note that the allowed level of concurrency
  during this operation will be the same as for
  inplace_alter_table() and thus might be higher than during
  prepare_inplace_alter_table(). (E.g concurrent writes were
  blocked during prepare, but might not be during commit).
  @param[in]    altered_table   TABLE object for new version of table.
  @param[in,out]        ha_alter_info   Structure describing changes to be done
  by ALTER TABLE and holding data used during in-place alter.
  @param[in]    commit          True to commit or false to rollback.
  @param[in]    old_dd_tab      dd::Table object representing old
  version of the table
  @param[in,out]        new_dd_tab      dd::Table object representing new
  version of the table. Can be adjusted by this call. Changes to the table
  definition will be persisted in the data-dictionary at statement
  commit time.
  @retval       true Failure
  @retval       false Success */
  bool commit_inplace_alter_table(TABLE *altered_table,
                                  Alter_inplace_info *ha_alter_info,
                                  bool commit, const dd::Table *old_dd_tab,
                                  dd::Table *new_dd_tab) override;
  /** @} */

  using Reader = Parallel_reader_adapter;

  /** Initializes a parallel scan. It creates a scan_ctx that has to
  be used across all parallel_scan methods. Also, gets the number of threads
  that would be spawned for parallel scan.
  @param[out]   scan_ctx              A scan context created by this method
                                      that has to be used in
                                      parallel_scan
  @param[out]   num_threads           Number of threads to be spawned
  @param[in]    use_reserved_threads  true if reserved threads are to be used
                                      if we exhaust the max cap of number of
                                      parallel read threads that can be
                                      spawned at a time
  @return error code
  @retval 0 on success */
  int parallel_scan_init(void *&scan_ctx, size_t *num_threads,
                         bool use_reserved_threads) override;

  /** Start parallel read of InnoDB records.
  @param[in]  scan_ctx          A scan context created by parallel_scan_init
  @param[in]  thread_ctxs       Context for each of the spawned threads
  @param[in]  init_fn           Callback called by each parallel load
                                thread at the beginning of the parallel load.
  @param[in]  load_fn           Callback called by each parallel load
                                thread when processing of rows is required.
  @param[in]  end_fn            Callback called by each parallel load
                                thread when processing of rows has ended.
  @return error code
  @retval 0 on success */
  int parallel_scan(void *scan_ctx, void **thread_ctxs, Reader::Init_fn init_fn,
                    Reader::Load_fn load_fn, Reader::End_fn end_fn) override;

  /** End of the parallel scan.
  @param[in]      scan_ctx      A scan context created by parallel_scan_init. */
  void parallel_scan_end(void *scan_ctx) override;

  bool check_if_incompatible_data(HA_CREATE_INFO *info,
                                  uint table_changes) override;

 private:
  /** @name Multi Range Read interface
  @{ */

  /** Initialize multi range read @see DsMrr_impl::dsmrr_init */
  int multi_range_read_init(RANGE_SEQ_IF *seq, void *seq_init_param,
                            uint n_ranges, uint mode,
                            HANDLER_BUFFER *buf) override;

  /** Process next multi range read @see DsMrr_impl::dsmrr_next */
  int multi_range_read_next(char **range_info) override;

  /** Initialize multi range read and get information.
  @see ha_myisam::multi_range_read_info_const
  @see DsMrr_impl::dsmrr_info_const */
  ha_rows multi_range_read_info_const(uint keyno, RANGE_SEQ_IF *seq,
                                      void *seq_init_param, uint n_ranges,
                                      uint *bufsz, uint *flags,
                                      Cost_estimate *cost) override;

  /** Initialize multi range read and get information.
  @see DsMrr_impl::dsmrr_info */
  ha_rows multi_range_read_info(uint keyno, uint n_ranges, uint keys,
                                uint *bufsz, uint *flags,
                                Cost_estimate *cost) override;

  /** Attempt to push down an index condition.
  @param[in] keyno MySQL key number
  @param[in] idx_cond Index condition to be checked
  @return idx_cond if pushed; NULL if not pushed */
  Item *idx_cond_push(uint keyno, Item *idx_cond) override;
  /** @} */

 private:
  void update_thd();

  int change_active_index(uint keynr);

  dberr_t innobase_lock_autoinc();

  dberr_t innobase_set_max_autoinc(ulonglong auto_inc);

  dberr_t innobase_get_autoinc(ulonglong *value);

  void innobase_initialize_autoinc();

  /** Resets a query execution 'template'.
  @see build_template() */
  void reset_template();

  /** Write Row Interface optimized for Intrinsic table. */
  int intrinsic_table_write_row(uchar *record);

  /** Find out if a Record_buffer is wanted by this handler, and what is
  the maximum buffer size the handler wants.

  @param[out] max_rows gets set to the maximum number of records to
              allocate space for in the buffer
  @retval true   if the handler wants a buffer
  @retval false  if the handler does not want a buffer */
  bool is_record_buffer_wanted(ha_rows *const max_rows) const override;

  /** TRUNCATE an InnoDB table.
  @param[in]            name            table name
  @param[in]            form            table definition
  @param[in,out]        table_def       dd::Table describing table to be
  truncated. Can be adjusted by SE, the changes will be saved into
  the data-dictionary at statement commit time.
  @return       error number
  @retval 0 on success */
  int truncate_impl(const char *name, TABLE *form, dd::Table *table_def);

 protected:
  /** Enter InnoDB engine after checking max allowed threads.
  @return mysql error code. */
  int srv_concurrency_enter();

  /** Leave Innodb, if no more tickets are left */
  void srv_concurrency_exit();

  void update_thd(THD *thd);

  int general_fetch(uchar *buf, uint direction, uint match_mode);

  virtual dict_index_t *innobase_get_index(uint keynr);

  /** Builds a 'template' to the m_prebuilt struct. The template is used in fast
  retrieval of just those column values MySQL needs in its processing.
  @param[in] whole_row true if access is needed to a whole row, false if
  accessing individual fields is enough */
  void build_template(bool whole_row);

  /** Returns statistics information of the table to the MySQL interpreter, in
  various fields of the handle object.
  @param[in]    flag            what information is requested
  @param[in]    is_analyze      True if called from "::analyze()".
  @return HA_ERR_* error code or 0 */
  virtual int info_low(uint flag, bool is_analyze);

  /**
  MySQL calls this method at the end of each statement. This method
  exists for readability only, called from reset(). The name reset()
  doesn't give any clue that it is called at the end of a statement. */
  int end_stmt();

  /** Implementation of prepare_inplace_alter_table()
  @tparam               Table           dd::Table or dd::Partition
  @param[in]    altered_table   TABLE object for new version of table.
  @param[in,out]        ha_alter_info   Structure describing changes to be done
                                  by ALTER TABLE and holding data used
                                  during in-place alter.
  @param[in]    old_dd_tab      dd::Table object representing old
                                  version of the table
  @param[in,out]        new_dd_tab      dd::Table object representing new
                                  version of the table
  @retval       true Failure
  @retval       false Success */
  template <typename Table>
  bool prepare_inplace_alter_table_impl(TABLE *altered_table,
                                        Alter_inplace_info *ha_alter_info,
                                        const Table *old_dd_tab,
                                        Table *new_dd_tab);

  /** Implementation of inplace_alter_table()
  @tparam               Table           dd::Table or dd::Partition
  @param[in]    altered_table   TABLE object for new version of table.
  @param[in,out]        ha_alter_info   Structure describing changes to be done
                                  by ALTER TABLE and holding data used
                                  during in-place alter.
  the table. Can be adjusted by this call. Changes to the table definition will
  be persisted in the data-dictionary at statement commit time.
  @retval true Failure
  @retval false Success
  */
  template <typename Table>
  bool inplace_alter_table_impl(TABLE *altered_table,
                                Alter_inplace_info *ha_alter_info);

  /** Implementation of commit_inplace_alter_table()
  @tparam               Table           dd::Table or dd::Partition
  @param[in]    altered_table   TABLE object for new version of table.
  @param[in,out]        ha_alter_info   Structure describing changes to be done
                                  by ALTER TABLE and holding data used
                                  during in-place alter.
  @param[in]    commit          True to commit or false to rollback.
  @param[in,out]        new_dd_tab      Table object for the new version of the
                                  table. Can be adjusted by this call.
                                  Changes to the table definition
                                  will be persisted in the data-dictionary
                                  at statement version of it.
  @retval       true Failure
  @retval       false Success */
  template <typename Table>
  bool commit_inplace_alter_table_impl(TABLE *altered_table,
                                       Alter_inplace_info *ha_alter_info,
                                       bool commit, Table *new_dd_tab);

  /**
    Return max limits for a single set of multi-valued keys

    @param[out]  num_keys      number of keys to store
    @param[out]  keys_length   total length of keys, bytes
  */
  void mv_key_capacity(uint *num_keys, size_t *keys_length) const override;

  /** Can reuse the template. Mainly used for partition.
  @retval       true Can reuse the mysql_template */
  virtual bool can_reuse_mysql_template() const { return false; }

  /** The multi range read session object */
  DsMrr_impl m_ds_mrr;

  /** Save CPU time with prebuilt/cached data structures */
  row_prebuilt_t *m_prebuilt;

  /** Thread handle of the user currently using the handler;
  this is set in external_lock function */
  THD *m_user_thd;

  /** information for MySQL table locking */
  INNOBASE_SHARE *m_share;

  /** buffer used in updates */
  uchar *m_upd_buf;

  /** the size of upd_buf in bytes */
  ulint m_upd_buf_size;

  /** Flags that specify the handler instance (table) capability. */
  Table_flags m_int_table_flags;

  /** this is set to 1 when we are starting a table scan but have
  not yet fetched any row, else false */
  bool m_start_of_scan;

  /*!< match mode of the latest search: ROW_SEL_EXACT,
  ROW_SEL_EXACT_PREFIX, or undefined */
  uint m_last_match_mode{0};

  /** this field is used to remember the original select_lock_type that
  was decided in ha_innodb.cc,":: store_lock()", "::external_lock()",
  etc. */
  ulint m_stored_select_lock_type;

  /** If mysql has locked with external_lock() */
  bool m_mysql_has_locked;
};

struct trx_t;

extern const struct _ft_vft ft_vft_result;

/** Return the number of read threads for this session.
@param[in]      thd       Session instance, or nullptr to query the global
                          innodb_parallel_read_threads value. */
ulong thd_parallel_read_threads(THD *thd);

/** Structure Returned by ha_innobase::ft_init_ext() */
typedef struct new_ft_info {
  struct _ft_vft *please;
  struct _ft_vft_ext *could_you;
  row_prebuilt_t *ft_prebuilt;
  fts_result_t *ft_result;
} NEW_FT_INFO;

/** Allocates an InnoDB transaction for a MySQL handler object for DML.
@param[in]      hton    Innobase handlerton.
@param[in]      thd     MySQL thd (connection) object.
@param[in]      trx     transaction to register. */
void innobase_register_trx(handlerton *hton, THD *thd, trx_t *trx);

/**
Allocates an InnoDB transaction for a MySQL handler object.
@return InnoDB transaction handle */
trx_t *innobase_trx_allocate(THD *thd); /*!< in: user thread handle */

/** Maps a MySQL trx isolation level code to the InnoDB isolation level code.
@param[in]  iso MySQL isolation level code
@return InnoDB isolation level */
trx_t::isolation_level_t innobase_trx_map_isolation_level(
    enum_tx_isolation iso);

/** Match index columns between MySQL and InnoDB.
This function checks whether the index column information
is consistent between KEY info from mysql and that from innodb index.
@param[in]      key_info        Index info from mysql
@param[in]      index_info      Index info from InnoDB
@return true if all column types match. */
bool innobase_match_index_columns(const KEY *key_info,
                                  const dict_index_t *index_info);

/** This function checks each index name for a table against reserved
 system default primary index name 'GEN_CLUST_INDEX'. If a name
 matches, this function pushes an warning message to the client,
 and returns true.
 @return true if the index name matches the reserved name */
[[nodiscard]] bool innobase_index_name_is_reserved(
    THD *thd,            /*!< in/out: MySQL connection */
    const KEY *key_info, /*!< in: Indexes to be
                         created */
    ulint num_of_keys);  /*!< in: Number of indexes to
                         be created. */

/** Check if the explicit tablespace targeted is file_per_table.
@param[in]      create_info     Metadata for the table to create.
@return true if the table is intended to use a file_per_table tablespace. */
static inline bool tablespace_is_file_per_table(
    const HA_CREATE_INFO *create_info) {
  return (create_info->tablespace != nullptr &&
          (0 ==
           strcmp(create_info->tablespace, dict_sys_t::s_file_per_table_name)));
}

/** Check if table will be explicitly put in an existing shared general
or system tablespace.
@param[in]      create_info     Metadata for the table to create.
@return true if the table will use a shared general or system tablespace. */
static inline bool tablespace_is_shared_space(
    const HA_CREATE_INFO *create_info) {
  return (create_info->tablespace != nullptr &&
          create_info->tablespace[0] != '\0' &&
          (0 !=
           strcmp(create_info->tablespace, dict_sys_t::s_file_per_table_name)));
}

/** Check if table will be explicitly put in a general tablespace.
@param[in]      create_info     Metadata for the table to create.
@return true if the table will use a general tablespace. */
static inline bool tablespace_is_general_space(
    const HA_CREATE_INFO *create_info) {
  return (
      create_info->tablespace != nullptr &&
      create_info->tablespace[0] != '\0' &&
      (0 !=
       strcmp(create_info->tablespace, dict_sys_t::s_file_per_table_name)) &&
      (0 != strcmp(create_info->tablespace, dict_sys_t::s_temp_space_name)) &&
      (0 != strcmp(create_info->tablespace, dict_sys_t::s_sys_space_name)));
}

/** Check if tablespace is shared tablespace.
@param[in]      tablespace_name Name of the tablespace
@return true if tablespace is a shared tablespace. */
static inline bool is_shared_tablespace(const char *tablespace_name) {
  if (tablespace_name != nullptr && tablespace_name[0] != '\0' &&
      (strcmp(tablespace_name, dict_sys_t::s_file_per_table_name) != 0)) {
    return true;
  }
  return false;
}

constexpr uint32_t SIZE_MB = 1024 * 1024;

/** Validate AUTOEXTEND_SIZE attribute for a tablespace.
@param[in]      ext_size        Value of autoextend_size attribute
@return DB_SUCCESS if the value of AUTOEXTEND_SIZE is valid. */
static inline int validate_autoextend_size_value(uint64_t ext_size) {
  ut_ad(ext_size > 0);

  page_no_t extent_size_pages = fsp_get_extent_size_in_pages(
      {static_cast<uint32_t>(srv_page_size),
       static_cast<uint32_t>(srv_page_size), false});

  /* Validate following for the AUTOEXTEND_SIZE attribute
  1. The autoextend_size should be a multiple of size of 4 extents
  2. The autoextend_size value should be between size of 4 extents and 4G */
  if (ext_size < (FSP_FREE_ADD * extent_size_pages * srv_page_size) ||
      ext_size > FSP_MAX_AUTOEXTEND_SIZE) {
    my_error(ER_INNODB_AUTOEXTEND_SIZE_OUT_OF_RANGE, MYF(0),
             (FSP_FREE_ADD * extent_size_pages * srv_page_size) / SIZE_MB,
             FSP_MAX_AUTOEXTEND_SIZE / SIZE_MB);
    return ER_INNODB_AUTOEXTEND_SIZE_OUT_OF_RANGE;
  }

  if ((ext_size / srv_page_size) % (FSP_FREE_ADD * extent_size_pages) != 0) {
    my_error(ER_INNODB_INVALID_AUTOEXTEND_SIZE_VALUE, MYF(0),
             FSP_FREE_ADD * extent_size_pages * srv_page_size / SIZE_MB);
    return ER_INNODB_INVALID_AUTOEXTEND_SIZE_VALUE;
  }

  return DB_SUCCESS;
}

/** Parse hint for table and its indexes, and update the information
in dictionary.
@param[in]      thd             Connection thread
@param[in,out]  table           Target table
@param[in]      table_share     Table definition */
void innobase_parse_hint_from_comment(THD *thd, dict_table_t *table,
                                      const TABLE_SHARE *table_share);

/** Obtain the InnoDB transaction of a MySQL thread.
@param[in,out]  thd     MySQL thread handler.
@return reference to transaction pointer */
trx_t *&thd_to_trx(THD *thd);

/** Class for handling create table information. */
class create_table_info_t {
 public:
  /** Constructor.
  Used in two ways:
  - all but file_per_table is used, when creating the table.
  - all but name/path is used, when validating options and using flags. */
  create_table_info_t(THD *thd, TABLE *form, HA_CREATE_INFO *create_info,
                      char *table_name, char *remote_path, char *tablespace,
                      bool file_per_table, bool skip_strict, uint32_t old_flags,
                      uint32_t old_flags2, bool is_partition)
      : m_thd(thd),
        m_trx(thd_to_trx(thd)),
        m_form(form),
        m_create_info(create_info),
        m_table_name(table_name),
        m_remote_path(remote_path),
        m_tablespace(tablespace),
        m_innodb_file_per_table(file_per_table),
        m_flags(old_flags),
        m_flags2(old_flags2),
        m_skip_strict(skip_strict),
        m_partition(is_partition) {}

  /** Initialize the object. */
  int initialize();

  /** Set m_tablespace_type. */
  void set_tablespace_type(bool table_being_altered_is_file_per_table);

  /** Create the internal innodb table.
  @param[in]    dd_table        dd::Table or nullptr for intrinsic table
  @param[in]    old_part_table  dd::Table from an old partition for partitioned
                                table, NULL otherwise.
  @return 0 or error number */
  int create_table(const dd::Table *dd_table, const dd::Table *old_part_table);

  /** Update the internal data dictionary. */
  int create_table_update_dict();

  /** Update the global data dictionary.
  @param[in]            dd_table        dd::Table or dd::Partition
  @retval       0               On success
  @retval       error number    On failure */
  template <typename Table>
  int create_table_update_global_dd(Table *dd_table);

  /** Validates the create options. Checks that the options
  KEY_BLOCK_SIZE, ROW_FORMAT, DATA DIRECTORY, TEMPORARY & TABLESPACE
  are compatible with each other and other settings.
  These CREATE OPTIONS are not validated here unless innodb_strict_mode
  is on. With strict mode, this function will report each problem it
  finds using a custom message with error code
  ER_ILLEGAL_HA_CREATE_OPTION, not its built-in message.
  @return NULL if valid, string name of bad option if not. */
  const char *create_options_are_invalid();

 private:
  /** Put a warning or error message to the error log for the
  DATA DIRECTORY option.
  @param[in]  msg     The reason that data directory is wrong.
  @param[in]  ignore  If true, append a message about ignoring
                      the data directory location. */
  void log_error_invalid_location(std::string &msg, bool ignore);

 public:
  /** Validate DATA DIRECTORY option. */
  bool create_option_data_directory_is_valid(bool ignore = false);

  /** Validate TABLESPACE option. */
  bool create_option_tablespace_is_valid();

  /** Validate COMPRESSION option. */
  bool create_option_compression_is_valid();

  /** Prepare to create a table. */
  int prepare_create_table(const char *name);

  /** Determine InnoDB table flags.
  If strict_mode=OFF, this will adjust the flags to what should be assumed.
  However, if an existing general tablespace is being targeted, we will NOT
  assume anything or adjust these flags.
  @retval true if successful, false if error */
  bool innobase_table_flags();

  /** Set flags and append '/' to remote path if necessary. */
  void set_remote_path_flags();

  /** Get table flags. */
  uint32_t flags() const { return (m_flags); }

  /** Get table flags2. */
  uint32_t flags2() const { return (m_flags2); }

  /** Reset table flags. */
  void flags_reset() { m_flags = 0; }

  /** Reset table flags2. */
  void flags2_reset() { m_flags2 = 0; }

  /** whether to skip strict check. */
  bool skip_strict() const { return (m_skip_strict); }

  /** Return table name. */
  const char *table_name() const { return (m_table_name); }

  THD *thd() const { return (m_thd); }

  inline bool is_intrinsic_temp_table() const {
    /* DICT_TF2_INTRINSIC implies DICT_TF2_TEMPORARY */
    ut_ad(!(m_flags2 & DICT_TF2_INTRINSIC) || (m_flags2 & DICT_TF2_TEMPORARY));
    return ((m_flags2 & DICT_TF2_INTRINSIC) != 0);
  }

  /** @return true only if table is temporary and not intrinsic */
  inline bool is_temp_table() const {
    return (((m_flags2 & DICT_TF2_TEMPORARY) != 0) &&
            ((m_flags2 & DICT_TF2_INTRINSIC) == 0));
  }

  /** Detach the just created table and its auxiliary tables if exist. */
  void detach();

  /** Normalizes a table name string.
  A normalized name consists of the database name catenated to '/' and
  table name. An example: test/mytable. On case insensitive file system
  normalization converts name to lower case.
  @param[in,out]        norm_name       Buffer to return the normalized name in.
  @param[in]            name            Table name string.
  @return true if successful. */
  static bool normalize_table_name(char *norm_name, const char *name);

 private:
  /** Parses the table name into normal name and either temp path or
  remote path if needed.*/
  int parse_table_name(const char *name);

  /** Create a table definition to an InnoDB database.
  @param[in]    dd_table        dd::Table or nullptr for intrinsic table
  @param[in]    old_part_table  dd::Table from an old partition for partitioned
                                table, NULL otherwise.
  @return HA_* level error */
  int create_table_def(const dd::Table *dd_table,
                       const dd::Table *old_part_table);

  /** Initialize the autoinc of this table if necessary, which should
  be called before we flush logs, so autoinc counter can be persisted. */
  void initialize_autoinc();

  /** Connection thread handle. */
  THD *m_thd;

  /** InnoDB transaction handle. */
  trx_t *m_trx;

  /** Information on table columns and indexes. */
  const TABLE *m_form;

  /** Create options. */
  HA_CREATE_INFO *m_create_info;

  /** Table name */
  char *m_table_name;
  /** Remote path (DATA DIRECTORY) or zero length-string */
  char *m_remote_path;
  /** Tablespace name or zero length-string. */
  char *m_tablespace;

  /** The newly created InnoDB table object. This is currently only
  used in this class, since the new table is not evictable until
  final success/failure, it can be accessed directly. */
  dict_table_t *m_table;

  /** Local copy of srv_file_per_table. */
  bool m_innodb_file_per_table;

  /** Allow file_per_table for this table either because:
  1) the setting innodb_file_per_table=on,
  2) it was explicitly requested by tablespace=innodb_file_per_table.
  3) the table being altered is currently file_per_table */
  bool m_allow_file_per_table;

  /** After all considerations, this shows whether we will actually
  create a table and tablespace using file-per-table. */
  bool m_use_file_per_table;

  /** Using DATA DIRECTORY */
  bool m_use_data_dir;

  /** Using a Shared General Tablespace */
  bool m_use_shared_space;

  /** Table flags */
  uint32_t m_flags;

  /** Table flags2 */
  uint32_t m_flags2;

  /** Skip strict check */
  bool m_skip_strict;

  /** True if this table is a partition */
  bool m_partition;
};

/** Class of basic DDL implementation, for CREATE/DROP/RENAME TABLE */
class innobase_basic_ddl {
 public:
  /** Create an InnoDB table.
  @tparam               Table           dd::Table or dd::Partition
  @param[in,out]        thd             THD object
  @param[in]    name            Table name, format: "db/table_name"
  @param[in]    form            Table format; columns and index
                                  information
  @param[in]    create_info     Create info(including create statement
                                  string)
  @param[in,out]        dd_tab          dd::Table describing table to be created
  @param[in]    file_per_table  whether to create a tablespace too
  @param[in]    evictable       whether the caller wants the
                                  dict_table_t to be kept in memory
  @param[in]    skip_strict     whether to skip strict check for create
                                  option
  @param[in]    old_flags       old Table flags
  @param[in]    old_flags2      old Table flags2
  @param[in]    old_dd_table    Table def for old table. Used in truncate or
                                while adding a new partition
  @return       error number
  @retval       0 on success */
  template <typename Table>
  static int create_impl(THD *thd, const char *name, TABLE *form,
                         HA_CREATE_INFO *create_info, Table *dd_tab,
                         bool file_per_table, bool evictable, bool skip_strict,
                         uint32_t old_flags, uint32_t old_flags2,
                         const dd::Table *old_dd_table);

  /** Drop an InnoDB table.
  @tparam               Table           dd::Table or dd::Partition
  @param[in,out]        thd             THD object
  @param[in]    name            table name
  @param[in]    dd_tab          dd::Table describing table to be dropped
  @param[in]    td              MySQL table definition
  @return       error number
  @retval       0 on success */

  template <typename Table>
  static int delete_impl(THD *thd, const char *name, const Table *dd_tab,
                         const TABLE *td);

  /** Renames an InnoDB table.
  @tparam               Table           dd::Table or dd::Partition
  @param[in,out]        thd             THD object
  @param[in]    from            old name of the table
  @param[in]    to              new name of the table
  @param[in]    from_table      dd::Table or dd::Partition of the table
                                  with old name
  @param[in]    to_table        dd::Table or dd::Partition of the table
                                  with new name
  @param[in]    td              MySQL table definition
  @return       error number
  @retval       0 on success */

  template <typename Table>
  static int rename_impl(THD *thd, const char *from, const char *to,
                         const Table *from_table, const Table *to_table,
                         const TABLE *td);
};

/** Class to handle TRUNCATE for one InnoDB table or one partition */
template <typename Table>
class innobase_truncate {
 public:
  /** Constructor
  @param[in]    thd             THD object
  @param[in]    name            normalized table name
  @param[in]    form            Table format; columns and index information
  @param[in]    dd_table        dd::Table or dd::Partition
  @param[in]    keep_autoinc    true to remember original autoinc counter
  @param[in]    table_truncate  true if this is full table truncate */
  innobase_truncate(THD *thd, const char *name, TABLE *form, Table *dd_table,
                    bool keep_autoinc, bool table_truncate)
      : m_thd(thd),
        m_name(name),
        m_dd_table(dd_table),
        m_trx(nullptr),
        m_table(nullptr),
        m_form(form),
        m_create_info(),
        m_file_per_table(false),
        m_keep_autoinc(keep_autoinc),
        m_table_truncate(table_truncate),
        m_flags(0),
        m_flags2(0) {}

  /** Destructor */
  ~innobase_truncate();

  /** Open the table/partition to be truncated
  @param[out]   innodb_table    InnoDB table object opened
  @return error number or 0 on success */
  int open_table(dict_table_t *&innodb_table);

  /** Do the truncate of the table/partition
  @return error number or 0 on success */
  int exec();

 private:
  /** Prepare for truncate
  @return error number or 0 on success */
  int prepare();

  /** Do the real truncation
  @return error number or 0 on success */
  int truncate();

  /** Rename tablespace file name
  @return error number or 0 on success */
  int rename_tablespace();

  /** Cleanup */
  void cleanup();

  /** Reload the FK related information
  @return error number or 0 on success */
  int load_fk();

 private:
  /** THD object */
  THD *m_thd;

  /** Normalized table name */
  const char *m_name;

  /** dd::Table or dd::Partition */
  Table *m_dd_table;

  /** Transaction attached to current thd */
  trx_t *m_trx;

  /** InnoDB table object for the table/partition */
  dict_table_t *m_table;

  /** Table format */
  TABLE *m_form;

  /** Create information */
  HA_CREATE_INFO m_create_info;

  /** True if this table/partition is file per table */
  bool m_file_per_table;

  /** True if the original autoinc counter should be kept. It's
  specified by caller, however if the table has no AUTOINC column,
  it would be reset to false internally */
  bool m_keep_autoinc;

  /** For a prtition table, this is true if full table is truncated. If only
  a partition is truncated, it is set to false. */
  bool m_table_truncate;

  /** flags of the table to be truncated, which should not change */
  uint32_t m_flags;

  /** flags2 of the table to be truncated, which should not change */
  uint32_t m_flags2;
};

/**
Initialize the table FTS stopword list
@return true if success */
[[nodiscard]] bool innobase_fts_load_stopword(
    dict_table_t *table, /*!< in: Table has the FTS */
    trx_t *trx,          /*!< in: transaction */
    THD *thd);           /*!< in: current thread */

/** Some defines for innobase_fts_check_doc_id_index() return value */
enum fts_doc_id_index_enum {
  FTS_INCORRECT_DOC_ID_INDEX,
  FTS_EXIST_DOC_ID_INDEX,
  FTS_NOT_EXIST_DOC_ID_INDEX
};

/**
Check whether the table has a unique index with FTS_DOC_ID_INDEX_NAME
on the Doc ID column.
@return the status of the FTS_DOC_ID index */
[[nodiscard]] fts_doc_id_index_enum innobase_fts_check_doc_id_index(
    const dict_table_t *table,  /*!< in: table definition */
    const TABLE *altered_table, /*!< in: MySQL table
                                that is being altered */
    ulint *fts_doc_col_no);     /*!< out: The column number for
                               Doc ID */

/**
Check whether the table has a unique index with FTS_DOC_ID_INDEX_NAME
on the Doc ID column in MySQL create index definition.
@return FTS_EXIST_DOC_ID_INDEX if there exists the FTS_DOC_ID index,
FTS_INCORRECT_DOC_ID_INDEX if the FTS_DOC_ID index is of wrong format */
[[nodiscard]] fts_doc_id_index_enum innobase_fts_check_doc_id_index_in_def(
    ulint n_key,          /*!< in: Number of keys */
    const KEY *key_info); /*!< in: Key definitions */

/**
Copy table flags from MySQL's TABLE_SHARE into an InnoDB table object.
Those flags are stored in .frm file and end up in the MySQL table object,
but are frequently used inside InnoDB so we keep their copies into the
InnoDB table object. */
void innobase_copy_frm_flags_from_table_share(
    dict_table_t *innodb_table,      /*!< in/out: InnoDB table */
    const TABLE_SHARE *table_share); /*!< in: table share */

/** Set up base columns for virtual column
@param[in]      table   the InnoDB table
@param[in]      field   MySQL field
@param[in,out]  v_col   virtual column to be set up */
void innodb_base_col_setup(dict_table_t *table, const Field *field,
                           dict_v_col_t *v_col);

/** Set up base columns for stored column
@param[in]      table   InnoDB table
@param[in]      field   MySQL field
@param[in,out]  s_col   stored column */
void innodb_base_col_setup_for_stored(const dict_table_t *table,
                                      const Field *field, dict_s_col_t *s_col);

/** whether this is a stored column */
static inline bool innobase_is_s_fld(const Field *field) {
  return field->gcol_info && field->stored_in_db;
}

/** Whether this is a computed multi-value virtual column.
This condition check should be equal to the following one:
(innobase_is_v_fld(field) && (field)->gcol_info->expr_item &&
 field->gcol_info->expr_item->returns_array())
*/
static inline bool innobase_is_multi_value_fld(const Field *field) {
  return field->is_array();
}

static inline bool normalize_table_name(char *norm_name, const char *name) {
  return create_table_info_t::normalize_table_name(norm_name, name);
}

/** Note that a transaction has been registered with MySQL.
@param[in]      trx     Transaction.
@return true if transaction is registered with MySQL 2PC coordinator */
inline bool trx_is_registered_for_2pc(const trx_t *trx) {
  return (trx->is_registered == 1);
}

/** Converts an InnoDB error code to a MySQL error code.
Also tells to MySQL about a possible transaction rollback inside InnoDB caused
by a lock wait timeout or a deadlock.
@param[in]  error   InnoDB error code.
@param[in]  flags   InnoDB table flags or 0.
@param[in]  thd     MySQL thread or NULL.
@return MySQL error code */
int convert_error_code_to_mysql(dberr_t error, uint32_t flags, THD *thd);

/** Converts a search mode flag understood by MySQL to a flag understood
by InnoDB.
@param[in]      find_flag       MySQL search mode flag.
@return InnoDB search mode flag. */
page_cur_mode_t convert_search_mode_to_innobase(
    enum ha_rkey_function find_flag);

extern bool innobase_stats_on_metadata;

/** Calculate Record Per Key value.
Need to exclude the NULL value if innodb_stats_method is set to "nulls_ignored"
@param[in]      index   InnoDB index.
@param[in]      i       The column we are calculating rec per key.
@param[in]      records Estimated total records.
@return estimated record per key value */
rec_per_key_t innodb_rec_per_key(const dict_index_t *index, ulint i,
                                 ha_rows records);

/** Build template for the virtual columns and their base columns. This
is done when the table first opened.
@param[in]      table           MySQL TABLE
@param[in]      ib_table        InnoDB dict_table_t
@param[in,out]  s_templ         InnoDB template structure
@param[in]      add_v           new virtual columns added along with
                                add index call
@param[in]      locked          true if dict_sys mutex is held
@param[in]      share_tbl_name  original MySQL table name */
void innobase_build_v_templ(const TABLE *table, const dict_table_t *ib_table,
                            dict_vcol_templ_t *s_templ,
                            const dict_add_v_col_t *add_v, bool locked,
                            const char *share_tbl_name);

/** Callback used by MySQL server layer to initialize
the table virtual columns' template
@param[in]      table           MySQL TABLE
@param[in,out]  ib_table        InnoDB table */
void innobase_build_v_templ_callback(const TABLE *table, void *ib_table);

/** Callback function definition, used by MySQL server layer to initialized
the table virtual columns' template */
typedef void (*my_gcolumn_templatecallback_t)(const TABLE *, void *);

/** Drop the statistics for a specified table, and mark it as discard
after DDL
@param[in,out]  thd     THD object
@param[in,out]  table   InnoDB table object */
void innobase_discard_table(THD *thd, dict_table_t *table);
#endif /* ha_innodb_h */

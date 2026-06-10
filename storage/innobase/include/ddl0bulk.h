/*****************************************************************************

Copyright (c) 2022, 2026, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is designed to work with certain software (including
but not limited to OpenSSL) that is licensed under separate terms,
as designated in a particular file or component or in included license
documentation.  The authors of MySQL hereby grant you an additional
permission to link the program and your derivative works with the
separately licensed software that they have either included with
the program or referenced in the documentation.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

/** @file include/ddl0bulk.h
BULK Data Load. Currently treated like DDL */

#pragma once

#include <list>
#include "api0api.h"
#include "btr0mtib.h"
#include "data0type.h"
#include "db0err.h"
#include "dict0types.h"
#include "mem0mem.h"
#include "mysql/components/services/bulk_data_service.h"
#include "row0mysql.h"
#include "sql/handler.h"

namespace ddl_bulk {

class Loader {
 public:
  using Blob_context = void *;
  using byte = unsigned char;

  class Table_reader {
   public:
    bool init(const std::string &schema, const std::string &table,
              const row_prebuilt_t *prebuilt,
              const std::optional<Rows_mysql> &lower_bound,
              const std::optional<Rows_mysql> &upper_bound);

    bool is_initialized() const { return m_initialized; }
    bool more_records_available() const { return m_more_records_available; }

    ib_tpl_t read() { /* Read the current row from cursor position */
      return m_read_tuple;
    }

    void next() {
      ib_err_t err;
      err = ib_cursor_next(m_read_cursor);
      if (err != DB_SUCCESS) {
        m_more_records_available = false;
        return;
      }
      err = ib_cursor_read_row(m_read_cursor, m_read_tuple, m_cmp_tuple,
                               IB_CUR_L, nullptr, nullptr, nullptr);
      m_more_records_available = err == DB_SUCCESS;
    }

    Table_reader() = default;
    Table_reader(Table_reader &&other) = default;

    ~Table_reader() {
      if (m_initialized) {
        ib_cursor_close(m_read_cursor);

        if (!m_prebuilt->index->is_clustered()) {
          ib_cursor_close(m_table_cursor);
        }

        if (m_cmp_tuple != nullptr) {
          ib_tuple_delete(m_cmp_tuple);
        }

        if (m_read_tuple != nullptr) {
          ib_tuple_delete(m_read_tuple);
        }

        trx_commit(m_trx);
        trx_free_for_background(m_trx);
      }
    }

   private:
    bool m_more_records_available{false};
    bool m_initialized{false};
    std::string m_table_name;
    const row_prebuilt_t *m_prebuilt;
    std::optional<Rows_mysql> m_lower_bound;
    std::vector<std::unique_ptr<char[]>> m_lower_bound_data;
    std::optional<Rows_mysql> m_upper_bound;
    std::vector<std::unique_ptr<char[]>> m_upper_bound_data;
    trx_t *m_trx{};
    ib_crsr_t m_table_cursor{};
    ib_crsr_t m_read_cursor{};
    ib_tpl_t m_cmp_tuple{};
    unsigned char m_cmp_tuple_row_id_data[DATA_ROW_ID_LEN];
    ib_tpl_t m_read_tuple{};
  };

  class Thread_data {
   public:
    /** Initialize thread specific data.
    @param[in]  prebuilt  prebuilt structures from innodb table handler */
    void init(const row_prebuilt_t *prebuilt);

    /** Load rows to a sub-tree for a specific thread.
    @param[in]      prebuilt  prebuilt structures from innodb table handler
    @param[in,out]  sub_tree  sub tree to load data to
    @param[in]      rows      rows to be loaded to the cluster index sub-tree
    @param[in]      wait_cbk    Stat callbacks
    @return innodb error code */
    dberr_t load(const row_prebuilt_t *prebuilt,
                 Btree_multi::Btree_load *sub_tree, const Rows_mysql &rows,
                 Bulk_load::Stat_callbacks &wait_cbk);

    bool set_source_table_data(
        const row_prebuilt_t *prebuilt,
        const Bulk_load::Source_table_data &source_table);

    dberr_t copy_existing_data(const row_prebuilt_t *prebuilt,
                               Btree_multi::Btree_load *sub_tree,
                               Bulk_load::Stat_callbacks &wait_cbk);

    /** Create a blob.
    @param[in]   sub_tree  sub tree to load data to
    @param[out]  blob_ctx  pointer to an opaque object representing a blob.
    @param[out]  ref       blob reference to be placed in the record.
    @return DB_SUCCESS on success or a failure error code. */
    dberr_t open_blob(Btree_multi::Btree_load *sub_tree, Blob_context &blob_ctx,
                      lob::ref_t &ref) {
      return sub_tree->open_blob(blob_ctx, ref);
    }

    /** Write data into the blob.
    @param[in]  sub_tree  sub tree to load data to
    @param[in]  blob_ctx  pointer to blob into which data is written.
    @param[out] ref       blob reference to be placed in the record.
    @param[in]  data      buffer containing data to be written
    @param[in]  len       length of the data to be written.
    @return DB_SUCCESS on success or a failure error code. */
    dberr_t write_blob(Btree_multi::Btree_load *sub_tree, Blob_context blob_ctx,
                       lob::ref_t &ref, const byte *data, size_t len) {
      return sub_tree->write_blob(blob_ctx, ref, data, len);
    }

    /** Indicate that the blob has been completed, so that resources can be
    removed, and as necessary flushing can be done.
    @param[in]  sub_tree  sub tree to load data to
    @param[in]  blob_ctx  pointer to blob which has been completely written.
    @param[out] ref       blob reference to be placed in the record.
    @return DB_SUCCESS on success or a failure error code. */
    dberr_t close_blob(Btree_multi::Btree_load *sub_tree, Blob_context blob_ctx,
                       lob::ref_t &ref) {
      return sub_tree->close_blob(blob_ctx, ref);
    }

    /** Free thread specific data. */
    void free();

    dberr_t get_error() const { return m_err; }
    std::string get_error_string() const { return m_sout.str(); }

    /** Get the client error code (eg. ER_LOAD_BULK_DATA_UNSORTED).
    @return the client error code. */
    int get_error_code() const { return m_errcode; }

    /** Add given subtree to the list of subtrees.
    @param[in]  subtree  the subtree to be added. */
    void add_subtree(Btree_multi::Btree_load *subtree) {
      m_list_subtrees.push_back(subtree);
    }

    /** Get the last subtree created by this thread. */
    Btree_multi::Btree_load *get_subtree() { return m_list_subtrees.back(); }

    Btree_multi::Btree_load *get_subtree(Blob_context ctx) const {
      auto iter = std::find_if(m_list_subtrees.begin(), m_list_subtrees.end(),
                               [ctx](const auto &subtree) {
                                 return subtree->verify_blob_context(ctx);
                               });
      ut_ad(iter != m_list_subtrees.end());
      return *iter;
    }

    /** Flush queue size used by the Bulk_flusher */
    size_t m_queue_size;

    /** Each subtree needs to have a disjoint set of keys.  In the case of
    generated DB_ROW_ID as PK, each thread can build one subtree for one range
    of row ids. */
    std::list<Btree_multi::Btree_load *> m_list_subtrees;

    /** The last DB_ROW_ID used by this thread. */
    uint64_t m_last_rowid{0};

   private:
    void read_input_entry(const Rows_mysql &rows, size_t &row_index,
                          const row_prebuilt_t *prebuilt, mem_heap_t *gcol_heap,
                          bool &gcol_blobs_flushed);
    void read_table_entry(const row_prebuilt_t *prebuilt);

    void insert_from_input_and_move_to_next(const row_prebuilt_t *prebuilt,
                                            const Rows_mysql &rows,
                                            size_t &row_index,
                                            Btree_multi::Btree_load *sub_tree,
                                            mem_heap_t *gcol_heap,
                                            bool &gcol_blobs_flushed);

    void insert_from_original_table_and_move_to_next(
        const row_prebuilt_t *prebuilt, Btree_multi::Btree_load *sub_tree);

    void insert_smaller_entry(const row_prebuilt_t *prebuilt,
                              Btree_multi::Btree_load *sub_tree,
                              const Rows_mysql &rows, size_t &row_index,
                              const ddl::Compare_key &compare_key,
                              mem_heap_t *gcol_heap, bool &gcol_blobs_flushed);

    /** Heap for allocating tuple memory. */
    mem_heap_t *m_heap{};

    /** Tuple for converting input data to table row. */
    dtuple_t *m_input_row{};

    dtuple_t *m_table_row{};

    /** Tuple for inserting row to index. */
    dtuple_t *m_input_entry{};

    /** Tuple for inserting row to index. */
    dtuple_t *m_original_table_entry{};

    /** Contains the tuple we detected an error, either m_original_table_entry
    or m_input_entry. */
    dtuple_t *m_error_entry{};

    /** Column data for system column transaction ID. */
    unsigned char m_trx_data[DATA_TRX_ID_LEN];

    /** Column data for system column Roll pointer. */
    unsigned char m_rollptr_data[DATA_ROLL_PTR_LEN];

    /** Column data for system column DATA_ROW_ID. */
    unsigned char m_rowid_data[DATA_ROW_ID_LEN];

    /** Error code at thread level. */
    dberr_t m_err{DB_SUCCESS};

    int m_errcode{0};

    std::ostringstream m_sout;

    size_t m_nth_index{std::numeric_limits<size_t>::max()};

    Table_reader m_table_reader;
    bool m_more_available_in_input{};
    bool m_more_available_in_original_table{};
    std::optional<std::string> m_original_table_name{};
  };

  /** Loader context constructor.
  @param[in]  num_threads  Number of threads to use for bulk loading
  @param[in]  keynr        index number
  @param[in]  trx          transaction context. */
  Loader(size_t num_threads, size_t keynr, const trx_t *trx)
      : m_num_threads(num_threads), m_keynr(keynr), m_trx(trx) {}

  /** Prepare bulk loading by multiple threads.
  @param[in]  prebuilt  prebuilt structures from innodb table handler
  @param[in]  data_size total data size to load in bytes
  @param[in]  memory    memory to be used from buffer pool
  @return innodb error code */
  dberr_t begin(const row_prebuilt_t *prebuilt, size_t data_size,
                size_t memory);

  /** Load rows to a sub-tree by a thread. Called concurrently by multiple
  execution threads.
  @param[in]  prebuilt      prebuilt structures from innodb table handler
  @param[in]  thread_index  identifies the thread and the B-tree to use.
  @param[in]  rows          rows to be loaded to the cluster index sub-tree
  @param[in]  wait_cbk      Stat callbacks
  @return innodb error code */
  dberr_t load(const row_prebuilt_t *prebuilt, size_t thread_index,
               const Rows_mysql &rows, Bulk_load::Stat_callbacks &wait_cbk);

  bool set_source_table_data(
      const row_prebuilt_t *prebuilt,
      const std::vector<Bulk_load::Source_table_data> &source_table_data);

  dberr_t copy_existing_data(const row_prebuilt_t *prebuilt,
                             size_t thread_index,
                             Bulk_load::Stat_callbacks &wait_cbk);

  size_t get_keynr() const { return m_keynr; }

  /** Open a blob.
  @param[in]   thread_index  identifies the thread and the B-tree to use.
  @param[out]  blob_ctx  pointer to an opaque object representing a blob.
  @param[out]  ref       blob reference to be placed in the record.
  @return DB_SUCCESS on success or a failure error code. */
  dberr_t open_blob(size_t thread_index, Blob_context &blob_ctx,
                    lob::ref_t &ref);

  /** Write data into the blob.
  @param[in]  thread_index  identifies the thread and the B-tree to use.
  @param[in]  blob_ctx  pointer to blob into which data is written.
  @param[out] ref       blob reference to be placed in the record.
  @param[in]  data      buffer containing data to be written
  @param[in]  len       length of the data to be written.
  @return DB_SUCCESS on success or a failure error code. */
  dberr_t write_blob(size_t thread_index, Blob_context blob_ctx,
                     lob::ref_t &ref, const byte *data, size_t len);

  /** Indicate that the blob has been completed, so that resources can be
  removed, and as necessary flushing can be done.
  @param[in]  thread_index  identifies the thread and the B-tree to use.
  @param[in]  blob_ctx  pointer to blob which has been completely written.
  @param[out]  ref       blob reference to be placed in the record.
  @return DB_SUCCESS on success or a failure error code. */
  dberr_t close_blob(size_t thread_index, Blob_context blob_ctx,
                     lob::ref_t &ref);

  /** Finish bulk load operation, combining the sub-trees produced by
  concurrent threads.
  @param[in]  is_error  true if called for cleanup and rollback after an error
  @return innodb error code */
  dberr_t end(bool is_error);

  using Btree_loads = std::vector<Btree_multi::Btree_load *,
                                  ut::allocator<Btree_multi::Btree_load *>>;
  using Thread_ctxs = std::vector<Thread_data, ut::allocator<Thread_data>>;

  dberr_t get_error() const;
  std::string get_error_string() const;

  /** Get the client error code (e.g. ER_LOAD_BULK_DATA_UNSORTED).
  @return the client error code. */
  int get_error_code() const;

  /** @return table name where the data is being loaded. */
  const char *get_table_name() const {
    if (m_original_table_name.has_value()) {
      return m_original_table_name.value().c_str();
    }
    return m_table->name.m_name;
  }

  /** @return index name where the data is being loaded. */
  const char *get_index_name() const { return m_index->name(); }

 private:
  /** Ensure that dict_sys->row_id is greater than max rowid used in bulk
  load of this table.
  @param[in]  max_rowid  max rowid used in this table. */
  void set_sys_max_rowid(uint64_t max_rowid);

  /** Merge the sub-trees to build the cluster index.
  @return innodb error code. */
  dberr_t merge_subtrees();

  /** Calculate the flush queue size to be used based on the available memory.
  @param[in] memory total buffer pool memory to use
  @param[out] flush_queue_size calculated queue size
  @param[out] allocate_in_pages true if need to allocate in pages
                                false if need to allocate in extents */
  void get_queue_size(size_t memory, size_t &flush_queue_size,
                      bool &allocate_in_pages) const;

 private:
  /** Number of threads for bulk loading. */
  const size_t m_num_threads{};

  const size_t m_keynr{};

  /** All thread specific data. */
  Thread_ctxs m_ctxs;

  /** Sub-tree loading contexts. */
  Btree_loads m_sub_tree_loads;

  /** Innodb dictionary table object. */
  dict_table_t *m_table;

  /** Index being loaded. Could be primary or secondary index. */
  dict_index_t *m_index{};

  /** Allocator to extend tablespace and allocate extents. */
  Btree_multi::Bulk_extent_allocator m_extent_allocator;

  const trx_t *const m_trx{};

  /** Flush queue size used by the Bulk_flusher */
  size_t m_queue_size;

  std::mutex m_gcol_mutex;

  std::optional<std::string> m_original_table_name;
};

inline std::string Loader::get_error_string() const {
  std::string error;
  for (auto &thr : m_ctxs) {
    if (thr.get_error() != DB_SUCCESS) {
      error = thr.get_error_string();
      break;
    }
  }
  return error;
}

inline int Loader::get_error_code() const {
  int errcode = 0;
  for (auto &thr : m_ctxs) {
    errcode = thr.get_error_code();
    if (errcode != 0) {
      break;
    }
  }
  return errcode;
}

inline dberr_t Loader::get_error() const {
  dberr_t e{DB_SUCCESS};
  for (auto &thr : m_ctxs) {
    e = thr.get_error();
    if (e != DB_SUCCESS) {
      break;
    }
  }
  return e;
}

}  // namespace ddl_bulk

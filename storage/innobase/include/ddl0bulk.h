/*****************************************************************************

Copyright (c) 2022, Oracle and/or its affiliates.

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

/** @file include/ddl0bulk.h
BULK Data Load. Currently treated like DDL */

#pragma once

#include "row0mysql.h"
#include "sql/handler.h"

namespace ddl_bulk {

class Loader {
 public:
  class Thread_data {
   public:
    /** Initialize thread specific data.
    @param[in]  prebuilt  prebuilt structures from innodb table handler */
    void init(const row_prebuilt_t *prebuilt);

    /** Load rows to a sub-tree for a specific thread.
    @param[in]      prebuilt  prebuilt structures from innodb table handler
    @param[in,out]  sub_tree  sub tree to load data to
    @param[in]      rows      rows to be loaded to the cluster index sub-tree
    @return innodb error code */
    dberr_t load(const row_prebuilt_t *prebuilt, Btree_load *sub_tree,
                 const Rows_mysql &rows);

    /** Free thread specific data. */
    void free();

    dberr_t get_error() const { return m_err; }
    std::string get_error_string() const { return m_sout.str(); }

    /** Get the client error code (eg. ER_LOAD_BULK_DATA_UNSORTED).
    @return the client error code. */
    int get_error_code() const { return m_errcode; }

   private:
    /** Fill system columns for index entry to be loaded.
    @param[in]  prebuilt  prebuilt structures from innodb table handler */
    void fill_system_columns(const row_prebuilt_t *prebuilt);

    /** Fil the tuple to set the column data
    @param[in]  prebuilt   prebuilt structures from innodb table handler
    @param[in]  rows       sql rows with column data
    @param[in]  row_index  current row index
    @return innodb error code. */
    dberr_t fill_tuple(const row_prebuilt_t *prebuilt, const Rows_mysql &rows,
                       size_t row_index);

    /** Fill he cluster index entry from tuple data.
    @param[in]  prebuilt  prebuilt structures from innodb table handler */
    void fill_index_entry(const row_prebuilt_t *prebuilt);

    /** Store integer column in Innodb format.
    @param[in,out]  dfield    innodb cloum to store data to
    @param[in]      col       sql column data
    @param[in,out]  data_ptr  data buffer for storing converted data
    @param[in]      data_len  data buffer length */
    void store_int_col(dfield_t *dfield, const Column_mysql &col,
                       byte *data_ptr, size_t data_len);

   private:
    /** Heap for allocating tuple memory. */
    mem_heap_t *m_heap{};

    /** Tuple for converting inpt data to table row. */
    dtuple_t *m_row{};

    /** Tuple for inserting row to cluster index. */
    dtuple_t *m_entry{};

    /** Column data for system column transaction ID. */
    unsigned char m_trx_data[DATA_TRX_ID_LEN];

    /** Column data for system column Roll pointer. */
    unsigned char m_rollptr_data[DATA_ROLL_PTR_LEN];

    /** Error code at thread level. */
    dberr_t m_err{DB_SUCCESS};

    int m_errcode{0};

    std::ostringstream m_sout;
  };

  /** Loader context constrcuctor.
  @param[in]  num_threads  Number of threads ro use for bulk loading */
  Loader(size_t num_threads) : m_num_threads(num_threads) {}

  /** Prepare bulk loading by mutliple threads.
  @param[in]  prebuilt  prebuilt structures from innodb table handler
  @return innodb error code */
  dberr_t begin(const row_prebuilt_t *prebuilt);

  /** Load rows to a sub-tree by a thread. Called concurrently by multiple
  execution threads.
  @param[in]  prebuilt      prebuilt structures from innodb table handler
  @param[in]  thread_index  true if called for cleanup and rollback after an
  error
  @param[in]  rows          rows to be loaded to the cluster index sub-tree
  @return innodb error code */
  dberr_t load(const row_prebuilt_t *prebuilt, size_t thread_index,
               const Rows_mysql &rows);

  /** Finish bulk load operation, combining the sub-trees procuced by concurrent
  threads.
  @param[in]  prebuilt  prebuilt structures from innodb table handler
  @param[in]  is_error  true if called for cleanup and rollback after an error
  @return innodb error code */
  dberr_t end(const row_prebuilt_t *prebuilt, bool is_error);

  using Btree_loads = std::vector<Btree_load *, ut::allocator<Btree_load *>>;
  using Thread_ctxs = std::vector<Thread_data, ut::allocator<Thread_data>>;

  /** @return auto extend size for the bulk operation. */
  uint64_t auto_extend_size();

  dberr_t get_error() const;
  std::string get_error_string() const;

  /** Get the client error code (eg. ER_LOAD_BULK_DATA_UNSORTED).
  @return the client error code. */
  int get_error_code() const;

  const char *get_table_name() const { return m_table->name.m_name; }

  /** Minimum auto extend size for bulk operation 4M. */
  static const uint64_t S_BULK_EXTEND_SIZE_MIN = 4 * 1024 * 1024;

  /** Maximum auto extend size for bulk operation 1G. */
  static const uint64_t S_BULK_EXTEND_SIZE_MAX = 1024 * 1024 * 1024;

 private:
  /** Merge the sub-trees to build the cluster index.
  @param[in]  prebuilt  prebuilt structures from innodb table handler
  @return innodb error code. */
  dberr_t merge_sutrees(const row_prebuilt_t *prebuilt);

 private:
  /** Number of threads for bulk loading. */
  size_t m_num_threads{};

  /** All thread specific data. */
  Thread_ctxs m_ctxs;

  /** Sub-tree loading contexts. */
  Btree_loads m_sub_tree_loads;

  dict_table_t *m_table;
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

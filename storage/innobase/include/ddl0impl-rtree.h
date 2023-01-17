
/*****************************************************************************

Copyright (c) 2020, 2023, Oracle and/or its affiliates.

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

/** @file include/ddl0impl-rtree.h
 DDL RTree insert interface.
 Created 2020-11-01 by Sunny Bains. */

#ifndef ddl0impl_rtree_h
#define ddl0impl_rtree_h

#include "ddl0fts.h"
#include "ddl0impl.h"
#include "row0pread.h"

namespace ddl {

/** Class that caches RTree index tuples made from a single cluster
index page scan, and then insert into corresponding index tree. */
class RTree_inserter {
 public:
  /** Constructor.
  @param[in,out] ctx            DDL context.
  @param[in]    index               Index to be created */
  RTree_inserter(Context &ctx, dict_index_t *index) noexcept;

  /** Destructor */
  ~RTree_inserter() noexcept;

  /** @return true if initialization succeeded . */
  [[nodiscard]] bool is_initialized() noexcept {
    return m_dtuple_heap != nullptr && m_dml_heap != nullptr;
  }

  /** Get the index instance.
  @return the index instance. */
  [[nodiscard]] dict_index_t *get_index() noexcept { return m_index; }

  /** Caches an index row into index tuple vector
  @param[in] row                      Table row
  @param[in] ext                      Externally stored column
  prefixes, or nullptr */
  void add_to_batch(const dtuple_t *row, const row_ext_t *ext) noexcept;

  /** Insert the rows cached in the batch (m_dtuples).
  @param[in]    trx_id                  Transaction id.
  @param[in,out] latch_release  Called when a log free check is required.
  @return DB_SUCCESS if successful, else error number */
  [[nodiscard]] dberr_t batch_insert(trx_id_t trx_id,
                                     Latch_release &&latch_release) noexcept;

  /** Deep copy the fields pointing to the clustered index record. */
  void deep_copy_tuples() noexcept { deep_copy_tuples(m_dtuples->begin()); }

 private:
  /** Cache index rows made from a cluster index scan. Usually
  for rows on single cluster index page */
  using Tuples = std::vector<dtuple_t *, ut::allocator<dtuple_t *>>;

  /** Deep copy the fields pointing to the clustered index record.
  @param[in] it                 Deep copy from this tuple onwards. */
  void deep_copy_tuples(Tuples::iterator it) noexcept;

 private:
  /** vector used to cache index rows made from cluster index scan */
  Tuples *m_dtuples{};

  /** Memory heap for creating index tuples */
  mem_heap_t *m_dtuple_heap{};

  /** Memory heap for inserting the tuples */
  mem_heap_t *m_dml_heap{};

  /** the index being built */
  dict_index_t *m_index{};

  /** Iterator to process m_dtuples */
  Tuples::iterator m_iter{};

  /** DDL context. */
  Context &m_ctx;
};

}  // namespace ddl

#endif /* !ddl0impl-rtree_h */

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

/** @file include/ddl0impl-cursor.h
 DDL scan cursor interface.
 Created 2020-11-01 by Sunny Bains. */

#ifndef ddl0impl_cursor_h
#define ddl0impl_cursor_h

#include "ddl0fts.h"
#include "ddl0impl.h"

namespace ddl {

/** Cursor for reading the data. */
struct Cursor {
  using Post_row = std::function<dberr_t()>;

  /** Constructor.
  @param[in,out] ctx            DDL context. */
  explicit Cursor(ddl::Context &ctx) noexcept : m_ctx(ctx) {}

  /** Destructor. */
  virtual ~Cursor() noexcept {
    if (m_prev_fields != nullptr) {
      ut::free(m_prev_fields);
      m_prev_fields = nullptr;
    }
  }

  /** Open the cursor. */
  virtual void open() noexcept = 0;

  /** Do any post processing.
  @param[in] err                Error code.
  @return DB_SUCCESS or error code. */
  virtual dberr_t finish(dberr_t err) noexcept;

  /** @return the index to iterate over. */
  [[nodiscard]] virtual dict_index_t *index() noexcept = 0;

  /** Copy the row data, by default only the pointers are copied.
  @param[in] thread_id          Scan thread ID.
  @param[in,out] row            Row to copy.
  @return DB_SUCCESS or error code. */
  [[nodiscard]] virtual dberr_t copy_row(size_t thread_id,
                                         Row &row) noexcept = 0;

  /** Setup the primary key sort data structures.
  @param[in] n_uniq             Number of columns to make they unique key.
  @return DB_SUCCESS or error code. */
  [[nodiscard]] dberr_t setup_pk_sort(size_t n_uniq) noexcept {
    auto p =
        ut::malloc_withkey(UT_NEW_THIS_FILE_PSI_KEY, n_uniq * sizeof(dfield_t));

    if (p == nullptr) {
      return DB_OUT_OF_MEMORY;
    }

    m_prev_fields = static_cast<dfield_t *>(p);

    m_tuple_heap.create(sizeof(mrec_buf_t), UT_LOCATION_HERE);

    return m_tuple_heap.get() == nullptr ? DB_OUT_OF_MEMORY : DB_SUCCESS;
  }

  /** Reads clustered index of the table and create temporary file(s)
  containing the index entries for the indexes to be built.
  @param[in,out] builders Merge buffers to use for reading.
  @return DB_SUCCESS or error code. */
  [[nodiscard]] virtual dberr_t scan(Builders &builders) noexcept = 0;

  /** @return true if EOF reached. */
  [[nodiscard]] virtual bool eof() const noexcept = 0;

  /** Create a cluster index scan cursor.
  @param[in,out] ctx            DDL context.
  @return a cursor instance or nullptr (if OOM). */
  static Cursor *create_cursor(ddl::Context &ctx) noexcept;

 public:
  /** DDL context. */
  ddl::Context &m_ctx;

  /** Scoped heap to use for rows. */
  Scoped_heap m_row_heap{};

  /** Scoped heap to use for tuple instances. */
  Scoped_heap m_tuple_heap{};

  /** Previous fields. */
  dfield_t *m_prev_fields{};
};

}  // namespace ddl

#endif /* !ddl0impl-cursor_h */

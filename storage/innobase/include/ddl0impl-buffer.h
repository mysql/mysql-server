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

/** @file include/ddl0impl-buffer.h
 DDL buffer infrastructure.
 Created 2020-11-01 by Sunny Bains. */

#ifndef ddl0impl_buffer_h
#define ddl0impl_buffer_h

#include "ddl0impl.h"
#include "dict0dict.h"

namespace ddl {
/** Buffer for sorting in main memory. */
struct Key_sort_buffer : private ut::Non_copyable {
  /** Callback for writing serialized data to to disk.
  @param[in] io_buffer          Buffer to persist.
  @param[in,out] n              Number of bytes written is returned.
                                Input value semantics:
                                0  - Write up to aligned length.
                                >0 - All data will be written and
                                     last block will be padded with zeros.
  @return DB_SUCCES or error code. */
  using Function = std::function<dberr_t(IO_buffer io_buffer, os_offset_t &n)>;

  /** Constructor.
  @param[in,out] index          Sort buffer is for this index.
  @param[in] size               Sort buffer size in bytes. */
  explicit Key_sort_buffer(dict_index_t *index, size_t size) noexcept;

  /** Destructor. */
  ~Key_sort_buffer() noexcept { mem_heap_free(m_heap); }

  /** Sort the elements in m_dtuples.
  @param[in,out] dup            For collecting the duplicate rows. */
  void sort(ddl::Dup *dup) noexcept;

  /** Serialize the contents for storing to disk.
  @param[in] io_buffer          Buffer for serializing.
  @param[in] f                  Function for persisting the data.
  @return DB_SUCCESS or error code. */
  dberr_t serialize(IO_buffer io_buffer, Function &&f) noexcept;

  /** Reset the sort buffer. clear the heap and entries. */
  void clear() noexcept;

  /** @return true if the index is clustered. */
  [[nodiscard]] bool is_clustered() const noexcept {
    return m_index->is_clustered();
  }

  /** @return true if the index is an FTS index. */
  [[nodiscard]] bool is_fts() const noexcept {
    return m_index->type & DICT_FTS;
  }

  /** @return true if the index has a unique constraint. */
  [[nodiscard]] bool is_unique() const noexcept {
    return dict_index_is_unique(m_index);
  }

  /** @return the heap to use. */
  [[nodiscard]] mem_heap_t *heap() noexcept { return m_heap; }

  /** @return number of tuples stored so far. */
  [[nodiscard]] size_t size() const noexcept { return m_n_tuples; }

  /** @return true if the buffer is full. */
  [[nodiscard]] bool full() const noexcept { return size() >= m_max_tuples; }

  /** @return true if the buffer is empty. */
  [[nodiscard]] bool empty() const noexcept { return size() == 0; }

  /** @return a references to the last element. */
  [[nodiscard]] dfield_t *&back() noexcept {
    ut_a(!empty());
    return m_dtuples[size() - 1];
  }

  /** Allocate fields from the heap.
  @param[in] n                  Number of fields to allocate.
  @return an array of n dfields. */
  dfield_t *alloc(size_t n) noexcept {
    const auto sz = sizeof(dfield_t) * n;
    return static_cast<dfield_t *>(mem_heap_alloc(m_heap, sz));
  }

  /** Check if n bytes will fit in the buffer.
  @param[in] n                  Number of bytes to check.
  @return true if n bytes will fit in the buffer. */
  bool will_fit(size_t n) const noexcept {
    /* Reserve one byte for the end marker and adjust for meta-data overhead. */
    return m_total_size + n +
               (sizeof(std::remove_pointer<decltype(
                           m_dtuples)::value_type>::type) *
                (m_n_tuples + 1)) <=
           m_buffer_size - 1;
  }

  /** Deep copy the field data starting from the back.
  @param[in] n_fields           Number of fields to copy.
  @param[in] data_size          Size in bytes of the data to copy. */
  void deep_copy(size_t n_fields, size_t data_size) noexcept;

  /** Compare two merge data tuples.
  @param[in] lhs                Fields to compare on the LHS
  @param[in] rhs                Fields to compare on the RHS
  @param[in,out] dup            For capturing duplicates (or nullptr).
  @retval +ve - if lhs > rhs
  @retval -ve - if lhs < rhs
  @retval 0 - if lhs == rhs */
  [[nodiscard]] static int compare(const dfield_t *lhs, const dfield_t *rhs,
                                   Dup *dup) noexcept;

  using DTuple = dfield_t *;
  using DTuples = std::vector<DTuple, ut::allocator<DTuple>>;

  /** Memory heap where allocated */
  mem_heap_t *m_heap{};

  /** The index the tuples belong to */
  dict_index_t *m_index{};

  /** Total amount of data bytes */
  size_t m_total_size{};

  /** Number of data tuples */
  size_t m_n_tuples{};

  /** Maximum number of data tuples */
  size_t m_max_tuples{};

  /** Array of data tuples */
  DTuples m_dtuples{};

  /** Buffer size. */
  size_t m_buffer_size{};
};

}  // namespace ddl

#endif /* !ddl0impl_buffer_h */

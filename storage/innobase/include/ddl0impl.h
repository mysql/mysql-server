/*****************************************************************************

Copyright (c) 2005, 2022, Oracle and/or its affiliates.

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

/** @file include/ddl0impl.h
 DDL implementation include file.
 Created 2020-11-01 by Sunny Bains. */

#ifndef ddl0impl_h
#define ddl0impl_h

#include "ddl0ddl.h"
#include "dict0mem.h"
#include "ut0class_life_cycle.h"
#include "ut0mpmcbq.h"

namespace ddl {

/** Cluster index ID (always the first index). */
static constexpr size_t SERVER_CLUSTER_INDEX_ID = 0;

/** @brief Block size for DDL I/O operations. The minimum is UNIV_PAGE_SIZE,
or page_get_free_space_of_empty() rounded to a power of 2. */
using IO_buffer = std::pair<byte *, os_offset_t>;

/** Called when a log free check is required. */
using Latch_release = std::function<dberr_t()>;

/* Ignore posix_fadvise() on those platforms where it does not exist */
#if defined _WIN32
#define posix_fadvise(fd, offset, len, advice) /* nothing */
#endif                                         /* _WIN32 */

// Forward declaration.
struct Cursor;
struct Builder;

using Builders = std::vector<Builder *, ut::allocator<Builder *>>;

/** Start offsets in the file, from where to merge records. */
using Merge_offsets = std::deque<os_offset_t, ut::allocator<os_offset_t>>;

/** Information about temporary files used in merge sort */
struct file_t {
  /** File. */
  Unique_os_file_descriptor m_file;

  /** Size of the file in bytes. */
  os_offset_t m_size;

  /** Number of records in the file */
  uint64_t m_n_recs{};
};

/** Fetch the document ID from the table. */
struct Fetch_sequence : public Context::FTS::Sequence {
  /** Constructor.
  @param[in] index              Document ID index. */
  explicit Fetch_sequence(dict_index_t *index) noexcept : m_index(index) {
    ut_a(m_index->type & DICT_FTS);
    m_max_doc_id = m_doc_id = 0;
  }

  /** Destructor. */
  ~Fetch_sequence() noexcept override {}

  /** Not supported.
  @return the current document ID. */
  [[nodiscard]] doc_id_t current() noexcept override { ut_error; }

  /** Not supported. */
  void increment() noexcept override { ut_error; }

  /** Get the next document ID.
  @param[in] dtuple             Row from which to fetch ID.
  @return the document ID from the row. */
  [[nodiscard]] doc_id_t fetch(const dtuple_t *dtuple) noexcept override;

  /** @return the number of document IDs generated. */
  doc_id_t generated_count() const noexcept override { ut_error; }

  /** @return the maximum document ID seen so far. */
  [[nodiscard]] doc_id_t max_doc_id() const noexcept override {
    return m_max_doc_id;
  }

  /** @return false, because we never generate the document ID. */
  [[nodiscard]] bool is_generated() const noexcept override { return false; }

  /** The document ID index. */
  dict_index_t *m_index{};

  /** Maximum document ID seen so far. */
  doc_id_t m_max_doc_id{};
};

/** Physical row context. */
struct Row {
  /** Constructor. */
  Row() = default;

  Row(const Row &) = default;

  /** Destructor. */
  ~Row() = default;

  /** Build a row from a raw record.
  @param[in,out] ctx            DDL context.
  @param[in,out] index          Index the record belongs to.
  @param[in,out] heap           Heap to use for allocation.
  @param[in] type               Copy pointers or copy data.
  @return DB_SUCCESS or error code. */
  [[nodiscard]] dberr_t build(ddl::Context &ctx, dict_index_t *index,
                              mem_heap_t *heap, size_t type) noexcept;

  /** Externally stored fields. */
  row_ext_t *m_ext{};

  /** Column offsets. */
  ulint *m_offsets{};

  /** Row data. */
  const rec_t *m_rec{};

  /** DTuple data, mapped over m_rec. */
  const dtuple_t *m_ptr{};

  /** Add column data values. */
  dtuple_t *m_add_cols{};
};

/** Create a merge file int the given location.
@param[out] file                Temporary generated during DDL.
@param[in] path                 Location for creating temporary file
@return true if file is created successfully */
[[nodiscard]] bool file_create(file_t *file, const char *path) noexcept;

/** Write a merge block to the file system.
@param[in] fd                   File descriptor
@param[in] ptr                  Buffer to write.
@param[in] size                 Number of bytes to write.
@param[in] offset               Byte offset where to write.
@return DB_SUCCESS or error code */
dberr_t pwrite(os_fd_t fd, void *ptr, size_t size, os_offset_t offset) noexcept;

/** Read a merge block from the file system.
@param[in] fd                   file descriptor.
@param[out] ptr                 Buffer to read into.
@param[in] len                  Number of bytes to read.
@param[in] offset               Byte offset to start reading from.
@return DB_SUCCESS or error code */
[[nodiscard]] dberr_t pread(os_fd_t fd, void *ptr, size_t len,
                            os_offset_t offset) noexcept;

}  // namespace ddl

#endif /* ddl0impl_h */

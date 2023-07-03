/*****************************************************************************

Copyright (c) 2020, 2022, Oracle and/or its affiliates.

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

/** @file include/ddl0impl-file-reader.h
 For scanning the temporary file.
 Created 2020-11-01 by Sunny Bains. */

#ifndef ddl0impl_file_reader_h
#define ddl0impl_file_reader_h

#include "ddl0impl-buffer.h"

namespace ddl {

// Forward declaration
struct File_cursor;

/** Read rows from the temporary file. */
struct File_reader : private ut::Non_copyable {
  /** Constructor.
  @param[in] file               Opened file.
  @param[in,out] index          Index that the rows belong to.
  @param[in] buffer_size        Size of file buffer for reading.
  @param[in] size               File size in bytes. */
  File_reader(const Unique_os_file_descriptor &file, dict_index_t *index,
              size_t buffer_size, os_offset_t size) noexcept
      : m_index(index), m_file(file), m_size(size), m_buffer_size(buffer_size) {
    ut_a(size > 0);
    ut_a(m_buffer_size > 0);
    ut_a(m_index != nullptr);
    ut_a(m_file.is_open());
  }

  /** Destructor. */
  ~File_reader() noexcept {
    if (m_aux_buf != nullptr) {
      ut::delete_arr(m_aux_buf);
    }
  }

  /** Prepare the file for reading.
  @return DB_SUCCESS or error code. */
  [[nodiscard]] dberr_t prepare() noexcept;

  /** The current row as a tuple. Note: the tuple only does a shallow copy.
  @param[in,out] builder        Index builder instance.
  @param[in,out] heap           Heap to use for allocation.
  @param[out] dtuple            Row converted to a tuple.
  @return DB_SUCCESS or error code. */
  [[nodiscard]] dberr_t get_tuple(Builder *builder, mem_heap_t *heap,
                                  dtuple_t *&dtuple) noexcept;

  /** Seek to the offset and read the page in.
  @param[in] offset              Offset to read in.
  @return DB_SUCCESS or error code. */
  dberr_t read(os_offset_t offset) noexcept;

  /** Set the range or rows to traverse.
  @param[in] offset              New offset to read from. */
  void set_offset(os_offset_t offset) noexcept { m_offset = offset; }

  /** @return true if the range first == second. */
  [[nodiscard]] bool eof() const noexcept { return m_offset == m_size; }

  /** @return the number of rows read from the file. */
  [[nodiscard]] uint64_t get_n_rows_read() const noexcept {
    return m_n_rows_read;
  }

 private:
  /** Seek to the start of the range and load load the page.
  @param[in] offset              Offset to read in.
  @return DB_SUCCESS or error code. */
  [[nodiscard]] dberr_t seek(os_offset_t offset) noexcept;

  /** Advance page number to the next and read in.
  @return DB_SUCCESS or error code. */
  [[nodiscard]] dberr_t read_next() noexcept;

  /** Advance the "cursor".
  @return DB_SUCCESS or error code. */
  [[nodiscard]] dberr_t next() noexcept;

 public:
  using Offsets = std::vector<ulint, ut::allocator<ulint>>;

  /** Index that the records belong to. */
  dict_index_t *m_index{};

  /** Pointer to current row. */
  const mrec_t *m_mrec{};

  /** Columns offsets. */
  Offsets m_offsets{};

  /** File handle to read from. */
  const Unique_os_file_descriptor &m_file;

 private:
  using Bounds = std::pair<const byte *, const byte *>;

  /** Size of the file in bytes. */
  os_offset_t m_size{};

  /** Offset to read. */
  os_offset_t m_offset{};

  /** Pointer current offset within file buffer. */
  const byte *m_ptr{};

  /** File buffer bounds. */
  Bounds m_bounds{};

  /** Auxiliary buffer for records that span across pages. */
  byte *m_aux_buf{};

  /** IO buffer size in bytes. */
  size_t m_buffer_size{};

  /** Aligned IO buffer. */
  Aligned_buffer m_aligned_buffer{};

  /** File buffer for reading. */
  IO_buffer m_io_buffer{};

  /** Number of rows read from the file. */
  uint64_t m_n_rows_read{};

  friend File_cursor;
};

}  // namespace ddl

#endif /* ddl0impl_file_reader_h */

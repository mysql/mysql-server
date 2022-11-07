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

/** @file ddl/ddl0builder.cc
 DDL build index implementation.
Created 2020-11-01 by Sunny Bains. */

#include <debug_sync.h>
#include "btr0load.h"
#include "clone0api.h"
#include "ddl0fts.h"
#include "ddl0impl-builder.h"
#include "ddl0impl-compare.h"
#include "ddl0impl-cursor.h"
#include "ddl0impl-loader.h"
#include "ddl0impl-merge.h"
#include "ddl0impl-rtree.h"
#include "lob0lob.h"
#include "os0thread-create.h"
#include "rem0rec.h"
#include "row0ext.h"
#include "row0vers.h"
#include "scope_guard.h"
#include "ut0stage.h"

namespace ddl {

/** Write records to a temporary file. */
struct File_writer {
  /** Constructor.
  @param[in]   builder       the index builder object.
  @param[in]   buffer_size   the i/o buffer size (in bytes). */
  File_writer(Builder *builder, size_t buffer_size);

  /** Destructor.  Close the temporary file and free the in-memory buffer. */
  ~File_writer();

  /** Open the file writer.  It creates a temporary file and allocates
  a memory buffer of size m_buffer_size.
  @return DB_SUCCESS on success, an error code on failure. */
  dberr_t open();

  /** Write the given record mrec to the temporary file.  First the record is
  added to the internal memory buffer and if this buffer becomes full, it is
  written to the temporary file.
  @param[in]  mrec   the serialized record.
  @param[in]  offsets   the record offsets.
  @return DB_SUCCESS on success, an error code on failure. */
  dberr_t write(const mrec_t *mrec, const ulint *offsets);

  /** Add the end-of-list marker to the in-memory buffer, and flush the
  contents of the buffer to the temporary file.
  @return DB_SUCCESS on success, an error code on failure. */
  dberr_t flush();

  /** Get the number of records written by this object.
  @return the number of records written by this writer. */
  size_t get_row_count() const { return m_n_wrote; }

  /** Get the current size of the underlying temporary file, including the
  contents in the in-memory buffer.
  @return the current size. */
  size_t get_current_size() const {
    return m_file.m_size + (m_ptr - m_io_buffer.first);
  }

  /** Reset the object.  Does not free the in-memory buffer. And does not
  close the underlying temporary file. */
  void reset() {
    m_n_wrote = 0;
    m_ptr = m_io_buffer.first;
    m_file.reset();
  }

  bool is_open() const { return m_file.is_open(); }

  /** Output temporary file into which this object writes data. */
  file_t m_file;

  /** Number of records written to the file. */
  size_t m_n_wrote{};

  /** The index builder that is using this File_writer object. */
  Builder *m_builder{};

  /** Current location within the in-memory buffer where the next write
  operation will take place. */
  byte *m_ptr{};

  /** The i/o buffer pointing to the m_buffer */
  IO_buffer m_io_buffer;

  /** The in-memory buffer of size m_buffer_size. */
  Aligned_buffer m_buffer;

  /** The size of the in-memory buffer. */
  size_t m_buffer_size;

#ifdef UNIV_DEBUG
  /** This is a debug function to check if we are able to read the same number
  of records that was written by this object. */
  size_t count_recs_in_file(size_t n_rows);
#endif /* UNIV_DEBUG */
};

File_writer::File_writer(Builder *builder, size_t buffer_size)
    : m_builder(builder), m_buffer_size(buffer_size) {}

File_writer::~File_writer() {
  /* Don't close the m_file here. */
  m_buffer.deallocate();
  reset();
}

dberr_t File_writer::open() {
  DBUG_EXECUTE_IF("ddl_write_failure", {
    m_builder->set_error(DB_CORRUPTION);
    return m_builder->get_error();
  });

  const os_fd_t fd = m_builder->create_file(m_file);
  if (fd == OS_FD_CLOSED) {
    return DB_FAIL;
  }

  if (m_io_buffer.first == nullptr) {
    if (!m_buffer.allocate(m_buffer_size)) {
      return DB_OUT_OF_MEMORY;
    }
  }

  m_io_buffer = m_buffer.io_buffer();
  m_n_wrote = 0;
  m_ptr = m_io_buffer.first;
  return DB_SUCCESS;
}

/** Write sorted data into multiple files. */
struct Split_writer {
  /** Constructor.
  @param[in]   builder   the index builder which uses this split writer.
  @param[in]  io_buffer_size  the i/o buffer size
  @param[in]  bytes_per_file  target size of each file created. */
  Split_writer(Builder *builder, size_t io_buffer_size, size_t bytes_per_file);

  /** Open and prepare the underlying File_writer object.
  @return DB_SUCCESS on success, an error code on failure. */
  dberr_t open() { return m_file_writer.open(); }

  /** Write the given record to the underlying file.  Before writing, check
  the current size of the file. If the current size of the file is more than
  target size, then create the next temporary file.
  @param[in]  mrec   the serialized record.
  @param[in]  offsets   the record offsets.
  @return DB_SUCCESS on success, an error code on failure. */
  dberr_t write(const mrec_t *mrec, const ulint *offsets);

  /** Flush the remaining data to the temporary file on disk.
  @return DB_SUCCESS on success, an error code on failure. */
  dberr_t finish();

  /** Create a new thread to build a B-tree subtree using the last temporary
  file containing sorted data.
  @return DB_SUCCESS on success, an error code on failure. */
  [[nodiscard]] dberr_t create_build_thread();

  /** The index builder that is using this split writer. */
  Builder *m_builder;

  /** The writer object used to write data to temporary file. */
  File_writer m_file_writer;

  /** Target size (in bytes) of the files created. */
  const size_t m_bytes_per_file;

  /** Check if the size of the current file has reached the expected size.
  @return true if the file can be considered full. */
  bool is_file_full() const;

#ifdef UNIV_DEBUG
 public:
  /** Split mode used for testing.  The height of the resulting subtrees can
  be controlled by using different split modes. */
  enum split_mode_t {
    SPLIT_MODE_NONE,
    SPLIT_MODE_1, /* Data distribution b/w files: ( 1%, 10%, 20%, 69%) */
    SPLIT_MODE_2, /* Data distribution b/w files: (69%, 20%, 10%,  1%) */
  };

  /** Set the split mode.  This is debug function which is to be called using
  DBUG_EXECUTE_IF() macro.
  @param[in]  bytes  total size of the B-tree data in bytes.
  @param[in]  max_files  maximum number of files that can be created.
  @param[in]  mode  split mode to control subtree heights. */
  void set_split_mode(size_t bytes, size_t max_files,
                      Split_writer::split_mode_t mode);

  /** Check the current file size against the expected file size by making
  use of the split mode.
  @return expected size of the current file. */
  size_t check_size_with_split_mode() const;

  /** Total size of the B-tree data in bytes. */
  size_t m_total_bytes{};

  /** Total number of files, equal to number of subtrees created. */
  size_t m_n_files{};

  /** Maximum number of files allowed. */
  size_t m_max_files{};

  /** Current file number. */
  size_t m_nth_file{};

  /** Split mode used. */
  split_mode_t m_split_mode{SPLIT_MODE_NONE};
#endif /* UNIV_DEBUG */
};

bool Split_writer::is_file_full() const {
#ifdef UNIV_DEBUG
  if (m_split_mode != SPLIT_MODE_NONE) {
    const size_t split_size = check_size_with_split_mode();
    const bool do_split = (m_file_writer.get_current_size() >= split_size);
    return do_split;
  }
#endif /* UNIV_DEBUG */
  return m_file_writer.get_current_size() >= m_bytes_per_file;
}

#ifdef UNIV_DEBUG
size_t Split_writer::check_size_with_split_mode() const {
  size_t nbytes = 0;
  const size_t file_1_size = (m_total_bytes * 1 / 100);
  const size_t file_2_size = (m_total_bytes * 10 / 100);
  const size_t file_3_size = (m_total_bytes * 20 / 100);
  const size_t file_4_size =
      (m_total_bytes - file_1_size - file_2_size - file_3_size);
  switch (m_split_mode) {
    case SPLIT_MODE_1: {
      if (m_nth_file == 0) {
        nbytes = file_1_size;
      } else if (m_nth_file == 1) {
        nbytes = file_2_size;
      } else if (m_nth_file == 2) {
        nbytes = file_3_size;
      } else if (m_nth_file == 3) {
        nbytes = file_4_size;
      } else {
        ut_ad(0);
      }
    } break;
    case SPLIT_MODE_2: {
      if (m_nth_file == 0) {
        nbytes = file_4_size;
      } else if (m_nth_file == 1) {
        nbytes = file_1_size;
      } else if (m_nth_file == 2) {
        nbytes = file_2_size;
      } else if (m_nth_file == 3) {
        nbytes = file_3_size;
      } else {
        ut_ad(0);
      }
    } break;
    default:
      ut_ad(0);
      break;
  }
  return nbytes;
}

void Split_writer::set_split_mode(size_t bytes, size_t max_files,
                                  Split_writer::split_mode_t mode) {
  if (max_files >= 4) {
    m_total_bytes = bytes;
    m_max_files = max_files;
    m_n_files = std::min((size_t)4, m_max_files);
    ut_ad(m_n_files <= m_max_files);
    m_nth_file = 0;
    m_split_mode = mode;
  } else {
    m_split_mode = SPLIT_MODE_NONE;
  }
}
#endif /* UNIV_DEBUG */

dberr_t Split_writer::finish() {
  dberr_t err{DB_SUCCESS};
  /* Don't do anything for an empty file. */
  if (m_file_writer.get_row_count() > 0) {
    err = m_file_writer.flush();
    m_builder->m_files_vec.push_back(m_file_writer.m_file);
    /* The build is started as soon as one temporary file is created. */
    err = create_build_thread();
  }
  return err;
}

Split_writer::Split_writer(Builder *builder, size_t io_buffer_size,
                           size_t bytes_per_file)
    : m_builder(builder),
      m_file_writer(builder, io_buffer_size),
      m_bytes_per_file(bytes_per_file) {}

dberr_t Split_writer::write(const mrec_t *mrec, const ulint *offsets) {
  dberr_t err = DB_SUCCESS;
  if (is_file_full()) {
    m_file_writer.flush();
    ut_ad(m_file_writer.is_open());
    m_builder->m_files_vec.push_back(m_file_writer.m_file);
    ut_ad(m_builder->m_files_vec.back().is_open());
    /* The build is started as soon as one temporary file is created. */
    err = create_build_thread();
    if (err != DB_SUCCESS) {
      return err;
    }
#ifdef UNIV_DEBUG
    if (m_split_mode != SPLIT_MODE_NONE) {
      m_nth_file++;
      ut_ad(m_nth_file < m_max_files);
    }
#endif /* UNIV_DEBUG */
    m_file_writer.reset();
    err = m_file_writer.open();
    if (err != DB_SUCCESS) {
      return err;
    }
  }
  return m_file_writer.write(mrec, offsets);
}

dberr_t Split_writer::create_build_thread() {
  /* One file is ready, so start building the sub-tree. */
  auto observer = m_builder->get_observer();
  Btree_load *btr_load = ut::new_withkey<Btree_load>(
      ut::make_psi_memory_key(mem_key_ddl), m_builder->index(),
      m_builder->trx(), observer);
  if (btr_load == nullptr) {
    return DB_OUT_OF_MEMORY;
  }
  dberr_t err = btr_load->init();
  if (err != DB_SUCCESS) {
    return err;
  }
  m_builder->m_btree_loads.push_back(btr_load);
  const size_t btree_load_id = m_builder->m_btree_loads.size() - 1;
#ifdef UNIV_DEBUG
  ut_ad(btree_load_id < m_builder->m_files_vec.size());
  auto load_file = m_builder->m_files_vec[btree_load_id];
  ut_ad(load_file.m_size > 0);
#endif /* UNIV_DEBUG */

  m_builder->m_build_threads.emplace_back(
      std::thread(Builder::btree_subtree_build, m_builder, btree_load_id));
  return DB_SUCCESS;
}

dberr_t File_writer::write(const mrec_t *mrec, const ulint *offsets) {
  /* Refer to Merge_file_sort::Output_file::write() */
  ++m_n_wrote;

  size_t need;
  char prefix[sizeof(uint16_t)];

  /* Normalize extra_size. Value 0 signals "end of list". */
  const auto extra_size = rec_offs_extra_size(offsets);
  const auto nes = extra_size + 1;

  if (likely(nes < 0x80)) {
    need = 1;
    prefix[0] = (byte)nes;
  } else {
    need = 2;
    prefix[0] = (byte)(0x80 | (nes >> 8));
    prefix[1] = (byte)nes;
  }

  const auto rec_size = extra_size + rec_offs_data_size(offsets);
  ut_ad(rec_size == rec_offs_size(offsets));

  const byte *end_ptr = m_io_buffer.first + m_io_buffer.second;
  if (unlikely(m_ptr + rec_size + need >= end_ptr)) {
    const size_t n_write = m_ptr - m_io_buffer.first;
    const auto len = ut_uint64_align_down(n_write, IO_BLOCK_SIZE);
    auto err = ddl::pwrite(m_file.fd(), m_io_buffer.first, len, m_file.m_size);

    if (err != DB_SUCCESS) {
      return err;
    }

    ut_a(n_write >= len);
    const auto n_move = n_write - len;

    m_ptr = m_io_buffer.first;
    memmove(m_ptr, m_ptr + len, n_move);
    m_ptr += n_move;

    m_file.m_size += len;
  }

  memcpy(m_ptr, prefix, need);
  m_ptr += need;

  ut_a(m_ptr + rec_size <= m_io_buffer.first + m_io_buffer.second);

  memcpy(m_ptr, mrec - extra_size, rec_size);
  m_ptr += rec_size;

  return DB_SUCCESS;
}

dberr_t File_writer::flush() {
  /* There must always be room to write the end of list marker. */
  *m_ptr++ = 0;

  const auto len = ut_uint64_align_up(m_ptr - m_io_buffer.first, IO_BLOCK_SIZE);
  const auto err =
      ddl::pwrite(m_file.fd(), m_io_buffer.first, len, m_file.m_size);

  m_file.m_size += len;
  m_file.m_n_recs = m_n_wrote;

  /* Start writing the next page from the start. */
  m_ptr = m_io_buffer.first;

#ifdef UNIV_DEBUG
  if (err == DB_SUCCESS && m_builder->get_error() == DB_SUCCESS) {
    const size_t n = count_recs_in_file(m_n_wrote);
    ut_ad(n == m_n_wrote);
  }
#endif /* UNIV_DEBUG */

  return err;
}

#ifdef UNIV_DEBUG
Split_writer::split_mode_t g_bulk_load_split_mode_debug{
    Split_writer::SPLIT_MODE_NONE};
#endif /* UNIV_DEBUG */

dberr_t Builder::split_data_into_files(Builder *builder,
                                       Merge_cursor &merge_cursor) {
  const size_t io_buffer_size = builder->get_io_buffer_size();
  const size_t n_max_data = merge_cursor.get_max_data_size();
  if (n_max_data == 0) {
    return DB_SUCCESS;
  }
  const size_t n_threads = builder->get_n_threads();
  const size_t bytes_per_thread = (n_max_data + n_threads - 1) / n_threads;
  /* The vector should not be re-allocated.  So reserve enough capacity. */
  builder->m_files_vec.reserve(n_threads + 1);

  Split_writer split_writer(builder, io_buffer_size, bytes_per_thread);

#ifdef UNIV_DEBUG
  switch (g_bulk_load_split_mode_debug) {
    case Split_writer::SPLIT_MODE_NONE:
      break;
    case Split_writer::SPLIT_MODE_1:
      split_writer.set_split_mode(n_max_data, n_threads,
                                  Split_writer::SPLIT_MODE_1);
      break;
    case Split_writer::SPLIT_MODE_2:
      split_writer.set_split_mode(n_max_data, n_threads,
                                  Split_writer::SPLIT_MODE_2);
      break;
    default:
      ut_error;
  }
#endif /* UNIV_DEBUG */

  dberr_t err = split_writer.open();
  if (err != DB_SUCCESS) {
    builder->set_error(err);
    return builder->get_error();
  }

  err = merge_cursor.open();
  if (err != DB_SUCCESS) {
    builder->set_error(err);
    return builder->get_error();
  }

  if (builder->get_error() != DB_SUCCESS) {
    return builder->get_error();
  }

  ulint *offsets;
  const mrec_t *rec;
  while ((err = merge_cursor.fetch(rec, offsets)) == DB_SUCCESS) {
    err = split_writer.write(rec, offsets);
    ut_ad(err == DB_SUCCESS);
    err = merge_cursor.next();

    DBUG_EXECUTE_IF("ddl_read_failure", err = DB_CORRUPTION;);

    if (err != DB_SUCCESS) {
      break;
    }
  }
  if (err == DB_END_OF_INDEX) {
    err = DB_SUCCESS;
  }

  if (err == DB_SUCCESS) {
    ut_ad(builder->get_error() == DB_SUCCESS);
    err = split_writer.finish();
  }

  if (err != DB_SUCCESS) {
    builder->set_error(err);
  }

  return builder->get_error();
}

/** Context for copying cluster index row for the index to being created. */
struct Copy_ctx {
  /** Constructor.
  @param[in] row                Row to copy.
  @param[in,out] my_table       Server table definition.
  @param[in] thread_id          ID of current thread. */
  Copy_ctx(const Row &row, TABLE *my_table, size_t thread_id) noexcept
      : m_row(row), m_my_table(my_table), m_thread_id(thread_id) {}

  /** Row to copy. */
  const Row &m_row;

  /** MySQL table definition. */
  TABLE *m_my_table{};

  /** Number of columns to copy. */
  size_t m_n_fields{};

  /** Number of multivalue rows to add. */
  size_t m_n_mv_rows_to_add{};

  /** For storing multi value data. */
  const multi_value_data *m_mv{};

  /** Number of rows added or UNIV_NO_INDEX_VALUE if this is a multi-value
  index and current row has nothing valid to be indexed. */
  size_t m_n_rows_added{};

  /** Number of bytes copied. */
  size_t m_data_size{};

  /** Number of extra bytes used. */
  size_t m_extra_size{};

  /** Number of rows added during copy. */
  size_t m_n_recs{};

  /** ID of the current thread. */
  size_t m_thread_id{std::numeric_limits<size_t>::max()};
};

/** Generate the next document ID using a monotonic sequence. */
struct Gen_sequence : public ddl::Context::FTS::Sequence {
  /** Constructor.
  @param[in] current            Current (maximum document ID) (> 0). */
  explicit Gen_sequence(doc_id_t current) noexcept {
    ut_a(current > 0);
    m_doc_id = current;
  }

  /** Destructor. */
  ~Gen_sequence() noexcept override {}

  /** Get the next document ID.
  @return the current document ID and advance the sequence. */
  doc_id_t current() noexcept override { return m_doc_id; }

  /** Not supported.
  @param[in] dtuple             Row from which to fetch ID (ignored).
  @return the current document ID. */
  doc_id_t fetch(const dtuple_t *dtuple
                 [[maybe_unused]] = nullptr) noexcept override {
    ut_error;
  }

  /** Advance the document ID. */
  void increment() noexcept override {
    ++m_doc_id;
    ++m_n_generated;
  }

  /** @return the maximum document ID seen so far. */
  [[nodiscard]] doc_id_t max_doc_id() const noexcept override {
    return m_doc_id;
  }

  /** @return true, because we always generate the document ID. */
  [[nodiscard]] bool is_generated() const noexcept override { return true; }

  /** @return the number of document IDs generated. */
  doc_id_t generated_count() const noexcept override { return m_n_generated; }

  /** Number of document IDs generated. */
  doc_id_t m_n_generated{};
};

/** For loading an index from a sorted buffer. */
struct Key_sort_buffer_cursor : public Load_cursor {
  /** Constructor.
  @param[in,out] builder  Index builder.
  @param[in,out] key_buffer     Key buffer to load from. */
  Key_sort_buffer_cursor(Builder *builder, Key_sort_buffer *key_buffer) noexcept
      : Load_cursor(builder, nullptr), m_key_buffer(key_buffer) {}

  /** Open the cursor.
  @return DB_SUCCESS or error code. */
  dberr_t open() noexcept;

  /** Fetch the current row as a tuple. Note: Tuple columns are shallow
  copies.
  @param[out] dtuple          Row represented as a tuple.
  @return DB_SUCCESS, DB_END_OF_INDEX or error code. */
  [[nodiscard]] dberr_t fetch(dtuple_t *&dtuple) noexcept override;

  /** Move to the next record.
  @return DB_SUCCESS, DB_END_OF_INDEX or error code. */
  [[nodiscard]] dberr_t next() noexcept override;

 private:
  /** Tuple to return. */
  dtuple_t *m_dtuple{};

  /** Number of rows read from the key buffer. */
  size_t m_n_rows{};

  /** Row offsets. */
  ulint *m_offsets{};

  /** Heap for m_offsets and m_buf. */
  Scoped_heap m_heap{};

  /** Current merge row in m_aligned_buffer. */
  const mrec_t *m_mrec{};

  /** Key buffer to read from. */
  Key_sort_buffer *m_key_buffer{};
};

/** For loading a Btree index from a file. */
struct File_cursor : public Load_cursor {
  /** Constructor.
  @param[in] builder          The index build driver.
  @param[in] fd               File to read from.
  @param[in] buffer_size      IO buffer size to use for reads.
  @param[in] size             Size of the file in bytes.
  @param[in,out] stage        PFS observability.
  @param[in] total_rows       Total number of rows in file. */
  File_cursor(Builder *builder, os_fd_t fd, size_t buffer_size,
              os_offset_t size, Alter_stage *stage,
              uint64_t total_rows) noexcept;

  /** Constructor.
  @param[in] builder            The index build driver.
  @param[in] file               File to read from.
  @param[in] buffer_size        IO buffer size to use for reads.
  @param[in,out] stage          PFS observability. */
  File_cursor(Builder *builder, const file_t &file, size_t buffer_size,
              Alter_stage *stage) noexcept;

  /** Destructor. */
  ~File_cursor() override = default;

  /** Open the cursor.
  @return DB_SUCCESS or error code. */
  [[nodiscard]] dberr_t open() noexcept;

  [[nodiscard]] bool eof() const noexcept { return m_reader.eof(); }

  /** Fetch the current row as a tuple. Note: Tuple columns are shallow
  copies.
  @param[out] dtuple            Row represented as a tuple.
  @return DB_SUCCESS, DB_END_OF_INDEX or error code. */
  [[nodiscard]] dberr_t fetch(dtuple_t *&dtuple) noexcept override;

  /** Fetch the current row.
  @param[out] mrec              Current row.
  @param[out] offsets           Column offsets inside mrec.
  @return DB_SUCCESS, DB_END_OF_INDEX or error code. */
  [[nodiscard]] dberr_t fetch(const mrec_t *&mrec, ulint *&offsets) noexcept;

  /** Move to the next record.
  @return DB_SUCCESS, DB_END_OF_INDEX or error code. */
  [[nodiscard]] dberr_t next() noexcept override;

  size_t get_row_count() const { return m_n_rows; }

  size_t get_max_data_size() const { return m_reader.m_size; }

  /** Get the current offset from which next read will happen.
  @return the current offset from which next read will happen. */
  os_offset_t get_offset() const { return m_reader.get_offset(); }

 private:
  /** Prepare to fetch the current row.
  @return DB_SUCCESS, DB_END_OF_INDEX or error code. */
  [[nodiscard]] dberr_t fetch() noexcept;

 private:
  /** Instance ID. */
  size_t m_id{};

  /** File reader. */
  File_reader m_reader;

  /** Number of rows read the file. */
  uint64_t m_n_rows{};

  /** PFS monitoring. */
  Alter_stage *m_stage{};

  /** Total records in file. */
  const uint64_t m_total_rows{};

  friend struct Merge_cursor;
};

bool Load_cursor::duplicates_detected() const noexcept {
  return m_dup != nullptr && !m_dup->empty();
}

dberr_t File_reader::get_tuple(Builder *builder, mem_heap_t *heap,
                               dtuple_t *&dtuple) noexcept {
  dtuple = row_rec_to_index_entry_low(m_mrec, m_index, &m_offsets[0], heap);
  if (!builder->is_fts_index()) {
    return builder->dtuple_copy_blobs(dtuple, &m_offsets[0], m_mrec, heap);
  } else {
    return DB_SUCCESS;
  }
}

File_cursor::File_cursor(Builder *builder, os_fd_t fd, size_t buffer_size,
                         os_offset_t size, Alter_stage *stage,
                         uint64_t total_rows) noexcept
    : Load_cursor(builder, nullptr),
      m_reader(fd, builder->index(), buffer_size, size),
      m_stage(stage),
      m_total_rows(total_rows) {
  ut_a(m_reader.is_open());
}

File_cursor::File_cursor(Builder *builder, const file_t &file,
                         size_t buffer_size, Alter_stage *stage) noexcept
    : File_cursor(builder, file.fd(), buffer_size, file.m_size, stage,
                  file.m_n_recs) {}

dberr_t File_cursor::open() noexcept {
  m_tuple_heap.create(2048, UT_LOCATION_HERE);

  return m_reader.prepare();
}

dberr_t File_cursor::fetch() noexcept {
  m_tuple_heap.clear();

  if (m_stage != nullptr) {
    m_stage->inc(1);
  }

  return m_builder->get_error();
}

dberr_t File_cursor::fetch(dtuple_t *&dtuple) noexcept {
  m_err = fetch();

  if (unlikely(m_err != DB_SUCCESS)) {
    return m_err;
  }

  m_err = m_reader.get_tuple(m_builder, m_tuple_heap.get(), dtuple);

  ut_a(m_err != DB_END_OF_INDEX);

  return m_err;
}

dberr_t File_cursor::fetch(const mrec_t *&mrec, ulint *&offsets) noexcept {
  m_err = fetch();

  if (unlikely(m_err != DB_SUCCESS)) {
    return m_err;
  }

  mrec = m_reader.m_mrec;
  offsets = &m_reader.m_offsets[0];

  return DB_SUCCESS;
}

dberr_t File_cursor::next() noexcept {
  auto err = m_reader.next();

  if (unlikely(err != DB_END_OF_INDEX)) {
    m_err = err;
  }

  ++m_n_rows;

  return err;
}

Merge_cursor::File_readers Merge_cursor::file_readers() noexcept {
  File_readers file_readers{};

  for (auto file_cursor : m_cursors) {
    file_readers.push_back(&file_cursor->m_reader);
  }

  return file_readers;
}

Merge_cursor::Merge_cursor(Builder *builder, Dup *dup,
                           Alter_stage *stage) noexcept
    : Load_cursor(builder, dup),
      m_pq(Compare(builder->index(), dup)),
      m_stage(stage) {}

Merge_cursor::~Merge_cursor() noexcept {
  for (auto cursor : m_cursors) {
    ut::delete_(cursor);
  }
}

bool Merge_cursor::Compare::operator()(const File_cursor *lhs,
                                       const File_cursor *rhs) const noexcept {
  const auto &l = lhs->m_reader;
  const auto &r = rhs->m_reader;

  ut_a(l.m_index == r.m_index);

  auto cmp = cmp_rec_rec_simple(r.m_mrec, l.m_mrec, &r.m_offsets[0],
                                &l.m_offsets[0], r.m_index,
                                m_dup != nullptr ? m_dup->m_table : nullptr);

  /* Check for duplicates. */
  if (unlikely(cmp == 0 && m_dup != nullptr)) {
    m_dup->report(l.m_mrec, &l.m_offsets[0]);
  }

  return cmp < 0;
}

dberr_t Merge_cursor::add_file(const ddl::file_t &file,
                               size_t buffer_size) noexcept {
  ut_a(file.is_open());

  auto cursor = ut::new_withkey<File_cursor>(
      ut::make_psi_memory_key(mem_key_ddl), m_builder, file.fd(), buffer_size,
      file.m_size, m_stage, file.m_n_recs);

  if (cursor == nullptr) {
    m_err = DB_OUT_OF_MEMORY;
    return m_err;
  }

  m_cursors.push_back(cursor);

  return DB_SUCCESS;
}

dberr_t Merge_cursor::add_file(const ddl::file_t &file, size_t buffer_size,
                               os_offset_t offset) noexcept {
  auto err = add_file(file, buffer_size);

  if (err != DB_SUCCESS) {
    return err;
  } else {
    m_cursors.back()->m_reader.set_offset(offset);
    return DB_SUCCESS;
  }
}

void Merge_cursor::clear_eof() noexcept {
  ut_a(m_pq.empty());
  ut_a(!m_cursors.empty());
  ut_a(m_err == DB_END_OF_INDEX);

  m_err = DB_SUCCESS;

  for (auto cursor : m_cursors) {
    ut_a(cursor->m_err == DB_END_OF_INDEX);
    if (!cursor->m_reader.eof()) {
      cursor->m_err = DB_SUCCESS;
      m_pq.push(cursor);
    }
  }
}

dberr_t Merge_cursor::open() noexcept {
  ut_a(m_pq.empty());
  ut_a(!m_cursors.empty());

  /* Prime the priority queue and skip empty files. */
  for (auto cursor : m_cursors) {
    ut_a(cursor != nullptr);
    m_err = cursor->open();
    if (m_err == DB_SUCCESS) {
      m_pq.push(cursor);
    } else if (m_err != DB_END_OF_INDEX) {
      return m_err;
    }
  }

  m_err = m_pq.empty() ? DB_END_OF_INDEX : DB_SUCCESS;

  return m_err;
}

File_cursor *Merge_cursor::pop() noexcept {
  ut_a(!m_pq.empty());
  ut_a(m_cursor == nullptr);
  ut_a(m_err == DB_SUCCESS);

  auto cursor = m_pq.top();

  m_pq.pop();

  return cursor;
}

dberr_t Merge_cursor::fetch(dtuple_t *&dtuple) noexcept {
  const auto err = m_builder->check_state_of_online_build_log();

  if (err != DB_SUCCESS) {
    return err;
  } else {
    m_cursor = pop();
    return m_cursor->fetch(dtuple);
  }
}

dberr_t Merge_cursor::fetch(const mrec_t *&rec, ulint *&offsets) noexcept {
  const auto err = m_builder->check_state_of_online_build_log();

  if (err != DB_SUCCESS) {
    return err;
  } else {
    m_cursor = pop();
    return m_cursor->fetch(rec, offsets);
  }
}

dberr_t Merge_cursor::next() noexcept {
  ut_a(m_err == DB_SUCCESS);

  m_err = m_cursor->next();

  if (likely(m_err == DB_SUCCESS)) {
    m_pq.push(m_cursor);
    m_cursor = nullptr;
  } else if (unlikely(m_err == DB_END_OF_INDEX)) {
    m_cursor->m_err = m_err;
    m_cursor = nullptr;
    m_err = m_pq.empty() ? m_err : DB_SUCCESS;
  }

  return m_err;
}

uint64_t Merge_cursor::get_n_rows() const noexcept {
  uint64_t n_rows{};

  for (auto cursor : m_cursors) {
    n_rows += cursor->m_n_rows;
  }

  return n_rows;
}

void Builder::convert(const dict_index_t *clust_index,
                      const dfield_t *row_field, dfield_t *field, ulint len,
                      const page_size_t &page_size,
                      IF_DEBUG(bool is_sdi, ) mem_heap_t *heap) noexcept {
  ut_ad(DATA_MBMAXLEN(field->type.mbminmaxlen) > 1);
  ut_ad(DATA_MBMINLEN(field->type.mbminmaxlen) == 1);

  auto field_len = row_field->len;

  ut_a(field_len <= len);

  const auto buf = reinterpret_cast<byte *>(mem_heap_alloc(heap, len));

  if (row_field->ext) {
    const byte *field_data = static_cast<byte *>(dfield_get_data(row_field));
    ulint ext_len;

    ut_a(field_len >= BTR_EXTERN_FIELD_REF_SIZE);
    ut_ad(memcmp(field_data + field_len - BTR_EXTERN_FIELD_REF_SIZE,
                 field_ref_zero, BTR_EXTERN_FIELD_REF_SIZE));

    const auto data = lob::btr_copy_externally_stored_field_func(
        nullptr, clust_index, &ext_len, nullptr, field_data, page_size,
        field_len, IF_DEBUG(is_sdi, ) heap);

    ut_a(ext_len < len);

    memcpy(buf, data, ext_len);
    field_len = ext_len;
  } else {
    memcpy(buf, row_field->data, field_len);
  }

  memset(buf + field_len, 0x20, len - field_len);

  dfield_set_data(field, buf, len);
}

dberr_t Key_sort_buffer_cursor::open() noexcept {
  auto index = m_builder->index();
  const auto n_fields = dict_index_get_n_fields(index);

  {
    const auto i = 1 + REC_OFFS_HEADER_SIZE + n_fields;

    m_heap.create(1024 + i * sizeof(ulint), UT_LOCATION_HERE);

    const size_t n = i * sizeof(*m_offsets);

    m_offsets = reinterpret_cast<decltype(m_offsets)>(m_heap.alloc(n));

    m_offsets[0] = i;
  }

  m_offsets[1] = n_fields;

  DBUG_EXECUTE_IF("ddl_read_failure", m_err = DB_CORRUPTION; return m_err;);

  m_dtuple = dtuple_create(m_heap.get(), n_fields);

  dtuple_set_n_fields_cmp(m_dtuple, dict_index_get_n_unique_in_tree(index));

  m_tuple_heap.create(2048, UT_LOCATION_HERE);

  return DB_SUCCESS;
}

dberr_t Key_sort_buffer_cursor::fetch(dtuple_t *&dtuple) noexcept {
  m_tuple_heap.clear();

  if (m_n_rows >= m_key_buffer->size()) {
    return DB_END_OF_INDEX;
  }

  const auto fields = m_key_buffer->m_dtuples[m_n_rows];

  memcpy(m_dtuple->fields, fields, m_dtuple->n_fields * sizeof(dfield_t));

  /* "nullptr" - LOB pointers must be copied from the dtuple. */
  m_err = m_builder->dtuple_copy_blobs(m_dtuple, m_offsets, nullptr,
                                       m_tuple_heap.get());

  if (m_err == DB_SUCCESS) {
    dtuple = m_dtuple;
  }

  return m_err;
}

dberr_t Key_sort_buffer_cursor::next() noexcept {
  ++m_n_rows;
  return DB_SUCCESS;
}

Builder::Thread_ctx::Thread_ctx(size_t id, Key_sort_buffer *key_buffer) noexcept
    : m_id(id), m_key_buffer(key_buffer) {}

Builder::Thread_ctx::~Thread_ctx() noexcept {
  if (m_key_buffer != nullptr) {
    ut::delete_(m_key_buffer);
  }
  if (m_rtree_inserter != nullptr) {
    ut::delete_(m_rtree_inserter);
  }
  if (m_prev_fields != nullptr) {
    ut::free(m_prev_fields);
  }
  m_file.close();
}

Builder::Builder(ddl::Context &ctx, Loader &loader, size_t i) noexcept
    : m_id(i),
      m_ctx(ctx),
      m_loader(loader),
      m_index(ctx.m_indexes[m_id]),
      m_clust_dup({ctx.m_indexes[0], ctx.m_table, ctx.m_col_map, 0}) {
  m_tmpdir = thd_innodb_tmpdir(m_ctx.thd());
  m_sort_index = is_fts_index() ? m_ctx.m_fts.m_ptr->sort_index() : m_index;

  if (dict_table_is_comp(m_ctx.m_old_table) &&
      !dict_table_is_comp(m_ctx.m_new_table)) {
    m_conv_heap.create(sizeof(mrec_buf_t), UT_LOCATION_HERE);
  }
}

Builder::~Builder() noexcept {
  for (auto &file : m_files_vec) {
    file.close();
  }
  m_files_vec.clear();

  for (auto &thr : m_build_threads) {
    if (thr.joinable()) {
      thr.join();
    }
  }
  m_build_threads.clear();

  for (auto thread_ctx : m_thread_ctxs) {
    ut::delete_(thread_ctx);
  }

  m_thread_ctxs.clear();

  if (m_local_stage != nullptr) {
    m_local_stage->begin_phase_end();
    ut::delete_(m_local_stage);
  }

  for (auto btr_load : m_btree_loads) {
    ut::delete_(btr_load);
  }
  m_btree_loads.clear();
}

dberr_t Builder::check_state_of_online_build_log() noexcept {
  const auto err = m_ctx.check_state_of_online_build_log();

  if (err != DB_SUCCESS) {
    set_error(err);
  }

  return get_error();
}

dberr_t Builder::init(Cursor &cursor, size_t n_threads) noexcept {
  ut_a(m_thread_ctxs.empty());
  ut_a(get_state() == State::INIT);

  if (m_ctx.m_stage != nullptr) {
    ut_a(m_local_stage == nullptr);
    m_local_stage = ut::new_withkey<Alter_stage>(
        ut::make_psi_memory_key(mem_key_ddl), *m_ctx.m_stage);

    if (m_local_stage == nullptr) {
      return DB_OUT_OF_MEMORY;
    }

    /* Each builder is responsible for building a single index. */
    m_local_stage->begin_phase_read_pk(1);
  }

  auto buffer_size = m_ctx.scan_buffer_size(n_threads);
  auto create_thread_ctx = [&](size_t id, dict_index_t *index) -> dberr_t {
    auto key_buffer = ut::new_withkey<Key_sort_buffer>(
        ut::make_psi_memory_key(mem_key_ddl), index, buffer_size.first);

    if (key_buffer == nullptr) {
      return DB_OUT_OF_MEMORY;
    }

    auto thread_ctx = ut::new_withkey<Thread_ctx>(
        ut::make_psi_memory_key(mem_key_ddl), id, key_buffer);

    if (thread_ctx == nullptr) {
      ut::delete_(key_buffer);
      key_buffer = nullptr;
    }

    if (dict_table_is_comp(m_ctx.m_old_table) &&
        !dict_table_is_comp(m_ctx.m_new_table)) {
      thread_ctx->m_conv_heap.create(sizeof(mrec_buf_t), UT_LOCATION_HERE);
      if (thread_ctx->m_conv_heap.is_null()) {
        return DB_OUT_OF_MEMORY;
      }
    }

    m_thread_ctxs.push_back(thread_ctx);

    if (!thread_ctx->m_aligned_buffer.allocate(buffer_size.second)) {
      return DB_OUT_OF_MEMORY;
    }

    if (is_spatial_index()) {
      thread_ctx->m_rtree_inserter = ut::new_withkey<RTree_inserter>(
          ut::make_psi_memory_key(mem_key_ddl), m_ctx, index);

      if (thread_ctx->m_rtree_inserter == nullptr ||
          !thread_ctx->m_rtree_inserter->is_initialized()) {
        ut::delete_(key_buffer);
        return DB_OUT_OF_MEMORY;
      }
    }

    return DB_SUCCESS;
  };

  if (is_fts_index()) {
    auto &fts = m_ctx.m_fts;
    auto new_table = m_ctx.m_new_table;

    ut_a(fts.m_doc_id == nullptr);

    if (DICT_TF2_FLAG_IS_SET(new_table, DICT_TF2_FTS_ADD_DOC_ID)) {
      /* Generate the document ID. */
      doc_id_t current{};
      auto table = reinterpret_cast<dict_table_t *>(new_table);

      /* Fetch the FTS Doc ID from the row. */
      fts_get_next_doc_id(table, &current);

      fts.m_doc_id = ut::new_withkey<Gen_sequence>(
          ut::make_psi_memory_key(mem_key_ddl), current);
    } else {
      fts.m_doc_id = ut::new_withkey<Fetch_sequence>(
          ut::make_psi_memory_key(mem_key_ddl), fts.m_ptr->index());
    }

    if (fts.m_doc_id == nullptr) {
      set_error(DB_OUT_OF_MEMORY);
      set_next_state();
      return get_error();
    }

    ut_a(m_sort_index == fts.m_ptr->sort_index());

    fts.m_ptr->start_parse_threads(this);
  } else {
    ut_a(m_sort_index == m_index);
  }

  for (size_t i = 0; i < n_threads; ++i) {
    auto err = create_thread_ctx(i, m_sort_index);

    if (err != DB_SUCCESS) {
      set_error(err);
      set_next_state();
      return get_error();
    }
  }

  if (cursor.m_row_heap.get() == nullptr) {
    cursor.m_row_heap.create(sizeof(mrec_buf_t), UT_LOCATION_HERE);

    if (cursor.m_row_heap.get() == nullptr) {
      set_error(DB_OUT_OF_MEMORY);
      set_next_state();
      return get_error();
    }
  }

  if (is_skip_file_sort()) {
    ut_a(m_btree_loads.empty());
    auto observer = m_ctx.flush_observer();

    for (size_t i = 0; i < n_threads; ++i) {
      auto ptr = ut::new_withkey<Btree_load>(
          ut::make_psi_memory_key(mem_key_ddl), m_index, m_ctx.trx(), observer);
      if (ptr == nullptr) {
        set_error(DB_OUT_OF_MEMORY);
        set_next_state();
        return get_error();
      }
      dberr_t err = ptr->init();
      if (err != DB_SUCCESS) {
        set_error(err);
        set_next_state();
        return get_error();
      }
      m_btree_loads.push_back(ptr);
    }
  }

  set_next_state();
  ut_a(get_state() != State::INIT);

  return DB_SUCCESS;
}

void Builder::fts_add_doc_id(dfield_t *dst, const dict_field_t *src,
                             doc_id_t &write_doc_id) noexcept {
  auto &fts = m_ctx.m_fts;
  const auto doc_id = fts.m_doc_id->current();

  ut_a(doc_id <= 4294967295u);

  fts_write_doc_id(reinterpret_cast<byte *>(&write_doc_id), doc_id);

  dfield_set_data(dst, &write_doc_id, sizeof(write_doc_id));

  dst->type.len = src->col->len;
  dst->type.mtype = src->col->mtype;
  dst->type.prtype = src->col->prtype;
  dst->type.mbminmaxlen = DATA_MBMINMAXLEN(0, 0);
}

dberr_t Builder::get_virtual_column(Copy_ctx &ctx, const dict_field_t *ifield,
                                    dict_col_t *col, dfield_t *&src_field,
                                    size_t &mv_rows_added) noexcept {
  const auto n_added = mv_rows_added;
  auto v_col = reinterpret_cast<const dict_v_col_t *>(col);
  const auto clust_index = m_ctx.m_new_table->first_index();
  auto key_buffer = m_thread_ctxs[ctx.m_thread_id]->m_key_buffer;

  if (col->is_multi_value()) {
    ut_a(m_index->is_multi_value());

    auto &mv = ctx.m_mv;

    src_field = dtuple_get_nth_v_field(ctx.m_row.m_ptr, v_col->v_pos);

    if (ctx.m_n_mv_rows_to_add == 0) {
      auto p = m_v_heap.get();

      src_field = innobase_get_computed_value(
          ctx.m_row.m_ptr, v_col, clust_index, &p, key_buffer->heap(), ifield,
          m_ctx.thd(), ctx.m_my_table, m_ctx.m_old_table, nullptr, nullptr);

      m_v_heap.reset(p);

      if (src_field == nullptr) {
        ctx.m_n_rows_added = 0;
        return DB_COMPUTE_VALUE_FAILED;
      } else if (dfield_is_null(src_field)) {
        ctx.m_n_mv_rows_to_add = 1;
      } else if (src_field->len == UNIV_NO_INDEX_VALUE) {
        /* Nothing to be indexed */
        ctx.m_n_rows_added = UNIV_NO_INDEX_VALUE;
        return DB_FAIL;
      } else {
        mv = static_cast<const multi_value_data *>(src_field->data);

        ut_a(mv->num_v > n_added);
        ctx.m_n_mv_rows_to_add = mv->num_v - n_added;

        src_field->len = mv->data_len[n_added];
        src_field->data = const_cast<void *>(mv->datap[n_added]);
      }
    } else {
      src_field->data = const_cast<void *>(mv->datap[n_added]);
      src_field->len = mv->data_len[n_added];
    }
  } else {
    auto p = m_v_heap.get();

    src_field = innobase_get_computed_value(
        ctx.m_row.m_ptr, v_col, clust_index, &p, nullptr, ifield, m_ctx.thd(),
        ctx.m_my_table, m_ctx.m_old_table, nullptr, nullptr);

    m_v_heap.reset(p);

    if (src_field == nullptr) {
      ctx.m_n_rows_added = 0;
      return DB_COMPUTE_VALUE_FAILED;
    }
  }

  return DB_SUCCESS;
}

dberr_t Builder::copy_fts_column(Copy_ctx &ctx, dfield_t *field) noexcept {
  doc_id_t doc_id;
  auto &fts = m_ctx.m_fts;

  if (!fts.m_doc_id->is_generated()) {
    /* Fetch Doc ID if it already exists in the row, and not supplied by
    the caller. Even if the value column is nullptr, we still need to
    get the Doc ID to maintain the correct max Doc ID. */
    doc_id = fts.m_doc_id->fetch(ctx.m_row.m_ptr);

    if (unlikely(doc_id == 0)) {
      ctx.m_n_rows_added = 0;
      ib::warn(ER_IB_MSG_964) << "FTS Doc ID is zero. Record skipped";
      return DB_FAIL;
    }
  } else {
    doc_id = fts.m_doc_id->current();
  }

  ut_a(doc_id <= 4294967295u);

  if (unlikely(!dfield_is_null(field))) {
    auto ptr = ut::malloc_withkey(UT_NEW_THIS_FILE_PSI_KEY,
                                  sizeof(FTS::Doc_item) + field->len);
    auto doc_item = static_cast<FTS::Doc_item *>(ptr);
    auto value = static_cast<byte *>(ptr) + sizeof(*doc_item);

    memcpy(value, field->data, field->len);

    field->data = value;

    doc_item->m_field = field;
    doc_item->m_doc_id = doc_id;

    fts.m_ptr->enqueue(doc_item);
  }

  ctx.m_n_rows_added = 1;

  return DB_SUCCESS;
}

dberr_t Builder::copy_columns(Copy_ctx &ctx, size_t &mv_rows_added,
                              doc_id_t &write_doc_id) noexcept {
  auto &fts = m_ctx.m_fts;
  auto key_buffer = m_thread_ctxs[ctx.m_thread_id]->m_key_buffer;
  auto &fields = key_buffer->m_dtuples[key_buffer->size()];
  Scoped_heap &conv_heap = m_thread_ctxs[ctx.m_thread_id]->m_conv_heap;

  const dict_field_t *ifield = m_index->get_field(0);
  auto field = fields = key_buffer->alloc(ctx.m_n_fields);
  const auto page_size = dict_table_page_size(m_ctx.m_old_table);

  for (size_t i = 0; i < ctx.m_n_fields; ++i, ++field, ++ifield) {
    const auto col = ifield->col;
    const auto col_no = dict_col_get_no(col);

    /* Process the Doc ID column. */
    if (likely(fts.m_doc_id == nullptr || !fts.m_doc_id->is_generated() ||
               col_no != m_index->table->fts->doc_col || col->is_virtual())) {
      dfield_t *src_field;

      /* Use callback to get the virtual column value */
      if (col->is_virtual()) {
        auto err =
            get_virtual_column(ctx, ifield, col, src_field, mv_rows_added);
        if (err != DB_SUCCESS) {
          return err;
        }
      } else {
        src_field = dtuple_get_nth_field(ctx.m_row.m_ptr, col_no);
      }

      dfield_copy(field, src_field);

      /* Tokenize and process data for FTS */
      if (unlikely(is_fts_index())) {
        auto err = copy_fts_column(ctx, field);
        if (err != DB_SUCCESS) {
          return err;
        }
        continue;
      }

      if (field->len != UNIV_SQL_NULL && col->mtype == DATA_MYSQL &&
          col->len != field->len) {
        if (!conv_heap.is_null()) {
          convert(m_ctx.m_old_table->first_index(), src_field, field, col->len,
                  page_size,
                  IF_DEBUG(dict_table_is_sdi(m_ctx.m_old_table->id), )
                      conv_heap.get());
        } else {
          /* Field length mismatch should not happen when rebuilding
          redundant row format table. */
          ut_a(dict_table_is_comp(m_index->table));
        }
      }
    } else {
      fts_add_doc_id(field, ifield, write_doc_id);
    }

    ulint len = dfield_get_len(field);

    if (dfield_is_null(field)) {
      ut_a(!(col->prtype & DATA_NOT_NULL));
      continue;
    } else if (ctx.m_row.m_ext == nullptr) {
      /* Not an externally stored column. */
    } else if (m_index->is_clustered()) {
      /* Flag externally stored fields. */
      const byte *buf = row_ext_lookup(ctx.m_row.m_ext, col_no, &len);

      if (buf != nullptr) {
        ut_a(buf != field_ref_zero);
        if (i < dict_index_get_n_unique(m_index)) {
          dfield_set_data(field, buf, len);
        } else {
          dfield_set_ext(field);
          len = dfield_get_len(field);
        }
      }
    } else if (!col->is_virtual()) {
      /* Only non-virtual column are stored externally */
      const byte *buf = row_ext_lookup(ctx.m_row.m_ext, col_no, &len);

      if (buf != nullptr) {
        ut_a(buf != field_ref_zero);
        dfield_set_data(field, buf, len);
      }
    }

    /* If a column prefix index, take only the prefix */

    if (ifield->prefix_len > 0) {
      len = dtype_get_at_most_n_mbchars(
          col->prtype, col->mbminmaxlen, ifield->prefix_len, len,
          static_cast<char *>(dfield_get_data(field)));

      dfield_set_len(field, len);
    }

    ut_a(len <= col->len || DATA_LARGE_MTYPE(col->mtype) ||
         (col->mtype == DATA_POINT && len == DATA_MBR_LEN));

    auto fixed_len = ifield->fixed_len;

    if (fixed_len > 0 && !dict_table_is_comp(m_index->table) &&
        DATA_MBMINLEN(col->mbminmaxlen) != DATA_MBMAXLEN(col->mbminmaxlen)) {
      /* CHAR in ROW_FORMAT=REDUNDANT is always fixed-length, but
      in the temporary file it is variable-length for variable-length
      character sets. */
      fixed_len = 0;
    }

    if (fixed_len > 0) {
#ifdef UNIV_DEBUG
      const auto mbminlen = DATA_MBMINLEN(col->mbminmaxlen);
      const auto mbmaxlen = DATA_MBMAXLEN(col->mbminmaxlen);

      /* len should be between size calculated base on mbmaxlen and mbminlen
       */
      ut_a(len <= fixed_len);
      ut_a(!mbmaxlen || len >= mbminlen * (fixed_len / mbmaxlen));
      ut_a(!dfield_is_ext(field));
#endif /* UNIV_DEBUG */
    } else if (dfield_is_ext(field)) {
      ctx.m_extra_size += 2;
    } else if (len < 128 || !DATA_BIG_COL(col)) {
      ++ctx.m_extra_size;
    } else {
      /* For variable-length columns, we look up the maximum length from
      the column itself.  If this is a prefix index column shorter than
      256 bytes, this will waste one byte. */
      ctx.m_extra_size += 2;
    }
    ctx.m_data_size += len;
  }

  return DB_SUCCESS;
}

dberr_t Builder::copy_row(Copy_ctx &ctx, size_t &mv_rows_added) noexcept {
  auto key_buffer = m_thread_ctxs[ctx.m_thread_id]->m_key_buffer;
  const auto is_multi_value_index = m_index->is_multi_value();
  Scoped_heap &conv_heap = m_thread_ctxs[ctx.m_thread_id]->m_conv_heap;

  ut_a(ctx.m_n_rows_added == 0);

  if (unlikely(key_buffer->full())) {
    return DB_OVERFLOW;
  }

  // clang-format off
  DBUG_EXECUTE_IF(
      "ddl_buf_add_two",
      if (key_buffer->size()>= 2) {
        return DB_OVERFLOW;
      });
  // clang-format on

  /* Create spatial index should not come here. */
  ut_a(!is_spatial_index());

  doc_id_t write_doc_id{};

  for (;;) {
    if (unlikely(key_buffer->full())) {
      return ctx.m_n_rows_added == 0 ? DB_OVERFLOW : DB_SUCCESS;
    }

    // clang-format off
    DBUG_EXECUTE_IF(
        "ddl_add_multi_value",
        if (ctx.m_n_rows_added == 5) {
          return DB_OVERFLOW;
        });
    // clang-format on

    ctx.m_data_size = 0;
    ctx.m_n_fields = dict_index_get_n_fields(m_index);
    ctx.m_extra_size = UT_BITS_IN_BYTES(m_index->n_nullable);

    /* Note: field->data will point to a value on the
    stack: "write_doc_id" after dfield_set_data(). Because
    there is only one doc_id per row, it shouldn't matter.
    We allocate a new buffer before we leave the function
    later below. */

    auto err = copy_columns(ctx, mv_rows_added, write_doc_id);

    if (unlikely(err != DB_SUCCESS)) {
      return err;
    }

    /* If this is an FTS index, we already populated the sort buffer. */
    if (unlikely(is_fts_index())) {
      return DB_SUCCESS;
    }

#ifdef UNIV_DEBUG
    {
      ulint extra;
      auto fields = key_buffer->m_dtuples[key_buffer->size()];

      auto size = rec_get_serialize_size(m_index, fields, ctx.m_n_fields,
                                         nullptr, &extra, MAX_ROW_VERSION);

      ut_a(ctx.m_data_size + ctx.m_extra_size == size);
      ut_a(ctx.m_extra_size == extra);
    }
#endif /* UNIV_DEBUG */

    /* Add to the total size of the record in the output buffer,
    the encoded length of extra_size and the extra bytes (extra_size).
    See Key_sort_buffer::write() for the variable-length encoding
    of extra_size. */
    ctx.m_data_size +=
        (ctx.m_extra_size + 1) + ((ctx.m_extra_size + 1) >= 0x80);

    /* Record size can exceed page size while converting to redundant row
    format. There is an assert ut_ad(size < UNIV_PAGE_SIZE) in
    rec_offs_data_size(). It may hit the assert before attempting to
    insert the row. */
    if (unlikely(conv_heap.get() != nullptr &&
                 ctx.m_data_size > UNIV_PAGE_SIZE)) {
      ctx.m_n_rows_added = 0;
      return DB_TOO_BIG_RECORD;
    }

    if (unlikely(!key_buffer->will_fit(ctx.m_data_size))) {
      if (!is_multi_value_index) {
        ctx.m_n_rows_added = 0;
      }
      return DB_OVERFLOW;
    }

    key_buffer->deep_copy(ctx.m_n_fields, ctx.m_data_size);

    /* Note row added and all fields copied. */
    ctx.m_n_fields = 0;
    ++ctx.m_n_rows_added;

    conv_heap.clear();

    if (ctx.m_n_rows_added < ctx.m_n_mv_rows_to_add) {
      ut_a(is_multi_value_index);
      ++mv_rows_added;

      // clang-format off
      DBUG_EXECUTE_IF(
          "ddl_add_multi_value",
          if (mv_rows_added == 7) {
            return DB_OVERFLOW;
          });
      // clang-format on

      continue;
    }
    break;
  }

  if (is_multi_value_index) {
    mv_rows_added = 0;
  }

  ut_a(ctx.m_n_rows_added > 0 && ctx.m_n_rows_added != UNIV_NO_INDEX_VALUE);
  return DB_SUCCESS;
}

bool Builder::create_file(ddl::file_t &file) noexcept {
  ut_a(!file.is_open());

  if (ddl::file_create(&file, m_tmpdir)) {
    MONITOR_ATOMIC_INC(MONITOR_ALTER_TABLE_SORT_FILES);
    ut_a(file.is_open());
    return true;
  } else {
    return false;
  }
}

dberr_t Builder::append(ddl::file_t &file, IO_buffer io_buffer) noexcept {
  auto err =
      ddl::pwrite(file.fd(), io_buffer.first, io_buffer.second, file.m_size);

  if (err != DB_SUCCESS) {
    set_error(DB_TEMP_FILE_WRITE_FAIL);
    return get_error();
  } else {
    file.m_size += io_buffer.second;
    return err;
  }
}

dberr_t Builder::batch_insert(size_t thread_id,
                              Latch_release &&latch_release) noexcept {
  ut_a(is_spatial_index());

  auto rtree_inserter = m_thread_ctxs[thread_id]->m_rtree_inserter;
  const auto trx_id = m_ctx.m_trx->id;

  return rtree_inserter->batch_insert(trx_id, std::move(latch_release));
}

void Builder::batch_insert_deep_copy_tuples(size_t thread_id) noexcept {
  ut_a(is_spatial_index());
  auto rtree_inserter = m_thread_ctxs[thread_id]->m_rtree_inserter;
  return rtree_inserter->deep_copy_tuples();
}

dberr_t Builder::key_buffer_sort(size_t thread_id) noexcept {
  auto key_buffer = m_thread_ctxs[thread_id]->m_key_buffer;

  if (key_buffer->is_unique()) {
    auto index = key_buffer->m_index;
    Dup dup = {index, m_ctx.m_table, m_ctx.m_col_map, 0};

    key_buffer->sort(&dup);

    if (dup.m_n_dup > 0) {
      set_error(DB_DUPLICATE_KEY);
      return get_error();
    }
  } else {
    key_buffer->sort(nullptr);
  }

  return DB_SUCCESS;
}

dberr_t Builder::insert_direct(Cursor &cursor, size_t thread_id) noexcept {
  ut_ad(is_skip_file_sort());
  ut_a(!is_fts_index());
  ut_a(m_ctx.m_trx->id > 0);
  ut_a(!is_spatial_index());
  ut_a(!srv_read_only_mode);
  ut_a(!dict_index_is_ibuf(m_index));
  ut_a(m_index->is_clustered());

  {
    auto err = m_ctx.check_state_of_online_build_log();

    if (err != DB_SUCCESS) {
      set_error(err);
      return get_error();
    }
  }

  auto thread_ctx = m_thread_ctxs[thread_id];
  auto key_buffer = thread_ctx->m_key_buffer;
  auto btree_load = m_btree_loads[thread_id];
  btree_load->latch();

  /* Temporary file is not used. Insert sorted block directly into the index.*/
  {
    if (thread_ctx->m_prev_heap.is_null()) {
      thread_ctx->m_prev_heap.create(2048, UT_LOCATION_HERE);
    } else {
      thread_ctx->m_prev_heap.clear();
    }
    /* Copy the last row for duplicate key check. */
    auto p = thread_ctx->m_prev_heap.get();
    auto fields = key_buffer->back();

    if (thread_ctx->m_prev_fields == nullptr) {
      dberr_t err = thread_ctx->init(m_index);
      if (err != DB_SUCCESS) {
        return err;
      }
    }

    memcpy(thread_ctx->m_prev_fields, fields,
           m_index->n_fields * sizeof(dfield_t));

    for (size_t i = 0; i < m_index->n_fields; ++i) {
      dfield_dup(&thread_ctx->m_prev_fields[i], p);
    }
  }

  {
    Key_sort_buffer_cursor key_buffer_cursor(this, key_buffer);

    auto err = key_buffer_cursor.open();

    if (err == DB_SUCCESS) {
      err = btree_load->build(key_buffer_cursor);

      /* Load didn't return an internal error, check cursor for errors. */
      if (err == DB_SUCCESS) {
        err = key_buffer_cursor.get_err();
      }
    }

    if (cursor.eof() || err != DB_SUCCESS) {
      const bool is_subtree = true;
      err = btree_load->finish(err, is_subtree);
    } else {
      btree_load->release();
    }

    if (err != DB_SUCCESS) {
      return err;
    }
  }

  return DB_SUCCESS;
}

dberr_t Builder::batch_add_row(Row &row, size_t thread_id) noexcept {
  ut_a(is_spatial_index());

  auto key_buffer = m_thread_ctxs[thread_id]->m_key_buffer;
  auto rtree_inserter = m_thread_ctxs[thread_id]->m_rtree_inserter;

  ut_a(rtree_inserter->get_index() == key_buffer->m_index);

  /* If the geometry field is invalid, report error. */
  {
    const auto ind_field = key_buffer->m_index->get_field(0);
    const auto col = ind_field->col;
    auto col_no = dict_col_get_no(col);
    const auto dfield = dtuple_get_nth_field(row.m_ptr, col_no);

    if (dfield_is_null(dfield) ||
        dfield_get_len(dfield) < GEO_DATA_HEADER_SIZE) {
      return DB_CANT_CREATE_GEOMETRY_OBJECT;
    }
  }

  /* Note: This is a shallow copy. */
  rtree_inserter->add_to_batch(row.m_ptr, row.m_ext);

  return DB_SUCCESS;
}

dberr_t Builder::add_to_key_buffer(Copy_ctx &ctx,
                                   size_t &mv_rows_added) noexcept {
  const size_t old_mv_rows_added = mv_rows_added;
  auto err = copy_row(ctx, mv_rows_added);
  auto thread_ctx = m_thread_ctxs[ctx.m_thread_id];
  auto key_buffer = thread_ctx->m_key_buffer;

  if (unlikely(ctx.m_n_rows_added <= 0 || mv_rows_added != 0)) {
    if (unlikely(mv_rows_added != 0)) {
      /* This signals that a partial row was added to the key buffer
      due to reaching its size limit. We need to increment the
      file size by this amount */
      thread_ctx->m_n_recs += (mv_rows_added - old_mv_rows_added);
    }

    if (unlikely(err == DB_COMPUTE_VALUE_FAILED)) {
      set_error(err);
    }

    return err;
  }

  if (unlikely(ctx.m_n_rows_added == UNIV_NO_INDEX_VALUE)) {
    ut_a(err == DB_FAIL);

    /* Nothing to be indexed from current row, skip this index. */
    ut_a(key_buffer->m_index->is_multi_value());
    return DB_SUCCESS;
  }

  /* If we are creating FTS index, a single row can generate multiple
  records for a tokenized word. */
  thread_ctx->m_n_recs += ctx.m_n_rows_added;

  if (unlikely(err != DB_SUCCESS)) {
    ut_a(err == DB_TOO_BIG_RECORD || err == DB_COMPUTE_VALUE_FAILED);
    return err;
  }

  if (unlikely(is_fts_index())) {
    auto &fts = m_ctx.m_fts;

    err = fts.m_ptr->check_for_errors();

    if (unlikely(err != DB_SUCCESS)) {
      return err;
    }
  }

  if (is_skip_file_sort()) {
    ut_a(!key_buffer->empty());
    auto &fields = key_buffer->back();

    ut_ad(m_id == 0);
    ut_ad(key_buffer->is_clustered());

    /* Detect duplicates by comparing the current record with previous
    record.*/
    if (thread_ctx->m_prev_fields != nullptr &&
        Key_sort_buffer::compare(thread_ctx->m_prev_fields, fields,
                                 &m_clust_dup) == 0) {
      set_error(DB_DUPLICATE_KEY);
      return get_error();
    }

    if (thread_ctx->m_prev_fields == nullptr) {
      dberr_t err = thread_ctx->init(key_buffer->m_index);
      if (err != DB_SUCCESS) {
        set_error(err);
        return get_error();
      }
    }

    if (thread_ctx->m_prev_heap.is_null()) {
      thread_ctx->m_prev_heap.create(2048, UT_LOCATION_HERE);
    } else {
      thread_ctx->m_prev_heap.clear();
    }

    memcpy(thread_ctx->m_prev_fields, fields,
           m_index->n_fields * sizeof(dfield_t));

    auto p = thread_ctx->m_prev_heap.get();
    ut_ad(p != nullptr);

    for (size_t i = 0; i < m_index->n_fields; ++i) {
      dfield_dup(&thread_ctx->m_prev_fields[i], p);
    }
  }

  return DB_SUCCESS;
}

dberr_t Builder::bulk_add_row(Cursor &cursor, Row &row, size_t thread_id,
                              Latch_release &&latch_release) noexcept {
  /* Non-zero indicates this number of multi-value data have been added to the
  key buffer, and it should just continue from this point, otherwise, this is
  a new row to be added to the key buffer. For the output, non-zero means the
  new number of multi-value data which have been handled, while zero means
  this is a normal row or all data of the multi-value data in this row have
  been parsed. */
  size_t mv_rows_added{};
  auto thread_ctx = m_thread_ctxs[thread_id];
  auto key_buffer = thread_ctx->m_key_buffer;

  do {
    dberr_t err{DB_SUCCESS};
    Copy_ctx ctx{row, m_ctx.m_eval_table, thread_id};

    if (likely(!cursor.eof())) {
      err = add_to_key_buffer(ctx, mv_rows_added);

      if (err != DB_OVERFLOW) {
        return err;
      }
      /* Need to make room, flush the current key buffer to disk and retry. */
    } else if (unlikely(thread_ctx->m_n_recs == 0 && key_buffer->empty())) {
      /* Table is empty. */
      return DB_END_OF_INDEX;
    }

    if (unlikely(is_fts_index() &&
                 (cursor.eof() || !m_ctx.m_fts.m_doc_id->is_generated()))) {
      return DB_SUCCESS;
    }

    ut_ad(m_ctx.m_old_table == m_ctx.m_new_table
              ? !key_buffer->is_clustered()
              : (m_id == 0) == key_buffer->is_clustered());

    if (!key_buffer->empty()) {
      ut_a(err == DB_SUCCESS || err == DB_OVERFLOW);
      err = key_buffer_sort(thread_id);

      if (err != DB_SUCCESS) {
        set_error(err);
        return get_error();
      }

      if (is_skip_file_sort()) {
        if (!cursor.eof()) {
          /* Copy the row data and release any latches held by the parallel
          scan thread. Required for the log_free_check() during mtr.commit().
        */
          err = cursor.copy_row(thread_id, row);

          if (err != DB_SUCCESS) {
            set_error(err);
            return get_error();
          }

          err = latch_release();

          if (err != DB_SUCCESS) {
            set_error(err);
            return get_error();
          }
        }

        err = insert_direct(cursor, thread_id);

        key_buffer->clear();

        if (err != DB_SUCCESS) {
          set_error(err);
          return get_error();
        }

        m_ctx.note_max_trx_id(key_buffer->m_index);

        if (!cursor.eof()) {
          continue;
        }

        return DB_END_OF_INDEX;
      }
    }

    /* Fulltext index read threads should not write to the temporary file
    directly, @see copy_fts_column(). */
    if (unlikely(key_buffer->is_fts())) {
      return DB_SUCCESS;
    }

    IF_ENABLED("ddl_tmpfile_fail", set_error(DB_OUT_OF_MEMORY);
               return get_error();)

    IF_ENABLED("ddl_ins_spatial_fail", set_error(DB_FAIL); return get_error();)

    if (!thread_ctx->m_file.is_open()) {
      if (!create_file(thread_ctx->m_file)) {
        set_error(DB_IO_ERROR);
        return get_error();
      }
    }

    IF_ENABLED("ddl_write_failure", set_error(DB_TEMP_FILE_WRITE_FAIL);
               return get_error();)

    auto persistor = [&](IO_buffer io_buffer, os_offset_t &n) -> dberr_t {
      auto &file = thread_ctx->m_file;

      ut_a(!(file.m_size % IO_BLOCK_SIZE));

      if (n == 0) {
        n = ut_uint64_align_down(io_buffer.second, IO_BLOCK_SIZE);
      } else {
        ut_a(n == io_buffer.second);
        n = ut_uint64_align_up(io_buffer.second, IO_BLOCK_SIZE);
      }
      ut_a(n >= IO_BLOCK_SIZE);

      auto err = ddl::pwrite(file.fd(), io_buffer.first, n, file.m_size);

      if (err != DB_SUCCESS) {
        set_error(DB_TEMP_FILE_WRITE_FAIL);
        return get_error();
      }

      file.m_size += n;

      return DB_SUCCESS;
    };

    auto &file = thread_ctx->m_file;

    thread_ctx->m_offsets.push_back(file.m_size);

    auto io_buffer = thread_ctx->m_aligned_buffer.io_buffer();

    err = key_buffer->serialize(io_buffer, persistor);

    if (err != DB_SUCCESS) {
      return err;
    }

    key_buffer->clear();

    m_ctx.note_max_trx_id(key_buffer->m_index);

  } while (!cursor.eof());

  return DB_END_OF_INDEX;
}

dberr_t Builder::add_row(Cursor &cursor, Row &row, size_t thread_id,
                         Latch_release &&latch_release) noexcept {
  auto err = m_ctx.check_state_of_online_build_log();

  if (err != DB_SUCCESS) {
    set_error(err);
  } else if (is_spatial_index()) {
    if (!cursor.eof()) {
      err = batch_add_row(row, thread_id);
    }
  } else {
    err = bulk_add_row(cursor, row, thread_id, std::move(latch_release));
    clear_virtual_heap();
  }

  return err;
}

void Builder::copy_blobs(const dict_index_t *index, const mrec_t *mrec,
                         const ulint *offsets, const page_size_t &page_size,
                         dtuple_t *tuple,
                         IF_DEBUG(bool is_sdi, ) mem_heap_t *heap) noexcept {
  ut_ad(mrec == nullptr || rec_offs_any_extern(offsets));

  for (size_t i = 0; i < dtuple_get_n_fields(tuple); i++) {
    ulint len;
    const void *data;
    auto field = dtuple_get_nth_field(tuple, i);

    if (likely(!dfield_is_ext(field))) {
      continue;
    }

    ut_ad(!dfield_is_null(field));

    /* During the creation of a PRIMARY KEY, the table is X-locked, and we
    skip copying records that have been marked for deletion. Therefore,
    externally stored columns cannot possibly be freed between the time the
    BLOB pointers are read (Loader::*read()) and dereferenced (below). */
    if (mrec == nullptr) {
      const auto field_data = static_cast<byte *>(dfield_get_data(field));
      const auto field_len = dfield_get_len(field);

      ut_a(field_len >= BTR_EXTERN_FIELD_REF_SIZE);

      ut_a(memcmp(field_data + field_len - BTR_EXTERN_FIELD_REF_SIZE,
                  field_ref_zero, BTR_EXTERN_FIELD_REF_SIZE));

      data = lob::btr_copy_externally_stored_field_func(
          nullptr, index, &len, nullptr, field_data, page_size, field_len,
          IF_DEBUG(is_sdi, ) heap);
    } else {
      data = lob::btr_rec_copy_externally_stored_field_func(
          nullptr, index, mrec, offsets, page_size, i, &len, nullptr,
          IF_DEBUG(is_sdi, ) heap, true);
    }

    /* Because we have locked the table, any records
    written by incomplete transactions must have been
    rolled back already. There must not be any incomplete
    BLOB columns. */
    ut_a(data != nullptr);

    dfield_set_data(field, data, len);
  }
}

dberr_t Builder::dtuple_copy_blobs(dtuple_t *dtuple, ulint *offsets,
                                   const mrec_t *mrec,
                                   mem_heap_t *heap) noexcept {
  const auto old_index = m_ctx.m_old_table->first_index();

  if (m_index->is_clustered() && dict_index_is_online_ddl(old_index)) {
    auto err = row_log_table_get_error(old_index);

    if (err != DB_SUCCESS) {
      return err;
    }
  }

  if (dtuple->has_ext()) {
    ut_a(m_index->is_clustered());

    /* Off-page columns can be fetched safely when concurrent modifications
    to the table are disabled. (Purge can process delete-marked records, but
    Loader::*read() would have skipped them.)

    When concurrent modifications are enabled, Loader::*read() will only
    see rows from transactions that were committed before the ALTER TABLE
    started (REPEATABLE READ).

    Any modifications after the Loader::*read() scan will go through
    row_log_table_apply(). Any modifications to off-page columns will be
    tracked by row_log_table_blob_alloc() and row_log_table_blob_free(). */
    Builder::copy_blobs(old_index, mrec, offsets,
                        dict_table_page_size(m_ctx.m_old_table), dtuple,
                        IF_DEBUG(dict_index_is_sdi(m_index), ) heap);
  }

  ut_ad(dtuple_validate(dtuple));

  return DB_SUCCESS;
}

dberr_t Builder::check_duplicates(Thread_ctxs &dupcheck, Dup *dup) noexcept {
  Merge_cursor cursor(this, nullptr, m_local_stage);
  const auto buffer_size = m_ctx.scan_buffer_size(m_thread_ctxs.size());

  size_t n_files_to_check{};

  for (auto thread_ctx : dupcheck) {
    if (thread_ctx->m_offsets.size() == 1) {
      auto err = cursor.add_file(thread_ctx->m_file, buffer_size.second);

      if (err != DB_SUCCESS) {
        return err;
      }
      ++n_files_to_check;
    }
  }

  auto err = n_files_to_check > 0 ? cursor.open() : DB_END_OF_INDEX;

  if (err != DB_SUCCESS) {
    return err == DB_END_OF_INDEX ? DB_SUCCESS : err;
  }

  dtuple_t *dtuple{};
  auto prev_dtuple = dtuple;
  Scoped_heap prev_tuple_heap;

  /* For secondary indexes we have to compare all the columns for the index,
  this includes the cluster index primary key columns too. */
  Compare_key compare_key(m_index, dup, !m_sort_index->is_clustered());

  const auto n_compare = dict_index_get_n_unique_in_tree(m_index);

  prev_tuple_heap.create(2048, UT_LOCATION_HERE);

  while ((err = cursor.fetch(dtuple)) == DB_SUCCESS) {
    if (prev_dtuple != nullptr) {
      const auto cmp = compare_key(prev_dtuple->fields, dtuple->fields);

      if (cmp > 0) {
        /* Rows are out of order. */
        return DB_CORRUPTION;
      }
      if (cmp == 0) {
        return DB_DUPLICATE_KEY;
      }
    }

    prev_tuple_heap.clear();

    /* Do a deep copy. */
    prev_dtuple = dtuple_copy(dtuple, prev_tuple_heap.get());
    dtuple_set_n_fields_cmp(prev_dtuple, n_compare);

    for (size_t i = 0; i < n_compare; ++i) {
      dfield_dup(&prev_dtuple->fields[i], prev_tuple_heap.get());
    }

    err = cursor.next();

    if (err != DB_SUCCESS) {
      break;
    }
  }

  return err == DB_END_OF_INDEX ? DB_SUCCESS : err;
}

dberr_t Builder::btree_subtree_build(Builder *builder,
                                     size_t btree_load_id) noexcept {
  auto load_file = builder->m_files_vec[btree_load_id];
  Btree_load *btr_load = builder->get_btree_load(btree_load_id);
  Context &ctx = builder->ctx();
  const auto io_buffer_size = ctx.load_io_buffer_size(1);
  ut_ad(load_file.m_size > 0);
  ut_ad(load_file.is_open());
  File_cursor cursor(builder, load_file.fd(), io_buffer_size, load_file.m_size,
                     nullptr, load_file.m_n_recs);

  const size_t n_rows = load_file.m_n_recs;
  dberr_t cursor_err{DB_SUCCESS};
  dberr_t err{DB_SUCCESS};
  if (n_rows > 0) {
    err = cursor.open();

    if (err == DB_SUCCESS) {
      err = btr_load->build(cursor);
    } else if (err == DB_END_OF_INDEX) {
      err = DB_SUCCESS;
    }
    cursor_err = cursor.get_err();

    if (cursor_err == DB_END_OF_INDEX) {
      cursor_err = DB_SUCCESS;
    }

    ut_a(err != DB_SUCCESS || n_rows == cursor.get_row_count());
  }

  const bool subtree = true;

  /* First we check if the Btree loader returned an internal error.
  If loader succeeded then we check if the cursor returned an error. */
  err = btr_load->finish(err != DB_SUCCESS ? err : cursor_err, subtree);

  if (err != DB_SUCCESS) {
    builder->set_error(err);
  }

  return builder->get_error();
}

#ifdef UNIV_DEBUG
dberr_t Builder::check_file_order() {
  ut_ad(get_error() == DB_SUCCESS);
  const auto n_files = m_files_vec.size();
  dberr_t err = DB_SUCCESS;

  for (auto &file : m_files_vec) {
    ut_ad(check_file_is_sorted(file) == DB_SUCCESS);
  }

  if (n_files > 1) {
    for (Files_t::size_type i = 0, j = 1; j < n_files; ++i, ++j) {
      err = check_keys_disjoint(m_files_vec[i], m_files_vec[j]);
      ut_ad(err == DB_SUCCESS);
    }
  }

  return err;
}
#endif /* UNIV_DEBUG */

#ifdef UNIV_DEBUG
dberr_t Builder::check_file_is_sorted(const file_t &file) {
  const auto io_buffer_size = m_ctx.load_io_buffer_size(1);
  File_cursor l_fcursor(this, file, io_buffer_size, nullptr);
  File_cursor r_fcursor(this, file, io_buffer_size, nullptr);
  const mrec_t *l_rec, *r_rec;
  ulint *l_offsets, *r_offsets;
  int cmp;

  dberr_t l_err = DB_SUCCESS, r_err = DB_SUCCESS;

  l_err = l_fcursor.open();
  r_err = r_fcursor.open();
  ut_ad(l_err == r_err);
  if (l_err != DB_SUCCESS) {
    return l_err;
  }

  while (l_err == DB_SUCCESS) {
    l_err = l_fcursor.fetch(l_rec, l_offsets);
    r_err = r_fcursor.fetch(r_rec, r_offsets);
    ut_ad(l_err == r_err);

    if (l_err != DB_SUCCESS) {
      break;
    }

    cmp = cmp_rec_rec(l_rec, r_rec, l_offsets, r_offsets, m_index, false,
                      nullptr, false);
    ut_ad(cmp == 0);

    r_err = r_fcursor.next();

    if (r_err == DB_SUCCESS) {
      dberr_t err = r_fcursor.fetch(r_rec, r_offsets);
      ut_ad(err = DB_SUCCESS);

      cmp = cmp_rec_rec(l_rec, r_rec, l_offsets, r_offsets, m_index, false,
                        nullptr, false);
      ut_ad(cmp < 0);
    }

    l_err = l_fcursor.next();
    ut_ad(l_err == r_err);
  }

  return DB_SUCCESS;
}
#endif /* UNIV_DEBUG */

#ifdef UNIV_DEBUG
dberr_t Builder::check_keys_disjoint(const file_t &l_file,
                                     const file_t &r_file) {
  const auto io_buffer_size = m_ctx.load_io_buffer_size(1);
  File_cursor l_file_cursor(this, l_file, io_buffer_size, nullptr);
  File_cursor r_file_cursor(this, r_file, io_buffer_size, nullptr);

  dberr_t err = l_file_cursor.open();
  ut_ad(err == DB_SUCCESS);
  err = r_file_cursor.open();
  ut_ad(err == DB_SUCCESS);

  const mrec_t *l_rec, *r_rec;
  ulint *l_offsets, *r_offsets;
  err = r_file_cursor.fetch(r_rec, r_offsets);
  ut_ad(err == DB_SUCCESS);

  while ((err = l_file_cursor.fetch(l_rec, l_offsets)) == DB_SUCCESS) {
    int rec_order = cmp_rec_rec(l_rec, r_rec, l_offsets, r_offsets, m_index,
                                false, nullptr, false);
    ut_ad(rec_order < 0);
    err = l_file_cursor.next();
    if (err != DB_SUCCESS) {
      break;
    }
  }

  return DB_SUCCESS;
}
#endif /* UNIV_DEBUG */

dberr_t Builder::btree_build_mt() noexcept {
  for (auto &thr : m_build_threads) {
    thr.join();
  }
  m_build_threads.clear();
  const dberr_t err = get_error();

#ifdef UNIV_DEBUG
  if (err == DB_SUCCESS) {
    /* For debug build, close the file after doing some checks. */
    const dberr_t e = check_file_order();
    ut_ad(e == DB_SUCCESS);
  }
#endif /* UNIV_DEBUG */
  for (auto &file : m_files_vec) {
    file.close();
  }
  m_files_vec.clear();
  m_is_subtree = true;
  DEBUG_SYNC(m_ctx.thd(), "ddl_btree_build_interrupt");
  DBUG_EXECUTE_IF("btree_build_mt_force_error",
                  { set_error(DB_CANNOT_OPEN_FILE); });
  if (err == DB_SUCCESS) {
    set_state(State::FINISH);
  } else {
    set_next_state();
  }
  m_loader.add_task(Loader::Task{this});
  auto observer = m_ctx.m_trx->flush_observer;
  observer->flush();
  return get_error();
}

dberr_t Builder::full_sort() noexcept {
  Merge_cursor cursor(this, nullptr, nullptr);
  const auto io_buffer_size = m_ctx.load_io_buffer_size(m_thread_ctxs.size());

  size_t total_files{};
  dberr_t err{DB_SUCCESS};

  for (auto thread_ctx : m_thread_ctxs) {
    if (thread_ctx->m_file.is_closed()) {
      continue;
    }

    err = cursor.add_file(thread_ctx->m_file, io_buffer_size);

    if (err != DB_SUCCESS) {
      set_error(err);
      return get_error();
    }

    ut_a(thread_ctx->m_n_recs == thread_ctx->m_file.m_n_recs);

    ++total_files;
  }

  if (total_files == 1) {
    /* Should not split data into files for single threaded build. Also,
    split_data_into_files() spawns build thread for sub-trees which would
    cause race with single threaded build. */
    set_next_state(State::BTREE_BUILD);
  } else {
    split_data_into_files(this, cursor);
    set_next_state(State::BTREE_BUILD_MT);
  }

  if (err == DB_SUCCESS) {
    m_loader.add_task(Loader::Task{this});
  }

  return get_error();
}

dberr_t Builder::btree_build() noexcept {
  ut_a(!is_skip_file_sort());
  /* There should not be any build threads spawned to race with single
  threaded build. */
  ut_a(m_build_threads.empty());

  DEBUG_SYNC(m_ctx.thd(), "ddl_btree_build_interrupt");
  if (m_local_stage != nullptr) {
    m_local_stage->begin_phase_insert();
  }

  auto observer = m_ctx.m_trx->flush_observer;
  Dup dup = {m_index, m_ctx.m_table, m_ctx.m_col_map, 0};
  Merge_cursor cursor(this, &dup, m_local_stage);
  const auto io_buffer_size = m_ctx.load_io_buffer_size(m_thread_ctxs.size());

  uint64_t total_rows{};
  dberr_t err{DB_SUCCESS};

  for (auto thread_ctx : m_thread_ctxs) {
    if (!thread_ctx->m_file.is_open()) {
      continue;
    }

    err = cursor.add_file(thread_ctx->m_file, io_buffer_size);

    if (err != DB_SUCCESS) {
      set_error(err);
      return get_error();
    }

    ut_a(thread_ctx->m_n_recs == thread_ctx->m_file.m_n_recs);

    total_rows += thread_ctx->m_n_recs;
  }

  Btree_load btr_load(m_index, m_ctx.m_trx, observer);
  err = btr_load.init();

  dberr_t cursor_err{DB_SUCCESS};

  if (err == DB_SUCCESS && total_rows > 0) {
    err = cursor.open();

    if (err == DB_SUCCESS) {
      err = btr_load.build(cursor);
    } else if (err == DB_END_OF_INDEX) {
      err = DB_SUCCESS;
    }

    cursor_err = cursor.get_err();

    if (cursor_err == DB_END_OF_INDEX) {
      cursor_err = DB_SUCCESS;
    }

    ut_a(err != DB_SUCCESS || total_rows == cursor.get_n_rows());
  }

  m_is_subtree = false;
  /* First we check if the Btree loader returned an internal error.
  If loader succeeded then we check if the cursor returned an error. */
  err = btr_load.finish(err != DB_SUCCESS ? err : cursor_err, m_is_subtree);

  if (err != DB_SUCCESS) {
    set_error(err);
  }

  set_next_state();

  if (err == DB_SUCCESS) {
    m_loader.add_task(Loader::Task{this});
  }

  return get_error();
}

dberr_t Builder::create_merge_sort_tasks() noexcept {
  ut_a(!is_fts_index());
  ut_a(m_ctx.m_trx->id > 0);
  ut_a(!is_spatial_index());
  ut_a(!srv_read_only_mode);
  ut_a(!dict_index_is_ibuf(m_index));
  ut_a(get_state() == State::SETUP_SORT);

  ut_a(!m_thread_ctxs.empty());

  Thread_ctxs dupcheck{};
  size_t n_runs_to_merge{};
  Dup dup = {m_index, m_ctx.m_table, m_ctx.m_col_map, 0};

  for (auto thread_ctx : m_thread_ctxs) {
    ut_a(thread_ctx->m_file.m_n_recs == 0);
    thread_ctx->m_file.m_n_recs = thread_ctx->m_n_recs;

    n_runs_to_merge += thread_ctx->m_offsets.size();

    /* If there is a single file then there is nothing to merge and the
    file must already be sorted. */
    if (thread_ctx->m_offsets.size() < 2 && is_unique_index()) {
      /* We have to check these files using a merge cursor. */
      dupcheck.push_back(thread_ctx);
    }
  }

  if (!dupcheck.empty()) {
#ifdef UNIV_DEBUG
    {
      size_t n_empty{};
      size_t n_single{};
      size_t n_multiple{};

      for (auto thread_ctx : m_thread_ctxs) {
        if (thread_ctx->m_offsets.empty()) {
          ++n_empty;
        } else if (thread_ctx->m_offsets.size() == 1) {
          ++n_single;
        } else {
          ++n_multiple;
        }
      }
      ut_a(n_single + n_empty == dupcheck.size() ||
           (n_empty == 0 && n_single == dupcheck.size()) ||
           (n_single == 0 && n_multiple + n_empty == dupcheck.size()));
    }
#endif /* UNIV_DEBUG */
    auto err = check_duplicates(dupcheck, &dup);

    if (err != DB_SUCCESS) {
      return err;
    }
  }

  if (m_local_stage != nullptr) {
    m_local_stage->begin_phase_sort(log2(n_runs_to_merge));
  }

  ut_a(m_n_sort_tasks == 0);

  /* Set the next state so that the tasks are executed in the context
  of the next state. */
  set_next_state();

  m_n_sort_tasks.fetch_add(m_thread_ctxs.size(), std::memory_order_relaxed);

  for (auto thread_ctx : m_thread_ctxs) {
    if (thread_ctx->m_key_buffer != nullptr) {
      /* Free up memory that is not going to be used anymore. */
      ut::delete_(thread_ctx->m_key_buffer);
      thread_ctx->m_key_buffer = nullptr;
    }

    m_loader.add_task(Loader::Task{this, thread_ctx->m_id});
  }

  return DB_SUCCESS;
}

void Builder::write_redo(const dict_index_t *index) noexcept {
  ut_ad(!index->table->is_temporary());

  mtr_t mtr;
  mtr.start();

  byte *log_ptr{};

  if (mlog_open(&mtr, 11 + 8, log_ptr)) {
    log_ptr = mlog_write_initial_log_record_low(MLOG_INDEX_LOAD, index->space,
                                                index->page, log_ptr, &mtr);

    mach_write_to_8(log_ptr, index->id);
    mlog_close(&mtr, log_ptr + 8);
  }

  mtr.commit();
}

dberr_t Builder::fts_sort_and_build() noexcept {
  ut_a(is_fts_index());

  auto &fts = m_ctx.m_fts;
  auto err = fts.m_ptr->insert(this);

  for (auto thread_ctx : m_thread_ctxs) {
    thread_ctx->m_file.close();
  }

  if (fts.m_ptr != nullptr) {
    ut::delete_(fts.m_ptr);
    fts.m_ptr = nullptr;
  }

  if (err != DB_SUCCESS) {
    set_error(err);
    set_next_state();
    return get_error();
  } else {
    set_state(State::FINISH);
    return DB_SUCCESS;
  }
}

dberr_t Builder::finalize() noexcept {
  ut_a(m_ctx.m_need_observer);
  ut_a(get_state() == State::FINISH);

  for (auto &thr : m_build_threads) {
    if (thr.joinable()) {
      thr.join();
    }
  }
  m_build_threads.clear();

  auto observer = m_ctx.m_trx->flush_observer;

  observer->flush();

  dberr_t err = DB_SUCCESS;
  auto new_table = m_ctx.m_new_table;
  auto space_id =
      new_table != nullptr ? new_table->space : dict_sys_t::s_invalid_space_id;

  Clone_notify notifier(Clone_notify::Type::SPACE_ALTER_INPLACE_BULK, space_id,
                        false);
  if (notifier.failed()) {
    err = DB_ERROR;
  }

  if (err == DB_SUCCESS) {
    write_redo(m_index);

    DEBUG_SYNC(m_ctx.thd(), "row_log_apply_before");

    err = row_log_apply(m_ctx.m_trx, m_index, m_ctx.m_table, m_local_stage);

    DEBUG_SYNC(m_ctx.thd(), "row_log_apply_after");
  }

  if (err != DB_SUCCESS) {
    set_error(err);
  }

  return err;
}

dberr_t Builder::merge_sort(size_t thread_id) noexcept {
  dberr_t err{DB_SUCCESS};
  auto thread_ctx = m_thread_ctxs[thread_id];

  /* If there is a single (or no) list of rows then there is nothing to merge
  and the file must already be sorted. */
  if (thread_ctx->m_file.is_open() && thread_ctx->m_offsets.size() > 1) {
    Merge_file_sort::Context merge_ctx;
    Dup dup = {m_index, m_ctx.m_table, m_ctx.m_col_map, 0};

    merge_ctx.m_dup = &dup;
    merge_ctx.m_stage = m_local_stage;
    merge_ctx.m_file = &thread_ctx->m_file;
    merge_ctx.m_n_threads = m_thread_ctxs.size();

    Merge_file_sort merge_file_sort{&merge_ctx};

    err = merge_file_sort.sort(this, thread_ctx->m_offsets);

    ut_a(err != DB_SUCCESS ||
         merge_file_sort.get_n_rows() == thread_ctx->m_n_recs);
  }

  const auto n = m_n_sort_tasks.fetch_sub(1, std::memory_order_seq_cst);
  ut_a(n > 0);

  if (err != DB_SUCCESS) {
    set_error(err);
    set_next_state();
  } else if (n == 1 && get_state() == State::SORT) {
    set_next_state();
    m_loader.add_task(Loader::Task{this});
  }

  return get_error();
}

dberr_t Builder::setup_sort() noexcept {
  ut_a(!is_skip_file_sort());
  ut_a(get_state() == State::SETUP_SORT);

  DEBUG_SYNC(m_ctx.thd(), "ddl_merge_sort_interrupt");

  const auto err = create_merge_sort_tasks();

  if (err != DB_SUCCESS) {
    set_error(err);
    return get_error();
  } else {
    return DB_SUCCESS;
  }
}

dberr_t Builder::merge_subtrees() noexcept {
  auto guard = create_scope_guard([this]() {
    get_observer()->flush();
    for (auto btr_load : m_btree_loads) {
      ut::delete_(btr_load);
    }
    m_btree_loads.clear();
  });

  if (is_fts_index()) {
    return DB_SUCCESS;
  }

  if (strcmp(m_index->name(), FTS_DOC_ID_INDEX_NAME) == 0) {
    return DB_SUCCESS;
  }

  if (!is_skip_file_sort() && !m_is_subtree) {
    return DB_SUCCESS;
  }

  Btree_load::Merger merger(m_btree_loads, m_index, m_ctx.m_trx);
  auto err = merger.merge(true);

  if (err != DB_SUCCESS) {
    set_error(err);
    return get_error();
  }
  return err;
}

dberr_t Builder::finish() noexcept {
  dberr_t err{DB_SUCCESS};

  for (auto &thr : m_build_threads) {
    if (thr.joinable()) {
      thr.join();
    }
  }
  m_build_threads.clear();

  err = merge_subtrees();
  if (err != DB_SUCCESS) {
    set_error(err);
    return get_error();
  }

  if (get_error() != DB_SUCCESS) {
    set_next_state();
    return get_error();
  }

  ut_a(m_n_sort_tasks == 0);
  ut_a(get_state() == State::FINISH);

  for (auto thread_ctx : m_thread_ctxs) {
    thread_ctx->m_file.close();
  }

  if (get_error() != DB_SUCCESS || !m_ctx.m_online) {
    /* Do not apply any online log. */
  } else if (m_ctx.m_old_table != m_ctx.m_new_table) {
    ut_a(!m_index->online_log);
    ut_a(m_index->online_status == ONLINE_INDEX_COMPLETE);

    auto observer = m_ctx.m_trx->flush_observer;
    observer->flush();

  } else {
    err = finalize();

    if (err != DB_SUCCESS) {
      set_error(err);
    }
  }

#ifdef UNIV_DEBUG
  if (err == DB_SUCCESS) {
    ut_ad(btr_validate_index(m_index, m_ctx.m_trx, false));
  }
#endif /* UNIV_DEBUG */

  set_next_state();
  return get_error();
}

void Builder::fallback_to_single_thread() noexcept {
  for (size_t i = 0; i < m_thread_ctxs.size(); ++i) {
    if (i > 0) {
      ut::delete_(m_thread_ctxs[i]);
      m_thread_ctxs[i] = nullptr;
    }
  }
  m_thread_ctxs.resize(1);
}

void Builder::set_next_state() noexcept {
  dberr_t err = get_error();
  if (err != DB_SUCCESS) {
    set_state(State::ERROR);
    return;
  }

  switch (get_state()) {
    case State::INIT:
      set_state(State::ADD);
      break;

    case State::ADD:
      if (is_fts_index()) {
        set_state(State::FTS_SORT_AND_BUILD);
      } else if (!is_skip_file_sort()) {
        set_state(State::SETUP_SORT);
      } else {
        set_state(State::FINISH);

        break;
      }
      break;

    case State::SETUP_SORT:
      set_state(State::SORT);
      break;

    case State::SORT:
      set_state(State::FULL_SORT);
      break;

    case State::FULL_SORT:
      set_state(State::BTREE_BUILD_MT);
      break;

    case State::BTREE_BUILD_MT:
      /* fall through. */
    case State::BTREE_BUILD:
      set_state(State::FINISH);
      break;

    case State::FTS_SORT_AND_BUILD:
      set_state(State::FINISH);
      break;

    case State::FINISH:
      set_state(State::STOP);
      break;

    case State::STOP:
    case State::ERROR:
      ut_error;
  }
}

dberr_t Loader::Task::operator()() noexcept {
  dberr_t err;

  switch (m_builder->get_state()) {
    case Builder::State::SETUP_SORT:
      ut_a(!m_builder->is_skip_file_sort());
      err = m_builder->setup_sort();
      break;

    case Builder::State::SORT:
      ut_a(!m_builder->is_skip_file_sort());
      err = m_builder->merge_sort(m_thread_id);
      break;

    case Builder::State::FULL_SORT:
      /* Data is sorted across files. */
      err = m_builder->full_sort();
      break;

    case Builder::State::BTREE_BUILD_MT:
      /* Multi-threaded btree build. */
      err = m_builder->btree_build_mt();
      break;

    case Builder::State::BTREE_BUILD:
      err = m_builder->btree_build();
      break;

    case Builder::State::FTS_SORT_AND_BUILD:
      ut_a(m_builder->is_fts_index());
      err = m_builder->fts_sort_and_build();
      break;

    case Builder::State::FINISH:
      err = m_builder->finish();
      break;

    case Builder::State::ERROR:
      err = m_builder->get_error();
      break;

    case Builder::State::ADD:
    case Builder::State::INIT:
    case Builder::State::STOP:
    default:
      ut_error;
  }

  return err;
}

// void file_destroy(ddl::file_t *file) noexcept {
// Builder::file_destroy(file);
// }

#ifdef UNIV_DEBUG
size_t File_writer::count_recs_in_file(size_t n_rows) {
  ut_ad(m_builder->get_error() == DB_SUCCESS);
  File_cursor cursor(m_builder, m_file.fd(), m_buffer_size, m_file.m_size,
                     nullptr, m_file.m_n_recs);
  ut_ad(cursor.get_offset() == 0);
  dberr_t err = cursor.open();
  if (err == DB_SUCCESS) {
    dtuple_t *tuple;
    while ((err = cursor.fetch(tuple)) == DB_SUCCESS) {
      err = cursor.next();
      if (err != DB_SUCCESS) {
        break;
      }
    }
  }
  const size_t N = cursor.get_row_count();
  if (n_rows != N) {
    ut_ad(n_rows == N || err != DB_SUCCESS);
  }
  return n_rows;
}
#endif /* UNIV_DEBUG */

size_t Merge_cursor::get_max_data_size() const noexcept {
  size_t N = 0;
  for (auto cursor : m_cursors) {
    N += cursor->get_max_data_size();
  }
  return N;
}

}  // namespace ddl

#ifdef UNIV_DEBUG
void set_bulk_load_split_mode(size_t split_mode) {
  switch (split_mode) {
    case 0:
      ddl::g_bulk_load_split_mode_debug = ddl::Split_writer::SPLIT_MODE_NONE;
      break;
    case 1:
      ddl::g_bulk_load_split_mode_debug = ddl::Split_writer::SPLIT_MODE_1;
      break;
    case 2:
      ddl::g_bulk_load_split_mode_debug = ddl::Split_writer::SPLIT_MODE_2;
      break;
  }
}
#endif /* UNIV_DEBUG */

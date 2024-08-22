/*****************************************************************************

Copyright (c) 2020, 2024, Oracle and/or its affiliates.

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

/** @file ddl/ddl0merge.cc
 DDL merge sort implementation.
Created 2020-11-01 by Sunny Bains. */

#include "ddl0impl-buffer.h"
#include "ddl0impl-builder.h"
#include "ddl0impl-merge.h"
#include "trx0trx.h"
#include "ut0stage.h"

namespace ddl {

/** Cursor for merging blocks from the same file. */
struct Merge_file_sort::Cursor : private ut::Non_copyable {
  /** Constructor.
  @param[in,out] builder        Index builder instance.
  @param[in] file               File to iterate over.
  @param[in,out] dup            For reporting duplicates.
  @param[in,out] stage          PFS staging. */
  Cursor(Builder *builder, file_t *file, Dup *dup, Alter_stage *stage) noexcept
      : m_file(file), m_cursor(builder, dup, stage) {
    ut_a(m_file->m_size > 0);
    ut_a(m_file->m_n_recs > 0);
#ifdef POSIX_FADV_SEQUENTIAL
    /* The input file will be read sequentially, starting from the
    beginning and the middle. In Linux, the POSIX_FADV_SEQUENTIAL
    affects the entire file. Each block will be read exactly once. */
    {
      const auto flags = POSIX_FADV_SEQUENTIAL | POSIX_FADV_NOREUSE;
      posix_fadvise(m_file->m_file.get(), 0, 0, flags);
    }
#endif /* POSIX_FADV_SEQUENTIAL */
  }

  /** Prepare the cursor for reading.
  @param[in] ranges             Ranges to merge in a pass.
  @param[in] buffer_size        IO Buffer size to use for reading.
  @return DB_SUCCESS or error code. */
  [[nodiscard]] dberr_t prepare(const Ranges &ranges,
                                size_t buffer_size) noexcept;

  /** Fetch the next record.
  @param[out] mrec              Row read from the file.
  @param[out] offsets           Column offsets inside mrec.
  @return DB_SUCCESS or error code. */
  [[nodiscard]] dberr_t fetch(const mrec_t *&mrec, ulint *&offsets) noexcept;

  /** Move to the next record.
  @return DB_SUCCESS, DB_END_OF_INDEX or error code. */
  [[nodiscard]] dberr_t next() noexcept;

  /** Move the cursor to the start of the new records lists to merge.
  @param[in] range              Seek to these offsets.
  @return DB_SUCCESS or error code. */
  [[nodiscard]] dberr_t seek(const Ranges &range) noexcept;

  /** @return the number of active readers. */
  [[nodiscard]] size_t size() const noexcept { return m_cursor.size(); }

 private:
  /** File to iterate over. */
  file_t *m_file{};

  /** Cursor to use for the merge. */
  Merge_cursor m_cursor;
};

/** For writing out the merged rows. */
struct Merge_file_sort::Output_file : private ut::Non_copyable {
  /** The transaction interrupted check is expensive, we check after this
  many page writes. */
  static constexpr uint64_t TRX_INTERRUPTED_CHECK = 64;

  /** Constructor.
  @param[in,out] ctx            DDL context.
  @param[in] file               File to write to.
  @param[in] io_buffer          Buffer to store records and write to file. */
  Output_file(ddl::Context &ctx, const Unique_os_file_descriptor &file,
              IO_buffer io_buffer) noexcept
      : m_ctx(ctx), m_file(file), m_buffer(io_buffer), m_ptr(m_buffer.first) {}

  /** Destructor. */
  ~Output_file() = default;

  /** Initialize the duplicate check infrastructure.
  @param[in] index              DDL index. */
  void init(const dict_index_t *index) noexcept {
    const auto n_fields = dict_index_get_n_fields(index);
    const auto i = 1 + REC_OFFS_HEADER_SIZE + n_fields;

    ut_a(m_offsets.empty());

    m_offsets.resize(i);

    m_offsets[0] = i;
    m_offsets[1] = n_fields;
  }

  /** Write the row to the buffer. If the buffer fills up write the
  buffer to the output file.
  @param[in] mrec               Row to write.
  @param[in] offsets            Column offsets in row.
  @param[in,out] dup            For duplicate checks.
  @return DB_SUCCESS or error code. */
  [[nodiscard]] dberr_t write(const mrec_t *mrec, const ulint *offsets,
                              Dup *dup) noexcept;

  /** Write end of block marker and flush buffer to disk.
  @return DB_SUCCESS or error code. */
  [[nodiscard]] dberr_t flush() noexcept;

  /** @return the current size of the output file in bytes. */
  [[nodiscard]] os_offset_t get_size() const { return m_offset; }

  /** @return number of rows in the output file. */
  [[nodiscard]] uint64_t get_n_rows() const noexcept { return m_n_rows; }

 private:
  /** @return the number of bytes copied so far. */
  [[nodiscard]] size_t copied() const noexcept {
    return std::ptrdiff_t(m_ptr - m_buffer.first);
  }

  /** Do a duplicate check against the incoming record.
  @param[in] mrec               Row to write.
  @param[in] offsets            Column offsets in row.
  @param[in,out] dup            For duplicate checks. */
  void duplicate_check(const mrec_t *mrec, const ulint *offsets,
                       Dup *dup) noexcept;

 private:
  /** Limit is [start, end]. */
  using Offsets = std::vector<ulint, ut::allocator<ulint>>;

  /** DDL context. */
  ddl::Context &m_ctx;

  /** File to write to. */
  const Unique_os_file_descriptor &m_file;

  /** Buffer to write to (output buffer). */
  IO_buffer m_buffer;

  /** Start writing new bytes at this offset. */
  byte *m_ptr{};

  /** Total number of bytes written. */
  os_offset_t m_offset{};

  /** Offsets of the last inserted row. */
  Offsets m_offsets{};

  /** Pointer to last record in the output buffer. */
  mrec_t *m_last_mrec{};

  /** Number of rows read/written. */
  uint64_t m_n_rows{};

  /** Counter for checking trx_is_interrupted. */
  uint64_t m_interrupt_check{};
};

dberr_t Merge_file_sort::Cursor::prepare(const Ranges &ranges,
                                         size_t buffer_size) noexcept {
  for (size_t i = 0; i < ranges.size() - 1; ++i) {
    if (const auto err =
            m_cursor.add_file(*m_file, buffer_size, {ranges[i], ranges[i + 1]});
        err != DB_SUCCESS && err != DB_END_OF_INDEX) {
      return err;
    }
  }
  return m_cursor.open();
}

dberr_t Merge_file_sort::Cursor::fetch(const mrec_t *&mrec,
                                       ulint *&offsets) noexcept {
  return m_cursor.fetch(mrec, offsets);
}

dberr_t Merge_file_sort::Cursor::next() noexcept { return m_cursor.next(); }

dberr_t Merge_file_sort::Cursor::seek(const Ranges &ranges) noexcept {
  auto file_readers = m_cursor.file_readers();
  ut_a(file_readers.size() == N_WAY_MERGE);
  ut_a(ranges.size() == N_WAY_MERGE + 1);

  dberr_t err{DB_ERROR_UNSET};
  bool can_seek = false;

  for (size_t i = 0; i < ranges.size() - 1; ++i) {
    if (ranges[i] == m_file->m_size) {
      err = DB_END_OF_INDEX;
    } else {
      err = file_readers[i]->read({ranges[i], ranges[i + 1]});
      if (err == DB_SUCCESS) {
        can_seek = true;
      }
    }
  }

  if (can_seek) {
    m_cursor.clear_eof();
    err = DB_SUCCESS;
  }

  return err;
}

void Merge_file_sort::Output_file::duplicate_check(const mrec_t *mrec,
                                                   const ulint *offsets,
                                                   Dup *dup) noexcept {
  if (m_offsets.empty()) {
    const auto n_fields = dict_index_get_n_fields(dup->m_index);
    const auto n = 1 + REC_OFFS_HEADER_SIZE + n_fields;

    m_offsets.resize(n);

    m_offsets[0] = n;
    m_offsets[1] = n_fields;

  } else if (m_last_mrec != nullptr && m_offsets[2] != 0) {
    auto last_mrec = m_last_mrec;
    size_t extra_size = *last_mrec++;

    if (extra_size >= 0x80) {
      extra_size = (extra_size & 0x7f) << 8;
      extra_size |= *last_mrec++;
    }

    /* Normalize extra_size. Above, value 0 signals "end of list". */
    --extra_size;

    last_mrec += extra_size;

    auto cmp = cmp_rec_rec_simple(mrec, last_mrec, offsets, &m_offsets[0],
                                  dup->m_index, dup->m_table);

    if (cmp <= 0) {
      ut_a(cmp == 0);
      dup->report(mrec, offsets);
    }
  }

  memcpy(&m_offsets[2], &offsets[2], (m_offsets.size() - 2) * sizeof(*offsets));
}

dberr_t Merge_file_sort::Output_file::write(const mrec_t *mrec,
                                            const ulint *offsets,
                                            Dup *dup) noexcept {
  if (unlikely(dup != nullptr)) {
    duplicate_check(mrec, offsets, dup);
  }

  ++m_n_rows;

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

  if (unlikely(m_ptr + rec_size + need >= m_buffer.first + m_buffer.second)) {
    const size_t n_write = m_ptr - m_buffer.first;
    const auto len = ut_uint64_align_down(n_write, IO_BLOCK_SIZE);
    if (len != 0) {
      auto err = ddl::pwrite(m_file.get(), m_buffer.first, len, m_offset);

      if (err != DB_SUCCESS) {
        return err;
      }

      ut_a(n_write >= len);
      const auto n_move = n_write - len;

      m_ptr = m_buffer.first;
      memmove(m_ptr, m_ptr + len, n_move);
      m_ptr += n_move;

      m_offset += len;
    }

    if (unlikely(m_ptr + rec_size + need >= m_buffer.first + m_buffer.second)) {
      // Should be caught earlier
      ut_d(ut_error);
      ut_o(return DB_TOO_BIG_RECORD);
    }
  }

  m_last_mrec = m_ptr;

  memcpy(m_ptr, prefix, need);
  m_ptr += need;

  ut_a(m_ptr + rec_size <= m_buffer.first + m_buffer.second);

  memcpy(m_ptr, mrec - extra_size, rec_size);
  m_ptr += rec_size;

  return DB_SUCCESS;
}

dberr_t Merge_file_sort::Output_file::flush() noexcept {
  /* There must always be room to write the end of list marker. */
  ut_a(copied() < m_buffer.second);

  /* End of the block marker */
  *m_ptr++ = 0;

  /* Reset the duplicate checks because we are going to start merging
  a new range after the flush. */
  m_last_mrec = nullptr;

  if (!m_offsets.empty()) {
    memset(&m_offsets[2], 0x0, (m_offsets.size() - 2) * sizeof(m_offsets[0]));
  }

  const auto len = ut_uint64_align_up(m_ptr - m_buffer.first, IO_BLOCK_SIZE);
  const auto err = ddl::pwrite(m_file.get(), m_buffer.first, len, m_offset);

  m_offset += len;

  /* Start writing the next page from the start. */
  m_ptr = m_buffer.first;

#ifdef UNIV_DEBUG
  if (Sync_point::enabled(m_ctx.thd(), "ddl_merge_sort_interrupt")) {
    ut_a(err == DB_SUCCESS);
    m_interrupt_check = TRX_INTERRUPTED_CHECK;
  }
#endif

  if (err == DB_SUCCESS && !(m_interrupt_check++ % TRX_INTERRUPTED_CHECK) &&
      m_ctx.is_interrupted()) {
    return DB_INTERRUPTED;
  } else {
    return err;
  }
}

Merge_file_sort::Ranges Merge_file_sort::next_ranges(
    Merge_offsets &offsets) noexcept {
  Ranges ranges(N_WAY_MERGE + 1, m_merge_ctx->m_file->m_size);
  for (size_t i = 0; i < N_WAY_MERGE; i++) {
    if (!offsets.empty()) {
      ranges[i] = offsets.front();
      offsets.pop_front();
    }
  }

  if (!offsets.empty()) {
    ranges.back() = offsets.front();
  }

  return ranges;
}

dberr_t Merge_file_sort::merge_rows(Cursor &cursor,
                                    Output_file &output_file) noexcept {
  dberr_t err;
  ulint *offsets{};
  const mrec_t *mrec{};

  while ((err = cursor.fetch(mrec, offsets)) == DB_SUCCESS) {
    /* If we are simply appending from a single partition then enable duplicate
    key checking for the write phase. */
    auto dup = cursor.size() == 0 ? m_merge_ctx->m_dup : nullptr;

    err = output_file.write(mrec, offsets, dup);

    if (unlikely(err != DB_SUCCESS)) {
      break;
    }

    err = cursor.next();

    if (unlikely(err != DB_SUCCESS)) {
      break;
    }
  }

  return err;
}

dberr_t Merge_file_sort::merge_ranges(Cursor &cursor, Merge_offsets &offsets,
                                      Output_file &output_file,
                                      size_t buffer_size) noexcept {
  auto ranges = next_ranges(offsets);
  auto err = cursor.prepare(ranges, buffer_size);

  if (err != DB_SUCCESS) {
    return err;
  }

  m_next_offsets.push_back(output_file.get_size());

  do {
    err = merge_rows(cursor, output_file);

    if (unlikely(err == DB_END_OF_INDEX)) {
      err = output_file.flush();
      m_next_offsets.push_back(output_file.get_size());
    }

    if (unlikely(err != DB_SUCCESS)) {
      return err;
    }

    if (unlikely(m_merge_ctx->m_dup != nullptr &&
                 m_merge_ctx->m_dup->m_n_dup > 0)) {
      return DB_DUPLICATE_KEY;
    }

    ranges = next_ranges(offsets);

    /* Reposition the merge cursor on the new range to merge next. */
  } while ((err = cursor.seek(ranges)) == DB_SUCCESS);

  m_next_offsets.pop_back();

  return err == DB_END_OF_INDEX ? DB_SUCCESS : err;
}

dberr_t Merge_file_sort::sort(Builder *builder,
                              Merge_offsets &offsets) noexcept {
  ut_a(m_merge_ctx->m_dup != nullptr);

  auto &ctx = builder->ctx();
  auto file = m_merge_ctx->m_file;
  const auto n_buffers = (m_merge_ctx->m_n_threads * N_WAY_MERGE) + 1;
  const auto io_buffer_size = ctx.merge_io_buffer_size(n_buffers);

  ut::unique_ptr_aligned<byte[]> aligned_buffer =
      ut::make_unique_aligned<byte[]>(ut::make_psi_memory_key(mem_key_ddl),
                                      UNIV_SECTOR_SIZE, io_buffer_size);

  if (!aligned_buffer) {
    return DB_OUT_OF_MEMORY;
  }

  /* Buffer for writing the merged rows to the output file. */
  IO_buffer io_buffer{aligned_buffer.get(), io_buffer_size};

  /* This is the output file for the first pass. */
  auto tmpfd = ddl::file_create_low(builder->tmpdir());

  if (tmpfd.is_open()) {
    MONITOR_ATOMIC_INC(MONITOR_ALTER_TABLE_SORT_FILES);
  } else {
    return DB_OUT_OF_RESOURCES;
  }

  dberr_t err{DB_SUCCESS};

  /* Merge until there is a single list of rows in the file. */
  while (offsets.size() > 1) {
    Output_file output_file(ctx, tmpfd, io_buffer);
    Cursor cursor{builder, file, m_merge_ctx->m_dup, m_merge_ctx->m_stage};

    err = merge_ranges(cursor, offsets, output_file, io_buffer_size);

    m_n_rows = output_file.get_n_rows();

    if (unlikely(err != DB_SUCCESS)) {
      break;
    }

#ifdef UNIV_DEBUG
    ib::info() << " Merge sort pass completed. Input file: "
               << file->m_file.get() << " Output file: " << tmpfd.get()
               << " New offsets: " << m_next_offsets.size()
               << " thread_id: " << std::this_thread::get_id();
#endif /* UNIV_DEBUG */

    /* Swap the input file with the output file and repeat. */
    tmpfd.swap(file->m_file);
    std::swap(offsets, m_next_offsets);

    ut_a(m_next_offsets.empty());

    if (!offsets.empty()) {
      file->m_size = output_file.get_size();
    }
  }

  ut_a(err != DB_SUCCESS || file->m_n_recs == m_n_rows);

  return err;
}

}  // namespace ddl

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

/** @file ddl/ddl0file-reader.cc
 For reading the DDL temporary files.
 Created 2020-11-01 by Sunny Bains. */

#include "ddl0impl-file-reader.h"
#include "dict0dict.h"
#include "rem/rec.h"
#include "rem0rec.h"

namespace ddl {

dberr_t File_reader::prepare() noexcept {
  ut_a(m_ptr == nullptr);
  ut_a(m_mrec == nullptr);
  ut_a(m_buffer_size > 0);
  ut_a(m_bounds.first == nullptr && m_bounds.second == nullptr);

  if (end_of_range()) {
    return DB_END_OF_INDEX;
  }

  m_aligned_buffer = ut::make_unique_aligned<byte[]>(
      ut::make_psi_memory_key(mem_key_ddl), UNIV_SECTOR_SIZE, m_buffer_size);

  if (!m_aligned_buffer) {
    return DB_OUT_OF_MEMORY;
  }

  m_io_buffer = {m_aligned_buffer.get(), m_buffer_size};

  m_mrec = m_io_buffer.first;
  m_bounds.first = m_io_buffer.first;
  m_bounds.second = m_bounds.first + m_io_buffer.second,

  m_ptr = m_io_buffer.first;

  const auto n_fields = dict_index_get_n_fields(m_index);
  const auto n = 1 + REC_OFFS_HEADER_SIZE + n_fields;

  ut_a(m_field_offsets.empty());

  m_field_offsets.resize(n);

  m_field_offsets[0] = n;
  m_field_offsets[1] = n_fields;

  ut_a(m_aux_buf == nullptr);
  m_aux_buf = ut::new_arr_withkey<byte>(ut::make_psi_memory_key(mem_key_ddl),
                                        ut::Count{UNIV_PAGE_SIZE_MAX / 2});

  if (m_aux_buf == nullptr) {
    return DB_OUT_OF_MEMORY;
  }

  if (const auto err = seek(); err != DB_SUCCESS) {
    return err;
  }

  /* Position m_mrec on the first record. */
  return next();
}

dberr_t File_reader::seek() noexcept {
  ut_a(m_range.second > m_range.first);

  const auto len = std::min(m_io_buffer.second, m_range.second - m_range.first);
  const auto err =
      ddl::pread(m_file.get(), m_io_buffer.first, len, m_range.first);

  if (err == DB_SUCCESS) {
    /* Fetch and advance to the next record. */
    m_ptr = m_io_buffer.first;
  }

  return err;
}

dberr_t File_reader::read(const Range &range) noexcept {
  ut_a(range.first < range.second);
  m_range = range;

  /* Read the page in the file buffer */
  const auto err = seek();

  if (err == DB_SUCCESS) {
    /* Position m_mrec on the first record. */
    return next();
  } else {
    return err;
  }
}

dberr_t File_reader::read_next() noexcept {
  m_range.first = m_range.first + m_io_buffer.second;
  return seek();
}

dberr_t File_reader::next() noexcept {
  ut_a(m_ptr >= m_bounds.first && m_ptr < m_bounds.second);

  size_t extra_size = *m_ptr++;

  if (extra_size == 0) {
    /* Mark as end of range. */
    m_range.first = m_range.second;
    return DB_END_OF_INDEX;
  }

  if (extra_size >= 0x80) {
    /* Read another byte of extra_size. */
    if (m_ptr >= m_bounds.second) {
      const auto err = read_next();
      if (err != DB_SUCCESS) {
        return err;
      }
    }

    extra_size = (extra_size & 0x7f) << 8;
    extra_size |= *m_ptr++;
  }

  /* Normalize extra_size. Above, value 0 signals "end of list". */
  --extra_size;

  /* Read the extra bytes. */

  auto rec = const_cast<byte *>(m_ptr);

  if (unlikely(rec + extra_size >= m_bounds.second)) {
    /* The record spans two blocks. Copy the entire record to the auxiliary
    buffer and handle this as a special case. */
    const auto partial_size = std::ptrdiff_t(m_bounds.second - m_ptr);

    ut_a(static_cast<size_t>(partial_size) < UNIV_PAGE_SIZE_MAX);

    rec = m_aux_buf;

    /* Copy the partial record from the file buffer to the aux buffer. */
    memcpy(rec, m_ptr, partial_size);

    {
      const auto err = read_next();

      if (err != DB_SUCCESS) {
        return err;
      }
    }

    {
      /* Copy the remaining record from the file buffer to the aux buffer. */
      const auto len = extra_size - partial_size;

      memcpy(rec + partial_size, m_ptr, len);

      m_ptr += len;
    }

    rec_deserialize_init_offsets(rec + extra_size, m_index,
                                 &m_field_offsets[0]);

    const auto data_size = rec_offs_data_size(&m_field_offsets[0]);

    /* These overflows should be impossible given that records are much
    smaller than either buffer, and the record starts near the beginning
    of each buffer. */
    ut_a(m_ptr + data_size < m_bounds.second);
    ut_a(extra_size + data_size < UNIV_PAGE_SIZE_MAX);

    /* Copy the data bytes. */
    memcpy(rec + extra_size, m_ptr, data_size);

    m_ptr += data_size;

  } else {
    rec_deserialize_init_offsets(rec + extra_size, m_index,
                                 &m_field_offsets[0]);

    const auto data_size = rec_offs_data_size(&m_field_offsets[0]);

    ut_a(extra_size + data_size < UNIV_PAGE_SIZE_MAX);

    const auto required = extra_size + data_size;

    /* Check if the record fits entirely in the block. */
    if (unlikely(m_ptr + required >= m_bounds.second)) {
      /* The record spans two blocks. Copy prefix it to buf. */
      const auto partial_size = std::ptrdiff_t(m_bounds.second - m_ptr);

      rec = m_aux_buf;

      memcpy(rec, m_ptr, partial_size);

      /* We cannot invoke rec_offs_make_valid() here, because there
      are no REC_N_NEW_EXTRA_BYTES between extra_size and data_size.
      Similarly, rec_offs_validate() would fail, because it invokes
      rec_get_status(). */
      ut_d(m_field_offsets[3] = (ulint)m_index);
      ut_d(m_field_offsets[2] = (ulint)rec + extra_size);

      {
        const auto err = read_next();

        if (err != DB_SUCCESS) {
          return err;
        }
      }

      {
        /* Copy the rest of the record. */
        const auto len = extra_size + data_size - partial_size;

        memcpy(rec + partial_size, m_ptr, len);
        m_ptr += len;
      }
    } else {
      m_ptr += required;
    }
  }

  ++m_n_rows_read;

  m_mrec = rec + extra_size;

  return DB_SUCCESS;
}

}  // namespace ddl

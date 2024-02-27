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

/** @file ddl/ddl0buffer.cc
 DDL key buffer implementation.
Created 2020-11-01 by Sunny Bains. */

#include "ddl0impl-buffer.h"
#include "ddl0impl-compare.h"

namespace ddl {

/** Merge sort a given array.
@param[in, out] arr              Array to sort.
@param[in, out] aux_arr          Auxiliary space to use for sort.
@param[in] low                   First element (inclusive).
@param[in] high                  Number of elements to sort from low.
@param[in] compare               Function to compare two elements. */
template <typename T, typename Compare>
inline void merge_sort(T *arr, T *aux_arr, const size_t low, const size_t high,
                       Compare compare) {
  ut_a(low < high);

  if (low == high - 2) {
    if (compare(arr[low], arr[high - 1]) > 0) {
      aux_arr[low] = arr[low];
      arr[low] = arr[high - 1];
      arr[high - 1] = aux_arr[low];
    }
    return;
  } else if (unlikely(low == high - 1)) {
    return;
  }

  auto l = low;
  auto h = (low + high) >> 1;
  const auto m = h;

  merge_sort(arr, aux_arr, low, m, compare);
  merge_sort(arr, aux_arr, m, high, compare);

  for (auto i = low; i < high; ++i) {
    if (l >= m) {
      aux_arr[i] = arr[h++];
    } else if (h >= high) {
      aux_arr[i] = arr[l++];
    } else if (compare(arr[l], arr[h]) > 0) {
      aux_arr[i] = arr[h++];
    } else {
      aux_arr[i] = arr[l++];
    }
  }

  memcpy(arr + low, aux_arr + low, (high - low) * sizeof(*arr));
}

Key_sort_buffer::Key_sort_buffer(dict_index_t *index, size_t size) noexcept
    : m_index(index), m_buffer_size(size) {
  m_max_tuples = m_buffer_size / std::max(ulint{1}, m_index->get_min_size());
  m_dtuples.resize(m_max_tuples);
  m_heap = mem_heap_create(1024, UT_LOCATION_HERE);
}

void Key_sort_buffer::deep_copy(size_t n_fields, size_t data_size) noexcept {
  auto field = m_dtuples[m_n_tuples++];

  do {
    dfield_dup(field++, m_heap);
  } while (--n_fields > 0);

  m_total_size += data_size;
}

void Key_sort_buffer::clear() noexcept {
  m_n_tuples = 0;
  m_total_size = 0;
  mem_heap_empty(m_heap);
}

void Key_sort_buffer::sort(Dup *dup) noexcept {
  ut_ad(!dict_index_is_spatial(m_index));

  DTuples aux{};

  aux.resize(m_n_tuples);

  /* Compare all the columns of the key to preserve order in the index. */
  Compare_key compare_key(m_index, dup, true);

  merge_sort(&m_dtuples[0], &aux[0], 0, m_n_tuples, compare_key);
}

dberr_t Key_sort_buffer::serialize(IO_buffer io_buffer,
                                   Function persist) noexcept {
  std::pair<const byte *, const byte *> bounds{
      io_buffer.first, io_buffer.first + io_buffer.second};

  /* Points past the filled part of buffer */
  auto ptr = io_buffer.first;

  /* Move as many blocks as possible out of the buffer by persisting them */
  auto write_buffer = [io_buffer, &ptr, persist]() -> dberr_t {
    auto persist_buffer = io_buffer;

    const size_t buf_filled = ptr - io_buffer.first;
    persist_buffer.second = ut_uint64_align_down(buf_filled, IO_BLOCK_SIZE);

    auto err = persist(persist_buffer);

    if (err != DB_SUCCESS) {
      return err;
    }

    const os_offset_t bytes_written = persist_buffer.second;
    const auto bytes_remaining = buf_filled - bytes_written;

    ptr = io_buffer.first;
    memmove(ptr, ptr + bytes_written, bytes_remaining);
    ptr += bytes_remaining;

    /* Remaining contents of buffer must be less than the needed alignment.*/
    ut_ad(bytes_remaining < IO_BLOCK_SIZE);

    return DB_SUCCESS;
  };

  size_t i{};
  const auto n_fields = dict_index_get_n_fields(m_index);

  for (const auto &fields : m_dtuples) {
    if (i++ >= m_n_tuples) {
      break;
    }

    ulint extra_size;

    const auto size = rec_get_serialize_size(m_index, fields, n_fields, nullptr,
                                             &extra_size, MAX_ROW_VERSION);

    {
      const auto rec_size = size + extra_size + 2;

      if (rec_size >= io_buffer.second) {
        /* Single row doesn't fit into our IO buffer. */
        return DB_TOO_BIG_RECORD;
      }

      ut_a(size >= extra_size);
    }

    size_t need;
    char prefix[sizeof(uint16_t)];

    /* Encode extra_size + 1 */
    if (extra_size + 1 < 0x80) {
      need = 1;
      prefix[0] = (byte)(extra_size + 1);
    } else {
      need = 2;
      ut_a((extra_size + 1) < 0x8000);
      prefix[0] = (byte)(0x80 | ((extra_size + 1) >> 8));
      prefix[1] = (byte)(extra_size + 1);
    }

    const auto rec_size = need + size;

    /* If serialized record won't fit in buffer, make space in the buffer
     by persisting a portion of it */
    if (unlikely(ptr + rec_size > bounds.second)) {
      const auto err = write_buffer();
      if (err != DB_SUCCESS) {
        return err;
      }
      ut_a(ptr + rec_size <= bounds.second);
    }

    memcpy(ptr, prefix, need);
    ptr += need;

    {
      const auto p = ptr + extra_size;
      rec_serialize_dtuple(p, m_index, fields, n_fields, nullptr,
                           MAX_ROW_VERSION);
    }

    ptr += size;
  }

  ut_a(ptr <= bounds.second);

  /* At this point there is some data remaining in buffer. It needs
  to be persisted, followed by zero-filled region at least 1 byte in
  length and aligned to IO_BLOCK_SIZE ("end-of-chunk" marker) */
  size_t buf_filled = ptr - io_buffer.first;
  size_t aligned_size = ut_uint64_align_up(buf_filled + 1, IO_BLOCK_SIZE);

  /* Check if adding the end-of-chunk marker would overflow the buffer */
  if (aligned_size > io_buffer.second) {
    /* If so, persist a portion of the buffer to free it up */
    const auto err = write_buffer();
    if (err != DB_SUCCESS) {
      return err;
    }
    ut_ad(ptr > io_buffer.first);
    ut_a(size_t(ptr - io_buffer.first) < IO_BLOCK_SIZE);
    /* After writing buffer contains [0, IO_BLOCK_SIZE) bytes,
    so aligning it to  IO_BLOCK_SIZE guarantees space for
    end-of-chunk marker */
    aligned_size = IO_BLOCK_SIZE;
  }

  /* Append the end-of-chunk marker. */
  ut_a(ptr < bounds.second);
  auto pad_end = io_buffer.first + aligned_size;
  ut_ad(pad_end > ptr);
  size_t pad_length = pad_end - ptr;
  memset(ptr, 0, pad_length);

  return persist({io_buffer.first, aligned_size});
}

int Key_sort_buffer::compare(const dfield_t *lhs, const dfield_t *rhs,
                             Dup *dup) noexcept {
  ut_ad(dup->m_index->is_clustered());

  Compare_key compare_key(dup->m_index, dup, false);

  return compare_key(lhs, rhs);
}

}  // namespace ddl

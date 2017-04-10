/*****************************************************************************

Copyright (c) 2016, Oracle and/or its affiliates. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA

*****************************************************************************/

#include "lob0fit.h"
#include <page0zip.h>

namespace lob {

/** Initialize the zlib streams.
@param[in]	level	the compression level.
@return 0 on success, -1 on failure. */
int FitBlock::init(int level) {
  /* Zlib deflate needs 128 kilobytes for the default
  window size, plus 512 << memLevel, plus a few
  kilobytes for small objects.  We use reduced memLevel
  to limit the memory consumption, and preallocate the
  heap, hoping to avoid memory fragmentation. */
  m_heap = mem_heap_create(250000);

  if (m_heap == nullptr) {
    return (-1);
  }

  page_zip_set_alloc(&m_def, m_heap);
  m_def.avail_in = 0;
  m_def.next_in = Z_NULL;

  const int windowBits = 15;
  const int memLevel = 8;
  const int strategy = Z_DEFAULT_STRATEGY;
  const int method = Z_DEFLATED;

  if (deflateInit2(&m_def, level, method, windowBits, memLevel, strategy) !=
      Z_OK) {
    return (-1);
  }

  return (0);
}

/** Free internally allocated memory. */
void FitBlock::free_mem() {
  m_outlen = 0;
  m_inlen = 0;

  if (m_heap != nullptr) {
    mem_heap_free(m_heap);
    m_heap = nullptr;
  }
}

/** Close the two zlib streams and free the two internal buffers. */
void FitBlock::destroy() {
  deflateEnd(&m_def);
  free_mem();
}

/** Fit the given uncompressed data into the output buffer. */
void FitBlock::fit(byte *output, uint size) {
  int ret;

  m_def.next_out = output;
  m_def.avail_out = size;

  do {
    do {
      ret = deflate(&m_def, Z_FINISH);
      if (m_def.avail_in == 0) {
        break;
      }
    } while (m_def.avail_out > 0);

    if (ret == Z_STREAM_END) {
      m_total_in += m_def.total_in;
      m_total_out += m_def.total_out;
      deflateReset(&m_def);

      uint remain = m_inlen - m_total_in;

      m_def.next_in = m_input + m_total_in;
      m_def.avail_in = remain > m_max_inlen ? m_max_inlen : remain;
    }

  } while ((m_def.avail_out > 0) && (m_total_in < m_inlen));

  if (m_total_in == m_inlen) {
    /* Insertion completed. */
    const uint unused = m_def.avail_out;
    if (unused > 0) {
      byte *ptr = output + size - unused;
      memset(ptr, 0, unused);
    }
  }
}

/** Initialize the zlib streams.
@return 0 on success, -1 on failure. */
int UnfitBlock::init() {
  /* Zlib deflate needs 128 kilobytes for the default
  window size, plus 512 << memLevel, plus a few
  kilobytes for small objects.  We use reduced memLevel
  to limit the memory consumption, and preallocate the
  heap, hoping to avoid memory fragmentation. */
  m_heap = mem_heap_create(250000);

  if (m_heap == nullptr) {
    return (-1);
  }

  page_zip_set_alloc(&m_inf, m_heap);
  m_inf.avail_in = 0;
  m_inf.next_in = Z_NULL;
  const int windowBits = 15;

  if (inflateInit2(&m_inf, windowBits) != Z_OK) {
    return (-1);
  }

  return (0);
}

/** Decompress LOB data from the given input buffer.
@param[in]	out	the input buffer.
@param[in]	size	the buffer size.*/
void UnfitBlock::unfit(byte *out, uint size) {
  int ret;
  m_inf.next_in = out;
  m_inf.avail_in = size;

  byte *end = out + size;

  do {
    ret = inflate(&m_inf, Z_FINISH);
    if (ret == Z_STREAM_END) {
      m_total_out += m_inf.total_out;

      byte *ptr = m_inf.next_in;
      inflateReset(&m_inf);
      m_inf.next_in = ptr;
      m_inf.avail_in = static_cast<uInt>(end - ptr);

      m_inf.next_out = m_output + m_total_out;
      m_inf.avail_out = m_outlen - m_total_out;
    }

    if (ret == Z_BUF_ERROR) {
      break;
    }

  } while (m_inf.avail_in > 0 && m_inf.avail_out > 0);
}

} // namespace lob

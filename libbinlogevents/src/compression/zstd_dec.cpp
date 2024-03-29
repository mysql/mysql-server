/* Copyright (c) 2019, 2023, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include <compression/zstd_dec.h>
#include <my_byteorder.h>  // TODO: fix this include
#include <algorithm>
#include "wrapper_functions.h"

namespace binary_log::transaction::compression {

Zstd_dec::Zstd_dec() : m_ctx(nullptr) {}

Zstd_dec::~Zstd_dec() { destroy(); }

void Zstd_dec::destroy() {
  if (m_ctx != nullptr) {
    ZSTD_freeDStream(m_ctx);
    m_ctx = nullptr;
  }
}

type Zstd_dec::do_get_type_code() const { return type_code; }

void Zstd_dec::do_reset() {
  BAPI_TRACE;

  if (m_ctx != nullptr) {
    auto ret = ZSTD_initDStream(m_ctx);
    if (ZSTD_isError(ret) != 0U) {
      BAPI_LOG("info", BAPI_VAR(ZSTD_getErrorName(ret)));
      destroy();
    }
  }
}

void Zstd_dec::do_feed(const Char_t *input_data, Size_t input_size) {
  BAPI_TRACE;
  BAPI_LOG("info", BAPI_VAR(input_size));

  // Protect against two successive calls to `feed` without a call to
  // `compress` between them.
  assert(m_ibuf.pos == m_ibuf.size);

  m_ibuf.src = input_data;
  m_ibuf.size = input_size;
  m_ibuf.pos = 0;
}

std::pair<Decompress_status, Decompressor::Size_t> Zstd_dec::do_decompress(
    Char_t *out, Size_t output_size) {
  BAPI_TRACE;
  assert(m_ibuf.src);

  // NOLINTBEGIN(cppcoreguidelines-macro-usage)
#define TRACE_RETURN(status, size)                                       \
  {                                                                      \
    BAPI_LOG("info", __FILE__ << ":" << __LINE__ << ": return ["         \
                              << debug_string(Decompress_status::status) \
                              << ", " << (size) << "]");                 \
    return std::make_pair(Decompress_status::status, (size));            \
  }
  // NOLINTEND(cppcoreguidelines-macro-usage)

  if (m_ctx == nullptr) {
    m_ctx = ZSTD_createDStream();
    if (m_ctx == nullptr) TRACE_RETURN(out_of_memory, 0);
  }

  ZSTD_outBuffer obuf{/*.dst = */ out,
                      /*.size = */ output_size,
                      /*.pos = */ 0};
  // Decompress.
  size_t ret = 0;
  do {
    // clang-format off
    BAPI_LOG("info", "before decompress:"
             << " " << BAPI_VAR(output_size)
             << " " << BAPI_VAR(m_ibuf.size)
             << " " << BAPI_VAR(m_ibuf.pos)
             << " " << BAPI_VAR(obuf.size)
             << " " << BAPI_VAR(obuf.pos));
    ret = ZSTD_decompressStream(m_ctx, &obuf, &m_ibuf);
    BAPI_LOG("info", "after decompress:"
             << " " << BAPI_VAR(ret)
             << " " << BAPI_VAR(output_size)
             << " " << BAPI_VAR(m_ibuf.size)
             << " " << BAPI_VAR(m_ibuf.pos)
             << " " << BAPI_VAR(obuf.size)
             << " " << BAPI_VAR(obuf.pos));
    // clang-format on
    // ZSTD detected corrupt data.
    if (ZSTD_isError(ret) != 0U) {
      BAPI_LOG("info", BAPI_VAR(ZSTD_getErrorName(ret)));
      TRACE_RETURN(corrupted, 0);
    }
    // If there is a frame boundary in the middle of the input, ZSTD
    // will stop at the boundary, even if there is more input
    // available and more output space available.  So in that case we
    // repeat.
  } while (m_ibuf.pos < m_ibuf.size && obuf.pos < obuf.size);

  auto was_frame_boundary = m_frame_boundary;
  m_frame_boundary = (ret == 0);
  if (obuf.pos == 0 && was_frame_boundary) TRACE_RETURN(end, 0);
  if (obuf.pos < output_size) TRACE_RETURN(truncated, obuf.pos);

  // ZSTD was able to decode all requested bytes.
  assert(obuf.pos == output_size);
  TRACE_RETURN(success, output_size);
}

Decompressor::Grow_constraint_t Zstd_dec::do_get_grow_constraint_hint() const {
  Grow_constraint_t ret;
  ret.set_grow_increment(ZSTD_DStreamOutSize());
  // Todo: we may use ZSTD_getFrameContentSize at the beginning of
  // each frame to get an upper bound, and pass that to
  // ret.set_max_size.
  return ret;
}

}  // namespace binary_log::transaction::compression

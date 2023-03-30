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

namespace binary_log {
namespace transaction {
namespace compression {

Zstd_dec::Zstd_dec() : m_ctx(nullptr) {
  // create the stream context
  m_ctx = ZSTD_createDStream();
  if (m_ctx != nullptr) {
    size_t ret = ZSTD_initDStream(m_ctx);
    if (ZSTD_isError(ret)) {
      ZSTD_freeDStream(m_ctx);
      m_ctx = nullptr;
    }
  }
}

Zstd_dec::~Zstd_dec() {
  if (m_ctx != nullptr) ZSTD_freeDStream(m_ctx);
}

type Zstd_dec::compression_type_code() { return ZSTD; }

bool Zstd_dec::open() {
  size_t ret{0};
  if (m_ctx == nullptr) return true;
    /*
     The advanced compression API used below as declared stable in
     1.4.0.

     The advanced API allows reusing the same context instead of
     creating a new one every time we open the compressor. This
     is useful within the binary log compression.
    */
#if ZSTD_VERSION_NUMBER >= 10400
  /** Reset session only. Dictionary will remain. */
  ret = ZSTD_DCtx_reset(m_ctx, ZSTD_reset_session_only);
#else
  ret = ZSTD_initDStream(m_ctx);
#endif

  return ZSTD_isError(ret);
}

bool Zstd_dec::close() {
  if (m_ctx == nullptr) return true;
  return false;
}

std::tuple<std::size_t, bool> Zstd_dec::decompress(const unsigned char *in,
                                                   size_t in_size) {
  ZSTD_outBuffer obuf{m_buffer, capacity(), size()};
  ZSTD_inBuffer ibuf{in, in_size, 0};
  std::size_t ret{0};
  auto err{false};

  do {
    auto min_buffer_len{ZSTD_DStreamOutSize()};

    // make sure that we have buffer space to hold the results
    if ((err = reserve(min_buffer_len))) break;

    // update the obuf buffer pointer and offset
    obuf.dst = m_buffer;
    obuf.size = capacity();

    // decompress
    ret = ZSTD_decompressStream(m_ctx, &obuf, &ibuf);

    // update the cursor
    m_buffer_cursor = m_buffer + obuf.pos;

    // error handling
    if ((err = ZSTD_isError(ret))) break;

  } while (obuf.size == obuf.pos);

  return std::make_tuple((ibuf.size - ibuf.pos), err);
}

}  // namespace compression
}  // namespace transaction
}  // namespace binary_log

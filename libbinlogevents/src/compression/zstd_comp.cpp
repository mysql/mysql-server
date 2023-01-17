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

#include <compression/zstd_comp.h>
#include <my_byteorder.h>  // TODO: fix this include
#include <algorithm>
#include "wrapper_functions.h"

namespace binary_log {
namespace transaction {
namespace compression {

Zstd_comp::Zstd_comp()
    : m_ctx(nullptr),
      m_compression_level_current(DEFAULT_COMPRESSION_LEVEL),
      m_compression_level_next(DEFAULT_COMPRESSION_LEVEL) {
  // create the stream context
  if (m_ctx == nullptr) m_ctx = ZSTD_createCStream();

  // initialize the stream
  if (m_ctx != nullptr) {
    if (ZSTD_isError(ZSTD_initCStream(m_ctx, m_compression_level_current))) {
      /* purecov: begin inspected */
      // Abnormal error when initializing the context
      ZSTD_freeCStream(m_ctx);
      m_ctx = nullptr;
      /* purecov: end */
    }
  }
}

void Zstd_comp::set_compression_level(unsigned int clevel) {
  if (clevel != m_compression_level_current) {
    m_compression_level_next = clevel;
  }
}

Zstd_comp::~Zstd_comp() {
  if (m_ctx != nullptr) {
    ZSTD_freeCStream(m_ctx);
    m_ctx = nullptr;
  }

  m_buffer_cursor = m_buffer;
}

type Zstd_comp::compression_type_code() { return ZSTD; }

bool Zstd_comp::open() {
  size_t ret{0};
  if (m_ctx == nullptr) goto err;

    /*
     The advanced compression API used below as declared stable in
     1.4.0 .

     The advanced API allows reusing the same context instead of
     creating a new one every time we open the compressor. This
     is useful within the binary log compression.
    */
#if ZSTD_VERSION_NUMBER >= 10400
  if (m_compression_level_current == m_compression_level_next) {
    ret = ZSTD_CCtx_reset(m_ctx, ZSTD_reset_session_only);
    if (ZSTD_isError(ret)) goto err;

    ret = ZSTD_CCtx_setPledgedSrcSize(m_ctx, ZSTD_CONTENTSIZE_UNKNOWN);
    if (ZSTD_isError(ret)) goto err;
  } else {
    ret = ZSTD_initCStream(m_ctx, m_compression_level_next);
    if (ZSTD_isError(ret)) goto err;
    m_compression_level_current = m_compression_level_next;
  }
#else
  ret = ZSTD_initCStream(m_ctx, m_compression_level_next);
  if (ZSTD_isError(ret)) goto err;
  m_compression_level_current = m_compression_level_next;
#endif

  m_buffer_cursor = m_buffer;

  return false;
err:
  return true;
}

bool Zstd_comp::close() {
  size_t ret{0};
  if (m_ctx == nullptr) goto err;

  do {
    ret = ZSTD_flushStream(m_ctx, &m_obuf);
    if (ZSTD_isError(ret)) goto err;
    m_buffer_cursor = static_cast<unsigned char *>(m_obuf.dst) + m_obuf.pos;

    // end the stream
    ret = ZSTD_endStream(m_ctx, &m_obuf);
    if (ZSTD_isError(ret)) goto err;

    if (ret > 0 && expand_buffer(ret)) goto err;

    m_buffer_cursor = static_cast<unsigned char *>(m_obuf.dst) + m_obuf.pos;
  } while (ret > 0);

  return false;
err:
  return true;
}

std::tuple<std::size_t, bool> Zstd_comp::compress(const unsigned char *buffer,
                                                  size_t length) {
  m_obuf = {m_buffer, capacity(), size()};
  ZSTD_inBuffer ibuf{static_cast<const void *>(buffer), length, 0};
  std::size_t ret{0};
  auto err{false};

  while (ibuf.pos < ibuf.size) {
    std::size_t min_capacity{ZSTD_CStreamOutSize()};

    // always have at least one block available
    if ((err = expand_buffer(min_capacity))) break;

      // compress now
#if ZSTD_VERSION_NUMBER >= 10400
    ret = ZSTD_compressStream2(m_ctx, &m_obuf, &ibuf, ZSTD_e_continue);
#else
    ret = ZSTD_compressStream(m_ctx, &m_obuf, &ibuf);
#endif

    // adjust the cursor
    m_buffer_cursor = m_buffer + m_obuf.pos;

    if ((err = ZSTD_isError(ret))) break;
  }

  return std::make_tuple(ibuf.size - ibuf.pos, err);
}

bool Zstd_comp::expand_buffer(size_t const &extra_bytes) {
  if (reserve(extra_bytes)) return true;
  // adjust the obuf
  m_obuf.dst = m_buffer;
  m_obuf.size = capacity();
  return false;
}

}  // namespace compression
}  // namespace transaction
}  // namespace binary_log

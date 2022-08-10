/*
 * Copyright (c) 2019, 2022, Oracle and/or its affiliates.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is also distributed with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have included with MySQL.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */
#ifndef PLUGIN_X_PROTOCOL_STREAM_COMPRESSION_COMPRESSION_ALGORITHM_ZSTD_H_
#define PLUGIN_X_PROTOCOL_STREAM_COMPRESSION_COMPRESSION_ALGORITHM_ZSTD_H_

#include <cstdint>
#include <memory>
#include <utility>

#include "my_compiler.h"
#include "my_dbug.h"  // NOLINT(build/include_subdir)
#include "zstd.h"     // NOLINT(build/include_subdir)

#include "plugin/x/protocol/stream/compression/compression_algorithm_interface.h"

namespace protocol {

class Compression_algorithm_zstd : public Compression_algorithm_interface {
 public:
  explicit Compression_algorithm_zstd(const int32_t level)
      : m_stream{ZSTD_createCStream()} {
#if ZSTD_VERSION_NUMBER < 10400
    is_error(ZSTD_initCStream(m_stream, level));
#else
    if (is_error(ZSTD_CCtx_reset(m_stream, ZSTD_reset_session_only)) ||
        is_error(ZSTD_CCtx_refCDict(m_stream, nullptr)) ||
        is_error(
            ZSTD_CCtx_setParameter(m_stream, ZSTD_c_compressionLevel, level)))
      return;
#endif
  }

  ~Compression_algorithm_zstd() override { ZSTD_freeCStream(m_stream); }

  void set_pledged_source_size(const int src_size [[maybe_unused]]) override {
    DBUG_TRACE;
    DBUG_LOG("debug", "set_pledged_source_size(" << src_size << ")");
#if ZSTD_VERSION_NUMBER < 10400
    // is_error(ZSTD_resetCStream(m_stream, src_size));
#else
    if (!is_error(ZSTD_CCtx_reset(m_stream, ZSTD_reset_session_only)))
      is_error(ZSTD_CCtx_setPledgedSrcSize(m_stream, src_size));
#endif
  }

  void set_input(uint8_t *in_ptr, const int in_size) override {
    DBUG_TRACE;
    m_in_buffer = ZSTD_inBuffer{in_ptr, static_cast<size_t>(in_size), 0};
    m_flush_finished = false;
  }

  bool compress(uint8_t *out_ptr, int *out_size) override {
    DBUG_TRACE;
    if (m_error) return false;
    ZSTD_outBuffer out_buffer{out_ptr, static_cast<size_t>(*out_size), 0};
    while (m_in_buffer.pos < m_in_buffer.size) {
#if ZSTD_VERSION_NUMBER < 10400
      if (is_error(ZSTD_compressStream(m_stream, &out_buffer, &m_in_buffer))) {
#else
      if (is_error(ZSTD_compressStream2(m_stream, &out_buffer, &m_in_buffer,
                                        ZSTD_e_continue))) {
#endif
        *out_size = 0;
        return false;
      }
    }
    DBUG_LOG("debug", "zstandard(in_pos:" << m_in_buffer.pos << ")");
    *out_size = static_cast<int>(out_buffer.pos);
    return true;
  }

  bool flush(uint8_t *out_ptr, int *out_size) override {
    DBUG_TRACE;
    if (m_error) return false;

    if (m_flush_finished) {
      *out_size = 0;
      return true;
    }

    ZSTD_outBuffer out_buffer{out_ptr, static_cast<size_t>(*out_size), 0};
#if ZSTD_VERSION_NUMBER < 10400
    const auto result = ZSTD_flushStream(m_stream, &out_buffer);
#else
    const auto result =
        ZSTD_compressStream2(m_stream, &out_buffer, &m_in_buffer, ZSTD_e_end);
#endif
    if (is_error(result)) {
      *out_size = 0;
      return false;
    }
    DBUG_LOG("debug", "zstandard(out_size:" << *out_size << "), executed with "
                                            << result);
    *out_size = static_cast<int>(out_buffer.pos);
    m_flush_finished = result == 0;
    return true;
  }

#if ZSTD_VERSION_NUMBER < 10400
  static int32_t get_level_min() { return 3; }
  static int32_t get_level_max() { return 3; }
#else
  static int32_t get_level_min() { return ZSTD_minCLevel(); }
  static int32_t get_level_max() { return ZSTD_maxCLevel(); }
#endif

 private:
  bool is_error(const uint64_t result) {
    if (!ZSTD_isError(result)) return false;
    DBUG_LOG("debug", "ZSTD error: " << ZSTD_getErrorName(result));
    return m_error = true;
  }

  ZSTD_CStream *m_stream;
  ZSTD_inBuffer m_in_buffer{nullptr, 0, 0};
  bool m_error{false};
  bool m_flush_finished{false};
};

}  // namespace protocol

#endif  // PLUGIN_X_PROTOCOL_STREAM_COMPRESSION_COMPRESSION_ALGORITHM_ZSTD_H_

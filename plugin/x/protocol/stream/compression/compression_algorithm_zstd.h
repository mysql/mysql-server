/*
 * Copyright (c) 2019, Oracle and/or its affiliates. All rights reserved.
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

#include "my_dbug.h"  // NOLINT(build/include_subdir)
#include "zstd.h"     // NOLINT(build/include_subdir)

#include "plugin/x/protocol/stream/compression/compression_algorithm_interface.h"

namespace protocol {

class Compression_algorithm_zstd : public Compression_algorithm_interface {
 public:
  Compression_algorithm_zstd() : m_stream{ZSTD_createCStream()} {
    ZSTD_initCStream(m_stream, -1);
  }

  ~Compression_algorithm_zstd() override { ZSTD_freeCStream(m_stream); }

  void set_input(uint8_t *in_ptr, const int in_size) override {
    DBUG_TRACE;
    m_in_buffer = ZSTD_inBuffer{in_ptr, static_cast<size_t>(in_size), 0};
  }

  bool compress(uint8_t *out_ptr, int *out_size) override {
    DBUG_TRACE;
    ZSTD_outBuffer out_buffer{out_ptr, static_cast<size_t>(*out_size), 0};
    const auto result = compress_impl(&out_buffer);
    *out_size = result ? static_cast<int>(out_buffer.pos) : 0;
    return result;
  }

  bool flush(uint8_t *out_ptr, int *out_size) override {
    DBUG_TRACE;
    ZSTD_outBuffer out_buffer{out_ptr, static_cast<size_t>(*out_size), 0};
    if (!compress_impl(&out_buffer)) {
      *out_size = 0;
      return false;
    }

    ZSTD_outBuffer flush_buffer{out_ptr + out_buffer.pos,
                                static_cast<size_t>(*out_size - out_buffer.pos),
                                0};
    const auto result = flush_impl(&flush_buffer);
    *out_size =
        result ? static_cast<int>(out_buffer.pos + flush_buffer.pos) : 0;
    return result;
  }

 private:
  bool flush_impl(ZSTD_outBuffer *out_buffer) {
    const auto result = ZSTD_flushStream(m_stream, out_buffer);
    if (!ZSTD_isError(result)) return true;
    DBUG_LOG("debug", "ZSTD error: " << ZSTD_getErrorName(result));
    return false;
  }

  bool compress_impl(ZSTD_outBuffer *out_buffer) {
    while (m_in_buffer.pos < m_in_buffer.size) {
      const auto result =
          ZSTD_compressStream(m_stream, out_buffer, &m_in_buffer);

      if (ZSTD_isError(result)) {
        DBUG_LOG("debug", "ZSTD error: " << ZSTD_getErrorName(result));
        return false;
      }
    }

    return true;
  }

  ZSTD_CStream *m_stream;
  ZSTD_inBuffer m_in_buffer{nullptr, 0, 0};
};

}  // namespace protocol

#endif  // PLUGIN_X_PROTOCOL_STREAM_COMPRESSION_COMPRESSION_ALGORITHM_ZSTD_H_

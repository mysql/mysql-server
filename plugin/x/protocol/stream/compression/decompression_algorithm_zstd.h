/*
 * Copyright (c) 2019, 2023, Oracle and/or its affiliates.
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
#ifndef PLUGIN_X_PROTOCOL_STREAM_COMPRESSION_DECOMPRESSION_ALGORITHM_ZSTD_H_
#define PLUGIN_X_PROTOCOL_STREAM_COMPRESSION_DECOMPRESSION_ALGORITHM_ZSTD_H_

#include "plugin/x/protocol/stream/compression/decompression_algorithm_interface.h"

#include "my_dbug.h"  // NOLINT(build/include_subdir)
#include "zstd.h"     // NOLINT(build/include_subdir)

namespace protocol {

class Decompression_algorithm_zstd : public Decompression_algorithm_interface {
 public:
  Decompression_algorithm_zstd() : m_stream{ZSTD_createDStream()} {
    ZSTD_initDStream(m_stream);
  }

  ~Decompression_algorithm_zstd() override { ZSTD_freeDStream(m_stream); }

  bool needs_input() const override {
    DBUG_TRACE;
    return m_needs_input;
  }

  void set_input(uint8_t *in_ptr, const int in_size) override {
    DBUG_TRACE;
    m_in_buffer = ZSTD_inBuffer{in_ptr, static_cast<size_t>(in_size), 0};
    m_needs_input = false;
  }

  bool decompress(uint8_t *out_ptr, int64_t *out_size) override {
    DBUG_TRACE;
    ZSTD_outBuffer out_buffer{out_ptr, static_cast<size_t>(*out_size), 0};
    const auto result =
        ZSTD_decompressStream(m_stream, &out_buffer, &m_in_buffer);
    if (ZSTD_isError(result)) {
      m_invalid = true;
      DBUG_LOG("debug", "ZSTD error: " << ZSTD_getErrorName(result));
      *out_size = 0;
      return false;
    }

    *out_size = out_buffer.pos;
    m_needs_input = (out_buffer.pos < out_buffer.size) &&
                    (m_in_buffer.pos == m_in_buffer.size);

    return true;
  }

  bool was_error() const override { return m_invalid; }

  bool m_invalid = false;
  bool m_needs_input = true;
  ZSTD_DStream *m_stream;
  ZSTD_inBuffer m_in_buffer{nullptr, 0, 0};
};

}  // namespace protocol

#endif  // PLUGIN_X_PROTOCOL_STREAM_COMPRESSION_DECOMPRESSION_ALGORITHM_ZSTD_H_

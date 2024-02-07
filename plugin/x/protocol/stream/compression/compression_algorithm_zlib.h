/*
 * Copyright (c) 2019, 2024, Oracle and/or its affiliates.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is designed to work with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have either included with
 * the program or referenced in the documentation.
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
#ifndef PLUGIN_X_PROTOCOL_STREAM_COMPRESSION_COMPRESSION_ALGORITHM_ZLIB_H_
#define PLUGIN_X_PROTOCOL_STREAM_COMPRESSION_COMPRESSION_ALGORITHM_ZLIB_H_

#include <memory>
#include <utility>

#include "my_dbug.h"  // NOLINT(build/include_subdir)
#include "zlib.h"     // NOLINT(build/include_subdir)

#include "plugin/x/protocol/stream/compression/compression_algorithm_interface.h"

namespace protocol {

class Compression_algorithm_zlib : public Compression_algorithm_interface {
 public:
  explicit Compression_algorithm_zlib(const int32_t level) {
    m_stream.zalloc = nullptr;
    m_stream.zfree = nullptr;
    m_stream.opaque = nullptr;
    m_stream.avail_in = 0;
    m_stream.avail_out = 0;
    m_stream.next_in = nullptr;
    m_stream.next_out = nullptr;
    deflateInit(&m_stream, level);
  }

  ~Compression_algorithm_zlib() override { deflateEnd(&m_stream); }

  void set_pledged_source_size(const int /*src_size*/) override {}

  void set_input(uint8_t *in_ptr, const int in_size) override {
    m_stream.avail_in = in_size;
    m_stream.next_in = in_ptr;
    m_flush_finished = false;
  }

  bool compress(uint8_t *out_ptr, int *out_size) override {
    DBUG_TRACE;

    if (m_stream.avail_in) return compress_impl(out_ptr, out_size, false);

    *out_size = 0;

    return true;
  }

  bool flush(uint8_t *out_ptr, int *out_size) override {
    DBUG_TRACE;
    if (m_flush_finished) {
      *out_size = 0;
      return true;
    }

    if (m_stream.avail_in) return compress(out_ptr, out_size);

    const auto result = compress_impl(out_ptr, out_size, true);

    if (0 != m_stream.avail_out) m_flush_finished = true;

    return result;
  }

  static int32_t get_level_min() { return Z_BEST_SPEED; }
  static int32_t get_level_max() { return Z_BEST_COMPRESSION; }

 private:
  bool compress_impl(uint8_t *out_ptr, int *out_size, const bool flush) {
    DBUG_TRACE;

    const int backup_out_size = *out_size;
    m_stream.avail_out = backup_out_size;
    m_stream.next_out = out_ptr;

    do {
      DBUG_LOG("debug", "deflate(in_size:" << m_stream.avail_in << ", out_size:"
                                           << m_stream.avail_out << ")");
      const auto result = deflate(&m_stream, flush ? Z_SYNC_FLUSH : Z_NO_FLUSH);
      if (Z_OK != result) {
        DBUG_LOG("debug",
                 "deflate(out_size:"
                     << m_stream.avail_out << ", in_size:" << m_stream.avail_in
                     << ") returned an error, executed with " << result);
        return false;
      }

      DBUG_LOG("debug", "should retry deflate(in_size:"
                            << m_stream.avail_in
                            << ", out_size:" << m_stream.avail_out << ")");
    } while (!flush && (0 != m_stream.avail_out && 0 != m_stream.avail_in));

    *out_size = backup_out_size - static_cast<int>(m_stream.avail_out);

    return true;
  }

  bool m_flush_finished = false;
  z_stream m_stream;
};

}  // namespace protocol

#endif  // PLUGIN_X_PROTOCOL_STREAM_COMPRESSION_COMPRESSION_ALGORITHM_ZLIB_H_

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
#ifndef PLUGIN_X_PROTOCOL_STREAM_COMPRESSION_DECOMPRESSION_ALGORITHM_ZLIB_H_
#define PLUGIN_X_PROTOCOL_STREAM_COMPRESSION_DECOMPRESSION_ALGORITHM_ZLIB_H_

#include "plugin/x/protocol/stream/compression/decompression_algorithm_interface.h"

#include "zlib.h"

#include "my_dbug.h"

namespace protocol {

class Decompression_algorithm_zlib : public Decompression_algorithm_interface {
 public:
  Decompression_algorithm_zlib() {
    m_zstream.zalloc = Z_NULL;
    m_zstream.zfree = Z_NULL;
    m_zstream.opaque = Z_NULL;
    m_zstream.avail_in = 0;
    m_zstream.next_in = Z_NULL;
    m_zstream.avail_out = 0;
    inflateInit(&m_zstream);
  }

  ~Decompression_algorithm_zlib() override { inflateEnd(&m_zstream); }

  bool needs_input() const override { return 0 == m_zstream.avail_in; }

  void set_input(uint8_t *in_ptr, const int in_size) override {
    DBUG_LOG("debug", "set_input(in_size:" << in_size << ")");
    DBUG_DUMP("decompress_zlib", in_ptr, in_size);
    m_zstream.avail_in = in_size;
    m_zstream.next_in = in_ptr;
  }

  bool decompress(uint8_t *out_ptr, int64_t *out_size) override {
    DBUG_TRACE;
    DBUG_LOG("debug", "decompress(out_size:" << *out_size << ")");
    int k_flush = m_zstream.avail_out ? Z_SYNC_FLUSH : Z_NO_FLUSH;

    m_zstream.avail_out = *out_size;
    m_zstream.next_out = out_ptr;

    const auto result = inflate(&m_zstream, k_flush);
    if (Z_OK != result) {
      // Ignore STREAM END, on the next interaction if should return an
      // error to client.
      if (Z_STREAM_END != result) {
        m_valid = false;
        DBUG_PRINT("info",
                   ("inflate failed with result: %i, flush:%i, avail_out:%i",
                    static_cast<int>(result), static_cast<int>(k_flush),
                    static_cast<int>(m_zstream.avail_out)));

        *out_size = 0;
        return false;
      }
    }

    *out_size -= m_zstream.avail_out;

    return true;
  }

  bool was_error() const override { return !m_valid; }

  bool m_valid = true;
  z_stream m_zstream;
};

}  // namespace protocol

#endif  // PLUGIN_X_PROTOCOL_STREAM_COMPRESSION_DECOMPRESSION_ALGORITHM_ZLIB_H_

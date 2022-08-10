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
#ifndef PLUGIN_X_PROTOCOL_STREAM_COMPRESSION_DECOMPRESSION_ALGORITHM_LZ4_H_
#define PLUGIN_X_PROTOCOL_STREAM_COMPRESSION_DECOMPRESSION_ALGORITHM_LZ4_H_

#include <lz4frame.h>

#include "my_dbug.h"

#include "plugin/x/protocol/stream/compression/decompression_algorithm_interface.h"

namespace protocol {

class Decompression_algorithm_lz4 : public Decompression_algorithm_interface {
 public:
  Decompression_algorithm_lz4() {
    LZ4F_createDecompressionContext(&m_ctxt, LZ4F_VERSION);
  }

  ~Decompression_algorithm_lz4() override {
    LZ4F_freeDecompressionContext(m_ctxt);
  }

  bool needs_input() const override { return m_decoded_whole_input; }

  void set_input(uint8_t *in_ptr, const int in_size) override {
    DBUG_TRACE;
    m_input_buffer_data_size = in_size;
    m_input_buffer_ptr = in_ptr;
    m_decoded_whole_input = false;
  }

  bool decompress(uint8_t *out_ptr, int64_t *out_size) override {
    DBUG_TRACE;
    size_t output_buffer_size = *out_size;
    size_t input_size = static_cast<size_t>(m_input_buffer_data_size);

    const auto result =
        LZ4F_decompress(m_ctxt, out_ptr, &output_buffer_size,
                        m_input_buffer_ptr, &input_size, nullptr);

    if (LZ4F_isError(result)) {
      m_valid = false;
      DBUG_LOG("debug", "LZ4F error:" << LZ4F_getErrorName(result));
      *out_size = 0;
      return false;
    }

    *out_size = output_buffer_size;

    m_input_buffer_data_size -= input_size;
    m_input_buffer_ptr += input_size;

    m_decoded_whole_input = (result == 0 || output_buffer_size == 0) &&
                            0 == m_input_buffer_data_size;

    DBUG_LOG("debug", "out_size:" << *out_size
                                  << ", need-more:" << m_decoded_whole_input);

    return true;
  }

  bool was_error() const override {
    DBUG_TRACE;
    return !m_valid;
  }

 private:
  LZ4F_decompressionContext_t m_ctxt;
  int m_input_buffer_data_size = 0;
  bool m_decoded_whole_input = true;
  uint8_t *m_input_buffer_ptr = nullptr;
  bool m_valid = true;
};

}  // namespace protocol

#endif  // PLUGIN_X_PROTOCOL_STREAM_COMPRESSION_DECOMPRESSION_ALGORITHM_LZ4_H_

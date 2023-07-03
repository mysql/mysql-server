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

#ifndef PLUGIN_X_PROTOCOL_STREAM_COMPRESSION_OUTPUT_STREAM_H_
#define PLUGIN_X_PROTOCOL_STREAM_COMPRESSION_OUTPUT_STREAM_H_

#include <google/protobuf/io/zero_copy_stream.h>
#include <memory>
#include <utility>
#include "zlib.h"

#include "my_dbug.h"

#include "plugin/x/protocol/stream/compression/compression_algorithm_interface.h"

namespace protocol {

class Compression_output_stream
    : public google::protobuf::io::ZeroCopyOutputStream {
 public:
  Compression_output_stream(Compression_algorithm_interface *algorithm,
                            ZeroCopyOutputStream *destination)
      : m_algorithm(algorithm), m_destination(destination) {}

  ~Compression_output_stream() override { Flush(); }

  void Flush() {
    DBUG_TRACE;
    if (m_input_buffer_offset) {
      if (!compress_input_buffer()) return;
    }

    int out_size = 0;

    do {
      if (!try_to_update_output_buffer()) return;

      auto out_ptr = m_output_buffer_ptr + m_output_buffer_offset;
      out_size = m_output_buffer_size - m_output_buffer_offset;

      if (!m_algorithm->flush(out_ptr, &out_size)) return;
      DBUG_LOG("debug", "flush returned " << out_size << " bytes");

      m_output_buffer_offset += out_size;
    } while (out_size);

    m_destination->BackUp(m_output_buffer_size - m_output_buffer_offset);
    DBUG_LOG("debug", "Destination backedup to " << m_output_buffer_offset);
    m_output_buffer_size = m_output_buffer_offset = 0;
  }

  bool Next(void **data, int *size) override {
    DBUG_TRACE;
    if (m_input_buffer_offset == sizeof(m_input_buffer)) {
      if (!compress_input_buffer()) return false;
    }

    *data = static_cast<void *>(m_input_buffer + m_input_buffer_offset);
    *size = sizeof(m_input_buffer) - m_input_buffer_offset;

    m_input_buffer_offset = sizeof(m_input_buffer);

    return true;
  }

  void BackUp(int count) override { m_input_buffer_offset -= count; }

  int64_t ByteCount() const override { return m_all + m_input_buffer_offset; }

 private:
  bool compress_input_buffer() {
    DBUG_TRACE;
    m_algorithm->set_input(m_input_buffer, m_input_buffer_offset);

    int out_size = 0;
    do {
      if (!try_to_update_output_buffer()) return false;

      auto out_ptr = m_output_buffer_ptr + m_output_buffer_offset;
      out_size = m_output_buffer_size - m_output_buffer_offset;

      if (!m_algorithm->compress(out_ptr, &out_size)) return false;
      DBUG_LOG("debug", "compress returned " << out_size << " bytes");

      m_output_buffer_offset += out_size;
    } while (out_size);

    m_all += m_input_buffer_offset;
    m_input_buffer_offset = 0;

    return true;
  }

  bool try_to_update_output_buffer() {
    DBUG_TRACE;
    if (m_output_buffer_size == m_output_buffer_offset) {
      if (!m_destination->Next(reinterpret_cast<void **>(&m_output_buffer_ptr),
                               &m_output_buffer_size)) {
        DBUG_PRINT("debug", ("Next() returned failed"));
        return false;
      }

      DBUG_LOG("debug",
               "Destination returned buffer of size " << m_output_buffer_size);
      m_output_buffer_offset = 0;
    }

    return true;
  }

  Compression_algorithm_interface *m_algorithm;
  ZeroCopyOutputStream *m_destination;

  int64_t m_all = 0;
  int m_input_buffer_offset = 0;
  int m_input_buffer_data_size = 0;
  uint8_t m_input_buffer[10];
  uint8_t *m_output_buffer_ptr = nullptr;
  int m_output_buffer_offset = 0;
  int m_output_buffer_size = 0;
};

}  // namespace protocol

#endif  // PLUGIN_X_PROTOCOL_STREAM_COMPRESSION_OUTPUT_STREAM_H_

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
#ifndef PLUGIN_X_PROTOCOL_STREAM_DECOMPRESSION_INPUT_STREAM_H_
#define PLUGIN_X_PROTOCOL_STREAM_DECOMPRESSION_INPUT_STREAM_H_

#include "google/protobuf/io/zero_copy_stream.h"

#include "my_dbug.h"

#include "plugin/x/protocol/stream/compression/decompression_algorithm_interface.h"

namespace protocol {

class Decompression_input_stream
    : public google::protobuf::io::ZeroCopyInputStream {
 public:
  using ZeroCopyInputStream = google::protobuf::io::ZeroCopyInputStream;

 public:
  Decompression_input_stream(Decompression_algorithm_interface *algorithm,
                             ZeroCopyInputStream *zero_copy_stream)
      : m_decompression_algorithm(algorithm), m_source(zero_copy_stream) {}

  bool Next(const void **data, int *size) override {
    DBUG_TRACE;
    const auto left = m_output_buffer_data_size - m_output_buffer_offset;

    if (left > 0) {
      *data = m_output_buffer + m_output_buffer_offset;
      *size = left;

      m_output_buffer_offset = m_output_buffer_data_size;

      DBUG_PRINT("info", ("Next(size:%i)=true", *size));
      DBUG_DUMP("Decompression_input_stream-out", (const uint8_t *)*data,
                (int)*size);

      return true;
    }

    m_all += m_output_buffer_offset;
    if (!ReadCompressed()) {
      return false;
    }

    return Next(data, size);
  }

  void BackUp(int count) override {
    m_output_buffer_offset -= count;
    DBUG_LOG("Decompression_input_stream::debug",
             "BackUp(" << count << ") where m_output_buffer_offset:"
                       << m_output_buffer_offset
                       << ", m_output_buffer_data_size:"
                       << m_output_buffer_data_size);
  }

  bool Skip(int count) override {
    DBUG_LOG("Decompression_input_stream::debug", "Skip(" << count << ")");
    auto left = m_output_buffer_data_size - m_output_buffer_offset;

    if (left >= count) {
      m_output_buffer_offset += count;
      return true;
    }

    m_output_buffer_offset = m_output_buffer_data_size;
    m_all += m_output_buffer_offset;
    if (!ReadCompressed()) return false;

    return Skip(count - left);
  }

  google::protobuf::int64 ByteCount() const override {
    return m_all + m_output_buffer_offset;
  }

 private:
  bool ReadCompressed() {
    DBUG_TRACE;

    if (m_decompression_algorithm->needs_input()) {
      uint8_t *in_ptr;
      int in_size;
      if (!m_source->Next(
              const_cast<const void **>(reinterpret_cast<void **>(&in_ptr)),
              &in_size))
        return false;

      m_decompression_algorithm->set_input(in_ptr, in_size);
    }

    m_output_buffer_offset = 0;
    m_output_buffer_data_size = sizeof(m_output_buffer);

    return m_decompression_algorithm->decompress(m_output_buffer,
                                                 &m_output_buffer_data_size);
  }

  Decompression_algorithm_interface *m_decompression_algorithm;
  ZeroCopyInputStream *m_source;

  int64_t m_all = 0;
  int64_t m_output_buffer_data_size = 0;
  int64_t m_output_buffer_offset = 0;
  uint8_t m_output_buffer[512];
};

}  // namespace protocol

#endif  // PLUGIN_X_PROTOCOL_STREAM_DECOMPRESSION_INPUT_STREAM_H_

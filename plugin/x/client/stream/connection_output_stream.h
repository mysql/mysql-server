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

#ifndef PLUGIN_X_CLIENT_STREAM_CONNECTION_OUTPUT_STREAM_H_
#define PLUGIN_X_CLIENT_STREAM_CONNECTION_OUTPUT_STREAM_H_

#include <google/protobuf/io/zero_copy_stream.h>
#include <zlib.h>

#include <memory>
#include <utility>

#include "mysqlxclient/xconnection.h"

namespace xcl {

class Connection_output_stream
    : public google::protobuf::io::ZeroCopyOutputStream {
 public:
  using ZeroCopyOutputStream = google::protobuf::io::ZeroCopyOutputStream;
  using pb_int64 = google::protobuf::int64;

 public:
  explicit Connection_output_stream(XConnection *connection)
      : m_connection(connection) {}

  ~Connection_output_stream() override { Flush(); }

  XError getLastError() const {
    if (m_error) {
      DBUG_LOG("debug",
               "Connection_output_stream::getLastError() = {error_code:"
                   << m_error.error() << ", msg: \"" << m_error.what() << "\"");
    }

    return m_error;
  }

  void Flush() {
    if (m_input_buffer_offset && !m_error) {
      m_all += m_input_buffer_offset;

      m_error = m_connection->write(m_input_buffer, m_input_buffer_offset);

      m_input_buffer_offset = 0;
    }
  }

  bool Next(void **data, int *size) override {
    const auto k_input_buffer_max_size = sizeof(m_input_buffer);

    if (m_error) return false;

    if (m_input_buffer_offset == k_input_buffer_max_size) {
      Flush();
      m_input_buffer_offset = 0;

      return Next(data, size);
    }

    *size = m_input_buffer_offset = k_input_buffer_max_size;
    *data = static_cast<void *>(m_input_buffer);

    return true;
  }

  void BackUp(int count) override { m_input_buffer_offset -= count; }

  pb_int64 ByteCount() const override { return m_all + m_input_buffer_offset; }

 private:
  XError m_error;
  XConnection *m_connection;

  pb_int64 m_all = 0;
  uint8_t m_input_buffer[1000];
  int m_input_buffer_offset = 0;
};

}  // namespace xcl

#endif  // PLUGIN_X_CLIENT_STREAM_CONNECTION_OUTPUT_STREAM_H_

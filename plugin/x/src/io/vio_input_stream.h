/*
 * Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.
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

#ifndef PLUGIN_X_SRC_IO_VIO_INPUT_STREAM_H_
#define PLUGIN_X_SRC_IO_VIO_INPUT_STREAM_H_

#include <google/protobuf/io/zero_copy_stream.h>
#include <my_inttypes.h>
#include <memory>

#include "plugin/x/ngs/include/ngs/interface/vio_interface.h"

namespace xpl {

class Vio_input_stream : public google::protobuf::io::ZeroCopyInputStream {
 public:
  using gint64 = google::protobuf::int64;

 public:
  Vio_input_stream(const std::shared_ptr<ngs::Vio_interface> &connection);
  ~Vio_input_stream() override;

  bool was_io_error(int *error_code) const;
  void reset_byte_count();

  void lock_data(const int count);
  void unlock_data();
  bool peek_data(const void **data, int *size);
  void mark_vio_as_idle();
  void mark_vio_as_active();

  // ZeroCopyInputStream
 public:
  bool Next(const void **data, int *size) override;
  void BackUp(int count) override;
  bool Skip(int count) override;
  gint64 ByteCount() const override;

 private:
  bool read_more_data();

  std::shared_ptr<ngs::Vio_interface> m_connection;

  // Internal buffer
  char *m_buffer;
  int m_buffer_data_pos{0};
  int m_buffer_data_count{0};

  int m_bytes_count{0};

  int m_locked_data_count{0};
  int m_locked_data_pos{0};

  // IO error/handling
  int m_last_io_return_value{1};
  bool m_idle{true};
  int m_idle_data{0};
};

}  // namespace xpl

#endif  // PLUGIN_X_SRC_IO_VIO_INPUT_STREAM_H_

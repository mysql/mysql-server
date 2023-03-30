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

#ifndef PLUGIN_X_PROTOCOL_PLUGIN_FILE_OUTPUT_H_
#define PLUGIN_X_PROTOCOL_PLUGIN_FILE_OUTPUT_H_

#include <google/protobuf/compiler/code_generator.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/io/zero_copy_stream.h>
#include <algorithm>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

class File_output {
 public:
  using ZeroCopyOutputStream = google::protobuf::io::ZeroCopyOutputStream;
  using Context = google::protobuf::compiler::GeneratorContext;

 public:
  explicit File_output(const std::string &name) : m_name(name) {}
  virtual ~File_output() { close(); }

  void close() {
    if (m_file) {
      write_footer(nullptr);
    }

    m_file.reset();
  }

  template <typename... Types>
  void write_to_context(Context *context, Types &&... values) {
    if (!m_file && context) {
      m_file.reset(context->Open(m_name));

      write_header(context);
    }

    writeln(std::forward<Types>(values)...);
  }

  virtual void write_header(Context *context) = 0;
  virtual void write_footer(Context *context) = 0;

 private:
  bool write_bin(const char *buffer, size_t size) {
    void *data;
    int data_size;

    if (!m_file) return false;

    while (0 < size && m_file->Next(&data, &data_size)) {
      const int pushed = std::min(data_size, static_cast<int>(size));

      memcpy(data, buffer, pushed);

      buffer += pushed;
      size -= pushed;

      if (pushed < data_size) m_file->BackUp(data_size - pushed);
    }

    return 0 == size;
  }

  bool write() { return true; }

  template <typename... Types>
  bool write(const std::string &value, Types &&... values) {
    if (!write_bin(value.c_str(), value.length())) return false;

    return write(std::forward<Types>(values)...);
  }

  template <typename... Types>
  bool writeln(Types &&... values) {
    return write(std::forward<Types>(values)..., "\n");
  }

  std::unique_ptr<ZeroCopyOutputStream> m_file;
  std::string m_name;
};

#endif  // PLUGIN_X_PROTOCOL_PLUGIN_FILE_OUTPUT_H_

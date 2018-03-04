/*
 * Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.
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

#ifndef X_PROTOCOL_PLUGIN_FILE_OUTPUT_H
#define X_PROTOCOL_PLUGIN_FILE_OUTPUT_H

#include <google/protobuf/descriptor.h>
#include <google/protobuf/compiler/code_generator.h>
#include <algorithm>
#include <memory>
#include <string>
#include <set>
#include <vector>
#include <utility>


class Chain_file_output {
 public:
  using ZeroCopyOutputStream = google::protobuf::io::ZeroCopyOutputStream;
  using Context              = google::protobuf::compiler::GeneratorContext;

 public:
  explicit Chain_file_output(const std::string &name)
  : m_name(name) {
  }

  ~Chain_file_output() {
    close();
  }

  void close() {
    if (nullptr != m_chain_file) {
      writeln("  };");
      writeln("};");
      writeln("");
      writeln("#endif  // X_PROTOCOL_XPROTOCOL_TAGS_H");
      m_chain_file.reset();
    }
  }

  void append_chain(
      Context *context,
      const std::string &chain) {
    start_output_if_not_started(context);
    writeln("    \"", chain, "\",");
  }

 private:
  void start_output_if_not_started(Context *context) {
    if (nullptr == m_chain_file) {
      m_chain_file.reset(context->Open(m_name));

      writeln("#ifndef X_PROTOCOL_XPROTOCOL_TAGS_H");
      writeln("#define X_PROTOCOL_XPROTOCOL_TAGS_H");
      writeln("");
      writeln("#include <set>");
      writeln("#include <string>");
      writeln("#include <cstring>");
      writeln("");
      writeln("");
      writeln("class XProtocol_tags {");
      writeln(" public:");
      writeln("  bool is_chain_acceptable(const std::string &chain) {");
      writeln("    auto iterator = m_allowed_tag_chains.lower_bound(chain);");
      writeln("    if (m_allowed_tag_chains.end() == iterator)");
      writeln("      return false;");
      writeln("    const auto to_match = (*iterator).c_str();");
      writeln("    return strstr(to_match, chain.c_str()) == to_match;");
      writeln("  }");
      writeln("");
      writeln(" private:");
      writeln("  std::set<std::string> m_allowed_tag_chains {");
    }
  }

  bool write_bin(
      const char *buffer,
      size_t size) {
    void *data;
    int data_size;

    while (0 < size && m_chain_file->Next(&data, &data_size)) {
      const int pushed = std::min(data_size, static_cast<int>(size));

      memcpy(data, buffer, pushed);

      buffer += pushed;
      size -= pushed;

      if (pushed < data_size)
        m_chain_file->BackUp(data_size - pushed);
    }

    return 0 == size;
  }

  bool write(const std::string &value) {
    return write_bin(value.c_str(), value.length());
  }

  template<typename... Types>
  bool write(
      const std::string &value,
      Types&&... values) {
    if (!write(value))
      return false;

    return write(std::forward<Types>(values)...);
  }

  template<typename... Types>
  bool writeln(
      const std::string &value,
      Types&&... values) {
    if (!write(value))
      return false;

    return write(std::forward<Types>(values)..., "\n");
  }

  std::unique_ptr<ZeroCopyOutputStream> m_chain_file;
  std::string m_name;
};

#endif  // X_PROTOCOL_PLUGIN_FILE_OUTPUT_H

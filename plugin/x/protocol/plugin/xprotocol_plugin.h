/*
 * Copyright (c) 2017, 2019, Oracle and/or its affiliates. All rights reserved.
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

#ifndef PLUGIN_X_PROTOCOL_PLUGIN_XPROTOCOL_PLUGIN_H_
#define PLUGIN_X_PROTOCOL_PLUGIN_XPROTOCOL_PLUGIN_H_

#include <google/protobuf/compiler/code_generator.h>
#include <google/protobuf/descriptor.h>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "plugin/x/protocol/plugin/chain_file_output.h"
#include "plugin/x/protocol/plugin/encoder_file_output.h"
#include "plugin/x/protocol/plugin/message_field_chain.h"
#include "plugin/x/protocol/plugin/messages_used_by_server.h"

class XProtocol_plugin : public google::protobuf::compiler::CodeGenerator {
 public:
  using GeneratorContext = google::protobuf::compiler::GeneratorContext;
  using FileDescriptor = google::protobuf::FileDescriptor;

 public:
  explicit XProtocol_plugin(Chain_file_output *chain_file,
                            Encoder_file_output *encoder_file)
      : m_chain_file(chain_file), m_encoder_file(encoder_file) {}

  bool Generate(const FileDescriptor *file, const std::string & /* parameter */,
                GeneratorContext *generator_context,
                std::string * /* error */) const override {
    ++m_count;

    Message_field_chain filed_chain_generator(generator_context, m_chain_file);

    for (int i = 0; i < file->message_type_count(); ++i) {
      auto message = file->message_type(i);

      // Output generated data to m_chain_file
      filed_chain_generator.indeep_search(message);

      m_server_only_generator.indeep_search_with_context(generator_context,
                                                         message);
    }

    if (processed_all_from(generator_context)) {
      m_chain_file->close();
      m_encoder_file->close();
    }

    return true;
  }

 private:
  bool processed_all_from(GeneratorContext *generator_context) const {
    std::vector<const FileDescriptor *> v;
    generator_context->ListParsedFiles(&v);

    return m_count == v.size();
  }

  Chain_file_output *m_chain_file;
  Encoder_file_output *m_encoder_file;
  mutable Messages_used_by_server m_server_only_generator{m_encoder_file};
  mutable size_t m_count{0};
};

#endif  // PLUGIN_X_PROTOCOL_PLUGIN_XPROTOCOL_PLUGIN_H_

/*
 * Copyright (c) 2017, 2022, Oracle and/or its affiliates.
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

#ifndef PLUGIN_X_PROTOCOL_PLUGIN_MESSAGE_FIELD_CHAIN_H_
#define PLUGIN_X_PROTOCOL_PLUGIN_MESSAGE_FIELD_CHAIN_H_

#include "my_compiler.h"
MY_COMPILER_DIAGNOSTIC_PUSH()
// Suppress warning C4251 'type' : class 'type1' needs to have dll-interface
// to be used by clients of class 'type2'
MY_COMPILER_MSVC_DIAGNOSTIC_IGNORE(4251)
#include <google/protobuf/compiler/code_generator.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/io/zero_copy_stream.h>
MY_COMPILER_DIAGNOSTIC_POP()
#include <memory>
#include <set>
#include <string>

#include "plugin/x/protocol/plugin/chain_file_output.h"
#include "plugin/x/protocol/plugin/message_deep_first_search.h"

class Message_field_chain : public Message_deep_first_search {
 public:
  using Descriptor = google::protobuf::Descriptor;
  using FieldDescriptor = google::protobuf::FieldDescriptor;
  using Context = google::protobuf::compiler::GeneratorContext;

 public:
  explicit Message_field_chain(Context *context, Chain_file_output *output_file)
      : m_context(context), m_output_file(output_file) {}

  bool begin_validate_field(const FieldDescriptor *field,
                            const Descriptor *message) override;
  void end_validate_field(const FieldDescriptor *field,
                          const Descriptor *message) override;

 private:
  Context *m_context;
  Chain_file_output *m_output_file;
  std::set<std::string> m_types_done;
  std::string m_chain;
};

#endif  // PLUGIN_X_PROTOCOL_PLUGIN_MESSAGE_FIELD_CHAIN_H_

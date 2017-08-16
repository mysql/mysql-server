/*
 * Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef X_PROTOCOL_PLUGIN_MESSSAGE_CHAIN_H
#define X_PROTOCOL_PLUGIN_MESSSAGE_CHAIN_H

#include <google/protobuf/compiler/code_generator.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/io/zero_copy_stream.h>
#include <memory>
#include <set>
#include <string>

#include "plugin/x/protocol/plugin/chain_file_output.h"

class Message_field_chain {
 public:
  using Protocol_descriptor =
      google::protobuf::FileDescriptor;
  using ZeroCopyOutputStream =
      google::protobuf::io::ZeroCopyOutputStream;

  using Field_descriptor = google::protobuf::FieldDescriptor;
  using Descriptor       = google::protobuf::Descriptor;
  using FieldDescriptor  = google::protobuf::FieldDescriptor;
  using Context          = google::protobuf::compiler::GeneratorContext;

  Message_field_chain(
      const Protocol_descriptor& proto_file,
      Context* context,
      Chain_file_output *output_file)
      : m_protocol_file(proto_file),
        m_context(context),
        m_output_file(*output_file) {}

  bool generate_chain_for_each_client_message();

 private:
  void chain_message_and_its_children(
      const std::string &chain,
      std::set<std::string> *types_done,
      const Descriptor *msg);

  const Protocol_descriptor& m_protocol_file;
  Context* m_context;
  Chain_file_output &m_output_file;
};

#endif  // X_PROTOCOL_PLUGIN_MESSSAGE_CHAIN_H

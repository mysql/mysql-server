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

#include "message_field_chain.h"

#include <string>
#include "protobuf/mysqlx.pb.h"


void Message_field_chain::chain_message_and_its_children(
    const std::string &chain,
    std::set<std::string> *types_done,
    const Descriptor *msg) {
  if (nullptr == msg ||
      0 != types_done->count(msg->full_name()) ||
      0 == msg->field_count()) {
    m_output_file.append_chain(m_context, chain);

    return;
  }

  types_done->emplace(msg->full_name());

  for (int i = 0; i < msg->field_count(); ++i) {
    auto field = msg->field(i);

    if (nullptr == field)
      continue;

    const Descriptor *field_descriptor = nullptr;

    if (FieldDescriptor::TYPE_MESSAGE == field->type() ||
        FieldDescriptor::TYPE_GROUP   == field->type())
      field_descriptor = field->message_type();

    chain_message_and_its_children(
        chain + "." + std::to_string(field->number()),
        types_done,
        field_descriptor);
  }

  types_done->erase(msg->full_name());
}

bool Message_field_chain::generate_chain_for_each_client_message() {
  for (int i = 0; i < m_protocol_file.message_type_count(); ++i) {
    auto message = m_protocol_file.message_type(i);

    if (nullptr == message)
      return false;


    if (!message->options().HasExtension(Mysqlx::client_message_id))
      continue;

    const auto client_id = static_cast<int>(
        message->options().GetExtension(Mysqlx::client_message_id));

    std::set<std::string> types;
    chain_message_and_its_children(std::to_string(client_id), &types, message);
  }
  return true;
}

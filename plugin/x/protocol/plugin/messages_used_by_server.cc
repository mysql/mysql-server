/*
 * Copyright (c) 2019, Oracle and/or its affiliates. All rights reserved.
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

#include "plugin/x/protocol/plugin/messages_used_by_server.h"

#include <string>

#include "plugin/x/generated/protobuf/mysqlx.pb.h"

Messages_used_by_server::Messages_used_by_server(
    Encoder_file_output *output_file)
    : m_output_file(output_file) {}

bool Messages_used_by_server::begin_validate_field(const FieldDescriptor *field,
                                                   const Descriptor *message) {
  const bool is_root = nullptr == field;

  if (is_root) {
    if (!message) return false;

    const auto &message_options = message->options();

    if (!message_options.HasExtension(Mysqlx::server_message_id)) {
      if (0 == m_forced_packages.count(message->file()->package()))
        return false;
    }
  }

  // Check against cycles in Message dependencies graph
  const bool was_node_visited =
      message && 0 != m_types_done.count(message->full_name());

  if (nullptr == message || was_node_visited) {
    return false;
  }

  m_types_done.emplace(message->full_name());
  m_output_file->append_message(m_context, message);

  return true;
}

void Messages_used_by_server::end_validate_field(const FieldDescriptor *field,
                                                 const Descriptor *message) {}

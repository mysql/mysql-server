/*
 * Copyright (c) 2017, 2024, Oracle and/or its affiliates.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is designed to work with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have either included with
 * the program or referenced in the documentation.
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

#include "plugin/x/protocol/plugin/message_field_chain.h"

#include <string>

#include "my_compiler.h"
MY_COMPILER_DIAGNOSTIC_PUSH()
// Suppress warning C4251 'type' : class 'type1' needs to have dll-interface
// to be used by clients of class 'type2'
MY_COMPILER_MSVC_DIAGNOSTIC_IGNORE(4251)
#include "plugin/x/generated/protobuf/mysqlx.pb.h"
MY_COMPILER_DIAGNOSTIC_POP()

bool Message_field_chain::begin_validate_field(const FieldDescriptor *field,
                                               const Descriptor *message) {
  const bool is_root = nullptr == field;

  std::string chain = m_chain;
  if (field) chain += "." + std::to_string(field->number());

  if (is_root) {
    m_types_done.clear();

    if (!message) return false;

    const auto &message_options = message->options();

    if (!message_options.HasExtension(Mysqlx::client_message_id)) return false;

    const auto client_id = static_cast<int>(
        message_options.GetExtension(Mysqlx::client_message_id));
    chain = std::to_string(client_id);
  }

  // Check against cycles in Message dependencies graph
  const bool was_node_visited =
      message && 0 != m_types_done.count(message->full_name());

  if (nullptr == message || was_node_visited || 0 == message->field_count()) {
    m_output_file->append_chain(m_context, chain);

    return false;
  }

  m_chain = chain;
  m_types_done.emplace(message->full_name());

  return true;
}

void Message_field_chain::end_validate_field(const FieldDescriptor *field,
                                             const Descriptor *message) {
  m_types_done.erase(message->full_name());
  const auto position = m_chain.find_last_of(".");

  if (std::string::npos != position) m_chain.resize(position);
}

/* Copyright (c) 2018, 2019, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "plugin/group_replication/include/plugin_messages/transaction_with_guarantee_message.h"
#include "my_dbug.h"

Transaction_with_guarantee_message::Transaction_with_guarantee_message(
    enum_group_replication_consistency_level consistency_level)
    : Transaction_message_interface(CT_TRANSACTION_WITH_GUARANTEE_MESSAGE),
      m_consistency_level(consistency_level) {
  DBUG_ASSERT(m_consistency_level >= GROUP_REPLICATION_CONSISTENCY_AFTER);
}

Transaction_with_guarantee_message::~Transaction_with_guarantee_message() {}

bool Transaction_with_guarantee_message::write(const unsigned char *buffer,
                                               my_off_t length) {
  m_data.insert(m_data.end(), buffer, buffer + length);
  return false;
}

my_off_t Transaction_with_guarantee_message::length() { return m_data.size(); }

void Transaction_with_guarantee_message::encode_payload(
    std::vector<unsigned char> *buffer) const {
  DBUG_TRACE;

  encode_payload_item_type_and_length(buffer, PIT_TRANSACTION_DATA,
                                      m_data.size());
  buffer->insert(buffer->end(), m_data.begin(), m_data.end());

  char consistency_level_aux = static_cast<char>(m_consistency_level);
  encode_payload_item_char(buffer, PIT_TRANSACTION_CONSISTENCY_LEVEL,
                           consistency_level_aux);
}

/* purecov: begin inspected */
void Transaction_with_guarantee_message::decode_payload(
    const unsigned char *buffer, const unsigned char *) {
  DBUG_TRACE;
  const unsigned char *slider = buffer;
  uint16 payload_item_type = 0;
  unsigned long long payload_item_length = 0;

  decode_payload_item_type_and_length(&slider, &payload_item_type,
                                      &payload_item_length);
  m_data.clear();
  m_data.insert(m_data.end(), slider, slider + payload_item_length);

  unsigned char consistency_level_aux = 0;
  decode_payload_item_char(&slider, &payload_item_type, &consistency_level_aux);
  m_consistency_level = static_cast<enum_group_replication_consistency_level>(
      consistency_level_aux);
  DBUG_ASSERT(m_consistency_level >= GROUP_REPLICATION_CONSISTENCY_AFTER);
}
/* purecov: end */

enum_group_replication_consistency_level
Transaction_with_guarantee_message::decode_and_get_consistency_level(
    const unsigned char *buffer, size_t) {
  DBUG_TRACE;

  // Get first payload item pointer and size.
  const unsigned char *payload_data = nullptr;
  size_t payload_size = 0;
  get_first_payload_item_raw_data(buffer, &payload_data, &payload_size);

  const unsigned char *slider = payload_data + payload_size;
  uint16 payload_item_type = 0;

  unsigned char consistency_level_aux = 0;
  decode_payload_item_char(&slider, &payload_item_type, &consistency_level_aux);
  enum_group_replication_consistency_level consistency_level =
      static_cast<enum_group_replication_consistency_level>(
          consistency_level_aux);
  DBUG_ASSERT(consistency_level >= GROUP_REPLICATION_CONSISTENCY_AFTER);

  return consistency_level;
}

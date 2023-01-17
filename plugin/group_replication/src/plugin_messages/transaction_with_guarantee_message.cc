/* Copyright (c) 2018, 2023, Oracle and/or its affiliates.

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
#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/gcs_message.h"

const uint64_t
    Transaction_with_guarantee_message::s_consistency_level_pit_size =
        Plugin_gcs_message::WIRE_PAYLOAD_ITEM_HEADER_SIZE + 1;

Transaction_with_guarantee_message::Transaction_with_guarantee_message(
    uint64_t payload_capacity,
    enum_group_replication_consistency_level consistency_level)
    : Transaction_message_interface(CT_TRANSACTION_WITH_GUARANTEE_MESSAGE),
      m_consistency_level(consistency_level) {
  DBUG_TRACE;
  assert(m_consistency_level >= GROUP_REPLICATION_CONSISTENCY_AFTER);

  /*
    Consider the message headers size on the Gcs_message_data capacity.
  */
  const uint64_t headers_size =
      Plugin_gcs_message::WIRE_FIXED_HEADER_SIZE +
      Plugin_gcs_message::WIRE_PAYLOAD_ITEM_HEADER_SIZE;
  const uint64_t message_capacity =
      headers_size + payload_capacity + s_consistency_level_pit_size;
  m_gcs_message_data = new Gcs_message_data(0, message_capacity);

  std::vector<unsigned char> buffer;
  encode_header(&buffer);
  encode_payload_item_type_and_length(&buffer, PIT_TRANSACTION_DATA,
                                      payload_capacity);
  assert(buffer.size() == headers_size);
  m_gcs_message_data->append_to_payload(&buffer.front(), headers_size);
}

Transaction_with_guarantee_message::~Transaction_with_guarantee_message() {
  DBUG_TRACE;
  delete m_gcs_message_data;
}

bool Transaction_with_guarantee_message::write(const unsigned char *buffer,
                                               my_off_t length) {
  DBUG_TRACE;
  if (nullptr == m_gcs_message_data) {
    return true;
  }

  return m_gcs_message_data->append_to_payload(buffer, length);
}

uint64_t Transaction_with_guarantee_message::length() {
  DBUG_TRACE;
  if (nullptr == m_gcs_message_data) {
    return 0;
  }

  return m_gcs_message_data->get_encode_size();
}

Gcs_message_data *
Transaction_with_guarantee_message::get_message_data_and_reset() {
  DBUG_TRACE;
  if (nullptr == m_gcs_message_data) {
    return nullptr;
  }

  /*
    Add the PIT_TRANSACTION_CONSISTENCY_LEVEL to the Gcs_message_data.
  */
  std::vector<unsigned char> buffer;
  char consistency_level_aux = static_cast<char>(m_consistency_level);
  encode_payload_item_char(&buffer, PIT_TRANSACTION_CONSISTENCY_LEVEL,
                           consistency_level_aux);
  m_gcs_message_data->append_to_payload(&buffer.front(),
                                        s_consistency_level_pit_size);

  Gcs_message_data *result = m_gcs_message_data;
  m_gcs_message_data = nullptr;
  return result;
}

void Transaction_with_guarantee_message::encode_payload(
    std::vector<unsigned char> *) const {
  DBUG_TRACE;
  assert(0);
}

void Transaction_with_guarantee_message::decode_payload(const unsigned char *,
                                                        const unsigned char *) {
  DBUG_TRACE;
  assert(0);
}

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
  assert(consistency_level >= GROUP_REPLICATION_CONSISTENCY_AFTER);

  return consistency_level;
}

/* Copyright (c) 2013, 2022, Oracle and/or its affiliates.

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

#include "plugin/group_replication/include/plugin_messages/transaction_message.h"
#include "my_dbug.h"
#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/gcs_message.h"

Transaction_message::Transaction_message(uint64_t payload_capacity)
    : Transaction_message_interface(CT_TRANSACTION_MESSAGE) {
  DBUG_TRACE;

  /*
    Consider the message headers size on the Gcs_message_data capacity.
  */
  const uint64_t headers_size =
      Plugin_gcs_message::WIRE_FIXED_HEADER_SIZE +
      Plugin_gcs_message::WIRE_PAYLOAD_ITEM_HEADER_SIZE;
  const uint64_t message_capacity = headers_size + payload_capacity;
  m_gcs_message_data = new Gcs_message_data(0, message_capacity);

  std::vector<unsigned char> buffer;
  encode_header(&buffer);
  encode_payload_item_type_and_length(&buffer, PIT_TRANSACTION_DATA,
                                      payload_capacity);
  assert(buffer.size() == headers_size);
  m_gcs_message_data->append_to_payload(&buffer.front(), headers_size);
}

Transaction_message::~Transaction_message() {
  DBUG_TRACE;
  delete m_gcs_message_data;
}

bool Transaction_message::write(const unsigned char *buffer, my_off_t length) {
  DBUG_TRACE;
  if (nullptr == m_gcs_message_data) {
    return true;
  }

  return m_gcs_message_data->append_to_payload(buffer, length);
}

uint64_t Transaction_message::length() {
  DBUG_TRACE;
  if (nullptr == m_gcs_message_data) {
    return 0;
  }

  return m_gcs_message_data->get_encode_size();
}

Gcs_message_data *Transaction_message::get_message_data_and_reset() {
  DBUG_TRACE;
  Gcs_message_data *result = m_gcs_message_data;
  m_gcs_message_data = nullptr;
  return result;
}

void Transaction_message::encode_payload(std::vector<unsigned char> *) const {
  DBUG_TRACE;
  assert(0);
}

void Transaction_message::decode_payload(const unsigned char *,
                                         const unsigned char *) {
  DBUG_TRACE;
  assert(0);
}

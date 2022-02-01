/* Copyright (c) 2013, 2021, Oracle and/or its affiliates.

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

Transaction_message::Transaction_message()
    : Transaction_message_interface(CT_TRANSACTION_MESSAGE) {}

Transaction_message::~Transaction_message() {}

bool Transaction_message::write(const unsigned char *buffer, my_off_t length) {
  m_data.insert(m_data.end(), buffer, buffer + length);
  return false;
}

my_off_t Transaction_message::length() { return m_data.size(); }

void Transaction_message::encode_payload(
    std::vector<unsigned char> *buffer) const {
  DBUG_TRACE;

  encode_payload_item_type_and_length(buffer, PIT_TRANSACTION_DATA,
                                      m_data.size());
  buffer->insert(buffer->end(), m_data.begin(), m_data.end());
}

/* purecov: begin inspected */
void Transaction_message::decode_payload(const unsigned char *buffer,
                                         const unsigned char *) {
  DBUG_TRACE;
  const unsigned char *slider = buffer;
  uint16 payload_item_type = 0;
  unsigned long long payload_item_length = 0;

  decode_payload_item_type_and_length(&slider, &payload_item_type,
                                      &payload_item_length);
  m_data.clear();
  m_data.insert(m_data.end(), slider, slider + payload_item_length);
}
/* purecov: end */

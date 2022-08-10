/* Copyright (c) 2018, 2022, Oracle and/or its affiliates.

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

#include "plugin/group_replication/include/plugin_messages/transaction_prepared_message.h"
#include "my_dbug.h"

Transaction_prepared_message::Transaction_prepared_message(const rpl_sid *sid,
                                                           rpl_gno gno)
    : Plugin_gcs_message(CT_TRANSACTION_PREPARED_MESSAGE),
      m_sid_specified(sid != nullptr ? true : false),
      m_gno(gno) {
  if (sid != nullptr) {
    m_sid = *sid;
  }
}

Transaction_prepared_message::Transaction_prepared_message(
    const unsigned char *buf, size_t len)
    : Plugin_gcs_message(CT_TRANSACTION_PREPARED_MESSAGE),
      m_sid_specified(false),
      m_gno(0) {
  decode(buf, len);
}

Transaction_prepared_message::~Transaction_prepared_message() = default;

void Transaction_prepared_message::encode_payload(
    std::vector<unsigned char> *buffer) const {
  DBUG_TRACE;

  uint64 gno_aux = static_cast<uint64>(m_gno);
  encode_payload_item_int8(buffer, PIT_TRANSACTION_PREPARED_GNO, gno_aux);

  if (m_sid_specified) {
    encode_payload_item_bytes(buffer, PIT_TRANSACTION_PREPARED_SID, m_sid.bytes,
                              m_sid.BYTE_LENGTH);
  }
}

void Transaction_prepared_message::decode_payload(const unsigned char *buffer,
                                                  const unsigned char *end) {
  DBUG_TRACE;
  const unsigned char *slider = buffer;
  uint16 payload_item_type = 0;
  unsigned long long payload_item_length = 0;

  uint64 gno_aux = 0;
  decode_payload_item_int8(&slider, &payload_item_type, &gno_aux);
  m_gno = static_cast<rpl_gno>(gno_aux);

  while (slider + Plugin_gcs_message::WIRE_PAYLOAD_ITEM_HEADER_SIZE <= end) {
    // Read payload item header to find payload item length.
    decode_payload_item_type_and_length(&slider, &payload_item_type,
                                        &payload_item_length);

    switch (payload_item_type) {
      case PIT_TRANSACTION_PREPARED_SID:
        if (slider + payload_item_length <= end) {
          memcpy(m_sid.bytes, slider, payload_item_length);
          m_sid_specified = true;
          slider += payload_item_length;
        }
        break;
    }
  }
}

const rpl_sid *Transaction_prepared_message::get_sid() {
  return m_sid_specified ? &m_sid : nullptr;
}

rpl_gno Transaction_prepared_message::get_gno() { return m_gno; }

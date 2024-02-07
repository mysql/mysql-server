/* Copyright (c) 2018, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "plugin/group_replication/include/plugin_messages/transaction_prepared_message.h"
#include "my_dbug.h"
#include "plugin/group_replication/include/plugin_handlers/metrics_handler.h"

Transaction_prepared_message::Transaction_prepared_message(
    const gr::Gtid_tsid &tsid, bool is_tsid_specified, rpl_gno gno)
    : Plugin_gcs_message(CT_TRANSACTION_PREPARED_MESSAGE),
      m_tsid_specified(is_tsid_specified),
      m_gno(gno),
      m_tsid(tsid) {}

Transaction_prepared_message::Transaction_prepared_message(
    const unsigned char *buf, size_t len)
    : Plugin_gcs_message(CT_TRANSACTION_PREPARED_MESSAGE),
      m_tsid_specified(false),
      m_gno(0) {
  decode(buf, len);
}

Transaction_prepared_message::~Transaction_prepared_message() = default;

void Transaction_prepared_message::encode_payload(
    std::vector<unsigned char> *buffer) const {
  DBUG_TRACE;

  uint64 gno_aux = static_cast<uint64>(m_gno);
  encode_payload_item_int8(buffer, PIT_TRANSACTION_PREPARED_GNO, gno_aux);

  if (m_tsid_specified) {
    encode_payload_item_bytes(buffer, PIT_TRANSACTION_PREPARED_SID,
                              m_tsid.get_uuid().bytes.data(),
                              m_tsid.get_uuid().bytes.size());
    if (m_tsid.get_tag().is_empty() == false) {
      auto tag_length =
          m_tsid.get_tag().get_encoded_length(mysql::gtid::Gtid_format::tagged);
      encode_payload_item_type_and_length(buffer, PIT_TRANSACTION_PREPARED_TAG,
                                          tag_length);
      buffer->resize(buffer->size() + tag_length);
      [[maybe_unused]] auto bytes_encoded = m_tsid.get_tag().encode_tag(
          buffer->data() + buffer->size() - tag_length,
          gr::Gtid_format::tagged);
      DBUG_EXECUTE_IF("gr_corrupted_transaction_prepare_message", {
        buffer->data()[buffer->size() - tag_length + 1] = '1';
      };);
      assert(bytes_encoded == tag_length);
    }
  }

  encode_payload_item_int8(buffer, PIT_SENT_TIMESTAMP,
                           Metrics_handler::get_current_time());
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

  mysql::gtid::Uuid sid;
  gr::Gtid_tag tag;

  while (slider + Plugin_gcs_message::WIRE_PAYLOAD_ITEM_HEADER_SIZE <= end) {
    // Read payload item header to find payload item length.
    decode_payload_item_type_and_length(&slider, &payload_item_type,
                                        &payload_item_length);

    switch (payload_item_type) {
      case PIT_TRANSACTION_PREPARED_SID:
        if (slider + payload_item_length <= end) {
          memcpy(sid.bytes.data(), slider, payload_item_length);
          m_tsid_specified = true;
        }
        break;
      case PIT_TRANSACTION_PREPARED_TAG:
        if (slider + payload_item_length <= end) {
          auto bytes_read = tag.decode_tag(slider, payload_item_length,
                                           gr::Gtid_format::tagged);
          if (bytes_read != payload_item_length) {
            m_error = std::make_unique<mysql::utils::Error>(
                "gr::Transaction_prepared_message", __FILE__, __LINE__,
                "Failed to decode a tag, wrong format");
          }
        }
        break;
    }

    // Seek to next payload item.
    slider += payload_item_length;
  }
  if (m_tsid_specified) {
    m_tsid = gr::Gtid_tsid(std::move(sid), std::move(tag));
  }
}

const gr::Gtid_tsid &Transaction_prepared_message::get_tsid() { return m_tsid; }

rpl_gno Transaction_prepared_message::get_gno() { return m_gno; }

uint64_t Transaction_prepared_message::get_sent_timestamp(
    const unsigned char *buffer, size_t length) {
  DBUG_TRACE;
  return Plugin_gcs_message::get_sent_timestamp(buffer, length,
                                                PIT_SENT_TIMESTAMP);
}

bool Transaction_prepared_message::is_valid() const {
  if (!m_error) {
    return true;
  }
  return m_error->is_error() == false;
}

const Transaction_prepared_message::Error_ptr &
Transaction_prepared_message::get_error() const {
  return m_error;
}

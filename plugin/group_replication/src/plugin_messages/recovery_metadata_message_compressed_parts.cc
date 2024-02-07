/* Copyright (c) 2023, 2024, Oracle and/or its affiliates.

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

#include <tuple>

#include "my_byteorder.h"
#include "plugin/group_replication/include/plugin_messages/recovery_metadata_message_compressed_parts.h"

typedef Recovery_metadata_message_compressed_parts::Iterator
    Recovery_metadata_Iterator;

// Recovery_metadata_message_compressed_parts functions
Recovery_metadata_message_compressed_parts::
    Recovery_metadata_message_compressed_parts(
        Recovery_metadata_message *recovery_metadata_message, uint count)
    : m_payload_start(nullptr),
      m_payload_packet_count(count),
      m_recovery_metadata_message(recovery_metadata_message) {}

Recovery_metadata_Iterator
Recovery_metadata_message_compressed_parts::begin() noexcept {
  return Recovery_metadata_Iterator(m_recovery_metadata_message, 0);
}

Recovery_metadata_Iterator
Recovery_metadata_message_compressed_parts::end() noexcept {
  return Recovery_metadata_Iterator(m_recovery_metadata_message,
                                    m_payload_packet_count);
}

// Recovery_metadata_message_compressed_parts::Iterator functions
Recovery_metadata_message_compressed_parts::Iterator::Iterator(
    Recovery_metadata_message *recovery_metadata_message, uint packet_count)
    : m_payload_pos(nullptr),
      m_payload_length(0),
      m_count(packet_count),
      m_recovery_metadata_message(recovery_metadata_message) {}

std::tuple<const unsigned char *, unsigned long long, unsigned long long>
Recovery_metadata_Iterator::operator*() {
  // 1. Get compressed certification info.
  const unsigned char *certification_info_payload_start;
  m_payload_length = 0;
  std::tuple<Recovery_metadata_message::enum_recovery_metadata_message_error,
             const unsigned char *, unsigned long long>
      payload_compression_info_payload_error =
          m_recovery_metadata_message
              ->get_decoded_compressed_certification_info_payload(
                  m_payload_pos);

  if (std::get<0>(payload_compression_info_payload_error) ==
      Recovery_metadata_message::enum_recovery_metadata_message_error::
          RECOVERY_METADATA_MESSAGE_OK) {
    // set compressed certification info payload start position.
    certification_info_payload_start =
        std::get<1>(payload_compression_info_payload_error);
    // add payload length.
    m_payload_length = std::get<2>(payload_compression_info_payload_error);
  } else {
    return std::make_tuple(nullptr, 0, 0);
  }

  // 2. Get certification info uncompressed length.
  uint64 uncompressed_item_size{0};
  m_payload_uncompressed_length = 0;
  std::tuple<Recovery_metadata_message::enum_recovery_metadata_message_error,
             unsigned long long, unsigned long long>
      payload_compression_info_uncompressed_length_error =
          m_recovery_metadata_message
              ->get_decoded_compressed_certification_info_uncompressed_length(
                  m_payload_pos);

  if (std::get<0>(payload_compression_info_uncompressed_length_error) ==
      Recovery_metadata_message::enum_recovery_metadata_message_error::
          RECOVERY_METADATA_MESSAGE_OK) {
    // set certification info payload uncompressed length.
    uncompressed_item_size =
        std::get<1>(payload_compression_info_uncompressed_length_error);
    // add certification info uncompressed length.
    m_payload_uncompressed_length =
        std::get<2>(payload_compression_info_uncompressed_length_error);
  } else {
    return std::make_tuple(nullptr, 0, 0);
  }

  // set m_payload_pos to certification info payload start.
  m_payload_pos = certification_info_payload_start;
  return std::make_tuple(certification_info_payload_start, m_payload_length,
                         uncompressed_item_size);
}

Recovery_metadata_Iterator &Recovery_metadata_Iterator::operator++() {
  /*
    The Recovery Metadata payload may contain multiple packets of compressed
    data. So to fetch next next certification info packet position m_payload_pos
    to end of current certification info packet.
    Add to m_payload_pos:
    - payload length for PIT_COMPRESSED_CERTIFICATION_INFO_PAYLOAD packet.
    - payload length for PIT_COMPRESSED_CERTIFICATION_INFO_UNCOMPRESSED_LENGTH
      packet.
    - payload type length packet size which is WIRE_PAYLOAD_ITEM_TYPE_SIZE for
      PIT_COMPRESSED_CERTIFICATION_INFO_UNCOMPRESSED_LENGTH packet.
    - payload length packet size which is WIRE_PAYLOAD_ITEM_LEN_SIZE for
      PIT_COMPRESSED_CERTIFICATION_INFO_UNCOMPRESSED_LENGTH packet.
  */
  m_payload_pos += m_payload_length + m_payload_uncompressed_length +
                   Plugin_gcs_message::WIRE_PAYLOAD_ITEM_TYPE_SIZE +
                   Plugin_gcs_message::WIRE_PAYLOAD_ITEM_LEN_SIZE;
  ++m_count;
  return *this;
}

Recovery_metadata_Iterator Recovery_metadata_Iterator::operator++(int) {
  Iterator tmp = *this;
  ++(*this);
  return tmp;
}

bool Recovery_metadata_Iterator::operator==(Recovery_metadata_Iterator &b) {
  return m_count == b.m_count;
}

bool Recovery_metadata_Iterator::operator!=(Recovery_metadata_Iterator &b) {
  return m_count != b.m_count;
}

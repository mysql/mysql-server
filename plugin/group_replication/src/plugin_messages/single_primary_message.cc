/* Copyright (c) 2016, 2023, Oracle and/or its affiliates.

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

#include "plugin/group_replication/include/plugin_messages/single_primary_message.h"

#include "my_byteorder.h"
#include "my_dbug.h"

Single_primary_message::Single_primary_message(Single_primary_message_type type)
    : Plugin_gcs_message(CT_SINGLE_PRIMARY_MESSAGE),
      single_primary_message_type(type),
      primary_uuid(""),
      election_mode(ELECTION_MODE_END) {}

Single_primary_message::Single_primary_message(std::string &uuid,
                                               enum_primary_election_mode mode)
    : Plugin_gcs_message(CT_SINGLE_PRIMARY_MESSAGE),
      single_primary_message_type(SINGLE_PRIMARY_PRIMARY_ELECTION),
      primary_uuid(uuid),
      election_mode(mode) {}

Single_primary_message::~Single_primary_message() = default;

Single_primary_message::Single_primary_message(const uchar *buf, size_t len)
    : Plugin_gcs_message(CT_SINGLE_PRIMARY_MESSAGE),
      primary_uuid(""),
      election_mode(ELECTION_MODE_END) {
  decode(buf, len);
}

void Single_primary_message::decode_payload(const unsigned char *buffer,
                                            const unsigned char *end) {
  DBUG_TRACE;

  const unsigned char *slider = buffer;
  uint16 payload_item_type = 0;
  unsigned long long payload_item_length = 0;

  uint16 single_primary_message_type_aux = 0;
  decode_payload_item_int2(&slider, &payload_item_type,
                           &single_primary_message_type_aux);
  single_primary_message_type =
      (Single_primary_message_type)single_primary_message_type_aux;

  while (slider + Plugin_gcs_message::WIRE_PAYLOAD_ITEM_HEADER_SIZE <= end) {
    // Read payload item header to find payload item length.
    decode_payload_item_type_and_length(&slider, &payload_item_type,
                                        &payload_item_length);

    switch (payload_item_type) {
      case PIT_SINGLE_PRIMARY_SERVER_UUID:
        if (slider + payload_item_length <= end) {
          assert(single_primary_message_type ==
                 SINGLE_PRIMARY_PRIMARY_ELECTION);
          primary_uuid.assign(slider, slider + payload_item_length);
          slider += payload_item_length;
        }
        break;

      case PIT_SINGLE_PRIMARY_ELECTION_MODE:
        if (slider + payload_item_length <= end) {
          assert(single_primary_message_type ==
                 SINGLE_PRIMARY_PRIMARY_ELECTION);
          uint16 election_mode_aux = uint2korr(slider);
          election_mode = (enum_primary_election_mode)election_mode_aux;
          slider += payload_item_length;
        }
    }
  }
}

void Single_primary_message::encode_payload(
    std::vector<unsigned char> *buffer) const {
  DBUG_TRACE;

  uint16 single_primary_message_type_aux = (uint16)single_primary_message_type;
  encode_payload_item_int2(buffer, PIT_SINGLE_PRIMARY_MESSAGE_TYPE,
                           single_primary_message_type_aux);

  /*
    Optional payload items.
  */
  if (single_primary_message_type == SINGLE_PRIMARY_PRIMARY_ELECTION) {
    encode_payload_item_string(buffer, PIT_SINGLE_PRIMARY_SERVER_UUID,
                               primary_uuid.c_str(), primary_uuid.length());
    encode_payload_item_int2(buffer, PIT_SINGLE_PRIMARY_ELECTION_MODE,
                             election_mode);
  }
}

std::string &Single_primary_message::get_primary_uuid() {
  assert(single_primary_message_type == SINGLE_PRIMARY_PRIMARY_ELECTION);
  return primary_uuid;
}

enum_primary_election_mode Single_primary_message::get_election_mode() {
  assert(single_primary_message_type == SINGLE_PRIMARY_PRIMARY_ELECTION);
  return election_mode;
}

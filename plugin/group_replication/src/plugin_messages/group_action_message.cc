/* Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.

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

#include "plugin/group_replication/include/plugin_messages/group_action_message.h"
#include "my_byteorder.h"
#include "my_dbug.h"

Group_action_message::Group_action_message()
    : Plugin_gcs_message(CT_GROUP_ACTION_MESSAGE),
      group_action_type(ACTION_MESSAGE_END),
      group_action_phase(ACTION_PHASE_END),
      return_value(0),
      primary_election_uuid(""),
      gcs_protocol(Gcs_protocol_version::UNKNOWN) {}

Group_action_message::Group_action_message(enum_action_message_type type)
    : Plugin_gcs_message(CT_GROUP_ACTION_MESSAGE),
      group_action_type(type),
      group_action_phase(ACTION_PHASE_END),
      return_value(0),
      primary_election_uuid(""),
      gcs_protocol(Gcs_protocol_version::UNKNOWN) {}

Group_action_message::Group_action_message(std::string &primary_uuid)
    : Plugin_gcs_message(CT_GROUP_ACTION_MESSAGE),
      group_action_type(ACTION_PRIMARY_ELECTION_MESSAGE),
      group_action_phase(ACTION_PHASE_END),
      return_value(0),
      primary_election_uuid(primary_uuid),
      gcs_protocol(Gcs_protocol_version::UNKNOWN) {}

Group_action_message::Group_action_message(Gcs_protocol_version gcs_protocol)
    : Plugin_gcs_message(CT_GROUP_ACTION_MESSAGE),
      group_action_type(ACTION_SET_COMMUNICATION_PROTOCOL_MESSAGE),
      group_action_phase(ACTION_PHASE_END),
      return_value(0),
      primary_election_uuid(""),
      gcs_protocol(gcs_protocol) {}

Group_action_message::~Group_action_message() {}

Group_action_message::Group_action_message(const uchar *buf, size_t len)
    : Plugin_gcs_message(CT_GROUP_ACTION_MESSAGE) {
  decode(buf, len);
}

void Group_action_message::decode_payload(const unsigned char *buffer,
                                          const unsigned char *end) {
  DBUG_ENTER("Group_action_message::decode_payload");
  const unsigned char *slider = buffer;
  uint16 payload_item_type = 0;
  unsigned long long payload_item_length = 0;

  uint16 group_action_message_type_aux = 0;
  decode_payload_item_int2(&slider, &payload_item_type,
                           &group_action_message_type_aux);
  group_action_type = (enum_action_message_type)group_action_message_type_aux;

  uint16 group_action_message_phase_aux = 0;
  decode_payload_item_int2(&slider, &payload_item_type,
                           &group_action_message_phase_aux);
  group_action_phase =
      (enum_action_message_phase)group_action_message_phase_aux;

  uint32 return_value_aux = 0;
  decode_payload_item_int4(&slider, &payload_item_type, &return_value_aux);
  return_value = (int32)return_value_aux;

  while (slider + Plugin_gcs_message::WIRE_PAYLOAD_ITEM_HEADER_SIZE <= end) {
    // Read payload item header to find payload item length.
    decode_payload_item_type_and_length(&slider, &payload_item_type,
                                        &payload_item_length);

    switch (payload_item_type) {
      case PIT_ACTION_PRIMARY_ELECTION_UUID:
        if (slider + payload_item_length <= end) {
          DBUG_ASSERT(ACTION_PRIMARY_ELECTION_MESSAGE == group_action_type);
          primary_election_uuid.assign(slider, slider + payload_item_length);
          slider += payload_item_length;
        }
        break;
      case PIT_ACTION_SET_COMMUNICATION_PROTOCOL_VERSION:
        DBUG_ASSERT(ACTION_SET_COMMUNICATION_PROTOCOL_MESSAGE ==
                    group_action_type);
        if (slider + payload_item_length <= end) {
          gcs_protocol = static_cast<Gcs_protocol_version>(uint2korr(slider));
          slider += payload_item_length;
        }
        break;
    }
  }

  DBUG_VOID_RETURN;
}

void Group_action_message::encode_payload(
    std::vector<unsigned char> *buffer) const {
  DBUG_ENTER("Recovery_message::encode_payload");

  uint16 group_action_message_type_aux = (uint16)group_action_type;
  encode_payload_item_int2(buffer, PIT_ACTION_TYPE,
                           group_action_message_type_aux);

  uint16 group_action_message_phase_aux = (uint16)group_action_phase;
  encode_payload_item_int2(buffer, PIT_ACTION_PHASE,
                           group_action_message_phase_aux);

  uint32 return_value_aux = (uint32)return_value;
  encode_payload_item_int4(buffer, PIT_ACTION_RETURN_VALUE, return_value_aux);
  /*
    Optional payload items.
  */
  if (ACTION_PRIMARY_ELECTION_MESSAGE == group_action_type) {
    encode_payload_item_string(buffer, PIT_ACTION_PRIMARY_ELECTION_UUID,
                               primary_election_uuid.c_str(),
                               primary_election_uuid.length());
  } else if (ACTION_SET_COMMUNICATION_PROTOCOL_MESSAGE == group_action_type) {
    encode_payload_item_int2(buffer,
                             PIT_ACTION_SET_COMMUNICATION_PROTOCOL_VERSION,
                             static_cast<uint16>(gcs_protocol));
  }

  DBUG_VOID_RETURN;
}

Group_action_message::enum_action_message_type
Group_action_message::get_action_type(const uchar *buffer) {
  const unsigned char *slider = buffer;
  uint16 group_action_message_type_aux = 0;

  slider += WIRE_FIXED_HEADER_SIZE;
  slider += WIRE_PAYLOAD_ITEM_TYPE_SIZE;
  slider += WIRE_PAYLOAD_ITEM_LEN_SIZE;

  group_action_message_type_aux = uint2korr(slider);

  return (enum_action_message_type)group_action_message_type_aux;
}

const std::string &Group_action_message::get_primary_to_elect_uuid() {
  DBUG_ASSERT(ACTION_PRIMARY_ELECTION_MESSAGE == group_action_type);
  return primary_election_uuid;
}

const Gcs_protocol_version &Group_action_message::get_gcs_protocol() {
  DBUG_ASSERT(ACTION_SET_COMMUNICATION_PROTOCOL_MESSAGE == group_action_type &&
              gcs_protocol != Gcs_protocol_version::UNKNOWN);
  return gcs_protocol;
}

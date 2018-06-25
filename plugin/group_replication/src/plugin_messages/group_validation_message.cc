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

#include "plugin/group_replication/include/plugin_messages/group_validation_message.h"
#include "my_dbug.h"

Group_validation_message::Group_validation_message(bool has_channels,
                                                   uint member_weight_arg)
    : Plugin_gcs_message(CT_GROUP_VALIDATION_MESSAGE),
      group_validation_message_type(ELECTION_VALIDATION_MESSAGE),
      has_channels(has_channels),
      member_weight(member_weight_arg) {}

Group_validation_message::~Group_validation_message() {}

Group_validation_message::Group_validation_message(const uchar *buf, size_t len)
    : Plugin_gcs_message(CT_GROUP_VALIDATION_MESSAGE) {
  decode(buf, len);
}

bool Group_validation_message::has_slave_channels() const {
  return has_channels;
}

uint Group_validation_message::get_member_weight() const {
  return member_weight;
}

void Group_validation_message::decode_payload(const unsigned char *buffer,
                                              const unsigned char *) {
  DBUG_ENTER("Group_validation_message::decode_payload");
  const unsigned char *slider = buffer;
  uint16 payload_item_type = 0;

  uint16 group_validation_message_type_aux = 0;
  decode_payload_item_int2(&slider, &payload_item_type,
                           &group_validation_message_type_aux);
  group_validation_message_type =
      (enum_validation_message_type)group_validation_message_type_aux;

  unsigned char has_channels_aux = '0';
  decode_payload_item_char(&slider, &payload_item_type, &has_channels_aux);
  has_channels = (has_channels_aux == '1') ? true : false;

  uint16 member_weight_aux = 0;
  decode_payload_item_int2(&slider, &payload_item_type, &member_weight_aux);
  member_weight = (uint)member_weight_aux;

  DBUG_VOID_RETURN;
}

void Group_validation_message::encode_payload(
    std::vector<unsigned char> *buffer) const {
  DBUG_ENTER("Group_validation_message::encode_payload");

  uint16 group_validation_message_type_aux =
      (uint16)group_validation_message_type;
  encode_payload_item_int2(buffer, PIT_VALIDATION_TYPE,
                           group_validation_message_type_aux);

  char has_channels_aux = has_channels ? '1' : '0';
  encode_payload_item_char(buffer, PIT_VALIDATION_CHANNEL, has_channels_aux);

  uint16 member_weight_aux = (uint16)member_weight;
  encode_payload_item_int2(buffer, PIT_MEMBER_WEIGHT, member_weight_aux);

  DBUG_VOID_RETURN;
}

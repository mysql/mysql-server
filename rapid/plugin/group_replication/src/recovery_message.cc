/* Copyright (c) 2014, 2017, Oracle and/or its affiliates. All rights reserved.

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

#include "plugin/group_replication/include/recovery_message.h"

#include "my_dbug.h"

Recovery_message::Recovery_message(Recovery_message_type type,
                                   const std::string& uuid)
    : Plugin_gcs_message(CT_RECOVERY_MESSAGE),
      recovery_message_type(type)
{
  member_uuid.assign(uuid);
}

Recovery_message::~Recovery_message()
{
}

Recovery_message::Recovery_message(const uchar* buf, size_t len)
    : Plugin_gcs_message(CT_RECOVERY_MESSAGE)
{
  decode(buf, len);
}

void Recovery_message::decode_payload(const unsigned char* buffer,
                                      const unsigned char*)
{
  DBUG_ENTER("Recovery_message::decode_payload");
  const unsigned char *slider= buffer;
  uint16 payload_item_type= 0;
  unsigned long long payload_item_length= 0;

  uint16 recovery_message_type_aux= 0;
  decode_payload_item_int2(&slider,
                           &payload_item_type,
                           &recovery_message_type_aux);
  recovery_message_type= (Recovery_message_type)recovery_message_type_aux;

  decode_payload_item_string(&slider,
                             &payload_item_type,
                             &member_uuid,
                             &payload_item_length);

  DBUG_VOID_RETURN;
}

void Recovery_message::encode_payload(std::vector<unsigned char>* buffer) const
{
  DBUG_ENTER("Recovery_message::encode_payload");

  uint16 recovery_message_type_aux= (uint16)recovery_message_type;
  encode_payload_item_int2(buffer, PIT_RECOVERY_MESSAGE_TYPE,
                           recovery_message_type_aux);

  encode_payload_item_string(buffer, PIT_MEMBER_UUID,
                             member_uuid.c_str(),
                             member_uuid.length());

  DBUG_VOID_RETURN;
}

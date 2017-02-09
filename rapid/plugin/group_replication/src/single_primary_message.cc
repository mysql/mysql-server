/* Copyright (c) 2016, 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

#include "single_primary_message.h"

Single_primary_message::Single_primary_message(Single_primary_message_type type)
    : Plugin_gcs_message(CT_SINGLE_PRIMARY_MESSAGE),
      single_primary_message_type(type)
{
}

Single_primary_message::~Single_primary_message()
{
}

Single_primary_message::Single_primary_message(const uchar* buf, uint64 len)
    : Plugin_gcs_message(CT_SINGLE_PRIMARY_MESSAGE)
{
  decode(buf, len);
}

void Single_primary_message::decode_payload(const unsigned char* buffer,
                                            const unsigned char* end)
{
  DBUG_ENTER("Single_primary_message::decode_payload");
  const unsigned char *slider= buffer;
  uint16 payload_item_type= 0;

  uint16 single_primary_message_type_aux= 0;
  decode_payload_item_int2(&slider,
                           &payload_item_type,
                           &single_primary_message_type_aux);
  single_primary_message_type= (Single_primary_message_type)single_primary_message_type_aux;

  DBUG_VOID_RETURN;
}

void Single_primary_message::encode_payload(std::vector<unsigned char>* buffer) const
{
  DBUG_ENTER("Single_primary_message::encode_payload");

  uint16 single_primary_message_type_aux= (uint16)single_primary_message_type;
  encode_payload_item_int2(buffer, PIT_SINGLE_PRIMARY_MESSAGE_TYPE,
                           single_primary_message_type_aux);

  DBUG_VOID_RETURN;
}

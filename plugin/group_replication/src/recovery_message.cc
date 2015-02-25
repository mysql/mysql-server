/* Copyright (c) 2014, 2015, Oracle and/or its affiliates. All rights reserved.

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

#include "applier.h"
#include "recovery_message.h"

Recovery_message::Recovery_message(Recovery_message_type type,
                                   string* uuid)
    : Gcs_plugin_message(PAYLOAD_RECOVERY_EVENT),
      recovery_message_type(type)
{
  node_uuid= new string(uuid->c_str());
}

Recovery_message::~Recovery_message()
{
  delete node_uuid;
}

Recovery_message::Recovery_message(uchar* buf, size_t len)
    : Gcs_plugin_message(PAYLOAD_RECOVERY_EVENT)
{
  decode(buf, len);
}

void Recovery_message::decode_message(uchar* data, size_t len)
{
  const uchar* slider= data;
  recovery_message_type= (Recovery_message_type)uint2korr(slider);
  slider += 2;
  node_uuid = new string((const char*) slider);
}

void Recovery_message::encode_message(vector<uchar>* mbuf_ptr)
{
  uchar s[RECOVERY_MESSAGE_TYPE_LENGTH];
  int2store(s, (uint16) recovery_message_type);
  mbuf_ptr->insert(mbuf_ptr->end(), s, s + RECOVERY_MESSAGE_TYPE_LENGTH);

  mbuf_ptr->insert(mbuf_ptr->end(), node_uuid->c_str(),
                   node_uuid->c_str() + (node_uuid->length() + 1));
}



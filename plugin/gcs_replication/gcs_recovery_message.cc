/* Copyright (c) 2014, Oracle and/or its affiliates. All rights reserved.

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

#include "gcs_applier.h"
#include "gcs_recovery_message.h"
#include "gcs_message.h"
#include "my_byteorder.h"

Recovery_message::Recovery_message(Recovery_message_type type,
                                   string* uuid)
    : recovery_message_type(type), node_uuid(uuid)
{}

Recovery_message::Recovery_message(const uchar* data, size_t len)
{
  const uchar* slider= data;
  recovery_message_type= (Recovery_message_type)uint2korr(slider);
  slider += 2;
  node_uuid = new string((const char*) slider);
  slider += node_uuid->length()+1;

  DBUG_ASSERT((uchar*) slider == len + (uchar*) data);
}

const uchar* Recovery_message::encode(MessageBuffer* mbuf_ptr)
{
  mbuf_ptr->append_uint16((uint16) get_code());
  mbuf_ptr->append_uint16((uint16) recovery_message_type);
  mbuf_ptr->append_stdstr(*node_uuid);

  return mbuf_ptr->data();
}



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

#include "gcs_plugin_messages.h"
#include "gcs_message.h"

#include <mysql/group_replication_priv.h>

Plugin_gcs_message::Plugin_gcs_message(payload_gcs_message_code message_code)
{
  this->message_code= message_code;
}

Plugin_gcs_message::~Plugin_gcs_message()
{
}

void
Plugin_gcs_message::encode(vector<uchar>* buf)
{
  /*
    Encode the message code in a fixed size and then delegate to the
    subclass implementation of encode_message()
   */
  uchar s[payload_gcs_message_code_encoded_size];
  int2store(s, this->message_code);
  buf->insert(buf->end(), s, s + payload_gcs_message_code_encoded_size);

  this->encode_message(buf);
}

void
Plugin_gcs_message::decode(uchar* buf, size_t len)
{
  /*
    Decode the message code in a fixed size and then delegate to the
    subclass implementation of encode_message()
  */
  this->message_code= Plugin_gcs_message_utils::retrieve_code(buf);

  uchar* data_pointer= buf + payload_gcs_message_code_encoded_size;
  this->decode_message(data_pointer, len);
}

payload_gcs_message_code
Plugin_gcs_message::get_message_code()
{
  return this->message_code;
}

payload_gcs_message_code
Plugin_gcs_message_utils::retrieve_code(uchar* buf)
{
  return (payload_gcs_message_code) uint2korr(buf);
}

uchar*
Plugin_gcs_message_utils::retrieve_data(Gcs_message* msg/*uchar* raw_buf*/)
{
  return msg->get_payload()+payload_gcs_message_code_encoded_size;
}

size_t
Plugin_gcs_message_utils::retrieve_length(Gcs_message* msg/*size_t raw_len*/)
{
  return msg->get_payload_length()-payload_gcs_message_code_encoded_size;
}

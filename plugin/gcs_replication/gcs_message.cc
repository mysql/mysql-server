/* Copyright (c) 2013, Oracle and/or its affiliates. All rights reserved.

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


#include "gcs_message.h"
#include "gcs_stdlib_and_types.h"

// TODO: to work around
#include "my_byteorder.h"

namespace GCS
{

Message::Message(const uchar* data_arg, size_t len_arg)
{
  assert(uint4korr((uint32*) &(((Message_header*) data_arg)->msg_size)) == len_arg);

  mbuf.append(data_arg, len_arg);
  data= mbuf.data();
  restore_header();
};

void MessageBuffer::append_uint64(ulonglong val)
{
  uchar s[sizeof(val)];
  int8store(s, val);
  this->buffer->insert(this->buffer->end(), s, s + 8);
}

void MessageBuffer::append_uint32(ulong val)
{
  uchar s[sizeof(val)];
  int4store(s, val);
  this->buffer->insert(this->buffer->end(), s, s + 4);
}

void MessageBuffer::append_uint16(uint val)
{
  uchar s[sizeof(val)];
  int2store(s, val);
  this->buffer->insert(this->buffer->end(), s, s + 2);
}

} // namespace GCS

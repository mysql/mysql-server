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

#include "gcs_payload.h"
#include "gcs_message.h"

// TODO: to work around
#include "my_byteorder.h"

namespace GCS
{

void Serializable::store_code(Payload_code code, MessageBuffer *mbuf)
{
  compile_time_assert(Payload_code_size == 2);
  mbuf->append_uint16((uint16) code);
}

/*
  The methods reads out the Payload code from the first bytes
  of the payload array.
  The caller is required to compute the arg value
  as Message::get_payload().

  @param ptr  a pointer to the byte array containing payload.
  @return enum value of the payload.
*/
Payload_code Serializable::read_code(const uchar *ptr)
{
  compile_time_assert(Payload_code_size == 2);
  return (Payload_code) uint2korr(ptr);
}

/*
  The method points at the first byte of the instance data ("pure" payload).
  Requirements to the caller are similar to @c read_code.

  @param ptr  a pointer to the byte array containing payload
  @return     a pointer to the first byte that starts an encoded
              instance.
*/
const uchar* Serializable::read_data_start(const uchar *ptr)
{
  return ptr + Payload_code_size;
}

/* returns the Payload code straight out of message */
Payload_code get_payload_code(Message *msg)
{
  return Serializable::read_code(msg->get_payload());
}

/* returns a pointer to the first byte of the instance data  */
const uchar* get_payload_data(Message *msg)
{
  return Serializable::read_data_start(msg->get_payload());
}

/* returns the length of a byte array that encodes an instance */
size_t get_data_len(Message *msg)
{
  return msg->get_payload_size() - Serializable::Payload_code_size;
}

} // namespace

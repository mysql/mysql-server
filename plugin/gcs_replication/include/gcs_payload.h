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

#ifndef GCS_PAYLOAD_H
#define GCS_PAYLOAD_H

/*
  The file contains declarations relevant to common properties of
  Message payload. That largely deals with metainfo of being replicated
  objects.
*/

#include "gcs_stdlib_and_types.h"

namespace GCS
{

/**
   The payload code represents an informational object that GCS message can carry.
   The code maps to a subclass of "abstract" Serializable, see below.
*/
typedef enum enum_payload_type
{
  PAYLOAD_TRANSACTION_EVENT, // WL#6822
  PAYLOAD_STATE_EXCHANGE,    // WL#7332
  PAYLOAD_END                // dummy, non informative
} Payload_code;

class MessageBuffer;

class Serializable
{
public:
  /*
    The following static attributes determine Payload array layout as
    the first @c Payload_code_size bytes of the code followed
    by the "pure" instance data. In short: the code, the data.
  */
  static const size_t Payload_code_size= 2; // in bytes
  /* writes the Payload code of an instance into the Message */
  static void store_code(Payload_code code, MessageBuffer *mbuf);
  /* returns the Payload code of an instance finding it in Payload byte array */
  static Payload_code read_code(const uchar *);
  /* returns the first byte of an array that contains encoded instance */
  static const uchar* read_data_start(const uchar *);

  /* Child classes need to define "private" encoders */
  virtual const uchar* encode(MessageBuffer* mbuf)= 0;
  /* returns the Payload code of an instance reading it in the Message */
  virtual Payload_code get_code()= 0;
};


/* useful shortcuts */
class Message;

/* returns the Payload code straight out of message */
Payload_code get_payload_code(Message *msg);

/* returns a pointer to the first byte of the instance data  */
const uchar* get_payload_data(Message *msg);

/* returns the length of a byte array that encodes an instance */
size_t get_data_len(Message *msg);

} // namespace
#endif

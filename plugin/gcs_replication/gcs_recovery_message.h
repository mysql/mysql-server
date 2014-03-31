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

#ifndef GCS_RECOVERY_MESSAGE_INCLUDED
#define GCS_RECOVERY_MESSAGE_INCLUDED

#include <string>
#include <set>
#include "gcs_payload.h"

using GCS::Serializable;
using GCS::Payload_code;
using GCS::MessageBuffer;
using std::string;

/**
  The several recovery type messages.
 */
typedef enum en_recovery_message_type
{
  /**Recovery ended, node is online.*/
  RECOVERY_END_MESSAGE,
  /**Donor transmitted all data (for future use)*/
  DONOR_FINISHED_MESSAGE,
  MEMBER_END  // the end of the enum
} Recovery_message_type;

class Recovery_message : public Serializable
{

public:

  /**
    Message constructor

    @param[in] type       the recovery message type
    @param[in] node_uuid  the origination node uuid
  */
  Recovery_message(Recovery_message_type type, string* node_uuid);

  /** Returns this recovery message type */
  Recovery_message_type get_recovery_message_type()
  {
    return recovery_message_type;
  }

  /** Returns this message sender's uuid */
  string *get_node_uuid()
  {
    return node_uuid;
  }

  /** Returns this message's payload type */
  Payload_code get_code() { return GCS::PAYLOAD_RECOVERY_EVENT; }

  /**
    Encodes the message contents for transmission.

    @param[out] mbuf_ptr   the message buffer to be written

    @return the written buffer
  */
  const uchar *encode(MessageBuffer* mbuf_ptr);

  /**
    Message decoding constructor

    @param[in] data  the received data
    @param[in] len   the received data size
  */
  Recovery_message(const uchar* data, size_t len);

private:
  /**The message type*/
  Recovery_message_type recovery_message_type;
  /**The node uuid where the message originated*/
  string* node_uuid;
};

#endif // GCS_RECOVERY_MESSAGE_INCLUDED
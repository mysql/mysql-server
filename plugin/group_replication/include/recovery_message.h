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

#ifndef GCS_RECOVERY_MESSAGE_INCLUDED
#define GCS_RECOVERY_MESSAGE_INCLUDED

#define RECOVERY_MESSAGE_TYPE_LENGTH 2

#include <string>
#include <set>
#include "plugin_messages.h"

using std::string;

class Recovery_message : public Gcs_plugin_message
{

public:

  /**
  The several recovery type messages.
 */
  typedef enum
  {
    /**Recovery ended, node is online.*/
    RECOVERY_END_MESSAGE,
    /**Donor transmitted all data (for future use)*/
    DONOR_FINISHED_MESSAGE,
    RECOVERY_MESSAGE_TYPE_END  // the end of the enum
  } Recovery_message_type;

public:

  /**
    Message constructor

    @param[in] type       the recovery message type
    @param[in] node_uuid  the origination node uuid
  */
  Recovery_message(Recovery_message_type type, string* node_uuid);

  /**
    Message destructor
   */
  ~Recovery_message();

  /**
    Message constructor for raw data

    @param[in] buf raw data
    @param[in] len raw length
  */
  Recovery_message(uchar* buf, size_t len);

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

protected:
  /**
    Encodes the message contents for transmission.

    @param[out] mbuf_ptr   the message buffer to be written

    @return the written buffer
  */
  void encode_message(vector<uchar>* mbuf_ptr);

  /**
    Message decoding method

    @param[in] data  the received data
    @param[in] len   the received data size
  */
  void decode_message(uchar* buf, size_t len);

private:
  /**The message type*/
  Recovery_message_type recovery_message_type;
  /**The node uuid where the message originated*/
  string* node_uuid;
};

#endif // GCS_RECOVERY_MESSAGE_INCLUDED
